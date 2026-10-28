#include "ultra64.h"

#include "game.h"
#include "gfxalloc.h"
#include "sys_matrix.h"
#include "tha.h"

#include "oot_dl.h"

/* One blob plays the role of a game segment: the display list, its two
 * matrices, its viewport and its vertices all come out of the same Gfx_Alloc
 * cursor, so every address the list carries is segment-relative the way the RSP
 * expects and nothing here had to be hand-assembled. */
#define OOT_DL_BLOB_BYTES 1024u
#define OOT_DL_CMDS       12u

/* Measurement toggles for the geometry engine's own clipper. */
/* #define OOT_PROBE_FAR */
/* #define OOT_PROBE_NEAR */

/* sys_matrix.c keeps its current matrix in a stack of 20 MtxF that it allocates
 * out of the GameState's arena, so the port has to hand Matrix_Init a GameState
 * and an arena big enough for those 1280 bytes plus the head it grows into. */
#define OOT_MTX_ARENA_BYTES 2048u

/* Math_SinS works in binangles, where a quarter turn is 0x4000, so this is 45
 * degrees: enough lean for the seam to cross about a third of a panel row per
 * row it falls, which is a number a screenshot can read, and small enough that
 * the quads stay on screen. It is also the one angle where the game's own
 * table is visibly asymmetric, because sins(0x2000) reads sintable[0x200] and
 * coss(0x2000) folds to sintable[0x1FF]: sine and cosine differ by one entry,
 * and the self test asserts that difference instead of assuming symmetry. */
#define OOT_DL_ROT_Z 0x2000

/* Half-width and half-height, in N64 pixels, of the frustum guPerspective
 * builds at z = -500: tan(30 deg) * 500 * (3/4) and tan(30 deg) * 500. The
 * quads use half of that, because the geometry engine is fed clip-space
 * triangles that straddle its frustum planes and it drops them; the RSP of the
 * N64 clipped every polygon before the RDP ever saw one, and this port has no
 * equivalent stage yet. */
#define OOT_DL_HALF_X 192
#define OOT_DL_HALF_Y 144
#define OOT_DL_Z      (-500)
/* How far the seam between the two quads travels from the centre line. */
#define OOT_DL_TRAVEL 100

static u8 sBlob[OOT_DL_BLOB_BYTES] __attribute__((aligned(16)));

static u8 sMtxArena[OOT_MTX_ARENA_BYTES] __attribute__((aligned(16)));
static GameState sMtxState;
static u32 sMtxStackReady;

/* Matrix_GetCurrent is defined in sys_matrix.c and never declared in its
 * header, because in the game nothing outside that file needs to know where the
 * stack lives. The self test does, so the prototype is repeated here rather
 * than patched into a vendored header. */
MtxF *Matrix_GetCurrent(void);

void ootMtxStackInit(void)
{
    if (sMtxStackReady == 0u) {
        THA_Init(&sMtxState.tha, sMtxArena, sizeof(sMtxArena));
        Matrix_Init(&sMtxState);
        sMtxStackReady = 1u;
    }
}

static u32 segAddr(const void *p)
{
    return ((u32)OOT_DL_SEG << 24) | (u32)((const u8 *)p - sBlob);
}

static void putVtx(Vtx *v, s32 x, s32 y, u8 r, u8 g, u8 b)
{
    v->v.ob[0] = (s16)x;
    v->v.ob[1] = (s16)y;
    v->v.ob[2] = (s16)OOT_DL_Z;
    v->v.flag = 0;
    v->v.tc[0] = 0;
    v->v.tc[1] = 0;
    v->v.cn[0] = r;
    v->v.cn[1] = g;
    v->v.cn[2] = b;
    v->v.cn[3] = 255;
}

