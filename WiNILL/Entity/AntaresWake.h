#pragma once
// ─────────────────────────────────────────────────────────────
// Nightmare ANTARES: the wake (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md §6).
// Where the elite body travels fast (a chase run at 500 px/s and up, every
// gather burst and lance dash) it leaves a line on the floor, kNodeSpacing px a
// piece. Each piece bursts kDelay seconds after the body passed it: two lines
// spread kWidth px out to both sides, then fade. A wall bounce on the way, and
// two fragments clashing hard, leave a ring that bursts the same way.
// One hit per run (and per clash ring); no knockback.
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <vector>

namespace antares_wake {

inline constexpr float kNodeSpacing = 40.0f;
inline constexpr float kMinSpeed = 500.0f;    // = Monster::ANTARES_PASS_SPEED
inline constexpr float kDelay = 0.4f;         // body passes -> burst
inline constexpr float kSpread = 0.2f;        // lines spread 0 -> kWidth
inline constexpr float kFade = 0.15f;
inline constexpr float kLife = kDelay + kSpread + kFade;
inline constexpr float kWidth = 110.0f;       // to each side / ring radius
inline constexpr float kDamage = 8.0f;
// Fragments clash: a ring only when they closed in at this speed or more.
inline constexpr float kClashSpeed = 300.0f;
inline constexpr int kClashCap = 2;           // clash rings alive per family
inline constexpr float kGenScale = 0.8f;      // = Monster::ANTARES_CHILD_SCALE
inline constexpr size_t kMaxNodes = 256;
inline constexpr size_t kMaxRings = 32;

struct Node { float ax, ay, bx, by, age; int run; };
struct Ring { float x, y, radius, age; int run, family; bool clash; };

struct State {
    std::vector<Node> nodes;
    std::vector<Ring> rings;
    int hitRuns[8] = {};        // runs that already hit the player (ring buffer)
    int hitHead = 0;
    int burstRuns[8] = {};      // runs whose first burst was already reported
    int burstHead = 0;
    int nextRun = 0;
};
inline State g;

inline void Reset() { g = State{}; }
inline int NewRun() { return ++g.nextRun; }

inline void AddNode(float ax, float ay, float bx, float by, int run) {
    if (g.nodes.size() < kMaxNodes) g.nodes.push_back({ ax, ay, bx, by, 0.0f, run });
}
inline void AddRing(float x, float y, float radius, int run, int family = 0, bool clash = false) {
    if (g.rings.size() < kMaxRings) g.rings.push_back({ x, y, radius, 0.0f, run, family, clash });
}
inline int ClashRingsAlive(int family) {
    int n = 0;
    for (const Ring& r : g.rings) n += (r.clash && r.family == family) ? 1 : 0;
    return n;
}
// A clash ring where two fragments met, sized by the bigger one (lower generation).
inline bool AddClash(float x, float y, int family, int minGen) {
    if (ClashRingsAlive(family) >= kClashCap) return false;
    AddRing(x, y, kWidth * std::pow(kGenScale, (float)minGen), NewRun(), family, true);
    return true;
}

// 0 before the burst, 0..1 while it spreads, 1 while it fades.
inline float SpreadOf(float age) { return std::clamp((age - kDelay) / kSpread, 0.0f, 1.0f); }
inline bool Spreading(float age) { return age >= kDelay && age < kDelay + kSpread; }

inline bool Has(const int (&ring)[8], int run) {
    for (int r : ring) if (r == run) return true;
    return false;
}
inline void Put(int (&ring)[8], int& head, int run) { ring[head] = run; head = (head + 1) % 8; }

// What this tick did to the player and what started.
struct TickOut {
    float wakeDamage = 0.0f, clashDamage = 0.0f;
    bool wakeBurst = false;     // a run's first piece just burst
    bool clashBurst = false;    // a clash ring just burst
};

// Ages everything, hits the player (radius pr) once per run, drops the dead.
inline TickOut Tick(float dt, float px, float py, float pr) {
    TickOut out;
    for (Node& n : g.nodes) {
        const float before = n.age;
        n.age += dt;
        if (before < kDelay && n.age >= kDelay && !Has(g.burstRuns, n.run)) {
            Put(g.burstRuns, g.burstHead, n.run);
            out.wakeBurst = true;
        }
        if (!Spreading(n.age) || Has(g.hitRuns, n.run)) continue;
        const float front = kWidth * SpreadOf(n.age);
        const float sx = n.bx - n.ax, sy = n.by - n.ay;
        const float len = std::sqrt(sx * sx + sy * sy);
        if (len < 0.001f) continue;
        const float ux = sx / len, uy = sy / len;
        const float rx = px - n.ax, ry = py - n.ay;
        const float along = rx * ux + ry * uy;
        const float side = std::fabs(rx * -uy + ry * ux);
        if (along >= -pr && along <= len + pr && side <= front + pr) {
            Put(g.hitRuns, g.hitHead, n.run);
            out.wakeDamage += kDamage;
        }
    }
    for (Ring& r : g.rings) {
        const float before = r.age;
        r.age += dt;
        if (r.clash && before < kDelay && r.age >= kDelay) out.clashBurst = true;
        if (!Spreading(r.age) || Has(g.hitRuns, r.run)) continue;
        const float front = r.radius * SpreadOf(r.age);
        const float dx = px - r.x, dy = py - r.y;
        if (dx * dx + dy * dy <= (front + pr) * (front + pr)) {
            Put(g.hitRuns, g.hitHead, r.run);
            (r.clash ? out.clashDamage : out.wakeDamage) += kDamage;
        }
    }
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(),
                                 [](const Node& n) { return n.age >= kLife; }), g.nodes.end());
    g.rings.erase(std::remove_if(g.rings.begin(), g.rings.end(),
                                 [](const Ring& r) { return r.age >= kLife; }), g.rings.end());
    return out;
}

}  // namespace antares_wake
