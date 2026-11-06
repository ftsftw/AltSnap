# Linked Grid Resize integration

Opt in with `LinkedGridResize=1` under `[General]` in AltSnap.ini. Default is 0.
Use the existing Sticky Resize activation (StickyResize and Shift). Activation
is sampled when the resize action starts, so hold the modifier before clicking.
Releasing/pressing Shift later cannot switch a captured grid into ordinary
Sticky Resize. No windows are moved by loading settings or enabling AltSnap.

Only an inner corner of a strict 2x2 grid is eligible. MDI, maximized windows,
windows spanning monitor boundaries, mixed window DPI, missing DWM frame data,
gaps and overlaps are not supported. Candidate enumeration is limited to 128
eligible windows; overflow falls back without truncating the candidate list.
The grid may occupy any rectangular area wholly inside a monitor, including
monitors with negative coordinates. SnapGap is deliberately ignored for this
mode: actual visible DWM edges must abut.

`linked_grid_win.h` is a private implementation included by hooks.c, which also
includes the portable linked_grid.c implementation. This matches the existing
single-translation-unit build style; do not separately link linked_grid.o into
hooks.dll. Makefile prerequisites include the new files. The pure test harness
continues to compile linked_grid.c separately.

The adapter enumerates once at action start, anchors detection on the chosen
inner corner, snapshots the four handles, process/thread IDs, DWM borders, DPI,
rectangles and min/max track sizes. It requires a successful WM_GETMINMAXINFO
query for each participant (32 ms timeout). Limits apply to native window sizes;
invisible border extents are subtracted for the visible grid calculation and
added back before DeferWindowPos. All four use physical screen coordinates in
AltSnap's existing DPI-aware context. Differing effective window DPI is rejected
rather than guessed. There is no repeated desktop enumeration during mouse-move;
only the captured four rectangles/identities are validated and calculated.

No grid / ambiguous grid uses legacy Sticky Resize. A captured grid with unsafe
or incompatible limits freezes the drag. Initial sizes must already meet limits;
capture never relocates an intersection to repair an invalid initial layout.
The mode and FullWin choice are latched for the operation. Mouse delta from the
initial intersection bypasses legacy individual clamping, ResizeSnap and Aero
resize paths, preserving the outside contour. Ctrl/Shift changes do not alter
that calculation. FullWin=1 applies all four via one DeferWindowPos batch on
movement. FullWin=0 previews the active cell using the existing outline, stores
all four target rectangles, and applies the batch at release. The three other
cells do not have preview outlines. Legacy single-window finalization is bypassed.

Escape/unhook discards an outline-mode pending batch; in FullWin mode it leaves
the last applied grid, matching the existing cancellation semantics. A missing,
externally changed, minimized or DPI/frame-changed participant stops this mode
until the next action. Finish cannot apply a stale pending batch after stop.
All moves use NOACTIVATE/NOZORDER/NOOWNERZORDER. Existing active-window size/move
notifications are retained; the three companion windows receive normal Win32
position/size messages, without additional modal size/move notifications.

Win32 batches are not transactions against arbitrary application code. The
adapter checks the native rectangles after applying, attempts to restore the
last verified four rectangles if a batch fails or an application adjusts its
size, then freezes the drag. Rollback is best effort: a disappearing window or
an application refusing both the new and old size cannot be made atomic by
DeferWindowPos. Limits are a snapshot; runtime constraint changes trigger this
failure path. HWND process/thread checks reduce stale-handle risk but do not
prove identity if a handle is reused within the same thread.

## Automated checks

`make -C tests test` checks geometry and LGDrag lifecycle transitions: disabled,
missing, ambiguous, captured, repeated movement, snapshot isolation, latched
FullWin, invalid initial dimensions, incompatible limits, stopped/cancelled
pending work, and restarting a later action. These are portable policy tests;
they do not run EnumDesktopWindows, DWM or DeferWindowPos.

