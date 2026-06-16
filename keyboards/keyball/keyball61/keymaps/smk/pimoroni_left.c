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
    // Always try to initialize - it will only work on the half with Pimoroni hardware

    dprintf("Pimoroni: Starting initialization on %s side\n", is_keyboard_left() ? "left" : "right");

    // Initialize I2C and Pimoroni trackball
    i2c_init();

    // Initialize Pimoroni trackball with error handling
    pimoroni_trackball_device_init();
    dprintf("Pimoroni: Device init completed\n");

    // Set initial RGBW color (bright red for initialization feedback)
    pimoroni_trackball_set_rgbw(255, 0, 0, 0);
    wait_ms(500);  // Wait 500ms to show red

    // Change to subtle white to indicate successful init
    pimoroni_trackball_set_rgbw(20, 20, 20, 10);

    pimoroni_initialized = true;
    dprintf("Pimoroni: Initialization completed successfully\n");

    // Test: Run a quick rainbow cycle to verify LED control works
    if (is_keyboard_left()) {
        dprintf("Pimoroni: Running rainbow test on left half\n");

        // Red
        pimoroni_trackball_set_rgbw(255, 0, 0, 0);
        wait_ms(200);

        // Green
        pimoroni_trackball_set_rgbw(0, 255, 0, 0);
        wait_ms(200);

        // Blue
        pimoroni_trackball_set_rgbw(0, 0, 255, 0);
        wait_ms(200);

        // White
        pimoroni_trackball_set_rgbw(50, 50, 50, 20);
        wait_ms(200);

        dprintf("Pimoroni: Rainbow test completed\n");
    }
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
        return false;
    }

    // Convert to keyball motion format
    *x = pimoroni_trackball_get_offsets(data.right, data.left, 3);  // Scale factor 3
    *y = pimoroni_trackball_get_offsets(data.down, data.up, 3);    // Scale factor 3
    *click = data.click;

    // Debug output
    static uint16_t debug_counter = 0;
    debug_counter++;
    if (debug_counter % 100 == 0) { // Print every 100th reading to avoid spam
        dprintf("Pimoroni: Read motion - x=%d, y=%d, click=%d, status=%d\n", *x, *y, *click, status);
    }

    return true;
}

// Set RGBW color for Pimoroni trackball
void pimoroni_left_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (pimoroni_initialized) {
        dprintf("Pimoroni: Setting RGBW to R=%d, G=%d, B=%d, W=%d\n", r, g, b, w);
        pimoroni_trackball_set_rgbw(r, g, b, w);
    } else {
        dprintf("Pimoroni: Cannot set RGBW - not initialized\n");
    }
}