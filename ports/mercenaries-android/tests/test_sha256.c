#include <bcrypt.h>
#include <stdio.h>
#include <string.h>
static void hex(const unsigned char *d, char *o) { for (int i = 0; i < 32; ++i) sprintf(o + 2*i, "%02x", d[i]); }
static int sha(const void *data, size_t n, char *out)
{
    BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE h; unsigned char obj[1024], dg[32]; ULONG osz = 0, dsz = 0, r;
    if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return 0;
    BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PUCHAR)&osz, 4, &r, 0); BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PUCHAR)&dsz, 4, &r, 0);
    if (dsz != 32 || osz > sizeof(obj)) return 0;
    BCryptCreateHash(a, &h, obj, osz, NULL, 0, 0); BCryptHashData(h, (PUCHAR)data, (ULONG)n, 0); BCryptFinishHash(h, dg, 32, 0);
    BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a, 0); hex(dg, out); return 1;
}
int main(void)
{
    char o[65]; int bad = 0;
    sha("", 0, o); if (strcmp(o, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")) { puts("FAIL empty"); bad++; }
    sha("abc", 3, o); if (strcmp(o, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) { puts("FAIL abc"); bad++; }
    const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"; sha(m, strlen(m), o);
    if (strcmp(o, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")) { puts("FAIL 448-bit"); bad++; }
    static unsigned char million[1000000]; memset(million, 'a', sizeof(million)); sha(million, sizeof(million), o);
    if (strcmp(o, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")) { puts("FAIL million a"); bad++; }
    puts(bad ? "SHA-256 FAILED" : "SHA-256: all 4 standard vectors pass"); return bad;
}
