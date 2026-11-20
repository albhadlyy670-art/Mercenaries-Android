#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic unsigned calls;
static char value[]="enabled";
static char *fake_getenv(const char *name) { atomic_fetch_add(&calls,1); return !strcmp(name,"PRESENT") ? value : NULL; }
#define getenv fake_getenv
#include "../../../src/nv2a/nv2a_env_cache.h"
static const char *present(void) { return NV2A_LAUNCH_ENV("PRESENT"); }
static const char *absent(void) { return NV2A_LAUNCH_ENV("ABSENT"); }
static void *worker(void *p) { (void)p; for (int i=0;i<10000;i++) { assert(present()==value); assert(!absent()); } return NULL; }
int main(void) {
    pthread_t threads[8];
    for (int i=0;i<8;i++) assert(!pthread_create(&threads[i],NULL,worker,NULL));
    for (int i=0;i<8;i++) assert(!pthread_join(threads[i],NULL));
    unsigned after=atomic_load(&calls); assert(after>=2 && after<=16);
    for (int i=0;i<100;i++) { assert(present()==value); assert(!absent()); }
    assert(atomic_load(&calls)==after);
    assert(NV2A_LAUNCH_ENV("PRESENT")==value); assert(atomic_load(&calls)==after+1);
    puts("NV2A launch environment: per-site hits, missing values and concurrent first use OK");
}
