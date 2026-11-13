#!/usr/bin/env python3
"""Android fix #10 (idempotent): the Mali driver crashes inside glLinkProgram for many shader pairs.
 - while glLinkProgram runs, a plain global flag is set; if the process takes a fatal signal in that window the crash
   handler appends the pair's fingerprint to <cache>/link_blacklist.txt using only async-signal-safe calls;
 - pairs in that file (and pairs skipped earlier this run) are never linked again: the draw is simply skipped;
 - every pair gets a log line with simple facts (attribute/varying/sampler counts, sizes) in events.log."""
base = "ports/mercenaries-android/app/src/main/cpp/"
b = base + "d3d11_gles/d3d11_gles_b.inc"
s = open(b, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("gles layer: already applied")
else:
    GUARD = r'''/* LINK_GUARD: remember shader pairs whose glLinkProgram kills the process, and never link them again */
#include <unistd.h>
void android_trace(const char *fmt, ...);
extern const char *g_android_crash_path;
volatile int g_android_linking;
unsigned long long g_cur_hash;
static char g_glsl_v[9000], g_glsl_f[9000];
static unsigned long long g_bad[256]; static int g_nbad, g_bad_loaded, g_link_count, g_pair_count;
static const void *g_skip[128][3]; static int g_nskip;
static unsigned long long fnv64(const char *s, unsigned long long h) { while (s && *s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; } return h; }
static int bad_path(char *out, size_t cap)
{
    if (!g_android_crash_path) return 0;
    snprintf(out, cap, "%s", g_android_crash_path);
    char *sl = strrchr(out, '/'); if (!sl) return 0;
    strcpy(sl + 1, "link_blacklist.txt"); return 1;
}
static int pending_path(char *out, size_t cap)
{
    if (!bad_path(out, cap)) return 0;
    char *sl = strrchr(out, '/'); strcpy(sl + 1, "link_pending.txt"); return 1;
}
static void bad_load(void)
{
    if (g_bad_loaded) return; g_bad_loaded = 1;
    char p[900]; if (!bad_path(p, sizeof(p))) return;
    FILE *f = fopen(p, "r");
    if (f) { unsigned long long h; while (g_nbad < 256 && fscanf(f, "%llx", &h) == 1) g_bad[g_nbad++] = h; fclose(f); }
    /* a leftover marker means the previous run died while linking that pair: remember it */
    char q[900];
    if (pending_path(q, sizeof(q))) {
        FILE *pf = fopen(q, "r");
        if (pf) {
            unsigned long long h = 0;
            if (fscanf(pf, "%llx", &h) == 1 && g_nbad < 256) {
                g_bad[g_nbad++] = h;
                FILE *af = fopen(p, "a"); if (af) { fprintf(af, "%llx\n", h); fclose(af); }
                android_trace("[gl] the previous run died while linking pair %llx: added to the skip list", h);
            }
            fclose(pf); unlink(q);
        }
    }
    android_trace("[gl] loaded %d known-bad shader pairs", g_nbad);
}
static int android_bad_known(void) { bad_load(); for (int i = 0; i < g_nbad; ++i) if (g_bad[i] == g_cur_hash) return 1; return 0; }
static int android_skip_has(const void *a, const void *b2, unsigned c)
{ for (int i = 0; i < g_nskip; ++i) if (g_skip[i][0] == a && g_skip[i][1] == b2 && g_skip[i][2] == (const void *)(uintptr_t)c) return 1; return 0; }
static void android_skip_add(const void *a, const void *b2, unsigned c)
{ if (g_nskip < 128) { g_skip[g_nskip][0] = a; g_skip[g_nskip][1] = b2; g_skip[g_nskip][2] = (const void *)(uintptr_t)c; ++g_nskip; } }
static void android_note_glsl(const char *v, const char *f)
{
    snprintf(g_glsl_v, sizeof(g_glsl_v), "%s", v ? v : "(null)");
    snprintf(g_glsl_f, sizeof(g_glsl_f), "%s", f ? f : "(null)");
    g_cur_hash = fnv64(f, fnv64(v, 1469598103934665603ULL));
}
static int g_allow_complex = -1;
static int android_allow_complex(void)
{   /* create an empty file named allow_complex in the app cache folder to link EVERY shader pair (for experiments) */
    if (g_allow_complex < 0) {
        char p[900]; g_allow_complex = 0;
        if (bad_path(p, sizeof(p))) { char *sl = strrchr(p, '/'); strcpy(sl + 1, "allow_complex"); FILE *f = fopen(p, "r"); if (f) { fclose(f); g_allow_complex = 1; } }
    }
    return g_allow_complex;
}
static int android_guarded_link(GLuint prog)
{
    if (g_link_count == 0)
        android_trace("[gl] renderer=%s | version=%s", (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    ++g_link_count;
    if (g_link_count <= 120) android_trace("[gl] link #%d begin pair=%llx", g_link_count, g_cur_hash);
    { char q[900]; if (pending_path(q, sizeof(q))) { FILE *pf = fopen(q, "w"); if (pf) { fprintf(pf, "%llx\n", g_cur_hash); fclose(pf); } } }
    g_android_linking = 1;               /* if the driver kills us in here, the marker file (and the crash handler) record g_cur_hash */
    glLinkProgram(prog);
    g_android_linking = 0;
    { char q[900]; if (pending_path(q, sizeof(q))) unlink(q); }
    return 1;
}

'''
    key = "static Program *get_program(Shader *vs, Shader *ps, uint32_t bgra_mask)\n{\n"
    old1 = "char *vsrc = build_vs_source(vs, ps, bgra_mask);\n"
    old2 = "glAttachShader(p->prog, f); glLinkProgram(p->prog);\n"
    if key not in s or old1 not in s or old2 not in s:
        print("WARNING: patterns not found in d3d11_gles_b.inc")
    else:
        s = s.replace(key, GUARD + key + "    if (android_skip_has(vs, ps, bgra_mask)) return NULL;\n", 1)
        s = s.replace(old1, old1 + "    android_note_glsl(vsrc, ps->glsl);\n"
                      "    if (++g_pair_count <= 200)\n"
                      "        android_trace(\"[gl] pair %llx: vs_in=%d vs_out=%d vs_samp=%d vs_cb=%d | ps_in=%d ps_samp=%d ps_cb=%d | glsl %d/%d bytes | bgra=%x %s\", g_cur_hash,\n"
                      "                      vs->r.n_inputs, vs->r.n_varyings, vs->r.n_samplers, vs->r.n_cbufs, ps->r.n_varyings, ps->r.n_samplers, ps->r.n_cbufs,\n"
                      "                      (int)strlen(g_glsl_v), (int)strlen(g_glsl_f), (unsigned)bgra_mask, android_bad_known() ? \"SKIP(known-bad)\" : (vs->r.n_inputs >= 4 && !android_allow_complex()) ? \"SKIP(heuristic: 4+ vertex inputs crash the Mali linker)\" : \"link\");\n"
                      "    if (android_bad_known() || (vs->r.n_inputs >= 4 && !android_allow_complex())) { free(vsrc); free(p); android_skip_add(vs, ps, bgra_mask); return NULL; }\n", 1)
        s = s.replace(old2, "glAttachShader(p->prog, f);\n    android_guarded_link(p->prog);\n", 1)
        open(b, "w", encoding="utf-8").write(s)
        print("gles layer: applied")

fpath = base + "android/android_fault.c"
s = open(fpath, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("android_fault.c: already applied")
else:
    h1 = "static void segv_handler(int sig, siginfo_t *si, void *ucv)\n{\n"
    h2 = "static void other_handler(int sig, siginfo_t *si, void *ucv)\n{\n"
    if h1 not in s or h2 not in s:
        print("WARNING: handlers not found")
    else:
        FUNC = r'''/* LINK_GUARD: called from a fatal-signal handler while glLinkProgram runs. Only async-signal-safe calls. */
extern volatile int g_android_linking;
extern unsigned long long g_cur_hash;
static void record_bad_pair(void)
{
    if (!g_android_linking || !g_android_crash_path) return;
    char path[900]; size_t n = 0, last = 0;
    while (g_android_crash_path[n] && n < sizeof(path) - 24) { path[n] = g_android_crash_path[n]; if (path[n] == '/') last = n; ++n; }
    const char *nm = "link_blacklist.txt";
    size_t k = last + 1; for (size_t i = 0; nm[i]; ++i) path[k++] = nm[i]; path[k] = 0;
    char line[24]; int len = 0; unsigned long long v = g_cur_hash;
    for (int sh = 60; sh >= 0; sh -= 4) line[len++] = "0123456789abcdef"[(v >> sh) & 15];
    line[len++] = '\n';
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { ssize_t r = write(fd, line, (size_t)len); (void)r; close(fd); }
    g_android_linking = 0;
}

'''
        s = s.replace(h1, FUNC + h1 + "    record_bad_pair();   /* LINK_GUARD */\n", 1)
        s = s.replace(h2, h2 + "    record_bad_pair();   /* LINK_GUARD */\n", 1)
        open(fpath, "w", encoding="utf-8").write(s)
        print("android_fault.c: applied")
