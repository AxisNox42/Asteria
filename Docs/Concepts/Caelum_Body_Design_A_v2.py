"""CAELUM body design A v2: the torn viewfinder, less square, nothing sticking out.

Feedback on A v1 (2026-11-01): keep the torn viewfinder, but it reads too
square, and in sniper form the rail barrel sticks out too far. v2 tries three
ways: a round viewfinder, an octagonal one, and a lens with an iris. The
sniper form now stays inside (or barely past) the body's outline: the aim is
shown by what happens inside the lens, not by a long barrel.
Same rules as v1: lines only, sharp corners, no dashes, no fills, no radial
thorns, 8 health marks, amber. Helpers come from A v1.
Usage: python Caelum_Body_Design_A_v2.py <out_dir>
"""
import math, os, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import (Canvas, COL, HOT, BG, TAU, handle, core, pointer,
                                     shed, font, HEALTH_LIT)


def torn(pts, bands, cy, F):
    """Shifts each point sideways by the band its height falls in (a torn display)."""
    out = []
    for x, y in pts:
        t = (y - (cy - F)) / (2 * F)
        dx = 0.0
        for t0, t1, d in bands:
            if t0 <= t < t1 or (t1 == 1 and t >= 1):
                dx = d
        out.append((x + dx, y))
    return out


def line(cv, pts, w, alpha=1.0, glow=True, col=COL, closed=False):
    n = len(pts) if closed else len(pts) - 1
    for i in range(n):
        cv.seg(pts[i], pts[(i + 1) % len(pts)], w, alpha, glow, col)


def arc_pts(cx, cy, r, a0, a1, step=5.0):
    n = max(6, int(abs(a1 - a0) * r / step))
    return [(cx + math.cos(a0 + (a1 - a0) * i / n) * r, cy + math.sin(a0 + (a1 - a0) * i / n) * r) for i in range(n + 1)]


def torn_arc(cv, cx, cy, r, a0, a1, w, alpha, bands, F, glow=True, col=COL):
    """An arc cut at the band edges: each piece drawn shifted, no connector."""
    pts = arc_pts(cx, cy, r, a0, a1, 3.0)
    run = [pts[0]]
    def band(p):
        t = (p[1] - (cy - F)) / (2 * F)
        for i, (t0, t1, d) in enumerate(bands):
            if t0 <= t < t1:
                return i
        return len(bands) - 1
    for p in pts[1:]:
        if band(p) != band(run[-1]):
            line(cv, torn(run, bands, cy, F), w, alpha, glow, col)
            run = [p]
        else:
            run.append(p)
    line(cv, torn(run, bands, cy, F), w, alpha, glow, col)


def ring_handles(cv, x, y, R, b, s=0.12, offset=TAU / 16, bands=None, F=None):
    for i in range(8):
        a = offset + i * TAU / 8 - math.pi / 2
        px, py = x + math.cos(a) * R, y + math.sin(a) * R
        if bands:
            px, py = torn([(px, py)], bands, y, F)[0]
        handle(cv, px, py, s * b, i in HEALTH_LIT)


def crop_arc_brackets(cv, x, y, R, arm, lw, alpha, bands=None, F=None, turn=0.0):
    """Crop-mark corners bent onto a circle: at four diagonals, an arc arm along
    the ring and a short straight arm inward."""
    for k in range(4):
        a = turn + TAU / 8 + k * TAU / 4
        pts = arc_pts(x, y, R, a - arm, a + arm, 3.0)
        inner = [(x + math.cos(a + s * arm) * R, y + math.sin(a + s * arm) * R) for s in (-1, 1)]
        if bands:
            torn_arc(cv, x, y, R, a - arm, a + arm, lw, alpha, bands, F)
        else:
            line(cv, pts, lw, alpha)
        for (px, py), s in zip(inner, (-1, 1)):
            qx, qy = x + math.cos(a + s * arm) * (R - 0.28 * R), y + math.sin(a + s * arm) * (R - 0.28 * R)
            seg = [(px, py), (qx, qy)]
            if bands:
                seg = torn(seg, bands, y, F)
            cv.seg(seg[0], seg[1], lw, alpha)


