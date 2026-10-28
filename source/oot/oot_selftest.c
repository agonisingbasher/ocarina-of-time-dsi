#include "ultra64.h"

#include "gfxalloc.h"
#include "libc64/math64.h"
#include "libc64/malloc.h"
#include "listalloc.h"
#include "oot_dl.h"
#include "oot_plat.h"
#include "oot_selftest.h"
#include "sys_matrix.h"
#include "sys_math.h"

/* The first time real Ocarina of Time instructions execute in this build.
 *
 * Only nine decomp translation units are in the link at this point, so the
 * test is deliberately narrow: it proves the arena allocator, the display-list
 * bump pointer, the fixed-point sine table and the float math library behave on
 * ARM9 as they do on MIPS, over the three libultra calls source/oot/oot_os.c
 * supplies. Nothing here reads a ROM asset. */

#define OOT_ARENA_BYTES (32u * 1024u)
#define OOT_CHURN 256u

/* The allocator rounds the arena start up to 16 and the size down, so an
 * unaligned buffer quietly shrinks the pool. Aligned here so the number on the
 * HUD is the number asked for. */
static u8 sArenaMem[OOT_ARENA_BYTES] __attribute__((aligned(16)));

/* One Gfx is two words, and Gfx_Alloc writes the branch word after each block,
 * so the scratch has to outlast three allocations plus their headers. */
static Gfx sGfxMem[64];

/* The display-list test inspects what the builder left in the blob, so the
 * result has to outlive the call. */
static OotDl sDl;

/* Assertion ids, because the HUD has room for the number and nothing else:
 * 1-2 arena init and size, 10-13 four live blocks, 14-17 their alignment,
 * 20-23 churn and the footprint coming back to where it started, 24 node walk,
 * 30-33 list alloc, 40-42 display-list bump pointer, 50-56 float and fixed
 * math, 60 the interrupt mask round trip, 70-78 the display list the game's own
 * Gfx_Alloc and gSP* macros build. */
static void check(OotSelfTest *t, u32 id, bool ok)
{
    t->checks++;
    if (ok) {
        t->passed++;
        return;
    }
    if (t->failAt == 0) {
        t->failAt = id;
    } else if (t->fail2 == 0) {
        t->fail2 = id;
    }
}

static void testArena(OotSelfTest *t)
{
    u32 maxFree;
    u32 freeBytes;
    u32 allocBytes;
    u32 freeBefore;
    u32 maxBefore;
    void *keep[4];
    u32 churned;
    u32 i;
    u32 t0;

    SystemArena_Init(sArenaMem, OOT_ARENA_BYTES);
    check(t, 1u, SystemArena_IsInitialized() != 0);

    SystemArena_GetSizes(&maxFree, &freeBytes, &allocBytes);
    freeBefore = freeBytes;
    maxBefore = maxFree;
    check(t, 2u, freeBefore > OOT_ARENA_BYTES / 2u);

    /* Four live blocks at once: first fit has to split the free node four
     * times and still find room for the block after each split. */
    for (i = 0; i < 4; i++) {
        keep[i] = SystemArena_Malloc(1024u * (i + 1u));
        check(t, 10u + i, keep[i] != NULL);
        check(t, 14u + i, (((u32)keep[i]) & 0xFu) == 0u);
    }
    for (i = 0; i < 4; i++) {
        SystemArena_Free(keep[i]);
    }

    /* 256 times around the split-and-coalesce path, timed. A room load runs
     * this hundreds of times per second once actors arrive, so the cost per
     * pair is what decides whether the game's allocator can stay as it is. */
    t0 = (u32)ootPlatUs();
    for (churned = 0; churned < OOT_CHURN; churned++) {
        void *p = SystemArena_Malloc(64);

        if (p == NULL) {
            break;
        }
        SystemArena_Free(p);
    }
    t->churnUs = (u32)ootPlatUs() - t0;
    check(t, 20u, churned == OOT_CHURN);

    /* Back to exactly the starting footprint, or the free path leaks a node
     * and the first real level would exhaust the pool. */
    SystemArena_GetSizes(&maxFree, &freeBytes, &allocBytes);
    check(t, 21u, freeBytes == freeBefore);
    check(t, 22u, allocBytes == 0u);
    check(t, 23u, maxFree == maxBefore);
    /* __osCheckArena answers 1 for "a node lost its magic" and 0 for "still
     * walkable", which is the opposite of the boolean the rest of this file
     * uses. */
    check(t, 24u, __osCheckArena(&gSystemArena) == 0);
}

