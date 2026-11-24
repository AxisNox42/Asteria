#include "SynergyRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>

#include "ActiveSkills.h"
#include "AugmentCombat.h"
#include "RangeUnit.h"
#include "AugmentSynergy.h"
#include "SynergyUpgrade.h"
#include "Bullet.h"
#include "EnemyParticles.h"
#include "GameManager.h"
#include "Juice.h"
#include "LaserFx.h"
#include "LineFx.h"
#include "StrikeFx.h"
#include "MonsterManager.h"
#include "PassiveHooks.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include "../Core/WaveSystem.h"

extern GameManager        g_GameManager;
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
// One count per synergy card, in table order: how many times a branch was taken.
struct Counts {
#define SYN(c) unsigned char c = 0;
#include "SynergyCodes.inc"
#undef SYN
};
constexpr int kCards = (int)(sizeof(Flags) / sizeof(bool));
static_assert(sizeof(Flags) == kCards * sizeof(bool), "Flags must be a plain bool array");

struct Zone  { float x, y, r, life, pct, tick, blastAtEnd, cr, cg, cb;   // pct: % of attack per second
               int chain = 0;            // Afterimage Trail: the ribbon it belongs to (0 = a plain zone)
               bool burns = false;       // Afterimage Trail dropped by a burning shot: sets enemies alight
               float age = 0.0f; };
// Afterimage Trail as a comet tail in the shot's own colour (agreed 2026-11-17):
// one outline, wide at the shot and tapering to a point at the old end, a faint
// wash inside (the user asked for a touch of fill here), a bright core line
// fading back from the shot, and small stardust crosses peeling off the edges.
constexpr float kTrailLife = 1.2f;
constexpr float kTrailOutline = 0.8f;     // outline alpha
constexpr float kTrailFill  = 0.12f;      // wash alpha at the shot end
constexpr float kTrailCore  = 0.95f;      // core line alpha at the shot end
constexpr float kTrailHalfWidth = 0.6f;   // of the zone radius, at the shot
constexpr float kTrailHead = 100.0f;      // px: the head's width halves about every 70 px back from the shot
constexpr float kTrailThin = 0.18f;       // the lingering tail's width, as a share of the head's
constexpr float kTrailOutlineCut = 0.12f; // the outline covers the head down to this share
constexpr float kTrailHeadFade = 0.12f;   // s: the head melts away this fast after its shot is gone
constexpr float kTrailTip = 24.0f;        // px: the front narrows to a point over this once the shot is gone
constexpr float kTrailNose = 16.0f;       // px: the front closes onto the live shot over this
constexpr float kTrailZoneR = 26.0f;      // the hurt zones' radius (the width the tail is sized from)
// The drawn tail is its own record, apart from the hurt zones: every shot keeps
// the path it really flew (a point every kStreakStep px, and one at every turn
// sharper than kStreakTurn, so a bounce keeps its corner). A jump no flight can
// make (a returning or orbiting shot put back by the ship, an echo) starts a new
// tail instead of drawing a line to where the shot reappeared.
struct StreakPt { float x, y, age; };
struct Streak {
    int id = 0;
    std::vector<StreakPt> pts;          // old -> new
    float lastX = 0, lastY = 0;         // the shot last step
    float dirX = 1, dirY = 0;           // its heading at the last point
    float cr = 1, cg = 1, cb = 1;
    bool  live = true, seen = true;
    float sinceLive = 0.0f;             // s since its shot was last seen
};
constexpr float kStreakStep = 10.0f;
constexpr float kStreakTurnCos = 0.966f;  // cos 15 deg
constexpr int   kMaxStreaks = 160;
constexpr int   kMaxStreakPts = 120;
struct Mote { float x, y, vx, vy, life, cr, cg, cb; };
constexpr int   kMaxMotes = 90;
constexpr float kMoteLife = 0.6f;
constexpr float kMoteRate = 0.7f;         // per tail point per second
struct Mine  { float x, y, life, pct, pulse; };
struct Burn  { float t, tick; };
// 과부하 레이저: stacking burn. t = time left at this stack count, gain = cooldown to the next stack,
// lx/ly = where the enemy was last step, trail = distance moved since the last ember.
struct LaserBurn { float t, tick, gain, lx, ly, trail; int stacks; };
struct Ember { float x, y, dx, dy, len, age, life, seed; };   // a burnt-out streak left where a burning enemy went
constexpr int   kLaserBurnMax = 5;        // stacks
constexpr float kLaserBurnPct = 0.10f;    // of attack per second, per stack
constexpr float kLaserBurnGain = 0.25f;   // one stack per this long on the beam
constexpr float kLaserBurnHold = 3.0f;    // stacks hold this long after the last touch...
constexpr float kLaserBurnDrop = 1.0f;    // ...then drop one per this long
constexpr int   kMaxEmbers = 420;
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

// The chance and dice outcomes one shot gets (a laser rolls once per beam and keeps them).
struct VerbRoll { int pierce = 0, bounce = 0; bool split = false, blast = false; int dice = -1;
                  bool splitUp = false, blastUp = false;   // an upgraded 100% split / blast chance
                  int dice2 = -1; };                       // 주사위탄 branch B: a second outcome

struct State {
    int   own[kCards + 1] = {};      // copies taken, by table index
    Flags on;                        // owned and switched on (its needs are met)
    // Upgrade branches taken (SynergyUpgrade.h), each as many times as picked (2026-11-24):
    // A key number, B second number, C shape.
    Counts nA, nB, nC;
    // Family shapes: picks of branch C of each family, across all cards (x1, x2, x3 ...).
    int   shape[(int)synergy::Family::Count] = {};
    bool  framePick = false;         // the run-start frame pick is open: only frames are offered
    uint64_t tags = 0;
    int   frame = 0;                 // table index of the owned frame, 0 = the bare gun
    bool  any = false;

    // Chance cards.
    std::vector<int> chanceRows;     // owned chance cards (index into kChance)
    std::vector<int> chanceUp;       // ...and its branch A picks (chance, then effect)
    std::vector<int> chanceEase;     // ...and its branch B picks (condition eased)
    float chance[6] = {};            // the chance of each kind that holds right now (0..1+)
    // Upgraded chance cards whose chance would pass 100% keep it and grow their effect
    // (synergy::ChanceUpgrade): their chances, kept apart so a hit from them gets the bigger
    // effect (pierce / bounce +2, two extra shots, 4-way split, 45% blast). Crit: > 0 = +40% crit damage.
    float chanceBig[6] = {};
    int   chanceBigLv[6] = {};       // ...and how far their effect has grown (the most of the cards holding)
    int   appliedChanceCrit = 0;
    float appliedChanceCritMul = 0;
    float sinceDash = 99.0f, sinceHurt = 99.0f, nearest = 1e9f;
    float sinceKill = 99.0f, sinceShield = 99.0f;   // eased conditions (branch B)
    float trackAt = -9.0f, critBlastAt = -9.0f;     // shapes: last lightning / crit blast (clock)
    int   crowd = 0, enemiesLeft = 0, nearBullets = 0;
    bool  bossUp = false;

    // Aggregated from the cards that change the gun's numbers.
    float dmg = 1, rate = 1, acc = 1, move = 1, bullets = 1, dmgAdd = 0;
    float cardDmg = 1;   // cost cards' damage factor (all damage); `dmg` is the per-bullet part
    int   pierce = 0, bounce = 0, split = 0;
    float splitDmg = 0, crit = 0, critMul = 0, hpDelta = 0, hpSet = 0;
    float appliedMove = 1, appliedCrit = 0, appliedCritMul = 0, appliedHp = 0;

    // Run state.
    float clock = 0, stillT = 0, stillChargeT = 0, waveAge = 0, comboT = 0, shieldPrev = 0, stillGuardCd = 0;
    int   lastWave = -1, shots = 0, rhythm = 0, combo = 0, kills = 0, grazes = 0, orbitShots = 0;
    int   quietWaves = 0, cakeArmed = 0, cakeUsed = 0, mineShots = 0;
    float waveDamage = 0, revengeAcc = 0, cakeAcc = 0, stored = 0, absorbed = 0, dashBoostT = 0;
    float shieldPulse = 0;
    // Mythic looks kept readable under a barrage: the last singularity's gather (place, clock),
    // and the ricochet sparks drawn this step.
    float singularT = -9.0f, singularX = 0, singularY = 0;
    float fateCutT = -9.0f;   // 운명의 필살 일격: when its last cut fell (clock)
    int   bounceFx = 0;
    std::unordered_map<const void*, glm::vec2> seen;        // enemies alive last step, to notice deaths
    std::unordered_set<const void*> counted;                // kills already handled this step
    bool  stillCharged = false, prevDash = false, revived = false, lastWallGiven = false,
          lastWallSaved = false;   // 최후의 방벽: the lethal hits it blocks this run (branch B: two)
    int   lastWallSaves = 0;
    float dashDropped = 0, dashLastX = 0, dashLastY = 0;
    std::vector<Zone>  zones;
    std::vector<Mote>  motes;                             // Afterimage Trail stardust
    std::vector<Streak> streaks;                          // Afterimage Trail drawn tails
    int   trailChains = 0;
    std::vector<Mine>  mines;
    std::vector<Echo>  echoes;
    std::vector<Bullet> pending;                          // spawned during a collision pass
    std::unordered_map<const void*, float> marks;
    std::unordered_map<const void*, Burn>  burns;
    std::unordered_map<const void*, LaserBurn> laserBurns;
    std::unordered_map<const void*, float> laserProcAt;   // when a beam last set off a hit proc on this enemy
    bool procOk = true;                                   // the hit being handled may set off procs (see ProcGate)
    std::vector<Ember> embers;
    std::vector<RingFx> rings;
    passive::StepInput in;
    // 레이저: one beam is one shot. The beam the rolls below were made for, and what it rolled.
    int   laserBeam = -1;
    float beamRollT = -1.0e9f;                            // s.clock when the beam's rolls were last made
    float beamBoost = 1.0f;
    bool  beamNth = false;
    std::vector<float> beamExtra;                         // extra shots' angle offsets
    VerbRoll beamRoll;
    std::unordered_map<const void*, float> chainStruck;   // 번개 연쇄탄: when lightning last struck it
    std::unordered_map<const void*, float> singularAt;    // 특이점 붕괴 탄두: its last implosion on it
    float openingAt = -1.0f;                              // 개전 일제사격: the wave's first hit (clock)
    bool  chanceWas[kChanceRows] = {};                    // chance card conditions last step
    float chanceFxAt[kChanceRows] = {};                   // when each last showed its ring
};
State s;

const std::vector<synergy::Aug>& T() { return synergy::Table(); }

