#ifndef OCARINA_GFX_F3DEX2_SELFTEST_H
#define OCARINA_GFX_F3DEX2_SELFTEST_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    bool ok;
    u32 triCount;
    u32 vtxCmds;
    u32 mtxPush;
    u32 mtxPop;
    u32 rdpOps;
    u32 texChanges;
    u32 imgChanges;
    u32 err;
    u32 unsupported;
    /* bit 1 little-endian walk, 2 viewport and combiner, 4 abort paths,
     * 8 the timing bench. The blocks with their own bitmask below share
     * nothing with these. */
    u32 stageBad;
    u32 omBad;    /* bit per failed OtherMode check; 0 when they all pass */
    u32 omChanges;
    u32 realBad;  /* bit per failed real-room-shape check; 0 when they pass */
    u32 beBad;    /* bit per failed big-endian check; 0 when they all pass */
    u32 dlUs;
} F3Test;

void f3SelfTest(F3Test *t);

#endif
