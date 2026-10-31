"""NORMA cue rework v1 (SVG): pattern warnings drawn the way the game's other
mobs warn — the body winds up, the real danger or safe spot is shown, and
the time left fills in — instead of diagram symbols (arrows, reticles, rays).

Lines only, no fills, no dashed lines, no backdrop plates, no thorn spikes.
Usage: python Norma_Cue_Rework_v1.py <out_dir>
"""
import math, sys, os

OUT = sys.argv[1]

STYLE = """
*{vector-effect:non-scaling-stroke}
.bg{fill:#080910}
.frame{fill:none;stroke:#2b2d3d;stroke-width:1.4}
.scr{fill:none;stroke:#3a3d52;stroke-width:1.4}
.i{fill:none;stroke:#b8efff;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:miter}
.it{fill:none;stroke:#b8efff;stroke-width:3;stroke-linecap:round;stroke-linejoin:miter}
.ig{fill:none;stroke:#b8efff;stroke-opacity:.22;stroke-width:9;stroke-linecap:round}
.id{fill:none;stroke:#4d6a78;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.ih{fill:none;stroke:#ebfdff;stroke-width:1.3;stroke-linecap:round;stroke-linejoin:miter}
.w{fill:none;stroke:#eef1ff;stroke-width:2.2;stroke-linecap:round;stroke-linejoin:miter}
.wt{fill:none;stroke:#eef1ff;stroke-width:3.4;stroke-linecap:round;stroke-linejoin:miter}
.wg{fill:none;stroke:#eef1ff;stroke-opacity:.18;stroke-width:10;stroke-linecap:round}
.b{fill:none;stroke:#7f93ff;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:miter}
.bt{fill:none;stroke:#9fb0ff;stroke-width:2.8;stroke-linecap:round;stroke-linejoin:miter}
.d{fill:none;stroke:#3f4778;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.v{fill:none;stroke:#c3b2ff;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:miter}
.vd{fill:none;stroke:#c3b2ff;stroke-opacity:.35;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.vt{fill:none;stroke:#d8ccff;stroke-width:2.6;stroke-linecap:round;stroke-linejoin:miter}
.vg{fill:none;stroke:#c3b2ff;stroke-opacity:.20;stroke-width:9;stroke-linecap:round}
.c{fill:none;stroke:#34e0f0;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.g{fill:none;stroke:#8a90a8;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:miter}
.gd{fill:none;stroke:#4a4e60;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.r{fill:none;stroke:#ff8a9a;stroke-width:2;stroke-linecap:round;stroke-linejoin:miter}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:20px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 26px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.p{font:17px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#aab0c6}
.s{font:15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9097ae}
.hg{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8a90a8}
.hr{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#ff8a9a}
.hc{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.hv{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
.hb{font:700 14px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8fa2ff}
.tag{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff;letter-spacing:2px}
"""

TAU = math.tau


class Svg:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.o = w, h, title, []

    def add(self, s):
        self.o.append(s)

    def path(self, cls, d):
        self.add(f'<path class="{cls}" d="{d}"/>')

    def line(self, cls, x1, y1, x2, y2):
        self.path(cls, f"M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}")

    def poly(self, cls, pts, close=True):
        d = "M" + " ".join(f"{x:.1f} {y:.1f}" for x, y in pts) + ("Z" if close else "")
        self.path(cls, d)

    def circle(self, cls, x, y, r):
        self.add(f'<circle class="{cls}" cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')

    def arc(self, cls, x, y, r, a0, sweep):
        x0, y0 = x + math.cos(a0) * r, y + math.sin(a0) * r
        x1, y1 = x + math.cos(a0 + sweep) * r, y + math.sin(a0 + sweep) * r
        large = 1 if sweep > math.pi else 0
        self.path(cls, f"M{x0:.1f} {y0:.1f} A{r:.1f} {r:.1f} 0 {large} 1 {x1:.1f} {y1:.1f}")

    def rect(self, cls, x, y, w, h):
        self.add(f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

    def text(self, cls, x, y, s, anchor="start"):
        s = s.replace("&", "&amp;").replace("<", "&lt;")
        self.add(f'<text class="{cls}" x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}">{s}</text>')

    def lines(self, cls, x, y, rows, gap=21, anchor="start"):
        for i, r in enumerate(rows):
            self.text(cls, x, y + i * gap, r, anchor)

    def save(self, name):
        body = "\n".join(self.o)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
               f'width="{self.w}" height="{self.h}"><title>{self.title}</title>'
               f'<defs><style>{STYLE}</style></defs>'
               f'<rect class="bg" width="{self.w}" height="{self.h}"/>\n{body}\n</svg>')
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(svg)


