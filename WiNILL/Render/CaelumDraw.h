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

// A corrupted cell: a dark see-through film, noise pixels that flicker a few
// times a second, and a couple of torn strips knocked sideways.
inline void FillCorrupt(int cell, float time, float a) {
    using namespace caelum;
    float x, y, w, h;
    CellRect(cell, x, y, w, h);
    const Cell& c = g.cells[cell];
    const unsigned tick = (unsigned)(time / 0.12f);
    drawRect(x, y, w, h, 0.08f, 0.03f, 0.02f, 0.34f * a);              // the film
    const int n = 10;                                                  // noise pixels
    const float pw = w / (float)n, ph = h / (float)n;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const unsigned k = Hash((unsigned)c.glitch + (unsigned)cell * 131u, tick, (unsigned)(j * n + i));
            const unsigned pick = k % 23u;
            if (pick == 0)      drawRect(x + pw * i, y + ph * j, pw, ph, 1.00f, 0.80f, 0.45f, 0.30f * a);
            else if (pick == 1) drawRect(x + pw * i, y + ph * j, pw, ph, 0.95f, 0.95f, 1.00f, 0.22f * a);
            else if (pick == 2) drawRect(x + pw * i, y + ph * j, pw, ph, 0.00f, 0.00f, 0.00f, 0.40f * a);
        }
    }
    for (int s = 0; s < 2; ++s) {                                       // torn strips
        const unsigned k = Hash((unsigned)c.glitch, tick / 2u, (unsigned)(cell * 3 + s));
        const float sy = y + (float)(k % 1000u) / 1000.0f * (h - 10.0f);
        const float sh = 3.0f + (float)((k >> 10) % 7u);
        const float sx = x + (float)((k >> 14) % 40u) - 20.0f;
        const float x0 = std::max(x, sx), x1 = std::min(x + w, sx + w * 0.7f);
        if (x1 > x0) drawRect(x0, sy, x1 - x0, sh, 1.00f, 0.82f, 0.50f, 0.20f * a);
    }
}

inline void DrawCorruptionLayer() {
    using namespace caelum;
    if (!g.active) return;
    for (int i = 0; i < CellCount(); ++i) {
        const Cell& c = g.cells[i];
        float a = 0.0f;
        if (c.state == CellState::Corrupt) a = std::min(1.0f, c.t / 0.15f + 0.4f);
        else if (c.state == CellState::Heal) a = 1.0f - Smooth(c.t / kHeal);
        else if (c.state == CellState::Warn && c.t > c.warn * 0.6f)
            a = std::fmod(g.time * 10.0f, 1.0f) < 0.4f ? 0.5f : 0.0f;   // blinks in before it lands
        if (a > 0.01f) FillCorrupt(i, g.time, a);
        if (c.bad) {   // a bad sector: darker, for good
            float x, y, w, h;
            CellRect(i, x, y, w, h);
            drawRect(x, y, w, h, 0.0f, 0.0f, 0.0f, 0.22f);
        }
    }
    BatchFlush();
}

