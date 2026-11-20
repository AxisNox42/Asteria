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
#include "Translations.h"
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
#include "../../Render/VitalsHud.h"
#include "RangeUnit.h"
#include "../../System/Audio.h"
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
constexpr float kTitleY = 56.0f, kTitlePx = 56.0f;
using ready_launch::kFieldW;
using ready_launch::kFieldH;
using ready_launch::kFieldX;
using ready_launch::kFieldY;
constexpr float kFieldCorner = 26.0f;
constexpr float kStepLabelY = 28.0f;   // top margin inside the field
constexpr float kStepHintY = 60.0f, kStepHintPx = 30.0f;
constexpr float kStepFadeIn = 0.35f, kStepRise = 10.0f;
constexpr float kObjectivesX = kFieldX + kFieldW + 40.0f;
constexpr float kObjectiveRowH = 48.0f, kObjectivePx = 26.0f, kCheckSize = 26.0f;
constexpr float kObjectiveNotePx = 18.0f;
constexpr float kNotePx = 21.0f;   // the ring and XP notes inside the field
// Beta testers on small windows could not read the page: the canvas shrinks
// with the window, but text never goes below these on-screen sizes.
constexpr float kMinTextPx = 16.0f, kMinSmallPx = 14.0f;
constexpr float kKeyPitch = 44.0f, kKeyGap = 5.0f;
// Legend right of the keyboard: the mouse, and beside it the aim line and one
// line per key role.
constexpr float kMouseW = 118.0f, kMouseH = 180.0f, kMouseSplit = 0.40f;
constexpr float kLegendGap = 110.0f, kLegendW = kMouseW;
constexpr float kKeyboardX = (kCanvasW - (kb::kLayoutW * kKeyPitch + kLegendGap + kLegendW)) * 0.5f;
constexpr float kKeyboardY = 780.0f;
constexpr float kCaptionMinW = 1.5f;   // key units
constexpr float kKeyPx = 19.0f, kKeyLongPx = 14.0f, kKeyCaptionPx = 15.0f, kSpacePx = 20.0f;

// The arena in world px. At 1:1 on the 1920x1080 canvas the ship, Swarms,
// bullets, dash and field radius are exactly the size they are in a run.
constexpr float kViewScale = 1.0f;
constexpr float kWorldW = kFieldW / kViewScale, kWorldH = kFieldH / kViewScale;

// How far the ship's picture (DrawPlayerWeaponShell) reaches from its centre,
// in ship sizes: the field keeps the ship's centre this far inside.
constexpr float kShipDrawReach = 1.8f;
// The HP ring fades to this alpha once it crosses the field's frame.
constexpr float kRingOutA = 0.35f;
constexpr float kRingFadePx = 10.0f;           // canvas px over which it fades

constexpr float kRespawnRing = 40.0f;          // world px
constexpr float kBob = 5.0f;                   // world px
constexpr float kBobSpeed = 1.6f;
constexpr float kWobbleSpin = 20.0f;
constexpr float kShardLife = 0.4f;
constexpr float kPulseFlash = 0.22f;           // Static Field ring flash, seconds
constexpr float kXpFlash = 0.6f;               // XP line flash on the level-up, seconds
constexpr float kHpBarW = 34.0f, kHpBarY = 46.0f;   // world px, above the Swarm

// ── Guided brightness ──
constexpr float kGlowWaiting = 0.34f;   // a later step, or keys outside the lesson
constexpr float kGlowDone    = 0.62f;   // finished step
constexpr float kGlowCurrent = 1.0f;    // the step being taught (pulses)
constexpr float kGlowEase    = 5.0f;    // per second
constexpr float kPulseSpeed  = 5.0f;
constexpr float kCurrentFillA = 0.42f;  // colour fill of the keys to press now
constexpr float kGhostKeyA   = 0.05f;   // caps with no role: a bare hint of the board, so the used keys stand out

// ── Colours ──
constexpr Rgb kCyan   { 0.55f, 0.90f, 1.00f };
constexpr Rgb kText   { 0.92f, 0.95f, 1.00f };
constexpr Rgb kWhite  { 1.00f, 1.00f, 1.00f };
constexpr Rgb kDone   { 0.59f, 0.94f, 0.67f };
constexpr Rgb kAmber  { 1.00f, 0.82f, 0.47f };
constexpr Rgb kPurple { 0.71f, 0.63f, 1.00f };
constexpr Rgb kGrey   { 0.80f, 0.80f, 0.80f };
constexpr Rgb kGold   { 1.00f, 0.86f, 0.40f };   // guide highlight (the shop tour's look)

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
static_assert(READY_STEP_CARD_COUNT ==
              (int)ready_launch::HudGuide::Launch - (int)ready_launch::HudGuide::Move + 1, "text per step card");

// What the lesson lights up: the key groups and the mouse.
enum class Guide { Move, Mouse, Dash, Start, Count };
constexpr int kGuideCount = (int)Guide::Count;

struct Canvas {
    float ox, oy, k;
    float X(float x) const { return ox + x * k; }
    float Y(float y) const { return oy + y * k; }
    float S(float v) const { return v * k; }
    // Text size: scaled like the layout, but never under the readable floor.
    float T(float v, float floor = kMinTextPx) const { return std::max(v * k, std::min(v, floor)); }
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

// Title with a constellation line on each side that ends in a node, the same
// anchor motif as the lobby commands.
void DrawTitle(const Canvas& cv, TextRenderer& title, float a) {
    const float cx = cv.X(kCanvasW * 0.5f), cy = cv.Y(kTitleY);
    const float px = cv.S(kTitlePx);
    Text(title, ReadyTitle(), cx, cy, px, kCyan, 0.98f * a, Align::Center);
    if (title.LineHeightPixels() <= 0.0f) return;
    const float halfW = title.Width(ReadyTitle(), px / title.LineHeightPixels()) * 0.5f;
    const float key = cv.S(1.5f), gap = cv.S(34.0f), len = cv.S(210.0f), r = cv.S(5.0f);
    for (int side = -1; side <= 1; side += 2) {
        const float x0 = cx + side * (halfW + gap), x1 = x0 + side * len;
        // Bright at the title, fading to the node.
        const int kSeg = 6;
        for (int i = 0; i < kSeg; ++i) {
            const float t0 = (float)i / kSeg, t1 = (float)(i + 1) / kSeg;
            Line(x0 + (x1 - x0) * t0, cy, x0 + (x1 - x0) * t1, cy, cv.S(1.6f), kCyan,
                 (0.62f - 0.42f * t0) * a, key);
        }
        hud_lines::DiamondOutline(x1, cy, r, cv.S(1.6f), kCyan, 0.85f * a, key);
        hud_lines::Dot(x0 - side * cv.S(12.0f), cy, cv.S(2.2f), kCyan, 0.9f * a, key);
    }
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
    int   lastKills = 0;
    float xpFlash = 0.0f;        // the XP line flashes when the kills fill it
    rp::Objective shownStep = rp::Objective::Count;
    float stepAge = 0.0f;
    double lastSeen = -1.0;
    // HUD guide (ready_launch::HudGuide): how faint the rest of the page is,
    // and the screen rectangle of the part the current card points at.
    float guideDim = 0.0f;
    float focusX = 0.0f, focusY = 0.0f, focusW = 0.0f, focusH = 0.0f;
    // The step whose card is waiting to open, and for how long it has waited
    // (the move that cleared the last step plays out first).
    ready_launch::HudGuide cardPending = ready_launch::HudGuide::Done;
    float cardWait = 0.0f;
};


// HUD guide look.
constexpr float kGuideRestA = 0.28f;      // alpha of everything but the part in focus
constexpr float kGuideDimEase = 6.0f;     // per second
constexpr float kHpDemoPeriod = 3.0f;     // the health ring's drain-and-refill demo, seconds
constexpr float kHpDemoLow = 0.62f;       // how far it drains

// Health fraction of the demo at t seconds into the HP card: full, a hit
// drains it, it holds, then it fills back up.
float HpDemoFrac(float t) {
    if (t < 0.0f) return 1.0f;
    const float p = std::fmod(t, kHpDemoPeriod);
    auto ease = [](float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); };
    if (p < 0.7f) return 1.0f;
    if (p < 1.0f) return 1.0f - (1.0f - kHpDemoLow) * ease((p - 0.7f) / 0.3f);
    if (p < 1.9f) return kHpDemoLow;
    if (p < 2.6f) return kHpDemoLow + (1.0f - kHpDemoLow) * ease((p - 1.9f) / 0.7f);
    return 1.0f;
}

PracticeView& View() {
    static PracticeView v;
    return v;
}

// A gap in READY frames means the screen was left and entered again.
constexpr double kReenterGap = 0.25;

// The dash cannot be practised here when it shares the start key.
bool DashPractisable() { return ActionKey(GameAction::Dash) != ActionKey(GameAction::Confirm); }

// The live game's numbers for this frame.
rp::Rules LiveRules(const PracticeView& v, bool firing) {
    rp::Rules r;
    r.worldW = kWorldW;
    r.worldH = kWorldH;
    // The whole ship picture (hull and its orbit marks) stays inside the
    // field's frame, not only its hit circle; the HP ring may still cross it.
    r.playerRadius = PLAYER_SIZE_PX * g_Stats.playerSizeMult * kShipDrawReach;
    r.moveSpeed = PLAYER_MOVE_SPEED * g_Stats.moveSpeedMult;
    r.dashDist = DASH_DIST;
    r.dashDur = DASH_DUR;
    r.dashCooldown = DASH_CD;
    r.fireInterval = g_Stats.fireInterval;
    r.bulletSpeed = g_Stats.bulletSpeed;
    r.bulletDamage = g_Stats.GetBaseDamage() * g_Stats.GetDamageMultiplier();
    r.fieldPulse = g_CurrentWeapon == (int)StartWeapon::SMG;
    r.pulseInterval = STATIC_FIELD_PULSE_INTERVAL;
    r.pulseRadius = STATIC_FIELD_PULSE_RADIUS;
    r.pulseDamage = r.bulletDamage * STATIC_FIELD_PULSE_DAMAGE_MULT;
    r.dummyHp = v.swarmHp;
    r.dummyHitRadius = PlayerShotHitRadius(MobBodyHitPx(MobKind::SWARM) * v.swarmScale, 1.0f);
    r.dashAvailable = DashPractisable();
    return r;
}

