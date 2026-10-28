#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "probe.h"
#include "bench.h"
#include "dl_demo.h"
#include "f3dex2.h"
#include "f3dex2_selftest.h"
#include "fs.h"
#include "gamemem.h"
#include "ge_draw.h"
#include "ndinput.h"
#include "oot_dl.h"
#include "oot_room.h"
#include "oot_selftest.h"
#include "oot_col.h"
#include "oot_play.h"
#include "perf.h"
#include "sched.h"
#include "timebase.h"
#include "video_top.h"
#include "ui_bottom.h"
#include "yaz0test.h"

/* 1 turns the room's slow auto rotation back on for a look around by hand; 0
 * leaves the camera where the projection starts so two captures of two builds
 * are the same picture. */
#define ROOM_SPIN 0

/* Where the camera is held while ROOM_SPIN is 0. A u16 angle, 65536 to a turn.
 * It is a knob because what a room looks like depends on where it is looked at
 * from: the panel grain, and the level the texture chain asks for, are both a
 * function of the angle, so a build is only comparable to another build that
 * was captured at the same one. */
#define ROOM_ANGLE 0u

static void line(int row, const char *text)
{
    iprintf("\x1b[%d;1H%-31.31s", row + 1, text);
}

static void printProbe(const HwProbe *hw)
{
    char buf[40];

    line(0, "Ocarina DSi P1 | START=exit");

    snprintf(buf, sizeof(buf), "DSi mode : %s", hw->twlMode ? "YES" : "NO");
    line(1, buf);

    snprintf(buf, sizeof(buf), "SCFG_CLK : %04X -> %04X", hw->scfgClkBefore, hw->scfgClkAfter);
    line(2, buf);

    snprintf(buf, sizeof(buf), "Heap     : %08lX (%lu.%lu MB)",
             (unsigned long)hw->heapLimit,
             (unsigned long)(hw->heapLimitTenths / 10),
             (unsigned long)(hw->heapLimitTenths % 10));
    line(3, buf);

    snprintf(buf, sizeof(buf), "RAM test : %s %lu KB %lu.%lu ms",
             hw->ramOk ? "OK" : "FAIL",
             (unsigned long)hw->ramTestedKb,
             (unsigned long)(hw->ramUs / 1000),
             (unsigned long)((hw->ramUs / 100) % 10));
    line(4, buf);
}

static void printInput(const NdInputTest *t, bool ok)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "InMap %u/%u sz%u,%u %s%s%u",
             (unsigned)t->passed, (unsigned)t->checks,
             (unsigned)sizeof(NdPad), (unsigned)sizeof(NdInput),
             ok ? "ok" : "BAD", ok ? "" : "#", ok ? 0u : (unsigned)t->failAt);
    line(5, buf);
}

static void printYaz0(const Yaz0Test *y)
{
    char buf[64];
    u32 memMb = 0;
    u32 strmMb = 0;

    if (!y->headerOk) {
        line(6, "Yaz0 test: BAD header");
        return;
    }

    if (y->ok && y->us) {
        memMb = (u32)(((u64)y->bytes * 10000000ull) / ((u64)y->us * 1048576ull));
    }
    if (y->streamOk && y->streamUs) {
        strmMb = (u32)(((u64)y->bytes * 10000000ull) /
                       ((u64)y->streamUs * 1048576ull));
    }

    snprintf(buf, sizeof(buf), "Yaz0 mem%lu.%lu str%lu.%lu MB/s %s",
             (unsigned long)(memMb / 10), (unsigned long)(memMb % 10),
             (unsigned long)(strmMb / 10), (unsigned long)(strmMb % 10),
             (y->ok && y->streamOk) ? "ok" : "FAIL");
    line(6, buf);
}

static void printFs(const NdFsTest *fs)
{
    char buf[96];
    u32 readMb;
    u32 decodeMb;

    if (!fs->fatOk) {
        line(7, "FS : no SD card mounted");
        return;
    }

    if (fs->passed != fs->checks) {
        snprintf(buf, sizeof(buf), "FS %lu/%lu BAD #%lu h%lu r%lu %04lX",
                 (unsigned long)fs->passed, (unsigned long)fs->checks,
                 (unsigned long)fs->failAt, (unsigned long)fs->hostStage,
                 (unsigned long)fs->hostBytes,
                 (unsigned long)(fs->hostHash >> 16));
        line(7, buf);
        return;
    }

    /* Both rates are what the room loader will live or die by: the compressed
     * bytes off the card (over a 32-pass sweep, not one short read), and the
     * bytes the decoder can publish per second end to end. */
    readMb = fs->bulkUs ? (u32)(((u64)fs->bulkBytes * 10000000ull) /
                                ((u64)fs->bulkUs * 1048576ull)) : 0;
    decodeMb = fs->decodeUs ? (u32)(((u64)fs->unpackedBytes * 10000000ull) /
                                    ((u64)fs->decodeUs * 1048576ull)) : 0;

    /* The star means an asset the host put on the card was consumed too. */
    snprintf(buf, sizeof(buf), "FS %lu/%lu%s sd%lu.%lu/dec%lu.%lu MB/s",
             (unsigned long)fs->passed, (unsigned long)fs->checks,
             fs->hostFound ? " *" : "",
             (unsigned long)(readMb / 10), (unsigned long)(readMb % 10),
             (unsigned long)(decodeMb / 10), (unsigned long)(decodeMb % 10));
    line(7, buf);
}

