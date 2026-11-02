// READY screen: title, a small practice field (the ship plus four Swarm
// training dummies, Core/ReadyPractice.h) and a full keyboard panel whose
// caps light up by the current key bindings (Render/KeyboardPanelModel.h).
//   The field runs on the live game's rules (move speed, dash, weapon,
//   Swarm hp and hit radius), shown through a fixed view scale like a
//   zoomed-out camera, and draws the ship, Swarms and bullets with the
//   gameplay renderers.
//   The screen teaches in order (move → shoot → dash → defeat):
//   the field shows the current step's instruction, and the inputs of that
//   step are filled with their colour and pulse; finished ones settle to a
//   steady outline, later ones wait dim. Brightness eases between levels.
//   The layout is authored on a 1920x1080 canvas and scaled to fit the
//   screen. No plates sit behind the UI; the keyline under each stroke
//   keeps it readable on bright wallpapers.
#include "Scenes.h"
#include <GLFW/glfw3.h>
#include "SceneInternal.h"
#include "SceneStart.h"
#include "SceneUI.h"
#include "TutorialText.h"
#include "TextRenderer.h"
#include "DrawPrim.h"
#include "EntityDraw.h"
#include "Monster.h"
#include "Bullet.h"
#include "CollisionSystem.h"
#include "Codex.h"
#include "EnemyShield.h"
#include "PlayerRuntime.h"
#include "PlayerStats.h"
#include "Weapons.h"
#include "Input.h"
#include "SaveSystem.h"
#include "GameContext.h"
#include "../ReadyLaunch.h"
#include "../ReadyPractice.h"
#include "../../Render/HudLines.h"
#include "../../Render/KeyboardPanelModel.h"
#include "../../Render/PlayerWeaponShell.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace rp = ready_practice;
namespace kb = keyboard_panel;
using hud_lines::Rgb;

// ── Canvas layout (1920x1080 px) ──
using ready_launch::kCanvasW;
using ready_launch::kCanvasH;
constexpr float kTitleY = 68.0f, kTitlePx = 56.0f;
constexpr float kBriefY = 122.0f, kBriefPx = 24.0f;
using ready_launch::kFieldW;
using ready_launch::kFieldH;
using ready_launch::kFieldX;
using ready_launch::kFieldY;
constexpr float kFieldLabelPx = 20.0f, kFieldLabelGap = 18.0f;
constexpr float kFieldCorner = 26.0f;
constexpr float kStepLabelY = 26.0f, kStepLabelPx = 16.0f;   // inside the field, from its top
constexpr float kStepHintY = 54.0f, kStepHintPx = 25.0f;
constexpr float kStepFadeIn = 0.35f, kStepRise = 10.0f;
constexpr float kObjectivesX = kFieldX + kFieldW + 70.0f;
constexpr float kObjectiveRowH = 44.0f, kObjectivePx = 23.0f, kCheckSize = 26.0f;
constexpr float kObjectiveNotePx = 16.0f;
constexpr float kKeyPitch = 50.0f, kKeyGap = 6.0f;
constexpr float kLegendGap = 50.0f, kLegendW = 300.0f;
constexpr float kKeyboardX = (kCanvasW - (kb::kLayoutW * kKeyPitch + kLegendGap + kLegendW)) * 0.5f;
constexpr float kKeyboardY = 708.0f;
constexpr float kCaptionMinW = 1.5f;   // key units
constexpr float kKeyPx = 19.0f, kKeyLongPx = 14.0f, kKeyCaptionPx = 13.0f, kSpacePx = 20.0f;
constexpr float kLegendRowH = 34.0f, kLegendPx = 20.0f, kLegendMark = 16.0f;
constexpr float kMouseW = 56.0f, kMouseH = 86.0f, kMouseSplit = 0.40f;
constexpr float kLegendRowsY = 132.0f;   // below the mouse

// The arena in world px. At 1:1 on the 1920x1080 canvas the ship, Swarms,
// bullets, dash and field radius are exactly the size they are in a run.
constexpr float kViewScale = 1.0f;
constexpr float kWorldW = kFieldW / kViewScale, kWorldH = kFieldH / kViewScale;

constexpr float kRespawnRing = 40.0f;          // world px
constexpr float kBob = 5.0f;                   // world px
constexpr float kBobSpeed = 1.6f;
constexpr float kWobbleSpin = 20.0f;
constexpr float kShardLife = 0.4f;
constexpr float kPulseFlash = 0.22f;           // Static Field ring flash, seconds
constexpr float kHpBarW = 34.0f, kHpBarY = 46.0f;   // world px, above the Swarm

// ── Guided brightness ──
constexpr float kGlowWaiting = 0.34f;   // a later step, or keys outside the lesson
constexpr float kGlowDone    = 0.62f;   // finished step
constexpr float kGlowCurrent = 1.0f;    // the step being taught (pulses)
constexpr float kGlowEase    = 5.0f;    // per second
constexpr float kPulseSpeed  = 5.0f;
constexpr float kCurrentFillA = 0.42f;  // colour fill of the keys to press now
constexpr float kIdleKeyA    = 0.24f;   // caps with no role
constexpr float kIdleLabelA  = 0.55f;

// ── Colours ──
constexpr Rgb kCyan   { 0.55f, 0.90f, 1.00f };
constexpr Rgb kText   { 0.92f, 0.95f, 1.00f };
constexpr Rgb kWhite  { 1.00f, 1.00f, 1.00f };
constexpr Rgb kDone   { 0.59f, 0.94f, 0.67f };
constexpr Rgb kAmber  { 1.00f, 0.82f, 0.47f };
constexpr Rgb kPurple { 0.71f, 0.63f, 1.00f };
constexpr Rgb kGrey   { 0.80f, 0.80f, 0.80f };