static void testListAlloc(OotSelfTest *t)
{
    ListAlloc head;
    ListAlloc *node;
    u32 live = 0;
    u32 walked = 0;
    u32 i;

    ListAlloc_Init(&head);
    check(t, 30u, head.prev == NULL && head.next == NULL);

    for (i = 0; i < 8; i++) {
        node = (ListAlloc *)ListAlloc_Alloc(&head, 40);
        if (node == NULL) {
            break;
        }
        live++;
    }
    check(t, 31u, live == 8u);

    /* ListAlloc_Alloc hangs the newest block on head->next and links each block's
     * own ->next to the block allocated after it, so head->prev is the oldest and
     * a forward walk from there is the only one that sees all eight. */
    for (node = head.prev; node != NULL; node = node->next) {
        walked++;
    }
    check(t, 32u, walked == live);

    ListAlloc_FreeAll(&head);
    check(t, 33u, head.prev == NULL && head.next == NULL);
}

static void testGfxAlloc(OotSelfTest *t)
{
    Gfx *cursor = sGfxMem;
    u8 *first;
    u8 *second;
    u32 used;

    cursor = Gfx_Open(cursor);
    first = (u8 *)Gfx_Alloc(&cursor, 24);
    second = (u8 *)Gfx_Alloc(&cursor, 24);

    /* ALIGN8(24) is 24 and the cursor lands one Gfx past the block, so the
     * second block starts 24 + sizeof(Gfx) bytes after the first. */
    used = (u32)(second - first);
    check(t, 40u, used == 24u + (u32)sizeof(Gfx));

    /* Gfx_Alloc closes every block with gSPBranchList, in the slot the cursor
     * held before the block: that word is what lets a display list walk from
     * one allocation to the next without the game tracking lengths. */
    check(t, 41u, ((const u32 *)(first - (s32)sizeof(Gfx)))[0] != 0u);
    check(t, 42u, cursor == (Gfx *)(second + 24u));
}

/* Defined in libc64/math64.c but not listed in libc64/math64.h. */
f32 Math_FAtanContFracF(f32 x);

static void testMath(OotSelfTest *t)
{
    f32 atan = Math_FAtan2F(1.0f, 1.0f);
    f32 err;
    f32 x = 3.0f;
    f32 y = 4.0f;
    f32 z = 0.0f;
    f32 len;
    s16 sine = sins(0x4000);
    s16 cosine = coss(0x4000);
    s16 ang = Math_Atan2S(1.0f, 1.0f);

    t->atanMicro = (u32)(atan * 1000000.0f);
    t->atanCF = (u32)(Math_FAtanContFracF(0.5f) * 1000000.0f);
    t->atan2S = (u32)(s32)ang;
    check(t, 50u, t->atanMicro > 785298u && t->atanMicro < 785498u);
    /* atan(0.5) is 0.46364761, so the continued fraction is checked at a second
     * point rather than only where its own truncation happens to land. */
    check(t, 55u, t->atanCF > 463548u && t->atanCF < 463748u);
    check(t, 56u, ang == 0x2000);

    check(t, 51u, sine > 32700);
    check(t, 52u, cosine > -100 && cosine < 100);

    guNormalize(&x, &y, &z);
    len = sqrtf(x * x + y * y + z * z);
    err = len - 1.0f;
    if (err < 0.0f) {
        err = -err;
    }
    t->normErr = (u32)(err * 1000000.0f);
    check(t, 53u, t->normErr < 100u);
    check(t, 54u, x > 0.59f && x < 0.61f);
}

/* The list built by the game's own Gfx_Alloc and gSP* macros, checked the way
 * the decoder will meet it: as bytes in one blob addressed by segment 7. */
