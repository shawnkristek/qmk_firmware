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
// Hard failure cutoff: a disconnected/dead Pimoroni makes every I2C read time
// out, which stalls the whole main loop (laggy typing). After this many
// consecutive failures we give up entirely so the keyboard stays responsive.
#define PIMORONI_FAIL_LIMIT 20
static uint8_t pimoroni_fail_count = 0;

// Initialize Pimoroni trackball on left half
void pimoroni_left_init(void) {
    // Always try to initialize - it only works on the half with the hardware.
    i2c_init();
    pimoroni_trackball_device_init();
    pimoroni_initialized = true;
    pimoroni_trackball_set_rgbw(0, 0, 0, 0); // start dark; layer/lights drive it
}

// Read Pimoroni trackball and convert to keyball motion format
bool pimoroni_left_read_motion(int16_t *x, int16_t *y, uint8_t *click) {
    if (!pimoroni_initialized) {
        return false;
    }

    // Read Pimoroni trackball data
    pimoroni_data_t data;
    i2c_status_t status = read_pimoroni_trackball(&data);
    if (status != I2C_STATUS_SUCCESS) {
        if (++pimoroni_fail_count >= PIMORONI_FAIL_LIMIT) {
            // Give up: disable the Pimoroni so its dead I2C stops stalling the
            // main loop. Stays off until the next reboot.
            pimoroni_initialized = false;
        }
        return false;
    }
    pimoroni_fail_count = 0;  // a good read resets the counter

    // Threshold the RAW directional counts before the squaring conversion.
    // The Pimoroni reports a small steady idle imbalance (e.g. right=1, left=0)
    // which get_offsets() squares and scales into perpetual scroll once it is
    // integrated by the accumulator. Require a difference of >1 count in a
    // direction for it to register at all; this kills idle drift at the source
    // without touching real movement (a deliberate roll produces several counts).
    int16_t dx_raw = (int16_t)data.right - (int16_t)data.left;
    int16_t dy_raw = (int16_t)data.down  - (int16_t)data.up;
    if (dx_raw > -2 && dx_raw < 2) { data.right = 0; data.left = 0; }
    if (dy_raw > -2 && dy_raw < 2) { data.down  = 0; data.up   = 0; }

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