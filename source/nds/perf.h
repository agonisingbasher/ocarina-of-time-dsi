#ifndef OCARINA_NDS_PERF_H
#define OCARINA_NDS_PERF_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    u32 lastEndTicks;
    u32 secondStartTicks;
    u64 textAccumUs;
    u32 framesInSec;
    u32 fps;
    u32 frameUs;
    u32 worstUs;
} Perf;

void perfInit(Perf *p);
void perfEnd(Perf *p);
bool perfConsume(u64 *accumUs, u32 hz);

#endif
