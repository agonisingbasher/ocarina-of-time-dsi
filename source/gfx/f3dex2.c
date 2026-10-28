#include "f3dex2.h"

#include <string.h>

#define CMD_LIMIT 65536u

static const u8 *resolveSeg(F3State *st, u32 addr)
{
    u32 seg = addr >> 24;
    const u8 *base;

    if (seg > 15u) {
        /* A real room's own lists never do this, but a list walked out of
         * position can carry anything in its top byte. */
        st->skipAddr++;
        return NULL;
    }
    base = st->seg[seg];
    if (!base) {
        /* Not an error: a real room reaches into segments the port has not
         * loaded, and dropping that one command keeps the rest of the room. */
        st->skipAddr++;
        return NULL;
    }
    return base + (addr & 0xFFFFFFu);
}

static bool segBe(F3State *st, u32 addr)
{
    return ((st->segBe >> ((addr >> 24) & 15u)) & 1u) != 0;
}

static bool triIndex(F3State *st, u32 idx)
{
    if (idx >= F3_VTX_MAX) {
        st->err = F3_ERR_TRI_INDEX;
        return false;
    }
    return true;
}

/* The N64's Vtx is six 16-bit fields then four bytes of colour, and its Vp is
 * eight 16-bit fields, so a big-endian one is read two bytes at a time. Swapping
 * whole words instead would land y in x's slot: the pair sits in one word and a
 * word swap reverses the order of the two halfwords as well as their bytes. */
static void copyBe16(void *dst, const u8 *src, u32 bytes)
{
    u16 *d = (u16 *)dst;
    u32 i;

    for (i = 0; i < bytes / 2u; i++) {
        d[i] = (u16)(((u32)src[i * 2u] << 8) | src[i * 2u + 1u]);
    }
}

static void copyVtx(F3Vtx *dst, const u8 *src, u32 n, bool be)
{
    u32 i;

    if (!be) {
        memcpy(dst, src, n * sizeof(F3Vtx));
        return;
    }
    for (i = 0; i < n; i++) {
        copyBe16(dst + i, src + i * sizeof(F3Vtx), 12);
        memcpy(&dst[i].r, src + i * sizeof(F3Vtx) + 12, 4);
    }
}

/* Mtx is sixteen 16.16 words, so a big-endian one swaps whole words. */
static void copyMtx(u8 *dst, const u8 *src, bool be)
{
    u32 i;

    if (!be) {
        memcpy(dst, src, F3_MTX_SIZE);
        return;
    }
    for (i = 0; i < F3_MTX_SIZE / 4u; i++) {
        ((u32 *)dst)[i] = __builtin_bswap32(((const u32 *)src)[i]);
    }
}

/* Vp is eight s16 fields, so it swaps the way a vertex does. */
static void copyVp(F3Vp *dst, const u8 *src, bool be)
{
    if (be) {
        copyBe16(dst, src, sizeof(*dst));
        return;
    }
    memcpy(dst, src, sizeof(*dst));
}

/* The RDP pass-through opcodes, taken from include/ultra64/gbi.h rather than
 * from the hardware manual: a real OoT display list carries gbi's numbers, and
 * the hardware's own 0xE4-0xE7 codes (SetCombine, SetTile, SetFillColor,
 * FillRect) never appear. Getting these wrong cost more than a name: walking
 * the one hundred thirty-four normal rooms of ntsc-1.2 counts nine thousand
 * five hundred thirty-four G_RDPPIPESYNC and four thousand five hundred
 * seventy-three G_RDPLOADSYNC, and while they were unclassified every one of
 * them was tallied as unsupported. */
