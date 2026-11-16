// Boomerang Rounds (V_RETURN) and Relentless Hunter Rounds (V_HUNTER) set the
// same return flag, so once either is owned the other is never offered.
// Run with scripts/test_return_hunter_offer.ps1.
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "PlayerStats.h"
#include "PassiveAugments.h"
#include "ActiveSkills.h"
#include "Settings.h"
#include "MonsterManager.h"
#include "Bullet.h"
#include "PlayerRuntime.h"
#include "Audio.h"
#include <cstdio>
#include <vector>

PlayerStats g_Stats;
GameManager g_GameManager;
// Globals the linked game sources reach for.
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

static int g_failures = 0;
#define EXPECT(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); ++g_failures; } } while (0)

static int AugIndexOf(AugType t) {
    for (int i = 0; i < AUG_TOTAL; ++i) if (ALL_AUGS[i].type == t) return i;
    return -1;
}

static void Fresh() {
    g_Stats = PlayerStats{};
    passive::Reset();
    active::Reset();
    g_GameManager = GameManager{};
}

int main() {
    const int ret = AugIndexOf(AugType::SYN_V_RETURN);
    const int hunter = AugIndexOf(AugType::SYN_V_HUNTER);
    EXPECT(ret >= 0 && hunter >= 0, "both cards exist");

    Fresh();
    EXPECT(passive::CanOffer(ret) && passive::CanOffer(hunter), "both offered on a fresh run");

    passive::Acquire(ret);
    EXPECT(!passive::CanOffer(hunter), "Hunter not offered once Boomerang is owned");

    Fresh();
    passive::Acquire(hunter);
    EXPECT(!passive::CanOffer(ret), "Boomerang not offered once Hunter is owned");

    if (g_failures) { std::printf("return/hunter offer: %d failure(s)\n", g_failures); return 1; }
    std::printf("return/hunter offer: never offered together\n");
    return 0;
}
