#ifndef OCARINA_NDS_UI_BOTTOM_H
#define OCARINA_NDS_UI_BOTTOM_H

#include <nds.h>

#define BOTTOM_ITEM_COUNT 9

/* Touch-zone overlay for the input layer: the virtual stick and the C-pad are
 * drawn so a screenshot shows what the mapping produced. Stick components are
 * in N64 units (-67..67); cButtons uses the OoT BTN_C* bits. */
#define BOTTOM_STICK_SPRITE (BOTTOM_ITEM_COUNT + 1)
#define BOTTOM_KNOB_SPRITE  (BOTTOM_ITEM_COUNT + 2)
#define BOTTOM_C_SPRITE     (BOTTOM_ITEM_COUNT + 3)
#define BOTTOM_C_HOT_SPRITE (BOTTOM_ITEM_COUNT + 4)

void bottomInit(void);
void bottomMoveCursor(int index);
int bottomPick(u16 px, u16 py);
void bottomInputInit(void);
void bottomInputShow(int stickX, int stickY, u16 cButtons);

#endif
