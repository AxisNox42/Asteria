#include "SynergyRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ActiveSkills.h"
#include "AugmentCombat.h"
#include "AugmentSynergy.h"
#include "Bullet.h"
#include "EnemyParticles.h"
#include "GameManager.h"
#include "Juice.h"
#include "LineFx.h"
#include "StrikeFx.h"
#include "MonsterManager.h"
#include "PassiveHooks.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"

extern GameManager         g_GameManager;
extern MonsterManager      g_MonsterManager;
extern std::vector<Bullet> g_Bullets;
extern PlayerStats         g_Stats;

namespace syn {
namespace {

constexpr float kPi = 3.14159265f;

// One bool per synergy card, in table order (the bare gun, table index 0, is not a card).
struct Flags {
#define SYN(c) bool c = false;
#include "SynergyCodes.inc"
#undef SYN
};
constexpr int kCards = (int)(sizeof(Flags) / sizeof(bool));
static_assert(sizeof(Flags) == kCards * sizeof(bool), "Flags must be a plain bool array");

struct Zone  { float x, y, r, life, pct, tick, blastAtEnd, cr, cg, cb; };   // pct: % of attack per second
struct Mine  { float x, y, life, pct, pulse; };
struct Burn  { float t, tick; };
struct RingFx { float x, y, r, age, life, cr, cg, cb; };   // a brief line ring (Pulse)
struct Echo  { float t; std::vector<Bullet> shots; };

// The chance cards: a chance of one kind that holds while a condition does (SynergyChance.inc).
enum class CKind { Crit, Extra, Pierce, Bounce, Split, Blast };
enum class CCond { Still, Moving, LowHp, FullHp, Shield, Dash, Kill, Wave, Crowd, Few, Boss, Far, Barrage, Clean, Hurt, Combo, Long };
struct ChanceRow { const char* code; CKind kind; CCond cond; int pct; };
const ChanceRow kChance[] = {
#define CH(c, k, n, p) { #c, CKind::k, CCond::n, p },
#include "SynergyChance.inc"
#undef CH
};
constexpr int kChanceRows = (int)(sizeof(kChance) / sizeof(kChance[0]));

struct State {
    int   own[kCards + 1] = {};      // copies taken, by table index
    Flags on;                        // owned and switched on (its needs are met)
    uint64_t tags = 0;
    int   frame = 0;                 // table index of the owned frame, 0 = the bare gun
    bool  any = false;

    // Chance cards.
    std::vector<int> chanceRows;     // owned chance cards (index into kChance)
    float chance[6] = {};            // the chance of each kind that holds right now (0..1+)
    int   appliedChanceCrit = 0;
    float sinceDash = 99.0f, sinceHurt = 99.0f, nearest = 1e9f;
    int   crowd = 0, enemiesLeft = 0, nearBullets = 0;
    bool  bossUp = false;

    // Aggregated from the cards that change the gun's numbers.
    float dmg = 1, rate = 1, acc = 1, move = 1, bullets = 1, dmgAdd = 0;
    int   pierce = 0, bounce = 0, split = 0;
    float splitDmg = 0, crit = 0, critMul = 0, hpDelta = 0, hpSet = 0;
    float appliedMove = 1, appliedCrit = 0, appliedCritMul = 0, appliedHp = 0;

    // Run state.
    float clock = 0, stillT = 0, waveAge = 0, comboT = 0, shieldPrev = 0, stillGuardCd = 0;
    int   lastWave = -1, shots = 0, rhythm = 0, combo = 0, kills = 0, grazes = 0, orbitShots = 0;
    int   quietWaves = 0, cakeArmed = 0, cakeUsed = 0, mineShots = 0;
    float waveDamage = 0, revengeAcc = 0, cakeAcc = 0, stored = 0, absorbed = 0, dashBoostT = 0, swapCd = 0;
    float shieldPulse = 0;
    std::unordered_map<const void*, glm::vec2> seen;        // enemies alive last step, to notice deaths
    std::unordered_set<const void*> counted;                // kills already handled this step
    bool  stillCharged = false, prevDash = false, revived = false, lastWallGiven = false;
    float dashDropped = 0, dashLastX = 0, dashLastY = 0;
    std::vector<Zone>  zones;
    std::vector<Mine>  mines;
    std::vector<Echo>  echoes;
    std::vector<Bullet> pending;                          // spawned during a collision pass
    std::unordered_map<const void*, float> marks;
    std::unordered_map<const void*, Burn>  burns;
    std::vector<RingFx> rings;
    passive::StepInput in;
};
State s;

const std::vector<synergy::Aug>& T() { return synergy::Table(); }
int Index(const char* code) { return synergy::FindCode(code); }
// MSVC's RAND_MAX is 32767: `rand() % 10000` only reached 0..0.33 and chances fired ~3x too often.
float Rand01() { return (float)std::rand() / ((float)RAND_MAX + 1.0f); }
float Hp01() { return std::clamp(g_GameManager.playerHP / std::max(1.0f, g_Stats.maxHP), 0.0f, 1.0f); }

template <class F> void ForEachEnemy(F&& f) {
    for (auto* m : g_MonsterManager.monsters) if (m->alive) f(*m, m->worldX, m->worldY);
    for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) f(*r, r->worldX, r->worldY);
}

// Damage as a share of attack, with the "all damage" multipliers; no sound.
void Burst(float x, float y, float radius, float pct, line_fx::Rgb col = { 1.0f, 0.58f, 0.25f },
           strike_fx::BlastStyle style = strike_fx::BlastStyle::Impact) {
    if (!(pct > 0.0f)) return;   // nothing to deal, nothing to show
    strike_fx::Blast(x, y, radius, col, 0.0f, style);
    const float dmg = passive::AttackPower() * pct / 100.0f * passive::AllDamageMult();
    ForEachEnemy([&](auto& e, float ex, float ey) {
        const float dx = ex - x, dy = ey - y;
        if (dx * dx + dy * dy > radius * radius) return;
        const float dealt = g_MonsterManager.ApplyDamage(e, dmg);
        if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
    });
}

Bullet MakeShot(float x, float y, float ang, float dmgMult, float range) {
    Bullet b(x, y, x + std::cos(ang) * 100.0f, y + std::sin(ang) * 100.0f);
    b.speed = g_Stats.bulletSpeed;
    b.dmgMult = dmgMult;
    b.maxRange = range;
    b.color = glm::vec3(1.0f, 0.78f, 0.35f);
    return b;
}

void AddShield(float amount) {
    g_PlayerShield = std::min(220.0f, g_PlayerShield + amount);
    g_PlayerShieldTimer = 1.0e9f;
}

// A brief line ring (a zone opening, a mine's reach, a swap, the shield).
void Pulse(float x, float y, float radius, float life, float r, float g, float b) {
    if (s.rings.size() >= 400) return;
    s.rings.push_back({ x, y, radius, 0.0f, life, r, g, b });
}

void AddZone(float x, float y, float r, float life, float pct, float blastAtEnd = 0.0f,
             float cr = 1.0f, float cg = 0.62f, float cb = 0.25f) {
    if (s.zones.size() > 300) return;
    s.zones.push_back({ x, y, r, life, pct, 0.0f, blastAtEnd, cr, cg, cb });
    Pulse(x, y, r, 0.45f, cr, cg, cb);
}

bool IsBoss(const Monster& m) { return IsBossMob(m.kind) || IsEliteMob(m.kind); }

int CountOrbiters() {
    int n = 0;
    for (const Bullet& b : g_Bullets) if (b.active && (b.synFlags & synf::Orbit)) ++n;
    return n;
}

void KillEvent(float x, float y, const void* target);

// Lightning from (x, y): up to `count` nearest enemies within `range`, each hit for pct of attack.
void Chain(float x, float y, const void* skip, int count, float range, float pct, float growth) {
    std::unordered_set<const void*> hit;
    if (skip) hit.insert(skip);
    float cx = x, cy = y, p = pct;
    for (int i = 0; i < count; ++i) {
        const void* best = nullptr; float bx = 0, by = 0, bd = range * range;
        auto consider = [&](const void* id, float ex, float ey) {
            if (hit.count(id)) return;
            const float d = (ex - cx) * (ex - cx) + (ey - cy) * (ey - cy);
            if (d < bd) { bd = d; best = id; bx = ex; by = ey; }
        };
        for (auto* m : g_MonsterManager.monsters) if (m->alive) consider(m, m->worldX, m->worldY);
        for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) consider(r, r->worldX, r->worldY);
        if (!best) break;
        hit.insert(best);
        Audio::Play(Audio::Cue::SfxChainZap);
        if (i == 0) strike_fx::Zap(s.in.px, s.in.py, bx, by, { 0.62f, 0.84f, 1.0f }, 2.2f, 0.0f, 0.0f);
        else        strike_fx::Bolt(cx, cy, bx, by, { 0.62f, 0.84f, 1.0f }, 2.2f, 0.05f * (float)i);
        strike_fx::Blast(bx, by, 34.0f, { 0.62f, 0.84f, 1.0f }, 0.05f * (float)i, strike_fx::BlastStyle::Discharge);
        const float dmg = passive::AttackPower() * p / 100.0f * passive::AllDamageMult();
        for (auto* m : g_MonsterManager.monsters)
            if (m == best) { const float d = g_MonsterManager.ApplyDamage(*m, dmg); if (d > 0) SpawnDamageNumber(bx, by, d, false); }
        for (auto* r : g_MonsterManager.rangedMobs)
            if (r == best) { const float d = g_MonsterManager.ApplyDamage(*r, dmg); if (d > 0) SpawnDamageNumber(bx, by, d, false); }
        cx = bx; cy = by; p *= 1.0f + growth;
    }
}

