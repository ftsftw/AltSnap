/* SPDX-License-Identifier: GPL-3.0-or-later
   Private hooks.c adapter. All geometry is in physical screen coordinates,
   using DWM visible bounds with SnapGap=0. Never used for MDI. */
#include "linked_grid.c"
static void ClearRestoreFlagOnResizeIfNeeded(HWND hwnd);

static struct {
    LGDrag drag;
    HWND hwnd[4];
    DWORD pid[4], tid[4];
    UINT dpi[4];
    RECT border[4], actual[4];
    POINT mouse;
    int32_t x, y;
} linked;

/* Bounded scratch storage: overflow rejects capture rather than truncating an
   ambiguity. Enumeration happens only when the resize action starts. */
static struct {
    LGWindow windows[128];
    HWND hwnd[128];
    unsigned count;
    int overflow;
    HMONITOR monitor;
    RECT monitor_rect;
} linked_candidates;

static BOOL CALLBACK EnumLinkedGrid(HWND hwnd, LPARAM unused)
{
    RECT r;
    unsigned i;
    (void)unused;
    if (hwnd != state.hwnd && (!ShouldSnapTo(hwnd)
        || blacklisted(hwnd, &BlkLst.Windows))) return TRUE;
    if (IsZoomed(hwnd) || !IsResizable(hwnd)
        || MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) != linked_candidates.monitor
        || !GetWindowRectLL(hwnd, &r, 0)) return TRUE;
    if (!RectInRect(&linked_candidates.monitor_rect, &r)) return TRUE;
    if (linked_candidates.count == ARR_SZ(linked_candidates.windows)) {
        linked_candidates.overflow = 1;
        return FALSE;
    }
    i = linked_candidates.count++;
    linked_candidates.hwnd[i] = hwnd;
    linked_candidates.windows[i].rect.left = r.left;
    linked_candidates.windows[i].rect.top = r.top;
    linked_candidates.windows[i].rect.right = r.right;
    linked_candidates.windows[i].rect.bottom = r.bottom;
    linked_candidates.windows[i].min_width = 1;
    linked_candidates.windows[i].min_height = 1;
    linked_candidates.windows[i].max_width = INT32_MAX;
    linked_candidates.windows[i].max_height = INT32_MAX;
    return TRUE;
}

static void BeginLinkedGrid(POINT mouse)
{
    LGResult found;
    LGWindow windows[4];
    size_t active = 0;
    unsigned i, q;
    linked.drag.mode = LG_DRAG_LEGACY;
    linked.drag.dirty = 0;
    if (!conf.LinkedGridResize || !ShouldResizeTouching() || state.mdiclient
        || (state.resize.x != RZ_LEFT && state.resize.x != RZ_RIGHT)
        || (state.resize.y != RZ_TOP && state.resize.y != RZ_BOTTOM)) return;
    q = (state.resize.x == RZ_LEFT) | ((state.resize.y == RZ_TOP) << 1);
    linked_candidates.count = 0;
    linked_candidates.overflow = 0;
    linked_candidates.monitor = MonitorFromWindow(state.hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfo(linked_candidates.monitor, &mi)) return;
    linked_candidates.monitor_rect = mi.rcMonitor;
    if (!EnumDesktopWindows(NULL, EnumLinkedGrid, 0) && !linked_candidates.overflow) return;
    if (linked_candidates.overflow) return;
    while (active < linked_candidates.count && linked_candidates.hwnd[active] != state.hwnd)
        ++active;
    if (active == linked_candidates.count) return;
    LGRect a = linked_candidates.windows[active].rect;
    if (lg_resize(linked_candidates.windows, linked_candidates.count, active,
                  (LGQuadrant)q, (q & 1) ? a.left : a.right,
                  (q & 2) ? a.top : a.bottom, &found) != LG_OK) return;
    /* From here a grid was captured. Any failure blocks, never falls back. */
    lg_drag_stop(&linked.drag);
    for (i = 0; i < 4; ++i) {
        HWND hwnd = linked_candidates.hwnd[found.indices[i]];
        POINT lo, hi;
        RECT visible;
        linked.hwnd[i] = hwnd;
        linked.tid[i] = GetWindowThreadProcessId(hwnd, &linked.pid[i]);
        linked.dpi[i] = GetDpiForWindow(hwnd);
        if (linked.dpi[i] != state.origin.dpi || !GetWindowRect(hwnd, &linked.actual[i])) return;
        /* Require DWM bounds, avoiding mixed virtualized/fallback rectangles. */
        if (S_OK != DwmGetWindowAttributeL(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                          &visible, sizeof visible)) return;
        FixDWMRectLL(hwnd, &linked.border[i], 0);
        windows[i] = linked_candidates.windows[found.indices[i]];
        if (visible.left != windows[i].rect.left || visible.top != windows[i].rect.top
            || visible.right != windows[i].rect.right || visible.bottom != windows[i].rect.bottom) return;
        /* A timed-out query cannot establish safe limits. */
        MINMAXINFO mmi;
        DWORD_PTR response;
        mem00(&mmi, sizeof mmi);
        mmi.ptMinTrackSize.x = GetSystemMetricsForDpi(SM_CXMINTRACK, linked.dpi[i]);
        mmi.ptMinTrackSize.y = GetSystemMetricsForDpi(SM_CYMINTRACK, linked.dpi[i]);
        mmi.ptMaxTrackSize.x = GetSystemMetricsForDpi(SM_CXMAXTRACK, linked.dpi[i]);
        mmi.ptMaxTrackSize.y = GetSystemMetricsForDpi(SM_CYMAXTRACK, linked.dpi[i]);
        if (!SendMessageTimeout(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi,
                                SMTO_ABORTIFHUNG, 32, &response)) return;
        lo = mmi.ptMinTrackSize;
        hi = mmi.ptMaxTrackSize;
        LONG bx = linked.border[i].left + linked.border[i].right;
        LONG by = linked.border[i].top + linked.border[i].bottom;
        if (lo.x < 1 || lo.y < 1 || hi.x < lo.x || hi.y < lo.y
            || bx < 0 || by < 0) return;
        windows[i].min_width = (int32_t)larger(1, (int64_t)lo.x - bx);
        windows[i].min_height = (int32_t)larger(1, (int64_t)lo.y - by);
        windows[i].max_width = hi.x - bx;
        windows[i].max_height = hi.y - by;
    }
    lg_drag_begin(&linked.drag, windows, 4, q, (LGQuadrant)q, 1, conf.FullWin);
    linked.mouse = mouse;
    linked.x = found.x;
    linked.y = found.y;
}

