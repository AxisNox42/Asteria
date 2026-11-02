#pragma once

class Monster;
class MonsterManager;
struct PlayerStats;
enum class Difficulty;
enum class MobKind;

void ResetEnemySpawnState();
void UpdateEnemySpawns(MonsterManager& monsters, const PlayerStats& stats,
                       int screenWidth, int screenHeight,
                       float delta, float gameTime, long long score,
                       Difficulty difficulty);

// The scheduled enemy of a fifth wave. Its kind is rolled when the run
// starts, so the wave HUD can name it before it enters.
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
// Which boss the final wave brings this run. A run with a mid-run boss
// (NIGHTMARE) brings the other one there.
MobKind FinalBossKind();
// Hand testing: brings NORMA in now, as the boss wave would.
// hpMultiplier is the stat/difficulty share; the run's HP ramp at the boss
// wave is added here so a debug boss matches the real one.
void DebugSpawnNorma(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                     Difficulty difficulty);
// Hand testing: brings CAELUM (the mid-run boss) in now.
void DebugSpawnCaelum(MonsterManager& monsters, int screenWidth, float hpMultiplier,
                      Difficulty difficulty);
