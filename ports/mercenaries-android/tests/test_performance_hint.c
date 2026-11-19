#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../app/src/main/cpp/android/android_performance_hint.h"
static int creates, reports, closes;
static int64_t duration;
static APerformanceHintManager *manager(void) { return (APerformanceHintManager *)(uintptr_t)1; }
static APerformanceHintSession *create(APerformanceHintManager *m, const int32_t *ids, size_t n, int64_t target)
{ assert(m && n == 1 && ids[0] == 42 && target == 33333333LL); ++creates; return (APerformanceHintSession *)(uintptr_t)2; }
static int report(APerformanceHintSession *s, int64_t d) { assert(s); ++reports; duration = d; return 0; }
static void close_session(APerformanceHintSession *s) { assert(s); ++closes; }
int main(void)
{
    AndroidHint h; AndroidHintApi missing = {0};
    assert(!android_hint_start(&h, missing, 42));
    assert(!android_hint_frame(&h, 1, 1.01));
    AndroidHintApi api = {manager, create, report, close_session};
    assert(android_hint_start(&h, api, 42) && creates == 1);
    android_hint_frame(&h, 1, 1.01); assert(reports == 0);
    android_hint_frame(&h, 1.06, 1.08); assert(reports == 1 && duration >= 49999999 && duration <= 50000001);
    android_hint_frame(&h, 4, 4.01); assert(reports == 1);
    android_hint_frame(&h, 4.02, 4.03); assert(reports == 2);
    android_hint_frame(&h, 3, 3.01); assert(reports == 2);
    android_hint_stop(&h); android_hint_stop(&h); assert(closes == 1);
    puts("performance hints: unavailable, work duration, pause and cleanup OK");
}
