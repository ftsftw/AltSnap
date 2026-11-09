/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../auto_tile_grid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int rect_equal(LGRect a, LGRect b)
{
    return a.left == b.left && a.top == b.top
        && a.right == b.right && a.bottom == b.bottom;
}

static ATGWindow window_at(LGRect r, uintptr_t key)
{
    ATGWindow w = { { { 0, 0, 0, 0 }, 1, 1, INT32_MAX, INT32_MAX },
                    { 0, 0, 0, 0 }, 0 };
    w.window.rect = r;
    w.key = key;
    return w;
}

static void assert_plan(const ATGWindow *w, size_t n, LGRect work, const ATGPlan *p)
{
    size_t i, j;
    int64_t area = 0;
    for (i = 0; i < n; ++i) {
        LGRect r = p->visible[i], native = p->native[i];
        const ATGWindow *a;
        int64_t width = (int64_t)r.right - r.left, height = (int64_t)r.bottom - r.top;
        assert(p->indices[i] < n);
        a = &w[p->indices[i]];
        assert(r.left >= work.left && r.top >= work.top);
        assert(r.right <= work.right && r.bottom <= work.bottom);
        assert(width >= a->window.min_width && width <= a->window.max_width);
        assert(height >= a->window.min_height && height <= a->window.max_height);
        assert((int64_t)native.left + a->border.left == r.left);
        assert((int64_t)native.top + a->border.top == r.top);
        assert((int64_t)native.right - a->border.right == r.right);
        assert((int64_t)native.bottom - a->border.bottom == r.bottom);
        area += width * height;
        for (j = 0; j < i; ++j) {
            LGRect b = p->visible[j];
            assert(p->indices[i] != p->indices[j]);
            assert(r.right <= b.left || b.right <= r.left
                || r.bottom <= b.top || b.bottom <= r.top);
        }
    }
    assert(area == ((int64_t)work.right - work.left) * ((int64_t)work.bottom - work.top));
}

static void layouts(void)
{
    static const LGRect work[] = {
        { 0, 0, 1920, 1040 }, { -1921, -1081, 0, -40 },
        { -701, 311, 800, 1312 }, { 1, 3, 4, 6 }
    };
    size_t m, n, i;
    for (m = 0; m < sizeof work / sizeof work[0]; ++m) {
        for (n = 1; n <= 4; ++n) {
            ATGWindow w[4];
            ATGPlan p;
            LGRect mon = work[m];
            int32_t x = mon.left + (mon.right - mon.left) / 2;
            int32_t y = mon.top + (mon.bottom - mon.top) / 2;
            for (i = 0; i < n; ++i) w[i] = window_at(mon, 4 - i);
            assert(atg_plan(w, n, mon, &p));
            assert_plan(w, n, mon, &p);
            assert(p.visible[0].left == mon.left && p.visible[0].top == mon.top);
            assert(p.visible[n-1].right == mon.right && p.visible[n-1].bottom == mon.bottom);
            if (n == 1) assert(rect_equal(p.visible[0], mon));
            if (n >= 2) {
                assert(p.visible[0].right == x && p.visible[1].left == x);
                assert(p.visible[0].bottom == (n == 4 ? y : mon.bottom));
            }
            if (n == 3) {
                assert(p.visible[1].bottom == y && p.visible[2].top == y);
                assert(p.visible[2].left == x);
            }
            if (n == 4) {
                assert(p.visible[0].bottom == y && p.visible[1].bottom == y);
                assert(p.visible[2].top == y && p.visible[3].top == y);
                assert(p.visible[2].right == x && p.visible[3].left == x);
            }
        }
    }
}

