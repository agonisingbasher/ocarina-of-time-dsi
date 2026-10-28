#include "ge_draw.h"

#include <string.h>

#include "timebase.h"

/* libultra stores 16.16 matrices row-major and transforms row vectors
 * (v * M); the DS stores 1.19.12 column-major and transforms column vectors
 * (M * v). Those are the same 16 values in the same order: element [row][col]
 * of the row-vector matrix is the transposed matrix's [col][row], which is
 * exactly the column-major slot. So no index swapping is needed, only the
 * fixed-point conversion.
 *
 * The translation column (flat 12..14) divides by 2^shift as well as by the
 * 16.16 -> 1.19.12 step, and the coordinate slots divide by the 1.19.12 step
 * only. Measured, not derived: with the game's list on screen alone, its
 * rotation and its seam both pinned to zero, the red/green seam sits at column
 * 127.5 - the panel's own centre - and the pair spans columns 64..190 for the
 * 384 world units between its outer corners, 0.331 columns each. That is what
 * guPerspective predicts: 1.299 * 192 / 500 of clip space is 0.4987, and half a
 * panel is 127.5 columns, so 63.6. Setting only the matrix's x translation to
 * 60 moves the seam to column 146.5, 19 columns for 60 world units, 0.317 each,
 * within four per cent of what a vertex offset of the same size is worth. A
 * matrix offset and a vertex offset are therefore worth the same to the engine,
 * so the two slots share the divisor, and the extra factor of 16 that a 1.19.12
 * element against a 16.16 w "should" cost is not there: dividing by 2^(8 +
 * shift) instead made the game's quads leave the panel entirely.
 *
 * Projections use the same path: guPerspective puts the z' row's w term
 * 2nf/(n-f) in mf[3][2], which is flat 14, so it takes the shift too. At shift
 * 8 that term lands as -500 / 2^8 * 2^12, and the quads at z = -500 come out at
 * the clip z / w the RSP reports for the same vertex; dividing it by a further
 * 16 took the depth out of the DS's clip range and painted nothing at all. */
static void mtxToDs(const u8 *src, u32 shift, m4x4 *out)
{
    const u16 *ip = (const u16 *)src;
    const u16 *fp = (const u16 *)(src + 32);
    u32 j;

    for (j = 0; j < 16; j++) {
        s64 v = ((s64)(s16)ip[j] << 16) + (s64)fp[j];

        v >>= (j >= 12 && j < 15) ? 4 + shift : 4;
        out->m[j] = (s32)v;
    }
}

void geMtxToDs(const u8 *src, u32 shift, m4x4 *out)
{
    mtxToDs(src, shift, out);
}

void geIdentityMtx(m4x4 *out)
{
    u32 i;

    for (i = 0; i < 16; i++) {
        out->m[i] = (i % 5 == 0) ? 4096 : 0;
    }
}

/* One vertex through a modelview already in the engine's layout. Positions
 * only, in the engine's units: the caller does the world -> unit scaling. */
void geEyeOf(const m4x4 *m, s32 vx, s32 vy, s32 vz, s32 out[3])
{
    const int *p = m->m;
    s64 x = vx;
    s64 y = vy;
    s64 z = vz;

    out[0] = (s32)(((s64)p[0] * x + (s64)p[4] * y + (s64)p[8] * z
                   + ((s64)p[12] << 12)) >> 12);
    out[1] = (s32)(((s64)p[1] * x + (s64)p[5] * y + (s64)p[9] * z
                   + ((s64)p[13] << 12)) >> 12);
    out[2] = (s32)(((s64)p[2] * x + (s64)p[6] * y + (s64)p[10] * z
                   + ((s64)p[14] << 12)) >> 12);
}

/* The near plane the eye-space cut uses, for a projection already converted by
 * geMtxToDs. Defined below with the rest of the matrix helpers; the self test
 * that pins it comes first. */
static bool projNearZ(const m4x4 *p, s32 *out);

bool geMtxSelfTest(void)
{
    /* Rz(30) then T(0,0,32), laid out the way libultra does it: the rotation
     * in rows 0-2 and the translation in mf[3][0..2], i.e. flat 12..14. At
     * shift 6 a vertex coordinate of 32 is submitted as 32 * 2^(12-6) = 2048 of
     * the rasterizer's units, so the translation slot has to read 2048 too: a
     * matrix offset and a vertex offset are worth the same to the engine, which
     * is what the on-panel measurement of the previous phase pinned. Dividing
     * the slot by a further 16 (2^(8+shift)) made it 128 and the game's quads
     * left the panel. */
    static const u16 ip[16] = {
        0, 0xFFFF, 0, 0,
        0, 0, 0, 0,
        0, 0, 1, 0,
        0, 0, 32, 1,
    };
    static const u16 fp[16] = {
        56755, 0x8000, 0, 0,
        32768, 56755, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    };
    /* guPerspective(1, 2, near 100, far 300): x scale 1, y scale 2,
     * z scale -2 at flat 10, -1 at flat 11 and the w term -300 at flat 14.
     * That term multiplies the input w like a translation does, so it lands as
     * -300 * 2^(8-6) = -1200, and -1200 * 16 is the -300 / 4 the eye space
     * wanted. */
    static const u16 pip[16] = {
        1, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, 0xFFFE, 0xFFFF,
        0, 0, 0xFED4, 0,
    };
    u8 raw[F3_MTX_SIZE];
    m4x4 m;

    memcpy(raw, ip, sizeof(ip));
    memcpy(raw + 32, fp, sizeof(fp));
    geMtxToDs(raw, 6, &m);

    if (!(m.m[0] == 3547 && m.m[1] == -2048 &&
          m.m[4] == 2048 && m.m[5] == 3547 &&
          m.m[3] == 0 && m.m[7] == 0 && m.m[11] == 0 &&
          m.m[10] == 4096 && m.m[14] == 2048 && m.m[15] == 4096)) {
        return false;
    }

    memset(raw, 0, sizeof(raw));
    memcpy(raw, pip, sizeof(pip));
    geMtxToDs(raw, 6, &m);

    if (!(m.m[0] == 4096 && m.m[5] == 8192 &&
          m.m[10] == -8192 && m.m[11] == -4096 && m.m[14] == -19200 &&
          m.m[3] == 0 && m.m[7] == 0 && m.m[15] == 0)) {
        return false;
    }

    /* The same projection read as a near plane: -19200 / 2^(12-6) is the world
     * -300 that guPerspective was given, and the plane where z' / w is -1 is
     * the near one, at world -100, i.e. eye z = -100 * 64. */
    {
        s32 nz = 0;

        return projNearZ(&m, &nz) && nz == -6400;
    }
}

static void setMtxMode(bool proj)
{
    glMatrixMode(proj ? GL_PROJECTION : GL_MODELVIEW);
}

static void identityMtx(m4x4 *out)
{
    u32 i;

    for (i = 0; i < 16; i++) {
        out->m[i] = (i % 5 == 0) ? 4096 : 0;
    }
}

/* C = A * B, both in the geometry engine's 1.19.12 and its flat column-major
 * layout, so a vertex gets B first and then A. The translation column is in the
 * same units as a vertex coordinate, which is what makes the plain fixed-point
 * product correct here: A's translation arrives multiplied by B's 1.0. */
static void mulMtx(m4x4 *out, const m4x4 *a, const m4x4 *b)
{
    u32 c;
    u32 r;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            s64 sum = (s64)a->m[r] * b->m[4 * c]
                    + (s64)a->m[4 + r] * b->m[4 * c + 1]
                    + (s64)a->m[8 + r] * b->m[4 * c + 2]
                    + (s64)a->m[12 + r] * b->m[4 * c + 3];

            out->m[4 * c + r] = (s32)(sum >> 12);
        }
    }
}

/* Where the near plane is, read out of the projection the list loaded.
 * guPerspective and glFrustumf32 write the same sixteen values: row z' takes
 * m[10] * z + m[14] and w takes m[11] * z, both in 1.19.12, so the plane where
 * z' / w is -1 sits at z = -m[14] * 4096 / (m[10] + m[11]). An orthographic
 * projection has m[11] = 0, no w to speak of, and nothing to clip against. */
static bool projNearZ(const m4x4 *p, s32 *out)
{
    s64 den = (s64)p->m[10] + p->m[11];

    if (p->m[11] == 0 || den == 0) {
        return false;
    }
    *out = (s32)(-((s64)p->m[14] << 12) / den);
    return true;
}

static void syncNear(GeDraw *d)
{
    d->hasNear = projNearZ(&d->proj, &d->nearZ);
}

/* Pops n levels. Only the projection ones exist in the engine any more: the
 * modelview is applied here, so its levels live in shStack. */
static void popLevels(GeDraw *d, u32 n)
{
    while (n-- && d->mtxDepth) {
        d->mtxDepth--;
        if (d->mtxType[d->mtxDepth] != 0) {
            setMtxMode(true);
            glPopMatrix(1);
            d->proj = d->shStack[d->mtxDepth];
        } else {
            d->mtx = d->shStack[d->mtxDepth];
        }
    }
    setMtxMode(false);
    syncNear(d);
}

static void onMtx(void *ctx, u32 ev, const u8 *mtx, u32 arg)
{
    GeDraw *d = (GeDraw *)ctx;
    m4x4 ds;
    bool proj;

    if (ev == F3_MTX_EV_POP) {
        popLevels(d, arg);
        d->mtxPops += arg;
        return;
    }

    proj = (arg & F3_MTX_PROJ) != 0;

    if (arg & F3_MTX_PUSH) {
        /* f3dex2.c rejects a push past F3_MTX_STACK, so this cannot overflow */
        d->shStack[d->mtxDepth] = proj ? d->proj : d->mtx;
        d->mtxType[d->mtxDepth++] = proj;
        if (proj) {
            setMtxMode(true);
            glPushMatrix();
        }
    } else {
        setMtxMode(proj);
    }

    mtxToDs(mtx, d->shift, &ds);
    if (proj) {
        setMtxMode(true);
        if (arg & F3_MTX_LOAD) {
            d->proj = ds;
            glLoadMatrix4x4(&ds);
        } else {
            m4x4 r;

            /* The RSP computes new * current; transposed for column vectors
             * that is current * new, which is what the GE's multiply does. */
            mulMtx(&r, &d->proj, &ds);
            d->proj = r;
            glMultMatrix4x4(&ds);
        }
    } else if (arg & F3_MTX_LOAD) {
        /* The engine keeps an identity modelview on purpose: the near cut wants
         * the vertex in eye space, and only that cut keeps the submitted
         * coordinates inside the 1.19.12 the vertex command takes. Measured with
         * one corner of a quad pushed to world z = +20000, behind a near plane
         * at -200: uncut its eye z is 320000, the clamp counter reads 1 and the
         * engine's own clipper - which tests position against w, so a negative w
         * flips every plane - lands the cut edge 14 panel pixels inside where
         * the RSP's near cut puts it. Cut first, nothing clamps. */
        d->mtx = ds;
    } else {
        m4x4 r;

        mulMtx(&r, &d->mtx, &ds);
        d->mtx = r;
    }
    setMtxMode(false);
    syncNear(d);

    d->mtxLoads++;
    if (proj) {
        d->projLoads++;
    }
}

