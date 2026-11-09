/* SPDX-License-Identifier: GPL-3.0-or-later
   Production AutoTileGrid adapter against a deterministic Win32 model.
   This does not model Windows GUI, DPI virtualization or real DWM timing. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef unsigned UINT;
typedef int BOOL;
typedef uintptr_t HWND;
typedef uintptr_t HMONITOR;
typedef uintptr_t HDWP;
typedef intptr_t LPARAM;
typedef intptr_t LONG_PTR;
typedef char TCHAR;
typedef TCHAR *LPTSTR;
typedef void *HMODULE;
typedef uintptr_t HANDLE;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { unsigned cbSize; RECT rcMonitor, rcWork; } MONITORINFO;
typedef struct { UINT length, flags, showCmd; POINT ptMinPosition, ptMaxPosition; RECT rcNormalPosition; } WINDOWPLACEMENT;
#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#define MONITOR_DEFAULTTONULL 0
#define MDT_DEFAULT 0
#define GWL_STYLE 0
#define GWL_EXSTYLE 1
#define WS_CHILD 1
#define WS_THICKFRAME 2
#define WS_CAPTION 48
#define SW_SHOWNORMAL 1
#define SW_SHOWMAXIMIZED 3
#define SW_SHOWNOACTIVATE 4
#define WS_EX_TOOLWINDOW 4
#define WS_EX_NOACTIVATE 8
#define WS_POPUP 64
#define WS_EX_APPWINDOW 128
#define WS_EX_DLGMODALFRAME 256
#define PROCESS_QUERY_INFORMATION 1024u
#define PROCESS_VM_READ 16u
#define IPTR ((void (*)(void))1)
#define ERROR_PROC_NOT_FOUND 127u
#define GW_OWNER 4
#define GA_ROOT 2
#define GA_ROOTOWNER 3
#define SWP_NOACTIVATE 16
#define SWP_NOZORDER 4
#define SWP_NOOWNERZORDER 512
#define MAX_PATH 260
#define TEXT(s) s
#define APP_MOVEONOFF 1
#define APP_ROLLED 2
#define ARR_SZ(a) (sizeof(a)/sizeof((a)[0]))
#define mem00(p,n) memset(p,0,n)
#define larger(a,b) ((a)>(b)?(a):(b))

static struct { struct { int ac; } action; } state;
static struct { HWND hwnd; } LastWin;
static int g_InMouseMove, g_InFinishMovement;
static struct { int busy; } linked_queue;
static struct { int pending; } linked_move;
typedef struct { unsigned length; const void *items; unsigned kind; } blacklist_t;
static struct { blacklist_t Processes, Windows, Bottommost, Snaplist; } BlkLst;
static LONG generation;
static unsigned count, captures, positions, restores, batches, clears;
static int fail_batch, fail_defer, cancel_batch, start_drag, change_preflight;
static int external_restore, replace_restore, reverse_enum, batch_effect, restore_fail;
static unsigned normalizes, max_restores, trace_lines;
static int fail_normalize, fail_max_restore, normalize_effect, normalize_effect_at;
static int placement_fail, change_placement_preflight, fail_begin;
static unsigned frame_reads_after_normalize;
static unsigned diagnostic_queries, traced_process_calls, enum_repeats;
static unsigned process_opens, process_closes, limited_queries;
static HWND process_window;
static DWORD last_error;
static unsigned owner_queries, owner_link_queries;
static int owner_exists, owned_exists, owner_race, owner_style_fail;
static HWND relation_root, relation_rootowner, relation_popup;
static HWND foreground;
static unsigned selected_scans;
static int selected_scan_fail, selected_effect, selected_effect_at;
static int class_query_fail, thread_changed, owner_thread_changed;
static int selected_batch_effect;
static int selected_dialog_during_limits;
static int ordinary_dialog_during_limits;
static int profile_limit_effect, profile_batch_effect;
static unsigned iconic_reads, iconic_flip_at, capture_flip_at;
static unsigned image_epoch;
static const char *model_class, *model_image, *model_owner_class;
static int enumerate_owner;
static unsigned foreground_queries;
static char trace_log[16384];
static size_t trace_used;
static int wsprintfA(char *buf, const char *fmt, ...)
{
    int n; va_list args; va_start(args,fmt); n=vsprintf(buf,fmt,args); va_end(args); return n;
}
static void OutputDebugStringA(const char *line)
{
    size_t n=strlen(line); ++trace_lines;
    assert(trace_used+n+1<sizeof trace_log); memcpy(trace_log+trace_used,line,n+1); trace_used+=n;
}
static RECT monitor_rect = {0, 0, 1200, 1000};
static RECT work_rect = {0, 0, 1200, 960};
static struct FakeWindow {
    RECT native, original;
    WINDOWPLACEMENT placement, original_placement;
    LONG_PTR style, exstyle;
    HMONITOR monitor;
    DWORD pid;
    int aware, dpi, enabled, visible, owned, zoomed, fullscreen;
    int blacklisted, prop, dwm_fail, min_fail, identity_fail, behavior;
    int cloaked, zero, iconic, borderless, snaplisted, snap_changed;
    int shell_kind, class_kind, name_fail, altsnap_exe, limited_only;
    POINT lo, hi;
    const char *class_override, *path_override;
    int root_self;
} fake[6], fake_owner, fake_owner2;
static RECT targets[6];
static unsigned batch_mask;
static struct FakeWindow *win(HWND w) {
    if (w==102) { ++owner_queries; return &fake_owner; }
    if (w==103) { ++owner_queries; return &fake_owner2; }
    assert(w>=1 && w<=count && w<=ARR_SZ(fake)); return &fake[w-1];
}
static BOOL EqualRect(const RECT *a, const RECT *b) { return !memcmp(a,b,sizeof *a); }
static void SetRect(RECT *r, LONG l, LONG t, LONG rr, LONG b) { *r=(RECT){l,t,rr,b}; }
static LONG MovementGeneration(void) { return generation; }
static LONG_PTR GetWindowLongPtr(HWND w, int which) {
    if (w==102 && owner_style_fail && (owner_style_fail==2 || which==GWL_EXSTYLE))
        { ++owner_queries; last_error=5; return 0; }
    return which ? win(w)->exstyle : win(w)->style;
}
static DWORD GetWindowThreadProcessId(HWND w, DWORD *pid)
{ process_window=w; *pid=win(w)->pid; return win(w)->identity_fail ? 0 :
    w==102 ? (DWORD)(10+owner_thread_changed) : (DWORD)(10+thread_changed); }
static DWORD GetCurrentProcessId(void) { return 999; }
static HWND GetDesktopWindow(void) { return fake[0].shell_kind==1 ? 1 : 100; }
static HWND GetShellWindow(void) { return fake[0].shell_kind==2 ? 1 : 101; }
static HWND GetForegroundWindow(void) { ++foreground_queries; return foreground; }
static HWND GetWindow(HWND w, int which) {
    assert(which==GW_OWNER); ++owner_link_queries;
    if (win(w)->owned==3) return w;
    return win(w)->owned==2 ? 103 : win(w)->owned ? 102 : 0;
}
static BOOL IsWindow(HWND w) {
    ++owner_queries;
    return w==102 ? owner_exists : w==103 ? 1 : owned_exists;
}
static HWND GetAncestor(HWND w, UINT which) {
    ++owner_queries; assert(which==GA_ROOT || which==GA_ROOTOWNER);
    if (win(w)->root_self) return w;
    return which==GA_ROOT ? relation_root : relation_rootowner;
}
static HWND GetLastActivePopup(HWND w) {
    ++owner_queries;
    if (w<=count && win(w)->root_self) return w;
    assert(w==relation_rootowner);
    if (owner_race==1) owner_exists=0;
    if (owner_race==2) fake[0].owned=2;
    if (owner_race==3) fake[0].owned=0;
    if (owner_race==4) owned_exists=0;
    return relation_popup;
}
static int IsWindowEnabled(HWND w) { return win(w)->enabled; }
static int IsWindowVisible(HWND w) { ++diagnostic_queries; return win(w)->visible; }
static int IsWindowCloaked(HWND w) { ++diagnostic_queries; return win(w)->cloaked; }
static int HasWindowSizeZero(HWND w) { ++diagnostic_queries; return win(w)->zero; }
static int IsIconic(HWND w) {
    ++diagnostic_queries;
    if (w==1 && ++iconic_reads==iconic_flip_at) fake[0].iconic=!fake[0].iconic;
    return win(w)->iconic;
}
static int GetBorderlessFlag(HWND w) { ++diagnostic_queries; return win(w)->borderless; }
static int IsSnapCandidate(HWND w)
{
    struct FakeWindow *f=win(w);
    return f->visible && !f->cloaked && !f->zero && !f->iconic && !f->snap_changed
        && !(f->exstyle & WS_EX_NOACTIVATE)
        && ((f->style & WS_CAPTION)==WS_CAPTION || (f->style & WS_THICKFRAME)
            || f->borderless || f->snaplisted);
}
static int IsZoomed(HWND w) { return win(w)->zoomed; }
static int IsFullscreen(HWND w) { return win(w)->fullscreen; }
static int GetProp(HWND w, int prop) { return win(w)->prop==prop; }
static HMONITOR MonitorFromWindow(HWND w, int flags) { (void)flags; return win(w)->monitor; }
static int isClassName(HWND w, const char *name)
{
    const char *names[]={"", "Shell_TrayWnd", "Shell_SecondaryTrayWnd", "Progman", "WorkerW"};
    return !strcmp(name,names[win(w)->class_kind]);
}
static int blacklisted(HWND w, const blacklist_t *list)
{
    /* General helper/cache must not be used for ATG process/window lists. */
    assert(list==&BlkLst.Snaplist); return win(w)->snaplisted;
}
static int GetWindowText(HWND w, TCHAR *buf, size_t n)
{ int k=snprintf(buf,n,"window%u",(unsigned)w); assert(k>0 && (size_t)k<n); return k; }
static int GetClassName(HWND w, TCHAR *buf, size_t n)
{
    const char *name=win(w)->class_override ? win(w)->class_override : model_owner_class && w==102 ? model_owner_class : model_class && w==1 ? model_class : win(w)->class_kind==5 ? "#32770" : "Class";
    assert(n>strlen(name));
    if (class_query_fail) return 0;
    strcpy(buf,name); return (int)strlen(name);
}
static int blacklisted_from_names(const blacklist_t *list, const TCHAR *exe,
                                 const TCHAR *title, const TCHAR *classname)
{
    unsigned w=0;
    assert(sscanf(title,"window%u",&w)==1 && !strcmp(classname,win(w)->class_override ? win(w)->class_override : model_class && w==1 ? model_class : "Class"));
    /* Every list receives the identity validated by the production lookup,
       including when the old process APIs fail or an unrelated cache is stale. */
    assert(!strcmp(exe,win(w)->path_override ? strrchr(win(w)->path_override,'\\')+1 : model_image && win(w)->pid==fake[0].pid ? model_image : win(w)->altsnap_exe ? "AltSnap.exe" : win(w)->limited_only ? "Code.exe" : "test.exe"));
    return win(w)->blacklisted==(int)list->kind;
}
static void SetLastError(DWORD error) { ++traced_process_calls; last_error=error; }
static DWORD GetLastError(void) { return last_error; }
static HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid)
{
    assert(!inherit && pid==win(process_window)->pid);
    if (win(process_window)->name_fail ||
        (win(process_window)->limited_only && access!=0x1000u)) { last_error=5; return 0; }
    assert(access==(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ) ||
           access==PROCESS_QUERY_INFORMATION || access==0x1000u);
    ++process_opens; return process_window;
}
static BOOL CloseHandle(HANDLE proc) { (void)win(proc); ++process_closes; return 1; }
static DWORD process_name(HANDLE proc, TCHAR *buf, DWORD n)
{
    const char *name=win(proc)->altsnap_exe ? "C:\\Apps\\AltSnap.exe" : win(proc)->limited_only ? "C:\\Apps\\Code.exe" : "C:\\Apps\\test.exe";
    if (win(proc)->path_override) {
        name=win(proc)->path_override;
    } else if (model_image && win(proc)->pid==fake[0].pid) {
        int length=snprintf(buf,n,"C:\\Apps\\%s",model_image);
        assert(length>0 && (DWORD)length<n); return (DWORD)length;
    }
    assert(strlen(name)<n); strcpy(buf,name); return (DWORD)strlen(name);
}
static DWORD module_name(HANDLE proc, HMODULE module, LPTSTR buf, DWORD n)
{ assert(!module); return process_name(proc,buf,n); }
static DWORD image_name(HANDLE proc, LPTSTR buf, DWORD n) { return process_name(proc,buf,n); }
static BOOL limited_name(HANDLE proc, DWORD flags, LPTSTR buf, DWORD *n)
{ assert(!flags); ++limited_queries; *n=process_name(proc,buf,*n); return 1; }
static BOOL EnumChildWindows(HWND w, BOOL (*callback)(HWND, LPARAM), LPARAM lp)
{ (void)w; (void)callback; (void)lp; assert(0); return 0; } /* No UWP fixture in this model. */
static size_t PathGetFileNameStart(const TCHAR *name)
{ const char *p=strrchr(name,'\\'); return p ? (size_t)(p-name+1) : 0; }
static void PathStripPathL(TCHAR *name)
{ size_t i=PathGetFileNameStart(name); memmove(name,name+i,strlen(name+i)+1); }
static int lstrcmpi(const TCHAR *a, const char *b) { return strcmp(a,b); }
static int lstrcmp(const TCHAR *a, const TCHAR *b) { return strcmp(a,b); }
static UINT GetDpiForWindow(HWND w) { return (UINT)win(w)->dpi; }
static HANDLE dpi_context(HWND w) { return (HANDLE)win(w)->aware; }
static int dpi_awareness(HANDLE context) { return (int)context; }
/* Generic function pointer, matching LoadDLLProc's GetProcAddress contract. */
static void (*LoadDLLProc(const char *dll, const char *name))(void)
{
    if (!strcmp(dll,"PSAPI.DLL")) {
        assert(!strcmp(name,"GetModuleFileNameExA") || !strcmp(name,"GetProcessImageFileNameA"));
        return !strcmp(name,"GetModuleFileNameExA") ? (void (*)(void))module_name : (void (*)(void))image_name;
    }
    if (!strcmp(dll,"KERNEL32.DLL")) {
        assert(!strcmp(name,"QueryFullProcessImageNameA")); return (void (*)(void))limited_name;
    }
    return !strcmp(name,"GetWindowDpiAwarenessContext")
        ? (void (*)(void))dpi_context : (void (*)(void))dpi_awareness;
}
#include "../window_process_name.h"