# ---------- pieces ----------

HALF = 24.0   # body half size on the board


def rot(x, y, a):
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def body(S, x, y, half=HALF, cls="it", sx=1.0, sy=1.0, ang=0.0, core=True):
    """Placeholder body: a square (optionally stretched) with a centre point."""
    pts = [rot(px * half * sx, py * half * sy, ang) for px, py in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    S.poly(cls, [(x + px, y + py) for px, py in pts])
    if core:
        S.circle("i", x, y, 2.5)


def orbit_brackets(S, x, y, r, phase, cls="i", arm=9.0):
    """The body's four orbiting corner brackets (as in game)."""
    for k in range(4):
        a = phase + math.pi / 4 + math.pi / 2 * k
        cx, cy = x + math.cos(a) * r, y + math.sin(a) * r
        # an L whose corner points outward
        u = (math.cos(a), math.sin(a))
        t1 = (math.cos(a + 0.75 * math.pi), math.sin(a + 0.75 * math.pi))
        t2 = (math.cos(a - 0.75 * math.pi), math.sin(a - 0.75 * math.pi))
        S.poly(cls, [(cx + t1[0] * arm, cy + t1[1] * arm), (cx, cy), (cx + t2[0] * arm, cy + t2[1] * arm)], close=False)


def barrel(S, x, y, a, r0, r1, fill, cls_rail="v", cls_fill="w", gap=6.0):
    """Two rails along angle a; light fed down the middle up to `fill`."""
    ux, uy = math.cos(a), math.sin(a)
    px, py = -uy, ux
    for s in (-1, 1):
        S.line(cls_rail, x + ux * r0 + px * gap * s, y + uy * r0 + py * gap * s,
               x + ux * r1 + px * gap * s, y + uy * r1 + py * gap * s)
    # cap at the muzzle
    S.line(cls_rail, x + ux * r1 + px * gap, y + uy * r1 + py * gap,
           x + ux * r1 - px * gap, y + uy * r1 - py * gap)
    if fill > 0:
        S.line(cls_fill, x + ux * r0, y + uy * r0, x + ux * (r0 + (r1 - r0) * fill), y + uy * (r0 + (r1 - r0) * fill))


def shard(S, x, y, ang, s=7, cls="i"):
    u = (math.cos(ang), math.sin(ang))
    v = (-u[1], u[0])
    pts = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]
    S.poly(cls, [(x + (px * u[0] + py * v[0]) * s, y + (px * u[1] + py * v[1]) * s) for px, py in pts])


def player(S, x, y, ang=-90):
    a = math.radians(ang)
    pts = []
    for da, r in ((0, 13), (140, 10), (180, 4), (220, 10)):
        t = a + math.radians(da)
        pts.append((x + r * math.cos(t), y + r * math.sin(t)))
    S.poly("c", pts)


def mob(S, x, y, r=10, cls="b"):
    S.poly(cls, [(x + r * math.cos(math.radians(60 * i + 30)), y + r * math.sin(math.radians(60 * i + 30))) for i in range(6)])
    S.circle(cls, x, y, 2)


