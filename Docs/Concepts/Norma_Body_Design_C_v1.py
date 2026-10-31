"""NORMA body design C v1 (SVG): the tesseract after the playtest.

1 hierarchy (bold outer shell, quiet inner cube), 2 the eight outer stars as
health, 3 a phase 2 whose shell comes apart, 4 the pane-bracket ring only
while the boss handles panes. Drawn the way the mobs are drawn: thin lines
over a faint glow, edges split short of their stars, glow nodes and a core.
Lines only for faces; no dashed lines, plates or thorn spikes.
Usage: python Norma_Body_Design_C_v1.py <out_dir>
"""
import math, sys, os

OUT = sys.argv[1]
ICE = "#b8efff"
WHITE = "#f4fdff"
RED = "#ff6a5c"
BLUE = "#8fa2ff"

STYLE = """
.bg{fill:#080910}
.frame{fill:none;stroke:#23263a;stroke-width:1.2}
.t1{font:700 40px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#f1f2f8}
.t2{font:19px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9aa0b8}
.h{font:700 24px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#eef0fa}
.s{font:15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#9097ae}
.tag{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#34e0f0}
.tagg{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#8a90a8}
.tagv{font:700 15px 'Malgun Gothic','Noto Sans CJK KR',sans-serif;fill:#c3b2ff}
"""


class Svg:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.o = w, h, title, []

    def seg(self, x1, y1, x2, y2, col, a=0.9, w=1.4, glow=True):
        if glow:
            self.o.append(f'<path d="M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}" stroke="{col}" stroke-opacity="{a * 0.14:.3f}" '
                          f'stroke-width="{w + 3.2:.1f}" stroke-linecap="round" fill="none"/>')
        self.o.append(f'<path d="M{x1:.1f} {y1:.1f} {x2:.1f} {y2:.1f}" stroke="{col}" stroke-opacity="{a:.3f}" '
                      f'stroke-width="{w:.1f}" stroke-linecap="round" fill="none"/>')

    def dot(self, x, y, r, col, a):
        self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{col}" fill-opacity="{a:.3f}"/>')

    def ring(self, x, y, r, col, a, w=1.2):
        self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="none" stroke="{col}" '
                      f'stroke-opacity="{a:.3f}" stroke-width="{w:.1f}"/>')

    def node(self, x, y, r, col, a=1.0):
        self.dot(x, y, r * 1.55, col, a * 0.20)
        self.dot(x, y, r * 0.72, col, a * 0.58)
        self.dot(x, y, r * 0.22, WHITE, a * 0.95)

    def dark_node(self, x, y, r, col):
        """A spent star: just its outline and a faint point."""
        self.ring(x, y, r * 0.72, col, 0.45, 1.1)
        self.dot(x, y, r * 0.16, col, 0.30)

    def core(self, x, y, s, col, a=1.0):
        self.dot(x, y, s * 2.35, col, a * 0.12)
        self.dot(x, y, s * 1.38, col, a * 0.34)
        self.dot(x, y, s * 0.72, col, a * 0.82)
        self.dot(x, y, s * 0.28, WHITE, a)

    def field(self, x, y, r, col, a):
        for k, f in enumerate((1.0, 0.8, 0.6, 0.4)):
            self.dot(x, y, r * f, col, a * (0.25 + 0.25 * k))

    def rect(self, x, y, w, h):
        self.o.append(f'<rect class="frame" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')

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


CUBE = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
EDGES = [(0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4), (0, 4), (1, 5), (2, 6), (3, 7)]


def project(x, y, r, yaw, pitch):
    out = []
    for px, py, pz in CUBE:
        c, s = math.cos(yaw), math.sin(yaw)
        x1, z1 = px * c + pz * s, -px * s + pz * c
        c, s = math.cos(pitch), math.sin(pitch)
        y2, z2 = py * c - z1 * s, py * s + z1 * c
        out.append((x + x1 * r, y + y2 * r, z2 / 1.732))
    return out