static void printArena(const GameMem *m, const GameMemTest *t)
{
    char buf[64];
    u32 tenthsMb;

    if (!t->ok) {
        snprintf(buf, sizeof(buf), "Arena FAIL %luKB free %luKB",
                 (unsigned long)(t->size / 1024u),
                 (unsigned long)(m->heapLeft / 1024u));
        line(15, buf);
        return;
    }

    tenthsMb = (u32)(((u64)t->size * 10u) / 1048576u);
    snprintf(buf, sizeof(buf), "Ar %lu.%luMB %luus pk%luKB h%luKB",
             (unsigned long)(tenthsMb / 10),
             (unsigned long)(tenthsMb % 10),
             (unsigned long)t->us,
             (unsigned long)(m->peak / 1024u),
             (unsigned long)(m->heapLeft / 1024u));
    line(15, buf);
}

/* Row 16 is the first real Ocarina of Time code running here: the game's own
 * arena allocator, its display-list bump pointer, its fixed-point sine table
 * and its float math. t is Math_FAtan2F(1,1) times a million, which is 785398
 * for pi/4; n is how far off a millionths guNormalize leaves the length of
 * (3,4,0). The number before "us" is 256 alloc/free pairs of 64 bytes. Once the
 * list the game builds for itself is on screen, e is where its red quad ends
 * and t is how many triangles the geometry engine took from it. When a row
 * fails, the two numbers after the # are the assertion ids. */
static void printOot(const OotSelfTest *t, const OotDl *dl, u32 tris)
{
    char buf[64];

    if (!t->allOk) {
        snprintf(buf, sizeof(buf), "OoT #%u,%u %u/%u", (unsigned)t->failAt,
                 (unsigned)t->fail2, (unsigned)t->passed, (unsigned)t->checks);
        line(16, buf);
        return;
    }

    if (dl->list != NULL) {
        snprintf(buf, sizeof(buf), "OoT %u/%u %luus e%ld t%lu",
                 (unsigned)t->passed, (unsigned)t->checks,
                 (unsigned long)t->churnUs, (long)dl->edge,
                 (unsigned long)tris);
        line(16, buf);
        return;
    }

    snprintf(buf, sizeof(buf), "OoT %u/%u %luus t%u n%u",
             (unsigned)t->passed, (unsigned)t->checks,
             (unsigned long)t->churnUs,
             (unsigned)t->atanMicro, (unsigned)t->normErr);
    line(16, buf);
}

static void printF3(const F3Test *t, bool mtxOk)
{
    char buf[64];

    if (!t->ok) {
        snprintf(buf, sizeof(buf), "F3FAIL s%lX o%lX R%lX B%lX t%lu",
                 (unsigned long)t->stageBad,
                 (unsigned long)t->omBad,
                 (unsigned long)t->realBad,
                 (unsigned long)t->beBad,
                 (unsigned long)t->triCount);
        line(17, buf);
        return;
    }

    snprintf(buf, sizeof(buf), "F3 t%lu mtx %lu/%lu %lu.%01lums %s",
             (unsigned long)t->triCount,
             (unsigned long)t->mtxPush,
             (unsigned long)t->mtxPop,
             (unsigned long)(t->dlUs / 1000u),
             (unsigned long)((t->dlUs / 100u) % 10u),
             mtxOk ? "MtxOK" : "MtxBAD");
    line(17, buf);
}

/* The drain probe runs once at boot and its numbers are recorded in
 * HISTORICO_MOTOR_GRAFICO.md (Fase 5). Row 12, which used to carry them, now
 * carries the collision mesh: the console is 23 rows, and the playable slice
 * has to have a row of its own. The probe itself still runs, so a regression in
 * the texture-drain path would still be visible in `dt` to a build that wants
 * to print it. */

/* The first real room, read off the SD card as the game would DMA it out of the
 * cartridge: one segment, big-endian, header and all.
 *
 * MKTDAY.ROO is market_day_room_0 and BMORI1.ROO is Bmori1_room_0, both straight
 * out of the extracted ntsc-1.2 baserom. The first is a plain NORMAL shape whose
 * lists turn texturing off; the second is a CULLABLE shape with seven entries
 * and real G_SETTIMG/G_SETTILE/G_LOADBLOCK chains pointing into its own segment.
 *
 * BMORI1.SCE is Bmori1_scene, the archive that room belongs to. It is not a
 * second opinion about the room: it is segment 2, the one the room's own chunks
 * address for the CI palette they load, so without it every paletted texture in
 * the room has nothing to look up. */
#define ND_ROOM_PATH "/BMORI1.ROO"
#define ND_SCENE_PATH "/BMORI1.SCE"

/* Bmori1_scene's spawn 0 points at player entry 0, which the file records at
 * (110, 309, 781). ferramentas/analisar_colisao.py reads the same triple out
 * of the same bytes, and ootColSelfTest checks that the collision floor under
 * it agrees with the entry's y, so a wrong constant cannot pass unnoticed. */
#define ND_SPAWN_X 110.0f
#define ND_SPAWN_Y 309.0f
#define ND_SPAWN_Z 781.0f

static OotCol sCol;
static OotColTest sColTest;
static OotPlay sPlay;

/* The walk starts looking toward -z, the side of the room the orbit captures
 * were taken from. The spawn entry's own rotY is 0, which the game reads as
 * facing +z under its (sin, cos) convention, but the marker is not an actor and
 * the camera picks the side the room was already measured from. */
#define ND_CAM_YAW 0x8000