void ResetView(PracticeView& v) {
    for (int i = 0; i < rp::kDummyCount; ++i) {
        if (!v.mobs[i]) {
            v.mobs[i] = std::make_unique<Monster>(0.0f, 0.0f, 1.0f, 1.0f, false);
            v.mobs[i]->MakeKind(MobKind::SWARM);
            // Read the Swarm's own numbers here, once: drawing overwrites
            // sizeScale with the view scale, so reading it on a later re-entry
            // would compound the size every time the tutorial is opened.
            if (i == 0) {
                v.swarmScale = v.mobs[0]->sizeScale;
                v.swarmHp = v.mobs[0]->hp;
            }
        }
        v.wasAlive[i] = true;
    }
    rp::Reset(v.field, LiveRules(v, false));
    v.shards.clear();
    v.glow.fill(kGlowWaiting);
    v.lastPulses = 0;
    v.pulseFlash = 0.0f;
    v.shownStep = rp::Objective::Count;
    v.stepAge = 0.0f;
    v.cardPending = ready_launch::HudGuide::Done;
    v.cardWait = 0.0f;
}

void TouchView(PracticeView& v) {
    // The tutorial starts over only when it is opened from the lobby. The card
    // pick and the settings opened from its pause menu leave this scene for a
    // while; the field must come back as it was.
    const bool requested = ready_launch::g_FieldResetPending;
    ready_launch::g_FieldResetPending = false;
    const bool reentered = !ready_launch::g_TutorialMode &&
                           glfwGetTime() - v.lastSeen > kReenterGap;
    if (v.lastSeen < 0.0 || requested || reentered) ResetView(v);
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

// The defeat card's lock-on: each Swarm's brackets close in over kLockTime,
// one after another kLockStagger apart.
constexpr float kLockTime = 0.45f, kLockStagger = 0.09f;

// The step's card has been shown (outside the tutorial there are none, so
// everything about a step shows at once).
bool StepCardSeen(rp::Objective o) {
    using ready_launch::HudGuide;
    if (!ready_launch::g_TutorialMode) return true;
    HudGuide card = HudGuide::Done;
    switch (o) {
    case rp::Objective::Move:  card = HudGuide::Move;  break;
    case rp::Objective::Shoot: card = HudGuide::Shoot; break;
    case rp::Objective::Dash:  card = HudGuide::Dash;  break;
    case rp::Objective::Kill:  card = HudGuide::Kill;  break;
    default:                   return true;
    }
    return (ready_launch::g_StepCardsShown & (1u << (unsigned)card)) != 0;
}

// Four corner brackets around (cx, cy), r from the centre to each corner's
// side: a target lock, drawn with solid lines only.
void TargetBrackets(float cx, float cy, float r, float arm, float th, Rgb c, float a, float key) {
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            const float x = cx + sx * r, y = cy + sy * r;
            Line(x, y, x - sx * arm, y, th, c, a, key);
            Line(x, y, x, y - sy * arm, th, c, a, key);
        }
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
    // The card pick follows the practice steps (tutorial only).
    if (ready_launch::g_TutorialMode) total += READY_EXTRA_OBJECTIVE_COUNT;
}

