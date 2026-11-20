#ifndef D3DGL_GPU_TIMER_H
#define D3DGL_GPU_TIMER_H
#include <stdint.h>
#include <string.h>
/* Nonblocking elapsed-time query sampler. The GL owner supplies functions;
 * slots are never reused until available, including after a disjoint event. */
#define GPU_TIMER_SLOTS 8
#define GPU_TIMER_ELAPSED 0x88BFu
#define GPU_TIMER_AVAILABLE 0x8867u
#define GPU_TIMER_RESULT 0x8866u
#define GPU_TIMER_DISJOINT 0x8FBBu
typedef struct {
    void (*gen)(int, unsigned *);
    void (*begin)(unsigned, unsigned);
    void (*end)(unsigned);
    void (*get_u32)(unsigned, unsigned, unsigned *);
    void (*get_u64)(unsigned, unsigned, uint64_t *);
    void (*get_int)(unsigned, int *);
} GpuTimerOps;
typedef struct {
    GpuTimerOps op;
    struct { unsigned id; int pending, invalid; } slots[GPU_TIMER_SLOTS];
    unsigned frame; int active; /* active is index+1 */
    uint64_t samples, disjoint, dropped, invalid_results;
    unsigned bad_streak; int unreliable;
    double total_ms, max_ms;
} GpuTimer;
static inline void gpu_timer_init(GpuTimer *t, GpuTimerOps ops)
{ memset(t, 0, sizeof(*t)); t->op = ops; }
static inline int gpu_timer_valid(const GpuTimer *t)
{ return t->op.gen && t->op.begin && t->op.end && t->op.get_u32 && t->op.get_u64 && t->op.get_int; }
static inline void gpu_timer_end_frame(GpuTimer *t)
{
    if (!gpu_timer_valid(t)) return;
    if (t->active) {
        unsigned n=(unsigned)t->active-1u;
        t->op.end(GPU_TIMER_ELAPSED); t->slots[n].pending=1; t->active=0;
    }
    int any=0; for (unsigned i=0;i<GPU_TIMER_SLOTS;++i) any |= t->slots[i].pending;
    if (!any) return;
    int disjoint=0; t->op.get_int(GPU_TIMER_DISJOINT,&disjoint);
    if (disjoint) {
        ++t->disjoint;
        for (unsigned i=0;i<GPU_TIMER_SLOTS;++i) if (t->slots[i].pending) t->slots[i].invalid=1;
    }
    for (unsigned i=0;i<GPU_TIMER_SLOTS;++i) if (t->slots[i].pending) {
        unsigned available=0; t->op.get_u32(t->slots[i].id,GPU_TIMER_AVAILABLE,&available);
        if (!available) continue; /* Never wait for the GPU. */
        if (!t->slots[i].invalid) {
            uint64_t ns=0; t->op.get_u64(t->slots[i].id,GPU_TIMER_RESULT,&ns);
            /* Some drivers return saturated/implausible results despite
             * reporting available and no disjoint. Never average them as
             * valid GPU time. A frame over 10 seconds is not useful here. */
            if (ns > 10000000000ULL) {
                ++t->invalid_results;
                if (++t->bad_streak >= 3u) t->unreliable=1;
            } else {
                double ms=(double)ns/1000000.0; t->total_ms+=ms;
                if (ms>t->max_ms) t->max_ms=ms;
                ++t->samples; t->bad_streak=0;
            }
        }
        t->slots[i].pending=t->slots[i].invalid=0;
    }
}
static inline void gpu_timer_begin_frame(GpuTimer *t)
{
    if (!gpu_timer_valid(t) || t->unreliable || t->active || (++t->frame & 15u)) return;
    for (unsigned i=0;i<GPU_TIMER_SLOTS;++i) if (!t->slots[i].pending) {
        if (!t->slots[i].id) t->op.gen(1,&t->slots[i].id);
        if (!t->slots[i].id) { ++t->dropped; return; }
        t->op.begin(GPU_TIMER_ELAPSED,t->slots[i].id); t->active=(int)i+1; return;
    }
    ++t->dropped;
}
static inline void gpu_timer_reset_stats(GpuTimer *t)
{ t->samples=t->disjoint=t->dropped=t->invalid_results=0; t->total_ms=t->max_ms=0.0; }
#endif
