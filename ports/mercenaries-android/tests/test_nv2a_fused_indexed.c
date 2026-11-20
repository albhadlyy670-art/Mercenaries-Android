#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_indexed_vertices.h"
int main(void)
{
    uint8_t source[12][32], expanded_scratch[2048], compact[4096];
    uint32_t ids[26], rebased[256], n; int skip;
    for (unsigned i=0;i<12;i++) memset(source[i], i+1, 32);
    for (unsigned i=0;i<25;i++) ids[i] = 3 + i%5;
    uint8_t *p=nv2a_expand_indexed_fused(&source[0][0],ids,25,24,32,expanded_scratch,sizeof(expanded_scratch),compact,sizeof(compact),rebased,256,&n,&skip);
    assert(p==expanded_scratch && n==5);
    for (unsigned i=0;i<25;i++) assert(!memcmp(p+i*32,source[ids[i]],32));
    for (unsigned i=0;i<24;i++) assert(!memcmp(compact+rebased[i]*32,p+i*32,32));
    nv2a_free_indexed_vertices(p,expanded_scratch);
    for (unsigned i=0;i<25;i++) ids[i] = i%12;
    p=nv2a_expand_indexed_fused(&source[0][0],ids,25,0,32,expanded_scratch,8,compact,sizeof(compact),rebased,256,&n,&skip);
    assert(p && p!=expanded_scratch && n==0);
    for (unsigned i=0;i<25;i++) assert(!memcmp(p+i*32,source[ids[i]],32));
    nv2a_free_indexed_vertices(p,expanded_scratch);
    assert(!nv2a_expand_indexed_fused(NULL,ids,25,24,32,expanded_scratch,sizeof(expanded_scratch),compact,sizeof(compact),rebased,256,&n,&skip));
    /* Deterministic sequences cover repeats, sparse ranges and list tails. */
    uint32_t seed=7;
    for (unsigned trial=0; trial<500; ++trial) {
        uint32_t count = 12u + trial % 14u;
        uint32_t used = count - count % 3u;
        for (unsigned i=0; i<count; ++i) { seed = seed*1664525u + 1013904223u; ids[i] = (seed >> 16) % (1u + trial % 12u); }
        p=nv2a_expand_indexed_fused(&source[0][0],ids,count,used,32,expanded_scratch,sizeof(expanded_scratch),compact,sizeof(compact),rebased,256,&n,&skip);
        assert(p);
        for (unsigned i=0; i<count; ++i) assert(!memcmp(p+i*32,source[ids[i]],32));
        if (n) for (unsigned i=0; i<used; ++i) { assert(rebased[i] < n); assert(!memcmp(compact+rebased[i]*32,p+i*32,32)); }
        if (skip) {
            Nv2aCompactIndexed fallback;
            assert(!nv2a_compact_indexed_vertices(p,ids,used,32,compact,sizeof(compact),rebased,256,&fallback));
        }
        nv2a_free_indexed_vertices(p,expanded_scratch);
    }
    /* A dense range can fail the 25% saving threshold; the general fallback
     * must agree, so its second scan is safely skipped. */
    for (unsigned i=0;i<24;i++) ids[i]=i%12;
    p=nv2a_expand_indexed_fused(&source[0][0],ids,24,24,4,expanded_scratch,sizeof(expanded_scratch),compact,sizeof(compact),rebased,256,&n,&skip);
    assert(p && !n && skip); nv2a_free_indexed_vertices(p,expanded_scratch);
    /* Insufficient scratch alone does not rule out heap-backed compaction. */
    for (unsigned i=0;i<24;i++) ids[i]=i%5;
    p=nv2a_expand_indexed_fused(&source[0][0],ids,24,24,32,expanded_scratch,sizeof(expanded_scratch),compact,64,rebased,256,&n,&skip);
    assert(p && !n && !skip); nv2a_free_indexed_vertices(p,expanded_scratch);
    puts("NV2A fused indexed snapshot: ordered expansion, compact bytes and fallback OK");
}