// ── Field ──
void DrawField(PracticeView& v, const rp::Rules& rules, rp::Objective current, bool dashOk,
               const Canvas& cv, TextRenderer& body, float now, float dt,
               ready_launch::HudGuide focus = ready_launch::HudGuide::Done) {
    using ready_launch::HudGuide;
    const float fx = cv.X(kFieldX), fy = cv.Y(kFieldY);
    const float fw = cv.S(kFieldW), fh = cv.S(kFieldH);
    const float key = cv.S(1.5f);
    const float ws = kViewScale * cv.k;   // world px -> screen px
    auto toX = [&](float wx) { return fx + wx * ws; };
    auto toY = [&](float wy) { return fy + wy * ws; };
    const float batchAlpha = g_BatchAlpha;
    // HUD guide: the part the card points at stays bright, the rest goes faint.
    const float restA = batchAlpha * (1.0f - (1.0f - kGuideRestA) * v.guideDim);
    auto partAlpha = [&](HudGuide part) { return focus == part ? batchAlpha : restA; };
    g_BatchAlpha = restA;

    // Frame with corner brackets.
    Box(fx, fy, fw, fh, cv.S(1.3f), kCyan, 0.32f, key);
    const float L = cv.S(kFieldCorner), t = cv.S(2.4f);
    const float cx[4] = { fx, fx + fw, fx + fw, fx }, cy[4] = { fy, fy, fy + fh, fy + fh };
    const float dx[4] = { 1, -1, -1, 1 }, dy[4] = { 1, 1, -1, -1 };
    for (int i = 0; i < 4; ++i) {
        Line(cx[i] + dx[i] * L, cy[i], cx[i], cy[i], t, kCyan, 0.75f, key);
        Line(cx[i], cy[i], cx[i], cy[i] + dy[i] * L, t, kCyan, 0.75f, key);
    }
    // A node on the middle of each edge ties the frame into the constellation look.
    {
        const float nr = cv.S(4.5f);
        hud_lines::DiamondOutline(fx + fw * 0.5f, fy, nr, cv.S(1.5f), kCyan, 0.7f, key);
        hud_lines::DiamondOutline(fx + fw * 0.5f, fy + fh, nr, cv.S(1.5f), kCyan, 0.7f, key);
        hud_lines::DiamondOutline(fx, fy + fh * 0.5f, nr, cv.S(1.5f), kCyan, 0.7f, key);
        hud_lines::DiamondOutline(fx + fw, fy + fh * 0.5f, nr, cv.S(1.5f), kCyan, 0.7f, key);
    }
    // No label over the field: the cards, the line above the ship and the
    // objectives say everything (beta: the page had too much to read).
    const Rgb swarm{ v.mobs[0]->color.r, v.mobs[0]->color.g, v.mobs[0]->color.b };
    const Rgb swarmText = Lighten(swarm, 0.45f);
    g_BatchAlpha = restA;

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
        g_BatchAlpha = partAlpha(HudGuide::Kill);   // the defeat card is about them
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

        // Defeat step: gold target brackets on every Swarm and a name tag on one,
        // so the step's word points at the thing on the field. They do not pop
        // in when the dash clears the step before: they lock on with the defeat
        // card (wide and faint, closing in on each Swarm one after another,
        // a flash as each locks, then the tag), and stay for the whole step; a
        // kill breaks them apart, a respawn locks on again.
        const bool killCard = focus == HudGuide::Kill;
        if (killCard || (current == rp::Objective::Kill && StepCardSeen(rp::Objective::Kill))) {
            const float baseA = killCard ? 1.0f : 0.80f;
            const float breathe = 0.5f + 0.5f * std::sin(now * 3.0f);
            const float lockT = killCard ? ready_launch::g_HudGuideT : 1e3f;   // seconds into the lock-on
            int tagged = -1;
            float tagX = 0.0f, tagY = 0.0f, tagR = 0.0f;
            for (int i = 0; i < rp::kDummyCount; ++i) {
                const rp::Dummy& d = v.field.dummies[i];
                float a = baseA, open = 0.0f;
                if (!d.Alive()) {
                    const float p = std::min(1.0f, rp::RespawnProgress(d) * 6.0f);
                    if (p >= 1.0f) continue;
                    a *= 1.0f - p;
                    open = p;
                }
                // Lock-on: from 2.6x wide and clear to the Swarm, eased out.
                const float since = lockT - kLockStagger * (float)i;
                const float p = std::clamp(since / kLockTime, 0.0f, 1.0f);
                if (p <= 0.0f) continue;
                const float e = 1.0f - (1.0f - p) * (1.0f - p) * (1.0f - p);
                const float flash = std::clamp(1.0f - (since - kLockTime) / 0.22f, 0.0f, 1.0f) * (p >= 1.0f ? 1.0f : 0.0f);
                const float bob = d.Alive() ? std::sin(now * kBobSpeed + 1.9f * (float)i) * kBob : 0.0f;
                const float cx = toX(rp::DummyX(d)), cy = toY(rp::DummyY(d) + bob);
                const float hit = std::clamp(d.hitFlash / rp::kHitFlash, 0.0f, 1.0f);
                const float r = (rules.dummyHitRadius + 14.0f + 3.0f * breathe - 5.0f * hit + 30.0f * open) *
                                ws * (1.0f + 1.6f * (1.0f - e));
                TargetBrackets(cx, cy, r, r * 0.45f, cv.S(2.2f + 1.6f * flash), kGold,
                               std::min(1.0f, a * (0.78f + 0.22f * breathe) * e + 0.4f * flash), key);
                if (d.Alive() && tagged < 0 && p >= 1.0f) { tagged = i; tagX = cx; tagY = cy; tagR = r; }
            }
            // The name tag follows once every Swarm is locked: the leader draws
            // out from the corner, then the word fades in.
            const float tagIn = std::clamp((lockT - kLockStagger * (rp::kDummyCount - 1) - kLockTime) / 0.3f,
                                           0.0f, 1.0f);
            if (tagged >= 0 && tagIn > 0.0f) {
                const wchar_t* tag = ReadySwarmTag();
                const float tagPx = cv.T(22.0f);
                const float tw = body.Width(tag, tagPx / std::max(1.0f, body.LineHeightPixels()));
                const float lead = cv.S(16.0f), run = cv.S(14.0f);
                const bool left = tagX + tagR + lead + run + tw + cv.S(12.0f) > fx + fw;
                const float side = left ? -1.0f : 1.0f;
                const float x0 = tagX + side * tagR, y0 = tagY - tagR;
                const float grow = std::min(1.0f, tagIn * 2.0f);
                const float x1 = x0 + side * lead * grow, y1 = y0 - lead * grow;
                const float x2 = x1 + side * run * grow;
                Line(x0, y0, x1, y1, cv.S(1.6f), kGold, 0.9f * baseA, key);
                Line(x1, y1, x2, y1, cv.S(1.6f), kGold, 0.9f * baseA, key);
                const float textA = std::clamp(tagIn * 2.0f - 1.0f, 0.0f, 1.0f);
                Text(body, tag, left ? x2 - cv.S(6.0f) : x2 + cv.S(6.0f), y1, tagPx, kGold, baseA * textA,
                     left ? Align::Right : Align::Left);
            }
        }
        g_BatchAlpha = restA;

        // Bullets and ship with the gameplay renderers, at the view scale.
        for (const rp::Bullet& b : v.field.bullets) {
            ::Bullet shot(toX(b.x), toY(b.y), toX(b.x) + b.dirX, toY(b.y) + b.dirY);
            shot.speed = rules.bulletSpeed * ws;
            shot.sizeScale = ws;
            drawBullet(shot);
        }
        // The ship at its spot in the field, at the run's size scaled by the view.
        const float shipRunSize = PLAYER_SIZE_PX * g_Stats.playerSizeMult;
        // The ship stays bright on the cards about it (health, and the move,
        // shoot and dash steps).
        const bool shipFocus = focus == HudGuide::Hp || focus == HudGuide::Move ||
                               focus == HudGuide::Shoot || focus == HudGuide::Dash;
        g_BatchAlpha = shipFocus ? batchAlpha : restA;
        DrawPlayerWeaponShell(toX(v.field.playerX), toY(v.field.playerY),
                              shipRunSize * ws, v.field.aim, now);
        g_BatchAlpha = partAlpha(HudGuide::Hp);

        // The health ring, full as in a fresh run, and what it measures: one
        // ring is the range unit augment cards count in ("4○", Entity/RangeUnit.h).
        {
            const float px = toX(v.field.playerX), py = toY(v.field.playerY);
            const float R = kRangeUnit * ws, thick = vitals_hud::HpArcThick(0.0f) * ws;
            const Rgb hp{ vitals_hud::kHpCol.r, vitals_hud::kHpCol.g, vitals_hud::kHpCol.b };
            // On the HUD guide's HP card a hit drains it and it fills back up.
            const float hpFrac = focus == HudGuide::Hp ? HpDemoFrac(ready_launch::g_HudGuideT) : 1.0f;
            const float a0 = vitals_hud::kArcStart, sweep = vitals_hud::kArcSweep;
            const Rgb trackC{ vitals_hud::kTrackCol.r, vitals_hud::kTrackCol.g, vitals_hud::kTrackCol.b };
            // The ring is wider than the ship, so near a wall it crosses the
            // field's frame: it is drawn whole, but turns see-through as it
            // goes out, so the frame still reads as the field's edge.
            const float inside = std::min(std::min(px - fx, fx + fw - px), std::min(py - fy, fy + fh - py));
            const float over = std::clamp((R + thick - inside) / cv.S(kRingFadePx), 0.0f, 1.0f);
            const float ringA = 1.0f - (1.0f - kRingOutA) * over;
            hud_lines::Arc(px, py, R, thick + cv.S(3.0f), a0, sweep,
                           hud_lines::KeyCol(hp), hud_lines::kKeyA * 0.85f * ringA);
            if (hpFrac < 1.0f)
                hud_lines::Arc(px, py, R, thick, a0 + sweep * hpFrac, sweep * (1.0f - hpFrac), trackC, 0.40f * ringA);
            hud_lines::Arc(px, py, R, thick, a0, sweep * hpFrac, hp, 0.85f * ringA);

            // In the tutorial the note is the HP card's: it goes with the card
            // (beta: it followed the ship through every step).
            const float noteIn = !ready_launch::g_TutorialMode ? 1.0f
                               : focus == HudGuide::Hp ? std::clamp(ready_launch::g_HudGuideT / 0.3f, 0.0f, 1.0f) : 0.0f;
            const float notePx = cv.T(kNotePx);
            const float scale = notePx / std::max(1.0f, body.LineHeightPixels());
            const wchar_t* note = ReadyRingNote();
            const float oneW = body.Width(L"1", scale), glyph = notePx * 0.80f, gap = cv.S(6.0f);
            const float labelW = oneW + glyph + gap + body.Width(note, scale);
            // Spoke from the ship to the ring along its open side, then the label;
            // it swaps to the left near the field's right edge.
            const bool left = px + R + cv.S(12.0f) + labelW > fx + fw - cv.S(8.0f);
            const float dir = left ? -1.0f : 1.0f;
            const float a = 0.85f;
            g_BatchAlpha *= noteIn;
            if (noteIn > 0.004f) {
                Line(px + dir * cv.S(16.0f), py, px + dir * R, py, cv.S(1.3f), hp, 0.75f, key);
                Line(px + dir * R, py - cv.S(5.0f), px + dir * R, py + cv.S(5.0f), cv.S(1.3f), hp, 0.75f, key);
                float tx = left ? px - R - cv.S(12.0f) - labelW : px + R + cv.S(12.0f);
                const Rgb noteCol = Lighten(hp, 0.35f);
                Text(body, L"1", tx, py, notePx, noteCol, a);
                tx += oneW;
                const float gx = tx + glyph * 0.5f, gr = notePx * 0.30f;
                hud_lines::Arc(gx, py, gr, cv.S(1.3f), 0.0f, 6.2831853f, noteCol, a);
                drawCircle(gx, py, std::max(1.0f, gr * 0.26f), noteCol.r, noteCol.g, noteCol.b, a);
                tx += glyph + gap;
                Text(body, note, tx, py, notePx, kText, 0.92f);
            }
            g_BatchAlpha = restA;
            if (focus == HudGuide::Hp) {
                // Focus: the ring and its note.
                const float pad = cv.S(16.0f);
                const float x0 = (left ? px - R - cv.S(12.0f) - labelW : px - R) - pad;
                const float x1 = (left ? px + R : px + R + cv.S(12.0f) + labelW) + pad;
                // Kept inside the field, like the ring it points at.
                const float in = cv.S(8.0f);
                const float cx0 = std::max(x0, fx + in), cx1 = std::min(x1, fx + fw - in);
                const float cy0 = std::max(py - R - pad, fy + in);
                const float cy1 = std::min(py + R + pad, fy + fh - in);
                v.focusX = cx0; v.focusY = cy0;
                v.focusW = cx1 - cx0; v.focusH = cy1 - cy0;
            }
        }

        // The XP line: the line a run draws along the screen's bottom edge, here
        // along the field's bottom. Each kill fills a quarter; the fourth fills it
        // and the level-up card follows. Bright while the kill step is taught.
        {
            const int kills = v.field.kills;
            if (kills < v.lastKills) v.lastKills = kills;   // field restarted
            if (v.lastKills < rp::kKillGoal && kills >= rp::kKillGoal) v.xpFlash = kXpFlash;
            v.lastKills = kills;
            v.xpFlash = std::max(0.0f, v.xpFlash - dt);
            const bool leveled = kills >= rp::kKillGoal;
            const float frac = leveled ? 0.0f : (float)kills / (float)rp::kKillGoal;
            const float flash = v.xpFlash / kXpFlash;
            const float pulse = 0.80f + 0.20f * (0.5f + 0.5f * std::sin(now * kPulseSpeed));
            float a = current < rp::Objective::Kill ? kGlowWaiting
                    : current == rp::Objective::Kill ? pulse : kGlowDone;
            float shownFrac = frac;
            if (focus == HudGuide::Xp) {
                // On the HUD guide's XP card it fills a quarter at a time, like four kills.
                a = pulse;
                shownFrac = std::floor(std::fmod(ready_launch::g_HudGuideT, 4.0f) / 0.8f) / 4.0f;
            }
            g_BatchAlpha = partAlpha(HudGuide::Xp);
            const Rgb xp{ vitals_hud::kXpCol.r, vitals_hud::kXpCol.g, vitals_hud::kXpCol.b };
            const Rgb track{ vitals_hud::kTrackCol.r, vitals_hud::kTrackCol.g, vitals_hud::kTrackCol.b };

            const float notePx = cv.T(kNotePx);
            const float scale = notePx / std::max(1.0f, body.LineHeightPixels());
            const float y = fy + fh - cv.S(28.0f);
            const float x0 = fx + cv.S(22.0f);
            // Words stay readable while the line waits; only the line itself dims.
            const float textA = std::max(0.62f, a);
            wchar_t lv[24];
            swprintf_s(lv, L"%ls%d", T(StrId::LV_PREFIX), leveled ? 2 : 1);
            Text(body, lv, x0, y, notePx, xp, std::min(1.0f, textA + 0.15f * flash));
            const float lx = x0 + body.Width(lv, scale) + cv.S(10.0f);
            const float rx = fx + fw * 0.46f;
            const float fillX = lx + (rx - lx) * std::min(1.0f, shownFrac);
            if (fillX < rx) Line(fillX, y, rx, y, cv.S(3.0f), track, 0.65f * a, key);
            if (fillX > lx) Line(lx, y, fillX, y, cv.S(4.0f), xp, a, key);
            if (flash > 0.0f) Line(lx, y, rx, y, cv.S(4.0f + 1.6f * flash), xp, 0.90f * flash, key);

            // The note after the line, shrunk if a language runs long.
            const wchar_t* note = ReadyXpNote();
            const float nx = rx + cv.S(14.0f);
            const float maxW = std::max(1.0f, fx + fw - cv.S(18.0f) - nx);
            const float noteW = body.Width(note, scale);
            const float px = noteW > maxW ? notePx * maxW / noteW : notePx;
            // In the tutorial the note is the XP card's (as the ring's is the HP card's).
            const float xpNoteIn = !ready_launch::g_TutorialMode ? 1.0f
                                 : focus == HudGuide::Xp ? std::clamp(ready_launch::g_HudGuideT / 0.3f, 0.0f, 1.0f) : 0.0f;
            if (xpNoteIn > 0.004f) Text(body, note, nx, y, px, kText, 0.92f * textA * xpNoteIn);
            g_BatchAlpha = restA;
            if (focus == HudGuide::Xp) {
                // Focus: Lv, the line and its note.
                const float pad = cv.S(12.0f);
                const float noteEnd = nx + body.Width(note, px / std::max(1.0f, body.LineHeightPixels()));
                v.focusX = x0 - pad; v.focusY = y - cv.S(14.0f) - pad;
                v.focusW = std::min(noteEnd, fx + fw - cv.S(10.0f)) + pad - v.focusX;
                v.focusH = cv.S(28.0f) + 2.0f * pad;
            }
        }
    }

    // The current step's instruction, inside the field. It steps aside while
    // a HUD guide card is up so the card's caption has the field to itself.
    g_BatchAlpha = batchAlpha * (1.0f - v.guideDim);
    if (current != v.shownStep) { v.shownStep = current; v.stepAge = 0.0f; }
    v.stepAge += dt;
    const float in = std::clamp(v.stepAge / kStepFadeIn, 0.0f, 1.0f);
    float topIn = in;   // the label and hint along the field's top
    const float rise = cv.S(kStepRise) * (1.0f - in);
    const float mid = fx + fw * 0.5f;
    using Stage = ready_launch::TutorialStage;
    const Stage stage = ready_launch::g_TutorialMode ? ready_launch::g_Stage : Stage::Practice;
    // Along the top: the card pick's title only. The step number lives on the
    // cards and the objectives, and the launch card says "ready" (beta: the
    // page repeated itself).
    const wchar_t* hint = ReadyStepHint((int)current);
    if (stage != Stage::Practice && stage != Stage::Done) hint = ReadyExtraHint(0);
    else if (ready_launch::g_TutorialMode) hint = L"";
    // The top line steps back while the ship (and its ring) flies under it.
    {
        const float sx = toX(v.field.playerX), sy = toY(v.field.playerY);
        const float R = kRangeUnit * ws;
        const float gapY = (sy - R) - (fy + cv.S(kStepHintY + 14.0f));
        const bool under = std::fabs(sx - mid) < R + cv.S(200.0f);
        if (under) topIn *= std::clamp(gapY / cv.S(30.0f), 0.2f, 1.0f);
    }
    (void)dashOk;
    const bool tutorialPractice = ready_launch::g_TutorialMode && stage == Stage::Practice &&
                                  current != rp::Objective::Count;
    if (tutorialPractice) {
        // Nothing new appears in the pause before a step's card: its line
        // comes in with the card (and shows once the card is closed).
        // While a card is up it stays off too (it would blink on for the card's
        // first frame), and it fades back in as the card closes.
        if (!StepCardSeen(current) || ready_launch::HudGuideActive()) { g_BatchAlpha = batchAlpha; return; }
        // The step's card said it in full; while practising, a short reminder
        // rides above the ship where the eyes are (below it near the top edge).
        const float px = toX(v.field.playerX), py = toY(v.field.playerY);
        const float R = kRangeUnit * ws;
        const float linePx = cv.T(24.0f);
        const wchar_t* s = ReadyStepShort((int)current, v.field.kills);
        float y = py - R - cv.S(22.0f);
        if (y - linePx < fy + cv.S(kStepLabelY) + cv.S(14.0f)) y = py + R + cv.S(22.0f);
        const float sw = body.Width(s, linePx / std::max(1.0f, body.LineHeightPixels()));
        const float x = std::clamp(px, fx + sw * 0.5f + cv.S(12.0f), fx + fw - sw * 0.5f - cv.S(12.0f));
        Text(body, s, x, y + rise, linePx, kWhite, in, Align::Center);
    } else {   // Shrunk if a language runs wider than the field.
        float px = cv.T(kStepHintPx);
        const float w = body.Width(hint, px / std::max(1.0f, body.LineHeightPixels()));
        const float room = fw - cv.S(40.0f);
        if (w > room && w > 0.0f) px *= room / w;
        Text(body, hint, mid, fy + cv.S(kStepHintY) + rise, px, kWhite, topIn, Align::Center);
    }
    g_BatchAlpha = batchAlpha;
}

