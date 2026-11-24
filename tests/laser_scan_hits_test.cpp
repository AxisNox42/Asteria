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
    {   // 몹별 틱: each enemy takes the standing beam on its own timer
        laser_fx::Clear();
        laser_fx::g_Main.tickGap = 0.2f;
        MonsterManager manager;
        Line(manager, 2);   // A at x=200, B at x=270
        Monster* A = manager.monsters[0];
        Monster* B = manager.monsters[1];
        auto beam = [&](float x0, float x1) {
            Bullet seg(x0, 300.0f, x1, 300.0f);
            seg.prevX = x0; seg.prevY = 300.0f; seg.x = x1; seg.y = 300.0f;
            seg.piercing = true; seg.speed = 0.0f; seg.sizeScale = 1.3f;
            seg.synFlags |= synf::Hitscan | synf::FrameLaser; seg.hitscanLeft = 2; seg.mobTick = true;
            std::vector<Bullet> bullets = { seg };
            CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, stats);
        };
        beam(150.0f, 220.0f);                       // only A is in the beam
        assert(A->hp < 99999.0f && B->hp == 99999.0f);
        const float aAfter1 = A->hp;
        laser_fx::g_Main.clock = 0.10f;
        beam(150.0f, 330.0f);                       // B enters: hit at once; A's timer is not up
        assert(B->hp < 99999.0f);
        assert(A->hp == aAfter1);
        const float bAfter = B->hp;
        laser_fx::g_Main.clock = 0.21f;
        beam(150.0f, 220.0f);                       // A's own tick is up again; B's is not
        assert(A->hp < aAfter1);
        assert(B->hp == bAfter);
        std::printf("per-enemy beam tick: A hit at 0 and 0.21 s, B at 0.10 s\n");
        laser_fx::Clear();
    }
    std::printf("laser scan hits regression passed\n");
    return 0;
}
