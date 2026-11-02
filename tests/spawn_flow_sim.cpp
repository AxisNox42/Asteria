// Spawn flow simulator: drives the real EnemySpawner over whole runs at a
// fixed 60 fps and reports spawns per 10 s by wave (each wave's target is
// taken down after a nominal fight, NominalWaveStartSeconds), the in-wave curve and the
// kind mix, for the current balance table and for a 1x baseline (every
// balance multiplier at 1, flat curve). Mobs are removed as soon as they
// spawn; the spawner budgets per wave, not per live population.
// Run with scripts/test_spawn_flow_sim.ps1 (pass a balance.cfg to compare).
#include <cstdio>
#include <cstring>
#include <vector>

#include "EnemySpawner.h"
#include "MonsterManager.h"
#include "PlayerStats.h"
#include "BalanceTuning.h"
#include "../Core/WaveSystem.h"


static const char* kDiff[4] = { "EASY", "NORMAL", "HARD", "NIGHTMARE" };
static constexpr int kDeciles = 10;

struct WaveCount {
    int regular = 0, scope = 0, elite = 0, boss = 0;
    int decile[kDeciles] = {};
    int kinds[(int)MobKind::MAGNETAR + 1] = {};
};

// Nominal fight length of a wave (waves end on a kill, not a clock).
static float WaveSeconds(int wave, Difficulty difficulty) {
    return NominalWaveStartSeconds(wave + 1, difficulty) - NominalWaveStartSeconds(wave, difficulty);
}

static std::vector<WaveCount> SimulateRun(Difficulty difficulty) {
    srand(12345);
    ResetEnemySpawnState();
    MonsterManager monsters;
    PlayerStats stats;
    const int finalWave = FinalWaveCount(difficulty);
    std::vector<WaveCount> waves(finalWave + 1);
    const float dt = 1.0f / 60.0f;
    int wave = 1;
    float waveStart = 0.0f;
    for (float t = 0.0f; wave <= finalWave; t += dt) {
        const float progress = (t - waveStart) / WaveSeconds(wave, difficulty);
        if (progress >= 1.0f) {
            // The fight's target falls: the next wave begins.
            DebugClearWaveObjective(monsters, t);
            ConsumeWaveClear();
            ++wave;
            waveStart = t;
            continue;
        }
        UpdateEnemySpawns(monsters, stats, 1920, 1080, dt, t, wave, t - waveStart, 0, difficulty);
        const int dec = std::clamp((int)(progress * kDeciles), 0, kDeciles - 1);
        WaveCount& w = waves[wave];
        for (Monster* mob : monsters.monsters) {
            if (IsBossMob(mob->kind)) ++w.boss;
            else if (IsEliteMob(mob->kind)) ++w.elite;
            else {
                ++w.regular; ++w.decile[dec];
                if ((int)mob->kind <= (int)MobKind::MAGNETAR) ++w.kinds[(int)mob->kind];
            }
            delete mob;
        }
        monsters.monsters.clear();
        for (RangedMob* mob : monsters.rangedMobs) { ++w.scope; ++w.decile[dec]; delete mob; }
        monsters.rangedMobs.clear();
    }
    return waves;
}

static void SetBaseline() {
    for (const char* row : { "wv1 = 1", "wv2 = 1", "wv3 = 1", "wv4 = 1",
                             "kSwarm = 1", "kGimbal = 1", "kGravis = 1", "kMagnetar = 1",
                             "scopeInterval = 1", "spawnInterval = 1",
                             "cvStart = 1", "cvPeak = 1", "cvEnd = 1" })
        ApplyBalanceLine(row);
}

int main(int argc, char** argv) {
    if (argc > 1) std::printf("balance file %s: %d rows\n", argv[1], LoadBalanceFile(argv[1]));
    const BalanceTuning live[4] = { g_Balance[0], g_Balance[1], g_Balance[2], g_Balance[3] };

    for (int d = 0; d < 4; ++d) {
        const Difficulty diff = (Difficulty)d;
        for (int i = 0; i < 4; ++i) g_Balance[i] = live[i];
        const std::vector<WaveCount> now = SimulateRun(diff);
        SetBaseline();
        const std::vector<WaveCount> base = SimulateRun(diff);

        std::printf("\n=== %s (%d waves)\n", kDiff[d], FinalWaveCount(diff));
        std::printf("wave type    sec | base/10s  new/10s  ratio  band |  elite boss | swarm rotor gimbal gravis magnetar scope (new)\n");
        for (int w = 1; w <= FinalWaveCount(diff); ++w) {
            const float sec = WaveSeconds(w, diff);
            const float b = (base[w].regular + base[w].scope) * 10.0f / sec;
            const float n = (now[w].regular + now[w].scope) * 10.0f / sec;
            const char* type = IsBossWave(w, diff) ? "boss " : IsEliteWaveNumber(w, diff) ? "elite" : "     ";
            const WaveCount& c = now[w];
            std::printf("%4d %s %5.0f | %8.2f %8.2f %6.3f %5.3f | %5d %4d | %5d %5d %6d %6d %8d %5d\n",
                        w, type, sec, b, n, b > 0 ? n / b : 0.0f, WaveBandMult(live[d], w),
                        c.elite, c.boss,
                        c.kinds[(int)MobKind::SWARM], c.kinds[(int)MobKind::ROTOR],
                        c.kinds[(int)MobKind::GIMBAL], c.kinds[(int)MobKind::GRAVIS],
                        c.kinds[(int)MobKind::MAGNETAR], c.scope);
        }
        // In-wave shape: spawns per decile of wave 2 (an elite wave everywhere).
        std::printf("wave 2 by 10%% | base:");
        for (int i = 0; i < kDeciles; ++i) std::printf(" %3d", base[2].decile[i]);
        std::printf(" | new:");
        for (int i = 0; i < kDeciles; ++i) std::printf(" %3d", now[2].decile[i]);
        std::printf("\n");
        // Same, pooled over every regular wave: each 10% slice's share of the
        // wave's spawns, x10 (1.00 = flat).
        int bd[kDeciles] = {}, nd[kDeciles] = {}, bt = 0, nt = 0;
        for (int w = 1; w <= FinalWaveCount(diff); ++w) {
            if (IsBossWave(w, diff)) continue;
            for (int i = 0; i < kDeciles; ++i) {
                bd[i] += base[w].decile[i]; nd[i] += now[w].decile[i];
                bt += base[w].decile[i]; nt += now[w].decile[i];
            }
        }
        std::printf("pooled share x10 | base:");
        for (int i = 0; i < kDeciles; ++i) std::printf(" %.2f", bt ? bd[i] * 10.0f / bt : 0.0f);
        std::printf("\n                 |  new:");
        for (int i = 0; i < kDeciles; ++i) std::printf(" %.2f", nt ? nd[i] * 10.0f / nt : 0.0f);
        std::printf("\n");
    }
    return 0;
}
