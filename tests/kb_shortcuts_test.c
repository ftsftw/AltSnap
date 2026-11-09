/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef unsigned DWORD;
typedef int HWND;
typedef char TCHAR;
#define TEXT(s) s
#define LOBYTE(w) ((BYTE)(w))
#define HIBYTE(w) ((BYTE)((w)>>8))
#define VK_SHIFT 16
#define VK_CONTROL 17
#define VK_MENU 18
#define VK_LSHIFT 160
#define VK_RSHIFT 161
#define VK_LCONTROL 162
#define VK_RCONTROL 163
#define VK_LMENU 164
#define VK_RMENU 165
#define VK_LWIN 91
#define VK_RWIN 92
#define ERROR_INVALID_DATA 13
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_HOTKEY_ALREADY_REGISTERED 1409
#define ERROR_WRITE_FAULT 29
#define ARR_SZ(a) (sizeof(a)/sizeof((a)[0]))
#define lstrlen strlen
#define lstrchr strchr
#define strtoi atoi
#define lstrcmp strcmp
static int lstrcmpi(const char *a,const char *b)
{ while(*a && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { ++a; ++b; } return tolower((unsigned char)*a)-tolower((unsigned char)*b); }
static void lstrcpy_s(char *out,size_t n,const char *s)
{ assert(strlen(s)<n); strcpy(out,s); }
static char *Uint2lStr(char *out,unsigned n) { sprintf(out,"%u",n); return out; }
static char entries[80][100];
static unsigned used, reads, writes, probes, releases;
static int truncated, fail_write, fail_readback, busy;
static DWORD GetLastError(void) { return 5; }
static DWORD GetPrivateProfileSection(const char *section,char *out,DWORD n,const char *ini)
{
    unsigned i; size_t len=0; (void)ini; assert(!strcmp(section,"KBShortcuts")); ++reads;
    if(truncated) return n-2;
    for(i=0;i<used;++i) { size_t k=strlen(entries[i])+1; if(len+k+1>=n) return n-2; memcpy(out+len,entries[i],k); len+=k; }
    out[len]=0; return (DWORD)len;
}
static int entry(const char *key)
{
    unsigned i; for(i=0;i<used;++i) { char name[100],*eq; strcpy(name,entries[i]); eq=strchr(name,'='); assert(eq); *eq=0; if(!lstrcmpi(name,key)) return (int)i; } return -1;
}
static int WritePrivateProfileString(const char *section,const char *key,const char *val,const char *ini)
{
    int at=entry(key); (void)ini; assert(!strcmp(section,"KBShortcuts")); ++writes;
    if(fail_write) return 0;
    if(!val) { if(at>=0) memmove(entries[at],entries[--used],sizeof entries[0]); return 1; }
    if(at<0) at=(int)used++;
    assert(used<=ARR_SZ(entries)); snprintf(entries[at],sizeof entries[at],"%s=%s",key,val); return 1;
}
static DWORD GetPrivateProfileString(const char *section,const char *key,const char *def,char *out,DWORD n,const char *ini)
{
    int at=entry(key); const char *s=at<0 || fail_readback ? def : strchr(entries[at],'=')+1;
    (void)section; (void)ini; assert(strlen(s)<n); strcpy(out,s); return (DWORD)strlen(s);
}
static int RegisterHotKey(HWND hwnd,int id,unsigned mod,unsigned key)
{ assert(hwnd==1 && id==0xBFFF && mod<=15 && key); ++probes; return !busy; }
static int UnregisterHotKey(HWND hwnd,int id)
{ assert(hwnd==1 && id==0xBFFF); ++releases; return 1; }
#include "../kb_shortcuts.h"
int main(void)
{
    char conflict[64]={0},out[16]; TCHAR *section;
    const char *names[]={"AutoTileGrid","AutoTileExpandBoth","AutoTileExpandHorizontal","AutoTileExpandVertical"};
    const unsigned keys[]={'S','Q','E','W'}; unsigned i,before;
    assert(KBShortcutValid(0) && KBShortcutValid('S') && !KBShortcutValid(3<<8));
    for(i=0;i<256;++i) if(i&~15u) assert(!KBShortcutValid((WORD)((i<<8)|'Q')));
    assert(!KBShortcutValid((3<<8)|VK_CONTROL));
    assert(WritePrivateProfileString("KBShortcuts","UsePtWindow","1","test"));
    /* Preserve legacy G; assigning new chords never migrates existing values. */
    assert(WritePrivateProfileString("KBShortcuts","AutoTileGrid","839","test"));
    for(i=0;i<4;++i) {
        assert(!KBShortcutSave(1,"test",names[i],(WORD)((3<<8)|keys[i]),conflict,sizeof conflict));
        GetPrivateProfileString("KBShortcuts",names[i],"",out,sizeof out,"test");
        assert((unsigned)atoi(out)==((3u<<8)|keys[i]));
    }
    before=writes; assert(KBShortcutSave(1,"test",names[1],(3<<8)|'S',conflict,sizeof conflict)==ERROR_HOTKEY_ALREADY_REGISTERED);
    assert(writes==before && !strcmp(conflict,"AutoTileGrid"));
    busy=1; assert(KBShortcutSave(1,"test",names[1],(3<<8)|'Z',conflict,sizeof conflict)==5 && writes==before); busy=0;
    fail_write=1; assert(KBShortcutSave(1,"test",names[1],(3<<8)|'Z',conflict,sizeof conflict)==ERROR_WRITE_FAULT); fail_write=0;
    fail_readback=1; assert(KBShortcutSave(1,"test",names[1],(3<<8)|'Z',conflict,sizeof conflict)==ERROR_WRITE_FAULT); fail_readback=0;
    for(i=0;i<4;++i) assert(!KBShortcutSave(1,"test",names[i],0,conflict,sizeof conflict));
    assert(used==1 && entry("UsePtWindow")==0);
    assert(probes==releases+1); /* Failed probe was never unregistered. */
    /* Grow past the old fixed 1600/1800-character list without losing actions. */
    for(i=0;i<70;++i) { char key[80]; snprintf(key,sizeof key,"AutoTileExpandHorizontal_0_%040u",i); assert(WritePrivateProfileString("KBShortcuts",key,"0","test")); }
    before=reads; section=KBShortcutSection("test"); assert(section && reads>before+1); free(section);
    truncated=1; before=writes;
    assert(KBShortcutSave(1,"test",names[0],(3<<8)|'S',conflict,sizeof conflict)==ERROR_INSUFFICIENT_BUFFER && writes==before);
    puts("kb_shortcuts: save/read/change/delete, duplicates, registration/write/readback failures and long/truncated sections passed (stub INI/User32)");
    return 0;
}
