"""Exercise the actual world radius helpers and generated patch on both targets."""
import pathlib, runpy, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[3]
options = (ROOT / 'ports/mercenaries/src/recomp_options.c').read_text()
manual = (ROOT / 'ports/mercenaries/src/recomp_manual.c').read_text()

def function(text, marker):
    start = text.index(marker); pos = text.index('{', start); depth = 1; end = pos + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}'); end += 1
    return text[start:end] + '\n'

code = '''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned char memory[0x100000];
static void *guest_ptr(uint32_t p) { return memory + p; }
static uint32_t guest_u32(uint32_t p) { return *(uint32_t *)(memory + p); }
'''
for text, marker in [(options, 'float recomp_options_object_distance_multiplier('),
                     (options, 'float recomp_options_world_view_distance('),
                     (options, 'float recomp_options_streaming_grid_radius('),
                     (options, 'float recomp_options_scale_object_distance('),
                     (manual, 'float recomp_options_camera_far_plane(')]:
    code += function(text, marker)
code += '''
int main(void) {
    uint32_t camera=0x20000;
    *(uint32_t *)(memory+camera)=0x00300DC4;
    *(float *)(memory+camera+0xA0)=300.f;
#ifdef XBOX_ANDROID
    assert(recomp_options_streaming_grid_radius()==80.f);
    assert(recomp_options_camera_far_plane(camera,300.f)==80.f);
    assert(recomp_options_scale_object_distance(300.f,0x6D8B34D5u)==80.f);
#else
    assert(recomp_options_streaming_grid_radius()==320.f);
    assert(recomp_options_camera_far_plane(camera,300.f)==300.f);
    assert(recomp_options_scale_object_distance(300.f,0x6D8B34D5u)==300.f);
#endif
    assert(recomp_options_scale_object_distance(60.f,0x6D8B34D5u)==60.f);
#ifdef XBOX_ANDROID
    assert(recomp_options_scale_object_distance(150.f,0x6D8B34D5u)==80.f);
#else
    assert(recomp_options_scale_object_distance(150.f,0x6D8B34D5u)==150.f);
#endif
    assert(recomp_options_scale_object_distance(300.f,0x12345678u)==300.f);
    assert(recomp_options_camera_far_plane(camera,600.f)==600.f);
    assert(recomp_options_camera_far_plane(0,300.f)==300.f);
    assert(*(float *)(memory+camera+0xA0)==300.f);
    puts("world streaming: camera, scenery, grid and exclusions OK");
}
'''
with tempfile.TemporaryDirectory() as td:
    source = pathlib.Path(td)/'test.c'; source.write_text(code)
    for defines in ([], ['-DXBOX_ANDROID=1']):
        exe = pathlib.Path(td)/'test'
        subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',*defines,str(source),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
patches = runpy.run_path(str(ROOT/'ports/mercenaries/scripts/Patch-Generated.py'))['PATCHES']
p = next(p for p in patches if p.name == 'Scenery streaming grid covers selected radius')
assert 'recomp_options_streaming_grid_radius()' in p.after
print('generated patch: grid uses shared radius helper')
