// The laser beam stops at OCTANS panes like any shot (WiNILL/Render/LaserFx.h ClipToSpace).
#include "LaserFx.h"

#include <cmath>
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
    space::g_Absorbed.clear();
}

static void AddPane(int id, float x0, float y0, float x1, float y1) {
    space::Region r;
    r.id = id; r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
    r.px0 = x0; r.py0 = y0; r.px1 = x1; r.py1 = y1;
    space::g_Regions.push_back(r);
}

int main() {
    const laser_fx::Rect arena{ 0.0f, 2000.0f, 0.0f, 1000.0f };
    std::vector<glm::vec2> pts;
    int pane = -1;

    // No panes: the beam runs its full range.
    Reset();
    laser_fx::Path(100.0f, 500.0f, 1.0f, 0.0f, 760.0f, 0, arena, pts, &pane);
    Check(pane == 0 && pts.size() == 2 && std::fabs(pts.back().x - 860.0f) < 0.5f, "no panes: full range");

    // A pane ahead in the open field stops the beam at its left edge.
    Reset();
    AddPane(4, 400.0f, 300.0f, 700.0f, 700.0f);
    laser_fx::Path(100.0f, 500.0f, 1.0f, 0.0f, 760.0f, 0, arena, pts, &pane);
    Check(pane == 4, "field beam into a pane: stopped by that pane");
    Check(std::fabs(pts.back().x - 400.0f) < 1.0f, "field beam ends at the pane's edge");

    // Fired from inside a pane: it ends where it leaves the pane.
    laser_fx::Path(500.0f, 500.0f, 1.0f, 0.0f, 760.0f, 0, arena, pts, &pane);
    Check(pane == 4, "beam leaving its pane: stopped by that pane");
    Check(std::fabs(pts.back().x - 700.0f) < 1.0f, "beam inside a pane ends at its edge");

    // A beam that misses the pane is not cut.
    laser_fx::Path(100.0f, 100.0f, 1.0f, 0.0f, 760.0f, 0, arena, pts, &pane);
    Check(pane == 0 && std::fabs(pts.back().x - 860.0f) < 0.5f, "beam past the pane keeps its range");

    // A bounced leg that turns into the pane is cut on that leg.
    Reset();
    AddPane(7, 0.0f, 600.0f, 2000.0f, 1000.0f);   // the lower band of the arena
    laser_fx::Path(100.0f, 300.0f, 0.0f, -1.0f, 1200.0f, 1, arena, pts, &pane);
    Check(pane == 7, "bounced beam: stopped by the pane on its second leg");
    Check(pts.size() == 3 && std::fabs(pts.back().y - 600.0f) < 1.0f, "bounced beam ends at the pane's edge");

    // Wearing: a stopped beam records one absorb on that pane, as the player's.
    space::g_Absorbed.clear();
    laser_fx::WearPane(pane, pts);
    Check(space::g_Absorbed.size() == 1 && space::g_Absorbed[0].paneId == 7 && !space::g_Absorbed[0].enemy,
          "a stopped beam wears the pane once");
    laser_fx::WearPane(0, pts);
    Check(space::g_Absorbed.size() == 1, "an unstopped beam wears nothing");

    // Turning: the standing beam speeds up while it keeps turning and starts over once it stops.
    laser_fx::Clear();
    laser_fx::Hold(true, 0.0f, 0.0f, 100.0f, 0.0f, 0.5f, 1);   // charges and opens toward +x
    Check(laser_fx::g_Main.on, "beam stands");
    const float dt = 1.0f / 60.0f;
    float prev = laser_fx::g_Main.aim, firstStep = 0.0f, lastStep = 0.0f;
    for (int i = 0; i < 60; ++i) {   // the aim sits behind the ship: a long turn
        laser_fx::Hold(true, 0.0f, 0.0f, -100.0f, 1.0f, dt, 1);
        float d = std::fabs(laser_fx::g_Main.aim - prev);
        if (d > 3.0f) d = 6.2831853f - d;
        if (i == 0) firstStep = d;
        lastStep = d; prev = laser_fx::g_Main.aim;
    }
    Check(lastStep > firstStep * 1.2f, "the beam turns faster the longer it keeps turning");
    Check(laser_fx::g_Main.turnSpeed > laser_fx::kTurnRate, "turn speed has grown");
    for (int i = 0; i < 200; ++i) laser_fx::Hold(true, 0.0f, 0.0f, -100.0f, 1.0f, dt, 1);   // arrives and stays
    Check(laser_fx::g_Main.turnSpeed == 0.0f && laser_fx::g_Main.turnAccel == laser_fx::kTurnAccel,
          "turn speed and its speed-up start over once the beam has stopped");

    if (g_failures == 0) std::printf("laser panes: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
