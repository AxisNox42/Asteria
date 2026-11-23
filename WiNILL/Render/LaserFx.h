#pragma once
// 레이저 (F_LASER) and its upgrades: a real beam, not a projectile.
//
//   tier 1  레이저        hold fire: charge kCharge s, then a beam stands from the ship along
//                         the aim and stays on while fire is held (range 9.5○).
//   tier 2  + 집속 레이저 the beam reaches the walls, charges in kChargeFocus s, and heats up
//                         while it stays on: thicker and whiter, up to +60% damage at kHeatTime.
//   tier 3  + 과부하 레이저 letting go of a beam held over kReleaseMin s releases the heat as one
//                         thick beam (kReleaseMult x the laser's damage at full heat).
//                         Enemies the beam touches catch a stacking burn and leave embers
//                         where they go (SynergyRuntime.cpp, StepLaserBurns).
//
// Damage: every gun tick (the fire interval) the shot volley is still made by the
// cards (Shape), but each laser shot is turned here into "hitscan" segments
// (synf::Hitscan): still bullets for the collision code, so every card that
// reads a hit keeps working, but they do not move. Each one lies along a leg of
// the beam's path (the arena edge stops it, or reflects it with bounce cards),
// hits every enemy on it once and is gone after a few steps (one hit per step).
//
// A laser shot that does not leave the ship with the main beam (a clone's copy,
// a split child) becomes a short flash beam of its own instead.
// EntityDraw.cpp (DrawPlayerLasers) draws all of it.
#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

#include "../Entity/Bullet.h"
#include "../Entity/SpaceRules.h"

namespace laser_fx {

inline constexpr float kCharge = 0.40f;        // hold this long before the beam opens
inline constexpr float kChargeFocus = 0.25f;   // with 집속 레이저
inline constexpr float kRange = 760.0f;        // 9.5○
inline constexpr float kFreeRange = 1600.0f;   // with return / hunter (the card lifts the range limit)
inline constexpr float kWallRange = 8000.0f;   // 집속 레이저: up to the arena edge
inline constexpr float kHeatTime = 2.5f;       // 집속 레이저: full heat after this long on
inline constexpr float kHeatDamage = 0.60f;    // +60% damage at full heat
inline constexpr float kHeatWidth = 0.50f;     // +50% width at full heat
inline constexpr float kReleaseMin = 0.60f;    // 과부하 레이저: held at least this long to release
inline constexpr float kReleaseMult = 10.0f;   // the release at full heat = this many laser hits
inline constexpr float kReleaseLife = 0.55f;   // how long the released beam is seen
inline constexpr int   kScanSteps = 9;         // fixed steps a segment keeps hitting (one enemy per step)
inline constexpr int   kReleaseSteps = 24;     // the release's segments (Bullet::kHitMemory)
inline constexpr float kHold = 0.05f;          // a flash beam stays this long ...
inline constexpr float kFade = 0.16f;          // ... then thins out over this long (the main beam too)
inline constexpr int   kMaxFlashes = 64;
inline constexpr float kTurnRate = 1.6f;       // rad/s: the standing beam turns toward the aim at this fixed rate
// The beam (and its hit band) grows with each laser card: 레이저 / + 집속 / + 과부하.
// Big steps on purpose: each tier must read as a clearly heavier beam at a glance.
inline constexpr float kTierWidth[4] = { 1.0f, 1.0f, 1.9f, 2.8f };
// Laser shots skip the shot size cap (the tiers outgrow it) and use this one instead.
inline constexpr float kMaxLaserScale = 7.0f;   // half-width 35 px

struct Rect { float minX = 0, maxX = 1920, minY = 0, maxY = 1080; };

// The beam's path from (x, y) along (dx, dy): stops at the arena edge, or turns back
// there while bounces are left. pts gets the corners, the start first.
// OCTANS panes (SpaceRules.h) stop the beam like any shot: it ends where its path
// first leaves the space it started in. Returns the pane that stopped it (0 = none).
inline int ClipToSpace(std::vector<glm::vec2>& pts) {
    if (!space::Active() || pts.size() < 2) return 0;
    const int from = space::SpaceAt(pts[0].x, pts[0].y);
    constexpr float kProbe = 4.0f;   // px between checks along the beam
    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        const glm::vec2 p0 = pts[k], p1 = pts[k + 1];
        const float len = glm::length(p1 - p0);
        const int n = std::max(1, (int)std::ceil(len / kProbe));
        for (int i = 1; i <= n; ++i) {
            const glm::vec2 q = p0 + (p1 - p0) * ((float)i / (float)n);
            const int now = space::SpaceAt(q.x, q.y);
            if (now == from) continue;
            // Narrow down to the border between the last point inside and this one.
            float lo = (float)(i - 1) / (float)n, hi = (float)i / (float)n;
            for (int it = 0; it < 10; ++it) {
                const float mid = 0.5f * (lo + hi);
                const glm::vec2 m = p0 + (p1 - p0) * mid;
                if (space::SpaceAt(m.x, m.y) == from) lo = mid; else hi = mid;
            }
            pts.resize(k + 1);
            pts.push_back(p0 + (p1 - p0) * lo);
            return space::AbsorbingPane(from, now);
        }
    }
    return 0;
}

