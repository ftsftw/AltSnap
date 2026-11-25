/* SPDX-License-Identifier: GPL-3.0-or-later
   Local image SHA-256, held read-only without write/delete sharing throughout
   one operation. Crypto is provided by Windows, not a private implementation. */
#ifndef AUTO_TILE_OWNED_IMAGE_H
#define AUTO_TILE_OWNED_IMAGE_H
#include <wincrypt.h>
typedef struct { HANDLE file; BY_HANDLE_FILE_INFORMATION info; } ATGOwnedImage;

static void ATGOwnedImageClose(ATGOwnedImage *image)
{
    if (image->file && image->file != INVALID_HANDLE_VALUE) CloseHandle(image->file);
    image->file=NULL;
}

static int ATGOwnedImageStable(const ATGOwnedImage *image)
{
    BY_HANDLE_FILE_INFORMATION info;
    return image->file && GetFileInformationByHandle(image->file, &info)
        && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
        && info.dwVolumeSerialNumber==image->info.dwVolumeSerialNumber
        && info.nFileIndexHigh==image->info.nFileIndexHigh
        && info.nFileIndexLow==image->info.nFileIndexLow
        && info.nFileSizeHigh==image->info.nFileSizeHigh
        && info.nFileSizeLow==image->info.nFileSizeLow
        && info.ftLastWriteTime.dwLowDateTime==image->info.ftLastWriteTime.dwLowDateTime
        && info.ftLastWriteTime.dwHighDateTime==image->info.ftLastWriteTime.dwHighDateTime;
}

static int ATGOwnedImageOpen(const TCHAR *path, TCHAR digest[65], ATGOwnedImage *image)
{
    typedef BOOL (WINAPI *acquire_t)(HCRYPTPROV *, LPCWSTR, LPCWSTR, DWORD, DWORD);
    typedef BOOL (WINAPI *create_t)(HCRYPTPROV, ALG_ID, HCRYPTKEY, DWORD, HCRYPTHASH *);
    typedef BOOL (WINAPI *data_t)(HCRYPTHASH, const BYTE *, DWORD, DWORD);
    typedef BOOL (WINAPI *value_t)(HCRYPTHASH, DWORD, BYTE *, DWORD *, DWORD);
    typedef BOOL (WINAPI *destroy_t)(HCRYPTHASH);
    typedef BOOL (WINAPI *release_t)(HCRYPTPROV, DWORD);
    typedef DWORD (WINAPI *final_t)(HANDLE, LPTSTR, DWORD, DWORD);
    acquire_t acquire=(acquire_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptAcquireContextW");
    create_t create=(create_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptCreateHash");
    data_t data=(data_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptHashData");
    value_t value=(value_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptGetHashParam");
    destroy_t destroy=(destroy_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptDestroyHash");
    release_t release=(release_t)(void (*)(void))LoadDLLProc("ADVAPI32.DLL", "CryptReleaseContext");
#ifdef _UNICODE
    final_t final_path=(final_t)(void (*)(void))LoadDLLProc("KERNEL32.DLL", "GetFinalPathNameByHandleW");
#else
    final_t final_path=(final_t)(void (*)(void))LoadDLLProc("KERNEL32.DLL", "GetFinalPathNameByHandleA");
#endif
    HCRYPTPROV provider=0;
    HCRYPTHASH hash=0;
    BYTE *buffer=NULL, bytes[32];
    DWORD read, total=0, length=sizeof bytes;
    unsigned i;
    int ok=0;
    TCHAR root[4];
    TCHAR canonical[MAX_PATH+8];
    UINT drive;
    mem00(image, sizeof *image); digest[0]=0;
    if (!ATGOwnedLocalPath(path) || !acquire || !create || !data || !value || !destroy || !release || !final_path) return 0;
    root[0]=path[0]; root[1]=':'; root[2]='\\'; root[3]=0;
    drive=GetDriveType(root);
    if (drive!=DRIVE_FIXED && drive!=DRIVE_REMOVABLE && drive!=DRIVE_RAMDISK) return 0;
    image->file=CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (image->file==INVALID_HANDLE_VALUE) { image->file=NULL; return 0; }
    length=final_path(image->file, canonical, ARR_SZ(canonical), 0);
    if (length<8 || length>=ARR_SZ(canonical) || canonical[length]
        || canonical[0]!='\\' || canonical[1]!='\\' || canonical[2]!='?' || canonical[3]!='\\'
        || lstrcmpi(path, canonical+4)) goto done;
    if (!GetFileInformationByHandle(image->file, &image->info)
        || (image->info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
        || image->info.nFileSizeHigh || !image->info.nFileSizeLow
        || image->info.nFileSizeLow>128u*1024u*1024u) goto done;
    buffer=(BYTE *)malloc(16384);
    if (!buffer || !acquire(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT|CRYPT_SILENT)
        || !create(provider, CALG_SHA_256, 0, 0, &hash)) goto done;
    for (;;) {
        if (!ReadFile(image->file, buffer, 16384, &read, NULL)) goto done;
        if (!read) break;
        if (read>16384 || total>image->info.nFileSizeLow
            || read>image->info.nFileSizeLow-total || !data(hash, buffer, read, 0)) goto done;
        total+=read;
    }
    if (total!=image->info.nFileSizeLow || !ATGOwnedImageStable(image)) goto done;
    length=sizeof bytes;
    if (!value(hash, HP_HASHVAL, bytes, &length, 0) || length!=sizeof bytes) goto done;
    for (i=0; i<sizeof bytes; ++i) {
        digest[i*2]=TEXT("0123456789abcdef")[bytes[i]>>4];
        digest[i*2+1]=TEXT("0123456789abcdef")[bytes[i]&15];
    }
    digest[64]=0; ok=1;
done:
    if (hash) destroy(hash);
    if (provider) release(provider, 0);
    if (buffer) free(buffer);
    if (!ok) ATGOwnedImageClose(image);
    return ok;
}
#endif
