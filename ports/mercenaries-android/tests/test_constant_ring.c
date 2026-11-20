#define _POSIX_C_SOURCE 200809L
#define D3DGL_TEST_PERSISTENT 1
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
#include <assert.h>
static unsigned char *memory[64];
static unsigned next_id, bound, maps, unmaps, fences, waits, deleted, range_calls, base_calls;
static GLuint range_buffer; static GLintptr range_offset; static GLsizeiptr range_size;
static int support = 1, fail_storage, fail_fence, fail_wait, fail_map;
static GLenum error;
GLenum glGetError(void) { GLenum e=error; error=0; return e; }
void glGetIntegerv(GLenum what, GLint *v) {
    if (what==GL_NUM_EXTENSIONS) *v=support ? 1 : 0;
    else { assert(what==GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT); *v=256; }
}
const GLubyte *glGetStringi(GLenum what, GLuint i) { assert(what==GL_EXTENSIONS && !i); return (const GLubyte *)"GL_EXT_buffer_storage"; }
void glGenBuffers(GLsizei n, GLuint *id) { assert(n==1 && next_id<63); *id=++next_id; }
void glDeleteBuffers(GLsizei n, const GLuint *id) { assert(n==1 && *id); free(memory[*id]); memory[*id]=NULL; }
void glBindBuffer(GLenum t, GLuint id) { assert(t==GL_COPY_WRITE_BUFFER); bound=id; }
void glBufferData(GLenum t, GLsizeiptr n, const void *p, GLenum u) { (void)t; (void)n; (void)p; (void)u; }
void glBufferSubData(GLenum t, GLintptr o, GLsizeiptr n, const void *p) { (void)t; (void)o; (void)n; (void)p; }
void d3dgl_test_buffer_storage(GLenum t, GLsizeiptr n, const void *p, GLbitfield flags) {
    assert(t==GL_COPY_WRITE_BUFFER && (n==1048576 || n==1024 || n==256) && !p && (flags&0xc2)==0xc2);
    if (fail_storage) { error=0x502; return; }
    memory[bound]=calloc(1,(size_t)n); assert(memory[bound]);
}
void *glMapBufferRange(GLenum t, GLintptr o, GLsizeiptr n, GLbitfield flags) {
    (void)flags; assert(t==GL_COPY_WRITE_BUFFER && o==0 && (n==1048576 || n==1024 || n==256) && memory[bound]); ++maps;
    return fail_map ? NULL : memory[bound];
}
GLboolean glUnmapBuffer(GLenum t) { assert(t==GL_COPY_WRITE_BUFFER); ++unmaps; return GL_TRUE; }
GLsync glFenceSync(GLenum t, GLbitfield flags) { assert(t==GL_SYNC_GPU_COMMANDS_COMPLETE && !flags); ++fences; return fail_fence ? NULL : (GLsync)(uintptr_t)fences; }
GLenum glClientWaitSync(GLsync f, GLbitfield flags, uint64_t timeout) { assert(f && flags==GL_SYNC_FLUSH_COMMANDS_BIT && timeout); ++waits; return fail_wait ? 0x911d : GL_CONDITION_SATISFIED; }
void glDeleteSync(GLsync f) { assert(f); ++deleted; }
void glBindBufferBase(GLenum t, GLuint i, GLuint b) { assert(t==GL_UNIFORM_BUFFER); (void)i; (void)b; ++base_calls; }
void glBindBufferRange(GLenum t, GLuint i, GLuint b, GLintptr o, GLsizeiptr n) {
    assert(t==GL_UNIFORM_BUFFER && i==1); ++range_calls; range_buffer=b; range_offset=o; range_size=n;
}
void glActiveTexture(GLenum t) { (void)t; }
void glBindTexture(GLenum t, GLuint n) { (void)t; (void)n; }
void glBindFramebuffer(GLenum t, GLuint n) { (void)t; (void)n; }
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint n, GLint l) { (void)t;(void)a;(void)tt;(void)n;(void)l; }
void glPixelStorei(GLenum t, GLint v) { (void)t;(void)v; }
void glReadBuffer(GLenum t) { (void)t; }
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) { (void)x;(void)y;(void)w;(void)h;(void)f;(void)t;(void)p; }
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *p) { (void)t;(void)l;(void)x;(void)y;(void)w;(void)h;(void)f;(void)ty;(void)p; }
int main(void) {
    D3D11_BUFFER_DESC d={64,D3D11_USAGE_DYNAMIC,D3D11_BIND_CONSTANT_BUFFER,D3D11_CPU_ACCESS_WRITE,0,0};
    ID3D11Buffer *handle=NULL; D3D11_MAPPED_SUBRESOURCE m;
    assert(SUCCEEDED(d3dgl_Device_CreateBuffer(NULL,&d,NULL,&handle))); Buf *b=(Buf *)handle; GLuint original=b->buf;
    setenv("D3DGL_NO_CONSTANT_RING","1",1); assert(!d3d11gles_enable_constant_ring(handle)); unsetenv("D3DGL_NO_CONSTANT_RING");
    support=0; assert(!d3d11gles_enable_constant_ring(handle)); support=1;
    fail_storage=1; assert(!d3d11gles_enable_constant_ring(handle) && b->buf==original); fail_storage=0;
    fail_map=1; assert(!d3d11gles_enable_constant_ring(handle) && b->buf==original); fail_map=0;
    assert(d3d11gles_enable_constant_ring(handle)); assert(b->constant_stride==256 && maps==2);
    b->persistent_bytes=1024; /* Force rotation after four aligned snapshots. */
    GLuint g0=b->buf;
    assert(FAILED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_READ,0,&m)));
    for (int i=0;i<4;i++) {
        assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
        assert(m.pData==memory[g0]+i*256 && m.RowPitch==64); memset(m.pData,i+1,64);
        assert(FAILED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
        d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
        bind_uniform_range(1,b); assert(range_buffer==g0 && range_offset==i*256 && range_size==64);
        bind_uniform_range(1,b); assert(range_calls==(unsigned)i+1);
    }
    assert(maps==2 && unmaps==0);
    fail_fence=1; assert(FAILED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
    assert(memory[g0][0]==1 && b->buf==g0); fail_fence=0;
    assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
    GLuint g1=b->buf; assert(g1!=g0 && memory[g0][0]==1); memset(m.pData,9,64); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
    bind_uniform_range(1,b); assert(range_buffer==g1 && range_offset==0 && range_calls==5);
    bind_uniform_buffer(1,g1); assert(base_calls==1); bind_uniform_range(1,b); assert(range_calls==6);
    for (int i=0;i<3;i++) { assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m))); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0); }
    assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m))); GLuint g2=b->buf; assert(g2!=g0 && g2!=g1); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
    for (int i=0;i<3;i++) { assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m))); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0); }
    fail_wait=1; assert(FAILED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m))); assert(b->buf==g2 && memory[g0][0]==1);
    fail_wait=0; assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m))); assert(b->buf==g0 && waits==2); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
    d3dgl_Release(b); assert(unmaps==3 && deleted>=3);
    /* Generation allocation failure waits on the current generation before reuse. */
    handle=NULL; assert(SUCCEEDED(d3dgl_Device_CreateBuffer(NULL,&d,NULL,&handle)));
    b=(Buf *)handle; assert(d3d11gles_enable_constant_ring(handle));
    b->persistent_bytes=256;
    assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
    memset(m.pData, 0x55, 64); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
    GLuint only=b->buf; unsigned previous_waits=waits;
    fail_storage=1;
    assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
    assert(b->buf==only && waits==previous_waits+1); memset(m.pData,0xaa,64);
    d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0); fail_storage=0;
    assert(SUCCEEDED(d3dgl_Ctx_Map(NULL,(ID3D11Resource *)b,0,D3D11_MAP_WRITE_DISCARD,0,&m)));
    assert(b->buf!=only && memory[only][0]==0xaa); d3dgl_Ctx_Unmap(NULL,(ID3D11Resource *)b,0);
    d3dgl_Release(b);
    puts("constant ring: aligned snapshots, binding cache, fences, retry and cleanup OK");
}
