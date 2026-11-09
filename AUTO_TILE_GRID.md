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
below and the explicit selection action have now been exercised by the user on
Windows 10. The reported cases establish explicit selection success, not safe
automatic main-window recognition. See the automatic-admission assessment below.

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
written. Without enabled profiles, ordinary AutoTileGrid performs no extra
diagnostic window/process queries or GetLastError/SetLastError calls with trace
off. Selected and registered-profile safety reads always run independently of
tracing. No diagnostic output is emitted with AutoTileTrace=0.

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
Ordinary AutoTileGrid keeps every WS_THICKFRAME, child, fullscreen and other
safety gate. Owned participation requires either an enabled registered profile
or the separate one-invocation Selected authorization; neither grants permission
to arbitrary owned windows.

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

Additional Windows GUI validation: test VS Code normal/maximized alone and
in 1–4-window mixes at equal privileges; repeat with Processes/Windows/Bottommost
blacklists (including executable-qualified entries and whitelist mode). Confirm
`process-limited-used` followed by the expected layout or blacklist refusal, and
repeat with trace off. Test inaccessible/elevated processes without elevating
AltSnap; an unreadable identity must remain excluded. Test an ApplicationFrameHost
app with and without an accessible child. Stub/CI success does not prove that a
real VS Code process permits the limited right or passes later DPI/size gates.

## MobaXterm: owner analysis and admission boundaries

Without an enabled registered profile, ordinary `AutoTileGrid` retains its
unconditional `skip-owner` refusal.
Only when `AutoTileTrace=1`,
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
In ordinary AutoTileGrid without enabled profiles and with tracing off, the
original single GW_OWNER eligibility read remains, and none of these extra
diagnostic queries, formatting, error-state operations or output runs. The explicit selection action performs
its safety checks regardless of tracing; tracing only controls its output.
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
| Modal-frame style or standard dialog class absent | Custom dialogs need neither; class rejection alone is insufficient |

The latest real Windows 10 trace is consistent across normal, button-maximized
and mouse-resized portable MobaXterm: stable same-process visible/enabled TOOLWINDOW
owner, no owner APPWINDOW/NOACTIVATE, non-POPUP enabled APPWINDOW candidate, and
root/rootowner/lastpopup all self. With a dialog open, both candidate and owner
become disabled; a `skip-child` record is also present. That child record cannot
identify which window produced it without a window ID, and no such ID is logged.
These are useful observations of this version, not a proof that the joint profile
uniquely identifies a main window. Registered profiles below express explicit
user trust in an exact build instead of claiming such a proof. A modeless/custom dialog can share the profile.
In particular, GA_ROOTOWNER=self despite a non-null GW_OWNER is an observed API
result, not grounds to discard the owner link.

No implicit owned-main classifier or built-in executable allowlist is implemented.
hooks.c's IsAltTabAble remains a presentation selector, too permissive for
automatic movement. Instead the separate action below requires an explicit
per-invocation user choice and uses the observed joint profile only as a narrow
safety guard. The registered-profile path additionally permits repeated ordinary
invocations after explicit one-time setup, as described below. Process lookup (including the VS Code fallback), all blacklists,
Linked Grid Resize, layout planning, min/max checks and recovery algorithms are
unchanged.

Portable packaging **could** indirectly affect topology if its launcher or UI
initialization creates a hidden helper owner or a different process relationship.
Portability itself is not a Win32 owner property and supplies no evidence for that
hypothesis. Static AltSnap inspection and stubs cannot establish whether portable
mode causes MobaXterm's owner. A controlled real Windows comparison of the same
MobaXterm version/configuration in portable and installed modes, normal/maximized
and with dialogs, is needed; no portable-specific cause or support is claimed.

### Experimental explicit selection: AutoTileGridSelected

Use a separate test copy and opt in by assigning this new shortcut in its existing
INI sections, then restarting AltSnap:

```ini
[KBShortcuts]
AutoTileGrid=839
AutoTileGridSelected=1863

[General]
AutoTileTrace=1
```

