#pragma once
// Card upgrades (augment rework, 2026-11-23; Docs/Plans/2026-11-23_강화화면_점검.md).
// Every upgradable card has three branches, each taken once (a card goes up to three times):
//   A  its key number            (UpRow::A, a cost card's cost grows with it: UpRow::A2)
//   B  a second number           (UpRow::B; chance cards: their condition is eased, ChanceCondB)
//   C  its family's shape (Family). Every card keeps all three branches (user 2026-11-23); a
//      second card's C on a family that is already on stacks it (effect x2, x3 ...).
// A wave clear shows three (card, branch) offers. This table is the only place the numbers
// live: SynergyRuntime.cpp reads them (UPA / UPA2 / UPB) and the card text is built from it.
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

struct UpVal {
    UpLabel l = UpLabel::None; UpUnit u = UpUnit::Count; float a = 0.0f, b = 0.0f;   // base → upgraded
    bool on() const { return l != UpLabel::None; }
};
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
        { "V_RICOCHET",  F::Bounce,  { L::Bounces, U::Count, 1, 2 },        none, { L::AfterRedirect, U::PlusPct, 0.0f, 0.20f } },
        { "V_PIERCE",    F::Pierce,  { L::Pierce, U::Count, 2, 3 },         none, { L::Damage, U::PlusPct, 0.0f, 0.10f } },
        { "V_SPLIT",     F::Split,   { L::Split, U::Count, 3, 4 },          none, { L::SplitDmg, U::Pct, 0.28f, 0.35f } },
        { "V_HOMING",    F::Track,   { L::Homing, U::PlusPct, 0.0f, 0.60f }, none, { L::HomingDmg, U::PlusPct, 0.0f, 0.15f } },
        { "V_BLAST",     F::Blast,   { L::Damage, U::Pct, 0.30f, 0.45f },   none, { L::Range, U::PlusPct, 0.0f, 0.40f } },
        { "V_ORBIT",     F::Orbit,   { L::Max, U::Count, 2, 3 },            none, { L::Duration, U::Sec, 4.5f, 6.5f } },
        { "V_CHAIN",     F::Track,   { L::Targets, U::Count, 3, 5 },        none, { L::Damage, U::Pct, 0.80f, 1.20f } },
        { "V_TRAIL",     F::Mobility, { L::PerSecond, U::Pct, 0.40f, 0.60f }, none, { L::Range, U::PlusPct, 0.0f, 0.40f } },
        { "V_HOVER",     F::Orbit,   { L::Damage, U::PlusPct, 0.70f, 1.00f }, none, { L::FireRate, U::PlusPct, -0.10f, 0.0f } },
        { "V_TWIN",      F::Beat,    { L::PerBullet, U::Pct, 0.65f, 0.80f }, none, { L::FireRate, U::PlusPct, 0.0f, 0.10f } },
        { "V_MARK",      F::Track,   { L::MarkTaken, U::PlusPct, 0.25f, 0.40f }, none, { L::Duration, U::Sec, 5.0f, 8.0f } },
        { "V_BURN",      F::Element, { L::PerSecond, U::Pct, 0.20f, 0.30f }, none, { L::Duration, U::Sec, 3.0f, 5.0f } },
        { "V_HUNTER",    F::Track,   { L::Damage, U::PlusPct, 0.0f, 0.30f }, none, { L::Homing, U::PlusPct, 0.0f, 0.35f } },
        { "V_SPRAY",     F::Beat,    { L::SideDmg, U::Pct, 0.35f, 0.50f },  none, { L::SideShots, U::Count, 2, 4 } },
        { "V_GRAVITY",   F::Element, { L::Range, U::Ring, 1.5f, 2.25f },    none, { L::PullDamage, U::PlusPct, 0.0f, 0.40f } },
        { "V_DICE",      F::Blast,   { L::DiceOutcome, U::Plus, 2, 3 },     none, { L::DiceCount, U::Count, 1, 2 } },
        { "V_MINE",      F::Blast,   { L::Damage, U::Pct, 2.20f, 3.30f },   none, { L::Every, U::Count, 3, 2 } },
        { "V_GROW",      F::Pierce,  { L::MaxDamage, U::Mult, 2.5f, 3.0f }, none, { L::MaxDistance, U::Ring, 13.0f, 9.0f } },
        { "V_ABSORB",    F::Element, { L::PerShot, U::PlusPct, 0.05f, 0.08f }, none, { L::Max, U::PlusPct, 0.50f, 0.80f } },
        // ── 트리거 ──
        { "T_NTH",       F::Beat,    { L::Every, U::Count, 6, 4 },          none, { L::ExtraShots, U::Count, 1, 2 } },
        { "T_KILLBURST", F::Track,   { L::Chance, U::Pct, 0.12f, 0.20f },   none, { L::Shots, U::Count, 4, 6 } },
        { "T_CRITECHO",  F::Crit,    { L::Damage, U::Pct, 0.80f, 1.20f },   none, { L::ExtraShots, U::Count, 1, 2 } },
        { "T_REVENGE",   F::Guard,   { L::Damage, U::Pct, 3.00f, 4.50f },   none, { L::TriggerDamage, U::Flat, 20, 12 } },
        { "T_STILL",     F::Mobility, { L::Damage, U::Mult, 3, 4 },         none, { L::StillTime, U::Sec, 1.0f, 0.6f } },
        { "T_GRAZE",     F::Guard,   { L::Shield, U::Flat, 25, 40 },        none, { L::GrazesNeeded, U::Count, 30, 20 } },
        { "T_CAKE",      F::Guard,   { L::Heal, U::Flat, 20, 35 },          none, { L::UsesPerRun, U::Count, 2, 4 } },
        { "T_LASTSTAND", F::Guard,   { L::Damage, U::PlusPct, 0.50f, 0.75f }, none, { L::HpLine, U::Pct, 0.30f, 0.45f } },
        { "T_GRUDGE",    F::Guard,   { L::PerDamage, U::Pct, 0.10f, 0.15f }, none, { L::StoreCap, U::Flat, 60, 100 } },
        { "T_COMBO",     F::Track,   { L::Max, U::PlusPct, 0.40f, 0.60f },  none, { L::PerKill, U::PlusPct, 0.02f, 0.03f } },
        { "T_RHYTHM",    F::Beat,    { L::Damage, U::Mult, 3, 4 },          none, { L::Every, U::Count, 4, 3 } },
        { "T_OPENING",   F::Beat,    { L::Seconds, U::Sec, 3, 5 },          none, { L::ShotsEach, U::Count, 2, 3 } },
        { "T_AFTERIMAGE", F::Mobility, { L::PerSecond, U::Pct, 0.35f, 0.55f }, none, { L::Duration, U::Sec, 2, 3 } },
        { "T_BERSERK",   F::Beat,    { L::MaxFireRate, U::PlusPct, 0.60f, 0.90f }, none, { L::MaxRateHp, U::Pct, 0.0f, 0.30f } },
        { "T_ANCHOR",    F::Mobility, { L::SizePerSec, U::PlusPct, 0.15f, 0.25f }, none, { L::Pierce, U::Plus, 1, 2 } },
        { "T_FIREWORK",  F::Mobility, { L::Shots, U::Count, 12, 18 },       none, { L::PerBullet, U::Pct, 0.60f, 0.90f } },
        // ── 대가 (A 를 고르면 대가도 커지고, B 는 대가를 덜어 줌) ──
        { "C_GLASS",     F::Crit,    { L::Damage, U::Mult, 3.1f, 3.6f }, { L::MaxHp, U::Flat, 30, 25 }, { L::MaxHp, U::Plus, 0, 10 } },
        { "C_HERESY",    F::Beat,    { L::FireRate, U::PlusPct, 0.45f, 0.65f }, { L::MaxHp, U::Plus, -20, -30 }, { L::Damage, U::PlusPct, 0.15f, 0.25f } },
        { "C_REVIVE",    F::Guard,   { L::ReviveHp, U::Flat, 20, 40 },      none, { L::AfterReviveTaken, U::PlusPct, 0.08f, 0.0f } },
        { "C_RECKLESS",  F::Mobility, { L::Damage, U::PlusPct, 0.12f, 0.20f }, none, { L::DashCdCut, U::Sec, 1.5f, 2.5f } },
        { "C_RITUAL",    F::Guard,   { L::PerWave, U::PlusPct, 0.08f, 0.12f }, none, { L::BreakDamage, U::Flat, 10, 20 } },
        { "C_ASCETIC",   F::Crit,    { L::Damage, U::PlusPct, 0.90f, 1.20f }, { L::Taken, U::PlusPct, 0.20f, 0.30f }, { L::Taken, U::PlusPct, 0.0f, -0.10f } },
        // ── 보호 ──
        { "G_CHARGER",   F::Guard,   { L::Shield, U::Flat, 40, 60 },        none, { L::ShieldTaken, U::PlusPct, 0.0f, -0.10f } },
        { "G_STILLGUARD", F::Guard,  { L::Shield, U::Flat, 30, 45 },        none, { L::Recharge, U::Sec, 20, 12 } },
        { "G_PARRY",     F::Mobility, { L::Immune, U::Sec, 0.45f, 0.70f },  none, { L::ClearRange, U::Ring, 3.0f, 4.5f } },
        { "G_CIRCUIT",   F::Guard,   { L::Shield, U::Flat, 32, 48 },        none, { L::KillsNeeded, U::Count, 40, 25 } },
        { "G_FORCEFIELD", F::Guard,  { L::Shield, U::Flat, 15, 30 },        none, { L::Slow, U::Pct, 0.20f, 0.35f } },
        { "G_MIRROR",    F::Guard,   { L::Shield, U::Flat, 90, 135 },       none, { L::Reflect, U::Ring, 3.5f, 5.0f } },
        { "G_LASTWALL",  F::Guard,   { L::Shield, U::Flat, 220, 330 },      none, { L::Saves, U::Count, 1, 2 } },
        // ── 증폭 ──
        { "A_COLLECTOR", F::Pierce,  { L::PerTag, U::PlusPct, 0.09f, 0.13f }, none, { L::MaxKinds, U::Count, 6, 8 } },
        { "A_REACTOR",   F::Track,   { L::PerTag, U::PlusPct, 0.14f, 0.20f }, none, { L::MaxKinds, U::Count, 6, 8 } },
        { "A_GAMBLER",   F::Crit,    { L::PerCard, U::PlusPct, 0.10f, 0.14f }, none, { L::MaxCards, U::Count, 6, 8 } },
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
// A: the chance x1.5 (whole percent) while that stays within 100%; a card whose chance would
// pass it keeps its chance and grows its effect instead (user 2026-11-23: "95% → 142.5%" made
// no sense): pierce / wall bounce +1 → +2, one extra shot → two, 3-way 28% split → 4-way 35%,
// blast 30% → 45%, +40% crit damage while the condition holds.
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

