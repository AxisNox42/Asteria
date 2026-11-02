#pragma once

// ─────────────────────────────────────────────────────────────
// CAELUM, the corrupted image (Docs/Design/Caelum_Boss_Design.md). The screen (the
// desktop picture) is split into a grid of kCell px cells and CAELUM corrupts it
// cell by cell. A corrupted cell is laid over the screen as a see-through
// glitch, hurts whoever stands in it and weakens them, and heals by itself after
// a while. The cell CAELUM is in is always corrupted, so it leaves a trail.
// This file holds the numbers and the pure rules (no GL): the grid, the state
// machine, every pattern's timing and the damage checks. CaelumDraw.h turns the
// same state into the corrupted cells and the lines.
//
// Patterns:
//   1 Spread   (concept) the cursor clicks seeds (the first on the player),
//              which corrupt and spread to their neighbours generation by
//              generation
//   2 Scanline (gimmick) the cursor drag-selects a stretch of the row (or
//              column) through the player, leaving one gap to cross by
//   3 Smear    (gimmick) the cursor grabs a puddle far from the player, lifts
//              it and drags it after the player: it hurts as it goes, smears
//              corruption along its way and shrinks with every cell it
//              travels; once small the cursor lets go
//   4 Snipe    (sniper form) single very fast shots at where the player is
//              going (their movement led by the flight time); the cell each
//              lands in starts to corrupt
//   - Overflow (condition) when too much of the picture is corrupted, the file
//              breaks: the corruption swells one cell and bursts, then clears
//   - Erode    (phase 2, always on) the cell the player lingers in corrupts
//   - Wave     (always on) every few seconds, whatever else is going on, a
//              shockwave shoves the player kWavePush px after a warning (no
//              damage; the threat is where they land, shown during the warning).
//              It takes turns: from the body (a ring), then from a straight
//              line across the screen beside the player (shoved straight away
//              from the line).
//   6 Siphon   (both phases) the cursor presses the body's core and the body
//              drinks the board: every puddle stretches into a thin stream
//              that spirals into it (near ones first), hurting as it passes,
//              while the player is drawn toward the body. The board ends
//              clean. Then it spouts it back: the body swells while cells
//              round the player warn, streams unwind out of it onto them
//              and a ring wave shoves the player out over the fresh
//              puddles; after that the body stays open longer
//   - Moving   in corrupt form the body glides to a new cell every couple of
//              seconds (it holds still sniping, changing form and when open)
//   - Forms    corrupt form lays the board (1-3), sniper form punishes where
//              the player stands (4); the body changes between them
//   - Cursor   CAELUM's own pointer (drawn, never the real mouse) is the hand
//              of the corrupt form: every board pattern is something it does
//   - Combos   every board pattern ends in a line shove aimed into what it
//              laid (the scan line, the window, the spread)
//   - Bad sectors every health handle lost leaves one cell corrupted for
//              good: a deeper, stronger puddle (more damage, more weakness)
//              that runs into the others and leaks into a neighbour now and then
// Every cell warns (flickering outline) before it corrupts. The world is the
// screen (zoom is cosmetic), so everything is in screen pixels.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "NormaBoss.h"
#include "PlayerStats.h"

