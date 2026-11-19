#ifndef NV2A_INDEXED_VERTICES_H
#define NV2A_INDEXED_VERTICES_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* The caller validates the highest index against guest VRAM first. Prefer a
 * small bounded caller-owned scratch chunk, but retain heap fallback for large
 * guest draws; no guest-controlled variable-length stack allocations. */
static uint8_t *nv2a_expand_indexed_vertices(const uint8_t *source,
    const uint32_t *indices, uint32_t count, uint32_t stride,
    uint8_t *scratch, size_t scratch_size)
{
    if (!source || !indices || !count || !stride ||
        (size_t)count > SIZE_MAX / stride) return NULL;
    size_t bytes = (size_t)count * stride;
    uint8_t *out = bytes <= scratch_size && scratch ? scratch : (uint8_t *)malloc(bytes);
    if (!out) return NULL;
    for (uint32_t i = 0; i < count; ++i)
        memcpy(out + (size_t)i * stride, source + (size_t)indices[i] * stride, stride);
    return out;
}
/* Common small programmable draws need both a sequential snapshot for
 * renderer observers and compact vertices for the GPU. Build both while
 * reading the guest once; repeated indices are compared against that exact
 * per-occurrence snapshot so a concurrent guest write cannot change a draw. */
static inline uint8_t *nv2a_expand_indexed_fused(const uint8_t *source,
    const uint32_t *indices, uint32_t count, uint32_t used, uint32_t stride,
    uint8_t *expanded_scratch, size_t expanded_scratch_size,
    uint8_t *compact, size_t compact_size, uint32_t *rebased,
    size_t rebased_capacity, uint32_t *compact_count)
{
    if (compact_count) *compact_count = 0;
    if (!source || !indices || !count || !stride || !compact_count ||
        (size_t)count > SIZE_MAX / stride) return NULL;
    size_t expanded_size = (size_t)count * stride;
    uint8_t *expanded = expanded_scratch && expanded_size <= expanded_scratch_size ?
        expanded_scratch : (uint8_t *)malloc(expanded_size);
    if (!expanded) return NULL;
    uint32_t min = UINT32_MAX, max = 0;
    int can_compact = compact && rebased && used >= 12u && used <= count &&
                      used <= rebased_capacity && compact_size <= 4096u;
    if (can_compact) {
        for (uint32_t i = 0; i < used; ++i) {
            if (indices[i] < min) min = indices[i];
            if (indices[i] > max) max = indices[i];
        }
        uint64_t span = (uint64_t)max - min + 1u;
        size_t original = (size_t)used * stride;
        if (span > UINT32_MAX || span > 4096u || span > SIZE_MAX / stride ||
            span * stride > compact_size ||
            span * stride + (uint64_t)used * sizeof(uint32_t) > original - original / 4u)
            can_compact = 0;
        else {
            *compact_count = (uint32_t)span;
            memset(compact, 0, (size_t)span * stride);
        }
    }
    unsigned char seen[512] = {0};
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t *dst = expanded + (size_t)i * stride;
        memcpy(dst, source + (size_t)indices[i] * stride, stride);
        if (can_compact && i < used) {
            uint32_t index = indices[i] - min;
            uint8_t bit = (uint8_t)(1u << (index & 7u));
            uint8_t *slot = compact + (size_t)index * stride;
            rebased[i] = index;
            if (seen[index >> 3] & bit) {
                if (memcmp(slot, dst, stride) != 0) can_compact = 0;
            } else { memcpy(slot, dst, stride); seen[index >> 3] |= bit; }
        }
    }
    if (!can_compact) *compact_count = 0;
    return expanded;
}

static void nv2a_free_indexed_vertices(uint8_t *data, const uint8_t *scratch)
{
    if (data != scratch) free(data);
}
/* Reconstruct a compact indexed submission from the already-snapshotted
 * expanded vertices. Repeated guest indices are checked for identical bytes.
 * This preserves the original primitive sequence; it never merges draws. */
typedef struct Nv2aCompactIndexed {
    uint8_t *vertices;
    uint32_t *indices;
    uint32_t vertex_count;
} Nv2aCompactIndexed;
static inline int nv2a_compact_indexed_vertices(const uint8_t *expanded,
    const uint32_t *indices, uint32_t count, uint32_t stride,
    uint8_t *vertex_scratch, size_t vertex_scratch_size,
    uint32_t *index_scratch, size_t index_scratch_count, Nv2aCompactIndexed *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!expanded || !indices || count < 12u || !stride ||
        (size_t)count > SIZE_MAX / stride) return 0;
#if SIZE_MAX <= UINT32_MAX
    if (count > SIZE_MAX / sizeof(uint32_t)) return 0;
#endif
    uint32_t min = indices[0], max = min;
    for (uint32_t i = 1; i < count; ++i) {
        if (indices[i] < min) min = indices[i];
        if (indices[i] > max) max = indices[i];
    }
    uint64_t span64 = (uint64_t)max - min + 1u;
    if (span64 > UINT32_MAX || span64 > SIZE_MAX / stride) return 0;
    size_t vb = (size_t)span64 * stride, ib = (size_t)count * sizeof(uint32_t);
    size_t original = (size_t)count * stride;
    /* Only use the ring-sized route when it reduces upload bytes by 25%.
     * This also excludes sparse, invalid and restart-like index ranges. */
    if (vb > 4u * 1024u * 1024u || ib > 4u * 1024u * 1024u ||
        vb > SIZE_MAX - ib || vb + ib > original - original / 4u) return 0;
    uint8_t *v = vertex_scratch && vb <= vertex_scratch_size ? vertex_scratch : (uint8_t *)malloc(vb);
    if (!v) return 0;
    uint32_t *ix = index_scratch && count <= index_scratch_count ? index_scratch : (uint32_t *)malloc(ib);
    if (!ix) { if (v != vertex_scratch) free(v); return 0; }
    memset(v, 0, vb);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t rebased = indices[i] - min;
        ix[i] = rebased;
        memcpy(v + (size_t)rebased * stride, expanded + (size_t)i * stride, stride);
    }
    /* Guest writes can race expansion: the same index need not have yielded
     * identical bytes at every occurrence. Preserve that snapshot exactly. */
    for (uint32_t i = 0; i < count; ++i) {
        if (memcmp(v + (size_t)ix[i] * stride, expanded + (size_t)i * stride, stride) != 0) {
            if (v != vertex_scratch) free(v);
            if (ix != index_scratch) free(ix);
            return 0;
        }
    }
    out->vertices = v; out->indices = ix; out->vertex_count = (uint32_t)span64;
    return 1;
}
static inline void nv2a_free_compact_indexed(Nv2aCompactIndexed *p, const uint8_t *vertex_scratch,
                                      const uint32_t *index_scratch)
{
    if (p->vertices != vertex_scratch) free(p->vertices);
    if (p->indices != index_scratch) free(p->indices);
    memset(p, 0, sizeof(*p));
}
#endif
