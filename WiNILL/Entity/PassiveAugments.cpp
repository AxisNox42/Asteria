#include "PassiveAugments.h"
#include "AugmentCombat.h"
#include "ActiveSkills.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Bullet.h"
#include "GameManager.h"
#include "MonsterManager.h"
#include "PassiveHooks.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include "SpaceRules.h"
#include "Juice.h"
#include "SaveSystem.h"

extern GameManager         g_GameManager;
extern MonsterManager      g_MonsterManager;
extern std::vector<Bullet> g_Bullets;
extern PlayerStats         g_Stats;

namespace passive {

// Radii (world px) for "주변" and "넓은 반경" in the planning sheet.
constexpr float kNearRadius = 220.0f;
constexpr float kWideRadius = 380.0f;
constexpr float kHurtLock = 0.4f;      // one counted hit per window (DoT safety)

struct State {
    int count[AUG_TOTAL] = {};
    int lastPicked = -1;
    // Run report: cards taken (not stat cards), first-taken order and counts.
    std::vector<int> takenOrder;
    int taken[AUG_TOTAL] = {};
    // Breakage and charges.
    int amethystHits = 0, topazHits = 0, rubyHits = 0;
    int emeraldCharges = 0, cakeSlices = 0;
    bool phoenixUsed = false;
    bool hornsOffThisWave = false;
    bool firescaleOffThisWave = false;
    bool darkWingsOffThisWave = false;
    float canaryBonus = 0.0f;
    // Timers.
    float crescentT = 0.0f, floralT = 0.0f, headlampT = 0.0f;
    float stillT = 0.0f;
    bool shinobiFired = false, greavesFired = false;
    float shinobiCd = 0.0f, greavesCd = 0.0f, pickaxeCd = 0.0f;
    float hurtLock = 0.0f;
    float strongmanWindow = 0.0f;
    bool dashWasActive = false;
    // Counters.
    int shots = 0, crits = 0, kills = 0;
    int staticHits = 0, batteryHits = 0, blackboltHits = 0;
    bool fastNext = false;
    float cloudDist = 0.0f;
    std::deque<std::pair<float, float>> moveLog;   // (time, distance) of the last 3s
    float clock = 0.0f;
    bool hasLast = false;
    float lastPX = 0.0f, lastPY = 0.0f;
    bool procBusy = false;
    // Enemies seen so far: highest HP (for execute) and last position / velocity.
    struct Seen { float maxHp, x, y, vx, vy; };
    std::unordered_map<const void*, Seen> seen;
    StepInput in;
};
State s;

bool Has(AugType type) { return s.count[(int)type] > 0; }
float Rand01() { return (float)(std::rand() % 10000) / 10000.0f; }
bool Chance(float p) { return Rand01() < p * LuckChanceMult(); }

float AttackPower() { return g_Stats.GetBaseDamage() * g_Stats.GetDamageMultiplier(); }

// "모든 피해" multipliers.
float AllDamageMult() {
    float m = 1.0f;
    if (Has(AugType::P_FLAME_LIMIT_BREAK) && !s.hornsOffThisWave) m *= 1.30f;
    if (Has(AugType::P_GEM_UNSTABLE_AMP) && s.amethystHits > 0) m *= 1.30f;
    if (Has(AugType::P_HERESY_BOUND_AMP)) m *= 1.40f;
    if (Has(AugType::P_HERESY_SLOT_SACRIFICE)) m *= 1.35f;
    if (Has(AugType::P_VEIN_SKILL_SYNERGY))
        m *= 1.05f + 0.07f * (float)active::EquippedCount();
    if (Has(AugType::P_LIFE_EMERGENCY_MEND_SHARDS)) m *= 1.0f + 0.10f * (float)s.cakeSlices;
    m *= 1.0f + s.canaryBonus;
    return m;
}

// "탄환 피해" multipliers.
float BulletOnlyMult() {
    float m = 1.0f;
    if (Has(AugType::P_STONE_NEUTRON_WARHEAD)) m *= 1.40f;
    if (Has(AugType::P_STONE_GIANT_WARHEAD)) m *= 1.70f;
    if (Has(AugType::P_GALE_PARTICLE_ROUNDS)) m *= 0.45f;
    if (Has(AugType::P_GEM_GLASS_STAR) && s.rubyHits > 0) m *= 1.25f;
    if (Has(AugType::P_HUNTER_PROXIMITY_WARHEAD)) m *= 1.30f;
    return m;
}

float CraneBonus() {
    if (!Has(AugType::P_FLIGHT_MOMENTUM_AMPLIFIER) || !s.in.moving) return 0.0f;
    float dist = 0.0f;
    for (const auto& e : s.moveLog) dist += e.second;
    return std::min(0.60f, 0.60f * dist / 1000.0f);   // full at 1000 px in 3s
}

bool OnScreen(float x, float y) {
    return x >= s.in.viewL && x <= s.in.viewR && y >= s.in.viewT && y <= s.in.viewB;
}

void Ring(float x, float y, float radius, float r, float g, float b) {
    if (g_Fx.shockwave) g_Fx.shockwave(x, y, radius, 0.35f, r, g, b);
}

float RollCrit(bool& crit) {
    crit = g_Stats.critChance > 0 && (std::rand() % 100) < g_Stats.critChance;
    return crit ? g_Stats.critMult : 1.0f;
}

// pct of attack to every enemy on screen. canCrit: the hit may crit.
void DamageAll(float pct, float r, float g, float b, bool canCrit, float extra) {
    const float base = AttackPower() * pct / 100.0f * AllDamageMult() * extra;
    auto hit = [&](auto& target) {
        if (!target.alive || !OnScreen(target.worldX, target.worldY)) return;
        bool crit = false;
        const float mult = canCrit ? RollCrit(crit) : 1.0f;
        const float dealt = g_MonsterManager.ApplyDamage(target, base * mult);
        if (dealt <= 0.0f) return;
        SpawnDamageNumber(target.worldX, target.worldY, dealt, crit);
        SpawnSparks(target.worldX, target.worldY, 4, r, g, b, 260.0f);
    };
    for (auto* m : g_MonsterManager.monsters) hit(*m);
    for (auto* rm : g_MonsterManager.rangedMobs) hit(*rm);
    TriggerFlash(r, g, b, 0.10f);
    // Small, frequent procs crackle; big ones land as a blast.
    Audio::Play(pct >= 500.0f ? Audio::Cue::ExplodeL
              : pct >= 150.0f ? Audio::Cue::Lightning : Audio::Cue::FieldPulse);
}

// pct of attack to every enemy within radius of (x, y).
void DamageRadius(float x, float y, float radius, float pct,
                  float r, float g, float b, float extra) {
    const float dmg = AttackPower() * pct / 100.0f * AllDamageMult() * extra;
    auto hit = [&](auto& target) {
        if (!target.alive) return;
        const float dx = target.worldX - x, dy = target.worldY - y;
        if (dx * dx + dy * dy > radius * radius) return;
        const float dealt = g_MonsterManager.ApplyDamage(target, dmg);
        if (dealt > 0.0f) SpawnDamageNumber(target.worldX, target.worldY, dealt, false);
    };
    for (auto* m : g_MonsterManager.monsters) hit(*m);
    for (auto* rm : g_MonsterManager.rangedMobs) hit(*rm);
    SpawnEnemyExplosion(x, y, r, g, b, true);
    Ring(x, y, radius, r, g, b);
    Audio::Play(radius >= 250.0f ? Audio::Cue::ExplodeL
              : radius >= 170.0f ? Audio::Cue::ExplodeM : Audio::Cue::ExplodeS);
}

// Clears enemy bullets within radius of (x, y); radius < 0 clears them all.
int ClearEnemyBullets(float x, float y, float radius) {
    int cleared = 0;
    for (Bullet& b : g_Bullets) {
        if (!b.active || !b.isEnemy) continue;
        if (radius >= 0.0f) {
            const float dx = b.x - x, dy = b.y - y;
            if (dx * dx + dy * dy > radius * radius) continue;
        }
        b.active = false;
        ++cleared;
        SpawnSparks(b.x, b.y, 2, 0.75f, 0.95f, 1.0f, 140.0f);
    }
    if (radius > 0.0f) Ring(x, y, radius, 0.75f, 0.95f, 1.0f);
    return cleared;
}

void AddStardust(long long n) {
    if (n <= 0 || !PlayEarnsStardust()) return;
    AddSaveValue(g_Coins, n);
    AddSaveValue(g_RunStardust, n);
    g_StardustHudPulse = 1.0f;
}

void ResetDash() {
    g_PlayerRuntime.dashCooldown = 0.0f;
}

// Another chance augment triggered: Crown of Storms adds its lightning.
void ChanceTriggered() {
    if (!Has(AugType::P_STORM_CASCADE_COLLISION) || s.procBusy) return;
    s.procBusy = true;
    DamageAll(400.0f, 0.65f, 0.80f, 1.0f);
    s.procBusy = false;
}

// One hit for the spark counters (bullets and extra strikes).
void CountHit() {
    if (Has(AugType::P_SPARK_STATIC_BUILDUP) && ++s.staticHits >= 10) {
        s.staticHits = 0;
        DamageAll(200.0f, 0.55f, 0.85f, 1.0f);
    }
    if (Has(AugType::P_SPARK_STELLAR_DYNAMO) && ++s.batteryHits >= 30) {
        s.batteryHits = 0;
        ResetDash();
    }
    if (Has(AugType::P_SPARK_CRITICAL_BURST) && ++s.blackboltHits >= 50) {
        s.blackboltHits = 0;
        DamageAll(2400.0f, 0.45f, 0.30f, 0.85f);
    }
}

// Triple Strike: three strikes on enemies around (x, y).
void ExtraStrikes(float x, float y) {
    constexpr float kRadius = 150.0f;
    std::vector<Monster*> mobs;
    std::vector<RangedMob*> scopes;
    for (auto* m : g_MonsterManager.monsters) {
        const float dx = m->worldX - x, dy = m->worldY - y;
        if (m->alive && dx * dx + dy * dy <= kRadius * kRadius) mobs.push_back(m);
    }
    for (auto* r : g_MonsterManager.rangedMobs) {
        const float dx = r->worldX - x, dy = r->worldY - y;
        if (r->alive && dx * dx + dy * dy <= kRadius * kRadius) scopes.push_back(r);
    }
    const int total = (int)(mobs.size() + scopes.size());
    if (total == 0) return;
    float dmg = AttackPower() * AllDamageMult();
    if (Has(AugType::P_SHADOW_MULTI_HIT_AMP)) dmg *= 1.30f;
    for (int i = 0; i < 3; ++i) {
        const int pick = std::rand() % total;
        float tx, ty, dealt;
        if (pick < (int)mobs.size()) {
            Monster& m = *mobs[pick];
            if (!m.alive) continue;
            dealt = g_MonsterManager.ApplyDamage(m, dmg);
            tx = m.worldX; ty = m.worldY;
        } else {
            RangedMob& r = *scopes[pick - mobs.size()];
            if (!r.alive) continue;
            dealt = g_MonsterManager.ApplyDamage(r, dmg);
            tx = r.worldX; ty = r.worldY;
        }
        if (dealt <= 0.0f) continue;
        SpawnDamageNumber(tx, ty, dealt, false);
        SpawnSparks(tx, ty, 4, 0.55f, 0.45f, 0.95f, 240.0f);
        CountHit();
    }
}

Bullet MakeShot(float tx, float ty) {
    Bullet b(s.in.px, s.in.py, tx, ty);
    b.speed = g_Stats.bulletSpeed;
    return b;
}

// ── hooks ──
float HookBulletDamageMult(const Bullet& bullet, const void* target,
                           float tx, float ty, bool& crit) {
    if (bullet.skillShot) {
        crit = bullet.forceCrit;
        return AllDamageMult() * active::DamageMult() * (crit ? g_Stats.critMult : 1.0f);
    }
    float m = AllDamageMult() * BulletOnlyMult();
    if (Has(AugType::P_EDGE_LONG_RANGE_CORRECTION)) {
        const float dx = tx - s.in.px, dy = ty - s.in.py;
        if (std::sqrt(dx * dx + dy * dy) * s.in.viewZoom >= 0.40f * s.in.screenW) m *= 1.30f;
    }
    m *= 1.0f + CraneBonus();
    (void)bullet;
    (void)target;
    return m * RollCrit(crit);
}

void HookBulletHit(const Bullet& bullet, const void*, float x, float y,
                   float, bool crit, bool killed) {
    CountHit();
    if (bullet.skillShot) {
        active::OnSkillBulletHit(bullet, x, y, killed);
        return;
    }
    if (crit && Has(AugType::P_EDGE_CRITICAL_CHAIN_BURST) && ++s.crits >= 6) {
        s.crits = 0;
        DamageAll(500.0f, 1.0f, 0.85f, 0.45f, true);
    }
    if (Has(AugType::P_SHADOW_TRIPLE_STRIKE) && Chance(0.10f)) {
        ExtraStrikes(x, y);
        ChanceTriggered();
    }
    if (Has(AugType::P_STORM_CORONA_ROUND) && Chance(0.03f)) {
        DamageAll(200.0f, 0.65f, 0.80f, 1.0f);
        ChanceTriggered();
    }
    if (Has(AugType::P_VEIN_CRATER_ROUND) && s.pickaxeCd <= 0.0f && Chance(0.20f)) {
        s.pickaxeCd = 5.0f;
        DamageRadius(x, y, 110.0f, 600.0f, 1.0f, 0.70f, 0.30f);
        ChanceTriggered();
    }
    if (bullet.blastOnHit) DamageRadius(x, y, 120.0f, 400.0f, 1.0f, 0.55f, 0.25f);
}

bool HookExecutes(const void* target, float hp, bool regular) {
    if (!regular || !Has(AugType::P_HUNTER_WEAK_POINT_EXECUTION)) return false;
    auto it = s.seen.find(target);
    return it != s.seen.end() && hp <= it->second.maxHp * 0.15f;
}

void HookKill() {
    ++s.kills;
    active::OnKill();
    if (Has(AugType::P_NOVA_AFTERGLOW_RECOVERY)) active::ReduceAllCooldowns(0.2f);
    if (Has(AugType::P_NOVA_STARDUST_HARVEST) && Chance(0.05f)) {
        AddStardust(1);
        ChanceTriggered();
    }
    if (Has(AugType::P_NOVA_STELLAR_CORE_RUPTURE) && s.kills % 20 == 0) {
        DamageAll(1500.0f, 1.0f, 0.95f, 0.75f);
        ClearEnemyBullets(0.0f, 0.0f, -1.0f);
    }
}

void HookHurt(float& hp, float dmg) {
    if (s.in.damageProtected) { hp -= dmg; return; }   // undone this step anyway
    active::RecordDamage(dmg);
    if (Has(AugType::P_STILL_STILLNESS_PLATING) && s.stillT >= 2.0f) dmg *= 0.75f;
    const bool counts = s.hurtLock <= 0.0f;
    if (Has(AugType::P_GEM_BUFFER_PLATING) && s.emeraldCharges > 0) {
        if (counts) {
            --s.emeraldCharges;
            s.hurtLock = kHurtLock;
        }
        return;                                         // the plate takes the hit
    }
    hp -= dmg;
    if (!counts) return;
    s.hurtLock = kHurtLock;
    if (s.amethystHits > 0) --s.amethystHits;
    if (s.topazHits > 0) --s.topazHits;
    if (s.rubyHits > 0) --s.rubyHits;
    if (Has(AugType::P_LIFE_EMERGENCY_MEND_SHARDS) && s.cakeSlices > 0) {
        --s.cakeSlices;
        hp = std::min(g_Stats.maxHP, hp + 30.0f);
    }
}

void TrackEnemies() {
    std::unordered_map<const void*, State::Seen> next;
    next.reserve(s.seen.size() + 16);
    const float dt = std::max(1e-4f, s.in.dt);
    auto track = [&](const void* key, float x, float y, float hp) {
        auto it = s.seen.find(key);
        State::Seen e{ hp, x, y, 0.0f, 0.0f };
        if (it != s.seen.end()) {
            e.maxHp = std::max(it->second.maxHp, hp);
            e.vx = (x - it->second.x) / dt;
            e.vy = (y - it->second.y) / dt;
        }
        next[key] = e;
    };
    for (auto* m : g_MonsterManager.monsters)
        if (m->alive) track(m, m->worldX, m->worldY, m->hp);
    for (auto* r : g_MonsterManager.rangedMobs)
        if (r->alive) track(r, r->worldX, r->worldY, r->hp);
    s.seen.swap(next);
}

bool RubyIntact() { return Has(AugType::P_GEM_GLASS_STAR) && s.rubyHits > 0; }
bool FirescaleOn() { return Has(AugType::P_FLAME_STELLAR_OVERHEAT) && !s.firescaleOffThisWave; }
bool DarkWingsOn() { return Has(AugType::P_HERESY_UNSTABLE_DRIVE) && !s.darkWingsOffThisWave; }
void SwitchOffDarkWings() { s.darkWingsOffThisWave = true; }

void InstallHooks() {
    passive_hooks::bulletDamageMult = HookBulletDamageMult;
    passive_hooks::bulletHit = HookBulletHit;
    passive_hooks::executes = HookExecutes;
    passive_hooks::kill = HookKill;
    passive_hooks::hurt = HookHurt;
}

void Reset() {
    s = State{};
    active::Reset();
}

int Owned(AugType type) { return s.count[(int)type]; }

int Luck() {
    int luck = 0;
    if (Has(AugType::P_LUCK_LUCKY_STAR))
        luck += g_RunStardust >= 50 ? 3 : g_RunStardust >= 20 ? 1 : 0;
    return luck;
}

float LuckChanceMult() { return 1.0f + 0.05f * (float)Luck(); }

bool CanOffer(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL) return false;
    const AugDef& d = ALL_AUGS[augIdx];
    if (AugIsActive(d)) return active::CanOffer(augIdx);
    if (!d.stackable && s.count[augIdx] > 0) return false;
    if (d.type == AugType::P_PERFORM_AUGMENT_REPLICATOR) {
        if (s.lastPicked < 0) return false;
        const AugDef& last = ALL_AUGS[s.lastPicked];
        return last.stackable && last.rarity != AugRarity::MYTHIC;
    }
    return true;
}

