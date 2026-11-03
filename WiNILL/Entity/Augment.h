#pragma once
#include "Platform.h"
#include "Settings.h"   // g_Language, LANG_COUNT

// Augments (Docs/QA/Asteria_증강기획_RnS기반_100.xlsx): passives work on their
// own (PassiveAugments.cpp); actives go into a skill slot and fire on a key
// (ActiveSkills.cpp).
enum class AugType {
    P_NIGHT_CRESCENT_DAGGER, P_NIGHT_PAJAMA_CAP,
    P_CHRONO_METRONOME_BOOTS, P_CHRONO_GEMINI_NECKLACE,
    P_GALE_GALE_MAGAZINE, P_GALE_SPARROW_FEATHER, P_GALE_SHINSOKU_BLADE,
    P_GALE_WINDBITE_FANG,
    P_SHADOW_BLACK_WAKIZASHI, P_SHADOW_SHADOW_BRACELET, P_SHADOW_SHINOBI_TABI,
    P_STONE_HARD_KNUCKLE, P_STONE_DRAGONHEAD_SPEAR, P_STONE_GIANT_STONE_CLUB,
    P_FLAME_DEMON_HORNS, P_FLAME_PHOENIX_CHARM,
    P_GEM_AMETHYST_BRACELET, P_GEM_EMERALD_PLATE, P_GEM_RUBY_CIRCLET,
    P_STORM_BRIGHTSTORM_SPEAR, P_STORM_THUNDERCLAP_GLOVES, P_STORM_CROWN_OF_STORMS,
    P_LUCK_SILVER_COIN, P_LUCK_TOPAZ_CHARM, P_LUCK_QUEENS_CROWN,
    P_LIFE_LIFE_SEED, P_LIFE_MORNING_DEW, P_LIFE_SUNFLOWER_CROWN,
    P_LIFE_STRAWBERRY_CAKE,
    P_ABYSS_SEASHELL_SHIELD,
    P_SPARK_STATICSHOCK_EARRINGS, P_SPARK_BATTERY_SHIELD, P_SPARK_BLACKBOLT_RIBBON,
    P_FLIGHT_TINY_WINGS, P_FLIGHT_CLOUD_GUARD, P_FLIGHT_CRANE_BLADE,
    P_EDGE_WHETSTONE, P_EDGE_POINTED_RING, P_EDGE_SNIPERS_EYEGLASS,
    P_EDGE_CROWN_OF_SWORDS,
    P_HERESY_ANGELS_HALO, P_HERESY_DARK_WINGS,
    P_HUNTER_WOLF_FANG, P_HUNTER_SAWTOOTH_CLEAVER, P_HUNTER_BLOODHOUND_FANG,
    P_PERFORM_STRONGMANS_BAR, P_PERFORM_RAINBOW_CAPE,
    P_STILL_FLORAL_BOW, P_STILL_CLAY_FIGURE, P_STILL_IRON_GREAVES,
    P_VEIN_CANARY_CHARM, P_VEIN_IRON_PICKAXE, P_VEIN_MINERS_HEADLAMP,
    P_NOVA_METEOR_SHARD, P_NOVA_SUPERNOVA_CORE,
    // Passives that act on active skills.
    P_NIGHT_MOON_PENDANT, P_CHRONO_TIMEMAGE_CAP, P_FLAME_FIRESCALE_CORSET,
    P_ABYSS_OCCULT_DAGGER, P_ABYSS_BLACKHOLE_CHARM, P_LUCK_ROYAL_SCEPTER,
    P_VEIN_PYRITE_EARRINGS, P_NOVA_AFTERGLOW, P_PERFORM_RIBBONED_STAFF,
    P_HERESY_RIGHTHAND_CAST,
    // Actives.
    A_NIGHT_NIGHTSTAR_GRIMOIRE, A_NIGHT_SLEEPING_ARROW,
    A_CHRONO_REWIND, A_CHRONO_COG_SPIN,
    A_GALE_GUST,
    A_SHADOW_KNIFE_FAN, A_SHADOW_SHADOW_CLONE,
    A_STONE_GRANITE_SLAM, A_STONE_BOULDER_WALL,
    A_FLAME_FIREBALL, A_FLAME_METEOR_FALL,
    A_GEM_DIAMOND_PRISM, A_GEM_GARNET_SHARDS,
    A_STORM_LIGHTNING_STRIKE, A_STORM_THUNDERCLOUD,
    A_LUCK_BOOK_OF_CHEATS,
    A_LIFE_BLOOM,
    A_ABYSS_TIDAL_WAVE, A_ABYSS_ABYSS_GATE,
    A_SPARK_DISCHARGE, A_SPARK_CHAIN_LIGHTNING,
    A_FLIGHT_BLINK, A_FLIGHT_COMET_RUSH,
    A_EDGE_SNIPE,
    A_HERESY_FORBIDDEN_RITE, A_HERESY_DAY_AND_NIGHT,
    A_HUNTER_SNARE_TRAP, A_HUNTER_EXECUTE,
    A_PERFORM_SPINNING_HOOP, A_PERFORM_ENCORE,
    A_STILL_STILL_TURRET, A_STILL_MEDITATION,
    A_VEIN_DYNAMITE,
    A_NOVA_SUPERNOVA, A_NOVA_STARBURST,
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
