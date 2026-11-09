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

/* Relations only: never format handles or discover names. No chain walk,
   messages, process opens or window mutations. A stable sample is still not an
   atomic snapshot (including same-process/thread HWND reuse). */
static const char *AutoTileOwnerRelation(HWND related, HWND hwnd, HWND owner, HWND rootowner)
{
    if (!related) return "unknown";
    if (related == hwnd) return "self";
    if (related == owner) return "owner";
    if (related == rootowner) return "rootowner";
    return "other";
}

static void AutoTileOwnerTrace(HWND hwnd, HWND owner, DWORD pid,
                               LONG_PTR style, LONG_PTR exstyle)
{
    char line[256];
    DWORD owner_pid = 0, owner_tid, saved_error;
    LONG_PTR owner_exstyle;
    HWND root, rootowner, popup, current;
    int visible, enabled, window_enabled, style_known, stable = 0;
    const char *link = "stable", *process;
    if (!auto_tile_trace) return;
    saved_error = GetLastError();
    if (!IsWindow(hwnd)) { link = "window-gone"; goto state; }
    if (!IsWindow(owner)) { link = "owner-gone"; goto state; }
    owner_tid = GetWindowThreadProcessId(owner, &owner_pid);
    process = !pid || !owner_tid || !owner_pid ? "unknown"
        : pid == owner_pid ? "same" : "different";
    visible = IsWindowVisible(owner);
    enabled = IsWindowEnabled(owner);
    window_enabled = IsWindowEnabled(hwnd);
    SetLastError(0);
    owner_exstyle = GetWindowLongPtr(owner, GWL_EXSTYLE);
    style_known = owner_exstyle != 0 || GetLastError() == 0;
    root = GetAncestor(hwnd, GA_ROOT);
    rootowner = GetAncestor(hwnd, GA_ROOTOWNER);
    popup = rootowner ? GetLastActivePopup(rootowner) : (HWND)0;
    current = GetWindow(hwnd, GW_OWNER);
    if (!IsWindow(hwnd)) link = "window-gone";
    else if (!IsWindow(owner)) link = "owner-gone";
    else if (!current) link = "cleared";
    else if (current != owner) link = "changed";
    else stable = 1;
state:
    wsprintfA(line, "AltSnap.ATG reason=owner-state link=%s\n", link);
    OutputDebugStringA(line);
    /* Discard observations when a race was detected. Refusal is unconditional. */
    if (stable) {
        wsprintfA(line, "AltSnap.ATG reason=owner-flags process=%s visible=%s enabled=%s tool=%s app=%s noactivate=%s\n",
            process, visible ? "yes" : "no", enabled ? "yes" : "no",
            !style_known ? "unknown" : owner_exstyle & WS_EX_TOOLWINDOW ? "yes" : "no",
            !style_known ? "unknown" : owner_exstyle & WS_EX_APPWINDOW ? "yes" : "no",
            !style_known ? "unknown" : owner_exstyle & WS_EX_NOACTIVATE ? "yes" : "no");
        OutputDebugStringA(line);
        wsprintfA(line, "AltSnap.ATG reason=owned-flags popup=%s app=%s enabled=%s\n",
            style & WS_POPUP ? "yes" : "no", exstyle & WS_EX_APPWINDOW ? "yes" : "no",
            window_enabled ? "yes" : "no");
        OutputDebugStringA(line);
        wsprintfA(line, "AltSnap.ATG reason=owner-relations root=%s rootowner=%s lastpopup=%s\n",
            AutoTileOwnerRelation(root, hwnd, owner, rootowner),
            AutoTileOwnerRelation(rootowner, hwnd, owner, rootowner),
            AutoTileOwnerRelation(popup, hwnd, owner, rootowner));
        OutputDebugStringA(line);
    }
    SetLastError(saved_error);
}

