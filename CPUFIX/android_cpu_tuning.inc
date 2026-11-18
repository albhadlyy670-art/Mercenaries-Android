/*
 * android_cpu_tuning.inc - included by android_game.c (needs _GNU_SOURCE, <pthread.h>,
 * <unistd.h>, <stdio.h>, <stdlib.h>, <string.h>, <stdatomic.h> and android_trace()).
 *
 * 1. Pins the game thread to the big cores after startup (the game thread is the
 *    bottleneck: ~83% busy, one thread). Override / disable with
 *    <tmp_dir>/cpu_affinity.txt containing a hex CPU mask ("c0" = cpu6+cpu7, "0" = off).
 * 2. Logs "[cpu] ..." every 10 s: which core the game thread runs on, per-core
 *    frequency and the hottest thermal zone, so throttling / core migration shows up
 *    next to the "[perf]" lines.
 *
 * Guest worker threads created during startup are not pinned: pinning is applied
 * ~15 s after start, from a helper thread, and only to the game thread.
 */
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#define TUNE_MAX_CPUS 16
static pid_t g_tune_game_ktid;
static char g_tune_dir[1024];

static long tune_read_long(const char *path)
{
    FILE *f = fopen(path, "r"); long v = -1;
    if (!f) return -1;
    if (fscanf(f, "%ld", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* Bit mask of the "big" cores, 0 if the SoC shows no big/little split. */
static unsigned long tune_detect_big_cores(char *how, size_t how_n)
{
    long val[TUNE_MAX_CPUS]; long maxv = 0, minv = 0x7fffffff; char p[128];
    for (int pass = 0; pass < 2; ++pass) {
        maxv = 0; minv = 0x7fffffff;
        for (int i = 0; i < TUNE_MAX_CPUS; ++i) {
            snprintf(p, sizeof(p), pass == 0 ? "/sys/devices/system/cpu/cpu%d/cpu_capacity"
                                             : "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
            val[i] = tune_read_long(p);
            if (val[i] > maxv) maxv = val[i];
            if (val[i] >= 0 && val[i] < minv) minv = val[i];
        }
        /* Only trust a source that shows a real spread between cores. */
        if (maxv > 0 && minv < maxv * 9 / 10) {
            unsigned long mask = 0;
            for (int i = 0; i < TUNE_MAX_CPUS; ++i)
                if (val[i] >= 0 && val[i] * 10 >= maxv * 6) mask |= 1ul << i;
            snprintf(how, how_n, "%s max=%ld min=%ld", pass == 0 ? "cpu_capacity" : "cpuinfo_max_freq", maxv, minv);
            return mask;
        }
    }
    snprintf(how, how_n, "no big/little split found in sysfs");
    return 0;
}

static int tune_game_cpu(void)
{
    char p[96], buf[1024];
    snprintf(p, sizeof(p), "/proc/self/task/%d/stat", (int)g_tune_game_ktid);
    FILE *f = fopen(p, "r"); if (!f) return -1;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    char *s = strrchr(buf, ')'); if (!s) return -1;
    /* fields after ')' start at field 3; "processor" is field 39 -> 37th token */
    int tok = 0; char *save = NULL;
    for (char *t = strtok_r(s + 1, " ", &save); t; t = strtok_r(NULL, " ", &save))
        if (++tok == 37) return atoi(t);
    return -1;
}

static void tune_log_sample(void)
{
    char p[128], line[320]; size_t k = 0;
    long ncpu = sysconf(_SC_NPROCESSORS_CONF); if (ncpu < 1 || ncpu > TUNE_MAX_CPUS) ncpu = TUNE_MAX_CPUS;
    k += (size_t)snprintf(line + k, sizeof(line) - k, "[cpu] game_cpu=%d freq_mhz=[", tune_game_cpu());
    for (int i = 0; i < ncpu && k + 16 < sizeof(line); ++i) {
        snprintf(p, sizeof(p), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        long v = tune_read_long(p);
        if (v < 0) k += (size_t)snprintf(line + k, sizeof(line) - k, "%soff", i ? "," : "");
        else k += (size_t)snprintf(line + k, sizeof(line) - k, "%s%ld", i ? "," : "", v / 1000);
    }
    long maxt = -1;
    for (int z = 0; z < 40; ++z) {
        snprintf(p, sizeof(p), "/sys/class/thermal/thermal_zone%d/temp", z);
        long t = tune_read_long(p); if (t > 1000) t /= 1000;
        if (t > 0 && t < 130 && t > maxt) maxt = t;
    }
    if (maxt >= 0) snprintf(line + k, sizeof(line) - k, "] max_temp_c=%ld", maxt);
    else snprintf(line + k, sizeof(line) - k, "] max_temp_c=n/a");
    android_trace("%s", line);
}

static void *tune_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 15 && !atomic_load(&g_android_quit); ++i) sleep(1);   /* let startup threads spawn first */
    unsigned long mask = 0; char how[96] = ""; int override_cfg = 0; char cfg[1100];
    snprintf(cfg, sizeof(cfg), "%s/cpu_affinity.txt", g_tune_dir);
    FILE *f = fopen(cfg, "r");
    if (f) {
        char b[32] = {0};
        if (fgets(b, sizeof(b), f)) { mask = strtoul(b, NULL, 16); override_cfg = 1; snprintf(how, sizeof(how), "cpu_affinity.txt override"); }
        fclose(f);
    }
    if (!override_cfg) mask = tune_detect_big_cores(how, sizeof(how));
    if (mask) {
        cpu_set_t set; CPU_ZERO(&set);
        for (int i = 0; i < TUNE_MAX_CPUS; ++i) if (mask & (1ul << i)) CPU_SET(i, &set);
        int rc = sched_setaffinity(g_tune_game_ktid, sizeof(set), &set);
        android_trace("[cpu] game thread tid=%d affinity mask=0x%lx (%s) rc=%d errno=%d", (int)g_tune_game_ktid, mask, how, rc, rc ? errno : 0);
        int nrc = setpriority(PRIO_PROCESS, (id_t)g_tune_game_ktid, -6);
        android_trace("[cpu] setpriority(-6) rc=%d errno=%d", nrc, nrc ? errno : 0);
    } else {
        android_trace("[cpu] affinity not applied (%s)", how[0] ? how : "disabled by cpu_affinity.txt");
    }
    while (!atomic_load(&g_android_quit)) { tune_log_sample(); sleep(10); }
    return NULL;
}

/* Call from the game thread, right before mercenaries_android_main(). */
static void android_cpu_tuning_start(const char *tmp_dir)
{
    g_tune_game_ktid = (pid_t)syscall(SYS_gettid);
    snprintf(g_tune_dir, sizeof(g_tune_dir), "%s", tmp_dir ? tmp_dir : ".");
    pthread_t t;
    if (pthread_create(&t, NULL, tune_thread, NULL) == 0) pthread_detach(t);
}
