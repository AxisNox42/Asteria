// Boss-rush wave flow through the real spawner: a wave's elite enters after
// its short delay, the wave clears only when it falls (with the kill time),
// a mid-run boss wave clears on the boss's fall, and the final boss's fall
// wins instead of clearing.
// Includes the spawner source so its private spawn state can be driven directly.
// Run with scripts/test_wave_clear_flow.ps1.
#include "EnemySpawner.cpp"
#include <cmath>
#include <cstdio>

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

static constexpr float DT = 1.0f / 60.0f;

struct Run {
    MonsterManager mm;
    PlayerStats stats;
    Difficulty difficulty = Difficulty::EASY;
    float time = 0.0f;
    int wave = 1;
    float waveStart = 0.0f;

    void Step(float seconds) {
        for (float t = 0.0f; t < seconds; t += DT) {
            UpdateEnemySpawns(mm, stats, 1280, 720, DT, time, wave, time - waveStart, 0, difficulty);
            time += DT;
        }
    }
    Monster* Find(bool (*pred)(MobKind)) {
        for (Monster* m : mm.monsters) if (m->alive && pred(m->kind)) return m;
        return nullptr;
    }
    void Kill(Monster* m) { m->alive = false; m->scored = true; }
    void NextWave() { ++wave; waveStart = time; }
};

