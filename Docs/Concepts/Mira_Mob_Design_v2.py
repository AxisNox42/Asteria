"""MIRA mob design v2: just a core with radiating lines around it.

Feedback on v1 (2026-11-05): wanted only a core with radial lines around it;
the lines are always there, only around the core, and are never left behind
(no tail, no bow shock, no rings, no companion). Three radial-line looks,
each in its three states. The shed puddles stay the only thing it leaves.
Usage: python Mira_Mob_Design_v2.py <out_dir>
"""
import math, os, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, TAU, font
from Mira_Mob_Design_v1 import BODY, BODY_HOT, FILM, PLAYER, ring, film, frame, player

R = 70


def rays(cv, x, y, r_in, lens, n, spin, alpha, w=2.0, col=BODY, lead=None, lead_len=0.0):
    """n straight rays from r_in outward; lens(i) gives each ray's length.
    lead: angle of one extra-long, brighter ray (the way it will run)."""
    for i in range(n):
        a = spin + TAU * i / n
        L = lens(i)
        cv.seg((x + math.cos(a) * r_in, y + math.sin(a) * r_in),
               (x + math.cos(a) * (r_in + L), y + math.sin(a) * (r_in + L)), w, alpha, col=col)
    if lead is not None:
        cv.seg((x + math.cos(lead) * r_in, y + math.sin(lead) * r_in),
               (x + math.cos(lead) * (r_in + lead_len), y + math.sin(lead) * (r_in + lead_len)),
               2.6, 1.0, col=BODY_HOT)


def core(cv, x, y, r, hot):
    ring(cv, x, y, r, 3.2, 1.0, BODY_HOT if hot else BODY)
    ring(cv, x, y, r * 0.38, 1.8, 0.9, BODY_HOT)


# state: (core radius, ray length scale, alpha, lead ray)
STATES = [
    ("1 수축 (평상시)", 0.80, 0.55, 0.65, False, ["코어가 작고 방사선이 짧고 어둡다.", "방사선은 코어에만 붙어 있다."]),
    ("2 팽창 (예고 1.0초)", 1.20, 1.00, 1.00, True, ["코어가 부풀고 방사선이 길고 밝아진다.", "한 가닥만 더 길고 하얗게 켜져 훑을 방향을 알린다."]),
    ("3 훑기 (2.0초)", 1.00, 0.85, 0.90, False, ["방사선은 그대로 몸에 붙어 따라온다 (뒤에 남지 않는다).", "지나간 자리에는 오염점만 남는다."]),
]


def look_a(i):      # even corona: 24 equal rays
    return 1.0


def look_b(i):      # alternate long / short
    return 1.0 if i % 2 == 0 else 0.45


def look_c(i):      # three lengths, a slow ordered rhythm
    return (1.0, 0.62, 0.38, 0.62)[i % 4]


LOOKS = [
    ("A 균일한 코로나", look_a, 24, "방사선 24가닥이 같은 길이. 가장 단순하다."),
    ("B 긴 것 / 짧은 것", look_b, 24, "긴 선과 짧은 선이 번갈아. 별빛 같은 리듬."),
    ("C 세 길이 리듬", look_c, 32, "길이가 4칸 주기로 반복. 가장 촘촘하다."),
]


def body_look(cv, x, y, look, n, st, t=0.0, heading=0.0):
    _, cr, ls, al, lead, _ = st
    core_r = R * 0.24 * cr
    r_in = core_r + R * 0.14
    base = R * 0.62 * ls
    rays(cv, x, y, r_in, lambda i: base * look(i), n, spin=t, alpha=al,
         lead=(heading if lead else None), lead_len=base * 1.7)
    core(cv, x, y, core_r, lead)


