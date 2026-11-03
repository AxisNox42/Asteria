#pragma once

#include "../System/Settings.h"
#include <algorithm>

// ─── Wave structure ──────────────────────────────────────────────────────────
// Sets of three regular waves and one elite wave, then the final boss:
//   R R R E | R R R E | ... | B
// A regular wave (regular mobs only) ends once its kill quota is met; the
// field refills as mobs fall, so fast killers fight more and finish sooner.
// An elite wave ends when its elite falls. NIGHTMARE swaps one elite wave for
// a mid-run boss. The final wave's boss decides the run. Every wave clear
// opens an augment reward, and the next wave waits until it is picked.

// The elite enters this long after its wave begins (time to read the banner).
inline constexpr float ELITE_SPAWN_DELAY_SECONDS = 1.5f;
// The final boss's fight lasts this long before its judgment verdict starts.
inline constexpr float BOSS_WAVE_DURATION_SECONDS = 15.0f * 60.0f;
// Nominal fight length the in-wave spawn curve is laid over.
inline constexpr float WAVE_CURVE_SECONDS = 120.0f;

// Regular spawn interval x this on elite waves (boss waves spawn none at all).
// 1.0 = the regular-wave pace; raise it to thin the mobs around the elite.
inline constexpr float SPECIAL_WAVE_SPAWN_STRETCH = 1.0f;

// Regular waves: kills to clear the first one, the rise per regular wave in
// the first set, and how much that rise grows after each elite wave.
inline constexpr int kRegularKillQuotaBase = 30;
inline constexpr int kRegularKillQuotaStep = 15;
inline constexpr int kRegularKillQuotaStepRise = 5;
// While a regular wave runs, the field is refilled at once whenever fewer
// regular mobs than this are alive (on top of the normal spawn pace).
inline constexpr int kRegularWaveMinAlive = 8;

// ─── Difficulty wave table ───────────────────────────────────────────────────
// One row per difficulty: run length, its elite waves and its boss waves (the
// last boss wave is always the final wave). Every other wave is regular.
inline constexpr int kMaxWaves = 32;
inline constexpr int kMaxScheduledElites = 5;
inline constexpr int kMaxScheduledBosses = 2;

struct WaveSchedule {
    int finalWave;
    int eliteCount;
    int eliteWaves[kMaxScheduledElites];
    int bossCount;
    int bossWaves[kMaxScheduledBosses];
};

inline constexpr WaveSchedule kWaveSchedules[DIFFICULTY_COUNT] = {
    /* EASY      */ {  9, 2, { 4, 8, 0, 0, 0 },      1, {  9, 0 } },
    /* NORMAL    */ { 13, 3, { 4, 8, 12, 0, 0 },     1, { 13, 0 } },
    /* HARD      */ { 17, 4, { 4, 8, 12, 16, 0 },    1, { 17, 0 } },
    /* NIGHTMARE */ { 21, 4, { 4, 8, 16, 20, 0 },    2, { 12, 21 } },   // 12: mid-run boss
};

inline const WaveSchedule& WaveScheduleFor(Difficulty difficulty) {
    const int idx = static_cast<int>(difficulty);
    return kWaveSchedules[idx >= 0 && idx < DIFFICULTY_COUNT ? idx : 1];
}

inline int FinalWaveCount(Difficulty difficulty) {
    return WaveScheduleFor(difficulty).finalWave;
}

inline bool IsBossWave(int wave, Difficulty difficulty) {
    const WaveSchedule& s = WaveScheduleFor(difficulty);
    for (int i = 0; i < s.bossCount; ++i)
        if (s.bossWaves[i] == wave) return true;
    return false;
}

// The boss whose defeat wins the run.
inline bool IsFinalBossWave(int wave, Difficulty difficulty) {
    return wave == FinalWaveCount(difficulty);
}

inline bool IsEliteWaveNumber(int wave, Difficulty difficulty) {
    const WaveSchedule& s = WaveScheduleFor(difficulty);
    for (int i = 0; i < s.eliteCount; ++i)
        if (s.eliteWaves[i] == wave) return true;
    return false;
}

// Zero-based position of `wave` among the elite waves, or -1.
inline int EliteOrdinal(int wave, Difficulty difficulty) {
    if (!IsEliteWaveNumber(wave, difficulty)) return -1;
    int ordinal = 0;
    for (int w = 1; w < wave; ++w)
        ordinal += IsEliteWaveNumber(w, difficulty) ? 1 : 0;
    return ordinal;
}

inline bool IsSpecialWave(int wave, Difficulty difficulty) {
    return IsBossWave(wave, difficulty) || IsEliteWaveNumber(wave, difficulty);
}

// A wave of regular mobs only, cleared by its kill quota.
inline bool IsRegularWave(int wave, Difficulty difficulty) {
    return wave >= 1 && wave <= FinalWaveCount(difficulty) && !IsSpecialWave(wave, difficulty);
}

// Kills that clear a regular wave (0 on any other wave). Each regular wave
// asks for the previous one's quota plus the set's step; the step grows by
// kRegularKillQuotaStepRise with every elite (or mid-boss) wave passed, so
//   set 1: 30 45 60 | set 2: 80 100 120 | set 3: 145 170 195 | ...
inline int RegularWaveKillQuota(int wave, Difficulty difficulty) {
    if (!IsRegularWave(wave, difficulty)) return 0;
    int quota = 0, step = kRegularKillQuotaStep, sets = 0;
    for (int w = 1; w <= wave; ++w) {
        if (IsSpecialWave(w, difficulty)) { ++sets; continue; }
        step = kRegularKillQuotaStep + kRegularKillQuotaStepRise * sets;
        quota = quota == 0 ? kRegularKillQuotaBase : quota + step;
    }
    return quota;
}

