#!/usr/bin/env python3
"""Generate a 1080x1920 portrait layer cheatsheet wallpaper for the smk Keyball61 keymap.

Renders layers 0-4 as true split halves, dark/neon themed, color-coded per layer.
Outputs cheatsheet.svg then rasterizes to cheatsheet.png via rsvg-convert.
"""

import subprocess
import os
import math

W, H = 1080, 1920

# Per-layer accent colors (match the Pimoroni LED scheme where it applies).
LAYERS = [
    {"n": 0, "name": "BASE",     "sub": "QWERTY • left ball = scroll", "accent": "#3da5ff"},
    {"n": 1, "name": "SHIFT",    "sub": "LT(1) hold • shifted base",    "accent": "#9d6bff"},
    {"n": 2, "name": "NAV / NUM","sub": "LT(2) hold / TG(2)",            "accent": "#39d98a"},
    {"n": 3, "name": "SETTINGS", "sub": "LT(3) hold • RGB / CPI / boot", "accent": "#ff9d33"},
    {"n": 4, "name": "GAMING",   "sub": "TG(4) • dedicated left hand",   "accent": "#ff4d4d"},
]

# Keycode -> display label. Friendly, compact glyphs for a wallpaper.
LABELS = {
    "_______": "", "XXXXXXX": "",
    "KC_GRV": "`", "KC_MINS": "-", "KC_EQL": "=", "KC_LBRC": "[", "KC_RBRC": "]",
    "KC_BSLS": "\\", "KC_SCLN": ";", "KC_QUOT": "'", "KC_COMM": ",", "KC_DOT": ".",
    "KC_SLSH": "/", "KC_TAB": "Tab", "KC_CAPS": "Caps", "KC_ENT": "Enter",
    "KC_BSPC": "Bksp", "KC_SPC": "Spc", "KC_ESC": "Esc", "KC_DEL": "Del",
    "KC_LSFT": "Shft", "KC_LCTL": "Ctrl", "KC_LALT": "Alt", "KC_LGUI": "Gui",
    "KC_LEFT": "←", "KC_RGHT": "→", "KC_RIGHT": "→", "KC_UP": "↑", "KC_DOWN": "↓",
    "KC_PGUP": "PgUp", "KC_PGDN": "PgDn", "KC_HOME": "Home", "KC_END": "End",
    "KC_BTN1": "M1", "KC_BTN2": "M2", "KC_BTN3": "M3",
    "RGB_TOG": "RGB", "RGB_MOD": "RGB+", "RGB_RMOD": "RGB-",
    "RGB_HUI": "Hue+", "RGB_HUD": "Hue-", "RGB_SAI": "Sat+", "RGB_SAD": "Sat-",
    "RGB_VAI": "Br+", "RGB_VAD": "Br-",
    "RGB_M_P": "P", "RGB_M_B": "B", "RGB_M_R": "R", "RGB_M_SW": "SW", "RGB_M_SN": "SN",
    "RGB_M_K": "K", "RGB_M_X": "X", "RGB_M_G": "G", "RGB_M_T": "T", "RGB_M_TW": "TW",
    "AML_TO": "AML", "AML_I50": "AML+", "AML_D50": "AML-",
    "CPI_D1K": "CPI--", "CPI_D100": "CPI-", "CPI_I100": "CPI+", "CPI_I1K": "CPI++",
    "KBC_SAVE": "Save", "KBC_RST": "Rst", "EE_CLR": "EECLR", "QK_BOOT": "BOOT",
    "SCRL_MO": "Scrl", "SCRL_TO": "ScrlT", "SCRL_DVD": "SDv-", "SCRL_DVI": "SDv+",
    "SSNP_FRE": "Snap0", "SSNP_VRT": "SnapV", "SSNP_HOR": "SnapH",
    "TG(2)": "TG2", "TG(4)": "TG4",
}