static void testGameDl(OotSelfTest *t)
{
    const Gfx *list;
    const Mtx *proj;
    const Mtx *mv;
    const u8 *blob;

    ootDlBuild(&sDl, 0x2000);
    check(t, 70u, sDl.stage == 0u);

    list = (const Gfx *)sDl.list;
    blob = (const u8 *)sDl.blob;
    proj = (const Mtx *)sDl.proj;
    mv = (const Mtx *)sDl.mv;

    /* Gfx_Alloc closes the block it hands back with a branch into the next, so
     * the Gfx immediately before the list is that branch: opcode G_DL. */
    check(t, 71u, (((const u32 *)(list - 1))[0] >> 24) == 0xDEu);
    /* The list itself has to sit inside the blob, and so does the eighth
     * vertex, or a segment-relative address would resolve into other code. */
    check(t, 72u, (const u8 *)list >= blob && (const u8 *)list < blob + 1024u);
    check(t, 73u, ((const u8 *)sDl.vtx - blob) + 8L * (long)sizeof(Vtx) <= 1024L);
    check(t, 74u, (list[0].words.w0 >> 24) == 0xE3u);
    /* Gfx_Alloc does not lay blocks end to end: each one is followed by the
     * 8-byte branch into the next, so the second matrix sits sizeof(Gfx) past
     * the end of the first. A Gfx is two words, not four. */
    check(t, 78u, ((const u8 *)mv - (const u8 *)proj) ==
                      (long)(sizeof(Mtx) + sizeof(Gfx)));

    /* guPerspective's w term is -1 at m[2][3], which is what makes the DS see a
     * perspective divide at all. Both of these failed until guMtxF2L stopped
     * writing the matrix as 32-bit words, which is a big-endian-only
     * assumption. */
    check(t, 75u, proj->intPart[2][3] == 0xFFFFu && proj->fracPart[2][3] == 0u);
    /* The modelview is not the identity any more, and this is the check that
     * says so: guMtxIdent used to put exactly 1.0 on the diagonal and nothing
     * in the translation. Now the game's matrix stack owns the slot, so the
     * first diagonal entry is a truncated 45 degree cosine and the x
     * translation is the seam. The identity still exists, but only in the
     * stack's base slot, which the next test reads with Matrix_Get. */
    check(t, 76u, mv->intPart[0][0] == 0u && mv->fracPart[0][0] != 0u &&
                  mv->intPart[3][0] != 0u);
    /* sins(0x2000) is a quarter turn, so the seam sits at sin 45 deg of the
     * 100 pixels it can travel either way: the fixed-point table is what moves
     * it, and 70 is that read straight out of the game's table. */
    check(t, 77u, sDl.edge == 70L);
}

/* The modelview is written by sys_matrix.c now, so what is under test is that
 * module: that its stack really grew out of the arena the game was handed, that
 * the 16.16 words Matrix_MtxFToMtx lays out are the rotation the game's own
 * sine table says they are, that Push and Pop hand the base slot back clean,
 * and that Matrix_Mult runs a real concatenation instead of a copy.
 *
 * The slots of a rotation are read by position, not by name: MtxF orders its
 * fields xx, yx, zx, wx, xy, and the note in ultratypes.h says that order is
 * the transpose of how a matrix is usually written down, so intPart[0][1] is
 * the field the game calls yx and intPart[1][0] is the one it calls xy. */
