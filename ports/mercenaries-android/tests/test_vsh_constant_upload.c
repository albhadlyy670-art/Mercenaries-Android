/* Test the actual programmable-shader constant dirty/upload path. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/d3d/d3d8_vsh.c"
static int maps, unmaps, fail_map;
static NV2AVSConstants gpu;
HRESULT d3dgl_Ctx_Map(ID3D11DeviceContext *ctx, ID3D11Resource *res, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *mapped)
{
    (void)ctx; (void)res; (void)sub; (void)flags;
    assert(type == D3D11_MAP_WRITE_DISCARD); ++maps;
    if (fail_map) return E_FAIL;
    mapped->pData = &gpu; return S_OK;
}
void d3dgl_Ctx_Unmap(ID3D11DeviceContext *ctx, ID3D11Resource *res, UINT sub)
{ (void)ctx; (void)res; (void)sub; ++unmaps; }
int main(void)
{
    float a[4] = {1,2,3,4}, b[4] = {5,6,7,8};
    g_vsh_cb = (ID3D11Buffer *)(uintptr_t)1;
    d3d8_vsh_upload_guest_constants(NULL); assert(maps == 1 && unmaps == 1 && !g_vsh_constants_dirty);
    d3d8_vsh_set_constant(0, a, 1); d3d8_vsh_upload_guest_constants(NULL);
    assert(maps == 2 && !memcmp(gpu.c[0], a, sizeof(a)));
    d3d8_vsh_set_constant(0, b, 1); d3d8_vsh_set_constant(0, a, 1);
    assert(g_vsh_constants_dirty); d3d8_vsh_upload_guest_constants(NULL);
    assert(maps == 2 && !g_vsh_constants_dirty);
    d3d8_vsh_set_constant(0, b, 1); fail_map = 1;
    d3d8_vsh_upload_guest_constants(NULL); assert(maps == 3 && g_vsh_constants_dirty);
    fail_map = 0; d3d8_vsh_upload_guest_constants(NULL);
    assert(maps == 4 && unmaps == 3 && !g_vsh_constants_dirty && !memcmp(gpu.c[0], b, sizeof(b)));
    d3d8_vsh_set_constant(INT_MAX, a, INT_MAX);
    d3d8_vsh_set_constant(0, a, INT_MIN);
    assert(!g_vsh_constants_dirty);
    g_vsh_uploaded_constants_valid = FALSE; g_vsh_constants_dirty = TRUE;
    d3d8_vsh_upload_guest_constants(NULL); assert(maps == 5 && !g_vsh_constants_dirty);
    uint32_t bits[4] = {0x80000000u, 0x7fc01234u, 0, 0}; float exact[4];
    memcpy(exact, bits, sizeof(exact)); d3d8_vsh_set_constant(1, exact, 1);
    d3d8_vsh_upload_guest_constants(NULL); assert(maps == 6 && !memcmp(gpu.c[1], bits, sizeof(bits)));
    bits[0] = 0; memcpy(exact, bits, sizeof(exact)); d3d8_vsh_set_constant(1, exact, 1);
    d3d8_vsh_upload_guest_constants(NULL); assert(maps == 7 && !memcmp(gpu.c[1], bits, sizeof(bits)));
    puts("programmable VS constants: A-B-A reuse, failed-map retry and reset OK");
}
