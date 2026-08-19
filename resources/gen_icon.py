#!/usr/bin/env python3
"""Generate a simple SVY icon PNG using only Python stdlib (no Pillow)."""
import struct, zlib, math, sys, os

def png(width, height, pixels):
    """pixels: list of (r,g,b,a) tuples, row-major."""
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b''
    for row in range(height):
        raw += b'\x00'
        for col in range(width):
            r, g, b, a = pixels[row * width + col]
            raw += bytes([r, g, b, a])

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    # RGBA = color type 6
    ihdr = struct.pack('>II', width, height) + bytes([8, 6, 0, 0, 0])
    idat = zlib.compress(raw, 9)
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', idat)
            + chunk(b'IEND', b''))

def make_icon(size):
    pixels = []
    cx, cy = size / 2, size / 2
    r = size / 2

    # Rounded-rect background: dark navy #0B1220
    bg = (11, 18, 32, 255)
    # Accent: bright blue #3B82F6
    fg = (59, 130, 246, 255)
    corner_r = size * 0.18

    def in_rounded_rect(x, y):
        # Distance from nearest corner centre
        lx = min(x, size - 1 - x)
        ly = min(y, size - 1 - y)
        if lx > corner_r or ly > corner_r:
            return True
        dx = corner_r - lx
        dy = corner_r - ly
        return math.sqrt(dx*dx + dy*dy) <= corner_r

    # Simple bitmap font for "SVY" — 5x7 pixel patterns
    glyphs = {
        'S': [
            "01110",
            "10001",
            "10000",
            "01110",
            "00001",
            "10001",
            "01110",
        ],
        'V': [
            "10001",
            "10001",
            "10001",
            "10001",
            "01010",
            "01010",
            "00100",
        ],
        'Y': [
            "10001",
            "10001",
            "01010",
            "00100",
            "00100",
            "00100",
            "00100",
        ],
    }

    # Scale glyph to fit nicely
    glyph_w = 5
    glyph_h = 7
    gap = max(1, size // 32)
    total_w = 3 * glyph_w + 2 * gap
    scale = max(1, int(size * 0.55 / total_w))
    scaled_gw = glyph_w * scale
    scaled_gh = glyph_h * scale
    scaled_total_w = 3 * scaled_gw + 2 * gap * scale
    ox = int((size - scaled_total_w) / 2)
    oy = int((size - scaled_gh) / 2)

    # Build a set of lit pixels
    lit = set()
    for gi, ch in enumerate("SVY"):
        glyph = glyphs[ch]
        gx = ox + gi * (scaled_gw + gap * scale)
        for row, line in enumerate(glyph):
            for col, bit in enumerate(line):
                if bit == '1':
                    for dr in range(scale):
                        for dc in range(scale):
                            lit.add((gy := oy + row * scale + dr, gx + col * scale + dc))

    for y in range(size):
        for x in range(size):
            if not in_rounded_rect(x, y):
                pixels.append((0, 0, 0, 0))  # transparent outside rounded rect
            elif (y, x) in lit:
                pixels.append(fg)
            else:
                pixels.append(bg)

    return png(size, size, pixels)

if __name__ == '__main__':
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out_dir, exist_ok=True)
    for size in [16, 32, 64, 128, 256, 512, 1024]:
        data = make_icon(size)
        path = os.path.join(out_dir, f'icon_{size}x{size}.png')
        with open(path, 'wb') as f:
            f.write(data)
        print(f'Generated {path}')
