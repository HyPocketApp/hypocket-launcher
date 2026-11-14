"""Draws the other map pieces in assets/tiles/: more kinds of tree, rocks,
water with its banks, the bridge, bushes, flowers and the lumberjack's house.

Same palette family as the grass and the trees (tools/make_grass.py,
tools/make_tiles.py). Light comes from the top left. Run:
    python3 tools/make_props.py

  pine_N.png, birch_N.png  16x24: other trees, drawn like tree_N.png
  rock_N.png      16x16: a boulder (rock_3 has moss)
  water_N.png     16x16: open water; the two frames alternate
  bank_X_N.png    16x16, transparent: the shore along one side (n, s, w, e)
                  where the next tile is land; three variants per side
  bank_in_XY.png  16x16, transparent: a bit of shore in one corner
  bank_out_XY.png 16x16, transparent: rounds the corner where the shore
                  turns (land on two sides)
  bridge_v.png    16x16: planks for walking north-south (bridge_h: east-west)
  bush_N.png      16x16: a bush (bush_2 has berries)
  flowers_N.png   16x16, transparent: a patch of flowers over the grass
  house.png       68x72: the house, for a block of 4x3 'H' tiles; drawn 2
                  pixels left of and 24 above its top-left tile
"""
import math
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'tiles')

G_BASE, G_SHADE, G_LIGHT, G_DARK = (92, 148, 21), (86, 140, 21), (111, 175, 24), (82, 119, 35)
SHADOW = (0, 0, 0, 70)
B_DARK, B_MID, B_LIGHT = (74, 48, 30), (106, 70, 42), (138, 96, 58)
# Pine needles: cooler and darker than the leaves.
P_OUT, P_DARK, P_MID, P_LIGHT, P_HI = (18, 48, 36), (28, 72, 46), (40, 94, 54), (58, 118, 62), (86, 146, 74)
# Birch leaves: warmer and lighter.
Y_OUT, Y_DARK, Y_MID, Y_LIGHT, Y_HI = (48, 84, 22), (78, 122, 28), (112, 156, 36), (150, 188, 52), (196, 216, 92)
BIRCH_W, BIRCH_S, BIRCH_K = (232, 228, 214), (186, 180, 166), (40, 36, 34)
# Stone.
R_OUT, R_DARK, R_MID, R_LIGHT, R_HI = (50, 52, 62), (88, 92, 104), (122, 126, 138), (156, 160, 170), (196, 198, 206)
# Water.
W_DEEP, W_MID, W_LIGHT, W_HI = (46, 102, 176), (58, 120, 196), (96, 158, 224), (186, 222, 248)
BANK, BANK_LIGHT = (96, 74, 44), (132, 104, 64)
# House.
ROOF_HI, ROOF_LIGHT, ROOF_MID, ROOF_DARK, ROOF_OUT = (196, 96, 72), (170, 74, 58), (146, 58, 48), (112, 42, 38), (70, 28, 30)
LOG_HI, LOG_MID, LOG_DARK = (176, 128, 80), (146, 102, 62), (98, 66, 40)
STONE, STONE_DARK = (150, 146, 140), (100, 96, 92)
GLASS, GLASS_HI = (240, 196, 96), (255, 232, 160)


def blank(w, h):
    return Image.new('RGBA', (w, h), (0, 0, 0, 0))


def put(px, x, y, c, w=None, h=None):
    if w is not None and not (0 <= x < w and 0 <= y < h):
        return
    px[x, y] = c + (255,) if len(c) == 3 else c


def ellipse_shadow(px, w, h, cx, cy, rx, ry):
    for y in range(h):
        for x in range(w):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1:
                px[x, y] = SHADOW


