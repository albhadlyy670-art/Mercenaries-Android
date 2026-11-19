/*
 * android_cpu_tuning.inc - included by android_game.c (needs _GNU_SOURCE, <pthread.h>,
 * <unistd.h>, <stdio.h>, <stdlib.h>, <string.h>, <stdatomic.h> and android_trace()).
 *
 * 1. Attempts to pin the game thread to detected big cores after startup.
 *    Scheduler/permission limits can prevent this. Override / disable with
 *    <tmp_dir>/cpu_affinity.txt containing a hex CPU mask ("c0" = cpu6+cpu7, "0" = off).
 * 2. Logs "[cpu] ..." every 10 s: which core the game thread runs on, per-core
 *    frequency, battery temperature and CPU6 frequency policy when readable.
 *    Samples are snapshots, not proof of a thermal or CPU bottleneck.
 *    next to the "[perf]" lines.
 *
 * The helper changes only the game thread ~15 s after start. Threads created
 * later by that thread can inherit its affinity, per pthread/Linux semantics.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#define TUNE_MAX_CPUS 16
static pid_t g_tune_game_ktid;
static char g_tune_dir[1024];
static pthread_t g_tune_thread;
static int g_tune_started;
static atomic_int g_tune_stop;

static long tune_read_long(const char *path)
{
    FILE *f = fopen(path, "r"); long v = -1;
    if (!f) return -1;
    if (fscanf(f, "%ld", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* Sysfs strings such as governors are a single bounded token. */
static int tune_read_token(const char *path, char *out, size_t size)
{
    if (!out || size < 2u) return 0;
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[64];
    int ok = fgets(buf, sizeof(buf), f) != NULL;
    fclose(f);
    if (!ok || !strchr(buf, '\n')) return 0;
    size_t n = strcspn(buf, "\r\n");
    if (!n || n >= size) return 0;
    for (size_t i = 0; i < n; ++i)
        if (!isalnum((unsigned char)buf[i]) && buf[i] != '_' && buf[i] != '-') return 0;
    memcpy(out, buf, n); out[n] = '\0';
    return 1;
}

/* Linux power_supply TEMP uses tenths of a degree Celsius. */
static int tune_battery_temp_tenths(long raw)
{ return raw >= 0 && raw <= 1000; }

/* Select only cores near the highest capacity, not every core above 60%:
 * many SoCs have little cores above that threshold. Require a real split. */
static unsigned long tune_select_big_cores(const long *val, int count)
{
    long maxv = 0, minv = 0x7fffffff;
    unsigned long mask = 0;
    for (int i = 0; i < count; ++i) {
        if (val[i] > maxv) maxv = val[i];
        if (val[i] > 0 && val[i] < minv) minv = val[i];
    }
    if (maxv <= 0 || minv >= maxv * 9 / 10) return 0;
    for (int i = 0; i < count; ++i)
        if (val[i] > 0 && val[i] >= maxv * 9 / 10) mask |= 1ul << i;
    return mask;
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
            if (val[i] > 0 && val[i] < minv) minv = val[i];
        }
        /* Only trust a source that shows a real spread between cores. */
        unsigned long mask = tune_select_big_cores(val, TUNE_MAX_CPUS);
        if (mask) {
            snprintf(how, how_n, "%s max=%ld min=%ld", pass == 0 ? "cpu_capacity" : "cpuinfo_max_freq", maxv, minv);
            return mask;
        }
    }
    snprintf(how, how_n, "no big/little split found in sysfs");
    return 0;
}

static int tune_parse_mask(const char *text, unsigned long *mask)
{
    while (isspace((unsigned char)*text)) ++text;
    if (*text == '-' || *text == '+' || !*text) return 0;
    char *end; errno = 0;
    unsigned long value = strtoul(text, &end, 16);
    if (end == text || errno == ERANGE) return 0;
    while (isspace((unsigned char)*end)) ++end;
    if (*end) return 0;
    *mask = value;
    return 1;
}

