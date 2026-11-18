#ifndef NV2A_DRAW_BATCH_H
#define NV2A_DRAW_BATCH_H
/*
 * Bounded CPU-side primitive merger. This deliberately does not decide when a
 * guest draw is safe to defer: the caller must prove exact pipeline/resource
 * state equality, stable vertex snapshots, no framebuffer feedback or other
 * observation, and no shader dependence on vertex IDs. It must flush BEFORE
 * any state mutation, readback, buffer reuse, context change or presentation.
 * An experimental Android inline triangle-list path uses this helper.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct Nv2aDrawBatch {
    unsigned char *data;
    size_t capacity_bytes, stride, count;
    unsigned mode; /* 5 triangle list, 6 triangle strip */
    unsigned draws;
} Nv2aDrawBatch;

typedef enum Nv2aBatchResult {
    NV2A_BATCH_REJECT = 0, /* unsupported/invalid, issue original draw */
    NV2A_BATCH_APPEND = 1,
    NV2A_BATCH_FLUSH = 2   /* flush current batch, then retry this draw */
} Nv2aBatchResult;

static inline void nv2a_batch_init(Nv2aDrawBatch *b, void *storage, size_t bytes)
{
    memset(b, 0, sizeof(*b)); b->data = (unsigned char *)storage; b->capacity_bytes = bytes;
}
static inline void nv2a_batch_reset(Nv2aDrawBatch *b)
{ b->stride = b->count = b->mode = b->draws = 0; }

/* All inputs must be separate from the batch storage. Vertex bytes are copied
 * now, not borrowed from mutable guest memory. A successful append keeps the
 * caller's draw and primitive order; triangle-strip seams are degenerate and
 * preserve the parity of each new strip. */
static inline Nv2aBatchResult nv2a_batch_append(Nv2aDrawBatch *b, unsigned mode,
                                               const void *vertices, size_t count,
                                               size_t stride)
{
    if (!b || !b->data || !vertices || !stride || count < 3 ||
        (mode != 5u && mode != 6u) || (mode == 5u && count % 3u))
        return NV2A_BATCH_REJECT;
    if (b->draws && (b->mode != mode || b->stride != stride)) return NV2A_BATCH_FLUSH;
    size_t seam = b->draws && mode == 6u ? 2u + (b->count & 1u) : 0u;
    if (count > SIZE_MAX - seam || b->count > SIZE_MAX - count - seam ||
        b->count + count + seam > b->capacity_bytes / stride ||
        b->draws == UINT32_MAX)
        return b->draws ? NV2A_BATCH_FLUSH : NV2A_BATCH_REJECT;
    size_t at = b->count;
    if (seam) {
        const unsigned char *last = b->data + (at - 1u) * stride;
        memcpy(b->data + at++ * stride, last, stride);
        if (seam == 3u) memcpy(b->data + at++ * stride, last, stride);
        memcpy(b->data + at++ * stride, vertices, stride);
    }
    memcpy(b->data + at * stride, vertices, count * stride);
    b->mode = mode; b->stride = stride; b->count = at + count; ++b->draws;
    return NV2A_BATCH_APPEND;
}
#endif