/* The DS only samples square power-of-two textures, so everything else needs
 * to be baked (padded or packed into an atlas) before it can be bound. This
 * path handles what OoT streams from ROM directly: RGBA16, CI4 and CI8. */
static u16 sTexScratch[GE_TEX_MAX * GE_TEX_MAX];
static u16 sPalScratch[256];
static u16 sRowScratch[GE_TEX_MAX];
/* The biggest MIP level the chain can hand over is half the side, so a quarter
 * of the full square holds it. */
static u16 sMipScratch[(GE_TEX_MAX / 2) * (GE_TEX_MAX / 2)];

static int texSizeEnum(u32 n)
{
    switch (n) {
    case 8:   return TEXTURE_SIZE_8;
    case 16:  return TEXTURE_SIZE_16;
    case 32:  return TEXTURE_SIZE_32;
    case 64:  return TEXTURE_SIZE_64;
    case 128: return TEXTURE_SIZE_128;
    default:  return -1;
    }
}

/* One entry of an N64 A1R5G5B5 word to the DS's A1B5G5R5. Used for both texels
 * and CI palette entries. */
static u16 toDsColor(u16 c)
{
    /* N64 A1R5G5B5 -> DS A1B5G5R5: the single alpha bit stays where it is and
     * the red and blue halves trade places. The alpha matters more than the
     * colour here: the GE stores a pixel's alpha as texel alpha times polygon
     * alpha, and a pixel left with alpha 0 is discarded by the alpha test and
     * can never be blended into again. */
    return (u16)((c & 0x8000u) |
                 ((c >> 10) & 0x001Fu) |
                 (c & 0x03E0u) |
                 ((c & 0x001Fu) << 10));
}

static u32 texSide(const F3Tex *t);

/* Luminance of one A1B5G5R5 texel on its own 0-31 channels, so a texture's
 * grain and the screen's are measured on the same scale. */
static u32 texLum(u16 c)
{
    return (((u32)(c & 0x1Fu) * 30u + ((u32)(c >> 5) & 0x1Fu) * 59u +
             ((u32)(c >> 10) & 0x1Fu) * 11u) / 100u);
}

/* Zero means the DS can hold this texture. Anything else is the reason it
 * cannot, and is reported through GeDraw.skipWhy for the first refusal of a
 * list so a room full of skips can be read without a debugger. */
static u32 texReject(const F3Tex *t)
{
    u32 words;

    if (texSide(t) == 0) {
        /* 1 = a non-square tile whose long side is not a power of two, 2 = a
         * square one the GE has no size code for or that is too big. */
        return (t->w == t->h) ? 2u : 1u;
    }
    /* G_LOADBLOCK counts 16-bit words: 4 CI4 texels, 2 CI8 texels or one
     * RGBA16 texel per word. */
    switch (t->fmt) {
    case 0: /* G_IM_FMT_RGBA */
        if (t->siz != 2) {
            return 6;
        }
        words = t->w * t->h;
        break;
    case 2: /* G_IM_FMT_CI */
        if (t->siz == 0) {
            words = (t->w * t->h) / 4u;
        } else if (t->siz == 1) {
            words = (t->w * t->h) / 2u;
        } else {
            return 6;
        }
        if (t->tlut == NULL) {
            return 3;
        }
        if (t->tlutCount < (t->siz == 0 ? 16u : 256u)) {
            return 4;
        }
        break;
    case 3: /* G_IM_FMT_IA */
    case 4: /* G_IM_FMT_I */
        if (t->siz == 0) {
            words = (t->w * t->h) / 4u;
        } else if (t->siz == 1) {
            words = (t->w * t->h) / 2u;
        } else {
            words = t->w * t->h;
        }
        break;
    default:
        return 6;
    }
    return t->loadWords == words ? 0u : 5u;
}

static bool texSupported(const F3Tex *t)
{
    return texReject(t) == 0u;
}

/* N64 RGBA16 textures live in ROM as linear rows of A1R5G5B5; the DS 3D engine
 * also reads textures as linear raster, only in A1B5G5R5. So this is a
 * per-texel channel swizzle: the 8x8 tiling happens inside the N64's TMEM load,
 * not in the asset. Confirmed in melonDS: pre-tiling the data produces stripes. */
static u16 read16(const u8 *p, u32 be)
{
    u16 c = (u16)((u32)p[0] | ((u32)p[1] << 8));

    return be ? (u16)__builtin_bswap16(c) : c;
}

/* The DS has no alpha in the 3D colour-lookup path, and the intensity formats
 * carry no colour either, so an I or IA texel becomes a grey A1B5G5R5 one. The
 * alpha is the bit that decides whether the texel survives the alpha test. */
static u16 dsGrey(u32 i8, u32 a8)
{
    u32 r = (i8 >> 3) & 0x1Fu;

    return (u16)(((a8 & 0x80u) ? 0x8000u : 0u) | (r << 10) | (r << 5) | r);
}

/* One texel of an N64 image, already in the DS's own A1B5G5R5. `p` is the index
 * inside the image's own w-by-h raster; the CI formats look the palette up,
 * which the caller has converted once for the whole table. */
static u16 texelAt(const F3Tex *t, const u16 *pal, u32 p)
{
    const u8 *src = t->img;

    if (t->siz == 2) {
        u16 c = read16(src + p * 2u, t->imgBe);

        if (t->fmt == 0) {
            return toDsColor(c);                    /* RGBA16 */
        }
        if (t->fmt == 3) {
            return dsGrey(c >> 8, c & 0xFFu);       /* IA16 */
        }
        return dsGrey(c >> 8, 0xFFu);               /* I16 */
    }
    if (t->siz == 1) {
        u32 b = src[p];

        if (t->fmt == 2) {
            return pal[b];                          /* CI8 */
        }
        if (t->fmt == 3) {
            return dsGrey((b >> 4) * 17u, (b & 15u) * 17u);   /* IA8 */
        }
        return dsGrey(b, 0xFFu);                    /* I8 */
    }
    {
        /* The N64 packs the left texel in the high nibble of each byte. */
        u32 n = (p & 1u) ? (u32)(src[p >> 1] & 15u) : (u32)(src[p >> 1] >> 4);

        if (t->fmt == 2) {
            return pal[n];                          /* CI4 */
        }
        if (t->fmt == 3) {
            return dsGrey(((n >> 2) & 3u) * 85u, (n & 3u) * 85u);   /* IA4 */
        }
        return dsGrey(n * 17u, 0xFFu);              /* I4 */
    }
}

/* The GE only samples square, power-of-two textures, and a real room is full of
 * 16x32 and 32x64 tiles, so an image is baked into the smallest square that
 * holds it. The side is a whole multiple of both w and h, which is what lets
 * texFill tile it. */
static u32 texSide(const F3Tex *t)
{
    u32 side = (t->w > t->h) ? t->w : t->h;

    if (side < 8 || side > GE_TEX_MAX || (side & (side - 1u))) {
        return 0;
    }
    return side;
}

/* Replaced by the MIP chain below: a blanket low-pass on every image blurs the
 * surfaces that are already close to the camera along with the minified ones, and
 * measured on Bmori1_room_0 that plateaued the panel grain at 17.25 / 14.04 /
 * 12.21 for 1 / 2 / 3 passes instead of fixing it. */
#define GE_TEX_SMOOTH 0u

/* N64 vertex texcoords to the DS's 12.4: one texel is 32 units on the N64 side
 * and 16 on the DS's, so one texel of world is one texel of image and a plain
 * >>1 is the whole conversion. Measured at 14336 (448 texels) for the widest
 * vertex this room has, against a wall about 150 pixels across: the mapping is
 * right and the texture is minified three times over. */
#define GE_TEX_SHIFT 1u

/* One level of the chain is half the side, and the geometry engine's smallest
 * texture code is 8x8, so a 64x64 image has three levels to offer and no more.
 * Zero switches the whole selection off, which is the only way to measure what
 * it is worth: with it off every upload is the image exactly as authored. */
#define GE_TEX_MIP_MAX 2u

/* What a triangle's texels-per-pixel ratio has to reach before it is rounded up
 * a level, in 1024ths. See mipOfTri: 2048 rounds up, 1448 rounds to nearest. */
#define GE_TEX_LOD_BIAS 2048u

/* 0 lets the ratio decide. Anything else draws every textured surface that has
 * a chain at that level, which is how the chain is asked whether it helps a
 * view the ratio says needs nothing. */
#define GE_TEX_MIP_FORCE 0u
/* Measurement switch, never shipped as 1: the level is computed and thrown
 * away, so the frame pays the selector but never changes texture. It splits
 * the chain's cost into "the arithmetic" and "the binds" with one build. */
#define GE_TEX_MIP_NULL 0

/* Measurement only, never shipped as 1: every material bind is redone even
 * when the id already bound is the same one. The pixels cannot change (same
 * texture, same geometry), so the delta in `s` over the extra count in `r` is
 * the price of the swap itself — the batch split plus glBindTexture — without
 * the uploads the mip chain also adds. */
#define GE_TEX_REBIND_PROBE 0

/* Where the projected area in mipOfTri comes from.
 *
 * 0 - the shoelace over the three projected corners: three reciprocals, a
 *     chain of 64-bit multiplies per corner, two clamps per corner.
 * 1 - the eye-space identity 2A = Sx Sy |det[x y w]| / (w0 w1 w2). No
 *     per-corner reciprocal and no clamp, and it is exact for the same
 *     projection the shoelace path already assumes (w = -z, no rotation
 *     between eye and clip x/y). The two disagree only where the shoelace's
 *     clamp or preshift rounding moves a face across a level boundary, so
 *     the probe below asks that face by face before this is trusted.
 * 2 - the same determinant with the product of the three depths replaced by
 *     the cube of their mean, which trades a 64-bit divide for three table
 *     multiplies. Faces that span a lot of depth are the ones it can judge
 *     differently; the probe counts exactly those. */