// The upgrade table (SynergyUpgrade.h): UPA(code) is the card's key number (branch A),
// UPA2 what grows with it, UPB its second number (branch B). Each use looks its row up once.
const synergy::UpRow& UpRowOf(const char* code) {
    static const synergy::UpRow kNone{ "", synergy::Family::None, {}, {}, {} };
    const synergy::UpRow* r = synergy::FindUp(code);
    return r ? *r : kNone;
}
#define UPROW(code) ([]() -> const synergy::UpRow& { static const synergy::UpRow& r_ = UpRowOf(#code); return r_; }())
#define UPA(code)  (synergy::UpAt(UPROW(code).A, s.nA.code))
#define UPA2(code) (synergy::UpAt(UPROW(code).A2, s.nA.code))
#define UPB(code)  (synergy::UpAt(UPROW(code).B, s.nB.code))
unsigned char* CountsOf(Counts& c) { return reinterpret_cast<unsigned char*>(&c); }
bool ShapeOn(synergy::Family f) { return s.shape[(int)f] > 0; }
float ShapeLv(synergy::Family f) { return (float)s.shape[(int)f]; }   // 1 one card, 2 two cards ...
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
           strike_fx::BlastStyle style = strike_fx::BlastStyle::Impact, float fxDelay = 0.0f) {
    if (!(pct > 0.0f)) return;   // nothing to deal, nothing to show
    strike_fx::Blast(x, y, radius, col, fxDelay, style);   // the damage itself is never delayed
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
    // The shield holds up to 220, or 최후의 방벽's own shield when it is owned (330 upgraded).
    const float cap = s.on.G_LASTWALL ? std::max(220.0f, UPA(G_LASTWALL)) : 220.0f;
    g_PlayerShield = std::min(cap, g_PlayerShield + amount);
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

// Afterimage Trail: one piece of a shot's ribbon. No opening ring: the ribbon
// itself shows it, and a fast gun would fill the screen with rings.
void AddTrailZone(float x, float y, float r, float pct, int chain, bool burns, glm::vec3 col) {
    if (s.zones.size() > 300) return;
    Zone z{ x, y, r, kTrailLife, pct, 0.0f, 0.0f, col.r, col.g, col.b };
    z.chain = chain;
    z.burns = burns;
    s.zones.push_back(z);
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
        s.chainStruck[best] = s.clock;
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
    s.comboT = COMBO_WINDOW + g_ComboWindowBonus;   // the same window as the HUD's combo (shop Combo Focus included)
    ++s.kills;
    s.sinceKill = 0.0f;
    if (o.T_KILLBURST && passive::Chance(UPA(T_KILLBURST))) {
        const int n = (int)UPB(T_KILLBURST);
        for (int i = 0; i < n; ++i)
            s.pending.push_back(MakeShot(x, y, 2.0f * kPi * (float)i / (float)n + 0.4f, 0.6f, 520.0f));
    }
    // An enemy the lightning itself killed sets off no new chain (a packed crowd chained without end).
    const auto struck = s.chainStruck.find(target);
    const bool chainKill = struck != s.chainStruck.end() && s.clock - struck->second < 0.5f;
    if (o.V_CHAIN && !chainKill) Chain(x, y, target, (int)UPA(V_CHAIN), 280.0f, UPB(V_CHAIN) * 100.0f, 0.0f);
    if (o.G_CIRCUIT && s.kills % std::max(1, (int)UPB(G_CIRCUIT)) == 0 && g_PlayerShield < 2.0f * UPA(G_CIRCUIT))
        AddShield(UPA(G_CIRCUIT));
    // Shape (추적): lightning to two nearby enemies, at most every 0.5 s.
    if (ShapeOn(synergy::Family::Track) && !chainKill && s.clock - s.trackAt >= 0.5f) {
        s.trackAt = s.clock;
        Chain(x, y, target, 2 * (int)ShapeLv(synergy::Family::Track), 280.0f, 50.0f, 0.0f);
    }
    // Shape (원소): enemies within 1.5 rings catch fire for 3 s.
    if (ShapeOn(synergy::Family::Element))
        ForEachEnemy([&](auto& e, float ex, float ey) {
            const float rr = Rings(1.0f + 0.5f * ShapeLv(synergy::Family::Element));   // 1.5 rings, +0.5 per stack
            if ((const void*)&e == target || (ex - x) * (ex - x) + (ey - y) * (ey - y) > rr * rr) return;
            auto [it, fresh] = s.burns.try_emplace(&e, Burn{ 3.0f, 0.5f });
            if (!fresh) it->second.t = std::max(it->second.t, 3.0f);
        });
}

// What a dash's end sets off. A swap shot ends one at once.
void DashEnded() {
    const Flags& o = s.on;
    if (o.T_FIREWORK) {
        const int n = (int)UPA(T_FIREWORK);
        for (int i = 0; i < n; ++i)
            s.pending.push_back(MakeShot(s.in.px, s.in.py, 2.0f * kPi * (float)i / (float)n, UPB(T_FIREWORK), 520.0f));
    }
    if (o.G_PARRY) passive::ClearEnemyBullets(s.in.px, s.in.py, Rings(UPB(G_PARRY)));
    // Shape (기동): a blast where the dash ends.
    if (ShapeOn(synergy::Family::Mobility)) Burst(s.in.px, s.in.py, Rings(1.5f), 150.0f * ShapeLv(synergy::Family::Mobility));
}

// An upgraded card whose numbers go straight into the gun: its branch A / B numbers after
// `na` / `nb` picks (SynergyUpgrade.h).
void UpgradeFx(const char* code, synergy::Fx& f, int na, int nb) {
    const synergy::UpRow* r = synergy::FindUp(code);
    if (!r) return;
    const std::string c = code;
    const float A = synergy::UpAt(r->A, na), A2 = synergy::UpAt(r->A2, na), B = synergy::UpAt(r->B, nb);
    if (na > 0) {
        if (c == "V_BOUNCE") f.bounce = (int)A;
        else if (c == "V_PIERCE") f.pierce = (int)A;
        else if (c == "V_SPLIT") f.split = (int)A;
        else if (c == "V_TWIN") f.dmg = A;
        else if (c == "V_HOVER") f.dmg = 1.0f + A;
        else if (c == "C_GLASS") { f.dmg = A; f.hpSet = A2; }
        else if (c == "C_HERESY") { f.rate = 1.0f + A; f.hpDelta = A2; }
        else if (c == "C_RECKLESS") f.dmgAdd = A;
        else if (c == "C_ASCETIC") f.dmg = 1.0f + A;
        else if (c == "A_COLLECTOR" || c == "A_REACTOR" || c == "A_GAMBLER") f.perTag = A;
    }
    if (nb > 0) {
        if (c == "V_PIERCE") f.dmg *= 1.0f + B;
        else if (c == "V_SPLIT") f.splitDmg = B;
        else if (c == "V_TWIN" || c == "V_HOVER") f.rate *= (1.0f + B) / (1.0f + r->B.a);
        else if (c == "C_GLASS") f.hpSet += B;
        else if (c == "C_HERESY") f.dmgAdd = B;
    }
}

// A card's family (its branch C shape): the upgrade table, or a chance card's kind.
synergy::Family CardFamily(int i) {
    const synergy::Aug& a = T()[i];
    if (a.role == synergy::Role::Chance) {
        for (int r = 0; r < kChanceRows; ++r)
            if (std::string(kChance[r].code) == a.code) return synergy::ChanceFamily((synergy::ChKind)(int)kChance[r].kind);
        return synergy::Family::None;
    }
    const synergy::UpRow* r = synergy::FindUp(a.code);
    return r ? r->fam : synergy::Family::None;
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
    s.chanceUp.clear();
    s.chanceEase.clear();
    for (int i = 1; i <= kCards; ++i)
        if (s.own[i] > 0 && t[i].role == synergy::Role::Chance)
            for (int r = 0; r < kChanceRows; ++r)
                if (std::string(kChance[r].code) == t[i].code) {
                    s.chanceRows.push_back(r);
                    s.chanceUp.push_back(CountsOf(s.nA)[i - 1]);
                    s.chanceEase.push_back(CountsOf(s.nB)[i - 1]);
                }
    const unsigned char* nA = CountsOf(s.nA);
    const unsigned char* nB = CountsOf(s.nB);
    const unsigned char* nC = CountsOf(s.nC);
    for (int& f : s.shape) f = 0;
    for (int i = 1; i <= kCards; ++i)
        if (s.own[i] > 0) s.shape[(int)CardFamily(i)] += nC[i - 1];
    bool* flags = reinterpret_cast<bool*>(&s.on);
    s.any = false;
    for (int i = 1; i <= kCards; ++i) {
        const bool alive = s.own[i] > 0 && (t[i].needs & s.tags) == t[i].needs;
        flags[i - 1] = alive;
        s.any = s.any || s.own[i] > 0;
    }
    g_FateCrits = s.on.Y_FATE;   // its crits are drawn as fate's (Juice.h SpawnDamageNumber)
    s.dmg = s.cardDmg = s.rate = s.acc = s.move = s.bullets = 1.0f;
    s.dmgAdd = s.splitDmg = s.crit = s.critMul = s.hpDelta = s.hpSet = 0.0f;
    s.pierce = s.bounce = s.split = 0;
    for (int i = 1; i <= kCards; ++i) {
        if (!flags[i - 1] || !Uses(t[i].code)) continue;
        synergy::Fx f = t[i].fx;
        if (nA[i - 1] || nB[i - 1]) UpgradeFx(t[i].code, f, nA[i - 1], nB[i - 1]);
        // Cost cards (Glass Cannon, Ascetic) raise all damage; frames and verbs only the bullet.
        if (t[i].role == synergy::Role::Cost) s.cardDmg *= f.dmg; else s.dmg *= f.dmg;
        s.rate *= f.rate; s.acc *= f.acc; s.move *= f.move; s.bullets *= f.bullets;
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
            // Branch B raises the cap (6 → 8).
            const synergy::UpRow* ur = synergy::FindUp(t[i].code);
            const int cap = ur ? (int)synergy::UpAt(ur->B, nB[i - 1]) : 6;
            s.dmgAdd += f.perTag * (float)std::min(n, cap);
        }
    }
    // Persistent changes to the player's stats, applied as differences.
    g_Stats.moveSpeedMult *= s.move / s.appliedMove;
    s.appliedMove = s.move;
    const int dc = (int)std::lround((s.crit - s.appliedCrit) * 100.0f);
    // Not clamped to 100: the sum is kept so taking a bonus back removes only what it added
    // (RollCrit caps the roll). Clamping here lost the base crit once a chance card switched off.
    g_Stats.critChance = std::max(0, g_Stats.critChance + dc);
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

// The chance cards' and the dice's outcomes for one shot (already shaped by its frame).
VerbRoll RollVerbs(const Bullet& b) {
    VerbRoll r;
    auto roll = [](float c) { return c > 0.0f && Rand01() < c; };
    // Pierce / bounce chances of several live cards add up past 100%: each whole 100% is one
    // more +1 (as the extra-shot chance does), so a second card is never wasted.
    for (float c = s.chance[(int)CKind::Pierce]; c > 0.0f; c -= 1.0f) if (roll(std::min(c, 1.0f))) r.pierce += 1;
    for (float c = s.chance[(int)CKind::Bounce]; c > 0.0f; c -= 1.0f) if (roll(std::min(c, 1.0f))) r.bounce += 1;
    // Upgraded cards that grew their effect: +2 when they fire.
    for (float c = s.chanceBig[(int)CKind::Pierce]; c > 0.0f; c -= 1.0f) if (roll(std::min(c, 1.0f))) r.pierce += 1 + s.chanceBigLv[(int)CKind::Pierce];
    for (float c = s.chanceBig[(int)CKind::Bounce]; c > 0.0f; c -= 1.0f) if (roll(std::min(c, 1.0f))) r.bounce += 1 + s.chanceBigLv[(int)CKind::Bounce];
    r.splitUp = roll(s.chanceBig[(int)CKind::Split]);
    r.blastUp = roll(s.chanceBig[(int)CKind::Blast]);
    r.split = r.splitUp || roll(s.chance[(int)CKind::Split]);
    r.blast = r.blastUp || roll(s.chance[(int)CKind::Blast]);
    if (s.on.V_DICE) {
        // Only outcomes that do something for this shot: a shell neither pierces, bounces nor
        // hits (its landing blast is its own), and a shot that pierces everything gains no pierce.
        const bool lob = (b.synFlags & synf::Lob) != 0;
        int pool[4], n = 0;
        if (!lob) pool[n++] = 0;                     // bounce +2
        if (!lob && !b.piercing && !s.on.V_HUNTER) pool[n++] = 1;   // pierce +2 (hunter shots pierce all)
        if (!lob) pool[n++] = 2;                     // blast
        pool[n++] = 3;                               // split
        r.dice = pool[std::rand() % n];
        // Branch B: a second, different outcome.
        if (UPB(V_DICE) >= 2.0f && n > 1) { do r.dice2 = pool[std::rand() % n]; while (r.dice2 == r.dice); }
    }
    return r;
}

// The verbs a single bullet gets from the cards in play.
void ApplyVerbs(Bullet& b, float tx, float ty, const VerbRoll& rolled) {
    const Flags& o = s.on;
    b.bouncesLeft += s.bounce;
    b.pierceLeft += s.pierce;
    if (s.split > 0) { b.splitCount = s.split; b.splitDmg = s.splitDmg; }
    if (o.V_RICOCHET) b.synRicochet = (int)UPA(V_RICOCHET);
    // Tracking Rounds only bends gently (1.5 rad/s, was 3.0: it hit almost everything); Hunter stays strong.
    // A mortar shell is not steered: it lands on the aimed spot.
    if ((o.V_HOMING || o.V_HUNTER) && !(b.synFlags & synf::Lob)) { b.homing = true; b.homingTurn = o.V_HUNTER ? 4.0f * (1.0f + UPB(V_HUNTER)) : 1.5f * (1.0f + UPA(V_HOMING));
        if (!o.V_HUNTER) b.dmgMult *= 1.0f + UPB(V_HOMING); }
    if (o.V_BLAST) b.blastPct = UPA(V_BLAST);
    if (o.V_MARK) b.synFlags |= synf::Mark;
    if (o.V_BURN) b.synFlags |= synf::Burn;
    if (o.V_GROW) { b.synFlags |= synf::Grow; b.synGrowBase = b.sizeScale; }
    if (o.V_HOVER) b.synFlags |= synf::Hover;
    // Relentless Hunter Rounds always pierce, on the way out as well as back.
    if (o.V_HUNTER) { b.synFlags |= synf::Return; b.maxRange = 0.0f; b.piercing = true; b.dmgMult *= 1.0f + UPA(V_HUNTER); }
    if (o.V_TRAIL) b.synFlags |= synf::Trail;
    if (o.V_GRAVITY || o.Y_SINGULAR) b.synFlags |= synf::Gravity;
    if (o.V_ABSORB) b.synFlags |= synf::Absorb;
    b.pierceLeft += rolled.pierce;
    b.bouncesLeft += rolled.bounce;
    if (rolled.split && b.splitCount == 0) {
        const int m = rolled.splitUp ? s.chanceBigLv[(int)CKind::Split] : 0;
        b.splitCount = synergy::ChanceSplitWays(m);
        b.splitDmg = synergy::ChanceSplitDmg(m);
    }
    if (rolled.blast) b.blastPct = std::max(b.blastPct, synergy::ChanceBlastPct(rolled.blastUp ? s.chanceBigLv[(int)CKind::Blast] : 0));
    for (int d = 0; d < 2 && o.V_DICE; ++d) {
        const int r = d == 0 ? rolled.dice : rolled.dice2;
        if (r < 0) continue;
        if (r == 0) b.bouncesLeft += (int)UPA(V_DICE);
        else if (r == 1) b.pierceLeft += (int)UPA(V_DICE);
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
    if (o.F_SNIPER) { b.speed *= 1.6f; b.sizeScale *= 1.4f; b.color = glm::vec3(0.85f, 0.95f, 1.0f); b.synFlags |= synf::FrameSniper; }
    if (o.F_CROSSBOW) { b.speed *= 1.3f; b.sizeScale *= 2.0f; b.color = glm::vec3(0.95f, 0.85f, 0.55f); b.synFlags |= synf::FrameCrossbow; }
    if (o.F_WAVE) {
        b.synFlags |= synf::FrameWave;
        b.dmgMult *= 1.3f; b.speed *= 0.55f; b.sizeScale *= 3.4f; b.piercing = true; b.synFlags |= synf::Erase;
        b.color = glm::vec3(0.55f, 0.85f, 1.0f);
    }
    if (o.F_LASER) {
        // A real beam (LaserFx.h): the shot never flies, it becomes the still segments
        // of a beam held from the ship. The beam's band is the shot's hit width.
        b.piercing = true; b.sizeScale *= 1.3f;   // range: unlimited unless a card caps it (maxRange is read in LaserFx Step)
        b.color = glm::vec3(0.55f, 0.95f, 1.0f);
        b.synFlags |= synf::FrameLaser;
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
    // 운명의 필살 일격: every shot is fate's crimson, whatever frame shaped it (user's call 2026-11-17).
    if (o.Y_FATE) b.color = glm::vec3(1.0f, 0.25f, 0.35f);
}

void Explode(float x, float y, float radius, float pct, bool pull, float pullRadius = 0.0f);

// Which chances hold right now: the conditions of the owned chance cards, summed per kind.
// `eased`: the card's branch B (synergy::ChanceCondB) loosens its condition.
// `ease`: the card's branch B picks (synergy::ChanceCondAt) loosen its condition.
bool CondHolds(const ChanceRow& row, const passive::StepInput& in, int ease = 0) {
    const float v = synergy::ChanceCondAt((synergy::ChCond)(int)row.cond, ease);
    const bool eased = ease > 0;
    switch (row.cond) {
        case CCond::Still:   return !in.moving && s.stillT >= v;
        case CCond::Moving:  return in.moving || (eased && s.stillT < v);
        case CCond::LowHp:   return Hp01() <= v;
        case CCond::FullHp:  return Hp01() >= std::min(v, 0.999f);
        case CCond::Shield:  return g_PlayerShield > 0.0f || (eased && s.sinceShield < v);
        // 도약 연발 / 도약 폭발 hold 1 s (2 s kept them on most of the time); 도약 도탄 2 s.
        case CCond::Dash:    return s.sinceDash < ((row.kind == CKind::Extra || row.kind == CKind::Blast) ? 1.0f : 2.0f) * v;
        case CCond::Kill:    return s.comboT > 0.0f || (eased && s.sinceKill < v);
        case CCond::Wave:    return s.waveAge < v;
        case CCond::Crowd:   return s.crowd >= (int)v;
        case CCond::Few: {   // a regular wave: once 90% of its kill quota is in; other waves: 1~3 enemies left
            const int quota = RegularWaveKillQuota(g_GameManager.currentWave, g_Difficulty);
            if (quota > 0) return (float)g_GameManager.waveKills >= (float)quota * v;
            return s.enemiesLeft >= 1 && s.enemiesLeft <= 3 + 2 * ease;
        }
        case CCond::Boss:    return s.bossUp || v >= 1.5f || (eased && IsEliteWaveNumber(g_GameManager.currentWave, g_Difficulty));
        case CCond::Far:     return s.enemiesLeft > 0 && s.nearest > Rings(v);
        case CCond::Barrage: return s.nearBullets >= (int)v;
        case CCond::Clean:   return s.waveDamage < std::max(0.01f, v);
        case CCond::Hurt:    return s.sinceHurt < v;
        case CCond::Combo:   return s.combo >= (int)v;
        case CCond::Long:    return s.waveAge >= v;
    }
    return false;
}

void UpdateChances(const passive::StepInput& in) {
    for (float& c : s.chance) c = 0.0f;
    for (float& c : s.chanceBig) c = 0.0f;
    for (int& c : s.chanceBigLv) c = 0;
    int wantCrit = 0;
    if (!s.chanceRows.empty()) {
        s.crowd = 0; s.enemiesLeft = 0; s.nearest = 1e9f; s.bossUp = false; s.nearBullets = 0;
        ForEachEnemy([&](auto& e, float ex, float ey) {
            ++s.enemiesLeft;
            const float d = std::hypot(ex - in.px, ey - in.py);
            s.nearest = std::min(s.nearest, d);
            if (d < Rings(4)) ++s.crowd;
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) if (IsBoss(e)) s.bossUp = true;   // elites too
        });
        for (const int r : s.chanceRows)
            if (kChance[r].cond == CCond::Barrage) {
                for (const Bullet& b : g_Bullets)
                    if (b.active && b.isEnemy && std::hypot(b.x - in.px, b.y - in.py) < Rings(2.5f)) ++s.nearBullets;
                break;
            }
        for (size_t k = 0; k < s.chanceRows.size(); ++k) {
            const int r = s.chanceRows[k];
            const bool holds = CondHolds(kChance[r], in, k < s.chanceEase.size() ? s.chanceEase[k] : 0);
            if (holds) {
                const CKind kind = kChance[r].kind;
                float p = (float)kChance[r].pct / 100.0f;
                bool big = false;
                if (k < s.chanceUp.size() && s.chanceUp[k] > 0) {
                    // Upgraded: x1.5 per pick while that stays within 100%, then the effect grows.
                    const synergy::ChanceAState st = synergy::ChanceAAt(kChance[r].pct, s.chanceUp[k]);
                    p = (float)st.pct / 100.0f;
                    big = st.effect > 0;
                    if (big) s.chanceBigLv[(int)kind] = std::max(s.chanceBigLv[(int)kind], st.effect);
                }
                if (big && kind == CKind::Crit) { s.chance[(int)kind] += p; s.chanceBig[(int)kind] += 1.0f; }
                else if (big) s.chanceBig[(int)kind] += p;
                else s.chance[(int)kind] += p;
            }
            // A small line ring round the ship when a chance card's condition comes on (not too often).
            if (holds && !s.chanceWas[r] && s.clock - s.chanceFxAt[r] > 1.5f) {
                static const float kCol[6][3] = { { 1.0f, 0.35f, 0.45f }, { 1.0f, 0.85f, 0.35f }, { 0.45f, 0.9f, 1.0f },
                                                  { 0.5f, 1.0f, 0.6f }, { 0.8f, 0.55f, 1.0f }, { 1.0f, 0.6f, 0.25f } };
                const float* c = kCol[(int)kChance[r].kind];
                s.chanceFxAt[r] = s.clock;
                Pulse(in.px, in.py, 44.0f, 0.35f, c[0], c[1], c[2]);
            }
            s.chanceWas[r] = holds;
        }
        wantCrit = (int)std::lround(s.chance[(int)CKind::Crit] * 100.0f);
    }
    g_Stats.critChance = std::max(0, g_Stats.critChance + wantCrit - s.appliedChanceCrit);   // unclamped sum, see Rebuild
    s.appliedChanceCrit = wantCrit;
    // An upgraded 100% crit card: more crit damage while its condition holds.
    const float wantCritMul = synergy::kChanceUpCritMul * (float)s.chanceBigLv[(int)CKind::Crit];
    g_Stats.critMult += wantCritMul - s.appliedChanceCritMul;
    s.appliedChanceCritMul = wantCritMul;
}

// What a shell that lands (instead of hitting one enemy) still does with its verbs.
void Landed(const Bullet& b, float radius) {
    const float x = b.synTx, y = b.synTy;
    const bool mark = (b.synFlags & synf::Mark) != 0, burn = (b.synFlags & synf::Burn) != 0;
    if (mark || burn)
        ForEachEnemy([&](auto& e, float ex, float ey) {
            if ((ex - x) * (ex - x) + (ey - y) * (ey - y) > radius * radius) return;
            if (mark) s.marks[&e] = 5.0f;
            if (burn) {   // refresh the 3s without restarting the 0.5s tick (a reset tick never burned)
                auto [it, fresh] = s.burns.try_emplace(&e, Burn{ 3.0f, 0.5f });
                if (!fresh) it->second.t = 3.0f;
            }
        });
    if (b.synFlags & synf::Gravity) Explode(x, y, s.on.Y_SINGULAR ? Rings(1.5f) : 75.0f, s.on.Y_SINGULAR ? 150.0f * b.dmgMult * s.dmg : 0.0f, true, s.on.Y_SINGULAR ? Rings(2.5f) : 0.0f);
    if (b.splitCount > 0 && s.pending.size() < 64) {
        const int n = std::min(b.splitCount, 5);
        for (int i = 0; i < n; ++i) {
            Bullet c = MakeShot(x, y, 2.0f * kPi * (float)i / (float)n + 0.3f, b.splitDmg, 520.0f);
            s.pending.push_back(c);
        }
    }
}

// Enemy shots within `radius` of the segment (x0, y0)-(x1, y1) are erased; returns how many.
int ClearAlong(float x0, float y0, float x1, float y1, float radius) {
    const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    int n = 0;
    for (Bullet& b : g_Bullets) {
        if (!b.active || !b.isEnemy) continue;
        const float t = len2 > 1e-4f ? std::clamp(((b.x - x0) * dx + (b.y - y0) * dy) / len2, 0.0f, 1.0f) : 0.0f;
        const float ex = b.x - (x0 + dx * t), ey = b.y - (y0 + dy * t);
        if (ex * ex + ey * ey > radius * radius) continue;
        b.active = false;
        ++n;
        SpawnSparks(b.x, b.y, 2, 0.75f, 0.95f, 1.0f, 140.0f);
    }
    if (n > 0) Audio::Play(Audio::Cue::SfxBulletErase);
    return n;
}

void Explode(float x, float y, float radius, float pct, bool pull, float pullRadius) {
    const line_fx::Rgb grav{ 0.62f, 0.45f, 1.0f };
    // 특이점 붕괴 탄두 (mythic) is told apart from 소형 중력탄 by shape, not colour: the reach
    // closes in on the point first, then it bursts with a ring thrown out (the burst drawn
    // a moment later; its damage lands now). Under a barrage one gather per place and moment.
    const bool singular = pull && s.on.Y_SINGULAR;
    bool gather = false;
    if (singular) {
        gather = s.clock - s.singularT > 0.15f || std::hypot(x - s.singularX, y - s.singularY) > Rings(1);
        if (gather) { s.singularT = s.clock; s.singularX = x; s.singularY = y; }
    }
    if (pull) {
        const float pr = pullRadius > 0.0f ? pullRadius : radius * 1.6f;
        if (!singular)  strike_fx::Vortex(x, y, pr, grav);   // a small black hole pulling from its reach
        else if (gather) {
            strike_fx::Implode(x, y, pr, grav, 0.0f, 0.14f);
            strike_fx::Implode(x, y, pr * 0.6f, line_fx::Lift(grav, 0.4f), 0.03f, 0.11f);
        }
        ForEachEnemy([&](auto& e, float ex, float ey) {
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) {
                if (IsBoss(e)) return;
                const float dx = x - ex, dy = y - ey, d = std::sqrt(dx * dx + dy * dy);
                if (d < 1.0f || d > pr) return;
                const float step = std::min(d, Rings(1));
                e.worldX += dx / d * step; e.worldY += dy / d * step;
            }
        });
    }
    if (singular) {
        Burst(x, y, radius, pct, grav, strike_fx::BlastStyle::Gravity, 0.12f);
        if (gather) strike_fx::Nova(x, y, radius, grav, 0.12f);
    }
    else if (pull) Burst(x, y, radius, pct, grav, strike_fx::BlastStyle::Gravity);
    else           Burst(x, y, radius, pct);
}

}  // namespace

