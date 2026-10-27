/*
 * d3d11_gles_types.h - Direct3D 11 / DXGI / D3DCompiler API surface used by the
 * renderer, implemented over OpenGL ES 3.0 by d3d11_gles.c.
 *
 * All interfaces are opaque handles and every method is a plain function
 * (the renderer only uses COBJMACROS-style calls, never vtables). Enum values
 * match the Windows SDK so structures filled by shared code mean the same thing.
 */
#ifndef D3D11_GLES_TYPES_H
#define D3D11_GLES_TYPES_H

#include "platform/win32_compat.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef D3DGL_HAVE_UINT64
typedef uint64_t UINT64;
typedef uint8_t  UINT8;
#endif
typedef struct { DWORD LowPart; LONG HighPart; } D3DGL_LUID;
typedef D3DGL_LUID LUID_T;
#ifndef LUID
#define LUID D3DGL_LUID
#endif

/* ---- opaque interfaces ---------------------------------------------------- */
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11Resource ID3D11Resource;
typedef struct ID3D11Buffer ID3D11Buffer;
typedef struct ID3D11Texture2D ID3D11Texture2D;
typedef struct ID3D11ShaderResourceView ID3D11ShaderResourceView;
typedef struct ID3D11RenderTargetView ID3D11RenderTargetView;
typedef struct ID3D11DepthStencilView ID3D11DepthStencilView;
typedef struct ID3D11VertexShader ID3D11VertexShader;
typedef struct ID3D11PixelShader ID3D11PixelShader;
typedef struct ID3D11GeometryShader ID3D11GeometryShader;
typedef struct ID3D11InputLayout ID3D11InputLayout;
typedef struct ID3D11BlendState ID3D11BlendState;
typedef struct ID3D11DepthStencilState ID3D11DepthStencilState;
typedef struct ID3D11RasterizerState ID3D11RasterizerState;
typedef struct ID3D11SamplerState ID3D11SamplerState;
typedef struct ID3D11ClassLinkage ID3D11ClassLinkage;
typedef struct ID3D11ClassInstance ID3D11ClassInstance;
typedef struct ID3D10Blob ID3D10Blob;
typedef ID3D10Blob ID3DBlob;
typedef struct IDXGISwapChain IDXGISwapChain;
typedef struct IDXGIFactory IDXGIFactory;
typedef IDXGIFactory IDXGIFactory1, IDXGIFactory5, IDXGIFactory6;
typedef struct IDXGIAdapter IDXGIAdapter;
typedef IDXGIAdapter IDXGIAdapter1;
typedef struct IDXGIDevice IDXGIDevice;
typedef struct IDXGIOutput IDXGIOutput;
typedef struct IUnknown IUnknown;
typedef struct ID3D11Query ID3D11Query;

/* ---- enums (SDK values) ------------------------------------------------------ */
typedef enum {
    DXGI_FORMAT_UNKNOWN = 0, DXGI_FORMAT_R32G32B32A32_FLOAT = 2, DXGI_FORMAT_R32G32B32_FLOAT = 6,
    DXGI_FORMAT_R16G16B16A16_FLOAT = 10, DXGI_FORMAT_R16G16B16A16_UNORM = 11, DXGI_FORMAT_R16G16B16A16_SNORM = 13,
    DXGI_FORMAT_R16G16B16A16_SINT = 14, DXGI_FORMAT_R32G32_FLOAT = 16, DXGI_FORMAT_R10G10B10A2_UNORM = 24,
    DXGI_FORMAT_R8G8B8A8_TYPELESS = 27, DXGI_FORMAT_R8G8B8A8_UNORM = 28, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_FORMAT_R16G16_UNORM = 35, DXGI_FORMAT_R16G16_SNORM = 37, DXGI_FORMAT_R16G16_SINT = 38,
    DXGI_FORMAT_R32_TYPELESS = 39, DXGI_FORMAT_D32_FLOAT = 40, DXGI_FORMAT_R32_FLOAT = 41, DXGI_FORMAT_R32_UINT = 42,
    DXGI_FORMAT_R32_SINT = 43, DXGI_FORMAT_R24G8_TYPELESS = 44, DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46, DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47, DXGI_FORMAT_R8G8_UNORM = 49,
    DXGI_FORMAT_R16_FLOAT = 54, DXGI_FORMAT_D16_UNORM = 55, DXGI_FORMAT_R16_UNORM = 56, DXGI_FORMAT_R16_UINT = 57,
    DXGI_FORMAT_R16_SNORM = 58, DXGI_FORMAT_R16_SINT = 59, DXGI_FORMAT_R8_UNORM = 61, DXGI_FORMAT_A8_UNORM = 65,
    DXGI_FORMAT_BC1_UNORM = 71, DXGI_FORMAT_BC2_UNORM = 74, DXGI_FORMAT_BC3_UNORM = 77, DXGI_FORMAT_B5G6R5_UNORM = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM = 86, DXGI_FORMAT_B8G8R8A8_UNORM = 87, DXGI_FORMAT_B8G8R8X8_UNORM = 88,
    DXGI_FORMAT_B4G4R4A4_UNORM = 115
} DXGI_FORMAT;
#define DXGI_FORMAT_BC3 DXGI_FORMAT_BC3_UNORM

