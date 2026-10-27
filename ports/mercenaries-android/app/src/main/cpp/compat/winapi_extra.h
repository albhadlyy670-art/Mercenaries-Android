/*
 * winapi_extra.h - Win32 declarations the *game* sources (ports/mercenaries/src) need on top of the
 * toolkit's win32_compat.h. Included from the shadow <windows.h>.
 *
 * Keyboard/mouse/window functions behave as "no such device": on Android the only input sources are
 * gamepads and the touch overlay (android_input.c), which reach the game through xinput_xbox.h.
 */
#ifndef WINAPI_EXTRA_H
#define WINAPI_EXTRA_H

#include "platform/win32_compat.h"
#include <setjmp.h>
#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

/* ---- MSVC storage-class attributes ---------------------------------------------------------- */
#ifndef _MSC_VER
#define __declspec_thread __thread
#define __declspec_dllexport
#define __declspec_dllimport
#define __declspec_noinline __attribute__((noinline))
#define __declspec(x) __declspec_##x
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- virtual-key codes (values are the Windows ones; nothing generates them on Android) ---- */
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F5 0x74
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define MAPVK_VSC_TO_VK_EX 3

static inline UINT MapVirtualKeyW(UINT code, UINT type) { (void)code; (void)type; return 0; }
static inline SHORT GetKeyState(int vk) { (void)vk; return 0; }

/* ---- cursor / window: there is no desktop ---- */
static inline BOOL GetCursorPos(LPPOINT p) { if (p) p->x = p->y = 0; return TRUE; }
static inline BOOL SetCursorPos(int x, int y) { (void)x; (void)y; return TRUE; }
static inline BOOL ScreenToClient(HWND h, LPPOINT p) { (void)h; (void)p; return TRUE; }
static inline BOOL ClientToScreen(HWND h, LPPOINT p) { (void)h; (void)p; return TRUE; }
static inline BOOL ClipCursor(const RECT *r) { (void)r; return TRUE; }
static inline int ShowCursor(BOOL show) { (void)show; return 0; }
static inline HWND GetForegroundWindow(void) { return (HWND)(uintptr_t)1; }
/* Set by the Android glue when the EGL surface is created. */
extern int g_android_surface_w, g_android_surface_h;
static inline BOOL GetClientRect(HWND h, RECT *r) { (void)h; r->left = r->top = 0; r->right = g_android_surface_w; r->bottom = g_android_surface_h; return TRUE; }
static inline BOOL GetWindowRect(HWND h, RECT *r) { return GetClientRect(h, r); }
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#endif
static inline int GetSystemMetrics(int i) { return i == SM_CXSCREEN ? g_android_surface_w : i == SM_CYSCREEN ? g_android_surface_h : 0; }

#ifndef IDC_ARROW
typedef void *HCURSOR;
#define IDC_ARROW ((LPCSTR)(uintptr_t)32512)
static inline HCURSOR LoadCursorA(void *inst, LPCSTR name) { (void)inst; (void)name; return NULL; }
#define LoadCursor LoadCursorA
static inline HCURSOR SetCursor(HCURSOR c) { (void)c; return NULL; }
static inline BOOL IsIconic(HWND h) { (void)h; return FALSE; }
#endif

/* ---- code pages and string helpers ---- */
#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x8u
#endif
#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x80u
#endif
#define CSTR_LESS_THAN 1
#define CSTR_EQUAL 2
#define CSTR_GREATER_THAN 3
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400u

/* UTF-16 (WCHAR is 16-bit in this codebase) <-> UTF-8, used for path handling. */
static inline int android_utf16_to_utf8(const WCHAR *w, char *out, int cap)
{
    int n = 0;
    for (; *w; ++w) {
        unsigned c = *w;
        if (c >= 0xD800 && c < 0xDC00 && w[1] >= 0xDC00 && w[1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (w[1] - 0xDC00); ++w; }
        int need = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        if (out && n + need >= cap) break;
        if (out) {
            if (need == 1) out[n] = (char)c;
            else if (need == 2) { out[n] = (char)(0xC0 | (c >> 6)); out[n+1] = (char)(0x80 | (c & 63)); }
            else if (need == 3) { out[n] = (char)(0xE0 | (c >> 12)); out[n+1] = (char)(0x80 | ((c >> 6) & 63)); out[n+2] = (char)(0x80 | (c & 63)); }
            else { out[n] = (char)(0xF0 | (c >> 18)); out[n+1] = (char)(0x80 | ((c >> 12) & 63)); out[n+2] = (char)(0x80 | ((c >> 6) & 63)); out[n+3] = (char)(0x80 | (c & 63)); }
        }
        n += need;
    }
    if (out && cap > 0) out[n < cap ? n : cap - 1] = 0;
    return n;
}
static inline BOOL GetFileAttributesWBool(const WCHAR *w, struct stat *st)
{ char p[1024]; android_utf16_to_utf8(w, p, sizeof(p)); return stat(p, st) == 0; }
static inline DWORD GetFileAttributesW(const WCHAR *w)
{
    struct stat st; if (!GetFileAttributesWBool(w, &st)) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? 0x10u : 0x80u;
}
static inline int CompareStringOrdinal(const WCHAR *a, int na, const WCHAR *b, int nb, BOOL ignore_case)
{
    if (na < 0) { na = 0; while (a[na]) ++na; } if (nb < 0) { nb = 0; while (b[nb]) ++nb; }
    int n = na < nb ? na : nb;
    for (int i = 0; i < n; ++i) {
        unsigned x = a[i], y = b[i]; if (ignore_case) { x = (unsigned)towlower((wint_t)x); y = (unsigned)towlower((wint_t)y); }
        if (x != y) return x < y ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
    }
    return na == nb ? CSTR_EQUAL : (na < nb ? CSTR_LESS_THAN : CSTR_GREATER_THAN);
}

#ifdef __cplusplus
}
#endif
#endif
