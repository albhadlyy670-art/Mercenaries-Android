/* Exercise the constant upload cache with a mock D3D11 buffer/context. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int BOOL;
typedef int HRESULT;
#define TRUE 1
#define FALSE 0
#define SUCCEEDED(x) ((x) >= 0)
#define D3D11_MAP_WRITE_DISCARD 4
typedef struct { int unused; } ID3D11DeviceContext;
typedef struct { unsigned char bytes[16]; } ID3D11Buffer;
typedef ID3D11Buffer ID3D11Resource;
typedef struct { void *pData; } D3D11_MAPPED_SUBRESOURCE;
static int maps, unmaps, fail_map;
static HRESULT mock_map(ID3D11DeviceContext *ctx, ID3D11Resource *b, int sub,
                        int type, int flags, D3D11_MAPPED_SUBRESOURCE *m)
{
    (void)ctx; (void)sub; (void)flags;
    assert(type == D3D11_MAP_WRITE_DISCARD); ++maps;
    if (fail_map) return -1;
    m->pData = b->bytes; return 0;
}
static void mock_unmap(ID3D11DeviceContext *ctx, ID3D11Resource *b, int sub)
{ (void)ctx; (void)b; (void)sub; ++unmaps; }
#define ID3D11DeviceContext_Map mock_map
#define ID3D11DeviceContext_Unmap mock_unmap
#include "../../../src/d3d/d3d8_constant_cache.h"
int main(void)
{
    ID3D11DeviceContext ctx = {0}; ID3D11Buffer b = {{0}};
    unsigned char value[16] = {1,2,3}, previous[16] = {0}; BOOL valid = FALSE;
    d3d8_upload_cached_constants(&ctx, &b, value, previous, sizeof(value), &valid);
    assert(valid && maps == 1 && unmaps == 1 && !memcmp(value, b.bytes, sizeof(value)));
    d3d8_upload_cached_constants(&ctx, &b, value, previous, sizeof(value), &valid);
    assert(maps == 1 && unmaps == 1);
    value[15] = 42; fail_map = 1;
    d3d8_upload_cached_constants(&ctx, &b, value, previous, sizeof(value), &valid);
    assert(maps == 2 && unmaps == 1 && previous[15] == 0 && b.bytes[15] == 0);
    fail_map = 0;
    d3d8_upload_cached_constants(&ctx, &b, value, previous, sizeof(value), &valid);
    assert(maps == 3 && unmaps == 2 && previous[15] == 42 && b.bytes[15] == 42);
    valid = FALSE; memset(b.bytes, 0, sizeof(b.bytes));
    d3d8_upload_cached_constants(&ctx, &b, value, previous, sizeof(value), &valid);
    assert(maps == 4 && unmaps == 3 && !memcmp(value, b.bytes, sizeof(value)));
    puts("constant cache: upload, reuse, failure retry, invalidation OK"); return 0;
}
