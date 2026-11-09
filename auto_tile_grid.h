/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALTSNAP_AUTO_TILE_GRID_H
#define ALTSNAP_AUTO_TILE_GRID_H
#include "linked_grid.h"

typedef struct {
    LGWindow window;             /* DWM visible rectangle and visible limits. */
    LGRect border;               /* Nonnegative native minus visible insets. */
    uintptr_t key;               /* Unique stable identity, independent of Z-order. */
} ATGWindow;

typedef struct {
    size_t indices[4];           /* Input indices in cell order. */
    LGRect visible[4], native[4];
} ATGPlan;

/* Pure, bounded planner; no OS calls or allocations. Count must be 1..4.
   Cells: full; L/R; L/RT/RB; TL/TR/BL/BR. Odd spare pixels go right/bottom.
   Among feasible assignments, minimize summed Manhattan distance of doubled
   window/cell centers. Ties use ascending stable keys in cell order. Exact
   layouts are fixed points even if enumeration order changes. Limits are
   inclusive; refuse rather than clamp cells. Native sizes/coordinates must
   fit signed 32-bit Win32 arguments. Output is untouched on failure.
   Input and output must not overlap. */
int atg_plan(const ATGWindow *windows, size_t count, LGRect work, ATGPlan *plan);
#endif
