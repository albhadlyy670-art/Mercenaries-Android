#!/usr/bin/env python3
"""Android fix: only accept handles the compat layer created (guest pointers passed to KeWaitForSingleObject
used to be read as objects and block forever in pthread_mutex_lock). Idempotent. Run from the repo root."""
import sys
p = "src/platform/win32_compat.c"
s = open(p, encoding="utf-8").read()
if "w32_ok(" in s:
    print("already patched"); sys.exit(0)
REG = r'''/* Registry of live objects. Guest code (e.g. KeWaitForSingleObject) can pass a pointer into Xbox memory where a
 * Windows HANDLE is expected; Windows rejects such values, but here the pointer would be read as a w32_object and
 * its "lock" taken, which can block forever. Only pointers we created are accepted as handles. */
#define W32_REG_BITS 16
#define W32_REG_CAP  (1u << W32_REG_BITS)
#define W32_TOMB     ((void *)1)
static void *s_reg[W32_REG_CAP];
static pthread_mutex_t s_reg_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned reg_slot(const void *p) { return (unsigned)((((uintptr_t)p >> 4) * 11400714819323198485ull) >> (64 - W32_REG_BITS)); }
static void reg_add(void *p)
{
    pthread_mutex_lock(&s_reg_lock);
    unsigned i = reg_slot(p);
    while (s_reg[i] && s_reg[i] != W32_TOMB) i = (i + 1) & (W32_REG_CAP - 1);
    s_reg[i] = p;
    pthread_mutex_unlock(&s_reg_lock);
}
static void reg_remove(void *p)
{
    pthread_mutex_lock(&s_reg_lock);
    unsigned i = reg_slot(p);
    for (unsigned n = 0; n < W32_REG_CAP && s_reg[i]; ++n, i = (i + 1) & (W32_REG_CAP - 1))
        if (s_reg[i] == p) { s_reg[i] = W32_TOMB; break; }
    pthread_mutex_unlock(&s_reg_lock);
}
static int w32_ok(HANDLE h)
{
    int found = 0;
    if (!h) return 0;
    pthread_mutex_lock(&s_reg_lock);
    unsigned i = reg_slot(h);
    for (unsigned n = 0; n < W32_REG_CAP && s_reg[i]; ++n, i = (i + 1) & (W32_REG_CAP - 1))
        if (s_reg[i] == (void *)h) { found = 1; break; }
    pthread_mutex_unlock(&s_reg_lock);
    return found;
}
static void w32_bad_handle(const char *fn, HANDLE h)
{
    static volatile int reported;
    if (reported < 20) { ++reported; fprintf(stderr, "[w32] %s on a non-handle %p (rejected, like Windows)\n", fn, (void *)h); }
    SetLastError(ERROR_INVALID_HANDLE);
}

'''
def rep(old, new):
    global s
    if old not in s:
        print("PATTERN NOT FOUND:\n" + old[:200]); sys.exit(1)
    s = s.replace(old, new, 1)
rep("static w32_object *obj_alloc(w32_kind kind)", REG + "static w32_object *obj_alloc(w32_kind kind)")
rep("    pthread_cond_init(&o->gate, NULL);\n    return o;\n}", "    pthread_cond_init(&o->gate, NULL);\n    reg_add(o);\n    return o;\n}")
rep("    pthread_mutex_destroy(&o->lock);\n    pthread_cond_destroy(&o->cond);\n    pthread_cond_destroy(&o->gate);\n    free(o);",
    "    reg_remove(o);\n    pthread_mutex_destroy(&o->lock);\n    pthread_cond_destroy(&o->cond);\n    pthread_cond_destroy(&o->gate);\n    free(o);")
rep("        return WAIT_OBJECT_0;\n    return wait_single((w32_object *)h, ms);",
    "        return WAIT_OBJECT_0;\n    if (!w32_ok(h)) { w32_bad_handle(\"WaitForSingleObject\", h); return WAIT_FAILED; }\n    return wait_single((w32_object *)h, ms);")
for kind in ("K_EVENT", "K_SEM", "K_MUTEX"):
    old = "if (!o || o->kind != %s) return FALSE;" % kind
    new = "if (!o || !w32_ok(h) || o->kind != %s) { if (o) w32_bad_handle(\"handle op\", h); return FALSE; }" % kind
    if old not in s:
        print("PATTERN NOT FOUND: " + old); sys.exit(1)
    s = s.replace(old, new)
rep("    obj_release((w32_object *)h);\n    return TRUE;\n}\n\nBOOL DuplicateHandle",
    "    if (!w32_ok(h)) { w32_bad_handle(\"CloseHandle\", h); return FALSE; }\n    obj_release((w32_object *)h);\n    return TRUE;\n}\n\nBOOL DuplicateHandle")
open(p, "w", encoding="utf-8").write(s)
print("patched", p)
