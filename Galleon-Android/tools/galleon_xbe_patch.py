#!/usr/bin/env python3
"""Patch Galleon XBE: add boot-to-main-menu check at loc_00066AA2.

At file offset 0x56aa2 in Galleon xbe/default.xbe:
    84 9D 30 01 00 00    test byte ptr [ebp+0x130], bl
    74 0D                jz +0x0D

We replace these 8 bytes with:
    E9 <rel32>           jmp cave
    90 90 90             nop; nop; nop

In the cave (appended at end of XBE):
    A1 88 36 22 00       mov eax, [0x223688]     ; read save slot marker
    40                   inc eax                   ; marker == -1 (0xFFFFFFFF) -> 0 (ZF=1)
    74 <cold_boot>       jz cold_boot              ; if marker == -1, run original test+jz
    48                   dec eax                   ; restore marker
    A3 ... -> reset [0x223688] = -1, set [ebp+0x15C]=eax, [ebp+0x168]=0x217F64,
              [ebp+0xC60]=1, [ebp+0x10]=0, [ebp+0x1C]=0x800, [0x238ED4]=ebp
    E9 <rel32>           jmp 0x56aaa (after original test+jz)

cold_boot:
    84 9D 30 01 00 00    test byte ptr [ebp+0x130], bl
    74 <jz_target>       jz 0x56abd (original jz +0x0D target)
    E9 <rel32>           jmp 0x56aaa (fallthrough)
"""
import sys, os, struct

XBE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'Galleon xbe', 'default.xbe'))

with open(XBE_PATH, 'rb') as f:
    data = bytearray(f.read())

print(f"Patching {XBE_PATH} ({len(data)} bytes)")

offset = 0x56aa2
expected = bytes([0x84, 0x9D, 0x30, 0x01, 0x00, 0x00, 0x74, 0x0D])
actual = data[offset:offset+8]
if actual != expected:
    print(f"ERROR: Pattern mismatch at offset {offset:#x}: {actual.hex()}")
    sys.exit(1)

cave_off = len(data)
# jmp rel32 from offset to cave_off
jmp_to_cave = cave_off - (offset + 5)
data[offset:offset+8] = b'\xE9' + struct.pack('<i', jmp_to_cave) + b'\x90\x90\x90'

# Build cave
cave = bytearray()
cave += b'\xA1\x88\x36\x22\x00'              # mov eax, [0x223688]
cave += b'\x40'                              # inc eax
jz_pos = len(cave)
cave += b'\x74\x00'                          # jz cold_boot (placeholder)

# Warm boot (marker != -1):
cave += b'\x48'                              # dec eax (eax = marker)
cave += b'\x89\x85\x5C\x01\x00\x00'          # mov [ebp+0x15C], eax
cave += b'\xC7\x05\x88\x36\x22\x00\xFF\xFF\xFF\xFF'  # mov dword ptr [0x223688], -1
cave += b'\xC7\x85\x68\x01\x00\x00\x64\x7F\x21\x00'  # mov dword ptr [ebp+0x168], 0x217F64
cave += b'\xC6\x85\x60\x0C\x00\x00\x01'      # mov byte ptr [ebp+0xC60], 1
cave += b'\xC7\x45\x10\x00\x00\x00\x00'      # mov dword ptr [ebp+0x10], 0
cave += b'\xC7\x45\x1C\x00\x08\x00\x00'      # mov dword ptr [ebp+0x1C], 0x800
cave += b'\x89\x2D\xD4\x8E\x23\x00'          # mov [0x238ED4], ebp
# jmp 0x56aaa (fallthrough after original test+jz)
ret_off = cave_off + len(cave) + 5
cave += b'\xE9' + struct.pack('<i', 0x56aaa - ret_off)

# cold_boot:
cold_off = len(cave)
cave[jz_pos + 1] = cold_off - (jz_pos + 2)
cave += b'\x84\x9D\x30\x01\x00\x00'          # test byte ptr [ebp+0x130], bl
# jz 0x56abd (original jz +0x0D target): use 0F 84 rel32
jz_orig_off = cave_off + len(cave) + 6
cave += b'\x0F\x84' + struct.pack('<i', 0x56abd - jz_orig_off)
# jmp 0x56aaa
fall_off = cave_off + len(cave) + 5
cave += b'\xE9' + struct.pack('<i', 0x56aaa - fall_off)

data.extend(cave)
with open(XBE_PATH, 'wb') as f:
    f.write(data)

print(f"Patched successfully! Cave at {cave_off:#x} ({len(cave)} bytes), new size {len(data)} bytes.")
