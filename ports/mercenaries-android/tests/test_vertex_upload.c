/* Exercise the GLES transient-vertex upload against a mock driver. */
#include <d3d11.h>
#include "gles_gl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char storage[128];
static GLintptr mapped_offset; static GLsizeiptr mapped_size;
static GLbitfield mapped_flags; static int map_fail, unmap_fail, orphan_count, map_count, bind_count;
GLenum glGetError(void) { return GL_NO_ERROR; }
void glGenBuffers(GLsizei n, GLuint *out) { assert(n == 1); *out = 7; }
void glDeleteBuffers(GLsizei n, const GLuint *out) { (void)n; (void)out; }
void glBindBuffer(GLenum target, GLuint id) { assert(target == GL_COPY_WRITE_BUFFER && id == 7); ++bind_count; }
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{ (void)target; (void)data; (void)usage; assert(size <= (GLsizeiptr)sizeof(storage)); memset(storage, 0, sizeof(storage)); ++orphan_count; }
void glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const void *data)
{ (void)target; assert(off >= 0 && size >= 0 && off + size <= (GLintptr)sizeof(storage)); memcpy(storage + off, data, size); }
void *glMapBufferRange(GLenum target, GLintptr off, GLsizeiptr size, GLbitfield flags)
{
    (void)target; ++map_count; mapped_offset = off; mapped_size = size; mapped_flags = flags;
    if (map_fail) return NULL;
    if (flags & GL_MAP_INVALIDATE_BUFFER_BIT) memset(storage, 0, sizeof(storage));
    assert(off >= 0 && size > 0 && off + size <= (GLintptr)sizeof(storage)); return storage + off;
}
GLboolean glUnmapBuffer(GLenum target) { (void)target; return unmap_fail ? GL_FALSE : GL_TRUE; }
int main(void)
{
    ID3D11Buffer *buffer = NULL;
    D3D11_BUFFER_DESC desc = { 128, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    const unsigned char a[] = {1, 2, 3, 4}, b[] = {5, 6, 7};
    assert(SUCCEEDED(d3dgl_Device_CreateBuffer(NULL, &desc, NULL, &buffer)));
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(buffer, 16, a, sizeof(a), FALSE)));
    assert(mapped_offset == 16 && mapped_size == 4 && (mapped_flags & GL_MAP_UNSYNCHRONIZED_BIT));
    assert(!memcmp(storage + 16, a, sizeof(a)));
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(buffer, 40, b, sizeof(b), FALSE)));
    assert(!memcmp(storage + 16, a, sizeof(a)) && !memcmp(storage + 40, b, sizeof(b)));
    assert(bind_count == 1); /* creation and repeated uploads reuse the binding */
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(buffer, 0, a, sizeof(a), TRUE)));
    assert((mapped_flags & GL_MAP_INVALIDATE_BUFFER_BIT) && !(mapped_flags & GL_MAP_UNSYNCHRONIZED_BIT));
    assert(!memcmp(storage, a, sizeof(a)));
    map_fail = 1;
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(buffer, 40, b, sizeof(b), FALSE)));
    assert(!memcmp(storage, a, sizeof(a)) && !memcmp(storage + 40, b, sizeof(b)));
    int before = orphan_count;
    assert(SUCCEEDED(d3d11gles_upload_vertex_range(buffer, 0, b, sizeof(b), TRUE)));
    assert(orphan_count == before + 1 && !memcmp(storage, b, sizeof(b)) && storage[40] == 0);
    map_fail = 0; unmap_fail = 1;
    assert(FAILED(d3d11gles_upload_vertex_range(buffer, 0, a, sizeof(a), FALSE)));
    before = map_count;
    assert(FAILED(d3d11gles_upload_vertex_range(buffer, 127, a, sizeof(a), FALSE)) && map_count == before);
    d3dgl_Release(buffer);
    puts("vertex upload: ranges, discard, fallback and failures OK"); return 0;
}
