#include "ActiveSkills.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "AugmentCombat.h"
#include "RangeUnit.h"
#include "Bullet.h"
#include "GameManager.h"
#include "HudLines.h"
#include "LineFx.h"
#include "StrikeFx.h"
#include "SkillTags.h"
#include "Juice.h"
#include "MonsterManager.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include "SaveSystem.h"

extern GameManager         g_GameManager;
extern MonsterManager      g_MonsterManager;
extern std::vector<Bullet> g_Bullets;
extern PlayerStats         g_Stats;

// Screen shake (main.cpp).
extern float g_ShakeTime;
extern float g_ShakeMag;

namespace active {
namespace {

using hud_lines::Rgb;
constexpr float kTwoPi = 6.2831853f;

// Shots that need follow-up on a hit (Bullet::skillTag): SkillTags.h.

// Field objects a skill leaves behind.
enum class ObjKind { Wall, Meteor, Dynamite, Prism, Gate, Trap, Turret, Storm,
                     Clone, Hoop, Line, Strike };
// How a Line object is drawn.
enum class LineStyle { Bolt, Execute, Gust, Warp, Rush, Rewind, Beam };
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
    LineStyle style = LineStyle::Bolt;
    bool fromSource = false;         // strike: starts at (x2, y2) instead of the sky
    uint32_t seed = 0;               // shape and flicker of bolts and rocks
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
    float meditateWait = -1.0f;   // cast while moving: time waited for a stop, -1 = charging
    int   meditateSlot = -1;      // the slot that paid for the charge (refund)
    int   castSlot = -1;          // slot of the skill being used now
    bool  echoing = false;        // inside an Echo Cast recast
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

// Precision Snipe's reach: the shot's range and its aim line.
float SnipeRange() { return 2.2f * s.in.screenW / std::max(0.01f, s.in.viewZoom); }

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

// StepObjects walks s.objs with a range-for; objects made during that walk (Storm Nebula's
// bolts) wait here and join after it, so the vector never reallocates under the loop.
bool g_SteppingObjs = false;
std::vector<Obj> g_PendingObjs;
void PushObj(Obj&& o) {
    if (g_SteppingObjs) g_PendingObjs.push_back(std::move(o));
    else s.objs.push_back(std::move(o));
}

void AddLine(std::vector<float> pts, Rgb col, float life, LineStyle style) {
    Obj o;
    o.kind = ObjKind::Line;
    o.pts = std::move(pts);
    o.col = col;
    o.life = life;
    o.style = style;
    o.seed = line_fx::Hash((uint32_t)std::rand());
    PushObj(std::move(o));
}

// A lightning bolt onto (x, y): from the sky, or from (sx, sy) when fromSource.
void AddStrike(float x, float y, Rgb col, bool fromSource = false, float sx = 0, float sy = 0) {
    Audio::Play(Audio::Cue::Lightning);
    Obj o;
    o.kind = ObjKind::Strike;
    o.x = x; o.y = y;
    o.x2 = sx; o.y2 = sy;
    o.fromSource = fromSource;
    o.col = col;
    o.life = 0.30f;
    o.seed = line_fx::Hash((uint32_t)std::rand());
    PushObj(std::move(o));
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
        if (picks[i].ranged) HitOne(*(RangedMob*)picks[i].p, 400.0f);
        else                 HitOne(*(Monster*)picks[i].p, 400.0f);
        AddStrike(picks[i].x, picks[i].y, { 0.65f, 0.80f, 1.0f }, true, s.in.px, s.in.py);
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
        Audio::Play(Audio::Cue::SfxChainZap);
        if (ranged) HitOne(*(RangedMob*)target, 350.0f * passive::MultiHitMult());
        else        HitOne(*(Monster*)target, 350.0f * passive::MultiHitMult());
        pts.push_back(tx); pts.push_back(ty);
        fromX = tx; fromY = ty;
        reach = 320.0f;
    }
    AddLine(pts, { 0.70f, 0.85f, 1.0f }, 0.50f, LineStyle::Bolt);   // hops land one after another
}

// Pinpoint Execution's target: the nearest enemy in the aim cone within 12 rings.
void* FindExecuteTarget(bool& ranged, float& tx, float& ty) {
    float dx, dy;
    AimDir(dx, dy);
    float best = Rings(12);
    void* target = nullptr;
    ForEachEnemy([&](auto& e, float x, float y) {
        const float ex = x - s.in.px, ey = y - s.in.py;
        const float d = std::sqrt(ex * ex + ey * ey);
        if (d < 1.0f || (ex * dx + ey * dy) / d < 0.80f) return;   // ~37° either side
        if (d < best) {
            best = d; target = (void*)&e; tx = x; ty = y;
            ranged = std::is_same<std::decay_t<decltype(e)>, RangedMob>::value;
        }
    });
    return target;
}

void CastExecute(int aug) {
    bool ranged = false;
    float tx = 0, ty = 0;
    void* target = FindExecuteTarget(ranged, tx, ty);
    if (!target) return;
    bool killed;
    if (ranged) { RangedMob& r = *(RangedMob*)target; HitOne(r, 1000.0f); killed = !r.alive; }
    else        { Monster& m = *(Monster*)target; HitOne(m, 1000.0f); killed = !m.alive; }
    AddLine({ s.in.px, s.in.py, tx, ty }, { 1.0f, 0.35f, 0.35f }, 0.35f, LineStyle::Execute);
    Audio::Play(Audio::Cue::SkillExecute);
    const int slot = SlotOf(aug);
    if (killed && slot >= 0) { s.slots[slot].cd = 0.0f; Audio::Play(Audio::Cue::SkillCooldownCut); }
}

void CastGust() {
    float dx, dy;
    AimDir(dx, dy);
    constexpr float kRange = Rings(5), kCos = 0.70f;   // ~45° either side
    Audio::Play(Audio::Cue::SkillWave);
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
            { 0.70f, 1.0f, 0.85f }, 0.40f, LineStyle::Gust);
}

void CastBlink() {
    float dx, dy;
    AimDir(dx, dy);
    float tx = s.in.px + dx * Rings(3.5f), ty = s.in.py + dy * Rings(3.5f);
    ClampToView(tx, ty);
    passive::ClearEnemyBullets(s.in.px, s.in.py, Rings(2));
    passive::ClearEnemyBullets(tx, ty, Rings(2));
    AddLine({ s.in.px, s.in.py, tx, ty }, { 0.70f, 0.90f, 1.0f }, 0.35f, LineStyle::Warp);
    Audio::Play(Audio::Cue::SkillTeleport);
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
    AddLine({ s.in.px, s.in.py, tx, ty }, { 1.0f, 0.85f, 0.45f }, 0.40f, LineStyle::Rush);
    Audio::Play(Audio::Cue::SkillRush);
    g_PlayerRuntime.dashInvulnerability = std::max(g_PlayerRuntime.dashInvulnerability, 0.25f);
    s.teleport = true; s.teleX = tx; s.teleY = ty;
}

void CastRewind() {
    if (s.trail.empty()) return;
    const State::Pos& p = s.trail.front();   // the oldest kept: ~3s ago
    float lost = 0.0f;
    if (!s.echoing)   // an echoed return moves you but does not heal again
        for (const auto& d : s.damage) lost += d.second;
    g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + lost * 0.5f);
    AddLine({ s.in.px, s.in.py, p.x, p.y }, { 0.85f, 0.75f, 1.0f }, 0.45f, LineStyle::Rewind);
    Audio::Play(Audio::Cue::SkillTeleport);
    if (lost > 0.0f) Audio::Play(Audio::Cue::SfxHealBig);
    s.teleport = true; s.teleX = p.x; s.teleY = p.y;
    // The history is used up: a second return within 3s can't heal the same damage.
    s.trail.clear();
    s.damage.clear();
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
        AddLine({ s.in.px, s.in.py, ex, ey }, { 1.0f, 0.92f, 0.60f }, 0.35f, LineStyle::Beam);
        Audio::Play(Audio::Cue::SkillBeam);
    } else {
        passive::DamageRadius(s.in.px, s.in.py, Rings(3.5f), 400.0f, 0.45f, 0.30f, 0.85f, DamageMult(), strike_fx::BlastStyle::Stellar);
        passive::ClearEnemyBullets(s.in.px, s.in.py, Rings(3.5f));
    }
    s.dayNightUses = (s.dayNightUses + 1) % 6;
}

void CastBookOfCheats() {
    int roll = std::rand() % 8;
    if (roll == 7 && passive::Chance(0.25f)) roll = std::rand() % 7;   // luck can reroll "nothing"
    Audio::Play(Audio::Cue::SkillDice);
    // What came up, over the ship (KR / EN / JP).
    static const wchar_t* const kRoll[8][3] = {
        { L"전체 낙뢰", L"Storm", L"全体落雷" },
        { L"회복", L"Heal", L"回復" },
        { L"탄막 소거", L"Clear", L"弾幕消去" },
        { L"대시 초기화", L"Dash", L"ダッシュリセット" },
        { L"별가루", L"Stardust", L"星屑" },
        { L"유도 파편", L"Shards", L"誘導破片" },
        { L"근접 폭발", L"Blast", L"近接爆発" },
        { L"꽝", L"Blank", L"ハズレ" },
    };
    SpawnWorldText(s.in.px, s.in.py - 70.0f, kRoll[roll][std::clamp(LangIndex(), 0, 2)],
                   roll == 7 ? 0.65f : 1.0f, roll == 7 ? 0.65f : 0.85f, roll == 7 ? 0.70f : 0.40f, 1.4f);
    switch (roll) {
    case 0: passive::DamageAll(500.0f, 0.65f, 0.80f, 1.0f, false, DamageMult(), passive::ScreenFx::Zap); break;
    case 1:
        g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 20.0f);
        Audio::Play(Audio::Cue::SfxHealBig);
        strike_fx::Implode(s.in.px, s.in.py, 110.0f, { 0.70f, 1.0f, 0.75f });
        break;
    case 2:
        passive::ClearEnemyBullets(0.0f, 0.0f, -1.0f);
        break;
    case 3: passive::ResetDash(); strike_fx::Implode(s.in.px, s.in.py, 90.0f, { 0.40f, 0.95f, 0.95f }); break;
    case 4: passive::AddStardust(10); Audio::Play(Audio::Cue::ProcStardustGain); break;
    case 5:
        for (int i = 0; i < 12; ++i) {
            const float a = kTwoPi * (float)i / 12.0f;
            Bullet b = SkillShot(s.in.px, s.in.py, std::cos(a), std::sin(a), 150.0f,
                                 TAG_SHARD, 700.0f, { 1.0f, 0.85f, 0.40f });
            b.homing = true; b.homingTurn = 6.0f;
            g_Bullets.push_back(b);
        }
        break;
    case 6: passive::DamageRadius(s.in.px, s.in.py, Rings(3.5f), 800.0f, 1.0f, 0.55f, 0.25f, DamageMult()); break;
    default: break;   // nothing happens
    }
}

