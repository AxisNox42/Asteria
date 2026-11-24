#pragma once
// ─────────────────────────────────────────────────────────────
// Nightmare REGULUS: royal wave (Docs/Plans/2026-11-23_레굴루스_난이도별_세부.md §8.1-8.2).
// From phase 2, every kInterval the Regulus shows a fan toward the player for
// kWarn, then a band of light runs out through it from kInner to kRadius in
// kTravel. The band hurts the player once (kDamage) and tops up the shield of
// every ally it passes (a new one is kShieldShare of the ally's HP). Phase 3:
// a wider fan and a second band kSecondBand behind the first.
// When a phase gate opens, a full ring with kRingGaps open lanes goes out
// once instead (warned longer, slower).
// Dodge: step out of the fan, stand inside kInner, or stand in a ring lane.
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>

namespace regulus_wave {

inline constexpr float kPi = 3.14159265f;
inline constexpr float kRadius = 900.0f;
inline constexpr float kInner = 80.0f;         // safe inside: the band starts here
inline constexpr float kHalfWidth = 22.0f;     // hit half-width (as the lasers)
inline constexpr float kDamage = 12.0f;
inline constexpr float kInterval = 6.0f;       // fan waves, phase 2 on
inline constexpr float kFirstDelay = 2.0f;     // between buff volleys (0.5 s + 3 s steps)
inline constexpr float kWarn = 0.8f;
inline constexpr float kTravel = 0.7f;
inline constexpr float kFanPhase2 = 1.5707963f;   // 90 deg
inline constexpr float kFanPhase3 = 2.0943951f;   // 120 deg
inline constexpr float kSecondBand = 0.35f;       // phase 3
inline constexpr float kShieldShare = 0.6f;       // of the ally's HP, a new shield
inline constexpr float kFade = 0.15f;             // the band fading at the rim
// The phase-change ring.
inline constexpr float kRingWarn = 1.0f;
inline constexpr float kRingTravel = 1.0f;
inline constexpr int kRingGaps = 3;
inline constexpr float kRingGapArc = 0.6981317f;  // 40 deg per open lane

struct Wave {
    float x = 0.0f, y = 0.0f;
    float aim = 0.0f;          // fan centre (radians)
    float halfArc = 0.0f;      // fan half-width; ring: kPi
    bool ring = false;
    float gapStart = 0.0f;     // ring: centre of the first open lane
    float warn = 0.0f, warnTotal = 0.0f;   // warning left / whole warning
    float age = -1.0f;         // since the band left (-1: still warned)
    float travel = kTravel;
    int bands = 1;
    bool hit[2] = { false, false };   // player hit per band
    const void* source = nullptr;
};

inline float AngleDiff(float a, float b) {
    float d = std::fmod(a - b + kPi, 2.0f * kPi);
    if (d < 0.0f) d += 2.0f * kPi;
    return d - kPi;
}

// Ring lane k's centre angle.
inline float GapAngle(const Wave& w, int k) { return w.gapStart + (float)k * 2.0f * kPi / (float)kRingGaps; }

// Whether the direction (dx, dy) from the wave's centre is covered by it.
inline bool Covers(const Wave& w, float dx, float dy) {
    const float a = std::atan2(dy, dx);
    if (!w.ring) return std::fabs(AngleDiff(a, w.aim)) <= w.halfArc;
    for (int k = 0; k < kRingGaps; ++k)
        if (std::fabs(AngleDiff(a, GapAngle(w, k))) <= kRingGapArc * 0.5f) return false;
    return true;
}

// Band `band`'s radius at time `age` after leaving (-1: not out yet / gone).
inline float BandRadius(const Wave& w, int band, float age) {
    const float t = age - kSecondBand * (float)band;
    if (t < 0.0f || t > w.travel) return -1.0f;
    return kInner + (kRadius - kInner) * (t / w.travel);
}

// Whether band `band` passed over (px, py) between ages a0 and a1 (swept, so
// a long frame cannot jump the band over a target).
inline bool Crossed(const Wave& w, int band, float a0, float a1, float px, float py, float extra = 0.0f) {
    const float t0 = std::max(0.0f, a0 - kSecondBand * (float)band);
    const float t1 = a1 - kSecondBand * (float)band;
    if (t1 < 0.0f || t0 > w.travel) return false;
    const float r0 = kInner + (kRadius - kInner) * std::min(1.0f, t0 / w.travel);
    const float r1 = kInner + (kRadius - kInner) * std::min(1.0f, t1 / w.travel);
    const float dx = px - w.x, dy = py - w.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    const float hw = kHalfWidth + extra;
    if (d < kInner || d < r0 - hw || d > r1 + hw) return false;
    return Covers(w, dx, dy);
}

inline float Life(const Wave& w) { return w.travel + kSecondBand * (float)(w.bands - 1) + kFade; }

inline Wave Fan(float x, float y, float aimX, float aimY, int phase, const void* source) {
    Wave w;
    w.x = x; w.y = y;
    w.aim = std::atan2(aimY - y, aimX - x);
    w.halfArc = (phase >= 3 ? kFanPhase3 : kFanPhase2) * 0.5f;
    w.warn = w.warnTotal = kWarn;
    w.travel = kTravel;
    w.bands = phase >= 3 ? 2 : 1;
    w.source = source;
    return w;
}

inline Wave Ring(float x, float y, float gapStart, const void* source) {
    Wave w;
    w.x = x; w.y = y;
    w.ring = true;
    w.halfArc = kPi;
    w.gapStart = gapStart;
    w.warn = w.warnTotal = kRingWarn;
    w.travel = kRingTravel;
    w.source = source;
    return w;
}

}  // namespace regulus_wave