// Everything a kill sets off, whoever made it (a bullet, a burn, a zone, a skill).
void KillEvent(float x, float y, const void* target) {
    if (!s.counted.insert(target).second) return;
    const Flags& o = s.on;
    s.combo += 1;
    s.comboT = 3.0f;
    ++s.kills;
    if (o.T_KILLBURST && passive::Chance(0.12f))
        for (int i = 0; i < 4; ++i)
            s.pending.push_back(MakeShot(x, y, kPi * 0.5f * (float)i + 0.4f, 0.6f, 520.0f));
    if (o.V_CHAIN) Chain(x, y, target, 3, 280.0f, 80.0f, 0.0f);
    if (o.G_CIRCUIT && s.kills % 40 == 0 && g_PlayerShield < 64.0f) AddShield(32.0f);
}

// What a dash's end sets off. A swap shot ends one at once.
void DashEnded() {
    const Flags& o = s.on;
    if (o.T_FIREWORK)
        for (int i = 0; i < 12; ++i)
            s.pending.push_back(MakeShot(s.in.px, s.in.py, 2.0f * kPi * (float)i / 12.0f, 0.6f, 520.0f));
    if (o.G_PARRY) passive::ClearEnemyBullets(s.in.px, s.in.py, 220.0f);
}

bool Uses(const std::string& code) {
    // The cards whose numbers go straight into the gun (everything else is coded by its rule text).
    static const std::unordered_set<std::string> k = {
        "F_MINIGUN", "F_LASER", "F_SHOTGUN", "F_SNIPER", "F_BURST", "F_CROSSBOW", "F_WAVE", "F_MORTAR",
        "V_BOUNCE", "V_PIERCE", "V_SPLIT", "V_TWIN",
        "C_GLASS", "C_HERESY", "C_RECKLESS", "C_ASCETIC",
        "A_COLLECTOR", "A_REACTOR", "A_GAMBLER", "Y_FATE", "Y_ETERNAL", "V_HOVER" };
    return k.count(code) > 0;
}

void Rebuild() {
    const auto& t = T();
    s.frame = 0;
    s.tags = 0;
    for (int i = 1; i <= kCards; ++i)
        if (s.own[i] > 0) {
            s.tags |= t[i].gives;
            if (t[i].role == synergy::Role::Frame) s.frame = i;
        }
    s.chanceRows.clear();
    for (int i = 1; i <= kCards; ++i)
        if (s.own[i] > 0 && t[i].role == synergy::Role::Chance)
            for (int r = 0; r < kChanceRows; ++r)
                if (std::string(kChance[r].code) == t[i].code) s.chanceRows.push_back(r);
    bool* flags = reinterpret_cast<bool*>(&s.on);
    s.any = false;
    for (int i = 1; i <= kCards; ++i) {
        const bool alive = s.own[i] > 0 && (t[i].needs & s.tags) == t[i].needs;
        flags[i - 1] = alive;
        s.any = s.any || s.own[i] > 0;
    }
    s.dmg = s.rate = s.acc = s.move = s.bullets = 1.0f;
    s.dmgAdd = s.splitDmg = s.crit = s.critMul = s.hpDelta = s.hpSet = 0.0f;
    s.pierce = s.bounce = s.split = 0;
    for (int i = 1; i <= kCards; ++i) {
        if (!flags[i - 1] || !Uses(t[i].code)) continue;
        const synergy::Fx& f = t[i].fx;
        s.dmg *= f.dmg; s.rate *= f.rate; s.acc *= f.acc; s.move *= f.move; s.bullets *= f.bullets;
        s.pierce += f.pierce; s.bounce += f.bounce; s.split += f.split; s.splitDmg += f.splitDmg;
        s.crit += f.crit; s.critMul += f.critMul; s.hpDelta += f.hpDelta;
        if (f.hpSet > 0) s.hpSet = s.hpSet > 0 ? std::min(s.hpSet, f.hpSet) : f.hpSet;
        s.dmgAdd += f.dmgAdd;
        if (t[i].scales) {
            int n = 0;
            if (t[i].scales == synergy::kRiskTags) {
                // Risk Collector counts risky cards, not tag kinds: there are only 4 risk
                // tags, so counting kinds could never reach the card's "max 6".
                for (int j = 1; j <= kCards; ++j)
                    if (j != i && s.own[j] > 0 && (t[j].gives & synergy::kRiskTags)) ++n;
            } else {
                for (uint64_t m = s.tags & t[i].scales; m; m &= m - 1) ++n;
            }
            s.dmgAdd += f.perTag * (float)std::min(n, 6);
        }
    }
    // Persistent changes to the player's stats, applied as differences.
    g_Stats.moveSpeedMult *= s.move / s.appliedMove;
    s.appliedMove = s.move;
    const int dc = (int)std::lround((s.crit - s.appliedCrit) * 100.0f);
    g_Stats.critChance = std::clamp(g_Stats.critChance + dc, 0, 100);
    s.appliedCrit = s.crit;
    g_Stats.critMult += s.critMul - s.appliedCritMul;
    s.appliedCritMul = s.critMul;
    const float base = g_Stats.maxHP - s.appliedHp;
    float want = base + s.hpDelta;
    if (s.hpSet > 0) want = std::min(want, s.hpSet);
    want = std::max(want, 20.0f);
    g_Stats.maxHP += (want - base) - s.appliedHp;
    s.appliedHp = want - base;
    g_GameManager.maxHP = g_Stats.maxHP;
    g_GameManager.playerHP = std::min(g_GameManager.playerHP, g_Stats.maxHP);
}

