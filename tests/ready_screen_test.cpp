// READY screen models: the practice field (Swarm dummies stay on their spot
// and respawn there, the live game's move/dash/fire/hit rules, guided step
// order) and the keyboard panel (layout sanity, roles follow the key
// bindings). Run with scripts/test_ready_screen.ps1.
#include "ReadyPractice.h"
#include "KeyboardPanelModel.h"
#include "TutorialText.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>

namespace rp = ready_practice;
namespace kb = keyboard_panel;

static constexpr float kDt = 1.0f / 120.0f;

static void StepFor(rp::Field& f, const rp::Rules& r, const rp::Input& in, float seconds) {
    for (float t = 0.0f; t < seconds; t += kDt) rp::Step(f, r, in, kDt);
}

// Aim straight at a dummy from the player's position and keep firing.
static rp::Input AimAt(const rp::Field& f, int i) {
    rp::Input in;
    in.fireHeld = true;
    in.aimValid = true;
    in.aimX = rp::DummyX(f.dummies[i]);
    in.aimY = rp::DummyY(f.dummies[i]);
    return in;
}

// Mark every step before `step` done, so its actions are unlocked.
static void UnlockTo(rp::Field& f, rp::Objective step) {
    if ((int)step > (int)rp::Objective::Move)  f.moved = rp::kMoveGoal * f.worldH;
    if ((int)step > (int)rp::Objective::Shoot) f.shots = rp::kShotGoal;
    if ((int)step > (int)rp::Objective::Dash)  f.dashes = rp::kDashGoal;
}

static int ShotsToKill(const rp::Rules& r) { return (int)std::ceil(r.dummyHp / r.bulletDamage); }

static void CheckDummiesStayAndRespawnInPlace() {
    rp::Rules r;
    rp::Field f;
    rp::Reset(f, r);
    UnlockTo(f, rp::Objective::Kill);
    const int shotsBefore = f.shots;
    const float hx = f.dummies[0].homeX, hy = f.dummies[0].homeY;
    StepFor(f, r, rp::Input{}, 5.0f);   // idle: nothing moves
    assert(f.dummies[0].homeX == hx && f.dummies[0].homeY == hy);
    assert(std::fabs(rp::DummyX(f.dummies[0]) - hx) < 1e-4f);

    for (int guard = 0; guard < 4000 && f.dummies[0].Alive(); ++guard)
        rp::Step(f, r, AimAt(f, 0), kDt);
    assert(!f.dummies[0].Alive());
    assert(f.kills == 1);
    assert(f.shots - shotsBefore >= ShotsToKill(r));   // the game's damage, not a fixed hit count
    assert(rp::FormProgress(f.dummies[0]) == 0.0f);

    StepFor(f, r, rp::Input{}, rp::kRespawnDelay - 0.2f);
    assert(!f.dummies[0].Alive());
    assert(rp::RespawnProgress(f.dummies[0]) > 0.9f);
    assert(rp::FormProgress(f.dummies[0]) > 0.0f);   // growing back
    StepFor(f, r, rp::Input{}, 0.3f);
    assert(f.dummies[0].Alive());
    assert(f.dummies[0].hp == f.dummies[0].maxHp);
    assert(std::fabs(rp::DummyX(f.dummies[0]) - hx) < 1e-3f);
    assert(std::fabs(rp::DummyY(f.dummies[0]) - hy) < 1e-3f);
}

static void CheckKnockbackSettles() {
    rp::Rules r;
    r.dummyHp = 1000.0f;
    rp::Field f;
    rp::Reset(f, r);
    UnlockTo(f, rp::Objective::Kill);
    const int shotsBefore = f.shots;
    rp::Step(f, r, AimAt(f, 3), kDt);   // one shot, then release
    assert(f.shots == shotsBefore + 1);
    for (int guard = 0; guard < 600 && f.dummies[3].hitFlash <= 0.0f; ++guard)
        rp::Step(f, r, rp::Input{}, kDt);
    assert(f.dummies[3].hp == 1000.0f - r.bulletDamage);
    assert(std::fabs(f.dummies[3].knockX) + std::fabs(f.dummies[3].knockY) > 0.0f);
    StepFor(f, r, rp::Input{}, 2.0f);
    assert(std::fabs(f.dummies[3].knockX) + std::fabs(f.dummies[3].knockY) < 1e-3f);
}

