// A piercing shot damages each enemy once, however many fixed steps it spends
// inside that enemy's hit radius; a ricochet may come back to an enemy it hit
// before the bounce. Run with scripts/test_pierce_hit_once.ps1.
#include "CollisionSystem.h"
#include <cassert>
#include <cstdio>

namespace Audio { void PlaySfx(Sfx) {} }   // kill sound: no audio in tests
float g_BaseOrtho[16] = {};

static PlayerStats PierceStats() {
    PlayerStats stats;
    stats.critChance = 0;
    stats.pierce = true;
    stats.pierceChance = 100;
    stats.ricochetChance = 0;
    return stats;
}

static Monster* AddMob(MonsterManager& manager, float x, float y, float sizeScale) {
    Monster* mob = new Monster(x, y, 1.0f);
    mob->MakeKind(MobKind::ROTOR);
    mob->hp = 1.0e6f;
    mob->sizeScale = sizeScale;
    manager.monsters.push_back(mob);
    return mob;
}

// Fly one bullet along +x from fromX to toX in 20px steps (1200px/s at 60Hz),
// counting the steps that damaged each mob.
static void Fly(MonsterManager& manager, PlayerStats& stats, float fromX, float toX,
                float y, int* hits) {
    std::vector<Bullet> bullets{ Bullet(fromX, y, toX, y) };
    float hp = 100.0f, scoreAccum = 0.0f;
    long long score = 0;
    const size_t n = manager.monsters.size();
    std::vector<float> before(n);
    for (float x = fromX; x < toX && !bullets.empty(); x += 20.0f) {
        bullets[0].prevX = x;        bullets[0].prevY = y;
        bullets[0].x     = x + 20.0f; bullets[0].y    = y;
        for (size_t i = 0; i < n; ++i) before[i] = manager.monsters[i]->hp;
        CollisionSystem::Update(0.0f, 0.0f, manager, bullets, hp, scoreAccum, score, stats);
        for (size_t i = 0; i < n; ++i)
            if (manager.monsters[i]->hp < before[i]) ++hits[i];
    }
}

int main() {
    {   // Elite-sized mob: 72px hit diameter, 20px steps -> 4 steps inside it.
        MonsterManager manager;
        AddMob(manager, 500.0f, 300.0f, 2.4f);
        PlayerStats stats = PierceStats();
        int hits[1] = {};
        Fly(manager, stats, 300.0f, 700.0f, 300.0f, hits);
        std::printf("pierce through one large mob: %d hit(s) (expected 1)\n", hits[0]);
        assert(hits[0] == 1);
    }
    {   // Two overlapping mobs: each is hit once, the second is not skipped.
        MonsterManager manager;
        AddMob(manager, 500.0f, 300.0f, 2.4f);
        AddMob(manager, 530.0f, 300.0f, 2.4f);
        PlayerStats stats = PierceStats();
        int hits[2] = {};
        Fly(manager, stats, 300.0f, 700.0f, 300.0f, hits);
        std::printf("pierce through two overlapping mobs: %d, %d (expected 1, 1)\n",
                    hits[0], hits[1]);
        assert(hits[0] == 1 && hits[1] == 1);
    }
    {   // Memory is per bullet: a fresh shot hits the same mob again.
        MonsterManager manager;
        AddMob(manager, 500.0f, 300.0f, 1.0f);
        PlayerStats stats = PierceStats();
        int hits[1] = {};
        Fly(manager, stats, 300.0f, 700.0f, 300.0f, hits);
        Fly(manager, stats, 300.0f, 700.0f, 300.0f, hits);
        assert(hits[0] == 2);
    }
    {   // Ricochet: a bounce clears the memory down to the mob it bounced off,
        // so a later bounce may come back to mobs hit before it.
        Bullet b(0.0f, 0.0f, 1.0f, 0.0f);
        int a = 0, c = 0, d = 0;
        b.RememberHit(&a);
        b.RememberHit(&c);
        b.ResetHits(&d);
        assert(b.HasHit(&d) && !b.HasHit(&a) && !b.HasHit(&c));
    }
    std::printf("pierce hit-once regression: OK\n");
    return 0;
}
