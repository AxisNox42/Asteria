import re, sys, os, glob, collections
# Reads the output of smoke_boss_probe.ps1 (out/ = phase 1 runs, out_p2/ = runs started at 35% HP).
# Usage: python scripts/smoke_boss_probe.py [folder holding out/ and out_p2/]
base = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'smoke', 'boss_probe')
NP = {0:'(사이)',2:'REFUGE',3:'CLOSE',7:'AIM',8:'THROW',10:'RAM',12:'DETONATE',13:'WALL',14:'WARP',15:'REINFORCE',16:'CRASH'}

def load(sub, name):
    mob = os.path.join(base, sub, name + '_mob_state.txt')
    if not os.path.exists(mob): return None
    P = {}
    for line in open(mob, encoding='utf-8', errors='replace'):
        m = re.match(r'T(\d+) player x=([-\d.]+) y=([-\d.]+) hp=([-\d.]+) .*time=([-\d.]+)', line)
        if m: P[int(m[1])] = dict(x=float(m[2]), y=float(m[3]), hp=float(m[4]), t=float(m[5]))
    N = {}
    nf = os.path.join(base, sub, name + '_norma_state.txt')
    if os.path.exists(nf):
        for line in open(nf, encoding='utf-8', errors='replace'):
            m = re.match(r'T(\d+) .*hp=([-\d.]+) boss=(\d) pattern=(-?\d+) stage=(-?\d+) exposed=([\d.]+) shots=(\d+)', line)
            if m: N[int(m[1])] = dict(pat=int(m[4]), stage=int(m[5]), exp=float(m[6]), shots=int(m[7]))
    return P, N

def summary(sub, name):
    d = load(sub, name)
    if not d: return None
    P, N = d
    ks = sorted(P)
    if len(ks) < 2: return None
    dur = P[ks[-1]]['t'] - P[ks[0]]['t']
    dmg = P[ks[0]]['hp'] - P[ks[-1]]['hp']
    # seconds in which damage was taken, knockback (displacement not explained by walking is hard; report path length)
    hurt = 0.0; path = 0.0; first = None
    per = collections.defaultdict(lambda: [0.0, 0.0, 0.0])  # pattern -> [sec, dmg, exposed sec]
    for a, b in zip(ks, ks[1:]):
        dt = P[b]['t'] - P[a]['t']; dd = P[a]['hp'] - P[b]['hp']
        path += abs(P[b]['x'] - P[a]['x']) + abs(P[b]['y'] - P[a]['y'])
        if dd > 0.5:
            hurt += dt
            if first is None: first = P[a]['t'] - P[ks[0]]['t']
        if b in N:
            key = NP.get(N[b]['pat'], str(N[b]['pat']))
            per[key][0] += dt; per[key][1] += max(0.0, dd)
            if N[b]['exp'] > 0: per[key][2] += dt
    return dict(dur=dur, dmg=dmg, hurt=hurt, path=path, first=first, per=per)

def row(sub, boss, arg):
    out = []
    for pol in ('still', 'walk'):
        s = summary(sub, f'{boss}_{arg}_{pol}')
        out.append(s)
    return out

print('== 패턴 단독 (약 13초 창): 정지 vs 좌우 왕복 ==')
print(f'{"boss":7}{"pattern":11}{"정지:피해":>10}{"피격초":>7}{"첫피격":>7} | {"왕복:피해":>10}{"피격초":>7}{"이동px":>8}')
for boss, args in (('NORMA', ['AIM','RAM','THROW','DETONATE','WALL','REINFORCE','WARP','REFUGE','CRASH']),
                   ('CAELUM', ['SPREAD','SCAN','SMEAR','SHOT','SIPHON','SPOUT','WAVE','LINE'])):
    for a in args:
        s, w = row('out', boss, a)
        f = lambda v: '   -' if v is None else f'{v:6.1f}'
        if s is None or w is None:
            print(f'{boss:7}{a:11} (없음 still={s is not None} walk={w is not None})'); continue
        print(f'{boss:7}{a:11}{s["dmg"]:10.1f}{s["hurt"]:7.1f}{f(s["first"]):>7} | {w["dmg"]:10.1f}{w["hurt"]:7.1f}{w["path"]:8.0f}   ({s["dur"]:.1f}s/{w["dur"]:.1f}s, 정지 이동 {s["path"]:.0f}px)')

print()
for sub, title in (('out', '자연 진행 1페이즈'), ('out_p2', '자연 진행 2페이즈(체력 35%에서 시작)')):
    for boss, arg in (('NORMA', 'PANES'), ('CAELUM', 'NONE')):
        for pol in ('still', 'walk'):
            s = summary(sub, f'{boss}_{arg}_{pol}')
            if not s: print(f'== {title} {boss} {pol}: 없음'); continue
            print(f'== {title} {boss} {pol}: {s["dur"]:.1f}s, 피해 {s["dmg"]:.1f} ({s["dmg"]/s["dur"]:.2f}/s), 피격 구간 {100*s["hurt"]/s["dur"]:.0f}%, 플레이어 이동 {s["path"]:.0f}px')
            tot = sum(v[0] for v in s['per'].values())
            for k, v in sorted(s['per'].items(), key=lambda kv: -kv[1][0]):
                print(f'     {k:10} {v[0]:6.1f}s ({100*v[0]/tot:4.1f}%)  피해 {v[1]:6.1f} ({v[1]/max(v[0],0.01):5.2f}/s)  노출 {v[2]:4.1f}s')
