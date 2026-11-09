# AutoTileGrid (experimental MVP)

## Windows 10 first test

1. Open [GitHub Actions](https://github.com/ftsftw/AltSnap/actions/workflows/linked-grid-win64-test.yml),
   select a successful **Grid Windows x64 test ZIP** run for
   **feat/auto-tile-grid**, and check its commit SHA. Under **Artifacts**, download
   **AltSnap-auto-tile-win64-test** (GitHub sign-in is normally required).
   Unpack the artifact, then the contained portable ZIP into a separate writable
   folder. Keep AltSnap.exe, hooks.dll, AltSnap.ini and Lang together.
2. Exit the currently running AltSnap before launching the test copy. Back up
   your normal INI; edit the test folder's AltSnap.ini and add the setting below
   to its existing `[KBShortcuts]` section. Restart the test copy after editing.
   The ZIP ships with AutoTileGrid and Linked Grid Resize disabled.
3. Start with 1–4 separate, ordinary, resizable windows (normal or maximized) of a recent per-monitor
   DPI-aware application, for example current Edge/Chrome windows. Use the same
   monitor/scaling and privilege level. Minimize other eligible windows on that
   monitor; tabs are not separate windows. Avoid MobaXterm and apps with special
   size constraints for the first successful layout check.
4. Put the cursor on that monitor and press **Ctrl+Alt+G**. Check the layouts in
   the table below, repeat to check stability, and try five windows to check
   refusal. Include maximized windows, windows partly beyond each screen edge,
   and mixed normal/maximized sets; no manual restore or reposition is needed.
   Windows assigned to the other monitor must remain untouched.
5. For the four-window integration test also set `[General]`
   `LinkedGridResize=1` and `StickyResize=2`, then restart. Use your configured
   AltSnap resize gesture (default Alt + right-button drag) at each inner corner.
   Check both `FullWin=0` and `FullWin=1`. Press Ctrl+Alt+G during a drag: it must
   leave the drag alone. Release the drag before testing another layout.
6. Finally try an app with restrictive sizes, including MobaXterm. An infeasible
   layout should refuse before moving anything. A late size correction can
   trigger best-effort restoration; verify the result and report any partial
   layout. No application-specific size restrictions are bypassed. Repeat the
   previously successful Linked Grid +107 px refusal/rollback scenario and
   confirm that all four windows remain aligned and the drag stops.

If nothing happens, check the INI next to the running test EXE, its section and
the value 839, restart AltSnap, and ensure another app has not registered
Ctrl+Alt+G. The command deliberately does nothing for zero or more than four
eligible windows, during a drag, or when any otherwise eligible participant
fails DPI, identity, timeout, min/max or stable-snapshot checks. Minimize suspect
windows and retry with one known per-monitor-aware window. An elevated, hung or
legacy-DPI window may cause a safe refusal; do not force resizing or elevate
AltSnap just to bypass it. Remove the shortcut or set `AutoTileGrid=0` and
restart to disable its registration.

## Eligibility diagnostics (next Windows test)

The supplied Windows 10 results confirm AutoTileGrid for Chrome, VS Code,
OpenCode and Windows Terminal, including 2×2 and subsequent Linked Grid Resize.
VS Code's earlier elevated instance blocked tiling after process lookup; running
it without elevation worked. This is not evidence that process lookup bypasses
Windows privileges. Portable MobaXterm returns `skip-owner` both normal and
maximized. With it open, three ordinary windows tile in 1+2 while MobaXterm stays
in place. Its earlier Linked Grid Resize minimum-size/guarded rollback test also
passed; that does not establish AutoTileGrid eligibility. The owner diagnostics
below are new and have not yet been tested against the real MobaXterm GUI.

Set `AutoTileTrace=1` in the existing `[General]` section and restart AltSnap.
Capture Win32 debugger output with DebugView, filter **AltSnap.ATG**, clear the
capture before each test, and press the shortcut once. Keep the full filtered
output through the final outcome. Test separately:

1. One VS Code window, with other eligible windows minimized.
2. One MobaXterm window, first normal and then maximized.
3. The four-window mix: VS Code, MobaXterm, OpenCode and Windows Terminal.
4. The previously working Chrome/OpenCode/Windows Terminal layouts and a
   four-window layout followed by Linked Grid Resize (FullWin=0 and FullWin=1).
5. Repeat the successful controls with tracing off to check timing influence.

Record app/state/monitor/scaling/privilege level manually, outside the trace.
Disable `AutoTileTrace` afterwards. Do not elevate AltSnap to force participation.
No trace files, window identifiers, titles, executable names, paths, PID/TID,
geometry or content are emitted. Only fixed codes, categorical owner observations,
counts/participant indices and existing numeric Win32 process-query errors are
written. No extra diagnostic window/process queries or GetLastError/SetLastError
calls run with `AutoTileTrace=0`.

Each capture is bracketed by `scan-begin count=0` and `scan-candidates count=N`.
Monitor/invisible/cloaked/zero-size/minimized exclusions are aggregated into at
most one line **per reason per capture**, e.g. `skip-invisible count=500`.
Other eligibility records have `count=1`. Safety checks intentionally perform
multiple captures; these are separate passes, not unique-window counts.
Identity rechecks outside enumeration can also emit individual rejection records.

| Event | First rejecting predicate / meaning |
| --- | --- |
| `skip-monitor` | Assigned to another monitor or no monitor |
| `skip-invisible`, `skip-cloaked`, `skip-zero-size`, `skip-minimized` | Visibility/snap helper exclusions (aggregated) |
| `skip-snap-shape` | No full caption, thick frame, AltSnap borderless flag or Snaplist match |
| `skip-snap-state-changed` | Snap helper refused, but diagnostic reread no longer explains it |
| `skip-noactivate`, `skip-fullscreen` | No-activate flag or existing fullscreen helper |
| `skip-thread-query`, `skip-own-process` | Failed thread/process query or AltSnap's current process |
| `skip-desktop`, `skip-shell` | Desktop or shell window |
| `skip-child`, `skip-no-thickframe` | WS_CHILD or missing WS_THICKFRAME |
| `skip-maximized-no-caption` | Maximized without the full WS_CAPTION bits |
| `skip-toolwindow`, `skip-owner`, `skip-not-enabled` | Toolwindow flag, owner, or disabled window |
| `skip-taskbar`, `skip-secondary-taskbar`, `skip-progman`, `skip-worker` | Existing shell class exclusions (fixed categories, no class text) |
| `skip-move-disabled`, `skip-rolled` | Existing AltSnap properties |
| `skip-blacklist-process`, `skip-blacklist-window`, `skip-blacklist-bottommost` | Existing corresponding blacklist matched |
| `skip-process-unreadable` | Terminal process identity lookup failure; inspect preceding `process-*` events |
| `skip-altsnap-exe` | Readable identity matched the AltSnap executable exclusion |

The snap helper stays unchanged. Only after it rejects, tracing rereads its
constituent predicates to classify the rejection. Those diagnostic observations
can race with window changes and never change eligibility; a mismatch should be
retested with the window stable. Ordinary/style/owner checks emit the actual
first short-circuited reason. One record need not reveal every possible blocker.
No WS_THICKFRAME, child, owner, fullscreen or other safety check was relaxed.

Process detail uses `reason=... error=N` (not `count`):

| Event | Stage |
| --- | --- |
| `process-window-thread-failed` | GetWindowThreadProcessId failed |
| `process-window-id-missing` | No usable process ID returned (no ID is emitted) |
| `process-open-vm-failed` | OpenProcess(QUERY_INFORMATION | VM_READ) failed |
| `process-open-query-failed` | Fallback OpenProcess(QUERY_INFORMATION) failed |
| `process-module-failed` | GetModuleFileNameEx failed |
| `process-image-failed` | Fallback GetProcessImageFileName failed |
| `process-name-at-capacity` | A result reached/exceeded capacity and was rejected |
| `process-limited-api-missing` | QueryFullProcessImageNameA/W export unavailable (127) |
| `process-open-limited-failed` | OpenProcess(QUERY_LIMITED_INFORMATION) failed |
| `process-limited-image-failed` | QueryFullProcessImageName failed, including insufficient buffer (122) |
| `process-limited-invalid` | API reported success but its count/string was incomplete or invalid |
| `process-limited-used` | Complete limited-query identity used; no name/path is emitted |
| `process-host-no-child`, `process-host-child-failed`, `process-host-child-used` | Existing ApplicationFrameHost child-identity fallback outcome |

Error 5 is access denied, 299 is partial copy, 122 is insufficient buffer and
127 means a missing dynamically loaded export (or an API-reported error 127).
Error 0 means no error was supplied or a non-error diagnostic event. LastError
is cleared before the traced call and sampled immediately on failure, before
CloseHandle or debug output. A failed first stage **can recover** through a
fallback: only the terminal `skip-process-unreadable` means process exclusion.
For VS Code, retain these stage/error lines plus both scan markers and final
`capture`, `success` or other refusal event. For MobaXterm, retain the exact
`skip-*` reason and scan/final events for both show states and the mixed test.

Other existing codes: `skip-dwm`, `skip-borderless-overhang`, `too-many`,
`placement`, `dpi-rect-limits`, `frame-overflow`, `frame-limits`,
`preflight-changed`, `normalized`, `normalize-failed`,
`normalized-capture-changed`, `normalized-preflight-changed`, `no-feasible-plan`,
`batch-create-failed`, `batch-verification-failed`, `cancelled`,
`restore-max[-failed]`, `restore-normal[-failed]`, `unchanged`, `success`.
`capture count=0` is the final summary for an empty or failed capture;
`busy-or-no-monitor` means the command cannot start. Successful tiling uses
`success count=N`; that result alone does not establish support for omitted apps.

`LinkedGridTrace` remains a separate **Linked Grid Resize** diagnostic setting;
see [its diagnostics](tests/LINKED_GRID_DIAGNOSTICS.md).
**Real Windows GUI testing was not performed in the cloud.** Portable adapter
stubs and cross-compilation do not establish real Windows application behavior.

## VS Code: limited process query

`window_process_name.h` still tries the existing PSAPI sequence first:
OpenProcess(QUERY_INFORMATION | VM_READ) plus GetModuleFileNameEx; if that open
fails, OpenProcess(QUERY_INFORMATION); then GetProcessImageFileName if no complete
module path was read. A successful old lookup never requests the new access
right or resolves the new API. Only after these paths fail does the shared
helper resolve QueryFullProcessImageNameA/W from KERNEL32.DLL and request
OpenProcess(QUERY_LIMITED_INFORMATION), with no inheritance or VM_READ. It uses
flags=0 for a Win32 path. Missing exports, access denial or failed reads remain
failures. This is ordinary Windows access checking, not a UIPI/protection bypass.
See the [Microsoft API contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamea).

The export matches the existing `_UNICODE` build convention. Buffer capacities
are TCHAR counts, not byte counts; the returned count excludes the terminator.
The helper checks size_t-to-DWORD range, nonzero length below capacity, the
terminator at that length, no embedded NUL and a nonempty basename. Truncated or
malformed legacy results are also rejected before trying another path. It does
not grow the caller's buffer or accept partial long paths. Every opened process
handle is closed, including read failures. A terminal failure clears the output.

All existing GetWindowProgName/FullPath callers, including general blacklists,
use this same fallback with tracing off. AutoTileGrid reads one validated process
name and passes it to the unchanged wildcard/whitelist matcher for Processes,
Windows and Bottommost, with the existing window title/class inputs. It avoids
the drag cache and independent process rereads for these three lists, so an empty
or stale process name cannot bypass them. An unreadable identity is now rejected
before these list matches. AltSnap's current process and AltSnap.exe remain
excluded. Snaplist and all other eligibility/geometry/rollback gates stay intact.

ApplicationFrameHost keeps the first child from another process policy and the
host fallback. Both identities can use the limited query. If a child query fails,
the host is reread because a failed API may have damaged the shared output; if
the host cannot be read again, the whole lookup fails rather than using child
bytes as a host name.

Windows GUI validation still needed: test VS Code normal/maximized alone and
in 1–4-window mixes at equal privileges; repeat with Processes/Windows/Bottommost
blacklists (including executable-qualified entries and whitelist mode). Confirm
`process-limited-used` followed by the expected layout or blacklist refusal, and
repeat with trace off. Test inaccessible/elevated processes without elevating
AltSnap; an unreadable identity must remain excluded. Test an ApplicationFrameHost
app with and without an accessible child. Stub/CI success does not prove that a
real VS Code process permits the limited right or passes later DPI/size gates.

## MobaXterm: owner analysis, no eligibility change

`skip-owner` retains its unconditional refusal. Only when `AutoTileTrace=1`,
an owner rejection is preceded by at most four contiguous diagnostic lines,
all matching the existing **AltSnap.ATG** DebugView filter. There are no handles,
PID/TID, executable names, titles, paths, class names, geometry or raw style/error
values in these new lines. They describe the current rejected candidate, not a
persistent window identity. Multiple captures can repeat the block.

| Fixed reason | Fields and exact categories |
| --- | --- |
| `owner-state` | `link=stable`, `owner-gone`, `window-gone`, `cleared`, or `changed` |
| `owner-flags` | `process=same/different/unknown`; owner `visible=yes/no`, `enabled=yes/no`; `tool`, `app`, `noactivate` each `yes/no/unknown` |
| `owned-flags` | Rejected candidate's `popup=yes/no`, `app=yes/no`, `enabled=yes/no` |
| `owner-relations` | `root`, `rootowner`, `lastpopup` each `self/owner/rootowner/other/unknown` as applicable |

`owner-state` checks candidate/owner existence before observation, then rechecks
existence and the original GW_OWNER link after observation. `stable` means those
checks saw the same link; `cleared` means the link became null while both handles
still passed IsWindow; `changed` means another non-null owner was observed.
`owner-gone`/`window-gone` mean IsWindow failed for the original owner/candidate,
including failure at the initial check. If several conditions change, candidate
absence takes precedence over owner absence, then cleared/changed link. This
cannot distinguish destruction from every other invalid-handle condition.
Only `owner-state` is emitted on detected instability, followed by `skip-owner`;
partial feature observations are discarded and exclusion is never reconsidered.

`process` compares the candidate PID already obtained by eligibility with one
GetWindowThreadProcessId(owner) result; failed or zero identities are `unknown`.
It never opens either process or performs executable lookup. Owner style bits
come from one GetWindowLongPtr(GWL_EXSTYLE); a failed zero-valued read produces
`unknown` for all three bits, while a successful zero means all three are `no`.
The thread's last-error value is restored after diagnostics. `visible` means
IsWindowVisible, not uncloaked, unobscured or user-perceived visibility. Candidate
popup/app bits use eligibility's existing style reads; candidate enabled and owner
visibility/enabled state are separate read-only observations.

`root` is GetAncestor(candidate, GA_ROOT), `rootowner` is GA_ROOTOWNER, and
`lastpopup` is GetLastActivePopup(rootowner), called once only if rootowner is
non-null. There is no ownership/popup chain traversal. Relations compare handles
only in memory, with precedence `self`, `owner`, `rootowner`, `other`; a null
result is `unknown`, not proof of an absent dialog. `rootowner=rootowner` means a
non-null ancestor different from both candidate and immediate owner (a deeper
chain). Last-active popup is activation history, not a complete dialog list or a
modality test. These APIs send no diagnostic messages, do not activate/resize or
otherwise modify windows, and require no background process, worker or elevation.
With tracing off the original single GW_OWNER eligibility read remains, and none
of these extra queries, formatting, error-state operations or output runs.
Earlier exclusions such as tool/no-activate windows still short-circuit before
GW_OWNER; they do not produce an owner block.

These are best-effort sequential samples, not atomic snapshots. Owner flags,
process identity and root/popup relations can change between calls even when the
link recheck is stable. Same-process/thread HWND reuse or a link changing away
and back can escape detection. Root/popup handles are only compared, not queried
for additional properties. Do not use these observations as an eligibility rule.

| Observation | Why it cannot prove an independent main window |
| --- | --- |
| Same process, hidden owner, enabled candidate | Also possible for modeless dialogs and toolkit helper owners |
| Disabled owner | Compatible with modality, but not a complete modal-dialog test |
| WS_POPUP absent/present | A window style choice, not a main-window guarantee |
| WS_EX_TOOLWINDOW absent | Dialogs need not be tool windows |
| WS_EX_APPWINDOW present | Taskbar presentation, not safe resizing or non-dialog status |
| Caption, thick frame, maximized state | Resizable dialogs can share all of these |
| GA_ROOT equals self | Top-level parentage; ownership is a separate relation |
| GA_ROOTOWNER / last-active popup relations | Ownership/activation history, not a reliable dialog classifier |
| Modal-frame style or standard dialog class absent | Custom dialogs need neither; collecting class names is unnecessary here |

A future exception would require stable joint evidence across normal/maximized
main windows and modal/modeless/custom dialogs, including disabled owners and
activation changes. These observations may reveal an auxiliary hidden-owner
pattern worth investigating; they cannot establish a general safe classifier.
No classifier, MobaXterm/EXE allowlist or Alt-Tab exception is implemented.
hooks.c's IsAltTabAble is a presentation selector, too permissive for automatic
movement. Process lookup (including the VS Code fallback), all blacklists,
Linked Grid Resize, min/max checks and recovery rules are unchanged.

Portable packaging **could** indirectly affect topology if its launcher or UI
initialization creates a hidden helper owner or a different process relationship.
Portability itself is not a Win32 owner property and supplies no evidence for that
hypothesis. Static AltSnap inspection and stubs cannot establish whether portable
mode causes MobaXterm's owner. A controlled real Windows comparison of the same
MobaXterm version/configuration in portable and installed modes, normal/maximized
and with dialogs, is needed; no portable-specific cause or support is claimed.

### Short Windows 10 portable MobaXterm check

1. Use the portable ZIP for this commit as described above, at ordinary privileges.
   Set `[KBShortcuts] AutoTileGrid=839` and `[General] AutoTileTrace=1` in the test
   INI; restart AltSnap. In DebugView filter `AltSnap.ATG`. Keep app/version,
   privilege, scaling, monitor and the test case in separate manual notes.
2. Minimize other eligible apps. Keep portable MobaXterm **normal**, clear DebugView,
   put the cursor on its monitor and press Ctrl+Alt+G once. Retain the complete
   scan/final output including the owner block and `skip-owner`; verify no movement.
3. **Maximize** MobaXterm, clear capture and repeat once. Verify it stays maximized
   in place. Compare categorical owner observations with the normal case.
4. Return it to normal, open a modal MobaXterm settings/confirmation dialog, clear
   capture and invoke once. Record whether the main surface is disabled and how
   the owner/popup block differs. Verify neither main surface nor dialog moves.
   Close the dialog and repeat with a modeless dialog if the tested version has
   one. A dialog may hit an earlier gate and have no owner block; record that
   limit instead of treating a missing block as evidence of no dialog.
5. Close dialogs. On that monitor open exactly three ordinary windows (for example
   Chrome, VS Code without elevation and Windows Terminal), plus MobaXterm. Clear
   capture and invoke once with MobaXterm normal, then again maximized. Expect
   `scan-candidates count=3`, a final successful three-window **1+2** layout, and
   MobaXterm still excluded/in place. The three windows use the whole work area:
   **they may overlap/cover MobaXterm**. Exclusion does not reserve space for it.
6. Repeat the mixed check with `AutoTileTrace=0` after restarting AltSnap. Expect
   the same exclusions/layout and no `AltSnap.ATG` output; leave tracing disabled.

Retain full filtered output, including repeated captures and terminal refusal
codes, rather than just the owner block. No new Windows GUI run was performed in
the Linux environment. Until that run, the new diagnostics and any distinction
between owned main surfaces and dialogs remain unverified on actual MobaXterm.

Sources: Microsoft [GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow),
[window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles),
[extended styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles),
[GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor),
[GetLastActivePopup](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastactivepopup),
[IsWindowEnabled](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindowenabled)
and [owned windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#owned-windows).

## Configuration and layout

Assign the command in the existing AltSnap.ini keyboard section, then reload
AltSnap settings:

```ini
[KBShortcuts]
AutoTileGrid=839
```

839 is Ctrl+Alt+G (G=71, Alt=256, Control=512). An absent entry or
`AutoTileGrid=0` leaves the command disabled, including in the shipped default
configuration. No GUI setting is added. `UsePtWindow` does not affect this
monitor-wide command: the cursor position at invocation selects the monitor.
There is no foreground-window initialization. Forward normalization requests
no activation, and the final positioning batch requests no activation or Z-order
change. Maximized-window failure recovery has a Win32 limitation described below.

The command uses the monitor's current work area (excluding the taskbar):

| Eligible windows | Layout / cell order |
| --- | --- |
| 1 | Full work area |
| 2 | Left, right |
| 3 | Full-height left, top-right, bottom-right |
| 4 | Top-left, top-right, bottom-left, bottom-right |
| 0 or more than 4 | No movement |

Splits use the same integer subdivision convention as generated Snap Layout
zones, with spare pixels on the right/bottom. All cells exactly abut; SnapGap
is deliberately ignored. Existing saved layouts/zones are not modified.
All feasible assignments (at most 24) are compared by summed Manhattan distance
between current and destination centers. Equal costs use ascending HWND values
in cell order. This is deterministic across enumeration/Z-order changes during
the windows' lifetimes, and an already tiled set is a fixed point. Constraints
can select a more distant feasible assignment. Splits stay at the exact halves;
an infeasible arrangement is refused instead of resized into overlapping cells.

## Eligibility and conservative refusals

Only visible, non-cloaked, enabled, resizable top-level windows on the selected
monitor are considered. Existing snap visibility and process/window blacklist
helpers are reused. Desktop/shell/taskbars, AltSnap's process and AltSnap.exe,
tool/no-activate windows, owned windows/dialogs, minimized, fullscreen,
rolled, MoveOnOff-disabled and Bottommost-blacklisted windows are excluded.
WS_THICKFRAME is required; ResizeAll/AlwaysResize cannot force participation.
Process identity must be readable. Missing DWM data remains excluded.
Ordinary captioned windows may be maximized or partly outside the selected
monitor, including spanning an adjacent monitor. `MonitorFromWindow` with
`MONITOR_DEFAULTTONULL` assigns them by largest intersection; merely touching
another monitor does not make them participate there. Completely off-desktop
windows have no selected monitor. No nearest-monitor fallback or origin-zero
coordinate assumption is used. Borderless/custom windows do not gain overhang
or maximized support. Other monitors' assigned windows are untouched.

The initial Windows 10 report confirmed normal 1–4-window layouts, five-window
refusal, Chrome Linked Grid Resize and the earlier MobaXterm recovery fix. It also
identified the old `IsZoomed` and full-monitor-containment exclusions as reasons
why newly opened Chrome windows could be omitted. Those exclusions are now
relaxed only for ordinary captioned windows.

The remaining participants must all be per-monitor DPI aware, with window DPI
matching the monitor's effective DPI. Legacy/system/unaware DPI virtualization,
mixed DPI, unavailable DPI APIs, negative/unrepresentable native frame insets,
failed/timed-out WM_GETMINMAXINFO queries (32 ms), invalid track limits and
infeasible native sizes reject the whole operation. Native limits are queried
even if IgnoreMinMaxInfo is set. Invisible borders are subtracted from limits
and added to each destination. Signed 64-bit planning arithmetic guards negative
coordinates, odd dimensions and 32-bit overflow.

The adapter captures twice and verifies that participants, IDs, rectangles,
placements (including normal restore rectangles), borders, limits, DPI and
monitor/work area have not changed, then rechecks the baseline after
message-sending queries. Busy/reentrant invocations and active or
pending AltSnap movement refuse without cancelling or altering the drag. It does
not start any process, service or worker, and never requests administrator rights.

Before planning, each ordinary maximized participant is synchronously normalized
with `SetWindowPlacement(SW_SHOWNOACTIVATE)`. The complete original
`WINDOWPLACEMENT` and native/DWM rectangles are kept separately. A temporary
normal rectangle on the selected work area uses workspace coordinates, with
signed 64-bit conversion and range checks, even when the saved restore rectangle
was on another monitor. No styles are changed, no synthetic input is sent, and
no asynchronous placement requests are queued. Each normalization is followed by
identity/state/geometry checks. Normal frame insets, DPI and min/max limits are
then captured again and checked for stability before planning. Maximized frame
insets are never reused for the tiled normal windows. A normalization failure,
changed participant set or infeasible normalized plan enters guarded recovery.

Final placement uses one DeferWindowPos batch with
NOACTIVATE/NOZORDER/NOOWNERZORDER; exact repeats skip positioning. Actual native
and DWM rectangles are checked afterwards. On failure, a best-effort restoration
uses original native rectangles for normal participants and the saved full
placement for originally maximized participants, only while process/thread
identity, DPI, monitor assignment, native/DWM geometry and observed placement
still match. Recovery also covers partial normalization, batch creation failures
and failures before the batch. A late minimize/maximize, restore-rectangle change,
external move or identity replacement is never deliberately overridden. A new AltSnap movement invalidates
further restoration. Win32 batches are not transactions: applications can change
constraints, disappear, refuse both sizes or alter focus/Z-order themselves.
Restoration cannot guarantee atomicity against such application behavior. HWND
reuse within the same process/thread is another existing Win32 identity limit.

**Maximized recovery limitation:** the documented `SW_SHOWMAXIMIZED` command
can activate/raise a window. Win32 has a no-activate restore command but no
corresponding documented no-activate maximize command. Recovery therefore uses
the saved `WINDOWPLACEMENT` unchanged, rather than changing styles, hiding windows,
stealing focus back or manipulating Z-order. A failed operation involving
maximized windows may change focus/Z-order or leave a partial restoration if an
application refuses recovery or a new user action cancels it. Successful forward
tilings request no activation; applications can still react to sizing themselves.
The successful user-reported tilings do not establish every failure-recovery
focus/Z-order scenario; those still need real Windows checks.
See the Microsoft [ShowWindow command contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)
and [WINDOWPLACEMENT coordinate contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement).

## Linked Grid Resize

Four destinations use one common X/Y seam and exact DWM visible bounds. They
are geometrically compatible with the unchanged `lg_resize` detector; no restore
flags or saved zone membership are needed for detection. Opt in separately:

```ini
[General]
LinkedGridResize=1
StickyResize=2
```

Start an existing AltSnap resize at an inner corner. StickyResize=2 activates
without Shift; StickyResize=1/3 uses Shift as documented by the existing feature.
The branch includes Linked Grid Resize diagnostics and the guarded-witness
rollback fix from commits `0ce2050` and `b894487`. Legacy Sticky Resize behavior
is preserved; AutoTileGrid itself does not change drag processing or the queue.

## Validation and manual Windows test

`make -C tests test` runs the linked-grid/queue/recovery tests, both production
Win32 adapters against deterministic stubs, and AutoTileGrid planner tests.
The planner suite covers 1–4 layouts, zero/five-window refusal,
negative origins, odd/tiny work areas, min/max limits, constrained assignments,
variable native borders, extreme coordinates/overflow, duplicate identities,
enumeration-independent ties, repeated calls and untouched output on failure.
2,000 cases are checked against an independent exhaustive assignment oracle.
The four-cell result, including different native borders, is fed into the actual
`lg_resize`, `lg_drag_begin` and `lg_drag_update` for every active quadrant.
The AutoTileGrid adapter suite covers excluded/other-monitor windows, stable
repeats, unsafe DPI/min-max refusal, changing preflight geometry, batch failures,
MobaXterm-like +107 px correction/refusal, guarded restoration after external
geometry/identity changes, and active/pending drag refusal. It now also covers
all 30 normal/maximized combinations for counts 1–4, repeated normalized layouts,
different maximized/normal frame insets, overhang at every edge, other-monitor
assignment, negative monitor origins and top/left taskbars, off-monitor saved
restore placements, changed enumeration order, normalization and placement API
failures, infeasible limits after restore, failed recovery APIs, DPI/monitor/show
state changes, cancellation and opt-in trace output. These model tests
cannot validate real DWM, UIPI or application message handling.
The new owner tests exhaust 2,048 candidate/owner/style/process/state/show-state
combinations with tracing both off and on (4,096 cases), plus link races,
root/popup relations, unknown styles and mixed three-window layouts, blacklist,
infeasible-plan/five-participant refusals and rollback. An exact fixed token
vocabulary checks owner log privacy. Trace-off tests count extra queries and
error-state operations. ASan/UBSan and MinGW x64 test compilation are supported
by tests/Makefile.

Full Windows x64 application build (no Windows GUI execution):

```sh
make -f MakefileX64 -j2 \
  CC=x86_64-w64-mingw32-gcc-win32 WR=x86_64-w64-mingw32-windres
```

The user-reported Windows 10 results above are established observations; the
new owner diagnostics and remaining GUI scenarios still require real testing.
Portable tests do not exercise enumeration, UIPI, DWM, DPI message marshalling,
DeferWindowPos or recovery against real applications. On Windows, verify:

1. 1–4 Chrome/Edge windows: all normal, all maximized, then mixed; repeat the
   shortcut and confirm stable assignment. Try a partly offscreen window at each
   edge, and a maximized window whose saved normal rectangle is on another monitor.
2. 0/5 eligible windows; invalid/large minimum sizes; hung/elevated windows.
3. Cursor on each monitor, including a negative origin and odd work-area size;
   confirm the other monitor, foreground app and Z-order remain unchanged.
4. 100/125/150/200% scaling, different borders, taskbar placement/autohide;
   legacy or mixed-DPI participants must safely refuse.
5. Excluded windows and process/window blacklists, including with UsePtWindow=1
   and a blacklisted foreground window on a different monitor.
6. Four-window tiling followed by Linked Grid Resize from all four inner
   corners, both FullWin=0 and FullWin=1. Verify exact visible seams.
7. Close/move a participant or change constraints during placement; verify the
   refusal/restoration behavior. Invoke during a drag and confirm no interference.

Legacy DPI, forced-resize/custom-border windows and stronger application-specific
recovery remain outside this implementation. No Windows GUI test was run in the
Linux cloud environment; portable tests and x64 compilation are not that test.
