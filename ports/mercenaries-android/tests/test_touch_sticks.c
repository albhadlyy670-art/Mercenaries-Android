/* Drive actual Android input glue with synthetic multitouch events. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <android/input.h>
#include "android_host.h"
#include "input/xinput_xbox.h"
struct AInputEvent { int type, source, action, count; int ids[10]; float x[10], y[10], axes[32]; };
int32_t AInputEvent_getType(const AInputEvent *e) { return e->type; }
int32_t AInputEvent_getSource(const AInputEvent *e) { return e->source; }
int32_t AMotionEvent_getAction(const AInputEvent *e) { return e->action; }
size_t AMotionEvent_getPointerCount(const AInputEvent *e) { return e->count; }
int32_t AMotionEvent_getPointerId(const AInputEvent *e, size_t i) { return e->ids[i]; }
float AMotionEvent_getX(const AInputEvent *e, size_t i) { return e->x[i]; }
float AMotionEvent_getY(const AInputEvent *e, size_t i) { return e->y[i]; }
float AMotionEvent_getAxisValue(const AInputEvent *e, int32_t axis, size_t i) { (void)i; return e->axes[axis]; }
int32_t AKeyEvent_getAction(const AInputEvent *e) { return e->action; }
int32_t AKeyEvent_getKeyCode(const AInputEvent *e) { (void)e; return 0; }
static XBOX_INPUT_STATE state(void) { XBOX_INPUT_STATE s; assert(xbox_InputGetState(0, &s) == ERROR_SUCCESS); return s; }
int main(void)
{
    const int w=1920, h=1080;
    AInputEvent e = { .type=AINPUT_EVENT_TYPE_MOTION, .source=AINPUT_SOURCE_TOUCHSCREEN, .count=1 };
    android_touch_overlay_t overlay;
    android_input_set_screen(w,h);
    e.action=AMOTION_EVENT_ACTION_DOWN; e.ids[0]=11; e.x[0]=.16f*w; e.y[0]=.70f*h;
    assert(android_input_event(&e)); assert(state().Gamepad.sThumbLX==0);
    e.count=2; e.ids[1]=22; e.x[1]=.72f*w; e.y[1]=.68f*h;
    e.action=AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    android_input_event(&e);
    e.action=AMOTION_EVENT_ACTION_MOVE; e.x[0]=.16f*w+.13f*h*2; e.y[0]=.70f*h;
    e.x[1]=.72f*w; e.y[1]=.68f*h-.105f*h;
    android_input_event(&e); XBOX_GAMEPAD p=state().Gamepad;
    assert(p.sThumbLX>32700 && p.sThumbLY==0 && p.sThumbRY>32700 && p.sThumbRX==0);
    android_input_overlay_snapshot(&overlay); assert(overlay.lx==p.sThumbLX && overlay.ry==p.sThumbRY);
    /* A third finger operates a face button without stealing either stick. */
    e.count=3; e.ids[2]=33; e.x[2]=.90f*w; e.y[2]=.78f*h;
    e.action=AMOTION_EVENT_ACTION_POINTER_DOWN | (2 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    android_input_event(&e); p=state().Gamepad;
    assert(p.bAnalogButtons[XBOX_BUTTON_A]==255 && p.sThumbLX>32700 && p.sThumbRY>32700);
    e.action=AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    android_input_event(&e); p=state().Gamepad;
    assert(p.sThumbRY==0 && p.sThumbLX>32700 && p.bAnalogButtons[XBOX_BUTTON_A]==255);
    e.action=AMOTION_EVENT_ACTION_CANCEL; android_input_event(&e); p=state().Gamepad;
    assert(p.sThumbLX==0 && p.sThumbRY==0 && p.bAnalogButtons[XBOX_BUTTON_A]==0);
    /* Diagonal displacement is clamped to a circular, not square, range. */
    e.count=1; e.ids[0]=44; e.x[0]=.72f*w; e.y[0]=.68f*h; e.action=AMOTION_EVENT_ACTION_DOWN; android_input_event(&e);
    e.action=AMOTION_EVENT_ACTION_MOVE; e.x[0]+=.105f*h*2; e.y[0]+=.105f*h*2; android_input_event(&e); p=state().Gamepad;
    assert(p.sThumbRX>22000 && p.sThumbRX<24000 && p.sThumbRY< -22000 && p.sThumbRY> -24000);
    /* Touch release restores the physical controller's right stick. */
    AInputEvent hw={.type=AINPUT_EVENT_TYPE_MOTION,.source=AINPUT_SOURCE_JOYSTICK}; hw.axes[AMOTION_EVENT_AXIS_Z]=.5f;
    android_input_event(&hw); e.action=AMOTION_EVENT_ACTION_UP; android_input_event(&e);
    p=state().Gamepad; assert(p.sThumbRX>16000 && p.sThumbRX<17000 && p.sThumbRY==0);
    puts("touch sticks: multi-touch, clamping, capture, buttons, release and hardware OK"); return 0;
}
