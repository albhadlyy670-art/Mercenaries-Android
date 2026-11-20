#ifndef NV2A_IMMEDIATE_VERTICES_H
#define NV2A_IMMEDIATE_VERTICES_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Pre-transformed immediate output: bit-copy the contiguous position and UV
 * registers rather than converting eight individual float components. This
 * preserves NaN payloads and signed zero; the compiler can vectorize the UV
 * copy on AArch64. */
typedef struct {
    float x, y, z, rhw;
    uint32_t color;
    float u0, v0, u1, v1, u2, v2, u3, v3;
} ImmediateOutputVertex;
_Static_assert(offsetof(ImmediateOutputVertex, y) == offsetof(ImmediateOutputVertex, x) + 4, "position layout");
_Static_assert(offsetof(ImmediateOutputVertex, v3) == offsetof(ImmediateOutputVertex, u0) + 28, "UV layout");
static inline void nv2a_immediate_convert(ImmediateOutputVertex *dst, const uint32_t *src)
{
    memcpy((uint8_t *)dst + offsetof(ImmediateOutputVertex, x), src, 2u * sizeof(uint32_t));
    dst->z = 0.0f;
    dst->rhw = 1.0f;
    dst->color = src[10];
    memcpy((uint8_t *)dst + offsetof(ImmediateOutputVertex, u0), src + 2, 8u * sizeof(uint32_t));
}

/* Immediate vertices contain all 16 persistent float4 input registers.
 * The source stays unchanged through synchronous draw submission. Strips and
 * lists need no transformation, while quads and line loops require scratch. */
typedef float Nv2aImmediateVertex[16][4];
static inline Nv2aImmediateVertex *nv2a_immediate_vertex_source(
    Nv2aImmediateVertex *src, Nv2aImmediateVertex *scratch, uint32_t count,
    int quads, int line_loop)
{
    if (!quads && !line_loop) return src;
    if (quads) {
        static const unsigned order[6] = {0, 1, 2, 0, 2, 3};
        for (uint32_t q = 0; q < count / 4u; ++q)
            for (unsigned i = 0; i < 6u; ++i)
                memcpy(scratch[q * 6u + i], src[q * 4u + order[i]], sizeof(*scratch));
    } else {
        memcpy(scratch, src, (size_t)count * sizeof(*scratch));
        if (count) memcpy(scratch[count], src[0], sizeof(*scratch));
    }
    return scratch;
}
#endif