def label(kc):
    kc = kc.strip()
    if kc in LABELS:
        return LABELS[kc]
    # Layer-tap: LT(n,KC_x) -> "x\nL n"
    if kc.startswith("LT("):
        inner = kc[3:-1]
        n, k = inner.split(",", 1)
        return (label(k.strip()) or "") + " ·L" + n.strip()
    # Shifted: S(KC_x)
    if kc.startswith("S(") and kc.endswith(")"):
        base = label(kc[2:-1].strip())
        return base
    # Plain KC_X -> X ; KC_1 -> 1
    if kc.startswith("KC_"):
        rest = kc[3:]
        if len(rest) == 1:
            return rest
        if rest.startswith("F") and rest[1:].isdigit():
            return rest
        if rest.isdigit():
            return rest
        return rest.capitalize()
    return kc

# Keymap rows per layer: list of 5 rows; each row = (left_keys, right_keys).
# Transcribed from keymap.c LAYOUT_universal (layers 0-4).
KEYMAP = {
0: [
  (["KC_GRV","KC_1","KC_2","KC_3","KC_4","KC_5"], ["KC_6","KC_7","KC_8","KC_9","KC_0","KC_MINS"]),
  (["KC_TAB","KC_Q","KC_W","KC_E","KC_R","KC_T"], ["KC_Y","KC_U","KC_I","KC_O","KC_P","KC_EQL"]),
  (["KC_CAPS","KC_A","KC_S","KC_D","KC_F","KC_G"], ["KC_H","KC_J","KC_K","KC_L","KC_SCLN","LT(1,KC_ENT)"]),
  (["LT(1,KC_LSFT)","KC_Z","KC_X","KC_C","KC_V","KC_B","KC_LBRC"], ["KC_RBRC","KC_N","KC_M","KC_COMM","KC_DOT","KC_SLSH","KC_QUOT"]),
  (["KC_LCTL","KC_LALT","KC_LEFT","KC_UP","KC_LGUI","LT(2,KC_SPC)","LT(3,KC_ESC)"], ["LT(3,KC_BSPC)","LT(2,KC_SPC)","_______","_______","_______","TG(2)","KC_BSLS"]),
],
1: [
  (["S(KC_GRV)","S(KC_1)","S(KC_2)","S(KC_3)","S(KC_4)","S(KC_5)"], ["S(KC_6)","S(KC_7)","S(KC_8)","S(KC_9)","S(KC_0)","S(KC_MINS)"]),
  (["S(KC_TAB)","S(KC_Q)","S(KC_W)","S(KC_E)","S(KC_R)","S(KC_T)"], ["S(KC_Y)","S(KC_U)","S(KC_I)","S(KC_O)","S(KC_P)","S(KC_EQL)"]),
  (["_______","S(KC_A)","S(KC_S)","S(KC_D)","S(KC_F)","S(KC_G)"], ["S(KC_H)","S(KC_J)","S(KC_K)","S(KC_L)","S(KC_SCLN)","_______"]),
  (["_______","S(KC_Z)","S(KC_X)","S(KC_C)","S(KC_V)","S(KC_B)","S(KC_LBRC)"], ["S(KC_RBRC)","S(KC_N)","S(KC_M)","S(KC_COMM)","S(KC_DOT)","S(KC_SLSH)","S(KC_QUOT)"]),
  (["_______","_______","_______","_______","_______","_______","_______"], ["_______","_______","_______","_______","_______","_______","S(KC_BSLS)"]),
],
2: [
  (["SSNP_FRE","KC_F1","KC_F2","KC_F3","KC_F4","KC_F5"], ["KC_F6","KC_F7","KC_F8","KC_F9","KC_F10","KC_F11"]),
  (["SSNP_VRT","_______","KC_7","KC_8","KC_9","_______"], ["_______","KC_LEFT","KC_UP","KC_RGHT","_______","KC_F12"]),
  (["SSNP_HOR","_______","KC_4","KC_5","KC_6","S(KC_SCLN)"], ["KC_PGUP","KC_BTN1","KC_DOWN","KC_BTN2","KC_BTN3","_______"]),
  (["_______","_______","KC_1","KC_2","KC_3","S(KC_MINS)","S(KC_8)"], ["S(KC_9)","KC_PGDN","_______","_______","_______","_______","_______"]),
  (["_______","_______","KC_0","KC_DOT","_______","_______","SCRL_MO"], ["_______","_______","_______","_______","_______","TG(2)","TG(4)"]),
],
3: [
  (["RGB_TOG","AML_TO","AML_I50","AML_D50","_______","_______"], ["RGB_M_P","RGB_M_B","RGB_M_R","RGB_M_SW","RGB_M_SN","RGB_M_K"]),
  (["RGB_MOD","RGB_HUI","RGB_SAI","RGB_VAI","_______","_______"], ["RGB_M_X","RGB_M_G","RGB_M_T","RGB_M_TW","_______","_______"]),
  (["RGB_RMOD","RGB_HUD","RGB_SAD","RGB_VAD","_______","_______"], ["CPI_D1K","CPI_D100","CPI_I100","CPI_I1K","KBC_SAVE","KBC_RST"]),
  (["_______","_______","SCRL_DVD","SCRL_DVI","SCRL_MO","SCRL_TO","EE_CLR"], ["EE_CLR","KC_HOME","KC_PGDN","KC_PGUP","KC_END","_______","_______"]),
  (["QK_BOOT","_______","KC_LEFT","KC_DOWN","KC_UP","KC_RGHT","_______"], ["_______","KC_DEL","_______","_______","_______","_______","QK_BOOT"]),
],
4: [
  (["KC_ESC","KC_1","KC_2","KC_3","KC_4","KC_5"], ["KC_6","KC_7","KC_8","KC_9","KC_0","KC_GRV"]),
  (["KC_TAB","KC_Q","KC_W","KC_E","KC_R","_______"], ["_______","KC_LEFT","KC_UP","KC_RGHT","_______","KC_F12"]),
  (["KC_LCTL","KC_A","KC_S","KC_D","KC_F","_______"], ["KC_PGUP","KC_BTN1","KC_DOWN","KC_BTN2","KC_BTN3","_______"]),
  (["KC_LSFT","KC_Z","KC_X","KC_C","KC_V","_______","_______"], ["KC_PGDN","_______","_______","_______","_______","_______","_______"]),
  (["_______","_______","_______","KC_SPC","_______","_______","_______"], ["KC_DEL","CPI_D1K","CPI_D100","CPI_I100","CPI_I1K","_______","TG(4)"]),
]
}

