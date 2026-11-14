"""Draws the monster sprites in assets/monsters/, two frames each, at 32 art
pixels per map tile. The game draws them at their own size on the map and
twice as big in battle, facing right; it flips them to face left. Run:
    python3 tools/make_monsters.py

  slime_1/2.png   32x32
  beetle_1/2.png  32x32
  wolf_1/2.png    48x32
  stump_1/2.png   48x48: the Elder Stump, the boss

(The fire creatures, ember_1/2.png, come from tools/make_characters.py.)
"""
import math

from make_characters import Canvas, save

# --- Slime --------------------------------------------------------------------------

S_OUT, S_DARK, S_MID, S_LIGHT, S_HI = (26, 70, 44), (46, 128, 70), (76, 176, 90), (130, 214, 118), (236, 255, 232)


def slime(frame):
    c = Canvas(32, 32)
    # Frame 2 is squashed: lower and wider.
    rx, top = (11.5, 9) if frame == 0 else (13.5, 12)
    base = 30
    cy = (top + base) / 2 + 2
    ry = (base - top) / 2 + 1

    def shade(x, y):
        dx, dy = (x + 0.5 - 16) / rx, (y + 0.5 - cy) / ry
        lit = dx * 0.6 + dy
        if y >= base - 2:
            return S_DARK
        return S_LIGHT if lit < -0.55 else S_MID if lit < 0.45 else S_DARK

    for y in range(top, base + 1):
        for x in range(32):
            dx, dy = (x + 0.5 - 16) / rx, (y + 0.5 - cy) / ry
            # A drop: round on top, flat on the ground.
            if dx * dx + (dy if dy < 0 else dy * 0.35) ** 2 <= 1:
                c.set(x, y, shade(x, y))
    # Shine and a bubble inside.
    hx, hy = 10 if frame == 0 else 9, top + 4
    for dx, dy in ((0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (0, 2)):
        c.set(hx + dx, hy + dy, S_HI)
    c.set(21, base - 6, S_LIGHT)
    c.set(22, base - 7, S_LIGHT)
    # Eyes and a small mouth.
    ey = top + 9 if frame == 0 else top + 7
    for ex in (12, 19):
        c.rect(ex, ey, ex + 2, ey + 4, (22, 28, 30))
        c.set(ex, ey, S_HI)
    c.rect(15, ey + 5, 18, ey + 6, (22, 60, 34))
    return c.outline(S_OUT)


# --- Beetle -------------------------------------------------------------------------

K_OUT, K_LEG = (24, 18, 36), (40, 32, 56)
K_DARK, K_MID, K_LIGHT, K_HI = (66, 44, 120), (104, 74, 176), (150, 124, 222), (220, 210, 255)
K_EYE = (255, 214, 90)


def beetle(frame):
    c = Canvas(32, 32)
    # Six legs, alternating between the frames.
    for i, y in enumerate((15, 20, 25)):
        up = (i + frame) % 2
        for side in (-1, 1):
            x0 = 16 + side * 10
            for k in range(4):
                c.set(x0 + side * k, y + k // 2 - up, K_LEG)
            c.set(x0 + side * 4, y + 2 - up, K_LEG)
    # Horns.
    for k in range(5):
        c.set(11 - k // 2, 4 - k + (frame and k > 2), K_OUT)
        c.set(20 + k // 2, 4 - k + (frame and k > 2), K_OUT)
    # Head.
    c.ellipse(16, 8, 6, 4.2, K_LEG)
    c.rect(12, 7, 14, 9, K_EYE)
    c.rect(18, 7, 20, 9, K_EYE)
    # Shell: two wing cases with a seam down the middle.
    def shell(x, y):
        dx, dy = x + 0.5 - 16, y + 0.5 - 20
        side = dx < 0
        lx = dx + (6 if side else -6)
        lit = lx * 0.08 + dy * 0.1
        return K_HI if lit < -0.75 else K_LIGHT if lit < -0.25 else K_MID if lit < 0.5 else K_DARK
    c.ellipse(16, 20, 10.5, 9.5, shell)
    c.rect(15, 11, 17, 30, K_OUT)
    c.rect(7, 12, 25, 13, K_OUT)  # the line between head plate and wings
    for x, y in ((10, 16), (11, 16), (10, 17), (20, 16), (21, 16), (20, 17)):
        c.set(x, y, K_HI)
    return c.outline(K_OUT)


# --- Wolf ---------------------------------------------------------------------------

F_OUT, F_DARK, F_MID, F_LIGHT, F_BELLY = (34, 34, 46), (78, 82, 98), (122, 126, 142), (168, 172, 184), (226, 228, 236)
F_EYE, F_NOSE = (255, 196, 64), (18, 18, 22)


def wolf(frame):
    c = Canvas(48, 32)
    # Legs: far pair darker, near pair lighter; they swap between frames.
    if frame == 0:
        legs = [(13, 21, -2, F_DARK), (30, 21, 2, F_DARK), (16, 21, 2, F_MID), (33, 21, -2, F_MID)]
    else:
        legs = [(12, 21, 3, F_DARK), (31, 21, -3, F_DARK), (17, 21, -1, F_MID), (34, 21, 1, F_MID)]
    for x, y, lean, col in legs:
        for k in range(10):
            dx = int(lean * k / 9)
            c.rect(x + dx, y + k, x + dx + 3, y + k + 1, col)
        c.rect(x + lean, y + 9, x + lean + 4, y + 10, F_OUT)  # paw
    # Tail: bushy, raised behind.
    c.poly([(11, 13), (3, 6 + frame), (1, 10 + frame), (4, 15), (10, 18)], lambda x, y: F_LIGHT if y < 10 + frame else F_MID)
    c.set(2, 9 + frame, F_BELLY)
    # Body.
    def body(x, y):
        if y > 19:
            return F_BELLY if 16 < x < 32 else F_LIGHT
        return F_DARK if y < 12 else F_MID
    c.ellipse(23, 16, 14, 6.5, body)
    # Ruff of fur at the neck.
    c.poly([(30, 9), (38, 6), (40, 16), (36, 23), (30, 21)], lambda x, y: F_LIGHT if x > 34 else F_MID)
    for y in range(12, 22, 3):
        c.set(32, y, F_DARK)
        c.set(35, y + 1, F_DARK)
    # Head with a long snout.
    c.ellipse(38, 9, 5.5, 5, lambda x, y: F_LIGHT if y > 10 else F_MID)
    c.poly([(40, 7), (47, 10), (47, 13), (40, 14)], lambda x, y: F_BELLY if y > 11 else F_LIGHT)
    c.rect(45, 9, 47, 11, F_NOSE)
    c.rect(41, 13, 46, 14, F_OUT)  # mouth line
    # Ears: two tall points.
    c.poly([(34, 6), (36, 0), (38, 5)], F_DARK)
    c.poly([(37, 5), (40, 0), (41, 6)], F_MID)
    c.set(39, 3, (210, 160, 160))
    # Eye.
    c.rect(40, 7, 42, 9, F_EYE)
    c.set(41, 8, F_NOSE)
    # Back stripe.
    for x in range(14, 32):
        if (x * 3) % 5:
            c.set(x, 10 + int(math.sin(x * 0.4)), F_DARK)
    return c.outline(F_OUT)


# --- Elder Stump -------------------------------------------------------------------

E_OUT, E_DARK, E_MID, E_LIGHT = (40, 26, 20), (74, 48, 30), (106, 70, 42), (138, 96, 58)
E_RING, E_RING_DARK, E_CORE = (214, 176, 118), (184, 142, 88), (150, 108, 64)
E_MOSS, E_MOSS_LIGHT, E_MOSS_DARK = (78, 142, 32), (108, 170, 40), (44, 92, 30)
E_GLOW, E_GLOW_HI = (255, 196, 64), (255, 246, 190)


def elder_stump(frame):
    c = Canvas(48, 48)
    sway = (0, 1)[frame]
    # Roots spreading over the ground.
    for pts in ([(10, 38), (2, 46), (6, 47), (14, 42)], [(38, 38), (46, 46), (42, 47), (34, 42)],
                [(18, 42), (14, 47), (20, 47), (22, 42)], [(28, 42), (30, 47), (35, 47), (31, 41)]):
        c.poly(pts, E_DARK)
    # The trunk: wide, a little narrower up top, with bark ridges.
    def bark(x, y):
        k = (x + (y // 7) % 2) % 6
        if x < 15:
            return E_LIGHT if k else E_MID
        if x > 32:
            return E_DARK if k else E_OUT
        return E_DARK if k == 0 else E_MID
    c.poly([(11, 16), (37, 16), (40, 44), (8, 44)], bark)
    # Arms: two dead branches, raised, with a few leaves.
    for side in (-1, 1):
        x0 = 24 + side * 13
        tip = (24 + side * 22, 6 + (sway if side < 0 else -sway))
        c.poly([(x0, 26), (x0 + side * 2, 21), tip, (tip[0] + side * 1, tip[1] + 3), (x0 + side * 3, 28)], E_MID)
        c.ellipse(tip[0], tip[1] - 1, 3.4, 2.6, lambda x, y: E_MOSS_LIGHT if y < tip[1] - 1 else E_MOSS)
    # The cut top with growth rings, under a cap of moss.
    for y in range(10, 22):
        for x in range(10, 39):
            e = ((x + 0.5 - 24) / 14) ** 2 + ((y + 0.5 - 16) / 5.5) ** 2
            if e <= 1:
                ring = int(e * 6)
                c.set(x, y, E_CORE if e < 0.05 else E_RING if ring % 2 == 0 else E_RING_DARK)
    for x in range(10, 39):  # moss hanging over the rim
        depth = 2 + int(2 * (1 + math.sin(x * 0.9 + sway)))
        for y in range(18, 18 + depth):
            if (x, y) in c.px:
                c.set(x, y, E_MOSS_LIGHT if y == 18 else E_MOSS if y < 17 + depth else E_MOSS_DARK)
    c.ellipse(15, 12, 3, 1.6, E_MOSS)
    c.ellipse(31, 11, 2.4, 1.4, E_MOSS_LIGHT)
    # A small sprout on top.
    c.rect(25, 5, 26, 11, E_MOSS_DARK)
    c.ellipse(23.5 - sway, 5.5, 2, 1.4, E_MOSS_LIGHT)
    c.ellipse(27.5 + sway, 4.5, 2, 1.4, E_MOSS)
    # The face: deep hollows with glowing eyes, and a jagged mouth.
    for ex in (15, 28):
        c.rect(ex, 25, ex + 6, 31, E_OUT)
        c.rect(ex + 1, 26 + frame, ex + 5, 29 + frame, E_GLOW)
        c.rect(ex + 2, 26 + frame, ex + 4, 27 + frame, E_GLOW_HI)
    c.rect(16, 23, 22, 24, E_OUT)  # angry brows
    c.rect(27, 23, 33, 24, E_OUT)
    c.rect(17, 34, 32, 39, E_OUT)
    for x in range(17, 32, 3):  # teeth of splinters
        c.set(x + 1, 34, E_RING_DARK)
        c.set(x + 1, 38, E_RING_DARK)
        c.set(x + 2, 34, E_RING_DARK)
    c.outline(E_OUT)
    return c


def main():
    for f in range(2):
        save(slime(f), 'monsters', 'slime_%d' % (f + 1))
        save(beetle(f), 'monsters', 'beetle_%d' % (f + 1))
        save(wolf(f), 'monsters', 'wolf_%d' % (f + 1))
        save(elder_stump(f), 'monsters', 'stump_%d' % (f + 1))
    print('wrote monsters')


if __name__ == '__main__':
    main()
