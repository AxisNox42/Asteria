#pragma once
// ─────────────────────────────────────────────────────────────
// TRIANGULUM: a regular mob that does nothing alive and pays out when it dies.
// A slow triangular body walks at the player with no attack and no contact
// damage. Its big triangular shell holds three small Swarms, one in each
// corner. When it dies the body freezes and shakes for kHoldTime, then the
// shell breaks and the three scatter within kScatterRadius before they chase. Design: Docs/Design/Triangulum_Mob_Design.md.
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <cmath>

namespace triangulum {

inline constexpr float kPi = 3.14159265f;
inline constexpr float kHpMult = 400.0f / 90.0f; // of a Rotor (90): 400, a little above a Scope (360)
inline constexpr float kSpeedMult = 0.3f;        // of a Rotor
inline constexpr float kSizeScale = 1.6f;          // a big shell: base 28.8 px
inline constexpr int kMaxAlive = 3;              // regular spawns turn into Rotors past this

inline constexpr int kChildCount = 3;
inline constexpr float kChildHpMult = 1.5f;      // of a Swarm
inline constexpr float kOrbitRadius = 20.0f;     // px from the centre to each caged Swarm (toward a shell corner)
inline constexpr float kOrbitSpeed = 0.7f;       // rad/s, the shell and its cargo turning together
inline constexpr float kHoldTime = 0.45f;        // s the dead body freezes and shakes before it bursts
inline constexpr float kShatterTime = 0.38f;     // s the broken shell flies apart after the burst
inline constexpr float kScatterRadius = 50.0f;   // px: the freed Swarms land within this of the centre
inline constexpr float kScatterMin = 0.70f;      // nearest landing, as a share of kScatterRadius
inline constexpr float kScatterJitter = 0.45f;   // rad each Swarm may stray from its corner's direction
inline constexpr float kBurstTime = 0.32f;       // s the freed Swarms are shoved to their landing spot

// Direction (unit vector) from the core to orbiting Swarm k when the ring is
// turned to ringAngle; the three sit 120 degrees apart. It is both where the
// Swarm is drawn and the way it leaves when the body dies.
inline void ChildDir(int k, float ringAngle, float& dirX, float& dirY) {
    const float a = ringAngle + (float)k * (2.0f * kPi / (float)kChildCount);
    dirX = std::cos(a);
    dirY = std::sin(a);
}

// Where freed Swarm k lands, as an offset from the centre. angleNoise is in
// [-1, 1] and radiusNoise in [0, 1] (the caller rolls them), so the three
// scatter unevenly around the body but never past kScatterRadius and never
// inside the spot they were caged at.
inline void ScatterOffset(int k, float ringAngle, float angleNoise, float radiusNoise,
                          float& offX, float& offY) {
    const float a = ringAngle + (float)k * (2.0f * kPi / (float)kChildCount) + angleNoise * kScatterJitter;
    const float r = kScatterRadius * (kScatterMin + (1.0f - kScatterMin) * radiusNoise);
    offX = std::cos(a) * r;
    offY = std::sin(a) * r;
}

}  // namespace triangulum