// B: the condition eased (one rule per condition; SynergyRuntime CondHolds reads these).
struct ChanceEase { float a, b; };
inline ChanceEase ChanceCondB(ChCond c) {
    switch (c) {
    case ChCond::Still:   return { 0.5f, 0.3f };    // s still
    case ChCond::Moving:  return { 0.0f, 0.5f };    // s it holds after stopping
    case ChCond::LowHp:   return { 0.50f, 0.65f };  // HP share at or below
    case ChCond::FullHp:  return { 1.00f, 0.90f };  // HP share at or above
    case ChCond::Shield:  return { 0.0f, 2.0f };    // s it holds after the shield breaks
    case ChCond::Dash:    return { 1.0f, 1.5f };    // x the window after a dash
    case ChCond::Kill:    return { 3.0f, 5.0f };    // s after a kill
    case ChCond::Wave:    return { 5.0f, 8.0f };    // s from the wave start
    case ChCond::Crowd:   return { 8.0f, 6.0f };    // enemies within 4 rings
    case ChCond::Few:     return { 0.90f, 0.75f };  // quota share (or enemies left 3 → 5)
    case ChCond::Boss:    return { 0.0f, 1.0f };    // 1: the whole elite wave too
    case ChCond::Far:     return { 4.5f, 3.5f };    // rings to the nearest enemy
    case ChCond::Barrage: return { 3.0f, 2.0f };    // enemy shots within 2.5 rings
    case ChCond::Clean:   return { 0.0f, 10.0f };   // damage taken this wave below
    case ChCond::Hurt:    return { 3.0f, 5.0f };    // s after a hit
    case ChCond::Combo:   return { 5.0f, 3.0f };    // combo at least
    case ChCond::Long:    return { 20.0f, 12.0f };  // s into the wave
    default: break;
    }
    return { 0.0f, 0.0f };
}

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

