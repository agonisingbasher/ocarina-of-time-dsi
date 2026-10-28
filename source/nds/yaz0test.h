#ifndef OCARINA_NDS_YAZ0_TEST_H
#define OCARINA_NDS_YAZ0_TEST_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    bool headerOk;
    bool ok;
    u32 bytes;
    u32 us;
    u32 hash;
    bool streamOk;
    u32 streamUs;
    u32 streamHash;
} Yaz0Test;

void yaz0SelfTest(Yaz0Test *t);

/* FNV-1a over the fixture, shared with the fs self test. */
u32 yaz0Hash(const u8 *p, u32 n);

#endif
