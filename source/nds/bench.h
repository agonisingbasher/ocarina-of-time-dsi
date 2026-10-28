#ifndef OCARINA_NDS_BENCH_H
#define OCARINA_NDS_BENCH_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    bool fatOk;
    bool fileOpened;
    bool writeComplete;
    bool readComplete;
    bool dataVerified;
    u32 writeBytes;
    u32 readBytes;
    u32 writeKBs;
    u32 readKBs;
} SdBench;

void sdBenchmark(SdBench *out, u32 totalBytes);

#endif