839 is Ctrl+Alt+G; 1863 is **Ctrl+Alt+Shift+G**. The shipped INI assigns neither
command. An absent or zero AutoTileGridSelected entry registers no shortcut.
Remove/zero that entry and restart to disable the experimental shortcut; leave
AutoTileTrace=0 after collecting results. No persistent permission, window property,
process allowlist or EXE-specific setting is stored. Explicit command-line action
invocation also counts as selecting the current foreground window, not a saved
selection. The ordinary shortcut does not inherit any permission from this action.

**Before each invocation, close the application's dialogs and explicitly focus
its intended main surface yourself.** The action selects exactly GetForegroundWindow
once, without promoting a child, owner, or popup. `UsePtWindow` does not change
selection. The cursor still chooses the monitor; the selected surface must be on
that monitor. The action requires an owned window; focusing an ordinary unowned
app is a refusal, not a fallback to ordinary tiling.

The selected candidate must pass all existing AutoTileGrid eligibility, process
identity, Processes/Windows/Bottommost blacklist, DWM, DPI, native frame, placement,
min/max, stable-capture and 1–4-participant checks. In addition:

- Pin candidate HWND, nonzero PID/TID and owner HWND/TID for this invocation;
  require the owner to have the same nonzero PID. Recheck both identities and the
  exact owner link around observation and at subsequent eligibility/witness checks.
- Require full caption plus thick frame; APPWINDOW; no CHILD, POPUP, TOOLWINDOW,
  NOACTIVATE or DLGMODALFRAME on the candidate. A failed/truncated class read or
  standard `#32770` dialog class refuses. Class text is never logged.
- Require the distinct owner to be visible, enabled, non-child TOOLWINDOW, without
  APPWINDOW, NOACTIVATE or DLGMODALFRAME. Unknown required owner style reads refuse.
  Candidate must also be visible/enabled. Root/rootowner/last-active-popup must
  each be exactly the selected candidate, matching the supplied trace.
- Require the selected candidate to remain foreground while checking. A read-only
  EnumWindows scan refuses **any other visible top-level surface in that process**
  except the pinned helper owner, including tool, disabled, modal/modeless and
  otherwise ineligible windows. An unknown visible window PID or enumeration
  failure also refuses. This is deliberately stricter than ordinary enumeration.
- Recheck identities, link, enabled/visible states, styles and root/popup relations
  after the scan. Repeat these guards during capture, identity/baseline checks,
  after normalization and immediately before creating the positioning batch.

Only this one HWND receives the owner exception. All other owned windows keep
`skip-owner`. If the selection fails any ordinary gate or is missing from a
capture, the whole selected command refuses; it does not silently tile only the
remaining three apps. Five eligible participants still refuse before any
normalization. Safety reads run even with AutoTileTrace=0. There are no new
messages, workers, elevation requests or process-query access rights.

| New fixed event | Meaning |
| --- | --- |
| `selected-owner-once count=1` | Initial explicit selection/owner guard passed; not yet an eligibility/layout success |
| `selected-owner-refused count=1` | Selected window/owner guard failed, including selection of a dialog/ordinary unowned app |
| `selected-not-captured count=0` | The selected HWND did not pass capture; no partial-set fallback |
| `selected-preflight-changed count=0` | Last baseline before batch creation changed; enter existing guarded recovery |

Existing scan/final/process/rollback events remain authoritative. New events use
only fixed codes and counts, with the same AltSnap.ATG DebugView filter.

**Limits:** explicit user intent replaces the impossible main/dialog inference;
the profile does not become a proven classifier. A custom dialog that perfectly
mimics this profile with no other visible same-process top-level surface can still
be explicitly misselected. Child-embedded/custom dialogs are not exhaustively
identified; other-process UI and invisible surfaces are not a complete dialog
model. Close dialogs yourself and do not select a dialog. Visible auxiliary
windows or another main window of the same process may cause a conservative
refusal. The observed profile may differ between MobaXterm releases/modes.

Checks are sequential, not atomic: same-process/thread HWND reuse, a link changing
away and back, or a new dialog/focus/style change after the last observation can
escape detection. Restricting authorization to one invocation avoids stale saved
handles but does not eliminate these Win32 races. The existing synchronous
min/max and placement calls can run application code. Process/UIPI restrictions
still apply; do not elevate AltSnap to force a pass. The existing rollback witness
also includes selection eligibility: if identity/owner/foreground/topology changes
after movement, it may refuse to restore the selected window while restoring safe
peers. Partial recovery and maximize activation remain the documented limitations;
no unsafe unconditional restoration was added.

