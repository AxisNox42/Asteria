#include "ActiveSkills.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "AugmentCombat.h"
#include "Bullet.h"
#include "GameManager.h"
#include "HudLines.h"
#include "Juice.h"
#include "MonsterManager.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include "SaveSystem.h"

extern GameManager         g_GameManager;
extern MonsterManager      g_MonsterManager;
extern std::vector<Bullet> g_Bullets;
extern PlayerStats         g_Stats;

namespace active {
namespace {

using hud_lines::Rgb;
constexpr float kTwoPi = 6.2831853f;

// Shots that need follow-up on a hit (Bullet::skillTag).
enum Tag { TAG_NONE, TAG_ARROW, TAG_KNIFE, TAG_FIREBALL, TAG_SHARD, TAG_SNIPE,
           TAG_TURRET, TAG_STAR, TAG_STAR_CHILD };

// Field objects a skill leaves behind.
enum class ObjKind { Wall, Meteor, Dynamite, Prism, Gate, Trap, Turret, Cloud,
                     Clone, Hoop, Line, Strike };
struct Obj {
    ObjKind kind;
    float x = 0, y = 0;          // position (wall: centre)
    float x2 = 0, y2 = 0;        // wall: half-extent vector; dynamite: target; line: end
    float t = 0, life = 0;       // age, lifetime
    float timer = 0;             // per-object clock (turret, cloud)
    float dirX = 0, dirY = 0;    // hoop: outward direction
    int   count = 0;             // prism / gate: bullets absorbed
    Rgb   col{ 1, 1, 1 };
    std::vector<const void*> hits;   // hoop: hit on this leg
    std::vector<float> pts;          // line: polyline (x, y pairs)
};

struct State {
    Slot slots[kSlots];
    bool sealed[kSlots] = {};
    int  nextOrder = 1;
    int  lastUsedAug = -1;        // Echo Cast's target / Unstable Drive repeat
    int  dayNightUses = 0;
    int  waveKills = 0;
    float ribbonWindow = 0.0f;
    bool  dashWas = false;
    float clock = 0.0f;
    bool keyHeld[kSlots] = {};
    // Orbital Return: positions and damage taken over the last few seconds.
    struct Pos { float t, x, y; };
    std::deque<Pos> trail;
    std::deque<std::pair<float, float>> damage;
    // Pending casts.
    float snipeT = -1.0f;         // aiming time left
    float meditateT = -1.0f;      // charge time, -1 = not channelling
    bool teleport = false;
    float teleX = 0, teleY = 0;
    std::unordered_map<int, float> moonCd;   // Moon Pendant per skill
    std::vector<Obj> objs;
    StepInput in;
};
State s;

AugType TypeOf(int aug) { return ALL_AUGS[aug].type; }
bool Has(AugType t) { return passive::Has(t); }

float Dmg(float pct) {
    return passive::AttackPower() * pct / 100.0f * passive::AllDamageMult() * DamageMult();
}

void AimDir(float& dx, float& dy) {
    dx = s.in.aimX - s.in.px;
    dy = s.in.aimY - s.in.py;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) { dx = 1.0f; dy = 0.0f; return; }
    dx /= len; dy /= len;
}

// Damages one enemy (Monster or RangedMob) by pct of attack.
template <class T>
float HitOne(T& target, float pct, bool crit = false) {
    if (!target.alive) return 0.0f;
    const float dealt = g_MonsterManager.ApplyDamage(target,
        Dmg(pct) * (crit ? g_Stats.critMult : 1.0f));
    if (dealt > 0.0f) SpawnDamageNumber(target.worldX, target.worldY, dealt, crit);
    return dealt;
}

// Calls fn(target, x, y) for every living enemy.
template <class Fn>
void ForEachEnemy(Fn fn) {
    for (auto* m : g_MonsterManager.monsters)
        if (m->alive) fn(*m, m->worldX, m->worldY);
    for (auto* r : g_MonsterManager.rangedMobs)
        if (r->alive) fn(*r, r->worldX, r->worldY);
}

float SegDist2(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax, aby = by - ay;
    const float len2 = abx * abx + aby * aby;
    float t = 0.0f;
    if (len2 > 1e-6f) t = std::clamp(((px - ax) * abx + (py - ay) * aby) / len2, 0.0f, 1.0f);
    const float dx = px - (ax + abx * t), dy = py - (ay + aby * t);
    return dx * dx + dy * dy;
}

void ClampToView(float& x, float& y) {
    const float m = 30.0f / std::max(0.01f, s.in.viewZoom);
    x = std::clamp(x, s.in.viewL + m, s.in.viewR - m);
    y = std::clamp(y, s.in.viewT + m, s.in.viewB - m);
}

Bullet SkillShot(float fromX, float fromY, float dirX, float dirY, float pct,
                 int tag, float speed, Rgb col, float size = 1.0f) {
    Bullet b(fromX, fromY, fromX + dirX * 100.0f, fromY + dirY * 100.0f);
    b.skillShot = true;
    b.skillTag = tag;
    b.dmgMult = pct / 100.0f;
    b.speed = speed;
    b.sizeScale = size;
    b.color = glm::vec3(col.r, col.g, col.b);
    return b;
}

void AddLine(std::vector<float> pts, Rgb col, float life) {
    Obj o;
    o.kind = ObjKind::Line;
    o.pts = std::move(pts);
    o.col = col;
    o.life = life;
    s.objs.push_back(std::move(o));
}

void AddStrike(float x, float y, Rgb col) {   // a lightning bolt from above
    Obj o;
    o.kind = ObjKind::Strike;
    o.x = x; o.y = y;
    o.col = col;
    o.life = 0.25f;
    s.objs.push_back(o);
}

int SlotOf(int aug) {
    for (int i = 0; i < kSlots; ++i)
        if (s.slots[i].aug == aug) return i;
    return -1;
}

// ── each skill ──
void Cast(int aug);

void CastLightningStrike() {
    struct Pick { float hp; void* p; bool ranged; float x, y; };
    std::vector<Pick> picks;
    for (auto* m : g_MonsterManager.monsters)
        if (m->alive && passive::OnScreen(m->worldX, m->worldY))
            picks.push_back({ m->hp, m, false, m->worldX, m->worldY });
    for (auto* r : g_MonsterManager.rangedMobs)
        if (r->alive && passive::OnScreen(r->worldX, r->worldY))
            picks.push_back({ r->hp, r, true, r->worldX, r->worldY });
    std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.hp > b.hp; });
    for (size_t i = 0; i < picks.size() && i < 3; ++i) {
        if (picks[i].ranged) HitOne(*(RangedMob*)picks[i].p, 600.0f);
        else                 HitOne(*(Monster*)picks[i].p, 600.0f);
        AddStrike(picks[i].x, picks[i].y, { 0.65f, 0.80f, 1.0f });
    }
}

