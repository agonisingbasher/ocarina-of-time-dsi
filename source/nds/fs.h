#ifndef OCARINA_NDS_FS_H
#define OCARINA_NDS_FS_H

#include <nds.h>
#include <stdbool.h>
#include <stdio.h>

#define ND_FS_PATH_MAX 96

/* One open file plus its read accounting. Streaming assets are only worth
 * loading if the read throughput is measured, so the handle keeps the totals
 * the decoder needs instead of making the caller time each chunk. */
typedef struct {
    FILE *f;
    u32 size;
    u32 pos;
    u32 reads;
    u32 readBytes;
    u32 readUs;
    u32 seeks;
    u8 mount;
    bool ok;
} NdFile;

typedef struct {
    u32 checks;
    u32 passed;
    u32 failAt;
    bool fatOk;
    u32 mount;
    u32 fileBytes;
    u32 writeUs;
    u32 readUs;
    u32 readBytes;
    u32 reads;
    u32 seeks;
    u32 decodeUs;
    u32 unpackedBytes;
    u32 bulkBytes;
    u32 bulkUs;
    u32 hash;
    bool hostFound;
    u32 hostStage;
    u32 hostSize;
    u32 hostMagic;
    u32 hostHash;
    u32 hostBytes;
    u32 hostUs;
} NdFsTest;

bool ndFsInit(void);

/* Tries "sd:", then "fat:", then the bare path; file->mount records which one. */
bool ndFsOpen(NdFile *file, const char *path);
void ndFsClose(NdFile *file);
bool ndFsRewind(NdFile *file);
bool ndFsReadAt(NdFile *file, u32 offset, u8 *dst, u32 len);

/* Matches Yaz0FillFn: fills exactly len bytes at the handle position. */
bool ndFsFill(void *ctx, u8 *dst, u32 len);

bool ndFsWrite(const char *path, const u8 *src, u32 len);
bool ndFsRemove(const char *path);

void ndFsSelfTest(NdFsTest *t);

#endif
