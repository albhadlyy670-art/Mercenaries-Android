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
    if (!write && n && ((f->xbox.size() >= 2 && (f->xbox[0] == 'U' || f->xbox[0] == 'u' || f->xbox[0] == 'T' || f->xbox[0] == 't')) ||
                        f->xbox.find("Prefs") != std::string::npos || f->xbox.find("SaveMeta") != std::string::npos ||
                        f->xbox.find("saveimage") != std::string::npos)) LOGR("files: read %s %u bytes at %u", f->xbox.c_str(), n, (uint32_t)pos);
    if (write && n < len) return Finish(*f, 0xC000007F, n, event, apc, apc_ctx, iosb);  // disk full]] _fc_src "${_fc_src}")
string(REPLACE [[    if (!write && len && !n) return Finish(*f, kEndOfFile, 0, event, apc, apc_ctx, iosb);]] [[    if (!write && len && !n) LOGR("files: read eof %s %u bytes", f->xbox.c_str(), len);
    if (!write && len && !n) return Finish(*f, kEndOfFile, 0, event, apc, apc_ctx, iosb);]] _fc_src "${_fc_src}")
# v1.0.15: every IRP completes its event like the console's, on the failure paths too. The v1.0.14
# freeze parked on `wait: forever on B0001000` right after the save: the save-slot listing's last
# NtQueryDirectoryFile returned STATUS_NO_MORE_FILES without signalling its event, and a wait on it
# (or on an ioctl's) never woke.
string(REPLACE [[    auto f = FileFromHandle(h);
    if (!f || (!f->fp && !f->entry)) return kInvalidHandle;
    if (write && f->entry) return kAccessDenied;
    ResetEvent(*f);]] [[    auto f = FileFromHandle(h);
    if (!f || (!f->fp && !f->entry)) {  // GALLEON-FILE-FIXES: a failed IRP completes like the console's
        IoStatus(iosb, kInvalidHandle, 0);
        Complete(event, apc, apc_ctx, iosb);
        return kInvalidHandle;
    }
    if (write && f->entry) {
        IoStatus(iosb, kAccessDenied, 0);
        SetEvent(*f);
        Complete(event, apc, apc_ctx, iosb);
        return kAccessDenied;
    }
    ResetEvent(*f);]] _fc_src "${_fc_src}")
string(REPLACE [[    auto f = FileFromHandle(h);
    if (!f || !f->dir) return kInvalidHandle;
    if (cls != 1) {
        LOG_FIRST(8, "NtQueryDirectoryFile: class %u", cls);
        return kInvalidParameter;
    }]] [[    auto f = FileFromHandle(h);
    if (!f || !f->dir) {  // GALLEON-FILE-FIXES: a failed IRP completes like the console's
        IoStatus(iosb, kInvalidHandle, 0);
        Complete(event, apc, apc_ctx, iosb);
        return kInvalidHandle;
    }
    if (cls != 1) {
        LOG_FIRST(8, "NtQueryDirectoryFile: class %u", cls);
        IoStatus(iosb, kInvalidParameter, 0);
        Complete(event, apc, apc_ctx, iosb);
        return kInvalidParameter;
    }]] _fc_src "${_fc_src}")
string(REPLACE [[    if (f->next >= (f->entry ? f->disc_entries.size() : f->entries.size())) {
        IoStatus(iosb, kNoMoreFiles, 0);
        return f->next == 0 ? kNoSuchFile : kNoMoreFiles;
    }]] [[    if (f->next >= (f->entry ? f->disc_entries.size() : f->entries.size())) {
        // GALLEON-FILE-FIXES: the end of the listing completes like every other IRP; without the
        // event's signal a save listing that waits on it never wakes (the v1.0.14 freeze)
        uint32_t st = f->next == 0 ? kNoSuchFile : kNoMoreFiles;
        IoStatus(iosb, st, 0);
        LOGR("files: dir end %s => %08X", f->xbox.c_str(), st);
        Complete(event, apc, apc_ctx, iosb);
        return st;
    }]] _fc_src "${_fc_src}")
string(REPLACE [[    if (len < need) return kBufferTooSmall;]] [[    if (len < need) {  // GALLEON-FILE-FIXES: a failed IRP completes like the console's
        IoStatus(iosb, kBufferTooSmall, 0);
        Complete(event, apc, apc_ctx, iosb);
        return kBufferTooSmall;
    }]] _fc_src "${_fc_src}")
