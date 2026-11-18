#!/usr/bin/env python3
"""Legacy no-op. Do not patch shaders based on the old signal-stack crashes.

The Android MMIO handler now has a suitably sized per-thread signal stack.
Old shader blacklists were prone to skipping healthy shaders.
"""
print("obsolete shader skip patch disabled; using larger MMIO signal stacks")