Rgb RoleColor(kb::KeyRole role) {
    switch (role) {
    case kb::KeyRole::Move:  return kCyan;
    case kb::KeyRole::Dash:  return kAmber;
    case kb::KeyRole::Skill: return kPurple;
    case kb::KeyRole::Start: return kWhite;
    case kb::KeyRole::Quit:  return kGrey;
    default:                 return kText;
    }
}
Rgb Lighten(Rgb c, float t) { return { c.r + (1 - c.r) * t, c.g + (1 - c.g) * t, c.b + (1 - c.b) * t }; }

constexpr int kRoleCount = READY_KEY_ROLE_COUNT;
static_assert(kRoleCount == (int)kb::KeyRole::Quit + 1, "caption per key role");
static_assert(READY_LEGEND_COUNT == kRoleCount - 1, "legend line per role except None");
static_assert(READY_OBJECTIVE_COUNT == rp::kObjectiveCount, "label per objective");
static_assert(READY_STEP_HINT_COUNT == rp::kObjectiveCount + 1, "hint per step plus done");

// What the lesson lights up: the key groups and the mouse.
enum class Guide { Move, Mouse, Dash, Start, Count };
constexpr int kGuideCount = (int)Guide::Count;

struct Canvas {
    float ox, oy, k;
    float X(float x) const { return ox + x * k; }
    float Y(float y) const { return oy + y * k; }
    float S(float v) const { return v * k; }
};

Canvas FitCanvas(float sw, float sh) {
    const float k = std::min(sw / kCanvasW, sh / kCanvasH);
    return { (sw - kCanvasW * k) * 0.5f, (sh - kCanvasH * k) * 0.5f, k };
}

enum class Align { Left, Center, Right };

void Text(TextRenderer& tr, const wchar_t* s, float x, float y, float px,
          Rgb c, float a, Align align = Align::Left) {
    if (!s || !s[0] || tr.LineHeightPixels() <= 0.0f) return;
    const float scale = px / tr.LineHeightPixels();
    const float w = tr.Width(s, scale);
    if (align == Align::Center) x -= w * 0.5f;
    else if (align == Align::Right) x -= w;
    tr.Draw(s, x, y - tr.Height(s, scale) * 0.5f, scale, c.r, c.g, c.b, a);
}

// Rectangle outline with mitred corners (no double-drawn overlap).
void Box(float x, float y, float w, float h, float thick, Rgb c, float a, float keyline) {
    const float px[4] = { x, x + w, x + w, x };
    const float py[4] = { y, y, y + h, y + h };
    hud_lines::ClosedOutline(px, py, 4, thick, c, a, keyline);
}

void Fill(float x, float y, float w, float h, Rgb c, float a) { drawRect(x, y, w, h, c.r, c.g, c.b, a); }

void Line(float x0, float y0, float x1, float y1, float thick, Rgb c, float a, float keyline) {
    if (keyline > 0.0f)
        hud_lines::Segment(x0, y0, x1, y1, thick + keyline, hud_lines::KeyCol(c), hud_lines::kKeyA * a);
    hud_lines::Segment(x0, y0, x1, y1, thick, c, a);
}

// Chevron pointing right, drawn at (x, y) = its tip.
void Chevron(float x, float y, float s, float thick, Rgb c, float a, float keyline) {
    Line(x - s, y - s, x, y, thick, c, a, keyline);
    Line(x, y, x - s, y + s, thick, c, a, keyline);
}

// ── Practice state (lives while the READY screen is shown) ──
struct Shard { float x, y, vx, vy, ang, spin, life; };

struct PracticeView {
    rp::Field field;
    std::array<std::unique_ptr<Monster>, rp::kDummyCount> mobs;   // stable addresses: per-mob phase
    float swarmScale = 1.0f;     // the Swarm's own sizeScale
    float swarmHp = 1.0f;        // the Swarm's own hp
    std::array<bool, rp::kDummyCount> wasAlive{};
    std::vector<Shard> shards;
    std::array<float, kGuideCount> glow{};
    int   lastPulses = 0;
    float pulseFlash = 0.0f;
    rp::Objective shownStep = rp::Objective::Count;
    float stepAge = 0.0f;
    double lastSeen = -1.0;
};

PracticeView& View() {
    static PracticeView v;
    return v;
}

// A gap in READY frames means the screen was left and entered again.
constexpr double kReenterGap = 0.25;

// The dash cannot be practised here when it shares the start key.
bool DashPractisable() { return NormalizeBindKey(ActionKey(GameAction::Dash)) != kBindKeySpace; }

// The live game's numbers for this frame.
rp::Rules LiveRules(const PracticeView& v, bool firing) {
    rp::Rules r;
    r.worldW = kWorldW;
    r.worldH = kWorldH;
    r.playerRadius = PLAYER_SIZE_PX * g_Stats.playerSizeMult * 0.5f;
    r.moveSpeed = PLAYER_MOVE_SPEED * g_Stats.GetMoveMultiplier(firing);
    r.dashDist = DASH_DIST;
    r.dashDur = DASH_DUR;
    r.dashCooldown = DASH_CD;
    r.fireInterval = g_Stats.fireInterval * g_Stats.GetFireIntervalMult();
    r.bulletSpeed = g_Stats.bulletSpeed + g_Stats.GetBulletSpeedBonus();
    r.bulletDamage = g_Stats.GetBaseDamage() * g_Stats.GetDamageMultiplier();
    r.fieldPulse = g_CurrentWeapon == (int)StartWeapon::SMG;
    r.pulseInterval = STATIC_FIELD_PULSE_INTERVAL;
    r.pulseRadius = STATIC_FIELD_PULSE_RADIUS;
    r.pulseDamage = r.bulletDamage * STATIC_FIELD_PULSE_DAMAGE_MULT;
    r.dummyHp = v.swarmHp;
    r.dummyHitRadius = MONSTER_BULLET_HIT_RADIUS * v.swarmScale;
    r.dashAvailable = DashPractisable();
    return r;
}

