#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_indexed_vertices.h"
int main(void)
{
    const uint8_t source[] = { 1,2,3,4, 5,6,7,8, 9,10,11,12 };
    const uint32_t indices[] = { 2,0,2,1 };
    const uint8_t expected[] = { 9,10,11,12, 1,2,3,4, 9,10,11,12, 5,6,7,8 };
    uint8_t scratch[16];
    uint8_t *p=nv2a_expand_indexed_vertices(source,indices,4,4,scratch,sizeof(scratch));
    assert(p==scratch && !memcmp(p,expected,sizeof(expected)));
    nv2a_free_indexed_vertices(p,scratch);
    p=nv2a_expand_indexed_vertices(source,indices,4,4,scratch,8);
    assert(p && p!=scratch && !memcmp(p,expected,sizeof(expected)));
    nv2a_free_indexed_vertices(p,scratch);
    assert(!nv2a_expand_indexed_vertices(source,indices,0,4,scratch,sizeof(scratch)));
    assert(!nv2a_expand_indexed_vertices(source,indices,4,0,scratch,sizeof(scratch)));
    nv2a_free_indexed_vertices(NULL,scratch);
    puts("indexed vertices: bounded scratch, heap fallback and order OK");
}
