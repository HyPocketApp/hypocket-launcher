"""Draws the other map pieces in assets/tiles/: more kinds of tree, rocks,
water with its banks, the bridge, bushes, flowers and the lumberjack's house.

Every map tile is 32x32 art pixels. Same palette family as the grass and the
trees (tools/make_grass.py, tools/make_tiles.py). Light comes from the top
left. Run:  python3 tools/make_props.py

  pine_N.png, birch_N.png  32x48: other trees, drawn like tree_N.png
  rock_N.png      32x32: a boulder (rock_3 has moss)
  water_N.png     32x32: open water; the two frames alternate
  bank_X_N.png    32x32, transparent: the shore along one side (n, s, w, e)
                  where the next tile is land; three variants per side
  bank_in_XY.png  32x32, transparent: a bit of shore in one corner
  bank_out_XY.png 32x32, transparent: rounds the corner where the shore
                  turns (land on two sides)
  bridge_v.png    32x32: planks for walking north-south (bridge_h: east-west)
  bush_N.png      32x32: a bush (bush_2 has berries)
  flowers_N.png   32x32, transparent: a patch of flowers over the grass
  house.png       136x144: the house, for a block of 4x3 'H' tiles; drawn 4
                  pixels left of and 48 above its top-left tile
"""
import math
import os
import random

from PIL import Image

from make_tiles import blank, ellipse_shadow, outline, lumpy, leaf_clusters, B_OUT

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'tiles')
T = 32

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


def put(px, x, y, c):
    px[x, y] = c + (255,) if len(c) == 3 else c


