#pragma once
// Run result commit, shared by the two ways a run ends in main.cpp:
// abandoning (AbandonCurrentRun) and dying (DYING -> GAMEOVER). Both had the
// same statements; moved here unchanged so REG-05 can drive them. The callers
// keep the run-once guards (abandon returns when already GAMEOVER; the death
// path commits once, on the DYING -> GAMEOVER transition).

#include "SaveSystem.h"

// Records the run (best score, totals, codex kills, coins — RecordRunResult
// saves). Creative and
// debug plays (callers pass !PlayRecordsProgress()) record nothing and earn
// no end-of-run coins; a debug play still saves the stardust it picked up.
// Returns the new-record flag (g_LastRunRecord).
inline bool CommitRunResult(bool creative, int difficultyIdx, long long score,
                            long long kills, int weaponIdx,
                            int wave = 0) {
    if (!creative) {
        return RecordRunResult(difficultyIdx, score, kills, weaponIdx, wave);
    }
    g_LastRunCoins = 0;
    // A debug play (not creative) still counts its codex kills.
    const bool keepCodexKills = g_PlayDebugTainted && !g_CreativeMode;
    bool codexChanged = false;
    for (int i = 0; i < CM_COUNT; ++i) {
        if (keepCodexKills && g_RunMobKillCounts[i] > 0) {
            AddSaveValue(g_MobKillCounts[i], g_RunMobKillCounts[i]);
            codexChanged = true;
        }
        g_RunMobKillCounts[i] = 0;
    }
    if (codexChanged || (PlayEarnsStardust() && g_RunStardust > 0)) SaveGame();
    return false;
}
