#!/usr/bin/env python3
"""Extracts the static HLSL shader strings embedded in the renderer's C sources into a directory."""
import os, re, sys, glob
root, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
tok = re.compile(r'"(?:[^"\\]|\\.)*"|/\*.*?\*/|//[^\n]*', re.S)
n = 0
for f in glob.glob(root + '/d3d/*.c') + glob.glob(root + '/nv2a/*.c'):
    src = open(f, errors='replace').read()
    for m in re.finditer(r'static\s+const\s+char\s+(\w+)\s*\[\s*\]\s*=\s*((?:\s*(?:"(?:[^"\\]|\\.)*"|/\*.*?\*/|//[^\n]*))+)\s*;', src, re.S):
        txt = ''.join(bytes(t[1:-1], 'utf-8').decode('unicode_escape') for t in tok.findall(m.group(2)) if t[0] == '"')
        if ('float4' in txt or 'cbuffer' in txt or 'SV_' in txt) and '%' not in txt.replace('%%', ''):
            open(f'{out}/{os.path.basename(f)[:-2]}__{m.group(1)}.hlsl', 'w').write(txt); n += 1
print(n, 'shaders extracted')
