#pragma once
// Card upgrades (augment rework 1차, 2026-11-23; Docs/Plans/2026-11-23_증강구조_개편_1차.md).
// A wave clear upgrades one owned card from stage 1 to stage 2. Each card has one key
// number that grows (B안: "핵심 수치 1개"), a few a second number with it (a cost card's
// cost grows too). This table is the only place the numbers live: SynergyRuntime.cpp
// reads both stages from it (UPV / UPV2), and the card text's "강화" line is built from it.
//
// Not upgraded in 1차: frames (picked at run start), mythics, and the two laser-only
// amps (their numbers live in LaserFx; the rework moves them to a frame-linked pool).
// Chance cards (Role::Chance) follow one rule instead of rows here: see ChanceUpgrade.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace synergy {

// What the number is, for the "강화" line (KR, EN, JP).
enum class UpLabel {
    Bounces, Pierce, Split, SplitDmg, Homing, Damage, DamageUp, Max, Targets, PerShot,
    Every, Chance, Shots, Mult, Shield, Heal, Seconds, Range, PerDamage, PerKill, PerWave,
    PerTag, PerCard, FireRate, MaxHp, MaxHpDown, Taken, PerSecond, Count_
};
// How it reads: Pct shows value x100 with "%", Mult "×v", Count/Flat whole numbers,
// Sec "v초", Ring "v○".
enum class UpUnit { Pct, Mult, Count, Flat, Sec, Ring };

struct UpRow {
    const char* code;
    UpLabel l1; UpUnit u1; float a1, b1;                      // stage 1 → stage 2
    UpLabel l2 = UpLabel::Count_; UpUnit u2 = UpUnit::Count; float a2 = 0.0f, b2 = 0.0f;
};