namespace caelum {

constexpr float kBaseHp = 24000.0f;         // before the run's HP scaling (NORMA is 30000)
constexpr float kPhase2Ratio = 0.50f;        // each phase holds half the HP, like OCTANS
constexpr float kSizeScale = 3.0f;
constexpr float kContactDps = 10.0f;
constexpr float kPhaseInvuln = 1.6f;
constexpr float kPhaseFlashSeconds = 1.2f;
constexpr float kRecoverSeconds = 0.8f, kRecoverPhase2 = 0.5f;
constexpr float kExposeSeconds = 1.1f;      // opening after every pattern
constexpr float kExposeDamage = 1.5f;
constexpr int   kHealthHandles = 8;

enum class Pattern { None = 0, Spread = 1, Scanline = 2, Smear = 3, Shot = 4, Siphon = 6 };
// Window: a = 0 when a puddle is lifted, 1 when it is dropped. Morph: a = the new form.
enum class Fx { Seed, Window, Fire, Overflow, Wave, Morph, PhaseShift, Plug, Swallow, Spout };
struct Event { Fx fx; float x = 0.0f, y = 0.0f, a = 0.0f; };
inline std::vector<Event> g_Events;

inline float g_W = 1920.0f, g_H = 1080.0f;  // the arena (the screen)
inline void SetArena(float w, float h) { if (w > 200.0f) g_W = w; if (h > 200.0f) g_H = h; }

inline float Rnd01() { return (float)(rand() % 10000) / 10000.0f; }
inline float RndRange(float a, float b) { return a + (b - a) * Rnd01(); }
inline float Smooth(float u) { u = std::clamp(u, 0.0f, 1.0f); return u * u * (3.0f - 2.0f * u); }

// ── The grid ────────────────────────────────────────────────
// Fixed-size cells, so a cell is as far to walk out of at any resolution. The
// last row / column may be cut short by the screen edge.
constexpr float kCell = 288.0f;             // 1.8x the first 160 px
constexpr int   kMaxCols = 32, kMaxRows = 20;
inline int Cols() { return std::clamp((int)std::ceil(g_W / kCell), 1, kMaxCols); }
inline int Rows() { return std::clamp((int)std::ceil(g_H / kCell), 1, kMaxRows); }
inline int CellCount() { return Cols() * Rows(); }
inline int CellAt(float x, float y) {
    const int c = std::clamp((int)(x / kCell), 0, Cols() - 1);
    const int r = std::clamp((int)(y / kCell), 0, Rows() - 1);
    return r * Cols() + c;
}
inline void CellRect(int cell, float& x, float& y, float& w, float& h) {
    x = (float)(cell % Cols()) * kCell; y = (float)(cell / Cols()) * kCell;
    w = std::min(kCell, g_W - x); h = std::min(kCell, g_H - y);
}
inline void CellCentre(int cell, float& x, float& y) {
    float w, h;
    CellRect(cell, x, y, w, h);
    x += w * 0.5f; y += h * 0.5f;
}

// A cell's life: Clean -> Warn (outline flickers) -> Corrupt (hurts) -> Heal
// (fades back, harmless) -> Clean.
enum class CellState : unsigned char { Clean, Warn, Corrupt, Heal };
constexpr float kWarn = 0.80f, kWarnP2 = 0.60f;
constexpr float kCorruptLife = 4.0f, kCorruptLifeP2 = 5.5f;   // +-1 s each
constexpr float kTrailLife = 1.5f;          // the boss's own trail (was 2.5 before it moved)
constexpr float kHeal = 1.0f;
constexpr float kCorruptDps = 20.0f;
// Weakened: while in a corrupted cell and kWeakenLinger s after leaving it.
constexpr float kWeakenLinger = 1.5f;
constexpr float kWeakMove = 0.85f, kWeakDealt = 0.70f;
struct Cell {
    CellState state = CellState::Clean;
    float t = 0.0f;          // seconds in this state
    float life = 0.0f;       // how long it stays corrupted
    float warn = kWarn;
    unsigned char glitch = 0;   // per-cell seed for the drawing
    bool  bad = false;          // a bad sector: corrupted for good
};

// ── Bad sectors ─────────────────────────────────────────────
// Each health handle lost corrupts one more cell for good (away from the
// player, not touching another), so the board shrinks as the fight goes on. A
// bad sector is a puddle like the others and runs into them, but it never
// dries up, it is stronger (kBadDps, and kBadWeak while in it), and every
// kLeakMin..kLeakMax s it leaks: a clean neighbour starts to corrupt. It is
// drawn deeper.
constexpr int   kMaxBad = 6;
constexpr float kLeakMin = 3.5f, kLeakMax = 5.5f;
constexpr float kBadDps = 40.0f;
constexpr float kBadWeak = 0.55f;          // damage dealt while in one
constexpr float kBadMove = 0.75f;          // move while in one

// ── 1 Spread ────────────────────────────────────────────────
// Seeds (the first under the player) warn and corrupt; every generation each
// cell of the last one infects one clean neighbour (a short creeping vine).
constexpr int   kSeeds = 2, kSeedsP2 = 3;
constexpr int   kGenerations = 2;           // both phases (phase 2 has more seeds)
constexpr float kGenStep = 0.55f;

// ── 2 Scanline ──────────────────────────────────────────────
// A write head runs along a stretch of kScanLen cells of the row (or column)
// through the player, warning each cell as it passes; one gap cell is spared.
// Phase 2 writes a row and a column.
constexpr float kScanRate = 12.0f;          // cells per second
constexpr int   kScanLen = 5;

// ── 3 Smear ─────────────────────────────────────────────────
// The cursor goes to a corrupted cell kSmearFar or more from the player (or a
// clean one there, which it clicks corrupt first), presses, and lifts that
// cell and up to two corrupted neighbours as one free puddle (they dry up
// where they were). It drags the puddle after the player at kSmearSpeed: the
// puddle hurts like any other, and every cell it passes over starts to corrupt
// (a smear). It shrinks by kSmearShrinkPerCell for every cell's width it
// travels; at kSmearMinScale (or after kSmearMax s) the cursor lets go and the
// rest soaks into the cell below.
constexpr float kSmearFar = 420.0f;
constexpr float kSmearSpeed = 330.0f;       // px/s, both phases
constexpr float kSmearShrinkPerCell = 0.11f;
constexpr float kSmearMinScale = 0.35f;
constexpr float kSmearMax = 6.0f;
constexpr float kSmearReach = 0.35f, kSmearGrab = 0.75f, kSmearTail = 0.50f;   // seconds in

// ── Cursor ──────────────────────────────────────────────────
// CAELUM's pointer, in both forms. It glides to where it is wanted (kCurFollow,
// 1/s) and sits exactly on a drag. Snipe: it rides the aim point and clicks
// each shot off. Spread: one click every kClickSlot (it presses kClickHold
// before). Scanline: the head waits kCurLead while the cursor gets there.
constexpr float kCurFollow = 14.0f;
constexpr float kClickSlot = 0.32f, kClickHold = 0.08f;
constexpr float kCurLead = 0.30f;
// Idle: it wanders the screen on its own, not after the player: it glides
// (kIdleFollow, 1/s) to a spot kIdleHop px away, rests there a moment, picks
// another.
constexpr float kIdleFollow = 5.5f;
constexpr float kIdleHopMin = 180.0f, kIdleHopMax = 650.0f;
constexpr float kIdleRestMin = 0.6f, kIdleRestMax = 1.6f;
constexpr int   kCurTrail = 8;

// ── 4 Snipe (sniper form) ───────────────────────────────────
// Single very fast shots at where the player will be: their movement, led by
// the flight time. Each shows its aim line kSnipeAim before it fires; the aim
// locks kSnipeLock before (a late turn dodges it). A shot hurts on touch, and
// where it lands (kShotOvershoot past the aim point) its cell starts to corrupt.
constexpr int   kSnipes = 5, kSnipesP2 = 7;
constexpr float kSnipeFirst = 0.50f;        // the first shot fires this far in
constexpr float kSnipeStep = 0.50f, kSnipeStepP2 = 0.38f;
constexpr float kSnipeAim = 0.40f, kSnipeLock = 0.12f;
constexpr float kShotSpeed = 1700.0f, kShotRadius = 12.0f;
constexpr float kShotDamage = 18.0f;
constexpr float kShotOvershoot = 160.0f;
struct Shot { float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f, left = 0.0f; bool done = false; };

// ── Forms ───────────────────────────────────────────────────
// The corrupt form lays the board (Spread, Scanline, Window); the sniper form
// punishes where the player stands (Snipe). Changing takes kMorph.
enum class Form { Corrupt, Sniper };
constexpr float kMorph = 0.60f;

// ── Overflow (condition) ────────────────────────────────────
// At kOverflowShare of the cells corrupted: kOverflowTele of warning, then the
// corruption swells one cell for a burst (once), then all of it clears.
constexpr float kOverflowShare = 0.30f;
constexpr float kOverflowTele = 1.20f, kOverflowBurst = 0.30f;
constexpr float kOverflowDamage = 30.0f, kOverflowCooldown = 10.0f;

// ── Wave (always on) ────────────────────────────────────────
// The first one kWaveFirst s into the fight, then every kWaveEvery s, taking
// turns. A ring: the source is fixed where the body was when the warning began.
// A line: a straight line across the whole screen kLineOffset px beside the
// player, turned so the shove lands on corruption if any is in reach; its front
// runs out both ways and shoves the player away from it.
constexpr float kWaveFirst = 2.0f;
constexpr float kWaveEvery = 4.0f, kWaveEveryP2 = 3.0f;
constexpr float kWaveTele = 1.20f;          // was 0.9: the user asked for a longer warning
constexpr float kWaveSpeed = 1400.0f;       // px/s of the front
constexpr float kWavePush = kCell;          // how far the player is shoved: one cell
constexpr float kLineOffset = 120.0f;
constexpr float kLineRun = 700.0f;          // how far its front runs

// ── Moving (corrupt form) ───────────────────────────────────
// Every kMoveRest s the body picks a cell whose centre is kMoveNear..kMoveFar
// from the player (and not its own) and glides there at up to kMoveSpeed.
constexpr float kMoveNear = 350.0f, kMoveFar = 650.0f;
constexpr float kMoveSpeed = 200.0f;
constexpr float kMoveRestMin = 1.5f, kMoveRestMax = 2.5f;
constexpr float kBodyMinX = 140.0f, kBodyMinY = 130.0f;   // kept this far inside the screen

// ── Erode (phase 2) ─────────────────────────────────────────
constexpr float kErodeAfter = 1.6f;         // seconds in one cell
constexpr float kErodeShow = 0.4f;          // the meter shows from here

// ── 6 Siphon ────────────────────────────────────────────────
// The cursor reaches the core and presses it (kSiphonPlug), then from
// kSiphonStart every puddle (not the bad sectors) is drawn in. A puddle's
// pull starts later the farther it is (up to kSiphonStagger); its head runs
// to the body over kSiphonHead (accelerating), its tail follows kSiphonLag
// behind over kSiphonTail, so it stretches into a stream that thins, spirals
// kSiphonSwirl round the body and is gone. Only with kSiphonMin puddles out.
constexpr float kSiphonPlug = 0.35f, kSiphonStart = 0.70f;
constexpr float kSiphonStagger = 0.40f;
constexpr float kSiphonHead = 0.80f, kSiphonLag = 0.30f, kSiphonTail = 1.00f;
constexpr float kSiphonSwirl = 1.10f;       // radians it turns on the way in
constexpr float kSiphonTailEnd = 0.30f;     // seconds after the last drop
constexpr float kSiphonExpose = 2.0f;       // open this long after (others: kExposeSeconds)
constexpr int   kSiphonMin = 3;
constexpr float kSiphonPull = 130.0f;       // px/s the player is drawn in
constexpr float kSiphonPullNear = 140.0f, kSiphonPullFull = 280.0f;   // no pull this close
constexpr int   kSiphonBalls = 28;          // per stream, at most
// ── 6b Spout (the siphon goes on into it) ───────────────────
// What it drank comes back out. On the swallow the body swells for the
// tune's spoutSwell while the landing cells warn: round the player, one or
// two cells off, more on the far side from the body, never the player's own
// cell nor more than kSpoutNear of its eight neighbours, and (if it can) not
// where a stream would run over the player getting there. Then one stream per
// cell leaves the body (kSpoutGap apart), unwinding the drinking spiral: its
// head flies out over kSpoutFly (slowing as it lands), its tail kSpoutLag
// behind; when the tail lands the cell is a puddle. A ring wave is queued as
// the streams leave, so its front shoves the player out over the fresh
// puddles. As many streams as puddles drunk, at most the tune's spoutMax.
constexpr float kSpoutSwell = 0.50f;
constexpr float kSpoutGap = 0.08f, kSpoutFly = 0.45f, kSpoutLag = 0.15f;
constexpr float kSpoutTail = 0.25f;         // after the last one lands
constexpr int   kSpoutMax = 5, kSpoutNear = 5;
// A puddle drunk (in: from sx, sy) or spouted (out: to sx, sy, started once
// it has landed).
struct Drop { float sx = 0.0f, sy = 0.0f, delay = 0.0f; unsigned char glitch = 0; int cell = -1; bool started = false, out = false; };
struct Ball { float x, y, r, w, wob, ph; };   // wob: how much its rim wobbles like a puddle's

// ── Difficulty ──────────────────────────────────────────────
// The numbers above are HARD (the fight as designed). EASY and NORMAL ease it,
// NIGHTMARE presses harder. Set when a CAELUM enters (SetDifficulty: 0 EASY,
// 1 NORMAL, 2 HARD, 3 NIGHTMARE); a fight reset keeps it.
struct Tune {
    float warn = kWarn, warnP2 = kWarnP2;
    float corruptDps = kCorruptDps, badDps = kBadDps;
    int   seeds = kSeeds, seedsP2 = kSeedsP2;
    int   snipes = kSnipes, snipesP2 = kSnipesP2;
    float shotSpeed = kShotSpeed;
    float waveEvery = kWaveEvery, waveEveryP2 = kWaveEveryP2;
    int   maxBad = kMaxBad;
    bool  leaks = true;
    float erodeAfter = kErodeAfter;         // 0: no erosion
    float overflowDamage = kOverflowDamage;
    float smearSpeed = kSmearSpeed;
    float siphonPull = kSiphonPull;
    int   spoutMax = kSpoutMax;
    float spoutSwell = kSpoutSwell;
};
inline Tune g_Tune;
inline void SetDifficulty(int d) {
    Tune t;   // HARD
    if (d <= 0) {          // EASY
        t.warn = 1.0f; t.warnP2 = 0.75f; t.corruptDps = 10.0f; t.badDps = 20.0f;
        t.seeds = 1; t.seedsP2 = 2; t.snipes = 3; t.snipesP2 = 4; t.shotSpeed = 1300.0f;
        t.waveEvery = 5.0f; t.waveEveryP2 = 4.0f; t.maxBad = 2; t.leaks = false;
        t.erodeAfter = 0.0f;
        t.overflowDamage = 15.0f; t.smearSpeed = 270.0f; t.siphonPull = 70.0f;
        t.spoutMax = 3; t.spoutSwell = 0.70f;
    } else if (d == 1) {   // NORMAL
        t.warn = 0.9f; t.warnP2 = 0.7f; t.corruptDps = 15.0f; t.badDps = 30.0f;
        t.seeds = 2; t.seedsP2 = 2; t.snipes = 4; t.snipesP2 = 5; t.shotSpeed = 1500.0f;
        t.waveEvery = 4.5f; t.waveEveryP2 = 3.5f; t.maxBad = 3;
        t.erodeAfter = 2.4f;
        t.overflowDamage = 20.0f; t.smearSpeed = 300.0f; t.siphonPull = 100.0f;
        t.spoutMax = 4; t.spoutSwell = 0.60f;
    } else if (d >= 3) {   // NIGHTMARE
        t.warn = 0.7f; t.warnP2 = 0.5f; t.corruptDps = 25.0f; t.badDps = 50.0f;
        t.seeds = 3; t.seedsP2 = 3; t.snipes = 6; t.snipesP2 = 8; t.shotSpeed = 1900.0f;
        t.waveEvery = 3.5f; t.waveEveryP2 = 2.6f;
        t.erodeAfter = 1.3f;
        t.overflowDamage = 35.0f; t.smearSpeed = 360.0f; t.siphonPull = 165.0f;
        t.spoutMax = 6; t.spoutSwell = 0.40f;
    }
    g_Tune = t;
}

// ── State ───────────────────────────────────────────────────
// A stretch [from, from + len) of a row (horiz) or column; gap is an index along the line.
struct Scan { bool horiz = true; int line = 0; int from = 0, len = 0; int dir = 1; int gap = 0; float head = 0.0f; int done = 0; };
struct State {
    bool    active = false;
    int     phase = 1;
    float   phaseFlash = 0.0f;
    float   invuln = 0.0f;
    float   exposed = 0.0f;         // > 0: open to extra damage, holding still
    Pattern pattern = Pattern::None, last = Pattern::None;
    bool    acting = false;
    float   t = 0.0f;               // seconds in the current pattern
    float   recoverT = 0.0f;
    int     count = 0;
    int     step = 0;               // pattern-local counter
    float   time = 0.0f;
    float   prevX = 0.0f, prevY = 0.0f;
    float   bossX = 0.0f, bossY = 0.0f;
    float   handleFlash = 0.0f;
    int     litHandles = kHealthHandles;

