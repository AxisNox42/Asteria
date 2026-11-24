#pragma once
// Card upgrades (augment rework; Docs/Plans/2026-11-24_반복강화_가중치.md).
// Every upgradable card has three branches, and each can be taken again and again:
//   A  its key number            (UpRow::A, a cost card's cost grows with it: UpRow::A2)
//   B  a second number           (UpRow::B; chance cards: their condition is eased, ChanceCondB)
//   C  its family's shape (Family): x1, x2, x3 ... with every pick of that family's C
// A number taken n times is a + (b - a) * n, stopping at `lim` where it would stop making
// sense (UpAt); a branch at its limit is no longer offered. A wave clear shows three
// (card, branch) offers. This table is the only place the numbers live: SynergyRuntime.cpp
// reads them (UPA / UPA2 / UPB) and the card text is built from it.
//
// Not upgraded: frames (picked at run start), mythics, and the two laser-only amps (their
// numbers live in LaserFx; the rework moves them to a frame-linked pool).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace synergy {

enum class Family { None, Pierce, Bounce, Split, Blast, Track, Orbit, Element, Mobility, Guard, Beat, Crit, Count };

// Chance cards: the same kinds and conditions as SynergyChance.inc (SynergyRuntime's CKind / CCond).
enum class ChKind { Crit, Extra, Pierce, Bounce, Split, Blast };
enum class ChCond { Still, Moving, LowHp, FullHp, Shield, Dash, Kill, Wave, Crowd, Few, Boss, Far, Barrage, Clean, Hurt, Combo, Long, Count };

inline Family ChanceFamily(ChKind k) {
    switch (k) {
    case ChKind::Crit:   return Family::Crit;
    case ChKind::Extra:  return Family::Beat;
    case ChKind::Pierce: return Family::Pierce;
    case ChKind::Bounce: return Family::Bounce;
    case ChKind::Split:  return Family::Split;
    case ChKind::Blast:  return Family::Blast;
    }
    return Family::None;
}

// What a number is, for the card text (KR, EN, JP).
enum class UpLabel {
    None, Bounces, Pierce, Split, SplitDmg, Homing, Damage, Max, Targets, PerShot, Every, Chance,
    Shots, Shield, Heal, Seconds, Range, PerDamage, PerKill, PerWave, PerTag, PerCard, FireRate,
    MaxHp, Taken, PerSecond, BounceDmg, AfterRedirect, HomingDmg, Duration, PerBullet, MarkTaken,
    SideDmg, SideShots, DiceOutcome, DiceCount, MaxDamage, MaxDistance, ExtraShots, TriggerDamage,
    StillTime, GrazesNeeded, UsesPerRun, HpLine, StoreCap, ShotsEach, MaxFireRate, MaxRateHp,
    SizePerSec, ReviveHp, AfterReviveTaken, DashCdCut, BreakDamage, ShieldTaken, Recharge, Immune,
    ClearRange, KillsNeeded, Slow, Reflect, Saves, MaxKinds, MaxCards, PullDamage, Count_
};
// How a number reads: Pct "12%", PlusPct "+12%" / "-10%" (0 alone: just the new value),
// Mult "×3", Count / Flat "12", Plus "+2", Sec "3초", Ring "4○".
enum class UpUnit { Pct, PlusPct, Mult, Count, Flat, Plus, Sec, Ring };

inline constexpr float kNoLimit = 3.0e38f;
struct UpVal {
    UpLabel l = UpLabel::None; UpUnit u = UpUnit::Count;
    float a = 0.0f, b = 0.0f;      // base → after one pick (every further pick adds b - a again)
    float lim = kNoLimit;          // where repeated picks stop
    bool on() const { return l != UpLabel::None; }
};
// The value after n picks.
inline float UpAt(const UpVal& v, int n) {
    float x = v.a + (v.b - v.a) * (float)std::max(0, n);
    if (v.lim < kNoLimit) x = v.b >= v.a ? std::min(x, v.lim) : std::max(x, v.lim);
    return x;
}
// One more pick still changes it.
inline bool UpCanGrow(const UpVal& v, int n) { return std::fabs(UpAt(v, n + 1) - UpAt(v, n)) > 1e-5f; }
struct UpRow {
    const char* code;
    Family fam;
    UpVal A, A2, B;   // A2: what grows along with A (a cost card's cost); optional
};

