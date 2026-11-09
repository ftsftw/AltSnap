/* SPDX-License-Identifier: GPL-3.0-or-later
   Operation-local consent witnesses. No persistent handles or learned rules. */
#ifndef AUTO_TILE_OWNED_RUNTIME_H
#define AUTO_TILE_OWNED_RUNTIME_H
#include "auto_tile_owned_match.h"
static ATGOwnedProfile auto_tile_owned_profiles[ATG_OWNED_SLOTS];
static unsigned auto_tile_owned_enabled;

static void AutoTileOwnedLoad(const TCHAR *ini)
{
    unsigned i;
    auto_tile_owned_enabled=0;
    for (i=0; i<ATG_OWNED_SLOTS; ++i) {
        ATGOwnedProfileRead(ini, i, &auto_tile_owned_profiles[i]);
        auto_tile_owned_enabled+=!!auto_tile_owned_profiles[i].enabled;
    }
}

/* A verified minimized surface is present, but is not a grid participant.
   Freeze this distinction for one operation; a transition requires a new call. */
enum { ATG_OWNED_ABSENT, ATG_OWNED_PARTICIPATING, ATG_OWNED_MINIMIZED };
typedef struct {
    HMONITOR monitor;
    ATGOwnedProfile profiles[ATG_OWNED_SLOTS];
    ATGSelectedOwned pins[ATG_OWNED_SLOTS];
    ATGOwnedImage images[ATG_OWNED_SLOTS];
    unsigned states[ATG_OWNED_SLOTS];
    unsigned seen[ATG_OWNED_SLOTS], matches[ATG_OWNED_SLOTS];
    int initialized, failed;
} ATGOwnedOperation;

static BOOL CALLBACK AutoTileOwnedScan(HWND hwnd, LPARAM lp)
{
    ATGOwnedOperation *op=(ATGOwnedOperation *)lp;
    TCHAR path[MAX_PATH], classname[256];
    DWORD pid=0;
    unsigned i;
    int length;
    if (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL)!=op->monitor
        || !IsWindowVisible(hwnd) || hwnd==GetDesktopWindow() || hwnd==GetShellWindow()) return TRUE;
    if (!GetWindowThreadProcessId(hwnd, &pid) || !pid) return FALSE;
    if (pid==GetCurrentProcessId()) return TRUE;
    /* Reuse the existing process fallback, without its UWP child substitution.
       Unknown visible image identity conservatively cancels this opt-in run. */
    if (!GetWindowProgNameFromPidTraced(pid, path, ARR_SZ(path), NULL)) return FALSE;
    length=GetClassName(hwnd, classname, ARR_SZ(classname));
    if (length<=0 || length>=(int)ARR_SZ(classname)-1 || classname[length]) return FALSE;
    for (i=0; i<ATG_OWNED_SLOTS; ++i) {
        const ATGOwnedProfile *p=&op->profiles[i];
        if (!p->enabled) continue;
        if (lstrcmpi(path, p->path)) {
            if (!lstrcmp(classname, p->candidate_class) && GetWindow(hwnd, GW_OWNER)) return FALSE;
            continue;
        }
        ++op->seen[i];
        if (!lstrcmp(classname, p->candidate_class)) {
            if (++op->matches[i]>1) return FALSE;
            if (!op->initialized) {
                int minimized=!!IsIconic(hwnd);
                if (!ATGOwnedPin(hwnd, &op->pins[i]) || !!IsIconic(hwnd)!=minimized) return FALSE;
                op->states[i]=minimized ? ATG_OWNED_MINIMIZED : ATG_OWNED_PARTICIPATING;
            } else if (hwnd!=op->pins[i].hwnd) return FALSE;
        }
    }
    return TRUE;
}

static int AutoTileOwnedWindowStable(const ATGOwnedOperation *op, unsigned i)
{
    unsigned window_state=op->states[i];
    int minimized=window_state==ATG_OWNED_MINIMIZED;
    return (window_state==ATG_OWNED_PARTICIPATING || minimized)
        && !!IsIconic(op->pins[i].hwnd)==minimized
        && ATGOwnedWindowMatches(&op->profiles[i], &op->pins[i])
        && !!IsIconic(op->pins[i].hwnd)==minimized;
}

static int AutoTileOwnedCheck(ATGOwnedOperation *op)
{
    unsigned i, j;
    if (!op || op->failed) return !op;
    for (i=0; i<ATG_OWNED_SLOTS; ++i)
        if (op->profiles[i].enabled && !op->profiles[i].valid) goto failed;
    mem00(op->seen, sizeof op->seen); mem00(op->matches, sizeof op->matches);
    if (!EnumWindows(AutoTileOwnedScan, (LPARAM)op)) goto failed;
    for (i=0; i<ATG_OWNED_SLOTS; ++i) {
        const ATGOwnedProfile *p=&op->profiles[i];
        if (!p->enabled) continue;
        /* Presence without a unique main-class match is suspicious, including
           an updated class, hidden main with a visible dialog/helper, etc. */
        if (!op->seen[i]) {
            if (op->pins[i].hwnd) goto failed;
            continue;
        }
        if (op->matches[i]!=1 || !AutoTileOwnedWindowStable(op, i)) goto failed;
        /* Duplicate enabled registrations must not authorize the same HWND. */
        for (j=0; j<i; ++j)
            if (op->profiles[j].enabled && op->pins[j].hwnd==op->pins[i].hwnd) goto failed;
        if (!op->images[i].file) {
            TCHAR digest[65];
            if (!ATGOwnedImageOpen(p->path, digest, &op->images[i]) || lstrcmp(digest, p->digest)) goto failed;
        }
        if (!ATGOwnedImageStable(&op->images[i]) || !AutoTileOwnedWindowStable(op, i)) goto failed;
    }
    op->initialized=1;
    return 1;
failed:
    op->failed=1;
    return 0;
}

/* Return 1 for an authorized participant, 0 for an unrelated or verified
   minimized window, -1 after a lost witness. Rollback may still restore peers. */
static int AutoTileOwnedParticipant(ATGOwnedOperation *op, HWND hwnd)
{
    unsigned i;
    if (!op) return 0;
    for (i=0; i<ATG_OWNED_SLOTS; ++i) if (op->pins[i].hwnd==hwnd) {
        if (op->failed || !AutoTileOwnedWindowStable(op, i)
            || !ATGOwnedImageStable(&op->images[i])) { op->failed=1; return -1; }
        return op->states[i]==ATG_OWNED_PARTICIPATING;
    }
    return 0;
}

static void AutoTileOwnedClose(ATGOwnedOperation *op)
{
    unsigned i;
    for (i=0; i<ATG_OWNED_SLOTS; ++i) ATGOwnedImageClose(&op->images[i]);
}
#endif
