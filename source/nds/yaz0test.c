#include "yaz0test.h"

#include <stdlib.h>
#include <string.h>

#include "timebase.h"
#include "yaz0.h"
#include "yaz0_test_data.h"

typedef struct {
    const u8 *src;
    u32 pos;
    u32 size;
} MemSource;

static bool memFill(void *ctx, u8 *dst, u32 len)
{
    MemSource *m = (MemSource *)ctx;

    if (m->pos + len > m->size) {
        return false;
    }
    memcpy(dst, m->src + m->pos, len);
    m->pos += len;
    return true;
}

u32 yaz0Hash(const u8 *p, u32 n)
{
    u32 h = 0x811c9dc5u;
    while (n--) {
        h ^= *p++;
        h *= 0x01000193u;
    }
    return h;
}

void yaz0SelfTest(Yaz0Test *t)
{
    static u8 chunkBuf[1024] __attribute__((aligned(4)));
    u8 smallBuf[8];
    MemSource m;
    u32 size;
    u8 *dst;

    memset(t, 0, sizeof(*t));

    size = yaz0DecompressedSize(yaz0TestPacked, YAZ0_TEST_PACKED_SIZE);
    t->headerOk = (size == YAZ0_TEST_SIZE);
    if (!t->headerOk) {
        return;
    }

    dst = (u8 *)malloc(size);
    if (!dst) {
        return;
    }

    {
        u32 t0 = timeTicks();
        t->ok = yaz0Decompress(yaz0TestPacked, YAZ0_TEST_PACKED_SIZE, dst, size);
        t->us = timeTicksToUs(timeTicks() - t0);
    }
    if (t->ok) {
        t->hash = yaz0Hash(dst, size);
        t->ok = (t->hash == YAZ0_TEST_HASH);
    }

    /* 8-byte window: every token straddles a refill at some point. */
    m.src = yaz0TestPacked;
    m.pos = 0;
    m.size = YAZ0_TEST_PACKED_SIZE;
    t->streamOk = yaz0DecompressStreamed(memFill, &m, YAZ0_TEST_PACKED_SIZE,
                                         dst, size, smallBuf, sizeof(smallBuf));
    if (t->streamOk) {
        t->streamHash = yaz0Hash(dst, size);
        t->streamOk = (t->streamHash == YAZ0_TEST_HASH);
    }

    /* 1 KB window: representative of the Phase 1 SD read granularity. */
    m.src = yaz0TestPacked;
    m.pos = 0;
    m.size = YAZ0_TEST_PACKED_SIZE;
    {
        u32 t0 = timeTicks();
        bool okBig = yaz0DecompressStreamed(memFill, &m, YAZ0_TEST_PACKED_SIZE,
                                            dst, size, chunkBuf, sizeof(chunkBuf));
        t->streamUs = timeTicksToUs(timeTicks() - t0);
        if (!okBig || yaz0Hash(dst, size) != YAZ0_TEST_HASH) {
            t->streamOk = false;
            t->streamUs = 0;
        }
    }

    t->bytes = size;
    free(dst);
}