# A key is "modified" (LT/TG/special) for accent highlight.
def is_special(kc):
    return kc.startswith("LT(") or kc.startswith("TG(") or kc in (
        "SCRL_MO","SCRL_TO","QK_BOOT","EE_CLR","KBC_SAVE","KBC_RST")

def esc(s):
    return s.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;")

# --- Layout geometry ---
MARGIN = 36
TITLE_H = 96
COL_GAP = 18
# 5 layers stacked vertically.
N = len(LAYERS)
avail_h = H - TITLE_H - MARGIN
block_h = avail_h // N
KEY = 38          # key unit size
KGAP = 5
ROW_H = KEY + KGAP

# Standard Keyball61 column stagger, in key-height units, per column of a half.
# Left half columns, outer->inner: [mod, pinky, ring, middle, index, inner].
# Negative = key sits higher. Right half mirrors this (inner..mod).
COL_STAGGER = [0.0, 0.0, -0.12, -0.28, -0.06, 0.18]
# Thumb row (row 5): each thumb key's vertical drop and horizontal inset.
THUMB_DROP = 0.06   # extra downward offset for the thumb cluster (key-height units)

# Shared-pivot thumb fan (left half; right mirrors). Keys fan about a pivot
# above-and-left of the cluster. Tuned in grid units / degrees.
# NOTE: pivot_dy / the arc are measured from the BOTTOM-ROW baseline (the y0
# passed to render_half for row 4), same as place()'s dy. So the pivot sits
# ABOVE the row (negative dy) and the keys swing down onto the row.
# Thumb fan: a monotonic directional sweep (all keys tilt progressively the
# SAME way as they step inward), not a symmetric splay. Large radius + small
# angular step keeps >1 key of spacing along the arc so keys never overlap.
# arc_gap ~= radius * dtheta(rad); radius 5.0 * 9deg -> ~0.78 key... so use a
# slightly larger gap below.
THUMB_PIVOT_COL = 4.6    # pivot x (column); cluster centre sits under the pivot
THUMB_PIVOT_DY  = -3.6   # pivot y, in rows relative to the bottom-row baseline
THUMB_RADIUS    = 4.6    # distance (key-units) from pivot to each thumb key
THUMB_THETA0    = -15.0  # angle of the first thumb key (deg, clockwise from down)
THUMB_DTHETA    = 15.0   # angular step between successive thumb keys (deg)

