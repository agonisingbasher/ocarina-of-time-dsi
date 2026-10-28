#include "ultra64.h"

#include <math.h>

#include "bgcheck.h"

#include "oot_col.h"

/* The scene's collision header points at three arrays, all in the same file
 * and all big-endian: the vertices, the polygons and the surface types. Only
 * the first two are read here. The queries are the game's own: the point-in-
 * triangle rule with 300 units of tolerance and the line/plane/inside-tri test
 * come from z_bgcheck.c and sys_math3d.c, not from a second implementation of
 * the same arithmetic. What this file adds is the parse, the byte swap and the
 * lookup lists that stand in for the game's spatial subdivision. */
extern s32 CollisionPoly_CheckYIntersect(CollisionPoly *poly, Vec3s *vtxList,
                                         f32 x, f32 z, f32 *yIntersect,
                                         f32 chkDist);
extern s32 CollisionPoly_LineVsPoly(CollisionPoly *poly, Vec3s *vtxList,
                                    Vec3f *posA, Vec3f *posB,
                                    Vec3f *planeIntersect, s32 chkOneFace,
                                    f32 chkDist);

#define SCENE_CMD_COLLISION_HEADER 0x03u
#define SCENE_CMD_END              0x14u

#define COL_OFF(type, field) __builtin_offsetof(type, field)

_Static_assert(sizeof(Vec3s) == 6, "a collision vertex is three s16");
_Static_assert(sizeof(CollisionPoly) == 16, "a collision polygon is 16 bytes");
_Static_assert(COL_OFF(CollisionHeader, minBounds) == 0x00, "header 0x00");
_Static_assert(COL_OFF(CollisionHeader, maxBounds) == 0x06, "header 0x06");
_Static_assert(COL_OFF(CollisionHeader, numVertices) == 0x0C, "header 0x0C");
_Static_assert(COL_OFF(CollisionHeader, vtxList) == 0x10, "header 0x10");
_Static_assert(COL_OFF(CollisionHeader, numPolygons) == 0x14, "header 0x14");
_Static_assert(COL_OFF(CollisionHeader, polyList) == 0x18, "header 0x18");

static Vec3s sVtx[OOT_COL_MAX_VTX];
static CollisionPoly sPoly[OOT_COL_MAX_POLY];
/* Lookup lists, because the game's static lookup is built over the same
 * classification (floor / wall / ceiling) and walking every polygon per query
 * would make the 20 Hz tick pay for 2696 wall tests it cannot use. Floors are
 * queried one at a time; walls and ceilings are pooled because one line test
 * has to see both. */
static u16 sFloorIdx[OOT_COL_MAX_POLY];
static u16 sBlockIdx[OOT_COL_MAX_POLY];
static u32 sNumFloor;
static u32 sNumBlock;
/* One integer box per polygon, filled at load. The queries reject with these
 * before paying for a float triangle test: on the DS every float compare and
 * every s16->f32 conversion is a soft-float call, and a 20 Hz tick that scans
 * 2696 walls was costing seconds per frame with the box recomputed per visit. */
static s16 sBoxMin[OOT_COL_MAX_POLY][3];
static s16 sBoxMax[OOT_COL_MAX_POLY][3];

