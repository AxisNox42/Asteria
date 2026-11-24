#pragma once
// 석궁 (Docs/Plans/2026-11-24_석궁_저격총_개편_기획.docx): the numbers and the plain rules, kept
// apart from SynergyRuntime.cpp so tests can check them without the game.
//   - The first bolt that hits an enemy leaves a mark (석궁 표식) on it; every later bolt into a
//     marked enemy hits 15% harder than the one before, up to +75% (5 steps), and the mark is
//     gone 4 s after its last hit. A hit reads the mark as it was (the first bolt +0%), then
//     raises it.
#include <algorithm>
#include <cmath>

namespace crossbow {

inline constexpr float kSpeed       = 1.3f;    // bolt speed, times the gun's
inline constexpr float kPerStack    = 0.15f;   // crossbow damage per step of the mark
inline constexpr int   kMaxStacks   = 5;
inline constexpr float kHold        = 4.0f;    // s after the last hit

// n: bolts that have hit since the mark was left (1 = just marked, no bonus yet for that hit).
struct Mark { int n = 0; float t = 0.0f; };

inline float DamageMult(const Mark& m) { return 1.0f + kPerStack * (float)std::min(m.n, kMaxStacks); }

inline void Hit(Mark& m) { m.n = std::min(kMaxStacks, m.n + 1); m.t = kHold; }

// false once the hold has run out (the whole mark goes at once).
inline bool Age(Mark& m, float dt) { m.t -= dt; return m.t > 0.0f; }

}  // namespace crossbow