def pine(seed):
    rng = random.Random(seed)
    w, h = T, 48
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 17, 43, 13, 4.4)
    trunk = {(x, y) for y in range(32, 44) for x in range(14, 18)} | {(13, 43), (18, 43)}
    for (x, y) in trunk:
        put(px, x, y, B_LIGHT if x == 14 else B_MID if x < 17 else B_DARK)
    outline(px, trunk, B_OUT)
    # Four tiers, each a triangle with a ragged lower edge of drooping tips.
    tiers = [(0, 13, 6.0), (7, 22, 9.6), (14, 30, 12.6), (21, 37, 15.2)]
    mask, under = set(), set()
    for top, bottom, half in tiers:
        tips = [rng.choice((0, 1, 2, 2, 3)) for _ in range(w)]
        for y in range(top, bottom + 3):
            k = (y - top + 1) / (bottom - top)
            hw = 1.0 + half * min(1.0, k)
            for x in range(w):
                dx = abs(x + 0.5 - 16)
                if dx > hw:
                    continue
                # Below the tier's line only the drooping tips hang.
                if y >= bottom and (y - bottom >= tips[x] or (x // 3) % 2):
                    continue
                mask.add((x, y))
                if y >= bottom - 3:
                    under.add((x, y))
    for (x, y) in mask:
        lit = (x - 16) / 14.0 + rng.uniform(-0.12, 0.12) + ((x + y) & 1) * 0.08
        c = P_HI if lit < -0.55 else P_LIGHT if lit < -0.1 else P_MID if lit < 0.4 else P_DARK
        if (x, y) in under and (x, y - 6) in mask:
            c = P_DARK if c != P_HI else P_MID  # shade under the tier above
        put(px, x, y, c)
    # Needle strokes: short diagonal lines down and outward.
    for _ in range(40):
        x, y = rng.randrange(4, 28), rng.randrange(3, 36)
        d = -1 if x < 16 else 1
        c = P_HI if x < 14 else P_LIGHT if x < 18 else P_MID
        for k in range(3):
            if (x + d * k, y + k) in mask and (x + d * k, y + k) not in under:
                put(px, x + d * k, y + k, c)
    outline(px, mask, P_OUT)
    return img


def birch(seed):
    rng = random.Random(seed)
    w, h = T, 48
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 17, 43, 12, 4)
    trunk = {(x, y) for y in range(16, 44) for x in range(14, 18)} | {(13, 43), (18, 43)}
    for (x, y) in trunk:
        put(px, x, y, BIRCH_W if x < 16 else BIRCH_S)
    for y in (21, 26, 31, 35, 39):  # dark marks
        x = 14 + (y % 3)
        put(px, x, y, BIRCH_K)
        put(px, x + 1, y, BIRCH_K)
    outline(px, trunk, (70, 66, 60))
    lumps = [(16, 15, 10.4), (10, 19, 6.8), (22, 18.4, 6.8), (16, 7.6, 7.2),
             (11 + rng.uniform(-1, 1), 10, 5.6), (21 + rng.uniform(-1, 1), 10, 5.6)]
    mask = lumpy(lumps, w, h, lambda x, y: y < 28)
    # Gaps in the leaves let the trunk show.
    for (x, y) in [(x, y) for x in range(14, 18) for y in range(22, 28)]:
        mask.discard((x, y))
    leaf_clusters(px, rng, mask, (Y_OUT, Y_DARK, Y_MID, Y_LIGHT, Y_HI), 22, (5, 2, 27, 25),
                  center=(16, 13), scale=14)
    return img


def rock(seed, moss=False):
    rng = random.Random(seed)
    w = h = T
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 17.6, 26.4, 14.8, 4.8)
    lumps = [(16, 18, 11.2), (11, 20, 8.4), (21.6, 20, 8.4), (14.4 + rng.uniform(-2, 2), 12.4, 7.6)]
    lumps = [(cx + rng.uniform(-1, 1), cy + rng.uniform(-0.6, 0.6), r + rng.uniform(-0.6, 0.6)) for cx, cy, r in lumps]
    mask = lumpy(lumps, w, h, lambda x, y: 3 <= y < 29 and 2 <= x < 30)
    for (x, y) in mask:
        # Flat faces: the shade steps by facet, not smoothly.
        lit = ((x - 14) * 0.6 + (y - 14) * 1.0) / 14.0
        facet = ((x // 5) * 7 + (y // 4) * 3) % 5 * 0.05
        c = R_HI if lit + facet < -0.8 else R_LIGHT if lit + facet < -0.25 else R_MID if lit + facet < 0.35 else R_DARK
        put(px, x, y, c)
    # A crack.
    x, y = rng.randrange(12, 20), 13
    for _ in range(8):
        if (x, y) in mask:
            put(px, x, y, R_DARK)
            if (x + 1, y) in mask:
                put(px, x + 1, y, R_LIGHT)
        x += rng.choice((-1, 0, 1))
        y += 1
    # Speckles.
    for _ in range(14):
        x, y = rng.randrange(4, 28), rng.randrange(6, 26)
        if (x, y) in mask:
            put(px, x, y, R_DARK if rng.random() < 0.5 else R_LIGHT)
    if moss:
        top = {}
        for (x, y) in mask:
            top[x] = min(top.get(x, 99), y)
        for x, y0 in top.items():
            depth = 2 + int(3 * (1 + math.sin(x * 0.7))) + (2 if x < 16 else 0)
            for y in range(y0, y0 + depth):
                if (x, y) in mask:
                    put(px, x, y, G_LIGHT if y - y0 < 2 and x < 18 else G_BASE if y - y0 < depth - 1 else G_DARK)
    outline(px, mask, R_OUT)
    return img


def water(frame):
    rng = random.Random(77)
    img = Image.new('RGBA', (T, T), W_MID + (255,))
    px = img.load()
    for y in range(T):
        for x in range(T):
            if rng.random() < 0.12:
                put(px, x, y, W_DEEP)
                if x + 1 < T:
                    put(px, x + 1, y, W_DEEP)
    # Ripples: short light dashes that shift between the two frames.
    ripples = [(3, 5, 7), (18, 11, 6), (7, 21, 6), (22, 26, 7), (26, 3, 4), (12, 15, 3)]
    for x, y, n in ripples:
        x += frame * 3 if y % 2 else -frame * 2
        for k in range(n):
            put(px, (x + k) % T, y, W_LIGHT if 0 < k < n - 1 else W_MID)
            if 1 < k < n - 2:
                put(px, (x + k) % T, y + 1, W_DEEP)
        if frame == 0:
            put(px, (x + 2) % T, y, W_HI)
        else:
            put(px, (x + n - 3) % T, y, W_HI)
    return img


def bank(side, seed):
    """Land along one side of the water: grass, wet earth, then foam."""
    rng = random.Random(seed)
    img = blank(T, T)
    px = img.load()
    depth = [4] * T
    for i in range(1, T - 1):
        d = depth[i - 1] + rng.choice((-1, 0, 0, 1))
        depth[i] = max(2, min(6, d, 4 + (T - 1 - i)))
    depth[T - 1] = 4
    for i in range(T):
        d = depth[i]
        earth = [BANK_LIGHT, BANK_LIGHT, BANK, BANK] if side == 'n' else [BANK, BANK]  # the far bank shows its face
        layers = [G_BASE] * (d - 1) + [G_SHADE] + earth + [W_HI]
        if (i + seed) % 4 < 2:
            layers.append(W_LIGHT)
        for k, c in enumerate(layers):
            if side == 'n': x, y = i, k
            elif side == 's': x, y = i, T - 1 - k
            elif side == 'w': x, y = k, i
            else: x, y = T - 1 - k, i
            if c == W_HI and (i + seed) % 5 == 0:
                c = W_LIGHT
            put(px, x, y, c)
    return img


def bank_inner(corner):
    img = blank(T, T)
    px = img.load()
    for y in range(10):
        for x in range(10):
            d = x + y
            c = G_SHADE if d < 4 else BANK if d < 6 else W_HI if d < 7 else W_LIGHT if d < 8 else None
            if c is None:
                continue
            xx, yy = (T - 1 - x if 'e' in corner else x), (T - 1 - y if 's' in corner else y)
            put(px, xx, yy, c)
    return img


def corner_out(corner, rings, depth):
    """Rounds the corner where land is on two sides (ne, nw, se, sw): land
    outside a quarter circle, then the rings (outermost first) along it."""
    img = blank(T, T)
    px = img.load()
    c = 16.0
    radius = c - depth
    for y in range(16):
        for x in range(16):
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
            xx, yy = (T - 1 - x if 'e' in corner else x), (T - 1 - y if 's' in corner else y)
            put(px, xx, yy, col)
    return img


def bridge(vertical):
    img = blank(T, T)
    px = img.load()
    for a in range(T):      # along the walking direction
        for b in range(3, T - 3):  # across it
            plank = a % 6
            c = LOG_DARK if plank == 5 else LOG_HI if plank == 0 else LOG_MID
            if plank in (2, 3) and (a // 6 * 5 + b) % 9 == 0:
                c = LOG_DARK  # grain
            if plank != 5 and (b == 3 or b == T - 4):
                c = LOG_DARK  # plank ends
            x, y = (b, a) if vertical else (a, b)
            put(px, x, y, c)
    for a in range(T):  # rails
        for b, c in ((0, B_DARK), (1, B_LIGHT), (2, B_MID), (T - 3, B_MID), (T - 2, B_LIGHT), (T - 1, B_DARK)):
            x, y = (b, a) if vertical else (a, b)
            put(px, x, y, c)
    for a in (2, 18):  # posts
        for b in (0, 1, 2, T - 3, T - 2, T - 1):
            for k in range(4):
                x, y = (b, a + k) if vertical else (a + k, b)
                put(px, x, y, LOG_HI if k == 0 else B_LIGHT if k < 3 else B_OUT)
    return img


def bush(seed, berries=False):
    rng = random.Random(seed)
    w = h = T
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 17.2, 26.8, 14, 4.4)
    lumps = [(16, 19, 10), (9.6, 21, 7.2), (22.4, 21, 7.2), (13, 13, 6.8), (20, 13.6, 6.4)]
    lumps = [(cx + rng.uniform(-0.8, 0.8), cy + rng.uniform(-0.8, 0.8), r) for cx, cy, r in lumps]
    mask = lumpy(lumps, w, h, lambda x, y: y < 29)
    leaves = [(30, 66, 28), (44, 92, 30), (58, 116, 30), (78, 142, 32), (108, 170, 40)]
    leaf_clusters(px, rng, mask, tuple(leaves), 16, (5, 6, 27, 26), center=(16, 18), scale=12)
    if berries:
        placed = 0
        for _ in range(60):
            if placed >= 9:
                break
            x, y = rng.randrange(6, 26), rng.randrange(10, 26)
            cells = [(x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)]
            if all(c in mask for c in cells) and all(px[c][:3] != (196, 40, 52) for c in cells):
                put(px, x, y, (236, 96, 100))
                put(px, x + 1, y, (196, 40, 52))
                put(px, x, y + 1, (196, 40, 52))
                put(px, x + 1, y + 1, (120, 20, 36))
                placed += 1
    return img


def flowers(seed):
    rng = random.Random(seed)
    img = blank(T, T)
    px = img.load()
    colors = [(226, 224, 26), (240, 240, 240), (224, 32, 36), (240, 144, 200), (150, 120, 230)]
    pick = rng.sample(colors, 2)
    spots = []
    while len(spots) < 7:
        x, y = rng.randrange(4, 27), rng.randrange(4, 25)
        if all(abs(x - a) + abs(y - b) > 7 for a, b in spots):
            spots.append((x, y))
    for i, (x, y) in enumerate(spots):
        petal = pick[i % 2]
        for k in range(2, 6):  # stem with a leaf
            put(px, x, y + k, G_DARK)
        put(px, x + 1, y + 4, G_LIGHT)
        put(px, x + 2, y + 3, G_LIGHT)
        put(px, x + 1, y + 6, (0, 0, 0, 40))
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-2, 0), (2, 0), (0, -2), (0, 2), (-1, -1), (1, -1), (-1, 1), (1, 1)):
            if abs(dx) + abs(dy) == 2 and dx and dy:
                continue
            put(px, x + dx, y + dy, petal)
        shade = tuple(max(0, v - 50) for v in petal)
        put(px, x + 1, y + 1, shade)
        put(px, x, y + 2, shade)
        put(px, x, y, (250, 200, 60) if petal != colors[0] else (200, 110, 30))
    for _ in range(8):  # tufts
        x, y = rng.randrange(1, T - 1), rng.randrange(3, T - 1)
        if all(px[x, y - k][3] == 0 for k in range(3)):
            put(px, x, y, G_DARK)
            put(px, x, y - 1, G_DARK)
            put(px, x, y - 2, G_LIGHT)
    return img


