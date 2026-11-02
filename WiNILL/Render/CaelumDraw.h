#pragma once

// ─────────────────────────────────────────────────────────────
// CAELUM's drawing (state and rules: Entity/CaelumBoss.h).
//   Corrupted cells are laid over whatever the screen shows (the desktop
//   picture, other windows) as a see-through glitch: a dark film, flickering
//   noise pixels and torn strips. That film is the one sanctioned fill; every
//   marker (warnings, scanlines, brackets, shots, meters) is thin amber lines.
//   Order in a frame: BeginFrame -> DrawCorruptionLayer (under the mobs)
//   -> mobs -> DrawOverlay. DrawBody is the boss's own body, called from drawMob.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "CaelumBoss.h"
#include "DrawPrim.h"
#include "Monster.h"

namespace caelum_draw {

// The boss's one colour family: pale amber, with a near-white for danger.
constexpr float kAmber[3] = { 1.00f, 0.82f, 0.50f };
constexpr float kHot[3]   = { 1.00f, 0.95f, 0.82f };

// ── Lines ───────────────────────────────────────────────────
inline void Seg(float x1, float y1, float x2, float y2, float w,
                float r, float g, float b, float a) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f || a <= 0.001f) return;
    const float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
    BatchTri(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, r, g, b, a);
    BatchTri(x1 + nx, y1 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny, r, g, b, a);
}
inline void Ring(float cx, float cy, float rad, float w, float r, float g, float b, float a,
                 int segs = 56, float start = 0.0f, float sweep = 6.2831853f) {
    if (rad <= 0.5f || a <= 0.001f) return;
    float px = cx + std::cos(start) * rad, py = cy + std::sin(start) * rad;
    for (int i = 1; i <= segs; ++i) {
        const float t = start + sweep * (float)i / (float)segs;
        const float x = cx + std::cos(t) * rad, y = cy + std::sin(t) * rad;
        Seg(px, py, x, y, w, r, g, b, a);
        px = x; py = y;
    }
}
inline void Box(float x, float y, float w, float h, float lw, float r, float g, float b, float a) {
    Seg(x, y, x + w, y, lw, r, g, b, a);
    Seg(x + w, y, x + w, y + h, lw, r, g, b, a);
    Seg(x + w, y + h, x, y + h, lw, r, g, b, a);
    Seg(x, y + h, x, y, lw, r, g, b, a);
}
// Crop-mark corners of a rectangle (the editor's handles).
inline void Corners(float x, float y, float w, float h, float arm, float lw,
                    float r, float g, float b, float a) {
    arm = std::min(arm, std::min(w, h) * 0.5f);
    Seg(x, y, x + arm, y, lw, r, g, b, a);             Seg(x, y, x, y + arm, lw, r, g, b, a);
    Seg(x + w, y, x + w - arm, y, lw, r, g, b, a);     Seg(x + w, y, x + w, y + arm, lw, r, g, b, a);
    Seg(x, y + h, x + arm, y + h, lw, r, g, b, a);     Seg(x, y + h, x, y + h - arm, lw, r, g, b, a);
    Seg(x + w, y + h, x + w - arm, y + h, lw, r, g, b, a);
    Seg(x + w, y + h, x + w, y + h - arm, lw, r, g, b, a);
}
// Diagonal hatch over a rectangle: marks "this is where it hurts" with solid
// lines only (sparse while warning, dense and bright once it is live).
inline void Hatch(float x, float y, float w, float h, float sp, float lw,
                  float r, float g, float b, float a) {
    if (a <= 0.001f || w < 2.0f || h < 2.0f) return;
    for (float t = 0.0f; t < w + h; t += sp) {
        float s0 = std::max(0.0f, t - w), s1 = std::min(h, t);
        if (s1 <= s0) continue;
        Seg(x + t - s0, y + s0, x + t - s1, y + s1, lw, r, g, b, a);
    }
}

// A mouse pointer's outline, tip at (x, y).
inline void Pointer(float x, float y, float sc, float lw, float r, float gg, float b, float a) {
    static const float P[7][2] = { { 0, 0 }, { 0, 26 }, { 7, 20 }, { 12, 31 }, { 17, 29 }, { 12, 18 }, { 21, 18 } };
    for (int k = 0; k < 7; ++k) {
        const int n = (k + 1) % 7;
        Seg(x + P[k][0] * sc, y + P[k][1] * sc, x + P[n][0] * sc, y + P[n][1] * sc, lw, r, gg, b, a);
    }
}

