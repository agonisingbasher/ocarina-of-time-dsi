#include "perf.h"

#include <string.h>
#include "timebase.h"

void perfInit(Perf *p)
{
    memset(p, 0, sizeof(*p));
    p->lastEndTicks = timeTicks();
    p->secondStartTicks = p->lastEndTicks;
}

/* Measured between successive perfEnd calls, so it covers the whole loop
 * iteration including console text updates and the VBlank wait. */
void perfEnd(Perf *p)
{
    u32 now = timeTicks();
    u32 us = timeTicksToUs(now - p->lastEndTicks);

    p->lastEndTicks = now;
    p->frameUs = us;
    if (us > p->worstUs) {
        p->worstUs = us;
    }

    p->textAccumUs += us;
    p->framesInSec++;

    if (now - p->secondStartTicks >= BUS_CLOCK) {
        p->fps = p->framesInSec;
        p->framesInSec = 0;
        p->secondStartTicks = now;
    }
}

bool perfConsume(u64 *accumUs, u32 hz)
{
    u64 period = 1000000ull / hz;

    if (*accumUs < period) {
        return false;
    }

    *accumUs -= period;
    return true;
}
