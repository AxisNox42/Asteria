#pragma once
// 석궁 (Docs/Plans/2026-11-24_석궁_저격총_개편_기획.docx): the numbers and the plain rules, kept
// apart from SynergyRuntime.cpp so tests can check them without the game.
//   - It aims for itself: the enemy nearest the cursor, kept until it dies or leaves the screen,
//     shot at where it will be when the bolt arrives (the bolt still flies straight).
//   - Every bolt that hits leaves one more arrow stuck in that enemy: +15% crossbow damage
//     taken per arrow, up to 5, all gone 4 s after its last hit. A hit reads the arrows already
//     in (the first bolt +0%), then adds its own.
#include <algorithm>
#include <cmath>

namespace crossbow {

inline constexpr float kSpeed       = 1.3f;    // bolt speed, times the gun's
inline constexpr float kPerArrow    = 0.15f;   // crossbow damage taken per stuck arrow
inline constexpr int   kMaxArrows   = 5;
inline constexpr float kHold        = 4.0f;    // s after the last hit
inline constexpr float kMaxLead     = 1.2f;    // s of target movement led at most
inline constexpr float kLockIn      = 0.10f;   // s the reticle takes to close on a new target

struct Arrows { int n = 0; float t = 0.0f; };

inline float DamageMult(const Arrows& a) { return 1.0f + kPerArrow * (float)std::min(a.n, kMaxArrows); }

inline void Stick(Arrows& a) { a.n = std::min(kMaxArrows, a.n + 1); a.t = kHold; }

// false once the hold has run out (all of them fall out at once).
inline bool Age(Arrows& a, float dt) { a.t -= dt; return a.t > 0.0f; }

// Where to shoot from (sx, sy) at a bolt speed `speed` to meet a target at (tx, ty) moving (vx, vy).
inline void Lead(float sx, float sy, float tx, float ty, float vx, float vy, float speed,
                 float& ax, float& ay) {
    ax = tx; ay = ty;
    if (!(speed > 1.0f)) return;
    for (int k = 0; k < 3; ++k) {
        const float t = std::min(kMaxLead, std::hypot(ax - sx, ay - sy) / speed);
        ax = tx + vx * t; ay = ty + vy * t;
    }
}

}  // namespace crossbow
