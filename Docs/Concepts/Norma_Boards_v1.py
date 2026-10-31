"""NORMA boss concept boards (SVG). Lines only, no fills, no dashed lines."""
import math, sys, os

OUT = sys.argv[1]

STYLE = """
*{vector-effect:non-scaling-stroke}
.bg{fill:#080910}
.frame{fill:none;stroke:#2b2d3d;stroke-width:1.4}
.scr{fill:none;stroke:#3a3d52;stroke-width:1.4}
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
.hv{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
.hc{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.hg{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8a90a8}
.hr{font:700 16px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#ff8a9a}
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


# ---------- primitives ----------

def bracket(S, x, y, sx, sy, ln, cls="w", node=True):
    """Right-angle bracket with its bend at (x,y); arms point away from quadrant (sx,sy)."""
    S.poly(cls, [(x - sx * ln, y), (x, y), (x, y - sy * ln)], close=False)
    if node:
        S.circle(cls, x, y, 4)
        S.circle(cls, x - sx * ln, y, 2.4)
        S.circle(cls, x, y - sy * ln, 2.4)


def core(S, x, y, s=1.0, cls="w"):
    r = 30 * s
    S.poly(cls, [(x, y - r), (x + r, y), (x, y + r), (x - r, y)])
    r2 = 12 * s
    S.poly(cls, [(x, y - r2), (x + r2, y), (x, y + r2), (x - r2, y)])
    S.circle(cls, x, y, 2.2 * s)


def norma(S, x, y, s=1.0, links=True, cls="w", bcls="w"):
    core(S, x, y, s, cls)
    d = 58 * s
    for sx in (-1, 1):
        for sy in (-1, 1):
            bx, by = x + sx * d, y + sy * d
            bracket(S, bx, by, sx, sy, 30 * s, bcls)
            if links:
                S.line("b", bx - sx * 6, by - sy * 6, x + sx * 15 * s, y + sy * 15 * s)


def window(S, x, y, w, h, kind="fake", state="mine", title=None):
    """kind fake|real ; state mine|other|lit"""
    if kind == "real":
        cls = {"mine": "g", "other": "gd", "lit": "g"}[state]
    else:
        cls = {"mine": "bt", "other": "d", "lit": "bt"}[state]
    S.rect(cls, x, y, w, h)
    S.line(cls, x, y + 24, x + w, y + 24)
    gx = x + w - 16
    # x close node, square, minimize
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
    pts = [(x + ux * 22, y + uy * 22), (x + vx * 9, y + vy * 9), (x - ux * 14, y - uy * 14), (x - vx * 9, y - vy * 9)]
    S.poly(cls, pts)
    S.circle(cls, x, y, 2)


def bullet(S, x, y, ang, s=7, cls="w"):
    """Boss bullet: right-angle bracket whose corner points along travel."""
    a = math.radians(ang)
    tx, ty = x + math.cos(a) * s * 0.5, y + math.sin(a) * s * 0.5
    p1 = (tx - math.cos(a - math.pi / 4) * s, ty - math.sin(a - math.pi / 4) * s)
    p2 = (tx - math.cos(a + math.pi / 4) * s, ty - math.sin(a + math.pi / 4) * s)
    S.poly(cls, [p1, (tx, ty), p2], close=False)


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


# ======================================================================
# Board 1: anatomy + layer rules
# ======================================================================

def board_anatomy():
    S = Svg(1800, 1160, "NORMA 보스 구조와 창 공간 규칙")
    S.text("t1", 30, 56, "NORMA · 직각자자리 보스 — 구조와 창 공간 규칙")
    S.text("t2", 30, 92, "바탕화면의 창을 재고 차지하는 자 · 선으로만 그린다 · 면 채움·점선·가시 없음 · 모서리는 날카롭게")

    # ---- Panel A: body
    S.rect("frame", 30, 120, 640, 1010)
    S.text("h", 52, 160, "A. 본체")
    S.text("p", 52, 188, "속이 빈 마름모 코어 + ㄱ자 브래킷 4개 + 별 노드")
    norma(S, 350, 400, 2.2)
    S.text("hw", 350, 400 + 2.2 * 58 + 2.2 * 30 + 40, "코어", "middle")
    S.text("p", 120, 245, "브래킷 꺾임점 = 별 노드")
    S.line("v", 228, 252, 350 - 2.2 * 58 + 4, 400 - 2.2 * 58 + 2)
    S.text("p", 455, 650, "코어와 브래킷을 잇는 연결선", "start")
    S.line("v", 452, 645, 350 + 2.2 * 30, 400 + 2.2 * 30)

    # detach sequence
    S.text("hw", 52, 735, "패턴마다 브래킷이 떨어져 사각형의 네 귀퉁이에 붙는다")
    core(S, 120, 870, 0.9)
    wx, wy, ww, wh = 260, 790, 300, 170
    window(S, wx, wy, ww, wh, "real", "mine", "실제 창")
    for (sx, sy, cx, cy) in ((-1, -1, wx, wy), (1, -1, wx + ww, wy), (-1, 1, wx, wy + wh), (1, 1, wx + ww, wy + wh)):
        bracket(S, cx, cy, sx, sy, 26)
        bracket(S, 120 + sx * 46, 870 + sy * 46, sx, sy, 14, "d", node=False)
    arrow(S, 175, 870, 245, 870, "v")
    S.text("s", 120, 945, "빈 자리", "middle")
    S.text("s", 410, 990, "어디에 붙었는지가 곧 어떤 패턴인지를 알려준다", "middle")

    # bullet comparison
    S.text("hw", 52, 1040, "보스 탄")
    for i in range(3):
        bullet(S, 140 + i * 30, 1085, 0, 9)
    arrow(S, 220, 1085, 260, 1085, "d", 6)
    S.text("p", 280, 1080, "ㄱ자 브래킷 모양, 모서리가 진행 방향")
    S.text("s", 280, 1104, "일반 몹 탄(작은 점)과 섞이지 않게 모양부터 다르게")

    # ---- Panel B: layer rules
    X0 = 700
    S.rect("frame", X0, 120, 1070, 1010)
    S.text("h", X0 + 22, 160, "B. 창 = 별도 공간")
    S.text("p", X0 + 22, 188, "창 안과 밖은 공격이 서로 닿지 않는다 · 경계를 넘는 것은 몹과 플레이어의 이동뿐")

    # B1 projectiles dissolve
    bx, by = X0 + 30, 230
    S.text("hw", bx, by, "① 투사체는 경계에서 소멸")
    window(S, bx + 230, by + 30, 260, 180, "fake", "other", "창")
    player(S, bx + 40, by + 130, 0)
    for i in range(2):
        pshot(S, bx + 90 + i * 70, by + 130, 0)
    spark(S, bx + 230, by + 130, "c")
    mob(S, bx + 370, by + 130, dim=True)
    for i, (yy) in enumerate((by + 95, by + 160)):
        bullet(S, bx + 300 - i * 20, yy, 180)
        S.line("d", bx + 312 - i * 20, yy, bx + 340 - i * 20, yy)
    spark(S, bx + 232, by + 95, "v")
    spark(S, bx + 232, by + 160, "v")
    S.lines("s", bx + 510, by + 70, ["플레이어 탄 · 보스 탄 · 몹 탄 모두", "경계에 닿으면 작은 별 조각으로 흩어짐", "", "전기장·폭발 같은 범위 공격도", "경계에서 잘린다", "", "접촉 피해도 같은 공간끼리만"], 22)

    # B2 mob crossing + antares
    by2 = 470
    S.text("hw", bx, by2, "② 몹과 플레이어는 걸어서 넘나든다 — 넘는 순간 소속이 바뀜")
    window(S, bx + 230, by2 + 30, 260, 180, "fake", "mine", "내가 있는 창")
    player(S, bx + 400, by2 + 150)
    mob(S, bx + 150, by2 + 100, dim=True)
    arrow(S, bx + 165, by2 + 100, bx + 215, by2 + 100, "d")
    mob(S, bx + 280, by2 + 100)
    S.text("s", bx + 150, by2 + 75, "다른 공간: 어둡고 얇게", "middle")
    S.text("s", bx + 300, by2 + 75, "같은 공간: 밝게", "start")
    antares(S, bx + 130, by2 + 175, 0, dim=True)
    S.line("d", bx + 60, by2 + 175, bx + 110, by2 + 175)
    arrow(S, bx + 160, by2 + 175, bx + 300, by2 + 175, "b")
    antares(S, bx + 330, by2 + 175, 0)
    S.lines("s", bx + 510, by2 + 80, ["안타레스 돌진도 예외 없음", "창 경계는 벽이 아니다 → 벽 충돌 기절 없이 통과", "넘어온 뒤부터 같은 공간 규칙을 따름", "", "경계에 걸친 몹·플레이어는 중심점이 있는 쪽 소속", "", "플레이어도 창에 갇히지 않고 자유롭게 넘나든다"], 22)

    # B3 overlap
    by3 = 710
    S.text("hw", bx, by3, "③ 창이 겹치면 앞 창이 겹친 영역을 가진다")
    ax_, ay_ = bx + 40, by3 + 40
    # back window B (dim), hidden inside overlap
    window(S, ax_, ay_, 260, 130, "fake", "mine", "앞 창 A")
    # back window B: border only where A does not cover it
    S.poly("d", [(ax_ + 260, ay_ + 60), (ax_ + 400, ay_ + 60), (ax_ + 400, ay_ + 180), (ax_ + 170, ay_ + 180), (ax_ + 170, ay_ + 130)], close=False)
    S.line("d", ax_ + 260, ay_ + 84, ax_ + 400, ay_ + 84)
    S.text("hb", ax_ + 280, ay_ + 78, "뒤 창 B")
    S.lines("s", bx + 510, by3 + 70, ["겹친 영역은 앞 창 A의 공간", "뒤 창 B의 테두리는 겹친 영역에서 그리지 않음", "", "창 순서가 바뀌면 겹친 영역의 소속도 바뀐다", "(패턴 4의 재료)"], 22)

    # B4 legend
    by4 = 985
    S.text("hw", bx, by4, "읽는 법 (막 없이 선 두께·밝기로만)")
    S.rect("bt", bx + 10, by4 + 25, 60, 36)
    S.text("p", bx + 85, by4 + 50, "내가 있는 창")
    S.rect("d", bx + 260, by4 + 25, 60, 36)
    S.text("p", bx + 335, by4 + 50, "다른 창")
    S.rect("g", bx + 470, by4 + 25, 60, 36)
    S.text("p", bx + 545, by4 + 50, "실제 창 (회색)")
    S.rect("bt", bx + 740, by4 + 25, 60, 36)
    S.text("p", bx + 815, by4 + 50, "가짜 창 (청색)")
    mob(S, bx + 40, by4 + 100)
    S.text("p", bx + 85, by4 + 106, "같은 공간 몹")
    mob(S, bx + 290, by4 + 100, dim=True)
    S.text("p", bx + 335, by4 + 106, "다른 공간 몹")
    spark(S, bx + 500, by4 + 100)
    S.text("p", bx + 545, by4 + 106, "경계에 막힌 탄")
    player(S, bx + 770, by4 + 100)
    S.text("p", bx + 815, by4 + 106, "플레이어")

    S.save("Norma_Anatomy_Rules_v1.svg")


# ======================================================================
# Board 2: window patterns 1–4
# ======================================================================

SW, SH = 390, 270


def pattern_panel(S, x, y, num, name, sub, prep, act, counter):
    S.rect("frame", x, y, 860, 490)
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
    wx, wy, ww, wh = x + 190, y + 70, 170, 140
    window(S, wx, wy, ww, wh, "real", "other", "실제 창")
    core(S, x + 90, y + 80, 0.6)
    for sx, sy, cx, cy in ((-1, -1, wx, wy), (1, -1, wx + ww, wy), (-1, 1, wx, wy + wh), (1, 1, wx + ww, wy + wh)):
        bracket(S, cx, cy, sx, sy, 18, "wt", node=False)
        S.circle("w", cx, cy, 4)
    arrow(S, x + 110, y + 95, x + 255, y + 150, "v")
    S.text("hv", x + 20, y + 128, "코어가 들어감")
    player(S, x + 70, y + 210)
    mob(S, x + 120, y + 230)
    mob(S, x + 40, y + 160)


def p1_act(S, x, y):
    wx, wy, ww, wh = x + 190, y + 70, 170, 140
    window(S, wx, wy, ww, wh, "real", "lit", "실제 창")
    for sx, sy, cx, cy in ((-1, -1, wx, wy), (1, -1, wx + ww, wy), (-1, 1, wx, wy + wh), (1, 1, wx + ww, wy + wh)):
        bracket(S, cx, cy, sx, sy, 18, "w", node=False)
    cx, cy = wx + ww / 2, wy + 85
    core(S, cx, cy, 0.6, "d")
    for i in range(8):
        a = 45 * i + 22
        r = 40
        bullet(S, cx + math.cos(math.radians(a)) * r, cy + math.sin(math.radians(a)) * r, a, 6, "d")
    spark(S, wx + ww, cy, "d", 7)
    player(S, x + 70, y + 160, 0)
    for i in range(2):
        pshot(S, x + 100 + i * 35, y + 160, 0)
    spark(S, wx, y + 160, "c")
    mob(S, x + 110, y + 230)
    mob(S, x + 40, y + 225)
    S.text("s", x + 10, y + 258, "밖: 탄이 경계에서 소멸")
    S.text("s", wx + 10, wy + wh + 22, "안: 코어 탄막 (안에서만)")


def ray_end(ox, oy, a, rmax, bounds, rects):
    """Distance along the ray until it leaves bounds or meets a rect edge."""
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


def p2_prep(S, x, y):
    norma(S, x + 195, y + 55, 0.42)
    rects = ((x + 30, y + 150, 120, 95), (x + 245, y + 160, 120, 90))
    for i in range(9):
        a = 30 + i * 15
        e = ray_end(x + 195, y + 55, a, 400, (x, y, SW, SH), rects)
        S.line("v", x + 195 + math.cos(math.radians(a)) * 45, y + 55 + math.sin(math.radians(a)) * 45,
               x + 195 + math.cos(math.radians(a)) * e, y + 55 + math.sin(math.radians(a)) * e)
    window(S, x + 30, y + 150, 120, 95, "fake", "other")
    window(S, x + 245, y + 160, 120, 90, "fake", "other")
    S.text("hb", x + 90, y + 142, "피난 창", "middle")
    player(S, x + 170, y + 200)


def p2_act(S, x, y):
    norma(S, x + 195, y + 55, 0.42)
    wx, wy, ww, wh = x + 30, y + 150, 120, 95
    window(S, wx, wy, ww, wh, "fake", "mine")
    window(S, x + 245, y + 160, 120, 90, "fake", "other")
    for i in range(9):
        a = 30 + i * 15
        for k in (90, 140, 190):
            px, py = x + 195 + math.cos(math.radians(a)) * k, y + 55 + math.sin(math.radians(a)) * k
            inside = any(rx <= px <= rx + rw and ry <= py <= ry + rh for rx, ry, rw, rh in ((wx, wy, ww, wh), (x + 245, y + 160, 120, 90)))
            if not inside and y + 5 < py < y + SH - 5 and x + 5 < px < x + SW - 5:
                bullet(S, px, py, a, 6)
    spark(S, wx + ww, wy + 30)
    spark(S, x + 245, y + 185)
    player(S, wx + 50, wy + 65)
    mob(S, x + 10 + 0, y + 200, dim=True)
    arrow(S, x + 22, y + 210, x + 50, y + 225, "b")
    mob(S, x + 62, y + 230)
    S.text("s", x + 8, y + 262, "몹은 걸어서 침입")


def p3_prep(S, x, y):
    wx, wy, ww, wh = x + 90, y + 60, 230, 170
    window(S, wx, wy, ww, wh, "fake", "mine")
    S.circle("vt", wx + ww - 16, wy + 12, 11)
    S.line("vt", wx + 8, wy + 32, wx + 120, wy + 32)
    S.line("d", wx + 120, wy + 32, wx + ww - 8, wy + 32)
    S.text("hv", wx + 8, wy + 52, "카운트다운")
    player(S, wx + 120, wy + 120)
    mob(S, wx + 60, wy + 130)
    mob(S, wx + 180, wy + 100)
    core(S, x + 40, y + 40, 0.5)


def p3_act(S, x, y):
    wx, wy, ww, wh = x + 90, y + 60, 230, 170
    window(S, wx, wy, ww, wh, "fake", "other")
    for k in (25, 50):
        S.rect("vt" if k == 25 else "v", wx + k, wy + k * 0.7, ww - 2 * k, wh - 1.4 * k)
    cx, cy = wx + ww / 2, wy + wh / 2 + 5
    for i in range(12):
        t = math.radians(30 * i)
        S.line("wt", cx + math.cos(t) * 10, cy + math.sin(t) * 10, cx + math.cos(t) * 30, cy + math.sin(t) * 30)
    mob(S, wx + 60, wy + 130, dim=True)
    mob(S, wx + 180, wy + 100, dim=True)
    S.line("r", wx + 52, wy + 122, wx + 68, wy + 138)
    S.line("r", wx + 68, wy + 122, wx + 52, wy + 138)
    S.line("r", wx + 172, wy + 92, wx + 188, wy + 108)
    S.line("r", wx + 188, wy + 92, wx + 172, wy + 108)
    arrow(S, wx + 200, wy + 150, wx + ww + 30, wy + wh + 15, "c")
    player(S, wx + ww + 45, wy + wh + 22)
    S.text("s", x + 10, y + 262, "안에 남은 몹도 함께 피해")


def p4_prep(S, x, y):
    ax_, ay_, aw, ah = x + 40, y + 50, 200, 150
    bx_, by_, bw, bh = x + 170, y + 90, 190, 140
    # back window B: only the parts A does not cover
    S.poly("d", [(ax_ + aw, by_), (bx_ + bw, by_), (bx_ + bw, by_ + bh), (bx_, by_ + bh), (bx_, ay_ + ah)], close=False)
    S.line("vt", ax_ + aw, by_ + 24, bx_ + bw, by_ + 24)
    window(S, ax_, ay_, aw, ah, "fake", "mine", "A (앞)")
    S.text("hv", ax_ + aw + 60, by_ + 18, "B 밝아짐", "middle")
    player(S, x + 205, y + 170)
    for i in range(3):
        bullet(S, x + 300 + i * 18, y + 180, 180, 6, "d")
    spark(S, ax_ + aw, y + 180, "d")
    S.text("s", x + 280, y + 215, "B의 탄", "start")


def p4_act(S, x, y):
    ax_, ay_, aw, ah = x + 40, y + 50, 200, 150
    bx_, by_, bw, bh = x + 170, y + 90, 190, 140
    # A back now: hide inside B
    S.poly("d", [(bx_, ay_ + ah), (ax_, ay_ + ah), (ax_, ay_), (ax_ + aw, ay_), (ax_ + aw, by_)], close=False)
    S.line("d", ax_, ay_ + 24, ax_ + aw, ay_ + 24)
    window(S, bx_, by_, bw, bh, "fake", "lit", "B (앞)")
    S.text("hb", ax_ + 8, ay_ + 18, "A")
    for i in range(3):
        bullet(S, x + 250 + i * 18, y + 180, 180, 6)
    player(S, x + 205, y + 170)
    arrow(S, x + 200, y + 160, x + 120, y + 120, "c")
    S.text("s", x + 10, y + 262, "겹친 영역이 B 공간 → 탄이 닿음")


def board_window_patterns():
    S = Svg(1800, 1170, "NORMA 창 패턴 1–4")
    S.text("t1", 30, 56, "NORMA · 창 패턴 1–4")
    S.text("t2", 30, 92, "예고 → 실행 · 회색 = 실제 창 · 청색 = 보스가 만든 가짜 창 · 보라 = 예고선 · 흰색 = 실제 위험")
    pattern_panel(S, 30, 130, 1, "창 너머의 코어", "브래킷 4개가 실제 창 귀퉁이에 박히고 코어가 그 공간으로 숨는다",
                  p1_prep, p1_act, ["대응: 들어가서 탄막을 견디며 코어를 칠지,", "밖에 남아 일반 몹을 정리할지 선택한다"])
    pattern_panel(S, 910, 130, 2, "피난 창", "화면 전체 탄막 예고와 함께 가짜 창 2~3개가 열린다",
                  p2_prep, p2_act, ["대응: 창으로 피신한다. 탄막은 못 들어오지만", "몹은 걸어 들어오므로 같은 공간에서 처리"])
    pattern_panel(S, 30, 650, 3, "창 닫기", "내가 있는 창의 × 노드가 켜지고 제목줄 카운트다운이 줄어든다",
                  p3_prep, p3_act, ["대응: 카운트가 끝나기 전에 빠져나간다", "안에 남겨둔 몹은 함께 정리된다"])
    pattern_panel(S, 910, 650, 4, "창 순서 바꾸기", "겹친 두 가짜 창 중 뒤 창의 제목줄이 밝아진다",
                  p4_prep, p4_act, ["대응: 겹친 영역에서 빠지거나,", "앞으로 올라올 창 쪽을 미리 정리한다"])
    S.save("Norma_Window_Patterns_v1.svg")


# ======================================================================
# Board 3: projectile patterns 5–7
# ======================================================================

def p5_prep(S, x, y):
    cx, cy = x + 70, y + 70
    norma(S, cx, cy, 0.42)
    px, py = x + 300, y + 210
    S.line("v", cx + 30, cy + 20, px - 15, py - 10)
    a = math.atan2(py - cy, px - cx)
    nx, ny = -math.sin(a), math.cos(a)
    L = math.hypot(px - cx, py - cy)
    for k in range(1, 9):
        t = 0.15 + k * 0.085
        tx, ty = cx + (px - cx) * t, cy + (py - cy) * t
        ln = 9 if k % 3 == 0 else 5
        S.line("v", tx - nx * ln, ty - ny * ln, tx + nx * ln, ty + ny * ln)
    player(S, px, py, math.degrees(a) + 180)
    S.text("hv", x + 200, y + 105, "자 모양 실선 + 눈금")


def p5_act(S, x, y):
    cx, cy = x + 70, y + 70
    norma(S, cx, cy, 0.42)
    wx, wy, ww, wh = x + 250, y + 120, 120, 105
    window(S, wx, wy, ww, wh, "fake", "mine")
    tx, ty = wx, wy + 60
    a = math.atan2(ty - cy, tx - cx)
    for t in (0.22, 0.3, 0.38, 0.62, 0.7, 0.78):
        bullet(S, cx + (tx - cx) * t, cy + (ty - cy) * t, math.degrees(a), 7)
    spark(S, tx, ty)
    player(S, wx + 75, wy + 75, -150)
    arrow(S, x + 215, y + 245, wx + 60, wy + 85, "c")
    S.text("s", x + 10, y + 262, "3발씩 끊어 쏘고 묶음마다 다시 조준 · 창으로 사선 끊기")


def p6_prep(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    norma(S, cx, cy, 0.42)
    R = 85
    S.circle("v", cx, cy, R)
    for i in range(12):
        a = math.radians(30 * i)
        px, py = cx + math.cos(a) * R, cy + math.sin(a) * R
        cw = 1 if i % 2 == 0 else -1
        # small L glyph: radial stub then tangent stub
        ox, oy = math.cos(a), math.sin(a)
        tx, ty = -oy * cw, ox * cw
        S.poly("vt", [(px - ox * 9, py - oy * 9), (px, py), (px + tx * 9, py + ty * 9)], close=False)
    S.text("hv", cx, y + 22, "꺾임 고리 + 방향 기호", "middle")
    player(S, x + 340, y + 230)


def p6_act(S, x, y):
    cx, cy = x + SW / 2, y + SH / 2
    norma(S, cx, cy, 0.42)
    R = 85
    S.circle("d", cx, cy, R)
    for i in range(12):
        a = math.radians(30 * i)
        ox, oy = math.cos(a), math.sin(a)
        cw = 1 if i % 2 == 0 else -1
        tx, ty = -oy * cw, ox * cw
        px, py = cx + ox * R, cy + oy * R
        S.line("d", cx + ox * 40, cy + oy * 40, px, py)
        ex, ey = px + tx * 50, py + ty * 50
        S.line("w", px, py, ex, ey)
        bullet(S, ex, ey, math.degrees(math.atan2(ty, tx)), 7)
    player(S, x + 340, y + 230)
    S.text("s", x + 10, y + 262, "고리에 닿는 순간 90° 꺾임 · 시계/반시계 교차")


def p7_prep(S, x, y):
    S.rect("bt", x + 60, y + 70, 130, 130)
    S.rect("bt", x + 66, y + 76, 118, 118)
    S.line("bt", x + 66, y + 100, x + 184, y + 100)
    S.text("hv", x + 125, y + 230, "두 겹 테두리 = 반사 창", "middle")
    window(S, x + 240, y + 70, 120, 130, "fake", "other")
    S.text("hb", x + 300, y + 230, "보통 창", "middle")
    norma(S, x + 200, y + 30, 0.3, links=False)


def p7_act(S, x, y):
    S.rect("bt", x + 60, y + 70, 130, 130)
    S.rect("bt", x + 66, y + 76, 118, 118)
    window(S, x + 240, y + 70, 120, 130, "fake", "other")
    # bullet reflects off left double window
    S.poly("w", [(x + 20, y + 30), (x + 60, y + 110), (x + 15, y + 190)], close=False)
    bullet(S, x + 15, y + 190, math.degrees(math.atan2(80, -45)), 8)
    S.text("s", x + 18, y + 250, "튕겨 나옴")
    # bullet dissolves at normal window
    S.line("w", x + 215, y + 20, x + 240, y + 110)
    spark(S, x + 240, y + 112)
    S.text("s", x + 250, y + 250, "보통 창은 소멸")
    player(S, x + 30, y + 225)


def board_projectiles():
    S = Svg(1800, 1210, "NORMA 투사체 패턴 5–7")
    S.text("t1", 30, 56, "NORMA · 직접 투사체 패턴 5–7")
    S.text("t2", 30, 92, "보스 탄은 쏜 공간에 속한다 · 창 경계에서 소멸 → 창은 엄폐물 (반사 창만 예외적으로 튕겨낸다)")

    # Use pattern_panel-like layout but 3 columns, each with stacked screens
    cols = [
        (30, 5, "눈금 사격", "조준 연사 · 나를 향한 한 줄 → 점들이 연속으로", p5_prep, p5_act,
         ["대응: 옆으로 꺾거나 창에 들어가 사선을 끊는다"]),
        (615, 6, "직각 탄막", "방사탄이 꺾임 고리에서 90° 꺾인다", p6_prep, p6_act,
         ["대응: 방향 기호를 읽고 빈 각도로 들어간다"]),
        (1200, 7, "반사 창 (2페이즈 강화)", "두 겹 테두리 창은 탄을 튕겨낸다", p7_prep, p7_act,
         ["5·6번의 탄을 튕기는 강화 · 독립 패턴으로 세지 않음"]),
    ]
    S.h = 1210
    for x, num, name, sub, prep, act, counter in cols:
        S.rect("frame", x, 130, 570, 850)
        S.text("h", x + 20, 170, f"{num}. {name}")
        S.text("p", x + 20, 198, sub)
        screen(S, x + 90, 250, SW, SH, "예고")
        prep(S, x + 90, 250)
        screen(S, x + 90, 610, SW, SH, "실행")
        act(S, x + 90, 610)
        S.lines("p", x + 20, 940, counter, 26)
    # bullet anatomy strip at bottom of each? keep shared legend
    y0 = 1010
    S.text("hw", 50, y0 + 20, "보스 탄과 경계")
    for i in range(4):
        bullet(S, 80 + i * 32, y0 + 70, 0, 9)
    S.text("p", 230, y0 + 76, "ㄱ자 브래킷 탄 · 모서리가 진행 방향")
    S.line("bt", 640, y0 + 40, 640, y0 + 110)
    bullet(S, 600, y0 + 75, 0, 9)
    S.line("d", 560, y0 + 75, 590, y0 + 75)
    spark(S, 642, y0 + 75)
    S.text("p", 670, y0 + 80, "보통 창 경계: 흩어져 소멸")
    S.line("bt", 1090, y0 + 40, 1090, y0 + 110)
    S.line("bt", 1098, y0 + 40, 1098, y0 + 110)
    S.poly("w", [(1030, y0 + 50), (1088, y0 + 75), (1030, y0 + 100)], close=False)
    bullet(S, 1030, y0 + 100, 157, 9)
    S.text("p", 1120, y0 + 80, "두 겹 테두리: 반사")
    S.text("s", 50, y0 + 150, "예고선은 보라색 얇은 실선, 실제 위험은 흰색 굵은 실선. 색을 빼도 선 굵기·도형으로 구별된다.")
    S.save("Norma_Projectile_Patterns_v1.svg")


board_anatomy()
board_window_patterns()
board_projectiles()
print("ok")
