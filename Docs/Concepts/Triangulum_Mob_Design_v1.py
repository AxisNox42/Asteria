"""TRIANGULUM mob design v1: a Genesis-magenta core with three Swarms orbiting it.
Compares the current in-game look with two cleaner directions and shows the death
release for the recommended one. Lines only, no fills, no dashes, no thorns.
Usage: python Triangulum_Mob_Design_v1.py <out_dir>"""
import math, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, font
from Mira_Mob_Design_v1 import ring, player

TAU = math.tau
BODY = (189, 26, 133)        # Monster::GENESIS_COLOR (0.74, 0.10, 0.52)
BODY_HOT = (255, 170, 225)
PINK = (255, 87, 199)        # Monster::GENESIS_CONTROLLED_COLOR (1.0, 0.34, 0.78)
RED = (255, 72, 60)          # a free Swarm
TEXT, SUB, EDGE = (241, 242, 248), (170, 176, 196), (43, 45, 61)

# Game numbers (px): TRIANGULUM base, orbit radius, Swarm base.
BASE, ORBIT, SWARM = 17.0, 46.0, 13.0


def swarm(cv, x, y, b, face, col, alpha=1.0, red_core=False):
    """The in-game Swarm: a wire triangle with node rings and a core, one vertex at `face`."""
    r = b * 0.92
    pts = [(x + math.cos(face + k * TAU / 3) * r, y + math.sin(face + k * TAU / 3) * r) for k in range(3)]
    cv.poly(pts, max(1.2, b * 0.12), 0.75 * alpha, col=col)
    for p in pts:
        ring(cv, p[0], p[1], b * 0.15, max(1.0, b * 0.07), 0.9 * alpha, col)
    ring(cv, x, y, b * 0.28, max(1.2, b * 0.10), 0.95 * alpha, RED if red_core else col)
    ring(cv, x, y, b * 0.10, max(1.0, b * 0.07), 0.95 * alpha, BODY_HOT if not red_core else (255, 210, 200))


def bare_tri(cv, x, y, r, face, w, col, alpha=1.0):
    pts = [(x + math.cos(face + k * TAU / 3) * r, y + math.sin(face + k * TAU / 3) * r) for k in range(3)]
    cv.poly(pts, w, alpha, col=col)


def core(cv, x, y, S, ring_w=2.0):
    ring(cv, x, y, BASE * 0.55 * S, ring_w * S ** 0.5, 0.9, BODY)
    ring(cv, x, y, BASE * 0.30 * S * 0.55, 1.6 * S ** 0.5, 1.0, BODY_HOT)


def now(cv, x, y, S, spin):
    """What the game draws today: thin small bare triangles."""
    core(cv, x, y, S)
    for k in range(3):
        a = spin + k * TAU / 3
        bare_tri(cv, x + math.cos(a) * ORBIT * S, y + math.sin(a) * ORBIT * S, BASE * 0.5 * S, a, 1.2 * S ** 0.5, PINK, 0.85)


def option_a(cv, x, y, S, spin, alpha=1.0):
    """Recommended: three small copies of the Swarm it will release, vertex outward."""
    core(cv, x, y, S, 2.8)
    for k in range(3):
        a = spin + k * TAU / 3
        swarm(cv, x + math.cos(a) * ORBIT * S, y + math.sin(a) * ORBIT * S, SWARM * 0.85 * S, a, PINK, alpha)


def option_b(cv, x, y, S, spin):
    """The sketch, scaled up: bare triangles 1.3x bigger and brighter."""
    core(cv, x, y, S, 2.8)
    for k in range(3):
        a = spin + k * TAU / 3
        bare_tri(cv, x + math.cos(a) * ORBIT * S, y + math.sin(a) * ORBIT * S, BASE * 0.65 * S, a, 2.2 * S ** 0.5, PINK, 1.0)


def frame(board, d, img, x0, y0, title, col, notes=()):
    board.paste(img, (x0, y0))
    d.rectangle([x0, y0, x0 + img.width, y0 + img.height], outline=EDGE, width=2)
    d.text((x0 + 12, y0 + 10), title, font=font(19, True), fill=col)
    for j, s in enumerate(notes):
        d.text((x0 + 4, y0 + img.height + 10 + j * 22), s, font=font(15), fill=SUB)