# ── A1 round viewfinder ────────────────────────────────────────
def a1_corrupt(cv, x, y, b):
    R = 1.6 * b
    bands = [(0, 0.36, 0), (0.36, 0.6, 0.24 * b), (0.6, 1.01, -0.12 * b)]
    torn_arc(cv, x, y, R, 0, TAU, 1.1, 0.35, bands, R, glow=False)
    crop_arc_brackets(cv, x, y, R, 0.36, 3.0, 1.0, bands, R)
    for r, w, a in ((1.05 * b, 2.0, 0.9), (0.72 * b, 1.1, 0.55), (0.40 * b, 1.6, 0.9)):
        for k in range(4):
            m = k * TAU / 4 + 0.3
            torn_arc(cv, x, y, r, m - 0.6, m + 0.6, w, a, bands, R)
    ring_handles(cv, x, y, R, b, bands=bands, F=R)
    shed(cv, x + 1.15 * b, y + 1.2 * b, 0.55, 0.5, 4, 0.15 * b)
    core(cv, x, y, 0.2 * b)
    pointer(cv, x + 2.15 * b, y + 1.55 * b)


def a1_sniper(cv, x, y, b, aim=-0.5):
    R = 1.4 * b
    cv.arc(x, y, R, 0, TAU, 1.0, 0.3, glow=False)
    crop_arc_brackets(cv, x, y, R, 0.30, 2.6, 0.95, turn=aim - TAU / 8)
    ring_handles(cv, x, y, R, b, s=0.11)
    cv.arc(x, y, 0.98 * b, 0, TAU, 2.4, 1.0)
    cv.arc(x, y, 0.70 * b, 0, TAU, 1.0, 0.5, glow=False)
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    # the barrel stays inside the lens: two short rails from the core to the
    # lens rim, and a notch in the outer ring where it points
    w = 0.14 * b
    for s in (-1, 1):
        cv.seg((x + ux * 0.32 * b + nx * w * s, y + uy * 0.32 * b + ny * w * s),
               (x + ux * 1.10 * b + nx * w * s, y + uy * 1.10 * b + ny * w * s), 2.0, 1.0, col=HOT)
    for s in (-1, 1):
        a = aim + s * 0.16
        cv.seg((x + math.cos(a) * R, y + math.sin(a) * R), (x + math.cos(a) * (R + 0.22 * b), y + math.sin(a) * (R + 0.22 * b)), 2.4, 1.0)
    for k in (1, 2, 3):   # focus ticks on the other three sides
        a = aim + k * TAU / 4
        cv.seg((x + math.cos(a) * 0.55 * b, y + math.sin(a) * 0.55 * b), (x + math.cos(a) * 0.85 * b, y + math.sin(a) * 0.85 * b), 1.6, 0.9)
    core(cv, x, y, 0.22 * b, col=HOT)


# ── A2 octagonal viewfinder ────────────────────────────────────
def octagon(x, y, R, turn=TAU / 16):
    return [(x + math.cos(turn + i * TAU / 8) * R, y + math.sin(turn + i * TAU / 8) * R) for i in range(8)]


def torn_poly(cv, pts, w, alpha, bands, cy, F, glow=True):
    for i in range(len(pts)):
        a, b2 = pts[i], pts[(i + 1) % len(pts)]
        sub = [(a[0] + (b2[0] - a[0]) * k / 24, a[1] + (b2[1] - a[1]) * k / 24) for k in range(25)]
        run = [sub[0]]
        def band(p):
            t = (p[1] - (cy - F)) / (2 * F)
            for j, (t0, t1, d) in enumerate(bands):
                if t0 <= t < t1:
                    return j
            return len(bands) - 1
        for p in sub[1:]:
            if band(p) != band(run[-1]):
                line(cv, torn(run, bands, cy, F), w, alpha, glow)
                run = [p]
            else:
                run.append(p)
        line(cv, torn(run, bands, cy, F), w, alpha, glow)