// ───────────────────────── registration ─────────────────────────

void Reset() { s = State{}; g_PlayerDmgMult = 1.0f; g_FateCrits = false; }

void ForgetEnemy(const void* enemy) {
    s.seen.erase(enemy);
    s.chainStruck.erase(enemy);
    s.singularAt.erase(enemy);
    s.marks.erase(enemy);
    s.burns.erase(enemy);
    s.laserBurns.erase(enemy);
    s.laserProcAt.erase(enemy);
}

Streak* FindStreak(int id) {
    for (Streak& st : s.streaks) if (st.id == id) return &st;
    return nullptr;
}

Streak& NewStreak(Bullet& b) {
    if ((int)s.streaks.size() >= kMaxStreaks) s.streaks.erase(s.streaks.begin());   // the oldest goes
    b.synTrailChain = ++s.trailChains;
    Streak st;
    st.id = b.synTrailChain;
    st.pts.push_back({ b.x, b.y, 0.0f });
    st.lastX = b.x; st.lastY = b.y;
    st.dirX = b.dirX; st.dirY = b.dirY;
    st.cr = b.color.r; st.cg = b.color.g; st.cb = b.color.b;
    s.streaks.push_back(std::move(st));
    return s.streaks.back();
}

// Once per step for every shot with Afterimage Trail.
void RecordStreak(Bullet& b, float dt) {
    Streak* st = b.synTrailChain > 0 ? FindStreak(b.synTrailChain) : nullptr;
    if (!st) { NewStreak(b); return; }
    const float moved = std::hypot(b.x - st->lastX, b.y - st->lastY);
    if (moved > std::max(40.0f, b.speed * dt * 3.0f)) {   // a jump, not a flight: close this tail, start anew
        st->live = false;
        NewStreak(b);
        return;
    }
    st->seen = true;
    // A sharp turn (a bounce): pin the corner where the shot was just before it.
    const float dot = b.dirX * st->dirX + b.dirY * st->dirY;
    if (dot < kStreakTurnCos) {
        const StreakPt& last = st->pts.back();
        if (std::hypot(st->lastX - last.x, st->lastY - last.y) > 1.0f) st->pts.push_back({ st->lastX, st->lastY, 0.0f });
        st->dirX = b.dirX; st->dirY = b.dirY;
    }
    const StreakPt& last = st->pts.back();
    if (std::hypot(b.x - last.x, b.y - last.y) >= kStreakStep) {
        st->pts.push_back({ b.x, b.y, 0.0f });
        st->dirX = b.dirX; st->dirY = b.dirY;
    }
    if ((int)st->pts.size() > kMaxStreakPts) st->pts.erase(st->pts.begin());
    st->lastX = b.x; st->lastY = b.y;
}

