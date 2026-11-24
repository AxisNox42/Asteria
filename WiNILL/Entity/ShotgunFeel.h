#pragma once

// 샷건 타격감 (Docs/Plans/2026-11-24_샷건_타격감.md): near-hitscan pellets, a tracer streak,
// a muzzle flash, a short recoil of the drawn ship and a stronger knockback on hit.
namespace shotgun_feel {

inline constexpr float kSpeedMult   = 5.8f;    // x the shot speed (1200 -> ~7000 px/s): the 7○ range in ~0.08 s
inline constexpr float kTracerLen   = 84.0f;   // px of streak behind a pellet (never longer than it has flown)
inline constexpr float kFlashTime   = 0.06f;   // s the muzzle flash shows
inline constexpr float kRecoilPx    = 6.0f;    // px the drawn ship kicks back (its real position does not move)
inline constexpr float kRecoilTime  = 0.09f;   // s to settle back
inline constexpr float kKnockMult   = 1.8f;    // x the hit-stun knock speed of a regular enemy

// Set when a volley leaves the ship (main.cpp); read by the draw code.
inline float g_FlashT = 0.0f;    // counts down from kFlashTime
inline float g_FlashAng = 0.0f;  // aim of the volley
inline float g_FlashHalf = 0.30f;   // half the spread (rad)
inline float g_RecoilT = 0.0f;   // counts down from kRecoilTime

inline void OnVolley(float aim, float spread) {
    g_FlashT = kFlashTime; g_FlashAng = aim; g_FlashHalf = spread * 0.5f;
    g_RecoilT = kRecoilTime;
}
inline void Tick(float dt) {
    if (g_FlashT > 0.0f) g_FlashT -= dt;
    if (g_RecoilT > 0.0f) g_RecoilT -= dt;
}
// How far back the ship is drawn now (px along -aim).
inline float RecoilOffset() {
    if (g_RecoilT <= 0.0f) return 0.0f;
    const float k = g_RecoilT / kRecoilTime;   // 1 at the shot -> 0
    return kRecoilPx * k * k;
}

}  // namespace shotgun_feel
