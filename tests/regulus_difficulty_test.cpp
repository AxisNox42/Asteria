// Regulus per difficulty (Docs/Plans/2026-11-23_레굴루스_난이도별_세부.md):
// EASY two buffs a volley, no cloak, 4 s apart, warning lines that do not hurt
// and no strike lasers; NORMAL one strike from phase 2; HARD adds the lead
// strike in phase 3; NIGHTMARE adds the royal wave (a fan from phase 2 that
// hurts the player once and shields the allies it passes, two bands in
// phase 3, a laned ring at each phase change).
// Run with scripts/test_regulus_difficulty.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include "audio_stub.h"   // game code plays sounds; tests stay silent

static constexpr float DT = 1.0f / 120.0f;

struct DiffScope {
    Difficulty old;
    explicit DiffScope(Difficulty d) : old(g_Difficulty) { g_Difficulty = d; }
    ~DiffScope() { g_Difficulty = old; }
};

static Monster* Add(MonsterManager& mm, MobKind kind, float x, float y) {
    auto* m = new Monster(x, y);
    m->MakeKind(kind);
    m->speed = 0.0f;
    m->contactDmg = 0.0f;   // only the lasers count
    mm.monsters.push_back(m);
    return m;
}

static Monster* AddRegulus(MonsterManager& mm, float x, float y, int phase) {
    Monster* r = Add(mm, MobKind::REGULUS, x, y);
    r->gatesPassed = phase - 1;
    r->regulusShotTimer = 0.0f;
    return r;
}

static void Run(MonsterManager& mm, float seconds, float px, float py, float& hp) {
    std::vector<Bullet> bullets;
    for (float t = 0.0f; t < seconds; t += DT) mm.UpdateAll(px, py, DT, hp, bullets);
}

// One frame: the Regulus (timer at 0, no free ally) sends its volley's strikes.
static void Queue(MonsterManager& mm, float px, float py) {
    std::vector<Bullet> bullets;
    float hp = 1.0e9f;
    mm.UpdateAll(px, py, DT, hp, bullets);
}

static void CheckTable() {
    using namespace elite_diff;
    assert(RegulusVolley(kEasy) == 2 && RegulusVolley(kNormal) == 3 && RegulusVolley(kNightmare) == 3);
    assert(!RegulusStealth(kEasy) && RegulusStealth(kNormal));
    assert(RegulusInterval(kEasy) == 4.0f && RegulusInterval(kHard) == Monster::REGULUS_SHOT_INTERVAL);
    assert(RegulusWarn(kEasy) == 0.8f && RegulusWarn(kHard) == Monster::REGULUS_WARN_TIME);
    assert(!RegulusLineHurts(kEasy) && RegulusLineHurts(kNormal));
    assert(!RegulusWave(kHard) && RegulusWave(kNightmare));
    assert(RegulusStrikes(kEasy, 3) == 0 && RegulusStrikes(kNormal, 1) == 0);
    assert(RegulusStrikes(kNormal, 2) == 1 && RegulusStrikes(kNormal, 3) == 1);
    assert(RegulusStrikes(kHard, 2) == 1 && RegulusStrikes(kHard, 3) == 2 && RegulusStrikes(kNightmare, 3) == 2);
    std::printf("table ok\n");
}

// EASY phase 2: two buffs, no cloak, lines shown but harmless, no strikes, 4 s apart.
static void CheckEasy() {
    DiffScope d(Difficulty::EASY);
    MonsterManager mm;
    Monster* r = AddRegulus(mm, 400.0f, 300.0f, 2);
    Monster* allies[6];
    for (int i = 0; i < 6; ++i) allies[i] = Add(mm, MobKind::ROTOR, 600.0f, 200.0f + 40.0f * i);
    float hp = 1000.0f;
    Run(mm, DT, 500.0f, 300.0f, hp);   // one volley queued
    assert(mm.regulusPending.size() == 2);
    for (const auto& p : mm.regulusPending) assert(p.harmless && (p.mob || p.ranged) && p.warn == 0.8f);
    assert(std::fabs(r->regulusShotTimer - 4.0f) < 0.02f);
    Run(mm, 0.9f, 500.0f, 300.0f, hp);
    assert(mm.regulusPending.empty() && hp == 1000.0f);   // stood on the lines: nothing
    // Many volleys: never a cloak.
    for (int v = 0; v < 6; ++v) {
        for (Monster* a : allies) { a->regulusShield = {}; a->regulusSpeedAge = -1.0f;
                                    a->regulusStealthAge = -1.0f; a->regulusReserved = false; }
        r->regulusShotTimer = 0.0f;
        Run(mm, 1.0f, 5000.0f, 5000.0f, hp);
        for (Monster* a : allies) assert(a->regulusStealthAge < 0.0f);
    }
    std::printf("easy ok\n");
}

