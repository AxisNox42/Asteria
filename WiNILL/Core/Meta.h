#pragma once
// ─────────────────────────────────────────────────────────────
// 메타 진행 — 판마다 코인 적립 → 영구 업그레이드 구매 (저장됨)
// Apply permanent meta stats to PlayerStats when a run starts.
// ─────────────────────────────────────────────────────────────
#include "PlayerStats.h"
#include "Settings.h"   // CurLangIdx 는 Augment.h 에 있지만 언어 인덱스만 필요

// The META_RESERVED_* slots preserve saved meta indices after removing the
// obsolete start-vision, start-augment and identity-slot upgrades.
// META_MAGNET is the 1-stardust module the first shop visit has the player buy
// (appended, not put in a reserved slot, so old saves cannot already own it).
enum MetaId { META_HP, META_DMG, META_MOVE, META_RESERVED_VISION, META_RESERVED_STARTAUG, META_RESERVED_IDSLOT,
              META_MAGNET, META_COUNT };
inline bool MetaSlotRemoved(int id) {
    return id == META_RESERVED_VISION || id == META_RESERVED_STARTAUG || id == META_RESERVED_IDSLOT;
}

struct MetaDef {
    const wchar_t* name[3];   // KR / EN / JP
    int            maxLv;
    long long      baseCost;  // 1레벨 비용 (레벨마다 ×1.6)
};
inline const MetaDef META_DEFS[META_COUNT] = {
    { { L"최대 체력 +20",  L"Max HP +20",       L"最大HP +20" },   5, 100 },
    { { L"공격력 +6%",     L"Attack +6%",       L"攻撃力 +6%" },   5, 150 },
    { { L"이동속도 +5%",   L"Move Speed +5%",   L"移動速度 +5%" }, 3, 120 },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0 },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0 },
    { { L"REMOVED", L"REMOVED", L"REMOVED" }, 0, 0 },
    { { L"별가루 자석 +15%", L"Stardust Magnet +15%", L"星屑マグネット +15%" }, 1, 1 },
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

// Stardust starts flying to the ship from this much farther (Stardust Magnet: +15%).
inline float MetaStardustPullMult() {
    return 1.0f + 0.15f * (float)g_MetaLv[META_MAGNET];
}

inline void ApplyMeta(PlayerStats& s) {
    s.maxHP            += 20.0f * g_MetaLv[META_HP];
    s.damageMultiplier *= (1.0f + 0.06f * (float)g_MetaLv[META_DMG]);
    s.moveSpeedMult    *= (1.0f + 0.05f * (float)g_MetaLv[META_MOVE]);
}
