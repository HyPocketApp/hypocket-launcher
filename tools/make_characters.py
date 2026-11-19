"""Draws the people, the fire creatures and the small things on the map, at
32 art pixels per map tile. Light comes from the top left. Run:
    python3 tools/make_characters.py

  chars/hero_D_F.png     32x40: the lumberjack facing D (s, n, e; the game
                         flips e to face west), frame F (1 standing, 2 and 3
                         walking). His feet are on the bottom row.
  chars/goddess_D_F.png  32x40: the Moon Goddess, the same way
  chars/goddess_fallen.png 48x24: lying on the ground, before she is saved
  monsters/ember_1/2.png 32x32: a fire creature
  tiles/chest.png, chest_open.png, woodpile.png  32x32
  tiles/fire_1..3.png    32x32: the camp fire, three frames

Every figure is built from filled shapes on a grid, then gets a dark
outline around its silhouette.
"""
import math
import os

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), '..', 'assets')

OUTLINE = (34, 24, 26)
SKIN, SKIN_SH = (236, 188, 150), (204, 150, 116)
BEARD, BEARD_DARK, HAIR = (136, 84, 46), (100, 58, 32), (112, 70, 40)
SHIRT, SHIRT_DARK, SHIRT_LIGHT = (196, 50, 50), (120, 28, 32), (226, 92, 80)
PANTS, PANTS_DARK = (60, 80, 130), (42, 56, 96)
BOOT, BOOT_LIGHT = (70, 46, 30), (104, 70, 44)
CAP, CAP_LIGHT, CAP_DARK = (58, 92, 70), (84, 124, 92), (38, 62, 48)
BELT, BUCKLE = (74, 48, 30), (230, 190, 70)
EYE = (28, 24, 30)

M_HAIR, M_HAIR_SH, M_HAIR_HI = (214, 220, 240), (164, 172, 204), (250, 252, 255)
M_DRESS, M_DRESS_SH, M_DRESS_DEEP = (246, 246, 252), (196, 202, 228), (160, 168, 204)
M_SKIN, M_SKIN_SH = (244, 222, 210), (214, 186, 176)
M_SILVER, M_SILVER_DARK = (200, 214, 240), (120, 136, 180)
M_EYE = (70, 96, 170)
M_OUTLINE = (60, 64, 96)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = {}

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[(x, y)] = c

    def rect(self, x0, y0, x1, y1, c):
        """Fills x0 <= x < x1, y0 <= y < y1; c may be a function of (x, y)."""
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set(x, y, c(x, y) if callable(c) else c)

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1:
                    self.set(x, y, c(x, y) if callable(c) else c)

    def poly(self, pts, c):
        ys = [p[1] for p in pts]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            for x in range(self.w):
                if inside(pts, x + 0.5, y + 0.5):
                    self.set(x, y, c(x, y) if callable(c) else c)

    def flip(self):
        self.px = {(self.w - 1 - x, y): c for (x, y), c in self.px.items()}
        return self

    def outline(self, color=OUTLINE):
        """A one-pixel line around the outside of the silhouette."""
        add = {}
        for (x, y) in self.px:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p = (x + dx, y + dy)
                if p not in self.px and 0 <= p[0] < self.w and 0 <= p[1] < self.h:
                    add[p] = color
        self.px.update(add)
        return self

    def image(self):
        img = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        for (x, y), c in self.px.items():
            img.putpixel((x, y), c + (255,) if len(c) == 3 else c)
        return img


