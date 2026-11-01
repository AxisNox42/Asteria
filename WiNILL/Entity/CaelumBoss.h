#pragma once

// ─────────────────────────────────────────────────────────────
// CAELUM, the corrupted image (Docs/Caelum_Boss_Design.md). The screen (the
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
//   3 Window   (gimmick) the cursor draws a real window, drags its edge out
//              toward the player, then clicks its close button: everything
//              the window covered corrupts at once
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
//   - Forms    corrupt form lays the board (1-3), sniper form punishes where
//              the player stands (4); the body changes between them
//   - Cursor   CAELUM's own pointer (drawn, never the real mouse) is the hand
//              of the corrupt form: every board pattern is something it does
//   - Combos   every board pattern ends in a line shove aimed into what it
//              laid (the scan line, the window, the spread)
//   - Bad sectors every health handle lost leaves one cell corrupted for good
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
constexpr float kPhase2Ratio = 0.40f;
constexpr float kSizeScale = 3.0f;
constexpr float kContactDps = 10.0f;
constexpr float kPhaseInvuln = 1.6f;
constexpr float kPhaseFlashSeconds = 1.2f;
constexpr float kRecoverSeconds = 0.8f, kRecoverPhase2 = 0.5f;
constexpr float kExposeSeconds = 1.1f;      // opening after every pattern
constexpr float kExposeDamage = 1.5f;
constexpr int   kHealthHandles = 8;

enum class Pattern { None = 0, Spread = 1, Scanline = 2, Window = 3, Shot = 4 };
// Window: a = 0 when it opens, 1 when it goes. Morph: a = the new form.
enum class Fx { Seed, Window, Fire, Overflow, Wave, Morph, PhaseShift };
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
constexpr float kTrailLife = 2.5f;          // the boss's own trail
constexpr float kHeal = 1.0f;
constexpr float kCorruptDps = 20.0f;
// Weakened: while in a corrupted cell and kWeakenLinger s after leaving it.
constexpr float kWeakenLinger = 2.0f;
constexpr float kWeakMove = 0.70f, kWeakDealt = 0.70f;
struct Cell {
    CellState state = CellState::Clean;
    float t = 0.0f;          // seconds in this state
    float life = 0.0f;       // how long it stays corrupted
    float warn = kWarn;
    unsigned char glitch = 0;   // per-cell seed for the drawing
    bool  bad = false;          // a bad sector: corrupted for good
};

// ── Bad sectors ─────────────────────────────────────────────
// Each health handle lost corrupts one more cell for good (away from the player,
// not touching another), so the board shrinks as the fight goes on.
constexpr int   kMaxBad = 6;

// ── 1 Spread ────────────────────────────────────────────────
// Seeds (the first under the player) warn and corrupt; every generation each
// cell of the last one infects one clean neighbour (a short creeping vine).
constexpr int   kSeeds = 2, kSeedsP2 = 3;
constexpr int   kGenerations = 2, kGenerationsP2 = 3;
constexpr float kGenStep = 0.55f;

// ── 2 Scanline ──────────────────────────────────────────────
// A write head runs along a stretch of kScanLen cells of the row (or column)
// through the player, warning each cell as it passes; one gap cell is spared.
// Phase 2 writes a row and a column.
constexpr float kScanRate = 12.0f;          // cells per second
constexpr int   kScanLen = 5;