static int GetWindowRect(HWND w, RECT *r)
{ if (win(w)->identity_fail) return 0; *r=win(w)->native; return 1; }
static int DwmGetWindowAttributeL(HWND w, int attr, RECT *r, size_t size)
{
    (void)attr; (void)size;
    if (win(w)->dwm_fail) return -1;
    if (normalizes && !win(w)->zoomed) ++frame_reads_after_normalize;
    *r=win(w)->native; r->left+=8; r->right-=8; r->bottom-=8;
    if (win(w)->zoomed) r->top+=8; /* Maximized and normal insets differ. */
    return S_OK;
}
static int GetMinMaxInfoF(HWND w, POINT *lo, POINT *hi, int ignore)
{
    assert(!ignore); *lo=win(w)->lo; *hi=win(w)->hi;
    if (change_preflight && captures==2 && w==count) fake[0].native.left++;
    if (change_placement_preflight && captures==2 && w==count) fake[0].placement.rcNormalPosition.left++;
    if (selected_dialog_during_limits && captures==1 && w==count) fake[count-1].pid=1;
    if (ordinary_dialog_during_limits && captures==1 && w==count) {
        fake[count-1].pid=1; fake[count-1].owned=1;
        fake[count-1].exstyle=WS_EX_APPWINDOW;
        if (ordinary_dialog_during_limits==1) {
            fake[count-1].class_kind=5;
            fake[0].enabled=fake_owner.enabled=0;
        }
    }
    if (profile_limit_effect && captures==1 && w==count) {
        if (profile_limit_effect==1) fake[0].owned=2;
        if (profile_limit_effect==2) model_class="ChangedMain";
        if (profile_limit_effect==3) model_owner_class="ChangedOwner";
        if (profile_limit_effect==4) fake[0].pid++;
        if (profile_limit_effect==5) fake_owner.pid++;
        if (profile_limit_effect==6) thread_changed=1;
        if (profile_limit_effect==7) owner_thread_changed=1;
        if (profile_limit_effect==8) { fake[0].enabled=0; fake_owner.enabled=0; }
        if (profile_limit_effect==9) ++image_epoch;
        if (profile_limit_effect==10) fake[0].iconic=!fake[0].iconic;
    }
    return !win(w)->min_fail;
}
static int GetMonitorInfo(HMONITOR m, MONITORINFO *mi)
{ (void)m; mi->rcMonitor=monitor_rect; mi->rcWork=work_rect; return 1; }
static int GetDpiForMonitorL(HMONITOR m, int kind, UINT *x, UINT *y)
{ (void)m; (void)kind; *x=*y=96; return S_OK; }
static int EnumDesktopWindows(void *desktop, BOOL (*callback)(HWND,LPARAM), LPARAM lp)
{
    HWND w; unsigned repeat; (void)desktop; ++captures;
    for (w=1; w<=count; ++w) {
        HWND next=reverse_enum && captures%2==0 ? count+1-w : w;
        for (repeat=0;repeat<(w==1 ? enum_repeats : 1);++repeat)
            if (!callback(next,lp)) return 0;
    }
    if (enumerate_owner && !callback(102,lp)) return 0;
    if (enumerate_owner && fake_owner2.visible && !callback(103,lp)) return 0;
    if (captures==capture_flip_at) fake[0].iconic=!fake[0].iconic;
    return 1;
}
static BOOL EnumWindows(BOOL (*callback)(HWND,LPARAM), LPARAM lp)
{
    HWND w;
    ++selected_scans;
    if (selected_scan_fail) return 0;
    for (w=1;w<=count;++w) if (!callback(w,lp)) return 0;
    if (enumerate_owner && !callback(102,lp)) return 0;
    if (enumerate_owner && fake_owner2.visible && !callback(103,lp)) return 0;
    if ((int)selected_scans==selected_effect_at) {
        if (selected_effect==1) fake[0].owned=2;
        if (selected_effect==2) fake[0].owned=0;
        if (selected_effect==3) owner_exists=0;
        if (selected_effect==4) owned_exists=0;
        if (selected_effect==5) fake_owner.pid++;
        if (selected_effect==6) fake[0].pid++;
        if (selected_effect==7) owner_thread_changed=1;
        if (selected_effect==8) thread_changed=1;
        if (selected_effect==9) foreground=0;
        if (selected_effect==10) fake[0].enabled=0;
        if (selected_effect==11) fake_owner.enabled=0;
        if (selected_effect==12) fake[0].exstyle=0;
        if (selected_effect==13) fake_owner.exstyle=0;
        if (selected_effect==14) relation_popup=102;
        if (selected_effect==15) relation_rootowner=102;
        if (selected_effect==16) fake[0].style|=WS_POPUP;
        if (selected_effect==17) fake[0].iconic=!fake[0].iconic;
    }
    return 1;
}
static HMONITOR MonitorFromPoint(POINT p, int flags) { (void)flags; return p.x<1200 ? 1 : 2; }
static HDWP BeginDeferWindowPos(unsigned n) { assert(n>=1 && n<=4); batch_mask=0; return fail_begin ? 0 : 1; }
static HDWP DeferWindowPos(HDWP b, HWND w, void *after, LONG x, LONG y, LONG cx, LONG cy, int flags)
{
    assert(!after && flags==(SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER));
    if (fail_defer) return 0;
    targets[w-1]=(RECT){x,y,x+cx,y+cy}; batch_mask|=1u<<(w-1); return b;
}
static int EndDeferWindowPos(HDWP b)
{
    unsigned i; (void)b; ++batches;
    for (i=0;i<count;++i) if (batch_mask & (1u<<i)) {
        ++positions;
        if (fake[i].behavior!=2) fake[i].native=targets[i];
        if (fake[i].behavior==1) fake[i].native.right+=107;
        fake[i].placement.rcNormalPosition=fake[i].native;
        fake[i].placement.rcNormalPosition.left-=work_rect.left-monitor_rect.left;
        fake[i].placement.rcNormalPosition.right-=work_rect.left-monitor_rect.left;
        fake[i].placement.rcNormalPosition.top-=work_rect.top-monitor_rect.top;
        fake[i].placement.rcNormalPosition.bottom-=work_rect.top-monitor_rect.top;
    }
    if (batch_effect==1) fake[1].pid++;
    if (batch_effect==2) { fake[1].zoomed=1; fake[1].placement.showCmd=SW_SHOWMAXIMIZED; }
    if (batch_effect==3) fake[1].identity_fail=1;
    if (batch_effect==4) fake[1].dwm_fail=1;
    if (selected_batch_effect==1) fake[0].owned=2;
    if (selected_batch_effect==2) fake[0].pid++;
    if (selected_batch_effect==3) fake_owner.enabled=0;
    if (selected_batch_effect==4) foreground=0;
    if (selected_batch_effect==5) relation_popup=102;
    if (profile_batch_effect==1) fake[0].owned=2;
    if (profile_batch_effect==2) model_class="ChangedMain";
    if (profile_batch_effect==3) model_owner_class="ChangedOwner";
    if (profile_batch_effect==4) fake[0].iconic=!fake[0].iconic;
    if (cancel_batch) ++generation;
    if (start_drag) state.action.ac=1;
    return !fail_batch;
}
static int SetWindowPos(HWND w, void *after, LONG x, LONG y, LONG cx, LONG cy, int flags)
{
    assert(!after && flags==(SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER));
    ++restores; if (restore_fail) return 0; win(w)->native=(RECT){x,y,x+cx,y+cy};
    win(w)->placement.rcNormalPosition=win(w)->native;
    win(w)->placement.rcNormalPosition.left-=work_rect.left-monitor_rect.left;
    win(w)->placement.rcNormalPosition.right-=work_rect.left-monitor_rect.left;
    win(w)->placement.rcNormalPosition.top-=work_rect.top-monitor_rect.top;
    win(w)->placement.rcNormalPosition.bottom-=work_rect.top-monitor_rect.top;
    /* An earlier restore's callback changes another participant. */
    if (external_restore) { fake[1].native.left+=31; fake[1].native.right+=31; external_restore=0; }
    if (replace_restore) { fake[1].pid++; replace_restore=0; }
    return 1;
}
static int GetWindowPlacement(HWND w, WINDOWPLACEMENT *p)
{
    assert(p->length==sizeof *p);
    if (placement_fail) return 0;
    *p=win(w)->placement; return 1;
}
static int SetWindowPlacement(HWND w, const WINDOWPLACEMENT *p)
{
    struct FakeWindow *f=win(w);
    assert(p->length==sizeof *p);
    if (p->showCmd==SW_SHOWNOACTIVATE) {
        ++normalizes;
        if (fail_normalize==(int)w) return 0;
        assert(!p->flags);
        f->placement=*p; f->placement.showCmd=SW_SHOWNORMAL; f->zoomed=0;
        f->native=p->rcNormalPosition;
        f->native.left+=work_rect.left-monitor_rect.left;
        f->native.right+=work_rect.left-monitor_rect.left;
        f->native.top+=work_rect.top-monitor_rect.top;
        f->native.bottom+=work_rect.top-monitor_rect.top;
        assert(EqualRect(&f->native,&work_rect));
        if (normalize_effect_at==(int)w) {
            if (normalize_effect==1) fake[0].native.left+=31;
            if (normalize_effect==2) fake[0].pid++;
            if (normalize_effect==3) ++generation;
            if (normalize_effect==4) state.action.ac=1;
            if (normalize_effect==5) f->lo.x=1800;
            if (normalize_effect==6) f->dpi=120;
            if (normalize_effect==7) { f->native.right+=107; return 0; }
            if (normalize_effect==8) f->monitor=2;
            if (normalize_effect==9) fake[0].placement.rcNormalPosition.left++;
            if (normalize_effect==10) f->min_fail=1;
        }
        return 1;
    }
    assert(p->showCmd==SW_SHOWMAXIMIZED); ++max_restores;
    if (fail_max_restore) return 0;
    assert(!memcmp(p,&f->original_placement,sizeof *p));
    f->placement=*p; f->zoomed=1; f->native=f->original;
    if (external_restore) { fake[1].native.left+=31; external_restore=0; }
    if (replace_restore) { fake[1].pid++; replace_restore=0; }
    return 1;
}
static void ClearRestoreFlagOnResizeIfNeeded(HWND w) { (void)w; ++clears; }
#define wsprintf wsprintfA
static const char *const ini_keys[]={"Enabled","Schema","Topology","Path","SHA256","Class","OwnerClass","Style","ExStyle","OwnerStyle","OwnerExStyle"};
static char ini_values[8][11][300];
static unsigned ini_reads, ini_writes;
static int ini_fail_write;
static unsigned ini_slot(const char *section)
{
    unsigned n=0; assert(sscanf(section,"AutoTileOwned%u",&n)==1 && n>=1 && n<=8); return n-1;
}
static unsigned ini_key(const char *key)
{
    unsigned i; for(i=0;i<ARR_SZ(ini_keys) && strcmp(ini_keys[i],key);++i) { }
    assert(i<ARR_SZ(ini_keys)); return i;
}
static DWORD GetPrivateProfileString(const char *section, const char *key, const char *def,
                                     char *out, unsigned n, const char *ini)
{
    const char *value=ini_values[ini_slot(section)][ini_key(key)]; size_t k;
    (void)ini; ++ini_reads; if (!value[0]) value=def;
    k=strlen(value); if(k>=n) k=n-1; memcpy(out,value,k); out[k]=0; return (DWORD)k;
}
static int WritePrivateProfileString(const char *section, const char *key, const char *value, const char *ini)
{
    unsigned slot=ini_slot(section); (void)ini;
    ++ini_writes; if ((int)ini_writes==ini_fail_write) return 0;
    if (!key) { mem00(ini_values[slot],sizeof ini_values[slot]); return 1; }
    assert(!value || strlen(value)<300);
    strcpy(ini_values[slot][ini_key(key)],value ? value : ""); return 1;
}
#include "../auto_tile_owned_store.h"
typedef struct { HANDLE file; unsigned epoch; } ATGOwnedImage;
static TCHAR image_digest[65];
static unsigned image_opens, image_closes, image_live;
static int image_open_fail;
static int ATGOwnedImageOpen(const TCHAR *path, TCHAR digest[65], ATGOwnedImage *image)
{
    image->file=0;
    if (image_open_fail || !ATGOwnedLocalPath(path)) return 0;
    ++image_opens; ++image_live; image->file=1; image->epoch=image_epoch;
    strcpy(digest,image_digest); return 1;
}
static int ATGOwnedImageStable(const ATGOwnedImage *image)
{ return image->file && image->epoch==image_epoch; }
static void ATGOwnedImageClose(ATGOwnedImage *image)
{
    if(image->file) { assert(image_live); --image_live; ++image_closes; image->file=0; }
}
#include "../auto_tile_grid_win.h"

