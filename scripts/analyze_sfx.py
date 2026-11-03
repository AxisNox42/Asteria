# -*- coding: utf-8 -*-
# SFX 파형·스펙트럼 분석: 48kHz float WAV 폴더를 읽어 지표 표 + 파형/스펙트로그램 PNG
import sys, os, json
import numpy as np
from scipy.io import wavfile
from scipy import signal

SR = 48000

def k_weight(x):
    # ITU-R BS.1770 K-weighting (48 kHz 계수)
    b1 = [1.53512485958697, -2.69169618940638, 1.19839281085285]
    a1 = [1.0, -1.69065929318241, 0.73248077421585]
    b2 = [1.0, -2.0, 1.0]
    a2 = [1.0, -1.99004745483398, 0.99007225036621]
    return signal.lfilter(b2, a2, signal.lfilter(b1, a1, x, axis=0), axis=0)

def loudness_max_momentary(x):
    # 400ms 창 최대 라우드니스(LUFS 근사). 짧은 효과음은 통합 라우드니스 대신 이 값을 비교
    y = k_weight(x)
    win = int(0.4 * SR); hop = int(0.01 * SR)
    if len(y) < win: y = np.pad(y, ((0, win - len(y)), (0, 0)))
    best = -120.0
    for s in range(0, len(y) - win + 1, hop):
        ms = np.mean(y[s:s + win] ** 2, axis=0).sum()
        if ms > 0: best = max(best, -0.691 + 10 * np.log10(ms))
    return best

def analyze(path):
    sr, x = wavfile.read(path)
    assert sr == SR
    x = x.astype(np.float64) / (32768.0 if x.dtype.kind == "i" else 1.0)
    if x.ndim == 1: x = x[:, None]
    mono = x.mean(axis=1)
    peak = np.max(np.abs(x))
    # 엔벨로프 (5ms RMS)
    w = int(0.005 * SR)
    env = np.sqrt(np.convolve(mono ** 2, np.ones(w) / w, mode="same"))
    ep = env.max()
    i_peak = int(np.argmax(env))
    above10 = np.where(env >= 0.1 * ep)[0]; above90 = np.where(env >= 0.9 * ep)[0]
    onset = above10[0] if len(above10) else 0
    attack_ms = (above90[0] - onset) / SR * 1000 if len(above90) else 0
    # 감쇠: 피크 이후 -20 dB, -40 dB 까지
    post = env[i_peak:]
    def t_drop(db):
        idx = np.where(post <= ep * 10 ** (db / 20))[0]
        return idx[0] / SR * 1000 if len(idx) else len(post) / SR * 1000
    # 유효 길이: 에너지 95%
    e = np.cumsum(mono ** 2); e /= e[-1] if e[-1] > 0 else 1
    dur95 = (np.searchsorted(e, 0.95) - onset) / SR * 1000
    # 스펙트럼 (에너지 가중 평균)
    f, t, Z = signal.stft(np.pad(mono, (0, max(0, 2048 - len(mono)))), SR, nperseg=1024, noverlap=768)
    P = np.abs(Z) ** 2
    frame_e = P.sum(axis=0)
    wts = frame_e / frame_e.sum()
    cent = (f[:, None] * P).sum(axis=0) / np.maximum(P.sum(axis=0), 1e-20)
    centroid = float((cent * wts).sum())
    Pm = (P * wts).sum(axis=1)
    cum = np.cumsum(Pm) / Pm.sum()
    rolloff = float(f[np.searchsorted(cum, 0.85)])
    flat = np.exp(np.mean(np.log(Pm + 1e-20))) / np.mean(Pm + 1e-20)
    bands = [(20, 200), (200, 2000), (2000, 8000), (8000, 20000)]
    bshare = [float(Pm[(f >= lo) & (f < hi)].sum() / Pm.sum()) for lo, hi in bands]
    # 스펙트럼 변화량 (flux) — 음색 공간의 세 번째 축
    Pn = P / np.maximum(P.sum(axis=0, keepdims=True), 1e-20)
    flux = float((np.sqrt((np.diff(Pn, axis=1) ** 2).sum(axis=0)) * wts[1:]).sum())
    # 센트로이드 궤적 기울기 (상승/하강 음)
    act = frame_e > frame_e.max() * 0.01
    tt = t[act]; cc = cent[act]
    slope = float(np.polyfit(tt, cc, 1)[0]) if len(tt) > 3 else 0.0
    corr = float(np.corrcoef(x[:, 0], x[:, 1])[0, 1]) if x.shape[1] == 2 and np.std(x[:, 1]) > 0 else 1.0
    return dict(
        file=os.path.basename(path)[:-4], ch=x.shape[1], len_ms=round(len(mono) / SR * 1000),
        dur95_ms=round(dur95), onset_ms=round(onset / SR * 1000, 1), attack_ms=round(attack_ms, 1),
        decay20_ms=round(t_drop(-20)), decay40_ms=round(t_drop(-40)),
        peak_db=round(20 * np.log10(peak), 1), lufs_m=round(loudness_max_momentary(x), 1),
        crest_db=round(20 * np.log10(peak / (np.sqrt(np.mean(mono[onset:] ** 2)) + 1e-12)), 1),
        centroid_hz=round(centroid), rolloff85_hz=round(rolloff), flatness=round(float(flat), 3),
        low=round(bshare[0], 2), mid=round(bshare[1], 2), high=round(bshare[2], 2), air=round(bshare[3], 2),
        flux=round(flux, 3), cent_slope_hz_s=round(slope), stereo_corr=round(corr, 2),
    ), (mono, env, f, t, P)

def plot(results, out_png):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    n = len(results)
    fig, axes = plt.subplots(n, 2, figsize=(12, 1.35 * n), gridspec_kw={"width_ratios": [1, 1.3]})
    for i, (m, (mono, env, f, t, P)) in enumerate(results):
        tx = np.arange(len(mono)) / SR * 1000
        a = axes[i, 0]; a.plot(tx, mono, lw=0.4, color="#3a7bd5"); a.plot(tx, env, lw=0.8, color="#d9534f")
        a.set_xlim(0, 2000); a.set_ylim(-1, 1); a.set_yticks([]); a.set_ylabel(m["file"], rotation=0, ha="right", fontsize=7)
        a.tick_params(labelsize=6)
        b = axes[i, 1]; b.pcolormesh(t * 1000, f, 10 * np.log10(P + 1e-12), shading="auto", vmin=-110, vmax=-20, cmap="magma")
        b.set_yscale("symlog", linthresh=500); b.set_ylim(50, 20000); b.set_xlim(0, 2000); b.tick_params(labelsize=6)
    fig.tight_layout(); fig.savefig(out_png, dpi=110)

if __name__ == "__main__":
    folder, out_json, out_png = sys.argv[1], sys.argv[2], sys.argv[3]
    files = sorted(p for p in os.listdir(folder) if p.endswith(".wav"))
    res = [analyze(os.path.join(folder, p)) for p in files]
    json.dump([m for m, _ in res], open(out_json, "w"), indent=1)
    keys = list(res[0][0].keys())
    print(" | ".join(keys))
    for m, _ in res: print(" | ".join(str(m[k]) for k in keys))
    plot(res, out_png)
