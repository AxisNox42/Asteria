#pragma once

#include "../System/Settings.h"
#include <algorithm>

// ─── Boss-rush waves ─────────────────────────────────────────────────────────
// Every wave is a fight with no clock: it ends only when its target falls.
// A boss wave brings a boss (OCTANS/ERIDANUS); every other wave brings one
// elite. The final wave's boss decides the run. Felling an elite or a mid-run
// boss opens a wave reward whose tier follows how fast it fell.

// The elite enters this long after its wave begins (time to read the banner).
inline constexpr float ELITE_SPAWN_DELAY_SECONDS = 1.5f;
// The final boss's fight lasts this long before its judgment verdict starts.
inline constexpr float BOSS_WAVE_DURATION_SECONDS = 15.0f * 60.0f;
// Nominal fight length the in-wave spawn curve is laid over.
inline constexpr float WAVE_CURVE_SECONDS = 120.0f;

// Regular spawns slow down on elite and boss waves (interval x this).
inline constexpr float SPECIAL_WAVE_SPAWN_STRETCH = 1.5f;

// ─── Difficulty wave table ───────────────────────────────────────────────────
// One row per difficulty: run length and the waves that host a boss. The last
// boss wave is always the final wave; its defeat wins the run.
inline constexpr int kMaxWaves = 32;
inline constexpr int kMaxScheduledBosses = 5;

struct WaveSchedule {
    int finalWave;
    int bossCount;
    int bossWaves[kMaxScheduledBosses];
};

inline constexpr WaveSchedule kWaveSchedules[DIFFICULTY_COUNT] = {
    /* EASY      */ {  6, 2, { 3, 6, 0, 0, 0 } },
    /* NORMAL    */ {  8, 3, { 4, 6, 8, 0, 0 } },
    /* HARD      */ { 12, 4, { 4, 7, 10, 12, 0 } },
    /* NIGHTMARE */ { 18, 5, { 5, 9, 13, 16, 18 } },
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

// Every wave of the run that is not a boss wave brings one elite.
inline bool IsEliteWaveNumber(int wave, Difficulty difficulty) {
    return wave >= 1 && wave <= FinalWaveCount(difficulty) &&
           !IsBossWave(wave, difficulty);
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

// About when a wave begins in a typical run, for HP ramps that need a time
// without a real run behind them (debug bosses).
inline float NominalWaveStartSeconds(int wave, Difficulty difficulty) {
    constexpr float kEliteFight = 90.0f, kBossFight = 150.0f;
    float start = 0.0f;
    for (int w = 1; w < wave && w <= FinalWaveCount(difficulty); ++w)
        start += IsBossWave(w, difficulty) ? kBossFight : kEliteFight;
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
