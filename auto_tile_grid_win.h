/* SPDX-License-Identifier: GPL-3.0-or-later
   Private hooks.c adapter. Deliberately independent of drag and mouse queues. */
#include "auto_tile_grid.c"

typedef struct {
    HMONITOR monitor;
    MONITORINFO mi;
    ATGWindow windows[4];
    HWND hwnd[4];
    DWORD pid[4], tid[4];
    RECT native[4];
    WINDOWPLACEMENT placement[4];
    UINT dpi;
    unsigned count;
    int failed;
} ATGSnapshot;

/* Debugger-only, opt-in; fixed reason codes and counts, no window metadata. */
static int auto_tile_trace;
static void AutoTileTrace(const char *reason, unsigned count)
{
    char line[128];
    if (!auto_tile_trace) return;
    wsprintfA(line, "AltSnap.ATG reason=%s count=%u\n", reason, count);
    OutputDebugStringA(line);
}

static int AutoTileSkip(const char *reason)
{
    AutoTileTrace(reason, 1);
    return 0;
}

static LGRect AutoTileRect(RECT r)
{
    return (LGRect){ r.left, r.top, r.right, r.bottom };
}

static int AutoTileSameRect(LGRect a, LGRect b)
{
    return a.left == b.left && a.top == b.top
        && a.right == b.right && a.bottom == b.bottom;
}

/* WM_GETMINMAXINFO uses the participant's coordinate space. Refuse legacy
   DPI virtualization rather than mix logical limits with physical DWM bounds. */
static int AutoTilePerMonitorAware(HWND hwnd)
{
    typedef HANDLE (WINAPI *context_t)(HWND);
    typedef int (WINAPI *awareness_t)(HANDLE);
    static context_t context;
    static awareness_t awareness;
    if (!context) context = (context_t)LoadDLLProc("USER32.DLL", "GetWindowDpiAwarenessContext");
    if (!awareness) awareness = (awareness_t)LoadDLLProc("USER32.DLL", "GetAwarenessFromDpiAwarenessContext");
    return context && awareness && awareness(context(hwnd)) == 2;
}

static int AutoTileEligible(HWND hwnd, const ATGSnapshot *s)
{
    DWORD pid;
    TCHAR exe[MAX_PATH];
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != s->monitor)
        return AutoTileSkip("skip-monitor");
    if (!IsSnapCandidate(hwnd) || IsFullscreen(hwnd)) return AutoTileSkip("skip-hidden-or-fullscreen");
    if (!GetWindowThreadProcessId(hwnd, &pid) || pid == GetCurrentProcessId()
        || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()
        || (style & WS_CHILD) || !(style & WS_THICKFRAME)
        || (IsZoomed(hwnd) && (style & WS_CAPTION) != WS_CAPTION)
        || (exstyle & (WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))
        || GetWindow(hwnd, GW_OWNER) || !IsWindowEnabled(hwnd)
        || isClassName(hwnd, TEXT("Shell_TrayWnd"))
        || isClassName(hwnd, TEXT("Shell_SecondaryTrayWnd"))
        || isClassName(hwnd, TEXT("Progman")) || isClassName(hwnd, TEXT("WorkerW")))
        return AutoTileSkip("skip-nonordinary");
    if (GetProp(hwnd, APP_MOVEONOFF) || GetProp(hwnd, APP_ROLLED)
        || blacklisted(hwnd, &BlkLst.Processes) || blacklisted(hwnd, &BlkLst.Windows)
        || blacklisted(hwnd, &BlkLst.Bottommost)) return AutoTileSkip("skip-disabled-or-blacklisted");
    /* Also require a readable process identity, including for process blacklists.
       Never elevate or attempt to force inaccessible/protected windows. */
    if (!GetWindowProgName(hwnd, exe, ARR_SZ(exe))
        || !lstrcmpi(exe, TEXT("AltSnap.exe"))) return AutoTileSkip("skip-process");
    return 1;
}

