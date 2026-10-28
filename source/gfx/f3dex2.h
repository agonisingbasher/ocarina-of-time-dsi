#ifndef OCARINA_GFX_F3DEX2_H
#define OCARINA_GFX_F3DEX2_H

#include <nds.h>
#include <stdbool.h>

/* Opcodes as emitted by zeldaret/oot with F3DEX_GBI_2
 * (include/ultra64/gbi.h). Do not confuse with plain F3DEX values. */
enum {
    F3_OP_VTX          = 0x01,
    F3_OP_MODIFYVTX    = 0x02,
    F3_OP_CULLDL       = 0x03,
    F3_OP_BRANCH_Z     = 0x04,
    F3_OP_TRI1         = 0x05,
    F3_OP_TRI2         = 0x06,
    F3_OP_QUAD         = 0x07,
    F3_OP_LINE3D       = 0x08,
    F3_OP_TEXTURE      = 0xD7,
    F3_OP_POPMTX       = 0xD8,
    F3_OP_GEOMETRYMODE = 0xD9,
    F3_OP_MTX          = 0xDA,
    F3_OP_MOVEWORD     = 0xDB,
    F3_OP_MOVEMEM      = 0xDC,
    F3_OP_DL           = 0xDE,
    F3_OP_ENDDL        = 0xDF,
    F3_OP_SPNOOP       = 0xE0,
    F3_OP_RDPHALF_1    = 0xE1,
    F3_OP_SETOTHERMODE_L = 0xE2,
    F3_OP_SETOTHERMODE_H = 0xE3,
    F3_OP_RDPHALF_2    = 0xF1,
};

enum {
    F3_ERR_NONE = 0,
    F3_ERR_ADDR,
    F3_ERR_TRI_INDEX,
    F3_ERR_DL_DEPTH,
    F3_ERR_DL_INF,
    F3_ERR_MTX_SIZE,
    F3_ERR_MTX_STACK,
};

/* F3DEX2's vertex cache holds eighty vertices. The one hundred thirty-four
 * normal rooms of ntsc-1.2 never ask for more than thirty-two at a time, but
 * the actor objects share the same ucode and cache, so the decoder keeps the
 * full eighty rather than the room-measured thirty-two. */
#define F3_VTX_MAX     80
#define F3_MTX_SIZE    64
#define F3_MTX_STACK   10
#define F3_DL_DEPTH    8

/* N64 vertex: x,y,z (s16), flag (u16), s,t (s16), rgba. */
typedef struct {
    s16 x, y, z;
    u16 flag;
    s16 s, t;
    u8 r, g, b, a;
} F3Vtx;

_Static_assert(sizeof(F3Vtx) == 16, "F3Vtx must match the N64 layout");

/* Called for every decoded triangle while f3Run walks a list. Indices refer
 * to st->vtx, so a renderer can read position, uv and color from there. */
typedef void (*F3TriHook)(void *ctx, const F3Vtx *vtx, u32 a, u32 b, u32 c);

/* Called on G_MTX and G_POPMTX. For F3_MTX_EV_LOAD, mtx is the 64 bytes that
 * were addressed by the command and arg is the params byte (see F3_MTX_*).
 * For F3_MTX_EV_POP, arg is how many levels were popped. */
typedef void (*F3MtxHook)(void *ctx, u32 ev, const u8 *mtx, u32 arg);

enum {
    F3_MTX_EV_LOAD = 0,
    F3_MTX_EV_POP  = 1,
};

enum {
    F3_MTX_PUSH = 0x01,
    F3_MTX_LOAD = 0x02,
    F3_MTX_PROJ = 0x04,
};

