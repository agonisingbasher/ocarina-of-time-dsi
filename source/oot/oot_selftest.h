#ifndef OCARINA_OOT_SELFTEST_H
#define OCARINA_OOT_SELFTEST_H

#include <stdbool.h>

/* The first slice of real Ocarina of Time code running on the ARM9: the game's
 * own arena allocator, its display-list bump pointer, its fixed-point sine
 * table and its float math, over the three libultra calls in source/oot/oot_os.c.
 *
 * Plain C types only. The body is compiled with the OoT flag set and main.c is
 * not, so this header is the one place the two worlds agree. */
typedef struct {
    unsigned int checks;    /* assertions made */
    unsigned int passed;    /* assertions held */
    unsigned int failAt;    /* first failed assertion, 0 when everything held */
    unsigned int fail2;     /* second failed assertion, 0 when there was only one */
    bool allOk;             /* every assertion held */
    unsigned int churnUs;   /* 256 alloc/free pairs of 64 bytes, microseconds */
    unsigned int atanMicro; /* Math_FAtan2F(1,1) times 1e6, pi/4 is 785398 */
    unsigned int atanCF;    /* Math_FAtanContFracF(0.5) times 1e6, 463647 */
    unsigned int atan2S;    /* Math_Atan2S(1,1), the game's binangle path */
    unsigned int normErr;   /* |len - 1| after guNormalize(3,4,0), same units */
} OotSelfTest;

void ootSelfTest(OotSelfTest *t);

#endif
