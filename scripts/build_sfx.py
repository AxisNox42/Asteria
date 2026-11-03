# -*- coding: utf-8 -*-
"""Asteria SFX 만들기 — Freesound CC0 후보 풀에서 '서로 비슷한 느낌'으로 고른 16개를 같은 기준으로 다듬는다.

선택 방법 (Docs/Audio/SFX_후보풀_설계.md, 기록 Docs/Audio/sfx_selection.json):
  · SFX 마다 검색어 6~9개 × 2쪽으로 CC0 후보 26~44개를 모으고 (총 499개), 전부 분석
  · 역할 적합(길이·방향·모래주머니·UI 날카로움) 을 통과한 것 중,
    이미 고른 소리들과 질감(잡음성·스펙트럼 기울기·변화량·어택·밝기)이 가장 가까운 것을 차례로 고름
  · 동시에 전투음끼리 구별(음색 거리 ≥ 0.8), 크기 ↔ 밝기 순서(피격 > 처치 > 폭발)를 제약으로 둠
  · 결과 세트의 평균 질감 거리 1.78 — 역할에 맞는 후보로 무작위로 짠 세트 2000개(중앙값 2.63)보다 모두 촘촘

처리: 모노, 앞 무음 제거, (필요한 것만) 피치·저역통과·사인 서브 킥, 역할별 라우드니스, 실제 피크 ≤ -1 dBTP

    python scripts/build_sfx.py     → WiNILL/Resource/Audio/*.wav + Docs/Audio/sfx_meta.json
"""
import json
import os
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from synth_sfx import DOC, DST, SR, TARGET, finish, hum, hz, saturate, space, sub_kick  # noqa: E402

SRC = os.path.join(DOC, "sfx_src")

# 게임 파일 ← Freesound 소리 ID (작성자 — 제목)
EXTERNAL = {
    "ui_hover":            "orig_shoot",   # 원래 게임에 있던 사격음 (git c703f92) — 요청: 호버에 가장 어울림
    "sfx_kill":            "536766",   # egomassive — Thud.ogg
    "enemy_hit":           "760563",   # NoisyRedFox — HitSound2 (짧고 단단한 '탁', 자연 감쇠 51 ms)
    "sfx_hurt":            "629656",   # stumpbutt — retro taking damage sfx (짧고 둥근 피격, 자연 감쇠)
    "sfx_shoot":           "617034",   # D4XX — Laser Shot
    "sfx_death":           "159399",   # noirenex — Power Down
    "sfx_explode_m":       "873742",   # The_Elementus — Low_Mid_End_Void_Energy_Blast_Punch_06
    "sfx_dash":            "485279",   # Joao_Janz — Wooden Stick Swing 1_4
    "sfx_chain_zap":       "643923",   # deadrobotmusic — Cool Lil Laser 1
    "sfx_pickup_stardust": "146727",   # Leszek_Szary — energy
    "skill_cast":          "467400",   # SergeyIonov — MnG - Activate.wav
    "sfx_levelup":         "523655",   # LilMati — Powerup 09.wav
    "ui_confirm":          "790532",   # Weak_Hero — mouse click (누름 '딸' + 뗌 '깍')
    "ui_back":             "407465",   # Loyalty_Freak_Music — cancel.mp3
    "ui_deny":             "136787",   # mitchelk — FX019
    "aug_flip":            "8399",   # SpeedY — Clean_Finger_slide_delay.wav
}

# 소리별 추가 처리 (없으면 공통 처리만)
#   semis: 피치(반음)  lp/post_lp: 저역통과 Hz  tau: 지수 감쇠 s  max_ms: 길이 컷
#   sub: 몸통 킥 (f0, f1, 길이, τ, dB)  body_db: 180 Hz 몸통 EQ  snap: '딱' 층 dB (없으면 역할 기본값)
EXTRA = {
    # 타격: 꼬리를 짧게 자르고 빠르게 감쇠 (모래주머니처럼 짧고 묵직하게)
    "sfx_kill":      dict(semis=1, max_ms=180, tau=0.06),   # 원음 높이 그대로 (역할 기본 -1 반음 상쇄 — 낮아지면 어색함)
    "enemy_hit":     dict(semis=1, lp=9000, trim=0.1),   # 자르지 않음, 시작 즉시. semis=1 로 역할 기본 -1 반음을 상쇄 — 원음 높이 그대로 (낮추면 이상하게 들림)
    "sfx_hurt":      dict(semis=1),   # 원음 높이 그대로 (역할 기본 -1 반음 상쇄), 컷 없음
    "sfx_explode_m": dict(max_ms=900),
    "sfx_death":     dict(max_ms=1500),
    # 원음 그대로 (요청): 앞부분 정리·라우드니스만, EQ·포화·잔향·'딱' 층 없음
    "ui_hover":      dict(raw=True),
    "ui_confirm":    dict(raw=True, lp=6500),                        # '딸깍'의 1.5~6 kHz 는 두고 그 위 쨍함만 덜어냄
    # 몸통 킥(100~220 Hz): 밝은 소리 밑에 작은 스피커에서도 들리는 두께
    "sfx_shoot":     dict(sub=(130, 75, 0.08, 0.025, -2)),
    "sfx_chain_zap": dict(sub=(200, 100, 0.1, 0.035, 2)),
    "sfx_dash":      dict(sub=(180, 90, 0.12, 0.05, -3)),
    "skill_cast":    dict(sub=(160, 80, 0.25, 0.08, 3), snap_target=0.20, shape=3.0),
    "sfx_levelup":   dict(sub=(180, 100, 0.4, 0.15, 3), body_db=5),
    "sfx_pickup_stardust": dict(sub=(200, 100, 0.1, 0.04, -2)),
}