def main():
    out_dir = sys.argv[1]
    W, PW, PH = 1800, 560, 330
    H = 150 + 3 * (PH + 56) + 330
    board = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(board)
    d.text((30, 22), "MIRA 일반 몹 디자인 v2 · 코어 + 방사선, 몸에만 붙고 남지 않는다", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "v1 에서 고리·충격파·꼬리·Mira B 를 모두 뺐다. 몸 = 코어 + 방사선 하나뿐. 방사선은 항상 코어 주변에만 있고 지나간 자리엔 오염점만 남는다.",
           font=font(18), fill=(154, 160, 184))
    d.text((30, 104), "선만 · 점선 없음 · 방사선은 가늘고 곧은 선이라 칼날 가시와 다르다 (끝이 뾰족하게 두꺼워지지 않고, 길이도 짧다)",
           font=font(15), fill=(144, 151, 174))
    for r_i, st in enumerate(STATES):
        y0 = 150 + r_i * (PH + 56)
        for c_i, (nm, lk, n, note) in enumerate(LOOKS):
            x0 = 30 + c_i * (PW + 40)
            cv = Canvas(PW, PH)
            body_look(cv, PW / 2, PH / 2, lk, n, st, t=0.13 * c_i, heading=0.0)
            img = cv.image()
            title = f"{nm} · {st[0]}" if True else nm
            board.paste(img, (x0, y0))
            d.rectangle([x0, y0, x0 + PW, y0 + PH], outline=(43, 45, 61), width=2)
            d.text((x0 + 12, y0 + 10), title, font=font(19, True), fill=BODY)
            if c_i == 0:
                for j, s in enumerate(st[5]):
                    d.text((x0 + 4, y0 + PH + 8 + j * 20), s, font=font(15), fill=(170, 176, 196))
            elif r_i == 0:
                d.text((x0 + 4, y0 + PH + 8), note, font=font(15), fill=(170, 176, 196))
    # A: the same body leaving puddles (shed)
    sy = 150 + 3 * (PH + 56) + 20
    d.text((30, sy - 8), "훑기 한 번 (A 형 기준, 위에서 본 모습, 절반 축척)", font=font(22, True), fill=(241, 242, 248))
    SW, SH = 410, 220
    xs = [70 + 70 * k for k in range(4)]
    py = 250 - 120

    def mob(cv, x, y, st):
        cr, ls, al, lead = st[1:5]
        S = 26.0 / R
        core_r = R * 0.24 * cr * S
        rays(cv, x, y, core_r + R * 0.14 * S, lambda i: R * 0.62 * ls * S, 24, 0.0, al, w=1.6,
             lead=(0.0 if lead else None), lead_len=R * 0.62 * ls * S * 1.7)
        ring(cv, x, y, core_r, 2.4, 1.0, BODY_HOT)

    frames = [
        ("a 팽창", lambda cv: (mob(cv, 70, 110, STATES[1]), player(cv, 250, 40)), []),
        ("b 훑기", lambda cv: (mob(cv, 345, 110, STATES[2]), player(cv, 250, 40)),
         [(xs[k] + 6, 112 + 2 * math.sin(k * 2.1), 17) for k in range(4)]),
        ("c 번짐 (2초 뒤)", lambda cv: (mob(cv, 345, 40, STATES[0]), player(cv, 120, 40)),
         [(xs[k] + 6, 112 + 2 * math.sin(k * 2.1), 17) for k in range(4)] +
         [(xs[k] + 6 + (6 if k % 2 else -6), 112 + (-1 if k % 2 else 1) * 27, 13) for k in (0, 2)]),
    ]
    for i, (nm, fn, blobs) in enumerate(frames):
        x0 = 30 + i * (SW + 40)
        cv = Canvas(SW, SH)
        fn(cv)
        img = film(cv.image(), blobs)
        board.paste(img, (x0, sy + 30))
        d.rectangle([x0, sy + 30, x0 + SW, sy + 30 + SH], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, sy + 40), nm, font=font(18, True), fill=FILM)
    d.text((30 + 3 * (SW + 40) + 10, sy + 40), "오염점 규칙은 v1 과 같다", font=font(16, True), fill=FILM)
    for j, s in enumerate(["반지름 48px · 140px 마다 한 점 × 4 · 수명 5초", "초당 8 피해, 이동 -15%",
                           "2초 뒤 새끼 한 점 (1세대)", "잡으면 0.8초 만에 옅어짐"]):
        d.text((30 + 3 * (SW + 40) + 10, sy + 70 + j * 24), s, font=font(15), fill=(170, 176, 196))
    board.save(os.path.join(out_dir, "Mira_Mob_Design_v2.png"))


if __name__ == "__main__":
    main()