#define GE_MIP_EYE 0

/* Measurement only: compute both selectors, draw with the one GE_MIP_EYE
 * names, and count the faces where they disagree about the level and by how
 * much. Needs GE_LOD_PROBE. */
#define GE_MIP_EYE_PROBE 0

/* The four products of the shoelace are 64-bit because a projected corner can
 * be a million sixteenths from the centre. Nothing on screen is further than
 * about 192 of them, so 1 clamps each corner at three panel widths before the
 * shoelace, which lets the four products be 32-bit. The probe counts the faces
 * where the clamp actually moved a corner: at zero the picture cannot differ,
 * because every other face goes through the same integers. */
#define GE_MIP_AREA32 1
#define GE_MIP_CLAMP 12000

/* Textured MODULATE triangles with G_LIGHTING clear take their colour from the
 * vertex's cn bytes, which is what the room's list asks for. 0 reproduces the
 * panel of the build before this rule — every triangle white — and exists to
 * re-measure the difference without rebuilding anything else. */
#define GE_VTX_SHADE 1

/* The two-image lerp (TEXEL1-TEXEL0)*ENVA+TEXEL0. The GE has one texture
 * stage and no way to read the frame buffer as a source, so two images can
 * only be combined by drawing the surface twice: the first pass lays T0 down
 * as an opaque polygon, the second samples T1 through the blender with
 * POLY_ALPHA(ENVA), which the hardware computes as src*ENVA + dst*(1-ENVA).
 * With T0 already in the destination that is exactly the RDP's arithmetic.
 *
 *   0  T0 only — the build before this phase, kept so the panel can be
 *      compared against it without touching anything else
 *   1  T1 only, opaque, no first pass: a probe, not a shipping path. It says
 *      whether the second tile is the image the room means and whether it
 *      lines up with the same texture coordinates the first one uses.
 *   2  both passes
 */
#define GE_LERP 2

/* The blend weight when the list itself never wrote G_SETENVCOLOR. A room
 * file does not carry one: OoT sets the env colour once per scene from the
 * scene's draw config, and this build runs a room without its scene. Forest
 * Temple, and so Bmori1_room_0, gets 128 — half — from
 * Scene_DrawConfigForestTemple() in src/code/z_scene_table.c. lerpEnvDefault
 * counts the triangles that had to fall back to this so the choice stays
 * visible on the HUD instead of being invisible in a comment. */
#define GE_LERP_ENV 128u

/* The GE samples one texel per pixel with no filter and no mip chain, and a
 * measured room sits at about 0.6 screen pixels per texel, so a raw upload is
 * point-sampled between unrelated texels and shows as coloured confetti. The
 * N64 hid that behind its MIP levels. Averaging each texel with its neighbour is
 * the same low-pass one MIP step performs, done once in the bake instead of per
 * pixel in hardware the DS doesn't have. */
static u16 avg16(u16 a, u16 b)
{
    u32 r = ((u32)(a & 0x1Fu) + (b & 0x1Fu) + 1u) >> 1;
    u32 g = ((((u32)(a >> 5) & 0x1Fu) + ((b >> 5) & 0x1Fu) + 1u) >> 1) << 5;
    u32 bl = ((((u32)(a >> 10) & 0x1Fu) + ((b >> 10) & 0x1Fu) + 1u) >> 1) << 10;

    return (u16)(r | g | bl | ((u32)a & 0x8000u) | ((u32)b & 0x8000u));
}

/* Wrap matters here: the image is repeated across the square, so the edge
 * texels average with the far edge exactly as the N64's wrap did. */
static void texSmooth(u16 *dst, u32 side)
{
    u32 x, y;
    u32 last = side - 1u;

    for (y = 0; y < side; y++) {
        u16 *row = dst + y * side;
        u16 first = row[0];

        for (x = 0; x < last; x++) {
            row[x] = avg16(row[x], row[x + 1u]);
        }
        row[last] = avg16(row[last], first);
    }

    for (x = 0; x < side; x++) {
        sRowScratch[x] = dst[last * side + x];
    }
    /* Each row is rewritten from itself and the row below, which is still
     * unaveraged when it is read, so no second square-sized buffer is needed. */
    for (y = 0; y < last; y++) {
        u16 *row = dst + y * side;
        const u16 *next = row + side;

        for (x = 0; x < side; x++) {
            row[x] = avg16(row[x], next[x]);
        }
    }
    {
        u16 *row = dst + last * side;

        for (x = 0; x < side; x++) {
            row[x] = avg16(row[x], sRowScratch[x]);
        }
    }
}

/* One MIP level of an already baked square, box filtered over the blocks that
 * tile it: the same neighbourhood the N64's own chain averaged, done once here
 * instead of per pixel in hardware that does not exist. Level 0 is the caller's
 * square, so this is only ever asked for level 1 and deeper.
 *
 * A texel's alpha is the OR of the block, not its mean. The room's cutout
 * surfaces are texel-alpha holes, and a mean would turn one opaque texel in four
 * into a half-transparent one that the alpha test then keeps or drops on a coin
 * toss. */
static void texBox(const u16 *src, u32 side, u32 level, u16 *dst)
{
    u32 step = 1u << level;
    u32 out = side >> level;
    u32 sh = level * 2u;
    u32 half = 1u << (sh - 1u);
    u32 x, y;

    for (y = 0; y < out; y++) {
        u16 *row = dst + y * out;
        u32 ry = y << level;

        for (x = 0; x < out; x++) {
            u32 rx = x << level;
            u32 r = 0;
            u32 g = 0;
            u32 b = 0;
            u32 a = 0;
            u32 i;
            u32 j;

            for (j = 0; j < step; j++) {
                const u16 *rp = src + (ry + j) * side;

                for (i = 0; i < step; i++) {
                    u16 c = rp[rx + i];

                    r += (u32)(c & 0x1Fu);
                    g += (u32)((c >> 5) & 0x1Fu);
                    b += (u32)((c >> 10) & 0x1Fu);
                    a |= (u32)(c & 0x8000u);
                }
            }
            row[x] = (u16)((((r + half) >> sh) & 0x1Fu) |
                           ((((g + half) >> sh) & 0x1Fu) << 5) |
                           ((((b + half) >> sh) & 0x1Fu) << 10) | a);
        }
    }
}

static void texFill(const F3Tex *t, const u16 *pal, u16 *dst, u32 side)
{
    u32 x, y;
    u32 sx = 0;
    u32 sy = 0;

    /* Tile the image across the whole square, not just its own w x h, so the
     * DS's wrap at the square edge agrees with the N64's wrap at the image edge
     * and no fetch ever lands on texel 0, which is transparent black. */
    for (y = 0; y < side; y++) {
        u16 *row = dst + y * side;
        u32 p = sy * t->w;

        sx = 0;
        for (x = 0; x < side; x++) {
            row[x] = texelAt(t, pal, p + sx);
            if (++sx == t->w) {
                sx = 0;
            }
        }
        if (++sy == t->h) {
            sy = 0;
        }
    }

    /* Kept in a variable so a zero pass count does not read as dead code the
     * compiler complains about; the chain in mipOfTri is what replaced it. */
    {
        u32 smooth = GE_TEX_SMOOTH;

        for (y = 0; y < smooth; y++) {
            texSmooth(dst, side);
        }
    }

    /* After the smoothing, so the bit cannot be spread into a neighbour that
     * has to keep its own: an image baked for the second pass is opaque
     * everywhere, because the RDP's alpha for that pass comes from the
     * combiner's alpha mux and not from these texels. */
    if (t->opaque) {
        for (y = 0; y < side; y++) {
            u16 *row = dst + y * side;

            for (x = 0; x < side; x++) {
                row[x] |= 0x8000u;
            }
        }
    }
}

static void palConvert(const u8 *src, u32 count, u16 *dst, u32 be, u32 keepAlpha)
{
    u32 i;

    for (i = 0; i < count; i++) {
        u16 c = toDsColor(read16(src + i * 2u, be));

        dst[i] = keepAlpha ? c : (u16)(c | 0x8000u);
    }
}

static void batchBegin(GeDraw *d);
static void batchEnd(GeDraw *d);

/* GXSTAT (0x04000600) says how far the engine has got with what we queued:
 * bit 26 the command FIFO is empty, bit 27 no command is executing, bit 14 no
 * matrix push/pop is pending. Those three only cover the geometry engine, which
 * just transforms vertices and files polygons; the texels are read later, by the
 * renderer, one polygon at a time, and the counter beside GXSTAT says how many
 * are still in the list. So the polygon count is the bit that matters before a
 * texture is overwritten: while it is non-zero, a triangle that samples that
 * slot has yet to be drawn. */
#define GE_STAT_FIFO_EMPTY BIT(26)
#define GE_STAT_GE_BUSY    BIT(27)
#define GE_STAT_MTX_BUSY   BIT(14)
#define GE_POLY_COUNT      0x0FFFu

/* A drain that cannot finish must cost one refused upload, not a hung frame.
 * The poll count only decides how often the clock is read; the clock is what
 * bounds the wait, because a register read through the emulator's MMIO path
 * costs far more than the same read on hardware. Measured on melonDS: 400000
 * polls of this loop ate 236 ms of a 16 ms frame. */
#define GE_DRAIN_POLLS 1024u
#define GE_DRAIN_US     2000u

bool geGpuDrain(GeDraw *d)
{
    u32 polls = 0;
    u32 t0 = timeTicks();
    bool idle = false;

    for (;;) {
        u32 stat = GFX_STATUS;
        u32 polys = (u32)(GFX_POLYGON_RAM_USAGE & GE_POLY_COUNT);

        if ((stat & GE_STAT_FIFO_EMPTY) &&
            !(stat & (GE_STAT_GE_BUSY | GE_STAT_MTX_BUSY)) &&
            polys == 0) {
            idle = true;
            break;
        }
        polls++;
        if ((polls & (GE_DRAIN_POLLS - 1u)) == 0 &&
            timeTicksToUs(timeTicks() - t0) >= GE_DRAIN_US) {
            d->texDrainFail++;
            d->texDrainStat = stat;
            d->texDrainPoly = polys;
            d->drainGaveUp = true;
            break;
        }
    }

    /* Both exits pay for the reads, so both are worth counting: the number of
     * polls is what says how much the wait costs on hardware, where a register
     * read is a few cycles rather than the emulator's MMIO trap. */
    if (polls > d->texDrainPoll) {
        d->texDrainPoll = polls;
    }
    d->texDrainUs += timeTicksToUs(timeTicks() - t0);
    if (idle) {
        d->texDrain++;
    }
    return idle;
}