static BOOL CALLBACK EnumAutoTileGrid(HWND hwnd, LPARAM lp)
{
    ATGSnapshot *s = (ATGSnapshot *)lp;
    RECT visible, native, border;
    POINT lo, hi;
    int64_t bx, by, bl, bt, br, bb;
    unsigned i = s->count;
    if (!AutoTileEligible(hwnd, s)) return TRUE;
    /* MonitorFromWindow assigns the largest intersection (no nearest fallback).
       Partial overhang is allowed; all geometry remains signed screen space. */
    if (S_OK != DwmGetWindowAttributeL(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &visible, sizeof visible)) { AutoTileTrace("skip-dwm", 1); return TRUE; }
    /* Keep the old exclusion for borderless/custom fullscreen-like overhangs.
       Only ordinary captioned windows gain the relaxed containment policy. */
    if ((visible.left < s->mi.rcMonitor.left || visible.top < s->mi.rcMonitor.top
            || visible.right > s->mi.rcMonitor.right || visible.bottom > s->mi.rcMonitor.bottom)
        && (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CAPTION) != WS_CAPTION) {
        AutoTileTrace("skip-borderless-overhang", 1); return TRUE;
    }
    if (i == ARR_SZ(s->windows)) { AutoTileTrace("too-many", i + 1); s->failed = 1; return FALSE; }
    mem00(&s->placement[i], sizeof s->placement[i]);
    s->placement[i].length = sizeof s->placement[i];
    if (!GetWindowPlacement(hwnd, &s->placement[i])
        || (s->placement[i].showCmd != SW_SHOWNORMAL
            && s->placement[i].showCmd != SW_SHOWMAXIMIZED)
        || !!IsZoomed(hwnd) != (s->placement[i].showCmd == SW_SHOWMAXIMIZED)) {
        AutoTileTrace("placement", i); s->failed = 1; return FALSE;
    }
    /* An otherwise eligible but unsafe participant rejects the entire plan. */
    if (!AutoTilePerMonitorAware(hwnd) || GetDpiForWindow(hwnd) != s->dpi
        || !GetWindowRect(hwnd, &native)
        || !GetMinMaxInfoF(hwnd, &lo, &hi, 0)) { AutoTileTrace("dpi-rect-limits", i); s->failed = 1; return FALSE; }
    bl = (int64_t)visible.left - native.left;
    bt = (int64_t)visible.top - native.top;
    br = (int64_t)native.right - visible.right;
    bb = (int64_t)native.bottom - visible.bottom;
    if (bl < 0 || bt < 0 || br < 0 || bb < 0
        || bl > INT32_MAX || bt > INT32_MAX || br > INT32_MAX || bb > INT32_MAX
        || (int64_t)native.right - native.left > INT32_MAX
        || (int64_t)native.bottom - native.top > INT32_MAX
        || visible.right <= visible.left || visible.bottom <= visible.top) {
        AutoTileTrace("frame-overflow", i); s->failed = 1; return FALSE;
    }
    SetRect(&border, (LONG)bl, (LONG)bt, (LONG)br, (LONG)bb);
    bx = (int64_t)border.left + border.right;
    by = (int64_t)border.top + border.bottom;
    if (border.left < 0 || border.top < 0 || border.right < 0 || border.bottom < 0
        || bx > INT32_MAX || by > INT32_MAX
        || lo.x < 1 || lo.y < 1 || hi.x < lo.x || hi.y < lo.y
        || hi.x <= bx || hi.y <= by) { AutoTileTrace("frame-limits", i); s->failed = 1; return FALSE; }
    s->hwnd[i] = hwnd;
    s->tid[i] = GetWindowThreadProcessId(hwnd, &s->pid[i]);
    s->native[i] = native;
    s->windows[i].key = (uintptr_t)hwnd;
    s->windows[i].border = AutoTileRect(border);
    s->windows[i].window = (LGWindow){ AutoTileRect(visible),
        (int32_t)larger(1, (int64_t)lo.x - bx),
        (int32_t)larger(1, (int64_t)lo.y - by),
        (int32_t)(hi.x - bx), (int32_t)(hi.y - by) };
    ++s->count;
    return TRUE;
}

static int AutoTileCapture(HMONITOR monitor, ATGSnapshot *s)
{
    UINT dx, dy;
    mem00(s, sizeof *s);
    s->monitor = monitor;
    s->mi.cbSize = sizeof s->mi;
    if (!GetMonitorInfo(monitor, &s->mi)
        || S_OK != GetDpiForMonitorL(monitor, MDT_DEFAULT, &dx, &dy)
        || !dx || dx != dy) return 0;
    s->dpi = dx;
    return EnumDesktopWindows(NULL, EnumAutoTileGrid, (LPARAM)s)
        && !s->failed && s->count;
}

