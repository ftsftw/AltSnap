/* SPDX-License-Identifier: GPL-3.0-or-later
   Real Windows filesystem/CryptoAPI/INI contracts, without application GUI. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../resource.h"
#undef assert
#define assert(condition) do { if (!(condition)) { fprintf(stderr,"native profile line %d error %lu\n",__LINE__,(unsigned long)GetLastError()); exit(1); } } while(0)
#define ARR_SZ(a) (sizeof(a)/sizeof((a)[0]))
#define mem00(p,n) memset(p,0,n)
static FARPROC LoadDLLProc(const char *dll, const char *name)
{
    HMODULE module=GetModuleHandleA(dll);
    if (!module) module=LoadLibraryA(dll);
    return module ? GetProcAddress(module,name) : NULL;
}
#include "../auto_tile_owned_store.h"
#include "../auto_tile_owned_image.h"

static void write_bytes(const TCHAR *path, const void *bytes, DWORD size)
{
    DWORD written=0;
    HANDLE f=CreateFile(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(f!=INVALID_HANDLE_VALUE && WriteFile(f,bytes,size,&written,NULL) && written==size);
    assert(CloseHandle(f));
}

int main(void)
{
    TCHAR temp[MAX_PATH], folder[MAX_PATH], file[MAX_PATH], ini[MAX_PATH], digest[65];
    ATGOwnedImage image;
    ATGOwnedProfile p, readback;
    HANDLE writer;
    unsigned i;
    static const TCHAR abc[]=TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    WORD bom=0xfeff;
    {
        TCHAR app[MAX_PATH];
        HMODULE resources;
        DWORD count=GetModuleFileName(NULL,app,ARR_SZ(app));
        assert(count && count<ARR_SZ(app));
        while(count && app[count-1]!='\\') --count;
        assert(count && count+12<ARR_SZ(app));
        lstrcpy(app+count,TEXT("AltSnap.exe"));
        resources=LoadLibraryEx(app,NULL,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        assert(resources && FindResource(resources,MAKEINTRESOURCE(IDD_OWNED_PROFILES),RT_DIALOG)
               && FindResource(resources,MAKEINTRESOURCE(IDD_BLACKLISTPAGE),RT_DIALOG));
        assert(FreeLibrary(resources)); /* Resource inspection never executes AltSnap. */
    }
    DWORD n=GetTempPath(ARR_SZ(temp),temp);
    assert(n && n<ARR_SZ(temp));
    assert(GetLongPathName(temp,folder,ARR_SZ(folder)));
    assert(GetTempFileName(folder,TEXT("atg"),0,temp));
    assert(DeleteFile(temp) && CreateDirectory(temp,NULL));
    lstrcpy(folder,temp);
    wsprintf(file,TEXT("%s\\build.bin"),folder);
    wsprintf(ini,TEXT("%s\\profiles.ini"),folder);
    write_bytes(file,"abc",3);
    assert(ATGOwnedImageOpen(file,digest,&image) && !lstrcmp(digest,abc));
    assert(ATGOwnedImageStable(&image));
    writer=CreateFile(file,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                      NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(writer==INVALID_HANDLE_VALUE); /* The operation holds no write sharing. */
    assert(!DeleteFile(file));
    ATGOwnedImageClose(&image);
    write_bytes(file,"abcd",4);
    assert(ATGOwnedImageOpen(file,digest,&image) && lstrcmp(digest,abc));
    ATGOwnedImageClose(&image);
    writer=CreateFile(file,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    assert(writer!=INVALID_HANDLE_VALUE);
    assert(!ATGOwnedImageOpen(file,digest,&image));
    assert(CloseHandle(writer));
    write_bytes(file,"",0);
    assert(!ATGOwnedImageOpen(file,digest,&image));
    assert(!ATGOwnedImageOpen(TEXT("\\\\invalid-server\\app.exe"),digest,&image));
    writer=CreateFile(file,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    assert(writer!=INVALID_HANDLE_VALUE);
    {
        LARGE_INTEGER size;
        size.QuadPart=128u*1024u*1024u+1;
        assert(SetFilePointerEx(writer,size,NULL,FILE_BEGIN) && SetEndOfFile(writer));
    }
    assert(CloseHandle(writer) && !ATGOwnedImageOpen(file,digest,&image));
    assert(DeleteFile(file));
    assert(!ATGOwnedImageOpen(file,digest,&image));
    /* UTF-16 INI matches the shipped portable configuration. */
    write_bytes(ini,&bom,sizeof bom);
    mem00(&p,sizeof p);
#ifdef _UNICODE
    lstrcpy(p.path,L"C:\\\u5de5\u5177\\portable.exe");
    lstrcpy(p.candidate_class,L"\u4e3b\u7a97\u53e3");
#else
    lstrcpy(p.path,TEXT("C:\\Tools\\portable.exe"));
    lstrcpy(p.candidate_class,TEXT("MainClass"));
#endif
    lstrcpy(p.owner_class,TEXT("OwnerClass"));
    for(i=0;i<64;++i) p.digest[i]='a';
    p.style=WS_CAPTION|WS_THICKFRAME; p.exstyle=WS_EX_APPWINDOW;
    p.owner_style=WS_POPUP; p.owner_exstyle=WS_EX_TOOLWINDOW; p.enabled=1; p.valid=1;
    assert(ATGOwnedProfileSave(ini,0,&p));
    assert(ATGOwnedProfileRead(ini,0,&readback) && !readback.enabled);
    assert(!lstrcmp(readback.path,p.path) && !lstrcmp(readback.candidate_class,p.candidate_class));
    assert(!lstrcmp(readback.digest,p.digest));
    assert(ATGOwnedProfileEnable(ini,0,1) && ATGOwnedProfileRead(ini,0,&readback) && readback.enabled);
    assert(WritePrivateProfileString(TEXT("AutoTileOwned1"),TEXT("Schema"),TEXT("2"),ini));
    assert(!ATGOwnedProfileRead(ini,0,&readback) && readback.enabled);
    assert(ATGOwnedProfileEnable(ini,0,0) && ATGOwnedProfileDelete(ini,0));
    assert(!ATGOwnedProfileRead(ini,0,&readback) && !readback.enabled);
    assert(DeleteFile(ini) && RemoveDirectory(folder));
    puts("native owned profiles: dialog resources, SHA-256 vectors, changed/empty/oversized/missing images, sharing locks and INI persistence passed (no MobaXterm GUI)");
    return 0;
}
