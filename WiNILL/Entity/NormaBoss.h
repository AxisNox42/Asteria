#pragma once

// ─────────────────────────────────────────────────────────────
// NORMA, the window boss (Docs/Norma_Boss_Design.md). Placeholder body: a
// plain square. It appears once, on the final (boss) wave, and the run is won
// when it falls. This file holds the numbers and the pure rules the boss,
// the game loop and the HUD share.
//
// Patterns: window patterns and the cross barrage take turns.
//   2 Refuge      open shelters near the player (reusing open ones), then
//                 flood the field
//   3 CloseWindow (phase 2) count down the pane the player stands in, then
//                 crush everything inside it
//   8 Throw       fill a small pane with shots, hurl it at the player, and
//                 burst it where it lands
//   7 Aim         a stream that keeps tracking the player
//  14 Warp        (phase 2) slip into a pane, hide, then smash out of another one
//  13 Wall        pop big panes open one after another (0.1s apart) in a cascade
//                 between the player and the boss: a still stack that blocks, then blows
//  12 Detonate    set every pane the boss has open to blow, then, once they
//                 have, open fresh ones elsewhere
// The boss cannot touch OS windows itself: it posts Asks, and the game loop
// serves them through os_pane and writes the result back on the boss.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <vector>

namespace norma {

// Hit points before the run's enemy HP scaling. Tuned for a level-20 player
// (about 19 augments) reaching the boss wave: the real boss then carries
// about 2.4x this (the run's HP ramp at the final wave), roughly two minutes
// of landed fire at the damage a level-20 build is expected to deal. The
// bare level-1 gun (50 dmg every 0.2s, 250/s) needs over two minutes even
// against the unscaled figure.
constexpr float kBaseHp = 30000.0f;
// Phase 2 begins at or below this share of max HP: each phase holds half of
// it, so both bars drain at the same pace.
constexpr float kPhase2Ratio = 0.50f;
// The boss keeps about this far from the player, drifting slowly.
constexpr float kHoverDistance = 420.0f;
constexpr float kHoverBand = 60.0f;
constexpr float kMoveSpeed = 70.0f;
// The drift eases in and out: speed climbs from 0 over kHoverRamp px beyond the
// band, and the body gains or sheds it at kHoverAccel px/s per second, so it
// never lurches between standing and moving.
constexpr float kHoverRamp = 140.0f;
constexpr float kHoverAccel = 150.0f;
constexpr float kCoastDecay = 5.0f;         // 1/s: how fast a loose drift dies out
constexpr float kSizeScale = 3.0f;
constexpr float kContactDps = 12.0f;
// Player shots hit the star's inner body (its inner corners sit at ~1.38
// base), the same reach as contact (26 px per sizeScale, Monster::TryContact):
// what can touch you is what you can hit.
constexpr float kShotHitPx = 26.0f;         // px per sizeScale
// How long the phase-change flash lasts.
constexpr float kPhaseFlashSeconds = 1.2f;

// Pattern timings and shots (first guesses, Normal difficulty).
constexpr float kRecoverSeconds = 0.7f;     // opening between patterns
constexpr float kRecoverPhase2 = 0.4f;
constexpr float kShotDamage = 9.0f;
// Every boss shot flies this much faster in phase 2.
constexpr float kPhase2ShotSpeed = 1.15f;
// 2 Refuge: the flood; the boss's open panes are the shelters.
constexpr float kRefugeTelegraph = 1.5f;
// If no pane is within reach (scaled by the player's move speed), the nearest
// one glides over to kRefugeGap of reach from the player in kRefugePull
// seconds, early in the telegraph; with none open, one opens there.
constexpr float kRefugeReach = 320.0f;      // px at move speed x1
constexpr float kRefugeReachMin = 180.0f, kRefugeReachMax = 420.0f;
constexpr float kRefugeGap = 0.5f;
constexpr float kRefugePull = 0.8f;
constexpr float kRefugeSeconds = 4.0f;
constexpr float kRefugeWaveInterval = 0.6f;
constexpr int   kRefugeShots = 30;
constexpr float kRefugeShotSpeed = 290.0f;
constexpr float kRefugeLinger = 1.0f;       // pause before the next pattern
// 12 Detonate: the panes in use are shut down on purpose (their fuse, see
// space::kFuseSeconds), then the boss opens new ones.
constexpr float kDetonateFuseWait = 1.8f;   // fuse plus a beat for the panes to fold shut
constexpr float kDetonateReopen = 1.2f;     // the new panes' laser and bracket snap
// 15 Reinforce: when the boss is short of panes it opens more (up to the phase's
// count), with a beat to show it first. Picked when 2 or fewer stand; when the
// full count stands, Detonate (blowing them all on purpose) is picked more often.
constexpr float kReinforceTelegraph = 0.7f;
constexpr int   kReinforceBelow = 2;
// How many panes the boss has open, kept current by the game loop; -1 unknown.
inline int g_PaneCount = -1;
constexpr float kDetonateBurstShare = 0.5f; // share of a thrown box's load a blown pane scatters
// 3 CloseWindow
constexpr float kCloseCountdown = 3.0f;
constexpr float kCloseDamage = 35.0f;       // to the player left inside
constexpr float kCloseMobDamage = 200.0f;   // to mobs left inside

// 7 Aim: the aim follows the player at a capped turn rate, so moving steadily
// across the stream gets out of it.
constexpr float kAimTelegraph = 0.8f;
constexpr float kAimSeconds = 3.5f;
constexpr float kAimStreamInterval = 0.11f;
constexpr float kAimStreamSpeed = 500.0f;
constexpr float kAimTrackTelegraph = 4.0f;  // rad/s while warning
constexpr float kAimTrackRate = 1.8f;       // rad/s while firing
constexpr float kAimJitter = 0.04f;         // rad, alternating
constexpr float kAimMuzzle = 1.8f;          // muzzle star distance, in body units (base)
constexpr float kAimDrill = 1.6f;           // rad/s the cube turns about the aim while firing

// 8 Throw: the box grows out of the boss's body to its spot (kThrowAppear),
// opens there, sits while it fills, flies at where the player
// stood with speed growing e-fold every kThrowRamp seconds, waits a moment,
// then bursts. Closing it before then (in pause) cancels the burst.
constexpr float kThrowW = 320.0f, kThrowH = 240.0f;
constexpr float kThrowAppear = 0.6f;
constexpr float kThrowFill = 1.0f;
constexpr float kThrowStartSpeed = 60.0f;   // screen px/s
constexpr float kThrowRamp = 0.18f;
constexpr float kThrowFuse = 0.45f;
constexpr int   kThrowRing = 30;
constexpr int   kThrowInner = 14;
constexpr float kThrowRingSpeed = 330.0f;
constexpr float kThrowInnerSpeed = 180.0f;
constexpr float kThrowSeconds = 3.0f;       // the boss's share of the pattern
// The tesseract's corners, and where corner i lands on screen (unit cube,
// before roll) at the given yaw and pitch. The boss turns its cube with
// these; the drawing projects the same way.
inline constexpr float kCubeCorner[8][3] = {
    { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
    { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
inline void CubeCornerOnScreen(int i, float yaw, float pitch, float& sx, float& sy) {
    const float px = kCubeCorner[i][0], py = kCubeCorner[i][1], pz = kCubeCorner[i][2];
    const float x1 = px * std::cos(yaw) + pz * std::sin(yaw);
    const float z1 = -px * std::sin(yaw) + pz * std::cos(yaw);
    sx = x1;
    sy = py * std::cos(pitch) - z1 * std::sin(pitch);
}

// Health on the body: the outer cube's eight stars, one per eighth of HP.
// They go dark in this corner order (fixed, so a dark star rides with the
// turning cube instead of hopping between corners).
constexpr int kHealthStars = 8;
inline constexpr int kStarOutOrder[8] = { 1, 3, 4, 6, 0, 5, 2, 7 };
inline int LitStarsFor(float hp, float maxHp) {
    if (maxHp <= 0.0f || hp <= 0.0f) return 0;
    const float share = std::clamp(hp / maxHp, 0.0f, 1.0f);
    return std::max(1, (int)std::ceil(share * (float)kHealthStars - 1e-4f));
}
constexpr float kStarOutFlash = 0.35f;      // a star going dark flares this long

// 10 Ram: the boss takes one of its panes and hurls it across the screen.
// While the boss controls it the pane's glow turns red. Several panes can be
// hurled together (RamCountFor), each keeping its own size. It draws back from the
// player (aim, tracking until kRamLock of the way, then locked; no lane is
// drawn: the draw-back is the tell), then flies, speeding up e-fold every
// kRamRamp seconds, until it hits the screen edge. Its frame sweeping over
// the player (or a mob) hurts and shoves it; being inside the pane when it
// sets off is safe. Dragging it away (Ctrl) breaks the control. The pane
// keeps its small size afterwards.
constexpr float kRamAim = 1.3f;
constexpr float kRamAimPhase2 = 1.0f;
constexpr float kRamLock = 0.55f;
constexpr float kRamPullBack = 130.0f;      // screen px: the only warning, the pane draws back from where it will go
constexpr float kRamStartSpeed = 250.0f;    // screen px/s
constexpr float kRamRamp = 0.18f;
constexpr float kRamMaxSpeed = 1800.0f;
constexpr float kRamSettle = 0.5f;          // red fades back after the hit
// The pane slamming the screen edge bursts like a thrown box, at this share
// of its load, spread evenly all the way round.
constexpr float kRamBurstShare = 1.0f;
constexpr float kRamBurstFan = 6.2831853f;  // the full circle, spread evenly
constexpr float kRamDamage = 25.0f;
constexpr float kRamMobDamage = 150.0f;
constexpr float kRamKnock = 260.0f;         // world px of shove
constexpr float kRamSeconds = 3.0f;         // the boss's share of the pattern
constexpr int kRamCount = 2, kRamCountPhase2 = 3;   // panes hurled at once, at most
inline int RamCountFor(int phase) { return phase >= 2 ? kRamCountPhase2 : kRamCount; }
inline float RamAimFor(int phase) { return phase >= 2 ? kRamAimPhase2 : kRamAim; }

// 13 Wall: a spray of error windows. kWallCount panes of random size pop open
// kWallStagger apart, strewn over the ground between the boss and the player.
// They are red panes: marked and sharpened for kWallWarn first, they burst open
// with a shockwave and no one can stand inside (blue panes, the boss's usual
// ones, can be entered). The barricade stands still:
// its pane borders stop the player's shots, and it blocks the view. After
// kWallStand seconds the panes are lit to blow (the Detonate fuse), each
// scattering its share of a thrown box's load.
// Rules of the wall panes:
//   - the footprints are marked for kWallWarn first; a player standing in one
//     when its pane pops open is hurt and thrown clear (once per wall)
//   - nothing can stand inside a wall pane (the player, the boss, every mob):
//     whatever is in one is pushed out through its nearest edge, so the
//     panes are solid. Mobs and the boss are also slid along the edge toward
//     the player's side, so they go round the corner
constexpr float kWallDamage = 20.0f;
constexpr float kWallKnock = 260.0f;        // world px of shove
constexpr float kWallPlayerMargin = 14.0f;  // px kept between the player's centre and a pane's edge
constexpr float kWallDetourSpeed = 170.0f;  // px/s a mob slides along a pane's edge toward the player's side
constexpr int   kWallCount = 4;
constexpr int   kWallCountPhase2 = 6;
// Extra wall panes per difficulty (EASY, NORMAL, HARD, NIGHTMARE).
constexpr int   kWallCountByDifficulty[4] = { 0, 0, 1, 2 };
constexpr float kWallStagger = 0.1f;
constexpr float kWallHold = 0.3f;           // beat after the last pane before the stand timer
// Each pane gets a random size within these (screen px, capped by a share of the screen)
// and lands across the boss-to-player line, kWallSpread apart sideways (+- kWallJitter).
constexpr float kWallMinW = 260.0f, kWallMaxW = 560.0f;
constexpr float kWallMinH = 200.0f, kWallMaxH = 400.0f;
constexpr float kWallMaxShareW = 0.45f, kWallMaxShareH = 0.5f;
constexpr float kWallSpread = 200.0f, kWallJitter = 60.0f;
constexpr float kWallWarn = 1.2f;           // red brackets mark the footprints before any pane opens
constexpr float kWallStand = 2.5f;          // seconds the wall stays up before it is lit
constexpr float kWallBuildSeconds = 1.0f;   // the boss's telegraph
constexpr float kWallSeconds = 4.5f;        // the boss's share of the pattern
inline int WallCountFor(int phase, int difficulty) {
    return (phase >= 2 ? kWallCountPhase2 : kWallCount) +
           kWallCountByDifficulty[difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty];
}
// The sideways spread narrows past 6 panes so a big wall stays on screen.
inline float WallSpreadFor(int n) { return n > 6 ? kWallSpread * 5.0f / (float)(n - 1) : kWallSpread; }

// 14 Warp (phase 2): the boss glides into one of its panes and is gone: it
// cannot be hurt or touch anyone. The pane it will burst out of (the one near
// the player) flickers red for kWarpHide seconds. Then that pane is smashed:
// the boss stands in its place, a ring of shots and glass spray out, a player
// standing in it is hurt and thrown clear, and the boss is left open for a
// moment.
constexpr float kWarpEnter = 0.7f;          // gliding into the pane
constexpr float kWarpHide = 2.0f;           // gone, the other pane flickering
// The pane the boss went into jolts the way it went in (kWarpRecoil px,
// ringing out), then acts as a turret: it fires an aimed stream from its edge
// toward the player while the boss is inside. The pane to be smashed trembles
// harder and cracks open as the time runs out.
constexpr float kWarpRecoil = 34.0f;
constexpr float kWarpStreamDelay = 0.45f;
constexpr float kWarpStreamInterval = 0.22f;
constexpr float kWarpStreamSpeed = 420.0f;
constexpr float kWarpTremble = 7.0f;
constexpr float kWarpExpose = 1.2f;         // open after smashing out
constexpr float kWarpDamage = 25.0f;
constexpr float kWarpKnock = 260.0f;        // world px of shove
constexpr float kWarpBurstShare = 0.5f;     // share of a thrown box's load

// Phase change: every pane the boss has blows, the boss holds still and
// untouchable for kPhaseInvuln, then a fresh spray of panes pops open
// kPhaseSprayStagger apart.
constexpr float kPhaseInvuln = 1.6f;
constexpr float kPhaseSprayStagger = 0.1f;

// Panes belong to patterns: Detonate blows the boss's panes and opens
// fresh ones, and Refuge opens one when none is near; nothing opens them on a clock. The
// others (Refuge, Ram, Throw) use whatever is open. At most this many stand.
constexpr int   kMaxBossPanes = 3;
constexpr int   kMaxBossPanesPhase2 = 5;
inline int MaxBossPanesFor(int phase) { return phase >= 2 ? kMaxBossPanesPhase2 : kMaxBossPanes; }

// Sizes of the panes the boss opens, in screen pixels (random within these).
constexpr float kRefugeMinW = 420.0f, kRefugeMaxW = 640.0f;
constexpr float kRefugeMinH = 300.0f, kRefugeMaxH = 460.0f;
constexpr float kPaneMaxShareW = 0.35f, kPaneMaxShareH = 0.42f;   // of the screen
// Boss panes stand still, and every so often hop to a new spot: brackets
// mark the spot for kPaneHopWarn, then the pane glides there in kPaneHopMove.
constexpr float kPaneHoldMin = 3.5f, kPaneHoldMax = 5.0f;   // seconds between hops
constexpr float kPaneHopWarn = 0.6f;
constexpr float kPaneHopMove = 0.5f;

// Opening to hit the boss: when a rammed pane hits the screen edge it holds
// still, exposed, taking extra damage.
constexpr float kExposeAfterRam = 2.0f;
constexpr float kExposeDamage = 1.5f;

// Laser flash and bracket snap when the boss claims or opens a pane.
constexpr float kLaserSeconds = 0.25f;
constexpr float kSnapSeconds = 0.25f;
constexpr float kSnapGrow = 0.6f;           // brackets start 1.6x and close in

enum class Pattern { None = 0, Refuge = 2, CloseWindow = 3, Aim = 7,
                     Throw = 8, Ram = 10, Detonate = 12, Wall = 13, Warp = 14,
                     Reinforce = 15 };
enum class Stage { Recover, Wait, Telegraph, Act, Exit };

// Requests from the boss to the game loop, served once per frame.
enum class Ask { OpenRefuges, ClosePlayerWindow, ReleaseWindows,
                 ThrowBox, RamWindow, DetonatePanes,
                 ReopenPanes, BuildWall, WarpPlan, WarpSmash,
                 PhaseBlow, PhaseSpray };
inline std::vector<Ask> g_Asks;

// Effect cues from the boss; the game loop turns them into shockwaves,
// sparks and shake (the boss cannot reach those systems itself).
//   Volley     a stream volley left the body (a = stream angle)
//   Muzzle     one aimed shot left the body (a = its angle)
//   Burst      a pattern opened or a big ring left (a = strength 0..2)
//   Wave       a flood ring left (a = ring speed)
//   EnterPane  the boss slipped into its pane
//   LeavePane  the boss came back out
//   PhaseShift phase 2 began
enum class Fx { Volley, Muzzle, Burst, Wave, EnterPane, LeavePane, PhaseShift, StarOut };
struct Event {
    Fx fx;
    float x = 0.0f, y = 0.0f, a = 0.0f;
};
inline std::vector<Event> g_Events;

// Recoil after a volley: the body swells, then settles.
constexpr float kRecoilSeconds = 0.12f;
// Afterimages left while the boss glides into or out of a pane.
constexpr int kGhostCount = 6;
constexpr float kGhostSpacing = 0.035f;     // seconds between afterimages

// Predictive aim: the angle from (sx, sy) that meets a target at (px, py)
// moving at (vx, vy) with a shot of `speed`, scaled by `lead` (0 = aim at
// where it is now, 1 = full intercept, >1 = past it). Falls back to the
// current position when no intercept exists.
inline float LeadAngle(float sx, float sy, float px, float py, float vx, float vy,
                       float speed, float lead = 1.0f) {
    const float rx = px - sx, ry = py - sy;
    const float a = vx * vx + vy * vy - speed * speed;
    const float b = 2.0f * (rx * vx + ry * vy);
    const float c = rx * rx + ry * ry;
    float t = -1.0f;
    if (std::fabs(a) < 1e-4f) {
        if (std::fabs(b) > 1e-4f) t = -c / b;
    } else {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float root = std::sqrt(disc);
            const float t1 = (-b - root) / (2.0f * a), t2 = (-b + root) / (2.0f * a);
            t = (t1 > 0.0f && (t2 <= 0.0f || t1 < t2)) ? t1 : t2;
        }
    }
    if (t <= 0.0f) return std::atan2(ry, rx);
    t = std::min(t, 2.0f) * lead;   // never chase more than two seconds ahead
    return std::atan2(ry + vy * t, rx + vx * t);
}

inline int PhaseFor(float hp, float maxHp) {
    if (maxHp <= 0.0f) return 1;
    return hp <= maxHp * kPhase2Ratio ? 2 : 1;
}

// The boss bar shows each phase as its own full bar: phase 1 drains the HP
// above kPhase2Ratio, then the bar refills (over kPhaseInvuln, after a beat
// of kBarRefillHold empty) and phase 2 drains the rest.
constexpr float kBarRefillHold = 0.25f;
inline float BarShareFor(float hp, float maxHp, int phase) {
    if (maxHp <= 0.0f) return 0.0f;
    const float split = maxHp * kPhase2Ratio;
    const float share = phase >= 2 ? hp / split : (hp - split) / (maxHp - split);
    return std::clamp(share, 0.0f, 1.0f);
}
// 0..1 how far the phase 2 bar has refilled, from the invulnerable time left.
inline float BarRefillFor(float invulnLeft) {
    const float t = 1.0f - std::clamp(invulnLeft / kPhaseInvuln, 0.0f, 1.0f);
    const float k = std::clamp((t - kBarRefillHold) / (1.0f - kBarRefillHold), 0.0f, 1.0f);
    return k * k * (3.0f - 2.0f * k);
}

// Signed speed toward the player (+) or away (-) that keeps the hover band.
inline float HoverSpeed(float distance) {
    if (distance > kHoverDistance + kHoverBand) return kMoveSpeed;
    if (distance < kHoverDistance - kHoverBand) return -kMoveSpeed;
    return 0.0f;
}

// Smooth version of HoverSpeed: zero inside the band, then rising evenly (with
// an eased start) to kMoveSpeed over kHoverRamp, signed like HoverSpeed.
inline float HoverSpeedSmooth(float distance) {
    const float over = distance - (kHoverDistance + kHoverBand);
    const float under = (kHoverDistance - kHoverBand) - distance;
    const float edge = over > 0.0f ? over : under > 0.0f ? -under : 0.0f;
    const float t = std::clamp(std::fabs(edge) / kHoverRamp, 0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);
    return (edge >= 0.0f ? 1.0f : -1.0f) * eased * kMoveSpeed;
}

// The pattern after `count` earlier ones: window patterns on even turns,
// the aimed stream on odd ones (the boss is built around its panes). CloseWindow
// joins the window pool in phase 2.
inline Pattern NextPattern(int count, int phase, int roll, Pattern last = Pattern::None) {
    const int r = roll < 0 ? -roll : roll;
    if (count % 4 == 3) return Pattern::Aim;   // the stream is the exception, not every other turn
    const int windowKinds = phase >= 2 ? 7 : 5;
    const auto kind = [&](int k) {
        switch (k % windowKinds) {
        case 0:  return Pattern::Refuge;
        case 1:  return Pattern::Throw;
        case 2:  return Pattern::Ram;
        case 3:  return Pattern::Detonate;
        case 4:  return Pattern::Wall;
        case 5:  return Pattern::Warp;
        default: return Pattern::CloseWindow;
        }
    };
    // Short of panes: open more. All panes up: blow them (every other time).
    if (g_PaneCount >= 0 && last != Pattern::Reinforce && g_PaneCount <= kReinforceBelow && r % 3 != 0)
        return Pattern::Reinforce;
    if (g_PaneCount >= MaxBossPanesFor(phase) && last != Pattern::Detonate && r % 2 == 0)
        return Pattern::Detonate;
    // Never the same window pattern twice running.
    return kind(r) == last ? kind(r + 1) : kind(r);
}

} // namespace norma