inline const UpRow* UpRows(int& n) {
    using L = UpLabel; using U = UpUnit; using F = Family;
    static const UpVal none{};
    static const UpRow k[] = {
        // ── 동사 ──
        { "V_BOUNCE",    F::Bounce,  { L::Bounces, U::Count, 2, 3 },        none, { L::BounceDmg, U::PlusPct, 0.0f, 0.10f } },
        { "V_RICOCHET",  F::Bounce,  { L::Bounces, U::Count, 1, 2, 5 },        none, { L::AfterRedirect, U::PlusPct, 0.0f, 0.20f } },
        { "V_PIERCE",    F::Pierce,  { L::Pierce, U::Count, 2, 3 },         none, { L::Damage, U::PlusPct, 0.0f, 0.10f } },
        { "V_SPLIT",     F::Split,   { L::Split, U::Count, 3, 4, 5 },          none, { L::SplitDmg, U::Pct, 0.28f, 0.35f, 0.70f } },
        { "V_HOMING",    F::Track,   { L::Homing, U::PlusPct, 0.0f, 0.60f, 1.80f }, none, { L::HomingDmg, U::PlusPct, 0.0f, 0.15f } },
        { "V_BLAST",     F::Blast,   { L::Damage, U::Pct, 0.30f, 0.45f },   none, { L::Range, U::PlusPct, 0.0f, 0.40f, 2.0f } },
        { "V_ORBIT",     F::Orbit,   { L::Max, U::Count, 2, 3, 6 },            none, { L::Duration, U::Sec, 4.5f, 6.5f, 12.0f } },
        { "V_CHAIN",     F::Track,   { L::Targets, U::Count, 3, 5, 12 },        none, { L::Damage, U::Pct, 0.80f, 1.20f } },
        { "V_TRAIL",     F::Mobility, { L::PerSecond, U::Pct, 0.40f, 0.60f }, none, { L::Range, U::PlusPct, 0.0f, 0.40f, 1.60f } },
        { "V_HOVER",     F::Orbit,   { L::Damage, U::PlusPct, 0.70f, 1.00f }, none, { L::FireRate, U::PlusPct, -0.10f, 0.0f, 0.30f } },
        { "V_TWIN",      F::Beat,    { L::PerBullet, U::Pct, 0.65f, 0.80f, 1.0f }, none, { L::FireRate, U::PlusPct, 0.0f, 0.10f } },
        { "V_MARK",      F::Track,   { L::MarkTaken, U::PlusPct, 0.25f, 0.40f }, none, { L::Duration, U::Sec, 5.0f, 8.0f, 20.0f } },
        { "V_BURN",      F::Element, { L::PerSecond, U::Pct, 0.20f, 0.30f }, none, { L::Duration, U::Sec, 3.0f, 5.0f, 15.0f } },
        { "V_HUNTER",    F::Track,   { L::Damage, U::PlusPct, 0.0f, 0.30f }, none, { L::Homing, U::PlusPct, 0.0f, 0.35f } },
        { "V_SPRAY",     F::Beat,    { L::SideDmg, U::Pct, 0.35f, 0.50f, 1.0f },  none, { L::SideShots, U::Count, 2, 4, 8 } },
        { "V_GRAVITY",   F::Element, { L::Range, U::Ring, 1.5f, 2.25f, 4.5f },    none, { L::PullDamage, U::PlusPct, 0.0f, 0.40f } },
        { "V_DICE",      F::Blast,   { L::DiceOutcome, U::Plus, 2, 3, 6 },     none, { L::DiceCount, U::Count, 1, 2, 4 } },
        { "V_MINE",      F::Blast,   { L::Damage, U::Pct, 2.20f, 3.30f },   none, { L::Every, U::Count, 3, 2, 1 } },
        { "V_GROW",      F::Pierce,  { L::MaxDamage, U::Mult, 2.5f, 3.0f }, none, { L::MaxDistance, U::Ring, 13.0f, 9.0f, 3.0f } },
        { "V_ABSORB",    F::Element, { L::PerShot, U::PlusPct, 0.05f, 0.08f }, none, { L::Max, U::PlusPct, 0.50f, 0.80f, 2.0f } },
        // ── 트리거 ──
        { "T_NTH",       F::Beat,    { L::Every, U::Count, 6, 4, 2 },          none, { L::ExtraShots, U::Count, 1, 2, 5 } },
        { "T_KILLBURST", F::Track,   { L::Chance, U::Pct, 0.12f, 0.20f, 0.60f },   none, { L::Shots, U::Count, 4, 6, 16 } },
        { "T_CRITECHO",  F::Crit,    { L::Damage, U::Pct, 0.80f, 1.20f },   none, { L::ExtraShots, U::Count, 1, 2, 4 } },
        { "T_REVENGE",   F::Guard,   { L::Damage, U::Pct, 3.00f, 4.50f },   none, { L::TriggerDamage, U::Flat, 20, 12, 4 } },
        { "T_STILL",     F::Mobility, { L::Damage, U::Mult, 3, 4 },         none, { L::StillTime, U::Sec, 1.0f, 0.6f, 0.2f } },
        { "T_GRAZE",     F::Guard,   { L::Shield, U::Flat, 25, 40 },        none, { L::GrazesNeeded, U::Count, 30, 20, 5 } },
        { "T_CAKE",      F::Guard,   { L::Heal, U::Flat, 20, 35 },          none, { L::UsesPerRun, U::Count, 2, 4 } },
        { "T_LASTSTAND", F::Guard,   { L::Damage, U::PlusPct, 0.50f, 0.75f }, none, { L::HpLine, U::Pct, 0.30f, 0.45f, 0.70f } },
        { "T_GRUDGE",    F::Guard,   { L::PerDamage, U::Pct, 0.10f, 0.15f }, none, { L::StoreCap, U::Flat, 60, 100 } },
        { "T_COMBO",     F::Track,   { L::Max, U::PlusPct, 0.40f, 0.60f },  none, { L::PerKill, U::PlusPct, 0.02f, 0.03f } },
        { "T_RHYTHM",    F::Beat,    { L::Damage, U::Mult, 3, 4 },          none, { L::Every, U::Count, 4, 3, 2 } },
        { "T_OPENING",   F::Beat,    { L::Seconds, U::Sec, 3, 5 },          none, { L::ShotsEach, U::Count, 2, 3, 5 } },
        { "T_AFTERIMAGE", F::Mobility, { L::PerSecond, U::Pct, 0.35f, 0.55f }, none, { L::Duration, U::Sec, 2, 3 } },
        { "T_BERSERK",   F::Beat,    { L::MaxFireRate, U::PlusPct, 0.60f, 0.90f }, none, { L::MaxRateHp, U::Pct, 0.0f, 0.30f, 0.70f } },
        { "T_ANCHOR",    F::Mobility, { L::SizePerSec, U::PlusPct, 0.15f, 0.25f }, none, { L::Pierce, U::Plus, 1, 2 } },
        { "T_FIREWORK",  F::Mobility, { L::Shots, U::Count, 12, 18, 36 },       none, { L::PerBullet, U::Pct, 0.60f, 0.90f } },
        // ── 대가 (A 를 고르면 대가도 커지고, B 는 대가를 덜어 줌) ──
        { "C_GLASS",     F::Crit,    { L::Damage, U::Mult, 3.1f, 3.6f }, { L::MaxHp, U::Flat, 30, 25, 10 }, { L::MaxHp, U::Plus, 0, 10 } },
        { "C_HERESY",    F::Beat,    { L::FireRate, U::PlusPct, 0.45f, 0.65f }, { L::MaxHp, U::Plus, -20, -30, -60 }, { L::Damage, U::PlusPct, 0.15f, 0.25f } },
        { "C_REVIVE",    F::Guard,   { L::ReviveHp, U::Flat, 20, 40 },      none, { L::AfterReviveTaken, U::PlusPct, 0.08f, 0.0f, 0.0f } },
        { "C_RECKLESS",  F::Mobility, { L::Damage, U::PlusPct, 0.12f, 0.20f }, none, { L::DashCdCut, U::Sec, 1.5f, 2.5f } },
        { "C_RITUAL",    F::Guard,   { L::PerWave, U::PlusPct, 0.08f, 0.12f }, none, { L::BreakDamage, U::Flat, 10, 20 } },
        { "C_ASCETIC",   F::Crit,    { L::Damage, U::PlusPct, 0.90f, 1.20f }, { L::Taken, U::PlusPct, 0.20f, 0.30f }, { L::Taken, U::PlusPct, 0.0f, -0.10f, -0.30f } },
        // ── 보호 ──
        { "G_CHARGER",   F::Guard,   { L::Shield, U::Flat, 40, 60 },        none, { L::ShieldTaken, U::PlusPct, 0.0f, -0.10f, -0.50f } },
        { "G_STILLGUARD", F::Guard,  { L::Shield, U::Flat, 30, 45 },        none, { L::Recharge, U::Sec, 20, 12, 4 } },
        { "G_PARRY",     F::Mobility, { L::Immune, U::Sec, 0.45f, 0.70f, 1.5f },  none, { L::ClearRange, U::Ring, 3.0f, 4.5f, 8.0f } },
        { "G_CIRCUIT",   F::Guard,   { L::Shield, U::Flat, 32, 48 },        none, { L::KillsNeeded, U::Count, 40, 25, 10 } },
        { "G_FORCEFIELD", F::Guard,  { L::Shield, U::Flat, 15, 30 },        none, { L::Slow, U::Pct, 0.20f, 0.35f, 0.80f } },
        { "G_MIRROR",    F::Guard,   { L::Shield, U::Flat, 90, 135 },       none, { L::Reflect, U::Ring, 3.5f, 5.0f, 8.0f } },
        { "G_LASTWALL",  F::Guard,   { L::Shield, U::Flat, 220, 330 },      none, { L::Saves, U::Count, 1, 2 } },
        // ── 증폭 ──
        { "A_COLLECTOR", F::Pierce,  { L::PerTag, U::PlusPct, 0.09f, 0.13f }, none, { L::MaxKinds, U::Count, 6, 8, 12 } },
        { "A_REACTOR",   F::Track,   { L::PerTag, U::PlusPct, 0.14f, 0.20f }, none, { L::MaxKinds, U::Count, 6, 8, 12 } },
        { "A_GAMBLER",   F::Crit,    { L::PerCard, U::PlusPct, 0.10f, 0.14f }, none, { L::MaxCards, U::Count, 6, 8, 12 } },
    };
    n = (int)(sizeof(k) / sizeof(k[0]));
    return k;
}

