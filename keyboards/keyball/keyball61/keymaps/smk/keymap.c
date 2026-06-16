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

// Forward declarations for keyball internal functions
extern int16_t add16(int16_t a, int16_t b);

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[0] = LAYOUT_universal(
    KC_GRV   , KC_1     , KC_2     , KC_3     , KC_4     , KC_5     ,                                  KC_6     , KC_7     , KC_8     , KC_9     , KC_0     , KC_MINS  ,
    KC_TAB   , KC_Q     , KC_W     , KC_E     , KC_R     , KC_T     ,                                  KC_Y     , KC_U     , KC_I     , KC_O     , KC_P     , KC_EQL   ,
    KC_CAPS  , KC_A     , KC_S     , KC_D     , KC_F     , KC_G     ,                                  KC_H     , KC_J     , KC_K     , KC_L     , KC_SCLN  ,LT(1,KC_ENT),
    LT(1,KC_LSFT),KC_Z  , KC_X     , KC_C     , KC_V     , KC_B     , KC_LBRC  ,              KC_RBRC, KC_N     , KC_M     , KC_COMM  , KC_DOT   , KC_SLSH  , KC_QUOT  ,
    KC_LCTL  , KC_LALT  , KC_LEFT  , KC_UP    , KC_LGUI  , LT(2,KC_SPC),LT(3,KC_ESC),      LT(3,KC_BSPC),LT(2,KC_SPC),_______,_______ , _______  , TG(2)    , KC_BSLS
),

[1] = LAYOUT_universal(
    S(KC_GRV), S(KC_1)  , S(KC_2)  , S(KC_3)  , S(KC_4)  , S(KC_5)  ,                                  S(KC_6)  , S(KC_7)  , S(KC_8)  , S(KC_9)  , S(KC_0)  , S(KC_MINS),
    S(KC_TAB), S(KC_Q)  , S(KC_W)  , S(KC_E)  , S(KC_R)  , S(KC_T)  ,                                  S(KC_Y)  , S(KC_U)  , S(KC_I)  , S(KC_O)  , S(KC_P)  , S(KC_EQL) ,
    _______  , S(KC_A)  , S(KC_S)  , S(KC_D)  , S(KC_F)  , S(KC_G)  ,                                  S(KC_H)  , S(KC_J)  , S(KC_K)  , S(KC_L)  , S(KC_SCLN),  _______ ,
    _______  , S(KC_Z)  , S(KC_X)  , S(KC_C)  , S(KC_V)  , S(KC_B)  , S(KC_LBRC),           S(KC_RBRC), S(KC_N)  , S(KC_M)  , S(KC_COMM), S(KC_DOT), S(KC_SLSH), S(KC_QUOT),
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , S(KC_BSLS)
),

[2] = LAYOUT_universal(
    SSNP_FRE , KC_F1    , KC_F2    , KC_F3    , KC_F4    , KC_F5    ,                                  KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
    SSNP_VRT , _______  , KC_7     , KC_8     , KC_9     , _______  ,                                  _______  , KC_LEFT  , KC_UP    , KC_RGHT  , _______  , KC_F12   ,
    SSNP_HOR , _______  , KC_4     , KC_5     , KC_6     , S(KC_SCLN),                                 KC_PGUP  , KC_BTN1  , KC_DOWN  , KC_BTN2  , KC_BTN3  , _______  ,
    _______  , _______  , KC_1     , KC_2     , KC_3     , S(KC_MINS), S(KC_8)  ,            S(KC_9)  , KC_PGDN  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , KC_0     , KC_DOT   , _______  , _______  , SCRL_MO  ,            _______   , _______  , _______  , _______  , _______  , TG(2)    , TG(4)
),

