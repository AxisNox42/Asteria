// REG-01 (D01 baseline): every kill entry point x every target, with the
// observable rewards. Run with scripts/test_reg01_kill_rewards.ps1.
//
// Entry points: bullet on a regular mob / on a Scope (CollisionSystem::Update)
// and common death collection (CollectMonsterKill / CollectScopeKill). Both
// paths use the shared reward owner in CollisionSystem.h.
//
// Expected values are literals worked out from the rules (default stats), not
// recomputed through the reward functions. Creative never counts codex kills
// (DEC-02).
#include "CollisionSystem.h"
#include <cassert>
#include <cmath>
#include <cstdio>

namespace Audio { void PlaySfx(Sfx) {} }   // kill sound: no audio in tests
float g_BaseOrtho[16] = {};

static constexpr float START_HP = 50.0f;

enum class Target { Rotor, Swarm, ThrownSwarm, Quasar, Scope };
enum class Means { Bullet, Collect };

static const char* Name(Target t) {
    switch (t) {
    case Target::Rotor: return "rotor";
    case Target::Swarm: return "swarm";
    case Target::ThrownSwarm: return "thrown swarm";
    case Target::Quasar: return "quasar";
    default: return "scope";
    }
}
static const char* Name(Means m) {
    return m == Means::Bullet ? "bullet" : "collect";
}

struct Snap {
    float hp = 0.0f, score = 0.0f;
    long long combo = 0, kills = 0, codex = 0;
    int dustValue = 0;
    XpAmount dustXp = 0;
};

struct Outcome {
    Snap d;                         // after - before
    bool pending = false;           // delayed drop held on the mob
    int pendingValue = 0;
    XpAmount pendingXp = 0;
    bool scored = false;
};

struct World {
    MonsterManager manager;
    PlayerStats stats;
    float hp = START_HP, scoreAccum = 0.0f;
    long long score = 0;
    KillRewardSink Sink() { return { hp, scoreAccum, score, stats, 0.0f, 0.0f }; }
};

static CodexMobId CodexOf(Target t) {
    switch (t) {
    case Target::Rotor: return CM_ROTOR;
    case Target::Swarm: case Target::ThrownSwarm: return CM_SWARM;
    case Target::Quasar: return CM_QUASAR;
    default: return CM_SCOPE;
    }
}

static Snap Take(World& w, Target t) {
    Snap s;
    s.hp = w.hp; s.score = w.scoreAccum;
    s.combo = g_Combo; s.kills = w.stats.killCount;
    s.codex = g_RunMobKillCounts[CodexOf(t)];
    for (const auto& p : g_StardustPickups) { s.dustValue += p.value; s.dustXp += p.xpValue; }
    return s;
}

static Snap Diff(const Snap& a, const Snap& b) {
    Snap d;
    d.hp = b.hp - a.hp; d.score = b.score - a.score;
    d.combo = b.combo - a.combo; d.kills = b.kills - a.kills;
    d.codex = b.codex - a.codex;
    d.dustValue = b.dustValue - a.dustValue; d.dustXp = b.dustXp - a.dustXp;
    return d;
}

static std::vector<Bullet> ShotThrough(float x, float y) {
    Bullet b(x - 12.0f, y, x + 12.0f, y);
    b.prevX = x - 12.0f; b.prevY = y;
    b.x = x + 12.0f;     b.y = y;
    return { b };
}

// Puts one nearly dead target in the world.
static Monster* AddMob(World& w, Target t) {
    Monster* m = new Monster(500.0f, 300.0f, 1.0f);
    m->MakeKind(t == Target::Rotor ? MobKind::ROTOR
              : t == Target::Quasar ? MobKind::QUASAR : MobKind::SWARM);
    m->genesisThrown = t == Target::ThrownSwarm;
    m->hp = 0.01f;
    w.manager.monsters.push_back(m);
    return m;
}
static RangedMob* AddScope(World& w) {
    RangedMob* r = new RangedMob(500.0f, 300.0f, 1920, 1080);
    r->hp = 0.01f;
    w.manager.rangedMobs.push_back(r);
    return r;
}

// Kills the target through one entry point, as the game does.
static void Kill(World& w, Target t, Means means, Monster* m, RangedMob* r) {
    KillRewardSink sink = w.Sink();
    if (means == Means::Bullet) {
        std::vector<Bullet> bullets = m ? ShotThrough(m->worldX, m->worldY)
                                        : ShotThrough(r->worldX, r->worldY);
        CollisionSystem::Update(0.0f, 0.0f, w.manager, bullets, w.hp, w.scoreAccum,
                                w.score, w.stats);
    } else {
        // Killed by something that pays nothing itself (burn, crush...).
        if (m) { m->ApplyDamage(1.0f); CollectMonsterKill(sink, *m); }
        else   { r->ApplyDamage(1.0f); CollectScopeKill(sink, *r); }
    }
    (void)t;
}