static long sRmMin[3];

/* Where the two N64 archives the room is drawn out of landed in the arena, so a
 * pointer the geometry engine reports can be turned back into an offset in the
 * file it came from. */
static const u8 *sRoomBase;
static const u8 *sSceneBase;
static u32 sRoomBytes;
static u32 sSceneBytes;
static long sRmMax[3];

/* The bounds come out of the triangles the decoder walks, because a room's own
 * lists are the only thing in the file that says how big it is: the bounding box
 * the game uses for room transitions lives in the scene header, which is a
 * different file. The vertices are world coordinates as long as the room loads
 * no modelview of its own, which market_day_room_0 measured as none. */
static void roomTri(void *ctx, const F3Vtx *vtx, u32 a, u32 b, u32 c)
{
    u32 idx[3];
    u32 i;
    u32 k;

    (void)ctx;
    idx[0] = a;
    idx[1] = b;
    idx[2] = c;
    for (i = 0; i < 3; i++) {
        const F3Vtx *v = &vtx[idx[i]];
        s32 p[3];

        p[0] = v->x;
        p[1] = v->y;
        p[2] = v->z;
        for (k = 0; k < 3; k++) {
            if (p[k] < sRmMin[k]) {
                sRmMin[k] = p[k];
            }
            if (p[k] > sRmMax[k]) {
                sRmMax[k] = p[k];
            }
        }
    }
}

/* Runs one list with nothing but the bounds hook attached, and returns how many
 * triangles it took to produce them. */
static u32 roomMeasure(F3State *st, const u8 *list)
{
    u32 k;

    for (k = 0; k < 3; k++) {
        sRmMin[k] = 1000000;
        sRmMax[k] = -1000000;
    }
    f3Reset(st);
    f3SetTriHook(st, roomTri, NULL);
    f3Run(st, list);
    f3SetTriHook(st, NULL, NULL);
    for (k = 0; k < 3; k++) {
        if (sRmMax[k] < sRmMin[k]) {
            sRmMin[k] = 0;
            sRmMax[k] = 0;
        }
    }
    return st->triCount;
}

/* Reads one whole N64 archive into the arena, the way the game would DMA it out
 * of the cartridge. Returns false when the file is not on the card at all. */
static bool segRead(GameMem *arena, const char *path, const u8 **out, u32 *sizeOut)
{
    NdFile f;
    u8 *data;

    if (!ndFsOpen(&f, path)) {
        return false;
    }
    data = (u8 *)gameMemAlloc(arena, f.size, 16u);
    if (!data || !ndFsReadAt(&f, 0, data, f.size)) {
        ndFsClose(&f);
        return false;
    }
    ndFsClose(&f);
    *out = data;
    *sizeOut = f.size;
    return true;
}

static bool roomLoad(OotRoom *room, F3State *st, GeDraw *g, GameMem *arena,
                     u32 *bytesOut, u32 *trisOut)
{
    const u8 *data;
    const u8 *scene;
    u32 size;

    /* The scene first, and only as far as it goes: it is segment 2, which the
     * room's own chunks reach through for the palette of every textured tile.
     * Without it the room still draws, unpaletted. */
    if (segRead(arena, ND_SCENE_PATH, &scene, &size)) {
        f3SetSegment(st, OOT_SCENE_SEG, scene);
        f3SetSegmentBe(st, OOT_SCENE_SEG, true);
        sSceneBase = scene;
        sSceneBytes = size;
    }
    if (!segRead(arena, ND_ROOM_PATH, &data, &size)) {
        return false;
    }
    sRoomBase = data;
    sRoomBytes = size;

    /* The room is the N64's bytes and this file does not rewrite them: the
     * decoder swaps as it reads, per segment. */
    f3SetSegment(st, OOT_ROOM_SEG, data);
    f3SetSegmentBe(st, OOT_ROOM_SEG, true);
    ootRoomOpen(room, data, size);
    if (room->stage != 0 || room->rootOpa == NULL) {
        return false;
    }

    /* Two walks. The first reads the room's own first opaque list, because the
     * prologue that carries every entry does not exist until a bound has been
     * built it a camera around. The second reads that prologue, and its bound is
     * the one the room is finally drawn with. */
    *trisOut = roomMeasure(st, (const u8 *)room->rootOpa);
    ootRoomBuild(room, sRmMin, sRmMax, 0);
    f3SetSegment(st, OOT_ROOM_PRO_SEG, room->blob);
    *trisOut = roomMeasure(st, (const u8 *)room->list);
    ootRoomBuild(room, sRmMin, sRmMax, 0);

    g->shift = (u32)room->shift;
    *bytesOut = size;
    return room->list != NULL;
}

/* Row 10, held permanently for as long as the room is loaded.
 *
 * In the orbit build it is the loader's own fit and is the line to read against
 * a measurement taken off the file by hand: shift 9 and a distance of 2314 are
 * what the +-1000 by -20..320 box that market_day_room_0 actually has needs in
 * order to stay under the geometry engine's ceiling. It also repeats that box
 * and the near plane, which is the one number that turned a rendered room into
 * a black screen when the camera put the whole mesh behind it.
 *
 * The walk owns the camera now, so the same row describes whichever frustum and
 * exponent the frame is actually drawn with instead: OOT_PLAY_SHIFT and the two
 * planes ootRoomWalkView is called with. The orbit fit still lives in the struct
 * and in HISTORICO_MOTOR_GRAFICO.md; printing it while the walk is drawing would
 * be a number the frame does not use.
 *
 * The triangle count is not duplicated here; it is the t field of the DL row.
 *
 * Pressing A runs the SD benchmark, which overwrites row 10 for the rest of the
 * run. */