    Cell              cells[kMaxCols * kMaxRows];
    std::vector<int>  frontier;     // Spread: the last generation
    std::vector<Scan> scans;
    std::vector<Shot> shots;
    float   scanDoneAt = -1.0f;     // Scanline: when the last head finished

    float   weaken = 0.0f;          // seconds of weakness left
    bool    weakHard = false;       // in a bad sector: the stronger weakness
    float   overflowT = -1.0f;      // >= 0: overflowing
    float   overflowCd = 0.0f;
    bool    overflowHit = false;
    int     dwellCell = -1;
    float   dwell = 0.0f;

    Form    form = Form::Corrupt;
    float   formBlend = 0.0f;       // 0 corrupt .. 1 sniper, for the body
    float   morph = -1.0f;          // >= 0: changing form, then `next` starts
    Pattern next = Pattern::None;
    float   pvx = 0.0f, pvy = 0.0f; // the player's velocity (smoothed)
    float   aimX = 0.0f, aimY = 0.0f;   // Snipe: where the next shot goes
    float   aimAng = 0.0f;          // where the body points
    bool    comboQueued = false;    // a combo shove waits for the next wave slot
    float   comboNx = 0.0f, comboNy = 0.0f;   // its way (0, 0: aimed at corruption)
    bool    combo = false;          // the running wave is a combo shove
    bool    comboSent = false;      // this pattern's combo is queued already

    std::vector<int> seeds;         // Spread: cells to click after the first (on the player)
    int     clicked = 0;            // Spread: seeds clicked so far

    float   hpRatio = 1.0f;
    // The body: where it glides to, or (bodyExact) exactly where it is put
    // (dragged); bodyHold: it holds still.
    float   moveX = -1.0f, moveY = -1.0f, moveT = 0.0f;
    bool    bodyHold = true, bodyExact = false;
    float   bodyX = 0.0f, bodyY = 0.0f;
    bool    comboRing = false;      // the queued combo wave is a ring from the body

    int     smearCell = 0;          // Smear: the cell it lifts
    bool    carrying = false;       // a puddle is being dragged
    float   carryX = 0.0f, carryY = 0.0f, carryScale = 1.0f, carryBase = 1.0f;
    float   carryMoved = 0.0f;      // px it has been dragged
    float   carryEndT = 0.0f;       // when the cursor let go

    std::vector<Drop> drops;        // Siphon: the puddles being drunk
    float   swirl = 1.0f;           // Siphon: +1 / -1, which way the streams turn
    float   siphonEndAt = -1.0f;    // Siphon: when the last stream was gone
    float   spoutAt = -1.0f;        // Spout: when it swallowed (< 0: no spout)
    float   spoutEndAt = -1.0f;     // Spout: when the last stream landed
    float   pullX = 0.0f, pullY = 0.0f;   // px/s drawing the player in (the game moves them)

    float   curX = -1.0f, curY = -1.0f;   // the cursor (< 0: not placed yet)
    bool    curPress = false;
    float   clickT = 9.0f;          // seconds since the last click
    float   clickX = 0.0f, clickY = 0.0f;
    float   trail[kCurTrail][2] = {};
    float   trailTick = 0.0f;
    float   idleX = -1.0f, idleY = -1.0f;   // where the idle cursor is headed
    float   idleT = 0.0f;           // seconds before it picks the next spot
    bool    curIdle = true;         // nothing to do: wandering