static bool isRdpOp(u32 op)
{
    switch (op) {
    case 0xFF: /* G_SETCIMG */
    case 0xFE: /* G_SETZIMG */
    case 0xFD: /* G_SETTIMG */
    case 0xFC: /* G_SETCOMBINE */
    case 0xFB: /* G_SETENVCOLOR */
    case 0xFA: /* G_SETPRIMCOLOR */
    case 0xF9: /* G_SETBLENDCOLOR */
    case 0xF8: /* G_SETFOGCOLOR */
    case 0xF7: /* G_SETFILLCOLOR */
    case 0xF6: /* G_FILLRECT */
    case 0xF5: /* G_SETTILE */
    case 0xF4: /* G_LOADTILE */
    case 0xF3: /* G_LOADBLOCK */
    case 0xF2: /* G_SETTILESIZE */
    case 0xF0: /* G_LOADTLUT */
    case 0xEF: /* G_RDPSETOTHERMODE */
    case 0xEE: /* G_SETPRIMDEPTH */
    case 0xED: /* G_SETSCISSOR */
    case 0xEC: /* G_SETCONVERT */
    case 0xEB: /* G_SETKEYR */
    case 0xEA: /* G_SETKEYGB */
    case 0xE9: /* G_RDPFULLSYNC */
    case 0xE8: /* G_RDPTILESYNC */
    case 0xE7: /* G_RDPPIPESYNC */
    case 0xE6: /* G_RDPLOADSYNC */
    case 0xE5: /* G_TEXRECTFLIP */
    case 0xE4: /* G_TEXRECT */
        return true;
    default:
        return false;
    }
}

/* G_SETCOMBINE carries a two-cycle mux network. The GE has one fixed
 * texel*shade stage, so only the cycle-0 colour muxes are classified, and the
 * 4-bit a0/b0 fields truncate G_CCMUX_0 (31) to 15. */
static void decodeCC(F3State *st, u32 w0, u32 w1)
{
    u32 a0 = (w0 >> 20) & 0xFu;
    u32 c0 = (w0 >> 15) & 0x1Fu;
    u32 Ac0 = (w0 >> 9) & 0x7u;
    u32 b0 = (w1 >> 28) & 0xFu;
    u32 d0 = (w1 >> 15) & 0x7u;
    u32 Ad0 = (w1 >> 9) & 0x7u;
    u32 mode;

    if (a0 == 1 && b0 == 0xF && c0 == 4) {
        mode = F3_CC_MODULATE; /* TEXEL0 * SHADE */
    } else if (a0 == 0xF && b0 == 0xF && c0 == 0x1F && d0 == 4) {
        mode = F3_CC_SHADE; /* vertex colour only */
    } else if (a0 == 0xF && b0 == 0xF && c0 == 0x1F && d0 == 1) {
        mode = F3_CC_DECAL; /* TEXEL0 only */
    } else if (a0 == 2 && b0 == 1 && c0 == 12 && d0 == 1) {
        /* (TEXEL1 - TEXEL0) * ENVA + TEXEL0. The cycle type is 1-cycle for
         * every triangle in the 359 extracted rooms, so cycle 0 is the whole
         * output and the second half of this command has no effect. */
        mode = F3_CC_LERP;
    } else {
        mode = F3_CC_OTHER;
    }

    st->cc.mode = mode;
    st->cc.env = st->env;
    st->cc.envValid = st->envValid;
    /* Ad0 is TEXEL0 or SHADE for the clean modes; when it is the previous
     * cycle (0) or the constant 0 the cycle-0 alpha comes from the Ac0/Aa0
     * pair instead (e.g. MODULATEIA). */
    st->cc.alpha = (Ad0 == F3_AC_COMBINED || Ad0 == F3_AC_0) ? Ac0 : Ad0;
    st->ccChanges++;
    st->rdpOps++;
    if (st->ccHook) {
        st->ccHook(st->ccCtx, &st->cc);
    }
}

/* Field positions inside each half, as gbi.h pre-shifts them:
 * G_MDSFT_CYCLETYPE 20 (2 bits), G_MDSFT_TEXTLUT 14 (2), G_MDSFT_TEXTFILT 12
 * (2), G_MDSFT_COMBKEY 8 (1) in the hi half; G_MDSFT_ALPHACOMPARE 0 (2), the
 * render mode's ZMODE pair at 10-11 and FORCE_BL at 14 in the lo half. */
#define OM_H_CYCLE   0x00300000u
#define OM_H_TT      0x00018000u
#define OM_H_FILT    0x00006000u
#define OM_H_KEY     0x00000100u
#define OM_L_AC      0x00000003u
#define OM_L_ZMODE   0x00000C00u
#define OM_L_FORCEBL 0x00004000u
/* GBL_c1's third argument: the blender's second colour. G_BL_CLR_MEM is the
 * only value that pulls the frame buffer back through the blender, so with
 * FORCE_BL it is what actually makes a primitive translucent. */