static void ApplyCard(int augIdx) {
    ++s.count[augIdx];
    PlayerStats& st = g_Stats;
    float& hp = g_GameManager.playerHP;
    switch (ALL_AUGS[augIdx].type) {
    case AugType::P_GALE_FIRE_RATE_UP:     st.fireInterval /= 1.06f; break;
    case AugType::P_STONE_ATTACK_UP:     st.baseDamage += 8.0f; break;
    case AugType::P_LIFE_MAX_HP_UP:         st.maxHP += 15.0f; hp += 15.0f; break;
    case AugType::P_LIFE_REGENERATION:       st.regenPerSec += 0.2f; break;
    case AugType::P_FLIGHT_MOVE_SPEED_UP:      st.moveSpeedMult += 0.10f; break;
    case AugType::P_EDGE_CRITICAL_STRIKE:         st.critChance = std::min(100, st.critChance + 8); break;
    case AugType::P_EDGE_CRITICAL_AMP:      st.critMult += 0.5f; break;
    case AugType::P_CHRONO_STAR_TEMPO: st.moveSpeedMult += 0.08f; break;
    case AugType::P_STONE_NEUTRON_WARHEAD: st.moveSpeedMult -= 0.08f; break;
    case AugType::P_STONE_GIANT_WARHEAD: st.moveSpeedMult -= 0.10f; break;
    case AugType::P_HERESY_UNSTABLE_DRIVE:      st.moveSpeedMult += 0.25f; break;
    case AugType::P_STILL_SEA_OF_TRANQUILITY:     st.moveSpeedMult -= 0.08f; break;
    case AugType::P_LIFE_GRAZE_RECOVERY:   st.maxHP += 10.0f; hp += 10.0f; break;
    case AugType::P_LUCK_STARDUST_SUPPLY:       AddStardust(30); break;
    case AugType::P_GEM_UNSTABLE_AMP:  s.amethystHits = 3; break;
    case AugType::P_LUCK_GIFT_OF_STARS:       s.topazHits = 3; break;
    case AugType::P_GEM_GLASS_STAR:       s.rubyHits = 1; break;
    case AugType::P_GEM_BUFFER_PLATING:      s.emeraldCharges = 3; break;
    case AugType::P_LIFE_EMERGENCY_MEND_SHARDS:   s.cakeSlices = 2; break;
    case AugType::P_HERESY_SLOT_SACRIFICE:  active::SealSlot(); break;
    // Level-up stat cards.
    case AugType::S_ATTACK:     st.baseDamage += 6.0f; break;
    case AugType::S_MAX_HP:     st.maxHP += 12.0f; hp += 12.0f; break;
    case AugType::S_FIRE_RATE:  st.fireInterval /= 1.05f; break;
    case AugType::S_MOVE_SPEED: st.moveSpeedMult += 0.05f; break;
    case AugType::S_REGEN:      st.regenPerSec += 0.15f; break;
    case AugType::S_CRIT:       st.critChance = std::min(100, st.critChance + 4); break;
    default: break;
    }
    st.moveSpeedMult = std::max(0.30f, st.moveSpeedMult);
    hp = std::min(hp, st.maxHP);
    g_GameManager.maxHP = st.maxHP;
}

