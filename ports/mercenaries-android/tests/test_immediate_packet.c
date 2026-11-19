#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../../src/nv2a/nv2a_immediate_packet.h"
static void le(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i)); }
int main(void)
{
    uint8_t storage[50]={0}, *p=storage+1;
    uint32_t sub=2, be=(1u<<18)|(sub<<13)|0x17fc, hdr=(sub<<13);
    Nv2aImmediatePacket result={123,456};
    le(p,be); le(p+4,6); le(p+8,(4u<<18)|hdr|0x1a00);
    for (unsigned i=0;i<4;i++) le(p+12+i*4,i);
    le(p+28,(1u<<18)|hdr|0x1940); le(p+32,0xff); le(p+36,be); le(p+40,0);
    for (unsigned n=0;n<44;n++) { result.words=123; result.data_methods=456; assert(!nv2a_parse_immediate_strip(p,n,sub,&result)); assert(result.words==123 && result.data_methods==456); }
    assert(nv2a_parse_immediate_strip(p,44,sub,&result)); assert(result.words==11 && result.data_methods==5);
    le(p+28,(1u<<18)|hdr|0x1b00); assert(!nv2a_parse_immediate_strip(p,44,sub,&result));
    le(p+28,(1u<<18)|hdr|0x1940); le(p+8,(2u<<18)|hdr|0x1afc); assert(!nv2a_parse_immediate_strip(p,44,sub,&result));
    le(p+8,(4u<<18)|hdr|0x1a00); le(p+36,be ^ (1u<<13)); assert(!nv2a_parse_immediate_strip(p,44,sub,&result));
    le(p+36,be); le(p+40,1); assert(!nv2a_parse_immediate_strip(p,44,sub,&result));
    le(p+40,0); le(p+8,0x40000000u|(4u<<18)|hdr|0x1a00); assert(nv2a_parse_immediate_strip(p,44,sub,&result));
    le(p+8,0x20000000u); assert(!nv2a_parse_immediate_strip(p,44,sub,&result));
    puts("immediate strip packet: complete packet, bounds, state barriers OK");
}