def house():
    W, H = 136, 144
    img = blank(W, H)
    px = img.load()
    # Shadow on the ground to the right and below.
    for y in range(132, 144):
        for x in range(8, 136):
            px[x, y] = SHADOW
    # Walls of round logs, sprite y 80..137 (the house tiles start at y 48).
    for y in range(80, 138):
        row = (y - 80) % 8
        c = LOG_HI if row in (1, 2) else LOG_DARK if row == 7 else (126, 86, 52) if row == 6 else LOG_MID
        for x in range(8, 128):
            px[x, y] = c + (255,)
        if row in (3, 4):
            for x in range(8, 128):
                if (x * 7 + y * 3) % 23 == 0:
                    px[x, y] = LOG_DARK + (255,)  # grain
                    px[x + 1, y] = LOG_DARK + (255,)
    # Corner posts: the ends of the logs, round.
    for y in range(80, 138):
        row = (y - 80) % 8
        for x0 in (3, 125):
            for x in range(x0, x0 + 8):
                d = math.hypot(x - x0 - 3.5, row - 3.5)
                c = (210, 168, 112) if d < 1.6 else (186, 140, 88) if d < 2.8 else LOG_MID if d < 3.6 else LOG_DARK
                px[x, y] = c + (255,)
    # Stone base.
    for y in range(134, 140):
        for x in range(3, 133):
            c = STONE if ((x + (y // 3) * 4) // 7 + y) % 3 else (130, 126, 120)
            if (x + (y // 3) * 4) % 7 == 0 or y % 3 == 2:
                c = STONE_DARK
            px[x, y] = c + (255,)
    # Roof: shingles from the ridge (y 20) to the eave (y 85), with overhang.
    for y in range(20, 86):
        r = (y - 20) // 6
        for x in range(0, 136):
            xx = x + (r % 2) * 5
            k = (y - 20) % 6
            c = ROOF_MID
            if k == 5:
                c = ROOF_DARK
            elif xx % 10 == 0:
                c = ROOF_DARK
            elif k == 0:
                c = ROOF_LIGHT
            elif xx % 10 == 1:
                c = ROOF_LIGHT
            # Light falls on the upper part of the roof.
            if y < 40 and c == ROOF_MID and (x + y) % 2:
                c = ROOF_LIGHT
            px[x, y] = c + (255,)
    for x in range(0, 136):  # ridge cap and eave
        for y, c in ((20, ROOF_OUT), (21, ROOF_OUT), (22, ROOF_HI), (23, ROOF_HI), (24, ROOF_LIGHT),
                     (82, ROOF_DARK), (83, ROOF_OUT), (84, ROOF_OUT), (85, ROOF_OUT)):
            px[x, y] = c + (255,)
    for y in range(20, 86):
        for x in (0, 1, 134, 135):
            px[x, y] = ROOF_OUT + (255,)
    for x in range(8, 128):  # shade under the eave
        for y in (86, 87, 88):
            px[x, y] = (LOG_DARK if y < 88 or x % 2 else (126, 86, 52)) + (255,)
    # Chimney.
    for y in range(0, 44):
        for x in range(96, 116):
            brick = ((x - 96) // 6 + (y // 5)) % 2
            c = STONE if brick else (136, 132, 126)
            if y % 5 == 4 or (x - 96 + (y // 5) * 3) % 6 == 0:
                c = STONE_DARK
            if x in (96, 97):
                c = (176, 172, 166)
            if x in (114, 115):
                c = (70, 66, 64)
            px[x, y] = c + (255,)
    for x in range(94, 118):
        for y, c in ((0, (70, 66, 64)), (1, (160, 156, 150)), (2, (110, 106, 100)), (3, (60, 56, 54))):
            px[x, y] = c + (255,)
    # Door: in the second tile column of the bottom row (sprite x 36..68).
    for y in range(100, 138):
        for x in range(40, 64):
            edge = x in (40, 41, 62, 63) or y in (100, 101)
            plank = (x - 42) % 5
            c = LOG_DARK if edge else (B_DARK if plank == 4 else B_LIGHT if plank == 0 else B_MID)
            if not edge and y in (108, 109, 128, 129):
                c = (54, 36, 24)  # cross braces
            px[x, y] = c + (255,)
    for (x, y), c in (((58, 119), (250, 214, 100)), ((59, 119), (230, 190, 70)), ((58, 120), (230, 190, 70)),
                      ((59, 120), (150, 110, 40)), ((58, 121), (120, 84, 30))):
        px[x, y] = c + (255,)  # knob
    for x in range(38, 66):  # step
        for y, c in ((138, (176, 172, 166)), (139, STONE), (140, STONE), (141, STONE_DARK)):
            px[x, y] = c + (255,)
    # A lamp beside the door.
    for y in range(104, 112):
        for x in range(68, 73):
            c = (60, 50, 40) if x in (68, 72) or y in (104, 111) else (255, 220, 120) if y < 108 else (240, 170, 70)
            px[x, y] = c + (255,)
    # Windows with a warm light.
    def window(x0, y0):
        for y in range(y0, y0 + 20):
            for x in range(x0, x0 + 22):
                frame = x in (x0, x0 + 1, x0 + 20, x0 + 21) or y in (y0, y0 + 1, y0 + 18, y0 + 19) \
                    or x in (x0 + 10, x0 + 11) or y in (y0 + 9, y0 + 10)
                c = B_DARK if frame else (GLASS_HI if (x - x0) % 11 + (y - y0) % 10 < 9 else GLASS)
                if not frame and (x - x0) % 11 - (y - y0) % 10 in (4, 5) and (y - y0) % 10 < 6:
                    c = (255, 246, 210)  # a glint
                px[x, y] = c + (255,)
        for x in range(x0 - 2, x0 + 24):  # sill
            px[x, y0 + 20] = LOG_HI + (255,)
            px[x, y0 + 21] = LOG_MID + (255,)
            px[x, y0 + 22] = LOG_DARK + (255,)
        for x in range(x0, x0 + 22):  # flower box
            for y in range(y0 + 15, y0 + 20):
                if y >= y0 + 17:
                    c = (120, 82, 50) if y < y0 + 19 else LOG_DARK
                else:
                    c = None
                    k = (x - x0) % 5
                    if y == y0 + 15 and k in (1, 2):
                        c = (214, 58, 62) if (x - x0) // 5 % 2 else (240, 220, 80)
                    elif y == y0 + 16:
                        c = G_BASE if k != 4 else G_DARK
                if c:
                    px[x, y] = c + (255,)
    window(12, 102)
    window(88, 102)
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
        if 'n' in corner:
            rings = [G_SHADE, BANK_LIGHT, BANK_LIGHT, BANK, BANK, W_HI]
        else:
            rings = [G_SHADE, BANK, BANK, W_HI]
        save(corner_out(corner, rings, 5), 'bank_out_' + corner)
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
