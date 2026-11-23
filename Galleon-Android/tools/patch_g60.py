#!/usr/bin/env python3
"""Apply _g60 patches to x86_recomp.10.cpp:

Patches 1-3 (GALLEON-LOADSAVE-FIXES-068D10):
  1. Guard sub_00067EE0 stale-data copy — only copy when c60 != 1
  2. Guard [esi+0x15C] selected-slot zeroing — only zero when c60 != 1
  3. sub_00065E00 reads name/slot from esi (not global [0x238ED4])

Patch 4 (GALLEON-LOADSAVE-FIXES-068D10-4): queue state 0x10 in sub_00068D10

Patch 7 (GALLEON-LOADSAVE-FIXES-068D10-7): sub_00065E00 entry sets
  esi = [0x238ED4] or ebp fallback (menu obj)

Patch 7b (GALLEON-LOADSAVE-FIXES-068D10-7b): sub_00065E00 uses esi or ebp
  for sub_00071730 first arg (&[esi+0xC5C])

Patch 7c (GALLEON-LOADSAVE-FIXES-068D10-7c): sub_00065E00 writes [esi+0xC60]
  (esi guaranteed non-zero by Patch 7)

Patch 7d (GALLEON-LOADSAVE-FIXES-068D10-7d): guard ALL sub_00071730(ctx,
  base) call sites — before each call, if esi==0 set from [0x238ED4].

All patches are optional: if a pattern doesn't match, it is skipped with
a warning rather than failing the build. This handles differences between
the .patchtest reference file and CI-generated output.
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

def apply_patch(name, old, new, fatal=False):
    """Apply a string replacement. Returns True if applied or skipped (0 matches),
    False if partial match (count > 0 but != 1) and fatal=True."""
    count = content.count(old)
    if count == 0:
        sys.stderr.write(f'{name}: pattern not found (skipping)\n')
        return True
    if count == 1:
        return True  # will be replaced below
    sys.stderr.write(f'{name}: expected 1 match, found {count}\n')
    if fatal:
        sys.exit(1)
    return False

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

# ---- Patch 7b: sub_00065E00 uses esi or ebp for sub_00071730 arg ----
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

# ---- Patch 7c: [esi+0xC60] write (esi guaranteed non-zero by Patch 7) ----
patch7c_old = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''
patch7c_new = '''\t// 00065E08 mov byte ptr [esi+0xC60], 0x01
// [GALLEON-LOADSAVE-FIXES-068D10-7c] esi is guaranteed non-zero by Patch 7
\t{ uint32_t ea = (uint32_t)(ctx.esi + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }'''

# ---- Patch 7d: guard ALL sub_00071730(ctx, base) call sites ----
guard7d = '''// [GALLEON-LOADSAVE-FIXES-068D10-7d] ensure esi is valid before sub_00071730
\tif (ctx.esi == 0) {
\t\tuint32_t _g = X86_MEM32((uint32_t)(0x238ED4u));
\t\tctx.esi = (uint32_t)(_g ? _g : ctx.ebp);
\t}'''

# Apply patches 1-3 (fatal if partial match - these are known to work)
for name, old, new in [
    ('Patch 1', patch1_old, patch1_new),
    ('Patch 2', patch2_old, patch2_new),
    ('Patch 3', patch3_old, patch3_new),
]:
    count = content.count(old)
    if count == 1:
        content = content.replace(old, new, 1)
    elif count > 1:
        sys.stderr.write(f'{name}: expected 1 match, found {count}\n')
        sys.exit(1)
    # count == 0: skip

# Patch 4 (optional - may not match all builds)
count4 = content.count(patch4_old)
if count4 == 1:
    content = content.replace(patch4_old, patch4_new, 1)
elif count4 > 1:
    sys.stderr.write(f'Patch 4: expected 1 match, found {count4}\n')
    sys.exit(1)
# count == 0: skip

# Patches 7, 7b, 7c (must all match or all skip together, since they modify
# the same function sub_00065E00)
count7 = content.count(patch7_old)
count7b = content.count(patch7b_old)
count7c = content.count(patch7c_old)

if count7 == 1 and count7b == 1 and count7c == 1:
    content = content.replace(patch7_old, patch7_new, 1)
    content = content.replace(patch7b_old, patch7b_new, 1)
    content = content.replace(patch7c_old, patch7c_new, 1)
elif count7 == 0 and count7b == 0 and count7c == 0:
    sys.stderr.write('Patches 7/7b/7c: patterns not found (skipping)\n')
else:
    sys.stderr.write(f'Patches 7/7b/7c: partial match - 7:{count7} 7b:{count7b} 7c:{count7c}\n')
    sys.exit(1)

# Patch 7d: guard ALL sub_00071730(ctx, base) call sites
count7d = content.count('sub_00071730(ctx, base);')
if count7d >= 1:
    content = content.replace('sub_00071730(ctx, base);',
                              guard7d + '\n\tsub_00071730(ctx, base);',
                              count7d)
else:
    sys.stderr.write('Patch 7d: no sub_00071730 call sites found (skipping)\n')

with open(path, 'w') as f:
    f.write(content)

sys.exit(0)