static void AutoTileProcessTrace(const char *stage, DWORD error)
{
    char line[128];
    if (!auto_tile_trace) return;
    wsprintfA(line, "AltSnap.ATG reason=%s error=%lu\n", stage, (unsigned long)error);
    OutputDebugStringA(line);
}

/* Routine desktop enumeration exclusions are summarized once per capture.
   No counters or diagnostic helper queries run with tracing disabled. */
static const char *const auto_tile_quiet_reasons[] = {
    "skip-monitor", "skip-invisible", "skip-cloaked", "skip-zero-size", "skip-minimized"
};
static int AutoTileQuietSkip(unsigned *quiet, unsigned reason)
{
    if (auto_tile_trace) {
        if (quiet) ++quiet[reason];
        else AutoTileTrace(auto_tile_quiet_reasons[reason], 1);
    }
    return 0;
}

/* Diagnose the first failed IsSnapCandidate predicate only after it refuses.
   Keep the shared snap helper and its short-circuit semantics unchanged. These
   additional observations can race; they never decide eligibility. */
static int AutoTileSnapSkip(HWND hwnd, unsigned *quiet)
{
    if (!auto_tile_trace) return 0;
    if (!IsWindowVisible(hwnd)) return AutoTileQuietSkip(quiet, 1);
    if (IsWindowCloaked(hwnd)) return AutoTileQuietSkip(quiet, 2);
    if (HasWindowSizeZero(hwnd)) return AutoTileQuietSkip(quiet, 3);
    if (IsIconic(hwnd)) return AutoTileQuietSkip(quiet, 4);
    if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE)
        return AutoTileSkip("skip-noactivate");
    if ((GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CAPTION) != WS_CAPTION
        && !(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_THICKFRAME)
        && !GetBorderlessFlag(hwnd) && !blacklisted(hwnd, &BlkLst.Snaplist))
        return AutoTileSkip("skip-snap-shape");
    return AutoTileSkip("skip-snap-state-changed");
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

/* Use the very same validated process identity for every list. The general
   blacklisted() helper can use a drag cache or do another process lookup; a
   failed/stale lookup there must not let an AutoTile participant bypass a list.
   Keep the existing wildcard/whitelist matcher and window/class semantics. */
static int AutoTileBlacklisted(HWND hwnd, const blacklist_t *list, const TCHAR *exe)
{
    TCHAR title[256], classname[256];
    if (!list->length || !list->items) return 0;
    title[0] = classname[0] = TEXT('\0');
    GetWindowText(hwnd, title, ARR_SZ(title));
    GetClassName(hwnd, classname, ARR_SZ(classname));
    return blacklisted_from_names(list, exe, title, classname);
}

