"""CAELUM body design A v1: one body that both corrupts and shoots fast.

CAELUM has two forms in play: the corrupt form lays corruption on the grid
(its cursor clicks, drags and deletes), the sniper form fires very fast single
shots. The body has to read as both, and the change between them is the
moment the player must notice (sniper form = no other warning than the
cursor). Each direction is shown in both forms. Lines only, sharp corners, no
dashes, no fills, no radial thorns; 8 health marks. Amber.
Usage: python Caelum_Body_Design_A_v1.py <out_dir>
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

TAU = math.tau
SS = 3
COL = (255, 209, 128)        # CAELUM amber (1.0, 0.82, 0.50)
HOT = (255, 242, 210)        # near white (danger)
BG = (6, 7, 12)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.glow = Image.new("RGB", (w * SS, h * SS), (0, 0, 0))
        self.line = Image.new("RGB", (w * SS, h * SS), (0, 0, 0))
        self.gd, self.ld = ImageDraw.Draw(self.glow), ImageDraw.Draw(self.line)

    def seg(self, a, b, w, alpha=1.0, glow=True, col=COL):
        c = tuple(int(v * alpha) for v in col)
        pa, pb = (a[0] * SS, a[1] * SS), (b[0] * SS, b[1] * SS)
        self.ld.line([pa, pb], fill=c, width=max(1, int(w * SS)))
        if glow:
            self.gd.line([pa, pb], fill=c, width=max(1, int((w + 3) * SS)))

    def poly(self, pts, w, alpha=1.0, glow=True, col=COL):
        for i in range(len(pts)):
            self.seg(pts[i], pts[(i + 1) % len(pts)], w, alpha, glow, col)

    def arc(self, cx, cy, r, a0, a1, w, alpha=1.0, glow=True, col=COL):
        steps = max(6, int(abs(a1 - a0) * r / 6))
        pts = [(cx + math.cos(a0 + (a1 - a0) * i / steps) * r,
                cy + math.sin(a0 + (a1 - a0) * i / steps) * r) for i in range(steps + 1)]
        for i in range(steps):
            self.seg(pts[i], pts[i + 1], w, alpha, glow, col)

    def box(self, x, y, w, h, lw, alpha=1.0, glow=True, col=COL):
        self.poly([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], lw, alpha, glow, col)

    def image(self):
        g = self.glow.filter(ImageFilter.GaussianBlur(2.5 * SS)).point(lambda v: int(v * 0.55))
        out = Image.new("RGB", self.glow.size, BG)
        out = ImageChops.screen(ImageChops.screen(out, g), self.line)
        return out.resize((self.w, self.h), Image.LANCZOS)


def rot(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c)


def corners(cv, x, y, w, h, arm, lw, alpha=1.0, col=COL):
    for (cx, cy, sx, sy) in ((x, y, 1, 1), (x + w, y, -1, 1), (x, y + h, 1, -1), (x + w, y + h, -1, -1)):
        cv.seg((cx, cy), (cx + sx * arm, cy), lw, alpha, col=col)
        cv.seg((cx, cy), (cx, cy + sy * arm), lw, alpha, col=col)


def handle(cv, x, y, s, lit=True):
    cv.box(x - s, y - s, 2 * s, 2 * s, 2.0 if lit else 1.0, 1.0 if lit else 0.25)
    if lit:
        cv.box(x - s * 0.45, y - s * 0.45, 0.9 * s, 0.9 * s, 1.2, 0.8, glow=False)


def core(cv, x, y, r, col=COL):
    cv.arc(x, y, r, 0, TAU, 2.0, 1.0, col=col)
    cv.seg((x - r * .55, y), (x + r * .55, y), 1.4, 0.9, col=col)
    cv.seg((x, y - r * .55), (x, y + r * .55), 1.4, 0.9, col=col)


def pointer(cv, x, y, sc=1.3):
    P = [(0, 0), (0, 26), (7, 20), (12, 31), (17, 29), (12, 18), (21, 18)]
    pts = [(x + px * sc, y + py * sc) for px, py in P]
    cv.poly([(p[0] + 4, p[1] + 1) for p in pts], 1.1, 0.35, glow=False)
    cv.poly(pts, 2.0, 1.0, col=HOT)


def shed(cv, x, y, dx, dy, n, s):
    """Hollow pixels drifting off a corner (corruption leaking out)."""
    for i in range(n):
        k = i + 1
        px, py = x + dx * k * 1.35 * s + (i % 2) * s * 0.6, y + dy * k * 1.35 * s
        cv.box(px, py, s * (1 - i * 0.12), s * (1 - i * 0.12), 1.2, 0.9 - i * 0.17, glow=i < 2)


HEALTH_LIT = (0, 1, 2, 4, 5, 7)     # two of eight dark, to show the read


# ── A: torn viewfinder ─────────────────────────────────────────
def torn_frame(cv, x, y, F, bands, lw, alpha):
    """A square frame cut into horizontal bands, each shifted sideways (a torn
    display). bands: [(t0, t1, dx)] with t in 0..1 from top to bottom."""
    for t0, t1, dx in bands:
        y0, y1 = y - F + 2 * F * t0, y - F + 2 * F * t1
        cv.seg((x - F + dx, y0), (x - F + dx, y1), lw, alpha)
        cv.seg((x + F + dx, y0), (x + F + dx, y1), lw, alpha)
        if t0 == 0:
            cv.seg((x - F + dx, y0), (x + F + dx, y0), lw, alpha)
        if t1 == 1:
            cv.seg((x - F + dx, y1), (x + F + dx, y1), lw, alpha)
        # the tear itself: a short step where two bands meet
    for (a0, a1, d0), (b0, b1, d1) in zip(bands, bands[1:]):
        yy = y - F + 2 * F * a1
        cv.seg((x - F + d0, yy), (x - F + d1, yy), lw * 0.7, alpha * 0.8)
        cv.seg((x + F + d0, yy), (x + F + d1, yy), lw * 0.7, alpha * 0.8)


def a_corrupt(cv, x, y, b):
    F = 1.55 * b
    bands = [(0, 0.34, 0), (0.34, 0.58, 0.22 * b), (0.58, 1, -0.10 * b)]
    torn_frame(cv, x, y, F, bands, 1.2, 0.35)
    # crop corners follow the bands they sit in
    arm = 0.62 * b
    for sx, sy, dx in ((-1, -1, 0), (1, -1, 0), (-1, 1, -0.10 * b), (1, 1, -0.10 * b)):
        cx, cy = x + sx * F + dx, y + sy * F
        cv.seg((cx, cy), (cx - sx * arm, cy), 3.0, 1.0)
        cv.seg((cx, cy), (cx, cy - sy * arm), 3.0, 1.0)
    # lens: three broken rings, the inner one slipped sideways with the middle band
    for r, w, a, off in ((1.05 * b, 2.0, 0.9, 0.0), (0.78 * b, 1.2, 0.6, 0.22 * b), (0.42 * b, 1.6, 0.9, 0.0)):
        for k in range(4):
            m = k * TAU / 4 + 0.3
            cv.arc(x + off, y, r, m - 0.58, m + 0.58, w, a)
    # health: 8 handles on the frame (corners + mids)
    pos = [(-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
    for i, (hx, hy) in enumerate(pos):
        dx = 0.22 * b if hy == 0 else (-0.10 * b if hy == 1 else 0)
        handle(cv, x + hx * F + dx, y + hy * F, 0.13 * b, i in HEALTH_LIT)
    shed(cv, x + F - 0.1 * b, y + F, 0.55, 0.5, 4, 0.16 * b)
    core(cv, x, y, 0.2 * b)
    pointer(cv, x + 2.2 * b, y + 1.6 * b)


def a_sniper(cv, x, y, b, aim=-0.5):
    # The frame snaps back into one piece and pulls in: in focus.
    F = 1.25 * b
    cv.box(x - F, y - F, 2 * F, 2 * F, 1.0, 0.25)
    corners(cv, x - F, y - F, 2 * F, 2 * F, 0.38 * b, 2.4, 0.9)
    pos = [(-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
    for i, (hx, hy) in enumerate(pos):
        handle(cv, x + hx * F, y + hy * F, 0.12 * b, i in HEALTH_LIT)
    # lens rings closed and turned to the aim; focus ticks; a long rail barrel
    cv.arc(x, y, 1.0 * b, 0, TAU, 2.4, 1.0)
    cv.arc(x, y, 0.72 * b, 0, TAU, 1.0, 0.5, glow=False)
    for k in range(4):
        a = aim + k * TAU / 4
        r0, r1 = (0.55 * b, 1.35 * b) if k else (1.0 * b, 1.0 * b)
        if k:
            cv.seg((x + math.cos(a) * r0, y + math.sin(a) * r0), (x + math.cos(a) * r1, y + math.sin(a) * r1), 1.8, 0.95)
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    w = 0.16 * b
    for s in (-1, 1):
        cv.seg((x + ux * 1.0 * b + nx * w * s, y + uy * 1.0 * b + ny * w * s),
               (x + ux * 2.9 * b + nx * w * s, y + uy * 2.9 * b + ny * w * s), 2.0, 1.0, col=HOT)
    # muzzle: a crop-corner pair at the barrel's end
    mx, my = x + ux * 2.9 * b, y + uy * 2.9 * b
    for s in (-1, 1):
        px, py = mx + nx * w * 2.2 * s, my + ny * w * 2.2 * s
        cv.seg((px, py), (px - ux * 0.35 * b, py - uy * 0.35 * b), 2.0, 1.0)
        cv.seg((px, py), (px - nx * w * 1.1 * s, py - ny * w * 1.1 * s), 2.0, 1.0)
    core(cv, x, y, 0.22 * b, col=HOT)


# ── B: pixel matrix ────────────────────────────────────────────
def cell(cv, x, y, s, lit, glitch=0.0, lw=2.0):
    a = 1.0 if lit else 0.25
    cv.box(x - s, y - s, 2 * s, 2 * s, lw if lit else 1.0, a)
    if lit:
        cv.box(x - s * 0.62, y - s * 0.62, 1.24 * s, 1.24 * s, 1.0, 0.5, glow=False)
    if glitch:   # a doubled, slipped outline: this cell is corrupting
        cv.box(x - s + glitch, y - s - glitch * 0.3, 2 * s, 2 * s, 1.0, 0.45, glow=False)


def b_corrupt(cv, x, y, b):
    s, gap = 0.42 * b, 0.12 * b
    step = 2 * s + gap
    # 3x3: eight outer cells = health, the centre is the core; two cells slipped
    order = [(-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
    slip = {2: (0.30 * b, 0), 4: (0.12 * b, 0.22 * b)}
    for i, (cx, cy) in enumerate(order):
        dx, dy = slip.get(i, (0, 0))
        cell(cv, x + cx * step + dx, y + cy * step + dy, s, i in HEALTH_LIT,
             glitch=(0.14 * b if i in slip else 0))
    core(cv, x, y, 0.24 * b)
    cv.box(x - s, y - s, 2 * s, 2 * s, 1.0, 0.45, glow=False)
    # the grid's own frame: crop corners round the 3x3, a broken ring
    F = 1.5 * step
    corners(cv, x - F - 0.1 * b, y - F - 0.1 * b, 2 * F + 0.2 * b, 2 * F + 0.2 * b, 0.5 * b, 2.6, 0.95)
    for k in range(4):
        m = k * TAU / 4 + TAU / 8
        cv.arc(x, y, 2.35 * b, m - 0.45, m + 0.45, 1.6, 0.7)
    shed(cv, x + 1.5 * step + 0.3 * b, y - 0.6 * b, 0.6, -0.35, 4, 0.15 * b)
    pointer(cv, x + 2.3 * b, y + 1.7 * b)


def b_sniper(cv, x, y, b, aim=-0.5):
    # The eight cells slide into two rails of four along the aim: the grid
    # becomes the gun. The core sits at the breech.
    s = 0.30 * b
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    for i in range(8):
        k, side = i // 2, (1 if i % 2 else -1)
        d = 0.9 * b + k * (2 * s + 0.08 * b)
        cx, cy = x + ux * d + nx * side * (s + 0.12 * b), y + uy * d + ny * side * (s + 0.12 * b)
        pts = [(cx + rot((px, py), aim)[0], cy + rot((px, py), aim)[1]) for px, py in ((-s, -s), (s, -s), (s, s), (-s, s))]
        cv.poly(pts, 2.0 if i in HEALTH_LIT else 1.0, 1.0 if i in HEALTH_LIT else 0.25)
    # the bore between the rails
    L = 0.9 * b + 4 * (2 * s + 0.08 * b)
    cv.seg((x + ux * 0.7 * b, y + uy * 0.7 * b), (x + ux * (L + 0.3 * b), y + uy * (L + 0.3 * b)), 1.4, 0.9, col=HOT)
    # breech: core inside a turned square
    sq = [(x + rot(p, aim + TAU / 8)[0], y + rot(p, aim + TAU / 8)[1]) for p in
          [(0.75 * b, 0), (0, 0.75 * b), (-0.75 * b, 0), (0, -0.75 * b)]]
    cv.poly(sq, 2.4, 1.0)
    core(cv, x, y, 0.24 * b, col=HOT)
    corners(cv, x - 0.95 * b, y - 0.95 * b, 1.9 * b, 1.9 * b, 0.3 * b, 2.0, 0.8)


# ── C: engraver (burin) ────────────────────────────────────────
def c_corrupt(cv, x, y, b):
    # A burin's cross-section (a lozenge) set in a crop frame; the carved
    # hatching inside is the corruption it engraves, cut into steps.
    F = 1.55 * b
    corners(cv, x - F, y - F, 2 * F, 2 * F, 0.6 * b, 3.0, 1.0)
    cv.box(x - F, y - F, 2 * F, 2 * F, 1.0, 0.22)
    L = 1.25 * b
    loz = [(x, y - L), (x + L * 0.8, y), (x, y + L), (x - L * 0.8, y)]
    cv.poly(loz, 2.6, 1.0)
    inner = [(x, y - L * 0.72), (x + L * 0.58, y), (x, y + L * 0.72), (x - L * 0.58, y)]
    cv.poly(inner, 1.0, 0.5, glow=False)
    # stepped engraving: hatch lines inside the lower half, each line shifted
    for i in range(6):
        yy = y + 0.12 * b + i * 0.15 * b
        half = (L * 0.58) * (1 - (yy - y) / (L * 0.72))
        sh = (0.08 * b if i % 2 else -0.05 * b)
        if half > 2:
            cv.seg((x - half + sh, yy), (x + half + sh, yy), 1.1, 0.6, glow=False)
    pos = [(-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
    for i, (hx, hy) in enumerate(pos):
        handle(cv, x + hx * F, y + hy * F, 0.13 * b, i in HEALTH_LIT)
    shed(cv, x - F, y + F, -0.5, 0.45, 4, 0.15 * b)
    core(cv, x, y - 0.3 * b, 0.18 * b)
    pointer(cv, x + 2.2 * b, y + 1.6 * b)


def c_sniper(cv, x, y, b, aim=-0.5):
    # The lozenge stretches along the aim into a long blade with a flat tip
    # (a burin, not a spike); the frame folds to two brackets at its back.
    ux, uy = math.cos(aim), math.sin(aim)
    nx, ny = -uy, ux
    W = 0.45 * b
    back, tip = -0.9 * b, 2.9 * b
    pts = [(x + ux * back, y + uy * back), (x + ux * 0.3 * b + nx * W, y + uy * 0.3 * b + ny * W),
           (x + ux * tip + nx * W * 0.35, y + uy * tip + ny * W * 0.35),
           (x + ux * tip - nx * W * 0.35, y + uy * tip - ny * W * 0.35),
           (x + ux * 0.3 * b - nx * W, y + uy * 0.3 * b - ny * W)]
    cv.poly(pts, 2.6, 1.0)
    cv.seg((x + ux * 0.1 * b, y + uy * 0.1 * b), (x + ux * (tip - 0.2 * b), y + uy * (tip - 0.2 * b)), 1.2, 0.9, col=HOT)
    for s in (-1, 1):   # frame folded into brackets behind
        bx, by = x - ux * 1.2 * b + nx * s * 1.0 * b, y - uy * 1.2 * b + ny * s * 1.0 * b
        cv.seg((bx, by), (bx + ux * 0.55 * b, by + uy * 0.55 * b), 2.6, 1.0)
        cv.seg((bx, by), (bx - nx * s * 0.55 * b, by - ny * s * 0.55 * b), 2.6, 1.0)
    for i in range(8):   # health along the blade's two edges
        k, side = i // 2, (1 if i % 2 else -1)
        t = 0.55 + k * 0.5
        hx = x + ux * t * b + nx * side * (W * (1 - (t * b - 0.3 * b) / (tip - 0.3 * b) * 0.65) + 0.18 * b)
        hy = y + uy * t * b + ny * side * (W * (1 - (t * b - 0.3 * b) / (tip - 0.3 * b) * 0.65) + 0.18 * b)
        handle(cv, hx, hy, 0.09 * b, i in HEALTH_LIT)
    core(cv, x, y, 0.22 * b, col=HOT)


DESIGNS = [
    ("A 찢긴 뷰파인더", a_corrupt, a_sniper,
     ["오염: 화면 찢김처럼 틀이 3단으로 어긋나고,", "렌즈 고리도 같이 밀린다. 픽셀이 새어 나온다.",
      "사격: 찢긴 틀이 딱 맞춰지며 초점이 잡힌다.", "렌즈가 닫히고 긴 레일 총열이 나온다."]),
    ("B 픽셀 행렬", b_corrupt, b_sniper,
     ["오염: 3x3 칸 = 경기장 격자의 축소판.", "바깥 8칸 = 체력. 오염 중인 칸은 어긋나 겹친다.",
      "사격: 8칸이 4칸씩 두 줄로 미끄러져", "총열(레일)이 된다. 격자가 총으로 바뀐다."]),
    ("C 조각칼", c_corrupt, c_sniper,
     ["오염: 조각칼 단면(마름모)이 자르기 틀 안에서", "계단처럼 어긋난 새김선을 판다.",
      "사격: 마름모가 조준 방향으로 늘어나", "끝이 평평한 긴 날이 된다(가시 아님)."]),
]


def font(sz, bold=False):
    for name in (["malgunbd.ttf"] if bold else []) + ["malgun.ttf"]:
        try:
            return ImageFont.truetype(name, sz)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    out_dir = sys.argv[1]
    W, P = 1800, 470
    H = 150 + 2 * P + 20 + 130
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 22), "CAELUM 몸체 디자인 A v1 · 오염하는 몸 / 빠르게 쏘는 몸", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "두 형태를 오간다: 오염 형태(커서로 판을 깐다) ↔ 사격 형태(아주 빠른 단발 예측 사격). "
           "사격 형태로 바뀌는 순간이 몸으로 확실히 읽혀야 한다 (사격 예고는 커서뿐).",
           font=font(18), fill=(154, 160, 184))
    d.text((30, 104), "선만 · 채우기/점선/막/방사 가시 없음 · 모서리 날카롭게 · 체력 표시 8개 (여기선 2개 꺼짐) · 호박색 · 커서는 별도 개체(오염 형태 옆에 표시)",
           font=font(15), fill=(144, 151, 174))
    for i, (name, fc, fs, notes) in enumerate(DESIGNS):
        x0 = 30 + i * (P + 120)
        for row, fn, tag in ((0, fc, "오염 형태"), (1, fs, "사격 형태")):
            cv = Canvas(P, P)
            fn(cv, P / 2 - (20 if row else 0), P / 2 + (30 if row else 0), 78)
            y0 = 150 + row * (P + 10)
            board.paste(cv.image(), (x0, y0))
            d.rectangle([x0, y0, x0 + P, y0 + P], outline=(43, 45, 61), width=2)
            d.text((x0 + 12, y0 + 10), f"{name} · {tag}", font=font(20, True), fill=(255, 209, 128))
        for j, s in enumerate(notes):
            d.text((x0 + 4, 150 + 2 * P + 30 + j * 24), s, font=font(16), fill=(170, 176, 196))
    board.save(os.path.join(out_dir, "Caelum_Body_Design_A_v1.png"))


if __name__ == "__main__":
    main()
