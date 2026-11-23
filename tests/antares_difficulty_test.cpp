// Antares per difficulty (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md):
// EASY softer numbers, one split and no lance; NORMAL no lance; HARD as it was;
// NIGHTMARE the wake (fast runs and dashes burst sideways 0.4 s later) and
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
    a.antaresBounds = glm::vec4(50.0f, 50.0f, 1870.0f, 1030.0f);
    a.antaresInArena = true;
    a.gatesPassed = phase - 1;
    return a;
}

// The state a fresh run starts in, after `runs` earlier runs.
static S PickAfterRest(Monster& a, int runs) {
    a.worldX = 400.0f; a.worldY = 300.0f;
    a.antaresRuns = runs;
    a.antaresState = S::REST;
    a.antaresStateTimer = Monster::AntaresRestTime();
    Tick(a, DT);
    return a.antaresState;
}

static void CheckTable() {
    using namespace elite_diff;
    assert(!AntaresLance(kEasy) && !AntaresLance(kNormal) && AntaresLance(kHard) && AntaresLance(kNightmare));
    assert(!AntaresWake(kHard) && AntaresWake(kNightmare));
    assert(AntaresMaxGen(kEasy) == 1 && AntaresMaxGen(kNormal) == 2 && AntaresMaxGen(kNightmare) == 2);
    assert(AntaresTopSpeed(kEasy) == 700.0f && AntaresTopSpeed(kHard) == Monster::ANTARES_MAX_SPEED);
    // HARD keeps every constant: it is the reference.
    DiffScope d(Difficulty::HARD);
    assert(Monster::AntaresGatherTime() == Monster::ANTARES_GATHER_TIME);
    assert(Monster::AntaresRadialTime() == Monster::ANTARES_RADIAL_TIME);
    assert(Monster::AntaresRestTime() == Monster::ANTARES_REST_TIME);
    assert(Monster::AntaresMaxGen() == Monster::ANTARES_MAX_GEN);
    std::printf("table ok\n");
}

// Phase 3: the lance only from HARD; below it the phase-2 turn goes on.
static void CheckLanceGate() {
    const Difficulty all[] = { Difficulty::EASY, Difficulty::NORMAL, Difficulty::HARD, Difficulty::NIGHTMARE };
    for (Difficulty diff : all) {
        DiffScope d(diff);
        Monster a = MakeElite(3);
        int aims = 0, gathers = 0, chases = 0;
        for (int runs = 0; runs < 6; ++runs) {
            const S s = PickAfterRest(a, runs);
            aims += s == S::AIM; gathers += s == S::GATHER; chases += s == S::CHASE;
        }
        const bool lance = (int)diff >= (int)Difficulty::HARD;
        assert(aims == (lance ? 2 : 0));
        assert(gathers == (lance ? 2 : 3) && chases == (lance ? 2 : 3));
        // Phase 2 is the same turn everywhere.
        Monster b = MakeElite(2);
        int g2 = 0;
        for (int runs = 0; runs < 4; ++runs) g2 += PickAfterRest(b, runs) == S::GATHER;
        assert(g2 == 2);
    }
    std::printf("lance gate ok\n");
}

