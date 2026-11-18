#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/nv2a/nv2a_immediate_vertices.h"
int main(void)
{
    Nv2aImmediateVertex src[8], scratch[12];
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned a = 0; a < 16; ++a)
            for (unsigned j = 0; j < 4; ++j) src[i][a][j] = (float)(i * 100 + a * 4 + j);
    assert(nv2a_immediate_vertex_source(src, NULL, 8, 0, 0) == src);
    assert(nv2a_immediate_vertex_source(src, scratch, 8, 1, 0) == scratch);
    const unsigned order[] = {0,1,2,0,2,3,4,5,6,4,6,7};
    for (unsigned i = 0; i < 12; ++i) assert(!memcmp(scratch[i], src[order[i]], sizeof(src[0])));
    assert(nv2a_immediate_vertex_source(src, scratch, 5, 0, 1) == scratch);
    for (unsigned i = 0; i < 5; ++i) assert(!memcmp(scratch[i], src[i], sizeof(src[0])));
    assert(!memcmp(scratch[5], src[0], sizeof(src[0])));
    puts("immediate vertices: direct strips, quad expansion and line loop OK");
    return 0;
}