[3] = LAYOUT_universal(
    RGB_TOG  , AML_TO   , AML_I50  , AML_D50  , _______  , _______  ,                                  RGB_M_P  , RGB_M_B  , RGB_M_R  , RGB_M_SW , RGB_M_SN , RGB_M_K  ,
    RGB_MOD  , RGB_HUI  , RGB_SAI  , RGB_VAI  , _______  , _______  ,                                  RGB_M_X  , RGB_M_G  , RGB_M_T  , RGB_M_TW , _______  , _______  ,
    RGB_RMOD , RGB_HUD  , RGB_SAD  , RGB_VAD  , _______  , _______  ,                                  CPI_D1K  , CPI_D100 , CPI_I100 , CPI_I1K  , KBC_SAVE , KBC_RST  ,
    _______  , _______  , SCRL_DVD , SCRL_DVI , SCRL_MO  , SCRL_TO  , EE_CLR   ,            EE_CLR   , KC_HOME  , KC_PGDN  , KC_PGUP  , KC_END   , _______  , _______  ,
    QK_BOOT  , _______  , KC_LEFT  , KC_DOWN  , KC_UP    , KC_RGHT  , _______  ,            _______   , KC_DEL   , _______  , _______  , _______  , _______  , QK_BOOT
),

[4] = LAYOUT_universal(
    KC_ESC   , KC_1     , KC_2     , KC_3     , KC_4     , KC_5     ,                                  KC_6     , KC_7     , KC_8     , KC_9     , KC_0     , KC_GRV   ,
    KC_TAB   , KC_Q     , KC_W     , KC_E     , KC_R     , _______  ,                                  _______  , KC_LEFT  , KC_UP    , KC_RGHT  , _______  , KC_F12   ,
    KC_LCTL  , KC_A     , KC_S     , KC_D     , KC_F     , _______  ,                                  KC_PGUP  , KC_BTN1  , KC_DOWN  , KC_BTN2  , KC_BTN3  , _______  ,
    KC_LSFT  , KC_Z     , KC_X     , KC_C     , KC_V     , _______  , _______  ,            _______   , KC_PGDN  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , KC_SPC   , _______  , _______  , _______  ,            KC_DEL    , CPI_D1K  , CPI_D100 , CPI_I100 , CPI_I1K  , _______  , TG(4)
),

[5] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______
),

[6] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______
),

[7] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______   , _______  , _______  , _______  , _______  , _______  , _______
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

// Enable runtime debug so CONSOLE_ENABLE dprintf output is actually emitted.
void keyboard_post_init_user(void) {
    debug_enable   = true;
    debug_mouse    = true;
}

// NOTE: The Pimoroni's motion registers are read-and-clear, so it must be read
// from exactly ONE place. That owner is pointing_device_task_kb() below. Do not
// add a second read here (an earlier debug "test" read in this task was stealing
// the deltas, leaving the pointing task with x=0/y=0 and no scroll).

// Split pointing device callback - runs on each half independently
report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    static bool last_click_state = false;
    bool current_click_state = false;

    // Only process Pimoroni on the left half
    if (is_keyboard_left()) {
        // Read Pimoroni trackball data
        int16_t x, y;
        uint8_t click;
        if (pimoroni_left_read_motion(&x, &y, &click)) {
            dprintf("Left half Pimoroni motion: x=%d, y=%d, click=%d\n", x, y, click);

            current_click_state = (click != 0);

            // Handle scroll mode on layer 0 (base layer)
            uint8_t current_layer = get_highest_layer(layer_state);
            dprintf("Left half layer: %d\n", current_layer);

            if (current_layer == 0) {
                // Layer 0: Dedicated scroll wheel mode.
                // Trackball is mounted rotated 90deg, so the ball's x axis is
                // physical up/down -> drive vertical scroll from x.
                //
                // The raw deltas come out fast (one detent ~= 3, quick rolls
                // ~= 12) because the driver squares the offset. Accumulate the
                // motion and only emit one scroll tick per SCROLL_DIVISOR units,
                // so small movements aren't dropped (as integer division would)
                // but overall scroll speed is reduced.
                static int16_t scroll_acc_v = 0;
                static int16_t scroll_acc_h = 0;
                const int16_t SCROLL_DIVISOR = 8;

                scroll_acc_v += -x; // Vertical scroll (natural rolling motion)
                scroll_acc_h += y;  // Horizontal scroll

                mouse_report.v = scroll_acc_v / SCROLL_DIVISOR;
                mouse_report.h = scroll_acc_h / SCROLL_DIVISOR;
                scroll_acc_v -= mouse_report.v * SCROLL_DIVISOR; // keep remainder
                scroll_acc_h -= mouse_report.h * SCROLL_DIVISOR;

                // Suppress cursor movement entirely in scroll mode so the ball
                // does not also drag the pointer from the combined report.
                mouse_report.x = 0;
                mouse_report.y = 0;
                if (mouse_report.v != 0 || mouse_report.h != 0) {
                    dprintf("Left half SCROLL: h=%d, v=%d\n", mouse_report.h, mouse_report.v);
                }
            } else {
                // Other layers: Mouse cursor mode
                if (x != 0 || y != 0) {
                    mouse_report.x = x;
                    mouse_report.y = -y; // Invert Y for natural movement
                    dprintf("Left half MOUSE: x=%d, y=%d\n", mouse_report.x, mouse_report.y);
                }
            }

            // Convert Pimoroni click to left mouse button
            if (current_click_state != last_click_state) {
                if (current_click_state) {
                    mouse_report.buttons |= MOUSE_BTN1;  // Left mouse button
                    dprintf("Left half: Left mouse PRESSED\n");
                } else {
                    mouse_report.buttons &= ~MOUSE_BTN1;
                    dprintf("Left half: Left mouse RELEASED\n");
                }
                last_click_state = current_click_state;
            }
        }
    }

    return mouse_report;
}