typedef enum { D3D11_USAGE_DEFAULT = 0, D3D11_USAGE_IMMUTABLE = 1, D3D11_USAGE_DYNAMIC = 2, D3D11_USAGE_STAGING = 3 } D3D11_USAGE;
enum { D3D11_BIND_VERTEX_BUFFER = 0x1, D3D11_BIND_INDEX_BUFFER = 0x2, D3D11_BIND_CONSTANT_BUFFER = 0x4,
       D3D11_BIND_SHADER_RESOURCE = 0x8, D3D11_BIND_RENDER_TARGET = 0x20, D3D11_BIND_DEPTH_STENCIL = 0x40 };
enum { D3D11_CPU_ACCESS_WRITE = 0x10000, D3D11_CPU_ACCESS_READ = 0x20000 };
enum { D3D11_RESOURCE_MISC_TEXTURECUBE = 0x4 };
enum { D3D11_CLEAR_DEPTH = 0x1, D3D11_CLEAR_STENCIL = 0x2 };
typedef enum { D3D11_MAP_READ = 1, D3D11_MAP_WRITE = 2, D3D11_MAP_READ_WRITE = 3, D3D11_MAP_WRITE_DISCARD = 4, D3D11_MAP_WRITE_NO_OVERWRITE = 5 } D3D11_MAP;
typedef enum { D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED = 0, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST = 1, D3D11_PRIMITIVE_TOPOLOGY_LINELIST = 2,
               D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP = 3, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST = 4, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5 } D3D11_PRIMITIVE_TOPOLOGY;
typedef D3D11_PRIMITIVE_TOPOLOGY D3D_PRIMITIVE_TOPOLOGY;
typedef enum { D3D11_RESOURCE_DIMENSION_UNKNOWN = 0, D3D11_RESOURCE_DIMENSION_BUFFER = 1, D3D11_RESOURCE_DIMENSION_TEXTURE1D = 2, D3D11_RESOURCE_DIMENSION_TEXTURE2D = 3 } D3D11_RESOURCE_DIMENSION;
typedef enum { D3D11_BLEND_ZERO = 1, D3D11_BLEND_ONE = 2, D3D11_BLEND_SRC_COLOR = 3, D3D11_BLEND_INV_SRC_COLOR = 4, D3D11_BLEND_SRC_ALPHA = 5,
               D3D11_BLEND_INV_SRC_ALPHA = 6, D3D11_BLEND_DEST_ALPHA = 7, D3D11_BLEND_INV_DEST_ALPHA = 8, D3D11_BLEND_DEST_COLOR = 9,
               D3D11_BLEND_INV_DEST_COLOR = 10, D3D11_BLEND_SRC_ALPHA_SAT = 11, D3D11_BLEND_BLEND_FACTOR = 14, D3D11_BLEND_INV_BLEND_FACTOR = 15 } D3D11_BLEND;
typedef enum { D3D11_BLEND_OP_ADD = 1, D3D11_BLEND_OP_SUBTRACT = 2, D3D11_BLEND_OP_REV_SUBTRACT = 3, D3D11_BLEND_OP_MIN = 4, D3D11_BLEND_OP_MAX = 5 } D3D11_BLEND_OP;
#define D3D11_COLOR_WRITE_ENABLE_ALL 15
typedef enum { D3D11_COMPARISON_NEVER = 1, D3D11_COMPARISON_LESS = 2, D3D11_COMPARISON_EQUAL = 3, D3D11_COMPARISON_LESS_EQUAL = 4,
               D3D11_COMPARISON_GREATER = 5, D3D11_COMPARISON_NOT_EQUAL = 6, D3D11_COMPARISON_GREATER_EQUAL = 7, D3D11_COMPARISON_ALWAYS = 8 } D3D11_COMPARISON_FUNC;
typedef enum { D3D11_STENCIL_OP_KEEP = 1, D3D11_STENCIL_OP_ZERO = 2, D3D11_STENCIL_OP_REPLACE = 3, D3D11_STENCIL_OP_INCR_SAT = 4,
               D3D11_STENCIL_OP_DECR_SAT = 5, D3D11_STENCIL_OP_INVERT = 6, D3D11_STENCIL_OP_INCR = 7, D3D11_STENCIL_OP_DECR = 8 } D3D11_STENCIL_OP;
typedef enum { D3D11_DEPTH_WRITE_MASK_ZERO = 0, D3D11_DEPTH_WRITE_MASK_ALL = 1 } D3D11_DEPTH_WRITE_MASK;
typedef enum { D3D11_FILL_WIREFRAME = 2, D3D11_FILL_SOLID = 3 } D3D11_FILL_MODE;
typedef enum { D3D11_CULL_NONE = 1, D3D11_CULL_FRONT = 2, D3D11_CULL_BACK = 3 } D3D11_CULL_MODE;
typedef enum { D3D11_TEXTURE_ADDRESS_WRAP = 1, D3D11_TEXTURE_ADDRESS_MIRROR = 2, D3D11_TEXTURE_ADDRESS_CLAMP = 3,
               D3D11_TEXTURE_ADDRESS_BORDER = 4, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE = 5 } D3D11_TEXTURE_ADDRESS_MODE;
typedef enum { D3D11_FILTER_MIN_MAG_MIP_POINT = 0x0, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR = 0x1, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT = 0x4,
               D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR = 0x5, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT = 0x10, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x11,
               D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT = 0x14, D3D11_FILTER_MIN_MAG_MIP_LINEAR = 0x15, D3D11_FILTER_ANISOTROPIC = 0x55 } D3D11_FILTER;
