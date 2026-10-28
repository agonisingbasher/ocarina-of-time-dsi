#include "probe.h"

#include <stdlib.h>
#include <string.h>
#include <calico/nds/scfg.h>
#include "timebase.h"

#define RAM_TEST_WANT   (12u << 20)
#define PAGE_SIZE       4096u
static u32 pagePattern0(u32 page)
{
    return 0xA5A50000u ^ (page * 0x01000193u);
}

static bool ramPattern(u8 *base, u32 bytes, u32 *failKb)
{
    u32 pages = bytes / PAGE_SIZE;
    u32 i;

    for (i = 0; i < pages; i++) {
        volatile u32 *w = (volatile u32 *)(base + i * PAGE_SIZE);
        w[0] = pagePattern0(i);
        w[1] = ~pagePattern0(i);
        w[PAGE_SIZE / 4 - 1] = 0x5A5A0000u ^ i;
    }

    for (i = 0; i < pages; i++) {
        volatile u32 *w = (volatile u32 *)(base + i * PAGE_SIZE);
        if (w[0] != pagePattern0(i) ||
            w[1] != ~pagePattern0(i) ||
            w[PAGE_SIZE / 4 - 1] != (0x5A5A0000u ^ i)) {
            *failKb = i * (PAGE_SIZE / 1024);
            return false;
        }
    }

    return true;
}

void probeHardware(HwProbe *p, u8 *ram, u32 ramBytes)
{
    u8 *aligned;
    u32 usable;
    u32 t0;
    u32 t1;

    memset(p, 0, sizeof(*p));

    p->twlMode = systemIsTwlMode();
    p->scfgClkBefore = REG_SCFG_CLK;
    p->scfgExt = REG_SCFG_EXT;

    p->hadBoost = setCpuClock(true);
    p->scfgClkAfter = REG_SCFG_CLK;

    p->heapStart = (u32)getHeapStart();
    p->heapEnd = (u32)getHeapEnd();
    p->heapLimit = (u32)getHeapLimit();
    p->heapLimitMb = (p->heapLimit - 0x02000000u) >> 20;
    p->heapLimitTenths = (u32)(((u64)(p->heapLimit - 0x02000000u) * 10ull) >> 20);

    if (ramBytes > RAM_TEST_WANT) {
        ramBytes = RAM_TEST_WANT;
    }
    if (!ram || ramBytes < PAGE_SIZE * 2) {
        p->ramOk = false;
        p->ramFailKb = 0;
        return;
    }

    aligned = (u8 *)(((u32)ram + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    usable = (ramBytes - (u32)(aligned - ram)) & ~(PAGE_SIZE - 1);

    t0 = timeTicks();
    p->ramOk = ramPattern(aligned, usable, &p->ramFailKb);
    t1 = timeTicks();

    p->ramUs = timeTicksToUs(t1 - t0);
    p->ramTestedKb = usable / 1024;
}
