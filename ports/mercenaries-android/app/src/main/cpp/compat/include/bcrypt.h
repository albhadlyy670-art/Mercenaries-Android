/* Shadow <bcrypt.h>: only the SHA-256 hashing subset the game uses (retail XBE verification). */
#ifndef BCRYPT_SHADOW_H
#define BCRYPT_SHADOW_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *BCRYPT_ALG_HANDLE;
typedef void *BCRYPT_HASH_HANDLE;
#define BCRYPT_SHA256_ALGORITHM L"SHA256"
#define BCRYPT_OBJECT_LENGTH L"ObjectLength"
#define BCRYPT_HASH_LENGTH L"HashDigestLength"
NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE *alg, const void *id, const void *impl, ULONG flags);
NTSTATUS BCryptGetProperty(void *obj, const void *name, PUCHAR out, ULONG size, ULONG *result, ULONG flags);
NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE alg, BCRYPT_HASH_HANDLE *hash, PUCHAR object, ULONG object_size, PUCHAR secret, ULONG secret_size, ULONG flags);
NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE hash, PUCHAR data, ULONG size, ULONG flags);
NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE hash, PUCHAR out, ULONG size, ULONG flags);
NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hash);
NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE alg, ULONG flags);
#ifdef __cplusplus
}
#endif
#endif
