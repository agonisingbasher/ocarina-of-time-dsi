#include "dl_demo.h"

#include <stddef.h>

#include "f3dex2.h"

/* Byte offsets from the list base, which is also segment 6. The command array
 * is 298 words, so every region after the vertices sits eight bytes further
 * than it did before the drain run's G_DL was inserted. */
#define OFF_VTX     0x4A8
#define OFF_MV1     0x7A8
#define OFF_MV1F    0x7C8
#define OFF_MV2     0x7E8
#define OFF_MV2F    0x808
#define OFF_MV3     0x828
#define OFF_MV3F    0x848
#define OFF_TEX     0x868
#define OFF_PROJ    0x1068
#define OFF_PROJF   0x1088
#define OFF_ZOOM    0x10A8
#define OFF_ZOOMF   0x10C8
#define OFF_MV4     0x10E8
#define OFF_MV4F    0x1108
#define OFF_MV5     0x1128
#define OFF_MV5F    0x1148
#define OFF_MV6     0x1168
#define OFF_MV6F    0x1188
#define OFF_PAL4    0x11A8
#define OFF_CI4     0x11C8
#define OFF_PAL8    0x13C8
#define OFF_CI8     0x15C8
#define OFF_TINY    0x19C8
#define OFF_VP      0x1CC8
#define OFF_MV7     0x1CD8
#define OFF_MV7F    0x1CF8
#define OFF_CUT     0x1D18
#define OFF_XLU     0x2518
#define OFF_DRILL     0x25D8
#define OFF_DRILLV    0x2BE0
#define OFF_DRILLTEX  0x31E0

#define TEX_W       32
#define TEX_H       32
#define TINY_W      8
#define TINY_H      8
#define TINY_COUNT  6
#define XLU_QUAD    3

/* The drain run: twenty-four more 8x8 images, each on its own quad, in a
 * twelve-by-two grid inside the band the six flat-colour quads already own.
 * Twenty-eight distinct textures in one list against a sixteen-slot cache is
 * what forces the mid-frame wait; twelve of them cannot be uploaded at all
 * without it. Twelve columns of 28 world units fit the same -170..170 span. */
#define DRILL_COUNT    24
#define DRILL_COLS     12
#define DRILL_PITCH    28
#define DRILL_ROW      20
#define DRILL_QUAD_W   22
#define DRILL_QUAD_H   18
#define DRILL_X0       (-166)
#define DRILL_Y0       (-118)
#define DRILL_Z        (-250)

/* Texel and palette colours in the N64 A1R5G5B5 order the loader expects: bit
 * 15 is the one-bit alpha and it decides whether the pixel survives the GE's
 * alpha test at all, so the transparent probe is the only entry that clears it. */
#define C_RED       0xFC00u
#define C_GREEN     0x83E0u
#define C_BLUE      0x801Fu
#define C_YELLOW    0xFFE0u
#define C_CYAN      0x83FFu
#define C_MAGENTA   0xFC1Fu
#define C_WHITE     0xFFFFu
#define C_BLACK     0x8000u
#define C_CLEAR     0x0000u
#define C_BLUE_XRAY 0x001Fu /* blue with the alpha bit cleared */

/* Commands (F3DEX_GBI_2):
 *   G_MOVEMEM viewport: a 320x180 letterbox in N64 screen space.
 *   SETTIMG + SETTILE + LOADBLOCK + SETTILE + SETTILESIZE describe a 32x32
 *     RGBA16 texture.
 *   SETCOMBINE MODULATEIA before the first quad (texel * shade, the DS
 *     default), SHADE before the second (vertex colours despite G_TEXTURE
 *     being on) and DECALRGBA before the third (texel replaces the vertex
 *     colour). MODULATE is restored before the CI quads.
 *   G_MTX projection LOAD: a guPerspective-style matrix (60 degrees, near 200,
 *     far 2000) that replaces the ortho used by the grid.
 *   G_MTX modelview LOAD: first quad, textured, rotated 30 degrees about z
 *     and pushed back to z = -300.
 *   G_MTX push + LOAD: second quad, vertex colours only, upper left.
 *   G_MTX push + MUL (projection): zoom the frustum by 0.75 on x/y.
 *   G_MTX push + LOAD: third quad, lower right and nearer, under that zoom.
 *   Three G_POPMTX in a row: the top level is modelview, the next is
 *     projection and the last one modelview again, so this checks that the
 *     decoder's per-level type reaches the right GE stack.
 *   Two CI quads: CI4 through palette slot 5 and CI8 through slot 0 (256
 *     entries), each with its own SETTIMG/LOADTLUT pair. The CI4 has one
 *     sampled entry with alpha 0 past index 0, so it is baked to direct colour
 *     and cuts itself; the CI8 asks for alpha only at index 0, so it keeps the
 *     palette and cuts through the GE's colour-0-transparent flag.
 *   Six 8x8 quads with six different images along the bottom: ten distinct
 *     textures in one list, two more than the cache holds, so the last uploads
 *     have to recycle slots.
 *   G_DL to a built-at-init run of twenty-four more 8x8 images in the same
 *     band: twenty-eight distinct textures against sixteen slots, which is what
 *     makes the draw wait for the engine to go idle before it refills one.
 *   One more G_MTX push + LOAD and a 32x32 RGBA image whose slits carry
 *     alpha=0, drawn over the big quad with DECALRGBA: the texel alpha reaches
 *     the DS alpha test, so the material cuts itself out instead of painting a
 *     yellow block.
 *   G_SETOTHERMODE_H with G_TT_RGBA16 (a CI fetch may return an alpha), then an
 *     opaque SHADE plate across the panel behind the CI quads, G_SETOTHERMODE_L
 *     with the translucent render mode and two SHADE quads over that plate with
 *     vertex alphas 128 and 64: the DS has no vertex alpha, so each triangle
 *     becomes one 5-bit polygon alpha.
 * The push bit is inverted in the stream, as gSPMatrix emits
 * ((p) ^ G_MTX_PUSH), and G_POPMTX carries bytes (64 per level) in w1. */
