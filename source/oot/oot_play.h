#ifndef OCARINA_OOT_PLAY_H
#define OCARINA_OOT_PLAY_H

#include "oot_col.h"

/* The walk slice: one player position, one camera, and the room's own collision
 * mesh answering the two questions a walk asks it - what is the floor under
 * here, and does this step cross a wall.
 *
 * The player is a placeholder marker, not Link. There is no skeleton, no
 * animation and no actor system in this build; what the marker proves is that
 * the position the collision layer accepts reaches the same camera and the same
 * room mesh the previous phases validated. Anything the game does beyond that
 * (turn rate, acceleration, slopes, ledges, water) is either approximated here
 * or not modelled at all, and the history file says which.
 *
 * Plain C types only: this header is read from both sides of the seam, the same
 * reason oot_plat.h gives. */

#define OOT_PLAY_SEG 4u

/* The F3DEX2 marker list lives in segment 4, which no other module uses: the
 * room owns 3, the scene archive 2, its prologue 5, the demo 6 and the game's
 * own Gfx_Alloc list 7. */

/* Movement constants. STEP is how far above the feet a floor may be and still
 * be walked onto. It is the port's number, not the game's: OoT derives the step
 * from the actor's step height and the current speed, which this build does not
 * model. RADIUS is the push-out distance after a wall line test hits, the same
 * shape of correction the game applies along the polygon's horizontal normal.
 * CHEST is the height the horizontal ray is cast at, high enough to miss the
 * floor the player stands on and low enough to be stopped by a doorway. */
#define OOT_PLAY_STEP   26.0f
#define OOT_PLAY_RADIUS 12.0f
#define OOT_PLAY_CHEST  26.0f

/* R_RUN_SPEED_LIMIT / 100 from z_player_lib.c's boot registers: 600 / 100. The
 * speed curve below is Player_CalcSpeedAndYawFromControlStick's curved mode,
 * clipped to this cap the way the game's speedCap does. */
#define OOT_PLAY_SPEED_CAP 6.0f

/* World units per 20 Hz update, from the same curved mode:
 *     mag -= 20; speed = (1 - cosS(mag * 450))^2 * 30 + 7; speed *= 0.14.
 * The constants are the game's, copied from z_player.c, not fitted to anything.
 * The floor-pitch term of that function is not modelled: the collision layer
 * reports the floor's height, not its normal, so there is no pitch to read. */
#define OOT_PLAY_CAM_DIST 220.0f
#define OOT_PLAY_CAM_HEIGHT 150.0f
#define OOT_PLAY_AT_HEIGHT 36.0f
#define OOT_PLAY_FOV 60.0f
#define OOT_PLAY_NEAR 24.0f
#define OOT_PLAY_FAR 2200.0f

/* The walk camera's world -> engine exponent. The orbit fit takes 9 because its
 * eye is only 2397 units from the room's farthest corner; a camera inside the
 * room can see up to ~7000 units away, and at shift 9 that is 56000 in the
 * engine's 4.12, past the +-32768 clamp. At 10 the same corner is 28000, and one
 * world unit is four engine units, so the quantisation is a quarter of a unit. */
#define OOT_PLAY_SHIFT 10u

typedef struct {
    float pos[3];    /* feet, world units */
    float spawn[3];
    float eye[3];    /* rebuilt by ootPlayBuild */
    float at[3];
    float floorY;    /* the floor ootPlayTick last accepted */
    float vy;        /* vertical speed while airborne */
    float speed;     /* last horizontal step, world units per tick */
    short yaw;       /* facing, binangle; forward = (sin, cos) */
    short camYaw;    /* camera input yaw, same convention */
    unsigned long ticks;
    unsigned long moves;    /* ticks that moved horizontally */
    unsigned long blocked;  /* ticks whose step a wall stopped */
    unsigned long grounded; /* ticks that ended on a floor */
    unsigned long voids;    /* ticks with no floor under the feet */
    unsigned long falls;    /* respawns after leaving the mesh */
    unsigned long camHits;  /* frames the camera ray was stopped by a wall */
    int onFloor;
    int ok;                 /* 1 once the marker list is built */
    const void *list;       /* segment 4, host byte order */
    const void *blob;
    const void *proj;
    const void *mv;
    const void *vtx;
    unsigned long bytes;
} OotPlay;

/* spawn is the player-entry position the scene's spawn 0 points at. camYaw is
 * the direction the camera looks in: at 0x8000 the room is seen toward -z. */
void ootPlayInit(OotPlay *p, const float spawn[3], short camYaw);

/* One 20 Hz update. stickX/stickY are the N64's own signed values (-60..60,
 * positive x = right, positive y = up/forward); camTurn is added to the camera
 * yaw, one binangle per C button press. */
void ootPlayTick(OotPlay *p, const OotCol *col, int stickX, int stickY,
                 int camTurn);

/* Rewrites the marker's vertices and the projection/view matrices from wherever
 * the tick left the player, and writes both matrices into the list's own
 * prologue. Safe to call once per rendered frame, after the 20 Hz ticks. */
void ootPlayBuild(OotPlay *p, const OotCol *col);

#endif