# 역할별 공통 처리
#   이전 판은 무게만 좇아(피치 -2~-3, 몸통 +3~5 dB, 8 kHz 저역통과) 1~5 kHz '딱'이 0 % 가 돼 둔하고 뭉개졌다.
#   이번에는 무게는 가볍게 유지하고 선명함을 되살린다:
#   · rumble: 60 Hz 아래를 걷어냄 (작은 스피커에서 안 들리고 라우드니스만 잡아먹음)
#   · semis/body_db/sat: 무게 (낮은 음 = 무거움, Parise & Spence 2009) — 이전보다 약하게
#   · snap: 1.5~5 kHz 짧은 '딱' 층 (단단함 = 어택의 밝기, Freed 1990)
#   · shape: 어택 앞 15 ms 를 키워 느린 소리도 바로 '딱' 시작 (ITU-R BT.1359)
WEIGHT = {
    "ui":     dict(semis=0,  body_db=0.0, sat=0.0, louder=2, snap=-14, shape=0.5),
    "combat": dict(semis=-1, body_db=2.0, sat=1.3, louder=2, snap=-10, shape=1.0),
    "hit":    dict(semis=-1, body_db=3.0, sat=1.6, louder=1, snap=-6,  shape=0.8),
    "event":  dict(semis=-2, body_db=2.0, sat=1.4, louder=2, snap=-10, shape=0.6),
}
NO_SNAP = {"ui_hover", "ui_confirm", "sfx_death", "sfx_levelup", "sfx_pickup_stardust", "aug_flip"}   # 부드러워야 하는 소리 (호버·클릭은 '딱딱함' 피드백)


def snap_layer(ms=8, seed=3):
    """1.5~5 kHz 노이즈 '딱' (지수 감쇠 τ 2 ms)"""
    rng = np.random.default_rng(seed)
    n = int(SR * ms / 1000)
    x = rng.standard_normal(n)
    x = signal.sosfilt(signal.butter(2, [1500, 5000], "bandpass", fs=SR, output="sos"), x)
    x *= np.exp(-np.arange(n) / SR / 0.003)
    return x / (np.max(np.abs(x)) + 1e-12)


def peak_eq(x, f0, db, q=0.8):
    """RBJ 피킹 EQ"""
    a = 10 ** (db / 40)
    w = 2 * np.pi * f0 / SR
    al = np.sin(w) / (2 * q)
    b = np.array([1 + al * a, -2 * np.cos(w), 1 - al * a])
    den = np.array([1 + al / a, -2 * np.cos(w), 1 - al / a])
    return signal.lfilter(b / den[0], den / den[0], x)


KIND = {n: ("ui" if n.startswith(("ui_", "aug_")) else "event" if n in ("sfx_levelup", "sfx_explode_m", "sfx_death")
            else "hit" if n in ("sfx_kill", "enemy_hit", "sfx_hurt") else "combat") for n in EXTERNAL}
WET = {"ui": 0.05, "combat": 0.04, "hit": 0.02, "event": 0.08}


# 합성하는 소리 (원본 녹음 대신)
#   호버: '웅웅' 허밍 — 약간 어긋난 톱니파 2개의 맥놀이 12 Hz(0.2 s 안에 두 번 넘게 떨림),
#   음높이 변화 없음(큰 상승 글라이드는 '귀여움'의 원인이었다), 필터가 살짝 열림, 부드러운 15 ms 어택
def synth_hover():
    return hum(hz("B3"), 0.24, 0.08, attack=0.015, beat=12, cut=(450, 1100), q=3.0, sub=0.15)


SYNTH = {}   # 호버 허밍(synth_hover)은 요청으로 원래 사격음으로 바꿈 — 함수는 남겨 둠


def load(name):
    sr, x = wavfile.read(os.path.join(SRC, f"{name}__{EXTERNAL[name]}.wav"))
    assert sr == SR
    x = x.astype(np.float64) / 32768.0
    return x.mean(axis=1) if x.ndim == 2 else x


SNAP_TARGET = {"ui": 0.08, "combat": 0.10, "hit": 0.12, "event": 0.10}   # 어택 20 ms 의 1~5 kHz 목표 비율


def _band_energy(y):
    f, P = signal.periodogram(np.pad(y, (0, 2048 - len(y) % 2048)), SR)
    return P.sum(), P[(f >= 1000) & (f < 5000)].sum()


