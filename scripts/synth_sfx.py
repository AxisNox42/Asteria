# -*- coding: utf-8 -*-
"""Asteria SFX 공용 DSP 도구 — 라우드니스·필터·잔향·서브 킥·허밍 목소리 등.
(현재 효과음 16개는 scripts/build_sfx.py 가 Freesound CC0 후보 풀에서 골라 이 도구로 다듬는다.)

이전 설계 메모:

  · 허밍(웅웅) — 타격 외 모든 소리는 한 '허밍 목소리'로 만든다:
      약간 어긋난 톱니파 2개(맥놀이 6~9 Hz = 웅웅 떨림) + 한 옥타브 아래 사인 + 공진 저역통과.
      올라가는 소리는 필터가 열리고, 내려가는 소리는 기계가 꺼지듯 닫힌다.
      음높이는 BGM 과 같은 B 단조 5음 음계 (Krumhansl & Kessler 1982), 8 kHz 위는 깎음.
  · 타격(모래주머니) — 적 피격·처치·플레이어 피격·폭발은 CC0 펀치 녹음을 짧게 다듬어 쓴다.
      100~400 Hz 몸통이 크고, 짧고(≤ 100 ms), 고역 울림이 적은 것을 분석으로 골랐다
      (Docs/Audio/sfx_src/punch_*.wav, 출처 WiNILL/Resource/Audio/SOURCES.txt).
  · 거부·피해 — 음계 밖 단2도 허밍 + 50 Hz 거칠기 (불협 = 부정)
  · 공용 처리 — 모노, 앞 무음 0, 역할별 라우드니스, 실제 피크 ≤ -1 dBTP, 같은 짧은 잔향

사용:  python scripts/synth_sfx.py   → WiNILL/Resource/Audio/*.wav + Docs/Audio/sfx_meta.json
"""
import json
import os
import numpy as np
from scipy import signal
from scipy.io import wavfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DST = os.path.join(ROOT, "WiNILL", "Resource", "Audio")
DOC = os.path.join(ROOT, "Docs", "Audio")
SRC = os.path.join(DOC, "sfx_src")
SR = 48000

NOTE = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3,
        "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
SCALE_PC = {11, 2, 4, 6, 9}          # B 단조 5음 음계: B D E F# A
EXTRA_PC = {1}                       # C# — BGM 에 쓰여 있어 허용


def hz(name):
    p, o = (name[:2], name[2:]) if name[1] == "#" else (name[:1], name[1:])
    return 440.0 * 2 ** ((NOTE[p] + 12 * (int(o) - 4)) / 12)


class Sound:
    """조각을 시간축에 쌓고, 음 있는 조각의 음높이를 기록한다 (음계·방향 검사용)
    종류: tonal(음계 안) / clash(일부러 음계 밖)"""

    def __init__(self):
        self.parts = []
        self.notes = []

    def add(self, x, at=0.0, db=0.0):
        self.parts.append((x * 10 ** (db / 20), int(at * SR)))

    def note(self, at, f, w=1.0, kind="tonal"):
        self.notes.append((round(at, 4), round(float(f), 2), w, kind))

    def mix(self):
        n = max(o + len(x) for x, o in self.parts)
        y = np.zeros(n)
        for x, o in self.parts:
            y[o:o + len(x)] += x
        return y


def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def tail_fade(x, ms=12):
    """조각 끝을 0 으로 — 잘린 끝에서 '틱' 클릭이 나지 않게"""
    f = min(len(x), int(SR * ms / 1000))
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def curve(a, b, n, shape="exp"):
    return np.geomspace(a, b, n) if shape == "exp" else np.linspace(a, b, n)