def pane(S, x, y, w, h, cls="bt", title=None):
    S.rect(cls, x, y, w, h)
    S.line(cls, x, y + 20, x + w, y + 20)
    gx = x + w - 13
    S.line(cls, gx - 4, y + 6, gx + 4, y + 14)
    S.line(cls, gx + 4, y + 6, gx - 4, y + 14)
    S.rect(cls, gx - 24, y + 6, 8, 8)
    if title:
        S.text("hb", x + 8, y + 15, title)


def pane_brackets(S, x, y, w, h, arm, cls="w"):
    for cx, cy, sx, sy in ((x, y, -1, -1), (x + w, y, 1, -1), (x, y + h, -1, 1), (x + w, y + h, 1, 1)):
        ax = min(arm, w / 2)
        ay = min(arm, h / 2)
        S.poly(cls, [(cx - ax * sx, cy), (cx, cy), (cx, cy - ay * sy)], close=False)


def spark(S, x, y, cls="v", r=7):
    for i in range(6):
        t = math.radians(60 * i + 15)
        S.line(cls, x + math.cos(t) * r * 0.45, y + math.sin(t) * r * 0.45, x + math.cos(t) * r, y + math.sin(t) * r)


def arrow(S, x1, y1, x2, y2, cls="v", head=8):
    S.line(cls, x1, y1, x2, y2)
    a = math.atan2(y2 - y1, x2 - x1)
    for da in (2.6, -2.6):
        S.line(cls, x2, y2, x2 + math.cos(a + da) * head, y2 + math.sin(a + da) * head)


# ---------- layout ----------

W = 1800
ROW_H = 300
TOP = 170
FW, FH = 300, 230        # frame size
LABEL_W = 250
NOW_X = 30 + LABEL_W
NEW_X0 = NOW_X + FW + 70
GAP = 24


def frame(S, x, y, caption, cls_tag, tag):
    S.rect("frame", x, y, FW, FH)
    S.text(cls_tag, x + 10, y + 22, tag)
    S.lines("s", x, y + FH + 22, caption, 20)


def row(S, idx, num, name, why, now_draw, now_caption, steps):
    y = TOP + idx * ROW_H
    S.text("h", 30, y + 34, f"{num}  {name}")
    S.lines("p", 30, y + 66, why, 22)
    frame(S, NOW_X, y, now_caption, "hr", "지금")
    now_draw(S, NOW_X + FW / 2, y + FH / 2 + 12)
    arrow(S, NOW_X + FW + 16, y + FH / 2, NOW_X + FW + 54, y + FH / 2, "g", 9)
    for k, (tag, draw, cap) in enumerate(steps):
        fx = NEW_X0 + k * (FW + GAP)
        frame(S, fx, y, cap, "hc" if k < len(steps) - 1 else "hv", tag)
        draw(S, fx + FW / 2, y + FH / 2 + 12)


# ---------- 6 cross ----------

def cross_now(S, x, y):
    body(S, x, y)
    orbit_brackets(S, x, y, HALF * 1.6, 0.3, "id")
    for k in range(4):
        a = 0.25 + math.pi / 2 * k
        S.line("v", x + math.cos(a) * 30, y + math.sin(a) * 30, x + math.cos(a) * 95, y + math.sin(a) * 95)
    S.arc("v", x, y, 70, 0.5, 0.75)
    ae = 1.25
    ex, ey = x + math.cos(ae) * 70, y + math.sin(ae) * 70
    tx, ty = -math.sin(ae), math.cos(ae)
    S.line("v", ex, ey, ex - tx * 12 + math.cos(ae) * 7, ey - ty * 12 + math.sin(ae) * 7)
    S.line("v", ex, ey, ex - tx * 12 - math.cos(ae) * 7, ey - ty * 12 - math.sin(ae) * 7)


