"""NORMA body design F v1: a boss that reads stronger than the elites.

Problem with the shipped body (four small set squares round a square heart):
it is mostly empty space, its silhouette is no bigger than the Quasar's and
it shares nothing with the elites' language (rings, arcs, nested shells).
F keeps NORMA's own motif (the window: squares, frame corners) but builds it
into a large, dense, ring-bound seal. Lines only, sharp corners, no dashes,
no fills, no radial thorns. 8 health nodes on the silhouette as before.
Usage: python Norma_Body_Design_F_v1.py <out_dir> [elite_strip.png]
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

TAU = math.tau
K = 0.85                     # design radii below x K = in-game radii in base units
SS = 3                       # supersampling
COL = (184, 240, 255)        # NORMA colour (0.72, 0.94, 1.0)
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

    def poly(self, pts, w, alpha=1.0, gap=0.07, glow=True):
        """Closed outline whose edges stop short of the vertices (elite language)."""
        n = len(pts)
        for i in range(n):
            a, b = pts[i], pts[(i + 1) % n]
            ax, ay = a[0] + (b[0] - a[0]) * gap, a[1] + (b[1] - a[1]) * gap
            bx, by = b[0] + (a[0] - b[0]) * gap, b[1] + (a[1] - b[1]) * gap
            self.seg((ax, ay), (bx, by), w, alpha, glow)

    def arc(self, cx, cy, r, a0, a1, w, alpha=1.0, glow=True, steps=None):
        steps = steps or max(6, int(abs(a1 - a0) * r / 6))
        pts = [(cx + math.cos(a0 + (a1 - a0) * i / steps) * r,
                cy + math.sin(a0 + (a1 - a0) * i / steps) * r) for i in range(steps + 1)]
        for i in range(steps):
            self.seg(pts[i], pts[i + 1], w, alpha, glow)

    def node(self, x, y, r, alpha=1.0, lit=True):
        c = tuple(int(v * alpha) for v in COL)
        if lit:
            self.gd.ellipse([(x - r * 2.2) * SS, (y - r * 2.2) * SS, (x + r * 2.2) * SS, (y + r * 2.2) * SS],
                            fill=tuple(int(v * 0.35) for v in c))
            self.ld.ellipse([(x - r * .55) * SS, (y - r * .55) * SS, (x + r * .55) * SS, (y + r * .55) * SS],
                            fill=(235, 252, 255))
        self.ld.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS],
                        outline=c, width=max(1, int(1.2 * SS)))

    def core(self, x, y, r):
        for k in range(6, 0, -1):
            rr = r * (1 + k * 0.55)
            a = 0.10 * (7 - k) / 6
            self.gd.ellipse([(x - rr) * SS, (y - rr) * SS, (x + rr) * SS, (y + rr) * SS],
                            fill=tuple(int(v * a * 2.2) for v in COL))
        self.ld.ellipse([(x - r * .55) * SS, (y - r * .55) * SS, (x + r * .55) * SS, (y + r * .55) * SS],
                        fill=(245, 254, 255))
        self.ld.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS],
                        outline=COL, width=int(1.4 * SS))

    def image(self):
        g = self.glow.filter(ImageFilter.GaussianBlur(2.5 * SS)).point(lambda v: int(v * 0.55))
        out = Image.new("RGB", self.glow.size, BG)
        out = _screen(_screen(out, g), self.line)
        return out.resize((self.w, self.h), Image.LANCZOS)


def _screen(a, b):
    from PIL import ImageChops
    return ImageChops.screen(a, b)


def rot(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c)


def at(cx, cy, pts, a=0.0, k=1.0):
    return [(cx + rot(p, a)[0] * k, cy + rot(p, a)[1] * k) for p in pts]


def square(r, a=0.0):
    """Square with circumradius r, a corner at angle a."""
    return [(math.cos(a + i * TAU / 4) * r, math.sin(a + i * TAU / 4) * r) for i in range(4)]


def inset_poly(pts, d):
    """Same polygon pulled toward its centroid by d px (an inset contour)."""
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    out = []
    for x, y in pts:
        L = math.hypot(x - cx, y - cy)
        out.append((cx + (x - cx) * (L - d) / L, cy + (y - cy) * (L - d) / L))
    return out


def bracket(cv, x, y, a, arm, w, alpha=1.0):
    """A window's corner bracket: two arms from (x, y) at a +/- 45deg."""
    for s in (-1, 1):
        d = a + s * TAU / 8
        cv.seg((x, y), (x + math.cos(d) * arm, y + math.sin(d) * arm), w, alpha)


