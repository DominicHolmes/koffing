#!/usr/bin/env python3
"""Generate Koffing pixel art sprites in multiple formats.

Run from art/generator/:
  uv run python generate.py

Outputs (relative to art/):
  preview/color/*.png    Color sprites for review
  preview/mono/*.xbm     Monochrome XBM for SSD1306
  preview/preview.png    Scaled-up sheet showing all sprites
  include/*.h            C headers for Arduino
"""

import os
from PIL import Image, ImageDraw

BASE = os.path.dirname(os.path.abspath(__file__))
ART_ROOT = os.path.dirname(BASE)
COLOR_DIR = os.path.join(ART_ROOT, "preview", "color")
MONO_DIR = os.path.join(ART_ROOT, "preview", "mono")
HEADERS_DIR = os.path.join(ART_ROOT, "include")

# --- Palette indices ---
BG = 0
OL = 1          # outline
BODY = 2
BODY_DK = 3     # body shadow
BODY_LT = 4     # body highlight
BODY_BR = 5     # body bright specular
SKULL = 6       # skull marking
EYE_W = 7       # eye white
PUPIL = 8
MOUTH = 9       # mouth interior
TEETH = 10
CLD = 11        # cloud main
CLD_DK = 12     # cloud shadow
CLD_LT = 13     # cloud highlight
CRAT_R = 14     # crater rim
CRAT_I = 15     # crater interior

# --- RGB values per palette index ---
PALETTE_NORMAL = {
    BG:      (0, 0, 0),
    OL:      (30, 20, 50),
    BODY:    (123, 94, 167),
    BODY_DK: (85, 58, 125),
    BODY_LT: (155, 125, 195),
    BODY_BR: (185, 160, 215),
    SKULL:   (230, 215, 160),
    EYE_W:   (255, 255, 255),
    PUPIL:   (20, 10, 30),
    MOUTH:   (45, 25, 65),
    TEETH:   (240, 238, 230),
    CLD:     (230, 208, 148),
    CLD_DK:  (195, 168, 100),
    CLD_LT:  (248, 235, 190),
    CRAT_R:  (175, 148, 128),
    CRAT_I:  (65, 48, 95),
}

# Mono: which palette indices map to white (1) vs black (0)
MONO_WHITE = {OL, SKULL, EYE_W, TEETH, CLD, CLD_DK, CLD_LT, CRAT_R}


# --- Helpers ---

def parse_grid(text, char_map):
    """Parse a text grid into [(dx, dy, palette_index), ...].
    Characters not in char_map are skipped (transparent)."""
    pixels = []
    for y, line in enumerate(text.strip().splitlines()):
        for x, ch in enumerate(line):
            if ch in char_map:
                pixels.append((x, y, char_map[ch]))
    return pixels


def stamp(draw, pixels, ox, oy):
    """Place pixel pattern at offset."""
    for dx, dy, c in pixels:
        draw.point((ox + dx, oy + dy), fill=c)


def make_image(w, h):
    """Create palette-mode image."""
    img = Image.new('P', (w, h), BG)
    pal = [0] * 768
    for idx, (r, g, b) in PALETTE_NORMAL.items():
        pal[idx * 3: idx * 3 + 3] = [r, g, b]
    img.putpalette(pal)
    return img


# --- Body ---

def shade_body(img):
    """Apply sphere shading to BODY pixels."""
    px = img.load()
    cx, cy = 15.5, 15.5
    radius = 13.0
    # Light direction (from center toward upper-left light source)
    lx, ly = -5.5, -7.5
    ld = (lx * lx + ly * ly) ** 0.5
    lx, ly = lx / ld, ly / ld

    for y in range(32):
        for x in range(32):
            if px[x, y] != BODY:
                continue
            dx, dy = x - cx, y - cy
            dist = (dx * dx + dy * dy) ** 0.5
            if dist < 0.1:
                continue
            nx, ny = dx / dist, dy / dist
            dot = nx * lx + ny * ly
            edge = dist / radius

            if edge > 0.88 and dot < 0.25:
                px[x, y] = BODY_DK
            elif dot > 0.55:
                px[x, y] = BODY_BR
            elif dot > 0.2:
                px[x, y] = BODY_LT
            elif dot < -0.25:
                px[x, y] = BODY_DK


