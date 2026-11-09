/* Production compatibility lookup against deterministic API stubs, no OS calls. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD;
typedef int BOOL;
typedef uintptr_t HWND;
typedef uintptr_t HANDLE;
typedef void *HMODULE;
typedef intptr_t LPARAM;
typedef char TCHAR;
typedef char *LPTSTR;
#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define TEXT(s) s
#define IPTR ((void (*)(void))1)
#define PROCESS_QUERY_INFORMATION 1024u
#define PROCESS_VM_READ 16u
#define ERROR_PROC_NOT_FOUND 127u

static DWORD last_error;
static unsigned error_reads, error_writes, traces, opens, closes, modules, images, enumerations;
static unsigned child_kind, window_fail, zero_pid, host, fail_child;
static unsigned fail_vm, fail_query, fail_module, fail_image, at_capacity;
static char operations[128], trace_log[2048];
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
        op('V'); if (fail_vm || (pid==200 && fail_child)) { last_error=5; return 0; }
    } else {
        assert(access==PROCESS_QUERY_INFORMATION); op('Q');
        if (fail_query || (pid==200 && fail_child)) { last_error=5; return 0; }
    }
    return pid;
}
static BOOL CloseHandle(HANDLE proc) { assert(proc==100 || proc==200); ++closes; op('C'); return 1; }
static BOOL EnumChildWindows(HWND hwnd, BOOL (*callback)(HWND, LPARAM), LPARAM lp)
{
    assert(hwnd==1); ++enumerations; op('E');
    if (child_kind==1) callback(1,lp); /* Same process: not selected. */
    if (child_kind==2) callback(2,lp);
    return 1;
}
#ifndef PROCESS_NAME_MISSING_APIS
static DWORD copy_name(HANDLE proc, TCHAR *name, DWORD size)
{
    const char *path=host && proc==100 ? "C:\\System\\ApplicationFrameHost.exe" : "C:\\Apps\\test.exe";
    assert(size>strlen(path)); strcpy(name,path);
    return at_capacity ? size : (DWORD)strlen(path);
}
static DWORD module_query(HANDLE proc, HMODULE module, LPTSTR name, DWORD size)
{
    assert(!module); ++modules; op('M');
    if (fail_module) { if (fail_module==1) last_error=299; return 0; }
    return copy_name(proc,name,size);
}
static DWORD image_query(HANDLE proc, TCHAR *name, DWORD size)
{
    ++images; op('I');
    if (fail_image) { if (fail_image==1) last_error=5; return 0; }
    return copy_name(proc,name,size);
}
#endif
static void (*LoadDLLProc(const char *dll, const char *name))(void)
{
    assert(!strcmp(dll,"PSAPI.DLL"));
    last_error=7777; /* Loader diagnostics must not leak into API failures. */
#ifdef PROCESS_NAME_MISSING_APIS
    (void)name; return NULL;
#else
    if (!strcmp(name,"GetModuleFileNameExA")) return (void (*)(void))module_query;
    assert(!strcmp(name,"GetProcessImageFileNameA")); return (void (*)(void))image_query;
#endif
}
static size_t PathGetFileNameStart(const TCHAR *name)
{
    const char *p=strrchr(name,'\\'); return p ? (size_t)(p-name+1) : 0;
}
static void PathStripPathL(TCHAR *name)
{ size_t i=PathGetFileNameStart(name); memmove(name,name+i,strlen(name+i)+1); }
static int lstrcmpi(const TCHAR *a, const TCHAR *b) { return strcmp(a,b); }
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
    last_error=9999;
    error_reads=error_writes=traces=opens=closes=modules=images=enumerations=0;
    child_kind=window_fail=zero_pid=host=fail_child=0;
    fail_vm=fail_query=fail_module=fail_image=at_capacity=0;
    operation_used=trace_used=0; operations[0]=trace_log[0]=0;
}
int main(void)
{
    TCHAR name[260];
    unsigned enabled;
#ifdef PROCESS_NAME_MISSING_APIS
    for (enabled=0;enabled<=1;++enabled) {
        reset();
        assert(!GetWindowProgNameTraced(1,name,sizeof name,enabled ? trace : NULL));
        assert(opens==1 && closes==1 && !modules && !images);
        if (enabled) {
            assert(strstr(trace_log,"process-module-failed:127\n"));
            assert(strstr(trace_log,"process-image-failed:127\n"));
        } else assert(!error_reads && !error_writes && !traces);
    }
#else
    unsigned kind;
    const char *expected_operations[]={
        "TVMC", "TVQIC", "TVQ", "TVMIC", "TVMIC", "T", "T",
        "TVMCETVMC", "TVMCETVQ", "TVMCET", "TVMC", "TVMIC", "TVMIC"
    };
    for (kind=0;kind<13;++kind) {
        char untraced_operations[128], untraced_name[260];
        DWORD untraced_result=0;
        for (enabled=0;enabled<=1;++enabled) {
            DWORD result;
            reset(); name[0]=0;
            if (kind==1) fail_vm=1;
            if (kind==2) fail_vm=fail_query=1;
            if (kind==3) fail_module=1;
            if (kind==4) fail_module=fail_image=1;
            if (kind==5) window_fail=1;
            if (kind==6) zero_pid=1;
            if (kind==7) { host=1; child_kind=2; }
            if (kind==8) { host=1; child_kind=2; fail_child=1; }
            if (kind==9) { host=1; child_kind=1; }
            if (kind==10) at_capacity=1;
            if (kind==11) { fail_module=2; fail_image=1; }
            if (kind==12) { fail_module=1; fail_image=2; }
            result=GetWindowProgNameTraced(1,name,sizeof name,enabled ? trace : NULL);
            assert(!strcmp(operations,expected_operations[kind]));
            assert(result==(kind==2 || kind==4 || kind==5 || kind==6 || kind>=11 ? 0u : kind==7 ? 200u : 100u));
            assert(closes==(kind==2 || kind==5 || kind==6 ? 0u : kind==7 ? 2u : 1u));
            assert(!strstr(trace_log,"test.exe") && !strstr(trace_log,"ApplicationFrameHost.exe"));
            assert(!strstr(trace_log,"C:\\") && !strstr(trace_log,"100") && !strstr(trace_log,"200"));
            if (!enabled) {
                assert(!error_reads && !error_writes && !traces);
                strcpy(untraced_operations,operations); strcpy(untraced_name,name); untraced_result=result;
            } else {
                assert(!strcmp(untraced_operations,operations));
                assert(!strcmp(untraced_name,name) && untraced_result==result);
                if (kind==1 || kind==2) assert(strstr(trace_log,"process-open-vm-failed:5\n"));
                if (kind==2 || kind==8) assert(strstr(trace_log,"process-open-query-failed:5\n"));
                if (kind==3 || kind==4) assert(strstr(trace_log,"process-module-failed:299\n"));
                if (kind==4) assert(strstr(trace_log,"process-image-failed:5\n"));
                if (kind==5) assert(strstr(trace_log,"process-window-thread-failed:1400\n"));
                if (kind==6) assert(strstr(trace_log,"process-window-id-missing:0\n"));
                if (kind==7) assert(strstr(trace_log,"process-host-child-used:0\n"));
                if (kind==8) assert(strstr(trace_log,"process-host-child-failed:0\n"));
                if (kind==9) assert(strstr(trace_log,"process-host-no-child:0\n"));
                if (kind==10) assert(strstr(trace_log,"process-name-at-capacity:0\n"));
                if (kind==11) assert(strstr(trace_log,"process-module-failed:0\n") && strstr(trace_log,"process-image-failed:5\n"));
                if (kind==12) assert(strstr(trace_log,"process-module-failed:299\n") && strstr(trace_log,"process-image-failed:0\n"));
            }
        }
    }
#endif
    /* The original entry points remain usable by blacklist and other callers. */
    reset();
#ifdef PROCESS_NAME_MISSING_APIS
    assert(!GetWindowProgName(1,name,sizeof name));
    assert(!GetWindowProgNameFullPath(1,name,sizeof name));
#else
    assert(GetWindowProgName(1,name,sizeof name)==100 && !strcmp(name,"test.exe"));
    assert(GetWindowProgNameFullPath(1,name,sizeof name)==100 && !strcmp(name,"C:\\Apps\\test.exe"));
#endif
    assert(!error_reads && !error_writes && !traces);
    puts("window_process_name: legacy API order, errors, access rights, handle closure, opt-in trace and wrappers passed (no Windows GUI)");
    return 0;
}
