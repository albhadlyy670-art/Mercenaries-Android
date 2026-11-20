/* Check Android-only counters, buckets and interval reset without a GPU. */
#define __ANDROID__ 1
#include "../app/src/main/cpp/d3d11_gles/d3d11_gles.c"
#include <assert.h>
#include <stdarg.h>
static char workload_line[512], small_line[256], mmio_line[256], buffer_line[512], target_line[256], target_other_line[128];
static Nv2aMmioPerfCounts mock_mmio;
Nv2aMmioPerfCounts nv2a_mmio_get_perf_counts(void) { return mock_mmio; }
void android_trace(const char *fmt, ...)
{
    if (strncmp(fmt, "[perf-gles-buffer]", 18) == 0 && strstr(fmt, "kind=%s")) {
        va_list ap; va_start(ap, fmt);
        const char *kind = va_arg(ap, const char *);
        va_end(ap);
        if (!strcmp(kind, "constant")) {
            va_start(ap, fmt); vsnprintf(buffer_line, sizeof(buffer_line), fmt, ap); va_end(ap);
        }
    } else if (strncmp(fmt, "[perf-gles-target]", 18) == 0) {
        va_list ap; va_start(ap, fmt); vsnprintf(target_line, sizeof(target_line), fmt, ap); va_end(ap);
    } else if (strncmp(fmt, "[perf-gles-target-other]", 24) == 0) {
        va_list ap; va_start(ap, fmt); vsnprintf(target_other_line, sizeof(target_other_line), fmt, ap); va_end(ap);
    } else if (strncmp(fmt, "[perf-gles-workload]", 20) == 0) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(workload_line, sizeof(workload_line), fmt, ap);
        va_end(ap);
    } else if (strncmp(fmt, "[perf-mmio]", 11) == 0) {
        va_list ap; va_start(ap, fmt); vsnprintf(mmio_line, sizeof(mmio_line), fmt, ap); va_end(ap);
    } else if (strncmp(fmt, "[perf-gles-small]", 17) == 0) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(small_line, sizeof(small_line), fmt, ap);
        va_end(ap);
    }
}
int main(void)
{
    mock_mmio = (Nv2aMmioPerfCounts){100, 200, 30}; perf_start = 0; perf_report();
    PERF_DRAW_WORK(16, 0, 1, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(17, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(64, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    PERF_DRAW_WORK(65, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(256, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); PERF_DRAW_WORK(257, 1, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    PERF_FBO_REBIND(); PERF_ATTACHMENT(); PERF_ATTACHMENT();
    assert(perf_buffer_kind(D3D11_BIND_CONSTANT_BUFFER) == 0);
    assert(perf_buffer_kind(D3D11_BIND_VERTEX_BUFFER) == 1);
    assert(perf_buffer_kind(D3D11_BIND_INDEX_BUFFER) == 2);
    assert(perf_buffer_kind(D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER) == 3);
    perf_workload.map_kind[0] = 3; perf_workload.map_discard[0] = 2;
    perf_workload.map_bytes[0] = 120; perf_workload.updates[0] = 1;
    perf_constant_map_size(16); perf_constant_map_size(16);
    for (unsigned i = 0; i < 6; ++i) perf_constant_map_size(100 + i);
    assert(perf_workload.constant_sizes[0].maps == 2 && perf_workload.constant_size_other == 1);
    PERF_TARGET_DRAW(640, 480, 1, 8); PERF_TARGET_DRAW(640, 480, 1, 12);
    for (unsigned i = 0; i < 6; ++i) PERF_TARGET_DRAW(1 + i, 1, 0, 3);
    assert(perf_workload.target_other == 1);
    assert(perf_workload.arrays == 3 && perf_workload.indexed == 3);
    assert(perf_workload.vertices == 675 && perf_workload.small == 1);
    assert(perf_workload.small_vertexid == 1 && perf_workload.small_attributes == 0 && perf_workload.small_triangles == 1);
    assert(perf_workload.medium == 2 && perf_workload.large == 2 && perf_workload.huge == 1);
    mock_mmio = (Nv2aMmioPerfCounts){112, 245, 33};
    perf_start = perf_now() - 11.0; perf_report();
    assert(strstr(workload_line, "arrays=3 indexed=3 vertices=675"));
    assert(strstr(workload_line, "fbo_rebinds=1 attachment_updates=2"));
    assert(strstr(small_line, "vertexid=1 attributes=0 triangle_list=1"));
    assert(strstr(buffer_line, "kind=constant maps=3 discard=2 failures=0 mapped_bytes=120"));
    assert(strstr(buffer_line, "updates=1 update_bytes=0"));
    assert(strstr(target_other_line, "draws=1"));
    assert(strstr(mmio_line, "sigsegv_reads=12 sigsegv_writes=45 put_kicks=3"));
    mock_mmio = (Nv2aMmioPerfCounts){1000, 2000, 300};
    perf_start = perf_now() - 31.0; perf_report(); /* background gap resets baseline */
    mock_mmio = (Nv2aMmioPerfCounts){1001, 2002, 303};
    perf_start = perf_now() - 11.0; perf_report();
    assert(strstr(mmio_line, "sigsegv_reads=1 sigsegv_writes=2 put_kicks=3"));
    assert(perf_workload.arrays == 0 && perf_workload.attachments == 0);
    assert(perf_workload.map_kind[0] == 0 && perf_workload.target_other == 0);
    PERF_DRAW_WORK(4, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    PERF_DRAW_WORK(2, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    assert(perf_workload.small_attributes == 2 && perf_workload.small_strips == 1 && perf_workload.small_other == 1);
    puts("GLES workload counters: categories, reporting and reset OK");
    return 0;
}
