"""Draws the grass tiles in assets/tiles/ (64x64, each covers 2x2 map tiles
of 32x32 art pixels).

Colors follow the author's grass. Design rules, so the ground does not show
a grid: no bright corners or edges, light patches only away from the
borders, small details spread evenly. Most variants are plain grass; a few
have flowers. Run:  python3 tools/make_grass.py
"""
import math
import os
import random

from PIL import Image

N = 64
BASE = (92, 148, 21)
SHADE = (86, 140, 21)    # slightly darker patches
LIGHTER = (100, 158, 22)  # slightly lighter patches
LIGHT = (111, 175, 24)   # highlights on blades
DARK = (82, 119, 35)     # blade shadows
FLOWERS = [(224, 32, 36), (226, 224, 26), (240, 144, 144), (222, 128, 32), (240, 240, 240)]
CENTER = (226, 224, 26)


def smooth_noise(rng, cells=6):
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
    return max(0.0, min(1.0, (d - 3) / 10))


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

    # Grass blades: two or three thin strokes leaning apart, dark at the
    # root and light at the tip, spread over the block.
    placed, tries = 0, 0
    while placed < 30 and tries < 2000:
        tries += 1
        x, y = rng.randrange(2, N - 5), rng.randrange(4, N - 2)
        ring = [(x + dx, y + dy) for dx in range(-1, 6) for dy in range(-4, 2)]
        if not free(ring):
            continue
        tall = rng.choice((2, 3, 3))
        cells = [((x, y), DARK), ((x, y - 1), DARK), ((x - 1 if tall == 3 else x, y - 2), LIGHT if tall == 2 else DARK)]
        if tall == 3:
            cells.append(((x - 1, y - 3), LIGHT))
        cells += [((x + 2, y), DARK), ((x + 2, y - 1), DARK), ((x + 3, y - 2), LIGHT)]
        if rng.random() < 0.5:
            cells += [((x + 1, y), DARK), ((x + 1, y - 1), DARK), ((x + 1, y - 2), DARK), ((x + 1, y - 3), LIGHT)]
        put(cells)
        placed += 1
    # Single light and dark specks.
    for color, count in ((LIGHT, 26), (DARK, 18)):
        for _ in range(count):
            x, y = rng.randrange(N), rng.randrange(N)
            if (x, y) not in used:
                put([((x, y), color)])
    # Flowers: a few small ones, away from each other.
    for i in range(flowers):
        for _ in range(200):
            x, y = rng.randrange(4, N - 4), rng.randrange(4, N - 4)
            ring = [(x + dx, y + dy) for dx in range(-3, 4) for dy in range(-3, 5)]
            if not free(ring):
                continue
            petal = rng.choice(FLOWERS)
            if i % 2 == 0:  # a bigger flower: four round petals and a center
                cells = [((x, y), CENTER)]
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-2, 0), (2, 0), (0, -2), (0, 2),
                               (-1, -1), (1, -1), (-1, 1), (1, 1)):
                    if abs(dx) + abs(dy) < 3 and not (abs(dx) == 1 and abs(dy) == 1):
                        cells.append(((x + dx, y + dy), petal))
                cells += [((x, y + 3), DARK), ((x + 1, y + 3), DARK), ((x + 2, y + 1), DARK)]
            else:  # a small one: a 2x2 bud on a stem, never white (it reads as noise)
                if petal == (240, 240, 240):
                    petal = FLOWERS[1]
                cells = [((x, y), petal), ((x + 1, y), petal), ((x, y + 1), petal), ((x + 1, y + 1), CENTER),
                         ((x, y + 2), DARK), ((x, y + 3), DARK)]
            put(cells)
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
