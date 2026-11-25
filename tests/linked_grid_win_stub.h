/* SPDX-License-Identifier: GPL-3.0-or-later
   Deliberately small Win32 model for the production adapter, not a GUI test. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../linked_grid_queue.h"
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef unsigned UINT;
typedef int BOOL;
typedef uintptr_t HWND;
typedef uintptr_t HMONITOR;
typedef uintptr_t HDWP;
typedef intptr_t LPARAM;
typedef uintptr_t DWORD_PTR;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { unsigned cbSize; RECT rcMonitor; } MONITORINFO;
typedef struct { POINT ptMinTrackSize, ptMaxTrackSize; } MINMAXINFO;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#define MONITOR_DEFAULTTONEAREST 2
#define SMTO_ABORTIFHUNG 2
#define WM_GETMINMAXINFO 36
#define SM_CXMINTRACK 0
#define SM_CYMINTRACK 1
#define SM_CXMAXTRACK 2
#define SM_CYMAXTRACK 3
#define SWP_NOACTIVATE 16
#define SWP_NOZORDER 4
#define SWP_NOOWNERZORDER 512
#define RZ_LEFT 1
#define RZ_RIGHT 2
#define RZ_TOP 1
#define RZ_BOTTOM 2
#define ARR_SZ(a) (sizeof(a)/sizeof((a)[0]))
#define mem00(p,n) memset(p,0,n)

static struct { int LinkedGridResize, FullWin; } conf;
static struct { HWND hwnd; int mdiclient; POINT resize; struct { UINT dpi; } origin; } state;
static struct { int Windows; } BlkLst;
static LGQueue linked_queue;
static uintptr_t linked_dispatch;
enum { EXACT, ADJUST, REJECT, REPOSITION };
enum { NO_EFFECT, MOVE_EXTERNAL, RESIZE_EXTERNAL, MOVE_TO_REQUESTED, DISAPPEAR, REPLACE_IDENTITY,
       CANCEL_OPERATION, STALE_OPERATION };
/* Effects run inside a native read or a positioning callback, after the
   forward batch. They model races without changing the production adapter. */
