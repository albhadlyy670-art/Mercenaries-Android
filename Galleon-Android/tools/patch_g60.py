#!/usr/bin/env python3
"""Apply _g60 patch to x86_recomp.10.cpp: guard sub_00068D10 stale-data copy
and selected-slot zeroing when main-menu items are already built at [obj+0xC5C],
so sub_00071730's main-menu items survive into the menu frame instead of being
wiped by stale save-select data from [obj+0xD48]."""
import sys

if len(sys.argv) < 2:
    sys.stderr.write("Usage: patch_g60.py <path_to_x86_recomp.10.cpp>\n")
    sys.exit(1)

path = sys.argv[1]

with open(path, 'r') as f:
    content = f.read()

# Already patched?
if 'GALLEON-LOADSAVE-FIXES-068D10' in content:
    sys.exit(0)

# Patch 1: guard the sub_00067EE0 stale-data copy — only copy when [obj+0xC5C]
# is empty (i.e. sub_00071730 has not yet built main-menu items there).
patch1_old = '''\t// 00068D55 call 0x00067EE0
\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);'''
patch1_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] skip stale save-select copy when main-menu
\t// items already built at [esi+0xC5C]
\tif (X86_MEM32((uint32_t)(ctx.esi + 0xC5Cu)) == 0) {
\t\t// 00068D55 call 0x00067EE0
\t\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);
\t}'''

# Patch 2: guard the [esi+0x15C] selected-slot zeroing — only zero when
# [obj+0xC5C] is empty (main-menu items not yet built there).
patch2_old = '''\t// 00068D64 mov [esi+0x15C], ebx
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }'''
patch2_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] keep selected slot when main-menu items exist
\tif (X86_MEM32((uint32_t)(ctx.esi + 0xC5Cu)) == 0) {
\t\t// 00068D64 mov [esi+0x15C], ebx
\t\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }
\t}'''

count1 = content.count(patch1_old)
count2 = content.count(patch2_old)

if count1 != 1:
    sys.stderr.write(f'Patch 1: expected 1 match, found {count1}\n')
    sys.exit(1)
if count2 != 1:
    sys.stderr.write(f'Patch 2: expected 1 match, found {count2}\n')
    sys.exit(1)

content = content.replace(patch1_old, patch1_new, 1)
content = content.replace(patch2_old, patch2_new, 1)

with open(path, 'w') as f:
    f.write(content)

sys.exit(0)
