#!/usr/bin/env python3
"""Simulates linking the *shell* build (no recompiler output) and fails if any symbol would be undefined.

Compiles the Android glue (against the stand-in NDK headers in tests/fake_ndk), the toolkit and the shell stubs,
then follows static-archive semantics: toolkit objects are only linked if something reachable references them.
"""
import glob, os, re, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); cpp = os.path.normpath(here + '/../app/src/main/cpp'); repo = os.path.normpath(here + '/../../..')
src = repo + '/src'; port = repo + '/ports/mercenaries'; fake = here + '/fake_ndk'
work = tempfile.mkdtemp(); gen = work + '/gen'; objs = work + '/o'; os.makedirs(objs)
subprocess.check_call([sys.executable, cpp + '/gen_game_headers.py', port, gen], stdout=subprocess.DEVNULL)
def g(p, ex=None): return [f for f in sorted(glob.glob(p)) if not (ex and re.search(ex, f))]
toolkit = (g(src + '/kernel/*.c', r'(preview_log|mod_overlay)\.c$') + g(src + '/apu/*.c', r'apu_xaudio2\.c$') + g(src + '/nv2a/*.c', r'nv2a_pb_test\.c$')
           + g(src + '/d3d/*.c', r'd3d8_(gl|compiler)\.c$') + [src + '/platform/win32_compat.c']
           + [cpp + '/d3d11_gles/' + f for f in ('d3d11_gles.c', 'hlsl2glsl.c', 'd3d8_compiler_gles.c')] + [cpp + '/compat/winapi_extra.c', cpp + '/compat/bcrypt_sha256.c'])
glue = [f for f in g(cpp + '/android/*.c') if not re.search(r'android_(jni|fault)\.c$', f)]   # need real jni.h / arm64 ucontext
inc = ['-I' + fake, '-I' + cpp + '/compat/include', '-I' + cpp + '/d3d11_gles/include'] + ['-I' + p for p in [cpp + '/compat', cpp + '/d3d11_gles', cpp + '/android', src, src + '/kernel', src + '/apu', src + '/nv2a', src + '/platform', src + '/input', src + '/audio', src + '/d3d', port + '/src', port + '/src/recomp', repo + '/include', gen]]
base = ['gcc', '-c', '-O0', '-w', '-fwrapv', '-DXBOX_ANDROID=1', '-D_GNU_SOURCE', '-DXBOX_DEFAULT_THREAD_STACK=67108864', '-D__ANDROID_API__=26'] + inc
allobj = {}
for i, f in enumerate(toolkit + glue):
    fl = ['-D__ANDROID__']
    if re.search(r'(apu|nv2a)_mmio_hook', f): fl.append('-D__aarch64__')
    if f.endswith('d3d11_gles.c'): fl = []
    o = '%s/%03d.o' % (objs, i); r = subprocess.run(base + fl + [f, '-o', o], capture_output=True, text=True)
    if r.returncode: sys.exit('COMPILE FAIL %s\n%s' % (f, r.stderr[:500]))
    allobj[o] = f
info = {}
for o in allobj:
    d, u = set(), set()
    for l in subprocess.run(['nm', o], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 2 and p[0] == 'U': u.add(p[1])
        elif len(p) == 3 and p[1] in 'TDBRCWV': d.add(p[2])
    info[o] = (d, u)
prov = {}
for o, (d, u) in info.items():
    for s in d: prov.setdefault(s, o)
roots = [o for o, f in allobj.items() if f in glue]
need = set(roots); stack = list(roots)
for s in ('nv2a_hook_handle_mmio', 'apu_hook_handle_mmio', 'ac97_hook_handle_mmio', 'nv2a_hook_handle_vram', 'xbox_MapPhysicalAlias', 'g_xbox_mem_offset'):   # referenced by android_fault.c
    if s in prov and prov[s] not in need: need.add(prov[s]); stack.append(prov[s])
while stack:
    o = stack.pop()
    for s in info[o][1]:
        p = prov.get(s)
        if p and p not in need: need.add(p); stack.append(p)
unres = set()
for o in need: unres |= {s for s in info[o][1] if s not in prov}
libc = set()
for lib in ('libc.so.6', 'libm.so.6'):
    libc |= {l.split()[-1].split('@')[0] for l in subprocess.run(['nm', '-D', '/lib/x86_64-linux-gnu/' + lib], capture_output=True, text=True).stdout.splitlines() if l.strip()}
ndk = re.compile(r'^(A[A-Z][A-Za-z]+_|ALooper|ANative|AInput|AMotion|AKey|AAudio|egl[A-Z]|gl[A-Z]|__android_log|JNI_)')
bad = sorted(s for s in unres if s not in libc and not ndk.match(s) and s != 'atexit' and not s.startswith(('__', '_GLOBAL')))
print('shell link: %d of %d toolkit objects pulled in' % (len(need) - len(roots), len(toolkit)))
if bad: print('WOULD BE UNDEFINED:', ' '.join(bad)); sys.exit(1)
print('OK: the shell build links (every symbol is defined, libc, or an NDK library)')
