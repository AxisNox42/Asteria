// 표적 반사탄 (V_RICOCHET): a shot that ends on a hit turns toward another enemy instead.
// Takes the real card, builds the shot through syn::Shape, runs the real collision step.
// Run with scripts/test_ricochet.ps1.
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "PlayerStats.h"
#include "PassiveAugments.h"
#include "SynergyRuntime.h"
#include "ActiveSkills.h"
#include "Settings.h"
#include "WaveSystem.h"
#include "CollisionSystem.h"
#include "PlayerRuntime.h"
#include "Audio.h"
#include <cstdio>
#include <cstring>
#include <vector>

PlayerStats g_Stats;
GameManager g_GameManager;
MonsterManager g_MonsterManager;
std::vector<Bullet> g_Bullets;
PlayerRuntimeState g_PlayerRuntime;
namespace Audio { void Play(Cue) {} void PlaySfx(Sfx) {} void StartLoop(Cue) {} void StopLoop(Cue) {} }
bool keys[1024] = {};
int g_CurrentWeapon = 0;
GameState g_SettingsReturnTo = GameState::MAIN_MENU;
int g_HoveredAug = -1, g_AugExitSlot = -1;
float g_AugExitT = 0.0f;
bool TryEscNavigateBack() { return false; }
float g_BaseOrtho[16] = {};
float g_ShakeTime = 0.0f, g_ShakeMag = 0.0f;

static int Find(const char* name) {
    for (int i = 0; i < AUG_TOTAL; ++i) if (std::strcmp(ALL_AUGS[i].name, name) == 0) return i;
    return -1;
}
static Monster* Add(MonsterManager& m, float x, float y) {
    Monster* mob = new Monster(x, y, 1.0f);
    mob->MakeKind(MobKind::SWARM);
    mob->hp = 99999.0f;
    m.monsters.push_back(mob);
    return mob;
}

int main() {
    int fail = 0;
    g_Stats = PlayerStats{};
    g_GameManager = GameManager{};
    passive::Reset();
    passive::InstallHooks();
    const int ric = Find("SYN_V_RICOCHET");
    std::printf("card index %d\n", ric);
    if (ric < 0) return 2;
    passive::Acquire(ric);

    MonsterManager& manager = g_MonsterManager;
    Monster* A = Add(manager, 400.0f, 300.0f); if (std::getenv("RIC_KILL")) A->hp = 1.0f;
    Monster* B = Add(manager, 400.0f, 500.0f);   // off to the side, 200px away
    Bullet gun(100.0f, 300.0f, 900.0f, 300.0f);
    gun.speed = 900.0f; gun.sizeScale = 1.0f;
    std::vector<Bullet> volley;
    syn::Shape(gun, 100.0f, 300.0f, 900.0f, 300.0f, volley);
    std::printf("volley %zu, synRicochet %d\n", volley.size(), volley.empty() ? -1 : volley[0].synRicochet);
    if (volley.empty() || volley[0].synRicochet < 1) { std::printf("FAIL: shot carries no ricochet\n"); ++fail; }

    // Place the shot on enemy A and run one collision step.
    Bullet b = volley.empty() ? gun : volley[0];
    b.prevX = 380.0f; b.prevY = 300.0f; b.x = 400.0f; b.y = 300.0f;
    std::vector<Bullet> bullets = { b };
    float hp = 50.0f, scoreAccum = 0.0f; long long score = 0;
    CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, g_Stats);
    std::printf("after hit: A hp %.0f, shots alive %zu\n", A->hp, bullets.size());
    if (!(A->hp < 99999.0f) && A->alive) { std::printf("FAIL: A not hit\n"); ++fail; }
    if (bullets.empty()) { std::printf("FAIL: shot ended instead of turning\n"); ++fail; }
    else {
        const Bullet& r = bullets[0];
        std::printf("dir %.2f %.2f (want toward B: 0,1)\n", r.dirX, r.dirY);
        if (r.dirY < 0.9f) { std::printf("FAIL: not turned toward B\n"); ++fail; }
        // Fly on and hit B.
        for (int i = 0; i < 60 && B->hp == 99999.0f; ++i) {
            for (Bullet& x : bullets) x.Update(1.0f / 60.0f);
            CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, g_Stats);
            if (bullets.empty()) break;
        }
        if (!(B->hp < 99999.0f)) { std::printf("FAIL: B never hit\n"); ++fail; }
    }
    std::printf(fail ? "ricochet test FAILED\n" : "ricochet test passed\n");
    return fail ? 1 : 0;
}
