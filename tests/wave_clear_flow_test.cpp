// Wave flow through the real spawner: regular waves bring regular mobs only
// and refill the field; an elite wave's elite enters after
// its short delay, the wave clears only when it falls (with the kill time),
// a mid-run boss wave clears on the boss's fall, and the final boss's fall
// wins instead of clearing.
// Includes the spawner source so its private spawn state can be driven directly.
// Run with scripts/test_wave_clear_flow.ps1.
#include "EnemySpawner.cpp"
#include <cmath>
#include <cstdio>
#include "audio_stub.h"   // game code now plays sounds; tests stay silent

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
    // One kill ends these waves: Quasar (Antares splits; tested on its own below).
    for (unsigned char& p : g_EnemySpawnState.elitePick) p = 0;
    Run run;

    // Waves 1-3 (regular): regular mobs only, no elite, no target clear (the
    // kill quota is counted by the wave flow in main.cpp), and the field is
    // refilled whenever it runs thin.
    for (int w = 1; w <= 3; ++w) {
        EXPECT(IsRegularWave(w, run.difficulty), "EASY %d is regular", w);
        run.Step(ELITE_SPAWN_DELAY_SECONDS + 1.0f);
        EXPECT(run.Find(IsEliteMob) == nullptr && run.Find(IsBossMob) == nullptr, "regular only on wave %d", w);
        int alive = 0;
        for (Monster* m : run.mm.monsters) alive += m->alive;
        EXPECT(alive >= kRegularWaveMinAlive, "wave %d field filled (%d)", w, alive);
        // Kill them all: the field is topped up again at once.
        for (Monster* m : run.mm.monsters) if (m->alive) run.Kill(m);
        run.Step(0.3f);
        alive = 0;
        for (Monster* m : run.mm.monsters) alive += m->alive;
        EXPECT(alive >= kRegularWaveMinAlive, "wave %d refilled after kills (%d)", w, alive);
        EXPECT(!ConsumeWaveClear().cleared, "no target clear on regular wave %d", w);
        run.mm.Clear();
        run.NextWave();
    }

    // Wave 4 (elite): the elite waits out its delay, then enters.
    run.Step(ELITE_SPAWN_DELAY_SECONDS * 0.5f);
    EXPECT(run.Find(IsEliteMob) == nullptr, "elite enters only after the delay");
    run.Step(ELITE_SPAWN_DELAY_SECONDS);
    Monster* elite = run.Find(IsEliteMob);
    EXPECT(elite != nullptr, "wave 4 brings its elite");
    EXPECT(GetScheduledElite(4, run.difficulty).alive, "HUD sees the elite alive");

    // However long it lives, the wave does not end on its own.
    run.Step(200.0f);
    EXPECT(!ConsumeWaveClear().cleared, "no clear while the elite lives");
    const float fight = WaveFightSeconds(4, run.time);
    EXPECT(fight > 200.0f, "fight clock runs from the elite's entry (%.1f)", fight);
    int elites = 0;
    for (Monster* m : run.mm.monsters) elites += m->alive && IsEliteMob(m->kind);
    EXPECT(elites == 1, "exactly one elite on wave 4 (%d)", elites);

    // It falls: one clear, with the kill time, handed over once.
    if (elite) run.Kill(elite);
    run.Step(DT);
    WaveClearInfo clear = ConsumeWaveClear();
    EXPECT(clear.cleared && clear.wave == 4 && !clear.boss, "elite fall clears wave 4");
    EXPECT(clear.killSeconds > 200.0f && clear.killSeconds < 202.0f, "kill time %.2f", clear.killSeconds);
    EXPECT(!ConsumeWaveClear().cleared, "the clear is handed over once");
    run.mm.Clear();

    // Waves 5-7 regular (nothing to skip-clear), wave 8 elite (skip = fall).
    for (int w = 5; w <= 8; ++w) {
        run.NextWave();
        run.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
        if (w < 8) {
            EXPECT(!DebugClearWaveObjective(run.mm, run.time), "nothing to skip-clear on regular %d", w);
        } else {
            EXPECT(DebugClearWaveObjective(run.mm, run.time), "debug skip clears wave %d", w);
            clear = ConsumeWaveClear();
            EXPECT(clear.cleared && clear.wave == w, "skip clear on wave %d", w);
        }
        run.mm.Clear();
    }

    // NIGHTMARE wave 12: the mid-run boss; its fall clears the wave.
    {
        MobKind midBoss = MobKind::CAELUM;
        {
            const EnemySpawnState saved = g_EnemySpawnState;
            ResetEnemySpawnState();
            Run r;
            r.difficulty = Difficulty::NIGHTMARE;
            r.wave = 12;
            midBoss = g_EnemySpawnState.midBoss;
            r.Step(1.0f);
            EXPECT(r.Find(IsEliteMob) == nullptr, "no elite on the mid boss wave");
            Monster* mid = r.Find(IsBossMob);
            EXPECT(mid != nullptr && mid->kind == midBoss, "mid boss enters");
            EXPECT(WaveFightIsBoss(12), "boss fight flagged");
            r.Step(30.0f);
            EXPECT(!ConsumeWaveClear().cleared, "no clear while the boss lives");
            if (mid) r.Kill(mid);
            r.Step(DT);
            clear = ConsumeWaveClear();
            EXPECT(clear.cleared && clear.wave == 12 && clear.boss, "boss fall clears wave 12");
            EXPECT(!NormaDefeated(), "a mid-run boss does not win the run");
            r.mm.Clear();
            g_EnemySpawnState = saved;   // back to the EASY run
        }
    }

    // Wave 9: the final boss. Its fall wins, it is not a wave clear.
    run.NextWave();
    run.Step(1.0f);
    Monster* boss = run.Find(IsBossMob);
    EXPECT(boss != nullptr && boss->kind == FinalBossKind(), "final boss enters");
    EXPECT(!DebugClearWaveObjective(run.mm, run.time), "debug skip cannot clear the final boss");
    if (boss) run.Kill(boss);
    run.Step(DT);
    EXPECT(!ConsumeWaveClear().cleared, "final boss fall is not a wave clear");
    EXPECT(NormaDefeated(), "final boss fall wins");
    run.mm.Clear();

    // The first elite (wave 4) sits near 9,000 effective HP on NORMAL, and the
    // next one (wave 8) scales by the HP ramp.
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
        const float first = eliteHp(4), later = eliteHp(8);
        std::printf("elite %d: wave 4 %.0f HP, wave 8 %.0f HP\n", kind, first, later);
        EXPECT(first > 7500.0f && first < 10200.0f, "elite %d first HP %.0f", kind, first);
        EXPECT(std::fabs(later / first - WaveHpRamp(8, Difficulty::NORMAL)) < 0.01f,
               "elite %d ramps by wave (%.0f -> %.0f)", kind, first, later);
    }

    // Antares: the wave waits for every split fragment, in the game's real
    // order: a body falls in one frame and splits in the next mob update, so
    // the spawner sees it dead before its fragments exist.
    {
        ResetEnemySpawnState();
        for (unsigned char& p : g_EnemySpawnState.elitePick) p = 3;   // ANTARES
        Run r;
        r.wave = 4;                                       // the first elite wave
        r.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
        Monster* parent = r.Find(IsEliteMob);
        EXPECT(parent && parent->kind == MobKind::ANTARES, "antares enters");
        auto killAllAntares = [&]() {
            for (Monster* m : r.mm.monsters)
                if (m->alive && m->kind == MobKind::ANTARES) r.Kill(m);
        };
        int expected = 1;
        for (int gen = 0; gen <= Monster::ANTARES_MAX_GEN; ++gen) {
            int alive = 0;
            for (Monster* m : r.mm.monsters) alive += m->alive && m->kind == MobKind::ANTARES;
            EXPECT(alive == expected, "gen %d bodies %d (want %d)", gen, alive, expected);
            killAllAntares();
            r.Step(DT);                                    // dead, not split yet
            if (gen < Monster::ANTARES_MAX_GEN) {
                EXPECT(!ConsumeWaveClear().cleared, "gen %d down but not split: no clear", gen);
                EXPECT(GetScheduledElite(4, r.difficulty).alive, "HUD still sees the elite (gen %d)", gen);
                r.mm.SpawnAntaresSplits();                 // the next mob update
                r.Step(DT);
                EXPECT(!ConsumeWaveClear().cleared, "fragments of gen %d standing: no clear", gen);
            }
            expected *= Monster::ANTARES_SPLIT_COUNT;
        }
        EXPECT(ConsumeWaveClear().cleared, "last fragment down clears the wave");
        r.mm.Clear();
    }

    // Regulus elite wave: the field is refilled like a regular wave.
    {
        ResetEnemySpawnState();
        for (unsigned char& p : g_EnemySpawnState.elitePick) p = 1;   // REGULUS
        Run r;
        r.wave = 4;
        r.Step(ELITE_SPAWN_DELAY_SECONDS + 0.1f);
        Monster* reg = r.Find(IsEliteMob);
        EXPECT(reg && reg->kind == MobKind::REGULUS, "regulus enters");
        for (int round = 0; round < 3; ++round) {
            for (Monster* m : r.mm.monsters)
                if (m->alive && !IsEliteMob(m->kind)) r.Kill(m);
            r.Step(0.3f);
            int alive = 0;
            for (Monster* m : r.mm.monsters) alive += m->alive && !IsEliteMob(m->kind);
            EXPECT(alive >= RegularWaveMinAlive(4, r.difficulty), "regulus wave refilled (%d)", alive);
        }
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
