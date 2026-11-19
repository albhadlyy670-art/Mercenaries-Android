#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../../src/nv2a/nv2a_batch_probe.h"
static void le(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i)); }
int main(void)
{
    uint8_t storage[17]={0}, *p=storage+1; /* deliberately unaligned */
    const uint32_t sub=2, hdr=(1u<<18)|(sub<<13);
    le(p,hdr|0x17fc); le(p+4,6); le(p+8,hdr|0x1800);
    assert(nv2a_probe_next(p,0,sub)==NV2A_PROBE_BOUNDARY);
    assert(nv2a_probe_next(p,7,sub)==NV2A_PROBE_BOUNDARY);
    assert(nv2a_probe_next(p,8,sub)==NV2A_PROBE_STRIP_PARTIAL);
    assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_INDEXED);
    le(p+8,0x40000000u|hdr|0x1808); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_INDEXED);
    le(p+8,hdr|0x1810); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_ARRAY);
    le(p+8,hdr|0x1818); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_INLINE);
    le(p+8,hdr|0x1940); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_IMMEDIATE);
    le(p+8,hdr|0x1518); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_IMMEDIATE);
    le(p+8,hdr|0x1b00); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_OTHER);
    le(p+8,(sub<<13)|0x1810); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_STRIP_OTHER);
    le(p+8,hdr|0x1810); le(p+4,5); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_BEGIN_OTHER);
    le(p+4,6); assert(nv2a_probe_next(p,12,sub+1)==NV2A_PROBE_BARRIER);
    le(p,0x20000000); assert(nv2a_probe_next(p,12,sub)==NV2A_PROBE_BARRIER);
    assert(nv2a_probe_next(NULL,12,sub)==NV2A_PROBE_BOUNDARY);
    puts("adjacency probe: bounds, endian, subchannels and packet classification OK");
}
