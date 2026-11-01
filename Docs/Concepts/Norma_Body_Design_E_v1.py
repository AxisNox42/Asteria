"""NORMA body design E v1 (SVG): a mechanism whose abilities you read afterwards.

Not a picture of what the boss does. The body is a small machine whose parts
slide, open, shut and detach; the patterns are those parts doing what they
were built to do. Seen idle it is only an odd, quiet mechanism; after the
first slab flies out you understand the rest.
Lines only: no fills, no dashed lines, no plates behind things, no thorn spikes.
Usage: python Norma_Body_Design_E_v1.py <out_dir>
"""
import math, os, sys

OUT = sys.argv[1]
TAU = math.tau

STYLE = """
*{vector-effect:non-scaling-stroke}
.bg{fill:#080910}
.frame{fill:none;stroke:#2b2d3d;stroke-width:1.4}
.i{fill:none;stroke:#b8efff;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:miter}
.it{fill:none;stroke:#b8efff;stroke-width:2.8;stroke-linecap:round;stroke-linejoin:miter}
.id{fill:none;stroke:#4d6a78;stroke-width:1.1;stroke-linecap:round;stroke-linejoin:miter}
.ih{fill:none;stroke:#ebfdff;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.w{fill:none;stroke:#eef1ff;stroke-width:2.2;stroke-linecap:round;stroke-linejoin:miter}
.r{fill:none;stroke:#ff6a5c;stroke-width:2.6;stroke-linecap:round;stroke-linejoin:miter}
.rd{fill:none;stroke:#ff6a5c;stroke-opacity:.4;stroke-width:1.2;stroke-linecap:round;stroke-linejoin:miter}
.c{fill:none;stroke:#34e0f0;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:20px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 24px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.s{font:15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9097ae}
.hc{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.hv{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
"""


class Svg:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.o = w, h, title, []

    def poly(self, cls, pts, close=True):
        d = "M" + " ".join(f"{x:.1f} {y:.1f}" for x, y in pts) + ("Z" if close else "")
        self.o.append(f'<path class="{cls}" d="{d}"/>')

    def line(self, cls, x1, y1, x2, y2):
        self.poly(cls, [(x1, y1), (x2, y2)], close=False)

    def circle(self, cls, x, y, r):
        self.o.append(f'<circle class="{cls}" cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/>')

    def rect(self, cls, x, y, w, h):
        self.o.append(f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

    def text(self, cls, x, y, s):
        s = s.replace("&", "&amp;").replace("<", "&lt;")
        self.o.append(f'<text class="{cls}" x="{x:.1f}" y="{y:.1f}">{s}</text>')

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
    c, n = math.cos(ang), math.sin(ang)
    return lambda x, y: (cx + (x * c - y * n) * s, cy + (x * n + y * c) * s)


def shard(S, x, y, ang, s=7, cls="i"):
    u = (math.cos(ang), math.sin(ang)); v = (-u[1], u[0])
    S.poly(cls, [(x + (px * u[0] + py * v[0]) * s, y + (px * u[1] + py * v[1]) * s)
                 for px, py in ((-.5, -.5), (.5, -.5), (.5, .5), (-.5, .5))])


def core(S, x, y, r, cls="w"):
    S.circle(cls, x, y, r)
    S.circle("ih", x, y, r * 0.35)


def slot(S, T, x0, x1, y, cls="id"):
    S.line(cls, *T(x0, y), *T(x1, y))


def pips(S, T, xs, y, lit, cls="i"):
    """Health pips on a slab: `lit` of them still lit, the rest only an outline."""
    for k, x in enumerate(xs):
        px, py = T(x, y)
        S.circle(cls if k < lit else "id", px, py, 2.6)


# ---------- E1: the shutter ----------
# Four slabs in a pinwheel round a square aperture, each on its own rail.

R0, TK, LEN = 16.0, 30.0, 62.0


def slab(S, T, i, open_, slide=0.0, cls="it", pip_lit=2):
    """Slab i of the pinwheel. open_: aperture half-size; slide: along its rail."""
    a = i * math.pi / 2
    Ti = lambda x, y: T(*( x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)))
    x0, y0 = -R0 + slide, -open_ - TK
    S.poly(cls, [Ti(x0, y0), Ti(x0 + LEN, y0), Ti(x0 + LEN, y0 + TK), Ti(x0, y0 + TK)])
    g = 4.0
    S.poly("ih" if cls == "it" else "id", [Ti(x0 + g, y0 + g), Ti(x0 + LEN - g, y0 + g),
                                          Ti(x0 + LEN - g, y0 + TK - g), Ti(x0 + g, y0 + TK - g)])
    slot(S, Ti, x0 + 10, x0 + LEN - 10, y0 + TK / 2, "id")
    pips(S, Ti, (x0 + LEN * 0.35, x0 + LEN * 0.65), y0 + TK / 2, pip_lit, "i" if cls == "it" else "id")