typedef struct {
    u32 cmds[298];
    F3Vtx vtx[48];
    u16 mtxInt[16];
    u16 mtxFrac[16];
    u16 mtx2Int[16];
    u16 mtx2Frac[16];
    u16 mtx3Int[16];
    u16 mtx3Frac[16];
    u16 tex[TEX_W * TEX_H];
    u16 projInt[16];
    u16 projFrac[16];
    u16 zoomInt[16];
    u16 zoomFrac[16];
    u16 mtx4Int[16];
    u16 mtx4Frac[16];
    u16 mtx5Int[16];
    u16 mtx5Frac[16];
    u16 mtx6Int[16];
    u16 mtx6Frac[16];
    u16 pal4[16];
    u8 ci4[TEX_W * TEX_H / 2];
    u16 pal8[256];
    u8 ci8[TEX_W * TEX_H];
    u16 tiny[TINY_COUNT][TINY_W * TINY_H];
    s16 vp[8];
    u16 mtx7Int[16];
    u16 mtx7Frac[16];
    u16 cut[TEX_W * TEX_H];
    F3Vtx xlu[XLU_QUAD * 4];
    /* Filled by initDrill(): the nested list, one quad of its own per image. */
    u32 drill[DRILL_COUNT * 16 + 2];
    F3Vtx drillV[DRILL_COUNT][4];
    u16 drillTex[DRILL_COUNT][TINY_W * TINY_H];
} DemoList;

_Static_assert(offsetof(DemoList, vtx) == OFF_VTX, "vtx offset");
_Static_assert(offsetof(DemoList, mtxInt) == OFF_MV1, "matrix offset");
_Static_assert(offsetof(DemoList, mtxFrac) == OFF_MV1F, "matrix frac offset");
_Static_assert(offsetof(DemoList, mtx2Int) == OFF_MV2, "second matrix offset");
_Static_assert(offsetof(DemoList, mtx2Frac) == OFF_MV2F, "second matrix frac");
_Static_assert(offsetof(DemoList, mtx3Int) == OFF_MV3, "third matrix offset");
_Static_assert(offsetof(DemoList, mtx3Frac) == OFF_MV3F, "third matrix frac");
_Static_assert(offsetof(DemoList, tex) == OFF_TEX, "texture offset");
_Static_assert(offsetof(DemoList, projInt) == OFF_PROJ, "projection offset");
_Static_assert(offsetof(DemoList, projFrac) == OFF_PROJF, "projection frac");
_Static_assert(offsetof(DemoList, zoomInt) == OFF_ZOOM, "zoom matrix offset");
_Static_assert(offsetof(DemoList, zoomFrac) == OFF_ZOOMF, "zoom matrix frac");
_Static_assert(offsetof(DemoList, mtx4Int) == OFF_MV4, "fourth matrix offset");
_Static_assert(offsetof(DemoList, mtx4Frac) == OFF_MV4F, "fourth matrix frac");
_Static_assert(offsetof(DemoList, mtx5Int) == OFF_MV5, "fifth matrix offset");
_Static_assert(offsetof(DemoList, mtx5Frac) == OFF_MV5F, "fifth matrix frac");
_Static_assert(offsetof(DemoList, mtx6Int) == OFF_MV6, "sixth matrix offset");
_Static_assert(offsetof(DemoList, mtx6Frac) == OFF_MV6F, "sixth matrix frac");
_Static_assert(offsetof(DemoList, pal4) == OFF_PAL4, "CI4 palette offset");
_Static_assert(offsetof(DemoList, ci4) == OFF_CI4, "CI4 image offset");
_Static_assert(offsetof(DemoList, pal8) == OFF_PAL8, "CI8 palette offset");
_Static_assert(offsetof(DemoList, ci8) == OFF_CI8, "CI8 image offset");
_Static_assert(offsetof(DemoList, tiny) == OFF_TINY, "tiny textures offset");
_Static_assert(offsetof(DemoList, vp) == OFF_VP, "viewport offset");
_Static_assert(offsetof(DemoList, mtx7Int) == OFF_MV7, "seventh matrix offset");
_Static_assert(offsetof(DemoList, mtx7Frac) == OFF_MV7F, "seventh matrix frac");
_Static_assert(offsetof(DemoList, cut) == OFF_CUT, "cutout texture offset");
_Static_assert(offsetof(DemoList, xlu) == OFF_XLU, "translucent vertex offset");
_Static_assert(offsetof(DemoList, drill) == OFF_DRILL, "drain list offset");
_Static_assert(offsetof(DemoList, drillV) == OFF_DRILLV, "drain vertex offset");
_Static_assert(offsetof(DemoList, drillTex) == OFF_DRILLTEX, "drain texture offset");

