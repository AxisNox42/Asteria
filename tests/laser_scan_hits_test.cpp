// Laser beam segments (synf::Hitscan, LaserFx.h) hit every enemy lying on them in one
// collision step; a flying shot still ends its step at its first hit. One hit per step capped
// a beam at its step count of enemies once the beam had unlimited range.
// Run with scripts/test_laser_scan_hits.ps1.
#include "CollisionSystem.h"
#include <cassert>
#include <cstdio>

namespace Audio { void PlaySfx(Sfx) {} void Play(Cue) {} }   // no audio in tests
float g_BaseOrtho[16] = {};

static int CountHurt(const MonsterManager& m) {
    int n = 0;
    for (const Monster* mob : m.monsters) if (mob->hp < 99999.0f) ++n;
    return n;
}

static void Line(MonsterManager& manager, int count) {
    for (int i = 0; i < count; ++i) {
        Monster* mob = new Monster(200.0f + 70.0f * (float)i, 300.0f, 1.0f);
        mob->MakeKind(MobKind::SWARM);
        mob->hp = 99999.0f;
        manager.monsters.push_back(mob);
    }
}

int main() {
    PlayerStats stats;
    float hp = 50.0f, scoreAccum = 0.0f;
    long long score = 0;

    {   // a beam segment through 15 enemies: all hit in the one step
        MonsterManager manager;
        Line(manager, 15);
        Bullet seg(100.0f, 300.0f, 1500.0f, 300.0f);
        seg.prevX = 100.0f; seg.prevY = 300.0f; seg.x = 1500.0f; seg.y = 300.0f;
        seg.piercing = true; seg.speed = 0.0f; seg.sizeScale = 1.3f;
        seg.synFlags |= synf::Hitscan | synf::FrameLaser; seg.hitscanLeft = 9;
        std::vector<Bullet> bullets = { seg };
        CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, stats);
        std::printf("beam segment: %d of 15 enemies hit in one step\n", CountHurt(manager));
        assert(CountHurt(manager) == 15);
        assert(!bullets.empty());   // a piercing segment stays for its remaining steps
        // Another step: nothing is hit twice (damage numbers aside, hp must not drop again).
        std::vector<float> hpBefore;
        for (const Monster* mob : manager.monsters) hpBefore.push_back(mob->hp);
        CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, stats);
        for (size_t i = 0; i < manager.monsters.size(); ++i) assert(manager.monsters[i]->hp == hpBefore[i]);
    }
    {   // a flying piercing shot still hits one enemy per step
        MonsterManager manager;
        Line(manager, 15);
        Bullet fly(100.0f, 300.0f, 1500.0f, 300.0f);
        fly.prevX = 100.0f; fly.prevY = 300.0f; fly.x = 1500.0f; fly.y = 300.0f;
        fly.piercing = true; fly.sizeScale = 1.3f;
        std::vector<Bullet> bullets = { fly };
        CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, stats);
        std::printf("flying shot: %d enemy hit in one step\n", CountHurt(manager));
        assert(CountHurt(manager) == 1);
    }
    std::printf("laser scan hits regression passed\n");
    return 0;
}