static void RecordTaken(int augIdx) {
    if (AugIsStatCard(ALL_AUGS[augIdx])) return;
    if (s.taken[augIdx]++ == 0) s.takenOrder.push_back(augIdx);
}

const std::vector<int>& AcquiredOrder() { return s.takenOrder; }
int AcquiredCount(int augIdx) {
    return augIdx >= 0 && augIdx < AUG_TOTAL ? s.taken[augIdx] : 0;
}

void Acquire(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL) return;
    RecordTaken(augIdx);
    if (AugIsActive(ALL_AUGS[augIdx])) {
        active::Acquire(augIdx);
        return;
    }
    if (ALL_AUGS[augIdx].type == AugType::P_PERFORM_AUGMENT_REPLICATOR && s.lastPicked >= 0)
    {
        RecordTaken(s.lastPicked);
        ApplyCard(s.lastPicked);   // the previous card once more
    }
    ApplyCard(augIdx);
    if (!AugIsStatCard(ALL_AUGS[augIdx])) s.lastPicked = augIdx;
}

void Step(const StepInput& in) {
    s.in = in;
    const float dt = in.dt;
    s.clock += dt;
    s.hurtLock = std::max(0.0f, s.hurtLock - dt);
    s.pickaxeCd = std::max(0.0f, s.pickaxeCd - dt);
    s.shinobiCd = std::max(0.0f, s.shinobiCd - dt);
    s.greavesCd = std::max(0.0f, s.greavesCd - dt);
    s.strongmanWindow = std::max(0.0f, s.strongmanWindow - dt);

    TrackEnemies();

    // Movement: light-years (1 ly = a quarter of the screen width) and the 3s log.
    float moved = 0.0f;
    if (s.hasLast) moved = std::hypot(in.px - s.lastPX, in.py - s.lastPY);
    s.lastPX = in.px; s.lastPY = in.py; s.hasLast = true;
    s.moveLog.push_back({ s.clock, moved });
    while (!s.moveLog.empty() && s.moveLog.front().first < s.clock - 3.0f) s.moveLog.pop_front();
    if (Has(AugType::P_FLIGHT_ROUTE_SWEEP)) {
        const float lightYear = in.screenW * 0.25f / std::max(0.01f, in.viewZoom);
        s.cloudDist += moved;
        if (s.cloudDist >= 10.0f * lightYear) {
            s.cloudDist -= 10.0f * lightYear;
            ClearEnemyBullets(in.px, in.py, kNearRadius);
        }
    }

    // Standing still.
    if (in.moving) {
        s.stillT = 0.0f;
        s.shinobiFired = s.greavesFired = false;
    } else {
        s.stillT += dt;
    }
    if (Has(AugType::P_SHADOW_QUIET_ORBIT) && s.stillT >= 1.0f &&
        !s.shinobiFired && s.shinobiCd <= 0.0f) {
        s.shinobiFired = true;
        s.shinobiCd = 10.0f;
        ClearEnemyBullets(in.px, in.py, kNearRadius);
        active::ResetLongestCooldown();
    }
    if (Has(AugType::P_STILL_SEA_OF_TRANQUILITY) && s.stillT >= 1.0f &&
        !s.greavesFired && s.greavesCd <= 0.0f) {
        s.greavesFired = true;
        s.greavesCd = 12.0f;
        ClearEnemyBullets(in.px, in.py, kWideRadius);
        ResetDash();
    }

    // Periodic shots.
    if (Has(AugType::P_NIGHT_PULSE_ROUND) && (s.crescentT += dt) >= 10.0f) {
        s.crescentT = 0.0f;
        Bullet b = MakeShot(in.aimX, in.aimY);
        b.dmgMult = 9.0f;
        b.piercing = true;
        b.sizeScale = 1.8f;
        b.speed = g_Stats.bulletSpeed * 1.1f;
        b.color = glm::vec3(0.75f, 0.70f, 1.0f);
        g_Bullets.push_back(b);
    }
    if (Has(AugType::P_STILL_STATIONARY_SATELLITE) && !in.moving && (s.floralT += dt) >= 6.0f) {
        float tx = 0.0f, ty = 0.0f;
        if (g_MonsterManager.FindNearestEnemy(in.px, in.py, tx, ty)) {
            s.floralT = 0.0f;
            Bullet b = MakeShot(tx, ty);
            b.dmgMult = 5.0f;
            b.homing = true;
            b.homingTurn = 6.0f;
            b.sizeScale = 1.3f;
            b.color = glm::vec3(1.0f, 0.60f, 0.80f);
            g_Bullets.push_back(b);
        }
    }
    if (Has(AugType::P_VEIN_CIRCULATING_COOLER) && (s.headlampT += dt) >= 6.0f) {
        s.headlampT -= 6.0f;
        g_PlayerRuntime.dashCooldown = std::max(0.0f, g_PlayerRuntime.dashCooldown - 2.0f);
        active::ReduceAllCooldowns(2.0f);
    }

    // Rush Shot: the window opens when a dash ends.
    if (s.dashWasActive && !in.dashActive && Has(AugType::P_PERFORM_RUSH_SHOT))
        s.strongmanWindow = 0.5f;
    s.dashWasActive = in.dashActive;

    // Graze Recovery: grazes (just outside the hit radius, once per bullet).
    if (Has(AugType::P_LIFE_GRAZE_RECOVERY)) {
        const float hitR = 10.0f * g_Stats.playerSizeMult;
        for (Bullet& b : g_Bullets) {
            if (!b.active || !b.isEnemy || b.grazed) continue;
            const float d = std::hypot(b.x - in.px, b.y - in.py);
            if (d > hitR && d < hitR + 18.0f) {
                b.grazed = true;
                g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 0.5f);
            }
        }
    }
}

