#!/usr/bin/env python3
"""Apply the persistent-mapped vertex/index ring optimisation to d3d11_gles_a.inc.

Usage (from the repo root):
    python3 apply_persistent_ring.py
    python3 apply_persistent_ring.py path/to/d3d11_gles_a.inc

Idempotent: running it twice does nothing the second time. It refuses to touch the file
if any expected anchor text is missing (so it never half-applies).
"""
import sys, pathlib

DEFAULT = "ports/mercenaries-android/app/src/main/cpp/d3d11_gles/d3d11_gles_a.inc"
MARK = "persistently mapped transient rings"

PR_BLOCK = r'''
/* ---- persistently mapped transient rings (Android, EXT_buffer_storage) ---------------------------
 * The renderer uploads every guest draw's vertices/indices into a dynamic ring buffer with
 * glMapBufferRange + memcpy + glUnmapBuffer (about one map/unmap pair per draw). On mobile drivers the
 * unmap alone costs several microseconds, which adds up to a large share of the game thread.
 * With EXT_buffer_storage the ring is mapped once, coherently, and uploads become a plain memcpy.
 * GL cannot orphan immutable storage, so a ring "discard" rotates to the next of PR_GENS buffers.
 * A fence on each retired buffer guarantees the GPU is done with it before it is written again.
 * If the extension, the entry point or any allocation is missing, the classic path is used unchanged.
 * Set D3DGL_NO_PERSISTENT_RING=1 to force the classic path. */
#ifdef __ANDROID__
#ifndef GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_PERSISTENT_BIT_EXT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT_EXT
#define GL_MAP_COHERENT_BIT_EXT 0x0080
#endif
#ifndef GL_DYNAMIC_STORAGE_BIT_EXT
#define GL_DYNAMIC_STORAGE_BIT_EXT 0x0100
#endif
#define PR_GENS 3
typedef struct { GLuint buf; uint8_t *ptr; GLsync fence; } PrGen;
typedef struct PersistRing { PrGen gen[PR_GENS]; int cur; } PersistRing;
typedef void (*PrBufferStorageFn)(GLenum, GLsizeiptr, const void *, GLbitfield);
static PrBufferStorageFn pr_buffer_storage;
static int pr_state;   /* 0 = not probed, 1 = usable, -1 = unavailable */

static int pr_available(void)
{
    if (pr_state) return pr_state > 0;
    pr_state = -1;
    const char *dis = getenv("D3DGL_NO_PERSISTENT_RING");
    if (dis && *dis && *dis != '0') { fprintf(stderr, "[d3d11gles] persistent rings disabled by D3DGL_NO_PERSISTENT_RING\n"); return 0; }
    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    if (!ext || !strstr(ext, "GL_EXT_buffer_storage")) { fprintf(stderr, "[d3d11gles] persistent rings unavailable: GL_EXT_buffer_storage not exposed\n"); return 0; }
    typedef void *(*GetProcFn)(const char *);
    void *egl = dlopen("libEGL.so", RTLD_NOW);
    GetProcFn get_proc = egl ? (GetProcFn)dlsym(egl, "eglGetProcAddress") : NULL;
    if (!get_proc) { fprintf(stderr, "[d3d11gles] persistent rings unavailable: eglGetProcAddress not found\n"); return 0; }
    pr_buffer_storage = (PrBufferStorageFn)get_proc("glBufferStorageEXT");
    if (!pr_buffer_storage) { fprintf(stderr, "[d3d11gles] persistent rings unavailable: glBufferStorageEXT missing\n"); return 0; }
    pr_state = 1;
    fprintf(stderr, "[d3d11gles] persistent mapped vertex/index rings enabled (EXT_buffer_storage)\n");
    return 1;
}
static void pr_drain_errors(void) { for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; ++i) {} }
static void pr_release_gen(PrGen *g)
{
    if (g->fence) { glDeleteSync(g->fence); g->fence = NULL; }
    if (g->buf) {
        if (g->ptr) { bind_copy_write_buffer(g->buf); glUnmapBuffer(GL_COPY_WRITE_BUFFER); }
        glDeleteBuffers(1, &g->buf); G.copy_write_valid = 0;
    }
    g->buf = 0; g->ptr = NULL;
}
static int pr_make_gen(const Buf *b, PrGen *g)
{
    const GLbitfield sflags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT | GL_DYNAMIC_STORAGE_BIT_EXT;
    const GLbitfield mflags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
    GLsizeiptr sz = (GLsizeiptr)((b->d.ByteWidth + 15u) & ~15u);
    memset(g, 0, sizeof(*g));
    pr_drain_errors();
    glGenBuffers(1, &g->buf); if (!g->buf) return 0;
    bind_copy_write_buffer(g->buf);
    pr_buffer_storage(GL_COPY_WRITE_BUFFER, sz, NULL, sflags);
    g->ptr = (uint8_t *)glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, sz, mflags);
    if (glGetError() != GL_NO_ERROR || !g->ptr) {
        pr_drain_errors();
        if (g->ptr) glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        glDeleteBuffers(1, &g->buf); G.copy_write_valid = 0;
        g->buf = 0; g->ptr = NULL; return 0;
    }
    return 1;
}
static void pr_destroy(Buf *b)
{
    PersistRing *r = b->pr; if (!r) return;
    for (int i = 0; i < PR_GENS; ++i) pr_release_gen(&r->gen[i]);
    free(r); b->pr = NULL; b->buf = 0;   /* the current GL name was owned by the ring */
}
static int pr_init(Buf *b)
{
    PersistRing *r = (PersistRing *)calloc(1, sizeof(*r)); if (!r) return 0;
    if (!pr_make_gen(b, &r->gen[0])) { free(r); return 0; }
    if (b->buf) { glDeleteBuffers(1, &b->buf); G.copy_write_valid = 0; }   /* replace the mutable buffer */
    r->cur = 0; b->buf = r->gen[0].buf; b->pr = r; return 1;
}
/* Equivalent of D3D11_MAP_WRITE_DISCARD for the ring: move to the next buffer. */
static void pr_rotate(Buf *b)
{
    PersistRing *r = b->pr; PrGen *old = &r->gen[r->cur];
    if (!old->fence) { old->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); if (!old->fence) glFinish(); }
    int next = (r->cur + 1) % PR_GENS; PrGen *n = &r->gen[next];
    if (!n->buf) {
        if (!pr_make_gen(b, n)) { glFinish(); return; }   /* cannot grow: wait for the GPU and keep reusing the current buffer */
    } else if (n->fence) {
        GLenum w = glClientWaitSync(n->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (w == GL_TIMEOUT_EXPIRED) w = glClientWaitSync(n->fence, GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64)100000000u);
        if (w != GL_ALREADY_SIGNALED && w != GL_CONDITION_SATISFIED) glFinish();
        glDeleteSync(n->fence); n->fence = NULL;
    }
    r->cur = next; b->buf = n->buf;
}
static int pr_upload(Buf *b, UINT offset, const void *data, UINT size, BOOL discard)
{
    if (b->pr_off || !pr_available()) return 0;
    if (!b->pr) { if (!pr_init(b)) { b->pr_off = 1; return 0; } }
    else if (discard) pr_rotate(b);
    memcpy(b->pr->gen[b->pr->cur].ptr + offset, data, size);
    return 1;
}
#else
static void pr_destroy(Buf *b) { (void)b; }
#endif

'''

