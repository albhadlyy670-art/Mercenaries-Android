#include "android_shader_guard.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

extern const char *g_android_crash_path;
void android_trace(const char *fmt, ...);

/* Versioned separately from the old link guard: its delayed marker could
 * incorrectly blacklist a successful shader after an unrelated later crash. */
#define MAX_BAD 256
static uint64_t bad[MAX_BAD];
static unsigned bad_count;
static unsigned char bad_reported[MAX_BAD];
static int loaded;

static int guard_path(char *out, size_t size, const char *name)
{
    if (!g_android_crash_path) return 0;
    const char *slash = strrchr(g_android_crash_path, '/');
    if (!slash) return 0;
    size_t dirlen = (size_t)(slash - g_android_crash_path + 1);
    size_t len = strlen(name);
    if (dirlen + len + 1 > size) return 0;
    memcpy(out, g_android_crash_path, dirlen);
    memcpy(out + dirlen, name, len + 1);
    return 1;
}

static uint64_t hash_string(uint64_t h, const char *s)
{
    if (!s) return h;
    while (*s) { h ^= (unsigned char)*s++; h *= UINT64_C(1099511628211); }
    return h;
}
uint64_t android_shader_pair_hash(const char *vertex, const char *fragment)
{
    return hash_string(hash_string(UINT64_C(1469598103934665603), vertex), fragment);
}
static void load_bad(void)
{
    char path[1024], pending[1024], stage[32];
    uint64_t hash;
    if (loaded) return;
    loaded = 1;
    if (!guard_path(path, sizeof(path), "shader_blacklist_v7.txt") ||
        !guard_path(pending, sizeof(pending), "shader_pending_v7.txt")) return;
    FILE *f = fopen(path, "r");
    if (f) {
        while (bad_count < MAX_BAD && fscanf(f, "%" SCNx64, &hash) == 1)
            bad[bad_count++] = hash;
        fclose(f);
    }
    f = fopen(pending, "r");
    if (!f) return;
    int valid = fscanf(f, "%" SCNx64 " %31s", &hash, stage) == 2;
    fclose(f);
    if (valid && bad_count < MAX_BAD) {
        FILE *out = fopen(path, "a");
        if (!out) return; /* keep the pending marker for the next run */
        int written = fprintf(out, "%016" PRIx64 "\n", hash) > 0;
        if (fclose(out) != 0) written = 0;
        if (!written) return;
        bad[bad_count++] = hash;
        android_trace("[gl] previous run ended during %s for pair %016" PRIx64 "; skipping it", stage, hash);
    }
    unlink(pending);
}
int android_shader_pair_bad(uint64_t hash)
{
    load_bad();
    for (unsigned i = 0; i < bad_count; ++i) {
        if (bad[i] == hash) {
            if (!bad_reported[i]) {
                bad_reported[i] = 1;
                android_trace("[gl] skipping previously interrupted shader pair %016" PRIx64, hash);
            }
            return 1;
        }
    }
    return 0;
}
void android_shader_guard_begin(uint64_t hash, const char *stage)
{
    char path[1024];
    if (!guard_path(path, sizeof(path), "shader_pending_v7.txt")) return;
    FILE *f = fopen(path, "w");
    if (!f) { android_trace("[gl] could not write shader crash marker"); return; }
    fprintf(f, "%016" PRIx64 " %s\n", hash, stage);
    fclose(f); /* close before entering a driver call that might crash */
    android_trace("[gl] %s begin pair=%016" PRIx64, stage, hash);
}
void android_shader_guard_end(void)
{
    char path[1024];
    if (guard_path(path, sizeof(path), "shader_pending_v7.txt")) unlink(path);
}
