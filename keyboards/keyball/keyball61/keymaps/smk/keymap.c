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

// Custom split transaction carrying Pimoroni scroll/click to the master.
#include "transactions.h"
#include "pimoroni_split.h"

// Forward declarations for Pimoroni trackball integration
void pimoroni_left_init(void);
bool pimoroni_left_read_motion(int16_t *x, int16_t *y, uint8_t *click);
void pimoroni_left_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
static void pimoroni_apply_layer_color(uint8_t layer);

// Window-management helper: emit Ctrl+Opt+<key> for Rectangle shortcuts.
#define WM(kc) LCTL(LALT(kc))

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[0] = LAYOUT_universal(
    KC_GRV       , KC_1         , KC_2         , KC_3         , KC_4         , KC_5         ,                                 KC_6         , KC_7         , KC_8         , KC_9         , KC_0         , KC_MINS      ,
    KC_TAB       , KC_Q         , KC_W         , KC_E         , KC_R         , KC_T         ,                                 KC_Y         , KC_U         , KC_I         , KC_O         , KC_P         , KC_EQL       ,
    KC_CAPS      , KC_A         , KC_S         , KC_D         , KC_F         , KC_G         ,                                 KC_H         , KC_J         , KC_K         , KC_L         , KC_SCLN      , KC_ENT       ,
    KC_LSFT      , KC_Z         , KC_X         , KC_C         , KC_V         , KC_B         , KC_LBRC      ,   KC_RBRC      , KC_N         , KC_M         , KC_COMM      , KC_DOT       , KC_SLSH      , KC_RSFT      ,
    KC_LCTL      , KC_LALT      , _______      , _______      , KC_LGUI      , LT(1,KC_SPC) , KC_ESC       ,   KC_BSPC      , MO(2)        , _______      , _______      , _______      , TG(1)        , KC_BSLS
),

// Layer 1: Nav / Num. Hold left thumb (Space). MO(3) reaches Settings.
[1] = LAYOUT_universal(
    SSNP_FRE     , KC_F1        , KC_F2        , KC_F3        , KC_F4        , KC_F5        ,                                 KC_F6        , KC_F7        , KC_F8        , KC_F9        , KC_F10       , KC_F11       ,
    SSNP_VRT     , _______      , KC_7         , KC_8         , KC_9         , _______      ,                                 _______      , KC_LEFT      , KC_UP        , KC_RGHT      , _______      , KC_F12       ,
    SSNP_HOR     , _______      , KC_4         , KC_5         , KC_6         , S(KC_SCLN)   ,                                 KC_PGUP      , KC_BTN1      , KC_DOWN      , KC_BTN2      , KC_BTN3      , _______      ,
    _______      , _______      , KC_1         , KC_2         , KC_3         , S(KC_MINS)   , S(KC_8)      ,   S(KC_9)      , KC_PGDN      , _______      , _______      , _______      , _______      , _______      ,
    _______      , MO(3)        , KC_0         , KC_DOT       , _______      , _______      , SCRL_MO      ,   _______      , _______      , _______      , _______      , _______      , TG(1)        , TG(4)
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
    RGB_TOG      , AML_TO       , AML_I50      , AML_D50      , _______      , _______      ,                                 RGB_M_P      , RGB_M_B      , RGB_M_R      , RGB_M_SW     , RGB_M_SN     , RGB_M_K      ,
    RGB_MOD      , RGB_HUI      , RGB_SAI      , RGB_VAI      , _______      , _______      ,                                 RGB_M_X      , RGB_M_G      , RGB_M_T      , RGB_M_TW     , _______      , _______      ,
    RGB_RMOD     , RGB_HUD      , RGB_SAD      , RGB_VAD      , _______      , _______      ,                                 CPI_D1K      , CPI_D100     , CPI_I100     , CPI_I1K      , KBC_SAVE     , KBC_RST      ,
    _______      , _______      , SCRL_DVD     , SCRL_DVI     , SCRL_MO      , SCRL_TO      , EE_CLR       ,   EE_CLR       , KC_HOME      , KC_PGDN      , KC_PGUP      , KC_END       , _______      , _______      ,
    QK_BOOT      , _______      , KC_LEFT      , KC_DOWN      , KC_UP        , KC_RGHT      , _______      ,   _______      , KC_DEL       , _______      , _______      , _______      , _______      , QK_BOOT
),

