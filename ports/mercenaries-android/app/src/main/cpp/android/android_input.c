/*
 * android_input.c - implements input/xinput_xbox.h for Android.
 * Sources: physical gamepads (key + joystick motion events) and an on-screen
 * touch overlay (left stick, A/B/X/Y, triggers, Start/Back/Black/White).
 * Events arrive on the UI thread; the guest polls from the game thread.
 */
#include "input/xinput_xbox.h"
#include "android_host.h"
#include <android/input.h>
#include <android/keycodes.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static XBOX_GAMEPAD g_pad_hw, g_pad_touch;
static volatile int g_hw_connected, g_prompt = XBOX_PROMPT_XBOX;
static DWORD g_packet;
static XBOX_INPUT_MAPPER g_mapper;

/* touch tracking */
typedef struct { int32_t id; float x, y; int down; } finger_t;
static finger_t g_fingers[10];
static int g_sw = 1280, g_sh = 720;

void android_input_set_screen(int w, int h) { g_sw = w > 0 ? w : 1280; g_sh = h > 0 ? h : 720; }

/* Overlay layout shared with android_gl.c (normalised 0..1 of screen). */
const android_touch_btn_t k_buttons[ANDROID_TOUCH_BUTTONS] = {
    { "A", 0.90f, 0.78f, 0.065f, XBOX_BUTTON_A }, { "B", 0.96f, 0.64f, 0.065f, XBOX_BUTTON_B },
    { "X", 0.84f, 0.62f, 0.065f, XBOX_BUTTON_X }, { "Y", 0.90f, 0.46f, 0.065f, XBOX_BUTTON_Y },
    { "L", 0.08f, 0.12f, 0.07f, XBOX_BUTTON_LTRIGGER }, { "R", 0.92f, 0.12f, 0.07f, XBOX_BUTTON_RTRIGGER },
    { "BLK", 0.72f, 0.88f, 0.045f, XBOX_BUTTON_BLACK }, { "WHT", 0.64f, 0.88f, 0.045f, XBOX_BUTTON_WHITE },
    { "STA", 0.54f, 0.90f, 0.04f, 100 }, { "BCK", 0.46f, 0.90f, 0.04f, 101 },
};
const float k_stick_cx = 0.16f, k_stick_cy = 0.70f, k_stick_r = 0.13f;

static void rebuild_touch(void)
{
    XBOX_GAMEPAD p; memset(&p, 0, sizeof(p));
    const float ar = (float)g_sw / (float)g_sh;
    for (int i = 0; i < 10; ++i) {
        if (!g_fingers[i].down) continue;
        float fx = g_fingers[i].x / g_sw, fy = g_fingers[i].y / g_sh;
        float dx = (fx - k_stick_cx) * ar, dy = fy - k_stick_cy;           /* aspect-correct */
        if (sqrtf(dx * dx + dy * dy) < k_stick_r * 1.6f) {
            float m = k_stick_r * 1.0f;
            float nx = fmaxf(-1.f, fminf(1.f, dx / m)), ny = fmaxf(-1.f, fminf(1.f, dy / m));
            p.sThumbLX = (SHORT)(nx * 32767.f); p.sThumbLY = (SHORT)(-ny * 32767.f);
            continue;
        }
        for (int b = 0; b < ANDROID_TOUCH_BUTTONS; ++b) {
            float bx = (fx - k_buttons[b].x) * ar, by = fy - k_buttons[b].y;
            if (sqrtf(bx * bx + by * by) > k_buttons[b].r * 1.4f) continue;
            int id = k_buttons[b].id;
            if (id < 8) p.bAnalogButtons[id] = 255;
            else if (id == 100) p.wButtons |= XBOX_GAMEPAD_START;
            else if (id == 101) p.wButtons |= XBOX_GAMEPAD_BACK;
        }
    }
    g_pad_touch = p;
}