// The verbs a single bullet gets from the cards in play.
void ApplyVerbs(Bullet& b, float tx, float ty) {
    const Flags& o = s.on;
    b.bouncesLeft += s.bounce;
    b.pierceLeft += s.pierce;
    if (s.split > 0) { b.splitCount = s.split; b.splitDmg = s.splitDmg; }
    if (o.V_RICOCHET) b.synRicochet = 1;
    if (o.V_HOMING || o.V_HUNTER) { b.homing = true; b.homingTurn = o.V_HUNTER ? 4.0f : 3.0f; }
    if (o.V_BLAST) b.blastPct = 0.30f;
    if (o.V_MARK) b.synFlags |= synf::Mark;
    if (o.V_BURN) b.synFlags |= synf::Burn;
    if (o.V_GROW) { b.synFlags |= synf::Grow; b.synGrowBase = b.sizeScale; }
    if (o.V_HOVER) b.synFlags |= synf::Hover;
    if (o.V_RETURN || o.V_HUNTER) { b.synFlags |= synf::Return; b.maxRange = 0.0f; }
    if (o.V_TRAIL) b.synFlags |= synf::Trail;
    if (o.V_GRAVITY || o.Y_SINGULAR) b.synFlags |= synf::Gravity;
    if (o.V_SWAP) b.synFlags |= synf::Swap;
    if (o.V_ABSORB) b.synFlags |= synf::Absorb;
    {
        auto roll = [](float c) { return c > 0.0f && Rand01() < c; };
        if (roll(s.chance[(int)CKind::Pierce])) b.pierceLeft += 1;
        if (roll(s.chance[(int)CKind::Bounce])) b.bouncesLeft += 1;
        if (roll(s.chance[(int)CKind::Split]) && b.splitCount == 0) { b.splitCount = 3; b.splitDmg = 0.28f; }
        if (roll(s.chance[(int)CKind::Blast])) b.blastPct = std::max(b.blastPct, 0.30f);
    }
    if (o.V_DICE) {
        auto roll = [] { return std::rand() % 4; };
        int r = roll();
        if (r == 0) b.bouncesLeft += 2;
        else if (r == 1) b.pierceLeft += 2;
        else if (r == 2) b.blastPct = std::max(b.blastPct, 0.30f);
        else { b.splitCount = std::max(b.splitCount, 3); b.splitDmg = std::max(b.splitDmg, 0.28f); }
    }
}

void ApplyFrameLook(Bullet& b, float tx, float ty) {
    const Flags& o = s.on;
    // A frame that fires few bullets kills about one enemy per shot however hard it hits, so the
    // slow frames fire faster with less per shot (table) and the spread frames get a small correction
    // measured in the smoke run (kills in 20 s).
    if (o.F_SHOTGUN) { b.maxRange = 560.0f; b.speed *= 0.9f; b.dmgMult *= 1.3f; }
    if (o.F_SNIPER) { b.speed *= 1.6f; b.sizeScale *= 1.4f; b.color = glm::vec3(0.85f, 0.95f, 1.0f); }
    if (o.F_CROSSBOW) { b.speed *= 1.3f; b.sizeScale *= 2.0f; b.color = glm::vec3(0.95f, 0.85f, 0.55f); }
    if (o.F_WAVE) {
        b.dmgMult *= 1.3f; b.speed *= 0.55f; b.sizeScale *= 3.4f; b.piercing = true; b.synFlags |= synf::Erase;
        b.color = glm::vec3(0.55f, 0.85f, 1.0f);
    }
    if (o.F_LASER) {
        b.speed = 3400.0f; b.maxRange = 760.0f; b.piercing = true; b.sizeScale *= 0.55f;
        b.color = glm::vec3(0.55f, 0.95f, 1.0f);
    }
    if (o.F_MORTAR) {
        // The shell lands at a distance fixed now, along its own heading (spread shells land spread
        // around the aim instead of vanishing short of a point they never fly to).
        const float aimDist = std::max(40.0f, std::hypot(tx - b.x, ty - b.y));
        b.synFlags |= synf::Lob; b.synLandDist = aimDist;
        b.synTx = b.x + b.dirX * aimDist; b.synTy = b.y + b.dirY * aimDist;
        b.speed = 900.0f; b.sizeScale *= 1.4f;
        b.color = glm::vec3(1.0f, 0.55f, 0.25f);
    }
}

void Explode(float x, float y, float radius, float pct, bool pull);

// Which chances hold right now: the conditions of the owned chance cards, summed per kind.
bool CondHolds(CCond c, const passive::StepInput& in) {
    switch (c) {
        case CCond::Still:   return !in.moving && s.stillT >= 0.5f;
        case CCond::Moving:  return in.moving;
        case CCond::LowHp:   return Hp01() <= 0.5f;
        case CCond::FullHp:  return Hp01() >= 0.999f;
        case CCond::Shield:  return g_PlayerShield > 0.0f;
        case CCond::Dash:    return s.sinceDash < 2.0f;
        case CCond::Kill:    return s.comboT > 0.0f;
        case CCond::Wave:    return s.waveAge < 5.0f;
        case CCond::Crowd:   return s.crowd >= 8;
        case CCond::Few:     return s.enemiesLeft >= 1 && s.enemiesLeft <= 3;
        case CCond::Boss:    return s.bossUp;
        case CCond::Far:     return s.enemiesLeft > 0 && s.nearest > 350.0f;
        case CCond::Barrage: return s.nearBullets >= 3;
        case CCond::Clean:   return s.waveDamage < 0.01f;
        case CCond::Hurt:    return s.sinceHurt < 3.0f;
        case CCond::Combo:   return s.combo >= 5;
        case CCond::Long:    return s.waveAge >= 20.0f;
    }
    return false;
}

void UpdateChances(const passive::StepInput& in) {
    for (float& c : s.chance) c = 0.0f;
    int wantCrit = 0;
    if (!s.chanceRows.empty()) {
        s.crowd = 0; s.enemiesLeft = 0; s.nearest = 1e9f; s.bossUp = false; s.nearBullets = 0;
        ForEachEnemy([&](auto& e, float ex, float ey) {
            ++s.enemiesLeft;
            const float d = std::hypot(ex - in.px, ey - in.py);
            s.nearest = std::min(s.nearest, d);
            if (d < 300.0f) ++s.crowd;
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) if (IsBossMob(e.kind)) s.bossUp = true;
        });
        for (const int r : s.chanceRows)
            if (kChance[r].cond == CCond::Barrage) {
                for (const Bullet& b : g_Bullets)
                    if (b.active && b.isEnemy && std::hypot(b.x - in.px, b.y - in.py) < 200.0f) ++s.nearBullets;
                break;
            }
        for (const int r : s.chanceRows)
            if (CondHolds(kChance[r].cond, in)) s.chance[(int)kChance[r].kind] += (float)kChance[r].pct / 100.0f;
        wantCrit = (int)std::lround(s.chance[(int)CKind::Crit] * 100.0f);
    }
    g_Stats.critChance = std::clamp(g_Stats.critChance + wantCrit - s.appliedChanceCrit, 0, 100);
    s.appliedChanceCrit = wantCrit;
}

// What a shell that lands (instead of hitting one enemy) still does with its verbs.
void Landed(const Bullet& b, float radius) {
    const float x = b.synTx, y = b.synTy;
    const bool mark = (b.synFlags & synf::Mark) != 0, burn = (b.synFlags & synf::Burn) != 0;
    if (mark || burn)
        ForEachEnemy([&](auto& e, float ex, float ey) {
            if ((ex - x) * (ex - x) + (ey - y) * (ey - y) > radius * radius) return;
            if (mark) s.marks[&e] = 5.0f;
            if (burn) s.burns[&e] = { 3.0f, 0.5f };
        });
    if (b.synFlags & synf::Gravity) Explode(x, y, s.on.Y_SINGULAR ? 120.0f : 70.0f, s.on.Y_SINGULAR ? 150.0f : 0.0f, true);
    if (b.splitCount > 0 && s.pending.size() < 64) {
        const int n = std::min(b.splitCount, 5);
        for (int i = 0; i < n; ++i) {
            Bullet c = MakeShot(x, y, 2.0f * kPi * (float)i / (float)n + 0.3f, b.splitDmg, 520.0f);
            s.pending.push_back(c);
        }
    }
}

