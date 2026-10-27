/*
 * preview_log_android.c - logcat implementation of kernel/preview_log.h.
 * The Windows build writes timestamped files under preview-logs/; on Android
 * everything goes to logcat (tag "MercRecomp"), which `adb logcat` and the
 * in-app "Share log" button read.
 */
#include "kernel/preview_log.h"
#include <android/log.h>
#include <stdio.h>
#include <string.h>

#define TAG "MercRecomp"
static char g_crash_context[512];
static int g_enabled = 1;

int xbox_preview_log_init(const wchar_t *directory) { (void)directory; g_enabled = 1; return 1; }
int xbox_preview_log_enabled(void) { return g_enabled; }

void xbox_preview_log_eventv(const char *category, const char *format, va_list args)
{
    char msg[1024];
    vsnprintf(msg, sizeof(msg), format, args);
    __android_log_print(ANDROID_LOG_INFO, TAG, "[%s] %s", category ? category : "log", msg);
}

void xbox_preview_log_event(const char *category, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    xbox_preview_log_eventv(category, format, ap);
    va_end(ap);
}

void xbox_preview_log_sample(const char *category, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    xbox_preview_log_eventv(category, format, ap);
    va_end(ap);
}

void xbox_preview_log_set_crash_context(const char *text)
{
    snprintf(g_crash_context, sizeof(g_crash_context), "%s", text ? text : "");
}

void xbox_preview_log_crash(uint32_t code, uintptr_t pc, uint32_t guest_pc,
                            uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esp)
{
    __android_log_print(ANDROID_LOG_FATAL, TAG,
        "CRASH code=%08X host_pc=%p guest_func=%08X eax=%08X ecx=%08X edx=%08X esp=%08X ctx=%s",
        code, (void *)pc, guest_pc, eax, ecx, edx, esp, g_crash_context);
}

void xbox_preview_log_shutdown(void) {}
