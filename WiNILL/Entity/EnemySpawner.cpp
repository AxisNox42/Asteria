#include "EnemySpawner.h"

#include "Monster.h"
#include "MonsterManager.h"
#include "EnemyProgression.h"
#include "BalanceTuning.h"
#include "EnemyRoster.h"
#include "PlayerStats.h"
#include "../Core/WaveSystem.h"
#include "../System/Settings.h"
#include <algorithm>
#include <cstdlib>

// What a wave's fight is against: its fall ends the wave (the final boss's
// fall wins the run instead).
enum class WaveObjective { None, Elite, MidBoss, FinalBoss };

struct EnemySpawnState {
    float mobTimer = 0.0f;
    float rangedTimer = 0.0f;
    int lastEliteWave = 0;
    int currentWave = 0;
    int spawnedThisWave = 0;
    // Index into kEliteKinds for each wave (by wave number), rolled once per
    // run so the HUD can mark it before the elite enters.
    unsigned char elitePick[kMaxWaves + 1] = {};
    const Monster* eliteMob = nullptr;   // the scheduled elite while alive
    MobKind eliteKind = MobKind::ROTOR;  // what the current wave's elite is
    int lastBossWave = 0;                // the boss wave that already got its boss
    // The run's bosses, rolled when it starts: the final one is NORMA or
    // CAELUM; every mid-run boss wave brings the other one.
    MobKind finalBoss = MobKind::NORMA;
    MobKind midBoss = MobKind::CAELUM;
    bool normaSpawned = false;           // the final boss entered this run
    bool normaAlive = false;             // any boss is on the field
    bool normaDefeated = false;          // the final boss fell: the run is won
    // The current wave's target, from the moment it entered.
    WaveObjective objective = WaveObjective::None;
    int objectiveWave = 0;
    float objectiveSpawnTime = 0.0f;
    // The target fell; ConsumeWaveClear hands this to the wave flow once.
    WaveClearInfo clear;
};

static EnemySpawnState g_EnemySpawnState;

static MobKind MobKindFor(RosterKind kind) {
    switch (kind) {
    case RosterKind::SWARM:    return MobKind::SWARM;
    case RosterKind::GIMBAL:   return MobKind::GIMBAL;
    case RosterKind::GRAVIS:   return MobKind::GRAVIS;
    case RosterKind::MAGNETAR: return MobKind::MAGNETAR;
    default:                   return MobKind::ROTOR;
    }
}

// Converts a freshly spawned ROTOR into this wave's pick from the roster
// (EnemyRoster.h holds each enemy's first wave per difficulty and its odds).
static void SpecializeRegularMob(Monster& mob, int wave, Difficulty difficulty) {
    const RosterKind pick = RosterPickRegular(wave, (int)difficulty, rand() % 100);
    if (pick != RosterKind::ROTOR)
        mob.MakeKind(MobKindFor(pick));
}

// Every elite wave, on every difficulty, brings one of these at random.
static constexpr MobKind kEliteKinds[] = {
    MobKind::QUASAR, MobKind::REGULUS, MobKind::GENESIS, MobKind::ANTARES
};
static constexpr int kEliteKindCount = (int)(sizeof(kEliteKinds) / sizeof(kEliteKinds[0]));

static MobKind ScheduledEliteKind(int wave) {
    const int slot = std::clamp(wave, 0, kMaxWaves);
    return kEliteKinds[g_EnemySpawnState.elitePick[slot] % kEliteKindCount];
}

static void BeginWaveObjective(WaveObjective objective, int wave, float gameTime) {
    g_EnemySpawnState.objective = objective;
    g_EnemySpawnState.objectiveWave = wave;
    g_EnemySpawnState.objectiveSpawnTime = gameTime;
}