static void CheckEasyNumbers() {
    DiffScope d(Difficulty::EASY);
    // Chase tops out at 700 and turns as wide there as HARD does at 900.
    Monster a = MakeElite(1);
    a.antaresBounds = glm::vec4(0.0f);
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
    // Gather tell 1.3 s, lines 0.65 s.
    Monster g = MakeElite(2);
    assert(PickAfterRest(g, 0) == S::GATHER);
    Tick(g, 1.25f);
    assert(g.antaresState == S::GATHER && g.antaresPathCount == 0);
    Tick(g, 0.10f);
    assert(g.antaresState == S::GATHER && g.antaresPathCount == 2);
    Tick(g, 0.55f);
    assert(g.antaresState == S::GATHER);
    Tick(g, 0.10f);
    assert(g.antaresState == S::DASH);
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

// Lays a straight top-speed chase run past a parked player.
static void CheckWakeLaid() {
    {
        DiffScope d(Difficulty::HARD);
        antares_wake::Reset();
        Monster a = MakeElite(1);
        a.antaresBounds = glm::vec4(0.0f);
        a.antaresState = S::CHASE; a.antaresMoveSpeed = 900.0f; a.antaresHeadingX = 1.0f;
        Tick(a, 0.3f, 1.0e6f, 300.0f);
        assert(antares_wake::g.nodes.empty());   // HARD: no wake
    }
    DiffScope d(Difficulty::NIGHTMARE);
    antares_wake::Reset();
    Monster a = MakeElite(1);
    a.antaresBounds = glm::vec4(0.0f);
    a.antaresState = S::CHASE; a.antaresMoveSpeed = 900.0f; a.antaresHeadingX = 1.0f;
    Tick(a, 0.3f, 1.0e6f, 300.0f);
    // 0.3 s at 900 px/s = 270 px: pieces of 40 px or a frame more, all one run.
    assert(antares_wake::g.nodes.size() >= 5 && antares_wake::g.nodes.size() <= 7);
    for (const auto& n : antares_wake::g.nodes) assert(n.run == antares_wake::g.nodes[0].run);
    // A slow crawl (under 500 px/s) lays nothing, and closes the run.
    a.antaresMoveSpeed = 200.0f;
    Tick(a, DT, 1.0e6f, 300.0f);
    assert(a.antaresWakeRun == 0);
    antares_wake::Reset();
    Tick(a, 0.5f, 1.0e6f, 300.0f);
    assert(antares_wake::g.nodes.empty() && a.antaresWakeRun == 0);
    // A fragment never lays a wake.
    Monster f = MakeElite(1);
    f.antaresGen = 1;
    f.antaresBounds = glm::vec4(0.0f);
    f.antaresState = S::CHASE; f.antaresMoveSpeed = 900.0f; f.antaresHeadingX = 1.0f;
    Tick(f, 0.3f, 1.0e6f, 300.0f);
    assert(antares_wake::g.nodes.empty());
    // A wall bounce on the way leaves a ring at the wall.
    antares_wake::Reset();
    Monster w = MakeElite(1);
    w.worldX = 900.0f; w.worldY = 300.0f;
    w.antaresState = S::CHASE; w.antaresMoveSpeed = 900.0f; w.antaresHeadingX = 1.0f;
    for (float t = 0.0f; t < 0.3f; t += DT) {
        Tick(w, DT, 1.0e6f, 300.0f);
        w.ConfineAntares(0.0f, 0.0f, 1000.0f, 600.0f);
    }
    assert(w.antaresBounces == 1 && antares_wake::g.rings.size() == 1);
    assert(!antares_wake::g.rings[0].clash && antares_wake::g.rings[0].radius == antares_wake::kWidth);
    std::printf("wake laid ok\n");
}

// Bursts kDelay after the pass, out to kWidth, kDamage once per run.
static void CheckWakeBurst() {
    using namespace antares_wake;
    Reset();
    const int run = NewRun();
    for (int i = 0; i < 10; ++i) AddNode(i * 40.0f, 0.0f, (i + 1) * 40.0f, 0.0f, run);
    // Before the burst nothing hurts, even on the line.
    TickOut o = Tick(kDelay - 0.01f, 100.0f, 0.0f, 10.0f);
    assert(o.wakeDamage == 0.0f && !o.wakeBurst);
    // The burst: first frame reports it; a player 60 px off the line is caught once.
    float dmg = 0.0f; bool burst = false;
    for (float t = 0.0f; t < 0.4f; t += DT) {
        o = Tick(DT, 200.0f, 60.0f, 10.0f);
        dmg += o.wakeDamage; burst = burst || o.wakeBurst;
    }
    assert(burst && dmg == kDamage);
    assert(g.nodes.empty());   // gone after kLife
    // 130 px off: outside the reach.
    Reset();
    const int run2 = NewRun();
    for (int i = 0; i < 10; ++i) AddNode(i * 40.0f, 0.0f, (i + 1) * 40.0f, 0.0f, run2);
    dmg = 0.0f;
    for (float t = 0.0f; t < 1.0f; t += DT) dmg += Tick(DT, 200.0f, 130.0f, 10.0f).wakeDamage;
    assert(dmg == 0.0f);
    // A run and its wall ring share the one hit.
    Reset();
    const int run3 = NewRun();
    AddNode(0.0f, 0.0f, 40.0f, 0.0f, run3);
    AddRing(40.0f, 0.0f, kWidth, run3);
    dmg = 0.0f;
    for (float t = 0.0f; t < 1.0f; t += DT) dmg += Tick(DT, 40.0f, 30.0f, 10.0f).wakeDamage;
    assert(dmg == kDamage);
    std::printf("wake burst ok\n");
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
    assert(antares_wake::g.rings.size() == 1 && antares_wake::g.rings[0].clash);
    assert(std::fabs(antares_wake::g.rings[0].radius - 88.0f) < 0.01f);
    Clash(mm, 2000.0f, 2, 2, 300.0f, 1);
    assert(antares_wake::g.rings.size() == 2);
    assert(std::fabs(antares_wake::g.rings[1].radius - 70.4f) < 0.01f);
    Clash(mm, 3000.0f, 2, 2, 300.0f, 1);         // cap: still two
    assert(antares_wake::g.rings.size() == 2);
    Clash(mm, 4000.0f, 1, 1, 300.0f, 2);         // another family has its own cap
    assert(antares_wake::g.rings.size() == 3);
    // A clash ring hurts once, through the manager, as ANTARES_CLASH; then a
    // new pair may clash again.
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

// The whole nightmare body against a player standing beside its burst line:
// the wake reaches the player through MonsterManager.
static void CheckNightmareFight() {
    DiffScope d(Difficulty::NIGHTMARE);
    antares_wake::Reset();
    MonsterManager mm;
    Monster* a = new Monster(200.0f, 500.0f);
    a->MakeKind(MobKind::ANTARES);
    a->antaresInArena = true;
    a->gatesPassed = 1;
    a->hp = 1.0e9f;
    mm.monsters.push_back(a);
    std::vector<Bullet> bullets;
    float hp = 1.0e9f;
    // Phase 2, a gather burst at a player 80 px off the dash line's end.
    a->antaresRuns = 0;
    a->antaresState = S::REST;
    a->antaresStateTimer = Monster::AntaresRestTime();
    bool sawWake = false;
    for (float t = 0.0f; t < 4.0f; t += DT) {
        a->hp = 1.0e9f;
        mm.UpdateAll(1500.0f, 500.0f, DT, hp, bullets);
        a->ConfineAntares(0.0f, 0.0f, 1920.0f, 1080.0f);
        sawWake = sawWake || !antares_wake::g.nodes.empty();
    }
    assert(sawWake);
    std::printf("nightmare fight ok\n");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // keep the output of a failing assert
    CheckTable();
    CheckLanceGate();
    CheckEasyNumbers();
    CheckSplitDepth(Difficulty::EASY, 0);
    CheckSplitDepth(Difficulty::NORMAL, 4);
    CheckSplitDepth(Difficulty::NIGHTMARE, 4);
    CheckWakeLaid();
    CheckWakeBurst();
    CheckClash();
    CheckNightmareFight();
    std::printf("antares difficulty regression passed\n");
    return 0;
}