// A wave's elite gets this much more HP than the wave's ramp alone gives it
// (first elite on NORMAL ≈ 9,000 effective HP). Antares fragments inherit it.
inline constexpr float kEliteWaveHpMult = 3.0f;

// Elite waves the run has already passed on its way to `wave`.
inline int EliteWavesBefore(int wave, Difficulty difficulty) {
    int count = 0;
    for (int w = 1; w < wave; ++w)
        count += IsEliteWaveNumber(w, difficulty) ? 1 : 0;
    return count;
}

// Every enemy's HP (regular, elite, boss) climbs once per elite wave behind
// the run: +kWaveHpStep of the starting HP each time, so wave 1 is x1.
inline constexpr float kWaveHpStep = 0.30f;
inline float WaveHpRamp(int wave, Difficulty difficulty) {
    return 1.0f + kWaveHpStep * (float)EliteWavesBefore(wave, difficulty);
}

// Spawns climb the same way: the spawn rate gains +kWaveSpawnStep and the
// regular-wave refill floor +kRegularWaveMinAliveStep per elite wave behind.
inline constexpr float kWaveSpawnStep = 0.25f;
inline constexpr int kRegularWaveMinAliveStep = 3;
inline float WaveSpawnRamp(int wave, Difficulty difficulty) {
    return 1.0f + kWaveSpawnStep * (float)EliteWavesBefore(wave, difficulty);
}
inline int RegularWaveMinAlive(int wave, Difficulty difficulty) {
    return kRegularWaveMinAlive + kRegularWaveMinAliveStep * EliteWavesBefore(wave, difficulty);
}

// About when a wave begins in a typical run, for HP ramps that need a time
// without a real run behind them (debug bosses).
inline float NominalWaveStartSeconds(int wave, Difficulty difficulty) {
    constexpr float kRegularFight = 60.0f, kEliteFight = 90.0f, kBossFight = 150.0f;
    float start = 0.0f;
    for (int w = 1; w < wave && w <= FinalWaveCount(difficulty); ++w)
        start += IsBossWave(w, difficulty) ? kBossFight
               : IsEliteWaveNumber(w, difficulty) ? kEliteFight : kRegularFight;
    return start;
}

// Who a spawn belongs to, for the slowdown exemption hook.
enum class SpawnSource { Regular, Ranged, Regulus };

// Hook for spawns that must ignore the elite/boss-wave slowdown. Regulus is
// exempt (it is becoming an elite). It still rolls through the shared regular
// timer today; give it its own timer when it moves to the elite path and ask
// this hook with SpawnSource::Regulus.
inline bool ExemptFromSpecialWaveSlowdown(SpawnSource source) {
    return source == SpawnSource::Regulus;
}

// Interval multiplier from the wave type (1.0 = unchanged). It is derived
// from the current wave every frame, so it ends with the wave on its own.
inline float SpecialWaveSpawnStretch(int wave, Difficulty difficulty,
                                     SpawnSource source = SpawnSource::Regular) {
    if (!IsSpecialWave(wave, difficulty)) return 1.0f;
    if (ExemptFromSpecialWaveSlowdown(source)) return 1.0f;
    return SPECIAL_WAVE_SPAWN_STRETCH;
}

// ─── Wave reward tier ────────────────────────────────────────────────────────
// The faster the wave's target falls, the higher the reward: one card of the
// earned tier is guaranteed, and the other cards lean upward with speed.
enum class WaveRewardTier { Common, Rare, Epic, Legendary };

// Kill time (seconds since the target entered) that still earns each tier.
struct WaveRewardTimes { float legendary, epic, rare; };
inline constexpr WaveRewardTimes kEliteRewardTimes = { 30.0f,  60.0f,  90.0f };
inline constexpr WaveRewardTimes kBossRewardTimes  = { 60.0f, 120.0f, 180.0f };

inline const WaveRewardTimes& WaveRewardTimesFor(bool boss) {
    return boss ? kBossRewardTimes : kEliteRewardTimes;
}

inline WaveRewardTier WaveRewardTierFor(float killSeconds, bool boss) {
    const WaveRewardTimes& t = WaveRewardTimesFor(boss);
    if (killSeconds <= t.legendary) return WaveRewardTier::Legendary;
    if (killSeconds <= t.epic)      return WaveRewardTier::Epic;
    if (killSeconds <= t.rare)      return WaveRewardTier::Rare;
    return WaveRewardTier::Common;
}

// 1 for an instant kill, falling to 0 once only the Common tier is left.
inline float WaveRewardSpeed(float killSeconds, bool boss) {
    const float slowest = WaveRewardTimesFor(boss).rare;
    return std::clamp(1.0f - killSeconds / slowest, 0.0f, 1.0f);
}

// Seconds the current tier still holds, and that tier's whole window, for the
// HUD's draining node. Both are 0 once the Common tier is reached.
inline void WaveRewardTierWindow(float killSeconds, bool boss,
                                 float& secondsLeft, float& windowSeconds) {
    const WaveRewardTimes& t = WaveRewardTimesFor(boss);
    const float edges[4] = { 0.0f, t.legendary, t.epic, t.rare };
    secondsLeft = windowSeconds = 0.0f;
    for (int i = 1; i < 4; ++i) {
        if (killSeconds <= edges[i]) {
            secondsLeft = edges[i] - killSeconds;
            windowSeconds = edges[i] - edges[i - 1];
            return;
        }
    }
}