// The wave's elite always enters (outside the wave's spawn budget): the wave
// cannot end without it.
static void SpawnScheduledElite(MonsterManager& monsters, int screenWidth,
                                int screenHeight, int wave, float waveSeconds,
                                float gameTime, Difficulty difficulty,
                                float hpMultiplier) {
    if (!IsEliteWaveNumber(wave, difficulty) ||
        waveSeconds < ELITE_SPAWN_DELAY_SECONDS ||
        g_EnemySpawnState.lastEliteWave == wave) return;

    const MobKind elite = ScheduledEliteKind(wave);

    const size_t before = monsters.monsters.size();
    // Natural spawns share the wave budget, but have no concurrent population cap.
    monsters.SpawnMob(screenWidth, screenHeight,
                      (int)before + 1, hpMultiplier);
    if (monsters.monsters.size() == before) return;
    Monster& mob = *monsters.monsters.back();
    mob.MakeKind(elite);
    if (elite == MobKind::QUASAR)
        mob.quasarReflections = Monster::QuasarReflectionsFor((int)difficulty);
    // From NIGHTMARE a Genesis' Swarm throws summon random regular enemies (no elites, no boss).
    if (elite == MobKind::GENESIS)
        mob.genesisRandomSummons = (difficulty == Difficulty::NIGHTMARE);
    if (elite == MobKind::GENESIS) {
        mob.spawnAnchorX = 160.0f +
            (float)(rand() % (screenWidth > 360 ? screenWidth - 320 : 1));
        mob.spawnAnchorY = 160.0f +
            (float)(rand() % (screenHeight > 360 ? screenHeight - 320 : 1));
    }
    g_EnemySpawnState.lastEliteWave = wave;
    g_EnemySpawnState.eliteMob = &mob;
    g_EnemySpawnState.eliteKind = elite;
    BeginWaveObjective(WaveObjective::Elite, wave, gameTime);
}

// Drops the elite handle once it is no longer a living member of the field.
// An Antares lives on in its split fragments: the handle moves to one of them
// until the last falls, so the wave waits for the whole elite.
static void TrackScheduledElite(const MonsterManager& monsters) {
    const Monster* elite = g_EnemySpawnState.eliteMob;
    if (!elite) return;
    for (const Monster* mob : monsters.monsters)
        if (mob == elite && mob->alive) return;
    g_EnemySpawnState.eliteMob = nullptr;
    if (g_EnemySpawnState.eliteKind != MobKind::ANTARES) return;
    for (const Monster* mob : monsters.monsters)
        if (mob->alive && mob->kind == MobKind::ANTARES) { g_EnemySpawnState.eliteMob = mob; return; }
}

// A boss enters once per boss wave, at its start, from above the middle of
// the screen. A mid-run boss's fall ends its wave; only the final wave's
// boss decides the run.
static void PlaceNormaBoss(MonsterManager& monsters, int screenWidth,
                           float hpMultiplier, bool finalBoss, MobKind kind, Difficulty difficulty) {
    // A mid-run boss still standing when the final boss arrives withdraws (no
    // reward): the last wave is the final boss's duel alone.
    if (finalBoss)
        for (Monster* mob : monsters.monsters)
            if (IsBossMob(mob->kind) && mob->alive) { mob->alive = false; mob->scored = true; }
    Monster* boss = new Monster((float)screenWidth * 0.5f, -160.0f,
                                hpMultiplier * BalanceFor((int)difficulty).bossHp);
    boss->MakeKind(kind);
    if (kind == MobKind::CAELUM) caelum::SetDifficulty((int)difficulty);
    monsters.monsters.push_back(boss);
    g_EnemySpawnState.normaAlive = true;
    if (finalBoss) g_EnemySpawnState.normaSpawned = true;
}

static void SpawnNormaBoss(MonsterManager& monsters, int screenWidth, int wave,
                           float gameTime, Difficulty difficulty, float hpMultiplier) {
    if (!IsBossWave(wave, difficulty) || g_EnemySpawnState.lastBossWave == wave) return;
    g_EnemySpawnState.lastBossWave = wave;
    const bool isFinal = IsFinalBossWave(wave, difficulty);
    PlaceNormaBoss(monsters, screenWidth, hpMultiplier, isFinal,
                   isFinal ? g_EnemySpawnState.finalBoss : g_EnemySpawnState.midBoss, difficulty);
    BeginWaveObjective(isFinal ? WaveObjective::FinalBoss : WaveObjective::MidBoss,
                       wave, gameTime);
}