def norma_e1(S, x, y, s=1.0, state="idle", aim=-0.45):
    ang = aim if state in ("aim", "ram") else 0.0
    T = xf(x, y, ang, s)
    open_ = R0
    slides = [0.0, 0.0, 0.0, 0.0]
    cls = ["it"] * 4
    if state == "aim":
        slides[0] = 46.0                      # the slab facing the target slides out: a barrel
    if state == "ram":
        slides[2] = -34.0                     # the trailing slab draws back along its rail
        cls[2] = "r"
    if state == "hide":
        open_ = 0.0
    for i in range(4):
        slab(S, T, i, open_, slides[i], cls[i], pip_lit=2)
    if state == "hide":
        pass
    else:
        core(S, x, y, 4.2 * s)
        S.poly("id", [T(-R0, -R0), T(R0, -R0), T(R0, R0), T(-R0, R0)])
    if state == "aim":
        for k in range(3):
            shard(S, *T(LEN - R0 + slides[0] + (22 + k * 24), -R0 - TK / 2), ang, 7 * s * (1 - k * .15))
    if state == "ram":
        # where the slab was: the gap it left, and the lane it is about to take
        a = 2 * math.pi / 2
        Ti = lambda px, py: T(px * math.cos(a) - py * math.sin(a), px * math.sin(a) + py * math.cos(a))
        S.poly("rd", [Ti(-R0, -R0 - TK), Ti(-R0 + LEN, -R0 - TK), Ti(-R0 + LEN, -R0), Ti(-R0, -R0)])


# ---------- E2: the gantry ----------
# Two hollow bars crossing, a carriage that runs along them.

def bar(S, T, half, th, cls="it"):
    S.poly(cls, [T(-half, -th), T(half, -th), T(half, th), T(-half, th)])
    S.poly("id", [T(-half + 4, -th + 4), T(half - 4, -th + 4), T(half - 4, th - 4), T(-half + 4, th - 4)])


def norma_e2(S, x, y, s=1.0, state="idle", aim=-0.45):
    ang = aim if state in ("aim", "ram") else 0.0
    T = xf(x, y, ang, s)
    half = 78.0 if state != "hide" else 26.0
    th = 7.0
    cx = 0.0
    cls = "it"
    if state == "aim":
        cx = 58.0
    if state == "ram":
        cx = -52.0
        cls = "r"
    # bars drawn as four arms stopping at the carriage box so nothing crosses it
    c = 21.0
    for sgn in (-1, 1):
        a0, a1 = c, half
        S.poly("i", [T(sgn * a0, -th), T(sgn * a1, -th), T(sgn * a1, th), T(sgn * a0, th)])
        S.poly("i", [T(-th, sgn * a0), T(-th, sgn * a1), T(th, sgn * a1), T(th, sgn * a0)])
        for k in range(1, 6):
            d = c + (half - c) * k / 6
            S.line("id", *T(sgn * d, -th), *T(sgn * d, -th - 4))
            S.line("id", *T(-th - 4, sgn * d), *T(-th, sgn * d)) if False else None
        # end stops
        S.line("it", *T(sgn * half, -th - 8), *T(sgn * half, th + 8))
        S.line("it", *T(-th - 8, sgn * half), *T(th + 8, sgn * half))
    # the carriage
    pts = [T(cx - c, -c), T(cx + c, -c), T(cx + c, c), T(cx - c, c)]
    S.poly(cls, pts)
    S.poly("ih" if cls == "it" else "rd", [T(cx - c + 5, -c + 5), T(cx + c - 5, -c + 5),
                                           T(cx + c - 5, c - 5), T(cx - c + 5, c - 5)])
    core(S, *T(cx, 0), 3.6 * s)
    if state == "aim":
        for k in range(3):
            shard(S, *T(cx + c + 20 + k * 24, 0), ang, 7 * s * (1 - k * .15))
    if state == "ram":
        S.poly("rd", [T(-c, -c), T(c, -c), T(c, c), T(-c, c)])


