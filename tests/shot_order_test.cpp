// 샷건 타격감 (Docs/Plans/2026-11-24_샷건_타격감.md): a fast shot's long step hits the nearest
// enemy first, a piercing shot takes the next ones along the same step, and the 샷건 card
// shapes near-hitscan tracer pellets. Runs the real collision step.
// Run with scripts/test_shot_order.ps1.
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
// Draw-side size of a mob (EntityDraw.cpp), read by the 석궁 mark; no renderer here.
float MobOutlineExtent(const Monster*) { return 20.0f; }
float MobOutlineExtent(const RangedMob*) { return 20.0f; }

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
static void Clear(MonsterManager& m) { for (Monster* x : m.monsters) delete x; m.monsters.clear(); }
static bool Hit(const Monster* m) { return m->hp < 99999.0f || !m->alive; }

int main() {
    int fail = 0;
    g_Stats = PlayerStats{};
    g_GameManager = GameManager{};
    passive::Reset();
    passive::InstallHooks();
    MonsterManager& manager = g_MonsterManager;
    float hp = 50.0f, scoreAccum = 0.0f; long long score = 0;

    // 1. One long step (a 샷건 pellet flies ~116 px a frame) over two enemies, the farM one first
    //    in the list: the near one is hit and the shot ends there.
    {
        Monster* farM = Add(manager, 600.0f, 300.0f);
        Monster* nearM = Add(manager, 300.0f, 300.0f);
        Bullet b(100.0f, 300.0f, 900.0f, 300.0f);
        b.prevX = 100.0f; b.prevY = 300.0f; b.x = 700.0f; b.y = 300.0f;
        std::vector<Bullet> shots = { b };
        CollisionSystem::Update(0.0f, 0.0f, manager, shots, hp, scoreAccum, score, g_Stats);
        std::printf("1: near hit %d, farM hit %d, shots %zu\n", (int)Hit(nearM), (int)Hit(farM), shots.size());
        if (!Hit(nearM)) { std::printf("FAIL: the near enemy was not hit\n"); ++fail; }
        if (Hit(farM)) { std::printf("FAIL: the farM enemy was hit through the near one\n"); ++fail; }
        if (!shots.empty()) { std::printf("FAIL: a plain shot flew on\n"); ++fail; }
        Clear(manager);
    }
    // 2. The same step with one pierce: both enemies, in one step.
    {
        Monster* farM = Add(manager, 600.0f, 300.0f);
        Monster* nearM = Add(manager, 300.0f, 300.0f);
        Bullet b(100.0f, 300.0f, 900.0f, 300.0f);
        b.pierceLeft = 1;
        b.prevX = 100.0f; b.prevY = 300.0f; b.x = 700.0f; b.y = 300.0f;
        std::vector<Bullet> shots = { b };
        CollisionSystem::Update(0.0f, 0.0f, manager, shots, hp, scoreAccum, score, g_Stats);
        std::printf("2: near hit %d, farM hit %d, shots %zu\n", (int)Hit(nearM), (int)Hit(farM), shots.size());
        if (!Hit(nearM) || !Hit(farM)) { std::printf("FAIL: a piercing shot skipped an enemy on its step\n"); ++fail; }
        if (!shots.empty()) { std::printf("FAIL: the shot should end at its second enemy (pierce 1)\n"); ++fail; }
        Clear(manager);
    }
    // 3. An enemy off the step is never hit.
    {
        Monster* side = Add(manager, 400.0f, 420.0f);
        Bullet b(100.0f, 300.0f, 900.0f, 300.0f);
        b.prevX = 100.0f; b.prevY = 300.0f; b.x = 700.0f; b.y = 300.0f;
        std::vector<Bullet> shots = { b };
        CollisionSystem::Update(0.0f, 0.0f, manager, shots, hp, scoreAccum, score, g_Stats);
        if (Hit(side) || shots.empty()) { std::printf("FAIL: an enemy 120 px off the step was hit\n"); ++fail; }
        Clear(manager);
    }
    // 4. The 샷건 card: 7 near-hitscan tracer pellets that cross 7○ in about 0.08 s.
    {
        const int sg = Find("SYN_F_SHOTGUN");
        if (sg < 0) { std::printf("FAIL: no SYN_F_SHOTGUN\n"); return 2; }
        passive::Acquire(sg);
        Bullet gun(100.0f, 300.0f, 900.0f, 300.0f);
        gun.speed = 1200.0f;
        std::vector<Bullet> volley;
        syn::Shape(gun, 100.0f, 300.0f, 900.0f, 300.0f, volley);
        const Bullet& p = volley.empty() ? gun : volley[0];
        const float cross = p.maxRange / std::max(1.0f, p.speed);
        std::printf("4: pellets %zu, speed %.0f, range %.0f, crosses in %.3f s, tracer %d\n", volley.size(), p.speed,
                    p.maxRange, cross, (int)((p.synFlags & synf::FrameShotgun) != 0));
        if (volley.size() != 7) { std::printf("FAIL: want 7 pellets\n"); ++fail; }
        if (!(p.synFlags & synf::FrameShotgun)) { std::printf("FAIL: pellet not marked for the tracer\n"); ++fail; }
        if (cross > 0.1f) { std::printf("FAIL: a pellet takes over 0.1 s to its range\n"); ++fail; }
        if (shotgun_feel::g_FlashT <= 0.0f || shotgun_feel::RecoilOffset() <= 0.0f) { std::printf("FAIL: no muzzle flash / recoil\n"); ++fail; }
    }
    std::printf(fail ? "shot order test FAILED\n" : "shot order test passed\n");
    return fail ? 1 : 0;
}