static Outcome Run(Target t, Means means, World& w) {
    g_StardustPickups.clear();
    Monster* m = t == Target::Scope ? nullptr : AddMob(w, t);
    RangedMob* r = t == Target::Scope ? AddScope(w) : nullptr;
    const Snap before = Take(w, t);
    Kill(w, t, means, m, r);
    Outcome o;
    o.d = Diff(before, Take(w, t));
    if (m) {
        assert(!m->alive);
        o.scored = m->scored;
        o.pending = m->dustPending; o.pendingValue = m->pendingDustValue; o.pendingXp = m->pendingDustXp;
        // Reprocessing the same death must pay nothing more (the collection
        // pass runs over every dead mob each frame until it explodes).
        KillRewardSink sink = w.Sink();
        const Snap again = Take(w, t);
        CollectMonsterKill(sink, *m);
        CollectMonsterKill(sink, *m);
        const Snap d = Diff(again, Take(w, t));
        assert(d.hp == 0.0f && d.score == 0.0f && d.combo == 0 && d.kills == 0 &&
               d.codex == 0 && d.dustValue == 0);
    } else {
        assert(!r->alive);
        o.scored = r->scored;
        KillRewardSink sink = w.Sink();
        const Snap again = Take(w, t);
        CollectScopeKill(sink, *r);
        const Snap d = Diff(again, Take(w, t));
        assert(d.hp == 0.0f && d.score == 0.0f && d.combo == 0 && d.kills == 0 && d.codex == 0);
    }
    return o;
}

