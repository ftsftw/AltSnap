# Fast movement investigation (Windows 10)

## Confirmed Windows 10 evidence

The supplied real trace reproduces with MobaXterm while four Notepads and three
Notepads plus Windows Terminal work. Capture and earlier batches verified all
four windows. On the failing FullWin=1 jump:

- `end-defer a=1 b=15`: Win32 reported success for the four-window request.
- `verify-mismatch cell=3`: requested native RECT `1485,429,2567,1366`, observed
  `1485,429,2674,1366`. Only the right edge changed, by **+107 pixels**.
- DWM followed that actual size. This trace supplies no evidence of DWM lag.
- `recover-mask a=7 b=15`: the original adapter excluded the adjusted window,
  restored the other three, then blocked the drag, leaving a broken grid.

This confirms the exact-target-only recovery exclusion. It does **not** establish
which MobaXterm callback changed the size. An application can modify WINDOWPOS
in WM_WINDOWPOSCHANGING or perform another resize during/after positioning.
WM_GETMINMAXINFO supplies min/max track sizes, not arbitrary size rules,
character-cell increments, content-dependent limits or subsequent corrections.
API success is not exact-RECT acceptance or a transaction. No queue malfunction
was observed in this trace, so queue/drag processing is unchanged.

## Guarded size-correction recovery

Commit retains all four native RECT reads from the immediate post-apply
verification, before diagnostic geometry reads. On a successful batch a differing
RECT is an eligible size-correction witness only if its left/top still equal the
requested origin and its width/height are positive. A changed origin is skipped.
On batch API failure only exact requested observations are eligible, preserving
the conservative rule for ambiguous partial failures. Unreadable observations
are never eligible.

Recovery requires the same generation, HWND process/thread identity, visibility,
normal state, DPI and frame extents, plus a native RECT still equal to the
witness. It then rechecks each participant immediately before one SetWindowPos
back to the **last verified** grid. The recovery batch was removed: an earlier
window's positioning callback can move another participant after selection, and
that later window must be revalidated before it is touched. A participant already
at baseline needs no write. A later differing RECT is skipped, including a move
back to the originally requested RECT. Neither tracing nor a failed restoration
can replace the saved witness. Every restoration is read back; refusal/correction
is diagnosed, without retrying or adopting a new baseline. Failure blocks this
drag. Cancellation/stale dispatch stops further writes and cannot block a newer
operation on recovery return.

This is **guarded best effort**, not proof of ownership or full rollback. The
interval between EndDeferWindowPos and the first native observation cannot be
attributed atomically: an external resize with the same requested origin could
already be in that observation. There is also a check-to-SetWindowPos race and
same-thread HWND reuse cannot be ruled out by PID/TID. No sequence of ordinary
GetWindowRect/SetWindowPos calls eliminates these races. A later external change
that is actually observed is protected, but an ABA change back to the witness is
not detectable. A window can refuse the old size, disappear, change DPI/state or
fail the unchanged frame checks; the grid can then remain broken. Cancellation
cannot undo positioning already entered and adds no rollback-on-Escape.

If these residual races are unacceptable for a particular application, the
conservative alternative is `LinkedGridResize=0` for that session. FullWin=0
reduces application frequency to release but uses the same acceptance/recovery
boundary and is not a safety guarantee. Keep the unsupported application out of
the grid if necessary. There is no repeated negotiation, bypass of application
constraints, DWM workaround, new service, dependency or background process.

## Deterministic adapter evidence

`linked_grid_win_test.c` includes the production adapter against Win32 stubs.
The exact Windows trace coordinates and +107 correction now restore **all four**
participants when the baseline is accepted. The prior adapter at
`0ce205002b81463450ab489a50b25aebac1e17d8` leaves the adjusted participant behind.
Tests exercise all four active corners and every corrected/refusing participant,
including restoration after earlier successful commits.

| Simulated behavior | Result |
| --- | --- |
| Exact sizing, extreme jumps, repeated coalesced samples | Grid remains aligned; four min/max intervals honored |
| +107 right-edge correction, API success, DWM current | All four restore; drag blocked |
| Application refuses the old size despite API success | Other eligible windows restore; one attempt; mismatch diagnosed |
| Origin changes, or third RECT with batch API failure | Third RECT skipped; exact eligible survivors restore |
| External move before recovery selection or at final recheck | Changed participant skipped |
| Earlier restoration callback moves later participant (also to request) | Later participant skipped |
| HWND disappears or PID/TID differs after apply | That participant receives no restoration |
| Cancellation/stale generation during apply, verification or recovery | No subsequent positioning from the old operation |
| Native change during restoration readback | Mismatch diagnosed; no repeated restoration |
| Native change after a successful commit | Next validation stops drag |
| Exact sizes with stale DWM | Native grid intact; next border validation stops drag |
| Rejected size with stale DWM on survivors | Frame checks still reject recovery; grid can remain broken |

The final two cases remain deliberately unchanged; the supplied trace does not
justify relaxing DWM frame protection. These tests model behavior, **not a real
Windows GUI or MobaXterm callback**. The exact message that imposes the size rule
still needs application-side debugging if that distinction is required.

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
4. On each monitor separately, first repeat four Notepads and three Notepads
   plus Windows Terminal. Then recreate the failing mixed grid, preserving the
   application in every quadrant (especially MobaXterm/cell=3). Hold the Sticky
   Resize modifier before clicking. Try slow moves and sharp long horizontal, vertical
   and diagonal jumps from each inner corner. Include monitor 2/negative
   coordinates; record the scaling of both monitors.
5. On the +107 case expect `recover-witness a=15 b=1`, then (if state and RECTs
   stayed unchanged) `recover-mask a=15 b=15`, four `recover-individual` attempts,
   all four `recover-result` native RECTs at the preceding verified baseline,
   and `blocked`. A refusal should emit `recover-mismatch`; an intervening move
   should be skipped, not overwritten. Save capture through mouse release.
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
bitmask. Error records contain Win32 error codes. `recover-witness.a` is the
eligible post-apply observation mask, `.b` the forward batch API result.
`recover-mask.a` is restoration eligibility at recovery selection, `.b` valid
identity/state/frame checks; bits correspond to cells and explain skips.
A selected participant can still become ineligible before its individual attempt.
`recover-individual.a` is the SetWindowPos API result, not proof of acceptance;
`recover-mismatch` records failed baseline readback.

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
