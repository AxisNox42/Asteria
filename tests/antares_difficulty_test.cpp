// Antares per difficulty (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md):
// EASY softer numbers and one split; NORMAL and HARD the defaults;
// NIGHTMARE the friction spray (a wedge in front of a fast run) and
// clash rings where fragments meet hard. Run with scripts/test_antares_difficulty.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include "audio_stub.h"   // game code plays sounds; tests stay silent

static constexpr float DT = 1.0f / 120.0f;
using S = Monster::AntaresState;

struct DiffScope {
    Difficulty old;
    explicit DiffScope(Difficulty d) : old(g_Difficulty) { g_Difficulty = d; }
    ~DiffScope() { g_Difficulty = old; }
};

static void Tick(Monster& m, float seconds, float px = 5000.0f, float py = 5000.0f) {
    float hp = 1.0e9f;
    for (float t = 0.0f; t < seconds - 1e-5f; t += DT) m.Update(px, py, DT, hp);
}

static Monster MakeElite(int phase) {
    Monster a(400.0f, 300.0f);
    a.MakeKind(MobKind::ANTARES);
    a.antaresInArena = true;
    a.gatesPassed = phase - 1;
    return a;
}

static void CheckTable() {
    using namespace elite_diff;
    assert(!AntaresWake(kHard) && AntaresWake(kNightmare));
    assert(AntaresMaxGen(kEasy) == 1 && AntaresMaxGen(kNormal) == 2 && AntaresMaxGen(kNightmare) == 2);
    assert(AntaresTopSpeed(kEasy) == 700.0f && AntaresTopSpeed(kHard) == Monster::ANTARES_MAX_SPEED);
    // HARD keeps every constant: it is the reference.
    DiffScope d(Difficulty::HARD);
    assert(Monster::AntaresRestTime() == Monster::ANTARES_REST_TIME);
    assert(Monster::AntaresMaxGen() == Monster::ANTARES_MAX_GEN);
    std::printf("table ok\n");
}

static void CheckEasyNumbers() {
    DiffScope d(Difficulty::EASY);
    // Chase tops out at 700 and turns as wide there as HARD does at 900.
    Monster a = MakeElite(1);
    a.antaresState = S::CHASE; a.antaresMoveSpeed = Monster::ANTARES_MIN_SPEED;
    a.antaresHeadingX = 1.0f; a.antaresHeadingY = 0.0f;
    float top = 0.0f;
    for (float t = 0.0f; t < 8.0f; t += DT) {
        a.worldX = 0.0f; a.worldY = 0.0f;   // keep the player far ahead
        Tick(a, DT, 1.0e6f, 0.0f);
        top = std::max(top, a.antaresMoveSpeed);
    }
    assert(std::fabs(top - 700.0f) < 0.01f);
    assert(std::fabs(a.AntaresSpeedRatio() - 1.0f) < 0.001f);
    // Rest 1.0 s.
    Monster r = MakeElite(1);
    r.antaresState = S::REST; r.antaresStateTimer = 0.0f;
    Tick(r, 0.95f);
    assert(r.antaresState == S::REST);
    Tick(r, 0.10f);
    assert(r.antaresState == S::CHASE);
    // Fragments bouncing never pass the EASY top speed either.
    MonsterManager mm;
    Monster* c = new Monster(0.0f, 0.0f); c->MakeKind(MobKind::ANTARES);
    Monster* e = new Monster(60.0f, 0.0f); e->MakeKind(MobKind::ANTARES);
    for (Monster* m : { c, e }) { m->antaresInArena = true; m->antaresGen = 1; mm.monsters.push_back(m); }
    c->antaresHeadingX = 1.0f; c->antaresMoveSpeed = 700.0f;
    e->antaresHeadingX = -1.0f; e->antaresMoveSpeed = 700.0f;
    mm.ResolveAntaresCollisions();
    assert(c->antaresMoveSpeed <= 700.0f && e->antaresMoveSpeed <= 700.0f);
    std::printf("easy numbers ok\n");
}

static int CountAntares(const MonsterManager& mm, int gen) {
    int n = 0;
    for (const Monster* m : mm.monsters) n += m->kind == MobKind::ANTARES && m->antaresGen == gen;
    return n;
}

