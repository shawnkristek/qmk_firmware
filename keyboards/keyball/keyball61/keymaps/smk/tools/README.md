# smk keymap: LED tuning firmware

Prebuilt `.uf2` files here are gitignored build outputs; rebuild with the
commands below from the repo root. Flash by putting a half into its
bootloader (double-tap reset) and copying the file to the `RPI-RP2` drive,
or run the matching `qmk flash` command and reset when it says "Waiting".

Console output needs `qmk console` with the Homebrew `hidapi` library:

```sh
DYLD_LIBRARY_PATH=/opt/homebrew/lib qmk console -w 2
```

## Calibration field (`smk_cal_white.uf2`, `smk_cal_orange.uf2`)

```sh
qmk flash -e EXTRAFLAGS="-DLED_CAL" -e CONSOLE_ENABLE=yes
qmk flash -e EXTRAFLAGS="-DLED_CAL -DLED_CAL_LAYER=3" -e CONSOLE_ENABLE=yes
```

`LED_CAL_LAYER=n` paints the field in layer n's colour (1 nav green,
2 cyan, 3 orange, 4 red); without it the field is white. Trims start from
the `led_trim` table compiled into the keymap.

Group trims, +/-5 % per press (the group is the LEDs listed in
`led_trim`'s comment for that half):

| Half  | R-    | R+ | G- | G+ | B- | B+    |
| ----- | ----- | -- | -- | -- | -- | ----- |
| left  | grave | 1  | 2  | 3  | 4  | 5     |
| right | 6     | 7  | 8  | 9  | 0  | minus |

Per-key sweep and lock: press a key to sweep its selected channel;
lock with Space (left) or Backspace (right). Channel select on the left:
Ctrl red, Alt green, Right arrow blue, Left arrow all; on the right the
Nav toggle cycles red, green, blue, all. Dump the table with Esc (left)
or Right Shift (right); the console prints `CAL { idx, r, g, b },` lines
to paste into `led_trim` in `keymap.c`.

## LED map walk (`smk_mapwalk.uf2`)

```sh
qmk flash -e EXTRAFLAGS="-DLED_MAPWALK" -e CONSOLE_ENABLE=yes
```

Lights one LED at a time in this half's index range. Press the lit key to
record `MAP idx = row col` and advance; Esc (left) or Right Shift (right)
records "no key" for underglow or trackball LEDs; Space (left) or
Backspace (right) steps back one. Use the pairings to rebuild
`g_led_config` in `keymap.c`.

## Pimoroni ball colour (`smk_pimcal.uf2`)

```sh
qmk flash -e EXTRAFLAGS="-DPIM_CAL" -e CONSOLE_ENABLE=yes
```

Left half only. All key LEDs show the target layer's colour so the ball
can be matched to them. grave cycles the target layer 1 to 7; 1/2 nudge
red, 3/4 green, Tab/Q blue, W/E white, in steps of 10; Esc dumps one
`pimoroni_left_set_rgbw(...)` line per layer to paste into
`pimoroni_apply_layer_color` in `keymap.c`.

## VIA (`keyball61_via.json`)

Always build with `-e SKIP_VERSION=yes`, for example
`qmk flash -e SKIP_VERSION=yes`. VIA's saved-keymap check is derived from
the build date, so a normal build wipes the keymap edited in VIA on the
next boot. The flag has to be on the command line; setting it in
`rules.mk` is too late in the build. Back up your VIA keymap from the
Save + Load tab before experimenting.

VIA has no built-in definition for this board's USB ID, so load this one:
in the VIA app enable Settings > Show Design tab, then Design > Load Draft
Definition and pick `keyball61_via.json`. Drafts live in the browser, so
reload it if VIA stops recognising the board. Connect the LEFT half over
USB; VIA edits the master's EEPROM keymap and take effect immediately.
The left half's per-key lighting follows VIA edits; the right half shows
the compiled defaults until it is reflashed. Choose "Dual" under Ball
availability. Custom keys appear as Lights and PimMode.
