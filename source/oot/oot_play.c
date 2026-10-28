#include "ultra64.h"

#include <math.h>
#include <string.h>

#include "gfxalloc.h"
#include "sys_math.h"
#include "z_lib.h"

#include "oot_col.h"
#include "oot_play.h"

/* The marker is drawn by the same decoder the room is: a display list in this
 * CPU's byte order that the decoder walks through segment 4. It is geometry the
 * port wrote, not game content, and its job is to make the position the
 * collision layer accepted visible on the panel.
 *
 * Three boxes, feet at y = 0: a magenta body, a yellow head and a cyan nose.
 * The nose is there because the walk has a facing and a screenshot cannot
 * invent one; with it, the direction the tick moved the player and the
 * direction the marker points can be compared face to face.
 *
 * The vertex layout is the N64's and has to match F3Vtx in f3dex2.h, which
 * cannot be included on this side of the seam: that header pulls in <nds.h>.
 * The assert below is the mirror image of the one in f3dex2.h. */
typedef struct {
    s16 x, y, z;
    u16 flag;
    s16 s, t;
    u8 r, g, b, a;
} PlayVtx;

_Static_assert(sizeof(PlayVtx) == 16, "marker vertex must match the N64 layout");

#define PLAY_VTX_COUNT 24u
#define PLAY_CMD_WORDS 96u
#define PLAY_BLOB_BYTES 2048u

/* Twelve triangles for eight corners. The GE is fed CULL_NONE for these, so the
 * winding only has to be self-consistent, not outward. */
static const u8 sBoxTris[36] = {
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    0, 4, 5, 0, 5, 1,
    3, 2, 6, 3, 6, 7,
    0, 3, 7, 0, 7, 4,
    1, 5, 6, 1, 6, 2,
};

static u8 sBlob[PLAY_BLOB_BYTES] __attribute__((aligned(16)));
static PlayVtx *sVtx;

static u32 segAddr(const void *p)
{
    return ((u32)OOT_PLAY_SEG << 24) | (u32)((const u8 *)p - sBlob);
}

/* The marker's eight corners are written straight into world coordinates, each
 * frame, rather than left local and placed by a modelview of its own.
 *
 * That is a decision about matrix conventions, not about geometry. guLookAt
 * writes the N64's row-vector Mtx; the game's float stack works in the
 * column-vector MtxF of sys_matrix.c, and a modelview built by transposing one
 * into the other and multiplying landed somewhere the arithmetic did not
 * predict (the capture showed 36 submitted triangles, all of them clipped, and
 * an eye-space probe reading (110, 581, 395) where the hand calculation says
 * (0, -10, -253)). Twenty-four vertices cost a multiply each, avoid the
 * conversion entirely, and leave the marker loading exactly the view matrix the
 * room is drawn with.
 *
 * The placement is a yaw rotation whose local +z is the movement direction
 * (sin, cos) and whose local +x is the camera-right of that facing, f x up:
 *     world.x = pos.x - lx * cos(yaw) + lz * sin(yaw)
 *     world.z = pos.z + lx * sin(yaw) + lz * cos(yaw)
 * so at yaw 0x8000 (facing -z) the nose points -z and the box's right side
 * faces +x. */
