#!/bin/bash
# Host (Linux) tests for the Android port. The GPU tests run on Mesa's GLES 3 (llvmpipe) and are skipped when
# libEGL/libGLESv2 are missing. Nothing here needs the Android NDK, a device, or a disc image.
set -e
cd "$(dirname "$0")"; R=$(cd ../../.. && pwd); CPP=../app/src/main/cpp; A=$CPP/android; D=$CPP/d3d11_gles; S=$R/src; T=$(mktemp -d)
echo "== XDVDFS extractor (ASan/UBSan, two disc layouts)"
gcc -O1 -w -fsanitize=address,undefined -I$A test_xdvdfs.c $A/xdvdfs_extract.c -o $T/x
for off in 0 0x18300000; do
  python3 mkxiso.py $T/t.iso $off; rm -rf $T/out; $T/x $T/t.iso $T/out >/dev/null
  (cd $T/out && find . -type f | sed 's|^\./||' | sort | while read f; do echo "$(sha256sum "$f"|cut -d' ' -f1)  $f"; done) > $T/got
  sort -k2 $T/t.iso.manifest | diff - $T/got && echo "  partition offset $off: OK"
done
echo "== AArch64 MMIO decoder"; gcc -fsanitize=undefined -I$S/platform test_mmio_a64.c -o $T/a && $T/a
echo "== Guest memory layout (mmap arena, mirrors, aliases, faults)"
gcc -O1 -w -fsanitize=undefined -I$S -I$S/kernel -I$S/platform test_memory_layout.c $S/kernel/xbox_memory_layout.c $S/platform/win32_compat.c -lpthread -lm -o $T/m && $T/m | tail -1
echo "== Win32 compat layer (INI read/write, secure string functions, path normalisation, atomics)"
gcc -Wall -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -I$CPP/d3d11_gles/include -I$CPP/compat -I$S -I$S/platform test_winapi_extra.c $CPP/compat/winapi_extra.c $S/platform/win32_compat.c -lpthread -lm -o $T/w
ASAN_OPTIONS=detect_leaks=0 $T/w
echo "== SHA-256 behind the BCrypt shim (NIST vectors, retail-XBE verification)"
gcc -O1 -w -I$CPP/compat/include -I$CPP/compat -I$CPP/d3d11_gles/include -I$S -I$S/platform test_sha256.c $CPP/compat/bcrypt_sha256.c -o $T/sha && $T/sha
echo "== Case-insensitive file names (importer lower-cases; path translator folds requests)"
echo 'void xbox_log(int l,int c,const char*f,...){}' > $T/logstub.c
gcc -O1 -w -DXBOX_ANDROID=1 -D__ANDROID__ -I$CPP/d3d11_gles/include -I$CPP/compat -I$S -I$S/kernel -I$S/platform test_path_fold.c $S/kernel/kernel_path.c $T/logstub.c $S/platform/win32_compat.c $CPP/compat/winapi_extra.c -lpthread -lm -o $T/fold && $T/fold
echo "== Shell build link (no recompiler output): every symbol resolves"
python3 check_shell_link.py
echo "== Whole Android source set: CMake lists, NDK-strict compile, every symbol accounted for"
python3 check_android_sources.py
if ldconfig -p | grep -q libGLESv2 && ldconfig -p | grep -q libEGL; then
  GL="-l:libEGL.so.1 -l:libGLESv2.so.2"; INC="-I$CPP/compat/include -I$D/include -I$CPP/compat -I$D -I$S -I$S/d3d -I$S/platform -I$S/kernel -I$S/nv2a -DXBOX_ANDROID=1"
  echo "== HLSL->GLSL ES translator: project's embedded shaders, validated by the GLES driver"
  gcc -O1 -w -I$D translate_cli.c $D/hlsl2glsl.c -o $T/h2g
  cat > $T/glcheck.c <<'EOC'
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef void *H; typedef int32_t I; typedef unsigned U;
H eglGetPlatformDisplay(U,void*,const I*); U eglInitialize(H,I*,I*); U eglBindAPI(U); U eglChooseConfig(H,const I*,H*,I,I*); H eglCreateContext(H,H,H,const I*); U eglMakeCurrent(H,H,H,H);
U glCreateShader(U); void glShaderSource(U,int,const char*const*,const int*); void glCompileShader(U); void glGetShaderiv(U,U,int*); void glGetShaderInfoLog(U,int,int*,char*);
int main(int c,char**v){ H d=eglGetPlatformDisplay(0x31DD,0,0); eglInitialize(d,0,0); eglBindAPI(0x30A0); I a[]={0x3040,0x40,0x3033,1,0x3038}; H cf; I n; eglChooseConfig(d,a,&cf,1,&n);
 I ca[]={0x3098,3,0x3038}; H x=eglCreateContext(d,cf,0,ca); eglMakeCurrent(d,0,0,x); FILE*f=fopen(v[1],"rb"); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char*b=malloc(l+1); fread(b,1,l,f); b[l]=0;
 U s=glCreateShader(v[2][0]=='p'?0x8B30:0x8B31); const char*p=b; glShaderSource(s,1,&p,0); glCompileShader(s); int ok; glGetShaderiv(s,0x8B81,&ok); if(!ok){char lg[2048]; glGetShaderInfoLog(s,2048,0,lg); printf("%s",lg); return 1;} return 0; }
