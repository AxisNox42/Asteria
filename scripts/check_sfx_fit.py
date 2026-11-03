# -*- coding: utf-8 -*-
"""SFX 적합성 검사 — 합성한 소리가 게임 정체성(BGM 조·음색, UI 시각 언어)과 역할에 맞는지 수치로 확인한다.

    python scripts/check_sfx_fit.py            → 표 출력 + Docs/Audio/sfx_fit.json, sfx_fit.png (실패 시 종료 코드 1)

검사 항목과 근거 (Docs/Audio/SFX_정체성_설계.md):
  음계    음 있는 소리는 BGM 조(B 단조 5음 음계 + C#) 안. 거부·피해음은 일부러 음계 밖 음을 포함  — Krumhansl & Kessler 1982
  큐 조   Audio.cpp 의 Cue 가 원본을 옮기는 반음(semi)도 조 안에 머무는가 (게임에서 실제로 나는 음)
  쨍함    8 kHz 위 에너지 ≤ 8 %
  선명함  어택 20 ms 의 1~5 kHz ≥ 8 % (부드러운 소리 제외, 이전 세트 중앙값 1 %)                                 — Freed 1990
  럼블    60 Hz 아래 ≤ 10 %
  즉시성  소리 시작 ≤ 5 ms                                                                  — ITU-R BT.1359
  길이    역할별 유효 길이 상한
  방향    긍정음은 상승, 뒤로·피해음은 하강 (UI 는 음 개수·윤곽으로 구별)                      — Brewster 1993, Blattner 1989
  계층    역할별 라우드니스 구간                                                            — Grassi 2005, EBU R128
  피크    실제 피크 ≤ -1 dBTP                                                               — EBU R128
  무게    200 Hz 아래 비율 + 작은 스피커에서 들리는 100~400 Hz 몸통 비율 (UI 0.10 / 반복 0.20 / 타격·사건 0.30)  — Parise & Spence 2009
  날카로움 UI 긍정음 ≤ 1.75 acum (근사)                                                     — Zwicker & Fastl, 심리음향 불쾌도
  크기    밝기: 적 피격 > 처치 > 폭발 (물체 크기 순서. 사망은 제외)                           — Parise & Spence 2009
  구별    전투음끼리 음색 거리 ≥ 0.8                                                         — McAdams et al. 1995
  가족    전투음 밝기 퍼짐이 이전 세트보다 좁음                                              — McAdams 1995, Hicks 2024
"""
import json
import os
import re
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_sfx import analyze, plot  # noqa: E402
from synth_sfx import EXTRA_PC, SCALE_PC  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUD = os.path.join(ROOT, "WiNILL", "Resource", "Audio")
DOC = os.path.join(ROOT, "Docs", "Audio")
AUDIO_CPP = os.path.join(ROOT, "WiNILL", "System", "Audio.cpp")