void Explode(float x, float y, float radius, float pct, bool pull) {
    if (pull) {
        const float pr = radius * 1.6f;
        strike_fx::Implode(x, y, pr, { 0.62f, 0.45f, 1.0f }, 0.0f, 0.30f);   // the pull's reach, closing in
        ForEachEnemy([&](auto& e, float ex, float ey) {
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) {
                if (IsBoss(e)) return;
                const float dx = x - ex, dy = y - ey, d = std::sqrt(dx * dx + dy * dy);
                if (d < 1.0f || d > pr) return;
                const float step = std::min(d, 90.0f);
                e.worldX += dx / d * step; e.worldY += dy / d * step;
            }
        });
    }
    if (pull) Burst(x, y, radius, pct, { 0.62f, 0.45f, 1.0f }, strike_fx::BlastStyle::Gravity);
    else      Burst(x, y, radius, pct);
}

}  // namespace

// ───────────────────────── registration ─────────────────────────

void Reset() { s = State{}; }

void ForgetEnemy(const void* enemy) {
    s.seen.erase(enemy);
    s.marks.erase(enemy);
    s.burns.erase(enemy);
}

void DrawWorld() {
    namespace lf = line_fx;
    // Ring pulses: a line that widens a little as it fades.
    for (const RingFx& r : s.rings) {
        const float k = std::clamp(r.age / r.life, 0.0f, 1.0f);
        const float grow = 0.88f + 0.12f * (1.0f - (1.0f - k) * (1.0f - k));
        lf::Circle(r.x, r.y, r.r * grow, 1.5f, { r.cr, r.cg, r.cb }, 0.8f * (1.0f - k));
    }
    if (!s.any) return;
    // Force field: its reach round the ship (six marks turning slowly on it),
    // and a small ring on every enemy shot it has slowed.
    if (s.on.G_FORCEFIELD) {
        const lf::Rgb fc{ 0.55f, 0.85f, 1.0f };
        lf::Circle(s.in.px, s.in.py, 160.0f, 1.2f, fc, 0.35f);
        for (int i = 0; i < 6; ++i) {
            const float a = s.clock * 0.5f + lf::kTwoPi * (float)i / 6.0f;
            const float ca = std::cos(a), sa = std::sin(a);
            lf::Line(s.in.px + ca * 152.0f, s.in.py + sa * 152.0f,
                     s.in.px + ca * 168.0f, s.in.py + sa * 168.0f, 1.8f, fc, 0.7f);
        }
        for (const Bullet& b : g_Bullets)
            if (b.active && b.isEnemy && (b.synFlags & synf::Slowed))
                lf::Circle(b.x, b.y, 9.0f, 1.0f, fc, 0.6f);
    }
    // Zones: a steady circle for as long as the zone lasts, fading out at the end.
    for (const Zone& z : s.zones)
        lf::Circle(z.x, z.y, z.r, 1.2f, { z.cr, z.cg, z.cb }, 0.45f * std::min(1.0f, z.life / 0.3f));
    // Mines: a small diamond in a diamond.
    for (const Mine& m : s.mines) {
        lf::Diamond(m.x, m.y, 9.0f, 1.5f, { 1.0f, 0.45f, 0.35f }, 0.95f);
        lf::Diamond(m.x, m.y, 4.0f, 1.0f, { 1.0f, 0.80f, 0.70f }, 0.8f);
    }
    if (s.marks.empty() && s.burns.empty()) return;
    const float t = s.clock;
    ForEachEnemy([&](auto& e, float ex, float ey) {
        // Marked: four corner brackets locked on the enemy.
        if (s.marks.count(&e)) {
            const float h = 20.0f, c = 7.0f;
            const lf::Rgb col{ 1.0f, 0.9f, 0.3f };
            for (int i = 0; i < 4; ++i) {
                const float sx = (i & 1) ? 1.0f : -1.0f, sy = (i & 2) ? 1.0f : -1.0f;
                const float cx = ex + sx * h, cy = ey + sy * h;
                lf::Line(cx, cy, cx - sx * c, cy, 1.5f, col, 0.9f);
                lf::Line(cx, cy, cx, cy - sy * c, 1.5f, col, 0.9f);
            }
        }
        // Burning: two small flame chevrons over the enemy, flickering.
        if (s.burns.count(&e)) {
            const lf::Rgb col{ 1.0f, 0.55f, 0.15f };
            for (int i = 0; i < 2; ++i) {
                const float fy = ey - 24.0f - 7.0f * (float)i - 3.0f * std::sin(t * 9.0f + (float)i);
                const float w = 6.0f - 1.5f * (float)i;
                const float a = 0.6f + 0.4f * std::sin(t * 13.0f + 1.7f * (float)i);
                lf::Line(ex - w, fy + w, ex, fy, 1.5f, col, a);
                lf::Line(ex + w, fy + w, ex, fy, 1.5f, col, a);
            }
        }
    });
}

void PickupFx(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL) return;
    const AugDef& d = ALL_AUGS[augIdx];
    float r, g, b;
    GetRarityColor(d.rarity, r, g, b);
    r = r * 0.55f + 0.45f; g = g * 0.55f + 0.45f; b = b * 0.55f + 0.45f;   // lifted so thin lines read
    const int tier = (int)d.rarity;                                          // 0 common .. 4 mythic
    const float x = s.in.px, y = s.in.py;
    Pulse(x, y, 70.0f + 28.0f * tier, 0.55f, r, g, b);
    if (tier >= 2) Pulse(x, y, 130.0f + 40.0f * tier, 0.75f, r, g, b);
    if (tier >= 3) Pulse(x, y, 240.0f + 40.0f * tier, 0.95f, 1.0f, 1.0f, 1.0f);
    SpawnEnemyExplosion(x, y, r, g, b, tier >= 2);
    SpawnWorldText(x, y - 34.0f, AugName(d), r, g, b, 1.5f + 0.15f * tier);
    if (tier >= 2) TriggerFlash(r, g, b, 0.08f + 0.05f * tier);
}

// The mortar's shell flies over walls and lands where it is aimed, so bounce / pierce / return verbs
// have nothing to act on. Neither side of such a pair is offered once the other is owned.
static bool ClashesWithMortar(int card) {
    static const char* kFlight[] = { "V_BOUNCE", "V_RICOCHET", "V_PIERCE", "V_RETURN", "V_HUNTER" };
    const int mortar = Index("F_MORTAR");
    const auto& t = T();
    bool isFlight = false;
    for (const char* c : kFlight) if (t[card].code == std::string(c)) isFlight = true;
    if (card == mortar) {
        for (const char* c : kFlight) if (s.own[Index(c)] > 0) return true;
        return false;
    }
    return isFlight && s.own[mortar] > 0;
}

bool CanOffer(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL || !AugIsSynergy(ALL_AUGS[augIdx].type)) return false;
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    if (i < 1 || i > kCards || s.own[i] > 0) return false;
    if (T()[i].role == synergy::Role::Frame && s.frame != 0) return false;   // one frame per run
    if ((T()[i].needs & s.tags) != T()[i].needs) return false;                // a card that would do nothing
    return !ClashesWithMortar(i);
}

