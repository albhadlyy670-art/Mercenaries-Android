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
    kernel::Return(ctx, 0, 6);
}
static const bool kregister_IoQueryVolumeInformation =
    (kernel::RegisterExport(76, "IoQueryVolumeInformation", IoQueryVolumeInformationEntry), true);

}  // namespace game