def edges(S, pts, col, alpha, w, gap=0.16, spread=0.0, cx=0.0, cy=0.0, glow=True):
    for i, j in EDGES:
        a, b = pts[i], pts[j]
        front = 0.5 + 0.25 * (a[2] + b[2])
        al = alpha * (0.45 + 0.55 * front)
        ax, ay = a[0] + (b[0] - a[0]) * gap, a[1] + (b[1] - a[1]) * gap
        bx, by = b[0] + (a[0] - b[0]) * gap, b[1] + (a[1] - b[1]) * gap
        if spread:
            mx, my = (ax + bx) / 2 - cx, (ay + by) / 2 - cy
            ln = max(1.0, math.hypot(mx, my))
            ax += mx / ln * spread; bx += mx / ln * spread
            ay += my / ln * spread; by += my / ln * spread
        S.seg(ax, ay, bx, by, col, al, w, glow)


def stars(S, pts, r, col, lit=8):
    order = sorted(range(8), key=lambda i: -pts[i][2])   # front stars stay lit longest
    keep = set(order[:lit])
    for i in sorted(range(8), key=lambda i: pts[i][2]):
        px, py, pz = pts[i]
        k = 0.6 + 0.4 * (pz + 1) / 2
        if i in keep:
            S.node(px, py, r * (0.85 + 0.25 * k), col, k)
        else:
            S.dark_node(px, py, r, col)


def brackets(S, x, y, r, phase, col, a, arm):
    for k in range(4):
        ang = phase + math.pi / 4 + k * math.pi / 2
        bx, by = x + math.cos(ang) * r, y + math.sin(ang) * r
        for da in (0.75 * math.pi, -0.75 * math.pi):
            S.seg(bx, by, bx + math.cos(ang + da) * arm, by + math.sin(ang + da) * arm, col, a, 1.8)


def norma_now(S, x, y, base):
    """The tesseract as it is in game now."""
    S.field(x, y, base * 3.0, ICE, 0.06)
    out = project(x, y, base * 1.15, 0.6, 0.55)
    inn = project(x, y, base * 1.15 * 0.45, -0.38, 0.33)
    for a, b in zip(out, inn):
        S.seg(a[0], a[1], b[0], b[1], ICE, 0.28, 0.9, False)
    edges(S, inn, ICE, 0.8, 1.0)
    edges(S, out, ICE, 0.95, 1.4)
    stars(S, inn, base * 0.11, ICE)
    stars(S, out, base * 0.17, ICE)
    S.core(x, y, base * 0.34, ICE)
    brackets(S, x, y, base * 2.6, 0.3, ICE, 0.75, base * 0.42)


def norma_new(S, x, y, base, lit=8, phase2=False, ring=False, red=False, yaw=0.6, pitch=0.55):
    """Design C: bold shell, quiet inner cube, health stars."""
    col = RED if red else ICE
    s = 1.12   # a little bigger
    S.field(x, y, base * 3.0 * s, col, 0.07)
    out = project(x, y, base * 1.15 * s, yaw, pitch)
    inn = project(x, y, base * 1.15 * s * (0.62 if phase2 else 0.45), -yaw * 1.3 + 0.4, pitch * 0.6)
    for a, b in zip(out, inn):
        S.seg(a[0], a[1], b[0], b[1], col, 0.16 if not phase2 else 0.10, 0.7, False)
    if phase2:
        # The inner cube takes over: bright and bold; the shell comes apart.
        edges(S, inn, col, 1.0, 2.2)
        edges(S, out, col, 0.55, 1.6, gap=0.30, spread=base * 0.32, cx=x, cy=y)
        stars(S, inn, base * 0.15, col)
        stars(S, out, base * 0.20, col, lit)
    else:
        edges(S, inn, col, 0.45, 0.9, glow=False)
        edges(S, out, col, 1.0, 2.6)
        stars(S, inn, base * 0.08, col)
        stars(S, out, base * 0.22, col, lit)
    S.core(x, y, base * (0.40 if not phase2 else 0.48), col)
    if ring:
        brackets(S, x, y, base * 2.7 * s, 0.3, col, 0.85, base * 0.45)


