/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALTSNAP_LINKED_GRID_QUEUE_H
#define ALTSNAP_LINKED_GRID_QUEUE_H
#include <stdint.h>
/* Single owner only: the hook-installing thread, including reentrant callbacks.
   Never share this object with the worker. No waits/locks in Win32 callbacks. */
typedef struct { uintptr_t generation; int busy, closed; } LGQueue;
static int lg_queue_current(const LGQueue *q, uintptr_t generation)
{
    return !q->closed && generation == q->generation;
}
static void lg_queue_invalidate(LGQueue *q)
{
    /* Do not recycle tokens, even on 32-bit builds. */
    if (q->generation == UINTPTR_MAX) q->closed = 1;
    else ++q->generation;
}
static int lg_queue_enter(LGQueue *q, uintptr_t generation)
{
    if (q->busy || !lg_queue_current(q, generation)) return 0;
    q->busy = 1;
    return 1;
}
#endif