# 역할: (분류, 음높이 방향, 유효 길이 상한 ms, 라우드니스 구간)
ROLE = {
    "ui_hover":            ("ui",   "any",  400,  (-33, -22)),   # 원래 사격음을 그대로 씀 (요청) — 길이는 원음 기준
    "ui_confirm":          ("ui",   "any",  250,  (-33, -22)),   # 큰 상승 글라이드가 '귀여운' 느낌을 만들어 평탄한 기계 클릭으로 바꿈
    "ui_back":             ("ui",   "down", 250,  (-33, -22)),
    "ui_deny":             ("ui",   "any",  250,  (-33, -22)),
    "aug_flip":            ("ui",   "any",  200,  (-33, -22)),
    "sfx_shoot":           ("반복", "down", 150,  (-28, -20)),   # 연사 간격 0.20 s 보다 짧게
    "enemy_hit":           ("반복", "any",  90,   (-30, -20)),   # 쓰로틀 30 ms × 보이스 8. 3 ms 클릭이 피크 한도에 닿아 하한을 -30 으로
    "sfx_pickup_stardust": ("반복", "any",  250,  (-28, -20)),   # 상승은 엔진의 5음 음계 사다리로
    "sfx_chain_zap":       ("반복", "any",  180,  (-28, -20)),
    "sfx_dash":            ("반복", "any",  230,  (-28, -20)),   # 바람 소리는 음높이가 거의 없음
    "skill_cast":          ("타격", "up",   750,  (-22, -17)),
    "sfx_hurt":            ("타격", "any",  250,  (-23, -17)),   # 데스크톱 오류음은 평탄 — 버즈·불협으로 부정 표시
    "sfx_kill":            ("타격", "any",  250,  (-22, -17)),
    "sfx_levelup":         ("사건", "up",   900,  (-20, -12)),
    "sfx_explode_m":       ("사건", "any",  800,  (-20, -12)),
    "sfx_death":           ("사건", "down", 1600, (-20, -12)),
}
CLASH = {"ui_deny", "sfx_hurt"}                    # 음계 밖 음이 '있어야' 하는 소리
SHARP_CHECK = {"ui_hover", "ui_back"}   # 확정은 사용자가 고른 '딸깍' 마우스 클릭 — 본래 밝아서 제외
MIN_LOW = {"반복": 0.08, "타격": 0.15, "사건": 0.25}
# 작은 스피커에서 들리는 무게: 100~400 Hz 몸통 최소 비율 (UI 호버는 가볍게 둠)
MIN_BODY = {"ui": 0.0, "반복": 0.15, "타격": 0.30, "사건": 0.25}    # UI 는 가볍게 둔다. 선명함과 균형 (이전 판은 몸통만 좇다 둔해짐)
COMBAT = ["sfx_shoot", "enemy_hit", "sfx_kill", "sfx_pickup_stardust", "sfx_hurt", "sfx_chain_zap", "sfx_dash"]
PREV_COMBAT_CENTROID = [400, 840, 1732, 1664, 1060, 7645, 885]   # 이전 Freesound 처리본
SFX_FILE = {"Shoot": "sfx_shoot", "Kill": "sfx_kill", "Hurt": "sfx_hurt", "Death": "sfx_death",
            "UiHover": "ui_hover", "UiConfirm": "ui_confirm", "UiDeny": "ui_deny", "LevelUp": "sfx_levelup",
            "CardFlip": "aug_flip", "Dash": "sfx_dash", "EnemyHit": "enemy_hit", "Explosion": "sfx_explode_m",
            "Zap": "sfx_chain_zap", "Pickup": "sfx_pickup_stardust", "SkillCast": "skill_cast", "UiBack": "ui_back"}


def in_key(f, tol_cents=30):
    m = 69 + 12 * np.log2(f / 440.0)
    r = round(m)
    return abs(m - r) * 100 <= tol_cents and (r % 12) in (SCALE_PC | EXTRA_PC)


def direction(notes):
    pts = [(t, f, w) for t, f, w, k in notes if k == "tonal"]
    if len(pts) < 2:
        return 0.0
    t = np.array([p[0] for p in pts]); st = 12 * np.log2(np.array([p[1] for p in pts]) / 440)
    w = np.array([p[2] for p in pts])
    if np.ptp(t) < 1e-6:
        return 0.0
    a = np.polyfit(t, st, 1, w=w)[0]
    return float(a * np.ptp(t))


def sharpness_acum(path):
    """날카로움 근사 (DIN 45692 형식): 바크 대역 에너지^0.23 을 특정 라우드니스로 근사.
    절대값은 정밀하지 않으므로 기준(1.75 acum)과의 비교용 상대 지표로만 쓴다"""
    sr, x = wavfile.read(path)
    x = x.astype(np.float64) / 32768.0
    f, P = signal.welch(x, sr, nperseg=4096)
    z = 13 * np.arctan(0.00076 * f) + 3.5 * np.arctan((f / 7500) ** 2)   # Hz → Bark
    N = np.zeros(24)
    for b in range(24):
        sel = (z >= b) & (z < b + 1)
        N[b] = P[sel].sum() ** 0.23 if sel.any() else 0
    zc = np.arange(24) + 0.5
    g = np.where(zc <= 15.8, 1.0, 0.066 * np.exp(0.171 * zc))
    return float(0.11 * (N * g * zc).sum() / max(N.sum(), 1e-12))


def band_shares(path):
    """(60 Hz 아래 럼블, 앞 20 ms 의 1~5 kHz '딱')"""
    sr, x = wavfile.read(path)
    x = x.astype(np.float64)
    f, P = signal.welch(x, sr, nperseg=2048)
    a = x[: int(0.02 * sr)]
    fa, Pa = signal.welch(np.pad(a, (0, 2048)), sr, nperseg=1024)
    return float(P[f < 60].sum() / P.sum()), float(Pa[(fa >= 1000) & (fa < 5000)].sum() / Pa.sum())


SOFT = {"ui_hover", "ui_confirm", "sfx_death", "sfx_levelup", "sfx_pickup_stardust", "aug_flip"}   # 부드러워야 해서 '딱' 검사 제외


def body_share(path):
    """100~400 Hz 에너지 비율 — 모래주머니처럼 '퍽' 하는 몸통"""
    sr, x = wavfile.read(path)
    f, P = signal.welch(x.astype(np.float64), sr, nperseg=2048)
    return float(P[(f >= 100) & (f < 400)].sum() / P.sum())


