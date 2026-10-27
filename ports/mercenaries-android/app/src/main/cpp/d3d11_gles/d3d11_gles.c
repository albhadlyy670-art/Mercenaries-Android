/*
 * d3d11_gles.c - Direct3D 11 / DXGI / D3DCompile implemented on OpenGL ES 3.0.
 *
 * Built from three numbered parts into one translation unit. The renderer's
 * D3D11 calls (nv2a_pgraph_d3d11.c, d3d8_*.c) compile unmodified against
 * include/d3d11.h and run here. Requires a current GLES 3.0 context on the
 * calling thread; window-system presentation is delegated through
 * d3d11gles_set_host().
 *
 * Known gaps (GLES 3.0 limits): geometry shaders, per-render-target blend
 * state (render target 0's state is used), wireframe fill, depth-clip control,
 * stencil-value texture sampling, BC read-back, MSAA.
 */
#include "d3d11_gles_a.inc"
#include "d3d11_gles_b.inc"
#include "d3d11_gles_c.inc"