void DebugSpawnNorma(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                     Difficulty difficulty) {
    if (g_EnemySpawnState.normaSpawned && !g_EnemySpawnState.normaDefeated) return;
    g_EnemySpawnState.normaDefeated = false;
    const float rampHp = EnemyWaveHpMultiplier(FinalWaveCount(difficulty), difficulty,
                                               BalanceFor((int)difficulty));
    // Hand testing: this NORMA is the run's final boss (its fall wins).
    g_EnemySpawnState.finalBoss = MobKind::NORMA;
    g_EnemySpawnState.midBoss = MobKind::CAELUM;
    PlaceNormaBoss(monsters, screenWidth, hpMultiplier * rampHp, true, MobKind::NORMA, difficulty);
}

void DebugSpawnCaelum(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                      Difficulty difficulty) {
    for (const Monster* mob : monsters.monsters)
        if (mob->kind == MobKind::CAELUM && mob->alive) return;
    // Scaled like the first mid-run boss wave, or the final one where the
    // schedule has no mid-run boss.
    const WaveSchedule& schedule = WaveScheduleFor(difficulty);
    int bossWave = FinalWaveCount(difficulty);
    for (int i = 0; i < schedule.bossCount; ++i)
        if (!IsFinalBossWave(schedule.bossWaves[i], difficulty)) { bossWave = schedule.bossWaves[i]; break; }
    const float rampHp = EnemyWaveHpMultiplier(bossWave, difficulty,
                                               BalanceFor((int)difficulty));
    PlaceNormaBoss(monsters, screenWidth, hpMultiplier * rampHp, false, MobKind::CAELUM, difficulty);
}

// Looked up by kind rather than by pointer: dead mobs are freed elsewhere.
static void TrackNormaBoss(const MonsterManager& monsters) {
    // normaAlive covers any boss so nothing spawns into its duel; only the
    // final boss's fall decides the run.
    bool alive = false, anyBoss = false;
    for (const Monster* mob : monsters.monsters) {
        if (!mob->alive || !IsBossMob(mob->kind)) continue;
        anyBoss = true;
        if (mob->kind == g_EnemySpawnState.finalBoss) alive = true;
    }
    g_EnemySpawnState.normaAlive = anyBoss;
    if (!g_EnemySpawnState.normaSpawned || g_EnemySpawnState.normaDefeated || alive) return;
    g_EnemySpawnState.normaDefeated = true;
}

// The current wave's target is gone: record the clear once (the final boss's
// fall is NormaDefeated's business, not a wave clear).
static void TrackWaveObjective(float gameTime) {
    EnemySpawnState& s = g_EnemySpawnState;
    if (s.objectiveWave != s.currentWave || s.clear.cleared) return;
    bool gone = false;
    if (s.objective == WaveObjective::Elite) gone = s.eliteMob == nullptr;
    else if (s.objective == WaveObjective::MidBoss) gone = !s.normaAlive;
    if (!gone) return;
    s.clear.cleared = true;
    s.clear.wave = s.objectiveWave;
    s.clear.killSeconds = std::max(0.0f, gameTime - s.objectiveSpawnTime);
    s.clear.boss = s.objective == WaveObjective::MidBoss;
    s.objective = WaveObjective::None;
    s.objectiveWave = 0;
}

WaveClearInfo ConsumeWaveClear() {
    const WaveClearInfo info = g_EnemySpawnState.clear;
    g_EnemySpawnState.clear = {};
    return info;
}

float WaveFightSeconds(int wave, float gameTime) {
    const EnemySpawnState& s = g_EnemySpawnState;
    if (s.objective == WaveObjective::None || s.objectiveWave != wave) return 0.0f;
    return std::max(0.0f, gameTime - s.objectiveSpawnTime);
}

bool WaveFightIsBoss(int wave) {
    const EnemySpawnState& s = g_EnemySpawnState;
    return s.objectiveWave == wave &&
           (s.objective == WaveObjective::MidBoss || s.objective == WaveObjective::FinalBoss);
}