// What Echo Cast would recast: the last skill used, if still equipped and not a move.
int EchoTarget() {
    const int last = s.lastUsedAug;
    if (last < 0 || SlotOf(last) < 0) return -1;
    switch (TypeOf(last)) {
    case AugType::A_PERFORM_ECHO_CAST:
    case AugType::A_FLIGHT_WARP_JUMP:
    case AugType::A_FLIGHT_COMET_RUSH:
        return -1;
    default: return last;
    }
}

// False when using aug now would do nothing (the cooldown is then kept).
bool CanCast(int aug) {
    switch (TypeOf(aug)) {
    case AugType::A_STILL_CONDENSED_BLAST: return s.meditateT < 0.0f;   // still charging
    case AugType::A_EDGE_PRECISION_SNIPE:  return s.snipeT < 0.0f;      // still aiming
    case AugType::A_NOVA_NOVA_BURST:       return s.waveKills > 0;
    case AugType::A_HUNTER_PINPOINT_EXECUTION: {
        bool ranged; float tx, ty;
        return FindExecuteTarget(ranged, tx, ty) != nullptr;
    }
    case AugType::A_PERFORM_ECHO_CAST: {
        const int target = EchoTarget();
        return target >= 0 && CanCast(target);
    }
    default: return true;
    }
}

// 블랙홀 포획: the seed's flight from the ship before the hole opens (it is
// added to the hole's life so the card's 3s is all open).
static constexpr float kHoleSeed = 0.25f;
// The last kHoleSuck seconds: it inhales hard (pull up to 3.5x, drawn as
// streaks rushing in), then snaps to a point for kHoleSnap and lets go.
static constexpr float kHoleSuck = 0.55f, kHoleSnap = 0.12f;

void Cast(int aug) {
    gamelog::Cast();
    gamelog::Event("SKILL_CAST", "aug=%s", ALL_AUGS[aug].name);
    Audio::Play(Audio::Cue::SkillCast);
    const float px = s.in.px, py = s.in.py;
    float dx, dy;
    AimDir(dx, dy);
    const float extra = DamageMult();
    switch (TypeOf(aug)) {
    case AugType::A_NIGHT_STARFALL:
        passive::DamageAll(1200.0f, 0.75f, 0.70f, 1.0f, false, extra, passive::ScreenFx::StarFall);
        Audio::Play(Audio::Cue::SkillScreenwide);
        break;
    case AugType::A_NIGHT_METEOR_LANCE: {
        Bullet b = SkillShot(px, py, dx, dy, 600.0f, TAG_ARROW, 420.0f, { 0.75f, 0.70f, 1.0f }, 2.6f);
        b.piercing = true;
        g_Bullets.push_back(b);
        Audio::Play(Audio::Cue::SkillProjLaunch);
        break;
    }
    case AugType::A_CHRONO_ORBITAL_RETURN: CastRewind(); break;
    case AugType::A_CHRONO_RESONANT_SYNC:
        for (int i = 0; i < kSlots; ++i)   // not itself, nor the Echo Cast that recast it
            if (s.slots[i].aug >= 0 && s.slots[i].aug != aug && i != s.castSlot) s.slots[i].cd *= 0.5f;
        Audio::Play(Audio::Cue::SkillCooldownCut);
        // Time folds in on the ship: two rings closing, one after the other.
        strike_fx::Implode(px, py, 150.0f, { 0.55f, 0.95f, 1.0f }, 0.0f, 0.32f);
        strike_fx::Implode(px, py, 95.0f, { 0.80f, 1.0f, 1.0f }, 0.12f, 0.26f);
        break;
    case AugType::A_GALE_STELLAR_GALE: CastGust(); break;
    case AugType::A_SHADOW_SHARD_VOLLEY:
        for (int i = 0; i < 8; ++i) {
            const float a = kTwoPi * (float)i / 8.0f;
            g_Bullets.push_back(SkillShot(px, py, std::cos(a), std::sin(a), 300.0f * passive::MultiHitMult(),
                                          TAG_KNIFE, 950.0f, { 0.60f, 0.50f, 0.95f }));
        }
        Audio::Play(Audio::Cue::SkillProjMulti);
        break;
    case AugType::A_SHADOW_TWIN_STAR_CLONE: {
        Obj o; o.kind = ObjKind::Clone; o.x = px; o.y = py; o.life = 8.0f * MetaDurationMult();
        o.col = { 0.60f, 0.50f, 0.95f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillSummon);
        break;
    }
    case AugType::A_STONE_GRAVITY_SLAM:
        passive::DamageRadius(px, py, Rings(5), 800.0f, 0.85f, 0.70f, 0.50f, extra, strike_fx::BlastStyle::Gravity);
        passive::ClearEnemyBullets(px, py, Rings(5));
        Audio::Play(Audio::Cue::SkillSlam);
        break;
    case AugType::A_STONE_ASTEROID_WALL: {
        Obj o; o.kind = ObjKind::Wall; o.life = 6.0f * MetaDurationMult();
        o.x = px + dx * 130.0f; o.y = py + dy * 130.0f;
        o.x2 = -dy * 120.0f; o.y2 = dx * 120.0f;    // half-extent along the wall
        o.col = { 0.85f, 0.70f, 0.50f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillWall);
        break;
    }
    case AugType::A_FLAME_PROMINENCE_SHELL: {
        Bullet b = SkillShot(px, py, dx, dy, 400.0f, TAG_FIREBALL, 800.0f, { 1.0f, 0.45f, 0.15f }, 1.6f);
        b.maxRange = 1100.0f;
        g_Bullets.push_back(b);
        Audio::Play(Audio::Cue::SkillProjLaunch);
        break;
    }
    case AugType::A_FLAME_METEOR_BARRAGE: {
        // Thrown from the ship: (x2, y2) is where it left, (x, y) where it lands.
        Obj o; o.kind = ObjKind::Meteor; o.x = s.in.aimX; o.y = s.in.aimY; o.life = 1.0f;
        o.x2 = px; o.y2 = py;
        o.col = { 1.0f, 0.45f, 0.15f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillTelegraph);   // 1초 예고 링 (착탄은 DamageRadius 폭발음)
        break;
    }
    case AugType::A_GEM_REFRACTION_CRYSTAL: {
        Obj o; o.kind = ObjKind::Prism; o.x = px; o.y = py; o.life = 4.0f * MetaDurationMult();
        o.col = { 0.75f, 0.95f, 1.0f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillSummon);
        break;
    }
    case AugType::A_GEM_BARRAGE_RECYCLE: {
        const int n = std::min(24, passive::ClearEnemyBullets(px, py, Rings(3), false));
        strike_fx::Implode(px, py, Rings(3), { 1.0f, 0.45f, 0.55f }, 0.0f, 0.30f);   // drawn into the ship
        Audio::Play(Audio::Cue::SkillAbsorb);
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
        // No cloud: for 6s the ship itself throws a bolt every 0.5s.
        Obj o; o.kind = ObjKind::Storm; o.life = 6.0f * MetaDurationMult(); o.col = { 0.65f, 0.80f, 1.0f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillSummon);
        break;
    }
    case AugType::A_LUCK_DICE_OF_FATE: CastBookOfCheats(); break;
    case AugType::A_LIFE_STARLIGHT_MEND:
        g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 20.0f);
        passive::ClearEnemyBullets(px, py, Rings(3));
        Audio::Play(Audio::Cue::SfxHealBig);
        // The cleared reach rings out; the heal gathers into the ship.
        strike_fx::Implode(px, py, 90.0f, { 0.70f, 1.0f, 0.75f }, 0.10f, 0.30f);
        break;
    case AugType::A_ABYSS_TIDAL_SURGE:
        passive::DamageRadius(px, py, Rings(5), 1000.0f, 0.30f, 0.60f, 1.0f, extra, strike_fx::BlastStyle::Gravity);
        Audio::Play(Audio::Cue::SkillWave);
        break;
    case AugType::A_ABYSS_BLACK_HOLE_CAPTURE: {
        Obj o; o.kind = ObjKind::Gate; o.x = s.in.aimX; o.y = s.in.aimY; o.life = 3.0f * MetaDurationMult() + kHoleSeed;
        o.col = { 0.55f, 0.35f, 0.95f };
        o.x2 = px; o.y2 = py;   // thrown from the ship: the seed flies from here (DrawBlackHole)
        s.objs.push_back(o);
        Audio::StartLoop(Audio::Cue::SkillVortexLoop);
        break;
    }
    case AugType::A_SPARK_FORCED_DISCHARGE:
        passive::DamageRadius(px, py, Rings(3.5f), 350.0f, 0.55f, 0.85f, 1.0f, extra, strike_fx::BlastStyle::Discharge);
        for (int i = 0; i < 10; ++i) passive::CountHit();
        Audio::Play(Audio::Cue::SkillWave);
        break;
    case AugType::A_SPARK_CHAIN_LIGHTNING: CastChainLightning(); break;
    case AugType::A_FLIGHT_WARP_JUMP: CastBlink(); break;
    case AugType::A_FLIGHT_COMET_RUSH: CastCometRush(); break;
    case AugType::A_EDGE_PRECISION_SNIPE:
        s.snipeT = 0.6f;
        Audio::Play(Audio::Cue::SkillTelegraph);   // 0.6초 조준선
        Audio::StartLoop(Audio::Cue::SkillChargeLoop);
        break;
    case AugType::A_HERESY_LIFE_BURN_BLAST: {
        const float spent = g_GameManager.playerHP * 0.25f;
        g_GameManager.playerHP -= spent;
        Audio::Play(Audio::Cue::SkillHpCost);
        passive::DamageAll(110.0f * spent, 0.85f, 0.20f, 0.30f, false, extra, passive::ScreenFx::Nova);
        Audio::Play(Audio::Cue::SkillScreenwide);
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
        Audio::Play(Audio::Cue::SkillSummon);
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
        Audio::StartLoop(Audio::Cue::SkillBoomerangLoop);
        break;
    case AugType::A_PERFORM_ECHO_CAST: {
        const int target = EchoTarget();
        if (target >= 0) {
            Audio::Play(Audio::Cue::SkillCooldownCut);
            s.echoing = true;
            Cast(target);
            s.echoing = false;
        }
        break;
    }
    case AugType::A_STILL_STARLIGHT_TURRET: {
        Obj o; o.kind = ObjKind::Turret; o.x = px; o.y = py; o.life = 10.0f * MetaDurationMult();
        o.col = { 0.60f, 0.95f, 0.80f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillSummon);
        break;
    }
    case AugType::A_STILL_CONDENSED_BLAST:
        s.meditateT = 0.0f;
        s.meditateWait = s.in.moving ? 0.0f : -1.0f;   // used on the move: charge from the first stop
        s.meditateSlot = s.castSlot;
        Audio::StartLoop(Audio::Cue::SkillChargeLoop);
        break;
    case AugType::A_VEIN_TIMED_BOMB: {
        Obj o; o.kind = ObjKind::Dynamite; o.x = px; o.y = py;
        o.x2 = s.in.aimX; o.y2 = s.in.aimY; o.life = 1.0f;
        o.col = { 1.0f, 0.55f, 0.25f };
        s.objs.push_back(o);
        Audio::Play(Audio::Cue::SkillFuse);
        break;
    }
    case AugType::A_NOVA_NOVA_BURST: {
        const float pct = std::min(2000.0f, 20.0f * (float)s.waveKills);   // > 0: CanCast holds it at 0 kills
        if (pct > 0.0f) { passive::DamageAll(pct, 1.0f, 0.95f, 0.75f, false, extra, passive::ScreenFx::Nova); Audio::Play(Audio::Cue::SkillScreenwide); }
        break;
    }
    case AugType::A_NOVA_STARLIGHT_SCATTER:
        for (int i = 0; i < 12; ++i) {
            const float a = kTwoPi * (float)i / 12.0f;
            g_Bullets.push_back(SkillShot(px, py, std::cos(a), std::sin(a), 150.0f * passive::MultiHitMult(),
                                          TAG_STAR, 850.0f, { 1.0f, 0.95f, 0.60f }));
        }
        Audio::Play(Audio::Cue::SkillProjMulti);
        break;
    default: break;
    }
}