typedef enum { D3D11_INPUT_PER_VERTEX_DATA = 0, D3D11_INPUT_PER_INSTANCE_DATA = 1 } D3D11_INPUT_CLASSIFICATION;
typedef enum { D3D11_SRV_DIMENSION_TEXTURE2D = 4, D3D11_SRV_DIMENSION_TEXTURECUBE = 9 } D3D11_SRV_DIMENSION;
typedef enum { D3D11_DSV_DIMENSION_TEXTURE2D = 3 } D3D11_DSV_DIMENSION;
typedef enum { D3D11_RTV_DIMENSION_TEXTURE2D = 4 } D3D11_RTV_DIMENSION;
typedef enum { D3D_DRIVER_TYPE_UNKNOWN = 0, D3D_DRIVER_TYPE_HARDWARE = 1 } D3D_DRIVER_TYPE;
typedef enum { D3D_FEATURE_LEVEL_10_0 = 0xa000, D3D_FEATURE_LEVEL_10_1 = 0xa100, D3D_FEATURE_LEVEL_11_0 = 0xb000, D3D_FEATURE_LEVEL_11_1 = 0xb100 } D3D_FEATURE_LEVEL;
typedef enum { D3D11_FEATURE_D3D11_OPTIONS2 = 9 } D3D11_FEATURE;
typedef enum { DXGI_SWAP_EFFECT_DISCARD = 0, DXGI_SWAP_EFFECT_FLIP_DISCARD = 4 } DXGI_SWAP_EFFECT;
typedef enum { DXGI_FEATURE_PRESENT_ALLOW_TEARING = 0 } DXGI_FEATURE;
typedef enum { DXGI_GPU_PREFERENCE_UNSPECIFIED = 0, DXGI_GPU_PREFERENCE_MINIMUM_POWER = 1, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE = 2 } DXGI_GPU_PREFERENCE;
enum { DXGI_USAGE_RENDER_TARGET_OUTPUT = 0x20, DXGI_PRESENT_TEST = 0x1, DXGI_PRESENT_ALLOW_TEARING = 0x200,
       DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING = 0x800, DXGI_MWA_NO_ALT_ENTER = 0x2, DXGI_ADAPTER_FLAG_SOFTWARE = 0x2,
       D3D11_CREATE_DEVICE_DEBUG = 0x2, D3D11_SDK_VERSION = 7, D3DCOMPILE_OPTIMIZATION_LEVEL3 = (1 << 15),
       D3D11_DEFAULT_STENCIL_READ_MASK = 0xff, D3D11_DEFAULT_STENCIL_WRITE_MASK = 0xff,
       D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION = 16384, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE = 16 };
#define D3D11_FLOAT32_MAX 3.402823466e+38f
#ifndef D3D_OK
#define D3D_OK S_OK
#endif
#define DXGI_ERROR_NOT_FOUND ((HRESULT)0x887A0002L)
#define DXGI_ERROR_UNSUPPORTED ((HRESULT)0x887A0004L)
#define DXGI_ERROR_DEVICE_REMOVED ((HRESULT)0x887A0005L)

/* ---- structures --------------------------------------------------------------- */
typedef struct { UINT Count, Quality; } DXGI_SAMPLE_DESC;
typedef struct { UINT Numerator, Denominator; } DXGI_RATIONAL;
typedef struct { UINT Width, Height; DXGI_RATIONAL RefreshRate; DXGI_FORMAT Format; UINT ScanlineOrdering, Scaling; } DXGI_MODE_DESC;
typedef struct { DXGI_MODE_DESC BufferDesc; DXGI_SAMPLE_DESC SampleDesc; UINT BufferUsage, BufferCount; HWND OutputWindow; BOOL Windowed;
                 DXGI_SWAP_EFFECT SwapEffect; UINT Flags; } DXGI_SWAP_CHAIN_DESC;
typedef struct { WCHAR Description[128]; UINT VendorId, DeviceId, SubSysId, Revision; SIZE_T DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory; D3DGL_LUID AdapterLuid; } DXGI_ADAPTER_DESC;
typedef struct { WCHAR Description[128]; UINT VendorId, DeviceId, SubSysId, Revision; SIZE_T DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory; D3DGL_LUID AdapterLuid; UINT Flags; } DXGI_ADAPTER_DESC1;

typedef struct { UINT Width, Height, MipLevels, ArraySize; DXGI_FORMAT Format; DXGI_SAMPLE_DESC SampleDesc; D3D11_USAGE Usage; UINT BindFlags, CPUAccessFlags, MiscFlags; } D3D11_TEXTURE2D_DESC;
typedef struct { UINT ByteWidth; D3D11_USAGE Usage; UINT BindFlags, CPUAccessFlags, MiscFlags, StructureByteStride; } D3D11_BUFFER_DESC;
typedef struct { const void *pSysMem; UINT SysMemPitch, SysMemSlicePitch; } D3D11_SUBRESOURCE_DATA;
typedef struct { void *pData; UINT RowPitch, DepthPitch; } D3D11_MAPPED_SUBRESOURCE;
typedef struct { UINT left, top, front, right, bottom, back; } D3D11_BOX;
typedef RECT D3D11_RECT;
typedef struct { FLOAT TopLeftX, TopLeftY, Width, Height, MinDepth, MaxDepth; } D3D11_VIEWPORT;
typedef struct { LPCSTR SemanticName; UINT SemanticIndex; DXGI_FORMAT Format; UINT InputSlot, AlignedByteOffset; D3D11_INPUT_CLASSIFICATION InputSlotClass; UINT InstanceDataStepRate; } D3D11_INPUT_ELEMENT_DESC;
typedef struct { D3D11_FILTER Filter; D3D11_TEXTURE_ADDRESS_MODE AddressU, AddressV, AddressW; FLOAT MipLODBias; UINT MaxAnisotropy;
                 D3D11_COMPARISON_FUNC ComparisonFunc; FLOAT BorderColor[4]; FLOAT MinLOD, MaxLOD; } D3D11_SAMPLER_DESC;
