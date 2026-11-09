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

For AutoTileGrid diagnostics, set `AutoTileTrace=1` in the existing `[General]`
section and restart. Leave it absent/0 normally. Capture debugger output with
DebugView (Win32 capture) or an attached debugger, press the shortcut once, then
filter `AltSnap.ATG`. Output contains fixed reason codes and participant counts
or zero-based participant indices only: no handles, process IDs, executable names,
window titles, geometry or content. No log file or worker is created, and the
trace does not issue extra window queries. Disable it again after diagnosis.

Useful codes: `skip-monitor`, `skip-hidden-or-fullscreen`, `skip-nonordinary`,
`skip-disabled-or-blacklisted`, `skip-process`, `skip-dwm`,
`skip-borderless-overhang`, `too-many`, `placement`, `dpi-rect-limits`,
`frame-overflow`, `frame-limits`, `preflight-changed`, `normalized`,
`normalize-failed`, `normalized-capture-changed`, `normalized-preflight-changed`,
`no-feasible-plan`, `batch-create-failed`, `batch-verification-failed`,
`cancelled`, `restore-max[-failed]`, `restore-normal[-failed]`, `unchanged`,
`success`. `capture` is the final summary for an empty or failed capture;
`busy-or-no-monitor` means the command cannot start. The trace can contain
several exclusion records because the safety checks capture more than once.

`LinkedGridTrace` remains a separate **Linked Grid Resize** diagnostic setting;
see [its diagnostics](tests/LINKED_GRID_DIAGNOSTICS.md).
**Real Windows GUI testing was not performed in the cloud.** Portable adapter
stubs and cross-compilation do not establish real Windows application behavior.

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
This needs a real Chrome/Edge Windows 10 check before claiming GUI stability.
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
ASan/UBSan and MinGW x64 test compilation are supported by tests/Makefile.

Full Windows x64 application build (no Windows GUI execution):

```sh
make -f MakefileX64 -j2 \
  CC=x86_64-w64-mingw32-gcc-win32 WR=x86_64-w64-mingw32-windres
```

Real Windows GUI behavior remains **unverified** until the user tests it.
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
