"""NORMA body design B v1 (SVG), in the regular mobs' own drawing language.

The mobs are drawn as constellations: thin lines over a faint wide glow,
edges split short of each vertex, glowing star nodes on the vertices, a
soft core, and 3D wire bodies that turn (Magnetar's lattice, Quasar's orb).
This board draws NORMA that way, next to Regulus and Antares at game scale.
Lines only for faces (no filled faces); nodes and cores are glows, as on
every mob. No dashed lines, plates, or thorn spikes.
Usage: python Norma_Body_Design_B_v1.py <out_dir>
"""
import math, sys, os

OUT = sys.argv[1]
TAU = math.tau

ICE = "#b8efff"
WHITE = "#f4fdff"
BLUE = "#8fa2ff"
RED = "#ff6a5c"
DIM = "#4d6a78"

STYLE = """
.bg{fill:#080910}
.frame{fill:none;stroke:#23263a;stroke-width:1.2}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:19px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 24px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.s{font:15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9097ae}
.tag{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.tagv{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
.tagg{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8a90a8}
"""


class Svg:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.o = w, h, title, []

    def seg(self, x1, y1, x2, y2, col, a=0.9, w=1.4):
        """A mob line: faint wide glow under a thin bright stroke."""
        self.o.append(f'<path d="M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}" stroke="{col}" stroke-opacity="{a * 0.14:.3f}" '
                      f'stroke-width="{w + 3.2:.1f}" stroke-linecap="round" fill="none"/>')
        self.o.append(f'<path d="M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}" stroke="{col}" stroke-opacity="{a:.3f}" '
                      f'stroke-width="{w:.1f}" stroke-linecap="round" fill="none"/>')

    def dot(self, x, y, r, col, a):
        self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{col}" fill-opacity="{a:.3f}"/>')

    def node(self, x, y, r, col, a=1.0):
        """Star node: halo, body, white point (DrawEnemyNode)."""
        self.dot(x, y, r * 1.55, col, a * 0.20)
        self.dot(x, y, r * 0.72, col, a * 0.58)
        self.dot(x, y, r * 0.22, WHITE, a * 0.95)

    def core(self, x, y, s, col, a=1.0):
        """Soft core (DrawEnemyCore)."""
        self.dot(x, y, s * 2.35, col, a * 0.12)
        self.dot(x, y, s * 1.38, col, a * 0.34)
        self.dot(x, y, s * 0.72, col, a * 0.82)
        self.dot(x, y, s * 0.28, WHITE, a)

    def arc(self, x, y, r, a0, sweep, col, a=0.9, w=1.2, n=24):
        for i in range(n):
            u0, u1 = a0 + sweep * i / n, a0 + sweep * (i + 1) / n
            self.seg(x + math.cos(u0) * r, y + math.sin(u0) * r, x + math.cos(u1) * r, y + math.sin(u1) * r, col, a, w)

    def rect(self, x, y, w, h):
        self.o.append(f'<rect class="frame" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

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


# ---------- 3D wire helpers ----------

CUBE = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
EDGES = [(0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4), (0, 4), (1, 5), (2, 6), (3, 7)]


def turn(p, yaw, pitch):
    x, y, z = p
    c, s = math.cos(yaw), math.sin(yaw)
    x, z = x * c + z * s, -x * s + z * c
    c, s = math.cos(pitch), math.sin(pitch)
    y, z = y * c - z * s, y * s + z * c
    return x, y, z


def split_edge(S, a, b, col, alpha, w=1.3, gap=0.16):
    """Edge split short of both vertices (Antares' frame)."""
    ax, ay = a[0] + (b[0] - a[0]) * gap, a[1] + (b[1] - a[1]) * gap
    bx, by = b[0] + (a[0] - b[0]) * gap, b[1] + (a[1] - b[1]) * gap
    S.seg(ax, ay, bx, by, col, alpha, w)


def wire_cube(S, x, y, r, yaw, pitch, col, node_r, alpha=1.0, nodes=True, w=1.3):
    pts = [turn(p, yaw, pitch) for p in CUBE]
    scr = [(x + p[0] * r, y + p[1] * r, p[2]) for p in pts]
    for i, j in EDGES:
        depth = (scr[i][2] + scr[j][2]) * 0.5   # -1 far .. +1 near (z toward viewer)
        a = alpha * (0.45 + 0.45 * (depth + 1.732) / 3.464)
        split_edge(S, scr[i], scr[j], col, a, w)
    if nodes:
        for px, py, pz in sorted(scr, key=lambda q: q[2]):
            k = 0.55 + 0.45 * (pz + 1.732) / 3.464
            S.node(px, py, node_r * (0.8 + 0.3 * k), col, alpha * k)
    return scr


def tesseract(S, x, y, r, yaw, pitch, col, node_r, inner=0.5, alpha=1.0):
    outer = wire_cube(S, x, y, r, yaw, pitch, col, node_r, alpha)
    inn = wire_cube(S, x, y, r * inner, -yaw * 1.3 + 0.4, pitch * 0.6, col, node_r * 0.7, alpha * 0.85)
    for (ox, oy, _), (ix, iy, _) in zip(outer, inn):
        S.seg(ox, oy, ix, iy, col, alpha * 0.30, 0.9)
    return outer, inn


def crest(S, x, y, base, phase, col, alpha=0.92):
    """Regulus' shield crest: four kite plates with cut tips round a square heart."""
    tip = base * 1.15
    for k in range(4):
        a = phase + math.pi / 2 * k
        ps = [(x + math.cos(a) * tip * 0.30, y + math.sin(a) * tip * 0.30),
              (x + math.cos(a - 0.42) * tip * 0.56, y + math.sin(a - 0.42) * tip * 0.56),
              (x + math.cos(a - 0.13) * tip * 0.95, y + math.sin(a - 0.13) * tip * 0.95),
              (x + math.cos(a + 0.13) * tip * 0.95, y + math.sin(a + 0.13) * tip * 0.95),
              (x + math.cos(a + 0.42) * tip * 0.56, y + math.sin(a + 0.42) * tip * 0.56)]
        for i in range(5):
            S.seg(*ps[i], *ps[(i + 1) % 5], col, alpha, 1.5)
        S.seg(*ps[0], x + math.cos(a) * tip * 0.8, y + math.sin(a) * tip * 0.8, col, alpha * 0.45, 0.9)
    sq = [(x + math.cos(phase + math.pi / 4 + k * math.pi / 2) * base * 0.3,
           y + math.sin(phase + math.pi / 4 + k * math.pi / 2) * base * 0.3) for k in range(4)]
    for k in range(4):
        S.seg(*sq[k], *sq[(k + 1) % 4], WHITE, 0.9, 1.2)


def plates(S, x, y, base, phase, col):
    for k in range(4):
        a0 = phase + math.pi / 2 * k + 0.22
        S.arc(x, y, base * 1.75, a0, math.pi / 2 - 0.44, col, 0.85, 2.6, 10)


def regulus(S, x, y, base=18.0):
    plates(S, x, y, base, 0.3, ICE)
    crest(S, x, y, base, 0.0, ICE)
    S.core(x, y, base * 0.30, ICE)


def antares(S, x, y, base=18.0, ang=-1.2):
    vs = [(x + math.cos(ang + k * TAU / 5) * base * 1.4, y + math.sin(ang + k * TAU / 5) * base * 1.4) for k in range(5)]
    for k in range(5):
        split_edge(S, vs[k], vs[(k + 1) % 5], BLUE, 0.85, 1.3, 0.14)
    for v in vs:
        S.node(*v, base * 0.16, BLUE, 0.9)
    S.core(x, y, base * 0.22, BLUE)


def square_ring(S, x, y, r, phase, col, alpha, node_r):
    """A flat square frame, split at the corners, nodes on the corners."""
    vs = [(x + math.cos(phase + math.pi / 4 + k * math.pi / 2) * r,
           y + math.sin(phase + math.pi / 4 + k * math.pi / 2) * r) for k in range(4)]
    for k in range(4):
        split_edge(S, vs[k], vs[(k + 1) % 4], col, alpha, 1.3, 0.12)
    for v in vs:
        S.node(*v, node_r, col, alpha)


def bracket_ring(S, x, y, r, phase, col, alpha, arm):
    """Four corner brackets (the panes' own mark) orbiting as a ring."""
    for k in range(4):
        a = phase + math.pi / 4 + k * math.pi / 2
        cx, cy = x + math.cos(a) * r, y + math.sin(a) * r
        for da in (0.75 * math.pi, -0.75 * math.pi):
            S.seg(cx, cy, cx + math.cos(a + da) * arm, cy + math.sin(a + da) * arm, col, alpha, 1.6)


# ---------- NORMA variants (base = body unit, like the mobs) ----------

def norma_b1(S, x, y, base, yaw=0.6, pitch=0.5, col=ICE, ring=0.3, phase2=False, red=None):
    """B1 tesseract: a turning wire cube with a cube inside it; Regulus'
    square heart and core at the centre; the panes' brackets orbit."""
    c = red or col
    if phase2:
        tesseract(S, x, y, base * 1.25, yaw, pitch, c, base * 0.17, inner=0.78)
    else:
        tesseract(S, x, y, base * 1.25, yaw, pitch, c, base * 0.17, inner=0.45)
    bracket_ring(S, x, y, base * 2.7, ring, c, 0.75, base * 0.42)
    S.core(x, y, base * 0.34, c)


def norma_b2(S, x, y, base, col=ICE):
    """B2 crowned Regulus: the crest doubled (eight plates in two tiers) in a
    split square frame with star corners."""
    crest(S, x, y, base * 1.6, math.pi / 4, col, 0.55)
    crest(S, x, y, base * 1.15, 0.0, col)
    square_ring(S, x, y, base * 2.35, 0.0, col, 0.9, base * 0.2)
    plates(S, x, y, base * 1.55, 0.3, col)
    S.core(x, y, base * 0.34, col)


def norma_b3(S, x, y, base, col=ICE):
    """B3 one cube, flat square rings: a single wire cube inside two
    counter-turning split square frames."""
    wire_cube(S, x, y, base * 1.1, 0.75, 0.55, col, base * 0.16)
    square_ring(S, x, y, base * 2.2, 0.2, col, 0.75, base * 0.14)
    square_ring(S, x, y, base * 2.5, -0.35, col, 0.45, base * 0.10)
    S.core(x, y, base * 0.30, col)


def player(S, x, y, ang=-90):
    a = math.radians(ang)
    ps = [(x + r * math.cos(a + math.radians(da)), y + r * math.sin(a + math.radians(da)))
          for da, r in ((0, 13), (140, 10), (180, 4), (220, 10))]
    for i in range(4):
        S.seg(*ps[i], *ps[(i + 1) % 4], "#34e0f0", 0.95, 1.8)


def shot(S, x, y, ang, s=6):
    u = (math.cos(ang), math.sin(ang)); v = (-u[1], u[0])
    ps = [(x + (px * u[0] + py * v[0]) * s, y + (px * u[1] + py * v[1]) * s)
          for px, py in ((-.5, -.5), (.5, -.5), (.5, .5), (-.5, .5))]
    for i in range(4):
        S.seg(*ps[i], *ps[(i + 1) % 4], ICE, 0.9, 1.3)


def board():
    W, H = 1800, 1290
    S = Svg(W, H, "NORMA 몸체 디자인 B v1")
    S.text("t1", 30, 56, "NORMA 몸체 디자인 B v1 · 잡몹과 같은 그림 언어로")
    S.text("t2", 30, 90, "별 노드(꼭짓점의 빛나는 점) + 꼭짓점 앞에서 끊긴 모서리 + 은은한 코어 + 도는 3D 와이어 · Magnetar·Quasar처럼 입체로 돈다")

    # row 1: references and variants at game scale
    S.text("h", 30, 150, "1. 실루엣 후보 · 잡몹과 같은 배율(레굴루스·안타레스 옆)")
    fy, fh = 168, 400
    cells = [(300, "참고: 레굴루스 · 안타레스", "tagg"), (455, "B1 테서랙트 (추천)", "tag"),
             (455, "B2 왕관 레굴루스", "tagv"), (455, "B3 큐브와 사각 고리", "tagv")]
    x = 30
    caps = [["잡몹 기준 크기 · 같은 선 굵기,", "같은 별 노드, 같은 코어"],
            ["도는 와이어 큐브 안에 작은 큐브(4차원 정육면체)", "꼭짓점마다 별 노드 · 창 괄호 4개가 궤도를 돈다",
             "\"프로그램된 우주\"의 관리자다운 기하학 · 사각 계승"],
            ["레굴루스 문장을 두 겹(판 8장)으로 키우고", "꼭짓점이 별인 사각 틀로 감쌈 · 혈통이 가장 분명"],
            ["큐브 하나 + 서로 반대로 도는 사각 고리 2개", "가장 단순 · 보스 무게감은 B1보다 약함"]]
    for i, (w, tag, cls) in enumerate(cells):
        S.rect(x, fy, w, fh)
        S.text(cls, x + 10, fy + 22, tag)
        S.lines("s", x, fy + fh + 24, caps[i])
        cx, cy = x + w / 2, fy + fh / 2 + 10
        if i == 0:
            regulus(S, cx - 60, cy, 18)
            antares(S, cx + 60, cy, 18)
        elif i == 1:
            norma_b1(S, cx, cy, 52)
        elif i == 2:
            norma_b2(S, cx, cy, 52)
        else:
            norma_b3(S, cx, cy, 52)
        x += w + 20

    # row 2: B1 at work
    y2 = fy + fh + 130
    S.text("h", 30, y2 - 18, "2. B1이 패턴마다 하는 일")
    sw, sh = 330, 320
    gap = (W - 60 - 5 * sw) / 4
    st = lambda i: 30 + i * (sw + gap)
    base = 34

    fx = st(0); S.rect(fx, y2, sw, sh); S.text("tag", fx + 10, y2 + 22, "평상시")
    S.lines("s", fx, y2 + sh + 24, ["큐브가 천천히 돌고 안쪽 큐브는 반대로", "창 괄호 고리가 둘레를 돈다"])
    norma_b1(S, fx + sw / 2, y2 + sh / 2 + 10, base)

    fx = st(1); S.rect(fx, y2, sw, sh); S.text("tag", fx + 10, y2 + 22, "7 조준 연사")
    S.lines("s", fx, y2 + sh + 24, ["꼭짓점 하나가 플레이어를 향해 멈추고", "그 별 노드가 밝아지며 그곳에서 쏜다"])
    cx, cy = fx + sw / 2 - 50, y2 + sh / 2 + 30
    norma_b1(S, cx, cy, base, yaw=0.785, pitch=0.615)
    a = -0.6
    tipx, tipy = cx + math.cos(a) * base * 2.1, cy + math.sin(a) * base * 2.1
    S.node(tipx, tipy, base * 0.32, WHITE, 1.0)
    for j in range(3):
        shot(S, tipx + math.cos(a) * (30 + j * 24), tipy + math.sin(a) * (30 + j * 24), a)
    player(S, cx + math.cos(a) * 250, cy + math.sin(a) * 250, 150)

    fx = st(2); S.rect(fx, y2, sw, sh); S.text("tag", fx + 10, y2 + 22, "11 본체 돌진")
    S.lines("s", fx, y2 + sh + 24, ["큐브가 빠르게 굴러가듯 돌며 돌진", "지나간 자리에 별 노드 잔상"])
    cx, cy = fx + sw / 2 + 60, y2 + sh / 2 + 10
    for j in (3, 2, 1):
        S.node(cx - j * 46, cy + j * 8, base * 0.4, ICE, 0.5 - j * 0.1)
    norma_b1(S, cx, cy, base * 0.9, yaw=1.3, pitch=0.9)

    fx = st(3); S.rect(fx, y2, sw, sh); S.text("tag", fx + 10, y2 + 22, "창 조종")
    S.lines("s", fx, y2 + sh + 24, ["궤도의 창 괄호 하나가 날아가 창에 붙고", "조종하는 동안 몸 전체가 빨강"])
    cx, cy = fx + 95, y2 + sh / 2 + 50
    norma_b1(S, cx, cy, base * 0.8, red=RED)
    px, py, pw, ph = fx + 175, y2 + 55, 135, 105
    for (ax, ay, bx, by) in ((px, py, px + pw, py), (px + pw, py, px + pw, py + ph),
                             (px + pw, py + ph, px, py + ph), (px, py + ph, px, py), (px, py + 16, px + pw, py + 16)):
        S.seg(ax, ay, bx, by, BLUE, 0.8, 1.6)
    for (bx, by, sx, sy) in ((px, py, 1, 1), (px + pw, py, -1, 1), (px, py + ph, 1, -1), (px + pw, py + ph, -1, -1)):
        S.seg(bx, by, bx + sx * 24, by, RED, 0.95, 2.0)
        S.seg(bx, by, bx, by + sy * 24, RED, 0.95, 2.0)

    fx = st(4); S.rect(fx, y2, sw, sh); S.text("tagv", fx + 10, y2 + 22, "2페이즈")
    S.lines("s", fx, y2 + sh + 24, ["안쪽 큐브가 바깥 큐브만큼 부풀며", "4차원으로 뒤집히듯 돈다 · 고리가 빨라짐"])
    norma_b1(S, fx + sw / 2, y2 + sh / 2 + 10, base, yaw=1.1, pitch=0.3, ring=0.9, phase2=True)

    S.save("Norma_Body_Design_B_v1.svg")


board()
print("ok")