#define OM_L_BLEND   0x00C00000u
#define OM_L_BLEND_MEM 0x00400000u /* OM_L_BLEND set to G_BL_CLR_MEM (1) */

static u32 omFlags(const F3Om *om)
{
    u32 flags = 0;

    /* Any ZMODE other than OPA leaves the previous pixel in the frame buffer,
     * so it needs the DS's polygon alpha rather than a plain write.
     *
     * With ZMODE_OPA the blender is bypassed unless FORCE_BL asks for it, and
     * gbi.h puts FORCE_BL in RM_OPA_SURF, so FORCE_BL alone cannot be the test:
     * every non-Z-buffered opaque surface of the game carried it and would be
     * classified translucent. What separates RM_OPA_SURF from RM_XLU_SURF is
     * the blender's second colour, which only RM_XLU_SURF sets to CLR_MEM.
     * Reading FORCE_BL on its own was why the game's own list submitted four
     * triangles and painted none. */
    if ((om->lo & OM_L_ZMODE) != 0 ||
        ((om->lo & OM_L_FORCEBL) != 0 &&
         (om->lo & OM_L_BLEND) == OM_L_BLEND_MEM)) {
        flags |= F3_OM_XLU;
    }
    if ((om->lo & OM_L_ZMODE) == OM_L_ZMODE) {
        flags |= F3_OM_DECAL;
    }
    if ((om->hi & OM_H_CYCLE) != 0) {
        flags |= F3_OM_2CYC;
    }
    if ((om->hi & OM_H_TT) != 0) {
        flags |= F3_OM_TTALPHA;
    }
    if ((om->hi & OM_H_FILT) != 0) {
        flags |= F3_OM_FILT;
    }
    if ((om->hi & OM_H_KEY) != 0) {
        flags |= F3_OM_KEY;
    }
    return flags;
}

/* G_SETOTHERMODE_H/L write len bits of one half, the payload already shifted
 * by gbi.h's G_MDSFT_*: word0 carries (32 - sft - len) in bits 8-15 and
 * (len - 1) in bits 0-7 under F3DEX_GBI_2. */
static void decodeOm(F3State *st, u32 op, u32 w0, u32 w1)
{
    u32 len = (w0 & 0xFFu) + 1u;
    u32 sft = 32u - ((w0 >> 8) & 0xFFu) - len;
    bool hi = op == F3_OP_SETOTHERMODE_H;
    u32 *dst = hi ? &st->om.hi : &st->om.lo;
    u32 mask;

    if (sft > 31u || len > 32u || sft + len > 32u) {
        st->unsupported++;
        st->lastOpcode = op;
        return;
    }

    mask = (len == 32u) ? 0xFFFFFFFFu : (((1u << len) - 1u) << sft);
    *dst = (*dst & ~mask) | (w1 & mask);

    st->omFlags = omFlags(&st->om);
    st->omAc = st->om.lo & OM_L_AC;
    st->omChanges++;
    if (st->omHook) {
        st->omHook(st->omCtx, &st->om, st->omFlags, st->omAc);
    }
}