static int tune_parse_game_cpu(char *buf)
{
    char *s = strrchr(buf, ')'); if (!s) return -1;
    /* Fields after ')' start at 3; processor is field 39. */
    int tok = 0; char *save = NULL;
    for (char *t = strtok_r(s + 1, " ", &save); t; t = strtok_r(NULL, " ", &save))
        if (++tok == 37) {
            char *end; long value = strtol(t, &end, 10);
            return end != t && (*end == 0 || *end == '\n') && value >= 0 && value <= INT_MAX ? (int)value : -1;
        }
    return -1;
}

static int tune_game_cpu(void)
{
    char p[96], buf[2048];
    snprintf(p, sizeof(p), "/proc/self/task/%d/stat", (int)g_tune_game_ktid);
    FILE *f = fopen(p, "r"); if (!f) return -1;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    return tune_parse_game_cpu(buf);
}

static void tune_log_sample(void)
{
    char p[128], line[512]; size_t k = 0;
    long ncpu = sysconf(_SC_NPROCESSORS_CONF); if (ncpu < 1 || ncpu > TUNE_MAX_CPUS) ncpu = TUNE_MAX_CPUS;
    k += (size_t)snprintf(line + k, sizeof(line) - k, "[cpu] game_cpu=%d freq_mhz=[", tune_game_cpu());
    for (int i = 0; i < ncpu && k + 16 < sizeof(line); ++i) {
        snprintf(p, sizeof(p), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        long v = tune_read_long(p);
        if (v < 0) k += (size_t)snprintf(line + k, sizeof(line) - k, "%sn/a", i ? "," : "");
        else k += (size_t)snprintf(line + k, sizeof(line) - k, "%s%ld", i ? "," : "", v / 1000);
    }
    long maxt = -1;
    for (int z = 0; z < 40; ++z) {
        snprintf(p, sizeof(p), "/sys/class/thermal/thermal_zone%d/temp", z);
        long t = tune_read_long(p); if (t > 1000) t /= 1000;
        if (t > 0 && t < 130 && t > maxt) maxt = t;
    }
    if (maxt >= 0) k += (size_t)snprintf(line + k, sizeof(line) - k, "] max_temp_c=%ld", maxt);
    else k += (size_t)snprintf(line + k, sizeof(line) - k, "] max_temp_c=n/a");
    long battery = tune_read_long("/sys/class/power_supply/battery/temp");
    if (tune_battery_temp_tenths(battery))
        k += (size_t)snprintf(line + k, sizeof(line) - k, " battery_temp_c=%.1f", (double)battery / 10.0);
    else k += (size_t)snprintf(line + k, sizeof(line) - k, " battery_temp_c=n/a");
    long maxfreq = tune_read_long("/sys/devices/system/cpu/cpu6/cpufreq/scaling_max_freq");
    long hardware = tune_read_long("/sys/devices/system/cpu/cpu6/cpufreq/cpuinfo_max_freq");
    if (maxfreq > 0) k += (size_t)snprintf(line + k, sizeof(line) - k, " cpu6_scaling_max_mhz=%ld", maxfreq / 1000);
    else k += (size_t)snprintf(line + k, sizeof(line) - k, " cpu6_scaling_max_mhz=n/a");
    if (hardware > 0) k += (size_t)snprintf(line + k, sizeof(line) - k, " cpu6_hardware_max_mhz=%ld", hardware / 1000);
    else k += (size_t)snprintf(line + k, sizeof(line) - k, " cpu6_hardware_max_mhz=n/a");
    char governor[64];
    if (!tune_read_token("/sys/devices/system/cpu/cpu6/cpufreq/scaling_governor", governor, sizeof(governor)))
        snprintf(governor, sizeof(governor), "n/a");
    snprintf(line + k, sizeof(line) - k, " cpu6_governor=%s", governor);
    android_trace("%s", line);
}

static void *tune_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 15 && !atomic_load(&g_android_quit) && !atomic_load(&g_tune_stop); ++i) sleep(1);
    if (atomic_load(&g_android_quit) || atomic_load(&g_tune_stop)) return NULL;
    unsigned long mask = 0; char how[96] = ""; int override_cfg = 0; char cfg[1100];
    snprintf(cfg, sizeof(cfg), "%s/cpu_affinity.txt", g_tune_dir);
    FILE *f = fopen(cfg, "r");
    if (f) {
        char b[32] = {0};
        if (fgets(b, sizeof(b), f)) {
            if (tune_parse_mask(b, &mask)) {
                override_cfg = 1;
                snprintf(how, sizeof(how), "cpu_affinity.txt override");
            } else android_trace("[cpu] invalid cpu_affinity.txt; using autodetection");
        }
        fclose(f);
    }
    if (!override_cfg) mask = tune_detect_big_cores(how, sizeof(how));
    if (mask) {
        cpu_set_t set; CPU_ZERO(&set);
        for (int i = 0; i < TUNE_MAX_CPUS; ++i) if (mask & (1ul << i)) CPU_SET(i, &set);
        cpu_set_t allowed;
        int get_rc = sched_getaffinity(g_tune_game_ktid, sizeof(allowed), &allowed);
        if (get_rc != 0) {
            android_trace("[cpu] sched_getaffinity failed errno=%d; tuning skipped", errno);
        } else {
            unsigned long effective = 0;
            for (int i = 0; i < TUNE_MAX_CPUS; ++i)
                if (CPU_ISSET(i, &set) && CPU_ISSET(i, &allowed)) effective |= 1ul << i;
            CPU_ZERO(&set);
            for (int i = 0; i < TUNE_MAX_CPUS; ++i) if (effective & (1ul << i)) CPU_SET(i, &set);
            if (!effective) android_trace("[cpu] affinity skipped: requested=0x%lx has no allowed cores (%s)", mask, how);
            else {
                int rc = sched_setaffinity(g_tune_game_ktid, sizeof(set), &set);
                int error = rc ? errno : 0;
                android_trace("[cpu] game thread tid=%d requested=0x%lx affinity mask=0x%lx (%s) rc=%d errno=%d", (int)g_tune_game_ktid, mask, effective, how, rc, error);
                int nrc = setpriority(PRIO_PROCESS, (id_t)g_tune_game_ktid, -6);
                error = nrc ? errno : 0;
                android_trace("[cpu] setpriority(-6) rc=%d errno=%d", nrc, error);
            }
        }
    } else {
        android_trace("[cpu] affinity not applied (%s)", how[0] ? how : "disabled by cpu_affinity.txt");
    }
    while (!atomic_load(&g_android_quit) && !atomic_load(&g_tune_stop)) {
        tune_log_sample();
        for (int i = 0; i < 10 && !atomic_load(&g_android_quit) && !atomic_load(&g_tune_stop); ++i) sleep(1);
    }
    return NULL;
}

/* Call from the game thread, right before mercenaries_android_main(). */
static void android_cpu_tuning_start(const char *tmp_dir)
{
    g_tune_game_ktid = (pid_t)syscall(SYS_gettid);
    snprintf(g_tune_dir, sizeof(g_tune_dir), "%s", tmp_dir ? tmp_dir : ".");
    atomic_store(&g_tune_stop, 0);
    int rc = pthread_create(&g_tune_thread, NULL, tune_thread, NULL);
    if (rc == 0) {
        g_tune_started = 1;
        android_trace("[cpu] monitoring started tid=%d; affinity attempt after 15s", (int)g_tune_game_ktid);
    } else android_trace("[cpu] tuning helper creation failed rc=%d", rc);
}

/* Stop before the game thread exits so its Linux TID cannot be reused. */
static void android_cpu_tuning_stop(void)
{
    if (!g_tune_started) return;
    atomic_store(&g_tune_stop, 1);
    pthread_join(g_tune_thread, NULL);
    g_tune_started = 0;
}
