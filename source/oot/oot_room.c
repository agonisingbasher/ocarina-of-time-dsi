#include "ultra64.h"

#include "gfxalloc.h"
#include "room.h"
#include "sys_matrix.h"

#include "oot_dl.h"
#include "oot_room.h"

/* The room's own bytes are never rewritten here. oot_room.c reads the header
 * with the decomp's struct layouts, and the display lists themselves stay where
 * they are: the decoder walks them in place through segment 3, which the caller
 * has marked big-endian. What this file produces is one small list in this
 * CPU's own order that hands the room's lists to the decoder with a viewport, a
 * projection and a camera already loaded. */
#define OOT_ROOM_BLOB_BYTES 1024u

/* The SceneCmd codes this loader cares about, by the name the decomp gives them
 * in include/scene.h. A room file reuses the scene header format: the game's own
 * Scene_ExecuteCommands walks the same list out of a room segment. */
#define SCENE_CMD_ACTOR_LIST  0x01u /* SCmdActorEntryList, data1 = count */
#define SCENE_CMD_MESH        0x0Au /* SCmdMesh, data = RoomShapeBase* */
#define SCENE_CMD_OBJECT_LIST 0x0Bu /* SCmdObjectList, data1 = count */
#define SCENE_CMD_END         0x14u /* SCmdEndMarker */

/* Offsets come out of the game's own structs rather than literals, so a
 * decomp that renames or resizes a field breaks the build here instead of
 * quietly reading the wrong four bytes at run time. */
#define RM_OFF(type, field) __builtin_offsetof(type, field)

/* The shape header's entry pointers, for the two shapes that carry display
 * lists. RoomShapeImage* holds a background bitmap instead and is reported as
 * an unsupported stage rather than drawn as nothing. */
_Static_assert(sizeof(RoomShapeBase) == 1, "shape type is one byte");
_Static_assert(sizeof(RoomShapeNormal) == 12, "normal shape header is 12 bytes");
_Static_assert(sizeof(RoomShapeCullable) == 12, "cullable shape header is 12 bytes");
_Static_assert(sizeof(RoomShapeDListsEntry) == 8, "normal entry is 8 bytes");
_Static_assert(sizeof(RoomShapeCullableEntry) == 16, "cullable entry is 16 bytes");

/* One room at a time, which is how the game loads them too: RoomContext holds a
 * single curRoom, and the pointers below belong to whichever room was last
 * opened. */
static u32 sEntryOpa[OOT_ROOM_MAX_ENTRIES];
static u32 sEntryXlu[OOT_ROOM_MAX_ENTRIES];
static u8 sBlob[OOT_ROOM_BLOB_BYTES] __attribute__((aligned(16)));

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static u32 segOff(u32 addr)
{
    return addr & 0xFFFFFFu;
}

/* Integer square root, because the include world this file is built in has no
 * math.h of its own and a camera fit does not need a fractional one: the value
 * only ever decides a distance and a shift. */
static long isqrt(long v)
{
    long x = v;
    long y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return x;
}

