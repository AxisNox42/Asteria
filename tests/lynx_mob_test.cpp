// Headless regression for the Lynx sniper: it is a regular mob on the roster, it
// tracks while aiming, freezes at the lock, and the hitscan ray hurts only a
// player still on the line. Run with scripts/test_lynx_mob.ps1.
#include "MonsterManager.h"
#include "EnemyRoster.h"
#include "audio_stub.h"   // game code now plays sounds; tests stay silent
#include <cassert>
#include <cmath>
#include <cstdio>

static constexpr float DT = 1.0f / 120.0f;

static Monster* MakeLynx() {
    auto* m = new Monster(0.0f, 0.0f);
    m->MakeKind(MobKind::LYNX);
    m->lynxInArena = true;
    m->lynxSnipeTimer = 0.0f;
    m->lynxHeading = 0.0f;
    return m;
}

static void Step(Monster* m, float px, float py, float& hp, std::vector<Bullet>& bullets) {
    m->worldX = m->worldY = 0.0f;
    m->Update(px, py, DT, hp, 1.0f, -1, -1, -1, -1, &bullets);
    m->worldX = m->worldY = 0.0f;
}

static void CheckRank() {
    assert(!IsEliteMob(MobKind::LYNX));
    assert(!IsBossMob(MobKind::LYNX));
    assert(RosterFirstWave(RosterKind::LYNX, 1) > 0);
    assert(RosterFirstWave(RosterKind::LYNX, 0) == 0);    // not on EASY
    // Each debut gets its wave to itself: no other first appearance, no elite or boss.
    for (int d = 1; d < kRosterDifficulties; ++d) {
        const int first = RosterFirstWave(RosterKind::LYNX, d);
        for (const RosterEntry& entry : kEnemyRoster)
            assert(entry.kind == RosterKind::LYNX || entry.firstWave[d] != first);
        assert(first % 4 != 0);
    }
    Monster m(0.0f, 0.0f);
    const float rotorHp = m.hp;
    m.MakeKind(MobKind::LYNX);
    assert(std::fabs(m.hp - rotorHp * lynx::kHpMult) < 0.01f);
}

static void CheckRayGeometry() {
    using namespace lynx;
    assert(RayHits(0.0f, 0.0f, 0.0f, 500.0f, 0.0f));
    const float reach = kPlayerHitRadius + kRayHalfWidth;
    assert(RayHits(0.0f, 0.0f, 0.0f, 500.0f, reach - 1.0f));
    assert(!RayHits(0.0f, 0.0f, 0.0f, 500.0f, reach + 1.0f));
    // A bigger player is a bigger target.
    assert(RayHits(0.0f, 0.0f, 0.0f, 500.0f, reach + 1.0f, 1.5f));
    assert(!RayHits(0.0f, 0.0f, 0.0f, -100.0f, 0.0f));    // behind the core
    assert(RayHits(0.0f, 0.0f, 0.0f, 50000.0f, 0.0f));   // no range limit
    assert(RayHits(0.0f, 0.0f, kPi * 0.5f, 0.0f, 400.0f));
}

// A player who stays put takes one ray per cycle, exactly at the end of the lock.
static void CheckStillPlayerIsHit() {
    Monster* m = MakeLynx();
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    float t = 0.0f, hitAt = -1.0f;
    const float cycle = lynx::kRestTime + lynx::kAimTime + lynx::kLockTime + lynx::kFlashTime;
    while (t < cycle) {
        const float before = hp;
        Step(m, 450.0f, 0.0f, hp, bullets);
        t += DT;
        if (hp < before) {
            assert(hitAt < 0.0f);                     // one hit only
            hitAt = t;
            assert(std::fabs(before - hp - lynx::kRayDamage) < 0.01f);
        }
    }
    assert(std::fabs(hitAt - (lynx::kRestTime + lynx::kAimTime + lynx::kLockTime)) < 0.05f);
    assert(bullets.empty());                          // a ray, not a projectile
    delete m;
}

// The line stops following at the lock, so stepping off it dodges the ray.
static void CheckLockFreezesAndDodges() {
    Monster* m = MakeLynx();
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    while (m->lynxSnipe != lynx::SnipeLock) Step(m, 450.0f, 0.0f, hp, bullets);
    const float locked = m->lynxHeading;
    while (m->lynxSnipe == lynx::SnipeLock) {
        Step(m, 450.0f, 120.0f, hp, bullets);
        assert(m->lynxHeading == locked);
    }
    assert(hp == 1000.0f);
    delete m;
}

