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
// 2.5 since 2026-11-17: the mobs buried the elite's tells (Regulus waves are
// filled like regular waves and keep their pace).
inline constexpr float SPECIAL_WAVE_SPAWN_STRETCH = 2.5f;

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

// A wave's elite gets this much more HP than the wave's ramp alone gives it.
// It is tuned per difficulty (the run's own enemyHpMultiplier 0.9/1/1.15/1.3 is
// already inside the ramp) so the strongest first elite (Quasar, 3,200 base)
// lands at about 1,500 / 2,500 / 4,000 / 6,000 effective HP on
// EASY / NORMAL / HARD / NIGHTMARE. Later elites follow WaveHpRamp.
// Antares fragments inherit it.
inline float EliteWaveHpMult(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::EASY:      return 0.534f;
    case Difficulty::NORMAL:    return 0.800f;
    case Difficulty::HARD:      return 1.114f;
    case Difficulty::NIGHTMARE: return 1.478f;
    }
    return 0.800f;
}

// Elite waves the run has already passed on its way to `wave`.
inline int EliteWavesBefore(int wave, Difficulty difficulty) {
    int count = 0;
    for (int w = 1; w < wave; ++w)
        count += IsEliteWaveNumber(w, difficulty) ? 1 : 0;
    return count;
}

// Every enemy's HP (regular, elite, boss) climbs a little every wave:
// +kWaveHpStep of the starting HP per wave, so wave 1 is x1. Until 2026-11-17
// it was +30% in one step per elite wave behind, which made the first wave of
// each set the hardest and the end of a set the easiest (plan stage 6).
// Wave 13 lands where the old steps put it (about x1.9); elites now get the
// in-between waves too (wave 4 x1.24, was x1.0), which lengthens their fights.
inline constexpr float kWaveHpStep = 0.08f;
inline float WaveHpRamp(int wave, Difficulty difficulty) {
    (void)difficulty;
    return 1.0f + kWaveHpStep * (float)std::max(0, wave - 1);
}

// Base spawn rate multiplier for every wave (regular and ranged mobs).
inline constexpr float kBaseSpawnRateMult = 1.8f;
// Spawns climb the same way: the spawn rate gains +kWaveSpawnStep and the
// regular-wave refill floor +kRegularWaveMinAliveStep per elite wave behind.
inline constexpr float kWaveSpawnStep = 0.40f;
inline constexpr int kRegularWaveMinAliveStep = 3;
inline float WaveSpawnRamp(int wave, Difficulty difficulty) {
    return kBaseSpawnRateMult * (1.0f + kWaveSpawnStep * (float)EliteWavesBefore(wave, difficulty));
}
inline int RegularWaveMinAlive(int wave, Difficulty difficulty) {
    return kRegularWaveMinAlive + kRegularWaveMinAliveStep * EliteWavesBefore(wave, difficulty);
}

// ─── Mob supply follows the kill rate (2026-11-17 plan, stage 3) ─────────────
// A fixed refill floor capped kills at about one a second however strong the
// build. The floor now rises with the kills of the last kSupplyWindowSeconds:
// the field holds about kSupplyLeadSeconds of them, so a stronger build meets
// more mobs and clears its quota sooner. A separate cap keeps the screen
// readable and the frame rate steady.
inline constexpr float kSupplyWindowSeconds = 5.0f;
inline constexpr float kSupplyLeadSeconds = 6.0f;
inline constexpr int kRegularFieldCap = 24;
inline constexpr int kRegularFieldCapStep = 4;   // + per elite wave behind
inline int RegularWaveFieldCap(int wave, Difficulty difficulty) {
    return kRegularFieldCap + kRegularFieldCapStep * EliteWavesBefore(wave, difficulty);
}
// How many regular mobs the refill keeps alive at this kill rate.
inline int RegularWaveFieldTarget(int wave, Difficulty difficulty, float killsPerSecond) {
    const int floor = RegularWaveMinAlive(wave, difficulty);
    const int cap = std::max(floor, RegularWaveFieldCap(wave, difficulty));
    const float want = std::max(0.0f, killsPerSecond) * kSupplyLeadSeconds;
    const int target = static_cast<int>(want + 0.999f);
    return std::min(cap, std::max(floor, target));
}

