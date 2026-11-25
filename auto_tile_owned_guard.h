/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AUTO_TILE_OWNED_GUARD_H
#define AUTO_TILE_OWNED_GUARD_H
/* One invocation only; never a persisted/window-name/process allowlist. */
typedef struct {
    HWND hwnd, owner;
    DWORD pid, tid, owner_tid;
} ATGSelectedOwned;

/* Reject ambiguous extra surfaces, even if the normal eligibility chain would
   skip them. This deliberately includes visible tool/disabled/modeless windows
   of the selected process. No dialog-class or Alt-Tab inference is sufficient. */
static BOOL CALLBACK AutoTileSelectedOtherWindow(HWND hwnd, LPARAM lp)
{
    const ATGSelectedOwned *selected = (const ATGSelectedOwned *)lp;
    DWORD pid = 0;
    if (hwnd == selected->hwnd || hwnd == selected->owner || !IsWindowVisible(hwnd))
        return TRUE;
    return GetWindowThreadProcessId(hwnd, &pid) && pid && pid != selected->pid;
}

/* A restrictive guard for an explicitly chosen surface, NOT a main/dialog
   classifier. Revalidate both identities/link and foreground around the reads.
   Same-process/thread handle reuse and changes between calls remain possible. */
static int AutoTileOwnedTopologyValid(const ATGSelectedOwned *selected, int foreground_required)
{
    DWORD pid = 0, owner_pid = 0, tid, owner_tid, saved_error;
    LONG_PTR style, exstyle, owner_style, owner_style_after, owner_exstyle;
    TCHAR classname[64];
    int ok = 0, class_length;
    saved_error = GetLastError();
    if ((foreground_required && GetForegroundWindow() != selected->hwnd)
        || !IsWindow(selected->hwnd) || !IsWindow(selected->owner)
        || selected->hwnd == selected->owner || !selected->pid
        || !selected->tid || !selected->owner_tid) goto done;
    tid = GetWindowThreadProcessId(selected->hwnd, &pid);
    owner_tid = GetWindowThreadProcessId(selected->owner, &owner_pid);
    if (tid != selected->tid || pid != selected->pid
        || owner_tid != selected->owner_tid || owner_pid != selected->pid
        || GetWindow(selected->hwnd, GW_OWNER) != selected->owner) goto done;
    style = GetWindowLongPtr(selected->hwnd, GWL_STYLE);
    exstyle = GetWindowLongPtr(selected->hwnd, GWL_EXSTYLE);
    SetLastError(0);
    owner_style = GetWindowLongPtr(selected->owner, GWL_STYLE);
    if (!owner_style && GetLastError()) goto done;
    SetLastError(0);
    owner_exstyle = GetWindowLongPtr(selected->owner, GWL_EXSTYLE);
    if ((!owner_exstyle && GetLastError())
        || (style & (WS_CHILD|WS_POPUP))
        || (style & (WS_CAPTION|WS_THICKFRAME)) != (WS_CAPTION|WS_THICKFRAME)
        || !(exstyle & WS_EX_APPWINDOW)
        || (exstyle & (WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_DLGMODALFRAME))
        || (owner_style & WS_CHILD) || !(owner_exstyle & WS_EX_TOOLWINDOW)
        || (owner_exstyle & (WS_EX_APPWINDOW|WS_EX_NOACTIVATE|WS_EX_DLGMODALFRAME))
        || !IsWindowVisible(selected->hwnd) || !IsWindowVisible(selected->owner)
        || !IsWindowEnabled(selected->hwnd) || !IsWindowEnabled(selected->owner)) goto done;
    /* A standard dialog is always refused; a nonstandard class is not proof. */
    class_length = GetClassName(selected->hwnd, classname, ARR_SZ(classname));
    if (class_length <= 0 || class_length >= (int)ARR_SZ(classname)-1
        || !lstrcmpi(classname, TEXT("#32770"))
        || GetAncestor(selected->hwnd, GA_ROOT) != selected->hwnd
        || GetAncestor(selected->hwnd, GA_ROOTOWNER) != selected->hwnd
        || GetLastActivePopup(selected->hwnd) != selected->hwnd
        || !EnumWindows(AutoTileSelectedOtherWindow, (LPARAM)selected)) goto done;
    SetLastError(0);
    owner_style_after = GetWindowLongPtr(selected->owner, GWL_STYLE);
    if (!owner_style_after && GetLastError()) goto done;
    pid = owner_pid = 0;
    ok = (!foreground_required || GetForegroundWindow() == selected->hwnd)
        && IsWindow(selected->hwnd) && IsWindow(selected->owner)
        && GetWindowThreadProcessId(selected->hwnd, &pid) == selected->tid
        && pid == selected->pid
        && GetWindowThreadProcessId(selected->owner, &owner_pid) == selected->owner_tid
        && owner_pid == selected->pid
        && GetWindow(selected->hwnd, GW_OWNER) == selected->owner
        && IsWindowVisible(selected->hwnd) && IsWindowVisible(selected->owner)
        && IsWindowEnabled(selected->hwnd) && IsWindowEnabled(selected->owner)
        && GetWindowLongPtr(selected->hwnd, GWL_STYLE) == style
        && GetWindowLongPtr(selected->hwnd, GWL_EXSTYLE) == exstyle
        && owner_style_after == owner_style
        && GetWindowLongPtr(selected->owner, GWL_EXSTYLE) == owner_exstyle
        && GetAncestor(selected->hwnd, GA_ROOT) == selected->hwnd
        && GetAncestor(selected->hwnd, GA_ROOTOWNER) == selected->hwnd
        && GetLastActivePopup(selected->hwnd) == selected->hwnd;
done:
    SetLastError(saved_error);
    return ok;
}

static int AutoTileSelectedOwnedValid(const ATGSelectedOwned *selected)
{
    return AutoTileOwnedTopologyValid(selected, 1);
}

#endif
