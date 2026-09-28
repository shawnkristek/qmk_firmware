/*
Copyright 2022 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H

#include "quantum.h"

// Dual trackball support
#include "lib/keyball/keyball.h"

// Forward declarations for Pimoroni trackball integration
void pimoroni_left_init(void);
bool pimoroni_left_read_motion(int16_t *x, int16_t *y, uint8_t *click);
void pimoroni_left_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
bool pimoroni_left_ok(void);
static void pimoroni_apply_layer_color(uint8_t layer);
// Highest active layer, ignoring the auto-mouse layer (it flips on and off
// with every trackball move and must not drive the ball LED or layer logic).
static uint8_t top_layer(layer_state_t s) {
    return get_highest_layer(s & ~((layer_state_t)1 << AUTO_MOUSE_DEFAULT_LAYER));
}
static bool layer_rgb(uint8_t layer, uint8_t *r, uint8_t *g, uint8_t *b);
static void apply_led_trim(uint8_t idx, uint8_t *r, uint8_t *g, uint8_t *b);

// Motion/click computed from one Pimoroni read. h/v = scroll, x/y = cursor
// (only one pair is nonzero depending on mode).
typedef struct {
    int16_t h;
    int16_t v;
    int16_t x;
    int16_t y;
    uint8_t click;
} pimoroni_scroll_t;

// Window-management helper: emit Ctrl+Opt+<key> for Rectangle shortcuts.
#define WM(kc) LCTL(LALT(kc))

// Custom keycodes.
//   LIGHTS   = manual all-off toggle (RGB + OLED + Pimoroni)
//   PIM_MODE = toggle the Pimoroni between scroll-wheel and cursor (sticky)
// VIA numbers "custom" keys from QK_KB_0; Keyball's own keys use QK_KB_0..15,
// so these start at QK_KB_16 and must stay in this order to match the
// customKeycodes list in tools/keyball61_via.json (QK_KB has 32 slots).
enum custom_keycodes {
    LIGHTS = QK_KB_16, // all lights off/on
    PIM_MODE,          // Pimoroni scroll <-> cursor
    TM_NEW,            // tmux: new window        (prefix c)
    TM_NEXT,           // tmux: next window       (prefix n)
    TM_PREV,           // tmux: previous window   (prefix p)
    TM_LAST,           // tmux: last window       (prefix l)
    TM_VSPL,           // tmux: split left/right  (prefix %)
    TM_HSPL,           // tmux: split top/bottom  (prefix ")
    TM_ZOOM,           // tmux: zoom pane         (prefix z)
    TM_COPY,           // tmux: copy mode         (prefix [)
    PREC_TG,           // mouse: precision toggle, 1/4 cursor speed (MX Ergo style)
    VIM_W,             // vim: Esc :w Enter
    VIM_Q,             // vim: Esc :q Enter
    VIM_WQ,            // vim: Esc :wq Enter
    DRAG_LK,           // mouse: toggle left button held (drag lock)
    DBL_CLK,           // mouse: double click
};
#define TMUX_PREFIX SS_LCTL("b")

// When true the Pimoroni acts as a cursor (second pointer) instead of a scroll
// wheel. Toggled by PIM_MODE.
static bool pimoroni_cursor_mode = false;
// Precision (sniper) mode, toggled by PREC_TG: trackball cursor motion is
// divided by PRECISION_DIV, keeping the remainder so slow moves still count.
#define PRECISION_DIV 4
static bool    precision_mode = false;
static inline bool precision_on_master(void) { return precision_mode; }
static int16_t prec_acc_x = 0, prec_acc_y = 0;

// Pimoroni ball colour per layer (RGBW), matched by eye to the key LEDs with
// the PIM_CAL tuning build. Layer 0 is off (the ball is a scroll wheel there).
static const uint8_t pim_layer_rgbw[8][4] = {
    {   0,   0,   0,   0 }, // 0 base - off
    {   0,  75,  60,  20 }, // 1 nav
    {  50,   0, 245,   0 }, // 2 window-mgmt
    { 145,  20, 200,   0 }, // 3 settings
    { 185,   0,   0,   0 }, // 4 gaming
    { 115,  60,   0,  50 }, // 5
    { 185, 200,   0,   0 }, // 6 (not yet rematched to the orange keys)
    {   0, 210,   0,  90 }, // 7
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[0] = LAYOUT_universal(
    KC_GRV       , KC_1         , KC_2         , KC_3         , KC_4         , KC_5         ,                                 KC_6         , KC_7         , KC_8         , KC_9         , KC_0         , KC_MINS      ,
    KC_TAB       , KC_Q         , KC_W         , KC_E         , KC_R         , KC_T         ,                                 KC_Y         , KC_U         , KC_I         , KC_O         , KC_P         , KC_EQL       ,
    KC_CAPS      , KC_A         , KC_S         , KC_D         , KC_F         , KC_G         ,                                 KC_H         , KC_J         , KC_K         , KC_L         , KC_SCLN      , KC_ENT       ,
    KC_LSFT      , KC_Z         , KC_X         , KC_C         , KC_V         , KC_B         , KC_LBRC      ,   KC_RBRC      , KC_N         , KC_M         , KC_COMM      , KC_DOT       , KC_SLSH      , KC_QUOT      ,
    KC_LCTL      , KC_LALT      , KC_LEFT      , KC_RIGHT     , KC_LGUI      , LT(1,KC_SPC) , KC_ESC       ,   KC_BSPC      , KC_RSFT      , _______      , _______      , _______      , TG(1)        , LT(2,KC_BSLS)
),

// Layer 1: Nav / Num. Hold left thumb (Space). MO(3) reaches Settings.
[1] = LAYOUT_universal(
    SSNP_FRE     , KC_F1        , KC_F2        , KC_F3        , KC_F4        , KC_F5        ,                                 KC_F6        , KC_F7        , KC_F8        , KC_F9        , KC_F10       , KC_F11       ,
    SSNP_VRT     , _______      , KC_7         , KC_8         , KC_9         , PIM_MODE     ,                                 _______      , KC_LEFT      , KC_UP        , KC_RGHT      , _______      , KC_F12       ,
    SSNP_HOR     , _______      , KC_4         , KC_5         , KC_6         , S(KC_SCLN)   ,                                 KC_PGUP      , KC_BTN1      , KC_DOWN      , KC_BTN2      , KC_BTN3      , _______      ,
    _______      , _______      , KC_1         , KC_2         , KC_3         , S(KC_MINS)   , S(KC_8)      ,   S(KC_9)      , KC_PGDN      , _______      , _______      , _______      , _______      , _______      ,
    TG(4)        , MO(3)        , KC_0         , KC_DOT       , _______      , _______      , SCRL_MO      ,   _______      , _______      , _______      , _______      , _______      , _______      , TG(1)
),

// Layer 2: Window management (Rectangle). Held via right thumb (Enter).
// Every key emits Ctrl+Opt+<key> to match the Rectangle shortcuts:
//   arrows = halves, U/I/J/K = corners, D/F/G = first/center/last third,
//   E = first two-thirds, T = last two-thirds.
[2] = LAYOUT_universal(
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , WM(KC_E)     , _______      , WM(KC_T)     ,                                 _______      , WM(KC_U)     , WM(KC_I)     , _______      , _______      , _______      ,
    _______      , _______      , _______      , WM(KC_D)     , WM(KC_F)     , WM(KC_G)     ,                                 _______      , WM(KC_J)     , WM(KC_K)     , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , WM(KC_LEFT)  , WM(KC_DOWN)  , WM(KC_UP)    , WM(KC_RGHT)  , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______
),

// Layer 3: Settings (RGB / CPI / boot). Reached via MO(3) on the Nav layer.
[3] = LAYOUT_universal(
    LIGHTS       , AML_TO       , AML_I50      , AML_D50      , _______      , _______      ,                                 RGB_M_P      , RGB_M_B      , RGB_M_R      , RGB_M_SW     , RGB_M_SN     , RGB_M_K      ,
    RGB_MOD      , RGB_HUI      , RGB_SAI      , RGB_VAI      , _______      , _______      ,                                 RGB_M_X      , RGB_M_G      , RGB_M_T      , RGB_M_TW     , _______      , _______      ,
    RGB_RMOD     , RGB_HUD      , RGB_SAD      , RGB_VAD      , _______      , _______      ,                                 CPI_D1K      , CPI_D100     , CPI_I100     , CPI_I1K      , KBC_SAVE     , KBC_RST      ,
    _______      , _______      , SCRL_DVD     , SCRL_DVI     , SCRL_MO      , SCRL_TO      , EE_CLR       ,   EE_CLR       , KC_HOME      , KC_PGDN      , KC_PGUP      , KC_END       , _______      , _______      ,
    QK_BOOT      , _______      , KC_LEFT      , KC_DOWN      , KC_UP        , KC_RGHT      , _______      ,   _______      , KC_DEL       , _______      , _______      , _______      , _______      , QK_BOOT
),

// Layer 4: Gaming. Toggled via TG(4).
[4] = LAYOUT_universal(
    KC_ESC       , KC_1         , KC_2         , KC_3         , KC_4         , KC_5         ,                                 KC_6         , KC_7         , KC_8         , KC_9         , KC_0         , KC_GRV       ,
    KC_TAB       , KC_Q         , KC_W         , KC_E         , KC_R         , PIM_MODE     ,                                 _______      , KC_LEFT      , KC_UP        , KC_RGHT      , _______      , KC_F12       ,
    KC_LCTL      , KC_A         , KC_S         , KC_D         , KC_F         , _______      ,                                 KC_PGUP      , KC_BTN1      , KC_DOWN      , KC_BTN2      , KC_BTN3      , _______      ,
    KC_LSFT      , KC_Z         , KC_X         , KC_C         , KC_V         , _______      , _______      ,   _______      , KC_PGDN      , _______      , _______      , _______      , _______      , _______      ,
    TG(4)        , _______      , _______      , KC_SPC       , _______      , _______      , _______      ,   KC_DEL       , CPI_D1K      , CPI_D100     , CPI_I100     , CPI_I1K      , _______      , TG(1)
),

// Layer 5: Mouse (auto). Turns on when a trackball moves; any other key
// drops back. J/K/L = left/right/middle click, ; = scroll while held,
// U/I = back/forward, O = double click, P = drag lock, H = precision toggle.
[5] = LAYOUT_universal(
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , KC_BTN4      , KC_BTN5      , DBL_CLK      , DRAG_LK      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 PREC_TG      , KC_BTN1      , KC_BTN2      , KC_BTN3      , SCRL_MO      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______
),

[6] = LAYOUT_universal(
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______
),

[7] = LAYOUT_universal(
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      ,                                 _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , _______      , _______      , _______      , _______      ,   _______      , _______      , _______      , _______      , _______      , _______      , _______
),
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED layout for RGB_MATRIX, built empirically (see scratchpad/led_map.md):
// global LED index 0-36 = LEFT half (keys 0-28, underglow 29-36),
// 37-73 = RIGHT half (underglow 37-43, keys 44-72, trackball 73). Right keys
// run column by column from the rightmost column, top to bottom (verified
// by LED walk 2026-09-26).
// matrix_co maps each [row][col] to its LED index (NO_LED where unmapped).
// clang-format off
led_config_t g_led_config = { {
    { 24, 19, 14, NO_LED, 9, 5, 1, NO_LED },
    { 25, 20, 15, NO_LED, 10, 6, 2, NO_LED },
    { 26, 21, 16, NO_LED, 11, 7, 3, NO_LED },
    { 27, 22, 17, NO_LED, 12, 8, 4, 0 },
    { 28, 23, 18, NO_LED, 13, NO_LED, NO_LED, NO_LED },
    { 44, 49, 54, NO_LED, 58, 62, 66, NO_LED },
    { 45, 50, 55, NO_LED, 59, 63, 67, NO_LED },
    { 46, 51, 56, NO_LED, 60, 64, 68, NO_LED },
    { 47, 52, 57, NO_LED, 61, 65, 69, 71 },
    { 48, 53, NO_LED, NO_LED, NO_LED, NO_LED, 70, 72 },
}, {
    { 103, 48 }, {  86,  0 }, {  86, 16 }, {  86, 32 }, {  86, 48 }, {  68,  0 },
    {  68, 16 }, {  68, 32 }, {  68, 48 }, {  51,  0 }, {  51, 16 }, {  51, 32 },
    {  51, 48 }, {  51, 64 }, {  34,  0 }, {  34, 16 }, {  34, 32 }, {  34, 48 },
    {  34, 64 }, {  17,  0 }, {  17, 16 }, {  17, 32 }, {  17, 48 }, {  17, 64 },
    {   0,  0 }, {   0, 16 }, {   0, 32 }, {   0, 48 }, {   0, 64 }, {   0, 64 },
    {  14, 64 }, {  28, 64 }, {  42, 64 }, {  57, 64 }, {  71, 64 }, {  85, 64 },
    { 100, 64 }, { 124, 64 }, { 138, 64 }, { 152, 64 }, { 166, 64 }, { 181, 64 },
    { 195, 64 }, { 209, 64 }, { 224,  0 }, { 224, 16 }, { 224, 32 }, { 224, 48 },
    { 224, 64 }, { 206,  0 }, { 206, 16 }, { 206, 32 }, { 206, 48 }, { 206, 64 },
    { 189,  0 }, { 189, 16 }, { 189, 32 }, { 189, 48 }, { 172,  0 }, { 172, 16 },
    { 172, 32 }, { 172, 48 }, { 155,  0 }, { 155, 16 }, { 155, 32 }, { 155, 48 },
    { 137,  0 }, { 137, 16 }, { 137, 32 }, { 137, 48 }, { 137, 64 }, { 120, 48 },
    { 120, 64 }, { 180, 56 },
}, {
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW,
} };
// clang-format on
#endif

#ifdef OLED_ENABLE

#    include "lib/oledkit/oledkit.h"

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_180;
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Dual Trackball Implementation (PMW3360 + Pimoroni)

// Initialize Pimoroni trackball on left half in addition to existing PMW3360
void keyboard_pre_init_kb(void) {
    // Always try to initialize Pimoroni trackball
    // It will only work on the half that has the hardware connected
    pimoroni_left_init();

    keyboard_pre_init_user();
}

// Read the Pimoroni once and turn it into a scroll/click payload. Read-and-clear
// registers, so this is the single read per cycle. With USB on the LEFT half the
// Pimoroni is on the master and this runs locally -- no split transport needed.
static pimoroni_scroll_t pimoroni_compute_scroll(void) {
    static int16_t scroll_acc_v = 0;
    static int16_t scroll_acc_h = 0;
    // Lower = more responsive/faster scroll (fewer accumulated units per tick).
    const int16_t  SCROLL_DIVISOR = 4;

    pimoroni_scroll_t out = {0, 0, 0, 0, 0};

    int16_t x, y;
    uint8_t click;
    if (!pimoroni_left_read_motion(&x, &y, &click)) {
        return out;
    }

    out.click = click;

    // Cursor mode (toggled by PIM_MODE): act as a second pointer. Send motion
    // as x/y, which sums with the right PMW3360 into one cursor. The raw offset
    // is small (tuned for scroll detents), so scale it UP for usable cursor
    // speed. Bump CURSOR_GAIN to taste.
    if (pimoroni_cursor_mode) {
        const int16_t CURSOR_GAIN = 3;
        out.x = (int16_t)(-y * CURSOR_GAIN);  // ball rotated 90deg: horizontal -> x
        out.y = (int16_t)(x * CURSOR_GAIN);   // vertical -> y
        return out;
    }

    // Otherwise: scroll wheel on the base layer (idle elsewhere; cursor work is
    // done by the right-hand PMW3360).
    if (top_layer(layer_state) == 0) {
        static uint8_t idle_reads = 0;
        if (x == 0 && y == 0) {
            // Only clear the accumulator after SUSTAINED idle, not a single
            // zero read -- slow rolls have zero reads interspersed, and wiping
            // on every one of them stopped slow scrolling from registering.
            // A persistent idle bias still decays to zero (kills drift); brief
            // gaps mid-roll keep their accumulation.
            if (++idle_reads >= 8) {
                scroll_acc_v = 0;
                scroll_acc_h = 0;
            }
        } else {
            idle_reads = 0;
            // Trackball is mounted rotated 90deg, so the ball's x axis is
            // physical up/down -> drive vertical scroll from x. Accumulate and
            // emit one tick per SCROLL_DIVISOR units; small movements are kept.
            scroll_acc_v += -x;
            scroll_acc_h += y;

            out.v = scroll_acc_v / SCROLL_DIVISOR;
            out.h = scroll_acc_h / SCROLL_DIVISOR;
            scroll_acc_v -= out.v * SCROLL_DIVISOR; // keep remainder
            scroll_acc_h -= out.h * SCROLL_DIVISOR;
        }
    }

    return out;
}

// Lights state: "everything dark" (auto-sleep on idle, or the manual all-off
// key). The Pimoroni is on the master half (USB left), so all of this is local.
static bool     lights_off    = false;
static uint32_t last_activity = 0;       // timer of last key/pointer activity

#ifdef OLED_ENABLE
// ---- OLED status screen ---------------------------------------------------
// The OLED sits on the right half, which is the slave when USB is on the left.
// The master sends this packet every 250 ms (or on change) so either half can
// draw the same screen.
#include "transactions.h"
#include "lib/lib8tion/lib8tion.h"
typedef struct __attribute__((packed)) {
    uint8_t  cpi;      // keyball CPI step; actual = (cpi + 1) * 100
    uint8_t  div;      // scroll divider
    uint8_t  flags;    // bit0 scroll, bit1 pim cursor, bit2 pim ok, bit3 lights off, bit4 precision, bit5 host asleep
    uint8_t  mods;
    uint16_t kc;       // last pressed keycode
    uint8_t  row, col; // and its matrix position
} oled_sync_t;
_Static_assert(sizeof(oled_sync_t) <= RPC_M2S_BUFFER_SIZE, "OLED sync packet too big");
static oled_sync_t osync_local, osync_remote;

// Per-key colour "class", decided on the master from the live (VIA) keymap:
//   KC_LEAVE = leave the effect/underlying colour, 0 = off, 1..7 = that
//   layer's palette colour, KC_PULSE = precision pulse.
// Both halves compute this from their own keymap; the master mirrors the VIA
// keymap into the slave's EEPROM (see keymap mirror below).
#define KC_LEAVE 0xF
#define KC_PULSE 0x8
static uint8_t key_class(uint8_t row, uint8_t col) {
    bool    precision_mode = is_keyboard_master() ? precision_on_master() : (osync_remote.flags & 16) != 0;
    bool    mouse_on = layer_state_is(AUTO_MOUSE_DEFAULT_LAYER);
    uint8_t layer    = top_layer(layer_state);
    if (mouse_on) {
        uint16_t mk = keymap_key_to_keycode(AUTO_MOUSE_DEFAULT_LAYER, (keypos_t){ .col = col, .row = row });
        if (mk == PREC_TG && precision_mode) return KC_PULSE;
        if (mk != KC_NO && mk != KC_TRANSPARENT) return AUTO_MOUSE_DEFAULT_LAYER;
    }
    uint16_t kc = keymap_key_to_keycode(layer, (keypos_t){ .col = col, .row = row });
    if (kc == PREC_TG && precision_mode) return KC_PULSE;
    if (layer == 0) return KC_LEAVE; // base: the animated effect owns it
    if (kc == KC_NO || kc == KC_TRANSPARENT) return 0;
    uint8_t dest = 0xFF;
    if (kc >= QK_MOMENTARY && kc <= QK_MOMENTARY_MAX)            dest = QK_MOMENTARY_GET_LAYER(kc);
    else if (kc >= QK_TOGGLE_LAYER && kc <= QK_TOGGLE_LAYER_MAX) dest = QK_TOGGLE_LAYER_GET_LAYER(kc);
    else if (kc >= QK_TO && kc <= QK_TO_MAX)                     dest = QK_TO_GET_LAYER(kc);
    else if (kc >= QK_LAYER_TAP && kc <= QK_LAYER_TAP_MAX)       dest = QK_LAYER_TAP_GET_LAYER(kc);
    return (dest >= 1 && dest <= 7) ? dest : layer;
}

static void osync_fill(oled_sync_t *o) {
    o->cpi   = keyball_get_cpi();
    o->div   = keyball_get_scroll_div();
    o->flags = (keyball_get_scroll_mode() ? 1 : 0) | (pimoroni_cursor_mode ? 2 : 0) | (pimoroni_left_ok() ? 4 : 0) | (lights_off ? 8 : 0) | (precision_mode ? 16 : 0);
    o->mods  = get_mods() | get_oneshot_mods();
}
static void osync_slave_handler(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    if (in_len != sizeof(oled_sync_t)) return;
    memcpy(&osync_remote, in, sizeof(oled_sync_t));
    // The master stops all split traffic while the host sleeps, so this packet
    // is the slave's only notice: blank the LEDs now (RGB_MATRIX_SLEEP). The
    // master's normal RGB sync clears it again on wake.
    rgb_matrix_set_suspend_state((osync_remote.flags & 32) != 0);
}

// Host going to sleep: tell the slave once, before the master goes quiet.
static bool osync_suspend_sent = false;
void suspend_power_down_user(void) {
    if (osync_suspend_sent || !is_keyboard_master()) return;
    osync_fill(&osync_local);
    osync_local.flags |= 32;
    osync_suspend_sent = transaction_rpc_send(USER_OLED_SYNC, sizeof(osync_local), &osync_local);
}
void suspend_wakeup_init_user(void) {
    osync_suspend_sent = false; // next housekeeping sync clears the flag
}
static void osync_task(void) {
    static uint32_t    last = 0;
    static oled_sync_t sent;
    static uint32_t last_fill = 0;
    if (!is_keyboard_master()) return;
    if (timer_elapsed32(last_fill) < 10) return;
    last_fill = timer_read32();
    osync_fill(&osync_local);
    bool changed = memcmp(&sent, &osync_local, sizeof(sent)) != 0;
    if ((changed && timer_elapsed32(last) > 50) || timer_elapsed32(last) > 250) {
        if (transaction_rpc_send(USER_OLED_SYNC, sizeof(osync_local), &osync_local)) {
            sent = osync_local;
        }
        last = timer_read32();
    }
}

static const char *const layer_names[8] = { "Base", "Nav", "WinMgr", "Settings", "Gaming", "Mouse", "L6", "L7" };

static void oled_render_status(const oled_sync_t *o) {
    char    line[32]; // OLED shows 21 columns; room for worst-case numbers
    uint8_t layer = get_highest_layer(layer_state);
    bool    caps  = host_keyboard_led_state().caps_lock;
    uint8_t m     = o->mods;
    snprintf(line, sizeof(line), "%-8s %c%c%c%c %s", layer < 8 ? layer_names[layer] : "?",
             (m & MOD_MASK_CTRL) ? 'C' : ' ', (m & MOD_MASK_SHIFT) ? 'S' : ' ',
             (m & MOD_MASK_ALT) ? 'A' : ' ', (m & MOD_MASK_GUI) ? 'G' : ' ', caps ? "CAPS" : "");
    oled_write_ln(line, false);
    snprintf(line, sizeof(line), "Ball %5u %s/%u %s", (unsigned)((o->cpi + 1) * 100), (o->flags & 1) ? "SCR" : "scr", o->div,
             (o->flags & 16) ? "PREC" : "");
    oled_write_ln(line, false);
    snprintf(line, sizeof(line), "Pim %-7s %s", (o->flags & 2) ? "cursor" : "scroll", (o->flags & 4) ? "ok" : "--");
    oled_write_ln(line, false);
    snprintf(line, sizeof(line), "Key %04X r%uc%u", o->kc, o->row, o->col);
    oled_write_ln(line, false);
}

bool oled_task_user(void) {
    const oled_sync_t *o = is_keyboard_master() ? &osync_local : &osync_remote;
    if (o->flags & (8 | 32)) { // lights off (idle / manual) or host asleep
        oled_clear();
        oled_off();
        return false;
    }
    oled_on();
    oled_set_cursor(0, 0);
    oled_render_status(o);
    return false;
}
#endif // OLED_ENABLE

// ---- VIA keymap mirror ------------------------------------------------------
// VIA edits only the master's EEPROM keymap, but each half lights its own keys
// from its own copy. After boot (once the Keyball ball handshake is done) and
// after any VIA keymap write, the master streams the whole keymap to the slave
// in small chunks, one every KM_SYNC_GAP_MS; the slave only rewrites bytes that
// changed. Traffic is zero the rest of the time.
#include "transactions.h"
#include "dynamic_keymap.h"
#define KM_SYNC_CHUNK   24
#define KM_SYNC_GAP_MS  30
#define KM_SYNC_BOOT_MS 4000
#define KM_SYNC_TOTAL   (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2)
typedef struct __attribute__((packed)) {
    uint16_t offset;
    uint8_t  len;
    uint8_t  data[KM_SYNC_CHUNK];
} km_chunk_t;
_Static_assert(sizeof(km_chunk_t) <= RPC_M2S_BUFFER_SIZE, "keymap chunk too big");
static uint16_t km_sync_off   = 0;     // next offset to send; KM_SYNC_TOTAL = idle
static uint32_t km_sync_after = 0;     // don't start before this time
static bool     km_sync_armed = true;  // boot sync pending

static void km_slave_handler(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    const km_chunk_t *c = (const km_chunk_t *)in;
    if (in_len != sizeof(km_chunk_t) || c->len > KM_SYNC_CHUNK || c->offset + c->len > KM_SYNC_TOTAL) return;
    dynamic_keymap_set_buffer(c->offset, c->len, (uint8_t *)c->data);
}
static void km_sync_request(uint16_t delay_ms) {
    km_sync_armed = true;
    km_sync_off   = 0;
    km_sync_after = timer_read32() + delay_ms;
}
static void km_sync_task(void) {
    static uint32_t last = 0;
    if (!is_keyboard_master() || !km_sync_armed) return;
    if ((int32_t)(timer_read32() - km_sync_after) < 0 || timer_elapsed32(last) < KM_SYNC_GAP_MS) return;
    last = timer_read32();
    km_chunk_t c;
    c.offset = km_sync_off;
    c.len    = (KM_SYNC_TOTAL - km_sync_off) < KM_SYNC_CHUNK ? (KM_SYNC_TOTAL - km_sync_off) : KM_SYNC_CHUNK;
    dynamic_keymap_get_buffer(c.offset, c.len, c.data);
    if (transaction_rpc_send(USER_KEYMAP_SYNC, sizeof(c), &c)) {
        km_sync_off += c.len;
        if (km_sync_off >= KM_SYNC_TOTAL) km_sync_armed = false;
    }
}
// Any VIA keymap write re-arms the mirror (debounced, so a burst of edits
// sends one pass). Returning false lets VIA handle the command as usual.
bool via_command_kb(uint8_t *data, uint8_t length) {
    switch (data[0]) {
        case id_dynamic_keymap_set_keycode:
        case id_dynamic_keymap_reset:
        case id_dynamic_keymap_set_buffer:
            km_sync_request(300);
            break;
        default:
            break;
    }
    return false;
}
#define LIGHTS_SLEEP_MS 300000            // 5 minutes idle -> lights off

// Apply the current lights_off state to the Pimoroni LED, OLED, and RGB.
// Safe to call every cycle; it only writes when the state changes.
static void apply_lights(void) {
    static int8_t applied = -1;          // -1 = unknown, forces first apply
    if (applied == (int8_t)lights_off) {
        return;
    }
    applied = (int8_t)lights_off;

#ifdef OLED_ENABLE
    if (lights_off) {
        oled_clear();
        oled_off();
    } else {
        oled_on();
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (lights_off) {
        rgb_matrix_disable_noeeprom();
    } else {
        rgb_matrix_enable_noeeprom();
    }
#endif
    // Pimoroni LED: off when sleeping, else the current layer color.
    pimoroni_apply_layer_color(top_layer(layer_state));
}

void keyboard_post_init_user(void) {
    last_activity = timer_read32();  // don't sleep immediately at boot
#ifdef OLED_ENABLE
    transaction_register_rpc(USER_OLED_SYNC, osync_slave_handler);
#endif
    transaction_register_rpc(USER_KEYMAP_SYNC, km_slave_handler);
    km_sync_request(KM_SYNC_BOOT_MS);
    // Keyball restores auto-mouse on/off from its saved config (default off)
    // before this runs, so switch it on here. AML_TO on Settings toggles it.
    set_auto_mouse_enable(true);
}

// Read the Pimoroni locally on the half that has it and inject scroll/click.
// With USB on the left, that half is the master, so this reaches the host
// directly -- no split transport for the Pimoroni.
report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    static bool last_click_state = false;

    if (precision_mode) {
        prec_acc_x += mouse_report.x;
        prec_acc_y += mouse_report.y;
        mouse_report.x = prec_acc_x / PRECISION_DIV;
        mouse_report.y = prec_acc_y / PRECISION_DIV;
        prec_acc_x -= mouse_report.x * PRECISION_DIV;
        prec_acc_y -= mouse_report.y * PRECISION_DIV;
    }

    if (is_keyboard_left()) {
        pimoroni_scroll_t s = pimoroni_compute_scroll();

        // Trackball use counts as activity (wake the lights / reset sleep).
        if (s.h || s.v || s.x || s.y || s.click) {
            last_activity = timer_read32();
            lights_off    = false;
        }

        // Scroll mode -> h/v; cursor mode -> add x/y so it sums with the
        // PMW3360 into one pointer (second cursor). Clamp the sum to the report
        // range so it never overflows.
        mouse_report.h = s.h;
        mouse_report.v = s.v;
        int16_t nx = (int16_t)mouse_report.x + s.x;
        int16_t ny = (int16_t)mouse_report.y + s.y;
        if (nx > 127) nx = 127; else if (nx < -127) nx = -127;
        if (ny > 127) ny = 127; else if (ny < -127) ny = -127;
        mouse_report.x = (mouse_xy_report_t)nx;
        mouse_report.y = (mouse_xy_report_t)ny;

        // Edge-detect the click so we set/clear the button cleanly.
        bool current_click_state = (s.click != 0);
        if (current_click_state != last_click_state) {
            if (current_click_state) {
                mouse_report.buttons |= MOUSE_BTN1; // Left mouse button
            } else {
                mouse_report.buttons &= ~MOUSE_BTN1;
            }
            last_click_state = current_click_state;
        } else if (current_click_state) {
            mouse_report.buttons |= MOUSE_BTN1; // hold across reports
        }
    }

    return mouse_report;
}

// Any keypress counts as activity: reset the idle timer and wake the lights.
// The LIGHTS keycode manually toggles everything off/on.
#ifdef LED_CAL
// TEMP: live per-LED white-balance calibration. Build with
//   qmk flash -e EXTRAFLAGS=-DLED_CAL -e CONSOLE_ENABLE=yes
// All LEDs white. Press a key to select its LED: it starts SWEEPING the
// active channel's trim 60..130 % (about 8 s per pass). Tap Space when it
// matches its neighbours to LOCK the value. Bottom-row Ctrl/Alt/GUI choose
// the R/G/B channel to sweep. Esc dumps the table. Keystrokes are swallowed.
static uint8_t  cal_pct[RGB_MATRIX_LED_COUNT][3];
static bool     cal_init     = false;
static uint8_t  cal_sel      = NO_LED;
static uint8_t  cal_chan     = 0;
static bool     cal_sweeping = false;
static uint32_t cal_sweep_t0 = 0;
#define CAL_SWEEP_MIN     50
#define CAL_SWEEP_MAX     100
#define CAL_SWEEP_STEP    2
#define CAL_SWEEP_STEP_MS 1000
static uint8_t cal_sweep_value(void) {
    uint32_t n    = (CAL_SWEEP_MAX - CAL_SWEEP_MIN) / CAL_SWEEP_STEP + 1;
    uint32_t step = (timer_elapsed32(cal_sweep_t0) / CAL_SWEEP_STEP_MS) % n;
    return CAL_SWEEP_MIN + (uint8_t)(step * CAL_SWEEP_STEP);
}
// Print the swept value once per step so the console shows where the sweep is.
static void cal_sweep_report(void) {
    static uint8_t last = 0xFF;
    if (!cal_sweeping || cal_sel == NO_LED) return;
    uint8_t v = cal_sweep_value();
    if (v != last) { last = v; uprintf("CAL at %u\n", v); }
}
static void cal_dump(void) {
    uprintf("CAL table (idx r g b), only non-100 entries:\n");
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (cal_pct[i][0] != 100 || cal_pct[i][1] != 100 || cal_pct[i][2] != 100)
            uprintf("CAL { %u, %u, %u, %u },\n", i, cal_pct[i][0], cal_pct[i][1], cal_pct[i][2]);
    }
}
static bool cal_process(keyrecord_t *record) {
    if (!record->event.pressed) return false;
    last_activity = timer_read32(); // keep the idle sleep away while calibrating
    lights_off    = false;
    uint8_t row = record->event.key.row, col = record->event.key.col;
    const uint8_t orow = row, ocol = col; // original position for LED lookup
    // Bottom-row matrix columns are 0,1,2,4,5,6,7 (col 3 is unused).
    // LEFT half (row 4):  Ctrl=0 Alt=1 Left=2 Right=4 GUI=5 Space=6 Esc=7
    //   channel R=Ctrl G=Alt B=Right, lock=Space, dump=Esc.
    // Top-row group trims, +/-5 % per press, applied to a whole group:
    // RIGHT (row 5): 6/7 = R-/R+, 8/9 = G-/G+, 0/- = B-/B+  (cols 6,5,4,2,1,0)
    //   group = Y, O, slash.
    // LEFT  (row 0): `/1 = R-/R+, 2/3 = G-/G+, 4/5 = B-/B+  (cols 0,1,2,4,5,6)
    //   group = grave, Q, S, B, Alt.
    if (row == 5 || row == 0) {
        static const uint8_t group_r[] = { 67, 55, 52 };
        static const uint8_t group_l[] = { 24, 20, 16, 4, 23 };
        const uint8_t *group = row == 5 ? group_r : group_l;
        uint8_t        n     = row == 5 ? sizeof(group_r) : sizeof(group_l);
        int8_t chan = -1, delta = 0;
        if (row == 5) {
            switch (col) {
                case 6: chan = 0; delta = -5; break; // 6: R-
                case 5: chan = 0; delta = +5; break; // 7: R+
                case 4: chan = 1; delta = -5; break; // 8: G-
                case 2: chan = 1; delta = +5; break; // 9: G+
                case 1: chan = 2; delta = -5; break; // 0: B-
                case 0: chan = 2; delta = +5; break; // -: B+
                default: break;
            }
        } else {
            switch (col) {
                case 0: chan = 0; delta = -5; break; // `: R-
                case 1: chan = 0; delta = +5; break; // 1: R+
                case 2: chan = 1; delta = -5; break; // 2: G-
                case 4: chan = 1; delta = +5; break; // 3: G+
                case 5: chan = 2; delta = -5; break; // 4: B-
                case 6: chan = 2; delta = +5; break; // 5: B+
                default: break;
            }
        }
        if (chan >= 0) {
            for (uint8_t i = 0; i < n; i++) {
                int16_t v = cal_pct[group[i]][chan] + delta;
                if (v < 10) v = 10;
                if (v > 150) v = 150;
                cal_pct[group[i]][chan] = (uint8_t)v;
            }
            cal_sweeping = false;
            uprintf("CAL group %s -> r=%u g=%u b=%u\n", row == 5 ? "Y/O/slash" : "grave/Q/S/B/Alt",
                    cal_pct[group[0]][0], cal_pct[group[0]][1], cal_pct[group[0]][2]);
            return false;
        }
    }
    // RIGHT half (row 9) only has four physical bottom keys (the trackball
    // takes the rest): Bksp=0 RShift=1 TG(1)=6 Bksl=7.
    //   TG(1) cycles the mode R -> G -> B -> ALL, Backspace locks, RShift dumps.
    if (row == 9) {
        switch (col) {
            case 6:
                cal_chan = (cal_chan + 1) % 4;
                uprintf("CAL channel %s\n", cal_chan == 0 ? "R" : cal_chan == 1 ? "G" : cal_chan == 2 ? "B" : "ALL (brightness)");
                return false;
            case 0: uprintf("CAL key: Backspace (lock)\n"); col = 6; break;
            case 1: uprintf("CAL key: RShift (dump)\n");    col = 7; break;
            default: uprintf("CAL key: row 9 col %u (no function)\n", col); return false;
        }
        row = 4;
    }
    if (row == 4) {
        switch (col) {
            case 0: cal_chan = 0; uprintf("CAL channel R\n"); return false;
            case 1: cal_chan = 1; uprintf("CAL channel G\n"); return false;
            case 4: cal_chan = 2; uprintf("CAL channel B\n"); return false;
            case 2: cal_chan = 3; uprintf("CAL channel ALL (brightness)\n"); return false; // Left arrow on the left half
            case 6: // Space: lock the swept value
                if (cal_sel != NO_LED && cal_sweeping) {
                    uint8_t v = cal_sweep_value();
                    if (cal_chan == 3) { cal_pct[cal_sel][0] = v; cal_pct[cal_sel][1] = v; cal_pct[cal_sel][2] = v; }
                    else cal_pct[cal_sel][cal_chan] = v;
                    cal_sweeping = false;
                    uprintf("CAL LOCK idx %u -> r=%u g=%u b=%u\n", cal_sel, cal_pct[cal_sel][0], cal_pct[cal_sel][1], cal_pct[cal_sel][2]);
                }
                return false;
            case 7: cal_dump(); return false; // Esc
            default: break;
        }
    }
    uint8_t idx = g_led_config.matrix_co[orow][ocol];
    if (idx != NO_LED) {
        cal_sel = idx; cal_sweeping = true; cal_sweep_t0 = timer_read32();
        uprintf("CAL sweep idx %u (row %u col %u) channel %c\n", idx, orow, ocol, "RGBA"[cal_chan]);
    }
    return false;
}
#endif

#ifdef LED_MAPWALK
// TEMP: LED map walk. Build with
//   qmk flash -e EXTRAFLAGS=-DLED_MAPWALK -e CONSOLE_ENABLE=yes
// Lights one LED at a time (this half's range). Press the lit key to record
// "MAP idx = row col" on the console and advance. RShift/Esc = no key here
// (underglow/trackball), Backspace = step back one. Keystrokes are swallowed.
static uint8_t  mw_idx      = 0xFF;
static uint8_t  mw_first    = 0, mw_last = 0;
static void mw_init(void) {
    const uint8_t split[2] = RGB_MATRIX_SPLIT;
    if (is_keyboard_left()) { mw_first = 0;        mw_last = split[0] - 1; }
    else                    { mw_first = split[0]; mw_last = RGB_MATRIX_LED_COUNT - 1; }
    mw_idx = mw_first;
    uprintf("MAP walk %s half: idx %u..%u\n", is_keyboard_left() ? "LEFT" : "RIGHT", mw_first, mw_last);
}
static bool mw_process(keyrecord_t *record) {
    if (!record->event.pressed) return false;
    last_activity = timer_read32(); lights_off = false;
    if (mw_idx == 0xFF) mw_init();
    uint8_t row = record->event.key.row, col = record->event.key.col;
    bool skip = (row == 4 && col == 7) || (row == 9 && col == 1); // Esc (L) / RShift (R)
    bool back = (row == 4 && col == 6) || (row == 9 && col == 0); // Space (L) / Backspace (R)
    if (back) {
        if (mw_idx > mw_first) mw_idx--;
        uprintf("MAP back to idx %u\n", mw_idx);
        return false;
    }
    if (skip) uprintf("MAP idx %u = (no key)\n", mw_idx);
    else      uprintf("MAP idx %u = row %u col %u\n", mw_idx, row, col);
    if (mw_idx < mw_last) mw_idx++; else uprintf("MAP walk done\n");
    return false;
}
#endif

#ifdef PIM_CAL
// TEMP: Pimoroni LED colour tuning. Build with
//   qmk flash -e EXTRAFLAGS=-DPIM_CAL -e CONSOLE_ENABLE=yes
// All key LEDs show the target layer's colour; nudge the ball's RGBW until it
// matches. grave = cycle target layer 1..4; 1/2 = R-/R+; 3/4 = G-/G+;
// Tab/Q = B-/B+; W/E = W-/W+ (steps of 10); Esc = dump. Keys are swallowed.
static uint8_t pimc_layer = 1;
static uint8_t pimc[8][4];
static bool    pimc_init  = false, pimc_dirty = false;
static void pimc_setup(void) {
    for (uint8_t l = 1; l <= 7; l++)
        for (uint8_t c = 0; c < 4; c++) pimc[l][c] = pim_layer_rgbw[l][c];
    pimc_init = true; pimc_dirty = true;
}
static void pimc_dump(void) {
    for (uint8_t l = 1; l <= 7; l++)
        uprintf("PIMC case %u: pimoroni_left_set_rgbw(%u, %u, %u, %u);\n", l, pimc[l][0], pimc[l][1], pimc[l][2], pimc[l][3]);
}
static bool pimc_process(keyrecord_t *record) {
    if (!record->event.pressed) return false;
    last_activity = timer_read32(); lights_off = false;
    if (!pimc_init) pimc_setup();
    uint8_t row = record->event.key.row, col = record->event.key.col;
    int8_t chan = -1, delta = 0;
    if (row == 0) {
        switch (col) {
            case 0: pimc_layer = pimc_layer % 7 + 1; pimc_dirty = true; uprintf("PIMC target layer %u\n", pimc_layer); return false;
            case 1: chan = 0; delta = -10; break; // 1: R-
            case 2: chan = 0; delta = +10; break; // 2: R+
            case 4: chan = 1; delta = -10; break; // 3: G-
            case 5: chan = 1; delta = +10; break; // 4: G+
            default: break;
        }
    } else if (row == 1) {
        switch (col) {
            case 0: chan = 2; delta = -10; break; // Tab: B-
            case 1: chan = 2; delta = +10; break; // Q:   B+
            case 2: chan = 3; delta = -10; break; // W:   W-
            case 4: chan = 3; delta = +10; break; // E:   W+
            default: break;
        }
    } else if (row == 4 && col == 7) { pimc_dump(); return false; } // Esc
    if (chan >= 0) {
        int16_t v = pimc[pimc_layer][chan] + delta;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        pimc[pimc_layer][chan] = (uint8_t)v;
        pimc_dirty = true;
        uprintf("PIMC L%u r=%u g=%u b=%u w=%u\n", pimc_layer, pimc[pimc_layer][0], pimc[pimc_layer][1], pimc[pimc_layer][2], pimc[pimc_layer][3]);
    }
    return false;
}
#endif

#ifdef LED_PAL
// TEMP: layer palette explorer. Build with
//   qmk flash -e EXTRAFLAGS=-DLED_PAL -e CONSOLE_ENABLE=yes
// All key LEDs show the current slot's colour. grave = next slot (1..7);
// 1/2 = R-/R+; 3/4 = G-/G+; Tab/Q = B-/B+ (steps of 15); Esc = dump all
// seven as layer_rgb() case lines. Keystrokes are swallowed.
static uint8_t pal_slot = 1;
static uint8_t pal[8][3];
static bool    pal_init = false;
static void pal_setup(void) {
    for (uint8_t l = 1; l <= 7; l++) layer_rgb(l, &pal[l][0], &pal[l][1], &pal[l][2]);
    pal_init = true;
}
static bool pal_process(keyrecord_t *record) {
    if (!record->event.pressed) return false;
    last_activity = timer_read32(); lights_off = false;
    if (!pal_init) pal_setup();
    uint8_t row = record->event.key.row, col = record->event.key.col;
    int8_t chan = -1, delta = 0;
    if (row == 0) {
        switch (col) {
            case 0: pal_slot = pal_slot % 7 + 1; uprintf("PAL slot %u: r=%u g=%u b=%u\n", pal_slot, pal[pal_slot][0], pal[pal_slot][1], pal[pal_slot][2]); return false;
            case 1: chan = 0; delta = -15; break; // 1: R-
            case 2: chan = 0; delta = +15; break; // 2: R+
            case 4: chan = 1; delta = -15; break; // 3: G-
            case 5: chan = 1; delta = +15; break; // 4: G+
            default: break;
        }
    } else if (row == 1) {
        switch (col) {
            case 0: chan = 2; delta = -15; break; // Tab: B-
            case 1: chan = 2; delta = +15; break; // Q:   B+
            default: break;
        }
    } else if (row == 4 && col == 7) { // Esc: dump
        for (uint8_t l = 1; l <= 7; l++)
            uprintf("PAL case %u: *r = %u; *g = %u; *b = %u; return true;\n", l, pal[l][0], pal[l][1], pal[l][2]);
        return false;
    }
    if (chan >= 0) {
        int16_t v = pal[pal_slot][chan] + delta;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        pal[pal_slot][chan] = (uint8_t)v;
        uprintf("PAL slot %u: r=%u g=%u b=%u\n", pal_slot, pal[pal_slot][0], pal[pal_slot][1], pal[pal_slot][2]);
    }
    return false;
}
#endif

// Custom mouse keys keep the auto-mouse layer active like real buttons.
bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    return keycode == DRAG_LK || keycode == DBL_CLK || keycode == PREC_TG;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef LED_PAL
    return pal_process(record);
#endif
#ifdef PIM_CAL
    return pimc_process(record);
#endif
#ifdef LED_MAPWALK
    return mw_process(record);
#endif
#ifdef LED_CAL
    return cal_process(record);
#endif
    if (record->event.pressed) {
        last_activity = timer_read32();
#ifdef OLED_ENABLE
        osync_local.kc  = keycode;
        osync_local.row = record->event.key.row;
        osync_local.col = record->event.key.col;
#endif
        switch (keycode) {
            case TM_NEW:  SEND_STRING(TMUX_PREFIX "c"); return false;
            case TM_NEXT: SEND_STRING(TMUX_PREFIX "n"); return false;
            case TM_PREV: SEND_STRING(TMUX_PREFIX "p"); return false;
            case TM_LAST: SEND_STRING(TMUX_PREFIX "l"); return false;
            case TM_VSPL: SEND_STRING(TMUX_PREFIX "%"); return false;
            case TM_HSPL: SEND_STRING(TMUX_PREFIX "\""); return false;
            case TM_ZOOM: SEND_STRING(TMUX_PREFIX "z"); return false;
            case TM_COPY: SEND_STRING(TMUX_PREFIX "["); return false;
            case VIM_W:   SEND_STRING(SS_TAP(X_ESC) ":w\n"); return false;
            case VIM_Q:   SEND_STRING(SS_TAP(X_ESC) ":q\n"); return false;
            case VIM_WQ:  SEND_STRING(SS_TAP(X_ESC) ":wq\n"); return false;
            case DBL_CLK: tap_code16(KC_BTN1); tap_code16(KC_BTN1); return false;
            case PREC_TG: precision_mode = !precision_mode; prec_acc_x = prec_acc_y = 0; return false;
            case DRAG_LK: {
                static bool held = false;
                held = !held;
                if (held) register_code16(KC_BTN1); else unregister_code16(KC_BTN1);
                return false;
            }
            default: break;
        }
        if (keycode == LIGHTS) {
            lights_off = !lights_off;
            return false; // consume the key
        }
        if (keycode == PIM_MODE) {
            pimoroni_cursor_mode = !pimoroni_cursor_mode;
            // Don't touch the LED (I2C) here -- doing an I2C write inside the
            // keypress handler can stall/wedge a flaky bus. housekeeping picks
            // up the change and updates the LED safely.
            return false;
        }
        // Any other key wakes the lights if they were sleeping.
        if (lights_off) {
            lights_off = false;
        }
    }
    return true;
}

void housekeeping_task_user(void) {
#ifdef OLED_ENABLE
    osync_task();
#endif
    km_sync_task();
#ifdef PIM_CAL
    if (pimc_dirty) {
        pimc_dirty = false;
        pimoroni_left_set_rgbw(pimc[pimc_layer][0], pimc[pimc_layer][1], pimc[pimc_layer][2], pimc[pimc_layer][3]);
    }
#endif
    // ---- Auto-sleep (master decides) -------------------------------------
    // After LIGHTS_SLEEP_MS with no key/pointer activity, blank the lights.
    // last_activity is reset by process_record_user and by pointer motion.
    if (is_keyboard_master()) {
        if (!lights_off && TIMER_DIFF_32(timer_read32(), last_activity) > LIGHTS_SLEEP_MS) {
            lights_off = true;
        }
    }

    // ---- Apply RGB/OLED on lights_off change ------------------------------
    apply_lights();
    // ---- Drive the Pimoroni LED only when its color should change ---------
    // (The underglow/per-key coloring is handled by the RGB_MATRIX indicator
    // hook below; only the Pimoroni I2C LED is rate-limited here.)
    static uint8_t last_layer = 0xFF;
    static int8_t  last_off   = -1;
    static int8_t  last_cur   = -1;
    uint8_t cur_layer = top_layer(layer_state);
    if (cur_layer != last_layer || (int8_t)lights_off != last_off
        || (int8_t)pimoroni_cursor_mode != last_cur) {
        last_layer = cur_layer;
        last_off   = (int8_t)lights_off;
        last_cur   = (int8_t)pimoroni_cursor_mode;
        pimoroni_apply_layer_color(cur_layer);
    }
}

// Enhanced OLED rendering for dual trackball
#ifdef OLED_ENABLE
#endif

// Set the Pimoroni trackball LED for the given layer. Forced off when the
// lights are off (idle sleep or manual all-off). Only acts on the half that
// has the Pimoroni (left).
static void pimoroni_apply_layer_color(uint8_t layer) {
#ifdef PIM_CAL
    return; // tuning build drives the ball LED itself
#endif
    if (!is_keyboard_left()) {
        return;
    }
    if (lights_off) {
        pimoroni_left_set_rgbw(0, 0, 0, 0);
        return;
    }
    // Base layer: white when the Pimoroni is in cursor mode (visual cue),
    // otherwise dark. Cursor mode shows only on the base layer; other layers
    // keep their normal colors.
    if (layer == 0 && pimoroni_cursor_mode) {
        pimoroni_left_set_rgbw(80, 80, 80, 30);
        return;
    }
    // Colors match the cheatsheet per-layer accents. Base layer (0) keeps the
    // LED off so the trackball is dark during normal typing.
    if (layer < 8) pimoroni_left_set_rgbw(pim_layer_rgbw[layer][0], pim_layer_rgbw[layer][1], pim_layer_rgbw[layer][2], pim_layer_rgbw[layer][3]);
    else           pimoroni_left_set_rgbw(0, 0, 0, 0);
}

// Underglow (RGBLIGHT strip) per layer: base layer runs the animated effect,
// every other layer shows a solid color matching that layer (and the Pimoroni
// LED / cheatsheet). Driven from housekeeping on layer change.
#ifdef RGB_MATRIX_ENABLE
// RGB (in 0-255) for a given layer's accent. NULL via the return-by-pointer
// pattern: returns true and fills r/g/b for layers that have a color; layer 0
// (base) returns false (handled by the running effect).
static bool layer_rgb(uint8_t layer, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (layer) {
        // Chosen by eye with the LED_PAL palette explorer build.
        case 1: *r = 15;  *g = 105; *b = 60;  return true; // nav
        case 2: *r = 0;   *g = 0;   *b = 255; return true; // window-mgmt
        case 3: *r = 135; *g = 0;   *b = 255; return true; // settings
        case 4: *r = 255; *g = 0;   *b = 0;   return true; // gaming
        case 5: *r = 255; *g = 30;  *b = 0;   return true;
        case 6: *r = 255; *g = 110; *b = 0;   return true;
        case 7: *r = 45;  *g = 255; *b = 0;   return true;
        default: return false;                              // base: effect
    }
}

// Per-LED color trim, in percent, to even out LED-to-LED hue variance on
// specific keys. Applied to indicator colors only (effects are untouched).
// 100 = no change. Tune by eye on a solid-color layer (Gaming = pure red).
typedef struct { uint8_t idx; uint8_t r, g, b; } led_trim_t;
static const led_trim_t led_trim[] = {
    // These five LEDs run brighter and warmer than the rest; values matched by
    // eye on a white field (swept per channel on B, applied to all five).
    { 24, 40, 72, 65 }, // grave
    { 20, 40, 72, 65 }, // Q
    { 16, 40, 72, 65 }, // S
    {  4, 40, 72, 65 }, // B
    { 23, 40, 72, 65 }, // Alt
    // Right half, matched by eye with the top-row trim keys.
    { 67, 40, 67, 43 }, // Y
    { 55, 40, 67, 43 }, // O
    { 52, 40, 67, 43 }, // slash
};
static void apply_led_trim(uint8_t idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    for (uint8_t i = 0; i < sizeof(led_trim) / sizeof(led_trim[0]); i++) {
        if (led_trim[i].idx != idx) continue;
        uint16_t tr = (uint16_t)*r * led_trim[i].r / 100;
        uint16_t tg = (uint16_t)*g * led_trim[i].g / 100;
        uint16_t tb = (uint16_t)*b * led_trim[i].b / 100;
        *r = tr > 255 ? 255 : tr;
        *g = tg > 255 ? 255 : tg;
        *b = tb > 255 ? 255 : tb;
        return;
    }
}

// Per-key indicator: on non-base layers, paint only mapped keys in the layer
// color, and tint each layer-switch key (TG/MO/LT/TO) with the color of the
// layer it activates. Base layer is left to the running effect.
// Light the keys that are mapped on the auto-mouse layer (buttons, scroll)
// in that layer's colour, on top of whatever the layer below shows.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#ifdef LED_PAL
    {
        if (!pal_init) pal_setup();
        uint8_t v = rgb_matrix_get_val();
        for (uint8_t i = led_min; i < led_max; i++) {
            uint8_t r = pal[pal_slot][0], g = pal[pal_slot][1], b = pal[pal_slot][2];
            apply_led_trim(i, &r, &g, &b);
            rgb_matrix_set_color(i, (uint16_t)r * v / 255, (uint16_t)g * v / 255, (uint16_t)b * v / 255);
        }
        return false;
    }
#endif
#ifdef PIM_CAL
    {
        if (!pimc_init) pimc_setup();
        uint8_t r = 0, g = 0, b = 0, v = rgb_matrix_get_val();
        layer_rgb(pimc_layer, &r, &g, &b);
        for (uint8_t i = led_min; i < led_max; i++) {
            uint8_t tr = r, tg = g, tb = b;
            apply_led_trim(i, &tr, &tg, &tb);
            rgb_matrix_set_color(i, (uint16_t)tr * v / 255, (uint16_t)tg * v / 255, (uint16_t)tb * v / 255);
        }
        return false;
    }
#endif
#ifdef LED_MAPWALK
    if (mw_idx == 0xFF) mw_init();
    for (uint8_t i = led_min; i < led_max; i++) rgb_matrix_set_color(i, i == mw_idx ? 120 : 0, i == mw_idx ? 120 : 0, i == mw_idx ? 120 : 0);
    return false;
#endif
#ifdef LED_CAL
    if (!cal_init) {
        memset(cal_pct, 100, sizeof(cal_pct));
        for (uint8_t i = 0; i < sizeof(led_trim) / sizeof(led_trim[0]); i++) {
            cal_pct[led_trim[i].idx][0] = led_trim[i].r; cal_pct[led_trim[i].idx][1] = led_trim[i].g; cal_pct[led_trim[i].idx][2] = led_trim[i].b;
        }
        cal_init = true;
    }
    // Field colour: white by default, or a layer's colour with -DLED_CAL_LAYER=n
    // (e.g. 3 = Settings orange) so trims are judged on the colour that shows
    // the mismatch most.
    uint8_t fr = 120, fg = 120, fb = 120;
#ifdef LED_CAL_LAYER
    {
        uint8_t r, g, b;
        if (layer_rgb(LED_CAL_LAYER, &r, &g, &b)) { fr = (uint16_t)r * 120 / 255; fg = (uint16_t)g * 120 / 255; fb = (uint16_t)b * 120 / 255; }
    }
#endif
    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t pr = cal_pct[i][0], pg = cal_pct[i][1], pb = cal_pct[i][2];
        if (i == cal_sel && cal_sweeping) {
            cal_sweep_report();
            uint8_t v = cal_sweep_value();
            if (cal_chan == 0) pr = v; else if (cal_chan == 1) pg = v; else if (cal_chan == 2) pb = v; else pr = pg = pb = v;
        }
        rgb_matrix_set_color(i, (uint16_t)fr * pr / 100, (uint16_t)fg * pg / 100, (uint16_t)fb * pb / 100);
    }
    return false;
#endif
    bool master = is_keyboard_master();
    if (master ? lights_off : (osync_remote.flags & (8 | 32))) {
        return false;
    }
    uint8_t layer    = top_layer(layer_state);
    uint8_t v        = rgb_matrix_get_val();

    // Off-base layers own every LED: blank the range (underglow, trackball,
    // unmapped keys) so the effect never bleeds through, then paint keys.
    if (layer != 0) {
        for (uint8_t i = led_min; i < led_max; i++) rgb_matrix_set_color(i, 0, 0, 0);
    }
    uint8_t pulse = (uint16_t)v * (40 + scale8(sin8((uint8_t)(timer_read() >> 3)), 215)) / 255;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t idx = g_led_config.matrix_co[row][col];
            if (idx == NO_LED || idx < led_min || idx >= led_max) continue;
            uint8_t k = key_class(row, col);
            if (k == KC_LEAVE) continue;
            if (k == KC_PULSE) { rgb_matrix_set_color(idx, pulse, pulse, pulse); continue; }
            if (k == 0)        { rgb_matrix_set_color(idx, 0, 0, 0); continue; }
            uint8_t r = 80, g = 80, b = 80;
            layer_rgb(k, &r, &g, &b);
            apply_led_trim(idx, &r, &g, &b);
            rgb_matrix_set_color(idx, (uint16_t)r * v / 255, (uint16_t)g * v / 255, (uint16_t)b * v / 255);
        }
    }
    return false;
}
#endif

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode on the Settings layer (now layer 3).
    // Auto scroll mode on the Settings layer (3). Only touch it when entering
    // or leaving Settings: the auto-mouse layer flips on every trackball move,
    // and forcing scroll mode on each layer change cancelled a held SCRL_MO.
    static uint8_t prev_top = 0;
    uint8_t        top      = top_layer(state);
    if (top != prev_top) {
        if (top == 3) {
            keyball_set_scroll_mode(true);
        } else if (prev_top == 3) {
            keyball_set_scroll_mode(false);
        }
        prev_top = top;
    }
    // No Pimoroni LED write here: housekeeping applies it when the (non-mouse)
    // layer actually changes, keeping I2C out of the layer-change path.
    return state;
}
