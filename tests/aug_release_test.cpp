// F1 debug console: taking a card back (passive::Release) undoes it.
// For every card in the build: take it, take it back, and the player's stats,
// the held count and whether it can be offered again match the start.
// Run with scripts/test_aug_release.ps1.

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
#include "AugmentSynergy.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
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
namespace Audio { void Play(Cue) {} void StartLoop(Cue) {} void StopLoop(Cue) {} }
bool keys[1024] = {};
int g_CurrentWeapon = 0;
GameState g_SettingsReturnTo = GameState::MAIN_MENU;
int g_HoveredAug = -1, g_AugExitSlot = -1;
float g_AugExitT = 0.0f;
bool TryEscNavigateBack() { return false; }

namespace {
int g_fail = 0;
struct Snap {
    float dmg, maxHP, fire, move, regen, critMult; int crit;
};
Snap Take() {
    return { g_Stats.baseDamage, g_Stats.maxHP, g_Stats.fireInterval, g_Stats.moveSpeedMult,
             g_Stats.regenPerSec, g_Stats.critMult, g_Stats.critChance };
}
bool Near(float a, float b) { return std::fabs(a - b) <= 1e-3f * std::max(1.0f, std::fabs(b)); }
bool Same(const Snap& a, const Snap& b) {
    return Near(a.dmg, b.dmg) && Near(a.maxHP, b.maxHP) && Near(a.fire, b.fire) &&
           Near(a.move, b.move) && Near(a.regen, b.regen) && Near(a.critMult, b.critMult) &&
           a.crit == b.crit;
}
void Fresh() {
    g_Stats = PlayerStats{};
    g_GameManager = GameManager{};
    passive::Reset();
    active::Reset();
    g_GameManager.maxHP = g_Stats.maxHP;
    g_GameManager.playerHP = g_Stats.maxHP;
}
void Fail(int aug, const char* what) {
    std::printf("FAIL %s: %s\n", ALL_AUGS[aug].name, what);
    ++g_fail;
}
}  // namespace

int main() {
    int checked = 0, held2 = 0;
    for (int aug = 0; aug < AUG_TOTAL; ++aug) {
        const AugDef& d = ALL_AUGS[aug];
        if (!AugInBuild(d)) continue;
        const bool undoable = AugIsSynergy(d.type) || AugIsActive(d) || AugIsStatCard(d);
        Fresh();
        const Snap before = Take();
        const bool offerBefore = passive::CanOffer(aug);
        passive::Acquire(aug);
        if (passive::HeldCount(aug) != 1) { Fail(aug, "held count after take != 1"); continue; }
        if (!undoable) {
            if (passive::Release(aug)) Fail(aug, "old passive card released");
            continue;
        }
        if (!passive::Release(aug)) { Fail(aug, "release refused"); continue; }
        if (passive::HeldCount(aug) != 0) Fail(aug, "still held after release");
        if (!Same(Take(), before)) Fail(aug, "stats not restored");
        if (passive::CanOffer(aug) != offerBefore) Fail(aug, "offer state differs");
        if (passive::AcquiredCount(aug) != 0) Fail(aug, "run record not cleared");
        if (passive::Release(aug)) Fail(aug, "second release accepted");
        ++checked;
        // Stackable cards: two copies, take one back, one left.
        if (passive::CanOffer(aug)) {
            passive::Acquire(aug);
            if (passive::CanOffer(aug)) {
                passive::Acquire(aug);
                if (passive::HeldCount(aug) == 2) {
                    passive::Release(aug);
                    if (passive::HeldCount(aug) != 1) Fail(aug, "stack release left != 1");
                    ++held2;
                }
            }
        }
    }
    // A frame switched OFF lets another frame in.
    Fresh();
    int f1 = -1, f2 = -1;
    for (int aug = 0; aug < AUG_TOTAL; ++aug) {
        const AugDef& d = ALL_AUGS[aug];
        if (!AugInBuild(d) || !AugIsSynergy(d.type)) continue;
        const int si = SynergyIndex(d.type);
        if (si < 0 || synergy::Table()[(size_t)si].role != synergy::Role::Frame) continue;
        if (!passive::CanOffer(aug)) continue;
        if (f1 < 0) f1 = aug; else if (f2 < 0) { f2 = aug; break; }
    }
    if (f1 >= 0 && f2 >= 0) {
        passive::Acquire(f1);
        if (passive::CanOffer(f2)) { std::printf("FAIL second frame offered while one is held\n"); ++g_fail; }
        passive::Release(f1);
        if (!passive::CanOffer(f2)) { std::printf("FAIL frame not offered after the first was released\n"); ++g_fail; }
    }
    std::printf("aug_release_test: %d cards checked, %d stack cases\n", checked, held2);
    if (g_fail) { std::printf("%d failure(s)\n", g_fail); return 1; }
    std::printf("all passed\n");
    return 0;
}