### Exact Windows 10 check for the experimental command

1. Download the portable artifact for this commit, unpack both ZIP layers and
   start only that AltSnap copy without elevation. Use the INI above. Filter
   DebugView by `AltSnap.ATG`, clear it before each invocation and keep complete
   output through the final event. Record version/scaling/monitor/privilege and
   case labels separately, without adding identifiers to the trace.
2. Control: portable MobaXterm + exactly three ordinary apps (e.g. Chrome, VS Code
   without elevation and Windows Terminal), all on the cursor's monitor. Press
   **Ctrl+Alt+G**. Expect MobaXterm `skip-owner`, three candidates and 1+2 success.
   Those three windows may cover MobaXterm: exclusion reserves no work-area space.
3. Close every MobaXterm dialog, explicitly focus its main surface and keep the
   cursor on that monitor. Press **Ctrl+Alt+Shift+G**. Expect
   `selected-owner-once`, four captured candidates and `success count=4` in 2×2.
   Repeat to check the fixed point (`unchanged count=4` is valid on a repeat).
   If any safety gate refuses, retain its full output; do not weaken the guard.
4. Repeat step 3 with MobaXterm maximized by its button, then with its main window
   resized by mouse. Also test it alone (one-cell layout). No manual normalization
   is required. A successful normalization may log `normalized` before tiling.
5. Open a modal dialog, leave it focused and invoke the selected command: expect
   refusal and no window movement. If the disabled main surface can be foreground,
   test that too: it must refuse. Repeat with a modeless dialog, trying both its
   surface and the main surface while the dialog remains visible; neither the
   dialog nor any ordinary peers should move. A child-style selected dialog must
   not be promoted to its parent. An indistinguishable child-embedded custom UI
   remains a documented limit, not evidence of reliable dialog classification.
6. Close dialogs. Blacklist the selected app via Processes, Windows and Bottommost
   in turn: the selected command must move nothing. Remove each test blacklist
   before continuing. Add a fourth ordinary app (five total eligible participants)
   and invoke: expect `too-many`, with no movement or normalization.
7. Repeat the successful four-window case with AutoTileTrace=0 after restart;
   expect the same behavior and no AltSnap.ATG output. Press the ordinary shortcut
   afterwards: MobaXterm must again be excluded. Removing the experimental shortcut
   must leave all ordinary behavior unchanged.
8. With dialogs closed, repeat the prior successful Linked Grid Resize/minimum-size
   and guarded-rollback test on a deliberately prepared layout. The Linked Grid
   implementation is unchanged. Record any selected-command application size
   correction, refusal, partial recovery, focus or Z-order change separately.

The user has now confirmed on real Windows 10: AutoTileGridSelected succeeds
for portable MobaXterm alone (`success count=1`) and with three ordinary apps
(`success count=4`); button-maximized MobaXterm normalizes and tiles; selecting the
tested modal dialog gives `selected-owner-refused`; five eligible windows give
`too-many` without movement; trace off preserves those observed results.
Linked Grid Resize with MobaXterm and minimum-size restrictions also worked.
This is a bounded GUI report for explicit selection. It does not establish every
modeless/custom dialog, release, theme or portable/installed mode. No new GUI run
was performed in the Linux environment.

### Registered owned-window compatibility profiles (experimental)

This implementation records **explicit user consent to an exact application
build**. It does not infer a main-window role from APPWINDOW, a class, EXE name,
owner or enabled bit. Nothing is automatically enrolled, and no MobaXterm name
is built into the implementation. Portable executables may use arbitrary names.
AutoTileGridSelected keeps its existing foreground-based behavior as a fallback.
The supplied Windows 10 report confirms Selected only; the new registered-profile
path still requires the real GUI test below.

#### One-time setup through the Windows 10 interface

1. Use the portable ZIP for this commit, unpacking both ZIP layers. Exit the old
   AltSnap copy and start this copy without elevation. Preserve your INI backup.
   Keep `[KBShortcuts] AutoTileGrid=839` for the ordinary Ctrl+Alt+G command.