/* Every slot became safe to recycle the moment the engine went quiet, so the
 * frame stamp that pinned it is pushed back one level. The LRU stamps are left
 * alone: they are what decides which slot is refilled first. */
static void texUnpinAll(GeDraw *d)
{
    u32 j;

    for (j = 0; j < d->texSlots; j++) {
        if (d->texCache[j].frame == d->frameStamp) {
            d->texCache[j].frame = d->frameStamp - 1u;
        }
    }
}

/* Least recently used slot this frame has not uploaded. Returns -1 when every
 * slot is pinned, which is what a full room does sixteen textures in. */
static int texEvictScan(const GeDraw *d)
{
    int best = -1;
    u32 oldest = 0;
    u32 j;

    for (j = 0; j < d->texSlots; j++) {
        if (d->texCache[j].frame == d->frameStamp) {
            continue;
        }
        if (best < 0 || d->texCache[j].stamp < oldest) {
            oldest = d->texCache[j].stamp;
            best = (int)j;
        }
    }
    return best;
}

static int ensureTex(GeDraw *d, const F3Tex *t, u32 mip)
{
    u32 i;
    int id;
    bool pal = (t->fmt == 2);
    bool wasOpen = d->batch;
    u32 base = texSide(t);
    /* G_TT_NONE says a paletted image has no alpha channel at all: the RDP
     * takes the palette entries as opaque RGB and only G_TT_RGBA16/IA16 make
     * entry bit 15 mean anything. The GE has no way to ignore a texel's alpha
     * bit while the global alpha test is on, so the bake has to write it. */
    u32 ciAlpha = (d->omFlags & F3_OM_TTALPHA) ? 1u : 0u;
    u32 side;

    /* The engine has no texture size code below 8x8, so a 16x16 image stops at
     * one level and a 64x64 one at three. */
    while (mip > 0u && (base >> mip) < 8u) {
        mip--;
    }
    side = base >> mip;

    for (i = 0; i < d->texCacheCount; i++) {
        GeTexSlot *e = &d->texCache[i];

        if (e->img == t->img && e->w == t->w && e->h == t->h &&
            e->tlut == (pal ? t->tlut : NULL) &&
            e->tlutCount == (pal ? t->tlutCount : 0u) &&
            e->ciAlpha == (pal ? ciAlpha : 0u) && e->mip == mip &&
            e->opaque == t->opaque) {
            e->stamp = ++d->texClock;
            e->frame = d->frameStamp;
            d->texHits++;
            return e->id;
        }
    }

    if (!texSupported(t)) {
        if (d->texSkip == 0u) {
            d->skipWhy = texReject(t);
            d->skipW = t->w;
            d->skipH = t->h;
            d->skipFmt = t->fmt;
            d->skipSiz = t->siz;
            d->skipLoad = t->loadWords;
            d->skipTlut = t->tlutCount;
        }
        d->texSkip++;
        return -1;
    }

    /* The image reaches VRAM by DMA, which the queued geometry does not wait
     * for, so the batch is closed here and the slot itself stays pinned for
     * the rest of the frame (see the eviction below). */
    if (wasOpen) {
        batchEnd(d);
    }

    if (d->texCacheCount < d->texSlots) {
        i = d->texCacheCount++;
        glGenTextures(1, &id);
        /* A negative name means libnds found no room left in texture RAM. The
         * upload would then land wherever the last bound slot points, which
         * paints one texture's texels through another's coordinates. */
        if (id < 0) {
            d->texNoVram++;
            d->texCacheCount--;
            if (wasOpen) {
                batchBegin(d);
            }
            return -1;
        }
    } else {
        int best = texEvictScan(d);

        /* Nothing to recycle means every slot was touched since the engine last
         * went idle. Waiting for that idle is the only safe way past a full
         * cache: the geometry that sampled the oldest slot has by then been
         * rasterised, so refilling it changes nothing. */
        if (best < 0 && !d->drainGaveUp && geGpuDrain(d)) {
            texUnpinAll(d);
            best = texEvictScan(d);
        }
        if (best < 0) {
            d->texPinned++;
            if (wasOpen) {
                batchBegin(d);
            }
            return -1;
        }
        i = (u32)best;
        /* Keep the VRAM allocation and overwrite it instead of delete +
         * generate: nothing queued samples this id any more. */
        id = d->texCache[i].id;
        d->boundId = -1;
        d->texEvict++;
    }

    glBindTexture(0, id);

    {
        u32 param = GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD;
        int size = texSizeEnum(side);
        const u16 *up = sTexScratch;

        if (pal) {
            u32 count = (t->siz == 0) ? 16u : 256u;

            /* The DS has one 1 KB texture palette for the whole machine, and an
             * 8bpp image reads entries 32-287 of it: two paletted textures with
             * different palettes cannot both be live in one display list. A real
             * room has thirty seven of them against two palettes, and the writes
             * happen on the CPU while the geometry is still queued, so whichever
             * palette was stored last colours every triangle in the frame. The
             * lookup is therefore baked out here: index and palette become one
             * direct-colour texel, which costs twice the VRAM of an 8bpp image and
             * buys the only correct answer the hardware allows. */
            palConvert(t->tlut, count, sPalScratch, t->tlutBe, ciAlpha);
            d->palBinds++;
        }
        texFill(t, sPalScratch, sTexScratch, base);
        if (mip > 0u) {
            texBox(sTexScratch, base, mip, sMipScratch);
            up = sMipScratch;
            d->mipBinds++;
        }
        /* How grainy the bytes about to be uploaded are, measured the same way
         * the panel is: if a square this smooth cannot account for a screen this
         * rough, the roughness is made after the fetch. Every upload in this
         * build belongs to the room; the boot demo is not drawn once it is up.
         * It measures the square that is actually going up, so a box-filtered MIP
         * level reports its own smoothness and the two numbers stay comparable. */
        {
            u32 x;
            u32 y;

            for (y = 0; y < side; y++) {
                const u16 *row = up + y * side;

                for (x = 0; x < side; x++) {
                    u16 c = row[x];
                    u32 l = texLum(c);

                    if (x) {
                        u32 lp = texLum(row[x - 1u]);
                        u32 dl = (lp > l) ? (lp - l) : (l - lp);

                        d->bakeDl += dl;
                        d->bakePairs++;
                    }
                    if (y) {
                        u32 lp = texLum(row[x - side]);
                        u32 dl = (lp > l) ? (lp - l) : (l - lp);

                        d->bakeDl += dl;
                        d->bakePairs++;
                    }
                    d->bakePix++;
                }
            }
        }
        glTexImage2D(0, 0, GL_RGBA, size, size, 0, param, up);
    }

    d->texCache[i].img = t->img;
    d->texCache[i].tlut = pal ? t->tlut : NULL;
    d->texCache[i].w = t->w;
    d->texCache[i].h = t->h;
    d->texCache[i].tlutCount = pal ? t->tlutCount : 0u;
    d->texCache[i].ciAlpha = pal ? ciAlpha : 0u;
    d->texCache[i].opaque = t->opaque;
    d->texCache[i].mip = mip;
    d->texCache[i].stamp = ++d->texClock;
    d->texCache[i].frame = d->frameStamp;
    d->texCache[i].id = id;
    if (d->texBinds == 0u) {
        d->bindW = t->w;
        d->bindFmt = t->fmt;
        d->bindSiz = t->siz;
        d->bindLoad = t->loadWords;
        d->bindPal = t->tlutCount;
        d->bindBe = t->imgBe;
        d->bindPalBe = t->tlutBe;
        d->bindImg = t->img;
        d->bindTlut = t->tlut;
    }
    d->texBinds++;

    if (wasOpen) {
        batchBegin(d);
    }
    return id;
}

static void batchBegin(GeDraw *d)
{
    if (!d->batch) {
        glBegin(GL_TRIANGLES);
        d->batch = true;
    }
}

static void batchEnd(GeDraw *d)
{
    if (d->batch) {
        glEnd();
        d->batch = false;
    }
}

/* Texturing is a single global GE switch (DISP3DCNT, not FIFO-ordered), so
 * it stays on for the whole run: "untextured" geometry samples one texel of
 * an opaque white 8x8 texture, which leaves vertex colours untouched. */
static void bindTex(GeDraw *d, int id)
{
    bool wasOpen = d->batch;

    if (d->boundId == id && !GE_TEX_REBIND_PROBE) {
        return;
    }
    d->texRebinds++;
    if (wasOpen) {
        batchEnd(d);
    }
    glBindTexture(0, id);
    d->boundId = id;
    if (wasOpen) {
        batchBegin(d);
    }
}

/* Which image the GE must sample right now. SHADE ignores the texture even
 * when G_TEXTURE is on, and anything untextured samples the white helper. */
static int liveTexId(GeDraw *d)
{
    if (d->texOn && d->texBound && d->ccMode != F3_CC_SHADE) {
        return d->texId;
    }
    return d->whiteTexId;
}

static void syncTex(GeDraw *d)
{
    int id = liveTexId(d);

    d->texLive = (id == d->texId);
    /* Rebinding the material always puts the image as authored back on the
     * geometry, so the level the last triangle chose no longer describes what
     * is bound. */
    d->texMip = 0u;
    bindTex(d, id);
}

static void onTex(void *ctx, u32 ev, const F3Tex *t)
{
    GeDraw *d = (GeDraw *)ctx;

    /* A tile-1 event describes the second image of a two-image material. It
     * has its own slot and its own cached id; nothing about the first tile's
     * binding moves when it arrives, because the first pass must still be
     * able to draw the other one. */
    if (t->slot == 1u) {
        if (ev == F3_TEX_EV_READY) {
            int id = ensureTex(d, t, 0u);

            d->tex1Bound = (id >= 0);
            if (id >= 0) {
                d->texId1 = id;
                d->texCur1 = *t;
            }
        }
        return;
    }

    if (ev == F3_TEX_EV_READY) {
        int id = ensureTex(d, t, 0u);

        d->texBound = (id >= 0);
        d->texPal = (id >= 0) && (t->fmt == 2);
        if (id >= 0) {
            d->texId = id;
            /* Kept so a triangle drawn further down the list can ask for the
             * same image at a coarser level without the tile state still being
             * where it was. */
            d->texCur = *t;
            d->texMip = 0u;
        }
        syncTex(d);
        return;
    }

    d->texOn = (ev == F3_TEX_EV_ON);
    if (!d->texOn || !d->texBound) {
        d->texPal = false;
    }
    syncTex(d);
}

