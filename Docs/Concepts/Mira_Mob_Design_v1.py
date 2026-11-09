"""MIRA mob design v1: a regular mob that sheds a small, ERIDANUS-style corruption trail.

Real star: Mira (omicron Ceti), a pulsing red giant that sheds material and drags a
bow shock and a ~13 light-year tail behind it as it moves; its companion Mira B
(a white dwarf) draws some of that material into a disc. MIRA the mob does the
same in the small: it swells, strokes a straight line across the field and
leaves a trail of small corruption puddles that creep one step outward.

Rules kept: lines only (the corruption film is the sanctioned exception),
no dashes, no thorn spikes, no text on the mob, polygon corners stay sharp.
Usage: python Mira_Mob_Design_v1.py <out_dir>
"""
import math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageChops

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, TAU, font

BODY = (255, 150, 96)        # red-giant orange for the body
BODY_HOT = (255, 226, 190)
FILM = (255, 209, 128)       # ERIDANUS amber: what it sheds is the boss's corruption
PLAYER = (214, 232, 255)


def ring(cv, x, y, r, w, alpha=1.0, col=BODY):
    cv.arc(x, y, r, 0, TAU, w, alpha, col=col)


def wave(cv, x, y, ang, r0, length, amp, phase, w, alpha, col=BODY, n=26):
    """A flowing line from behind the body; amplitude grows toward its free end."""
    ca, sa = math.cos(ang), math.sin(ang)
    pts = []
    for i in range(n + 1):
        t = i / n
        d = r0 + length * t
        off = math.sin(phase + t * 5.2) * amp * t
        pts.append((x + ca * d - sa * off, y + sa * d + ca * off))
    for i in range(n):
        t = i / n
        cv.seg(pts[i], pts[i + 1], max(1.2, w * (1 - 0.6 * t)), alpha * (1 - t) ** 1.2, col=col)


def mira(cv, x, y, R, swell, heading, tail, shock, b_ang=2.4):
    """swell 0..1 pulse (0 = contracted, 1 = fully swollen); heading in radians;
    tail 0..1 length of the stripped tail; shock 0..1 brightness of the bow arcs."""
    s = 0.82 + 0.46 * swell
    hot = BODY_HOT if swell > 0.8 else BODY
    ring(cv, x, y, R * 0.26 * (1.1 - 0.15 * swell), 3.0, col=BODY_HOT)      # core
    ring(cv, x, y, R * 0.55 * s, 2.4, 0.95, hot)                            # photosphere
    ring(cv, x, y, R * 0.86 * s, 1.8, 0.65 + 0.3 * swell)                   # outer envelope
    ring(cv, x, y, R * 1.16 * s, 1.2, 0.18 + 0.55 * swell)                  # swollen shell
    # bow shock: three nested arcs ahead, brighter when about to run
    for k, rr in enumerate((1.7, 2.15, 2.6)):
        a = shock * (0.9 - 0.2 * k)
        if a > 0.02:
            cv.arc(x, y, R * rr, heading - 0.95, heading + 0.95, 2.0 - 0.3 * k, a, col=BODY_HOT if k == 0 else BODY)
    # stripped tail: flowing lines behind
    if tail > 0.02:
        back = heading + math.pi
        for k, (da, amp, ph) in enumerate(((0.0, 14, 0.0), (0.32, 11, 1.7), (-0.32, 11, 3.1), (0.62, 8, 4.4), (-0.62, 8, 5.5))):
            wave(cv, x, y, back + da * 0.45, R * 0.9, R * 6.2 * tail * (1 - 0.12 * abs(k - 0)), amp * R / 60, ph, 2.0 - 0.2 * (k > 2),
                 0.8 - 0.1 * k)
    # Mira B: a small diamond circling at the edge, with a short accretion line to the body
    bx, by = x + math.cos(b_ang) * R * 1.45, y + math.sin(b_ang) * R * 1.45
    d = R * 0.12
    cv.poly([(bx, by - d), (bx + d, by), (bx, by + d), (bx - d, by)], 1.8, col=BODY_HOT)
    cv.arc(x, y, R * 1.45, b_ang - 0.55, b_ang - 0.08, 1.2, 0.5)
    ca, sa = math.cos(b_ang), math.sin(b_ang)
    cv.seg((bx - ca * d * 1.3, by - sa * d * 1.3), (x + ca * R * 1.05 * s, y + sa * R * 1.05 * s), 1.1, 0.35)


