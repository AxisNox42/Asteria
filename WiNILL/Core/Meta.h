#pragma once
// ─────────────────────────────────────────────────────────────
// 메타 진행 — 판마다 코인 적립 → 영구 업그레이드 구매 (저장됨)
// Apply permanent meta stats to PlayerStats when a run starts.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include "PlayerStats.h"
#include "Settings.h"   // CurLangIdx 는 Augment.h 에 있지만 언어 인덱스만 필요

// The META_RESERVED_* slots preserve saved meta indices after removing the
// obsolete start-vision, start-augment and identity-slot upgrades.
// META_MAGNET is the 1-stardust module the first shop visit has the player buy
// (appended, not put in a reserved slot, so old saves cannot already own it).
enum MetaId { META_HP, META_DMG, META_MOVE, META_RESERVED_VISION, META_RESERVED_STARTAUG, META_RESERVED_IDSLOT,
              META_MAGNET,
              // Shop batch 1 (2026-11-16). Append only: saves store levels by index.
              META_ATKSPD, META_REGEN, META_SHOTSPD, META_CRIT, META_CRITDMG,
              META_COOLDOWN, META_LUCK, META_REWARD, META_INTEREST,
              // Shop batch 2 (2026-11-16)
              META_ELITEDMG, META_BOSSDMG, META_BERSERK, META_KILLHEAL, META_BOUNTY,
              META_WARD, META_BOSSPREP, META_WAVEHEAL,
              // Shop batch 3 (2026-11-16)
              META_FIRSTCARD, META_CODEX, META_COMBO, META_DURATION,
              META_COUNT };
inline bool MetaSlotRemoved(int id) {
    return id == META_RESERVED_VISION || id == META_RESERVED_STARTAUG || id == META_RESERVED_IDSLOT;
}

