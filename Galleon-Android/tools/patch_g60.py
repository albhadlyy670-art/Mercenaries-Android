#!/usr/bin/env python3
"""Apply _g60 patch to x86_recomp.10.cpp: guard sub_00068D10 stale-data copy
and selected-slot zeroing when main-menu mode is active (c60==1).
Also patch sub_00065E00 to read name/slot from esi (the obj _g10 set up)
instead of from global [0x238ED4], so sub_00071730 builds main-menu items.
c60==1 is more reliable than [obj+0xC5C] because sub_00071730 reads name/slot
from the global menu obj which may not match the dispatcher's obj.
"""
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

# Patch 1: guard the sub_00067EE0 stale-data copy — only copy when c60 != 1
# (main-menu mode is NOT active). When c60==1, sub_00065E00 requested the
# main menu, so skip the stale save-select copy.
patch1_old = '''\\t// 00068D55 call 0x00067EE0
\\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);'''
patch1_new = '''\\t// [GALLEON-LOADSAVE-FIXES-068D10] skip stale save-select copy when main-menu
\\t// mode is active (c60==1)
\\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\\t\\t// 00068D55 call 0x00067EE0
\\t\\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);
\\t}'''

# Patch 2: guard the [esi+0x15C] selected-slot zeroing — only zero when
# c60 != 1 (main-menu mode is NOT active).
patch2_old = '''\\t// 00068D64 mov [esi+0x15C], ebx
\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }'''
patch2_new = '''\\t// [GALLEON-LOADSAVE-FIXES-068D10] keep selected slot when main-menu mode is active (c60==1)
\\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\\t\\t// 00068D64 mov [esi+0x15C], ebx
\\t\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }
\\t}'''

# Patch 3: sub_00065E00 reads name/slot from global [0x238ED4] but _g10 set
# them on the dispatcher obj (esi). Read from esi so sub_00071730 gets correct
# args and builds main-menu items at [obj+0xC5C].
patch3_old = '''\\t// 00065E0F mov ecx, [eax+0x168]
\\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\\t// 00065E15 mov edx, [eax+0x15C]
\\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''
patch3_new = '''\\t// [GALLEON-LOADSAVE-FIXES-068D10-2] read name/slot from obj (esi), not global [0x238ED4]
\\t// 00065E0F mov ecx, [esi+0x168]
\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\\t// 00065E15 mov edx, [esi+0x15C]
\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''

count1 = content.count(patch1_old)
count2 = content.count(patch2_old)
count3 = content.count(patch3_old)

if count1 != 1:
    sys.stderr.write(f'Patch 1: expected 1 match, found {count1}\\n')
    sys.exit(1)
if count2 != 1:
    sys.stderr.write(f'Patch 2: expected 1 match, found {count2}\\n')
    sys.exit(1)
if count3 != 1:
    sys.stderr.write(f'Patch 3: expected 1 match, found {count3}\\n')
    sys.exit(1)

content = content.replace(patch1_old, patch1_new, 1)
content = content.replace(patch2_old, patch2_new, 1)
content = content.replace(patch3_old, patch3_new, 1)

with open(path, 'w') as f:
    f.write(content)

sys.exit(0)
