"""Draws the trees, the stump and the dirt path in assets/tiles/.

Same palette family as the grass (tools/make_grass.py). Light comes from the
top left. Run:  python3 tools/make_tiles.py

  tree_N.png      16x24: a tree on one tile; the top 8 rows hang over the
                  tile above, so rows of trees read as a forest
  stump.png       16x16: a chopped tree
  path_N.png      16x16: dirt
  path_edge_X_N.png 16x16, transparent: grass creeping over the dirt along
                  one side (n, s, w, e), drawn where the next tile is not
                  path; three variants per side so the edge does not repeat
  path_edge_out_XY.png 16x16, transparent: rounds the outside of a turn
  path_edge_in_XY.png 16x16, transparent: a little grass in one corner (ne, nw,
                  se, sw), drawn where the path turns
"""
import math
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'tiles')

# Grass (as in make_grass.py) for the path edges.
G_BASE, G_SHADE, G_LIGHT, G_DARK = (92, 148, 21), (86, 140, 21), (111, 175, 24), (82, 119, 35)
# Leaves, from deep shadow to highlight.
L_OUT = (30, 66, 28)
L_DARK = (44, 92, 30)
L_MID = (58, 116, 30)
L_LIGHT = (78, 142, 32)
L_HI = (108, 170, 40)
# Bark.
B_DARK, B_MID, B_LIGHT = (74, 48, 30), (106, 70, 42), (138, 96, 58)
SHADOW = (0, 0, 0, 70)
# Dirt.
D_BASE, D_DARK, D_LIGHT = (172, 136, 88), (146, 112, 70), (190, 156, 106)
D_STONE, D_STONE_DARK = (206, 186, 146), (122, 96, 62)


def blank(w, h):
    return Image.new('RGBA', (w, h), (0, 0, 0, 0))