static void fillBox(PlayVtx *v, const float pos[3], float sy, float cy,
                    s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1,
                    u8 r, u8 g, u8 b)
{
    static const s16 bx[8] = { 0, 1, 1, 0, 0, 1, 1, 0 };
    static const s16 by[8] = { 0, 0, 1, 1, 0, 0, 1, 1 };
    static const s16 bz[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    u32 i;

    for (i = 0; i < 8; i++) {
        float lx = (float)(bx[i] ? x1 : x0);
        float ly = (float)(by[i] ? y1 : y0);
        float lz = (float)(bz[i] ? z1 : z0);

        v[i].x = (s16)(pos[0] + (-lx * cy + lz * sy));
        v[i].y = (s16)(pos[1] + ly);
        v[i].z = (s16)(pos[2] + (lx * sy + lz * cy));
        v[i].flag = 0;
        v[i].s = 0;
        v[i].t = 0;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
        v[i].a = 255;
    }
}

/* The three boxes at one position and facing, into the vertices the list
 * already points at. Called once by ootPlayInit and again every frame from
 * ootPlayBuild. */
static void placeMarker(const float pos[3], short yaw)
{
    PlayVtx *v = (PlayVtx *)sVtx;
    float sy = Math_SinS(yaw);
    float cy = Math_CosS(yaw);

    fillBox(v + 0, pos, sy, cy, -12, 0, -8, 12, 36, 8, 255, 0, 220);
    fillBox(v + 8, pos, sy, cy, -8, 36, -8, 8, 50, 8, 255, 220, 40);
    fillBox(v + 16, pos, sy, cy, -3, 42, 8, 3, 47, 16, 0, 255, 255);
}

static Gfx *cmd(Gfx *g, u32 w0, u32 w1)
{
    g->words.w0 = w0;
    g->words.w1 = w1;
    return g + 1;
}

void ootPlayInit(OotPlay *p, const float spawn[3], short camYaw)
{
    Gfx *cur = Gfx_Open((Gfx *)sBlob);
    PlayVtx *v = (PlayVtx *)Gfx_Alloc(&cur, PLAY_VTX_COUNT * sizeof(PlayVtx));
    Mtx *proj = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Mtx *mv = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Gfx *list = (Gfx *)Gfx_Alloc(&cur, PLAY_CMD_WORDS * sizeof(Gfx));
    Gfx *g = list;
    u32 box;

    memset(p, 0, sizeof(*p));
    p->pos[0] = spawn[0];
    p->pos[1] = spawn[1];
    p->pos[2] = spawn[2];
    p->spawn[0] = spawn[0];
    p->spawn[1] = spawn[1];
    p->spawn[2] = spawn[2];
    p->floorY = spawn[1];
    p->camYaw = camYaw;
    p->yaw = camYaw;
    p->onFloor = 1;

    sVtx = v;
    placeMarker(p->pos, p->yaw);

    /* G_SETCOMBINE SHADE: the same two words the demo list uses to make the
     * vertex colours win with texturing disabled. The decoder starts every list
     * with texturing off, so no G_TEXTURE command is needed. */
    g = cmd(g, 0xFCFFFFFFu, 0xFFFE793Cu);
    gSPMatrix(g++, segAddr(proj), G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(g++, segAddr(mv), G_MTX_LOAD);

    for (box = 0; box < 3; box++) {
        u32 v0 = box * 8u;
        u32 i;

        g = cmd(g, 0x01000000u | (8u << 12) | ((v0 + 8u) << 1),
                segAddr(v + v0));
        for (i = 0; i < 12u; i++) {
            u32 a = v0 + sBoxTris[i * 3u];
            u32 b = v0 + sBoxTris[i * 3u + 1u];
            u32 c = v0 + sBoxTris[i * 3u + 2u];

            g = cmd(g, 0x05000000u | (a << 17) | (b << 9) | (c << 1), 0);
        }
    }
    g = cmd(g, 0xDF000000u, 0);

    p->proj = proj;
    p->mv = mv;
    p->vtx = v;
    p->list = list;
    p->blob = sBlob;
    p->bytes = (unsigned long)((const u8 *)cur - sBlob);
    p->ok = (p->bytes <= PLAY_BLOB_BYTES) &&
            ((unsigned long)(g - list) <= PLAY_CMD_WORDS);
}

/* The camera the walk is seen through. It sits OOT_PLAY_CAM_DIST behind the
 * facing it looks along and OOT_PLAY_CAM_HEIGHT above the feet, aimed at chest
 * height. If the room's walls stand between the head and that point, the eye is
 * pulled back along the same ray and stopped short of the wall.
 *
 * The marker's corners are already in world coordinates (see placeMarker), so
 * its list loads no model matrix at all: the view written here is the whole
 * modelview for both lists, and marker and room are seen through the same
 * camera by construction. */
void ootPlayBuild(OotPlay *p, const OotCol *col)
{
    u16 perspNorm = 0;
    float fwd[2];
    float from[3];
    float hit[3];
    float n[3];

    guPerspective((Mtx *)p->proj, &perspNorm, OOT_PLAY_FOV, 4.0f / 3.0f,
                  OOT_PLAY_NEAR, OOT_PLAY_FAR, 1.0f);

    fwd[0] = Math_SinS(p->camYaw);
    fwd[1] = Math_CosS(p->camYaw);

    p->at[0] = p->pos[0];
    p->at[1] = p->pos[1] + OOT_PLAY_AT_HEIGHT;
    p->at[2] = p->pos[2];

    p->eye[0] = p->pos[0] - fwd[0] * OOT_PLAY_CAM_DIST;
    p->eye[1] = p->pos[1] + OOT_PLAY_CAM_HEIGHT;
    p->eye[2] = p->pos[2] - fwd[1] * OOT_PLAY_CAM_DIST;

    if (col != NULL && col->stage == 0) {
        from[0] = p->pos[0];
        from[1] = p->pos[1] + 60.0f;
        from[2] = p->pos[2];
        if (ootColLine(col, from, p->eye, hit, n)) {
            float ex = p->eye[0] - from[0];
            float ey = p->eye[1] - from[1];
            float ez = p->eye[2] - from[2];
            float hx = hit[0] - from[0];
            float hy = hit[1] - from[1];
            float hz = hit[2] - from[2];
            float len = sqrtf(ex * ex + ey * ey + ez * ez);
            float hlen = sqrtf(hx * hx + hy * hy + hz * hz);
            float t;

            p->camHits++;
            if (len > 0.001f) {
                t = (hlen - 12.0f) / len;
                if (t < 0.2f) {
                    t = 0.2f;
                }
                p->eye[0] = from[0] + ex * t;
                p->eye[1] = from[1] + ey * t;
                p->eye[2] = from[2] + ez * t;
            }
        }
    }

    placeMarker(p->pos, p->yaw);
    guLookAt((Mtx *)p->mv, p->eye[0], p->eye[1], p->eye[2],
             p->at[0], p->at[1], p->at[2], 0.0f, 1.0f, 0.0f);
}

void ootPlayTick(OotPlay *p, const OotCol *col, int stickX, int stickY,
                 int camTurn)
{
    float mag;
    float target = 0.0f;
    float dx;
    float dz;
    float fy;
    float from[3];
    float to[3];
    float hit[3];
    float n[3];

    p->ticks++;
    p->camYaw = (s16)(p->camYaw + camTurn);

    /* Lib_GetControlStickData's magnitude and angle, then the curved speed mode
     * of Player_CalcSpeedAndYawFromControlStick. The stick is read raw, not
     * dead-zoned: OoT's own Input has no dead-zone subtraction and the curve
     * already returns zero below 20. */
    mag = sqrtf((float)stickX * (float)stickX + (float)stickY * (float)stickY);
    if (mag > 60.0f) {
        mag = 60.0f;
    }

    if (mag > 0.0f) {
        p->yaw = (s16)(p->camYaw + Math_Atan2S((float)stickY, (float)-stickX));
    }

    if (mag >= 20.0f) {
        float t = 1.0f - Math_CosS((s16)((mag - 20.0f) * 450.0f));

        target = ((t * t) * 30.0f + 7.0f) * 0.14f;
        if (target > OOT_PLAY_SPEED_CAP) {
            target = OOT_PLAY_SPEED_CAP;
        }
    }
    p->speed = target;

    if (col == NULL || col->stage != 0) {
        return;
    }

    dx = target * Math_SinS(p->yaw);
    dz = target * Math_CosS(p->yaw);

    if (dx != 0.0f || dz != 0.0f) {
        from[0] = p->pos[0];
        from[1] = p->pos[1] + OOT_PLAY_CHEST;
        from[2] = p->pos[2];
        to[0] = p->pos[0] + dx;
        to[1] = from[1];
        to[2] = p->pos[2] + dz;

        /* Walls only: ootColLine tests the polygons this loader classified as
         * walls and ceilings, so the floor the player stands on can never stop
         * a step that is parallel to it. */
        if (ootColLine(col, from, to, hit, n)) {
            float nx = n[0];
            float nz = n[2];
            float len = sqrtf(nx * nx + nz * nz);

            if (len > 0.0001f) {
                nx /= len;
                nz /= len;
            }
            p->pos[0] = hit[0] + nx * OOT_PLAY_RADIUS;
            p->pos[2] = hit[2] + nz * OOT_PLAY_RADIUS;
            p->blocked++;
        } else {
            p->pos[0] += dx;
            p->pos[2] += dz;
            p->moves++;
        }
    }

    /* Floor first at step height, so a kerb is walked onto and a platform above
     * it is not. ootColFloor returns the highest floor strictly below the limit,
     * so this also reports the floor the player is standing on. */
    if (ootColFloor(col, p->pos[0], p->pos[2], p->pos[1] + OOT_PLAY_STEP, &fy) &&
        fy >= p->pos[1] - OOT_PLAY_STEP) {
        p->pos[1] = fy;
        p->floorY = fy;
        p->vy = 0.0f;
        p->onFloor = 1;
        p->grounded++;
    } else {
        /* No floor within a step: either a ledge or nothing at all. Fall with
         * the game's gravity constant (PLAYER_GRAVITY = -2 per update, terminal
         * -20) and land on the first floor the feet reach. */
        p->onFloor = 0;
        p->vy -= 2.0f;
        if (p->vy < -20.0f) {
            p->vy = -20.0f;
        }
        p->pos[1] += p->vy;
        p->voids++;

        if (ootColFloor(col, p->pos[0], p->pos[2], p->pos[1] + 1.0f, &fy) &&
            p->pos[1] <= fy) {
            p->pos[1] = fy;
            p->floorY = fy;
            p->vy = 0.0f;
            p->onFloor = 1;
            p->grounded++;
        } else if (p->pos[1] < -2000.0f) {
            p->pos[0] = p->spawn[0];
            p->pos[1] = p->spawn[1];
            p->pos[2] = p->spawn[2];
            p->floorY = p->spawn[1];
            p->vy = 0.0f;
            p->onFloor = 1;
            p->falls++;
        }
    }
}
