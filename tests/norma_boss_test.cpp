// NORMA boss body rules (WiNILL/Entity/NormaBoss.h).
#include "NormaBoss.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

static void PhaseTwoAtFortyPercent() {
    Check(norma::PhaseFor(1000.0f, 1000.0f) == 1, "full HP is phase 1");
    Check(norma::PhaseFor(501.0f, 1000.0f) == 1, "just above 50% is phase 1");
    Check(norma::PhaseFor(500.0f, 1000.0f) == 2, "exactly 50% is phase 2");
    Check(norma::PhaseFor(10.0f, 1000.0f) == 2, "low HP is phase 2");
    Check(norma::PhaseFor(10.0f, 0.0f) == 1, "unknown max HP stays phase 1");
}

static void KeepsHoverDistance() {
    Check(norma::HoverSpeed(norma::kHoverDistance + norma::kHoverBand + 1.0f) > 0.0f,
          "too far: closes in");
    Check(norma::HoverSpeed(norma::kHoverDistance - norma::kHoverBand - 1.0f) < 0.0f,
          "too close: backs off");
    Check(norma::HoverSpeed(norma::kHoverDistance) == 0.0f, "inside the band: holds");
    Check(norma::HoverSpeedSmooth(norma::kHoverDistance) == 0.0f, "smooth: inside the band holds");
    const float edge = norma::kHoverDistance + norma::kHoverBand;
    Check(norma::HoverSpeedSmooth(edge + 1.0f) > 0.0f && norma::HoverSpeedSmooth(edge + 1.0f) < 1.0f,
          "smooth: speed starts from nothing just past the band");
    Check(std::fabs(norma::HoverSpeedSmooth(edge + 1000.0f) - norma::kMoveSpeed) < 1e-3f,
          "smooth: far away reaches the full speed");
    Check(norma::HoverSpeedSmooth(norma::kHoverDistance - norma::kHoverBand - 40.0f) < 0.0f,
          "smooth: too close backs off");
    float prev = 0.0f;
    bool even = true;
    for (float d = edge; d < edge + 200.0f; d += 1.0f) {
        const float v = norma::HoverSpeedSmooth(d);
        if (v < prev - 1e-4f || v - prev > 4.0f) even = false;
        prev = v;
    }
    Check(even, "smooth: speed rises evenly, no jumps");
}

static int g_seed = 1;
static int TestRoll() {   // a small LCG, so the decks are the same every run
    g_seed = g_seed * 1103515245 + 12345;
    return (g_seed >> 16) & 0x7fff;
}

// The pattern deck: every pattern once a round, none back within two turns.
static void PatternsComeFromADeck() {
    using norma::Pattern;
    for (int act = 1; act <= 4; ++act) {
        const int n = act >= 3 ? 7 : 5;
        for (int seed = 1; seed <= 40; ++seed) {
            g_seed = seed;
            norma::Deck deck;
            Pattern last = Pattern::None, last2 = Pattern::None;
            for (int round = 0; round < 6; ++round) {
                int seen[32] = {};
                for (int i = 0; i < n; ++i) {
                    const Pattern p = norma::DrawPattern(deck, act, last, last2, TestRoll);
                    Check(p != Pattern::None, "the deck always deals a pattern");
                    Check(p != last && p != last2, "no pattern comes back within two turns");
                    Check(p != Pattern::Detonate && p != Pattern::Reinforce && p != Pattern::Crash &&
                          p != Pattern::Stretch, "Detonate, Reinforce, the blue screen and Stretch are never dealt");
                    if (act < 3)
                        Check(p != Pattern::Warp && p != Pattern::CloseWindow,
                              "phase 1 never warps or closes a window");
                    ++seen[(int)p];
                    last2 = last;
                    last = p;
                }
                bool once = true;
                for (int k = 0; k < 32; ++k) once = once && seen[k] <= 1;
                Check(once, "a round shows every pattern once");
                Check(seen[(int)Pattern::Aim] == 1 && seen[(int)Pattern::Ram] == 1 &&
                      seen[(int)Pattern::Refuge] == 1 && seen[(int)Pattern::Throw] == 1 &&
                      seen[(int)Pattern::Wall] == 1, "the five base patterns are in every round");
                if (act >= 3)
                    Check(seen[(int)Pattern::Warp] == 1 && seen[(int)Pattern::CloseWindow] == 1,
                          "phase 2 adds Warp and CloseWindow");
            }
        }
    }
    // Phase 2 deals a fresh hand at once; crossing 75% or 25% keeps the hand.
    norma::Deck deck;
    g_seed = 7;
    norma::DrawPattern(deck, 1, Pattern::None, Pattern::None, TestRoll);
    norma::DrawPattern(deck, 2, Pattern::None, Pattern::None, TestRoll);
    Check(deck.n == 5 && deck.pos == 2, "act 2 goes on with act 1's hand");
    norma::DrawPattern(deck, 3, Pattern::None, Pattern::None, TestRoll);
    Check(deck.n == 7 && deck.pos == 1, "act 3 starts its own hand");
    // Different orders from round to round.
    bool differs = false;
    norma::Deck d2;
    g_seed = 3;
    Pattern first[5], l = Pattern::None, l2 = Pattern::None;
    for (int i = 0; i < 5; ++i) { first[i] = norma::DrawPattern(d2, 1, l, l2, TestRoll); l2 = l; l = first[i]; }
    for (int round = 0; round < 8 && !differs; ++round)
        for (int i = 0; i < 5; ++i) {
            const Pattern p = norma::DrawPattern(d2, 1, l, l2, TestRoll);
            differs = differs || p != first[i];
            l2 = l; l = p;
        }
    Check(differs, "the order changes between rounds");
    // A card that cannot run with the panes left waits; only what can run is dealt.
    for (int panes = 0; panes <= 1; ++panes)
        for (int seed = 1; seed <= 40; ++seed) {
            g_seed = seed;
            norma::Deck d3;
            Pattern a = Pattern::None, b = Pattern::None;
            for (int i = 0; i < 30; ++i) {
                const Pattern p = norma::DrawPattern(d3, 4, a, b, TestRoll, panes);
                Check(norma::CanRun(p, panes), "nothing that needs more panes than stand is dealt");
                Check(p != Pattern::Wall && p != Pattern::Warp, "under two panes: no Wall or Warp");
                if (panes == 0) Check(p == Pattern::Aim || p == Pattern::Throw, "no pane: the stream or the box");
                b = a; a = p;
            }
        }
}

