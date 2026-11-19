#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../../src/d3d/d3d8_indexed_ring.h"
int main(void)
{
    D3D8RingReservation r;
    assert(d3d8_ring_reserve(64, 0, 7, &r) && r.offset == 0 && r.next == 16 && !r.discard);
    assert(d3d8_ring_reserve(64, r.next, 48, &r) && r.offset == 16 && r.next == 64 && !r.discard);
    assert(d3d8_ring_reserve(64, r.next, 1, &r) && r.offset == 0 && r.next == 16 && r.discard);
    assert(d3d8_ring_reserve(63, 48, 15, &r) && r.next == 63 && !r.discard);
    assert(d3d8_ring_reserve(63, r.next, 1, &r) && r.offset == 0 && r.discard);
    assert(!d3d8_ring_reserve(64, 0, 0, &r));
    assert(!d3d8_ring_reserve(64, 0, 65, &r));
    assert(!d3d8_ring_reserve(64, 65, 1, &r));
    uint16_t a[] = {0, 2, 1, 2};
    uint32_t b[] = {4, 1, 3, 1};
    assert(d3d8_ring_indices_safe(a, 4, 2, 3));
    assert(!d3d8_ring_indices_safe(a, 4, 2, 2));
    assert(d3d8_ring_indices_safe(b, 4, 4, 5));
    assert(!d3d8_ring_indices_safe(b, 4, 4, 4));
    a[1] = UINT16_MAX; assert(!d3d8_ring_indices_safe(a, 4, 2, 65536));
    b[1] = UINT32_MAX; assert(!d3d8_ring_indices_safe(b, 4, 4, UINT32_MAX));
    unsigned char unaligned[9]; uint16_t c[] = {2, 0, 1, 2}; memcpy(unaligned + 1, c, sizeof(c));
    assert(d3d8_ring_indices_safe(unaligned + 1, 4, 2, 3));
    assert(!d3d8_ring_indices_safe(NULL, 3, 2, 3));
    assert(!d3d8_ring_indices_safe(c, 3, 1, 3));
    puts("indexed ring: alignment, wrap, 16/32-bit bounds and restart safety OK");
}