inline const wchar_t* ChanceAEffectText(ChKind k, int lang) {
    static const wchar_t* const t[6][3] = {
        { L"조건 동안 치명타 피해 +40%", L"Crit damage +40% while it holds", L"条件中 会心ダメージ+40%" },
        { L"추가 탄 1발 → 2발", L"Extra shots 1 → 2", L"追加弾 1発 → 2発" },
        { L"관통 +1 → +2", L"Pierce +1 → +2", L"貫通 +1 → +2" },
        { L"벽 튕김 +1 → +2", L"Wall bounce +1 → +2", L"壁跳弾 +1 → +2" },
        { L"분열 3갈래 28% → 4갈래 35%", L"Split 3-way 28% → 4-way 35%", L"分裂 3方向28% → 4方向35%" },
        { L"폭발 피해 30% → 45%", L"Blast damage 30% → 45%", L"爆発ダメージ 30% → 45%" } };
    return t[(int)k][std::max(0, std::min(2, lang))];
}

inline const wchar_t* ChanceBText(ChCond c, int lang) {
    static const wchar_t* const t[(int)ChCond::Count][3] = {
        { L"멈춰 있는 시간 0.5초 → 0.3초", L"Stillness needed 0.5s → 0.3s", L"静止時間 0.5秒 → 0.3秒" },
        { L"멈춘 뒤에도 0.5초 유지", L"Holds 0.5s after you stop", L"止まった後も0.5秒維持" },
        { L"체력 기준 50% → 65% 이하", L"HP line 50% → 65% or less", L"体力の基準 50% → 65%以下" },
        { L"체력 기준 100% → 90% 이상", L"HP line 100% → 90% or more", L"体力の基準 100% → 90%以上" },
        { L"보호막이 깨진 뒤에도 2초 유지", L"Holds 2s after the shield breaks", L"シールドが割れた後も2秒維持" },
        { L"대시 뒤 유지 시간 ×1.5", L"Window after a dash x1.5", L"ダッシュ後の維持時間 ×1.5" },
        { L"처치 뒤 유지 3초 → 5초", L"Holds 3s → 5s after a kill", L"撃破後の維持 3秒 → 5秒" },
        { L"웨이브 시작 뒤 5초 → 8초", L"5s → 8s from the wave start", L"ウェーブ開始後 5秒 → 8秒" },
        { L"주변 적 8명 → 6명 이상", L"Enemies nearby 8 → 6 or more", L"周囲の敵 8体 → 6体以上" },
        { L"할당량 90% → 75% (적 3명 → 5명 이하)", L"Quota 90% → 75% (enemies left 3 → 5)", L"ノルマ 90% → 75% (敵 3体 → 5体以下)" },
        { L"엘리트 웨이브에서는 내내", L"All through elite waves", L"エリートウェーブでは常に" },
        { L"가장 가까운 적 4.5○ → 3.5○ 이상", L"Nearest enemy 4.5○ → 3.5○ or farther", L"最も近い敵 4.5○ → 3.5○以上" },
        { L"주변 적탄 3발 → 2발 이상", L"Enemy shots nearby 3 → 2 or more", L"周囲の敵弾 3発 → 2発以上" },
        { L"받은 피해 0 → 10 미만", L"Damage taken 0 → under 10", L"被ダメージ 0 → 10未満" },
        { L"피해 뒤 유지 3초 → 5초", L"Holds 3s → 5s after a hit", L"被弾後の維持 3秒 → 5秒" },
        { L"콤보 5 → 3 이상", L"Combo 5 → 3 or more", L"コンボ 5 → 3以上" },
        { L"웨이브 시작 20초 → 12초 뒤부터", L"From 20s → 12s into the wave", L"ウェーブ開始 20秒 → 12秒後から" },
    };
    const int i = std::max(0, std::min((int)ChCond::Count - 1, (int)c));
    return t[i][std::max(0, std::min(2, lang))];
}