static void reset(unsigned n)
{
    unsigned i;
    count=n; captures=positions=restores=batches=clears=0;
    generation=0; fail_batch=fail_defer=cancel_batch=start_drag=change_preflight=0;
    external_restore=replace_restore=reverse_enum=batch_effect=restore_fail=0;
    normalizes=max_restores=trace_lines=frame_reads_after_normalize=0;
    trace_used=0; trace_log[0]=0; auto_tile_trace=0;
    diagnostic_queries=traced_process_calls=0; enum_repeats=1;
    owner_queries=owner_link_queries=0;
    owner_exists=owned_exists=1; owner_race=owner_style_fail=0;
    relation_root=relation_popup=1; relation_rootowner=102;
    mem00(&fake_owner,sizeof fake_owner); fake_owner.pid=1;
    mem00(&fake_owner2,sizeof fake_owner2);
    foreground=0; selected_scans=0;
    selected_scan_fail=selected_effect=selected_effect_at=0;
    class_query_fail=thread_changed=owner_thread_changed=0;
    selected_batch_effect=0;
    selected_dialog_during_limits=0;
    ordinary_dialog_during_limits=0;
    profile_limit_effect=profile_batch_effect=0;
    iconic_reads=iconic_flip_at=capture_flip_at=0;
    model_class=model_image=model_owner_class=NULL; foreground_queries=0; enumerate_owner=0;
    mem00(auto_tile_owned_profiles,sizeof auto_tile_owned_profiles); auto_tile_owned_enabled=0;
    mem00(ini_values,sizeof ini_values); ini_reads=ini_writes=0; ini_fail_write=0;
    assert(!image_live && image_opens==image_closes);
    image_epoch=image_opens=image_closes=0; image_open_fail=0;
    memset(image_digest,'a',64); image_digest[64]=0;
    fail_normalize=fail_max_restore=normalize_effect=normalize_effect_at=0;
    placement_fail=change_placement_preflight=fail_begin=0;
    monitor_rect=(RECT){0,0,1200,1000}; work_rect=(RECT){0,0,1200,960};
    mem00(&state,sizeof state); mem00(&LastWin,sizeof LastWin);
    mem00(&linked_queue,sizeof linked_queue); mem00(&linked_move,sizeof linked_move);
    g_InMouseMove=g_InFinishMovement=0;
    BlkLst.Processes=(blacklist_t){1,&BlkLst,1};
    BlkLst.Windows=(blacklist_t){1,&BlkLst,2};
    BlkLst.Bottommost=(blacklist_t){1,&BlkLst,3};
    BlkLst.Snaplist=(blacklist_t){1,&BlkLst,4};
    assert(process_opens==process_closes);
    process_opens=process_closes=limited_queries=0;
    mem00(fake,sizeof fake);
    for (i=0;i<n;++i) {
        fake[i].native=fake[i].original=(RECT){20+(LONG)i*30,20,400+(LONG)i*30,320};
        fake[i].placement=(WINDOWPLACEMENT){sizeof(WINDOWPLACEMENT),0,SW_SHOWNORMAL,{-1,-1},{-1,-1},fake[i].native};
        fake[i].original_placement=fake[i].placement;
        fake[i].style=WS_THICKFRAME|WS_CAPTION; fake[i].monitor=1; fake[i].pid=1;
        fake[i].aware=2; fake[i].dpi=96; fake[i].enabled=fake[i].visible=1;
        fake[i].lo=(POINT){32,32}; fake[i].hi=(POINT){3000,3000};
    }
}
static void invoke(void) { AutoTileGrid((POINT){100,100}); }
static void unchanged(void)
{
    unsigned i;
    for (i=0;i<count;++i) assert(EqualRect(&fake[i].native,&fake[i].original));
}
static void layouts_and_refusals(void)
{
    unsigned n,i;
    for (n=0;n<=5;++n) {
        reset(n); invoke();
        if (!n || n>4) { assert(!positions); unchanged(); continue; }
        assert(positions==n && clears==n && !restores);
        for (i=0;i<n;++i) {
            RECT r; assert(!DwmGetWindowAttributeL(i+1,9,&r,sizeof r));
            assert(r.left>=0 && r.right<=1200 && r.top>=0 && r.bottom<=960);
        }
        invoke(); assert(positions==n && batches==1); /* Fixed point. */
    }
    reset(4); fake[2].lo.x=800; invoke(); assert(!positions); unchanged();
    reset(4); fake[2].hi.y=400; invoke(); assert(!positions); unchanged();
    reset(4); fake[2].min_fail=1; invoke(); assert(!positions); unchanged();
    reset(4); fake[2].aware=1; invoke(); assert(!positions); unchanged();
    reset(4); fake[2].dpi=120; invoke(); assert(!positions); unchanged();
    reset(4); change_preflight=1; invoke(); assert(!positions);
    reset(1); fake[0].native=(RECT){INT32_MIN,20,INT32_MAX,320};
    invoke(); assert(!positions && !normalizes);
}
static void exclusions(void)
{
    unsigned kind;
    for (kind=0;kind<16;++kind) {
        RECT saved;
        reset(2);
        switch(kind) {
        case 0: fake[1].monitor=2; break;
        case 1: fake[1].visible=0; break; /* Includes minimized/cloaked helper refusals. */
        case 2: fake[1].style=0; break;
        case 3: fake[1].style|=WS_CHILD; break;
        case 4: fake[1].exstyle=WS_EX_TOOLWINDOW; break;
        case 5: fake[1].exstyle=WS_EX_NOACTIVATE; break;
        case 6: fake[1].owned=1; break;
        case 7: fake[1].enabled=0; break;
        case 8: fake[1].zoomed=1; fake[1].style=WS_THICKFRAME; break; /* Nonordinary maximized. */
        case 9: fake[1].fullscreen=1; break;
        case 10: fake[1].blacklisted=1; break;
        case 11: fake[1].prop=APP_MOVEONOFF; break;
        case 12: fake[1].prop=APP_ROLLED; break;
        case 13: fake[1].dwm_fail=1; break;
        case 14: fake[1].identity_fail=1; break;
        case 15: fake[1].style=WS_THICKFRAME; fake[1].native.left=-100; break;
        }
        saved=fake[1].native; invoke();
        assert(positions==1 && EqualRect(&fake[1].native,&saved));
        assert(fake[0].native.left==-8 && fake[0].native.right==1208);
    }
    reset(2); fake[1].monitor=2; AutoTileGrid((POINT){1300,100});
    assert(positions==1 && EqualRect(&fake[0].native,&fake[0].original));
}
static void recovery_and_drag(void)
{
    unsigned kind;
    for (kind=0;kind<4;++kind) {
        reset(4);
        if (kind==0) fake[2].behavior=1; /* MobaXterm-like +107 px. */
        if (kind==1) fake[2].behavior=2; /* Refuse one destination. */
        if (kind==2) fail_batch=1;
        if (kind==3) fail_defer=1;
        invoke(); unchanged(); assert(!clears);
        if (kind!=3) assert(restores>=3);
    }
    for (kind=0;kind<7;++kind) {
        reset(4);
        if (kind==0) state.action.ac=1;
        if (kind==1) g_InMouseMove=1;
        if (kind==2) g_InFinishMovement=1;
        if (kind==3) LastWin.hwnd=1;
        if (kind==4) linked_queue.busy=1;
        if (kind==5) linked_move.pending=1;
        if (kind==6) { state.action.ac=1; linked_queue.busy=1; }
        invoke(); assert(!captures && !positions); unchanged();
    }
    reset(4); fake[2].behavior=1; cancel_batch=1; invoke(); assert(!restores && !clears);
    reset(4); fake[2].behavior=1; start_drag=1; invoke(); assert(!restores && !clears);
    reset(4); fake[2].behavior=1; external_restore=1; invoke();
    assert(restores==3 && fake[1].native.left==targets[1].left+31);
    reset(4); fake[2].behavior=1; replace_restore=1; invoke();
    assert(restores==3 && EqualRect(&fake[1].native,&targets[1]));
}
static void maximize(unsigned i)
{
    fake[i].zoomed=1;
    fake[i].native=fake[i].original=(RECT){monitor_rect.left-8,monitor_rect.top-8,monitor_rect.right+8,work_rect.bottom+8};
    fake[i].placement.showCmd=SW_SHOWMAXIMIZED;
    fake[i].original_placement=fake[i].placement;
}
static void maximized_and_overhang(void)
{
    unsigned n,i,kind;
    /* Every combination of normal/maximized windows, not just all-maximized. */
    for (n=1;n<=4;++n) for (kind=0;kind<(1u<<n);++kind) {
        reset(n);
        for (i=0;i<n;++i) if (kind&(1u<<i)) maximize(i);
        invoke(); assert(clears==n && !restores && !max_restores);
        assert(normalizes<=n);
        for (i=0;i<n;++i) {
            RECT r; assert(!fake[i].zoomed && fake[i].placement.showCmd==SW_SHOWNORMAL);
            assert(!DwmGetWindowAttributeL(i+1,9,&r,sizeof r));
            assert(r.left>=0 && r.right<=1200 && r.top>=0 && r.bottom<=960);
        }
        if (kind) assert(frame_reads_after_normalize);
        i=positions; invoke(); assert(positions==i); /* Normalized fixed point. */
    }
    reset(4); maximize(0); maximize(2); reverse_enum=1; invoke();
    assert(clears==4 && !max_restores && normalizes==2);
    reset(5); for(i=0;i<5;++i) maximize(i);
    invoke(); unchanged(); assert(!normalizes && !positions);
    for (kind=0;kind<4;++kind) {
        reset(4);
        for (i=0;i<4;++i) {
            if (kind==0) fake[i].native.left=-200;
            if (kind==1) fake[i].native.top=-150;
            if (kind==2) fake[i].native.right=1300;
            if (kind==3) fake[i].native.bottom=1100;
        }
        invoke(); assert(clears==4 && positions==4);
    }
    reset(3); fake[1].native.left=-200; fake[1].native.right=50;
    fake[1].monitor=2; maximize(2); invoke();
    assert(clears==2 && fake[1].native.left==-200 && fake[1].native.right==50);
    /* Negative monitor origin and taskbars at top/left exercise workspace conversion. */
    reset(4); monitor_rect=(RECT){-1600,-1000,-400,0}; work_rect=(RECT){-1560,-970,-400,0};
    for(i=0;i<4;++i) maximize(i);
    invoke(); assert(clears==4 && normalizes==4);
    for(i=0;i<4;++i) { RECT r; DwmGetWindowAttributeL(i+1,9,&r,sizeof r);
        assert(r.left>=-1560 && r.right<=-400 && r.top>=-970 && r.bottom<=0); }
    /* Original placement is retained byte-for-byte, including off-monitor restore rect. */
    for(kind=0;kind<7;++kind) {
        reset(4); maximize(0); maximize(2);
        fake[0].placement.rcNormalPosition=(RECT){1300,30,1700,400};
        fake[0].original_placement=fake[0].placement;
        if(kind==0) fake[1].behavior=1;
        if(kind==1) fake[1].behavior=2;
        if(kind==2) fail_batch=1;
        if(kind==3) fail_defer=1;
        if(kind==4) fail_begin=1;
        if(kind==5) { normalize_effect_at=3; normalize_effect=5; }
        if(kind==6) { normalize_effect_at=3; normalize_effect=10; }
        invoke(); unchanged(); assert(!clears && max_restores==2);
        assert(fake[0].zoomed && fake[2].zoomed);
        assert(AutoTileSamePlacement(&fake[0].placement,&fake[0].original_placement));
    }
    reset(4); maximize(0); maximize(2); fail_normalize=3;
    invoke(); unchanged(); assert(normalizes==2 && max_restores==1 && !positions);
    reset(4); maximize(0); maximize(2); normalize_effect_at=3; normalize_effect=7;
    invoke(); unchanged(); assert(max_restores==2 && !positions);
    reset(4); maximize(0); placement_fail=1; invoke(); assert(!normalizes); unchanged();
    reset(4); maximize(0); change_placement_preflight=1; invoke(); assert(!normalizes && !positions);
    /* Cancellation, identity, geometry, DPI and monitor changes never get forced back. */
    for(kind=1;kind<=9;++kind) {
        if(kind==5 || kind==7) continue;
        reset(4); maximize(0); maximize(2); normalize_effect_at=3; normalize_effect=(int)kind;
        invoke(); assert(!positions && !clears);
        if(kind==3 || kind==4) assert(!max_restores);
        if(kind==1 || kind==2 || kind==9) assert(!fake[0].zoomed && fake[2].zoomed);
        if(kind==6 || kind==8) assert(fake[0].zoomed && !fake[2].zoomed);
    }
    reset(4); maximize(0); maximize(1); fake[2].behavior=1; external_restore=1;
    invoke(); assert(max_restores==1 && fake[0].zoomed && !fake[1].zoomed);
    reset(4); maximize(0); maximize(1); fake[2].behavior=1; replace_restore=1;
    invoke(); assert(max_restores==1 && fake[0].zoomed && !fake[1].zoomed);
    for(kind=1;kind<=4;++kind) {
        reset(4); maximize(0); maximize(1); batch_effect=(int)kind; invoke();
        assert(!clears && max_restores==1 && fake[0].zoomed && !fake[2].zoomed);
        if (kind==2) assert(fake[1].zoomed); /* External maximization survives. */
    }
    reset(4); fake[2].behavior=1; restore_fail=1; invoke();
    assert(restores==4 && !clears && !EqualRect(&fake[0].native,&fake[0].original));
    reset(1); maximize(0); normalize_effect_at=1; normalize_effect=5; fail_max_restore=1;
    invoke(); assert(max_restores==1 && !fake[0].zoomed && !clears); /* Best effort only. */
}
static void diagnostic_trace(void)
{
    unsigned kind, enabled;
    const char *reasons[]={
        "skip-monitor", "skip-invisible", "skip-cloaked", "skip-zero-size", "skip-minimized",
        "skip-noactivate", "skip-snap-shape", "skip-snap-state-changed", "skip-fullscreen",
        "skip-thread-query", "skip-own-process", "skip-desktop", "skip-shell", "skip-child",
        "skip-no-thickframe", "skip-maximized-no-caption", "skip-toolwindow", "skip-owner",
        "skip-not-enabled", "skip-taskbar", "skip-secondary-taskbar", "skip-progman", "skip-worker",
        "skip-move-disabled", "skip-rolled", "skip-blacklist-process", "skip-blacklist-window",
        "skip-blacklist-bottommost", "skip-process-unreadable", "skip-altsnap-exe"
    };
    reset(4); invoke(); assert(!trace_lines && !diagnostic_queries && !traced_process_calls);
    /* Same production eligibility decisions with tracing on and off, every
       constituent refusal. The model keeps diagnostic-only reads countable. */
    for (kind=0;kind<ARR_SZ(reasons);++kind) for (enabled=0;enabled<=1;++enabled) {
        reset(1); auto_tile_trace=(int)enabled;
        switch(kind) {
        case 0: fake[0].monitor=2; break;
        case 1: fake[0].visible=0; break;
        case 2: fake[0].cloaked=1; break;
        case 3: fake[0].zero=1; break;
        case 4: fake[0].iconic=1; break;
        case 5: fake[0].exstyle=WS_EX_NOACTIVATE; break;
        case 6: fake[0].style=0; break;
        case 7: fake[0].snap_changed=1; break;
        case 8: fake[0].fullscreen=1; break;
        case 9: fake[0].identity_fail=1; break;
        case 10: fake[0].pid=999; break;
        case 11: fake[0].shell_kind=1; break;
        case 12: fake[0].shell_kind=2; break;
        case 13: fake[0].style|=WS_CHILD; break;
        case 14: fake[0].style=WS_CAPTION; break;
        case 15: fake[0].style=WS_THICKFRAME; fake[0].zoomed=1; break;
        case 16: fake[0].exstyle=WS_EX_TOOLWINDOW; break;
        case 17: fake[0].owned=1; break;
        case 18: fake[0].enabled=0; break;
        case 19: case 20: case 21: case 22: fake[0].class_kind=(int)kind-18; break;
        case 23: fake[0].prop=APP_MOVEONOFF; break;
        case 24: fake[0].prop=APP_ROLLED; break;
        case 25: case 26: case 27: fake[0].blacklisted=(int)kind-24; break;
        case 28: fake[0].name_fail=1; break;
        case 29: fake[0].altsnap_exe=1; break;
        }
        invoke(); unchanged(); assert(!positions && !normalizes);
        if (enabled) {
            assert(strstr(trace_log,reasons[kind]));
            assert(strstr(trace_log,"reason=scan-candidates count=0"));
            if (kind==28) assert(strstr(trace_log,"reason=process-open-limited-failed error=5"));
        } else assert(!trace_lines && !diagnostic_queries && !traced_process_calls);
        assert(!strstr(trace_log,"test.exe") && !strstr(trace_log,"AltSnap.exe"));
    }
    /* Hundreds of routine exclusions produce one count, not hundreds of lines. */
    for (kind=0;kind<5;++kind) {
        reset(1); auto_tile_trace=1; enum_repeats=1000;
        if (kind==0) fake[0].monitor=2;
        if (kind==1) fake[0].visible=0;
        if (kind==2) fake[0].cloaked=1;
        if (kind==3) fake[0].zero=1;
        if (kind==4) fake[0].iconic=1;
        invoke(); assert(trace_lines==4 && strstr(trace_log,"count=1000\n"));
        assert(!positions && !strstr(trace_log,"skip-hidden-or-fullscreen"));
    }
    /* A process blacklist cannot be bypassed by a failed name lookup. */
    reset(1); fake[0].blacklisted=fake[0].name_fail=1; auto_tile_trace=1; invoke();
    assert(strstr(trace_log,"skip-process-unreadable") && traced_process_calls && !positions);
    reset(5); auto_tile_trace=1; invoke(); assert(strstr(trace_log,"reason=too-many"));
    reset(4); auto_tile_trace=1; fake[1].dpi=120; invoke();
    assert(strstr(trace_log,"reason=dpi-rect-limits"));
    reset(2); auto_tile_trace=1; maximize(0); fake[1].behavior=1; invoke();
    assert(strstr(trace_log,"reason=normalized") && strstr(trace_log,"reason=restore-max"));
    assert(!strstr(trace_log,"test.exe"));
}
static void limited_fallback_and_owner_safety(void)
{
    unsigned kind, enabled;
    for (enabled=0;enabled<=1;++enabled) for (kind=0;kind<7;++kind) {
        reset(1); auto_tile_trace=(int)enabled; fake[0].limited_only=1;
        if (kind>=1 && kind<=3) fake[0].blacklisted=(int)kind;
        if (kind==4) fake[0].altsnap_exe=1;
        if (kind==5) fake[0].name_fail=1;
        /* Empty lists preserve their existing no-match behavior. */
        if (kind==6) BlkLst.Processes.length=BlkLst.Windows.length=BlkLst.Bottommost.length=0;
        invoke();
        assert(process_opens==process_closes);
        if (kind==0 || kind==6) assert(positions==1 && limited_queries);
        else { assert(!positions && !normalizes); unchanged(); }
        if (kind==5) assert(!limited_queries);
        if (enabled && kind==0) assert(strstr(trace_log,"process-limited-used"));
        if (!enabled) assert(!trace_lines && !traced_process_calls && !diagnostic_queries);
        assert(!strstr(trace_log,"Code.exe") && !strstr(trace_log,"C:\\"));
    }
    /* Styles/taskbar hints and maximization cannot turn an owned dialog into
       a proven main window. All combinations retain the unconditional gate. */
    for (kind=0;kind<16;++kind) {
        reset(1); fake[0].owned=1;
        if (kind&1) fake[0].style|=WS_POPUP;
        if (kind&2) fake[0].exstyle|=WS_EX_APPWINDOW;
        if (kind&4) fake[0].exstyle|=WS_EX_TOOLWINDOW;
        if (kind&8) maximize(0);
        invoke(); unchanged(); assert(!positions && !normalizes && !process_opens);
    }
    /* Untraced public wrappers used by existing blacklist callers also fall back. */
    {
        TCHAR name[MAX_PATH];
        reset(1); fake[0].limited_only=1;
        assert(GetWindowProgName(1,name,ARR_SZ(name)) && !strcmp(name,"Code.exe"));
        assert(GetWindowProgNameFullPath(1,name,ARR_SZ(name)) && !strcmp(name,"C:\\Apps\\Code.exe"));
        assert(limited_queries==2 && process_opens==process_closes && !traced_process_calls);
    }
}

