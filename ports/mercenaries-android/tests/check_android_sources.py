#!/usr/bin/env python3
"""Simulates the Android CMake build on the host (no NDK needed) and checks it.

 1. reads the source lists out of app/src/main/cpp/CMakeLists.txt and verifies every file exists
 2. generates the build-time headers with gen_game_headers.py
 3. compiles every toolkit + game source with the same include path / definitions (host gcc, -Werror for
    implicit function declarations, which the NDK's clang rejects)
 4. lists every unresolved symbol and classifies it: libc, Android glue, recompiler output (expected until the
    user generates it) - anything else is a missing implementation and fails the check.
"""
import glob, os, re, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); cpp = os.path.normpath(os.path.join(here, '../app/src/main/cpp'))
repo = os.path.normpath(os.path.join(here, '../../..')); src = repo + '/src'; port = repo + '/ports/mercenaries'
cm = open(cpp + '/CMakeLists.txt').read(); work = tempfile.mkdtemp(); gen = work + '/game_gen'; objs = work + '/objs'; os.makedirs(objs)
subprocess.check_call([sys.executable, cpp + '/gen_game_headers.py', port, gen], stdout=subprocess.DEVNULL)

def g(pat, excl=None):
    fs = sorted(glob.glob(pat)); return [f for f in fs if not (excl and re.search(excl, f))]
toolkit = (g(src + '/kernel/*.c', r'(preview_log|mod_overlay)\.c$') + g(src + '/apu/*.c', r'apu_xaudio2\.c$') + g(src + '/nv2a/*.c', r'nv2a_pb_test\.c$')
           + g(src + '/d3d/*.c', r'd3d8_(gl|compiler)\.c$') + [src + '/platform/win32_compat.c']
           + [cpp + '/d3d11_gles/' + f for f in ('d3d11_gles.c', 'hlsl2glsl.c', 'd3d8_compiler_gles.c')] + [cpp + '/compat/winapi_extra.c', cpp + '/compat/bcrypt_sha256.c'])
m = re.search(r'foreach\(f ([^)]*)\)', cm, re.S); names = m.group(1).split()
game = [port + '/src/%s.c' % n for n in names]
missing = [f for f in toolkit + game if not os.path.exists(f)]
if missing: sys.exit('MISSING SOURCES: %s' % missing)
print('%d toolkit + %d game sources listed in CMakeLists.txt all exist' % (len(toolkit), len(game)))

inc = ['-I' + cpp + '/compat/include', '-I' + cpp + '/d3d11_gles/include'] + ['-I' + p for p in [cpp + '/compat', cpp + '/d3d11_gles', cpp + '/android', src, src + '/kernel', src + '/apu', src + '/nv2a', src + '/platform', src + '/input', src + '/audio', src + '/d3d', port + '/src', port + '/src/recomp', repo + '/include', gen]]
base = ['gcc', '-c', '-O0', '-w', '-Wimplicit-function-declaration', '-Werror=implicit-function-declaration', '-fwrapv', '-DXBOX_ANDROID=1', '-D_GNU_SOURCE', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', '-DXBOX_DEFAULT_THREAD_STACK=67108864', '-DMERC_HAVE_GAME=1'] + inc
fail = 0
for i, f in enumerate(toolkit + game):
    flags = ['-D__ANDROID__']
    if re.search(r'(apu|nv2a)_mmio_hook', f): flags.append('-D__aarch64__')     # AArch64-only paths
    if f.endswith('d3d11_gles.c'): flags = []                                    # includes NDK GL headers when __ANDROID__ is set
    r = subprocess.run(base + flags + [f, '-o', '%s/%03d_%s.o' % (objs, i, os.path.basename(f)[:-2])], capture_output=True, text=True)
    if r.returncode: fail += 1; print('COMPILE FAIL', f, '\n', r.stderr[:600])
if fail: sys.exit('%d files failed to compile' % fail)
print('all %d sources compiled' % len(toolkit + game))

o = sorted(glob.glob(objs + '/*.o'))
undef = set(); defd = set()
for f in o:
    for line in subprocess.run(['nm', f], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 2 and p[0] == 'U': undef.add(p[1])
        elif len(p) == 3 and p[1] in 'TDBRCWVSd': defd.add(p[2])
# the toolkit is a static archive: only objects reachable from the game/glue are linked, so compute reachability
byobj = {}
for f in o:
    d, u = set(), set()
    for line in subprocess.run(['nm', f], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 2 and p[0] == 'U': u.add(p[1])
        elif len(p) == 3 and p[1] in 'TDBRCWV': d.add(p[2])
    byobj[f] = (d, u)
glue = set()
for gf in glob.glob(cpp + '/android/*.c'):
    t = open(gf).read()
    glue |= set(re.findall(r'^(?:[A-Za-z_][\w \*]*?[ \*])([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{', t, re.M)) | set(re.findall(r'^(?:int|void|const float|atomic_int|extern int)\s+\**(g_\w+)', t, re.M))
glue |= {'g_android_surface_w', 'g_android_surface_h', 'g_android_rendering', 'g_android_paused', 'g_android_quit'}
roots = [f for f in o if os.path.basename(f)[4:-2] + '.c' in [os.path.basename(x) for x in game]]
needed = set(roots); work_set = list(roots); provided = {}
for f, (d, u) in byobj.items():
    for s in d: provided.setdefault(s, f)
while work_set:
    f = work_set.pop(); 
    for s in byobj[f][1]:
        d = provided.get(s)
        if d and d not in needed: needed.add(d); work_set.append(d)
unres = set()
for f in needed: unres |= {s for s in byobj[f][1] if s not in provided}
libc = set()
for lib in ('libc.so.6', 'libm.so.6'):
    for p in glob.glob('/lib/x86_64-linux-gnu/' + lib):
        libc |= {l.split()[-1].split('@')[0] for l in subprocess.run(['nm', '-D', p], capture_output=True, text=True).stdout.splitlines() if l.strip()}
gl = {l.split()[-1] for p in glob.glob('/usr/lib/x86_64-linux-gnu/libGLESv2.so.2') for l in subprocess.run(['nm', '-D', p], capture_output=True, text=True).stdout.splitlines() if l.strip()}
GEN = re.compile(r'^(sub_[0-9A-Fa-f]+|recomp_lookup|xbe_entry_point|recomp_init_.*|g_recomp_.*)$')
unexpected = sorted(s for s in unres if s not in libc and s not in gl and s not in glue and not GEN.match(s) and not s.startswith(('__', '_GLOBAL_OFFSET', 'GLOBAL')) and s not in ('atexit',))
print('linked toolkit objects: %d of %d; unresolved: %d (libc/GL/glue/recompiler output filtered)' % (len(needed) - len(roots), len(o) - len(roots), len(unres)))
if unexpected: print('UNEXPECTED UNRESOLVED SYMBOLS:', ' '.join(unexpected)); sys.exit(1)
print('OK: every unresolved symbol is libc, the Android glue, or recompiler output')