def draw_body(draw):
    """Draw Koffing's spherical body."""
    draw.ellipse([3, 3, 28, 28], fill=BODY, outline=OL)


def draw_craters(draw):
    """Draw crater bumps on body edges."""
    # Top-right
    draw.ellipse([21, 0, 26, 4], fill=CRAT_R, outline=OL)
    draw.ellipse([22, 1, 25, 3], fill=CRAT_I)
    # Right
    draw.ellipse([27, 12, 31, 16], fill=CRAT_R, outline=OL)
    draw.ellipse([28, 13, 30, 15], fill=CRAT_I)
    # Bottom-left
    draw.ellipse([0, 20, 4, 24], fill=CRAT_R, outline=OL)
    draw.ellipse([1, 21, 3, 23], fill=CRAT_I)


CM = {'O': OL, 'S': SKULL, 'W': EYE_W, 'P': PUPIL, 'M': MOUTH, 'T': TEETH}


def draw_skull(draw):
    """Draw skull & crossbones marking on lower body."""
    pattern = parse_grid("""
..OOOO..
.OSSSSO.
OS.SS.SO
.OSSSSO.
..OOOO..
.O.OO.O.
O..OO..O
.O....O.
""", CM)
    stamp(draw, pattern, 12, 19)


# --- Faces ---

def draw_face_happy(draw):
    """Content smile — good air quality."""
    # Flat-top eyes (Koffing's signature angry brow)
    eye_l = parse_grid("""
OOOOOO
OWWWWO
OWWWPO
OWWPPO
.OOOO.
""", CM)
    stamp(draw, eye_l, 7, 8)

    eye_r = parse_grid("""
OOOOOO
OWWWWO
OPWWWO
OPPWWO
.OOOO.
""", CM)
    stamp(draw, eye_r, 18, 8)

    # Gentle curved smile
    smile = parse_grid("""
.O......O.
..O....O..
...OOOO...
""", CM)
    stamp(draw, smile, 10, 14)


def draw_face_grin(draw):
    """Wider grin — moderate air quality. He's starting to enjoy this."""
    # Narrower eyes (squinting with delight)
    eye_l = parse_grid("""
OOOOOO
OWWPPO
.OOOO.
""", CM)
    stamp(draw, eye_l, 7, 9)

    eye_r = parse_grid("""
OOOOOO
OPPWWO
.OOOO.
""", CM)
    stamp(draw, eye_r, 18, 9)

    # Wide toothy grin curving up at corners
    grin = parse_grid("""
O..........O
.OOOOOOOOOO.
.OTTTTTTTTO.
.OMMMMMMMMO.
..OOOOOOOO..
""", CM)
    stamp(draw, grin, 9, 13)


def draw_face_thrilled(draw):
    """Grin with raised eyebrows — getting excited. Transition to ecstatic."""
    # Raised eyebrow lines above the eyes
    brows = parse_grid("""
.OOOO....OOOO.
""", CM)
    stamp(draw, brows, 7, 7)

    # Squinting eyes like grin, but shifted down to make room for brows
    eye_l = parse_grid("""
OOOOOO
OWWPPO
.OOOO.
""", CM)
    stamp(draw, eye_l, 7, 9)

    eye_r = parse_grid("""
OOOOOO
OPPWWO
.OOOO.
""", CM)
    stamp(draw, eye_r, 18, 9)

    # Same wide grin as grin state but slightly bigger
    grin = parse_grid("""
O............O
.OOOOOOOOOOOO.
.OTTTTTTTTTTO.
.OMMMMMMMMMMO.
..OOOOOOOOOO..
""", CM)
    stamp(draw, grin, 8, 13)


def draw_face_ecstatic(draw):
    """Maniacal joy — terrible air quality. He LOVES this."""
    # Wide eyes with angry brow — wild-eyed, pupils tiny
    eye_l = parse_grid("""
OOOOOO
OWWWWO
OWWWWO
OWPWWO
OWWWWO
.OOOO.
""", CM)
    stamp(draw, eye_l, 7, 7)

    eye_r = parse_grid("""
OOOOOO
OWWWWO
OWWWWO
OWWPWO
OWWWWO
.OOOO.
""", CM)
    stamp(draw, eye_r, 18, 7)

    # HUGE toothy grin
    mouth = parse_grid("""
O............O
.OOOOOOOOOOOO.
.OTTTTTTTTTTO.
.OTTTTTTTTTTO.
.OMMMMMMMMMMO.
..OOOOOOOOOO..
""", CM)
    stamp(draw, mouth, 8, 14)


