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
#include <stdarg.h>
#include <pthread.h>

#define TAG "MercRecomp"
static char g_crash_context[512];
static int g_enabled = 1;

/* <cache>/events.log: every event is written and flushed straight to disk so it survives a sudden kill. */
extern const char *g_android_crash_path;
static FILE *g_ev;
static pthread_mutex_t g_ev_lock = PTHREAD_MUTEX_INITIALIZER;
void android_tracev(const char *fmt, va_list ap)
{
    pthread_mutex_lock(&g_ev_lock);
    if (!g_ev && g_android_crash_path) {
        char p[900]; snprintf(p, sizeof(p), "%s", g_android_crash_path);
        char *sl = strrchr(p, '/');
        if (sl) { strcpy(sl + 1, "events.log"); g_ev = fopen(p, "w"); }
    }
    if (g_ev) { vfprintf(g_ev, fmt, ap); fputc('\n', g_ev); fflush(g_ev); }
    pthread_mutex_unlock(&g_ev_lock);
}
void android_trace(const char *fmt, ...) { va_list ap; va_start(ap, fmt); android_tracev(fmt, ap); va_end(ap); }

int xbox_preview_log_init(const wchar_t *directory) { (void)directory; g_enabled = 1; return 1; }
int xbox_preview_log_enabled(void) { return g_enabled; }

void xbox_preview_log_eventv(const char *category, const char *format, va_list args)
{
    char msg[1024];
    vsnprintf(msg, sizeof(msg), format, args);
    __android_log_print(ANDROID_LOG_INFO, TAG, "[%s] %s", category ? category : "log", msg);
    android_trace("[%s] %s", category ? category : "log", msg);
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
    android_trace("CRASH code=%08X host_pc=%p guest_func=%08X eax=%08X ecx=%08X edx=%08X esp=%08X ctx=%s",
        code, (void *)pc, guest_pc, eax, ecx, edx, esp, g_crash_context);
    __android_log_print(ANDROID_LOG_FATAL, TAG,
        "CRASH code=%08X host_pc=%p guest_func=%08X eax=%08X ecx=%08X edx=%08X esp=%08X ctx=%s",
        code, (void *)pc, guest_pc, eax, ecx, edx, esp, g_crash_context);
}

void xbox_preview_log_shutdown(void) {}
