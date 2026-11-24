// Phase gates (WiNILL/Entity/PhaseGate.h): a boss or elite cannot be burst
// past a gate line; it holds there for its pattern, then the next stretch of
// HP opens. Run with scripts/test_phase_gate.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include "audio_stub.h"   // game code plays sounds; tests stay silent

static constexpr float DT = 1.0f / 120.0f;

static void Tick(Monster& m, float seconds) {
    float hp = 1.0e9f;
    for (float t = 0.0f; t < seconds; t += DT)
        m.Update(5000.0f, 5000.0f, DT, hp);
}

// A hit with the Regulus shield out of the way (it soaks hits before the HP).
static float Hit(Monster& m, float dmg) {
    m.regulusShield.hp = 0.0f;
    return m.ApplyDamage(dmg);
}

static void CheckHelpers() {
    using namespace phase_gate;
    assert(std::fabs(FloorFor(kBossLines, kBossGateCount, 0, 1000.0f) - 750.0f) < 0.01f);
    assert(std::fabs(FloorFor(kBossLines, kBossGateCount, 1, 1000.0f) - 250.0f) < 0.01f);
    assert(FloorFor(kBossLines, kBossGateCount, 2, 1000.0f) == 0.0f);
    assert(std::fabs(FloorFor(kEliteLines, kEliteGateCount, 0, 900.0f) - 600.0f) < 0.01f);
    assert(std::fabs(FloorFor(kEliteLines, kEliteGateCount, 1, 900.0f) - 300.0f) < 0.01f);
    assert(ClampToFloor(500.0f, 800.0f, 750.0f) == 50.0f);
    assert(ClampToFloor(500.0f, 700.0f, 750.0f) == 0.0f);    // already under: nothing lands
    assert(ClampToFloor(500.0f, 800.0f, 0.0f) == 500.0f);    // no gate left
    assert(AtGate(750.0f, 750.0f) && !AtGate(751.0f, 750.0f) && !AtGate(10.0f, 0.0f));
    std::printf("helpers ok\n");
}

// An elite stops at 2/3 and 1/3, is untouchable for the hold, then held at
// the line for the show, and only then can be hit lower; after both gates a
// hit kills it.
static void CheckEliteGates(MobKind kind) {
    Monster m(400.0f, 300.0f);
    m.MakeKind(kind);
    assert(m.UsesEliteGates());
    const float max = m.eliteMaxHp;
    Hit(m, max * 10.0f);
    assert(m.alive && std::fabs(m.hp - max * (2.0f / 3.0f)) < 0.5f);
    Tick(m, DT * 2.0f);
    assert(m.gateArmed && m.gateHold > 0.0f);
    assert(Hit(m, 100.0f) == 0.0f);            // the hold: untouchable
    Tick(m, phase_gate::kEliteHoldSeconds + 0.05f);
    assert(m.gateHold <= 0.0f && m.gatesPassed == 0);
    Hit(m, max * 10.0f);                       // the show: held at the line
    assert(std::fabs(m.hp - max * (2.0f / 3.0f)) < 0.5f);
    Tick(m, phase_gate::kQuasarShowMaxSeconds + 0.1f);
    assert(m.gatesPassed == 1 && !m.gateArmed);
    Hit(m, max * 10.0f);
    assert(m.alive && std::fabs(m.hp - max / 3.0f) < 0.5f);
    Tick(m, phase_gate::kEliteHoldSeconds + phase_gate::kQuasarShowMaxSeconds + 0.2f);
    assert(m.gatesPassed == 2);
    Hit(m, max * 10.0f);
    assert(!m.alive);
    std::printf("elite gates ok (%s)\n", MobKindName(kind));
}