// Ages the tails (points expire one by one, so a tail shortens from its old
// end) and lets stardust peel off them.
void StepStreaks(float dt) {
    for (Streak& st : s.streaks) {
        if (!st.seen) st.live = false;
        st.seen = false;
        if (!st.live) st.sinceLive += dt;
        for (StreakPt& p : st.pts) p.age += dt;
        size_t drop = 0;
        while (drop < st.pts.size() && st.pts[drop].age >= kTrailLife) ++drop;
        if (drop) st.pts.erase(st.pts.begin(), st.pts.begin() + (long long)drop);
        // Stardust: now and then a small cross leaves the tail's edge and drifts outward.
        for (size_t i = 1; i < st.pts.size(); ++i) {
            if ((int)s.motes.size() >= kMaxMotes || Rand01() >= dt * kMoteRate) continue;
            const StreakPt& p0 = st.pts[i - 1];
            const StreakPt& p1 = st.pts[i];
            const float dx = p1.x - p0.x, dy = p1.y - p0.y, len = std::hypot(dx, dy);
            if (len < 0.5f) continue;
            const float ux = dx / len, uy = dy / len;
            const float k = std::clamp(1.0f - p1.age / kTrailLife, 0.0f, 1.0f);
            const float w = kTrailZoneR * kTrailHalfWidth * kTrailThin * k;   // the edge of the lingering tail
            const float side = Rand01() < 0.5f ? -1.0f : 1.0f;
            const float nx = -uy * side, ny = ux * side;
            const float sp = 18.0f + 16.0f * Rand01();
            s.motes.push_back({ p1.x + nx * w, p1.y + ny * w, nx * sp - ux * 10.0f, ny * sp - uy * 10.0f,
                                kMoteLife * (0.7f + 0.3f * Rand01()), st.cr, st.cg, st.cb });
        }
    }
    s.streaks.erase(std::remove_if(s.streaks.begin(), s.streaks.end(),
                                   [](const Streak& st) { return st.pts.empty() || (!st.live && st.pts.size() < 2); }),
                    s.streaks.end());
    for (Mote& m : s.motes) { m.life -= dt; m.x += m.vx * dt; m.y += m.vy * dt; }
    s.motes.erase(std::remove_if(s.motes.begin(), s.motes.end(), [](const Mote& m) { return m.life <= 0.0f; }), s.motes.end());
}

// An open stroke through any number of points (miter::Path takes at most
// kMaxPoints), in overlapping pieces so it stays unbroken.
void LongPath(const std::vector<float>& x, const std::vector<float>& y, float w, float r, float g, float b, float a) {
    const int n = (int)x.size();
    constexpr int kPiece = miter::kMaxPoints;
    for (int start = 0; start + 1 < n; start += kPiece - 1) {
        const int m = std::min(kPiece, n - start);
        miter::Path(x.data() + start, y.data() + start, m, w, r, g, b, a);
    }
}

// Afterimage Trail: every tail drawn as a comet (see kTrail*): a head that is
// widest just behind the shot and closes round onto it, a thin tail behind,
// a faint wash, an outline round the head, a bright core and stardust. Many
// tails at once (a fast gun) would add up to a bright beam, so each gets
// fainter the more there are and the total stays about the same.
void DrawTrailStreaks() {
    namespace lf = line_fx;
    const float dim = std::clamp(2.2f / std::sqrt((float)std::max<size_t>(1, s.streaks.size())), 0.6f, 1.0f);
    const float half = kTrailZoneR * kTrailHalfWidth;
    std::vector<float> px, py, pk, ph, lx, ly, rx, ry, bx, by;
    for (const Streak& st : s.streaks) {
        // The points, with the shot itself as the live head.
        px.clear(); py.clear(); pk.clear();
        for (const StreakPt& p : st.pts) {
            px.push_back(p.x); py.push_back(p.y);
            pk.push_back(std::min(1.0f, (1.0f - p.age / kTrailLife) / 0.3f));   // fades over the last 30% of life
        }
        if (st.live && std::hypot(st.lastX - px.back(), st.lastY - py.back()) > 0.5f) {
            px.push_back(st.lastX); py.push_back(st.lastY); pk.push_back(1.0f);
        }
        const int n = (int)px.size();
        if (n < 2) continue;
        // Once the shot is gone (it hit something), the head melts into the thin
        // tail within a moment and the front draws out into a point.
        const float headFade = st.live ? 1.0f : std::exp(-st.sinceLive / kTrailHeadFade);
        // Distance back from the head along the path.
        ph.assign(n, 0.0f);
        std::vector<float> dist(n, 0.0f);
        for (int i = n - 2; i >= 0; --i) dist[i] = dist[i + 1] + std::hypot(px[i + 1] - px[i], py[i + 1] - py[i]);
        lx.assign(n, 0.0f); ly.assign(n, 0.0f); rx.assign(n, 0.0f); ry.assign(n, 0.0f);
        for (int i = 0; i < n; ++i) {
            const int a = std::max(0, i - 1), b = std::min(n - 1, i + 1);
            float dx = px[b] - px[a], dy = py[b] - py[a];
            const float len = std::hypot(dx, dy);
            if (len < 0.01f) { dx = 1.0f; dy = 0.0f; } else { dx /= len; dy /= len; }
            const float h = std::exp(-dist[i] / kTrailHead) * headFade;   // 1 at the shot, falling off behind it
            // The front closes round onto the shot (a teardrop, never a blunt box).
            const float tip = std::sqrt(std::min(1.0f, dist[i] / (st.live ? kTrailNose : kTrailTip)));
            const float w = half * (kTrailThin + (1.0f - kTrailThin) * h) * pk[i] * tip;
            ph[i] = h;
            lx[i] = px[i] - dy * w; ly[i] = py[i] + dx * w;
            rx[i] = px[i] + dy * w; ry[i] = py[i] - dx * w;
        }
        const float lr = st.cr + (1.0f - st.cr) * 0.6f, lg = st.cg + (1.0f - st.cg) * 0.6f, lb = st.cb + (1.0f - st.cb) * 0.6f;
        // Wash: strongest in the head, a thin glow behind it.
        for (int i = 1; i < n; ++i) {
            const float h = 0.5f * (ph[i - 1] + ph[i]), k = 0.5f * (pk[i - 1] + pk[i]);
            const float a = kTrailFill * dim * (0.3f + 0.7f * h) * k;
            if (a < 0.004f) continue;
            BatchTri(lx[i - 1], ly[i - 1], rx[i - 1], ry[i - 1], lx[i], ly[i], st.cr, st.cg, st.cb, a);
            BatchTri(rx[i - 1], ry[i - 1], rx[i], ry[i], lx[i], ly[i], st.cr, st.cg, st.cb, a);
        }
        // Outline: the head only, out one side, round the shot and back the other.
        {
            int h0 = n;
            while (h0 > 0 && ph[h0 - 1] > kTrailOutlineCut) --h0;
            if (n - h0 >= 2) {
                bx.clear(); by.clear();
                for (int i = h0; i < n; ++i) { bx.push_back(lx[i]); by.push_back(ly[i]); }
                for (int i = n - 1; i >= h0; --i) { bx.push_back(rx[i]); by.push_back(ry[i]); }
                LongPath(bx, by, 1.3f, st.cr, st.cg, st.cb, kTrailOutline * dim);
            }
        }
        // Core: brightest at the shot, in a few unbroken steps back along the tail.
        static const float kCut[5] = { 0.0f, 0.08f, 0.3f, 0.7f, 1.01f };   // by head share h
        static const float kCoreA[4] = { 0.10f, 0.22f, 0.45f, 1.0f };
        for (int band = 0; band < 4; ++band) {
            bx.clear(); by.clear();
            float kk = 0.0f;
            for (int i = 0; i < n; ++i)
                if (ph[i] >= kCut[band] - 0.02f && ph[i] <= kCut[band + 1]) { bx.push_back(px[i]); by.push_back(py[i]); kk += pk[i]; }
            if (bx.size() < 2) continue;
            LongPath(bx, by, 1.4f, lr, lg, lb, kTrailCore * dim * kCoreA[band] * (kk / (float)bx.size()));
        }
    }
    // Stardust: small crosses that shrink and fade as they drift off.
    for (const Mote& m : s.motes) {
        const float k = std::clamp(m.life / kMoteLife, 0.0f, 1.0f);
        const float sz = 0.6f + 2.2f * k;
        const lf::Rgb c{ m.cr + (1.0f - m.cr) * 0.6f, m.cg + (1.0f - m.cg) * 0.6f, m.cb + (1.0f - m.cb) * 0.6f };
        lf::Line(m.x - sz, m.y, m.x + sz, m.y, 1.1f, c, 0.8f * k * dim);
        lf::Line(m.x, m.y - sz, m.x, m.y + sz, 1.1f, c, 0.8f * k * dim);
    }
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
        lf::Circle(s.in.px, s.in.py, Rings(2), 1.2f, fc, 0.35f);
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
        if (z.chain == 0)
            lf::Circle(z.x, z.y, z.r, 1.2f, { z.cr, z.cg, z.cb }, 0.45f * std::min(1.0f, z.life / 0.3f));
    DrawTrailStreaks();
    // Mines: a small diamond in a diamond.
    for (const Mine& m : s.mines) {
        lf::Diamond(m.x, m.y, 9.0f, 1.5f, { 1.0f, 0.45f, 0.35f }, 0.95f);
        lf::Diamond(m.x, m.y, 4.0f, 1.0f, { 1.0f, 0.80f, 0.70f }, 0.8f);
    }
    for (const Bullet& b : g_Bullets) {
        if (!b.active || b.isEnemy || b.skillShot) continue;
        // 박격포: where the shell will land, a ring closing in as it falls, with four ticks round it.
        if ((b.synFlags & synf::Lob) && !(b.synFlags & synf::Orbit) && b.synLandDist > 0.0f) {
            const float u = std::clamp(b.traveled / b.synLandDist, 0.0f, 1.0f);
            const lf::Rgb c{ b.color.r, b.color.g, b.color.b };
            const float r = 18.0f - 8.0f * u, a = 0.45f + 0.45f * u;
            lf::Circle(b.synTx, b.synTy, r, 1.4f, c, a);
            for (int k = 0; k < 4; ++k) {
                const float ca = std::cos(kPi * 0.5f * (float)k), sa = std::sin(kPi * 0.5f * (float)k);
                lf::Line(b.synTx + ca * (r + 3.0f), b.synTy + sa * (r + 3.0f),
                         b.synTx + ca * (r + 9.0f), b.synTy + sa * (r + 9.0f), 1.4f, c, a);
            }
        }
        // 공중 정지탄: a held shot wears a ring, and a second one pulses out from it.
        if (b.synFlags & synf::Hovering) {
            const lf::Rgb c = lf::Lift({ b.color.r, b.color.g, b.color.b }, 0.4f);
            const float base = 6.0f * BulletDrawScale(b) + 6.0f;
            const float ph = std::fmod(s.clock * 2.5f, 1.0f);
            lf::Circle(b.x, b.y, base, 1.4f, c, 0.75f);
            lf::Circle(b.x, b.y, base * (1.0f + 0.8f * ph), 1.2f, c, 0.6f * (1.0f - ph));
        }
    }
    // 정밀한 의식: five short marks in an arc under the ship, one lit per stack (+8% each);
    // they run toward red as this wave's damage nears the 10 that breaks them.
    if (s.on.C_RITUAL) {
        const int lit = std::min(5, s.quietWaves);
        const float risk = std::clamp(s.waveDamage / 10.0f, 0.0f, 1.0f);
        const lf::Rgb on{ 1.0f, 0.85f - 0.55f * risk, 0.45f - 0.2f * risk }, off{ 0.8f, 0.75f, 0.6f };
        for (int k = 0; k < 5; ++k) {
            const float ang = kPi * 0.5f + 0.22f * (float)(k - 2);
            const float ca = std::cos(ang), sa = std::sin(ang);
            const bool l = k < lit;
            lf::Line(s.in.px + ca * 44.0f, s.in.py + sa * 44.0f, s.in.px + ca * 52.0f, s.in.py + sa * 52.0f,
                     l ? 2.0f : 1.0f, l ? on : off, l ? 0.9f : 0.3f);
        }
    }
    // 과부하 레이저 embers: a short streak that glows white-yellow, cools to orange, then dull red, and shrinks.
    for (const Ember& e : s.embers) {
        const float u = e.age / e.life;
        const float flick = 0.8f + 0.2f * std::sin(s.clock * 23.0f + e.seed);
        const lf::Rgb col{ 1.0f, std::max(0.18f, 0.85f - 0.75f * u), std::max(0.05f, 0.45f - 0.6f * u) };
        const float L = e.len * (1.0f - 0.6f * u);
        lf::Line(e.x - e.dx * L * 0.5f, e.y - e.dy * L * 0.5f, e.x + e.dx * L * 0.5f, e.y + e.dy * L * 0.5f,
                 1.8f - 0.8f * u, col, (1.0f - u) * flick);
        if (u < 0.35f)   // fresh: a hot white core along the streak
            lf::Line(e.x - e.dx * L * 0.3f, e.y - e.dy * L * 0.3f, e.x + e.dx * L * 0.3f, e.y + e.dy * L * 0.3f,
                     1.0f, { 1.0f, 0.95f, 0.8f }, (1.0f - u / 0.35f) * flick);
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
}

// Pairs where one card leaves the other nothing to act on. Neither side of such a pair is offered
// once the other is owned.
//   - The mortar's shell flies over walls and lands where it is aimed: bounce / pierce / return verbs,
//     and the on-hit blast (the shell never hits; Landed() has its own blast), do nothing.
//   - Wave, laser and Relentless Hunter shots pierce every enemy already: +2 pierce does nothing, and a
//     ricochet only turns a shot that ends on a hit, so it never fires (the laser also drops it).
struct CardClash { const char* a; const char* b; };
static const CardClash kClashes[] = {
    { "F_MORTAR", "V_BOUNCE" }, { "F_MORTAR", "V_RICOCHET" }, { "F_MORTAR", "V_PIERCE" },
    { "F_MORTAR", "V_HUNTER" }, { "F_MORTAR", "V_BLAST" },
    { "F_MORTAR", "Y_ETERNAL" }, { "F_MORTAR", "T_ANCHOR" },   // no bounces to count / a fixed landing blast, no direct hits
    { "F_MORTAR", "T_CRITECHO" }, { "F_MORTAR", "Y_FATE" },    // the landing blast never crits
    { "F_WAVE", "V_PIERCE" }, { "F_WAVE", "V_RICOCHET" },
    { "F_LASER", "V_PIERCE" }, { "F_LASER", "V_RICOCHET" },
    // The beam does not fly (LaserFx LaySegments drops homing / return / hover / grow, and trail zones
    // follow the distance flown), so these leave the laser nothing to act on.
    { "F_LASER", "V_HOMING" }, { "F_LASER", "V_HUNTER" }, { "F_LASER", "V_GROW" },
    { "F_LASER", "V_HOVER" }, { "F_LASER", "V_TRAIL" },
    { "V_HUNTER", "V_PIERCE" }, { "V_HUNTER", "V_RICOCHET" },
};
// Chance cards: a shell never pierces, bounces or hits (pierce / bounce / blast chances do nothing),
// and a wave, laser or hunter shot pierces everything already (a pierce chance does nothing).
static bool ChanceClash(const std::string& chance, const std::string& other) {
    for (const ChanceRow& row : kChance) {
        if (chance != row.code) continue;
        if (other == "F_MORTAR") return row.kind == CKind::Pierce || row.kind == CKind::Bounce || row.kind == CKind::Blast;
        if (other == "F_WAVE" || other == "F_LASER" || other == "V_HUNTER") return row.kind == CKind::Pierce;
        return false;
    }
    return false;
}
static bool ClashesWithOwned(int card) {
    const std::string code = T()[card].code;
    for (const CardClash& c : kClashes) {
        if (code == c.a && s.own[Index(c.b)] > 0) return true;
        if (code == c.b && s.own[Index(c.a)] > 0) return true;
    }
    for (int j = 1; j <= kCards; ++j) {
        if (s.own[j] <= 0) continue;
        const std::string other = T()[j].code;
        if (ChanceClash(code, other) || ChanceClash(other, code)) return true;
    }
    return false;
}

bool CanOffer(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL || !AugIsSynergy(ALL_AUGS[augIdx].type)) return false;
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    if (i < 1 || i > kCards || s.own[i] > 0) return false;
    // Frames are picked once at run start and never come up in a reward (augment rework);
    // the run-start pick shows frames only.
    const bool frame = T()[i].role == synergy::Role::Frame;
    if (frame != s.framePick) return false;
    if (frame && s.frame != 0) return false;   // one frame per run
    if ((T()[i].needs & s.tags) != T()[i].needs) return false;                // a card that would do nothing
    return !ClashesWithOwned(i);
}

