# Linked grid geometry tests

From the repository root, run:

```
make -C tests test
```

This builds `linked_grid.c`, the owner-queue policy in `linked_grid_queue.h`,
the recovery policy, `auto_tile_grid.c` and their framework-free C99 tests. It neither
builds nor starts AltSnap. Outputs go to ignored `work/linked-grid-tests/`.
It also runs the production AutoTileGrid adapter against deterministic Win32
stubs: eligible/other-monitor windows, repeated layouts, DPI and size refusals,
preflight changes, failed batches, +107 px application corrections, guarded
rollback and active/pending drag exclusion. This does not run Windows GUI code.
Warnings are errors. To check address and undefined behaviour sanitizers:

```
make -C tests test BUILD_DIR=../work/linked-grid-sanitized \
  CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer'
```

In environments running under ptrace, LeakSanitizer cannot run; prefix the
command with `ASAN_OPTIONS=detect_leaks=0` to retain ASan and UBSan checks.
The module does not allocate memory.

Cross-compile the module and test harness for Windows x64 with an installed
MinGW toolchain (compilation only; this does not run Windows tests):

```
make -C tests build CC=x86_64-w64-mingw32-gcc EXEEXT=.exe \
  BUILD_DIR=../work/linked-grid-win64
```

Tests cover all four active inner corners, both movement directions and each
axis alone, unchanged coordinates, all 720 permutations of six candidates,
complete versus incomplete competing grids, permutation-independent ambiguity,
missing windows, duplicate candidates, positive gaps, overlaps, broken alignment,
invalid arguments, min/max restrictions contributed by each quadrant, empty and
singleton feasible ranges, negative coordinates, 32-bit coordinate extremes,
and untouched output on failures. A deterministic 2,000-case exhaustive oracle
checks feasible final intersections independently of the interval algorithm.

# API and integration boundary

`lg_resize` takes a snapshot of candidates, an active index and its quadrant,
and the requested absolute intersection coordinates. The header defines the
contract and failure statuses. Results always use TL/TR/BL/BR order and retain
indices into the supplied snapshot. Ambiguity is decided before size limits;
constraints never choose between competing geometrical grids. The active window
and its quadrant anchor the search, so a separate grid elsewhere is irrelevant.

Detection requires exact, full-edge alignment and positive-area rectangles.
Gaps (including uniform gaps), overlaps between grid cells, staggered seams and
approximate alignment are rejected. Unrelated windows are ignored unless they
complete an alternative grid; arbitrary occlusion/visibility is not modeled.
Caller must filter to one monitor and provide unique window entries in one
coordinate space. Invalid rectangle geometry anywhere is an argument error;
limits of unrelated candidates are ignored. Limits are inclusive positive
sizes, with explicit maxima (`INT32_MAX` is the largest supported size).
Coordinates are signed 32-bit; intermediate limit arithmetic is signed 64-bit.
Initial dimensions may violate limits if the final grid can satisfy them.

The module uses O(n^3) worst-case detection time, constant storage, no heap and
no OS APIs. The hooks.c adapter selects eligible
windows from the same monitor and maps HWNDs, frame bounds and track limits to
the pure geometry API. Tolerance and gaps remain unsupported. The Win32 adapter is documented in [LINKED_GRID_INTEGRATION.md](LINKED_GRID_INTEGRATION.md);
portable tests do not exercise Windows GUI behavior. Old compiler support before C99 is not established.

AutoTileGrid usage, conservative eligibility, automated coverage, and the pending
Windows GUI checklist are documented in [AUTO_TILE_GRID.md](../AUTO_TILE_GRID.md).

The test target also runs `linked_grid_win_test.c`, which includes the production
Win32 adapter against deterministic stubs for exact sizing, the supplied +107
right-edge correction and full rollback, refusal to accept the old size, external
changes between checks/participant restores, disappearance/identity replacement,
cancellation/stale generations, delayed DWM and late native changes. These are not Windows GUI tests.
See [fast movement diagnostics](LINKED_GRID_DIAGNOSTICS.md) for evidence, remaining
hypotheses and the opt-in `LinkedGridTrace=1` Windows test procedure.