inline const UpRow* FindUp(const char* code) {
    int n = 0;
    const UpRow* rows = UpRows(n);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(rows[i].code, code) == 0) return &rows[i];
    return nullptr;
}

// ── Chance cards ──
// A: the chance x1.5 (whole percent) while that stays within 100%; once it would pass it, the
// chance stays and every further pick grows the effect instead (user 2026-11-23: "95% → 142.5%"
// made no sense): pierce / wall bounce +1 → +2 → +3, extra shots 1 → 2 → 3, split 3-way 28% →
// 4-way 35% → 5-way 42% (then +7% damage), blast 30% → 45% → 60%, crit damage +40% each.
inline constexpr float kChanceUpMult = 1.5f;
inline constexpr float kChanceUpSplitDmg = 0.35f;
inline constexpr float kChanceUpBlastPct = 0.45f;
inline constexpr float kChanceUpCritMul = 0.40f;

struct ChanceUp {
    int  pct;           // the chance (%) after branch A
    bool effectGrows;   // the chance stays and the effect grows instead
};
inline ChanceUp ChanceUpgrade(int pct) {
    const int up = (int)std::lround((double)pct * kChanceUpMult);
    if (up <= 100) return { up, false };
    return { pct, true };
}
// After n picks of branch A: the chance (%) and how many picks went into the effect.
struct ChanceAState { int pct; int effect; };
inline ChanceAState ChanceAAt(int pct, int n) {
    ChanceAState st{ pct, 0 };
    for (int k = 0; k < n; ++k) {
        const ChanceUp up = ChanceUpgrade(st.pct);
        if (up.effectGrows) ++st.effect; else st.pct = up.pct;
    }
    return st;
}
// The grown effect, `m` picks into it.
inline int   ChanceSplitWays(int m) { return std::min(5, 3 + m); }
inline float ChanceSplitDmg(int m)  { return 0.28f + 0.07f * (float)m; }
inline float ChanceBlastPct(int m)  { return 0.30f + 0.15f * (float)m; }

