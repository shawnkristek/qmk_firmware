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

// Process Pimoroni trackball data in keyball's housekeeping task
void housekeeping_task_kb_user(void) {
    // Process Pimoroni trackball data on left half
    // Add to keyball's motion system for split communication
    int16_t x, y;
    uint8_t click;

    // Read Pimoroni trackball data if available
    if (pimoroni_left_read_motion(&x, &y, &click)) {
        // Add to keyball's motion system (this will be shared in split communication)
        if (x != 0 || y != 0) {
            keyball.this_motion.x = add16(keyball.this_motion.x, x);
            keyball.this_motion.y = add16(keyball.this_motion.y, y);
        }
    }
}

// Handle Pimoroni clicks in keyball's pointing device task
report_mouse_t pointing_device_task_kb_user(report_mouse_t mouse_report) {
    // Handle Pimoroni click on master (right half with USB)
    if (!is_keyboard_left()) {
        static bool last_click_state = false;
        bool current_click_state = false;

        // Check if Pimoroni has click data
        int16_t x, y;
        uint8_t click;
        if (pimoroni_left_read_motion(&x, &y, &click)) {
            current_click_state = (click != 0);
        }

        // Only update if click state changed
        if (current_click_state != last_click_state) {
            if (current_click_state) {
                mouse_report.buttons |= MOUSE_BTN1;
            } else {
                mouse_report.buttons &= ~MOUSE_BTN1;
            }
            last_click_state = current_click_state;
        }
    }

    // Return modified mouse report
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
        oled_write_P(PSTR("L:Pimoroni"), false);
    } else {
        oled_write_P(PSTR("R:PMW3360"), false);
    }
}
#endif

// Set Pimoroni trackball RGB based on layer and mode
layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    // Set Pimoroni RGB based on current layer (only on left half)
    if (is_keyboard_left()) {
        uint8_t layer = get_highest_layer(state);

        // Check if we're in scroll mode
        bool scroll_mode = keyball_get_scroll_mode();

        if (scroll_mode) {
            // Purple when in scroll mode
            pimoroni_left_set_rgbw(150, 0, 150, 10);
        } else {
            switch (layer) {
                case 0: // Base layer - subtle white
                    pimoroni_left_set_rgbw(20, 20, 20, 10);
                    break;
                case 1: // Symbol layer - blue
                    pimoroni_left_set_rgbw(0, 50, 100, 5);
                    break;
                case 2: // Mouse layer - green
                    pimoroni_left_set_rgbw(0, 100, 50, 5);
                    break;
                case 3: // RGB layer - orange
                    pimoroni_left_set_rgbw(200, 100, 0, 5);
                    break;
                case 5: // Trading layer - red/orange
                    pimoroni_left_set_rgbw(200, 50, 0, 10);
                    break;
                default: // Other layers - cyan
                    pimoroni_left_set_rgbw(0, 150, 150, 5);
                    break;
            }
        }
    }

    return state;
}