void DrawObjectives(const rp::Field& f, rp::Objective current, bool dashOk,
                    const Canvas& cv, TextRenderer& body, float pulse) {
    const float x = cv.X(kObjectivesX), y0 = cv.Y(kFieldY + 10.0f);
    const float key = cv.S(1.5f);
    float y = y0 + cv.S(14.0f);   // no "OBJECTIVES" heading: the list reads as one
    // Steps are nodes on one constellation line; finished ones fill and light the
    // line up to them, the current one pulses.
    const float nodeX = x + cv.S(kCheckSize) * 0.5f, nodeR = cv.S(kCheckSize) * 0.36f;
    float prevY = -1.0f;
    bool prevDone = false;
    auto node = [&](float ny, bool done, bool isCurrent, bool skipped, Rgb c) {
        if (prevY >= 0.0f)
            Line(nodeX, prevY + nodeR * 1.6f, nodeX, ny - nodeR * 1.6f, cv.S(1.6f),
                 prevDone && done ? kDone : kCyan, prevDone ? 0.7f : 0.22f, key);
        if (done) {
            hud_lines::SectorFill(nodeX, ny, nodeR, hud_lines::FillShape::Diamond, 1.0f, c, 0.85f);
            hud_lines::DiamondOutline(nodeX, ny, nodeR, cv.S(1.8f), c, 0.95f, key);
        } else if (isCurrent) {
            hud_lines::SectorFill(nodeX, ny, nodeR, hud_lines::FillShape::Diamond, 1.0f, kCyan, 0.30f * pulse);
            hud_lines::DiamondOutline(nodeX, ny, nodeR * 1.15f, cv.S(2.4f), kCyan, pulse, key);
            hud_lines::Ring(nodeX, ny, nodeR * 2.1f, cv.S(1.4f), kCyan, 0.4f * pulse, 0.0f);
        } else {
            hud_lines::DiamondOutline(nodeX, ny, nodeR, cv.S(1.5f), c, skipped ? 0.3f : 0.5f, key);
        }
        prevY = ny;
        prevDone = done;
    };
    for (int i = 0; i < rp::kObjectiveCount; ++i) {
        const rp::Objective o = (rp::Objective)i;
        const bool skipped = o == rp::Objective::Dash && !dashOk;
        const bool done = !skipped && rp::ObjectiveDone(f, o);
        const bool isCurrent = o == current;
        const Rgb c = done ? kDone : kCyan;
        const float boxA = done ? 0.95f : isCurrent ? pulse : skipped ? 0.3f : 0.45f;
        const float s = cv.S(kCheckSize);
        (void)boxA;
        node(y, done, isCurrent, skipped, c);
        const float textA = done ? 0.95f : isCurrent ? 1.0f : skipped ? 0.55f : 0.78f;
        Text(body, ReadyObjective(i), x + s + cv.S(14.0f), y, cv.T(kObjectivePx),
             done ? c : isCurrent ? kWhite : kText, textA);
        if (o == rp::Objective::Kill) {
            const std::wstring prog = std::to_wstring(std::min(f.kills, rp::kKillGoal)) + L"/" +
                                      std::to_wstring(rp::kKillGoal);
            Text(body, prog.c_str(), x + cv.S(300.0f), y, cv.T(23.0f), c,
                 done || isCurrent ? 0.95f : 0.75f, Align::Right);
        }
        if (skipped) {
            y += cv.S(26.0f);
            Text(body, ReadyDashOnStart(), x + s + cv.S(14.0f), y, cv.T(kObjectiveNotePx, kMinSmallPx),
                 kAmber, 0.9f);
        }
        y += cv.S(kObjectiveRowH);
    }
    using Stage = ready_launch::TutorialStage;
    const bool tutorial = ready_launch::g_TutorialMode;
    const Stage stage = ready_launch::g_Stage;
    if (tutorial) {
        // Step 5: the real card pick.
        for (int i = 0; i < READY_EXTRA_OBJECTIVE_COUNT; ++i) {
            const bool done = stage == Stage::Done;
            const bool isCurrent = stage == Stage::AugPick;
            const Rgb c = done ? kDone : kCyan;
            const float boxA = done ? 0.95f : isCurrent ? pulse : 0.45f;
            const float s = cv.S(kCheckSize);
            (void)boxA;
            node(y, done, isCurrent, false, c);
            const float textA = done ? 0.95f : isCurrent ? 1.0f : 0.78f;
            Text(body, ReadyExtraObjective(i), x + s + cv.S(14.0f), y, cv.T(kObjectivePx),
                 done ? c : isCurrent ? kWhite : kText, textA);
            y += cv.S(kObjectiveRowH);
        }
    }
}

// ── Keyboard panel ──
bool CapHeld(int glfwKey) { return glfwKey >= 0 && glfwKey < 1024 && keys[glfwKey]; }

void DrawMouse(float x, float y, const Canvas& cv, const GuideLook& look, bool lmb, float now,
               float scale = 1.0f) {
    const float key = cv.S(1.5f);
    const float w = cv.S(kMouseW) * scale, h = cv.S(kMouseH) * scale, split = h * kMouseSplit;
    const float a = lmb ? 1.0f : look.a * 0.7f;
    // The left button fires: always tinted cyan like the move keys, filled
    // while it is the step and solid while held.
    const float fireFill = lmb ? 0.7f : look.current ? kCurrentFillA * look.a + 0.18f : 0.18f;
    Fill(x, y, w * 0.5f, split, kCyan, fireFill);
    Box(x, y, w, h, cv.S(look.current ? 2.8f : 2.0f), kCyan, a, key);
    Line(x + w * 0.5f, y, x + w * 0.5f, y + split, cv.S(2.0f), kCyan, a, key);
    Line(x, y + split, x + w, y + split, cv.S(2.0f), kCyan, a, key);
    if (look.current) {
        // Click ripples rising off the left button.
        const float t = std::fmod(now * 1.4f, 1.0f);
        const float bx = x + w * 0.25f;
        for (int i = 0; i < 2; ++i) {
            const float r = (cv.S(22.0f + 18.0f * i) + cv.S(14.0f) * t) * scale;
            hud_lines::Arc(bx, y, r, cv.S(2.4f), -2.5f, 1.4f, kCyan, (1.0f - t) * (i ? 0.55f : 0.95f));
        }
    }
}

// The start key's cap reads "confirm" while drawn on the card pick.
bool g_CapConfirm = false;

// How one key cap is drawn.
struct CapLook {
    Rgb   c;          // outline / fill colour
    float a;          // outline alpha
    float ta;         // label alpha
    bool  filled;     // filled with its colour (to press now, or held)
    float fillA;
    bool  strong;     // thick outline, white label
    bool  active;     // has a role on this screen
};

// One key cap at (x, y, w, h) in screen px. ts scales the label, the caption,
// the arrow and the outline (1 on the keyboard panel, larger on a guide card),
// so the card's picture is the panel's own cap, only bigger.
void DrawCap(const kb::KeyCap& cap, float x, float y, float w, float h, float ts,
             const CapLook& L, const Canvas& cv, TextRenderer& body) {
    const float key = cv.S(1.5f) * ts;
    const kb::KeyRole role = kb::RoleOf(cap.key);
    if (L.filled) Fill(x, y, w, h, L.c, L.fillA);
    Box(x, y, w, h, cv.S(L.strong ? 2.6f : L.active ? 2.0f : 1.4f) * ts, L.c, L.a, key);

    const Rgb tc = L.strong ? kWhite : L.c;
    const float mx = x + w * 0.5f, my = y + h * 0.5f;
    // Shrink text that would touch the cap's outline.
    auto fitPx = [&](const wchar_t* s, float px) {
        const float room = w - cv.S(8.0f) * ts;
        const float tw = body.Width(s, px / body.LineHeightPixels());
        return tw > room && tw > 0.0f ? px * room / tw : px;
    };
    if (role == kb::KeyRole::Start) {
        const wchar_t* s = g_CapConfirm ? ReadyConfirmCap() : ReadyLegend((int)role - 1);
        Text(body, s, mx, my, fitPx(s, cv.S(kSpacePx) * ts), tc, L.ta, Align::Center);
        return;
    }
    const kb::ArrowDir dir = kb::CapArrow(cap.key);
    if (dir.x != 0.0f || dir.y != 0.0f) {
        const float r = cv.S(9.0f) * ts, px = -dir.y, py = dir.x;
        const float tx = mx + dir.x * r, ty = my + dir.y * r;
        const float lw = cv.S(2.0f) * ts;
        Line(mx - dir.x * r, my - dir.y * r, tx, ty, lw, tc, L.ta, key);
        Line(tx, ty, tx - dir.x * r * 0.6f + px * r * 0.6f, ty - dir.y * r * 0.6f + py * r * 0.6f, lw, tc, L.ta, key);
        Line(tx, ty, tx - dir.x * r * 0.6f - px * r * 0.6f, ty - dir.y * r * 0.6f - py * r * 0.6f, lw, tc, L.ta, key);
        return;
    }
    const std::wstring label = kb::CapLabel(cap.key);
    // Captions only on wide caps; a 1-unit cap has no room and the
    // legend already names every role.
    const wchar_t* caption = ReadyKeyCaption((int)role);
    const bool hasCaption = caption[0] != 0 && cap.w >= kCaptionMinW;
    const float labelPx = cv.S(label.size() > 2 ? kKeyLongPx : kKeyPx) * ts;
    Text(body, label.c_str(), mx, my - (hasCaption ? cv.S(8.0f) * ts : 0.0f),
         fitPx(label.c_str(), labelPx), tc, L.ta, Align::Center);
    if (hasCaption)
        Text(body, caption, mx, my + cv.S(9.0f) * ts, fitPx(caption, cv.S(kKeyCaptionPx) * ts),
             tc, L.ta, Align::Center);
}