void CastChainLightning() {
    std::vector<const void*> visited;
    std::vector<float> pts{ s.in.px, s.in.py };
    float fromX = s.in.px, fromY = s.in.py, reach = 600.0f;
    for (int jump = 0; jump < 8; ++jump) {
        float best = reach * reach;
        void* target = nullptr;
        bool ranged = false;
        float tx = 0, ty = 0;
        auto consider = [&](auto& e, float x, float y) {
            if (std::find(visited.begin(), visited.end(), (const void*)&e) != visited.end()) return;
            const float d2 = (x - fromX) * (x - fromX) + (y - fromY) * (y - fromY);
            if (d2 < best) {
                best = d2; target = (void*)&e; tx = x; ty = y;
                ranged = std::is_same<std::decay_t<decltype(e)>, RangedMob>::value;
            }
        };
        ForEachEnemy(consider);
        if (!target) break;
        visited.push_back(target);
        if (ranged) HitOne(*(RangedMob*)target, 350.0f);
        else        HitOne(*(Monster*)target, 350.0f);
        pts.push_back(tx); pts.push_back(ty);
        fromX = tx; fromY = ty;
        reach = 320.0f;
    }
    AddLine(pts, { 0.70f, 0.85f, 1.0f }, 0.30f);
}

void CastExecute(int aug) {
    float dx, dy;
    AimDir(dx, dy);
    float best = 1e12f;
    void* target = nullptr;
    bool ranged = false;
    float tx = 0, ty = 0;
    ForEachEnemy([&](auto& e, float x, float y) {
        const float ex = x - s.in.px, ey = y - s.in.py;
        const float d = std::sqrt(ex * ex + ey * ey);
        if (d < 1.0f || (ex * dx + ey * dy) / d < 0.80f) return;   // ~37° either side
        if (d < best) {
            best = d; target = (void*)&e; tx = x; ty = y;
            ranged = std::is_same<std::decay_t<decltype(e)>, RangedMob>::value;
        }
    });
    if (!target) return;
    bool killed;
    if (ranged) { RangedMob& r = *(RangedMob*)target; HitOne(r, 1000.0f); killed = !r.alive; }
    else        { Monster& m = *(Monster*)target; HitOne(m, 1000.0f); killed = !m.alive; }
    AddLine({ s.in.px, s.in.py, tx, ty }, { 1.0f, 0.35f, 0.35f }, 0.25f);
    const int slot = SlotOf(aug);
    if (killed && slot >= 0) s.slots[slot].cd = 0.0f;
}

void CastGust() {
    float dx, dy;
    AimDir(dx, dy);
    constexpr float kRange = 380.0f, kCos = 0.70f;   // ~45° either side
    for (Bullet& b : g_Bullets) {
        if (!b.active || !b.isEnemy) continue;
        const float ex = b.x - s.in.px, ey = b.y - s.in.py;
        const float d = std::sqrt(ex * ex + ey * ey);
        if (d < kRange && (d < 1.0f || (ex * dx + ey * dy) / d >= kCos)) b.active = false;
    }
    auto push = [&](auto& e, float x, float y, bool canMove) {
        const float ex = x - s.in.px, ey = y - s.in.py;
        const float d = std::sqrt(ex * ex + ey * ey);
        if (d > kRange || (d > 1.0f && (ex * dx + ey * dy) / d < kCos)) return;
        HitOne(e, 300.0f);
        if (canMove && d > 1.0f) { e.worldX += ex / d * 240.0f; e.worldY += ey / d * 240.0f; }
    };
    for (auto* m : g_MonsterManager.monsters)
        if (m->alive) push(*m, m->worldX, m->worldY, !IsBossMob(m->kind) && !IsEliteMob(m->kind));
    for (auto* r : g_MonsterManager.rangedMobs)
        if (r->alive) push(*r, r->worldX, r->worldY, true);
    AddLine({ s.in.px, s.in.py, s.in.px + dx * kRange, s.in.py + dy * kRange },
            { 0.70f, 1.0f, 0.85f }, 0.25f);
}

void CastBlink() {
    float dx, dy;
    AimDir(dx, dy);
    float tx = s.in.px + dx * 260.0f, ty = s.in.py + dy * 260.0f;
    ClampToView(tx, ty);
    passive::ClearEnemyBullets(s.in.px, s.in.py, 140.0f);
    passive::ClearEnemyBullets(tx, ty, 140.0f);
    AddLine({ s.in.px, s.in.py, tx, ty }, { 0.70f, 0.90f, 1.0f }, 0.20f);
    s.teleport = true; s.teleX = tx; s.teleY = ty;
}