// A pointing hand (the click model), the fingertip at (x, y).
inline void ClickHand(float x, float y, float sc, float lw, float r, float gg, float b, float a) {
    static const float P[16][2] = { { -3, 0 }, { 3, 0 }, { 3, 12 }, { 7, 11 }, { 11, 12 }, { 14, 14 }, { 17, 15 },
                                    { 19, 18 }, { 19, 28 }, { 16, 34 }, { -2, 34 }, { -6, 28 }, { -9, 20 },
                                    { -9, 16 }, { -6, 15 }, { -3, 19 } };
    for (int k = 0; k < 16; ++k) {
        const int n = (k + 1) % 16;
        Seg(x + P[k][0] * sc, y + P[k][1] * sc, x + P[n][0] * sc, y + P[n][1] * sc, lw, r, gg, b, a);
    }
    // the folded fingers' seams
    Seg(x + 7.0f * sc, y + 11.0f * sc, x + 7.0f * sc, y + 17.0f * sc, lw * 0.6f, r, gg, b, a * 0.8f);
    Seg(x + 11.0f * sc, y + 12.0f * sc, x + 11.0f * sc, y + 18.0f * sc, lw * 0.6f, r, gg, b, a * 0.8f);
    Seg(x + 15.0f * sc, y + 14.5f * sc, x + 15.0f * sc, y + 19.0f * sc, lw * 0.6f, r, gg, b, a * 0.8f);
}
// ── Frame hooks ─────────────────────────────────────────────
// Call once per frame before drawing. Clears the fight's leftovers when the boss
// is gone (the hazards live in caelum::g, advanced only by the boss itself).
inline void BeginFrame(bool bossAlive, float screenW, float screenH) {
    caelum::SetArena(screenW, screenH);
    if (!bossAlive && caelum::g.active) caelum::Reset();
}

inline unsigned Hash(unsigned a, unsigned b, unsigned c) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2)) ^ (c * 40503u);
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12;
    return h;
}

// ── Puddles (marching squares) ──────────────────────────────
// A field over the screen sampled every kStep px, built from per-cell weights
// (a weight under 1 shrinks that cell's blob: a puddle growing in or drying
// up). Filled, contoured and lined by marching squares.
struct FieldGrid {
    static constexpr float kStep = 16.0f;
    int nx = 0, ny = 0;
    bool any = false;
    std::vector<float> v;
    float at(int i, int j) const { return v[(size_t)j * nx + i]; }
};
inline void BuildField(FieldGrid& fg, const float* w, float time, bool carry = false) {
    using namespace caelum;
    fg.nx = (int)(g_W / FieldGrid::kStep) + 2;
    fg.ny = (int)(g_H / FieldGrid::kStep) + 2;
    fg.v.assign((size_t)fg.nx * fg.ny, 0.0f);
    int list[kMaxCols * kMaxRows], n = 0;
    for (int i = 0; i < CellCount(); ++i) if (w[i] > 0.001f) list[n++] = i;
    carry = carry && g.carrying;
    fg.any = n > 0 || carry;
    if (!fg.any) return;
    for (int j = 0; j < fg.ny; ++j)
        for (int i = 0; i < fg.nx; ++i) {
            const float x = (float)i * FieldGrid::kStep, y = (float)j * FieldGrid::kStep;
            float f = carry ? CarryField(x, y, time) : 0.0f;
            for (int k = 0; k < n; ++k) f += w[list[k]] * BlobField(list[k], x, y, time);
            fg.v[(size_t)j * fg.nx + i] = f;
        }
}
// Walks one square of the grid: its inside polygon (corners at or above T and
// the edge crossings) and the crossings alone.
inline int MarchSquare(const FieldGrid& fg, int i, int j, float T, float poly[8][2], float cross[4][2], int& nc) {
    static const int ci[4] = { 0, 1, 1, 0 }, cj[4] = { 0, 0, 1, 1 };
    int np = 0;
    nc = 0;
    for (int k = 0; k < 4; ++k) {
        const int n = (k + 1) & 3;
        const float va = fg.at(i + ci[k], j + cj[k]), vb = fg.at(i + ci[n], j + cj[n]);
        const float ax = (float)(i + ci[k]) * FieldGrid::kStep, ay = (float)(j + cj[k]) * FieldGrid::kStep;
        const float bx = (float)(i + ci[n]) * FieldGrid::kStep, by = (float)(j + cj[n]) * FieldGrid::kStep;
        if (va >= T) { poly[np][0] = ax; poly[np][1] = ay; ++np; }
        if ((va >= T) != (vb >= T)) {
            const float t = (T - va) / (vb - va);
            const float x = ax + (bx - ax) * t, y = ay + (by - ay) * t;
            poly[np][0] = x; poly[np][1] = y; ++np;
            cross[nc][0] = x; cross[nc][1] = y; ++nc;
        }
    }
    return np;
}
inline void FieldFill(const FieldGrid& fg, float T, float r, float g, float b, float a) {
    if (!fg.any || a <= 0.001f) return;
    float poly[8][2], cross[4][2];
    int nc;
    for (int j = 0; j + 1 < fg.ny; ++j)
        for (int i = 0; i + 1 < fg.nx; ++i) {
            const int np = MarchSquare(fg, i, j, T, poly, cross, nc);
            for (int k = 1; k + 1 < np; ++k)
                BatchTri(poly[0][0], poly[0][1], poly[k][0], poly[k][1], poly[k + 1][0], poly[k + 1][1], r, g, b, a);
        }
}
// Like FieldFill, but each vertex's colour runs from c0 to c1 with a second
// field (`mix`, same grid) at that spot: one puddle whose colour deepens
// smoothly where the mix field is strong, with no seam between the two.
inline void FieldFillBlend(const FieldGrid& fg, const FieldGrid& mix, float T, const float c0[4], const float c1[4]) {
    if (!fg.any) return;
    const bool hasMix = mix.any && mix.nx == fg.nx && mix.ny == fg.ny;
    static const int ci[4] = { 0, 1, 1, 0 }, cj[4] = { 0, 0, 1, 1 };
    auto vtx = [&](float x, float y, float m) {
        const float u = caelum::Smooth((m - 0.15f) / 0.75f);
        BatchVtx(x, y, c0[0] + (c1[0] - c0[0]) * u, c0[1] + (c1[1] - c0[1]) * u,
                 c0[2] + (c1[2] - c0[2]) * u, c0[3] + (c1[3] - c0[3]) * u);
    };
    for (int j = 0; j + 1 < fg.ny; ++j)
        for (int i = 0; i + 1 < fg.nx; ++i) {
            float px[8], py[8], pm[8];
            int np = 0;
            for (int k = 0; k < 4; ++k) {
                const int n = (k + 1) & 3;
                const int ai = i + ci[k], aj = j + cj[k], bi = i + ci[n], bj = j + cj[n];
                const float va = fg.at(ai, aj), vb = fg.at(bi, bj);
                const float ma = hasMix ? mix.at(ai, aj) : 0.0f, mb = hasMix ? mix.at(bi, bj) : 0.0f;
                const float ax = (float)ai * FieldGrid::kStep, ay = (float)aj * FieldGrid::kStep;
                const float bx = (float)bi * FieldGrid::kStep, by = (float)bj * FieldGrid::kStep;
                if (va >= T) { px[np] = ax; py[np] = ay; pm[np] = ma; ++np; }
                if ((va >= T) != (vb >= T)) {
                    const float t = (T - va) / (vb - va);
                    px[np] = ax + (bx - ax) * t; py[np] = ay + (by - ay) * t; pm[np] = ma + (mb - ma) * t; ++np;
                }
            }
            for (int k = 1; k + 1 < np; ++k) {
                vtx(px[0], py[0], pm[0]);
                vtx(px[k], py[k], pm[k]);
                vtx(px[k + 1], py[k + 1], pm[k + 1]);
            }
        }
}
inline void FieldContour(const FieldGrid& fg, float T, float w, float r, float g, float b, float a) {
    if (!fg.any || a <= 0.001f) return;
    float poly[8][2], cross[4][2];
    int nc;
    for (int j = 0; j + 1 < fg.ny; ++j)
        for (int i = 0; i + 1 < fg.nx; ++i) {
            MarchSquare(fg, i, j, T, poly, cross, nc);
            for (int k = 0; k + 1 < nc; k += 2)
                Seg(cross[k][0], cross[k][1], cross[k + 1][0], cross[k + 1][1], w, r, g, b, a);
        }
}
// Horizontal lines across the inside, every `every` rows (a broken display).
inline void FieldRows(const FieldGrid& fg, float T, int every, float w, float r, float g, float b, float a) {
    if (!fg.any || a <= 0.001f) return;
    for (int j = 1; j < fg.ny; j += every) {
        const float y = (float)j * FieldGrid::kStep;
        float x0 = -1.0f;
        for (int i = 0; i + 1 < fg.nx; ++i) {
            const float va = fg.at(i, j), vb = fg.at(i + 1, j);
            const float xa = (float)i * FieldGrid::kStep;
            if (va >= T && x0 < 0.0f) x0 = xa;
            if ((va >= T) != (vb >= T)) {
                const float x = xa + FieldGrid::kStep * (T - va) / (vb - va);
                if (va >= T) { Seg(x0, y, x, y, w, r, g, b, a); x0 = -1.0f; }
                else x0 = x;
            }
        }
    }
}