// The caps on the panel that do `role`, one per bound key (both Shift caps
// can map to one binding; the first one stands for it).
std::vector<const kb::KeyCap*> RoleCaps(kb::KeyRole role) {
    std::vector<const kb::KeyCap*> out;
    std::vector<int> seen;
    for (const kb::KeyCap& cap : kb::Layout()) {
        if (kb::RoleOf(cap.key) != role) continue;
        const int k = NormalizeBindKey(cap.key);
        if (std::find(seen.begin(), seen.end(), k) != seen.end()) continue;
        seen.push_back(k);
        out.push_back(&cap);
    }
    return out;
}

// Screen rectangle of a cap on the keyboard panel.
void PanelCapRect(const kb::KeyCap& cap, const Canvas& cv, float& x, float& y, float& w, float& h) {
    x = cv.X(kKeyboardX + cap.x * kKeyPitch);
    y = cv.Y(kKeyboardY + cap.y * kKeyPitch);
    w = cv.S(cap.w * kKeyPitch - kKeyGap);
    h = cv.S(cap.h * kKeyPitch - kKeyGap);
}

void MousePanelRect(const Canvas& cv, float& x, float& y, float& w, float& h) {
    x = cv.X(kKeyboardX + kb::kLayoutW * kKeyPitch + kLegendGap);
    y = cv.Y(kKeyboardY + (kb::kLayoutH * kKeyPitch - kKeyGap - kMouseH) * 0.5f);
    w = cv.S(kMouseW);
    h = cv.S(kMouseH);
}

// Screen boxes around the panel caps that pass `keep`; caps that touch
// (W A S D, 1 2 3) share one box, so one set of corners frames the group.
struct CapBox { float x0, y0, x1, y1; };
template <class Keep>
std::vector<CapBox> CapClusters(const Canvas& cv, Keep keep) {
    std::vector<CapBox> boxes;
    const float reach = cv.S(kKeyPitch * 0.5f);
    for (const kb::KeyCap& cap : kb::Layout()) {
        if (!keep(cap)) continue;
        float x, y, w, h;
        PanelCapRect(cap, cv, x, y, w, h);
        CapBox b{ x, y, x + w, y + h };
        bool merged = true;
        while (merged) {
            merged = false;
            for (size_t i = 0; i < boxes.size(); ++i) {
                const CapBox& o = boxes[i];
                if (b.x0 - reach < o.x1 && o.x0 - reach < b.x1 && b.y0 - reach < o.y1 && o.y0 - reach < b.y1) {
                    b = { std::min(b.x0, o.x0), std::min(b.y0, o.y0), std::max(b.x1, o.x1), std::max(b.y1, o.y1) };
                    boxes.erase(boxes.begin() + (long)i);
                    merged = true;
                    break;
                }
            }
        }
        boxes.push_back(b);
    }
    return boxes;
}

// ── The card pick on the panel ──
// While the tutorial's level-up cards are up, the start key confirms a card
// (not "launch") and the card keys pick one, so the panel says that.
bool IsCardKey(int k) {
    for (GameAction a : { GameAction::Card1, GameAction::Card2, GameAction::Card3 })
        if (ActionBound(a) && ActionKey(a) == k) return true;
    return false;
}
bool IsTermsKey(int k) {
    return ActionBound(GameAction::Glossary) && ActionKey(GameAction::Glossary) == k;
}
// Keys the open card-pick guide is about: the glossary key on "rarity & text",
// the card keys and the start key on "choose".
bool PickGuideKey(const kb::KeyCap& cap) {
    if (!ready_launch::g_SelectionUp || !ready_launch::PickGuideActive()) return false;
    const int k = NormalizeBindKey(cap.key);
    switch (ready_launch::g_PickGuide) {
    case ready_launch::PickGuide::Detail: return IsTermsKey(k);
    case ready_launch::PickGuide::Keys:   return IsCardKey(k) || kb::RoleOf(cap.key) == kb::KeyRole::Start;
    default:                              return false;
    }
}

// ── Quiet keys ──
// Keys that do nothing here are ghosts: a faint outline, no label (it shows
// while held). The panel's side blocks (F1~F12, PrtSc/ScrLk/Pause, Ins~PgDn,
// the arrows) are left out whole while none of their keys does anything, so
// a first look shows the keys to learn, not a whole keyboard.
enum class CapBlock { Main, FKeys, System, Nav, Arrows };
CapBlock BlockOf(const kb::KeyCap& cap) {
    constexpr float kSide = 15.0f;   // key units: the main block's width
    if (cap.y < 1.0f) return cap.x >= kSide ? CapBlock::System : cap.x >= 1.5f ? CapBlock::FKeys : CapBlock::Main;
    if (cap.x < kSide) return CapBlock::Main;
    return cap.y < 4.0f ? CapBlock::Nav : CapBlock::Arrows;
}
// A key in use on this screen: its role, or a card-pick key while cards are up.
bool CapInUse(const kb::KeyCap& cap, bool picking) {
    if (kb::RoleOf(cap.key) != kb::KeyRole::None) return true;
    const int k = NormalizeBindKey(cap.key);
    return picking && (IsCardKey(k) || IsTermsKey(k));
}

// focusRole / focusMouse: the guide card is about these, so they stay at full
// brightness while the rest of the page is faint.
void DrawKeyboard(const PracticeView& v, rp::Objective current, const Canvas& cv,
                  TextRenderer& body, float pulse, bool lmb, float now,
                  kb::KeyRole focusRole = kb::KeyRole::None, bool focusMouse = false) {
    const float pageA = g_BatchAlpha;
    const bool picking = ready_launch::g_SelectionUp;
    g_CapConfirm = picking;

    // Which side blocks have a key in use (a held key shows its block too).
    bool blockOn[5] = { true, false, false, false, false };
    for (const kb::KeyCap& cap : kb::Layout())
        if (CapInUse(cap, picking) || CapHeld(cap.key)) blockOn[(int)BlockOf(cap)] = true;

    for (const kb::KeyCap& cap : kb::Layout()) {
        if (!blockOn[(int)BlockOf(cap)]) continue;
        float x, y, w, h;
        PanelCapRect(cap, cv, x, y, w, h);
        const kb::KeyRole role = kb::RoleOf(cap.key);
        const bool on = CapHeld(cap.key);
        const int nk = NormalizeBindKey(cap.key);
        // On the card pick: the start key and the card keys are the ones in use.
        const bool pickKey = picking && (role == kb::KeyRole::Start || IsCardKey(nk) || IsTermsKey(nk));
        const bool active = role != kb::KeyRole::None || pickKey;
        const bool focused = picking ? PickGuideKey(cap) : role != kb::KeyRole::None && role == focusRole;

        const GuideLook look = LookOf(v, GuideOfRole(role), current, pulse);
        CapLook L;
        L.c = (on && !active) || pickKey ? kWhite : RoleColor(role);
        L.active = active;
        if (pickKey && !focused) {
            // In use, but nothing to press until the guide says so: plain outline.
            L.a = on ? 1.0f : 0.7f; L.ta = on ? 1.0f : 0.85f;
            L.filled = on; L.fillA = 0.6f; L.strong = on;
        } else if (focused) {
            // The card's key: filled and bright, like the step being taught.
            L.a = pulse; L.ta = 1.0f; L.filled = true; L.fillA = kCurrentFillA * pulse + 0.12f; L.strong = true;
        } else {
            L.a = on ? 1.0f : active ? look.a : kGhostKeyA;
            L.ta = on || look.current ? 1.0f : active ? std::min(1.0f, look.a + 0.2f) : 0.0f;
            // Keys to press now are filled with their colour; a press fills any key.
            L.filled = look.current || on;
            L.fillA = on ? 0.6f : kCurrentFillA * look.a + 0.12f;
            L.strong = on || look.current;
        }
        g_BatchAlpha = focused ? 1.0f : pageA;
        DrawCap(cap, x, y, w, h, 1.0f, L, cv, body);
    }
    g_BatchAlpha = pageA;
    g_CapConfirm = false;

    // The card-pick guide's keys get the guide cards' gold corners, closing
    // in as its page opens (as on the practice steps' cards).
    if (picking && ready_launch::PickGuideActive()) {
        const std::vector<CapBox> boxes = CapClusters(cv, PickGuideKey);
        const float in = std::min(1.0f, ready_launch::g_PickGuideT / 0.25f);
        for (const CapBox& b : boxes) {
            const float grow = (1.0f - in) * cv.S(14.0f) + cv.S(8.0f);
            const float rx = b.x0 - grow, ry = b.y0 - grow;
            const float rw = b.x1 - b.x0 + 2.0f * grow, rh = b.y1 - b.y0 + 2.0f * grow;
            g_BatchAlpha = 1.0f;
            DrawGuideCorners(rx, ry, rw, rh, std::min(cv.S(22.0f), std::min(rw, rh) * 0.3f), cv.S(1.8f),
                             (0.70f + 0.30f * pulse) * in);
        }
        g_BatchAlpha = pageA;
    }

    // The mouse sits right of the keyboard. No written legend: the caps name
    // their keys and the cards say what they do.
    float lx, ly, lw, lh;
    MousePanelRect(cv, lx, ly, lw, lh);
    const GuideLook mouse = LookOf(v, (int)Guide::Mouse, current, pulse);
    g_BatchAlpha = focusMouse ? 1.0f : pageA;
    DrawMouse(lx, ly, cv, focusMouse ? GuideLook{ pulse, true } : mouse, lmb, now);
    g_BatchAlpha = pageA;
}

// ── HUD guide card ──
// Gold corners around the part in focus (the shop tour's look) and one caption
// block beside it: progress diamonds, "n / 2", title, body, and the key to go on.
constexpr float kCardTitlePx = 30.0f, kCardBodyPx = 23.0f, kCardSmallPx = 18.0f;

// Greedy wrap by measured width (at spaces when it can, else between characters).
std::vector<std::wstring> WrapLines(TextRenderer& body, const wchar_t* text, float px, float wrapW,
                                    size_t maxLines) {
    std::vector<std::wstring> lines;
    if (!text || body.LineHeightPixels() <= 0.0f) return lines;
    const float scale = px / body.LineHeightPixels();
    std::wstring cur;
    size_t lastSpace = std::wstring::npos;
    for (const wchar_t* p = text; *p && lines.size() < maxLines; ++p) {
        if (*p == L' ') lastSpace = cur.size();
        cur += *p;
        if (cur.size() > 1 && body.Width(cur.c_str(), scale) > wrapW) {
            std::wstring rest;
            if (lastSpace != std::wstring::npos && lastSpace > 0) {
                rest = cur.substr(lastSpace + 1);
                cur.resize(lastSpace);
            } else {
                rest = cur.substr(cur.size() - 1);
                cur.pop_back();
            }
            lines.push_back(cur);
            cur = rest;
            lastSpace = cur.find_last_of(L' ');
        }
    }
    if (!cur.empty() && lines.size() < maxLines) lines.push_back(cur);
    return lines;
}