string(REPLACE [[KERNEL_EXPORT(200, NtFsControlFile, 10) {
    IoStatus(a[4], kSuccess, 0);
    return kSuccess;
}]] [[KERNEL_EXPORT(200, NtFsControlFile, 10) {
    // GALLEON-FILE-FIXES: the IRP completes: without the event's signal a wait on it never wakes
    { static unsigned g_n = 0; if (g_n++ < 40) LOGR("files: fsctl code %08X event %08X", a[5], a[1]); }
    IoStatus(a[4], kSuccess, 0);
    Complete(a[1], a[2], a[3], a[4]);
    return kSuccess;
}]] _fc_src "${_fc_src}")
string(REPLACE [[    if (code == 0x70000 && out_len >= 0x18) {  // IOCTL_DISK_GET_DRIVE_GEOMETRY
        W64(out, 1000);
        W32(out + 8, 12);
        W32(out + 12, 255);
        W32(out + 16, 63);
        W32(out + 20, 512);
    }
    IoStatus(iosb, kSuccess, out_len);
    return kSuccess;
}]] [[    if (code == 0x70000 && out_len >= 0x18) {  // IOCTL_DISK_GET_DRIVE_GEOMETRY
        W64(out, 1000);
        W32(out + 8, 12);
        W32(out + 12, 255);
        W32(out + 16, 63);
        W32(out + 20, 512);
    }
    IoStatus(iosb, kSuccess, out_len);
    // GALLEON-FILE-FIXES: the IRP completes like the console's, event and APC included
    { static unsigned g_n = 0; if (g_n++ < 40) LOGR("files: ioctl code %08X event %08X", code, a[1]); }
    Complete(a[1], a[2], a[3], iosb);
    return kSuccess;
}]] _fc_src "${_fc_src}")
if(NOT _fc_src MATCHES "GALLEON-FILE-FIXES")
    message(FATAL_ERROR "the SDK's files.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_fc} "${_fc_src}")
endif()

# v1.0.14: the input chain end to end. v1.0.13's save-select freeze logged no button changes at
# all, so either the game never polls the pads or the host never sees the taps: the XAPI HLEs now
# log what the game asks for and what it gets (first 40 polls each), and port changes as they land.
set(_xapi ${SDK}/runtime/xdk/xapi.cpp)
file(READ ${_xapi} _xapi_src)
if(NOT _xapi_src MATCHES "GALLEON-XAPI-FIXES")
string(REPLACE [[#include "xdk/hle.h"]] [[#include "xdk/hle.h"
#include "core/log.h"]] _xapi_src "${_xapi_src}")
string(REPLACE [[        LOGI("xapi: controller ports now %X", cur);]] [[        LOGR("xapi: controller ports now %X", cur);  // GALLEON-XAPI-FIXES]] _xapi_src "${_xapi_src}")
string(REPLACE [[    UpdatePorts();
    if (!g_poller) {]] [[    UpdatePorts();
    LOGR("input: XInitDevices ports %08X", R32(kGamepadType + XDK_XPP_Current));  // GALLEON-XAPI-FIXES
    if (!g_poller) {]] _xapi_src "${_xapi_src}")
string(REPLACE [[    if (type != kGamepadType || port > 3) return 0;]] [[    if (type != kGamepadType || port > 3) {  // GALLEON-XAPI-FIXES
        LOGR("input: XInputOpen reject type %08X port %u", type, port);
        return 0;
    }]] _xapi_src "${_xapi_src}")
string(REPLACE [[    g_handles[h] = {port, 0, {}};
    return h;]] [[    g_handles[h] = {port, 0, {}};
    LOGR("input: XInputOpen type %08X port %u => %08X", type, port, h);  // GALLEON-XAPI-FIXES
    return h;]] _xapi_src "${_xapi_src}")
string(REPLACE [[HLE_STDCALL(XInputGetState, 2) {
    uint32_t port;
    {
        std::lock_guard<std::mutex> lock(g_ports_lock);
        auto it = g_handles.find(a[0]);
        if (it == g_handles.end()) return 6;  // ERROR_INVALID_HANDLE
        port = it->second.port;
    }
    input::Pad p = input::Poll(int(port));
    if (!p.connected) return 1167;  // ERROR_DEVICE_NOT_CONNECTED]] [[HLE_STDCALL(XInputGetState, 2) {
    static std::atomic<unsigned> g_sn{0};  // GALLEON-XAPI-FIXES
    uint32_t port;
    {
        std::lock_guard<std::mutex> lock(g_ports_lock);
        auto it = g_handles.find(a[0]);
        if (it == g_handles.end()) {
            if (g_sn.fetch_add(1) < 40) LOGR("input: GetState bad handle %08X", a[0]);
            return 6;  // ERROR_INVALID_HANDLE
        }
        port = it->second.port;
    }
    input::Pad p = input::Poll(int(port));
    if (!p.connected) {
        if (g_sn.fetch_add(1) < 40) LOGR("input: GetState disconnected port %u", port);
        return 1167;  // ERROR_DEVICE_NOT_CONNECTED
    }
    if (g_sn.fetch_add(1) < 40) LOGR("input: GetState port %u btn %04x ana %02x%02x", port, p.buttons, p.analog[0], p.analog[1]);  // GALLEON-XAPI-FIXES]] _xapi_src "${_xapi_src}")
string(REPLACE [[    if (std::memcmp(&p, &h.last, sizeof(p))) h.packet++;
    h.last = p;]] [[    if (std::memcmp(&p, &h.last, sizeof(p))) h.packet++;
    if (p.buttons != h.last.buttons || std::memcmp(p.analog, h.last.analog, 8)) {  // GALLEON-XAPI-FIXES
        static unsigned g_bn = 0;
        if (g_bn++ < 200) LOGR("input: pad %04x %02x%02x%02x%02x%02x%02x%02x%02x port %u", p.buttons, p.analog[0],
                               p.analog[1], p.analog[2], p.analog[3], p.analog[4], p.analog[5], p.analog[6],
                               p.analog[7], port);
    }
    h.last = p;]] _xapi_src "${_xapi_src}")
string(REPLACE [[    input::Rumble(int(port), R16(fb + 66), R16(fb + 68));]] [[    { static std::atomic<unsigned> g_rn{0}; if (g_rn.fetch_add(1) < 40) LOGR("input: SetState port %u mot %04X %04X ev %08X", port, R16(fb + 66), R16(fb + 68), R32(fb + 4)); }  // GALLEON-XAPI-FIXES
    input::Rumble(int(port), R16(fb + 66), R16(fb + 68));]] _xapi_src "${_xapi_src}")
if(NOT _xapi_src MATCHES "GALLEON-XAPI-FIXES")
    message(FATAL_ERROR "xapi.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_xapi} "${_xapi_src}")
endif()

# v1.0.14: log the waits that drag past 300ms and the ones that never come back: a hung frame's
# last lines name the object the game is parked on (a dialog waiting for input waits forever)
set(_obj ${SDK}/runtime/kernel/objects.cpp)
file(READ ${_obj} _obj_src)
if(NOT _obj_src MATCHES "GALLEON-WAIT-FIXES")
string(REPLACE [[uint32_t Wait(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout, bool alertable) {
    SuspendPoint();
    uint32_t r = WaitFor(objs, wait_all, timeout, alertable);
    SuspendPoint();
    return r;
}]] [[uint32_t Wait(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout, bool alertable) {
    SuspendPoint();
    // GALLEON-WAIT-FIXES: a wait that never ends (a dialog parked on input) and one that drags
    // (a slow save) both show up in the log with the object's guest address
    static std::mutex g_wait_log_lock;
    static std::map<uint32_t, int> g_wait_forever;
    static int g_wait_long = 0;
    uint32_t guest = objs.empty() || !objs[0] ? 0 : objs[0]->guest;
    if (timeout == INT64_MAX) {
        std::lock_guard<std::mutex> lk(g_wait_log_lock);
        int& n = g_wait_forever[guest];
        if (n < 3) {
            n++;
            uint32_t galleon_caller = 0;  // GALLEON-WAIT-FIXES v1.0.17: the guest call site that parked here
            if (Thread* me = Current()) if (me->ctx) galleon_caller = R32(me->ctx->esp);
            static const char* tn[] = {"event", "semaphore", "mutant", "timer", "thread", "file", "symlink", "device", "queue", "other"};
            LOGR("wait: forever on %08X type %s sig %d caller %08X", guest,
                 objs.empty() || !objs[0] || unsigned(objs[0]->type) >= 10 ? "?" : tn[unsigned(objs[0]->type)],
                 objs.empty() || !objs[0] ? 0 : objs[0]->signal, galleon_caller);
        }
    }
    auto t0 = std::chrono::steady_clock::now();
    uint32_t r = WaitFor(objs, wait_all, timeout, alertable);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    if (timeout == INT64_MAX) {  // GALLEON-WAIT-FIXES v1.0.16: how the forever-waits end (the join at the save flow)
        std::lock_guard<std::mutex> lk(g_wait_log_lock);
        static std::map<uint32_t, int> g_wait_done;
        int& n = g_wait_done[guest];
        if (n < 4) {
            n++;
            LOGR("wait: done on %08X r=%u after %dms", guest, r, int(ms));
        }
    }
    if (ms > 300) {
        std::lock_guard<std::mutex> lk(g_wait_log_lock);
        if (g_wait_long < 60) {
            g_wait_long++;
            static const char* tn[] = {"event", "semaphore", "mutant", "timer", "thread", "file", "symlink", "device", "queue", "other"};
            LOGR("wait: %dms on %08X type %s r=%u", int(ms), guest,
                 objs.empty() || !objs[0] || unsigned(objs[0]->type) >= 10 ? "?" : tn[unsigned(objs[0]->type)], r);
        }
    }
    SuspendPoint();
    return r;
}]] _obj_src "${_obj_src}")
if(NOT _obj_src MATCHES "GALLEON-WAIT-FIXES")
    message(FATAL_ERROR "objects.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_obj} "${_obj_src}")
endif()

# v1.0.16: thread lifecycles in the log (create/start/exit/parked) and the host-thread exit fix:
# StartHostThread's lambda returned without MarkExited, so a host thread that ended (audio, IO worker)
# left its ethread unsignalled and a join on it waited forever
set(_thr ${SDK}/runtime/kernel/threads.cpp)
file(READ ${_thr} _thr_src)
if(NOT _thr_src MATCHES "GALLEON-THREAD-FIXES")
string(REPLACE [[#include "kernel/kernel.h"]] [[#include "kernel/kernel.h"
#include "core/log.h"]] _thr_src "${_thr_src}")
string(REPLACE [[    if (suspended) t->suspend = 1;]] [[    if (suspended) t->suspend = 1;  // GALLEON-THREAD-FIXES
    LOGR("thread: create %X start=%08X ctx=%08X susp=%d stack=%08X ethread=%08X", t->id, start_routine, start_context,
         suspended ? 1 : 0, t->stack_top, t->ethread);]] _thr_src "${_thr_src}")
string(REPLACE [[void MarkExited(Thread& t, uint32_t code) {
    t.exit_code = code;]] [[void MarkExited(Thread& t, uint32_t code) {  // GALLEON-THREAD-FIXES
    LOGR("thread: %X exit code=%08X", t.id, code);
    t.exit_code = code;]] _thr_src "${_thr_src}")
string(REPLACE [[        LOGI("thread %X starts at %08X (%08X, %08X)", t->id, si.system_routine ? si.system_routine : si.start_routine,
             si.start_routine, si.start_context);]] [[        LOGR("thread: %X starts at %08X (%08X, %08X) susp=%d", t->id,  // GALLEON-THREAD-FIXES
             si.system_routine ? si.system_routine : si.start_routine, si.start_routine, si.start_context,
             t->suspend.load());]] _thr_src "${_thr_src}")
string(REPLACE [[        LOGI("thread %X returned %08X", t->id, code);]] [[        LOGR("thread: %X returned %08X", t->id, code);  // GALLEON-THREAD-FIXES]] _thr_src "${_thr_src}")
string(REPLACE [[        fn(ctx);
        t_current.reset();]] [[        fn(ctx);
        MarkExited(*t, 0);  // GALLEON-THREAD-FIXES: signal the ethread so joins on host threads end
        Unregister(t.get());
        t_current.reset();]] _thr_src "${_thr_src}")
string(REPLACE [[        LOGI("thread %X exits with %08X", t->id, code);]] [[        LOGR("thread: %X exits with %08X", t->id, code);  // GALLEON-THREAD-FIXES]] _thr_src "${_thr_src}")
if(NOT _thr_src MATCHES "GALLEON-THREAD-FIXES")
    message(FATAL_ERROR "threads.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_thr} "${_thr_src}")
endif()

# v1.0.17: the game's exit paths leave visible traces (GALLEON-EXIT-FIXES): after the save flow the app
# died with no signal, no Exit and an empty crash file — only an unlogged _Exit or an outside SIGKILL can
# do that. HalReturnToFirmware's story was LOGI (release-invisible) and Relaunch's execl-failure _Exit
# logged nothing at all; both speak now.
set(_krnl ${SDK}/runtime/kernel/xboxkrnl.cpp)
file(READ ${_krnl} _krnl_src)
if(NOT _krnl_src MATCHES "GALLEON-EXIT-FIXES")
string(REPLACE [[LOGI("HalReturnToFirmware(%u): the title asked to leave", a[0]);]] [[LOGR("game: HalReturnToFirmware(%u): the title asked to leave", a[0]);  // GALLEON-EXIT-FIXES]] _krnl_src "${_krnl_src}")
string(REPLACE [[LOGI("relaunching into %s", path.c_str());]] [[LOGR("game: relaunching into %s", path.c_str());  // GALLEON-EXIT-FIXES]] _krnl_src "${_krnl_src}")
string(REPLACE [[            if (FILE* f = std::fopen(g_launch_file.c_str(), "wb")) {]] [[            LOGR("boot: saving launch page: type %u title %08X data0 %08X", R32(page), R32(page + 4), R32(page + 0x400));  // GALLEON-EXIT-FIXES v1.0.20
            if (FILE* f = std::fopen(g_launch_file.c_str(), "wb")) {]] _krnl_src "${_krnl_src}")
if(NOT _krnl_src MATCHES "GALLEON-EXIT-FIXES")
    message(FATAL_ERROR "xboxkrnl.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_krnl} "${_krnl_src}")
endif()
set(_boot ${SDK}/runtime/kernel/boot.cpp)
file(READ ${_boot} _boot_src)
if(NOT _boot_src MATCHES "GALLEON-EXIT-FIXES")
string(REPLACE [[    execl(path.c_str(), path.c_str(), nullptr);
    std::_Exit(0);]] [[    {  // GALLEON-EXIT-FIXES v1.0.18: Android cannot execl an app restart — the hook restarts the activity
        bool GalleonRelaunchHook(const char*);
        if (GalleonRelaunchHook(exe)) std::_Exit(0);
    }
    execl(path.c_str(), path.c_str(), nullptr);
    LOGR("app: execl into %s failed — exiting silently", path.c_str());  // GALLEON-EXIT-FIXES
    std::_Exit(0);]] _boot_src "${_boot_src}")
string(REPLACE [[    LOGI("boot: launch data from the previous run, type %u", R32(page));]] [[    LOGR("boot: launch data from the previous run: type %u title %08X expect %08X data0 %08X", R32(page), R32(page + 4), R32(R32(0x10118) + 8), R32(page + 0x400));  // GALLEON-EXIT-FIXES v1.0.20: title vs expect exposes the game's launch-info title check; data0 is the save-payload marker]] _boot_src "${_boot_src}")
if(NOT _boot_src MATCHES "GALLEON-EXIT-FIXES")
    message(FATAL_ERROR "boot.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_boot} "${_boot_src}")
endif()

set(_log ${SDK}/runtime/core/log.cpp)
file(READ ${_log} _log_src)
if(NOT _log_src MATCHES "GALLEON-LOG-FIXES")
string(REPLACE [[                 : std::snprintf(line, sizeof(line), "[%9.3f] %s%s\n", t, lv == Level::Error ? "error: " : "", msg);]] [[                 : std::snprintf(line, sizeof(line), "[%9.3f] [t%04zx] %s%s\n", t, size_t(tid), lv == Level::Error ? "error: " : "", msg);  // GALLEON-LOG-FIXES]] _log_src "${_log_src}")
if(NOT _log_src MATCHES "GALLEON-LOG-FIXES")
    message(FATAL_ERROR "log.cpp could not be patched (unexpected SDK version)")
endif()
file(WRITE ${_log} "${_log_src}")
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

# v1.0.16: save-crash breadcrumbs (GALLEON-RECOMP-FIXES): the save-select's FATX path builds a MU context
# (manager at 0x1E6FC8, init sub_001D8870 -> sub_001DEA00) and later reads it via sub_001D72D0(-1); when the
# init failed and rolled back the manager to NULL the parse derefs garbage and the worker dies mid-save.
# Log the rollback, the init's OOM path and the context the parse gets.
set(_g58 ${REPO}/generated/game/x86_recomp.58.cpp)
file(READ ${_g58} _g58_src)
if(NOT _g58_src MATCHES "GALLEON-RECOMP-FIXES")
string(REPLACE [[#include "x86_recomp_shared.h"]] [[#include "x86_recomp_shared.h"
#include "core/log.h"]] _g58_src "${_g58_src}")
string(REPLACE [[	// 001D88F5 mov dword ptr [0x001E6FC8], 0x00
	{ uint32_t ea = (uint32_t)(0x1E6FC8u); X86_MEM32(ea) = (uint32_t)(0x0u); }]] [[	LOGR("mu: init ROLLED BACK r=%08X (manager cleared)", ctx.esi);  // GALLEON-RECOMP-FIXES
	// 001D88F5 mov dword ptr [0x001E6FC8], 0x00
	{ uint32_t ea = (uint32_t)(0x1E6FC8u); X86_MEM32(ea) = (uint32_t)(0x0u); }]] _g58_src "${_g58_src}")
if(NOT _g58_src MATCHES "GALLEON-RECOMP-FIXES")
    message(FATAL_ERROR "x86_recomp.58.cpp could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g58} "${_g58_src}")
endif()
set(_g59 ${REPO}/generated/game/x86_recomp.59.cpp)
file(READ ${_g59} _g59_src)
if(NOT _g59_src MATCHES "GALLEON-RECOMP-FIXES")
string(REPLACE [[#include "x86_recomp_shared.h"]] [[#include "x86_recomp_shared.h"
#include "core/log.h"]] _g59_src "${_g59_src}")
string(REPLACE [[	// 001DEA1F mov eax, 0x8007000E
	{ ctx.eax = (uint32_t)(0x8007000Eu); }]] [[	LOGR("mu: init alloc FAILED (E_OUTOFMEMORY)");  // GALLEON-RECOMP-FIXES
	// 001DEA1F mov eax, 0x8007000E
	{ ctx.eax = (uint32_t)(0x8007000Eu); }]] _g59_src "${_g59_src}")
string(REPLACE [[	// 001E25E2 mov ebp, eax
	{ ctx.ebp = (uint32_t)(ctx.eax); }]] [[	// 001E25E2 mov ebp, eax
	{ ctx.ebp = (uint32_t)(ctx.eax); }
	LOGR("mu: parse ctx=%08X mgr=%08X", ctx.ebp, X86_MEM32((uint32_t)(0x1E6FC8u)));  // GALLEON-RECOMP-FIXES]] _g59_src "${_g59_src}")
if(NOT _g59_src MATCHES "GALLEON-RECOMP-FIXES")
    message(FATAL_ERROR "x86_recomp.59.cpp could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g59} "${_g59_src}")
endif()

# v1.0.21: relaunch-loop probes (GALLEON-PROBE-FIXES): the loop is fully traced — the menu's save-commit
# (obj.method+0x1C at 0x64403) returns -1 instead of the save's slot id, the relaunch carries data0=FFFFFFFF,
# the boot check for that save fails and the boot path relaunches with the same marker forever.
# Probe the decisive points until the -1's origin is named: the commit call args/return, the boot save-check,
# which relaunch branch runs, and the gameplay-start entry.
set(_g10 ${REPO}/generated/game/x86_recomp.10.cpp)
file(READ ${_g10} _g10_src)
if(NOT _g10_src MATCHES "GALLEON-PROBE-FIXES")
string(REPLACE [[#include "x86_recomp_shared.h"]] [[#include "x86_recomp_shared.h"
#include "core/log.h"]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00064403 call [esi+0x1C]
	{ uint32_t t = X86_MEM32((uint32_t)(ctx.esi + 0x1Cu)); X86_PUSH32(0x64406u); x86_icall(ctx, base, t); }
	// 00064406 mov [ebp+0x1A4], eax
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x1A4u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }]] [[	// 00064403 call [esi+0x1C]
	{ LOGR("save: commit call obj=%08X m=%08X a1=%08X a2=%08X a3=%08X", ctx.ecx, X86_MEM32((uint32_t)(ctx.esi + 0x1Cu)), ctx.eax, ctx.edx, ctx.ebx); }  // GALLEON-PROBE-FIXES
	{ uint32_t t = X86_MEM32((uint32_t)(ctx.esi + 0x1Cu)); X86_PUSH32(0x64406u); x86_icall(ctx, base, t); }
	// 00064406 mov [ebp+0x1A4], eax
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x1A4u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
	{ LOGR("save: commit ret=%08X", ctx.eax); }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 000635A4 call 0x00068C30
	X86_PUSH32(0x635A9u); sub_00068C30(ctx, base);
	// 000635A9 test eax, eax]] [[	// 000635A4 call 0x00068C30
	X86_PUSH32(0x635A9u); sub_00068C30(ctx, base);
	{ LOGR("boot: save-check id=%08X ret=%08X", X86_MEM32((uint32_t)(0x223688u)), ctx.eax); }  // GALLEON-PROBE-FIXES
	// 000635A9 test eax, eax]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00068EBA mov ebx, [0x00223688]
	{ uint32_t ea = (uint32_t)(0x223688u); ctx.ebx = (uint32_t)(X86_MEM32(ea)); }]] [[	// 00068EBA mov ebx, [0x00223688]
	{ uint32_t ea = (uint32_t)(0x223688u); ctx.ebx = (uint32_t)(X86_MEM32(ea)); }
	{ LOGR("relaunch: marker path ebx=%08X", ctx.ebx); }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00068ED5 mov ebx, [esi+0x1A4]
	{ uint32_t ea = (uint32_t)(ctx.esi + 0x1A4u); ctx.ebx = (uint32_t)(X86_MEM32(ea)); }]] [[	// 00068ED5 mov ebx, [esi+0x1A4]
	{ uint32_t ea = (uint32_t)(ctx.esi + 0x1A4u); ctx.ebx = (uint32_t)(X86_MEM32(ea)); }
	{ LOGR("relaunch: slot path ebx=%08X", ctx.ebx); }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00068D10) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;]] [[X86_FUNC(__imp__sub_00068D10) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("menu: save-select entry ret=%08X marker=%08X", X86_MEM32(ctx.esp), X86_MEM32((uint32_t)(0x223688u))); }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00068C85 call [edx+0x1C]
	{ uint32_t t = X86_MEM32((uint32_t)(ctx.edx + 0x1Cu)); X86_PUSH32(0x68C88u); x86_icall(ctx, base, t); }
	// 00068C88 test al, al]] [[	// 00068C85 call [edx+0x1C]
	{ uint32_t t = X86_MEM32((uint32_t)(ctx.edx + 0x1Cu)); X86_PUSH32(0x68C88u); x86_icall(ctx, base, t); }
	{ char gp[72]; int gi; for (gi = 0; gi < 71; gi++) { gp[gi] = (char)X86_MEM8((uint32_t)(ctx.esp + 0x0Cu) + (uint32_t)gi); if (!gp[gi]) break; } gp[gi] = 0; LOGR("check: '%s' -> %u id=%08X", gp, (unsigned)(ctx.eax & 0xFFu), ctx.esi); }  // GALLEON-PROBE-FIXES
	// 00068C88 test al, al]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00068CD0) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	// 00068CD0 mov eax, [0x00223688]
	{ uint32_t ea = (uint32_t)(0x223688u); ctx.eax = (uint32_t)(X86_MEM32(ea)); }]] [[X86_FUNC(__imp__sub_00068CD0) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("check: refresh marker before=%08X", X86_MEM32((uint32_t)(0x223688u))); }  // GALLEON-PROBE-FIXES
	// 00068CD0 mov eax, [0x00223688]
	{ uint32_t ea = (uint32_t)(0x223688u); ctx.eax = (uint32_t)(X86_MEM32(ea)); }]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00068CE0 mov [0x00223688], eax
	{ uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }]] [[	// 00068CE0 mov [0x00223688], eax
	{ uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
	{ LOGR("check: refresh marker after=%08X", ctx.eax); }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
# v1.0.23: loop-decision probes (GALLEON-PROBE-FIXES): why the save-check gate never runs + the boot name path
string(REPLACE [[	// 0006353A mov eax, [ebp+0x1D0]
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x1D0u); ctx.eax = (uint32_t)(X86_MEM32(ea)); }]] [[	// 0006353A mov eax, [ebp+0x1D0]
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x1D0u); ctx.eax = (uint32_t)(X86_MEM32(ea)); }
	{ static uint32_t g1 = 0, g2 = 0, g3 = 0, g4 = 0, g5 = 0; static bool gfirst = true; uint32_t n1 = X86_MEM32((uint32_t)(ctx.ebp + 0x1D0u)), n2 = X86_MEM32((uint32_t)(ctx.ebp + 0x1C4u)), n3 = X86_MEM32((uint32_t)(ctx.ebp + 0x130u)), n4 = X86_MEM32((uint32_t)(0x223688u)), n5 = X86_MEM32((uint32_t)(0x2390B8u)); if (gfirst || n1 != g1 || n2 != g2 || n3 != g3 || n4 != g4 || n5 != g5) { gfirst = false; g1 = n1; g2 = n2; g3 = n3; g4 = n4; g5 = n5; LOGR("guard: idx=%08X cnt=%08X f130=%08X marker=%08X p200=%08X b8=%08X", n1, n2, n3, n4, X86_MEM32((uint32_t)(ctx.ebp + 0x200u)), n5); } }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 0006482E comiss xmm1, xmm0
	{ float a = ctx.xmm[1].f[0], b = ctx.xmm[0].f[0]; bool u = a != a || b != b; zf = u || a == b; pf = u; cf = u || a < b; of = 0; sf = 0; }]] [[	// 0006482E comiss xmm1, xmm0
	{ float a = ctx.xmm[1].f[0], b = ctx.xmm[0].f[0]; bool u = a != a || b != b; zf = u || a == b; pf = u; cf = u || a < b; of = 0; sf = 0; }
	{ static int gc = 0; if (gc < 8) { gc++; LOGR("gate: xmm0=%08X marker=%08X", ctx.xmm[0].u[0], X86_MEM32((uint32_t)(0x223688u))); } }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 000634F8 call 0x00061000
	X86_PUSH32(0x634FDu); sub_00061000(ctx, base);]] [[	// 000634F8 call 0x00061000
	X86_PUSH32(0x634FDu); sub_00061000(ctx, base);
	{ static int nb = 0; if (nb < 4) { nb++; LOGR("bootname: shellidx=%08X marker=%08X", ctx.eax, X86_MEM32((uint32_t)(0x223688u))); } }  // GALLEON-PROBE-FIXES]] _g10_src "${_g10_src}")
if(NOT _g10_src MATCHES "GALLEON-PROBE-FIXES")
    message(FATAL_ERROR "x86_recomp.10.cpp could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g10} "${_g10_src}")
endif()

# v1.0.23: the relaunch-loop fix (GALLEON-SAVESELECT-FIXES): confirming a save at the save-select
# runs sub_00067B70 whose tail relaunches via sub_00068F00(1) -> sub_00068E60(obj,1) -> the marker-form
# launch payload data[0]=[0x223688]. The marker is still -1 (the refresher sub_00068CD0 that would
# advance it is gated off), so the relaunch lands right back on the save select: the loop. Write the
# selected save index (ebx) into the marker right before the launch so the payload carries the slot.
if(NOT _g10_src MATCHES "GALLEON-SAVESELECT-FIXES")
string(REPLACE [[	// 00067C07 push 0x01
	{ uint32_t v = (uint32_t)(0x1u); X86_PUSH32(v); }
	// 00067C09 call 0x00068F00]] [[	// 00067C07 push 0x01
	{ uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(ctx.ebx); }  // GALLEON-SAVESELECT-FIXES: the relaunch carries the selected save slot
	{ LOGR("fix: tap relaunch slot=%08X marker=%08X obj=%08X name=%08X", ctx.ebx, X86_MEM32((uint32_t)(0x223688u)), ctx.esi, X86_MEM32((uint32_t)(ctx.esi + 0x154u))); }  // GALLEON-SAVESELECT-FIXES
	{ uint32_t v = (uint32_t)(0x1u); X86_PUSH32(v); }
	// 00067C09 call 0x00068F00]] _g10_src "${_g10_src}")
if(NOT _g10_src MATCHES "GALLEON-SAVESELECT-FIXES")
    message(FATAL_ERROR "x86_recomp.10.cpp save-select fix could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g10} "${_g10_src}")
endif()

# v1.0.27/1.0.26: the relaunch boot goes to the game's REAL MAIN MENU (GALLEON-LOADSAVE-FIXES).
# v1.0.24 queued state 0x1000 (the attract demo); v1.0.25 queued 0x8000 (the save load, which
# ran the save machinery and stalled in a fade = the black screen). The user wants the real
# main menu (New Game / Options / Clear Save Game) after selecting a save and relaunching.
# The menu system has two modes selected by [obj+0xC60]: mode 0 = the save-select (slot picker,
# A/B/C slot letters), mode 1 = the post-select main menu. The game's own mode-1 entry is
# state 0x800 -> sub_00065E00: sets [obj+0xC60]=1, runs sub_00068210 (the save-name setup),
# then sub_00068D10 (the menu entry) -> state 0x10 (the menu frame). The state-0x200 entry
# (sub_00066A00 at 0x66AA2) queues the save-select (mode 0) unconditionally. Fix: when the
# marker is not -1 (a save slot was selected before the relaunch), skip the save-select entry
# and queue state 0x800 instead - the game then shows the real main menu.
# v1.0.30: sub_00071730 (the menu-item list) exits immediately when [obj+0x168] (the save name)
# is null; at a relaunch boot nothing restores it and the items never build (the save-rows panel
# stays visible). Seed the selection at the auto-menu entry: [obj+0x15C] = the marker slot and
# [obj+0x168] = the game's own empty-name fallback (the string 0x217F64, which the tap handler
# itself uses for a missing name at 0x67BC0). The v1.0.27 null-name fallback also treats the
# empty name as missing (its last-wchar decode would read before the string).
# v1.0.29: the menu items are built by state 0x10 (sub_00065520), which sub_00068D10 queues only
# when the phase [obj+0x10] is 0; the v1.0.26 fix set it to 1 and the queue never happened, so the
# relaunch boot stayed on the bare save-rows panel (galleon-25.log). The phase now stays 0.
# v1.0.27: sub_00068210 decodes the slot letter from the save name's last wchar
# ([obj+0x168]); at a relaunch boot that name is null and the read crashed (SIGSEGV at
# 0x100000000, galleon-crash-10.log). When the name is null, take the slot from the marker
# (0x223688 = the chosen save) and continue at loc_00068295.
if(NOT _g10_src MATCHES "GALLEON-LOADSAVE-FIXES")
string(REPLACE [[loc_00066AA2:
	// 00066AA2 test [ebp+0x130], bl
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x130u); uint8_t a = (uint8_t)(X86_MEM8(ea)), b = (uint8_t)(((uint8_t)ctx.ebx)); uint8_t r = (uint8_t)(a & b); ca = a; cb = b; cf = 0; of = 0; zf = r == 0; sf = (uint32_t)(r >> 7) & 1; pf = X86_PARITY(r); }
	// 00066AA8 jz 0x00066AB7
	if (((uint8_t)(ca & cb)) == 0) { goto loc_00066AB7; }]] [[loc_00066AA2:
	// [GALLEON-LOADSAVE-FIXES v1.0.34] boot to main menu mode (c60=1): press A -> New Game/Options/Clear Save Game
	// marker != -1: relaunch boot with selected save; marker == -1: cold boot, use slot 0.
	// 00066AA2 mov eax, [0x00223688]
	{ ctx.eax = (uint32_t)(X86_MEM32((uint32_t)(0x223688u))); }
	// 00066AA7 cmp eax, 0xFFFFFFFF
	{ uint32_t a = (uint32_t)(ctx.eax), b = (uint32_t)(0xFFFFFFFFu); uint32_t r = (uint32_t)(a - b); ca = a; cb = b; cf = a < b; of = (uint32_t)(((a ^ b) & (a ^ r)) >> 31) & 1; zf = r == 0; sf = (uint32_t)(r >> 31) & 1; pf = X86_PARITY(r); }
	// 00066AA9 jz 0x00066AA2_MENU   ; marker == -1: cold boot -> slot 0
	if (zf) { goto loc_00066AA2_MENU; }
	// marker != -1: relaunch boot -> idx = marker
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x15Cu); X86_MEM32(ea) = (uint32_t)(ctx.eax); }  // GALLEON-LOADSAVE-FIXES: idx = marker (relaunch boot)
	goto loc_00066AA2_DONE;
loc_00066AA2_MENU:
	// marker == -1: cold boot -> idx = 0
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x15Cu); X86_MEM32(ea) = (uint32_t)(0x0u); }  // GALLEON-LOADSAVE-FIXES: idx = 0 (cold boot)
loc_00066AA2_DONE:
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x168u); X86_MEM32(ea) = (uint32_t)(0x217F64u); }  // GALLEON-LOADSAVE-FIXES: name = empty fallback (game's own 0x217F64)
	{ uint32_t ea = (uint32_t)(ctx.esi + 0x168u); X86_MEM32(ea) = (uint32_t)(0x217F64u); }  // GALLEON-LOADSAVE-FIXES: name = empty fallback (esi copy, for sub_00065520 via esi)
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0xC60u); X86_MEM8(ea) = (uint8_t)(0x1u); }  // GALLEON-LOADSAVE-FIXES: c60=1 main-menu mode
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x10u); X86_MEM32(ea) = (uint32_t)(0x0u); }  // GALLEON-LOADSAVE-FIXES: phase stays 0 so state 0x10 (menu frame) runs
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x10u); }  // GALLEON-LOADSAVE-FIXES: queue state 0x10 (main menu frame, sub_00065520)
	{ uint32_t g = X86_MEM32((uint32_t)(0x238ED4u)); if (g) { X86_MEM8(g + 0xC60u) = (uint8_t)(0x1u); X86_MEM32(g + 0x168u) = X86_MEM32((uint32_t)(0x217F64u)); X86_MEM32(g + 0x15Cu) = X86_MEM32((uint32_t)(ctx.ebp + 0x15Cu)); } }  // GALLEON-LOADSAVE-FIXES: mirror c60/name/slot onto global menu obj [0x238ED4] for sub_00071730 internal reads
	{ LOGR("fix: auto-menu idx=%08X obj=%08X", X86_MEM32((uint32_t)(ctx.ebp + 0x15Cu)), ctx.ebp); }  // GALLEON-LOADSAVE-FIXES
	// 00066AB1 pop esi
	{ uint32_t v = X86_MEM32(ctx.esp); ctx.esp += 4; ctx.esi = (uint32_t)(v); }
	// 00066AB2 pop ebp
	{ uint32_t v = X86_MEM32(ctx.esp); ctx.esp += 4; ctx.ebp = (uint32_t)(v); }
	// 00066AB3 pop ebx
	{ uint32_t v = X86_MEM32(ctx.esp); ctx.esp += 4; ctx.ebx = (uint32_t)(v); }
	// 00066AB4 ret 0x04
	ctx.esp += 8; return;]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00065520) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	// 00065520 push ebp]] [[X86_FUNC(__imp__sub_00065520) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ uint32_t o = X86_MEM32((uint32_t)(ctx.esp + 0x4u)); X86_MEM8((uint32_t)(o + 0xC60u)) = (uint8_t)(0x1u); LOGR("frame: c60=%02X idx=%08X name=%08X", X86_MEM8((uint32_t)(o + 0xC60u)), X86_MEM32((uint32_t)(o + 0x15Cu)), X86_MEM32((uint32_t)(o + 0x168u))); }  // GALLEON-LOADSAVE-FIXES: force c60=1 main-menu mode at state 0x10 (obj from stack arg, crash-12 proved eax is NOT obj)
	// 00065520 push ebp]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00065A20) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;]] [[X86_FUNC(__imp__sub_00065A20) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("menu: load-save marker=%08X ret=%08X", X86_MEM32((uint32_t)(0x223688u)), X86_MEM32(ctx.esp)); }  // GALLEON-LOADSAVE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00062725 mov dword ptr [edi+0x1C], 0x01
	{ uint32_t ea = (uint32_t)(ctx.edi + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x1u); }]] [[	// 00062725 mov dword ptr [edi+0x1C], 0x01
	{ uint32_t ea = (uint32_t)(ctx.edi + 0x1Cu); X86_MEM32(ea) = (uint32_t)(0x1u); }
	{ LOGR("menu: skip-to-game (sub_001B4B70 ret 0) marker=%08X", X86_MEM32((uint32_t)(0x223688u))); }  // GALLEON-LOADSAVE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00066AD0) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;]] [[X86_FUNC(__imp__sub_00066AD0) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("menu: done-entry (state 0x400 teardown) marker=%08X", X86_MEM32((uint32_t)(0x223688u))); }  // GALLEON-LOADSAVE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00067970) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;]] [[X86_FUNC(__imp__sub_00067970) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("tap-in ret=%08X", X86_MEM32(ctx.esp)); }  // GALLEON-LOADSAVE-FIXES]] _g10_src "${_g10_src}")
string(REPLACE [[	// 00068263 mov esi, [ebp+0x168]
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x168u); ctx.esi = (uint32_t)(X86_MEM32(ea)); }
	// 00068269 push esi]] [[	// 00068263 mov esi, [ebp+0x168]
	{ uint32_t ea = (uint32_t)(ctx.ebp + 0x168u); ctx.esi = (uint32_t)(X86_MEM32(ea)); }
	// [GALLEON-LOADSAVE-FIXES v1.0.27] null save name at a relaunch boot: derive the slot
	// letter from the marker (the chosen save) instead of decoding the name's last wchar.
	if (!ctx.esi || X86_MEM16((uint32_t)(ctx.esi)) == 0) { ctx.ebx = (uint32_t)(X86_MEM32((uint32_t)(0x223688u))); goto loc_00068295; }  // GALLEON-LOADSAVE-FIXES
	// 00068269 push esi]] _g10_src "${_g10_src}")
if(NOT _g10_src MATCHES "GALLEON-LOADSAVE-FIXES")
    message(FATAL_ERROR "x86_recomp.10.cpp load-save fix could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g10} "${_g10_src}")
endif()

# v1.0.23: boot-gate probe (GALLEON-GATE-FIXES): where the launch payload's marker restore
set(_g53 ${REPO}/generated/game/x86_recomp.53.cpp)
file(READ ${_g53} _g53_src)
if(NOT _g53_src MATCHES "GALLEON-GATE-FIXES")
string(REPLACE [[	// 001B44F0 mov [0x00223688], eax
	{ uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }]] [[	// 001B44F0 mov [0x00223688], eax
	{ uint32_t ea = (uint32_t)(0x223688u); X86_MEM32(ea) = (uint32_t)(ctx.eax); }
	{ LOGR("gate: boot marker=%08X", ctx.eax); }  // GALLEON-GATE-FIXES]] _g53_src "${_g53_src}")
if(NOT _g53_src MATCHES "GALLEON-GATE-FIXES")
    message(FATAL_ERROR "x86_recomp.53.cpp gate probe could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g53} "${_g53_src}")
endif()

# v1.0.28: crash-11 null path suffix (GALLEON-LOADSAVE-FIXES): the mode-1 menu context resolves
# assets through sub_001B5C70 -> sub_001C5979 (path append) -> sub_001C553F (wide append) and a
# NULL asset name faults reading guest-NULL. Guard both helpers: sub_001C5979's caller (@0x1B5CA1
# "test eax,eax / jz") continues on 0 and bails on non-zero, so return 1 on a null suffix;
# sub_001C553F appends nothing and returns 0 on a null source. ret 0x18 -> esp += 28; ret 0x0C -> esp += 16.
set(_g55 ${REPO}/generated/game/x86_recomp.55.cpp)
file(READ ${_g55} _g55_src)
if(NOT _g55_src MATCHES "GALLEON-LOADSAVE-FIXES")
string(REPLACE [[X86_FUNC(__imp__sub_001C5979) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	// 001C5979 push ebp]] [[X86_FUNC(__imp__sub_001C5979) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	if (!X86_MEM32((uint32_t)(ctx.esp + 0x8u))) { ctx.eax = 1u; ctx.esp += 28; return; }  // GALLEON-LOADSAVE-FIXES: v1.0.28 null path suffix -> fail cleanly
	// 001C5979 push ebp]] _g55_src "${_g55_src}")
string(REPLACE [[X86_FUNC(__imp__sub_001C553F) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	// 001C553F push ebp]] [[X86_FUNC(__imp__sub_001C553F) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	if (!X86_MEM32((uint32_t)(ctx.esp + 0x4u))) { ctx.eax = 0u; ctx.esp += 16; return; }  // GALLEON-LOADSAVE-FIXES: v1.0.28 null wide source -> append nothing
	// 001C553F push ebp]] _g55_src "${_g55_src}")
if(NOT _g55_src MATCHES "GALLEON-LOADSAVE-FIXES")
    message(FATAL_ERROR "x86_recomp.55.cpp null-path guard could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g55} "${_g55_src}")
endif()

# v1.0.30: probe the menu-item list builder (GALLEON-LOADSAVE-FIXES)
set(_g11 ${REPO}/generated/game/x86_recomp.11.cpp)
file(READ ${_g11} _g11_src)
if(NOT _g11_src MATCHES "GALLEON-LOADSAVE-FIXES")
string(REPLACE [[#include "x86_recomp_shared.h"]] [[#include "x86_recomp_shared.h"
#include "core/log.h"]] _g11_src "${_g11_src}")
string(REPLACE [[X86_FUNC(__imp__sub_00071730) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	// 00071730 mov eax, 0x295C]] [[X86_FUNC(__imp__sub_00071730) {
	X86_LOAD_FLAGS();
	uint32_t ca = 0, cb = 0;
	(void)ca; (void)cb;
	{ LOGR("list: a1=%08X a2=%08X a3=%08X a4=%08X", X86_MEM32((uint32_t)(ctx.esp + 0x4u)), X86_MEM32((uint32_t)(ctx.esp + 0x8u)), X86_MEM32((uint32_t)(ctx.esp + 0xCu)), X86_MEM32((uint32_t)(ctx.esp + 0x10u))); }  // GALLEON-LOADSAVE-FIXES
	// 00071730 mov eax, 0x295C]] _g11_src "${_g11_src}")
if(NOT _g11_src MATCHES "GALLEON-LOADSAVE-FIXES")
    message(FATAL_ERROR "x86_recomp.11.cpp menu-list probe could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g11} "${_g11_src}")
endif()


# v1.0.32: skip sub_001C3EA8 empty-name processing in sub_00067DE0 (crash-13)
set(_g12 ${REPO}/generated/game/x86_recomp.10.cpp)
file(READ ${_g12} _g12_src)
if(NOT _g12_src MATCHES "GALLEON-LOADSAVE-FIXES-067DE0")
string(REPLACE [[// 00067DF6 jz 0x00067E06
	if (((uint32_t)(ca & cb)) == 0) { goto loc_00067E06; }
	// 00067DF8 push eax
	{ uint32_t v = (uint32_t)(ctx.eax); X86_PUSH32(v); }]] [[// 00067DF6 jz 0x00067E06
	if (((uint32_t)(ca & cb)) == 0) { goto loc_00067E28; }  // GALLEON-LOADSAVE-FIXES-067DE0: v1.0.32 empty name -> skip name processing AND the [obj+0x168] zeroing at 0x67E22
	{ uint8_t b = X86_MEM8((uint32_t)(ctx.eax)); if (b == 0) { goto loc_00067E28; } }  // GALLEON-LOADSAVE-FIXES-067DE0: v1.0.32 skip sub_001C3EA8 empty-name processing + name zeroing (crash-13)
	// 00067DF8 push eax
	{ uint32_t v = (uint32_t)(ctx.eax); X86_PUSH32(v); }]] _g12_src "${_g12_src}")
if(NOT _g12_src MATCHES "GALLEON-LOADSAVE-FIXES-067DE0")
    message(FATAL_ERROR "x86_recomp.10.cpp sub_00067DE0 empty-name guard could not be patched (recompiler output changed?)")
endif()
file(WRITE ${_g12} "${_g12_src}")
endif()

# v1.0.43: main-menu items not wiped on relaunch boot (GALLEON-LOADSAVE-FIXES).
# sub_00068D10 copies [obj+0xD48] (stale save-select data) over [obj+0xC5C] (the main-menu
# items sub_00071730 just built) and zeroes [obj+0x15C] (the selected slot),
# so the relaunch boot shows save-select instead of the real main menu. When main-menu
# items are already built at [obj+0xC5C], skip both: keep sub_00071730's items and
# the selected slot.
set(_g60 ${REPO}/generated/game/x86_recomp.10.cpp)
if(NOT EXISTS "${_g60}")
    message(FATAL_ERROR "x86_recomp.10.cpp not found: ${_g60}")
endif()

# v1.0.43: main-menu items not wiped on relaunch boot (GALLEON-LOADSAVE-FIXES-068D10).
# sub_00068D10 copies [obj+0xD48] (stale save-select data) over [obj+0xC5C] (the main-menu
# items sub_00065E00/sub_00071730 just built) and zeroes [obj+0x15C] (the selected slot),
# so the relaunch boot shows save-select instead of the real main menu. When main-menu
# items already exist at [obj+0xC5C], skip both: keep sub_00071730's items and
# the selected slot.
execute_process(
    COMMAND python3 "${REPO}/tools/patch_g60.py" "${_g60}"
    WORKING_DIRECTORY ${REPO}
    RESULT_VARIABLE _g60_result
    ERROR_VARIABLE _g60_error
    OUTPUT_VARIABLE _g60_output
)
if(NOT _g60_result EQUAL 0)
    message(FATAL_ERROR "x86_recomp.10.cpp _g60 patch failed: ${_g60_error}\n${_g60_output}")
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