// This frame's fields: the corruption as drawn (growing in, drying up, a
// dragged puddle), the bad sectors (the stronger zones), the warnings, and
// the overflow's reach.
inline FieldGrid g_Corrupt, g_Bad, g_Warn, g_Preview;
inline float g_WarnU = 0.0f;

inline void DrawCorruptionLayer() {
    using namespace caelum;
    if (!g.active) return;
    float wc[kMaxCols * kMaxRows] = {}, wb[kMaxCols * kMaxRows] = {}, ww[kMaxCols * kMaxRows] = {}, wp[kMaxCols * kMaxRows] = {};
    const bool overflowing = g.overflowT >= 0.0f;
    g_WarnU = 0.0f;
    for (int i = 0; i < CellCount(); ++i) {
        const Cell& c = g.cells[i];
        if (c.state == CellState::Corrupt) wc[i] = std::min(1.0f, 0.4f + c.t / 0.15f);
        else if (c.state == CellState::Heal) wc[i] = 1.0f - Smooth(c.t / kHeal);
        else if (c.state == CellState::Warn) {
            const float u = std::clamp(c.t / c.warn, 0.0f, 1.0f);
            ww[i] = 0.55f + 0.45f * u;        // the warning swells to the puddle's size
            g_WarnU = std::max(g_WarnU, u);
        }
        if (c.bad) wb[i] = 1.0f;   // part of the puddles, and drawn deeper on top
        if (overflowing && !Hurts(c) && OverflowReach(i)) wp[i] = 1.0f;
    }
    BuildField(g_Corrupt, wc, g.time, true);
    BuildField(g_Bad, wb, g.time);
    BuildField(g_Warn, ww, g.time);
    BuildField(g_Preview, wp, g.time);

    // The film (the one sanctioned fill): one puddle, deepening smoothly into
    // the bad sectors' stronger zones (blended per vertex, no seam).
    static const float kFilm[4] = { 0.08f, 0.03f, 0.02f, 0.34f };
    static const float kDeep[4] = { 0.05f, 0.01f, 0.005f, 0.68f };
    FieldFillBlend(g_Corrupt, g_Bad, kPuddleEdge, kFilm, kDeep);
    // Warnings blink in before they land.
    if (g_WarnU > 0.6f && std::fmod(g.time * 10.0f, 1.0f) < 0.4f)
        FieldFill(g_Warn, kPuddleEdge, 0.08f, 0.03f, 0.02f, 0.17f);
    BatchFlush();
}

