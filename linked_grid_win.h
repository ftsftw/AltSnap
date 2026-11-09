/* SPDX-License-Identifier: GPL-3.0-or-later
   Private hooks.c adapter. All geometry is in physical screen coordinates,
   using DWM visible bounds with SnapGap=0. Never used for MDI. */
#include "linked_grid.c"
#include "linked_grid_recovery.h"
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

/* Opt-in debugger output only. No handles, process IDs, titles or content.
   All calls (including extra DWM reads) are confined to the enabled path. */
static int linked_trace;
static int linked_trace_ready;
static unsigned linked_trace_offers;
static void LinkedTrace(const char *event, int cell, int a, int b)
{
    char line[256];
    if (!linked_trace) return;
    wsprintfA(line, "AltSnap.LG t=%lu g=%08x%08x mode=%d event=%s cell=%d a=%d b=%d\n",
              (unsigned long)GetTickCount(), (unsigned)((uint64_t)linked_queue.generation >> 32),
              (unsigned)linked_queue.generation, (int)linked.drag.mode, event, cell, a, b);
    OutputDebugStringA(line);
}

static void LinkedTraceRect(const char *event, unsigned i, const RECT *wanted,
                            const RECT *actual, int readable)
{
    char line[320];
    if (!linked_trace) return;
    wsprintfA(line, "AltSnap.LG g=%08x%08x event=%s cell=%u readable=%d want=%d,%d,%d,%d got=%d,%d,%d,%d\n",
              (unsigned)((uint64_t)linked_queue.generation >> 32),
              (unsigned)linked_queue.generation, event, i, readable, (int)wanted->left, (int)wanted->top,
              (int)wanted->right, (int)wanted->bottom, (int)actual->left,
              (int)actual->top, (int)actual->right, (int)actual->bottom);
    OutputDebugStringA(line);
}

/* A diagnostic snapshot is not atomic. Keep the exact verification read in
   a separate record; these later DWM/native reads help identify compositor lag. */
static void LinkedTraceGeometry(const char *event, const RECT *target)
{
    unsigned i;
    if (!linked_trace || !linked_trace_ready) return;
    LinkedTrace(event, -1, linked.drag.fullwin, linked.drag.dirty);
    for (i = 0; i < 4; ++i) {
        RECT native = {0, 0, 0, 0}, dwm = {0, 0, 0, 0};
        int native_ok = GetWindowRect(linked.hwnd[i], &native);
        int dwm_ok = S_OK == DwmGetWindowAttributeL(linked.hwnd[i],
                                DWMWA_EXTENDED_FRAME_BOUNDS, &dwm, sizeof dwm);
        LinkedTraceRect("native", i, &target[i], &native, native_ok);
        LinkedTraceRect("dwm-vs-baseline", i, &linked.actual[i], &dwm, dwm_ok);
        LinkedTraceRect("cached-border", i, &linked.border[i], &linked.border[i], 1);
    }
}