void ResetView(PracticeView& v) {
    for (int i = 0; i < rp::kDummyCount; ++i) {
        if (!v.mobs[i]) {
            v.mobs[i] = std::make_unique<Monster>(0.0f, 0.0f, 1.0f, 1.0f, false);
            v.mobs[i]->MakeKind(MobKind::SWARM);
        }
        v.wasAlive[i] = true;
    }
    v.swarmScale = v.mobs[0]->sizeScale;
    v.swarmHp = v.mobs[0]->hp;
    rp::Reset(v.field, LiveRules(v, false));
    v.shards.clear();
    v.glow.fill(kGlowWaiting);
    v.lastPulses = 0;
    v.pulseFlash = 0.0f;
    v.shownStep = rp::Objective::Count;
    v.stepAge = 0.0f;
}

void TouchView(PracticeView& v) {
    // Card picks (tutorial steps 5-6) leave this scene for a while; the field
    // must come back as it was.
    const bool inPicks = ready_launch::g_TutorialMode &&
                         ready_launch::g_Stage != ready_launch::TutorialStage::Practice;
    const bool reentered = glfwGetTime() - v.lastSeen > kReenterGap && !inPicks;
    if (v.lastSeen < 0.0 || reentered) ResetView(v);
    v.lastSeen = glfwGetTime();
}

bool IsCurrent(int guide, rp::Objective current) {
    switch ((Guide)guide) {
    case Guide::Move:  return current == rp::Objective::Move;
    case Guide::Mouse: return current == rp::Objective::Shoot || current == rp::Objective::Kill;
    case Guide::Dash:  return current == rp::Objective::Dash;
    case Guide::Start: return current == rp::Objective::Count;
    default:           return false;
    }
}

// Target brightness of each guide for the lesson's current step.
std::array<float, kGuideCount> GuideTargets(const rp::Field& f, rp::Objective current, bool dashOk) {
    std::array<float, kGuideCount> t{};
    const bool done[kGuideCount] = {
        rp::ObjectiveDone(f, rp::Objective::Move),
        rp::ObjectiveDone(f, rp::Objective::Shoot),
        dashOk && rp::ObjectiveDone(f, rp::Objective::Dash),
        false,
    };
    for (int g = 0; g < kGuideCount; ++g)
        t[g] = IsCurrent(g, current) ? kGlowCurrent : done[g] ? kGlowDone : kGlowWaiting;
    if (!dashOk) t[(int)Guide::Dash] = kGlowWaiting;
    return t;
}

int GuideOfRole(kb::KeyRole role) {
    switch (role) {
    case kb::KeyRole::Move:  return (int)Guide::Move;
    case kb::KeyRole::Dash:  return (int)Guide::Dash;
    case kb::KeyRole::Start: return (int)Guide::Start;
    default:                 return -1;   // skills and Esc are outside the lesson
    }
}

struct GuideLook {
    float a;         // outline / text alpha
    bool  current;   // filled and pulsing
};

GuideLook LookOf(const PracticeView& v, int guide, rp::Objective current, float pulse) {
    if (guide < 0) return { kGlowWaiting, false };   // outside the lesson: stays quiet
    const bool isCurrent = IsCurrent(guide, current);
    return { isCurrent ? v.glow[guide] * pulse : v.glow[guide], isCurrent };
}

void SpawnShards(PracticeView& v, float x, float y, float k) {
    constexpr int kCount = 6;
    for (int i = 0; i < kCount; ++i) {
        const float a = 6.2831853f * (float)i / kCount + 0.35f * (float)(i % 2);
        const float speed = (160.0f + 30.0f * (float)(i % 3)) * k;
        v.shards.push_back({ x, y, std::cos(a) * speed, std::sin(a) * speed,
                             a, (i % 2 ? 1.0f : -1.0f) * 6.0f, kShardLife });
    }
}

// Step number among the steps that can be practised here (1-based), and
// how many there are.
void StepNumber(rp::Objective current, bool dashOk, int& index, int& total) {
    index = 0;
    total = 0;
    for (int i = 0; i < rp::kObjectiveCount; ++i) {
        if ((rp::Objective)i == rp::Objective::Dash && !dashOk) continue;
        ++total;
        if (i <= (int)current) index = total;
    }
    // The augment and nerf picks follow the practice steps (tutorial only).
    if (ready_launch::g_TutorialMode) total += READY_EXTRA_OBJECTIVE_COUNT;
}

