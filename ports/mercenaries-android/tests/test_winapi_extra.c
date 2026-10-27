#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    char buf[256]; const char *ini = "/tmp/t_win.ini"; unlink(ini);
    CHECK(GetPrivateProfileIntA("Video", "Width", 640, ini) == 640, "missing file returns default");
    CHECK(WritePrivateProfileStringA("Video", "Width", "1280", ini), "write new file");
    CHECK(WritePrivateProfileStringA("Video", "Height", "720", ini), "add key to existing section");
    CHECK(WritePrivateProfileStringA("Audio", "Volume", "7", ini), "add new section");
    CHECK(GetPrivateProfileIntA("video", "WIDTH", 0, ini) == 1280, "section and key names are case-insensitive");
    CHECK(GetPrivateProfileIntA("Video", "Height", 0, ini) == 720 && GetPrivateProfileIntA("Audio", "Volume", 0, ini) == 7, "all values readable");
    WritePrivateProfileStringA("Video", "Width", "1920", ini); CHECK(GetPrivateProfileIntA("Video", "Width", 0, ini) == 1920, "replace existing key");
    GetPrivateProfileStringA("Video", "Mode", "borderless", buf, sizeof(buf), ini); CHECK(!strcmp(buf, "borderless"), "string default");
    WritePrivateProfileStringA("Video", "Title", "Hello World", ini); GetPrivateProfileStringA("Video", "Title", "", buf, sizeof(buf), ini); CHECK(!strcmp(buf, "Hello World"), "string with a space");
    WritePrivateProfileStringA("Video", "Height", NULL, ini); CHECK(GetPrivateProfileIntA("Video", "Height", -1, ini) == (DWORD)-1, "NULL value deletes the key");
    CHECK(GetPrivateProfileIntA("Video", "Width", 0, ini) == 1920 && GetPrivateProfileIntA("Audio", "Volume", 0, ini) == 7, "other keys survive a delete");
    char sec[256]; DWORD n = GetPrivateProfileSectionA("Video", sec, sizeof(sec), ini); CHECK(n > 0 && strstr(sec, "Width=1920"), "section enumeration returns key=value");
    WritePrivateProfileStringA("Audio", NULL, NULL, ini); CHECK(GetPrivateProfileIntA("Audio", "Volume", 99, ini) == 99, "NULL key deletes the section");
    FILE *f = fopen("/tmp/t_win2.ini", "w"); fputs("; comment\n[ Dev ]\n  logging = 1 \n#x\nname=\"quoted\"\n", f); fclose(f);
    CHECK(GetPrivateProfileIntA("Dev", "logging", 0, "/tmp/t_win2.ini") == 1, "tolerates comments, padding and spaced section name");
    GetPrivateProfileStringA("Dev", "name", "", buf, sizeof(buf), "/tmp/t_win2.ini"); CHECK(!strcmp(buf, "quoted"), "surrounding quotes are stripped");
    char t[16]; CHECK(strcpy_s(t, sizeof(t), "short") == 0 && !strcmp(t, "short"), "strcpy_s");
    CHECK(strcpy_s(t, 4, "toolong") != 0 && t[0] == 0, "strcpy_s overflow clears the buffer");
    strcpy_s(t, sizeof(t), "ab"); CHECK(strcat_s(t, sizeof(t), "cd") == 0 && !strcmp(t, "abcd"), "strcat_s");
    CHECK(strncpy_s(t, 4, "abcdef", _TRUNCATE) == STRUNCATE && !strcmp(t, "abc"), "strncpy_s _TRUNCATE truncates");
    char full[512]; DWORD fl = GetFullPathNameA("/a/b/../c/./d.txt", sizeof(full), full, NULL); CHECK(!strcmp(full, "/a/c/d.txt") && fl == strlen(full), "GetFullPathNameA normalises (%s)", full);
    fl = GetFullPathNameA("game_files\\mercenaries-retail", sizeof(full), full, NULL); CHECK(strstr(full, "/game_files/mercenaries-retail") != NULL, "backslashes become separators (%s)", full);
    volatile LONG x = 0xF0; InterlockedAnd(&x, 0x3C); CHECK(x == 0x30, "InterlockedAnd"); InterlockedOr(&x, 0x0F); CHECK(x == 0x3F, "InterlockedOr");
    volatile LONG64 y = 5; CHECK(InterlockedCompareExchange64(&y, 9, 5) == 5 && y == 9, "InterlockedCompareExchange64 success"); CHECK(InterlockedCompareExchange64(&y, 1, 5) == 9 && y == 9, "…failure leaves value");
    CHECK(InterlockedExchange64(&y, 3) == 9 && y == 3, "InterlockedExchange64");
    FILETIME a = { 1, 0 }, b = { 2, 0 }; CHECK(CompareFileTime(&a, &b) < 0 && CompareFileTime(&b, &a) > 0 && CompareFileTime(&a, &a) == 0, "CompareFileTime");
    SetEnvironmentVariableA("WINAPI_T", "v"); CHECK(!strcmp(getenv("WINAPI_T"), "v"), "SetEnvironmentVariableA"); SetEnvironmentVariableA("WINAPI_T", NULL); CHECK(!getenv("WINAPI_T"), "…and removal");
    CHECK(GetFileAttributesA("/tmp") == 0x10 && GetFileAttributesA("/nonexistent") == INVALID_FILE_ATTRIBUTES, "GetFileAttributesA");
    printf(bad ? "%d FAILURES\n" : "winapi_extra: all checks passed\n", bad); return bad != 0;
}