namespace {
const wchar_t* const kTagNames[3][synergy::kTagCount] = {
    { L"관통", L"튕김", L"분열", L"유도", L"폭발", L"광선", L"공전", L"연쇄", L"연사", L"장전", L"산탄", L"박자",
      L"처치", L"치명타", L"튕길 때", L"피격", L"정지", L"대시", L"웨이브", L"보호막", L"체력", L"잔상", L"왕복",
      L"유리대포", L"벽 충돌", L"체공", L"점사", L"화상", L"표식", L"주사위", L"인력", L"소거", L"지뢰", L"성장",
      L"자리바꿈", L"저장", L"콤보", L"박자 사격", L"고각" },
    { L"Pierce", L"Bounce", L"Split", L"Homing", L"Blast", L"Beam", L"Orbit", L"Chain", L"Rapid", L"Charge", L"Spread", L"Beat",
      L"Kill", L"Crit", L"On bounce", L"Hurt", L"Still", L"Dash", L"Wave", L"Shield", L"HP", L"Trail", L"Return",
      L"Glass", L"Wall hit", L"Hover", L"Burst", L"Burn", L"Mark", L"Dice", L"Pull", L"Erase", L"Mine", L"Grow",
      L"Swap", L"Store", L"Combo", L"Beat shot", L"Lob" },
    { L"貫通", L"跳弾", L"分裂", L"誘導", L"爆発", L"光線", L"周回", L"連鎖", L"連射", L"装填", L"散弾", L"拍子",
      L"撃破", L"会心", L"跳ね時", L"被弾", L"静止", L"ダッシュ", L"ウェーブ", L"シールド", L"体力", L"残像", L"往復",
      L"ガラス砲", L"壁衝突", L"滞空", L"点射", L"火傷", L"刻印", L"サイコロ", L"引力", L"消去", L"地雷", L"成長",
      L"入替", L"蓄積", L"コンボ", L"拍子射撃", L"高角" } };
const wchar_t* const kRoleNames[3][7] = {
    { L"프레임", L"동사", L"트리거", L"증폭", L"대가", L"보호", L"확률" },
    { L"Frame", L"Verb", L"Trigger", L"Amp", L"Cost", L"Guard", L"Chance" },
    { L"フレーム", L"動詞", L"トリガー", L"増幅", L"代償", L"防護", L"確率" } };
const wchar_t* const kNeedsWord[3] = { L"필요", L"Needs", L"必要" };

std::wstring TagList(uint64_t mask, int lang, int maxTags) {
    std::wstring r;
    int n = 0;
    for (int b = 0; b < synergy::kTagCount; ++b)
        if (mask & (1ull << b)) {
            if (++n > maxTags) { r += L" …"; break; }
            if (!r.empty()) r += L" + ";
            r += kTagNames[lang][b];
        }
    return r;
}
}  // namespace

std::wstring MetaLine(int augIdx, int lang) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL || !AugIsSynergy(ALL_AUGS[augIdx].type)) return L"";
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    if (i < 1 || i > kCards) return L"";
    lang = std::clamp(lang, 0, 2);
    const synergy::Aug& a = T()[i];
    std::wstring r = kRoleNames[lang][(int)a.role];
    if (a.needs) r += L" · " + std::wstring(kNeedsWord[lang]) + L": " + TagList(a.needs, lang, 3);
    else if (a.gives) r += L" · " + TagList(a.gives, lang, 3);
    return r;
}

void Acquire(int augIdx) {
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    if (i < 1 || i > kCards) return;
    ++s.own[i];
    Rebuild();
    if (s.on.G_LASTWALL && !s.lastWallGiven) { s.lastWallGiven = true; AddShield(220.0f); }
    if (s.on.G_MIRROR && s.own[Index("G_MIRROR")] == 1) AddShield(90.0f);
}

// ───────────────────────── fire path ─────────────────────────

float FireIntervalMult() {
    if (!s.any) return 1.0f;
    float m = s.rate;
    if (s.on.T_BERSERK) m *= 1.0f + 0.6f * (1.0f - Hp01());
    return std::max(0.2f, m);
}

void Shape(const Bullet& base, float px, float py, float tx, float ty, std::vector<Bullet>& out) {
    out.clear();
    if (!s.any) { out.push_back(base); return; }
    const Flags& o = s.on;
    ++s.shots;
    const float aim = std::atan2(base.dirY, base.dirX);
    int n = std::clamp((int)std::lround(s.bullets), 1, 16);
    const bool parallel = n == 2 && !o.F_SHOTGUN && !o.F_BURST;
    float width = 0.0f;
    if (o.F_SHOTGUN) width = 0.60f;
    else if (o.F_BURST) width = 0.14f;
    else if (n > 1 && !parallel) width = std::min(0.12f * (float)(n - 1), 0.7f);

    float boost = 1.0f;
    if (s.absorbed > 0.0f && o.V_ABSORB) {
        boost += std::min(1.0f, s.absorbed * 0.05f);
        s.absorbed = 0.0f;
    }
    if (o.T_STILL && s.stillCharged) { boost *= 3.0f; s.stillCharged = false; }
    if (o.T_RHYTHM && ++s.rhythm >= 4) { s.rhythm = 0; boost *= 3.0f; Audio::Play(Audio::Cue::ProcCounterFull); }
    if (s.dashBoostT > 0.0f) boost *= 1.3f;
    if (o.T_GRUDGE && s.stored > 0.0f) {
        const float pct = std::min(600.0f, s.stored * 10.0f);
        // The grudge leaves the ship as a red bolt and bursts where it is aimed.
        strike_fx::Zap(s.in.px, s.in.py, tx, ty, { 1.0f, 0.30f, 0.30f }, 3.0f, 0.0f, 0.0f);
        Burst(tx, ty, 120.0f, pct, { 1.0f, 0.30f, 0.30f }, strike_fx::BlastStyle::Discharge);
        s.stored = 0.0f;
    }

    for (int i = 0; i < n; ++i) {
        Bullet b = base;
        float ang = aim;
        if (n > 1 && width > 0.0f) ang += -width * 0.5f + width * (float)i / (float)(n - 1);
        if (s.acc < 1.0f) ang += ((float)(std::rand() % 2001) - 1000.0f) / 1000.0f * (1.0f - s.acc) * 0.35f;
        b.dirX = std::cos(ang); b.dirY = std::sin(ang);
        if (parallel) {
            const float side = i == 0 ? -9.0f : 9.0f;
            b.x += -b.dirY * side; b.y += b.dirX * side; b.prevX = b.x; b.prevY = b.y;
        }
        b.dmgMult = base.dmgMult * boost;
        ApplyFrameLook(b, tx, ty);
        ApplyVerbs(b, tx, ty);
        if (o.T_ANCHOR && s.stillT > 0.5f) {
            b.sizeScale *= 1.0f + 0.15f * std::min(s.stillT, 4.0f);
            if (s.stillT > 1.0f) b.pierceLeft += 1;
        }
        out.push_back(b);
    }
    // Side shots of the spray: weak, but they carry the verbs.
    if (o.V_SPRAY) {
        for (int side = -1; side <= 1; side += 2) {
            Bullet b = out.front();
            const float ang = aim + 0.22f * (float)side;
            b.dirX = std::cos(ang); b.dirY = std::sin(ang);
            b.dmgMult *= 0.35f;
            out.push_back(b);
        }
    }
    // Every few shots one extra bullet that inherits the verbs.
    if (o.T_NTH && s.shots % 6 == 0) {
        Bullet b = out.front();
        const float ang = aim + 0.06f;
        b.dirX = std::cos(ang); b.dirY = std::sin(ang);
        out.push_back(b);
    }
    // Chance of an extra bullet (the chance cards).
    for (float c = s.chance[(int)CKind::Extra]; c > 0.0f; c -= 1.0f)
        if (Rand01() < std::min(c, 1.0f)) {
            Bullet b = out.front();
            const float ang = aim + (Rand01() < 0.5f ? -0.08f : 0.08f);
            b.dirX = std::cos(ang); b.dirY = std::sin(ang);
            out.push_back(b);
        }
    // The opening salvo: every shot goes out twice for the first seconds of a wave.
    if (o.T_OPENING && s.waveAge < 3.0f) {
        const size_t m = out.size();
        for (size_t i = 0; i < m; ++i) {
            Bullet b = out[i];
            const float ang = std::atan2(b.dirY, b.dirX) + 0.05f;
            b.dirX = std::cos(ang); b.dirY = std::sin(ang);
            out.push_back(b);
        }
    }
    // Orbit: every fifth shot is held around the player instead of flying off.
    if ((o.V_ORBIT || o.Y_HALO) && ++s.orbitShots >= (o.Y_HALO ? 3 : 5)) {
        s.orbitShots = 0;
        const int cap = 2 + (o.Y_HALO ? 2 : 0);
        if (CountOrbiters() < cap) {
            Bullet b = out.front();
            b.synFlags |= synf::Orbit; b.synOrbitAngle = Rand01() * 2.0f * kPi; b.synOrbitLeft = 4.5f;
            b.speed = 0.0f; b.piercing = true; b.maxRange = 0.0f; b.sizeScale *= 1.2f;
            b.x = px; b.y = py;
            out.push_back(b);
        }
    }
    // Mines: every third shot adds one that sticks in front of the aimed spot instead of flying on.
    if (o.V_MINE && ++s.mineShots >= 3) {
        s.mineShots = 0;
        Bullet b = base;
        b.dmgMult = base.dmgMult * boost;
        b.synFlags |= synf::Mine;
        b.synTx = b.x + (tx - b.x) * 0.55f; b.synTy = b.y + (ty - b.y) * 0.55f;
        b.synLandDist = std::hypot(b.synTx - b.x, b.synTy - b.y);
        out.push_back(b);
    }
    // Echo: the same volley again a moment later.
    if (o.Y_ECHO) {
        Echo e; e.t = 0.2f; e.shots = out;
        s.echoes.push_back(std::move(e));
    }
}