static int AutoTileIdentity(const ATGSnapshot *s, unsigned i)
{
    DWORD pid;
    return GetWindowThreadProcessId(s->hwnd[i], &pid) == s->tid[i]
        && pid == s->pid[i] && AutoTileEligible(s->hwnd[i], s)
        && AutoTilePerMonitorAware(s->hwnd[i])
        && GetDpiForWindow(s->hwnd[i]) == s->dpi;
}

static int AutoTileSamePlacement(const WINDOWPLACEMENT *a, const WINDOWPLACEMENT *b)
{
    return a->flags == b->flags && a->showCmd == b->showCmd
        && a->ptMinPosition.x == b->ptMinPosition.x && a->ptMinPosition.y == b->ptMinPosition.y
        && a->ptMaxPosition.x == b->ptMaxPosition.x && a->ptMaxPosition.y == b->ptMaxPosition.y
        && EqualRect(&a->rcNormalPosition, &b->rcNormalPosition);
}

static int AutoTileUnchanged(const ATGSnapshot *a, const ATGSnapshot *b)
{
    unsigned i, j;
    if (a->count != b->count || a->dpi != b->dpi
        || !EqualRect(&a->mi.rcWork, &b->mi.rcWork)
        || !EqualRect(&a->mi.rcMonitor, &b->mi.rcMonitor)) return 0;
    for (i = 0; i < a->count; ++i) {
        const LGWindow *aw = &a->windows[i].window;
        for (j = 0; j < b->count && a->hwnd[i] != b->hwnd[j]; ++j) { }
        if (j == b->count || a->pid[i] != b->pid[j] || a->tid[i] != b->tid[j]
            || !EqualRect(&a->native[i], &b->native[j])
            || !AutoTileSamePlacement(&a->placement[i], &b->placement[j])
            || !AutoTileSameRect(aw->rect, b->windows[j].window.rect)
            || !AutoTileSameRect(a->windows[i].border, b->windows[j].border)
            || aw->min_width != b->windows[j].window.min_width
            || aw->min_height != b->windows[j].window.min_height
            || aw->max_width != b->windows[j].window.max_width
            || aw->max_height != b->windows[j].window.max_height) return 0;
    }
    return 1;
}

static int AutoTileIdle(void)
{
    return !state.action.ac && !g_InMouseMove && !g_InFinishMovement
        && !LastWin.hwnd && !linked_queue.busy && !linked_move.pending;
}

/* No message-sending size queries here: this is also the rollback witness.
   Include placement so a late maximize/minimize/restore-rectangle change wins. */
static int AutoTileObserve(ATGSnapshot *s, unsigned i)
{
    RECT visible;
    WINDOWPLACEMENT p;
    mem00(&p, sizeof p); p.length = sizeof p;
    if (!AutoTileIdentity(s, i) || !GetWindowPlacement(s->hwnd[i], &p)
        || !GetWindowRect(s->hwnd[i], &s->native[i])
        || S_OK != DwmGetWindowAttributeL(s->hwnd[i], DWMWA_EXTENDED_FRAME_BOUNDS,
                                          &visible, sizeof visible)) return 0;
    s->placement[i] = p;
    s->windows[i].window.rect = AutoTileRect(visible);
    return 1;
}

static int AutoTileWitness(const ATGSnapshot *s, unsigned i)
{
    /* Avoid another large snapshot on the hooks thread's stack. */
    RECT native, visible;
    WINDOWPLACEMENT p;
    mem00(&p, sizeof p); p.length = sizeof p;
    return AutoTileIdentity(s, i) && GetWindowPlacement(s->hwnd[i], &p)
        && AutoTileSamePlacement(&p, &s->placement[i])
        && !!IsZoomed(s->hwnd[i]) == (p.showCmd == SW_SHOWMAXIMIZED)
        && GetWindowRect(s->hwnd[i], &native) && EqualRect(&native, &s->native[i])
        && S_OK == DwmGetWindowAttributeL(s->hwnd[i], DWMWA_EXTENDED_FRAME_BOUNDS,
                                          &visible, sizeof visible)
        && AutoTileSameRect(AutoTileRect(visible), s->windows[i].window.rect);
}

static int AutoTileMonitorUnchanged(const ATGSnapshot *s)
{
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfo(s->monitor, &mi)
        || !EqualRect(&mi.rcMonitor, &s->mi.rcMonitor)
        || !EqualRect(&mi.rcWork, &s->mi.rcWork)) return 0;
    return 1;
}

