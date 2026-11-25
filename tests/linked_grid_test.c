#include "../linked_grid_queue.h"
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../linked_grid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static LGWindow window(int32_t l, int32_t t, int32_t r, int32_t b)
{
    LGWindow w = { { 0, 0, 0, 0 }, 1, 1, INT32_MAX, INT32_MAX };
    w.rect.left = l; w.rect.top = t; w.rect.right = r; w.rect.bottom = b;
    return w;
}

static void grid(LGWindow *w, int32_t l, int32_t t,
                 int32_t x, int32_t y, int32_t r, int32_t b)
{
    w[0] = window(l, t, x, y); w[1] = window(x, t, r, y);
    w[2] = window(l, y, x, b); w[3] = window(x, y, r, b);
}

static void geometry(const LGWindow *w, const LGResult *r, int32_t x, int32_t y)
{
    unsigned i;
    CHECK(r->x == x && r->y == y);
    for (i = 0; i < 4; ++i) {
        LGRect before = w[r->indices[i]].rect, after = r->rects[i];
        const LGWindow *limits = &w[r->indices[i]];
        int64_t width = (int64_t)after.right - after.left;
        int64_t height = (int64_t)after.bottom - after.top;
        CHECK(after.left == ((i & 1) ? x : before.left));
        CHECK(after.right == ((i & 1) ? before.right : x));
        CHECK(after.top == ((i & 2) ? y : before.top));
        CHECK(after.bottom == ((i & 2) ? before.bottom : y));
        CHECK(width >= limits->min_width && width <= limits->max_width);
        CHECK(height >= limits->min_height && height <= limits->max_height);
    }
    CHECK(r->rects[0].right == r->rects[1].left);
    CHECK(r->rects[2].right == r->rects[3].left);
    CHECK(r->rects[0].bottom == r->rects[2].top);
    CHECK(r->rects[1].bottom == r->rects[3].top);
}

static void failure(const LGWindow *w, size_t n, size_t active,
                    LGQuadrant q, LGStatus expected)
{
    LGResult result, saved;
    memset(&result, 0xA5, sizeof result);
    memcpy(&saved, &result, sizeof result);
    CHECK(lg_resize(w, n, active, q, 60, 70, &result) == expected);
    CHECK(memcmp(&result, &saved, sizeof result) == 0);
}

/* Every ordering of four grid windows plus two unrelated windows. */
static void permutations(LGWindow *w, unsigned *ids, unsigned at)
{
    unsigned i, q;
    if (at == 6) {
        static const int32_t targets[][2] = {
            { 60, 70 }, { 30, 20 }, { 60, 40 }, { 50, 70 }, { 50, 40 }
        };
        for (q = 0; q < 4; ++q) {
            size_t active = 0;
            unsigned move;
            while (ids[active] != q) ++active;
            for (move = 0; move < sizeof targets / sizeof targets[0]; ++move) {
                LGResult r;
                CHECK(lg_resize(w, 6, active, (LGQuadrant)q,
                                targets[move][0], targets[move][1], &r) == LG_OK);
                geometry(w, &r, targets[move][0], targets[move][1]);
                for (i = 0; i < 4; ++i) CHECK(ids[r.indices[i]] == i);
            }
        }
        return;
    }
    for (i = at; i < 6; ++i) {
        LGWindow tmp = w[at]; unsigned id = ids[at];
        w[at] = w[i]; w[i] = tmp; ids[at] = ids[i]; ids[i] = id;
        permutations(w, ids, at + 1);
        tmp = w[at]; w[at] = w[i]; w[i] = tmp;
        id = ids[at]; ids[at] = ids[i]; ids[i] = id;
    }
}

static void ambiguous_orders(LGWindow *w, unsigned *ids, unsigned at, unsigned n)
{
    unsigned i;
    if (at == n) {
        size_t active = 0;
        while (ids[active] != 0) ++active;
        failure(w, n, active, LG_TL, LG_AMBIGUOUS);
        return;
    }
    for (i = at; i < n; ++i) {
        LGWindow tmp = w[at]; unsigned id = ids[at];
        w[at] = w[i]; w[i] = tmp; ids[at] = ids[i]; ids[i] = id;
        ambiguous_orders(w, ids, at + 1, n);
        tmp = w[at]; w[at] = w[i]; w[i] = tmp;
        id = ids[at]; ids[at] = ids[i]; ids[i] = id;
    }
}

