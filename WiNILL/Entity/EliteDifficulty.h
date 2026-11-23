#pragma once
// ─────────────────────────────────────────────────────────────
// Elite patterns per difficulty (Docs/Plans/2026-11-23_엘리트_난이도별_패턴.md).
//   EASY      learn the elite: its two core patterns, longer tells, softer numbers
//   NORMAL    the default numbers, phase-2 patterns on
//   HARD      the whole elite as it was before this table (the reference)
//   NIGHTMARE HARD + one pattern seen only here
// Difficulty is the int of Difficulty (0 EASY, 1 NORMAL, 2 HARD, 3 NIGHTMARE).
// Kept free of game headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────

namespace elite_diff {

inline constexpr int kEasy = 0, kNormal = 1, kHard = 2, kNightmare = 3;

// ── ANTARES (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md) ──
// Phase-3 lance: from HARD.
inline bool AntaresLance(int d) { return d >= kHard; }
// Nightmare wake: fast runs and dashes leave a path that bursts sideways,
// fragments that clash burst where they met (AntaresWake.h).
inline bool AntaresWake(int d) { return d >= kNightmare; }
// Split generations: EASY stops after the first split (2 fragments).
inline int AntaresMaxGen(int d) { return d <= kEasy ? 1 : 2; }
// Chase top speed (fragments too). Turning, afterimages and spin are read
// against this, so a top-speed run turns just as wide on every difficulty.
inline float AntaresTopSpeed(int d) { return d <= kEasy ? 700.0f : 900.0f; }
// Gather tell (closing rings, then the six lines) length factor.
inline float AntaresTellScale(int d) { return d <= kEasy ? 1.3f : 1.0f; }
// Rest after a finished run (the opening to hit it).
inline float AntaresRestTime(int d) { return d <= kEasy ? 1.0f : 0.8f; }

}  // namespace elite_diff