// ── 3 Window ────────────────────────────────────────────────
// The cursor goes to a spot away from the player, drags out a window (a real
// one opens when it lets go; the game opens it, see main.cpp), grabs the edge
// facing the player and drags it out past them, then goes to the close button
// and clicks. Every cell the window covered by kWinCover of its area corrupts
// at once (kWinDeleteWarn), and a combo shove pushes the player toward it.
constexpr float kWinW = 340.0f, kWinH = 230.0f;
constexpr float kWinMinDist = 420.0f;       // the spot is this far from the player at least
constexpr float kWinCover = 0.30f;
constexpr float kWinDeleteWarn = 0.70f;
constexpr float kWinStretch = 640.0f, kWinStretchP2 = 820.0f;   // most an edge is dragged
// Seconds into the pattern: at the corner (press), let go (opens), on the edge
// (grab), edge let go, close clicked, done.
constexpr float kWinPress = 0.30f, kWinOpen = 0.85f, kWinGrab = 1.20f;
constexpr float kWinDrag = 1.00f, kWinDragP2 = 0.75f;
constexpr float kWinToClose = 0.95f, kWinTail = 0.70f;   // the cursor takes its time to close

// ── Cursor ──────────────────────────────────────────────────
// CAELUM's pointer, in both forms. It glides to where it is wanted (kCurFollow,
// 1/s) and sits exactly on a drag. Snipe: it rides the aim point and clicks
// each shot off. Spread: one click every kClickSlot (it presses kClickHold
// before). Scanline: the head waits kCurLead while the cursor gets there.
constexpr float kCurFollow = 14.0f;
constexpr float kClickSlot = 0.32f, kClickHold = 0.08f;
constexpr float kCurLead = 0.30f;
constexpr float kCurHover = 190.0f;         // idle: this far from the player, toward the boss
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
constexpr float kWaveTele = 0.90f;
constexpr float kWaveSpeed = 1400.0f;       // px/s of the front
constexpr float kWavePush = kCell;          // how far the player is shoved: one cell
constexpr float kLineOffset = 120.0f;
constexpr float kLineRun = 700.0f;          // how far its front runs

// ── Erode (phase 2) ─────────────────────────────────────────
constexpr float kErodeAfter = 1.6f;         // seconds in one cell
constexpr float kErodeShow = 0.4f;          // the meter shows from here

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

    float   winX = 0.0f, winY = 0.0f, winW = 0.0f, winH = 0.0f;   // Window: its frame
    int     winSide = 0;            // the edge dragged: 0 left, 1 right, 2 top, 3 bottom
    float   winFrom = 0.0f, winTo = 0.0f;   // that edge's coordinate, before and after
    bool    winOpen = false;        // the game keeps a real window here while true

    float   curX = -1.0f, curY = -1.0f;   // the cursor (< 0: not placed yet)
    bool    curPress = false;
    float   clickT = 9.0f;          // seconds since the last click
    float   clickX = 0.0f, clickY = 0.0f;
    float   trail[kCurTrail][2] = {};
    float   trailTick = 0.0f;

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

inline float WarnTime() { return g.phase == 2 ? kWarnP2 : kWarn; }
inline bool Hurts(const Cell& c) { return c.state == CellState::Corrupt; }

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
    g.waveT = -1.0f; g.waveCd = kWaveEvery; g.comboQueued = false;
    g.winOpen = false;
    g.morph = -1.0f;
}

// One more bad sector: a clean cell two or more cells from the player, not the
// boss's, and (if it can) not touching another bad sector.
inline void AddBadSector(float px, float py) {
    if (BadCount() >= kMaxBad) return;
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
            d.glitch = (unsigned char)(rand() % 256);
            float x, y;
            CellCentre(i, x, y);
            g_Events.push_back({ Fx::Seed, x, y, 0.0f });
            return;
        }
    }
}
inline void Reset() { g = State{}; }

// Player's movement and damage to CAELUM while weakened.
inline float MoveFactor() { return g.active && g.weaken > 0.0f ? kWeakMove : 1.0f; }
inline float DealtFactor() { return g.active && g.weaken > 0.0f ? kWeakDealt : 1.0f; }

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
    return (maxHp > 0.0f && hp <= maxHp * kPhase2Ratio) ? 2 : 1;
}

