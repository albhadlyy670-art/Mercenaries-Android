/* d3d8_compiler_gles.c - replaces d3d8_compiler.c on Android.
 * The "compiler" is the in-process HLSL->GLSL ES translator behind D3DCompile
 * (d3d11_gles.c); there is no DLL to select. */
#include "d3d8_compiler.h"

HRESULT d3d8_compile_shader(LPCVOID source, SIZE_T size, LPCSTR name,
    const D3D_SHADER_MACRO *defines, ID3DInclude *include, LPCSTR entry,
    LPCSTR target, UINT flags1, UINT flags2, ID3DBlob **code, ID3DBlob **errors)
{
    return D3DCompile(source, size, name, defines, (ID3DInclude)include, entry, target, flags1, flags2, code, errors);
}
