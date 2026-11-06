/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALTSNAP_LINKED_GRID_H
#define ALTSNAP_LINKED_GRID_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t left, top, right, bottom;
} LGRect;

typedef struct {
    LGRect rect;
    int32_t min_width, min_height, max_width, max_height;
} LGWindow;

/* Quadrant of the active window; its opposite corner is the inner corner.
   Thus TL means dragging bottom-right, TR bottom-left, BL top-right, BR top-left. */
typedef enum { LG_TL, LG_TR, LG_BL, LG_BR } LGQuadrant;
typedef enum {
    LG_OK, LG_INVALID_ARGUMENT, LG_NOT_FOUND, LG_AMBIGUOUS, LG_INCOMPATIBLE_LIMITS
} LGStatus;

typedef struct {
    size_t indices[4];          /* Input indices in TL, TR, BL, BR order. */
    LGRect rects[4];
    int32_t x, y;              /* Clamped intersection. */
} LGResult;

/* Pure geometry: caller supplies one monitor's candidates in one coordinate
   space, with one entry per window. Exact abutment is required: no tolerance,
   gaps, overlaps, or normalization. All input rectangles must have positive area.
   Finds exactly one grid containing active in quadrant; unrelated windows do
   not participate. Duplicate grid participants or multiple complete grids are ambiguous,
   even if limits would exclude one. Only selected windows' limits are inspected:
   1 <= min <= max, inclusive; use INT32_MAX for an effectively unbounded max.
   Initial sizes need not satisfy limits if a feasible final intersection exists.
   Result is untouched on any failure. Input and output must not overlap.
   No allocations or platform APIs; O(count^3) time, O(1) auxiliary storage. */
LGStatus lg_resize(const LGWindow *windows, size_t count, size_t active,
                   LGQuadrant quadrant, int32_t x, int32_t y, LGResult *result);

/* Drag lifetime, independent of Win32. LEGACY is latched until next begin;
   BLOCKED consumes the drag without allowing a fallback after grid capture. */
typedef enum { LG_DRAG_LEGACY, LG_DRAG_ACTIVE, LG_DRAG_BLOCKED } LGDragMode;
typedef struct {
    LGDragMode mode;
    LGWindow windows[4];
    LGQuadrant quadrant;
    LGResult pending;
    int fullwin, dirty;
} LGDrag;
LGStatus lg_drag_begin(LGDrag *drag, const LGWindow *windows, size_t count,
                       size_t active, LGQuadrant quadrant, int enabled, int fullwin);
LGStatus lg_drag_update(LGDrag *drag, int32_t x, int32_t y);
void lg_drag_stop(LGDrag *drag);
#endif
