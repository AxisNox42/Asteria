#pragma once

// Phase gates (Docs/Plans/2026-11-17_보스엘리트_게임구조_개선.md, 2단계).
//
// A boss or elite cannot be burst past a gate line: its HP stops at the line,
// it shows a pattern (a fixed one for the bosses, a beat of its normal moves
// for the elites), and only then can the next stretch of HP be hit. A strong
// build still clears each stretch fast, but every fight shows its patterns.
// HP is never scaled to the player's damage.
//
//   bosses  75% and 25% (50% is their existing phase change)
//   elites  66% and 33%
//
// Header-only with no game includes, so tests can check the numbers.

#include <algorithm>

namespace phase_gate {

// Boss gates: the fraction of max HP each one stands at, top to bottom.
inline constexpr float kBossLines[] = { 0.75f, 0.25f };
inline constexpr int kBossGateCount = 2;
// Elite gates.
inline constexpr float kEliteLines[] = { 2.0f / 3.0f, 1.0f / 3.0f };
inline constexpr int kEliteGateCount = 2;

// Elites: untouchable this long when a gate is reached (a short flash), then
// held at the line for the show time while they keep fighting.
inline constexpr float kEliteHoldSeconds = 1.2f;
// Show time after the hold. The Quasar's ends early, when its forced beam ends.
inline constexpr float kEliteShowSeconds = 3.5f;
inline constexpr float kQuasarShowMaxSeconds = 7.0f;

// The HP an attack may not take the target below: the next unpassed line, or
// 0 when every gate is passed.
inline float FloorFor(const float* lines, int count, int passed, float maxHp) {
    if (passed < 0 || passed >= count || maxHp <= 0.0f) return 0.0f;
    return lines[passed] * maxHp;
}

// The damage that may land: capped so HP stops at the floor (never negative).
inline float ClampToFloor(float damage, float hp, float floor) {
    if (floor <= 0.0f) return damage;
    return std::min(damage, std::max(0.0f, hp - floor));
}

// True when HP sits on (or under) the floor of a gate not yet reached.
inline bool AtGate(float hp, float floor) {
    return floor > 0.0f && hp <= floor + 0.01f;
}

} // namespace phase_gate