float DamageMult(const Bullet& b, const void* target) {
    if (b.skillShot || !s.any) return 1.0f;
    const Flags& o = s.on;
    float m = s.dmg * (1.0f + s.dmgAdd);
    auto mk = s.marks.find(target);
    if (mk != s.marks.end() && mk->second > 0.0f) m *= 1.25f;
    if (b.synFlags & synf::Grow) m *= 1.0f + std::min(1.5f, b.traveled / 700.0f);
    if (b.synBounces > 0) m *= 1.0f + (float)b.synBounces * (o.Y_ETERNAL ? 0.22f : 0.0f);
    if (o.T_COMBO) m *= 1.0f + std::min(0.40f, 0.02f * (float)s.combo);
    if (o.C_RITUAL) m *= 1.0f + std::min(0.40f, 0.08f * (float)s.quietWaves);
    if (o.T_LASTSTAND && Hp01() <= 0.30f) m *= 1.5f;
    return m;
}

float DamageTakenMult() {
    if (!s.any) return 1.0f;
    float m = 1.0f;
    if (s.on.C_ASCETIC) m *= 1.2f;
    if (s.on.C_RECKLESS) m *= 2.0f;
    if (s.on.C_REVIVE && s.revived) m *= 1.08f;
    if (s.on.T_LASTSTAND) m *= 1.05f;
    if (s.on.T_BERSERK) m *= 1.08f;
    return m;
}

// ───────────────────────── events ─────────────────────────

void OnBulletHit(const Bullet& b, const void* target, float x, float y, bool crit, bool killed) {
    if (!s.any || b.skillShot) return;
    const Flags& o = s.on;
    if (b.synFlags & synf::Mark) s.marks[target] = 5.0f;
    if (b.synFlags & synf::Burn) s.burns[target] = { 3.0f, 0.5f };
    if (b.blastPct > 0.0f) {
        const float r = 80.0f;
        Burst(x, y, r, b.blastPct * 100.0f);
    }
    if (b.synFlags & synf::Gravity) Explode(x, y, o.Y_SINGULAR ? 120.0f : 70.0f, o.Y_SINGULAR ? 150.0f : 0.0f, true);
    if (b.splitCount > 0 && !b.splitSpent && s.pending.size() < 64) {
        b.splitSpent = true;
        const float fan[5] = { -0.70f, 0.70f, 0.0f, -1.30f, 1.30f };
        const int n = std::min(b.splitCount, 5);
        for (int i = 0; i < n; ++i) {
            Bullet c = b;
            const float a = std::atan2(b.dirY, b.dirX) + fan[i];
            c.dirX = std::cos(a); c.dirY = std::sin(a);
            c.x = c.prevX = b.x; c.y = c.prevY = b.y;
            c.dmgMult = b.dmgMult * b.splitDmg;
            c.sizeScale = b.sizeScale * 0.75f;
            c.splitCount = 0; c.splitSpent = true; c.synRicochet = 0;
            if (!b.piercing && c.pierceLeft > 0) --c.pierceLeft;   // the parent spends one on this hit
            c.ClearHits(); c.RememberHit(target);
            s.pending.push_back(c);
        }
    }
    if ((b.synFlags & synf::Swap) && s.swapCd <= 0.0f) {
        bool ok = false;
        ForEachEnemy([&](auto& e, float ex, float ey) {
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) {
                if (&e != target || IsBoss(e) || ok) return;
                ok = true;
                const float ox = s.in.px, oy = s.in.py;
                active::RequestTeleport(ex, ey);
                e.worldX = ox; e.worldY = oy;
                s.swapCd = 1.5f;
                Pulse(ox, oy, 70.0f, 0.5f, 0.6f, 0.8f, 1.0f);
                Pulse(ex, ey, 70.0f, 0.5f, 0.6f, 0.8f, 1.0f);
                OnDashStarted();   // counts as a dash
                DashEnded();
            }
        });
    }
    if (crit && o.T_CRITECHO) {
        const int extra = 1;
        for (int i = 0; i < extra; ++i) {
            const float a = std::atan2(b.dirY, b.dirX) + 0.10f * (float)(i + 1) * (i % 2 ? 1.0f : -1.0f);
            Bullet e = MakeShot(s.in.px, s.in.py, a, 0.8f, 0.0f);
            e.bouncesLeft = b.bouncesLeft; e.pierceLeft = b.pierceLeft;
            s.pending.push_back(e);
        }
    }
    if (killed) KillEvent(x, y, target);
}

bool Redirect(Bullet& b, const void* hitTarget) {
    if (b.synRicochet <= 0) return false;
    const void* best = nullptr; float bd = 650.0f * 650.0f, bx = 0, by = 0;
    auto consider = [&](const void* id, float ex, float ey) {
        if (id == hitTarget) return;
        const float d = (ex - b.x) * (ex - b.x) + (ey - b.y) * (ey - b.y);
        if (d < bd) { bd = d; best = id; bx = ex; by = ey; }
    };
    for (auto* m : g_MonsterManager.monsters) if (m->alive) consider(m, m->worldX, m->worldY);
    for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) consider(r, r->worldX, r->worldY);
    if (!best) return false;
    --b.synRicochet;
    const float dx = bx - b.x, dy = by - b.y, d = std::sqrt(dx * dx + dy * dy);
    b.dirX = dx / d; b.dirY = dy / d;
    b.ClearHits(); b.RememberHit(hitTarget);
    OnBounce(b, false);   // a turn off a target is a bounce for the bounce cards too
    return true;
}

void OnBounce(Bullet& b, bool wall) {
    if (!s.any) return;
    ++b.synBounces;
    const Flags& o = s.on;
    (void)o; (void)wall;
}

void OnHurt(float dmg) {
    if (!s.any || dmg <= 0.0f) return;
    const Flags& o = s.on;
    s.waveDamage += dmg;
    s.sinceHurt = 0.0f;
    if (s.waveDamage >= 10.0f) s.quietWaves = 0;
    if (o.T_GRUDGE) s.stored = std::min(60.0f, s.stored + dmg);
    if (o.T_REVENGE) {
        s.revengeAcc += dmg;
        if (s.revengeAcc >= 20.0f) {
            s.revengeAcc = 0.0f;
            strike_fx::Implode(s.in.px, s.in.py, 120.0f, { 1.0f, 0.40f, 0.30f }, 0.0f, 0.16f);
            passive::ClearEnemyBullets(s.in.px, s.in.py, 320.0f);
            passive::DamageRadius(s.in.px, s.in.py, 300.0f, 300.0f, 1.0f, 0.4f, 0.3f);
        }
    }
    if (o.T_CAKE && s.cakeUsed + s.cakeArmed < 2) {
        s.cakeAcc += dmg;
        if (s.cakeAcc >= 20.0f) { s.cakeAcc = 0.0f; ++s.cakeArmed; }
    }
}

