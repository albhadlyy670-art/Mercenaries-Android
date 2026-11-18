/* Developer telemetry is opt-in; essential traces and crashes remain available. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../app/src/main/cpp/android/preview_log_android.c"
static int logcat_count;
const char *g_android_crash_path;
int __android_log_print(int priority, const char *tag, const char *fmt, ...)
{ (void)priority; (void)tag; (void)fmt; ++logcat_count; return 0; }
int main(void)
{
    FILE *f = tmpfile(); assert(f); g_ev = f;
    assert(!xbox_preview_log_enabled());
    xbox_preview_log_event("faction-target", "expensive event %d", 1);
    assert(logcat_count == 0 && ftell(f) == 0);
    android_trace("[perf] fps=8"); assert(ftell(f) > 0);
    xbox_preview_log_crash(1, 0, 0, 0, 0, 0, 0);
    assert(logcat_count == 1);
    assert(xbox_preview_log_init(NULL) && xbox_preview_log_enabled());
    xbox_preview_log_event("test", "enabled %d", 7);
    assert(logcat_count == 2);
    xbox_preview_log_shutdown();
    assert(!xbox_preview_log_enabled());
    xbox_preview_log_event("test", "disabled"); assert(logcat_count == 2);
    rewind(f); char text[2048]; size_t n = fread(text, 1, sizeof(text)-1, f); text[n] = 0;
    assert(strstr(text,"[perf] fps=8") && strstr(text,"CRASH") && strstr(text,"[test] enabled 7"));
    assert(!strstr(text,"expensive event") && !strstr(text,"disabled"));
    fclose(f); g_ev = NULL;
    puts("Android preview logging: opt-in events, independent perf/crash traces OK"); return 0;
}