/* Exact token vocabulary, rather than only checking for a few known secrets.
   Any new owner diagnostic field/value must be explicitly reviewed here. */
static void owner_log_safe(void)
{
    const char *p=trace_log;
    const char *const allowed[]={
        "AltSnap.ATG", "reason=owner-state", "reason=owner-flags", "reason=owned-flags", "reason=owner-relations",
        "link=stable", "link=owner-gone", "link=window-gone", "link=changed", "link=cleared",
        "process=same", "process=different", "process=unknown",
        "visible=yes", "visible=no", "enabled=yes", "enabled=no",
        "tool=yes", "tool=no", "tool=unknown", "app=yes", "app=no", "app=unknown",
        "noactivate=yes", "noactivate=no", "noactivate=unknown", "popup=yes", "popup=no",
        "root=self", "root=owner", "root=other", "root=rootowner", "root=unknown",
        "rootowner=self", "rootowner=owner", "rootowner=rootowner", "rootowner=unknown",
        "lastpopup=self", "lastpopup=owner", "lastpopup=rootowner", "lastpopup=other", "lastpopup=unknown"
    };
    while (*p) {
        const char *end=strchr(p,'\n');
        char line[256], *token;
        assert(end);
        if (!strncmp(p,"AltSnap.ATG reason=owner",23)) {
            size_t n=(size_t)(end-p); assert(n<sizeof line);
            memcpy(line,p,n); line[n]=0;
            for (token=strtok(line," ");token;token=strtok(NULL," ")) {
                unsigned i;
                for (i=0;i<ARR_SZ(allowed) && strcmp(token,allowed[i]);++i) { }
                assert(i<ARR_SZ(allowed));
            }
        }
        p=end+1;
    }
}