static void printRoom(const OotRoom *room, u32 tris, u32 bytes, bool ok,
                      bool walk)
{
    char buf[64];

    if (!ok) {
        snprintf(buf, sizeof(buf), "ROOM none st%lu e%lu t%lu",
                 (unsigned long)room->stage,
                 (unsigned long)room->numEntries, (unsigned long)tris);
        line(10, buf);
        return;
    }

    if (walk) {
        snprintf(buf, sizeof(buf), "RM w e%lu sh%lu n%ld f%ld %lub",
                 (unsigned long)room->entriesUsed,
                 (unsigned long)OOT_PLAY_SHIFT,
                 (long)OOT_PLAY_NEAR, (long)OOT_PLAY_FAR,
                 (unsigned long)bytes);
        line(10, buf);
        return;
    }

    snprintf(buf, sizeof(buf), "RM st%lu e%lu sh%lu d%lu %lub",
             (unsigned long)room->stage,
             (unsigned long)room->entriesUsed,
             (unsigned long)room->shift,
             (unsigned long)room->dist,
             (unsigned long)bytes);
    line(10, buf);
}

/* Row 11, the walk: where the collision layer accepts the marker, how fast the
 * speed curve says it is moving, and how many ticks moved or were stopped.
 *
 * g/f is the floor state, and the trailing a says the numbers come from the
 * scripted probe, i.e. nobody has touched the panel yet. m counts ticks that
 * moved the player horizontally, b ticks a wall stopped. s is in world units
 * per 20 Hz update, so 6.0 is the R_RUN_SPEED_LIMIT cap, not a pixel speed. */
static void printPlayer(const OotPlay *p, bool walkOk, bool autoPilot)
{
    char buf[64];
    long sp;

    if (!walkOk) {
        line(11, "PL -- no collision");
        return;
    }

    sp = (long)(p->speed * 10.0f + 0.5f);
    snprintf(buf, sizeof(buf), "PL %ld,%ld y%ld s%ld.%ld m%lu b%lu%c%s",
             (long)p->pos[0], (long)p->pos[2], (long)p->pos[1],
             sp / 10, sp % 10,
             p->moves, p->blocked, p->onFloor ? 'g' : 'f',
             autoPilot ? "a" : "");
    line(11, buf);
}

/* Row 23, the last row of the console, is the scene's collision mesh: the
 * counts the loader classified and whether the mesh agrees with the spawn.
 * Bmori1_scene reads as 3123 vertices, 3843 polygons, 797 floors, 2696 walls
 * and 350 ceilings, and the floor under (110, 781) is the polygon at 309.0,
 * which is also the y of the player entry the spawn points at. */
static void printCol(const OotCol *col, const OotColTest *t)
{
    char buf[64];

    if (col->stage != 0 || t->passed != t->checks) {
        snprintf(buf, sizeof(buf), "Col st%lu %lu/%lu #%lu",
                 (unsigned long)col->stage,
                 (unsigned long)t->passed, (unsigned long)t->checks,
                 (unsigned long)t->failAt);
    } else {
        snprintf(buf, sizeof(buf), "Col v%lu p%lu f%lu w%lu c%lu",
                 (unsigned long)col->numVtx,
                 (unsigned long)col->numPoly,
                 (unsigned long)col->numFloor,
                 (unsigned long)col->numWall,
                 (unsigned long)col->numCeil);
    }
    line(12, buf);
}

