#include "ultra64.h"

#include "oot_plat.h"

/* The three libultra calls that the first slice of real OoT code needs.
 *
 * __osMalloc_n64.c stamps every node it allocates with the owning thread and
 * the current time, and wraps the arena walk in osSetIntMask. On the N64 those
 * are a thread control block, a 64-bit counter driven by the 937 kHz timer and
 * a 24-bit per-source interrupt mask. On the ARM9 there is one hardware thread
 * and one interrupt enable bit, so each call collapses to what the allocator
 * actually uses it for: a value that orders allocations in time and a flag that
 * keeps an interrupt out of the arena walk. */

OSTime osGetTime(void)
{
    return (OSTime)ootPlatUs();
}

OSIntMask osSetIntMask(OSIntMask mask)
{
    unsigned long wasEnabled;

    wasEnabled = ootPlatIrqSet(mask != OS_IM_NONE);

    return wasEnabled ? OS_IM_ALL : OS_IM_NONE;
}

OSId osGetThreadId(OSThread *thread)
{
    (void)thread;

    /* One hardware thread, so the id can only be useful as "not zero": the
     * allocator's debug path treats a mismatched id as a cross-thread free. */
    return 1;
}

/* game/oot/include/libc/assert.h declares this as __assert(expression, file,
 * line); the Makefile renames it, because newlib's <assert.h> spells the same
 * symbol (function, line, file) and already defines it in libg.a. The argument
 * order is the decomp's own, so the console work in oot_plat.c takes them
 * apart the same way. */
__attribute__((noreturn)) void ootCompatAssert(const char *assertion,
                                               const char *file, int line)
{
    ootPlatHalt(file, line, assertion);
}
