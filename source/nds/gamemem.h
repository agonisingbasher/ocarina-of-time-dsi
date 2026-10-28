#ifndef OCARINA_NDS_GAMEMEM_H
#define OCARINA_NDS_GAMEMEM_H

#include <nds.h>
#include <stdbool.h>

/* Kept out of the arena for newlib, libfat and the console. */
#define GAMEMEM_RESERVE (384u * 1024u)

/* Linear arena carved from the libnds heap once at boot. The N64 game owns
 * fixed pools, so a bump allocator is enough until a real allocator is
 * needed; nothing here is thread safe. */
typedef struct {
    u8 *base;
    u32 size;
    u32 used;
    u32 peak;
    u32 blocks;
    u32 heapLeft;
} GameMem;

bool gameMemInit(GameMem *m);

/* align must be a power of two; anything below 4 is raised to 4. */
void *gameMemAlloc(GameMem *m, u32 bytes, u32 align);

/* Frees everything at once. peak survives so the overlay can show the high
 * water mark across resets. */
void gameMemReset(GameMem *m);

typedef struct {
    bool ok;
    u32 size;
    u32 us;
} GameMemTest;

void gameMemSelfTest(GameMem *m, GameMemTest *t);

#endif
