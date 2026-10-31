"""NORMA boss concept boards v4 (SVG): rules after A1–A10 and patterns 1·2·3·6.

Lines only, no fills, no dashed lines. Window links show only at the moment the
boss creates or commands a window (Regulus' instant shield laser), never persist.
Usage: python Norma_Boards_v4.py <out_dir>
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
.hr{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#ff8a9a}
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
    """Placeholder body: a plain square with a centre point, until patterns settle."""
    h = base * 1.3
    S.rect("it" if plates_on else "id", x - h, y - h, 2 * h, 2 * h)
    S.circle("i", x, y, max(2.0, base * 0.12))


def regulus(S, x, y, base=20.0):
    crest(S, x, y, base)
    plates(S, x, y, base)


def flash(S, x1, y1, x2, y2):
    """Regulus' instant laser: wide faint glow + thin bright core."""
    S.line("ig", x1, y1, x2, y2)
    S.line("ih", x1, y1, x2, y2)


def shard(S, x, y, ang, s=8, cls="i"):
    """Boss bullet placeholder: a small square turned to its travel direction."""
    a = math.radians(ang)
    u = (math.cos(a), math.sin(a))
    v = (-u[1], u[0])
    pts = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]
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
    S = Svg(1800, 1130, "NORMA 보스 본체와 창 규칙 v3")
    S.text("t1", 30, 56, "NORMA v3 · 임시 본체와 창 규칙")
    S.text("t2", 30, 92, "본체는 네모로 틀만 잡고 패턴을 먼저 구체화한다 · 공격하는 강화 레굴루스 방향은 유지")

    # ---- Panel A: placeholder body
    S.rect("frame", 30, 120, 860, 560)
    S.text("h", 52, 160, "A. 임시 본체 — 네모")
    S.text("p", 52, 188, "외형은 패턴을 구체화한 뒤 정한다 · 지금은 크기·중심·판정 기준만 잡는다")
    norma(S, 330, 420, 110)
    S.text("hi", 330, 600, "보스 본체 (임시)", "middle")
    S.text("s", 330, 625, "정사각 테두리 + 중심점", "middle")
    S.lines("p", 560, 300, ["유지하는 방향", "· 레굴루스 계열: 얼음빛 청백색", "· 즉발 레이저와 모서리 괄호로", "  창을 만들고 명령한다", "",
                            "임시로 두는 것", "· 본체 외형 (네모)", "· 보스 탄 모양 (작은 사각형)", "· 7번에서 던지는 것 = 네모의 네 변"], 28)
    # ---- Panel B: link only at creation
    X0 = 910
    S.rect("frame", X0, 120, 860, 560)
    S.text("h", X0 + 22, 160, "B. 창과의 연결은 생성·명령하는 순간에만")
    S.text("p", X0 + 22, 188, "레굴루스가 보호막을 줄 때의 즉발 레이저와 괄호 조임 연출을 쓴다")
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

    S.save("Norma_Body_Rules_v3.svg")


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
    S = Svg(1800, 1170, "NORMA 창 패턴 1–4 v3")
    S.text("t1", 30, 56, "NORMA v3 · 창 패턴 1–4 (임시 본체)")
    S.text("t2", 30, 92, "회색 = 실제 창 · 청색 = 가짜 창 · 얼음빛 = 보스와 괄호 · 연결 레이저는 생성·명령 순간에만 · 보라 = 예고")
    pattern_panel(S, 30, 130, 1, "창 너머의 보스", "실제 창에 괄호를 씌우고 보스가 그 공간으로 들어간다",
                  p1_prep, p1_act, ["대응: 들어가서 보스를 칠지,", "밖에 남아 일반 몹을 정리할지 선택한다"])
    pattern_panel(S, 910, 130, 2, "피난 창", "화면 전체 탄막 예고와 함께 가짜 창 2~3개가 생성된다",
                  p2_prep, p2_act, ["대응: 창으로 피신한다. 탄막은 못 들어오지만", "몹은 걸어 들어오므로 같은 공간에서 처리"])
    pattern_panel(S, 30, 650, 3, "창 닫기", "내가 있는 창의 × 노드에 레이저가 꽂히고 카운트다운이 줄어든다",
                  p3_prep, p3_act, ["대응: 카운트가 끝나기 전에 빠져나간다", "안에 남겨둔 몹은 함께 정리된다"])
    pattern_panel(S, 910, 650, 4, "창 순서 바꾸기", "레이저가 뒤 창 제목줄을 치고, 그 창이 밝아진다",
                  p4_prep, p4_act, ["대응: 겹친 영역에서 빠지거나,", "앞으로 올라올 창 쪽을 미리 정리한다"])
    S.save("Norma_Window_Patterns_v3.svg")


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
    S.text("s", x + 10, y + 262, "생성 레이저의 공격판 · 조준 후 즉발")


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
    S.text("hv", x + 12, y + 30, "네 변 정렬 + 회전 방향")
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
    S.text("s", x + 10, y + 262, "사각 탄 4줄기가 돌며 나선을 그린다")