def player(cv, x, y, r=9):
    ring(cv, x, y, r, 2.0, 1.0, PLAYER)
    ring(cv, x, y, r * 0.35, 1.5, 1.0, PLAYER)


def field(blobs, w, h, step=2):
    """Metaball field of puddles at panel size; returns (mask, edge) as float arrays."""
    ys, xs = np.mgrid[0:h:step, 0:w:step]
    f = np.zeros(xs.shape, np.float32)
    for bx, by, br in blobs:
        f += (br * br) / (((xs - bx) ** 2 + (ys - by) ** 2) + 1.0)
    return f >= 0.5


def film(img, blobs, alpha=0.20):
    """Translucent amber film plus its outline for a set of puddles (the boss's look)."""
    if not blobs:
        return img
    w, h = img.size
    m = field(blobs, w, h)
    mk = Image.fromarray((m * 255).astype(np.uint8)).resize((w, h), Image.BILINEAR)
    a = np.asarray(mk, np.float32) / 255.0
    edge = np.clip(np.abs(np.gradient(a, axis=0)) + np.abs(np.gradient(a, axis=1)), 0, 1)
    edge = np.clip(edge * 5.0, 0, 1)
    arr = np.asarray(img, np.float32)
    fill = np.array(FILM, np.float32)
    arr = arr + a[..., None] * fill * alpha + edge[..., None] * fill * 0.75
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))


def panel(W, H, draw_fn, blobs=()):
    cv = Canvas(W, H)
    draw_fn(cv)
    return film(cv.image(), list(blobs))


def frame(board, d, img, x0, y0, title, notes, tc=BODY):
    board.paste(img, (x0, y0))
    d.rectangle([x0, y0, x0 + img.size[0], y0 + img.size[1]], outline=(43, 45, 61), width=2)
    d.text((x0 + 12, y0 + 10), title, font=font(20, True), fill=tc)
    for j, s in enumerate(notes):
        d.text((x0 + 4, y0 + img.size[1] + 10 + j * 22), s, font=font(15), fill=(170, 176, 196))


