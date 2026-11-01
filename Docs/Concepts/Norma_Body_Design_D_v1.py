"""NORMA body design D v1 (SVG): a body that reads as a WINDOW, not a tesseract.

The boss is the window boss and its name is the carpenter's square (Norma et
Regula). Candidates reuse the helpers of design A (set squares, rule, heart,
shards) and add window cues: pane corner brackets, a title bar with its three
buttons, cascaded panes, a pane cut by a diagonal into two set triangles.
Lines only: no fills, no dashed lines, no plates, no thorn spikes.
Usage: python Norma_Body_Design_D_v1.py <out_dir>
"""
import math, os, sys

src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "Norma_Body_Design_A_v1.py"),
           encoding="utf-8").read()
exec(src[:src.index("def board():")])   # STYLE, Svg, xf, set_square, heart, rule, shard, player, frame


def corner_bracket(S, cx, cy, sx, sy, arm, th, cls="it"):
    """A pane corner bracket whose outer corner sits at (cx, cy); sx, sy = -1/+1
    say which corner of the pane it is (arms run inward)."""
    T = lambda u, v: (cx - sx * u, cy - sy * v)
    set_square(S, T, arm, th, cls, ticks=False)


def window_buttons(S, x, y, s, lit=False):
    """Minimise, maximise, close glyphs, right-aligned at (x, y)."""
    c = "w" if lit else "i"
    S.line(c, x - 44 * s, y, x - 34 * s, y)                                   # minimise
    S.rect(c, x - 27 * s, y - 5 * s, 10 * s, 10 * s)                          # maximise
    S.line(c, x - 9 * s, y - 5 * s, x + 1 * s, y + 5 * s)                     # close
    S.line(c, x - 9 * s, y + 5 * s, x + 1 * s, y - 5 * s)


# ---------- D1: the window frame ----------

def norma_d1(S, x, y, s=1.0, state="idle", aim=-0.5):
    W, H = 176 * s, 128 * s
    spread = 20 * s if state == "p2" else 0.0
    arm, th = 44 * s, 8 * s
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        d = spread
        if state == "fire" and (sx, sy) == (1, -1):
            continue                                          # this corner becomes the barrel
        corner_bracket(S, x + sx * (W / 2 + d), y + sy * (H / 2 + d), sx, sy, arm, th,
                       "i" if state == "p2" else "it")
    # inner contour and the title bar
    iw, ih = W / 2 - 12 * s, H / 2 - 12 * s
    ty = y - H / 2 + 24 * s
    if state == "p2":
        S.line("id", x - iw, ty, x - 24 * s, ty)
        S.line("id", x + 24 * s, ty, x + iw, ty)
    else:
        S.line("i", x - iw, ty, x + iw, ty)
    S.poly("id", [(x - iw, y - ih), (x + iw, y - ih), (x + iw, y + ih), (x - iw, y + ih)])
    S.circle("i", x - iw + 12 * s, y - H / 2 + 12 * s, 3.2 * s)               # the window's icon
    window_buttons(S, x + iw - 6 * s, y - H / 2 + 12 * s, s, lit=(state == "p2"))
    heart(S, x, y + 14 * s, 17 * s, "it", ang=0.0, open_=(state == "p2"))
    if state == "fire":
        # the top-right bracket stretches into a ruled barrel pointing at the target
        cx, cy = x + W / 2, y - H / 2
        T = xf(cx, cy, aim)
        S.poly("it", [T(0, -th / 2), T(92 * s, -th / 2), T(92 * s, th / 2), T(0, th / 2)])
        for k in range(1, 14):
            S.line("ih" if k % 5 == 0 else "id", *T(k * 6.2 * s, -th / 2), *T(k * 6.2 * s, -th / 2 + th * (0.7 if k % 5 == 0 else 0.35)))
        for k in range(3):
            shard(S, *T((112 + k * 26) * s, 0), aim, 7 * s * (1 - k * .15), "i")


# ---------- D2: cascaded panes ----------

