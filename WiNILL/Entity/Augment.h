#pragma once
#include "Platform.h"
#include "Settings.h"   // g_Language, LANG_COUNT

// Augments (Docs/QA/Asteria_증강기획_RnS기반_100.xlsx): passives work on their
// own (PassiveAugments.cpp); actives go into a skill slot and fire on a key
// (ActiveSkills.cpp).
enum class AugType {
    P_NIGHT_PULSE_ROUND, P_NIGHT_DWARF_COOLING,
    P_CHRONO_STAR_TEMPO, P_CHRONO_DOUBLE_CAST,
    P_GALE_FIRE_RATE_UP, P_GALE_SCAN_PULSE, P_GALE_VARIABLE_FIRE,
    P_GALE_PARTICLE_ROUNDS,
    P_SHADOW_TRIPLE_STRIKE, P_SHADOW_MULTI_HIT_AMP, P_SHADOW_QUIET_ORBIT,
    P_STONE_ATTACK_UP, P_STONE_NEUTRON_WARHEAD, P_STONE_GIANT_WARHEAD,
    P_FLAME_LIMIT_BREAK, P_FLAME_UNDYING_REIGNITION,
    P_GEM_UNSTABLE_AMP, P_GEM_BUFFER_PLATING, P_GEM_GLASS_STAR,
    P_STORM_CORONA_ROUND, P_STORM_MAGNETIC_STORM, P_STORM_CASCADE_COLLISION,
    P_LUCK_STARDUST_SUPPLY, P_LUCK_GIFT_OF_STARS, P_LUCK_LUCKY_STAR,
    P_LIFE_MAX_HP_UP, P_LIFE_REGENERATION, P_LIFE_GRAZE_RECOVERY,
    P_LIFE_EMERGENCY_MEND_SHARDS,
    P_ABYSS_ERRATIC_ORBIT,
    P_SPARK_STATIC_BUILDUP, P_SPARK_STELLAR_DYNAMO, P_SPARK_CRITICAL_BURST,
    P_FLIGHT_MOVE_SPEED_UP, P_FLIGHT_ROUTE_SWEEP, P_FLIGHT_MOMENTUM_AMPLIFIER,
    P_EDGE_CRITICAL_STRIKE, P_EDGE_CRITICAL_AMP, P_EDGE_LONG_RANGE_CORRECTION,
    P_EDGE_CRITICAL_CHAIN_BURST,
    P_HERESY_BOUND_AMP, P_HERESY_UNSTABLE_DRIVE,
    P_HUNTER_PROXIMITY_WARHEAD, P_HUNTER_WEAK_POINT_EXECUTION,
    P_PERFORM_RUSH_SHOT, P_PERFORM_AUGMENT_REPLICATOR,
    P_STILL_STATIONARY_SATELLITE, P_STILL_STILLNESS_PLATING, P_STILL_SEA_OF_TRANQUILITY,
    P_VEIN_GROWING_STAR, P_VEIN_CRATER_ROUND, P_VEIN_CIRCULATING_COOLER,
    P_NOVA_STARDUST_HARVEST, P_NOVA_STELLAR_CORE_RUPTURE,
    // Passives that act on active skills.
    P_NIGHT_RESONANT_SHOCKWAVE, P_CHRONO_TIME_COMPRESSOR, P_FLAME_STELLAR_OVERHEAT,
    P_ABYSS_OVERCHARGED_CAST, P_ABYSS_SINGULARITY_CASCADE, P_LUCK_WEALTH_AMP,
    P_VEIN_SKILL_SYNERGY, P_NOVA_AFTERGLOW_RECOVERY, P_PERFORM_ECLIPSE_COMBO,
    P_HERESY_SLOT_SACRIFICE,
    // Actives.
    A_NIGHT_STARFALL, A_NIGHT_METEOR_LANCE,
    A_CHRONO_ORBITAL_RETURN, A_CHRONO_RESONANT_SYNC,
    A_GALE_STELLAR_GALE,
    A_SHADOW_SHARD_VOLLEY, A_SHADOW_TWIN_STAR_CLONE,
    A_STONE_GRAVITY_SLAM, A_STONE_ASTEROID_WALL,
    A_FLAME_PROMINENCE_SHELL, A_FLAME_METEOR_BARRAGE,
    A_GEM_REFRACTION_CRYSTAL, A_GEM_BARRAGE_RECYCLE,
    A_STORM_TRIPLE_BOLT, A_STORM_STORM_NEBULA,
    A_LUCK_DICE_OF_FATE,
    A_LIFE_STARLIGHT_MEND,
    A_ABYSS_TIDAL_SURGE, A_ABYSS_BLACK_HOLE_CAPTURE,
    A_SPARK_FORCED_DISCHARGE, A_SPARK_CHAIN_LIGHTNING,
    A_FLIGHT_WARP_JUMP, A_FLIGHT_COMET_RUSH,
    A_EDGE_PRECISION_SNIPE,
    A_HERESY_LIFE_BURN_BLAST, A_HERESY_DAY_NIGHT_BOUNDARY,
    A_HUNTER_INTERSTELLAR_MINE, A_HUNTER_PINPOINT_EXECUTION,
    A_PERFORM_ORBIT_RING, A_PERFORM_ECHO_CAST,
    A_STILL_STARLIGHT_TURRET, A_STILL_CONDENSED_BLAST,
    A_VEIN_TIMED_BOMB,
    A_NOVA_NOVA_BURST, A_NOVA_STARLIGHT_SCATTER,
    // Level-up stat cards (only these are offered on a level-up).
    S_ATTACK, S_MAX_HP, S_FIRE_RATE, S_MOVE_SPEED, S_REGEN, S_CRIT,
    COUNT
};