def add_snap(x, target):
    """어택 20 ms 의 1~5 kHz 비율이 target 이 되도록 '딱' 층의 크기를 소리마다 계산해 섞는다.
    이미 충분하면 넣지 않고, '딱'의 피크는 원래 소리 피크를 넘지 않게 한다"""
    n = int(0.02 * SR)
    sn = snap_layer()
    ex, ebx = _band_energy(x[:n])
    es, ebs = _band_energy(sn)
    num, den = target * ex - ebx, ebs - target * es
    if num <= 0 or den <= 0:
        return x
    g = min(np.sqrt(num / den), 1.0 * np.max(np.abs(x[:n])) / (np.max(np.abs(sn)) + 1e-12))
    x = x.copy()
    x[: len(sn)] += g * sn[: len(x)]
    return x


def trim_lead(x, rel):
    idx = np.where(np.abs(x) > rel * np.max(np.abs(x)))[0]
    return x[max(0, (idx[0] if len(idx) else 0) - SR // 1000):]


def prepare(name):
    e = EXTRA.get(name, {})
    k = KIND[name]
    w = WEIGHT[k]
    x = SYNTH[name][0]() if name in SYNTH else load(name)
    if e.get("raw"):
        if e.get("lp"):
            x = signal.sosfilt(signal.butter(4, e["lp"], "lowpass", fs=SR, output="sos"), x)
        return finish(trim_lead(x, 0.02), TARGET[name] + w["louder"])
    x = signal.sosfilt(signal.butter(4, 60, "highpass", fs=SR, output="sos"), x)       # 럼블 제거
    x = trim_lead(x, 0.02)   # 세게 자르면 본음 앞의 '딱'(크랙)이 잘려 선명함이 사라진다
    semis = e.get("semis", 0) + w["semis"]
    if semis:
        r = 2 ** (semis / 12)
        x = signal.resample_poly(x, 1000, int(round(1000 * r)))
    if e.get("tau"):
        x = x * np.exp(-np.arange(len(x)) / SR / e["tau"])
    if e.get("max_ms"):
        x = x[: int(SR * e["max_ms"] / 1000)]
        f = min(len(x), int(0.01 * SR))
        x[-f:] *= np.linspace(1, 0, f) ** 2
    if e.get("lp"):
        x = signal.sosfilt(signal.butter(4, e["lp"], "lowpass", fs=SR, output="sos"), x)
    if e.get("sub"):
        f0, f1, dur, tau, db = e["sub"]
        kk = sub_kick(f0, f1, dur, tau)
        kk *= np.sqrt(np.mean(x ** 2)) / (np.sqrt(np.mean(kk ** 2)) + 1e-12) * 10 ** (db / 20)
        kk = np.concatenate([np.zeros(int(0.008 * SR)), kk])   # 8 ms 뒤에: '딱'이 먼저, 몸통이 뒤따름 (몸통이 어택을 덮지 않게)
        n = max(len(x), len(kk))
        x = np.pad(x, (0, n - len(x))) + np.pad(kk, (0, n - len(kk)))
    body = e.get("body_db", w["body_db"])
    if body:
        x = peak_eq(x, 180, body)
    if w["sat"]:
        x = saturate(x, w["sat"])
    if e.get("post_lp"):
        x = signal.sosfilt(signal.butter(4, e["post_lp"], "lowpass", fs=SR, output="sos"), x)
    x = trim_lead(x, e.get("trim", 0.02))
    shape = e.get("shape", w["shape"])
    if shape:   # 트랜지언트 셰이퍼: 앞 15 ms 의 1 kHz 위만 키워 어택을 당김 (저역까지 키우면 몸통이 '딱'을 덮는다)
        t = np.arange(len(x)) / SR
        hi = signal.sosfilt(signal.butter(2, 1000, "highpass", fs=SR, output="sos"), x)
        x = x + hi * (1.5 * shape) * np.exp(-t / 0.015)
    if name not in NO_SNAP:
        x = add_snap(x, e.get("snap_target", SNAP_TARGET[k]))
    return finish(space(x, WET[k]), TARGET[name] + w["louder"])


if __name__ == "__main__":
    meta = {}
    for name in list(EXTERNAL) + list(SYNTH):
        x = prepare(name)
        wavfile.write(os.path.join(DST, name + ".wav"), SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
        if name in SYNTH:
            meta[name] = {"kind": "synth", "source": "scripts/build_sfx.py 합성", "notes": SYNTH[name][1]}
        else:
            src = "원래 게임 사격음 (git c703f92)" if EXTERNAL[name] == "orig_shoot" else "freesound:" + EXTERNAL[name]
            meta[name] = {"kind": "punch" if KIND[name] == "hit" else "external", "source": src, "notes": []}
        print(f"{name:22s} ← {EXTERNAL.get(name, '합성'):7s} {len(x) / SR * 1000:5.0f} ms")
    json.dump(meta, open(os.path.join(DOC, "sfx_meta.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
