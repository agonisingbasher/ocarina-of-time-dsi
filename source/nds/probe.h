#ifndef OCARINA_NDS_PROBE_H
#define OCARINA_NDS_PROBE_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    bool twlMode;
    u16 scfgClkBefore;
    u16 scfgClkAfter;
    u32 scfgExt;
    bool hadBoost;

    u32 heapStart;
    u32 heapEnd;
    u32 heapLimit;
    u32 heapLimitMb;
    u32 heapLimitTenths;

    u32 ramTestedKb;
    u32 ramUs;
    u32 ramFailKb;
    bool ramOk;
} HwProbe;

/* ram/ramBytes point at game RAM to pattern-test (usually the game arena),
 * so the probe never mallocs and the result describes the memory the port
 * will actually run in. */
void probeHardware(HwProbe *p, u8 *ram, u32 ramBytes);

#endif
