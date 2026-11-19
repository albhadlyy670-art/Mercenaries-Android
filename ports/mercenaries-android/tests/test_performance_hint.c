#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../app/src/main/cpp/android/android_performance_hint.h"
static int creates, reports, closes, updates, update_error;
static int64_t last_target;
static int64_t duration;
static APerformanceHintManager *manager(void) { return (APerformanceHintManager *)(uintptr_t)1; }
static APerformanceHintSession *create(APerformanceHintManager *m, const int32_t *ids, size_t n, int64_t target)
{ assert(m && n == 1 && ids[0] == 42 && target == 33333333LL); ++creates; return (APerformanceHintSession *)(uintptr_t)2; }
static int report(APerformanceHintSession *s, int64_t d) { assert(s); ++reports; duration = d; return 0; }
static int update(APerformanceHintSession *s, int64_t target) { assert(s); ++updates; last_target = target; return update_error; }
static void close_session(APerformanceHintSession *s) { assert(s); ++closes; }
int main(void)
{
    AndroidHint h; AndroidHintApi missing = {0};
    assert(!android_hint_start(&h, missing, 42));
    assert(!android_hint_frame(&h, 1, 1.01));
    AndroidHintApi api = {manager, create, report, update, close_session};
    assert(android_hint_start(&h, api, 42) && creates == 1);
    assert(!android_hint_set_fps(&h, 30) && updates == 0);
    assert(!android_hint_set_fps(&h, 60) && updates == 1 && last_target == 16666666LL);
    assert(!android_hint_set_fps(&h, 60) && updates == 1);
    assert(!android_hint_set_fps(&h, 120) && updates == 2 && last_target == 8333333LL);
    assert(!android_hint_set_fps(&h, 0) && updates == 3 && last_target == 16666666LL);
    update_error = 22; assert(android_hint_set_fps(&h, 90) == 22 && updates == 4);
    assert(!android_hint_set_fps(&h, 90) && updates == 4);
    android_hint_frame(&h, 1, 1.01); assert(reports == 0);
    android_hint_frame(&h, 1.06, 1.08); assert(reports == 1 && duration >= 49999999 && duration <= 50000001);
    android_hint_frame(&h, 4, 4.01); assert(reports == 1);
    android_hint_frame(&h, 4.02, 4.03); assert(reports == 2);
    android_hint_frame(&h, 3, 3.01); assert(reports == 2);
    android_hint_stop(&h); android_hint_stop(&h); assert(closes == 1);
    api.update = NULL; assert(android_hint_start(&h, api, 42));
    assert(!android_hint_set_fps(&h, 60)); android_hint_stop(&h); assert(closes == 2);
    puts("performance hints: target changes, failure, duration and cleanup OK");
}
