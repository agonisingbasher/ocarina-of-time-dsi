#include "timebase.h"

void timeInit(void)
{
    cpuStartTiming(0);
}

u32 timeTicks(void)
{
    return cpuGetTiming();
}

/* Callers must subtract ticks first: the 32-bit tick counter wraps every
 * ~128 s at BUS_CLOCK, while 32-bit microseconds wrap at ~71 min. */
u32 timeTicksToUs(u32 ticks)
{
    return (u32)(((u64)ticks * 1000000ull) / BUS_CLOCK);
}