// What an active skill does, for its HUD glyph (분류 in the planning sheet).
enum class SkillKind { None, Projectile, Area, Install, Summon, Move, Heal,
                       Link, Channel, Random, Form };

enum class AugRarity { COMMON, RARE, EPIC, LEGENDARY, MYTHIC };

struct AugDef {
    AugType        type;
    AugRarity      rarity;
    bool           stackable;             // may be taken again
    float          cooldown;              // active: base cooldown (s); passive: 0
    SkillKind      kind;                  // active: glyph group; passive: None
    const char*    name;                  // 영문 코드명 (디버그)
    const wchar_t* locName[LANG_COUNT];   // [KR, EN, JP] 카드 이름
    const wchar_t* locDesc[LANG_COUNT];   // [KR, EN, JP] 카드 설명
};

// 현재 언어 인덱스 (범위 클램프)
inline int CurLangIdx() {
    int li = LangIndex();
    if (li < 0 || li >= LANG_COUNT) li = 0;
    return li;
}

static constexpr int AUG_TOTAL = (int)AugType::COUNT;
extern const AugDef ALL_AUGS[];

inline const char* AugRarityName(AugRarity r) {
    switch (r) {
    case AugRarity::COMMON:    return "COMMON";
    case AugRarity::RARE:      return "RARE";
    case AugRarity::EPIC:      return "EPIC";
    case AugRarity::LEGENDARY: return "LEGENDARY";
    case AugRarity::MYTHIC:    return "MYTHIC";
    }
    return "?";
}

inline bool AugIsActive(const AugDef& d) { return d.kind != SkillKind::None; }
// Level-up stat cards: drawn on level-ups only, never on a wave clear.
inline bool AugIsStatCard(const AugDef& d) { return (int)d.type >= (int)AugType::S_ATTACK; }
inline const wchar_t* AugName(const AugDef& d) { return d.locName[CurLangIdx()]; }
inline const wchar_t* AugDesc(const AugDef& d) { return d.locDesc[CurLangIdx()]; }

inline void GetRarityColor(AugRarity r, float& cr, float& cg, float& cb) {
    switch (r) {
    case AugRarity::COMMON:    cr = 0.15f; cg = 0.65f; cb = 0.20f; break; // 녹
    case AugRarity::RARE:      cr = 0.10f; cg = 0.30f; cb = 0.85f; break; // 청
    case AugRarity::EPIC:      cr = 0.55f; cg = 0.10f; cb = 0.80f; break; // 보라
    case AugRarity::LEGENDARY: cr = 0.95f; cg = 0.70f; cb = 0.10f; break; // 금
    case AugRarity::MYTHIC:    cr = 1.00f; cg = 0.25f; cb = 0.35f; break; // 진홍
    }
}

// 등급 라벨 (현재 언어)
inline const wchar_t* GetRarityLabel(AugRarity r) {
    static const wchar_t* s[5][3] = {
        { L"일반", L"Common", L"コモン" },
        { L"희귀", L"Rare", L"レア" },
        { L"에픽", L"Epic", L"エピック" },
        { L"전설", L"Legendary", L"レジェンド" },
        { L"신화", L"Mythic", L"神話" },
    };
    return s[(int)r][CurLangIdx()];
}