// The boss's panes: refilled in a batch, lit a few at a time.
static void PanesAreRefilledInBatches() {
    using norma::Pattern;
    using norma::Tail;
    Check(norma::RefillCount(-1, 1) == 0, "unknown pane count: no refill");
    Check(norma::RefillCount(3, 1) == 0 && norma::RefillCount(2, 1) == 0, "phase 1: full or one short, no refill");
    Check(norma::RefillCount(1, 1) == 2 && norma::RefillCount(0, 1) == 3, "phase 1: two or three open together");
    Check(norma::RefillCount(5, 2) == 0 && norma::RefillCount(4, 2) == 0, "phase 2: full or one short, no refill");
    Check(norma::RefillCount(3, 2) == 2 && norma::RefillCount(2, 2) == 3, "phase 2: two or three open together");
    Check(norma::RefillCount(0, 2) == 3, "never more than three in one batch");
    for (int phase = 1; phase <= 2; ++phase)
        for (int n = 0; n <= 5; ++n) {
            const int r = norma::RefillCount(n, phase);
            Check(r == 0 || (r >= 2 && r <= 3), "a batch is two or three panes");
        }
    Check(norma::kRefillStagger == 0.7f, "one pane every 0.7 s");
    // Partial: only with a free pattern, two or more panes, every other time.
    Check(norma::TailFor(Pattern::Aim, 3, 0) == Tail::Partial && norma::TailFor(Pattern::Throw, 2, 0) == Tail::Partial,
          "the stream or the box, panes standing: some are lit");
    Check(norma::TailFor(Pattern::Aim, 3, 1) == Tail::None, "every other time");
    Check(norma::TailFor(Pattern::Aim, 3, -4) == Tail::Partial, "negative rolls count too");
    Check(norma::TailFor(Pattern::Aim, 1, 0) == Tail::None && norma::TailFor(Pattern::Aim, -1, 0) == Tail::None,
          "one pane (or unknown): left alone");
    for (int roll = 0; roll < 4; ++roll)
        Check(norma::TailFor(Pattern::Refuge, 3, roll) == Tail::None && norma::TailFor(Pattern::Ram, 3, roll) == Tail::None &&
              norma::TailFor(Pattern::Wall, 3, roll) == Tail::None, "patterns that use the panes keep them");
    Check(norma::PartialCountFor(1) == 1 && norma::PartialCountFor(2) == 1 && norma::PartialCountFor(3) == 2,
          "one pane lit, two from act 3");
    Check(norma::PanesNeeded(Pattern::Aim) == 0 && norma::PanesNeeded(Pattern::Throw) == 0 &&
          norma::PanesNeeded(Pattern::Wall) == 2, "the stream and the box need no pane, the wall gathers two");
    Check(norma::kWallRedden < norma::kWallWarn && norma::kWallRelease > 0.0f, "the wall reddens late and is let go over time");
}

