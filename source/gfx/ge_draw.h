#ifndef OCARINA_GFX_GE_DRAW_H
#define OCARINA_GFX_GE_DRAW_H

#include <nds.h>
#include <stdbool.h>

#include "f3dex2.h"

/* Converts N64 world units to the 4.12 vertex format the geometry engine
 * takes: v16 = world << (12 - shift). shift 6 means one 4.12 unit = 64 world
 * units, so coordinates up to +/-511 stay in range. */
/* Sixteen was the first guess and a real room walked past it: an OoT room's
 * environment set plus its actors is tens of distinct images, and the DS's
 * 656 KB of VRAM holds sixty-four 32x32 RGBA16 textures with room to spare.
 * libnds itself caps texture names at 2048, so the limit here is memory, and
 * the LRU below is what keeps it that way. */
#define GE_TEX_CACHE 64
#define GE_TEX_MAX   128

/* Reports the two ratios the level test sees, once per frame, on the console:
 * the level histogram, the worst face's texture and screen areas, and how many
 * faces the guards turned away before they had areas. It divides per face and
 * it takes the last two HUD rows, so it is off in a build that is being timed
 * or photographed for anything but the level test itself. */
/* #define GE_LOD_PROBE */

typedef struct {
    const u8 *img;
    const u8 *tlut;  /* NULL for direct-colour textures */
    u32 w, h;
    u32 tlutCount;
    u32 ciAlpha;     /* the palette's alpha bit was live: G_TT_RGBA16/IA16 */
    u32 opaque;      /* the bake wrote every texel's alpha bit */
    u32 mip;         /* which MIP-equivalent of the image this slot holds */
    u32 stamp;       /* last use, for eviction */
    u32 frame;       /* frame that last uploaded it, see texPinned */
    int id;
} GeTexSlot;