namespace {
const wchar_t* const kTagNames[3][synergy::kTagCount] = {
    { L"관통", L"튕김", L"분열", L"유도", L"폭발", L"광선", L"공전", L"연쇄", L"연사", L"장전", L"산탄", L"박자",
      L"처치", L"치명타", L"튕길 때", L"피격", L"정지", L"대시", L"웨이브", L"보호막", L"체력", L"잔상", L"왕복",
      L"유리대포", L"벽 충돌", L"체공", L"점사", L"화상", L"표식", L"주사위", L"인력", L"소거", L"지뢰", L"성장",
      L"저장", L"콤보", L"박자 사격", L"고각", L"광선 II" },
    { L"Pierce", L"Bounce", L"Split", L"Homing", L"Blast", L"Beam", L"Orbit", L"Chain", L"Rapid", L"Charge", L"Spread", L"Beat",
      L"Kill", L"Crit", L"On bounce", L"Hurt", L"Still", L"Dash", L"Wave", L"Shield", L"HP", L"Trail", L"Return",
      L"Glass", L"Wall hit", L"Hover", L"Burst", L"Burn", L"Mark", L"Dice", L"Pull", L"Erase", L"Mine", L"Grow",
      L"Store", L"Combo", L"Beat shot", L"Lob", L"Beam II" },
    { L"貫通", L"跳弾", L"分裂", L"誘導", L"爆発", L"光線", L"周回", L"連鎖", L"連射", L"装填", L"散弾", L"拍子",
      L"撃破", L"会心", L"跳ね時", L"被弾", L"静止", L"ダッシュ", L"ウェーブ", L"シールド", L"体力", L"残像", L"往復",
      L"ガラス砲", L"壁衝突", L"滞空", L"点射", L"火傷", L"刻印", L"サイコロ", L"引力", L"消去", L"地雷", L"成長",
      L"蓄積", L"コンボ", L"拍子射撃", L"高角", L"光線 II" } };
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
    if (s.on.G_LASTWALL && !s.lastWallGiven) { s.lastWallGiven = true; AddShield(UPA(G_LASTWALL)); }
    if (i == Index("G_MIRROR") && s.own[i] == 1) AddShield(UPA(G_MIRROR));   // once, when the card itself is taken
}

// ───────────────────────── upgrades (augment rework) ─────────────────────────

static int CardOf(int augIdx) {
    if (augIdx < 0 || augIdx >= AUG_TOTAL || !AugIsSynergy(ALL_AUGS[augIdx].type)) return -1;
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    return i >= 1 && i <= kCards ? i : -1;
}

bool IsFrame(int augIdx) {
    const int i = CardOf(augIdx);
    return i > 0 && T()[i].role == synergy::Role::Frame;
}

void SetFramePick(bool on) { s.framePick = on; }
bool HasFrame() { return s.frame != 0; }

static bool Upgradable(int i) {
    const synergy::Aug& a = T()[i];
    if (a.role == synergy::Role::Frame || a.rar == synergy::Rar::Mythic) return false;
    return a.role == synergy::Role::Chance || synergy::FindUp(a.code) != nullptr;
}

int MaxStage(int augIdx) {
    const int i = CardOf(augIdx);
    return i > 0 && Upgradable(i) ? 999 : 1;
}

static int Picks(int i, int branch) {
    Counts& c = branch == 0 ? s.nA : branch == 1 ? s.nB : s.nC;
    return CountsOf(c)[i - 1];
}

int BranchCount(int augIdx, int branch) {
    const int i = CardOf(augIdx);
    return i > 0 && branch >= 0 && branch <= 2 ? Picks(i, branch) : 0;
}

// 1 + every pick on the card (the card's upgrade level + 1).
int Stage(int augIdx) {
    const int i = CardOf(augIdx);
    if (i < 0 || s.own[i] <= 0) return 0;
    return 1 + Picks(i, 0) + Picks(i, 1) + Picks(i, 2);
}

int Branches(int augIdx) {
    const int i = CardOf(augIdx);
    if (i < 0) return 0;
    return (Picks(i, 0) ? 1 : 0) | (Picks(i, 1) ? 2 : 0) | (Picks(i, 2) ? 4 : 0);
}

static const ChanceRow* ChanceRowOf(int i) {
    for (int r = 0; r < kChanceRows; ++r)
        if (std::string(kChance[r].code) == T()[i].code) return &kChance[r];
    return nullptr;
}

// Owned and the branch can still grow: every branch can be taken again (2026-11-24) until
// its number reaches its limit.
bool CanTakeBranch(int augIdx, int branch) {
    const int i = CardOf(augIdx);
    if (i < 0 || branch < 0 || branch > 2 || s.own[i] <= 0 || !Upgradable(i)) return false;
    if (branch == 2) return CardFamily(i) != synergy::Family::None;
    if (Picks(i, branch) >= 250) return false;
    if (const ChanceRow* row = ChanceRowOf(i))
        return branch == 0 || synergy::ChanceCondCanGrow((synergy::ChCond)(int)row->cond, Picks(i, 1));
    const synergy::UpRow* r = synergy::FindUp(T()[i].code);
    if (!r) return false;
    return branch == 0 ? synergy::UpCanGrow(r->A, Picks(i, 0)) : (r->B.on() && synergy::UpCanGrow(r->B, Picks(i, 1)));
}

bool CanUpgrade(int augIdx) {
    for (int b = 0; b < 3; ++b) if (CanTakeBranch(augIdx, b)) return true;
    return false;
}

void Upgrade(int augIdx, int branch) {
    if (!CanTakeBranch(augIdx, branch)) return;
    const int i = CardOf(augIdx);
    Counts& c = branch == 0 ? s.nA : branch == 1 ? s.nB : s.nC;
    ++CountsOf(c)[i - 1];
    Rebuild();
}

// The branch's line for its next pick ("튕김 3 → 4"; a shape: its text, and its stack).
std::wstring UpgradeLine(int augIdx, int branch, int lang) {
    const int i = CardOf(augIdx);
    if (i < 0 || branch < 0 || branch > 2) return L"";
    lang = std::clamp(lang, 0, 2);
    const int n = branch == 2 ? s.shape[(int)CardFamily(i)] : Picks(i, branch);
    if (const ChanceRow* row = ChanceRowOf(i))
        return synergy::BranchLine(nullptr, true, (synergy::ChKind)(int)row->kind, (synergy::ChCond)(int)row->cond,
                                   row->pct, branch, lang, n);
    return synergy::BranchLine(synergy::FindUp(T()[i].code), false, synergy::ChKind::Crit, synergy::ChCond::Still, 0,
                               branch, lang, n);
}

// What the picks of a branch did so far, for the build panel ("튕김 2 → 4", "확률 40% → 90%").
std::wstring UpgradeSummary(int augIdx, int branch, int lang) {
    const int i = CardOf(augIdx);
    if (i < 0 || branch < 0 || branch > 2) return L"";
    lang = std::clamp(lang, 0, 2);
    const int n = Picks(i, branch);
    if (n <= 0) return L"";
    if (branch == 2) {
        wchar_t buf[24];
        swprintf(buf, 24, L" ×%d", s.shape[(int)CardFamily(i)]);
        return synergy::ShapeText(CardFamily(i), lang) + std::wstring(buf);
    }
    if (const ChanceRow* row = ChanceRowOf(i)) {
        if (branch == 0) {
            const synergy::ChanceAState st = synergy::ChanceAAt(row->pct, n);
            std::wstring out = synergy::UpPart(synergy::UpVal{ synergy::UpLabel::Chance, synergy::UpUnit::Pct,
                                                               (float)row->pct / 100.0f, (float)st.pct / 100.0f }, lang);
            if (st.effect > 0) {
                static const wchar_t* const k[3] = { L" · 효과 +%d단계", L" · effect +%d", L" · 効果 +%d段階" };
                wchar_t buf[32];
                swprintf(buf, 32, k[lang], st.effect);
                out += buf;
            }
            return out;
        }
        const synergy::ChCond c = (synergy::ChCond)(int)row->cond;
        const float a = synergy::ChanceCondAt(c, 0), b = synergy::ChanceCondAt(c, n);
        (void)a; (void)b;
        return synergy::ChanceBText(c, n - 1, lang);   // the last step (the condition as it is now)
    }
    const synergy::UpRow* r = synergy::FindUp(T()[i].code);
    if (!r) return L"";
    const synergy::UpVal& v = branch == 0 ? r->A : r->B;
    return synergy::UpPart(synergy::UpVal{ v.l, v.u, synergy::UpAt(v, 0), synergy::UpAt(v, n) }, lang);
}

int CardFamilyOf(int augIdx) {
    const int i = CardOf(augIdx);
    return i > 0 ? (int)CardFamily(i) : 0;
}

void Release(int augIdx) {
    const int i = SynergyIndex(ALL_AUGS[augIdx].type);
    if (i < 1 || i > kCards || s.own[i] <= 0) return;
    --s.own[i];
    if (s.own[i] <= 0) { CountsOf(s.nA)[i - 1] = 0; CountsOf(s.nB)[i - 1] = 0; CountsOf(s.nC)[i - 1] = 0; }
    Rebuild();
}

// ───────────────────────── fire path ─────────────────────────

void RefreshStats() { Rebuild(); }   // Rebuild applies differences only: safe to repeat

bool LaserFrame() { return s.on.F_LASER; }
int LaserTier() {
    if (!s.on.F_LASER) return 0;
    return s.on.A_LASER_OVERLOAD ? 3 : (s.on.A_LASER_FOCUS ? 2 : 1);
}

float FireIntervalMult() {
    if (!s.any) return 1.0f;
    float m = s.rate;
    if (s.on.T_BERSERK)   // branch B: the full bonus already at UPB of HP left
        m *= 1.0f + UPA(T_BERSERK) * std::clamp((1.0f - Hp01()) / std::max(0.05f, 1.0f - UPB(T_BERSERK)), 0.0f, 1.0f);
    return std::max(0.2f, m);
}