def cross_1(S, x, y):
    # the orbit brackets swing out to the four barrel axes (ghosts show the path)
    body(S, x, y, cls="it")
    orbit_brackets(S, x, y, HALF * 1.6, 0.3, "vd")
    for k in range(4):
        a = 0.25 + math.pi / 2 * k
        barrel(S, x, y, a, HALF * 1.5, HALF * 3.0, 0.0, "v")


def cross_2(S, x, y):
    # barrels turn the real way (afterimages trail behind) while light fills them
    body(S, x, y, cls="it", sx=0.9, sy=0.9)
    for k in range(4):
        a = 0.45 + math.pi / 2 * k
        for g in (1, 2):
            barrel(S, x, y, a - 0.11 * g, HALF * 1.5, HALF * 3.0, 0.0, "vd")
        barrel(S, x, y, a, HALF * 1.5, HALF * 3.0, 0.65, "v", "w")


def cross_3(S, x, y):
    body(S, x, y, cls="wt")
    for k in range(4):
        a = 0.65 + math.pi / 2 * k
        barrel(S, x, y, a, HALF * 1.5, HALF * 3.0, 1.0, "w", "wt")
        for j in range(3):
            d = HALF * 3.3 + j * 14
            aa = a - j * 0.10
            shard(S, x + math.cos(aa) * d, y + math.sin(aa) * d, aa, 7, "i")


# ---------- 7 aim ----------

PLX, PLY = 105, -55


def aim_now(S, x, y):
    body(S, x, y)
    orbit_brackets(S, x, y, HALF * 1.6, 0.3, "id")
    player(S, x + PLX, y + PLY, 150)
    a = math.atan2(PLY, PLX)
    ux, uy = math.cos(a), math.sin(a)
    S.line("v", x + ux * 30, y + uy * 30, x + ux * 95, y + uy * 95)
    ex, ey = x + ux * 95, y + uy * 95
    px, py = -uy, ux
    for s in (-1, 1):
        S.line("v", ex + px * 11 * s, ey + py * 11 * s, ex + px * 11 * s - ux * 11, ey + py * 11 * s - uy * 11)
    S.line("v", ex + px * 11, ey + py * 11, ex - px * 11, ey - py * 11)


def aim_frame(S, x, y, k):
    a = math.atan2(PLY, PLX)
    ux, uy = math.cos(a), math.sin(a)
    px, py = -uy, ux
    pull = 8 * k if k < 1.5 else -4
    body(S, x - ux * pull * 0.4, y - uy * pull * 0.4, cls="wt" if k >= 1.5 else "it")
    player(S, x + PLX, y + PLY, 150)
    # the player-side bracket drawn back as a sling (tension grows)
    bx, by = x + ux * (HALF * 1.7 - pull), y + uy * (HALF * 1.7 - pull)
    arm = 11
    S.poly("v" if k < 1.5 else "w", [(bx + px * arm - ux * 5, by + py * arm - uy * 5), (bx + px * arm, by + py * arm),
                                     (bx - px * arm, by - py * arm), (bx - px * arm - ux * 5, by - py * arm - uy * 5)], close=False)
    # the other brackets fold back behind the body
    for s in (-1, 1):
        a2 = a + math.pi + s * 0.6
        cx, cy = x + math.cos(a2) * HALF * 1.7, y + math.sin(a2) * HALF * 1.7
        S.circle("id", cx, cy, 3)
    if k < 1.5:
        # the real line of fire grows with the charge, and stops well short
        ln = 30 + 70 * k
        S.line("vg", bx, by, bx + ux * ln, by + uy * ln)
        S.line("v", bx, by, bx + ux * ln, by + uy * ln)
    else:
        S.line("wg", bx, by, bx + ux * 100, by + uy * 100)
        for j in range(5):
            d = 26 + j * 19
            shard(S, bx + ux * d + px * (2 if j % 2 else -2), by + uy * d + py * (2 if j % 2 else -2), a, 7, "i")


# ---------- 2 refuge ----------

