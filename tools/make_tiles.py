"""Draws the trees, the stump and the dirt path in assets/tiles/.

Every map tile is 32x32 art pixels. Same palette family as the grass
(tools/make_grass.py). Light comes from the top left. Run:
    python3 tools/make_tiles.py

  tree_N.png      32x48: an oak on one tile; the top 16 rows hang over the
                  tile above, so rows of trees read as a forest
  stump.png       32x32: a chopped tree
  path_N.png      32x32: dirt
  path_edge_X_N.png 32x32, transparent: grass creeping over the dirt along
                  one side (n, s, w, e), drawn where the next tile is not
                  path; three variants per side so the edge does not repeat
  path_edge_out_XY.png 32x32, transparent: rounds the outside of a turn
  path_edge_in_XY.png 32x32, transparent: a little grass in one corner (ne, nw,
                  se, sw), drawn where the path turns
"""
import math
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'tiles')
T = 32  # one map tile

# Grass (as in make_grass.py) for the path edges.
G_BASE, G_SHADE, G_LIGHT, G_DARK = (92, 148, 21), (86, 140, 21), (111, 175, 24), (82, 119, 35)
# Leaves, from deep shadow to highlight.
L_OUT = (30, 66, 28)
L_DARK = (44, 92, 30)
L_MID = (58, 116, 30)
L_LIGHT = (78, 142, 32)
L_HI = (108, 170, 40)
# Bark.
B_OUT, B_DARK, B_MID, B_LIGHT = (52, 34, 24), (74, 48, 30), (106, 70, 42), (138, 96, 58)
SHADOW = (0, 0, 0, 70)
# Dirt.
D_BASE, D_DARK, D_LIGHT = (172, 136, 88), (146, 112, 70), (190, 156, 106)
D_STONE, D_STONE_DARK = (206, 186, 146), (122, 96, 62)


def blank(w, h):
    return Image.new('RGBA', (w, h), (0, 0, 0, 0))


def ellipse_shadow(px, w, h, cx, cy, rx, ry):
    for y in range(h):
        for x in range(w):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1:
                px[x, y] = SHADOW


