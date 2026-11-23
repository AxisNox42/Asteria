// Headless regression for the HOROLOGIUM clock elite, third pattern pass
// (HorologiumMob.h + its Monster wiring): the reverse volley runs its lanes
// out and back and can hit on both legs; the stasis ring freezes only a
// player still inside after 1 s, and its strike only hits inside; the
// winding rest takes x1.3; recall only from phase 2, pulls the player back
// onto the mark and the shock can be dodged, and enough damage during the
// countdown breaks it; tempo only from phase 3, alternating slow and fast.
// Run with scripts/test_horologium_elite.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "audio_stub.h"   // game code plays sounds; tests stay silent

using namespace horologium;

static constexpr float DT = 1.0f / 120.0f;

struct Hurt {
    float* total; int* hits; const char** last;
    void operator()(float d, const char* s) const { *total += d; ++*hits; *last = s; }
};

static void ResetPlayerTime() { g_player = {}; }

static void CheckVolleyOutAndBack() {
    struct NeedleHits {
        int* out; int* back;
        void operator()(float, const char* s) const {
            if (std::string(s) == "HOROLOGIUM_NEEDLE") ++*out;
            if (std::string(s) == "HOROLOGIUM_REVERSE") ++*back;
        }
    };
    // No range: in an arena, needles fly to its edge, hold, then come back
    // to the dial (and are gone once there).
    ResetPlayerTime();
    Brain b;
    b.volleyAim = 0.0f;
    b.FireSalvo(500.0f, 300.0f);
    int o = 0, k = 0;
    bool reachedEdge = false;
    float ex = 500.0f, ey = 300.0f;
    NeedleHits counter{ &o, &k };
    for (int i = 0; i < 120 * 6 && b.AnyNeedle(); ++i) {
        b.StepNeedles(ex, ey, 500.0f, -5000.0f, DT, 0.0f, 0.0f, 1000.0f, 600.0f, counter);
        for (const auto& n : b.needles) reachedEdge |= n.active && n.leg == Leg::Hold && n.x >= 999.0f;
    }
    assert(reachedEdge && "the middle lane reaches the arena edge");
    assert(!b.AnyNeedle() && "all back in the dial");
    // Each salvo re-aims at where the player is when it fires.
    ResetPlayerTime();
    Brain s;
    s.Begin(State::Idle, 0.0f);
    float sx = 0.0f, sy = 0.0f;
    int so = 0, sk = 0;
    float firstAim = 0.0f, lastAim = 0.0f;
    for (int i = 0; i < (int)((kVolleyWarn + kSalvoGap * kSalvos) / DT); ++i) {
        const float t = i * DT;
        const float py = t < kVolleyWarn + 0.1f ? 0.0f : 400.0f;   // jumps sideways after the first salvo
        const int before = s.salvosFired;
        s.Step(sx, sy, 400.0f, py, DT, 1, 0, 0, 0, 0, 0.0f, NeedleHits{ &so, &sk });
        if (s.salvosFired == 1 && before == 0) firstAim = s.LaneAngle(kLanes / 2);
        if (s.salvosFired == kSalvos && before == kSalvos - 1) lastAim = s.LaneAngle(kLanes / 2);
    }
    assert(s.salvosFired == kSalvos);
    assert(std::fabs(firstAim) < 0.05f && std::fabs(lastAim - std::atan2(400.0f, 400.0f)) < 0.1f && "re-aimed");
    // A player standing in front is hit going out and coming back.
    ResetPlayerTime();
    Brain c;
    c.Begin(State::Idle, 0.0f);
    int outHits = 0, backHits = 0;
    float cx = 0.0f, cy = 0.0f;
    for (int i = 0; i < 120 * 8; ++i)
        c.Step(cx, cy, 300.0f, 0.0f, DT, 1, -2000.0f, -1200.0f, 2000.0f, 1200.0f, 0.0f, NeedleHits{ &outHits, &backHits });
    std::printf("  volley hits: out %d, back %d\n", outHits, backHits);
    assert(outHits >= 1 && backHits >= 1 && "hit going out and coming back");
    ResetPlayerTime();
    std::printf("reverse volley ok\n");
}

