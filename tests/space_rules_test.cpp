// Window space rules for the NORMA boss (WiNILL/Entity/SpaceRules.h).
#include "SpaceRules.h"

#include <cmath>
#include <vector>
#include <cstdio>

static int g_failures = 0;

static void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

static void Reset() {
    space::g_Regions.clear();
    space::g_PlayerSpace = 0;
    space::g_HurtSource = space::kUntracked;
}

static void NoPanesAllowsEverything() {
    Reset();
    Check(!space::Active(), "no panes: inactive");
    Check(space::SpaceAt(100.0f, 100.0f) == 0, "no panes: open field");
    Check(space::Reaches(space::kPlayerSource, 5.0f, 5.0f), "no panes: player reaches");
    Check(space::Reaches(7, 5.0f, 5.0f), "no panes: any source reaches");
    space::g_HurtSource = 3;
    Check(space::PlayerExposed(), "no panes: player exposed");
    int pane = -1;
    Check(!space::CrossedBorder(0, 5.0f, 5.0f, pane), "no panes: shot keeps going");
}

static void FrontPaneOwnsOverlap() {
    Reset();
    // Pane 2 in front of pane 1; they overlap on x 150..200.
    space::g_Regions = { {2, 150.0f, 0.0f, 300.0f, 100.0f},
                         {1, 0.0f, 0.0f, 200.0f, 100.0f} };
    Check(space::SpaceAt(50.0f, 50.0f) == 1, "inside back pane only");
    Check(space::SpaceAt(175.0f, 50.0f) == 2, "overlap belongs to the front pane");
    Check(space::SpaceAt(250.0f, 50.0f) == 2, "inside front pane only");
    Check(space::SpaceAt(400.0f, 50.0f) == 0, "outside both");
    Check(space::SpaceAt(200.0f, 150.0f) == 0, "below both");
}

static void AttacksStayInTheirSpace() {
    Reset();
    space::g_Regions = { {4, 0.0f, 0.0f, 100.0f, 100.0f} };
    space::g_PlayerSpace = 4;
    Check(space::Reaches(space::kPlayerSource, 50.0f, 50.0f), "player hits a mob in its pane");
    Check(!space::Reaches(space::kPlayerSource, 150.0f, 50.0f), "player cannot hit outside its pane");
    Check(space::Reaches(0, 150.0f, 50.0f), "open-field shot hits an open-field mob");
    Check(!space::Reaches(0, 50.0f, 50.0f), "open-field shot cannot hit inside the pane");
    Check(space::Reaches(space::kUntracked, 50.0f, 50.0f), "untracked source is allowed");
    Check(space::SameSpace(10.0f, 10.0f, 90.0f, 90.0f), "same pane");
    Check(!space::SameSpace(10.0f, 10.0f, 190.0f, 90.0f), "pane and field differ");
}

static void EnemyHitsNeedTheSameSpace() {
    Reset();
    space::g_Regions = { {4, 0.0f, 0.0f, 100.0f, 100.0f} };
    space::g_PlayerSpace = 4;
    {
        space::HurtSourceScope outside(0);
        Check(!space::PlayerExposed(), "mob outside cannot hurt the player inside");
        {
            space::HurtSourceScope inside(4);
            Check(space::PlayerExposed(), "mob inside hurts the player inside");
        }
        Check(space::g_HurtSource == 0, "scope restores the outer source");
    }
    Check(space::g_HurtSource == space::kUntracked, "scope restores untracked");
    Check(space::PlayerExposed(), "untracked hurt is allowed");
}

