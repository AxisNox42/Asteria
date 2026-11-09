#pragma once
// ─────────────────────────────────────────────────────────────
// MIRA: a regular mob, the smallest piece of ERIDANUS's corruption. A bare core
// wrapped in a corruption zone walks slowly at the player. Standing in the zone
// hurts and slows. When MIRA dies its zone stays where it fell for a few
// seconds. Design: Docs/Design/Mira_Mob_Design.md.
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <vector>

namespace mira {

inline constexpr float kHpMult = 2.0f;          // of a Rotor
inline constexpr float kSpeedMult = 0.5f;
inline constexpr float kSizeScale = 1.0f;
inline constexpr int kMaxAlive = 6;             // regular spawns turn into Rotors past this

// The zone: a disc round the core (drawn as a wobbling blob of the same reach).
inline constexpr float kZoneRadius = 80.0f;
inline constexpr float kZoneDps = 8.0f;         // ERIDANUS's puddle is 20 on HARD
inline constexpr float kZoneMove = 0.85f;       // player speed factor inside / just after
inline constexpr float kLinger = 1.0f;          // seconds the slow outlasts the zone

// Zones left behind shrink in real time, from their start radius to nothing over
// their life (the hurt area shrinks with them).
// A dead MIRA's zone: its whole zone, staying where it fell.
inline constexpr float kDeathLife = 3.0f;
// The trail a walking MIRA drops: smaller, a puddle every kTrailSpacing px.
inline constexpr float kTrailRadius = 52.0f;
inline constexpr float kTrailLife = 4.0f;
inline constexpr float kTrailSpacing = 80.0f;   // consecutive puddles are joined into one ribbon
inline constexpr size_t kMaxZones = 160;

inline bool InZone(float zoneX, float zoneY, float px, float py, float playerSizeMult = 1.0f,
                   float radius = kZoneRadius) {
    const float dx = px - zoneX, dy = py - zoneY;
    const float r = radius + 8.0f * playerSizeMult;
    return dx * dx + dy * dy <= r * r;
}

struct Zone { float x, y, age, life, radius0; int chain; };   // chain > 0: joined to its neighbours
inline std::vector<Zone> g_zones;
inline float g_weaken = 0.0f;                   // > 0: the player is slowed

inline int g_chainCounter = 0;
inline int NewChain() { return ++g_chainCounter; }
inline void AddZone(float x, float y, float life = kDeathLife, float radius0 = kZoneRadius, int chain = 0) {
    if (g_zones.size() < kMaxZones) g_zones.push_back({ x, y, 0.0f, life, radius0, chain });
}
inline void AddTrail(float x, float y, int chain) { AddZone(x, y, kTrailLife, kTrailRadius, chain); }
// The live head of a chain: the walking MIRA itself, so its ribbon runs unbroken
// from its own zone back through the trail. Set by the mob every update; a head
// that was not refreshed this tick (the mob died) is dropped.
struct Head { int chain; float x, y; unsigned stamp; };
inline std::vector<Head> g_heads;
inline unsigned g_tick = 0;
inline void SetHead(int chain, float x, float y) {
    for (Head& h : g_heads)
        if (h.chain == chain) { h.x = x; h.y = y; h.stamp = g_tick; return; }
    g_heads.push_back({ chain, x, y, g_tick });
}
inline const Head* HeadOf(int chain) {
    for (const Head& h : g_heads)
        if (h.chain == chain && h.stamp == g_tick) return &h;
    return nullptr;
}
inline void Reset() { g_zones.clear(); g_heads.clear(); g_weaken = 0.0f; }

// The current radius: shrinks straight down to 0 at the end of its life.
inline float ZoneRadius(const Zone& z) {
    return z.radius0 * std::clamp(1.0f - z.age / z.life, 0.0f, 1.0f);
}

// The older puddle of the same chain just before index i, or -1.
inline int PrevInChain(int i) {
    if (g_zones[i].chain <= 0) return -1;
    for (int j = i - 1; j >= 0; --j)
        if (g_zones[j].chain == g_zones[i].chain) return j;
    return -1;
}

// Is (px, py) inside the ribbon joining zone a to zone b (radius runs from a's to b's)?
inline bool InLink(const Zone& a, const Zone& b, float px, float py, float playerSizeMult) {
    const float vx = b.x - a.x, vy = b.y - a.y;
    const float len2 = vx * vx + vy * vy;
    const float t = len2 > 1e-6f ? std::clamp(((px - a.x) * vx + (py - a.y) * vy) / len2, 0.0f, 1.0f) : 0.0f;
    const float ra = ZoneRadius(a), rb = ZoneRadius(b);
    const float cx = a.x + vx * t, cy = a.y + vy * t;
    const float r = ra + (rb - ra) * t + 8.0f * playerSizeMult;
    const float dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy <= r * r;
}

// A living MIRA's zone is touching the player this step.
inline void Touch() { g_weaken = kLinger; }

inline float MoveFactor() { return g_weaken > 0.0f ? kZoneMove : 1.0f; }

// Ages the dead zones; true while the player stands in one (the caller hurts,
// at kZoneDps * dt). Also winds the slow down.
inline bool Tick(float dt, float px, float py, float playerSizeMult = 1.0f) {
    g_weaken = std::max(0.0f, g_weaken - dt);
    ++g_tick;
    g_heads.erase(std::remove_if(g_heads.begin(), g_heads.end(),
                                 [](const Head& h) { return g_tick - h.stamp > 1; }),
                  g_heads.end());
    bool inside = false;
    for (Zone& z : g_zones) {
        z.age += dt;
        if (z.age < z.life && InZone(z.x, z.y, px, py, playerSizeMult, ZoneRadius(z))) inside = true;
    }
    for (int i = 0; i < (int)g_zones.size() && !inside; ++i) {
        const int j = PrevInChain(i);
        if (j >= 0 && g_zones[i].age < g_zones[i].life && g_zones[j].age < g_zones[j].life &&
            InLink(g_zones[j], g_zones[i], px, py, playerSizeMult)) inside = true;
    }
    g_zones.erase(std::remove_if(g_zones.begin(), g_zones.end(),
                                 [](const Zone& z) { return z.age >= z.life; }),
                  g_zones.end());
    // The ribbon between the newest puddle and the walking MIRA's own zone.
    for (const Head& h : g_heads) {
        if (inside) break;
        int last = -1;
        for (int j = (int)g_zones.size() - 1; j >= 0 && last < 0; --j)
            if (g_zones[j].chain == h.chain) last = j;
        if (last < 0 || g_zones[last].age >= g_zones[last].life) continue;
        const Zone hz{ h.x, h.y, 0.0f, 1.0f, kZoneRadius, h.chain };
        if (InLink(g_zones[last], hz, px, py, playerSizeMult)) inside = true;
    }
    // The ribbon between the newest puddle and the walking MIRA's own zone.
    for (const Head& h : g_heads) {
        if (inside) break;
        int last = -1;
        for (int j = (int)g_zones.size() - 1; j >= 0 && last < 0; --j)
            if (g_zones[j].chain == h.chain) last = j;
        if (last < 0 || g_zones[last].age >= g_zones[last].life) continue;
        const Zone hz{ h.x, h.y, 0.0f, 1.0f, kZoneRadius, h.chain };
        if (InLink(g_zones[last], hz, px, py, playerSizeMult)) inside = true;
    }
    if (inside) Touch();
    return inside;
}

}  // namespace mira