# --- Clouds ---

def make_cloud(size, circles):
    """Draw a cloud from overlapping ellipses.
    circles: list of (x1, y1, x2, y2, fill_index)."""
    img = make_image(size, size)
    draw = ImageDraw.Draw(img)
    for x1, y1, x2, y2, fill in circles:
        draw.ellipse([x1, y1, x2, y2], fill=fill, outline=OL)
    return img


def cloud_small():
    return make_cloud(12, [
        (1, 4, 8, 11, CLD),
        (3, 1, 10, 8, CLD_LT),
        (0, 3, 6, 9, CLD_DK),
    ])


def cloud_medium():
    return make_cloud(16, [
        (1, 5, 10, 14, CLD),
        (5, 1, 14, 10, CLD_LT),
        (2, 7, 10, 15, CLD_DK),
        (4, 3, 12, 11, CLD),
    ])


def cloud_large():
    return make_cloud(20, [
        (1, 7, 12, 18, CLD),
        (6, 1, 17, 12, CLD_LT),
        (2, 10, 12, 19, CLD_DK),
        (8, 3, 18, 13, CLD),
        (4, 5, 14, 15, CLD_LT),
    ])


# --- Sprite assembly ---

FACES = {
    'happy': draw_face_happy,
    'grin': draw_face_grin,
    'thrilled': draw_face_thrilled,
    'ecstatic': draw_face_ecstatic,
}


def create_koffing(state):
    """Create a full Koffing sprite for a given face state."""
    img = make_image(32, 32)
    draw = ImageDraw.Draw(img)
    draw_body(draw)
    shade_body(img)
    draw = ImageDraw.Draw(img)  # refresh after pixel manipulation
    draw.ellipse([3, 3, 28, 28], outline=OL)  # re-outline after shading
    draw_craters(draw)
    FACES[state](draw)
    draw_skull(draw)
    return img


# --- Export functions ---

def to_mono(img):
    """Convert indexed image to monochrome."""
    mono = Image.new('1', img.size, 0)
    src = img.load()
    dst = mono.load()
    for y in range(img.height):
        for x in range(img.width):
            if src[x, y] in MONO_WHITE:
                dst[x, y] = 1
    return mono