// Outlines, arrows, meters: all thin lines.
inline void DrawOverlay(float time) {
    using namespace caelum;
    if (!g.active) return;
    const float* A = kAmber;
    const float* H = kHot;
    static const float D[3] = { 0.30f, 0.18f, 0.06f };   // dark under-line, for bright desktops
    const float pulse = 0.65f + 0.35f * std::sin(time * 9.0f);
    const bool overflowing = g.overflowT >= 0.0f && g.overflowT < kOverflowTele;
    const bool bursting = g.overflowT >= kOverflowTele;

    // Puddles: lines across them, a dark under-line and a firm rim (it reads
    // on dark and bright desktops alike), a faint inner rim and an outer ripple.
    {
        const float* C = overflowing ? H : A;
        FieldRows(g_Corrupt, kPuddleEdge, 1, 1.0f, C[0], C[1], C[2], 0.18f);
        FieldContour(g_Corrupt, kPuddleEdge, 4.0f, D[0], D[1], D[2], 0.25f);
        FieldContour(g_Corrupt, kPuddleEdge, overflowing ? 2.6f : 2.0f, C[0], C[1], C[2], overflowing ? 0.6f + 0.4f * pulse : 0.9f);
        FieldContour(g_Corrupt, 1.0f, 1.0f, C[0], C[1], C[2], 0.35f);
        FieldContour(g_Corrupt, 0.3f, 1.0f, A[0], A[1], A[2], 0.22f);
        // (Bad sectors have no rim of their own: the film deepens into them.)
        // Warnings: a flickering rim that swells to the puddle's size.
        const float flick = 0.6f + 0.4f * std::sin(time * 30.0f);
        FieldContour(g_Warn, kPuddleEdge, 1.6f + 1.4f * g_WarnU, H[0], H[1], H[2], (0.4f + 0.5f * g_WarnU) * flick);
        FieldContour(g_Warn, 0.85f, 1.0f, A[0], A[1], A[2], 0.25f * flick);
        // Preview: where the overflow will swell.
        if (overflowing && g.overflowT < kOverflowTele) {
            const float u = g.overflowT / kOverflowTele;
            FieldContour(g_Preview, kPuddleEdge, 1.6f, H[0], H[1], H[2], (0.3f + 0.6f * u) * pulse);
            FieldRows(g_Preview, kPuddleEdge, 2, 1.0f, H[0], H[1], H[2], 0.15f + 0.3f * u);
        } else if (bursting) {
            const float u = std::clamp((g.overflowT - kOverflowTele) / kOverflowBurst, 0.0f, 1.0f);
            FieldContour(g_Preview, kPuddleEdge, 3.5f * (1.0f - u) + 1.0f, H[0], H[1], H[2], 1.0f - u);
        }
    }

    // 2 Scanline: the stretch's rails ahead of the head, the head, the gap.
    for (const Scan& s : g.scans) {
        const int len = s.len;
        float lx, ly, lw, lh;
        CellRect(s.horiz ? s.line * Cols() : s.line, lx, ly, lw, lh);
        const float a0 = (float)s.from * kCell, a1 = std::min((float)(s.from + len) * kCell, s.horiz ? g_W : g_H);
        if (s.horiz) { lx = a0; lw = a1 - a0; } else { ly = a0; lh = a1 - a0; }
        const float rail = s.done < len ? 0.55f : 0.0f;
        if (rail > 0.0f) {
            if (s.horiz) { Seg(lx, ly, lx + lw, ly, 1.2f, H[0], H[1], H[2], rail); Seg(lx, ly + lh, lx + lw, ly + lh, 1.2f, H[0], H[1], H[2], rail); }
            else         { Seg(lx, ly, lx, ly + lh, 1.2f, H[0], H[1], H[2], rail); Seg(lx + lw, ly, lx + lw, ly + lh, 1.2f, H[0], H[1], H[2], rail); }
        }
        const int gapCell = s.horiz ? s.line * Cols() + s.gap : s.gap * Cols() + s.line;
        float gx, gy, gw, gh;
        CellRect(gapCell, gx, gy, gw, gh);
        Corners(gx + 6.0f, gy + 6.0f, gw - 12.0f, gh - 12.0f, 26.0f, 3.0f, 0.75f, 1.0f, 0.85f, 0.9f);   // the way through
        if (s.done < len) {
            const float k = std::clamp(s.head, 0.0f, (float)len);
            const float along = ((float)s.from + (s.dir > 0 ? k : (float)len - k)) * kCell;
            if (s.horiz) Seg(along, ly - 10.0f, along, ly + lh + 10.0f, 3.0f, H[0], H[1], H[2], 1.0f);
            else         Seg(lx - 10.0f, along, lx + lw + 10.0f, along, 3.0f, H[0], H[1], H[2], 1.0f);
        }
    }

    // 5 Drag: the cursor's grip closing on the body, then each stroke's path
    // (a line to a framed landing spot) before the body is dragged along it.
    if (g.pattern == Pattern::Drag && g.acting && g.step < 100) {
        if (g.t < kDragGrab) {
            const float u = g.t / kDragGrab;
            Ring(g.bossX, g.bossY, 150.0f - 60.0f * u, 3.4f, D[0], D[1], D[2], 0.2f + 0.2f * u, 48);
            Ring(g.bossX, g.bossY, 150.0f - 60.0f * u, 2.0f, H[0], H[1], H[2], 0.4f + 0.6f * u, 48);
        } else {
            float u;
            const int k = DragStroke(u);
            const float local = g.t - kDragGrab - (float)k * (kDragPreview + kDragMove);
            if (k >= 0 && k < DragStrokes() && local < kDragPreview) {
                const float v = local / kDragPreview;
                Seg(g.dragFromX, g.dragFromY, g.dragToX, g.dragToY, 4.0f, D[0], D[1], D[2], 0.2f + 0.2f * v);
                Seg(g.dragFromX, g.dragFromY, g.dragToX, g.dragToY, 1.8f, H[0], H[1], H[2], 0.35f + 0.6f * v);
                const float s2 = 46.0f;
                Corners(g.dragToX - s2, g.dragToY - s2, s2 * 2.0f, s2 * 2.0f, 16.0f, 2.4f, H[0], H[1], H[2], 0.5f + 0.5f * v);
            }
        }
    }

    // Phase 2 erosion: the player's cell fills its outline as they linger.
    if (g.phase == 2 && g_Tune.erodeAfter > 0.0f && g.dwellCell >= 0 && g.dwell > kErodeShow) {
        float x, y, w, h;
        CellRect(g.dwellCell, x, y, w, h);
        const float k = std::clamp((g.dwell - kErodeShow) / (g_Tune.erodeAfter - kErodeShow), 0.0f, 1.0f);
        const float per = 2.0f * (w + h);
        float left = per * k;
        const float px[5] = { x, x + w, x + w, x, x }, py[5] = { y, y, y + h, y + h, y };
        for (int e = 0; e < 4 && left > 0.0f; ++e) {
            const float len = std::hypot(px[e + 1] - px[e], py[e + 1] - py[e]);
            const float f = std::min(1.0f, left / len);
            Seg(px[e], py[e], px[e] + (px[e + 1] - px[e]) * f, py[e] + (py[e + 1] - py[e]) * f, 2.6f, H[0], H[1], H[2], 0.5f + 0.5f * k);
            left -= len;
        }
    }

    // 4 Snipe: no marker at all; the cursor on the aim point is the warning.
    for (const Shot& s : g.shots) {
        const float sp = std::sqrt(s.vx * s.vx + s.vy * s.vy);
        const float dx = s.vx / sp, dy = s.vy / sp, r = kShotRadius;
        Seg(s.x + dx * r, s.y + dy * r, s.x - dy * r * 0.6f, s.y + dx * r * 0.6f, 2.0f, H[0], H[1], H[2], 1.0f);
        Seg(s.x - dy * r * 0.6f, s.y + dx * r * 0.6f, s.x - dx * r, s.y - dy * r, 2.0f, H[0], H[1], H[2], 1.0f);
        Seg(s.x - dx * r, s.y - dy * r, s.x + dy * r * 0.6f, s.y - dx * r * 0.6f, 2.0f, H[0], H[1], H[2], 1.0f);
        Seg(s.x + dy * r * 0.6f, s.y - dx * r * 0.6f, s.x + dx * r, s.y + dy * r, 2.0f, H[0], H[1], H[2], 1.0f);
        Seg(s.x - dx * r, s.y - dy * r, s.x - dx * (r + 90.0f), s.y - dy * (r + 90.0f), 1.6f, A[0], A[1], A[2], 0.6f);
    }

    // Wave: rings (or lines) closing on the source, then the front. Where the
    // shove sends the player is not shown: reading it is the player's job.
    const float tx = -g.waveNy, ty = g.waveNx;   // along the line
    const float span = std::hypot(g_W, g_H);      // the line runs off the screen both ways
    auto lineAt = [&](float off, float w, float r, float gg, float b, float a) {
        const float mx = g.waveX + g.waveNx * off, my = g.waveY + g.waveNy * off;
        Seg(mx - tx * span, my - ty * span, mx + tx * span, my + ty * span, w, r, gg, b, a);
    };
    if (g.waveT >= 0.0f) {
        if (g.waveT < kWaveTele) {
            const float u = g.waveT / kWaveTele;
            if (g.waveLine) {
                for (int k = 0; k < 3; ++k) {
                    const float f = std::fmod(u * 2.0f + (float)k / 3.0f, 1.0f);
                    const float off = 16.0f + 120.0f * (1.0f - f);
                    lineAt(off, 1.4f, H[0], H[1], H[2], (0.2f + 0.5f * u) * f);
                    lineAt(-off, 1.4f, H[0], H[1], H[2], (0.2f + 0.5f * u) * f);
                }
                lineAt(0.0f, 2.0f + 2.0f * u, H[0], H[1], H[2], 0.45f + 0.55f * u);
            } else {
                // Three ripples closing on the body, then a ring that swells as
                // it is about to go; a thin dark line under each keeps it
                // readable on a bright desktop.
                for (int k = 0; k < 3; ++k) {
                    const float f = std::fmod(u * 2.0f + (float)k / 3.0f, 1.0f);
                    const float rr = 70.0f + 260.0f * (1.0f - f);
                    Ring(g.waveX, g.waveY, rr, 3.4f, D[0], D[1], D[2], (0.12f + 0.2f * u) * f, 64);
                    Ring(g.waveX, g.waveY, rr, 1.9f, H[0], H[1], H[2], (0.25f + 0.5f * u) * f, 64);
                }
                Ring(g.waveX, g.waveY, 64.0f + 14.0f * u * u, 4.2f, D[0], D[1], D[2], 0.2f + 0.2f * u, 48);
                Ring(g.waveX, g.waveY, 64.0f + 14.0f * u * u, 2.4f + 1.0f * u, H[0], H[1], H[2], 0.45f + 0.5f * u, 48);
            }
        } else if (g.waveLine) {
            const float r = WaveRadius();
            const float fade = std::clamp(1.0f - r / kLineRun, 0.0f, 1.0f);
            for (int e = -1; e <= 1; e += 2) {
                lineAt(r * (float)e, 4.0f, H[0], H[1], H[2], 0.95f * fade);
                lineAt(std::max(0.0f, r - 30.0f) * (float)e, 1.5f, A[0], A[1], A[2], 0.5f * fade);
            }
        } else {
            const float r = WaveRadius();
            Ring(g.waveX, g.waveY, r, 6.0f, D[0], D[1], D[2], 0.3f, 128);
            Ring(g.waveX, g.waveY, r, 4.0f, H[0], H[1], H[2], 0.95f, 128);
            Ring(g.waveX, g.waveY, std::max(0.0f, r - 40.0f), 1.5f, A[0], A[1], A[2], 0.5f, 128);
        }
    }

    // Weakened: a slow ring round the player.
    if (g.weaken > 0.0f) {
        const float a = std::clamp(g.weaken / 0.5f, 0.0f, 1.0f);
        Ring(g.prevX, g.prevY, 36.0f, 1.6f, A[0], A[1], A[2], 0.7f * a, 40);
        for (int i = 0; i < 4; ++i) {   // ticks pointing down: slowed
            const float ang = 0.7853982f + 1.5707963f * (float)i + time * 0.6f;
            const float cx = g.prevX + std::cos(ang) * 36.0f, cy = g.prevY + std::sin(ang) * 36.0f;
            Seg(cx - 5.0f, cy - 4.0f, cx, cy + 3.0f, 1.4f, A[0], A[1], A[2], 0.8f * a);
            Seg(cx + 5.0f, cy - 4.0f, cx, cy + 3.0f, 1.4f, A[0], A[1], A[2], 0.8f * a);
        }
    }
    // Where it just hurt: a flash ring on the player (the cause is named in text).
    if (caelum::g_LastHit.age < 0.5f && caelum::g_LastHit.src >= 2) {
        const float u = caelum::g_LastHit.age / 0.5f;
        Ring(g.prevX, g.prevY, 30.0f + 50.0f * u, 3.5f * (1.0f - u) + 1.0f, 1.0f, 0.45f, 0.35f, 1.0f - u, 40);
    }

    BatchFlush();
}

