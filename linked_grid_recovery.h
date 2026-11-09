/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALTSNAP_LINKED_GRID_RECOVERY_H
#define ALTSNAP_LINKED_GRID_RECOVERY_H
#include "linked_grid.h"
/* The linked outline uses the already solved visible rectangle. */
static LGRect lg_preview_active(const LGDrag *drag)
{
    return drag->pending.rects[drag->quadrant];
}
static int lg_rect_equal(LGRect a, LGRect b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
/* Restore only an unchanged expected post-apply observation. The adapter
   supplies eligible witnesses and checks identity/generation; a later differing
   rectangle is not ours to restore. An observation alone cannot prove origin. */
static unsigned lg_recovery_mask(const LGRect before[4], const LGRect expected[4],
                                 const LGRect observed[4], unsigned identity_mask)
{
    unsigned i, mask = 0;
    for (i = 0; i < 4; ++i)
        if ((identity_mask & (1u << i))
            && lg_rect_equal(observed[i], expected[i])
            && !lg_rect_equal(observed[i], before[i])) mask |= 1u << i;
    return mask;
}
#endif