typedef struct { D3D11_FILL_MODE FillMode; D3D11_CULL_MODE CullMode; BOOL FrontCounterClockwise; INT DepthBias; FLOAT DepthBiasClamp, SlopeScaledDepthBias;
                 BOOL DepthClipEnable, ScissorEnable, MultisampleEnable, AntialiasedLineEnable; } D3D11_RASTERIZER_DESC;
typedef struct { D3D11_STENCIL_OP StencilFailOp, StencilDepthFailOp, StencilPassOp; D3D11_COMPARISON_FUNC StencilFunc; } D3D11_DEPTH_STENCILOP_DESC;
typedef struct { BOOL DepthEnable; D3D11_DEPTH_WRITE_MASK DepthWriteMask; D3D11_COMPARISON_FUNC DepthFunc; BOOL StencilEnable; UINT8 StencilReadMask, StencilWriteMask;
                 D3D11_DEPTH_STENCILOP_DESC FrontFace, BackFace; } D3D11_DEPTH_STENCIL_DESC;
typedef struct { BOOL BlendEnable; D3D11_BLEND SrcBlend, DestBlend; D3D11_BLEND_OP BlendOp; D3D11_BLEND SrcBlendAlpha, DestBlendAlpha; D3D11_BLEND_OP BlendOpAlpha; UINT8 RenderTargetWriteMask; } D3D11_RENDER_TARGET_BLEND_DESC;
typedef struct { BOOL AlphaToCoverageEnable, IndependentBlendEnable; D3D11_RENDER_TARGET_BLEND_DESC RenderTarget[8]; } D3D11_BLEND_DESC;
typedef struct { DXGI_FORMAT Format; D3D11_SRV_DIMENSION ViewDimension;
                 union { struct { UINT MostDetailedMip, MipLevels; } Texture2D; struct { UINT MostDetailedMip, MipLevels; } TextureCube; }; } D3D11_SHADER_RESOURCE_VIEW_DESC;
typedef struct { DXGI_FORMAT Format; D3D11_DSV_DIMENSION ViewDimension; UINT Flags; union { struct { UINT MipSlice; } Texture2D; }; } D3D11_DEPTH_STENCIL_VIEW_DESC;
typedef struct { DXGI_FORMAT Format; D3D11_RTV_DIMENSION ViewDimension; union { struct { UINT MipSlice; } Texture2D; }; } D3D11_RENDER_TARGET_VIEW_DESC;
typedef struct { BOOL PSSpecifiedStencilRefSupported, TypedUAVLoadAdditionalFormats, ROVsSupported; UINT ConservativeRasterizationTier, TiledResourcesTier; BOOL MapOnDefaultTextures, StandardSwizzle, UnifiedMemoryArchitecture; } D3D11_FEATURE_DATA_D3D11_OPTIONS2;
typedef struct { LPCSTR Name; LPCSTR Definition; } D3D_SHADER_MACRO;
typedef void *ID3DInclude;

/* ---- IIDs (values only need to be distinct) ------------------------------------- */
#define D3DGL_IID(n, a) static const GUID n = { 0x7d3d6000u + (a), 0x6c1d, 0x4a11, { 0x9a, 0x01, 0, 0, 0, 0, 0, (uint8_t)(a) } }
D3DGL_IID(IID_ID3D11Texture2D, 1); D3DGL_IID(IID_IDXGIDevice, 2); D3DGL_IID(IID_IDXGIAdapter1, 3); D3DGL_IID(IID_IDXGIFactory, 4);
D3DGL_IID(IID_IDXGIFactory1, 5); D3DGL_IID(IID_IDXGIFactory5, 6); D3DGL_IID(IID_IDXGIFactory6, 7);
#define __uuidof(x) IID_##x

/* ---- host integration (window system side) -------------------------------------- */
typedef struct {
    /* Called by IDXGISwapChain::Present. `texture` is the GL name of the
     * back buffer (top row first, RGBA8). The host blits it to its surface. */
    void (*present)(void *user, uint32_t texture, int width, int height, int sync_interval);
    void *user;
} D3d11GlesHost;
void d3d11gles_set_host(const D3d11GlesHost *host);
/* Number of times GL reported an error since the last call (debug aid). */
int  d3d11gles_take_error_count(void);

/* ---- creation -------------------------------------------------------------------- */
HRESULT D3D11CreateDeviceAndSwapChain(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type, HMODULE software, UINT flags,
        const D3D_FEATURE_LEVEL *levels, UINT n_levels, UINT sdk, const DXGI_SWAP_CHAIN_DESC *sc_desc,
        IDXGISwapChain **swap_chain, ID3D11Device **device, D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context);
HRESULT CreateDXGIFactory1(const GUID *riid, void **factory);
HRESULT D3DCreateBlob(SIZE_T size, ID3D10Blob **blob);
HRESULT D3DCompile(const void *src, SIZE_T len, LPCSTR name, const D3D_SHADER_MACRO *defines, ID3DInclude include,
        LPCSTR entry, LPCSTR target, UINT flags1, UINT flags2, ID3D10Blob **code, ID3D10Blob **errors);

/* ---- implementation functions (see d3d11_gles.c) ----------------------------------- */
ULONG d3dgl_AddRef(void *obj);
ULONG d3dgl_Release(void *obj);
HRESULT d3dgl_QueryInterface(void *obj, const GUID *riid, void **out);
void *d3dgl_Blob_GetBufferPointer(ID3D10Blob *b);
SIZE_T d3dgl_Blob_GetBufferSize(ID3D10Blob *b);