struct MetaDef {
    const wchar_t* name[3];   // KR / EN / JP
    int            maxLv;
    long long      baseCost;  // 1레벨 비용 (레벨마다 ×1.6)
    const wchar_t* desc[3];   // shop detail line, exact numbers (KR / EN / JP)
};
inline const MetaDef META_DEFS[META_COUNT] = {
    { { L"최대 체력 +20",  L"Max HP +20",       L"最大HP +20" },   5, 100,
      { L"레벨마다 최대 체력 +20", L"Max HP +20 per level", L"レベルごとに最大HP +20" } },
    { { L"공격력 +6%",     L"Attack +6%",       L"攻撃力 +6%" },   5, 150,
      { L"레벨마다 공격력 +6%", L"Attack +6% per level", L"レベルごとに攻撃力 +6%" } },
    { { L"이동속도 +5%",   L"Move Speed +5%",   L"移動速度 +5%" }, 3, 120,
      { L"레벨마다 이동속도 +5%", L"Move speed +5% per level", L"レベルごとに移動速度 +5%" } },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0, { L"", L"", L"" } },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0, { L"", L"", L"" } },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0, { L"", L"", L"" } },
    { { L"별가루 자석 +15%", L"Stardust Magnet +15%", L"星屑マグネット +15%" }, 1, 1,
      { L"별가루가 15% 더 먼 곳에서 날아온다", L"Stardust flies in from 15% farther",
        L"星屑が15%遠くから飛んでくる" } },
    { { L"공격 속도 +5%",  L"Fire Rate +5%",    L"攻撃速度 +5%" }, 5, 130,
      { L"레벨마다 발사 속도 +5%", L"Fire rate +5% per level", L"レベルごとに発射速度 +5%" } },
    { { L"체력 재생 +0.1", L"HP Regen +0.1",    L"HP再生 +0.1" },  5, 140,
      { L"레벨마다 초당 체력 회복 +0.1", L"+0.1 HP regenerated per second per level",
        L"レベルごとに毎秒HP回復 +0.1" } },
    { { L"투사체 속도 +8%", L"Shot Speed +8%",  L"弾速 +8%" },     3, 100,
      { L"레벨마다 탄 속도 +8%", L"Bullet speed +8% per level", L"レベルごとに弾速 +8%" } },
    { { L"치명타 확률 +3%", L"Crit Chance +3%", L"クリティカル率 +3%" }, 5, 200,
      { L"레벨마다 치명타 확률 +3%p", L"Crit chance +3 points per level",
        L"レベルごとにクリティカル率 +3%p" } },
    { { L"치명타 피해 +10%", L"Crit Damage +10%", L"クリティカルダメージ +10%" }, 5, 200,
      { L"레벨마다 치명타 피해 배율 +0.1 (기본 ×2.0)", L"Crit damage multiplier +0.1 per level (base x2.0)",
        L"レベルごとにクリティカル倍率 +0.1（基本 ×2.0）" } },
    { { L"스킬 쿨다운 -4%", L"Skill Cooldown -4%", L"スキルクールダウン -4%" }, 5, 220,
      { L"레벨마다 액티브 스킬 재사용 대기시간 -4%", L"Active skill cooldowns -4% per level",
        L"レベルごとにアクティブスキルの待ち時間 -4%" } },
    { { L"행운 +1",        L"Luck +1",          L"幸運 +1" },      3, 200,
      { L"레벨마다 행운 +1 (1점당 확률 ×1.1)", L"Luck +1 per level (each point: odds x1.1)",
        L"レベルごとに幸運 +1（1点ごとに確率 ×1.1）" } },
    { { L"정산 별가루 +10%", L"Run Reward +10%", L"精算星屑 +10%" }, 5, 250,
      { L"게임이 끝날 때 받는 별가루가 레벨마다 +10%", L"Stardust paid at the end of a run +10% per level",
        L"プレイ終了時にもらえる星屑がレベルごとに +10%" } },
    { { L"별가루 이자",    L"Stardust Interest", L"星屑の利子" },  1, 400,
      { L"게임이 끝날 때 보유 별가루의 2%를 더 받는다 (최대 50)",
        L"At the end of a run, gain 2% of your stardust (up to 50)",
        L"プレイ終了時、所持星屑の2%を追加でもらう（最大50）" } },
    { { L"엘리트 추가 피해 +8%", L"Elite Damage +8%", L"エリート追加ダメージ +8%" }, 3, 250,
      { L"레벨마다 엘리트에게 주는 피해 +8%", L"Damage to elites +8% per level",
        L"レベルごとにエリートへのダメージ +8%" } },
    { { L"보스 추가 피해 +8%", L"Boss Damage +8%", L"ボス追加ダメージ +8%" }, 3, 300,
      { L"레벨마다 보스에게 주는 피해 +8%", L"Damage to bosses +8% per level",
        L"レベルごとにボスへのダメージ +8%" } },
    { { L"궁지의 분노",    L"Last Stand",       L"窮地の怒り" },   3, 260,
      { L"체력 30% 이하일 때 레벨마다 주는 피해 +8%", L"Below 30% HP: damage +8% per level",
        L"HP30%以下のとき、レベルごとにダメージ +8%" } },
    { { L"처치 시 회복",   L"Kill Mend",        L"撃破回復" },     3, 240,
      { L"처치할 때 레벨마다 3% 확률로 체력 +3", L"On kill: 3% chance per level to heal 3 HP",
        L"撃破時、レベルごとに3%の確率でHP +3" } },
    { { L"엘리트 현상금",  L"Elite Bounty",     L"エリート懸賞金" }, 3, 120,
      { L"엘리트를 쓰러뜨리면 레벨마다 별가루 +5", L"Elites drop +5 stardust per level",
        L"エリート撃破でレベルごとに星屑 +5" } },
    { { L"웨이브 보호막",  L"Wave Ward",        L"ウェーブ防壁" }, 1, 400,
      { L"웨이브가 시작될 때마다 피해 1번을 막는다", L"Blocks one hit at the start of every wave",
        L"ウェーブ開始ごとに被弾を1回防ぐ" } },
    { { L"보스 전 정비",   L"Boss Prep",        L"ボス前の整備" }, 1, 300,
      { L"보스 웨이브가 시작될 때 체력 20% 회복", L"Heal 20% HP when a boss wave starts",
        L"ボスウェーブ開始時にHP20%回復" } },
    { { L"웨이브 사이 회복", L"Wave Mend",      L"ウェーブ間回復" }, 3, 110,
      { L"웨이브를 깰 때마다 레벨마다 체력 3% 회복", L"Heal 3% HP per level when a wave is cleared",
        L"ウェーブクリアごとにレベルごとにHP3%回復" } },
    { { L"첫 카드 보장",   L"First Card Boon",  L"初回カード保証" }, 1, 350,
      { L"게임의 첫 카드 선택에 희귀 이상 카드 1장 보장", L"The first card pick of a run has a Rare or better card",
        L"プレイ最初のカード選択にレア以上を1枚保証" } },
    { { L"도감 해독기",    L"Codex Decoder",    L"図鑑デコーダー" }, 1, 50,
      { L"한 번 쓰러뜨린 몹은 도감 정보(능력치·공격 패턴)가 전부 열린다",
        L"Defeat an enemy once to open its full codex data (stats and attacks)",
        L"一度倒した敵は図鑑情報（ステータス・攻撃）がすべて開く" } },
    { { L"콤보 집중",      L"Combo Focus",      L"コンボ集中" },   3, 90,
      { L"레벨마다 콤보 유지 +0.4초, 25콤보 이상일 때 피해 +3%",
        L"Per level: combo lasts +0.4s, damage +3% at 25+ combo",
        L"レベルごとにコンボ持続 +0.4秒、25コンボ以上でダメージ +3%" } },
    { { L"지속 시간 +8%",  L"Duration +8%",     L"持続時間 +8%" }, 3, 180,
      { L"레벨마다 액티브 스킬 소환물·장판 지속 +8%", L"Active skill summons and fields last +8% per level",
        L"レベルごとにアクティブスキルの召喚物・フィールド持続 +8%" } },
};

inline long long g_Coins = 0;
inline int       g_MetaLv[META_COUNT] = { 0 };