static void layouts(void)
{
    LGWindow w[7];
    LGResult r;
    unsigned i;
    grid(w, 0, 0, 50, 40, 100, 100);
    failure(NULL, 4, 0, LG_TL, LG_INVALID_ARGUMENT);
    failure(w, 0, 0, LG_TL, LG_INVALID_ARGUMENT);
    failure(w, 4, 4, LG_TL, LG_INVALID_ARGUMENT);
    failure(w, 4, 0, (LGQuadrant)-1, LG_INVALID_ARGUMENT);
    failure(w, 4, 0, (LGQuadrant)4, LG_INVALID_ARGUMENT);
    CHECK(lg_resize(w, 4, 0, LG_TL, 1, 1, NULL) == LG_INVALID_ARGUMENT);
    for (i = 0; i < 4; ++i) {
        LGWindow missing[3]; unsigned j, n = 0;
        for (j = 0; j < 4; ++j) if (j != i) missing[n++] = w[j];
        failure(missing, 3, 0, i == 0 ? LG_TR : LG_TL, LG_NOT_FOUND);
    }
    w[4] = window(50, 200, 100, 300); /* X match only */
    w[5] = window(200, 40, 300, 100); /* Y match only */
    { unsigned ids[] = { 0, 1, 2, 3, 4, 5 }; permutations(w, ids, 0); }
    /* A competing incomplete row must not make the complete grid ambiguous. */
    w[6] = window(50, 0, 120, 40);
    CHECK(lg_resize(w, 7, 0, LG_TL, 60, 70, &r) == LG_OK);
    geometry(w, &r, 60, 70);
    w[4] = w[0];
    failure(w, 5, 0, LG_TL, LG_AMBIGUOUS);
    w[4] = w[3];
    /* A duplicated diagonal remains ambiguous even with impossible limits. */
    w[4].min_width = 200;
    { unsigned ids[] = { 0, 1, 2, 3, 4 }; ambiguous_orders(w, ids, 0, 5); }
    w[4] = window(50, 0, 120, 40);
    w[5] = window(50, 40, 120, 100);
    { unsigned ids[] = { 0, 1, 2, 3, 4, 5 }; ambiguous_orders(w, ids, 0, 6); }
    for (i = 0; i < 4; ++i) {
        grid(w, 0, 0, 50, 40, 100, 100);
        ++w[i].rect.left; /* broken outer alignment or internal gap */
        failure(w, 4, 0, LG_TL, LG_NOT_FOUND);
    }
    grid(w, 0, 0, 50, 40, 100, 100);
    ++w[1].rect.left; ++w[3].rect.left; /* uniform vertical gap */
    failure(w, 4, 0, LG_TL, LG_NOT_FOUND);
    grid(w, 0, 0, 50, 40, 100, 100);
    --w[2].rect.top; --w[3].rect.top; /* horizontal overlap */
    failure(w, 4, 0, LG_TL, LG_NOT_FOUND);
    grid(w, 0, 0, 50, 40, 100, 100);
    ++w[2].rect.top; ++w[3].rect.top; /* horizontal gap */
    failure(w, 4, 0, LG_TL, LG_NOT_FOUND);
    w[0].rect.right = w[0].rect.left;
    failure(w, 4, 0, LG_TL, LG_INVALID_ARGUMENT);
    w[0].rect.right = -1;
    failure(w, 4, 0, LG_TL, LG_INVALID_ARGUMENT);
    grid(w, -200, -150, -100, -70, -10, -20);
    CHECK(lg_resize(w, 4, 3, LG_BR, -80, -60, &r) == LG_OK);
    geometry(w, &r, -80, -60);
    grid(w, INT32_MIN, INT32_MIN, -1, -1, INT32_MAX, INT32_MAX);
    CHECK(lg_resize(w, 4, 0, LG_TL, INT32_MIN, INT32_MAX, &r)
          == LG_INCOMPATIBLE_LIMITS); /* extent exceeds two INT32_MAX widths */
    grid(w, INT32_MIN, INT32_MIN, -1, -1, INT32_MAX - 1, INT32_MAX - 1);
    CHECK(lg_resize(w, 4, 0, LG_TL, INT32_MIN, INT32_MAX, &r) == LG_OK);
    geometry(w, &r, -1, -1);
}