// B: the condition eased, the same step again with every pick (one rule per condition;
// SynergyRuntime CondHolds reads these), up to `lim`.
struct ChanceEase { float a, b, lim; };
inline ChanceEase ChanceCondB(ChCond c) {
    switch (c) {
    case ChCond::Still:   return { 0.5f, 0.3f, 0.1f };     // s still
    case ChCond::Moving:  return { 0.0f, 0.5f, 2.0f };     // s it holds after stopping
    case ChCond::LowHp:   return { 0.50f, 0.65f, 0.90f };  // HP share at or below
    case ChCond::FullHp:  return { 1.00f, 0.90f, 0.50f };  // HP share at or above
    case ChCond::Shield:  return { 0.0f, 2.0f, 8.0f };     // s it holds after the shield breaks
    case ChCond::Dash:    return { 1.0f, 1.5f, 3.0f };     // x the window after a dash
    case ChCond::Kill:    return { 3.0f, 5.0f, 13.0f };    // s after a kill
    case ChCond::Wave:    return { 5.0f, 8.0f, 20.0f };    // s from the wave start
    case ChCond::Crowd:   return { 8.0f, 6.0f, 2.0f };     // enemies within 4 rings
    case ChCond::Few:     return { 0.90f, 0.75f, 0.30f };  // quota share (enemies left 3 → 5 → 7 …)
    case ChCond::Boss:    return { 0.0f, 1.0f, 2.0f };     // 1: all through elite waves, 2: always
    case ChCond::Far:     return { 4.5f, 3.5f, 1.5f };     // rings to the nearest enemy
    case ChCond::Barrage: return { 3.0f, 2.0f, 1.0f };     // enemy shots within 2.5 rings
    case ChCond::Clean:   return { 0.0f, 10.0f, 50.0f };   // damage taken this wave below
    case ChCond::Hurt:    return { 3.0f, 5.0f, 13.0f };    // s after a hit
    case ChCond::Combo:   return { 5.0f, 3.0f, 1.0f };     // combo at least
    case ChCond::Long:    return { 20.0f, 12.0f, 0.0f };   // s into the wave
    default: break;
    }
    return { 0.0f, 0.0f, 0.0f };
}
inline float ChanceCondAt(ChCond c, int n) {
    const ChanceEase e = ChanceCondB(c);
    float x = e.a + (e.b - e.a) * (float)std::max(0, n);
    return e.b >= e.a ? std::min(x, e.lim) : std::max(x, e.lim);
}
inline bool ChanceCondCanGrow(ChCond c, int n) { return std::fabs(ChanceCondAt(c, n + 1) - ChanceCondAt(c, n)) > 1e-5f; }