typedef struct {
    u32 shift;
    u32 tris;
    u32 clamped;
    u32 mtxLoads;   /* G_MTX applied, modelview and projection alike */
    u32 projLoads;  /* of those, the ones aimed at the projection stack */
    u32 mtxPops;    /* levels popped by G_POPMTX */
    u32 mtxLeft;    /* levels still pushed when the list ended (should be 0) */
    u32 mtxDepth;
    u32 texBinds;    /* VRAM uploads this frame (cache misses) */
    u32 texRebinds;  /* glBindTexture calls this frame, uploads or not: a MIP
                      * chain that changes every triangle closes and reopens the
                      * batch every triangle, and that is FIFO traffic, not VRAM */
    u32 texHits;     /* reused textures this frame */
    u32 texSkip;     /* formats/sizes not representable on the DS */
    /* The first texture the engine refused, described. A room that skips forty
     * seven of them says nothing about why in the count, and the why is the
     * whole question, so the first rejection is kept here and printed. */
    u32 skipW;
    u32 skipH;
    u32 skipFmt;     /* G_IM_FMT_*: 0 RGBA, 2 CI */
    u32 skipSiz;     /* G_IM_SIZ_*: 0 4b, 1 8b, 2 16b */
    u32 skipLoad;    /* words G_LOADBLOCK said to copy */
    u32 skipTlut;    /* palette entries the decoder found */
    u32 skipWhy;     /* 1 side not a power of two, 2 no size code for it,
                      * 3 no palette, 4 palette too short, 5 load length
                      * mismatch, 6 format this engine cannot read */
    /* The first texture the engine accepted this frame, described the same way.
     * Whether the byte-order flags reach the upload is a question the picture
     * cannot answer on its own: a wrong palette and a wrong texel order both
     * look like noise, and only the flags say which one to go after. */
    u32 bindW;
    u32 bindFmt;
    u32 bindSiz;
    u32 bindLoad;
    u32 bindPal;     /* palette entries the decoder handed over */
    u32 bindBe;      /* the image's byte order flag at upload */
    u32 bindPalBe;   /* the palette's */
    const u8 *bindImg;   /* the two pointers, so the caller can turn them back
                          * into offsets in the file they came out of */
    const u8 *bindTlut;
    u32 texEvict;    /* cache slots recycled this frame */
    u32 texPinned;   /* uploads refused because every slot was still in use */
    u32 texNoVram;   /* glGenTextures ran out of texture RAM, so the name came
                      * back negative and nothing was uploaded into it */
    u32 bakePix;     /* texels uploaded since the last reset, so the grain below
                      * has a denominator */
    u32 bakeDl;      /* |luminance step| over every neighbour pair inside the
                      * uploaded squares: the same quantity the panel is measured
                      * with, so the two say directly how much of the screen's
                      * grain the texels can account for */
    u32 bakePairs;   /* how many of those neighbour pairs there were */
    u32 texMaxS;     /* widest texcoord any vertex carried, in the N64's 10.5
                      * units: /32 says how many texels one mapping spans */
    u32 texMaxT;
    /* The MIP chain the triangles actually asked for. The geometry engine has no
     * filter, so the only way a minified surface stops being aliasing is a
     * pre-filtered image, and the only way that does not also blur the surfaces
     * in front of the camera is choosing it per triangle. These say how often
     * that happened and how far down the chain it went. */
    u32 mipTris;     /* textured triangles drawn above level 0 */
    u32 mipBinds;    /* uploads of a level above 0 */
    u32 mipMax;      /* highest level any triangle asked for */
    u32 mipFail;     /* levels asked for that VRAM or the cache would not give */
#ifdef GE_LOD_PROBE
    /* What the level test actually sees, for the one face with the highest
     * minification in the frame. lodN counts the textured triangles by the level
     * they were drawn with.
     *
     * lodTexA and lodPxA are the two areas the test compares - twice the patch of
     * texture in N64 10.5 units squared, and twice its projected area in
     * sixteenths of a panel pixel squared - and lodRworst is 16 times the square
     * of their ratio, so 16 is one texel per pixel, 64 is two and 256 is four.
     * They are kept together because a level nobody can redo on paper is a level
     * nobody can trust: lodDs, lodDx, lodZ and lodK are two edge vectors of that
     * same face, in the units the test read them in. */
    u32 lodN[4];
    u32 lodRworst;
    u32 lodTexA;
    u32 lodPxA;
    u32 lodGm;   /* faces that gave up on the projection's own gains */
    u32 lodGw;   /* faces with a corner at or behind the eye */
    u32 lodGp;   /* faces whose projected area came out zero */
    u32 lodGt;   /* faces whose texture area came out zero */
    u32 lodDs;
    u32 lodDx;
    u32 lodZ;
    u32 lodK;
    /* The eye-space determinant against the shoelace: faces where the two
     * selectors disagree on the level this frame, and by how many levels at
     * most. Zero disagreement is what lets the cheaper formula replace the
     * one that shipped without changing a single drawn triangle. */
    u32 lodEyeDiv;
    u32 lodEyeMax;
    /* Faces whose projected corner the 32-bit clamp actually moved. At zero
     * the 32-bit shoelace and the 64-bit one draw the same picture. */
    u32 lodClamp;
#endif
    u32 texDrain;    /* mid-frame waits that proved the engine idle */
    u32 texDrainFail;/* waits that hit the poll cap and refused */
    u32 texDrainUs;  /* microseconds spent inside those waits this frame */
    u32 texDrainPoll;/* polls of the longest wait this frame */
    u32 texDrainStat;/* GXSTAT + polygon count when the last wait failed */
    u32 texDrainPoly;
    u32 texTris;     /* triangles submitted with a texture actually sampled */
    u32 palBinds;    /* uploads that baked a CI palette into their texels */
    u32 palTris;     /* of texTris, the ones using a paletted texture */
    u32 cutTris;     /* texTris whose cycle-0 alpha is the texel (cutout) */
    u32 xluTris;     /* triangles drawn while OtherMode asked for blending */
    u32 xluAlpha;    /* polygon alpha of the last blended triangle (0-31) */
    u32 omFlags;     /* F3_OM_* from the OtherMode shadow */
    u32 omAc;        /* G_MDSFT_ALPHACOMPARE: 0 none, 1 threshold, 3 dither */
    u32 omChanges;   /* G_SETOTHERMODE_H/L commands this frame */
    u32 polyFmt;     /* last GFX_POLY_FORMAT word written (alpha + polygon ID) */
    u32 polyChanges; /* batches reopened for a GFX_POLY_FORMAT change */
    u32 ccMode;      /* F3_CC_* currently driving the GE stage */
    u32 ccAlpha;     /* cycle-0 alpha source: feeds POLY_ALPHA when blending */
    u32 ccEnv;       /* G_SETENVCOLOR as decoded, when ccEnvValid */
    u32 ccChanges;   /* G_SETCOMBINE commands this frame */
    u32 geomMode;    /* G_SETGEOMETRYMODE shadow (F3_GEOM_*) */
    u32 shadeTris;   /* textured triangles drawn with the vertex colour */
    u32 litTris;     /* textured triangles whose SHADE the RSP would have lit */
    u32 otherTris;   /* textured triangles whose combiner is not modelled */
    u32 vpLoads;     /* viewport commands applied this frame */
    u8 vpX1, vpY1, vpX2, vpY2; /* last viewport corners applied; y from the bottom */
    u32 clipTris;    /* triangles that crossed the near plane and were cut */
    u32 clipDrop;    /* triangles thrown away because the cut left nothing */
    bool ok;
    m4x4 mtx;
    m4x4 proj;
    /* The modelview is applied here, not by the engine, so a vertex can be cut
     * against the near plane before it is submitted. The engine's own clipper
     * tests each position against w, so a vertex with a negative w has all six
     * of its planes flipped, and the eye coordinate it would have been given
     * does not fit the 1.19.12 the vertex command takes anyway. proj is shadowed
     * too, because the near plane is read out of it. */
    m4x4 shStack[F3_MTX_STACK]; /* the matrix each pushed level had to restore */
    s32 nearZ;       /* near plane in eye space; valid only with hasNear */
    bool hasNear;
    u8 mtxType[F3_MTX_STACK]; /* F3_MTX_PROJ or 0 per pushed level */
    GeTexSlot texCache[GE_TEX_CACHE];
    u32 texCacheCount;
    u32 texSlots;   /* how many of them are in play; the probe shrinks this to
                   * make the overflow path happen on demand */
    u32 texClock;   /* use counter for the LRU stamps, kept across frames */
    u32 frameStamp; /* counts geDrawList calls; a slot may only be recycled
                     * once no triangle of the current frame samples it */
    int texId;      /* texture from the last G_LOADBLOCK/SETTILESIZE */
    /* A copy of that tile's descriptor, kept because a triangle drawn later in
     * the list may need the *same image at another MIP level*, which means
     * finding or baking it again — and the decoder's own tile state moves on
     * between the two moments. */
    F3Tex texCur;
    u32 texMip;      /* level the currently bound id holds; 0 is the image as authored */
    /* The second tile of a two-image material, when the list described one.
     * It travels beside texCur rather than replacing it: the first pass needs
     * both, one after the other. */
    F3Tex texCur1;
    int texId1;
    u32 lerpTris;    /* triangles that got their second pass */
    u32 lerpFail;    /* second passes the texture cache would not pay for */
    u32 lerpEnv;     /* blend weight actually used, 0-31, from ENVA */
    u32 lerpEnvDefault; /* of those, the triangles whose factor came from the
                         * constant instead of from G_SETENVCOLOR */
    int whiteTexId; /* 8x8 opaque white, bound while G_TEXTURE is off */
    int boundId;
    bool drainGaveUp; /* a wait that timed out stops the others this frame */
    bool texOn;
    bool texBound;
    bool texPal;
    bool texLive;   /* bound texture is the scene texture, not the white helper */
    bool tex1Bound;
    bool ccEnvValid;
    bool lerpDraw;  /* the triangle being submitted wants a second pass */
    bool batch;
} GeDraw;