static int LinkedInvalid(const char *reason, unsigned i)
{
    LinkedTrace(reason, (int)i, 0, 0);
    return 0;
}

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
    LGStatus detected = lg_resize(linked_candidates.windows, linked_candidates.count, active,
                  (LGQuadrant)q, (q & 1) ? a.left : a.right,
                  (q & 2) ? a.top : a.bottom, &found);
    LinkedTrace("detect", -1, (int)detected, 0);
    if (detected != LG_OK) return;
    /* From here a grid was captured. Any failure blocks, never falls back. */
    linked.drag.mode = LG_DRAG_BLOCKED;
    linked.drag.dirty = 0;
    linked_trace_ready = 0;
    LinkedTrace("captured", -1, (int)q, conf.FullWin);
    for (i = 0; i < 4; ++i) {
        HWND hwnd = linked_candidates.hwnd[found.indices[i]];
        POINT lo, hi;
        RECT visible;
        linked.hwnd[i] = hwnd;
        linked.tid[i] = GetWindowThreadProcessId(hwnd, &linked.pid[i]);
        linked.dpi[i] = GetDpiForWindow(hwnd);
        if (linked.dpi[i] != state.origin.dpi || !GetWindowRect(hwnd, &linked.actual[i])) {
            LinkedTrace("capture-native-dpi", (int)i, 0, 0); return;
        }
        /* Require DWM bounds, avoiding mixed virtualized/fallback rectangles. */
        if (S_OK != DwmGetWindowAttributeL(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                          &visible, sizeof visible)) {
            LinkedTrace("capture-dwm", (int)i, 0, 0); return;
        }
        FixDWMRectLL(hwnd, &linked.border[i], 0);
        windows[i] = linked_candidates.windows[found.indices[i]];
        if (visible.left != windows[i].rect.left || visible.top != windows[i].rect.top
            || visible.right != windows[i].rect.right || visible.bottom != windows[i].rect.bottom) {
            LinkedTrace("capture-changed", (int)i, 0, 0); return;
        }
        /* A timed-out query cannot establish safe limits. */
        MINMAXINFO mmi;
        DWORD_PTR response;
        mem00(&mmi, sizeof mmi);
        mmi.ptMinTrackSize.x = GetSystemMetricsForDpi(SM_CXMINTRACK, linked.dpi[i]);
        mmi.ptMinTrackSize.y = GetSystemMetricsForDpi(SM_CYMINTRACK, linked.dpi[i]);
        mmi.ptMaxTrackSize.x = GetSystemMetricsForDpi(SM_CXMAXTRACK, linked.dpi[i]);
        mmi.ptMaxTrackSize.y = GetSystemMetricsForDpi(SM_CYMAXTRACK, linked.dpi[i]);
        if (!SendMessageTimeout(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi,
                                SMTO_ABORTIFHUNG, 32, &response)) {
            if (linked_trace) LinkedTrace("capture-limits-query", (int)i, (int)GetLastError(), 0);
            return;
        }
        lo = mmi.ptMinTrackSize;
        hi = mmi.ptMaxTrackSize;
        LONG bx = linked.border[i].left + linked.border[i].right;
        LONG by = linked.border[i].top + linked.border[i].bottom;
        if (lo.x < 1 || lo.y < 1 || hi.x < lo.x || hi.y < lo.y
            || bx < 0 || by < 0) {
            LinkedTrace("capture-limits-invalid", (int)i, 0, 0); return;
        }
        windows[i].min_width = (int32_t)larger(1, (int64_t)lo.x - bx);
        windows[i].min_height = (int32_t)larger(1, (int64_t)lo.y - by);
        windows[i].max_width = hi.x - bx;
        windows[i].max_height = hi.y - by;
        LinkedTrace("limits-width", (int)i, windows[i].min_width, windows[i].max_width);
        LinkedTrace("limits-height", (int)i, windows[i].min_height, windows[i].max_height);
    }
    linked_trace_ready = 1; /* All four native rectangles/borders are initialized. */
    LGStatus status = lg_drag_begin(&linked.drag, windows, 4, q, (LGQuadrant)q, 1, conf.FullWin);
    LinkedTrace("begin", -1, (int)status, 0);
    LinkedTraceGeometry("baseline", linked.actual);
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
        if (!tid || tid != linked.tid[i] || pid != linked.pid[i])
            return LinkedInvalid("valid-identity", i);
        if (IsIconic(hwnd) || IsZoomed(hwnd) || !IsVisible(hwnd))
            return LinkedInvalid("valid-state", i);
        if (GetDpiForWindow(hwnd) != linked.dpi[i]) return LinkedInvalid("valid-dpi", i);
        if (!EqualRect(&bd, &linked.border[i])) {
            LinkedTraceRect("valid-border", i, &linked.border[i], &bd, 1);
            LinkedTraceGeometry("valid-border-stop", linked.actual);
            return 0;
        }
        int readable = GetWindowRect(hwnd, &rc);
        if (!readable || !EqualRect(&rc, &linked.actual[i])) {
            if (!readable) SetRectEmpty(&rc);
            LinkedTraceRect("valid-native", i, &linked.actual[i], &rc, readable);
            return 0;
        }
    }
    return 1;
}