def resonant_lp(x, cutoff, q):
    """시간에 따라 움직이는 공진 저역통과 (RBJ biquad, 128 샘플 블록). cutoff: 샘플별 배열"""
    out, z, blk = np.zeros_like(x), np.zeros(2), 128
    for k in range(0, len(x), blk):
        fc = float(np.clip(cutoff[min(k, len(cutoff) - 1)], 40, SR * 0.45))
        w = 2 * np.pi * fc / SR
        al = np.sin(w) / (2 * q)
        cw = np.cos(w)
        b = np.array([(1 - cw) / 2, 1 - cw, (1 - cw) / 2]) / (1 + al)
        a = np.array([1, -2 * cw / (1 + al), (1 - al) / (1 + al)])
        out[k:k + blk], z = signal.lfilter(b, a, x[k:k + blk], zi=z)
    return out


def hum(f, dur, tau, attack=0.012, glide=0.0, glide_tau=None, beat=7.0, cut=(700, 1800), q=3.5, sub=0.5):
    """허밍 목소리: 어긋난 톱니파 2개(맥놀이 beat Hz) + 옥타브 아래 사인 + 공진 저역통과.
    glide: 반음 단위 음높이 이동, cut: 필터 차단점 (시작 Hz, 끝 Hz)"""
    t = t_axis(dur)
    gt = glide_tau or dur * 0.4
    fi = f * 2 ** (glide * (1 - np.exp(-t / gt)) / 12) if glide else np.full_like(t, f)
    y = np.zeros_like(t)
    for detune in (0.0, beat):
        ph = 2 * np.pi * np.cumsum(fi + detune) / SR
        kmax = max(1, int(7000 / (fi.max() + detune)))
        for k in range(1, kmax + 1):          # 대역 제한 톱니파
            y += np.sin(k * ph) / k
    y *= 0.5
    y += sub * np.sin(np.pi * np.cumsum(fi) / SR) * 2      # 옥타브 아래 사인 (무게)
    y = resonant_lp(y, curve(cut[0], cut[1], len(t)), q)
    env = np.minimum(1, t / attack) * np.exp(-t / tau)
    return tail_fade(y * env)


def noise(dur, tau, lo=None, hi=None, seed=0, attack=0.001):
    rng = np.random.default_rng(seed)
    t = t_axis(dur)
    x = rng.standard_normal(len(t))
    if hi:
        x = signal.sosfilt(signal.butter(2, hi, "lowpass", fs=SR, output="sos"), x)
    if lo:
        x = signal.sosfilt(signal.butter(2, lo, "highpass", fs=SR, output="sos"), x)
    x /= np.max(np.abs(x)) + 1e-12
    return tail_fade(x * np.minimum(1, t / attack) * np.exp(-t / tau))


def sweep_noise(dur, tau, f0, f1, q=4.0, seed=0, attack=0.01):
    """공진 필터가 움직이는 노이즈 '휙'"""
    rng = np.random.default_rng(seed)
    t = t_axis(dur)
    x = resonant_lp(rng.standard_normal(len(t)), curve(f0, f1, len(t)), q)
    x /= np.max(np.abs(x)) + 1e-12
    return tail_fade(x * np.minimum(1, t / attack) * np.exp(-t / tau))


def sub_kick(f0, f1, dur, tau, pitch_tau=0.03):
    t = t_axis(dur)
    f = f1 + (f0 - f1) * np.exp(-t / pitch_tau)
    return tail_fade(np.sin(2 * np.pi * np.cumsum(f) / SR) * np.minimum(1, t / 0.001) * np.exp(-t / tau))


def saturate(x, drive=2.5):
    """부드러운 포화(tanh) — 피크(크레스트)를 눌러 크게 들리게, 약간의 거칠기"""
    x = x / (np.max(np.abs(x)) + 1e-12)
    return np.tanh(drive * x) / np.tanh(drive)


def rough(x, hz_=50.0, depth=0.6):
    t = np.arange(len(x)) / SR
    return x * (1 - depth * (0.5 - 0.5 * np.cos(2 * np.pi * hz_ * t)))


