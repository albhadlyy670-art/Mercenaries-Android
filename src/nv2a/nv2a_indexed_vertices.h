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
static void nv2a_free_indexed_vertices(uint8_t *data, const uint8_t *scratch)
{
    if (data != scratch) free(data);
}
#endif