void DrawHudGuideCard(const PracticeView& v, ready_launch::HudGuide card, const Canvas& cv,
                      TextRenderer& body, float now) {
    using ready_launch::HudGuide;
    if (v.focusW <= 0.0f || v.focusH <= 0.0f || body.LineHeightPixels() <= 0.0f) return;
    const float since = ready_launch::g_HudGuideT -
                        (card == HudGuide::Hp ? ready_launch::kHudGuideDelay : 0.0f);
    float in = std::clamp(since / 0.30f, 0.0f, 1.0f);
    in = in * in * (3.0f - 2.0f * in);
    const float pulse = 0.5f + 0.5f * std::sin(now * 3.0f);

    // Corners close in on the part as the card opens.
    const float grow = (1.0f - in) * cv.S(14.0f);
    const float rx = v.focusX - grow, ry = v.focusY - grow;
    const float rw = v.focusW + 2.0f * grow, rh = v.focusH + 2.0f * grow;
    BindMainShader();
    DrawGuideCorners(rx, ry, rw, rh, std::min(cv.S(28.0f), std::min(rw, rh) * 0.25f), cv.S(1.8f),
                     (0.70f + 0.30f * pulse) * in);

    const int idx = card == HudGuide::Hp ? 0 : 1;
    const wchar_t* title = ReadyHudGuide(idx, 0);
    const wchar_t* text = ReadyHudGuide(idx, 1);
    const wchar_t* footer = ReadyHudGuideNext();
    const float titlePx = cv.T(kCardTitlePx), bodyPx = cv.T(kCardBodyPx), smallPx = cv.T(kCardSmallPx, kMinSmallPx);
    const float wrapW = cv.S(600.0f);
    const std::vector<std::wstring> lines = WrapLines(body, text, bodyPx, wrapW, 4);
    const float headH = titlePx * 1.25f, lineH = bodyPx * 1.45f, footH = smallPx * 1.3f;
    const float blockH = headH + cv.S(10.0f) + (float)lines.size() * lineH + cv.S(8.0f) + footH;

    // Place: under the health ring (above it if that runs past the XP line),
    // above the XP line. Kept inside the field.
    const float fieldL = cv.X(kFieldX), fieldR = cv.X(kFieldX + kFieldW);
    const float fieldT = cv.Y(kFieldY), fieldB = cv.Y(kFieldY + kFieldH);
    const float gap = cv.S(18.0f);
    float top = card == HudGuide::Xp ? v.focusY - gap - blockH : v.focusY + v.focusH + gap;
    if (card == HudGuide::Hp && top + blockH > fieldB - cv.S(64.0f)) top = v.focusY - gap - blockH;
    top = std::max(top, fieldT + cv.S(12.0f));
    top += (1.0f - in) * cv.S(8.0f);
    float x = std::min(v.focusX + cv.S(8.0f), fieldR - cv.S(16.0f) - wrapW);
    x = std::max(x, fieldL + cv.S(16.0f));

    // Row 1: progress diamonds, step count, title.
    const float rowY = top + headH * 0.5f;
    const float key = cv.S(1.5f);
    for (int k = 0; k < ready_launch::kHudGuideCards; ++k) {
        const float dx = x + cv.S(4.0f) + (float)k * cv.S(15.0f);
        if (k <= idx) hud_lines::SectorFill(dx, rowY, cv.S(4.5f), hud_lines::FillShape::Diamond, 1.0f, kGold, 0.9f * in);
        hud_lines::DiamondOutline(dx, rowY, cv.S(4.5f), cv.S(1.4f), kGold, (k <= idx ? 0.95f : 0.40f) * in, key);
    }
    float tx = x + cv.S(4.0f) + (float)(ready_launch::kHudGuideCards - 1) * cv.S(15.0f) + cv.S(18.0f);
    wchar_t stepBuf[16];
    swprintf_s(stepBuf, L"%d / %d", idx + 1, ready_launch::kHudGuideCards);
    Text(body, stepBuf, tx, rowY, smallPx, kText, 0.85f * in);
    tx += body.Width(stepBuf, smallPx / body.LineHeightPixels()) + cv.S(14.0f);
    Text(body, title, tx, rowY, titlePx, kGold, in);

    float y = top + headH + cv.S(10.0f) + lineH * 0.5f;
    for (const std::wstring& l : lines) {
        Text(body, l.c_str(), x, y, bodyPx, kWhite, 0.95f * in);
        y += lineH;
    }
    // Footer: the key to go on, breathing a little so it reads as the action.
    y += cv.S(8.0f) - lineH * 0.5f + footH * 0.5f;
    Text(body, footer, x, y, smallPx, kGold, (0.70f + 0.25f * pulse) * in);
}

// ── Step cards ──
// The same look as the HUD cards, for every practice step and the launch:
// gold corners on what the step uses (its keys on the keyboard panel, the
// mouse; the Swarms get target brackets in the field), and a card in the
// field with a picture of that thing (the panel's own key caps, bigger),
// "STEP n / total", the title, the text and the key to begin.
constexpr float kCardPitch = 66.0f;      // key unit on the card's picture, canvas px
constexpr float kCardPicMinW = 160.0f, kCardPicMaxW = 420.0f, kCardPicH = 150.0f;
constexpr float kCardTextW = 440.0f, kCardColGap = 32.0f;

kb::KeyRole CardRole(ready_launch::HudGuide card) {
    using ready_launch::HudGuide;
    switch (card) {
    case HudGuide::Move:   return kb::KeyRole::Move;
    case HudGuide::Dash:   return kb::KeyRole::Dash;
    case HudGuide::Launch: return kb::KeyRole::Start;
    default:               return kb::KeyRole::None;
    }
}

rp::Objective CardObjective(ready_launch::HudGuide card) {
    using ready_launch::HudGuide;
    switch (card) {
    case HudGuide::Move:  return rp::Objective::Move;
    case HudGuide::Shoot: return rp::Objective::Shoot;
    case HudGuide::Dash:  return rp::Objective::Dash;
    case HudGuide::Kill:  return rp::Objective::Kill;
    default:              return rp::Objective::Count;
    }
}

// The card for the step the tutorial is on (Done when there is none).
ready_launch::HudGuide StepCardFor(rp::Objective current) {
    using ready_launch::HudGuide;
    using Stage = ready_launch::TutorialStage;
    if (ready_launch::g_Stage == Stage::Done) return HudGuide::Launch;
    if (ready_launch::g_Stage != Stage::Practice) return HudGuide::Done;
    switch (current) {
    case rp::Objective::Move:  return HudGuide::Move;
    case rp::Objective::Shoot: return HudGuide::Shoot;
    case rp::Objective::Dash:  return HudGuide::Dash;
    case rp::Objective::Kill:  return HudGuide::Kill;
    default:                   return HudGuide::Done;
    }
}

