"""Draws the grass tiles in assets/tiles/ (32x32, each covers 2x2 map tiles).

Colors follow the author's grass. Design rules, so the ground does not show
a grid: no bright corners or edges, light patches only away from the
borders, small details spread evenly. Most variants are plain grass; a few
have flowers. Run:  python3 tools/make_grass.py
"""
import math
import os
import random

from PIL import Image

N = 32
BASE = (92, 148, 21)
SHADE = (86, 140, 21)    # slightly darker patches
LIGHTER = (100, 158, 22)  # slightly lighter patches
LIGHT = (111, 175, 24)   # highlights on blades
DARK = (82, 119, 35)     # blade shadows
FLOWERS = [(224, 32, 36), (226, 224, 26), (240, 144, 144), (222, 128, 32), (240, 240, 240)]
CENTER = (226, 224, 26)


def smooth_noise(rng, cells=4):
    """Value noise on a wrapped grid of cells x cells, sampled at N x N."""
    grid = [[rng.random() for _ in range(cells)] for _ in range(cells)]

    def at(x, y):
        fx, fy = x / N * cells, y / N * cells
        x0, y0 = int(fx), int(fy)
        tx, ty = fx - x0, fy - y0
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        g = lambda i, j: grid[j % cells][i % cells]
        a = g(x0, y0) + (g(x0 + 1, y0) - g(x0, y0)) * tx
        b = g(x0, y0 + 1) + (g(x0 + 1, y0 + 1) - g(x0, y0 + 1)) * tx
        return a + (b - a) * ty

    return at


def edge_fade(x, y):
    """1 in the middle, 0 at the borders: patches never reach a block edge."""
    d = min(x, y, N - 1 - x, N - 1 - y)
    return max(0.0, min(1.0, (d - 2) / 6))


def make_tile(seed, flowers):
    rng = random.Random(seed)
    img = Image.new('RGBA', (N, N), BASE + (255,))
    px = img.load()
    noise = smooth_noise(rng)
    for y in range(N):
        for x in range(N):
            n = 0.5 + (noise(x, y) - 0.5) * edge_fade(x, y)
            if n > 0.66:
                px[x, y] = LIGHTER + (255,)
            elif n < 0.34:
                px[x, y] = SHADE + (255,)
    used = set()

    def free(cells):
        return all((cx, cy) not in used and 0 <= cx < N and 0 <= cy < N for cx, cy in cells)

    def put(cells_colors):
        for (cx, cy), c in cells_colors:
            px[cx, cy] = c + (255,)
            used.add((cx, cy))

    # Grass blades: a small dark "^" with a light tip, spread over the block.
    placed, tries = 0, 0
    while placed < 9 and tries < 400:
        tries += 1
        x, y = rng.randrange(1, N - 3), rng.randrange(2, N - 1)
        cells = [(x, y), (x + 1, y - 1), (x + 2, y), (x + 1, y)]
        ring = [(x + dx, y + dy) for dx in range(-1, 4) for dy in range(-2, 2)]
        if not free(ring):
            continue
        put([((x, y), DARK), ((x + 1, y - 1), DARK), ((x + 2, y), DARK), ((x + 1, y), LIGHT)])
        placed += 1
    # Single light and dark specks.
    for color, count in ((LIGHT, 7), (DARK, 5)):
        for _ in range(count):
            x, y = rng.randrange(N), rng.randrange(N)
            if (x, y) not in used:
                put([((x, y), color)])
    # Flowers: a few small ones, away from each other.
    for i in range(flowers):
        for _ in range(100):
            x, y = rng.randrange(2, N - 2), rng.randrange(2, N - 2)
            ring = [(x + dx, y + dy) for dx in range(-2, 3) for dy in range(-2, 3)]
            if not free(ring):
                continue
            petal = rng.choice(FLOWERS)
            if i == 0 and rng.random() < 0.6:  # one bigger flower: a cross with a center
                put([((x, y), CENTER), ((x - 1, y), petal), ((x + 1, y), petal),
                     ((x, y - 1), petal), ((x, y + 1), petal), ((x + 1, y + 1), DARK)])
            else:
                put([((x, y), petal), ((x, y + 1), DARK)])
            break
    return img


def main():
    out = os.path.join(os.path.dirname(__file__), '..', 'assets', 'tiles')
    os.makedirs(out, exist_ok=True)
    for name in os.listdir(out):
        if name.startswith('grass_') and name.endswith('.png'):
            os.remove(os.path.join(out, name))
    # Four plain variants and two with flowers.
    variants = [(11, 0), (23, 0), (37, 0), (41, 0), (53, 3), (67, 2)]
    for i, (seed, flowers) in enumerate(variants, 1):
        make_tile(seed, flowers).save(os.path.join(out, 'grass_%d.png' % i), optimize=True)
    print('wrote %d grass tiles to %s' % (len(variants), os.path.normpath(out)))


if __name__ == '__main__':
    main()