void geDrawInit(GeDraw *d, u32 shift);

/* Converts a 64-byte N64 matrix (16.16, row-major, translation in the 4th row
 * because libultra transforms row vectors) into the libnds m4x4 (1.19.12,
 * column-major, column vectors). Both layouts are the same flat array; the
 * translation column takes the 16.16 -> 1.19.12 step and the matrix's own
 * 2^shift, so a matrix offset is worth exactly what a vertex offset of the same
 * size is. Exposed so the conversion can be checked without the geometry
 * engine. */
void geMtxToDs(const u8 *src, u32 shift, m4x4 *out);

/* Checks geMtxToDs against a known rotation plus translation. */
bool geMtxSelfTest(void);

/* The 1.19.12 identity, in the engine's flat column-major layout. */
void geIdentityMtx(m4x4 *out);

/* One vertex through a modelview in that layout, in the engine's units. */
void geEyeOf(const m4x4 *m, s32 vx, s32 vy, s32 vz, s32 out[3]);

/* The near plane of a projection converted by geMtxToDs: the eye-space z where
 * the projection puts z' / w at -1, which is where a vertex stops having a
 * positive w. False for a projection with no near plane, i.e. an ortho one. */
bool geProjNearZ(const m4x4 *p, s32 *out);

/* A triangle cut against the plane z = plane, positions only. Returns how many
 * vertices it left, 0..4; `out` must hold four points. */
