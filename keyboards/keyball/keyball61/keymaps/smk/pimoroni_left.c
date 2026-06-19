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
// A flaky/disconnected Pimoroni makes every I2C read time out, stalling the
// main loop (laggy typing). After this many consecutive failures we BACK OFF:
// stop reading for a window so typing stays fast, then periodically retry with
// a bus re-init so it recovers on its own when the connection comes back (no
// power-cycle needed).
#define PIMORONI_FAIL_LIMIT 20
#define PIMORONI_RETRY_MS   1000
static uint8_t  pimoroni_fail_count = 0;
static bool     pimoroni_backoff    = false;
static uint16_t pimoroni_retry_at   = 0;

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

    // While backing off (the ball went unresponsive), skip reads so a dead I2C
    // doesn't stall typing. Once PIMORONI_RETRY_MS passes, re-init the bus and
    // try again -- recovers automatically when the connection returns.
    if (pimoroni_backoff) {
        if (timer_elapsed(pimoroni_retry_at) < PIMORONI_RETRY_MS) {
            return false;
        }
        i2c_init();                 // attempt bus recovery
        pimoroni_backoff   = false;
        pimoroni_fail_count = 0;
    }

    // Read Pimoroni trackball data
    pimoroni_data_t data;
    i2c_status_t status = read_pimoroni_trackball(&data);
    if (status != I2C_STATUS_SUCCESS) {
        if (++pimoroni_fail_count >= PIMORONI_FAIL_LIMIT) {
            // Enter backoff: pause reads for a while, then retry (see top).
            pimoroni_backoff = true;
            pimoroni_retry_at = timer_read();
        }
        return false;
    }
    pimoroni_fail_count = 0;  // a good read resets the counter

    // Note: no raw dead-zone here. Slow rolls produce small (magnitude-1)
    // counts, and zeroing those made slow scrolling not register. Idle drift is
    // instead handled by the accumulator decay in pimoroni_compute_scroll
    // (which tells transient movement from a persistent idle bias).

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