def outline(px, mask, color):
    for (x, y) in list(mask):
        if any((x + dx, y + dy) not in mask for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            px[x, y] = color + (255,)


def lumpy(lumps, w, h, keep=lambda x, y: True):
    return {(x, y) for y in range(h) for x in range(w)
            if keep(x, y) and any((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r for cx, cy, r in lumps)}


def leaf_clusters(px, rng, mask, colors, count, area, light_dir=(0.7, 1.0), center=(16, 16), scale=18.0):
    """Shades a canopy as many small round clusters: each one is lit on its
    top-left and shaded on its bottom-right, so the crown reads as leaves and
    not as one flat blob. colors: out, dark, mid, light, hi."""
    out, dark, mid, light, hi = colors
    # Base shading by where the pixel sits in the whole crown.
    for (x, y) in mask:
        lit = ((x - center[0]) * light_dir[0] + (y - center[1]) * light_dir[1]) / scale
        lit += ((x + y) & 1) * 0.06
        c = light if lit < -0.45 else mid if lit < 0.25 else dark
        px[x, y] = c + (255,)
    # Clusters.
    x0, y0, x1, y1 = area
    for _ in range(count):
        cx, cy = rng.uniform(x0, x1), rng.uniform(y0, y1)
        r = rng.uniform(2.6, 4.2)
        lit = ((cx - center[0]) * light_dir[0] + (cy - center[1]) * light_dir[1]) / scale
        top = hi if lit < -0.5 else light if lit < 0.3 else mid
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                if (x, y) not in mask:
                    continue
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy)
                if d > r:
                    continue
                if d > r - 1.2 and dx + dy > 0.5:
                    px[x, y] = dark + (255,)  # the shaded rim of the cluster
                elif dx + dy < -r * 0.6:
                    px[x, y] = top + (255,)
                elif dx + dy < 0:
                    px[x, y] = (light if top == hi else mid) + (255,)
    outline(px, mask, out)


def tree(seed):
    rng = random.Random(seed)
    w, h = T, 48
    img = blank(w, h)
    px = img.load()
    ellipse_shadow(px, w, h, 17.5, 42.5, 15, 5)
    # Trunk with roots spreading at the foot.
    trunk = set()
    for y in range(26, 44):
        spread = max(0, y - 39)
        for x in range(12 - spread, 20 + spread):
            trunk.add((x, y))
    for (x, y) in trunk:
        c = B_LIGHT if x <= 13 else B_MID if x < 18 else B_DARK
        if (x * 5 + y * 3) % 11 == 0 and 13 < x < 18:
            c = B_DARK  # bark lines
        px[x, y] = c + (255,)
    outline(px, trunk, B_OUT)
    # Canopy: a union of lumps, a little different on each tree.
    lumps = [(16, 19, 13), (9.2, 14.4, 8.4), (22.8, 14.4, 8.4), (16, 9.2, 8.8),
             (8.4 + rng.uniform(-1.2, 1.2), 22, 7.2), (23.6 + rng.uniform(-1.2, 1.2), 22, 7.2)]
    lumps = [(cx + rng.uniform(-0.8, 0.8), cy + rng.uniform(-0.8, 0.8), r) for cx, cy, r in lumps]
    mask = lumpy(lumps, w, h, lambda x, y: y < 33)
    leaf_clusters(px, rng, mask, (L_OUT, L_DARK, L_MID, L_LIGHT, L_HI), 26, (4, 2, 28, 30),
                  center=(16, 17), scale=17)
    # A few gaps where the shade inside shows.
    for _ in range(3):
        x, y = rng.randrange(8, 24), rng.randrange(18, 28)
        for dx, dy in ((0, 0), (1, 0), (0, 1)):
            if (x + dx, y + dy) in mask:
                px[x + dx, y + dy] = L_OUT + (255,)
    return img


def stump():
    img = blank(T, T)
    px = img.load()
    ellipse_shadow(px, T, T, 17, 26, 13, 4.4)
    body = set()
    for y in range(15, 28):
        spread = max(0, y - 24)
        for x in range(8 - spread, 24 + spread):
            body.add((x, y))
    for (x, y) in body:
        c = B_LIGHT if x < 12 else B_MID if x < 20 else B_DARK
        if (x * 3 + y) % 7 == 0 and 11 < x < 20:
            c = B_DARK
        px[x, y] = c + (255,)
    # The cut top: rings of light wood.
    top = set()
    for y in range(11, 20):
        for x in range(7, 25):
            e = ((x + 0.5 - 16) / 8.5) ** 2 + ((y + 0.5 - 15.5) / 4) ** 2
            if e <= 1:
                top.add((x, y))
                c = (214, 176, 118) if e > 0.75 or 0.3 < e < 0.45 else (196, 154, 100)
                if e < 0.06:
                    c = (160, 118, 72)
                px[x, y] = c + (255,)
    outline(px, body | top, B_OUT)
    # A crack in the wood.
    for x, y in ((18, 14), (19, 15), (20, 15)):
        px[x, y] = (150, 108, 64, 255)
    # Chips on the ground.
    for x, y in ((2, 28), (3, 28), (27, 24), (28, 25), (28, 30), (29, 30)):
        px[x, y] = (210, 172, 112, 255)
    return img


def dirt(seed):
    rng = random.Random(seed)
    img = Image.new('RGBA', (T, T), D_BASE + (255,))
    px = img.load()
    for _ in range(46):
        x, y = rng.randrange(T), rng.randrange(T)
        c = rng.choice((D_DARK, D_LIGHT))
        px[x, y] = c + (255,)
        if rng.random() < 0.4:
            px[(x + 1) % T, y] = c + (255,)
    for _ in range(rng.randrange(2, 4)):  # a pebble with its shadow
        x, y = rng.randrange(2, T - 4), rng.randrange(2, T - 4)
        for dx, dy in ((1, 0), (0, 1), (1, 1), (2, 1)):
            px[x + dx, y + dy] = D_STONE + (255,)
        px[x + 1, y] = (226, 210, 176, 255)
        for dx, dy in ((0, 2), (1, 2), (2, 2), (3, 1)):
            px[x + dx, y + dy] = D_STONE_DARK + (255,)
    return img


def edge(side, seed):
    """Grass over the dirt along one side, 2 to 7 pixels deep, ragged."""
    rng = random.Random(seed)
    img = blank(T, T)
    px = img.load()
    # Both ends are 4 deep so any variant joins any other; in between the
    # line wanders between 2 and 7.
    depth = [4] * T
    for i in range(1, T - 1):
        d = depth[i - 1] + rng.choice((-1, 0, 0, 0, 1))
        d = max(2, min(7, d))
        depth[i] = min(d, 4 + (T - 1 - i))  # come back to 4 by the end
    depth[T - 1] = 4

    def at(i, k):
        if side == 'n': return i, k
        if side == 's': return i, T - 1 - k
        if side == 'w': return k, i
        return T - 1 - k, i

    for i in range(T):
        d = depth[i]
        for k in range(d + 2):
            x, y = at(i, k)
            if k < d:
                c = G_BASE if k < d - 1 else G_SHADE
                px[x, y] = c + (255,)
            elif k == d:
                px[x, y] = D_DARK + (255,)  # the dirt darkens along the grass
            elif i % 3:
                px[x, y] = (160, 124, 78, 255)
    # Blades poking over the edge.
    for _ in range(6):
        i = rng.randrange(2, T - 2)
        k = depth[i]
        for j, c in ((0, G_DARK), (1, G_LIGHT)):
            x, y = at(i, k + j)
            px[x, y] = c + (255,)
    return img


def inner(corner):
    """A little grass in one corner, for where the path turns."""
    img = blank(T, T)
    px = img.load()
    for y in range(8):
        for x in range(8):
            d = x + y
            if d > 6:
                continue
            c = G_BASE if d < 4 else G_SHADE if d < 6 else D_DARK
            xx, yy = (T - 1 - x if 'e' in corner else x), (T - 1 - y if 's' in corner else y)
            px[xx, yy] = c + (255,)
    return img


def outer(corner):
    """Rounds the corner where the path turns: grass outside a quarter circle."""
    img = blank(T, T)
    px = img.load()
    c, radius = 16.0, 11.0
    for y in range(16):
        for x in range(16):
            s = math.hypot(c - x - 0.5, c - y - 0.5) - radius
            if s <= -0.5:
                continue
            col = G_BASE if s > 1.5 else G_SHADE if s > 0.5 else D_DARK
            xx, yy = (T - 1 - x if 'e' in corner else x), (T - 1 - y if 's' in corner else y)
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
