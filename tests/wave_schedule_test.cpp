// Wave schedule: run length, boss placement (every other wave is an elite
// wave), the special-wave spawn slowdown (with its exemption hook), and the
// kill-time reward tiers, per difficulty.
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

    EXPECT(FinalWaveCount(easy) == 6, "easy waves");
    EXPECT(FinalWaveCount(normal) == 8, "normal waves");
    EXPECT(FinalWaveCount(hard) == 12, "hard waves");
    EXPECT(FinalWaveCount(nightmare) == 18, "nightmare waves");

    EXPECT(Count(IsBossWave, easy) == 2, "easy bosses");
    EXPECT(Count(IsBossWave, normal) == 3, "normal bosses");
    EXPECT(Count(IsBossWave, hard) == 4, "hard bosses");
    EXPECT(Count(IsBossWave, nightmare) == 5, "nightmare bosses");

    for (int d = 0; d < DIFFICULTY_COUNT; ++d) {
        const Difficulty diff = static_cast<Difficulty>(d);
        const int last = FinalWaveCount(diff);
        EXPECT(last <= kMaxWaves, "run fits kMaxWaves (d=%d)", d);
        EXPECT(IsEliteWaveNumber(1, diff) && !IsBossWave(1, diff), "wave 1 is an elite wave (d=%d)", d);
        EXPECT(IsBossWave(last, diff) && IsFinalBossWave(last, diff), "final wave is a boss wave (d=%d)", d);
        int finals = 0, ordinal = 0;
        for (int w = 1; w <= last; ++w) {
            finals += IsFinalBossWave(w, diff) ? 1 : 0;
            // Every wave is exactly one of elite / boss.
            EXPECT(IsEliteWaveNumber(w, diff) != IsBossWave(w, diff), "one kind d=%d w=%d", d, w);
            EXPECT(IsSpecialWave(w, diff), "every wave is a fight d=%d w=%d", d, w);
            if (IsEliteWaveNumber(w, diff))
                EXPECT(EliteOrdinal(w, diff) == ordinal++, "ordinal d=%d w=%d", d, w);
            else
                EXPECT(EliteOrdinal(w, diff) == -1, "no ordinal on a boss wave d=%d w=%d", d, w);
            // A boss never follows a boss: an elite wave sits between them.
            if (w > 1) EXPECT(!(IsBossWave(w, diff) && IsBossWave(w - 1, diff)),
                              "back-to-back bosses d=%d w=%d", d, w);
            EXPECT(NominalWaveStartSeconds(w + 1, diff) > NominalWaveStartSeconds(w, diff),
                   "nominal clock rises d=%d w=%d", d, w);
        }
        EXPECT(finals == 1, "one final boss (d=%d)", d);
        EXPECT(!IsEliteWaveNumber(0, diff) && !IsEliteWaveNumber(last + 1, diff), "outside the run (d=%d)", d);
    }

    // Slowdown: on every wave of the run, the hook can opt out.
    EXPECT(SpecialWaveSpawnStretch(1, nightmare) == SPECIAL_WAVE_SPAWN_STRETCH, "elite slowdown");
    EXPECT(SpecialWaveSpawnStretch(9, nightmare) == SPECIAL_WAVE_SPAWN_STRETCH, "boss slowdown");
    EXPECT(SpecialWaveSpawnStretch(5, nightmare, SpawnSource::Regulus) == 1.0f, "regulus exempt");
    EXPECT(SpecialWaveSpawnStretch(5, nightmare, SpawnSource::Ranged) == SPECIAL_WAVE_SPAWN_STRETCH, "ranged still slowed");

    // Reward tiers fall with the kill time; bosses get longer windows.
    EXPECT(WaveRewardTierFor(0.0f, false) == WaveRewardTier::Legendary, "instant elite kill");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.legendary, false) == WaveRewardTier::Legendary, "legendary edge");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.legendary + 0.1f, false) == WaveRewardTier::Epic, "epic");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.epic + 0.1f, false) == WaveRewardTier::Rare, "rare");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.rare + 0.1f, false) == WaveRewardTier::Common, "common");
    EXPECT(WaveRewardTierFor(kEliteRewardTimes.rare + 0.1f, true) != WaveRewardTier::Common, "boss window longer");
    EXPECT(WaveRewardSpeed(0.0f, false) == 1.0f, "speed at 0 s");
    EXPECT(WaveRewardSpeed(kEliteRewardTimes.rare, false) == 0.0f, "speed gone at the common edge");
    EXPECT(WaveRewardSpeed(20.0f, false) > WaveRewardSpeed(40.0f, false), "speed falls with time");
    {
        float left = 0.0f, window = 0.0f;
        WaveRewardTierWindow(10.0f, false, left, window);
        EXPECT(left == kEliteRewardTimes.legendary - 10.0f && window == kEliteRewardTimes.legendary,
               "legendary window (%.1f/%.1f)", left, window);
        WaveRewardTierWindow(kEliteRewardTimes.epic + 5.0f, false, left, window);
        EXPECT(window == kEliteRewardTimes.rare - kEliteRewardTimes.epic, "rare window");
        WaveRewardTierWindow(kEliteRewardTimes.rare + 5.0f, false, left, window);
        EXPECT(left == 0.0f && window == 0.0f, "common has no window");
    }

    if (g_failures == 0) std::printf("wave schedule: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
