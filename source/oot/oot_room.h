#ifndef OCARINA_OOT_ROOM_H
#define OCARINA_OOT_ROOM_H

/* A room file as the game DMA's it: one segment, big-endian, whose first bytes
 * are a SceneCmd list. oot_room.c reads it with the decomp's own struct
 * definitions, so the offsets below are the game's and not a second opinion
 * written down by the port.
 *
 * Plain C types only, for the reason spelled out in oot_plat.h: this header is
 * read from both sides of the seam. */

/* The segment the room's own addresses carry. The game points gSegments[3] at
 * the room buffer, so every Gfx*, Vtx* and Mtx* inside the file is 0x03xxxxxx
 * and the decoder resolves them all through one base pointer. */
#define OOT_ROOM_SEG 3u

/* The segment the scene archive gets registered as, because a textured room
 * reaches through it for the palette every chunk loads: z_play.c points
 * gSegments[2] at play->sceneSegment right before it calls Room_DrawRoom, and
 * measured over Bmori1_room_0, twenty-five of its seventy-five G_SETTIMG carry
 * 0x02 -- the two CI palettes and the second texture of each pair. */
#define OOT_SCENE_SEG 2u

/* The segment the prologue blob gets registered as, because its own commands
 * address its viewport and matrices. */
#define OOT_ROOM_PRO_SEG 5u

/* Shape entries the prologue is willing to call. A normal room's header carries
 * one per visible region, and the one hundred thirty-four normal rooms of
 * ntsc-1.2 measured through the extracted baserom top out well under this;
 * entriesUsed reports how many were actually emitted. */
#define OOT_ROOM_MAX_ENTRIES 8

typedef struct {
    /* Parse. stage stays 0 when every command the loader needs was understood;
     * a non-zero value says which check refused, and the draw pass then emits a
     * prologue with no display lists in it. */
    unsigned long stage;
    unsigned long cmds;        /* SceneCmds walked up to the end marker */
    unsigned long shapeType;   /* RoomShapeBase.type */
    unsigned long roomType;    /* SCmdMesh's data1: RoomType */
    unsigned long numEntries;  /* what the shape header claims */
    unsigned long entriesUsed; /* what the prologue calls */
    unsigned long numObjects;  /* SCmdObjectList's count */
    unsigned long numActors;   /* SCmdActorEntryList's count */
    const void *rootOpa;       /* host pointer of the first opaque Gfx */
    const void *rootXlu;       /* its translucent partner, NULL when absent */

    /* Draw. */
    const void *list;   /* prologue's first Gfx, in this CPU's byte order */
    const void *blob;   /* register as OOT_ROOM_PRO_SEG */
    const void *proj;   /* the Mtx guPerspective wrote */
    const void *mv;     /* the Mtx the game's matrix stack wrote */
    unsigned long bytes; /* prologue blob bytes the cursor consumed */
    unsigned long shift; /* world -> geometry engine exponent, see below */
    long min[3];         /* the bounds it was drawn against, world units */
    long max[3];
    long center[3];
    long dist;           /* eye distance from the centre */
    long eye[3];         /* where that put the camera */
    long eyeMax;         /* farthest any corner of the bounds can reach */
    long nearPlane;
    long farPlane;
} OotRoom;

/* Reads the header out of a room image already in memory. The image stays
 * borrowed: nothing here copies or decodes it, because the decoder walks it in
 * place through segment 3. */
void ootRoomOpen(OotRoom *rm, const unsigned char *data, unsigned long size);

/* Builds the prologue list and the two matrices for one orbit of the camera
 * around the given bounds. yaw is a binangle, so a quarter turn is 0x4000.
 *
 * The bounds come from the caller because only the display-list walk can
 * produce them, and that walk belongs to the decoder rather than this file.
 * From them this function takes the eye distance that frames the room and the
 * shift that keeps its farthest corner inside the 4.12 the geometry engine
 * takes for a vertex, which is the whole of the port's scale policy: no
 * scaling in the modelview, one exponent per room. */
void ootRoomBuild(OotRoom *rm, const long min[3], const long max[3],
                  unsigned short yaw);

/* Rewrites the same two matrices the orbit fit wrote, this time as a free
 * camera: guLookAt for the modelview and a frustum the caller sizes for walking
 * rather than for framing the whole room.
 *
 * The shift comes from the caller because the orbit fit's exponent is tied to
 * the distance from the room's centre, and a walk camera sits inside the room
 * where the farthest visible corner is a different number. The engine takes
 * 4.12 vertex coordinates, so the exponent has to keep the farthest point of
 * the room inside +-32767 of those or the eye-space conversion clamps it.
 *
 * The display-list part of the prologue is untouched: only proj and mv change,
 * so the room keeps being drawn by its own lists. */
void ootRoomWalkView(OotRoom *rm, const float eye[3], const float at[3],
                     unsigned long shift, float nearPlane, float farPlane);

#endif