/* The combiner decides what the bound texture contributes, so a G_SETCOMBINE
 * can change which image the GE has to sample. */
static void onCC(void *ctx, const F3Combiner *cc)
{
    GeDraw *d = (GeDraw *)ctx;

    d->ccMode = cc->mode;
    d->ccAlpha = cc->alpha;
    d->ccEnv = cc->envValid ? cc->env : 0u;
    d->ccEnvValid = cc->envValid;
    d->ccChanges++;
    syncTex(d);
}

/* G_LIGHTING decides what the vertex's cn bytes mean. With it clear the RSP
 * hands them to the RDP as SHADE, so a (TEXEL0 - 0) * SHADE + 0 material is
 * texel times those bytes, which is exactly what the GE's own stage computes.
 * With it set they are a diffuse material the RSP would still have to light
 * against the current lights, and this backend has no lighting stage: the
 * surface gets the texture as authored, and the triangles are counted so the
 * approximation is visible instead of implied. */
static void onGeom(void *ctx, u32 geometryMode)
{
    GeDraw *d = (GeDraw *)ctx;

    d->geomMode = geometryMode;
}

/* OtherMode carries no GE state of its own here: it only decides whether the
 * next triangles blend, which the per-triangle polygon alpha expresses. */
static void onOm(void *ctx, const F3Om *om, u32 flags, u32 acMode)
{
    GeDraw *d = (GeDraw *)ctx;

    (void)om;
    d->omFlags = flags;
    d->omAc = acMode;
    d->omChanges++;
}

static u8 vpClamp(s32 v, s32 max)
{
    if (v < 0) {
        return 0;
    }
    if (v > max) {
        return (u8)max;
    }
    return (u8)v;
}

/* Vp_t is 2.2 fixed over the N64's 320x240 screen: divide by 4 to get N64
 * pixels, then scale by 4/5 onto the 256x192 DS panel.
 *
 * The two engines also disagree about how a viewport rectangle is written down.
 * Measured in melonDS with three copies of one list, each with its own viewport
 * and its own colour: GFX_VIEWPORT holds two corners, not an offset and a size,
 * and its y counts up from the bottom of the panel (y=0 is DS row 191). A
 * rectangle whose low edge is above its high edge submits its triangles and
 * paints none of them, which is what hid the game's list behind a viewport
 * passed as offset-plus-size: the 24-row band at the top of the panel became
 * 167..24, and the two bands that survived in that run were 39 rows tall
 * because their |y2-y1| happened to match. Without the flip every material in
 * the game draws mirrored into the other half of the panel.
 *
 * The band is [y1,y2) in DS rows from the top, so its corners from the bottom
 * are 192-y2 (low) and 191-y1 (high). */
static void onVp(void *ctx, const F3Vp *vp)
{
    GeDraw *d = (GeDraw *)ctx;
    s32 x1 = ((s32)vp->trans[0] - vp->scale[0]) >> 2;
    s32 y1 = ((s32)vp->trans[1] - vp->scale[1]) >> 2;
    s32 x2 = ((s32)vp->trans[0] + vp->scale[0]) >> 2;
    s32 y2 = ((s32)vp->trans[1] + vp->scale[1]) >> 2;

    x1 = x1 * 4 / 5;
    x2 = x2 * 4 / 5;
    y1 = y1 * 4 / 5;
    y2 = y2 * 4 / 5;

    /* A direct register write, not a FIFO command: close the primitive around
     * it so the next vertex is transformed with the new rectangle. */
    batchEnd(d);
    d->vpX1 = vpClamp(x1, 255);
    d->vpX2 = vpClamp(x2 - 1, 255);
    d->vpY1 = vpClamp(192 - y2, 191);
    d->vpY2 = vpClamp(191 - y1, 191);
    glViewport(d->vpX1, d->vpY1, d->vpX2, d->vpY2);
    batchBegin(d);

    d->vpLoads++;
}

/* One vertex in the geometry engine's own units, after the modelview this file
 * applies itself. Positions are 1.19.12, the same format glVertex takes; texel
 * coordinates stay in the N64's 10.5 and colours in 0..255 because the near cut
 * has to interpolate them along the edge before either conversion. */
typedef struct {
    s32 x, y, z;
    s32 s, t;
    s32 r, g, b, a;
} GeoV;

static void eyeOf(const GeDraw *d, const F3Vtx *v, GeoV *out)
{
    s32 p[3];
    s32 sh = 12 - (s32)d->shift;

    geEyeOf(&d->mtx, (s32)v->x << sh, (s32)v->y << sh, (s32)v->z << sh, p);
    out->x = p[0];
    out->y = p[1];
    out->z = p[2];
    out->s = v->s;
    out->t = v->t;
    out->r = v->r;
    out->g = v->g;
    out->b = v->b;
    out->a = v->a;
}

static void putEye(GeDraw *d, const GeoV *v)
{
    s32 x = v->x;
    s32 y = v->y;
    s32 z = v->z;

    if (x > 32767 || x < -32768 || y > 32767 || y < -32768 ||
        z > 32767 || z < -32768) {
        d->clamped++;
        if (x > 32767) x = 32767;
        if (x < -32768) x = -32768;
        if (y > 32767) y = 32767;
        if (y < -32768) y = -32768;
        if (z > 32767) z = 32767;
        if (z < -32768) z = -32768;
    }

    if (d->texLive) {
        /* N64 s/t are 10.5; the DS takes 12.4, so one texel goes 32 -> 16. A
         * MIP level has half the texels of the one above it, so the same
         * fraction of the image has to be the same number of units there too
         * and the shift grows by one per level. */
        u32 as = (u32)(v->s < 0 ? -v->s : v->s);
        u32 at = (u32)(v->t < 0 ? -v->t : v->t);

        if (as > d->texMaxS) {
            d->texMaxS = as;
        }
        if (at > d->texMaxT) {
            d->texMaxT = at;
        }
        glTexCoord2t16((t16)(v->s >> (GE_TEX_SHIFT + d->texMip)),
                       (t16)(v->t >> (GE_TEX_SHIFT + d->texMip)));
    } else {
        /* Centre of texel (0,0) in the white texture: 0.5 texel = 8 in 12.4. */
        glTexCoord2t16(8, 8);
    }
    /* The RDP's output is (TEXEL0 - 0) * SHADE + 0 for the opaque room
     * materials, and the GE's own stage is texel * vertex colour, so with
     * G_LIGHTING clear the vertex's cn bytes belong here: the RSP passes them
     * through as SHADE, and this room's own list clears G_LIGHTING before every
     * textured entry. Measured on Bmori1_room_0: 979 vertices loaded, mean cn
     * (96, 81, 59), mean luminance 82,7 of 255 (32 %), no vertex with blue
     * above red — the "warmer than white" look of the room is theirs to give.
     *
     * DECAL replaces the shade with the texel, so it stays white. So does a
     * combiner this stage does not model. So does a lit surface: with
     * G_LIGHTING set the RSP would compute SHADE from the normals and the
     * lights, which is not implemented here, so the colour is unknown rather
     * than the cn bytes. */
    if (GE_VTX_SHADE &&
        (!d->texLive || (d->ccMode == F3_CC_MODULATE &&
                         !(d->geomMode & F3_GEOM_LIGHTING)))) {
        glColor3b((u8)v->r, (u8)v->g, (u8)v->b);
    } else {
        glColor3b(255, 255, 255);
    }
    glVertex3v16((v16)x, (v16)y, (v16)z);
}

/* Cut a triangle against the near plane. After the modelview that plane is just
 * z = nearZ, and everything the camera can see is at or below it. One plane
 * leaves three vertices when one corner is behind and four when two are, so the
 * result is a triangle or a two-triangle fan. A corner going out emits its
 * crossing on the way, which is what makes the two-behind case close up. */
static u32 clipNear(const GeoV *in, GeoV *out, s32 plane)
{
    const GeoV *pa = &in[2];
    bool paIn = pa->z <= plane;
    u32 n = 0;
    u32 i;

    for (i = 0; i < 3; i++) {
        const GeoV *pb = &in[i];
        bool pbIn = pb->z <= plane;

        if (paIn != pbIn) {
            /* fraction along pa->pb, in 16.16, where the edge meets the plane */
            s64 f = (((s64)plane - pa->z) << 16) / ((s64)pb->z - pa->z);
            GeoV c;

#define GEOL(fld) (c.fld = pa->fld + (s32)(((s64)(pb->fld - pa->fld) * f) >> 16))
            GEOL(x);
            GEOL(y);
            GEOL(s);
            GEOL(t);
            GEOL(r);
            GEOL(g);
            GEOL(b);
            GEOL(a);
#undef GEOL
            c.z = plane;
            out[n++] = c;
        }
        if (pbIn) {
            out[n++] = *pb;
        }
        pa = pb;
        paIn = pbIn;
    }
    return n;
}

static u32 mipLimit(u32 side)
{
    u32 lvl = 0u;

    while (lvl < GE_TEX_MIP_MAX && (side >> (lvl + 1u)) >= 8u) {
        lvl++;
    }
    return lvl;
}

/* Which MIP level a triangle needs: how many texels its texture covers against
 * how many pixels of panel it covers. The geometry engine samples exactly one
 * texel per pixel with no filter, so a face that spans more texels than pixels
 * shows the texels it skipped as noise, and the N64 hid that behind a chain of
 * its own. The ratio is read as a level and never as a fraction, so there is no
 * interpolation to get wrong.
 *
 * Both sides of the comparison are AREAS, which is what makes the test answer
 * to the picture: the triangle's own projected area, from the same perspective
 * transform the engine is about to apply to those three corners, against the
 * area of the patch of texture they map. An earlier version measured edges
 * instead, with one mean depth per triangle, and printed two routes to one
 * ratio seven times apart on the same edge - which is what a mean depth is
 * worth when the third corner is somewhere else in depth entirely.
 *
 * Cost is one 32-bit divide per corner, which is the cheap kind on an ARM9 that
 * has no divider at all, against the six 64-bit divides that took a 30 fps room
 * down to 12. */