static int LinkedGridValid(void)
{
    unsigned i;
    for (i = 0; i < 4; ++i) {
        DWORD pid;
        RECT rc, bd;
        HWND hwnd = linked.hwnd[i];
        DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
        FixDWMRectLL(hwnd, &bd, 0);
        if (!tid || tid != linked.tid[i] || pid != linked.pid[i]
            || IsIconic(hwnd) || IsZoomed(hwnd) || !IsVisible(hwnd)
            || GetDpiForWindow(hwnd) != linked.dpi[i]
            || !EqualRect(&bd, &linked.border[i])
            || !GetWindowRect(hwnd, &rc) || !EqualRect(&rc, &linked.actual[i])) return 0;
    }
    return 1;
}

static int ApplyLinkedRects(const RECT *rects)
{
    HDWP batch = BeginDeferWindowPos(4);
    unsigned i;
    for (i = 0; batch && i < 4; ++i)
        batch = DeferWindowPos(batch, linked.hwnd[i], NULL, rects[i].left, rects[i].top,
                    rects[i].right - rects[i].left, rects[i].bottom - rects[i].top,
                    SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
    return batch && EndDeferWindowPos(batch);
}

static int CommitLinkedGrid(void)
{
    RECT next[4];
    unsigned i;
    if (!LinkedGridValid()) { lg_drag_stop(&linked.drag); return 0; }
    for (i = 0; i < 4; ++i) {
        LGRect r = linked.drag.pending.rects[i];
        SetRect(&next[i], r.left, r.top, r.right, r.bottom);
        InflateRectBorder(&next[i], &linked.border[i]);
    }
    int ok = ApplyLinkedRects(next);
    /* Applications may reject sizes in WM_WINDOWPOSCHANGING. Verify all four;
       best-effort rollback to the last verified batch, then freeze the drag. */
    for (i = 0; i < 4; ++i) {
        RECT actual;
        if (!GetWindowRect(linked.hwnd[i], &actual) || !EqualRect(&actual, &next[i])) ok = 0;
    }
    if (!ok) {
        ApplyLinkedRects(linked.actual);
        lg_drag_stop(&linked.drag);
        return 0;
    }
    for (i = 0; i < 4; ++i) {
        linked.actual[i] = next[i];
        ClearRestoreFlagOnResizeIfNeeded(linked.hwnd[i]);
    }
    linked.drag.dirty = 0;
    return 1;
}
