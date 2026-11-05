#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <glm/glm.hpp>

#include "MonsterManager.h"
#include "Bullet.h"
#include "PlayerStats.h"
#include "BalanceTuning.h"
#include "Juice.h"
#include "../Core/Codex.h"

// A player shot hits a monster within this radius, times its sizeScale
// (also used by the READY practice field).
inline constexpr float MONSTER_BULLET_HIT_RADIUS = 15.0f;

// Kill stardust: spawned now, or held on the mob (Quasar, Genesis) and
// released by ReleasePendingStardust.
inline void SpawnKillStardust(Monster& m, int value, float playerCX, float playerCY,
                              XpAmount xp) {
    if (m.kind == MobKind::QUASAR || m.kind == MobKind::GENESIS) {
        m.dustPending = true;
        m.pendingDustValue = value;
        m.pendingDustXp = xp;
        return;
    }
    SpawnStardust(m.worldX, m.worldY, value, playerCX, playerCY, xp);
}

// A thrown Genesis Swarm's kill, on every kill path: the combo and the codex
// only — no score, XP/stardust or run kill count.
inline void CreditThrownSwarmKill(Monster& m) {
    m.scored = true;
    AddKillCombo();
    RegisterCodexMobKill(m.kind);
}

// The only owner of ordinary kill rewards. Callers decide when a death is
// eligible; scored prevents the same death being credited by another weapon.
struct KillRewardSink {
    float& playerHP;
    float& scoreAccum;
    long long& score;
    PlayerStats& stats;
    float playerCX, playerCY;
};

inline void CreditRegularKill(KillRewardSink& s, CodexMobId mobId, float scoreBase) {
    AddKillCombo();
    s.stats.RegisterKill();
    RegisterCodexMobKill(mobId);
    s.scoreAccum += scoreBase;
    s.score = (long long)s.scoreAccum;
    if (passive_hooks::kill) passive_hooks::kill();
}

// An Antares pays nothing until its whole family (the first body and every
// split fragment) is down: each body's share is held, then the sum drops at
// the last fragment.
struct AntaresFamilyReward {
    int outstanding = 1;      // bodies of the family still to fall
    XpAmount xp = 0;
    float score = 0.0f;
    int stardust = 0;
};
inline std::unordered_map<int, AntaresFamilyReward> g_AntaresFamilies;

// Adds this body's share to its family. True (with the family's totals in the
// arguments) when this was the family's last body.
inline bool HoldAntaresReward(const Monster& m, XpAmount& xp, float& score, int& stardust) {
    AntaresFamilyReward& f = g_AntaresFamilies[m.antaresFamily];
    f.xp += xp;
    f.score += score;
    f.stardust += stardust;
    --f.outstanding;
    // Same rule as MonsterManager::SpawnAntaresSplits: this body splits.
    if (!m.noBlast && m.antaresGen < Monster::ANTARES_MAX_GEN)
        f.outstanding += Monster::ANTARES_SPLIT_COUNT;
    if (f.outstanding > 0) return false;
    xp = f.xp;
    score = f.score;
    stardust = f.stardust;
    g_AntaresFamilies.erase(m.antaresFamily);
    return true;
}

inline void CollectMonsterKill(KillRewardSink& s, Monster& m) {
    if (m.scored) return;
    if (m.genesisThrown && !m.genesisSummoned) {      // capsules, Swarms and Rotors: no reward
        CreditThrownSwarmKill(m);
        return;
    }
    m.scored = true;
    float xpBase, scoreBase;
    MobKillReward(m.kind, xpBase, scoreBase);
    scoreBase *= MobRewardMult(m.kind);
    XpAmount pickupXp = MobExperienceReward(m.kind, s.stats);
    for (int g = 0; m.kind == MobKind::ANTARES && g < m.antaresGen; ++g) {   // each split generation pays half
        pickupXp /= 2;
        scoreBase *= 0.5f;
    }
    if (m.genesisSummoned) {                          // what a Genesis summons pays half
        pickupXp /= 2;
        scoreBase *= 0.5f;
    }
    int stardust = StardustRewardFor(m.kind);
    if (m.kind == MobKind::ANTARES && !HoldAntaresReward(m, pickupXp, scoreBase, stardust)) {
        CreditRegularKill(s, CodexMobIdForKind(m.kind), 0.0f);   // counts as a kill; pays later
        return;
    }
    SpawnKillStardust(m, stardust, s.playerCX, s.playerCY, pickupXp);
    CreditRegularKill(s, CodexMobIdForKind(m.kind), scoreBase);
    gamelog::Kill(MobKindName(m.kind));
    gamelog::Event("KILL", "mob=%s,xp=%s,score=%.0f,stardust=%d%s", MobKindName(m.kind),
                   FormatXp(pickupXp).c_str(), scoreBase, stardust,
                   m.genesisSummoned ? ",summoned=1" : "");
}