// From aim to shot nothing moves it, stuns it or turns it after the lock.
static void CheckHeldThroughShot() {
    Monster* m = new Monster(100.0f, 200.0f);
    m->MakeKind(MobKind::LYNX);
    m->lynxInArena = true;
    m->lynxSnipeTimer = 0.0f;
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    while (m->lynxSnipe == lynx::SnipeRest) m->Update(100.0f, 800.0f, DT, hp, 1.0f, -1, -1, -1, -1, &bullets);
    const float x = m->worldX, y = m->worldY;
    while (m->lynxSnipe != lynx::SnipeRest) {
        m->worldX += 30.0f;                           // a shove from anything
        m->worldY -= 30.0f;
        m->TryHitStun(1.0f, 0.0f);
        assert(m->hitStunTimer == 0.0f);
        m->Update(100.0f, 800.0f, DT, hp, 1.0f, -1, -1, -1, -1, &bullets);
        if (m->lynxSnipe == lynx::SnipeRest) break;
        m->worldX += 30.0f;
        m->ConstrainLynxToViewport(0.0f, 0.0f, 1920.0f, 1080.0f, DT);
        assert(m->worldX == x && m->worldY == y);
    }
    delete m;
}

// The heading stays a plain angle however long it spins.
static void CheckHeadingStaysWrapped() {
    Monster* m = MakeLynx();
    std::vector<Bullet> bullets;
    float hp = 1e9f;
    for (int i = 0; i < 12000; ++i) {
        const float a = (float)i * 0.01f;
        Step(m, cosf(a) * 450.0f, sinf(a) * 450.0f, hp, bullets);
        assert(std::fabs(m->lynxHeading) <= lynx::kPi + 0.001f);
    }
    delete m;
}

// While aiming it tracks a moving player closely.
static void CheckAimTracks() {
    Monster* m = MakeLynx();
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    m->lynxSnipe = lynx::SnipeAim;
    for (int i = 0; i < 90; ++i) Step(m, 0.0f, 450.0f, hp, bullets);
    assert(std::fabs(m->lynxHeading - lynx::kPi * 0.5f) < 0.05f);
    delete m;
}

// It holds still while aiming.
static void CheckStandsToAim() {
    Monster* m = new Monster(0.0f, 0.0f);
    m->MakeKind(MobKind::LYNX);
    m->lynxInArena = true;
    m->lynxSnipe = lynx::SnipeAim;
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    for (int i = 0; i < 60; ++i) m->Update(900.0f, 0.0f, DT, hp, 1.0f, -1, -1, -1, -1, &bullets);
    assert(m->worldX == 0.0f && m->worldY == 0.0f);
    delete m;
}

// From the off-screen spawn line it must reach the view, and only then aim.
static void CheckEntersView(float playerX) {
    const float left = 0.0f, top = 0.0f, right = 1920.0f, bottom = 1080.0f;
    Monster* c = new Monster(right + 150.0f, 540.0f);
    c->MakeKind(MobKind::LYNX);
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    float t = 0.0f;
    while (!c->lynxInArena) {
        c->Update(playerX, 540.0f, DT, hp, 1.0f, -1, -1, -1, -1, &bullets);
        c->ConstrainLynxToViewport(left, top, right, bottom, DT);
        t += DT;
        assert(t < 4.0f);
        assert(c->lynxSnipe == lynx::SnipeRest);
    }
    assert(c->worldX <= right && hp == 1000.0f);
    assert(c->lynxSnipeTimer < 1.0f);                 // it rests after arriving, not on the way
    delete c;
}

int main() {
    CheckRank();
    CheckRayGeometry();
    CheckStillPlayerIsHit();
    CheckLockFreezesAndDodges();
    CheckHeldThroughShot();
    CheckHeadingStaysWrapped();
    CheckAimTracks();
    CheckStandsToAim();
    CheckEntersView(1800.0f);
    CheckEntersView(1450.0f);
    CheckEntersView(200.0f);
    std::puts("lynx mob ok");
    return 0;
}