// Strike lasers queued per volley: by difficulty and phase (a moving player
// gets the HARD lead shot).
static int StrikeCount(Difficulty diff, int phase) {
    DiffScope d(diff);
    MonsterManager mm;
    Monster* r = AddRegulus(mm, 0.0f, 0.0f, phase);
    mm.regulusPlayerVel = glm::vec2(300.0f, 0.0f);
    Queue(mm, 600.0f, 0.0f);
    return (int)mm.regulusPending.size();
}

static void CheckStrikes() {
    assert(StrikeCount(Difficulty::EASY, 3) == 0);
    assert(StrikeCount(Difficulty::NORMAL, 1) == 0);
    assert(StrikeCount(Difficulty::NORMAL, 2) == 1);
    assert(StrikeCount(Difficulty::NORMAL, 3) == 1);
    assert(StrikeCount(Difficulty::HARD, 2) == 1);
    assert(StrikeCount(Difficulty::HARD, 3) == 2);
    std::printf("strikes ok\n");
}

// The wave's shape: fan cover, the safe inside, swept crossing, ring lanes.
static void CheckWaveGeometry() {
    namespace rw = regulus_wave;
    const rw::Wave f = rw::Fan(0.0f, 0.0f, 500.0f, 0.0f, 2, nullptr);
    assert(f.bands == 1 && std::fabs(f.halfArc - 0.7853982f) < 1e-4f);
    assert(rw::Covers(f, 1.0f, 0.9f) && !rw::Covers(f, 1.0f, 1.1f));          // 42 deg in, 48 deg out
    assert(rw::BandRadius(f, 0, 0.0f) == rw::kInner && std::fabs(rw::BandRadius(f, 0, rw::kTravel) - rw::kRadius) < 0.1f);
    assert(rw::Crossed(f, 0, 0.0f, 0.4f, 400.0f, 0.0f));                       // (80 + 820 * 0.4/0.7 = 549)
    assert(!rw::Crossed(f, 0, 0.0f, 0.1f, 400.0f, 0.0f));                      // not there yet
    assert(rw::Crossed(f, 0, 0.0f, 1.0f / 15.0f, 140.0f, 0.0f) || rw::Crossed(f, 0, 1.0f / 15.0f, 0.2f, 140.0f, 0.0f));
    assert(!rw::Crossed(f, 0, 0.0f, rw::kTravel, 50.0f, 0.0f));                // inside kInner: safe
    assert(!rw::Crossed(f, 0, 0.0f, rw::kTravel, 0.0f, 400.0f));               // beside the fan: safe
    const rw::Wave f3 = rw::Fan(0.0f, 0.0f, 500.0f, 0.0f, 3, nullptr);
    assert(f3.bands == 2 && rw::Covers(f3, 1.0f, 1.6f) && !rw::Covers(f3, 1.0f, 1.8f));   // 58 deg in, 61 out
    assert(rw::BandRadius(f3, 1, rw::kSecondBand * 0.5f) < 0.0f);
    const rw::Wave r = rw::Ring(0.0f, 0.0f, 0.0f, nullptr);
    assert(!rw::Covers(r, 1.0f, 0.0f) && rw::Covers(r, 0.0f, 1.0f) && !rw::Covers(r, -0.5f, -0.866f));
    assert(r.warnTotal == rw::kRingWarn && r.travel == rw::kRingTravel);
    std::printf("wave geometry ok\n");
}

// A nightmare Regulus in phase `phase` whose buff volleys stay quiet.
static Monster* WaveRegulus(MonsterManager& mm, int phase) {
    Monster* r = AddRegulus(mm, 0.0f, 0.0f, phase);
    r->regulusShotTimer = 1.0e9f;   // no buff volley, no strike: only the wave
    return r;
}

