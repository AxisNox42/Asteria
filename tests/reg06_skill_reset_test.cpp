// REG-06 (D06 baseline): the dash runtime reset, with every runtime field
// filled first. Run with scripts/test_reg06_skill_reset.ps1.
//   ResetPlayerSkills (PlayerRuntime.cpp): the dash/shield runtime it lists;
//     post-pick grace, dash endpoints and input edges are left alone
//     (ResetPlayerInputEdges owns the edges).
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include <cassert>
#include <cstdio>

static void FillEverything() {
    PlayerRuntimeState& r = g_PlayerRuntime;
    r.dashCooldown = 1.5f; r.dashInvulnerability = 0.1f; r.dashActive = true;
    r.dashProgress = 0.4f;
    r.knockbackVX = 50.0f; r.knockbackVY = -20.0f;
    r.dashCooldownMax = 2.9f;
    r.postPickGrace = 0.3f;
    r.dashFromX = 1.0f; r.dashFromY = 2.0f; r.dashToX = 3.0f; r.dashToY = 4.0f;
    r.dashInputHeld = true;
    g_PlayerShield = 25.0f; g_PlayerShieldTimer = 4.0f;
}

static bool KeptByReset() {
    const PlayerRuntimeState& r = g_PlayerRuntime;
    return r.postPickGrace == 0.3f && r.dashFromX == 1.0f && r.dashFromY == 2.0f &&
           r.dashToX == 3.0f && r.dashToY == 4.0f && r.dashInputHeld;
}

int main() {
    // Full reset: the listed runtime.
    FillEverything();
    ResetPlayerSkills();
    const PlayerRuntimeState& r = g_PlayerRuntime;
    assert(r.dashCooldown == 0.0f && r.dashInvulnerability == 0.0f && !r.dashActive &&
           r.dashProgress == 0.0f && r.knockbackVX == 0.0f &&
           r.knockbackVY == 0.0f && r.dashCooldownMax == 0.0f);
    assert(g_PlayerShield == 0.0f && g_PlayerShieldTimer == 0.0f);
    assert(KeptByReset());
    std::printf("ResetPlayerSkills: dash/shield cleared; "
                "grace, dash endpoints, input edges kept\n");

    // Dash cooldown max: the value each dash started with (HUD wedge base).
    ResetPlayerSkills();
    BeginPlayerDash(0.0f, 0.0f, 1.0f, 0.0f, -10000.0f, 10000.0f, -10000.0f, 10000.0f);
    assert(g_PlayerRuntime.dashCooldownMax == DASH_CD);
    assert(g_PlayerRuntime.dashCooldown == g_PlayerRuntime.dashCooldownMax);
    std::printf("BeginPlayerDash: dashCooldownMax records the dash's cooldown\n");

    // Input edges: only ResetPlayerInputEdges clears them.
    FillEverything();
    ResetPlayerInputEdges();
    assert(!g_PlayerRuntime.dashInputHeld);
    std::printf("ResetPlayerInputEdges: dash edge cleared\n");

    std::printf("REG-06 skill reset regression passed\n");
    return 0;
}
