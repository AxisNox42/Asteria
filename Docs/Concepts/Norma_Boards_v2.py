"""NORMA boss concept boards v2 (SVG): an armed, enlarged Regulus.

Lines only, no fills, no dashed lines. Window links show only at the moment the
boss creates or commands a window (Regulus' instant shield laser), never persist.
Usage: python Norma_Boards_v2.py <out_dir>
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
.b{fill:none;stroke:#7f93ff;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:miter}
.bt{fill:none;stroke:#9fb0ff;stroke-width:2.8;stroke-linecap:round;stroke-linejoin:miter}
.d{fill:none;stroke:#3f4778;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.v{fill:none;stroke:#c3b2ff;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:miter}
.vt{fill:none;stroke:#d8ccff;stroke-width:2.6;stroke-linecap:round;stroke-linejoin:miter}
.c{fill:none;stroke:#34e0f0;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.g{fill:none;stroke:#8a90a8;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:miter}
.gd{fill:none;stroke:#4a4e60;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.r{fill:none;stroke:#ff8a9a;stroke-width:2;stroke-linecap:round;stroke-linejoin:miter}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:20px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 26px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.p{font:17px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#aab0c6}
.s{font:14px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#868ca4}
.hw{font:700 18px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef1ff}
.hb{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8fa2ff}
.hi{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#b8efff}
.hv{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
.hc{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.hg{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8a90a8}
.tag{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff;letter-spacing:2px}
"""


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
        """Arc from angle a0 (rad) sweeping `sweep` rad clockwise on screen."""
        x0, y0 = x + math.cos(a0) * r, y + math.sin(a0) * r
        x1, y1 = x + math.cos(a0 + sweep) * r, y + math.sin(a0 + sweep) * r
        large = 1 if sweep > math.pi else 0
        self.path(cls, f"M{x0:.1f} {y0:.1f} A{r:.1f} {r:.1f} 0 {large} 1 {x1:.1f} {y1:.1f}")

    def rect(self, cls, x, y, w, h):
        self.add(f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

    def text(self, cls, x, y, s, anchor="start"):
        s = s.replace("&", "&amp;").replace("<", "&lt;")
        self.add(f'<text class="{cls}" x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}">{s}</text>')

    def lines(self, cls, x, y, rows, gap=24, anchor="start"):
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


# ---------- Regulus-derived body ----------

def crest(S, x, y, base, phase=0.0, cls="i"):
    """Regulus shield crest: four kite plates with flat tips around a square heart."""
    tip = base * 1.15
    for k in range(4):
        a = phase + math.pi / 2 * k
        pts = [(x + math.cos(a) * tip * 0.30, y + math.sin(a) * tip * 0.30),
               (x + math.cos(a - 0.42) * tip * 0.56, y + math.sin(a - 0.42) * tip * 0.56),
               (x + math.cos(a - 0.13) * tip * 0.95, y + math.sin(a - 0.13) * tip * 0.95),
               (x + math.cos(a + 0.13) * tip * 0.95, y + math.sin(a + 0.13) * tip * 0.95),
               (x + math.cos(a + 0.42) * tip * 0.56, y + math.sin(a + 0.42) * tip * 0.56)]
        S.poly(cls, pts)
        S.line("id", pts[0][0], pts[0][1], x + math.cos(a) * tip * 0.80, y + math.sin(a) * tip * 0.80)
    sq = [(x + math.cos(phase + math.pi / 4 + math.pi / 2 * k) * base * 0.30,
           y + math.sin(phase + math.pi / 4 + math.pi / 2 * k) * base * 0.30) for k in range(4)]
    S.poly("ih", sq)
    S.circle(cls, x, y, base * 0.12)


def plates(S, x, y, base, phase=0.0, r_mul=1.75, cls="it", broken=False):
    """Regulus' own shield: four curved plates."""
    for k in range(4):
        a0 = phase + math.pi / 2 * k + 0.22
        sweep = math.pi / 2 - 0.44
        if broken:
            S.arc("id", x, y, base * r_mul, a0, sweep)
            continue
        S.arc(cls, x, y, base * r_mul, a0, sweep)
        S.arc("i", x, y, base * (r_mul - 0.13), a0 + 0.08, sweep - 0.16)


def aegis(S, x, y, half, cls="i", arm_mul=0.42):
    """Regulus granted-shield mark: four corner brackets around a square."""
    arm = half * arm_mul
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = x + half * sx, y + half * sy
            S.poly(cls, [(cx - arm * sx, cy), (cx, cy), (cx, cy - arm * sy)], close=False)


def aegis_rect(S, x, y, w, h, cls="it", arm=22):
    for cx, cy, sx, sy in ((x, y, -1, -1), (x + w, y, 1, -1), (x, y + h, -1, 1), (x + w, y + h, 1, 1)):
        S.poly(cls, [(cx - arm * sx, cy), (cx, cy), (cx, cy - arm * sy)], close=False)


def norma(S, x, y, base=20.0, phase=0.0, plates_on=True, outer=True):
    """NORMA = enlarged Regulus crest + its plates + a boss-only outer aegis ring."""
    crest(S, x, y, base, phase)
    if plates_on:
        plates(S, x, y, base, phase)
    else:
        plates(S, x, y, base, phase, broken=True)
    if outer:
        aegis(S, x, y, base * 2.35, "i", 0.30)


def regulus(S, x, y, base=20.0):
    crest(S, x, y, base)
    plates(S, x, y, base)


def flash(S, x1, y1, x2, y2):
    """Regulus' instant laser: wide faint glow + thin bright core."""
    S.line("ig", x1, y1, x2, y2)
    S.line("ih", x1, y1, x2, y2)


def shard(S, x, y, ang, s=8, cls="i"):
    """Boss bullet: a small kite plate with a flat tip, pointing along travel."""
    a = math.radians(ang)
    u = (math.cos(a), math.sin(a))
    v = (-u[1], u[0])
    pts = [(-1.0, 0.0), (-0.1, 0.55), (0.9, 0.2), (0.9, -0.2), (-0.1, -0.55)]
    S.poly(cls, [(x + (px * u[0] + py * v[0]) * s, y + (px * u[1] + py * v[1]) * s) for px, py in pts])


# ---------- shared scene primitives ----------

def window(S, x, y, w, h, kind="fake", state="mine", title=None):
    """kind fake|real ; state mine|other|lit"""
    if kind == "real":
        cls = {"mine": "g", "other": "gd", "lit": "g"}[state]
    else:
        cls = {"mine": "bt", "other": "d", "lit": "bt"}[state]
    S.rect(cls, x, y, w, h)
    S.line(cls, x, y + 24, x + w, y + 24)
    gx = x + w - 16
    S.line(cls, gx - 5, y + 7, gx + 5, y + 17)
    S.line(cls, gx + 5, y + 7, gx - 5, y + 17)
    S.rect(cls, gx - 30, y + 7, 10, 10)
    S.line(cls, gx - 54, y + 12, gx - 44, y + 12)
    if title:
        S.text("hg" if kind == "real" else "hb", x + 10, y + 18, title)


def player(S, x, y, ang=-90):
    a = math.radians(ang)
    pts = []
    for da, r in ((0, 14), (140, 11), (180, 4), (220, 11)):
        t = a + math.radians(da)
        pts.append((x + r * math.cos(t), y + r * math.sin(t)))
    S.poly("c", pts)


def mob(S, x, y, dim=False, r=10):
    cls = "d" if dim else "b"
    S.poly(cls, [(x + r * math.cos(math.radians(60 * i + 30)), y + r * math.sin(math.radians(60 * i + 30))) for i in range(6)])
    S.circle(cls, x, y, 2)


def antares(S, x, y, ang, dim=False):
    cls = "d" if dim else "b"
    a = math.radians(ang)
    ux, uy = math.cos(a), math.sin(a)
    vx, vy = -uy, ux
    S.poly(cls, [(x + ux * 22, y + uy * 22), (x + vx * 9, y + vy * 9), (x - ux * 14, y - uy * 14), (x - vx * 9, y - vy * 9)])
    S.circle(cls, x, y, 2)


def pshot(S, x, y, ang, ln=12):
    a = math.radians(ang)
    S.line("c", x, y, x + math.cos(a) * ln, y + math.sin(a) * ln)


def spark(S, x, y, cls="v", r=8):
    for i in range(6):
        t = math.radians(60 * i + 15)
        S.line(cls, x + math.cos(t) * r * 0.45, y + math.sin(t) * r * 0.45, x + math.cos(t) * r, y + math.sin(t) * r)


def arrow(S, x1, y1, x2, y2, cls="v", head=8):
    S.line(cls, x1, y1, x2, y2)
    a = math.atan2(y2 - y1, x2 - x1)
    for da in (2.6, -2.6):
        S.line(cls, x2, y2, x2 + math.cos(a + da) * head, y2 + math.sin(a + da) * head)


def screen(S, x, y, w, h, label):
    S.rect("scr", x, y, w, h)
    S.text("tag", x, y - 10, label)


def ray_end(ox, oy, a, rmax, bounds, rects):
    """Distance along the ray until it leaves bounds or meets a rect."""
    dx, dy = math.cos(math.radians(a)), math.sin(math.radians(a))
    r = 0.0
    while r < rmax:
        px, py = ox + dx * r, oy + dy * r
        bx, by, bw, bh = bounds
        if not (bx + 4 <= px <= bx + bw - 4 and by + 4 <= py <= by + bh - 4):
            return r - 1
        if any(rx <= px <= rx + rw and ry <= py <= ry + rh for rx, ry, rw, rh in rects):
            return r
        r += 1
    return rmax


SW, SH = 390, 270


# ======================================================================
# Board 1: body + window link rule + layer rules
# ======================================================================

def board_body():
    S = Svg(1800, 1130, "NORMA 보스 본체와 창 규칙 v2")
    S.text("t1", 30, 56, "NORMA v2 · 공격하는 강화 레굴루스")
    S.text("t2", 30, 92, "레굴루스의 방패 문장·곡면 판·괄호·즉발 레이저를 그대로 키우고, 지키던 힘을 공격과 창 결계로 쓴다")

    # ---- Panel A: Regulus vs NORMA
    S.rect("frame", 30, 120, 860, 560)
    S.text("h", 52, 160, "A. 레굴루스 → NORMA")
    S.text("p", 52, 188, "얼음빛 청백색 · 같은 문장과 판 · 크기와 바깥 괄호로 보스임을 구별")
    regulus(S, 160, 400, 26)
    S.text("hw", 160, 520, "레굴루스 (일반 몹)", "middle")
    S.text("s", 160, 545, "보호막을 주는 지원형 · 공격 없음", "middle")
    arrow(S, 250, 400, 330, 400, "id")
    norma(S, 590, 400, 62)
    S.text("hw", 590, 640, "NORMA (보스)", "middle")
    S.text("s", 590, 663, "같은 구성 + 바깥 괄호 4개 · 판과 문장이 무기가 된다", "middle")
    S.text("hi", 760, 250, "바깥 괄호", "start")
    S.line("id", 757, 255, 590 + 62 * 2.35 - 4, 400 - 62 * 2.35 + 4)
    S.text("hi", 760, 470, "곡면 판 4장", "start")
    S.line("id", 757, 465, 590 + 62 * 1.75 * 0.75, 400 + 62 * 1.75 * 0.66)
    S.text("hi", 400, 250, "방패 문장", "start")
    S.line("id", 470, 256, 590 - 18, 400 - 50)

    # ---- Panel B: link only at creation
    X0 = 910
    S.rect("frame", X0, 120, 860, 560)
    S.text("h", X0 + 22, 160, "B. 창과의 연결은 생성·명령하는 순간에만")
    S.text("p", X0 + 22, 188, "레굴루스가 보호막을 줄 때의 즉발 레이저와 괄호 조임을 그대로 쓴다")
    steps = [("① 레이저 번쩍 (0.25초)", 0), ("② 괄호가 1.6배에서 조여듦", 1), ("③ 연결 사라짐 · 괄호만 남음", 2)]
    for i, (label, st) in enumerate(steps):
        sx, sy = X0 + 25 + i * 280, 240
        S.rect("scr", sx, sy, 260, 300)
        S.text("tag", sx, sy - 10, label)
        bx, by = sx + 50, sy + 60
        norma(S, bx, by, 13, outer=True)
        wx, wy, ww, wh = sx + 95, sy + 130, 140, 120
        window(S, wx, wy, ww, wh, "real", "other" if st < 2 else "mine", "실제 창")
        if st == 0:
            for cx, cy in ((wx, wy), (wx + ww, wy), (wx, wy + wh), (wx + ww, wy + wh)):
                flash(S, bx + 10, by + 10, cx, cy)
        if st == 1:
            for cx, cy in ((wx, wy), (wx + ww, wy), (wx, wy + wh), (wx + ww, wy + wh)):
                S.line("ig", bx + 10, by + 10, cx, cy)
            aegis_rect(S, wx - 22, wy - 18, ww + 44, wh + 36, "id", 22)
            aegis_rect(S, wx, wy, ww, wh, "it", 22)
            for cx, cy, dx, dy in ((wx - 22, wy - 18, 1, 1), (wx + ww + 22, wy + wh + 18, -1, -1)):
                arrow(S, cx, cy, cx + dx * 16, cy + dy * 14, "i", 5)
            spark(S, wx + ww + 8, wy + wh / 2, "i", 9)
        if st == 2:
            aegis_rect(S, wx, wy, ww, wh, "it", 22)
    S.lines("p", X0 + 25, 590, ["창 위에는 레굴루스가 아군에게 씌우는 것과 같은 모서리 괄호만 남는다.",
                                "보스와 창 사이에 계속 이어진 선은 없다. 창을 닫거나 순서를 바꾸거나",
                                "반사를 걸 때만 같은 레이저가 다시 한 번 번쩍인다."], 26)

    # ---- Panel C: layer rules (compact)
    S.rect("frame", 30, 700, 1740, 400)
    S.text("h", 52, 740, "C. 창 = 별도 공간 (확정 규칙)")
    S.text("p", 52, 768, "창 안과 밖은 공격이 서로 닿지 않는다 · 경계를 넘는 것은 몹과 플레이어의 이동뿐")

    bx, by = 60, 810
    S.text("hw", bx, by, "① 투사체·레이저는 경계에서 소멸")
    window(S, bx + 230, by + 30, 240, 170, "fake", "other", "창")
    player(S, bx + 40, by + 120, 0)
    pshot(S, bx + 90, by + 120, 0)
    pshot(S, bx + 160, by + 120, 0)
    spark(S, bx + 230, by + 120, "c")
    mob(S, bx + 370, by + 120, dim=True)
    for yy in (by + 80, by + 165):
        shard(S, bx + 290, yy, 180, 8)
        spark(S, bx + 232, yy, "v")
    S.lines("s", bx, by + 235, ["전기장·폭발도 경계에서 잘리고, 접촉 피해도 같은 공간끼리만 생긴다"], 22)

    bx2 = 640
    S.text("hw", bx2, by, "② 몹과 플레이어는 걸어서 넘나든다")
    window(S, bx2 + 200, by + 30, 240, 170, "fake", "mine", "내가 있는 창")
    player(S, bx2 + 380, by + 150)
    mob(S, bx2 + 120, by + 90, dim=True)
    arrow(S, bx2 + 135, by + 90, bx2 + 190, by + 90, "d")
    mob(S, bx2 + 250, by + 90)
    antares(S, bx2 + 100, by + 165, 0, dim=True)
    arrow(S, bx2 + 130, by + 165, bx2 + 280, by + 165, "b")
    antares(S, bx2 + 310, by + 165, 0)
    S.lines("s", bx2, by + 235, ["넘는 순간 소속이 바뀐다 · 경계에 걸치면 중심점 쪽 소속",
                                 "안타레스 돌진도 예외 없음: 창 경계에서 기절하지 않고 통과"], 22)

    bx3 = 1220
    S.text("hw", bx3, by, "③ 겹치면 앞 창이 겹친 영역을 가진다")
    ax_, ay_ = bx3 + 20, by + 40
    window(S, ax_, ay_, 240, 120, "fake", "mine", "앞 창 A")
    S.poly("d", [(ax_ + 240, ay_ + 55), (ax_ + 380, ay_ + 55), (ax_ + 380, ay_ + 175), (ax_ + 160, ay_ + 175), (ax_ + 160, ay_ + 120)], close=False)
    S.line("d", ax_ + 240, ay_ + 79, ax_ + 380, ay_ + 79)
    S.text("hb", ax_ + 255, ay_ + 73, "뒤 창 B")
    S.lines("s", bx3, by + 235, ["뒤 창 테두리는 겹친 영역에서 그리지 않는다",
                                 "내 창은 밝게, 다른 창과 다른 공간의 몹은 어둡고 얇게"], 22)

    S.save("Norma_Body_Rules_v2.svg")


# ======================================================================
# Board 2: window patterns 1–4 (boss = crest, links only on creation)
# ======================================================================

def pattern_panel(S, x, y, num, name, sub, prep, act, counter, w=860):
    S.rect("frame", x, y, w, 490)
    S.text("h", x + 22, y + 40, f"{num}. {name}")
    S.text("p", x + 22, y + 68, sub)
    s1 = (x + 25, y + 110)
    s2 = (x + 445, y + 110)
    screen(S, *s1, SW, SH, "예고")
    screen(S, *s2, SW, SH, "실행")
    prep(S, *s1)
    act(S, *s2)
    S.lines("p", x + 25, y + 420, counter, 26)


def p1_prep(S, x, y):
    wx, wy, ww, wh = x + 200, y + 80, 160, 140
    window(S, wx, wy, ww, wh, "real", "other", "실제 창")
    bx, by = x + 80, y + 75
    norma(S, bx, by, 12)
    for cx, cy in ((wx, wy), (wx + ww, wy), (wx, wy + wh), (wx + ww, wy + wh)):
        flash(S, bx + 8, by + 8, cx, cy)
    aegis_rect(S, wx, wy, ww, wh, "it", 18)
    S.text("hi", x + 200, y + 250, "생성 순간 레이저")
    player(S, x + 70, y + 215)
    mob(S, x + 130, y + 240)


def p1_act(S, x, y):
    wx, wy, ww, wh = x + 200, y + 80, 160, 140
    window(S, wx, wy, ww, wh, "real", "lit", "실제 창")
    aegis_rect(S, wx, wy, ww, wh, "it", 18)
    cx, cy = wx + ww / 2, wy + 82
    norma(S, cx, cy, 11, outer=False)
    for i in range(4):
        a = 90 * i + 20
        for r in (38, 54):
            shard(S, cx + math.cos(math.radians(a + r * 0.6)) * r, cy + math.sin(math.radians(a + r * 0.6)) * r, a + r * 0.6, 5, "id")
    arrow(S, x + 100, y + 60, wx - 10, wy + 40, "id")
    S.text("s", x + 20, y + 50, "보스가 창 안으로")
    player(S, x + 70, y + 170, 0)
    pshot(S, x + 110, y + 170, 0)
    pshot(S, x + 160, y + 170, 0)
    spark(S, wx, y + 170, "c")
    mob(S, x + 110, y + 235)
    S.text("s", x + 10, y + 262, "밖: 탄이 경계에서 소멸 · 안: 보스 연사")


def p2_prep(S, x, y):
    bx, by = x + 195, y + 60
    norma(S, bx, by, 11)
    rects = ((x + 30, y + 150, 120, 95), (x + 245, y + 160, 120, 90))
    for i in range(9):
        a = 30 + i * 15
        e = ray_end(bx, by, a, 400, (x, y, SW, SH), rects)
        S.line("v", bx + math.cos(math.radians(a)) * 38, by + math.sin(math.radians(a)) * 38,
               bx + math.cos(math.radians(a)) * e, by + math.sin(math.radians(a)) * e)
    for rx, ry, rw, rh in rects:
        window(S, rx, ry, rw, rh, "fake", "other")
        flash(S, bx, by, rx + rw / 2, ry)
        aegis_rect(S, rx, ry, rw, rh, "i", 14)
    S.text("hi", x + 12, y + 30, "창 생성 순간만 레이저")
    player(S, x + 190, y + 215)


def p2_act(S, x, y):
    bx, by = x + 195, y + 60
    norma(S, bx, by, 11)
    rects = ((x + 30, y + 150, 120, 95), (x + 245, y + 160, 120, 90))
    window(S, *rects[0], "fake", "mine")
    window(S, *rects[1], "fake", "other")
    for rx, ry, rw, rh in rects:
        aegis_rect(S, rx, ry, rw, rh, "i", 14)
    for i in range(9):
        a = 30 + i * 15
        for k in (85, 130, 175, 220):
            px, py = bx + math.cos(math.radians(a)) * k, by + math.sin(math.radians(a)) * k
            inside = any(rx <= px <= rx + rw and ry <= py <= ry + rh for rx, ry, rw, rh in rects)
            if not inside and y + 8 < py < y + SH - 8 and x + 8 < px < x + SW - 8:
                shard(S, px, py, a, 6)
    spark(S, rects[0][0] + rects[0][2], rects[0][1] + 30)
    spark(S, rects[1][0], rects[1][1] + 25)
    player(S, x + 80, y + 215)
    mob(S, x + 12, y + 195, dim=True)
    arrow(S, x + 22, y + 205, x + 45, y + 222, "b")
    mob(S, x + 55, y + 232)
    S.text("s", x + 8, y + 262, "몹은 걸어서 침입")


def p3_prep(S, x, y):
    wx, wy, ww, wh = x + 90, y + 60, 230, 170
    window(S, wx, wy, ww, wh, "fake", "mine")
    aegis_rect(S, wx, wy, ww, wh, "i", 18)
    bx, by = x + 40, y + 45
    norma(S, bx, by, 9)
    flash(S, bx + 6, by + 6, wx + ww - 16, wy + 12)
    S.circle("vt", wx + ww - 16, wy + 12, 11)
    S.line("vt", wx + 8, wy + 32, wx + 120, wy + 32)
    S.line("d", wx + 120, wy + 32, wx + ww - 8, wy + 32)
    S.text("hv", wx + 8, wy + 52, "카운트다운")
    player(S, wx + 120, wy + 120)
    mob(S, wx + 60, wy + 130)
    mob(S, wx + 180, wy + 100)


def p3_act(S, x, y):
    wx, wy, ww, wh = x + 90, y + 60, 230, 170
    window(S, wx, wy, ww, wh, "fake", "other")
    for k in (25, 50):
        S.rect("vt" if k == 25 else "v", wx + k, wy + k * 0.7, ww - 2 * k, wh - 1.4 * k)
    cx, cy = wx + ww / 2, wy + wh / 2 + 5
    for i in range(12):
        t = math.radians(30 * i)
        S.line("wt", cx + math.cos(t) * 10, cy + math.sin(t) * 10, cx + math.cos(t) * 30, cy + math.sin(t) * 30)
    for mx, my in ((wx + 60, wy + 130), (wx + 180, wy + 100)):
        mob(S, mx, my, dim=True)
        S.line("r", mx - 8, my - 8, mx + 8, my + 8)
        S.line("r", mx + 8, my - 8, mx - 8, my + 8)
    arrow(S, wx + 200, wy + 150, wx + ww + 30, wy + wh + 15, "c")
    player(S, wx + ww + 45, wy + wh + 22)
    S.text("s", x + 10, y + 262, "안에 남은 몹도 함께 피해")


def p4_prep(S, x, y):
    ax_, ay_, aw, ah = x + 40, y + 50, 200, 150
    bx_, by_, bw, bh = x + 170, y + 90, 190, 140
    S.poly("d", [(ax_ + aw, by_), (bx_ + bw, by_), (bx_ + bw, by_ + bh), (bx_, by_ + bh), (bx_, ay_ + ah)], close=False)
    S.line("vt", ax_ + aw, by_ + 24, bx_ + bw, by_ + 24)
    window(S, ax_, ay_, aw, ah, "fake", "mine", "A (앞)")
    nx, ny = x + 345, y + 30
    norma(S, nx, ny, 8, outer=False)
    flash(S, nx, ny + 8, bx_ + bw - 30, by_ + 12)
    S.text("hv", ax_ + aw + 60, by_ + 48, "B 밝아짐", "middle")
    player(S, x + 205, y + 170)
    for i in range(3):
        shard(S, x + 300 + i * 18, y + 180, 180, 6, "id")
    spark(S, ax_ + aw, y + 180, "d")


def p4_act(S, x, y):
    ax_, ay_, aw, ah = x + 40, y + 50, 200, 150
    bx_, by_, bw, bh = x + 170, y + 90, 190, 140
    S.poly("d", [(bx_, ay_ + ah), (ax_, ay_ + ah), (ax_, ay_), (ax_ + aw, ay_), (ax_ + aw, by_)], close=False)
    S.line("d", ax_, ay_ + 24, ax_ + aw, ay_ + 24)
    window(S, bx_, by_, bw, bh, "fake", "lit", "B (앞)")
    S.text("hb", ax_ + 8, ay_ + 18, "A")
    for i in range(3):
        shard(S, x + 250 + i * 18, y + 180, 180, 6)
    player(S, x + 205, y + 170)
    arrow(S, x + 200, y + 160, x + 120, y + 120, "c")
    S.text("s", x + 10, y + 262, "겹친 영역이 B 공간 → 탄이 닿음")


def board_window_patterns():
    S = Svg(1800, 1170, "NORMA 창 패턴 1–4 v2")
    S.text("t1", 30, 56, "NORMA v2 · 창 패턴 1–4")
    S.text("t2", 30, 92, "회색 = 실제 창 · 청색 = 가짜 창 · 얼음빛 = 보스와 괄호 · 연결 레이저는 생성·명령 순간에만 · 보라 = 예고")
    pattern_panel(S, 30, 130, 1, "창 너머의 보스", "실제 창에 괄호를 씌우고 보스가 그 공간으로 들어간다",
                  p1_prep, p1_act, ["대응: 들어가서 보스를 칠지,", "밖에 남아 일반 몹을 정리할지 선택한다"])
    pattern_panel(S, 910, 130, 2, "피난 창", "화면 전체 탄막 예고와 함께 가짜 창 2~3개가 생성된다",
                  p2_prep, p2_act, ["대응: 창으로 피신한다. 탄막은 못 들어오지만", "몹은 걸어 들어오므로 같은 공간에서 처리"])
    pattern_panel(S, 30, 650, 3, "창 닫기", "내가 있는 창의 × 노드에 레이저가 꽂히고 카운트다운이 줄어든다",
                  p3_prep, p3_act, ["대응: 카운트가 끝나기 전에 빠져나간다", "안에 남겨둔 몹은 함께 정리된다"])
    pattern_panel(S, 910, 650, 4, "창 순서 바꾸기", "레이저가 뒤 창 제목줄을 치고, 그 창이 밝아진다",
                  p4_prep, p4_act, ["대응: 겹친 영역에서 빠지거나,", "앞으로 올라올 창 쪽을 미리 정리한다"])
    S.save("Norma_Window_Patterns_v2.svg")


# ======================================================================
# Board 3: direct attacks 5–8 (armed Regulus) + reflect enhancement
# ======================================================================

def a5_prep(S, x, y):
    bx, by = x + 80, y + 80
    norma(S, bx, by, 13)
    px, py = x + 310, y + 200
    S.line("v", bx + 20, by + 12, px, py)
    aegis(S, px, py, 22, "vt", 0.38)
    player(S, px, py, 200)
    S.text("hv", x + 170, y + 120, "조준선 + 괄호 고정")
    S.text("s", x + 10, y + 262, "레굴루스 보호막 레이저의 공격판 · 조준 0.8초")


def a5_act(S, x, y):
    bx, by = x + 80, y + 80
    norma(S, bx, by, 13)
    # earlier shots fading
    for i, (tx, ty) in enumerate(((x + 330, y + 110), (x + 160, y + 225))):
        S.line("id", bx, by, tx, ty)
        S.text("s", tx - 6, ty + (16 if i else -6), str(i + 1), "middle")
    wx, wy, ww, wh = x + 250, y + 155, 120, 90
    window(S, wx, wy, ww, wh, "fake", "mine")
    tx, ty = wx, wy + 45
    flash(S, bx, by, tx, ty)
    S.circle("i", tx, ty, 12)
    spark(S, tx, ty, "i", 10)
    S.text("hi", tx - 30, ty - 22, "3", "middle")
    player(S, wx + 70, wy + 60)
    S.text("s", x + 10, y + 262, "즉발 3연발 · 매번 다시 조준 · 창이 레이저를 막음")


def a6_prep(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    norma(S, cx, cy, 18)
    for k in range(4):
        a = math.pi / 2 * k
        S.line("v", cx + math.cos(a) * 30, cy + math.sin(a) * 30, cx + math.cos(a) * 120, cy + math.sin(a) * 120)
    S.arc("vt", cx, cy, 120, -0.9, 0.7)
    a = -0.2
    ex, ey = cx + math.cos(a) * 120, cy + math.sin(a) * 120
    S.line("vt", ex, ey, ex - 8, ey - 6)
    S.line("vt", ex, ey, ex + 2, ey - 10)
    S.text("hv", x + 12, y + 30, "문장 판 정렬 + 회전 방향")
    player(S, x + 340, y + 230)


def a6_act(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    norma(S, cx, cy, 18, phase=0.3)
    for k in range(4):
        for j in range(7):
            r = 48 + j * 17
            a = math.pi / 2 * k + 0.3 - j * 0.16
            px, py = cx + math.cos(a) * r, cy + math.sin(a) * r
            if x + 8 < px < x + SW - 8 and y + 8 < py < y + SH - 8:
                shard(S, px, py, math.degrees(a), 7)
    player(S, x + 340, y + 230)
    S.text("s", x + 10, y + 262, "판 모양 탄 4줄기가 돌며 나선을 그린다")


def a7_prep(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    crest(S, cx, cy, 18)
    plates(S, cx, cy, 18, r_mul=2.4)
    for k in range(4):
        a = math.pi / 4 + math.pi / 2 * k
        arrow(S, cx + math.cos(a) * 46, cy + math.sin(a) * 46, cx + math.cos(a) * 66, cy + math.sin(a) * 66, "i", 5)
    S.circle("v", cx, cy, 112)
    S.text("hv", x + 12, y + 258, "판이 벌어짐 + 궤도 예고")
    player(S, x + 345, y + 235)


def a7_act(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    crest(S, cx, cy, 18)
    plates(S, cx, cy, 18, broken=True)
    S.circle("id", cx, cy, 112)
    for k in range(4):
        a0 = math.pi / 2 * k + 0.5
        S.arc("wt", cx, cy, 112, a0, 0.55)
        S.arc("i", cx, cy, 104, a0 + 0.05, 0.45)
        S.arc("id", cx, cy, 112, a0 - 0.35, 0.3)
    player(S, cx + 30, cy + 40, -120)
    S.text("hc", cx + 48, cy + 50, "파고들기")
    S.text("s", x + 10, y + 262, "판 4장이 궤도를 돌고 돌아옴 · 그동안 보스 보호막 꺼짐")


def a8_prep(S, x, y):
    bx, by = x + 70, y + 70
    norma(S, bx, by, 12)
    px, py = x + 250, y + 170
    aegis(S, px, py, 62, "vt", 0.32)
    aegis(S, px, py, 38, "v", 0.4)
    for sx in (-1, 1):
        for sy in (-1, 1):
            arrow(S, px + sx * 58, py + sy * 58, px + sx * 44, py + sy * 44, "v", 5)
    player(S, px, py)
    S.text("hv", x + 170, y + 92, "플레이어 둘레 괄호 1.6배")


def a8_act(S, x, y):
    bx, by = x + 70, y + 70
    norma(S, bx, by, 12)
    px, py = x + 250, y + 170
    aegis(S, px, py, 22, "wt", 0.5)
    for i in range(8):
        t = math.radians(45 * i)
        S.line("w", px + math.cos(t) * 8, py + math.sin(t) * 8, px + math.cos(t) * 18, py + math.sin(t) * 18)
    arrow(S, px + 10, py + 30, px + 85, py + 75, "c")
    player(S, px + 95, py + 82, 30)
    S.text("s", x + 10, y + 262, "괄호가 조여 닫히는 순간 그 안에 있으면 피해")


def board_direct():
    S = Svg(1800, 1300, "NORMA 직접 공격 5–8 v2")
    S.text("t1", 30, 56, "NORMA v2 · 직접 공격 5–8")
    S.text("t2", 30, 92, "레굴루스가 지키는 데 쓰던 레이저·문장·곡면 판·괄호를 공격판으로 바꾼다 · 모든 공격은 창 경계에서 막힌다")
    pattern_panel(S, 30, 130, 5, "방패 레이저", "보호막을 주던 즉발 레이저를 플레이어에게 쏜다",
                  a5_prep, a5_act, ["대응: 조준선 옆으로 비키거나 창에 들어가 막는다.", "예고는 나를 향한 한 줄 + 내 둘레 작은 괄호"])
    pattern_panel(S, 910, 130, 6, "문장 연사", "방패 문장의 판 4장 방향으로 판 모양 탄을 돌려 뿜는다",
                  a6_prep, a6_act, ["대응: 회전 방향을 보고 줄기 사이 빈 각도로 따라 돈다.", "예고는 십자 4줄 + 회전 화살표"])
    pattern_panel(S, 30, 650, 7, "곡면 판 투척", "자기 보호막인 곡면 판 4장을 원궤도로 던진다",
                  a7_prep, a7_act, ["대응: 궤도 선을 넘을 때 판 사이 틈을 탄다.", "판이 나간 동안 보스 보호막이 꺼져 있어 안쪽이 공격 기회"])
    pattern_panel(S, 910, 650, 8, "괄호 포박", "아군에게 씌우던 괄호를 플레이어 둘레에 씌우고 조인다",
                  a8_prep, a8_act, ["대응: 괄호가 다 조여지기 전에 사각형 밖으로 빠져나간다.", "예고는 내 둘레 큰 괄호 + 안쪽으로 향하는 화살표"])

    y0 = 1170
    S.text("hw", 50, y0, "보스 탄과 경계")
    for i in range(4):
        shard(S, 80 + i * 34, y0 + 50, 0, 10)
    S.text("p", 240, y0 + 56, "판 모양 탄 · 끝이 평평하게 잘림 (가시 아님)")
    S.line("bt", 760, y0 + 20, 760, y0 + 90)
    shard(S, 720, y0 + 55, 0, 10)
    spark(S, 762, y0 + 55)
    S.text("p", 790, y0 + 60, "보통 창: 흩어져 소멸")
    S.line("bt", 1150, y0 + 20, 1150, y0 + 90)
    S.line("bt", 1158, y0 + 20, 1158, y0 + 90)
    S.poly("i", [(1090, y0 + 30), (1148, y0 + 55), (1090, y0 + 80)], close=False)
    shard(S, 1090, y0 + 80, 157, 10)
    S.text("p", 1180, y0 + 60, "두 겹 테두리 창: 반사 (2페이즈 강화)")
    S.text("s", 50, y0 + 110, "예고는 보라색 얇은 실선, 실제 위험은 흰색·얼음빛 굵은 실선. 색을 빼도 굵기와 도형으로 구별된다.")
    S.save("Norma_Direct_Attacks_v2.svg")


board_body()
board_window_patterns()
board_direct()
print("ok")