    float   waveCd = kWaveFirst;    // seconds to the next warning
    float   waveT = -1.0f;          // >= 0: warning (< kWaveTele), then the front
    float   waveX = 0.0f, waveY = 0.0f;   // the ring's source / the line's middle
    bool    waveLine = false;
    float   waveNx = 0.0f, waveNy = 1.0f; // the line's normal (the shove's way)
    int     waves = 0;
    bool    waveHit = false;
    float   pushX = 0.0f, pushY = 0.0f;   // shove for the game to apply (then zeroed)
};
inline State g;

inline float WarnTime() { return g.phase == 2 ? g_Tune.warnP2 : g_Tune.warn; }
inline bool Hurts(const Cell& c) { return c.state == CellState::Corrupt; }

// ── Puddles ─────────────────────────────────────────────────
// Corruption is laid out by cell, but it lies like spilled liquid: every
// corrupted cell is a soft blob round its centre with a slowly wobbling rim,
// the blobs of neighbouring cells run together, and the puddle's edge is where
// their summed field reaches kPuddleEdge. It hurts inside that edge; the
// drawing contours the same field, so what you see is what hurts. A lone
// blob's rim sits about 0.55 of a cell from its centre (0.45..0.63 as it
// wobbles); a scanline's gap cell stays open.
constexpr float kBlobReach = 1.02f * kCell;
constexpr float kPuddleEdge = 0.5f;
inline float BlobField(int cell, float x, float y, float time) {
    float cx, cy;
    CellCentre(cell, cx, cy);
    const float dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
    if (d2 >= kBlobReach * kBlobReach * 1.36f) return 0.0f;
    const float ph = (float)g.cells[cell].glitch * 0.0246f;
    const float a = std::atan2(dy, dx);
    const float R = kBlobReach * (1.0f + 0.10f * std::sin(3.0f * a + ph + time * 0.8f)
                                       + 0.06f * std::sin(5.0f * a - ph * 1.7f - time * 1.1f));
    const float q = 1.0f - d2 / (R * R);
    return q > 0.0f ? q * q : 0.0f;
}
inline float CarryField(float x, float y, float time);
// Siphon: this tick's streams as metaballs (UpdateDropBalls), summed like the
// cells' blobs, so a stream hurts exactly where it is drawn.
inline std::vector<Ball> g_DropBalls;
inline float DropField(float x, float y) {
    float f = 0.0f;
    for (const Ball& b : g_DropBalls) {
        const float dx = x - b.x, dy = y - b.y, d2 = dx * dx + dy * dy;
        const float rMax = b.r * (1.0f + 0.16f * b.wob);
        if (d2 >= rMax * rMax) continue;
        float r = b.r;
        if (b.wob > 0.01f) {   // the same wobbling rim as a puddle's blob (BlobField)
            const float a = std::atan2(dy, dx);
            r *= 1.0f + b.wob * (0.10f * std::sin(3.0f * a + b.ph + g.time * 0.8f) +
                                 0.06f * std::sin(5.0f * a - b.ph * 1.7f - g.time * 1.1f));
        }
        const float q = 1.0f - d2 / (r * r);
        if (q > 0.0f) f += b.w * q * q;
    }
    return f;
}
// The field of the cells that hurt (corrupted ones, a dragged puddle and the
// siphon's streams) at (x, y).
inline float HurtField(float x, float y) {
    float f = 0.0f;
    for (int i = 0; i < CellCount(); ++i)
        if (Hurts(g.cells[i])) f += BlobField(i, x, y, g.time);
    return f + CarryField(x, y, g.time) + DropField(x, y);
}
inline bool InPuddle(float x, float y) { return HurtField(x, y) >= kPuddleEdge; }
// The bad sectors' own field: inside it is the stronger zone.
inline bool InBadZone(float x, float y) {
    float f = 0.0f;
    for (int i = 0; i < CellCount(); ++i)
        if (g.cells[i].bad) f += BlobField(i, x, y, g.time);
    return f >= kPuddleEdge;
}

// Starts a clean (or healing) cell's warning; corrupting ones are left alone.
inline void Infect(int i, float warn = -1.0f) {
    if (i < 0 || i >= CellCount()) return;
    Cell& c = g.cells[i];
    if (c.state == CellState::Warn || c.state == CellState::Corrupt) return;
    c.state = CellState::Warn;
    c.t = 0.0f;
    c.warn = warn >= 0.0f ? warn : WarnTime();
    c.life = (g.phase == 2 ? kCorruptLifeP2 : kCorruptLife) + RndRange(-1.0f, 1.0f);
    c.glitch = (unsigned char)(rand() % 256);
}
// Clears the corruption; bad sectors stay.
inline void ClearCells() { for (Cell& c : g.cells) if (!c.bad) c = Cell{}; }
// Corrupted cells, bad sectors not counted (they would keep the overflow going).
inline int CorruptCount() {
    int n = 0;
    for (int i = 0; i < CellCount(); ++i) if (Hurts(g.cells[i]) && !g.cells[i].bad) ++n;
    return n;
}
inline int BadCount() {
    int n = 0;
    for (int i = 0; i < CellCount(); ++i) if (g.cells[i].bad) ++n;
    return n;
}
inline int Neighbours(int i, int out[4]) {
    const int cols = Cols(), rows = Rows(), c = i % cols, r = i / cols;
    int n = 0;
    if (c > 0) out[n++] = i - 1;
    if (c < cols - 1) out[n++] = i + 1;
    if (r > 0) out[n++] = i - cols;
    if (r < rows - 1) out[n++] = i + cols;
    return n;
}
// True for a clean cell next to a corrupted one (the overflow's reach).
inline bool OverflowReach(int i) {
    if (Hurts(g.cells[i])) return true;
    int nb[4];
    const int n = Neighbours(i, nb);
    for (int k = 0; k < n; ++k) if (Hurts(g.cells[nb[k]])) return true;
    return false;
}

inline void ClearHazards() {
    ClearCells();
    g.frontier.clear(); g.scans.clear(); g.shots.clear();
    g.overflowT = -1.0f; g.dwell = 0.0f;
    g.waveT = -1.0f; g.waveCd = g_Tune.waveEvery; g.comboQueued = false;
    g.carrying = false;
    g.drops.clear(); g_DropBalls.clear(); g.pullX = g.pullY = 0.0f;
    g.morph = -1.0f;
}

// One more bad sector: a clean cell two or more cells from the player, not the
// boss's, and (if it can) not touching another bad sector.
inline void AddBadSector(float px, float py) {
    if (BadCount() >= g_Tune.maxBad) return;
    const int here = CellAt(px, py), boss = CellAt(g.bossX, g.bossY), cols = Cols();
    for (int pass = 0; pass < 2; ++pass) {
        for (int tries = 0; tries < 80; ++tries) {
            const int i = rand() % CellCount();
            const Cell& c = g.cells[i];
            if (c.bad || i == boss) continue;
            if (std::max(std::abs(i % cols - here % cols), std::abs(i / cols - here / cols)) < 2) continue;
            if (pass == 0) {
                int nb[4];
                const int n = Neighbours(i, nb);
                bool touches = false;
                for (int k = 0; k < n; ++k) touches = touches || g.cells[nb[k]].bad;
                if (touches) continue;
            }
            Cell& d = g.cells[i];
            d.state = CellState::Corrupt; d.t = 0.0f; d.bad = true;
            d.life = RndRange(kLeakMin, kLeakMax);   // next leak
            d.glitch = (unsigned char)(rand() % 256);
            float x, y;
            CellCentre(i, x, y);
            g_Events.push_back({ Fx::Seed, x, y, 0.0f });
            return;
        }
    }
}
inline void Reset() { g = State{}; g_DropBalls.clear(); }

// Player's movement and damage to CAELUM while weakened.
inline float MoveFactor() { return g.active && g.weaken > 0.0f ? (g.weakHard ? kBadMove : kWeakMove) : 1.0f; }
inline float DealtFactor() { return g.active && g.weaken > 0.0f ? (g.weakHard ? kBadWeak : kWeakDealt) : 1.0f; }

// What last hurt the player, so the screen can say why (1 corruption, 2 overflow,
// 3 shot).
struct HitInfo { int src = 0; float age = 99.0f; float dmg = 0.0f; float x = 0.0f, y = 0.0f; };
inline HitInfo g_LastHit;
inline void Hit(float& playerHP, float dmg, int src, float x, float y) {
    HurtPlayer(playerHP, dmg);
    if (g_LastHit.src == src && g_LastHit.age < 0.3f) g_LastHit.dmg += dmg; else g_LastHit.dmg = dmg;
    g_LastHit.src = src; g_LastHit.age = 0.0f; g_LastHit.x = x; g_LastHit.y = y;
}

inline int PhaseFor(float hp, float maxHp) {
    return (maxHp > 0.0f && hp <= maxHp * kPhase2Ratio + 0.01f) ? 2 : 1;   // the phase-1 floor lands on the line
}

// ── Cells, overflow, erosion, damage ────────────────────────
inline void TickCells(float dt, float px, float py, float& playerHP) {
    for (int i = 0; i < CellCount(); ++i) {
        Cell& c = g.cells[i];
        c.t += dt;
        switch (c.state) {
        case CellState::Warn:    if (c.t >= c.warn) { c.state = CellState::Corrupt; c.t = 0.0f; } break;
        case CellState::Corrupt:
            if (!c.bad && c.t >= c.life) { c.state = CellState::Heal; c.t = 0.0f; }
            else if (c.bad && g_Tune.leaks && c.t >= c.life) {   // a leak into a clean neighbour
                int nb[4];
                const int n = Neighbours(i, nb);
                const int start = n > 0 ? rand() % n : 0;
                for (int k = 0; k < n; ++k) {
                    const int j = nb[(start + k) % n];
                    if (g.cells[j].state == CellState::Clean) { Infect(j); break; }
                }
                c.t = 0.0f;
                c.life = RndRange(kLeakMin, kLeakMax);
            }
            break;
        case CellState::Heal:    if (c.t >= kHeal) c = Cell{}; break;
        default: break;
        }
    }
    const int here = CellAt(px, py);
    // Standing in a puddle: steady damage and weakness.
    g.weaken = std::max(0.0f, g.weaken - dt);
    if (InPuddle(px, py)) {
        const bool bad = InBadZone(px, py);
        g.weaken = kWeakenLinger;
        g.weakHard = bad;
        Hit(playerHP, (bad ? g_Tune.badDps : g_Tune.corruptDps) * dt, 1, px, py);
    }
    // Overflow.
    g.overflowCd = std::max(0.0f, g.overflowCd - dt);
    if (g.overflowT < 0.0f && g.overflowCd <= 0.0f &&
        (float)CorruptCount() >= kOverflowShare * (float)CellCount()) {
        g.overflowT = 0.0f;
        g.overflowHit = false;
    }
    if (g.overflowT >= 0.0f) {
        const float before = g.overflowT;
        g.overflowT += dt;
        if (g.overflowT >= kOverflowTele && before < kOverflowTele)
            g_Events.push_back({ Fx::Overflow, px, py, 0.0f });
        if (g.overflowT >= kOverflowTele && !g.overflowHit && OverflowReach(here)) {
            Hit(playerHP, g_Tune.overflowDamage, 2, px, py);
            g.overflowHit = true;
        }
        if (g.overflowT >= kOverflowTele + kOverflowBurst) {
            for (int i = 0; i < CellCount(); ++i) {
                Cell& c = g.cells[i];
                if (c.bad) continue;
                if (c.state == CellState::Warn || c.state == CellState::Corrupt) { c.state = CellState::Heal; c.t = 0.0f; }
            }
            g.overflowT = -1.0f;
            g.overflowCd = kOverflowCooldown;
        }
    }
    // Erosion (phase 2): the cell the player lingers in.
    if (g.phase == 2 && g_Tune.erodeAfter > 0.0f) {
        if (here != g.dwellCell) { g.dwellCell = here; g.dwell = 0.0f; }
        if (g.cells[here].state == CellState::Clean) g.dwell += dt; else g.dwell = 0.0f;
        if (g.dwell >= g_Tune.erodeAfter) { Infect(here); g.dwell = 0.0f; }
    } else {
        g.dwellCell = -1; g.dwell = 0.0f;
    }
}

// ── Patterns ────────────────────────────────────────────────
inline void EndPattern() {
    g.acting = false;
    g.recoverT = 0.0f;
    g.exposed = kExposeSeconds;
    g.last = g.pattern;
    g.pattern = Pattern::None;
}

inline Form FormFor(Pattern p) { return p == Pattern::Shot ? Form::Sniper : Form::Corrupt; }

// Two board patterns (corrupt form), then a snipe (sniper form), and again.
inline Pattern PickPattern() {
    // The siphon comes after the board has filled (it needs kSiphonMin
    // puddles, else the next one runs).
    static const Pattern p1[] = { Pattern::Spread, Pattern::Scanline, Pattern::Shot,
                                  Pattern::Smear, Pattern::Siphon, Pattern::Shot,
                                  Pattern::Spread, Pattern::Scanline, Pattern::Shot,
                                  Pattern::Smear, Pattern::Siphon, Pattern::Shot };
    // Phase 2: the same patterns in another order.
    static const Pattern p2[] = { Pattern::Spread, Pattern::Scanline, Pattern::Shot,
                                  Pattern::Smear, Pattern::Siphon, Pattern::Shot,
                                  Pattern::Smear, Pattern::Scanline, Pattern::Shot,
                                  Pattern::Spread, Pattern::Siphon, Pattern::Shot };
    const Pattern* list = g.phase == 2 ? p2 : p1;
    const int n = g.phase == 2 ? (int)(sizeof(p2) / sizeof(p2[0])) : (int)(sizeof(p1) / sizeof(p1[0]));
    Pattern pick = list[g.count % n];
    if (pick == g.last || (pick == Pattern::Siphon && CorruptCount() < kSiphonMin))
        pick = list[(g.count + 1) % n];
    return pick;
}

// Snipe: seconds into the pattern when shot k fires.
inline float SnipeFireAt(int k) { return kSnipeFirst + (g.phase == 2 ? kSnipeStepP2 : kSnipeStep) * (float)k; }
// Spread: the seeds after the first are picked now; the first is wherever the
// player is when the cursor clicks.
inline int SeedCount() { return g.phase == 2 ? g_Tune.seedsP2 : g_Tune.seeds; }
inline void StartSpread(float px, float py) {
    g.frontier.clear();
    g.seeds.clear();
    g.clicked = 0;
    const int here = CellAt(px, py);
    for (int tries = 0; tries < 60 && (int)g.seeds.size() < SeedCount() - 1; ++tries) {
        const int i = rand() % CellCount();
        if (i == here || g.cells[i].state != CellState::Clean) continue;
        if (std::find(g.seeds.begin(), g.seeds.end(), i) != g.seeds.end()) continue;
        g.seeds.push_back(i);
    }
}
// Where the cursor clicks seed k.
// Seeds the cursor clicks: the player's cell, then the picked ones.
inline int SeedTotal() { return 1 + (int)g.seeds.size(); }
inline void SeedSpot(int k, float px, float py, float& x, float& y) {
    if (k == 0 || k - 1 >= (int)g.seeds.size()) { x = px; y = py; return; }
    CellCentre(g.seeds[k - 1], x, y);
}
inline void Click(float x, float y) { g.clickT = 0.0f; g.clickX = x; g.clickY = y; }
inline void StepSpread() {
    std::vector<int> next;
    for (int i : g.frontier) {
        int nb[4];
        int n = Neighbours(i, nb);
        for (int k = 0; k < 1 && n > 0; ++k) {
            const int pick = rand() % n;
            const int j = nb[pick];
            nb[pick] = nb[--n];
            if (g.cells[j].state == CellState::Clean || g.cells[j].state == CellState::Heal) {
                Infect(j);
                next.push_back(j);
            }
        }
    }
    g.frontier.swap(next);
}

inline void AddScan(bool horiz, float px, float py) {
    Scan s;
    s.horiz = horiz;
    s.head = -kScanRate * kCurLead;   // the cursor gets to the start first
    s.line = horiz ? CellAt(px, py) / Cols() : CellAt(px, py) % Cols();
    s.dir = rand() % 2 == 0 ? 1 : -1;
    const int full = horiz ? Cols() : Rows();
    const int playerAt = horiz ? CellAt(px, py) % Cols() : CellAt(px, py) / Cols();
    s.len = std::min(kScanLen, full);
    s.from = std::clamp(playerAt - s.len / 2, 0, full - s.len);
    const int len = s.len;
    // The gap: not under the player, so crossing means walking to it, and not
    // where the boss's body would hide its marker.
    const int boss = CellAt(g.bossX, g.bossY);
    const int bossAt = horiz ? boss % Cols() : boss / Cols();
    const bool bossOnLine = std::abs((horiz ? boss / Cols() : boss % Cols()) - s.line) <= 1;
    for (int tries = 0; tries < 40; ++tries) {
        s.gap = s.from + rand() % len;
        if (len > 2 && std::abs(s.gap - playerAt) < 2) continue;
        if (bossOnLine && std::abs(s.gap - bossAt) <= 1) continue;
        break;
    }
    g.scans.push_back(s);
}
inline bool TickScans(float dt) {
    bool running = false;
    for (Scan& s : g.scans) {
        const int len = s.len;
        s.head += kScanRate * dt;
        while (s.done < len && (float)s.done < s.head) {
            const int k = s.from + (s.dir > 0 ? s.done : len - 1 - s.done);
            if (k != s.gap) Infect(s.horiz ? s.line * Cols() + k : k * Cols() + s.line);
            ++s.done;
        }
        if (s.done < len) running = true;
    }
    return running;
}

// Smear: a corrupted cell far from the player to lift (a clean one there if
// none is corrupted).
inline void StartSmear(float px, float py) {
    g.carrying = false;
    g.carryMoved = 0.0f;
    int best = -1;
    float bestD = -1.0f;
    for (int pass = 0; pass < 2 && best < 0; ++pass) {
        for (int i = 0; i < CellCount(); ++i) {
            const Cell& c = g.cells[i];
            if (c.bad || (pass == 0 ? !Hurts(c) : c.state != CellState::Clean)) continue;
            float x, y;
            CellCentre(i, x, y);
            const float d = std::hypot(x - px, y - py);
            if (d < kSmearFar) continue;
            const float score = d + RndRange(0.0f, kCell);   // some variety
            if (score > bestD) { bestD = score; best = i; }
        }
    }
    g.smearCell = best >= 0 ? best : rand() % CellCount();
}
// The free puddle's field: a wobbling blob round the cursor, sloshing faster
// than the still ones.
inline float CarryField(float x, float y, float time) {
    if (!g.carrying) return 0.0f;
    const float dx = x - g.carryX, dy = y - g.carryY, d2 = dx * dx + dy * dy;
    const float R0 = kBlobReach * g.carryScale;
    if (d2 >= R0 * R0 * 1.45f) return 0.0f;
    const float a = std::atan2(dy, dx);
    const float R = R0 * (1.0f + 0.12f * std::sin(3.0f * a + time * 2.4f) + 0.07f * std::sin(5.0f * a - time * 1.9f));
    const float q = 1.0f - d2 / (R * R);
    return q > 0.0f ? 1.1f * q * q : 0.0f;
}
inline void TickSmear(float dt, float px, float py) {
    const float t = g.t;
    if (t >= kSmearGrab && g.step == 0) {               // lift it
        g.step = 1;
        Cell& c = g.cells[g.smearCell];
        if (!Hurts(c)) { c.state = CellState::Corrupt; c.t = 0.0f; c.life = 1.0f; }   // clicked corrupt
        int lifted = 0;
        int nb[4];
        const int n = Neighbours(g.smearCell, nb);
        for (int k = 0; k < n && lifted < 2; ++k) {
            Cell& o = g.cells[nb[k]];
            if (Hurts(o) && !o.bad) { o.state = CellState::Heal; o.t = 0.0f; ++lifted; }
        }
        c.state = CellState::Heal; c.t = 0.0f;
        CellCentre(g.smearCell, g.carryX, g.carryY);
        g.carryBase = g.carryScale = 1.0f + 0.2f * (float)lifted;
        g.carrying = true;
        g_Events.push_back({ Fx::Window, g.carryX, g.carryY, 0.0f });
    }
    if (g.step == 1) {                                 // dragged after the player
        const float dx = px - g.carryX, dy = py - g.carryY, d = std::hypot(dx, dy);
        const float step = std::min(d, g_Tune.smearSpeed * dt);
        if (d > 1.0f) { g.carryX += dx / d * step; g.carryY += dy / d * step; }
        g.carryMoved += step;
        g.carryScale = std::max(kSmearMinScale, g.carryBase - kSmearShrinkPerCell * g.carryMoved / kCell);
        // The smear: cells whose centre the puddle's rim reaches start to corrupt.
        const float rim = kBlobReach * 0.54f * g.carryScale;
        for (int i = 0; i < CellCount(); ++i) {
            float x, y;
            CellCentre(i, x, y);
            if (std::hypot(x - g.carryX, y - g.carryY) < rim + kCell * 0.25f) Infect(i);
        }
        if (g.carryScale <= kSmearMinScale || t >= kSmearGrab + kSmearMax) {   // let go
            g.step = 2;
            g.carrying = false;
            Infect(CellAt(g.carryX, g.carryY), 0.3f);
            Click(g.carryX, g.carryY);
            g_Events.push_back({ Fx::Window, g.carryX, g.carryY, 1.0f });
            g.carryEndT = t;
        }
    }
}

// Siphon: the puddles to drink (all but the bad sectors and the body's own),
// the near ones first.
inline void StartSiphon() {
    g.drops.clear();
    g.siphonEndAt = g.spoutAt = g.spoutEndAt = -1.0f;
    g.swirl = rand() % 2 ? 1.0f : -1.0f;
    const int boss = CellAt(g.bossX, g.bossY);
    float maxD = 1.0f;
    for (int i = 0; i < CellCount(); ++i) {
        const Cell& c = g.cells[i];
        if (!Hurts(c) || c.bad || i == boss) continue;
        Drop d;
        CellCentre(i, d.sx, d.sy);
        d.cell = i; d.glitch = c.glitch;
        d.delay = std::hypot(d.sx - g.bossX, d.sy - g.bossY);   // distance for now
        maxD = std::max(maxD, d.delay);
        g.drops.push_back(d);
    }
    for (Drop& d : g.drops) d.delay = kSiphonStagger * d.delay / maxD + RndRange(0.0f, 0.06f);
}
// How far along its way in a drop's head and tail are (0 at its puddle, 1 in
// the body). Both accelerate, like water finding the drain.
inline void DropSpan(const Drop& d, float& head, float& tail) {
    const float tau = g.t - kSiphonStart - d.delay;
    const float h = std::clamp(tau / kSiphonHead, 0.0f, 1.0f);
    const float l = std::clamp((tau - kSiphonLag) / kSiphonTail, 0.0f, 1.0f);
    head = h * h; tail = l * l;
}
// Spout: how far out a stream's head and tail are (0 in the body, 1 landed),
// slowing as they land.
inline void SpoutSpan(const Drop& d, float& head, float& tail) {
    const float tau = g.t - g.spoutAt - d.delay;
    const float h = std::clamp(tau / kSpoutFly, 0.0f, 1.0f);
    const float l = std::clamp((tau - kSpoutLag) / kSpoutFly, 0.0f, 1.0f);
    head = 1.0f - (1.0f - h) * (1.0f - h); tail = 1.0f - (1.0f - l) * (1.0f - l);
}
// A point on a drop's way in: straight at the body, turning kSiphonSwirl as
// it closes in (a whirlpool).
inline void DropPath(const Drop& d, float s, float& x, float& y) {
    const float rx = d.sx - g.bossX, ry = d.sy - g.bossY;
    const float rho = std::hypot(rx, ry) * (1.0f - s);
    const float th = std::atan2(ry, rx) + g.swirl * kSiphonSwirl * s * s;
    x = g.bossX + std::cos(th) * rho; y = g.bossY + std::sin(th) * rho;
}
// The streams as metaballs, from each drop's tail (its fat body) to its head
// (thin, reaching the body). The longer a stream, the thinner (its water is
// spread out); nearer the body, the narrower; and it carries less as its tail
// drains. A drop that has only just started is one ball the size of its
// puddle's blob, so the puddle turns into the stream without a jump.
// A spouted stream is the same shape run backwards along the same path: its
// fat end is its head, landing as one ball the size of the puddle it becomes.
inline void UpdateDropBalls(float time) {
    g_DropBalls.clear();
    if (!g.acting || g.pattern != Pattern::Siphon) return;
    for (const Drop& d : g.drops) {
        float sh, st;   // the stream lies from st (its far, fat end) to sh (the body's side)
        if (d.out) {
            float head, tail;
            SpoutSpan(d, head, tail);
            if (head <= 0.0f || tail >= 1.0f) continue;
            st = 1.0f - head; sh = 1.0f - tail;
        } else {
            if (!d.started) continue;
            DropSpan(d, sh, st);
            if (st >= 1.0f) continue;
        }
        const float rho0 = std::max(1.0f, std::hypot(d.sx - g.bossX, d.sy - g.bossY));
        const float stretch = (sh - st) * rho0 / kCell;
        const float thin = 1.0f / std::sqrt(1.0f + 1.2f * stretch);
        const float w = 1.0f - 0.55f * std::clamp(stretch / 0.6f, 0.0f, 1.0f);
        // Still a puddle at first: its rim wobbles as it did, calming as it stretches.
        const float wob = 1.0f - std::clamp(stretch / 0.5f, 0.0f, 1.0f);
        const float ph = (float)d.glitch * 0.0246f;
        const auto radius = [&](float s) {
            const float f = sh > st ? (s - st) / (sh - st) : 0.0f;     // 0 tail .. 1 head
            const float ripple = 1.0f + 0.07f * (1.0f - wob) * std::sin(time * 7.0f + s * 9.0f + (float)d.glitch * 0.05f);
            return kBlobReach * thin * (0.25f + 0.75f * std::pow(1.0f - s, 0.7f)) * (1.0f - 0.55f * f) * ripple;
        };
        const float dsMin = (sh - st) / (float)(kSiphonBalls - 1);
        float s = st;
        for (int k = 0; k < kSiphonBalls; ++k) {
            const bool last = s >= sh || k == kSiphonBalls - 1;
            const float at = last ? sh : s;
            float x, y;
            DropPath(d, at, x, y);
            const float r = radius(at);
            g_DropBalls.push_back({ x, y, r, w, k == 0 ? wob : 0.0f, ph });
            if (last) break;
            s = std::min(sh, s + std::max(dsMin, 0.45f * r / rho0));
        }
    }
}
inline void TickSiphon(float px, float py) {
    if (g.step >= 3) return;                             // spouting it back
    const float t = g.t;
    if (g.step == 0 && t >= kSiphonPlug) {               // the cursor presses the core
        g.step = 1;
        Click(g.bossX, g.bossY);
        g_Events.push_back({ Fx::Plug, g.bossX, g.bossY, 0.0f });
    }
    if (t < kSiphonStart) return;
    if (g.step == 1) {                                   // it drinks: what was about to land goes too
        g.step = 2;
        for (int i = 0; i < CellCount(); ++i) {
            Cell& c = g.cells[i];
            if (c.state == CellState::Warn && !c.bad) c = Cell{};
        }
    }
    bool left = false;
    for (Drop& d : g.drops) {
        if (!d.started && t >= kSiphonStart + d.delay) {   // the puddle lets go of its cell
            d.started = true;
            Cell& c = g.cells[d.cell];
            if (!c.bad) c = Cell{};
        }
        float sh, st;
        DropSpan(d, sh, st);
        if (st < 1.0f) left = true;
    }
    if (!left && g.siphonEndAt < 0.0f) {
        g.siphonEndAt = t;
        g_Events.push_back({ Fx::Swallow, g.bossX, g.bossY, (float)g.drops.size() });
    }
    // The pull: the player is drawn toward the body while it drinks (not
    // right up against it).
    if (g.siphonEndAt < 0.0f) {
        const float dx = g.bossX - px, dy = g.bossY - py, d = std::hypot(dx, dy);
        if (d > 1.0f) {
            const float k = Smooth((t - kSiphonStart) / 0.3f) *
                            Smooth((d - kSiphonPullNear) / (kSiphonPullFull - kSiphonPullNear));
            g.pullX = dx / d * g_Tune.siphonPull * k;
            g.pullY = dy / d * g_Tune.siphonPull * k;
        }
    }
}

// Spout: the cells it will land on, and a stream for each (the drunk ones go).
inline void StartSpout(float px, float py) {
    const int n = std::min((int)g.drops.size(), g_Tune.spoutMax);
    g.drops.clear();
    g.spoutAt = g.t; g.spoutEndAt = -1.0f;
    const int cols = Cols(), rows = Rows();
    const int here = CellAt(px, py), boss = CellAt(g.bossX, g.bossY);
    const int hx = here % cols, hy = here / cols;
    float ax = px - g.bossX, ay = py - g.bossY;          // away from the body
    const float al = std::hypot(ax, ay);
    if (al > 1.0f) { ax /= al; ay /= al; } else { ax = 0.0f; ay = 1.0f; }
    struct Spot { int cell; float score; bool adj; };
    std::vector<Spot> spots;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            const int x = hx + dx, y = hy + dy;
            if ((dx == 0 && dy == 0) || x < 0 || y < 0 || x >= cols || y >= rows) continue;
            const int i = y * cols + x;
            const Cell& c = g.cells[i];
            if (i == boss || c.bad || c.state != CellState::Clean) continue;
            float cx, cy;
            CellCentre(i, cx, cy);
            const float d = std::max(1.0f, std::hypot(cx - px, cy - py));
            const bool adj = std::abs(dx) <= 1 && std::abs(dy) <= 1;
            // Not one whose stream would run over the player on its way.
            Drop way; way.sx = cx; way.sy = cy;
            float clear = 1.0e9f;
            for (int k = 1; k < 10; ++k) {
                float wx, wy;
                DropPath(way, (float)k / 10.0f, wx, wy);
                clear = std::min(clear, std::hypot(wx - px, wy - py));
            }
            spots.push_back({ i, ((cx - px) * ax + (cy - py) * ay) / d + RndRange(0.0f, 1.2f) - (adj ? 0.0f : 0.3f) -
                                 (clear < kCell * 0.75f ? 3.0f : 0.0f), adj });
        }
    std::sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) { return a.score > b.score; });
    int taken = 0, nearTaken = 0;
    for (const Spot& s : spots) {
        if (taken >= n) break;
        if (s.adj && nearTaken >= kSpoutNear) continue;
        nearTaken += s.adj ? 1 : 0;
        Drop d;
        CellCentre(s.cell, d.sx, d.sy);
        d.cell = s.cell; d.out = true;
        d.delay = g_Tune.spoutSwell + kSpoutGap * (float)taken + RndRange(0.0f, 0.03f);
        Infect(s.cell, d.delay + kSpoutLag + kSpoutFly);   // it corrupts as the stream lands
        d.glitch = g.cells[s.cell].glitch;
        g.drops.push_back(d);
        ++taken;
    }
}
inline void QueueRing();
inline void TickSpout() {
    if (g.step == 3 && g.t - g.spoutAt >= g_Tune.spoutSwell) {   // it lets go: out they come
        g.step = 4;
        Click(g.bossX, g.bossY);
        g_Events.push_back({ Fx::Spout, g.bossX, g.bossY, (float)g.drops.size() });
        QueueRing();                                     // its front comes as they land
    }
    bool flying = false;
    for (Drop& d : g.drops) {
        float head, tail;
        SpoutSpan(d, head, tail);
        if (tail < 1.0f) { flying = true; continue; }
        if (d.started) continue;
        d.started = true;                                // landed: the cell is a puddle now
        Cell& c = g.cells[d.cell];
        if (!c.bad) {
            if (c.state != CellState::Corrupt) { c.state = CellState::Corrupt; c.t = 0.0f; }
            c.glitch = d.glitch;
        }
        g_Events.push_back({ Fx::Seed, d.sx, d.sy, 0.0f });
    }
    if (!flying && g.step == 4 && g.spoutEndAt < 0.0f) g.spoutEndAt = g.t;
}

