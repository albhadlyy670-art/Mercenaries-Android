#!/usr/bin/env python3
"""Apply _g60 patch to x86_recomp.10.cpp: guard sub_00068D10 stale-data copy
and selected-slot zeroing when main-menu mode is active (c60==1).
Also patch sub_00065E00 to read name/slot from esi (the obj _g10 set up)
instead of from global [0x238ED4], so sub_00071730 builds main-menu items.
Patch 4: queue state 0x10 (menu frame) in sub_00068D10 after phase setup so
the main menu actually renders instead of black screen.
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
patch1_old = '''\t// 00068D55 call 0x00067EE0
\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);'''
patch1_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] skip stale save-select copy when main-menu
\t// mode is active (c60==1)
\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\t\t// 00068D55 call 0x00067EE0
\t\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);
\t}'''

# Patch 2: guard the [esi+0x15C] selected-slot zeroing — only zero when c60 != 1
patch2_old = '''\t// 00068D64 mov [esi+0x15C], ebx
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }'''
patch2_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] keep selected slot when main-menu mode is active (c60==1)
\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\t\t// 00068D64 mov [esi+0x15C], ebx
\t\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }
\t}'''

# Patch 3: sub_00065E00 reads name/slot from global [0x238ED4] but _g10 set
# them on the dispatcher obj (esi). Read from esi so sub_00071730 gets correct
# args and builds main-menu items at [obj+0xC5C].
patch3_old = '''\t// 00065E0F mov ecx, [eax+0x168]
\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\t// 00065E15 mov edx, [eax+0x15C]
\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''
patch3_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10-2] read name/slot from obj (esi), not global [0x238ED4]
\t// 00065E0F mov ecx, [esi+0x168]
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\t// 00065E15 mov edx, [esi+0x15C]
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''

count1 = content.count(patch1_old)
count2 = content.count(patch2_old)
count3 = content.count(patch3_old)

if count1 != 1:
    sys.stderr.write(f'Patch 1: expected 1 match, found {count1}\n')
    sys.exit(1)
if count2 != 1:
    sys.stderr.write(f'Patch 2: expected 1 match, found {count2}\n')
    sys.exit(1)
if count3 != 1:
    sys.stderr.write(f'Patch 3: expected 1 match, found {count3}\n')
    sys.exit(1)

# Patch 4: sub_00068D10 sets phase=1 ([esi+0x10]=ctx.eax) but may not queue
# state 0x10 (the menu frame handler). Without state 0x10, the menu never
# renders — black screen. Queue state 0x10 right after phase setup.
patch4_old = '''\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x10u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }'''
patch4_new = '''\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x10u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
// [GALLEON-LOADSAVE-FIXES-068D10-4] queue state 0x10 (menu frame) after menu entry setup
{ uint32_t ea = (uint32_t)(ctx.esi + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x10u); }'''

count4 = content.count(patch4_old)
if count4 == 1:
    content = content.replace(patch4_old, patch4_new, 1)
else:
    sys.stderr.write(f'Patch 4: expected 1 match, found {count4} (skipping - pattern may not match this build)\n')

# Patch 7: sub_00065E00 entry sets esi = eax (dispatcher's eax), but the
# dispatcher doesn't set eax to the menu obj -> esi=0 -> sub_00071730 crashes.
# Fix: after mov esi, eax, override esi with the menu obj from ebp (which
# the caller set to the menu obj, e.g. 0x2D2B4C in the crash log), falling
# back to the global [0x238ED4] that _g10 sets.
patch7_old = '''\t// 00065E01 mov esi, eax
\t{ ctx.esi = (uint32_t)(ctx.eax); }'''
patch7_new = '''\t// 00065E01 mov esi, eax
\t{ ctx.esi = (uint32_t)(ctx.eax); }
// [GALLEON-LOADSAVE-FIXES-068D10-7] set esi = menu obj (ebp, fallback to [0x238ED4])
\t{ uint32_t _g = X86_MEM32((uint32_t)(0x238ED4u)); ctx.esi = (uint32_t)(_g ? _g : ctx.ebp); }'''

count7 = content.count(patch7_old)
if count7 != 1:
    sys.stderr.write(f'Patch 7: expected 1 match, found {count7}\n')
    sys.exit(1)

# Patch 7b: sub_00065E00 passes &[esi+0xC5C] as first arg to sub_00071730.
# If esi is 0, this is &[0xC5C] (invalid). Use ebp (menu obj) instead.
patch7b_old = '''\t// 00065E1D push 0x01
\t{ uint32_t v = (uint32_t)(0x1u); X86_PUSH32(v); }
\t// 00065E1F lea eax, [esi+0xC5C]
\tctx.eax = (uint32_t)((uint32_t)(ctx.esi + 0xC5Cu));
\t// 00065E25 push eax
\t{ uint32_t v = (uint32_t)(ctx.eax); X86_PUSH32(v); }
\t// 00065E26 call 0x00071730
\tX86_PUSH32(0x65E2Bu); sub_00071730(ctx, base);'''

patch7b_new = '''\t// 00065E1D push 0x01
\t{ uint32_t v = (uint32_t)(0x1u); X86_PUSH32(v); }
// [GALLEON-LOADSAVE-FIXES-068D10-7b] use ebp (menu obj) instead of esi for sub_00071730 arg
\t{ uint32_t _esi = ctx.esi ? ctx.esi : ctx.ebp;
\t  ctx.eax = (uint32_t)((uint32_t)(_esi + 0xC5Cu)); }
\t// 00065E25 push eax
\t{ uint32_t v = (uint32_t)(ctx.eax); X86_PUSH32(v); }
\t// 00065E26 call 0x00071730
\tX86_PUSH32(0x65E2Bu); sub_00071730(ctx, base);'''

count7b = content.count(patch7b_old)
if count7b != 1:
    sys.stderr.write(f'Patch 7b: expected 1 match, found {count7b}\n')
    sys.exit(1)

# Patch 7c: sub_00065E00 writes [esi+0xC60]=1. Patch 7 now guarantees esi is
# non-zero (set from [0x238ED4] or ebp at entry), so writing to [esi+0xC60]
# is safe. ebp is NOT the menu obj at this point (it was 0x10 in the crash),
# so we must NOT use ebp here.
patch7c_old = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''

patch7c_new = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
// [GALLEON-LOADSAVE-FIXES-068D10-7c] esi is guaranteed non-zero by Patch 7
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''

count7c = content.count(patch7c_old)
if count7c != 1:
    sys.stderr.write(f'Patch 7c: expected 1 match, found {count7c}\n')
    sys.exit(1)

# Patch 7d: guard ALL sub_00071730(ctx, base) call sites in 10.cpp — if esi is
# 0 at any call site, set it from [0x238ED4] (global menu obj set by _g10) or
# ebp (caller's menu obj). This catches sub_00065520 (state 0x10 handler) and
# any other caller that doesn't set esi properly.
guard = '''// [GALLEON-LOADSAVE-FIXES-068D10-7d] ensure esi is valid before sub_00071730
\tif (ctx.esi == 0) {
\t\tuint32_t _g = X86_MEM32((uint32_t)(0x238ED4u));
\t\tctx.esi = (uint32_t)(_g ? _g : ctx.ebp);
\t}'''

count7d = content.count('sub_00071730(ctx, base);')
if count7d < 1:
    sys.stderr.write(f'Patch 7d: expected >=1 sub_00071730 calls, found {count7d}\n')
    sys.exit(1)

content = content.replace(patch1_old, patch1_new, 1)
content = content.replace(patch2_old, patch2_new, 1)
content = content.replace(patch3_old, patch3_new, 1)
content = content.replace(patch7_old, patch7_new, 1)
content = content.replace(patch7b_old, patch7b_new, 1)
content = content.replace(patch7c_old, patch7c_new, 1)
content = content.replace('sub_00071730(ctx, base);', guard + '\n\tsub_00071730(ctx, base);', count7d)

with open(path, 'w') as f:
    f.write(content)

sys.exit(0)
