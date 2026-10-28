#ifndef OCARINA_OOT_DL_H
#define OCARINA_OOT_DL_H

/* A display list built by the game's own code: Gfx_Alloc carves it out of one
 * blob together with its matrices, its viewport and its vertices, and the gSP*
 * macros write the command words. Plain pointer types only, for the reason
 * spelled out in oot_plat.h. */
typedef struct {
    const void *list;     /* first Gfx of the list, run it through the decoder */
    const void *blob;     /* base of the segment the addresses are relative to */
    const void *vtx;      /* the eight Vtx the two quads use */
    const void *proj;     /* the Mtx guPerspective wrote */
    const void *mv;       /* the Mtx the game's own matrix stack wrote */
    unsigned long stage;  /* 0 when every block landed inside the blob */
    unsigned long bytes;  /* blob bytes consumed by the cursor */
    long edge;            /* N64 pixels where the red quad ends */
    long rotZ;            /* binangle the stack rotated the modelview by */
    unsigned long stackOff;  /* where the matrix stack's current slot sits */
    unsigned long arenaBytes; /* size of the arena that stack grew out of */
} OotDl;

/* Segment the addresses in the list are tagged with. Segment 6 is the demo
 * blob's, so this list gets its own. */
#define OOT_DL_SEG 7u

void ootDlBuild(OotDl *out, unsigned short angle);

/* Grows the 20-slot matrix stack sys_matrix.c keeps its current matrix in, out
 * of an arena this module owns. Idempotent, and shared: oot_room.c builds its
 * camera with the same stack rather than keeping a second one alive. */
void ootMtxStackInit(void);

#endif