/* RDP texture state as described by the display list. The real extent comes
 * from the render tile size: gDPLoadTextureBlock passes width 1 to
 * G_SETTIMG and describes the texture with G_SETTILESIZE instead.
 *
 * w/h are in texels; loadWords is the number of 16-bit words from the last
 * G_LOADBLOCK (the RDP expands it by 2 or 4 for 8b/4b formats). Coordinates
 * are in 10.5 fixed. tlut/tlutCount are the palette entry point and entry
 * count captured from G_LOADTLUT for the render tile's palette slot, and are
 * only set for CI images.
 *
 * imgBe and tlutBe say whether those bytes came out of a segment marked with
 * f3SetSegmentBe. Only the sixteen-bit readers care: a CI texel is one byte,
 * so an 8bpp image reads the same either way, but an A1R5G5B5 texel and a
 * palette entry are 16-bit words the N64 stored the other end first. */
typedef struct {
    const u8 *img;
    u32 imgFmt;
    u32 imgSiz;
    u32 imgWidth;
    u32 imgBe;
    u32 fmt;
    u32 siz;
    u32 line;
    u32 tmem;
    u32 palette;
    u32 w;
    u32 h;
    u32 loadWords;
    u32 on;
    u32 tile;
    const u8 *tlut;
    u32 tlutCount;
    u32 tlutBe;
    bool ready;
    /* Which of the two render tiles this describes. The RDP can sample two
     * tiles in one cycle and OoT uses that for its 50/50 detail blend; the DS
     * has one texture stage, so the second descriptor has to be carried apart
     * from the first instead of replacing it. */
    u32 slot;
    /* Set for the second tile of a two-image material. The RDP's output alpha
     * for the lerp comes from the alpha mux, not from this image, and OoT's
     * detail overlays are RGB with the alpha bit clear in every texel; the DS
     * takes the pixel's alpha from the texel, so the bake has to write it
     * opaque or the global cutout drops the whole second pass. */
    u32 opaque;
} F3Tex;

enum {
    F3_TEX_EV_READY = 0, /* render tile described: w/h/img valid */
    F3_TEX_EV_ON    = 1,
    F3_TEX_EV_OFF   = 2,
};

typedef void (*F3TexHook)(void *ctx, u32 ev, const F3Tex *tex);

/* Decoded from G_SETCOMBINE. The N64 combiner is a two-cycle (A-B)*C+D mux
 * network; the DS has a fixed texel*shade pipeline, so only the cycle-0 colour
 * muxes are classified, and only into the shapes the GE can approximate. */
enum {
    F3_CC_MODULATE = 0, /* TEXEL0 * SHADE (the DS default) */
    F3_CC_DECAL,        /* TEXEL0 replaces the vertex colour */
    F3_CC_SHADE,        /* vertex colour, texture ignored */
    F3_CC_OTHER,
    /* (TEXEL1 - TEXEL0) * ENVA + TEXEL0: two images blended by the env alpha.
     * Its own mode because it is the one OTHER shape this backend can now
     * express, and because it needs a second texture descriptor to do it. */
    F3_CC_LERP,
};

typedef struct {
    u32 mode;  /* F3_CC_* */
    u32 alpha; /* G_ACMUX_* that feeds the cycle-0 alpha output */
    /* G_SETENVCOLOR. Only the alpha byte is read, and only by F3_CC_LERP,
     * where it is the blend weight. envValid says the list actually set it:
     * a copy of the room alone never does, because OoT sets this once per
     * scene from the scene's own draw config. */
    u32 env;
    u32 envValid;
} F3Combiner;

/* Cycle-0 alpha sources the translator cares about, with the same values as
 * G_ACMUX_* in gbi.h. TEXEL0 is the one that matters most: it is OoT's cutout
 * material, where the texel's own alpha decides which pixels are drawn. */
enum {
    F3_AC_COMBINED = 0,
    F3_AC_TEXEL0   = 1,
    F3_AC_SHADE    = 4,
    F3_AC_1        = 6,
    F3_AC_0        = 7,
};

typedef void (*F3CCHook)(void *ctx, const F3Combiner *cc);

/* G_SETGEOMETRYMODE bits this backend reads. Only three of them change what
 * the DS pipeline has to do; the rest are carried and ignored. */
