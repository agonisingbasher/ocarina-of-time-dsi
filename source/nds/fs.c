#include "fs.h"

#include <fat.h>
#include <stdlib.h>
#include <string.h>

#include "timebase.h"
#include "yaz0.h"
#include "yaz0_test_data.h"
#include "yaz0test.h"

/* libfat registers the card as "fat:", flashcart loaders use "sd:", and the
 * bare path works when the devoptab root already is the card. The order is a
 * preference, not a guess: whichever opens first is recorded in NdFile.mount. */
static const char *const ndFsMounts[] = { "sd:", "fat:", "" };
#define ND_FS_MOUNTS ((u32)(sizeof(ndFsMounts) / sizeof(ndFsMounts[0])))

#define ND_FS_TMP "/ocarina_fs.tmp"
#define ND_FS_MISSING "/ocarina_no_such_file.bin"
/* Same blob, but placed on the card by the host instead of by this program. */
#define ND_FS_HOST "/ocarina.y0"

static bool fsMounted;

static void buildPath(char *out, u32 outSize, u32 mount, const char *path)
{
    const char *bare = path;

    if (bare[0] && bare[1] == ':') {
        bare += 2;
    }
    if (bare[0] != '/') {
        snprintf(out, outSize, "%s/%s", ndFsMounts[mount], bare);
    } else {
        snprintf(out, outSize, "%s%s", ndFsMounts[mount], bare);
    }
}

bool ndFsInit(void)
{
    if (!fsMounted) {
        fsMounted = fatInitDefault();
    }
    return fsMounted;
}

bool ndFsOpen(NdFile *file, const char *path)
{
    char full[ND_FS_PATH_MAX];
    u32 i;

    memset(file, 0, sizeof(*file));

    if (!ndFsInit()) {
        return false;
    }

    for (i = 0; i < ND_FS_MOUNTS; i++) {
        FILE *f;

        buildPath(full, sizeof(full), i, path);
        f = fopen(full, "rb");
        if (!f) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        file->size = (u32)ftell(f);
        fseek(f, 0, SEEK_SET);
        file->f = f;
        file->mount = (u8)i;
        file->ok = true;
        return true;
    }

    return false;
}

void ndFsClose(NdFile *file)
{
    if (file->f) {
        fclose(file->f);
    }
    file->f = NULL;
    file->ok = false;
}

/* Seeks are only counted when the position really has to move, so a streamed
 * decode reports one seek rather than one per chunk. */
static bool ndFsSeek(NdFile *file, u32 offset)
{
    if (!file->ok) {
        return false;
    }
    if (file->pos == offset) {
        return true;
    }
    if (fseek(file->f, (long)offset, SEEK_SET) != 0) {
        file->ok = false;
        return false;
    }
    file->pos = offset;
    file->seeks++;
    return true;
}

bool ndFsRewind(NdFile *file)
{
    return ndFsSeek(file, 0);
}

bool ndFsFill(void *ctx, u8 *dst, u32 len)
{
    NdFile *file = (NdFile *)ctx;
    u32 t0;
    size_t got;

    if (!file->ok || len == 0 || file->pos + len > file->size) {
        return false;
    }

    t0 = timeTicks();
    got = fread(dst, 1, len, file->f);
    file->readUs += timeTicksToUs(timeTicks() - t0);

    if (got != len) {
        file->ok = false;
        return false;
    }

    file->reads++;
    file->readBytes += len;
    file->pos += len;
    return true;
}

bool ndFsReadAt(NdFile *file, u32 offset, u8 *dst, u32 len)
{
    if (!ndFsSeek(file, offset)) {
        return false;
    }
    return ndFsFill(file, dst, len);
}

bool ndFsWrite(const char *path, const u8 *src, u32 len)
{
    char full[ND_FS_PATH_MAX];
    FILE *f = NULL;
    u32 i;
    size_t put = 0;

    if (!ndFsInit()) {
        return false;
    }

    for (i = 0; i < ND_FS_MOUNTS; i++) {
        buildPath(full, sizeof(full), i, path);
        f = fopen(full, "wb");
        if (f) {
            break;
        }
    }
    if (!f) {
        return false;
    }

    put = fwrite(src, 1, len, f);
    /* fclose is what flushes a libfat write; the file is unusable before it. */
    fclose(f);

    return put == len;
}

bool ndFsRemove(const char *path)
{
    char full[ND_FS_PATH_MAX];
    u32 i;

    if (!ndFsInit()) {
        return false;
    }

    for (i = 0; i < ND_FS_MOUNTS; i++) {
        buildPath(full, sizeof(full), i, path);
        if (remove(full) == 0) {
            return true;
        }
    }

    return false;
}

