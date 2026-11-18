#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_draw_batch.h"
/* Compare ordered, oriented, non-degenerate triangles, including odd seams. */
typedef struct { int a,b,c; } Tri;
static size_t strip_tris(const int *v, size_t n, Tri *out)
{
    size_t k=0;
    for(size_t i=0;i+2<n;i++) {
        int a=v[i],b=v[i+1],c=v[i+2];
        if(a==b||b==c||a==c)continue;
        out[k++]=(Tri){(i&1)?b:a,(i&1)?a:b,c};
    }
    return k;
}
static void check_strip(const int *a,size_t na,const int *b,size_t nb,const int *c,size_t nc)
{
    int buf[128]={0}; Tri expected[128], actual[128]; Nv2aDrawBatch q;
    size_t en=strip_tris(a,na,expected); en+=strip_tris(b,nb,expected+en); en+=strip_tris(c,nc,expected+en);
    nv2a_batch_init(&q,buf,sizeof(buf));
    assert(nv2a_batch_append(&q,6,a,na,sizeof(int))==NV2A_BATCH_APPEND);
    assert(nv2a_batch_append(&q,6,b,nb,sizeof(int))==NV2A_BATCH_APPEND);
    assert(nv2a_batch_append(&q,6,c,nc,sizeof(int))==NV2A_BATCH_APPEND);
    size_t an=strip_tris(buf,q.count,actual); assert(en==an);
    for(size_t i=0;i<en;i++) assert(expected[i].a==actual[i].a && expected[i].b==actual[i].b && expected[i].c==actual[i].c);
}
int main(void)
{
    int a[]={1,2,3},b[]={4,5,6,7},c[]={8,9,10,11,12};
    check_strip(a,3,b,4,c,5); check_strip(b,4,c,5,a,3);
    int storage[9]={0}; Nv2aDrawBatch q; nv2a_batch_init(&q,storage,sizeof(storage));
    assert(nv2a_batch_append(&q,5,a,3,sizeof(int))==NV2A_BATCH_APPEND);
    assert(nv2a_batch_append(&q,5,b,3,sizeof(int))==NV2A_BATCH_APPEND);
    assert(q.count==6 && !memcmp(storage,a,sizeof(a)) && !memcmp(storage+3,b,3*sizeof(int)));
    int snapshot[9]; memcpy(snapshot,storage,sizeof(storage));
    assert(nv2a_batch_append(&q,6,c,5,sizeof(int))==NV2A_BATCH_FLUSH);
    assert(nv2a_batch_append(&q,5,c,4,sizeof(int))==NV2A_BATCH_REJECT);
    assert(nv2a_batch_append(&q,5,c,6,sizeof(int))==NV2A_BATCH_FLUSH);
    assert(q.count==6 && q.draws==2 && !memcmp(snapshot,storage,sizeof(storage)));
    nv2a_batch_reset(&q); assert(q.count==0 && q.draws==0);
    assert(nv2a_batch_append(&q,6,b,4,sizeof(int))==NV2A_BATCH_APPEND);
    assert(nv2a_batch_append(&q,6,c,5,sizeof(int))==NV2A_BATCH_FLUSH);
    puts("primitive batching: list, strip winding, ordering and boundaries OK");
}
