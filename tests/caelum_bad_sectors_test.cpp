// CAELUM's bad sectors (Entity/CaelumBoss.h): they hurt harder than a puddle, do
// not count toward the overflow, never heal, and leak into a neighbour.
// Run with scripts/test_caelum_bad_sectors.ps1.
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

// A fresh fight on a 1920x1080 screen with one bad sector at `cell`.
static void Setup(int cell) {
    caelum::Reset();
    caelum::SetArena(1920.0f, 1080.0f);
    caelum::g.active = true;
    caelum::Cell& c = caelum::g.cells[cell];
    c.state = caelum::CellState::Corrupt;
    c.bad = true;
    c.life = 4.0f;
}

static void CheckHurts() {
    const int cell = 1 * caelum::Cols() + 3;
    Setup(cell);
    float cx, cy;
    caelum::CellCentre(cell, cx, cy);
    Check(caelum::InPuddle(cx, cy), "the centre of a bad sector hurts");
    Check(!caelum::InPuddle(cx + caelum::kCell * 0.7f, cy), "well outside its puddle is safe");
    float hp = 100.0f;
    for (int i = 0; i < 60; ++i) caelum::TickCells(1.0f / 60.0f, cx, cy, hp);
    Check(hp < 100.0f - caelum::kBadDps * 0.9f, "standing in it for a second does about kBadDps");
    Check(caelum::g.weaken > 0.0f && caelum::MoveFactor() == caelum::kBadWeak, "standing in it weakens harder");
}

static void CheckNoOverflow() {
    Setup(1 * caelum::Cols() + 1);
    Check(caelum::CorruptCount() == 0, "bad sectors do not count toward the overflow");
}

static void CheckLeak() {
    const int cell = 2 * caelum::Cols() + 3;
    Setup(cell);
    caelum::g.cells[cell].life = 0.5f;   // leaks soon
    float hp = 100.0f;
    for (int i = 0; i < 60; ++i) caelum::TickCells(1.0f / 60.0f, 50.0f, 50.0f, hp);
    int nb[4];
    const int n = caelum::Neighbours(cell, nb);
    int leaked = 0;
    for (int k = 0; k < n; ++k)
        if (caelum::g.cells[nb[k]].state != caelum::CellState::Clean) ++leaked;
    Check(leaked == 1, "one neighbour starts to corrupt after the leak time");
    Check(caelum::g.cells[cell].bad && caelum::Hurts(caelum::g.cells[cell]), "the bad sector stays");
    Check(caelum::g.cells[cell].life >= caelum::kLeakMin, "the next leak is scheduled");
}

// Difficulty: HARD is the fight as designed; EASY has no leaks, no erosion and
// no drag; NIGHTMARE presses harder.
static void CheckDifficulty() {
    caelum::SetDifficulty(2);
    Check(caelum::g_Tune.corruptDps == caelum::kCorruptDps && caelum::g_Tune.snipes == caelum::kSnipes,
          "HARD is the designed fight");
    caelum::SetDifficulty(0);
    const int cell = 2 * caelum::Cols() + 3;
    Setup(cell);
    caelum::g.cells[cell].life = 0.2f;
    float hp = 100.0f;
    for (int i = 0; i < 120; ++i) caelum::TickCells(1.0f / 60.0f, 50.0f, 50.0f, hp);
    int nb[4], leaked = 0;
    const int n = caelum::Neighbours(cell, nb);
    for (int k = 0; k < n; ++k) if (caelum::g.cells[nb[k]].state != caelum::CellState::Clean) ++leaked;
    Check(leaked == 0, "EASY: bad sectors do not leak");
    caelum::g.phase = 2;
    bool drag = false;
    for (int k = 0; k < 40; ++k) {
        caelum::g.count = k;
        drag = drag || caelum::PickPattern() == caelum::Pattern::Drag;
    }
    Check(!drag, "EASY: phase 2 never drags the body");
    Check(caelum::g_Tune.erodeAfter == 0.0f, "EASY: no erosion");
    caelum::SetDifficulty(3);
    Check(caelum::g_Tune.shotSpeed > caelum::kShotSpeed && caelum::g_Tune.warn < caelum::kWarn,
          "NIGHTMARE: faster shots, shorter warnings");
    caelum::SetDifficulty(2);
}

int main() {
    CheckDifficulty();
    CheckHurts();
    CheckNoOverflow();
    CheckLeak();
    if (g_failures == 0) std::printf("caelum bad sectors: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