static bool runList(F3State *st, const u8 *dl, u32 depth, bool be)
{
    u32 guard = CMD_LIMIT;

    if (depth > F3_DL_DEPTH) {
        st->err = F3_ERR_DL_DEPTH;
        return false;
    }

    while (guard--) {
        const u32 *w = (const u32 *)dl;
        u32 w0 = be ? (u32)__builtin_bswap32(w[0]) : w[0];
        u32 w1 = be ? (u32)__builtin_bswap32(w[1]) : w[1];
        u32 op = w0 >> 24;

        st->ops++;

        switch (op) {
        case F3_OP_SPNOOP:
        case F3_OP_RDPHALF_1:
        case F3_OP_RDPHALF_2:
            break;

        case F3_OP_SETOTHERMODE_L:
        case F3_OP_SETOTHERMODE_H:
            decodeOm(st, op, w0, w1);
            break;

        case F3_OP_VTX: {
            u32 v0n = (w0 >> 1) & 0x7Fu;
            u32 n = (w0 >> 12) & 0xFFu;
            const u8 *src;
            u32 v0;

            if (n == 0 || n > F3_VTX_MAX || v0n < n || v0n > F3_VTX_MAX) {
                st->skipVtx++;
                st->skips++;
                break;
            }
            v0 = v0n - n;
            src = resolveSeg(st, w1);
            if (!src) {
                st->skips++;
                break;
            }
            copyVtx(&st->vtx[v0], src, n, segBe(st, w1));
            st->vtxCmds++;
            break;
        }

        case F3_OP_MODIFYVTX:
        case F3_OP_CULLDL:
        case F3_OP_BRANCH_Z:
        case F3_OP_LINE3D:
            break;

        case F3_OP_TRI1: {
            u32 a = ((w0 >> 16) & 0xFFu) >> 1;
            u32 b = ((w0 >> 8) & 0xFFu) >> 1;
            u32 c = (w0 & 0xFFu) >> 1;

            if (!triIndex(st, a) || !triIndex(st, b) || !triIndex(st, c)) {
                return false;
            }
            st->triCount++;
            if (st->triHook) {
                st->triHook(st->triCtx, st->vtx, a, b, c);
            }
            break;
        }

        /* F3DEX2's G_QUAD is two triangles packed exactly like G_TRI2: the
         * gSP1Quadrangle flag has already chosen which of the four vertices go
         * into each word, so only the opcode differs. */
        case F3_OP_QUAD:
        case F3_OP_TRI2: {
            u32 a = ((w0 >> 16) & 0xFFu) >> 1;
            u32 b = ((w0 >> 8) & 0xFFu) >> 1;
            u32 c = (w0 & 0xFFu) >> 1;
            u32 d = ((w1 >> 16) & 0xFFu) >> 1;
            u32 e = ((w1 >> 8) & 0xFFu) >> 1;
            u32 f = (w1 & 0xFFu) >> 1;

            if (!triIndex(st, a) || !triIndex(st, b) || !triIndex(st, c) ||
                !triIndex(st, d) || !triIndex(st, e) || !triIndex(st, f)) {
                return false;
            }
            st->triCount += 2;
            if (st->triHook) {
                st->triHook(st->triCtx, st->vtx, a, b, c);
                st->triHook(st->triCtx, st->vtx, d, e, f);
            }
            break;
        }

        case F3_OP_MTX: {
            const u8 *src;
            u32 len = (((w0 >> 19) & 0x1Fu) + 1u) * 8u;
            /* gSPMatrix emits (p) ^ G_MTX_PUSH, so the push bit arrives
             * inverted in the stream; undo it to get the caller's flags. */
            u32 params = (w0 & 0xFFu) ^ F3_MTX_PUSH;

            if (len != F3_MTX_SIZE) {
                st->err = F3_ERR_MTX_SIZE;
                return false;
            }
            src = resolveSeg(st, w1);
            if (!src) {
                st->skips++;
                break;
            }
            if (params & F3_MTX_PUSH) {
                if (st->mtxDepth >= F3_MTX_STACK) {
                    st->err = F3_ERR_MTX_STACK;
                    return false;
                }
                memcpy(st->mtxStack[st->mtxDepth], st->mtx, F3_MTX_SIZE);
                st->mtxDepth++;
                st->mtxPush++;
            }
            copyMtx(st->mtx, src, segBe(st, w1));
            /* The hook gets the shadow, not the segment: for a big-endian
             * matrix that is the only copy in the host's order, and the
             * geometry engine converts it right there rather than storing the
             * pointer. */
            if (st->mtxHook) {
                st->mtxHook(st->mtxCtx, F3_MTX_EV_LOAD, st->mtx, params);
            }
            break;
        }

        case F3_OP_POPMTX: {
            /* gSPPopMatrixN puts the number of bytes (64 * n) in w1. */
            u32 n;

            if (w1 % F3_MTX_SIZE) {
                st->err = F3_ERR_MTX_SIZE;
                return false;
            }
            n = w1 / F3_MTX_SIZE;
            if (n > st->mtxDepth) {
                st->err = F3_ERR_MTX_STACK;
                return false;
            }
            if (n == 0) {
                break;
            }
            st->mtxDepth -= n;
            memcpy(st->mtx, st->mtxStack[st->mtxDepth], F3_MTX_SIZE);
            st->mtxPop += n;
            if (st->mtxHook) {
                st->mtxHook(st->mtxCtx, F3_MTX_EV_POP, st->mtx, n);
            }
            break;
        }

        case F3_OP_GEOMETRYMODE: {
            u32 clearMask = w0 & 0xFFFFFFu;

            st->geometryMode = (st->geometryMode & clearMask) |
                               (w1 & 0xFFFFFFu);
            if (st->geomHook) {
                st->geomHook(st->geomCtx, st->geometryMode);
            }
            break;
        }

        case F3_OP_TEXTURE:
            st->texOn = (w0 >> 1) & 0x7Fu;
            st->texTile = (w0 >> 8) & 0x7u;
            st->tex.on = st->texOn;
            st->tex.tile = st->texTile;
            st->texChanges++;
            if (st->texHook) {
                st->texHook(st->texCtx, st->texOn ? F3_TEX_EV_ON : F3_TEX_EV_OFF,
                            &st->tex);
            }
            break;

        case 0xFB: /* G_SETENVCOLOR */
            /* Only the alpha byte reaches the combiner, and only through an
             * ENVA mux, which in practice means the two-image lerp. Kept
             * whole so the value a list wrote can be reported as written. */
            st->env = w1;
            st->envValid = 1u;
            st->rdpOps++;
            break;

        case 0xF5: { /* G_SETTILE */
            u32 tile = (w1 >> 24) & 0x7u;

            if (tile == 0) { /* G_TX_RENDERTILE */
                st->tex.fmt = (w0 >> 21) & 0x7u;
                st->tex.siz = (w0 >> 19) & 0x3u;
                st->tex.line = (w0 >> 9) & 0x1FFu;
                st->tex.tmem = w0 & 0x1FFu;
                st->tex.palette = (w1 >> 20) & 0xFu;
            } else if (tile == 1) { /* the second tile of a two-image material */
                st->tex1.fmt = (w0 >> 21) & 0x7u;
                st->tex1.siz = (w0 >> 19) & 0x3u;
                st->tex1.line = (w0 >> 9) & 0x1FFu;
                st->tex1.tmem = w0 & 0x1FFu;
                st->tex1.palette = (w1 >> 20) & 0xFu;
            } else if (tile == 7) { /* G_TX_LOADTILE */
                /* The TLUT load's tile carries the palette slot in TMEM. */
                st->loadTmem = w0 & 0x1FFu;
            }
            st->rdpOps++;
            break;
        }

        case 0xF2: { /* G_SETTILESIZE */
            u32 tile = (w1 >> 24) & 0x7u;
            u32 uls = (w0 >> 12) & 0xFFFu;
            u32 ult = w0 & 0xFFFu;
            u32 lrs = (w1 >> 12) & 0xFFFu;
            u32 lrt = w1 & 0xFFFu;

            st->rdpOps++;
            if (lrs < uls || lrt < ult) {
                break;
            }
            if (tile == 1) {
                /* The second tile takes the image G_SETTIMG last pointed at,
                 * exactly as the first one does, and has no deferred-palette
                 * path: a CI image whose TLUT has not arrived yet leaves this
                 * tile not ready, which costs the material its second pass and
                 * never costs it the first. */
                const F3Tlut *tl;

                st->tex1.img = st->tex.img;
                st->tex1.imgBe = st->tex.imgBe;
                st->tex1.imgFmt = st->tex.imgFmt;
                st->tex1.imgSiz = st->tex.imgSiz;
                st->tex1.imgWidth = st->tex.imgWidth;
                st->tex1.slot = 1u;
                st->tex1.opaque = 1u;
                st->tex1.w = ((lrs - uls) >> 2) + 1u;
                st->tex1.h = ((lrt - ult) >> 2) + 1u;
                /* gDPLoadTextureBlock ends with G_SETTILESIZE, so the word
                 * count of the load that filled this tile is already in the
                 * slot the block wrote. Without it the bake refuses the image
                 * as a bad extent (reason 5) and the material silently loses
                 * its second pass. */
                st->tex1.loadWords = st->tex.loadWords;
                tl = &st->tluts[st->tex1.palette & 15u];
                st->tex1.tlut = tl->img;
                st->tex1.tlutCount = tl->count;
                st->tex1.tlutBe = tl->be;
                st->tex1.ready = st->tex1.img != NULL &&
                                 (st->tex1.fmt != 2u || st->tex1.tlut != NULL);
                if (st->tex1.ready) {
                    st->tex1Loads++;
                    if (st->texHook) {
                        st->texHook(st->texCtx, F3_TEX_EV_READY, &st->tex1);
                    }
                }
                break;
            }
            if (tile != 0) {
                break;
            }
            st->tex.slot = 0u;
            st->tex.w = ((lrs - uls) >> 2) + 1u;
            st->tex.h = ((lrt - ult) >> 2) + 1u;
            {
                const F3Tlut *tl = &st->tluts[st->tex.palette & 15u];

                st->tex.tlut = tl->img;
                st->tex.tlutCount = tl->count;
                st->tex.tlutBe = tl->be;
            }
            st->tex.ready = st->tex.img != NULL;
            /* A paletted tile has nothing to show until its palette is in TMEM,
             * and a room's lists put G_LOADTLUT after this command, so the
             * first texture of a chunk is described before the table has any
             * entry for it. Hold the event; the load below fires it. */
            st->texWaitTlut = (st->tex.ready && st->tex.fmt == 2 &&
                               st->tex.tlut == NULL) ? 1u : 0u;
            if (st->tex.ready && !st->texWaitTlut && st->texHook) {
                st->texHook(st->texCtx, F3_TEX_EV_READY, &st->tex);
            }
            break;
        }

        case 0xF3: /* G_LOADBLOCK */
            st->tex.loadWords = ((w1 >> 12) & 0xFFFu) + 1u;
            /* Whatever the render tile samples, it samples from this image:
             * the palette load that follows moves tex.img on, and the room's
             * own lists describe the tile after that. */
            st->blkImg = st->tex.img;
            st->blkImgBe = st->tex.imgBe;
            st->texWaitTlut = 0u;
            st->texLoads++;
            st->rdpOps++;
            break;

        case 0xF4: /* G_LOADTILE */
            st->rdpOps++;
            break;

        case 0xF0: { /* G_LOADTLUT */
            /* gDPLoadTLUTCmd puts (count - 1) in bits 14-23; the palette
             * source is whatever G_SETTIMG pointed at when the load ran. */
            u32 count = ((w1 >> 14) & 0x3FFu) + 1u;
            u32 slot = (st->loadTmem >= 256) ? ((st->loadTmem - 256) / 16u) : 0u;

            if (slot > 15u) {
                slot = 0;
            }
            if (st->tex.img && count <= 256u) {
                st->tluts[slot].img = st->tex.img;
                st->tluts[slot].count = count;
                st->tluts[slot].be = st->tex.imgBe;
                st->tlutLoads++;
                if (st->texWaitTlut && slot == (st->tex.palette & 15u) &&
                    st->blkImg) {
                    /* The tile G_SETTILESIZE described, completed: the image its
                     * own G_LOADBLOCK latched, plus the palette that was still
                     * missing when the tile was written. */
                    F3Tex pend = st->tex;

                    pend.img = st->blkImg;
                    pend.imgBe = st->blkImgBe;
                    pend.tlut = st->tex.img;
                    pend.tlutCount = count;
                    pend.tlutBe = st->tex.imgBe;
                    st->texWaitTlut = 0u;
                    if (st->texHook) {
                        st->texHook(st->texCtx, F3_TEX_EV_READY, &pend);
                    }
                }
            }
            st->rdpOps++;
            break;
        }

        case 0xFD: { /* G_SETTIMG */
            const u8 *src = resolveSeg(st, w1);

            if (!src) {
                /* Bind nothing rather than keep the previous image: the tile
                 * descriptors that follow would otherwise upload the old
                 * texture at the new extent, reading past its end. The
                 * triangles still go to the GE, untextured. */
                st->tex.img = NULL;
                st->tex.imgBe = 0u;
                st->tex.ready = false;
                st->skips++;
                break;
            }
            st->tex.img = src;
            st->tex.imgBe = segBe(st, w1) ? 1u : 0u;
            st->tex.imgFmt = (w0 >> 21) & 0x7u;
            st->tex.imgSiz = (w0 >> 19) & 0x3u;
            st->tex.imgWidth = (w0 & 0xFFFu) + 1u;
            st->tex.ready = false;
            st->imgChanges++;
            st->rdpOps++;
            break;
        }

        case 0xFC: /* G_SETCOMBINE */
            decodeCC(st, w0, w1);
            break;

        case F3_OP_MOVEMEM: {
            u32 idx = w0 & 0xFFu;
            u32 len = (((w0 >> 19) & 0x1Fu) + 1u) * 8u;

            /* In F3DEX_GBI_2 the viewport uses index 8; the payload is
             * Vp_t: scale[4] then trans[4], both s16 in 2.2 fixed. */
            if (idx == 8 && len >= sizeof(F3Vp)) {
                const u8 *src = resolveSeg(st, w1);

                if (!src) {
                    st->skips++;
                    break;
                }
                copyVp(&st->vp, src, segBe(st, w1));
                st->vpLoads++;
                if (st->vpHook) {
                    st->vpHook(st->vpCtx, &st->vp);
                }
            }
            break;
        }

        case F3_OP_MOVEWORD:
            break;

        case F3_OP_DL: {
            const u8 *sub = resolveSeg(st, w1);

            if (!sub) {
                st->skips++;
                break;
            }
            /* gsDma1p leaves G_DL's param in bits 16-23, and gbi.h calls it
             * with G_DL_PUSH (0) for gSPDisplayList and G_DL_NOPUSH (1) for
             * gSPBranchList: push returns to this list, nopush replaces it.
             * Reading the field as "one means call" survived the demo because
             * the demo was built with the same mistake; all eight hundred
             * seventy-seven G_DL commands in the one hundred thirty-four real
             * rooms carry zero, so the wrong reading turned every room into
             * its first sub-list and dropped the rest. */
            if ((w0 >> 16) & 0x01u) {
                return runList(st, sub, depth, segBe(st, w1));
            }
            if (!runList(st, sub, depth + 1u, segBe(st, w1))) {
                return false;
            }
            break;
        }

        case F3_OP_ENDDL:
            return true;

        default:
            if (isRdpOp(op)) {
                st->rdpOps++;
            } else {
                st->unsupported++;
                st->lastOpcode = op;
            }
            break;
        }

        dl += 8;
    }

    st->err = F3_ERR_DL_INF;
    return false;
}