static void failures(void)
{
    ATGWindow w[5];
    ATGPlan p, sentinel;
    LGRect work = { 0, 0, 1000, 800 };
    size_t i;
    for (i = 0; i < 5; ++i) w[i] = window_at(work, i + 1);
    memset(&sentinel, 0xa5, sizeof sentinel);
    p = sentinel;
#define REFUSE(expr) do { assert(!(expr)); assert(!memcmp(&p, &sentinel, sizeof p)); } while (0)
    REFUSE(atg_plan(w, 0, work, &p));
    REFUSE(atg_plan(w, 5, work, &p));
    REFUSE(atg_plan(NULL, 1, work, &p));
    assert(!atg_plan(w, 1, work, NULL));
    REFUSE(atg_plan(w, 1, (LGRect){ 0, 0, 0, 1 }, &p));
    REFUSE(atg_plan(w, 2, (LGRect){ 0, 0, 1, 4 }, &p));
    REFUSE(atg_plan(w, 3, (LGRect){ 0, 0, 4, 1 }, &p));
    REFUSE(atg_plan(w, 4, (LGRect){ 5, 0, 4, 1 }, &p));
    REFUSE(atg_plan(w, 1, (LGRect){ INT32_MIN, 0, INT32_MAX, 1 }, &p));
    w[0].window.min_width = 501;
    REFUSE(atg_plan(w, 4, work, &p));
    w[0].window.min_width = 1;
    w[0].window.max_height = 399;
    REFUSE(atg_plan(w, 4, work, &p));
    w[0].window.max_height = 0;
    REFUSE(atg_plan(w, 1, work, &p));
    w[0] = window_at(work, 1);
    w[0].window.min_height = 0;
    REFUSE(atg_plan(w, 1, work, &p));
    w[0] = window_at(work, 1);
    w[0].window.rect.right = 0;
    REFUSE(atg_plan(w, 1, work, &p));
    w[0] = window_at(work, 1);
    w[0].border.left = -1;
    REFUSE(atg_plan(w, 1, work, &p));
    w[0].border.left = 8;
    REFUSE(atg_plan(w, 1, (LGRect){ INT32_MIN, 0, INT32_MIN+100, 100 }, &p));
    w[0].border.left = 0;
    w[0].border.bottom = 8;
    REFUSE(atg_plan(w, 1, (LGRect){ 0, INT32_MAX-100, 100, INT32_MAX }, &p));
    w[0].border.bottom = INT32_MAX;
    REFUSE(atg_plan(w, 1, work, &p));
    w[0].border.bottom = 0;
    w[1].key = w[0].key;
    REFUSE(atg_plan(w, 2, work, &p));
#undef REFUSE
}

static void limits_and_borders(void)
{
    LGRect work = { -1001, -801, 0, 0 };
    ATGWindow w[4];
    ATGPlan p;
    size_t i;
    for (i = 0; i < 4; ++i) {
        w[i] = window_at(work, i + 1);
        w[i].border = (LGRect){ 7, 0, 8, 9 };
    }
    w[0].window.min_height = 801; /* Only the full-height left cell fits. */
    w[1].window.min_width = 501; /* Only the odd wider right half fits. */
    w[1].window.min_height = 401; /* Only the odd taller bottom cell fits. */
    w[2].window.max_height = 400;
    assert(atg_plan(w, 3, work, &p));
    assert(p.indices[0] == 0 && p.indices[1] == 2 && p.indices[2] == 1);
    assert_plan(w, 3, work, &p);
    for (i = 0; i < 4; ++i) {
        w[i].window.min_width = w[i].window.max_width = (i & 1) ? 501 : 500;
        w[i].window.min_height = w[i].window.max_height = (i & 2) ? 401 : 400;
    }
    assert(atg_plan(w, 4, work, &p));
    for (i = 0; i < 4; ++i) assert(p.indices[i] == i);
    assert_plan(w, 4, work, &p);
    work = (LGRect){ INT32_MAX-100, INT32_MIN, INT32_MAX, INT32_MIN+100 };
    w[0] = window_at((LGRect){ INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX }, 1);
    assert(atg_plan(w, 1, work, &p));
    assert_plan(w, 1, work, &p);
}

static void assignment_and_repeats(void)
{
    LGRect work = { -801, 0, 800, 901 };
    size_t n, i, trial;
    for (n = 1; n <= 4; ++n) {
        ATGWindow w[4], laid[4], permuted[4];
        ATGPlan first, again;
        for (i = 0; i < n; ++i) w[i] = window_at(work, 10 - i);
        assert(atg_plan(w, n, work, &first));
        for (i = 0; i < n; ++i) {
            /* All initial centers tie; smallest key wins first cell. */
            assert(w[first.indices[i]].key == 11 - n + i);
            laid[i] = w[first.indices[i]];
            laid[i].window.rect = first.visible[i];
        }
        /* Enumerate every ordering, including changed Z-order after a repeat. */
        for (trial = 0; trial < 256; ++trial) {
            size_t code = trial, used = 0;
            for (i = 0; i < n; ++i) {
                size_t idx = code % n;
                code /= n;
                while (used & (1u << idx)) idx = (idx + 1) % n;
                used |= 1u << idx;
                permuted[i] = laid[idx];
            }
            assert(atg_plan(permuted, n, work, &again));
            for (i = 0; i < n; ++i) {
                assert(rect_equal(again.visible[i], first.visible[i]));
                assert(permuted[again.indices[i]].key == laid[i].key);
            }
            /* Enumeration independence also before the first placement. */
            for (i = 0; i < n; ++i) permuted[i].window.rect = work;
            assert(atg_plan(permuted, n, work, &again));
            for (i = 0; i < n; ++i) assert(permuted[again.indices[i]].key == laid[i].key);
        }
    }
    {
        ATGWindow w[2] = { window_at((LGRect){ 700, 20, 790, 120 }, 1),
                           window_at((LGRect){ -790, 20, -700, 120 }, 2) };
        ATGPlan p;
        assert(atg_plan(w, 2, work, &p));
        assert(p.indices[0] == 1 && p.indices[1] == 0);
    }
}

