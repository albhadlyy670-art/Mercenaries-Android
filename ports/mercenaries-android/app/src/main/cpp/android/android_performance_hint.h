/* Android ADPF: optional scheduler hints for the game thread, never a clock lock. */
#ifndef ANDROID_PERFORMANCE_HINT_H
#define ANDROID_PERFORMANCE_HINT_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;
typedef struct {
    APerformanceHintManager *(*manager)(void);
    APerformanceHintSession *(*create)(APerformanceHintManager *, const int32_t *, size_t, int64_t);
    int (*report)(APerformanceHintSession *, int64_t);
    int (*update)(APerformanceHintSession *, int64_t);
    void (*close)(APerformanceHintSession *);
} AndroidHintApi;
typedef struct {
    AndroidHintApi api;
    APerformanceHintSession *session;
    double last_end;
    int requested_fps;
} AndroidHint;

static int android_hint_start(AndroidHint *h, AndroidHintApi api, int32_t tid)
{
    *h = (AndroidHint){0};
    if (!api.manager || !api.create || !api.report || !api.close || tid <= 0) return 0;
    APerformanceHintManager *manager = api.manager();
    if (!manager) return 0;
    /* Target 30 fps, not a frame limiter. Android remains free to ignore it. */
    h->session = api.create(manager, &tid, 1, 33333333LL);
    if (!h->session) return 0;
    h->api = api;
    h->requested_fps = 30;
    return 1;
}
/* Uncapped rendering uses a conservative 60 FPS hint. This does not cap the
 * game or modify its frame pacing, and Android may ignore the hint. */
static inline int android_hint_set_fps(AndroidHint *h, int fps)
{
    if (!h->session) return 0;
    if (fps != 30 && fps != 60 && fps != 90 && fps != 120) fps = 60;
    if (h->requested_fps == fps) return 0;
    h->requested_fps = fps; /* avoid repeatedly calling an unsupported HAL */
    if (!h->api.update) return 0;
    return h->api.update(h->session, 1000000000LL / fps);
}
static int android_hint_frame(AndroidHint *h, double swap_start, double swap_end)
{
    if (!h->session || swap_end <= 0.0) return 0;
    double previous = h->last_end;
    h->last_end = swap_end;
    /* Work ends before blocking swap; skip first frame and lifecycle pauses. */
    if (previous <= 0.0 || swap_start <= previous || swap_end < swap_start ||
        swap_end - previous > 1.0) return 0;
    double work = swap_start - previous;
    return h->api.report(h->session, (int64_t)(work * 1000000000.0));
}
static void android_hint_stop(AndroidHint *h)
{
    if (h->session) h->api.close(h->session);
    *h = (AndroidHint){0};
}
#endif