HRESULT d3dgl_Device_CreateBuffer(ID3D11Device *, const D3D11_BUFFER_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Buffer **);
HRESULT d3dgl_Device_CreateTexture2D(ID3D11Device *, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);
HRESULT d3dgl_Device_CreateShaderResourceView(ID3D11Device *, ID3D11Resource *, const D3D11_SHADER_RESOURCE_VIEW_DESC *, ID3D11ShaderResourceView **);
HRESULT d3dgl_Device_CreateRenderTargetView(ID3D11Device *, ID3D11Resource *, const D3D11_RENDER_TARGET_VIEW_DESC *, ID3D11RenderTargetView **);
HRESULT d3dgl_Device_CreateDepthStencilView(ID3D11Device *, ID3D11Resource *, const D3D11_DEPTH_STENCIL_VIEW_DESC *, ID3D11DepthStencilView **);
HRESULT d3dgl_Device_CreateVertexShader(ID3D11Device *, const void *, SIZE_T, ID3D11ClassLinkage *, ID3D11VertexShader **);
HRESULT d3dgl_Device_CreatePixelShader(ID3D11Device *, const void *, SIZE_T, ID3D11ClassLinkage *, ID3D11PixelShader **);
HRESULT d3dgl_Device_CreateGeometryShader(ID3D11Device *, const void *, SIZE_T, ID3D11ClassLinkage *, ID3D11GeometryShader **);
HRESULT d3dgl_Device_CreateInputLayout(ID3D11Device *, const D3D11_INPUT_ELEMENT_DESC *, UINT, const void *, SIZE_T, ID3D11InputLayout **);
HRESULT d3dgl_Device_CreateBlendState(ID3D11Device *, const D3D11_BLEND_DESC *, ID3D11BlendState **);
HRESULT d3dgl_Device_CreateDepthStencilState(ID3D11Device *, const D3D11_DEPTH_STENCIL_DESC *, ID3D11DepthStencilState **);
HRESULT d3dgl_Device_CreateRasterizerState(ID3D11Device *, const D3D11_RASTERIZER_DESC *, ID3D11RasterizerState **);
HRESULT d3dgl_Device_CreateSamplerState(ID3D11Device *, const D3D11_SAMPLER_DESC *, ID3D11SamplerState **);
HRESULT d3dgl_Device_CheckFeatureSupport(ID3D11Device *, D3D11_FEATURE, void *, UINT);
HRESULT d3dgl_Device_GetDeviceRemovedReason(ID3D11Device *);

void d3dgl_Ctx_IASetPrimitiveTopology(ID3D11DeviceContext *, D3D11_PRIMITIVE_TOPOLOGY);
void d3dgl_Ctx_IAGetPrimitiveTopology(ID3D11DeviceContext *, D3D11_PRIMITIVE_TOPOLOGY *);
void d3dgl_Ctx_IASetInputLayout(ID3D11DeviceContext *, ID3D11InputLayout *);
void d3dgl_Ctx_IAGetInputLayout(ID3D11DeviceContext *, ID3D11InputLayout **);
void d3dgl_Ctx_IASetVertexBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *, const UINT *, const UINT *);
void d3dgl_Ctx_IASetIndexBuffer(ID3D11DeviceContext *, ID3D11Buffer *, DXGI_FORMAT, UINT);
void d3dgl_Ctx_VSSetShader(ID3D11DeviceContext *, ID3D11VertexShader *, ID3D11ClassInstance *const *, UINT);
void d3dgl_Ctx_PSSetShader(ID3D11DeviceContext *, ID3D11PixelShader *, ID3D11ClassInstance *const *, UINT);
void d3dgl_Ctx_GSSetShader(ID3D11DeviceContext *, ID3D11GeometryShader *, ID3D11ClassInstance *const *, UINT);
void d3dgl_Ctx_VSGetShader(ID3D11DeviceContext *, ID3D11VertexShader **, ID3D11ClassInstance **, UINT *);
void d3dgl_Ctx_PSGetShader(ID3D11DeviceContext *, ID3D11PixelShader **, ID3D11ClassInstance **, UINT *);
void d3dgl_Ctx_GSGetShader(ID3D11DeviceContext *, ID3D11GeometryShader **, ID3D11ClassInstance **, UINT *);
void d3dgl_Ctx_VSSetConstantBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
void d3dgl_Ctx_PSSetConstantBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
void d3dgl_Ctx_GSSetConstantBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
void d3dgl_Ctx_PSGetConstantBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer **);
void d3dgl_Ctx_GSGetConstantBuffers(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer **);
void d3dgl_Ctx_PSSetShaderResources(ID3D11DeviceContext *, UINT, UINT, ID3D11ShaderResourceView *const *);
void d3dgl_Ctx_PSGetShaderResources(ID3D11DeviceContext *, UINT, UINT, ID3D11ShaderResourceView **);
void d3dgl_Ctx_VSSetShaderResources(ID3D11DeviceContext *, UINT, UINT, ID3D11ShaderResourceView *const *);
void d3dgl_Ctx_PSSetSamplers(ID3D11DeviceContext *, UINT, UINT, ID3D11SamplerState *const *);
void d3dgl_Ctx_PSGetSamplers(ID3D11DeviceContext *, UINT, UINT, ID3D11SamplerState **);
void d3dgl_Ctx_VSSetSamplers(ID3D11DeviceContext *, UINT, UINT, ID3D11SamplerState *const *);
void d3dgl_Ctx_RSSetViewports(ID3D11DeviceContext *, UINT, const D3D11_VIEWPORT *);
void d3dgl_Ctx_RSGetViewports(ID3D11DeviceContext *, UINT *, D3D11_VIEWPORT *);
void d3dgl_Ctx_RSSetState(ID3D11DeviceContext *, ID3D11RasterizerState *);
void d3dgl_Ctx_RSGetState(ID3D11DeviceContext *, ID3D11RasterizerState **);
void d3dgl_Ctx_RSSetScissorRects(ID3D11DeviceContext *, UINT, const D3D11_RECT *);
void d3dgl_Ctx_OMSetRenderTargets(ID3D11DeviceContext *, UINT, ID3D11RenderTargetView *const *, ID3D11DepthStencilView *);
void d3dgl_Ctx_OMGetRenderTargets(ID3D11DeviceContext *, UINT, ID3D11RenderTargetView **, ID3D11DepthStencilView **);
void d3dgl_Ctx_OMSetBlendState(ID3D11DeviceContext *, ID3D11BlendState *, const FLOAT *, UINT);
void d3dgl_Ctx_OMGetBlendState(ID3D11DeviceContext *, ID3D11BlendState **, FLOAT *, UINT *);
void d3dgl_Ctx_OMSetDepthStencilState(ID3D11DeviceContext *, ID3D11DepthStencilState *, UINT);
void d3dgl_Ctx_OMGetDepthStencilState(ID3D11DeviceContext *, ID3D11DepthStencilState **, UINT *);
void d3dgl_Ctx_Draw(ID3D11DeviceContext *, UINT, UINT);
void d3dgl_Ctx_DrawIndexed(ID3D11DeviceContext *, UINT, UINT, INT);
HRESULT d3dgl_Ctx_Map(ID3D11DeviceContext *, ID3D11Resource *, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE *);
void d3dgl_Ctx_Unmap(ID3D11DeviceContext *, ID3D11Resource *, UINT);
void d3dgl_Ctx_UpdateSubresource(ID3D11DeviceContext *, ID3D11Resource *, UINT, const D3D11_BOX *, const void *, UINT, UINT);
void d3dgl_Ctx_CopyResource(ID3D11DeviceContext *, ID3D11Resource *, ID3D11Resource *);
void d3dgl_Ctx_CopySubresourceRegion(ID3D11DeviceContext *, ID3D11Resource *, UINT, UINT, UINT, UINT, ID3D11Resource *, UINT, const D3D11_BOX *);
void d3dgl_Ctx_ClearRenderTargetView(ID3D11DeviceContext *, ID3D11RenderTargetView *, const FLOAT[4]);
void d3dgl_Ctx_ClearDepthStencilView(ID3D11DeviceContext *, ID3D11DepthStencilView *, UINT, FLOAT, UINT8);
void d3dgl_Ctx_Flush(ID3D11DeviceContext *);