// The Quasar's gate winds up its beam at once and its show ends with the beam.
static void CheckQuasarGateBeam() {
    Monster q(400.0f, 300.0f);
    q.MakeKind(MobKind::QUASAR);
    q.quasarNextFall = true;          // the gate overrides a pending starfall
    q.ApplyDamage(q.eliteMaxHp * 10.0f);
    Tick(q, DT * 2.0f);
    assert(q.gateArmed && !q.quasarNextFall);
    bool beamed = false;
    for (float t = 0.0f; t < 4.0f && !beamed; t += DT) {
        float hp = 1.0e9f;
        q.Update(5000.0f, 5000.0f, DT, hp);
        beamed = q.quasarState == 3;
    }
    assert(beamed);                   // wind-up + lock, then the beam
    Tick(q, Monster::QUASAR_BEAM_TIME + 0.2f);
    assert(q.gatesPassed == 1);       // ended with the beam, before the time cap
    std::printf("quasar gate beam ok\n");
}

// Split pieces and summons are never gated.
static void CheckUngated() {
    Monster piece(0.0f, 0.0f);
    piece.MakeKind(MobKind::ANTARES);
    piece.antaresGen = 1;
    assert(!piece.UsesEliteGates());
    Monster rotor(0.0f, 0.0f);
    rotor.MakeKind(MobKind::ROTOR);
    assert(!rotor.UsesEliteGates());
    rotor.ApplyDamage(rotor.hp + 1.0f);
    assert(!rotor.alive);
    std::printf("ungated ok\n");
}

// OCTANS: HP stops at 75%, the gate's pattern (the wall) is queued and runs
// next, and the 25% line holds in phase 2 until the blue screen has run.
static void CheckNormaClamp() {
    Monster b(400.0f, 300.0f);
    b.MakeKind(MobKind::NORMA);
    const float max = b.normaMaxHp;
    b.ApplyDamage(max);
    assert(std::fabs(b.hp - max * 0.75f) < 0.5f);
    b.gatesPassed = 1;                 // past 75%: the 50% phase line holds next
    b.ApplyDamage(max);
    assert(std::fabs(b.hp - max * 0.5f) < 0.5f);
    b.normaPhase = 2;
    b.ApplyDamage(max);
    assert(std::fabs(b.hp - max * 0.25f) < 0.5f);
    b.gatesPassed = 2;
    b.ApplyDamage(max);
    assert(!b.alive);
    std::printf("norma clamp ok\n");
}

static void CheckNormaGateQueuesPattern() {
    Monster b(400.0f, 300.0f);
    b.MakeKind(MobKind::NORMA);
    b.ApplyDamage(b.normaMaxHp);
    Tick(b, DT * 2.0f);
    // Either queued or already started (it was between patterns).
    assert(b.normaGateForce == norma::Pattern::Wall ||
           (b.normaGateRunning && b.normaPattern == norma::Pattern::Wall));
    std::printf("norma gate pattern ok\n");
}

// The fight record (System/GameLog.h): begin, pattern, gate, damage both
// ways, and one FIGHT_END line with the totals and damage taken per pattern.
static void CheckFightLog() {
    const char* path = "phase_gate_fight_log.txt";
    {
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        gamelog::Attach(&out);
        int who = 0;
        gamelog::SetClock(10.0f, 4, 80.0f, 200.0f);
        gamelog::FightBegin(&who, "QUASAR", 3000.0f);
        gamelog::FightPattern("BEAM");
        gamelog::Taken("QUASAR_BEAM", 15.0f);
        gamelog::FightDealt(&who, 400.0f, true);
        gamelog::FightDealt(&who, 600.0f, false);
        gamelog::FightDealt(nullptr, 999.0f, false);   // someone else: not counted
        gamelog::SetClock(22.5f, 4, 65.0f, 200.0f);
        gamelog::FightGate(1, 0.667f);
        gamelog::FightPattern("STARFALL");
        gamelog::Taken("QUASAR_FALL", 10.0f);
        gamelog::SetClock(40.0f, 4, 55.0f, 200.0f);
        gamelog::FightEnd("killed");
        assert(!gamelog::g_fight.on);
        gamelog::Detach();
    }
    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto has = [&](const char* s) { return text.find(s) != std::string::npos; };
    assert(has("type=FIGHT_BEGIN,mob=QUASAR,max_hp=3000"));
    assert(has("type=FIGHT_GATE,mob=QUASAR,gate=1"));
    assert(has("type=FIGHT_END,mob=QUASAR,reason=killed,fight_sec=30.00"));
    assert(has("gates=1,gate_sec=12.5"));
    assert(has("dealt=1000,dealt_exposed_pct=40.0"));
    assert(has("taken=25.0,taken_pct_max_hp=12.5"));
    assert(has("patterns=BEAM:1|STARFALL:1"));
    assert(has("taken_by_pattern=BEAM:15.0|STARFALL:10.0"));
    in.close();
    std::remove(path);
    std::printf("fight log ok\n");
}

