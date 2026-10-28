#include "video_top.h"

#define GRID_SIDE   32
#define GRID_SPAN   (14 * 4096)

/* The grid is a backdrop, so it has to lose the geometry engine's depth test to
 * everything the game list submits. Measured with the game's own quads, which
 * land at clip z / w = +0.5: the grid at z = 0 (depth 0), at +3072 (depth -0.75)
 * and at +4095 (depth -0.9998) all hid the game's list completely, so the
 * engine keeps the *smaller* value, and glOrthof32's own mapping is the
 * inverted one - z_out = -(2z + far + near) / (far - near), which puts its near
 * plane at +1. -3072 is the same three-quarter distance on the side that reads
 * as behind, and it is inside the clip range, unlike the far plane itself. */
#define GRID_Z      (-3072)

static u32 sAlphaThr;

void topInit(void)
{
    videoSetMode(MODE_0_3D);
    glInit();

    glEnable(GL_ANTIALIAS);

    /* Texel alpha is the cutout the GE offers for free: one global test at the
     * lowest threshold, so an A1B5G5R5 texel with alpha 0 drops out and
     * whatever was behind it survives. DISP3DCNT is not FIFO-ordered with
     * geometry, so per-material switching would need a batch close on every
     * G_SETCOMBINE; instead, assets that must stay opaque get their alpha
     * forced at bake time. */
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(1);
    sAlphaThr = 1;

    /* Blending is the other global DISP3DCNT bit, so it also cannot follow the
     * display list per material. Left on for the whole frame, it costs an
     * opaque material nothing: the GE mixes with the polygon alpha written
     * through the FIFO, and 31 of 31 is the source colour unchanged. */
    glEnable(GL_BLEND);

    glClearColor(0, 0, 0, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);

    glViewport(0, 0, 255, 191);

    /* Top screen is 3D only, so both 128 KB banks can hold textures. */
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    /* Palette RAM for the CI textures; bank I feeds the bottom screen. */
    vramSetBankE(VRAM_E_TEX_PALETTE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof32(-inttof32(8), inttof32(8), -inttof32(6), inttof32(6), -inttof32(1), inttof32(1));

    glMatrixMode(GL_MODELVIEW);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
}

void topRender(u32 quads, u16 angle)
{
    s32 step = GRID_SPAN / GRID_SIDE;
    s32 origin = -(GRID_SPAN / 2);
    u32 drawn = 0;
    u32 y;

    /* The F3DEX2 demo list replaces the projection with its own perspective
     * and may move the viewport, so both have to be re-established every
     * frame. */
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof32(-inttof32(8), inttof32(8), -inttof32(6), inttof32(6),
               -inttof32(1), inttof32(1));
    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    glRotateZi((int)angle);

    glBegin(GL_QUADS);
    for (y = 0; y < GRID_SIDE && drawn < quads; y++) {
        u32 x;
        for (x = 0; x < GRID_SIDE && drawn < quads; x++, drawn++) {
            s32 x0 = origin + (s32)(x * step);
            s32 y0 = origin + (s32)(y * step);
            s32 x1 = x0 + step;
            s32 y1 = y0 + step;

            glColor3b((u8)(x * 8), (u8)(y * 8), (u8)(64 + ((x + y) & 63)));
            /* Texturing is always on (it is not FIFO-ordered with geometry);
             * the white 8x8 texture keeps these quads at vertex colour. */
            glTexCoord2t16(8, 8);
            glVertex3v16((v16)x0, (v16)y0, GRID_Z);
            glTexCoord2t16(8, 8);
            glVertex3v16((v16)x1, (v16)y0, GRID_Z);
            glTexCoord2t16(8, 8);
            glVertex3v16((v16)x1, (v16)y1, GRID_Z);
            glTexCoord2t16(8, 8);
            glVertex3v16((v16)x0, (v16)y1, GRID_Z);
        }
    }
    glEnd();
}

u32 topAlphaTest(void)
{
    /* DISP3DCNT reads back, but the alpha-test value register does not: it
     * reads 0 in melonDS even while the cutout is happening. Report what was
     * written. */
    return ((GFX_CONTROL & GL_ALPHA_TEST) ? 0x80000000u : 0u) | sAlphaThr;
}

bool topBlendEnabled(void)
{
    return (GFX_CONTROL & GL_BLEND) != 0;
}
