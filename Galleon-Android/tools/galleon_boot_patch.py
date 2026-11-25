#!/usr/bin/env python3
"""Patch boot-to-main-menu into recompiled Galleon code.
Searches for the crash guard site (ctx.esi + 0xC5Cu) in generated C++ files
and inserts a marker check before the esi==0 bail, so that after save-select
the game boots to main menu instead of save-select.
"""
import re, sys, glob, os

repo = os.environ.get('GALLEON_REPO', '.')
gen_dir = os.path.join(repo, 'generated', 'game')
files = sorted(glob.glob(os.path.join(gen_dir, 'x86_recomp.*.cpp')))

TAB = '\t'
NL = '\n'

# Boot-to-main-menu code: check marker [0x223688], if != -1 set up main menu mode and return
boot_code = (
    NL
    + TAB + '// v1.0.129: boot to main menu when marker != -1' + NL
    + TAB + '{ ctx.eax = (uint32_t)(X86_MEM32((uint32_t)(0x223688u))); }' + NL
    + TAB + '{ uint32_t a = (uint32_t)(ctx.eax), b = (uint32_t)(0xFFFFFFFFu); uint32_t r = (uint32_t)(a - b); ca = a; cb = b; cf = a < b; of = (uint32_t)(((a ^ b) & (a ^ r)) >> 31) & 1; zf = r == 0; sf = (uint32_t)(r >> 31) & 1; pf = X86_PARITY(r); }' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(0xFFFFFFFFu); }  // v1.0.129: reset marker' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.eax); }  // v1.0.129: idx = marker' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x168u); X86_MEM32(ea) = (uint32_t)(0x217F64u); }  // v1.0.129: name = empty fallback' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }  // v1.0.129: c60=1 main-menu mode' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x10u); X86_MEM32(ea) = (uint32_t)(0x0u); }  // v1.0.129: phase = 0' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(ctx.ebp + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x800u); }  // v1.0.129: queue state 0x800 (main menu entry)' + NL
    + TAB + 'if (!zf) { uint32_t ea = (uint32_t)(0x238ED4u); X86_MEM32(ea) = (uint32_t)(ctx.ebp); }  // v1.0.129: global menu obj' + NL
    + TAB + 'if (!zf) { ctx.esp += 4; return; }  // v1.0.129: skip crash, go to main menu' + NL
)

# Search for the crash guard line: ctx.esi + 0xC5Cu
pattern = r'ctx\.esi \s* \+ \s* 0xC5Cu'

patched = 0
searched = 0
for fpath in files:
    with open(fpath, 'r') as f:
        src = f.read()

    if 'GALLEON-BOOTMENU-FIXES-068D10-129' in src:
        continue

    searched += 1
    m = re.search(pattern, src)
    if not m:
        print(f'  {os.path.basename(fpath)}: pattern not found')
        continue

    # Insert boot code right before the crash guard line
    insert_pos = m.start()
    # Find the beginning of the line (go back to start of line or previous }
    line_start = src.rfind(NL, 0, insert_pos) + 1
    # Check if this line starts with a tab (it's inside a block)
    if line_start < len(src) and src[line_start] == TAB:
        # Insert before this line
        src = src[:line_start] + boot_code + src[line_start:]
    else:
        # Insert at the match position
        src = src[:insert_pos] + boot_code + src[insert_pos:]

    # Add marker comment near the pattern
    src = src.replace('GALLEON-BOOTMENU-FIXES-068D10-129', 'GALLEON-BOOTMENU-FIXES-068D10-129', 1)
    # Add the marker right before the crash guard comment line
    crash_guard_comment = '// [GALLEON-CRASH-GUARD] bail if esi is 0'
    if crash_guard_comment in src:
        src = src.replace(crash_guard_comment, '// [GALLEON-BOOTMENU-FIXES-068D10-129]\n' + crash_guard_comment, 1)

    with open(fpath, 'w') as f:
        f.write(src)
    patched += 1
    print(f'  {os.path.basename(fpath)}: patched')

print(f'{NL}Searched {searched} file(s), patched {patched}.')
if patched == 0:
    print('ERROR: No crash guard pattern found in any generated file.')
    sys.exit(1)
