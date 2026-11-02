#pragma once

class Monster;
class MonsterManager;
struct PlayerStats;
enum class Difficulty;
enum class MobKind;

void ResetEnemySpawnState();
// wave is the current wave (GameManager::currentWave); waveSeconds is how
// long it has run.
void UpdateEnemySpawns(MonsterManager& monsters, const PlayerStats& stats,
                       int screenWidth, int screenHeight,
                       float delta, float gameTime, int wave, float waveSeconds,
                       long long score, Difficulty difficulty);

// A wave's target (its elite, or a mid-run boss) fell. Waves end only this
// way; killSeconds runs from the moment the target entered.
struct WaveClearInfo {
    bool cleared = false;
    int wave = 0;
    float killSeconds = 0.0f;
    bool boss = false;        // a mid-run boss, not an elite
};
// Hands over the latest clear once (cleared = false when there is none).
WaveClearInfo ConsumeWaveClear();
// Seconds since `wave`'s target entered (0 before it has, or once it fell).
float WaveFightSeconds(int wave, float gameTime);
// `wave`'s target on the field is a boss (mid-run or final).
bool WaveFightIsBoss(int wave);
// Hand testing: removes the current wave's elite or mid-run boss (no score),
// so the wave clears as if it fell now. False when there is nothing to clear.
bool DebugClearWaveObjective(MonsterManager& monsters, float gameTime);

// The elite of an elite wave. Its kind is rolled when the run starts, so the
// wave HUD can name it before it enters.
struct ScheduledEliteInfo {
    bool scheduled = false;   // wave has a scheduled spawn at all
    MobKind kind{};
    bool spawned = false;     // entered this run
    bool alive = false;       // still on the field
};
ScheduledEliteInfo GetScheduledElite(int wave, Difficulty difficulty);

// The final wave's boss (NORMA or CAELUM, rolled per run) has entered and
// then fallen this run.
bool NormaDefeated();
// Which boss the final wave brings this run. Mid-run boss waves bring the
// other one.
MobKind FinalBossKind();
// Hand testing: brings NORMA in now, as the boss wave would.
// hpMultiplier is the stat/difficulty share; the run's HP ramp at the boss
// wave is added here so a debug boss matches the real one.
void DebugSpawnNorma(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                     Difficulty difficulty);
// Hand testing: brings CAELUM (the mid-run boss) in now.
void DebugSpawnCaelum(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                      Difficulty difficulty);