// 다음 레벨 구매 비용 (이미 만렙이면 -1)
inline long long MetaNextCost(int id) {
    if (id < 0 || id >= META_COUNT) return -1;
    if (g_MetaLv[id] >= META_DEFS[id].maxLv) return -1;
    double c = (double)META_DEFS[id].baseCost;
    for (int i = 0; i < g_MetaLv[id]; i++) c *= 1.6;
    return (long long)c;
}

inline const wchar_t* MetaName(int id) {
    int li = LangIndex();
    return META_DEFS[id].name[li];
}
inline const wchar_t* MetaDesc(int id) {
    return META_DEFS[id].desc[LangIndex()];
}

// Stardust spent on every level bought so far (what a refund gives back).
inline long long MetaSpentOn(int id) {
    if (id < 0 || id >= META_COUNT) return 0;
    long long total = 0;
    double c = (double)META_DEFS[id].baseCost;
    for (int i = 0; i < g_MetaLv[id]; i++) { total += (long long)c; c *= 1.6; }
    return total;
}

// Stardust starts flying to the ship from this much farther (Stardust Magnet: +15%).
inline float MetaStardustPullMult() {
    return 1.0f + 0.15f * (float)g_MetaLv[META_MAGNET];
}

// Active skill cooldowns x this (Skill Cooldown: -4% per level).
inline float MetaCooldownMult() {
    return 1.0f - 0.04f * (float)g_MetaLv[META_COOLDOWN];
}
inline int MetaLuck() { return g_MetaLv[META_LUCK]; }

// Player HP / max HP and the live combo, kept current by the game loop
// (Last Stand and Combo Focus read them).
inline float g_MetaPlayerHpFrac = 1.0f;
inline int   g_MetaComboNow = 0;

// Multiplier on one player hit on an enemy: Elite / Boss Damage by target, and
// Last Stand while the player is at 30% HP or less.
inline float MetaPlayerHitMult(bool elite, bool boss) {
    float m = 1.0f;
    if (elite) m *= 1.0f + 0.08f * (float)g_MetaLv[META_ELITEDMG];
    if (boss)  m *= 1.0f + 0.08f * (float)g_MetaLv[META_BOSSDMG];
    if (g_MetaLv[META_BERSERK] > 0 && g_MetaPlayerHpFrac <= 0.30f)
        m *= 1.0f + 0.08f * (float)g_MetaLv[META_BERSERK];
    if (g_MetaLv[META_COMBO] > 0 && g_MetaComboNow >= 25)
        m *= 1.0f + 0.03f * (float)g_MetaLv[META_COMBO];
    return m;
}
inline float MetaKillHealChance() { return 0.03f * (float)g_MetaLv[META_KILLHEAL]; }
inline constexpr float kMetaKillHealHp = 3.0f;
inline int   MetaEliteBounty() { return 5 * g_MetaLv[META_BOUNTY]; }
inline bool  MetaWaveWard() { return g_MetaLv[META_WARD] > 0; }
inline float MetaBossPrepFrac() { return g_MetaLv[META_BOSSPREP] > 0 ? 0.20f : 0.0f; }
inline float MetaWaveMendFrac() { return 0.03f * (float)g_MetaLv[META_WAVEHEAL]; }

// First Card Boon: armed at run start, spent by the first wave-reward pick.
inline bool  g_MetaFirstCardPending = false;
// Combo Focus.
inline constexpr int kMetaComboHot = 25;
inline float MetaComboWindowBonus() { return 0.4f * (float)g_MetaLv[META_COMBO]; }
// Duration: lasting active-skill objects (clone, wall, prism, storm, gate, turret).
inline float MetaDurationMult() { return 1.0f + 0.08f * (float)g_MetaLv[META_DURATION]; }

// End-of-run stardust: Run Reward (+10% per level), then Interest
// (2% of the stardust held once the reward is in, at most 50).
// held: stardust owned before this run's reward is added.
inline long long MetaRunReward(long long earned, long long held) {
    if (earned < 0) earned = 0;
    long long out = (long long)((double)earned * (1.0 + 0.10 * (double)g_MetaLv[META_REWARD]));
    if (g_MetaLv[META_INTEREST] > 0)
        out += std::min<long long>(50, std::max<long long>(0, held + out) / 50);
    return out;
}

inline void ApplyMeta(PlayerStats& s) {
    s.maxHP            += 20.0f * g_MetaLv[META_HP];
    s.damageMultiplier *= (1.0f + 0.06f * (float)g_MetaLv[META_DMG]);
    s.moveSpeedMult    *= (1.0f + 0.05f * (float)g_MetaLv[META_MOVE]);
    s.fireInterval     /= (1.0f + 0.05f * (float)g_MetaLv[META_ATKSPD]);
    s.regenPerSec      += 0.1f * (float)g_MetaLv[META_REGEN];
    s.bulletSpeed      *= (1.0f + 0.08f * (float)g_MetaLv[META_SHOTSPD]);
    s.critChance       += 3 * g_MetaLv[META_CRIT];
    s.critMult         += 0.1f * (float)g_MetaLv[META_CRITDMG];
}
