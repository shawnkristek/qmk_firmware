/*
This is the c configuration file for the keymap

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

#pragma once

#ifdef RGBLIGHT_ENABLE
#    define RGBLIGHT_EFFECT_BREATHING
#    define RGBLIGHT_EFFECT_RAINBOW_MOOD
#    define RGBLIGHT_EFFECT_RAINBOW_SWIRL
#    define RGBLIGHT_EFFECT_SNAKE
#    define RGBLIGHT_EFFECT_KNIGHT
#    define RGBLIGHT_EFFECT_CHRISTMAS
#    define RGBLIGHT_EFFECT_STATIC_GRADIENT
#    define RGBLIGHT_EFFECT_RGB_TEST
#    define RGBLIGHT_EFFECT_ALTERNATING
#    define RGBLIGHT_EFFECT_TWINKLE
#endif

#define TAP_CODE_DELAY 5

#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define AUTO_MOUSE_DEFAULT_LAYER 1

// Custom split transaction to carry the left-half Pimoroni scroll/click to the
// master half (see pimoroni_split.h).
#define SPLIT_TRANSACTION_IDS_USER PIMORONI_GET_SCROLL

#define DYNAMIC_KEYMAP_LAYER_COUNT 8

// Dual Trackball Configuration (PMW3360 + Pimoroni)
#ifdef POINTING_DEVICE_ENABLE

// I2C configuration for Pimoroni trackball (left half)
#define I2C_SDA_PIN GP2
#define I2C_SCL_PIN GP3

// Pimoroni trackball configuration
#define PIMORONI_TRACKBALL_ADDRESS 0x0A
#define PIMORONI_TRACKBALL_SCALE 3
#define PIMORONI_TRACKBALL_DEBOUNCE_CYCLES 20
#define PIMORONI_TRACKBALL_ERROR_COUNT 10
#define PIMORONI_TRACKBALL_TIMEOUT 100

// Pimoroni trackball orientation for left half installation
#define PIMORONI_TRACKBALL_INVERT_Y
// #define PIMORONI_TRACKBALL_ROTATE  // Uncomment if trackball needs 90-degree rotation

// Dual trackball specific settings
#define DUAL_TRACKBALL_ENABLE
#define PIMORONI_PRIMARY_SCROLL  // Use Pimoroni for precision scrolling

#endif // POINTING_DEVICE_ENABLE