// CAELUM's cursor, in both forms, drawn over everything (the player too: in
// sniper form it sits on the aim point, often on the player). A trail, a click
// ripple, a pointer with a glitched ghost beside it; pressed, it shrinks.
inline void DrawCursor(float time) {
    using namespace caelum;
    if (!g.active) return;
    const float* A = kAmber;
    const float* H = kHot;
    if (g.curX >= 0.0f) {
        for (int k = 1; k < kCurTrail; ++k)
            Seg(g.trail[k - 1][0], g.trail[k - 1][1], g.trail[k][0], g.trail[k][1], 1.4f,
                A[0], A[1], A[2], 0.4f * (1.0f - (float)k / kCurTrail));
        if (g.clickT < 0.35f) {
            const float u = g.clickT / 0.35f;
            Ring(g.clickX, g.clickY, 10.0f + 44.0f * u, 2.6f * (1.0f - u) + 0.6f, H[0], H[1], H[2], 1.0f - u, 28);
        }
        const float sc = g.curPress ? 1.15f : 1.35f;
        const unsigned k = Hash(7u, (unsigned)(time / 0.07f), 3u);
        const float jx = (float)(k % 7u) - 3.0f, jy = (float)((k >> 4) % 5u) - 2.0f;
        // The model: a pointing hand while it presses a puddle (or the body) to
        // lift; the arrow otherwise, dragging included.
        const bool clicking = g.acting && g.curPress &&
            ((g.pattern == Pattern::Smear && g.step == 0) ||
             (g.pattern == Pattern::Drag && g.step < 100 && g.t < kDragGrab));
        auto draw = [&](float x, float y, float lw, const float* c, float a) {
            if (clicking) ClickHand(x, y, sc, lw, c[0], c[1], c[2], a);
            else Pointer(x, y, sc, lw, c[0], c[1], c[2], a);
        };
        draw(g.curX + jx + 4.0f, g.curY + jy, 1.2f, A, 0.35f);   // ghost
        draw(g.curX, g.curY, 2.2f, H, 1.0f);
    }
    BatchFlush();
}