enum {
    F3_GEOM_SHADE         = 0x00000004u,
    F3_GEOM_FOG           = 0x00010000u,
    F3_GEOM_LIGHTING      = 0x00020000u,
    F3_GEOM_SHADING_SMOOTH = 0x00200000u,
};

/* Called after G_SETGEOMETRYMODE/G_CLEARGEOMETRYMODE with the new word. The
 * flag matters to the renderer: with G_LIGHTING clear the RSP passes each
 * vertex's cn bytes straight through as SHADE, which is what OoT's room lists
 * rely on for every textured material; with it set they are a diffuse material
 * the RSP still has to light, and this backend has no lighting stage. */
typedef void (*F3GeomHook)(void *ctx, u32 geometryMode);

/* OtherMode: the RDP's 64-bit render-state register, shadowed in the RSP as
 * two 32-bit halves. gbi.h's masks are pre-shifted inside their own half
 * (G_MDSFT_* is the LSB-relative bit index), so the payload of a
 * G_SETOTHERMODE_H/L lands in these values without further shifting. */
typedef struct {
    u32 hi; /* cycle type, texture LUT/filter/persp, dithers, combine key */
    u32 lo; /* alpha compare, depth source, 12-bit render mode, blender */
} F3Om;

/* What the DS translator reads out of OtherMode. Everything else in the
 * register (dither patterns, LOD, perspective correction) has no GE
 * equivalent and stays unclassified. */
enum {
    F3_OM_XLU     = 1u << 0, /* blends: ZMODE other than OPA, or FORCE_BL with
                              * the cycle-1 blend colour set to CLR_MEM */
    F3_OM_DECAL   = 1u << 1, /* ZMODE_DEC: composites over the background */
    F3_OM_KEY     = 1u << 2, /* G_CK_KEY: cycle-1 chromatic key gate */
    F3_OM_2CYC    = 1u << 3, /* cycle type other than G_CYC_1CYCLE */
    F3_OM_TTALPHA = 1u << 4, /* G_TT_RGBA16/IA16: CI palettes carry alpha */
    F3_OM_FILT    = 1u << 5, /* texture filter other than G_TF_POINT */
};

/* G_MDSFT_ALPHACOMPARE, the 2-bit field at the bottom of the lo half. The
 * threshold mode compares the pixel alpha against the primitive colour's, so
 * the value itself comes from G_SETPRIMCOLOR. */
enum {
    F3_ALPHA_CMP_NONE      = 0,
    F3_ALPHA_CMP_THRESHOLD = 1,
    F3_ALPHA_CMP_DITHER    = 3,
};

typedef void (*F3OmHook)(void *ctx, const F3Om *om, u32 flags, u32 acMode);

/* G_MOVEMEM VIEWPORT: scale/translate in 2.2 fixed, 320x240 screen space. */
typedef struct {
    s16 scale[4];
    s16 trans[4];
} F3Vp;

typedef void (*F3VpHook)(void *ctx, const F3Vp *vp);

/* One palette loaded by G_LOADTLUT. CI4 palettes occupy slot 0-15 (16 entries
 * each, TMEM 256 + slot * 16); CI8 uses slot 0 with 256 entries. */
typedef struct {
    const u8 *img;
    u32 count;
    u32 be;
} F3Tlut;