static void owner_topology_trace(void)
{
    unsigned enabled, flags, owner_flags, process, state_bits, maximized, kind;
    ATGSnapshot s;
    mem00(&s,sizeof s); s.monitor=1;
    for (enabled=0;enabled<=1;++enabled)
    for (flags=0;flags<8;++flags) /* Candidate popup/app/tool, including earlier refusal. */
    for (owner_flags=0;owner_flags<8;++owner_flags)
    for (process=0;process<4;++process)
    for (state_bits=0;state_bits<4;++state_bits)
    for (maximized=0;maximized<=1;++maximized) {
        reset(1); auto_tile_trace=(int)enabled; fake[0].owned=1;
        if (flags&1) fake[0].style|=WS_POPUP;
        if (flags&2) fake[0].exstyle|=WS_EX_APPWINDOW;
        if (flags&4) fake[0].exstyle|=WS_EX_TOOLWINDOW;
        if (maximized) maximize(0);
        fake_owner.exstyle=(owner_flags&1 ? WS_EX_TOOLWINDOW : 0)
            | (owner_flags&2 ? WS_EX_APPWINDOW : 0) | (owner_flags&4 ? WS_EX_NOACTIVATE : 0);
        fake_owner.visible=!!(state_bits&1); fake_owner.enabled=!!(state_bits&2);
        if (process==1) fake_owner.pid=7654321;
        if (process==2) fake_owner.identity_fail=1;
        if (process==3) fake_owner.pid=0;
        last_error=1234567;
        assert(!AutoTileEligible(1,&s,NULL)); unchanged();
        assert(!positions && !normalizes && !process_opens && !limited_queries);
        assert(last_error==1234567);
        if (!enabled) {
            assert(!trace_lines && !owner_queries && !diagnostic_queries && !traced_process_calls);
            assert(owner_link_queries==((flags&4) ? 0u : 1u));
        } else if (flags&4) {
            assert(!owner_queries && !owner_link_queries && !strstr(trace_log,"reason=owner-"));
        } else {
            char expected[128];
            assert(trace_lines==5 && owner_queries<=16 && owner_link_queries==2);
            snprintf(expected,sizeof expected,"process=%s visible=%s enabled=%s tool=%s app=%s noactivate=%s",
                process>=2 ? "unknown" : process==1 ? "different" : "same",
                state_bits&1 ? "yes" : "no", state_bits&2 ? "yes" : "no",
                owner_flags&1 ? "yes" : "no", owner_flags&2 ? "yes" : "no", owner_flags&4 ? "yes" : "no");
            assert(strstr(trace_log,expected));
            snprintf(expected,sizeof expected,"reason=owned-flags popup=%s app=%s enabled=yes",
                flags&1 ? "yes" : "no", flags&2 ? "yes" : "no");
            assert(strstr(trace_log,expected)); owner_log_safe();
        }
    }
    /* Races before and during observation cannot undo the initial refusal. */
    for (enabled=0;enabled<=1;++enabled) for (kind=0;kind<6;++kind) {
        const char *links[]={"owner-gone","window-gone","owner-gone","changed","cleared","window-gone"};
        reset(1); auto_tile_trace=(int)enabled; fake[0].owned=1;
        if (kind==0) owner_exists=0;
        else if (kind==1) owned_exists=0;
        else owner_race=(int)kind-1;
        assert(!AutoTileEligible(1,&s,NULL));
        if (enabled) {
            assert(strstr(trace_log,links[kind]) && strstr(trace_log,"reason=skip-owner"));
            assert(trace_lines==2 && !strstr(trace_log,"reason=owner-flags"));
        } else assert(!trace_lines && !owner_queries && !diagnostic_queries && !traced_process_calls
                      && owner_link_queries==1);
        assert(!process_opens && !positions && !normalizes); owner_log_safe();
    }
    for (kind=0;kind<7;++kind) {
        const char *relations[]={
            "root=self rootowner=owner lastpopup=self",
            "root=other rootowner=rootowner lastpopup=rootowner",
            "root=owner rootowner=owner lastpopup=owner",
            "root=self rootowner=self lastpopup=other",
            "root=unknown rootowner=unknown lastpopup=unknown",
            "root=self rootowner=owner lastpopup=unknown",
            "root=self rootowner=owner lastpopup=self"
        };
        reset(1); auto_tile_trace=1; fake[0].owned=1;
        if (kind==1) { relation_root=104; relation_rootowner=105; relation_popup=105; }
        if (kind==2) relation_root=relation_popup=102;
        if (kind==3) { relation_rootowner=1; relation_popup=104; }
        if (kind==4) relation_root=relation_rootowner=relation_popup=0;
        if (kind==5) relation_popup=0;
        if (kind==6) { owner_style_fail=1; fake[0].enabled=0; }
        assert(!AutoTileEligible(1,&s,NULL));
        assert(strstr(trace_log,relations[kind]));
        if (kind==6) assert(strstr(trace_log,"tool=unknown app=unknown noactivate=unknown")
                            && strstr(trace_log,"reason=owned-flags popup=no app=no enabled=no"));
        owner_log_safe();
    }
    /* MobaXterm-like owned surface + three ordinary applications: same layout,
       blacklists, infeasible plans and rollback with diagnostics on and off. */
    for (enabled=0;enabled<=1;++enabled) for (maximized=0;maximized<=1;++maximized)
    for (kind=0;kind<5;++kind) {
        unsigned i, full_height=0, half_height=0;
        reset(kind==4 ? 6 : 4); auto_tile_trace=(int)enabled; fake[count-1].owned=1;
        if (maximized) maximize(count-1);
        if (kind==1) fake[1].behavior=1;
        if (kind==2) fake[1].blacklisted=1;
        if (kind==3) fake[1].lo.x=1800;
        invoke(); assert(EqualRect(&fake[count-1].native,&fake[count-1].original));
        assert(AutoTileSamePlacement(&fake[count-1].placement,&fake[count-1].original_placement));
        assert(!normalizes && !max_restores);
        if (kind==0) {
            assert(positions==3 && clears==3 && batch_mask==7);
            for (i=1;i<=3;++i) {
                RECT r; DwmGetWindowAttributeL(i,9,&r,sizeof r);
                assert(r.right-r.left==600);
                full_height+=r.bottom-r.top==960; half_height+=r.bottom-r.top==480;
            }
            assert(full_height==1 && half_height==2);
        } else if (kind==1) { unchanged(); assert(restores==3 && !clears); }
        else if (kind==2) { assert(positions==2 && clears==2); assert(EqualRect(&fake[1].native,&fake[1].original)); }
        else { unchanged(); assert(!positions && !clears); }
        if (!enabled) assert(!trace_lines && !owner_queries && !diagnostic_queries && !traced_process_calls);
        else owner_log_safe();
    }
}
static void selected_setup(unsigned n, unsigned trace)
{
    unsigned i;
    reset(n); auto_tile_trace=(int)trace;
    foreground=1; fake[0].owned=1; fake[0].exstyle=WS_EX_APPWINDOW;
    fake_owner.style=WS_POPUP; fake_owner.exstyle=WS_EX_TOOLWINDOW;
    fake_owner.visible=fake_owner.enabled=1;
    relation_root=relation_rootowner=relation_popup=1;
    for (i=1;i<n;++i) fake[i].pid=i+20;
}
static void invoke_selected(void) { AutoTileGridSelected((POINT){100,100}); }

static void explicitly_selected_owned(void)
{
    unsigned trace, n, mask, i, kind, at;
    for (trace=0;trace<=1;++trace) {
        for (n=1;n<=4;++n) for (mask=0;mask<(1u<<n);++mask) {
            selected_setup(n,trace);
            for (i=0;i<n;++i) if (mask&(1u<<i)) maximize(i);
            invoke_selected();
            assert(positions==n && clears==n && !restores && !max_restores);
            i=positions; invoke_selected(); assert(positions==i); /* Fixed point. */
            assert(selected_scans);
            if (trace) assert(strstr(trace_log,"reason=selected-owner-once count=1"));
            else assert(!trace_lines);
        }
        /* Ordinary shortcut remains unchanged, with no sticky authorization. */
        selected_setup(4,trace); invoke();
        assert(positions==3 && !selected_scans && EqualRect(&fake[0].native,&fake[0].original));
        selected_setup(4,trace); invoke_selected();
        {
            RECT selected_rect=fake[0].native;
            unsigned scans=selected_scans;
            invoke(); assert(EqualRect(&fake[0].native,&selected_rect) && selected_scans==scans);
        }
        /* Another owned window in another process never inherits permission. */
        selected_setup(5,trace); fake[4].owned=1; fake[4].exstyle=WS_EX_APPWINDOW;
        invoke_selected(); assert(positions==4 && clears==4 && EqualRect(&fake[4].native,&fake[4].original));
        /* No fallback to tiling only the other windows when selection is unsafe. */
        for (kind=0;kind<43;++kind) {
            selected_setup(4,trace);
            switch (kind) {
            case 0: foreground=0; break;
            case 1: fake[0].owned=0; break;
            case 2: owner_exists=0; break;
            case 3: owned_exists=0; break;
            case 4: fake[0].enabled=0; break;
            case 5: fake_owner.enabled=0; break;
            case 6: fake_owner.visible=0; break;
            case 7: fake[0].style|=WS_CHILD; break;
            case 8: fake[0].style|=WS_POPUP; break;
            case 9: fake[0].exstyle=0; break;
            case 10: fake[0].exstyle|=WS_EX_TOOLWINDOW; break;
            case 11: fake[0].exstyle|=WS_EX_NOACTIVATE; break;
            case 12: fake[0].exstyle|=WS_EX_DLGMODALFRAME; break;
            case 13: fake_owner.exstyle=0; break;
            case 14: fake_owner.exstyle|=WS_EX_APPWINDOW; break;
            case 15: fake_owner.exstyle|=WS_EX_NOACTIVATE; break;
            case 16: fake_owner.style|=WS_CHILD; break;
            case 17: fake_owner.pid=2; break;
            case 18: fake_owner.identity_fail=1; break;
            case 19: fake[0].identity_fail=1; break;
            case 20: owner_style_fail=1; break;
            case 21: relation_root=102; break;
            case 22: relation_rootowner=102; break;
            case 23: relation_popup=102; break;
            case 24: fake[0].class_kind=5; break; /* Standard dialog, even APPWINDOW. */
            case 25: class_query_fail=1; break;
            case 26: selected_scan_fail=1; break;
            case 27: fake[0].name_fail=1; break; /* No privilege/process bypass. */
            case 28: fake[0].dpi=120; break;
            case 29: fake[0].min_fail=1; break;
            case 30: fake[0].lo.x=1800; break;
            case 31: fake[0].monitor=2; break;
            case 32: fake[0].visible=0; break;
            case 33: fake[0].cloaked=1; break;
            case 34: fake[0].iconic=1; break;
            case 35: fake[0].pid=fake_owner.pid=999; break;
            case 36: fake_owner.exstyle|=WS_EX_DLGMODALFRAME; break;
            case 37: owner_style_fail=2; break;
            case 38: fake[0].pid=fake_owner.pid=0; break;
            case 39: fake_owner.pid=0; break;
            case 40: fake[0].style=0; break;
            case 41: fake[0].style=WS_THICKFRAME; break;
            case 42: fake[0].owned=3; break;
            }
            invoke_selected(); unchanged(); assert(!positions && !normalizes && !clears);
            if (!trace) assert(!trace_lines);
        }
        /* Modal/modeless/custom/tool/disabled extra top-level surfaces of the
           selected process block the whole command, before ordinary filtering. */
        for (kind=0;kind<5;++kind) {
            selected_setup(4,trace); fake[1].pid=1;
            if (kind==0) { fake[1].owned=1; fake[1].style|=WS_POPUP; }
            if (kind==1) fake[1].exstyle=WS_EX_TOOLWINDOW;
            if (kind==2) fake[1].enabled=0;
            if (kind==3) fake[1].class_kind=5;
            if (kind==4) { /* Explicitly choosing a look-alike modeless dialog. */
                foreground=2; fake[1].owned=1; fake[1].exstyle=WS_EX_APPWINDOW;
                relation_root=relation_rootowner=relation_popup=2;
            }
            invoke_selected(); unchanged(); assert(!positions && !normalizes);
        }
        selected_setup(2,trace); fake[1].pid=1; fake[1].visible=0;
        invoke_selected(); assert(positions==1); /* Hidden auxiliary surface is not moved. */
        selected_setup(2,trace); fake[1].identity_fail=1;
        invoke_selected(); unchanged(); assert(!positions); /* Unknown visible identity fails closed. */
        selected_setup(2,trace); fake[1].pid=0;
        invoke_selected(); unchanged(); assert(!positions);
        selected_setup(4,trace); selected_dialog_during_limits=1;
        invoke_selected(); unchanged(); assert(!positions && !normalizes);
        selected_setup(4,trace); owner_thread_changed=1;
        invoke_selected(); assert(positions==4); /* Same process need not mean same thread. */
        for (kind=1;kind<=3;++kind) {
            selected_setup(4,trace); fake[0].blacklisted=(int)kind;
            invoke_selected(); unchanged(); assert(!positions && !normalizes);
            selected_setup(4,trace); fake[1].blacklisted=(int)kind;
            invoke_selected(); assert(positions==3 && EqualRect(&fake[1].native,&fake[1].original));
        }
        selected_setup(5,trace); maximize(0); invoke_selected(); unchanged(); assert(!normalizes && !positions);
        for (kind=0;kind<6;++kind) for (mask=0;mask<2;++mask) {
            selected_setup(4,trace); if (mask) maximize(0);
            if (kind==0) fake[0].behavior=1; /* +107 px correction by chosen surface. */
            if (kind==1) fake[0].behavior=2;
            if (kind==2) fail_batch=1;
            if (kind==3) fail_defer=1;
            if (kind==4) fail_begin=1;
            if (kind==5) { maximize(0); normalize_effect_at=1; normalize_effect=5; }
            invoke_selected(); unchanged(); assert(!clears);
            if (fake[0].original_placement.showCmd==SW_SHOWMAXIMIZED) assert(max_restores==1);
        }
        /* Link/identity/topology/foreground races in initial validation, either
           capture, and baseline checks all refuse before forward positioning. */
        for (at=1;at<=4;++at) for (kind=1;kind<=16;++kind) {
            selected_setup(4,trace); selected_effect_at=(int)at; selected_effect=(int)kind;
            invoke_selected(); unchanged(); assert(!positions && !normalizes && !clears);
        }
        /* Once identity/authorization changes after the batch, do not force the
           selected window back. Existing guarded recovery can restore peers. */
        for (kind=1;kind<=5;++kind) {
            selected_setup(4,trace); selected_batch_effect=(int)kind;
            invoke_selected(); assert(positions==4 && restores==3 && !clears);
            assert(!EqualRect(&fake[0].native,&fake[0].original));
            for (i=1;i<4;++i) assert(EqualRect(&fake[i].native,&fake[i].original));
        }
        selected_setup(4,trace); maximize(0); normalize_effect_at=1; normalize_effect=2;
        invoke_selected(); assert(!positions && !max_restores && !fake[0].zoomed);
        for (kind=0;kind<6;++kind) {
            selected_setup(4,trace);
            if (kind==0) state.action.ac=1;
            if (kind==1) g_InMouseMove=1;
            if (kind==2) g_InFinishMovement=1;
            if (kind==3) LastWin.hwnd=1;
            if (kind==4) linked_queue.busy=1;
            if (kind==5) linked_move.pending=1;
            invoke_selected(); unchanged(); assert(!selected_scans && !captures && !positions);
        }
        selected_setup(1,trace); last_error=1234567;
        {
            ATGSelectedOwned selected={1,102,1,10,10};
            assert(AutoTileSelectedOwnedValid(&selected) && last_error==1234567);
            owner_style_fail=1;
            assert(!AutoTileSelectedOwnedValid(&selected) && last_error==1234567);
        }
        assert(!strstr(trace_log,"#32770") && !strstr(trace_log,"Class")
               && !strstr(trace_log,"test.exe") && !strstr(trace_log,"C:\\"));
    }
}
/* Counterexample to treating a validated resize target as a main-window
   classifier. These are SYNTHETIC role assignments, not observations of actual
   MobaXterm dialogs. The same EXE/class/topology can represent different roles;
   the Win32 model, like these APIs, has no trusted application-role field. */
