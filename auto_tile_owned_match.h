/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AUTO_TILE_OWNED_MATCH_H
#define AUTO_TILE_OWNED_MATCH_H
#include "auto_tile_owned_guard.h"
#include "auto_tile_owned_store.h"

static int ATGOwnedClass(HWND hwnd, TCHAR out[ATG_OWNED_CLASS])
{
    int n=GetClassName(hwnd, out, ATG_OWNED_CLASS);
    return n>0 && n<ATG_OWNED_CLASS-1 && out[n]==0;
}

static int ATGOwnedPin(HWND hwnd, ATGSelectedOwned *pin)
{
    DWORD owner_pid=0;
    mem00(pin, sizeof *pin);
    pin->hwnd=hwnd;
    if (!hwnd || !IsWindow(hwnd)) return 0;
    pin->owner=GetWindow(hwnd, GW_OWNER);
    if (!pin->owner || !IsWindow(pin->owner)) return 0;
    pin->tid=GetWindowThreadProcessId(hwnd, &pin->pid);
    pin->owner_tid=GetWindowThreadProcessId(pin->owner, &owner_pid);
    return pin->pid && pin->pid!=GetCurrentProcessId() && owner_pid==pin->pid
        && AutoTileOwnedTopologyValid(pin, 0);
}

static int ATGOwnedPropertiesMatch(const ATGOwnedProfile *p, const ATGSelectedOwned *pin)
{
    TCHAR candidate[ATG_OWNED_CLASS], owner[ATG_OWNED_CLASS];
    return ATGOwnedClass(pin->hwnd, candidate) && !lstrcmp(candidate, p->candidate_class)
        && ATGOwnedClass(pin->owner, owner) && !lstrcmp(owner, p->owner_class)
        && ((DWORD)GetWindowLongPtr(pin->hwnd, GWL_STYLE)&ATG_OWNED_STYLE_MASK)==p->style
        && ((DWORD)GetWindowLongPtr(pin->hwnd, GWL_EXSTYLE)&ATG_OWNED_EXSTYLE_MASK)==p->exstyle
        && ((DWORD)GetWindowLongPtr(pin->owner, GWL_STYLE)&ATG_OWNED_STYLE_MASK)==p->owner_style
        && ((DWORD)GetWindowLongPtr(pin->owner, GWL_EXSTYLE)&ATG_OWNED_EXSTYLE_MASK)==p->owner_exstyle;
}

static int ATGOwnedWindowMatches(const ATGOwnedProfile *p, const ATGSelectedOwned *pin)
{
    TCHAR path[MAX_PATH];
    return p->valid && AutoTileOwnedTopologyValid(pin, 0) && ATGOwnedPropertiesMatch(p, pin)
        && GetWindowProgNameFullPath(pin->hwnd, path, ARR_SZ(path))==pin->pid
        && !lstrcmpi(path, p->path) && AutoTileOwnedTopologyValid(pin, 0)
        && ATGOwnedPropertiesMatch(p, pin);
}

/* Enrollment is explicit user consent, never inference or automatic learning. */
static int ATGOwnedEnroll(HWND hwnd, ATGOwnedProfile *p)
{
    ATGSelectedOwned pin;
    ATGOwnedImage image;
    int ok;
    mem00(p, sizeof *p);
    if (!ATGOwnedPin(hwnd, &pin)
        || !ATGOwnedClass(hwnd, p->candidate_class) || !ATGOwnedClass(pin.owner, p->owner_class)
        || GetWindowProgNameFullPath(hwnd, p->path, ARR_SZ(p->path))!=pin.pid
        || !ATGOwnedImageOpen(p->path, p->digest, &image)) return 0;
    p->style=(DWORD)GetWindowLongPtr(hwnd, GWL_STYLE)&ATG_OWNED_STYLE_MASK;
    p->exstyle=(DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE)&ATG_OWNED_EXSTYLE_MASK;
    p->owner_style=(DWORD)GetWindowLongPtr(pin.owner, GWL_STYLE)&ATG_OWNED_STYLE_MASK;
    p->owner_exstyle=(DWORD)GetWindowLongPtr(pin.owner, GWL_EXSTYLE)&ATG_OWNED_EXSTYLE_MASK;
    p->valid=1;
    ok=ATGOwnedWindowMatches(p, &pin) && ATGOwnedImageStable(&image);
    ATGOwnedImageClose(&image);
    if (!ok) mem00(p, sizeof *p);
    return ok;
}
#endif
