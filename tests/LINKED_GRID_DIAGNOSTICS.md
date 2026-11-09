# Fast movement investigation (Windows 10)

The reported mixed-application failure has **not** been reproduced on a Windows
desktop here. This change adds opt-in diagnostics, not a speculative geometry or
rollback fix. Four Notepads working on the user's machine does not establish how
terminals or Explorer handle positioning messages.

## Audit and deterministic evidence

The bounded mailbox retains one complete `(generation, x, y)` sample and at most
one outstanding wake. Offers replace both axes together; intermediate coordinates
are unnecessary. Finish is posted behind that wake and production stops before
finish. Busy reentry invalidates the operation instead of recursively applying
another batch. Threading/queue policy is unchanged; slow cross-process Win32
calls can still delay the hook owner.

`MouseMoveNow` calculates the intersection from the initial cursor using 64-bit
arithmetic, saturates to int32, and `lg_drag_update` clamps against all four
captured min/max intervals. Large jumps do not accumulate rounding or use a
previous cell's geometry. WM_GETMINMAXINFO is only a snapshot: it cannot describe
character-cell increments, arbitrary WM_WINDOWPOSCHANGING corrections or later
asynchronous changes. No application constraints are bypassed by this change.

`LinkedGridValid` checks identity, state, DPI, native rectangles and DWM-derived
border extents before movement and commit. Commit applies four native targets
with DeferWindowPos and verifies exact native rectangles afterward. Failure
attempts recovery and blocks the drag. Recovery requires unchanged identity,
DPI/border and an actual rectangle equal to the requested one; a third rectangle
is left alone because an external move cannot be distinguished from an app's
correction. DeferWindowPos is not an atomic transaction against application code.

`linked_grid_win_test.c` includes the **production adapter** with Win32 stubs.
These outcomes also reproduce against the original adapter at
`0dff0da5988124d38fd80fafbb7b255bbf397d22` (before diagnostic changes):

| Simulated behavior | Result |
| --- | --- |
| Exact sizes, extreme jumps, repeated coalesced samples | Grid aligned; four min/max intervals honored |
| One participant rejects size; DWM current | Accepted survivors restore; drag blocked |
| One participant corrects size by 7 pixels | Third rectangle skipped; survivors restore; grid can remain broken |
| Exact sizes; stale DWM bounds | Native grid intact; next border validation stops drag |
| One rejection plus stale DWM on survivors | Survivors fail border eligibility; rollback skips them; grid can remain broken |
| Native change after successful verification | Next validation stops drag; changed grid can remain broken |

Tests also cover failed batches, rollback refusal and cancellation during a batch.
All four active corners and each failing participant are exercised. Stub behavior
is injected deliberately; it is **not evidence that a particular real app or
DWM behaves this way**. Late correction, transient DWM bounds, runtime limits
and notification-related changes remain hypotheses for the user's case.
Removing frame checks or restoring arbitrary third rectangles without evidence
would weaken protection of external changes, so no such fix is made.

Capture latches ACTIVE/BLOCKED. `lg_drag_stop` cannot switch either to LEGACY;
movement returns before legacy clamping/Sticky Resize, clears LastWin.hwnd, and
finish bypasses the legacy finalizer. Shift changes cannot switch the mode.
Tests exercise adapter/policy transitions, not hooks or the real message pump.

## Next Windows test

1. Use this commit's x64 test build (the existing feature-branch Actions workflow
   uploads `AltSnap-linked-grid-win64-test`; no Release is created). Exit the old
   AltSnap instance before replacing its exe and hooks.dll.
2. In AltSnap.ini set `[General] LinkedGridResize=1` and `LinkedGridTrace=1`.
   Keep your previously successful StickyResize/modifier settings. Set
   `[Performance] FullWin=1` explicitly for the first test. Restart AltSnap.
3. Capture OutputDebugString output with a debugger or Sysinternals DebugView
   (Capture Win32), filtering **only** `AltSnap.LG`. Start before the drag.
   The app writes no trace files and needs no additional runtime library.
   Tracing defaults off (missing key or zero). With LinkedGridResize=0 the key
   is not read and no new diagnostic calls, counters, queries, allocations or
   output run on the legacy move path.
4. On each monitor separately, arrange four Notepads with Win+arrows, then the
   mixed Explorer/OpenCode/MobaXterm/Windows Terminal grid. Hold the Sticky Resize
   modifier before clicking. Try slow moves and sharp long horizontal, vertical
   and diagonal jumps from each inner corner. Include monitor 2/negative
   coordinates; record the scaling of both monitors.
5. Save one short filtered trace from capture through failure and mouse release.
   Try Escape and a fresh drag after stopping. Repeat with FullWin=0 (outline,
   batch on release), then tracing off to check timing influence. Finally check
   ordinary resizing with LinkedGridResize=0.
6. Report active quadrant, app in each quadrant, FullWin, monitor/scaling,
   direction and whether breakage appeared during movement or at release.
   App names are manual test context, not trace data. Share only the filtered
   trace. Set LinkedGridTrace=0 afterward.

## Reading the trace

Scalar records carry ticks, generation, mode (`0=LEGACY, 1=ACTIVE, 2=BLOCKED`),
event, cell (`0=TL, 1=TR, 2=BL, 3=BR; -1=whole drag`) and event-specific `a,b`.
`requested`/`solved` give cursor-derived/clamped intersections. `offers.a` counts
samples since dispatch (more than one means coalescing). `begin.a` is LGStatus
(`0=OK, 4=incompatible limits`). `end-defer.a` is success; `.b` is the participant
bitmask. Error records contain Win32 error codes. `recover-mask.a` is restoration
eligibility, `.b` valid identity; bits correspond to cells and explain skips.

Rectangle records contain generation, cell, readable flag and `want/got` as
left,top,right,bottom. `verify-mismatch` preserves the exact failed native read.
`valid-native` means subsequent drift; `valid-border`/`recover-border` compare
cached/fresh frame extents. Snapshots at baseline/failure/recovery/release show
native target/observation, DWM bounds versus the last verified native baseline,
and cached border extents. Deflate native bounds by the cached border to obtain
expected visible bounds. Reads are sequential, not atomic; DWM can catch up
between them. `dispatch-busy`, `dispatch-stale` and `cancel` identify queue stops.

Only geometry, limits, internal modes/tokens/counters and fixed event names are
emitted. No HWND, PID/TID, titles, classes, contents, paths or input text is logged.
Tracing adds calls and can affect timing, hence the required trace-off comparison.
A trace diagnoses a stop; it does not make rollback transactional.