void OnDashStarted() {
    if (!s.any) return;
    const Flags& o = s.on;
    s.dashDropped = 0.0f; s.dashLastX = s.in.px; s.dashLastY = s.in.py;
    s.sinceDash = 0.0f;
    if (o.C_RECKLESS)
        g_PlayerRuntime.dashCooldown = std::max(0.4f, g_PlayerRuntime.dashCooldown - 1.5f);
    if (o.G_PARRY) g_PlayerRuntime.dashInvulnerability = std::max(g_PlayerRuntime.dashInvulnerability, 0.45f);
}

void OnWaveCleared() {
    if (!s.any) return;
    const Flags& o = s.on;
    if (o.G_CHARGER) AddShield(40.0f);
    if (o.G_FORCEFIELD) AddShield(15.0f);
    if (s.waveDamage < 10.0f) s.quietWaves = std::min(8, s.quietWaves + 1);
    s.waveDamage = 0.0f;
    if (o.T_CAKE && s.cakeArmed > 0) {
        --s.cakeArmed; ++s.cakeUsed;
        g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 20.0f);
        Audio::Play(Audio::Cue::SfxHealSmall);
    }
}

bool TryRevive() {
    if (!s.on.C_REVIVE || s.revived) return false;
    s.revived = true;
    Audio::Play(Audio::Cue::SfxRevive);
    strike_fx::Revive(s.in.px, s.in.py, { 0.55f, 1.0f, 0.75f });
    g_GameManager.playerHP = std::min(g_Stats.maxHP, 20.0f);
    return true;
}

// ───────────────────────── per step ─────────────────────────