static void ShotsStopAtBorders() {
    Reset();
    space::g_Regions = { {5, 0.0f, 0.0f, 100.0f, 100.0f} };
    int pane = -1;
    Check(!space::CrossedBorder(0, 150.0f, 50.0f, pane), "field shot stays in the field");
    Check(space::CrossedBorder(0, 99.0f, 50.0f, pane) && pane == 5,
          "field shot entering the pane is stopped by it");
    pane = -1;
    Check(space::CrossedBorder(5, 101.0f, 50.0f, pane) && pane == 5,
          "pane shot leaving is stopped by its own pane");
    // A pane dragged over a field shot: the shot now stands inside it.
    space::g_Regions = { {5, 120.0f, 0.0f, 220.0f, 100.0f} };
    pane = -1;
    Check(space::CrossedBorder(0, 150.0f, 50.0f, pane) && pane == 5,
          "pane dragged over a shot stops it");
    // The pane closed: its shots are left in the open field.
    space::g_Regions.clear();
    pane = -1;
    Check(space::CrossedBorder(5, 150.0f, 50.0f, pane) && pane == 5,
          "shots of a closed pane dissolve");
    Check(space::AbsorbingPane(3, 6) == 6, "pane to pane: entered pane absorbs");
}

static void SmallerPanesAreSturdier() {
    const float base = space::MaxDurability(space::kReferenceArea);
    Check(std::fabs(base - space::kBaseDurability) < 0.01f, "reference area gives base durability");
    Check(space::MaxDurability(space::kReferenceArea * 0.5f) > base, "half area is sturdier");
    Check(std::fabs(space::MaxDurability(1000.0f) - space::kBaseDurability * 3.0f) < 0.01f,
          "tiny panes cap at three times");
    Check(std::fabs(space::MaxDurability(space::kReferenceArea * 4.0f) -
                    space::kBaseDurability) < 0.01f,
          "huge panes keep the base");
}

static void NewPanesLetOlderShotsPass() {
    Reset();
    space::g_Regions = { {3, 0.0f, 0.0f, 100.0f, 100.0f, 5.0f} };
    Check(!space::Absorbs(3, 4.0f), "pane born after the shot lets it pass");
    Check(space::Absorbs(3, 5.0f), "pane stops shots fired once it stood");
    Check(space::Absorbs(0, 1.0f), "open field is never exempt");
    Check(space::Absorbs(9, 1.0f), "unknown pane keeps the old rule");
}

static void SolidBoxesNeverTrapAnything() {
    // A cascade of four overlapping panes, like the Wall pattern's stack.
    std::vector<space::Box> cascade;
    for (int i = 0; i < 4; ++i) cascade.push_back({ 300.0f + 70.0f * (float)i, 100.0f + 56.0f * (float)i, 440.0f, 320.0f });
    int trapped = 0, moved = 0;
    for (float px = 280.0f; px < 880.0f; px += 7.0f)
        for (float py = 80.0f; py < 620.0f; py += 7.0f) {
            float x = px, y = py;
            const bool wasInside = space::InsideBoxes(cascade, 14.0f, x, y);
            const bool pushed = space::PushOutOfBoxes(cascade, 14.0f, 1280.0f, 720.0f, x, y);
            if (pushed) ++moved;
            if (space::InsideBoxes(cascade, 14.0f, x, y)) ++trapped;
            if (pushed != wasInside) ++trapped;
            if (pushed && (x < 0.0f || x > 1280.0f || y < 0.0f || y > 720.0f)) ++trapped;
        }
    Check(moved > 100, "points inside the stack were pushed");
    Check(trapped == 0, "nothing is left inside the overlapping stack or thrown off the screen");
    // A pane against the screen edge: out through the side that is on the screen.
    std::vector<space::Box> edge = { { 0.0f, 100.0f, 400.0f, 300.0f } };
    float x = 100.0f, y = 250.0f;
    Check(space::PushOutOfBoxes(edge, 0.0f, 1280.0f, 720.0f, x, y) && x >= 0.0f && !space::InsideBoxes(edge, 0.0f, x, y),
          "a pane at the screen edge pushes out onto the screen");
    float ox = 1100.0f, oy = 650.0f;
    Check(!space::PushOutOfBoxes(cascade, 14.0f, 1280.0f, 720.0f, ox, oy) && ox == 1100.0f,
          "a point outside is left alone");
}

int main() {
    NoPanesAllowsEverything();
    FrontPaneOwnsOverlap();
    AttacksStayInTheirSpace();
    EnemyHitsNeedTheSameSpace();
    ShotsStopAtBorders();
    SmallerPanesAreSturdier();
    NewPanesLetOlderShotsPass();
    SolidBoxesNeverTrapAnything();
    if (g_failures == 0) std::printf("space rules: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