static void limits(void)
{
    LGWindow w[4]; LGResult r;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        grid(w, 0, 0, 50, 40, 100, 100);
        w[i].min_width = 35; w[i].max_width = 65;
        w[i].min_height = 25; w[i].max_height = 75;
        CHECK(lg_resize(w, 4, 0, LG_TL, -100, 200, &r) == LG_OK);
        geometry(w, &r, 35, 75); /* every quadrant, including diagonal */
        CHECK(lg_resize(w, 4, 0, LG_TL, 200, -100, &r) == LG_OK);
        geometry(w, &r, 65, 25);
        w[i].max_width = 34;
        failure(w, 4, 0, LG_TL, LG_INCOMPATIBLE_LIMITS);
    }
    grid(w, 0, 0, 50, 40, 100, 100);
    w[0].min_width = 70; w[3].min_width = 40;
    failure(w, 4, 0, LG_TL, LG_INCOMPATIBLE_LIMITS);
    grid(w, 0, 0, 50, 40, 100, 100);
    w[0].max_height = 20; w[3].max_height = 30;
    failure(w, 4, 0, LG_TL, LG_INCOMPATIBLE_LIMITS);
    grid(w, 0, 0, 50, 40, 100, 100);
    w[3].min_width = w[3].max_width = 30;
    w[3].min_height = w[3].max_height = 20;
    CHECK(lg_resize(w, 4, 0, LG_TL, 1, 1, &r) == LG_OK);
    geometry(w, &r, 70, 80); /* singleton range, initially outside limits */
    w[0].min_height = 0;
    failure(w, 4, 0, LG_TL, LG_INCOMPATIBLE_LIMITS);
}

static unsigned rng = 620;
static int32_t random_size(void)
{
    rng = rng * 1664525u + 1013904223u;
    return (int32_t)(rng % 14u) + 1;
}

/* Independent exhaustive oracle: enumerate every final intersection and check
   actual dimensions rather than reproducing the interval calculation. */
static void oracle(void)
{
    unsigned trial, i;
    for (trial = 0; trial < 2000; ++trial) {
        LGWindow w[4]; LGResult r;
        int x, y, bestx = 0, besty = 0, distance = 100000;
        int32_t targetx = random_size() * 3 - 20;
        int32_t targety = random_size() * 3 - 20;
        grid(w, 0, 0, 6, 6, 12, 12);
        for (i = 0; i < 4; ++i) {
            w[i].min_width = random_size();
            w[i].max_width = w[i].min_width + random_size();
            w[i].min_height = random_size();
            w[i].max_height = w[i].min_height + random_size();
        }
        /* Include abundant feasible cases as well as incompatible ones. */
        if (trial % 2 == 0) for (i = 0; i < 4; ++i) {
            w[i].min_width = w[i].min_height = 1;
        }
        for (x = 1; x < 12; ++x) for (y = 1; y < 12; ++y) {
            int valid = 1, dist;
            for (i = 0; i < 4; ++i) {
                int width = (i & 1) ? 12 - x : x;
                int height = (i & 2) ? 12 - y : y;
                if (width < w[i].min_width || width > w[i].max_width
                    || height < w[i].min_height || height > w[i].max_height)
                    valid = 0;
            }
            dist = (x-targetx)*(x-targetx) + (y-targety)*(y-targety);
            if (valid && dist < distance) {
                distance = dist; bestx = x; besty = y;
            }
        }
        if (distance == 100000) failure(w, 4, 0, LG_TL, LG_INCOMPATIBLE_LIMITS);
        else {
            CHECK(lg_resize(w, 4, 0, LG_TL, targetx, targety, &r) == LG_OK);
            geometry(w, &r, bestx, besty);
        }
    }
}