// The passive follow-ups of using a skill (not of an Echo Cast recast).
void AfterUse(int aug, int slot) {
    const float cd = CooldownFor(aug);
    if (Has(AugType::P_CHRONO_DOUBLE_CAST) && passive::Chance(0.30f)) {
        s.slots[slot].cd = 0.0f;
        Audio::Play(Audio::Cue::SkillCooldownCut);
        passive::ChanceTriggered();
    }
    if (Has(AugType::P_STORM_MAGNETIC_STORM) && passive::Chance(0.20f)) {
        passive::DamageAll(200.0f, 0.65f, 0.80f, 1.0f, false, 1.0f, passive::ScreenFx::Zap);
        Audio::Play(Audio::Cue::SkillScreenwide);
        passive::ChanceTriggered();
    }
    if (Has(AugType::P_NIGHT_RESONANT_SHOCKWAVE) && s.moonCd[aug] <= 0.0f) {
        s.moonCd[aug] = 12.0f;
        passive::DamageRadius(s.in.px, s.in.py, Rings(3), 800.0f, 0.85f, 0.85f, 1.0f, 1.0f, strike_fx::BlastStyle::Stellar);
        Audio::Play(Audio::Cue::ProcShockwave);
    }
    if (Has(AugType::P_ABYSS_SINGULARITY_CASCADE) && cd >= 10.0f) {
        const int n = std::min(4, 1 + (int)((cd - 10.0f) / 10.0f));   // the card says "up to 4"
        passive::DamageAll(400.0f, 0.55f, 0.35f, 0.95f, false, 1.0f, passive::ScreenFx::Nova);
        for (int i = 1; i < n; ++i) {
            passive::DamageAll(400.0f, 0.55f, 0.35f, 0.95f);
            strike_fx::Nova(s.in.px, s.in.py, 1400.0f, { 0.55f, 0.35f, 0.95f }, 0.14f * (float)i);
        }
    }
    if (Has(AugType::P_PERFORM_ECLIPSE_COMBO) && s.ribbonWindow > 0.0f) {
        s.ribbonWindow = 0.0f;
        passive::DamageRadius(s.in.px, s.in.py, Rings(5), 250.0f, 1.0f, 0.60f, 0.85f, 1.0f, strike_fx::BlastStyle::Stellar);
        Audio::Play(Audio::Cue::ProcShockwave);
    }
    if (Has(AugType::P_HERESY_UNSTABLE_DRIVE) && s.lastUsedAug == aug) {
        if (passive::DarkWingsOn()) Audio::Play(Audio::Cue::ProcSeal);   // 이번 웨이브 동안 꺼짐
        passive::SwitchOffDarkWings();
    }
    if (TypeOf(aug) != AugType::A_PERFORM_ECHO_CAST) s.lastUsedAug = aug;
}

bool TryUse(int slot) {
    Slot& sl = s.slots[slot];
    if (s.sealed[slot] || sl.aug < 0 || sl.cd > 0.0f) return false;
    // Nothing to do (pending snipe/charge, no target, no kills, nothing to echo): keep the cooldown.
    // Echo checks its target, so it also waits for a pending snipe or charge to land.
    if (!CanCast(sl.aug)) return false;
    sl.cd = sl.cdMax = CooldownFor(sl.aug);
    s.castSlot = slot;
    Cast(sl.aug);
    s.castSlot = -1;
    AfterUse(sl.aug, slot);
    return true;
}

// 블랙홀 포획 letting go: everything it swallowed thrown back out at once.
// Layers (particle drag is 2.5/s, so reach ~ speed / 2.5):
//   white-hot sparks from the core, a ring of particles riding the shock
//   front out past the blast, a mass of matter flung in every direction at
//   mixed speeds and sizes, and slow embers left hanging after it.
void SpawnHoleBlast(float x, float y, line_fx::Rgb c) {
    const line_fx::Rgb hi = line_fx::Lift(c, 0.55f);
    auto rnd = []() { return (float)(rand() % 10001) / 10000.0f; };
    SpawnSparks(x, y, 70, 1.0f, 0.94f, 1.0f, 1500.0f);
    int slot = 0;
    const int cap = EnemyParticleCap();
    auto put = [&](float px, float py, float vx, float vy, float life, float size,
                   line_fx::Rgb col, EnemyParticleShape shape, float delay) {
        while (slot < cap && g_EnemyParts[slot].active) ++slot;
        if (slot >= cap) return;
        EnemyParticle& e = g_EnemyParts[slot];
        e = {};
        e.x = px; e.y = py; e.vx = vx; e.vy = vy;
        e.life = e.maxLife = life; e.size = size;
        e.r = col.r; e.g = col.g; e.b = col.b;
        e.active = true; e.shape = shape; e.delay = delay;
    };
    // Shock front: an even ring, fast, small and bright (~5.5 rings out).
    for (int i = 0; i < 72; ++i) {
        const float ang = 6.2831853f * (float)i / 72.0f;
        const float v = 1100.0f + 80.0f * rnd();
        put(x, y, std::cos(ang) * v, std::sin(ang) * v, 0.60f, 5.5f, hi, EnemyParticleShape::CIRCLE, 0.0f);
    }
    // The swallowed matter: every direction, mixed speeds (200 - 800 px) and sizes.
    for (int i = 0; i < 180; ++i) {
        const float ang = rnd() * 6.2831853f;
        const float v = 500.0f + 1500.0f * rnd() * rnd();
        const float r0 = 30.0f * rnd();
        const float k = rnd();
        const line_fx::Rgb col = k < 0.20f ? line_fx::Rgb{ 1.0f, 0.95f, 1.0f }
                               : k < 0.60f ? hi : c;
        const EnemyParticleShape sh = static_cast<EnemyParticleShape>(rand() % 3);
        put(x + std::cos(ang) * r0, y + std::sin(ang) * r0, std::cos(ang) * v, std::sin(ang) * v,
            0.55f + 0.55f * rnd(), 4.0f + 13.0f * rnd() * rnd(), col, sh, 0.0f);
    }
    // Embers that hang in the air after the blast has passed.
    for (int i = 0; i < 40; ++i) {
        const float ang = rnd() * 6.2831853f;
        const float v = 150.0f + 300.0f * rnd();
        put(x, y, std::cos(ang) * v, std::sin(ang) * v, 1.0f + 0.4f * rnd(), 2.0f + 2.0f * rnd(),
            c, EnemyParticleShape::SQUARE, 0.06f + 0.10f * rnd());
    }
}