2. Open AltSnap settings, **Blacklist**, then use the existing **Identify window**
   tool on the intended main window, with its dialogs closed. Its displayed title,
   class and executable let you check the target locally. The tool selects
   GA_ROOT, as before; point at the intended main surface, not an embedded dialog.
3. Click **Owned profiles...** beside Identify window. This small experimental
   manager has eight local slots. Choose an empty slot and click
   **Register identified**. Confirm the explicit-trust explanation. Registration
   always saves **off**; replacing a slot also revokes its previous enabled state.
   The temporary Identify selection is discarded after registration or closing
   the manager; use Identify again to register another window.
4. Check the saved local path, SHA-256 and candidate/owner classes in the manager.
   Select **Enable / Disable** and confirm trust for this build. The image hash
   is checked again before enabling. Changes save immediately and reload AltSnap
   settings; Apply is not required for profiles. The new manager uses English
   labels; the existing settings/shortcut interface and language files are unchanged.
5. Close settings, keep MobaXterm visible with dialogs closed, and focus any of
   Chrome/VS Code/Terminal. Put the cursor on the intended monitor and press the
   **ordinary Ctrl+Alt+G**. No Selected shortcut or repeated permission is needed.
   The consent survives AltSnap/application restarts for the same path/build.
6. To stop trusting the build, reopen the manager and toggle it **off**, or
   **Delete** the record. To update the EXE or move/rename it, identify the new
   main surface and register again; the replacement starts off. Never enable an
   old fingerprint merely by editing its hash. Keep your profile INI private.

Only the local AltSnap INI is changed. There is no registry permission, window
property, saved HWND/PID/TID, process allowlist by basename, background worker,
new hotkey or automatic foreground activation. The confirmation is application
consent, not a request for Windows administrator rights. Do not elevate to force
an otherwise refused window.

#### Persisted contract and operation witnesses

Records use `[AutoTileOwned1]` through `[AutoTileOwned8]` in the active INI, with
`Enabled`, `Schema`, `Path`, `SHA256`, `Class`, `OwnerClass`, `Topology`, `Style`,
`ExStyle`, `OwnerStyle` and `OwnerExStyle`. These are local settings, never trace
fields. Schema 1 names the supported `caption-app-same-tool-self-v1` topology.
The serializer writes Enabled=0 and an invalid schema before replacing fields,
then commits the schema and checks the round trip. Interrupted/failed registration
cannot enable a partial record. Unknown/malformed enabled records cancel ordinary
tilings until disabled/deleted; malformed disabled records grant nothing.
The shipped portable INI contains no registrations and enables none by default.

The saved path is the complete process image path returned by the existing
validated process-query helper. SHA-256 is computed by Windows CryptoAPI over the
actual local file bytes. Local fixed/removable/RAM drives are accepted; UNC/device
paths, reparse files, noncanonical aliases, missing/empty/unreadable images and
images above **128 MiB** refuse. Classes must fit the supported **62 TCHAR** limit
without truncation. The existing MAX_PATH and build TCHAR/code-page limits still apply; unsupported
paths/classes refuse rather than being accepted after truncation. A build update
changes the hash even if its filename/version string is unchanged.

At each ordinary invocation with at least one enabled profile, an operation-local
copy of the profile records is made. Candidate/owner HWND, PID/TID and file handles
exist only for that call. Each matching image is hashed once and held open with
read sharing only, without write/delete sharing, until success/refusal/recovery
returns. Volume/file index, size and write time are checked through that handle
while the operation runs. There is no digest cache or saved process identity
between invocations. All paths close the handles, including partial normalization
and failure recovery. Local disk hashing adds work to this opt-in path; native
file I/O has no application-message timeout and slow storage can delay the call.

The shared owned-topology guard is used by both Selected and registered profiles.
Foreground equality is required only by Selected. Profile participation also
requires exact saved candidate/owner classes, matching image path/hash and masked
styles. The style mask records CHILD/POPUP/CAPTION/THICKFRAME and the extended
APPWINDOW/TOOLWINDOW/NOACTIVATE/DLGMODALFRAME bits. Transient maximize/visible bits
are not persisted, allowing normal, button-maximized and mouse-resized states.
The joint guard still requires:

- A visible, enabled, captioned, resizable, non-child/non-POPUP APPWINDOW candidate;
  no TOOLWINDOW, NOACTIVATE, DLGMODALFRAME or standard dialog class.
- A distinct, stable, same-process, visible/enabled TOOLWINDOW owner, without
  APPWINDOW/NOACTIVATE/DLGMODALFRAME and with the enrolled owner class/style mask.
- GA_ROOT, GA_ROOTOWNER and last-active-popup equal to the candidate, as in the
  supplied MobaXterm trace. Classes/styles and both identities are rechecked
  around observation. Unknown/truncated required reads are refusals.
- No additional visible top-level surface of the candidate's process except
  that exact owner. This includes tool, disabled, modal/modeless/custom and
  otherwise ineligible windows, even on another monitor.

On the selected monitor, each enabled profile must have **one unique main-class
match** when that image is visible. A changed main class, only a visible helper/
dialog, duplicate enabled registrations or multiple instances matching the same
profile on that monitor cancel the whole layout. Different registered applications
can participate together. Independent instances on other monitors are not selected;
a same-process visible extra surface is still a conservative refusal. An owned
window with the enrolled main class but a different image path is suspicious and
also cancels instead of inheriting consent. Other unrelated owned windows retain
their ordinary exclusion. With profiles enabled, an unreadable visible process
identity/class during the monitor scan conservatively cancels; this can refuse an
otherwise unrelated protected/transient app and never increases access rights.

Profile discovery and witnesses are rechecked before/after capture, through
baseline checks, after message-sending min/max queries and normalization, before
the final batch, and in observation/recovery eligibility. A matched profiled window
that fails an ordinary gate (including blacklist, minimized/fullscreen/DPI/min-max
or DWM checks), or disappears from capture, cancels the **whole** operation. It is
not silently omitted so that peers can cover its dialog. Ordinary peers retain
their prior exclusions, but five eligible participants still refuse before any
normalization. Drag/busy refusal, 1–4 planning, double capture, the VS Code process
fallback and Linked Grid Resize algorithms stay unchanged.

A detected identity/owner/class/build loss latches refusal for profiled targets
in that operation. Existing guarded rollback may still restore safe unrelated
peers, but must not force a now-invalid trusted HWND back. As with Selected, this
can leave partial recovery after application callbacks or an external change.
The existing maximize recovery can activate/raise a window; no stronger atomicity
or focus guarantee is claimed.

With **no enabled profiles**, the ordinary path does not scan profile candidates,
open/hash images, read profile keys per shortcut or query foreground. It keeps the
previous owner exclusion. Disabled profiles and failed registrations are not
permissions. Selected continues to work independently of enabled or invalid
profile records, with exactly its prior foreground policy.

#### Diagnostics and residual risk

All new trace output stays under **AltSnap.ATG** and uses fixed codes/counts:

| Event | Meaning |
| --- | --- |
| `profile-refused` | Invalid enabled record, discovery ambiguity, missing/changed build/classes/topology, suspect peer/dialog or failed required read |
| `profile-not-captured` | A pinned profiled target failed ordinary capture; no partial-set fallback |
| `profile-witness-lost` | The profiled HWND no longer satisfies its operation-local consent witness |
| `profile-allocation-failed` | The operation-local state could not be allocated |
| `profile-preflight-changed` | Last shared baseline before batch creation changed |

Existing scan, process, size, normalization, final and rollback records remain
relevant. A final success/unchanged count includes the registered participant.
No new path, hash, class, handle, PID/TID, geometry
or raw style is written to DebugView. Trace off removes diagnostic output while
retaining required profile safety checks.

**The synthetic role counterexample remains valid.** An indistinguishable custom
surface of the explicitly approved build, with the approved classes/topology and
no other visible same-process surface, can match while the real main is hidden.
Registration is deliberate trust in that build's UI, not proof that every match
is a main window. Stub tests retain both the no-consent refusal and this opt-in
residual case. Checks are sequential: same-process/thread HWND reuse, changes
away and back or a dialog appearing after the last observation can escape them.
The fingerprint identifies disk bytes, not running-memory attestation; restarting
after an application update and re-registering the new build remains necessary.
Other-process or child-embedded UI is not a complete dialog model. Keep profiles
only for builds and UI modes you have actually checked. No universal compatibility
or new real MobaXterm GUI success is claimed by this implementation.

