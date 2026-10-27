#!/usr/bin/env python3
"""Generates the headers the game sources include at build time.

The Windows build produces these with three CMake scripts (Mission-Script-Fixes.cmake, Dev-Vehicle-Catalog.cmake,
Shader-Warmup.cmake). This script does the same job in Python so it can be run and verified on any host, and is
called from the Android CMake at configure time. Output is byte-for-byte what the CMake scripts write.

usage: gen_game_headers.py <ports/mercenaries dir> <output dir>
"""
import json, os, re, subprocess, sys

port, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)

def c_string_literal(text, wrap):
    # identical to the CMake string(REPLACE ...) chain; only Mission-Script-Fixes' WMD path also wraps the
    # result in quotes (mod_gameplay_script.h.in supplies its own).
    text = text.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '').replace('\n', '\\n"\n"')
    return '"' + text + '"' if wrap else text

def configure_file(template, dest, token, value):
    t = open(template, encoding='utf-8').read()
    open(dest, 'w', encoding='utf-8', newline='\n').write(t.replace('@%s@' % token, value))

for lua, tmpl, dest, token, wrap in (('wmd_inspectors.lua', 'wmd_inspectors_script.h.in', 'wmd_inspectors_script.h', 'WMD_FIX_SOURCE', True),
                                     ('mod_gameplay.lua', 'mod_gameplay_script.h.in', 'mod_gameplay_script.h', 'MOD_GAMEPLAY_SOURCE', False)):
    src = open(os.path.join(port, 'src', lua), encoding='utf-8').read()
    configure_file(os.path.join(port, 'src', tmpl), os.path.join(out, dest), token, c_string_literal(src, wrap))

def load(name):   # keep numbers as the exact text CMake's string(JSON GET) would return
    return json.load(open(os.path.join(port, 'data', name), encoding='utf-8'), parse_float=str, parse_int=str)

def esc(v): return str(v).replace('\\', '\\\\').replace('"', '\\"')
fields = 'name template faction kind template_hash model_hash radius bottom height half_width half_length'.split()

veh = '/* Generated from data/dev-vehicles.json. Do not edit. */\n'
for v in load('dev-vehicles.json')['vehicles']:
    if not re.match(r'^template_vehicles(_ch[1-4])?[.]lyr$', v['source']):
        sys.exit('Unexpected developer vehicle template layer: ' + v['source'])
    f = {k: esc(v[k]) for k in fields}; asset = re.sub(r'[.]lyr$', '', esc(v['source']))
    veh += '{"%s","%s","%s","%s",%su,%su,%s,%s,%s,%s,%s,"%s"},\n' % (f['name'], f['template'], f['faction'], f['kind'], f['template_hash'], f['model_hash'], f['radius'], f['bottom'], f['height'], f['half_width'], f['half_length'], asset)
open(os.path.join(out, 'dev_vehicle_catalog.inc'), 'w', encoding='utf-8', newline='\n').write(veh)

troop = '/* Generated from data/dev-troops.json. Do not edit. */\n'
for t in load('dev-troops.json')['troops']:
    f = {k: str(t[k]) for k in fields}
    troop += '{"%s","%s","%s","%s",%su,%su,%s,%s,%s,%s,%s,"template_humans"},\n' % (f['name'], f['template'], f['faction'], f['kind'], f['template_hash'], f['model_hash'], f['radius'], f['bottom'], f['height'], f['half_width'], f['half_length'])
open(os.path.join(out, 'dev_troop_catalog.inc'), 'w', encoding='utf-8', newline='\n').write(troop)

subprocess.check_call([sys.executable, os.path.join(port, 'scripts', 'Generate-Shader-Warmup.py'),
                       os.path.join(port, 'data', 'shader-warmup.json'), os.path.join(out, 'shader_warmup_catalog.inc')])
# ---- ps2_upgrades.rc -> arrays (Windows embeds these as RCDATA resources)
rc = open(os.path.join(port, 'resources', 'ps2_upgrades.rc'), encoding='utf-8', errors='replace').read()
entries = re.findall(r'^\s*(\d+)\s+RCDATA\s+"([^"]+)"', rc, re.M)
inc = '/* Generated from resources/ps2_upgrades.rc. Do not edit. */\n'
table = 'static const struct { unsigned id; const uint8_t *data; unsigned size; } k_ps2_resources[] = {\n'
for n, (rid, rel) in enumerate(entries):
    data = open(os.path.join(port, 'resources', rel), 'rb').read()
    inc += 'static const uint8_t k_ps2_res_%d[] = {%s};\n' % (n, ','.join(str(b) for b in data) if data else '0')
    table += '    { %s, k_ps2_res_%d, %du },\n' % (rid, n, len(data))
inc += table + '};\n'
open(os.path.join(out, 'ps2_upgrade_resources.inc'), 'w', newline='\n').write(inc)

print('generated', ', '.join(sorted(os.listdir(out))))
