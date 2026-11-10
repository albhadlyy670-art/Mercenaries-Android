#!/usr/bin/env python3
"""Android fix #5 (idempotent, run from the repo root): NtCreateFile with FILE_DIRECTORY_FILE on an existing
regular file. Windows (CreateFile + FILE_FLAG_BACKUP_SEMANTICS) opens the file; the POSIX version used O_DIRECTORY and
failed with ENOTDIR, so the game could never open assets.dsk / streamed.dsk / english.dsk."""
p = "src/kernel/kernel_file.c"
s = open(p, encoding="utf-8").read()
if "ANDROID_DIRFLAG_FILE" in s:
    print("already applied")
else:
    old = """    int fd;
    if (CreateOptions & XBOX_FILE_DIRECTORY_FILE) {
        if (CreateDisposition == XBOX_FILE_CREATE || CreateDisposition == XBOX_FILE_OPEN_IF)
            mkdir(host_path, 0755);   /* EEXIST is fine */
        fd = open(host_path, O_RDONLY | O_DIRECTORY);
    } else {"""
    new = """    int fd;
    struct stat dirflag_st_;   /* ANDROID_DIRFLAG_FILE */
    if ((CreateOptions & XBOX_FILE_DIRECTORY_FILE) && stat(host_path, &dirflag_st_) == 0 && !S_ISDIR(dirflag_st_.st_mode)) {
        /* Windows opens an existing regular file here (FILE_FLAG_BACKUP_SEMANTICS); do the same. */
        fd = open(host_path, posix_open_flags(DesiredAccess, XBOX_FILE_OPEN), 0644);
    } else if (CreateOptions & XBOX_FILE_DIRECTORY_FILE) {
        if (CreateDisposition == XBOX_FILE_CREATE || CreateDisposition == XBOX_FILE_OPEN_IF)
            mkdir(host_path, 0755);   /* EEXIST is fine */
        fd = open(host_path, O_RDONLY | O_DIRECTORY);
    } else {"""
    if old not in s:
        print("WARNING pattern not found")
    else:
        open(p, "w", encoding="utf-8").write(s.replace(old, new, 1))
        print("applied")