static void owned_role_ambiguity(void)
{
    ATGSelectedOwned selected={1,102,1,10,10};
    ATGSnapshot main_sample, dialog_sample;
    TCHAR main_class[64], dialog_class[64], main_exe[MAX_PATH], dialog_exe[MAX_PATH];
    selected_setup(2,0);
    model_class="TMobaXtermForm"; model_image="MobaXterm.exe";
    fake[1].pid=1; fake[1].visible=0; /* Hidden auxiliary surface in this scenario. */
    assert(GetClassName(1,main_class,ARR_SZ(main_class)));
    assert(GetWindowProgName(1,main_exe,ARR_SZ(main_exe)));
    assert(AutoTileSelectedOwnedValid(&selected));
    assert(AutoTileCapture(1,&main_sample,&selected));
    /* In a different UI state the sole visible surface is a custom/modeless
       dialog sharing the class; a real main surface may be hidden. Changing
       only its semantic role leaves every observable in this fixture equal. */
    assert(GetClassName(1,dialog_class,ARR_SZ(dialog_class)));
    assert(GetWindowProgName(1,dialog_exe,ARR_SZ(dialog_exe)));
    assert(!strcmp(main_class,dialog_class) && !strcmp(main_exe,dialog_exe));
    assert(AutoTileSelectedOwnedValid(&selected));
    assert(AutoTileCapture(1,&dialog_sample,&selected));
    assert(AutoTileUnchanged(&main_sample,&dialog_sample));
    /* Ordinary mode rejects both, including when an unrelated app has focus.
       Explicit selection is intentional authorization, not proof of role. */
    foreground=0;
    main_sample.selected=NULL;
    assert(!AutoTileEligible(1,&main_sample,NULL));
    assert(AutoTileCapture(1,&dialog_sample,NULL)==0);
    assert(!positions && !normalizes && !trace_lines);
}

static void ordinary_owned_refusals(void)
{
    unsigned trace, n, show, focus, role, kind;
    for (trace=0;trace<=1;++trace)
    for (n=1;n<=6;++n) {
        if (n==2 || n==3) continue;
        for (show=0;show<3;++show) for (focus=0;focus<3;++focus)
        for (role=0;role<8;++role) {
            selected_setup(n,trace);
            model_class="TMobaXtermForm"; model_image="MobaXterm.exe";
            foreground=focus==0 ? 0 : focus==1 ? 1 : 101; /* Unrelated foreground. */
            if (show==1) maximize(0);
            if (show==2) { /* Ordinary mouse-resized surface. */
                fake[0].native=fake[0].original=(RECT){31,40,811,740};
                fake[0].placement.rcNormalPosition=fake[0].native;
                fake[0].original_placement=fake[0].placement;
            }
            if (role==1) { /* Observed modal state: both surfaces disabled. */
                fake[0].enabled=fake_owner.enabled=0; model_class="#32770";
            }
            if (role==2) model_class="TModelessDialog";
            /* role 3: synthetic custom dialog indistinguishable from role 0. */
            if (role==4) model_image="OtherApplication.exe";
            if (role==5) model_class="TMobaXtermFormNext"; /* Hypothetical changed build. */
            if (role==6) fake_owner.pid=7654321;
            if (role==7) { class_query_fail=n==1; fake[0].name_fail=1; }
            invoke();
            assert(EqualRect(&fake[0].native,&fake[0].original));
            assert(AutoTileSamePlacement(&fake[0].placement,&fake[0].original_placement));
            assert(!normalizes && !max_restores && !foreground_queries && !selected_scans);
            if (n<=5) assert(positions==n-1 && clears==n-1);
            else { unchanged(); assert(!positions && !clears); } /* Five eligible peers. */
            if (!trace) assert(!trace_lines && !owner_queries && !diagnostic_queries && !traced_process_calls);
            assert(!strstr(trace_log,"TMobaXtermForm") && !strstr(trace_log,"MobaXterm.exe")
                   && !strstr(trace_log,"OtherApplication.exe") && !strstr(trace_log,"TModelessDialog"));
        }
    }
    /* Peer limits/blacklists and guarded recovery remain active while a
       Moba-like main surface or a look-alike owned dialog is omitted. */
    for (trace=0;trace<=1;++trace) for (kind=0;kind<6;++kind) {
        selected_setup(4,trace); foreground=101;
        model_class="TMobaXtermForm"; model_image="MobaXterm.exe";
        maximize(0);
        if (kind<3) fake[1].blacklisted=(int)kind+1;
        if (kind==3) fake[1].lo.x=1800;
        if (kind==4) fake[1].behavior=1;
        if (kind==5) fail_batch=1;
        invoke();
        assert(EqualRect(&fake[0].native,&fake[0].original) && !normalizes && !max_restores);
        if (kind<3) {
            assert(positions==2 && clears==2);
            assert(EqualRect(&fake[1].native,&fake[1].original));
        } else { unchanged(); assert(!clears); }
        if (kind==3) assert(!positions);
        if (kind>=4) assert(restores==3);
    }
    /* An owned dialog appears while a peer's min/max callback runs. The
       subsequent capture may observe it, but it never becomes a participant. */
    for (trace=0;trace<=1;++trace) for (kind=1;kind<=2;++kind) {
        selected_setup(4,trace); foreground=101; ordinary_dialog_during_limits=(int)kind;
        model_class="TMobaXtermForm"; model_image="MobaXterm.exe";
        invoke(); unchanged();
        assert(!positions && !clears && !normalizes && !selected_scans && !foreground_queries);
    }
}
static void profile_setup(unsigned n, unsigned trace, int enabled)
{
    ATGOwnedProfile p;
    selected_setup(n,trace); foreground=101;
    model_class="TrustedMain"; model_owner_class="ToolkitOwner";
    model_image="PortableRenamed.exe";
    enumerate_owner=1; fake_owner.monitor=1;
    assert(ATGOwnedEnroll(1,&p) && p.valid && !p.enabled);
    assert(ATGOwnedProfileSave("test.ini",0,&p));
    if (enabled) assert(ATGOwnedProfileEnable("test.ini",0,1));
    AutoTileOwnedLoad("test.ini");
    assert(auto_tile_owned_enabled==(unsigned)enabled);
}

static void profile_store_tests(void)
{
    ATGOwnedProfile p, readback;
    unsigned i;
    profile_setup(1,0,0);
    assert(ATGOwnedProfileRead("test.ini",0,&p) && !p.enabled);
    assert(!strcmp(p.path,"C:\\Apps\\PortableRenamed.exe") && strlen(p.digest)==64);
    assert(!strcmp(p.candidate_class,"TrustedMain") && !strcmp(p.owner_class,"ToolkitOwner"));
    /* Save ignores an input enabled flag: enrollment always starts disabled. */
    p.enabled=1; assert(ATGOwnedProfileSave("test.ini",1,&p));
    assert(ATGOwnedProfileRead("test.ini",1,&readback) && !readback.enabled);
    assert(ATGOwnedProfileEnable("test.ini",1,1));
    assert(ATGOwnedProfileRead("test.ini",1,&readback) && readback.enabled);
    assert(ATGOwnedProfileDelete("test.ini",1));
    assert(!ATGOwnedProfileRead("test.ini",1,&readback) && !readback.enabled);
    {
        unsigned writes=ini_writes;
        ATGOwnedProfile unsafe=p;
        strcpy(unsafe.candidate_class,"Main\r\nEnabled=1");
        assert(!ATGOwnedProfileSave("test.ini",1,&unsafe) && ini_writes==writes);
        strcpy(unsafe.candidate_class,"\"quoted\"");
        assert(!ATGOwnedProfileSave("test.ini",1,&unsafe) && ini_writes==writes);
    }
    for (i=1;i<=12;++i) {
        mem00(ini_values,sizeof ini_values); ini_writes=0; ini_fail_write=(int)i;
        assert(!ATGOwnedProfileSave("test.ini",0,&p));
        ATGOwnedProfileRead("test.ini",0,&readback); assert(!readback.enabled);
        AutoTileOwnedLoad("test.ini"); assert(!auto_tile_owned_enabled);
    }
    ini_fail_write=0; assert(ATGOwnedProfileSave("test.ini",0,&p));
    assert(ATGOwnedProfileEnable("test.ini",0,1));
    for (i=0;i<6;++i) {
        assert(ATGOwnedProfileSave("test.ini",0,&p));
        assert(ATGOwnedProfileEnable("test.ini",0,1));
        if(i==0) strcpy(ini_values[0][ini_key("Schema")],"2");
        if(i==1) ini_values[0][ini_key("SHA256")][0]='z';
        if(i==2) strcpy(ini_values[0][ini_key("Path")],"\\\\server\\app.exe");
        if(i==3) strcpy(ini_values[0][ini_key("Class")],"#32770");
        if(i==4) strcpy(ini_values[0][ini_key("Style")],"ffffffff");
        if(i==5) strcpy(ini_values[0][ini_key("Enabled")],"yes");
        assert(!ATGOwnedProfileRead("test.ini",0,&readback) && readback.enabled);
        AutoTileOwnedLoad("test.ini"); invoke(); unchanged(); assert(!positions);
        assert(ATGOwnedProfileEnable("test.ini",0,0)); /* Invalid records remain disableable. */
    }
    assert(ATGOwnedProfileDelete("test.ini",0)); AutoTileOwnedLoad("test.ini");
    assert(!auto_tile_owned_enabled && !image_live && image_opens==image_closes);
}