def pitch_direction_audio(path, lo=150):
    """음 기록이 없는 외부 소리: 150~6000 Hz 에서 가장 센 주파수의 반음 변화 (서브 킥 제외)"""
    sr, x = wavfile.read(path)
    x = x.astype(np.float64)
    x = np.pad(x, (0, max(0, 4096 - len(x))))   # 아주 짧은 소리(마우스 클릭 등)도 분석되게
    f, t, Z = signal.stft(x, sr, nperseg=2048, noverlap=1536)
    P = np.abs(Z) ** 2
    band = (f > lo) & (f < 6000)
    pk = f[band][np.argmax(P[band], axis=0)]
    e = P.sum(0)
    m = (e > e.max() * 0.05) & (t >= 0.02)   # 앞 20 ms(어택 '딱'·몸통 킥)를 빼고 본음의 움직임만
    if m.sum() < 3:
        return 0.0
    return float(np.polyfit(np.arange(m.sum()), 12 * np.log2(pk[m] / 440.0), 1, w=e[m])[0] * m.sum())


def cue_key_check(meta):
    """Audio.cpp CUE_DEFS 를 읽어, 각 단계가 원본 음을 옮긴 결과가 조 안인지 확인"""
    src = open(AUDIO_CPP, encoding="utf-8").read()
    bad = []
    for cue, body in re.findall(r"/\*\s*(\w+)\s*\*/\s*\{\{(.*?)\}\s*,\s*\d+\s*,", src, re.S):
        for sfx, semi in re.findall(r"S::(\w+),\s*(-?\d+)", body):
            fname = SFX_FILE[sfx]
            for t, f, w, kind in meta[fname]["notes"]:
                if kind == "tonal" and not in_key(f * 2 ** (int(semi) / 12)):
                    bad.append(f"{cue}:{sfx}{int(semi):+d}")
                    break
    notes = meta["sfx_pickup_stardust"]["notes"]
    if notes:   # 외부 소리는 음 기록이 없어 사다리 검사를 건너뜀
        ladder = [int(v) for v in re.search(r"PICKUP_LADDER\[\] = \{([^}]*)\}", src).group(1).split(",")]
        bad += [f"Pickup사다리{v:+d}" for v in ladder if not in_key(notes[0][1] * 2 ** (v / 12))]
    return bad


