"""NORMA body design A v1 (SVG): the carpenter's square and rule.

Norma was "Norma et Regula" (set square and rule); Regulus shares the root.
The body is two interlocking L set squares around Regulus' square heart, with
a graduated rule orbiting it. Lines only: no fills, no dashed lines, no
plates, no thorn spikes; density comes from inset contours and tick marks.
Usage: python Norma_Body_Design_A_v1.py <out_dir>
"""
import math, sys, os

OUT = sys.argv[1]
TAU = math.tau

STYLE = """
*{vector-effect:non-scaling-stroke}
.bg{fill:#080910}
.frame{fill:none;stroke:#2b2d3d;stroke-width:1.4}
.i{fill:none;stroke:#b8efff;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:miter}
.it{fill:none;stroke:#b8efff;stroke-width:2.8;stroke-linecap:round;stroke-linejoin:miter}
.ig{fill:none;stroke:#b8efff;stroke-opacity:.20;stroke-width:9;stroke-linecap:round}
.id{fill:none;stroke:#4d6a78;stroke-width:1.1;stroke-linecap:round;stroke-linejoin:miter}
.ih{fill:none;stroke:#ebfdff;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.w{fill:none;stroke:#eef1ff;stroke-width:2.2;stroke-linecap:round;stroke-linejoin:miter}
.wt{fill:none;stroke:#eef1ff;stroke-width:3.2;stroke-linecap:round;stroke-linejoin:miter}
.b{fill:none;stroke:#7f93ff;stroke-width:1.6;stroke-linecap:round;stroke-linejoin:miter}
.bt{fill:none;stroke:#9fb0ff;stroke-width:2.6;stroke-linecap:round;stroke-linejoin:miter}
.v{fill:none;stroke:#c3b2ff;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:miter}
.c{fill:none;stroke:#34e0f0;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.r{fill:none;stroke:#ff6a5c;stroke-width:2.2;stroke-linecap:round;stroke-linejoin:miter}
.rd{fill:none;stroke:#ff6a5c;stroke-opacity:.35;stroke-width:1.4;stroke-linecap:round;stroke-linejoin:miter}
.rg{fill:none;stroke:#ff6a5c;stroke-opacity:.18;stroke-width:10;stroke-linecap:round}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:20px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 24px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.s{font:15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9097ae}
.hc{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.hv{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
.hb{font:700 13px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8fa2ff}
"""


class Svg:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.o = w, h, title, []

    def path(self, cls, d):
        self.o.append(f'<path class="{cls}" d="{d}"/>')

    def line(self, cls, x1, y1, x2, y2):
        self.path(cls, f"M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}")

    def poly(self, cls, pts, close=True):
        self.path(cls, "M" + " ".join(f"{x:.1f} {y:.1f}" for x, y in pts) + ("Z" if close else ""))

    def circle(self, cls, x, y, r):
        self.o.append(f'<circle class="{cls}" cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')

    def arc(self, cls, x, y, r, a0, sweep):
        x0, y0 = x + math.cos(a0) * r, y + math.sin(a0) * r
        x1, y1 = x + math.cos(a0 + sweep) * r, y + math.sin(a0 + sweep) * r
        self.path(cls, f"M{x0:.1f} {y0:.1f} A{r:.1f} {r:.1f} 0 {1 if sweep > math.pi else 0} 1 {x1:.1f} {y1:.1f}")

    def rect(self, cls, x, y, w, h):
        self.o.append(f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

    def text(self, cls, x, y, s, anchor="start"):
        s = s.replace("&", "&amp;").replace("<", "&lt;")
        self.o.append(f'<text class="{cls}" x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}">{s}</text>')

    def lines(self, cls, x, y, rows, gap=20):
        for i, r in enumerate(rows):
            self.text(cls, x, y + i * gap, r)

    def save(self, name):
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
               f'width="{self.w}" height="{self.h}"><title>{self.title}</title>'
               f'<defs><style>{STYLE}</style></defs>'
               f'<rect class="bg" width="{self.w}" height="{self.h}"/>\n' + "\n".join(self.o) + "\n</svg>")
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(svg)


def xf(cx, cy, ang, s=1.0):
    """Local (x, y) -> board, rotated by ang and scaled."""
    c, n = math.cos(ang), math.sin(ang)
    return lambda x, y: (cx + (x * c - y * n) * s, cy + (x * n + y * c) * s)