// ── Field ──
void DrawField(PracticeView& v, const rp::Rules& rules, rp::Objective current, bool dashOk,
               const Canvas& cv, TextRenderer& body, float now, float dt) {
    const float fx = cv.X(kFieldX), fy = cv.Y(kFieldY);
    const float fw = cv.S(kFieldW), fh = cv.S(kFieldH);
    const float key = cv.S(1.5f);
    const float ws = kViewScale * cv.k;   // world px -> screen px
    auto toX = [&](float wx) { return fx + wx * ws; };
    auto toY = [&](float wy) { return fy + wy * ws; };
    const float batchAlpha = g_BatchAlpha;

    // Frame with corner brackets.
    Box(fx, fy, fw, fh, cv.S(1.3f), kCyan, 0.32f, key);
    const float L = cv.S(kFieldCorner), t = cv.S(2.4f);
    const float cx[4] = { fx, fx + fw, fx + fw, fx }, cy[4] = { fy, fy, fy + fh, fy + fh };
    const float dx[4] = { 1, -1, -1, 1 }, dy[4] = { 1, 1, -1, -1 };
    for (int i = 0; i < 4; ++i) {
        Line(cx[i] + dx[i] * L, cy[i], cx[i], cy[i], t, kCyan, 0.75f, key);
        Line(cx[i], cy[i], cx[i], cy[i] + dy[i] * L, t, kCyan, 0.75f, key);
    }
    const float labelY = fy - cv.S(kFieldLabelGap);
    const float labelPx = cv.S(kFieldLabelPx);
    Text(body, kReadyFieldTitle, fx, labelY, labelPx, kCyan, 0.85f);
    const float titleW = body.Width(kReadyFieldTitle, labelPx / body.LineHeightPixels());
    Text(body, ReadyFieldNote(), fx + titleW + cv.S(20.0f), labelY, labelPx, kText, 0.72f);

    const Rgb swarm{ v.mobs[0]->color.r, v.mobs[0]->color.g, v.mobs[0]->color.b };
    const Rgb swarmText = Lighten(swarm, 0.45f);
    const std::wstring kills = std::wstring(ReadyKills()) + L" " + std::to_wstring(v.field.kills);
    Text(body, kills.c_str(), fx + fw, labelY, labelPx + cv.S(2.0f), swarmText, 0.95f, Align::Right);
    g_BatchAlpha = batchAlpha;

    // The field stays empty while a tutorial card screen is drawn over it.
    if (!ready_launch::g_SelectionUp) {
        // Static Field: the pulse's reach flashes on each beat.
        if (rules.fieldPulse) {
            if (v.field.pulses != v.lastPulses) { v.lastPulses = v.field.pulses; v.pulseFlash = kPulseFlash; }
            v.pulseFlash = std::max(0.0f, v.pulseFlash - dt);
            const float px = toX(v.field.playerX), py = toY(v.field.playerY);
            const float rr = rules.pulseRadius * ws;
            hud_lines::Arc(px, py, rr, cv.S(1.2f), 0.0f, 6.2831853f, kCyan, 0.16f);
            if (v.pulseFlash > 0.0f)
                hud_lines::Arc(px, py, rr, cv.S(2.2f), 0.0f, 6.2831853f, kCyan,
                               0.55f * v.pulseFlash / kPulseFlash);
        }

        // Dummies: base tick, Swarm body (gameplay renderer), hp bar, respawn ring.
        const bool wasSuppressed = g_SuppressMobSeen;
        g_SuppressMobSeen = true;   // practice dummies are not Codex discoveries
        for (int i = 0; i < rp::kDummyCount; ++i) {
            const rp::Dummy& d = v.field.dummies[i];
            const float hx = toX(d.homeX), hy = toY(d.homeY);
            const float base = (rules.dummyHitRadius + 14.0f) * ws;
            Line(hx - cv.S(14), hy + base, hx + cv.S(14), hy + base, cv.S(1.6f), swarm, 0.45f, key);
            Line(hx, hy + base - cv.S(5), hx, hy + base + cv.S(5), cv.S(1.6f), swarm, 0.45f, key);

            if (v.wasAlive[i] && !d.Alive())
                SpawnShards(v, toX(rp::DummyX(d)), toY(rp::DummyY(d)), cv.k);
            v.wasAlive[i] = d.Alive();

            Monster& mob = *v.mobs[i];
            const float bob = std::sin(now * kBobSpeed + 1.9f * (float)i) * kBob;
            float form = 1.0f;
            if (d.Alive()) {
                mob.worldX = toX(rp::DummyX(d));
                mob.worldY = toY(rp::DummyY(d) + bob);
                mob.hitFlashTimer = d.hitFlash / rp::kHitFlash * MOB_HIT_FLASH_TIME;
                const float bw = kHpBarW * ws, by = hy - kHpBarY * ws;
                const float frac = std::clamp(d.hp / std::max(1e-3f, d.maxHp), 0.0f, 1.0f);
                Line(hx - bw * 0.5f, by, hx + bw * 0.5f, by, cv.S(2.6f), swarmText, 0.22f, key);
                if (frac > 0.0f)
                    Line(hx - bw * 0.5f, by, hx - bw * 0.5f + bw * frac, by, cv.S(2.6f), swarmText, 0.95f, 0.0f);
            } else {
                const float rr = kRespawnRing * ws;
                hud_lines::Arc(hx, hy, rr, cv.S(3.5f), 0.0f, 6.2831853f,
                               hud_lines::KeyCol(swarm), hud_lines::kKeyA * 0.6f);
                hud_lines::Arc(hx, hy, rr, cv.S(2.0f), 0.0f, 6.2831853f, swarm, 0.25f);
                hud_lines::Arc(hx, hy, rr, cv.S(2.4f), -1.5707963f,
                               6.2831853f * rp::RespawnProgress(d), swarmText, 0.9f);
                form = rp::FormProgress(d);
                mob.worldX = hx;
                mob.worldY = hy;
                mob.hitFlashTimer = 0.0f;
                if (form <= 0.0f) {
                    const float s = cv.S(6.0f);
                    Line(hx - s, hy, hx + s, hy, cv.S(1.8f), swarmText, 0.7f, key);
                    Line(hx, hy - s, hx, hy + s, cv.S(1.8f), swarmText, 0.7f, key);
                    continue;
                }
            }
            mob.sizeScale = v.swarmScale * ws * form;
            const float savedAlpha = g_BatchAlpha;
            g_BatchAlpha = savedAlpha * (d.Alive() ? 1.0f : 0.25f + 0.5f * form);
            drawMob(&mob, now + std::sin(now * kWobbleSpin) * d.wobble);
            g_BatchAlpha = savedAlpha;
        }
        g_SuppressMobSeen = wasSuppressed;

        for (Shard& s : v.shards) {
            s.x += s.vx * dt; s.y += s.vy * dt; s.ang += s.spin * dt; s.life -= dt;
            const float hl = cv.S(8.0f), ca = std::cos(s.ang) * hl, sa = std::sin(s.ang) * hl;
            Line(s.x - ca, s.y - sa, s.x + ca, s.y + sa, cv.S(2.2f), swarmText,
                 std::max(0.0f, s.life / kShardLife), 0.0f);
        }
        v.shards.erase(std::remove_if(v.shards.begin(), v.shards.end(),
            [](const Shard& s) { return s.life <= 0.0f; }), v.shards.end());

        // Bullets and ship with the gameplay renderers, at the view scale.
        for (const rp::Bullet& b : v.field.bullets) {
            ::Bullet shot(toX(b.x), toY(b.y), toX(b.x) + b.dirX, toY(b.y) + b.dirY);
            shot.speed = rules.bulletSpeed * ws;
            shot.sizeScale = ws;
            drawBullet(shot);
        }
        // The ship at its spot in the field, at the run's size scaled by the view.
        const float shipRunSize = PLAYER_SIZE_PX * g_Stats.playerSizeMult;
        DrawPlayerWeaponShell(toX(v.field.playerX), toY(v.field.playerY),
                              shipRunSize * ws, v.field.aim, now);
    }

    // The current step's instruction, inside the field.
    if (current != v.shownStep) { v.shownStep = current; v.stepAge = 0.0f; }
    v.stepAge += dt;
    const float in = std::clamp(v.stepAge / kStepFadeIn, 0.0f, 1.0f);
    const float rise = cv.S(kStepRise) * (1.0f - in);
    const float mid = fx + fw * 0.5f;
    using Stage = ready_launch::TutorialStage;
    const Stage stage = ready_launch::g_TutorialMode ? ready_launch::g_Stage : Stage::Practice;
    const wchar_t* hint = ReadyStepHint((int)current);
    if (stage != Stage::Practice && stage != Stage::Done) {
        // Steps 5-6: the labels follow the practice steps.
        int index = 0, total = 0;
        StepNumber(rp::Objective::Count, dashOk, index, total);
        const int step5 = total - READY_EXTRA_OBJECTIVE_COUNT + 1;
        const bool augStep = stage == Stage::AugPick;
        const std::wstring label = std::wstring(kReadyStepLabel) + L" " +
            std::to_wstring(augStep ? step5 : step5 + 1) + L" / " + std::to_wstring(total);
        Text(body, label.c_str(), mid, fy + cv.S(kStepLabelY) + rise, cv.S(kStepLabelPx), kCyan, 0.9f * in,
             Align::Center);
        hint = augStep ? ReadyExtraHint(0)
             : stage == Stage::NerfWait
                 ? ReadyExtraHint(1, (int)std::ceil(std::max(0.0f, ready_launch::g_NerfClock)))
                 : ReadyExtraHint(2);
    } else if (stage == Stage::Practice && current != rp::Objective::Count) {
        int index = 0, total = 0;
        StepNumber(current, dashOk, index, total);
        const std::wstring step = std::wstring(kReadyStepLabel) + L" " + std::to_wstring(index) +
                                  L" / " + std::to_wstring(total);
        Text(body, step.c_str(), mid, fy + cv.S(kStepLabelY) + rise, cv.S(kStepLabelPx), kCyan, 0.9f * in,
             Align::Center);
    }
    Text(body, hint, mid, fy + cv.S(kStepHintY) + rise, cv.S(kStepHintPx),
         kWhite, in, Align::Center);
    g_BatchAlpha = batchAlpha;
}