/* 65536 / w for w >= 4, without the divide. The ARM9 has no divider, so
 * __aeabi_uidiv is a 32-step bit loop and this selector was calling it three
 * times per face: measured, those three divides were 8,4 ms of a 19,6 ms cost.
 * w is split as 2^e * (1 + u) and the table holds 65536 / (1 + u) at eight
 * mantissa bits, so the reciprocal is a lookup and a shift. Error is under
 * 0,2 %, against a level test whose steps are a factor of four in area. */
static const u32 recipTab[256] = {
    65536u, 65281u, 65028u, 64777u, 64528u, 64281u, 64035u, 63792u,
    63550u, 63310u, 63072u, 62836u, 62602u, 62369u, 62138u, 61909u,
    61681u, 61455u, 61231u, 61008u, 60787u, 60568u, 60350u, 60133u,
    59919u, 59705u, 59494u, 59283u, 59075u, 58867u, 58662u, 58457u,
    58254u, 58053u, 57852u, 57654u, 57456u, 57260u, 57065u, 56872u,
    56680u, 56489u, 56299u, 56111u, 55924u, 55738u, 55554u, 55370u,
    55188u, 55007u, 54828u, 54649u, 54471u, 54295u, 54120u, 53946u,
    53773u, 53601u, 53431u, 53261u, 53092u, 52925u, 52759u, 52593u,
    52429u, 52265u, 52103u, 51942u, 51782u, 51622u, 51464u, 51306u,
    51150u, 50995u, 50840u, 50686u, 50534u, 50382u, 50231u, 50081u,
    49932u, 49784u, 49637u, 49490u, 49345u, 49200u, 49056u, 48913u,
    48771u, 48630u, 48489u, 48349u, 48210u, 48072u, 47935u, 47798u,
    47663u, 47528u, 47393u, 47260u, 47127u, 46995u, 46864u, 46733u,
    46603u, 46474u, 46346u, 46218u, 46091u, 45965u, 45839u, 45714u,
    45590u, 45467u, 45344u, 45222u, 45100u, 44979u, 44859u, 44739u,
    44620u, 44502u, 44384u, 44267u, 44151u, 44035u, 43919u, 43805u,
    43691u, 43577u, 43464u, 43352u, 43240u, 43129u, 43019u, 42908u,
    42799u, 42690u, 42582u, 42474u, 42367u, 42260u, 42154u, 42048u,
    41943u, 41838u, 41734u, 41631u, 41528u, 41425u, 41323u, 41222u,
    41121u, 41020u, 40920u, 40820u, 40721u, 40623u, 40525u, 40427u,
    40330u, 40233u, 40137u, 40041u, 39946u, 39851u, 39756u, 39662u,
    39569u, 39476u, 39383u, 39291u, 39199u, 39108u, 39017u, 38926u,
    38836u, 38746u, 38657u, 38568u, 38480u, 38392u, 38304u, 38217u,
    38130u, 38044u, 37958u, 37872u, 37787u, 37702u, 37617u, 37533u,
    37449u, 37366u, 37283u, 37200u, 37118u, 37036u, 36954u, 36873u,
    36792u, 36712u, 36631u, 36552u, 36472u, 36393u, 36314u, 36236u,
    36158u, 36080u, 36003u, 35926u, 35849u, 35772u, 35696u, 35620u,
    35545u, 35470u, 35395u, 35320u, 35246u, 35172u, 35099u, 35026u,
    34953u, 34880u, 34808u, 34735u, 34664u, 34592u, 34521u, 34450u,
    34380u, 34309u, 34239u, 34169u, 34100u, 34031u, 33962u, 33893u,
    33825u, 33757u, 33689u, 33622u, 33554u, 33487u, 33421u, 33354u,
    33288u, 33222u, 33157u, 33091u, 33026u, 32961u, 32897u, 32832u,
};

static u32 recip65536(u32 w)
{
    u32 e = 31u - (u32)__builtin_clz(w);
    u32 j = ((w << (31u - e)) >> 23) - 256u;

    return recipTab[j] >> e;
}

static u32 mipOfTri(GeDraw *d, const GeoV *a, const GeoV *b, const GeoV *c,
                    u32 max, int eye, int record)
{
    const GeoV *v[3];
    s32 px[3];
    s32 py[3];
    s32 w[3];
    u64 texA;
    u64 pxA;
    u32 lvl = 0u;
    u32 bs = GE_TEX_LOD_BIAS >> 6;
    u32 i;

    v[0] = a;
    v[1] = b;
    v[2] = c;

    /* Screen position in sixteenths of a panel pixel, for a projection whose w
     * is the eye-space depth and nothing else. The corners are v16 (4.12), the
     * matrix is 1.19.12, and the engine forms the clip coordinate as
     * (m * x) >> 12, so
     *
     *   ndc        = ((m * x) >> 12) / w          with w = -z exactly,
     *   sixteenths = 8 * vp * ndc = vp * m * x / (512 * w).
     *
     * Each of m, x and w is shifted down 6 bits before it is multiplied, which
     * costs under 2 % of the ratio; the reciprocal of the depth is one 32-bit
     * divide in 16.16 and the rest is multiplies. The three preshifts take out
     * 64^3 and the reciprocal puts 2^16 back, so cancelling the 512 and the
     * remaining 64^2 needs a shift of 28. It said 16 for a whole phase, which
     * made every projected position 4096 times its true size: the clamp below
     * then flattened most of a room's corners onto the same wall and the
     * shoelace of three equal points is zero. */
    {
        const int *pm = d->proj.m;
        u32 vpw = (u32)((s32)d->vpX2 - (s32)d->vpX1 + 1);
        u32 vph = (u32)((s32)d->vpY2 - (s32)d->vpY1 + 1);
        s32 m0 = pm[0] >> 6;
        s32 m5 = pm[5] >> 6;

        if (m0 <= 0 || m5 <= 0) {
#ifdef GE_LOD_PROBE
            if (record) {
                d->lodGm++;
            }
#endif
            return 0u;
        }
        for (i = 0; i < 3u; i++) {
            /* A corner at or behind the eye has no area worth measuring, and one
             * within a few units of it projects past the panel anyway. Level 0 is
             * the answer that costs nothing and is never wrong about detail that
             * is not on screen. */
            w[i] = -v[i]->z >> 6;
            if (w[i] < 4) {
#ifdef GE_LOD_PROBE
                if (record) {
                    d->lodGw++;
                }
#endif
                return 0u;
            }
        }
        if (!eye || record) {
            for (i = 0; i < 3u; i++) {
                s32 inv = (s32)recip65536((u32)w[i]);

                s32 lim = GE_MIP_AREA32 ? GE_MIP_CLAMP : 1048575;

                px[i] = (s32)(((s64)512 * vpw * m0 * inv * (v[i]->x >> 6)) >> 28);
                py[i] = (s32)(((s64)512 * vph * m5 * inv * (v[i]->y >> 6)) >> 28);
                if (px[i] > lim || px[i] < -lim || py[i] > lim || py[i] < -lim) {
#ifdef GE_LOD_PROBE
#if GE_MIP_AREA32
                    if (record) {
                        d->lodClamp++;
                    }
#endif
#endif
                    if (px[i] > lim) px[i] = lim;
                    if (px[i] < -lim) px[i] = -lim;
                    if (py[i] > lim) py[i] = lim;
                    if (py[i] < -lim) py[i] = -lim;
                }
            }
        }

        /* Twice the area of the patch of texture, by the shoelace over two of
         * its edges. The factor of two cancels in the ratio, and so does the
         * sign. */
        {
            s64 t = (s64)(v[1]->s - v[0]->s) * (v[2]->t - v[0]->t)
                  - (s64)(v[2]->s - v[0]->s) * (v[1]->t - v[0]->t);

            if (t < 0) {
                t = -t;
            }
            texA = (u64)t;
        }

        if (eye) {
            /* Twice the projected area from the eye-space determinant. With
             * u = Sx x / w and v = Sy y / w the shoelace over three corners
             * multiplies out to
             *
             *   2A = Sx Sy * det[x y w] / (w0 w1 w2),
             *
             * so the per-corner projections and their clamps disappear, at the
             * price of three products for the determinant and one division.
             * Sx Sy is vpw m0 vph m5 / 2^18; the x and y preshifts below divide
             * the determinant by 2^12, which the << 12 after the divide puts
             * back. */
            s64 x0 = v[0]->x >> 6, y0 = v[0]->y >> 6;
            s64 x1 = v[1]->x >> 6, y1 = v[1]->y >> 6;
            s64 x2 = v[2]->x >> 6, y2 = v[2]->y >> 6;
            s64 det = x0 * ((s64)y1 * w[2] - (s64)y2 * w[1])
                    - y0 * ((s64)x1 * w[2] - (s64)x2 * w[1])
                    + (s64)w[0] * ((s64)x1 * y2 - (s64)x2 * y1);
            u64 adet = (u64)(det < 0 ? -det : det);
            u64 sxy = (((u64)vpw * (u32)m0) * ((u64)vph * (u32)m5)) >> 18;
            u64 n = sxy * adet;
            u64 wprod = (u64)w[0] * (u64)w[1] * (u64)w[2];
            u64 q;

            if (eye == 1) {
                /* The << 12 has to happen before the divide to keep the small
                 * areas, where the level boundary is, so it only goes first
                 * while it cannot overflow. */
                if (n <= 0x000FFFFFFFFFFFFFull) {
                    q = (n << 12) / wprod;
                } else {
                    q = (n / wprod) << 12;
                }
            } else {
                /* One reciprocal of the mean depth, used three times. n is
                 * shifted up first so the three >> 16 steps cannot round a
                 * small area to zero: with n at 48 bits every intermediate
                 * keeps 2^46 or more, and the << 12 lands in the end. */
                u32 wm = (u32)(w[0] + w[1] + w[2] + 1) / 3u;
                u32 inv = recip65536(wm);
                u32 e = 63u - (u32)__builtin_clzll(n | 1u);

                if (e > 48u) {
                    /* A face this big is off the panel by orders of magnitude;
                     * the ceiling is the only answer the level test needs. */
                    q = 0x7FFFFFFFull;
                } else {
                    u32 s = 48u - e;
                    u64 t = n << s;

                    t = (t * inv) >> 16;
                    t = (t * inv) >> 16;
                    t = (t * inv) >> 16;
                    q = s >= 12u ? (t >> (s - 12u)) : (t << (12u - s));
                }
            }
            pxA = q > 0x7FFFFFFFull ? 0x7FFFFFFFull : q;
        } else {
            /* Twice the projected area, by the shoelace over the same two
             * edges of the projected corners. */
#if GE_MIP_AREA32
            /* With the corners held at +-12000 sixteenths every difference is
             * at most 24000, so each product fits 30 bits and their difference
             * 31: the four 64-bit products become four 32-bit ones. */
            s32 p = (px[1] - px[0]) * (py[2] - py[0])
                  - (px[2] - px[0]) * (py[1] - py[0]);

            if (p < 0) {
                p = -p;
            }
            pxA = (u64)(u32)p;
#else
            s64 p = (s64)(px[1] - px[0]) * (py[2] - py[0])
                  - (s64)(px[2] - px[0]) * (py[1] - py[0]);

            if (p < 0) {
                p = -p;
            }
            pxA = (u64)p;
#endif
        }
    }

    if (pxA == 0u || texA == 0u) {
#ifdef GE_LOD_PROBE
        if (record) {
            if (pxA == 0u) {
                d->lodGp++;
            } else {
                d->lodGt++;
            }
        }
#endif
        return 0u;
    }

    /* One texel is 32 units of s or t and one panel pixel is 16 sixteenths, so
     * the squared ratio is texA / (4 * pxA) and asking for level lvl + 1 is
     * asking for that to reach 4 ^ lvl * (2048 / bias) ^ 2. Cleared of fractions
     * and of the 64-bit squares of the bias, that is the line below - with
     * nothing divided, and with 2048 meaning "a level as soon as a face walks
     * one texel per pixel" and 1024 meaning the RDP's own "wait for two". */
    while (lvl < max && texA * bs * bs >= (pxA << (2 * lvl + 12u))) {
        lvl++;
    }

#ifdef GE_LOD_PROBE
    /* The worst face in the frame, with both of its areas, so the level above
     * can be redone on paper. lodR16 is 4 * texA / pxA, which is 16 * R ^ 2:
     * 16 is one texel per pixel, 64 is two, 256 is four. */
    if (record) {
        u32 r16 = (u32)((texA * 4u) / pxA);

        /* The first face that gets this far is recorded even if its ratio is
         * zero, because a frame in which every face is zero tells the same story
         * as a frame in which nothing was measured at all unless the areas of
         * one real face are on the screen. */
        if (r16 > d->lodRworst || d->lodPxA == 0u) {
            d->lodRworst = r16;
            d->lodTexA = (u32)(texA > 0xffffffffu ? 0xffffffffu : texA);
            d->lodPxA = (u32)(pxA > 0xffffffffu ? 0xffffffffu : pxA);
            d->lodDs = (u32)(v[1]->s - v[0]->s);
            d->lodDx = (u32)(v[2]->s - v[0]->s);
            d->lodZ = (u32)(-v[0]->z >> 6);
            d->lodK = (u32)(px[1] - px[0]);
        }
    }
#endif
    return lvl;
}