def save_xbm(mono_img, filepath, name):
    """Save 1-bit image as XBM (C-includable)."""
    w, h = mono_img.size
    px = mono_img.load()
    bpr = (w + 7) // 8
    data = []
    for y in range(h):
        for bx in range(bpr):
            byte = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x, y]:
                    byte |= (1 << bit)
            data.append(byte)

    safe = name.replace('-', '_')
    lines = [
        f"#define {safe}_width {w}",
        f"#define {safe}_height {h}",
        f"static unsigned char {safe}_bits[] = {{",
    ]
    hexvals = [f"0x{b:02x}" for b in data]
    for i in range(0, len(hexvals), 12):
        chunk = ", ".join(hexvals[i:i + 12])
        if i + 12 < len(hexvals):
            chunk += ","
        lines.append(f"   {chunk}")
    lines.append("};")

    with open(filepath, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def save_indexed_header(img, filepath, name):
    """Save as C header with 4-bit packed indexed pixel data."""
    w, h = img.size
    px = img.load()
    safe = name.replace('-', '_')

    data = []
    for y in range(h):
        for x in range(0, w, 2):
            hi = px[x, y] & 0x0F
            lo = px[x + 1, y] & 0x0F if x + 1 < w else 0
            data.append((hi << 4) | lo)

    lines = [
        f"#pragma once",
        f"#include <stdint.h>",
        f"#include <pgmspace.h>",
        f"",
        f"#define {safe.upper()}_WIDTH {w}",
        f"#define {safe.upper()}_HEIGHT {h}",
        f"",
        f"const uint8_t {safe}_data[] PROGMEM = {{",
    ]
    hexvals = [f"0x{b:02x}" for b in data]
    for i in range(0, len(hexvals), 16):
        chunk = ", ".join(hexvals[i:i + 16])
        if i + 16 < len(hexvals):
            chunk += ","
        lines.append(f"    {chunk}")
    lines.append("};")

    with open(filepath, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def rgb_to_565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def save_palette_header(filepath):
    """Save palette as RGB565 C header."""
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "#include <pgmspace.h>",
        "",
        "const uint16_t koffing_palette_normal[] PROGMEM = {",
    ]
    for idx in range(16):
        r, g, b = PALETTE_NORMAL.get(idx, (0, 0, 0))
        val = rgb_to_565(r, g, b)
        lines.append(f"    0x{val:04x},  // {idx}: ({r}, {g}, {b})")
    lines.append("};")

    with open(filepath, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def save_mono_header(mono_img, filepath, name):
    """Save monochrome bitmap as C header (same data as XBM but PROGMEM)."""
    w, h = mono_img.size
    px = mono_img.load()
    bpr = (w + 7) // 8
    data = []
    for y in range(h):
        for bx in range(bpr):
            byte = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x, y]:
                    byte |= (1 << bit)
            data.append(byte)

    safe = name.replace('-', '_')
    lines = [
        f"#pragma once",
        f"#include <stdint.h>",
        f"#include <pgmspace.h>",
        f"",
        f"#define {safe.upper()}_WIDTH {w}",
        f"#define {safe.upper()}_HEIGHT {h}",
        f"",
        f"const uint8_t {safe}_mono[] PROGMEM = {{",
    ]
    hexvals = [f"0x{b:02x}" for b in data]
    for i in range(0, len(hexvals), 12):
        chunk = ", ".join(hexvals[i:i + 12])
        if i + 12 < len(hexvals):
            chunk += ","
        lines.append(f"    {chunk}")
    lines.append("};")

    with open(filepath, 'w') as f:
        f.write('\n'.join(lines) + '\n')


# --- Scene composition ---

# Cloud pool — drawn incrementally as level increases (matches koffing_gfx.h)
# Small wisps interleaved between larger clouds for smooth buildup.
# Positions spread around the body (centered at 16,16 in 64x64 scene).
CLOUD_POOL = [
    ('small',  48, 12),   #  1: wisp, right
    ('small',  26,  0),   #  2: wisp, top-center
    ('medium',  0,  2),   #  3: puff, upper-left
    ('small',   0, 26),   #  4: wisp, left
    ('medium', 42, 42),   #  5: puff, lower-right
    ('small',  26, 52),   #  6: wisp, bottom-center
    ('large',   0, 40),   #  7: billow, lower-left
    ('small',  52, 28),   #  8: wisp, far-right
    ('large',  40,  0),   #  9: billow, upper-right
    ('small',  50, 50),   # 10: wisp, far lower-right
]

# Face: 0 → happy, 1-4 → grin, 5-6 → thrilled, 7-10 → ecstatic
def face_for_level(level):
    if level == 0: return 'happy'
    if level <= 4: return 'grin'
    if level <= 6: return 'thrilled'
    return 'ecstatic'

CLOUD_FNS = {'small': cloud_small, 'medium': cloud_medium, 'large': cloud_large}


def compose_scene(koffing_img, level):
    """Compose a 64x64 scene with Koffing + N clouds for given level (0-5)."""
    scene = make_image(64, 64)
    for i in range(min(level, len(CLOUD_POOL))):
        size_name, cx, cy = CLOUD_POOL[i]
        cld = CLOUD_FNS[size_name]()
        _paste_indexed(scene, cld, cx, cy)
    _paste_indexed(scene, koffing_img, 16, 16)
    return scene


def _paste_indexed(dst, src, ox, oy):
    """Paste src onto dst in palette mode, skipping BG (0) pixels."""
    dp = dst.load()
    sp = src.load()
    for y in range(src.height):
        for x in range(src.width):
            dx, dy = ox + x, oy + y
            if 0 <= dx < dst.width and 0 <= dy < dst.height:
                val = sp[x, y]
                if val != BG:
                    dp[dx, dy] = val


def render_scaled(img, scale, mono=False):
    """Render an indexed image to RGB at given scale."""
    if mono:
        m = to_mono(img)
        rgb = Image.new('RGB', m.size, (0, 0, 0))
        mp = m.load()
        rp = rgb.load()
        for y in range(m.height):
            for x in range(m.width):
                if mp[x, y]:
                    rp[x, y] = (255, 255, 255)
    else:
        rgb = img.convert('RGB')
    return rgb.resize((img.width * scale, img.height * scale), Image.NEAREST)


# --- Preview ---

def make_preview(sprites, scenes, filepath, scale=8):
    """Preview sheet: top = individual sprites, middle = color scenes, bottom = mono scenes."""
    margin = 8
    ms = margin * scale

    # Row 1: individual sprites
    max_w = max(s.width for _, s in sprites)
    max_h = max(s.height for _, s in sprites)
    sprite_cols = len(sprites)
    cell_w = (max_w + margin) * scale
    cell_h = (max_h + margin) * scale
    row1_w = cell_w * sprite_cols + ms

    # Row 2-3: scenes (64x64 each)
    scene_cols = len(scenes)
    scene_cell = (64 + margin) * scale
    row23_w = scene_cell * scene_cols + ms

    sheet_w = max(row1_w, row23_w)
    sheet_h = cell_h + ms + scene_cell * 2 + ms * 4

    sheet = Image.new('RGB', (sheet_w, sheet_h), (32, 32, 32))

    # Row 1: individual color sprites
    for i, (name, img) in enumerate(sprites):
        x = ms + i * cell_w
        sheet.paste(render_scaled(img, scale), (x, ms))

    # Row 2: color scenes
    y_off = ms + cell_h + ms
    for i, (label, scene) in enumerate(scenes):
        x = ms + i * scene_cell
        sheet.paste(render_scaled(scene, scale), (x, y_off))

    # Row 3: mono scenes
    y_off2 = y_off + scene_cell + ms
    for i, (label, scene) in enumerate(scenes):
        x = ms + i * scene_cell
        sheet.paste(render_scaled(scene, scale, mono=True), (x, y_off2))

    sheet.save(filepath)


# --- Main ---

def main():
    for d in [COLOR_DIR, MONO_DIR, HEADERS_DIR]:
        os.makedirs(d, exist_ok=True)

    sprites = []

    for state in ['happy', 'grin', 'thrilled', 'ecstatic']:
        img = create_koffing(state)
        name = f"koffing_{state}"

        img.save(os.path.join(COLOR_DIR, f"{name}.png"))

        mono = to_mono(img)
        save_xbm(mono, os.path.join(MONO_DIR, f"{name}.xbm"), name)
        save_mono_header(mono, os.path.join(HEADERS_DIR, f"{name}_mono.h"), name)
        save_indexed_header(img, os.path.join(HEADERS_DIR, f"{name}_color.h"), name)

        sprites.append((name, img))

    for size_name, fn in [('small', cloud_small), ('medium', cloud_medium), ('large', cloud_large)]:
        img = fn()
        name = f"cloud_{size_name}"

        img.save(os.path.join(COLOR_DIR, f"{name}.png"))

        mono = to_mono(img)
        save_xbm(mono, os.path.join(MONO_DIR, f"{name}.xbm"), name)
        save_mono_header(mono, os.path.join(HEADERS_DIR, f"{name}_mono.h"), name)
        save_indexed_header(img, os.path.join(HEADERS_DIR, f"{name}_color.h"), name)

        sprites.append((name, img))

    save_palette_header(os.path.join(HEADERS_DIR, "koffing_palettes.h"))

    # Compose scenes for all 6 levels (0-5)
    koffing_imgs = {s: create_koffing(s) for s in ['happy', 'grin', 'thrilled', 'ecstatic']}
    scenes = []
    for level in range(11):
        face = face_for_level(level)
        scene = compose_scene(koffing_imgs[face], level)
        scenes.append((f"Level {level}", scene))

    make_preview(sprites, scenes, os.path.join(ART_ROOT, "preview", "preview.png"))

    print("Generated:")
    for name, img in sprites:
        print(f"  {name}: {img.width}x{img.height}")
    print(f"\nPreview: {os.path.join(ART_ROOT, 'preview', 'preview.png')}")


if __name__ == '__main__':
    main()
