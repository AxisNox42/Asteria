"""MIRA mob design v5: a bare core wrapped in its own corruption zone; the zone stays 3 s after it dies.
Usage: python Mira_Mob_Design_v5.py <out_dir>"""
import math, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from Caelum_Body_Design_A_v1 import Canvas, BG, font
from Mira_Mob_Design_v1 import BODY, BODY_HOT, FILM, ring, film, player

def core(cv, x, y, S=1.0):
    ring(cv, x, y, 15 * S, 3.0 if S >= 1 else 2.2, 1.0, BODY_HOT)
    ring(cv, x, y, 6 * S, 1.8 if S >= 1 else 1.4, 0.95, BODY)

def zone(x, y, r, t=0.0):
    """Corruption hugging the core: a main blob plus two lobes that drift round it."""
    return [(x, y, r), (x + math.cos(t) * r * 0.75, y + math.sin(t) * r * 0.75, r * 0.62),
            (x + math.cos(t + 2.4) * r * 0.8, y + math.sin(t + 2.4) * r * 0.8, r * 0.55)]

def main():
    out = sys.argv[1]
    W, H = 1760, 960
    b = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(b)
    d.text((30, 22), "MIRA 일반 몹 v5 · 코어에 오염지대가 붙어 다닌다", font=font(36, True), fill=(241, 242, 248))
    d.text((30, 74), "방사선·고리 등 선은 전부 제거. 몸 = 코어 하나 + 코어에 붙은 오염지대. 죽으면 그 오염지대가 제자리에 3초 남는다.", font=font(18), fill=(154, 160, 184))
    PW, PH = 520, 300
    cv = Canvas(PW, PH); core(cv, PW / 2, PH / 2, 2.2)
    img = film(cv.image(), zone(PW / 2, PH / 2, 54), alpha=0.20)
    b.paste(img, (30, 130)); d.rectangle([30, 130, 30 + PW, 130 + PH], outline=(43, 45, 61), width=2)
    d.text((42, 140), "몸 (코어 + 붙은 오염지대)", font=font(19, True), fill=BODY)
    for j, s in enumerate(["코어는 고리 두 겹뿐. 오염지대는 코어를 감싸 천천히 일렁인다 (반지름 약 80px).", "지대에 닿은 플레이어는 초당 8 피해 + 이동 -15%. 접촉 피해는 이것으로 대신한다."]):
        d.text((34, 130 + PH + 10 + j * 22), s, font=font(15), fill=(170, 176, 196))
    sy = 130 + PH + 110
    d.text((30, sy - 40), "걷다가 죽을 때 (위에서, 절반 축척)", font=font(22, True), fill=(241, 242, 248))
    SW, SH = 400, 300
    fr = [("a 걷는 중: 지대가 코어를 따라온다", 300, False, 1.0), ("b 처치: 지대가 제자리에 남는다", 200, True, 1.0), ("c 3초 뒤: 옅어져 사라짐", 200, True, 0.45)]
    for i, (nm, mx, dead, al) in enumerate(fr):
        cv = Canvas(SW, SH)
        if not dead: core(cv, mx, 150, 26 / 70 * 1.6)
        player(cv, 90 if i == 0 else 330, 60 if i != 1 else 230)
        r = 28
        bl = zone(mx, 150, r, 0.6 * i) if al == 1.0 else [(mx, 150, 20)]
        img = film(cv.image(), bl, alpha=0.20 * (1 if al == 1.0 else 0.5))
        x0 = 30 + i * (SW + 40)
        b.paste(img, (x0, sy)); d.rectangle([x0, sy, x0 + SW, sy + SH], outline=(43, 45, 61), width=2)
        d.text((x0 + 12, sy + 10), nm, font=font(18, True), fill=FILM)
    ty = sy + SH + 20
    for j, s in enumerate(["걷는 동안 지대가 지나간 자리에 남지 않는다 (코어에만 붙는다). 죽는 순간에만 제자리에 3초 남고, 마지막 0.5초는 옅어진다.",
                           "가까이서 잡으면 그 자리에 갇히므로 멀리서 잡거나 잡은 뒤 비켜야 한다. 번짐·새끼 없음."]):
        d.text((30, ty + j * 24), s, font=font(15), fill=(170, 176, 196))
    b.save(os.path.join(out, "Mira_Mob_Design_v5.png"))
main()