void d3dgl_Texture2D_GetDesc(ID3D11Texture2D *, D3D11_TEXTURE2D_DESC *);
void d3dgl_Resource_GetType(ID3D11Resource *, D3D11_RESOURCE_DIMENSION *);
void d3dgl_SRV_GetResource(ID3D11ShaderResourceView *, ID3D11Resource **);
void d3dgl_RSState_GetDesc(ID3D11RasterizerState *, D3D11_RASTERIZER_DESC *);
void d3dgl_BlendState_GetDesc(ID3D11BlendState *, D3D11_BLEND_DESC *);

HRESULT d3dgl_SC_Present(IDXGISwapChain *, UINT, UINT);
HRESULT d3dgl_SC_GetBuffer(IDXGISwapChain *, UINT, const GUID *, void **);
HRESULT d3dgl_SC_ResizeBuffers(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
HRESULT d3dgl_SC_SetFullscreenState(IDXGISwapChain *, BOOL, IDXGIOutput *);
HRESULT d3dgl_SC_GetFullscreenState(IDXGISwapChain *, BOOL *, IDXGIOutput **);
HRESULT d3dgl_SC_GetParent(IDXGISwapChain *, const GUID *, void **);
HRESULT d3dgl_Factory_MakeWindowAssociation(IDXGIFactory *, HWND, UINT);
HRESULT d3dgl_Factory_CheckFeatureSupport(IDXGIFactory *, DXGI_FEATURE, void *, UINT);
HRESULT d3dgl_Factory_EnumAdapterByGpuPreference(IDXGIFactory *, UINT, DXGI_GPU_PREFERENCE, const GUID *, void **);
HRESULT d3dgl_Adapter_GetDesc(IDXGIAdapter *, DXGI_ADAPTER_DESC *);
HRESULT d3dgl_Adapter_GetDesc1(IDXGIAdapter *, DXGI_ADAPTER_DESC1 *);
HRESULT d3dgl_DXGIDevice_GetAdapter(IDXGIDevice *, IDXGIAdapter **);

/* ---- COBJMACROS-style names used by the renderer ---------------------------------- */
#define D3DGL_REL(T) static inline ULONG T##_Release(void *p) { return d3dgl_Release(p); }
D3DGL_REL(ID3D10Blob) D3DGL_REL(ID3D11Texture2D) D3DGL_REL(ID3D11ShaderResourceView) D3DGL_REL(ID3D11PixelShader) D3DGL_REL(ID3D11Buffer)
D3DGL_REL(ID3D11DepthStencilState) D3DGL_REL(ID3D11RasterizerState) D3DGL_REL(ID3D11BlendState) D3DGL_REL(ID3D11VertexShader)
D3DGL_REL(ID3D11RenderTargetView) D3DGL_REL(ID3D11DepthStencilView) D3DGL_REL(ID3D11SamplerState) D3DGL_REL(ID3D11InputLayout)
D3DGL_REL(ID3D11GeometryShader) D3DGL_REL(ID3D11Resource) D3DGL_REL(ID3D11Device) D3DGL_REL(ID3D11DeviceContext)
D3DGL_REL(IDXGISwapChain) D3DGL_REL(IDXGIFactory) D3DGL_REL(IDXGIFactory5) D3DGL_REL(IDXGIFactory6) D3DGL_REL(IDXGIAdapter)
D3DGL_REL(IDXGIAdapter1) D3DGL_REL(IDXGIDevice)
#define ID3D11ShaderResourceView_AddRef(p) d3dgl_AddRef(p)
#define ID3D11Device_QueryInterface(p, iid, out) d3dgl_QueryInterface(p, iid, out)
#define ID3D10Blob_GetBufferPointer(b) d3dgl_Blob_GetBufferPointer(b)
#define ID3D10Blob_GetBufferSize(b) d3dgl_Blob_GetBufferSize(b)

#define ID3D11Device_CreateBuffer(...) d3dgl_Device_CreateBuffer(__VA_ARGS__)
#define ID3D11Device_CreateTexture2D(...) d3dgl_Device_CreateTexture2D(__VA_ARGS__)
#define ID3D11Device_CreateShaderResourceView(...) d3dgl_Device_CreateShaderResourceView(__VA_ARGS__)
#define ID3D11Device_CreateRenderTargetView(...) d3dgl_Device_CreateRenderTargetView(__VA_ARGS__)
#define ID3D11Device_CreateDepthStencilView(...) d3dgl_Device_CreateDepthStencilView(__VA_ARGS__)
#define ID3D11Device_CreateVertexShader(...) d3dgl_Device_CreateVertexShader(__VA_ARGS__)
#define ID3D11Device_CreatePixelShader(...) d3dgl_Device_CreatePixelShader(__VA_ARGS__)
#define ID3D11Device_CreateGeometryShader(...) d3dgl_Device_CreateGeometryShader(__VA_ARGS__)
#define ID3D11Device_CreateInputLayout(...) d3dgl_Device_CreateInputLayout(__VA_ARGS__)
#define ID3D11Device_CreateBlendState(...) d3dgl_Device_CreateBlendState(__VA_ARGS__)
#define ID3D11Device_CreateDepthStencilState(...) d3dgl_Device_CreateDepthStencilState(__VA_ARGS__)
#define ID3D11Device_CreateRasterizerState(...) d3dgl_Device_CreateRasterizerState(__VA_ARGS__)
#define ID3D11Device_CreateSamplerState(...) d3dgl_Device_CreateSamplerState(__VA_ARGS__)
#define ID3D11Device_CheckFeatureSupport(...) d3dgl_Device_CheckFeatureSupport(__VA_ARGS__)
#define ID3D11Device_GetDeviceRemovedReason(...) d3dgl_Device_GetDeviceRemovedReason(__VA_ARGS__)

#define ID3D11DeviceContext_IASetPrimitiveTopology(...) d3dgl_Ctx_IASetPrimitiveTopology(__VA_ARGS__)
#define ID3D11DeviceContext_IAGetPrimitiveTopology(...) d3dgl_Ctx_IAGetPrimitiveTopology(__VA_ARGS__)
#define ID3D11DeviceContext_IASetInputLayout(...) d3dgl_Ctx_IASetInputLayout(__VA_ARGS__)
#define ID3D11DeviceContext_IAGetInputLayout(...) d3dgl_Ctx_IAGetInputLayout(__VA_ARGS__)
#define ID3D11DeviceContext_IASetVertexBuffers(...) d3dgl_Ctx_IASetVertexBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_IASetIndexBuffer(...) d3dgl_Ctx_IASetIndexBuffer(__VA_ARGS__)
#define ID3D11DeviceContext_VSSetShader(...) d3dgl_Ctx_VSSetShader(__VA_ARGS__)
#define ID3D11DeviceContext_PSSetShader(...) d3dgl_Ctx_PSSetShader(__VA_ARGS__)
#define ID3D11DeviceContext_GSSetShader(...) d3dgl_Ctx_GSSetShader(__VA_ARGS__)
#define ID3D11DeviceContext_VSGetShader(...) d3dgl_Ctx_VSGetShader(__VA_ARGS__)
#define ID3D11DeviceContext_PSGetShader(...) d3dgl_Ctx_PSGetShader(__VA_ARGS__)
#define ID3D11DeviceContext_GSGetShader(...) d3dgl_Ctx_GSGetShader(__VA_ARGS__)
#define ID3D11DeviceContext_VSSetConstantBuffers(...) d3dgl_Ctx_VSSetConstantBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_PSSetConstantBuffers(...) d3dgl_Ctx_PSSetConstantBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_GSSetConstantBuffers(...) d3dgl_Ctx_GSSetConstantBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_PSGetConstantBuffers(...) d3dgl_Ctx_PSGetConstantBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_GSGetConstantBuffers(...) d3dgl_Ctx_GSGetConstantBuffers(__VA_ARGS__)
#define ID3D11DeviceContext_PSSetShaderResources(...) d3dgl_Ctx_PSSetShaderResources(__VA_ARGS__)
#define ID3D11DeviceContext_PSGetShaderResources(...) d3dgl_Ctx_PSGetShaderResources(__VA_ARGS__)
#define ID3D11DeviceContext_VSSetShaderResources(...) d3dgl_Ctx_VSSetShaderResources(__VA_ARGS__)
#define ID3D11DeviceContext_PSSetSamplers(...) d3dgl_Ctx_PSSetSamplers(__VA_ARGS__)
#define ID3D11DeviceContext_PSGetSamplers(...) d3dgl_Ctx_PSGetSamplers(__VA_ARGS__)
#define ID3D11DeviceContext_VSSetSamplers(...) d3dgl_Ctx_VSSetSamplers(__VA_ARGS__)
#define ID3D11DeviceContext_RSSetViewports(...) d3dgl_Ctx_RSSetViewports(__VA_ARGS__)
#define ID3D11DeviceContext_RSGetViewports(...) d3dgl_Ctx_RSGetViewports(__VA_ARGS__)
#define ID3D11DeviceContext_RSSetState(...) d3dgl_Ctx_RSSetState(__VA_ARGS__)
#define ID3D11DeviceContext_RSGetState(...) d3dgl_Ctx_RSGetState(__VA_ARGS__)
#define ID3D11DeviceContext_RSSetScissorRects(...) d3dgl_Ctx_RSSetScissorRects(__VA_ARGS__)
#define ID3D11DeviceContext_OMSetRenderTargets(...) d3dgl_Ctx_OMSetRenderTargets(__VA_ARGS__)
#define ID3D11DeviceContext_OMGetRenderTargets(...) d3dgl_Ctx_OMGetRenderTargets(__VA_ARGS__)
#define ID3D11DeviceContext_OMSetBlendState(...) d3dgl_Ctx_OMSetBlendState(__VA_ARGS__)
#define ID3D11DeviceContext_OMGetBlendState(...) d3dgl_Ctx_OMGetBlendState(__VA_ARGS__)
#define ID3D11DeviceContext_OMSetDepthStencilState(...) d3dgl_Ctx_OMSetDepthStencilState(__VA_ARGS__)
#define ID3D11DeviceContext_OMGetDepthStencilState(...) d3dgl_Ctx_OMGetDepthStencilState(__VA_ARGS__)
#define ID3D11DeviceContext_Draw(...) d3dgl_Ctx_Draw(__VA_ARGS__)
#define ID3D11DeviceContext_DrawIndexed(...) d3dgl_Ctx_DrawIndexed(__VA_ARGS__)
#define ID3D11DeviceContext_Map(...) d3dgl_Ctx_Map(__VA_ARGS__)
#define ID3D11DeviceContext_Unmap(...) d3dgl_Ctx_Unmap(__VA_ARGS__)
#define ID3D11DeviceContext_UpdateSubresource(...) d3dgl_Ctx_UpdateSubresource(__VA_ARGS__)
#define ID3D11DeviceContext_CopyResource(...) d3dgl_Ctx_CopyResource(__VA_ARGS__)
#define ID3D11DeviceContext_CopySubresourceRegion(...) d3dgl_Ctx_CopySubresourceRegion(__VA_ARGS__)
#define ID3D11DeviceContext_ClearRenderTargetView(...) d3dgl_Ctx_ClearRenderTargetView(__VA_ARGS__)
#define ID3D11DeviceContext_ClearDepthStencilView(...) d3dgl_Ctx_ClearDepthStencilView(__VA_ARGS__)
#define ID3D11DeviceContext_Flush(...) d3dgl_Ctx_Flush(__VA_ARGS__)

#define ID3D11Texture2D_GetDesc(...) d3dgl_Texture2D_GetDesc(__VA_ARGS__)
#define ID3D11Resource_GetType(...) d3dgl_Resource_GetType(__VA_ARGS__)
#define ID3D11ShaderResourceView_GetResource(...) d3dgl_SRV_GetResource(__VA_ARGS__)
#define ID3D11RasterizerState_GetDesc(...) d3dgl_RSState_GetDesc(__VA_ARGS__)
#define ID3D11BlendState_GetDesc(...) d3dgl_BlendState_GetDesc(__VA_ARGS__)

#define IDXGISwapChain_Present(...) d3dgl_SC_Present(__VA_ARGS__)
#define IDXGISwapChain_GetBuffer(...) d3dgl_SC_GetBuffer(__VA_ARGS__)
#define IDXGISwapChain_ResizeBuffers(...) d3dgl_SC_ResizeBuffers(__VA_ARGS__)
#define IDXGISwapChain_SetFullscreenState(...) d3dgl_SC_SetFullscreenState(__VA_ARGS__)
#define IDXGISwapChain_GetFullscreenState(...) d3dgl_SC_GetFullscreenState(__VA_ARGS__)
#define IDXGISwapChain_GetParent(...) d3dgl_SC_GetParent(__VA_ARGS__)
#define IDXGIFactory_MakeWindowAssociation(...) d3dgl_Factory_MakeWindowAssociation(__VA_ARGS__)
#define IDXGIFactory5_CheckFeatureSupport(...) d3dgl_Factory_CheckFeatureSupport(__VA_ARGS__)
#define IDXGIFactory6_EnumAdapterByGpuPreference(...) d3dgl_Factory_EnumAdapterByGpuPreference(__VA_ARGS__)
#define IDXGIAdapter_GetDesc(...) d3dgl_Adapter_GetDesc(__VA_ARGS__)
#define IDXGIAdapter1_GetDesc1(...) d3dgl_Adapter_GetDesc1(__VA_ARGS__)
#define IDXGIDevice_GetAdapter(...) d3dgl_DXGIDevice_GetAdapter(__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