// Enhanced OLED rendering for dual trackball
#ifdef OLED_ENABLE
void oledkit_render_info_user(void) {
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

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    uint8_t layer = get_highest_layer(state);
    dprintf("Layer change: Current layer = %d\n", layer);

    // Set Pimoroni RGB based on current layer (only on left half)
    if (is_keyboard_left()) {
        dprintf("Setting Pimoroni RGB for layer %d on left half\n", layer);

        // Set RGB based on current mode
        if (layer == 0) {
            // Layer 0: Scroll mode - Blue color
            pimoroni_left_set_rgbw(0, 100, 255, 10);
            dprintf("Pimoroni: Setting BLUE (scroll mode) for layer 0\n");
        } else {
            // Other layers: Mouse mode - Green color
            pimoroni_left_set_rgbw(0, 255, 100, 10);
            dprintf("Pimoroni: Setting GREEN (mouse mode) for layer %d\n", layer);
        }

        // Optional: Add layer-specific variations
        switch (layer) {
            case 1: // Symbols layer - Cyan
                pimoroni_left_set_rgbw(0, 200, 200, 15);
                dprintf("Pimoroni: Setting CYAN for layer 1 (Symbols)\n");
                break;
            case 2: // Media/Mouse layer - Bright green
                pimoroni_left_set_rgbw(0, 255, 50, 15);
                dprintf("Pimoroni: Setting BRIGHT GREEN for layer 2 (Media)\n");
                break;
            case 3: // RGB/Settings layer - Orange
                pimoroni_left_set_rgbw(255, 150, 0, 15);
                dprintf("Pimoroni: Setting ORANGE for layer 3 (Settings)\n");
                break;
            case 4: // Gaming layer - Red
                pimoroni_left_set_rgbw(255, 50, 0, 15);
                dprintf("Pimoroni: Setting RED for layer 4 (Gaming)\n");
                break;
            case 5: // Trading layer - Purple
                pimoroni_left_set_rgbw(200, 0, 200, 15);
                dprintf("Pimoroni: Setting PURPLE for layer 5 (Trading)\n");
                break;
            default:
                // Default colors based on mode
                if (layer == 0) {
                    pimoroni_left_set_rgbw(0, 100, 255, 10); // Scroll mode blue
                    dprintf("Pimoroni: Setting default BLUE for layer 0\n");
                } else {
                    pimoroni_left_set_rgbw(0, 255, 100, 10); // Mouse mode green
                    dprintf("Pimoroni: Setting default GREEN for layer %d\n", layer);
                }
                break;
        }
    } else {
        dprintf("Right half detected, not setting Pimoroni RGB\n");
    }

    return state;
}
