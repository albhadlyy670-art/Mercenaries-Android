#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/d3d11_gles/gpu_timer.h"
static unsigned ids, begins, ends, available, active, result_reads;
static int disjoint;
static void gen(int n,unsigned *id) { assert(n==1); *id=++ids; }
static void begin(unsigned target,unsigned id) { assert(target==GPU_TIMER_ELAPSED && !active); active=id; ++begins; }
static void end(unsigned target) { assert(target==GPU_TIMER_ELAPSED && active); active=0; ++ends; }
static void getu32(unsigned id,unsigned p,unsigned *v) { assert(id && p==GPU_TIMER_AVAILABLE); *v=available; }
static void getu64(unsigned id,unsigned p,uint64_t *v) { assert(id && p==GPU_TIMER_RESULT); ++result_reads; *v=12500000; }
static void geti(unsigned p,int *v) { assert(p==GPU_TIMER_DISJOINT); *v=disjoint; disjoint=0; }
int main(void) {
    GpuTimer t; gpu_timer_init(&t,(GpuTimerOps){gen,begin,end,getu32,getu64,geti});
    for (int i=0;i<16;i++) { gpu_timer_end_frame(&t); gpu_timer_begin_frame(&t); }
    assert(begins==1 && active); gpu_timer_end_frame(&t);
    assert(ends==1 && !t.samples && !result_reads && t.slots[0].pending);
    available=1; gpu_timer_end_frame(&t); assert(t.samples==1 && t.total_ms==12.5 && t.max_ms==12.5 && result_reads==1);
    gpu_timer_reset_stats(&t); assert(!t.samples && t.max_ms==0);
    for (int i=0;i<16;i++) gpu_timer_begin_frame(&t);
    available=0; disjoint=1; gpu_timer_end_frame(&t); assert(t.disjoint==1 && t.slots[0].invalid);
    available=1; gpu_timer_end_frame(&t); assert(!t.samples && result_reads==1 && !t.slots[0].pending);
    available=0;
    for (int f=0;f<16*9;f++) { gpu_timer_begin_frame(&t); gpu_timer_end_frame(&t); }
    assert(t.dropped==1 && ids==GPU_TIMER_SLOTS);
    gpu_timer_init(&t,(GpuTimerOps){0}); gpu_timer_begin_frame(&t); gpu_timer_end_frame(&t); assert(!t.samples);
    puts("GPU timer: nonblocking availability, disjoint, bounded slots and reset OK");
}
