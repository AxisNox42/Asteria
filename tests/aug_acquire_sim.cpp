// Offline augment-acquisition simulation (balance aid, not a regression test).
// Plays the wave-clear reward flow of main.cpp's ProcessWaveTransitions for
// every difficulty, many runs each, through the real card draw
// (GameManager::PickRewardChoices) and the real acquire rules
// (passive::CanOffer / passive::Acquire), and prints how often each augment
// is offered and taken.
//
// Assumptions (the sim has no combat):
//   * every wave from 1 to final-1 is cleared and pays one augment reward;
//   * the clear time is uniform over [0, 1.25 x the slowest reward band], so
//     all reward tiers come up in proportion to the band widths;
//   * the player takes one of the offered cards at random (a no-preference
//     picker), so "taken" shows what the draw puts in front of players;
//   * level-up stat cards are not simulated (they never appear on wave clears).
// Run with scripts/sim_aug_acquire.ps1 [-Runs 1000]. Output: a text table on
// stdout and a CSV per difficulty beside the exe.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "PlayerStats.h"
#include "PassiveAugments.h"
#include "ActiveSkills.h"
#include "Settings.h"
#include "WaveSystem.h"
#include "MonsterManager.h"
#include "Bullet.h"
#include "PlayerRuntime.h"
#include "Audio.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

PlayerStats g_Stats;
GameManager g_GameManager;
// Globals the linked game sources reach for; the sim only reads card rules.
MonsterManager g_MonsterManager;
std::vector<Bullet> g_Bullets;
PlayerRuntimeState g_PlayerRuntime;
namespace Audio { void Play(Cue) {} }
int g_CurrentWeapon = 0;
GameState g_SettingsReturnTo = GameState::MAIN_MENU;
int g_HoveredAug = -1, g_AugExitSlot = -1;
float g_AugExitT = 0.0f;
bool TryEscNavigateBack() { return false; }

namespace {
struct Tally {
    long long offered[AUG_TOTAL] = {};
    long long taken[AUG_TOTAL] = {};
    long long picks = 0;
    long long rewards = 0;
    long long offeredByWave[32][5] = {};   // wave -> rarity -> cards shown
    long long rewardsByWave[32] = {};
    long long withRareByWave[32] = {};     // rewards that showed a Rare or better
};
}

static void SimulateRun(Difficulty difficulty, std::mt19937& rng, Tally& tally) {
    g_Difficulty = difficulty;
    g_Stats = PlayerStats{};
    passive::Reset();
    active::Reset();
    g_GameManager = GameManager{};   // the draw and Acquire read the shared instance
    GameManager& m = g_GameManager;
    m.playerHP = m.maxHP = g_Stats.maxHP;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const int finalWave = FinalWaveCount(difficulty);
    for (int wave = 1; wave < finalWave; ++wave) {
        m.currentWave = wave;
        const bool boss = IsBossWave(wave, difficulty);
        const bool special = IsSpecialWave(wave, difficulty);
        const int quota = RegularWaveKillQuota(wave, difficulty);
        const WaveRewardTimes times = special ? WaveRewardTimesFor(boss)
                                              : RegularWaveRewardTimes(quota);
        const float seconds = unit(rng) * times.rare * 1.25f;
        m.rewardTierCap = WaveRewardTierCap(wave, difficulty);
        const WaveRewardTier earned = WaveRewardTierFor(seconds, times);
        m.rewardTier = WaveRewardLuckyBump(WaveRewardGuaranteedTier(earned, m.rewardTierCap), earned,
                                           unit(rng), passive::LuckChanceMult(), m.rewardPityCount);
        m.rewardSpeed = WaveRewardSpeed(seconds, times);
        m.QueueAugmentReward();
        if (!m.ActivateNextAugmentReward()) continue;   // nothing left to offer
        ++tally.rewards;
        bool rare = false;
        for (int i = 0; i < m.augChoiceCount; ++i) {
            ++tally.offered[m.augChoices[i]];
            const int r = (int)ALL_AUGS[m.augChoices[i]].rarity;
            if (wave < 32 && r >= 0 && r < 5) ++tally.offeredByWave[wave][r];
            rare = rare || r >= 1;
        }
        if (wave < 32) { ++tally.rewardsByWave[wave]; tally.withRareByWave[wave] += rare; }
        const int slot = (int)(rng() % (unsigned)m.augChoiceCount);
        const int pick = m.augChoices[slot];
        passive::Acquire(pick);
        ++tally.taken[pick];
        ++tally.picks;
        m.ConsumeActiveReward();
        m.augmentRewardActive = false;
    }
}