static void CheckLiveRules() {
    rp::Rules r;
    r.moveSpeed = 400.0f;
    rp::Field f;
    rp::Reset(f, r);
    const float x0 = f.playerX;
    rp::Input right;
    right.right = true;
    StepFor(f, r, right, 0.5f);   // the game's speed, in world px
    assert(std::fabs((f.playerX - x0) - 200.0f) < 4.0f);
    StepFor(f, r, right, 10.0f);  // clamped by the ship's radius
    assert(std::fabs(f.playerX - (r.worldW - r.playerRadius)) < 1e-3f);
    assert(rp::ObjectiveDone(f, rp::Objective::Move));

    // Dash: the game's distance over its duration, then its cooldown.
    rp::Field g;
    rp::Reset(g, r);
    UnlockTo(g, rp::Objective::Dash);
    rp::Input dash;
    dash.dashHeld = true;
    dash.left = true;
    const float gx = g.playerX;
    rp::Step(g, r, dash, kDt);
    assert(g.dashActive && g.dashes == 1);   // the first dash clears the step
    rp::Input hold = dash;
    StepFor(g, r, hold, r.dashDur + 0.02f);
    assert(!g.dashActive);
    // Moved one dash plus walking after it ended (no walking during it).
    assert(g.playerX < gx - r.dashDist * 0.99f);
    rp::Step(g, r, rp::Input{}, kDt);
    StepFor(g, r, dash, 0.05f);
    assert(g.dashes == 1);                    // still cooling down
    StepFor(g, r, rp::Input{}, r.dashCooldown);
    StepFor(g, r, dash, 0.05f);
    assert(g.dashes == 2);

    // Fire rate: ready at once, then one shot per interval.
    rp::Field h;
    rp::Reset(h, r);
    UnlockTo(h, rp::Objective::Shoot);
    rp::Input fire;
    fire.fireHeld = true;
    StepFor(h, r, fire, 1.0f);
    const int expected = 1 + (int)(1.0f / r.fireInterval);
    assert(std::abs(h.shots - expected) <= 1);

    // Static Field pulse damages dummies within its radius only.
    rp::Rules p = r;
    p.fieldPulse = true;
    p.pulseRadius = 50.0f;
    rp::Field q;
    rp::Reset(q, p);
    UnlockTo(q, rp::Objective::Kill);
    q.playerX = q.dummies[1].homeX + 20.0f;
    q.playerY = q.dummies[1].homeY;
    StepFor(q, p, rp::Input{}, p.pulseInterval * 2.5f);
    assert(q.dummies[1].hp < q.dummies[1].maxHp || !q.dummies[1].Alive());
    assert(q.dummies[0].hp == q.dummies[0].maxHp);
    assert(q.pulses >= 2);
}

static void CheckKillObjective() {
    rp::Rules r;
    rp::Field f;
    rp::Reset(f, r);
    UnlockTo(f, rp::Objective::Kill);
    for (int i = 0; i < rp::kDummyCount; ++i)
        for (int guard = 0; guard < 8000 && f.dummies[i].Alive(); ++guard)
            rp::Step(f, r, AimAt(f, i), kDt);
    assert(f.kills >= rp::kKillGoal);
    assert(rp::ObjectiveDone(f, rp::Objective::Kill));
}

// Actions of a later step do nothing until that step comes.
static void CheckStepGating() {
    rp::Rules r;
    rp::Field f;
    rp::Reset(f, r);
    rp::Input all = AimAt(f, 0);
    all.dashHeld = true;
    StepFor(f, r, all, 1.0f);                 // move step: no fire, no dash
    assert(f.shots == 0 && f.dashes == 0 && f.bullets.empty());

    f.moved = rp::kMoveGoal * f.worldH;       // shoot step: fire, but no dash yet
    rp::Input fireDash = AimAt(f, 0);
    fireDash.dashHeld = true;
    for (int guard = 0; guard < 2000 && f.dummies[0].hitFlash <= 0.0f; ++guard)
        rp::Step(f, r, fireDash, kDt);
    assert(f.shots > 0 && f.dashes == 0);
    assert(f.dummies[0].hp == f.dummies[0].maxHp);   // hits land but deal no damage
    StepFor(f, r, fireDash, 3.0f);
    assert(f.kills == 0 && f.dummies[0].Alive());

    f.shots = rp::kShotGoal;                  // dash step: the dash unlocks
    rp::Input dashOnly;
    dashOnly.dashHeld = true;
    rp::Step(f, r, rp::Input{}, kDt);         // release the key held through the lock
    rp::Step(f, r, dashOnly, kDt);
    assert(f.dashes == 1);

    // Dash on the start key: the dash step is skipped and stays locked.
    rp::Rules noDash = r;
    noDash.dashAvailable = false;
    rp::Field g;
    rp::Reset(g, noDash);
    UnlockTo(g, rp::Objective::Dash);
    g.dashes = 0;
    rp::Step(g, noDash, dashOnly, kDt);
    assert(g.dashes == 0);
    assert(rp::CurrentObjective(g, false) == rp::Objective::Kill);
    for (int guard = 0; guard < 8000 && g.dummies[0].Alive(); ++guard)
        rp::Step(g, noDash, AimAt(g, 0), kDt);
    assert(g.kills == 1);                     // kill step: damage is on
}

static void CheckGuidedOrder() {
    rp::Rules r;
    rp::Field f;
    rp::Reset(f, r);
    assert(rp::CurrentObjective(f, true) == rp::Objective::Move);
    f.shots = rp::kShotGoal;   // done out of order: still teaching movement
    assert(rp::CurrentObjective(f, true) == rp::Objective::Move);
    f.moved = rp::kMoveGoal * f.worldH;
    assert(rp::CurrentObjective(f, true) == rp::Objective::Dash);
    assert(rp::CurrentObjective(f, false) == rp::Objective::Kill);   // dash on the start key
    f.dashes = rp::kDashGoal;
    assert(rp::CurrentObjective(f, true) == rp::Objective::Kill);
    f.kills = rp::kKillGoal;
    assert(rp::CurrentObjective(f, true) == rp::Objective::Count);
    f.dashes = 0;
    assert(rp::CurrentObjective(f, false) == rp::Objective::Count);
}