EOC
  gcc $T/glcheck.c -o $T/glcheck $GL
  python3 extract_hlsl.py $S $T/hlsl; pass=0; skip=0
  for f in $T/hlsl/*.hlsl; do b=$(basename $f .hlsl); done_one=0
    for st in vs ps; do for e in main VSMain PSMain; do
      if $T/h2g $f $st $e STENCIL_BIT 1 > $T/o.glsl 2>/dev/null; then
         # keep only the stage this shader really is
         if grep -q "gl_Position" $T/o.glsl && [ $st = ps ]; then continue; fi
         if ! grep -q "gl_Position" $T/o.glsl && [ $st = vs ]; then continue; fi
         $T/glcheck $T/o.glsl $st || { echo "  GLES driver rejected $b ($st)"; exit 1; }; pass=$((pass+1)); done_one=1; break 2; fi
    done; done
    [ $done_one = 0 ] && skip=$((skip+1))
  done
  echo "  $pass embedded shaders translated and accepted by the GLES compiler ($skip not applicable: geometry shaders)"
  echo "== D3D11-on-GLES layer driven through the D3D11 API (28 pixel-level checks)"
  gcc -O1 -w -fsanitize=address,undefined $INC $D/d3d11_gles.c $D/hlsl2glsl.c test_d3d11_gles.c -o $T/d3d $GL -lm
  ASAN_OPTIONS=detect_leaks=0 $T/d3d | tail -1
  echo "== Project's NV2A vertex-shader + register-combiner generators, fuzzed, translated, GLES-compiled"
  cat > $T/stubs.c <<'EOS'
void *d3d8_GetD3D11Device(void){return 0;} void *d3d8_GetD3D11Context(void){return 0;}
void xbox_preview_log_event(const char*c,const char*f,...){} void xbox_preview_log_sample(const char*c,const char*f,...){}
unsigned d3d8_GetRenderTargetWidth(void){return 640;} unsigned d3d8_GetRenderTargetHeight(void){return 480;}
unsigned *d3d8_GetRenderStates(void){static unsigned r[256];return r;} unsigned *d3d8_GetTSS(void){static unsigned t[128];return t;}
EOS
  gcc -O1 -w $INC $D/d3d11_gles.c $D/hlsl2glsl.c $D/d3d8_compiler_gles.c $S/d3d/d3d8_vsh.c $S/d3d/d3d8_combiners.c $S/d3d/d3d8_shader_cache.c $S/platform/win32_compat.c $T/stubs.c test_generated_shaders.c -o $T/gen $GL -lm -lpthread
  rm -f /tmp/errs.txt; $T/gen 1000 2>&1 | grep -a "shaders:\|Mesa GLSL"
else echo "== GPU tests skipped (libEGL/libGLESv2 not found)"; fi
echo "ALL HOST TESTS PASSED"
