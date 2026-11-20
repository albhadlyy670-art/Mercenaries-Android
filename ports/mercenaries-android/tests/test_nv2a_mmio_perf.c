#include "../../../src/nv2a/nv2a_mmio_perf.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define N 100000
static Nv2aMmioPerfCounts counts;
static void *increment(void *arg)
{
    (void)arg;
    for (int i=0; i<N; ++i) {
        nv2a_mmio_perf_increment(&counts.reads);
        nv2a_mmio_perf_increment(&counts.writes);
        if ((i & 3) == 0) nv2a_mmio_perf_increment(&counts.puts);
    }
    return NULL;
}
int main(void)
{
    pthread_t threads[4];
    for (int i=0; i<4; ++i) assert(!pthread_create(&threads[i], NULL, increment, NULL));
    for (int i=0; i<4; ++i) assert(!pthread_join(threads[i], NULL));
    Nv2aMmioPerfCounts s = nv2a_mmio_perf_snapshot(&counts);
    assert(s.reads==4*N && s.writes==4*N && s.puts==N);
    Nv2aMmioPerfCounts old = {UINT64_MAX-2, UINT64_MAX, 1}, now = {3,2,5};
    Nv2aMmioPerfCounts d = nv2a_mmio_perf_delta(now,old);
    assert(d.reads==6 && d.writes==3 && d.puts==4);
    puts("NV2A MMIO counters: concurrent increments and modular intervals OK");
}