static void CheckLayout() {
    const auto& caps = kb::Layout();
    std::set<int> seen;
    for (size_t i = 0; i < caps.size(); ++i) {
        const kb::KeyCap& a = caps[i];
        assert(seen.insert(a.key).second);   // every physical key once
        assert(a.x >= 0.0f && a.y >= 0.0f);
        assert(a.x + a.w <= kb::kLayoutW + 1e-4f && a.y + a.h <= kb::kLayoutH + 1e-4f);
        for (size_t j = i + 1; j < caps.size(); ++j) {
            const kb::KeyCap& b = caps[j];
            const bool apart = a.x + a.w <= b.x + 1e-4f || b.x + b.w <= a.x + 1e-4f ||
                               a.y + a.h <= b.y + 1e-4f || b.y + b.h <= a.y + 1e-4f;
            assert(apart);
        }
    }
    assert(caps.size() == 87);    // tenkeyless ANSI keyboard (no numpad)
    for (int k = 'A'; k <= 'Z'; ++k) assert(seen.count(k));
    for (int k = '0'; k <= '9'; ++k) assert(seen.count(k));
    assert(seen.count(kBindKeySpace) && seen.count(kBindKeyEscape));
}

static void CheckRolesFollowBindings() {
    ResetKeyBindings();
    assert(kb::RoleOf('W') == kb::KeyRole::Move);
    assert(kb::RoleOf('D') == kb::KeyRole::Move);
    assert(kb::RoleOf(kBindKeyLeftShift) == kb::KeyRole::Dash);
    assert(kb::RoleOf(kBindKeyRightShift) == kb::KeyRole::Dash);
    assert(kb::RoleOf('Q') == kb::KeyRole::Skill);
    assert(kb::RoleOf(kBindKeySpace) == kb::KeyRole::Start);
    assert(kb::RoleOf(kBindKeyEscape) == kb::KeyRole::Quit);
    assert(kb::RoleOf('F') == kb::KeyRole::None);

    AssignKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
    AssignKeyBinding(g_KeyBinds, GameAction::MoveUp, kb::key::Up);
    assert(kb::RoleOf('F') == kb::KeyRole::Dash);
    assert(kb::RoleOf(kBindKeyLeftShift) == kb::KeyRole::None);
    assert(kb::RoleOf(kb::key::Up) == kb::KeyRole::Move);
    assert(kb::RoleOf('W') == kb::KeyRole::None);
    AssignKeyBinding(g_KeyBinds, GameAction::Dash, kBindKeySpace);
    assert(kb::RoleOf(kBindKeySpace) == kb::KeyRole::Start);
    ResetKeyBindings();
}

static void CheckLabels() {
    assert(kb::CapLabel('W') == L"W");
    assert(kb::CapLabel(kBindKeyRightShift) == L"Shift");
    assert(kb::CapLabel(kb::key::Up).empty());
    assert(kb::CapArrow(kb::key::Left).x == -1.0f);
    for (const kb::KeyCap& cap : kb::Layout()) {
        const std::wstring label = kb::CapLabel(cap.key);
        assert(label.rfind(L"KEY", 0) != 0);   // no raw key codes on the panel
        if (cap.key != kBindKeySpace) assert(label.size() <= 5);
    }

    ResetKeyBindings();
    for (int lang = 0; lang < LANG_COUNT; ++lang) {
        g_Language = (Language)lang;
        for (int i = 0; i < READY_LEGEND_COUNT; ++i)
            assert(std::wstring(ReadyLegend(i)).find(L'{') == std::wstring::npos);
        for (int i = 0; i < READY_STEP_HINT_COUNT; ++i)
            assert(std::wstring(ReadyStepHint(i)).find(L'{') == std::wstring::npos);
        assert(std::wstring(ReadyBrief()).find(L"SPACE") != std::wstring::npos);
    }
    g_Language = Language::EN;
    assert(std::wstring(ReadyLegend(1)) == L"SHIFT  dash");
    AssignKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
    assert(std::wstring(ReadyLegend(1)) == L"F  dash");
    assert(std::wstring(ReadyStepHint(2)).find(L"Press F to dash") == 0);
    assert(std::wstring(ReadyLegend(4)) == L"ESC  pause");   // ESC opens the tutorial pause menu
    ResetKeyBindings();
    g_Language = Language::KR;
}

int main() {
    CheckDummiesStayAndRespawnInPlace();
    CheckKnockbackSettles();
    CheckLiveRules();
    CheckKillObjective();
    CheckStepGating();
    CheckGuidedOrder();
    CheckLayout();
    CheckRolesFollowBindings();
    CheckLabels();
    std::puts("ready screen tests passed");
    return 0;
}