void StepObjects(float dt) {
    g_SteppingObjs = true;
    for (Obj& o : s.objs) {
        o.t += dt;
        switch (o.kind) {
        case ObjKind::Wall: {
            const float ax = o.x - o.x2, ay = o.y - o.y2, bx = o.x + o.x2, by = o.y + o.y2;
            for (Bullet& b : g_Bullets)
                if (b.active && b.isEnemy && SegDist2(b.x, b.y, ax, ay, bx, by) < 16.0f * 16.0f) {
                    b.active = false;
                    Audio::Play(Audio::Cue::SkillWall);   // 적탄 막힘
                }
            if (o.t >= o.life) Audio::Play(Audio::Cue::SkillSummonEnd);
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
                passive::DamageRadius(o.x, o.y, Rings(4), 2500.0f, 1.0f, 0.45f, 0.15f, DamageMult());
            }
            break;
        case ObjKind::Dynamite:
            if (o.t >= o.life)
                passive::DamageRadius(o.x2, o.y2, Rings(2.5f), 500.0f, 1.0f, 0.55f, 0.25f, DamageMult());
            break;
        case ObjKind::Prism:
            for (Bullet& b : g_Bullets) {
                if (!b.active || !b.isEnemy) continue;
                if ((b.x - o.x) * (b.x - o.x) + (b.y - o.y) * (b.y - o.y) < 70.0f * 70.0f) {
                    b.active = false;
                    ++o.count;
                    Audio::Play(Audio::Cue::SkillAbsorb);
                }
            }
            if (o.t >= o.life && o.count > 0)
                passive::DamageRadius(o.x, o.y, Rings(3), 70.0f * (float)o.count,
                                      0.75f, 0.95f, 1.0f, DamageMult(), strike_fx::BlastStyle::Crystal);
            break;
        case ObjKind::Gate: {
            // Full pull inside 5 rings; a weak field out to 8 rings that fades
            // from kFieldPull at the edge of the pull to nothing.
            constexpr float kPull = Rings(5), kField = Rings(8), kFieldPull = 90.0f;
            // Nothing pulls while the seed flies; the pull grows as the hole opens.
            const float gu = std::clamp((o.t - kHoleSeed) / 0.55f, 0.0f, 1.0f);
            const float suckU = std::clamp((o.t - (o.life - kHoleSuck)) / (kHoleSuck - kHoleSnap), 0.0f, 1.0f);
            const float grip = gu * gu * (3.0f - 2.0f * gu) * (1.0f + 2.5f * suckU * suckU);
            auto pull = [&](float& x, float& y, float speed) {
                const float ex = o.x - x, ey = o.y - y;
                const float d = std::sqrt(ex * ex + ey * ey);
                if (d < 1.0f || d > kField || grip <= 0.0f) return;
                const float v = grip * (d <= kPull ? speed
                    : kFieldPull * (speed / 260.0f) * (1.0f - (d - kPull) / (kField - kPull)));
                const float step = std::min(d, v * dt);
                x += ex / d * step; y += ey / d * step;
            };
            for (auto* m : g_MonsterManager.monsters)
                if (m->alive && !IsBossMob(m->kind)) pull(m->worldX, m->worldY, 260.0f);
            for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) pull(r->worldX, r->worldY, 260.0f);
            for (Bullet& b : g_Bullets) {
                if (!b.active || !b.isEnemy) continue;
                pull(b.x, b.y, 520.0f);
                if (grip > 0.5f && (b.x - o.x) * (b.x - o.x) + (b.y - o.y) * (b.y - o.y) < 30.0f * 30.0f) {
                    b.active = false;
                    ++o.count;
                }
            }
            if (o.t >= o.life) {
                // The vortex hum stops with the last black hole open, not the first to close.
                bool otherOpen = false;
                for (const Obj& g : s.objs)
                    if (&g != &o && g.kind == ObjKind::Gate && g.t < g.life) otherOpen = true;
                if (!otherOpen) Audio::StopLoop(Audio::Cue::SkillVortexLoop);
                // Collapsed to a point (DrawBlackHole), it lets go: its own burst,
                // not the shared gravity blast.
                passive::BurstDamage(o.x, o.y, Rings(4), (1500.0f + 50.0f * (float)o.count) * DamageMult());
                strike_fx::HoleBurst(o.x, o.y, Rings(4), o.col);
                SpawnHoleBlast(o.x, o.y, o.col);
                Audio::Play(Audio::Cue::ExplodeL);
                // A beat of stillness, then a short jolt.
                TriggerHitStop(0.05f);
                g_ShakeTime = std::max(g_ShakeTime, 0.30f);
                g_ShakeMag = std::max(g_ShakeMag, 12.0f);
            }
            break;
        }
        case ObjKind::Trap: {
            bool sprung = false;
            auto step = [&](auto& e, float x, float y) {
                if (sprung) return;
                if ((x - o.x) * (x - o.x) + (y - o.y) * (y - o.y) < 40.0f * 40.0f) {
                    HitOne(e, 700.0f);
                    sprung = true;
                }
            };
            ForEachEnemy(step);
            if (sprung) {
                o.life = 0.0f;
                Audio::Play(Audio::Cue::ExplodeS);
                strike_fx::Blast(o.x, o.y, 60.0f, { 1.0f, 0.45f, 0.35f }, 0.0f, strike_fx::BlastStyle::Crystal);   // where it went off
            }
            break;
        }
        case ObjKind::Turret:
            o.timer += dt;
            if (o.t >= o.life) Audio::Play(Audio::Cue::SkillSummonEnd);
            if (o.timer >= 0.6f) {
                float tx, ty;
                if (g_MonsterManager.FindNearestEnemy(o.x, o.y, tx, ty)) {
                    o.timer = 0.0f;
                    float ex = tx - o.x, ey = ty - o.y;
                    const float d = std::max(0.001f, std::sqrt(ex * ex + ey * ey));
                    o.dirX = ex / d; o.dirY = ey / d;
                    g_Bullets.push_back(SkillShot(o.x, o.y, ex / d, ey / d, 100.0f * passive::MultiHitMult(), TAG_TURRET,
                                                  1100.0f, { 0.60f, 0.95f, 0.80f }));
                    Audio::Play(Audio::Cue::SkillTurretShot);
                }
            }
            break;
        case ObjKind::Storm:
            if (o.t >= o.life) Audio::Play(Audio::Cue::SkillSummonEnd);
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
                    if (t.second) { RangedMob& r = *(RangedMob*)t.first; HitOne(r, 300.0f); AddStrike(r.worldX, r.worldY, o.col, true, s.in.px, s.in.py); }
                    else          { Monster& m = *(Monster*)t.first; HitOne(m, 300.0f); AddStrike(m.worldX, m.worldY, o.col, true, s.in.px, s.in.py); }
                }
            }
            break;
        case ObjKind::Hoop: {
            const float half = o.life * 0.5f;
            if (o.t >= o.life) Audio::StopLoop(Audio::Cue::SkillBoomerangLoop);   // 고리가 돌아옴
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
                HitOne(e, 200.0f * passive::MultiHitMult());
                passive::CountHit();
            });
            break;
        }
        case ObjKind::Clone:
            if (o.t >= o.life) Audio::Play(Audio::Cue::SkillSummonEnd);
            break;
        default: break;
        }
    }
    g_SteppingObjs = false;
    for (Obj& o : g_PendingObjs) s.objs.push_back(std::move(o));
    g_PendingObjs.clear();
    s.objs.erase(std::remove_if(s.objs.begin(), s.objs.end(),
        [](const Obj& o) { return o.t >= o.life; }), s.objs.end());
}

}  // namespace

// ── public ──
void Reset() {
    // 런 리셋: 돌던 루프 정지
    Audio::StopLoop(Audio::Cue::SkillVortexLoop);
    Audio::StopLoop(Audio::Cue::SkillChargeLoop);
    Audio::StopLoop(Audio::Cue::SkillBoomerangLoop);
    s = State{};
}

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

bool Equipped(int augIdx) { return SlotOf(augIdx) >= 0; }

void Release(int augIdx) {
    const int i = SlotOf(augIdx);
    if (i >= 0) s.slots[i] = Slot{};
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
        base = 14.0f;   // floor: on its own it out-hit Gravity Slam on a shorter cooldown
        for (const Slot& o : s.slots)
            if (o.aug >= 0 && o.aug != augIdx) base = std::max(base, CooldownFor(o.aug));
        return base;
    }
    // The shop's Skill Cooldown upgrade scales the finished cooldown (Tidal Surge
    // above follows the other slots, so it inherits it).
    const float metaMul = MetaCooldownMult();
    if (Has(AugType::P_CHRONO_TIME_COMPRESSOR) && base <= 12.0f) return 7.0f * metaMul;
    float cd = base;
    if (Has(AugType::P_NIGHT_DWARF_COOLING)) cd -= 1.0f;
    if (passive::FirescaleOn()) cd -= 3.0f;
    if (Has(AugType::P_ABYSS_OVERCHARGED_CAST)) cd += 5.0f;
    if (passive::DarkWingsOn()) cd -= 2.0f;
    return std::max(1.0f, cd) * metaMul;
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

void HalveLongestCooldown() {
    Slot* longest = nullptr;
    for (Slot& o : s.slots)
        if (o.aug >= 0 && o.cd > 0.0f && (!longest || o.cd > longest->cd)) longest = &o;
    if (longest) longest->cd *= 0.5f;
}