void DrawObjectives(const rp::Field& f, rp::Objective current, bool dashOk,
                    const Canvas& cv, TextRenderer& body, float pulse) {
    const float x = cv.X(kObjectivesX), y0 = cv.Y(kFieldY + 10.0f);
    const float key = cv.S(1.5f);
    Text(body, kReadyObjectivesTitle, x, y0, cv.S(20.0f), kCyan, 0.85f);
    float y = y0 + cv.S(46.0f);
    for (int i = 0; i < rp::kObjectiveCount; ++i) {
        const rp::Objective o = (rp::Objective)i;
        const bool skipped = o == rp::Objective::Dash && !dashOk;
        const bool done = !skipped && rp::ObjectiveDone(f, o);
        const bool isCurrent = o == current;
        const Rgb c = done ? kDone : kCyan;
        const float boxA = done ? 0.95f : isCurrent ? pulse : skipped ? 0.3f : 0.45f;
        const float s = cv.S(kCheckSize);
        if (isCurrent) Chevron(x - cv.S(12.0f), y, cv.S(7.0f), cv.S(2.4f), kCyan, pulse, key);
        Box(x, y - s * 0.5f, s, s, cv.S(isCurrent ? 2.6f : 2.0f), c, boxA, key);
        if (done) {
            Line(x + s * 0.23f, y, x + s * 0.42f, y + s * 0.23f, cv.S(2.4f), c, 0.95f, key);
            Line(x + s * 0.42f, y + s * 0.23f, x + s * 0.81f, y - s * 0.27f, cv.S(2.4f), c, 0.95f, key);
        }
        const float textA = done ? 0.95f : isCurrent ? 1.0f : skipped ? 0.4f : 0.6f;
        Text(body, ReadyObjective(i), x + s + cv.S(14.0f), y, cv.S(kObjectivePx),
             done ? c : isCurrent ? kWhite : kText, textA);
        if (o == rp::Objective::Kill) {
            const std::wstring prog = std::to_wstring(std::min(f.kills, rp::kKillGoal)) + L"/" +
                                      std::to_wstring(rp::kKillGoal);
            Text(body, prog.c_str(), x + cv.S(280.0f), y, cv.S(21.0f), c,
                 done || isCurrent ? 0.95f : 0.6f, Align::Right);
        }
        if (skipped) {
            y += cv.S(24.0f);
            Text(body, ReadyDashOnStart(), x + s + cv.S(14.0f), y, cv.S(kObjectiveNotePx), kAmber, 0.8f);
        }
        y += cv.S(kObjectiveRowH);
    }
    using Stage = ready_launch::TutorialStage;
    const bool tutorial = ready_launch::g_TutorialMode;
    const Stage stage = ready_launch::g_Stage;
    if (tutorial) {
        // Steps 5-6: the real augment and nerf picks.
        for (int i = 0; i < READY_EXTRA_OBJECTIVE_COUNT; ++i) {
            const bool aug = i == 0;
            const bool done = aug ? (stage == Stage::NerfWait || stage == Stage::NerfPick || stage == Stage::Done)
                                  : stage == Stage::Done;
            const bool isCurrent = aug ? stage == Stage::AugPick
                                       : (stage == Stage::NerfWait || stage == Stage::NerfPick);
            const Rgb c = done ? kDone : kCyan;
            const float boxA = done ? 0.95f : isCurrent ? pulse : 0.45f;
            const float s = cv.S(kCheckSize);
            if (isCurrent) Chevron(x - cv.S(12.0f), y, cv.S(7.0f), cv.S(2.4f), kCyan, pulse, key);
            Box(x, y - s * 0.5f, s, s, cv.S(isCurrent ? 2.6f : 2.0f), c, boxA, key);
            if (done) {
                Line(x + s * 0.23f, y, x + s * 0.42f, y + s * 0.23f, cv.S(2.4f), c, 0.95f, key);
                Line(x + s * 0.42f, y + s * 0.23f, x + s * 0.81f, y - s * 0.27f, cv.S(2.4f), c, 0.95f, key);
            }
            const float textA = done ? 0.95f : isCurrent ? 1.0f : 0.6f;
            Text(body, ReadyExtraObjective(i), x + s + cv.S(14.0f), y, cv.S(kObjectivePx),
                 done ? c : isCurrent ? kWhite : kText, textA);
            if (!aug && stage == Stage::NerfWait) {
                const std::wstring clock = L"0:0" + std::to_wstring(
                    (int)std::ceil(std::max(0.0f, ready_launch::g_NerfClock)));
                Text(body, clock.c_str(), x + cv.S(280.0f), y, cv.S(21.0f), kCyan, 0.95f, Align::Right);
            }
            y += cv.S(kObjectiveRowH);
        }
    }
    if (tutorial ? stage == Stage::Done : current == rp::Objective::Count)
        Text(body, ReadyAllDone(), x, y + cv.S(4.0f), cv.S(kObjectivePx), kWhite, pulse);
}