static int AutoTileBaselines(const ATGSnapshot *s, LONG generation)
{
    unsigned i;
    if (!AutoTileMonitorUnchanged(s)) return 0;
    for (i = 0; i < s->count; ++i) if (!AutoTileWitness(s, i)) return 0;
    return AutoTileIdle() && generation == MovementGeneration();
}

/* WINDOWPLACEMENT uses workspace coordinates for these non-tool windows.
   Normalize on the selected monitor even if the saved restore rect was on a
   different monitor. Never feed screen coordinates directly to placement. */
static int AutoTileNormalPlacement(const ATGSnapshot *s, unsigned i, WINDOWPLACEMENT *p)
{
    int64_t dx = (int64_t)s->mi.rcWork.left - s->mi.rcMonitor.left;
    int64_t dy = (int64_t)s->mi.rcWork.top - s->mi.rcMonitor.top;
    int64_t l = (int64_t)s->mi.rcWork.left - dx;
    int64_t t = (int64_t)s->mi.rcWork.top - dy;
    int64_t r = (int64_t)s->mi.rcWork.right - dx;
    int64_t b = (int64_t)s->mi.rcWork.bottom - dy;
    if (l < INT32_MIN || t < INT32_MIN || r > INT32_MAX || b > INT32_MAX
        || r <= l || b <= t || r - l > INT32_MAX || b - t > INT32_MAX) return 0;
    *p = s->placement[i];
    p->flags = 0; /* Synchronous, no restore-to-maximize or stale async request. */
    p->showCmd = SW_SHOWNOACTIVATE;
    SetRect(&p->rcNormalPosition, (LONG)l, (LONG)t, (LONG)r, (LONG)b);
    return 1;
}