static void CheckSplitDepth(Difficulty diff, int expectGen2) {
    DiffScope d(diff);
    MonsterManager mm;
    Monster* a = new Monster(300.0f, 300.0f);
    a->MakeKind(MobKind::ANTARES);
    a->antaresInArena = true; a->antaresHeadingX = 1.0f;
    a->gatesPassed = phase_gate::kEliteGateCount;
    mm.monsters.push_back(a);
    std::vector<Bullet> bullets;
    float hp = 1.0e9f;
    a->ApplyDamage(a->hp);
    mm.UpdateAll(5000.0f, 5000.0f, DT, hp, bullets);
    assert(CountAntares(mm, 1) == Monster::ANTARES_SPLIT_COUNT);
    for (Monster* m : mm.monsters) if (m->antaresGen == 1) m->ApplyDamage(m->hp);
    mm.UpdateAll(5000.0f, 5000.0f, DT, hp, bullets);
    assert(CountAntares(mm, 2) == expectGen2);
    std::printf("split depth %d ok\n", (int)diff);
}

// The friction wedge of the elite body (R = 26 x 2.4 = 62.4): tip 80.4 px
// ahead, back edge 40.6 px behind, 132.4 px to each side there.
static void CheckFrictionZone() {
    using namespace antares_wake;
    const float R = 62.4f;
    assert(std::fabs(TipAhead(R) - 80.4f) < 0.01f && std::fabs(Width(R) - 132.4f) < 0.01f);
    assert(InFriction(-40.0f, 135.0f, 1.0f, 10.0f, R));    // back edge, near its corner
    assert(!InFriction(-40.0f, 150.0f, 1.0f, 10.0f, R));   // past it
    assert(InFriction(85.0f, 0.0f, 1.0f, 10.0f, R));       // at the tip
    assert(!InFriction(95.0f, 0.0f, 1.0f, 10.0f, R));      // ahead of it
    assert(!InFriction(-55.0f, 0.0f, 1.0f, 10.0f, R));     // behind the back edge
    assert(!InFriction(0.0f, 100.0f, 1.0f, 10.0f, R));     // beside the centre: narrower there
    assert(!InFriction(-40.0f, 135.0f, 0.5f, 10.0f, R));   // half open: half as wide
    assert(!InFriction(0.0f, 0.0f, 0.0f, 10.0f, R));       // closed
    std::printf("friction zone ok\n");
}

// A straight run past a player standing 120 px off its line (outside the
// 80 px ram reach): NIGHTMARE hurts once with the spray, HARD not at all; a
// slow crawl never opens it.
static float DashPast(Difficulty diff, float speed, int* sparks = nullptr) {
    DiffScope d(diff);
    Monster a = MakeElite(2);
    a.antaresHeadingX = 1.0f; a.antaresHeadingY = 0.0f;
    a.antaresState = S::CHASE; a.antaresMoveSpeed = speed;
    g_Sparks.clear();
    float hp = 100.0f;
    const float px = 700.0f, py = 420.0f;   // 300 px ahead, 120 px off the line
    for (float t = 0.0f; t < 0.6f; t += DT) {
        a.antaresHeadingX = 1.0f; a.antaresHeadingY = 0.0f;   // straight, no turning in
        a.antaresPrevFacing = 1.0f;                            // and no skid on the pass
        a.Update(px, py, DT, hp);
    }
    if (sparks) *sparks = (int)g_Sparks.size();
    return 100.0f - hp;
}

static void CheckFrictionHits() {
    int sparks = 0;
    const float nightmare = DashPast(Difficulty::NIGHTMARE, 900.0f, &sparks);
    assert(std::fabs(nightmare - antares_wake::kDamage * g_PlayerDmgMult) < 0.01f);
    assert(sparks > 0);
    int none = 0;
    assert(DashPast(Difficulty::HARD, 900.0f, &none) == 0.0f && none == 0);
    assert(DashPast(Difficulty::NIGHTMARE, 200.0f) == 0.0f);   // under 500: no spray
    // A fragment never sprays.
    DiffScope d(Difficulty::NIGHTMARE);
    Monster f = MakeElite(1);
    f.antaresGen = 1;
    f.antaresState = S::CHASE; f.antaresMoveSpeed = 900.0f; f.antaresHeadingX = 1.0f;
    Tick(f, 0.3f, 1.0e6f, 300.0f);
    assert(f.antaresFriction == 0.0f);
    // A body that slows down closes the spray.
    Monster a = MakeElite(1);
    a.antaresState = S::CHASE; a.antaresMoveSpeed = 900.0f; a.antaresHeadingX = 1.0f;
    Tick(a, 0.2f, 1.0e6f, 300.0f);
    assert(a.antaresFriction == 1.0f);
    a.antaresMoveSpeed = 200.0f;
    Tick(a, 0.25f, 1.0e6f, 300.0f);
    assert(a.antaresFriction == 0.0f);
    std::printf("friction hits ok\n");
}

