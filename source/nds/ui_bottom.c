#include "ui_bottom.h"

#include <string.h>

#include "ndinput.h"

#define GRID_COLS   3
#define GRID_ROWS   3
#define CELL_W      16
#define CELL_H      16
#define GRID_X      208
#define GRID_Y      144

#define CURSOR_SPRITE BOTTOM_ITEM_COUNT

#define STICK_BANK 10
#define C_BANK     11
/* How far the knob travels inside its 32x32 ring, in pixels. */
#define KNOB_TRAVEL 12

static u16 *stickKnobTile;
static u16 *cHotTile;

static const u16 itemColors[BOTTOM_ITEM_COUNT] = {
    RGB15(31, 6, 6),
    RGB15(6, 24, 6),
    RGB15(6, 14, 31),
    RGB15(31, 24, 6),
    RGB15(24, 6, 31),
    RGB15(6, 31, 24),
    RGB15(31, 14, 6),
    RGB15(18, 18, 18),
    RGB15(31, 31, 31),
};

static void clearTile(u16 *tile)
{
    memset(tile, 0, 16 * 16 / 2);
}

static void putPixel(u16 *tile, int x, int y, u8 color)
{
    u16 *word = &tile[y * 4 + (x >> 2)];
    int shift = (x & 3) * 4;
    *word = (u16)((*word & ~(0xFu << shift)) | ((u16)(color & 0xF) << shift));
}

static void fillTile(u16 *tile, u8 border, u8 inner)
{
    int x;
    int y;

    clearTile(tile);
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            u8 color = (x == 0 || y == 0 || x == 15 || y == 15) ? border : inner;
            putPixel(tile, x, y, color);
        }
    }
}

static void putPx(u16 *tile, int size, int x, int y, u8 color)
{
    u16 *word = &tile[y * (size / 4) + (x >> 2)];
    int shift = (x & 3) * 4;

    *word = (u16)((*word & ~(0xFu << shift)) | ((u16)(color & 0xF) << shift));
}

/* Ring outline plus a centre tick: the only way to show a radius on the DS,
 * whose largest sprite is 32x32, while the touch zone itself is wider. */
static void drawZoneBase(u16 *tile, int isCross)
{
    const int size = 32;
    const int c = size / 2;
    int x;
    int y;

    memset(tile, 0, size * size / 2);
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            int dx = x - c;
            int dy = y - c;
            u32 d2 = (u32)(dx * dx + dy * dy);
            u8 color = 0;

            if (d2 >= 13u * 13u && d2 <= 15u * 15u) {
                color = 1;
            }
            if (isCross && d2 <= 12u * 12u &&
                ((dx >= -1 && dx <= 1) || (dy >= -1 && dy <= 1))) {
                color = 1;
            }
            if (!isCross && d2 <= 4u) {
                color = 1;
            }
            if (color) {
                putPx(tile, size, x, y, color);
            }
        }
    }
}

static void fillDot(u16 *tile, int size, u8 color)
{
    int x;
    int y;

    memset(tile, 0, (size * size) / 2);
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            int dx = x - size / 2;
            int dy = y - size / 2;

            if ((u32)(dx * dx + dy * dy) <= (u32)(size / 2) * (size / 2)) {
                putPx(tile, size, x, y, color);
            }
        }
    }
}

static void loadPalettes(void)
{
    u16 *pal;
    int i;

    for (i = 1; i <= BOTTOM_ITEM_COUNT; i++) {
        pal = &SPRITE_PALETTE_SUB[i * 16];
        memset(pal, 0, 16 * sizeof(u16));
        pal[0] = 0;
        pal[1] = itemColors[i - 1];
        pal[15] = RGB15(31, 31, 31);
    }

    pal = &SPRITE_PALETTE_SUB[15 * 16];
    memset(pal, 0, 16 * sizeof(u16));
    pal[15] = RGB15(31, 31, 0);

    pal = &SPRITE_PALETTE_SUB[STICK_BANK * 16];
    memset(pal, 0, 16 * sizeof(u16));
    pal[1] = RGB15(10, 26, 26);
    pal[2] = RGB15(26, 31, 10);

    pal = &SPRITE_PALETTE_SUB[C_BANK * 16];
    memset(pal, 0, 16 * sizeof(u16));
    pal[1] = RGB15(24, 10, 26);
    pal[2] = RGB15(31, 6, 31);
}

void bottomInit(void)
{
    int i;

    consoleDemoInit();

    vramSetBankI(VRAM_I_SUB_SPRITE);
    oamInit(&oamSub, SpriteMapping_1D_32, false);

    loadPalettes();

    for (i = 0; i < BOTTOM_ITEM_COUNT; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        u16 *tile = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_16Color);

        if (!tile) {
            continue;
        }

        fillTile(tile, 15, 1);
        oamSet(&oamSub, i,
               GRID_X + col * CELL_W, GRID_Y + row * CELL_H,
               1, i + 1,
               SpriteSize_16x16, SpriteColorFormat_16Color,
               tile, -1, false, false, false, false, false);
    }

    {
        u16 *cursor = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_16Color);
        if (cursor) {
            fillTile(cursor, 15, 0);
            oamSet(&oamSub, CURSOR_SPRITE,
                   GRID_X - 1, GRID_Y - 1,
                   0, 15,
                   SpriteSize_16x16, SpriteColorFormat_16Color,
                   cursor, -1, false, false, false, false, false);
        }
    }

    bottomInputInit();

    oamUpdate(&oamSub);
}