static void CheckStasis() {
    // Stays inside: frozen, struck.
    ResetPlayerTime();
    Brain b;
    b.Begin(State::Volley, 0.001f);
    b.salvosFired = kSalvos;   // only the stasis here
    float ex = 0, ey = 0, t = 0; int hits = 0; const char* src = "";
    for (int i = 0; i < (int)((kStasisWarn + 0.1f) / DT); ++i)
        b.Step(ex, ey, 400.0f, 0.0f, DT, 1, 0, 0, 0, 0, 0.0f, Hurt{ &t, &hits, &src });
    assert(b.state == State::Freeze && b.caught && MoveFactor() == 0.0f);
    for (int i = 0; i < (int)(kFreezeTime / DT); ++i)
        b.Step(ex, ey, 400.0f, 0.0f, DT, 1, 0, 0, 0, 0, 0.0f, Hurt{ &t, &hits, &src });
    assert(t >= kStasisDamage);
    // Leaves in time: not frozen, not struck.
    ResetPlayerTime();
    Brain c;
    c.Begin(State::Volley, 0.001f);
    c.salvosFired = kSalvos;   // only the stasis here
    float t2 = 0; int h2 = 0; ex = 0; ey = 0;
    float px = 400.0f;
    for (int i = 0; i < (int)((kStasisWarn + kFreezeTime + 0.1f) / DT); ++i) {
        // While the ring follows, moving does nothing; once it locks, dash out.
        if (c.state == State::StasisWarn && c.timer <= kStasisWarn - kStasisFollow) px += 700.0f * DT;
        c.Step(ex, ey, px, 0.0f, DT, 1, 0, 0, 0, 0, 0.0f, Hurt{ &t2, &h2, &src });
    }
    assert(!c.caught && t2 == 0.0f);
    // It follows: the ring ends up where the player was when it locked.
    ResetPlayerTime();
    Brain f;
    f.Begin(State::Volley, 0.001f);
    f.salvosFired = kSalvos;   // only the stasis here
    float fx = 0, fy = 0, ft = 0; int fh = 0;
    float lockedAtX = 0.0f;
    for (int i = 0; i < (int)((kStasisWarn - 0.05f) / DT); ++i) {
        const float ppx = 400.0f + 200.0f * (i * DT);
        f.Step(fx, fy, ppx, 0.0f, DT, 1, 0, 0, 0, 0, 0.0f, Hurt{ &ft, &fh, &src });
        if (f.state == State::StasisWarn && f.timer > kStasisWarn - kStasisFollow) lockedAtX = ppx;
    }
    assert(std::fabs(f.stasisX - lockedAtX) < 5.0f && f.stasisX > 450.0f && "followed, then locked");
    std::printf("stasis ok\n");
}

static void CheckRestDamage() {
    Monster m(0.0f, 0.0f);
    m.MakeKind(MobKind::HOROLOGIUM);
    m.horologium.Begin(State::Rest, kRestTime);
    const float hp0 = m.hp;
    m.ApplyDamage(10.0f);
    assert(std::fabs((hp0 - m.hp) - 13.0f) < 0.01f && m.InOpening());
    std::printf("rest damage ok\n");
}