def main():
    meta = json.load(open(os.path.join(DOC, "sfx_meta.json"), encoding="utf-8"))
    res, raw = {}, []
    for name in ROLE:
        m, r = analyze(os.path.join(AUD, name + ".wav"))
        res[name] = m
        raw.append((m, r))
    rows, fails = [], 0
    for name, (cls, want, maxdur, (lo, hi)) in ROLE.items():
        m, notes = res[name], meta[name]["notes"]
        tonal = [f for t, f, w, k in notes if k == "tonal"]
        clash = [f for t, f, w, k in notes if k == "clash"]
        external = meta[name].get("kind") == "external"
        punchy = meta[name].get("kind") == "punch"
        scale_ok = external or punchy or (all(in_key(f) for f in tonal) and (bool(clash) if name in CLASH else not clash)
                                and all(not in_key(f) for f in clash))
        # 큰 사건(폭발·사망)은 저역에서 움직이므로 60 Hz 부터 본다
        d = direction(notes) if tonal else pitch_direction_audio(os.path.join(AUD, name + ".wav"), 60 if cls == "사건" else 150)
        sharp = sharpness_acum(os.path.join(AUD, name + ".wav"))
        checks = {
            "음계": scale_ok,
            "쨍함": m["air"] <= 0.08,                       # 8 kHz 위 과다 (저역통과를 없앤 뒤 귀 아픈 소리 방지)
            # 호버는 원래 사격음을 원음 그대로 씀(요청) — 서서히 커지는 시작이 그 소리의 성격이라 제외
            "즉시성": name == "ui_hover" or m["onset_ms"] <= 5.0,
            "길이": m["dur95_ms"] <= maxdur,
            "방향": want == "any" or (want == "up" and d > 0.5) or (want == "down" and d < -0.5),
            "계층": lo <= m["lufs_m"] <= hi,
            "피크": m["peak_db"] <= -1.0,
            "무게": (cls not in MIN_LOW or m["low"] >= MIN_LOW[cls] or name == "sfx_pickup_stardust")
                    and (name == "ui_hover" or body_share(os.path.join(AUD, name + ".wav")) >= MIN_BODY[cls]),
            "날카로움": name not in SHARP_CHECK or sharp <= 1.75,
            # 선명함: 어택 20 ms 의 1~5 kHz 가 8 % 이상 (단단함 = 어택의 밝기, Freed 1990).
            #   기준값은 근거 없는 선택 — 이전 세트 중앙값 1 % 의 약 8 배. 에너지 비율이라 저역 몸통에 눌려 낮게 나온다
            "선명함": name in SOFT or band_shares(os.path.join(AUD, name + ".wav"))[1] >= 0.08,
            # 럼블: 60 Hz 아래 ≤ 10 % (작은 스피커에서 안 들리고 라우드니스만 잡아먹음)
            "럼블": band_shares(os.path.join(AUD, name + ".wav"))[0] <= 0.10,
            # 모래주머니 타격감: 짧고(유효 ≤ 120 ms) 100~400 Hz 몸통이 30 % 이상
            # 플레이어 피격은 자연 감쇠를 살려 260 ms 까지 (강제로 자르면 '끊기는' 느낌 — 2026-11-03 피드백)
            "모래주머니": not punchy or (m["dur95_ms"] <= (260 if name == "sfx_hurt" else 120)
                                         and body_share(os.path.join(AUD, name + ".wav")) >= 0.30),
        }
        fails += sum(not v for v in checks.values())
        rows.append(dict(name=name, role=cls, onset_ms=m["onset_ms"], dur95_ms=m["dur95_ms"], max_ms=maxdur,
                         pitch_semis=round(d, 1), want=want, lufs=m["lufs_m"], centroid=m["centroid_hz"],
                         low=m["low"], air=m["air"], sharp=round(sharp, 2),
                         checks={k: bool(v) for k, v in checks.items()}))

    print(f"{'소리':22s} {'분류':4s} {'길이':>5s}/{'상한':<5s} {'음높이':>6s} {'기대':5s} {'LUFS':>6s} {'중심Hz':>7s} {'저역':>5s} {'acum':>5s}  결과")
    for r in rows:
        bad = [k for k, v in r["checks"].items() if not v]
        print(f"{r['name']:22s} {r['role']:4s} {r['dur95_ms']:5d}/{r['max_ms']:<5d} {r['pitch_semis']:6.1f} {r['want']:5s} "
              f"{r['lufs']:6.1f} {r['centroid']:7d} {r['low']:5.2f} {r['sharp']:5.2f}  " + ("통과" if not bad else "실패: " + ", ".join(bad)))

    cue_bad = cue_key_check(meta)
    print("\n[큐 조] 엔진이 옮긴 음도 조 안:", "통과" if not cue_bad else "실패 " + ", ".join(cue_bad))
    fails += bool(cue_bad)

    c = {n: res[n]["centroid_hz"] for n in ROLE}
    order = ["enemy_hit", "sfx_kill", "sfx_explode_m"]   # 사망(전원 꺼짐)은 물체 크기가 아니라 순서에서 뺌
    size_ok = all(c[order[i]] > c[order[i + 1]] for i in range(len(order) - 1))
    print("[크기] 밝기 피격 > 처치 > 폭발:", " > ".join(f"{n}={c[n]}" for n in order), "→", "통과" if size_ok else "실패")
    fails += not size_ok

    F = np.array([[np.log(res[n]["centroid_hz"]), np.log(res[n]["attack_ms"] + 1),
                   res[n]["flux"], np.log(res[n]["dur95_ms"] + 1)] for n in COMBAT])
    Z = (F - F.mean(0)) / (F.std(0) + 1e-9)
    pairs = sorted(((float(np.linalg.norm(Z[i] - Z[j])), COMBAT[i], COMBAT[j])
                    for i in range(len(COMBAT)) for j in range(i + 1, len(COMBAT))))
    dist_ok = pairs[0][0] >= 0.8
    print("[구별] 가장 가까운 전투음 쌍:", ", ".join(f"{a}↔{b} {d:.2f}" for d, a, b in pairs[:3]), "→", "통과" if dist_ok else "실패")
    fails += not dist_ok

    now = float(np.std(np.log2([c[n] for n in COMBAT])))
    prev = float(np.std(np.log2(PREV_COMBAT_CENTROID)))
    fam_ok = now < prev
    print(f"[가족] 전투음 밝기 퍼짐 {now:.2f} 옥타브 (이전 세트 {prev:.2f}) →", "통과" if fam_ok else "실패")
    fails += not fam_ok

    json.dump({"rows": rows, "cue_key_bad": cue_bad, "size_order": size_ok, "closest_pairs": pairs[:3],
               "spread_oct": [now, prev]}, open(os.path.join(DOC, "sfx_fit.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    plot(raw, os.path.join(DOC, "sfx_fit.png"))
    print(f"\n실패 {fails}건")
    return fails


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
