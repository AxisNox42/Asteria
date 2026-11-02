"""ERIDANUS (CAELUM) boss bar design v1: a health bar of its own.

Today both bosses share one bar (a dim line, a bright share, the name above,
a white edge while phase 2 refills). ERIDANUS should get a bar that reads as
itself: the river its puddles flow into, the cursor that drags them, the
viewfinder body. Three directions, each shown in four moments: phase 1 full,
phase 1 almost drained, phase 2 refilling, phase 2 in play.
Same rules as the body boards: lines only, no fills, no dashes, no plates,
no thorns; amber. Helpers come from Caelum_Body_Design_A_v1.
Usage: python Eridanus_BossBar_Design_v1.py <out_dir>
"""
import math, os, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, COL, HOT, BG, TAU, font, core

PW, PH = 430, 190          # one moment's panel
BW = 360                   # bar width in a panel
DIM = 0.22


def name_text(d, x, y, s, col=COL):
    d.text((x, y), s, font=font(17, True), fill=col)


# ── current bar (for reference) ────────────────────────────────
def cur(cv, x0, y, share, refill):
    cv.seg((x0, y), (x0 + BW, y), 2.0, DIM, glow=False)
    if share > 0:
        cv.seg((x0, y), (x0 + BW * share, y), 3.0, 0.95)
    if refill > 0:
        e = x0 + BW * share
        cv.seg((e - 3, y - 4), (e - 3, y + 4), 2.0, refill, col=(255, 255, 255))


# ── A: river ───────────────────────────────────────────────────
def river_pts(x0, x1, y, amp, phase, step=4.0):
    pts = []
    n = max(2, int((x1 - x0) / step))
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i / n
        pts.append((x, y + amp * math.sin(x * 0.045 + phase)))
    return pts


def poly_open(cv, pts, w, a, glow=True, col=COL):
    for i in range(len(pts) - 1):
        cv.seg(pts[i], pts[i + 1], w, a, glow, col)


def a_bar(cv, x0, y, share, refill, t=0.0):
    e = x0 + BW * share
    # the dry bed: one dim straight line where the water has gone
    cv.seg((e, y), (x0 + BW, y), 1.4, DIM, glow=False)
    if share <= 0:
        return
    # the water: two banks that meander together, pinching at the source
    for s in (-1, 1):
        pts = river_pts(x0, e, y, 2.2, t + (0.9 if s > 0 else 0.0))
        pts = [(px, py + s * 4.0 * min(1.0, (px - x0) / 24.0)) for px, py in pts]
        poly_open(cv, pts, 1.6, 0.95)
    # the current: a brighter thread down the middle
    poly_open(cv, river_pts(x0, e, y, 1.2, t + 2.0), 1.0, 0.55, glow=False)
    # the front: a rounded puddle edge (a half ellipse), white while refilling
    col = (255, 255, 255) if refill > 0 else COL
    a = refill if refill > 0 else 0.9
    pts = [(e + 7 * math.cos(u), y + 6 * math.sin(u)) for u in [(-math.pi / 2 + math.pi * i / 10) for i in range(11)]]
    poly_open(cv, pts, 1.8, max(a, 0.6), col=col)
    if refill > 0:   # the water rushing in: two short streaks behind the front
        for k, dy in ((14, -2), (26, 2)):
            cv.seg((e - k - 10, y + dy), (e - k, y + dy), 1.2, refill * 0.8, col=HOT)


# ── B: selection box with handles ─────────────────────────────
def b_bar(cv, x0, y, share, refill, phase, lit):
    e = x0 + BW * share
    h = 7.0
    cv.seg((x0, y), (x0 + BW, y), 1.2, DIM, glow=False)
    if share > 0:
        cv.seg((x0, y), (e, y), 3.0, 0.95)
        # the selection the cursor holds: crop marks at its four corners
        for cx, sx in ((x0, 1), (e, -1)):
            for cy, sy in ((y - h, 1), (y + h, -1)):
                cv.seg((cx, cy), (cx + sx * 9, cy), 1.6, 0.8)
                cv.seg((cx, cy), (cx, cy + sy * 5), 1.6, 0.8)
    # the handles: one per share of the phase (5 in phase 1, 3 in phase 2),
    # the same squares the body wears; they go dark as the share runs out
    n = 5 if phase == 1 else 3
    for i in range(n):
        hx = x0 + BW * (i + 0.5) / n
        on = i < lit
        s = 4.0
        cv.box(hx - s, y + 14 - s, 2 * s, 2 * s, 1.6 if on else 1.0, 1.0 if on else 0.22)
        if on:
            cv.box(hx - 1.8, y + 14 - 1.8, 3.6, 3.6, 1.0, 0.8, glow=False)
    # the cursor drags the selection's edge (an arrow, as in the fight)
    if share > 0:
        sc = 0.75
        P = [(0, 0), (0, 26), (7, 20), (12, 31), (17, 29), (12, 18), (21, 18)]
        pts = [(e + 2 + px * sc, y - 2 + py * sc) for px, py in P]
        cv.poly(pts, 1.6, 1.0, col=HOT if refill > 0 else COL)


