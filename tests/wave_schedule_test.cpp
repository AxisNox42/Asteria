// Wave schedule: sets of three regular waves and an elite wave, the final
// boss (NIGHTMARE also a mid-run boss), regular kill quotas, the elite-wave
// spawn pace, the HP ramp and the kill-time reward tiers, per difficulty.
#include <cstdio>

#include "WaveSystem.h"

static int g_failures = 0;
#define EXPECT(cond, ...)                                   \
    do {                                                    \
        if (!(cond)) {                                      \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                       \
            std::printf("\n");                              \
            ++g_failures;                                   \
        }                                                   \
    } while (0)

static int Count(bool (*pred)(int, Difficulty), Difficulty d) {
    int n = 0;
    for (int w = 1; w <= FinalWaveCount(d); ++w) n += pred(w, d) ? 1 : 0;
    return n;
}

int main() {
    const Difficulty easy = Difficulty::EASY, normal = Difficulty::NORMAL,
                     hard = Difficulty::HARD, nightmare = Difficulty::NIGHTMARE;

    // Sets of 3 regular + 1 elite, then the final boss.
    EXPECT(FinalWaveCount(easy) == 9, "easy waves");
    EXPECT(FinalWaveCount(normal) == 13, "normal waves");
    EXPECT(FinalWaveCount(hard) == 17, "hard waves");
    EXPECT(FinalWaveCount(nightmare) == 21, "nightmare waves");

    EXPECT(Count(IsEliteWaveNumber, easy) == 2 && Count(IsBossWave, easy) == 1, "easy counts");
    EXPECT(Count(IsEliteWaveNumber, normal) == 3 && Count(IsBossWave, normal) == 1, "normal counts");
    EXPECT(Count(IsEliteWaveNumber, hard) == 4 && Count(IsBossWave, hard) == 1, "hard counts");
    // NIGHTMARE: the third set's elite wave is a mid-run boss instead.
    EXPECT(Count(IsEliteWaveNumber, nightmare) == 4 && Count(IsBossWave, nightmare) == 2, "nightmare counts");
    EXPECT(IsBossWave(12, nightmare) && !IsFinalBossWave(12, nightmare), "nightmare mid boss at 12");

    for (int d = 0; d < DIFFICULTY_COUNT; ++d) {
        const Difficulty diff = static_cast<Difficulty>(d);
        const int last = FinalWaveCount(diff);
        EXPECT(last <= kMaxWaves, "run fits kMaxWaves (d=%d)", d);
        EXPECT(IsBossWave(last, diff) && IsFinalBossWave(last, diff), "final wave is a boss wave (d=%d)", d);
        int finals = 0, ordinal = 0;
        for (int w = 1; w <= last; ++w) {
            finals += IsFinalBossWave(w, diff) ? 1 : 0;
            const bool setEnd = w % 4 == 0;
            if (w < last) {
                // Waves 1-3 of each set are regular; the 4th is the elite
                // (or NIGHTMARE's mid boss).
                EXPECT(IsRegularWave(w, diff) == !setEnd, "regular pattern d=%d w=%d", d, w);
                EXPECT(!setEnd || IsSpecialWave(w, diff), "set end is special d=%d w=%d", d, w);
            }
            const int kinds = (IsRegularWave(w, diff) ? 1 : 0) + (IsEliteWaveNumber(w, diff) ? 1 : 0) +
                              (IsBossWave(w, diff) ? 1 : 0);
            EXPECT(kinds == 1, "one kind d=%d w=%d", d, w);
            if (IsEliteWaveNumber(w, diff))
                EXPECT(EliteOrdinal(w, diff) == ordinal++, "ordinal d=%d w=%d", d, w);
            EXPECT((RegularWaveKillQuota(w, diff) > 0) == IsRegularWave(w, diff), "quota only on regular d=%d w=%d", d, w);
            EXPECT(NominalWaveStartSeconds(w + 1, diff) > NominalWaveStartSeconds(w, diff),
                   "nominal clock rises d=%d w=%d", d, w);
        }
        EXPECT(finals == 1, "one final boss (d=%d)", d);
        EXPECT(!IsRegularWave(0, diff) && !IsRegularWave(last + 1, diff), "outside the run (d=%d)", d);
    }

    // Quota: each regular wave = previous + the set's step; the step grows by
    // 5 after every elite (or mid-boss) wave.
    //   30 45 60 | 80 100 120 | 145 170 195 | 225 255 285 | 320 355 390
    {
        const int want[] = { 30, 45, 60, 80, 100, 120, 145, 170, 195, 225, 255, 285, 320, 355, 390 };
        int i = 0;
        for (int w = 1; w <= FinalWaveCount(nightmare); ++w) {
            if (!IsRegularWave(w, nightmare)) continue;
            EXPECT(RegularWaveKillQuota(w, nightmare) == want[i], "nightmare wave %d quota %d (want %d)",
                   w, RegularWaveKillQuota(w, nightmare), want[i]);
            ++i;
        }
        EXPECT(i == 15, "nightmare has 15 regular waves (%d)", i);
        EXPECT(RegularWaveKillQuota(5, normal) == 80 && RegularWaveKillQuota(9, normal) == 145, "normal sets");
    }

    // HP ramp: +kWaveHpStep per elite wave behind; mid boss adds no step.
    EXPECT(WaveHpRamp(4, normal) == 1.0f, "no elite behind before wave 5");
    EXPECT(WaveHpRamp(5, normal) == 1.0f + kWaveHpStep, "one elite behind");
    EXPECT(EliteWavesBefore(13, nightmare) == 2, "nightmare mid boss is not an elite");
    // Spawns ramp per elite wave behind too.
    EXPECT(WaveSpawnRamp(3, normal) == kBaseSpawnRateMult &&
           WaveSpawnRamp(5, normal) == kBaseSpawnRateMult * (1.0f + kWaveSpawnStep),
           "spawn ramp per elite wave");
    EXPECT(RegularWaveMinAlive(1, normal) == kRegularWaveMinAlive &&
           RegularWaveMinAlive(9, normal) == kRegularWaveMinAlive + 2 * kRegularWaveMinAliveStep,
           "refill floor per elite wave");

    // Spawn pace: elite waves use SPECIAL_WAVE_SPAWN_STRETCH, regular waves 1.
    EXPECT(SpecialWaveSpawnStretch(1, nightmare) == 1.0f, "regular wave pace");
    EXPECT(SpecialWaveSpawnStretch(4, nightmare) == SPECIAL_WAVE_SPAWN_STRETCH, "elite pace");
    EXPECT(SpecialWaveSpawnStretch(4, nightmare, SpawnSource::Regulus) == 1.0f, "regulus exempt");

    // Reward tiers fall with the kill time; bosses get longer windows.
    EXPECT(WaveRewardTierFor(0.0f, false) == WaveRewardTier::Legendary, "instant elite kill");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.legendary + 0.1f, false) == WaveRewardTier::Epic, "epic");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.rare + 0.1f, false) == WaveRewardTier::Common, "common");
    EXPECT(WaveRewardSpeed(20.0f, false) > WaveRewardSpeed(40.0f, false), "speed falls with time");
    // Tier cap climbs through the run: NORMAL 1-3 Rare, 4-6 Epic, 7-9
    // Legendary, 10-12 Mythic; the guaranteed card is the cap only when fast.
    EXPECT(WaveRewardTierCap(1, normal) == 1 && WaveRewardTierCap(3, normal) == 1, "early cap rare");
    EXPECT(WaveRewardTierCap(4, normal) == 2 && WaveRewardTierCap(7, normal) == 3 &&
           WaveRewardTierCap(12, normal) == 4, "cap climbs");
    for (int d = 0; d < DIFFICULTY_COUNT; ++d)
        for (int w = 2; w <= FinalWaveCount((Difficulty)d); ++w)
            EXPECT(WaveRewardTierCap(w, (Difficulty)d) >= WaveRewardTierCap(w - 1, (Difficulty)d),
                   "cap never drops d=%d w=%d", d, w);
    EXPECT(WaveRewardGuaranteedTier(WaveRewardTier::Legendary, 1) == 0, "fast early clear: common");
    EXPECT(WaveRewardGuaranteedTier(WaveRewardTier::Epic, 1) == 0, "slower early clear: common");
    EXPECT(WaveRewardGuaranteedTier(WaveRewardTier::Legendary, 4) == 3, "fast late clear: legendary");
    // Lucky bump: fastest band only, one tier above, never past Mythic.
    {
        int pity = 0;
        EXPECT(WaveRewardLuckyBump(1, WaveRewardTier::Legendary, 0.05f, 1.0f, pity) == 2 && pity == 0, "lucky bump");
        EXPECT(WaveRewardLuckyBump(1, WaveRewardTier::Legendary, 0.50f, 1.0f, pity) == 1 && pity == 1, "no luck counts");
        EXPECT(WaveRewardLuckyBump(0, WaveRewardTier::Epic, 0.0f, 1.0f, pity) == 0 && pity == 1, "slow clear: no count");
        EXPECT(WaveRewardLuckyBump(4, WaveRewardTier::Legendary, 0.0f, 1.0f, pity) == 4 && pity == 1, "mythic: no count");
        EXPECT(WaveRewardLuckyBump(1, WaveRewardTier::Legendary, 0.10f, 2.0f, pity) == 2 && pity == 0, "luck widens it, resets");
        // Pity: seven misses, then the eighth fastest clear is bumped for sure.
        for (int i = 0; i < kRewardPityCeiling - 1; ++i)
            EXPECT(WaveRewardLuckyBump(1, WaveRewardTier::Legendary, 0.99f, 1.0f, pity) == 1, "miss %d", i);
        EXPECT(pity == kRewardPityCeiling - 1, "pity counted (%d)", pity);
        EXPECT(WaveRewardLuckyBump(1, WaveRewardTier::Legendary, 0.99f, 1.0f, pity) == 2 && pity == 0,
               "ceiling guarantees the bump");
    }
    // Regular waves: the windows scale with the quota (seconds per kill).
    {
        const WaveRewardTimes t30 = RegularWaveRewardTimes(30), t60 = RegularWaveRewardTimes(60);
        EXPECT(t30.legendary == 30.0f && t30.rare == 60.0f, "regular 30-kill windows");
        EXPECT(t60.legendary == 2.0f * t30.legendary, "windows grow with the quota");
        EXPECT(WaveRewardTierFor(29.0f, t30) == WaveRewardTier::Legendary, "fast regular clear");
        EXPECT(WaveRewardTierFor(61.0f, t30) == WaveRewardTier::Common, "slow regular clear");
    }

    if (g_failures == 0) std::printf("wave schedule: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
