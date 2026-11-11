#!/usr/bin/env python3
"""Android diagnostics #3 (idempotent, run from the repo root): record file opens/reads/directory listings and
rejected handles into events.log via android_trace()."""
import sys

def patch_part(path, marker, old, new, tag):
    s = open(path, encoding="utf-8").read()
    if tag in s:
        print(tag, "already applied"); return
    i = s.find(marker) if marker else 0
    if i < 0 or old not in s[i:]:
        print("WARNING pattern not found for", tag); return
    head, tail = s[:i], s[i:]
    tail = tail.replace(old, new, 1)
    open(path, "w", encoding="utf-8").write(head + tail)
    print("applied", tag)

K = "src/kernel/kernel_file.c"
M = "#else /* !_WIN32 */"
patch_part(K, None, M, M + """
#ifdef __ANDROID__
void android_trace(const char *fmt, ...);
static int g_file_trace_n;   /* FILE_TRACE_MACRO */
#define FILE_TRACE(...) do { if (g_file_trace_n < 200) { ++g_file_trace_n; android_trace(__VA_ARGS__); } } while (0)
#else
#define FILE_TRACE(...) ((void)0)
#endif""", "FILE_TRACE_MACRO")
patch_part(K, M, '''        xbox_log(XBOX_LOG_ERROR, XBOX_LOG_FILE, "NtCreateFile: path translation failed");
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }

    int fd;''', '''        xbox_log(XBOX_LOG_ERROR, XBOX_LOG_FILE, "NtCreateFile: path translation failed");
        FILE_TRACE("[file] path translation failed: %s", xbox_path ? xbox_path : "(null)"); /* FILE_TRACE_A */
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }

    int fd;''', "FILE_TRACE_A")
patch_part(K, M, '''        XBOX_TRACE(XBOX_LOG_FILE, "NtCreateFile FAILED: %s (errno=%d)", host_path, e);''',
 '''        XBOX_TRACE(XBOX_LOG_FILE, "NtCreateFile FAILED: %s (errno=%d)", host_path, e);
        { struct stat st_; int sr_ = stat(host_path, &st_);
          FILE_TRACE("[file] open FAILED %s errno=%d opts=%08X acc=%08X disp=%u stat=%d mode=%o size=%lld", host_path, e,
                     (unsigned)CreateOptions, (unsigned)DesiredAccess, (unsigned)CreateDisposition, sr_,
                     sr_ == 0 ? (unsigned)st_.st_mode : 0u, sr_ == 0 ? (long long)st_.st_size : -1LL); } /* FILE_TRACE_B */''', "FILE_TRACE_B")
patch_part(K, M, '''    XBOX_TRACE(XBOX_LOG_FILE, "NtCreateFile: %s -> handle=%p", host_path, *FileHandle);''',
 '''    XBOX_TRACE(XBOX_LOG_FILE, "NtCreateFile: %s -> handle=%p", host_path, *FileHandle);
    FILE_TRACE("[file] open ok %s", host_path); /* FILE_TRACE_C */''', "FILE_TRACE_C")
patch_part(K, M, '''    ssize_t n = read(fd, Buffer, Length);
    if (n < 0) {''', '''    ssize_t n = read(fd, Buffer, Length);
    FILE_TRACE("[file] read fd=%d len=%u off=%lld -> %lld", fd, (unsigned)Length, ByteOffset ? (long long)ByteOffset->QuadPart : -1LL, (long long)n); /* FILE_TRACE_D */
    if (n < 0) {''', "FILE_TRACE_D")
patch_part(K, M, '''    if (!s_dir_cs_init) { InitializeCriticalSection(&s_dir_cs); s_dir_cs_init = TRUE; }
    EnterCriticalSection(&s_dir_cs);

    /* Locate or create''', '''    FILE_TRACE("[file] dir query handle=%p restart=%d class=%d name=%.*s", FileHandle, (int)RestartScan, (int)FileInformationClass,
               FileName && FileName->Buffer ? (int)FileName->Length : 0, FileName && FileName->Buffer ? FileName->Buffer : ""); /* FILE_TRACE_E */
    if (!s_dir_cs_init) { InitializeCriticalSection(&s_dir_cs); s_dir_cs_init = TRUE; }
    EnterCriticalSection(&s_dir_cs);

    /* Locate or create''', "FILE_TRACE_E")

W = "src/platform/win32_compat.c"
patch_part(W, None, "static void w32_bad_handle(const char *fn, HANDLE h)\n{",
 """#ifdef __ANDROID__
void android_trace(const char *fmt, ...);
#endif
static void w32_bad_handle(const char *fn, HANDLE h)
{
#ifdef __ANDROID__
    { static volatile int n;
      extern ptrdiff_t g_xbox_mem_offset;
      if (n < 40) { ++n;
        uintptr_t va_ = (uintptr_t)h - (uintptr_t)g_xbox_mem_offset;
        if (va_ < 0x08000000u) {
            const unsigned char *b_ = (const unsigned char *)h;
            android_trace("[w32] %s on a non-handle %p guest_va=%08X bytes=%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",
                          fn, (void *)h, (unsigned)va_, b_[0], b_[1], b_[2], b_[3], b_[4], b_[5], b_[6], b_[7],
                          b_[8], b_[9], b_[10], b_[11], b_[12], b_[13], b_[14], b_[15]);
        } else android_trace("[w32] %s on a non-handle %p (outside guest RAM)", fn, (void *)h);
      } } /* W32_TRACE */
#endif""", "W32_TRACE")