def outline(px, mask, color):
    for (x, y) in list(mask):
        if any((x + dx, y + dy) not in mask for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            px[x, y] = color + (255,)


def pine(seed):
    rng = random.Random(seed)
    w, h = 16, 24
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 8.5, 21.5, 6.5, 2.2)
    for y in range(17, 22):
        for x in range(7, 9):
            put(px, x, y, B_LIGHT if x == 7 else B_DARK)
    px[6, 21] = B_DARK + (255,)
    px[9, 21] = B_DARK + (255,)
    # Three tiers, each a triangle with a ragged lower edge.
    tiers = [(0, 7, 3.2), (4, 12, 5.4), (8, 18, 7.6)]
    mask, under = set(), set()
    for top, bottom, half in tiers:
        jag = [rng.choice((0, 0, 1)) for _ in range(w)]
        for y in range(top, bottom):
            k = (y - top + 1) / (bottom - top)
            hw = 0.6 + half * k
            for x in range(w):
                if abs(x + 0.5 - 8) <= hw and (y < bottom - 1 or not jag[x]):
                    mask.add((x, y))
                    if y >= bottom - 2:
                        under.add((x, y))
    for (x, y) in mask:
        lit = (x - 8) / 7.0 + rng.uniform(-0.15, 0.15) + ((x + y) & 1) * 0.1
        c = P_HI if lit < -0.55 else P_LIGHT if lit < -0.1 else P_MID if lit < 0.4 else P_DARK
        if (x, y) in under and (x, y - 3) in mask:
            c = P_DARK if c != P_HI else P_MID  # shade under the tier above
        px[x, y] = c + (255,)
    outline(px, mask, P_OUT)
    return img