int main() {
    srand(7);
    ResetEnemySpawnState();
    Run run;

    // Wave 1: the elite waits out its delay, then enters.
    run.Step(ELITE_SPAWN_DELAY_SECONDS * 0.5f);
    EXPECT(run.Find(IsEliteMob) == nullptr, "elite enters only after the delay");
    run.Step(ELITE_SPAWN_DELAY_SECONDS);
    Monster* elite = run.Find(IsEliteMob);
    EXPECT(elite != nullptr, "wave 1 brings its elite");
    EXPECT(GetScheduledElite(1, run.difficulty).alive, "HUD sees the elite alive");

    // However long it lives, the wave does not end on its own.
    run.Step(200.0f);
    EXPECT(!ConsumeWaveClear().cleared, "no clear while the elite lives");
    const float fight = WaveFightSeconds(1, run.time);
    EXPECT(fight > 200.0f, "fight clock runs from the elite's entry (%.1f)", fight);
    int elites = 0;
    for (Monster* m : run.mm.monsters) elites += m->alive && IsEliteMob(m->kind);
    EXPECT(elites == 1, "exactly one elite on wave 1 (%d)", elites);

    // It falls: one clear, with the kill time, handed over once.
    if (elite) run.Kill(elite);
    run.Step(DT);
    WaveClearInfo clear = ConsumeWaveClear();
    EXPECT(clear.cleared && clear.wave == 1 && !clear.boss, "elite fall clears wave 1");
    EXPECT(clear.killSeconds > 200.0f && clear.killSeconds < 202.0f, "kill time %.2f", clear.killSeconds);
    EXPECT(!ConsumeWaveClear().cleared, "the clear is handed over once");
    run.mm.Clear();

    // Wave 2 (elite), quick kill.
    run.NextWave();
    run.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
    elite = run.Find(IsEliteMob);
    EXPECT(elite != nullptr, "wave 2 brings its elite");
    run.Step(5.0f);
    if (elite) run.Kill(elite);
    run.Step(DT);
    clear = ConsumeWaveClear();
    EXPECT(clear.cleared && clear.wave == 2 && clear.killSeconds < 6.0f, "quick clear (%.2f)", clear.killSeconds);
    EXPECT(WaveRewardTierFor(clear.killSeconds, clear.boss) == WaveRewardTier::Legendary, "quick kill earns legendary");
    run.mm.Clear();

    // Wave 3 (EASY's mid-run boss): no elite, the boss's fall clears it.
    run.NextWave();
    EXPECT(IsBossWave(3, run.difficulty) && !IsFinalBossWave(3, run.difficulty), "EASY 3 is a mid boss wave");
    run.Step(1.0f);
    EXPECT(run.Find(IsEliteMob) == nullptr, "no elite on a boss wave");
    Monster* boss = run.Find(IsBossMob);
    EXPECT(boss != nullptr && boss->kind == g_EnemySpawnState.midBoss, "mid boss enters");
    EXPECT(WaveFightIsBoss(3), "boss fight flagged");
    run.Step(30.0f);
    EXPECT(!ConsumeWaveClear().cleared, "no clear while the boss lives");
    if (boss) run.Kill(boss);
    run.Step(DT);
    clear = ConsumeWaveClear();
    EXPECT(clear.cleared && clear.wave == 3 && clear.boss, "boss fall clears wave 3");
    EXPECT(!NormaDefeated(), "a mid-run boss does not win the run");
    run.mm.Clear();

    // Waves 4-5: elites; debug skip clears like a fall.
    for (int w = 4; w <= 5; ++w) {
        run.NextWave();
        run.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
        EXPECT(DebugClearWaveObjective(run.mm, run.time), "debug skip clears wave %d", w);
        clear = ConsumeWaveClear();
        EXPECT(clear.cleared && clear.wave == w, "skip clear on wave %d", w);
        run.mm.Clear();
    }

    // Wave 6: the final boss. Its fall wins, it is not a wave clear.
    run.NextWave();
    run.Step(1.0f);
    boss = run.Find(IsBossMob);
    EXPECT(boss != nullptr && boss->kind == FinalBossKind(), "final boss enters");
    EXPECT(!DebugClearWaveObjective(run.mm, run.time), "debug skip cannot clear the final boss");
    if (boss) run.Kill(boss);
    run.Step(DT);
    EXPECT(!ConsumeWaveClear().cleared, "final boss fall is not a wave clear");
    EXPECT(NormaDefeated(), "final boss fall wins");
    run.mm.Clear();

    // Wave-1 elites sit near 3,000 effective HP on NORMAL, and a later elite
    // wave scales by the HP ramp.
    for (int kind = 0; kind < kEliteKindCount; ++kind) {
        auto eliteHp = [&](int wave) {
            ResetEnemySpawnState();
            for (unsigned char& p : g_EnemySpawnState.elitePick) p = (unsigned char)kind;
            Run r;
            r.difficulty = Difficulty::NORMAL;
            r.wave = wave;
            r.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
            Monster* e = r.Find(IsEliteMob);
            float hp = e ? e->hp : 0.0f;
            if (e && e->kind == MobKind::REGULUS) hp += e->regulusShield.hp;
            if (e && e->kind == MobKind::ANTARES)   // both split generations
                hp *= 1.0f + (2.0f * Monster::ANTARES_CHILD_HP +
                              4.0f * Monster::ANTARES_CHILD_HP * Monster::ANTARES_CHILD_HP) /
                             Monster::ANTARES_ELITE_HP_MULT;
            r.mm.Clear();
            return hp;
        };
        const float first = eliteHp(1), later = eliteHp(3);
        std::printf("elite %d: wave 1 %.0f HP, wave 3 %.0f HP\n", kind, first, later);
        EXPECT(first > 2500.0f && first < 3400.0f, "elite %d wave-1 HP %.0f", kind, first);
        EXPECT(std::fabs(later / first - WaveHpRamp(3, Difficulty::NORMAL)) < 0.01f,
               "elite %d ramps by wave (%.0f -> %.0f)", kind, first, later);
    }

    // Antares: the wave waits for every split fragment.
    {
        ResetEnemySpawnState();
        for (unsigned char& p : g_EnemySpawnState.elitePick) p = 3;   // ANTARES
        Run r;
        r.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
        Monster* parent = r.Find(IsEliteMob);
        EXPECT(parent && parent->kind == MobKind::ANTARES, "antares enters");
        Monster* frag = new Monster(0.0f, 0.0f, parent ? parent->antaresHpMult : 1.0f);
        frag->MakeKind(MobKind::ANTARES);
        frag->antaresGen = 1;
        r.mm.monsters.push_back(frag);
        if (parent) r.Kill(parent);
        r.Step(DT);
        EXPECT(!ConsumeWaveClear().cleared, "fragment still standing: no clear");
        EXPECT(GetScheduledElite(1, r.difficulty).alive, "HUD still sees the elite");
        r.Kill(frag);
        r.Step(DT);
        EXPECT(ConsumeWaveClear().cleared, "last fragment down clears the wave");
        r.mm.Clear();
    }

    // Elite rolls: never the same elite on back-to-back waves.
    for (int seed = 1; seed < 50; ++seed) {
        srand(seed);
        ResetEnemySpawnState();
        for (int w = 2; w <= kMaxWaves; ++w)
            EXPECT(g_EnemySpawnState.elitePick[w] != g_EnemySpawnState.elitePick[w - 1],
                   "repeat elite seed %d wave %d", seed, w);
    }

    if (g_failures == 0) std::printf("wave clear flow: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
