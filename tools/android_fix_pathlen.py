#!/usr/bin/env python3
"""Android fix #8 (idempotent, run from the repo root): honour OBJECT_STRING.Length when reading an Xbox path.
FindFirstFile hands the kernel a path whose Length covers only the parent directory while the buffer still holds the
full file name. The Windows code copies exactly Length bytes; the POSIX code used the whole buffer, so the game's
"does streamed.dsk exist?" probe opened the file itself instead of its directory and found nothing."""
p = "src/kernel/kernel_file.c"
s = open(p, encoding="utf-8").read()
if "ANDROID_PATHLEN_FIX" in s:
    print("already applied")
else:
    old = """static const char* get_xbox_path(PXBOX_OBJECT_ATTRIBUTES ObjectAttributes)
{
    if (!ObjectAttributes || !ObjectAttributes->ObjectName ||
        !ObjectAttributes->ObjectName->Buffer)
        return NULL;
    return ObjectAttributes->ObjectName->Buffer;
}"""
    new = """static const char* get_xbox_path(PXBOX_OBJECT_ATTRIBUTES ObjectAttributes)
{
    /* ANDROID_PATHLEN_FIX */
    static __thread char path_ring[4][1024];
    static __thread unsigned path_ring_pos;
    if (!ObjectAttributes || !ObjectAttributes->ObjectName ||
        !ObjectAttributes->ObjectName->Buffer)
        return NULL;
    size_t n = ObjectAttributes->ObjectName->Length;
    if (n >= sizeof(path_ring[0])) n = sizeof(path_ring[0]) - 1;
    char *out = path_ring[path_ring_pos++ & 3];
    memcpy(out, ObjectAttributes->ObjectName->Buffer, n);
    out[n] = '\\0';
    return out;
}"""
    if old not in s:
        print("WARNING pattern not found")
    else:
        open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
        print("applied")
