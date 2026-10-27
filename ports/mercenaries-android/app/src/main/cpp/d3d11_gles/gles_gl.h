/* gles_gl.h - OpenGL ES 3.0 entry points used by d3d11_gles.c.
 * Android: the NDK header. Host builds (tests against Mesa): minimal declarations. */
#ifndef GLES_GL_H
#define GLES_GL_H
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#else
#include <stdint.h>
#include <stddef.h>
typedef unsigned int GLenum, GLuint, GLbitfield; typedef int GLint, GLsizei; typedef unsigned char GLboolean, GLubyte;
typedef float GLfloat, GLclampf; typedef char GLchar; typedef ptrdiff_t GLsizeiptr, GLintptr; typedef signed char GLbyte; typedef short GLshort; typedef unsigned short GLushort;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NONE 0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NO_ERROR 0
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_RASTERIZER_DISCARD 0x8C89
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
#define GL_INVERT 0x150A
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_RG 0x8227
#define GL_RED_INTEGER 0x8D94
#define GL_RG_INTEGER 0x8228
#define GL_RGBA_INTEGER 0x8D99
#define GL_DEPTH_STENCIL 0x84F9
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RGBA8 0x8058
#define GL_RGB565 0x8D62
#define GL_RGB10_A2 0x8059
#define GL_R16F 0x822D
#define GL_RG16F 0x822F
#define GL_RGBA16F 0x881A
#define GL_R32F 0x822E
#define GL_RG32F 0x8230
#define GL_RGBA32F 0x8814
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_RG16I 0x8239
#define GL_RGBA16I 0x8D88
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_STENCIL 0x1802
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_INVALID_INDEX 0xFFFFFFFFu
#define GL_PRIMITIVE_RESTART_FIXED_INDEX 0x8D69
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_UNIFORM_BUFFER_BINDINGS 0x8A2F
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_VENDOR 0x1F00
#define GL_EXTENSIONS 0x1F03
#ifdef __cplusplus
extern "C" {
#endif
GLenum glGetError(void); const GLubyte *glGetString(GLenum); void glGetIntegerv(GLenum, GLint *);
void glEnable(GLenum); void glDisable(GLenum); void glViewport(GLint, GLint, GLsizei, GLsizei); void glScissor(GLint, GLint, GLsizei, GLsizei);
void glDepthRangef(GLfloat, GLfloat); void glDepthFunc(GLenum); void glDepthMask(GLboolean); void glColorMask(GLboolean, GLboolean, GLboolean, GLboolean);
void glStencilFuncSeparate(GLenum, GLenum, GLint, GLuint); void glStencilOpSeparate(GLenum, GLenum, GLenum, GLenum); void glStencilMaskSeparate(GLenum, GLuint);
void glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum); void glBlendEquationSeparate(GLenum, GLenum); void glBlendColor(GLfloat, GLfloat, GLfloat, GLfloat);
void glCullFace(GLenum); void glFrontFace(GLenum); void glPolygonOffset(GLfloat, GLfloat); void glPixelStorei(GLenum, GLint);
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat); void glClear(GLbitfield); void glFlush(void);
void glClearBufferfv(GLenum, GLint, const GLfloat *); void glClearBufferiv(GLenum, GLint, const GLint *); void glClearBufferuiv(GLenum, GLint, const GLuint *); void glClearBufferfi(GLenum, GLint, GLfloat, GLint);
void glGenBuffers(GLsizei, GLuint *); void glDeleteBuffers(GLsizei, const GLuint *); void glBindBuffer(GLenum, GLuint); void glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
void glBufferSubData(GLenum, GLintptr, GLsizeiptr, const void *); void *glMapBufferRange(GLenum, GLintptr, GLsizeiptr, GLbitfield); GLboolean glUnmapBuffer(GLenum);
void glBindBufferBase(GLenum, GLuint, GLuint); void glBindBufferRange(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr); void glCopyBufferSubData(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
void glGenTextures(GLsizei, GLuint *); void glDeleteTextures(GLsizei, const GLuint *); void glBindTexture(GLenum, GLuint); void glActiveTexture(GLenum);
void glTexStorage2D(GLenum, GLsizei, GLenum, GLsizei, GLsizei); void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
void glTexParameteri(GLenum, GLenum, GLint); void glGenSamplers(GLsizei, GLuint *); void glDeleteSamplers(GLsizei, const GLuint *); void glBindSampler(GLuint, GLuint);
void glSamplerParameteri(GLuint, GLenum, GLint); void glSamplerParameterf(GLuint, GLenum, GLfloat);
void glGenFramebuffers(GLsizei, GLuint *); void glDeleteFramebuffers(GLsizei, const GLuint *); void glBindFramebuffer(GLenum, GLuint);
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint); GLenum glCheckFramebufferStatus(GLenum); void glDrawBuffers(GLsizei, const GLenum *); void glReadBuffer(GLenum);
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
void glBlitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
GLuint glCreateShader(GLenum); void glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *); void glCompileShader(GLuint); void glDeleteShader(GLuint);
void glGetShaderiv(GLuint, GLenum, GLint *); void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *, GLchar *);
GLuint glCreateProgram(void); void glAttachShader(GLuint, GLuint); void glLinkProgram(GLuint); void glUseProgram(GLuint); void glDeleteProgram(GLuint);
void glGetProgramiv(GLuint, GLenum, GLint *); void glGetProgramInfoLog(GLuint, GLsizei, GLsizei *, GLchar *);
GLint glGetUniformLocation(GLuint, const GLchar *); GLuint glGetUniformBlockIndex(GLuint, const GLchar *); void glUniformBlockBinding(GLuint, GLuint, GLuint); void glUniform1i(GLint, GLint);
void glGenVertexArrays(GLsizei, GLuint *); void glBindVertexArray(GLuint); void glEnableVertexAttribArray(GLuint); void glDisableVertexAttribArray(GLuint);
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *); void glVertexAttribIPointer(GLuint, GLint, GLenum, GLsizei, const void *);
void glVertexAttrib4f(GLuint, GLfloat, GLfloat, GLfloat, GLfloat); void glVertexAttribDivisor(GLuint, GLuint);
void glDrawArrays(GLenum, GLint, GLsizei); void glDrawElements(GLenum, GLsizei, GLenum, const void *);
#ifdef __cplusplus
}
#endif
#endif
#endif
