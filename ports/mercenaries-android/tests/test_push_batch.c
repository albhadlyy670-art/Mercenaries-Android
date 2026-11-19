#include <assert.h>
#include <stdio.h>
#include "../../../src/nv2a/nv2a_push_batch.h"
int main(void)
{
    uint32_t n=0, sub=2, be=0x17fcu | (sub<<13), ia=0x1818u | (sub<<13);
    uint32_t p[]={ (1u<<18)|be, 5, 0x40000000u|(6u<<18)|ia, 1,2,3,4,5,6, (1u<<18)|be, 0 };
    assert(nv2a_batch_inline_packet(p,11,sub,&n)==11 && n==6);
    for(unsigned i=0;i<11;i++) assert(!nv2a_batch_inline_packet(p,i,sub,&n));
    assert(!nv2a_batch_inline_packet(p,11,sub+1,&n));
    p[1]=6; assert(!nv2a_batch_inline_packet(p,11,sub,&n)); p[1]=5;
    p[2]^=0x40000000u; assert(!nv2a_batch_inline_packet(p,11,sub,&n)); p[2]^=0x40000000u;
    p[9]^=4; assert(!nv2a_batch_inline_packet(p,11,sub,&n)); p[9]^=4;
    p[10]=1; assert(!nv2a_batch_inline_packet(p,11,sub,&n));
    puts("PFIFO batch: strict complete packet, boundaries and state barriers OK");
}