def birch(seed):
    rng = random.Random(seed)
    w, h = 16, 24
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 8.5, 21.5, 6.0, 2.0)
    for y in range(9, 22):
        put(px, 7, y, BIRCH_W)
        put(px, 8, y, BIRCH_S)
    for y in (11, 14, 17, 19):  # dark marks
        px[7 + (y & 1), y] = BIRCH_K + (255,)
    px[6, 21] = BIRCH_S + (255,)
    px[9, 21] = BIRCH_S + (255,)
    lumps = [(8, 7.5, 5.2), (5, 9.5, 3.4), (11, 9.2, 3.4), (8, 3.8, 3.6),
             (5.5 + rng.uniform(-0.5, 0.5), 5, 2.8), (10.5 + rng.uniform(-0.5, 0.5), 5, 2.8)]
    mask = set()
    for y in range(h):
        for x in range(w):
            if y < 14 and any((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r for cx, cy, r in lumps):
                mask.add((x, y))
    # Gaps in the leaves let the trunk show.
    for (x, y) in ((7, 12), (8, 12), (8, 11), (7, 13), (8, 13)):
        mask.discard((x, y))
    for (x, y) in mask:
        lit = ((x - 8) * 0.7 + (y - 7) * 1.0) / 7.0 + rng.uniform(-0.15, 0.15) + ((x + y) & 1) * 0.08
        c = Y_HI if lit < -0.75 else Y_LIGHT if lit < -0.2 else Y_MID if lit < 0.35 else Y_DARK
        px[x, y] = c + (255,)
    for _ in range(4):
        x, y = rng.randrange(3, 12), rng.randrange(2, 10)
        if (x, y) in mask and (x + 1, y) in mask:
            px[x, y] = Y_HI + (255,)
    outline(px, mask, Y_OUT)
    return img


def rock(seed, moss=False):
    rng = random.Random(seed)
    w = h = 16
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 8.8, 13.2, 7.4, 2.4)
    lumps = [(8, 9, 5.6), (5.5, 10, 4.2), (10.8, 10, 4.2), (7.2 + rng.uniform(-1, 1), 6.2, 3.8)]
    lumps = [(cx + rng.uniform(-0.5, 0.5), cy + rng.uniform(-0.3, 0.3), r + rng.uniform(-0.3, 0.3)) for cx, cy, r in lumps]
    mask = {(x, y) for y in range(2, 15) for x in range(1, 15)
            if any((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r for cx, cy, r in lumps)}
    for (x, y) in mask:
        lit = ((x - 7) * 0.6 + (y - 7) * 1.0) / 7.0 + ((x * 3 + y) % 4 == 0) * 0.12
        c = R_HI if lit < -0.85 else R_LIGHT if lit < -0.3 else R_MID if lit < 0.35 else R_DARK
        px[x, y] = c + (255,)
    # A crack.
    x, y = rng.randrange(6, 10), 7
    for _ in range(4):
        if (x, y) in mask:
            px[x, y] = R_DARK + (255,)
        x += rng.choice((-1, 0, 1))
        y += 1
    if moss:
        for (x, y) in mask:
            if y <= 7 and rng.random() < 0.55 and (x, y - 1) not in mask or (y <= 5 and rng.random() < 0.4 and (x, y) in mask):
                px[x, y] = (G_LIGHT if x < 8 else G_BASE) + (255,)
    outline(px, mask, R_OUT)
    return img


def water(frame):
    rng = random.Random(77)
    img = Image.new('RGBA', (16, 16), W_MID + (255,))
    px = img.load()
    for y in range(16):
        for x in range(16):
            if rng.random() < 0.18:
                px[x, y] = W_DEEP + (255,)
    # Ripples: short light dashes that shift between the two frames.
    ripples = [(2, 3, 4), (9, 6, 3), (4, 11, 3), (11, 13, 4)]
    for x, y, n in ripples:
        x += frame * 2 if y % 2 else -frame
        for k in range(n):
            px[(x + k) % 16, y] = (W_LIGHT if 0 < k < n - 1 else W_MID) + (255,)
        if frame == 0:
            px[(x + 1) % 16, y] = W_HI + (255,)
    return img


def bank(side, seed):
    """Land along one side of the water: grass, wet earth, then foam."""
    rng = random.Random(seed)
    img = blank(16, 16)
    px = img.load()
    depth = [2] * 16
    for i in range(1, 15):
        d = depth[i - 1] + rng.choice((-1, 0, 0, 1))
        depth[i] = max(1, min(3, d, 2 + (15 - i)))
    for i in range(16):
        d = depth[i]
        earth = [BANK_LIGHT, BANK] if side == 'n' else [BANK]  # the far bank shows its face
        layers = [G_BASE] * (d - 1) + [G_SHADE] + earth + [W_HI]
        for k, c in enumerate(layers):
            if side == 'n': x, y = i, k
            elif side == 's': x, y = i, 15 - k
            elif side == 'w': x, y = k, i
            else: x, y = 15 - k, i
            if c == W_HI and (i + seed) % 3 == 0:
                c = W_LIGHT
            px[x, y] = c + (255,)
    return img


def bank_inner(corner):
    img = blank(16, 16)
    px = img.load()
    land = [(0, 0), (1, 0), (0, 1)]
    edge = [(2, 0), (1, 1), (0, 2)]
    foam = [(3, 0), (2, 1), (1, 2), (0, 3)]
    for cells, c in ((land, G_SHADE), (edge, BANK), (foam, W_HI)):
        for x, y in cells:
            if 'e' in corner: x = 15 - x
            if 's' in corner: y = 15 - y
            px[x, y] = c + (255,)
    return img


def corner_out(corner, rings, depth):
    """Rounds the corner where land is on two sides (ne, nw, se, sw): land
    outside a quarter circle, then the rings (outermost first) along it."""
    img = blank(16, 16)
    px = img.load()
    c = 8.0
    radius = c - depth
    for y in range(8):
        for x in range(8):
            s = math.hypot(c - x - 0.5, c - y - 0.5) - radius
            k = None
            if s > len(rings) - 0.5:
                k = 0
            else:
                for i in range(len(rings)):
                    if s > len(rings) - 1.5 - i:
                        k = i + 1
                        break
            if k is None:
                continue
            col = G_BASE if k == 0 else rings[k - 1]
            xx, yy = (15 - x if 'e' in corner else x), (15 - y if 's' in corner else y)
            px[xx, yy] = col + (255,)
    return img


def bridge(vertical):
    img = blank(16, 16)
    px = img.load()
    for a in range(16):      # along the walking direction
        for b in range(1, 15):  # across it
            plank = a % 4
            c = LOG_DARK if plank == 3 else LOG_HI if plank == 0 else LOG_MID
            if (a // 4 + b) % 7 == 0 and plank != 3:
                c = LOG_DARK  # a knot
            x, y = (b, a) if vertical else (a, b)
            px[x, y] = c + (255,)
    for a in range(16):  # rails
        for b, c in ((0, B_DARK), (1, LOG_DARK), (14, LOG_DARK), (15, B_DARK)):
            x, y = (b, a) if vertical else (a, b)
            px[x, y] = c + (255,)
    for a in (1, 9):  # posts
        for b in (0, 15):
            x, y = (b, a) if vertical else (a, b)
            px[x, y] = B_LIGHT + (255,)
    return img


def bush(seed, berries=False):
    rng = random.Random(seed)
    w = h = 16
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 8.6, 13.4, 7.0, 2.2)
    lumps = [(8, 9.5, 5.0), (4.8, 10.5, 3.6), (11.2, 10.5, 3.6), (6.5, 6.5, 3.4), (10, 6.8, 3.2)]
    lumps = [(cx + rng.uniform(-0.4, 0.4), cy + rng.uniform(-0.4, 0.4), r) for cx, cy, r in lumps]
    mask = {(x, y) for y in range(16) for x in range(16) if y < 15 and
            any((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r for cx, cy, r in lumps)}
    leaves = [(30, 66, 28), (44, 92, 30), (58, 116, 30), (78, 142, 32), (108, 170, 40)]
    for (x, y) in mask:
        lit = ((x - 8) * 0.7 + (y - 9) * 1.0) / 6.0 + rng.uniform(-0.15, 0.15) + ((x + y) & 1) * 0.08
        c = leaves[4] if lit < -0.8 else leaves[3] if lit < -0.25 else leaves[2] if lit < 0.35 else leaves[1]
        px[x, y] = c + (255,)
    if berries:
        for _ in range(6):
            x, y = rng.randrange(3, 13), rng.randrange(6, 13)
            if (x, y) in mask and (x + 1, y) in mask:
                px[x, y] = (196, 40, 52, 255)
                px[x + 1, y] = (120, 20, 36, 255) if (x + 1, y + 1) in mask else px[x + 1, y]
    outline(px, mask, leaves[0])
    return img


def flowers(seed):
    rng = random.Random(seed)
    img = blank(16, 16)
    px = img.load()
    colors = [(226, 224, 26), (240, 240, 240), (224, 32, 36), (240, 144, 200), (150, 120, 230)]
    pick = rng.sample(colors, 2)
    spots = []
    while len(spots) < 6:
        x, y = rng.randrange(2, 13), rng.randrange(3, 14)
        if all(abs(x - a) + abs(y - b) > 3 for a, b in spots):
            spots.append((x, y))
    for i, (x, y) in enumerate(spots):
        petal = pick[i % 2]
        px[x, y + 2] = G_DARK + (255,)  # stem
        px[x, y + 1] = G_DARK + (255,)
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            px[x + dx, y + dy] = petal + (255,)
        px[x, y] = (250, 200, 60, 255) if petal != colors[0] else (200, 110, 30, 255)
        if y + 3 < 16:
            px[x + 1, y + 3] = (0, 0, 0, 40)
    for _ in range(4):  # tufts
        x, y = rng.randrange(1, 15), rng.randrange(2, 15)
        if px[x, y][3] == 0 and px[x, y - 1][3] == 0:
            px[x, y] = G_DARK + (255,)
            px[x, y - 1] = G_LIGHT + (255,)
    return img


def house():
    W, H = 68, 72
    img = blank(W, H)
    px = img.load()
    # Shadow on the ground to the right and below.
    for y in range(66, 72):
        for x in range(4, 68):
            px[x, y] = SHADOW
    # Walls of logs, sprite y 40..69 (the house tiles start at y 24).
    for y in range(40, 69):
        row = (y - 40) % 5
        c = LOG_HI if row == 0 else LOG_DARK if row == 4 else LOG_MID
        for x in range(4, 64):
            px[x, y] = c + (255,)
        if row == 2:
            for x in range(4, 64):
                if (x * 7 + y) % 13 == 0:
                    px[x, y] = LOG_DARK + (255,)  # grain
    # Corner posts with log ends.
    for y in range(40, 69):
        for x in (2, 3, 4, 63, 64, 65):
            row = (y - 40) % 5
            c = LOG_HI if row in (1, 2) else LOG_MID if row in (0, 3) else LOG_DARK
            px[x, y] = c + (255,)
        px[2, y] = LOG_DARK + (255,)
        px[65, y] = LOG_DARK + (255,)
    # Stone base.
    for y in range(67, 70):
        for x in range(2, 66):
            px[x, y] = (STONE if (x // 4 + y) % 3 else STONE_DARK) + (255,)
    for x in range(2, 66, 4):
        px[x, 67] = STONE_DARK + (255,)
    # Roof: shingles from the ridge (y 10) to the eave (y 42), with overhang.
    for y in range(10, 43):
        for x in range(0, 68):
            r = (y - 10) // 4
            xx = x + (r % 2) * 3
            c = ROOF_MID
            if y - 10 < 4:
                c = ROOF_LIGHT
            if (y - 10) % 4 == 3:
                c = ROOF_DARK
            elif xx % 6 == 0:
                c = ROOF_DARK
            elif (y - 10) % 4 == 0 and y > 14:
                c = ROOF_LIGHT
            px[x, y] = c + (255,)
    for x in range(0, 68):  # ridge cap and eave
        px[x, 10] = ROOF_OUT + (255,)
        px[x, 11] = ROOF_HI + (255,)
        px[x, 12] = ROOF_LIGHT + (255,)
        px[x, 41] = ROOF_OUT + (255,)
        px[x, 42] = ROOF_OUT + (255,)
    for y in range(10, 43):
        px[0, y] = ROOF_OUT + (255,)
        px[67, y] = ROOF_OUT + (255,)
    for x in range(4, 64):  # shade under the eave
        px[x, 43] = LOG_DARK + (255,)
        px[x, 44] = (LOG_DARK if x % 2 else LOG_MID) + (255,)
    # Chimney.
    for y in range(0, 22):
        for x in range(48, 58):
            c = STONE if ((x - 48) // 3 + (y // 3)) % 2 else (136, 132, 126)
            if y % 3 == 2:
                c = STONE_DARK
            if x in (48, 57):
                c = (70, 66, 64)
            px[x, y] = c + (255,)
    for x in range(47, 59):
        px[x, 0] = (70, 66, 64, 255)
        px[x, 1] = (60, 56, 54, 255)
    # Door: in the second tile column of the bottom row (sprite x 18..34).
    for y in range(50, 69):
        for x in range(20, 32):
            edge = x in (20, 31) or y == 50
            c = LOG_DARK if edge else (B_MID if (x - 21) % 3 else B_DARK)
            if y == 51 and x in (20, 31):
                c = LOG_DARK
            px[x, y] = c + (255,)
    for x in range(21, 31):
        px[x, 50] = (54, 36, 24, 255)
    px[29, 60] = (230, 190, 70, 255)  # knob
    px[29, 61] = (150, 110, 40, 255)
    for x in range(19, 33):  # step
        px[x, 69] = STONE + (255,)
        px[x, 70] = STONE_DARK + (255,)
    # Windows with a warm light.
    def window(x0, y0):
        for y in range(y0, y0 + 10):
            for x in range(x0, x0 + 11):
                frame = x in (x0, x0 + 10) or y in (y0, y0 + 9) or x == x0 + 5 or y == y0 + 4
                c = B_DARK if frame else (GLASS_HI if (x - x0) + (y - y0) < 6 else GLASS)
                px[x, y] = c + (255,)
        for x in range(x0 - 1, x0 + 12):  # sill and flower box
            px[x, y0 + 10] = LOG_HI + (255,)
            px[x, y0 + 11] = LOG_DARK + (255,)
        for x in range(x0, x0 + 11, 2):
            px[x, y0 + 9] = ((214, 58, 62) if x % 4 else (240, 220, 80)) + (255,)
            px[x + 1, y0 + 9] = G_BASE + (255,)
    window(6, 51)
    window(44, 51)
    return img


def save(img, name):
    img.save(os.path.join(OUT, name + '.png'), optimize=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    for i, seed in enumerate((2, 7, 13), 1):
        save(pine(seed), 'pine_%d' % i)
    for i, seed in enumerate((4, 11), 1):
        save(birch(seed), 'birch_%d' % i)
    save(rock(3), 'rock_1')
    save(rock(9), 'rock_2')
    save(rock(17, moss=True), 'rock_3')
    for f in range(2):
        save(water(f), 'water_%d' % (f + 1))
    for side, seed in (('n', 50), ('s', 60), ('w', 70), ('e', 80)):
        for v in range(1, 4):
            save(bank(side, seed + v), 'bank_%s_%d' % (side, v))
    for corner in ('ne', 'nw', 'se', 'sw'):
        save(bank_inner(corner), 'bank_in_' + corner)
        rings = [G_SHADE, BANK_LIGHT if 'n' in corner else BANK, BANK, W_HI] if 'n' in corner else [G_SHADE, BANK, W_HI]
        save(corner_out(corner, rings, 2.5), 'bank_out_' + corner)
    save(bridge(True), 'bridge_v')
    save(bridge(False), 'bridge_h')
    save(bush(5), 'bush_1')
    save(bush(12, berries=True), 'bush_2')
    for i, seed in enumerate((1, 6, 15), 1):
        save(flowers(seed), 'flowers_%d' % i)
    save(house(), 'house')
    print('wrote props to', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