def tree(seed):
    rng = random.Random(seed)
    w, h = 16, 24
    img = blank(w, h)
    px = img.load()
    # Shadow on the ground, under the canopy, slightly to the right.
    for y in range(19, 24):
        for x in range(w):
            if ((x - 9) / 7.5) ** 2 + ((y - 21.5) / 2.6) ** 2 <= 1:
                px[x, y] = SHADOW
    # Trunk with roots.
    for y in range(14, 22):
        for x in range(6, 10):
            px[x, y] = (B_LIGHT if x == 6 else B_MID if x < 9 else B_DARK) + (255,)
    for x, y in ((5, 21), (10, 21), (5, 20)):
        px[x, y] = B_DARK + (255,)
    px[8, 17] = B_DARK + (255,)
    # Canopy: a union of lumps, a little different on each tree.
    lumps = [(8, 9.5, 6.6), (4.6, 7.2, 4.2), (11.4, 7.2, 4.2), (8, 4.6, 4.4),
             (4.2 + rng.uniform(-0.6, 0.6), 11, 3.6), (11.8 + rng.uniform(-0.6, 0.6), 11, 3.6)]
    lumps = [(cx + rng.uniform(-0.4, 0.4), cy + rng.uniform(-0.4, 0.4), r) for cx, cy, r in lumps]

    def inside(x, y):
        return any((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r for cx, cy, r in lumps)

    mask = {(x, y) for y in range(h) for x in range(w) if inside(x, y) and y < 17}
    for (x, y) in mask:
        # Distance toward the light (top left) picks the shade, with a
        # checker dither between bands.
        lit = ((x - 8) * 0.7 + (y - 8) * 1.0) / 9.0
        lit += rng.uniform(-0.12, 0.12)
        dither = ((x + y) & 1) * 0.08
        v = lit + dither
        c = L_HI if v < -0.75 else L_LIGHT if v < -0.2 else L_MID if v < 0.35 else L_DARK
        px[x, y] = c + (255,)
    # Leaf clusters: small bright arcs on the lit side, dark notches below.
    for _ in range(5):
        x, y = rng.randrange(3, 11), rng.randrange(2, 10)
        if (x, y) in mask and (x + 1, y) in mask:
            px[x, y] = L_HI + (255,)
            px[x + 1, y] = L_LIGHT + (255,)
            if (x, y + 1) in mask:
                px[x, y + 1] = L_MID + (255,)
    for _ in range(4):
        x, y = rng.randrange(5, 14), rng.randrange(8, 15)
        if (x, y) in mask:
            px[x, y] = L_DARK + (255,)
    # Outline: leaf pixels next to an empty pixel get the darkest green.
    for (x, y) in list(mask):
        if any((x + dx, y + dy) not in mask for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            px[x, y] = L_OUT + (255,)
    return img


def stump():
    img = blank(16, 16)
    px = img.load()
    for y in range(10, 16):
        for x in range(16):
            if ((x - 8.5) / 6.5) ** 2 + ((y - 13) / 2.2) ** 2 <= 1:
                px[x, y] = SHADOW
    for y in range(8, 14):
        for x in range(4, 12):
            px[x, y] = (B_LIGHT if x < 6 else B_MID if x < 10 else B_DARK) + (255,)
    for x in (3, 12):
        px[x, 13] = B_DARK + (255,)
    # The cut top: light rings.
    for y in range(6, 10):
        for x in range(4, 12):
            if ((x - 7.5) / 4) ** 2 + ((y - 7.5) / 1.8) ** 2 <= 1:
                px[x, y] = (214, 176, 118, 255)
    for x, y in ((6, 7), (7, 7), (8, 8), (9, 7)):
        px[x, y] = (184, 142, 88, 255)
    px[7, 8] = B_DARK + (255,)
    # Chips on the ground.
    for x, y in ((1, 14), (13, 12), (14, 15)):
        px[x, y] = (200, 162, 104, 255)
    return img


def dirt(seed):
    rng = random.Random(seed)
    img = Image.new('RGBA', (16, 16), D_BASE + (255,))
    px = img.load()
    for _ in range(14):
        x, y = rng.randrange(16), rng.randrange(16)
        px[x, y] = rng.choice((D_DARK, D_LIGHT)) + (255,)
    for _ in range(rng.randrange(1, 3)):  # a pebble with its shadow
        x, y = rng.randrange(1, 14), rng.randrange(1, 14)
        px[x, y] = D_STONE + (255,)
        px[x + 1, y] = D_STONE + (255,)
        px[x, y + 1] = D_STONE_DARK + (255,)
        px[x + 1, y + 1] = D_STONE_DARK + (255,)
    return img


def edge(side, seed):
    """Grass over the dirt along one side, 1 to 4 pixels deep, ragged."""
    rng = random.Random(seed)
    img = blank(16, 16)
    px = img.load()
    # Both ends are 2 deep so any variant joins any other; in between the
    # line wanders between 1 and 4.
    depth = [2] * 16
    for i in range(1, 15):
        d = depth[i - 1] + rng.choice((-1, 0, 0, 0, 1))
        d = max(1, min(4, d))
        depth[i] = min(d, 2 + (15 - i))  # come back to 2 by the end
    depth[15] = 2
    for i in range(16):
        d = depth[i]
        for k in range(d + 1):
            if side == 'n': x, y = i, k
            elif side == 's': x, y = i, 15 - k
            elif side == 'w': x, y = k, i
            else: x, y = 15 - k, i
            if k < d:
                c = G_BASE if k < d - 1 else G_SHADE
                px[x, y] = c + (255,)
            else:
                px[x, y] = D_DARK + (255,)  # the dirt darkens along the grass
    # A few blades poking over the edge.
    for _ in range(3):
        i = rng.randrange(1, 15)
        k = depth[i]
        if side == 'n': cells = [(i, k)]
        elif side == 's': cells = [(i, 15 - k)]
        elif side == 'w': cells = [(k, i)]
        else: cells = [(15 - k, i)]
        for x, y in cells:
            px[x, y] = G_DARK + (255,)
    return img


def inner(corner):
    """A little grass in one corner, for where the path turns."""
    img = blank(16, 16)
    px = img.load()
    shape = [(0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (0, 2)]
    for x, y in shape:
        if 'e' in corner: x = 15 - x
        if 's' in corner: y = 15 - y
        px[x, y] = G_BASE + (255,)
    for x, y in ((3, 0), (2, 1), (1, 2), (0, 3)):
        if 'e' in corner: x = 15 - x
        if 's' in corner: y = 15 - y
        px[x, y] = D_DARK + (255,)
    return img


def outer(corner):
    """Rounds the corner where the path turns: grass outside a quarter circle."""
    img = blank(16, 16)
    px = img.load()
    c, radius = 8.0, 5.5
    for y in range(8):
        for x in range(8):
            s = math.hypot(c - x - 0.5, c - y - 0.5) - radius
            if s <= -0.5:
                continue
            col = G_BASE if s > 1.5 else G_SHADE if s > 0.5 else D_DARK
            xx, yy = (15 - x if 'e' in corner else x), (15 - y if 's' in corner else y)
            px[xx, yy] = col + (255,)
    return img


def save(img, name):
    img.save(os.path.join(OUT, name + '.png'), optimize=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    for i, seed in enumerate((3, 8, 21), 1):
        save(tree(seed), 'tree_%d' % i)
    save(stump(), 'stump')
    for i, seed in enumerate((5, 9, 14), 1):
        save(dirt(seed), 'path_%d' % i)
    for name in os.listdir(OUT):
        if name.startswith('path_edge_'):
            os.remove(os.path.join(OUT, name))
    for side, seed in (('n', 10), ('s', 20), ('w', 30), ('e', 40)):
        for v in range(1, 4):
            save(edge(side, seed + v), 'path_edge_%s_%d' % (side, v))
    for corner in ('ne', 'nw', 'se', 'sw'):
        save(inner(corner), 'path_edge_in_' + corner)
        save(outer(corner), 'path_edge_out_' + corner)
    print('wrote trees, stump and path to', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