static u16 be16(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static s16 bes16(const u8 *p)
{
    return (s16)be16(p);
}

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static u32 segOff(u32 addr)
{
    return addr & 0xFFFFFFu;
}

void ootColOpen(OotCol *col, const unsigned char *data, unsigned long size)
{
    u32 hdr = 0xFFFFFFFFu;
    u32 nv;
    u32 vtx;
    u32 np;
    u32 pol;
    unsigned long o;
    u32 i;

    *col = (OotCol){ 0 };
    sNumFloor = 0;
    sNumBlock = 0;

    for (o = 0; o + 8 <= size; o += 8) {
        u32 code = data[o];

        if (code == SCENE_CMD_END) {
            break;
        }
        col->cmds++;
        if (code == SCENE_CMD_COLLISION_HEADER) {
            hdr = segOff(be32(data + o + 4));
            break;
        }
    }
    if (hdr == 0xFFFFFFFFu || (unsigned long)hdr + 0x2C > size) {
        col->stage = 1; /* no header, or its fixed part runs past the file */
        return;
    }

    col->min[0] = bes16(data + hdr + COL_OFF(CollisionHeader, minBounds) + 0);
    col->min[1] = bes16(data + hdr + COL_OFF(CollisionHeader, minBounds) + 2);
    col->min[2] = bes16(data + hdr + COL_OFF(CollisionHeader, minBounds) + 4);
    col->max[0] = bes16(data + hdr + COL_OFF(CollisionHeader, maxBounds) + 0);
    col->max[1] = bes16(data + hdr + COL_OFF(CollisionHeader, maxBounds) + 2);
    col->max[2] = bes16(data + hdr + COL_OFF(CollisionHeader, maxBounds) + 4);

    nv = be16(data + hdr + COL_OFF(CollisionHeader, numVertices));
    vtx = segOff(be32(data + hdr + COL_OFF(CollisionHeader, vtxList)));
    np = be16(data + hdr + COL_OFF(CollisionHeader, numPolygons));
    pol = segOff(be32(data + hdr + COL_OFF(CollisionHeader, polyList)));

    if ((unsigned long)vtx + (unsigned long)nv * sizeof(Vec3s) > size ||
        (unsigned long)pol + (unsigned long)np * sizeof(CollisionPoly) > size) {
        col->stage = 2; /* an array runs past the file */
        return;
    }
    if (nv > OOT_COL_MAX_VTX || np > OOT_COL_MAX_POLY) {
        col->stage = 3; /* the scene needs more room than this port reserves */
        return;
    }

    for (i = 0; i < nv; i++) {
        const u8 *v = data + vtx + (unsigned long)i * sizeof(Vec3s);

        sVtx[i].x = bes16(v + 0);
        sVtx[i].y = bes16(v + 2);
        sVtx[i].z = bes16(v + 4);
    }

    col->numVtx = nv;
    col->numPoly = np;
    for (i = 0; i < np; i++) {
        const u8 *q = data + pol + (unsigned long)i * sizeof(CollisionPoly);
        CollisionPoly *p = &sPoly[i];
        u32 ia;
        u32 ib;
        u32 ic;

        p->type = be16(q + 0);
        p->flags_vIA = be16(q + 2);
        p->flags_vIB = be16(q + 4);
        p->vIC = be16(q + 6);
        p->normal.x = bes16(q + 8);
        p->normal.y = bes16(q + 10);
        p->normal.z = bes16(q + 12);
        p->dist = bes16(q + 14);

        ia = COLPOLY_VTX_INDEX(p->flags_vIA);
        ib = COLPOLY_VTX_INDEX(p->flags_vIB);
        ic = COLPOLY_VTX_INDEX(p->vIC);
        if (ia >= nv || ib >= nv || ic >= nv) {
            col->stage = 4; /* an index points outside the vertex list */
            return;
        }

        {
            s16 lo;
            s16 hi;

            lo = sVtx[ia].x < sVtx[ib].x ? sVtx[ia].x : sVtx[ib].x;
            sBoxMin[i][0] = lo < sVtx[ic].x ? lo : sVtx[ic].x;
            hi = sVtx[ia].x > sVtx[ib].x ? sVtx[ia].x : sVtx[ib].x;
            sBoxMax[i][0] = hi > sVtx[ic].x ? hi : sVtx[ic].x;

            lo = sVtx[ia].y < sVtx[ib].y ? sVtx[ia].y : sVtx[ib].y;
            sBoxMin[i][1] = lo < sVtx[ic].y ? lo : sVtx[ic].y;
            hi = sVtx[ia].y > sVtx[ib].y ? sVtx[ia].y : sVtx[ib].y;
            sBoxMax[i][1] = hi > sVtx[ic].y ? hi : sVtx[ic].y;

            lo = sVtx[ia].z < sVtx[ib].z ? sVtx[ia].z : sVtx[ib].z;
            sBoxMin[i][2] = lo < sVtx[ic].z ? lo : sVtx[ic].z;
            hi = sVtx[ia].z > sVtx[ib].z ? sVtx[ia].z : sVtx[ib].z;
            sBoxMax[i][2] = hi > sVtx[ic].z ? hi : sVtx[ic].z;
        }

        /* The same two thresholds z_bgcheck.c uses to fill its three lookup
         * lists: a floor is steeper than 0.5 and a ceiling is above -0.8;
         * everything else is a wall. */
        if (p->normal.y > COLPOLY_SNORMAL(0.5f)) {
            sFloorIdx[sNumFloor++] = (u16)i;
            col->numFloor++;
        } else {
            sBlockIdx[sNumBlock++] = (u16)i;
            if (p->normal.y < COLPOLY_SNORMAL(-0.8f)) {
                col->numCeil++;
            } else {
                col->numWall++;
            }
        }
    }
}

static int polyBoxXZMiss(u32 i, long x0, long x1, long z0, long z1);
static int polyBoxMiss(u32 i, const long bmin[3], const long bmax[3]);

int ootColFloor(const OotCol *col, float x, float z, float yLimit, float *yOut)
{
    f32 best = 0.0f;
    int found = 0;
    long x0;
    long x1;
    long z0;
    long z1;
    u32 k;

    if (col->stage != 0) {
        return 0;
    }
    /* The game's floor test admits a point within 1.0 of the polygon; 1.5
     * keeps this reject a strict superset of that rule. */
    x0 = (long)floorf(x - 1.5f);
    x1 = (long)ceilf(x + 1.5f);
    z0 = (long)floorf(z - 1.5f);
    z1 = (long)ceilf(z + 1.5f);
    for (k = 0; k < col->numFloor; k++) {
        u32 i = sFloorIdx[k];
        CollisionPoly *p = &sPoly[i];
        f32 y;

        if (polyBoxXZMiss(i, x0, x1, z0, z1)) {
            continue;
        }
        if (CollisionPoly_CheckYIntersect(p, sVtx, x, z, &y, 1.0f) &&
            y < yLimit && (!found || y > best)) {
            best = y;
            found = 1;
        }
    }
    if (found) {
        *yOut = best;
    }
    return found;
}

/* Integer bounding-box rejects against the boxes precomputed at load. The
 * query boxes are already grown by the caller's tolerance, so an overlap here
 * is a superset of "the float test can succeed" and a miss cannot be. */
static int polyBoxXZMiss(u32 i, long x0, long x1, long z0, long z1)
{
    return (long)sBoxMin[i][0] > x1 || (long)sBoxMax[i][0] < x0 ||
           (long)sBoxMin[i][2] > z1 || (long)sBoxMax[i][2] < z0;
}

static int polyBoxMiss(u32 i, const long bmin[3], const long bmax[3])
{
    return (long)sBoxMin[i][0] > bmax[0] || (long)sBoxMax[i][0] < bmin[0] ||
           (long)sBoxMin[i][1] > bmax[1] || (long)sBoxMax[i][1] < bmin[1] ||
           (long)sBoxMin[i][2] > bmax[2] || (long)sBoxMax[i][2] < bmin[2];
}

int ootColLine(const OotCol *col, const float from[3], const float to[3],
               float hit[3], float normal[3])
{
    Vec3f a;
    Vec3f b;
    Vec3f ip;
    CollisionPoly *best = NULL;
    long bmin[3];
    long bmax[3];
    f32 bestSq = 0.0f;
    u32 k;

    a.x = from[0];
    a.y = from[1];
    a.z = from[2];
    b.x = to[0];
    b.y = to[1];
    b.z = to[2];
    if (col->stage != 0) {
        return 0;
    }
    for (k = 0; k < 3; k++) {
        f32 lo = from[k] < to[k] ? from[k] : to[k];
        f32 hi = from[k] < to[k] ? to[k] : from[k];

        bmin[k] = (long)floorf(lo - 1.5f);
        bmax[k] = (long)ceilf(hi + 1.5f);
    }

    for (k = 0; k < sNumBlock; k++) {
        u32 i = sBlockIdx[k];
        CollisionPoly *p = &sPoly[i];
        f32 dx;
        f32 dy;
        f32 dz;
        f32 dsq;

        if (polyBoxMiss(i, bmin, bmax)) {
            continue;
        }
        if (!CollisionPoly_LineVsPoly(p, sVtx, &a, &b, &ip, 0, 1.0f)) {
            continue;
        }
        dx = ip.x - a.x;
        dy = ip.y - a.y;
        dz = ip.z - a.z;
        dsq = dx * dx + dy * dy + dz * dz;
        if (best == NULL || dsq < bestSq) {
            best = p;
            bestSq = dsq;
            hit[0] = ip.x;
            hit[1] = ip.y;
            hit[2] = ip.z;
        }
    }
    if (best == NULL) {
        return 0;
    }
    normal[0] = COLPOLY_GET_NORMAL(best->normal.x);
    normal[1] = COLPOLY_GET_NORMAL(best->normal.y);
    normal[2] = COLPOLY_GET_NORMAL(best->normal.z);
    return 1;
}

void ootColSelfTest(const OotCol *col, float spawnX, float spawnY, float spawnZ,
                    OotColTest *t)
{
    f32 y = 0.0f;

    t->stage = col->stage;
    t->checks = 3;
    t->passed = 0;
    t->failAt = 0;

    if (col->numFloor + col->numWall + col->numCeil != col->numPoly) {
        t->failAt = 1;
        return;
    }
    t->passed++;

    if (spawnX < (f32)col->min[0] || spawnX > (f32)col->max[0] ||
        spawnY < (f32)col->min[1] || spawnY > (f32)col->max[1] ||
        spawnZ < (f32)col->min[2] || spawnZ > (f32)col->max[2]) {
        t->failAt = 2;
        return;
    }
    t->passed++;

    /* The scene file states the spawn's height twice: once as the player
     * entry's y and once as the floor polygon under it. They agree at 309.0
     * for Bmori1, so disagreement here is a parse or query error. */
    if (!ootColFloor(col, spawnX, spawnZ, spawnY + 4.0f, &y) ||
        y < spawnY - 1.0f || y > spawnY + 1.0f) {
        t->failAt = 3;
        return;
    }
    t->passed++;
}