void RecordDamage(float dmg) { s.damage.push_back({ s.clock, dmg }); }
void OnKill() { ++s.waveKills; }
void OnWaveCleared() { s.waveKills = 0; }
void OnDash() { s.ribbonWindow = 0.0f; }   // opens when the dash ends (Step)

void OnPlayerBullet(const Bullet& shot) {
    if (shot.synFlags & synf::Orbit) return;   // an orbiter is held round the ship: a copy broke the orbiter cap
    for (const Obj& o : s.objs) {
        if (o.kind != ObjKind::Clone) continue;
        Bullet b = shot;
        const float offX = o.x - s.in.px, offY = o.y - s.in.py;
        b.x += offX; b.y += offY; b.prevX = b.x; b.prevY = b.y;
        b.dmgMult *= passive::MultiHitMult();   // the clone's copy hits as hard as the ship's shot
        b.color = glm::vec3(0.60f, 0.50f, 0.95f);
        // "착탄 폭발은 없음": the dash blast and the synergy on-hit blasts (파열, 중력탄) stay with the ship.
        b.blastOnHit = false;
        b.blastPct = 0.0f;
        b.synFlags &= ~synf::Gravity;
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
        passive::DamageRadius(x, y, Rings(2), 200.0f, 1.0f, 0.45f, 0.15f, DamageMult());
        break;
    case TAG_STAR:
        if (killed) {
            for (int i = 0; i < 2; ++i) {
                const float a = kTwoPi * passive::Rand01();
                g_Bullets.push_back(SkillShot(x, y, std::cos(a), std::sin(a), 150.0f * passive::MultiHitMult(),
                                              TAG_STAR_CHILD, 850.0f, { 1.0f, 0.95f, 0.60f }));
            }
        }
        break;
    default: break;
    }
}

// Meteor Lance flight: cruise for 200 px, stall (nearly stopped) for 0.25 s,
// then burst to 2400 px/s over 0.08 s.
constexpr float kLanceCruiseSpeed = 420.0f;
constexpr float kLanceCruiseDist  = 200.0f;
constexpr float kLanceStallSpeed  = 60.0f;
constexpr float kLanceStallTime   = 0.25f;
constexpr float kLanceBurstSpeed  = 2400.0f;
constexpr float kLanceBurstRise   = 0.08f;

static void StepLanceFlight(Bullet& lance, float dt) {
    lance.lanceTimer += dt;
    if (lance.lancePhase == 0 && lance.traveled >= kLanceCruiseDist) {
        lance.lancePhase = 1;
        lance.lanceTimer = 0.0f;
    } else if (lance.lancePhase == 1 && lance.lanceTimer >= kLanceStallTime) {
        lance.lancePhase = 2;
        lance.lanceTimer = 0.0f;
        Audio::Play(Audio::Cue::SkillProjLaunch);
    }
    switch (lance.lancePhase) {
    case 0: lance.speed = kLanceCruiseSpeed; break;
    case 1: lance.speed = kLanceStallSpeed; break;
    default: {
        const float k = std::min(1.0f, lance.lanceTimer / kLanceBurstRise);
        lance.speed = kLanceStallSpeed + (kLanceBurstSpeed - kLanceStallSpeed) * k;
        break;
    }
    }
}