def a2_corrupt(cv, x, y, b):
    R = 1.7 * b
    bands = [(0, 0.34, 0), (0.34, 0.58, 0.24 * b), (0.58, 1.01, -0.12 * b)]
    oc = octagon(x, y, R)
    torn_poly(cv, oc, 2.6, 1.0, bands, y, R)
    inner = octagon(x, y, R - 0.2 * b)
    torn_poly(cv, inner, 1.0, 0.45, bands, y, R, glow=False)
    for r, w, a in ((1.0 * b, 2.0, 0.9), (0.68 * b, 1.1, 0.55), (0.38 * b, 1.6, 0.9)):
        for k in range(4):
            m = k * TAU / 4 + 0.3
            torn_arc(cv, x, y, r, m - 0.6, m + 0.6, w, a, bands, R)
    for i, (px, py) in enumerate(oc):   # health on the eight corners
        px, py = torn([(px, py)], bands, y, R)[0]
        handle(cv, px, py, 0.12 * b, i in HEALTH_LIT)
    shed(cv, x + 1.2 * b, y + 1.25 * b, 0.55, 0.5, 4, 0.15 * b)
    core(cv, x, y, 0.2 * b)
    pointer(cv, x + 2.2 * b, y + 1.6 * b)


def a2_sniper(cv, x, y, b, aim=-0.5):
    R = 1.45 * b
    # the octagon turns so a corner points at the aim; that corner opens into a
    # short muzzle (two edges pulled in), barely past the outline
    turn = aim
    oc = octagon(x, y, R, turn)
    for i in range(8):
        if i in (0,):
            continue
        a, c = oc[i], oc[(i + 1) % 8]
        if i == 7:   # edge into the muzzle corner: stop short
            c = (a[0] + (c[0] - a[0]) * 0.72, a[1] + (c[1] - a[1]) * 0.72)
        cv.seg(a, c, 2.4, 1.0)
    a, c = oc[0], oc[1]
    cv.seg((a[0] + (c[0] - a[0]) * 0.28, a[1] + (c[1] - a[1]) * 0.28), c, 2.4, 1.0)
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    w = 0.15 * b
    for s in (-1, 1):   # muzzle rails: from inside the lens to just past the corner
        cv.seg((x + ux * 0.4 * b + nx * w * s, y + uy * 0.4 * b + ny * w * s),
               (x + ux * (R + 0.18 * b) + nx * w * s, y + uy * (R + 0.18 * b) + ny * w * s), 2.0, 1.0, col=HOT)
    for i, (px, py) in enumerate(oc):   # health on all eight corners (the muzzle's too)
        handle(cv, px, py, 0.11 * b, i in HEALTH_LIT)
    cv.arc(x, y, 0.95 * b, aim + 0.35, aim + TAU - 0.35, 2.4, 1.0)   # lens, open at the bore
    cv.arc(x, y, 0.66 * b, 0, TAU, 1.0, 0.5, glow=False)
    core(cv, x, y, 0.22 * b, col=HOT)


# ── A3 iris lens ───────────────────────────────────────────────
def iris(cv, x, y, r_out, r_in, turn, blades, w, alpha, col=COL):
    """Aperture blades: each a chord from the outer ring toward the opening."""
    for i in range(blades):
        a = turn + i * TAU / blades
        p0 = (x + math.cos(a) * r_out, y + math.sin(a) * r_out)
        a2 = a + TAU / blades * 1.6
        p1 = (x + math.cos(a2) * r_in, y + math.sin(a2) * r_in)
        cv.seg(p0, p1, w, alpha, col=col)


def a3_corrupt(cv, x, y, b):
    R = 1.6 * b
    bands = [(0, 0.36, 0), (0.36, 0.6, 0.24 * b), (0.6, 1.01, -0.12 * b)]
    for r, w, a in ((R, 2.6, 1.0), (R - 0.18 * b, 1.0, 0.45)):
        for k in range(4):
            m = k * TAU / 4
            torn_arc(cv, x, y, r, m - 0.66, m + 0.66, w, a, bands, R)
    # the iris, open wide and out of step: blades at uneven angles
    for i in range(6):
        a = 0.2 + i * TAU / 6 + (0.18 if i % 2 else 0)
        p0 = (x + math.cos(a) * 1.15 * b, y + math.sin(a) * 1.15 * b)
        a2 = a + TAU / 6 * 1.5
        p1 = (x + math.cos(a2) * 0.62 * b, y + math.sin(a2) * 0.62 * b)
        pp = torn([p0, p1], bands, y, R)
        cv.seg(pp[0], pp[1], 1.6, 0.8)
    torn_arc(cv, x, y, 1.15 * b, 0, TAU, 1.0, 0.4, bands, R, glow=False)
    ring_handles(cv, x, y, R, b, bands=bands, F=R)
    shed(cv, x + 1.15 * b, y + 1.2 * b, 0.55, 0.5, 4, 0.15 * b)
    core(cv, x, y, 0.22 * b)
    pointer(cv, x + 2.15 * b, y + 1.55 * b)