void CastCometRush() {
    float dx, dy;
    AimDir(dx, dy);
    float tx = s.in.px + dx * 520.0f, ty = s.in.py + dy * 520.0f;
    ClampToView(tx, ty);
    ForEachEnemy([&](auto& e, float x, float y) {
        if (SegDist2(x, y, s.in.px, s.in.py, tx, ty) < 50.0f * 50.0f) HitOne(e, 800.0f);
    });
    AddLine({ s.in.px, s.in.py, tx, ty }, { 1.0f, 0.85f, 0.45f }, 0.35f);
    g_PlayerRuntime.dashInvulnerability = std::max(g_PlayerRuntime.dashInvulnerability, 0.25f);
    s.teleport = true; s.teleX = tx; s.teleY = ty;
}

void CastRewind() {
    if (s.trail.empty()) return;
    const State::Pos& p = s.trail.front();   // the oldest kept: ~3s ago
    float lost = 0.0f;
    for (const auto& d : s.damage) lost += d.second;
    g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + lost * 0.5f);
    AddLine({ s.in.px, s.in.py, p.x, p.y }, { 0.85f, 0.75f, 1.0f }, 0.35f);
    s.teleport = true; s.teleX = p.x; s.teleY = p.y;
}

void CastDayAndNight() {
    const bool night = s.dayNightUses >= 3;
    if (!night) {
        float dx, dy;
        AimDir(dx, dy);
        const float ex = s.in.px + dx * 900.0f, ey = s.in.py + dy * 900.0f;
        ForEachEnemy([&](auto& e, float x, float y) {
            if (SegDist2(x, y, s.in.px, s.in.py, ex, ey) < 40.0f * 40.0f) HitOne(e, 500.0f);
        });
        AddLine({ s.in.px, s.in.py, ex, ey }, { 1.0f, 0.92f, 0.60f }, 0.30f);
    } else {
        passive::DamageRadius(s.in.px, s.in.py, 260.0f, 400.0f, 0.45f, 0.30f, 0.85f, DamageMult());
        passive::ClearEnemyBullets(s.in.px, s.in.py, 260.0f);
    }
    s.dayNightUses = (s.dayNightUses + 1) % 6;
}

void CastBookOfCheats() {
    int roll = std::rand() % 8;
    if (roll == 7 && passive::Chance(0.25f)) roll = std::rand() % 7;   // luck can reroll "nothing"
    switch (roll) {
    case 0: passive::DamageAll(500.0f, 0.65f, 0.80f, 1.0f, false, DamageMult()); break;
    case 1: g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 20.0f); break;
    case 2: passive::ClearEnemyBullets(0.0f, 0.0f, -1.0f); break;
    case 3: passive::ResetDash(); break;
    case 4: passive::AddStardust(10); break;
    case 5:
        for (int i = 0; i < 12; ++i) {
            const float a = kTwoPi * (float)i / 12.0f;
            Bullet b = SkillShot(s.in.px, s.in.py, std::cos(a), std::sin(a), 150.0f,
                                 TAG_SHARD, 700.0f, { 1.0f, 0.85f, 0.40f });
            b.homing = true; b.homingTurn = 6.0f;
            g_Bullets.push_back(b);
        }
        break;
    case 6: passive::DamageRadius(s.in.px, s.in.py, 260.0f, 800.0f, 1.0f, 0.55f, 0.25f, DamageMult()); break;
    default: break;   // nothing happens
    }
    passive::Ring(s.in.px, s.in.py, 120.0f, 1.0f, 0.85f, 0.40f);
}

