#!/usr/bin/env python3
"""Android fix #10 (idempotent): the Mali driver can crash inside glLinkProgram for particular shader pairs.
 - the link is wrapped (sigsetjmp/siglongjmp from the SEGV handler) so a crash there is caught;
 - the pair's fingerprint is saved to <cache>/link_blacklist.txt and the pair is skipped from then on, in this run
   (in-memory skip list) and in every later run (file);
 - both shaders' GLSL, the GPU/driver strings and the link count go to events.log."""
base = "ports/mercenaries-android/app/src/main/cpp/"
b = base + "d3d11_gles/d3d11_gles_b.inc"
s = open(b, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("gles layer: already applied")
else:
    GUARD = r'''/* LINK_GUARD: survive a driver crash inside glLinkProgram, remember the bad shader pair, never link it again */
#include <setjmp.h>
void android_trace(const char *fmt, ...);
extern const char *g_android_crash_path;
__thread void *g_android_link_guard;
static char g_glsl_v[9000], g_glsl_f[9000];
static unsigned long long g_cur_hash;
static unsigned long long g_bad[128]; static int g_nbad, g_bad_loaded, g_link_count;
static const void *g_skip[64][3]; static int g_nskip;
static unsigned long long fnv64(const char *s, unsigned long long h) { while (s && *s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; } return h; }
static int bad_path(char *out, size_t cap)
{
    if (!g_android_crash_path) return 0;
    snprintf(out, cap, "%s", g_android_crash_path);
    char *sl = strrchr(out, '/'); if (!sl) return 0;
    strcpy(sl + 1, "link_blacklist.txt"); return 1;
}
static void bad_load(void)
{
    if (g_bad_loaded) return; g_bad_loaded = 1;
    char p[900]; if (!bad_path(p, sizeof(p))) return;
    FILE *f = fopen(p, "r"); if (!f) return;
    unsigned long long h; while (g_nbad < 128 && fscanf(f, "%llx", &h) == 1) g_bad[g_nbad++] = h;
    fclose(f);
    android_trace("[gl] loaded %d known-bad shader pairs", g_nbad);
}
static int android_bad_known(void) { bad_load(); for (int i = 0; i < g_nbad; ++i) if (g_bad[i] == g_cur_hash) return 1; return 0; }
static void android_bad_add(void)
{
    if (g_nbad < 128) g_bad[g_nbad++] = g_cur_hash;
    char p[900]; if (!bad_path(p, sizeof(p))) return;
    FILE *f = fopen(p, "a"); if (!f) return; fprintf(f, "%llx\n", g_cur_hash); fclose(f);
}
static int android_skip_has(const void *a, const void *b2, unsigned c)
{ for (int i = 0; i < g_nskip; ++i) if (g_skip[i][0] == a && g_skip[i][1] == b2 && g_skip[i][2] == (const void *)(uintptr_t)c) return 1; return 0; }
static void android_skip_add(const void *a, const void *b2, unsigned c)
{ if (g_nskip < 64) { g_skip[g_nskip][0] = a; g_skip[g_nskip][1] = b2; g_skip[g_nskip][2] = (const void *)(uintptr_t)c; ++g_nskip; } }
static void android_note_glsl(const char *v, const char *f)
{
    snprintf(g_glsl_v, sizeof(g_glsl_v), "%s", v ? v : "(null)");
    snprintf(g_glsl_f, sizeof(g_glsl_f), "%s", f ? f : "(null)");
    g_cur_hash = fnv64(f, fnv64(v, 1469598103934665603ULL));
}
static int android_guarded_link(GLuint prog)
{
    sigjmp_buf jb;
    if (g_link_count == 0)
        android_trace("[gl] renderer=%s | version=%s", (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    ++g_link_count;
    if (g_link_count <= 80) android_trace("[gl] link #%d begin pair=%llx", g_link_count, g_cur_hash);
    if (sigsetjmp(jb, 1) == 0) { g_android_link_guard = &jb; glLinkProgram(prog); g_android_link_guard = NULL; return 1; }
    g_android_link_guard = NULL;
    /* no GL calls from here on: the driver is in an unknown state */
    android_bad_add();
    android_trace("[LINK-CRASH] link #%d pair=%llx crashed in the GPU driver; vertex shader follows", g_link_count, g_cur_hash);
    android_trace("%s", g_glsl_v);
    android_trace("[LINK-CRASH] fragment shader follows");
    android_trace("%s", g_glsl_f);
    android_trace("[LINK-CRASH] end of shaders; pair saved to the blacklist and skipped");
    return 0;
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
                      "    if (android_bad_known()) { free(vsrc); free(p); android_skip_add(vs, ps, bgra_mask); "
                      "android_trace(\"[gl] skipping known-bad shader pair %llx\", g_cur_hash); return NULL; }\n", 1)
        s = s.replace(old2, "glAttachShader(p->prog, f);\n    if (!android_guarded_link(p->prog)) { android_skip_add(vs, ps, bgra_mask); free(p); return NULL; }\n", 1)
        open(b, "w", encoding="utf-8").write(s)
        print("gles layer: applied")

fpath = base + "android/android_fault.c"
s = open(fpath, encoding="utf-8").read()
if "LINK_GUARD" in s:
    print("android_fault.c: already applied")
else:
    old = "static void segv_handler(int sig, siginfo_t *si, void *ucv)\n{\n"
    if old not in s:
        print("WARNING: segv_handler not found")
    else:
        new = old + """    { /* LINK_GUARD: a crash inside glLinkProgram is recoverable (see android_guarded_link) */
        extern __thread void *g_android_link_guard;
        if (g_android_link_guard && sig == SIGSEGV) {
            void *g_ = g_android_link_guard; g_android_link_guard = NULL;
            siglongjmp(*(sigjmp_buf *)g_, 1);
        }
    }
"""
        s = s.replace(old, new, 1)
        if "<setjmp.h>" not in s:
            s = s.replace("#include <dlfcn.h>\n", "#include <dlfcn.h>\n#include <setjmp.h>\n", 1)
        open(fpath, "w", encoding="utf-8").write(s)
        print("android_fault.c: applied")
