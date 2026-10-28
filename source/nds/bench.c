#include "bench.h"

#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "timebase.h"

#define IO_CHUNK 32768u

static u8 ioBuf[IO_CHUNK] __attribute__((aligned(32)));

static u8 chunkByte(u32 offset)
{
    return (u8)((offset * 37u) ^ (offset >> 8));
}

static bool chunkMatches(const u8 *buf, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++) {
        if (buf[i] != chunkByte(i)) {
            return false;
        }
    }
    return true;
}

static u32 kbs(u32 bytes, u32 fromTicks, u32 toTicks)
{
    u32 us = timeTicksToUs(toTicks - fromTicks);
    if (!us) {
        return 0;
    }
    return (u32)(((u64)bytes * 1000000ull) / (1024ull * us));
}

void sdBenchmark(SdBench *out, u32 totalBytes)
{
    static const char *const paths[] = {
        "sd:/ocarina_phase0.tmp",
        "fat:/ocarina_phase0.tmp",
        "/ocarina_phase0.tmp",
    };

    FILE *f = NULL;
    const char *path = NULL;
    u32 i;
    u32 left;
    u32 t0;
    u32 t1;

    memset(out, 0, sizeof(*out));

    out->fatOk = ndFsInit();

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        f = fopen(paths[i], "wb");
        if (f) {
            path = paths[i];
            break;
        }
    }

    if (!f) {
        return;
    }

    out->fileOpened = true;

    for (i = 0; i < IO_CHUNK; i++) {
        ioBuf[i] = chunkByte(i);
    }

    t0 = timeTicks();
    left = totalBytes;
    while (left) {
        u32 n = left > IO_CHUNK ? IO_CHUNK : left;
        if (fwrite(ioBuf, 1, n, f) != n) {
            break;
        }
        out->writeBytes += n;
        left -= n;
    }
    fclose(f);
    t1 = timeTicks();

    out->writeComplete = (out->writeBytes == totalBytes);
    out->writeKBs = kbs(out->writeBytes, t0, t1);

    f = fopen(path, "rb");
    if (!f) {
        remove(path);
        return;
    }

    out->dataVerified = true;
    t0 = timeTicks();
    left = totalBytes;
    while (left) {
        u32 n = left > IO_CHUNK ? IO_CHUNK : left;
        if (fread(ioBuf, 1, n, f) != n) {
            break;
        }
        if (!chunkMatches(ioBuf, n)) {
            out->dataVerified = false;
        }
        out->readBytes += n;
        left -= n;
    }
    fclose(f);
    t1 = timeTicks();

    out->readComplete = (out->readBytes == totalBytes);
    out->readKBs = kbs(out->readBytes, t0, t1);

    remove(path);
}