Full Windows x64 build used the existing MinGW-w64 GCC 14 toolchain, MakefileX64,
and the external `/workspace/altsnap-setup/compat.mk`, which adds `-lshell32`
(required by the pre-existing ShellExecuteW use in hooks.dll). Outputs remain
in ignored work/build-x64, with no installer or release. Example command after
placing sources in an isolated build directory:

```
make -f MakefileX64 -f /workspace/altsnap-setup/compat.mk \
  CC=/workspace/altsnap-setup/toolchain/usr/bin/x86_64-w64-mingw32-gcc-win32 \
  WR=/workspace/altsnap-setup/toolchain/usr/bin/x86_64-w64-mingw32-windres
```

## Windows 10 manual verification still required

No GUI execution has been performed. Check all four inner corners, each axis,
min/max limits (especially diagonal constraints), escape/release, FullWin 0/1,
and unchanged legacy behavior with LinkedGridResize=0. Check Shift/key changes
during drag, threshold initiation, reload/unhook and closing a participant.
Test negative-coordinate monitors, 100/125/150/200 percent scaling, DWM invisible
frames and legacy DPI-unaware apps; unsupported mixed DPI must refuse capture.
Check applications adjusting sizes (terminals), live WM_GETMINMAXINFO changes,
hung/elevated windows and failed batch/rollback. Verify activation does not move
windows, external moves stop capture, and no unrelated fifth window moves.
Stress rapid successive drags/Escape against the existing hook/worker queue.

## Cancellation and queue ownership (audit fixes)

With LinkedGridResize=1, movement, finalization and move/resize combo work are
posted to a private message window on the hook-installing thread. The existing
worker remains in use for legacy (option off) movement and other AltSnap jobs.
There is no cross-thread access to linked state and no waiting for the worker
from a hook. This deliberately moves enabled-mode processing to the owner thread;
hook/owner latency and preview optimization are separate future work.

Each enabled-mode message carries a non-reused generation. New actions, Escape,
unhook, combo locking, completion (even while Alt remains down), and Unload
invalidate it. Probes do not advance generations, and probes/reentrant starts
cannot overwrite a captured operation. Generation exhaustion fails closed.
Load after Unload preserves the sequence, including when the DLL stays loaded.
Pending heap-backed move messages are freed on dequeue, post failure or unload.
Legacy worker messages and finalizers also carry an atomic generation, but the
option-off path has no linked admission guard and retains the existing worker.
Its already executing legacy movement is not made transactional by these tokens.

Win32 calls may reenter hooks. A busy owner rejects replacement until its callback
unwinds; cancellation can still invalidate the token immediately. Commit checks
the token after validation and before starting ApplyLinkedRects. There are no
callbacks/position changes in BeginDeferWindowPos/DeferWindowPos; EndDeferWindowPos
is the batch application boundary. An already entered batch may finish after
Escape. On return, cancellation prevents further application (including the
existing best-effort recovery batch). No rollback-on-Escape is added. A nested
modal loop dispatching another movement/finalizer while busy cancels the operation
instead of recursively applying or leaving the finish flag stuck.

Mouse-up is queued after preceding mouse samples. Those samples are processed
before finalization even when the finish flag is set. FullWin=0 still applies
only at finalization; FullWin=1 still applies on movement. Stop preserves LEGACY;
only ACTIVE becomes BLOCKED. Capture failure explicitly selects BLOCKED so the
legacy fix cannot accidentally enable single-cell fallback for a captured grid.

The portable tests use the production queue policy with deterministic interruption
points: before dequeue, after pending calculation, inside an already started
batch, after dispatch, old move/finish tokens after a new drag, repeated stop,
option off/on, FullWin off/on, busy reentry and generation exhaustion. They test
policy and geometry, **not real Win32 message dispatch, hooks or GUI behavior**.
Windows GUI checks above remain required, including rapid drag/Escape/reload and
notification ordering. The existing general-purpose worker shutdown timeout and
legacy in-flight shared-state behavior are outside this linked-state fix.
