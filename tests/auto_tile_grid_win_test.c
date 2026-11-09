/* SPDX-License-Identifier: GPL-3.0-or-later
   Production AutoTileGrid adapter against a deterministic Win32 model.
   This does not model Windows GUI, DPI virtualization or real DWM timing. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
typedef uintptr_t HANDLE;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { unsigned cbSize; RECT rcMonitor, rcWork; } MONITORINFO;
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
#define WS_EX_TOOLWINDOW 4
#define WS_EX_NOACTIVATE 8
#define GW_OWNER 4
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
static struct { int Processes, Windows, Bottommost; } BlkLst;
static LONG generation;
static unsigned count, captures, positions, restores, batches, clears;
static int fail_batch, fail_defer, cancel_batch, start_drag, change_preflight;
static int external_restore, replace_restore;
static const RECT monitor_rect = {0, 0, 1200, 1000};
static const RECT work_rect = {0, 0, 1200, 960};
static struct FakeWindow {
    RECT native, original;
    LONG_PTR style, exstyle;
    HMONITOR monitor;
    DWORD pid;
    int aware, dpi, enabled, visible, owned, zoomed, fullscreen;
    int blacklisted, prop, dwm_fail, min_fail, identity_fail, behavior;
    POINT lo, hi;
} fake[6];
static RECT targets[6];
static unsigned batch_mask;
static struct FakeWindow *win(HWND w) { assert(w>=1 && w<=count); return &fake[w-1]; }
static BOOL EqualRect(const RECT *a, const RECT *b) { return !memcmp(a,b,sizeof *a); }
static void SetRect(RECT *r, LONG l, LONG t, LONG rr, LONG b) { *r=(RECT){l,t,rr,b}; }
static void SubRect(RECT *r, const RECT *n)
{ *r=(RECT){r->left-n->left,r->top-n->top,n->right-r->right,n->bottom-r->bottom}; }
static LONG MovementGeneration(void) { return generation; }
static LONG_PTR GetWindowLongPtr(HWND w, int which) { return which ? win(w)->exstyle : win(w)->style; }
static DWORD GetWindowThreadProcessId(HWND w, DWORD *pid)
{ *pid=win(w)->pid; return win(w)->identity_fail ? 0 : 10; }
static DWORD GetCurrentProcessId(void) { return 999; }
static HWND GetDesktopWindow(void) { return 100; }
static HWND GetShellWindow(void) { return 101; }
static HWND GetWindow(HWND w, int which) { (void)which; return win(w)->owned ? 102 : 0; }
static int IsWindowEnabled(HWND w) { return win(w)->enabled; }
static int IsSnapCandidate(HWND w) { return win(w)->visible; }
static int IsZoomed(HWND w) { return win(w)->zoomed; }
static int IsFullscreen(HWND w) { return win(w)->fullscreen; }
static int GetProp(HWND w, int prop) { return win(w)->prop==prop; }
static HMONITOR MonitorFromWindow(HWND w, int flags) { (void)flags; return win(w)->monitor; }
static int isClassName(HWND w, const char *name) { (void)w; (void)name; return 0; }
static int blacklisted(HWND w, const int *list) { (void)list; return win(w)->blacklisted; }
static int GetWindowProgName(HWND w, TCHAR *buf, size_t n)
{ (void)n; strcpy(buf,"test.exe"); return !win(w)->identity_fail; }
static int lstrcmpi(const TCHAR *a, const char *b) { return strcmp(a,b); }
static UINT GetDpiForWindow(HWND w) { return (UINT)win(w)->dpi; }
static HANDLE dpi_context(HWND w) { return (HANDLE)win(w)->aware; }
static int dpi_awareness(HANDLE context) { return (int)context; }
/* Generic function pointer, matching LoadDLLProc's GetProcAddress contract. */
static void (*LoadDLLProc(const char *dll, const char *name))(void)
{
    (void)dll;
    return !strcmp(name,"GetWindowDpiAwarenessContext")
        ? (void (*)(void))dpi_context : (void (*)(void))dpi_awareness;
}
static int GetWindowRect(HWND w, RECT *r)
{ if (win(w)->identity_fail) return 0; *r=win(w)->native; return 1; }
static int DwmGetWindowAttributeL(HWND w, int attr, RECT *r, size_t size)
{
    (void)attr; (void)size;
    if (win(w)->dwm_fail) return -1;
    *r=win(w)->native; r->left+=8; r->right-=8; r->bottom-=8; return S_OK;
}
static int RectInRect(const RECT *a, const RECT *b)
{ return b->left>=a->left && b->top>=a->top && b->right<=a->right && b->bottom<=a->bottom; }
static int GetMinMaxInfoF(HWND w, POINT *lo, POINT *hi, int ignore)
{
    assert(!ignore); *lo=win(w)->lo; *hi=win(w)->hi;
    if (change_preflight && captures==2 && w==count) fake[0].native.left++;
    return !win(w)->min_fail;
}
static int GetMonitorInfo(HMONITOR m, MONITORINFO *mi)
{ (void)m; mi->rcMonitor=monitor_rect; mi->rcWork=work_rect; return 1; }
static int GetDpiForMonitorL(HMONITOR m, int kind, UINT *x, UINT *y)
{ (void)m; (void)kind; *x=*y=96; return S_OK; }
static int EnumDesktopWindows(void *desktop, BOOL (*callback)(HWND,LPARAM), LPARAM lp)
{
    HWND w; (void)desktop; ++captures;
    for (w=1; w<=count; ++w) if (!callback(w,lp)) return 0;
    return 1;
}
static HMONITOR MonitorFromPoint(POINT p, int flags) { (void)flags; return p.x<1200 ? 1 : 2; }
static HDWP BeginDeferWindowPos(unsigned n) { assert(n>=1 && n<=4); batch_mask=0; return 1; }
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
    }
    if (cancel_batch) ++generation;
    if (start_drag) state.action.ac=1;
    return !fail_batch;
}
static int SetWindowPos(HWND w, void *after, LONG x, LONG y, LONG cx, LONG cy, int flags)
{
    assert(!after && flags==(SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER));
    ++restores; win(w)->native=(RECT){x,y,x+cx,y+cy};
    /* An earlier restore's callback changes another participant. */
    if (external_restore) { fake[1].native.left+=31; fake[1].native.right+=31; external_restore=0; }
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
    external_restore=replace_restore=0;
    mem00(&state,sizeof state); mem00(&LastWin,sizeof LastWin);
    mem00(&linked_queue,sizeof linked_queue); mem00(&linked_move,sizeof linked_move);
    g_InMouseMove=g_InFinishMovement=0;
    mem00(fake,sizeof fake);
    for (i=0;i<n;++i) {
        fake[i].native=fake[i].original=(RECT){20+(LONG)i*30,20,400+(LONG)i*30,320};
        fake[i].style=WS_THICKFRAME; fake[i].monitor=1; fake[i].pid=1;
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
}
static void exclusions(void)
{
    unsigned kind;
    for (kind=0;kind<15;++kind) {
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
        case 8: fake[1].zoomed=1; break;
        case 9: fake[1].fullscreen=1; break;
        case 10: fake[1].blacklisted=1; break;
        case 11: fake[1].prop=APP_MOVEONOFF; break;
        case 12: fake[1].prop=APP_ROLLED; break;
        case 13: fake[1].dwm_fail=1; break;
        case 14: fake[1].native.right=1300; break;
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
int main(void)
{
    layouts_and_refusals(); exclusions(); recovery_and_drag();
    puts("auto_tile_grid_win_stubs: layouts, exclusions, repeat, limits, +107px recovery, guarded restores and drag refusal passed (no Windows GUI)");
    return 0;
}
