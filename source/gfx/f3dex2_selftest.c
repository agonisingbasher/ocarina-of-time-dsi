#include "f3dex2_selftest.h"

#include <string.h>

#include "f3dex2.h"
#include "timebase.h"

#define BUF_SIZE     16384
#define OFF_MAIN     0x000
#define OFF_SUB      0x100
#define OFF_VPT      0x160
#define OFF_VTX      0x180
#define OFF_MTX      0x1C0
#define OFF_BENCH    0x200
#define OFF_BIG      0x3000
#define OFF_SUB2     0x3600
#define OFF_BE       0x3C00
#define BE_VTX       0x000u
#define BE_MTX       0x080u
#define BE_VPT       0x100u
#define BE_SUB       0x180u
#define BE_MAIN      0x200u
#define BENCH_TRIS   2048
#define BENCH_RUNS   20

static u32 sCcMode[8];
static u32 sCcAlpha[8];
static u32 sCcSeen;
static u32 sVpSeen;

static void ccProbe(void *ctx, const F3Combiner *cc)
{
    (void)ctx;
    if (sCcSeen < 8) {
        sCcMode[sCcSeen] = cc->mode;
        sCcAlpha[sCcSeen] = cc->alpha;
        sCcSeen++;
    }
}

static void vpProbe(void *ctx, const F3Vp *vp)
{
    (void)ctx;
    (void)vp;
    sVpSeen++;
}

static u32 sOmFlags[8];
static u32 sOmAc[8];
static u32 sOmSeen;

static void omProbe(void *ctx, const F3Om *om, u32 flags, u32 acMode)
{
    (void)ctx;
    (void)om;
    if (sOmSeen < 8) {
        sOmFlags[sOmSeen] = flags;
        sOmAc[sOmSeen] = acMode;
        sOmSeen++;
    }
}

static u32 segAddr(u32 off)
{
    return 0x06000000u | off;
}

/* Segment 5 is the big-endian one: a room file, DMA'd verbatim out of the
 * cartridge, is the N64's bytes and nothing else. */
static u32 beAddr(u32 off)
{
    return 0x05000000u | off;
}

static const u8 *sMtxPtr;
static u32 sMtxWords[16];
static u32 sMtxArgs;
static u32 sMtxSeen;

static void mtxProbe(void *ctx, u32 ev, const u8 *mtx, u32 arg)
{
    u32 i;

    (void)ctx;
    if (ev == F3_MTX_EV_LOAD && sMtxSeen < 4) {
        for (i = 0; i < 16; i++) {
            sMtxWords[i] = ((const u32 *)mtx)[i];
        }
        sMtxPtr = mtx;
        sMtxArgs = arg;
        sMtxSeen++;
    }
}

static void put(u32 **p, u32 w0, u32 w1)
{
    (*p)[0] = w0;
    (*p)[1] = w1;
    *p += 2;
}

static void putBe(u32 **p, u32 w0, u32 w1)
{
    (*p)[0] = (u32)__builtin_bswap32(w0);
    (*p)[1] = (u32)__builtin_bswap32(w1);
    *p += 2;
}

static void putBe16(u8 *p, u32 off, u32 v)
{
    p[off] = (u8)(v >> 8);
    p[off + 1u] = (u8)v;
}

static u32 tri(u32 a, u32 b, u32 c)
{
    return (a * 2u << 16) | (b * 2u << 8) | (c * 2u);
}

static u32 vtxCmd(u32 n, u32 v0)
{
    return ((u32)F3_OP_VTX << 24) | (n << 12) | ((v0 + n) << 1);
}

