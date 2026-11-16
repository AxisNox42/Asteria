# -*- coding: utf-8 -*-
"""The player laser's sounds (레이저 / 집속 레이저 / 과부하 레이저, WiNILL/Render/LaserFx.h).

numpy only (no scipy). Written to WiNILL/Resource/Audio/sfx/:
  player_laser_charge.wav   0.35 s  a rising whine while fire is held (the charge)
  player_laser_open.wav     0.30 s  the beam snapping on
  player_laser_loop.wav     1.00 s  the hum while the beam stands (seamless loop)
  player_laser_release.wav  1.00 s  과부하 레이저: the stored heat going out as one beam

Pitches sit on the BGM's B minor pentatonic (B D E F# A), like the other hums
(scripts/synth_sfx.py). The loop only uses whole-number frequencies over exactly
one second, so it repeats without a seam.

    python scripts/synth_laser_sfx.py      then  python scripts/gen_audio_embed.py
"""
import os
import wave

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DST = os.path.join(ROOT, "WiNILL", "Resource", "Audio", "sfx")
SR = 44100
rng = np.random.default_rng(20261115)

B2, FS3, B3, D4, FS4, B4, FS5 = 123.47, 185.0, 246.94, 293.66, 369.99, 493.88, 739.99


def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def saw(phase, max_hz, f_hint):
    """Band-limited saw from a phase track (radians); harmonics up to max_hz."""
    n = max(1, int(max_hz / max(f_hint, 1.0)))
    out = np.zeros_like(phase)
    for k in range(1, n + 1):
        out += np.sin(k * phase) / k
    return out * (2 / np.pi)


def phase_of(freq):
    return 2 * np.pi * np.cumsum(freq) / SR


def one_pole_lp(x, cutoff):
    """One-pole low-pass; cutoff may be a per-sample array."""
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=float), x.shape)
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1 - a[i]) * x[i] + a[i] * s
        y[i] = s
    return y


def lp(x, cutoff, poles=2):
    for _ in range(poles):
        x = one_pole_lp(x, cutoff)
    return x


def hp(x, cutoff):
    return x - one_pole_lp(x, cutoff)


def fade(x, a_ms=3, r_ms=10):
    a, r = int(a_ms * SR / 1000), int(r_ms * SR / 1000)
    x = x.copy()
    if a: x[:a] *= np.linspace(0, 1, a)
    if r: x[-r:] *= np.linspace(1, 0, r)
    return x


def write(name, x, peak):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    path = os.path.join(DST, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(data.tobytes())
    print(f"{name}: {len(x) / SR:.2f}s peak {peak:.2f}")


# ── charge: B3 → B4 rising, the wobble speeding up, brightening ─────────────
def charge():
    t = t_axis(0.35)
    u = t / t[-1]
    f = B3 * 2 ** (u ** 1.4)                      # one octave up, faster at the end
    wob = 1 + 0.012 * np.sin(phase_of(6 + 28 * u))
    body = saw(phase_of(f * wob), 5000, B3) + 0.6 * saw(phase_of(f * 1.5 * wob), 5000, B3 * 1.5)
    body = lp(body, 600 + 4200 * u ** 1.5)
    air = hp(rng.standard_normal(len(t)), 3000) * 0.10 * u ** 2
    env = (0.25 + 0.75 * u ** 1.2)
    return fade((body + air) * env, 4, 6)


# ── open: a bright snap down into the hum, with a short fizz ────────────────
def open_():
    t = t_axis(0.30)
    f = FS5 * np.exp(-t / 0.03) + B3 * (1 - np.exp(-t / 0.03))
    zap = saw(phase_of(f), 6000, B3) * np.exp(-t / 0.09)
    sub = np.sin(phase_of(B2 * (1 + 0.5 * np.exp(-t / 0.02)))) * np.exp(-t / 0.12)
    fizz = hp(rng.standard_normal(len(t)), 2500) * np.exp(-t / 0.035) * 0.5
    x = lp(zap, 5000, 1) + 0.7 * sub + fizz
    return fade(x, 1, 20)


# ── loop: steady hum, B2 + F#3 with a 7 Hz beat, a faint crackle ────────────
def loop():
    # Periodic over exactly 1 s: whole-number Hz only, and the noise is tiled.
    n = SR
    t3 = np.arange(3 * n) / SR
    body = np.zeros_like(t3)
    for f0, amp in ((123, 1.0), (130, 0.55), (185, 0.45), (246, 0.30), (370, 0.12)):
        body += amp * saw(2 * np.pi * f0 * t3, 4000, f0)
    body *= 1 + 0.18 * np.sin(2 * np.pi * 7 * t3)      # the 웅웅 beat
    noise = np.tile(rng.standard_normal(n), 3)
    crackle = hp(noise, 4000) * (0.5 + 0.5 * np.sin(2 * np.pi * 13 * t3)) * 0.06
    x = lp(body, 1400) + crackle
    return x[n:2 * n]                                   # the steady middle second


# ── release: a heavy drop with a white-hot sweep and a long tail ────────────
def release():
    t = t_axis(1.0)
    drop = np.sin(phase_of(40 + 110 * np.exp(-t / 0.10))) * np.exp(-t / 0.35)
    f = B4 * np.exp(-t / 0.25) + B2 * (1 - np.exp(-t / 0.25))
    beam = saw(phase_of(f), 7000, B2) + 0.5 * saw(phase_of(f * 1.498), 7000, B2)
    beam = lp(beam, 300 + 6000 * np.exp(-t / 0.18)) * np.exp(-t / 0.40)
    blast = rng.standard_normal(len(t))
    blast = lp(blast, 200 + 7000 * np.exp(-t / 0.06)) * np.exp(-t / 0.22)
    crack = hp(rng.standard_normal(len(t)), 3000) * np.exp(-t / 0.015)
    x = 1.1 * drop + 0.8 * beam + 0.9 * blast + 0.6 * crack
    x = np.tanh(1.6 * x) / np.tanh(1.6)
    return fade(x, 1, 60)


if __name__ == "__main__":
    os.makedirs(DST, exist_ok=True)
    # Levels: the loop plays the whole time the beam stands, so it sits well under
    # the gun shot (sfx_shoot peaks ~0.34); the release is an event (~0.55, saturated so it is still loud).
    write("player_laser_charge", charge(), 0.34)
    write("player_laser_open", open_(), 0.42)
    write("player_laser_loop", loop(), 0.16)
    write("player_laser_release", release(), 0.55)