static DemoList sDemo __attribute__((aligned(8))) = {
    .cmds = {
        /* G_SETOTHERMODE_H: gDPSetTextureLUT(G_TT_RGBA16), which is what
         * gDPLoadTextureBlock asks for so a CI fetch can return an alpha. */
        0xE3001001, 0x00008000,
        /* G_MOVEMEM VIEWPORT: scale (640,360), trans (640,480) in 2.2, i.e.
         * N64 pixels 0..320 x 30..210, so the 4/5 mapping onto the DS is a
         * 0..255 x 24..168 letterbox. */
        0xDC080008, 0x06000000 | OFF_VP,
        /* G_SETTIMG: RGBA, 16-bit block load, width field 1 as emitted by
         * gDPLoadTextureBlock (the real extent comes from SETTILESIZE). */
        0xFD100000, 0x06000000 | OFF_TEX,
        /* G_SETTILE load tile 7: fmt RGBA, siz 16b, line 0, tmem 0. */
        0xF5100000, 0x07000000,
        /* G_LOADBLOCK tile 7: 1024 texels (0x3FF), dxt 0. */
        0xF3000000, 0x073FF000,
        /* G_SETTILE render tile 0: line 8 words (32 texels/row), wrap/mask 5. */
        0xF5101000, 0x00014050,
        /* G_SETTILESIZE render tile 0: uls/ult 0, lrs/lrt (31 << 2). */
        0xF2000000, 0x0007C07C,
        /* G_MTX 64 B, LOAD | PROJECTION (stream params 0x07). */
        0xDA380007, 0x06000000 | OFF_PROJ,
        /* G_MTX 64 B, LOAD modelview. */
        0xDA380003, 0x06000000 | OFF_MV1,
        /* G_TEXTURE on, render tile 0. */
        0xD7000002, 0,
        /* G_SETCOMBINE MODULATEIA: TEXEL0 * SHADE, the native GE stage. */
        0xFC121824, 0xFF33FFFF,
        0x01004008, 0x06000000 | OFF_VTX,  /* VTX  n=4, v0=0 */
        0x05000204, 0,                     /* TRI1 (0,1,2) */
        0x05000406, 0,                     /* TRI1 (0,2,3) */
        /* G_SETCOMBINE SHADE: the vertex colours must win even though the
         * texture stays enabled. */
        0xFCFFFFFF, 0xFFFE793C,
        0xDA380002, 0x06000000 | OFF_MV2,  /* MTX  64 B, PUSH + LOAD */
        0x01004010, 0x06000000 | (OFF_VTX + 4 * 16), /* VTX n=4, v0=4 */
        0x05080A0C, 0,                     /* TRI1 (4,5,6) */
        0x05080C0E, 0,                     /* TRI1 (4,6,7) */
        /* G_MTX 64 B, PUSH | MUL | PROJECTION (stream params 0x04, since the
         * push bit is inverted: 0x04 ^ 0x01 = 0x05). */
        0xDA380004, 0x06000000 | OFF_ZOOM,
        0xDA380002, 0x06000000 | OFF_MV3,  /* MTX  64 B, PUSH + LOAD */
        /* G_SETCOMBINE DECALRGBA: the texel replaces the vertex colour. */
        0xFCFFFFFF, 0xFFFCF279,
        0x01004018, 0x06000000 | (OFF_VTX + 8 * 16), /* VTX n=4, v0=8 */
        0x05101214, 0,                     /* TRI1 (8,9,10) */
        0x05101416, 0,                     /* TRI1 (8,10,11) */
        0xD8000000, 64,                    /* POPMTX modelview */
        0xD8000000, 64,                    /* POPMTX projection */
        0xD8000000, 64,                    /* POPMTX modelview */
        /* Back to MODULATE for the CI quads, whose vertex colours are white. */
        0xFC121824, 0xFF33FFFF,
        /* CI4: 16-entry palette into slot 5 (TMEM 256 + 5 * 16 = 336). */
        0xFD100000, 0x06000000 | OFF_PAL4, /* SETTIMG RGBA16 palette */
        0xF5100150, 0x07000000,            /* SETTILE load, tmem 336 */
        0xF0000000, 0x0703C000,            /* LOADTLUT tile 7, 16 entries */
        0xFD400000, 0x06000000 | OFF_CI4,  /* SETTIMG CI, 4bpp */
        0xF5400000, 0x07000000,            /* SETTILE load, tmem 0 */
        0xF3000000, 0x070FF000,            /* LOADBLOCK 256 words */
        0xF5400400, 0x00514050,            /* SETTILE render: CI4, line 2,
                                            * palette 5, mask 5 */
        0xF2000000, 0x0007C07C,            /* SETTILESIZE 32x32 */
        0xDA380002, 0x06000000 | OFF_MV4,  /* MTX push + LOAD */
        0xD7000002, 0,                     /* G_TEXTURE on */
        0x01004020, 0x06000000 | (OFF_VTX + 12 * 16), /* VTX n=4, v0=12 */
        0x05181A1C, 0,                     /* TRI1 (12,13,14) */
        0x05181C1E, 0,                     /* TRI1 (12,14,15) */
        0xD7000000, 0,                     /* G_TEXTURE off */
        /* CI8: 256-entry palette into slot 0 (TMEM 256). */
        0xFD100000, 0x06000000 | OFF_PAL8,
        0xF5100100, 0x07000000,            /* SETTILE load, tmem 256 */
        0xF0000000, 0x073FC000,            /* LOADTLUT, 256 entries */
        0xFD480000, 0x06000000 | OFF_CI8,  /* SETTIMG CI, 8bpp */
        0xF5480000, 0x07000000,            /* SETTILE load, tmem 0 */
        0xF3000000, 0x071FF000,            /* LOADBLOCK 512 words */
        0xF5480800, 0x00014050,            /* SETTILE render: CI8, line 4 */
        0xF2000000, 0x0007C07C,
        0xDA380002, 0x06000000 | OFF_MV5,  /* MTX push + LOAD */
        0xD7000002, 0,
        0x01004028, 0x06000000 | (OFF_VTX + 16 * 16), /* VTX n=4, v0=16 */
        0x05202224, 0,                     /* TRI1 (16,17,18) */
        0x05202426, 0,                     /* TRI1 (16,18,19) */
        0xD7000000, 0,
        0xD8000000, 64,                    /* POPMTX modelview (fifth) */
        0xD8000000, 64,                    /* POPMTX modelview (fourth) */
        /* Six 8x8 quads, one texture each, in a row along the bottom. Nine
         * distinct textures total, so the ninth upload recycles a slot. */
        0xDA380002, 0x06000000 | OFF_MV6,  /* MTX push + LOAD, identity */
        0xD7000002, 0,                     /* G_TEXTURE on for the run */
        0xFD100000, 0x06000000 | (OFF_TINY + 0 * 128),
        0xF5100000, 0x07000000,            /* SETTILE load tile 7 */
        0xF3000000, 0x0703F000,            /* LOADBLOCK 64 texels */
        0xF5100400, 0x0000C030,            /* SETTILE render: 8x8, line 2 */
        0xF2000000, 0x0001C01C,            /* SETTILESIZE 8x8 */
        0x01004009, 0x06000000 | (OFF_VTX + 20 * 16), /* VTX n=4, v0=0 */
        0x05000204, 0,                     /* TRI1 (0,1,2) */
        0x05000406, 0,                     /* TRI1 (0,2,3) */
        0xFD100000, 0x06000000 | (OFF_TINY + 1 * 128),
        0xF5100000, 0x07000000,
        0xF3000000, 0x0703F000,
        0xF5100400, 0x0000C030,
        0xF2000000, 0x0001C01C,
        0x01004009, 0x06000000 | (OFF_VTX + 24 * 16), /* v0=0, source quad 2 */
        0x05000204, 0,
        0x05000406, 0,
        0xFD100000, 0x06000000 | (OFF_TINY + 2 * 128),
        0xF5100000, 0x07000000,
        0xF3000000, 0x0703F000,
        0xF5100400, 0x0000C030,
        0xF2000000, 0x0001C01C,
        0x01004009, 0x06000000 | (OFF_VTX + 28 * 16), /* v0=0, source quad 3 */
        0x05000204, 0,
        0x05000406, 0,
        0xFD100000, 0x06000000 | (OFF_TINY + 3 * 128),
        0xF5100000, 0x07000000,
        0xF3000000, 0x0703F000,
        0xF5100400, 0x0000C030,
        0xF2000000, 0x0001C01C,
        0x01004009, 0x06000000 | (OFF_VTX + 32 * 16), /* v0=0, source quad 4 */
        0x05000204, 0,
        0x05000406, 0,
        0xFD100000, 0x06000000 | (OFF_TINY + 4 * 128),
        0xF5100000, 0x07000000,
        0xF3000000, 0x0703F000,
        0xF5100400, 0x0000C030,
        0xF2000000, 0x0001C01C,
        0x01004009, 0x06000000 | (OFF_VTX + 36 * 16), /* v0=0, source quad 5 */
        0x05000204, 0,
        0x05000406, 0,
        0xFD100000, 0x06000000 | (OFF_TINY + 5 * 128),
        0xF5100000, 0x07000000,
        0xF3000000, 0x0703F000,
        0xF5100400, 0x0000C030,
        0xF2000000, 0x0001C01C,
        0x01004009, 0x06000000 | (OFF_VTX + 40 * 16), /* v0=0, source quad 6 */
        0x05000204, 0,
        0x05000406, 0,
        /* G_DL call: the twenty-four-image drain run, still under the same
         * identity modelview and the same textured MODULATE. Built at init by
         * initDrill() because writing it as hex would be two hundred lines.
         * Bits 16-23 carry gbi.h's param: G_DL_PUSH is zero. */
        0xDE000000, 0x06000000 | OFF_DRILL,
        0xD7000000, 0,                     /* G_TEXTURE off */
        0xD8000000, 64,                    /* POPMTX modelview (sixth) */
        /* Cutout: DECALRGBA hands the texel's own alpha to the DS alpha test,
         * so the alpha=0 slits drop and the big quad shows through. Drawn at
         * z = -240, nearer than every corner quad but the bottom row, which it
         * deliberately stays clear of. */
        0xDA380002, 0x06000000 | OFF_MV7,  /* MTX push + LOAD: identity, z=-240 */
        0xD7000002, 0,                     /* G_TEXTURE on */
        0xFD100000, 0x06000000 | OFF_CUT,  /* SETTIMG RGBA16 */
        0xF5100000, 0x07000000,            /* SETTILE load tile 7, tmem 0 */
        0xF3000000, 0x073FF000,            /* LOADBLOCK 1024 texels */
        0xF5101000, 0x00014050,            /* SETTILE render: line 8 words,
                                            * mask 5 */
        0xF2000000, 0x0007C07C,            /* SETTILESIZE 32x32 */
        0xFCFFFFFF, 0xFFFCF279,            /* SETCOMBINE DECALRGBA */
        0x01004030, 0x06000000 | (OFF_VTX + 44 * 16), /* VTX n=4, v0=20 */
        0x05282A2C, 0,                     /* TRI1 (20,21,22) */
        0x05282C2E, 0,                     /* TRI1 (20,22,23) */
        /* Translucent pass, under the same modelview. First an opaque cyan
         * plate at eye -295 - behind the CI quads, in front of the big one - so
         * the two quads after it blend into a destination this test already
         * knows, and so do the texels the CI palettes cut out.
         * G_RM_AA_ZB_XLU_SURF for both cycles says "blend", the SHADE
         * combiner says "the factor is the vertex alpha", and the DS takes one
         * 5-bit polygon alpha per triangle instead of one per vertex. */
        0xFCFFFFFF, 0xFFFE793C,            /* SETCOMBINE SHADE */
        0x01004038, 0x06000000 | OFF_XLU,  /* VTX n=4, GE slots 24..27: plate */
        0x05303234, 0,                     /* TRI1 (24,25,26) */
        0x05303436, 0,                     /* TRI1 (24,26,27) */
        0xE200001C, 0x005049D8,            /* SETOTHERMODE_L: XLU render mode */
        0x01004040, 0x06000000 | (OFF_XLU + 64), /* VTX n=4, slots 28..31: red */
        0x05383A3C, 0,                     /* TRI1 (28,29,30) */
        0x05383C3E, 0,                     /* TRI1 (28,30,31) */
        0x01004038, 0x06000000 | (OFF_XLU + 128), /* VTX n=4, slots 24..27: blue */
        0x05303234, 0,
        0x05303436, 0,
        0xD7000000, 0,                     /* G_TEXTURE off */
        0xD8000000, 64,                    /* POPMTX modelview (seventh) */
        0xDF000000, 0,                     /* ENDDL */
    },
    /* Big quad covering the whole texture (s/t are 10.5, so 32 texels =
     * 1024). White vertex colours keep the texels unmodulated. */
    .vtx = {
        { -200, -140, 0, 0,    0, 1024, 255, 255, 255, 255 },
        {  200, -140, 0, 0, 1024, 1024, 255, 255, 255, 255 },
        {  200,  140, 0, 0, 1024,    0, 255, 255, 255, 255 },
        { -200,  140, 0, 0,    0,    0, 255, 255, 255, 255 },
        /* Small quad drawn with the second modelview, upper left. */
        { -60, -60, 0, 0, 0, 0, 255,   0,   0, 255 },
        {  60, -60, 0, 0, 0, 0,   0, 255,   0, 255 },
        {  60,  60, 0, 0, 0, 0, 255, 255,   0, 255 },
        { -60,  60, 0, 0, 0, 0,   0, 255, 255, 255 },
        /* Same shape again, lower right, under the zoomed projection. Drawn
         * with DECAL, so the texture shows and these colours do not. */
        { -60, -60, 0, 0,    0, 1024,   0,   0, 255, 255 },
        {  60, -60, 0, 0, 1024, 1024, 255,   0, 255, 255 },
        {  60,  60, 0, 0, 1024,    0, 255, 255, 255, 255 },
        { -60,  60, 0, 0,    0,    0, 128, 128, 128, 255 },
        /* CI4 quad, centred and in front of the other textured quads. */
        { -60, -60, 0, 0,    0, 1024, 255, 255, 255, 255 },
        {  60, -60, 0, 0, 1024, 1024, 255, 255, 255, 255 },
        {  60,  60, 0, 0, 1024,    0, 255, 255, 255, 255 },
        { -60,  60, 0, 0,    0,    0, 255, 255, 255, 255 },
        /* CI8 quad, upper right. */
        { -60, -60, 0, 0,    0, 1024, 255, 255, 255, 255 },
        {  60, -60, 0, 0, 1024, 1024, 255, 255, 255, 255 },
        {  60,  60, 0, 0, 1024,    0, 255, 255, 255, 255 },
        { -60,  60, 0, 0,    0,    0, 255, 255, 255, 255 },
        /* Six 8x8 quads in a row, 40 units wide and 60 apart, at z = -250.
         * The identity modelview leaves these coordinates as world space. */
        { -170, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        { -130, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        { -130, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        { -170, -80, -250, 0,   0,   0, 255, 255, 255, 255 },

        { -110, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        {  -70, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        {  -70, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        { -110, -80, -250, 0,   0,   0, 255, 255, 255, 255 },

        {  -50, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        {  -10, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        {  -10, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        {  -50, -80, -250, 0,   0,   0, 255, 255, 255, 255 },

        {   10, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        {   50, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        {   50, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        {   10, -80, -250, 0,   0,   0, 255, 255, 255, 255 },

        {   70, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        {  110, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        {  110, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        {   70, -80, -250, 0,   0,   0, 255, 255, 255, 255 },

        {  130, -120, -250, 0,   0, 256, 255, 255, 255, 255 },
        {  170, -120, -250, 0, 256, 256, 255, 255, 255, 255 },
        {  170, -80, -250, 0, 256,   0, 255, 255, 255, 255 },
        {  130, -80, -250, 0,   0,   0, 255, 255, 255, 255 },
        /* Cutout quad, centre-high over the big quad and covering the whole
         * 32x32 image, so every slit has to come out as a hole. */
        {  -95,    5, 0, 0,    0, 1024, 255, 255, 255, 255 },
        {   95,    5, 0, 0, 1024, 1024, 255, 255, 255, 255 },
        {   95,  120, 0, 0, 1024,    0, 255, 255, 255, 255 },
        {  -95,  120, 0, 0,    0,    0, 255, 255, 255, 255 },
    },
    /* Flat 16.16 as libultra builds it: rotation in rows 0-2, translation in
     * mf[3][0..2] (flat 12..14). Negatives split as int = floor plus
     * fraction: -0.5 is int 0xFFFF, frac 0x8000. */
    .mtxInt = {
        0, 0xFFFF, 0, 0,
        0, 0, 0, 0,
        0, 0, 1, 0,
        0, 0, 0xFED4, 1,
    },
    .mtxFrac = {
        56755, 0x8000, 0, 0,
        32768, 56755, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity plus (-130, 70, -290): upper left, nearer than the big quad. */
    .mtx2Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0xFF7E, 0x0046, 0xFEDE, 1,
    },
    .mtx2Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity plus (130, -70, -250): lower right and closest of all. */
    .mtx3Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        130, 0xFFBA, 0xFF06, 1,
    },
    .mtx3Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* guPerspective: fovy 60, aspect 4:3, near 200, far 2000.
     * x scale 1.299, y scale 1.732, z scale -1.222 at flat 10, the w row at
     * flat 11 (-1) and the w term -444.444 at flat 14, exactly where libnds's
     * glFrustumf32 puts them. */
    .projInt = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0xFFFE, 0xFFFF,
        0, 0, 0xFE43, 0,
    },
    .projFrac = {
        19597, 0, 0, 0,
        0, 47975, 0, 0,
        0, 0, 50972, 0,
        0, 0, 36409, 0,
    },
    /* Uniform 0.75 on x and y: a projection MUL that only zooms, so the
     * result does not depend on the RSP's multiply order. */
    .zoomInt = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    },
    .zoomFrac = {
        49152, 0, 0, 0,
        0, 49152, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity plus (0, 0, -215): dead centre and nearer than everything else
     * textured, so no other quad can hide the palette stripe. The cutout quad
     * it used to clear sits at -240 and the plate at -295, both behind it. */
    .mtx4Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0xFF29, 1,
    },
    .mtx4Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity plus (130, 70, -280): upper right. */
    .mtx5Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0x0082, 0x0046, 0xFEE8, 1,
    },
    .mtx5Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity: the six bottom quads carry their own world position. */
    .mtx6Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    },
    .mtx6Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Identity plus (0, 0, -240): the cutout quad floats in front of the big
     * quad at z = -300; the bottom row is kept clear of its screen area by
     * placing the quad high instead of centred. */
    .mtx7Int = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0xFF10, 1,
    },
    .mtx7Frac = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Vp_t: scale then trans, both s16 in 2.2 over the 320x240 N64 screen.
     * 320x180 centred vertically: x = 160 +- 160, y = 120 +- 90. */
    .vp = {
        640, 360, 1023, 0,
        640, 480, 0, 0,
    },
    /* An opaque cyan plate across the panel at eye depth -295, then two
     * translucent halves over it at -292, one at alpha 16/31 and one at 8/31.
     * The plate makes the destination of every blended pixel known, so each
     * half is a direct read of the polygon alpha the GE received: pure red or
     * blue means nothing composited, a pale band means the factor landed.
     *
     * Both sit behind the CI quads (-280) and the vertex-colour quad (-290) and
     * in front of the big one (-300), because a plate nearer than the CI quads
     * buries their palette probes - and their holes land on this plate, so the
     * cut-out texels read the same known cyan as the blends do.
     * The plate is under the same modelview as the cutout quad: identity plus
     * z = -240, so its own z is an offset from that. -55 puts the plate at eye
     * -295, behind the CI quads (-280) and the vertex-colour quad (-290) and in
     * front of the big one (-300); a plate nearer than the CI quads buries
     * their palette probes. The halves are at -292, 2 % inside the plate so
     * their edges never reach past it. */
    .xlu = {
        { -200, -140, -55, 0, 0, 0,   0, 255, 255, 255 },   /* plate */
        {  200, -140, -55, 0, 0, 0,   0, 255, 255, 255 },
        {  200,  140, -55, 0, 0, 0,   0, 255, 255, 255 },
        { -200,  140, -55, 0, 0, 0,   0, 255, 255, 255 },

        { -196, -137, -52, 0, 0, 0, 255,   0,   0, 128 },   /* red a16/31 */
        {    0, -137, -52, 0, 0, 0, 255,   0,   0, 128 },
        {    0,  137, -52, 0, 0, 0, 255,   0,   0, 128 },
        { -196,  137, -52, 0, 0, 0, 255,   0,   0, 128 },

        {    0, -137, -52, 0, 0, 0,   0,   0, 255,  64 },   /* blue a8/31 */
        {  196, -137, -52, 0, 0, 0,   0,   0, 255,  64 },
        {  196,  137, -52, 0, 0, 0,   0,   0, 255,  64 },
        {    0,  137, -52, 0, 0, 0,   0,   0, 255,  64 },
    },
};

const u8 *dlDemoList(void)
{
    return (const u8 *)&sDemo;
}

/* RGBA quadrants make flips obvious: red/green on top, blue/yellow below, a
 * dark diagonal from corner to corner and a white patch near the top-left. */
static void initRgba(void)
{
    u16 *tex = sDemo.tex;
    u32 x, y;

    for (y = 0; y < TEX_H; y++) {
        for (x = 0; x < TEX_W; x++) {
            u16 c;

            if (x < 16 && y < 16) {
                c = C_RED;
            } else if (y < 16) {
                c = C_GREEN;
            } else if (x < 16) {
                c = C_BLUE;
            } else {
                c = C_YELLOW;
            }

            if (x == y || x + 1 == y) {
                c = C_BLACK; /* opaque black diagonal */
            }
            if (x >= 2 && x < 6 && y >= 2 && y < 6) {
                c = C_WHITE; /* white marker */
            }
            tex[y * TEX_W + x] = c;
        }
    }
}

/* A 5-texel magenta frame around a red field, plus an 8x8 white patch on an
 * odd texel. Both widths are deliberately odd: if the DS read the nibble pair
 * in the wrong order the inner edges would come out ragged with a one-texel
 * alternation instead of straight lines.
 *
 * A four-texel stripe of index 3 is the palette-bake probe: that entry is blue
 * with the N64 alpha bit cleared. The DS 3D palette cannot carry it, so this
 * image has to be expanded to direct colour at upload; the stripe coming out
 * blue means the bake did not happen or the alpha was lost in it. Measured with
 * the CI4's own colours for the rest: magenta, red and white must stay exactly
 * where the cheap palette path had them. */
static void initCi4(void)
{
    u32 x, y;

    for (y = 0; y < TEX_H; y++) {
        for (x = 0; x < TEX_W; x++) {
            u8 idx = 2;

            if (x < 5 || x > 26 || y < 5 || y > 26) {
                idx = 1;
            }
            if (x >= 10 && x < 14) {
                idx = 3;
            }
            if (x >= 17 && x < 25 && y >= 17 && y < 25) {
                idx = 15;
            }
            if (!(x & 1)) {
                sDemo.ci4[(y * TEX_W + x) >> 1] = (u8)(idx << 4);
            } else {
                sDemo.ci4[(y * TEX_W + x) >> 1] |= idx;
            }
        }
    }

    sDemo.pal4[1] = C_MAGENTA;
    sDemo.pal4[2] = C_RED;      /* alpha 1 */
    sDemo.pal4[3] = C_BLUE_XRAY; /* blue with alpha 0: if the stripe shows, the
                                  * palette's alpha bit never reached the GE */
    sDemo.pal4[15] = C_WHITE;
}

/* Index = 8 * column + row band, so the image shows 32 grey steps across and
 * the top half is tinted blue: a monotonic ramp means both the index order
 * and the 256-entry palette came through right.
 *
 * The 4-texel frame is index 0, and index 0 alone is blue with the N64 alpha
 * bit cleared. That is the second CI path: the GE has a colour-0-transparent
 * flag that costs no VRAM, so an asset that hides only behind index 0 must not
 * be baked to direct colour. Frame gone = flag reached the texture param;
 * frame solid blue = it did not. */
static void initCi8(void)
{
    u32 x, y, i;

    for (y = 0; y < TEX_H; y++) {
        for (x = 0; x < TEX_W; x++) {
            u8 idx = (u8)(x * 8u + (y & 7u));

            if (x < 4 || x > 27 || y < 4 || y > 27) {
                idx = 0;
            }
            sDemo.ci8[y * TEX_W + x] = idx;
        }
    }

    for (i = 0; i < 256; i++) {
        u32 v = i >> 3;
        u32 b = (i < 128) ? 31u : 0u;

        sDemo.pal8[i] = (u16)(0x8000u | (v << 10) | (v << 5) | b);
    }
    sDemo.pal8[0] = C_BLUE_XRAY;
}

/* Six flat colours. Each quad must show its own: if the cache recycled a slot
 * while the list still used it, the quad would come out with the previous
 * image instead. */
static void initTiny(void)
{
    static const u16 colors[TINY_COUNT] = {
        C_RED,
        C_GREEN,
        C_BLUE,
        C_YELLOW,
        C_CYAN,
        C_MAGENTA,
    };
    u32 i, j;

    for (i = 0; i < TINY_COUNT; i++) {
        for (j = 0; j < TINY_W * TINY_H; j++) {
            sDemo.tiny[i][j] = colors[i];
        }
    }
}

/* The drain run, built here instead of written as hex: twenty-four images and
 * twenty-four quads is three hundred and eighty-six words of commands. The
 * colours are a permutation of a twelve-hue wheel at two brightnesses with a
 * step of seven, so no two neighbours in the grid look alike. A slot recycled
 * while the list still used it therefore shows up as a square wearing someone
 * else's colour, which is what the screenshot has to rule out. */
static void initDrill(void)
{
    static const u16 colors[DRILL_COUNT] = {
        0xFC00, 0x821F, 0xC620, 0xA411, 0x83E0, 0xFC10,
        0x8231, 0xFE00, 0x801F, 0xA620, 0xC411, 0x83F0,
        0xC400, 0x8131, 0xFFE0, 0xC01F, 0x8220, 0xC409,
        0x83FF, 0xC520, 0x8011, 0xC3E0, 0xFC1F, 0x8229,
    };
    static const s16 dx[4] = { 0, DRILL_QUAD_W, DRILL_QUAD_W, 0 };
    static const s16 dy[4] = { 0, 0, DRILL_QUAD_H, DRILL_QUAD_H };
    static const s16 dds[4] = { 0, 256, 256, 0 };
    static const s16 ddt[4] = { 256, 256, 0, 0 };
    u32 *p = sDemo.drill;
    u32 i;

    for (i = 0; i < DRILL_COUNT; i++) {
        s32 x = DRILL_X0 + (s32)(i % DRILL_COLS) * DRILL_PITCH;
        s32 y = DRILL_Y0 + (s32)(i / DRILL_COLS) * DRILL_ROW;
        u32 j;

        /* The same five RDP commands the flat-colour run uses, then four
         * corners and the two triangles they make. */
        *p++ = 0xFD100000; *p++ = 0x06000000 | (OFF_DRILLTEX + i * 128);
        *p++ = 0xF5100000; *p++ = 0x07000000;
        *p++ = 0xF3000000; *p++ = 0x0703F000;
        *p++ = 0xF5100400; *p++ = 0x0000C030;
        *p++ = 0xF2000000; *p++ = 0x0001C01C;
        *p++ = 0x01004009; *p++ = 0x06000000 | (OFF_DRILLV + i * 64);
        *p++ = 0x05000204; *p++ = 0;
        *p++ = 0x05000406; *p++ = 0;

        for (j = 0; j < 4; j++) {
            F3Vtx *v = &sDemo.drillV[i][j];

            v->x = (s16)(x + dx[j]);
            v->y = (s16)(y + dy[j]);
            v->z = DRILL_Z;
            v->flag = 0;
            v->s = dds[j];
            v->t = ddt[j];
            v->r = 255;
            v->g = 255;
            v->b = 255;
            v->a = 255;
        }
        for (j = 0; j < TINY_W * TINY_H; j++) {
            sDemo.drillTex[i][j] = colors[i];
        }
    }
    *p++ = (u32)F3_OP_ENDDL << 24;
    *p = 0;
}

/* Yellow/green checker in 4x4 blocks, an opaque red frame and a solid blue
 * core, crossed by a lattice of alpha=0 texels: those are the ones the DS has
 * to drop, so the big quad behind shows through as straight 1-texel slits. The
 * frame is checked last so it stays opaque even where a slit would cut it. */
static void initCut(void)
{
    u32 x, y;

    for (y = 0; y < TEX_H; y++) {
        for (x = 0; x < TEX_W; x++) {
            u16 c = (((x >> 2) + (y >> 2)) & 1u) ? C_GREEN : C_YELLOW;

            if (x % 8u == 6u || y % 8u == 3u) {
                c = C_CLEAR;
            }
            if (x >= 14 && x < 18 && y >= 14 && y < 18) {
                c = C_BLUE;
            }
            if (x < 4 || x > 27 || y < 4 || y > 27) {
                c = C_RED;
            }
            sDemo.cut[y * TEX_W + x] = c;
        }
    }
}

void dlDemoInit(void)
{
    initRgba();
    initCi4();
    initCi8();
    initTiny();
    initDrill();
    initCut();
}