// ── Cells, overflow, erosion, damage ────────────────────────
inline void TickCells(float dt, float px, float py, float& playerHP) {
    for (int i = 0; i < CellCount(); ++i) {
        Cell& c = g.cells[i];
        c.t += dt;
        switch (c.state) {
        case CellState::Warn:    if (c.t >= c.warn) { c.state = CellState::Corrupt; c.t = 0.0f; } break;
        case CellState::Corrupt: if (!c.bad && c.t >= c.life) { c.state = CellState::Heal; c.t = 0.0f; } break;
        case CellState::Heal:    if (c.t >= kHeal) c = Cell{}; break;
        default: break;
        }
    }
    const int here = CellAt(px, py);
    // Standing in corruption: steady damage and weakness.
    g.weaken = std::max(0.0f, g.weaken - dt);
    if (Hurts(g.cells[here])) {
        g.weaken = kWeakenLinger;
        Hit(playerHP, kCorruptDps * dt, 1, px, py);
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
            Hit(playerHP, kOverflowDamage, 2, px, py);
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
    if (g.phase == 2) {
        if (here != g.dwellCell) { g.dwellCell = here; g.dwell = 0.0f; }
        if (g.cells[here].state == CellState::Clean) g.dwell += dt; else g.dwell = 0.0f;
        if (g.dwell >= kErodeAfter) { Infect(here); g.dwell = 0.0f; }
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
    static const Pattern p1[] = { Pattern::Spread, Pattern::Scanline, Pattern::Shot,
                                  Pattern::Window, Pattern::Spread, Pattern::Shot,
                                  Pattern::Scanline, Pattern::Window, Pattern::Shot };
    const int n = (int)(sizeof(p1) / sizeof(p1[0]));
    Pattern pick = p1[g.count % n];
    if (pick == g.last) pick = p1[(g.count + 1) % n];
    return pick;
}

// Snipe: seconds into the pattern when shot k fires.
inline float SnipeFireAt(int k) { return kSnipeFirst + (g.phase == 2 ? kSnipeStepP2 : kSnipeStep) * (float)k; }
// Spread: the seeds after the first are picked now; the first is wherever the
// player is when the cursor clicks.
inline int SeedCount() { return g.phase == 2 ? kSeedsP2 : kSeeds; }
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

// Window: a spot away from the player, inside the screen (its top-left; the
// cursor drags the rest out from there).
inline void StartWindow(float px, float py) {
    float best = -1.0f;
    for (int tries = 0; tries < 24; ++tries) {
        const float x = RndRange(20.0f, std::max(20.0f, g_W - kWinW - 20.0f));
        const float y = RndRange(20.0f, std::max(20.0f, g_H - kWinH - 20.0f));
        const float d = std::hypot(x + kWinW * 0.5f - px, y + kWinH * 0.5f - py);
        if (d > best) { best = d; g.winX = x; g.winY = y; }
        if (d >= kWinMinDist) break;
    }
    g.winW = g.winH = 1.0f;
    g.winOpen = false;
}
inline float WinDragTime() { return g.phase == 2 ? kWinDragP2 : kWinDrag; }
inline float WinLetGo() { return kWinGrab + WinDragTime(); }
inline float WinClose() { return WinLetGo() + kWinToClose; }
// True when the window covers kWinCover of the cell (it corrupts when deleted).
inline bool WinCovers(int cell) {
    float x, y, w, h;
    CellRect(cell, x, y, w, h);
    const float ox = std::min(x + w, g.winX + g.winW) - std::max(x, g.winX);
    const float oy = std::min(y + h, g.winY + g.winH) - std::max(y, g.winY);
    return ox > 0.0f && oy > 0.0f && ox * oy >= kWinCover * w * h;
}
// Grabs the edge facing the player; it will be dragged out a cell past them.
inline void GrabEdge(float px, float py) {
    const float cx = g.winX + g.winW * 0.5f, cy = g.winY + g.winH * 0.5f;
    const float dx = (px - cx) / g.winW, dy = (py - cy) / g.winH;
    const float most = g.phase == 2 ? kWinStretchP2 : kWinStretch, past = kCell * 0.6f;
    if (std::fabs(dx) >= std::fabs(dy)) {
        g.winSide = dx < 0.0f ? 0 : 1;
        g.winFrom = dx < 0.0f ? g.winX : g.winX + g.winW;
        g.winTo = dx < 0.0f ? std::max({ 0.0f, px - past, g.winFrom - most })
                            : std::min({ g_W, px + past, g.winFrom + most });
    } else {
        g.winSide = dy < 0.0f ? 2 : 3;
        g.winFrom = dy < 0.0f ? g.winY : g.winY + g.winH;
        g.winTo = dy < 0.0f ? std::max({ 0.0f, py - past, g.winFrom - most })
                            : std::min({ g_H, py + past, g.winFrom + most });
    }
    // Never shrink it: a player behind the edge is dragged at nonetheless.
    if ((g.winSide == 0 || g.winSide == 2) ? g.winTo > g.winFrom : g.winTo < g.winFrom) g.winTo = g.winFrom;
}
// Moves the grabbed edge to `at`.
inline void SetEdge(float at) {
    switch (g.winSide) {
    case 0: g.winW += g.winX - at; g.winX = at; break;
    case 1: g.winW = at - g.winX; break;
    case 2: g.winH += g.winY - at; g.winY = at; break;
    default: g.winH = at - g.winY; break;
    }
}
// The middle of the grabbed edge (where the cursor holds it).
inline void EdgeSpot(float& x, float& y) {
    x = g.winSide == 0 ? g.winX : g.winSide == 1 ? g.winX + g.winW : g.winX + g.winW * 0.5f;
    y = g.winSide == 2 ? g.winY : g.winSide == 3 ? g.winY + g.winH : g.winY + g.winH * 0.5f;
}
// The close button, top right of the title bar.
inline void CloseSpot(float& x, float& y) { x = g.winX + g.winW - 22.0f; y = g.winY + 15.0f; }
inline void TickWindow(float px, float py) {
    const float t = g.t;
    if (t >= kWinPress && t < kWinOpen) {             // dragging it out
        const float u = Smooth((t - kWinPress) / (kWinOpen - kWinPress));
        g.winW = std::max(1.0f, kWinW * u); g.winH = std::max(1.0f, kWinH * u);
    } else if (t >= kWinOpen && !g.winOpen && t < WinClose()) {
        g.winW = kWinW; g.winH = kWinH;
        g.winOpen = true;
        Click(g.winX + g.winW, g.winY + g.winH);
        g_Events.push_back({ Fx::Window, g.winX + g.winW * 0.5f, g.winY + g.winH * 0.5f, 0.0f });
    }
    if (t >= kWinGrab && g.step == 0) { GrabEdge(px, py); g.step = 1; }
    if (g.step == 1) {                                 // dragging the edge
        const float u = Smooth((t - kWinGrab) / WinDragTime());
        SetEdge(g.winFrom + (g.winTo - g.winFrom) * u);
        if (t >= WinLetGo()) g.step = 2;   // let go; RunPattern sends the combo
    }
    if (g.step == 2 && t >= WinClose()) {              // close: what it covered corrupts
        g.step = 3;
        float cx, cy;
        CloseSpot(cx, cy);
        Click(cx, cy);
        for (int i = 0; i < CellCount(); ++i) if (WinCovers(i)) Infect(i, kWinDeleteWarn);
        g.winOpen = false;
        g_Events.push_back({ Fx::Window, g.winX + g.winW * 0.5f, g.winY + g.winH * 0.5f, 1.0f });
    }
}

// ── Cursor ──────────────────────────────────────────────────
// Where the cursor wants to be this frame, whether it presses, and whether it
// sits exactly there (a drag) rather than gliding.
inline void CursorWant(float px, float py, float& tx, float& ty, bool& press, bool& exact) {
    press = false; exact = false;
    // Idle: hovering beside the player, on the boss's side.
    {
        const float dx = g.bossX - px, dy = g.bossY - py, d = std::max(1.0f, std::hypot(dx, dy));
        tx = px + dx / d * kCurHover; ty = py + dy / d * kCurHover;
    }
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
        if (g.step < (g.phase == 2 ? kSnipesP2 : kSnipes) && g.t >= SnipeFireAt(g.step) - kSnipeAim) {
            tx = g.aimX; ty = g.aimY;
            press = g.t >= SnipeFireAt(g.step) - kSnipeLock;
        }
        break;
    case Pattern::Window:
        if (g.t < kWinPress) { tx = g.winX; ty = g.winY; }
        else if (g.t < kWinOpen) { tx = g.winX + g.winW; ty = g.winY + g.winH; press = exact = true; }
        else if (g.step == 0) { GrabEdge(px, py); EdgeSpot(tx, ty); g.winTo = g.winFrom; }   // heading for the edge
        else if (g.step == 1) { EdgeSpot(tx, ty); press = exact = true; }
        else if (g.step == 2) { CloseSpot(tx, ty); press = g.t >= WinClose() - kClickHold; }
        break;
    default: break;
    }
}
inline void TickCursor(float dt, float px, float py) {
    float tx, ty;
    bool press, exact;
    CursorWant(px, py, tx, ty, press, exact);
    if (g.curX < 0.0f || exact) { g.curX = tx; g.curY = ty; }
    else {
        const float k = std::min(1.0f, kCurFollow * dt);
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
    case Pattern::Window: StartWindow(px, py); break;
    default: break;
    }
}

// Where a shot from (bx, by) meets the player if they keep moving as they are.
inline void LeadAim(float bx, float by, float px, float py, float& ax, float& ay) {
    ax = px; ay = py;
    for (int k = 0; k < 2; ++k) {
        const float t = std::hypot(ax - bx, ay - by) / kShotSpeed;
        ax = px + g.pvx * t; ay = py + g.pvy * t;
    }
    ax = std::clamp(ax, 0.0f, g_W); ay = std::clamp(ay, 0.0f, g_H);
}
inline void FireSnipe(float bx, float by) {
    const float dx = g.aimX - bx, dy = g.aimY - by, d = std::max(1.0f, std::hypot(dx, dy));
    Shot s;
    s.x = bx; s.y = by;
    s.vx = dx / d * kShotSpeed; s.vy = dy / d * kShotSpeed;
    s.left = d + kShotOvershoot;
    g.shots.push_back(s);
    g_Events.push_back({ Fx::Fire, bx, by, std::atan2(dy, dx) });
}
inline void TickShots(float dt, float px, float py, float& playerHP) {
    for (Shot& s : g.shots) {
        const float ox = s.x, oy = s.y;
        s.x += s.vx * dt; s.y += s.vy * dt;
        s.left -= kShotSpeed * dt;
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
        if (g.combo) {
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
        g.waveCd = g.phase == 2 ? kWaveEveryP2 : kWaveEvery;
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
        const int gens = g.phase == 2 ? kGenerationsP2 : kGenerations;
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
    case Pattern::Window:
        TickWindow(px, py);
        // Combo: as the cursor heads for close, shove the player toward the window.
        if (g.step >= 2 && !g.comboSent) {
            const float dx = g.winX + g.winW * 0.5f - px, dy = g.winY + g.winH * 0.5f - py;
            if (std::fabs(dx) < g.winW * 0.5f && std::fabs(dy) < g.winH * 0.5f) QueueShove();
            else { float nx, ny; Snap8(dx, dy, nx, ny); QueueShove(nx, ny); }
        }
        if (g.t >= WinClose() + kWinTail) EndPattern();
        break;
    case Pattern::Shot: {
        const int n = g.phase == 2 ? kSnipesP2 : kSnipes;
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
}

} // namespace caelum