void DrawStepCard(PracticeView& v, ready_launch::HudGuide card, const Canvas& cv,
                  TextRenderer& body, float now, bool dashOk) {
    using ready_launch::HudGuide;
    if (body.LineHeightPixels() <= 0.0f) return;
    // The defeat card's words come in as the brackets start closing on the Swarms.
    const float lead = card == HudGuide::Kill ? 0.18f : 0.0f;
    float in = std::clamp((ready_launch::g_HudGuideT - lead) / 0.30f, 0.0f, 1.0f);
    in = in * in * (3.0f - 2.0f * in);
    const float pulse = 0.5f + 0.5f * std::sin(now * 3.0f);
    const float keyPulse = 0.72f + 0.28f * std::sin(now * kPulseSpeed);
    const float key = cv.S(1.5f);
    BindMainShader();

    // Corners on the panel: the step's keys, or the mouse. They close in as
    // the card opens.
    const kb::KeyRole role = CardRole(card);
    const std::vector<const kb::KeyCap*> caps = role != kb::KeyRole::None ? RoleCaps(role)
                                                                         : std::vector<const kb::KeyCap*>{};
    auto corners = [&](float x0, float y0, float x1, float y1) {
        const float grow = (1.0f - in) * cv.S(14.0f) + cv.S(8.0f);
        const float rx = x0 - grow, ry = y0 - grow;
        const float rw = x1 - x0 + 2.0f * grow, rh = y1 - y0 + 2.0f * grow;
        DrawGuideCorners(rx, ry, rw, rh, std::min(cv.S(22.0f), std::min(rw, rh) * 0.3f), cv.S(1.8f),
                         (0.70f + 0.30f * pulse) * in);
    };
    if (!caps.empty()) {
        // Every cap that does it gets corners (both Shifts dash); caps that
        // touch (W A S D) share one set.
        const std::vector<CapBox> boxes =
            CapClusters(cv, [role](const kb::KeyCap& cap) { return kb::RoleOf(cap.key) == role; });
        for (const CapBox& b : boxes) corners(b.x0, b.y0, b.x1, b.y1);
    } else if (card == HudGuide::Shoot) {
        float x, y, w, h;
        MousePanelRect(cv, x, y, w, h);
        corners(x, y, x + w, y + h);
    }

    // Picture size.
    float unitsX0 = 1e9f, unitsY0 = 1e9f, unitsX1 = -1e9f, unitsY1 = -1e9f;
    for (const kb::KeyCap* c : caps) {
        unitsX0 = std::min(unitsX0, c->x); unitsY0 = std::min(unitsY0, c->y);
        unitsX1 = std::max(unitsX1, c->x + c->w); unitsY1 = std::max(unitsY1, c->y + c->h);
    }
    const float unitsW = caps.empty() ? 0.0f : unitsX1 - unitsX0;
    const float unitsH = caps.empty() ? 0.0f : unitsY1 - unitsY0;
    // The defeat card has no picture: the Swarms in the field are lit and
    // bracketed, and a drawn one on the card would read as a fifth target.
    const bool hasPic = card != HudGuide::Kill;
    const float picW = hasPic ? cv.S(std::clamp(unitsW * kCardPitch, kCardPicMinW, kCardPicMaxW)) : 0.0f;
    const float picH = hasPic ? cv.S(kCardPicH) : 0.0f;
    const float colGap = hasPic ? cv.S(kCardColGap) : 0.0f;

    // Text.
    const int idx = (int)card - (int)HudGuide::Move;
    const wchar_t* title = ReadyStepCard(idx, 0);
    const wchar_t* text = ReadyStepCard(idx, 1);
    const wchar_t* footer = ReadyStepCardGo(card == HudGuide::Launch);
    const float titlePx = cv.T(kCardTitlePx + 2.0f), bodyPx = cv.T(kCardBodyPx);
    const float smallPx = cv.T(kCardSmallPx, kMinSmallPx);
    const float textW = cv.S(kCardTextW);
    const std::vector<std::wstring> lines = WrapLines(body, text, bodyPx, textW, 4);
    const float headH = titlePx * 1.3f, lineH = bodyPx * 1.45f, footH = smallPx * 1.4f;
    const float textH = headH + cv.S(10.0f) + (float)lines.size() * lineH + cv.S(10.0f) + footH;
    const float blockH = std::max(picH, textH);
    const float blockW = picW + colGap + textW;

    // Place: in the half of the field away from the ship (the defeat card sits
    // in the band between the two rows of Swarms), centred across.
    const float fieldT = cv.Y(kFieldY), fieldB = cv.Y(kFieldY + kFieldH);
    const float fieldMid = cv.Y(kFieldY + kFieldH * 0.5f);
    const float shipY = cv.Y(kFieldY) + v.field.playerY * kViewScale * cv.k;
    float top;
    if (card == HudGuide::Kill) top = fieldMid - blockH * 0.5f;
    else if (shipY > fieldMid) top = fieldT + cv.S(28.0f);
    else top = fieldB - cv.S(60.0f) - blockH;
    top += (1.0f - in) * cv.S(8.0f);
    const float left = cv.X(kFieldX + kFieldW * 0.5f) - blockW * 0.5f;
    const float picCx = left + picW * 0.5f, picCy = top + blockH * 0.5f;

    // Picture.
    if (!caps.empty()) {
        const float pitch = std::min(cv.S(kCardPitch), picW / std::max(1.0f, unitsW));
        const float ts = pitch / cv.S(kKeyPitch);
        const float ox = picCx - unitsW * pitch * 0.5f, oy = picCy - unitsH * pitch * 0.5f;
        const float gap = cv.S(kKeyGap) * ts;
        CapLook L;
        L.c = RoleColor(role);
        L.a = keyPulse * in; L.ta = in; L.filled = true;
        L.fillA = (kCurrentFillA * keyPulse + 0.12f) * in; L.strong = true; L.active = true;
        for (const kb::KeyCap* c : caps)
            DrawCap(*c, ox + (c->x - unitsX0) * pitch, oy + (c->y - unitsY0) * pitch,
                    c->w * pitch - gap, c->h * pitch - gap, ts, L, cv, body);
    } else if (card == HudGuide::Shoot) {
        const float s = 0.78f;
        const float mw = cv.S(kMouseW) * s, mh = cv.S(kMouseH) * s;
        const float saved = g_BatchAlpha;
        g_BatchAlpha = saved * in;
        DrawMouse(picCx - mw * 0.5f, picCy - mh * 0.5f, cv, GuideLook{ keyPulse, true }, false, now, s);
        g_BatchAlpha = saved;
    }
    (void)key;

    // Row 1: "STEP n / total", then the title.
    const float x = left + picW + colGap;
    float y = top + (blockH - textH) * 0.5f;
    const float rowY = y + headH * 0.5f;
    float tx = x;
    if (card != HudGuide::Launch) {
        int index = 0, total = 0;
        StepNumber(CardObjective(card), dashOk, index, total);
        const std::wstring step = std::wstring(kReadyStepLabel) + L" " + std::to_wstring(index) +
                                  L" / " + std::to_wstring(total);
        Text(body, step.c_str(), tx, rowY, smallPx, kGold, 0.90f * in);
        tx += body.Width(step.c_str(), smallPx / body.LineHeightPixels()) + cv.S(16.0f);
    }
    Text(body, title, tx, rowY, titlePx, kGold, in);
    y += headH + cv.S(10.0f) + lineH * 0.5f;
    for (const std::wstring& l : lines) {
        Text(body, l.c_str(), x, y, bodyPx, kWhite, in);
        y += lineH;
    }
    // Footer: the key to begin, breathing a little so it reads as the action.
    y += cv.S(10.0f) - lineH * 0.5f + footH * 0.5f;
    Text(body, footer, x, y, smallPx, kGold, (0.72f + 0.25f * pulse) * in);
}

} // namespace

void Ready_SmokeCompleteSteps(int steps) {
    PracticeView& v = View();
    TouchView(v);
    rp::Field& f = v.field;
    // Past the first step the guide cards have been read (TUTGUIDE= shows one again).
    if (steps > 0) {
        ready_launch::g_HudGuide = ready_launch::HudGuide::Done;
        // ...all but the launch card, which follows the card pick still ahead.
        ready_launch::g_StepCardsShown = ~(1u << (unsigned)ready_launch::HudGuide::Launch);
    }
    if (steps > 0) f.moved = rp::kMoveGoal * f.worldH;
    if (steps > 1) f.shots = rp::kShotGoal;
    if (steps > 2) f.dashes = rp::kDashGoal;
    if (steps > 3) f.kills = rp::kKillGoal;
}

