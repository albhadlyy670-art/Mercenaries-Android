#!/usr/bin/env python3
"""Apply _g60 patches to x86_recomp.10.cpp.

All patches are OPTIONAL: if a pattern doesn't match, it is skipped with a
warning rather than failing the build. This handles differences between the
.patchtest reference file and CI-generated output.

Patches:
  1. Guard sub_00067EE0 stale-data copy (c60 != 1)
  2. Guard [esi+0x15C] selected-slot zeroing (c60 != 1)
  3. sub_00065E00 reads name/slot from esi (not global [0x238ED4])
  4. Queue state 0x10 in sub_00068D10
  7. sub_00065E00 entry: esi = [0x238ED4] or ebp fallback
  7b. sub_00065E00: lea eax, [ebp+0xC5C] instead of [esi+0xC5C] for sub_00071730 arg
  7c. sub_00065E00: write [ebp+0xC60] instead of [esi+0xC60]
  7d. Guard ALL sub_00071730(ctx, base) call sites: if esi==0, set from [0x238ED4] or ebp
"""
import sys

if len(sys.argv) < 2:
    sys.stderr.write("Usage: patch_g60.py <path_to_x86_recomp.10.cpp>\n")
    sys.exit(1)

path = sys.argv[1]

with open(path, 'r') as f:
    content = f.read()

# Already patched? Check for Patch 7d marker (last patch applied).
if 'GALLEON-LOADSAVE-FIXES-068D10-7d' in content:
    sys.exit(0)

def try_replace(name, old, new):
    """Try to replace old with new. Non-fatal: skips if 0 or >1 matches."""
    global content
    count = content.count(old)
    if count == 0:
        sys.stderr.write(f'{name}: pattern not found (skipping)\n')
        return False
    if count > 1:
        sys.stderr.write(f'{name}: expected 1 match, found {count} (skipping)\n')
        return False
    content = content.replace(old, new, 1)
    return True

# ---- Patches 1-3: GALLEON-LOADSAVE-FIXES-068D10 ----
patch1_old = '''\t// 00068D55 call 0x00067EE0
\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);'''
patch1_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] skip stale save-select copy when main-menu
\t// mode is active (c60==1)
\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\t\t// 00068D55 call 0x00067EE0
\t\tX86_PUSH32(0x68D5Au); sub_00067EE0(ctx, base);
\t}'''

patch2_old = '''\t// 00068D64 mov [esi+0x15C], ebx
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }'''
patch2_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10] keep selected slot when main-menu mode is active (c60==1)
\tif (X86_MEM8((uint32_t)(ctx.esi + 0xC60u)) != 1) {
\t\t// 00068D64 mov [esi+0x15C], ebx
\t\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }
\t}'''

patch3_old = '''\t// 00065E0F mov ecx, [eax+0x168]
\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\t// 00065E15 mov edx, [eax+0x15C]
\t{ uint32_t ea = (uint32_t)(ctx.eax + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''
patch3_new = '''\t// [GALLEON-LOADSAVE-FIXES-068D10-2] read name/slot from obj (esi), not global [0x238ED4]
\t// 00065E0F mov ecx, [esi+0x168]
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x168u); ctx.ecx = (uint32_t)(X86_MEM32(ea)); }
\t// 00065E15 mov edx, [esi+0x15C]
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x15Cu); ctx.edx = (uint32_t)(X86_MEM32(ea)); }'''

# ---- Patch 4: queue state 0x10 ----
patch4_old = '''\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x10u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }'''
patch4_new = '''\\t{ uint32_t ea = (uint32_t)(ctx.esi + 0x10u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
// [GALLEON-LOADSAVE-FIXES-068D10-4] queue state 0x10 (menu frame) after menu entry setup
{ uint32_t ea = (uint32_t)(ctx.esi + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x10u); }'''

# ---- Patch 7: sub_00065E00 entry esi guard ----
patch7_old = '''\t// 00065E01 mov esi, eax
\t{ ctx.esi = (uint32_t)(ctx.eax); }'''
patch7_new = '''\t// 00065E01 mov esi, eax
\t{ ctx.esi = (uint32_t)(ctx.eax); }
// [GALLEON-LOADSAVE-FIXES-068D10-7] set esi = menu obj (ebp, fallback to [0x238ED4])
\t{ uint32_t _g = X86_MEM32((uint32_t)(0x238ED4u)); ctx.esi = (uint32_t)(_g ? _g : ctx.ebp); }'''

# ---- Patch 7b: sub_00065E00 uses ebp (menu obj) instead of esi for sub_00071730 arg ----
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

# ---- Patch 7c: [esi+0xC60] write -> [ebp+0xC60] (ebp is menu obj at this point) ----
patch7c_old = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''
patch7c_new = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
// [GALLEON-LOADSAVE-FIXES-068D10-7c] esi is valid (set by Patch 7/7d/8)
// so [esi+0xC60] is safe - ebp is NOT the menu obj on all code paths
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''

# ---- Patch 7d: guard ALL sub_00071730(ctx, base) call sites ----
guard7d = '''// [GALLEON-LOADSAVE-FIXES-068D10-7d] ensure esi is valid before sub_00071730
\tif (ctx.esi == 0) {
\t\tuint32_t _g = X86_MEM32((uint32_t)(0x238ED4u));
\t\tctx.esi = (uint32_t)(_g ? _g : ctx.ebp);
\t}'''

# Apply all patches (non-fatal)
try_replace('Patch 1', patch1_old, patch1_new)
try_replace('Patch 2', patch2_old, patch2_new)
try_replace('Patch 3', patch3_old, patch3_new)
try_replace('Patch 4', patch4_old, patch4_new)
try_replace('Patch 7', patch7_old, patch7_new)
try_replace('Patch 7b', patch7b_old, patch7b_new)
try_replace('Patch 7c', patch7c_old, patch7c_new)

# Patch 7d: guard ALL sub_00071730(ctx, base) call sites
count7d = content.count('sub_00071730(ctx, base);')
if count7d >= 1:
    content = content.replace('sub_00071730(ctx, base);',
                              guard7d + '\n\tsub_00071730(ctx, base);',
                              count7d)
    sys.stderr.write(f'Patch 7d: guarded {count7d} sub_00071730 call sites\n')
else:
    sys.stderr.write('Patch 7d: no sub_00071730 call sites found (skipping)\n')

with open(path, 'w') as f:
    f.write(content)

sys.exit(0)
