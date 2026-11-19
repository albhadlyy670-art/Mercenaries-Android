#ifndef D3D8_INDEXED_RING_H
#define D3D8_INDEXED_RING_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Reserve without changing the cursor: commit only after a successful upload. */
typedef struct D3D8RingReservation { uint32_t offset, next; int discard; } D3D8RingReservation;
static int d3d8_ring_reserve(uint32_t capacity, uint32_t cursor, uint32_t size,
                             D3D8RingReservation *out)
{
    if (!out || !capacity || !size || size > capacity || cursor > capacity) return 0;
    uint32_t offset = cursor, discard = 0;
    if (size > capacity - cursor) { offset = 0; discard = 1; }
    uint64_t next = ((uint64_t)offset + size + 15u) & ~(uint64_t)15u;
    /* A non-aligned allocation cannot be followed by another allocation when
       alignment extends past the end; the next call must discard and wrap. */
    out->offset = offset; out->next = next > capacity ? capacity : (uint32_t)next;
    out->discard = (int)discard; return 1;
}
/* GLES fixed-index restart treats all-ones as a cut. Do not route such a
 * draw through this path: preserve the old path for ambiguous input. */
static int d3d8_ring_indices_safe(const void *data, uint32_t count,
                                   unsigned width, uint32_t vertices)
{
    if (!data || !count || !vertices || (width != 2 && width != 4)) return 0;
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t v = 0;
        if (width == 2) { uint16_t x; memcpy(&x, p + (size_t)i * width, 2); v = x; }
        else memcpy(&v, p + (size_t)i * width, 4);
        if (v >= vertices || v == (width == 2 ? UINT16_MAX : UINT32_MAX)) return 0;
    }
    return 1;
}
#endif