// ERIDANUS (caelum): at 75% the gate's pattern is queued, it holds still for
// kGateHold (untouchable), runs the smear, and the gate is passed when it ends.
static void CheckCaelumGate() {
    caelum::Reset();
    caelum::SetArena(1920.0f, 1080.0f);
    const float max = 1000.0f, dt = 1.0f / 120.0f;
    float playerHp = 1.0e9f;
    caelum::Step(dt, max * 0.75f, max, 960.0f, 300.0f, 960.0f, 800.0f, playerHp);
    assert(caelum::g.gateForce == caelum::Pattern::Smear);
    bool held = false, ran = false;
    for (float t = 0.0f; t < 30.0f && caelum::g.gatesPassed == 0; t += dt) {
        caelum::Step(dt, max * 0.75f, max, 960.0f, 300.0f, 960.0f, 800.0f, playerHp);
        held = held || caelum::g.gateHold > 0.0f;
        ran = ran || (caelum::g.gateRunning && caelum::g.pattern == caelum::Pattern::Smear);
    }
    assert(held && ran && caelum::g.gatesPassed == 1);
    // The next line (25%) is not reached yet: nothing more is queued.
    caelum::Step(dt, max * 0.6f, max, 960.0f, 300.0f, 960.0f, 800.0f, playerHp);
    assert(caelum::g.gateForce == caelum::Pattern::None);
    caelum::Reset();
    std::printf("caelum gate ok\n");
}

// ---- Stage 8: each elite's phase patterns -------------------------------
// Quasar phase 3: a smaller volley falls alongside the beam (none before).
static void CheckQuasarSideFall() {
    for (int gates = 0; gates <= 2; gates += 2) {
        Monster q(400.0f, 300.0f);
        q.MakeKind(MobKind::QUASAR);
        q.gatesPassed = gates;
        q.quasarState = 2;
        q.quasarTimer = Monster::QUASAR_LOCK_TIME;
        Tick(q, DT);
        assert(q.quasarState == 3);
        assert(q.quasarSideFall == (gates == 2));
        int most = 0;
        for (float t = 0.0f; t < 3.0f; t += DT) {
            float hp = 1.0e9f;
            q.Update(5000.0f, 5000.0f, DT, hp);
            most = std::max(most, q.quasarFallFired);
        }
        assert(most == (gates == 2 ? Monster::QUASAR_SIDE_FALL_COUNT : 0));
        assert(!q.quasarSideFall);            // the volley ended by itself
    }
    std::printf("quasar phase 3 ok\n");
}