static int ApplyLinkedRects(const RECT *rects, unsigned mask)
{
    HDWP batch;
    unsigned i, count = 0;
    if (!lg_queue_current(&linked_queue, linked_dispatch)) {
        LinkedTrace("apply-stale", -1, 0, 0); return 0;
    }
    for (i = 0; i < 4; ++i) count += !!(mask & (1u << i));
    if (!count) return 1;
    batch = BeginDeferWindowPos(count);
    for (i = 0; batch && i < 4; ++i) if (mask & (1u << i))
        batch = DeferWindowPos(batch, linked.hwnd[i], NULL, rects[i].left, rects[i].top,
                    rects[i].right - rects[i].left, rects[i].bottom - rects[i].top,
                    SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
    if (!batch) {
        if (linked_trace) LinkedTrace("defer-failed", -1, (int)mask, (int)GetLastError());
        return 0;
    }
    if (!lg_queue_current(&linked_queue, linked_dispatch)) {
        LinkedTrace("defer-stale", -1, 0, 0); return 0;
    }
    int ok = EndDeferWindowPos(batch);
    if (!ok && linked_trace) LinkedTrace("end-defer-error", -1, (int)GetLastError(), 0);
    LinkedTrace("end-defer", -1, ok, (int)mask);
    return ok;
}

static int LinkedParticipantIdentity(unsigned i)
{
    DWORD pid;
    HWND hwnd = linked.hwnd[i];
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    RECT bd;
    if (!tid || tid != linked.tid[i] || pid != linked.pid[i])
        return LinkedInvalid("recover-identity", i);
    if (IsIconic(hwnd) || IsZoomed(hwnd) || !IsVisible(hwnd))
        return LinkedInvalid("recover-state", i);
    if (GetDpiForWindow(hwnd) != linked.dpi[i]) return LinkedInvalid("recover-dpi", i);
    FixDWMRectLL(hwnd, &bd, 0);
    if (!EqualRect(&bd, &linked.border[i]))
        LinkedTraceRect("recover-border", i, &linked.border[i], &bd, 1);
    return EqualRect(&bd, &linked.border[i]);
}

/* The post-apply witness is an expected value, not proof of causation. Recheck
   it immediately before each restore; never overwrite an observed later move. */
static int LinkedRestoreEligible(unsigned i, const RECT *expected, RECT *current)
{
    return lg_queue_current(&linked_queue, linked_dispatch)
        && LinkedParticipantIdentity(i) && GetWindowRect(linked.hwnd[i], current)
        && (EqualRect(current, &linked.actual[i]) || EqualRect(current, &expected[i]));
}

static void RecoverLinkedGrid(const RECT *expected, unsigned witnessed)
{
    LGRect before[4], owned[4], observed[4];
    unsigned identity = 0, mask, i;
    for (i = 0; i < 4; ++i) {
        RECT current;
        before[i] = (LGRect){linked.actual[i].left, linked.actual[i].top,
                             linked.actual[i].right, linked.actual[i].bottom};
        owned[i] = (LGRect){expected[i].left, expected[i].top,
                          expected[i].right, expected[i].bottom};
        if (lg_queue_current(&linked_queue, linked_dispatch)
            && LinkedParticipantIdentity(i)
            && GetWindowRect(linked.hwnd[i], &current)) {
            identity |= 1u << i;
            observed[i] = (LGRect){current.left, current.top, current.right, current.bottom};
            LinkedTraceRect("recover-observed", i, &expected[i], &current, 1);
        }
    }
    if (!lg_queue_current(&linked_queue, linked_dispatch)) return;
    mask = lg_recovery_mask(before, owned, observed, identity & witnessed);
    LinkedTrace("recover-mask", -1, (int)mask, (int)identity);
    /* No recovery batch: an earlier participant's positioning callback can move
       a later one. Revalidate each HWND after the preceding restore returns. */
    for (i = 0; i < 4; ++i) if (mask & (1u << i)) {
        RECT current = {0, 0, 0, 0};
        if (!lg_queue_current(&linked_queue, linked_dispatch)) return;
        if (!LinkedRestoreEligible(i, expected, &current)) {
            LinkedTrace("recover-ineligible", (int)i, 0, 0); continue;
        }
        if (EqualRect(&current, &linked.actual[i])) continue;
        if (!lg_queue_current(&linked_queue, linked_dispatch)) return;
        int restored = SetWindowPos(linked.hwnd[i], NULL, linked.actual[i].left, linked.actual[i].top,
                     linked.actual[i].right - linked.actual[i].left,
                     linked.actual[i].bottom - linked.actual[i].top,
                     SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
        LinkedTrace("recover-individual", (int)i, restored, 0);
        if (!lg_queue_current(&linked_queue, linked_dispatch)) return;
        /* One attempt only, even if the API succeeds but the app corrects or
           refuses the baseline. Never adopt a new witness from a failed restore. */
        int readable = GetWindowRect(linked.hwnd[i], &current);
        if (!readable || !EqualRect(&current, &linked.actual[i]))
            LinkedTraceRect("recover-mismatch", i, &linked.actual[i], &current, readable);
    }
    LinkedTraceGeometry("recover-result", linked.actual);
}

static int CommitLinkedGrid(void)
{
    RECT next[4], witnessed[4];
    unsigned i, witness_mask = 0;
    if (!lg_queue_current(&linked_queue, linked_dispatch)
        || linked.drag.mode != LG_DRAG_ACTIVE || !linked.drag.dirty) return 0;
    if (!LinkedGridValid()) { lg_drag_stop(&linked.drag); return 0; }
    for (i = 0; i < 4; ++i) {
        LGRect r = linked.drag.pending.rects[i];
        SetRect(&next[i], r.left, r.top, r.right, r.bottom);
        InflateRectBorder(&next[i], &linked.border[i]);
    }
    int applied = ApplyLinkedRects(next, 15);
    int ok = applied;
    if (!lg_queue_current(&linked_queue, linked_dispatch)) return 0;
    /* Record the native post-apply observations before any diagnostic reads.
       A successful API call may still have allowed WM_WINDOWPOSCHANGING size
       corrections. Accept a third RECT only for a successful batch with the
       requested origin intact: this is a size-correction witness, not arbitrary
       repositioning. On API failure retain only exact requested observations. */
    for (i = 0; i < 4; ++i) {
        RECT actual = {0, 0, 0, 0};
        int readable = GetWindowRect(linked.hwnd[i], &actual);
        witnessed[i] = actual;
        if (readable && (EqualRect(&actual, &next[i])
            || (applied && actual.left == next[i].left && actual.top == next[i].top
                && actual.right > actual.left && actual.bottom > actual.top)))
            witness_mask |= 1u << i;
        if (!readable || !EqualRect(&actual, &next[i])) {
            ok = 0;
            LinkedTraceRect("verify-mismatch", i, &next[i], &actual, readable);
        }
    }
    if (!lg_queue_current(&linked_queue, linked_dispatch)) return 0;
    if (!ok) {
        LinkedTraceGeometry("apply-failed", next);
        LinkedTrace("recover-witness", -1, (int)witness_mask, applied);
        RecoverLinkedGrid(witnessed, witness_mask);
        if (!lg_queue_current(&linked_queue, linked_dispatch)) return 0;
        lg_drag_stop(&linked.drag);
        LinkedTrace("blocked", -1, 0, 0);
        return 0;
    }
    for (i = 0; i < 4; ++i) {
        linked.actual[i] = next[i];
        ClearRestoreFlagOnResizeIfNeeded(linked.hwnd[i]);
    }
    linked.drag.dirty = 0;
    LinkedTrace("verified", -1, linked.drag.pending.x, linked.drag.pending.y);
    return 1;
}
