#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/d3d/d3d8_vertex_ring_cache.h"
int main(void)
{
    D3D8VertexRingCache cache = {0}; uint32_t offset;
    uint8_t data[129] = {1,2,3}; uint8_t same[129]; memcpy(same,data,sizeof(data));
    assert(!d3d8_vertex_cache_find(&cache,data,3,&offset));
    d3d8_vertex_cache_store(&cache,data,3,16);
    assert(d3d8_vertex_cache_find(&cache,same,3,&offset) && offset == 16);
    data[1] = 7;
    assert(!d3d8_vertex_cache_find(&cache,data,3,&offset));
    assert(d3d8_vertex_cache_find(&cache,same,3,&offset) && offset == 16);
    assert(!d3d8_vertex_cache_find(&cache,same,2,&offset));
    assert(!d3d8_vertex_cache_find(&cache,same,129,&offset));
    for (unsigned i=0; i<D3D8_VERTEX_CACHE_ENTRIES; i++) { data[0]=(uint8_t)(10+i); d3d8_vertex_cache_store(&cache,data,3,32+i*16); }
    assert(!d3d8_vertex_cache_find(&cache,same,3,&offset));
    assert(d3d8_vertex_cache_find(&cache,data,3,&offset) && offset==80);
    d3d8_vertex_cache_clear(&cache);
    assert(!d3d8_vertex_cache_find(&cache,data,3,&offset));
    d3d8_vertex_cache_store(&cache,data,129,123);
    assert(!d3d8_vertex_cache_find(&cache,data,129,&offset));
    puts("vertex ring cache: exact bytes, eviction, snapshot and discard invalidation OK");
}
