// CAELUM's siphon (Entity/CaelumBoss.h): the puddles become streams that run
// into the body, hurt where they are, leave the board clean, draw the player
// in, and the pattern only runs with enough puddles out. Then it spouts
// what it drank back round the player.
// Run with scripts/test_caelum_siphon.ps1.
#include "CaelumBoss.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

constexpr float kBx = 1000.0f, kBy = 430.0f;   // the body (cell 3, 1)
constexpr float kPx = 150.0f, kPy = 950.0f;    // the player, far off

// A fight with four puddles away from the body, the siphon started.
static void Setup(int phase = 1) {
    caelum::Reset();
    caelum::SetArena(1920.0f, 1080.0f);
    caelum::SetDifficulty(2);
    caelum::g.active = true;
    caelum::g.phase = phase;
    caelum::g.bossX = kBx; caelum::g.bossY = kBy;
    caelum::g.prevX = kPx; caelum::g.prevY = kPy;
    const int cols = caelum::Cols();
    const int cells[4] = { 0 * cols + 0, 0 * cols + 6, 3 * cols + 5, 2 * cols + 1 };
    for (int c : cells) {
        caelum::g.cells[c].state = caelum::CellState::Corrupt;
        caelum::g.cells[c].life = 9.0f;
    }
    caelum::StartPattern(kPx, kPy, caelum::Pattern::Siphon);
}

static void Run(float seconds, float& hp) {
    const float dt = 1.0f / 120.0f;
    for (float t = 0.0f; t < seconds; t += dt)
        caelum::Step(dt, 1000.0f, 1000.0f, kBx, kBy, kPx, kPy, hp);
}

static void CheckDrinks() {
    Setup();
    Check(caelum::g.drops.size() == 4, "every puddle away from the body is a drop");
    float hp = 1.0e6f;
    Run(caelum::kSiphonStart + caelum::kSiphonStagger + 0.5f, hp);
    Check(!caelum::g_DropBalls.empty(), "mid-way the streams are out");
    bool cleaned = true;
    for (const caelum::Drop& d : caelum::g.drops)
        if (caelum::Hurts(caelum::g.cells[d.cell])) cleaned = false;
    Check(cleaned, "a drinking puddle has left its cell");
    // The stream hurts where it is: the middle of its body.
    const caelum::Ball& b = caelum::g_DropBalls[caelum::g_DropBalls.size() / 2];
    Check(caelum::InPuddle(b.x, b.y), "the stream hurts where it runs");
    // The player is drawn toward the body.
    const float dot = caelum::g.pullX * (kBx - kPx) + caelum::g.pullY * (kBy - kPy);
    Check(dot > 0.0f && std::hypot(caelum::g.pullX, caelum::g.pullY) > caelum::kSiphonPull * 0.9f,
          "the player is pulled toward the body");
    // On the swallow (before anything is spouted back) the board is clean.
    for (int i = 0; i < 1200 && caelum::g.spoutAt < 0.0f; ++i)
        caelum::Step(1.0f / 120.0f, 1000.0f, 1000.0f, kBx, kBy, kPx, kPy, hp);
    int left = 0;
    const int boss = caelum::CellAt(kBx, kBy);
    for (int i = 0; i < caelum::CellCount(); ++i)
        if (i != boss && caelum::Hurts(caelum::g.cells[i])) ++left;
    Check(left == 0, "the board is clean (but the body's own cell)");
    Check(caelum::g.pullX == 0.0f, "no pull once it has swallowed");
    Run(3.0f, hp);
    Check(caelum::g.pattern != caelum::Pattern::Siphon && caelum::g.last == caelum::Pattern::Siphon, "it ends");
    Check(caelum::g_DropBalls.empty() && caelum::g.pullX == 0.0f, "no streams and no pull after");
}

static void CheckOpenAfter() {
    Setup();
    float hp = 1.0e6f;
    for (int i = 0; i < 1200 && caelum::g.pattern == caelum::Pattern::Siphon; ++i)
        caelum::Step(1.0f / 120.0f, 1000.0f, 1000.0f, kBx, kBy, kPx, kPy, hp);
    Check(caelum::g.exposed > caelum::kExposeSeconds, "it stays open longer after drinking");
}

// In either phase what it drank comes back out round the player, then it opens.
static void CheckSpout(int phase) {
    Setup(phase);
    float hp = 1.0e6f;
    const float dt = 1.0f / 120.0f;
    for (int i = 0; i < 1200 && caelum::g.spoutAt < 0.0f; ++i)
        caelum::Step(dt, 1000.0f, 1000.0f, kBx, kBy, kPx, kPy, hp);
    Check(caelum::g.spoutAt >= 0.0f && caelum::g.pattern == caelum::Pattern::Siphon, "it goes on into the spout");
    const int n = (int)caelum::g.drops.size();
    Check(n == 4, "a stream for every puddle drunk (under the cap)");
    const int here = caelum::CellAt(kPx, kPy), cols = caelum::Cols();
    bool warned = true, round = true;
    for (const caelum::Drop& d : caelum::g.drops) {
        if (caelum::g.cells[d.cell].state != caelum::CellState::Warn) warned = false;
        const int dx = std::abs(d.cell % cols - here % cols), dy = std::abs(d.cell / cols - here / cols);
        if (d.cell == here || dx > 2 || dy > 2) round = false;
    }
    Check(warned, "where they land warns");
    Check(round, "they land round the player, never on them");
    Check(caelum::g_DropBalls.empty(), "nothing flies while it swells");
    Run(caelum::g_Tune.spoutSwell + 0.25f, hp);
    Check(!caelum::g_DropBalls.empty(), "then the streams fly");
    Check(caelum::g.comboQueued || caelum::g.waveT >= 0.0f, "a ring wave is on its way");
    for (int i = 0; i < 1200 && caelum::g.pattern == caelum::Pattern::Siphon; ++i)
        caelum::Step(dt, 1000.0f, 1000.0f, kBx, kBy, kPx, kPy, hp);
    Check(caelum::g.last == caelum::Pattern::Siphon && caelum::g.exposed > caelum::kExposeSeconds,
          "it ends wide open");
    int puddles = 0;   // (the player's own cell erodes: they stood still)
    for (int i = 0; i < caelum::CellCount(); ++i)
        if (i != caelum::CellAt(kBx, kBy) && i != here && caelum::Hurts(caelum::g.cells[i])) ++puddles;
    Check(puddles == n, "each stream left a puddle");
    Check(caelum::g_DropBalls.empty(), "no streams after");
}

static void CheckNeedsPuddles() {
    caelum::Reset();
    caelum::SetArena(1920.0f, 1080.0f);
    caelum::g.phase = 1;
    caelum::g.count = 4;   // the siphon's slot in phase 1
    Check(caelum::PickPattern() != caelum::Pattern::Siphon, "no siphon on a clean board");
    for (int c = 0; c < 4; ++c) caelum::g.cells[c * 2].state = caelum::CellState::Corrupt;
    Check(caelum::PickPattern() == caelum::Pattern::Siphon, "the siphon with puddles out");
}

int main() {
    CheckDrinks();
    CheckOpenAfter();
    CheckSpout(1);
    CheckSpout(2);
    CheckNeedsPuddles();
    if (g_failures == 0) std::printf("caelum siphon: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