int main(void)
{
    HwProbe hw;
    Perf perf;
    Sched sched;
    GameMem arena;
    GameMemTest arenaTest;
    F3Test f3;
    F3State f3draw;
    GeDraw gdraw;
    GeDrainTest dt;
    SdBench sd;
    Yaz0Test yaz0;
    NdFsTest fs;
    NdInputState ndin;
    NdInputTest ndinTest;
    OotSelfTest oot;
    OotDl odl;
    OotRoom room = { 0 };
    u32 ootTris = 0;
    u32 roomTris = 0;
    u32 roomBytes = 0;
    bool roomOk;
    bool walkOk;
    bool ndinOk;
    bool sdDone = false;
    bool mtxOk;
    u32 quads = 1024;
    u16 angle = ROOM_ANGLE;
    int selected = 4;
    u32 lastFps = 0;
    u16 showBtn;
    int showSx;
    int showSy;
    int showRx;
    int showRy;
    char buf[64];

    timeInit();
    gameMemInit(&arena);
    probeHardware(&hw, arena.base, arena.size);
    yaz0SelfTest(&yaz0);
    ndFsSelfTest(&fs);
    gameMemSelfTest(&arena, &arenaTest);
    ootSelfTest(&oot);
    f3SelfTest(&f3);
    ndInputInit(&ndin);
    ndinOk = ndInputSelfTest(&ndinTest);
    bottomInit();
    topInit();

    f3Init(&f3draw);
    f3SetSegment(&f3draw, 6, dlDemoList());
    /* One world unit is 2^(12-shift) of the geometry engine's 4.12 clip units,
     * so shift is the only thing standing between the game's coordinates and
     * the engine's +-8 ceiling on both matrix multiplies. At 6 a vertex at
     * z = -500 lands on -7.81 and the projection's own z term reaches 11.7
     * before the divide; at 8 the same walk stays inside 3.9. The demo scene is
     * pixel-identical either way, so the headroom is free. */
    geDrawInit(&gdraw, 8);
    dlDemoInit();
    mtxOk = geMtxSelfTest();
    geDrainProbe(&gdraw, &f3draw, dlDemoList(), &dt);

    /* Segment 7 is the blob the game's own Gfx_Alloc cursor carved its list out
     * of; the demo list stays in segment 6. */
    ootDlBuild(&odl, angle);
    f3SetSegment(&f3draw, OOT_DL_SEG, odl.blob);

    /* The room is read after the drain probe, because that probe redraws the
     * demo list through a shrunk texture cache and resets the state it shares
     * with the room's segments. */
    roomOk = roomLoad(&room, &f3draw, &gdraw, &arena, &roomBytes, &roomTris);

    /* The collision mesh lives in the scene archive, so it is opened right
     * after the room loader has put that archive in the arena. Bmori1's scene
     * is the only one this build knows, and its player entry 0 is where the
     * walk will start. */
    if (sSceneBase != NULL) {
        ootColOpen(&sCol, sSceneBase, sSceneBytes);
        ootColSelfTest(&sCol, ND_SPAWN_X, ND_SPAWN_Y, ND_SPAWN_Z, &sColTest);
    }

    /* The walk needs three things the boot has already proved: the room drew,
     * the collision mesh parsed, and its own self test agrees with the spawn
     * entry. Anything less and the frame falls back to the orbit, which needs
     * none of them. The marker list is built from segment 4's own blob, so it
     * is registered with the decoder here and never rebuilt: the walk rewrites
     * its two matrices every frame instead. */
    walkOk = roomOk && sCol.stage == 0 && sColTest.passed == sColTest.checks;
    if (walkOk) {
        const float spawn[3] = { ND_SPAWN_X, ND_SPAWN_Y, ND_SPAWN_Z };

        ootPlayInit(&sPlay, spawn, ND_CAM_YAW);
        walkOk = sPlay.ok != 0;
        f3SetSegment(&f3draw, OOT_PLAY_SEG, sPlay.blob);
    }

    memset(&sd, 0, sizeof(sd));
    printProbe(&hw);
    printInput(&ndinTest, ndinOk);
    printYaz0(&yaz0);
    printFs(&fs);
    printArena(&arena, &arenaTest);
    printOot(&oot, &odl, 0u);
    printF3(&f3, mtxOk);
    printRoom(&room, roomTris, roomBytes, roomOk, walkOk);
    printPlayer(&sPlay, walkOk, !ndin.everLive);
    printCol(&sCol, &sColTest);
    bottomMoveCursor(selected);
    bottomInputShow(ndinTest.probeStickX, ndinTest.probeStickY, ndinTest.probeButtons);
    oamUpdate(&oamSub);

    perfInit(&perf);
    schedInit(&sched);

    while (pmMainLoop()) {
        u32 loopStart = timeTicks();
        u32 renderStart;
        u32 renderUs;
        u32 waitStart;
        u32 waitUs = 0;
        u32 tickStart;
        u32 tickUs = 0;
        u32 hudStart;
        u32 inUs = 0;
        u32 hudUs = 0;
        u32 ticks;
        u32 down;
        u16 pressed;
        NdZone zone;

        ndInputUpdate(&ndin);
        down = keysDown();
        pressed = ndin.in.press.button;
        zone = ndInputZone(ndin.touch.px, ndin.touch.py);

        if ((down & KEY_LEFT) && quads >= 64) {
            quads -= 64;
        }
        if ((down & KEY_RIGHT) && quads < 1024) {
            quads += 64;
        }
        if ((pressed & BTN_L) && quads >= 64) {
            quads -= 64;
        }
        if ((pressed & BTN_R) && quads < 1024) {
            quads += 64;
        }

        if (pressed & BTN_A) {
            line(10, "SD benchmark running...");
            sdBenchmark(&sd, 1u << 20);
            sdDone = true;
        }

        renderStart = timeTicks();
        inUs = timeTicksToUs(renderStart - loopStart);
        /* The grid stays as the backdrop only while there is nothing to put in
         * front of it: it covers the whole panel, and a room that has just been
         * framed to fill that panel would trade places with it on depth alone. */
        topRender(roomOk ? 0u : quads, angle);
        glLoadIdentity();
        ootDlBuild(&odl, angle);
        if (walkOk) {
            /* The room keeps being drawn by its own lists; what changes is the
             * two matrices its prologue loads, which ootRoomWalkView rewrites
             * in place. The marker carries its own projection and modelview,
             * because geDrawList starts every list from an identity pair. The
             * shift is a frame-wide exponent, so it has to be told to the
             * drawer, not left at the orbit fit's 9: one world unit is four
             * engine units at 10, and the same conversion applies to the
             * marker's own translation. */
            ootPlayBuild(&sPlay, &sCol);
            gdraw.shift = OOT_PLAY_SHIFT;
            ootRoomWalkView(&room, sPlay.eye, sPlay.at, OOT_PLAY_SHIFT,
                            OOT_PLAY_NEAR, OOT_PLAY_FAR);
            /* The marker first, then the room. geDrawList zeroes its counters
             * per list, so drawing the room last leaves row 18 describing the
             * 646-triangle list rather than the 36-triangle marker; the total
             * is summed here instead of read back. The marker is opaque, so the
             * depth test does not depend on which went first. */
            ootTris = 0;
            if (sPlay.ok) {
                geDrawList(&gdraw, &f3draw, (const u8 *)sPlay.list);
                ootTris = gdraw.tris;
            }
            geDrawList(&gdraw, &f3draw, (const u8 *)room.list);
            ootTris += gdraw.tris;
        } else if (roomOk) {
            /* The camera is the game's own matrix stack again, once per frame,
             * so the orbit is a number the sine table produced and not one the
             * port wrote down. */
            ootRoomBuild(&room, room.min, room.max, angle);
            gdraw.shift = (u32)room.shift;
            geDrawList(&gdraw, &f3draw, (const u8 *)room.list);
            ootTris = gdraw.tris;
        } else {
            geDrawList(&gdraw, &f3draw, dlDemoList());
            /* Last, because the demo's grid covers the whole panel and this list's
             * viewport covers only its top 24 rows. The grid is a backdrop and has
             * to be behind this list in depth as well as in submission order: while
             * its corners sat at the ortho projection's mid depth its pixels won the
             * geometry engine's test against these quads' clip z / w of +0.5 and the
             * whole list disappeared without a trace. */
            geDrawList(&gdraw, &f3draw, (const u8 *)odl.list);
            ootTris = gdraw.tris;
        }
        glFlush(0);
        renderUs = timeTicksToUs(timeTicks() - renderStart);
        waitStart = timeTicks();

        swiWaitForVBlank();
        oamUpdate(&oamSub);

        ticks = schedFrame(&sched);
        waitUs = timeTicksToUs(timeTicks() - waitStart);
        tickStart = timeTicks();
        while (ticks--) {
            if (walkOk) {
                int sx = 0;
                int sy = 0;
                int turn = 0;
                unsigned long t = sPlay.ticks;

                /* Nobody can press a button from the capture script, so until a
                 * finger lands the walk drives itself. The phases run off the
                 * tick counter rather than the wall clock, so two runs of the
                 * same build end in the same place:
                 *   40..79   stick up   - forward along the camera, past the
                 *                         speed curve's knee into its cap
                 *   100..107 C-right    - eight ticks of -0x800, a quarter
                 *                         turn; the camera then looks along +x
                 *   120..139 stick up   - the same push in the new camera
                 *                         frame, so a move along +x is what
                 *                         proves the stick is camera-relative
                 *   150..159 stick left - a strafe across the view, which
                 *                         turns the marker side-on and makes its
                 *                         nose readable in a capture
                 *   165..184 stick left - the same strafe kept up for twenty
                 *                         ticks, which walks into the scene
                 *                         wall at z=358 (poly 253, x 215..251,
                 *                         read out of the scene dump) and stops
                 *                         at the 12-unit radius; the blocked
                 *                         ticks are row 11's b
                 *   190..199 left-up    - an oblique push at that wall: the
                 *                         blocked part is lost and the rest of
                 *                         the step runs along the face, which
                 *                         is the slide, visible as x falling
                 *                         while z stays on the wall
                 * After 200 the sticks rest and the HUD keeps the end state. */
                if (ndin.everLive) {
                    /* OoT reads the stick raw, not dead-zoned, and the speed
                     * curve returns zero below magnitude 20; the live path
                     * passes it through unchanged. */
                    sx = ndin.in.cur.stick_x;
                    sy = ndin.in.cur.stick_y;
                    /* Forward is (sin, cos), so at 0x8000 (looking -z) a larger
                     * yaw swings the view toward -x, the left of the screen. */
                    if (ndin.in.cur.button & BTN_CLEFT) {
                        turn += 0x800;
                    }
                    if (ndin.in.cur.button & BTN_CRIGHT) {
                        turn -= 0x800;
                    }
                } else if (t >= 40 && t < 80) {
                    sy = 60;
                } else if (t >= 100 && t < 108) {
                    turn = -0x800;
                } else if (t >= 120 && t < 140) {
                    sy = 60;
                } else if (t >= 150 && t < 160) {
                    sx = -60;
                } else if (t >= 165 && t < 185) {
                    sx = -60;
                } else if (t >= 190 && t < 200) {
                    sx = -60;
                    sy = 60;
                }
                ootPlayTick(&sPlay, &sCol, sx, sy, turn);
            } else if (ndin.in.rel.stick_x) {
                /* Orbit fallback: rel is already dead-zoned by the input layer,
                 * so a resting stick leaves the view alone.
                 *
                 * The auto spin is behind a switch because it made the panel
                 * grain unmeasurable: grain is a function of the camera, the
                 * frame a capture lands on is a function of the frame rate, and
                 * the frame rate is the thing being measured. Two builds of the
                 * same room disagreed 18.75 to 24.64 for no reason but the
                 * angle. */
                angle = (u16)(angle + ndin.in.rel.stick_x * 8);
            }
#if ROOM_SPIN
            else {
                angle += 546;
            }
#endif

            if (ndin.touch.down && zone == ND_ZONE_NONE) {
                int pick = bottomPick(ndin.touch.px, ndin.touch.py);
                if (pick >= 0 && pick != selected) {
                    selected = pick;
                    bottomMoveCursor(selected);
                }
            }
        }
        tickUs = timeTicksToUs(timeTicks() - tickStart);
        hudStart = timeTicks();

        /* Until a finger lands, the overlay and the HUD both show the scripted
         * deflection from the self test, so a screenshot proves the mapping. */
        if (ndin.everLive) {
            showBtn = ndin.in.cur.button;
            showSx = ndin.in.cur.stick_x;
            showSy = ndin.in.cur.stick_y;
            showRx = ndin.in.rel.stick_x;
            showRy = ndin.in.rel.stick_y;
        } else {
            showBtn = ndinTest.probeButtons;
            showSx = ndinTest.probeStickX;
            showSy = ndinTest.probeStickY;
            showRx = ndinTest.probeRelX;
            showRy = ndinTest.probeRelY;
        }
        bottomInputShow(showSx, showSy, (u16)(showBtn & (BTN_CUP | BTN_CDOWN |
                                                        BTN_CLEFT | BTN_CRIGHT)));

        if (perfConsume(&perf.textAccumUs, 4)) {
            lastFps = perf.fps;

            snprintf(buf, sizeof(buf),
                     "FPS %lu f%lu.%lu s%lu.%lu w%lu ms",
                     (unsigned long)lastFps,
                     (unsigned long)(perf.frameUs / 1000),
                     (unsigned long)((perf.frameUs / 100) % 10),
                     (unsigned long)(renderUs / 1000),
                     (unsigned long)((renderUs / 100) % 10),
                     (unsigned long)(perf.worstUs / 1000));
            line(8, buf);

            /* Row 15 splits what row 8's f counts, in milliseconds of the same
             * loop iteration. In is the input snapshot and the key handling
             * before rendering starts, Tk the whole 20 Hz batch (both collision
             * queries per tick included), Wt the VBlank wait plus the two OAM
             * updates and schedFrame, Hd the input overlay and the HUD text up
             * to the sample. Row 8's s is inside In..Hd, not extra to it; what
             * does not show up here is the tail of glFlush and perfEnd. */
            snprintf(buf, sizeof(buf), "In%lu Tk%lu Wt%lu Hd%lu ms",
                     (unsigned long)(inUs / 1000),
                     (unsigned long)(tickUs / 1000),
                     (unsigned long)(waitUs / 1000),
                     (unsigned long)(hudUs / 1000));
            line(15, buf);

            if (roomOk) {
                /* The demo's quad count says nothing while a room is on screen,
                 * so row 9 carries the first texture the engine uploaded: the
                 * image and the palette it was baked with, as offsets in the
                 * file each came out of. Measured offline, the room's 32x32
                 * images with scene+0x14288 are bark and with scene+0x14080 are
                 * noise, and the screen matches neither, so which of the three
                 * the upload used is the question this row answers. */
                const u8 *bi = gdraw.bindImg;
                const u8 *bp = gdraw.bindTlut;
                char src = '?';
                char pal = '?';
                u32 off = 0;
                u32 poff = 0;

                if (bi && bi >= sSceneBase && bi < sSceneBase + sSceneBytes) {
                    src = 'S';
                    off = (u32)(bi - sSceneBase);
                } else if (bi && bi >= sRoomBase && bi < sRoomBase + sRoomBytes) {
                    src = 'R';
                    off = (u32)(bi - sRoomBase);
                }
                if (bp && bp >= sSceneBase && bp < sSceneBase + sSceneBytes) {
                    pal = 'S';
                    poff = (u32)(bp - sSceneBase);
                } else if (bp && bp >= sRoomBase && bp < sRoomBase + sRoomBytes) {
                    pal = 'R';
                    poff = (u32)(bp - sRoomBase);
                }
                snprintf(buf, sizeof(buf), "B1 i%c:%lX p%c:%lX f%lu s%lu b%lu%lu",
                         src, (unsigned long)off,
                         pal, (unsigned long)poff,
                         (unsigned long)gdraw.bindFmt,
                         (unsigned long)gdraw.bindSiz,
                         (unsigned long)gdraw.bindBe,
                         (unsigned long)gdraw.bindPalBe);
                line(9, buf);
            } else {
                snprintf(buf, sizeof(buf), "Q%lu (%lu t) T%u,%u z%u i%d",
                         (unsigned long)quads, (unsigned long)(quads * 2),
                         (unsigned)ndin.touch.px, (unsigned)ndin.touch.py,
                         (unsigned)zone, selected);
                line(9, buf);
            }

            if (sdDone) {
                snprintf(buf, sizeof(buf), "SD w %lu KB/s r %lu KB/s %s",
                         (unsigned long)sd.writeKBs,
                         (unsigned long)sd.readKBs,
                         sd.dataVerified ? "ok" : "BAD");
                line(10, buf);
            }

            snprintf(buf, sizeof(buf), "B%04X p%04X S%+d,%+d R%+d,%+d %c",
                     (unsigned)showBtn,
                     (unsigned)pressed,
                     showSx, showSy, showRx, showRy,
                     ndin.everLive ? 'l' : 'g');
            line(13, buf);

            snprintf(buf, sizeof(buf), "Logic %lu Hz  ticks %lu",
                     (unsigned long)sched.tickRate,
                     (unsigned long)sched.ticks);
            line(14, buf);

            printOot(&oot, &odl, ootTris);
            printPlayer(&sPlay, walkOk, !ndin.everLive);

            snprintf(buf, sizeof(buf), "DL t%lu c%lu k%lu p%lu m%lu/%lu x%lu %s",
                     (unsigned long)ootTris,
                     (unsigned long)gdraw.clamped,
                     (unsigned long)gdraw.clipTris,
                     (unsigned long)gdraw.projLoads,
                     (unsigned long)gdraw.mtxLoads,
                     (unsigned long)gdraw.mtxPops,
                     (unsigned long)gdraw.mtxLeft,
                     gdraw.ok ? "ok" : "FAIL");
            line(18, buf);

            snprintf(buf, sizeof(buf), "Tex b%lu h%lu x%lu e%lu t%lu/%lu%s",
                     (unsigned long)gdraw.texBinds,
                     (unsigned long)gdraw.texHits,
                     (unsigned long)gdraw.texSkip,
                     (unsigned long)gdraw.texEvict,
                     (unsigned long)gdraw.texTris,
                     (unsigned long)gdraw.palTris,
                     gdraw.texBound ? " on" : " off");
            line(19, buf);

            /* s = textured triangles multiplied by the vertex colour, l = the
             * ones where G_LIGHTING means the RSP would still have to light
             * them (not implemented, drawn as authored), o = a cycle-0 combiner
             * this stage does not model, p = the two-image lerp's blend factor
             * (polygon alpha, 0-31) x how many second passes were drawn. The
             * mode letter is L while that lerp is the current combiner and !
             * if any second pass was refused a texture. */
            {
                char cc = gdraw.ccMode < 5 ? "MDSOL"[gdraw.ccMode] : '?';

                if (gdraw.lerpFail != 0u) {
                    cc = '!';
                }
                snprintf(buf, sizeof(buf),
                         "CC %c a%lu n%lu v%lu s%lu l%lu o%lu p%lux%lu", cc,
                         (unsigned long)gdraw.ccAlpha,
                         (unsigned long)gdraw.ccChanges,
                         (unsigned long)gdraw.vpLoads,
                         (unsigned long)gdraw.shadeTris,
                         (unsigned long)gdraw.litTris,
                         (unsigned long)gdraw.otherTris,
                         (unsigned long)gdraw.lerpEnv,
                         (unsigned long)gdraw.lerpTris);
            }
            line(20, buf);

            u32 at = topAlphaTest();

            snprintf(buf, sizeof(buf), "Cut %lu/%lu at%u p%lu",
                     (unsigned long)gdraw.cutTris,
                     (unsigned long)gdraw.texTris,
                     (at & 0x80000000u) ? 1u : 0u,
                     (unsigned long)gdraw.palBinds);
            line(13, buf);

            snprintf(buf, sizeof(buf), "Bl %lu/%lu a%lu n%lu f%lX p%lu %s",
                     (unsigned long)gdraw.xluTris,
                     (unsigned long)gdraw.tris,
                     (unsigned long)gdraw.xluAlpha,
                     (unsigned long)gdraw.omChanges,
                     (unsigned long)gdraw.omFlags,
                     (unsigned long)gdraw.polyChanges,
                     topBlendEnabled() ? "on" : "off");
            line(21, buf);

#ifdef GE_LOD_PROBE
            /* Textured triangles by the level they were drawn with, then the
             * worst face's 16 * (texels per pixel) squared: 16 is one texel per
             * pixel, 64 two, 256 four. Over the blending row, which says nothing
             * new while the same twenty triangles are the only ones that blend. */
            snprintf(buf, sizeof(buf), "Lo %lu/%lu/%lu/%lu r%lu",
                     (unsigned long)gdraw.lodN[0],
                     (unsigned long)gdraw.lodN[1],
                     (unsigned long)gdraw.lodN[2],
                     (unsigned long)gdraw.lodN[3],
                     (unsigned long)gdraw.lodRworst);
            line(21, buf);
#endif

            /* Row 22 is the last row the console can write, so the MIP readout
             * shares it with the texel grain instead of taking one of its own.
             * d = mean |luminance step| between neighbouring texels of what was
             * uploaded; m = triangles drawn above level 0 / deepest level
             * reached / levels VRAM would not give; r = texture rebinds, which is
             * the FIFO cost the chain buys; e = texture slots recycled. */
            snprintf(buf, sizeof(buf), "Bk d%lu.%lu m%lu/%lu/%lu r%lu e%lu",
                     (unsigned long)(gdraw.bakePairs ? gdraw.bakeDl / gdraw.bakePairs : 0),
                     (unsigned long)(gdraw.bakePairs ?
                         (gdraw.bakeDl * 10u / gdraw.bakePairs) % 10u : 0),
                     (unsigned long)gdraw.mipTris,
                     (unsigned long)gdraw.mipMax,
                     (unsigned long)gdraw.mipFail,
                     (unsigned long)gdraw.texRebinds,
                     (unsigned long)gdraw.texEvict);
            line(22, buf);

#ifdef GE_LOD_PROBE
            /* The faces the level test could not finish: gp are the ones whose
             * projected area came out zero, gt the ones whose texture area did,
             * c are the ones the 32-bit corner clamp actually moved and e are
             * the ones where the eye-space selector and the shoelace disagree,
             * with the largest disagreement after the slash. The last row ends
             * under the colour swatches, so the counters share it instead of
             * running the line off the console. The worst face's two areas stay
             * in the struct, off this line, for a build that wants them. */
            snprintf(buf, sizeof(buf), "Lo gp%lu gt%lu c%lu e%lu/%lu",
                     (unsigned long)gdraw.lodGp,
                     (unsigned long)gdraw.lodGt,
                     (unsigned long)gdraw.lodClamp,
                     (unsigned long)gdraw.lodEyeDiv,
                     (unsigned long)gdraw.lodEyeMax);
            line(22, buf);
#endif
        }

        hudUs = timeTicksToUs(timeTicks() - hudStart);
        perfEnd(&perf);

        if (pressed & BTN_START) {
            break;
        }
    }

    return 0;
}
