/*
 * hlsl2glsl.h - translator from the HLSL dialect emitted by the renderer
 * (d3d8_vsh.c, d3d8_combiners.c, d3d8_shaders.c, nv2a_pgraph_d3d11.c) to
 * GLSL ES 3.00, plus the reflection data the D3D11-on-GLES layer needs.
 *
 * Conventions baked into the output (see d3d11_gles.c):
 *   - Clip space is remapped from D3D (z in [0,w], y up in NDC with a top-left
 *     window origin) to GL by  gl_Position = (x, -y, 2z - w, w).  Every render
 *     target is therefore stored top-row-first, exactly like D3D memory.
 *   - Varyings are matched between stages by name: v_<SEMANTIC><index>.
 *   - Constant buffers become std140 uniform blocks "cb<slot>_<stage>".
 *   - Every (texture, sampler) pair that is sampled becomes one combined
 *     GLSL sampler uniform; HlslSamplerUse lists them.
 */
#ifndef HLSL2GLSL_H
#define HLSL2GLSL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { HLSL_STAGE_VS = 0, HLSL_STAGE_PS = 1 };
enum { HLSL_BASE_FLOAT = 0, HLSL_BASE_INT = 1, HLSL_BASE_UINT = 2 };
enum { HLSL_DIM_2D = 2, HLSL_DIM_3D = 3, HLSL_DIM_CUBE = 4, HLSL_DIM_1D = 1 };

#define HLSL_MAX_IO       32
#define HLSL_MAX_CBUF     16
#define HLSL_MAX_SAMPLERS 32
#define HLSL_NO_SLOT      0xFF

typedef struct {            /* vertex-shader input / varying */
    char     semantic[24];  /* upper-case, no trailing index */
    uint32_t index;
    uint8_t  base;          /* HLSL_BASE_* */
    uint8_t  comps;         /* 1..4 */
    uint8_t  location;      /* VS inputs only: layout(location=) */
    uint8_t  flat;
} HlslIo;

typedef struct {
    char    name[40];       /* GLSL uniform-block name */
    uint8_t slot;           /* register(bN) */
} HlslCBuf;

typedef struct {
    char    name[48];       /* GLSL combined-sampler uniform name */
    uint8_t tex_slot;       /* register(tN) */
    uint8_t samp_slot;      /* register(sN) or HLSL_NO_SLOT (Load / no sampler) */
    uint8_t dim;            /* HLSL_DIM_* */
    uint8_t base;           /* HLSL_BASE_* of the texel type */
    uint8_t unit;           /* texture unit it must be bound to */
} HlslSamplerUse;

typedef struct {
    int      stage;
    char    *glsl;          /* malloc'd, NUL terminated */
    char     error[512];
    int      n_inputs;      HlslIo inputs[HLSL_MAX_IO];    /* VS attribute inputs */
    int      n_varyings;    HlslIo varyings[HLSL_MAX_IO];  /* VS outputs / PS inputs */
    int      n_targets;     uint8_t target_base[8];        /* PS outputs */
    int      n_cbufs;       HlslCBuf cbufs[HLSL_MAX_CBUF];
    int      n_samplers;    HlslSamplerUse samplers[HLSL_MAX_SAMPLERS];
    int      writes_depth;
    int      uses_geometry;                                 /* unsupported */
} HlslResult;

/* Returns 0 on success (result->glsl set), non-zero on error (result->error set). */
int  hlsl_translate(const char *src, size_t len, const char *entry, int stage, HlslResult *out);
/* defines: n_defines (name, value) pairs, as passed to D3DCompile's D3D_SHADER_MACRO array. */
int  hlsl_translate_ex(const char *src, size_t len, const char *entry, int stage,
                       const char *const *defines, int n_defines, HlslResult *out);
void hlsl_result_free(HlslResult *r);

#ifdef __cplusplus
}
#endif
#endif
