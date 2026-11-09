/* Production compatibility lookup against deterministic API stubs, no OS calls. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
typedef uint32_t DWORD;
typedef int BOOL;
typedef uintptr_t HWND;
typedef uintptr_t HANDLE;
typedef void *HMODULE;
typedef intptr_t LPARAM;
#ifdef _UNICODE
typedef wchar_t TCHAR;
#define TEXT(s) L##s
#define tlen wcslen
#define tcpy wcscpy
#define tcmp wcscmp
#define trchr wcsrchr
#else
typedef char TCHAR;
#define TEXT(s) s
#define tlen strlen
#define tcpy strcpy
#define tcmp strcmp
#define trchr strrchr
#endif
typedef TCHAR *LPTSTR;
#define CAP(a) (sizeof(a)/sizeof((a)[0]))
#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define IPTR ((void (*)(void))1)
#define PROCESS_QUERY_INFORMATION 1024u
#define PROCESS_VM_READ 16u
#define PROCESS_QUERY_LIMITED_INFORMATION 4096u
#define ERROR_PROC_NOT_FOUND 127u

static DWORD last_error;
static unsigned error_reads, error_writes, traces, opens, closes, modules, images, limited;
static unsigned enumerations, live_handles;
static unsigned child_kind, window_fail, zero_pid, host, fail_child;
static unsigned fail_vm, fail_query, fail_module, fail_image, at_capacity;
static unsigned fail_limited_open, fail_limited_image, malformed, unicode_path, fail_host_reread, fail_child_image;
static char operations[256], trace_log[4096];
static size_t operation_used, trace_used;
static void op(char c) { assert(operation_used+1<sizeof operations); operations[operation_used++]=c; operations[operation_used]=0; }
static void SetLastError(DWORD error) { ++error_writes; last_error=error; }
static DWORD GetLastError(void) { ++error_reads; return last_error; }
static DWORD GetWindowThreadProcessId(HWND hwnd, DWORD *pid)
{
    op('T');
    if (window_fail) { last_error=1400; return 0; }
    *pid=zero_pid ? 0 : hwnd==2 ? 200 : 100;
    return 10;
}
static HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid)
{
    assert(!inherit && (pid==100 || pid==200)); ++opens;
    if (access==(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ)) {
        op('V'); if (fail_vm || (pid==200 && fail_child) || (fail_host_reread && enumerations)) { last_error=5; return 0; }
    } else if (access==PROCESS_QUERY_INFORMATION) {
        op('Q'); if (fail_query || (pid==200 && fail_child) || (fail_host_reread && enumerations)) { last_error=5; return 0; }
    } else {
        assert(access==PROCESS_QUERY_LIMITED_INFORMATION); op('L');
        if (fail_limited_open || (pid==200 && fail_child) || (fail_host_reread && enumerations)) { last_error=5; return 0; }
    }
    assert(!live_handles); ++live_handles;
    return pid;
}
static BOOL CloseHandle(HANDLE proc)
{ assert((proc==100 || proc==200) && live_handles==1); --live_handles; ++closes; op('C'); return 1; }
static BOOL EnumChildWindows(HWND hwnd, BOOL (*callback)(HWND, LPARAM), LPARAM lp)
{
    assert(hwnd==1); ++enumerations; op('E');
    if (child_kind==1) callback(1,lp); /* Same process: not selected. */
    if (child_kind==2) callback(2,lp);
    return 1;
}
#if !defined(PROCESS_NAME_MISSING_APIS)
static DWORD copy_name(HANDLE proc, TCHAR *name, DWORD size)
{
    const TCHAR *path=host && proc==100 ? TEXT("C:\\System\\ApplicationFrameHost.exe") : TEXT("C:\\Apps\\test.exe");
    DWORD length;
#ifdef _UNICODE
    if (unicode_path) path=TEXT("C:\\Apps\\\u0442\u0435\u0441\u0442.exe");
#endif
    length=(DWORD)tlen(path);
    if (size<=length) {
        DWORD i;
        for (i=0;i+1<size;++i) name[i]=path[i];
        name[size-1]=0;
        return size;
    }
    tcpy(name,path);
    if (malformed==1) name[length]=TEXT('x'); /* Missing terminator. */
    if (malformed==2) name[3]=0;             /* Embedded terminator. */
    if (malformed==3) { name[length-1]=TEXT('\\'); } /* Empty basename. */
    return at_capacity ? size : length;
}
#ifndef PROCESS_NAME_MISSING_PSAPI
static DWORD module_query(HANDLE proc, HMODULE module, LPTSTR name, DWORD size)
{
    assert(!module); ++modules; op('M');
    if (proc==200 && fail_child_image) { name[0]=TEXT('!'); last_error=299; return 0; }
    if (fail_module) { if (fail_module==1) last_error=299; return 0; }
    return copy_name(proc,name,size);
}
static DWORD image_query(HANDLE proc, TCHAR *name, DWORD size)
{
    ++images; op('I');
    if (proc==200 && fail_child_image) { name[0]=TEXT('!'); last_error=5; return 0; }
    if (fail_image) { if (fail_image==1) last_error=5; return 0; }
    return copy_name(proc,name,size);
}
#endif
#ifndef PROCESS_NAME_MISSING_LIMITED
static BOOL limited_query(HANDLE proc, DWORD flags, LPTSTR name, DWORD *size)
{
    DWORD length;
    assert(!flags && live_handles==1); ++limited; op('F');
    if (fail_limited_image || (proc==200 && fail_child_image)) {
        name[0]=TEXT('!'); /* Failed APIs need not preserve the caller's buffer. */
        if (fail_limited_image!=2) last_error=fail_limited_image==3 ? 122 : 5;
        return 0;
    }
    length=copy_name(proc,name,*size);
    if (length>=*size && !at_capacity) { last_error=122; return 0; }
    *size=malformed==4 ? 0 : malformed==5 ? *size+1 : length;
    return 1;
}
#endif
#endif
static void (*LoadDLLProc(const char *dll, const char *name))(void)
{
    last_error=7777; /* Loader diagnostics must not leak into API failures. */
    if (!strcmp(dll,"PSAPI.DLL")) {
#ifdef _UNICODE
        assert(!strcmp(name,"GetModuleFileNameExW") || !strcmp(name,"GetProcessImageFileNameW"));
#else
        assert(!strcmp(name,"GetModuleFileNameExA") || !strcmp(name,"GetProcessImageFileNameA"));
#endif
#if defined(PROCESS_NAME_MISSING_APIS) || defined(PROCESS_NAME_MISSING_PSAPI)
        return NULL;
#else
        return !strncmp(name,"GetModule",9) ? (void (*)(void))module_query : (void (*)(void))image_query;
#endif
    }
    assert(!strcmp(dll,"KERNEL32.DLL"));
#ifdef _UNICODE
    assert(!strcmp(name,"QueryFullProcessImageNameW"));
#else
    assert(!strcmp(name,"QueryFullProcessImageNameA"));
#endif
#if defined(PROCESS_NAME_MISSING_APIS) || defined(PROCESS_NAME_MISSING_LIMITED)
    return NULL;
#else
    return (void (*)(void))limited_query;
#endif
}
static size_t PathGetFileNameStart(const TCHAR *name)
{
    const TCHAR *p=trchr(name,TEXT('\\')); return p ? (size_t)(p-name+1) : 0;
}
static void PathStripPathL(TCHAR *name)
{ size_t i=PathGetFileNameStart(name); memmove(name,name+i,(tlen(name+i)+1)*sizeof *name); }
static int lstrcmpi(const TCHAR *a, const TCHAR *b) { return tcmp(a,b); }
#include "../window_process_name.h"