// ── Keyboard panel ──
bool CapHeld(int glfwKey) { return glfwKey >= 0 && glfwKey < 1024 && keys[glfwKey]; }

void DrawMouse(float x, float y, const Canvas& cv, const GuideLook& look, bool lmb, float now) {
    const float key = cv.S(1.5f);
    const float w = cv.S(kMouseW), h = cv.S(kMouseH), split = h * kMouseSplit;
    const float a = lmb ? 1.0f : look.a;
    // The left button is what the lesson asks for: filled while it is the step.
    if (look.current || lmb)
        Fill(x, y, w * 0.5f, split, kCyan, lmb ? 0.7f : kCurrentFillA * look.a + 0.18f);
    Box(x, y, w, h, cv.S(look.current ? 2.8f : 2.0f), kCyan, a, key);
    Line(x + w * 0.5f, y, x + w * 0.5f, y + split, cv.S(2.0f), kCyan, a, key);
    Line(x, y + split, x + w, y + split, cv.S(2.0f), kCyan, a, key);
    if (look.current) {
        // Click ripples rising off the left button.
        const float t = std::fmod(now * 1.4f, 1.0f);
        const float bx = x + w * 0.25f;
        for (int i = 0; i < 2; ++i) {
            const float r = cv.S(12.0f + 10.0f * i) + cv.S(8.0f) * t;
            hud_lines::Arc(bx, y, r, cv.S(2.2f), -2.5f, 1.4f, kCyan, (1.0f - t) * (i ? 0.55f : 0.95f));
        }
    }
}

