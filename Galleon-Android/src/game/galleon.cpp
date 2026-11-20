// Galleon: what the host needs from this file. Black's game code was full of guest-address hooks (FOV, HUD, cheats,
// weapons, the in-game options menu); none of those addresses survive the hop to Galleon's image, so the features
// they implemented are absent, not emulated with Black's numbers. What remains is host-side.
#include <string>

#include <algorithm>
#include <iterator>

#include "common/options.h"

namespace game {

// Galleon's disc has not been checked for localized strings, so this build cannot tell a language the game has from
// one it does not: the LANGUAGE row offers the six text languages the port itself is translated into, and the game's
// own strings stay in English either way; only the panel's text follows it.
bool LanguageAvailable(const std::string& code) {
    return std::find(std::begin(kLanguages), std::end(kLanguages), code) != std::end(kLanguages);
}

}  // namespace game

// The one kernel export the image calls that the SDK does not provide, used on the volume the game mounts:
// ordinals 8 (DbgPrint) and 76 (IoQueryVolumeInformation) are the only imported kernel exports missing from the
// SDK, and DbgPrint is provided there out of band. Galleon asks it for FileFsAttributeInformation (class 5) and
// rejects anything but FATX or GDFX, so this answers FATX, the filesystem the host volume is treated as
// throughout (the same attributes and the same 42-character maximum the SDK's NtQueryVolumeInformationFile
// reports for class 5).
#include <cstring>

#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace game {

static void IoQueryVolumeInformationEntry(X86Context& ctx, uint8_t*) {
    kernel::Args a{ctx};
    const uint32_t cls = a[1];    // 5 is the only class this image asks for
    const uint32_t len = a[2];    // 0x20 at the call site
    const uint32_t out = a[3];    // FileFsAttributeInformation, or class-specific output otherwise
    const uint32_t iosb = a[4];   // IO_STATUS_BLOCK, may be null

    uint32_t info = 0;
    if (cls == 5) {
        const size_t n = std::min<size_t>(len, 24);
        if (out) std::memset(kernel::HostPtr(out), 0, n);
        if (out) {
            kernel::W32(out, 0);         // FileSystemAttributes: none
            kernel::W32(out + 4, 42);    // MaximumComponentNameLength
            kernel::W32(out + 8, 4);     // FileSystemNameLength, "FATX"
            std::memcpy(kernel::HostPtr(out + 12), "FATX", 4);
        }
        info = 16;
    } else {
        LOG_FIRST(8, "IoQueryVolumeInformation: class %u, asked once per class and answered with zeroes", cls);
        if (out && len) std::memset(kernel::HostPtr(out), 0, len);
    }
    if (iosb) {
        kernel::W32(iosb, 0);  // kSuccess
        kernel::W32(iosb + 4, info);
    }
    kernel::Return(ctx, 0, 5);  // the call site pushes five arguments (ret pops four of them)
}
static const bool kregister_IoQueryVolumeInformation =
    (kernel::RegisterExport(76, "IoQueryVolumeInformation", IoQueryVolumeInformationEntry), true);

