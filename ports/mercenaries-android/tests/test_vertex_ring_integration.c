/* Exercise the actual D3D8 upload function with a mock GLES ring. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/d3d/d3d8_device.c"
static unsigned uploads, reuses, discards, fail_upload;
static uint8_t gpu_ring[UP_RING_BUFFER_SIZE];
HRESULT d3dgl_Device_CreateBuffer(ID3D11Device *device, const D3D11_BUFFER_DESC *d,
                                 const D3D11_SUBRESOURCE_DATA *initial, ID3D11Buffer **out)
{ (void)device; (void)d; (void)initial; *out = (ID3D11Buffer *)(uintptr_t)1; return S_OK; }
ULONG d3dgl_Release(void *object) { (void)object; return 0; }
void d3d11gles_note_vertex_ring_reuse(void) { ++reuses; }
HRESULT d3d11gles_upload_vertex_range(ID3D11Buffer *b, UINT offset, const void *data, UINT size, BOOL discard)
{
    (void)b; ++uploads;
    if (fail_upload) return E_FAIL;
    assert(offset <= sizeof(gpu_ring) && size <= sizeof(gpu_ring) - offset);
    if (discard) { ++discards; memset(gpu_ring, 0, sizeof(gpu_ring)); }
    memcpy(gpu_ring + offset, data, size); return S_OK;
}
int main(void)
{
    uint8_t a[8] = {1,2,3,4}, b[8] = {5,6,7,8};
    UINT x = up_ring_upload(a, sizeof(a)); assert(x == 0 && uploads == 1);
    UINT y = up_ring_upload(a, sizeof(a)); assert(y == x && uploads == 1 && reuses == 1);
    a[0] = 9; y = up_ring_upload(a, sizeof(a)); assert(y != x && uploads == 2);
    assert(gpu_ring[x] == 1 && gpu_ring[y] == 9);
    /* Wrapping invalidates earlier offsets, even if bytes match later. */
    g_up_ring_offset = UP_RING_BUFFER_SIZE;
    assert(up_ring_upload(b, sizeof(b)) == 0 && discards == 1);
    a[0] = 1; assert(up_ring_upload(a, sizeof(a)) != x && uploads == 4);
    fail_upload = 1; b[0] = 42; assert(up_ring_upload(b, sizeof(b)) == (UINT)-1);
    fail_upload = 0; unsigned before = discards;
    assert(up_ring_upload(a, sizeof(a)) == 0 && discards == before + 1);
    up_ring_shutdown();
    before = uploads; assert(up_ring_upload(a, sizeof(a)) == 0 && uploads == before + 1);
    up_ring_shutdown();
    puts("vertex ring integration: reuse, wrap, failure and shutdown OK");
}