int main(int argc, char** argv) {
    int runs = 1000;
    if (argc > 1) runs = std::max(1, std::atoi(argv[1]));
    if (std::getenv("PROBE")) {
        passive::Reset(); active::Reset(); g_Stats = PlayerStats{};
        int n[5] = {}, tot[5] = {};
        for (int i = 0; i < AUG_TOTAL; ++i) { if (AugIsStatCard(ALL_AUGS[i])) continue; ++tot[(int)ALL_AUGS[i].rarity]; if (passive::CanOffer(i)) ++n[(int)ALL_AUGS[i].rarity]; }
        for (int r = 0; r < 5; ++r) std::printf("fresh run: rarity %d offerable %d / %d\n", r, n[r], tot[r]);
    }
    if (std::getenv("PROBE")) {
        std::srand(1u);
        g_Difficulty = Difficulty::NORMAL;
        for (int cap = 1; cap <= 2; ++cap) {
            long long cnt[5] = {};
            for (int k = 0; k < 3000; ++k) {
                passive::Reset(); active::Reset(); g_Stats = PlayerStats{}; g_GameManager = GameManager{};
                GameManager& m = g_GameManager;
                m.currentWave = 1; m.rewardTierCap = cap; m.rewardTier = 0; m.rewardSpeed = 0.5f;
                m.PickRewardChoices();
                for (int i = 0; i < m.augChoiceCount; ++i) ++cnt[(int)ALL_AUGS[m.augChoices[i]].rarity];
            }
            std::printf("probe wave1 cap=%d: C %lld R %lld E %lld L %lld M %lld\n", cap, cnt[0], cnt[1], cnt[2], cnt[3], cnt[4]);
        }
    }
    std::srand(12345u);
    std::mt19937 rng(12345u);
    const char* names[DIFFICULTY_COUNT] = { "EASY", "NORMAL", "HARD", "NIGHTMARE" };
    for (int d = 0; d < DIFFICULTY_COUNT; ++d) {
        Tally t;
        for (int r = 0; r < runs; ++r) SimulateRun((Difficulty)d, rng, t);
        std::printf("== %s: %d runs, final wave %d, %.2f rewards/run, %.2f picks/run\n", names[d], runs,
                    FinalWaveCount((Difficulty)d), (double)t.rewards / runs, (double)t.picks / runs);
        for (int w = 1; w < FinalWaveCount((Difficulty)d) && w < 32; ++w) {
            const long long n = t.rewardsByWave[w];
            if (!n) continue;
            std::printf("   wave %2d: shown/reward C %.2f R %.2f E %.2f L %.2f M %.2f | has Rare+ %.1f%%\n", w,
                        (double)t.offeredByWave[w][0] / n, (double)t.offeredByWave[w][1] / n,
                        (double)t.offeredByWave[w][2] / n, (double)t.offeredByWave[w][3] / n,
                        (double)t.offeredByWave[w][4] / n, 100.0 * (double)t.withRareByWave[w] / n);
        }
        char path[64];
        std::snprintf(path, sizeof(path), "aug_acquire_%s.csv", names[d]);
        if (FILE* f = std::fopen(path, "w")) {
            std::fprintf(f, "aug,rarity,kind,offered,taken,offered_per_run,taken_per_run,taken_share_pct\n");
            for (int i = 0; i < AUG_TOTAL; ++i) {
                const AugDef& a = ALL_AUGS[i];
                if (AugIsStatCard(a)) continue;
                std::fprintf(f, "%s,%s,%s,%lld,%lld,%.4f,%.4f,%.3f\n", a.name, AugRarityName(a.rarity),
                             AugIsActive(a) ? "active" : "passive", t.offered[i], t.taken[i],
                             (double)t.offered[i] / runs, (double)t.taken[i] / runs,
                             t.picks ? 100.0 * (double)t.taken[i] / (double)t.picks : 0.0);
            }
            std::fclose(f);
        }
    }
    return 0;
}