// ObReferenceObjectByHandle (246). The SDK's handler binds a 32-byte body and stamps a serial at [+8]
// (Bind's WaitListHead bookkeeping). Galleon reads [+8] as the file object's Vpb pointer and
// dereferences it, so the serial (a small integer) becomes a bogus pointer and the game faults at
// that address. Bind the body the same way but point [+8] at a zeroed VPB instead: the guest reads
// Vpb->Flags (0) and Vpb+0x1c (0) and returns 0, which the caller stores as "no volume info" (the
// same value it uses when neither handle is valid). If the body was already bound by another export
// its [+8] still holds a serial, so point that at a zeroed VPB too; a serial is a small value and a
// guest address is not.
static void ObReferenceObjectByHandleEntry(X86Context& ctx, uint8_t*) {
    kernel::Args a{ctx};
    const uint32_t h = a[0];
    kernel::ObjectRef o = (h == 0xFFFFFFFE) ? kernel::CurrentRef() : kernel::FromHandle(h);
    if (!o) {
        kernel::Return(ctx, kernel::kInvalidHandle, 3);
        return;
    }
    if (!o->guest) {
        // 64 bytes: the 32-byte object body plus a 32-byte zeroed VPB behind it.
        const uint32_t body = mem::AllocSystem(64);
        std::memset(kernel::HostPtr(body), 0, 64);
        kernel::Bind(o, body);              // stamps [+8] serial, [+12] tag
        kernel::W32(body + 8, body + 32);   // [+8] = VPB, not the serial
    } else if (kernel::R32(o->guest + 8) < 0x10000) {
        const uint32_t vpb = mem::AllocSystem(32);
        std::memset(kernel::HostPtr(vpb), 0, 32);
        kernel::W32(o->guest + 8, vpb);
    }
    kernel::W32(a[2], o->guest);
    kernel::Return(ctx, kernel::kSuccess, 3);
}
static const bool kregister_ObReferenceObjectByHandle =
    (kernel::RegisterExport(246, "ObReferenceObjectByHandle", ObReferenceObjectByHandleEntry), true);

// NtUserIoApcDispatcher (232). The game registers this export's own kernel stub as the ApcRoutine of the
// asynchronous reads and writes it issues and passes its completion callback as the ApcContext, so the SDK
// finishing a transfer queues the stub and the stub lands here as (completion routine, iosb, reserved) —
// the ReadFileEx contract: the callback takes a Win32 error code and a byte count (none on a failure), and
// the iosb it is handed is the game's own request object (the callback sets the state at its [+0x24]).
// The pinned SDK predates the SDK's own implementation of this export (upstream commit 7afcc5c), so this
// mirrors that handler here, including its status-to-error mapping.
static uint32_t DosError(uint32_t status) {
    switch (status) {
        case kernel::kSuccess: return 0;
        case kernel::kPending: return 997;
        case kernel::kTimeout: return 1460;
        case kernel::kNoSuchFile:
        case kernel::kObjectNameNotFound: return 2;
        case kernel::kObjectPathNotFound: return 3;
        case kernel::kAccessDenied:
        case kernel::kCannotDelete:
        case kernel::kFileIsADirectory: return 5;
        case kernel::kInvalidHandle: return 6;
        case kernel::kNoMemory: return 8;
        case kernel::kNoMoreFiles: return 18;
        case kernel::kEndOfFile: return 38;
        case kernel::kNotSupported: return 50;
        case kernel::kInvalidParameter: return 87;
        case kernel::kNotImplemented: return 120;
        case kernel::kBufferTooSmall: return 122;
        case kernel::kObjectNameInvalid: return 123;
        case kernel::kObjectNameCollision: return 183;
        case kernel::kNotADirectory: return 267;
        case 0xC000007F: return 112;  // disk full
        case kernel::kDeviceNotConnected: return 1167;
        default: return (status & 0xC0000000) ? 317 : 0;
    }
}
static void NtUserIoApcDispatcherEntry(X86Context& ctx, uint8_t*) {
    kernel::Args a{ctx};
    const uint32_t fn = a[0];    // completion routine the game passed as the ApcContext
    const uint32_t iosb = a[1];  // IO_STATUS_BLOCK, the game's request object
    const uint32_t status = kernel::R32(iosb);
    const bool failed = status >= 0xC0000000;  // NT_ERROR
    const uint32_t err = failed ? DosError(status) : 0;
    if (failed) LOG_FIRST(16, "NtUserIoApcDispatcher: transfer failed, status %08X -> error %u", status, err);
    kernel::CallGuest(ctx, fn, {err, failed ? 0 : kernel::R32(iosb + 4), iosb});
    kernel::Return(ctx, 0, 3);  // the APC stub is stdcall with three arguments
}
static const bool kregister_NtUserIoApcDispatcher =
    (kernel::RegisterExport(232, "NtUserIoApcDispatcher", NtUserIoApcDispatcherEntry), true);

}  // namespace game
