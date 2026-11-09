/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "auto_tile_grid.h"

static int64_t atg_abs(int64_t n) { return n < 0 ? -n : n; }

static int atg_native(LGRect cell, LGRect border, LGRect *native)
{
    int64_t l = (int64_t)cell.left - border.left;
    int64_t t = (int64_t)cell.top - border.top;
    int64_t r = (int64_t)cell.right + border.right;
    int64_t b = (int64_t)cell.bottom + border.bottom;
    if (l < INT32_MIN || t < INT32_MIN || r > INT32_MAX || b > INT32_MAX
        || r - l > INT32_MAX || b - t > INT32_MAX) return 0;
    *native = (LGRect){ (int32_t)l, (int32_t)t, (int32_t)r, (int32_t)b };
    return 1;
}

int atg_plan(const ATGWindow *windows, size_t count, LGRect work, ATGPlan *plan)
{
    ATGPlan best = { { 0 }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } };
    LGRect cells[4];
    size_t order[4], p[4], i, j;
    int64_t width = (int64_t)work.right - work.left;
    int64_t height = (int64_t)work.bottom - work.top;
    int64_t best_cost = INT64_MAX;
    int32_t x, y;
    int more = 1;
    if (!windows || !plan || count < 1 || count > 4
        || width < (count > 1 ? 2 : 1) || height < (count > 2 ? 2 : 1)
        || width > INT32_MAX || height > INT32_MAX) return 0;
    x = (int32_t)((int64_t)work.left + width / 2);
    y = (int32_t)((int64_t)work.top + height / 2);
    cells[0] = work;
    if (count > 1) {
        cells[0].right = x;
        cells[1] = (LGRect){ x, work.top, work.right, work.bottom };
    }
    if (count == 3) {
        cells[1].bottom = y;
        cells[2] = (LGRect){ x, y, work.right, work.bottom };
    } else if (count == 4) {
        cells[0].bottom = cells[1].bottom = y;
        cells[2] = (LGRect){ work.left, y, x, work.bottom };
        cells[3] = (LGRect){ x, y, work.right, work.bottom };
    }
    for (i = 0; i < count; ++i) {
        const LGWindow *w = &windows[i].window;
        LGRect b = windows[i].border;
        if (w->rect.left >= w->rect.right || w->rect.top >= w->rect.bottom
            || w->min_width < 1 || w->min_height < 1
            || w->max_width < w->min_width || w->max_height < w->min_height
            || b.left < 0 || b.top < 0 || b.right < 0 || b.bottom < 0) return 0;
        for (j = 0; j < i; ++j)
            if (windows[j].key == windows[i].key) return 0;
        order[i] = i;
        p[i] = i;
        /* Stable keys settle ties, never desktop enumeration/Z-order. */
        for (j = i; j && windows[order[j]].key < windows[order[j-1]].key; --j) {
            size_t tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp;
        }
    }
    /* At most 24 permutations, visited in key order. */
    while (more) {
        ATGPlan next = best;
        int64_t cost = 0;
        int feasible = 1;
        for (i = 0; i < count; ++i) {
            const ATGWindow *w = &windows[order[p[i]]];
            LGRect r = w->window.rect, cell = cells[i];
            int64_t cw = (int64_t)cell.right - cell.left;
            int64_t ch = (int64_t)cell.bottom - cell.top;
            if (cw < w->window.min_width || cw > w->window.max_width
                || ch < w->window.min_height || ch > w->window.max_height
                || !atg_native(cell, w->border, &next.native[i])) {
                feasible = 0;
                break;
            }
            next.indices[i] = order[p[i]];
            next.visible[i] = cell;
            cost += atg_abs((int64_t)r.left + r.right - cell.left - cell.right)
                  + atg_abs((int64_t)r.top + r.bottom - cell.top - cell.bottom);
        }
        if (feasible && cost < best_cost) { best = next; best_cost = cost; }
        i = count - 1;
        while (i && p[i-1] >= p[i]) --i;
        if (!i) more = 0;
        else {
            size_t tmp, k = count - 1;
            while (p[k] <= p[i-1]) --k;
            tmp = p[i-1]; p[i-1] = p[k]; p[k] = tmp;
            for (j = count - 1; i < j; ++i, --j) {
                tmp = p[i]; p[i] = p[j]; p[j] = tmp;
            }
        }
    }
    if (best_cost == INT64_MAX) return 0;
    *plan = best;
    return 1;
}

