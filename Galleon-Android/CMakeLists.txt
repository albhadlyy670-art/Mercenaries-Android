# Galleon (original Xbox), statically recompiled, as an Android library: libmain.so, loaded by SDL's activity.
# The x86 code is recompiled on the PC first (build.bat): this build takes generated/game as it is, with the SDK's
# portable runtime, the game's sources in src and the Android host in native.

cmake_minimum_required(VERSION 3.22)
project(galleon_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR})
set(NATIVE ${CMAKE_CURRENT_SOURCE_DIR}/native)
set(GAME ${CMAKE_CURRENT_SOURCE_DIR}/src)  # the port's game-side sources: sound, Direct3D 8, the panel, the options

# the SDK: sdk/ in the repo, a checkout beside it, else downloaded
include(FetchContent)
if(EXISTS ${REPO}/sdk/cmake/XbeRecomp.cmake)
    set(SDK ${REPO}/sdk)
elseif(EXISTS ${REPO}/../XboxXBE-Recomp/cmake/XbeRecomp.cmake)
    get_filename_component(SDK ${REPO}/../XboxXBE-Recomp ABSOLUTE)
else()
    # pinned: the SDK's main moved to game code built into a DLL at first start (Windows), which this port's
    # recompiler step and libmain.so do not take; this is the commit v1.5.0 was built with
    FetchContent_Declare(xbe_sdk GIT_REPOSITORY https://github.com/TGP482/XboxXBE-Recomp.git
                         GIT_TAG 99078b119b5d5e0d1d6afb1390e29437c0c984c0)
    FetchContent_MakeAvailable(xbe_sdk)
    set(SDK ${xbe_sdk_SOURCE_DIR})
endif()

# Galleon fixes for the SDK's file layer, applied at configure time (idempotent, marker-checked):
#   - the profile save rotation renames Prefs.prf to the bare name Prefs.bak: a rename target that
#     does not resolve on its own is resolved within the source file's directory
#   - open on a file inside a missing folder fails with STATUS_OBJECT_PATH_NOT_FOUND as on the console
#   - the file operations and their outcome are logged so crash logs show the failing step
set(_fc ${SDK}/runtime/kernel/files.cpp)
file(READ ${_fc} _fc_src)
string(REPLACE [[        if (delete_on_close) {
            std::error_code ec;
            fs::remove(host, ec);
        }]] [[        if (delete_on_close) {
            std::error_code ec;
            LOGR("files: delete-on-close %s", host.string().c_str());
            fs::remove(host, ec);
        }]] _fc_src "${_fc_src}")
string(REPLACE [[        case 13:  // disposition
            f->delete_on_close = G<uint8_t>(in) != 0;]] [[        case 13:  // disposition
            f->delete_on_close = G<uint8_t>(in) != 0;
            LOGR("files: disposition %u on %s", f->delete_on_close ? 1u : 0u, f->host.string().c_str());]] _fc_src "${_fc_src}")