float FireInterval(float base) {
    float interval = base;
    if (Has(AugType::P_CHRONO_STAR_TEMPO)) {
        interval = 0.18f;   // fixed: other fire-rate cards do not apply
    } else {
        if (Has(AugType::P_GALE_PARTICLE_ROUNDS)) interval *= 0.5f;
        if (Has(AugType::P_STONE_GIANT_WARHEAD)) interval += 0.08f;
    }
    if (s.fastNext) interval *= 0.6f;
    return std::max(0.03f, interval);
}

void ModifyShot(Bullet& bullet) {
    if (Has(AugType::P_STONE_GIANT_WARHEAD)) bullet.sizeScale *= 1.3f;
    if (Has(AugType::P_HUNTER_PROXIMITY_WARHEAD))
        bullet.maxRange = 0.8f * s.in.screenW / std::max(0.01f, s.in.viewZoom);
    if (s.strongmanWindow > 0.0f) {
        bullet.blastOnHit = true;   // the first shot after the dash only
        s.strongmanWindow = 0.0f;
    }
}

void OnShot() {
    ++s.shots;
    if (Has(AugType::P_GALE_SCAN_PULSE) && s.shots % 10 == 0)
        DamageAll(50.0f, 0.80f, 0.95f, 0.85f);
    // Shinsoku's chance does not feed Crown of Storms: it would fire on half
    // of all shots.
    s.fastNext = Has(AugType::P_GALE_VARIABLE_FIRE) && Chance(0.50f);
}

