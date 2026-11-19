#ifndef NV2A_IMMEDIATE_VERTICES_H
#define NV2A_IMMEDIATE_VERTICES_H
#include <stdint.h>
#include <string.h>

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
