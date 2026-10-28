#ifndef OCARINA_NDS_SCHED_H
#define OCARINA_NDS_SCHED_H

#include <nds.h>

/* OoT runs its game logic at 20 Hz, i.e. one tick every three 60 Hz VBlanks.
 * ticks are derived from a real VBlank counter, so a frame that overruns
 * still yields the correct number of logic steps instead of drifting. */
typedef struct {
    u32 lastVBlank;
    u32 secondStart;
    u32 ticks;
    u32 ticksInSec;
    u32 tickRate;
} Sched;

void schedInit(Sched *s);

/* Call once per rendered frame, right after swiWaitForVBlank().
 * Returns how many 20 Hz ticks are due (0, 1 or more). */
u32 schedFrame(Sched *s);

#endif