// ── Cursor ──────────────────────────────────────────────────
// Where the cursor wants to be this frame, whether it presses, and whether it
// sits exactly there (a drag) rather than gliding.
inline void CursorWant(float px, float py, float& tx, float& ty, bool& press, bool& exact) {
    press = false; exact = false;
    // Idle: wherever it is wandering to.
    tx = g.idleX; ty = g.idleY;
    if (!g.acting) return;
    switch (g.pattern) {
    case Pattern::Spread:
        if (g.clicked < SeedTotal()) {
            SeedSpot(g.clicked, px, py, tx, ty);
            press = g.t >= kClickSlot * (float)(g.clicked + 1) - kClickHold;
        }
        break;
    case Pattern::Scanline:
        if (!g.scans.empty() && g.scans[0].done < g.scans[0].len) {
            const Scan& s = g.scans[0];
            const float k = std::clamp(s.head, 0.0f, (float)s.len);
            const float along = ((float)s.from + (s.dir > 0 ? k : (float)s.len - k)) * kCell;
            const float across = ((float)s.line + 0.5f) * kCell;
            tx = s.horiz ? along : across; ty = s.horiz ? across : along;
            press = s.head >= 0.0f;
            exact = press;
        }
        break;
    case Pattern::Shot:
        if (g.step < (g.phase == 2 ? g_Tune.snipesP2 : g_Tune.snipes) && g.t >= SnipeFireAt(g.step) - kSnipeAim) {
            tx = g.aimX; ty = g.aimY;
            press = g.t >= SnipeFireAt(g.step) - kSnipeLock;
        }
        break;
    case Pattern::Siphon:                                // it holds the core down while it drinks (and swells)
        if (g.siphonEndAt < 0.0f || g.step == 3) {
            tx = g.bossX; ty = g.bossY;
            press = g.t >= kSiphonPlug - kClickHold;
            exact = g.t >= kSiphonPlug;
        }
        break;
    case Pattern::Smear:
        if (g.step == 0) {
            CellCentre(g.smearCell, tx, ty);
            press = g.t >= kSmearReach;
        } else if (g.step == 1) { tx = g.carryX; ty = g.carryY; press = exact = true; }
        break;
    default: break;
    }
}
// The next spot the idle cursor wanders to: a hop from where it is, on screen.
inline void PickIdleSpot() {
    const float fx = g.curX < 0.0f ? g_W * 0.5f : g.curX, fy = g.curY < 0.0f ? g_H * 0.5f : g.curY;
    const float a = RndRange(0.0f, 6.2831853f), d = RndRange(kIdleHopMin, kIdleHopMax);
    g.idleX = std::clamp(fx + std::cos(a) * d, 80.0f, std::max(80.0f, g_W - 80.0f));
    g.idleY = std::clamp(fy + std::sin(a) * d, 80.0f, std::max(80.0f, g_H - 80.0f));
    g.idleT = RndRange(kIdleRestMin, kIdleRestMax);
}
inline void TickCursor(float dt, float px, float py) {
    if (g.idleX < 0.0f) PickIdleSpot();
    if (g.curIdle) { g.idleT -= dt; if (g.idleT <= 0.0f) PickIdleSpot(); }
    float tx, ty;
    bool press, exact;
    CursorWant(px, py, tx, ty, press, exact);
    const bool wasIdle = g.curIdle;
    g.curIdle = tx == g.idleX && ty == g.idleY && !press;
    // Done with a job: it rests where it finished and wanders on from there,
    // instead of going back to the spot it had before.
    if (g.curIdle && !wasIdle && g.curX >= 0.0f) {
        g.idleX = tx = g.curX; g.idleY = ty = g.curY;
        g.idleT = RndRange(kIdleRestMin, kIdleRestMax);
    }
    if (g.curX < 0.0f || exact) { g.curX = tx; g.curY = ty; }
    else {
        const float k = std::min(1.0f, (g.curIdle ? kIdleFollow : kCurFollow) * dt);
        g.curX += (tx - g.curX) * k; g.curY += (ty - g.curY) * k;
    }
    g.curPress = press;
    g.clickT += dt;
    g.trailTick -= dt;
    if (g.trailTick <= 0.0f) {
        g.trailTick = 0.025f;
        for (int k = kCurTrail - 1; k > 0; --k) { g.trail[k][0] = g.trail[k - 1][0]; g.trail[k][1] = g.trail[k - 1][1]; }
        g.trail[0][0] = g.curX; g.trail[0][1] = g.curY;
    }
}

