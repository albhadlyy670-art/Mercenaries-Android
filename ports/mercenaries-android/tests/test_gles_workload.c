/* Check Android-only counters, buckets and interval reset without a GPU. */
#define __ANDROID__ 1
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
#include <assert.h>
#include <stdarg.h>
static char workload_line[512], small_line[256];
void android_trace(const char *fmt, ...)
{
    if (strncmp(fmt, "[perf-gles-workload]", 20) == 0) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(workload_line, sizeof(workload_line), fmt, ap);
        va_end(ap);
    } else if (strncmp(fmt, "[perf-gles-small]", 17) == 0) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(small_line, sizeof(small_line), fmt, ap);
        va_end(ap);
    }
}
int main(void)
{
    PERF_DRAW_WORK(16, 0, 1, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(17, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(64, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    PERF_DRAW_WORK(65, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(256, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(257, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    PERF_FBO_REBIND(); PERF_ATTACHMENT(); PERF_ATTACHMENT();
    assert(perf_workload.arrays == 3 && perf_workload.indexed == 3);
    assert(perf_workload.vertices == 675 && perf_workload.small == 1);
    assert(perf_workload.small_vertexid == 1 && perf_workload.small_attributes == 0 && perf_workload.small_triangles == 1);
    assert(perf_workload.medium == 2 && perf_workload.large == 2 && perf_workload.huge == 1);
    perf_start = perf_now() - 11.0; perf_report();
    assert(strstr(workload_line, "arrays=3 indexed=3 vertices=675"));
    assert(strstr(workload_line, "fbo_rebinds=1 attachment_updates=2"));
    assert(strstr(small_line, "vertexid=1 attributes=0 triangle_list=1"));
    assert(perf_workload.arrays == 0 && perf_workload.attachments == 0);
    PERF_DRAW_WORK(4, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    PERF_DRAW_WORK(2, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    assert(perf_workload.small_attributes == 2 && perf_workload.small_strips == 1 && perf_workload.small_other == 1);
    puts("GLES workload counters: categories, reporting and reset OK");
    return 0;
}