// Layer 4: Gaming. Toggled via TG(4).
[4] = LAYOUT_universal(
    KC_ESC       , KC_1         , KC_2         , KC_3         , KC_4         , KC_5         ,                                 KC_6         , KC_7         , KC_8         , KC_9         , KC_0         , KC_GRV       ,
    KC_TAB       , KC_Q         , KC_W         , KC_E         , KC_R         , _______      ,                                 _______      , KC_LEFT      , KC_UP        , KC_RGHT      , _______      , KC_F12       ,
    KC_LCTL      , KC_A         , KC_S         , KC_D         , KC_F         , _______      ,                                 KC_PGUP      , KC_BTN1      , KC_DOWN      , KC_BTN2      , KC_BTN3      , _______      ,
    KC_LSFT      , KC_Z         , KC_X         , KC_C         , KC_V         , _______      , _______      ,   _______      , KC_PGDN      , _______      , _______      , _______      , _______      , _______      ,
    _______      , _______      , _______      , KC_SPC       , _______      , _______      , _______      ,   KC_DEL       , CPI_D1K      , CPI_D100     , CPI_I100     , CPI_I1K      , _______      , TG(4)
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

// Read the Pimoroni once and turn it into a scroll/click payload. The Pimoroni's
// motion registers are read-and-clear, so this MUST be the only place the ball
// is read each cycle. Whichever half physically has the ball calls this; the
// result is then either applied locally (ball on master) or shipped to the
// master over the split RPC (ball on slave).
static pimoroni_scroll_t pimoroni_compute_scroll(void) {
    static int16_t scroll_acc_v = 0;
    static int16_t scroll_acc_h = 0;
    const int16_t  SCROLL_DIVISOR = 8;

    pimoroni_scroll_t out = {0, 0, 0};

    int16_t x, y;
    uint8_t click;
    if (!pimoroni_left_read_motion(&x, &y, &click)) {
        return out;
    }

    out.click = click;

    // Layer 0 = dedicated scroll wheel. On other layers the ball is idle for
    // now (cursor work is done by the right-hand PMW3360).
    if (get_highest_layer(layer_state) == 0) {
        // Trackball is mounted rotated 90deg, so the ball's x axis is physical
        // up/down -> drive vertical scroll from x. Deltas come out fast (one
        // detent ~= 3, quick rolls ~= 12) because the driver squares the
        // offset, so accumulate and emit one tick per SCROLL_DIVISOR units;
        // small movements are kept (not dropped as integer division would).
        scroll_acc_v += -x; // Vertical scroll (natural rolling motion)
        scroll_acc_h += y;  // Horizontal scroll

        out.v = scroll_acc_v / SCROLL_DIVISOR;
        out.h = scroll_acc_h / SCROLL_DIVISOR;
        scroll_acc_v -= out.v * SCROLL_DIVISOR; // keep remainder
        scroll_acc_h -= out.h * SCROLL_DIVISOR;

        if (out.v != 0 || out.h != 0) {
            dprintf("Pimoroni SCROLL: h=%d, v=%d (x=%d y=%d)\n", out.h, out.v, x, y);
        }
    }

    return out;
}

// Latest Pimoroni payload, ready for the master to apply to its mouse report.
// On the slave this is filled by the RPC handler; on the master it is filled
// either by the RPC invoke (ball on slave) or directly (ball on master).
static pimoroni_scroll_t pimoroni_latest = {0, 0, 0};

#ifdef SPLIT_KEYBOARD
// Slave side: the master asks for the current Pimoroni scroll/click. We read
// the ball here (read-and-clear) and hand back the computed payload.
static void pimoroni_get_scroll_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    pimoroni_scroll_t s = pimoroni_compute_scroll();
    *(pimoroni_scroll_t *)out_data = s;
}
#endif

// Enable runtime debug so CONSOLE_ENABLE dprintf output is actually emitted,
// and register the slave-side Pimoroni RPC handler.
void keyboard_post_init_user(void) {
    debug_enable = true;
    debug_mouse  = true;

#ifdef SPLIT_KEYBOARD
    // Only the slave answers RPCs. The Pimoroni lives on the left half, so the
    // handler matters when the left is the slave (USB on the right).
    if (!is_keyboard_master()) {
        transaction_register_rpc(PIMORONI_GET_SCROLL, pimoroni_get_scroll_handler);
    }
#endif
}