# ── C: viewfinder ruler ───────────────────────────────────────
def c_bar(cv, x0, y, share, refill, phase):
    e = x0 + BW * share
    # the lens at the source: the body's core in small (ring + crosshair) and
    # one lens arc per phase still to come
    lx = x0 - 22
    core(cv, lx, y, 9)
    for k in range(4):
        m = k * TAU / 4 + 0.3
        cv.arc(lx, y, 15, m - 0.5, m + 0.5, 1.2, 0.7 if phase == 1 else 0.35)
    # the ruler: a fine baseline with ticks every sixteenth, tall at quarters
    cv.seg((x0, y), (x0 + BW, y), 1.0, DIM, glow=False)
    for i in range(17):
        tx = x0 + BW * i / 16
        tall = 6 if i % 4 == 0 else 3
        on = tx <= e + 0.5
        cv.seg((tx, y - tall), (tx, y), 1.2 if on else 1.0, 0.85 if on else DIM, glow=on)
    if share > 0:
        cv.seg((x0, y + 4), (e, y + 4), 3.0, 0.95)
        # the focus bracket rides the end: a round bracket like the lens rim
        col = (255, 255, 255) if refill > 0 else HOT
        cv.arc(e, y + 1, 10, -math.pi / 2 + 0.3, math.pi / 2 - 0.3, 1.8, 1.0, col=col)


MOMENTS = [
    ("1페이즈 · 70%", 1, 0.70, 0.0),
    ("1페이즈 · 거의 다 깎임", 1, 0.06, 0.0),
    ("전환 · 새 바 차오르는 중", 2, 0.55, 0.7),
    ("2페이즈 · 60%", 2, 0.60, 0.0),
]

DESIGNS = [
    ("지금 (두 보스 공용)", "cur",
     ["가는 선 + 밝은 몫 + 이름. 차오를 때 흰 끝선.", "ERIDANUS만의 것은 색(호박색)뿐."]),
    ("A 물길", "a",
     ["체력 = 흐르는 물길: 두 물가 선이 같이 굽이치고,", "가운데 물살 선. 깎인 곳은 마른 바닥(희미한 직선).",
      "끝은 웅덩이처럼 둥근 물머리. 2페이즈엔 물이", "왼쪽에서 다시 밀려 들어온다(흰 물머리 + 물살)."]),
    ("B 선택 상자", "b",
     ["체력 = 커서가 드래그로 잡은 선택 영역:", "양 끝 네 모서리에 자르기 표시, 끝엔 커서 화살표.",
      "아래 네모 핸들 = 몸의 핸들(1페이즈 5개, 2페이즈 3개).", "2페이즈엔 커서가 상자를 다시 끝까지 끌어 연다."]),
    ("C 렌즈 눈금자", "c",
     ["왼쪽 끝에 몸의 코어(고리+십자)와 렌즈 호.", "바는 1/16 눈금자, 끝에 둥근 초점 괄호.",
      "남은 페이즈만큼 렌즈 호가 밝다.", "사격 형태와 가장 잘 맞지만, 물/오염 느낌은 없다."]),
]


def draw(kind, cv, x0, y, phase, share, refill):
    if kind == "cur":
        cur(cv, x0, y, share, refill)
    elif kind == "a":
        a_bar(cv, x0, y, share, refill, t=1.3 * phase)
    elif kind == "b":
        n = 5 if phase == 1 else 3
        lit = math.ceil(share * n - 1e-6) if refill == 0 else int(share * n)
        b_bar(cv, x0, y, share, refill, phase, lit)
    else:
        c_bar(cv, x0, y, share, refill, phase)


def main():
    out_dir = sys.argv[1]
    LW = 380
    W = LW + 4 * (PW + 10) + 20
    H = 150 + len(DESIGNS) * (PH + 14) + 20
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 22), "ERIDANUS 전용 체력바 v1", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "에리다누스 = 하늘의 강. 웅덩이가 흘러 이어지는 보스, 커서가 끄는 오염, 둥근 뷰파인더 몸. "
           "페이즈마다 따로 차는 바(1페이즈 60% / 2페이즈 40%).", font=font(18), fill=(154, 160, 184))
    d.text((30, 104), "선만 · 채우기/점선/막/가시 없음 · 호박색 · 화면 위쪽 가운데, 너비 = 화면의 36% (지금과 같은 자리)",
           font=font(15), fill=(144, 151, 174))
    for j, (label, *_rest) in enumerate(MOMENTS):
        d.text((LW + 20 + j * (PW + 10) + 10, 128), label, font=font(15, True), fill=(170, 176, 196))
    for i, (title, kind, notes) in enumerate(DESIGNS):
        y0 = 150 + i * (PH + 14)
        d.text((30, y0 + 6), title, font=font(22, True), fill=COL)
        for k, s in enumerate(notes):
            d.text((30, y0 + 44 + k * 22), s, font=font(13), fill=(170, 176, 196))
        for j, (_, phase, share, refill) in enumerate(MOMENTS):
            x = LW + 20 + j * (PW + 10)
            cv = Canvas(PW, PH)
            bx = (PW - BW) / 2 + (12 if kind == "c" else 0)
            draw(kind, cv, bx, PH / 2 + 12, phase, share, refill)
            board.paste(cv.image(), (x, y0))
            name = "ERIDANUS  II" if phase == 2 else "ERIDANUS"
            name_text(d, x + bx, y0 + PH / 2 - 26 - (8 if kind == "c" else 0), name)
            d.rectangle([x, y0, x + PW, y0 + PH], outline=(43, 45, 61), width=2)
    board.save(os.path.join(out_dir, "Eridanus_BossBar_Design_v1.png"))


if __name__ == "__main__":
    main()