void Shape(const Bullet& base, float px, float py, float tx, float ty, std::vector<Bullet>& out) {
    out.clear();
    if (!s.any) { out.push_back(base); return; }
    const Flags& o = s.on;
    // 레이저: the gun ticks every 0.1 s while a beam stands, but one beam is one shot. Counters and
    // rolls are made when the beam opens and its later ticks reuse them.
    const bool newShot = !o.F_LASER || laser_fx::g_Main.beams != s.laserBeam;
    if (o.F_LASER) s.laserBeam = laser_fx::g_Main.beams;
    if (newShot) ++s.shots;
    // A beam held on re-rolls its chance cards and dice every kBeamRerollGap (their conditions are read again),
    // so a condition that ended (a dash's 1 s, standing still ...) does not live as long as the beam.
    constexpr float kBeamRerollGap = 1.0f;
    const bool reroll = o.F_LASER && !newShot && s.clock - s.beamRollT >= kBeamRerollGap;
    if (newShot || reroll) s.beamRollT = s.clock;
    // 정지 반응 / 박자 사격 x3 on a beam lasts only its first kBeamBoostTime (not as long as it is held).
    constexpr float kBeamBoostTime = 1.0f;
    const float beamBoost = (o.F_LASER && laser_fx::g_Main.onT > kBeamBoostTime) ? 1.0f : s.beamBoost;
    const float aim = std::atan2(base.dirY, base.dirX);
    int n = std::clamp((int)std::lround(s.bullets), 1, 16);
    const bool parallel = n == 2 && !o.F_SHOTGUN && !o.F_BURST;
    float width = 0.0f;
    if (o.F_SHOTGUN) width = 0.60f;
    else if (o.F_BURST) width = 0.14f;
    else if (n > 1 && !parallel) width = std::min(0.12f * (float)(n - 1), 0.7f);

    float boost = 1.0f;
    if (s.absorbed > 0.0f && o.V_ABSORB) {
        boost += std::min(UPB(V_ABSORB), s.absorbed * UPA(V_ABSORB));   // the card's "최대 +50%" (was +100% in code)
        s.absorbed = 0.0f;
    }
    if (newShot) {
        s.beamBoost = 1.0f;
        // 정지 반응: the x3 is spent; another full second still charges the next one.
        if (o.T_STILL && s.stillCharged) { s.beamBoost *= UPA(T_STILL); s.stillCharged = false; s.stillChargeT = 0.0f; }
        if (o.T_RHYTHM && ++s.rhythm >= std::max(2, (int)UPB(T_RHYTHM))) { s.rhythm = 0; s.beamBoost *= UPA(T_RHYTHM); Audio::Play(Audio::Cue::ProcCounterFull); }
    }
    // Shape (박자): a boosted shot (stillness, beat) deals +50%.
    boost *= beamBoost * (beamBoost > 1.0f ? 1.0f + 0.5f * ShapeLv(synergy::Family::Beat) : 1.0f);
    if (s.dashBoostT > 0.0f) boost *= 1.3f;
    if (o.T_GRUDGE && s.stored > 0.0f) {
        const float pct = std::min(UPB(T_GRUDGE) * 100.0f * UPA(T_GRUDGE), s.stored * 100.0f * UPA(T_GRUDGE));
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
        b.synBoost = boost;
        ApplyFrameLook(b, tx, ty);
        if (!o.F_LASER) ApplyVerbs(b, tx, ty, RollVerbs(b));
        else {
            if ((newShot || reroll) && i == 0) s.beamRoll = RollVerbs(b);
            ApplyVerbs(b, tx, ty, s.beamRoll);
        }
        if (o.T_ANCHOR && s.stillT > 0.5f) {
            b.sizeScale *= 1.0f + UPA(T_ANCHOR) * std::min(s.stillT, 4.0f);
            if (s.stillT > 1.0f) b.pierceLeft += (int)UPB(T_ANCHOR);
        }
        out.push_back(b);
    }
    // Side shots of the spray: weak, but they carry the verbs.
    if (o.V_SPRAY) {
        const int pairs = std::max(1, (int)UPB(V_SPRAY) / 2);   // branch B: two pairs
        for (int k = 0; k < pairs * 2; ++k) {
            const int side = (k % 2) ? 1 : -1;
            Bullet b = out.front();
            const float ang = aim + 0.22f * (float)(k / 2 + 1) * (float)side;
            b.dirX = std::cos(ang); b.dirY = std::sin(ang);
            b.dmgMult *= UPA(V_SPRAY);
            out.push_back(b);
        }
    }
    // Every few shots one extra bullet that inherits the verbs.
    if (newShot) s.beamNth = s.shots % std::max(2, (int)UPA(T_NTH)) == 0;
    if (o.T_NTH && s.beamNth) {
        for (int k = 0; k < std::max(1, (int)UPB(T_NTH)); ++k) {
            Bullet b = out.front();
            const float ang = aim + 0.06f * (float)(k % 2 ? -1 : 1) * (float)(k / 2 + 1);
            b.dirX = std::cos(ang); b.dirY = std::sin(ang);
            b.dmgMult *= 1.0f + 0.5f * ShapeLv(synergy::Family::Beat);
            out.push_back(b);
        }
    }
    // Chance of an extra bullet (the chance cards).
    if (newShot || reroll) {
        s.beamExtra.clear();
        for (float c = s.chance[(int)CKind::Extra]; c > 0.0f; c -= 1.0f)
            if (Rand01() < std::min(c, 1.0f)) s.beamExtra.push_back(Rand01() < 0.5f ? -0.08f : 0.08f);
        for (float c = s.chanceBig[(int)CKind::Extra]; c > 0.0f; c -= 1.0f)   // upgraded: 1 + level extra shots
            if (Rand01() < std::min(c, 1.0f))
                for (int k = 0; k <= s.chanceBigLv[(int)CKind::Extra]; ++k)
                    s.beamExtra.push_back(0.08f * (float)(k / 2 + 1) * (k % 2 ? 1.0f : -1.0f));
    }
    for (const float off : s.beamExtra) {
        Bullet b = out.front();
        const float ang = aim + off;
        b.dirX = std::cos(ang); b.dirY = std::sin(ang);
        b.dmgMult *= 1.0f + 0.5f * ShapeLv(synergy::Family::Beat);
        out.push_back(b);
    }
    // The opening salvo: every shot goes out twice for 3 s from the wave's first hit
    // (from the wave's start it was mostly spent before any enemy was in reach).
    if (o.T_OPENING && s.openingAt >= 0.0f && s.clock - s.openingAt < UPA(T_OPENING)) {
        const size_t m = out.size();
        const int copies = std::max(1, (int)UPB(T_OPENING) - 1);   // branch B: three shots each
        for (int c = 1; c <= copies; ++c)
            for (size_t i = 0; i < m; ++i) {
                Bullet b = out[i];
                const float ang = std::atan2(b.dirY, b.dirX) + 0.05f * (float)c;
                b.dirX = std::cos(ang); b.dirY = std::sin(ang);
                out.push_back(b);
            }
    }
    // Orbit: every fifth shot is held around the player instead of flying off.
    if ((o.V_ORBIT || o.Y_HALO) && newShot && ++s.orbitShots >= (o.Y_HALO ? 3 : 5)) {
        s.orbitShots = 0;
        const int cap = (o.V_ORBIT ? (int)UPA(V_ORBIT) : 2) + (o.Y_HALO ? 2 : 0);
        if (CountOrbiters() < cap) {
            Bullet b = out.front();
            b.synFlags |= synf::Orbit; b.synOrbitAngle = Rand01() * 2.0f * kPi; b.synOrbitLeft = o.Y_HALO ? 4.5f : UPB(V_ORBIT);
            if (o.Y_HALO) b.synFlags |= synf::Halo;
            b.speed = 0.0f; b.piercing = true; b.maxRange = 0.0f; b.sizeScale *= 1.2f;
            b.splitCount = 0;   // an orbiter hits again every 0.4 s; its split children inherited Orbit and broke the cap
            b.bouncesLeft = 0; b.synRicochet = 0;   // held near a screen edge it bounced off the wall, counting bounces and clearing its hits
            b.synFlags &= ~(synf::Lob | synf::Mine);   // a mortar shell held in orbit never hit (shells skip hits until they land)
            // It hits again every 0.4 s for 4.5 s: without the volley's one-shot bonuses (x3 stillness / beat).
            b.dmgMult /= std::max(1e-3f, b.synBoost); b.synBoost = 1.0f;
            b.x = px; b.y = py;
            out.push_back(b);
        }
    }
    // Mines: every third shot adds one that sticks in front of the aimed spot instead of flying on.
    if (o.V_MINE && newShot && ++s.mineShots >= std::max(1, (int)UPB(V_MINE))) {
        s.mineShots = 0;
        Bullet b = base;
        b.dmgMult = base.dmgMult * boost;
        b.synFlags |= synf::Mine;
        b.synTx = b.x + (tx - b.x) * 0.55f; b.synTy = b.y + (ty - b.y) * 0.55f;
        b.synLandDist = std::hypot(b.synTx - b.x, b.synTy - b.y);
        out.push_back(b);
    }
    // Echo: the same volley again a moment later.
    if (o.Y_ECHO && newShot) {   // a beam is one shot: one echo of it, not one per 0.1 s tick
        Echo e; e.t = 0.2f; e.shots = out;
        for (Bullet& shot : e.shots) shot.synFlags |= synf::Echo;
        s.echoes.push_back(std::move(e));
    }
}

float DamageMult(const Bullet& b, const void* target) {
    if (b.skillShot || !s.any) return 1.0f;
    const Flags& o = s.on;
    float m = s.dmg;
    auto mk = s.marks.find(target);
    if (mk != s.marks.end() && mk->second > 0.0f) m *= 1.0f + UPA(V_MARK);
    if (b.synFlags & synf::Grow) { const float g = UPA(V_GROW) - 1.0f; m *= 1.0f + std::min(g, b.traveled / (Rings(UPB(V_GROW)) / g)); }
    // Shape (관통): +15% for every enemy the shot has passed.
    if (ShapeOn(synergy::Family::Pierce) && b.synPierced > 0) m *= 1.0f + 0.15f * ShapeLv(synergy::Family::Pierce) * (float)b.synPierced;
    if (b.synBounces > 0) m *= 1.0f + (float)b.synBounces * ((o.Y_ETERNAL ? 0.22f : 0.0f) + (o.V_BOUNCE ? UPB(V_BOUNCE) : 0.0f));
    return m;
}

float ShotDamageMult() { return s.any ? s.dmg : 1.0f; }

// Every "피해 +X%" / "모든 피해" card: bullets, skills, blasts, burns, zones alike.
float CardDamageMult() {
    if (!s.any) return 1.0f;
    const Flags& o = s.on;
    float m = s.cardDmg * (1.0f + s.dmgAdd);
    if (o.T_COMBO) m *= 1.0f + std::min(UPA(T_COMBO), UPB(T_COMBO) * (float)s.combo);
    if (o.C_RITUAL) m *= 1.0f + std::min(5.0f * UPA(C_RITUAL), UPA(C_RITUAL) * (float)s.quietWaves);
    if (o.T_LASTSTAND && Hp01() <= UPB(T_LASTSTAND)) m *= 1.0f + UPA(T_LASTSTAND);
    return m;
}

float DamageTakenMult() {
    if (!s.any) return 1.0f;
    float m = 1.0f;
    if (s.on.C_ASCETIC) m *= 1.0f + UPA2(C_ASCETIC) + UPB(C_ASCETIC);
    if (s.on.C_RECKLESS) m *= 2.0f;
    if (s.on.C_REVIVE && s.revived) m *= 1.0f + UPB(C_REVIVE);
    if (s.on.G_CHARGER && g_PlayerShield > 0.0f) m *= 1.0f + UPB(G_CHARGER);
    return m;
}

// ───────────────────────── events ─────────────────────────

// 레이저: a beam lays new segments every 0.1 s, so each enemy on it is "hit" ten times a second. Hit procs
// (blasts, pulls, splits, echo shots, chance procs, crit counters) are rated per hit, not per second:
// a beam's segments may set them off on one enemy only once per kLaserProcGap. Damage, marks, burns,
// the beam's own burn stacks and kills still count on every hit.
constexpr float kLaserProcGap = 0.5f;
bool ProcGate(const Bullet& b, const void* target) {
    if (!(b.synFlags & synf::Hitscan)) return true;
    auto [it, fresh] = s.laserProcAt.try_emplace(target, s.clock);
    if (fresh) return true;
    if (s.clock - it->second < kLaserProcGap) return false;
    it->second = s.clock;
    return true;
}
bool ProcOk() { return s.procOk; }

