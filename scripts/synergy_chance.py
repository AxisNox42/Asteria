"""The "chance" cards: stand-alone cards that raise one probability while a condition holds.

They replace the old amplifier cards that did nothing until another card was taken first. Every
card here works on its own: a condition (when) + a kind (which chance).

Used by scripts/apply_chance_cards.py (patches the table, locale and WiNILL/Entity/SynergyChance.inc).
"""

# cond: code, KR/EN/JP short word for the name, KR/EN/JP phrase for the rule, share of time it holds (model)
CONDS = [
    ("Still",    "정지", "Still",   "静止",   "0.5초 이상 멈춰 있을 때",            "While standing still for 0.5s+",       "0.5秒以上止まっている間", 0.25),
    ("Moving",   "질주", "Sprint",  "疾走",   "움직이는 동안",                      "While moving",                         "移動中",                 0.75),
    ("LowHp",    "위기", "Crisis",  "危機",   "체력이 50% 이하일 때",               "At 50% HP or less",                    "体力50%以下のとき",       0.25),
    ("FullHp",   "만전", "Prime",   "万全",   "체력이 가득 찼을 때",                "At full HP",                           "体力が満タンのとき",      0.45),
    ("Shield",   "방벽", "Bulwark", "防壁",   "보호막이 있을 때",                   "While a shield is up",                 "シールドがあるとき",      0.40),
    ("Dash",     "도약", "Leap",    "跳躍",   "대시 후 2초 동안",                   "For 2s after a dash",                  "ダッシュ後2秒間",         0.15),
    ("Kill",     "연승", "Streak",  "連勝",   "처치 후 3초 동안",                   "For 3s after a kill",                  "撃破後3秒間",             0.80),
    ("Wave",     "개막", "Opening", "開幕",   "웨이브 시작 후 5초 동안",            "For 5s after a wave starts",           "ウェーブ開始後5秒間",     0.20),
    ("Crowd",    "혼전", "Melee",   "混戦",   "주변 300 안에 적이 8명 이상일 때",   "With 8+ enemies within 300",           "周囲300以内に敵が8体以上のとき", 0.50),
    ("Few",      "소수", "Duel",    "少数",   "남은 적이 3명 이하일 때",            "With 3 or fewer enemies left",         "残りの敵が3体以下のとき", 0.30),
    ("Boss",     "거물", "Titan",   "大物",   "보스가 있을 때",                     "While a boss is alive",                "ボスがいるとき",         0.15),
    ("Far",      "원거리", "Sniping", "遠距離", "가장 가까운 적이 350 이상 멀 때",  "When the nearest enemy is 350+ away",  "最も近い敵が350以上離れているとき", 0.35),
    ("Barrage",  "탄막", "Barrage", "弾幕",   "주변 200 안에 적탄이 3발 이상일 때", "With 3+ enemy bullets within 200",     "周囲200以内に敵弾が3発以上のとき", 0.50),
    ("Clean",    "무결", "Flawless", "無傷",  "이번 웨이브에 피해를 받지 않았을 때", "While untouched this wave",            "このウェーブで被弾していないとき", 0.40),
    ("Hurt",     "분노", "Wrath",   "憤怒",   "피해를 받은 후 3초 동안",            "For 3s after taking damage",           "被弾後3秒間",             0.30),
    ("Combo",    "고조", "Fervor",  "高揚",   "처치 콤보가 5 이상일 때",            "At a kill combo of 5+",                "撃破コンボが5以上のとき", 0.35),
    ("Long",     "지구전", "Attrition", "持久", "웨이브 시작 20초 뒤부터",          "From 20s into a wave",                 "ウェーブ開始20秒後から", 0.40),
]

# kind: code, KR/EN/JP short word for the name, KR/EN/JP rule with {p}, value of 1.0 of chance*uptime in model terms
KINDS = [
    ("Crit",   "치명", "Crit",   "会心", "치명타 확률 +{p}%",                      "crit chance +{p}%",                          "会心率+{p}%"),
    ("Extra",  "연발", "Volley", "連発", "사격마다 {p}% 확률로 탄 1발 추가",       "each shot has a {p}% chance to fire 1 extra bullet", "射撃ごとに{p}%の確率で弾を1発追加"),
    ("Pierce", "관통", "Pierce", "貫通", "탄이 {p}% 확률로 관통 +1",               "bullets have a {p}% chance of +1 pierce",    "弾が{p}%の確率で貫通+1"),
    ("Bounce", "도탄", "Ricochet", "跳弾", "탄이 {p}% 확률로 벽 튕김 +1",          "bullets have a {p}% chance of +1 wall bounce", "弾が{p}%の確率で壁跳ね+1"),
    ("Split",  "분열", "Split",  "分裂", "탄이 {p}% 확률로 3갈래 분열 (피해 28%)", "bullets have a {p}% chance to split in 3 on hit (28% damage)", "弾が{p}%の確率で命中時に3方向へ分裂 (ダメージ28%)"),
    ("Blast",  "폭발", "Blast",  "爆発", "탄이 {p}% 확률로 착탄 시 폭발",          "bullets have a {p}% chance to burst where they land", "弾が{p}%の確率で着弾時に爆発"),
]

# (cond, kind, effect strength): the strength is the average chance the card adds (chance * uptime);
# the shown chance is strength / uptime, rounded to 5% and capped at 100%.
PAIRS = []
_strength = [0.16, 0.26, 0.38, 0.52]
_n = 0
for ci, c in enumerate(CONDS):
    for k in range(3 if ci < 8 else 2):
        kind = KINDS[(ci + 2 * k) % 6]
        if _n >= 42:
            break
        PAIRS.append((c[0], kind[0], _strength[(_n * 7 + ci) % 4]))
        _n += 1
