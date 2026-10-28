#include "yaz0.h"

#include <string.h>

static u32 readBe32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

u32 yaz0DecompressedSize(const u8 *src, u32 srcLen)
{
    if (!src || srcLen < 0x10 || memcmp(src, "Yaz0", 4) != 0) {
        return 0;
    }
    return readBe32(src + 4);
}

/* Non-streaming version: src must contain the whole blob. The Phase 1 asset
 * loader will feed this from a 1 KB ring buffer instead of the whole file. */
bool yaz0Decompress(const u8 *src, u32 srcLen, u8 *dst, u32 dstLen)
{
    u32 sp;
    u32 dp = 0;
    u32 code = 0;
    u32 bitsLeft = 0;

    if (!src || !dst || srcLen < 0x10 || memcmp(src, "Yaz0", 4) != 0) {
        return false;
    }
    if (readBe32(src + 4) != dstLen) {
        return false;
    }

    sp = 0x10;

    while (dp < dstLen) {
        if (bitsLeft == 0) {
            if (sp >= srcLen) {
                return false;
            }
            code = src[sp++];
            bitsLeft = 8;
        }

        if (code & 0x80) {
            if (sp >= srcLen) {
                return false;
            }
            dst[dp++] = src[sp++];
        } else {
            u32 b0;
            u32 len;
            u32 dist;

            if (sp + 1 >= srcLen) {
                return false;
            }
            b0 = src[sp];
            dist = (((b0 & 0x0F) << 8) | src[sp + 1]) + 1;
            sp += 2;

            if ((b0 >> 4) == 0) {
                if (sp >= srcLen) {
                    return false;
                }
                len = (u32)src[sp++] + 0x12;
            } else {
                len = (b0 >> 4) + 2;
            }

            if (dist > dp || len > dstLen - dp) {
                return false;
            }

            {
                const u8 *back = dst + dp - dist;
                u32 i;
                for (i = 0; i < len; i++) {
                    dst[dp + i] = back[i];
                }
            }
            dp += len;
        }

        code <<= 1;
        bitsLeft--;
    }

    return true;
}

typedef struct {
    Yaz0FillFn fill;
    void *ctx;
    u8 *buf;
    u32 bufSize;
    u32 srcLeft;
    u8 *cur;
    u8 *end;
} Yaz0Stream;

/* Keeps unconsumed bytes (lookahead never exceeds 3) and refills the rest of
 * the window from the source, looping if n needs more than one refill. */
static bool streamNeed(Yaz0Stream *s, u32 n)
{
    if (n > s->bufSize) {
        return false;
    }

    while ((u32)(s->end - s->cur) < n) {
        u32 keep = (u32)(s->end - s->cur);
        u32 want;

        if (!s->fill) {
            return false;
        }
        if (keep) {
            memmove(s->buf, s->cur, keep);
        }

        want = s->bufSize - keep;
        if (want > s->srcLeft) {
            want = s->srcLeft;
        }
        if (want == 0) {
            s->cur = s->buf;
            s->end = s->buf + keep;
            return false;
        }

        if (!s->fill(s->ctx, s->buf + keep, want)) {
            return false;
        }
        s->srcLeft -= want;
        s->cur = s->buf;
        s->end = s->buf + keep + want;
    }

    return true;
}

bool yaz0DecompressStreamed(Yaz0FillFn fill, void *ctx, u32 srcLen,
                            u8 *dst, u32 dstLen, u8 *buf, u32 bufSize)
{
    Yaz0Stream s;
    u32 dp = 0;
    u32 code = 0;
    u32 bitsLeft = 0;
    u32 declared;
    u32 i;

    if (!fill || !dst || !buf || srcLen < 0x10 || bufSize < 8) {
        return false;
    }

    s.fill = fill;
    s.ctx = ctx;
    s.buf = buf;
    s.bufSize = bufSize;
    s.srcLeft = srcLen;
    s.cur = buf;
    s.end = buf;

    if (!streamNeed(&s, 4) || memcmp(s.cur, "Yaz0", 4) != 0) {
        return false;
    }
    s.cur += 4;

    if (!streamNeed(&s, 4)) {
        return false;
    }
    declared = readBe32(s.cur);
    s.cur += 4;

    /* compInfoOffset / uncompDataOffset (unused, but part of the header). */
    for (i = 0; i < 8; i++) {
        if (!streamNeed(&s, 1)) {
            return false;
        }
        s.cur++;
    }

    if (declared != dstLen) {
        return false;
    }

#define ENSURE(n)                                                       \
    do {                                                                \
        if ((u32)(s.end - s.cur) < (u32)(n) && !streamNeed(&s, (n))) {  \
            return false;                                               \
        }                                                               \
    } while (0)

    while (dp < dstLen) {
        if (bitsLeft == 0) {
            ENSURE(1);
            code = *s.cur++;
            bitsLeft = 8;
        }

        if (code & 0x80) {
            ENSURE(1);
            dst[dp++] = *s.cur++;
        } else {
            u32 b0;
            u32 len;
            u32 dist;

            ENSURE(2);
            b0 = s.cur[0];
            dist = (((b0 & 0x0F) << 8) | s.cur[1]) + 1;
            s.cur += 2;

            if ((b0 >> 4) == 0) {
                ENSURE(1);
                len = (u32)*s.cur++ + 0x12;
            } else {
                len = (b0 >> 4) + 2;
            }

            if (dist > dp || len > dstLen - dp) {
                return false;
            }

            {
                const u8 *back = dst + dp - dist;
                u32 i;
                for (i = 0; i < len; i++) {
                    dst[dp + i] = back[i];
                }
            }
            dp += len;
        }

        code <<= 1;
        bitsLeft--;
    }

    return true;
}
