#pragma once
#include <algorithm>
#include <cmath>
#include "SpaceRules.h"
#include "PassiveHooks.h"
#include "../System/GameLog.h"

struct PlayerStats {
    float damageMultiplier = 1.0f;
    float baseDamage       = 50.0f;
    float maxHP            = 100.0f;
    float bulletSpeed      = 1200.0f;
    float fireInterval     = 0.20f;   // 기본 연사력 하향 (0.15 → 0.20, 더 느리게)
    float windowSize       = 800.0f;
    float moveSpeedMult    = 1.0f;
    float regenPerSec      = 1.0f / 3.0f;  // 기본 3초당 HP 1 회복
    float playerSizeMult   = 1.0f;
    float xpMult           = 1.0f;   // Global experience multiplier.
    float bulletSpread     = 0.0f;   // 발사 시 각도 흔들기 (라디안). 0 = 정확
    int   critChance       = 0;      // 치명타 확률 (%)
    float critMult         = 2.0f;   // 치명타 피해 배율
    long long killCount = 0;         // Total kills in the current run.
    float baseFireInterval = 0.20f;  // 무기 적용 후 발사 간격

    void RegisterKill() { ++killCount; }

    float GetBaseDamage() const { return baseDamage < 1.0f ? 1.0f : baseDamage; }
    float GetDamageMultiplier() const { return damageMultiplier; }
};

// 전투 피해 감소 — main 에서 g_Stats.GetDamageTakenMult() 로 매 프레임 동기화
inline float g_PlayerDmgMult = 1.0f;
inline float g_PlayerShield     = 0.0f;
inline float g_PlayerShieldTimer = 0.0f;
inline float g_PlayerDamagePulse = 0.0f;

inline void GrantPlayerShield(float amount, float durationSec) {
    if (amount <= 0.0f || durationSec <= 0.0f) return;
    g_PlayerShield      = amount;
    g_PlayerShieldTimer = durationSec;
}

inline void TickPlayerShield(float dt) {
    if (g_PlayerShieldTimer > 0.0f) {
        g_PlayerShieldTimer -= dt;
        if (g_PlayerShieldTimer <= 0.0f) {
            g_PlayerShieldTimer = 0.0f;
            g_PlayerShield      = 0.0f;
        }
    }
}

// src names the attacker for the balance log (System/GameLog.h).
inline void HurtPlayer(float& hp, float raw, const char* src = "?") {
    if (raw <= 0.0f) return;
    // NORMA window spaces: an attacker in another space cannot reach.
    if (!space::PlayerExposed()) return;
    float dmg = raw * g_PlayerDmgMult;
    if (g_PlayerShield > 0.0f) {
        if (dmg <= g_PlayerShield) {
            g_PlayerShield -= dmg;
            gamelog::Taken("SHIELD_ABSORBED", dmg);
            return;
        }
        gamelog::Taken("SHIELD_ABSORBED", g_PlayerShield);
        dmg -= g_PlayerShield;
        g_PlayerShield = 0.0f;
    }
    gamelog::Taken(src, dmg);
    if (passive_hooks::hurt) passive_hooks::hurt(hp, dmg);
    else                     hp -= dmg;
    // Notify the combat HUD at the source of the hit, including tiny hits
    // that can be obscured by several fixed-step updates in one frame.
    g_PlayerDamagePulse = std::max(g_PlayerDamagePulse, 2.2f);
}