def a7_prep(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    h, g = 23.4, 8
    for (x1, y1, x2, y2) in ((-h, -h - g, h, -h - g), (h + g, -h, h + g, h), (h, h + g, -h, h + g), (-h - g, h, -h - g, -h)):
        S.line("it", cx + x1, cy + y1, cx + x2, cy + y2)
    S.circle("i", cx, cy, 2.5)
    for k in range(4):
        a = math.pi / 2 * k - math.pi / 2
        arrow(S, cx + math.cos(a) * 40, cy + math.sin(a) * 40, cx + math.cos(a) * 62, cy + math.sin(a) * 62, "i", 5)
    S.circle("v", cx, cy, 112)
    S.text("hv", x + 12, y + 258, "네 변이 벌어짐 + 궤도 예고")
    player(S, x + 345, y + 235)


def a7_act(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    norma(S, cx, cy, 18, plates_on=False)
    S.circle("id", cx, cy, 112)
    for k in range(4):
        a = math.pi / 2 * k + 0.75
        px, py = cx + math.cos(a) * 112, cy + math.sin(a) * 112
        tx, ty = -math.sin(a), math.cos(a)
        S.line("wt", px - tx * 30, py - ty * 30, px + tx * 30, py + ty * 30)
        S.arc("id", cx, cy, 112, a - 0.62, 0.3)
    player(S, cx + 30, cy + 40, -120)
    S.text("hc", cx + 48, cy + 50, "파고들기")
    S.text("s", x + 10, y + 262, "네 변이 궤도를 돌고 돌아옴 · 그동안 본체 방어 꺼짐")


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
    S = Svg(1800, 1300, "NORMA 직접 공격 5–8 v3")
    S.text("t1", 30, 56, "NORMA v3 · 직접 공격 5–8 (임시 본체)")
    S.text("t2", 30, 92, "레이저·연사·투척·괄호 · 모든 공격은 창 경계에서 막힌다 · 본체와 탄 모양은 임시")
    pattern_panel(S, 30, 130, 5, "방패 레이저", "창을 만들 때 쓰는 즉발 레이저를 플레이어에게 쏜다",
                  a5_prep, a5_act, ["대응: 조준선 옆으로 비키거나 창에 들어가 막는다.", "예고는 나를 향한 한 줄 + 내 둘레 작은 괄호"])
    pattern_panel(S, 910, 130, 6, "십자 연사", "본체 네 방향으로 탄 4줄기를 돌려 뿜는다",
                  a6_prep, a6_act, ["대응: 회전 방향을 보고 줄기 사이 빈 각도로 따라 돈다.", "예고는 십자 4줄 + 회전 화살표"])
    pattern_panel(S, 30, 650, 7, "변 투척", "본체의 네 변을 떼어 원궤도로 던진다",
                  a7_prep, a7_act, ["대응: 궤도 선을 넘을 때 변 사이 틈을 탄다.", "변이 나간 동안 본체 방어가 꺼져 있어 안쪽이 공격 기회"])
    pattern_panel(S, 910, 650, 8, "괄호 포박", "창에 씌우는 모서리 괄호를 플레이어 둘레에 씌우고 조인다",
                  a8_prep, a8_act, ["대응: 괄호가 다 조여지기 전에 사각형 밖으로 빠져나간다.", "예고는 내 둘레 큰 괄호 + 안쪽으로 향하는 화살표"])

    y0 = 1170
    S.text("hw", 50, y0, "보스 탄과 경계")
    for i in range(4):
        shard(S, 80 + i * 34, y0 + 50, 0, 10)
    S.text("p", 240, y0 + 56, "사각 탄 (임시 모양)")
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
    S.save("Norma_Direct_Attacks_v3.svg")



# ======================================================================
# v4 (2026-10-22): rules after A1–A10, patterns 1·2·3·6, 4 on hold
# ======================================================================

def xmark(S, x, y, r=7, cls="r"):
    S.line(cls, x - r, y - r, x + r, y + r)
    S.line(cls, x + r, y - r, x - r, y + r)


def check(S, x, y, cls="c"):
    S.poly(cls, [(x - 7, y), (x - 2, y + 6), (x + 8, y - 7)], close=False)


def os_window(S, x, y, w, h, state="mine", title=None, cls_over=None):
    """Real OS window as the game spawns it: opaque title bar band, see-through client."""
    window(S, x, y, w, h, "fake", state, title)
    cls = cls_over or ("bt" if state in ("mine", "lit") else "d")
    S.line(cls, x, y + 20, x + w - 70, y + 20)   # second line reads as a solid band


def rules_panel(S, x, y, title, sub):
    S.rect("frame", x, y, 570, 455)
    S.text("h", x + 20, y + 40, title)
    S.lines("p", x + 20, y + 68, sub, 22)
    screen(S, x + 20, y + 130, 530, 300, "")


def board_rules_v4():
    S = Svg(1800, 1080, "NORMA 창 규칙 v4")
    S.text("t1", 30, 56, "NORMA v4 · 창 규칙 정리")
    S.text("t2", 30, 92, "어그로와 이동은 경계를 넘는다 · 눈에 보이는 공격과 효과는 못 넘는다 · 창은 진짜 OS 창")

    # 1. what crosses
    x, y = 30, 120
    rules_panel(S, x, y, "1. 넘는 것: 이동 · 어그로", ["몹은 창 안의 플레이어에게도 끌려 걸어서 쫓아온다", "넘는 순간 그 공간 소속이 된다"])
    sx, sy = x + 20, y + 130
    os_window(S, sx + 250, sy + 60, 250, 200, "mine", "창")
    player(S, sx + 400, sy + 180)
    for i, (mx, my) in enumerate(((sx + 60, sy + 120), (sx + 90, sy + 230))):
        mob(S, mx, my, dim=True)
        arrow(S, mx + 15, my, sx + 240, my - (10 if i == 0 else 30), "b")
    mob(S, sx + 290, sy + 175)
    S.line("v", sx + 300, sy + 175, sx + 385, sy + 180)
    S.text("hv", sx + 300, sy + 165, "어그로")
    check(S, sx + 30, sy + 30)
    S.text("hc", sx + 45, sy + 36, "이동 · 어그로 통과")

    # 2. what is blocked
    x = 615
    rules_panel(S, x, y, "2. 못 넘는 것: 투사체 · 레이저 · 효과", ["원거리 몹은 쏘기는 하지만 탄은 경계에서 흩어진다", "레굴루스는 경계 너머 아군에게 보호막을 시도하지 않는다"])
    sx, sy = x + 20, y + 130
    os_window(S, sx + 250, sy + 60, 250, 200, "mine", "창")
    player(S, sx + 400, sy + 200)
    mob(S, sx + 280, sy + 120)
    antares(S, sx + 70, sy + 150, 0, dim=False)
    for k, yy in enumerate((sy + 140, sy + 160)):
        for i in range(3):
            shard(S, sx + 110 + i * 40 + k * 15, yy, 0, 6)
        spark(S, sx + 250, yy)
    S.text("s", sx + 40, sy + 120, "원거리 몹")
    regulus(S, sx + 80, sy + 250, 10)
    S.text("s", sx + 105, sy + 255, "레굴루스: 시도 안 함")
    xmark(S, sx + 30, sy + 30)
    S.text("hr", sx + 45, sy + 36, "탄 · 레이저 · 끌어당김 · 감속 · 보호막")

    # 3. durability + self-destruct
    x = 1200
    rules_panel(S, x, y, "3. 창 내구도와 자폭", ["막아 낸 탄만큼 내구도가 깎인다", "다 떨어지면 자폭 · 효과는 미정"])
    sx, sy = x + 20, y + 130
    labels = ["가득", "깎임", "자폭"]
    for i in range(3):
        wx = sx + 20 + i * 172
        if i == 0:
            os_window(S, wx, sy + 70, 150, 130, "mine")
            aegis_rect(S, wx, sy + 70, 150, 130, "it", 18)
        elif i == 1:
            os_window(S, wx, sy + 70, 150, 130, "other")
            aegis_rect(S, wx, sy + 70, 150, 130, "id", 12)
            for k in range(3):
                spark(S, wx, sy + 110 + k * 30)
        else:
            cx, cy = wx + 75, sy + 135
            for k in range(12):
                t = math.radians(30 * k)
                S.line("wt", cx + math.cos(t) * 20, cy + math.sin(t) * 20, cx + math.cos(t) * 60, cy + math.sin(t) * 60)
            S.rect("id", wx + 20, sy + 85, 110, 100)
        S.text("hw", wx + 75, sy + 240, labels[i], "middle")
    S.text("s", sx + 20, sy + 280, "내구도는 괄호·테두리 선의 밝기와 굵기로만 보인다")

    # 4. resize
    y2 = 595
    x = 30
    rules_panel(S, x, y2, "4. 창 크기 변경", ["줄이면 단단해진다 (내구도 상승)", "대신 닫기 · 자폭 때 압축된 강한 피해"])
    sx, sy = x + 20, y2 + 130
    os_window(S, sx + 30, sy + 50, 220, 170, "mine")
    aegis_rect(S, sx + 30, sy + 50, 220, 170, "i", 16)
    arrow(S, sx + 260, sy + 135, sx + 320, sy + 135, "v")
    os_window(S, sx + 335, sy + 85, 130, 100, "mine")
    aegis_rect(S, sx + 335, sy + 85, 130, 100, "wt", 16)
    S.text("hw", sx + 140, sy + 250, "보통", "middle")
    S.text("hw", sx + 400, sy + 215, "작게 = 단단", "middle")
    cx, cy = sx + 400, sy + 260
    for k in range(8):
        t = math.radians(45 * k)
        S.line("wt", cx + math.cos(t) * 6, cy + math.sin(t) * 6, cx + math.cos(t) * 22, cy + math.sin(t) * 22)
    S.text("s", sx + 430, sy + 265, "압축 피해")

    # 5. close == minimize
    x = 615
    rules_panel(S, x, y2, "5. 닫기 = 최소화", ["둘 다 그 창의 공간이 사라진다", "최소화한 창은 다시 띄우면 돌아올 수 있다"])
    sx, sy = x + 20, y2 + 130
    os_window(S, sx + 30, sy + 60, 190, 150, "mine")
    S.circle("vt", sx + 30 + 190 - 16, sy + 72, 11)
    S.circle("vt", sx + 30 + 190 - 65, sy + 72, 11)
    arrow(S, sx + 230, sy + 135, sx + 300, sy + 135, "v")
    S.rect("id", sx + 320, sy + 70, 170, 130)
    S.text("hw", sx + 405, sy + 140, "공간 사라짐", "middle")
    arrow(S, sx + 405, sy + 215, sx + 140, sy + 240, "c")
    S.text("hc", sx + 270, sy + 265, "최소화했던 창 다시 띄우기 → 복귀", "middle")
    S.text("s", sx + 30, sy + 290, "실제 프로그램 창을 닫아도 공략상 이득은 없다")

    # 6. countdown lock
    x = 1200
    rules_panel(S, x, y2, "6. 카운트다운 중인 창", ["옮기기는 가능 · 최소화와 닫기는 막힘", "막으면 제목줄이 잠깐 바뀐다"])
    sx, sy = x + 20, y2 + 130
    wx, wy = sx + 140, sy + 70
    os_window(S, wx, wy, 250, 170, "mine")
    S.text("hv", wx + 10, wy + 16, "(응답 없음)")
    S.line("vt", wx + 8, wy + 32, wx + 140, wy + 32)
    S.line("d", wx + 140, wy + 32, wx + 242, wy + 32)
    xmark(S, wx + 250 - 16, wy - 14, 6)
    xmark(S, wx + 250 - 65, wy - 14, 6)
    for dx, dy in ((-1, 0), (1, 0), (0, 1)):
        cx, cy = wx + 125 + dx * 150, wy + 85 + dy * 115
        arrow(S, wx + 125 + dx * 132, wy + 85 + dy * 95, cx, cy, "c")
    check(S, sx + 30, sy + 30)
    S.text("hc", sx + 45, sy + 36, "이동 OK: 몹 무리를 덮거나 탄을 막는 데 써도 된다")
    S.save("Norma_Rules_v4.svg")


def p2_prep_v4(S, x, y):
    p2_prep(S, x, y)
    S.arc("c", x + 190, y + 215, 140, math.pi, math.pi)
    S.text("hc", x + 12, y + 262, "청록 호 = 닿을 수 있는 거리")


def p3_prep_v4(S, x, y):
    p3_prep(S, x, y)
    S.text("hv", x + 90 + 230, y + 255, "옮기기만 가능", "end")


def board_patterns_v4():
    S = Svg(1800, 1330, "NORMA 패턴 v4")
    S.text("t1", 30, 56, "NORMA v4 · 패턴 1 · 2 · 3 · 6")
    S.text("t2", 30, 92, "직접 공격은 6번 연사 하나 · 4번 창 순서 바꾸기는 보류 · 반사 창 삭제 · 본체는 임시 네모")
    pattern_panel(S, 30, 130, 1, "창 너머의 보스", "플레이어에게서 가장 먼 실제 창에 들어가 그 안에서 연사",
                  p1_prep, p1_act, ["대응: 들어가서 보스를 칠지,", "밖에 남아 몹을 정리할지 선택"])
    pattern_panel(S, 910, 130, 2, "피난 창", "화면 전체 탄막 + 닿을 수 있는 거리 안에 창 2~3개",
                  p2_prep_v4, p2_act, ["대응: 창으로 피신. 탄막은 못 들어오지만", "어그로가 끌린 몹은 걸어 들어온다"])
    pattern_panel(S, 30, 650, 3, "창 닫기 (2페이즈)", "내가 있는 창에 카운트다운 · 옮기기만 되고 접기·닫기는 막힘",
                  p3_prep_v4, p3_act, ["대응: 빠져나가거나, 창을 옮겨 몹 무리를 덮는다", "내가 창 밖이면 이 패턴 대신 다른 패턴"])
    pattern_panel(S, 910, 650, 6, "십자 연사 (유일한 직접 공격)", "탄 4줄기가 돌며 나선을 그린다",
                  a6_prep, a6_act, ["대응: 회전 방향을 보고 줄기 사이로 따라 돈다", "탄은 창 경계에서 흩어지고 창 내구도를 깎는다"])

    # phase bar
    y0 = 1170
    S.text("hw", 50, y0, "페이즈와 6번 탄 모양")
    x1, x2, by = 60, 1000, y0 + 60
    S.line("i", x1, by, x2, by)
    for pct in (100, 60, 40, 30, 0):
        px = x1 + (x2 - x1) * (100 - pct) / 100
        S.line("it" if pct == 40 else "id", px, by - (14 if pct == 40 else 8), px, by + (14 if pct == 40 else 8))
        S.text("hw" if pct == 40 else "s", px, by + 34, f"{pct}%", "middle")
    S.text("hi", x1 + (x2 - x1) * 0.3, by - 16, "1페이즈: 1 · 2 · 6", "middle")
    S.text("hi", x1 + (x2 - x1) * 0.8, by - 16, "2페이즈: + 3 · 더 괴랄하게", "middle")
    # skins
    bx = 1100
    S.text("s", bx, y0 + 20, "우주")
    for i in range(3):
        a = math.radians(120 * i - 90)
        S.circle("i", bx + 30 + math.cos(a) * 10, by + 10 + math.sin(a) * 10, 2.5)
    S.poly("id", [(bx + 30 + math.cos(math.radians(120 * i - 90)) * 10, by + 10 + math.sin(math.radians(120 * i - 90)) * 10) for i in range(3)])
    S.text("s", bx + 160, y0 + 20, "균열")
    for i in range(3):
        S.rect("i", bx + 160 + i * 16, by + 4, 9, 9)
    S.text("s", bx + 330, y0 + 20, "노출")
    gx = bx + 330
    S.line("i", gx, by + 10, gx + 12, by + 10)
    S.rect("i", gx + 24, by + 4, 12, 12)
    S.line("i", gx + 48, by + 4, gx + 60, by + 16)
    S.line("i", gx + 60, by + 4, gx + 48, by + 16)
    S.text("s", bx, by + 50, "판정은 같고 모양만 바뀐다 · 60/30% 연출 구간과 40% 페이즈를 맞출지는 미정")
    S.save("Norma_Patterns_v4.svg")


board_rules_v4()
board_patterns_v4()
print("ok")