void ndFsSelfTest(NdFsTest *t)
{
    static u8 window[1024] __attribute__((aligned(32)));
    u8 head[16];
    u8 tail[16];
    NdFile file;
    NdFile ghost;
    NdFile host;
    u8 *dst;
    u32 readsBefore = 0;
    u32 bytesBefore = 0;
    u32 usBefore = 0;
    u32 t0;
    bool streamOk = false;
    bool hostOk = false;

    memset(t, 0, sizeof(*t));

#define CK(cond)                            \
    do {                                    \
        t->checks++;                        \
        if (cond) {                         \
            t->passed++;                    \
        } else if (!t->failAt) {            \
            t->failAt = t->checks;          \
        }                                   \
    } while (0)

    t->fatOk = ndFsInit();
    CK(t->fatOk);
    if (!t->fatOk) {
        return;
    }

    t0 = timeTicks();
    CK(ndFsWrite(ND_FS_TMP, yaz0TestPacked, YAZ0_TEST_PACKED_SIZE));
    t->writeUs = timeTicksToUs(timeTicks() - t0);

    CK(ndFsOpen(&file, ND_FS_TMP));
    if (!file.ok) {
        return;
    }
    t->mount = file.mount;
    t->fileBytes = file.size;
    CK(file.size == YAZ0_TEST_PACKED_SIZE);

    /* Offset 0 is how the archive loader sizes a room before reading it. */
    CK(ndFsReadAt(&file, 0, head, sizeof(head)));
    CK(memcmp(head, "Yaz0", 4) == 0);
    CK(yaz0DecompressedSize(head, sizeof(head)) == YAZ0_TEST_SIZE);

    dst = (u8 *)malloc(YAZ0_TEST_SIZE);
    CK(dst != NULL);
    if (dst) {
        readsBefore = file.reads;
        bytesBefore = file.readBytes;
        usBefore = file.readUs;

        CK(ndFsRewind(&file));
        t0 = timeTicks();
        streamOk = yaz0DecompressStreamed(ndFsFill, &file, file.size, dst,
                                         YAZ0_TEST_SIZE, window, sizeof(window));
        t->decodeUs = timeTicksToUs(timeTicks() - t0);
        CK(streamOk);

        t->unpackedBytes = YAZ0_TEST_SIZE;
        t->hash = yaz0Hash(dst, YAZ0_TEST_SIZE);
        CK(t->hash == YAZ0_TEST_HASH);

        /* The stream must have pulled the blob off the card exactly once. */
        t->reads = file.reads - readsBefore;
        t->readBytes = file.readBytes - bytesBefore;
        t->readUs = file.readUs - usBefore;
        CK(streamOk && t->readBytes == YAZ0_TEST_PACKED_SIZE);

        hostOk = ndFsOpen(&host, ND_FS_HOST);
        t->hostFound = hostOk;
        if (hostOk) {
            u8 hh[16] = { 0 };

            t->hostStage = 1;
            t->hostSize = host.size;
            hostOk = ndFsReadAt(&host, 0, hh, sizeof(hh));
            t->hostMagic = ((u32)hh[0] << 24) | ((u32)hh[1] << 16) |
                           ((u32)hh[2] << 8) | hh[3];
            if (hostOk) {
                t->hostStage = 2;
                hostOk = t->hostSize == YAZ0_TEST_PACKED_SIZE &&
                         yaz0DecompressedSize(hh, sizeof(hh)) == YAZ0_TEST_SIZE;
            }
            if (hostOk) {
                t->hostStage = 3;
                /* The header read left the handle at 16; the stream has to
                 * start at 0 like any other asset. */
                hostOk = ndFsRewind(&host);
                bytesBefore = host.readBytes;
                if (hostOk) {
                    t0 = timeTicks();
                    hostOk = yaz0DecompressStreamed(ndFsFill, &host, host.size,
                                                    dst, YAZ0_TEST_SIZE,
                                                    window, sizeof(window));
                    t->hostUs = timeTicksToUs(timeTicks() - t0);
                    t->hostBytes = host.readBytes - bytesBefore;
                    /* Only hash a decode that worked: a buffer left over from
                     * the previous test hashes clean and hides the failure. */
                    if (hostOk) {
                        t->hostHash = yaz0Hash(dst, YAZ0_TEST_SIZE);
                    }
                    t->hostStage = 4;
                    hostOk = hostOk && t->hostBytes == YAZ0_TEST_PACKED_SIZE &&
                             t->hostHash == YAZ0_TEST_HASH;
                    if (hostOk) {
                        t->hostStage = 5;
                    }
                }
            }
            CK(hostOk);
            ndFsClose(&host);
        }

        free(dst);
    }

    /* Random access at the far end of the file: the lookup path. */
    CK(ndFsReadAt(&file, YAZ0_TEST_PACKED_SIZE - sizeof(tail), tail,
                  sizeof(tail)));
    CK(memcmp(tail, yaz0TestPacked + YAZ0_TEST_PACKED_SIZE - sizeof(tail),
              sizeof(tail)) == 0);
    t->seeks = file.seeks;

    /* One 6 KB read is too short a sample to quote as a card rate, so sweep
     * the blob 32 times and time the whole sweep. */
    {
        const u32 sweeps = 32;
        u32 pass;

        t0 = timeTicks();
        for (pass = 0; pass < sweeps; pass++) {
            u32 left = file.size;
            bool ok = ndFsRewind(&file);

            while (ok && left) {
                u32 n = left > sizeof(window) ? (u32)sizeof(window) : left;
                ok = ndFsFill(&file, window, n);
                left -= n;
            }
            if (!ok) {
                break;
            }
            t->bulkBytes += file.size;
        }
        t->bulkUs = timeTicksToUs(timeTicks() - t0);
        CK(t->bulkBytes == sweeps * YAZ0_TEST_PACKED_SIZE);
    }

    ndFsClose(&file);

    /* A missing asset has to fail cleanly, not read past the end. */
    CK(!ndFsOpen(&ghost, ND_FS_MISSING));

    CK(ndFsRemove(ND_FS_TMP));
    CK(!ndFsOpen(&file, ND_FS_TMP));

#undef CK
}