static void registered_profile_layouts(void)
{
    unsigned trace, n, mask, i, kind;
    for(trace=0;trace<=1;++trace) {
        for(n=1;n<=4;++n) for(mask=0;mask<(1u<<n);++mask) {
            profile_setup(n,trace,1);
            for(i=0;i<n;++i) if(mask&(1u<<i)) maximize(i);
            invoke(); assert(positions==n && clears==n && !restores && !max_restores);
            assert(!foreground_queries && !image_live && image_opens==image_closes);
            i=positions; invoke(); assert(positions==i); /* Fixed point, fresh operation witnesses. */
            assert(!image_live && image_opens==image_closes);
            if(!trace) assert(!trace_lines);
        }
        profile_setup(4,trace,0);
        {
            unsigned scans=selected_scans, reads=ini_reads, opens=image_opens;
            invoke(); assert(positions==3 && !foreground_queries && selected_scans==scans);
            assert(ini_reads==reads && image_opens==opens); /* Disabled means original path. */
        }
        profile_setup(4,trace,1);
        assert(ATGOwnedProfileEnable("test.ini",0,0)); AutoTileOwnedLoad("test.ini");
        invoke(); assert(positions==3 && EqualRect(&fake[0].native,&fake[0].original));
        profile_setup(4,trace,1);
        assert(ATGOwnedProfileDelete("test.ini",0)); AutoTileOwnedLoad("test.ini");
        invoke(); assert(positions==3);
        /* A changed binary, classes, topology or an unreadable participant
           cannot silently fall back to arranging only its peers. */
        for(kind=0;kind<22;++kind) {
            profile_setup(4,trace,1);
            if(kind==0) image_digest[0]='b';
            if(kind==1) image_open_fail=1;
            if(kind==2) model_class="ChangedMain";
            if(kind==3) model_owner_class="ChangedOwner";
            if(kind==4) fake[0].enabled=0;
            if(kind==5) fake_owner.enabled=0;
            if(kind==6) fake_owner.visible=0;
            if(kind==7) fake[0].style|=WS_POPUP;
            if(kind==8) fake[0].exstyle=0;
            if(kind==9) relation_popup=102;
            if(kind==10) fake[0].owned=0;
            if(kind==11) fake_owner.pid++;
            if(kind==12) fake[0].name_fail=1;
            if(kind==13) fake[0].min_fail=1;
            if(kind==14) fake[0].dpi=120;
            if(kind==15) fake[0].lo.x=1800;
            if(kind==16) fake[0].blacklisted=1;
            if(kind==17) fake[0].blacklisted=2;
            if(kind==18) fake[0].blacklisted=3;
            if(kind==19) model_image="RenamedAgain.exe";
            if(kind==20) strcpy(ini_values[0][ini_key("Schema")],"99");
            if(kind==21) model_class="#32770";
            if(kind==20) AutoTileOwnedLoad("test.ini");
            invoke(); unchanged(); assert(!positions && !normalizes && !clears && !image_live);
            assert(!foreground_queries);
        }
        /* Ordinary-app peers keep their previous blacklist behavior. */
        for(kind=1;kind<=3;++kind) {
            profile_setup(4,trace,1); fake[1].blacklisted=(int)kind;
            invoke(); assert(positions==3 && EqualRect(&fake[1].native,&fake[1].original));
        }
        /* Visible modal/modeless/custom/tool surfaces of the trusted process
           block even if ordinary eligibility would have omitted that surface. */
        for(kind=0;kind<5;++kind) {
            profile_setup(4,trace,1); fake[1].pid=1;
            if(kind==0) fake[1].enabled=0;
            if(kind==1) fake[1].exstyle=WS_EX_TOOLWINDOW;
            if(kind==2) { fake[1].owned=1; fake[1].style|=WS_POPUP; }
            if(kind==3) fake[1].class_override="#32770";
            if(kind==4) fake[1].class_override="TrustedMain";
            invoke(); unchanged(); assert(!positions && !normalizes);
        }
        /* Same class in an unregistered image is not admitted; the ambiguity
           cancels the opt-in layout rather than trusting its style/name. */
        profile_setup(4,trace,1);
        fake[1].owned=1; fake[1].class_override="TrustedMain";
        invoke(); unchanged(); assert(!positions);
        /* Multiple instances of the registered image on this monitor. */
        profile_setup(4,trace,1);
        fake[1].owned=2; fake[1].class_override="TrustedMain";
        fake[1].path_override="C:\\Apps\\PortableRenamed.exe";
        invoke(); unchanged(); assert(!positions);
        /* Duplicate enabled registrations are also ambiguous. */
        profile_setup(4,trace,1);
        {
            ATGOwnedProfile p;
            assert(ATGOwnedProfileRead("test.ini",0,&p));
            assert(ATGOwnedProfileSave("test.ini",1,&p) && ATGOwnedProfileEnable("test.ini",1,1));
            AutoTileOwnedLoad("test.ini"); invoke(); unchanged(); assert(!positions);
        }
        profile_setup(5,trace,1); maximize(0); invoke(); unchanged(); assert(!positions && !normalizes);
        for(kind=0;kind<4;++kind) for(mask=0;mask<2;++mask) {
            profile_setup(4,trace,1); if(mask) maximize(0);
            if(kind==0) fake[0].behavior=1;
            if(kind==1) fail_batch=1;
            if(kind==2) fail_begin=1;
            if(kind==3) fail_defer=1;
            invoke(); unchanged(); assert(!clears && !image_live);
            if(mask) assert(max_restores==1);
        }
        for(kind=1;kind<=9;++kind) {
            profile_setup(4,trace,1); profile_limit_effect=(int)kind;
            invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
        }
        for(kind=1;kind<=2;++kind) {
            profile_setup(4,trace,1); ordinary_dialog_during_limits=(int)kind;
            invoke(); unchanged(); assert(!positions && !normalizes);
        }
        for(kind=1;kind<=3;++kind) {
            profile_setup(4,trace,1); profile_batch_effect=(int)kind;
            invoke(); assert(positions==4 && restores==3 && !clears && !image_live);
            assert(!EqualRect(&fake[0].native,&fake[0].original));
            for(i=1;i<4;++i) assert(EqualRect(&fake[i].native,&fake[i].original));
        }
        /* The explicit Selected tool ignores profile configuration, as before. */
        profile_setup(1,trace,1); strcpy(ini_values[0][ini_key("Schema")],"99");
        AutoTileOwnedLoad("test.ini"); foreground=1;
        invoke_selected(); assert(positions==1);
        assert(!strstr(trace_log,"PortableRenamed.exe") && !strstr(trace_log,"TrustedMain")
               && !strstr(trace_log,"ToolkitOwner") && !strstr(trace_log,"aaaaaaaa"));
        /* Two independently trusted applications use the same generic guard. */
        profile_setup(4,trace,1);
        fake[1].pid=55; fake[1].owned=2; fake[1].exstyle=WS_EX_APPWINDOW; fake[1].root_self=1;
        fake[1].class_override="OtherMain"; fake[1].path_override="C:\\Apps\\Other.exe";
        fake_owner2=fake_owner; fake_owner2.pid=55;
        fake_owner2.class_override="OtherOwner"; fake_owner2.path_override="C:\\Apps\\Other.exe";
        {
            ATGOwnedProfile p;
            assert(ATGOwnedEnroll(2,&p));
            assert(ATGOwnedProfileSave("test.ini",1,&p) && ATGOwnedProfileEnable("test.ini",1,1));
            AutoTileOwnedLoad("test.ini"); invoke(); assert(positions==4 && !image_live);
        }
        /* Same generic topology in an unrelated class/image stays excluded. */
        profile_setup(4,trace,1);
        fake[1].owned=2; fake[1].exstyle=WS_EX_APPWINDOW; fake[1].root_self=1;
        fake[1].class_override="UntrustedMain";
        invoke(); assert(positions==3 && EqualRect(&fake[1].native,&fake[1].original));
        /* A profile is consent, not a semantic-role certificate. A synthetic
           indistinguishable custom surface can match if the real main is hidden. */
        profile_setup(2,trace,1); fake[1].pid=1; fake[1].visible=0;
        invoke(); assert(positions==1 && EqualRect(&fake[1].native,&fake[1].original));
    }
}
static void registered_minimized_regression(void)
{
    ATGOwnedOperation op;
    ATGSnapshot snapshot;
    int ok;
    profile_setup(4,1,1); fake[0].iconic=1;
    mem00(&op,sizeof op); op.monitor=1;
    memcpy(op.profiles,auto_tile_owned_profiles,sizeof op.profiles);
    assert(AutoTileOwnedCheck(&op) && op.pins[0].hwnd==1);
    assert(op.states[0]==ATG_OWNED_MINIMIZED && !AutoTileOwnedParticipant(&op,1));
    ok=AutoTileCaptureOwned(1,&snapshot,NULL,&op);
    if (!ok) fprintf(stderr,"minimized profile regression: captured=%u failed=%d\n%s",snapshot.count,op.failed,trace_log);
    assert(ok && snapshot.count==3);
    assert(!strstr(trace_log,"profile-not-captured"));
    /* Restoring does not admit a new participant halfway through this call. */
    fake[0].iconic=0; assert(!AutoTileOwnedCheck(&op) && op.failed);
    fake[0].iconic=1; assert(!AutoTileOwnedCheck(&op)); /* Failure latches. */
    AutoTileOwnedClose(&op);
}

