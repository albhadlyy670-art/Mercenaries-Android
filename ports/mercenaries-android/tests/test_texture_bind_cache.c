/* Exercise actual GLES texture/sampler binding and upload paths using mock GL. */
#include <assert.h>
#include <stdio.h>
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
static GLuint unit, tex2d[32], cube[32], samplers[32];
static int act_calls, bind_calls, sampler_calls, parameter_calls, uploads, deletes;
static int row_length, observed_row_length, pack_row_length, observed_pack_row_length;
static void *observed_read_pixels;
static const void *observed_pixels;
static unsigned char uploaded_pixels[64];
void glActiveTexture(GLenum v) { assert(v >= GL_TEXTURE0 && v < GL_TEXTURE0+32); unit=v-GL_TEXTURE0; ++act_calls; }
void glBindTexture(GLenum t, GLuint x) { if(t==GL_TEXTURE_2D) tex2d[unit]=x; else { assert(t==GL_TEXTURE_CUBE_MAP); cube[unit]=x; } ++bind_calls; }
void glBindSampler(GLuint u, GLuint x) { assert(u<32); samplers[u]=x; ++sampler_calls; }
void glTexParameteri(GLenum t, GLenum p, GLint v) { assert(t==GL_TEXTURE_2D && (p==GL_TEXTURE_MIN_FILTER || p==GL_TEXTURE_MAG_FILTER) && v==GL_NEAREST); assert(tex2d[unit]==11); ++parameter_calls; }
void glPixelStorei(GLenum p, GLint v) { if (p==GL_UNPACK_ROW_LENGTH) row_length=v; if (p==GL_PACK_ROW_LENGTH) pack_row_length=v; }
void glBindFramebuffer(GLenum t, GLuint f) { (void)t;(void)f; }
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint id, GLint m) { (void)t;(void)a;(void)tt;(void)id;(void)m; }
void glReadBuffer(GLenum t) { (void)t; }
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, void *p)
{ (void)x;(void)y;(void)f;(void)ty; assert(w==2 && h==2); observed_read_pixels=p; observed_pack_row_length=pack_row_length;
  for(int r=0;r<h;r++) memcpy((unsigned char *)p+r*(pack_row_length?pack_row_length*4:8),uploaded_pixels+r*8,8); }
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *p)
{ (void)l;(void)x;(void)y;(void)f;(void)ty; assert(t==GL_TEXTURE_2D && unit==0 && tex2d[0]==11); ++uploads;
  observed_pixels=p; observed_row_length=row_length;
  assert(w==2 && h==2);
  for(int r=0;r<h;r++) memcpy(uploaded_pixels + r*8,(const unsigned char *)p+r*(row_length?row_length*4:8),8);
}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glDeleteFramebuffers(GLsizei n, const GLuint *id) { (void)n; (void)id; }
void glDeleteTextures(GLsizei n, const GLuint *id) { (void)n; (void)id; ++deletes; }
void glDeleteSamplers(GLsizei n, const GLuint *id) { (void)n; (void)id; ++deletes; }
void glBindBufferRange(GLenum t, GLuint i, GLuint b, GLintptr o, GLsizeiptr n) { (void)t; (void)i; (void)b; (void)o; (void)n; }
void glBindBufferBase(GLenum t, GLuint i, GLuint b) { (void)t; (void)i; (void)b; }
int main(void)
{
    Ctx c; Shader vs, ps; Program program; Tex t; View view; Sampler sm;
    memset(&c,0,sizeof(c)); memset(&vs,0,sizeof(vs)); memset(&ps,0,sizeof(ps));
    memset(&program,0,sizeof(program)); memset(&t,0,sizeof(t)); memset(&view,0,sizeof(view)); memset(&sm,0,sizeof(sm));
    program.vs=&vs; program.ps=&ps; t.tex=11; t.target=GL_TEXTURE_2D; t.fmt=find_fmt_raw(DXGI_FORMAT_R8G8B8A8_UNORM);
    t.d.Width=2; t.d.Height=2; t.mips=1; view.tex=&t; c.srv_ps[0]=&view;
    ps.r.n_samplers=1; ps.r.samplers[0].dim=HLSL_DIM_2D; ps.r.samplers[0].tex_slot=0;
    ps.r.samplers[0].samp_slot=HLSL_NO_SLOT; ps.r.samplers[0].unit=0;
    invalidate_texture_bindings(); bind_resources(&c,&program);
    assert(bind_calls==1 && sampler_calls==1 && parameter_calls==2 && t.point_params);
    bind_resources(&c,&program);
    assert(bind_calls==1 && sampler_calls==1 && parameter_calls==2);
    /* Selecting another unit must not make a later upload use that unit. */
    bind_texture_unit(1, GL_TEXTURE_2D, 22); assert(unit==1);
    unsigned char data[16]={0}; tex_upload(&t,0,0,0,2,2,data,8);
    assert(uploads==1 && unit==0 && tex2d[0]==11 && observed_pixels==data);
    unsigned char padded[24]={1,2,3,4,5,6,7,8,99,99,99,99,9,10,11,12,13,14,15,16};
    tex_upload(&t,0,0,0,2,2,padded,12);
    assert(observed_pixels==padded && observed_row_length==3 && row_length==0);
    assert(!memcmp(uploaded_pixels,padded,8) && !memcmp(uploaded_pixels+8,padded+12,8));
    /* Unrepresentable byte pitch uses the original tightly packed fallback. */
    unsigned char odd[17]={1,2,3,4,5,6,7,8,99,9,10,11,12,13,14,15,16};
    tex_upload(&t,0,0,0,2,2,odd,9);
    assert(observed_pixels!=odd && observed_row_length==0);
    assert(!memcmp(uploaded_pixels,odd,8) && !memcmp(uploaded_pixels+8,odd+9,8));
    unsigned char readback[24]={0};
    assert(tex_read(&t,0,0,0,2,2,readback,12));
    assert(observed_read_pixels==readback && observed_pack_row_length==3 && pack_row_length==0);
    assert(!memcmp(readback,uploaded_pixels,8) && !memcmp(readback+12,uploaded_pixels+8,8));
    unsigned char oddread[17]={0};
    assert(tex_read(&t,0,0,0,2,2,oddread,9));
    assert(observed_read_pixels!=oddread && observed_pack_row_length==0);
    assert(!memcmp(oddread,uploaded_pixels,8) && !memcmp(oddread+9,uploaded_pixels+8,8));
    ps.r.samplers[0].samp_slot=0; sm.gl=33; sm.gl_point=34; c.smp_ps[0]=&sm;
    bind_resources(&c,&program); assert(samplers[0]==33 && sampler_calls==2);
    bind_resources(&c,&program); assert(sampler_calls==2);
    int before=bind_calls; invalidate_att(); bind_resources(&c,&program);
    assert(bind_calls==before+1 && sampler_calls==3 && parameter_calls==2);
    tex_destroy((Obj *)&t); assert(deletes==1);
    before=bind_calls; bind_resources(&c,&program); assert(bind_calls==before+1); /* deleted GL name may be reused */
    sampler_destroy((Obj *)&sm); before=sampler_calls; bind_resources(&c,&program); assert(sampler_calls==before+1);
    puts("texture bindings: reuse, independent units, upload and invalidation OK"); return 0;
}