void Scene_Ready(const SceneCtx& c, const SceneTextContext& text) {
    const Canvas cv = FitCanvas(c.sw, c.sh);
    const float now = (float)glfwGetTime();
    const float dt = std::clamp(c.delta, 0.0f, 0.05f);

    // Launch animation clock; the run starts when the wall has swollen out.
    if (ready_launch::Launching()) {
        ready_launch::g_LaunchT += dt;
        if (ready_launch::g_LaunchT >=
            ready_launch::kLaunchShrinkSec + ready_launch::kLaunchExpandSec) {
            ready_launch::CancelLaunch();
            ready_launch::g_RunIntro = 0.0f;
            ready_launch::g_StartGraceT = ready_launch::kStartGraceSec;
            g_UiShakePx = 0.0f;
            if (ready_launch::g_PicksTaken) {
                // The tutorial's card (and its level-up) was only a demonstration:
                // the run starts from a clean loadout.
                ready_launch::g_PicksTaken = false;
                if (c.reset) c.reset();
                FinalizeLoadout(c, FixedWeaponForSelectedJob());
            }
            ready_launch::g_TutorialMode = false;
            g_Difficulty = Difficulty::EASY;
            g_GameManager.currentState = GameState::RUNNING;
            return;
        }
    }
    const bool launching = ready_launch::Launching();
    {   // Space 출격: 필드가 조여드는 순간 한 번
        static bool s_chargeSfx = false;
        if (!launching) s_chargeSfx = false;
        else if (!s_chargeSfx) { s_chargeSfx = true; Audio::Play(Audio::Cue::RunLaunchCharge); }
    }
    ready_launch::g_LaunchLockedT = std::max(0.0f, ready_launch::g_LaunchLockedT - dt);

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
    // The HUD guide ends a toggled burst; a card screen does not (it carries on after the pick).
    if (ready_launch::HudGuideActive()) g_FireLatched = false;
    // A left click moves the HUD guide on too (same as Space); that click
    // does not shoot or toggle the fire latch.
    static bool s_GuideLmbPrev = false;
    const bool guideClick = c.lmb && !s_GuideLmbPrev;
    s_GuideLmbPrev = c.lmb;
    const bool guideTookClick = guideClick && ready_launch::HudGuideActive() &&
        !ready_launch::g_SelectionUp && !ready_launch::Launching() &&
        g_GameManager.currentState != GameState::PAUSED;
    in.fireHeld = ResolveFireInput(c.lmb, g_LmbPrev, !ready_launch::g_SelectionUp);
    if (guideTookClick) {
        const ready_launch::HudGuide guideBefore = ready_launch::g_HudGuide;
        ready_launch::HudGuideConfirm();
        if (ready_launch::g_HudGuide != guideBefore) Audio::Play(Audio::Cue::UiConfirm);
        in.fireHeld = false;
        g_FireLatched = false;
    }
    in.aimValid = true;
    in.aimX = ((float)c.mx - cv.X(kFieldX)) / (kViewScale * cv.k);
    in.aimY = ((float)c.my - cv.Y(kFieldY)) / (kViewScale * cv.k);
    // While a card screen is up this scene only draws; the field stands still.
    const bool picking = ready_launch::g_SelectionUp;
    // The field stands still under a card screen and during the launch.
    const bool paused = g_GameManager.currentState == GameState::PAUSED;   // tutorial pause menu
    // Tutorial HUD guide: the field waits while its cards are up.
    const bool guideUp = ready_launch::HudGuideActive();
    if (guideUp && !picking && !launching && !paused) {
        // The first card opens with a tab sound; the press that turns to the
        // second one already plays its confirm.
        const bool wasShown = ready_launch::HudGuideShown();
        ready_launch::g_HudGuideT += dt;
        if (!wasShown && ready_launch::HudGuideShown()) Audio::Play(Audio::Cue::UiTab);
    }
    const bool guideShown = ready_launch::HudGuideShown();
    v.guideDim = UiApproach(v.guideDim, guideShown ? 1.0f : 0.0f, dt, kGuideDimEase);
    const bool still = picking || launching || paused || guideUp;
    if (still) in = rp::Input{};
    bool dummyWasAlive[rp::kDummyCount];
    for (int i = 0; i < rp::kDummyCount; ++i) dummyWasAlive[i] = v.field.dummies[i].Alive();
    rp::Step(v.field, rules, in, still ? 0.0f : dt);
    for (int i = 0; i < rp::kDummyCount; ++i) {
        const rp::Dummy& d = v.field.dummies[i];
        // 이번 프레임에 맞으면 hitFlash 가 막 kHitFlash 로 채워져 있다 (감쇠가 먼저 돈다)
        if (!still && dt > 0.0f && d.hitFlash >= rp::kHitFlash) Audio::Play(Audio::Cue::TutDummyHit);
        if (!dummyWasAlive[i] && d.Alive()) Audio::Play(Audio::Cue::TutDummyRespawn);
    }

    const bool dashOk = rules.dashAvailable;
    const rp::Objective current = rp::CurrentObjective(v.field, dashOk);
    {   // 단계 완료 → 다음 단계 해금
        static int s_prevObjective = -1;
        if (s_prevObjective >= 0 && (int)current > s_prevObjective) Audio::Play(Audio::Cue::TutStepClear);
        s_prevObjective = (int)current;
    }
    if (ready_launch::g_TutorialMode && !picking && !launching && !paused) {
        using Stage = ready_launch::TutorialStage;
        switch (ready_launch::g_Stage) {
        case Stage::Practice:
            if (current == rp::Objective::Count) {
                // The fourth kill is the level-up: open the real card screen.
                ready_launch::g_Stage = Stage::AugPick;
                GameManager& gm = g_GameManager;
                ++gm.playerLevel;
                gm.xp = 0;
                gm.OpenTutorialChoice();
                ready_launch::OpenPickGuide();   // the card screen explains itself first
            }
            break;
        case Stage::AugPick:
            // Back on this screen: the card was taken. Finishing every step
            // unlocks the rest of the lobby.
            ready_launch::g_Stage = Stage::Done;
            if (!g_TutorialDone) {
                g_TutorialDone = true;
                g_TutorialVanishPending = true;   // the lobby glitches TUTORIAL out once
                SaveGame();
            }
            break;
        case Stage::Done:
            break;
        }
        // Every step opens with its card, once per tutorial (after the HUD cards).
        // The first step's card follows the HUD cards at once; a later one waits
        // until the move that cleared the step before it has played out (a dash
        // must be seen to land before the field stops).
        using ready_launch::HudGuide;
        const HudGuide card = StepCardFor(rp::CurrentObjective(v.field, dashOk));
        if (card != v.cardPending) { v.cardPending = card; v.cardWait = 0.0f; }
        if (!ready_launch::HudGuideActive()) v.cardWait += dt;
        // How long each card waits: ready_launch::kStepGap.
        const float wait = ready_launch::StepGap(card);
        if (card != HudGuide::Done && v.cardWait >= wait && !v.field.dashActive &&
            ready_launch::OpenStepCard(card))
            Audio::Play(Audio::Cue::UiTab);
    }
    const auto targets = GuideTargets(v.field, current, dashOk);
    const float ease = 1.0f - std::exp(-dt * kGlowEase);
    for (int i = 0; i < kGuideCount; ++i) v.glow[i] += (targets[i] - v.glow[i]) * ease;
    const float pulse = 0.72f + 0.28f * std::sin(now * kPulseSpeed);

    if (launching) {
        namespace rl = ready_launch;
        const float t = rl::g_LaunchT;
        auto frame = [&](float fx, float fy, float fw, float fh, float a) {
            const float key = cv.S(1.5f);
            Box(fx, fy, fw, fh, cv.S(1.3f) + cv.S(1.4f) * a, kCyan, 0.32f + 0.55f * a, key);
            const float L = cv.S(kFieldCorner), th = cv.S(2.4f);
            const float cx[4] = { fx, fx + fw, fx + fw, fx }, cy[4] = { fy, fy, fy + fh, fy + fh };
            const float dx[4] = { 1, -1, -1, 1 }, dy[4] = { 1, 1, -1, -1 };
            for (int i = 0; i < 4; ++i) {
                Line(cx[i] + dx[i] * L, cy[i], cx[i], cy[i], th, kCyan, 0.75f + 0.2f * a, key);
                Line(cx[i], cy[i], cx[i], cy[i] + dy[i] * L, th, kCyan, 0.75f + 0.2f * a, key);
            }
        };
        BindMainShader();
        const float fcx = cv.X(kFieldX + kFieldW * 0.5f), fcy = cv.Y(kFieldY + kFieldH * 0.5f);
        // The page (title, notes, objectives, keyboard) fades by alpha, not by a
        // dark cover: g_BatchAlpha scales both geometry and text.
        auto drawPage = [&](float alpha) {
            if (alpha <= 0.004f) return;
            g_BatchAlpha = alpha;
            DrawTitle(cv, text.title, 1.0f);
            DrawObjectives(v.field, current, dashOk, cv, text.body, pulse);
            DrawKeyboard(v, current, cv, text.body, pulse, false, now);
            BatchFlush();
            g_BatchAlpha = 1.0f;
            BindMainShader();
        };
        // The practice field at the bottom of the squeeze, as a canvas.
        const float sqz = rl::kLaunchShrinkTo;
        const float k0 = cv.k * sqz;
        const float ox0 = fcx - (kFieldX + kFieldW * 0.5f) * k0;
        const float oy0 = fcy - (kFieldY + kFieldH * 0.5f) * k0;
        static bool s_burstDone = false;
        if (t < 0.05f) s_burstDone = false;
        if (t < rl::kLaunchShrinkSec) {
            // Squeeze: the page fades back and the field draws a little smaller.
            const float p = t / rl::kLaunchShrinkSec;
            const float e = 1.0f - (1.0f - p) * (1.0f - p);
            const float sc = 1.0f - (1.0f - rl::kLaunchShrinkTo) * e;
            drawPage(1.0f - 0.75f * e);
            Canvas cs = cv;
            cs.k = cv.k * sc;
            cs.ox = fcx - (kFieldX + kFieldW * 0.5f) * cs.k;
            cs.oy = fcy - (kFieldY + kFieldH * 0.5f) * cs.k;
            DrawField(v, rules, current, dashOk, cs, text.body, now, dt);
        } else {
            // Swell: the wall grows out over the keyboard and the notes. It
            // reaches the screen edge at 80% of the swell; the dark floor then
            // fades and the game shows through as the run starts.
            const float p = std::min(1.0f, (t - rl::kLaunchShrinkSec) / rl::kLaunchExpandSec);
            const float reach = std::min(1.0f, p / 0.8f);
            const float e = reach * reach * reach * (reach * (reach * 6.0f - 15.0f) + 10.0f);
            const float fadeP = std::max(0.0f, (p - 0.8f) / 0.2f);
            const float x0 = fcx - cv.S(kFieldW) * 0.5f * sqz, y0 = fcy - cv.S(kFieldH) * 0.5f * sqz;
            const float w0 = cv.S(kFieldW) * sqz, h0 = cv.S(kFieldH) * sqz;
            const float inset = cv.S(6.0f) * (1.0f - e);
            const float fx = x0 + (inset - x0) * e, fy = y0 + (inset - y0) * e;
            const float fw = w0 + (c.sw - 2.0f * inset - w0) * e;
            const float fh = h0 + (c.sh - 2.0f * inset - h0) * e;

            // The page is already faint; it finishes fading in the first half.
            const float pf = std::min(1.0f, p / 0.5f);
            drawPage(0.25f * (1.0f - pf * pf * (3.0f - 2.0f * pf)));

            // The dummies break into shards at the start of the swell.
            const Rgb swarm{ v.mobs[0]->color.r, v.mobs[0]->color.g, v.mobs[0]->color.b };
            const Rgb swarmText = Lighten(swarm, 0.45f);
            if (!s_burstDone) {
                s_burstDone = true;
                Audio::Play(Audio::Cue::RunLaunchBurst);   // 필드 확장 → 런 인트로
                for (int i = 0; i < rp::kDummyCount; ++i) {
                    const rp::Dummy& d = v.field.dummies[i];
                    if (!d.Alive()) continue;
                    SpawnShards(v, ox0 + (kFieldX + rp::DummyX(d) * kViewScale) * k0,
                                oy0 + (kFieldY + rp::DummyY(d) * kViewScale) * k0, k0);
                }
            }

            // Dark floor inside the wall.
            drawRect(fx, fy, fw, fh, 0.012f, 0.026f, 0.060f, 0.96f * (1.0f - fadeP));
            BatchFlush();
            BindMainShader();
            for (Shard& sh2 : v.shards) {
                sh2.x += sh2.vx * dt; sh2.y += sh2.vy * dt; sh2.ang += sh2.spin * dt; sh2.life -= dt;
                const float hl = k0 * 8.0f * 1.5f, ca = std::cos(sh2.ang) * hl, sa = std::sin(sh2.ang) * hl;
                Line(sh2.x - ca, sh2.y - sa, sh2.x + ca, sh2.y + sa, k0 * 2.2f, swarmText,
                     std::max(0.0f, sh2.life / kShardLife), 0.0f);
            }
            v.shards.erase(std::remove_if(v.shards.begin(), v.shards.end(),
                [](const Shard& sd) { return sd.life <= 0.0f; }), v.shards.end());

            // The ship leaves its spot, glides to the screen centre and grows to
            // its run size with a small overshoot, where the run will start.
            {
                const float ws0 = kViewScale * k0;
                const float sx0 = ox0 + (kFieldX + v.field.playerX * kViewScale) * k0;
                const float sy0 = oy0 + (kFieldY + v.field.playerY * kViewScale) * k0;
                const float shipX = sx0 + (c.sw * 0.5f - sx0) * e;
                const float shipY = sy0 + (c.sh * 0.5f - sy0) * e;
                const float u = e - 1.0f;
                const float eBack = 1.0f + 2.0f * u * u * u + 1.0f * u * u;   // ease-out-back, ~4% over
                const float shipRunSize = PLAYER_SIZE_PX * g_Stats.playerSizeMult;
                const float shipScale = ws0 + (1.0f - ws0) * eBack;
                DrawPlayerWeaponShell(shipX, shipY, shipRunSize * shipScale,
                                      v.field.aim, now);
            }
            frame(fx, fy, fw, fh, e);
            if (p > 0.8f) {
                // Arrival: the wall's edge flares once and the screen gives a nudge.
                const float flash = 1.0f - fadeP;
                Box(fx, fy, fw, fh, cv.S(5.0f), kCyan, 0.55f * flash * flash, 0.0f);
                g_UiShakePx = 4.0f * cv.k * flash * flash;
            }
        }
        BatchFlush();
        return;
    }

    // HUD guide: the page around the part in focus goes faint (alpha, no veil).
    const float pageA = 1.0f - (1.0f - kGuideRestA) * v.guideDim;
    const ready_launch::HudGuide focusCard =
        guideShown ? ready_launch::g_HudGuide : ready_launch::HudGuide::Done;
    BindMainShader();
    g_BatchAlpha = pageA;
    DrawTitle(cv, text.title, 1.0f);
    g_BatchAlpha = 1.0f;

    BindMainShader();
    DrawField(v, rules, current, dashOk, cv, text.body, now, dt, focusCard);
    g_BatchAlpha = pageA;
    DrawObjectives(v.field, current, dashOk, cv, text.body, pulse);
    if (ready_launch::g_TutorialMode) {
        // (The card pick explains itself on its own screen; no note under the field.)
        if (ready_launch::g_Stage != ready_launch::TutorialStage::AugPick &&
            ready_launch::g_LaunchLockedT > 0.0f) {
            // Space before every step is cleared: say why nothing happened.
            const float t = ready_launch::g_LaunchLockedT;
            const float a = std::min(1.0f, t / 0.4f) *
                            std::min(1.0f, (ready_launch::kLaunchLockedSec - t) / 0.12f);
            Text(text.body, ReadyLaunchLocked(), cv.X(kCanvasW * 0.5f),
                 cv.Y(kFieldY + kFieldH + 32.0f), cv.T(26.0f), kAmber, a, Align::Center);
        }
    }
    DrawKeyboard(v, current, cv, text.body, pulse, c.lmb, now,
                 CardRole(focusCard), focusCard == ready_launch::HudGuide::Shoot);
    g_BatchAlpha = 1.0f;
    if (ready_launch::IsStepCard(focusCard)) DrawStepCard(v, focusCard, cv, text.body, now, dashOk);
    else if (focusCard != ready_launch::HudGuide::Done) DrawHudGuideCard(v, focusCard, cv, text.body, now);
    BatchFlush();
}