// ── Text ──
inline std::wstring UpNumber(UpUnit u, float v, int lang) {
    wchar_t buf[32];
    const bool whole = std::fabs(v - std::round(v)) < 0.01f;
    switch (u) {
    case UpUnit::Pct:
    case UpUnit::PlusPct: {
        const float p = v * 100.0f;
        const bool wp = std::fabs(p - std::round(p)) < 0.05f;
        if (u == UpUnit::PlusPct && std::fabs(p) >= 0.05f) swprintf(buf, 32, wp ? L"%+.0f%%" : L"%+.1f%%", p);
        else swprintf(buf, 32, wp ? L"%.0f%%" : L"%.1f%%", p);
        break;
    }
    case UpUnit::Mult: swprintf(buf, 32, whole ? L"×%.0f" : L"×%.1f", v); break;
    case UpUnit::Plus: swprintf(buf, 32, L"%+.0f", v); break;
    case UpUnit::Sec:
        swprintf(buf, 32, L"%g", v);
        return std::wstring(buf) + (lang == 1 ? L"s" : lang == 2 ? L"秒" : L"초");
    case UpUnit::Ring: swprintf(buf, 32, L"%g○", v); break;
    default: swprintf(buf, 32, L"%.0f", v); break;
    }
    return buf;
}

inline const wchar_t* UpLabelText(UpLabel l, int lang) {
    static const wchar_t* const k[(int)UpLabel::Count_][3] = {
        { L"",                L"",                  L"" },
        { L"튕김",            L"Bounces",           L"跳弾" },
        { L"관통",            L"Pierce",            L"貫通" },
        { L"갈래",            L"Split ways",        L"分裂数" },
        { L"분열 피해",       L"Split damage",      L"分裂ダメージ" },
        { L"유도 세기",       L"Homing",            L"誘導の強さ" },
        { L"피해",            L"Damage",            L"ダメージ" },
        { L"최대",            L"Max",               L"最大" },
        { L"대상",            L"Targets",           L"対象" },
        { L"적탄당",          L"Per enemy shot",    L"敵弾ごと" },
        { L"주기 (발)",       L"Every (shots)",     L"周期 (発)" },
        { L"확률",            L"Chance",            L"確率" },
        { L"탄 수",           L"Shots",             L"弾数" },
        { L"보호막",          L"Shield",            L"シールド" },
        { L"회복",            L"Heal",              L"回復" },
        { L"시간",            L"Duration",          L"時間" },
        { L"범위",            L"Area",              L"範囲" },
        { L"피해 1당",        L"Per damage",        L"ダメージ1ごと" },
        { L"처치당",          L"Per kill",          L"撃破ごと" },
        { L"웨이브당",        L"Per wave",          L"ウェーブごと" },
        { L"효과 1종당",      L"Per kind",          L"1種ごと" },
        { L"1장당",           L"Per card",          L"1枚ごと" },
        { L"연사",            L"Fire rate",         L"連射" },
        { L"최대 체력",       L"Max HP",            L"最大体力" },
        { L"받는 피해",       L"Damage taken",      L"被ダメージ" },
        { L"초당",            L"Per second",        L"毎秒" },
        { L"튕길 때마다 피해", L"Damage per bounce", L"跳ねるたびダメージ" },
        { L"반사 후 피해",    L"Damage after a ricochet", L"反射後のダメージ" },
        { L"유도탄 피해",     L"Homing shot damage", L"誘導弾ダメージ" },
        { L"지속",            L"Lasts",             L"持続" },
        { L"탄당 피해",       L"Damage per shot",   L"1発ごとのダメージ" },
        { L"표식 적이 받는 피해", L"Marked enemies take", L"刻印の敵の被ダメージ" },
        { L"옆 탄 피해",      L"Side shot damage",  L"横弾ダメージ" },
        { L"옆 탄",           L"Side shots",        L"横弾" },
        { L"관통·튕김 결과",  L"Pierce / bounce outcome", L"貫通・跳弾の結果" },
        { L"한 번에 얻는 결과", L"Outcomes per shot", L"一度に得る結果" },
        { L"최대 피해",       L"Max damage",        L"最大ダメージ" },
        { L"최대까지 거리",   L"Distance to max",   L"最大までの距離" },
        { L"추가 탄",         L"Extra shots",       L"追加弾" },
        { L"발동 기준 피해",  L"Damage to trigger", L"発動に必要な被ダメージ" },
        { L"정지 시간",       L"Stillness needed",  L"静止時間" },
        { L"필요 스침",       L"Grazes needed",     L"必要なかすり" },
        { L"런당 횟수",       L"Uses per run",      L"ランごとの回数" },
        { L"체력 기준",       L"HP line",           L"体力の基準" },
        { L"저장 상한",       L"Storage cap",       L"蓄積上限" },
        { L"한 번에 나가는 탄", L"Shots per fire",  L"一度に出る弾" },
        { L"최대 연사",       L"Max fire rate",     L"最大連射" },
        { L"최대 연사 도달 체력", L"Max rate at HP", L"最大連射に届く体力" },
        { L"멈춘 1초당 크기", L"Size per second still", L"静止1秒ごとの大きさ" },
        { L"부활 체력",       L"Revive HP",         L"復活体力" },
        { L"부활 뒤 받는 피해", L"Damage taken after reviving", L"復活後の被ダメージ" },
        { L"대시 재사용 감소", L"Dash cooldown cut", L"ダッシュ再使用短縮" },
        { L"소멸 기준 피해",  L"Damage that breaks it", L"消滅する被ダメージ" },
        { L"보호막 동안 받는 피해", L"Damage taken while shielded", L"シールド中の被ダメージ" },
        { L"재충전",          L"Recharge",          L"再充電" },
        { L"피격 면제",       L"Hit immunity",      L"被弾免除" },
        { L"소거 범위",       L"Clear radius",      L"消去範囲" },
        { L"필요 처치",       L"Kills needed",      L"必要な撃破" },
        { L"적탄 감속",       L"Enemy shot slow",   L"敵弾減速" },
        { L"반사 범위",       L"Reflect radius",    L"反射範囲" },
        { L"죽음 막기",       L"Deaths blocked",    L"死を防ぐ回数" },
        { L"최대 종류",       L"Max kinds",         L"最大種類" },
        { L"최대 장수",       L"Max cards",         L"最大枚数" },
        { L"끌어당길 때 피해", L"Pull damage",      L"引き寄せダメージ" },
    };
    const int i = std::max(0, std::min((int)UpLabel::Count_ - 1, (int)l));
    return k[i][std::max(0, std::min(2, lang))];
}