static int read_effect, read_effect_at, restore_effect;
static unsigned effect_cell, post_apply_reads[4], restore_calls[4];
static void inject_effect(int effect);
static struct FakeWindow {
    RECT native, dwm, before;
    int behavior, lag, missing, refuse_restore, replaced;
    LONG correction;
    LONG min_width, min_height;
} fake[4];
static RECT batch_rect[4];
static unsigned batch_mask, position_calls, native_reads, dwm_reads, debug_lines;
static int batch_fail, batch_calls, cancel_in_batch;
static char debug_log[65536];
static size_t debug_used;
static struct FakeWindow *window(HWND w) { return &fake[w - 1]; }
static BOOL EqualRect(const RECT *a, const RECT *b) { return !memcmp(a,b,sizeof *a); }
static void SetRect(RECT *r, LONG l, LONG t, LONG rr, LONG b) { *r=(RECT){l,t,rr,b}; }
static void SetRectEmpty(RECT *r) { memset(r,0,sizeof *r); }
static DWORD GetTickCount(void) { return 123; }
static DWORD GetLastError(void) { return 5; }
static int wsprintfA(char *s, const char *fmt, ...)
{
    int n;
    va_list args;
    va_start(args,fmt); n=vsprintf(s,fmt,args); va_end(args);
    return n;
}
static void OutputDebugStringA(const char *s)
{
    size_t n=strlen(s);
    ++debug_lines;
    if (debug_used+n+1 < sizeof debug_log) {
        memcpy(debug_log+debug_used,s,n+1); debug_used+=n;
    }
}
static BOOL GetWindowRect(HWND w, RECT *r)
{
    ++native_reads;
    if (batch_calls && ++post_apply_reads[w-1] == (unsigned)read_effect_at
        && w-1 == effect_cell) inject_effect(read_effect);
    if (window(w)->missing) return 0;
    *r=window(w)->native; return 1;
}
static int DwmGetWindowAttributeL(HWND w, int attr, RECT *r, size_t size)
{
    (void)attr; (void)size; ++dwm_reads;
    if (window(w)->missing) return -1;
    *r=window(w)->dwm; return S_OK;
}
/* Same native/DWM subtraction as FixDWMRectLL in unfuck.h. */
static void FixDWMRectLL(HWND w, RECT *r, int gap)
{
    RECT n, d;
    (void)gap;
    if (DwmGetWindowAttributeL(w,9,&d,sizeof d)==S_OK && GetWindowRect(w,&n))
        *r=(RECT){d.left-n.left,d.top-n.top,n.right-d.right,n.bottom-d.bottom};
    else SetRectEmpty(r);
}
static int GetWindowRectLL(HWND w, RECT *r, int gap)
{ (void)gap; return DwmGetWindowAttributeL(w,9,r,sizeof *r)==S_OK; }
static void InflateRectBorder(RECT *r, const RECT *b)
{ r->left-=b->left; r->top-=b->top; r->right+=b->right; r->bottom+=b->bottom; }
static int ShouldResizeTouching(void) { return 1; }
static int ShouldSnapTo(HWND w) { (void)w; return 1; }
static int blacklisted(HWND w, const int *list) { (void)w; (void)list; return 0; }
static int IsZoomed(HWND w) { (void)w; return 0; }
static int IsIconic(HWND w) { (void)w; return 0; }
static int IsVisible(HWND w) { return !window(w)->missing; }
static int IsResizable(HWND w) { (void)w; return 1; }
static UINT GetDpiForWindow(HWND w) { (void)w; return 96; }
static DWORD GetWindowThreadProcessId(HWND w, DWORD *pid)
{ *pid=window(w)->replaced ? 2 : 1; return window(w)->missing ? 0 : 1; }
static HMONITOR MonitorFromWindow(HWND w, int flags) { (void)w; (void)flags; return 1; }
static int GetMonitorInfo(HMONITOR m, MONITORINFO *mi)
{ (void)m; mi->rcMonitor=(RECT){-4000,-4000,4000,4000}; return 1; }
static int RectInRect(const RECT *a, const RECT *b)
{ return b->left>=a->left && b->top>=a->top && b->right<=a->right && b->bottom<=a->bottom; }
static int EnumDesktopWindows(void *desktop, BOOL (*callback)(HWND,LPARAM), LPARAM p)
{
    HWND w;
    (void)desktop;
    for (w=1;w<=4;++w) if (!callback(w,p)) return 0;
    return 1;
}
static int GetSystemMetricsForDpi(int what, UINT dpi)
{ (void)dpi; return what<2 ? 1 : 2000; }
static int SendMessageTimeout(HWND w, int msg, int wp, LPARAM lp, int flags, int ms, DWORD_PTR *result)
{
    MINMAXINFO *mmi=(MINMAXINFO *)lp;
    (void)msg; (void)wp; (void)flags; (void)ms; (void)result;
    mmi->ptMinTrackSize.x=window(w)->min_width;
    mmi->ptMinTrackSize.y=window(w)->min_height;
    return 1;
}
static void apply(HWND w, RECT r)
{
    struct FakeWindow *f=window(w);
    ++position_calls;
    if (f->missing || (f->refuse_restore && EqualRect(&r,&f->before))) return;
    if (!EqualRect(&r,&f->before)) {
        if (f->behavior==REJECT) return;
        if (f->behavior==ADJUST) r.right+=f->correction;
        if (f->behavior==REPOSITION) { r.left+=31; r.right+=31; }
    }
    f->native=r;
    if (!f->lag) f->dwm=(RECT){r.left+8,r.top,r.right-8,r.bottom-8};
}
static HDWP BeginDeferWindowPos(unsigned count)
{ (void)count; batch_mask=0; return 1; }
static HDWP DeferWindowPos(HDWP b, HWND w, void *after, LONG x, LONG y, LONG cx, LONG cy, int flags)
{
    (void)after; (void)flags;
    batch_mask |= 1u<<(w-1); batch_rect[w-1]=(RECT){x,y,x+cx,y+cy}; return b;
}
static int EndDeferWindowPos(HDWP b)
{
    unsigned i;
    (void)b; ++batch_calls;
    for (i=0;i<4;++i) if (batch_mask & (1u<<i)) apply(i+1,batch_rect[i]);
    if (cancel_in_batch) lg_queue_invalidate(&linked_queue);
    return !batch_fail;
}
static void inject_effect(int effect)
{
    struct FakeWindow *f=&fake[effect_cell];
    if (effect==MOVE_EXTERNAL) { f->native.left+=31; f->native.right+=31; }
    if (effect==RESIZE_EXTERNAL) f->native.right+=31;
    if (effect==MOVE_TO_REQUESTED) f->native=batch_rect[effect_cell];
    if (effect==DISAPPEAR) f->missing=1;
    if (effect==REPLACE_IDENTITY) f->replaced=1;
    if (effect==CANCEL_OPERATION) lg_queue_invalidate(&linked_queue);
    if (effect==STALE_OPERATION) {
        lg_queue_invalidate(&linked_queue);
        linked_queue.busy=0;
        (void)lg_queue_enter(&linked_queue,linked_queue.generation);
    }
    if (!f->lag) f->dwm=(RECT){f->native.left+8,f->native.top,
                              f->native.right-8,f->native.bottom-8};
}
static int SetWindowPos(HWND w, void *after, LONG x, LONG y, LONG cx, LONG cy, int flags)
{
    (void)after; (void)flags;
    ++restore_calls[w-1];
    apply(w,(RECT){x,y,x+cx,y+cy});
    if (restore_effect) { int effect=restore_effect; restore_effect=0; inject_effect(effect); }
    return 1;
}
static void ClearRestoreFlagOnResizeIfNeeded(HWND w) { (void)w; }