/* Second pass of the two-image lerp, over one triangle of the fan that the
 * first pass has just laid down in T0.
 *
 * The GE reads the frame buffer back through the blender, so T1 * ENVA +
 * dst * (1 - ENVA) with T0 already in dst is (T1 - T0) * ENVA + T0 — the RDP's
 * own arithmetic, in a second primitive instead of in the combiner. The blend
 * factor is the polygon alpha, which is the same for every pixel of the
 * primitive because ENVA is a register, not a vertex value.
 *
 * The level is the one the first pass used: the two images have to be sampled
 * at the same level or the composite is one image sharp over the other blurred.
 * Both sides go through the same area test, so when the two descriptors agree
 * on size -- which is what the whole-game census found for 83 % of the
 * triangles of this family -- they also agree on the level. */
static void lerpPass(GeDraw *d, const GeoV *a, const GeoV *b, const GeoV *c,
                     u32 lvl)
{
    int id = ensureTex(d, &d->texCur1, lvl);
    u32 fmt;

    if (id < 0) {
        d->lerpFail++;
        return;
    }

    /* The image, the level and the polygon attributes all change, and the GE
     * takes the attributes when a batch begins, so the batch is closed around
     * the whole thing. */
    batchEnd(d);

    if (GE_LERP == 1) {
        /* Probe: the second image alone, opaque. Says whether it is the image
         * the room means and whether it lands where the geometry is. */
        fmt = POLY_ALPHA(31) | POLY_CULL_NONE;
    } else {
        d->xluTris++;
        fmt = POLY_ALPHA(d->lerpEnv) | POLY_CULL_NONE |
              POLY_ID((d->xluTris % 63u) + 1u);
    }
    d->texRebinds++;
    glBindTexture(0, id);
    d->boundId = id;
    glPolyFmt(fmt);
    d->polyFmt = fmt;   /* so the next triangle re-issues its own */

    d->texMip = lvl;
    d->texLive = true;
    batchBegin(d);
    putEye(d, a);
    putEye(d, b);
    putEye(d, c);
    batchEnd(d);

    d->lerpTris++;

    /* Put the first tile back on the engine. syncTex re-binds level 0 of the
     * material's own image and clears texMip, so the next triangle's level
     * test starts from a known state instead of inheriting the second tile's
     * image at the second tile's level. */
    syncTex(d);
}

static void putFan(GeDraw *d, const GeoV *v, u32 n)
{
    u32 i;
    u32 max = 0u;

    if (d->texLive) {
        const int *p = d->proj.m;

        /* A projection that does not shrink with depth has no business asking
         * for a MIP, and the deepest level this material can be drawn from is
         * the one that still leaves the hardware a legal texture size. */
        if (p[0] > 0 && p[5] > 0) {
            max = mipLimit(texSide(&d->texCur));
        }
    }

    for (i = 1; i + 1 < n; i++) {
        u32 lvl = d->texMip;

        if (d->texLive && max != 0u) {
            lvl = mipOfTri(d, &v[0], &v[i], &v[i + 1u], max, GE_MIP_EYE, 1);

#if GE_MIP_EYE_PROBE && defined(GE_LOD_PROBE)
            /* Face by face: does the formula that would replace the shoelace
             * put this triangle on the same level? The answer is drawn with
             * the shipped selector either way, so a build of this probe looks
             * exactly like the build it is measuring. */
            {
                u32 alt = mipOfTri(d, &v[0], &v[i], &v[i + 1u], max,
                                   !GE_MIP_EYE, 0);
                u32 dlt = alt > lvl ? alt - lvl : lvl - alt;

                if (dlt != 0u) {
                    d->lodEyeDiv++;
                    if (dlt > d->lodEyeMax) {
                        d->lodEyeMax = dlt;
                    }
                }
            }
#endif

#if GE_TEX_MIP_FORCE
            /* Not a shipping path: it draws every surface that has a chain at
             * one fixed level, whatever the ratio says, so the picture can be
             * asked directly whether blurring this view helps or only smears
             * it. The level test still runs and still reports. */
            lvl = GE_TEX_MIP_FORCE > max ? max : GE_TEX_MIP_FORCE;
#endif

#if GE_TEX_MIP_NULL
            /* Keep the selector observable so the compiler cannot delete it,
             * then draw from level 0 exactly as if the chain were off. */
            if (lvl > d->mipMax) {
                d->mipMax = lvl;
            }
            lvl = 0u;
#endif

            if (lvl != d->texMip) {
                int id = ensureTex(d, &d->texCur, lvl);

                /* Nothing to put it in is not a reason to draw the triangle
                 * wrong: stay on the level already bound. */
                if (id >= 0) {
                    d->texMip = lvl;
                    bindTex(d, id);
                } else {
                    d->mipFail++;
                    lvl = d->texMip;
                }
            }
            if (d->texMip > 0u) {
                d->mipTris++;
                if (d->texMip > d->mipMax) {
                    d->mipMax = d->texMip;
                }
            }
#ifdef GE_LOD_PROBE
            {
                u32 lvlSeen = d->texMip;

                if (lvlSeen > 3u) {
                    lvlSeen = 3u;
                }
                d->lodN[lvlSeen]++;
            }
#endif
        }
#if GE_TEX_REBIND_PROBE
        /* One swap per triangle, whatever the material says: the price of a
         * bind is what this build exists to measure. The id is the one already
         * bound, not the material's base image — rebinding the base would drop
         * a triangle that had chosen a MIP level back to level 0 and the panel
         * would change for a reason that has nothing to do with the swap. */
        if (d->boundId >= 0) {
            bindTex(d, d->boundId);
        }
#endif
        putEye(d, &v[0]);
        putEye(d, &v[i]);
        putEye(d, &v[i + 1]);
        d->tris++;

        if (d->lerpDraw) {
            lerpPass(d, &v[0], &v[i], &v[i + 1u], lvl);
        }
    }
}

/* The near plane for a projection already converted by geMtxToDs, and the cut
 * itself, both exposed because the only honest way to say the near plane is
 * where the game's projection puts it is to build that projection and read the
 * number back. geClipNearZ returns how many vertices the cut left. */
bool geProjNearZ(const m4x4 *p, s32 *out)
{
    return projNearZ(p, out);
}

u32 geClipNearZ(const s32 in[3][3], s32 plane, s32 out[4][3])
{
    GeoV v[3];
    GeoV c[4];
    u32 n;
    u32 i;

    for (i = 0; i < 3; i++) {
        memset(&v[i], 0, sizeof(v[i]));
        v[i].x = in[i][0];
        v[i].y = in[i][1];
        v[i].z = in[i][2];
    }
    n = clipNear(v, c, plane);
    for (i = 0; i < n; i++) {
        out[i][0] = c[i].x;
        out[i][1] = c[i].y;
        out[i][2] = c[i].z;
    }
    return n;
}


/* The polygon format every non-blended material wants: fully opaque, no ID.
 * topInit writes the same word before the first frame. */
#define GE_FMT_OPAQUE (POLY_ALPHA(31) | POLY_CULL_NONE)