void Step(const passive::StepInput& in) {
    s.in = in;
    // Ring pulses age even without synergy cards (a pickup of any card shows one).
    for (RingFx& r : s.rings) r.age += in.dt;
    s.rings.erase(std::remove_if(s.rings.begin(), s.rings.end(),
        [](const RingFx& r) { return r.age >= r.life; }), s.rings.end());
    if (!s.any) return;
    const Flags& o = s.on;
    const float dt = in.dt;
    s.clock += dt;
    s.swapCd = std::max(0.0f, s.swapCd - dt);
    s.stillGuardCd = std::max(0.0f, s.stillGuardCd - dt);
    s.dashBoostT = std::max(0.0f, s.dashBoostT - dt);
    s.sinceDash += dt; s.sinceHurt += dt;
    UpdateChances(in);
    g_PlayerDmgMult = DamageTakenMult();

    if (g_GameManager.currentWave != s.lastWave) { s.lastWave = g_GameManager.currentWave; s.waveAge = 0.0f; }
    else s.waveAge += dt;

    // Stillness.
    if (in.moving) { s.stillT = 0.0f; s.stillCharged = false; }
    else {
        s.stillT += dt;
        if (o.T_STILL && s.stillT >= 1.0f) s.stillCharged = true;
        if (o.G_STILLGUARD && s.stillT >= 2.0f && s.stillGuardCd <= 0.0f && g_PlayerShield < 30.0f) {
            AddShield(30.0f - g_PlayerShield); s.stillGuardCd = 20.0f;
        }
    }
    // Combo.
    if (s.comboT > 0.0f) { s.comboT -= dt; if (s.comboT <= 0.0f) s.combo = 0; }

    // Dash: the path it leaves, and what happens when it ends.
    if (in.dashActive && o.T_AFTERIMAGE) {
        s.dashDropped += std::hypot(in.px - s.dashLastX, in.py - s.dashLastY);
        s.dashLastX = in.px; s.dashLastY = in.py;
        if (s.dashDropped >= 40.0f) { s.dashDropped = 0.0f; AddZone(in.px, in.py, 34.0f, 2.0f, 35.0f, 0.0f, 0.75f, 0.55f, 1.0f); }
    }
    if (s.prevDash && !in.dashActive) DashEnded();
    s.prevDash = in.dashActive;

    // Shield: breaking it.
    if (s.shieldPrev > 0.0f && g_PlayerShield <= 0.0f) {
        Audio::Play(Audio::Cue::SfxShieldBreak);
        if (o.G_MIRROR)
            for (Bullet& b : g_Bullets)
                if (b.active && b.isEnemy &&
                    (b.x - in.px) * (b.x - in.px) + (b.y - in.py) * (b.y - in.py) < 260.0f * 260.0f) {
                    b.isEnemy = false; b.dirX = -b.dirX; b.dirY = -b.dirY; b.dmgMult = 1.0f;
                    b.color = glm::vec3(0.7f, 0.9f, 1.0f);
                }
    }
    s.shieldPrev = g_PlayerShield;

    // Graze: 30 bullets that miss by a hair give a shield.
    if (o.T_GRAZE) {
        const float hitR = 10.0f * g_Stats.playerSizeMult;
        for (Bullet& b : g_Bullets) {
            if (!b.active || !b.isEnemy || b.grazed) continue;
            const float d = std::hypot(b.x - in.px, b.y - in.py);
            if (d > hitR && d < hitR + 18.0f) {
                b.grazed = true;
                if (++s.grazes >= 30) { s.grazes = 0; AddShield(25.0f); Audio::Play(Audio::Cue::ProcCounterFull); }
            }
        }
    }
    // Force field: enemy bullets that come close slow down.
    if (o.G_FORCEFIELD)
        for (Bullet& b : g_Bullets)
            if (b.active && b.isEnemy && !(b.synFlags & synf::Slowed) &&
                (b.x - in.px) * (b.x - in.px) + (b.y - in.py) * (b.y - in.py) < 160.0f * 160.0f) {
                b.synFlags |= synf::Slowed; b.speed *= 0.8f;
            }

    // Marks and burns.
    for (auto it = s.marks.begin(); it != s.marks.end();) { it->second -= dt; it = it->second <= 0.0f ? s.marks.erase(it) : std::next(it); }
    // The shield shows as a ring round the ship while it holds.
    s.shieldPulse -= dt;
    if (s.shieldPulse <= 0.0f && g_PlayerShield > 0.0f) { s.shieldPulse = 0.9f; Pulse(in.px, in.py, 36.0f, 0.4f, 0.45f, 0.85f, 1.0f); }
    if (!s.burns.empty()) {
        const float dmg = passive::AttackPower() * 0.10f * passive::AllDamageMult();
        ForEachEnemy([&](auto& e, float ex, float ey) {
            auto it = s.burns.find(&e);
            if (it == s.burns.end()) return;
            it->second.t -= dt; it->second.tick -= dt;
            if (it->second.tick <= 0.0f) {
                it->second.tick += 0.5f;
                const float dealt = g_MonsterManager.ApplyDamage(e, dmg);
                if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
                Pulse(ex, ey, 16.0f, 0.3f, 1.0f, 0.55f, 0.15f);
            }
        });
        for (auto it = s.burns.begin(); it != s.burns.end();) it = it->second.t <= 0.0f ? s.burns.erase(it) : std::next(it);
    }

    // Zones (trails, wall clouds, dash afterimages).
    for (Zone& z : s.zones) {
        z.life -= dt; z.tick -= dt;
        if (z.tick <= 0.0f) {
            z.tick += 0.3f;
            const float dmg = passive::AttackPower() * z.pct * 0.3f / 100.0f * passive::AllDamageMult();
            ForEachEnemy([&](auto& e, float ex, float ey) {
                if ((ex - z.x) * (ex - z.x) + (ey - z.y) * (ey - z.y) > z.r * z.r) return;
                const float dealt = g_MonsterManager.ApplyDamage(e, dmg);
                if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
            });
        }
        if (z.life <= 0.0f && z.blastAtEnd > 0.0f) Burst(z.x, z.y, z.r * 1.4f, z.blastAtEnd);
    }
    s.zones.erase(std::remove_if(s.zones.begin(), s.zones.end(), [](const Zone& z) { return z.life <= 0.0f; }), s.zones.end());

    // Mines.
    for (size_t i = 0; i < s.mines.size(); ++i) {
        Mine& m = s.mines[i];
        m.life -= dt; m.pulse -= dt;
        if (m.pulse <= 0.0f) { m.pulse = 1.2f; Pulse(m.x, m.y, 80.0f, 0.6f, 1.0f, 0.45f, 0.35f); }
        bool trip = false;
        ForEachEnemy([&](auto&, float ex, float ey) {
            if ((ex - m.x) * (ex - m.x) + (ey - m.y) * (ey - m.y) < 80.0f * 80.0f) trip = true;
        });
        if (trip) {
            const float bx = m.x, by = m.y, bp = m.pct;
            m.life = 0.0f;
            Burst(bx, by, 95.0f, bp, { 1.0f, 0.45f, 0.35f }, strike_fx::BlastStyle::Crystal);
        }
    }
    s.mines.erase(std::remove_if(s.mines.begin(), s.mines.end(), [](const Mine& m) { return m.life <= 0.0f; }), s.mines.end());

    // Echoes of the volley.
    for (Echo& e : s.echoes) {
        e.t -= dt;
        if (e.t <= 0.0f) {
            for (Bullet b : e.shots) {
                if (g_Bullets.size() >= 800) break;
                b.x = b.prevX = in.px; b.y = b.prevY = in.py; b.splitSpent = false;
                g_Bullets.push_back(b);
            }
        }
    }
    s.echoes.erase(std::remove_if(s.echoes.begin(), s.echoes.end(), [](const Echo& e) { return e.t <= 0.0f; }), s.echoes.end());

    // Kills nobody reported (burn, zones, skills, explosions): an enemy that was alive and now is not.
    if (o.T_COMBO || o.T_KILLBURST || o.V_CHAIN || o.G_CIRCUIT) {
        std::unordered_map<const void*, glm::vec2> now;
        ForEachEnemy([&](auto& e, float ex, float ey) { now[&e] = glm::vec2(ex, ey); });
        for (const auto& kv : s.seen)
            if (!now.count(kv.first)) KillEvent(kv.second.x, kv.second.y, kv.first);
        s.seen.swap(now);
    }
    s.counted.clear();

    // Bullets spawned by events during the collision pass.
    for (const Bullet& b : s.pending) if (g_Bullets.size() < 800) g_Bullets.push_back(b);
    s.pending.clear();

    // The shots themselves.
    float absorbedNow = 0.0f;
    for (Bullet& b : g_Bullets) {
        if (!b.active || b.isEnemy || b.skillShot || b.synFlags == 0) continue;
        const uint32_t f = b.synFlags;
        if (f & synf::Grow) {
            b.sizeScale = std::max(b.sizeScale, b.synGrowBase * (1.0f + std::min(1.5f, b.traveled / 450.0f)));
            ClampPlayerBulletSize(b);
        }
        if ((f & synf::Trail) && b.traveled - b.synTrailAt >= 60.0f) {
            b.synTrailAt = b.traveled;
            AddZone(b.x, b.y, 26.0f, 1.2f, 40.0f, 0.0f);
        }
        if ((f & synf::Hover) && !(f & synf::Hovered)) {
            if (!(f & synf::Hovering) && b.traveled >= 300.0f) {
                b.synFlags |= synf::Hovering; b.synHoverSpeed = b.speed; b.synHoverLeft = 0.8f; b.speed = 0.0f;
            } else if (f & synf::Hovering) {
                b.synHoverLeft -= dt;
                if (b.synHoverLeft <= 0.0f) {
                    b.speed = b.synHoverSpeed; b.synFlags = (b.synFlags & ~synf::Hovering) | synf::Hovered;
                }
            }
        }
        if ((f & synf::Return) && !(f & synf::Returning) && b.traveled >= 520.0f) {
            b.synFlags |= synf::Returning; b.ClearHits(); b.homing = false; b.piercing = true;
        }
        if (b.synFlags & synf::Returning) {
            const float dx = in.px - b.x, dy = in.py - b.y, d = std::sqrt(dx * dx + dy * dy);
            if (d < 26.0f || b.traveled > 1600.0f) b.active = false;
            else { b.dirX = dx / d; b.dirY = dy / d; }
        }
        if (f & synf::Orbit) {
            b.synOrbitAngle += 5.0f * dt; b.synOrbitLeft -= dt;
            b.x = b.prevX = in.px + std::cos(b.synOrbitAngle) * 90.0f; b.y = b.prevY = in.py + std::sin(b.synOrbitAngle) * 90.0f;
            if (std::fmod(b.synOrbitLeft, 0.4f) < dt) b.ClearHits();
            if (b.synOrbitLeft <= 0.0f) b.active = false;
            if (o.Y_HALO) absorbedNow += (float)passive::ClearEnemyBullets(b.x, b.y, 30.0f, false);
        }
        if (f & synf::Erase) absorbedNow += (float)passive::ClearEnemyBullets(b.x, b.y, 38.0f * b.sizeScale, false);
        if (f & synf::Absorb) absorbedNow += (float)passive::ClearEnemyBullets(b.x, b.y, 22.0f, false);
        if ((f & synf::Seek) && !(f & synf::Lob)) {   // a shell flies a set course; steering it would miss its landing
            float tx = 0, ty = 0; bool found = false;
            float bd = 600.0f * 600.0f;
            ForEachEnemy([&](auto& e, float ex, float ey) {
                const float d = (ex - b.x) * (ex - b.x) + (ey - b.y) * (ey - b.y);
                auto mk = s.marks.find(&e);
                if (mk != s.marks.end() && d < bd) { bd = d; tx = ex; ty = ey; found = true; }
            });
            if (!found) found = g_MonsterManager.FindNearestEnemy(b.x, b.y, tx, ty);
            if (found) {
                const float want = std::atan2(ty - b.y, tx - b.x), cur = std::atan2(b.dirY, b.dirX);
                float diff = want - cur;
                while (diff > kPi) diff -= 2.0f * kPi;
                while (diff < -kPi) diff += 2.0f * kPi;
                const float a = cur + std::clamp(diff, -6.0f * dt, 6.0f * dt);
                b.dirX = std::cos(a); b.dirY = std::sin(a);
            }
        }
        if (f & synf::Lob) {
            if (b.traveled >= b.synLandDist) {
                b.active = false;
                // Back off the last step's overshoot so it lands exactly on the aimed distance (the cursor).
                const float over = b.traveled - b.synLandDist;
                b.synTx = b.x - b.dirX * over; b.synTy = b.y - b.dirY * over;
                const float r = 110.0f;
                Burst(b.synTx, b.synTy, r, 90.0f * b.dmgMult * s.dmg * (1.0f + s.dmgAdd));
                Landed(b, r);
            }
        }
        if (f & synf::Mine) {
            if (b.traveled >= b.synLandDist - 2.0f) {
                b.active = false;
                if (s.mines.size() < 40) s.mines.push_back({ b.x, b.y, 8.0f, 220.0f * b.dmgMult * s.dmg * (1.0f + s.dmgAdd), 0.0f });
            }
        }
    }
    s.absorbed = std::min(10.0f, s.absorbed + absorbedNow);
}

}  // namespace syn
