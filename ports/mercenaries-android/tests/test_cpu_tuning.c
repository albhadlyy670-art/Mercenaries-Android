#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
static atomic_int g_android_quit;
static void android_trace(const char *fmt, ...) { (void)fmt; }
#include "../app/src/main/cpp/android/android_cpu_tuning.inc"
int main(void)
{
    long caps[TUNE_MAX_CPUS] = {500, 500, 500, 500, 500, 500, 1024, 1024,
                                -1,-1,-1,-1,-1,-1,-1,-1};
    assert(tune_select_big_cores(caps,TUNE_MAX_CPUS)==0xc0);
    for (int i=0;i<6;i++) caps[i]=700;
    assert(tune_select_big_cores(caps,TUNE_MAX_CPUS)==0xc0);
    for (int i=0;i<8;i++) caps[i]=1024;
    assert(tune_select_big_cores(caps,TUNE_MAX_CPUS)==0);
    caps[0]=-1; assert(tune_select_big_cores(caps,TUNE_MAX_CPUS)==0);
    unsigned long mask=33;
    assert(tune_parse_mask("  0xc0\n", &mask) && mask==0xc0);
    assert(tune_parse_mask("0", &mask) && mask==0);
    assert(!tune_parse_mask(" -1", &mask));
    assert(!tune_parse_mask("", &mask));
    assert(!tune_parse_mask("c0 junk", &mask));
    assert(!tune_parse_mask("1000000000000000000000000000", &mask));
    char stat[2048]="42 (game worker) S";
    for (int i=1;i<36;i++) strcat(stat," 0");
    strcat(stat," 7 0\n");
    assert(tune_parse_game_cpu(stat)==7);
    char malformed[]="42 (game) S 0"; assert(tune_parse_game_cpu(malformed)==-1);
    /* A stopped game cannot leave a helper behind that tunes a recycled TID. */
    android_cpu_tuning_start("/tmp"); assert(g_tune_started);
    android_cpu_tuning_stop(); assert(!g_tune_started);
    puts("CPU tuning: selection, mask parsing, stat parsing and lifecycle OK");
    return 0;
}