def a3_sniper(cv, x, y, b, aim=-0.5):
    R = 1.4 * b
    cv.arc(x, y, R, 0, TAU, 2.4, 1.0)
    cv.arc(x, y, R - 0.16 * b, 0, TAU, 1.0, 0.45, glow=False)
    ring_handles(cv, x, y, R, b, s=0.11, offset=TAU / 16)
    # the iris closes to a small, even opening: precision. No barrel: the aim
    # is a slot cut through the ring and the closed iris.
    cv.arc(x, y, 1.0 * b, 0, TAU, 1.2, 0.5, glow=False)
    iris(cv, x, y, 1.0 * b, 0.30 * b, aim * 0.5, 8, 1.8, 0.95)
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    w = 0.12 * b
    for s in (-1, 1):
        cv.seg((x + ux * 0.32 * b + nx * w * s, y + uy * 0.32 * b + ny * w * s),
               (x + ux * (R + 0.12 * b) + nx * w * s, y + uy * (R + 0.12 * b) + ny * w * s), 2.2, 1.0, col=HOT)
    core(cv, x, y, 0.20 * b, col=HOT)


DESIGNS = [
    ("A1 둥근 뷰파인더", a1_corrupt, a1_sniper,
     ["틀을 원으로: 자르기 표시가 원 위에서 꺾인다.", "찢김(가로 3단 어긋남)은 원과 렌즈 모두에.",
      "사격: 총열이 렌즈 안에만 있고,", "바깥 고리엔 조준 방향 홈만 튀어나온다."]),
    ("A2 팔각 뷰파인더", a2_corrupt, a2_sniper,
     ["네모 대신 팔각 틀. 꼭짓점 8개 = 체력.", "모서리는 날카롭게 유지.",
      "사격: 꼭짓점 하나가 조준을 향해 열리고,", "짧은 총구가 틀 밖으로 살짝만 나온다."]),
    ("A3 조리개 렌즈", a3_corrupt, a3_sniper,
     ["틀 없이 렌즈와 조리개만. 오염 땐", "조리개 날이 제각각 벌어져 어긋난다.",
      "사격: 조리개가 작게 닫히고(정밀),", "고리를 가르는 좁은 홈이 조준 방향."]),
]


def main():
    out_dir = sys.argv[1]
    W, P = 1800, 470
    H = 150 + 2 * P + 20 + 130
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 22), "CAELUM 몸체 디자인 A v2 · 찢긴 뷰파인더 다듬기", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "v1 피드백: A 방향 유지 · 네모난 느낌 줄이기 · 사격 형태가 너무 튀어나오지 않게. "
           "조준은 긴 총열 대신 렌즈 안의 변화로 보여 준다.", font=font(18), fill=(154, 160, 184))
    d.text((30, 104), "선만 · 채우기/점선/막/방사 가시 없음 · 모서리 날카롭게 · 체력 표시 8개 (여기선 2개 꺼짐) · 호박색 · 커서는 별도 개체",
           font=font(15), fill=(144, 151, 174))
    for i, (name, fc, fs, notes) in enumerate(DESIGNS):
        x0 = 30 + i * (P + 120)
        for row, fn, tag in ((0, fc, "오염 형태"), (1, fs, "사격 형태")):
            cv = Canvas(P, P)
            fn(cv, P / 2 - (10 if row else 0), P / 2 + (10 if row else 0), 80)
            y0 = 150 + row * (P + 10)
            board.paste(cv.image(), (x0, y0))
            d.rectangle([x0, y0, x0 + P, y0 + P], outline=(43, 45, 61), width=2)
            d.text((x0 + 12, y0 + 10), f"{name} · {tag}", font=font(20, True), fill=(255, 209, 128))
        for j, s in enumerate(notes):
            d.text((x0 + 4, 150 + 2 * P + 30 + j * 24), s, font=font(16), fill=(170, 176, 196))
    board.save(os.path.join(out_dir, "Caelum_Body_Design_A_v2.png"))


if __name__ == "__main__":
    main()