if(NOT _fc_src MATCHES "GALLEON-FILE-FIXES")
string(REPLACE [[Target Resolve(const std::string& xbox_path, uint32_t root_handle) {
    std::string p = Normalize(xbox_path);
    if (root_handle) {
        if (auto f = FileFromHandle(root_handle)) p = Normalize(f->xbox + "\\" + p);
    }]] [[Target Resolve(const std::string& xbox_path, uint32_t root_handle) {
    std::string p = Normalize(xbox_path);
    bool absolute = p.size() >= 2 && (p[1] == ':' || p.compare(0, 4, "\\??\\") == 0);
    if (root_handle && !absolute) {
        if (auto f = FileFromHandle(root_handle)) {
            if (!f->entry && !f->host.empty()) {
                // a bare name resolves within the root directory's own host folder (GALLEON-FILE-FIXES):
                // building an xbox path and re-resolving it landed the save reads on stale copies
                std::vector<std::string> parts;
                size_t s = 0;
                while (s < p.size()) {
                    size_t e = p.find('\\', s);
                    if (e == std::string::npos) e = p.size();
                    if (e > s) parts.push_back(p.substr(s, e - s));
                    s = e + 1;
                }
                return {MatchCase(f->host, parts).string()};
            }
            p = Normalize(f->xbox + "\\" + p);
        }
    }]] _fc_src "${_fc_src}")
string(REPLACE [[        case 10: {  // rename: ReplaceIfExists, RootDirectory, FileName (ANSI_STRING inline)
            uint32_t root = R32(in + 4);
            std::string target = ReadAnsiString(in + 8);
            std::string host = ResolvePath(target, root);
            if (host.empty()) return kObjectPathNotFound;
            if (f->fp) std::fflush(f->fp);
            fs::rename(f->host, host, ec);
            if (ec) return kAccessDenied;
            f->host = host;
            break;
        }]] [[        case 10: {  // rename: ReplaceIfExists, RootDirectory, FileName (ANSI_STRING inline)  (GALLEON-FILE-FIXES)
            uint32_t root = R32(in + 4);
            std::string target = ReadAnsiString(in + 8);
            std::string host = ResolvePath(target, root);
            if (host.empty()) {
                // a bare target like Prefs.bak renames within the source file's directory
                std::string dir = f->xbox;
                std::string::size_type p = dir.find_last_of("\\/");
                if (p != std::string::npos) host = ResolvePath(dir.substr(0, p + 1) + target, 0);
            }
            LOGR("files: rename %s -> '%s' => %s", f->host.c_str(), target.c_str(),
                 host.empty() ? "no mapping" : host.c_str());
            if (host.empty()) return kObjectPathNotFound;
            if (f->fp) std::fflush(f->fp);
            fs::rename(f->host, host, ec);
            if (ec) {
                LOGR("files: rename failed: %s", ec.message().c_str());
                return kAccessDenied;
            }
            f->host = host;
            break;
        }]] _fc_src "${_fc_src}")
string(REPLACE [[    if (host.empty() || !fs::exists(host, ec)) {
        LOG_FIRST(200, "missing %s", name.c_str());
        IoStatus(iosb, kObjectNameNotFound, 5);
        return host.empty() ? kObjectPathNotFound : kObjectNameNotFound;
    }]] [[    if (host.empty() || !fs::exists(host, ec)) {
        bool missing_path = host.empty() || !fs::is_directory(fs::path(host).parent_path(), ec);
        LOGR("files: open missing %s root %08x (%s) => %s", name.c_str(), root, missing_path ? "path" : "name", host.c_str());
        IoStatus(iosb, missing_path ? kObjectPathNotFound : kObjectNameNotFound, 5);
        return missing_path ? kObjectPathNotFound : kObjectNameNotFound;
    }]] _fc_src "${_fc_src}")
string(REPLACE [[    (void)options;
    W32(handle_out, NewHandle(f));
    IoStatus(iosb, kSuccess, 1);
    return kSuccess;]] [[    (void)options;
    W32(handle_out, NewHandle(f));
    LOGR("files: open ok %s root %08x => %s", name.c_str(), root, host.c_str());
    IoStatus(iosb, kSuccess, 1);
    return kSuccess;]] _fc_src "${_fc_src}")
string(REPLACE [[    while (out.size() > 1 && out.back() == '\\') out.pop_back();
    return out;
}]] [[    while (!out.empty() && out.back() == '\0') out.pop_back();
    while (out.size() > 1 && out.back() == '\\') out.pop_back();
    return out;
}]] _fc_src "${_fc_src}")
string(REPLACE [[    // a file in a missing folder fails as on the console: XAPI probes saves with OPEN_ALWAYS
    if (!exists && !fs::is_directory(fs::path(host).parent_path(), ec)) {]] [[    LOGR("files: open %s disp %u root %08x => %s", name.c_str(), disposition, root, host.c_str());
    // a file in a missing folder fails as on the console: XAPI probes saves with OPEN_ALWAYS
    if (!exists && !fs::is_directory(fs::path(host).parent_path(), ec)) {
        LOGR("files: create %s disp %u in a missing folder", name.c_str(), disposition);]] _fc_src "${_fc_src}")
string(REPLACE [[    f->pos = pos + n;
    if (write && n < len) return Finish(*f, 0xC000007F, n, event, apc, apc_ctx, iosb);  // disk full]] [[    f->pos = pos + n;
    if (write) LOGR("files: write %s %u bytes at %u => %08x", f->xbox.c_str(), n, (uint32_t)pos, n < len ? 0xC000007Fu : 0u);
    if (!write && n && f->xbox.size() >= 2 && (f->xbox[0] == 'U' || f->xbox[0] == 'u' || f->xbox[0] == 'T' || f->xbox[0] == 't')) LOGR("files: read %s %u bytes at %u", f->xbox.c_str(), n, (uint32_t)pos);
    if (write && n < len) return Finish(*f, 0xC000007F, n, event, apc, apc_ctx, iosb);  // disk full]] _fc_src "${_fc_src}")
string(REPLACE [[    if (!write && len && !n) return Finish(*f, kEndOfFile, 0, event, apc, apc_ctx, iosb);]] [[    if (!write && len && !n) LOGR("files: read eof %s %u bytes", f->xbox.c_str(), len);
    if (!write && len && !n) return Finish(*f, kEndOfFile, 0, event, apc, apc_ctx, iosb);]] _fc_src "${_fc_src}")
if(NOT _fc_src MATCHES "GALLEON-FILE-FIXES")
    message(FATAL_ERROR "the SDK's files.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_fc} "${_fc_src}")
endif()

# v1.0.13: log controller button changes so the crash logs show whether the game received the
# "Press A to continue" input (the dialog loop either waits forever or repeats its message)
set(_xapi ${SDK}/runtime/xdk/xapi.cpp)
file(READ ${_xapi} _xapi_src)
if(NOT _xapi_src MATCHES "GALLEON-XAPI-FIXES")
string(REPLACE [[#include "xdk/hle.h"]] [[#include "xdk/hle.h"
#include "core/log.h"]] _xapi_src "${_xapi_src}")
string(REPLACE [[    if (std::memcmp(&p, &h.last, sizeof(p))) h.packet++;
    h.last = p;]] [[    if (std::memcmp(&p, &h.last, sizeof(p))) h.packet++;
    if (p.buttons != h.last.buttons) LOGR("input: buttons %04x port %u", p.buttons, port);  // GALLEON-XAPI-FIXES
    h.last = p;]] _xapi_src "${_xapi_src}")
if(NOT _xapi_src MATCHES "GALLEON-XAPI-FIXES")
    message(FATAL_ERROR "xapi.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_xapi} "${_xapi_src}")
endif()

# v1.0.10: recompiled-code fix (GALLEON-RECOMP-FIXES): sub_001B5870 keeps its out-pointer in ebp across the
# profile-save call chain, but ebp comes back clobbered (0x10), so the final `mov [ebp],eax` writes to guest 0x10
# and faults (crash-6/7/8, host 0x..00000010). Snapshot arg5 at entry and use the snapshot for both stores.
set(_g53 ${REPO}/generated/game/x86_recomp.53.cpp)
file(READ ${_g53} _g53_src)
if(NOT _g53_src MATCHES "GALLEON-RECOMP-FIXES")
string(REPLACE [[#include "x86_recomp_shared.h"]] [[#include "x86_recomp_shared.h"
#include "core/log.h"]] _g53_src "${_g53_src}")
string(REPLACE [[	// 001B587B mov ebp, [esp+0x120]
	{ uint32_t ea = (uint32_t)(ctx.esp + 0x120u); ctx.ebp = (uint32_t)(X86_MEM32(ea)); }]] [[	// 001B587B mov ebp, [esp+0x120]
	{ uint32_t ea = (uint32_t)(ctx.esp + 0x120u); ctx.ebp = (uint32_t)(X86_MEM32(ea)); }
	const uint32_t galleon_out5 = ctx.ebp;  // GALLEON-RECOMP-FIXES: arg5 snapshot; callees may clobber ebp
	LOGR("save: build out=%08X a1=%08X a4=%08X", galleon_out5, X86_MEM32((uint32_t)(ctx.esp + 0x110u)), X86_MEM32((uint32_t)(ctx.esp + 0x11Cu)));]] _g53_src "${_g53_src}")
string(REPLACE [[	// 001B588F mov dword ptr [ebp], 0x00
	{ uint32_t ea = (uint32_t)(ctx.ebp); X86_MEM32(ea) = (uint32_t)(0x0u); }]] [[	// 001B588F mov dword ptr [ebp], 0x00
	{ uint32_t ea = (uint32_t)(galleon_out5); X86_MEM32(ea) = (uint32_t)(0x0u); }]] _g53_src "${_g53_src}")
string(REPLACE [[	// 001B58ED mov [ebp], eax
	{ uint32_t ea = (uint32_t)(ctx.ebp); X86_MEM32(ea) = (uint32_t)(ctx.eax); }]] [[	// 001B58ED mov [ebp], eax
	{ uint32_t ea = (uint32_t)(galleon_out5); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
	{ char galleon_name[24]; int galleon_i; for (galleon_i = 0; galleon_i < 23; galleon_i++) { galleon_name[galleon_i] = (char)X86_MEM8((uint32_t)(ctx.esp + 0x14u) + (uint32_t)galleon_i); if (!galleon_name[galleon_i]) break; } galleon_name[galleon_i] = 0; LOGR("save: obj=%08X -> out=%08X name=%s", ctx.eax, galleon_out5, galleon_name); }  // GALLEON-RECOMP-FIXES]] _g53_src "${_g53_src}")
if(NOT _g53_src MATCHES "GALLEON-RECOMP-FIXES")
    message(FATAL_ERROR "x86_recomp.53.cpp could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g53} "${_g53_src}")
endif()

# the recompiled code and the xbe it came from
set(XBE_GENERATED ${REPO}/generated/game CACHE PATH "the recompiled code of default.xbe")
set(XBE_IMAGE ${REPO}/assets/default.xbe CACHE FILEPATH "the xbe it was recompiled from")
set(GEN ${XBE_GENERATED})
set(XBE ${XBE_IMAGE})
if(NOT EXISTS ${GEN}/x86_func_mapping.cpp OR NOT EXISTS ${XBE})
    message(FATAL_ERROR "No recompiled code: put the game in assets/ or iso/ and run build.bat, it writes generated/game")
endif()
file(STRINGS ${REPO}/config/game.toml GAME_PARTITIONS REGEX "^partitions *=")
string(REGEX MATCH "[0-9]+" GAME_PARTITIONS "${GAME_PARTITIONS}")
math(EXPR last "${GAME_PARTITIONS} - 1")
set(GEN_SOURCES ${GEN}/x86_func_mapping.cpp)
foreach(i RANGE ${last})
    list(APPEND GEN_SOURCES ${GEN}/x86_recomp.${i}.cpp)
endforeach()
file(SHA1 ${XBE} xbe_sha1)
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/image_sha1.cpp CONTENT "extern const char kImageSha1[] = \"${xbe_sha1}\";\n")
# the build's version for the log: the commit when the project is in git, else unknown
set(BUILD_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_EXECUTABLE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD WORKING_DIRECTORY ${REPO}
                    OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE failed)
    if(NOT failed AND head)
        set(BUILD_COMMIT ${head})
    endif()
endif()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build_info.h CONTENT "#define GALLEON_COMMIT \"${BUILD_COMMIT}\"\n")

# dependencies: SDL3 from the checkout Gradle makes (its Java side goes into the APK), the rest fetched
set(SDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL)
if(NOT EXISTS ${SDL_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "SDL3 is missing from ${SDL_DIR}: the Gradle build clones it")
endif()
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
add_subdirectory(${SDL_DIR} ${CMAKE_CURRENT_BINARY_DIR}/SDL EXCLUDE_FROM_ALL)

set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git GIT_TAG 16.0.0 GIT_SHALLOW TRUE)
FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glslang tomlplusplus imgui)

# the SDK's runtime but its Windows parts, and those the game replaces (src/sound, src/d3d)
file(GLOB RUNTIME_SOURCES ${SDK}/runtime/*/*.cpp)
list(FILTER RUNTIME_SOURCES EXCLUDE REGEX
     "/runtime/app/|/core/input\\.cpp$|/gpu/(d3d12|render_thread)\\.cpp$|/audio/xaudio2\\.cpp$|/xdk/(dsound|d3d8)\\.cpp$")
# the host's game-side sources: its sound and Direct3D 8 reimplementations, the settings panel and the port's options
file(GLOB GAME_HOST_SOURCES CONFIGURE_DEPENDS ${GAME}/*/*.cpp)
file(GLOB ANDROID_SOURCES CONFIGURE_DEPENDS ${NATIVE}/*.cpp)
set(IMGUI_SOURCES ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

add_library(main SHARED ${RUNTIME_SOURCES} ${GAME_HOST_SOURCES} ${ANDROID_SOURCES} ${GEN_SOURCES} ${IMGUI_SOURCES}
            ${CMAKE_CURRENT_BINARY_DIR}/image_sha1.cpp)
# compat/ for the SDK's x86 header, the SDK harness's for the Winsock and DXGI names its portable runtime uses
target_include_directories(main PRIVATE ${NATIVE}/compat ${SDK}/harness/compat ${NATIVE} ${GAME} ${SDK}/runtime ${GEN}
                           ${CMAKE_CURRENT_BINARY_DIR} ${glslang_SOURCE_DIR})
target_include_directories(main SYSTEM PRIVATE ${imgui_SOURCE_DIR})
# x86 chars are signed, as the game's code and the runtime were written for
target_compile_options(main PRIVATE -fno-strict-aliasing -fsigned-char)
set(HOST_SOURCES ${RUNTIME_SOURCES} ${GAME_HOST_SOURCES} ${ANDROID_SOURCES})
set_source_files_properties(${HOST_SOURCES} PROPERTIES COMPILE_OPTIONS "-include;${NATIVE}/compat/crt.h")
set_source_files_properties(${GEN_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-O2;-w;-fno-strict-aliasing;-fsigned-char")
set_source_files_properties(${NATIVE}/vk_shader.cpp PROPERTIES COMPILE_DEFINITIONS ENABLE_HLSL)
target_link_libraries(main PRIVATE SDL3::SDL3 tomlplusplus::tomlplusplus glslang glslang-default-resource-limits
                      vulkan android log dl)
# 16 KB pages, as Android 15 devices may have
target_link_options(main PRIVATE -Wl,-z,max-page-size=16384)
