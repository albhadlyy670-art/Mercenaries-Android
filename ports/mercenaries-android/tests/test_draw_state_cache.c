/* Run the real GLES state application with stubbed GL entry points. */
#include <assert.h>
#include <stdio.h>
#define D3DGL_TEST_DEFER_HOT_ERRORS 1
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
static int calls, destroyed, draw_buffers_calls, framebuffer_calls, program_calls, vao_calls, uniform_calls, delete_calls, error_checks;
static int copy_bind_calls, fbo_gen_calls, fbo_delete_calls, attachment_calls;
static GLuint next_fbo = 10;
static GLenum last_attachment;
static GLenum pending_error;
static int viewport_calls, raster_calls, depth_calls, blend_calls;
GLenum glGetError(void) { ++error_checks; GLenum e = pending_error; pending_error = 0; return e; }
static void destroy_mock(Obj *o) { (void)o; ++destroyed; }
#define V1(n,t) void n(t a) { (void)a; ++calls; }
#define V2(n,t,u) void n(t a,u b) { (void)a; (void)b; ++calls; }
#define V4(n,t,u,v,w) void n(t a,u b,v c,w d) { (void)a; (void)b; (void)c; (void)d; ++calls; }
V1(glEnable,GLenum) V1(glDisable,GLenum) V1(glDepthFunc,GLenum)
V1(glCullFace,GLenum)
void glDepthMask(GLboolean a) { (void)a; ++calls; ++depth_calls; }
void glFrontFace(GLenum a) { (void)a; ++calls; ++raster_calls; }
V2(glDepthRangef,GLfloat,GLfloat) V2(glPolygonOffset,GLfloat,GLfloat)
V2(glStencilMaskSeparate,GLenum,GLuint) V2(glBlendEquationSeparate,GLenum,GLenum)
V4(glScissor,GLint,GLint,GLsizei,GLsizei)
void glViewport(GLint a, GLint b, GLsizei c, GLsizei d) { (void)a; (void)b; (void)c; (void)d; ++calls; ++viewport_calls; }
void glColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) { (void)a; (void)b; (void)c; (void)d; ++calls; ++blend_calls; }
V4(glStencilFuncSeparate,GLenum,GLenum,GLint,GLuint)
V4(glStencilOpSeparate,GLenum,GLenum,GLenum,GLenum)
V4(glBlendFuncSeparate,GLenum,GLenum,GLenum,GLenum)
V4(glBlendColor,GLfloat,GLfloat,GLfloat,GLfloat)
void glUseProgram(GLuint p) { (void)p; ++program_calls; }
void glBindVertexArray(GLuint vao) { (void)vao; ++vao_calls; }
GLboolean glUnmapBuffer(GLenum target) { (void)target; return GL_TRUE; }
void glBindBuffer(GLenum target, GLuint buf) { assert(target == GL_COPY_WRITE_BUFFER); (void)buf; ++copy_bind_calls; }
void glBindBufferBase(GLenum target, GLuint slot, GLuint buf) { assert(target == GL_UNIFORM_BUFFER); (void)slot; (void)buf; ++uniform_calls; }
void glDeleteBuffers(GLsizei n, const GLuint *buf) { (void)n; (void)buf; ++delete_calls; }
void glGenFramebuffers(GLsizei n, GLuint *out) { assert(n == 1); *out = next_fbo++; ++fbo_gen_calls; }
void glDeleteFramebuffers(GLsizei n, const GLuint *out) { assert(n == 1 && *out); ++fbo_delete_calls; }
void glBindFramebuffer(GLenum target, GLuint fbo) { (void)target; (void)fbo; ++framebuffer_calls; }
void glFramebufferTexture2D(GLenum target, GLenum point, GLenum tex_target, GLuint tex, GLint level)
{ (void)target; (void)tex_target; (void)tex; (void)level; last_attachment = point; ++attachment_calls; }
void glDrawBuffers(GLsizei n, const GLenum *values) { assert(n > 0 && values); ++draw_buffers_calls; }
int main(void)
{
    Shader vs1 = {0}, vs2 = {0}, ps = {0}; Program p1 = {0}, p2 = {0}, p3 = {0};
    p3.bgra = 1;
    G.progs[0] = (ProgKey){ &vs1, &ps, &p1 };
    G.progs[1] = (ProgKey){ &vs2, &ps, &p2 };
    G.progs[2] = (ProgKey){ &vs1, &ps, &p3 }; G.nprogs = 3;
    assert(find_cached_program(&vs1, &ps, 0) == &p1 && G.last_program == 1);
    assert(find_cached_program(&vs1, &ps, 0) == &p1);
    assert(find_cached_program(&vs2, &ps, 0) == &p2 && G.last_program == 2);
    assert(find_cached_program(&vs1, &ps, 1) == &p3 && G.last_program == 3);
    assert(find_cached_program(&vs2, &ps, 1) == NULL);
    G.nprogs = G.last_program = 0;
    Ctx c; StateObj raster, depth, blend;
    memset(&c, 0, sizeof(c)); memset(&raster, 0, sizeof(raster));
    memset(&depth, 0, sizeof(depth)); memset(&blend, 0, sizeof(blend));
    apply_state(&c, 640, 480); assert(calls > 0); int first = calls;
    apply_state(&c, 640, 480); assert(calls == first);
    apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.nvp = 1; c.vp[0] = (D3D11_VIEWPORT){0,0,320,240,0,1};
    apply_state(&c, 320, 240); assert(calls == first); /* equivalent viewport */
    c.vp[0].Width = 300; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.rs = &raster; raster.u.rs.ScissorEnable = TRUE; c.nsc = 1;
    c.sc[0] = (D3D11_RECT){0,0,10,10}; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.sc[0].right = 9; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.dss = &depth; depth.u.ds.DepthEnable = TRUE; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.stencil_ref = 1; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.blend = &blend; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    c.blend_factor[0] = 0.5f; apply_state(&c, 320, 240); assert(calls > first); first = calls;
    invalidate_att(); apply_state(&c, 320, 240); assert(calls > first);
    /* A change to one state group must not reissue unrelated GL state.
     * Full invalidation must still restore every group. */
    int v = viewport_calls, r = raster_calls, d = depth_calls, b = blend_calls;
    c.blend_factor[0] = 0.75f; apply_state(&c, 320, 240);
    assert(viewport_calls == v && raster_calls == r && depth_calls == d && blend_calls == b + 1); b++;
    c.stencil_ref++; apply_state(&c, 320, 240);
    assert(viewport_calls == v && raster_calls == r && depth_calls == d + 1 && blend_calls == b); d++;
    c.sc[0].right++; apply_state(&c, 320, 240);
    assert(viewport_calls == v && raster_calls == r + 1 && depth_calls == d && blend_calls == b); r++;
    c.vp[0].Width++; apply_state(&c, 320, 240);
    assert(viewport_calls == v + 1 && raster_calls == r && depth_calls == d && blend_calls == b); v++;
    invalidate_att(); apply_state(&c, 320, 240);
    assert(viewport_calls == v + 1 && raster_calls == r + 1 && depth_calls == d + 1 && blend_calls == b + 1);
    /* Repeated render-target configurations reuse immutable FBOs; clear,
     * presentation and copies invalidate the binding but not the objects. */
    Tex tex; View view; int width, height;
    memset(&tex, 0, sizeof(tex)); memset(&view, 0, sizeof(view));
    tex.d.Width = 640; tex.d.Height = 480; tex.tex = 1; view.tex = &tex; view.o.ref = 1;
    ID3D11RenderTargetView *target = (ID3D11RenderTargetView *)&view;
    d3dgl_Ctx_OMSetRenderTargets((ID3D11DeviceContext *)&c, 1, &target, NULL); invalidate_att();
    bind_ctx_fbo(&c, &width, &height);
    assert(width == 640 && height == 480 && draw_buffers_calls == 1 && framebuffer_calls == 1 && attachment_calls == 1);
    d3dgl_Ctx_OMSetRenderTargets((ID3D11DeviceContext *)&c, 1, &target, NULL);
    bind_ctx_fbo(&c, &width, &height); assert(framebuffer_calls == 1 && attachment_calls == 1);
    d3dgl_Ctx_OMSetRenderTargets((ID3D11DeviceContext *)&c, 0, NULL, NULL);
    bind_ctx_fbo(&c, &width, &height); assert(draw_buffers_calls == 2 && framebuffer_calls == 2);
    invalidate_att(); bind_ctx_fbo(&c, &width, &height);
    assert(draw_buffers_calls == 2 && framebuffer_calls == 3 && attachment_calls == 1);
    d3dgl_Ctx_OMSetRenderTargets((ID3D11DeviceContext *)&c, 1, &target, NULL);
    bind_ctx_fbo(&c, &width, &height); assert(fbo_gen_calls == 2 && attachment_calls == 1);
    /* Different mip is a distinct attachment; reuse after switching back. */
    view.mip = 1; G.fbo_dirty = 1; bind_ctx_fbo(&c, &width, &height);
    assert(width == 320 && height == 240 && fbo_gen_calls == 3 && attachment_calls == 2);
    view.mip = 0; G.fbo_dirty = 1; bind_ctx_fbo(&c, &width, &height);
    assert(fbo_gen_calls == 3 && attachment_calls == 2);
    /* A destroyed texture must evict every mip before its GL name is reused. */
    invalidate_draw_fbos_for_texture(1); assert(fbo_delete_calls == 2);
    bind_ctx_fbo(&c, &width, &height); assert(fbo_gen_calls == 4 && attachment_calls == 3);
    Ctx another; memset(&another, 0, sizeof(another));
    bind_ctx_fbo(&another, &width, &height); assert(fbo_gen_calls == 4);
    bind_ctx_fbo(&c, &width, &height); assert(fbo_gen_calls == 4);
    /* Depth-only and packed depth/stencil use distinct attachment points. */
    Tex ztex = {0}; View zview = {0}; Ctx zctx = {0};
    ztex.tex = 2; ztex.d.Width = 640; ztex.d.Height = 480;
    ztex.fmt = find_fmt_raw(DXGI_FORMAT_D16_UNORM); zview.tex = &ztex; zctx.dsv = &zview;
    bind_ctx_fbo(&zctx, &width, &height); assert(last_attachment == GL_DEPTH_ATTACHMENT);
    ztex.fmt = find_fmt_raw(DXGI_FORMAT_D24_UNORM_S8_UINT); G.fbo_dirty = 1;
    bind_ctx_fbo(&zctx, &width, &height); assert(last_attachment == GL_DEPTH_STENCIL_ATTACHMENT);
    int depth_gen = fbo_gen_calls; ztex.fmt = find_fmt_raw(DXGI_FORMAT_D16_UNORM); G.fbo_dirty = 1;
    bind_ctx_fbo(&zctx, &width, &height); assert(fbo_gen_calls == depth_gen);
    /* Production defers hot-path errors to the next normal check; debug
     * mode still checks immediately. */
    G.dbg = 0; pending_error = 0x0500; error_checks = 0;
    gl_check_hot("draw"); assert(error_checks == 0 && pending_error == 0x0500);
    gl_check("present"); assert(error_checks == 2 && G.errors == 1);
    G.dbg = 1; gl_check_hot("debug draw"); assert(error_checks == 3);
    G.dbg = 0; G.errors = 0;
    /* Bindings are cached only while the renderer owns GL state. */
    G.vao = 5; invalidate_att();
    use_renderer_program(1); use_renderer_program(1); assert(program_calls == 1);
    use_renderer_program(2); assert(program_calls == 2);
    bind_renderer_vao(); bind_renderer_vao(); assert(vao_calls == 1);
    bind_uniform_buffer(1, 7); bind_uniform_buffer(1, 7); assert(uniform_calls == 1);
    bind_uniform_buffer(1, 8); bind_uniform_buffer(17, 7); assert(uniform_calls == 3);
    invalidate_att(); use_renderer_program(2); bind_renderer_vao(); bind_uniform_buffer(1, 8);
    assert(program_calls == 3 && vao_calls == 2 && uniform_calls == 4);
    bind_copy_write_buffer(7); bind_copy_write_buffer(7); assert(copy_bind_calls == 1);
    bind_copy_write_buffer(8); assert(copy_bind_calls == 2);
    invalidate_att(); bind_copy_write_buffer(8); assert(copy_bind_calls == 3);
    Buf *dying = new_obj(sizeof(Buf), K_BUFFER, buf_destroy); dying->buf = 8;
    d3dgl_Release(dying); assert(delete_calls == 1);
    bind_copy_write_buffer(8); assert(copy_bind_calls == 4); /* deleted GL name may be reused */
    bind_uniform_buffer(1, 8); assert(uniform_calls == 5); /* GL name reuse */
    /* Repeated binding of the same object must retain it exactly once while
     * still updating independent vertex-buffer offsets and strides. */
    Ctx bindings; memset(&bindings, 0, sizeof(bindings));
    Buf *buf = new_obj(sizeof(Buf), K_BUFFER, destroy_mock);
    ID3D11Buffer *handle = (ID3D11Buffer *)buf;
    UINT stride = 8, offset = 4;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(buf->o.ref == 2); d3dgl_Release(buf); assert(buf->o.ref == 1);
    stride = 16; offset = 12;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(buf->o.ref == 1 && bindings.vb[0].stride == 16 && bindings.vb[0].offset == 12);
    handle = NULL;
    d3dgl_Ctx_IASetVertexBuffers((ID3D11DeviceContext *)&bindings, 0, 1, &handle, &stride, &offset);
    assert(destroyed == 1 && bindings.vb[0].b == NULL);
    /* Cache stays bounded and evicted target configurations are rebuilt. */
    Ctx many = {0}; Tex many_tex = {0}; View many_view = {0};
    many_tex.d.Width = many_tex.d.Height = 32; many_view.tex = &many_tex; many.rtv[0] = &many_view;
    int deletes_before = fbo_delete_calls;
    for (unsigned i = 0; i < DRAW_FBO_CACHE_SIZE + 4; ++i) {
        many_tex.tex = 100 + i; G.fbo_dirty = 1; bind_ctx_fbo(&many, &width, &height);
    }
    assert(fbo_delete_calls >= deletes_before + 4);
    int generated = fbo_gen_calls; many_tex.tex = 100; G.fbo_dirty = 1;
    bind_ctx_fbo(&many, &width, &height); assert(fbo_gen_calls == generated + 1);
    puts("draw state: reuse, changes, invalidation and binding references OK"); return 0;
}