static void drag_lifetime(void)
{
    LGWindow w[5], snapshot[4];
    LGDrag d;
    LGResult previous;
    unsigned q;
    for (q = 0; q < 4; ++q) {
        grid(w, -100, -100, 0, 0, 100, 100);
        CHECK(lg_drag_begin(&d, w, 4, q, (LGQuadrant)q, 0, 1) == LG_NOT_FOUND);
        CHECK(d.mode == LG_DRAG_LEGACY && !d.dirty);
        CHECK(lg_drag_update(&d, 10, 20) == LG_NOT_FOUND);
        CHECK(lg_drag_begin(&d, w, 4, q, (LGQuadrant)q, 1, q & 1) == LG_OK);
        CHECK(d.mode == LG_DRAG_ACTIVE && d.fullwin == (int)(q & 1) && !d.dirty);
        memcpy(snapshot, d.windows, sizeof snapshot);
        /* Later candidate/limit changes do not replace the captured snapshot. */
        w[0].rect.left = -999; w[3].min_width = 500;
        CHECK(lg_drag_update(&d, 15, -25) == LG_OK);
        CHECK(d.dirty && memcmp(snapshot, d.windows, sizeof snapshot) == 0);
        geometry(d.windows, &d.pending, 15, -25);
        previous = d.pending;
        /* Missing participant, cancellation or application failure use stop. */
        lg_drag_stop(&d);
        CHECK(d.mode == LG_DRAG_BLOCKED && !d.dirty);
        CHECK(lg_drag_update(&d, 30, 40) == LG_NOT_FOUND);
        CHECK(memcmp(&previous, &d.pending, sizeof previous) == 0);
        grid(w, 0, 0, 50, 50, 100, 100);
        CHECK(lg_drag_begin(&d, w, 3, 0, LG_TL, 1, 0) == LG_NOT_FOUND);
        CHECK(d.mode == LG_DRAG_LEGACY);
        w[4] = w[3];
        CHECK(lg_drag_begin(&d, w, 5, 0, LG_TL, 1, 0) == LG_AMBIGUOUS);
        CHECK(d.mode == LG_DRAG_LEGACY);
        w[0].min_width = 80; w[3].min_width = 80;
        CHECK(lg_drag_begin(&d, w, 4, 0, LG_TL, 1, 0) == LG_INCOMPATIBLE_LIMITS);
        CHECK(d.mode == LG_DRAG_BLOCKED && !d.dirty);
        w[3].min_width = 1; /* Feasible, but initial intersection invalid. */
        CHECK(lg_drag_begin(&d, w, 4, 0, LG_TL, 1, 0) == LG_INCOMPATIBLE_LIMITS);
        CHECK(d.mode == LG_DRAG_BLOCKED);
        grid(w, 0, 0, 50, 50, 100, 100);
        CHECK(lg_drag_begin(&d, w, 4, 0, LG_TL, 1, 0) == LG_OK);
        CHECK(d.mode == LG_DRAG_ACTIVE && !d.dirty && !d.fullwin);
    }
}

/* These use the production single-owner queue policy, not Windows GUI stubs.
   Calls between enter/current simulate reentrant hooks at deterministic points. */
