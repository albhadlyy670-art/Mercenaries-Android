#define _POSIX_C_SOURCE 200809L
#include <d3d11.h>
#include "gles_gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char memory[64][128];
static unsigned next_id, bound, maps, unmaps, fences, waits, deleted_sync, buffer_deletes;
static int support = 1, storage_error, fail_map, wait_failure, fail_fence;
static GLenum error;
GLenum glGetError(void) { GLenum e = error; error = 0; return e; }
void glGetIntegerv(GLenum what, GLint *v) { assert(what == GL_NUM_EXTENSIONS); *v = support ? 1 : 0; }
const GLubyte *glGetStringi(GLenum what, GLuint i) { assert(what == GL_EXTENSIONS && i == 0); return (const GLubyte *)"GL_EXT_buffer_storage"; }
void glGenBuffers(GLsizei n, GLuint *id) { assert(n == 1 && next_id < 63); *id = ++next_id; }
void glDeleteBuffers(GLsizei n, const GLuint *id) { assert(n == 1 && *id); ++buffer_deletes; }
void glBindBuffer(GLenum t, GLuint id) { assert(t == GL_COPY_WRITE_BUFFER); bound = id; }
void glBufferData(GLenum t, GLsizeiptr n, const void *p, GLenum usage) { (void)t; (void)n; (void)p; (void)usage; }
void glBufferSubData(GLenum t, GLintptr o, GLsizeiptr n, const void *p) { (void)t; memcpy(memory[bound] + o, p, n); }
void d3dgl_test_buffer_storage(GLenum t, GLsizeiptr n, const void *p, GLbitfield bits)
{ assert(t == GL_COPY_WRITE_BUFFER && n == 128 && !p && (bits & 0xc2) == 0xc2 && bound); if (storage_error) error = 0x502; }
void *glMapBufferRange(GLenum t, GLintptr off, GLsizeiptr n, GLbitfield bits)
{ assert(t == GL_COPY_WRITE_BUFFER && off >= 0 && off + n <= 128 && bound); ++maps; if (fail_map) return NULL; (void)bits; return memory[bound] + off; }
GLboolean glUnmapBuffer(GLenum t) { assert(t == GL_COPY_WRITE_BUFFER); ++unmaps; return GL_TRUE; }
GLsync glFenceSync(GLenum t, GLbitfield f) { assert(t == GL_SYNC_GPU_COMMANDS_COMPLETE && f == 0); ++fences; return fail_fence ? NULL : (GLsync)(uintptr_t)fences; }
GLenum glClientWaitSync(GLsync s, GLbitfield flags, uint64_t timeout)
{ assert(s && flags == GL_SYNC_FLUSH_COMMANDS_BIT && timeout); ++waits; if (wait_failure) return 0x911d; return waits % 2 ? GL_TIMEOUT_EXPIRED : GL_CONDITION_SATISFIED; }
void glDeleteSync(GLsync s) { assert(s); ++deleted_sync; }
int main(void)
{
    D3D11_BUFFER_DESC d = { 128, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    ID3D11Buffer *b = NULL; const unsigned char a[] = {1, 2, 3, 4}, bdata[] = {5, 6, 7, 8}, cdata[] = {9, 10, 11, 12};
    assert(SUCCEEDED(d3dgl_Device_CreateBuffer(NULL, &d, NULL, &b)));
    assert(!setenv("D3DGL_NO_PERSISTENT_RING", "1", 1));
    assert(!d3d11gles_enable_persistent_ring(b)); assert(!unsetenv("D3DGL_NO_PERSISTENT_RING"));
    support = 0; assert(!d3d11gles_enable_persistent_ring(b)); support = 1;
    storage_error = 1; assert(!d3d11gles_enable_persistent_ring(b)); storage_error = 0;
    fail_map = 1; assert(!d3d11gles_enable_persistent_ring(b)); fail_map = 0;
    assert(d3d11gles_enable_persistent_ring(b));
    unsigned gen0 = bound, initial_maps = maps;
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 0, a, sizeof(a), FALSE)));
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 16, a, sizeof(a), FALSE)));
    assert(maps == initial_maps && fences == 0 && !memcmp(memory[gen0] + 16, a, sizeof(a)));
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 0, bdata, sizeof(bdata), TRUE)));
    unsigned gen1 = bound; assert(gen1 != gen0 && fences == 1 && waits == 0);
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 0, cdata, sizeof(cdata), TRUE)));
    unsigned gen2 = bound; assert(gen2 != gen1 && gen2 != gen0 && fences == 2 && waits == 0);
    assert(!memcmp(memory[gen0], a, sizeof(a)) && !memcmp(memory[gen1], bdata, sizeof(bdata)) && !memcmp(memory[gen2], cdata, sizeof(cdata)));
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 0, a, sizeof(a), TRUE)));
    assert(fences == 3 && waits == 2 && deleted_sync == 1);
    /* gen1 is still in flight: no writes on wait failure. A retry replaces
     * gen0's older fence so subsequent reads from it remain protected. */
    wait_failure = 1; memory[gen1][0] = 9;
    assert(FAILED(d3d11gles_upload_vertex_range(b, 0, a, sizeof(a), TRUE)));
    assert(memory[gen1][0] == 9);
    wait_failure = 0;
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(b, 0, a, sizeof(a), TRUE)));
    assert(!memcmp(memory[gen1], a, sizeof(a)) && fences == 5 && deleted_sync == 3);
    fail_fence = 1; assert(FAILED(d3d11gles_upload_vertex_range(b, 0, a, sizeof(a), TRUE))); fail_fence = 0;
    d3dgl_Release(b); assert(unmaps == 3 && deleted_sync == 5);
    d.BindFlags = D3D11_BIND_INDEX_BUFFER; b = NULL;
    assert(SUCCEEDED(d3dgl_Device_CreateBuffer(NULL, &d, NULL, &b)));
    assert(d3d11gles_enable_persistent_ring(b)); unsigned only = bound;
    assert(SUCCEEDED(d3d11gles_upload_index_range(b, 0, a, sizeof(a), FALSE)));
    /* Out-of-memory during generation growth falls back to a fenced wait. */
    storage_error = 1; unsigned prior_waits = waits;
    assert(SUCCEEDED(d3d11gles_upload_index_range(b, 0, a, sizeof(a), TRUE)));
    assert(waits > prior_waits && !memcmp(memory[only], a, sizeof(a)));
    storage_error = 0;
    assert(SUCCEEDED(d3d11gles_upload_index_range(b, 0, a, sizeof(a), TRUE)));
    assert(bound != only);
    d3dgl_Release(b); assert(unmaps == 5 && buffer_deletes > 0);
    puts("persistent ring: rotation, fence retry, allocation fallback and cleanup OK");
}