// Genesis phase 2: six spots on a row of seven across the player, the gap
// never the middle (the player's) slot nor an end. Phase 3: landing rings.
static void CheckGenesisPhases() {
    Monster g(400.0f, 300.0f);
    g.MakeKind(MobKind::GENESIS);
    for (int run = 0; run < 40; ++run) {
        g.PlanGenesisLane(1000.0f, 300.0f);
        assert(g.genesisLaneVolley);
        bool onPlayer = false, end0 = false, end6 = false;
        for (const auto& t : g.genesisLaneTargets) {
            const glm::vec2 d = t - glm::vec2(400.0f, 300.0f);
            assert(std::fabs(glm::length(d) - (600.0f + Monster::GENESIS_LANE_OVERSHOOT)) < 0.5f);
            // Where the path crosses the player's distance (x = 1000).
            const float slot = (d.y * 600.0f / d.x) / Monster::GENESIS_LANE_SPACING + 3.0f;
            onPlayer = onPlayer || std::fabs(slot - 3.0f) < 0.01f;
            end0 = end0 || std::fabs(slot) < 0.01f;
            end6 = end6 || std::fabs(slot - 6.0f) < 0.01f;
        }
        assert(onPlayer && end0 && end6);
    }
    g.AddGenesisMark(0, 600.0f, 300.0f, 0.3f);
    float hp = 100.0f;
    g.UpdateGenesisMarks(0.2f, 600.0f, 300.0f, hp);
    assert(hp == 100.0f);                               // still in the air
    g.UpdateGenesisMarks(0.2f, 600.0f, 300.0f, hp);
    assert(std::fabs(hp - (100.0f - Monster::GENESIS_MARK_DAMAGE)) < 0.01f);
    g.UpdateGenesisMarks(0.2f, 600.0f, 300.0f, hp);     // inside the tick: no second hit
    assert(std::fabs(hp - (100.0f - Monster::GENESIS_MARK_DAMAGE)) < 0.01f);
    g.UpdateGenesisMarks(Monster::GENESIS_MARK_TIME, 600.0f, 300.0f, hp);
    const float after = hp;
    g.UpdateGenesisMarks(0.6f, 600.0f, 300.0f, hp);
    assert(hp == after);                                // gone
    std::printf("genesis phases ok\n");
}

// Regulus phase 2: the line shows first, then the laser buffs and hurts a
// player on that line. Phase 3: a buffed ally's death exposes the Regulus.
static void CheckRegulusPhases() {
    MonsterManager mm;
    auto* r = new Monster(400.0f, 300.0f);
    r->MakeKind(MobKind::REGULUS);
    r->speed = 0.0f;
    r->gatesPassed = 1;
    r->regulusShotTimer = 0.0f;
    mm.monsters.push_back(r);
    auto* ally = new Monster(600.0f, 300.0f);
    ally->MakeKind(MobKind::ROTOR);
    ally->speed = 0.0f;
    mm.monsters.push_back(ally);
    std::vector<Bullet> bullets;
    float hp = 1000.0f;
    for (float t = 0.0f; t < 0.3f; t += DT) mm.UpdateAll(500.0f, 300.0f, DT, hp, bullets);
    assert(!mm.regulusPending.empty() && !ally->RegulusBuffed() && ally->regulusReserved);
    assert(hp == 1000.0f);
    for (float t = 0.0f; t < 0.4f; t += DT) mm.UpdateAll(500.0f, 300.0f, DT, hp, bullets);
    assert(mm.regulusPending.empty() && ally->RegulusBuffed());
    // The buff laser and the strike laser both run through a player standing
    // between the Regulus and its ally.
    assert(std::fabs(hp - (1000.0f - 2.0f * Monster::REGULUS_LASER_DAMAGE)) < 0.5f);
    // Phase 3.
    r->gatesPassed = 2;
    r->regulusShield.hp = r->regulusShield.maxHp = 100.0f;
    ally->alive = false;
    mm.UpdateAll(5000.0f, 5000.0f, DT, hp, bullets);
    assert(r->regulusExposed > 0.0f && r->regulusShield.hp == 0.0f && r->InOpening());
    assert(std::fabs(r->ApplyDamage(10.0f) - 10.0f * Monster::REGULUS_EXPOSE_DAMAGE) < 0.01f);
    // A shield about to come back stays down for the whole opening.
    r->regulusShield.hp = 0.0f;
    r->regulusShieldRegenTimer = 0.5f;
    r->ExposeRegulus();
    assert(r->regulusShieldRegenTimer >= Monster::REGULUS_EXPOSE_TIME);
    std::printf("regulus phases ok\n");
}

int main() {
    CheckHelpers();
    CheckFightLog();
    CheckCaelumGate();
    CheckEliteGates(MobKind::ANTARES);
    CheckEliteGates(MobKind::GENESIS);
    CheckEliteGates(MobKind::REGULUS);
    CheckEliteGates(MobKind::QUASAR);
    CheckQuasarGateBeam();
    CheckUngated();
    CheckNormaClamp();
    CheckNormaGateQueuesPattern();
    CheckQuasarSideFall();
    CheckGenesisPhases();
    CheckRegulusPhases();
    std::printf("phase gate test passed\n");
    return 0;
}