// Fragments clashing hard (NIGHTMARE): a ring where they met, sized by the
// bigger one, at most kClashCap per family; soft touches and HARD make none.
static void CheckClash() {
    auto Clash = [](MonsterManager& mm, float x, int genA, int genB, float speed, int family) {
        Monster* a = new Monster(x, 0.0f); a->MakeKind(MobKind::ANTARES);
        Monster* b = new Monster(x + 30.0f, 0.0f); b->MakeKind(MobKind::ANTARES);
        a->antaresGen = genA; b->antaresGen = genB;
        a->sizeScale = std::pow(Monster::ANTARES_CHILD_SCALE, (float)genA);
        b->sizeScale = std::pow(Monster::ANTARES_CHILD_SCALE, (float)genB);
        for (Monster* m : { a, b }) { m->antaresInArena = true; m->antaresFamily = family; mm.monsters.push_back(m); }
        a->antaresHeadingX = 1.0f; a->antaresMoveSpeed = speed;
        b->antaresHeadingX = -1.0f; b->antaresMoveSpeed = speed;
        mm.ResolveAntaresCollisions();
    };
    {
        DiffScope d(Difficulty::HARD);
        antares_wake::Reset();
        MonsterManager mm;
        Clash(mm, 0.0f, 1, 1, 600.0f, 1);
        assert(antares_wake::g.rings.empty());
    }
    DiffScope d(Difficulty::NIGHTMARE);
    antares_wake::Reset();
    MonsterManager mm;
    Clash(mm, 0.0f, 1, 1, 100.0f, 1);            // closing 200: soft
    assert(antares_wake::g.rings.empty());
    Clash(mm, 1000.0f, 1, 2, 300.0f, 1);         // closing 600
    assert(antares_wake::g.rings.size() == 1);
    assert(std::fabs(antares_wake::g.rings[0].radius - 88.0f) < 0.01f);
    Clash(mm, 2000.0f, 2, 2, 300.0f, 1);
    assert(antares_wake::g.rings.size() == 2);
    assert(std::fabs(antares_wake::g.rings[1].radius - 70.4f) < 0.01f);
    Clash(mm, 3000.0f, 2, 2, 300.0f, 1);         // cap: still two
    assert(antares_wake::g.rings.size() == 2);
    Clash(mm, 4000.0f, 1, 1, 300.0f, 2);         // another family has its own cap
    assert(antares_wake::g.rings.size() == 3);
    // A clash ring hurts once, through the manager, as ANTARES_CLASH.
    antares_wake::Reset();
    MonsterManager mm2;
    Clash(mm2, 0.0f, 1, 1, 300.0f, 1);
    const float cx = antares_wake::g.rings[0].x;
    float far = 5000.0f;
    for (Monster* m : mm2.monsters) { m->worldX = far; m->worldY = 5000.0f; m->antaresMoveSpeed = 0.0f; far += 600.0f; }
    std::vector<Bullet> bullets;
    float hp = 100.0f;
    for (float t = 0.0f; t < 1.0f; t += DT) {
        for (Monster* m : mm2.monsters) m->singularityGrace = 1.0e9f;   // bodies never ram
        mm2.UpdateAll(cx + 30.0f, 0.0f, DT, hp, bullets);
    }
    assert(std::fabs(hp - (100.0f - antares_wake::kDamage * g_PlayerDmgMult)) < 0.01f);
    assert(antares_wake::g.rings.empty());
    std::printf("clash ok\n");
}

// The whole nightmare body chasing a far player: once the run passes 500 px/s
// the spray opens (logged as FRICTION).
static void CheckNightmareFight() {
    DiffScope d(Difficulty::NIGHTMARE);
    antares_wake::Reset();
    MonsterManager mm;
    Monster* a = new Monster(100.0f, 540.0f);
    a->MakeKind(MobKind::ANTARES);
    a->antaresInArena = true;
    a->hp = 1.0e9f;
    mm.monsters.push_back(a);
    std::vector<Bullet> bullets;
    float hp = 1.0e9f;
    bool sawSpray = false;
    for (float t = 0.0f; t < 6.0f; t += DT) {
        a->hp = 1.0e9f;
        mm.UpdateAll(1.0e5f, 540.0f, DT, hp, bullets);
        sawSpray = sawSpray || a->antaresFriction >= 1.0f;
    }
    assert(sawSpray);
    std::printf("nightmare fight ok\n");
}

