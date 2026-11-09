#include "../linked_grid_recovery.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d: %s\n", __FILE__,__LINE__,#x); exit(1); } } while (0)
static void preview_cases(void)
{
    LGDrag drag = {0};
    LGRect expected;
    unsigned quadrant, previous;
    for (previous = 0; previous < 4; ++previous)
    for (quadrant = 0; quadrant < 4; ++quadrant) {
        unsigned i;
        for (i = 0; i < 4; ++i)
            drag.pending.rects[i] = (LGRect){(int32_t)i*90, (int32_t)previous*10,
                                              (int32_t)i*90+80, (int32_t)previous*10+80};
        drag.quadrant = (LGQuadrant)quadrant;
        drag.fullwin = 0;
        expected = drag.pending.rects[quadrant];
        CHECK(lg_rect_equal(lg_preview_active(&drag), expected));
        drag.fullwin = 1;
        CHECK(lg_rect_equal(lg_preview_active(&drag), expected));
    }
}
static void recovery_cases(void)
{
    LGRect before[4], target[4], observed[4];
    unsigned i, failure, missing, refusal;
    LGRect witness[4];
    for (i = 0; i < 4; ++i) {
        before[i] = (LGRect){(int32_t)i*100, 0, (int32_t)i*100+100, 100};
        target[i] = before[i]; target[i].right += 10;
    }
    for (failure = 0; failure < 4; ++failure)
    for (missing = 0; missing < 4; ++missing)
    for (refusal = 0; refusal < 2; ++refusal) {
        unsigned mask;
        for (i = 0; i < 4; ++i) observed[i] = target[i];
        observed[failure] = before[failure];
        mask = lg_recovery_mask(before,target,observed,15u & ~(1u << missing));
        CHECK(!(mask & (1u << failure)));
        CHECK(!(mask & (1u << missing)));
        for (i = 0; i < 4; ++i) if (mask & (1u << i)) {
            /* Simulate failed common rollback, then individual refusal. */
            if (!refusal || i != (failure+1)%4) observed[i] = before[i];
        }
        for (i = 0; i < 4; ++i) if (mask & (1u << i))
            CHECK(lg_rect_equal(observed[i],before[i])
                  || (refusal && i == (failure+1)%4));
    }
    for (i = 0; i < 4; ++i) observed[i] = target[i];
    observed[2].left += 3; /* externally changed: not ours to restore */
    CHECK(lg_recovery_mask(before,target,observed,15) == 11);
    CHECK(lg_recovery_mask(before,target,observed,0) == 0);
    for (i = 0; i < 4; ++i) witness[i] = observed[i] = target[i];
    witness[3].right += 107; observed[3] = witness[3];
    CHECK(lg_recovery_mask(before,witness,observed,15) == 15);
    observed[3].right += 1; /* later external resize differs from the witness */
    CHECK(lg_recovery_mask(before,witness,observed,15) == 7);
    observed[3] = target[3]; /* even a move to the request is a later change */
    CHECK(lg_recovery_mask(before,witness,observed,15) == 7);
    observed[3] = witness[3];
    CHECK(lg_recovery_mask(before,witness,observed,7) == 7);
    observed[3] = before[3]; /* already restored: no write needed */
    CHECK(lg_recovery_mask(before,witness,observed,15) == 7);
    printf("linked_grid_recovery: %u checks passed\n",checks);
}
int main(void) { preview_cases(); recovery_cases(); return 0; }