#### Exact Windows 10 acceptance test

1. **No consent control:** before registration, ordinary AutoTileGrid must still
   exclude MobaXterm and tile three ordinary apps in 1+2, with trace on and off.
2. Register and enable as above. Test MobaXterm alone (count=1), with three ordinary
   apps (count=4, 2×2), normal/button-maximized/mouse-resized and repeated calls.
   Focus Chrome, VS Code and Terminal in turn; the same ordinary shortcut must
   include the trusted window without activating it deliberately. Repeat after
   restarting both AltSnap and MobaXterm to check persisted consent and fresh IDs.
3. Open the tested modal dialog; invoke while another app has focus. Expect a
   whole-operation refusal with no movement. Repeat with available modeless/custom
   dialogs, detached tabs, settings/session/password prompts and editor/tools;
   do not mark unavailable cases as tested. Include main-visible and main-hidden
   modes where possible, noting the indistinguishable-custom-dialog limit.
4. Add a fourth ordinary app (five eligible including MobaXterm): expect too-many
   without normalization/movement. Check Processes/Windows/Bottommost blacklists
   against the registered target: none of the peers should move. Check restrictive
   min/max/DPI refusals and the previously successful Linked Grid Resize limits.
5. Disable/delete the profile in the manager: ordinary AutoTileGrid returns to
   the previous exclusion immediately. Register again off, and confirm it still
   grants nothing until Enable. Test an updated/replaced EXE: the old hash must
   refuse the whole layout; re-registering the new build must start disabled.
6. Test duplicate records/multiple instances, unknown owners/classes, dialogs
   appearing during sizing and partial application corrections/recovery where
   practical. Keep full filtered trace and manual case/version notes separately.
   A guarded refusal to restore a changed identity is expected, not a reason to
   force rollback. Repeat successful controls with AutoTileTrace=0.

Portable tests/stubs cover the policy and persistence. CI additionally executes
real Windows ANSI/Unicode INI, SHA-256 known-vector, changed/empty/oversized image
and file-sharing-lock tests. These are **not a MobaXterm GUI test**. The user must
validate the new automatic registered-profile path on the actual Windows 10
build before treating it as confirmed compatibility.

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
configuration. The experimental selection shortcut is separate and disabled by default.
The profile manager is on the Blacklist page; shortcut UI is unchanged.
`UsePtWindow` does not affect this
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
tool/no-activate windows, owned windows/dialogs without registered consent, minimized, fullscreen,
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
error-state operations. Explicit-selection tests additionally cover every 1–4-window
normal/maximized combination, no retained authorization, modal/modeless/custom and
child dialog refusals, disabled/unknown owners, failed class/process reads,
blacklists, five participants, min/max/DPI refusal, owner/identity/foreground/style
races, ordinary recovery and guarded partial recovery after authorization loss.
The no-consent regressions retain 576 trace/show-state/focus/role/count cases and
the synthetic role collision. Profile tests add real serializer round trips,
partial-write failures, invalid enable/schema/hash/style inputs, disable/delete,
arbitrary executable/class names, absence of foreground, every 1–4 normal/maximized
combination, multiple profiles/instances, changed build/class/owner identities,
late modal/modeless dialogs, count/blacklist/size refusals and guarded recovery.
Native ANSI/Unicode Windows contract tests are compiled in the Linux job and run
on Windows in a dependent CI job. They check the actual CryptoAPI/INI/file boundary,
not GUI role recognition.
ASan/UBSan and MinGW x64 test compilation are supported
by tests/Makefile.

Full Windows x64 application build (no Windows GUI execution):

```sh
make -f MakefileX64 -j2 \
  CC=x86_64-w64-mingw32-gcc-win32 WR=x86_64-w64-mingw32-windres
```

The user-reported Windows 10 results above are established observations; the
the new registered-profile path and remaining GUI scenarios still require testing.
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