static void handle_touch(AInputEvent *e)
{
    int32_t action = AMotionEvent_getAction(e), kind = action & AMOTION_EVENT_ACTION_MASK;
    size_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    size_t n = AMotionEvent_getPointerCount(e);
    pthread_mutex_lock(&g_lock);
    if (kind == AMOTION_EVENT_ACTION_MOVE) {
        for (size_t i = 0; i < n; ++i) {
            int32_t id = AMotionEvent_getPointerId(e, i);
            for (int f = 0; f < 10; ++f) if (g_fingers[f].down && g_fingers[f].id == id) {
                g_fingers[f].x = AMotionEvent_getX(e, i); g_fingers[f].y = AMotionEvent_getY(e, i);
            }
        }
    } else if (kind == AMOTION_EVENT_ACTION_DOWN || kind == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        for (int f = 0; f < 10; ++f) if (!g_fingers[f].down) {
            g_fingers[f] = (finger_t){ AMotionEvent_getPointerId(e, idx),
                AMotionEvent_getX(e, idx), AMotionEvent_getY(e, idx), 1 };
            break;
        }
    } else if (kind == AMOTION_EVENT_ACTION_UP || kind == AMOTION_EVENT_ACTION_POINTER_UP ||
               kind == AMOTION_EVENT_ACTION_CANCEL) {
        if (kind == AMOTION_EVENT_ACTION_CANCEL) memset(g_fingers, 0, sizeof(g_fingers));
        else { int32_t id = AMotionEvent_getPointerId(e, idx);
               for (int f = 0; f < 10; ++f) if (g_fingers[f].id == id) g_fingers[f].down = 0; }
    }
    rebuild_touch(); ++g_packet;
    pthread_mutex_unlock(&g_lock);
}

static SHORT axis16(float v, float dead)
{
    if (fabsf(v) < dead) return 0;
    return (SHORT)(fmaxf(-1.f, fminf(1.f, v)) * 32767.f);
}

static void handle_joystick(AInputEvent *e)
{
    pthread_mutex_lock(&g_lock);
    g_hw_connected = 1; g_prompt = XBOX_PROMPT_XBOX;
    g_pad_hw.sThumbLX = axis16(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_X, 0), 0.12f);
    g_pad_hw.sThumbLY = axis16(-AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Y, 0), 0.12f);
    g_pad_hw.sThumbRX = axis16(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Z, 0), 0.12f);
    g_pad_hw.sThumbRY = axis16(-AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_RZ, 0), 0.12f);
    float lt = fmaxf(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_LTRIGGER, 0), AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_BRAKE, 0));
    float rt = fmaxf(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_RTRIGGER, 0), AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_GAS, 0));
    g_pad_hw.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = (BYTE)(fminf(lt, 1.f) * 255.f);
    g_pad_hw.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = (BYTE)(fminf(rt, 1.f) * 255.f);
    float hx = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_X, 0), hy = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_Y, 0);
    g_pad_hw.wButtons &= ~(XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_DOWN | XBOX_GAMEPAD_DPAD_LEFT | XBOX_GAMEPAD_DPAD_RIGHT);
    if (hx < -0.5f) g_pad_hw.wButtons |= XBOX_GAMEPAD_DPAD_LEFT;  if (hx > 0.5f) g_pad_hw.wButtons |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (hy < -0.5f) g_pad_hw.wButtons |= XBOX_GAMEPAD_DPAD_UP;    if (hy > 0.5f) g_pad_hw.wButtons |= XBOX_GAMEPAD_DPAD_DOWN;
    ++g_packet;
    pthread_mutex_unlock(&g_lock);
}

static int handle_key(AInputEvent *e)
{
    int down = AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_DOWN;
    int32_t k = AKeyEvent_getKeyCode(e);
    int analog = -1; WORD mask = 0;
    switch (k) {
    case AKEYCODE_BUTTON_A: analog = XBOX_BUTTON_A; break;   case AKEYCODE_BUTTON_B: analog = XBOX_BUTTON_B; break;
    case AKEYCODE_BUTTON_X: analog = XBOX_BUTTON_X; break;   case AKEYCODE_BUTTON_Y: analog = XBOX_BUTTON_Y; break;
    case AKEYCODE_BUTTON_L1: analog = XBOX_BUTTON_WHITE; break; case AKEYCODE_BUTTON_R1: analog = XBOX_BUTTON_BLACK; break;
    case AKEYCODE_BUTTON_L2: analog = XBOX_BUTTON_LTRIGGER; break; case AKEYCODE_BUTTON_R2: analog = XBOX_BUTTON_RTRIGGER; break;
    case AKEYCODE_BUTTON_START: mask = XBOX_GAMEPAD_START; break; case AKEYCODE_BUTTON_SELECT: mask = XBOX_GAMEPAD_BACK; break;
    case AKEYCODE_BUTTON_THUMBL: mask = XBOX_GAMEPAD_LEFT_THUMB; break; case AKEYCODE_BUTTON_THUMBR: mask = XBOX_GAMEPAD_RIGHT_THUMB; break;
    case AKEYCODE_DPAD_UP: mask = XBOX_GAMEPAD_DPAD_UP; break;     case AKEYCODE_DPAD_DOWN: mask = XBOX_GAMEPAD_DPAD_DOWN; break;
    case AKEYCODE_DPAD_LEFT: mask = XBOX_GAMEPAD_DPAD_LEFT; break; case AKEYCODE_DPAD_RIGHT: mask = XBOX_GAMEPAD_DPAD_RIGHT; break;
    default: return 0;
    }
    pthread_mutex_lock(&g_lock);
    g_hw_connected = 1;
    if (analog >= 0) g_pad_hw.bAnalogButtons[analog] = down ? 255 : 0;
    if (mask) { if (down) g_pad_hw.wButtons |= mask; else g_pad_hw.wButtons &= ~mask; }
    ++g_packet;
    pthread_mutex_unlock(&g_lock);
    return 1;
}