static void onTri(void *ctx, const F3Vtx *vtx, u32 a, u32 b, u32 c)
{
    GeDraw *d = (GeDraw *)ctx;
    GeoV tri[4];
    u32 alpha = 31u;
    u32 id = 0u;
    u32 fmt;
    if (d->omFlags & F3_OM_XLU) {
        if (d->ccAlpha == F3_AC_SHADE) {
            /* The shade alpha is the blend factor OoT's translucent materials
             * ask for; 8 bits to the GE's 5. */
            alpha = (u32)vtx[a].a >> 3;
        }
        /* The GE drops a translucent pixel when its polygon ID matches the ID
         * already stored at that point, so two blended triangles that overlap
         * need different IDs. 1..63 because 0 is the backdrop's own. */
        d->xluTris++;
        id = (d->xluTris % 63u) + 1u;
        d->xluAlpha = alpha;
    }
    fmt = POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id);
    if (fmt != d->polyFmt) {
        /* The GE takes the polygon attributes when a batch begins, not when a
         * triangle is submitted, so a format change has to close the open batch,
         * write the word and start a new one; leaving the batch closed would
         * drop every vertex that follows. */
        batchEnd(d);
        glPolyFmt(fmt);
        batchBegin(d);
        d->polyFmt = fmt;
        d->polyChanges++;
    }

    if (d->texLive) {
        d->texTris++;
        if (d->texPal) {
            d->palTris++;
        }
        if (d->ccAlpha == F3_AC_TEXEL0) {
            d->cutTris++;
        }
        if (d->ccMode == F3_CC_MODULATE) {
            if (d->geomMode & F3_GEOM_LIGHTING) {
                d->litTris++;
            } else {
                d->shadeTris++;
            }
        } else if (d->ccMode == F3_CC_OTHER) {
            d->otherTris++;
        }
    }

    /* A two-image material gets its second pass only when the list described
     * both tiles; otherwise it keeps the first image alone, which is exactly
     * what the build before this phase drew, so a missing tile is a visible
     * no-op rather than a wrong picture. */
    d->lerpDraw = (GE_LERP != 0) && d->ccMode == F3_CC_LERP &&
                  d->texLive && d->tex1Bound;
    if (d->lerpDraw) {
        u32 env = d->ccEnvValid ? d->ccEnv : GE_LERP_ENV;

        if (!d->ccEnvValid) {
            d->lerpEnvDefault++;
        }
        /* The RDP takes ENVA/255; the GE takes POLY_ALPHA/31. Rounding to the
         * nearest of the 32 levels instead of shifting keeps the error at half
         * a level (1.6 %) instead of up to one whole level at the low end.
         * Measured on the Bmori1 floor: fitted weight 0.527 with ENVA = 128,
         * which is 16/31 = 0.516, against the RDP's exact 0.5. */
        env &= 0xFFu;
        d->lerpEnv = (env * 31u + 127u) / 255u;
        if (d->lerpEnv > 31u) {
            d->lerpEnv = 31u;
        }
    }

    eyeOf(d, &vtx[a], &tri[0]);
    eyeOf(d, &vtx[b], &tri[1]);
    eyeOf(d, &vtx[c], &tri[2]);

    if (d->hasNear && (tri[0].z > d->nearZ || tri[1].z > d->nearZ ||
                       tri[2].z > d->nearZ)) {
        GeoV cut[4];
        u32 n = clipNear(tri, cut, d->nearZ);

        d->clipTris++;
        if (n < 3) {
            d->clipDrop++;
            return;
        }
        putFan(d, cut, n);
        return;
    }
    putFan(d, tri, 3);
}

void geDrawInit(GeDraw *d, u32 shift)
{
    static const u16 white[8 * 8] = {
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    };
    int id = -1;

    memset(d, 0, sizeof(*d));
    d->shift = shift;
    d->texSlots = GE_TEX_CACHE;
    d->texId = -1;
    d->texId1 = -1;
    d->boundId = -1;

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &id);
    glBindTexture(0, id);
    glTexImage2D(0, 0, GL_RGBA, TEXTURE_SIZE_8, TEXTURE_SIZE_8, 0,
                 GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T | TEXGEN_TEXCOORD, white);
    d->whiteTexId = id;
    d->boundId = id;
}

void geDrawList(GeDraw *d, F3State *st, const u8 *dl)
{
    /* Kept across frames on purpose: a slot carries the stamp of the frame
     * that filled it, and only a frame of the past may be refilled. */
    d->frameStamp++;
    d->tris = 0;
    d->clamped = 0;
    d->mtxLoads = 0;
    d->projLoads = 0;
    d->mtxPops = 0;
    d->mtxLeft = 0;
    d->mtxDepth = 0;
    d->texBinds = 0;
    d->texRebinds = 0;
    d->texHits = 0;
    d->texSkip = 0;
    d->mipBinds = 0;
    d->mipTris = 0;
    d->mipMax = 0;
    d->mipFail = 0;
#ifdef GE_LOD_PROBE
    d->lodRworst = 0;
    d->lodTexA = 0;
    d->lodPxA = 0;
    d->lodGm = 0;
    d->lodGw = 0;
    d->lodGp = 0;
    d->lodGt = 0;
    d->lodDs = 0;
    d->lodDx = 0;
    d->lodZ = 0;
    d->lodK = 0;
    d->lodEyeDiv = 0;
    d->lodEyeMax = 0;
    d->lodClamp = 0;
    d->lodN[0] = 0;
    d->lodN[1] = 0;
    d->lodN[2] = 0;
    d->lodN[3] = 0;
#endif
    d->texMip = 0;
    d->texEvict = 0;
    d->texPinned = 0;
    d->texDrain = 0;
    d->texDrainFail = 0;
    d->texDrainUs = 0;
    d->texDrainPoll = 0;
    d->drainGaveUp = false;
    d->texTris = 0;
    d->palBinds = 0;
    /* The bake counters are not per-frame: a room uploads its textures once and
     * every later frame only hits the cache, so resetting them here would leave
     * the readout at zero from the second frame on. */
    d->palTris = 0;
    d->cutTris = 0;
    d->xluTris = 0;
    d->xluAlpha = 31;
    d->omFlags = 0;
    d->omAc = 0;
    d->omChanges = 0;
    d->polyChanges = 0;
    /* GFX_POLY_FORMAT survives between frames. The list ends by restoring the
     * opaque word (alpha 31, ID 0) and topInit wrote that before the very
     * first frame, so this cache starts true without another FIFO write. */
    d->polyFmt = GE_FMT_OPAQUE;
    d->ccMode = F3_CC_MODULATE;
    d->ccAlpha = 0;
    d->ccChanges = 0;
    d->geomMode = 0;
    d->shadeTris = 0;
    d->litTris = 0;
    d->otherTris = 0;
    d->vpLoads = 0;
    d->texOn = false;
    d->texBound = false;
    d->texPal = false;
    d->texLive = false;
    d->batch = false;
    d->clipTris = 0;
    d->clipDrop = 0;
    /* Every list starts from an identity pair. The engine's modelview is
     * whatever the grid left in it, so it has to be told as well as the shadow. */
    identityMtx(&d->mtx);
    identityMtx(&d->proj);
    syncNear(d);

    f3Reset(st);
    f3SetTriHook(st, onTri, d);
    f3SetMtxHook(st, onMtx, d);
    f3SetTexHook(st, onTex, d);
    f3SetCCHook(st, onCC, d);
    f3SetVpHook(st, onVp, d);
    f3SetOmHook(st, onOm, d);
    f3SetGeomHook(st, onGeom, d);

    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    batchBegin(d);
    d->ok = f3Run(st, dl);
    batchEnd(d);

    /* Leave the FIFO in the state the rest of the frame expects: an opaque
     * polygon with no ID, so the grid and the next list start solid. */
    if (d->polyFmt != GE_FMT_OPAQUE) {
        glPolyFmt(GE_FMT_OPAQUE);
        d->polyFmt = GE_FMT_OPAQUE;
    }

    /* A list is expected to balance its G_MTX pushes. If it did not, the GE
     * stacks would drift into the next frame, so unwind and report it. */
    d->mtxLeft = d->mtxDepth;
    popLevels(d, d->mtxDepth);

    f3SetTriHook(st, NULL, NULL);
    f3SetMtxHook(st, NULL, NULL);
    f3SetTexHook(st, NULL, NULL);
    f3SetCCHook(st, NULL, NULL);
    f3SetVpHook(st, NULL, NULL);
    f3SetOmHook(st, NULL, NULL);
    f3SetGeomHook(st, NULL, NULL);

    bindTex(d, d->whiteTexId);
}

/* Enough images to walk past a deliberately small cache; the demo list carries
 * twenty-six that this backend can upload. */
#define GE_PROBE_SLOTS 8u

/* The probe has to start where the first real frame starts. The table is
 * emptied rather than torn down: it happens once at boot, the names it leaves
 * behind are well inside the 2048 libnds hands out and glDeleteTextures would
 * release the parameter slot without giving the VRAM back in a form that the
 * texture cache here could reuse. */
static void texCacheCold(GeDraw *d)
{
    memset(d->texCache, 0, sizeof(d->texCache));
    d->texCacheCount = 0;
    d->boundId = -1;
}

void geDrainProbe(GeDraw *d, F3State *st, const u8 *dl, GeDrainTest *t)
{
    memset(t, 0, sizeof(*t));

    /* Make the overflow happen on purpose and write down what the machine says
     * while the CPU is mid-list with a frame's worth of polygons queued. The
     * cache is small first so the names stay inside what one frame needs. */
    texCacheCold(d);
    d->texSlots = GE_PROBE_SLOTS;
    geDrawList(d, st, dl);
    glFlush(0);
    t->pinsSmall = d->texPinned;
    t->bindsSmall = d->texBinds;
    t->drains = d->texDrain;
    t->fails = d->texDrainFail;
    t->polls = d->texDrainPoll;
    t->us = d->texDrainUs;
    t->stat = d->texDrainStat;
    t->poly = d->texDrainPoly;

    /* And the same list through the cache the build actually ships with. */
    d->texSlots = GE_TEX_CACHE;
    geDrawList(d, st, dl);
    glFlush(0);
    t->bindsBig = d->texBinds;
    t->evictBig = d->texEvict;

    /* Last, the case the wait has to be able to answer: this frame's geometry
     * is queued, the frame is drawn, so the engine must now say it is idle.
     * Without this the overflow branch above could only ever mean "the bits
     * are wrong". */
    swiWaitForVBlank();
    t->idleOk = geGpuDrain(d);
}