bool DebugClearWaveObjective(MonsterManager& monsters, float gameTime) {
    EnemySpawnState& s = g_EnemySpawnState;
    if (s.objectiveWave != s.currentWave) return false;
    if (s.objective == WaveObjective::Elite) {
        // An Antares goes with all its fragments.
        for (Monster* mob : monsters.monsters)
            if (mob->alive && (mob == s.eliteMob ||
                               (s.eliteKind == MobKind::ANTARES && mob->kind == MobKind::ANTARES))) {
                mob->alive = false; mob->scored = true; mob->noBlast = true;
            }
        s.eliteMob = nullptr;
    } else if (s.objective == WaveObjective::MidBoss) {
        for (Monster* mob : monsters.monsters)
            if (IsBossMob(mob->kind) && mob->alive) { mob->alive = false; mob->scored = true; }
        s.normaAlive = false;
    } else {
        return false;
    }
    TrackWaveObjective(gameTime);
    return true;
}

void DebugHoldWave(int wave) {
    EnemySpawnState& s = g_EnemySpawnState;
    s.lastEliteWave = s.lastBossWave = wave;
    s.eliteMob = nullptr;
    s.objective = WaveObjective::None;
    s.objectiveWave = 0;
    s.clear = {};
}

bool NormaDefeated() { return g_EnemySpawnState.normaDefeated; }
MobKind FinalBossKind() { return g_EnemySpawnState.finalBoss; }

void ResetEnemySpawnState() {
    g_EnemySpawnState = {};
    // The final boss is NORMA or CAELUM at random; a mid-run boss is the other.
    if (rand() % 2) { g_EnemySpawnState.finalBoss = MobKind::CAELUM; g_EnemySpawnState.midBoss = MobKind::NORMA; }
    // Back-to-back elite waves never bring the same elite twice in a row.
    int previous = -1;
    for (unsigned char& pick : g_EnemySpawnState.elitePick) {
        int roll = rand() % kEliteKindCount;
        if (roll == previous)
            roll = (roll + 1 + rand() % (kEliteKindCount - 1)) % kEliteKindCount;
        pick = (unsigned char)roll;
        previous = roll;
    }
}

ScheduledEliteInfo GetScheduledElite(int wave, Difficulty difficulty) {
    ScheduledEliteInfo info;
    if (!IsEliteWaveNumber(wave, difficulty)) return info;
    info.scheduled = true;
    info.kind = ScheduledEliteKind(wave);
    info.spawned = g_EnemySpawnState.lastEliteWave == wave;
    info.alive = info.spawned && g_EnemySpawnState.eliteMob != nullptr;
    return info;
}

