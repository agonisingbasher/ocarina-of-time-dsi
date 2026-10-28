#ifndef OCARINA_NDS_NDINPUT_H
#define OCARINA_NDS_NDINPUT_H

#include <nds.h>
#include <stdbool.h>

/*
 * Button bits and their values are copied from game/oot/include/controller.h so
 * the terms line up with the decomp. Keep them identical to that header: a game
 * translation unit that includes both only redefines each macro to itself.
 */
#define BTN_A       0x8000
#define BTN_B       0x4000
#define BTN_Z       0x2000
#define BTN_START   0x1000
#define BTN_DUP     0x0800
#define BTN_DDOWN   0x0400
#define BTN_DLEFT   0x0200
#define BTN_DRIGHT  0x0100
#define BTN_L       0x0020
#define BTN_R       0x0010
#define BTN_CUP     0x0008
#define BTN_CDOWN   0x0004
#define BTN_CLEFT   0x0002
#define BTN_CRIGHT  0x0001

/*
 * game/oot/src/libu64/pad.c:PadUtils_UpdateRelXY treats the stick as a signed
 * byte that saturates at 0x43 and dies at |v| <= 7. The DS has no analog nub,
 * so the touch pad below has to produce numbers in exactly that range.
 */
#define ND_STICK_MAX 67
#define ND_STICK_DEADZONE 7
#define ND_STICK_REL_MAX (ND_STICK_MAX - ND_STICK_DEADZONE)

/* Touch regions on the 256x192 sub screen. The item grid starts at x=208.
 * ND_STICK_R is the radius at which the stick reads full deflection; the zone
 * accepts a finger up to ND_STICK_GRAB away and pins those extra pixels to the
 * rim, so the drawable ring stays small without making edge presses dead. */
#define ND_STICK_CX 56
#define ND_STICK_CY 124
#define ND_STICK_R  36
#define ND_STICK_GRAB 64
#define ND_C_CX     152
#define ND_C_CY     124
#define ND_C_R      32
#define ND_C_DEAD   8

typedef enum {
    ND_ZONE_NONE = 0,
    ND_ZONE_STICK,
    ND_ZONE_C
} NdZone;

/* Layout mirror of OSContPad (game/oot/include/ultra64/controller.h:127).
 * Its fourth byte sits where that struct has `errno`; the name differs only
 * because libc makes `errno` a macro. */
typedef struct {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 err;
} NdPad;

/* Layout mirror of Input (game/oot/include/libu64/pad.h:6), 24 bytes. */
typedef struct {
    NdPad cur;
    NdPad prev;
    NdPad press;
    NdPad rel;
} NdInput;

typedef struct {
    u16 px;
    u16 py;
    bool down;
    bool pressed;
    bool released;
} NdTouch;

typedef struct {
    NdInput in;
    NdTouch touch;
    u32 dsHeld;
    u32 dsDown;
    u16 touchButton;
    s8 touchStickX;
    s8 touchStickY;
    bool touchOwnsStick;
    bool dpadStick;
    bool everLive;
} NdInputState;

typedef struct {
    u16 checks;
    u16 passed;
    u16 failAt;
    bool sizeOk;
    u16 probeButtons;
    s8 probeStickX;
    s8 probeStickY;
    s8 probeRelX;
    s8 probeRelY;
} NdInputTest;

void ndInputInit(NdInputState *st);
void ndInputStep(NdInputState *st, u32 held, u16 px, u16 py, bool touching);
void ndInputUpdate(NdInputState *st);
NdZone ndInputZone(u16 px, u16 py);

/* Runs the mapping table against scripted hardware states, fills `probe` with a
 * touch deflection for the on-screen stick to draw until real input arrives,
 * and returns false if any expectation is not met. */
bool ndInputSelfTest(NdInputTest *out);

#endif