bool DashSealed() { return Has(AugType::P_HERESY_BOUND_AMP); }

void OnDashStarted() {
    float cd = DASH_CD;
    if (Has(AugType::P_ABYSS_ERRATIC_ORBIT)) cd = 1.0f + 5.0f * Rand01();
    if (Has(AugType::P_NIGHT_DWARF_COOLING)) cd -= 1.0f;
    if (DarkWingsOn()) cd -= 2.0f;
    cd = std::max(0.4f, cd);
    g_PlayerRuntime.dashCooldown = cd;
    g_PlayerRuntime.dashCooldownMax = cd;
    if (Has(AugType::P_FLAME_LIMIT_BREAK)) s.hornsOffThisWave = true;
    if (Has(AugType::P_FLAME_STELLAR_OVERHEAT)) s.firescaleOffThisWave = true;
    active::OnDash();
    if (Has(AugType::P_CHRONO_DOUBLE_CAST) && Chance(0.30f)) {
        ResetDash();
        ChanceTriggered();
    }
    if (Has(AugType::P_STORM_MAGNETIC_STORM) && Chance(0.20f)) {
        DamageAll(400.0f, 0.65f, 0.80f, 1.0f);
        ChanceTriggered();
    }
}

void OnWaveCleared() {
    if (Has(AugType::P_VEIN_GROWING_STAR))
        s.canaryBonus = std::min(0.30f, s.canaryBonus + 0.02f);
    if (Has(AugType::P_LUCK_GIFT_OF_STARS) && s.topazHits > 0) AddStardust(8);
    s.hornsOffThisWave = false;
    s.firescaleOffThisWave = false;
    s.darkWingsOffThisWave = false;
    active::OnWaveCleared();
}

bool TryPhoenix() {
    if (!Has(AugType::P_FLAME_UNDYING_REIGNITION) || s.phoenixUsed) return false;
    s.phoenixUsed = true;
    g_GameManager.playerHP = g_Stats.maxHP;
    DamageAll(1000.0f, 1.0f, 0.55f, 0.20f);
    Ring(s.in.px, s.in.py, 520.0f, 1.0f, 0.55f, 0.20f);
    return true;
}

}  // namespace passive