def key_rect(x, y, w, lab, accent, special, parts, rot=0.0):
    fill = "#1c1f26"
    stroke = accent if special else "#363b45"
    tcol = accent if special else "#c9d1d9"
    # Rotate the whole key (box + label) about its centre when requested, so
    # thumb keys can fan along an arc.
    if rot:
        cx, cy = x + w/2, y + KEY/2
        parts.append(f'<g transform="rotate({rot:.2f} {cx:.1f} {cy:.1f})">')
    parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{KEY:.1f}" rx="5" '
                 f'fill="{fill}" stroke="{stroke}" stroke-width="1.2"/>')
    if lab:
        # split layer-tap label "x·Ln" onto a small second line
        if "·L" in lab:
            main, lt = lab.split("·L")
            main = main.strip()
            mfs = 14 if len(main) <= 3 else (11 if len(main) <= 4 else 9)
            parts.append(f'<text x="{x+w/2:.1f}" y="{y+KEY/2:.1f}" text-anchor="middle" '
                         f'font-size="{mfs}" fill="{tcol}" font-family="Menlo,monospace">{esc(main)}</text>')
            parts.append(f'<text x="{x+w/2:.1f}" y="{y+KEY-6:.1f}" text-anchor="middle" '
                         f'font-size="9" fill="{accent}" font-family="Menlo,monospace">L{esc(lt)}</text>')
        else:
            fs = 15 if len(lab) <= 2 else (12 if len(lab) <= 4 else 9)
            parts.append(f'<text x="{x+w/2:.1f}" y="{y+KEY/2+5:.1f}" text-anchor="middle" '
                         f'font-size="{fs}" fill="{tcol}" font-family="Menlo,monospace">{esc(lab)}</text>')
    if rot:
        parts.append('</g>')

U = KEY + KGAP  # one key unit (size + gap)