inline void CollectScopeKill(KillRewardSink& s, RangedMob& r) {
    if (r.scored) return;
    r.scored = true;
    XpAmount pickupXp = RangedMob::ExperienceReward(s.stats);
    float scopeScore = 300.0f;
    if (r.genesisSummoned) {                          // what a Genesis summons pays half
        pickupXp /= 2;
        scopeScore *= 0.5f;
    }
    SpawnStardust(r.worldX, r.worldY, 3,
                  s.playerCX, s.playerCY, pickupXp);
    CreditRegularKill(s, CM_SCOPE, scopeScore);
    gamelog::Kill("SCOPE");
    gamelog::Event("KILL", "mob=SCOPE,xp=%s,score=%.0f%s",
                   FormatXp(pickupXp).c_str(), scopeScore,
                   r.genesisSummoned ? ",summoned=1" : "");
}

// Releases held kill stardust whose moment has come. Call every frame.
inline void ReleasePendingStardust(MonsterManager& manager, float playerCX, float playerCY) {
    for (auto* m : manager.monsters) {
        if (!m || !m->TakePendingDust()) continue;
        const float base = (m->summoned ? 28.0f : 18.0f) * m->sizeScale;
        SpawnStardust(m->worldX, m->worldY, m->pendingDustValue, playerCX, playerCY,
                      m->pendingDustXp, base * 0.9f);
    }
}

static inline float SegDistSq(float px, float py, float ax, float ay,
                              float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float len2 = abx * abx + aby * aby;
    float t = 0.0f;
    if (len2 > 1e-6f) {
        t = ((px - ax) * abx + (py - ay) * aby) / len2;
        t = std::max(0.0f, std::min(1.0f, t));
    }
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy;
}

static inline float SegDist(float px, float py, float ax, float ay,
                            float bx, float by) {
    return std::sqrt(SegDistSq(px, py, ax, ay, bx, by));
}