u32 geClipNearZ(const s32 in[3][3], s32 plane, s32 out[4][3]);

/* Resets st (keeping its segments), then submits the list. Both matrix
 * targets are converted and applied: G_MTX_PROJECTION loads go to the GE
 * projection stack, the rest to the modelview stack, and G_POPMTX pops from
 * the stack each pushed level was aimed at. The list must balance its pushes;
 * anything left over is unwound and reported in mtxLeft.
 *
 * G_SETCOMBINE is approximated with the GE's fixed texel*shade stage:
 * MODULATE runs natively, DECAL forces white vertex colours, SHADE binds the
 * white texture so the texture is ignored, and anything else falls back to
 * MODULATE. When the cycle-0 alpha source is the texel, the 1-bit alpha of an
 * N64 texel becomes the DS alpha test, which cuts the texel out instead of
 * painting it. The DS 3D palettes have no alpha channel, so a CI image whose
 * sampled entries ask for transparency is baked to direct colour at upload, and
 * one that only needs index 0 cut uses the GE's colour-0-transparent flag. G_MOVEMEM VIEWPORT maps the N64 320x240 viewport onto the DS
 * panel by the 4/5 factor (N64 pixels -> DS pixels).
 *
 * A texture slot may not be refilled while the geometry that samples it is
 * still queued: the GE reads VRAM when it renders, which happens after the CPU
 * has already moved on, so the upload DMA destroys pixels that not-yet-drawn
 * triangles are about to fetch. Slots are therefore pinned for the frame that
 * uploaded them. When an upload finds every slot pinned, the draw waits — for a
 * bounded time — for the engine to go idle (command FIFO empty, geometry engine
 * and matrix stacks not busy, polygon list RAM empty), and only then recycles,
 * because nothing queued samples texture memory any more. A list that still
 * finds nowhere to put a texture counts it in texPinned instead of overwriting
 * one.
 *
 * OtherMode decides whether a polygon blends at all: with the render mode's
 * ZMODE other than OPA, or with FORCE_BL and the cycle-1 blend colour set to
 * CLR_MEM, the material is translucent and its alpha comes from the combiner's
 * cycle-0 output. The GE has no vertex alpha, so a translucent triangle gets
 * one 5-bit POLY_ALPHA taken from its first vertex; the DS's blend mode is a
 * global DISP3DCNT bit, so blending stays enabled for the whole scene and
 * alpha 31 is what an opaque material writes.
 * A blended triangle also gets a running POLY_ID: the GE drops a translucent
 * pixel whose polygon ID equals the ID already stored at that screen point,
 * and an unset ID of 0 matches the backdrop, so ID 0 never composites. */
void geDrawList(GeDraw *d, F3State *st, const u8 *dl);

/* The bounded wait itself, exposed because the only way to know whether the
 * machine reports idle at all is to ask it and write down the answer. Returns
 * true when the engine said it was done with everything queued. */
bool geGpuDrain(GeDraw *d);

/* What the overflow path does on this machine, measured rather than assumed:
 * one drain right after a vblank, then the demo list drawn through a deliberately
 * small cache so the pinned path is taken, then the same list through the real
 * one. Everything the HUD has to show about the drain comes out of here. */
typedef struct {
    bool idleOk;       /* the wait after the vblank reported idle */
    u32 pinsSmall;     /* uploads refused with the small cache */
    u32 bindsSmall;    /* uploads that got through with the small cache */
    u32 drains;        /* waits that reported idle during that list */
    u32 fails;         /* waits that hit the bound */
    u32 polls;         /* MMIO reads of the longest wait */
    u32 us;            /* microseconds spent inside the waits */
    u32 stat;          /* GXSTAT when the last wait gave up */
    u32 poly;          /* polygons still queued at that moment */
    u32 bindsBig;      /* uploads with the real cache */
    u32 evictBig;      /* slots recycled there */
} GeDrainTest;

void geDrainProbe(GeDraw *d, F3State *st, const u8 *dl, GeDrainTest *t);

#endif
