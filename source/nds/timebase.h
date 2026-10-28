#ifndef OCARINA_NDS_TIMEBASE_H
#define OCARINA_NDS_TIMEBASE_H

#include <nds.h>

void timeInit(void);
u32 timeTicks(void);
u32 timeTicksToUs(u32 ticks);

#endif