// Outlines, arrows, meters: all thin lines.
inline void DrawOverlay(float time) {
    using namespace caelum;
    if (!g.active) return;
    const float* A = kAmber;
    const float* H = kHot;
    const float pulse = 0.65f + 0.35f * std::sin(time * 9.0f);
    const bool overflowing = g.overflowT >= 0.0f && g.overflowT < kOverflowTele;
    const bool bursting = g.overflowT >= kOverflowTele;

    // Cells.
    for (int i = 0; i < CellCount(); ++i) {
        const Cell& c = g.cells[i];
        float x, y, w, h;
        CellRect(i, x, y, w, h);
        switch (c.state) {
        case CellState::Warn: {
            const float u = std::clamp(c.t / c.warn, 0.0f, 1.0f);
            const float flick = 0.6f + 0.4f * std::sin(time * 30.0f);
            Box(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, 1.5f + 1.5f * u, H[0], H[1], H[2], (0.35f + 0.55f * u) * flick);
            Hatch(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, 22.0f - 8.0f * u, 1.1f, A[0], A[1], A[2], 0.10f + 0.25f * u);
            break;
        }
        case CellState::Corrupt: {
            // Bad sector: a doubled outline and a cross (it never heals).
            if (c.bad) {
                Box(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 2.6f, A[0], A[1], A[2], 0.95f);
                Box(x + 9.0f, y + 9.0f, w - 18.0f, h - 18.0f, 1.2f, A[0], A[1], A[2], 0.6f);
                Seg(x + 9.0f, y + 9.0f, x + w - 9.0f, y + h - 9.0f, 1.4f, A[0], A[1], A[2], 0.55f);
                Seg(x + w - 9.0f, y + 9.0f, x + 9.0f, y + h - 9.0f, 1.4f, A[0], A[1], A[2], 0.55f);
                break;
            }
            // Scanlines across it (a broken display) and a firm outline, so it
            // reads on dark and bright wallpapers alike.
            const float* C = overflowing ? H : A;
            for (float sy = y + 6.0f; sy < y + h - 2.0f; sy += 10.0f)
                Seg(x + 2.0f, sy, x + w - 2.0f, sy, 1.0f, C[0], C[1], C[2], 0.22f);
            Box(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, overflowing ? 2.6f : 2.0f, C[0], C[1], C[2],
                overflowing ? 0.6f + 0.4f * pulse : 0.9f);
            break;
        }
        case CellState::Heal:
            Box(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 1.0f, A[0], A[1], A[2], 0.4f * (1.0f - c.t / kHeal));
            break;
        default:
            // Overflow: the clean cells it will swell into.
            if ((overflowing || bursting) && OverflowReach(i)) {
                if (overflowing) {
                    const float u = g.overflowT / kOverflowTele;
                    Box(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, 1.6f, H[0], H[1], H[2], (0.3f + 0.6f * u) * pulse);
                    Hatch(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, 16.0f, 1.2f, H[0], H[1], H[2], 0.15f + 0.3f * u);
                } else {
                    const float u = std::clamp((g.overflowT - kOverflowTele) / kOverflowBurst, 0.0f, 1.0f);
                    Box(x, y, w, h, 3.5f * (1.0f - u) + 1.0f, H[0], H[1], H[2], 1.0f - u);
                    Hatch(x, y, w, h, 12.0f, 1.6f, H[0], H[1], H[2], 0.5f * (1.0f - u));
                }
            }
            break;
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

    // 3 Window: the box the cursor drags out; then on the open window its
    // brackets, the cells it takes when closed (hatched, filling in), the
    // grabbed edge and where it is going, and the close button once the
    // cursor heads there.
    if (g.pattern == Pattern::Window && g.acting) {
        if (g.t >= kWinPress && !g.winOpen && g.step < 3) {
            Box(g.winX, g.winY, g.winW, g.winH, 1.4f, H[0], H[1], H[2], 0.85f);
            Corners(g.winX, g.winY, g.winW, g.winH, 22.0f, 2.4f, H[0], H[1], H[2], 0.9f);
        } else if (g.winOpen) {
            Corners(g.winX - 6.0f, g.winY - 6.0f, g.winW + 12.0f, g.winH + 12.0f, 34.0f, 3.0f, H[0], H[1], H[2], 0.9f);
            const float u = std::clamp(g.t / WinClose(), 0.0f, 1.0f);
            for (int i = 0; i < CellCount(); ++i) {
                if (!WinCovers(i)) continue;
                float x, y, w, h;
                CellRect(i, x, y, w, h);
                Box(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, 1.2f, H[0], H[1], H[2], 0.25f + 0.45f * u);
                Hatch(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, 28.0f - 12.0f * u, 1.1f, A[0], A[1], A[2], 0.10f + 0.30f * u);
            }
            if (g.step == 1) {
                const bool vert = g.winSide <= 1;
                if (vert) {
                    const float ex = g.winSide == 0 ? g.winX : g.winX + g.winW;
                    Seg(ex, g.winY, ex, g.winY + g.winH, 4.0f, H[0], H[1], H[2], 1.0f);
                    Seg(g.winTo, g.winY, g.winTo, g.winY + g.winH, 2.0f, H[0], H[1], H[2], 0.45f + 0.35f * pulse);
                } else {
                    const float ey = g.winSide == 2 ? g.winY : g.winY + g.winH;
                    Seg(g.winX, ey, g.winX + g.winW, ey, 4.0f, H[0], H[1], H[2], 1.0f);
                    Seg(g.winX, g.winTo, g.winX + g.winW, g.winTo, 2.0f, H[0], H[1], H[2], 0.45f + 0.35f * pulse);
                }
            }
            if (g.step == 2) {
                float cx, cy;
                CloseSpot(cx, cy);
                Box(cx - 17.0f, cy - 12.0f, 34.0f, 24.0f, 2.0f, H[0], H[1], H[2], pulse);
                Seg(cx - 6.0f, cy - 6.0f, cx + 6.0f, cy + 6.0f, 2.0f, H[0], H[1], H[2], pulse);
                Seg(cx + 6.0f, cy - 6.0f, cx - 6.0f, cy + 6.0f, 2.0f, H[0], H[1], H[2], pulse);
            }
        }
    }

    // Phase 2 erosion: the player's cell fills its outline as they linger.
    if (g.phase == 2 && g.dwellCell >= 0 && g.dwell > kErodeShow) {
        float x, y, w, h;
        CellRect(g.dwellCell, x, y, w, h);
        const float k = std::clamp((g.dwell - kErodeShow) / (kErodeAfter - kErodeShow), 0.0f, 1.0f);
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
                for (int k = 0; k < 3; ++k) {
                    const float f = std::fmod(u * 2.0f + (float)k / 3.0f, 1.0f);
                    Ring(g.waveX, g.waveY, 60.0f + 220.0f * (1.0f - f), 1.6f, H[0], H[1], H[2], (0.2f + 0.6f * u) * f, 48);
                }
                Ring(g.waveX, g.waveY, 60.0f, 2.5f, H[0], H[1], H[2], 0.4f + 0.6f * u, 40);
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
            Ring(g.waveX, g.waveY, r, 4.0f, H[0], H[1], H[2], 0.95f, 96);
            Ring(g.waveX, g.waveY, std::max(0.0f, r - 40.0f), 1.5f, A[0], A[1], A[2], 0.5f, 96);
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
        Pointer(g.curX + jx + 4.0f, g.curY + jy, sc, 1.2f, A[0], A[1], A[2], 0.35f);   // ghost
        Pointer(g.curX, g.curY, sc, 2.2f, H[0], H[1], H[2], 1.0f);
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