void OnBulletHit(const Bullet& b, const void* target, float x, float y, bool crit, bool killed, float damage) {
    if (!s.any || b.skillShot) { s.procOk = true; return; }
    const Flags& o = s.on;
    s.procOk = ProcGate(b, target);
    if (s.openingAt < 0.0f) s.openingAt = s.clock;   // 개전 일제사격 starts on the wave's first hit
    // 운명의 필살 일격: a shot's crit cuts an X through the hit point; every other enemy in
    // its reach takes kFateCutShare of that crit. At most one cut per kFateCutGap (a fast gun
    // cannot multiply it). The cut's damage is plain: no crit, no on-hit, no execute.
    if (crit && o.Y_FATE && s.clock - s.fateCutT >= kFateCutGap) {
        s.fateCutT = s.clock;
        // End to end: most of the reach. Every cut shows, on the same spot again too (user
        // 2026-11-17), except on the hit that killed: its larger kill X (Juice.h SpawnFateKill)
        // is already there. At most 8 on screen.
        if (!killed && strike_fx::Live(strike_fx::Kind::Slash) < 8)
            strike_fx::Slash(x, y, kFateCutReach * 2.0f, { 1.0f, 0.25f, 0.35f });   // each arm reaches as far as the cut hurts
        const float share = damage * kFateCutShare;
        if (share > 0.0f)
            ForEachEnemy([&](auto& e, float ex, float ey) {
                if ((const void*)&e == target) return;
                const float dx = ex - x, dy = ey - y;
                if (dx * dx + dy * dy > kFateCutReach * kFateCutReach) return;
                const float dealt = g_MonsterManager.ApplyDamage(e, share);
                if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
            });
    }
    if (b.synFlags & synf::Mark) s.marks[target] = UPB(V_MARK);
    if (b.synFlags & synf::Burn) {   // refresh the 3s without restarting the 0.5s tick: hits faster than 0.5s never burned
        auto [it, fresh] = s.burns.try_emplace(target, Burn{ UPB(V_BURN), 0.5f });
        if (!fresh) it->second.t = UPB(V_BURN);
    }
    if ((b.synFlags & (synf::FrameLaser | synf::LaserBlast)) && o.A_LASER_OVERLOAD) {
        auto [it, fresh] = s.laserBurns.try_emplace(target, LaserBurn{ 0.0f, 0.25f, 0.0f, x, y, 0.0f, 0 });
        LaserBurn& lb = it->second;
        if (b.synFlags & synf::LaserBlast) lb.stacks = kLaserBurnMax;
        else if (lb.gain <= 0.0f) {
            // The wait carries its overshoot (beam ticks are 0.1 s apart): one stack per 0.25 s, not per 0.3 s.
            lb.stacks = std::min(kLaserBurnMax, lb.stacks + 1);
            lb.gain = kLaserBurnGain + std::max(lb.gain, -0.1f);
        }
        lb.t = kLaserBurnHold;
    }
    // On-hit blasts scale with the shot's own damage factor (frame, split child, side shot, laser tick),
    // as a mortar shell's landing already does: a flat % of attack made fast frames multiply them.
    const float shotMult = b.dmgMult * s.dmg;
    if (b.blastPct > 0.0f && s.procOk) {
        const float r = 80.0f * (1.0f + (o.V_BLAST ? UPB(V_BLAST) : 0.0f));
        Burst(x, y, r, b.blastPct * 100.0f * shotMult);
        // Shape (폭발): the same spot goes off again 0.5 s later at half the damage.
        if (ShapeOn(synergy::Family::Blast)) AddZone(x, y, r * 0.8f, 0.5f, 0.0f, b.blastPct * std::min(100.0f, 50.0f * ShapeLv(synergy::Family::Blast)) * shotMult);
    }
    if ((b.synFlags & synf::Gravity) && s.procOk) {
        // 특이점 붕괴 탄두: at most one implosion per enemy per 0.15 s (piercing, split and beam hits
        // made it go off on every hit and the damage grew with the square of the crowd).
        bool go = true;
        if (o.Y_SINGULAR) {
            auto [it, fresh] = s.singularAt.try_emplace(target, s.clock);
            if (!fresh) { if (s.clock - it->second < 0.15f) go = false; else it->second = s.clock; }
        }
        if (go) Explode(x, y, o.Y_SINGULAR ? Rings(1.5f) : 75.0f * UPA(V_GRAVITY) / 1.5f, o.Y_SINGULAR ? 150.0f * shotMult : UPB(V_GRAVITY) * 100.0f * shotMult, true, o.Y_SINGULAR ? Rings(2.5f) : 0.0f);
    }
    if (b.splitCount > 0 && !b.splitSpent && s.procOk && s.pending.size() < 64) {
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
            c.synTrailChain = 0; c.synTrailAt = c.traveled;   // its own tail, from the split point
            if (!b.piercing && c.pierceLeft > 0) --c.pierceLeft;   // the parent spends one on this hit
            if (b.synFlags & synf::Hitscan) {
                // A beam segment lies still from its start to the beam's end: its children
                // would copy that still line and sit at the far end. They fly from the hit instead.
                c.synFlags &= ~(synf::Hitscan | synf::FrameLaser);
                c.x = c.prevX = x; c.y = c.prevY = y;
                c.speed = g_Stats.bulletSpeed;
                c.maxRange = Rings(6.5f);
                c.traveled = 0.0f; c.synTrailAt = 0.0f;
                c.hitscanLeft = 0;
            }
            // Shape (분열): split shots curve toward the nearest enemy.
            if (ShapeOn(synergy::Family::Split) && !(c.synFlags & synf::Lob)) { c.homing = true; c.homingTurn = std::max(c.homingTurn, 3.0f * ShapeLv(synergy::Family::Split)); }
            c.ClearHits(); c.RememberHit(target);
            s.pending.push_back(c);
        }
    }
    // An echo shot's crit sets off no echo of its own: at a high crit chance they chained without end.
    if (crit && s.procOk && o.T_CRITECHO && !(b.synFlags & synf::CritEchoShot)) {
        // From the ship at the enemy nearest to it (the hit spot if none is left).
        float ex = x, ey = y;
        g_MonsterManager.FindNearestEnemy(s.in.px, s.in.py, ex, ey);
        const int shots = std::max(1, (int)UPB(T_CRITECHO));   // branch B: two
        for (int k = 0; k < shots; ++k) {
            Bullet e = MakeShot(s.in.px, s.in.py, std::atan2(ey - s.in.py, ex - s.in.px) + 0.08f * (float)k, UPA(T_CRITECHO), 0.0f);
            e.bouncesLeft = b.bouncesLeft; e.pierceLeft = b.pierceLeft;
            e.synFlags |= synf::CritEchoShot;
            s.pending.push_back(e);
        }
    }
    // Shape (치명): a crit sets off a small blast, at most every 0.1 s.
    if (crit && s.procOk && ShapeOn(synergy::Family::Crit) && s.clock - s.critBlastAt >= 0.1f) {
        s.critBlastAt = s.clock;
        Burst(x, y, 55.0f, 40.0f * ShapeLv(synergy::Family::Crit) * shotMult);
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
    if (s.on.V_RICOCHET) b.dmgMult *= 1.0f + UPB(V_RICOCHET);   // branch B
    OnBounce(b, false);   // a turn off a target is a bounce for the bounce cards too
    return true;
}

void OnBounce(Bullet& b, bool wall) {
    if (!s.any) return;
    ++b.synBounces;
    const Flags& o = s.on;
    (void)wall;
    // Shape (튕김): a small blast at every bounce.
    if (ShapeOn(synergy::Family::Bounce) && !b.isEnemy) Burst(b.x, b.y, 60.0f, 30.0f * ShapeLv(synergy::Family::Bounce) * b.dmgMult * s.dmg);
    if (o.Y_ETERNAL && !b.isEnemy) {
        // 영겁의 반사탄: each bounce adds +22%, so each one shows — a ring more on the shot
        // (EntityDraw DrawCardMarks) and a small crystal spark where it turned, in the shot's colour.
        b.synFlags |= synf::Eternal;
        if (s.bounceFx < 6 && passive::OnScreen(b.x, b.y)) {
            ++s.bounceFx;
            const float k = std::min(1.0f, 0.2f * (float)b.synBounces);
            const line_fx::Rgb c{ b.color.r, b.color.g, b.color.b };
            strike_fx::Blast(b.x, b.y, 20.0f + 4.0f * (float)std::min(b.synBounces, 4), line_fx::Lift(c, k),
                             0.0f, strike_fx::BlastStyle::Crystal);
        }
    }
}

void OnHurt(float dmg) {
    if (!s.any || dmg <= 0.0f) return;
    const Flags& o = s.on;
    s.waveDamage += dmg;
    s.sinceHurt = 0.0f;
    if (s.waveDamage >= (o.C_RITUAL ? UPB(C_RITUAL) : 10.0f)) s.quietWaves = 0;
    if (o.T_GRUDGE) s.stored = std::min(UPB(T_GRUDGE), s.stored + dmg);
    if (o.T_REVENGE) {
        s.revengeAcc += dmg;
        if (s.revengeAcc >= UPB(T_REVENGE)) {
            s.revengeAcc -= UPB(T_REVENGE);   // the overflow counts toward the next one
            strike_fx::Implode(s.in.px, s.in.py, 120.0f, { 1.0f, 0.40f, 0.30f }, 0.0f, 0.16f);
            passive::ClearEnemyBullets(s.in.px, s.in.py, Rings(4));
            passive::DamageRadius(s.in.px, s.in.py, Rings(4), UPA(T_REVENGE) * 100.0f, 1.0f, 0.4f, 0.3f);
        }
    }
    if (o.T_CAKE && s.cakeUsed + s.cakeArmed < (int)UPB(T_CAKE)) {
        s.cakeAcc += dmg;
        if (s.cakeAcc >= 20.0f) { s.cakeAcc -= 20.0f; ++s.cakeArmed; }   // the overflow counts toward the next one
    }
}

void OnDashStarted() {
    if (!s.any) return;
    const Flags& o = s.on;
    s.dashDropped = 0.0f; s.dashLastX = s.in.px; s.dashLastY = s.in.py;
    s.sinceDash = 0.0f;
    if (o.C_RECKLESS)
    {
        g_PlayerRuntime.dashCooldown = std::max(0.4f, g_PlayerRuntime.dashCooldown - UPB(C_RECKLESS));
        g_PlayerRuntime.dashCooldownMax = g_PlayerRuntime.dashCooldown;   // the HUD ring starts empty, not half full
    }
    if (o.G_PARRY) g_PlayerRuntime.dashInvulnerability = std::max(g_PlayerRuntime.dashInvulnerability, UPA(G_PARRY));
}

void OnWaveCleared() {
    if (!s.any) return;
    const Flags& o = s.on;
    if (o.G_CHARGER) AddShield(UPA(G_CHARGER));
    if (o.G_FORCEFIELD) AddShield(UPA(G_FORCEFIELD));
    // Only while Precise Ritual is owned: quiet waves banked before the pick made it start at up to +40%.
    if (o.C_RITUAL && s.waveDamage < UPB(C_RITUAL)) s.quietWaves = std::min(8, s.quietWaves + 1);
    s.waveDamage = 0.0f;
    if (o.T_CAKE && s.cakeArmed > 0) {
        --s.cakeArmed; ++s.cakeUsed;
        g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + UPA(T_CAKE));
        Audio::Play(Audio::Cue::SfxHealSmall);
    }
}

// A saved death: a moment of invulnerability and the shots round the ship erased, so the next
// bullet does not finish the job.
static void Saved() {
    g_PlayerRuntime.dashInvulnerability = std::max(g_PlayerRuntime.dashInvulnerability, 0.75f);
    passive::ClearEnemyBullets(s.in.px, s.in.py, Rings(3));
}

bool TryRevive() {
    // 최후의 방벽 blocks one lethal hit per run before any revive card is spent:
    // the hit is cut down to leave 1 HP, with a shield break around the ship.
    if (s.on.G_LASTWALL && s.lastWallSaves < (int)UPB(G_LASTWALL)) {
        ++s.lastWallSaves;
        s.lastWallSaved = true;
        Audio::Play(Audio::Cue::SfxShieldBreak);
        strike_fx::Implode(s.in.px, s.in.py, 120.0f, { 0.55f, 0.85f, 1.0f }, 0.0f, 0.30f);
        Pulse(s.in.px, s.in.py, 120.0f, 0.45f, 0.55f, 0.85f, 1.0f);
        g_GameManager.playerHP = 1.0f;
        Saved();
        return true;
    }
    if (!s.on.C_REVIVE || s.revived) return false;
    s.revived = true;
    Audio::Play(Audio::Cue::SfxRevive);
    strike_fx::Revive(s.in.px, s.in.py, { 0.55f, 1.0f, 0.75f });
    g_GameManager.playerHP = std::min(g_Stats.maxHP, UPA(C_REVIVE));
    Saved();
    return true;
}

// ───────────────────────── per step ─────────────────────────

// 과부하 레이저 burns: damage per stack, and the embers a burning enemy leaves where it goes.
// The embers stay where they fell (no rising sparks, no tint on the enemy): the path it burned along.
void StepLaserBurns(float dt) {
    for (Ember& e : s.embers) e.age += dt;
    s.embers.erase(std::remove_if(s.embers.begin(), s.embers.end(), [](const Ember& e) { return e.age >= e.life; }), s.embers.end());
    if (s.laserBurns.empty()) return;
    const float perStack = passive::AttackPower() * kLaserBurnPct * 0.25f * passive::AllDamageMult();   // one 0.25s tick
    ForEachEnemy([&](auto& e, float ex, float ey) {
        auto it = s.laserBurns.find(&e);
        if (it == s.laserBurns.end()) return;
        LaserBurn& lb = it->second;
        lb.gain -= dt;
        lb.t -= dt;
        if (lb.t <= 0.0f) { --lb.stacks; lb.t += kLaserBurnDrop; }
        if (lb.stacks <= 0) return;
        lb.tick -= dt;
        if (lb.tick <= 0.0f) {
            lb.tick += 0.25f;
            const float dealt = g_MonsterManager.ApplyDamage(e, perStack * (float)lb.stacks);
            if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
        }
        // Embers: one per few px moved (denser with more stacks), and a slow drip while standing still.
        const float mx = ex - lb.lx, my = ey - lb.ly, moved = std::sqrt(mx * mx + my * my);
        lb.lx = ex; lb.ly = ey;
        if (moved > 200.0f) return;   // teleported: no streak across the screen
        const float k = (float)lb.stacks / (float)kLaserBurnMax;
        float body = 28.0f;   // ranged mobs: about a regular mob's body
        if constexpr (std::is_same_v<std::decay_t<decltype(e)>, Monster>) body = 26.0f * e.sizeScale;
        lb.trail += moved + dt * 30.0f;
        const float every = 22.0f - 10.0f * k;
        const float ux = moved > 0.01f ? mx / moved : 0.0f, uy = moved > 0.01f ? my / moved : 0.0f;
        while (lb.trail >= every && (int)s.embers.size() < kMaxEmbers) {
            lb.trail -= every;
            const float ang = Rand01() * 2.0f * kPi;
            const float rr = body * 0.6f * std::sqrt(Rand01());   // within 0.6x the body (user 2026-11-16)
            Ember em;
            em.x = ex + std::cos(ang) * rr - ux * lb.trail;
            em.y = ey + std::sin(ang) * rr - uy * lb.trail;
            // Streaks lie along the way the enemy went (random when it stands still).
            const float sa = moved > 0.01f ? std::atan2(uy, ux) + (Rand01() - 0.5f) * 0.8f : Rand01() * 2.0f * kPi;
            em.dx = std::cos(sa); em.dy = std::sin(sa);
            em.len = 4.0f + 4.0f * Rand01() + 3.0f * k;
            em.age = 0.0f;
            em.life = 0.40f + 0.45f * k + 0.2f * Rand01();
            em.seed = Rand01() * 6.28f;
            s.embers.push_back(em);
        }
        if (lb.trail > every) lb.trail = every;
    });
    for (auto it = s.laserBurns.begin(); it != s.laserBurns.end();) it = it->second.stacks <= 0 ? s.laserBurns.erase(it) : std::next(it);
}