def board():
    W, H = 1800, 1560
    S = Svg(W, H, "NORMA 몸체 디자인 C v1")
    S.text("t1", 30, 56, "NORMA 몸체 디자인 C v1 · 플레이테스트 뒤 다듬기")
    S.text("t2", 30, 90, "1 위계 · 2 몸에 체력 · 3 2페이즈는 껍데기가 풀린다 · 4 창 괄호는 창을 다룰 때만 · 색 고정(빨강은 조종 때만), 선만")
    base = 46

    # ---- 1 hierarchy
    y = 120
    S.text("h", 30, y + 28, "1. 위계 — 실루엣은 바깥 껍데기가, 안쪽은 조용히")
    for i, (tag, cls, cap, fn) in enumerate([
            ("지금", "tagg", ["바깥·안쪽·연결선·괄호가 비슷한 굵기라", "멀리서 보면 선 뭉치"], norma_now),
            ("개편", "tag", ["바깥 큐브 굵고 밝게(실루엣), 꼭짓점 별 크게", "안쪽·연결선은 가늘고 흐리게 · 크기 +12%"],
             lambda S, x, y, b: norma_new(S, x, y, b))]):
        fx = 30 + i * 450
        S.rect(fx, y + 44, 420, 330)
        S.text(cls, fx + 10, y + 66, tag)
        S.lines("s", fx, y + 400, cap)
        fn(S, fx + 210, y + 44 + 170, base)
    # far-away read: small versions
    fx = 930
    S.rect(fx, y + 44, 840, 330)
    S.text("tag", fx + 10, y + 66, "멀리서 (실제 화면 크기의 절반)")
    S.lines("s", fx, y + 400, ["작게 보면 차이가 커진다: 지금은 흐린 뭉치,", "개편은 큐브 윤곽이 또렷하다"])
    norma_now(S, fx + 230, y + 44 + 170, base * 0.5)
    norma_new(S, fx + 610, y + 44 + 170, base * 0.5)
    S.text("tagg", fx + 200, y + 340, "지금")
    S.text("tag", fx + 580, y + 340, "개편")

    # ---- 2 health stars
    y = 560
    S.text("h", 30, y + 28, "2. 몸에 체력 — 바깥 별 8개 = 체력 8칸 (12.5%마다 하나씩 꺼짐)")
    for i, (lit, label) in enumerate([(8, "100%"), (6, "75%"), (4, "50%"), (3, "40% · 2페이즈 진입"), (1, "12%")]):
        fx = 30 + i * 350
        S.rect(fx, y + 44, 330, 290)
        S.text("tag" if i < 3 else "tagv", fx + 10, y + 66, label)
        norma_new(S, fx + 165, y + 44 + 150, base * 0.82, lit=lit, phase2=lit <= 3)
    S.lines("s", 30, y + 362, ["꺼진 별은 빈 테두리만 남는다(채우기 없음). 앞쪽 별이 가장 오래 남아 실루엣을 지킨다.",
                               "꺼지는 순간 번쩍 + 짧은 멈칫 → 체력바를 안 봐도 '얼마나 깎았는지'가 몸에서 보인다."])

    # ---- 3 phase 2
    y = 980
    S.text("h", 30, y + 28, "3. 2페이즈 — 껍데기가 풀리고 안쪽 큐브가 실루엣을 넘겨받는다")
    for i, (tag, cls, cap, p2) in enumerate([
            ("1페이즈", "tag", ["굵은 바깥 껍데기 + 조용한 안쪽"], False),
            ("2페이즈", "tagv", ["바깥 모서리 12개가 끊긴 채 바깥으로 벌어지고(흐려짐)", "안쪽 큐브가 커지고 굵고 밝아진다"], True)]):
        fx = 30 + i * 450
        S.rect(fx, y + 44, 420, 330)
        S.text(cls, fx + 10, y + 66, tag)
        S.lines("s", fx, y + 400, cap)
        norma_new(S, fx + 210, y + 44 + 170, base, lit=8 if not p2 else 3, phase2=p2)

    # ---- 4 bracket ring
    fx = 930
    S.text("h", fx, y + 28, "4. 창 괄호 고리 — 창을 다룰 때만")
    for i, (tag, cls, cap, ring, red) in enumerate([
            ("평소", "tag", ["괄호 없음 · 화면이 조용하다"], False, False),
            ("창 조종 중", "tag", ["창 생성·숨기·창 돌진 때만 괄호가 펼쳐진다", "창 돌진은 빨강"], True, True)]):
        gx = fx + i * 425
        S.rect(gx, y + 44, 405, 330)
        S.text(cls, gx + 10, y + 66, tag)
        S.lines("s", gx, y + 400, cap)
        norma_new(S, gx + 202, y + 44 + 170, base * 0.9, ring=ring, red=red)

    S.save("Norma_Body_Design_C_v1.svg")


board()
print("ok")
