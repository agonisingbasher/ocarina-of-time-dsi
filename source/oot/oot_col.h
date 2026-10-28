#ifndef OCARINA_OOT_COL_H
#define OCARINA_OOT_COL_H

/* The scene's collision mesh, read out of the scene archive the same way the
 * room loader reads its display lists: the file stays big-endian and borrowed,
 * and this layer makes one host-order copy of just the two arrays the game's
 * own queries need.
 *
 * Plain C types only, for the reason spelled out in oot_plat.h. */

/* Bmori1's scene has 3123 vertices and 3843 polygons. The caps are generous
 * enough for that room's scene and tight enough to stay a bounded static
 * allocation on a 4 MB machine; a scene that needs more is refused by stage
 * instead of silently truncated. */
#define OOT_COL_MAX_VTX  4096u
#define OOT_COL_MAX_POLY 6144u

typedef struct {
    /* Parse. stage stays 0 when the header was understood and every index is
     * in range; anything else names the check that refused. */
    unsigned long stage;
    unsigned long cmds;     /* SceneCmds walked up to the end marker */
    unsigned long numVtx;
    unsigned long numPoly;
    unsigned long numFloor; /* normal.y > 0.5 */
    unsigned long numWall;
    unsigned long numCeil;  /* normal.y < -0.8 */
    long min[3];            /* the header's own bounding box, world units */
    long max[3];
} OotCol;

/* Reads SCENE_CMD_COLLISION_HEADER out of a scene image already in memory. */
void ootColOpen(OotCol *col, const unsigned char *scene, unsigned long size);

/* Highest floor polygon under (x,z) with an intersection strictly below
 * yLimit, which is the game's own raycast-down rule. Returns 1 and stores the
 * height when there is one. */
int ootColFloor(const OotCol *col, float x, float z, float yLimit, float *yOut);

/* Nearest wall/ceiling polygon crossed by the segment from -> to. Returns 1
 * and stores the intersection point and the polygon's unit normal. The caller
 * pushes the result out by its own radius, as the game does. */
int ootColLine(const OotCol *col, const float from[3], const float to[3],
               float hit[3], float normal[3]);

typedef struct {
    unsigned long stage;    /* ootColOpen's stage, repeated for the readout */
    unsigned long checks;
    unsigned long passed;
    unsigned long failAt;
} OotColTest;

/* Checks the loaded mesh against the scene's own spawn point: the player entry
 * the game would start from must stand on a floor polygon at its recorded
 * height. That is one fact stated twice in the file, so it validates the parse
 * without a second copy of the geometry. */
void ootColSelfTest(const OotCol *col, float spawnX, float spawnY, float spawnZ,
                    OotColTest *t);

#endif