inline void Path(float x, float y, float dx, float dy, float range, int bounces, const Rect& a,
                 std::vector<glm::vec2>& pts, int* stoppedBy = nullptr) {
    pts.clear();
    x = std::clamp(x, a.minX, a.maxX); y = std::clamp(y, a.minY, a.maxY);
    pts.emplace_back(x, y);
    float left = range;
    for (int guard = 0; guard < 8 && left > 0.5f; ++guard) {
        float t = left;
        int wall = 0;   // 1 = x wall, 2 = y wall
        if (dx > 1e-5f) { const float tw = (a.maxX - x) / dx; if (tw < t) { t = tw; wall = 1; } }
        else if (dx < -1e-5f) { const float tw = (a.minX - x) / dx; if (tw < t) { t = tw; wall = 1; } }
        if (dy > 1e-5f) { const float tw = (a.maxY - y) / dy; if (tw < t) { t = tw; wall = 2; } }
        else if (dy < -1e-5f) { const float tw = (a.minY - y) / dy; if (tw < t) { t = tw; wall = 2; } }
        t = std::max(0.0f, t);
        x += dx * t; y += dy * t; left -= t;
        pts.emplace_back(x, y);
        if (wall == 0 || bounces <= 0) break;
        --bounces;
        if (wall == 1) dx = -dx; else dy = -dy;
    }
    const int pane = ClipToSpace(pts);
    if (stoppedBy) *stoppedBy = pane;
}

// A beam a pane stopped wears that pane, like a shot it absorbs.
inline void WearPane(int pane, const std::vector<glm::vec2>& pts) {
    if (pane > 0 && !pts.empty()) space::g_Absorbed.push_back({ pane, pts.back().x, pts.back().y, false });
}

// The main beam (the one held from the ship).
struct Main {
    int   tier = 1;
    float charge = 0.0f;        // 0..ChargeTime() while fire is held
    bool  on = false;           // the beam stands
    float onT = 0.0f;           // since it opened
    float offT = 99.0f;         // since it was let go (the fade)
    float px = 0, py = 0, aim = 0;   // ship and aim (radians), followed every frame
    // The last tick's volley, as angles off the aim, and what the shots carried.
    std::vector<float> offsets;
    std::vector<float> sides;   // each shot's sideways offset (쌍발 사격탄: two beams side by side)
    int   bounces = 0;
    float range = kRange;
    float baseHalfW = 6.5f;     // the band before heat
    glm::vec3 color{ 0.55f, 0.95f, 1.0f };
    // Where the tick that is being fired left from.
    int   tick = 0, offsetsTick = -1;
    float tickX = 0, tickY = 0, tickAim = 0;
    // 과부하 레이저: a main shot (before heat) to make the release from, and a release due.
    Bullet lastShot{ 0, 0, 1, 0 };
    bool  hasShot = false;
    float releaseHeat = 0.0f;
    // Sound events since main.cpp last read them (TakeEvents).
    bool  evCharge = false, evOpen = false;
    int   beams = 0;            // beams opened so far: one beam counts as one shot (SynergyRuntime Shape)
};