# ---------------------------------------------------------------- designs
def f1_seal(cv, x, y, b, t=0.0):
    """F1 octagram seal: two big squares crossed into an 8-point star inside a
    broken ring; window brackets in the ring's gaps; a counter-turning heart."""
    spin = t
    R = 2.55 * b
    # Ring: four arcs, gaps on the diagonals holding a frame corner each.
    for k in range(4):
        mid = spin * 0.4 + k * TAU / 4
        cv.arc(x, y, R, mid - 0.62, mid + 0.62, 2.0, 0.85)
        cv.arc(x, y, R - 0.16 * b, mid - 0.50, mid + 0.50, 0.9, 0.45, glow=False)
        g = mid + TAU / 8
        bx, by = x + math.cos(g) * (R + 0.05 * b), y + math.sin(g) * (R + 0.05 * b)
        bracket(cv, bx, by, g + math.pi, 0.42 * b, 2.0, 0.95)
    # Octagram: two squares.
    S = 2.2 * b
    for j, a in enumerate((spin, spin + TAU / 8)):
        sq = at(x, y, square(S, a))
        cv.poly(sq, 2.6, 1.0, gap=0.05)
        cv.poly(inset_poly(sq, 0.22 * b), 1.0, 0.5, gap=0.09, glow=False)
    # Health nodes on the 8 star points.
    for i in range(8):
        a = spin + i * TAU / 8
        cv.node(x + math.cos(a) * S, y + math.sin(a) * S, 0.15 * b, lit=i not in (3, 6))
    # Heart: a turned square + struts to the star's inner corners.
    H = 0.95 * b
    heart = at(x, y, square(H, -spin * 1.3 + TAU / 8))
    cv.poly(heart, 1.8, 0.9, gap=0.08)
    cv.poly(inset_poly(heart, 0.18 * b), 0.9, 0.45, glow=False)
    inner = S * math.cos(TAU / 8) / math.cos(TAU / 16) * math.cos(TAU / 16)
    for i in range(8):
        a = spin + TAU / 16 + i * TAU / 8
        ri = S * math.cos(TAU / 8) / math.cos(TAU / 16)
        cv.seg((x + math.cos(a) * H * 0.75, y + math.sin(a) * H * 0.75),
               (x + math.cos(a) * ri * 0.92, y + math.sin(a) * ri * 0.92), 0.8, 0.30, glow=False)
    cv.core(x, y, 0.38 * b)


def f2_vortex(cv, x, y, b, t=0.0):
    """F2 twisted frames: nested window frames, each turned a little more and a
    little smaller, so the body reads as a square vortex/drill; ring outside."""
    spin = t
    R = 2.7 * b
    for k in range(4):
        mid = -spin * 0.5 + k * TAU / 4
        cv.arc(x, y, R, mid - 0.55, mid + 0.55, 1.9, 0.8)
        cv.arc(x, y, R + 0.14 * b, mid - 0.30, mid + 0.30, 0.9, 0.4, glow=False)
        bx, by = x + math.cos(mid) * (R - 0.02 * b), y + math.sin(mid) * (R - 0.02 * b)
        bracket(cv, bx, by, mid + math.pi, 0.36 * b, 1.6, 0.8)
    layers = 5
    S = 2.35 * b
    for i in range(layers):
        a = spin + i * 0.21
        r = S * (0.80 ** i)
        sq = at(x, y, square(r, a))
        w = 2.8 - 0.42 * i
        cv.poly(sq, w, 1.0 - 0.12 * i, gap=0.06 + 0.012 * i, glow=i < 3)
        if i < 2:
            cv.poly(inset_poly(sq, 0.18 * b), 0.9, 0.45, gap=0.1, glow=False)
        if i < 2:   # 8 health nodes on the two outer frames' corners
            for c, (px, py) in enumerate(sq):
                cv.node(px, py, 0.14 * b if i == 0 else 0.12 * b, lit=(i * 4 + c) not in (1, 6))
    cv.core(x, y, 0.36 * b)


def f3_halo(cv, x, y, b, t=0.0):
    """F3 frame halo: a ring band of eight arc plates (the Genesis' shells),
    pierced at four points by a big diamond window frame; a second, square
    frame and a counter-turning heart inside."""
    spin = t
    R1, R2 = 2.05 * b, 2.45 * b
    for k in range(8):
        a0 = spin * 0.6 + k * TAU / 8 + 0.07
        a1 = a0 + TAU / 8 - 0.14
        pts = [(x + math.cos(a0 + (a1 - a0) * i / 10) * R2, y + math.sin(a0 + (a1 - a0) * i / 10) * R2)
               for i in range(11)]
        pts += [(x + math.cos(a1 - (a1 - a0) * i / 10) * R1, y + math.sin(a1 - (a1 - a0) * i / 10) * R1)
                for i in range(11)]
        # outline (arcs + ends)
        for i in range(10):
            cv.seg(pts[i], pts[i + 1], 2.2, 0.95)
            cv.seg(pts[11 + i], pts[12 + i], 1.6, 0.8)
        cv.seg(pts[10], pts[11], 2.0, 0.9)
        cv.seg(pts[21], pts[0], 2.0, 0.9)
        cv.arc(x, y, (R1 + R2) / 2, a0 + 0.08, a1 - 0.08, 0.9, 0.4, glow=False)
        # health node at each plate's leading edge
        am = a0 - 0.07
        cv.node(x + math.cos(am) * (R1 + R2) / 2, y + math.sin(am) * (R1 + R2) / 2, 0.13 * b,
                lit=k not in (2, 5))
    # The big diamond frame: corners poke through the band (sharp, forward).
    D = 2.85 * b
    dia = at(x, y, square(D, -spin * 0.35))
    cv.poly(dia, 2.6, 1.0, gap=0.05)
    cv.poly(inset_poly(dia, 0.24 * b), 1.0, 0.5, gap=0.08, glow=False)
    # Square frame inside, turned 45deg to the diamond.
    sq = at(x, y, square(1.45 * b, -spin * 0.35 + TAU / 8))
    cv.poly(sq, 2.0, 0.9, gap=0.07)
    cv.poly(inset_poly(sq, 0.18 * b), 0.9, 0.45, glow=False)
    heart = at(x, y, square(0.75 * b, spin * 1.2))
    cv.poly(heart, 1.4, 0.8, gap=0.1)
    cv.core(x, y, 0.36 * b)