# ---------- parts ----------

def set_square(S, T, arm, th, cls="it", inset=True, ticks=True, tcls="id"):
    """An L set square with its corner at local (0, 0), arms along +x and +y.
    T maps local points to the board."""
    pts = [(0, 0), (arm, 0), (arm, th), (th, th), (th, arm), (0, arm)]
    S.poly(cls, [T(*p) for p in pts])
    if inset:
        g = 4.0
        ins = [(g, g), (arm - g, g), (arm - g, th - g), (th - g, th - g), (th - g, arm - g), (g, arm - g)]
        S.poly("ih" if cls in ("it", "wt") else "id", [T(*p) for p in ins])
    if ticks:
        # graduations along the outer edges, longer every fourth
        n = int(arm / 7)
        for k in range(1, n):
            d = k * 7.0
            ln = 6.0 if k % 4 == 0 else 3.0
            S.line(tcls, *T(d, 0), *T(d, -ln))
            S.line(tcls, *T(0, d), *T(-ln, d))


def heart(S, x, y, r, cls="it", ang=0.0, open_=False):
    """Regulus' square heart: a square on its corner, an inset square, a point."""
    pts = [(x + math.cos(ang + math.pi / 4 + k * math.pi / 2) * r,
            y + math.sin(ang + math.pi / 4 + k * math.pi / 2) * r) for k in range(4)]
    S.poly(cls, [(x + math.cos(ang + k * math.pi / 2) * r, y + math.sin(ang + k * math.pi / 2) * r) for k in range(4)])
    S.poly("ih", [(x + math.cos(ang + k * math.pi / 2) * r * 0.55, y + math.sin(ang + k * math.pi / 2) * r * 0.55) for k in range(4)])
    if open_:
        S.poly("i", [(x + math.cos(ang + math.pi / 4 + k * math.pi / 2) * r * 0.9,
                      y + math.sin(ang + math.pi / 4 + k * math.pi / 2) * r * 0.9) for k in range(4)])
    S.circle("i", x, y, 2.4)


def rule(S, x, y, ang, length, th=8.0, cls="i", lit=0.0):
    """The graduated rule, centred at (x, y) along angle ang."""
    T = xf(x, y, ang)
    h = length / 2
    S.poly(cls, [T(-h, -th / 2), T(h, -th / 2), T(h, th / 2), T(-h, th / 2)])
    n = int(length / 6)
    for k in range(1, n):
        d = -h + k * 6.0
        ln = th * 0.7 if k % 5 == 0 else th * 0.35
        lit_k = lit > 0 and (k / n) < lit
        S.line("w" if lit_k else "id", *T(d, -th / 2), *T(d, -th / 2 + ln))


def norma_a1(S, x, y, s=1.0, ang=0.0, spread=0.0, cls="it", heart_open=False, red=False, detail=True):
    """Variant A1: two Ls interlocked into a square frame, gaps at the
    other two corners; `spread` pushes them apart (phase 2)."""
    side = 64 * s
    arm, th = side * 0.74, 9 * s
    for k in (0, 2):
        # each L's corner sits at a frame corner; the second is turned 180
        T0 = xf(x, y, ang + (math.pi if k == 2 else 0.0))
        T = (lambda T0: (lambda u, v: T0(u - side / 2 - spread, v - side / 2 - spread)))(T0)
        set_square(S, T, arm, th, "r" if red else cls, inset=detail, ticks=detail)
    if detail:
        heart(S, x, y, 14 * s * (1.3 if heart_open else 1.0), "wt" if heart_open else cls, ang, heart_open)


def norma_a2(S, x, y, s=1.0):
    """Variant A2: the same pair turned 45 degrees: a diamond outline."""
    norma_a1(S, x, y, s, ang=math.pi / 4)


def norma_a3(S, x, y, s=1.0):
    """Variant A3: one large L with a small one nested in its crook, like a
    square knot; the rule crosses both."""
    side = 64 * s
    T = xf(x, y, 0.0)
    set_square(S, lambda u, v: T(u - side / 2, v - side / 2), side, 12 * s, "it")
    T2 = xf(x, y, math.pi)
    set_square(S, lambda u, v: T2(u - side * 0.25, v - side * 0.25), side * 0.62, 9 * s, "i", ticks=False)
    heart(S, x + side * 0.12, y + side * 0.12, 9 * s, "i")