// "튕김 2 → 3"; a value that starts at 0 reads as the new value alone ("유도 세기 +60%").
inline std::wstring UpPart(const UpVal& v, int lang) {
    std::wstring s = UpLabelText(v.l, lang);
    if ((v.u == UpUnit::PlusPct || v.u == UpUnit::Plus) && std::fabs(v.a) < 1e-6f)
        return s + L" " + UpNumber(v.u, v.b, lang);
    return s + L" " + UpNumber(v.u, v.a, lang) + L" → " + UpNumber(v.u, v.b, lang);
}

inline const wchar_t* ShapeText(Family f, int lang) {
    static const wchar_t* const k[(int)Family::Count][3] = {
        { L"", L"", L"" },
        { L"관통할 때마다 피해 +15% (누적)", L"+15% damage per enemy pierced (stacks)", L"貫通するたびダメージ+15% (累積)" },
        { L"튕길 때 작은 폭발 (탄 피해의 30%)", L"A small blast on every bounce (30% of the shot)", L"跳ねるたび小爆発 (弾ダメージの30%)" },
        { L"분열탄이 가까운 적 쪽으로 휨", L"Split shots curve toward the nearest enemy", L"分裂弾が近い敵へ曲がる" },
        { L"폭발 0.5초 뒤 같은 자리에 작은 2차 폭발 (피해 50%)", L"Blasts go off again 0.5s later (50% damage)", L"爆発0.5秒後に同じ場所で小さな2次爆発 (50%)" },
        { L"처치 시 근처 적 2명에게 번개 (공격력 50%)", L"Kills send lightning to 2 nearby enemies (50% attack)", L"撃破時に近くの敵2体へ雷 (攻撃力50%)" },
        { L"공전체·멈춘 탄이 닿은 적탄을 지움", L"Orbiting and hovering shots erase enemy shots", L"周回弾・停止弾が触れた敵弾を消す" },
        { L"처치 시 주변 1.5○ 적이 3초간 불탐", L"Kills set enemies within 1.5○ alight for 3s", L"撃破時に周囲1.5○の敵が3秒燃える" },
        { L"대시가 끝나면 그 자리에 폭발 (공격력 150%)", L"A blast where each dash ends (150% attack)", L"ダッシュ終了地点で爆発 (攻撃力150%)" },
        { L"보호막이 깨질 때 주변 3.5○ 적탄 소거", L"A breaking shield erases enemy shots within 3.5○", L"シールドが割れると周囲3.5○の敵弾を消去" },
        { L"강화탄·추가탄 피해 +50%", L"Boosted and extra shots deal +50%", L"強化弾・追加弾のダメージ+50%" },
        { L"치명타 시 주변 작은 폭발 (탄 피해의 40%)", L"Crits set off a small blast (40% of the shot)", L"会心時に小爆発 (弾ダメージの40%)" },
    };
    const int i = std::max(0, std::min((int)Family::Count - 1, (int)f));
    return k[i][std::max(0, std::min(2, lang))];
}

inline const wchar_t* FamilyName(Family f, int lang) {
    static const wchar_t* const k[(int)Family::Count][3] = {
        { L"", L"", L"" },
        { L"관통", L"Pierce", L"貫通" }, { L"튕김", L"Bounce", L"跳弾" }, { L"분열", L"Split", L"分裂" },
        { L"폭발", L"Blast", L"爆発" }, { L"추적", L"Track", L"追跡" }, { L"궤도", L"Orbit", L"軌道" },
        { L"원소", L"Element", L"元素" }, { L"기동", L"Motion", L"機動" }, { L"방벽", L"Guard", L"防壁" },
        { L"박자", L"Beat", L"拍子" }, { L"치명", L"Crit", L"会心" },
    };
    const int i = std::max(0, std::min((int)Family::Count - 1, (int)f));
    return k[i][std::max(0, std::min(2, lang))];
}

