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
static void pimoroni_apply_layer_color(uint8_t layer);

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
enum custom_keycodes {
    LIGHTS = QK_USER,
    PIM_MODE,
};

// When true the Pimoroni acts as a cursor (second pointer) instead of a scroll
// wheel. Toggled by PIM_MODE.
static bool pimoroni_cursor_mode = false;

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
// 37-73 = RIGHT half (underglow 37-44, keys 45-71, trackball 72-73).
// matrix_co maps each [row][col] to its LED index (NO_LED where unmapped).
// clang-format off
led_config_t g_led_config = { {
    { 24, 19, 14, NO_LED, 9, 5, 1, NO_LED },
    { 25, 20, 15, NO_LED, 10, 6, 2, NO_LED },
    { 26, 21, 16, NO_LED, 11, 7, 3, NO_LED },
    { 27, 22, 17, NO_LED, 12, 8, 4, 0 },
    { 28, 23, 18, NO_LED, 13, NO_LED, NO_LED, NO_LED },
    { 45, 50, 55, NO_LED, 59, 63, 67, NO_LED },
    { 46, 51, 56, NO_LED, 60, 64, 68, NO_LED },
    { 47, 52, 57, NO_LED, 61, 65, 69, NO_LED },
    { 48, 53, 58, NO_LED, 62, 66, 70, 71 },
    { 49, 54, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
}, {
    { 103, 48 }, {  86,  0 }, {  86, 16 }, {  86, 32 }, {  86, 48 },
    {  68,  0 }, {  68, 16 }, {  68, 32 }, {  68, 48 },
    {  51,  0 }, {  51, 16 }, {  51, 32 }, {  51, 48 }, {  51, 64 },
    {  34,  0 }, {  34, 16 }, {  34, 32 }, {  34, 48 }, {  34, 64 },
    {  17,  0 }, {  17, 16 }, {  17, 32 }, {  17, 48 }, {  17, 64 },
    {   0,  0 }, {   0, 16 }, {   0, 32 }, {   0, 48 }, {   0, 64 },
    {   0, 64 }, {  14, 64 }, {  28, 64 }, {  42, 64 }, {  57, 64 }, {  71, 64 }, {  85, 64 }, { 100, 64 },
    { 124, 64 }, { 138, 64 }, { 152, 64 }, { 166, 64 }, { 181, 64 }, { 195, 64 }, { 209, 64 }, { 224, 64 },
    { 224,  0 }, { 224, 16 }, { 224, 32 }, { 224, 48 }, { 224, 64 },
    { 206,  0 }, { 206, 16 }, { 206, 32 }, { 206, 48 }, { 206, 64 },
    { 189,  0 }, { 189, 16 }, { 189, 32 }, { 189, 48 },
    { 172,  0 }, { 172, 16 }, { 172, 32 }, { 172, 48 },
    { 155,  0 }, { 155, 16 }, { 155, 32 }, { 155, 48 },
    { 137,  0 }, { 137, 16 }, { 137, 32 }, { 137, 48 },
    { 120, 48 }, { 180, 56 }, { 180, 56 }
}, {
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW
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
    if (get_highest_layer(layer_state) == 0) {
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
    pimoroni_apply_layer_color(get_highest_layer(layer_state));
}

void keyboard_post_init_user(void) {
    last_activity = timer_read32();  // don't sleep immediately at boot
}

// Read the Pimoroni locally on the half that has it and inject scroll/click.
// With USB on the left, that half is the master, so this reaches the host
// directly -- no split transport for the Pimoroni.
report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    static bool last_click_state = false;

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
    uint8_t row = record->event.key.row, col = record->event.key.col;
    // Bottom-row matrix columns are 0,1,2,4,5,6,7 (col 3 is unused):
    // Ctrl=0 Alt=1 Left=2 Right=4 GUI=5 Space=6 Esc=7.
    if (row == 4) {
        switch (col) {
            case 0: cal_chan = 0; uprintf("CAL channel R\n"); return false;
            case 1: cal_chan = 1; uprintf("CAL channel G\n"); return false;
            case 4: cal_chan = 2; uprintf("CAL channel B\n"); return false;
            case 6: // Space: lock the swept value
                if (cal_sel != NO_LED && cal_sweeping) {
                    cal_pct[cal_sel][cal_chan] = cal_sweep_value();
                    cal_sweeping = false;
                    uprintf("CAL LOCK idx %u -> r=%u g=%u b=%u\n", cal_sel, cal_pct[cal_sel][0], cal_pct[cal_sel][1], cal_pct[cal_sel][2]);
                }
                return false;
            case 7: cal_dump(); return false; // Esc
            default: break;
        }
    }
    uint8_t idx = g_led_config.matrix_co[row][col];
    if (idx != NO_LED) {
        cal_sel = idx; cal_sweeping = true; cal_sweep_t0 = timer_read32();
        uprintf("CAL sweep idx %u (row %u col %u) channel %c\n", idx, row, col, "RGB"[cal_chan]);
    }
    return false;
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef LED_CAL
    return cal_process(record);
#endif
    if (record->event.pressed) {
        last_activity = timer_read32();
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
    uint8_t cur_layer = get_highest_layer(layer_state);
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
void oledkit_render_info_user(void) {
    // Keep the OLED dark when the lights are off (idle sleep / manual all-off).
    if (lights_off) {
        return;
    }
    keyball_oled_render_keyinfo();
    keyball_oled_render_ballinfo();
    keyball_oled_render_layerinfo();

    // Add dual trackball status indicators
    oled_set_cursor(0, 3);
    uint8_t hl = get_highest_layer(layer_state);
    char dbg[10] = {0};
    dbg[0] = is_keyboard_left() ? 'L' : 'R';
    dbg[1] = ' ';
    dbg[2] = 'H';
    dbg[3] = 'L';
    dbg[4] = ':';
    dbg[5] = '0' + (hl % 10);
    oled_write(dbg, false);
}
#endif

// Set the Pimoroni trackball LED for the given layer. Forced off when the
// lights are off (idle sleep or manual all-off). Only acts on the half that
// has the Pimoroni (left).
static void pimoroni_apply_layer_color(uint8_t layer) {
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
    switch (layer) {
        case 0: pimoroni_left_set_rgbw(0, 0, 0, 0);        break; // L0 base - off
        case 1: pimoroni_left_set_rgbw(57, 217, 138, 10);  break; // L1 nav     - green  #39d98a
        case 2: pimoroni_left_set_rgbw(0, 220, 220, 10);   break; // L2 window-mgmt - cyan
        case 3: pimoroni_left_set_rgbw(255, 157, 51, 10);  break; // L3 settings - orange #ff9d33
        case 4: pimoroni_left_set_rgbw(255, 77, 77, 10);   break; // L4 gaming  - red    #ff4d4d
        default: pimoroni_left_set_rgbw(0, 0, 0, 0);       break; // off
    }
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
        // Fully saturated hues: any white component (the cheatsheet's pastel
        // values) washes out on the key LEDs, and blue reads strong through
        // the caps, so keep blue low where it is not the point of the color.
        case 1: *r = 0;   *g = 255; *b = 60;  return true; // nav   - green
        case 2: *r = 0;   *g = 200; *b = 255; return true; // wm    - cyan
        case 3: *r = 255; *g = 90;  *b = 0;   return true; // set   - orange
        case 4: *r = 255; *g = 0;   *b = 0;   return true; // game  - red
        default: return false;                              // base/other: effect
    }
}

// Per-LED color trim, in percent, to even out LED-to-LED hue variance on
// specific keys. Applied to indicator colors only (effects are untouched).
// 100 = no change. Tune by eye on a solid-color layer (Gaming = pure red).
typedef struct { uint8_t idx; uint8_t r, g, b; } led_trim_t;
static const led_trim_t led_trim[] = {
    // These five LEDs run brighter and warmer than the rest; values matched by
    // eye on a white field (swept per channel on B, applied to all five).
    { 24, 45, 72, 58 }, // grave
    { 20, 45, 72, 58 }, // Q
    { 16, 45, 72, 58 }, // S
    {  4, 45, 72, 58 }, // B
    { 23, 45, 72, 58 }, // Alt
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
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#ifdef LED_CAL
    if (!cal_init) {
        memset(cal_pct, 100, sizeof(cal_pct));
        for (uint8_t i = 0; i < sizeof(led_trim) / sizeof(led_trim[0]); i++) {
            cal_pct[led_trim[i].idx][0] = led_trim[i].r; cal_pct[led_trim[i].idx][1] = led_trim[i].g; cal_pct[led_trim[i].idx][2] = led_trim[i].b;
        }
        cal_init = true;
    }
    for (uint8_t i = led_min; i < led_max; i++) {
        const uint8_t w = 120;
        uint8_t pr = cal_pct[i][0], pg = cal_pct[i][1], pb = cal_pct[i][2];
        if (i == cal_sel && cal_sweeping) {
            cal_sweep_report();
            uint8_t v = cal_sweep_value();
            if (cal_chan == 0) pr = v; else if (cal_chan == 1) pg = v; else pb = v;
        }
        rgb_matrix_set_color(i, (uint16_t)w * pr / 100, (uint16_t)w * pg / 100, (uint16_t)w * pb / 100);
    }
    return false;
#endif
    if (lights_off) {
        return false;
    }
    uint8_t layer = get_highest_layer(layer_state);
    if (layer == 0) {
        return false; // base layer: let the effect run untouched
    }
    uint8_t lr = 80, lg = 80, lb = 80; // fallback (any non-color layer)
    layer_rgb(layer, &lr, &lg, &lb);

    // Blank every LED in this frame's range first (underglow, trackball,
    // unmapped keys) so the base-layer effect never bleeds through, then paint
    // only the mapped keys below. The effect keeps running underneath, so no
    // mode/brightness juggling is needed when returning to the base layer.
    for (uint8_t i = led_min; i < led_max; i++) {
        rgb_matrix_set_color(i, 0, 0, 0);
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t idx = g_led_config.matrix_co[row][col];
            if (idx == NO_LED || idx < led_min || idx >= led_max) {
                continue;
            }
            uint16_t kc = keymaps[layer][row][col];
            if (kc == KC_NO || kc == KC_TRANSPARENT) {
                rgb_matrix_set_color(idx, 0, 0, 0); // unmapped key: off
                continue;
            }
            // Layer-switch keys glow the destination layer's color.
            uint8_t dest = 0xFF;
            if (kc >= QK_MOMENTARY && kc <= QK_MOMENTARY_MAX)        dest = QK_MOMENTARY_GET_LAYER(kc);
            else if (kc >= QK_TOGGLE_LAYER && kc <= QK_TOGGLE_LAYER_MAX) dest = QK_TOGGLE_LAYER_GET_LAYER(kc);
            else if (kc >= QK_TO && kc <= QK_TO_MAX)                 dest = QK_TO_GET_LAYER(kc);
            else if (kc >= QK_LAYER_TAP && kc <= QK_LAYER_TAP_MAX)   dest = QK_LAYER_TAP_GET_LAYER(kc);

            uint8_t r = lr, g = lg, b = lb;
            uint8_t dr, dg, db;
            if (dest != 0xFF && layer_rgb(dest, &dr, &dg, &db)) {
                r = dr; g = dg; b = db;
            }
            // Scale to the matrix brightness (capped by
            // RGB_MATRIX_MAXIMUM_BRIGHTNESS). Raw 0-255 colors wash out to
            // near-white through the keycaps.
            apply_led_trim(idx, &r, &g, &b);
            uint8_t v = rgb_matrix_get_val();
            rgb_matrix_set_color(idx, (uint16_t)r * v / 255, (uint16_t)g * v / 255, (uint16_t)b * v / 255);
        }
    }
    return false;
}
#endif

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode on the Settings layer (now layer 3).
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    pimoroni_apply_layer_color(get_highest_layer(state));
    return state;
}