# ---------- E3: the nesting ----------
# Three frames of one shape set inside each other, each with its own gaps.

def frame_gap(S, T, w, h, gap, cls, rot=0.0):
    """A rectangle outline whose four edges are cut at their middles."""
    pts = [(-w, -h), (w, -h), (w, h), (-w, h)]
    TT = xf(0, 0, rot)
    P = lambda px, py: T(*TT(px, py))
    for k in range(4):
        a, b = pts[k], pts[(k + 1) % 4]
        ln = math.hypot(b[0] - a[0], b[1] - a[1])
        u = ((b[0] - a[0]) / ln, (b[1] - a[1]) / ln)
        m = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        S.poly(cls, [P(*a), P(m[0] - u[0] * gap, m[1] - u[1] * gap)], close=False)
        S.poly(cls, [P(m[0] + u[0] * gap, m[1] + u[1] * gap), P(*b)], close=False)


def norma_e3(S, x, y, s=1.0, state="idle", aim=-0.45):
    ang = aim if state in ("aim", "ram") else 0.0
    T0 = xf(x, y, ang, s)
    sizes = [(78, 54), (56, 39), (36, 25)]
    offs = [0.0, 0.0, 0.0]
    cls = ["it", "i", "i"]
    gaps = [14, 10, 7]
    rots = [0.0, 0.0, 0.0]
    if state == "aim":
        offs[0] = 40.0
    if state == "ram":
        offs[0] = -44.0
        cls[0] = "r"
    if state == "hide":
        sizes = [(36, 25), (36, 25), (36, 25)]    # all three fold down into the smallest
        gaps = [7, 7, 7]
    if state == "idle":
        rots = [0.0, 0.0, 0.0]
    for k in range(3):
        T = (lambda px, py, o=offs[k]: T0(px + o, py))
        w, h = sizes[k]
        frame_gap(S, T, w, h, gaps[k], cls[k], rots[k])
        if k == 0 and state != "hide":
            # the outer frame's rail marks: eight pips, a quarter of its edge each
            for j, px in enumerate((-48, -24, 24, 48)):
                S.circle("i", *T(px, -h), 2.4)
                S.circle("i", *T(px, h), 2.4)
    core(S, *T0(0, 0), 4.2 * s)
    if state == "aim":
        for k in range(3):
            shard(S, *T0(120 + k * 24, 0), ang, 7 * s * (1 - k * .15))
    if state == "ram":
        S.poly("rd", [T0(-78, -54), T0(78, -54), T0(78, 54), T0(-78, 54)])


CANDS = [
    ("E1  셔터", "hc", norma_e1,
     ["슬래브 4장이 풍차처럼 짜인 정사각 조리개.", "슬래브마다 레일 홈과 점 2개. 평소엔 낯선 기계,", "한 장이 레일을 타고 나가는 걸 본 뒤에야 읽힌다."]),
    ("E2  갠트리", "hv", norma_e2,
     ["속 빈 막대 두 개가 십자로 겹치고", "가운데 캐리지가 레일 위를 달린다.", "가장 가벼운 실루엣. 캐리지가 끝으로 달리면 공격."]),
    ("E3  겹틀", "hv", norma_e3,
     ["같은 비율의 틀 세 겹이 서로 안에 들어 있고", "각자 변마다 끊겨 있다. 한 겹씩 떨어져 나가거나", "접혀 들어가는 구조. 바깥 틀이 체력이자 무기."]),
]
STATES = [("aim", "조준 연사"), ("ram", "창 돌진 준비"), ("hide", "숨기·접힘")]