// A short beam of a laser shot that did not leave with the main beam,
// or the released beam (big = true).
struct Flash {
    std::vector<glm::vec2> pts;
    float halfW = 6.5f, age = 0.0f, alpha = 1.0f, heat = 0.0f;
    bool  big = false;
    glm::vec3 color{ 0.55f, 0.95f, 1.0f };
};

inline Main g_Main;
inline std::vector<Flash> g_Flashes;
inline Rect g_Arena;

inline void Clear() { g_Main = Main{}; g_Flashes.clear(); }

inline float ChargeTime() { return g_Main.tier >= 2 ? kChargeFocus : kCharge; }
// 집속 레이저: 0..1 heat of the standing beam.
inline float Heat() {
    const Main& m = g_Main;
    if (m.tier < 2 || !m.on) return 0.0f;
    return std::min(1.0f, m.onT / kHeatTime);
}
inline float HalfWidth() { return std::min(5.0f * kMaxLaserScale, g_Main.baseHalfW * (1.0f + kHeatWidth * Heat())); }

// Width of the main beam as a share of its band: 1 while on, easing to 0 once let go.
inline float MainWidth() {
    if (g_Main.on) return 1.0f;
    const float u = std::clamp(g_Main.offT / kFade, 0.0f, 1.0f);
    return 1.0f - u * u * (3.0f - 2.0f * u);
}
inline float FlashWidth(const Flash& f) {
    if (f.big) {   // the release: full for a moment, then thins out
        const float u = std::clamp((f.age - 0.12f) / (kReleaseLife - 0.12f), 0.0f, 1.0f);
        return 1.0f - u * u * (3.0f - 2.0f * u);
    }
    if (f.age < kHold) return 1.0f;
    const float u = std::clamp((f.age - kHold) / kFade, 0.0f, 1.0f);
    return 1.0f - u * u * (3.0f - 2.0f * u);
}

// Every frame while the laser frame is owned. `held`: fire is held with a target.
// True while the beam stands (the gun may tick).
inline bool Hold(bool held, float px, float py, float tx, float ty, float dt, int tier) {
    Main& m = g_Main;
    m.tier = std::max(1, tier);
    m.px = px; m.py = py;
    if (held) {
        if (std::hypot(tx - px, ty - py) > 1.0f) {
            const float want = std::atan2(ty - py, tx - px);
            if (!m.on) m.aim = want;   // charging: the beam opens toward the aim
            else {                     // standing: turns toward it at kTurnRate, the short way
                float d = want - m.aim;
                while (d > 3.14159265f) d -= 6.2831853f;
                while (d < -3.14159265f) d += 6.2831853f;
                const float step = kTurnRate * dt;
                m.aim += std::clamp(d, -step, step);
                if (m.aim > 3.14159265f) m.aim -= 6.2831853f;
                else if (m.aim < -3.14159265f) m.aim += 6.2831853f;
            }
        }
        if (!m.on && m.charge <= 0.0f) m.evCharge = true;   // a fresh press starts the charge
        m.charge = std::min(ChargeTime(), m.charge + dt);
        const bool was = m.on;
        m.on = m.charge >= ChargeTime();
        if (m.on && !was) { m.evOpen = true; ++m.beams; }
        if (m.on) { m.onT = was ? m.onT + dt : 0.0f; m.offT = 0.0f; }
    } else {
        m.charge = std::max(0.0f, m.charge - 2.0f * dt);   // a quick re-press charges faster
        if (m.on) {
            m.on = false; m.offT = 0.0f;
            // 과부하 레이저: the heat the beam built up goes out as one beam.
            if (m.tier >= 3 && m.hasShot && m.onT >= kReleaseMin)
                m.releaseHeat = std::min(1.0f, m.onT / kHeatTime);
        }
        m.offT += dt;
    }
    return m.on;
}