# (anchor, replacement) pairs. Each anchor must occur exactly once.
EDITS = [
 # 1. dlfcn for dlopen/dlsym
 ('#include <android/log.h>\n#define D3DGL_LOG',
  '#include <android/log.h>\n#include <dlfcn.h>\n#define D3DGL_LOG'),
 # 2. Buf gets ring state
 ('struct Buf { Obj o; D3D11_BUFFER_DESC d; GLuint buf; uint8_t *tmp; D3D11_MAP map_type; int mapped_gl; };',
  '/* pr / pr_off: persistently mapped ring state for dynamic vertex/index buffers\n'
  ' * (Android, EXT_buffer_storage). NULL / 0 means the classic map/unmap path. */\n'
  'struct Buf { Obj o; D3D11_BUFFER_DESC d; GLuint buf; uint8_t *tmp; D3D11_MAP map_type; int mapped_gl; struct PersistRing *pr; int pr_off; };'),
 # 3. ring implementation + destroy hook
 ('static void buf_destroy(Obj *o) { Buf *b = (Buf *)o; G.uniform_valid = 0;',
  PR_BLOCK + 'static void buf_destroy(Obj *o) { Buf *b = (Buf *)o; pr_destroy(b); G.uniform_valid = 0;'),
 # 4. upload path
 ('        return E_INVALIDARG;\n    bind_copy_write_buffer(b->buf);\n    GLbitfield flags = GL_MAP_WRITE_BIT | (discard',
  '        return E_INVALIDARG;\n#ifdef __ANDROID__\n'
  '    {   /* Persistent mapping: a memcpy replaces map + copy + unmap. Counted under ring_maps for comparison. */\n'
  '        PERF_BEGIN(ringmap);\n        int done = pr_upload(b, offset, data, size, discard);\n        PERF_END(ringmap);\n        if (done) return S_OK;\n    }\n#endif\n'
  '    bind_copy_write_buffer(b->buf);\n    GLbitfield flags = GL_MAP_WRITE_BIT | (discard'),
 # 5. Map on a ring buffer
 ('        Buf *b = (Buf *)o; GLbitfield acc = 0;\n        switch (type)',
  '        Buf *b = (Buf *)o; GLbitfield acc = 0;\n#ifdef __ANDROID__\n'
  '        if (b->pr) {   /* persistently mapped ring: hand out the live pointer, no GL call */\n'
  '            if (type == D3D11_MAP_WRITE_DISCARD) pr_rotate(b);\n'
  '            b->map_type = type; m->pData = b->pr->gen[b->pr->cur].ptr; m->RowPitch = b->d.ByteWidth; m->DepthPitch = b->d.ByteWidth; return S_OK;\n        }\n#endif\n'
  '        switch (type)'),
 # 6. Unmap on a ring buffer
 ('        Buf *b = (Buf *)o; bind_copy_write_buffer(b->buf);\n        if (b->mapped_gl) { PERF_BEGIN(unmap);',
  '        Buf *b = (Buf *)o;\n#ifdef __ANDROID__\n        if (b->pr) return;   /* persistently mapped: coherent, nothing to unmap */\n#endif\n'
  '        bind_copy_write_buffer(b->buf);\n        if (b->mapped_gl) { PERF_BEGIN(unmap);'),
]

def main():
    path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else DEFAULT)
    if not path.is_file():
        sys.exit(f"not found: {path} (run from the repo root or pass the path)")
    text = path.read_bytes().decode("utf-8")
    crlf = "\r\n" in text
    if crlf: text = text.replace("\r\n", "\n")
    if MARK in text:
        print("already applied; nothing to do"); return
    for i, (old, _) in enumerate(EDITS, 1):
        n = text.count(old)
        if n != 1:
            sys.exit(f"edit {i}: anchor found {n} times (expected 1) - file differs from the inspected revision; nothing written")
    for old, new in EDITS:
        text = text.replace(old, new, 1)
    if crlf: text = text.replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))
    print(f"patched {path} ({len(EDITS)} edits)")

if __name__ == "__main__":
    main()