class CollisionSystem {
public:
    static bool Update(float playerCX, float playerCY,
                       MonsterManager& manager, std::vector<Bullet>& bullets,
                       float& playerHP, float& scoreAccum, long long& score,
                       PlayerStats& stats) {
        bool playerHit = false;
        KillRewardSink rewards{playerHP, scoreAccum, score, stats, playerCX, playerCY};

        for (auto& bullet : bullets) {
            if (!bullet.active) continue;
            if (bullet.isEnemy) {
                if (space::Active() && bullet.space >= 0 &&
                    bullet.space != space::g_PlayerSpace) continue;
                const float hitRadius = 10.0f * stats.playerSizeMult;
                if (SegDistSq(playerCX, playerCY, bullet.prevX, bullet.prevY,
                              bullet.x, bullet.y) < hitRadius * hitRadius) {
                    const float damage = bullet.enemyDmg > 0.0f
                        ? bullet.enemyDmg : 10.0f;
                    HurtPlayer(playerHP, damage *
                                         BalanceFor((int)g_Difficulty).rangedDmg,
                               "ENEMY_BULLET");
                    bullet.active = false;
                    playerHit = true;
                }
                continue;
            }

            if (bullet.synFlags & kSynNoHit) continue;   // a mortar shell lands, it does not hit on the way

            const float minX = std::min(bullet.prevX, bullet.x);
            const float maxX = std::max(bullet.prevX, bullet.x);
            const float minY = std::min(bullet.prevY, bullet.y);
            const float maxY = std::max(bullet.prevY, bullet.y);
            bool consumed = false;

            const int shotSpace = bullet.space >= 0 ? bullet.space : space::kPlayerSource;
            const float shotScale = std::max(1.0f, bullet.sizeScale);
            for (auto* monster : manager.monsters) {
                if (!monster->alive) continue;
                if (bullet.HasHit(monster)) continue;
                if (!space::Reaches(shotSpace, monster->worldX, monster->worldY)) continue;
                const float hitRadius = (monster->kind == MobKind::NORMA
                    ? norma::kShotHitPx : MONSTER_BULLET_HIT_RADIUS) * monster->sizeScale * shotScale;
                if (monster->worldX < minX - hitRadius || monster->worldX > maxX + hitRadius ||
                    monster->worldY < minY - hitRadius || monster->worldY > maxY + hitRadius)
                    continue;
                if (SegDistSq(monster->worldX, monster->worldY,
                              bullet.prevX, bullet.prevY, bullet.x, bullet.y) >=
                    hitRadius * hitRadius)
                    continue;

                bool crit = false;
                const float passiveMult = passive_hooks::bulletDamageMult
                    ? passive_hooks::bulletDamageMult(bullet, monster, monster->worldX,
                                                      monster->worldY, crit)
                    : 1.0f;
                const float baseDamage = stats.GetBaseDamage() *
                    stats.GetDamageMultiplier() * bullet.dmgMult * passiveMult;
                // Only a crit dents an elite (the shot's direction is the dent's).
                const float dealt = manager.ApplyDamage(*monster, baseDamage, shotSpace,
                                                        crit ? bullet.dirX : 0.0f,
                                                        crit ? bullet.dirY : 0.0f);
                SpawnDamageNumber(monster->worldX, monster->worldY, dealt, crit);
                if (monster->alive) Audio::Play(Audio::Cue::EnemyHit);
                const bool regular = !IsBossMob(monster->kind) && !IsEliteMob(monster->kind);
                if (monster->alive && passive_hooks::executes &&
                    passive_hooks::executes(monster, monster->hp, regular))
                    manager.ApplyDamage(*monster, monster->hp + 1.0f, shotSpace);
                if (monster->alive && !IsBossMob(monster->kind) && !IsEliteMob(monster->kind) &&
                    !monster->genesisThrown && monster->genesisRotorPhase == 0 &&
                    monster->thrownPhase == 0) {
                    monster->TryHitStun(bullet.dirX, bullet.dirY);
                }
                SpawnSparks(bullet.x, bullet.y, crit ? 6 : 3,
                            bullet.color.r, bullet.color.g, bullet.color.b);
                if (passive_hooks::bulletHit)
                    passive_hooks::bulletHit(bullet, monster, monster->worldX,
                                             monster->worldY, dealt, crit, !monster->alive);
                if (!monster->alive) CollectMonsterKill(rewards, *monster);

                if (bullet.SurvivesHit()) { bullet.RememberHit(monster); ++bullet.synPierced; }
                else if (passive_hooks::bulletRedirect && passive_hooks::bulletRedirect(bullet, monster)) {}
                else                 bullet.active = false;
                consumed = true;
                break;
            }

            if (consumed) continue;
            for (auto* ranged : manager.rangedMobs) {
                if (!ranged->alive) continue;
                if (bullet.HasHit(ranged)) continue;
                if (!space::Reaches(shotSpace, ranged->worldX, ranged->worldY)) continue;
                const float HIT_RADIUS = 20.0f * shotScale;
                if (ranged->worldX < minX - HIT_RADIUS || ranged->worldX > maxX + HIT_RADIUS ||
                    ranged->worldY < minY - HIT_RADIUS || ranged->worldY > maxY + HIT_RADIUS)
                    continue;
                if (SegDistSq(ranged->worldX, ranged->worldY,
                              bullet.prevX, bullet.prevY, bullet.x, bullet.y) >=
                    HIT_RADIUS * HIT_RADIUS)
                    continue;

                bool crit = false;
                const float passiveMult = passive_hooks::bulletDamageMult
                    ? passive_hooks::bulletDamageMult(bullet, ranged, ranged->worldX,
                                                      ranged->worldY, crit)
                    : 1.0f;
                const float baseDamage = stats.GetBaseDamage() *
                    stats.GetDamageMultiplier() * bullet.dmgMult * passiveMult;
                const float dealt = manager.ApplyDamage(*ranged, baseDamage, shotSpace);
                SpawnDamageNumber(ranged->worldX, ranged->worldY, dealt, crit);
                if (ranged->alive) Audio::Play(Audio::Cue::EnemyHit);
                if (ranged->alive && passive_hooks::executes &&
                    passive_hooks::executes(ranged, ranged->hp, true))
                    manager.ApplyDamage(*ranged, ranged->hp + 1.0f, shotSpace);
                if (ranged->alive) {
                    ranged->TryHitStun(bullet.dirX, bullet.dirY, Monster::HIT_STUN_TIME,
                                       Monster::HIT_STUN_COOLDOWN, Monster::HIT_KNOCK_SPEED);
                }
                SpawnSparks(bullet.x, bullet.y, crit ? 6 : 3,
                            bullet.color.r, bullet.color.g, bullet.color.b);
                if (passive_hooks::bulletHit)
                    passive_hooks::bulletHit(bullet, ranged, ranged->worldX,
                                             ranged->worldY, dealt, crit, !ranged->alive);
                if (!ranged->alive) CollectScopeKill(rewards, *ranged);

                if (bullet.SurvivesHit()) { bullet.RememberHit(ranged); ++bullet.synPierced; }
                else if (passive_hooks::bulletRedirect && passive_hooks::bulletRedirect(bullet, ranged)) {}
                else                 bullet.active = false;
                break;
            }
        }

        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                                     [](const Bullet& bullet) { return !bullet.active; }),
                      bullets.end());
        return playerHit;
    }
};