void StepSkillBullets() {
    const float dt = s.in.dt;
    for (Bullet& arrow : g_Bullets) {
        if (!arrow.active || !arrow.skillShot || arrow.skillTag != TAG_ARROW) continue;
        StepLanceFlight(arrow, dt);
        // Erase along the whole stretch flown this step: in the burst one step
        // covers about 40 px, more than the lance's own radius.
        const float r = 14.0f * arrow.sizeScale;
        const float ax = arrow.prevX, ay = arrow.prevY;
        const float sx = arrow.x - ax, sy = arrow.y - ay;
        const float len2 = sx * sx + sy * sy;
        for (Bullet& b : g_Bullets) {
            if (!b.active || !b.isEnemy) continue;
            float t = len2 > 0.0f ? ((b.x - ax) * sx + (b.y - ay) * sy) / len2 : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float qx = ax + sx * t - b.x, qy = ay + sy * t - b.y;
            if (qx * qx + qy * qy < r * r) b.active = false;
        }
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
            if (s.meditateT < 0.0f) Audio::StopLoop(Audio::Cue::SkillChargeLoop);   // 응축 충전 중이면 유지
            Audio::Play(Audio::Cue::SkillSnipe);
            float dx, dy;
            AimDir(dx, dy);
            Bullet b = SkillShot(in.px, in.py, dx, dy, 750.0f, TAG_SNIPE, 3200.0f,
                                 { 1.0f, 0.95f, 0.85f }, 1.4f);
            b.piercing = true;
            b.forceCrit = true;
            b.maxRange = SnipeRange();
            g_Bullets.push_back(b);
        }
    }
    // Condensed Blast: charge while still; moving or 2s sets it off. Used on the move,
    // it first waits for a stop (up to 3s). A charge too short to count is a 0 fire:
    // nothing goes off and the cooldown that paid for it comes back.
    if (s.meditateT >= 0.0f) {
        bool end = false, refund = false;
        if (s.meditateWait >= 0.0f) {
            s.meditateWait += dt;
            if (!in.moving) s.meditateWait = -1.0f;
            else if (s.meditateWait >= 3.0f) end = refund = true;
        } else {
            s.meditateT += dt;
            if (in.moving || s.meditateT >= 2.0f) {
                end = true;
                if (s.meditateT < 0.1f) {
                    refund = true;
                } else {
                    const float k = std::min(1.0f, s.meditateT / 2.0f);
                    passive::DamageRadius(in.px, in.py, Rings(4), 2000.0f * k, 0.70f, 1.0f, 0.85f, DamageMult(), strike_fx::BlastStyle::Stellar);
                    g_GameManager.playerHP = std::min(g_Stats.maxHP, g_GameManager.playerHP + 10.0f * k);
                }
            }
        }
        if (end) {
            if (refund && s.meditateSlot >= 0 && s.meditateSlot < kSlots) s.slots[s.meditateSlot].cd = 0.0f;
            s.meditateT = -1.0f;
            s.meditateWait = -1.0f;
            s.meditateSlot = -1;
            if (s.snipeT < 0.0f) Audio::StopLoop(Audio::Cue::SkillChargeLoop);   // 저격 조준 중이면 유지
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

// ── drawing the field objects (lines only: no fills, no dashes) ──
namespace lf = line_fx;

static float Smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// A Line object, by its style. k = age / life (0..1).
static void DrawLineObj(const Obj& o, float k, float time) {
    if (o.pts.size() < 4) return;
    const float x0 = o.pts[0], y0 = o.pts[1];
    const float x1 = o.pts[o.pts.size() - 2], y1 = o.pts[o.pts.size() - 1];
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
    const float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    const float fade = 1.0f - k;
    switch (o.style) {
    case LineStyle::Bolt: {
        // Chain lightning: each hop lands a moment after the last and fades on its own.
        const float hopDelay = 0.035f;
        for (size_t i = 2; i + 1 < o.pts.size(); i += 2) {
            const int hop = (int)(i / 2) - 1;
            const float age = o.t - hopDelay * (float)hop;
            if (age < 0.0f) break;
            const float a = 1.0f - age / (o.life * 0.6f);
            if (a <= 0.0f) continue;
            const uint32_t seed = o.seed + 977u * (uint32_t)hop + (uint32_t)(age * 24.0f);
            lf::Bolt(o.pts[i - 2], o.pts[i - 1], o.pts[i], o.pts[i + 1], seed, 2.2f, o.col, a);
            const float ik = std::clamp(age / 0.18f, 0.0f, 1.0f);
            lf::Diamond(o.pts[i], o.pts[i + 1], 8.0f + 14.0f * ik, 1.5f, o.col, a * (1.0f - ik));
        }
        break;
    }
    case LineStyle::Execute: {
        // A sniper's line, then a reticle that snaps shut on the target.
        const float a = fade * fade;
        lf::Line(x0, y0, x1, y1, 5.0f, o.col, 0.18f * a);
        lf::Line(x0, y0, x1, y1, 1.5f, lf::Lift(o.col, 0.5f), a);
        const float shut = Smooth01(k / 0.5f);
        const float r = 30.0f - 18.0f * shut;
        lf::Polygon(x1, y1, r, 4, 0.0f, 2.0f, o.col, fade);
        lf::Diamond(x1, y1, r * 0.45f, 1.5f, lf::Lift(o.col, 0.5f), fade);
        break;
    }
    case LineStyle::Gust: {
        // Wind fronts sweeping out across the cone, with the cone's edges.
        const float half = 0.795f;   // acos(0.70): the push cone
        const float ang = std::atan2(uy, ux);
        lf::Line(x0, y0, x0 + std::cos(ang - half) * len, y0 + std::sin(ang - half) * len, 1.0f, o.col, 0.25f * fade);
        lf::Line(x0, y0, x0 + std::cos(ang + half) * len, y0 + std::sin(ang + half) * len, 1.0f, o.col, 0.25f * fade);
        for (int i = 0; i < 3; ++i) {
            const float p = k * 1.25f - 0.16f * (float)i;
            if (p <= 0.05f || p > 1.0f) continue;
            const float r = len * Smooth01(p);
            lf::ArcLine(x0, y0, r, ang - half, half * 2.0f, 2.5f - 0.6f * (float)i, o.col,
                        (1.0f - p) * (1.0f - 0.25f * (float)i));
        }
        break;
    }
    case LineStyle::Warp: {
        // Out of one diamond, into another, a thin thread between.
        lf::Line(x0, y0, x1, y1, 1.0f, o.col, 0.45f * fade);
        const float shrink = 1.0f - Smooth01(k / 0.6f);
        lf::Diamond(x0, y0, 22.0f * shrink, 2.0f, o.col, fade);
        const float grow = Smooth01(k / 0.5f);
        lf::Diamond(x1, y1, 8.0f + 20.0f * grow, 2.0f, o.col, fade);
        lf::Diamond(x1, y1, 4.0f + 8.0f * grow, 1.2f, lf::Lift(o.col, 0.5f), fade);
        break;
    }
    case LineStyle::Rush: {
        // Speed lines left along the path, shortening toward the arrival.
        const float tail = std::max(0.0f, 1.0f - k * 1.4f);
        for (int i = -1; i <= 1; ++i) {
            const float off = 16.0f * (float)i;
            const float from = (i == 0 ? 0.0f : 0.25f) + (1.0f - tail) * 0.75f;
            const float sx = x0 + dx * from + nx * off, sy = y0 + dy * from + ny * off;
            lf::Line(sx, sy, x1 + nx * off * 0.4f, y1 + ny * off * 0.4f, i == 0 ? 2.5f : 1.2f, o.col,
                     (i == 0 ? 0.9f : 0.5f) * fade);
        }
        const float c = 14.0f;
        lf::Line(x1 - ux * c + nx * c, y1 - uy * c + ny * c, x1 + ux * 4.0f, y1 + uy * 4.0f, 2.0f, o.col, fade);
        lf::Line(x1 - ux * c - nx * c, y1 - uy * c - ny * c, x1 + ux * 4.0f, y1 + uy * 4.0f, 2.0f, o.col, fade);
        break;
    }
    case LineStyle::Rewind: {
        // A solid thread back to where you were; a bright stretch runs along it
        // to the return point, one arrowhead at its tip (no repeated marks).
        lf::Line(x0, y0, x1, y1, 1.2f, o.col, 0.5f * fade);
        const float head = len * Smooth01(k / 0.6f);
        const float tail = std::max(0.0f, head - len * 0.35f);
        if (head > tail + 1.0f) {
            lf::Line(x0 + ux * tail, y0 + uy * tail, x0 + ux * head, y0 + uy * head,
                     2.6f, lf::Lift(o.col, 0.45f), fade);
            const float hx = x0 + ux * head, hy = y0 + uy * head, c = 8.0f;
            lf::Line(hx - ux * c + nx * c, hy - uy * c + ny * c, hx, hy, 2.0f, lf::Lift(o.col, 0.45f), fade);
            lf::Line(hx - ux * c - nx * c, hy - uy * c - ny * c, hx, hy, 2.0f, lf::Lift(o.col, 0.45f), fade);
        }
        lf::Circle(x1, y1, 10.0f + 14.0f * Smooth01(k / 0.5f), 1.5f, o.col, fade);
        break;
    }
    case LineStyle::Beam: {
        // A band of light as wide as it hits: a bright core and two edges.
        const float w = 40.0f * (0.55f + 0.45f * fade);
        lf::Line(x0, y0, x1, y1, 3.0f, lf::Lift(o.col, 0.5f), fade);
        lf::Line(x0 + nx * w, y0 + ny * w, x1 + nx * w, y1 + ny * w, 1.2f, o.col, 0.6f * fade);
        lf::Line(x0 - nx * w, y0 - ny * w, x1 - nx * w, y1 - ny * w, 1.2f, o.col, 0.6f * fade);
        lf::Line(x1 + nx * w, y1 + ny * w, x1 - nx * w, y1 - ny * w, 1.2f, o.col, 0.6f * fade);
        break;
    }
    }
    (void)time;
}

// 블랙홀 포획: lines only.
//   seed (0 - kHoleSeed)   a bright dart thrown from the ship onto the aim
//   tear                   space splits open along a short slit, which curls
//                          into the horizon
//   open (to ~0.9s)        horizon rings, then the disc spinning up, then the
//                          arms growing out to the reach, then the field rings
//   steady                 weak field (8 rings) ripples drifting in, pull reach
//                          (5 rings), long arms, tilted disc with arc shards,
//                          one bead per enemy bullet eaten, horizon contours and
//                          a pulsing photon ring, tails on what is being pulled
//   unstable (last 0.6s)   everything wobbles harder, the arms wind faster
//   collapse (last 0.18s)  all of it falls into a point; HoleBurst follows
static void DrawBlackHole(const Obj& o, float /*born*/, float time) {
    constexpr float kPullR = Rings(5), kFieldR = Rings(8);
    const Rgb c = o.col;
    const Rgb hi = lf::Lift(c, 0.5f);
    const Rgb white = { 1.0f, 0.97f, 1.0f };
    const Rgb deep = { c.r * 0.6f, c.g * 0.5f, c.b * 0.85f };
    const float t = o.t;
    auto win = [&](float a, float b) { return Smooth01((t - a) / (b - a)); };

    // ── seed: thrown from the ship ──
    if (t < kHoleSeed + 0.05f) {
        const float u = std::clamp(t / kHoleSeed, 0.0f, 1.0f);
        const float e = u * u;   // speeds up into the point
        const float hx = o.x2 + (o.x - o.x2) * e, hy = o.y2 + (o.y - o.y2) * e;
        const float tu = std::max(0.0f, e - 0.35f);
        const float tx = o.x2 + (o.x - o.x2) * tu, ty = o.y2 + (o.y - o.y2) * tu;
        lf::Line(tx, ty, hx, hy, 1.4f, c, 0.7f);
        lf::Diamond(hx, hy, 6.0f, 1.6f, white, 1.0f);
        lf::Circle(hx, hy, 11.0f, 1.0f, hi, 0.6f);
        if (t < kHoleSeed) return;
    }
    const float ht = t - kHoleSeed;   // time since the hole began

    // ── tear: a slit across the flight line opens, then curls up ──
    const float fdx = o.x - o.x2, fdy = o.y - o.y2;
    const float fd = std::max(1.0f, std::sqrt(fdx * fdx + fdy * fdy));
    const float nx = -fdy / fd, ny = fdx / fd;
    {
        const float grow = Smooth01(ht / 0.14f), curl = Smooth01((ht - 0.10f) / 0.22f);
        if (curl < 1.0f) {
            const float half = 70.0f * grow * (1.0f - curl);
            const float jag = 5.0f * (1.0f - curl);
            float px[7], py[7];
            for (int j = 0; j < 7; ++j) {
                const float v = (float)j / 6.0f * 2.0f - 1.0f;
                const float off = (j % 2 ? jag : -jag) * (1.0f - std::fabs(v));
                px[j] = o.x + nx * half * v + (fdx / fd) * off;
                py[j] = o.y + ny * half * v + (fdy / fd) * off;
            }
            miter::Path(px, py, 7, 2.2f, white.r, white.g, white.b, 0.95f * (1.0f - curl));
        }
    }

    // ── phases ──
    const float horizonIn = win(kHoleSeed + 0.10f, kHoleSeed + 0.34f);
    const float discIn    = win(kHoleSeed + 0.22f, kHoleSeed + 0.55f);
    const float armsIn    = win(kHoleSeed + 0.30f, kHoleSeed + 0.85f);
    const float fieldIn   = win(kHoleSeed + 0.45f, kHoleSeed + 0.95f);
    const float unstable  = Smooth01((t - (o.life - 0.60f)) / 0.42f);
    // The inhale: 0 -> 1 over the suck window, accelerating; then the snap.
    const float suckLin   = std::clamp((t - (o.life - kHoleSuck)) / (kHoleSuck - kHoleSnap), 0.0f, 1.0f);
    const float suck      = suckLin * suckLin;
    const float collapse  = Smooth01((t - (o.life - kHoleSnap)) / kHoleSnap);
    const float shrink = 1.0f - collapse;
    const float a = 1.0f - 0.5f * collapse;
    if (shrink <= 0.01f) {   // the instant before it lets go: one hot point
        lf::Circle(o.x, o.y, 4.0f, 2.0f, white, 1.0f);
        return;
    }
    // The disc spins up as it opens and winds faster as it destabilises.
    const float spin = ht * (1.2f + 1.6f * discIn) + unstable * unstable * 4.0f + suck * 9.0f;
    const float wob = 0.03f + 0.14f * unstable;   // ring wobble (fraction of radius)
    const float ph = time * (3.0f + 6.0f * unstable);

    // ── weak field ──
    if (fieldIn > 0.0f) {
        strike_fx::WarpRing(o.x, o.y, kFieldR * shrink, wob * 0.5f, ph * 0.5f, 1.0f, c, 0.14f * fieldIn * a);
        for (int i = 0; i < 3; ++i) {
            const float p = std::fmod(ht * 0.35f + (float)i / 3.0f, 1.0f);
            const float r = kFieldR - (kFieldR - kPullR) * p;
            lf::Circle(o.x, o.y, r * shrink, 1.0f, c, 0.16f * std::sin(lf::kPi * p) * fieldIn * a);
        }
    }
    // ── pull reach ──
    if (armsIn > 0.0f) {
        strike_fx::WarpRing(o.x, o.y, kPullR * armsIn * shrink, wob * 0.6f, ph * 0.7f, 1.4f, c, 0.34f * armsIn * a);
        for (int i = 0; i < 2; ++i) {
            const float p = std::fmod(ht * 0.8f + 0.5f * (float)i, 1.0f);
            lf::Circle(o.x, o.y, (kPullR * (1.0f - p) + 40.0f * p) * shrink, 1.0f, c, 0.28f * p * fieldIn * a);
        }
        // Long arms growing out from the disc to the reach.
        for (int arm = 0; arm < 4; ++arm) {
            constexpr int kN = 28;
            float px[kN], py[kN];
            const float outer = 40.0f + (kPullR * 0.92f - 40.0f) * armsIn;
            for (int j = 0; j < kN; ++j) {
                const float u = (float)j / (kN - 1);
                const float r = (40.0f + (outer - 40.0f) * u * u) * shrink;
                const float ang = lf::kTwoPi * (float)arm / 4.0f + u * (4.2f + 1.5f * unstable + 5.0f * suck) - spin;
                const float w = 1.0f + 0.35f * wob * std::sin(2.0f * ang + ph);   // a slow bend, not a scribble
                px[j] = o.x + std::cos(ang) * r * w; py[j] = o.y + std::sin(ang) * r * w;
            }
            miter::Path(px, py, kN, arm % 2 ? 1.2f : 1.8f, c.r, c.g, c.b, (arm % 2 ? 0.45f : 0.75f) * armsIn * a);
        }
    }
    // ── accretion disc ──
    const float tilt = 0.38f;
    const float discR = discIn * shrink;
    if (discIn > 0.0f) {
        for (int e = 0; e < 3; ++e) {
            constexpr int kN = 40;
            float px[kN], py[kN];
            const float rx = (58.0f + 26.0f * (float)e) * discR, ry = rx * tilt;
            const float rot = -0.35f + 0.12f * (float)e + unstable * 0.25f * std::sin(time * 7.0f + (float)e);
            const float cr = std::cos(rot), sr = std::sin(rot);
            for (int j = 0; j < kN; ++j) {
                const float th = lf::kTwoPi * (float)j / kN;
                const float w = 1.0f + wob * 0.6f * std::sin(3.0f * th + ph + (float)e);
                const float ex = std::cos(th) * rx * w, ey = std::sin(th) * ry * w;
                px[j] = o.x + ex * cr - ey * sr; py[j] = o.y + ex * sr + ey * cr;
            }
            const Rgb ec = e == 0 ? hi : c;
            miter::Loop(px, py, kN, e == 0 ? 2.2f : 1.3f, ec.r, ec.g, ec.b,
                        (e == 0 ? 0.9f : 0.55f - 0.12f * (float)e) * discIn * a);
        }
        uint32_t seed = lf::SeedAt(o.x, o.y);
        for (int i = 0; i < 10; ++i) {
            const float sp = 0.55f + 0.25f * lf::Rnd(seed);
            const float p = std::fmod(ht * sp + lf::Rnd(seed), 1.0f);
            const float r = (150.0f - 95.0f * p) * discR;
            const float a0 = lf::Rnd(seed) * lf::kTwoPi - spin * (1.4f + p);
            lf::ArcLine(o.x, o.y, r, a0, 0.35f + 0.5f * p, 1.2f, hi, 0.6f * std::sin(lf::kPi * p) * discIn * a);
        }
        const int beads = std::min(12, o.count);
        const float cr = std::cos(-0.35f), sr = std::sin(-0.35f);
        for (int i = 0; i < beads; ++i) {
            const float ang = spin * 1.6f + lf::kTwoPi * (float)i / (float)std::max(1, beads);
            const float ex = std::cos(ang) * 70.0f * discR, ey = std::sin(ang) * 70.0f * tilt * discR;
            lf::Diamond(o.x + ex * cr - ey * sr, o.y + ex * sr + ey * cr, 3.5f, 1.2f, hi, 0.95f * a);
        }
    }
    // ── horizon ──
    if (horizonIn > 0.0f) {
        const float hr = horizonIn * shrink;
        const float eaten = std::min(1.0f, (float)o.count / 12.0f);
        for (int i = 0; i < 4; ++i) {
            const float r = (34.0f - 6.0f * (float)i) * hr;
            strike_fx::WarpRing(o.x, o.y, r, wob * 0.8f, ph + (float)i, 1.0f + 0.3f * (float)i,
                                i < 2 ? deep : c, (0.35f + 0.18f * (float)i) * horizonIn * a);
        }
        // The photon ring flickers as it destabilises.
        const float pulse = 0.5f + 0.5f * std::sin(time * (9.0f + 20.0f * unstable));
        strike_fx::WarpRing(o.x, o.y, (38.0f + 2.5f * pulse) * hr, wob, -ph, 2.4f, collapse > 0.0f ? white : hi,
                            (0.65f + 0.25f * pulse + 0.1f * eaten) * horizonIn * (1.0f - 0.3f * collapse) + 0.3f * collapse);
        lf::Circle(o.x, o.y, 9.0f * hr, 1.2f, hi, (0.4f + 0.6f * eaten) * horizonIn * a);
    }
    // ── the inhale: streaks rushing in from the field's edge, rings snapping inward ──
    if (suckLin > 0.0f && collapse < 1.0f) {
        uint32_t sd = lf::SeedAt(o.y, o.x);
        for (int i = 0; i < 28; ++i) {
            const float ang = lf::Rnd(sd) * lf::kTwoPi;
            const float off = lf::Rnd(sd);
            // Each streak starts at the field's edge and falls in, faster and faster.
            const float p = std::fmod(suckLin * (1.6f + 1.4f * lf::Rnd(sd)) + off, 1.0f);
            const float head = kFieldR * (1.0f - p * p) + 20.0f;
            const float len = 40.0f + 160.0f * p * suckLin;
            const float tail = std::min(kFieldR * 1.05f, head + len);
            const float bend = 0.25f * (1.0f - p);   // curls round as it falls
            lf::Line(o.x + std::cos(ang + bend) * tail, o.y + std::sin(ang + bend) * tail,
                     o.x + std::cos(ang) * head, o.y + std::sin(ang) * head,
                     i % 4 == 0 ? 1.8f : 1.1f, i % 4 == 0 ? white : hi, (0.35f + 0.55f * suckLin) * std::sin(lf::kPi * p));
        }
        for (int i = 0; i < 3; ++i) {
            const float u = std::clamp(suckLin * 1.5f - 0.25f * (float)i, 0.0f, 1.0f);
            if (u <= 0.0f || u >= 1.0f) continue;
            const float r = kFieldR * (1.0f - u * u * u);
            strike_fx::WarpRing(o.x, o.y, r, 0.04f, ph, i == 0 ? 2.4f : 1.4f, i == 0 ? hi : c, 0.75f * u);
        }
    }

    // ── tails on what is being pulled ──
    if (collapse <= 0.0f && armsIn > 0.3f) {
        auto tail = [&](float x, float y, float len) {
            const float dx = o.x - x, dy = o.y - y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < 50.0f || d > kFieldR) return;
            const float inside = d <= kPullR ? 1.0f : 0.45f * (1.0f - (d - kPullR) / (kFieldR - kPullR));
            const float ux = dx / d, uy = dy / d;
            const float bx = x - ux * len - uy * len * 0.35f, by = y - uy * len + ux * len * 0.35f;
            const float mx = x - ux * len * 0.5f - uy * len * 0.08f, my = y - uy * len * 0.5f + ux * len * 0.08f;
            const float qx[3] = { bx, mx, x }, qy[3] = { by, my, y };
            miter::Path(qx, qy, 3, 1.2f, c.r, c.g, c.b, 0.55f * inside * armsIn);
        };
        for (auto* m : g_MonsterManager.monsters)
            if (m->alive && !IsBossMob(m->kind)) tail(m->worldX, m->worldY, 34.0f);
        for (auto* r : g_MonsterManager.rangedMobs) if (r->alive) tail(r->worldX, r->worldY, 34.0f);
        for (const Bullet& b : g_Bullets)
            if (b.active && b.isEnemy) tail(b.x, b.y, 18.0f);
    }
}

void DrawWorld(float time) {
    using namespace hud_lines;
    BindMainShader();
    for (const Obj& o : s.objs) {
        const float fade = o.life > 0.0f && o.life < 1e8f
            ? std::clamp((o.life - o.t) / 0.3f, 0.0f, 1.0f) : 1.0f;
        const float born = Smooth01(o.t / 0.15f);   // pop-in
        const float k = o.life > 0.0f ? std::clamp(o.t / o.life, 0.0f, 1.0f) : 0.0f;
        const Rgb hi = lf::Lift(o.col, 0.5f);
        switch (o.kind) {
        case ObjKind::Wall: {
            // A row of asteroid chunks along the wall, each with an inner contour.
            const float len = std::sqrt(o.x2 * o.x2 + o.y2 * o.y2);
            const float ux = o.x2 / len, uy = o.y2 / len;
            uint32_t seed = lf::SeedAt(o.x, o.y);
            for (int i = 0; i < 5; ++i) {
                const float u = -0.8f + 0.4f * (float)i;
                const float r = (19.0f + 7.0f * lf::Rnd(seed)) * born;
                const float rot = lf::Rnd(seed) * lf::kTwoPi + time * (i % 2 ? 0.25f : -0.25f);
                const uint32_t shape = lf::Hash(seed + (uint32_t)i);
                const float cx = o.x + ux * len * u, cy = o.y + uy * len * u;
                lf::Rock(cx, cy, r, 7, rot, shape, 2.0f, o.col, 0.9f * fade);
                lf::Rock(cx, cy, r * 0.55f, 7, rot + 0.4f, shape, 1.0f, o.col, 0.4f * fade);
            }
            break;
        }
        case ObjKind::Meteor: {
            // Target reticle, a ring closing in, and the rock thrown from the ship onto it.
            lf::Circle(o.x, o.y, Rings(4), 1.5f, o.col, 0.5f);
            lf::Circle(o.x, o.y, Rings(4) * (1.0f - k), 2.0f, o.col, 0.9f);
            for (int i = 0; i < 4; ++i) {
                const float a = lf::kPi * 0.5f * (float)i;
                const float c = std::cos(a), sn = std::sin(a);
                lf::Line(o.x + c * Rings(4), o.y + sn * Rings(4), o.x + c * (Rings(4) - 30.0f), o.y + sn * (Rings(4) - 30.0f), 2.0f, o.col, 0.8f);
            }
            // A high arc: up out of the ship, over, and down onto the reticle.
            const float tdx = o.x - o.x2, tdy = o.y - o.y2;
            const float lift = 120.0f + 0.30f * std::sqrt(tdx * tdx + tdy * tdy);
            auto at = [&](float u, float& ax, float& ay) {
                ax = o.x2 + tdx * u;
                ay = o.y2 + tdy * u - lift * 4.0f * u * (1.0f - u);
            };
            float mx, my;
            at(k, mx, my);
            const float grow = 0.55f + 0.45f * k;   // bigger as it comes down
            const uint32_t seed = lf::SeedAt(o.x, o.y);
            // Trail: the arc just behind the rock.
            float tx[7], ty[7];
            for (int i = 0; i < 7; ++i) at(std::max(0.0f, k - 0.18f * (float)(6 - i) / 6.0f), tx[i], ty[i]);
            for (int i = 0; i < 6; ++i)
                lf::Line(tx[i], ty[i], tx[i + 1], ty[i + 1], 1.0f + 0.25f * (float)i, o.col,
                         0.15f + 0.10f * (float)i);
            lf::Rock(mx, my, 20.0f * grow, 7, time * 2.0f, seed, 2.0f, o.col, 0.95f);
            lf::Rock(mx, my, 11.0f * grow, 7, time * 2.0f + 0.5f, seed, 1.0f, hi, 0.6f);
            break;
        }
        case ObjKind::Dynamite: {
            // The bomb flies in an arc; the blast area counts down round its edge.
            const float fly = std::clamp(o.t / 0.35f, 0.0f, 1.0f);
            const float bx = o.x + (o.x2 - o.x) * fly;
            const float by = o.y + (o.y2 - o.y) * fly - 60.0f * 4.0f * fly * (1.0f - fly);
            lf::Polygon(bx, by, 10.0f, 8, time * 3.0f, 2.0f, o.col, 1.0f);
            lf::Circle(bx, by, 4.0f, 1.2f, hi, 0.5f + 0.5f * (std::sin(time * 30.0f) > 0.0f ? 1.0f : 0.0f));
            lf::Circle(o.x2, o.y2, 200.0f, 1.2f, o.col, 0.35f);
            lf::ArcLine(o.x2, o.y2, 200.0f, -lf::kPi * 0.5f, lf::kTwoPi * (1.0f - k), 2.5f, o.col, 0.85f);
            break;
        }
        case ObjKind::Prism: {
            // A standing crystal; its core fills in with every bullet it eats.
            const float bob = std::sin(time * 3.0f) * 3.0f;
            const float sc = born;
            const float cx = o.x, cy = o.y + bob;
            const float px[6] = { cx, cx + 16 * sc, cx + 16 * sc, cx, cx - 16 * sc, cx - 16 * sc };
            const float py[6] = { cy - 36 * sc, cy - 16 * sc, cy + 18 * sc, cy + 36 * sc, cy + 18 * sc, cy - 16 * sc };
            miter::Loop(px, py, 6, 2.0f, o.col.r, o.col.g, o.col.b, 0.95f * fade);
            lf::Line(cx, cy - 36 * sc, cx, cy + 36 * sc, 1.0f, o.col, 0.4f * fade);
            lf::Line(cx - 16 * sc, cy - 16 * sc, cx + 16 * sc, cy - 16 * sc, 1.0f, o.col, 0.4f * fade);
            lf::Line(cx - 16 * sc, cy + 18 * sc, cx + 16 * sc, cy + 18 * sc, 1.0f, o.col, 0.4f * fade);
            const float charge = std::min(1.0f, (float)o.count / 10.0f);
            if (charge > 0.0f) {
                const float qx[4] = { cx, cx + 8 * sc, cx, cx - 8 * sc };
                const float qy[4] = { cy - 18 * sc, cy, cy + 18 * sc, cy };
                miter::Loop(qx, qy, 4, 1.5f, hi.r, hi.g, hi.b, (0.3f + 0.7f * charge) * fade);
            }
            lf::Circle(o.x, o.y, 70.0f, 1.0f, o.col, 0.3f * fade);   // where bullets are caught
            break;
        }
        case ObjKind::Gate: {
            DrawBlackHole(o, born, time);
            break;
        }
        case ObjKind::Trap: {
            // A small mine: a diamond in a diamond, the core blinking, the
            // trigger radius as a faint circle.
            const bool on = std::fmod(o.t, 0.8f) < 0.4f;
            lf::Diamond(o.x, o.y, 11.0f * born, 2.0f, o.col, 1.0f);
            lf::Diamond(o.x, o.y, 5.0f * born, 1.2f, hi, on ? 1.0f : 0.3f);
            lf::Circle(o.x, o.y, 40.0f, 1.0f, o.col, 0.25f);
            break;
        }
        case ObjKind::Turret: {
            // Hexagonal base, a barrel that turns to its target and kicks back
            // on each shot, the time left as an arc round it.
            float ax = o.dirX, ay = o.dirY;
            if (ax == 0.0f && ay == 0.0f) { ax = 0.0f; ay = -1.0f; }
            const float kick = std::max(0.0f, 1.0f - o.timer / 0.12f) * 5.0f;
            const float sc = born;
            lf::Polygon(o.x, o.y, 16.0f * sc, 6, 0.0f, 2.0f, o.col, 0.95f * fade);
            lf::Circle(o.x, o.y, 7.0f * sc, 1.5f, hi, 0.9f * fade);
            const float bnx = -ay, bny = ax;
            const float b0 = 6.0f - kick, b1 = 24.0f - kick;
            for (int sd = -1; sd <= 1; sd += 2) {
                const float ox = bnx * 2.8f * (float)sd, oy = bny * 2.8f * (float)sd;
                lf::Line(o.x + ax * b0 + ox, o.y + ay * b0 + oy, o.x + ax * b1 + ox, o.y + ay * b1 + oy,
                         1.5f, o.col, 0.95f * fade);
            }
            lf::Line(o.x + ax * b1 + bnx * 4.0f, o.y + ay * b1 + bny * 4.0f,
                     o.x + ax * b1 - bnx * 4.0f, o.y + ay * b1 - bny * 4.0f, 1.5f, o.col, 0.95f * fade);
            lf::ArcLine(o.x, o.y, 22.0f, -lf::kPi * 0.5f, lf::kTwoPi * (1.0f - k), 1.2f, o.col, 0.45f * fade);
            break;
        }
        case ObjKind::Clone: {
            // A twin star: the ship's four-point outline, slowly turning, tied
            // to the ship by a thin line.
            const float sc = born;
            lf::Line(o.x, o.y, s.in.px, s.in.py, 1.0f, o.col, 0.15f * fade);
            lf::Star4(o.x, o.y, 16.0f * sc, 5.0f * sc, time * 0.8f, 2.0f, o.col, 0.95f * fade);
            lf::Star4(o.x, o.y, 8.0f * sc, 2.5f * sc, time * 0.8f, 1.0f, hi, 0.6f * fade);
            lf::Circle(o.x, o.y, (24.0f + 2.5f * std::sin(time * 4.0f)) * sc, 1.0f, o.col, 0.35f * fade);
            break;
        }
        case ObjKind::Hoop: {
            // A spinning ring: a thin circle with three bright segments running round it.
            lf::Circle(o.x2, o.y2, 22.0f, 1.2f, o.col, 0.6f);
            lf::Circle(o.x2, o.y2, 13.0f, 1.0f, o.col, 0.35f);
            for (int i = 0; i < 3; ++i)
                lf::ArcLine(o.x2, o.y2, 22.0f, time * 9.0f + lf::kTwoPi * (float)i / 3.0f, 0.7f, 3.0f, hi, 1.0f);
            break;
        }
        case ObjKind::Line:
            DrawLineObj(o, k, time);
            break;
        case ObjKind::Strike: {
            // A forked bolt from the sky (or the storm cloud), then a diamond
            // spreading from where it lands.
            const float sx = o.fromSource ? o.x2 : o.x - 60.0f + 120.0f * (float)(o.seed % 101u) / 100.0f;
            const float sy = o.fromSource ? o.y2 : o.y - 340.0f;
            const float a = (1.0f - k) * (1.0f - k);
            lf::Bolt(sx, sy, o.x, o.y, o.seed + (uint32_t)(o.t * 24.0f), 2.4f, o.col, a);
            const float ik = Smooth01(k / 0.6f);
            lf::Diamond(o.x, o.y, 6.0f + 22.0f * ik, 2.0f, o.col, 1.0f - ik);
            lf::Diamond(o.x, o.y, 4.0f + 8.0f * ik, 1.2f, hi, 1.0f - ik);
            break;
        }
        }
    }
    // Precision Snipe aim line and Condensed Blast charge.
    if (s.snipeT >= 0.0f) {
        float dx, dy;
        AimDir(dx, dy);
        const float reach = SnipeRange();   // as far as the shot goes
        Segment(s.in.px, s.in.py, s.in.px + dx * reach, s.in.py + dy * reach, 1.0f,
                { 1.0f, 0.95f, 0.85f }, 0.6f);
    }
    if (s.meditateT >= 0.0f) {
        // The blast always reaches 4 rings (only its damage grows with the charge),
        // so the ring sits on its edge from the start; the charge fills it as a
        // bright arc running clockwise from the top.
        constexpr float kReach = Rings(4);
        const float k = std::min(1.0f, s.meditateT / 2.0f);
        const Rgb c{ 0.70f, 1.0f, 0.85f };
        Ring(s.in.px, s.in.py, kReach, 1.2f, c, 0.35f, 0.0f);
        const float top = -lf::kPi * 0.5f;
        lf::ArcLine(s.in.px, s.in.py, kReach, top, lf::kTwoPi * k, 5.0f, { 0.04f, 0.08f, 0.06f }, 0.30f);
        lf::ArcLine(s.in.px, s.in.py, kReach, top, lf::kTwoPi * k, 3.0f, lf::Lift(c, 0.3f), 0.95f);
    }
    BatchFlush();
}

}  // namespace active
