/*
Copyright 2024 @smk (Pimoroni Trackball Integration for Keyball61 Left Half)

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

#include "quantum.h"
#include "drivers/sensors/pimoroni_trackball.h"

// Pimoroni trackball configuration for left half
static bool pimoroni_initialized = false;

// Initialize Pimoroni trackball on left half
void pimoroni_left_init(void) {
    // Only initialize on left half
    if (!is_keyboard_left()) {
        return;
    }

    // Initialize I2C and Pimoroni trackball
    i2c_init();
    pimoroni_trackball_device_init();

    // Set initial RGBW color (subtle white for left half)
    pimoroni_trackball_set_rgbw(20, 20, 20, 10);

    pimoroni_initialized = true;
}

// Read Pimoroni trackball and convert to keyball motion format
bool pimoroni_left_read_motion(int16_t *x, int16_t *y, uint8_t *click) {
    if (!pimoroni_initialized || !is_keyboard_left()) {
        return false;
    }

    // Read Pimoroni trackball data
    pimoroni_data_t data;
    i2c_status_t status = read_pimoroni_trackball(&data);
    if (status != I2C_STATUS_SUCCESS) {
        return false;
    }

    // Convert to keyball motion format
    *x = pimoroni_trackball_get_offsets(data.right, data.left, 3);  // Scale factor 3
    *y = pimoroni_trackball_get_offsets(data.down, data.up, 3);    // Scale factor 3
    *click = data.click;

    return true;
}

// Set RGBW color for Pimoroni trackball
void pimoroni_left_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (pimoroni_initialized) {
        pimoroni_trackball_set_rgbw(r, g, b, w);
    }
}