def render_half(keys, x0, y0, accent, parts, right=False, row_idx=0):
    """Render a row of a half with column stagger.

    Column model (visual columns 0..5, left-to-right within the half):
      left half  : col = finger column, stagger[col]
      right half : col 0 = inner (board centre), col 5 = outer; we index the
                   reversed stagger so the halves mirror.
    Special keys:
      [ / ]  bracket keys sit against the inner column (B / N) pushed up.
      thumb  cluster keys drop slightly and angle toward board centre.
    """
    rstag = list(reversed(COL_STAGGER))   # right-half stagger, inner->outer

    def place(col, dy, dx, kc, rot=0.0):
        kx = x0 + (col + dx) * U
        ky = y0 + dy * U
        draw_key(kx, ky, kc, accent, parts, rot)

    def place_arc(cluster, pivot_col, pivot_dy, radius, theta0, dtheta,
                  mirror=False, mirror_axis=3.0):
        """Fan a list of key labels around a shared pivot.

        Each key k is placed at angle theta = theta0 + k*dtheta (clockwise from
        straight down) at `radius` units from the pivot, and rotated by theta so
        the cluster fans about one point. When `mirror` is set, the whole arc is
        reflected left/right about column `mirror_axis` (and rotations negated),
        producing an exact mirror of the same parameters.
        """
        px = x0 + pivot_col * U
        py = y0 + pivot_dy * U
        for k, kc in enumerate(cluster):
            th = math.radians(theta0 + k * dtheta)
            cxp = px + radius * U * math.sin(th)
            cyp = py + radius * U * math.cos(th)
            rot = math.degrees(th)
            if mirror:
                axis_x = x0 + mirror_axis * U
                cxp = 2 * axis_x - cxp
                rot = -rot
            draw_key(cxp - KEY / 2, cyp - KEY / 2, kc, accent, parts, rot)

    if not right:
        # ---- LEFT HALF ----
        if row_idx < 4:
            # rows 0-2 have 6 keys (cols 0-5); row 3 has a 7th = [ bracket.
            for i, kc in enumerate(keys):
                if i < 6:
                    place(i, COL_STAGGER[i], 0.0, kc)
                else:
                    # [ : up against B (col 5), bumped down 15% of key height.
                    place(5, COL_STAGGER[5] + 0.15, 1.0, kc)
        else:
            # row 4: Ctrl Alt <- ^ (cols 0-3 grid) then Gui Spc Esc thumb fan.
            for i in range(4):
                place(i, COL_STAGGER[i], 0.0, keys[i])
            # Thumb cluster: Gui is the anchor (square with the grid); Spc & Esc
            # step inward-and-down, tops tilting toward board centre (CW).
            # Each entry: (key, col, dy, rotation-deg).
            for kc, col, dy, rot in [
                (keys[4], 4.30, 0.55, 0.0),   # Gui  - square anchor
                (keys[5], 5.25, 0.78, 13.0),  # Spc  - tilt toward centre
                (keys[6], 6.12, 1.18, 26.0),  # Esc  - tilt more, lowest
            ]:
                place(col, dy, 0.0, kc, rot=rot)
    else:
        # ---- RIGHT HALF ---- (col 0 = inner / board centre)
        if row_idx < 3:
            for i, kc in enumerate(keys):    # 6 keys, inner->outer
                place(i, rstag[i], 0.0, kc)
        elif row_idx == 3:
            # [ ], N, M, ',', '.', '/', ' ] : ] is bracket, rest are cols 0-5.
            for i, kc in enumerate(keys):
                if i == 0:
                    # ] : against N (col 0), bumped down 15% of key height.
                    place(0, rstag[0] + 0.15, -1.0, kc)
                else:
                    place(i - 1, rstag[i - 1], 0.0, kc)
        else:
            # row 4: Bksp Spc [ball ball ball] TG2 \ .
            # Thumb cluster = Bksp(i0) Spc(i1); TG2(i5) \(i6) shift to cols 4,5.
            # Mirror of the left fan: Bksp is the square anchor (sits just left
            # of the ball); Spc steps inward-and-down toward centre, top tilting
            # toward centre (CCW). Each entry: (key, col, dy, rotation-deg).
            # The right cluster reserves a ghost slot mirroring the left's Gui
            # (the square outer anchor) nearest the ball. Bksp & Spc occupy the
            # mirror of the left's Spc & Esc slots, shifted one key-slot left so
            # that ghost gap sits between Spc and the ball.
            for kc, col, dy, rot in [
                (keys[0], 0.05, 1.18, -26.0),  # Bksp - innermost (mirror of Esc), most tilt, lowest
                (keys[1], 0.95, 0.78, -13.0),  # Spc  - middle (mirror of left Spc)
                # ghost Gui-slot ~col 1.90 (mirror of Gui) intentionally empty
            ]:
                place(col, dy, 0.0, kc, rot=rot)
            # TG2(i5)->col4, \(i6)->col5 on the normal grid (no thumb drop).
            for i in (5, 6):
                col = i - 1        # 4, 5
                place(col, rstag[col], 0.0, keys[i])
    return
def draw_key(kx, ky, kc, accent, parts, rot=0.0):
    """Draw one key box + label at absolute (kx, ky), optionally rotated."""
    key_rect(kx, ky, KEY, label(kc), accent, is_special(kc), parts, rot)

def draw_ball(cx, cy, r, color, name, parts):
    """Draw a trackball as a ringed circle in the same gray as the key boxes."""
    parts.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" '
                 f'fill="#1c1f26" stroke="#363b45" stroke-width="1.6"/>')
    parts.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r*0.62:.1f}" '
                 f'fill="none" stroke="#363b45" stroke-width="1" opacity="0.7"/>')

