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
static void apply_underglow_color(uint8_t layer);

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
#ifdef RGBLIGHT_ENABLE
    if (lights_off) {
        rgblight_disable_noeeprom();
    } else {
        rgblight_enable_noeeprom();
        apply_underglow_color(get_highest_layer(layer_state)); // restore per-layer color/effect
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
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
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
    // Writing the LED over I2C every cycle floods the bus (it can wedge, taking
    // scroll down with it). Only write when the layer or lights state changes.
    static uint8_t last_layer = 0xFF;
    static int8_t  last_off   = -1;
    static int8_t  last_cur   = -1;
    uint8_t cur_layer = get_highest_layer(layer_state);
    if (cur_layer != last_layer || (int8_t)lights_off != last_off
        || (int8_t)pimoroni_cursor_mode != last_cur) {
        bool layer_changed = (cur_layer != last_layer);
        last_layer = cur_layer;
        last_off   = (int8_t)lights_off;
        last_cur   = (int8_t)pimoroni_cursor_mode;
        pimoroni_apply_layer_color(cur_layer);
        if (layer_changed) {
            apply_underglow_color(cur_layer);
        }
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
    if (is_keyboard_left()) {
        uint8_t layer = get_highest_layer(layer_state);
        if (layer == 0) {
            oled_write_P(PSTR("L:Scroll"), false);
        } else {
            oled_write_P(PSTR("L:Mouse"), false);
        }
    } else {
        oled_write_P(PSTR("R:PMW3360"), false);
    }
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
static void apply_underglow_color(uint8_t layer) {
#ifdef RGBLIGHT_ENABLE
    if (lights_off) {
        return; // handled by apply_lights (RGB disabled entirely)
    }
    // QMK hue is 0-255. green~85, cyan~128, orange~17, red~0.
    switch (layer) {
        case 0: // base: animated effect
            rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_SWIRL);
            break;
        case 1: // nav - green
            rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
            rgblight_sethsv_noeeprom(85, 255, RGBLIGHT_LIMIT_VAL);
            break;
        case 2: // window-mgmt - cyan
            rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
            rgblight_sethsv_noeeprom(128, 255, RGBLIGHT_LIMIT_VAL);
            break;
        case 3: // settings - orange
            rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
            rgblight_sethsv_noeeprom(17, 255, RGBLIGHT_LIMIT_VAL);
            break;
        case 4: // gaming - red
            rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
            rgblight_sethsv_noeeprom(0, 255, RGBLIGHT_LIMIT_VAL);
            break;
        default:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_SWIRL);
            break;
    }
#endif
}

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode on the Settings layer (now layer 3).
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    pimoroni_apply_layer_color(get_highest_layer(state));
    return state;
}
