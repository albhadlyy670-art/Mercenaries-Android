#!/usr/bin/env python3
"""Patch boot-to-main-menu into recompiled Galleon code.
Searches for references to the save-select marker [0x223688] or the main-menu
mode flag [0xC60] in generated code, and injects main-menu-mode setup logic.
"""
import re, sys, glob, os

repo = os.environ.get('GALLEON_REPO', '.')
gen_dir = os.path.join(repo, 'generated', 'game')
files = sorted(glob.glob(os.path.join(gen_dir, 'x86_recomp.*.cpp')))

TAB = '\t'
NL = '\n'

boot_code = (
    NL
    + TAB + '// v1.0.125: boot to main menu when marker != -1' + NL
    + TAB + '{ ctx.eax = (uint32_t)(X86_MEM32((uint32_t)(0x223688u))); }' + NL
    + TAB + '{ uint32_t a = (uint32_t)(ctx.eax), b = (uint32_t)(0xFFFFFFFFu); uint32_t r = (uint32_t)(a - b); ca = a; cb = b; cf = a < b; of = (uint32_t)(((a ^ b) & (a ^ r)) >> 31) & 1; zf = r == 0; sf = (uint32_t)(r >> 31) & 1; pf = X86_PARITY(r); }' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.eax); }  // v1.0.125: idx = marker' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x168u); X86_MEM32(ea) = (uint32_t)(0x217F64u); }  // v1.0.125: name = empty fallback' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }  // v1.0.125: c60=1 main-menu mode' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x10u); X86_MEM32(ea) = (uint32_t)(0x0u); }  // v1.0.125: phase = 0' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x800u); }  // v1.0.125: queue state 0x800 (main menu entry)' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(0x238ED4u); X86_MEM32(ea) = (uint32_t)(ctx.ebp); }  // v1.0.125: global menu obj' + NL
)

# Search patterns, most specific first
patterns = [
    r'0x223688',      # save-select marker address (referenced in original code)
    r'0xC60',         # main menu mode flag at [ebp+0xC60]
    r'sub_00068CD0',  # save slot create/refresh
    r'sub_00068C30',  # save existence check
    r'sub_00068F00',  # relaunch function (known to exist sometimes)
]

patched = 0
searched = 0
for fpath in files:
    with open(fpath, 'r') as f:
        src = f.read()

    if 'GALLEON-BOOTMENU-FIXES-068D10-125' in src:
        continue

    searched += 1
    found = None
    for pat in patterns:
        m = re.search(pat, src)
        if m:
            found = (pat, m)
            break

    if not found:
        print(f'  {os.path.basename(fpath)}: no pattern found')
        continue

    pat, m = found
    # Insert boot code right before the match
    start = m.start()
    src = src[:start] + boot_code + src[start:]
    # Add marker comment
    src = src[:start] + '  // [GALLEON-BOOTMENU-FIXES-068D10-125]' + src[start:]

    with open(fpath, 'w') as f:
        f.write(src)
    patched += 1
    print(f'  {os.path.basename(fpath)}: patched at pattern "{pat}"')

print(f'{NL}Searched {searched} file(s), patched {patched}.')
if patched == 0:
    print('ERROR: No boot-related code found in any generated file.')
    sys.exit(1)
