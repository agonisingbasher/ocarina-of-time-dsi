#include "sched.h"

#include <string.h>

#define TICKS_PER_VBLANKS 3
#define VBLANKS_PER_SECOND 60

static volatile u32 sVBlankCount;

static void onVBlank(void)
{
    sVBlankCount++;
}

void schedInit(Sched *s)
{
    memset(s, 0, sizeof(*s));

    irqSet(IRQ_VBLANK, onVBlank);
    irqEnable(IRQ_VBLANK);

    s->lastVBlank = sVBlankCount;
    s->secondStart = s->lastVBlank;
}

u32 schedFrame(Sched *s)
{
    u32 now = sVBlankCount;
    u32 elapsed = now - s->lastVBlank;
    u32 ticks = elapsed / TICKS_PER_VBLANKS;

    s->lastVBlank = now - (elapsed % TICKS_PER_VBLANKS);
    s->ticks += ticks;
    s->ticksInSec += ticks;

    if (now - s->secondStart >= VBLANKS_PER_SECOND) {
        s->tickRate = (u32)(((u64)s->ticksInSec * VBLANKS_PER_SECOND) /
                            (now - s->secondStart));
        s->ticksInSec = 0;
        s->secondStart = now;
    }

    return ticks;
}