void Step(const passive::StepInput& in) {
    s.in = in;
    s.bounceFx = 0;
    // Ring pulses age even without synergy cards (a pickup of any card shows one).
    for (RingFx& r : s.rings) r.age += in.dt;
    s.rings.erase(std::remove_if(s.rings.begin(), s.rings.end(),
        [](const RingFx& r) { return r.age >= r.life; }), s.rings.end());
    if (!s.any) { g_PlayerDmgMult = 1.0f; return; }   // no card: a past run's taken-damage factor must not linger
    const Flags& o = s.on;
    const float dt = in.dt;
    s.clock += dt;
    s.stillGuardCd = std::max(0.0f, s.stillGuardCd - dt);
    s.dashBoostT = std::max(0.0f, s.dashBoostT - dt);
    s.sinceDash += dt; s.sinceHurt += dt; s.sinceKill += dt;
    s.sinceShield = g_PlayerShield > 0.0f ? 0.0f : s.sinceShield + dt;
    UpdateChances(in);
    g_PlayerDmgMult = DamageTakenMult();

    if (g_GameManager.currentWave != s.lastWave) {
        // A new wave starts clean: hits taken between the clear and the card pick were counted
        // against the next wave and broke its 무결 cards.
        s.lastWave = g_GameManager.currentWave; s.waveAge = 0.0f; s.waveDamage = 0.0f; s.openingAt = -1.0f;
    }
    else s.waveAge += dt;

    // Stillness.
    if (in.moving) { s.stillT = 0.0f; s.stillChargeT = 0.0f; s.stillCharged = false; }
    else {
        s.stillT += dt; s.stillChargeT += dt;
        if (o.T_STILL && s.stillChargeT >= UPB(T_STILL)) s.stillCharged = true;
        if (o.G_STILLGUARD && s.stillT >= 2.0f && s.stillGuardCd <= 0.0f && g_PlayerShield < UPA(G_STILLGUARD)) {
            AddShield(UPA(G_STILLGUARD) - g_PlayerShield); s.stillGuardCd = UPB(G_STILLGUARD);
        }
    }
    // Per-enemy timers kept only as long as they matter.
    for (auto it = s.chainStruck.begin(); it != s.chainStruck.end();) it = s.clock - it->second > 1.0f ? s.chainStruck.erase(it) : std::next(it);
    for (auto it = s.singularAt.begin(); it != s.singularAt.end();) it = s.clock - it->second > 1.0f ? s.singularAt.erase(it) : std::next(it);
    // Combo.
    if (s.comboT > 0.0f) { s.comboT -= dt; if (s.comboT <= 0.0f) s.combo = 0; }

    // Dash: the path it leaves, and what happens when it ends.
    if (in.dashActive && o.T_AFTERIMAGE) {
        s.dashDropped += std::hypot(in.px - s.dashLastX, in.py - s.dashLastY);
        s.dashLastX = in.px; s.dashLastY = in.py;
        if (s.dashDropped >= 40.0f) { s.dashDropped = 0.0f; AddZone(in.px, in.py, 34.0f, UPB(T_AFTERIMAGE), UPA(T_AFTERIMAGE) * 100.0f, 0.0f, 0.75f, 0.55f, 1.0f); }
    }
    if (s.prevDash && !in.dashActive) DashEnded();
    s.prevDash = in.dashActive;

    // Shield: breaking it.
    if (s.shieldPrev > 0.0f && g_PlayerShield <= 0.0f) {
        Audio::Play(Audio::Cue::SfxShieldBreak);
        // Shape (방벽): a breaking shield erases the enemy shots around the ship.
        if (ShapeOn(synergy::Family::Guard)) passive::ClearEnemyBullets(in.px, in.py, Rings(2.5f + 1.0f * ShapeLv(synergy::Family::Guard)));
        if (o.G_MIRROR)
            for (Bullet& b : g_Bullets)
                if (b.active && b.isEnemy &&
                    (b.x - in.px) * (b.x - in.px) + (b.y - in.py) * (b.y - in.py) < Rings(UPB(G_MIRROR)) * Rings(UPB(G_MIRROR))) {
                    b.isEnemy = false; b.dirX = -b.dirX; b.dirY = -b.dirY; b.dmgMult = 1.0f;
                    b.color = glm::vec3(0.7f, 0.9f, 1.0f);
                }
    }
    s.shieldPrev = g_PlayerShield;

    // Graze: 30 bullets that miss by a hair give a shield.
    if (o.T_GRAZE) {
        const float hitR = 10.0f * g_Stats.playerSizeMult;
        for (Bullet& b : g_Bullets) {
            // Its own mark (not Bullet::grazed), so Graze Recovery still heals on the same bullet.
            if (!b.active || !b.isEnemy || (b.synFlags & synf::Grazed)) continue;
            // A shot counts once it has been in the band and leaves it outward still flying: counted on
            // entry, almost every shot that then hit the player was a "graze" the step before.
            const float d = std::hypot(b.x - in.px, b.y - in.py);
            if (d > hitR && d < hitR + 18.0f) {
                b.synFlags |= synf::GrazeNear;
            } else if ((b.synFlags & synf::GrazeNear) && d >= hitR + 18.0f) {
                b.synFlags |= synf::Grazed;
                if (++s.grazes >= std::max(1, (int)UPB(T_GRAZE))) { s.grazes = 0; AddShield(UPA(T_GRAZE)); Audio::Play(Audio::Cue::ProcCounterFull); }
            }
        }
    }
    // Shape (궤도): orbiting and hovering shots erase the enemy shots they touch.
    if (ShapeOn(synergy::Family::Orbit))
        for (const Bullet& b : g_Bullets)
            if (b.active && !b.isEnemy && (b.synFlags & (synf::Orbit | synf::Hovering)))
                passive::ClearEnemyBullets(b.x, b.y, 22.0f * b.sizeScale * (0.5f + 0.5f * ShapeLv(synergy::Family::Orbit)), false);
    // Force field: enemy bullets that come close slow down.
    if (o.G_FORCEFIELD)
        for (Bullet& b : g_Bullets)
            if (b.active && b.isEnemy && !(b.synFlags & synf::Slowed) &&
                (b.x - in.px) * (b.x - in.px) + (b.y - in.py) * (b.y - in.py) < Rings(2) * Rings(2)) {
                b.synFlags |= synf::Slowed; b.speed *= 1.0f - UPB(G_FORCEFIELD);
            }

    // Marks and burns.
    for (auto it = s.marks.begin(); it != s.marks.end();) { it->second -= dt; it = it->second <= 0.0f ? s.marks.erase(it) : std::next(it); }
    // The shield shows as a ring round the ship while it holds.
    s.shieldPulse -= dt;
    if (s.shieldPulse <= 0.0f && g_PlayerShield > 0.0f) { s.shieldPulse = 0.9f; Pulse(in.px, in.py, 36.0f, 0.4f, 0.45f, 0.85f, 1.0f); }
    if (!s.burns.empty()) {
        const float dmg = passive::AttackPower() * UPA(V_BURN) * 0.5f * passive::AllDamageMult();
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
    StepLaserBurns(dt);

    // Zones (trails, wall clouds, dash afterimages).
    for (Zone& z : s.zones) {
        z.life -= dt; z.tick -= dt; z.age += dt;
        if (z.tick <= 0.0f) {
            z.tick += 0.3f;
            const float dmg = passive::AttackPower() * z.pct * 0.3f / 100.0f * passive::AllDamageMult();
            ForEachEnemy([&](auto& e, float ex, float ey) {
                if ((ex - z.x) * (ex - z.x) + (ey - z.y) * (ey - z.y) > z.r * z.r) return;
                if (z.burns) {   // keep it burning (3s from now) without resetting its tick
                    auto [it, fresh] = s.burns.try_emplace(&e, Burn{ 3.0f, 0.5f });
                    if (!fresh) it->second.t = 3.0f;
                }
                const float dealt = g_MonsterManager.ApplyDamage(e, dmg);
                if (dealt > 0.0f) SpawnDamageNumber(ex, ey, dealt, false);
            });
        }
        if (z.life <= 0.0f && z.blastAtEnd > 0.0f) Burst(z.x, z.y, z.r * 1.4f, z.blastAtEnd);
    }
    s.zones.erase(std::remove_if(s.zones.begin(), s.zones.end(), [](const Zone& z) { return z.life <= 0.0f; }), s.zones.end());
    StepStreaks(dt);

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
            Burst(bx, by, Rings(1), bp, { 1.0f, 0.45f, 0.35f }, strike_fx::BlastStyle::Crystal);
        }
    }
    s.mines.erase(std::remove_if(s.mines.begin(), s.mines.end(), [](const Mine& m) { return m.life <= 0.0f; }), s.mines.end());

    // Echoes of the volley.
    for (Echo& e : s.echoes) {
        e.t -= dt;
        if (e.t <= 0.0f) {
            // 겹겹 메아리 사격: the ship gives a small ring as the echo leaves it, in the shots' colour.
            if (!e.shots.empty()) {
                const glm::vec3 c = e.shots.front().color;
                strike_fx::Implode(in.px, in.py, 38.0f, line_fx::Lift({ c.r, c.g, c.b }, 0.3f), 0.0f, 0.18f);
            }
            for (Bullet b : e.shots) {
                if (g_Bullets.size() >= 800) break;
                if ((b.synFlags & synf::Orbit) && CountOrbiters() >= 2 + (o.Y_HALO ? 2 : 0)) continue;   // the cap holds for echoes too
                b.x = b.prevX = in.px; b.y = b.prevY = in.py; b.splitSpent = false;
                b.synTrailChain = 0;   // an echo flies its own tail
                g_Bullets.push_back(b);
            }
        }
    }
    s.echoes.erase(std::remove_if(s.echoes.begin(), s.echoes.end(), [](const Echo& e) { return e.t <= 0.0f; }), s.echoes.end());

    // Kills nobody reported (burn, zones, skills, explosions): an enemy that was alive and now is not.
    if (o.T_COMBO || o.T_KILLBURST || o.V_CHAIN || o.G_CIRCUIT ||
        o.P_KILL_CRIT || o.P_KILL_PIERCE || o.P_KILL_SPLIT || o.P_COMBO_BOUNCE || o.P_COMBO_BLAST) {
        std::unordered_map<const void*, glm::vec2> now;
        ForEachEnemy([&](auto& e, float ex, float ey) { now[&e] = glm::vec2(ex, ey); });
        for (const auto& kv : s.seen)
            if (!now.count(kv.first)) KillEvent(kv.second.x, kv.second.y, kv.first);
        s.seen.swap(now);
    }
    s.counted.clear();

    // Bullets spawned by events during the collision pass.
    for (Bullet& b : s.pending) if (g_Bullets.size() < 800) { ClampPlayerBulletSize(b); g_Bullets.push_back(b); }
    s.pending.clear();

    // The shots themselves.
    float absorbedNow = 0.0f;
    for (Bullet& b : g_Bullets) {
        if (!b.active || b.isEnemy || b.skillShot || b.synFlags == 0) continue;
        const uint32_t f = b.synFlags;
        if (f & synf::Grow) {
            // The same curve as its damage (DamageMult): full size where the damage is full, at 13 rings.
            b.sizeScale = std::max(b.sizeScale, b.synGrowBase * (1.0f + std::min(UPA(V_GROW) - 1.0f, b.traveled / (Rings(UPB(V_GROW)) / (UPA(V_GROW) - 1.0f)))));
            ClampPlayerBulletSize(b);
        }
        if (f & synf::Trail) RecordStreak(b, dt);
        if ((f & synf::Trail) && b.traveled - b.synTrailAt >= 60.0f) {
            b.synTrailAt = b.traveled;
            // With Ember Rounds (a burning shot) the afterimage sets enemies alight too.
            // 40% of the shot's own damage: a fast, light gun (minigun) lays many thin trails, not many full ones.
            AddTrailZone(b.x, b.y, kTrailZoneR * (1.0f + UPB(V_TRAIL)), UPA(V_TRAIL) * 100.0f * b.dmgMult * s.dmg, b.synTrailChain, (f & synf::Burn) != 0, b.color);
        }
        if ((f & synf::Hover) && !(f & synf::Hovered)) {
            if (!(f & synf::Hovering) && b.traveled >= Rings(4)) {
                b.synFlags |= synf::Hovering; b.synHoverSpeed = b.speed; b.synHoverLeft = 0.8f; b.speed = 0.0f;
            } else if (f & synf::Hovering) {
                b.synHoverLeft -= dt;
                if (b.synHoverLeft <= 0.0f) {
                    b.speed = b.synHoverSpeed; b.synFlags = (b.synFlags & ~synf::Hovering) | synf::Hovered;
                }
            }
        }
        if ((f & synf::Return) && !(f & synf::Returning) && b.traveled >= Rings(13.0f)) {
            b.synFlags |= synf::Returning; b.ClearHits(); b.homing = false; b.piercing = true;
        }
        if (b.synFlags & synf::Returning) {
            const float dx = in.px - b.x, dy = in.py - b.y, d = std::sqrt(dx * dx + dy * dy);
            if (d < 26.0f || b.traveled > 2800.0f) b.active = false;   // 13○ out and back is 2080px
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
        if (f & synf::Absorb)   // a beam segment absorbs along its whole length
            absorbedNow += (f & synf::Hitscan) ? (float)ClearAlong(b.prevX, b.prevY, b.x, b.y, 22.0f)
                                               : (float)passive::ClearEnemyBullets(b.x, b.y, 22.0f, false);
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
                const float r = Rings(1.5f);
                Burst(b.synTx, b.synTy, r, 90.0f * b.dmgMult * s.dmg);   // Burst adds the card part
                Landed(b, r);
            }
        }
        if (f & synf::Mine) {
            if (b.traveled >= b.synLandDist - 2.0f) {
                b.active = false;
                if (s.mines.size() < 40) s.mines.push_back({ b.x, b.y, 8.0f, UPA(V_MINE) * 100.0f * b.dmgMult * s.dmg, 0.0f });   // Burst adds the card part
            }
        }
    }
    s.absorbed = std::min(10.0f, s.absorbed + absorbedNow);
}

}  // namespace syn