static void linked_compatibility(void)
{
    LGRect work = { -1921, -1081, 0, -40 };
    ATGWindow w[4];
    ATGPlan p;
    LGWindow linked[4];
    size_t i;
    unsigned q;
    for (i = 0; i < 4; ++i) {
        w[i] = window_at(work, i + 1);
        w[i].border = (LGRect){ (int32_t)i + 3, 0, 8, (int32_t)i + 4 };
    }
    assert(atg_plan(w, 4, work, &p));
    for (i = 0; i < 4; ++i) {
        LGRect b = w[p.indices[i]].border, r = p.native[i];
        linked[i] = w[p.indices[i]].window;
        /* Model DWM's native -> visible mapping with each participant's border. */
        linked[i].rect = (LGRect){ r.left+b.left, r.top+b.top, r.right-b.right, r.bottom-b.bottom };
        assert(rect_equal(linked[i].rect, p.visible[i]));
    }
    for (q = LG_TL; q <= LG_BR; ++q) {
        LGResult result;
        LGDrag drag;
        assert(lg_resize(linked, 4, q, (LGQuadrant)q,
                         p.visible[0].right, p.visible[0].bottom, &result) == LG_OK);
        for (i = 0; i < 4; ++i) assert(rect_equal(result.rects[i], p.visible[i]));
        assert(lg_drag_begin(&drag, linked, 4, q, (LGQuadrant)q, 1, 1) == LG_OK);
        assert(drag.mode == LG_DRAG_ACTIVE);
        assert(lg_drag_update(&drag, p.visible[0].right+17, p.visible[0].bottom-13) == LG_OK);
        assert(drag.pending.x == p.visible[0].right+17);
        assert(drag.pending.y == p.visible[0].bottom-13);
    }
}

/* Independent n^n assignment oracle: validate feasibility and global cost,
   including cases in which only a non-nearest assignment satisfies limits. */
static uint32_t seed = 0x92ac14;
static uint32_t random_value(void) { seed = seed * 1664525u + 1013904223u; return seed; }
static int64_t abs64(int64_t n) { return n < 0 ? -n : n; }
static void oracle(void)
{
    unsigned trial;
    for (trial = 0; trial < 2000; ++trial) {
        size_t n = 1 + trial % 4, i, code, possibilities = 1;
        LGRect work = { -901, -701, 0, 0 }, cells[4];
        ATGWindow w[4], unconstrained[4];
        ATGPlan base, result;
        int64_t best = INT64_MAX, actual = 0;
        for (i = 0; i < n; ++i) {
            int32_t x = (int32_t)(random_value() % 1600) - 1000;
            int32_t y = (int32_t)(random_value() % 1200) - 800;
            w[i] = window_at((LGRect){ x, y, x+100, y+100 }, i+1);
            unconstrained[i] = w[i];
            w[i].window.min_width = 1 + (int32_t)(random_value() % 600);
            w[i].window.min_height = 1 + (int32_t)(random_value() % 500);
            w[i].window.max_width = w[i].window.min_width + (int32_t)(random_value() % 1000);
            w[i].window.max_height = w[i].window.min_height + (int32_t)(random_value() % 900);
            possibilities *= n;
        }
        assert(atg_plan(unconstrained, n, work, &base));
        for (i = 0; i < n; ++i) cells[i] = base.visible[i];
        for (code = 0; code < possibilities; ++code) {
            size_t k = code, mask = 0;
            int64_t cost = 0;
            for (i = 0; i < n; ++i) {
                size_t idx = k % n;
                LGRect c = cells[i], r = w[idx].window.rect;
                k /= n;
                if ((mask & (1u << idx)) || c.right-c.left < w[idx].window.min_width
                    || c.right-c.left > w[idx].window.max_width
                    || c.bottom-c.top < w[idx].window.min_height
                    || c.bottom-c.top > w[idx].window.max_height) break;
                mask |= 1u << idx;
                cost += abs64((int64_t)r.left+r.right-c.left-c.right)
                      + abs64((int64_t)r.top+r.bottom-c.top-c.bottom);
            }
            if (i == n && cost < best) best = cost;
        }
        assert(atg_plan(w, n, work, &result) == (best != INT64_MAX));
        if (best == INT64_MAX) continue;
        assert_plan(w, n, work, &result);
        for (i = 0; i < n; ++i) {
            LGRect c = result.visible[i], r = w[result.indices[i]].window.rect;
            actual += abs64((int64_t)r.left+r.right-c.left-c.right)
                    + abs64((int64_t)r.top+r.bottom-c.top-c.bottom);
        }
        assert(actual == best);
    }
}