def refuge_scene(S, x, y, boss_dx=-70):
    """Boss on the left, the nearest pane on the right, player between."""
    return x + boss_dx, y + 10, (x + 18, y - 70, 110, 120), (x - 10, y + 30)


def refuge_now(S, x, y):
    bx, by, pr, pl = refuge_scene(S, x, y)
    pane(S, *pr, "d")
    body(S, bx, by)
    for i in range(24):
        a = TAU * i / 24
        S.line("vd", bx + math.cos(a) * 32, by + math.sin(a) * 32, bx + math.cos(a) * 82, by + math.sin(a) * 82)
    player(S, *pl, 0)


def refuge_frame(S, x, y, k):
    bx, by, pr, pl = refuge_scene(S, x, y)
    px0, py0, pw, ph = pr
    if k < 1.5:
        pane(S, *pr, "d")
        # safe spot shows: brackets grow along the pane edges with the time left
        pane_brackets(S, px0, py0, pw, ph, 14 + (pw / 2 - 14) * k, "w")
        if k > 0.5:
            for cx, cy in ((px0, py0), (px0 + pw, py0), (px0, py0 + ph), (px0 + pw, py0 + ph)):
                S.circle("wg", cx, cy, 1)
        # the boss gathers: rings shrink into the body
        body(S, bx, by, cls="it", sx=1 - 0.12 * k, sy=1 - 0.12 * k)
        for j in range(3):
            r = HALF * (2.5 - 1.1 * k) + j * 8
            S.arc("v", bx, by, r, 0.3 + j * 0.9, TAU * 0.8)
        player(S, *pl, -20)
        if k > 0.5:
            S.line("c", pl[0] + 8, pl[1] - 3, pl[0] + 26, pl[1] - 10)
    else:
        pane(S, *pr, "bt")
        pane_brackets(S, px0, py0, pw, ph, 18, "w")
        body(S, bx, by, cls="wt")
        for ring, (r, n) in enumerate(((44, 14), (70, 20))):
            for i in range(n):
                a = TAU * (i + 0.5 * ring) / n
                sx, sy = bx + math.cos(a) * r, by + math.sin(a) * r
                inside = px0 < sx < px0 + pw and py0 < sy < py0 + ph
                if inside:
                    continue
                if abs(sx - px0) < 10 and py0 < sy < py0 + ph:
                    spark(S, px0, sy, "v", 7)
                    continue
                shard(S, sx, sy, a, 6, "i")
        player(S, px0 + pw * 0.55, py0 + ph * 0.62, 180)


# ---------- 9 summon ----------

STAR = [(-26, -14), (-6, -26), (18, -16), (22, 10), (0, 22), (-22, 12)]


def summon_now(S, x, y):
    px0, py0, pw, ph = x - 85, y - 75, 170, 150
    pane(S, px0, py0, pw, ph, "bt", "note_07.txt")
    S.rect("v", px0 + 10, py0 + 26, pw - 20, 8)
    S.line("v", px0 + 12, py0 + 30, px0 + 12 + (pw - 24) * 0.6, py0 + 30)
    S.text("hg", x, y + 20, "로딩 바", "middle")


