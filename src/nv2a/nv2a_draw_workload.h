#ifndef NV2A_DRAW_WORKLOAD_H
#define NV2A_DRAW_WORKLOAD_H
#include <stdint.h>

typedef struct Nv2aDrawWorkload {
    uint64_t array, indexed, inline_array, immediate;
    uint64_t array_vertices, indexed_vertices, inline_vertices, immediate_vertices;
    uint64_t small_strips, small_strips_array, small_strips_indexed;
    uint64_t small_strips_inline, small_strips_immediate, small_strips_textured;
    uint64_t disjoint_array_splits, strip_array_splits;
    uint64_t fog_enabled, fog_disabled, fog_programmable;
    uint32_t last_fog_mode, last_fog_gen_mode;
} Nv2aDrawWorkload;

/* Observes active draw state only; this cannot infer a world fog cutoff. */
static inline void nv2a_workload_fog(Nv2aDrawWorkload *w, int enabled,
                                     int programmable, uint32_t mode, uint32_t gen)
{
    if (enabled) {
        ++w->fog_enabled;
        if (programmable) ++w->fog_programmable;
        w->last_fog_mode = mode; w->last_fog_gen_mode = gen;
    } else ++w->fog_disabled;
}

typedef enum Nv2aDrawSource {
    NV2A_DRAW_ARRAY, NV2A_DRAW_INDEXED, NV2A_DRAW_INLINE, NV2A_DRAW_IMMEDIATE
} Nv2aDrawSource;

static inline void nv2a_workload_draw(Nv2aDrawWorkload *w, Nv2aDrawSource source,
                                      uint32_t mode, uint32_t vertices, int textured)
{
    switch (source) {
    case NV2A_DRAW_ARRAY: ++w->array; w->array_vertices += vertices; break;
    case NV2A_DRAW_INDEXED: ++w->indexed; w->indexed_vertices += vertices; break;
    case NV2A_DRAW_INLINE: ++w->inline_array; w->inline_vertices += vertices; break;
    case NV2A_DRAW_IMMEDIATE: ++w->immediate; w->immediate_vertices += vertices; break;
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
