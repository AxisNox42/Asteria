# 응집 선택: 역할 적합도 + 이미 고른 소리들과의 음색(질감) 유사도, 구별·크기 순서는 제약
import json, math, re, numpy as np
C = json.load(open("candidates.json", encoding="utf-8")); F = json.load(open("features.json"))
META = {c["id"]: c for v in C.values() for c in v}
# 방향: 묵직한 SF 임팩트 — 레트로·8비트·장난감 계열은 빼고, SF·에너지·임팩트 계열에 가산
STYLE_NEG = re.compile(r"8.?bit|retro|chip|ps1|nes|basket|ball|cartoon|cute|kid|toy|family|arcade|coin|mario|bubble|hurt_c|chiptone|sfxr|bfxr", re.I)
STYLE_POS = re.compile(r"sci.?fi|scifi|futur|energy|plasma|laser|blaster|impact|bass|heavy|hud|hologram|power|shield|tesla|electric|space|synth|cannon|boom|thud|punch", re.I)
# 역할 이름과 맞는 제목 가산, 다른 UI 역할 이름이면 감산 (예: 호버 자리에 'Cancel')
ROLE_KW = {
 "ui_hover": r"hover|blip|tick|over", "ui_confirm": r"confirm|select|accept|ok|apply", "ui_back": r"back|cancel|close|return",
 "ui_deny": r"error|deni|deny|wrong|invalid|fail", "aug_flip": r"swipe|flip|slide|whoosh|swish|click",
 "sfx_shoot": r"laser|shot|blast|gun|fire", "enemy_hit": r"hit|impact|punch|thud|contact", "sfx_kill": r"impact|thud|punch|kick|hit",
 "sfx_hurt": r"hit|impact|punch|thud|damage|hurt", "sfx_pickup_stardust": r"pick|collect|item|energy|orb",
 "sfx_chain_zap": r"zap|electric|spark|shock|tesla|arc", "sfx_dash": r"whoosh|dash|swoosh|swish",
 "skill_cast": r"activate|power|charge|ability|shield|cast|surge", "sfx_levelup": r"level|power.?up|upgrade|achiev|success|rise",
 "sfx_explode_m": r"explo|boom|blast", "sfx_death": r"power.?down|shut|death|game.?over|collapse"}
UI_ROLES = ["ui_hover", "ui_confirm", "ui_back", "ui_deny"]
VOICE = re.compile(r"voice|vocal|grunt|scream|yell|man |woman|male|female|girl|boy|speech|says|ouch|groan|laugh|music|loop|song", re.I)
# (최대 유효 ms, 방향, 모래주머니, UI날카로움)
ROLE = {
 "sfx_kill": (300, "any", True, False), "enemy_hit": (300, "any", True, False), "sfx_hurt": (300, "any", True, False),
 "sfx_shoot": (150, "down", False, False), "sfx_death": (1500, "down", False, False), "sfx_explode_m": (700, "any", False, False), "sfx_dash": (200, "any", False, False),
 "sfx_chain_zap": (160, "any", False, False), "sfx_pickup_stardust": (250, "any", False, False), "skill_cast": (700, "up", False, False),
 "sfx_levelup": (900, "up", False, False),
 "ui_confirm": (250, "up", False, True), "ui_back": (250, "down", False, True), "ui_hover": (120, "any", False, True),
 "ui_deny": (250, "any", False, False), "aug_flip": (200, "any", False, False)}
ORDER = list(ROLE)
COMBAT = ["sfx_shoot", "enemy_hit", "sfx_kill", "sfx_pickup_stardust", "sfx_hurt", "sfx_chain_zap", "sfx_dash"]
def tex(m):   # 질감 벡터: 잡음성, 스펙트럼 기울기, 변화량, 어택, 밝기(가중 아주 낮게 — 역할마다 밝기는 달라도 됨)
    return np.array([math.log(m["flatness"] + 1e-4), m["tilt"] / 6, m["flux"] * 5, math.log(m["attack_ms"] + 1), 0.15 * math.log2(m["centroid_hz"] + 1)])
