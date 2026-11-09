#pragma once
// ─────────────────────────────────────────────────────────────
// LYNX: a regular sniper mob. Two long triangular plates flank a diamond core.
// It keeps its distance, tracks the player with a thin aim line, freezes the
// line, then fires a thick hitscan ray from the core along it. From the moment it
// starts aiming it cannot be moved; the ray has no range limit.
// Hull coordinates: +x is forward, in hull units; multiply by
// HullScale(sizeScale) for world pixels. Kept free of game headers so the
// regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>

namespace lynx {

inline constexpr float kPi = 3.14159265f;
inline constexpr float kHpMult = 2.4f;          // of a Rotor
inline constexpr float kSpeedMult = 0.45f;
inline constexpr float kSizeScale = 2.0f;       // about 90 px nose to tail
inline constexpr float kContactRange = 34.0f;
inline constexpr float kContactDps = 4.0f;
inline constexpr int kMaxAlive = 3;             // regular spawns turn into Rotors past this

// Keeps this far from the player.
inline constexpr float kFarRange = 520.0f;
inline constexpr float kNearRange = 380.0f;
inline constexpr float kRetreatMult = 0.6f;
inline constexpr float kEntrySpeed = 120.0f;    // px/s from the spawn edge into the view

// Sniping: rest, aim while tracking, lock the line, fire, fade.
enum SnipePhase { SnipeRest, SnipeAim, SnipeLock, SnipeFlash };
inline constexpr float kRestTime = 2.2f;
inline constexpr float kAimTime = 1.2f;
inline constexpr float kLockTime = 0.25f;
inline constexpr float kFlashTime = 0.28f;
inline constexpr float kTurnRate = 2.4f;        // rad/s while aiming
inline constexpr float kRestTurnRate = 0.9f;
inline constexpr float kRayDrawLength = 8000.0f;  // past any viewport; the hit test has no limit
inline constexpr float kPlayerHitRadius = 10.0f;  // same as enemy shots, x the player's size
inline constexpr float kRayHalfWidth = 16.0f;   // what the laser draws, so it hits what it shows
inline constexpr float kRayDamage = 12.0f;

inline float HullScale(float sizeScale) { return sizeScale * 0.25f; }

inline float WrapAngle(float a) { return std::remainder(a, 6.2831853f); }

struct Vec { float x, y; };

inline Vec HullToWorld(float wx, float wy, float heading, float scale, float hx, float hy) {
    const float c = cosf(heading), s = sinf(heading);
    return { wx + scale * (hx * c - hy * s), wy + scale * (hx * s + hy * c) };
}

// Hitscan: does the ray from (ox, oy) along heading pass over the target?
inline bool RayHits(float ox, float oy, float heading, float tx, float ty, float playerSizeMult = 1.0f) {
    const float dx = tx - ox, dy = ty - oy;
    const float along = dx * cosf(heading) + dy * sinf(heading);
    if (along < 0.0f) return false;
    const float across = -dx * sinf(heading) + dy * cosf(heading);
    return std::fabs(across) <= kPlayerHitRadius * playerSizeMult + kRayHalfWidth;
}

}  // namespace lynx