void ootRoomOpen(OotRoom *rm, const unsigned char *data, unsigned long size)
{
    u32 meshOff = 0;
    u32 ent;
    u32 entEnd;
    u32 stride;
    u32 opaAt;
    u32 xluAt;
    unsigned long o;
    unsigned long i;

    *rm = (OotRoom){ 0 };
    rm->stage = 0;

    for (o = 0; o + 8 <= size; o += 8) {
        u32 code = data[o];
        u32 param = data[o + 1];
        u32 payload = be32(data + o + 4);

        if (code == SCENE_CMD_END) {
            break;
        }
        rm->cmds++;
        if (code == SCENE_CMD_MESH) {
            meshOff = segOff(payload);
            rm->roomType = param;
        } else if (code == SCENE_CMD_OBJECT_LIST) {
            rm->numObjects = param;
        } else if (code == SCENE_CMD_ACTOR_LIST) {
            rm->numActors = param;
        }
    }

    if (meshOff == 0 || meshOff + RM_OFF(RoomShapeNormal, entriesEnd) >= size) {
        rm->stage = 1; /* no mesh command, or its header runs past the file */
        return;
    }

    rm->shapeType = data[meshOff];
    if (rm->shapeType != ROOM_SHAPE_TYPE_NORMAL &&
        rm->shapeType != ROOM_SHAPE_TYPE_CULLABLE) {
        rm->stage = 2; /* an image shape: a background, not geometry */
        return;
    }

    if (rm->shapeType == ROOM_SHAPE_TYPE_NORMAL) {
        stride = (u32)sizeof(RoomShapeDListsEntry);
        opaAt = RM_OFF(RoomShapeDListsEntry, opa);
        xluAt = RM_OFF(RoomShapeDListsEntry, xlu);
    } else {
        stride = (u32)sizeof(RoomShapeCullableEntry);
        opaAt = RM_OFF(RoomShapeCullableEntry, opa);
        xluAt = RM_OFF(RoomShapeCullableEntry, xlu);
    }

    rm->numEntries = data[meshOff + RM_OFF(RoomShapeNormal, numEntries)];
    ent = segOff(be32(data + meshOff + RM_OFF(RoomShapeNormal, entries)));
    entEnd = segOff(be32(data + meshOff + RM_OFF(RoomShapeNormal, entriesEnd)));

    if (entEnd < ent || (entEnd - ent) != stride * rm->numEntries ||
        entEnd > size) {
        rm->stage = 3; /* the entry array does not describe itself */
        return;
    }

    rm->entriesUsed = rm->numEntries;
    if (rm->entriesUsed > OOT_ROOM_MAX_ENTRIES) {
        rm->entriesUsed = OOT_ROOM_MAX_ENTRIES;
    }
    for (i = 0; i < rm->entriesUsed; i++) {
        const u8 *e = data + ent + (unsigned long)i * stride;

        sEntryOpa[i] = be32(e + opaAt);
        sEntryXlu[i] = be32(e + xluAt);
    }
    rm->rootOpa = data + segOff(sEntryOpa[0]);
    rm->rootXlu = sEntryXlu[0] ? data + segOff(sEntryXlu[0]) : NULL;
}

static u32 segAddr(const void *p)
{
    return ((u32)OOT_ROOM_PRO_SEG << 24) |
           (u32)((const u8 *)p - (const u8 *)sBlob);
}