static int AutoTileEligible(HWND hwnd, const ATGSnapshot *s, unsigned *quiet)
{
    DWORD pid;
    HWND owner;
    TCHAR exe[MAX_PATH];
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != s->monitor)
        return AutoTileQuietSkip(quiet, 0);
    if (!IsSnapCandidate(hwnd)) return AutoTileSnapSkip(hwnd, quiet);
    if (IsFullscreen(hwnd)) return AutoTileSkip("skip-fullscreen");
    if (!GetWindowThreadProcessId(hwnd, &pid)) return AutoTileSkip("skip-thread-query");
    if (pid == GetCurrentProcessId()) return AutoTileSkip("skip-own-process");
    if (hwnd == GetDesktopWindow()) return AutoTileSkip("skip-desktop");
    if (hwnd == GetShellWindow()) return AutoTileSkip("skip-shell");
    if (style & WS_CHILD) return AutoTileSkip("skip-child");
    if (!(style & WS_THICKFRAME)) return AutoTileSkip("skip-no-thickframe");
    if (IsZoomed(hwnd) && (style & WS_CAPTION) != WS_CAPTION)
        return AutoTileSkip("skip-maximized-no-caption");
    if (exstyle & WS_EX_TOOLWINDOW) return AutoTileSkip("skip-toolwindow");
    if (exstyle & WS_EX_NOACTIVATE) return AutoTileSkip("skip-noactivate");
    if ((owner = GetWindow(hwnd, GW_OWNER))) {
        if (auto_tile_trace) AutoTileOwnerTrace(hwnd, owner, pid, style, exstyle);
        return AutoTileSkip("skip-owner");
    }
    if (!IsWindowEnabled(hwnd)) return AutoTileSkip("skip-not-enabled");
    if (isClassName(hwnd, TEXT("Shell_TrayWnd"))) return AutoTileSkip("skip-taskbar");
    if (isClassName(hwnd, TEXT("Shell_SecondaryTrayWnd"))) return AutoTileSkip("skip-secondary-taskbar");
    if (isClassName(hwnd, TEXT("Progman"))) return AutoTileSkip("skip-progman");
    if (isClassName(hwnd, TEXT("WorkerW"))) return AutoTileSkip("skip-worker");
    if (GetProp(hwnd, APP_MOVEONOFF)) return AutoTileSkip("skip-move-disabled");
    if (GetProp(hwnd, APP_ROLLED)) return AutoTileSkip("skip-rolled");
    /* Also require a readable process identity, including for process blacklists.
       Never elevate or attempt to force inaccessible/protected windows. */
    if (!GetWindowProgNameTraced(hwnd, exe, ARR_SZ(exe),
                               auto_tile_trace ? AutoTileProcessTrace : NULL))
        return AutoTileSkip("skip-process-unreadable");
    if (AutoTileBlacklisted(hwnd, &BlkLst.Processes, exe)) return AutoTileSkip("skip-blacklist-process");
    if (AutoTileBlacklisted(hwnd, &BlkLst.Windows, exe)) return AutoTileSkip("skip-blacklist-window");
    if (AutoTileBlacklisted(hwnd, &BlkLst.Bottommost, exe)) return AutoTileSkip("skip-blacklist-bottommost");
    if (!lstrcmpi(exe, TEXT("AltSnap.exe"))) return AutoTileSkip("skip-altsnap-exe");
    return 1;
}

typedef struct { ATGSnapshot *snapshot; unsigned quiet[5]; } ATGCapture;

static BOOL CALLBACK EnumAutoTileGrid(HWND hwnd, LPARAM lp)
{
    ATGCapture *capture = (ATGCapture *)lp;
    ATGSnapshot *s = capture->snapshot;
    RECT visible, native, border;
    POINT lo, hi;
    int64_t bx, by, bl, bt, br, bb;
    unsigned i = s->count;
    if (!AutoTileEligible(hwnd, s, capture->quiet)) return TRUE;
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
    int enumerated;
    unsigned i;
    ATGCapture capture;
    mem00(s, sizeof *s);
    s->monitor = monitor;
    s->mi.cbSize = sizeof s->mi;
    if (!GetMonitorInfo(monitor, &s->mi)
        || S_OK != GetDpiForMonitorL(monitor, MDT_DEFAULT, &dx, &dy)
        || !dx || dx != dy) return 0;
    s->dpi = dx;
    capture.snapshot = s;
    if (auto_tile_trace) {
        mem00(capture.quiet, sizeof capture.quiet);
        AutoTileTrace("scan-begin", 0);
    }
    enumerated = EnumDesktopWindows(NULL, EnumAutoTileGrid, (LPARAM)&capture);
    if (auto_tile_trace) {
        for (i = 0; i < ARR_SZ(capture.quiet); ++i)
            if (capture.quiet[i]) AutoTileTrace(auto_tile_quiet_reasons[i], capture.quiet[i]);
        AutoTileTrace("scan-candidates", s->count);
    }
    return enumerated && !s->failed && s->count;
}

static int AutoTileIdentity(const ATGSnapshot *s, unsigned i)
{
    DWORD pid;
    return GetWindowThreadProcessId(s->hwnd[i], &pid) == s->tid[i]
        && pid == s->pid[i] && AutoTileEligible(s->hwnd[i], s, NULL)
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