int android_input_event(AInputEvent *e)
{
    int32_t type = AInputEvent_getType(e), src = AInputEvent_getSource(e);
    if (type == AINPUT_EVENT_TYPE_KEY) return handle_key(e);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        if (src & AINPUT_SOURCE_JOYSTICK) { handle_joystick(e); return 1; }
        if (src & AINPUT_SOURCE_TOUCHSCREEN) { handle_touch(e); return 1; }
    }
    return 0;
}

int android_input_overlay_pressed(int id)      /* for drawing pressed state */
{
    int r = 0;
    pthread_mutex_lock(&g_lock);
    if (id < 8) r = g_pad_touch.bAnalogButtons[id] > 0;
    else if (id == 100) r = (g_pad_touch.wButtons & XBOX_GAMEPAD_START) != 0;
    else if (id == 101) r = (g_pad_touch.wButtons & XBOX_GAMEPAD_BACK) != 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* ---- xinput_xbox.h ------------------------------------------------------ */
void xbox_InputSetMapper(XBOX_INPUT_MAPPER m) { g_mapper = m; }
int xbox_InputPromptDevice(void) { return g_prompt; }
int xbox_InputPromptController(void) { return XBOX_PROMPT_XBOX; }
void xbox_InputNotifyKeyboardActivity(void) {}
void xbox_InputInit(void) {}
void xbox_InputSetOverlayCapture(BOOL c) { (void)c; }
BOOL xbox_InputKeyDown(int vk) { (void)vk; return FALSE; }
BOOL xbox_InputKeyDownContinuous(int vk) { (void)vk; return FALSE; }
void xbox_InputStopTestAutoA(void) {}
void xbox_InputArmTestAutoY(void) {}
void xbox_InputDisarmTestAutoY(void) {}
void xbox_InputArmTestAutoMove(void) {}
BOOL xbox_InputApplyTestExactLoadGame(XBOX_INPUT_STATE *s) { (void)s; return FALSE; }

DWORD xbox_InputGetState(DWORD port, XBOX_INPUT_STATE *out)
{
    if (port != 0 || !out) return ERROR_DEVICE_NOT_CONNECTED;
    pthread_mutex_lock(&g_lock);
    XBOX_GAMEPAD g = g_pad_hw;
    for (int i = 0; i < 8; ++i) if (g_pad_touch.bAnalogButtons[i] > g.bAnalogButtons[i]) g.bAnalogButtons[i] = g_pad_touch.bAnalogButtons[i];
    g.wButtons |= g_pad_touch.wButtons;
    if (abs(g_pad_touch.sThumbLX) + abs(g_pad_touch.sThumbLY) > abs(g.sThumbLX) + abs(g.sThumbLY)) { g.sThumbLX = g_pad_touch.sThumbLX; g.sThumbLY = g_pad_touch.sThumbLY; }
    out->Gamepad = g; out->dwPacketNumber = g_packet;
    pthread_mutex_unlock(&g_lock);
    if (g_mapper) g_mapper(out, FALSE);
    return ERROR_SUCCESS;
}
DWORD xbox_InputGetOverlayState(DWORD port, XBOX_INPUT_STATE *out) { return xbox_InputGetState(port, out); }
DWORD xbox_InputSetState(DWORD port, const XBOX_VIBRATION *v) { (void)port; (void)v; return ERROR_SUCCESS; }
BOOL xbox_InputIsConnected(DWORD port) { return port == 0; }   /* touch pad is always present */
DWORD xbox_InputGetCapabilities(DWORD port, DWORD flags, XBOX_INPUT_CAPABILITIES *c)
{
    (void)flags;
    if (port != 0 || !c) return ERROR_DEVICE_NOT_CONNECTED;
    memset(c, 0, sizeof(*c)); c->Type = 1; c->SubType = 1;
    return ERROR_SUCCESS;
}
