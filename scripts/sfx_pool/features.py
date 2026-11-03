import os, sys, json, warnings
import numpy as np
from scipy.io import wavfile
from scipy import signal
warnings.filterwarnings("ignore")
ROOT = sys.argv[1]; sys.path.insert(0, os.path.join(ROOT, "scripts"))
from analyze_sfx import analyze
from check_sfx_fit import sharpness_acum, body_share, pitch_direction_audio
SR = 48000
def one(fn):
    try:
        sr, x = wavfile.read("wav/" + fn); x = x.astype(np.float64)
        x = x.mean(1) if x.ndim > 1 else x
        if np.max(np.abs(x)) < 1e-4: return None
        idx = np.where(np.abs(x) > 0.03 * np.max(np.abs(x)))[0]; x = x[max(0, idx[0] - 48):]
        x = np.pad(x, (0, max(0, 9600 - len(x)))) / np.max(np.abs(x)) * 0.5
        p = "trim/" + fn; wavfile.write(p, SR, (x * 32767).astype(np.int16))
        m, _ = analyze(p)
        f, P = signal.welch(x, SR, nperseg=2048); sel = (f > 80) & (f < 12000)
        tilt = float(np.polyfit(np.log2(f[sel]), 10 * np.log10(P[sel] + 1e-20), 1)[0])   # dB/옥타브
        Pn = P / P.sum()
        a20 = x[: int(0.02 * SR)]
        fa, Pa = signal.welch(np.pad(a20, (0, 2048)), SR, nperseg=1024); Pa /= Pa.sum()
        m.update(id=fn[:-4], body=round(body_share(p), 3), dir=round(pitch_direction_audio(p), 1),
                 sharp=round(sharpness_acum(p), 2), tilt=round(tilt, 2),
                 sub60=round(float(Pn[f < 60].sum()), 3),                       # 작은 스피커에서 안 들리는 럼블
                 b80=round(float(Pn[(f >= 80) & (f < 250)].sum()), 3),          # 무게(두께)
                 pres=round(float(Pn[(f >= 1000) & (f < 5000)].sum()), 3),      # 선명함
                 snap=round(float(Pa[(fa >= 1000) & (fa < 5000)].sum()), 3))    # 어택 20 ms 의 '딱'

        return m
    except Exception as e:
        return None
if __name__ == "__main__":
    from concurrent.futures import ProcessPoolExecutor
    os.makedirs("trim", exist_ok=True)
    with ProcessPoolExecutor(8) as ex: res = [r for r in ex.map(one, sorted(os.listdir("wav")), chunksize=8) if r]
    json.dump({r["id"]: r for r in res}, open("features.json", "w"), indent=0)
    print("features", len(res))