static void testGameMtx(OotSelfTest *t)
{
    const Mtx *mv;
    MtxF moved;
    MtxF cur;
    MtxF twice;
    u32 cosSlot;

    ootDlBuild(&sDl, 0x2000);
    mv = (const Mtx *)sDl.mv;
    cosSlot = mv->fracPart[0][0];

    /* Matrix_Init took its 20 slots out of the arena oot_dl.c handed the
     * GameState, so the live slot sits inside it with room for a push above. */
    check(t, 80u, sDl.stackOff > 0ul &&
                  sDl.stackOff + sizeof(MtxF) <= sDl.arenaBytes);
    /* cos at a binangle of 0x2000 is coss(0x2000) = sins(0x6000), and sins
     * folds 0x600 into sintable[0x3FF - 0x200] = sintable[0x1FF] = 23151. That
     * over 32767, times 0x10000 and truncated is 46303: the window below is
     * the game's table resolution, not a fudge. */
    check(t, 81u, mv->intPart[0][0] == 0u && cosSlot >= 46300u &&
                  cosSlot <= 46380u);
    /* The slot beside the cosine is the sine, and at 45 degrees the game's own
     * table says they are NOT the same number: sins(0x2000) reads
     * sintable[0x200] straight, while coss folds to sintable[0x1FF], one entry
     * earlier. The two entries are 23187 and 23151, and that step of 36 in
     * 15-bit doubles to 72 in 16.16. No hand-written identity or rotation
     * built from a symmetric table gives this pair. */
    check(t, 82u, mv->intPart[0][1] == 0u &&
                  (u32)mv->fracPart[0][1] - cosSlot == 72u);
    /* The partner on the other side of the diagonal is the negated *sine*, not
     * the negated cosine, and truncation toward zero makes it and the sine
     * slot add back to 2^16 exactly. */
    check(t, 83u, mv->intPart[1][0] == 0xFFFFu &&
                  (u32)mv->fracPart[1][0] + mv->fracPart[0][1] == 65536u);
    /* Rotation about z leaves the third row at exactly 1.0, which is what keeps
     * the quads at the depth guPerspective put them at. */
    check(t, 84u, mv->intPart[2][2] == 1u && mv->fracPart[2][2] == 0u);
    /* The translation is the game's arithmetic all the way down: pos.x is the
     * seam, the seam is 70 at this angle, and 70 * 0x10000 has no fraction. */
    check(t, 85u, sDl.edge == 70L && mv->intPart[3][0] == (u16)sDl.edge &&
                  mv->fracPart[3][0] == 0u);
    check(t, 86u, mv->intPart[3][1] == 0u && mv->intPart[3][2] == 0u &&
                  mv->fracPart[3][1] == 0u && mv->fracPart[3][2] == 0u);

    /* Pop hands the base slot back unchanged, so the next frame starts from the
     * identity again and a stack that leaked would fail here instead of walking
     * off the arena. */
    Matrix_Get(&cur);
    check(t, 87u, cur.xx == 1.0f && cur.xy == 0.0f && cur.xw == 0.0f &&
                  cur.ww == 1.0f);

    /* Concatenation. MTXMODE_NEW lays a pure translate down through
     * SkinMatrix_SetTranslate, and applying it to the identity twice has to
     * double the offset, which is what only a real multiply does. */
    Matrix_Translate(64.0f, 32.0f, -16.0f, MTXMODE_NEW);
    Matrix_Get(&moved);
    Matrix_Put(&gIdentityMtxF);
    Matrix_Mult(&moved, MTXMODE_APPLY);
    Matrix_Get(&cur);
    check(t, 88u, cur.xx == 1.0f && cur.xw == 64.0f && cur.yw == 32.0f &&
                  cur.zw == -16.0f);
    Matrix_Mult(&moved, MTXMODE_APPLY);
    Matrix_Get(&twice);
    check(t, 89u, twice.xx == 1.0f && twice.xw == 128.0f &&
                  twice.yw == 64.0f && twice.zw == -32.0f);

    Matrix_Put(&gIdentityMtxF);
}

static void testIrq(OotSelfTest *t)
{
    OSIntMask was = osSetIntMask(OS_IM_NONE);
    OSIntMask prev = osSetIntMask(was);

    /* The mask that came back on the way in has to be the one reported by the
     * way out: the allocator leans on this pair to keep an interrupt from
     * walking a half-spliced free list. */
    check(t, 60u, prev == OS_IM_NONE);
}

void ootSelfTest(OotSelfTest *t)
{
    memset(t, 0, sizeof(*t));

    testArena(t);
    testListAlloc(t);
    testGfxAlloc(t);
    testMath(t);
    testGameDl(t);
    testGameMtx(t);
    testIrq(t);

    t->allOk = t->passed == t->checks;
}