void DrawKeyboard(const PracticeView& v, rp::Objective current, const Canvas& cv,
                  TextRenderer& body, float pulse, bool lmb, float now) {
    const float key = cv.S(1.5f);
    std::array<bool, kRoleCount> roleHeld{};

    for (const kb::KeyCap& cap : kb::Layout()) {
        const float x = cv.X(kKeyboardX + cap.x * kKeyPitch);
        const float y = cv.Y(kKeyboardY + cap.y * kKeyPitch);
        const float w = cv.S(cap.w * kKeyPitch - kKeyGap);
        const float h = cv.S(cap.h * kKeyPitch - kKeyGap);
        const kb::KeyRole role = kb::RoleOf(cap.key);
        const bool on = CapHeld(cap.key);
        const bool active = role != kb::KeyRole::None;
        if (on) roleHeld[(int)role] = true;

        const GuideLook look = LookOf(v, GuideOfRole(role), current, pulse);
        const Rgb c = on && !active ? kWhite : RoleColor(role);
        const float a = on ? 1.0f : active ? look.a : kIdleKeyA;
        const float ta = on || look.current ? 1.0f : active ? std::min(1.0f, look.a + 0.2f) : kIdleLabelA;
        // Keys to press now are filled with their colour; a press fills any key.
        if (look.current || on)
            Fill(x, y, w, h, c, on ? 0.6f : kCurrentFillA * look.a + 0.12f);
        Box(x, y, w, h, cv.S(on || look.current ? 2.6f : active ? 2.0f : 1.4f), c, a, key);

        const Rgb tc = on || look.current ? kWhite : c;
        const float mx = x + w * 0.5f, my = y + h * 0.5f;
        if (role == kb::KeyRole::Start) {
            Text(body, ReadyLegend((int)role - 1), mx, my, cv.S(kSpacePx), tc, ta, Align::Center);
            continue;
        }
        const kb::ArrowDir dir = kb::CapArrow(cap.key);
        if (dir.x != 0.0f || dir.y != 0.0f) {
            const float r = cv.S(9.0f), px = -dir.y, py = dir.x;
            const float tx = mx + dir.x * r, ty = my + dir.y * r;
            const float lw = cv.S(2.0f);
            Line(mx - dir.x * r, my - dir.y * r, tx, ty, lw, tc, ta, key);
            Line(tx, ty, tx - dir.x * r * 0.6f + px * r * 0.6f, ty - dir.y * r * 0.6f + py * r * 0.6f, lw, tc, ta, key);
            Line(tx, ty, tx - dir.x * r * 0.6f - px * r * 0.6f, ty - dir.y * r * 0.6f - py * r * 0.6f, lw, tc, ta, key);
            continue;
        }
        const std::wstring label = kb::CapLabel(cap.key);
        // Captions only on wide caps; a 1-unit cap has no room and the
        // legend already names every role.
        const wchar_t* caption = ReadyKeyCaption((int)role);
        const bool hasCaption = caption[0] != 0 && cap.w >= kCaptionMinW;
        // Shrink text that would touch the cap's outline.
        auto fitPx = [&](const wchar_t* s, float px) {
            const float room = w - cv.S(8.0f);
            const float tw = body.Width(s, px / body.LineHeightPixels());
            return tw > room && tw > 0.0f ? px * room / tw : px;
        };
        const float labelPx = cv.S(label.size() > 2 ? kKeyLongPx : kKeyPx);
        Text(body, label.c_str(), mx, my - (hasCaption ? cv.S(8.0f) : 0.0f),
             fitPx(label.c_str(), labelPx), tc, ta, Align::Center);
        if (hasCaption)
            Text(body, caption, mx, my + cv.S(9.0f), fitPx(caption, cv.S(kKeyCaptionPx)),
                 tc, ta, Align::Center);
    }

    // Legend: mouse, then one line per key role in the same colours and levels.
    const float lx = cv.X(kKeyboardX + kb::kLayoutW * kKeyPitch + kLegendGap);
    const float ly = cv.Y(kKeyboardY + 6.0f);
    const GuideLook mouse = LookOf(v, (int)Guide::Mouse, current, pulse);
    DrawMouse(lx, ly, cv, mouse, lmb, now);
    Text(body, ReadyAim(), lx + cv.S(kMouseW + 16.0f), ly + cv.S(kMouseH * kMouseSplit * 0.5f),
         cv.S(kLegendPx), mouse.current ? kWhite : kCyan, mouse.current ? 1.0f : std::min(1.0f, mouse.a + 0.2f));

    for (int role = (int)kb::KeyRole::Move; role < kRoleCount; ++role) {
        const GuideLook look = LookOf(v, GuideOfRole((kb::KeyRole)role), current, pulse);
        const Rgb c = RoleColor((kb::KeyRole)role);
        const float y = ly + cv.S(kLegendRowsY + kLegendRowH * (role - 1));
        const float s = cv.S(kLegendMark);
        const float a = roleHeld[role] ? 1.0f : look.a;
        if (roleHeld[role] || look.current) Fill(lx, y - s * 0.5f, s, s, c, 0.75f * a);
        Box(lx, y - s * 0.5f, s, s, cv.S(look.current ? 2.6f : 2.0f), c, a, key);
        Text(body, ReadyLegend(role - 1), lx + s + cv.S(12.0f), y, cv.S(kLegendPx),
             look.current ? kWhite : kText, look.current ? 1.0f : std::min(1.0f, a + 0.25f));
    }
}

} // namespace