// Moving: the next cell to glide to (kept as is when none fits).
inline void PickMoveSpot(float px, float py) {
    g.moveT = RndRange(kMoveRestMin, kMoveRestMax);
    for (int tries = 0; tries < 40; ++tries) {
        float x, y;
        CellCentre(rand() % CellCount(), x, y);
        x = std::clamp(x, kBodyMinX, g_W - kBodyMinX);
        y = std::clamp(y, kBodyMinY, g_H - kBodyMinY);
        const float d = std::hypot(x - px, y - py);
        if (d < kMoveNear || d > kMoveFar) continue;
        if (std::hypot(x - g.bossX, y - g.bossY) < kCell * 0.8f) continue;
        g.moveX = x; g.moveY = y;
        return;
    }
    if (g.moveX < 0.0f) { g.moveX = g.bossX; g.moveY = g.bossY; }
}
inline void TickBody(float dt, float px, float py) {
    g.bodyExact = false;
    g.bodyHold = g.exposed > 0.0f || g.invuln > 0.0f || g.form == Form::Sniper || g.morph >= 0.0f ||
                 (g.acting && g.pattern == Pattern::Siphon);
    g.moveT -= dt;
    // The player came close (a shove, a dash): move off at once.
    if (!g.bodyHold && std::hypot(g.bossX - px, g.bossY - py) < kMoveNear * 0.6f &&
        std::hypot(g.moveX - px, g.moveY - py) < kMoveNear) g.moveT = 0.0f;
    if (g.moveX < 0.0f || g.moveT <= 0.0f) PickMoveSpot(px, py);
}
inline void StartPattern(float px, float py, Pattern force = Pattern::None) {
    g.pattern = force != Pattern::None ? force : PickPattern();
    ++g.count;
    g.acting = true;
    g.t = 0.0f;
    g.step = 0;
    g.comboSent = false;
    g.form = FormFor(g.pattern);
    switch (g.pattern) {
    case Pattern::Spread: StartSpread(px, py); break;
    case Pattern::Scanline:
        g.scans.clear();
        g.scanDoneAt = -1.0f;
        AddScan(rand() % 2 == 0, px, py);
        if (g.phase == 2) AddScan(!g.scans[0].horiz, px, py);
        break;
    case Pattern::Smear: StartSmear(px, py); break;
    case Pattern::Siphon: StartSiphon(); break;
    default: break;
    }
}