def norma_d2(S, x, y, s=1.0, state="idle", aim=-0.5):
    spread = {"idle": 1.0, "fire": 1.0, "p2": 1.7}[state]
    n = 3
    for k in reversed(range(n)):                               # back panes first
        W, H = (150 - k * 18) * s, (108 - k * 14) * s
        ox, oy = (k - 1) * 26 * s * spread, -(k - 1) * 19 * s * spread
        if state == "fire":
            ox += k * 30 * s * math.cos(aim) * 0.9
            oy += k * 30 * s * math.sin(aim) * 0.9
        ang = (k - 1) * 0.10 * (spread - 0.5) if state == "p2" else 0.0
        T = xf(x + ox, y + oy, ang)
        cls = ("it", "i", "id")[k]
        arm, th = W * 0.46, (8 if k == 0 else 6) * s
        set_square(S, lambda u, v, T=T, W=W, H=H: T(-W / 2 + u, -H / 2 + v), arm, th, cls,
                   inset=(k == 0), ticks=(k == 0))
        set_square(S, lambda u, v, T=T, W=W, H=H: T(W / 2 - u, H / 2 - v), arm, th, cls,
                   inset=(k == 0), ticks=(k == 0))
        if k == 0:
            S.line("i", *T(-W / 2 + th + 6 * s, -H / 2 + 20 * s), *T(W / 2 - th - 6 * s, -H / 2 + 20 * s))
    heart(S, x, y, 15 * s, "it", open_=(state == "p2"))
    if state == "fire":
        for k in range(3):
            shard(S, x + (110 + k * 30) * s * math.cos(aim), y + (110 + k * 30) * s * math.sin(aim), aim, 7 * s, "i")


# ---------- D3: a pane cut in two set triangles ----------

def tri(S, pts, s, ticks_on, cls="it"):
    S.poly(cls, pts)
    gx = sum(p[0] for p in pts) / 3
    gy = sum(p[1] for p in pts) / 3
    ins = [(gx + (px - gx) * 0.86, gy + (py - gy) * 0.86) for px, py in pts]
    S.poly("ih", ins)
    for (a, b) in ticks_on:
        p, q = pts[a], pts[b]
        ln = math.hypot(q[0] - p[0], q[1] - p[1])
        ux, uy = (q[0] - p[0]) / ln, (q[1] - p[1]) / ln
        nx, ny = -uy, ux
        if (gx - p[0]) * nx + (gy - p[1]) * ny > 0:
            nx, ny = -nx, -ny                                  # point away from the centre
        for k in range(1, int(ln / 8 / s)):
            d = k * 8 * s
            L = (7 if k % 4 == 0 else 3.5) * s
            S.line("id", p[0] + ux * d, p[1] + uy * d, p[0] + ux * d + nx * L, p[1] + uy * d + ny * L)


def norma_d3(S, x, y, s=1.0, state="idle", aim=-0.5):
    W, H = 168 * s, 112 * s
    gap = {"idle": 7.0, "fire": 7.0, "p2": 26.0}[state] * s
    ang = 0.0
    tl, tr, br, bl = (-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)
    # the cut runs top-left -> bottom-right; each half slides off its normal
    dx, dy = br[0] - tl[0], br[1] - tl[1]
    dl = math.hypot(dx, dy)
    nx, ny = -dy / dl, dx / dl
    T = xf(x, y, ang)
    up = [tl, tr, br]
    lo = [tl, br, bl]
    up = [T(px + nx * gap, py + ny * gap) for px, py in up]
    lo = [T(px - nx * gap, py - ny * gap) for px, py in lo]
    tri(S, up, s, [(0, 1), (1, 2)])
    tri(S, lo, s, [(2, 0), (0, 1)] if False else [(1, 2), (2, 0)])
    # the title bar rides the upper half, with its close mark in the right angle
    tb0, tb1 = T(-W / 2 + 18 * s + nx * gap, -H / 2 + 20 * s + ny * gap), T(W / 2 - 40 * s + nx * gap, -H / 2 + 20 * s + ny * gap)
    S.line("i", *tb0, *tb1)
    cx, cy = T(W / 2 - 22 * s + nx * gap, -H / 2 + 20 * s + ny * gap)
    S.line("w" if state == "p2" else "i", cx - 5 * s, cy - 5 * s, cx + 5 * s, cy + 5 * s)
    S.line("w" if state == "p2" else "i", cx - 5 * s, cy + 5 * s, cx + 5 * s, cy - 5 * s)
    heart(S, x, y, 16 * s, "it", ang=ang, open_=(state == "p2"))
    if state == "fire":
        da = math.atan2(H, W)          # the cut itself is the barrel
        hx, hy = T(W / 2, H / 2)
        for k in range(3):
            shard(S, hx + (22 + k * 26) * s * math.cos(da), hy + (22 + k * 26) * s * math.sin(da), da, 7 * s, "i")