def summon_frame(S, x, y, k):
    px0, py0, pw, ph = x - 85, y - 75, 170, 150
    pane(S, px0, py0, pw, ph, "bt" if k < 1.5 else "w", "note_07.txt")
    cx, cy = x, y + 14
    n = len(STAR)
    if k < 1.5:
        lit = max(1, int(round(n * k)))
        for i, (sx, sy) in enumerate(STAR):
            S.circle("w" if i < lit else "id", cx + sx, cy + sy, 2.6 if i < lit else 1.8)
        for i in range(lit - 1):
            a, b = STAR[i], STAR[i + 1]
            S.line("v", cx + a[0], cy + a[1], cx + b[0], cy + b[1])
        # the newest edge is being drawn: a bright head on it
        if lit < n:
            a, b = STAR[lit - 1], STAR[lit]
            t = 0.55
            hx, hy = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            S.line("w", cx + a[0], cy + a[1], cx + hx, cy + hy)
            S.circle("w", cx + hx, cy + hy, 3)
    else:
        for i in range(n):
            a, b = STAR[i], STAR[(i + 1) % n]
            S.line("vd", cx + a[0] * 1.5, cy + a[1] * 1.5, cx + b[0] * 1.5, cy + b[1] * 1.5)
        mob(S, cx - 18, cy - 4, 11, "b")
        mob(S, cx + 20, cy + 4, 11, "b")
        mob(S, cx, cy - 30, 9, "b")
        for i in range(6):
            a = TAU * i / 6 + 0.2
            S.line("w", cx + math.cos(a) * 34, cy + math.sin(a) * 34, cx + math.cos(a) * 44, cy + math.sin(a) * 44)


# ---------- 1 hide ----------

def hide_now(S, x, y):
    px0, py0, pw, ph = x + 20, y - 80, 110, 100
    pane(S, px0, py0, pw, ph, "g")
    bx, by = x - 80, y + 50
    body(S, bx, by)
    S.line("ig", bx, by, px0 + pw / 2, py0 + ph / 2 + 10)
    S.line("ih", bx, by, px0 + pw / 2, py0 + ph / 2 + 10)


def hide_frame(S, x, y, k):
    px0, py0, pw, ph = x + 20, y - 80, 110, 100
    tx, ty = px0 + pw / 2, py0 + ph / 2 + 10
    bx, by = x - 80, y + 50
    a = math.atan2(ty - by, tx - bx)
    if k < 0.75:
        pane(S, px0, py0, pw, ph, "g")
        pane_brackets(S, px0 - 12, py0 - 12, pw + 24, ph + 24, 12, "v")
        # the body stretches toward the window, its brackets lean the same way
        body(S, bx, by, cls="it", sx=1.45, sy=0.72, ang=a)
        for j in (1, 2):
            gx, gy = bx - math.cos(a) * 8 * j, by - math.sin(a) * 8 * j
            body(S, gx, gy, cls="id", sx=1.2, sy=0.8, ang=a, core=False)
    elif k < 1.5:
        pane(S, px0, py0, pw, ph, "g")
        pane_brackets(S, px0 - 4, py0 - 4, pw + 8, ph + 8, 16, "w")
        # a chain of afterimages pulled into the window, shrinking as they go
        for j in range(5):
            t = j / 4
            gx, gy = bx + (tx - bx) * t * 0.85, by + (ty - by) * t * 0.85
            s = 1.0 - 0.45 * t
            body(S, gx, gy, half=HALF * s, cls="id" if j < 4 else "it", sx=1.3, sy=0.75, ang=a, core=j == 4)
    else:
        pane(S, px0, py0, pw, ph, "g")
        pane_brackets(S, px0, py0, pw, ph, 20, "wt")
        body(S, tx, ty, half=HALF * 0.8, cls="wt")
        for j in range(6):
            aa = TAU * j / 6 + 0.3
            d = 36
            shard(S, tx + math.cos(aa) * d, ty + math.sin(aa) * d, aa, 6, "i")


# ---------- board ----------