// Apply the latest Pimoroni payload to the outgoing mouse report. Runs on the
// master so the data actually reaches the host.
report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    static bool last_click_state = false;

    if (is_keyboard_master()) {
        // If the ball is on THIS (master) half, read it directly. Otherwise the
        // payload was already pulled from the slave in housekeeping_task_user.
        if (is_keyboard_left()) {
            pimoroni_latest = pimoroni_compute_scroll();
        }

        // Inject scroll, then consume it so the same delta is not re-applied on
        // the next report (which would scroll forever). New motion accumulates
        // into pimoroni_latest between reports via the local read / RPC poll.
        mouse_report.h = pimoroni_latest.h;
        mouse_report.v = pimoroni_latest.v;
        pimoroni_latest.h = 0;
        pimoroni_latest.v = 0;

        // Edge-detect the click so we set/clear the button cleanly.
        bool current_click_state = (pimoroni_latest.click != 0);
        if (current_click_state != last_click_state) {
            if (current_click_state) {
                mouse_report.buttons |= MOUSE_BTN1; // Left mouse button
                dprintf("Pimoroni: Left mouse PRESSED\n");
            } else {
                mouse_report.buttons &= ~MOUSE_BTN1;
                dprintf("Pimoroni: Left mouse RELEASED\n");
            }
            last_click_state = current_click_state;
        } else if (current_click_state) {
            // Hold the button while pressed across reports.
            mouse_report.buttons |= MOUSE_BTN1;
        }
    }

    return mouse_report;
}

// Master side: pull the Pimoroni scroll/click from the slave every few ms so
// pointing_device_task_kb has fresh data to inject. Only needed when the ball
// is on the OTHER half (i.e. left is the slave). When the ball is on the master
// itself, pointing_device_task_kb reads it directly and this is skipped.
void housekeeping_task_user(void) {
    // Mirror the RGB on/off state (toggled by RGB_TOG on layer 3) onto the OLED
    // and the Pimoroni LED. RGB enable is synced across the split, so this runs
    // correctly on both halves and reacts even without a layer change.
    static bool last_rgb_on = true;
    bool rgb_on = rgblight_is_enabled();
    if (rgb_on != last_rgb_on) {
        last_rgb_on = rgb_on;
#ifdef OLED_ENABLE
        if (rgb_on) {
            oled_on();
        } else {
            oled_clear();
            oled_off();
        }
#endif
        pimoroni_apply_layer_color(get_highest_layer(layer_state));
    }

#ifdef SPLIT_KEYBOARD
    if (is_keyboard_master() && !is_keyboard_left()) {
        static uint32_t last_sync = 0;
        uint32_t        now       = timer_read32();
        if (TIMER_DIFF_32(now, last_sync) < 4) {
            return;
        }
        last_sync = now;

        pimoroni_scroll_t recv = {0, 0, 0};
        if (transaction_rpc_exec(PIMORONI_GET_SCROLL, 0, NULL, sizeof(recv), &recv)) {
            // Accumulate scroll so deltas are never lost if the pointing task
            // has not consumed the previous poll yet. Click is a level, not a
            // delta, so take it as-is.
            pimoroni_latest.h     += recv.h;
            pimoroni_latest.v     += recv.v;
            pimoroni_latest.click = recv.click;
        }
    }
#endif
}

// Enhanced OLED rendering for dual trackball
#ifdef OLED_ENABLE
void oledkit_render_info_user(void) {
    // When RGB is toggled off (RGB_TOG on layer 3), keep the OLED dark too.
    if (!rgblight_is_enabled()) {
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

// Set the Pimoroni trackball LED for the given layer -- or turn it fully off
// when RGB lighting is disabled (RGB_TOG on layer 3). Only acts on the half
// that has the Pimoroni (left). RGB enable state is synced across the split,
// so rgblight_is_enabled() is valid on both halves.
static void pimoroni_apply_layer_color(uint8_t layer) {
    if (!is_keyboard_left()) {
        return;
    }
    if (!rgblight_is_enabled()) {
        pimoroni_left_set_rgbw(0, 0, 0, 0); // LED off with the rest of the RGB
        return;
    }
    // Colors match the cheatsheet per-layer accents. Base layer (0) keeps the
    // LED off so the trackball is dark during normal typing.
    switch (layer) {
        case 0: pimoroni_left_set_rgbw(0, 0, 0, 0);        break; // L0 base    - off
        case 1: pimoroni_left_set_rgbw(57, 217, 138, 10);  break; // L1 nav     - green  #39d98a
        case 2: pimoroni_left_set_rgbw(0, 220, 220, 10);   break; // L2 window-mgmt - cyan
        case 3: pimoroni_left_set_rgbw(255, 157, 51, 10);  break; // L3 settings - orange #ff9d33
        case 4: pimoroni_left_set_rgbw(255, 77, 77, 10);   break; // L4 gaming  - red    #ff4d4d
        default: pimoroni_left_set_rgbw(0, 0, 0, 0);       break; // off
    }
}

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode on the Settings layer (now layer 3).
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    pimoroni_apply_layer_color(get_highest_layer(state));
    return state;
}