def inside(pts, x, y):
    n, ok = len(pts), False
    for i in range(n):
        (x1, y1), (x2, y2) = pts[i], pts[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            ok = not ok
    return ok


def plaid(x, y):
    """The red flannel: dark stripes both ways, darker where they cross."""
    a, b = x % 5 == 2, y % 5 == 1
    if a and b:
        return (92, 20, 26)
    if a or b:
        return SHIRT_DARK
    return SHIRT_LIGHT if (x + y) % 9 == 0 else SHIRT


# --- The axe --------------------------------------------------------------------

AXE_W, AXE_H = 20, 28
AXE_GRIP = (5, 24)  # the point in his hand; the game turns the axe around it
WOOD_D, WOOD, WOOD_L = (92, 58, 34), (134, 88, 50), (176, 124, 72)
STEEL_D, STEEL, STEEL_L, EDGE = (78, 86, 98), (130, 140, 154), (182, 192, 204), (236, 242, 248)
WRAP, WRAP_L = (70, 40, 30), (110, 66, 46)


def axe():
    """A lumberjack's axe standing up: the handle along x 4-6, the head at the
    top with its blade to the right. The game rotates and flips it."""
    c = Canvas(AXE_W, AXE_H)
    # Handle: light on the left, a few grain lines, a little curve.
    for y in range(3, 27):
        x0 = 4 - (1 if 13 <= y <= 18 else 0)
        c.set(x0, y, WOOD_L)
        c.set(x0 + 1, y, WOOD_D if y % 6 == 0 else WOOD)
        c.set(x0 + 2, y, WOOD_D)
    # Leather wrap at the grip and a knob at the end.
    for y in range(20, 25):
        for x in range(4, 7):
            c.set(x, y, WRAP_L if (y + x) % 3 == 0 else WRAP)
    c.rect(3, 25, 8, 27, WOOD_D)
    # The head: a short poll behind the handle, a narrow neck, and a blade
    # that flares out, longer below (a bearded axe), with a bright edge.
    def steel(x, y):
        return STEEL_L if y <= 3 else STEEL_D if y >= 8 else STEEL
    c.rect(2, 3, 8, 9, steel)                                   # around the handle
    c.poly([(8, 3.5), (11, 4), (11, 8), (8, 8.5)], steel)       # neck
    c.poly([(11, 4), (14, 1.5), (17, 0.5), (18, 3), (18, 10), (17, 13), (15, 13), (11, 8)], steel)
    for y in range(1, 13):  # the cutting edge, curved
        x = 18 if 3 <= y <= 10 else 17
        c.set(x, y, EDGE)
        c.set(x - 1, y, STEEL_L)
    c.rect(4, 4, 7, 8, STEEL_D)  # the eye, with the handle's end showing
    c.rect(5, 5, 6, 7, WOOD_D)
    return c.outline((32, 30, 36))


# --- The lumberjack -------------------------------------------------------------

def hero_legs_front(c, step):
    """Legs seen from the front or back; step lifts one foot (1 left, 2 right)."""
    for i, (x0, x1) in enumerate(((11, 15), (17, 21))):
        lift = 2 if step == i + 1 else 0
        c.rect(x0, 29, x1, 36 - lift, lambda x, y, x0=x0: PANTS_DARK if x == x0 + 3 else PANTS)
        c.rect(x0 - (1 if i == 0 else 0), 35 - lift, x1 + (1 if i == 1 else 0), 39 - lift,
               lambda x, y, l=lift: BOOT_LIGHT if y == 35 - l else BOOT)


def hero_body_front(c, back=False):
    c.rect(9, 18, 23, 29, plaid)                  # torso
    c.rect(6, 19, 9, 27, plaid)                   # arms
    c.rect(23, 19, 26, 27, plaid)
    c.rect(6, 26, 9, 29, SKIN if not back else SKIN_SH)  # hands
    c.rect(23, 26, 26, 29, SKIN if not back else SKIN_SH)
    c.rect(9, 27, 23, 29, BELT)
    if not back:
        c.rect(15, 27, 17, 29, BUCKLE)
        c.rect(15, 18, 17, 23, SHIRT_DARK)        # collar opening
        c.rect(15, 18, 17, 19, SKIN_SH)
    for x in (9, 22):                             # side shade
        for y in range(18, 27):
            c.set(x, y, SHIRT_DARK)


def hero_head_front(c):
    c.rect(10, 7, 22, 18, lambda x, y: SKIN_SH if x >= 20 else SKIN)
    for p in ((10, 7), (21, 7), (10, 17), (21, 17)):
        c.px.pop(p, None)
    # Beard, full and round, with the mouth.
    c.rect(10, 13, 22, 19, lambda x, y: BEARD_DARK if x >= 20 or y == 18 else BEARD)
    c.rect(11, 19, 21, 21, BEARD)
    c.rect(13, 21, 19, 22, BEARD_DARK)
    c.rect(10, 10, 12, 13, BEARD)                 # sideburns
    c.rect(20, 10, 22, 13, BEARD_DARK)
    c.rect(14, 15, 18, 16, (150, 70, 60))         # mouth
    c.rect(15, 12, 17, 14, SKIN_SH)               # nose
    for x in (13, 18):                            # eyes
        c.rect(x, 10, x + 1, 12, EYE)
        c.set(x + 1, 10, (240, 240, 250))
        c.set(x + 1, 11, EYE)
    c.rect(12, 8, 15, 9, BEARD_DARK)              # brows
    c.rect(17, 8, 20, 9, BEARD_DARK)


def hero_cap(c, profile=False):
    x0, x1 = (10, 23) if profile else (9, 23)
    c.ellipse((x0 + x1) / 2, 5.5, (x1 - x0) / 2, 4.5, lambda x, y: CAP_LIGHT if x < 13 and y < 4 else CAP)
    c.rect(x0, 5, x1, 8, lambda x, y: CAP_DARK if y == 7 else CAP_LIGHT if (x + y) % 3 == 0 else CAP)  # folded brim
    c.rect(15, 0, 18, 2, (230, 200, 120))         # a pompom
    c.set(15, 0, (250, 230, 170))


def hero_s(step):
    c = Canvas(32, 40)
    hero_legs_front(c, step)
    hero_body_front(c)
    hero_head_front(c)
    hero_cap(c)
    return c.outline()


def hero_n(step):
    c = Canvas(32, 40)
    hero_legs_front(c, step)
    hero_body_front(c, back=True)
    c.rect(10, 7, 22, 18, lambda x, y: HAIR if y < 15 else SKIN_SH)  # back of the head
    for p in ((10, 17), (21, 17)):
        c.px.pop(p, None)
    c.rect(10, 13, 12, 18, BEARD)                 # beard showing at the sides
    c.rect(20, 13, 22, 18, BEARD_DARK)
    c.rect(8, 11, 10, 14, SKIN_SH)                # ears
    c.rect(22, 11, 24, 14, SKIN_SH)
    hero_cap(c)
    return c.outline()


def hero_e(step):
    c = Canvas(32, 40)
    # Legs: apart while walking, together when standing.
    legs = {1: ((12, 16, 0), (17, 21, 0)), 2: ((11, 15, 2), (18, 22, 0)), 3: ((14, 18, 0), (14, 18, 2))}[step]
    back, front = legs
    for (x0, x1, lift), shade in ((back, True), (front, False)):
        c.rect(x0, 29, x1, 36 - lift, PANTS_DARK if shade else PANTS)
        c.rect(x0, 35 - lift, x1 + 2, 39 - lift, lambda x, y, l=lift: BOOT_LIGHT if y == 35 - l else BOOT)
    c.rect(11, 18, 21, 29, plaid)                 # torso
    c.rect(11, 27, 21, 29, BELT)
    c.rect(11, 18, 12, 27, SHIRT_DARK)
    # Head in profile, looking right.
    c.rect(11, 7, 23, 18, lambda x, y: SKIN if x > 13 else SKIN_SH)
    c.px.pop((22, 17), None)
    c.rect(11, 8, 15, 16, HAIR)                   # hair at the back
    c.rect(14, 11, 16, 14, SKIN_SH)               # ear
    c.rect(16, 13, 23, 19, lambda x, y: BEARD_DARK if y == 18 or x == 16 else BEARD)
    c.rect(17, 19, 22, 21, BEARD)
    c.rect(18, 21, 21, 22, BEARD_DARK)
    c.rect(16, 10, 18, 13, BEARD)                 # sideburn
    c.rect(23, 12, 25, 14, SKIN)                  # nose
    c.set(24, 13, SKIN_SH)
    c.rect(20, 10, 22, 12, EYE)
    c.set(21, 10, (240, 240, 250))
    c.rect(19, 8, 23, 9, BEARD_DARK)
    c.rect(21, 15, 23, 16, (150, 70, 60))
    hero_cap(c, profile=True)
    # The near arm, swinging a little as he walks.
    sw = {1: 0, 2: 1, 3: -1}[step]
    c.rect(14 + sw, 19, 18 + sw, 27, lambda x, y: SHIRT_DARK if x == 17 + sw else plaid(x, y))
    c.rect(14 + sw, 26, 18 + sw, 29, SKIN)
    return c.outline()


# --- The Moon Goddess ------------------------------------------------------------

def hair_shade(x, y):
    # Long strands: thin darker lines running down, a shine near the top.
    if x % 4 == 1 and y > 9 and (y + x) % 9 != 0:
        return M_HAIR_SH
    if 4 <= y <= 6 and x % 4 != 1:
        return M_HAIR_HI
    return M_HAIR


def dress_shade(x, y):
    if x >= 20:
        return M_DRESS_SH
    if (x + 1) % 5 == 0 and y > 27:
        return M_DRESS_SH  # folds of the skirt
    return M_DRESS


def crescent(c, cx, cy):
    for y in range(cy - 3, cy + 3):
        for x in range(cx - 3, cx + 4):
            outer = (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= 9
            inner = (x + 0.5 - cx - 1.5) ** 2 + (y + 0.5 - cy + 1) ** 2 <= 6
            if outer and not inner:
                c.set(x, y, M_SILVER if x < cx else M_SILVER_DARK)


def goddess_skirt(c, step):
    sway = {1: 0, 2: -1, 3: 1}[step]
    c.poly([(11, 22), (21, 22), (26 + sway, 39), (6 + sway, 39)], dress_shade)
    for x in range(7 + sway, 26 + sway):
        c.set(x, 38, M_DRESS_DEEP)
    c.rect(11, 22, 21, 24, M_SILVER)              # belt
    c.set(16, 23, (255, 255, 255))


def goddess_s(step):
    c = Canvas(32, 40)
    c.rect(9, 6, 23, 30, hair_shade)              # hair behind her
    c.ellipse(16, 7, 7, 5, hair_shade)
    goddess_skirt(c, step)
    c.rect(11, 15, 21, 23, dress_shade)           # bodice
    c.rect(8, 16, 11, 25, M_DRESS_SH)             # sleeves
    c.rect(21, 16, 24, 25, M_DRESS_SH)
    c.rect(8, 24, 11, 27, M_SKIN)
    c.rect(21, 24, 24, 27, M_SKIN_SH)
    c.rect(11, 6, 21, 16, lambda x, y: M_SKIN_SH if x >= 19 else M_SKIN)  # face
    c.px.pop((11, 15), None)
    c.px.pop((20, 15), None)
    c.rect(10, 4, 22, 9, hair_shade)              # bangs
    c.set(13, 9, M_HAIR)
    c.set(18, 9, M_HAIR)
    c.rect(10, 9, 12, 20, hair_shade)             # locks framing the face
    c.rect(20, 9, 22, 20, M_HAIR_SH)
    for x in (13, 17):
        c.rect(x, 10, x + 2, 13, M_EYE)
        c.set(x, 10, (30, 36, 70))
        c.set(x + 1, 12, (230, 240, 255))
    c.rect(15, 14, 17, 15, (220, 140, 150))       # mouth
    c.rect(12, 13, 13, 14, (246, 196, 196))       # cheeks
    c.rect(19, 13, 20, 14, (246, 196, 196))
    c.outline(M_OUTLINE)
    crescent(c, 16, 3)
    return c


def goddess_n(step):
    c = Canvas(32, 40)
    goddess_skirt(c, step)
    c.rect(8, 16, 11, 25, M_DRESS_SH)
    c.rect(21, 16, 24, 25, M_DRESS_SH)
    c.rect(8, 24, 11, 27, M_SKIN_SH)
    c.rect(21, 24, 24, 27, M_SKIN_SH)
    c.ellipse(16, 8, 7, 5.5, hair_shade)
    c.poly([(9, 8), (23, 8), (24, 31), (16, 33), (8, 31)], hair_shade)  # hair down her back
    for y in range(12, 32, 4):
        c.set(13, y, M_HAIR_SH)
        c.set(19, y + 2, M_HAIR_SH)
    c.outline(M_OUTLINE)
    crescent(c, 16, 3)
    return c


def goddess_e(step):
    c = Canvas(32, 40)
    c.poly([(8, 8), (16, 8), (16, 30), (10, 31), (7, 26)], hair_shade)  # hair behind
    goddess_skirt(c, step)
    c.rect(12, 15, 21, 23, dress_shade)
    c.rect(12, 6, 23, 16, lambda x, y: M_SKIN if x > 14 else M_SKIN_SH)
    c.px.pop((22, 15), None)
    c.rect(23, 10, 24, 12, M_SKIN)                # nose
    c.ellipse(15, 7, 7, 4.5, hair_shade)
    c.rect(9, 7, 17, 22, hair_shade)
    c.rect(17, 4, 23, 8, hair_shade)              # bangs
    c.rect(19, 10, 21, 13, M_EYE)
    c.set(19, 10, (30, 36, 70))
    c.set(20, 12, (230, 240, 255))
    c.rect(20, 14, 22, 15, (220, 140, 150))
    c.set(21, 13, (246, 196, 196))
    sw = {1: 0, 2: 1, 3: -1}[step]
    c.rect(15 + sw, 16, 19 + sw, 25, M_DRESS_SH)   # near arm
    c.rect(15 + sw, 24, 19 + sw, 27, M_SKIN)
    c.outline(M_OUTLINE)
    crescent(c, 17, 3)
    return c


def goddess_fallen():
    c = Canvas(48, 24)
    c.poly([(2, 8), (16, 6), (18, 18), (4, 20), (1, 14)], hair_shade)   # spread hair
    c.ellipse(13, 13, 5, 5, M_SKIN)                                      # face, turned up
    c.rect(9, 8, 17, 11, hair_shade)
    c.set(12, 13, M_EYE)
    c.set(15, 13, M_EYE)
    c.poly([(17, 8), (34, 9), (45, 12), (45, 19), (34, 20), (17, 18)], dress_shade)
    c.rect(24, 9, 26, 19, M_SILVER)                                      # belt
    c.rect(20, 18, 28, 20, M_SKIN_SH)                                    # an arm
    c.outline(M_OUTLINE)
    crescent(c, 5, 6)
    return c


# --- Fire ------------------------------------------------------------------------

EMBER_OUT, EMBER_RED, EMBER_ORANGE, EMBER_YELLOW, EMBER_WHITE = (110, 24, 20), (214, 60, 40), (255, 150, 40), (255, 220, 90), (255, 250, 210)


def flame(c, cx, cy, r, height, lean, layers=True):
    """A flame: a round body at (cx, cy) whose top narrows into a tip
    `height` above, leaning by `lean`. Red outside, then orange and yellow."""
    def fill(cy, r, height, lean, color):
        for y in range(int(cy - height) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 2, int(cx + r) + 3):
                px, py = x + 0.5, y + 0.5
                body = (px - cx) ** 2 + (py - cy) ** 2 <= r * r
                t = (cy - py) / height
                tip = 0 < t < 1 and abs(px - (cx + lean * t * t)) <= r * (1 - t) ** 1.3
                if body or tip:
                    c.set(x, y, color)

    fill(cy, r, height, lean, EMBER_RED)
    if layers:
        fill(cy + r * 0.2, r * 0.68, height * 0.62, lean * 0.6, EMBER_ORANGE)
        fill(cy + r * 0.4, r * 0.38, height * 0.36, lean * 0.3, EMBER_YELLOW)


def ember(frame):
    c = Canvas(32, 32)
    lean = (-3, 3)[frame]
    flame(c, 16, 20, 8.5, 18, lean)
    c.rect(12, 17, 14, 21, (40, 20, 24))   # eyes
    c.rect(18, 17, 20, 21, (40, 20, 24))
    c.set(12, 17, EMBER_WHITE)
    c.set(18, 17, EMBER_WHITE)
    c.rect(15, 23, 17, 24, (150, 50, 30))  # mouth
    c.outline(EMBER_OUT)
    for x, y in (((6, 9), (25, 5)), ((26, 12), (5, 4)))[frame]:  # sparks
        c.set(x, y, EMBER_YELLOW)
    return c


def campfire(frame):
    c = Canvas(32, 32)
    # Ash and a ring of stones.
    c.ellipse(16, 24, 11, 4.5, (60, 54, 52))
    for i in range(8):
        a = i * math.pi / 4 + 0.3
        x, y = 16 + math.cos(a) * 11, 24 + math.sin(a) * 4.6
        c.ellipse(x, y, 2.6, 2, lambda xx, yy, y=y: (176, 176, 184) if yy < y - 0.5 else (122, 124, 136))
    # Two crossed logs.
    c.poly([(7, 22), (24, 25), (24, 28), (7, 25)], lambda x, y: (160, 116, 72) if y < 24 else (106, 70, 42))
    c.poly([(25, 22), (8, 25), (8, 28), (25, 25)], lambda x, y: (176, 132, 86) if y < 24 else (112, 76, 48))
    c.ellipse(7.5, 23.5, 1.6, 1.6, (214, 176, 118))
    c.ellipse(24.5, 23.5, 1.6, 1.6, (214, 176, 118))
    flame(c, 16 + (0, 1, -1)[frame], 21, 5, (14, 16, 13)[frame], (-2, 1, 2)[frame])
    for x, y in (((10, 6), (21, 3)), ((22, 8), (12, 4)), ((9, 10), (19, 5)))[frame]:
        c.set(x, y, EMBER_YELLOW)
    return c


# --- Things on the map -------------------------------------------------------------

WOOD_HI, WOOD, WOOD_DARK, WOOD_OUT = (190, 140, 88), (150, 104, 62), (106, 70, 42), (60, 38, 26)
IRON, IRON_LIGHT = (88, 92, 104), (150, 156, 170)
GOLD, GOLD_LIGHT = (230, 180, 60), (255, 230, 120)


def chest(opened):
    c = Canvas(32, 32)
    c.rect(4, 15, 28, 29, lambda x, y: WOOD_DARK if (y - 15) % 5 == 4 else WOOD)   # box
    if opened:
        c.rect(4, 6, 28, 13, lambda x, y: WOOD_HI if y < 8 else WOOD)  # lid, tipped back
        c.rect(5, 13, 27, 16, (34, 24, 22))                            # dark inside
        c.rect(4, 6, 6, 13, IRON)
        c.rect(26, 6, 28, 13, IRON)
    else:
        c.ellipse(16, 15, 12, 6, lambda x, y: WOOD_HI if y < 12 else WOOD)  # round lid
        c.rect(4, 15, 28, 17, WOOD_DARK)
        c.rect(14, 14, 18, 20, GOLD)                                    # lock
        c.set(14, 14, GOLD_LIGHT)
        c.set(16, 17, (60, 40, 20))
    for x in (4, 5, 26, 27):                                            # iron bands
        for y in range(9 if not opened else 15, 29):
            if (x, y) in c.px:
                c.set(x, y, IRON_LIGHT if x in (4, 26) else IRON)
    c.outline(WOOD_OUT)
    return c


def woodpile():
    c = Canvas(32, 32)
    rows = [(4, 26, 6), (7, 20, 5), (10, 14, 4)]
    for x0, y, n in rows:
        for i in range(n if x0 == 4 else n - 1):
            cx, cy = x0 + 2 + i * 5.2, y
            c.ellipse(cx, cy, 2.7, 2.7, (176, 132, 86))
            c.ellipse(cx, cy, 1.8, 1.8, (214, 176, 118))
            c.set(int(cx), int(cy), (160, 118, 72))
    c.outline(WOOD_OUT)
    return c


# --- Writing them out -------------------------------------------------------------------

def save(c, folder, name):
    d = os.path.join(ROOT, folder)
    os.makedirs(d, exist_ok=True)
    (c.image() if isinstance(c, Canvas) else c).save(os.path.join(d, name + '.png'), optimize=True)


def main():
    save(axe(), 'chars', 'axe')
    for f in (1, 2, 3):
        save(hero_s(0 if f == 1 else f - 1), 'chars', 'hero_s_%d' % f)
        save(hero_n(0 if f == 1 else f - 1), 'chars', 'hero_n_%d' % f)
        save(hero_e(f), 'chars', 'hero_e_%d' % f)
        save(goddess_s(f), 'chars', 'goddess_s_%d' % f)
        save(goddess_n(f), 'chars', 'goddess_n_%d' % f)
        save(goddess_e(f), 'chars', 'goddess_e_%d' % f)
        save(campfire(f - 1), 'tiles', 'fire_%d' % f)
    save(goddess_fallen(), 'chars', 'goddess_fallen')
    save(ember(0), 'monsters', 'ember_1')
    save(ember(1), 'monsters', 'ember_2')
    save(chest(False), 'tiles', 'chest')
    save(chest(True), 'tiles', 'chest_open')
    save(woodpile(), 'tiles', 'woodpile')
    print('wrote characters and objects to', os.path.normpath(ROOT))


if __name__ == '__main__':
    main()