// "관통 +1 → +2" for the grown effect, `m` picks in → m + 1.
inline std::wstring ChanceEffectText(ChKind k, int m, int lang) {
    wchar_t buf[96];
    lang = std::max(0, std::min(2, lang));
    switch (k) {
    case ChKind::Crit: {
        static const wchar_t* const f[3] = { L"조건 동안 치명타 피해 +%d%% → +%d%%", L"Crit damage +%d%% → +%d%% while it holds", L"条件中 会心ダメージ +%d%% → +%d%%" };
        static const wchar_t* const f0[3] = { L"조건 동안 치명타 피해 +%d%%", L"Crit damage +%d%% while it holds", L"条件中 会心ダメージ +%d%%" };
        if (m == 0) swprintf(buf, 96, f0[lang], 40); else swprintf(buf, 96, f[lang], 40 * m, 40 * (m + 1));
        break;
    }
    case ChKind::Extra: {
        static const wchar_t* const f[3] = { L"추가 탄 %d발 → %d발", L"Extra shots %d → %d", L"追加弾 %d発 → %d発" };
        swprintf(buf, 96, f[lang], 1 + m, 2 + m); break;
    }
    case ChKind::Pierce: {
        static const wchar_t* const f[3] = { L"관통 +%d → +%d", L"Pierce +%d → +%d", L"貫通 +%d → +%d" };
        swprintf(buf, 96, f[lang], 1 + m, 2 + m); break;
    }
    case ChKind::Bounce: {
        static const wchar_t* const f[3] = { L"벽 튕김 +%d → +%d", L"Wall bounce +%d → +%d", L"壁跳弾 +%d → +%d" };
        swprintf(buf, 96, f[lang], 1 + m, 2 + m); break;
    }
    case ChKind::Split: {
        static const wchar_t* const f[3] = { L"분열 %d갈래 %.0f%% → %d갈래 %.0f%%", L"Split %d-way %.0f%% → %d-way %.0f%%", L"分裂 %d方向%.0f%% → %d方向%.0f%%" };
        swprintf(buf, 96, f[lang], ChanceSplitWays(m), ChanceSplitDmg(m) * 100.0f, ChanceSplitWays(m + 1), ChanceSplitDmg(m + 1) * 100.0f); break;
    }
    case ChKind::Blast: {
        static const wchar_t* const f[3] = { L"폭발 피해 %.0f%% → %.0f%%", L"Blast damage %.0f%% → %.0f%%", L"爆発ダメージ %.0f%% → %.0f%%" };
        swprintf(buf, 96, f[lang], ChanceBlastPct(m) * 100.0f, ChanceBlastPct(m + 1) * 100.0f); break;
    }
    }
    return buf;
}

// "멈춰 있는 시간 0.5초 → 0.3초" — the condition after n picks of B → n + 1.
inline std::wstring ChanceBText(ChCond c, int n, int lang) {
    lang = std::max(0, std::min(2, lang));
    const float x = ChanceCondAt(c, n), y = ChanceCondAt(c, n + 1);
    wchar_t buf[128];
    auto F = [&](const wchar_t* kr, const wchar_t* en, const wchar_t* jp, double a, double b) {
        swprintf(buf, 128, lang == 0 ? kr : lang == 1 ? en : jp, a, b);
        return std::wstring(buf);
    };
    switch (c) {
    case ChCond::Still:   return F(L"멈춰 있는 시간 %g초 → %g초", L"Stillness needed %gs → %gs", L"静止時間 %g秒 → %g秒", x, y);
    case ChCond::Moving:  return F(L"멈춘 뒤 유지 %g초 → %g초", L"Holds %gs → %gs after you stop", L"止まった後の維持 %g秒 → %g秒", x, y);
    case ChCond::LowHp:   return F(L"체력 기준 %.0f%% → %.0f%% 이하", L"HP line %.0f%% → %.0f%% or less", L"体力の基準 %.0f%% → %.0f%%以下", x * 100, y * 100);
    case ChCond::FullHp:  return F(L"체력 기준 %.0f%% → %.0f%% 이상", L"HP line %.0f%% → %.0f%% or more", L"体力の基準 %.0f%% → %.0f%%以上", x * 100, y * 100);
    case ChCond::Shield:  return F(L"보호막이 깨진 뒤 유지 %g초 → %g초", L"Holds %gs → %gs after the shield breaks", L"シールドが割れた後の維持 %g秒 → %g秒", x, y);
    case ChCond::Dash:    // as a share longer than the card's own window
        if (x <= 1.0001f) {
            static const wchar_t* const one[3] = { L"대시 뒤 유지 시간 +%.0f%%", L"Window after a dash +%.0f%%", L"ダッシュ後の維持時間 +%.0f%%" };
            swprintf(buf, 128, one[lang], (double)((y - 1) * 100));
            return std::wstring(buf);
        }
        return F(L"대시 뒤 유지 시간 +%.0f%% → +%.0f%%", L"Window after a dash +%.0f%% → +%.0f%%", L"ダッシュ後の維持時間 +%.0f%% → +%.0f%%", (x - 1) * 100, (y - 1) * 100);
    case ChCond::Kill:    return F(L"처치 뒤 유지 %g초 → %g초", L"Holds %gs → %gs after a kill", L"撃破後の維持 %g秒 → %g秒", x, y);
    case ChCond::Wave:    return F(L"웨이브 시작 뒤 %g초 → %g초", L"%gs → %gs from the wave start", L"ウェーブ開始後 %g秒 → %g秒", x, y);
    case ChCond::Crowd:   return F(L"주변 적 %.0f명 → %.0f명 이상", L"Enemies nearby %.0f → %.0f or more", L"周囲の敵 %.0f体 → %.0f体以上", x, y);
    case ChCond::Few:     return F(L"할당량 %.0f%% → %.0f%%", L"Quota %.0f%% → %.0f%%", L"ノルマ %.0f%% → %.0f%%", x * 100, y * 100);
    case ChCond::Boss: {
        static const wchar_t* const t[2][3] = {
            { L"엘리트 웨이브에서는 내내", L"All through elite waves", L"エリートウェーブでは常に" },
            { L"보스·엘리트가 없어도 항상", L"Always, boss or not", L"ボス・エリートがいなくても常に" } };
        return t[y >= 1.5f ? 1 : 0][lang];
    }
    case ChCond::Far:     return F(L"가장 가까운 적 %g○ → %g○ 이상", L"Nearest enemy %g○ → %g○ or farther", L"最も近い敵 %g○ → %g○以上", x, y);
    case ChCond::Barrage: return F(L"주변 적탄 %.0f발 → %.0f발 이상", L"Enemy shots nearby %.0f → %.0f or more", L"周囲の敵弾 %.0f発 → %.0f発以上", x, y);
    case ChCond::Clean:   return F(L"받은 피해 %.0f → %.0f 미만", L"Damage taken %.0f → under %.0f", L"被ダメージ %.0f → %.0f未満", x, y);
    case ChCond::Hurt:    return F(L"피해 뒤 유지 %g초 → %g초", L"Holds %gs → %gs after a hit", L"被弾後の維持 %g秒 → %g秒", x, y);
    case ChCond::Combo:   return F(L"콤보 %.0f → %.0f 이상", L"Combo %.0f → %.0f or more", L"コンボ %.0f → %.0f以上", x, y);
    case ChCond::Long:    return F(L"웨이브 시작 %g초 → %g초 뒤부터", L"From %gs → %gs into the wave", L"ウェーブ開始 %g秒 → %g秒後から", x, y);
    default: break;
    }
    return L"";
}

