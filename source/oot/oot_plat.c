#include "oot_plat.h"

#include <stdio.h>

#include <nds.h>

#include "timebase.h"

static u32 sLastTicks;
static u32 sWrapCount;

unsigned long long ootPlatUs(void)
{
    u64 ticks;

    /* cpuGetTiming() is a 32-bit counter that wraps every ~128 s at BUS_CLOCK.
     * A clock that rewinds would make a fresh allocation look older than the
     * one before it, so the wraps are counted here and the game never sees the
     * discontinuity. Not reentrant: only the scheduler thread calls this. */
    u32 now = timeTicks();

    if (now < sLastTicks) {
        sWrapCount++;
    }
    sLastTicks = now;

    ticks = ((u64)sWrapCount << 32) + now;

    /* 6.4e9 ticks per wrap times 1e6 stays inside 64 bits for centuries. */
    return (unsigned long long)((ticks * 1000000ull) / BUS_CLOCK);
}

unsigned long ootPlatIrqSet(int enable)
{
    u32 wasEnabled = REG_IME != IME_DISABLE;

    REG_IME = enable ? IME_ENABLE : IME_DISABLE;

    return wasEnabled ? 1ul : 0ul;
}

/* game/oot/include/libc/assert.h declares __assert(assertion, file, line) for
 * the decomp's own ASSERT macro, which stays live in a NON_MATCHING build, and
 * newlib declares the same symbol with the arguments in a different order. So
 * the game's version is defined in oot_os.c, where only the decomp's headers
 * are visible, and it stops here.
 *
 * sys_matrix.c uses ASSERT to catch a matrix stack underflow and the arena
 * code to catch a pool that ran out, so reaching this means the game's own data
 * is already inconsistent. Continuing would let the caller carry a half-written
 * value into the next frame. The last two rows of the console get the file, the
 * line and the expression, and the ARM9 stops there: a screenshot with a frozen
 * counter and an ASSERT row is a measurement, a screenshot with wrong numbers
 * in it is not. */
void ootPlatHalt(const char *file, int line, const char *expr)
{
    iprintf("\x1b[23;1HASSERT %.20s:%d       ", file, line);
    iprintf("\x1b[22;1H%.31s                    ", expr);

    for (;;) {
        __asm__ volatile("" ::: "memory");
    }
}