typedef struct {
    F3Vtx vtx[F3_VTX_MAX];
    u32 geometryMode;
    u32 vtxCmds;
    u32 triCount;
    u32 rdpOps;
    u32 texChanges;
    u32 imgChanges;
    u32 texLoads;
    u32 tlutLoads;
    u32 ccChanges;
    u32 vpLoads;
    u32 omChanges;
    u32 omFlags;
    u32 omAc;
    u32 mtxPush;
    u32 mtxPop;
    u32 mtxDepth;
    u32 ops;
    u32 unsupported;
    u32 lastOpcode;
    /* Commands the decoder stepped over without ending the list. A room's own
     * lists mostly live in the room segment, but the one hundred thirty-four
     * normal rooms of ntsc-1.2 also point forty-five commands at segments the
     * port has not loaded, and losing the whole room for one of them was worse
     * than losing that command. These count what was dropped. */
    u32 skips;
    u32 skipAddr;   /* of those, unresolved segment addresses */
    u32 skipVtx;    /* of those, vertex batches outside the cache */
    u32 err;
    u32 texOn;
    u32 texTile;
    u32 loadTmem;
    /* The image the last G_LOADBLOCK actually copied into TMEM, with its byte
     * order. A render tile is described by G_SETTILESIZE before the palette
     * that belongs to it is loaded, because that is the order the room's own
     * lists use, and by then G_SETTIMG has already moved tex.img on to the
     * palette. The deferred bind reads the image back from here. */
    const u8 *blkImg;
    u32 blkImgBe;
    /* One CI tile is waiting for the G_LOADTLUT that completes it. */
    u32 texWaitTlut;
    F3Tlut tluts[16];
    F3Tex tex;
    /* The second render tile, for the two-image combiner. It is described by
     * the same G_SETTIMG/G_SETTILE/G_SETTILESIZE sequence as the first, with
     * tile number 1, and kept apart so the first tile's material stays live
     * while both are on screen. */
    F3Tex tex1;
    u32 tex1Loads;   /* second tiles described and complete */
    /* G_SETENVCOLOR as last written. */
    u32 env;
    u32 envValid;
    F3Combiner cc;
    F3Vp vp;
    F3Om om;
    u8 mtx[F3_MTX_SIZE];
    u8 mtxStack[F3_MTX_STACK][F3_MTX_SIZE];
    const u8 *seg[16];
    /* One bit per segment: which of them holds N64 memory rather than a blob
     * this CPU wrote. A room file is DMA'd verbatim out of the cartridge, so
     * its commands, vertices, viewports and matrices are all big-endian, while
     * the demo and the game's own Gfx_Alloc blob are native little-endian. The
     * unit differs per structure (commands and matrices are 32-bit, vertices
     * and viewports are 16-bit fields), so it is the segment's byte order that
     * decides, not a flag on the state. */
    u32 segBe;
    F3TriHook triHook;
    void *triCtx;
    F3MtxHook mtxHook;
    void *mtxCtx;
    F3TexHook texHook;
    void *texCtx;
    F3CCHook ccHook;
    void *ccCtx;
    F3VpHook vpHook;
    void *vpCtx;
    F3OmHook omHook;
    void *omCtx;
    F3GeomHook geomHook;
    void *geomCtx;
} F3State;

void f3Init(F3State *st);
void f3Reset(F3State *st);
void f3SetSegment(F3State *st, u32 seg, const void *base);
/* Marks a segment as holding N64-order bytes. Segment 0 is never resolved, and
 * a segment cleared with f3SetSegment(…, NULL) needs no clearing here: nothing
 * resolves through it either way. */
void f3SetSegmentBe(F3State *st, u32 seg, bool be);
void f3SetTriHook(F3State *st, F3TriHook hook, void *ctx);
void f3SetMtxHook(F3State *st, F3MtxHook hook, void *ctx);
void f3SetTexHook(F3State *st, F3TexHook hook, void *ctx);
void f3SetCCHook(F3State *st, F3CCHook hook, void *ctx);
void f3SetVpHook(F3State *st, F3VpHook hook, void *ctx);
void f3SetOmHook(F3State *st, F3OmHook hook, void *ctx);
void f3SetGeomHook(F3State *st, F3GeomHook hook, void *ctx);

/* Walks a display list out of RAM. The list passed here is in the host's own
 * order; a list reached through a segment marked with f3SetSegmentBe is read as
 * N64 bytes, and so is every structure its commands address. Returns false on
 * the first error, which is also stored in st->err. */
bool f3Run(F3State *st, const u8 *dl);

#endif
