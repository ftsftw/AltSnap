/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALTSNAP_LINKED_GRID_RECOVERY_H
#define ALTSNAP_LINKED_GRID_RECOVERY_H
#include "linked_grid.h"
static int lg_rect_equal(LGRect a, LGRect b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
/* A changed third-party rectangle is not ours to restore. Identity is checked
   by the Win32 adapter before this policy is called. */
static unsigned lg_recovery_mask(const LGRect before[4], const LGRect target[4],
                                 const LGRect observed[4], unsigned identity_mask)
{
    unsigned i, mask = 0;
    for (i = 0; i < 4; ++i)
        if ((identity_mask & (1u << i))
            && lg_rect_equal(observed[i], target[i])
            && !lg_rect_equal(observed[i], before[i])) mask |= 1u << i;
    return mask;
}
#endif
