/* Choose one pixel pipeline per draw; a failed combiner falls back to FFP. */
#ifndef D3D8_PIXEL_SELECTION_H
#define D3D8_PIXEL_SELECTION_H
static void d3d8_prepare_selected_pixel(void)
{
    if (!d3d8_combiners_prepare_draw())
        d3d8_shaders_prepare_pixel();
}
#endif
