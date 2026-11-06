/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linked_grid.h"

static int same_rect(LGRect a, LGRect b)
{
    return a.left == b.left && a.top == b.top
        && a.right == b.right && a.bottom == b.bottom;
}

static int64_t larger(int64_t a, int64_t b) { return a > b ? a : b; }
static int64_t smaller(int64_t a, int64_t b) { return a < b ? a : b; }

LGStatus lg_resize(const LGWindow *windows, size_t count, size_t active,
                   LGQuadrant quadrant, int32_t x, int32_t y, LGResult *result)
{
    LGResult next = { { 0 }, { { 0, 0, 0, 0 } }, 0, 0 };
    LGRect a;
    size_t h, v, d, i;
    unsigned q = (unsigned)quadrant;
    int found = 0;
    int64_t xmin, xmax, ymin, ymax;

    if (!windows || !result || active >= count || q > LG_BR)
        return LG_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        LGRect r = windows[i].rect;
        if (r.left >= r.right || r.top >= r.bottom)
            return LG_INVALID_ARGUMENT;
    }
    a = windows[active].rect;
    /* A horizontal and a vertical neighbour determine the entire diagonal.
       Full edge spans, rather than isolated X/Y matches, define neighbours. */
    for (h = 0; h < count; ++h) {
        LGRect b = windows[h].rect;
        if (b.top != a.top || b.bottom != a.bottom
            || ((q & 1) ? b.right != a.left : b.left != a.right))
            continue;
        for (v = 0; v < count; ++v) {
            LGRect c = windows[v].rect;
            LGRect diagonal;
            if (c.left != a.left || c.right != a.right
                || ((q & 2) ? c.bottom != a.top : c.top != a.bottom))
                continue;
            diagonal.left = b.left;
            diagonal.right = b.right;
            diagonal.top = c.top;
            diagonal.bottom = c.bottom;
            for (d = 0; d < count; ++d) {
                if (!same_rect(windows[d].rect, diagonal))
                    continue;
                if (found)
                    return LG_AMBIGUOUS;
                found = 1;
                next.indices[q] = active;
                next.indices[q ^ 1] = h;
                next.indices[q ^ 2] = v;
                next.indices[q ^ 3] = d;
            }
        }
    }
    if (!found)
        return LG_NOT_FOUND;

    for (i = 0; i < count; ++i)
        if (i != active && same_rect(windows[i].rect, a))
            return LG_AMBIGUOUS;

    for (i = 0; i < 4; ++i)
        next.rects[i] = windows[next.indices[i]].rect;
    xmin = next.rects[LG_TL].left;
    xmax = next.rects[LG_BR].right;
    ymin = next.rects[LG_TL].top;
    ymax = next.rects[LG_BR].bottom;
    for (i = 0; i < 4; ++i) {
        const LGWindow *w = &windows[next.indices[i]];
        LGRect r = w->rect;
        int64_t lo, hi;
        if (w->min_width < 1 || w->min_height < 1
            || w->max_width < w->min_width || w->max_height < w->min_height)
            return LG_INCOMPATIBLE_LIMITS;
        if (i & 1) {
            lo = (int64_t)r.right - w->max_width;
            hi = (int64_t)r.right - w->min_width;
        } else {
            lo = (int64_t)r.left + w->min_width;
            hi = (int64_t)r.left + w->max_width;
        }
        xmin = larger(xmin, lo);
        xmax = smaller(xmax, hi);
        if (i & 2) {
            lo = (int64_t)r.bottom - w->max_height;
            hi = (int64_t)r.bottom - w->min_height;
        } else {
            lo = (int64_t)r.top + w->min_height;
            hi = (int64_t)r.top + w->max_height;
        }
        ymin = larger(ymin, lo);
        ymax = smaller(ymax, hi);
    }
    if (xmin > xmax || ymin > ymax)
        return LG_INCOMPATIBLE_LIMITS;
    next.x = (int32_t)larger(xmin, smaller(xmax, x));
    next.y = (int32_t)larger(ymin, smaller(ymax, y));
    for (i = 0; i < 4; ++i) {
        if (i & 1) next.rects[i].left = next.x;
        else       next.rects[i].right = next.x;
        if (i & 2) next.rects[i].top = next.y;
        else       next.rects[i].bottom = next.y;
    }
    *result = next;
    return LG_OK;
}

LGStatus lg_drag_begin(LGDrag *drag, const LGWindow *windows, size_t count,
                       size_t active, LGQuadrant quadrant, int enabled, int fullwin)
{
    LGResult result;
    LGStatus status;
    LGRect a;
    unsigned i, q = (unsigned)quadrant;
    drag->mode = LG_DRAG_LEGACY;
    drag->dirty = 0;
    drag->fullwin = !!fullwin;
    if (!enabled || !windows || active >= count || q > LG_BR)
        return LG_NOT_FOUND;
    a = windows[active].rect;
    status = lg_resize(windows, count, active, quadrant,
                       (q & 1) ? a.left : a.right,
                       (q & 2) ? a.top : a.bottom, &result);
    if (status == LG_NOT_FOUND || status == LG_AMBIGUOUS)
        return status;
    drag->mode = LG_DRAG_BLOCKED;
    if (status != LG_OK) return status;
    /* Do not jump to a different intersection just by starting a drag. */
    if (result.x != ((q & 1) ? a.left : a.right)
        || result.y != ((q & 2) ? a.top : a.bottom))
        return LG_INCOMPATIBLE_LIMITS;
    for (i = 0; i < 4; ++i) drag->windows[i] = windows[result.indices[i]];
    drag->pending = result;
    drag->quadrant = quadrant;
    drag->mode = LG_DRAG_ACTIVE;
    return LG_OK;
}

LGStatus lg_drag_update(LGDrag *drag, int32_t x, int32_t y)
{
    LGStatus status;
    if (drag->mode != LG_DRAG_ACTIVE) return LG_NOT_FOUND;
    status = lg_resize(drag->windows, 4, drag->quadrant, drag->quadrant,
                       x, y, &drag->pending);
    if (status != LG_OK) lg_drag_stop(drag);
    else drag->dirty = 1;
    return status;
}

void lg_drag_stop(LGDrag *drag)
{
    drag->mode = LG_DRAG_BLOCKED;
    drag->dirty = 0;
}