def main():
    out = sys.argv[1]
    W, H = 1760, 1080
    b = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(b)
    d.text((30, 22), "TRIANGULUM 일반 몹 v1 · 자홍 코어 + 공전하는 Swarm 3마리", font=font(36, True), fill=TEXT)
    d.text((30, 74), "요소는 줄이고 남은 것을 또렷하게. 선만 · 점선·가시·판 없음. 꼭짓점은 튀어나갈 방향(바깥). 위 3칸은 게임의 2.4배 축척.",
           font=font(18), fill=(154, 160, 184))

    PW, PH, S, spin = 540, 380, 2.4, -1.5708
    tops = [("지금 (게임 화면)", now, (150, 150, 168),
             ["맨 선 삼각형이 작고 가늘어 코어에 눌린다.", "나오는 Swarm(노드·코어 있음)과 모양이 다르다."]),
            ("A 추천 · 작은 Swarm 그대로", option_a, PINK,
             ["나올 Swarm을 0.85배로 줄인 모양 그대로.", "품은 것 = 나오는 것. 코어 고리만 굵게."]),
            ("B · 스케치 그대로 크게", option_b, PINK,
             ["맨 선 삼각형을 1.3배, 선을 굵고 밝게.", "가장 깔끔하지만 나올 때 모양이 바뀐다."])]
    for i, (title, fn, col, notes) in enumerate(tops):
        cv = Canvas(PW, PH); fn(cv, PW / 2, PH / 2 + 30, S, spin)
        frame(b, d, cv.image(), 30 + i * (PW + 40), 130, title, col, notes)

    sy = 130 + PH + 120
    d.text((30, sy - 42), "A안이 죽을 때 (게임의 1.6배 축척)", font=font(22, True), fill=TEXT)
    SW, SH, S2 = 400, 300, 1.6
    cx, cy, px, py = 200, 180, 350, 70
    steps = [("a 살아 있음: 위협 없음", 0.0), ("b 처치 직후 0.15초: 바깥으로 튐", 0.5), ("c 0.3초 뒤: 분홍 Swarm으로 추격", 1.0)]
    for i, (title, k) in enumerate(steps):
        cv = Canvas(SW, SH)
        if k == 0.0:
            option_a(cv, cx, cy, S2, spin)
        else:
            if k < 1.0:
                ring(cv, cx, cy, BASE * 0.55 * S2 * (1 + 1.6 * k), 2.0, 0.6 * (1 - k), BODY)   # the core breaking up
            for j in range(3):
                a = spin + j * TAU / 3
                dist = (ORBIT + 20 * min(1.0, k)) * S2
                sx, sy2 = cx + math.cos(a) * dist, cy + math.sin(a) * dist
                if k >= 1.0:   # chasing: turned toward the player, full Swarm size
                    face = math.atan2(py - sy2, px - sx)
                    swarm(cv, sx, sy2, SWARM * S2, face, PINK)
                else:
                    swarm(cv, sx, sy2, SWARM * (0.85 + 0.15 * k) * S2, a, PINK)
        player(cv, px, py)
        frame(b, d, cv.image(), 30 + i * (SW + 30), sy, title, PINK)

    # Readability: real-ish scale among free red Swarms.
    CW, x0 = 400, 30 + 3 * (SW + 30)
    cv = Canvas(CW, SH)
    option_a(cv, 260, 180, 1.5, 0.4)
    for (sx, sy2, f) in [(60, 90, 0.3), (110, 240, -0.6), (360, 70, 2.4), (190, 60, 1.2), (50, 170, 0.9), (360, 270, -2.0)]:
        swarm(cv, sx, sy2, SWARM * 1.5, f, RED, red_core=True)
    player(cv, 160, 150)
    frame(b, d, cv.image(), x0, sy, "무리 속 (1.5배)", TEXT)

    ty = sy + SH + 20
    for j, s in enumerate(["코어 고리만 2.0 → 2.8로 굵게. 숨쉬기·겹선·연결선·자체 회전은 넣지 않는다 (움직임은 느린 공전 하나).",
                           "궤도 Swarm은 나올 때 0.85 → 1.0배로 커지며 그대로 Swarm이 된다. 붉은 Swarm과는 분홍 코어로 구분된다.",
                           "열린 점: 제네시스가 조종하는 Swarm과 같은 분홍이라 같은 웨이브에서 헷갈릴 수 있다."]):
        d.text((30, ty + j * 24), s, font=font(15), fill=SUB)
    b.save(os.path.join(out, "Triangulum_Mob_Design_v1.png"))


if __name__ == "__main__":
    main()