void f3SelfTest(F3Test *t)
{
    static u8 buf[BUF_SIZE] __attribute__((aligned(8)));
    F3State st;
    F3State bst;
    F3Vtx *v = (F3Vtx *)(buf + OFF_VTX);
    u32 *p;
    u32 runs;
    u32 us;
    u32 i;

    memset(t, 0, sizeof(*t));
    memset(buf, 0, sizeof(buf));

    memset(v, 0, 3 * sizeof(F3Vtx));
    v[0].x = 100;
    v[0].y = -200;
    v[0].z = 300;
    v[0].flag = 0xFFFF;
    v[0].s = 10;
    v[0].t = 20;
    v[0].r = 0xFF;
    v[0].g = 0x00;
    v[0].b = 0x11;
    v[0].a = 0xFF;
    v[1].x = 400;
    v[2].x = -400;

    p = (u32 *)(buf + OFF_MTX);
    for (i = 0; i < 16; i++) {
        p[i] = 0x1000u + i;
    }

    p = (u32 *)(buf + OFF_SUB);
    put(&p, 0x05040200u, 0);                    /* TRI1(2,1,0) */
    put(&p, (u32)F3_OP_ENDDL << 24, 0);

    p = (u32 *)(buf + OFF_MAIN);
    put(&p, vtxCmd(3, 0), segAddr(OFF_VTX));
    put(&p, ((u32)F3_OP_MTX << 24) | (7u << 19) | 0x02u, segAddr(OFF_MTX));
    put(&p, ((u32)F3_OP_GEOMETRYMODE << 24) | 0xFFFFFFu, 0x00000001u);
    put(&p, ((u32)F3_OP_TEXTURE << 24) | (1u << 1), 0x80008000u);
    put(&p, 0xFD000000u, segAddr(OFF_VTX));     /* SETTIMG */
    put(&p, 0xF5000000u, 0);                    /* SETTILE */
    put(&p, 0xF2000000u, 0);                    /* SETTILESIZE */
    put(&p, 0xF3000000u, 0);                    /* LOADBLOCK */
    put(&p, 0xFC000000u, 0);                    /* SETCOMBINE */
    put(&p, 0xFA000000u, 0xFFFFFFFFu);          /* SETPRIMCOLOR */
    put(&p, ((u32)F3_OP_TRI1 << 24) | tri(0, 1, 2), 0);
    put(&p, ((u32)F3_OP_TRI2 << 24) | tri(0, 1, 2), tri(0, 2, 1));
    put(&p, (u32)F3_OP_DL << 24, segAddr(OFF_SUB));      /* G_DL_PUSH: call */
    put(&p, (u32)F3_OP_POPMTX << 24, 64);
    put(&p, 0xF7000000u, 0);                    /* G_SETFILLCOLOR */
    put(&p, 0xE6000000u, 0);                    /* G_RDPLOADSYNC */
    put(&p, 0xE7000000u, 0);                    /* G_RDPPIPESYNC */
    put(&p, 0xE8000000u, 0);                    /* G_RDPTILESYNC */
    put(&p, 0xE9000000u, 0);                    /* G_RDPFULLSYNC */
    put(&p, (u32)F3_OP_ENDDL << 24, 0);

    f3Init(&st);
    f3SetSegment(&st, 6, buf);

    {
        bool le = f3Run(&st, buf + OFF_MAIN) &&
                  st.err == F3_ERR_NONE &&
                  st.unsupported == 0 &&
                  st.triCount == 4 &&
                  st.vtxCmds == 1 &&
                  st.mtxPush == 1 &&
                  st.mtxPop == 1 &&
                  st.mtxDepth == 0 &&
                  st.rdpOps == 11 &&
                  st.texChanges == 1 &&
                  st.imgChanges == 1 &&
                  st.geometryMode == 0x00000001u &&
                  st.vtx[0].x == 100 &&
                  st.vtx[0].z == 300 &&
                  st.vtx[0].a == 0xFF &&
                  st.vtx[1].x == 400 &&
                  st.vtx[2].x == -400;

        /* Which block failed matters more than that one did: the checks
         * without their own bitmask share nothing but this flag. */
        t->stageBad |= le ? 0u : 1u;
        t->ok = le;
    }

    t->triCount = st.triCount;
    t->vtxCmds = st.vtxCmds;
    t->mtxPush = st.mtxPush;
    t->mtxPop = st.mtxPop;
    t->rdpOps = st.rdpOps;
    t->texChanges = st.texChanges;
    t->imgChanges = st.imgChanges;
    t->err = st.err;
    t->unsupported = st.unsupported;

    /* G_MOVEMEM VIEWPORT plus the three combiner shapes the DS can express.
     * The words are the ones gDPSetCombineLERP emits for MODULATEIA,
     * DECALRGBA and SHADE under F3DEX_GBI_2. */
    {
        s16 *vp = (s16 *)(buf + OFF_VPT);

        vp[0] = 640; vp[1] = 360; vp[2] = 1023; vp[3] = 0;
        vp[4] = 640; vp[5] = 480; vp[6] = 0;    vp[7] = 0;

        p = (u32 *)(buf + OFF_MAIN);
        put(&p, ((u32)F3_OP_MOVEMEM << 24) | (1u << 19) | 8u, segAddr(OFF_VPT));
        put(&p, 0xFC121824u, 0xFF33FFFFu);          /* MODULATEIA */
        put(&p, 0xFCFFFFFFu, 0xFFFCF279u);          /* DECALRGBA */
        put(&p, 0xFCFFFFFFu, 0xFFFE793Cu);          /* SHADE */
        put(&p, 0xFC000000u, 0u);                   /* unclassifiable */
        put(&p, (u32)F3_OP_ENDDL << 24, 0);

        sCcSeen = 0;
        sVpSeen = 0;
        f3Reset(&st);
        f3SetCCHook(&st, ccProbe, NULL);
        f3SetVpHook(&st, vpProbe, NULL);
        {
            bool vpc = f3Run(&st, buf + OFF_MAIN) &&
                       st.err == F3_ERR_NONE &&
                       st.vpLoads == 1 && st.ccChanges == 4 &&
                       sVpSeen == 1 && sCcSeen == 4 &&
                       st.vp.scale[0] == 640 && st.vp.trans[1] == 480 &&
                       sCcMode[0] == F3_CC_MODULATE && sCcAlpha[0] == F3_AC_SHADE &&
                       sCcMode[1] == F3_CC_DECAL && sCcAlpha[1] == F3_AC_TEXEL0 &&
                       sCcMode[2] == F3_CC_SHADE && sCcAlpha[2] == F3_AC_SHADE &&
                       sCcMode[3] == F3_CC_OTHER && sCcAlpha[3] == F3_AC_COMBINED;

            t->stageBad |= vpc ? 0u : 2u;
            t->ok = t->ok && vpc;
        }
        f3SetCCHook(&st, NULL, NULL);
        f3SetVpHook(&st, NULL, NULL);
    }

    /* OtherMode. The words are what gsDPSetRenderMode / gsDPSetTextureLUT /
     * gsDPSetAlphaCompare / gsDPSetCycleType emit under F3DEX_GBI_2, i.e.
     * w0 = cmd | (32 - sft - len) << 8 | (len - 1) with the payload already
     * shifted by gbi.h's G_MDSFT_*. The render mode values are
     * G_RM_AA_ZB_XLU_SURF|SURF2 and G_RM_AA_ZB_OPA_SURF|SURF2. */
    {
        u32 bad;
        bool run;

        p = (u32 *)(buf + OFF_MAIN);
        put(&p, 0xE200001Cu, 0x005049D8u);     /* L: XLU surface, blend on */
        put(&p, 0xE3001001u, 0x00008000u);     /* H: texture LUT RGBA16 */
        put(&p, 0xE2001E01u, 0x00000001u);     /* L: alpha compare threshold */
        put(&p, 0xE200001Cu, 0x00552078u);     /* L: OPA surface, blend off */
        put(&p, 0xE2001E01u, 0x00000001u);     /* L: 2-bit write keeps the rest */
        put(&p, 0xE3000A01u, 0x00100000u);     /* H: cycle type 2CYCLE */
        put(&p, (u32)F3_OP_ENDDL << 24, 0);

        sOmSeen = 0;
        f3Reset(&st);
        f3SetOmHook(&st, omProbe, NULL);
        run = f3Run(&st, buf + OFF_MAIN);
        bad = 0;
        if (!run || st.err != F3_ERR_NONE || st.unsupported != 0) {
            bad |= 1u;
        }
        if (st.omChanges != 6 || sOmSeen != 6) {
            bad |= 2u;
        }
        if (sOmFlags[0] != F3_OM_XLU || sOmAc[0] != 0u) {
            bad |= 4u;
        }
        if (sOmFlags[1] != (F3_OM_XLU | F3_OM_TTALPHA)) {
            bad |= 8u;
        }
        if (sOmAc[2] != F3_ALPHA_CMP_THRESHOLD) {
            bad |= 16u;
        }
        /* The render-mode write is 29 bits wide starting at bit 3, so it
         * cannot clear the alpha compare set by the previous command: the
         * threshold survives, exactly as it does on the RDP. */
        if (sOmFlags[3] != F3_OM_TTALPHA ||
            sOmAc[3] != F3_ALPHA_CMP_THRESHOLD) {
            bad |= 32u;
        }
        if (sOmFlags[4] != F3_OM_TTALPHA ||
            sOmAc[4] != F3_ALPHA_CMP_THRESHOLD) {
            bad |= 64u;
        }
        if (sOmFlags[5] != (F3_OM_TTALPHA | F3_OM_2CYC)) {
            bad |= 128u;
        }
        if (st.om.lo != 0x00552079u || st.om.hi != 0x00108000u) {
            bad |= 256u;
        }
        if (st.omFlags != (F3_OM_TTALPHA | F3_OM_2CYC) ||
            st.omAc != F3_ALPHA_CMP_THRESHOLD) {
            bad |= 512u;
        }
        t->ok = t->ok && bad == 0;
        t->omBad = bad;
        t->omChanges = st.omChanges;
        f3SetOmHook(&st, NULL, NULL);
    }

    /* The shape of a real room's display lists, as measured across the one
     * hundred thirty-four normal rooms of ntsc-1.2: a root list that calls
     * sub-lists with gbi's G_DL_PUSH (param zero, not one), eighty-vertex
     * batches, RDP syncs in the thousands, and a handful of commands pointing
     * at segments the port has not loaded. None of those may cost the room. */
    {
        F3Vtx *big = (F3Vtx *)(buf + OFF_BIG);
        u32 *sub;
        u32 *sub2;
        u32 bad;

        for (i = 0; i < F3_VTX_MAX; i++) {
            big[i].x = (s16)i;
            big[i].a = 0xFF;
        }

        sub = (u32 *)(buf + OFF_SUB);
        put(&sub, 0x05040200u, 0);                       /* TRI1(2,1,0) */
        put(&sub, (u32)F3_OP_ENDDL << 24, 0);

        sub2 = (u32 *)(buf + OFF_SUB2);
        put(&sub2, ((u32)F3_OP_TRI2 << 24) | tri(1, 2, 3), tri(4, 5, 6));
        put(&sub2, (u32)F3_OP_ENDDL << 24, 0);

        p = (u32 *)(buf + OFF_MAIN);
        put(&p, vtxCmd(F3_VTX_MAX, 0), segAddr(OFF_BIG));
        put(&p, (u32)F3_OP_DL << 24, segAddr(OFF_SUB));  /* push: come back */
        put(&p, vtxCmd(96, 0), segAddr(OFF_BIG));        /* beyond the cache */
        put(&p, 0xFD000000u, 0x04000010u);               /* SETTIMG, seg 4 gone */
        put(&p, ((u32)F3_OP_TRI1 << 24) | tri(79, 78, 77), 0);
        put(&p, ((u32)F3_OP_DL << 24) | (1u << 16), segAddr(OFF_SUB2));
        put(&p, ((u32)F3_OP_TRI1 << 24) | tri(0, 1, 2), 0); /* not reached */
        put(&p, (u32)F3_OP_ENDDL << 24, 0);

        f3Reset(&st);
        bad = 0;
        if (!f3Run(&st, buf + OFF_MAIN) || st.err != F3_ERR_NONE) {
            bad |= 1u;
        }
        /* one from the called sub-list, the parent's own, two from the list the
         * nopush jumped into, and none from after the jump. */
        if (st.triCount != 4u) {
            bad |= 2u;
        }
        if (st.vtxCmds != 1u || st.skipVtx != 1u || st.skipAddr != 1u ||
            st.skips != 2u) {
            bad |= 4u;
        }
        if (st.vtx[79].x != 79 || st.vtx[0].x != 0) {
            bad |= 8u;
        }
        if (st.tex.img != NULL || st.imgChanges != 0u) {
            bad |= 16u;
        }
        if (st.unsupported != 0u) {
            bad |= 32u;
        }
        t->ok = t->ok && bad == 0;
        t->realBad = bad;
    }

    /* A room file is big-endian, and this is the topology the room loader
     * actually uses: a native-order prologue calls into a segment marked with
     * f3SetSegmentBe. Commands and matrices swap as 32-bit words, vertices and
     * viewports as 16-bit fields, the vertex colour never swaps. */
    {
        u8 *be = buf + OFF_BE;
        u32 *bp;
        u32 bad = 0;
        u32 naive;
        bool run;

        putBe16(be, BE_VTX + 0u, 0x1234u);   /* x */
        putBe16(be, BE_VTX + 2u, 0x89ABu);   /* y, negative once read as s16 */
        putBe16(be, BE_VTX + 4u, 100u);      /* z */
        putBe16(be, BE_VTX + 6u, 0xCDCDu);   /* flag */
        putBe16(be, BE_VTX + 8u, 0xFF00u);   /* s */
        putBe16(be, BE_VTX + 10u, 0x0100u);  /* t */
        be[BE_VTX + 12u] = 0x11;
        be[BE_VTX + 13u] = 0x22;
        be[BE_VTX + 14u] = 0x33;
        be[BE_VTX + 15u] = 0x44;
        for (i = 1; i < 3; i++) {
            putBe16(be, BE_VTX + i * 16u, 1u + i);
        }

        bp = (u32 *)(be + BE_MTX);
        for (i = 0; i < 16; i++) {
            bp[i] = (u32)__builtin_bswap32(0x1000u + i);
        }

        putBe16(be, BE_VPT + 0u, 640u);
        putBe16(be, BE_VPT + 2u, 360u);
        putBe16(be, BE_VPT + 4u, 1023u);
        putBe16(be, BE_VPT + 6u, 0u);
        putBe16(be, BE_VPT + 8u, 640u);
        putBe16(be, BE_VPT + 10u, 480u);
        putBe16(be, BE_VPT + 12u, 0u);
        putBe16(be, BE_VPT + 14u, 0u);

        bp = (u32 *)(be + BE_SUB);
        putBe(&bp, ((u32)F3_OP_TRI2 << 24) | tri(1, 2, 0), tri(0, 2, 1));
        putBe(&bp, (u32)F3_OP_ENDDL << 24, 0);

        bp = (u32 *)(be + BE_MAIN);
        putBe(&bp, vtxCmd(3, 0), beAddr(BE_VTX));
        /* gbi's gsSPMatrix stores its params as (p ^ G_MTX_PUSH), so a plain
         * G_MTX_LOAD reaches the RSP as 0x03 and the decoder XORs the PUSH bit
         * back out. This is the shape a real room's list carries. */
        putBe(&bp, ((u32)F3_OP_MTX << 24) | (7u << 19) | 0x03u, beAddr(BE_MTX));
        putBe(&bp, ((u32)F3_OP_MOVEMEM << 24) | (1u << 19) | 8u, beAddr(BE_VPT));
        putBe(&bp, ((u32)F3_OP_TRI1 << 24) | tri(0, 1, 2), 0);
        putBe(&bp, (u32)F3_OP_DL << 24, beAddr(BE_SUB));   /* push: come back */
        putBe(&bp, (u32)F3_OP_ENDDL << 24, 0);

        p = (u32 *)(buf + OFF_MAIN);
        put(&p, (u32)F3_OP_DL << 24, beAddr(BE_MAIN));
        put(&p, ((u32)F3_OP_TRI1 << 24) | tri(0, 1, 2), 0);
        put(&p, (u32)F3_OP_ENDDL << 24, 0);

        f3Init(&st);
        f3SetSegment(&st, 6, buf);
        f3SetSegment(&st, 5, be);
        f3SetSegmentBe(&st, 5, true);
        /* The word the vertex really starts with, byte-swapped as a whole word
         * instead of field by field: this is what a lazy swap would read as x. */
        naive = (u32)__builtin_bswap32(0x123489ABu);
        sMtxSeen = 0;
        sMtxPtr = NULL;
        run = f3Run(&st, buf + OFF_MAIN);
        if (!run || st.err != F3_ERR_NONE || st.unsupported != 0) {
            bad |= 1u;
        }
        /* one from the big-endian list, two from the list it called, one from
         * the prologue it returned to. */
        if (st.triCount != 4u) {
            bad |= 2u;
        }
        if (st.vtx[0].x != 0x1234 || st.vtx[0].z != 100 ||
            st.vtx[0].flag != 0xCDCDu || st.vtx[0].s != (s16)0xFF00 ||
            st.vtx[0].t != 0x0100 || st.vtx[2].x != 3) {
            bad |= 4u;
        }
        if (st.vtx[0].y != (s16)0x89AB) {
            bad |= 8u;
        }
        if (st.vtx[0].r != 0x11 || st.vtx[0].g != 0x22 ||
            st.vtx[0].b != 0x33 || st.vtx[0].a != 0x44) {
            bad |= 16u;
        }
        /* The proof that the field-by-field rule matters: 0x3412 is what a
         * whole-word swap would have read as x, so if the two ever agreed this
         * case would prove nothing. */
        if ((s16)naive != (s16)0x3412 || (s16)naive == st.vtx[0].x) {
            bad |= 32u;
        }
        if (st.vp.scale[0] != 640 || st.vp.trans[1] != 480 ||
            st.vpLoads != 1u) {
            bad |= 64u;
        }
        f3Reset(&st);
        if (((st.segBe >> 5) & 1u) == 0 || st.seg[5] != be) {
            bad |= 128u;
        }
        f3SetMtxHook(&st, mtxProbe, NULL);
        if (!f3Run(&st, buf + OFF_MAIN)) {
            bad |= 256u;
        }
        /* The matrix reaches the hook as the state's own shadow, already in the
         * host's order, because that is the only copy a big-endian segment has. */
        if (sMtxSeen != 1u || sMtxPtr != st.mtx || sMtxArgs != F3_MTX_LOAD) {
            bad |= 512u;
        }
        for (i = 0; i < 16; i++) {
            if (sMtxWords[i] != 0x1000u + i) {
                bad |= 1024u;
            }
        }
        /* The params byte survived the swap, so no push happened. */
        if (st.mtxPush != 0u || st.mtxDepth != 0u) {
            bad |= 2048u;
        }
        t->ok = t->ok && bad == 0;
        t->beBad = bad;
    }

    p = (u32 *)(buf + OFF_MAIN);
    p[0] = 0x42000000u;
    p[1] = 0;
    p[2] = (u32)F3_OP_ENDDL << 24;
    p[3] = 0;
    f3Reset(&st);
    {
        bool tail = f3Run(&st, buf + OFF_MAIN) &&
                    st.unsupported == 1 && st.lastOpcode == 0x42u;

        /* One past the cache: the index that used to be the first bad one at
         * thirty-two is legal now that F3DEX2's whole eighty-vertex cache fits. */
        p[0] = ((u32)F3_OP_TRI1 << 24) | tri(F3_VTX_MAX, 1, 2);
        f3Reset(&st);
        tail = tail && !f3Run(&st, buf + OFF_MAIN) &&
               st.err == F3_ERR_TRI_INDEX;

        p[0] = (u32)F3_OP_POPMTX << 24;
        p[1] = 64;
        f3Reset(&st);
        tail = tail && !f3Run(&st, buf + OFF_MAIN) &&
               st.err == F3_ERR_MTX_STACK;

        t->stageBad |= tail ? 0u : 4u;
        t->ok = t->ok && tail;
    }

    p = (u32 *)(buf + OFF_BENCH);
    put(&p, vtxCmd(3, 0), segAddr(OFF_VTX));
    for (i = 0; i < BENCH_TRIS / 2; i++) {
        put(&p, ((u32)F3_OP_TRI2 << 24) | tri(0, 1, 2), tri(0, 2, 1));
    }
    put(&p, (u32)F3_OP_ENDDL << 24, 0);

    f3Init(&bst);
    f3SetSegment(&bst, 6, buf);

    runs = BENCH_RUNS;
    us = 0;
    {
        u32 t0 = timeTicks();

        for (i = 0; i < runs; i++) {
            f3Reset(&bst);
            f3Run(&bst, buf + OFF_BENCH);
        }
        us = timeTicksToUs(timeTicks() - t0);
    }

    t->dlUs = runs ? us / runs : 0;
    {
        bool bench = bst.err == F3_ERR_NONE &&
                     bst.triCount == BENCH_TRIS &&
                     bst.unsupported == 0;

        t->stageBad |= bench ? 0u : 8u;
        t->ok = t->ok && bench;
    }
}
