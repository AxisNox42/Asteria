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
// Nightmare: a fast run sprays friction sparks that hurt in a wedge in front
// of it; fragments that clash burst where they met (AntaresWake.h).
inline bool AntaresWake(int d) { return d >= kNightmare; }
// Split generations: EASY stops after the first split (2 fragments).
inline int AntaresMaxGen(int d) { return d <= kEasy ? 1 : 2; }
// Chase top speed (fragments too). Turning, afterimages and spin are read
// against this, so a top-speed run turns just as wide on every difficulty.
inline float AntaresTopSpeed(int d) { return d <= kEasy ? 700.0f : 900.0f; }
// Rest after a finished run (the opening to hit it).
inline float AntaresRestTime(int d) { return d <= kEasy ? 1.0f : 0.8f; }

// ── REGULUS (Docs/Plans/2026-11-23_레굴루스_난이도별_세부.md) ──
// The expose opening (phase 3) is on every difficulty: it is the player's way in.
// Allies buffed per volley, and whether the cloak buff is in the draw.
inline int RegulusVolley(int d) { return d <= kEasy ? 2 : 3; }
inline bool RegulusStealth(int d) { return d > kEasy; }
inline float RegulusInterval(int d) { return d <= kEasy ? 4.0f : 3.0f; }
// Phase 2 on: the warning line before each laser, and whether it hurts.
inline float RegulusWarn(int d) { return d <= kEasy ? 0.8f : 0.6f; }
inline bool RegulusLineHurts(int d) { return d > kEasy; }
// Strike lasers at the player per volley: none on EASY, one from phase 2,
// HARD adds the lead shot in phase 3.
inline int RegulusStrikes(int d, int phase) {
    if (d <= kEasy || phase < 2) return 0;
    return (d >= kHard && phase >= 3) ? 2 : 1;
}
// Nightmare: the royal wave (RegulusWave.h), a fan from phase 2 and a ring
// at each phase change.
inline bool RegulusWave(int d) { return d >= kNightmare; }

}  // namespace elite_diff