void Cast(int aug) {
    const float px = s.in.px, py = s.in.py;
    float dx, dy;
    AimDir(dx, dy);
    const float extra = DamageMult();
    switch (TypeOf(aug)) {
    case AugType::A_NIGHT_STARFALL:
        passive::DamageAll(1500.0f, 0.75f, 0.70f, 1.0f, false, extra);
        break;
    case AugType::A_NIGHT_METEOR_LANCE: {
        Bullet b = SkillShot(px, py, dx, dy, 600.0f, TAG_ARROW, 420.0f, { 0.75f, 0.70f, 1.0f }, 2.6f);
        b.piercing = true;
        g_Bullets.push_back(b);
        break;
    }
    case AugType::A_CHRONO_ORBITAL_RETURN: CastRewind(); break;
    case AugType::A_CHRONO_RESONANT_SYNC:
        for (Slot& o : s.slots)
            if (o.aug >= 0 && o.aug != aug) o.cd *= 0.5f;
        passive::Ring(px, py, 90.0f, 0.85f, 0.75f, 1.0f);
        break;
    case AugType::A_GALE_STELLAR_GALE: CastGust(); break;
    case AugType::A_SHADOW_SHARD_VOLLEY:
        for (int i = 0; i < 8; ++i) {
            const float a = kTwoPi * (float)i / 8.0f;
            g_Bullets.push_back(SkillShot(px, py, std::cos(a), std::sin(a), 300.0f,
                                          TAG_KNIFE, 950.0f, { 0.60f, 0.50f, 0.95f }));
        }
        break;
    case AugType::A_SHADOW_TWIN_STAR_CLONE: {
        Obj o; o.kind = ObjKind::Clone; o.x = px; o.y = py; o.life = 8.0f;
        o.col = { 0.60f, 0.50f, 0.95f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_STONE_GRAVITY_SLAM:
        passive::DamageRadius(px, py, 380.0f, 1400.0f, 0.85f, 0.70f, 0.50f, extra);
        passive::ClearEnemyBullets(px, py, 380.0f);
        break;
    case AugType::A_STONE_ASTEROID_WALL: {
        Obj o; o.kind = ObjKind::Wall; o.life = 6.0f;
        o.x = px + dx * 130.0f; o.y = py + dy * 130.0f;
        o.x2 = -dy * 120.0f; o.y2 = dx * 120.0f;    // half-extent along the wall
        o.col = { 0.85f, 0.70f, 0.50f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_FLAME_PROMINENCE_SHELL: {
        Bullet b = SkillShot(px, py, dx, dy, 500.0f, TAG_FIREBALL, 800.0f, { 1.0f, 0.45f, 0.15f }, 1.6f);
        b.maxRange = 1100.0f;
        g_Bullets.push_back(b);
        break;
    }
    case AugType::A_FLAME_METEOR_BARRAGE: {
        Obj o; o.kind = ObjKind::Meteor; o.x = s.in.aimX; o.y = s.in.aimY; o.life = 1.0f;
        o.col = { 1.0f, 0.45f, 0.15f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_GEM_REFRACTION_CRYSTAL: {
        Obj o; o.kind = ObjKind::Prism; o.x = px; o.y = py; o.life = 4.0f;
        o.col = { 0.75f, 0.95f, 1.0f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_GEM_BARRAGE_RECYCLE: {
        const int n = std::min(24, passive::ClearEnemyBullets(px, py, 220.0f));
        for (int i = 0; i < n; ++i) {
            const float a = kTwoPi * passive::Rand01();
            Bullet b = SkillShot(px, py, std::cos(a), std::sin(a), 100.0f, TAG_SHARD,
                                 750.0f, { 1.0f, 0.30f, 0.40f });
            b.homing = true; b.homingTurn = 7.0f;
            g_Bullets.push_back(b);
        }
        break;
    }
    case AugType::A_STORM_TRIPLE_BOLT: CastLightningStrike(); break;
    case AugType::A_STORM_STORM_NEBULA: {
        Obj o; o.kind = ObjKind::Cloud; o.life = 6.0f; o.col = { 0.65f, 0.80f, 1.0f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_LUCK_DICE_OF_FATE: CastBookOfCheats(); break;
    case AugType::A_LIFE_STARLIGHT_MEND:
        g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 15.0f);
        passive::ClearEnemyBullets(px, py, 220.0f);
        break;
    case AugType::A_ABYSS_TIDAL_SURGE:
        passive::DamageRadius(px, py, 380.0f, 1200.0f, 0.30f, 0.60f, 1.0f, extra);
        break;
    case AugType::A_ABYSS_BLACK_HOLE_CAPTURE: {
        Obj o; o.kind = ObjKind::Gate; o.x = s.in.aimX; o.y = s.in.aimY; o.life = 3.0f;
        o.col = { 0.55f, 0.35f, 0.95f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_SPARK_FORCED_DISCHARGE:
        passive::DamageRadius(px, py, 260.0f, 400.0f, 0.55f, 0.85f, 1.0f, extra);
        for (int i = 0; i < 10; ++i) passive::CountHit();
        break;
    case AugType::A_SPARK_CHAIN_LIGHTNING: CastChainLightning(); break;
    case AugType::A_FLIGHT_WARP_JUMP: CastBlink(); break;
    case AugType::A_FLIGHT_COMET_RUSH: CastCometRush(); break;
    case AugType::A_EDGE_PRECISION_SNIPE: s.snipeT = 0.6f; break;
    case AugType::A_HERESY_LIFE_BURN_BLAST: {
        const float spent = g_GameManager.playerHP * 0.30f;
        g_GameManager.playerHP -= spent;
        passive::DamageAll(100.0f * spent, 0.85f, 0.20f, 0.30f, false, extra);
        break;
    }
    case AugType::A_HERESY_DAY_NIGHT_BOUNDARY: CastDayAndNight(); break;
    case AugType::A_HUNTER_INTERSTELLAR_MINE: {
        int traps = 0;
        for (const Obj& o : s.objs) traps += o.kind == ObjKind::Trap;
        if (traps >= 3) {
            for (auto it = s.objs.begin(); it != s.objs.end(); ++it)
                if (it->kind == ObjKind::Trap) { s.objs.erase(it); break; }   // oldest
        }
        Obj o; o.kind = ObjKind::Trap; o.x = px; o.y = py; o.life = 1e9f;
        o.col = { 1.0f, 0.55f, 0.45f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_HUNTER_PINPOINT_EXECUTION: CastExecute(aug); break;
    case AugType::A_PERFORM_ORBIT_RING:
        for (int side = -1; side <= 1; side += 2) {
            Obj o; o.kind = ObjKind::Hoop; o.x = px; o.y = py; o.life = 0.9f;
            o.dirX = -dy * (float)side; o.dirY = dx * (float)side;
            o.col = { 1.0f, 0.80f, 0.40f };
            s.objs.push_back(o);
        }
        break;
    case AugType::A_PERFORM_ECHO_CAST:
        if (s.lastUsedAug >= 0 && TypeOf(s.lastUsedAug) != AugType::A_PERFORM_ECHO_CAST)
            Cast(s.lastUsedAug);
        break;
    case AugType::A_STILL_STARLIGHT_TURRET: {
        Obj o; o.kind = ObjKind::Turret; o.x = px; o.y = py; o.life = 10.0f;
        o.col = { 0.60f, 0.95f, 0.80f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_STILL_CONDENSED_BLAST: s.meditateT = 0.0f; break;
    case AugType::A_VEIN_TIMED_BOMB: {
        Obj o; o.kind = ObjKind::Dynamite; o.x = px; o.y = py;
        o.x2 = s.in.aimX; o.y2 = s.in.aimY; o.life = 1.0f;
        o.col = { 1.0f, 0.55f, 0.25f };
        s.objs.push_back(o);
        break;
    }
    case AugType::A_NOVA_NOVA_BURST: {
        const float pct = std::min(3000.0f, 30.0f * (float)s.waveKills);
        if (pct > 0.0f) passive::DamageAll(pct, 1.0f, 0.95f, 0.75f, false, extra);
        break;
    }
    case AugType::A_NOVA_STARLIGHT_SCATTER:
        for (int i = 0; i < 12; ++i) {
            const float a = kTwoPi * (float)i / 12.0f;
            g_Bullets.push_back(SkillShot(px, py, std::cos(a), std::sin(a), 150.0f,
                                          TAG_STAR, 850.0f, { 1.0f, 0.95f, 0.60f }));
        }
        break;
    default: break;
    }
}

// The passive follow-ups of using a skill (not of an Echo Cast recast).
void AfterUse(int aug, int slot) {
    const float cd = CooldownFor(aug);
    if (Has(AugType::P_CHRONO_DOUBLE_CAST) && passive::Chance(0.30f)) {
        s.slots[slot].cd = 0.0f;
        passive::ChanceTriggered();
    }
    if (Has(AugType::P_STORM_MAGNETIC_STORM) && passive::Chance(0.20f)) {
        passive::DamageAll(400.0f, 0.65f, 0.80f, 1.0f);
        passive::ChanceTriggered();
    }
    if (Has(AugType::P_NIGHT_RESONANT_SHOCKWAVE) && s.moonCd[aug] <= 0.0f) {
        s.moonCd[aug] = 12.0f;
        passive::DamageRadius(s.in.px, s.in.py, 220.0f, 800.0f, 0.85f, 0.85f, 1.0f);
    }
    if (Has(AugType::P_ABYSS_SINGULARITY_CASCADE) && cd >= 10.0f) {
        const int n = std::min(5, 1 + (int)((cd - 10.0f) / 10.0f));
        for (int i = 0; i < n; ++i) passive::DamageAll(400.0f, 0.55f, 0.35f, 0.95f);
    }
    if (Has(AugType::P_PERFORM_ECLIPSE_COMBO) && s.ribbonWindow > 0.0f) {
        s.ribbonWindow = 0.0f;
        passive::DamageRadius(s.in.px, s.in.py, 380.0f, 700.0f, 1.0f, 0.60f, 0.85f);
    }
    if (Has(AugType::P_HERESY_UNSTABLE_DRIVE) && s.lastUsedAug == aug) passive::SwitchOffDarkWings();
    if (TypeOf(aug) != AugType::A_PERFORM_ECHO_CAST) s.lastUsedAug = aug;
}

bool TryUse(int slot) {
    Slot& sl = s.slots[slot];
    if (s.sealed[slot] || sl.aug < 0 || sl.cd > 0.0f) return false;
    if (TypeOf(sl.aug) == AugType::A_STILL_CONDENSED_BLAST && s.meditateT >= 0.0f) return false;
    if (TypeOf(sl.aug) == AugType::A_EDGE_PRECISION_SNIPE && s.snipeT >= 0.0f) return false;
    sl.cd = sl.cdMax = CooldownFor(sl.aug);
    Cast(sl.aug);
    AfterUse(sl.aug, slot);
    return true;
}

void StepObjects(float dt) {
    for (Obj& o : s.objs) {
        o.t += dt;
        switch (o.kind) {
        case ObjKind::Wall: {
            const float ax = o.x - o.x2, ay = o.y - o.y2, bx = o.x + o.x2, by = o.y + o.y2;
            for (Bullet& b : g_Bullets)
                if (b.active && b.isEnemy && SegDist2(b.x, b.y, ax, ay, bx, by) < 16.0f * 16.0f)
                    b.active = false;
            const float len = std::sqrt(o.x2 * o.x2 + o.y2 * o.y2);
            const float nx = -o.y2 / len, ny = o.x2 / len;   // wall normal
            auto block = [&](auto& e) {
                if (SegDist2(e.worldX, e.worldY, ax, ay, bx, by) >= 34.0f * 34.0f) return;
                const float side = (e.worldX - o.x) * nx + (e.worldY - o.y) * ny;
                const float push = 34.0f - std::fabs(side);
                const float sign = side >= 0.0f ? 1.0f : -1.0f;
                e.worldX += nx * sign * push;
                e.worldY += ny * sign * push;
            };
            for (auto* m : g_MonsterManager.monsters)
                if (m->alive && !IsBossMob(m->kind)) block(*m);
            for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) block(*r);
            break;
        }
        case ObjKind::Meteor:
            if (o.t >= o.life) {
                passive::DamageRadius(o.x, o.y, 300.0f, 2500.0f, 1.0f, 0.45f, 0.15f, DamageMult());
                TriggerFlash(1.0f, 0.55f, 0.25f, 0.25f);
            }
            break;
        case ObjKind::Dynamite:
            if (o.t >= o.life)
                passive::DamageRadius(o.x2, o.y2, 200.0f, 1000.0f, 1.0f, 0.55f, 0.25f, DamageMult());
            break;
        case ObjKind::Prism:
            for (Bullet& b : g_Bullets) {
                if (!b.active || !b.isEnemy) continue;
                if ((b.x - o.x) * (b.x - o.x) + (b.y - o.y) * (b.y - o.y) < 70.0f * 70.0f) {
                    b.active = false;
                    ++o.count;
                }
            }
            if (o.t >= o.life && o.count > 0)
                passive::DamageRadius(o.x, o.y, 220.0f, 30.0f * (float)o.count,
                                      0.75f, 0.95f, 1.0f, DamageMult());
            break;
        case ObjKind::Gate: {
            constexpr float kPull = 420.0f;
            auto pull = [&](float& x, float& y, float speed) {
                const float ex = o.x - x, ey = o.y - y;
                const float d = std::sqrt(ex * ex + ey * ey);
                if (d < 1.0f || d > kPull) return;
                const float step = std::min(d, speed * dt);
                x += ex / d * step; y += ey / d * step;
            };
            for (auto* m : g_MonsterManager.monsters)
                if (m->alive && !IsBossMob(m->kind)) pull(m->worldX, m->worldY, 260.0f);
            for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) pull(r->worldX, r->worldY, 260.0f);
            for (Bullet& b : g_Bullets) {
                if (!b.active || !b.isEnemy) continue;
                pull(b.x, b.y, 520.0f);
                if ((b.x - o.x) * (b.x - o.x) + (b.y - o.y) * (b.y - o.y) < 30.0f * 30.0f) {
                    b.active = false;
                    ++o.count;
                }
            }
            if (o.t >= o.life) {
                passive::DamageRadius(o.x, o.y, 260.0f, 1000.0f + 50.0f * (float)o.count,
                                      0.55f, 0.35f, 0.95f, DamageMult());
                TriggerFlash(0.55f, 0.35f, 0.95f, 0.25f);
            }
            break;
        }
        case ObjKind::Trap: {
            bool sprung = false;
            auto step = [&](auto& e, float x, float y) {
                if (sprung) return;
                if ((x - o.x) * (x - o.x) + (y - o.y) * (y - o.y) < 40.0f * 40.0f) {
                    HitOne(e, 900.0f);
                    sprung = true;
                }
            };
            ForEachEnemy(step);
            if (sprung) { o.life = 0.0f; passive::Ring(o.x, o.y, 60.0f, 1.0f, 0.55f, 0.45f); }
            break;
        }
        case ObjKind::Turret:
            o.timer += dt;
            if (o.timer >= 0.4f) {
                float tx, ty;
                if (g_MonsterManager.FindNearestEnemy(o.x, o.y, tx, ty)) {
                    o.timer = 0.0f;
                    float ex = tx - o.x, ey = ty - o.y;
                    const float d = std::max(0.001f, std::sqrt(ex * ex + ey * ey));
                    g_Bullets.push_back(SkillShot(o.x, o.y, ex / d, ey / d, 100.0f, TAG_TURRET,
                                                  1100.0f, { 0.60f, 0.95f, 0.80f }));
                }
            }
            break;
        case ObjKind::Cloud:
            o.x = s.in.px; o.y = s.in.py - 90.0f;
            o.timer += dt;
            if (o.timer >= 0.5f) {
                o.timer -= 0.5f;
                std::vector<std::pair<void*, bool>> targets;
                for (auto* m : g_MonsterManager.monsters)
                    if (m->alive && passive::OnScreen(m->worldX, m->worldY)) targets.push_back({ m, false });
                for (auto* r : g_MonsterManager.rangedMobs)
                    if (r->alive && passive::OnScreen(r->worldX, r->worldY)) targets.push_back({ r, true });
                if (!targets.empty()) {
                    const auto& t = targets[std::rand() % targets.size()];
                    if (t.second) { RangedMob& r = *(RangedMob*)t.first; HitOne(r, 300.0f); AddStrike(r.worldX, r.worldY, o.col); }
                    else          { Monster& m = *(Monster*)t.first; HitOne(m, 300.0f); AddStrike(m.worldX, m.worldY, o.col); }
                }
            }
            break;
        case ObjKind::Hoop: {
            const float half = o.life * 0.5f;
            const bool back = o.t > half;
            if (back && o.count == 0) { o.count = 1; o.hits.clear(); }   // a new leg
            const float out = back ? 1.0f - (o.t - half) / half : o.t / half;
            const float hx = s.in.px + o.dirX * 320.0f * out;
            const float hy = s.in.py + o.dirY * 320.0f * out;
            o.x2 = hx; o.y2 = hy;
            ForEachEnemy([&](auto& e, float x, float y) {
                if ((x - hx) * (x - hx) + (y - hy) * (y - hy) > 36.0f * 36.0f) return;
                if (std::find(o.hits.begin(), o.hits.end(), (const void*)&e) != o.hits.end()) return;
                o.hits.push_back(&e);
                float dmg = 300.0f;
                if (Has(AugType::P_SHADOW_MULTI_HIT_AMP)) dmg *= 1.30f;
                HitOne(e, dmg);
                passive::CountHit();
            });
            break;
        }
        default: break;
        }
    }
    s.objs.erase(std::remove_if(s.objs.begin(), s.objs.end(),
        [](const Obj& o) { return o.t >= o.life; }), s.objs.end());
}

}  // namespace

// ── public ──
void Reset() { s = State{}; }

const Slot& GetSlot(int i) { return s.slots[i]; }
bool SlotSealed(int i) { return i >= 0 && i < kSlots && s.sealed[i]; }

int EquippedCount() {
    int n = 0;
    for (int i = 0; i < kSlots; ++i) n += !s.sealed[i] && s.slots[i].aug >= 0;
    return n;
}

int DayNightUses() { return s.dayNightUses; }

bool CanOffer(int augIdx) {
    if (SlotOf(augIdx) >= 0) return false;
    for (int i = 0; i < kSlots; ++i) if (!s.sealed[i]) return true;
    return false;
}

void Acquire(int augIdx) {
    int target = -1;
    for (int i = 0; i < kSlots && target < 0; ++i)
        if (!s.sealed[i] && s.slots[i].aug < 0) target = i;
    if (target < 0) {   // full: the oldest skill makes room
        int oldest = 1 << 30;
        for (int i = 0; i < kSlots; ++i)
            if (!s.sealed[i] && s.slots[i].order < oldest) { oldest = s.slots[i].order; target = i; }
    }
    if (target < 0) return;
    s.slots[target] = Slot{ augIdx, 0.0f, 0.0f, s.nextOrder++ };
}

void SealSlot() {
    for (int i = kSlots - 1; i >= 0; --i) {
        if (s.sealed[i]) continue;
        s.sealed[i] = true;
        s.slots[i] = Slot{};
        return;
    }
}

float CooldownFor(int augIdx) {
    const AugDef& d = ALL_AUGS[augIdx];
    float base = d.cooldown;
    if (d.type == AugType::A_ABYSS_TIDAL_SURGE) {
        base = 10.0f;
        for (const Slot& o : s.slots)
            if (o.aug >= 0 && o.aug != augIdx) base = std::max(base, CooldownFor(o.aug));
        return base;
    }
    if (Has(AugType::P_CHRONO_TIME_COMPRESSOR) && base <= 12.0f) return 7.0f;
    float cd = base;
    if (Has(AugType::P_NIGHT_DWARF_COOLING)) cd -= 1.0f;
    if (passive::FirescaleOn()) cd -= 3.0f;
    if (Has(AugType::P_ABYSS_OVERCHARGED_CAST)) cd += 5.0f;
    if (passive::DarkWingsOn()) cd -= 2.0f;
    return std::max(1.0f, cd);
}

float DamageMult() {
    float m = 1.0f;
    if (Has(AugType::P_ABYSS_OVERCHARGED_CAST)) m *= 1.80f;
    if (passive::RubyIntact()) m *= 1.80f;
    if (Has(AugType::P_LUCK_WEALTH_AMP))
        m *= 1.0f + std::min(1.0f, 0.01f * (float)std::max(0LL, g_RunStardust));
    return m;
}

void ReduceAllCooldowns(float seconds) {
    for (Slot& o : s.slots) o.cd = std::max(0.0f, o.cd - seconds);
}

void ResetLongestCooldown() {
    Slot* longest = nullptr;
    for (Slot& o : s.slots)
        if (o.aug >= 0 && o.cd > 0.0f && (!longest || o.cd > longest->cd)) longest = &o;
    if (longest) longest->cd = 0.0f;
}

void RecordDamage(float dmg) { s.damage.push_back({ s.clock, dmg }); }
void OnKill() { ++s.waveKills; }
void OnWaveCleared() { s.waveKills = 0; }
void OnDash() { s.ribbonWindow = 0.0f; }   // opens when the dash ends (Step)

void OnPlayerBullet(const Bullet& shot) {
    for (const Obj& o : s.objs) {
        if (o.kind != ObjKind::Clone) continue;
        Bullet b = shot;
        const float offX = o.x - s.in.px, offY = o.y - s.in.py;
        b.x += offX; b.y += offY; b.prevX = b.x; b.prevY = b.y;
        b.dmgMult *= 0.5f * (Has(AugType::P_SHADOW_MULTI_HIT_AMP) ? 1.30f : 1.0f);
        b.color = glm::vec3(0.60f, 0.50f, 0.95f);
        b.blastOnHit = false;
        g_Bullets.push_back(b);
    }
}

void OnSkillBulletHit(const Bullet& bullet, float x, float y, bool killed) {
    switch (bullet.skillTag) {
    case TAG_KNIFE:
        if (killed) {
            for (int i = 0; i < kSlots; ++i)
                if (s.slots[i].aug >= 0 && TypeOf(s.slots[i].aug) == AugType::A_SHADOW_SHARD_VOLLEY)
                    s.slots[i].cd = std::max(0.0f, s.slots[i].cd - 1.0f);
        }
        break;
    case TAG_FIREBALL:
        passive::DamageRadius(x, y, 140.0f, 300.0f, 1.0f, 0.45f, 0.15f, DamageMult());
        break;
    case TAG_STAR:
        if (killed) {
            for (int i = 0; i < 2; ++i) {
                const float a = kTwoPi * passive::Rand01();
                g_Bullets.push_back(SkillShot(x, y, std::cos(a), std::sin(a), 150.0f,
                                              TAG_STAR_CHILD, 850.0f, { 1.0f, 0.95f, 0.60f }));
            }
        }
        break;
    default: break;
    }
}

void StepSkillBullets() {
    for (Bullet& arrow : g_Bullets) {
        if (!arrow.active || !arrow.skillShot || arrow.skillTag != TAG_ARROW) continue;
        const float r = 14.0f * arrow.sizeScale;
        for (Bullet& b : g_Bullets)
            if (b.active && b.isEnemy &&
                (b.x - arrow.x) * (b.x - arrow.x) + (b.y - arrow.y) * (b.y - arrow.y) < r * r)
                b.active = false;
    }
}

void ResetInputEdges() {
    for (bool& held : s.keyHeld) held = true;   // a key down now is not a press
}

void Step(const StepInput& in) {
    s.in = in;
    const float dt = in.dt;
    s.clock += dt;

    // Orbital Return's 3s history.
    s.trail.push_back({ s.clock, in.px, in.py });
    while (!s.trail.empty() && s.trail.front().t < s.clock - 3.0f) s.trail.pop_front();
    while (!s.damage.empty() && s.damage.front().first < s.clock - 3.0f) s.damage.pop_front();

    for (Slot& o : s.slots) o.cd = std::max(0.0f, o.cd - dt);
    for (auto& e : s.moonCd) e.second = std::max(0.0f, e.second - dt);
    s.ribbonWindow = std::max(0.0f, s.ribbonWindow - dt);
    if (s.dashWas && !g_PlayerRuntime.dashActive) s.ribbonWindow = 1.0f;
    s.dashWas = g_PlayerRuntime.dashActive;

    for (int i = 0; i < kSlots; ++i) {
        const bool press = in.keys[i] && !s.keyHeld[i];
        s.keyHeld[i] = in.keys[i];
        if (press || (in.autoUse && s.slots[i].cd <= 0.0f)) TryUse(i);
    }

    // Precision Snipe: fire when the aim time runs out.
    if (s.snipeT >= 0.0f) {
        s.snipeT -= dt;
        if (s.snipeT < 0.0f) {
            float dx, dy;
            AimDir(dx, dy);
            Bullet b = SkillShot(in.px, in.py, dx, dy, 1500.0f, TAG_SNIPE, 3200.0f,
                                 { 1.0f, 0.95f, 0.85f }, 1.4f);
            b.piercing = true;
            b.forceCrit = true;
            b.maxRange = 2.2f * in.screenW / std::max(0.01f, in.viewZoom);
            g_Bullets.push_back(b);
        }
    }
    // Condensed Blast: charge while still; moving or 2s sets it off.
    if (s.meditateT >= 0.0f) {
        s.meditateT += dt;
        if (in.moving || s.meditateT >= 2.0f) {
            const float k = std::min(1.0f, s.meditateT / 2.0f);
            passive::DamageRadius(in.px, in.py, 300.0f, 2000.0f * k, 0.70f, 1.0f, 0.85f, DamageMult());
            g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 10.0f * k);
            s.meditateT = -1.0f;
        }
    }
    StepObjects(dt);
}

bool TakeTeleport(float& x, float& y) {
    if (!s.teleport) return false;
    s.teleport = false;
    x = s.teleX; y = s.teleY;
    return true;
}

void DrawWorld(float time) {
    using namespace hud_lines;
    BindMainShader();
    for (const Obj& o : s.objs) {
        const float fade = o.life > 0.0f && o.life < 1e8f
            ? std::clamp((o.life - o.t) / 0.3f, 0.0f, 1.0f) : 1.0f;
        switch (o.kind) {
        case ObjKind::Wall: {   // outline of the wall's slab, lines only
            const float len = std::sqrt(o.x2 * o.x2 + o.y2 * o.y2);
            const float nx = -o.y2 / len * 8.0f, ny = o.x2 / len * 8.0f;
            const float px[4] = { o.x - o.x2 + nx, o.x + o.x2 + nx, o.x + o.x2 - nx, o.x - o.x2 - nx };
            const float py[4] = { o.y - o.y2 + ny, o.y + o.y2 + ny, o.y + o.y2 - ny, o.y - o.y2 - ny };
            ClosedOutline(px, py, 4, 2.0f, o.col, 0.9f * fade, 0.0f);
            Segment(o.x - o.x2, o.y - o.y2, o.x + o.x2, o.y + o.y2, 1.0f, o.col, 0.5f * fade);
            break;
        }
        case ObjKind::Meteor: {
            const float k = std::clamp(o.t / o.life, 0.0f, 1.0f);
            Ring(o.x, o.y, 300.0f, 2.0f, o.col, 0.6f, 0.0f);
            Ring(o.x, o.y, 300.0f * (1.0f - k), 2.0f, o.col, 0.9f, 0.0f);
            break;
        }
        case ObjKind::Dynamite: {
            const float k = std::clamp(o.t / 0.35f, 0.0f, 1.0f);
            const float x = o.x + (o.x2 - o.x) * k, y = o.y + (o.y2 - o.y) * k;
            Ring(x, y, 10.0f, 2.0f, o.col, 1.0f, 0.0f);
            Ring(o.x2, o.y2, 200.0f, 1.5f, o.col, 0.35f, 0.0f);
            break;
        }
        case ObjKind::Prism: {
            const float r = 70.0f;
            const float px[4] = { o.x, o.x + r, o.x, o.x - r };
            const float py[4] = { o.y - r, o.y, o.y + r, o.y };
            ClosedOutline(px, py, 4, 2.0f, o.col, 0.9f * fade, 0.0f);
            break;
        }
        case ObjKind::Gate:
            for (int i = 0; i < 3; ++i)
                Arc(o.x, o.y, 40.0f + 30.0f * i, 2.0f, time * (3.0f - i) + i, 4.0f, o.col, 0.8f * fade);
            break;
        case ObjKind::Trap:
            Segment(o.x - 14, o.y - 14, o.x + 14, o.y + 14, 2.0f, o.col, 0.9f);
            Segment(o.x - 14, o.y + 14, o.x + 14, o.y - 14, 2.0f, o.col, 0.9f);
            Ring(o.x, o.y, 22.0f, 1.5f, o.col, 0.5f, 0.0f);
            break;
        case ObjKind::Turret: {
            const float px[4] = { o.x - 14, o.x + 14, o.x + 14, o.x - 14 };
            const float py[4] = { o.y - 14, o.y - 14, o.y + 14, o.y + 14 };
            ClosedOutline(px, py, 4, 2.0f, o.col, 0.9f * fade, 0.0f);
            break;
        }
        case ObjKind::Cloud:
            Arc(o.x, o.y, 34.0f, 2.0f, 3.4f, 2.6f, o.col, 0.8f * fade);
            Segment(o.x - 40, o.y + 6, o.x + 40, o.y + 6, 2.0f, o.col, 0.8f * fade);
            break;
        case ObjKind::Clone:
            Ring(o.x, o.y, 14.0f, 2.0f, o.col, 0.8f * fade, 0.0f);
            Ring(o.x, o.y, 24.0f + 3.0f * std::sin(time * 6.0f), 1.0f, o.col, 0.4f * fade, 0.0f);
            break;
        case ObjKind::Hoop:
            Ring(o.x2, o.y2, 22.0f, 3.0f, o.col, 0.9f, 0.0f);
            break;
        case ObjKind::Line:
            for (size_t i = 2; i + 1 < o.pts.size(); i += 2)
                Segment(o.pts[i - 2], o.pts[i - 1], o.pts[i], o.pts[i + 1], 3.0f, o.col, 0.9f * fade);
            break;
        case ObjKind::Strike:
            Segment(o.x, o.y - 260.0f, o.x - 10.0f, o.y - 120.0f, 3.0f, o.col, fade);
            Segment(o.x - 10.0f, o.y - 120.0f, o.x + 8.0f, o.y - 60.0f, 3.0f, o.col, fade);
            Segment(o.x + 8.0f, o.y - 60.0f, o.x, o.y, 3.0f, o.col, fade);
            break;
        }
    }
    // Precision Snipe aim line and Condensed Blast charge.
    if (s.snipeT >= 0.0f) {
        float dx, dy;
        AimDir(dx, dy);
        Segment(s.in.px, s.in.py, s.in.px + dx * 1600.0f, s.in.py + dy * 1600.0f, 1.0f,
                { 1.0f, 0.95f, 0.85f }, 0.6f);
    }
    if (s.meditateT >= 0.0f)
        Ring(s.in.px, s.in.py, 40.0f + 260.0f * std::min(1.0f, s.meditateT / 2.0f), 2.0f,
             { 0.70f, 1.0f, 0.85f }, 0.8f, 0.0f);
    BatchFlush();
}

}  // namespace active
