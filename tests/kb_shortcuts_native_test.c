/* SPDX-License-Identifier: GPL-3.0-or-later
   Real User32 combobox/hotkey and UTF-16 INI contracts; no AltSnap desktop moves. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(c) do { if(!(c)) { fprintf(stderr,"native shortcut line %d error %lu\n",__LINE__,(unsigned long)GetLastError()); exit(1); } } while(0)
#define ARR_SZ(a) (sizeof(a)/sizeof((a)[0]))
#define lstrchr _tcschr
#define strtoi _ttoi
static void lstrcpy_s(TCHAR *out,size_t n,const TCHAR *s)
{ assert((size_t)lstrlen(s)<n); lstrcpy(out,s); }
static TCHAR *Uint2lStr(TCHAR *out,unsigned n) { wsprintf(out,TEXT("%u"),n); return out; }
#include "../kb_shortcuts.h"
#define CB_ResetContent(h) SendMessage(h,CB_RESETCONTENT,0,0)
#define CB_AddString(h,s) SendMessage(h,CB_ADDSTRING,0,(LPARAM)(s))
static LRESULT CB_AddStringIfNotPresentAndGetIdx(HWND h,const TCHAR *s)
{ LRESULT i=SendMessage(h,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)s); return i<0 ? CB_AddString(h,s) : i; }
static LRESULT CB_GetTextAndIdx(HWND h,TCHAR *out,size_t n)
{
    LRESULT i=SendMessage(h,CB_GETCURSEL,0,0);
    if(i<0) GetWindowText(h,out,(int)n);
    else { assert((size_t)SendMessage(h,CB_GETLBTEXTLEN,i,0)<n); SendMessage(h,CB_GETLBTEXT,i,(LPARAM)out); }
    return i;
}
static size_t lstrcpy_noaccel(TCHAR *out,size_t n,const TCHAR *s)
{ lstrcpy_s(out,n,s); return (size_t)lstrlen(out); }
typedef struct { const TCHAR *action; short l10nidx; } actiondl_t;
static const TCHAR *labels[4];
#define L10NSTR(i) labels[i]
#include "../kb_shortcuts_ui.h"
static const actiondl_t actions[]={
    {TEXT("AutoTileGrid"),0}, {TEXT("AutoTileExpandBoth"),1},
    {TEXT("AutoTileExpandHorizontal"),2}, {TEXT("AutoTileExpandVertical"),3}
};
static DWORD WINAPI hold_chord(void *lp)
{
    HANDLE *events=(HANDLE *)lp;
    assert(RegisterHotKey(NULL,123,15,VK_F23));
    SetEvent(events[0]); assert(WaitForSingleObject(events[1],10000)==WAIT_OBJECT_0);
    assert(UnregisterHotKey(NULL,123)); return 0;
}
int main(void)
{
    TCHAR temp[MAX_PATH],ini[MAX_PATH],out[128],conflict[128]={0};
    HWND parent,combo; unsigned i,lang; WORD bom=0xfeff; DWORD written;
    HANDLE file,thread,events[2];
    assert(GetTempPath(ARR_SZ(temp),temp));
    assert(GetTempFileName(temp,TEXT("ask"),0,ini));
    file=CreateFile(ini,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(file!=INVALID_HANDLE_VALUE && WriteFile(file,&bom,sizeof bom,&written,NULL) && written==sizeof bom);
    assert(CloseHandle(file));
    parent=CreateWindowEx(0,TEXT("STATIC"),TEXT("shortcut-contract"),WS_OVERLAPPED,0,0,400,300,NULL,NULL,NULL,NULL);
    assert(parent);
    combo=CreateWindowEx(0,TEXT("COMBOBOX"),NULL,WS_CHILD|CBS_DROPDOWN,0,0,200,200,parent,NULL,NULL,NULL);
    assert(combo);
    for(lang=0;lang<2;++lang) {
        for(i=0;i<4;++i) labels[i]=lang ? TEXT("\u0421\u0435\u0442\u043a\u0430") : TEXT("Grid");
        /* Distinct labels are supplied by the localization table in the app. */
        labels[1]=TEXT("Expand both"); labels[2]=TEXT("Expand width"); labels[3]=TEXT("Expand height");
        assert(KBShortcutFill(combo,actions,4,ini));
        assert(SendMessage(combo,CB_GETCOUNT,0,0)==4); /* No assignments required. */
        for(i=0;i<4;++i) {
            assert(SendMessage(combo,CB_SETCURSEL,i,0)==(LRESULT)i);
            assert(KBShortcutAction(combo,actions,4,out,ARR_SZ(out))==(int)i);
            assert(!lstrcmp(out,actions[i].action));
            assert(KBShortcutAddAction(combo,actions,4,out)==(int)i);
        }
    }
    assert(WritePrivateProfileString(TEXT("KBShortcuts"),TEXT("UsePtWindow"),TEXT("1"),ini));
    assert(WritePrivateProfileString(TEXT("KBShortcuts"),TEXT("AutoTileGrid"),TEXT("839"),ini));
    assert(WritePrivateProfileString(TEXT("KBShortcuts"),TEXT("Step_L_40"),TEXT("0"),ini));
    assert(KBShortcutFill(combo,actions,4,ini));
    assert(SendMessage(combo,CB_GETCOUNT,0,0)==5);
    assert(SendMessage(combo,CB_SETCURSEL,4,0)==4);
    assert(KBShortcutAction(combo,actions,4,out,ARR_SZ(out))==4 && !lstrcmp(out,TEXT("Step_L_40")));
    /* Save, flush/re-read, change and remove each canonical key. */
    for(i=0;i<4;++i) {
        WORD hk=(WORD)((15<<8)|(VK_F19+i));
        assert(!KBShortcutSave(combo,ini,actions[i].action,hk,conflict,ARR_SZ(conflict)));
        WritePrivateProfileString(NULL,NULL,NULL,ini);
        assert(GetPrivateProfileInt(TEXT("KBShortcuts"),actions[i].action,0,ini)==hk);
    }
    assert(KBShortcutSave(combo,ini,actions[1].action,(15<<8)|VK_F19,conflict,ARR_SZ(conflict))==ERROR_HOTKEY_ALREADY_REGISTERED);
    assert(!lstrcmp(conflict,actions[0].action));
    events[0]=CreateEvent(NULL,TRUE,FALSE,NULL); events[1]=CreateEvent(NULL,TRUE,FALSE,NULL); assert(events[0] && events[1]);
    thread=CreateThread(NULL,0,hold_chord,events,0,NULL); assert(thread);
    assert(WaitForSingleObject(events[0],10000)==WAIT_OBJECT_0);
    assert(KBShortcutSave(combo,ini,actions[1].action,(15<<8)|VK_F23,conflict,ARR_SZ(conflict))!=0);
    assert(GetPrivateProfileInt(TEXT("KBShortcuts"),actions[1].action,0,ini)==((15<<8)|VK_F20));
    assert(SetEvent(events[1]) && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);
    assert(CloseHandle(thread) && CloseHandle(events[0]) && CloseHandle(events[1]));
    for(i=0;i<4;++i) assert(!KBShortcutSave(combo,ini,actions[i].action,0,conflict,ARR_SZ(conflict)));
    assert(GetPrivateProfileInt(TEXT("KBShortcuts"),TEXT("UsePtWindow"),0,ini)==1);
    assert(KBShortcutFill(combo,actions,4,ini) && SendMessage(combo,CB_GETCOUNT,0,0)==5);
    assert(DestroyWindow(parent) && DeleteFile(ini));
    puts("native shortcuts: real combobox canonical/localized mapping, unassigned actions, INI persistence/delete, duplicate and occupied-chord refusal passed (no AltSnap desktop moves)");
    return 0;
}