// Right before the gun fires a tick of the main beam.
inline void BeginTick() {
    Main& m = g_Main;
    ++m.tick;
    m.tickX = m.px; m.tickY = m.py; m.tickAim = m.aim;
}

// The shots that become a beam: an orbiting copy stays a normal shot.
inline bool IsLaserShot(const Bullet& b) {
    return !b.isEnemy && !b.skillShot && (b.synFlags & synf::FrameLaser) &&
           !(b.synFlags & (synf::Orbit | synf::Mine | synf::Lob));
}
inline bool IsScan(const Bullet& b) { return (b.synFlags & synf::Hitscan) != 0; }

// The still segments of a beam along pts, made from `proto`.
inline void LaySegments(const Bullet& proto, const std::vector<glm::vec2>& pts, int steps,
                        std::vector<Bullet>& out) {
    Bullet base = proto;
    base.synFlags |= synf::Hitscan;
    base.synFlags &= ~(synf::Return | synf::Returning | synf::Hover | synf::Hovering | synf::Grow);
    base.homing = false; base.bouncesLeft = 0; base.synRicochet = 0;
    base.maxRange = 0.0f; base.speed = 0.0f; base.launchAccel = 0.0f; base.launchRamp = 1.0f;
    base.hitscanLeft = steps;
    base.active = true;
    base.ClearHits();
    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        const glm::vec2 p0 = pts[k], p1 = pts[k + 1];
        const float len = glm::length(p1 - p0);
        if (len < 1.0f) continue;
        Bullet sb = base;
        sb.prevX = p0.x; sb.prevY = p0.y; sb.x = p1.x; sb.y = p1.y;
        sb.dirX = (p1.x - p0.x) / len; sb.dirY = (p1.y - p0.y) / len;
        sb.synBounces = (int)k;
        out.push_back(sb);
    }
}

