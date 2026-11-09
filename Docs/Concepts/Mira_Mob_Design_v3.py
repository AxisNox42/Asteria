"""MIRA mob design v3: a plain walker, core + radial lines (look B), drops corruption as it walks.
Usage: python Mira_Mob_Design_v3.py <out_dir>"""
import math, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, font
from Mira_Mob_Design_v1 import BODY, BODY_HOT, FILM, ring, film, player
from Mira_Mob_Design_v2 import rays, core, look_b, R

def body(cv, x, y, breath, S=1.0):
    cr = (0.86 + 0.26 * breath)
    core_r = R * 0.24 * cr * S
    ls = 0.7 + 0.45 * breath
    rays(cv, x, y, core_r + R * 0.14 * S, lambda i: R * 0.62 * ls * S * look_b(i), 24, 0.0, 0.6 + 0.4 * breath, w=2.0 if S >= 1 else 1.6)
    ring(cv, x, y, core_r, 3.2 if S >= 1 else 2.4, 1.0, BODY_HOT)
    ring(cv, x, y, core_r * 0.38, 1.8 if S >= 1 else 1.4, 0.9, BODY_HOT)

def main():
    out = sys.argv[1]
    W, H = 1700, 900
    b = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(b)
    d.text((30, 22), "MIRA 일반 몹 v3 · 그냥 걸어다니는 방사선", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "훑기·예고 없음. 코어 + 방사선(B형) 하나가 느리게 숨 쉬며 걸어 다니고, 걸은 자리에 오염점을 떨군다.", font=font(18), fill=(154, 160, 184))
    PW, PH = 520, 300
    for i, (nm, br) in enumerate((("숨 들이쉼 (수축)", 0.0), ("숨 내쉼 (팽창)", 1.0))):
        cv = Canvas(PW, PH); body(cv, PW / 2, PH / 2, br); x0 = 30 + i * (PW + 40)
        b.paste(cv.image(), (x0, 130)); d.rectangle([x0, 130, x0 + PW, 130 + PH], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, 140), nm, font=font(19, True), fill=BODY)
    d.text((30 + 2 * (PW + 40) + 8, 150), "방사선은 몸에만 붙고 3.2초 주기로\n길이와 밝기만 오르내린다.\n상태 전환·예고선·충격파 없음.", font=font(16), fill=(170, 176, 196))
    sy = 130 + PH + 70
    d.text((30, sy - 40), "걷는 모습 (위에서, 절반 축척)", font=font(22, True), fill=(241, 242, 248))
    SW, SH = 800, 330
    xs = [90 + 70 * k for k in range(6)]
    pu = [(x, 190 + 3 * math.sin(k * 1.9), 17) for k, x in enumerate(xs)]
    ch = [(xs[k] + (6 if k % 2 else -6), 190 + (-1 if k % 2 else 1) * 27, 13) for k in (1, 3)]
    for i, (nm, blobs, mx) in enumerate((("걷는 중: 140px 걸을 때마다 한 점씩", pu[:4], 360), ("2초 뒤: 점마다 새끼 한 점 (1세대)", pu + ch, 560))):
        cv = Canvas(SW, SH)
        body(cv, mx, 190, 0.4, S=26 / R); player(cv, 600, 70)
        img = film(cv.image(), blobs); x0 = 30 + i * (SW + 40)
        b.paste(img, (x0, sy)); d.rectangle([x0, sy, x0 + SW, sy + SH], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, sy + 10), nm, font=font(18, True), fill=FILM)
    b.save(os.path.join(out, "Mira_Mob_Design_v3.png"))
main()