static void AutoTileRollback(const ATGSnapshot *before, const ATGSnapshot *observed,
                             unsigned mask, LONG generation)
{
    unsigned i;
    for (i = 0; i < before->count; ++i) {
        RECT r = before->native[i];
        if (!AutoTileIdle() || generation != MovementGeneration()
            || !AutoTileMonitorUnchanged(before)) break;
        if (!(mask & (1u << i)) || !AutoTileWitness(observed, i)
            || !AutoTileIdle() || generation != MovementGeneration()) continue;
        if (before->placement[i].showCmd == SW_SHOWMAXIMIZED) {
            /* There is no documented no-activate maximize command. Restore the
               saved placement with the standard API, never style/focus hacks.
               Win32/the application may activate or raise on this failure path. */
            if (!AutoTileSamePlacement(&before->placement[i], &observed->placement[i])
                || !EqualRect(&r, &observed->native[i])) {
                int restored = SetWindowPlacement(before->hwnd[i], &before->placement[i]);
                AutoTileTrace(restored ? "restore-max" : "restore-max-failed", i);
            }
        } else if (!EqualRect(&r, &observed->native[i])) {
            int restored = SetWindowPos(before->hwnd[i], NULL, r.left, r.top,
                r.right - r.left, r.bottom - r.top,
                SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
            AutoTileTrace(restored ? "restore-normal" : "restore-normal-failed", i);
        }
    }
}

static void AutoTileGrid(POINT cursor)
{
    static int busy;
    ATGSnapshot before, live, check;
    ATGPlan plan;
    RECT target[4];
    unsigned i, changed = 0, restore_mask = 0;
    HDWP batch;
    int ok;
    LONG generation = MovementGeneration();
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONULL);
    if (busy || !monitor || !AutoTileIdle()) { AutoTileTrace("busy-or-no-monitor", 0); return; }
    busy = 1;
    if (!AutoTileCapture(monitor, &before)) { AutoTileTrace("capture", 0); goto done; }
    if (!AutoTileCapture(monitor, &check) || !AutoTileUnchanged(&before, &check)
        || !AutoTileBaselines(&before, generation)) {
        AutoTileTrace("preflight-changed", before.count); goto done;
    }
    live = before;
    for (i = 0; i < before.count; ++i) {
        WINDOWPLACEMENT normal;
        if (before.placement[i].showCmd != SW_SHOWMAXIMIZED) continue;
        if (!AutoTileBaselines(&live, generation)
            || !AutoTileNormalPlacement(&before, i, &normal)) goto rollback;
        ok = SetWindowPlacement(before.hwnd[i], &normal);
        /* A failed API may still have changed state. Read before deciding what
           is safe to undo. No async calls or queued restore commands. */
        if (!AutoTileObserve(&live, i)) goto rollback;
        restore_mask |= 1u << i;
        if (!ok || live.placement[i].showCmd != SW_SHOWNORMAL
            || !AutoTileBaselines(&live, generation)) {
            AutoTileTrace("normalize-failed", i); goto rollback;
        }
        AutoTileTrace("normalized", i);
    }
    /* Re-read normal frame insets, DPI and limits, not maximized insets. Timed
       min/max callbacks can change any participant; validate the witnesses too. */
    if (restore_mask && (!AutoTileCapture(monitor, &check) || check.count != before.count
        || !AutoTileBaselines(&live, generation))) {
        AutoTileTrace("normalized-capture-changed", before.count); goto rollback;
    }
    for (i = 0; i < check.count; ++i) {
        unsigned j;
        for (j = 0; j < live.count && check.hwnd[i] != live.hwnd[j]; ++j) { }
        if (j == live.count || check.pid[i] != live.pid[j] || check.tid[i] != live.tid[j]
            || check.placement[i].showCmd != SW_SHOWNORMAL
            || !EqualRect(&check.native[i], &live.native[j])
            || !AutoTileSamePlacement(&check.placement[i], &live.placement[j])
            || !AutoTileSameRect(check.windows[i].window.rect, live.windows[j].window.rect))
            goto rollback;
    }
    /* Preserve original index order for rollback; recapture once more to test
       the normalized plan's frame and size limits for stability. */
    if (!atg_plan(check.windows, check.count, AutoTileRect(check.mi.rcWork), &plan)) {
        AutoTileTrace("no-feasible-plan", check.count); goto rollback;
    }
    if (restore_mask) {
        ATGSnapshot stable;
        if (!AutoTileCapture(monitor, &stable) || !AutoTileUnchanged(&check, &stable)
            || !AutoTileBaselines(&live, generation)) {
            AutoTileTrace("normalized-preflight-changed", before.count); goto rollback;
        }
    }
    for (i = 0; i < check.count; ++i) {
        unsigned j, idx = (unsigned)plan.indices[i];
        LGRect r = plan.native[i];
        for (j = 0; j < before.count && before.hwnd[j] != check.hwnd[idx]; ++j) { }
        SetRect(&target[j], r.left, r.top, r.right, r.bottom);
        changed += !EqualRect(&target[j], &live.native[j]);
    }
    if (!changed) {
        AutoTileTrace("unchanged", before.count);
        if (!restore_mask) goto done;
        goto success;
    }
    batch = BeginDeferWindowPos(changed);
    for (i = 0; batch && i < before.count; ++i) {
        RECT r = target[i];
        if (!EqualRect(&r, &live.native[i]))
            batch = DeferWindowPos(batch, before.hwnd[i], NULL, r.left, r.top,
                        r.right - r.left, r.bottom - r.top,
                        SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
    }
    if (!batch) { AutoTileTrace("batch-create-failed", before.count); goto rollback; }
    ok = EndDeferWindowPos(batch);
    for (i = 0; i < before.count; ++i) {
        if (!AutoTileObserve(&live, i)) { restore_mask &= ~(1u << i); ok = 0; continue; }
        /* An external show-state change is never an authorization to undo it. */
        if (live.placement[i].showCmd != SW_SHOWNORMAL) {
            restore_mask &= ~(1u << i); ok = 0; continue;
        }
        restore_mask |= 1u << i;
        if (!EqualRect(&live.native[i], &target[i])) ok = 0;
    }
    for (i = 0; i < check.count; ++i) {
        unsigned j, idx = (unsigned)plan.indices[i];
        for (j = 0; j < before.count && before.hwnd[j] != check.hwnd[idx]; ++j) { }
        if (!AutoTileSameRect(live.windows[j].window.rect, plan.visible[i])) ok = 0;
    }
    if (!AutoTileIdle() || generation != MovementGeneration()) {
        AutoTileTrace("cancelled", before.count); goto done;
    }
    if (!AutoTileBaselines(&live, generation)) ok = 0;
    if (!ok) { AutoTileTrace("batch-verification-failed", before.count); goto rollback; }
success:
    for (i = 0; i < before.count; ++i) ClearRestoreFlagOnResizeIfNeeded(before.hwnd[i]);
    AutoTileTrace("success", before.count);
    goto done;
rollback:
    AutoTileRollback(&before, &live, restore_mask, generation);
done:
    busy = 0;
}
