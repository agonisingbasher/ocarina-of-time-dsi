#include "gamemem.h"

#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "timebase.h"

/* Must run before anything else touches the newlib heap: the break is still
 * at the linker heap start, so the whole loader-provided range is available. */
bool gameMemInit(GameMem *m)
{
    u32 limit = (u32)getHeapLimit();
    u32 brk = (u32)sbrk(0);
    u32 freeBytes;

    memset(m, 0, sizeof(*m));

    if (limit <= brk) {
        return false;
    }
    freeBytes = limit - brk;
    if (freeBytes <= GAMEMEM_RESERVE + (64u * 1024u)) {
        return false;
    }

    m->heapLeft = freeBytes;
    m->size = (freeBytes - GAMEMEM_RESERVE) & ~31u;
    m->base = memalign(32, m->size);
    if (!m->base) {
        m->size = 0;
        return false;
    }

    m->heapLeft = freeBytes - m->size;
    return true;
}

void *gameMemAlloc(GameMem *m, u32 bytes, u32 align)
{
    u32 off;
    void *p;

    if (align < 4) {
        align = 4;
    }
    if (align & (align - 1)) {
        return NULL;
    }

    off = (m->used + align - 1) & ~(align - 1);
    if (off > m->size || bytes > m->size - off) {
        return NULL;
    }

    p = m->base + off;
    m->used = off + bytes;
    m->blocks++;
    if (m->used > m->peak) {
        m->peak = m->used;
    }
    return p;
}

void gameMemReset(GameMem *m)
{
    m->used = 0;
    m->blocks = 0;
}

void gameMemSelfTest(GameMem *m, GameMemTest *t)
{
    void *a;
    void *b;
    void *c;
    u32 t0;

    memset(t, 0, sizeof(*t));
    t->size = m->size;
    if (!m->base) {
        return;
    }

    t0 = timeTicks();

    gameMemReset(m);
    a = gameMemAlloc(m, 32, 32);
    b = gameMemAlloc(m, 1000, 4);
    c = gameMemAlloc(m, m->size, 4);

    t->ok = a == m->base &&
            ((u32)a & 31u) == 0 &&
            b == m->base + 32 &&
            c == NULL;

    gameMemReset(m);
    t->ok = t->ok && gameMemAlloc(m, 64, 32) == m->base;
    gameMemReset(m);

    t->us = timeTicksToUs(timeTicks() - t0);
}