static void expand_grid(ATGWindow w[4], LGRect work, int x, int y)
{
    size_t i;
    for (i=0;i<4;++i) {
        LGRect r = { i&1 ? x : work.left, i&2 ? y : work.top,
                     i&1 ? work.right : x, i&2 ? work.bottom : y };
        w[i]=window_at(r, i+1);
        w[i].border=(LGRect){7,0,8,9};
    }
}
static void adopt(ATGWindow w[4], const ATGPlan *p)
{
    size_t i;
    for(i=0;i<4;++i) w[p->indices[i]].window.rect=p->visible[i];
}
static void expansions(void)
{
    LGRect work={-901,37,1100,1038};
    unsigned q, axes, proportions;
    for(proportions=0;proportions<3;++proportions) for(q=0;q<4;++q) for(axes=1;axes<=3;++axes) {
        ATGWindow w[4]; ATGPlan p, again, uniform;
        int x=-200+(int)proportions*330, y=300+(int)proportions*120;
        size_t i;
        expand_grid(w,work,x,y);
        for(i=0;i<4;++i) {
            w[i].window.min_width=130+(int)i*17;
            w[i].window.min_height=140+(int)i*11;
            w[i].window.max_width=1700-(int)i*13;
            w[i].window.max_height=900-(int)i*7;
        }
        assert(atg_expand(w,4,q,work,axes,&p));
        assert_plan(w,4,work,&p);
        for(i=0;i<4;++i) assert(p.indices[i]==i);
        if (!(axes&1)) assert(p.visible[0].right==x);
        if (!(axes&2)) assert(p.visible[0].bottom==y);
        /* Extremum independently constrained by all four minima/maxima. */
        if (axes&1) assert(p.visible[0].right==(q&1 ? work.right-1661 : work.left+1674));
        if (axes&2) assert(p.visible[0].bottom==(q&2 ? work.top+151 : work.bottom-173));
        adopt(w,&p);
        assert(atg_expand(w,4,q,work,axes,&again));
        assert(!memcmp(&p,&again,sizeof p));
        /* Linked resize can still recognize and resize the result. */
        {
            LGWindow linked[4]; LGResult r;
            for(i=0;i<4;++i) linked[i]=w[i].window;
            assert(lg_resize(linked,4,q,(LGQuadrant)q,100,550,&r)==LG_OK);
        }
        assert(atg_plan(w,4,work,&uniform));
        for(i=0;i<4;++i) assert(uniform.indices[i]==i);
        assert(uniform.visible[0].right==99 && uniform.visible[0].bottom==537);
        /* Q/E/W combinations retain the same simultaneous extremum. */
        assert(atg_expand(w,4,q,work,3,&p)); adopt(w,&p);
        assert(atg_expand(w,4,q,work,1,&again)); adopt(w,&again);
        assert(atg_expand(w,4,q,work,2,&again));
        assert(!memcmp(&p,&again,sizeof p));
    }
    /* Brute-force independent feasible-split oracle, varying all limits. */
    for(proportions=0;proportions<256;++proportions) for(q=0;q<4;++q) for(axes=1;axes<=3;++axes) {
        LGRect small={-11,-13,9,5};
        ATGWindow w[4], perm[4]; ATGPlan p;
        int x,y,bx=0,by=0,found=0,expected,actual;
        size_t i, active=0;
        expand_grid(w,small,-3,-5);
        for(i=0;i<4;++i) {
            w[i].window.min_width=1+(int)((proportions>>(i*2))&3)*2;
            w[i].window.min_height=1+(int)((proportions>>i)&3)*2;
            w[i].window.max_width=8+(int)((proportions>>(i+1))&3)*3;
            w[i].window.max_height=8+(int)((proportions>>(i+2))&3)*2;
        }
        expected=1;
        for(i=0;i<4;++i) {
            LGWindow a=w[i].window;
            int rw=a.rect.right-a.rect.left,rh=a.rect.bottom-a.rect.top;
            if(rw<a.min_width || rw>a.max_width || rh<a.min_height || rh>a.max_height) expected=0;
        }
        for(x=small.left+1;x<small.right;++x) for(y=small.top+1;y<small.bottom;++y) {
            int valid=1;
            if(!(axes&1) && x!=-3) continue;
            if(!(axes&2) && y!=-5) continue;
            for(i=0;i<4;++i) {
                int rw=i&1 ? small.right-x : x-small.left;
                int rh=i&2 ? small.bottom-y : y-small.top;
                LGWindow a=w[i].window;
                if(rw<a.min_width || rw>a.max_width || rh<a.min_height || rh>a.max_height) valid=0;
            }
            if(valid) {
                if(!found || (q&1 ? x<bx : x>bx)) bx=x;
                if(!found || (q&2 ? y<by : y>by)) by=y;
                found=1;
            }
        }
        for(i=0;i<4;++i) { perm[i]=w[3-i]; if(perm[i].key==q+1) active=i; }
        actual=atg_expand(perm,4,active,small,axes,&p);
        assert(actual==(expected && found));
        if(actual) { assert(p.visible[0].right==bx && p.visible[0].bottom==by); assert_plan(perm,4,small,&p); }
    }
}
static void expand_refusals(void)
{
    LGRect work={0,0,1200,960}; ATGWindow w[5]; ATGPlan p,sentinel;
    unsigned kind;
    memset(&sentinel,0xa5,sizeof sentinel);
    for(kind=0;kind<16;++kind) {
        expand_grid(w,work,600,480); w[4]=window_at(work,5); p=sentinel;
        switch(kind) {
        case 0: w[1].window.rect.left++; break;
        case 1: w[1].window.rect.left--; break;
        case 2: w[3].window.rect.bottom--; break;
        case 3: w[2].window.min_width=601; break;
        case 4: w[1].window.max_height=479; break;
        case 5: w[0].window.min_width=0; break;
        case 6: w[0].window.max_width=0; break;
        case 7: w[0].border.left=-1; break;
        case 8: w[0].border.right=INT32_MAX; break;
        case 9: w[1].key=w[0].key; break;
        case 10: w[2].window.rect=w[0].window.rect; break;
        case 11: w[3].window.rect.left=INT32_MIN; break;
        case 12: w[0].window.min_width=700; w[1].window.min_width=700; break;
        case 13: w[0].window.rect.right=w[0].window.rect.left; break;
        case 14: w[0].window.max_width=500; w[1].window.max_width=500; break;
        case 15: w[0].border.bottom=INT32_MAX; break;
        }
        assert(!atg_expand(w,4,0,work,3,&p)); assert(!memcmp(&p,&sentinel,sizeof p));
    }
    expand_grid(w,work,600,480);
    assert(!atg_expand(w,5,0,work,3,&p));
    assert(!atg_expand(w,3,0,work,3,&p));
    assert(!atg_expand(w,4,4,work,3,&p));
    assert(!atg_expand(w,4,0,work,0,&p));
    assert(!atg_expand(w,4,0,work,4,&p));
    assert(!atg_expand(NULL,4,0,work,3,&p));
    assert(!atg_expand(w,4,0,work,3,NULL));
    /* Coordinate bounds constrain the extremum before native conversion. */
    work=(LGRect){INT32_MIN+20,INT32_MIN+20,INT32_MIN+1020,INT32_MIN+820};
    expand_grid(w,work,work.left+500,work.top+400);
    w[1].border.left=200; w[3].border.left=200;
    assert(atg_expand(w,4,1,work,1,&p)); assert(p.visible[0].right==INT32_MIN+200);
    assert_plan(w,4,work,&p);
}

int main(void)
{
    expansions();
    expand_refusals();
    layouts();
    failures();
    limits_and_borders();
    assignment_and_repeats();
    linked_compatibility();
    oracle();
    puts("auto_tile_grid: layouts, limits, borders, repeats, refusals, linked compatibility and 2000 oracle cases passed");
    return 0;
}
