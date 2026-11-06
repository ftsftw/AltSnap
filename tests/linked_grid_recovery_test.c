#include "../linked_grid_recovery.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d: %s\n", __FILE__,__LINE__,#x); exit(1); } } while (0)
static void recovery_cases(void)
{
    LGRect before[4], target[4], observed[4];
    unsigned i, failure, missing, refusal;
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
    puts("linked_grid_recovery: checks passed");
}
int main(void) { recovery_cases(); return 0; }
