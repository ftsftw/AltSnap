/* SPDX-License-Identifier: GPL-3.0-or-later
   Production AutoTileGrid adapter against a deterministic Win32 model.
   This does not model Windows GUI, DPI virtualization or real DWM timing. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
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
} fake[6], fake_owner;
static RECT targets[6];
static unsigned batch_mask;
static struct FakeWindow *win(HWND w) {
    if (w==102) { ++owner_queries; return &fake_owner; }
    assert(w>=1 && w<=count && w<=ARR_SZ(fake)); return &fake[w-1];
}
static BOOL EqualRect(const RECT *a, const RECT *b) { return !memcmp(a,b,sizeof *a); }
static void SetRect(RECT *r, LONG l, LONG t, LONG rr, LONG b) { *r=(RECT){l,t,rr,b}; }
static LONG MovementGeneration(void) { return generation; }
static LONG_PTR GetWindowLongPtr(HWND w, int which) {
    if (w==102 && owner_style_fail) { ++owner_queries; last_error=5; return 0; }
    return which ? win(w)->exstyle : win(w)->style;
}
static DWORD GetWindowThreadProcessId(HWND w, DWORD *pid)
{ process_window=w; *pid=win(w)->pid; return win(w)->identity_fail ? 0 : 10; }
static DWORD GetCurrentProcessId(void) { return 999; }
static HWND GetDesktopWindow(void) { return fake[0].shell_kind==1 ? 1 : 100; }
static HWND GetShellWindow(void) { return fake[0].shell_kind==2 ? 1 : 101; }
static HWND GetWindow(HWND w, int which) {
    assert(which==GW_OWNER); ++owner_link_queries;
    return win(w)->owned==2 ? 103 : win(w)->owned ? 102 : 0;
}
static BOOL IsWindow(HWND w) {
    ++owner_queries;
    return w==102 ? owner_exists : owned_exists;
}
static HWND GetAncestor(HWND w, UINT which) {
    (void)w; ++owner_queries; assert(which==GA_ROOT || which==GA_ROOTOWNER);
    return which==GA_ROOT ? relation_root : relation_rootowner;
}
static HWND GetLastActivePopup(HWND w) {
    assert(w==relation_rootowner); ++owner_queries;
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
static int IsIconic(HWND w) { ++diagnostic_queries; return win(w)->iconic; }
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
{ (void)w; assert(n>5); strcpy(buf,"Class"); return 5; }
static int blacklisted_from_names(const blacklist_t *list, const TCHAR *exe,
                                 const TCHAR *title, const TCHAR *classname)
{
    unsigned w=0;
    assert(sscanf(title,"window%u",&w)==1 && !strcmp(classname,"Class"));
    /* Every list receives the identity validated by the production lookup,
       including when the old process APIs fail or an unrelated cache is stale. */
    assert(!strcmp(exe,win(w)->altsnap_exe ? "AltSnap.exe" : win(w)->limited_only ? "Code.exe" : "test.exe"));
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
int main(void)
{
    layouts_and_refusals(); exclusions(); recovery_and_drag(); maximized_and_overhang(); diagnostic_trace(); limited_fallback_and_owner_safety();
    owner_topology_trace();
    puts("auto_tile_grid_win_stubs: layouts, recovery, exclusions, owner topology/races, 4096 trace on/off combinations and fixed log vocabulary passed (no Windows GUI)");
    return 0;
}
