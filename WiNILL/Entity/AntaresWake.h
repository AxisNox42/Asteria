#pragma once
// ─────────────────────────────────────────────────────────────
// Nightmare ANTARES (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md §10.12).
// Friction: while the elite body is fast (a chase run at 500 px/s and up,
// every gather burst and lance dash) friction sparks spray from its nose out to
// both sides and back. The spray is the hit area, sized by the body radius R: a
// wedge from R + kTipExtra in front of its centre to R * kTailShare behind it,
// R + kWidthExtra to each side there (elite body R = 62: 80 / 40 / 132 px).
// kDamage once per fast stretch, no knockback; a ram is separate.
// Clash: two fragments meeting hard leave a ring that bursts kDelay seconds
// later where they met (at most kClashCap per family alive).
// Scatter: an Antares that hits a wall or another Antares at kMinSpeed or
// more throws shots in a kScatterArc fan along the way it bounces off:
// kScatterFast fast ones evenly spread and kScatterSlow slow ones at random.
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace antares_wake {

// ── Friction ──
inline constexpr float kMinSpeed = 500.0f;    // = Monster::ANTARES_PASS_SPEED
inline constexpr float kTipExtra = 18.0f;     // wedge tip, past the body's front
inline constexpr float kTailShare = 0.65f;    // wedge back edge, share of R behind the centre
inline constexpr float kWidthExtra = 70.0f;   // to each side at the back edge, past the body
inline constexpr float kDamage = 8.0f;
inline constexpr float kRise = 0.15f;         // spray opens over this once fast
inline constexpr float kFall = 0.20f;         // and closes over this once slow
// Sparks per second at full spray: out to the sides from the nose, and back.
inline constexpr float kSideRate = 190.0f;
inline constexpr float kBackRate = 70.0f;

inline float TipAhead(float bodyR) { return bodyR + kTipExtra; }
inline float TailBehind(float bodyR) { return bodyR * kTailShare; }
inline float Width(float bodyR) { return bodyR + kWidthExtra; }

// Is a player (radius pr) at (along, side) from the centre - along the
// heading and across it - inside the wedge of a body of radius bodyR opened
// `amount` (0-1)?
inline bool InFriction(float along, float side, float amount, float pr, float bodyR) {
    if (amount <= 0.0f) return false;
    const float tip = TipAhead(bodyR), tail = TailBehind(bodyR);
    if (along > tip + pr || along < -tail - pr) return false;
    const float a = std::clamp(along, -tail, tip);
    const float half = Width(bodyR) * amount * (tip - a) / (tip + tail);
    return std::fabs(side) <= half + pr;
}

// ── Scatter shots (§12) ──
inline constexpr float kScatterArc = 2.0943951f;      // 120 deg fan
inline constexpr int kScatterFast = 7;
inline constexpr int kScatterSlow = 12;
inline constexpr float kScatterFastSpeed = 520.0f;
inline constexpr float kScatterSlowMin = 180.0f, kScatterSlowMax = 260.0f;
inline constexpr float kScatterDamage = 6.0f;
inline constexpr float kScatterCooldown = 0.3f;       // per Antares
struct Scatter { float x, y, dirX, dirY; };

// ── Clash rings ──
inline constexpr float kDelay = 0.4f;         // clash -> burst
inline constexpr float kSpread = 0.2f;        // ring spreads 0 -> radius
inline constexpr float kFade = 0.15f;
inline constexpr float kLife = kDelay + kSpread + kFade;
inline constexpr float kClashRadius = 110.0f; // ring radius for a generation-0 size
inline constexpr float kClashSpeed = 300.0f;  // closing speed that counts as a clash
inline constexpr int kClashCap = 2;           // clash rings alive per family
inline constexpr float kGenScale = 0.8f;      // = Monster::ANTARES_CHILD_SCALE
inline constexpr size_t kMaxRings = 32;

struct Ring { float x, y, radius, age; int family; bool hit; };

struct State {
    std::vector<Ring> rings;
    std::vector<Scatter> scatters;   // queued fans, turned into shots by MonsterManager
};
inline State g;

inline void Reset() { g = State{}; }
inline void QueueScatter(float x, float y, float dirX, float dirY) {
    if (g.scatters.size() < 16) g.scatters.push_back({ x, y, dirX, dirY });
}
// The shots of one fan: angle offset from the fan's centre and speed.
struct ScatterShot { float angle, speed; bool fast; };
inline std::vector<ScatterShot> ScatterShots() {
    std::vector<ScatterShot> out;
    auto rnd = [] { return (float)(std::rand() % 1001) / 1000.0f; };
    for (int i = 0; i < kScatterFast; ++i) {
        const float u = (float)i / (float)(kScatterFast - 1) - 0.5f;   // -0.5 .. 0.5
        out.push_back({ u * kScatterArc + (rnd() - 0.5f) * 0.06f, kScatterFastSpeed, true });
    }
    for (int i = 0; i < kScatterSlow; ++i)
        out.push_back({ (rnd() - 0.5f) * kScatterArc,
                        kScatterSlowMin + (kScatterSlowMax - kScatterSlowMin) * rnd(), false });
    return out;
}

inline int ClashRingsAlive(int family) {
    int n = 0;
    for (const Ring& r : g.rings) n += r.family == family ? 1 : 0;
    return n;
}
// A clash ring where two fragments met, sized by the bigger one (lower generation).
inline bool AddClash(float x, float y, int family, int minGen) {
    if (ClashRingsAlive(family) >= kClashCap || g.rings.size() >= kMaxRings) return false;
    g.rings.push_back({ x, y, kClashRadius * std::pow(kGenScale, (float)minGen), 0.0f, family, false });
    return true;
}

// 0 before the burst, 0..1 while it spreads, 1 while it fades.
inline float SpreadOf(float age) { return std::clamp((age - kDelay) / kSpread, 0.0f, 1.0f); }
inline bool Spreading(float age) { return age >= kDelay && age < kDelay + kSpread; }

struct TickOut {
    float clashDamage = 0.0f;
    bool clashBurst = false;    // a clash ring just burst
};

// Ages the rings, hits the player (radius pr) once per ring, drops the dead.
inline TickOut Tick(float dt, float px, float py, float pr) {
    TickOut out;
    for (Ring& r : g.rings) {
        const float before = r.age;
        r.age += dt;
        if (before < kDelay && r.age >= kDelay) out.clashBurst = true;
        if (!Spreading(r.age) || r.hit) continue;
        const float front = r.radius * SpreadOf(r.age);
        const float dx = px - r.x, dy = py - r.y;
        if (dx * dx + dy * dy <= (front + pr) * (front + pr)) {
            r.hit = true;
            out.clashDamage += kDamage;
        }
    }
    g.rings.erase(std::remove_if(g.rings.begin(), g.rings.end(),
                                 [](const Ring& r) { return r.age >= kLife; }), g.rings.end());
    return out;
}

}  // namespace antares_wake