int atg_expand(const ATGWindow *windows, size_t count, size_t active,
               LGRect work, unsigned axes, ATGPlan *plan)
{
    LGWindow w[4];
    LGResult grid;
    ATGPlan next;
    LGRect a;
    size_t i, j;
    unsigned q;
    int32_t x, y;
    int64_t width = (int64_t)work.right - work.left;
    int64_t height = (int64_t)work.bottom - work.top;
    if (!windows || !plan || count != 4 || active >= count || !axes
        || (axes & ~ATG_EXPAND_BOTH) || width < 2 || height < 2
        || width > INT32_MAX || height > INT32_MAX) return 0;
    a = windows[active].window.rect;
    q = (a.left == work.left ? 0u : 1u) | (a.top == work.top ? 0u : 2u);
    for (i = 0; i < 4; ++i) {
        LGRect r = windows[i].window.rect, b = windows[i].border, native;
        int64_t rw = (int64_t)r.right - r.left, rh = (int64_t)r.bottom - r.top;
        int64_t maxw, maxh;
        w[i] = windows[i].window;
        if (r.left < work.left || r.top < work.top
            || r.right > work.right || r.bottom > work.bottom
            || w[i].min_width < 1 || w[i].min_height < 1
            || w[i].max_width < w[i].min_width || w[i].max_height < w[i].min_height
            || rw < w[i].min_width || rw > w[i].max_width
            || rh < w[i].min_height || rh > w[i].max_height
            || b.left < 0 || b.top < 0 || b.right < 0 || b.bottom < 0
            || !atg_native(r, b, &native)) return 0;
        for (j = 0; j < i; ++j) if (windows[j].key == windows[i].key) return 0;
        maxw = (int64_t)INT32_MAX - b.left - b.right;
        maxh = (int64_t)INT32_MAX - b.top - b.bottom;
        /* Also bound moving native coordinates, even near INT32 endpoints. */
        if (r.left == work.left) {
            int64_t bound = (int64_t)INT32_MAX - b.right - work.left;
            if (bound < maxw) maxw = bound;
        } else {
            int64_t bound = (int64_t)work.right - b.left - INT32_MIN;
            if (bound < maxw) maxw = bound;
        }
        if (r.top == work.top) {
            int64_t bound = (int64_t)INT32_MAX - b.bottom - work.top;
            if (bound < maxh) maxh = bound;
        } else {
            int64_t bound = (int64_t)work.bottom - b.top - INT32_MIN;
            if (bound < maxh) maxh = bound;
        }
        if (maxw < w[i].min_width || maxh < w[i].min_height) return 0;
        if (maxw < w[i].max_width) w[i].max_width = (int32_t)maxw;
        if (maxh < w[i].max_height) w[i].max_height = (int32_t)maxh;
    }
    x = (q & 1) ? a.left : a.right;
    y = (q & 2) ? a.top : a.bottom;
    if (lg_resize(w, 4, active, (LGQuadrant)q, x, y, &grid) != LG_OK
        || grid.x != x || grid.y != y
        || grid.rects[0].left != work.left || grid.rects[0].top != work.top
        || grid.rects[3].right != work.right || grid.rects[3].bottom != work.bottom) return 0;
    if (axes & ATG_EXPAND_HORIZONTAL) x = (q & 1) ? INT32_MIN : INT32_MAX;
    if (axes & ATG_EXPAND_VERTICAL) y = (q & 2) ? INT32_MIN : INT32_MAX;
    if (lg_resize(w, 4, active, (LGQuadrant)q, x, y, &grid) != LG_OK) return 0;
    for (i = 0; i < 4; ++i) {
        next.indices[i] = grid.indices[i];
        next.visible[i] = grid.rects[i];
        if (!atg_native(grid.rects[i], windows[grid.indices[i]].border, &next.native[i])) return 0;
    }
    *plan = next;
    return 1;
}