inline const UpRow* UpRows(int& n) {
    using L = UpLabel; using U = UpUnit;
    static const UpRow k[] = {
        // ── 동사 ──
        { "V_BOUNCE",    L::Bounces,  U::Count, 2.0f, 3.0f },
        { "V_RICOCHET",  L::Bounces,  U::Count, 1.0f, 2.0f },
        { "V_PIERCE",    L::Pierce,   U::Count, 2.0f, 3.0f },
        { "V_SPLIT",     L::Split,    U::Count, 3.0f, 4.0f, L::SplitDmg, U::Pct, 0.28f, 0.35f },
        { "V_HOMING",    L::Homing,   U::Mult,  1.0f, 1.6f },
        { "V_BLAST",     L::Damage,   U::Pct,   0.30f, 0.45f },
        { "V_ORBIT",     L::Max,      U::Count, 2.0f, 3.0f },
        { "V_CHAIN",     L::Targets,  U::Count, 3.0f, 5.0f },
        { "V_TRAIL",     L::PerSecond, U::Pct,  0.40f, 0.60f },
        { "V_HOVER",     L::DamageUp, U::Pct,   0.70f, 1.00f },
        { "V_TWIN",      L::Damage,   U::Pct,   0.65f, 0.80f },
        { "V_MARK",      L::DamageUp, U::Pct,   0.25f, 0.40f },
        { "V_BURN",      L::PerSecond, U::Pct,  0.20f, 0.30f },
        { "V_HUNTER",    L::Damage,   U::Mult,  1.0f, 1.3f },
        { "V_SPRAY",     L::Damage,   U::Pct,   0.35f, 0.50f },
        { "V_GRAVITY",   L::Range,    U::Ring,  1.5f, 2.25f },
        { "V_DICE",      L::Count_,   U::Count, 2.0f, 3.0f },   // bounce / pierce outcome +2 → +3
        { "V_MINE",      L::Damage,   U::Pct,   2.20f, 3.30f },
        { "V_GROW",      L::Max,      U::Mult,  2.5f, 3.0f },
        { "V_ABSORB",    L::PerShot,  U::Pct,   0.05f, 0.08f },
        // ── 트리거 ──
        { "T_NTH",       L::Every,    U::Count, 6.0f, 4.0f },
        { "T_KILLBURST", L::Chance,   U::Pct,   0.12f, 0.20f, L::Shots, U::Count, 4.0f, 6.0f },
        { "T_CRITECHO",  L::Damage,   U::Pct,   0.80f, 1.20f },
        { "T_REVENGE",   L::Damage,   U::Pct,   3.00f, 4.50f },
        { "T_STILL",     L::Mult,     U::Mult,  3.0f, 4.0f },
        { "T_GRAZE",     L::Shield,   U::Flat,  25.0f, 40.0f },
        { "T_CAKE",      L::Heal,     U::Flat,  20.0f, 35.0f },
        { "T_LASTSTAND", L::DamageUp, U::Pct,   0.50f, 0.75f },
        { "T_GRUDGE",    L::PerDamage, U::Pct,  0.10f, 0.15f },
        { "T_COMBO",     L::Max,      U::Pct,   0.40f, 0.60f },
        { "T_RHYTHM",    L::Mult,     U::Mult,  3.0f, 4.0f },
        { "T_OPENING",   L::Seconds,  U::Sec,   3.0f, 5.0f },
        { "T_AFTERIMAGE", L::PerSecond, U::Pct, 0.35f, 0.55f },
        { "T_BERSERK",   L::Max,      U::Pct,   0.60f, 0.90f },
        { "T_ANCHOR",    L::PerSecond, U::Pct,  0.15f, 0.25f },
        { "T_FIREWORK",  L::Shots,    U::Count, 12.0f, 18.0f },
        // ── 대가 (강화하면 대가도 커짐) ──
        { "C_GLASS",     L::Mult,     U::Mult,  3.1f, 3.6f, L::MaxHp, U::Flat, 30.0f, 25.0f },
        { "C_HERESY",    L::FireRate, U::Pct,   0.45f, 0.65f, L::MaxHpDown, U::Flat, 20.0f, 30.0f },
        { "C_REVIVE",    L::Heal,     U::Flat,  20.0f, 40.0f },
        { "C_RECKLESS",  L::DamageUp, U::Pct,   0.12f, 0.20f },
        { "C_RITUAL",    L::PerWave,  U::Pct,   0.08f, 0.12f },
        { "C_ASCETIC",   L::DamageUp, U::Pct,   0.90f, 1.20f, L::Taken, U::Pct, 0.20f, 0.30f },
        // ── 보호 ──
        { "G_CHARGER",   L::Shield,   U::Flat,  40.0f, 60.0f },
        { "G_STILLGUARD", L::Shield,  U::Flat,  30.0f, 45.0f },
        { "G_PARRY",     L::Seconds,  U::Sec,   0.45f, 0.70f },
        { "G_CIRCUIT",   L::Shield,   U::Flat,  32.0f, 48.0f },
        { "G_FORCEFIELD", L::Shield,  U::Flat,  15.0f, 30.0f },
        { "G_MIRROR",    L::Shield,   U::Flat,  90.0f, 135.0f },
        { "G_LASTWALL",  L::Shield,   U::Flat,  220.0f, 330.0f },
        // ── 증폭 ──
        { "A_COLLECTOR", L::PerTag,   U::Pct,   0.09f, 0.13f },
        { "A_REACTOR",   L::PerTag,   U::Pct,   0.14f, 0.20f },
        { "A_GAMBLER",   L::PerCard,  U::Pct,   0.10f, 0.14f },
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

// Chance cards: the upgraded card's chance is x1.5. A pierce / bounce / extra-shot
// chance past 100% is one more sure +1 (RollVerbs). Split, blast and crit cannot use
// a chance past 100%: those already at 100% grow the effect instead (4-way split at
// 35%, blast 45%, +40% crit damage while the condition holds).
inline constexpr float kChanceUpMult = 1.5f;
inline constexpr float kChanceUpSplitDmg = 0.35f;
inline constexpr float kChanceUpBlastPct = 0.45f;
inline constexpr float kChanceUpCritMul = 0.40f;

// Text for one number (lang 0 KR, 1 EN, 2 JP).
inline std::wstring UpNumber(UpUnit u, float v, int lang) {
    wchar_t buf[32];
    switch (u) {
    case UpUnit::Pct: {
        const float p = v * 100.0f;
        if (std::fabs(p - std::round(p)) < 0.05f) swprintf(buf, 32, L"%.0f%%", p);
        else swprintf(buf, 32, L"%.1f%%", p);
        break;
    }
    case UpUnit::Mult: swprintf(buf, 32, std::fabs(v - std::round(v)) < 0.01f ? L"×%.0f" : L"×%.1f", v); break;
    case UpUnit::Sec:
        swprintf(buf, 32, L"%g", v);
        return std::wstring(buf) + (lang == 1 ? L"s" : lang == 2 ? L"秒" : L"초");
    case UpUnit::Ring: swprintf(buf, 32, L"%g○", v); break;
    default: swprintf(buf, 32, L"%.0f", v); break;
    }
    return buf;
}

inline const wchar_t* UpLabelText(UpLabel l, int lang) {
    static const wchar_t* const k[(int)UpLabel::Count_ + 1][3] = {
        { L"튕김",          L"Bounces",       L"跳弾" },
        { L"관통",          L"Pierce",        L"貫通" },
        { L"갈래",          L"Split",         L"分裂" },
        { L"분열 피해",     L"Split damage",  L"分裂ダメージ" },
        { L"유도 세기",     L"Homing",        L"誘導の強さ" },
        { L"피해",          L"Damage",        L"ダメージ" },
        { L"피해 +",        L"Damage +",      L"ダメージ+" },
        { L"최대",          L"Max",           L"最大" },
        { L"대상",          L"Targets",       L"対象" },
        { L"적탄당",        L"Per bullet",    L"敵弾ごと" },
        { L"주기 (발)",     L"Every (shots)", L"周期 (発)" },
        { L"확률",          L"Chance",        L"確率" },
        { L"탄 수",         L"Shots",         L"弾数" },
        { L"배율",          L"Multiplier",    L"倍率" },
        { L"보호막",        L"Shield",        L"シールド" },
        { L"회복",          L"Heal",          L"回復" },
        { L"시간",          L"Duration",      L"時間" },
        { L"범위",          L"Range",         L"範囲" },
        { L"피해 1당",      L"Per damage",    L"ダメージ1ごと" },
        { L"처치당",        L"Per kill",      L"撃破ごと" },
        { L"웨이브당",      L"Per wave",      L"ウェーブごと" },
        { L"효과 1종당",    L"Per kind",      L"1種ごと" },
        { L"1장당",         L"Per card",      L"1枚ごと" },
        { L"연사 +",        L"Fire rate +",   L"連射+" },
        { L"최대 체력",     L"Max HP",        L"最大体力" },
        { L"최대 체력 -",   L"Max HP -",      L"最大体力-" },
        { L"받는 피해 +",   L"Damage taken +", L"被ダメージ+" },
        { L"초당",          L"Per second",    L"毎秒" },
        { L"결과",          L"Outcome",       L"結果" },
    };
    const int i = std::max(0, std::min((int)UpLabel::Count_, (int)l));
    return k[i][std::max(0, std::min(2, lang))];
}

inline std::wstring UpPart(UpLabel l, UpUnit u, float a, float b, int lang) {
    return std::wstring(UpLabelText(l, lang)) + L" " + UpNumber(u, a, lang) + L" → " + UpNumber(u, b, lang);
}

}  // namespace synergy
