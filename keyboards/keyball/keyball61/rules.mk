# Build Options
BOOTMAGIC_ENABLE = no       # Enable Bootmagic Lite
EXTRAKEY_ENABLE = yes        # Audio control and System control
CONSOLE_ENABLE = no         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration
NKRO_ENABLE = no            # Enable N-Key Rollover
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
AUDIO_ENABLE = no           # Audio output

# Duplex matrix.
CUSTOM_MATRIX = lite
SRC += lib/duplexmatrix/duplexmatrix.c

# Split keyboard.
SERIAL_DRIVER = vendor

# I2C for Pimoroni trackball (left half)
I2C_DRIVER_REQUIRED = yes

# Dual trackball support - PMW3360 (right) + Pimoroni (left)
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = custom
SPLIT_POINTING_ENABLE = yes
POINTING_DEVICE_COMBINED = yes
SRC += drivers/pmw3360/pmw3360.c
SRC += drivers/sensors/pimoroni_trackball.c
SRC += keyboards/keyball/keyball61/keymaps/smk/pimoroni_left.c
QUANTUM_LIB_SRC += spi_master.c # PMW3360 uses SPI
QUANTUM_LIB_SRC += i2c_master.c # Pimoroni uses I2C

# This is unnecessary for processing KC_MS_BTN*.
MOUSEKEY_ENABLE = no

# Enabled only one of RGBLIGHT and RGB_MATRIX if necessary.
RGBLIGHT_ENABLE = no        # Enable RGBLIGHT
RGB_MATRIX_ENABLE = no      # Enable RGB_MATRIX (not work yet)
RGB_MATRIX_DRIVER = ws2812

# Do not enable SLEEP_LED_ENABLE. it uses the same timer as BACKLIGHT_ENABLE
SLEEP_LED_ENABLE = no       # Breathing sleep LED during USB suspend

# To support OLED
OLED_ENABLE = no                # Please Enable this in each keymaps.
SRC += lib/oledkit/oledkit.c    # OLED utility for Keyball series.

# Include common library
SRC += lib/keyball/keyball.c

# Disable other features to squeeze firmware size
SPACE_CADET_ENABLE = no
MAGIC_ENABLE = no