void Ready_SmokeCompleteSteps(int steps) {
    PracticeView& v = View();
    TouchView(v);
    rp::Field& f = v.field;
    if (steps > 0) f.moved = rp::kMoveGoal * f.worldH;
    if (steps > 1) f.shots = rp::kShotGoal;
    if (steps > 2) f.dashes = rp::kDashGoal;
    if (steps > 3) f.kills = rp::kKillGoal;
}

void Scene_Ready(const SceneCtx& c, const SceneTextContext& text) {
    const Canvas cv = FitCanvas(c.sw, c.sh);
    const float now = (float)glfwGetTime();
    const float dt = std::clamp(c.delta, 0.0f, 0.05f);

    PracticeView& v = View();
    TouchView(v);
    const rp::Rules rules = LiveRules(v, c.lmb);

    // Input goes through the bindings; the field only sees actions.
    rp::Input in;
    const MoveInput mv = ResolvedMoveHeld();
    in.up    = mv.up;
    in.down  = mv.down;
    in.left  = mv.left;
    in.right = mv.right;
    in.dashHeld = ActionHeld(GameAction::Dash);
    in.fireHeld = c.lmb;
    in.aimValid = true;
    in.aimX = ((float)c.mx - cv.X(kFieldX)) / (kViewScale * cv.k);
    in.aimY = ((float)c.my - cv.Y(kFieldY)) / (kViewScale * cv.k);
    // While a card screen is up this scene only draws; the field stands still.
    const bool picking = ready_launch::g_SelectionUp;
    if (picking) in = rp::Input{};
    rp::Step(v.field, rules, in, picking ? 0.0f : dt);

    const bool dashOk = rules.dashAvailable;
    const rp::Objective current = rp::CurrentObjective(v.field, dashOk);
    if (ready_launch::g_TutorialMode && !picking) {
        using Stage = ready_launch::TutorialStage;
        switch (ready_launch::g_Stage) {
        case Stage::Practice:
            if (current == rp::Objective::Count) {
                // The fourth kill is the level-up: open the real augment cards.
                ready_launch::g_Stage = Stage::AugPick;
                GameManager& gm = g_GameManager;
                ++gm.playerLevel;
                gm.xp = 0;
                // The tutorial offers three fixed, simple cards.
                static const AugType kCards[3] = {
                    AugType::TUT_ATK_UP, AugType::TUT_HP_UP, AugType::TUT_MOVE_UP };
                gm.OpenFixedChoices(kCards, 3, false);
            }
            break;
        case Stage::AugPick:
            // Back on this screen: the card was taken. The wave clock starts.
            ready_launch::g_Stage = Stage::NerfWait;
            ready_launch::g_NerfClock = ready_launch::kNerfWaitSeconds;
            break;
        case Stage::NerfWait:
            ready_launch::g_NerfClock -= dt;
            if (ready_launch::g_NerfClock <= 0.0f) {
                // The clock ran out: judge it as a wave boundary (main opens the nerf cards).
                ready_launch::g_NerfClock = 0.0f;
                ready_launch::g_Stage = Stage::NerfPick;
                ready_launch::g_RequestNerfPick = true;
            }
            break;
        case Stage::NerfPick:
            if (!ready_launch::g_RequestNerfPick) {
                // Back on this screen: the nerf was taken. Finishing every step
                // unlocks the rest of the lobby.
                ready_launch::g_Stage = Stage::Done;
                if (!g_TutorialDone) {
                    g_TutorialDone = true;
                    SaveGame();
                }
            }
            break;
        case Stage::Done:
            break;
        }
    }
    const auto targets = GuideTargets(v.field, current, dashOk);
    const float ease = 1.0f - std::exp(-dt * kGlowEase);
    for (int i = 0; i < kGuideCount; ++i) v.glow[i] += (targets[i] - v.glow[i]) * ease;
    const float pulse = 0.72f + 0.28f * std::sin(now * kPulseSpeed);

    BindMainShader();
    Text(text.title, ReadyTitle(), cv.X(kCanvasW * 0.5f), cv.Y(kTitleY), cv.S(kTitlePx),
         kCyan, 0.98f, Align::Center);
    Text(text.body, ReadyBrief(), cv.X(kCanvasW * 0.5f), cv.Y(kBriefY), cv.S(kBriefPx),
         kText, 0.9f, Align::Center);

    BindMainShader();
    DrawField(v, rules, current, dashOk, cv, text.body, now, dt);
    DrawObjectives(v.field, current, dashOk, cv, text.body, pulse);
    if (ready_launch::g_TutorialMode) {
        // Explain the system the current step is about, under the field.
        using Stage = ready_launch::TutorialStage;
        const Stage stage = ready_launch::g_Stage;
        const int block = stage == Stage::AugPick ? 0
                        : (stage == Stage::NerfWait || stage == Stage::NerfPick) ? 1 : -1;
        if (block >= 0) {
            const float maxW = cv.S(kFieldW + 2.0f * 150.0f);
            const float cx = cv.X(kCanvasW * 0.5f);
            float y = cv.Y(kFieldY + kFieldH + 26.0f);
            for (int l = 0; l < 2; ++l) {
                const wchar_t* t = ReadyPickNote(block, l);
                float px = cv.S(l == 0 ? 22.0f : 20.0f);
                const float w = text.body.Width(t, px / text.body.LineHeightPixels());
                if (w > maxW) px *= maxW / w;
                Text(text.body, t, cx, y, px, l == 0 ? kWhite : kText, l == 0 ? 0.95f : 0.8f, Align::Center);
                y += cv.S(28.0f);
            }
        }
    }
    DrawKeyboard(v, current, cv, text.body, pulse, c.lmb, now);
    BatchFlush();
}