static void trace(const char *stage, DWORD error)
{
    int written;
    ++traces;
    written=snprintf(trace_log+trace_used,sizeof trace_log-trace_used,"%s:%lu\n",stage,(unsigned long)error);
    assert(written>0 && (size_t)written<sizeof trace_log-trace_used); trace_used+=(size_t)written;
    /* Debug output can overwrite LastError. The next API must capture its own. */
    last_error=9999;
}
static void reset(void)
{
    assert(!live_handles);
    last_error=9999;
    error_reads=error_writes=traces=opens=closes=modules=images=limited=enumerations=0;
    child_kind=window_fail=zero_pid=host=fail_child=0;
    fail_vm=fail_query=fail_module=fail_image=at_capacity=0;
    fail_limited_open=fail_limited_image=malformed=unicode_path=fail_host_reread=fail_child_image=0;
    operation_used=trace_used=0; operations[0]=trace_log[0]=0;
}
int main(void)
{
    TCHAR name[260];
    unsigned enabled;
#if defined(PROCESS_NAME_MISSING_APIS) || defined(PROCESS_NAME_MISSING_LIMITED) || defined(PROCESS_NAME_MISSING_PSAPI)
    for (enabled=0;enabled<=1;++enabled) {
        DWORD result;
        reset();
#ifdef PROCESS_NAME_MISSING_LIMITED
        fail_module=fail_image=1;
#endif
        result=GetWindowProgNameTraced(1,name,CAP(name),enabled ? trace : NULL);
#ifdef PROCESS_NAME_MISSING_PSAPI
        assert(result==100 && !tcmp(name,TEXT("test.exe")));
        assert(!strcmp(operations,"TVCLFC") && closes==2 && limited==1);
#else
        assert(!result && !name[0]);
        assert(opens==1 && closes==1 && !limited);
        if (enabled) assert(strstr(trace_log,"process-limited-api-missing:127\n"));
#endif
#ifdef PROCESS_NAME_MISSING_APIS
        assert(!modules && !images);
        if (enabled) {
            assert(strstr(trace_log,"process-module-failed:127\n"));
            assert(strstr(trace_log,"process-image-failed:127\n"));
        }
#endif
        assert(!live_handles);
        if (!enabled) assert(!error_reads && !error_writes && !traces);
    }
#else
    unsigned kind;
    const char *expected_operations[]={
        "TVMC", "TVQIC", "TVQLFC", "TVMIC", "TVMICLFC", "T", "T",
        "TVMCETVMC", "TVMCETVQLVMC", "TVMCET", "TVMICLFC", "TVMICLFC", "TVMICLFC",
        "TVQL", "TVQLFC", "TVQLFC", "TVQLFC", "TVMICLFC", "TVMICLFC", "TVMICLFC",
        "TVQLFC", "TVQLFC", "TVQLFC", "TVQLFCETVQLFC", "TVMCETVQLVQL", "TVMC",
        "TVMCETVMICLFCVMC"
    };
    for (kind=0;kind<CAP(expected_operations);++kind) {
        char untraced_operations[256]; TCHAR untraced_name[260];
        DWORD untraced_result=0;
        for (enabled=0;enabled<=1;++enabled) {
            DWORD result, expected=100;
            unsigned expected_closes=1;
            reset(); name[0]=0;
            if (kind==1) fail_vm=1;
            if (kind==2 || (kind>=13 && kind<=16) || (kind>=20 && kind<=23)) fail_vm=fail_query=1;
            if (kind==3) fail_module=1;
            if (kind==4) fail_module=fail_image=1;
            if (kind==5) window_fail=1;
            if (kind==6) zero_pid=1;
            if (kind==7) { host=1; child_kind=2; }
            if (kind==8) { host=1; child_kind=2; fail_child=1; }
            if (kind==9) { host=1; child_kind=1; }
            if (kind==10) at_capacity=1;
            if (kind==11) { fail_module=2; fail_image=1; fail_limited_image=1; }
            if (kind==12) { fail_module=1; fail_image=2; fail_limited_image=1; }
            if (kind==13) fail_limited_open=1;
            if (kind==14) fail_limited_image=1;
            if (kind==15) fail_limited_image=3; /* ERROR_INSUFFICIENT_BUFFER */
            if (kind==16) fail_limited_image=2; /* Failure without setting error. */
            if (kind>=17 && kind<=19) malformed=kind-16;
            if (kind==20) malformed=4;
            if (kind==21) malformed=5;
            if (kind==22) at_capacity=1;
            if (kind==23) { host=1; child_kind=2; }
            if (kind==24) { host=1; child_kind=2; fail_child=fail_host_reread=1; }
            if (kind==25) unicode_path=1;
            if (kind==26) { host=1; child_kind=2; fail_child_image=1; }
            result=GetWindowProgNameTraced(1,name,CAP(name),enabled ? trace : NULL);
            if (kind==5 || kind==6 || (kind>=10 && kind<=22) || kind==24) expected=0;
            if (kind==7 || kind==23) expected=200;
            if (kind==4 || kind==7 || kind==8 || (kind>=10 && kind<=12) || (kind>=17 && kind<=19) || kind==23) expected_closes=2;
            if (kind==5 || kind==6 || kind==13) expected_closes=0;
            if (kind==26) expected_closes=4;
            assert(!strcmp(operations,expected_operations[kind]));
            assert(result==expected && closes==expected_closes && !live_handles);
            if (!result) assert(!name[0]);
            if (kind==8 || kind==26) assert(!tcmp(name,TEXT("ApplicationFrameHost.exe")));
#ifdef _UNICODE
            if (kind==25) assert(!tcmp(name,TEXT("\u0442\u0435\u0441\u0442.exe")));
#endif
            assert(!strstr(trace_log,"test.exe") && !strstr(trace_log,"ApplicationFrameHost.exe"));
            assert(!strstr(trace_log,"C:\\") && !strstr(trace_log,"100") && !strstr(trace_log,"200"));
            if (!enabled) {
                assert(!error_reads && !error_writes && !traces);
                strcpy(untraced_operations,operations); tcpy(untraced_name,name); untraced_result=result;
            } else {
                assert(!strcmp(untraced_operations,operations));
                assert(!tcmp(untraced_name,name) && untraced_result==result);
                if (kind==1 || kind==2) assert(strstr(trace_log,"process-open-vm-failed:5\n"));
                if (kind==2 || kind==8) assert(strstr(trace_log,"process-open-query-failed:5\n"));
                if (kind==3 || kind==4) assert(strstr(trace_log,"process-module-failed:299\n"));
                if (kind==4) assert(strstr(trace_log,"process-image-failed:5\n"));
                if (kind==5) assert(strstr(trace_log,"process-window-thread-failed:1400\n"));
                if (kind==6) assert(strstr(trace_log,"process-window-id-missing:0\n"));
                if (kind==7 || kind==23) assert(strstr(trace_log,"process-host-child-used:0\n"));
                if (kind==8 || kind==24 || kind==26) assert(strstr(trace_log,"process-host-child-failed:0\n"));
                if (kind==9) assert(strstr(trace_log,"process-host-no-child:0\n"));
                if (kind==10 || kind==22) assert(strstr(trace_log,"process-name-at-capacity:0\n"));
                if (kind==11) assert(strstr(trace_log,"process-module-failed:0\n") && strstr(trace_log,"process-image-failed:5\n"));
                if (kind==12) assert(strstr(trace_log,"process-module-failed:299\n") && strstr(trace_log,"process-image-failed:0\n"));
                if (kind==2 || kind==4 || kind==23) assert(strstr(trace_log,"process-limited-used:0\n"));
                if (kind==13) assert(strstr(trace_log,"process-open-limited-failed:5\n"));
                if (kind==14) assert(strstr(trace_log,"process-limited-image-failed:5\n"));
                if (kind==15) assert(strstr(trace_log,"process-limited-image-failed:122\n"));
                if (kind==16) assert(strstr(trace_log,"process-limited-image-failed:0\n"));
                if (kind>=17 && kind<=22) assert(strstr(trace_log,"process-limited-invalid:0\n"));
            }
        }
    }
    for (enabled=0;enabled<=1;++enabled) {
        struct { TCHAR before, name[5], after; } guarded;
        reset(); guarded.before=guarded.after=TEXT('G');
        assert(!GetWindowProgNameTraced(1,guarded.name,CAP(guarded.name),enabled ? trace : NULL));
        assert(!guarded.name[0] && guarded.before==TEXT('G') && guarded.after==TEXT('G'));
        assert(!strcmp(operations,"TVMICLFC") && closes==2 && !live_handles);
        if (enabled) assert(strstr(trace_log,"process-limited-image-failed:122\n"));
        else assert(!error_reads && !error_writes && !traces);
        /* Exact fit (characters, not bytes): terminator occupies the last slot. */
        reset(); fail_vm=fail_query=1;
        assert(GetWindowProgNameTraced(1,name,tlen(TEXT("C:\\Apps\\test.exe"))+1,enabled ? trace : NULL)==100);
        assert(!tcmp(name,TEXT("test.exe")) && limited==1 && closes==1 && !live_handles);
    }
    reset();
    assert(!GetWindowProgName(1,NULL,260) && !opens);
    assert(!GetWindowProgName(1,name,0) && !opens);
    assert(!GetWindowProgName(1,name,1) && !opens && !name[0]);
#if SIZE_MAX > UINT32_MAX
    assert(!GetWindowProgName(1,name,(size_t)UINT32_MAX+1) && !opens);
#endif
#endif
    /* The original entry points used by blacklists share the same fallback. */
    reset();
#if !defined(PROCESS_NAME_MISSING_APIS) && !defined(PROCESS_NAME_MISSING_LIMITED)
    fail_vm=fail_query=1;
    assert(GetWindowProgName(1,name,CAP(name))==100 && !tcmp(name,TEXT("test.exe")));
    assert(GetWindowProgNameFullPath(1,name,CAP(name))==100 && !tcmp(name,TEXT("C:\\Apps\\test.exe")));
    assert(limited==2 && closes==2 && !live_handles);
#else
    fail_vm=fail_query=1;
    assert(!GetWindowProgName(1,name,CAP(name)) && !name[0]);
    assert(!GetWindowProgNameFullPath(1,name,CAP(name)) && !name[0]);
#endif
    assert(!error_reads && !error_writes && !traces);
    puts("window_process_name: legacy/limited lookup, ANSI/Unicode API contract, refusals, UWP, handle closure and opt-in trace passed (no Windows GUI)");
    return 0;
}