inline std::wstring ShapeLine(Family f, int lv, int lang);

// "label now → next" for a number picked n times.
inline std::wstring UpStep(const UpVal& v, int n, int lang) {
    return UpPart(UpVal{ v.l, v.u, UpAt(v, n), UpAt(v, n + 1) }, lang);
}

// One branch's line on the card: what the next pick does, `n` picks already taken of it.
// Branch 0 A, 1 B, 2 C. `chance`: a chance card (kind, cond, pct). For C, `n` is the family's
// shape level (picks of that family's C across all cards).
inline std::wstring BranchLine(const UpRow* row, bool chance, ChKind kind, ChCond cond, int pct,
                               int branch, int lang, int n = 0) {
    if (chance) {
        if (branch == 0) {
            const ChanceAState now = ChanceAAt(pct, n), next = ChanceAAt(pct, n + 1);
            if (next.effect > now.effect) return ChanceEffectText(kind, now.effect, lang);
            return UpPart(UpVal{ UpLabel::Chance, UpUnit::Pct, (float)now.pct / 100.0f, (float)next.pct / 100.0f }, lang);
        }
        if (branch == 1) return ChanceBText(cond, n, lang);
        return ShapeLine(ChanceFamily(kind), n, lang);
    }
    if (!row) return L"";
    if (branch == 0) {
        std::wstring s = UpStep(row->A, n, lang);
        if (row->A2.on() && UpCanGrow(row->A2, n)) s += L" · " + UpStep(row->A2, n, lang);
        return s;
    }
    if (branch == 1) return row->B.on() ? UpStep(row->B, n, lang) : std::wstring();
    return ShapeLine(row->fam, n, lang);
}

// The shape's line: its text, and " · 겹침 1 → 2" once it is already on (level `lv`).
inline std::wstring ShapeLine(Family f, int lv, int lang) {
    std::wstring s = ShapeText(f, lang);
    if (lv > 0) {
        static const wchar_t* const k[3] = { L" · 겹침 %d → %d", L" · stack %d → %d", L" · 重ね %d → %d" };
        wchar_t buf[48];
        swprintf(buf, 48, k[std::max(0, std::min(2, lang))], lv, lv + 1);
        s += buf;
    }
    return s;
}

inline const wchar_t* BranchName(int branch, int lang) {
    static const wchar_t* const k[3][3] = { { L"수치 A", L"Number A", L"数値A" },
                                            { L"수치 B", L"Number B", L"数値B" },
                                            { L"모양", L"Shape", L"形" } };
    return k[std::max(0, std::min(2, branch))][std::max(0, std::min(2, lang))];
}

}  // namespace synergy
