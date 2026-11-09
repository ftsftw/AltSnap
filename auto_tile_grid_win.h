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
    UINT dpi;
    unsigned count;
    int failed;
} ATGSnapshot;

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
    if (!GetWindowThreadProcessId(hwnd, &pid) || pid == GetCurrentProcessId()
        || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()
        || (style & WS_CHILD) || !(style & WS_THICKFRAME)
        || (exstyle & (WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))
        || GetWindow(hwnd, GW_OWNER) || !IsWindowEnabled(hwnd)
        || !IsSnapCandidate(hwnd) || IsZoomed(hwnd) || IsFullscreen(hwnd)
        || GetProp(hwnd, APP_MOVEONOFF) || GetProp(hwnd, APP_ROLLED)
        || MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != s->monitor
        || isClassName(hwnd, TEXT("Shell_TrayWnd"))
        || isClassName(hwnd, TEXT("Shell_SecondaryTrayWnd"))
        || isClassName(hwnd, TEXT("Progman")) || isClassName(hwnd, TEXT("WorkerW"))
        || blacklisted(hwnd, &BlkLst.Processes) || blacklisted(hwnd, &BlkLst.Windows)
        || blacklisted(hwnd, &BlkLst.Bottommost)) return 0;
    /* Also require a readable process identity, including for process blacklists.
       Never elevate or attempt to force inaccessible/protected windows. */
    if (!GetWindowProgName(hwnd, exe, ARR_SZ(exe))
        || !lstrcmpi(exe, TEXT("AltSnap.exe"))) return 0;
    return 1;
}

static BOOL CALLBACK EnumAutoTileGrid(HWND hwnd, LPARAM lp)
{
    ATGSnapshot *s = (ATGSnapshot *)lp;
    RECT visible, native, border;
    POINT lo, hi;
    int64_t bx, by;
    unsigned i = s->count;
    if (!AutoTileEligible(hwnd, s)) return TRUE;
    /* Missing DWM data or spanning monitors is outside this MVP. */
    if (S_OK != DwmGetWindowAttributeL(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                      &visible, sizeof visible)
        || !RectInRect(&s->mi.rcMonitor, &visible)) return TRUE;
    if (i == ARR_SZ(s->windows)) { s->failed = 1; return FALSE; }
    /* An otherwise eligible but unsafe participant rejects the entire plan. */
    if (!AutoTilePerMonitorAware(hwnd) || GetDpiForWindow(hwnd) != s->dpi
        || !GetWindowRect(hwnd, &native)
        || !GetMinMaxInfoF(hwnd, &lo, &hi, 0)) { s->failed = 1; return FALSE; }
    border = visible;
    SubRect(&border, &native);
    bx = (int64_t)border.left + border.right;
    by = (int64_t)border.top + border.bottom;
    if (border.left < 0 || border.top < 0 || border.right < 0 || border.bottom < 0
        || bx > INT32_MAX || by > INT32_MAX
        || lo.x < 1 || lo.y < 1 || hi.x < lo.x || hi.y < lo.y
        || hi.x <= bx || hi.y <= by) { s->failed = 1; return FALSE; }
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

static void AutoTileGrid(POINT cursor)
{
    static int busy;
    ATGSnapshot before, check;
    ATGPlan plan;
    RECT target[4], observed[4];
    unsigned i, changed = 0, observed_mask = 0;
    HDWP batch;
    int ok;
    LONG generation = MovementGeneration();
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONULL);
    if (busy || !monitor || !AutoTileIdle()) return;
    busy = 1;
    if (!AutoTileCapture(monitor, &before)
        || !atg_plan(before.windows, before.count, AutoTileRect(before.mi.rcWork), &plan)
        || !AutoTileCapture(monitor, &check) || !AutoTileUnchanged(&before, &check)
        || !AutoTileIdle() || generation != MovementGeneration()) goto done;
    /* A later timed min/max query may reenter while an earlier participant
       changes. Recheck every baseline after all message-sending preflight. */
    for (i = 0; i < before.count; ++i) {
        RECT native, visible;
        if (!AutoTileIdentity(&before, i)
            || !GetWindowRect(before.hwnd[i], &native)
            || !EqualRect(&native, &before.native[i])
            || S_OK != DwmGetWindowAttributeL(before.hwnd[i], DWMWA_EXTENDED_FRAME_BOUNDS,
                                              &visible, sizeof visible)
            || !AutoTileSameRect(AutoTileRect(visible), before.windows[i].window.rect)) goto done;
    }
    if (!AutoTileIdle() || generation != MovementGeneration()) goto done;
    for (i = 0; i < before.count; ++i) {
        unsigned idx = (unsigned)plan.indices[i];
        LGRect r = plan.native[i];
        SetRect(&target[idx], r.left, r.top, r.right, r.bottom);
        changed += !EqualRect(&target[idx], &before.native[idx]);
    }
    if (!changed) goto done;
    batch = BeginDeferWindowPos(changed);
    for (i = 0; batch && i < before.count; ++i) {
        RECT r = target[i];
        if (!EqualRect(&r, &before.native[i]))
            batch = DeferWindowPos(batch, before.hwnd[i], NULL, r.left, r.top,
                        r.right - r.left, r.bottom - r.top,
                        SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
    }
    ok = batch && EndDeferWindowPos(batch);
    for (i = 0; i < before.count; ++i) {
        RECT visible;
        unsigned idx = (unsigned)plan.indices[i];
        if (!AutoTileIdentity(&before, idx)
            || !GetWindowRect(before.hwnd[idx], &observed[idx])) { ok = 0; continue; }
        observed_mask |= 1u << idx;
        if (!EqualRect(&observed[idx], &target[idx])
            || S_OK != DwmGetWindowAttributeL(before.hwnd[idx], DWMWA_EXTENDED_FRAME_BOUNDS,
                                              &visible, sizeof visible)
            || !AutoTileSameRect(AutoTileRect(visible), plan.visible[i])) ok = 0;
    }
    if (generation != MovementGeneration() || !AutoTileIdle()) goto done;
    if (ok) {
        for (i = 0; i < before.count; ++i)
            ClearRestoreFlagOnResizeIfNeeded(before.hwnd[i]);
    } else {
        /* Win32 positioning is not transactional. Best-effort restore only
           participants whose identity and post-batch geometry still match.
           Never fight a new drag, external change, or reused HWND. */
        for (i = 0; i < before.count; ++i) {
            RECT current, r = before.native[i];
            if (generation != MovementGeneration() || !AutoTileIdle()) break;
            if ((observed_mask & (1u << i)) && AutoTileIdentity(&before, i)
                && GetWindowRect(before.hwnd[i], &current)
                && EqualRect(&current, &observed[i]) && !EqualRect(&current, &r)
                && generation == MovementGeneration() && AutoTileIdle())
                SetWindowPos(before.hwnd[i], NULL, r.left, r.top,
                    r.right - r.left, r.bottom - r.top,
                    SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
        }
    }
done:
    busy = 0;
}