static void registered_minimized_layouts(void)
{
    unsigned trace, peers, cycle, i, kind, minimized, at;
    for (trace=0;trace<=1;++trace) {
        /* Presence and participation differ. A fresh invocation restores
           participation automatically, without foreground or new enrollment. */
        for (peers=0;peers<=4;++peers) {
            profile_setup(peers+1,trace,1);
            for (cycle=0;cycle<6;++cycle) {
                unsigned moved=positions, cleared=clears;
                int inactive=!(cycle%2);
                for (i=0;i<count;++i) {
                    fake[i].native=fake[i].original;
                    fake[i].placement=fake[i].original_placement;
                }
                fake[0].iconic=inactive;
                invoke();
                if (inactive || peers<4) {
                    unsigned expected=peers+!inactive;
                    assert(positions==moved+expected && clears==cleared+expected);
                } else assert(positions==moved && clears==cleared); /* Five active. */
                if (inactive || peers==4) assert(EqualRect(&fake[0].native,&fake[0].original));
                assert(!normalizes && !restores && !foreground_queries && !image_live);
                assert(image_opens==image_closes);
                if (!trace) assert(!trace_lines);
            }
        }
        /* Closed/absent and disabled registrations retain the old peer path. */
        profile_setup(4,trace,1);
        fake[0].visible=fake_owner.visible=0; owned_exists=owner_exists=0;
        invoke(); assert(positions==3 && image_opens==1 && !image_live); /* Enrollment only. */
        /* Inactive mains are not sized, normalized or tested as participants;
           their profile identity/topology is nevertheless still required. */
        for (kind=0;kind<6;++kind) {
            profile_setup(4,trace,1); fake[0].iconic=1;
            if (kind<3) fake[0].blacklisted=(int)kind+1;
            if (kind==3) fake[0].min_fail=1;
            if (kind==4) fake[0].dpi=120;
            if (kind==5) fake[0].lo.x=1800;
            invoke(); assert(positions==3 && !normalizes && !image_live);
            assert(EqualRect(&fake[0].native,&fake[0].original));
        }
        for (kind=0;kind<2;++kind) {
            unsigned scans, reads, opens;
            profile_setup(5,trace,0); fake[0].iconic=1;
            if (kind) { mem00(auto_tile_owned_profiles,sizeof auto_tile_owned_profiles); }
            scans=selected_scans; reads=ini_reads; opens=image_opens;
            invoke(); assert(positions==4 && !foreground_queries && selected_scans==scans);
            assert(reads==ini_reads && opens==image_opens);
        }
        /* Minimization never excuses bad identity, build, classes or owner. */
        for (kind=0;kind<16;++kind) {
            profile_setup(4,trace,1); fake[0].iconic=1;
            if (kind==0) image_digest[0]='b';
            if (kind==1) image_open_fail=1;
            if (kind==2) model_class="ChangedMain";
            if (kind==3) model_owner_class="ChangedOwner";
            if (kind==4) fake[0].owned=2;
            if (kind==5) fake_owner.pid++;
            if (kind==6) fake[0].enabled=0;
            if (kind==7) fake_owner.enabled=0;
            if (kind==8) fake[0].name_fail=1;
            if (kind==9) fake[0].exstyle=0;
            if (kind==10) relation_popup=102;
            if (kind==11) { strcpy(ini_values[0][ini_key("Schema")],"99"); AutoTileOwnedLoad("test.ini"); }
            if (kind==12) owner_race=1;
            if (kind==13) owner_race=2;
            if (kind==14) fake_owner.visible=0;
            if (kind==15) class_query_fail=1;
            invoke(); unchanged(); assert(!positions && !normalizes && !clears && !image_live);
        }
        /* Visible modal/modeless/custom surfaces still block minimized mains,
           even if the dialog is disabled, iconic or on another monitor. */
        for (kind=0;kind<8;++kind) {
            profile_setup(4,trace,1); fake[0].iconic=1;
            fake[1].pid=1; fake[1].owned=1; fake[1].class_override="CustomDialog";
            if (kind==0) { fake[1].class_override="#32770"; fake[0].enabled=fake_owner.enabled=0; }
            if (kind==1) fake[1].enabled=0;
            if (kind==2) fake[1].exstyle=WS_EX_TOOLWINDOW;
            if (kind==3) fake[1].iconic=1;
            if (kind==4) fake[1].monitor=2;
            if (kind==5) fake[0].visible=0; /* Hidden main is not verified inactivity. */
            if (kind==6) fake[1].class_override="TrustedMain";
            if (kind==7) { fake[0].visible=0; fake[1].class_override="#32770"; }
            invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
        }
        /* State changes inside the profile scan/guard and after either capture
           cancel, in both directions. A stable minimize before scanning works. */
        for (minimized=0;minimized<=1;++minimized) {
            for (at=1;at<=12;++at) {
                profile_setup(4,trace,1); fake[0].iconic=(int)minimized;
                iconic_reads=0; iconic_flip_at=at;
                invoke();
                if (at==1) assert(positions==3+minimized); /* Before the first observation. */
                else { assert(!positions && !normalizes && !image_live); unchanged(); }
            }
            for (at=2;at<=4;++at) {
                profile_setup(4,trace,1); fake[0].iconic=(int)minimized;
                selected_effect=17; selected_effect_at=(int)selected_scans+(int)at;
                invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
                assert(fake[0].iconic==!(int)minimized);
            }
            for (at=1;at<=2;++at) {
                profile_setup(4,trace,1); fake[0].iconic=(int)minimized; capture_flip_at=at;
                invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
            }
            profile_setup(4,trace,1); fake[0].iconic=(int)minimized; profile_limit_effect=10;
            invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
            /* After the batch, restore safe peers; do not undo the external
               minimize/restore or force the profiled main's old geometry. */
            profile_setup(4,trace,1); fake[0].iconic=(int)minimized; profile_batch_effect=4;
            invoke(); assert(positions==4-minimized && restores==3 && !clears && !image_live);
            assert(fake[0].iconic==!(int)minimized);
            for (i=1;i<4;++i) assert(EqualRect(&fake[i].native,&fake[i].original));
            /* A dialog during a peer's min/max callback also cancels inactivity. */
            for (kind=1;kind<=2;++kind) {
                profile_setup(4,trace,1); fake[0].iconic=(int)minimized;
                ordinary_dialog_during_limits=(int)kind;
                invoke(); unchanged(); assert(!positions && !normalizes && !image_live);
            }
        }
        /* An inactive witness cannot be silently replaced by another HWND. */
        {
            ATGOwnedOperation op;
            profile_setup(3,trace,1); fake[0].iconic=1;
            mem00(&op,sizeof op); op.monitor=1;
            memcpy(op.profiles,auto_tile_owned_profiles,sizeof op.profiles);
            assert(AutoTileOwnedCheck(&op));
            fake[0].visible=0; fake[1].pid=1; fake[1].owned=1;
            fake[1].exstyle=WS_EX_APPWINDOW; fake[1].root_self=1;
            fake[1].class_override="TrustedMain"; fake[1].iconic=1;
            assert(!AutoTileOwnedCheck(&op) && op.pins[0].hwnd==1);
            AutoTileOwnedClose(&op); unchanged();
        }
        /* Two independently registered apps may be active/inactive together. */
        for (kind=0;kind<4;++kind) {
            ATGOwnedProfile p;
            profile_setup(4,trace,1);
            fake[1].pid=55; fake[1].owned=2; fake[1].exstyle=WS_EX_APPWINDOW; fake[1].root_self=1;
            fake[1].class_override="OtherMain"; fake[1].path_override="C:\\Apps\\Other.exe";
            fake_owner2=fake_owner; fake_owner2.pid=55;
            fake_owner2.class_override="OtherOwner"; fake_owner2.path_override="C:\\Apps\\Other.exe";
            assert(ATGOwnedEnroll(2,&p));
            assert(ATGOwnedProfileSave("test.ini",1,&p) && ATGOwnedProfileEnable("test.ini",1,1));
            AutoTileOwnedLoad("test.ini");
            fake[0].iconic=!!(kind&1); fake[1].iconic=!!(kind&2);
            invoke(); assert(positions==4u-!!(kind&1)-!!(kind&2) && !image_live);
            if (kind&1) assert(EqualRect(&fake[0].native,&fake[0].original));
            if (kind&2) assert(EqualRect(&fake[1].native,&fake[1].original));
        }
        /* Peers still obey blacklists/min-max and guarded rollback. */
        for (kind=1;kind<=3;++kind) {
            profile_setup(5,trace,1); fake[0].iconic=1; fake[1].blacklisted=(int)kind;
            invoke(); assert(positions==3 && EqualRect(&fake[0].native,&fake[0].original));
            assert(EqualRect(&fake[1].native,&fake[1].original));
        }
        profile_setup(5,trace,1); fake[0].iconic=1; fake[1].lo.x=1800;
        invoke(); unchanged(); assert(!positions && !normalizes);
        profile_setup(5,trace,1); fake[0].iconic=1; fake[1].behavior=1;
        invoke(); unchanged(); assert(positions==4 && restores==4 && !clears);
        profile_setup(5,trace,1); fake[0].iconic=1; maximize(1); fail_batch=1;
        invoke(); unchanged(); assert(normalizes==1 && max_restores==1 && !clears);
        /* Selected keeps its independent foreground and eligibility contract. */
        profile_setup(4,trace,1); fake[0].iconic=1; foreground=1;
        invoke_selected(); unchanged(); assert(!positions && !normalizes);
    }
}

static void expansion_baseline(int profile)
{
    unsigned i;
    if (profile) profile_setup(4,0,1); else reset(4);
    invoke(); assert(positions==4);
    for(i=0;i<4;++i) { fake[i].original=fake[i].native; fake[i].original_placement=fake[i].placement; }
    captures=positions=restores=batches=clears=0;
    foreground=1;
}
static void expand_adapter(void)
{
    unsigned q,axes,profile,kind,i;
    for(profile=0;profile<=1;++profile) for(q=0;q<4;++q) for(axes=1;axes<=3;++axes) {
        RECT first[4]; unsigned moved;
        expansion_baseline((int)profile); foreground=q+1;
        reverse_enum=1;
        AutoTileExpand(axes);
        assert(positions && batches==1 && clears==4 && !normalizes && !restores);
        assert(!image_live);
        for(i=0;i<4;++i) {
            RECT r;
            first[i]=fake[i].native;
            assert(!DwmGetWindowAttributeL(i+1,9,&r,sizeof r));
            assert(r.left>=0 && r.top>=0 && r.right<=1200 && r.bottom<=960);
            if(!(axes&1)) assert(r.left==(i&1 ? 600 : 0) && r.right==(i&1 ? 1200 : 600));
            if(!(axes&2)) assert(r.top==(i&2 ? 480 : 0) && r.bottom==(i&2 ? 960 : 480));
        }
        moved=positions;
        AutoTileExpand(axes); assert(positions==moved && batches==1);
        for(i=0;i<4;++i) assert(EqualRect(&first[i],&fake[i].native));
        AutoTileExpand(3); AutoTileExpand(1); AutoTileExpand(2);
        invoke();
        for(i=0;i<4;++i) assert(EqualRect(&fake[i].native,&fake[i].original));
    }
    for(profile=0;profile<=1;++profile) for(kind=0;kind<19;++kind) {
        expansion_baseline((int)profile);
        switch(kind) {
        case 0: foreground=0; break;
        case 1: foreground=102; break; /* Owner is never promoted to main. */
        case 2: fake[0].iconic=1; break;
        case 3: fake[2].iconic=1; break;
        case 4: fake[1].enabled=0; break;
        case 5: fake[1].native.left++; break;
        case 6: fake[0].dpi=120; break;
        case 7: fake[2].lo.x=800; break;
        case 8: fake[3].hi.y=100; break;
        case 9: fake[1].monitor=2; break;
        case 10: maximize(0); break;
        case 11: fake[2].min_fail=1; break;
        case 12: state.action.ac=1; break;
        case 13: linked_queue.busy=1; break;
        case 14: change_preflight=1; break;
        case 15: fake[0].placement.showCmd=SW_SHOWMAXIMIZED; break;
        case 16: capture_flip_at=2; break;
        case 17: ordinary_dialog_during_limits=1; break;
        case 18: fake[1].owned=1; break;
        }
        AutoTileExpand(3); assert(!positions && !normalizes && !restores && !image_live);
    }
    for(kind=0;kind<4;++kind) {
        reset(kind); foreground=kind ? 1 : 0; AutoTileExpand(3); unchanged(); assert(!positions);
    }
    reset(5); foreground=1; AutoTileExpand(3); unchanged(); assert(!positions);
    /* A fifth eligible surface cancels even when four still form a grid. */
    expansion_baseline(0); count=5; fake[4]=fake[3]; foreground=1;
    AutoTileExpand(3); assert(!positions);
    expansion_baseline(0); count=5; fake[4]=fake[3]; fake[4].visible=0; foreground=5;
    AutoTileExpand(3); assert(!positions); /* Same monitor, foreground is no participant. */
    for(profile=0;profile<=1;++profile) for(kind=0;kind<5;++kind) {
        expansion_baseline((int)profile);
        if(kind==0) fail_begin=1;
        if(kind==1) fail_defer=1;
        if(kind==2) fail_batch=1;
        if(kind==3) fake[2].behavior=1; /* Application clamps during batch. */
        if(kind==4) selected_batch_effect=4; /* Foreground changes during apply. */
        AutoTileExpand(3); unchanged(); assert(!clears && !normalizes && !image_live);
    }
    /* Registered build/profile witness changes cancel before apply. */
    for(kind=1;kind<=10;++kind) {
        expansion_baseline(1); profile_limit_effect=(int)kind;
        AutoTileExpand(3); unchanged(); assert(!positions && !image_live);
    }
    /* Valid nonuniform grid, foreground among peers of registered main. */
    expansion_baseline(1); foreground=4; AutoTileExpand(1); foreground=2; AutoTileExpand(2);
    positions=0; foreground=1; AutoTileExpand(3); assert(positions && !image_live);
    puts("auto_tile_expand_win_stubs: foreground, four corners, nonuniform grids, registered profiles, modal/minimize/state refusals and batch rollback passed (no Windows GUI)");
}

int main(void)
{
    expand_adapter();
    layouts_and_refusals(); exclusions(); recovery_and_drag(); maximized_and_overhang(); diagnostic_trace(); limited_fallback_and_owner_safety();
    owner_topology_trace();
    explicitly_selected_owned();
    owned_role_ambiguity(); ordinary_owned_refusals();
    profile_store_tests(); registered_profile_layouts(); registered_minimized_regression(); registered_minimized_layouts();
    puts("auto_tile_grid_minimized_profiles: inactive witnesses, 1-4 peers, repeated restore, state/identity races, dialogs, mixed profiles and rollback passed (no Windows GUI)");
    puts("auto_tile_grid_profiles: disabled consent persistence, build/class checks, foreground-independent layouts, ambiguous/dialog refusals and guarded rollback passed");
    puts("auto_tile_grid_owned_roles: synthetic EXE/class role collision, ordinary focus-independent owner refusal, dialog states, peers and rollback passed (no implicit admission without registered consent)");
    puts("auto_tile_grid_selected: one-shot selection, strict guards, modal/modeless dialogs, stale identities, blacklists, five windows and guarded rollback passed (no Windows GUI)");
    puts("auto_tile_grid_win_stubs: layouts, recovery, exclusions, owner topology/races, 4096 trace on/off combinations and fixed log vocabulary passed (no Windows GUI)");
    return 0;
}