def player(S, x, y, ang=-90):
    a = math.radians(ang)
    S.poly("c", [(x + r * math.cos(a + math.radians(da)), y + r * math.sin(a + math.radians(da)))
                 for da, r in ((0, 13), (140, 10), (180, 4), (220, 10))])


def shard(S, x, y, ang, s=7, cls="i"):
    u = (math.cos(ang), math.sin(ang)); v = (-u[1], u[0])
    S.poly(cls, [(x + (px * u[0] + py * v[0]) * s, y + (px * u[1] + py * v[1]) * s)
                 for px, py in ((-.5, -.5), (.5, -.5), (.5, .5), (-.5, .5))])


def pane(S, x, y, w, h, cls="bt"):
    S.rect(cls, x, y, w, h)
    S.line(cls, x, y + 18, x + w, y + 18)


# ---------- board ----------

def frame(S, x, y, w, h, tag, cls_tag, caption):
    S.rect("frame", x, y, w, h)
    S.text(cls_tag, x + 10, y + 22, tag)
    S.lines("s", x, y + h + 22, caption)


def board():
    W, H = 1800, 1330
    S = Svg(W, H, "NORMA 몸체 디자인 A v1")
    S.text("t1", 30, 56, "NORMA 몸체 디자인 A v1 · 직각자와 자")
    S.text("t2", 30, 92, "Norma = 옛 이름 Norma et Regula(직각자와 곧은 자) · Regulus와 같은 뿌리(regula) · 창 괄호도 ㄱ자 = 직각자")
    S.text("s", 30, 120, "선만 · 채우기/점선/막/가시 없음 · 모서리 날카롭게 · 밀도는 안쪽 윤곽선과 눈금으로")

    # --- row 1: silhouettes
    S.text("h", 30, 168, "1. 실루엣 후보")
    fw, fh, y1 = 540, 360, 186
    variants = [
        ("A1  맞물린 직각자", "hc", ["ㄱ자 두 개가 사각 틀을 이루고 반대쪽 두 모서리는", "비어 있다 · 가운데 레굴루스의 사각 심장 · 자가 둘레를 돈다"]),
        ("A2  마름모", "hv", ["A1을 45° 돌린 형태 · 꼭짓점이 위아래로", "서서 더 날카롭고 공격적으로 보인다"]),
        ("A3  겹친 직각자", "hv", ["큰 ㄱ자 안에 작은 ㄱ자가 반대로 끼워진", "매듭 모양 · 비대칭이라 방향감이 강하다"]),
    ]
    for i, (tag, cls, cap) in enumerate(variants):
        fx = 30 + i * (fw + 30)
        frame(S, fx, y1, fw, fh, tag, cls, cap)
        cx, cy = fx + fw / 2, y1 + fh / 2 + 10
        if i == 0:
            norma_a1(S, cx, cy, 2.2)
            S.arc("id", cx, cy, 128, 0, TAU)
            rule(S, cx + 128 * math.cos(-0.6), cy + 128 * math.sin(-0.6), -0.6 + math.pi / 2, 120)
        elif i == 1:
            norma_a2(S, cx, cy, 2.2)
            S.arc("id", cx, cy, 138, 0, TAU)
            rule(S, cx + 138 * math.cos(2.4), cy + 138 * math.sin(2.4), 2.4 + math.pi / 2, 120)
        else:
            norma_a3(S, cx, cy, 2.2)
            rule(S, cx + 10, cy + 10, -math.pi / 4, 250)

    # --- row 2: what each part does (A1)
    y2 = y1 + fh + 120
    S.text("h", 30, y2 - 18, "2. A1 기준, 패턴마다 부품이 하는 일")
    sw, sh = 330, 300
    gap = (W - 60 - 5 * sw) / 4

    def st(i):
        return 30 + i * (sw + gap)

    # idle
    fx = st(0); frame(S, fx, y2, sw, sh, "평상시", "hc", ["자가 둘레를 천천히 돈다", "직각자 눈금이 숨 쉬듯 깜빡"])
    cx, cy = fx + sw / 2, y2 + sh / 2 + 10
    norma_a1(S, cx, cy, 1.5)
    S.arc("id", cx, cy, 88, 0, TAU)
    rule(S, cx + 88 * math.cos(-2.2), cy + 88 * math.sin(-2.2), -2.2 + math.pi / 2, 84)

    # aim
    fx = st(1); frame(S, fx, y2, sw, sh, "7 조준 연사", "hc", ["자가 플레이어 쪽으로 정렬해 포신이", "된다 · 눈금이 끝까지 차면 발사"])
    cx, cy = fx + sw / 2 - 40, y2 + sh / 2 + 30
    norma_a1(S, cx, cy, 1.5)
    a = -0.55
    player(S, cx + 230 * math.cos(a), cy + 230 * math.sin(a), 150)
    rcx, rcy = cx + 92 * math.cos(a), cy + 92 * math.sin(a)
    rule(S, rcx, rcy, a, 90, lit=0.7)
    for j in range(3):
        d = 150 + j * 22
        shard(S, cx + d * math.cos(a), cy + d * math.sin(a), a, 7)

    # dash
    fx = st(2); frame(S, fx, y2, sw, sh, "11 본체 돌진", "hc", ["닫힌 모서리(ㄱ자 꼭짓점)가 앞장선다", "자는 등 뒤로 접혀 꼬리가 된다"])
    cx, cy = fx + sw / 2 + 50, y2 + sh / 2 + 10
    for j in (3, 2, 1):
        norma_a1(S, cx - j * 34, cy + j * 6, 1.3, ang=math.pi / 4 + math.pi, cls="id", detail=False)
    norma_a1(S, cx, cy, 1.3, ang=math.pi / 4 + math.pi)
    rule(S, cx - 92, cy + 16, math.radians(-10), 70)

    # pane command
    fx = st(3); frame(S, fx, y2, sw, sh, "창 조종 (돌진 등)", "hc", ["ㄱ자 하나가 떨어져 날아가 창 모서리", "괄호가 된다 · 조종 중엔 빨강"])
    cx, cy = fx + 80, y2 + sh / 2 + 50
    side = 64 * 1.2
    T0 = xf(cx, cy, 0.0)
    set_square(S, lambda u, v: T0(u - side / 2, v - side / 2), side * 0.74, 9 * 1.2, "id", inset=False, ticks=False)   # where it left
    T2 = xf(cx, cy, math.pi)
    set_square(S, lambda u, v: T2(u - side / 2, v - side / 2), side * 0.74, 9 * 1.2, "it")
    heart(S, cx, cy, 14 * 1.2)
    px, py, pw, ph = fx + 150, y2 + 50, 160, 120
    pane(S, px, py, pw, ph, "bt")
    S.line("rg", px, py, px + pw, py)
    S.line("rg", px, py, px, py + ph)
    Tp = xf(px, py, 0.0)
    set_square(S, lambda u, v: Tp(u - 6, v - 6), 54, 10, "r", ticks=True, tcls="rd")
    S.line("rd", cx - 30, cy - 30, px - 6, py + 30)

    # phase 2
    fx = st(4); frame(S, fx, y2, sw, sh, "2페이즈", "hv", ["직각자가 풀려 떨어져 궤도를 돈다", "자는 두 동강 · 심장이 커지며 드러남"])
    cx, cy = fx + sw / 2, y2 + sh / 2 + 10
    norma_a1(S, cx, cy, 1.5, ang=0.4, spread=22, heart_open=True)
    S.arc("id", cx, cy, 108, 0, TAU)
    rule(S, cx + 108 * math.cos(-1.9), cy + 108 * math.sin(-1.9), -1.9 + math.pi / 2, 46)
    rule(S, cx + 108 * math.cos(1.2), cy + 108 * math.sin(1.2), 1.2 + math.pi / 2, 46)

    # --- notes
    yn = y2 + sh + 80
    S.text("h", 30, yn, "3. 정할 것")
    S.lines("s", 30, yn + 30, [
        "· 실루엣: A1 / A2 / A3  (A1은 창 괄호와 가장 닮아 \"창을 재단하는 존재\"가 바로 읽힌다)",
        "· 크기: 지금 임시 사각형(반지름 약 24px) → A1 기준 한 변 약 140px 제안",
        "· 자: 1개(1페이즈) → 두 동강(2페이즈) · 조준 연사의 포신 역할",
        "· 색: 기본 하늘색 · 창이나 몸을 '조종'할 때(창 돌진, 본체 돌진 예고)만 빨강",
    ], 24)
    S.save("Norma_Body_Design_A_v1.svg")


board()
print("ok")
