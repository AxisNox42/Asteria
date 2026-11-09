"""MIRA mob design v4: static core + radial lines, walks plainly, leaves a corruption zone for 3 s when it dies.
Usage: python Mira_Mob_Design_v4.py <out_dir>"""
import math, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, font
from Mira_Mob_Design_v1 import BODY, BODY_HOT, FILM, ring, film, player
from Mira_Mob_Design_v2 import rays, look_b, R

def body(cv, x, y, S=1.0, a=1.0):
    core_r = R * 0.24 * S
    rays(cv, x, y, core_r + R * 0.14 * S, lambda i: R * 0.62 * 0.95 * S * look_b(i), 24, 0.0, a, w=2.0 if S >= 1 else 1.6)
    ring(cv, x, y, core_r, 3.2 if S >= 1 else 2.4, 1.0, BODY_HOT)
    ring(cv, x, y, core_r * 0.38, 1.8 if S >= 1 else 1.4, 0.9, BODY_HOT)

def main():
    out = sys.argv[1]
    W, H = 1760, 940
    b = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(b)
    d.text((30, 22), "MIRA 일반 몹 v4 · 걸어다니는 방사선, 죽으면 오염지대", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "수축·팽창 없음 (몸은 항상 같은 모양). 걷는 동안엔 아무것도 남기지 않고, 죽는 자리에 오염지대가 3초간 생긴다.", font=font(18), fill=(154, 160, 184))
    PW, PH = 520, 300
    cv = Canvas(PW, PH); body(cv, PW / 2, PH / 2)
    b.paste(cv.image(), (30, 130)); d.rectangle([30, 130, 30 + PW, 130 + PH], outline=(43, 45, 61), width=2)
    d.text((42, 140), "몸 (고정)", font=font(19, True), fill=BODY)
    for j, s in enumerate(["코어 + 방사선(B형) 24가닥, 긴 선과 짧은 선이 번갈아.", "방사선은 몸에만 붙고 움직이지 않는다. 지름 약 110px."]):
        d.text((34, 130 + PH + 10 + j * 22), s, font=font(15), fill=(170, 176, 196))
    sy = 130 + PH + 100
    d.text((30, sy - 40), "죽을 때 (위에서, 절반 축척)", font=font(22, True), fill=(241, 242, 248))
    SW, SH = 400, 300
    fr = [("a 걷는 중", [], True, 0.0), ("b 처치: 그 자리에 오염지대", [(200, 160, 34)], False, 1.0), ("c 3초 뒤: 옅어져 사라짐", [(200, 160, 24)], False, 0.35)]
    for i, (nm, bl, alive, k) in enumerate(fr):
        cv = Canvas(SW, SH)
        if alive: body(cv, 130, 160, 26 / R)
        player(cv, 330, 70 if i != 1 else 230)
        img = film(cv.image(), bl, alpha=0.20 * (1 if i < 2 else 0.5))
        x0 = 30 + i * (SW + 40)
        b.paste(img, (x0, sy)); d.rectangle([x0, sy, x0 + SW, sy + SH], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, sy + 10), nm, font=font(18, True), fill=FILM)
    ty = sy + SH + 20
    for j, s in enumerate(["오염지대: 반지름 약 96px 한 덩이, 3초 유지 (끝 0.5초는 옅어진다). 안에 있으면 초당 8 피해 + 이동 -15%.",
                           "가까이서 잡으면 그 자리에 갇히므로, 멀리서 잡거나 잡은 뒤 비켜야 한다. 번짐·새끼 점 없음."]):
        d.text((30, ty + j * 24), s, font=font(15), fill=(170, 176, 196))
    b.save(os.path.join(out, "Mira_Mob_Design_v4.png"))
main()
