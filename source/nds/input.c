#include "ndinput.h"

#include <string.h>

/*
 * The DS has no analog nub and only eight usable face buttons, so the sub screen
 * carries what the hardware lacks: a virtual stick on the left, a C-pad on the
 * right. Both are pure functions of the touch point, which lets the self test
 * feed synthetic coordinates instead of a finger.
 */

static const struct {
    u32 key;
    u16 btn;
} keyMap[] = {
    { KEY_A,     BTN_A },
    { KEY_B,     BTN_B },
    { KEY_X,     BTN_Z },
    { KEY_START, BTN_START },
    { KEY_UP,    BTN_DUP },
    { KEY_DOWN,  BTN_DDOWN },
    { KEY_LEFT,  BTN_DLEFT },
    { KEY_RIGHT, BTN_DRIGHT },
    { KEY_L,     BTN_L },
    { KEY_R,     BTN_R },
};

static u16 keyButtons(u32 held)
{
    u16 out = 0;
    size_t i;

    for (i = 0; i < sizeof(keyMap) / sizeof(keyMap[0]); i++) {
        if (held & keyMap[i].key) {
            out |= keyMap[i].btn;
        }
    }
    return out;
}

static int absInt(int v)
{
    return v < 0 ? -v : v;
}

/* Integer square root: the DS has no FPU and this runs per logic tick. */
static u32 isqrtU32(u32 v)
{
    u32 root = 0;
    u32 bit = 1u << 30;

    while (bit > v) {
        bit >>= 2;
    }
    while (bit) {
        if (v >= root + bit) {
            v -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

NdZone ndInputZone(u16 px, u16 py)
{
    int dx = (int)px - ND_STICK_CX;
    int dy = (int)py - ND_STICK_CY;
    int cdx = (int)px - ND_C_CX;
    int cdy = (int)py - ND_C_CY;

    if (isqrtU32((u32)(dx * dx + dy * dy)) <= ND_STICK_GRAB) {
        return ND_ZONE_STICK;
    }
    if (isqrtU32((u32)(cdx * cdx + cdy * cdy)) <= ND_C_R) {
        return ND_ZONE_C;
    }
    return ND_ZONE_NONE;
}

/* Circle rim -> +-67. Rounding is applied to the magnitude so a full left pull
 * reads exactly the negation of a full right pull. */
static s8 scaleAxis(int v)
{
    int a = absInt(v);

    a = (a * 2 * ND_STICK_MAX + ND_STICK_R) / (2 * ND_STICK_R);
    if (a > ND_STICK_MAX) {
        a = ND_STICK_MAX;
    }
    return (s8)(v < 0 ? -a : a);
}

static void stickFromTouch(u16 px, u16 py, s8 *outX, s8 *outY)
{
    int dx = (int)px - ND_STICK_CX;
    int dy = ND_STICK_CY - (int)py;
    u32 mag = isqrtU32((u32)(dx * dx + dy * dy));

    if (mag > (u32)ND_STICK_R) {
        dx = (int)((long)dx * ND_STICK_R / (long)mag);
        dy = (int)((long)dy * ND_STICK_R / (long)mag);
    }
    *outX = scaleAxis(dx);
    /* The screen grows downwards, the N64 stick grows upwards. */
    *outY = scaleAxis(dy);
}

static u16 cFromTouch(u16 px, u16 py)
{
    int dx = (int)px - ND_C_CX;
    int dy = ND_C_CY - (int)py;

    if (absInt(dx) <= ND_C_DEAD && absInt(dy) <= ND_C_DEAD) {
        return 0;
    }
    if (absInt(dx) > absInt(dy)) {
        return dx > 0 ? BTN_CRIGHT : BTN_CLEFT;
    }
    return dy > 0 ? BTN_CUP : BTN_CDOWN;
}

static s8 relAxis(s8 v)
{
    if (v > ND_STICK_DEADZONE) {
        return (v < ND_STICK_MAX) ? (s8)(v - ND_STICK_DEADZONE)
                                 : (s8)ND_STICK_REL_MAX;
    }
    if (v < -ND_STICK_DEADZONE) {
        return (v > -ND_STICK_MAX) ? (s8)(v + ND_STICK_DEADZONE)
                                   : (s8)-ND_STICK_REL_MAX;
    }
    return 0;
}

void ndInputInit(NdInputState *st)
{
    memset(st, 0, sizeof(*st));
    st->dpadStick = true;
}

void ndInputStep(NdInputState *st, u32 held, u16 px, u16 py, bool touching)
{
    NdInput *in = &st->in;
    u16 buttons = keyButtons(held);
    u16 diff;
    bool wasDown = st->touch.down;

    st->dsHeld = held;
    st->touch.px = px;
    st->touch.py = py;
    st->touch.down = touching;
    st->touch.pressed = touching && !wasDown;
    st->touch.released = !touching && wasDown;
    if (touching || held) {
        st->everLive = true;
    }

    st->touchButton = 0;
    st->touchOwnsStick = false;
    st->touchStickX = 0;
    st->touchStickY = 0;

    if (touching) {
        NdZone zone = ndInputZone(px, py);

        if (zone == ND_ZONE_STICK) {
            st->touchOwnsStick = true;
            stickFromTouch(px, py, &st->touchStickX, &st->touchStickY);
        } else if (zone == ND_ZONE_C) {
            st->touchButton = cFromTouch(px, py);
        }
    }

    in->prev = in->cur;
    in->cur.button = (u16)(buttons | st->touchButton);
    if (st->touchOwnsStick) {
        in->cur.stick_x = st->touchStickX;
        in->cur.stick_y = st->touchStickY;
    } else if (st->dpadStick) {
        in->cur.stick_x = (s8)((buttons & BTN_DRIGHT ? ND_STICK_MAX : 0) -
                               (buttons & BTN_DLEFT ? ND_STICK_MAX : 0));
        in->cur.stick_y = (s8)((buttons & BTN_DUP ? ND_STICK_MAX : 0) -
                               (buttons & BTN_DDOWN ? ND_STICK_MAX : 0));
    } else {
        in->cur.stick_x = 0;
        in->cur.stick_y = 0;
    }
    in->cur.err = 0;

    /* PadMgr_RequestPadData's game path: press is what just went down, rel is
     * what just came up, and the stick delta lands in press. */
    diff = (u16)(in->prev.button ^ in->cur.button);
    in->press.button = (u16)(in->cur.button & diff);
    in->rel.button = (u16)(in->prev.button & diff);
    in->press.stick_x = (s8)(in->cur.stick_x - in->prev.stick_x);
    in->press.stick_y = (s8)(in->cur.stick_y - in->prev.stick_y);
    in->rel.stick_x = relAxis(in->cur.stick_x);
    in->rel.stick_y = relAxis(in->cur.stick_y);
}

void ndInputUpdate(NdInputState *st)
{
    TouchData touch;
    bool touching;

    scanKeys();
    touching = touchRead(&touch);
    ndInputStep(st, keysHeld(), touch.px, touch.py, touching);
}

bool ndInputSelfTest(NdInputTest *out)
{
    static const struct {
        u32 key;
        u16 expect;
    } expect[] = {
        { KEY_A, BTN_A },         { KEY_B, BTN_B },
        { KEY_X, BTN_Z },         { KEY_START, BTN_START },
        { KEY_UP, BTN_DUP },      { KEY_DOWN, BTN_DDOWN },
        { KEY_LEFT, BTN_DLEFT },  { KEY_RIGHT, BTN_DRIGHT },
        { KEY_L, BTN_L },         { KEY_R, BTN_R },
    };
    static const struct {
        u16 px;
        u16 py;
        u16 expect;
    } cExpect[] = {
        { ND_C_CX, ND_C_CY - 24, BTN_CUP },    { ND_C_CX, ND_C_CY + 24, BTN_CDOWN },
        { ND_C_CX - 24, ND_C_CY, BTN_CLEFT },  { ND_C_CX + 24, ND_C_CY, BTN_CRIGHT },
        { ND_C_CX + 2, ND_C_CY - 2, 0 },
    };
    static const struct {
        u16 px;
        u16 py;
        s8 x;
        s8 y;
    } stickExpect[] = {
        { ND_STICK_CX + ND_STICK_R, ND_STICK_CY, ND_STICK_MAX, 0 },
        { ND_STICK_CX - ND_STICK_R, ND_STICK_CY, -ND_STICK_MAX, 0 },
        { ND_STICK_CX, ND_STICK_CY - ND_STICK_R, 0, ND_STICK_MAX },
        { ND_STICK_CX, ND_STICK_CY + ND_STICK_R, 0, -ND_STICK_MAX },
        { ND_STICK_CX + ND_STICK_R + 8, ND_STICK_CY, ND_STICK_MAX, 0 },
        { ND_STICK_CX, ND_STICK_CY - ND_STICK_GRAB, 0, ND_STICK_MAX },
        { ND_STICK_CX, ND_STICK_CY, 0, 0 },
    };
    NdInputState st;
    size_t i;

#define ND_OK(cond)                                        \
    if (cond) {                                            \
        out->passed++;                                     \
    } else if (!out->failAt) {                             \
        out->failAt = out->checks;                         \
    }

    memset(out, 0, sizeof(*out));
    out->sizeOk = (sizeof(NdPad) == 6) && (sizeof(NdInput) == 24);
    if (!out->sizeOk) {
        return false;
    }

    for (i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
        out->checks++;
        ndInputInit(&st);
        ndInputStep(&st, expect[i].key, 0, 0, false);
        ND_OK(st.in.cur.button == expect[i].expect);
    }

    for (i = 0; i < sizeof(cExpect) / sizeof(cExpect[0]); i++) {
        out->checks++;
        ndInputInit(&st);
        ndInputStep(&st, 0, cExpect[i].px, cExpect[i].py, true);
        ND_OK(st.in.cur.button == cExpect[i].expect);
    }

    for (i = 0; i < sizeof(stickExpect) / sizeof(stickExpect[0]); i++) {
        out->checks++;
        ndInputInit(&st);
        st.dpadStick = false;
        ndInputStep(&st, 0, stickExpect[i].px, stickExpect[i].py, true);
        ND_OK(st.in.cur.stick_x == stickExpect[i].x && st.in.cur.stick_y == stickExpect[i].y);
    }

    /* press fires on the frame a key goes down, not while it is held. */
    out->checks++;
    ndInputInit(&st);
    ndInputStep(&st, KEY_A, 0, 0, false);
    ND_OK(st.in.cur.button == BTN_A && st.in.press.button == BTN_A && st.in.rel.button == 0);
    out->checks++;
    ndInputStep(&st, KEY_A, 0, 0, false);
    ND_OK(st.in.press.button == 0 && st.in.rel.button == 0);
    out->checks++;
    ndInputStep(&st, 0, 0, 0, false);
    ND_OK(st.in.cur.button == 0 && st.in.rel.button == BTN_A);

    /* The item grid belongs to the inventory layer, not to the stick. */
    out->checks++;
    ndInputInit(&st);
    ndInputStep(&st, 0, 236, 150, true);
    ND_OK(st.in.cur.button == 0 && !st.touchOwnsStick && st.touch.pressed);
    out->checks++;
    ndInputStep(&st, 0, 236, 150, false);
    ND_OK(st.touch.released && !st.touch.down);

    /* A touch in the empty upper left belongs to neither zone. */
    out->checks++;
    ndInputInit(&st);
    st.dpadStick = false;
    ndInputStep(&st, 0, 20, 40, true);
    ND_OK(st.in.cur.button == 0 && st.in.cur.stick_x == 0 && st.in.cur.stick_y == 0);

    /* Deadzone maths straight from pad.c, measured on a held deflection. */
    out->checks++;
    ndInputInit(&st);
    st.dpadStick = false;
    ndInputStep(&st, 0, ND_STICK_CX + 12, ND_STICK_CY, true);
    ndInputStep(&st, 0, ND_STICK_CX + 12, ND_STICK_CY, true);
    ND_OK(st.in.cur.stick_x == 22 && st.in.rel.stick_x == 15 && st.in.press.stick_x == 0);

    out->checks++;
    ndInputInit(&st);
    st.dpadStick = false;
    ndInputStep(&st, 0, ND_STICK_CX + 4, ND_STICK_CY + 4, true);
    ND_OK(st.in.rel.stick_x == 0 && st.in.rel.stick_y == 0);

    out->checks++;
    ndInputInit(&st);
    ndInputStep(&st, KEY_UP | KEY_LEFT, 0, 0, false);
    ND_OK(st.in.cur.stick_x == -ND_STICK_MAX && st.in.cur.stick_y == ND_STICK_MAX &&
          st.in.rel.stick_x == -ND_STICK_REL_MAX && st.in.rel.stick_y == ND_STICK_REL_MAX);
#undef ND_OK

    /* What the on-screen stick draws until a real finger arrives. */
    ndInputInit(&st);
    st.dpadStick = false;
    ndInputStep(&st, 0, ND_STICK_CX + 24, ND_STICK_CY - 16, true);
    out->probeStickX = st.in.cur.stick_x;
    out->probeStickY = st.in.cur.stick_y;
    out->probeRelX = st.in.rel.stick_x;
    out->probeRelY = st.in.rel.stick_y;
    out->probeButtons = BTN_CUP;

    return out->passed == out->checks;
}
