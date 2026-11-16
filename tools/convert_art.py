#!/usr/bin/env python3
"""Convert an image to a 96x72, 15-color PC Engine background-tile image."""
from collections import Counter
from pathlib import Path
import struct
import sys

from PIL import Image, ImageOps

WIDTH, HEIGHT = 96, 72


def _rgb333(rgb):
    r, g, b = ((channel * 7 + 127) // 255 for channel in rgb)
    return (g << 6) | (r << 3) | b


def _rgb888(word):
    return tuple((channel * 255 + 3) // 7 for channel in
                 ((word >> 3) & 7, (word >> 6) & 7, word & 7))


def _distance(first, second):
    return sum((a - b) ** 2 for a, b in zip(first, second))


def _hardware_palette(canvas, indexed):
    raw_palette = indexed.getpalette()
    selected = []
    # Keep every hardware color the old image actually used. Rounding RGB888
    # centers can collapse several indices onto the same RGB333 color.
    for _, index in sorted(indexed.getcolors(), key=lambda item: item[1]):
        word = _rgb333(raw_palette[index * 3:index * 3 + 3])
        if word not in selected:
            selected.append(word)

    source = canvas.load()
    pixels = [source[x, y] for y in range(canvas.height) for x in range(canvas.width)]
    histogram = Counter(_rgb333(pixel) for pixel in pixels)
    hardware_rgb = {word: _rgb888(word) for word in histogram}
    available = set(histogram) - set(selected)
    errors = {word: min(_distance(hardware_rgb[word], _rgb888(color))
                        for color in selected) for word in available}
    while len(selected) < 15 and available:
        # Reclaim duplicate slots for frequently occurring, poorly represented
        # source colors. Numeric tie-breaking keeps output reproducible.
        word = max(available, key=lambda value: (histogram[value] * errors[value], -value))
        selected.append(word)
        available.remove(word)
        for value in available:
            errors[value] = min(errors[value], _distance(hardware_rgb[value], hardware_rgb[word]))

    palette_rgb = [_rgb888(word) for word in selected]
    mapping = {}
    indices = []
    for pixel in pixels:
        if pixel not in mapping:
            mapping[pixel] = 1 + min(range(len(selected)),
                                     key=lambda i: _distance(pixel, palette_rgb[i]))
        indices.append(mapping[pixel])
    return [0] + selected + [0] * (15 - len(selected)), indices


def _canvas(source):
    with Image.open(source) as original:
        original.load()
        image = ImageOps.contain(original.convert("RGB"), (WIDTH, HEIGHT),
                                 method=Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
    return canvas


def _palette(canvas):
    indexed = canvas.quantize(colors=15, method=Image.Quantize.MEDIANCUT,
                              dither=Image.Dither.FLOYDSTEINBERG)
    return _hardware_palette(canvas, indexed)


def _tiles(pixels):
    out = bytearray()
    for y0 in range(0, HEIGHT, 8):
        for x0 in range(0, WIDTH, 8):
            low = bytearray()
            high = bytearray()
            for y in range(y0, y0 + 8):
                planes = [0, 0, 0, 0]
                for x in range(x0, x0 + 8):
                    index = pixels[y * WIDTH + x]
                    for bit in range(4):
                        planes[bit] = (planes[bit] << 1) | ((index >> bit) & 1)
                low.extend(planes[:2])
                high.extend(planes[2:])
            out.extend(low + high)
    return bytes(out)


def convert(source):
    """Legacy single-palette output; byte-compatible with existing SD assets."""
    colors, pixels = _palette(_canvas(source))
    return struct.pack("<16H", *colors) + _tiles(pixels)


def convert_two_palette(source):
    """Two fixed RGB333 palettes, with one palette choice per 8x8 tile.

    Palette zero is the exact legacy result. Only tiles with strictly lower
    reconstruction error choose palette one, so every tile retains a fallback.
    Fit palette one to poorly represented tiles, then refine its assignments.
    """
    canvas = _canvas(source)
    colors0, baseline = _palette(canvas)
    source = canvas.load()
    rgb = [source[x, y] for y in range(HEIGHT) for x in range(WIDTH)]
    locations = [[(tile // 12 * 8 + y) * WIDTH + tile % 12 * 8 + x
                  for y in range(8) for x in range(8)] for tile in range(108)]
    error0 = [_distance(pixel, _rgb888(colors0[index]))
              for pixel, index in zip(rgb, baseline)]
    tile_error0 = [sum(error0[index] for index in tile) for tile in locations]
    best_error = sum(tile_error0)
    colors1, chosen, result = colors0, [False] * 108, baseline
    group = sorted(range(108), key=lambda tile: (-tile_error0[tile], tile))[:36]
    for _ in range(4):
        training = Image.new("RGB", (64, len(group)))
        training.putdata([rgb[index] for tile in group for index in locations[tile]])
        candidate, _ = _palette(training)
        palette_rgb = [_rgb888(word) for word in candidate[1:]]
        mapping = {}
        mapped, errors = [], []
        for pixel in rgb:
            if pixel not in mapping:
                index = min(range(15), key=lambda item: _distance(pixel, palette_rgb[item]))
                mapping[pixel] = (index + 1, _distance(pixel, palette_rgb[index]))
            index, error = mapping[pixel]
            mapped.append(index)
            errors.append(error)
        tile_errors = [sum(errors[index] for index in tile) for tile in locations]
        choices = [error < old for error, old in zip(tile_errors, tile_error0)]
        error = sum(min(a, b) for a, b in zip(tile_errors, tile_error0))
        if error >= best_error:
            break
        best_error = error
        colors1, chosen = candidate, choices
        result = baseline.copy()
        for tile, use_second in enumerate(choices):
            if use_second:
                for index in locations[tile]:
                    result[index] = mapped[index]
        new_group = [tile for tile, use_second in enumerate(choices) if use_second]
        if new_group == group:
            break
        group = new_group
    bitmap = bytearray(14)
    for tile, second in enumerate(chosen):
        if second:
            bitmap[tile // 8] |= 1 << (tile % 8)
    header = struct.pack("<4sBBBBHHI", b"TPA2", 2, WIDTH, HEIGHT, 2, 108, 14, 0)
    return header + struct.pack("<32H", *(colors0 + colors1)) + bitmap + _tiles(result)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: convert_art.py input.png output.bin")
    destination = Path(sys.argv[2])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(convert(sys.argv[1]))


if __name__ == "__main__":
    main()