def main():
    out_dir = sys.argv[1]
    W = 1800
    PW, PH = 560, 420          # body panels
    SW, SH = 410, 400          # storyboard panels
    H = 150 + PH + 120 + SH + 200 + 70
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 22), "MIRA 일반 몹 디자인 v1 · 팽창하고, 훑고, 지나간 자리에 오염을 남긴다", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "모델: Mira (오미크론 Ceti) — 맥동하는 적색거성. 물질을 흘리며 충격파와 약 13광년 꼬리를 끌고 다니고, "
           "동반성 Mira B 가 그 물질 일부를 끌어당긴다.", font=font(18), fill=(154, 160, 184))
    d.text((30, 104), "선만 (오염 필름은 ERIDANUS 와 같은 예외) · 점선/가시/판 없음 · 몹 위에 글자 없음 · 몸 주황 / 흘리는 오염은 ERIDANUS 호박색",
           font=font(15), fill=(144, 151, 174))

    R = 70
    states = [
        ("1 수축 (평상시)", 0.0, 0.0, 0.0, ["작게 수축한 상태로 천천히 떠다닌다.", "꼬리·충격파 없음. 아무것도 흘리지 않는다."]),
        ("2 팽창 (예고 1.0초)", 1.0, 0.0, 1.0, ["몸이 부풀고 멈춘다. 진행 방향 쪽에 충격파 호 3개가 켜진다.", "호가 향한 쪽이 곧 훑고 갈 방향이다 (별도 화살표 없음)."]),
        ("3 훑기 (2.0초)", 0.55, 1.0, 0.6, ["충격파를 앞세워 직선으로 달리며 꼬리가 뒤로 길게 흐른다.", "140px 마다 작은 오염 한 점이 꼬리에서 떨어진다."]),
    ]
    for i, (name, sw, tl, sh, notes) in enumerate(states):
        x0 = 30 + i * (PW + 40)
        y0 = 150

        def fn(cv, sw=sw, tl=tl, sh=sh):
            mira(cv, PW * (0.62 if tl else 0.5), PH / 2, R, sw, 0.0, tl, sh)
        frame(board, d, panel(PW, PH, fn), x0, y0, name, notes)

    # storyboard: a top-down field, half scale (1 px = 2 world px)
    sy = 150 + PH + 120
    d.text((30, sy - 44), "한 번의 훑기 (위에서 본 모습, 절반 축척)", font=font(22, True), fill=(241, 242, 248))
    r0 = 17                                       # field radius 17 -> reach ~24 panel px = 48 world px
    path_y = 250
    xs = [70 + 70 * k for k in range(4)]
    px, py = 262, 150                             # player beside the line

    def mob(cv, x, y, swell, tail, shock, R=26):
        mira(cv, x, y, R, swell, 0.0, tail, shock)

    boards = [
        ("A 휴면", lambda cv: (mob(cv, 120, path_y, 0.0, 0.0, 0.0), player(cv, px, py)), [],
         ["평상시. 플레이어는 자유롭다."]),
        ("B 팽창", lambda cv: (mob(cv, 120, path_y, 1.0, 0.0, 1.0), player(cv, px, py)), [],
         ["1.0초 정지. 충격파 호가 가는 길을 알린다."]),
        ("C 훑기", lambda cv: (mob(cv, 345, path_y, 0.55, 0.9, 0.6), player(cv, px, py)),
         [(xs[k] + 6, path_y + 2 * math.sin(k * 2.1), r0 * (1.0 - 0.06 * (4 - k))) for k in range(4)],
         ["달리는 동안 점 4개를 떨군다.", "플레이어는 줄 옆으로 비켜 서면 된다."]),
        ("D 번짐 (2초 뒤)", lambda cv: (mob(cv, 345, 120, 0.0, 0.0, 0.0), player(cv, 150, 150)),
         [(xs[k] + 6, path_y + 2 * math.sin(k * 2.1), r0) for k in range(4)] +
         [(xs[k] + 6 + (6 if k % 2 else -6), path_y + (-1 if k % 2 else 1) * 27, r0 * 0.78) for k in (0, 2)],
         ["점마다 한 번만 새끼 한 점을 옆으로 밀어 낸다.", "새끼는 더 번지지 않는다 (1세대)."]),
    ]
    for i, (name, fn, blobs, notes) in enumerate(boards):
        x0 = 30 + i * (SW + 40)
        frame(board, d, panel(SW, SH, fn, blobs), x0, sy, name, notes, FILM)

    # numbers
    ty = sy + SH + 80
    cols = [
        ("몸체", ["지름 약 110px (LYNX 90px 보다 약간 큼), 체력 로터의 2.0배, 이동 로터의 0.4배",
                  "수축↔팽창을 3.2초 주기로 맥동. 팽창할수록 외곽 고리가 진해진다",
                  "Mira B(마름모)는 몸 둘레를 돌기만 한다 — 판정·기능 없음"]),
        ("훑기 사이클 (7초)", ["수축 3.0초: 플레이어에서 380~520px 떠다님",
                              "팽창 1.0초 정지 → 훑기 2.0초 (속도 로터의 1.6배) → 숨 돌리기 1.0초",
                              "훑기 방향: 플레이어 옆 150px 을 스치는 직선 (몸으로 부딪히진 않는다)"]),
        ("오염점 (ERIDANUS 규칙의 축소판)", ["반지름 48px, 140px 마다 한 점, 한 번에 4점, 수명 5초 (보스 4초)",
                                          "안에 있으면 초당 8 피해 + 이동 -15% (보스 점은 초당 20)",
                                          "2초 뒤 점마다 새끼 한 점 (0.8배), 새끼는 번지지 않음. 한 마리당 최대 10점",
                                          "몹을 잡으면 그 몹의 점은 0.8초 만에 옅어진다"]),
    ]
    for i, (t, ls) in enumerate(cols):
        x0 = 30 + i * 600
        d.text((x0, ty), t, font=font(19, True), fill=FILM)
        for j, s in enumerate(ls):
            d.text((x0, ty + 32 + j * 24), s, font=font(15), fill=(170, 176, 196))
    board.save(os.path.join(out_dir, "Mira_Mob_Design_v1.png"))


if __name__ == "__main__":
    main()