// Where a shot from (bx, by) meets the player if they keep moving as they are.
inline void LeadAim(float bx, float by, float px, float py, float& ax, float& ay) {
    ax = px; ay = py;
    for (int k = 0; k < 2; ++k) {
        const float t = std::hypot(ax - bx, ay - by) / g_Tune.shotSpeed;
        ax = px + g.pvx * t; ay = py + g.pvy * t;
    }
    ax = std::clamp(ax, 0.0f, g_W); ay = std::clamp(ay, 0.0f, g_H);
}
inline void FireSnipe(float bx, float by) {
    const float dx = g.aimX - bx, dy = g.aimY - by, d = std::max(1.0f, std::hypot(dx, dy));
    Shot s;
    s.x = bx; s.y = by;
    s.vx = dx / d * g_Tune.shotSpeed; s.vy = dy / d * g_Tune.shotSpeed;
    s.left = d + kShotOvershoot;
    g.shots.push_back(s);
    g_Events.push_back({ Fx::Fire, bx, by, std::atan2(dy, dx) });
}
inline void TickShots(float dt, float px, float py, float& playerHP) {
    for (Shot& s : g.shots) {
        const float ox = s.x, oy = s.y;
        s.x += s.vx * dt; s.y += s.vy * dt;
        s.left -= g_Tune.shotSpeed * dt;
        // Fast: test the whole step, not just where it ended.
        const float sx = s.x - ox, sy = s.y - oy, sl = sx * sx + sy * sy;
        const float u = sl > 0.0f ? std::clamp(((px - ox) * sx + (py - oy) * sy) / sl, 0.0f, 1.0f) : 0.0f;
        const float dx = px - (ox + sx * u), dy = py - (oy + sy * u), r = kShotRadius + 16.0f;
        if (dx * dx + dy * dy < r * r) { Hit(playerHP, kShotDamage, 3, px, py); s.done = true; }
        else if (s.left <= 0.0f) {
            if (s.x >= 0.0f && s.y >= 0.0f && s.x < g_W && s.y < g_H) Infect(CellAt(s.x, s.y));
            s.done = true;
        }
    }
    g.shots.erase(std::remove_if(g.shots.begin(), g.shots.end(), [](const Shot& s) {
        return s.done || s.x < -40.0f || s.y < -40.0f || s.x > g_W + 40.0f || s.y > g_H + 40.0f;
    }), g.shots.end());
}

// Where the wave front is (px from its source); < 0 while it is only warning.
inline float WaveRadius() { return g.waveT < kWaveTele ? -1.0f : (g.waveT - kWaveTele) * kWaveSpeed; }
// How far the front has to run to reach (px, py).
inline float WaveDistance(float px, float py) {
    const float rx = px - g.waveX, ry = py - g.waveY;
    return g.waveLine ? std::fabs(rx * g.waveNx + ry * g.waveNy) : std::hypot(rx, ry);
}
// The direction the player would be shoved (unit), away from the source.
inline void WaveDir(float px, float py, float& dx, float& dy) {
    if (g.waveLine) {
        const float s = (px - g.waveX) * g.waveNx + (py - g.waveY) * g.waveNy;
        dx = s < 0.0f ? -g.waveNx : g.waveNx; dy = s < 0.0f ? -g.waveNy : g.waveNy;
        return;
    }
    dx = px - g.waveX; dy = py - g.waveY;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0f) { dx = 0.0f; dy = 1.0f; } else { dx /= d; dy /= d; }
}
// The line beside the player that shoves them along (nx, ny).
inline void AimLineAt(float px, float py, float nx, float ny) {
    g.waveNx = nx; g.waveNy = ny;
    g.waveX = px - nx * kLineOffset; g.waveY = py - ny * kLineOffset;
}
// (dx, dy) snapped to the nearest of eight ways.
inline void Snap8(float dx, float dy, float& nx, float& ny) {
    const float a = std::round(std::atan2(dy, dx) / 0.78539816f) * 0.78539816f;
    nx = std::cos(a); ny = std::sin(a);
}
// A combo shove: the next wave comes at once (or right after the running one),
// as a line shoving along (nx, ny); (0, 0) aims it at corruption.
inline void QueueShove(float nx = 0.0f, float ny = 0.0f) {
    g.comboQueued = true; g.comboNx = nx; g.comboNy = ny;
    g.comboRing = false;
    g.comboSent = true;
}
// A combo ring wave from where the body is when it starts.
inline void QueueRing() {
    g.comboQueued = true; g.comboRing = true;
    g.comboSent = true;
}
// The line: of eight ways to shove the player, one that lands on corruption
// (or a warning cell) if any does, else one that stays on screen.
inline void AimLine(float px, float py) {
    const int start = rand() % 8;
    int bestK = start, bestScore = -1;
    for (int i = 0; i < 8; ++i) {
        const int k = (start + i) % 8;
        const float a = 0.78539816f * (float)k;
        const float lx = px + std::cos(a) * kWavePush, ly = py + std::sin(a) * kWavePush;
        int score = 0;
        if (lx >= 0.0f && ly >= 0.0f && lx < g_W && ly < g_H) {
            const Cell& c = g.cells[CellAt(lx, ly)];
            score = Hurts(c) || c.state == CellState::Warn ? 2 : 1;
        }
        if (score > bestScore) { bestScore = score; bestK = k; }
    }
    const float a = 0.78539816f * (float)bestK;
    AimLineAt(px, py, std::cos(a), std::sin(a));
}
inline void TickWave(float dt, float bx, float by, float px, float py) {
    if (g.waveT < 0.0f) {
        g.waveCd -= dt;
        if (g.waveCd > 0.0f && !g.comboQueued) return;
        g.waveT = 0.0f; g.waveHit = false;
        g.combo = g.comboQueued;
        if (g.combo && g.comboRing) {
            g.comboQueued = false;
            g.waveLine = false;
            g.waveX = bx; g.waveY = by;
        } else if (g.combo) {
            g.comboQueued = false;
            g.waveLine = true;
            if (g.comboNx != 0.0f || g.comboNy != 0.0f) AimLineAt(px, py, g.comboNx, g.comboNy);
            else AimLine(px, py);
        } else {
            g.waveLine = (g.waves++ % 2) == 1;
            if (g.waveLine) AimLine(px, py); else { g.waveX = bx; g.waveY = by; }
        }
        return;
    }
    const float before = g.waveT;
    g.waveT += dt;
    if (g.waveT >= kWaveTele && before < kWaveTele) g_Events.push_back({ Fx::Wave, g.waveX, g.waveY, 0.0f });
    const float r = WaveRadius();
    const float d = WaveDistance(px, py);
    // Caught as the front passes (not by walking into it well behind).
    if (!g.waveHit && r >= 0.0f && r >= d && r - d < 120.0f) {
        float dx, dy;
        WaveDir(px, py, dx, dy);
        g.pushX += dx * kWavePush; g.pushY += dy * kWavePush;
        g.waveHit = true;
    }
    if (r > (g.waveLine ? kLineRun : std::hypot(g_W, g_H))) {
        g.waveT = -1.0f;
        g.waveCd = g.phase == 2 ? g_Tune.waveEveryP2 : g_Tune.waveEvery;
    }
}