// Nightmare scatter: a fast wall bounce or Antares clash throws a 120 deg fan
// of 7 fast and 12 slow shots along the way it bounces off; 0.3 s cooldown;
// none when slow or below NIGHTMARE.
static void CheckScatter() {
    using namespace antares_wake;
    auto WallHit = [](Monster& m, float speed) {
        m.worldX = 990.0f; m.worldY = 300.0f;
        m.antaresState = S::CHASE; m.antaresMoveSpeed = speed;
        m.antaresHeadingX = 1.0f; m.antaresHeadingY = 0.0f;
        m.ConfineAntares(0.0f, 0.0f, 1000.0f, 600.0f);
    };
    {
        DiffScope d(Difficulty::HARD);
        Reset();
        Monster a = MakeElite(1);
        WallHit(a, 900.0f);
        assert(a.antaresBounces == 1 && g.scatters.empty());
    }
    DiffScope d(Difficulty::NIGHTMARE);
    Reset();
    Monster slow = MakeElite(1);
    WallHit(slow, 300.0f);
    assert(slow.antaresBounces == 1 && g.scatters.empty());   // under 500: just a bounce
    Monster a = MakeElite(1);
    WallHit(a, 900.0f);
    assert(g.scatters.size() == 1 && g.scatters[0].dirX < -0.99f);
    std::vector<Bullet> bullets;
    MonsterManager::FireAntaresScatters(bullets);
    assert(g.scatters.empty() && bullets.size() == (size_t)(kScatterFast + kScatterSlow));
    int fast = 0, slowShots = 0;
    for (const Bullet& b : bullets) {
        assert(b.isEnemy && !b.homing && b.enemyDmg == kScatterDamage);
        // Within the fan: at most 60 deg (+ a little jitter) off straight back.
        assert(b.dirX <= -std::cos(kScatterArc * 0.5f + 0.04f));
        if (b.speed == kScatterFastSpeed) ++fast;
        else { assert(b.speed >= kScatterSlowMin && b.speed <= kScatterSlowMax); ++slowShots; }
    }
    assert(fast == kScatterFast && slowShots == kScatterSlow);
    // Cooldown: a second bounce at once throws nothing, after 0.3 s it does.
    WallHit(a, 900.0f);
    assert(g.scatters.empty());
    a.antaresScatterCd = 0.0f;
    WallHit(a, 900.0f);
    assert(g.scatters.size() == 1);
    // Two fragments clashing fast: a fan from each (and the clash ring).
    Reset();
    MonsterManager mm;
    Monster* c = new Monster(0.0f, 0.0f); c->MakeKind(MobKind::ANTARES);
    Monster* e = new Monster(30.0f, 0.0f); e->MakeKind(MobKind::ANTARES);
    for (Monster* m : { c, e }) {
        m->antaresInArena = true; m->antaresGen = 1; m->antaresFamily = 1;
        m->sizeScale = Monster::ANTARES_CHILD_SCALE;
        mm.monsters.push_back(m);
    }
    c->antaresHeadingX = 1.0f; c->antaresMoveSpeed = 600.0f;
    e->antaresHeadingX = -1.0f; e->antaresMoveSpeed = 600.0f;
    mm.ResolveAntaresCollisions();
    assert(g.scatters.size() == 2 && g.rings.size() == 1);
    assert(g.scatters[0].dirX < -0.99f && g.scatters[1].dirX > 0.99f);   // each along its own bounce
    std::printf("scatter ok\n");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // keep the output of a failing assert
    CheckTable();
    CheckEasyNumbers();
    CheckSplitDepth(Difficulty::EASY, 0);
    CheckSplitDepth(Difficulty::NORMAL, 4);
    CheckSplitDepth(Difficulty::NIGHTMARE, 4);
    CheckFrictionZone();
    CheckFrictionHits();
    CheckClash();
    CheckScatter();
    CheckNightmareFight();
    std::printf("antares difficulty regression passed\n");
    return 0;
}