def punch(pid, semis=0.0, max_ms=None, tau=None):
    """CC0 펀치 녹음: 앞 무음 제거, 피치 이동(리샘플), 길이 컷·감쇠"""
    sr, x = wavfile.read(os.path.join(SRC, f"punch_{pid}.wav"))
    x = x.astype(np.float64) / 32768.0
    idx = np.where(np.abs(x) > 0.03 * np.max(np.abs(x)))[0]
    x = x[max(0, idx[0] - SR // 1000):]
    if semis:
        r = 2 ** (semis / 12)
        x = signal.resample_poly(x, 1000, int(round(1000 * r)))
    if max_ms:
        x = x[: int(SR * max_ms / 1000)]
    if tau:
        x = x * np.exp(-np.arange(len(x)) / SR / tau)
    f = min(len(x), int(0.006 * SR))
    x[-f:] *= np.linspace(1, 0, f)
    return x / (np.max(np.abs(x)) + 1e-12)


# 공용 잔향 (RT60 0.5 s, 4 kHz 위 감쇠, 프리딜레이 10 ms)
_rng = np.random.default_rng(7)
_t = np.arange(int(0.6 * SR)) / SR
IR = signal.sosfilt(signal.butter(2, 4000, "lowpass", fs=SR, output="sos"),
                    _rng.standard_normal(len(_t)) * np.exp(-6.91 * _t / 0.5))
IR = np.concatenate([np.zeros(int(0.010 * SR)), IR])
IR /= np.sqrt(np.sum(IR ** 2))


def space(x, wet):
    w = signal.fftconvolve(x, IR)
    return np.pad(x, (0, len(w) - len(x))) + w * wet


def k_weight(x):
    b1 = [1.53512485958697, -2.69169618940638, 1.19839281085285]
    a1 = [1.0, -1.69065929318241, 0.73248077421585]
    b2 = [1.0, -2.0, 1.0]
    a2 = [1.0, -1.99004745483398, 0.99007225036621]
    return signal.lfilter(b2, a2, signal.lfilter(b1, a1, x))


def lufs_m(x):
    y = k_weight(x)
    win, hop = int(0.4 * SR), int(0.01 * SR)
    if len(y) < win:
        y = np.pad(y, (0, win - len(y)))
    c = np.cumsum(np.concatenate([[0], y ** 2]))
    starts = np.arange(0, len(y) - win + 1, hop)
    return -0.691 + 10 * np.log10(np.max((c[starts + win] - c[starts]) / win) + 1e-20)


def finish(x, target, tail_db=-60):
    """16 kHz 위 정리 → 역할별 라우드니스 → 실제 피크 ≤ -1 dBTP → 꼬리 정리"""
    x = signal.sosfilt(signal.butter(2, 35, "highpass", fs=SR, output="sos"), x)
    x = signal.sosfilt(signal.butter(4, 16000, "lowpass", fs=SR, output="sos"), x)   # 8 kHz 저역통과는 선명함을 죽여 16 kHz 로
    x = x * 10 ** ((target - lufs_m(x)) / 20)
    tp = np.max(np.abs(signal.resample_poly(x, 4, 1)))
    if tp > 10 ** (-1 / 20):
        x = x * 10 ** (-1 / 20) / tp
    last = np.where(np.abs(x) > 10 ** (tail_db / 20))[0]
    x = x[:min(len(x), (last[-1] if len(last) else len(x)) + int(0.01 * SR))]
    f = min(len(x), int(0.008 * SR))
    x[-f:] *= np.linspace(1, 0, f)
    return x


TARGET = {
    "ui_hover": -31, "ui_confirm": -25, "ui_back": -26, "ui_deny": -25, "aug_flip": -27,
    "sfx_shoot": -27, "enemy_hit": -25, "sfx_pickup_stardust": -24,
    "sfx_chain_zap": -23, "sfx_dash": -23, "skill_cast": -21, "sfx_hurt": -20,
    "sfx_kill": -20, "sfx_levelup": -18, "sfx_explode_m": -17, "sfx_death": -15,
}
STEP = 60 / 117 / 8      # 32분음표 ≈ 64 ms (BGM 117 BPM)
WET = {"ui": 0.08, "combat": 0.05, "hit": 0.03, "event": 0.12}