void ootRoomBuild(OotRoom *rm, const long min[3], const long max[3],
                  unsigned short yaw)
{
    Gfx *cur = Gfx_Open((Gfx *)sBlob);
    Mtx *proj = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Mtx *mv = (Mtx *)Gfx_Alloc(&cur, sizeof(Mtx));
    Vp *vp = (Vp *)Gfx_Alloc(&cur, sizeof(Vp));
    Gfx *list = (Gfx *)Gfx_Alloc(
        &cur, (4u + 2u * rm->entriesUsed) * sizeof(Gfx));
    Gfx *g = list;
    u16 perspNorm = 0;
    unsigned long shift;
    long rad;
    long dist;
    long eyeMax;
    f32 nearPlane;
    f32 farPlane;
    int i;

    rm->blob = sBlob;
    rm->proj = proj;
    rm->mv = mv;

    /* The fit is a sphere, not the box: the camera orbits, so the room has to
     * stay framed at every yaw, and the corner distance is what the farthest
     * point can be from the centre at any of them. */
    for (i = 0; i < 3; i++) {
        rm->min[i] = min[i];
        rm->max[i] = max[i];
        rm->center[i] = (min[i] + max[i]) / 2;
    }
    rad = (long)isqrt(
        ((max[0] - min[0]) / 2) * ((max[0] - min[0]) / 2) +
        ((max[1] - min[1]) / 2) * ((max[1] - min[1]) / 2) +
        ((max[2] - min[2]) / 2) * ((max[2] - min[2]) / 2));
    /* 1.625 radii: the projection's half-width at the centre plane is tan(30
     * deg) * 4/3 = 0.77 of the distance, so 1.3 radii would just fit and this
     * leaves the room's own silhouette inside the panel at every yaw. */
    dist = rad + rad / 2 + rad / 8;
    eyeMax = dist + rad;

    /* The geometry engine takes a vertex in 4.12, so an eye coordinate has to
     * land inside +-32767 of those, and one world unit is 2^(12-shift) of them.
     * Taking the exponent from the room's own size is the whole scale policy:
     * the modelview never scales, and the projection is scale-invariant because
     * its translation column goes through the same exponent. */
    shift = 6;
    while (shift < 14 &&
           eyeMax * (1L << (12 - (int)shift)) > 32000L) {
        shift++;
    }
    rm->shift = shift;
    rm->dist = dist;
    rm->eyeMax = eyeMax;

    /* A frustum wrapped tight around the orbit: the nearest and farthest the
     * sphere reaches along the view axis. Wide bounds cost depth precision on a
     * hardware that has 16 bits of it, and nothing here needs more. */
    nearPlane = (f32)(dist - rad) * 0.75f;
    farPlane = (f32)(dist + rad) * 1.25f;
    if (nearPlane < 1.0f) {
        nearPlane = 1.0f;
    }
    rm->nearPlane = (long)nearPlane;
    rm->farPlane = (long)farPlane;

    guPerspective(proj, &perspNorm, 60.0f, 4.0f / 3.0f, nearPlane, farPlane, 1.0f);

    /* The whole 320x240 N64 screen. A room's lists carry no G_MOVEMEM of their
     * own: the game sets the viewport once per frame outside the room, so the
     * prologue has to. vscale and vtrans are in quarter pixels, so 640 is the
     * half-width of 320. */
    vp->vp.vscale[0] = 640;
    vp->vp.vscale[1] = 480;
    vp->vp.vscale[2] = 511;
    vp->vp.vscale[3] = 0;
    vp->vp.vtrans[0] = 640;
    vp->vp.vtrans[1] = 480;
    vp->vp.vtrans[2] = 0;
    vp->vp.vtrans[3] = 0;

    /* The camera comes out of the game's own matrix stack, and the order is the
     * one its arithmetic asks for: Matrix_Translate with MTXMODE_NEW leaves the
     * 3x3 identity and puts the vector in the translation slots, a following
     * MTXMODE_APPLY rotation rewrites only the 3x3, and a second APPLY translate
     * adds R * its vector. So the whole walk is
     *     out = R * v + T1 + R * T2
     * and the orbit, which wants out = R * (v - center) + (0,0,-dist), is the
     * distance first and the centring last. Reversing them leaves the push
     * unrotated and the centring rotated, and at three quarters of a turn the
     * room lands behind the camera: measured as a modelview z translation of
     * +2137 world units against a near plane at -667, which clipped all 510 of
     * the room's triangles. */
    ootMtxStackInit();
    Matrix_Put(&gIdentityMtxF);
    Matrix_Push();
    Matrix_Translate(0.0f, 0.0f, -(f32)dist, MTXMODE_NEW);
    Matrix_RotateZYX(0x1000, (s16)yaw, 0, MTXMODE_APPLY);
    Matrix_Translate(-(f32)rm->center[0], -(f32)rm->center[1],
                     -(f32)rm->center[2], MTXMODE_APPLY);
    Matrix_ToMtx(mv);
    Matrix_Pop();

    gSPViewport(g++, segAddr(vp));
    gSPMatrix(g++, segAddr(proj), G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(g++, segAddr(mv), G_MTX_LOAD);
    for (i = 0; i < (int)rm->entriesUsed; i++) {
        /* The addresses are the room's own, already tagged with segment 3, so
         * they go into the command verbatim: the decoder resolves them through
         * the base the caller registered, in big-endian. */
        if (sEntryOpa[i]) {
            gSPDisplayList(g++, sEntryOpa[i]);
        }
        if (sEntryXlu[i]) {
            gSPDisplayList(g++, sEntryXlu[i]);
        }
    }
    gSPEndDisplayList(g++);

    rm->list = list;
    rm->bytes = (unsigned long)((const u8 *)cur - sBlob);
    /* Gfx_Alloc is a bump cursor with no bound of its own, so the only check is
     * against the blob. */
    if (rm->bytes > OOT_ROOM_BLOB_BYTES) {
        rm->stage |= 4;
    }
}

void ootRoomWalkView(OotRoom *rm, const float eye[3], const float at[3],
                     unsigned long shift, float nearPlane, float farPlane)
{
    u16 perspNorm = 0;

    /* Same two slots the prologue loads, rewritten in place: the list keeps
     * pointing at them, so nothing else in the prologue has to move. */
    guPerspective((Mtx *)rm->proj, &perspNorm, 60.0f, 4.0f / 3.0f,
                  nearPlane, farPlane, 1.0f);
    guLookAt((Mtx *)rm->mv, eye[0], eye[1], eye[2],
             at[0], at[1], at[2], 0.0f, 1.0f, 0.0f);

    rm->shift = shift;
    rm->nearPlane = (long)nearPlane;
    rm->farPlane = (long)farPlane;
}
