#!/usr/bin/env python3
"""The Android shader crash guard is now part of d3d11_gles_b.inc.

Kept as a no-op for older build instructions. Patching the fault handler is
unsafe: guest MMIO intentionally raises SIGSEGV and is not a shader crash.
"""
print("shader crash guard is built in; no patch needed")