static void CheckWaveFight() {
    namespace rw = regulus_wave;
    const float hit = rw::kDamage * g_PlayerDmgMult;
    const float until = rw::kFirstDelay + rw::kWarn + rw::kTravel + 0.2f;
    {   // HARD: no wave
        DiffScope d(Difficulty::HARD);
        MonsterManager mm;
        WaveRegulus(mm, 3);
        float hp = 100.0f;
        Run(mm, until, 400.0f, 0.0f, hp);
        assert(hp == 100.0f && mm.regulusWaves.empty());
    }
    DiffScope d(Difficulty::NIGHTMARE);
    {   // phase 1: no fan
        MonsterManager mm;
        WaveRegulus(mm, 1);
        float hp = 100.0f;
        Run(mm, until, 400.0f, 0.0f, hp);
        assert(hp == 100.0f && mm.regulusWaves.empty());
    }
    {   // phase 2: one hit; the ally in the fan gets a shield, the one behind does not
        MonsterManager mm;
        WaveRegulus(mm, 2);
        Monster* in = Add(mm, MobKind::ROTOR, 300.0f, 100.0f);
        Monster* behind = Add(mm, MobKind::ROTOR, -300.0f, 0.0f);
        const float inHp = in->hp;
        float hp = 100.0f;
        Run(mm, rw::kFirstDelay + 0.05f, 400.0f, 0.0f, hp);
        assert(mm.regulusWaves.size() == 1 && !mm.regulusWaves[0].ring && mm.regulusWaves[0].age < 0.0f);
        Run(mm, rw::kWarn + rw::kTravel + 0.2f, 400.0f, 0.0f, hp);
        assert(std::fabs(hp - (100.0f - hit)) < 0.01f);
        assert(in->regulusShield.acquiredOnce && std::fabs(in->regulusShield.maxHp - inHp * rw::kShieldShare) < 0.5f);
        assert(!behind->regulusShield.acquiredOnce);
        // The next wave fills the shield back up.
        in->regulusShield.hp = 1.0f;
        Run(mm, rw::kInterval, 400.0f, 0.0f, hp);
        assert(in->regulusShield.hp == in->regulusShield.maxHp);
        assert(std::fabs(hp - (100.0f - 2.0f * hit)) < 0.01f);
    }
    {   // dodges: step out of the fan after it is shown / stand inside
        MonsterManager mm;
        WaveRegulus(mm, 2);
        float hp = 100.0f;
        Run(mm, rw::kFirstDelay + 0.05f, 400.0f, 0.0f, hp);
        Run(mm, rw::kWarn + rw::kTravel + 0.2f, 0.0f, 400.0f, hp);
        assert(hp == 100.0f);
        MonsterManager mm2;
        WaveRegulus(mm2, 2);
        Run(mm2, until, 50.0f, 0.0f, hp);
        assert(hp == 100.0f && mm2.regulusWaves.empty());
    }
    {   // phase 3: two bands, two hits
        MonsterManager mm;
        WaveRegulus(mm, 3);
        float hp = 100.0f;
        Run(mm, until + rw::kSecondBand, 400.0f, 0.0f, hp);
        assert(std::fabs(hp - (100.0f - 2.0f * hit)) < 0.01f);
    }
    {   // a Regulus killed during the warning takes its wave with it
        MonsterManager mm;
        Monster* r = WaveRegulus(mm, 2);
        float hp = 100.0f;
        Run(mm, rw::kFirstDelay + 0.05f, 400.0f, 0.0f, hp);
        r->alive = false;
        Run(mm, rw::kWarn + rw::kTravel + 0.2f, 400.0f, 0.0f, hp);
        assert(hp == 100.0f);
    }
    std::printf("wave fight ok\n");
}

// A phase change sends the laned ring: a lane is safe, the rest is not.
static void CheckWaveRing() {
    namespace rw = regulus_wave;
    DiffScope d(Difficulty::NIGHTMARE);
    for (int lane = 1; lane >= 0; --lane) {
        MonsterManager mm;
        Monster* r = WaveRegulus(mm, 1);
        float hp = 100.0f;
        Run(mm, DT, 5000.0f, 5000.0f, hp);
        r->gatesPassed = 1;                       // phase 2 now
        Run(mm, DT, 5000.0f, 5000.0f, hp);
        assert(mm.regulusWaves.size() == 1 && mm.regulusWaves[0].ring);
        const float a = rw::GapAngle(mm.regulusWaves[0], 0) + (lane ? 0.0f : 3.14159265f / 3.0f);
        Run(mm, rw::kRingWarn + rw::kRingTravel + 0.2f, cosf(a) * 400.0f, sinf(a) * 400.0f, hp);
        if (lane) assert(hp == 100.0f);
        else assert(std::fabs(hp - (100.0f - rw::kDamage * g_PlayerDmgMult)) < 0.01f);
        assert(r->regulusWavePhase == 2);
    }
    std::printf("wave ring ok\n");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    CheckTable();
    CheckEasy();
    CheckStrikes();
    CheckWaveGeometry();
    CheckWaveFight();
    CheckWaveRing();
    std::printf("regulus difficulty regression passed\n");
    return 0;
}
