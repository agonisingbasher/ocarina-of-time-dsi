#ifndef OCARINA_GFX_DL_DEMO_H
#define OCARINA_GFX_DL_DEMO_H

#include <nds.h>

/* A self-contained F3DEX_GBI_2 list in segment 6: a 32x32 RGBA16 texture
 * loaded through SETTIMG/SETTILE/LOADBLOCK/SETTILESIZE and mapped onto the
 * first quad, then an untextured quad under a pushed matrix. The list base
 * is also the segment 6 base. The list ends with a nested G_DL run of
 * twenty-four more images, built by dlDemoInit, which is what pushes it past
 * what the texture cache can hold in one frame. Call dlDemoInit once to fill
 * the textures and build that run. */
const u8 *dlDemoList(void);
void dlDemoInit(void);

#endif
