#ifndef OCARINA_OOT_PLAT_H
#define OCARINA_OOT_PLAT_H

/* Two include worlds meet in source/oot and cannot be merged.
 *
 * game/oot/include/libc is the decomp's own libc shim: its stdint.h typedefs
 * intptr_t as "signed long" while newlib's says "int", so a translation unit
 * that sees both gets a conflicting-typedef error. Every file here that is
 * compiled with the OoT flag set (-D_LANGUAGE_C, -I game/oot/include/libc, ...)
 * therefore must not read <nds.h>, and every file compiled with our flags must
 * not read -I game/oot/include/libc.
 *
 * This header is the seam between them. It includes nothing and spells its
 * types in plain C, so both sides can read it. */

/* Microseconds since boot, 64 bit and monotonic. osGetTime() hands this to the
 * game's allocator, which stamps every node it touches. */
unsigned long long ootPlatUs(void);

/* Sets the master interrupt enable and returns the previous state as 1 for
 * enabled, 0 for disabled. osSetIntMask() maps OS_IM_NONE/OS_IM_ALL onto this;
 * the N64 had a 24-bit mask per interrupt source and the ARM9 has one bit. */
unsigned long ootPlatIrqSet(int enable);

/* Stops the ARM9 with the decomp's assertion text on the console. __assert is
 * the game's, defined in oot_os.c because the two include worlds disagree about
 * the argument order of the same symbol name. */
void ootPlatHalt(const char *file, int line, const char *expr)
    __attribute__((noreturn));

#endif