// Once per fixed step, before the projectile step: new laser shots become hitscan
// segments along their beam; flash beams age.
inline void Step(std::vector<Bullet>& bullets, float dt, const Rect& arena) {
    g_Arena = arena;
    for (Flash& f : g_Flashes) f.age += dt;
    g_Flashes.erase(std::remove_if(g_Flashes.begin(), g_Flashes.end(),
        [](const Flash& f) { return f.age >= (f.big ? kReleaseLife : kHold + kFade); }), g_Flashes.end());

    static std::vector<Bullet> add;
    static std::vector<glm::vec2> pts;
    add.clear();
    Main& m = g_Main;
    for (Bullet& b : bullets) {
        if (!b.active || IsScan(b) || !IsLaserShot(b)) continue;
        b.sizeScale = std::min(kMaxLaserScale, b.sizeScale * kTierWidth[std::clamp(m.tier, 0, 3)]);
        const bool free = (b.synFlags & synf::Return) != 0;
        const float range = m.tier >= 2 ? kWallRange : (free ? kFreeRange : kRange);
        const int bounces = std::max(0, b.bouncesLeft);
        const float halfW = std::max(2.5f, 5.0f * b.sizeScale);   // kPlayerShotHitPx per sizeScale
        // An echo (겹겹 메아리) leaves the ship later: its own flash beam, not merged into the main one.
        const bool main = std::hypot(b.x - m.tickX, b.y - m.tickY) < 12.0f && m.tick > 0 &&
                          !(b.synFlags & synf::Echo);
        if (main) {
            if (m.offsetsTick != m.tick) { m.offsetsTick = m.tick; m.offsets.clear(); m.sides.clear(); }
            float off = std::atan2(b.dirY, b.dirX) - m.tickAim;
            while (off > 3.14159265f) off -= 6.2831853f;
            while (off < -3.14159265f) off += 6.2831853f;
            // Sideways offset off the tick's aim; a side-by-side pair stands far enough apart to read as two.
            float side = -(b.x - m.tickX) * std::sin(m.tickAim) + (b.y - m.tickY) * std::cos(m.tickAim);
            if (std::fabs(side) > 1.0f) side = std::copysign(std::max(std::fabs(side), halfW * 1.6f), side);
            if (m.offsets.size() < 16) { m.offsets.push_back(off); m.sides.push_back(side); }
            m.bounces = bounces; m.range = range; m.baseHalfW = halfW; m.color = b.color;
            // The beam follows the aim between ticks, so the damage lies where it is drawn:
            // along the current aim from the current ship position.
            const float a = m.aim + off;
            b.dirX = std::cos(a); b.dirY = std::sin(a);
            b.x = m.px - b.dirY * side; b.y = m.py + b.dirX * side;
            if (m.offsets.size() == 1) { m.lastShot = b; m.hasShot = true; }   // the release is made from it (before heat)
            // 집속 레이저: the hot beam is wider and hits harder.
            const float heat = Heat();
            b.dmgMult *= 1.0f + kHeatDamage * heat;
            b.sizeScale = std::min(kMaxLaserScale, b.sizeScale * (1.0f + kHeatWidth * heat));
        }
        int pane = 0;
        Path(b.x, b.y, b.dirX, b.dirY, range, bounces, arena, pts, &pane);
        WearPane(pane, pts);
        if (!main && (int)g_Flashes.size() < kMaxFlashes) {
            Flash f; f.pts = pts; f.halfW = halfW; f.color = b.color;
            f.alpha = (b.synFlags & synf::Echo) ? 0.5f : 1.0f;
            g_Flashes.push_back(std::move(f));
        }
        LaySegments(b, pts, kScanSteps, add);
        b.active = false;
    }
    bullets.insert(bullets.end(), add.begin(), add.end());
}

// 과부하 레이저: when a release is due, lays the thick released beam along the aim.
// Returns its heat (0 = nothing released this call) so the caller can shake the view.
inline float FireRelease(std::vector<Bullet>& bullets) {
    Main& m = g_Main;
    const float heat = m.releaseHeat;
    if (heat <= 0.0f || !m.hasShot) { m.releaseHeat = 0.0f; return 0.0f; }
    m.releaseHeat = 0.0f;
    static std::vector<glm::vec2> pts;
    int pane = 0;
    Path(m.px, m.py, std::cos(m.aim), std::sin(m.aim), kWallRange, m.bounces, g_Arena, pts, &pane);
    WearPane(pane, pts);
    const float halfW = m.baseHalfW * (1.4f + 1.6f * heat);   // full heat ~ the Quasar beam (55 px)
    Bullet proto = m.lastShot;
    // The last tick's one-shot bonuses (정지 반응, 박자 사격 ...) are not multiplied again.
    proto.dmgMult *= kReleaseMult * heat / std::max(1e-3f, proto.synBoost);
    proto.synBoost = 1.0f;
    proto.sizeScale = halfW / 5.0f;                 // the hit band is the drawn band
    proto.synFlags |= synf::LaserBlast;             // exempt from the shot size cap
    proto.synFlags &= ~synf::Echo;
    static std::vector<Bullet> add;
    add.clear();
    LaySegments(proto, pts, kReleaseSteps, add);
    bullets.insert(bullets.end(), add.begin(), add.end());
    Flash f; f.pts = pts; f.halfW = halfW; f.color = m.color; f.big = true; f.heat = heat;
    g_Flashes.push_back(std::move(f));
    return heat;
}

}  // namespace laser_fx