def board():
    rows = 5
    H = TOP + rows * ROW_H + 40
    S = Svg(W, H, "NORMA 예고 개편안 v1")
    S.text("t1", 30, 56, "NORMA 예고 개편안 v1")
    S.text("t2", 30, 92, "예고 = 몸의 준비 동작 + 실제 위험(또는 안전) 위치 + 차오르는 남은 시간 · 화살표·조준경·방사선 같은 도식은 뺀다")
    S.text("s", 30, 122, "다른 몹과 같은 문법: Quasar 충전(고리 접힘 → 빛 유입 → 고리 기립), Genesis 껍데기 장전, Gravis 실제 비행 경로 + 차오르는 도착 원")
    S.text("s", 30, 144, "색: 보라 = 준비(경고) · 흰색 = 차오름/발사 순간 · 하늘 = 실제 탄 · 선만, 채우기·점선·막·가시 없음")

    row(S, 0, "6", "십자 연사", ["궤도 괄호가 포신이 된다.", "회전 방향은 화살표 대신", "실제로 돌면서 보여 준다."],
        cross_now, ["고정 직선 4개 + 회전 화살표"],
        [("1 펼침", cross_1, ["궤도 괄호 4개가 포신 축으로", "펼쳐져 레일 2줄이 된다"]),
         ("2 회전 · 장전", lambda S, x, y: cross_2(S, x, y), ["실제 방향으로 돌기 시작(잔상)", "레일 사이로 빛이 차오른다"]),
         ("3 발사", cross_3, ["빛이 끝까지 차면 포신이", "하얗게 튀며 4줄기 발사"])])

    row(S, 1, "7", "조준 연사", ["플레이어 쪽 괄호가", "새총처럼 뒤로 당겨진다."],
        aim_now, ["조준선 + 조준경 괄호"],
        [("1 당김", lambda S, x, y: aim_frame(S, x, y, 0.3), ["앞 괄호가 뒤로 당겨지고", "나머지 괄호는 뒤로 접힌다"]),
         ("2 조준", lambda S, x, y: aim_frame(S, x, y, 1.0), ["실제 탄이 갈 짧은 선이", "충전만큼 자라며 나를 따라온다"]),
         ("3 발사", lambda S, x, y: aim_frame(S, x, y, 2.0), ["괄호가 튕겨 나가고", "선이 하얗게 바뀌며 연사"])])

    row(S, 2, "2", "피난", ["위험 대신 안전한 곳을", "보여 준다."],
        refuge_now, ["희미한 방사선 24개"],
        [("1 모음", lambda S, x, y: refuge_frame(S, x, y, 0.25), ["고리들이 몸 안으로 수축", "피난 창 괄호가 자라기 시작"]),
         ("2 차오름", lambda S, x, y: refuge_frame(S, x, y, 0.9), ["창 괄호가 테두리를 따라 자라", "다 만나면 그때 쏟아진다"]),
         ("3 범람", lambda S, x, y: refuge_frame(S, x, y, 2.0), ["원형 탄막, 창 경계에서", "탄이 불꽃으로 사라진다"])])

    row(S, 3, "9", "몹 소환", ["창 안에서 별자리가", "\"컴파일\"되어 몹이 된다."],
        summon_now, ["제목줄 아래 로딩 바"],
        [("1 점", lambda S, x, y: summon_frame(S, x, y, 0.3), ["창 안에 별 점이 하나씩", "켜진다"]),
         ("2 연결", lambda S, x, y: summon_frame(S, x, y, 0.75), ["빛나는 머리가 점을 차례로", "잇는다 = 남은 시간"]),
         ("3 출현", lambda S, x, y: summon_frame(S, x, y, 2.0), ["별자리가 닫히면 펼쳐지며", "몹이 나온다"])])

    row(S, 4, "1", "창 너머의 보스", ["레이저 대신 몸이 직접", "창으로 빨려 들어간다."],
        hide_now, ["보스 → 창 레이저"],
        [("1 늘어남", lambda S, x, y: hide_frame(S, x, y, 0.5), ["몸이 창 쪽으로 늘어나고", "창 둘레에 보라 괄호"]),
         ("2 빨려 듦", lambda S, x, y: hide_frame(S, x, y, 1.0), ["작아지는 잔상 사슬로", "창 안으로 들어간다"]),
         ("3 안에서", lambda S, x, y: hide_frame(S, x, y, 2.0), ["창 괄호가 조여지고", "안에서 연사 시작"])])

    S.save("Norma_Cue_Rework_v1.svg")


board()
print("ok")