def build_svg():
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    # background gradient
    parts.append('<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">'
                 '<stop offset="0" stop-color="#0d0f14"/><stop offset="1" stop-color="#15181f"/>'
                 '</linearGradient></defs>')
    parts.append(f'<rect width="{W}" height="{H}" fill="url(#bg)"/>')
    # title
    parts.append(f'<text x="{W/2}" y="56" text-anchor="middle" font-size="34" '
                 f'fill="#e6edf3" font-family="Menlo,monospace" font-weight="bold">Keyball61 — smk layers</text>')
    parts.append(f'<text x="{W/2}" y="82" text-anchor="middle" font-size="15" '
                 f'fill="#7d8590" font-family="Menlo,monospace">left ball = scroll · right ball = cursor</text>')

    half_w = 6*(KEY+KGAP) + 8 + (KEY+KGAP)   # 7 keys incl thumb
    total_kb_w = half_w*2 + 60               # 60 = center gap for thumb clusters
    kb_x0 = (W - total_kb_w)//2

    for li, layer in enumerate(LAYERS):
        by = TITLE_H + li*block_h
        accent = layer["accent"]
        # layer header bar
        parts.append(f'<rect x="{MARGIN}" y="{by+6:.0f}" width="{W-2*MARGIN}" height="{block_h-14:.0f}" '
                     f'rx="10" fill="#11141a" stroke="{accent}" stroke-width="1.4" opacity="0.95"/>')
        parts.append(f'<rect x="{MARGIN}" y="{by+6:.0f}" width="6" height="{block_h-14:.0f}" rx="3" fill="{accent}"/>')
        parts.append(f'<text x="{MARGIN+22}" y="{by+34:.0f}" font-size="22" fill="{accent}" '
                     f'font-family="Menlo,monospace" font-weight="bold">L{layer["n"]} · {layer["name"]}</text>')
        parts.append(f'<text x="{MARGIN+22}" y="{by+54:.0f}" font-size="12.5" fill="#8b949e" '
                     f'font-family="Menlo,monospace">{esc(layer["sub"])}</text>')

        rows = KEYMAP[layer["n"]]
        # Vertically center the 5-row grid in the space below the header text.
        header_bottom = by + 64
        block_bottom = by + block_h - 14
        grid_total_h = 5*ROW_H - KGAP
        grid_y = header_bottom + ((block_bottom - header_bottom) - grid_total_h)/2
        rx0 = kb_x0 + half_w + 60
        for ri, (lk, rk) in enumerate(rows):
            ry = grid_y + ri*ROW_H
            render_half(lk, kb_x0, ry, accent, parts, row_idx=ri)
            render_half(rk, rx0, ry, accent, parts, right=True, row_idx=ri)

        # Trackballs. Drawn once per layer over the grid.
        # Right half: PMW3360 replaces the bottom-middle 3 keys (cols 2,3,4 of
        # the mirrored half) -> a large circle. Pimoroni: left half, just right
        # of the inner column around the T/G/B rows (a smaller circle).
        # Right PMW3360 big ball: over the skipped middle columns (1-3) of the
        # bottom row, dropped below the thumb keys.
        # Right gap (to the col-4 key, e.g. TG2/BOOT) equals the normal grid gap.
        # Vertically the ball is centred on the row-4 keys so it doesn't hang
        # below the board; being larger than a key, it naturally reaches up into
        # the row-3 gap and down a touch below row 4.
        pmw_r  = KEY*0.82
        pmw_cx = rx0 + 4.0*U - KGAP - pmw_r              # right edge KGAP left of col 4
        pmw_cy = grid_y + 4.0*ROW_H + KEY/2 + 1.25*KGAP  # row-4 centre, nudged down ~1.25 gaps
        draw_ball(pmw_cx, pmw_cy, pmw_r, "#ff8a3d", "PMW3360", parts)
        # Left Pimoroni small ball: right of the inner column, its vertical
        # centre level with the TOP edge of the T key (row 1, inner column).
        t_top = grid_y + 1*ROW_H + COL_STAGGER[5]*U
        pim_cx = kb_x0 + 6.55*U
        pim_cy = t_top
        draw_ball(pim_cx, pim_cy, KEY*0.6, accent, "Pimoroni", parts)

    parts.append('</svg>')
    return "\n".join(parts)

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    svg_path = os.path.join(here, "cheatsheet.svg")
    png_path = os.path.join(here, "cheatsheet.png")
    svg = build_svg()
    with open(svg_path, "w") as f:
        f.write(svg)
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), "-o", png_path, svg_path], check=True)
    print("wrote", svg_path)
    print("wrote", png_path)

if __name__ == "__main__":
    main()