void UpdateEnemySpawns(MonsterManager& monsters, const PlayerStats& stats,
                       int screenWidth, int screenHeight,
                       float delta, float gameTime, int wave, float waveSeconds,
                       long long score, Difficulty difficulty) {
#if defined(_DEBUG)
    constexpr float kEnemySpawnIntervalScale = 0.45f;
#else
    constexpr float kEnemySpawnIntervalScale = 1.0f;
#endif
    const DifficultyParams difficultyParams = GetDifficultyParams(difficulty);
    const BalanceTuning& balance = BalanceFor((int)difficulty);
    if (g_EnemySpawnState.currentWave != wave) {
        g_EnemySpawnState.currentWave = wave;
        g_EnemySpawnState.spawnedThisWave = 0;
        g_EnemySpawnState.mobTimer = 0.0f;
        g_EnemySpawnState.rangedTimer = 0.0f;
    }
    const float minutePressure = waveSeconds < 60.0f ? 1.0f
        : waveSeconds < 120.0f ? 1.15f : 1.30f;
    const float spawnPressure = (1.0f + balance.spawnPressureWave * (wave - 1)) * minutePressure;
    // Balance table: the wave band scales the rate; the in-wave curve
    // (average 1) speeds the spawn clocks, so a wave's total stays the same.
    const float balanceRate = WaveBandMult(balance, wave);
    const float waveCurve = WaveCurveMult(balance, waveSeconds / WAVE_CURVE_SECONDS);
    const int baseWaveSpawnCap = static_cast<int>(
        (100 + 12 * (wave - 1)) * difficultyParams.waveSpawnCapMultiplier + 0.5f);
    const int waveSpawnCap = std::max(1, baseWaveSpawnCap + TrialRangedMaxBonus(score));
    const float hpMultiplier = difficultyParams.enemyHpMultiplier *
        EnemyWaveHpMultiplier(wave, difficulty, balance) * TrialEnemyHpMult(score);

    TrackScheduledElite(monsters);
    TrackNormaBoss(monsters);
    TrackWaveObjective(gameTime);
    SpawnNormaBoss(monsters, screenWidth, wave, gameTime, difficulty, hpMultiplier);
    SpawnScheduledElite(monsters, screenWidth, screenHeight, wave, waveSeconds,
                        gameTime, difficulty, hpMultiplier);

    // A boss wave is an all-out duel: nothing else spawns, whether or not
    // NORMA is still standing, so no mob (or its bullets) crowds the fight.
    if (g_EnemySpawnState.normaAlive || IsBossWave(wave, difficulty)) {
        g_EnemySpawnState.mobTimer = 0.0f;
        g_EnemySpawnState.rangedTimer = 0.0f;
        return;
    }

    // Regular mobs keep coming, thinned, around the wave's elite.
    const float bossFight = 1.0f;
    const float bossStretch = std::max(bossFight,
        SpecialWaveSpawnStretch(wave, difficulty, SpawnSource::Regular));
    const float rangedStretch = std::max(bossFight,
        SpecialWaveSpawnStretch(wave, difficulty, SpawnSource::Ranged));
    g_EnemySpawnState.mobTimer += delta * waveCurve;
    const float spawnInterval = std::max(0.05f,
        2.4f * balance.spawnInterval * kEnemySpawnIntervalScale *
        bossStretch / std::max(0.01f, spawnPressure * TrialSpawnRateMult(score) *
                               difficultyParams.spawnRateMultiplier * balanceRate));
    if (g_EnemySpawnState.spawnedThisWave < waveSpawnCap &&
        g_EnemySpawnState.mobTimer >= spawnInterval) {
        const size_t before = monsters.monsters.size();
        monsters.SpawnMob(screenWidth, screenHeight,
                          (int)before + 1, hpMultiplier);
        if (monsters.monsters.size() > before) {
            SpecializeRegularMob(*monsters.monsters.back(), wave, difficulty);
            ++g_EnemySpawnState.spawnedThisWave;
            g_EnemySpawnState.mobTimer -= spawnInterval;
        }
    }

    // Scope uses its own ranged spawn path, gated by its roster entry.
    const float scopeRate = ScopeRateMult(wave, (int)difficulty);
    if (!RosterUnlocked(RosterKind::SCOPE, wave, (int)difficulty) || scopeRate <= 0.0f) {
        g_EnemySpawnState.rangedTimer = 0.0f;
        return;
    }
    g_EnemySpawnState.rangedTimer += delta * waveCurve;
    float rangedInterval = 5.0f * TrialRangedIntervalMult(score);
    rangedInterval = std::max(1.0f, rangedInterval) /
        std::max(0.01f, spawnPressure * difficultyParams.spawnRateMultiplier *
                        scopeRate * balanceRate) *
        kEnemySpawnIntervalScale * 2.5f * rangedStretch *
        balance.scopeInterval * balance.spawnInterval;
    if (g_EnemySpawnState.spawnedThisWave < waveSpawnCap &&
        g_EnemySpawnState.rangedTimer >= rangedInterval) {
        const size_t before = monsters.rangedMobs.size();
        monsters.SpawnRangedMob(screenWidth, screenHeight,
                                difficultyParams.enemyHpMultiplier *
                                    EnemyWaveHpMultiplier(wave, difficulty, balance),
                                (int)before + 1);
        if (monsters.rangedMobs.size() > before) {
            ++g_EnemySpawnState.spawnedThisWave;
            g_EnemySpawnState.rangedTimer -= rangedInterval;
        }
    }
}