CANDS = [
    ("D1  창틀", "hc", norma_d1,
     ["창 모서리 괄호 4개가 사각 틀을 이루고 안에 제목 줄과", "─ □ ✕ 버튼, 창 아이콘. 한눈에 '창' · 심장은 레굴루스의 사각"]),
    ("D2  겹창", "hv", norma_d2,
     ["창 세 장이 비스듬히 겹친 캐스케이드. 벽 패턴의 겹창과", "같은 모양이라 패턴과 외형이 이어진다 · 앞 창만 굵게"]),
    ("D3  갈라진 창", "hv", norma_d3,
     ["창 하나를 대각선으로 갈라 삼각자 두 장으로. 직각자(Norma)가", "그대로 읽히고, 2페이즈에 벌어져 심장이 드러난다"]),
]


def board():
    W, H = 1800, 1240
    S = Svg(W, H, "NORMA 몸체 디자인 D v1")
    S.text("t1", 30, 56, "NORMA 몸체 디자인 D v1 · 창으로 읽히는 몸")
    S.text("t2", 30, 92, "테서랙트는 창·직각자와 무관하다 → 몸을 '창'(괄호·제목 줄·겹창)과 '직각자'(Norma)에서 다시 뽑는다")
    S.text("s", 30, 120, "선만 · 채우기/점선/막/가시 없음 · 모서리 날카롭게 · 밀도는 안쪽 윤곽선과 눈금으로 · 기본 하늘색, 창을 '조종'할 때만 빨강")

    S.text("h", 30, 168, "1. 실루엣 후보 (평상시)")
    fw, fh, y1 = 540, 340, 186
    for i, (tag, cls, fn, cap) in enumerate(CANDS):
        fx = 30 + i * (fw + 30)
        frame(S, fx, y1, fw, fh, tag, cls, cap)
        fn(S, fx + fw / 2, y1 + fh / 2 + 12, 1.55, "idle")

    y2 = y1 + fh + 100
    S.text("h", 30, y2, "2. 후보마다: 조준 연사 · 2페이즈")
    mw, mh = 264, 250
    for i, (tag, cls, fn, cap) in enumerate(CANDS):
        for j, (state, label) in enumerate((("fire", "조준 연사"), ("p2", "2페이즈"))):
            fx = 30 + i * (fw + 30) + j * (mw + 12)
            fy = y2 + 18
            S.rect("frame", fx, fy, mw, mh)
            S.text(cls, fx + 10, fy + 22, f"{tag.split()[0]} · {label}")
            fn(S, fx + mw / 2 - (34 if state == "fire" and i != 2 else 0), fy + mh / 2 + 24, 0.9, state)
    S.text("h", 30, y2 + mh + 60, "3. 멀리서 보기 (실제 화면 크기의 절반) · 4. 정할 것")
    y3 = y2 + mh + 74
    for i, (tag, cls, fn, cap) in enumerate(CANDS):
        fx = 30 + i * (fw + 30)
        S.rect("frame", fx, y3, 200, 120)
        S.text(cls, fx + 10, y3 + 20, tag.split()[0])
        fn(S, fx + 100, y3 + 70, 0.55, "idle")
    S.lines("s", 30 + 3 * 0, y3 + 160, [
        "· 실루엣: D1 / D2 / D3  (D1이 가장 직관적으로 '창', D3이 이름(직각자)과 2페이즈 연출이 가장 선명)",
        "· 체력: 지금처럼 몸에 표시할지(D1 모서리 괄호 4개 × 2칸 = 8칸, D2 창 3장, D3 삼각자 눈금 8칸) 정한다",
        "· 창 괄호 고리·빨강 규칙·크기·판정은 지금 값 그대로 쓰고, 그리는 모양만 바꾼다",
    ], 24)
    S.save("Norma_Body_Design_D_v1.svg")


board()
print("ok")