void bottomInputInit(void)
{
    u16 *ring = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_16Color);
    u16 *knob = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_16Color);
    u16 *cross = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_16Color);
    u16 *lit = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_16Color);

    if (!ring || !knob || !cross || !lit) {
        return;
    }
    stickKnobTile = knob;
    cHotTile = lit;

    drawZoneBase(ring, 0);
    drawZoneBase(cross, 1);
    fillDot(knob, 8, 2);
    fillDot(lit, 8, 2);

    oamSet(&oamSub, BOTTOM_STICK_SPRITE, ND_STICK_CX - 16, ND_STICK_CY - 16,
           1, STICK_BANK, SpriteSize_32x32, SpriteColorFormat_16Color,
           ring, -1, false, false, false, false, false);
    oamSet(&oamSub, BOTTOM_C_SPRITE, ND_C_CX - 16, ND_C_CY - 16,
           1, C_BANK, SpriteSize_32x32, SpriteColorFormat_16Color,
           cross, -1, false, false, false, false, false);
    oamSet(&oamSub, BOTTOM_KNOB_SPRITE, ND_STICK_CX - 4, ND_STICK_CY - 4,
           0, STICK_BANK, SpriteSize_8x8, SpriteColorFormat_16Color,
           knob, -1, false, true, false, false, false);
    oamSet(&oamSub, BOTTOM_C_HOT_SPRITE, ND_C_CX - 4, ND_C_CY - 4,
           0, C_BANK, SpriteSize_8x8, SpriteColorFormat_16Color,
           lit, -1, false, true, false, false, false);
}

/* The dot parks at the centre of its ring instead of vanishing when the stick
 * rests, and only appears over an arm of the cross when a C direction is held. */
void bottomInputShow(int stickX, int stickY, u16 cButtons)
{
    int ox = (int)((long)stickX * KNOB_TRAVEL / ND_STICK_MAX);
    int oy = (int)((long)stickY * KNOB_TRAVEL / ND_STICK_MAX);
    int cx = 0;
    int cy = 0;
    bool hot = true;

    if (ox > KNOB_TRAVEL) {
        ox = KNOB_TRAVEL;
    }
    if (ox < -KNOB_TRAVEL) {
        ox = -KNOB_TRAVEL;
    }
    if (oy > KNOB_TRAVEL) {
        oy = KNOB_TRAVEL;
    }
    if (oy < -KNOB_TRAVEL) {
        oy = -KNOB_TRAVEL;
    }

    oamSet(&oamSub, BOTTOM_KNOB_SPRITE, ND_STICK_CX + ox - 4, ND_STICK_CY - oy - 4,
           0, STICK_BANK, SpriteSize_8x8, SpriteColorFormat_16Color,
           stickKnobTile, -1, false, false, false, false, false);

    if (cButtons & BTN_CUP) {
        cy = -KNOB_TRAVEL;
    } else if (cButtons & BTN_CDOWN) {
        cy = KNOB_TRAVEL;
    } else if (cButtons & BTN_CLEFT) {
        cx = -KNOB_TRAVEL;
    } else if (cButtons & BTN_CRIGHT) {
        cx = KNOB_TRAVEL;
    } else {
        hot = false;
    }

    oamSet(&oamSub, BOTTOM_C_HOT_SPRITE, ND_C_CX + cx - 4, ND_C_CY + cy - 4,
           0, C_BANK, SpriteSize_8x8, SpriteColorFormat_16Color,
           cHotTile, -1, false, !hot, false, false, false);
}

void bottomMoveCursor(int index)
{
    int col;
    int row;

    if (index < 0 || index >= BOTTOM_ITEM_COUNT) {
        return;
    }

    col = index % GRID_COLS;
    row = index / GRID_COLS;

    oamSetXY(&oamSub, CURSOR_SPRITE, GRID_X + col * CELL_W - 1, GRID_Y + row * CELL_H - 1);
}

int bottomPick(u16 px, u16 py)
{
    int col;
    int row;
    int relX;
    int relY;

    if (px < GRID_X || py < GRID_Y) {
        return -1;
    }

    relX = (int)px - GRID_X;
    relY = (int)py - GRID_Y;

    if (relX >= GRID_COLS * CELL_W || relY >= GRID_ROWS * CELL_H) {
        return -1;
    }

    col = relX / CELL_W;
    row = relY / CELL_H;
    return row * GRID_COLS + col;
}