static void Print(Target t, Means m, const Outcome& o) {
    std::printf("%-12s %-7s | score %+7.1f xp %+.3f dust %+2d pend %d/%d/%.3f | hp %+.3f "
                "kills %+lld | combo %+lld codex %+lld\n",
                Name(t), Name(m), o.d.score, XpToDouble(o.d.dustXp), o.d.dustValue, o.pending ? 1 : 0,
                o.pendingValue, XpToDouble(o.pendingXp), o.d.hp, o.d.kills,
                o.d.combo, o.d.codex);
    std::fflush(stdout);   // keep the table if an assert aborts
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// Expected rewards of a paying kill, per target (default stats).
struct Pay { float score; int dust; XpAmount xp; bool delayed; };
static Pay Expected(Target t) {
    switch (t) {
    case Target::Rotor:  return { 100.0f, 1, XpFromWhole(2), false };
    case Target::Swarm:  return { 2.4f, 1, XpFromWhole(1), false };     // Swarm: 20% score
    case Target::Quasar: return { 480.0f, 7, XpFromWhole(12), true };  // dropped on the blast
    default:             return { 300.0f, 3, XpFromWhole(8), false };   // Scope
    }
}

static void CheckMatrix() {
    std::printf("-- matrix (start HP %.0f)\n", START_HP);
    for (Target t : { Target::Rotor, Target::Swarm, Target::ThrownSwarm, Target::Quasar,
                      Target::Scope }) {
        for (Means m : { Means::Bullet, Means::Collect }) {
            World w;
            const Outcome o = Run(t, m, w);
            Print(t, m, o);
            assert(o.scored);
            assert(o.d.combo == 1 && o.d.codex == 1);
            assert(o.d.hp == 0.0f);                  // kills never heal
            if (t == Target::ThrownSwarm) {          // BEH-02: combo + codex only
                assert(o.d.score == 0.0f && o.d.dustValue == 0 && o.d.dustXp == 0 && !o.pending);
                assert(o.d.kills == 0);
                continue;
            }
            const Pay p = Expected(t);
            assert(Near(o.d.score, p.score));
            if (p.delayed) {
                assert(o.pending && o.pendingValue == p.dust && o.pendingXp == p.xp);
                assert(o.d.dustValue == 0 && o.d.dustXp == 0);
            } else {
                assert(!o.pending && o.d.dustValue == p.dust && o.d.dustXp == p.xp);
            }
            assert(o.d.kills == 1);
        }
    }
}

// DEC-02 (decided 2026-10-13): the codex never counts Creative kills.
static void CheckCreative() {
    std::printf("-- creative (DEC-02)\n");
    g_CreativeMode = true;
    for (Target t : { Target::Rotor, Target::Scope }) {
        for (Means m : { Means::Bullet, Means::Collect }) {
            World w;
            const Outcome o = Run(t, m, w);
            Print(t, m, o);
            assert(o.d.codex == 0 && o.d.kills == 1);
            assert(Near(o.d.score, t == Target::Scope ? 300.0f : 100.0f));
        }
    }
    g_CreativeMode = false;
}

static void CheckFractionalXp() {
    for (Means means : { Means::Bullet, Means::Collect }) {
        World swarm;
        swarm.stats.xpMult = 1.03f;
        assert(Run(Target::Swarm, means, swarm).d.dustXp == 1030);

        World rotor;
        rotor.stats.xpMult = 1.03f;
        assert(Run(Target::Rotor, means, rotor).d.dustXp == 2060);

        World scope;
        scope.stats.xpMult = 1.03f;
        assert(Run(Target::Scope, means, scope).d.dustXp == 8240);

        World quasar;
        quasar.stats.xpMult = 1.03f;
        assert(Run(Target::Quasar, means, quasar).pendingXp == 12360);
    }

    g_StardustPickups.clear();
    SpawnStardust(0, 0, 20, 0, 0, 1); // 0.001 XP over two pieces
    assert(g_StardustPickups.size() == 2);
    XpAmount total = 0;
    for (const auto& piece : g_StardustPickups) total += piece.xpValue;
    assert(total == 1);
    g_StardustPickups.clear();
    SpawnStardust(0, 0, 0, 0, 0, 1); // no currency, still carries XP
    assert(g_StardustPickups.size() == 1 && g_StardustPickups[0].xpValue == 1);
}

// The balance table's xpMult multiplies every kill's XP on top of the rules.
static void CheckBalanceXp() {
    ResetBalance();
    const float mult = BalanceFor((int)g_Difficulty).xpMult;
    for (Means means : { Means::Bullet, Means::Collect }) {
        World rotor;
        assert(Run(Target::Rotor, means, rotor).d.dustXp == XpFromReward(2.0 * mult));
        World scope;
        assert(Run(Target::Scope, means, scope).d.dustXp == XpFromReward(8.0 * mult));
    }
}

// An Antares pays nothing until its last fragment falls; then the whole
// family's share drops at once: XP 8 + 2x4 + 4x2 = 24, score 350 + 2x175 +
// 4x87.5 = 1050, stardust 7 bodies x 4 = 28. Each body still counts as a kill.
// XP is at the live balance table's multiplier (CheckBalanceXp reset it).
static void CheckAntaresFamily() {
    const double xpMult = BalanceFor((int)g_Difficulty).xpMult;
    World w;
    KillRewardSink sink = w.Sink();
    const Snap before = Take(w, Target::Rotor);
    auto body = [&](int gen, int family) {
        Monster* m = new Monster(500.0f, 300.0f, 1.0f);
        m->MakeKind(MobKind::ANTARES);
        if (family) m->antaresFamily = family;
        m->antaresGen = gen;
        m->hp = 0.01f;
        w.manager.monsters.push_back(m);
        return m;
    };
    Monster* parent = body(0, 0);
    const int family = parent->antaresFamily;
    std::vector<Monster*> rest = { body(1, family), body(1, family) };
    for (int i = 0; i < 4; ++i) rest.push_back(body(2, family));
    auto kill = [&](Monster* m) { m->ApplyDamage(1.0f); CollectMonsterKill(sink, *m); };

    kill(parent);
    for (size_t i = 0; i + 1 < rest.size(); ++i) kill(rest[i]);
    Snap held = Diff(before, Take(w, Target::Rotor));
    assert(held.dustXp == 0 && held.dustValue == 0 && Near(held.score, 0.0f));
    assert(held.kills == 6);                       // every body is still a kill

    kill(rest.back());
    Snap paid = Diff(before, Take(w, Target::Rotor));
    assert(paid.dustXp == XpFromReward(24.0 * xpMult));
    assert(Near(paid.score, 1050.0f));
    assert(paid.dustValue == 28);
    assert(paid.kills == 7);
    assert(g_AntaresFamilies.find(family) == g_AntaresFamilies.end());

    // A silent clear (noBlast) of the first body ends the family at once.
    Monster* lone = body(0, 0);
    lone->noBlast = true;
    const Snap beforeLone = Take(w, Target::Rotor);
    kill(lone);
    assert(Diff(beforeLone, Take(w, Target::Rotor)).dustXp == XpFromReward(8.0 * xpMult));
    std::printf("antares family reward: held until the last fragment, then 24 XP / 1050 score / 28 dust ok\n");
}

int main() {
    // The literals below are the rules at 1x; the balance table's XP
    // multiplier is checked on its own in CheckBalanceXp.
    for (int d = 0; d < kBalanceDifficulties; ++d) BalanceFor(d).xpMult = 1.0f;
    CheckMatrix();
    CheckCreative();
    CheckFractionalXp();
    CheckBalanceXp();
    CheckAntaresFamily();
    std::printf("REG-01 kill reward regression passed\n");
    return 0;
}