def board():
    W, H = 1800, 1380
    S = Svg(W, H, "NORMA 몸체 디자인 E v1")
    S.text("t1", 30, 56, "NORMA 몸체 디자인 E v1 · 쓰고 나서야 이해되는 몸")
    S.text("t2", 30, 92, "'창처럼 생겼으니 창 패턴'이 아니라 → 부품이 미끄러지고 열리고 접히는 기계. 패턴은 그 부품이 제 일을 하는 순간")
    S.text("s", 30, 120, "선만 · 채우기/점선/막/가시 없음 · 모서리 날카롭게 · 기본 하늘색, 창이나 몸을 '조종'할 때만 빨강")

    S.text("h", 30, 168, "1. 평상시 — 이 상태만 보면 무슨 보스인지 알 수 없다 (그게 의도)")
    fw, fh, y1 = 540, 340, 186
    for i, (tag, cls, fn, cap) in enumerate(CANDS):
        fx = 30 + i * (fw + 30)
        S.rect("frame", fx, y1, fw, fh)
        S.text(cls, fx + 10, y1 + 22, tag)
        S.lines("s", fx, y1 + fh + 22, cap)
        fn(S, fx + fw / 2, y1 + fh / 2 + 10, 1.9, "idle")

    y2 = y1 + fh + 90
    S.text("h", 30, y2, "2. 패턴이 일어나는 순간 — 같은 부품이 하는 일")
    mw, mh = 172, 220
    for i, (tag, cls, fn, cap) in enumerate(CANDS):
        for j, (state, label) in enumerate(STATES):
            fx = 30 + i * (fw + 30) + j * (mw + 12)
            fy = y2 + 18
            S.rect("frame", fx, fy, mw, mh)
            S.text(cls, fx + 8, fy + 20, f"{tag.split()[0]} {label}")
            shift = -22 if state == "aim" else (20 if state == "ram" else 0)
            fn(S, fx + mw / 2 + shift, fy + mh / 2 + 16, 0.82, state)

    y3 = y2 + mh + 70
    S.text("h", 30, y3, "3. 패턴 ↔ 부품 (E1 기준)")
    S.lines("s", 30, y3 + 28, [
        "· 7 조준 연사 — 목표 쪽 슬래브가 레일을 따라 미끄러져 나가 포신이 되고, 그 끝에서 발사",
        "· 10 창 돌진 — 뒤쪽 슬래브가 레일을 거슬러 뒤로 당겨지고(빨강) 빈자리가 돌진 방향을 그대로 가리킨다",
        "· 13 벽 / 15 보충 — 슬래브가 한 장씩 밖으로 밀려나며 늘어나 창이 된다 (몸에서 떨어져 나온 것처럼)",
        "· 14 숨기 — 조리개가 닫히고 슬래브가 겹쳐 하나의 단단한 사각 덩어리가 되고, 곧 사라진다",
        "· 3 창 닫기 카운트 — 조리개가 조여들며 닫힌다",
        "· 2페이즈 — 슬래브가 얇은 두 겹으로 갈라지고(8장) 조리개가 열린 채 코어가 드러난다",
        "· 체력 — 슬래브마다 점 2개 = 8칸. 맞을 때마다 하나씩 빈 테두리가 된다",
    ], 24)
    S.text("h", 30, y3 + 230, "4. 정할 것")
    S.lines("s", 30, y3 + 258, [
        "· 실루엣: E1(조리개) / E2(갠트리) / E3(겹틀) — 전부 '창'이라는 단어가 몸에 없고, 패턴의 동작만 몸에 들어 있다",
        "· 크기·판정·창 괄호 고리·빨강 규칙은 지금 값 그대로, 그리는 모양만 교체",
    ], 24)
    S.save("Norma_Body_Design_E_v1.svg")


board()
print("ok")