static void queue_lifetime(void)
{
    unsigned fullwin, phase, enabled, repeat;
    LGWindow w[4];
    grid(w, 0, 0, 50, 50, 100, 100);
    for (enabled = 0; enabled < 2; ++enabled)
    for (fullwin = 0; fullwin < 2; ++fullwin)
    for (phase = 0; phase < 4; ++phase) {
        LGQueue queue = {1, 0, 0};
        LGDrag drag;
        uintptr_t old = queue.generation;
        unsigned applies = 0;
        lg_drag_begin(&drag, w, 4, 0, LG_TL, enabled, fullwin);
        if (phase == 0) { /* Escape before dequeue. */
            lg_queue_invalidate(&queue);
            lg_drag_stop(&drag);
        }
        if (lg_queue_enter(&queue, old)) {
            CHECK(!lg_queue_enter(&queue, old)); /* reentrant drag/probe */
            if (enabled) CHECK(lg_drag_update(&drag, 60, 65) == LG_OK);
            if (phase == 1) { /* pending calculated, before batch */
                lg_queue_invalidate(&queue);
                lg_drag_stop(&drag);
            }
            if (enabled && lg_queue_current(&queue, old) && drag.dirty) ++applies;
            if (phase == 2) { /* inside the already started batch */
                lg_queue_invalidate(&queue);
                lg_drag_stop(&drag);
                CHECK(!lg_queue_current(&queue, old)); /* no second/rollback batch */
            }
            queue.busy = 0;
        }
        if (phase == 3) { /* after dispatch, before queued mouse-up */
            lg_queue_invalidate(&queue);
            lg_drag_stop(&drag);
        }
        CHECK(applies == (enabled && phase >= 2));
        CHECK(!lg_queue_enter(&queue, old)); /* queued move AND finish */
        CHECK(!drag.dirty);
        CHECK(drag.mode == (enabled ? LG_DRAG_BLOCKED : LG_DRAG_LEGACY));
        for (repeat = 0; repeat < 3; ++repeat) { /* up/Escape/unhook remain idempotent */
            lg_drag_stop(&drag);
            CHECK(drag.mode == (enabled ? LG_DRAG_BLOCKED : LG_DRAG_LEGACY));
        }
        lg_queue_invalidate(&queue); /* new drag while old messages remain */
        lg_drag_begin(&drag, w, 4, 0, LG_TL, enabled, fullwin);
        CHECK(!lg_queue_enter(&queue, old));
        CHECK(lg_queue_enter(&queue, queue.generation));
        CHECK(drag.mode == (enabled ? LG_DRAG_ACTIVE : LG_DRAG_LEGACY));
        queue.busy = 0;
        queue.closed = 1; /* unload rejects queued messages */
        CHECK(!lg_queue_enter(&queue, queue.generation));
    }
    /* Ordered move -> mouse-up, with FullWin selecting the apply boundary. */
    for (fullwin = 0; fullwin < 2; ++fullwin) {
        LGQueue queue = {1, 0, 0};
        LGDrag drag;
        unsigned applies = 0;
        uintptr_t generation = queue.generation;
        CHECK(lg_drag_begin(&drag, w, 4, 0, LG_TL, 1, fullwin) == LG_OK);
        CHECK(lg_queue_enter(&queue, generation));
        CHECK(lg_drag_update(&drag, 60, 65) == LG_OK);
        if (drag.fullwin) { ++applies; drag.dirty = 0; }
        queue.busy = 0;
        CHECK(applies == fullwin);
        CHECK(lg_queue_enter(&queue, generation)); /* mouse-up */
        if (drag.dirty) ++applies;
        lg_drag_stop(&drag);
        lg_queue_invalidate(&queue); /* finish with Alt still held */
        queue.busy = 0;
        CHECK(applies == 1);
        CHECK(!lg_queue_enter(&queue, generation));
        CHECK(drag.mode == LG_DRAG_BLOCKED && !drag.dirty);
    }
    {
        LGQueue queue = {UINTPTR_MAX, 0, 0};
        lg_queue_invalidate(&queue);
        CHECK(queue.closed && !lg_queue_enter(&queue, UINTPTR_MAX));
    }
}

static void move_coalescing(void)
{
    LGMoveSlot slot = {0, 0, 0, 0};
    uintptr_t generation;
    int32_t x, y;
    unsigned i, wakes = 0;
    for (i = 0; i < 1000000; ++i)
        wakes += lg_move_offer(&slot, 1, (int32_t)i, -(int32_t)i);
    CHECK(wakes == 1);
    CHECK(lg_move_take(&slot, &generation, &x, &y));
    CHECK(generation == 1 && x == 999999 && y == -999999);
    CHECK(!lg_move_take(&slot, &generation, &x, &y));
    /* An old outstanding wake can deliver only the newly offered sample,
       never the old coordinates under the new token. */
    CHECK(lg_move_offer(&slot, 2, 10, 20));
    CHECK(!lg_move_offer(&slot, 3, 30, 40));
    CHECK(lg_move_take(&slot, &generation, &x, &y));
    CHECK(generation == 3 && x == 30 && y == 40);
    for (i = 4; i < 10004; ++i) {
        CHECK(lg_move_offer(&slot, i, 1, 2));
        CHECK(!lg_move_offer(&slot, i, 3, 4));
        /* Producer stops at mouse-up; wake precedes finish in the queue. */
        CHECK(lg_move_take(&slot, &generation, &x, &y));
        CHECK(generation == i && x == 3 && y == 4);
        CHECK(!slot.pending);
    }
}

int main(void)
{
    layouts(); limits(); oracle(); drag_lifetime(); queue_lifetime(); move_coalescing();
    printf("linked_grid: %u checks passed\n", checks);
    return 0;
}
