#ifndef OCARINA_NDS_VIDEO_TOP_H
#define OCARINA_NDS_VIDEO_TOP_H

#include <nds.h>
#include <stdbool.h>

void topInit(void);
void topRender(u32 quads, u16 angle);

/* Whether the GE alpha test is on, plus its threshold in the low bits:
 * bit 31 = enabled, bits 0-4 = threshold. */
u32 topAlphaTest(void);

/* Whether DISP3DCNT has blending enabled for the whole frame. The per-material
 * part is the polygon alpha, which is a FIFO write and lives in GeDraw. */
bool topBlendEnabled(void);

#endif