void ootDlBuild(OotDl *out, u16 angle)
{
    Gfx *cur = Gfx_Open((Gfx *)sBlob);
    Mtx *proj = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Mtx *mv = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Vp *vp = (Vp *)Gfx_Alloc(&cur, sizeof(Vp));
    Vtx *vtx = (Vtx *)Gfx_Alloc(&cur, 8 * sizeof(Vtx));
    Gfx *list = (Gfx *)Gfx_Alloc(&cur, OOT_DL_CMDS * sizeof(Gfx));
    Gfx *g = list;
    u16 perspNorm = 0;
    Vec3f pos;
    Vec3s rot;
    s32 edge;

    /* The seam between the two quads rides the game's own sine table, so two
     * screenshots of two frames prove the list is rebuilt and not cached. */
    edge = (s32)(OOT_DL_TRAVEL * (s32)sins((s16)angle) / 32767);

    guPerspective(proj, &perspNorm, 60.0f, 4.0f / 3.0f, 200.0f, 1000.0f, 1.0f);

    /* The modelview comes out of the game's own module now, not guMtxIdent:
     * Matrix_Put drops the identity sys_matrix.c defines into the stack's
     * current slot, Matrix_TranslateRotateZYX rewrites it with Math_SinS and
     * Math_CosS, and Matrix_ToMtx does the 16.16 conversion in that file's own
     * words. The Push and Pop around it are the actor idiom, so the 20-slot
     * stack gets exercised the way the game uses it and not the way a test
     * would. */
    ootMtxStackInit();
    Matrix_Put(&gIdentityMtxF);
    Matrix_Push();
    pos.x = (f32)edge;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0;
    rot.y = 0;
    rot.z = OOT_DL_ROT_Z;
    Matrix_TranslateRotateZYX(&pos, &rot);
    Matrix_ToMtx(mv);
    Matrix_Pop();

    /* The top 24 rows of the DS panel. The demo list's own viewport starts at
     * row 24, so this band is the one region a screenshot can attribute to this
     * list alone. Vp_t is 2.2 fixed over the N64's 320x240 screen: trans/scale
     * of 58 puts the band at N64 y 0..29, which the 4/5 mapping lands on DS
     * rows 0..23. */
    vp->vp.vscale[0] = 640;
    vp->vp.vscale[1] = 58;
    vp->vp.vscale[2] = 511;
    vp->vp.vscale[3] = 0;
    vp->vp.vtrans[0] = 640;
    vp->vp.vtrans[1] = 58;
    vp->vp.vtrans[2] = 0;
    vp->vp.vtrans[3] = 0;

    putVtx(&vtx[0], -OOT_DL_HALF_X, -OOT_DL_HALF_Y, 255, 0, 0);
    putVtx(&vtx[1], edge, -OOT_DL_HALF_Y, 255, 0, 0);
    putVtx(&vtx[2], edge, OOT_DL_HALF_Y, 255, 0, 0);
    putVtx(&vtx[3], -OOT_DL_HALF_X, OOT_DL_HALF_Y, 255, 0, 0);
    putVtx(&vtx[4], edge, -OOT_DL_HALF_Y, 0, 255, 0);
    putVtx(&vtx[5], OOT_DL_HALF_X, -OOT_DL_HALF_Y, 0, 255, 0);
    putVtx(&vtx[6], OOT_DL_HALF_X, OOT_DL_HALF_Y, 0, 255, 0);
    putVtx(&vtx[7], edge, OOT_DL_HALF_Y, 0, 255, 0);

    /* Measurement build: two probes, one per quad, each in the triangle that
     * the other three vertices do not share (gSP1Quadrangle with flag 0 is
     * (v0,v1,v2) plus (v0,v2,v3), so vtx[3] and vtx[7] each appear once).
     * vtx[3] goes past the far plane, vtx[7] behind the camera. */
#ifdef OOT_PROBE_FAR
    vtx[3].v.ob[2] = -2000;
#endif
#ifdef OOT_PROBE_NEAR
    /* Far enough behind the camera that its eye-space z, at 16 units to the
     * world unit, is ten times past what the engine's vertex input holds. */
    vtx[7].v.ob[2] = 20000;
#endif

    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gDPSetRenderMode(g++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    /* SHADE, so the vertex colours are what reaches the panel: no texture is
     * bound and G_SETTIMG is never emitted by this list. */
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gSPViewport(g++, segAddr(vp));
    gSPMatrix(g++, segAddr(proj), G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(g++, segAddr(mv), G_MTX_LOAD);
    gSPVertex(g++, segAddr(vtx), 8, 0);
    gSP1Quadrangle(g++, 0, 1, 2, 3, 0);
    gSP1Quadrangle(g++, 4, 5, 6, 7, 0);
    gSPEndDisplayList(g++);

    out->list = list;
    out->blob = sBlob;
    out->vtx = vtx;
    out->proj = proj;
    out->mv = mv;
    out->bytes = (unsigned long)((const u8 *)cur - sBlob);
    out->edge = (long)edge;
    out->rotZ = (long)(u16)rot.z;
    out->stackOff = (unsigned long)((const u8 *)Matrix_GetCurrent() - sMtxArena);
    out->arenaBytes = (unsigned long)sizeof(sMtxArena);
    /* Gfx_Alloc is a bump pointer with no bound of its own: it walks off the
     * end of the arena quietly, so the only check is against the blob. */
    out->stage = (out->bytes <= OOT_DL_BLOB_BYTES) ? 0ul : 1ul;
}