DESIGNS = [
    ("F1 팔각 인장", f1_seal,
     ["창 두 장이 엇갈려 8각 별이 된 몸.", "별 끝 8개 = 체력 8칸. 링 틈마다 창 괄호.",
      "안쪽 심장은 반대로 돈다."]),
    ("F2 창 소용돌이", f2_vortex,
     ["창틀 5겹이 조금씩 비틀려 겹친 드릴.", "겹마다 다른 속도로 돌아 소용돌이가 산다.",
      "바깥 두 겹 꼭짓점 8개 = 체력."]),
    ("F3 창틀 후광", f3_halo,
     ["제네시스식 호 판 8장의 띠를", "큰 마름모 창틀이 꿰뚫는다(꼭짓점이 띠 밖으로).",
      "판 머리 8개 = 체력."]),
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
    strip = sys.argv[2] if len(sys.argv) > 2 else None
    W, PANEL = 1800, 560
    H = 140 + PANEL + 120 + 300
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 24), "NORMA 몸체 디자인 F v1 · 엘리트보다 세 보이는 보스", font=font(38, True), fill=(241, 242, 248))
    d.text((30, 78), "지금 몸: 빈 공간이 대부분, 실루엣이 퀘이사와 비슷한 크기, 엘리트의 링·호·겹 문법과 따로 논다.  "
           "F: 창(네모·괄호) 모티프는 지키고, 크고 촘촘한 링 인장으로.", font=font(19), fill=(154, 160, 184))
    d.text((30, 106), "선만 · 채우기/점선/막/방사 가시 없음 · 모서리 날카롭게 · 체력 노드 8개 유지 · 실루엣 반경 약 2.2~2.4 base, 약 120px (지금 약 1.1 base, 퀘이사 링 약 75px)",
           font=font(15), fill=(144, 151, 174))
    for i, (name, fn, notes) in enumerate(DESIGNS):
        cv = Canvas(PANEL, PANEL)
        fn(cv, PANEL / 2, PANEL / 2, 88 * K, t=0.18)
        x0 = 30 + i * (PANEL + 25)
        board.paste(cv.image(), (x0, 140))
        d.rectangle([x0, 140, x0 + PANEL, 140 + PANEL], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, 150), name, font=font(22, True), fill=(52, 224, 240))
        for j, s in enumerate(notes):
            d.text((x0 + 4, 140 + PANEL + 10 + j * 24), s, font=font(17), fill=(170, 176, 196))
    # Game scale: the elites as captured, then each candidate at base = 54.
    y0 = 140 + PANEL + 110
    d.text((30, y0 - 18), "실제 게임 크기 비교 (1600x900 캡처와 같은 축척, base 54px)", font=font(22, True), fill=(238, 240, 250))
    x = 30
    C = 208                      # comparison tile; a tile shows a 285px game crop
    if strip and os.path.exists(strip):
        s = Image.open(strip).convert("RGB")
        s = s.resize((int(s.width * C / s.height), C))
        board.paste(s, (x, y0 + 20))
        for j, n in enumerate(["QUASAR", "GENESIS", "ANTARES", "REGULUS"]):
            d.text((x + j * s.width // 4 + 8, y0 + 24), n, font=font(14, True), fill=(195, 178, 255))
        x += s.width + 20
    cur = Image.open(os.path.join(out_dir, "_cur.png")).convert("RGB") if os.path.exists(
        os.path.join(out_dir, "_cur.png")) else None
    if cur:
        board.paste(cur.resize((C, C)), (x, y0 + 20))
        d.text((x + 8, y0 + 24), "지금 NORMA", font=font(14, True), fill=(255, 106, 92))
        x += C + 8
    for name, fn, _ in DESIGNS:
        cv = Canvas(C, C)
        fn(cv, C / 2, C / 2, 54 * K * C / 285, t=0.18)
        board.paste(cv.image(), (x, y0 + 20))
        d.text((x + 8, y0 + 24), name.split()[0], font=font(14, True), fill=(52, 224, 240))
        x += C + 8
    board.save(os.path.join(out_dir, "Norma_Body_Design_F_v1.png"))


if __name__ == "__main__":
    main()
