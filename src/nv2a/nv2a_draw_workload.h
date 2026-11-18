#ifndef NV2A_DRAW_WORKLOAD_H
#define NV2A_DRAW_WORKLOAD_H
#include <stdint.h>

typedef struct Nv2aDrawWorkload {
    uint64_t array, indexed, inline_array, immediate;
    uint64_t small_strips, small_strips_array, small_strips_indexed;
    uint64_t small_strips_inline, small_strips_immediate, small_strips_textured;
    uint64_t disjoint_array_splits, strip_array_splits;
} Nv2aDrawWorkload;

typedef enum Nv2aDrawSource {
    NV2A_DRAW_ARRAY, NV2A_DRAW_INDEXED, NV2A_DRAW_INLINE, NV2A_DRAW_IMMEDIATE
} Nv2aDrawSource;

static inline void nv2a_workload_draw(Nv2aDrawWorkload *w, Nv2aDrawSource source,
                                      uint32_t mode, uint32_t vertices, int textured)
{
    switch (source) {
    case NV2A_DRAW_ARRAY: ++w->array; break;
    case NV2A_DRAW_INDEXED: ++w->indexed; break;
    case NV2A_DRAW_INLINE: ++w->inline_array; break;
    case NV2A_DRAW_IMMEDIATE: ++w->immediate; break;
    }
    if (mode != 6u || vertices > 16u) return;
    ++w->small_strips;
    if (textured) ++w->small_strips_textured;
    switch (source) {
    case NV2A_DRAW_ARRAY: ++w->small_strips_array; break;
    case NV2A_DRAW_INDEXED: ++w->small_strips_indexed; break;
    case NV2A_DRAW_INLINE: ++w->small_strips_inline; break;
    case NV2A_DRAW_IMMEDIATE: ++w->small_strips_immediate; break;
    }
}
#endif