// Kills per second over the last kSupplyWindowSeconds, from the run's kill
// count sampled every half second. Reset it when a wave starts.
struct KillRateMeter {
    static constexpr float kSlotSeconds = 0.5f;
    static constexpr int kSlots = static_cast<int>(kSupplyWindowSeconds / kSlotSeconds) + 1;
    long long kills[kSlots] = {};
    float at[kSlots] = {};
    int head = 0;          // next slot to write = the oldest one once full
    int filled = 0;
    float clock = 0.0f, next = 0.0f;
    void Reset(long long runKills) {
        head = 0; filled = 0; clock = 0.0f; next = 0.0f;
        Push(runKills);
    }
    void Push(long long runKills) {
        kills[head] = runKills; at[head] = clock;
        head = (head + 1) % kSlots;
        if (filled < kSlots) ++filled;
        next = clock + kSlotSeconds;
    }
    void Step(float dt, long long runKills) {
        if (filled == 0) Reset(runKills);
        clock += dt;
        if (clock >= next) Push(runKills);
    }
    float PerSecond(long long runKills) const {
        if (filled == 0) return 0.0f;
        const int oldest = filled < kSlots ? 0 : head;
        const float span = clock - at[oldest];
        if (span < 1.0f) return 0.0f;   // too soon to tell: the floor holds
        return (float)(runKills - kills[oldest]) / span;
    }
};

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
// The phase gates (Entity/PhaseGate.h) add about 10 s to an elite and 15 s to
// a boss whatever the build, so the bands moved out by that (2026-11-17; were
// 30/60/90 and 60/120/180).
inline constexpr WaveRewardTimes kEliteRewardTimes = { 40.0f,  70.0f, 100.0f };
inline constexpr WaveRewardTimes kBossRewardTimes  = { 75.0f, 135.0f, 195.0f };

// Highest augment tier (0 Common .. 4 Mythic) a clear of `wave` may offer,
// by how far through the run it is: the first quarter tops out at Rare, then
// Epic, Legendary, and Mythic in the last quarter.
inline int WaveRewardTierCap(int wave, Difficulty difficulty) {
    const float f = (float)wave / (float)std::max(1, FinalWaveCount(difficulty));
    return f <= 0.25f ? 1 : f <= 0.5f ? 2 : f <= 0.75f ? 3 : 4;
}

// The clear's guaranteed tier: one below the cap for the fastest clear, one
// lower per slower speed band (Common at the slowest).
inline int WaveRewardGuaranteedTier(WaveRewardTier earned, int cap) {
    return std::max(0, cap - 1 - ((int)WaveRewardTier::Legendary - (int)earned));
}

// A fastest-band clear has this chance (x luck) for its guaranteed card to
// land one tier above the cap. Pity ceiling (hidden, per run): fastest clears
// that miss the bump are counted in `pity`, and the kRewardPityCeiling-th one
// gets it for sure. A bump resets the count; slower clears and tiers already
// at Mythic leave it alone.
inline constexpr float kRewardLuckyBumpChance = 0.06f;
inline constexpr int kRewardPityCeiling = 8;
inline int WaveRewardLuckyBump(int tier, WaveRewardTier earned, float roll01, float luck, int& pity) {
    if (earned != WaveRewardTier::Legendary || tier >= 4) return tier;
    if (roll01 < kRewardLuckyBumpChance * luck || pity >= kRewardPityCeiling - 1) {
        pity = 0;
        return tier + 1;
    }
    ++pity;
    return tier;
}

// Regular waves: seconds per quota kill that still earn each tier, so the
// window grows with the quota (30 kills: 30 s / 45 s / 60 s).
inline constexpr WaveRewardTimes kRegularRewardSecondsPerKill = { 1.0f, 1.5f, 2.0f };

inline const WaveRewardTimes& WaveRewardTimesFor(bool boss) {
    return boss ? kBossRewardTimes : kEliteRewardTimes;
}

inline WaveRewardTimes RegularWaveRewardTimes(int quota) {
    const WaveRewardTimes& k = kRegularRewardSecondsPerKill;
    const float q = (float)std::max(1, quota);
    return { k.legendary * q, k.epic * q, k.rare * q };
}

inline WaveRewardTier WaveRewardTierFor(float killSeconds, const WaveRewardTimes& t) {
    if (killSeconds <= t.legendary) return WaveRewardTier::Legendary;
    if (killSeconds <= t.epic)      return WaveRewardTier::Epic;
    if (killSeconds <= t.rare)      return WaveRewardTier::Rare;
    return WaveRewardTier::Common;
}
inline WaveRewardTier WaveRewardTierFor(float killSeconds, bool boss) {
    return WaveRewardTierFor(killSeconds, WaveRewardTimesFor(boss));
}

// 1 for an instant kill, falling to 0 once only the Common tier is left.
inline float WaveRewardSpeed(float killSeconds, const WaveRewardTimes& t) {
    return std::clamp(1.0f - killSeconds / t.rare, 0.0f, 1.0f);
}
inline float WaveRewardSpeed(float killSeconds, bool boss) {
    return WaveRewardSpeed(killSeconds, WaveRewardTimesFor(boss));
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