static void CheckRecall() {
    // Phase 1: never.
    {
        ResetPlayerTime();
        Brain b; b.recallCooldown = 0.0f;
        float ex = 0, ey = 0, t = 0; int h = 0; const char* src = "";
        for (int i = 0; i < 120; ++i) b.Step(ex, ey, 400.0f, 0.0f, DT, 1, 0, 0, 0, 0, 0.0f, Hurt{ &t, &h, &src });
        assert(b.recall == Recall::Off);
    }
    // Phase 2: stamp, pull back, shock dodged by stepping off.
    ResetPlayerTime();
    Brain b; b.recallCooldown = 0.0f; b.maxHp = 3000.0f;
    b.Begin(State::Rest, 99.0f);   // keep the main cycle out of the way
    float ex = 0, ey = 0, t = 0; int h = 0; const char* src = "";
    for (int i = 0; i < (int)((kRecallAim + 0.05f) / DT); ++i)
        b.Step(ex, ey, 500.0f, 100.0f, DT, 2, 0, 0, 0, 0, 0.0f, Hurt{ &t, &h, &src });
    assert(b.recall == Recall::Count && std::fabs(b.markX - 500.0f) < 0.1f);
    float pulledX = 0, pulledY = 0;
    bool pulled = false;
    for (int i = 0; i < (int)((kRecallCount + 0.05f) / DT) && !pulled; ++i) {
        b.Step(ex, ey, 900.0f, 300.0f, DT, 2, 0, 0, 0, 0, 0.0f, Hurt{ &t, &h, &src });   // wandered off
        pulled = TakeRecall(pulledX, pulledY);
    }
    assert(pulled && std::fabs(pulledX - 500.0f) < 0.1f && std::fabs(pulledY - 100.0f) < 0.1f);
    // Step off the mark before the shock.
    for (int i = 0; i < (int)((kRecallShockDelay + 0.05f) / DT); ++i)
        b.Step(ex, ey, 500.0f + kRecallShockRadius + 40.0f, 100.0f, DT, 2, 0, 0, 0, 0, 0.0f, Hurt{ &t, &h, &src });
    assert(t == 0.0f && b.recall == Recall::Off && "the shock can be dodged");
    // Breaking it: enough damage during the countdown.
    ResetPlayerTime();
    Monster m(0.0f, 0.0f);
    m.MakeKind(MobKind::HOROLOGIUM);
    m.gatesPassed = 1;
    m.horologium.recallCooldown = 0.0f;
    m.horologium.Begin(State::Rest, 99.0f);
    float php = 100.0f;
    for (int i = 0; i < (int)((kRecallAim + 0.05f) / DT); ++i)
        m.Update(500.0f, 0.0f, DT, php, 1.0f, -1, -1, -1, -1, nullptr, glm::vec4(0.0f));
    assert(m.horologium.recall == Recall::Count);
    m.ApplyDamage(m.horologium.RecallBreakNeed() / 1.3f + 1.0f);   // rest x1.3 counts too
    m.Update(500.0f, 0.0f, DT, php, 1.0f, -1, -1, -1, -1, nullptr, glm::vec4(0.0f));
    assert(m.horologium.recall == Recall::Off && !g_player.recall && "broken by damage");
    std::printf("recall ok\n");
}

static void CheckTempo() {
    for (int phase = 2; phase <= 3; ++phase) {
        ResetPlayerTime();
        Brain b;
        b.Begin(State::Rest, 99.0f);
        float ex = 0, ey = 0, t = 0; int h = 0; const char* src = "";
        bool sawSlow = false, sawFast = false, announced = false, ticked = false;
        for (int i = 0; i < (int)(5.0f / DT); ++i) {
            b.Step(ex, ey, 2000.0f, 0.0f, DT, phase, 0, 0, 0, 0, 0.0f, Hurt{ &t, &h, &src });
            announced |= g_player.announce > 0.0f;
            ticked |= b.tickCue;
            Tick(DT);
            sawSlow |= MoveFactor() == kTempoSlow;
            sawFast |= MoveFactor() == kTempoFast;
        }
        assert((sawSlow && sawFast) == (phase >= 3));
        assert(announced == (phase >= 3) && "explained on the HUD when it starts");
        assert(ticked == (phase >= 3) && "a tick before each switch");
    }
    ResetPlayerTime();
    std::printf("tempo ok\n");
}

static void CheckMonsterWiring() {
    ResetPlayerTime();
    Monster m(0.0f, 0.0f);
    m.MakeKind(MobKind::HOROLOGIUM);
    assert(IsEliteMob(MobKind::HOROLOGIUM) && m.UsesEliteGates());
    assert(m.horologium.maxHp == m.eliteMaxHp);
    m.gatesPassed = 2;
    assert(m.HorologiumPhase() == 3);
    Monster g(0.0f, 0.0f);
    g.MakeKind(MobKind::HOROLOGIUM);
    float php = 100.0f;
    for (int i = 0; i < 120 * 8; ++i)
        g.Update(400.0f, 0.0f, DT, php, 1.0f, -1, -1, -1, -1, nullptr, glm::vec4(0.0f));
    assert(php < 100.0f && "a still player gets hit");
    ResetPlayerTime();
    std::printf("monster wiring ok\n");
}

int main() {
    CheckVolleyOutAndBack();
    CheckStasis();
    CheckRestDamage();
    CheckRecall();
    CheckTempo();
    CheckMonsterWiring();
    std::printf("horologium elite: all ok\n");
    return 0;
}