// ── The body ────────────────────────────────────────────────
// A round viewfinder (Docs/Concepts/Caelum_Body_Design_A_v2.png, A1).
// Corrupt form: a round frame with crop marks bent onto it and a three-ring
// lens, all torn sideways in three bands (a broken display, re-rolled a few
// times a second), hollow pixels leaking off one side. Sniper form
// (g.formBlend): the tear closes, the frame pulls in, the lens rings close,
// and a short bore runs from the core to the lens rim with a notch on the
// frame where it points; nothing sticks out further. Eight handles sit on the
// frame (one per eighth of health). Phase 2 tears harder and pushes the crop
// marks off the frame.
inline void DrawBody(const Monster* m, float base, float visualTime) {
    using namespace caelum;
    const float x = m->worldX, y = m->worldY;
    const float hit = std::clamp(m->hitFlashTimer / 0.12f, 0.0f, 1.0f);
    const bool p2 = g.phase == 2;
    const float flash = std::clamp(g.phaseFlash / kPhaseFlashSeconds, 0.0f, 1.0f);
    const float open = std::clamp(g.exposed / 0.25f, 0.0f, 1.0f);     // exposed: the frame lets go
    const float charge = g.acting ? std::min(1.0f, g.t * 2.0f) : 0.0f;
    const float pulse = 0.85f + 0.15f * std::sin(visualTime * 2.0f);
    const float r = m->color.r + (1.0f - m->color.r) * hit * 0.6f;
    const float gg = m->color.g + (1.0f - m->color.g) * hit * 0.6f;
    const float b = m->color.b + (1.0f - m->color.b) * (hit * 0.4f + flash * 0.6f);

    const float sn = Smooth(g.formBlend);                               // 1: sniper form
    const float crop = 1.0f - sn;
    const float R = base * 1.3f * (1.0f - 0.06f * charge + 0.08f * open) * (1.0f - 0.12f * sn);
    const float push = p2 ? R * (0.08f + 0.04f * std::sin(visualTime * 3.0f)) * crop : 0.0f;

    // The tear: three horizontal bands, the lower two shifted sideways.
    const unsigned tick = (unsigned)(visualTime / 0.2f);
    const float amp = base * (p2 ? 0.50f : 0.34f) * crop;
    float dx[3] = { 0.0f, 0.0f, 0.0f };
    for (int i = 1; i < 3; ++i) {
        const unsigned h = Hash(31u, tick, (unsigned)i);
        const float f = (float)(h % 1000u) / 1000.0f;
        dx[i] = amp * (i == 1 ? 0.6f + 0.4f * f : -(0.2f + 0.4f * f));
    }
    const float cut0 = y - R + 2.0f * R * 0.36f, cut1 = y - R + 2.0f * R * 0.60f;
    auto bandOf = [&](float py) { return py < cut0 ? 0 : (py < cut1 ? 1 : 2); };
    // An arc cut where it crosses a band edge, each piece shifted with its band.
    auto tornArc = [&](float rad, float a0, float a1, float w, float al) {
        const int n = std::max(6, (int)(std::fabs(a1 - a0) * rad / 6.0f));
        float px = x + std::cos(a0) * rad, py = y + std::sin(a0) * rad;
        int pb = bandOf(py);
        for (int k = 1; k <= n; ++k) {
            const float a = a0 + (a1 - a0) * (float)k / (float)n;
            const float qx = x + std::cos(a) * rad, qy = y + std::sin(a) * rad;
            const int qb = bandOf(qy);
            if (qb == pb) Seg(px + dx[pb], py, qx + dx[qb], qy, w, r, gg, b, al);
            px = qx; py = qy; pb = qb;
        }
    };
    auto tornSeg = [&](float x0, float y0, float x1, float y1, float w, float al) {
        const float d = dx[bandOf(y0)];
        Seg(x0 + d, y0, x1 + d, y1, w, r, gg, b, al);
    };

    // Frame: a faint ring.
    tornArc(R, 0.0f, 6.2831853f, 1.0f, 0.30f * pulse);
    // Crop marks bent onto the ring: an arc along it and a short arm inward at
    // each end. In sniper form one of them turns to face the aim.
    const float turn = std::remainder(g.aimAng - 0.7853982f, 6.2831853f) * sn;
    const float arm = 0.46f - 0.12f * sn, RB = R + push;
    for (int k = 0; k < 4; ++k) {
        const float a = turn + 0.7853982f + 1.5707963f * (float)k;
        tornArc(RB, a - arm, a + arm, 3.0f + 1.5f * flash, pulse);
        for (int s = -1; s <= 1; s += 2) {
            const float e = a + (float)s * arm;
            tornSeg(x + std::cos(e) * RB, y + std::sin(e) * RB,
                    x + std::cos(e) * RB * 0.84f, y + std::sin(e) * RB * 0.84f, 3.0f + 1.5f * flash, pulse);
        }
    }
    // Lens: three rings in four arcs each (closing into full rings when
    // sniping), turning slowly.
    const float va = m->normaVisualAngle;
    const float lensR[3] = { 0.66f, 0.45f, 0.25f }, lensW[3] = { 2.0f, 1.1f, 1.6f }, lensA[3] = { 0.9f, 0.55f, 0.9f };
    const float half = 0.6f + (0.7853982f - 0.6f) * sn;
    for (int i = 0; i < 3; ++i) {
        const float rr = R * lensR[i] * (1.0f + 0.10f * open);
        const float w = lensW[i] + (i == 0 ? 0.6f * sn : 0.0f);
        for (int k = 0; k < 4; ++k) {
            const float mid = (i == 1 ? -va : va) * 0.6f + 0.3f + 1.5707963f * (float)k;
            tornArc(rr, mid - half, mid + half, w, lensA[i] * (i == 0 ? pulse : 1.0f));
        }
    }
    // Handles on the frame: they go dark as health drops, in this order.
    static const int goesDark[8] = { 1, 3, 5, 7, 0, 2, 4, 6 };
    const int lit = g.litHandles;
    const float hs = base * 0.15f;
    for (int i = 0; i < kHealthHandles; ++i) {
        int pos = 0;
        for (int k = 0; k < 8; ++k) if (goesDark[k] == i) pos = k;
        const bool on = pos >= kHealthHandles - lit;
        const bool justLost = !on && pos == kHealthHandles - lit - 1 && g.handleFlash > 0.0f;
        const float a = -1.5707963f + 0.3926991f + 0.7853982f * (float)i;
        const float hy = y + std::sin(a) * R;
        const float hxp = x + std::cos(a) * R + dx[bandOf(hy)];
        Box(hxp - hs, hy - hs, hs * 2.0f, hs * 2.0f, on ? 2.0f : 1.0f, r, gg, b, on ? 1.0f : (justLost ? 0.9f : 0.18f));
        if (on) Box(hxp - hs * 0.45f, hy - hs * 0.45f, hs * 0.9f, hs * 0.9f, 1.2f, r, gg, b, 0.8f);
    }
    // Corruption leaking out: hollow pixels off the lower right.
    if (crop > 0.01f) {
        for (int i = 0; i < 4; ++i) {
            const unsigned h = Hash(57u, tick, (unsigned)i);
            const float s = base * 0.13f * (1.0f - 0.12f * (float)i);
            const float px = x + R * 0.72f + base * 0.2f * (float)(i + 1) + (float)(h % 9u) - 4.0f;
            const float py = y + R * 0.75f + base * 0.18f * (float)(i + 1);
            Box(px, py, s, s, 1.2f, r, gg, b, (0.85f - 0.18f * (float)i) * crop);
        }
    }
    // Sniper form: a short bore from the core to the lens rim, a notch on the
    // frame where it points, focus ticks on the other three sides.
    if (sn > 0.01f) {
        const float aa = g.aimAng, ux = std::cos(aa), uy = std::sin(aa), nx = -uy, ny = ux;
        const float w = base * 0.11f;
        for (int s = -1; s <= 1; s += 2)
            Seg(x + ux * R * 0.23f + nx * w * (float)s, y + uy * R * 0.23f + ny * w * (float)s,
                x + ux * R * 0.79f + nx * w * (float)s, y + uy * R * 0.79f + ny * w * (float)s,
                2.0f, kHot[0], kHot[1], kHot[2], sn);
        for (int s = -1; s <= 1; s += 2) {
            const float a = aa + 0.16f * (float)s;
            Seg(x + std::cos(a) * R, y + std::sin(a) * R, x + std::cos(a) * R * 1.16f, y + std::sin(a) * R * 1.16f,
                2.4f, r, gg, b, sn);
        }
        for (int k = 1; k <= 3; ++k) {
            const float a = aa + 1.5707963f * (float)k;
            Seg(x + std::cos(a) * R * 0.39f, y + std::sin(a) * R * 0.39f, x + std::cos(a) * R * 0.61f, y + std::sin(a) * R * 0.61f,
                1.6f, r, gg, b, 0.9f * sn);
        }
    }
    // Core: a small ring and crosshair, near white when sniping.
    const float cr = base * (0.20f + 0.10f * open);
    const float cR = r + (kHot[0] - r) * sn, cG = gg + (kHot[1] - gg) * sn, cB = b + (kHot[2] - b) * sn;
    Ring(x, y, cr, 2.0f, cR, cG, cB, 0.85f + 0.15f * pulse, 24);
    Seg(x - cr * 0.55f, y, x + cr * 0.55f, y, 1.4f, cR, cG, cB, 0.85f);
    Seg(x, y - cr * 0.55f, x, y + cr * 0.55f, 1.4f, cR, cG, cB, 0.85f);
    // Charging: short strokes sliding into the frame while a pattern builds.
    if (charge > 0.0f && !open) {
        for (int i = 0; i < 8; ++i) {
            const float a = 0.7853982f * (float)i + 0.3f;
            const float ph = std::fmod(visualTime * 1.6f + (float)i * 0.37f, 1.0f);
            const float r0 = R * (2.0f - 0.9f * ph), r1 = r0 - R * 0.25f;
            Seg(x + std::cos(a) * r0, y + std::sin(a) * r0, x + std::cos(a) * r1, y + std::sin(a) * r1,
                1.3f, r, gg, b, charge * (1.0f - ph) * 0.7f);
        }
    }
}

} // namespace caelum_draw