// One branch's line on the card. Branch 0 A, 1 B, 2 C. `chance`: a chance card (kind, cond, pct).
inline std::wstring BranchLine(const UpRow* row, bool chance, ChKind kind, ChCond cond, int pct,
                               int branch, int lang) {
    if (chance) {
        if (branch == 0) {
            const ChanceUp up = ChanceUpgrade(pct);
            if (up.effectGrows) return ChanceAEffectText(kind, lang);
            return UpPart(UpVal{ UpLabel::Chance, UpUnit::Pct, (float)pct / 100.0f, (float)up.pct / 100.0f }, lang);
        }
        if (branch == 1) return ChanceBText(cond, lang);
        return ShapeText(ChanceFamily(kind), lang);
    }
    if (!row) return L"";
    if (branch == 0) {
        std::wstring s = UpPart(row->A, lang);
        if (row->A2.on()) s += L" · " + UpPart(row->A2, lang);
        return s;
    }
    if (branch == 1) return row->B.on() ? UpPart(row->B, lang) : std::wstring();
    return ShapeText(row->fam, lang);
}

inline const wchar_t* BranchName(int branch, int lang) {
    static const wchar_t* const k[3][3] = { { L"수치 A", L"Number A", L"数値A" },
                                            { L"수치 B", L"Number B", L"数値B" },
                                            { L"모양", L"Shape", L"形" } };
    return k[std::max(0, std::min(2, branch))][std::max(0, std::min(2, lang))];
}

}  // namespace synergy