void f3Init(F3State *st)
{
    memset(st, 0, sizeof(*st));
}

void f3Reset(F3State *st)
{
    const u8 *seg[16];
    u32 segBe;

    memcpy(seg, st->seg, sizeof(seg));
    segBe = st->segBe;
    memset(st, 0, sizeof(*st));
    memcpy(st->seg, seg, sizeof(seg));
    st->segBe = segBe;
}

void f3SetSegment(F3State *st, u32 seg, const void *base)
{
    if (seg < 16) {
        st->seg[seg] = (const u8 *)base;
    }
}

void f3SetSegmentBe(F3State *st, u32 seg, bool be)
{
    if (seg < 16) {
        st->segBe = be ? (st->segBe | (1u << seg)) : (st->segBe & ~(1u << seg));
    }
}

void f3SetTriHook(F3State *st, F3TriHook hook, void *ctx)
{
    st->triHook = hook;
    st->triCtx = ctx;
}

void f3SetMtxHook(F3State *st, F3MtxHook hook, void *ctx)
{
    st->mtxHook = hook;
    st->mtxCtx = ctx;
}

void f3SetTexHook(F3State *st, F3TexHook hook, void *ctx)
{
    st->texHook = hook;
    st->texCtx = ctx;
}

void f3SetCCHook(F3State *st, F3CCHook hook, void *ctx)
{
    st->ccHook = hook;
    st->ccCtx = ctx;
}

void f3SetVpHook(F3State *st, F3VpHook hook, void *ctx)
{
    st->vpHook = hook;
    st->vpCtx = ctx;
}

void f3SetOmHook(F3State *st, F3OmHook hook, void *ctx)
{
    st->omHook = hook;
    st->omCtx = ctx;
}

void f3SetGeomHook(F3State *st, F3GeomHook hook, void *ctx)
{
    st->geomHook = hook;
    st->geomCtx = ctx;
}

bool f3Run(F3State *st, const u8 *dl)
{
    if (!dl) {
        st->err = F3_ERR_ADDR;
        return false;
    }
    return runList(st, dl, 0, false);
}