inline void RunPattern(float dt, float px, float py) {
    switch (g.pattern) {
    case Pattern::Spread: {
        // The cursor clicks each seed, then the vines grow.
        while (g.clicked < SeedTotal() && g.t >= kClickSlot * (float)(g.clicked + 1)) {
            const int cell = g.clicked == 0 ? CellAt(px, py) : g.seeds[g.clicked - 1];
            Infect(cell);
            g.frontier.push_back(cell);
            float x, y;
            SeedSpot(g.clicked, px, py, x, y);
            Click(x, y);
            g_Events.push_back({ Fx::Seed, x, y, 0.0f });
            ++g.clicked;
        }
        if (g.clicked < 1) break;
        const float base = kClickSlot * (float)SeedTotal() + WarnTime();
        const int gens = kGenerations;
        while (g.step < gens && g.t >= base + kGenStep * (float)g.step) { StepSpread(); ++g.step; }
        if (g.step >= gens && !g.comboSent) QueueShove();   // into the vine it grew
        if (g.step >= gens && g.t >= base + kGenStep * (float)gens + 0.4f) EndPattern();
        break;
    }
    case Pattern::Scanline: {
        // Ends once every head is through and its last cell has corrupted.
        bool running = false;
        for (const Scan& s : g.scans) if (s.done < s.len) running = true;
        if (running) { g.scanDoneAt = -1.0f; break; }
        if (g.scanDoneAt < 0.0f) {
            g.scanDoneAt = g.t;
            // Combo: shove the player back across the line (unless they stand
            // on it, in the gap: then at whatever corruption is near).
            const Scan& s = g.scans[0];
            const float at = ((float)s.line + 0.5f) * kCell;
            const float off = at - (s.horiz ? py : px);
            if (std::fabs(off) < kCell * 0.5f) QueueShove();
            else if (s.horiz) QueueShove(0.0f, off > 0.0f ? 1.0f : -1.0f);
            else QueueShove(off > 0.0f ? 1.0f : -1.0f, 0.0f);
        }
        if (g.t >= g.scanDoneAt + WarnTime() + 0.3f) { g.scans.clear(); EndPattern(); }
        break;
    }
    case Pattern::Smear:
        TickSmear(dt, px, py);
        // Combo: let go, a shove toward whatever corruption is near.
        if (g.step == 2 && !g.comboSent) QueueShove();
        if (g.step == 2 && g.t >= g.carryEndT + kSmearTail) EndPattern();
        break;
    case Pattern::Siphon:
        TickSiphon(px, py);
        if (g.siphonEndAt >= 0.0f && g.step == 2) {      // swallowed: it spouts it back
            g.step = 3;
            StartSpout(px, py);
        }
        if (g.step >= 3) {
            TickSpout();
            if (g.spoutEndAt >= 0.0f && g.t >= g.spoutEndAt + kSpoutTail) {   // all out: wide open
                g.drops.clear();
                EndPattern();
                g.exposed = kSiphonExpose;
            }
        }
        break;
    case Pattern::Shot: {
        const int n = g.phase == 2 ? g_Tune.snipesP2 : g_Tune.snipes;
        // The aim follows the lead until it locks just before the shot.
        if (g.step < n && g.t < SnipeFireAt(g.step) - kSnipeLock)
            LeadAim(g.bossX, g.bossY, px, py, g.aimX, g.aimY);
        if (g.step < n && g.t >= SnipeFireAt(g.step)) {
            FireSnipe(g.bossX, g.bossY);
            Click(g.aimX, g.aimY);   // the cursor clicks it off
            ++g.step;
        }
        if (g.step >= n && g.t >= SnipeFireAt(n - 1) + 0.35f) EndPattern();
        break;
    }
    default: EndPattern(); break;
    }
}

// One step of the fight. hp / maxHp drive the phase; (px, py) is the player.
inline void Step(float dt, float hp, float maxHp, float bx, float by,
                 float px, float py, float& playerHP) {
    if (dt <= 0.0f) return;
    g.active = true;
    g.time += dt;
    g_LastHit.age += dt;
    if (g.time > dt) {   // the player's velocity, for leading shots
        const float k = std::min(1.0f, dt * 8.0f);
        g.pvx += ((px - g.prevX) / dt - g.pvx) * k;
        g.pvy += ((py - g.prevY) / dt - g.pvy) * k;
        const float sp = std::hypot(g.pvx, g.pvy);
        if (sp > 900.0f) { g.pvx *= 900.0f / sp; g.pvy *= 900.0f / sp; }   // not a shove
    }
    g.prevX = px; g.prevY = py;
    g.bossX = bx; g.bossY = by;
    g.pullX = g.pullY = 0.0f;
    if (!g.acting || g.pattern != Pattern::Siphon) g_DropBalls.clear();
    g.hpRatio = maxHp > 0.0f ? hp / maxHp : 1.0f;
    g.bodyHold = true; g.bodyExact = false;
    g.phaseFlash = std::max(0.0f, g.phaseFlash - dt);
    g.handleFlash = std::max(0.0f, g.handleFlash - dt);
    const int lit = norma::LitStarsFor(hp, maxHp);
    if (lit < g.litHandles) {
        for (int k = lit; k < g.litHandles; ++k) AddBadSector(px, py);
        g.litHandles = lit; g.handleFlash = 0.35f;
    }
    g.formBlend = std::clamp(g.formBlend + (g.form == Form::Sniper ? dt : -dt) / kMorph, 0.0f, 1.0f);
    g.aimAng = g.pattern == Pattern::Shot ? std::atan2(g.aimY - by, g.aimX - bx) : std::atan2(py - by, px - bx);

    if (g.phase == 1 && PhaseFor(hp, maxHp) == 2) {
        g.phase = 2;
        g.phaseFlash = kPhaseFlashSeconds;
        g.invuln = kPhaseInvuln;
        g.exposed = 0.0f;
        g.acting = false; g.pattern = Pattern::None; g.recoverT = 0.0f;
        ClearHazards();
        g_Events.push_back({ Fx::PhaseShift, bx, by, 0.0f });
    }
    if (g.invuln > 0.0f) { g.invuln = std::max(0.0f, g.invuln - dt); return; }

    // The cell CAELUM is in is corrupted (kept fresh while it stays): a trail.
    {
        Cell& c = g.cells[CellAt(bx, by)];
        if (c.state != CellState::Corrupt) {
            c.state = CellState::Corrupt;
            c.life = kTrailLife;
            c.glitch = (unsigned char)(rand() % 256);
        }
        c.t = 0.0f;
    }
    TickCells(dt, px, py, playerHP);
    TickShots(dt, px, py, playerHP);
    TickWave(dt, bx, by, px, py);
    TickCursor(dt, px, py);
    TickBody(dt, px, py);
    if (g.pattern == Pattern::Scanline) TickScans(dt);
    if (g.exposed > 0.0f) { g.exposed = std::max(0.0f, g.exposed - dt); return; }
    if (!g.acting) {
        if (g.morph >= 0.0f) {   // changing form, then the pattern it is for
            g.morph += dt;
            if (g.morph >= kMorph) { g.morph = -1.0f; StartPattern(px, py, g.next); }
            return;
        }
        g.recoverT += dt;
        if (g.recoverT >= (g.phase == 2 ? kRecoverPhase2 : kRecoverSeconds)) {
            const Pattern p = PickPattern();
            if (FormFor(p) == g.form) { StartPattern(px, py, p); return; }
            g.form = FormFor(p);
            g.next = p;
            g.morph = 0.0f;
            g_Events.push_back({ Fx::Morph, bx, by, (float)g.form });
        }
        return;
    }
    g.t += dt;
    RunPattern(dt, px, py);
    UpdateDropBalls(g.time);
}

} // namespace caelum