// The fight's four acts and the "not responding" clock.
static void ActsFollowTheGates() {
    Check(norma::ActFor(1000.0f, 1000.0f) == 1 && norma::ActFor(751.0f, 1000.0f) == 1, "act 1 above 75%");
    Check(norma::ActFor(750.0f, 1000.0f) == 2 && norma::ActFor(501.0f, 1000.0f) == 2, "act 2 down to 50%");
    Check(norma::ActFor(500.0f, 1000.0f) == 3 && norma::ActFor(251.0f, 1000.0f) == 3, "act 3 is phase 2's first half");
    Check(norma::ActFor(250.0f, 1000.0f) == 4 && norma::ActFor(1.0f, 1000.0f) == 4, "act 4 under 25%");
    Check(norma::ActFor(10.0f, 0.0f) == 1, "unknown max HP is act 1");
    Check(norma::IdleSecondsFor(1) == norma::kIdleSecondsFirst && norma::IdleSecondsFor(1) > norma::IdleSecondsFor(2),
          "act 1 has the clock too, the longest one");
    Check(norma::IdleSecondsFor(2) == norma::kIdleSeconds && norma::IdleSecondsFor(3) == norma::kIdleSeconds,
          "acts 2 and 3 share the clock");
    Check(norma::IdleSecondsFor(4) < norma::IdleSecondsFor(3) && norma::IdleSecondsFor(4) > norma::kIdleWarn,
          "act 4 is quicker, still longer than its warning");
    Check(norma::kRefugeHopAt < norma::kRefugeSeconds - 1.4f, "the shelter's fuse burns out inside the flood");
    Check(norma::kAimCutAt < norma::kAimSeconds && norma::kAimCutAt2 < norma::kAimSeconds, "the cuts fall inside the stream");
    Check(norma::kWallStreamFrom < norma::kWallSeconds, "the wall stream fits the wall");
}

static void LeadsMovingTargets() {
    // Standing still: aim straight at it.
    Check(std::fabs(norma::LeadAngle(0, 0, 100, 0, 0, 0, 300)) < 1e-4f, "still target: direct aim");
    // Moving up across the line of fire: the aim leads upward (positive y).
    const float lead = norma::LeadAngle(0, 0, 300, 0, 0, 200, 400);
    Check(lead > 0.2f, "crossing target: aim leads it");
    // The shot really meets it: solve the meeting time along the lead angle.
    const float vx = std::cos(lead) * 400.0f, vy = std::sin(lead) * 400.0f;
    const float t = 300.0f / vx;
    Check(std::fabs(vy * t - 200.0f * t) < 1.0f, "lead shot meets the target");
    // lead 0 is the current position, lead 2 overshoots the intercept.
    Check(std::fabs(norma::LeadAngle(0, 0, 300, 0, 0, 200, 400, 0.0f)) < 1e-4f, "lead 0 aims at now");
    Check(norma::LeadAngle(0, 0, 300, 0, 0, 200, 400, 2.0f) > lead, "lead 2 aims past the intercept");
    // A target faster than the shot, running away: falls back to direct aim.
    Check(std::fabs(norma::LeadAngle(0, 0, 100, 0, 500, 0, 100)) < 1e-4f, "no intercept: direct aim");
}

static void HealthStarsCountEighths() {
    Check(norma::LitStarsFor(100.0f, 100.0f) == 8, "full health lights all eight stars");
    Check(norma::LitStarsFor(87.6f, 100.0f) == 8, "the first eighth keeps all eight");
    Check(norma::LitStarsFor(87.4f, 100.0f) == 7, "losing an eighth darkens one");
    Check(norma::LitStarsFor(40.0f, 100.0f) == 4, "phase 2 starts with four lit");
    Check(norma::LitStarsFor(1.0f, 100.0f) == 1, "the last star stays lit while alive");
    Check(norma::LitStarsFor(0.0f, 100.0f) == 0, "dead: all dark");
    bool seen[8] = {};
    for (int k = 0; k < 8; ++k) seen[norma::kStarOutOrder[k]] = true;
    bool all = true;
    for (bool s : seen) all = all && s;
    Check(all, "the star order covers every corner once");
}

int main() {
    HealthStarsCountEighths();
    LeadsMovingTargets();
    PhaseTwoAtFortyPercent();
    KeepsHoverDistance();
    PatternsComeFromADeck();
    PanesAreRefilledInBatches();
    ActsFollowTheGates();
    if (g_failures == 0) std::printf("norma boss: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