allv = np.array([tex(m) for m in F.values()]); MU, SD = allv.mean(0), allv.std(0) + 1e-9
def z(m): return (tex(m) - MU) / SD
def tim(m): return np.array([math.log(m["centroid_hz"] + 1), math.log(m["attack_ms"] + 1), m["flux"], math.log(m["dur95_ms"] + 1)])
def fit(name, m):
    maxd, want, sand, ui = ROLE[name]
    if m["dur95_ms"] > maxd or m["onset_ms"] > (60 if sand else 20): return None   # 앞부분은 처리 단계에서 다시 자름
    if want == "up" and m["dir"] <= 0.5: return None
    if want == "down" and m["dir"] >= -0.5: return None
    if sand and m["body"] < 0.30: return None   # 길이는 처리 단계에서 150 ms 로 자름
    if ui and m["sharp"] > 1.75: return None
    if m["air"] > 0.30: return None                      # 8 kHz 위가 아주 많은 쨍한 소리만 제외
    if m["sub60"] > 0.25: return None                    # 60 Hz 아래 럼블이 큰 녹음 제외 (작은 스피커에서 안 들림)
    # 원본에 '딱'이 있어야 함 (처리만으로는 자연스러운 어택을 만들 수 없었다). 부드러워야 하는 소리는 제외
    if name not in ("ui_hover", "sfx_death", "sfx_levelup", "sfx_pickup_stardust", "aug_flip") and m["snap"] < 0.10: return None
    s = 0.15 * math.log10(META[m["id"]]["downloads"] + 10)   # 커뮤니티 검증(다운로드 수)
    if sand: s += m["body"]                              # 몸통이 클수록 '퍽'
    if name in ("sfx_explode_m", "sfx_death", "sfx_levelup"): s += 0.3 * m["b80"]
    # 무게 = 80~250 Hz 두께 + 어택 1~5 kHz '딱' (이전에는 저역 비율만 봐서 럼블 녹음이 뽑히고 선명함이 없었다)
    s += (0.6 * m["snap"]) if ui else (0.6 * m["b80"] + 0.8 * m["snap"])
    if want != "any": s += min(abs(m["dir"]), 24) / 48   # 윤곽이 분명할수록
    return s
chosen = {}
def ok_constraints(name, m):
    c = {**{k: F[v] for k, v in chosen.items()}, name: m}
    cen = lambda k: c[k]["centroid_hz"]
    for a, b in (("enemy_hit", "sfx_kill"), ("sfx_kill", "sfx_explode_m")):   # 사망은 처리 단계에서 어둡게
        # 처리에서 큰 사건은 타격보다 피치를 더 내리므로 15 % 여유 (최종 순서는 check_sfx_fit.py 가 처리 후 엄격히 검사)
        if a in c and b in c and not cen(a) * 1.15 > cen(b): return False
    cs = [k for k in COMBAT if k in c]
    if name in COMBAT and len(cs) >= 2:
        T = np.array([tim(c[k]) for k in cs]); Z = (T - T.mean(0)) / (T.std(0) + 1e-9)
        i = cs.index(name)
        if min(np.linalg.norm(Z[i] - Z[j]) for j in range(len(cs)) if j != i) < 0.8: return False
    return True
report = {}
for name in ORDER:
    cands = []
    for c in C[name]:
        m = F.get(c["id"])
        if not m or c["id"] in chosen.values() or VOICE.search(c["title"]) or STYLE_NEG.search(c["title"]): continue
        if ROLE[name][2] and re.search(r"swing|swish|whoosh|swoosh", c["title"], re.I): continue   # 타격 자리에 바람 소리 제외
        f = fit(name, m)
        if f is None: continue
        if STYLE_POS.search(c["title"]): f += 0.5
        if re.search(ROLE_KW[name], c["title"], re.I): f += 0.6
        if name in UI_ROLES and any(re.search(ROLE_KW[o], c["title"], re.I) for o in UI_ROLES if o != name): f -= 0.8
        coh = 0.0 if not chosen else -np.linalg.norm(z(m) - np.mean([z(F[v]) for v in chosen.values()], axis=0)) / 2
        cands.append((f + coh, f, coh, c["id"]))
    cands.sort(reverse=True)
    pick = next((x for x in cands if ok_constraints(name, F[x[3]])), None)
    report[name] = {"pool": len(C[name]), "eligible": len(cands), "pick": pick, "alts": cands[:4]}
    if pick: chosen[name] = pick[3]
for name, r in report.items():
    p = r["pick"]
    if not p: print(f"{name:20s} 후보 {r['pool']:2d} 적합 {r['eligible']:2d}  선택 없음"); continue
    m = META[p[3]]
    print(f"{name:20s} 후보 {r['pool']:2d} 적합 {r['eligible']:2d}  → {p[3]} {m['user'][:14]:14s} {m['title'][:30]:30s} 총점 {p[0]:.2f} (적합 {p[1]:.2f}, 응집 {p[2]:.2f})")
json.dump({"chosen": chosen, "report": report}, open("selection.json", "w"), indent=1)
