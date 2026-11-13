#include "PassiveAugments.h"
#include "AugmentCombat.h"
#include "RangeUnit.h"
#include "ActiveSkills.h"
#include "SynergyRuntime.h"

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
#include "StrikeFx.h"
#include "SaveSystem.h"

extern GameManager         g_GameManager;
extern MonsterManager      g_MonsterManager;
extern std::vector<Bullet> g_Bullets;
extern PlayerStats         g_Stats;

namespace passive {

// Radii (world px) for "주변" and "넓은 반경" in the planning sheet.
constexpr float kNearRadius = Rings(3);
constexpr float kWideRadius = Rings(5);
// Hit effects count damage, not hits: a tick of 0.1 must not use up a charge.
constexpr float kPlateAbsorb = 30.0f;   // Buffer Plating: damage it blocks in total
constexpr float kAmethystLife = 30.0f;  // Unstable Amp / Gift of Stars break after this much damage
constexpr float kRubyLife = 10.0f;      // Glass Star breaks after this much damage
constexpr float kShardEvery = 20.0f;    // Emergency Mend Shards: one shard per this much damage

struct State {
    int count[AUG_TOTAL] = {};
    int lastPicked = -1;
    // Run report: cards taken (not stat cards), first-taken order and counts.
    std::vector<int> takenOrder;
    int taken[AUG_TOTAL] = {};
    // Breakage and charges.
    float amethystLeft = 0.0f, topazLeft = 0.0f, rubyLeft = 0.0f;   // damage left before they break
    float emeraldLeft = 0.0f;                                        // damage the plating can still block
    int   cakeSlices = 0;
    float cakeDamage = 0.0f;                                         // damage since the last shard
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
    float strongmanWindow = 0.0f;
    // Rush Shot's one blast, owed after the first shot of a dash window. Copies of
    // that shot (pierce, spread pellets, splits, echoes, orbiters) carry its flag,
    // so the flag alone would blast on every hit; this makes it the first hit only.
    // Expires so a shot that missed cannot blast seconds later.
    float rushOwed = 0.0f;
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
    // Children of split shots, added to g_Bullets next step (the hit hook runs
    // while the collision pass is walking that vector).
    std::vector<Bullet> pendingSplits;
    // Hits carried by something still in flight (성단 폭격's stars): each lands
    // on its enemy when the star reaches it, not at the cast.
    struct LandingHit {
        const void* target = nullptr;   // compared only; looked up again before use
        bool ranged = false;
        float dmg = 0.0f;
        bool crit = false;
        float t = 0.0f;                 // seconds until it lands
        float r = 1.0f, g = 1.0f, b = 1.0f;
    };
    std::vector<LandingHit> landing;
};
State s;

bool Has(AugType type) { return s.count[(int)type] > 0; }
// MSVC's RAND_MAX is 32767: `rand() % 10000` only reached 0..0.33 and chances fired ~3x too often.
float Rand01() { return (float)std::rand() / ((float)RAND_MAX + 1.0f); }
bool Chance(float p) { return Rand01() < p * LuckChanceMult(); }

float AttackPower() { return g_Stats.GetBaseDamage() * g_Stats.GetDamageMultiplier(); }

// "모든 피해" multipliers.
float AllDamageMult() {
    float m = 1.0f;
    if (Has(AugType::P_FLAME_LIMIT_BREAK) && !s.hornsOffThisWave) m *= 1.30f;
    if (Has(AugType::P_GEM_UNSTABLE_AMP) && s.amethystLeft > 0.0f) m *= 1.30f;
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
    if (Has(AugType::P_STONE_GIANT_WARHEAD)) m *= 1.80f;
    if (Has(AugType::P_GALE_PARTICLE_ROUNDS)) m *= 0.60f;
    if (Has(AugType::P_GEM_GLASS_STAR) && s.rubyLeft > 0.0f) m *= 1.25f;
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

float RollCrit(bool& crit) {
    crit = g_Stats.critChance > 0 && (std::rand() % 100) < g_Stats.critChance;
    return crit ? g_Stats.critMult : 1.0f;
}

// What each enemy hit by a screen-wide blow sees (fx), in hit order.
void ShowScreenHits(ScreenFx fx, const std::vector<std::pair<float, float>>& at, line_fx::Rgb c) {
    if (fx == ScreenFx::Sparks || at.empty()) return;
    const float px = s.in.px, py = s.in.py;
    const float vw = s.in.viewR - s.in.viewL, vh = s.in.viewB - s.in.viewT;
    const float reach = std::sqrt(vw * vw + vh * vh);
    auto stagger = [](int i, float step) { return std::min(0.35f, (float)i * step); };
    switch (fx) {
    case ScreenFx::Zap: {   // lightning out of the ship; only the nearest few get a bolt
        constexpr int kBolts = 8;   // more would bury the ship (and incoming shots) in lines
        std::vector<std::pair<float, int>> order;
        for (int i = 0; i < (int)at.size(); ++i) {
            const float dx = at[i].first - px, dy = at[i].second - py;
            order.push_back({ dx * dx + dy * dy, i });
        }
        std::sort(order.begin(), order.end());
        for (int k = 0; k < (int)order.size(); ++k) {
            const auto& p = at[order[k].second];
            if (k < kBolts) strike_fx::Zap(px, py, p.first, p.second, c, 2.2f, stagger(k, 0.012f), 40.0f);
            else            strike_fx::Blast(p.first, p.second, 40.0f, c, stagger(k, 0.012f), strike_fx::BlastStyle::Discharge);
        }
        break;
    }
    case ScreenFx::BlackBolt:   // 임계 대폭발: thick dark-violet bolts out of the ship, a wide blast under each
        for (int i = 0; i < (int)at.size(); ++i)
            strike_fx::Zap(px, py, at[i].first, at[i].second, c, 4.2f, stagger(i, 0.015f), 90.0f);
        strike_fx::Nova(px, py, reach * 0.5f, c);
        break;
    case ScreenFx::CritChain: {   // 치명 연쇄 폭발: one bolt hopping enemy to enemy from the ship
        std::vector<std::pair<float, float>> left = at, path;
        float cx = px, cy = py;
        while (!left.empty()) {
            size_t best = 0; float bd = 1e18f;
            for (size_t k = 0; k < left.size(); ++k) {
                const float dx = left[k].first - cx, dy = left[k].second - cy, d = dx * dx + dy * dy;
                if (d < bd) { bd = d; best = k; }
            }
            path.push_back(left[best]);
            cx = left[best].first; cy = left[best].second;
            left.erase(left.begin() + (std::ptrdiff_t)best);
        }
        cx = px; cy = py;
        for (int i = 0; i < (int)path.size(); ++i) {
            const float d = stagger(i, 0.035f);
            strike_fx::Bolt(cx, cy, path[i].first, path[i].second, c, 2.6f, d, 0.34f);
            strike_fx::Blast(path[i].first, path[i].second, 60.0f, c, d, strike_fx::BlastStyle::Discharge);
            cx = path[i].first; cy = path[i].second;
        }
        break;
    }
    case ScreenFx::StarFall:   // 성단 폭격: DamageAllStarFall launches the stars itself
        break;
    case ScreenFx::Scan: {   // 스캔 펄스: a radar line turns once; each enemy pings as it passes
        constexpr float kTurn = 0.45f;
        strike_fx::Sweep(px, py, reach * 0.55f, c, kTurn);
        for (const auto& p : at) {
            float a = std::atan2(p.second - py, p.first - px) + line_fx::kPi * 0.5f;   // 0 = straight up, clockwise
            while (a < 0.0f) a += line_fx::kTwoPi;
            strike_fx::Blast(p.first, p.second, 26.0f, c, kTurn * a / line_fx::kTwoPi, strike_fx::BlastStyle::Stellar);
        }
        break;
    }
    case ScreenFx::Nova: {   // a ring sweeps out from the ship; each enemy bursts as it passes
        strike_fx::Nova(px, py, reach, c);
        for (const auto& p : at) {
            const float dx = p.first - px, dy = p.second - py;
            const float k = std::min(1.0f, std::sqrt(dx * dx + dy * dy) / reach);
            strike_fx::Blast(p.first, p.second, 70.0f, c, 0.70f * (1.0f - std::sqrt(1.0f - k)), strike_fx::BlastStyle::Stellar);
        }
        break;
    }
    default: break;
    }
}

// pct of attack to every enemy on screen. canCrit: the hit may crit.
// 성단 폭격: the ship launches a star at every enemy on screen, fanning out on
// curves; each enemy takes its hit when its star lands (StepLandingHits).
void DamageAllStarFall(float base, float r, float g, float b, bool canCrit) {
    const float px = s.in.px, py = s.in.py;
    int i = 0;
    auto launch = [&](auto& target, bool ranged) {
        if (!target.alive || !OnScreen(target.worldX, target.worldY)) return;
        const float dx = target.worldX - px, dy = target.worldY - py;
        const float bend = ((i & 1) ? 1.0f : -1.0f) * std::min(110.0f, 0.28f * std::sqrt(dx * dx + dy * dy));
        const float delay = std::min(0.35f, (float)i * 0.025f);
        State::LandingHit h;
        h.target = &target; h.ranged = ranged;
        const float mult = canCrit ? RollCrit(h.crit) : 1.0f;
        h.dmg = base * mult;
        h.t = strike_fx::Star(px, py, target.worldX, target.worldY, { r, g, b }, delay, bend, 80.0f);
        h.r = r; h.g = g; h.b = b;
        s.landing.push_back(h);
        ++i;
    };
    for (auto* m : g_MonsterManager.monsters) launch(*m, false);
    for (auto* rm : g_MonsterManager.rangedMobs) launch(*rm, true);
}

// Lands the hits whose stars arrived this step. The target is looked up in the
// live lists first: it may have died (and been freed) while the star flew.
void StepLandingHits(float dt) {
    if (s.landing.empty()) return;
    for (auto& h : s.landing) {
        h.t -= dt;
        if (h.t > 0.0f) continue;
        auto land = [&](auto& target) {
            if (!target.alive) return;
            const float dealt = g_MonsterManager.ApplyDamage(target, h.dmg);
            if (dealt <= 0.0f) return;
            SpawnDamageNumber(target.worldX, target.worldY, dealt, h.crit);
            SpawnSparks(target.worldX, target.worldY, 4, h.r, h.g, h.b, 260.0f);
        };
        if (h.ranged) {
            for (auto* rm : g_MonsterManager.rangedMobs) if (rm == h.target) { land(*rm); break; }
        } else {
            for (auto* m : g_MonsterManager.monsters) if (m == h.target) { land(*m); break; }
        }
    }
    s.landing.erase(std::remove_if(s.landing.begin(), s.landing.end(),
        [](const State::LandingHit& h) { return h.t <= 0.0f; }), s.landing.end());
}

void DamageAll(float pct, float r, float g, float b, bool canCrit, float extra, ScreenFx fx) {
    const float base = AttackPower() * pct / 100.0f * AllDamageMult() * extra;
    if (fx == ScreenFx::StarFall) {
        DamageAllStarFall(base, r, g, b, canCrit);
        Audio::Play(Audio::Cue::ExplodeL);
        return;
    }
    std::vector<std::pair<float, float>> at;
    auto hit = [&](auto& target) {
        if (!target.alive || !OnScreen(target.worldX, target.worldY)) return;
        bool crit = false;
        const float mult = canCrit ? RollCrit(crit) : 1.0f;
        const float dealt = g_MonsterManager.ApplyDamage(target, base * mult);
        if (dealt <= 0.0f) return;
        SpawnDamageNumber(target.worldX, target.worldY, dealt, crit);
        SpawnSparks(target.worldX, target.worldY, 4, r, g, b, 260.0f);
        at.push_back({ target.worldX, target.worldY });
    };
    for (auto* m : g_MonsterManager.monsters) hit(*m);
    for (auto* rm : g_MonsterManager.rangedMobs) hit(*rm);
    ShowScreenHits(fx, at, { r, g, b });
    // No full-screen tint: every style draws its own lines (no veils over the desktop).
    // Small, frequent procs crackle; big ones land as a blast.
    Audio::Play(pct >= 500.0f ? Audio::Cue::ExplodeL
              : pct >= 150.0f ? Audio::Cue::Lightning : Audio::Cue::FieldPulse);
}

// pct of attack to every enemy within radius of (x, y).
void DamageRadius(float x, float y, float radius, float pct,
                  float r, float g, float b, float extra, strike_fx::BlastStyle style) {
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
    strike_fx::Blast(x, y, radius, { r, g, b }, 0.0f, style);
    Audio::Play(radius >= 250.0f ? Audio::Cue::ExplodeL
              : radius >= 170.0f ? Audio::Cue::ExplodeM : Audio::Cue::ExplodeS);
}

// Quiet burst for a gun verb: damage only (it fires per hit).
void BurstDamage(float x, float y, float radius, float pct) {
    const float dmg = AttackPower() * pct / 100.0f * AllDamageMult();
    auto hit = [&](auto& target) {
        if (!target.alive) return;
        const float dx = target.worldX - x, dy = target.worldY - y;
        if (dx * dx + dy * dy > radius * radius) return;
        const float dealt = g_MonsterManager.ApplyDamage(target, dmg);
        if (dealt > 0.0f) SpawnDamageNumber(target.worldX, target.worldY, dealt, false);
    };
    for (auto* m : g_MonsterManager.monsters) hit(*m);
    for (auto* rm : g_MonsterManager.rangedMobs) hit(*rm);
}

// Clears enemy bullets within radius of (x, y); radius < 0 clears them all.
int ClearEnemyBullets(float x, float y, float radius, bool show) {
    constexpr line_fx::Rgb kErase{ 0.70f, 0.95f, 1.0f };
    if (show && radius < 0.0f) strike_fx::Nova(s.in.px, s.in.py, 1400.0f, kErase);
    else if (show && radius >= 100.0f) strike_fx::Nova(x, y, radius - 40.0f, kErase);
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
    if (cleared > 0) Audio::Play(Audio::Cue::SfxBulletErase);
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
    DamageAll(100.0f, 0.65f, 0.80f, 1.0f, false, 1.0f, ScreenFx::Zap);
    s.procBusy = false;
}

// One hit for the spark counters (bullets and extra strikes).
void CountHit() {
    if (Has(AugType::P_SPARK_STATIC_BUILDUP) && ++s.staticHits >= 10) {
        s.staticHits = 0;
        Audio::Play(Audio::Cue::ProcCounterFull);
        DamageAll(25.0f, 0.55f, 0.85f, 1.0f, false, 1.0f, ScreenFx::Zap);
    }
    if (Has(AugType::P_SPARK_STELLAR_DYNAMO) && ++s.batteryHits >= 30) {
        s.batteryHits = 0;
        Audio::Play(Audio::Cue::ProcCounterFull);
        ResetDash();
    }
    if (Has(AugType::P_SPARK_CRITICAL_BURST) && ++s.blackboltHits >= 50) {
        s.blackboltHits = 0;
        Audio::Play(Audio::Cue::ProcCounterFull);
        DamageAll(600.0f, 0.45f, 0.30f, 0.85f, false, 1.0f, ScreenFx::BlackBolt);
    }
}

// Triple Strike: three strikes on enemies around (x, y).
void ExtraStrikes(float x, float y) {
    constexpr float kRadius = Rings(2);
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
    Audio::Play(Audio::Cue::ProcExtraHit);
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
    m *= syn::DamageMult(bullet, target);
    return m * RollCrit(crit);
}

void HookBulletHit(const Bullet& bullet, const void* target, float x, float y,
                   float, bool crit, bool killed) {
    CountHit();
    if (bullet.skillShot) {
        active::OnSkillBulletHit(bullet, x, y, killed);
        return;
    }
    syn::OnBulletHit(bullet, target, x, y, crit, killed);
    if (crit && Has(AugType::P_EDGE_CRITICAL_CHAIN_BURST) && ++s.crits >= 6) {
        s.crits = 0;
        Audio::Play(Audio::Cue::ProcCounterFull);
        DamageAll(500.0f, 1.0f, 0.85f, 0.45f, true, 1.0f, ScreenFx::CritChain);
    }
    if (Has(AugType::P_SHADOW_TRIPLE_STRIKE) && Chance(0.10f)) {
        ExtraStrikes(x, y);
        ChanceTriggered();
    }
    if (Has(AugType::P_STORM_CORONA_ROUND) && Chance(0.03f)) {
        DamageAll(100.0f, 0.65f, 0.80f, 1.0f, false, 1.0f, ScreenFx::Zap);
        ChanceTriggered();
    }
    if (Has(AugType::P_VEIN_CRATER_ROUND) && s.pickaxeCd <= 0.0f && Chance(0.20f)) {
        s.pickaxeCd = 5.0f;
        DamageRadius(x, y, Rings(1.5f), 200.0f, 1.0f, 0.70f, 0.30f);
        ChanceTriggered();
    }
    if (bullet.blastOnHit && s.rushOwed > 0.0f) {   // 대시 후 첫 사격 범위 피해 (첫 명중 한 번만)
        s.rushOwed = 0.0f;
        DamageRadius(x, y, Rings(1.5f), 200.0f, 1.0f, 0.55f, 0.25f);
        Audio::Play(Audio::Cue::ProcShockwave);
    }
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
        Audio::Play(Audio::Cue::ProcStardustGain);
        ChanceTriggered();
    }
    if (Has(AugType::P_NOVA_STELLAR_CORE_RUPTURE) && s.kills % 20 == 0) {
        Audio::Play(Audio::Cue::ProcCounterFull);
        DamageAll(700.0f, 1.0f, 0.95f, 0.75f, false, 1.0f, ScreenFx::Nova);
        ClearEnemyBullets(0.0f, 0.0f, -1.0f);
    }
}

void HookHurt(float& hp, float dmg) {
    if (s.in.damageProtected) { hp -= dmg; return; }   // undone this step anyway
    active::RecordDamage(dmg);
    if (Has(AugType::P_STILL_STILLNESS_PLATING) && s.stillT >= 2.0f) dmg *= 0.75f;
    if (Has(AugType::P_GEM_BUFFER_PLATING) && s.emeraldLeft > 0.0f) {
        const float blocked = std::min(dmg, s.emeraldLeft);   // the plate takes what it can
        s.emeraldLeft -= blocked;
        dmg -= blocked;
        Audio::Play(s.emeraldLeft <= 0.0f ? Audio::Cue::SfxShieldBreak : Audio::Cue::SfxShieldBlock);
        if (dmg <= 0.0f) return;
    }
    hp -= dmg;
    syn::OnHurt(dmg);
    if ((s.amethystLeft > 0.0f && s.amethystLeft <= dmg) || (s.topazLeft > 0.0f && s.topazLeft <= dmg) ||
        (s.rubyLeft > 0.0f && s.rubyLeft <= dmg))
        Audio::Play(Audio::Cue::SfxShieldBreak);   // 피해 누적으로 깨지는 증강
    s.amethystLeft = std::max(0.0f, s.amethystLeft - dmg);
    s.topazLeft = std::max(0.0f, s.topazLeft - dmg);
    s.rubyLeft = std::max(0.0f, s.rubyLeft - dmg);
    if (Has(AugType::P_LIFE_EMERGENCY_MEND_SHARDS) && s.cakeSlices > 0) {
        s.cakeDamage += dmg;
        while (s.cakeSlices > 0 && s.cakeDamage >= kShardEvery) {
            s.cakeDamage -= kShardEvery;
            --s.cakeSlices;
            hp = std::min(g_Stats.maxHP, hp + 30.0f);
            Audio::Play(Audio::Cue::SfxHealSmall);
        }
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

bool RubyIntact() { return Has(AugType::P_GEM_GLASS_STAR) && s.rubyLeft > 0.0f; }
bool FirescaleOn() { return Has(AugType::P_FLAME_STELLAR_OVERHEAT) && !s.firescaleOffThisWave; }
bool DarkWingsOn() { return Has(AugType::P_HERESY_UNSTABLE_DRIVE) && !s.darkWingsOffThisWave; }
void SwitchOffDarkWings() { s.darkWingsOffThisWave = true; }

void InstallHooks() {
    passive_hooks::bulletDamageMult = HookBulletDamageMult;
    passive_hooks::bulletHit = HookBulletHit;
    passive_hooks::executes = HookExecutes;
    passive_hooks::kill = HookKill;
    passive_hooks::hurt = HookHurt;
    passive_hooks::bulletBounce = [](Bullet& b) { syn::OnBounce(b, true); };
    passive_hooks::bulletRedirect = [](Bullet& b, const void* t) { return syn::Redirect(b, t); };
}

void Reset() {
    s = State{};
    strike_fx::Clear();
    syn::Reset();

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
    if (!AugInBuild(d)) return false;
    if (AugIsSynergy(d.type)) return syn::CanOffer(augIdx);
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
    case AugType::P_GALE_FIRE_RATE_UP:     st.fireInterval /= 1.10f; break;
    case AugType::P_STONE_ATTACK_UP:     st.baseDamage += 8.0f; break;
    case AugType::P_LIFE_MAX_HP_UP:         st.maxHP += 15.0f; hp += 15.0f; break;
    case AugType::P_LIFE_REGENERATION:       st.regenPerSec += 0.2f; break;
    case AugType::P_FLIGHT_MOVE_SPEED_UP:      st.moveSpeedMult += 0.10f; break;
    case AugType::P_EDGE_CRITICAL_STRIKE:         st.critChance = std::min(100, st.critChance + 12); break;
    case AugType::P_EDGE_CRITICAL_AMP:      st.critMult += 0.5f; break;
    case AugType::P_CHRONO_STAR_TEMPO: st.moveSpeedMult += 0.08f; break;
    case AugType::P_STONE_NEUTRON_WARHEAD: st.moveSpeedMult -= 0.08f; break;
    case AugType::P_STONE_GIANT_WARHEAD: st.moveSpeedMult -= 0.10f; break;
    case AugType::P_HERESY_UNSTABLE_DRIVE:      st.moveSpeedMult += 0.25f; break;
    case AugType::P_STILL_SEA_OF_TRANQUILITY:     st.moveSpeedMult -= 0.08f; break;
    case AugType::P_LIFE_GRAZE_RECOVERY:   st.maxHP += 10.0f; hp += 10.0f; break;
    case AugType::P_LUCK_STARDUST_SUPPLY:       AddStardust(30); break;
    case AugType::P_GEM_UNSTABLE_AMP:  s.amethystLeft = kAmethystLife; break;
    case AugType::P_LUCK_GIFT_OF_STARS:       s.topazLeft = kAmethystLife; break;
    case AugType::P_GEM_GLASS_STAR:       s.rubyLeft = kRubyLife; break;
    case AugType::P_GEM_BUFFER_PLATING:      s.emeraldLeft = kPlateAbsorb; break;
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
    syn::PickupFx(augIdx);
    RecordTaken(augIdx);
    gamelog::Event("AUG_ACQUIRE", "aug=%s,rarity=%s,kind=%s,count=%d,level=%d", ALL_AUGS[augIdx].name,
                   AugRarityName(ALL_AUGS[augIdx].rarity),
                   AugIsStatCard(ALL_AUGS[augIdx]) ? "stat" : AugIsActive(ALL_AUGS[augIdx]) ? "active" : "passive",
                   s.taken[augIdx], g_GameManager.playerLevel);
    if (AugIsSynergy(ALL_AUGS[augIdx].type)) {
        syn::Acquire(augIdx);
        return;
    }
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
    StepLandingHits(in.dt);
    if (!s.pendingSplits.empty()) {
        for (const Bullet& c : s.pendingSplits)
            if (g_Bullets.size() < 800) g_Bullets.push_back(c);
        s.pendingSplits.clear();
    }
    syn::Step(in);
    const float dt = in.dt;
    s.clock += dt;
    s.pickaxeCd = std::max(0.0f, s.pickaxeCd - dt);
    s.shinobiCd = std::max(0.0f, s.shinobiCd - dt);
    s.greavesCd = std::max(0.0f, s.greavesCd - dt);
    s.strongmanWindow = std::max(0.0f, s.strongmanWindow - dt);
    s.rushOwed = std::max(0.0f, s.rushOwed - dt);

    TrackEnemies();

    // Movement: Route Sweep's distance (60○) and the 3s log.
    float moved = 0.0f;
    if (s.hasLast) moved = std::hypot(in.px - s.lastPX, in.py - s.lastPY);
    s.lastPX = in.px; s.lastPY = in.py; s.hasLast = true;
    s.moveLog.push_back({ s.clock, moved });
    while (!s.moveLog.empty() && s.moveLog.front().first < s.clock - 3.0f) s.moveLog.pop_front();
    if (Has(AugType::P_FLIGHT_ROUTE_SWEEP)) {
        constexpr float kSweepEvery = Rings(60);   // card text: every 60○ moved, on every screen
        s.cloudDist += moved;
        if (s.cloudDist >= kSweepEvery) {
            s.cloudDist -= kSweepEvery;
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
        Audio::Play(Audio::Cue::SkillCooldownCut);
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
        b.dmgMult = 4.0f;
        b.piercing = true;
        b.sizeScale = 1.8f;
        b.speed = g_Stats.bulletSpeed * 1.1f;
        b.color = glm::vec3(0.75f, 0.70f, 1.0f);
        g_Bullets.push_back(b);
        Audio::Play(Audio::Cue::ProcAutoShot);
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
            Audio::Play(Audio::Cue::ProcAutoShot);
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
                g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 0.3f);
                Audio::Play(Audio::Cue::SfxHealSmall);
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
    interval /= syn::FireIntervalMult();
    return std::max(0.03f, interval);
}

void ModifyShot(Bullet& bullet) {
    if (Has(AugType::P_STONE_GIANT_WARHEAD)) bullet.sizeScale *= 1.3f;
    if (Has(AugType::P_HUNTER_PROXIMITY_WARHEAD))
        bullet.maxRange = Rings(19);   // the card says 19○ (was 80% of the screen width)
    if (s.strongmanWindow > 0.0f) {
        bullet.blastOnHit = true;   // the first shot after the dash only
        s.strongmanWindow = 0.0f;
        s.rushOwed = 3.0f;          // long enough for any shot to reach the screen edge
    }
}

void OnShot() {
    ++s.shots;
    if (Has(AugType::P_GALE_SCAN_PULSE) && s.shots % 10 == 0) {
        Audio::Play(Audio::Cue::ProcCounterFull);
        DamageAll(15.0f, 0.80f, 0.95f, 0.85f, false, 1.0f, ScreenFx::Scan);
    }
    // Shinsoku's chance does not feed Crown of Storms: it would fire on half
    // of all shots.
    s.fastNext = Has(AugType::P_GALE_VARIABLE_FIRE) && Chance(0.50f);
}

bool DashSealed() { return Has(AugType::P_HERESY_BOUND_AMP); }

float PlateShieldLeft() { return Has(AugType::P_GEM_BUFFER_PLATING) ? s.emeraldLeft : 0.0f; }

void OnDashStarted() {
    gamelog::Dash();
    gamelog::Event("DASH", "hp=%.1f", g_GameManager.playerHP);
    float cd = DASH_CD;
    if (Has(AugType::P_ABYSS_ERRATIC_ORBIT)) cd = 1.0f + 5.0f * Rand01();
    if (Has(AugType::P_NIGHT_DWARF_COOLING)) cd -= 1.0f;
    if (DarkWingsOn()) cd -= 2.0f;
    cd = std::max(0.4f, cd);
    g_PlayerRuntime.dashCooldown = cd;
    g_PlayerRuntime.dashCooldownMax = cd;
    if ((Has(AugType::P_FLAME_LIMIT_BREAK) && !s.hornsOffThisWave) ||
        (Has(AugType::P_FLAME_STELLAR_OVERHEAT) && !s.firescaleOffThisWave))
        Audio::Play(Audio::Cue::ProcSeal);   // 이번 웨이브 동안 꺼짐
    if (Has(AugType::P_FLAME_LIMIT_BREAK)) s.hornsOffThisWave = true;
    if (Has(AugType::P_FLAME_STELLAR_OVERHEAT)) s.firescaleOffThisWave = true;
    active::OnDash();
    if (Has(AugType::P_CHRONO_DOUBLE_CAST) && Chance(0.30f)) {
        ResetDash();
        Audio::Play(Audio::Cue::SkillCooldownCut);
        ChanceTriggered();
    }
    if (Has(AugType::P_STORM_MAGNETIC_STORM) && Chance(0.20f)) {
        DamageAll(200.0f, 0.65f, 0.80f, 1.0f, false, 1.0f, ScreenFx::Zap);
        Audio::Play(Audio::Cue::SkillScreenwide);
        ChanceTriggered();
    }
    syn::OnDashStarted();
}

void OnWaveCleared() {
    syn::OnWaveCleared();
    if (Has(AugType::P_VEIN_GROWING_STAR) && s.canaryBonus < 0.30f) {
        s.canaryBonus = std::min(0.30f, s.canaryBonus + 0.02f);
        Audio::Play(Audio::Cue::ProcGrow);
    }
    if (Has(AugType::P_LUCK_GIFT_OF_STARS) && s.topazLeft > 0.0f) { AddStardust(8); Audio::Play(Audio::Cue::ProcStardustGain); }
    if (s.hornsOffThisWave || s.firescaleOffThisWave || s.darkWingsOffThisWave)
        Audio::Play(Audio::Cue::ProcSeal);   // 꺼졌던 증강이 다시 켜짐
    s.hornsOffThisWave = false;
    s.firescaleOffThisWave = false;
    s.darkWingsOffThisWave = false;
    active::OnWaveCleared();
}

bool TryPhoenix() {
    if (syn::TryRevive()) return true;
    if (!Has(AugType::P_FLAME_UNDYING_REIGNITION) || s.phoenixUsed) return false;
    s.phoenixUsed = true;
    Audio::Play(Audio::Cue::SfxRevive);
    strike_fx::Revive(s.in.px, s.in.py, { 1.0f, 0.55f, 0.20f });
    g_GameManager.playerHP = g_Stats.maxHP;
    DamageAll(1000.0f, 1.0f, 0.55f, 0.20f);
    return true;
}

}  // namespace passive
