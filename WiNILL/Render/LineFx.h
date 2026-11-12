#pragma once
// Line-only building blocks for skill and synergy effects: lightning bolts,
// round circles at any size, sharp-cornered polygons. Everything is a stroke
// (no filled shapes, no dashes); width and alpha carry the emphasis.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "DrawPrim.h"
#include "HudLines.h"
#include "MiterStroke.h"

namespace line_fx {

using hud_lines::Rgb;
inline constexpr float kPi = 3.14159265f;
inline constexpr float kTwoPi = 6.2831853f;

inline uint32_t Hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
// Next value in [0, 1) from a running seed.
inline float Rnd(uint32_t& s) {
    s = Hash(s + 0x9e3779b9u);
    return (float)(s & 0xffffffu) / 16777216.0f;
}
inline uint32_t SeedAt(float x, float y) {
    return Hash((uint32_t)(int)(x * 7.0f) * 73856093u ^ (uint32_t)(int)(y * 13.0f) * 19349663u);
}
inline Rgb Lift(Rgb c, float k) {   // toward white
    return { c.r + (1.0f - c.r) * k, c.g + (1.0f - c.g) * k, c.b + (1.0f - c.b) * k };
}

// A circle as one line; the segment count grows with the radius so big
// circles stay round.
inline void Circle(float x, float y, float radius, float thick, Rgb c, float a) {
    if (radius < 0.5f || a <= 0.0f || thick <= 0.0f) return;
    const int segments = std::clamp((int)(radius * 0.35f), 24, 160);
    const float inner = std::max(0.0f, radius - thick * 0.5f), outer = radius + thick * 0.5f;
    for (int i = 0; i < segments; ++i) {
        const float t0 = kTwoPi * (float)i / (float)segments;
        const float t1 = kTwoPi * (float)(i + 1) / (float)segments;
        const float c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
        BatchTri(x + c0 * outer, y + s0 * outer, x + c1 * outer, y + s1 * outer,
                 x + c1 * inner, y + s1 * inner, c.r, c.g, c.b, a);
        BatchTri(x + c0 * outer, y + s0 * outer, x + c1 * inner, y + s1 * inner,
                 x + c0 * inner, y + s0 * inner, c.r, c.g, c.b, a);
    }
}

// Part of a circle from angle a0 over `sweep` radians.
inline void ArcLine(float x, float y, float radius, float a0, float sweep, float thick,
                    Rgb c, float a) {
    if (radius < 0.5f || a <= 0.0f || std::fabs(sweep) < 0.001f) return;
    const int n = std::clamp((int)(radius * std::fabs(sweep) * 0.06f) + 2, 3, 80);
    float px[81], py[81];
    for (int i = 0; i <= n; ++i) {
        const float t = a0 + sweep * (float)i / (float)n;
        px[i] = x + std::cos(t) * radius; py[i] = y + std::sin(t) * radius;
    }
    miter::Path(px, py, n + 1, thick, c.r, c.g, c.b, a);
}

inline void Line(float x0, float y0, float x1, float y1, float thick, Rgb c, float a) {
    if (a <= 0.0f) return;
    hud_lines::Segment(x0, y0, x1, y1, thick, c, a);
}

// Regular polygon outline (sharp corners). rot in radians.
inline void Polygon(float x, float y, float radius, int sides, float rot, float thick,
                    Rgb c, float a) {
    if (radius < 0.5f || a <= 0.0f) return;
    float px[16], py[16];
    sides = std::clamp(sides, 3, 16);
    for (int i = 0; i < sides; ++i) {
        const float t = rot + kTwoPi * (float)i / (float)sides;
        px[i] = x + std::cos(t) * radius; py[i] = y + std::sin(t) * radius;
    }
    miter::Loop(px, py, sides, thick, c.r, c.g, c.b, a);
}
inline void Diamond(float x, float y, float radius, float thick, Rgb c, float a) {
    Polygon(x, y, radius, 4, -kPi * 0.5f, thick, c, a);
}

// Four-pointed star outline (the ship's own silhouette family).
inline void Star4(float x, float y, float outer, float inner, float rot, float thick,
                  Rgb c, float a) {
    float px[8], py[8];
    for (int i = 0; i < 8; ++i) {
        const float r = (i % 2 == 0) ? outer : inner;
        const float t = rot + kPi * 0.25f * (float)i - kPi * 0.5f;
        px[i] = x + std::cos(t) * r; py[i] = y + std::sin(t) * r;
    }
    miter::Loop(px, py, 8, thick, c.r, c.g, c.b, a);
}

// An irregular rock outline: `sides` corners at seeded radii.
inline void Rock(float x, float y, float radius, int sides, float rot, uint32_t seed,
                 float thick, Rgb c, float a) {
    float px[12], py[12];
    sides = std::clamp(sides, 4, 12);
    for (int i = 0; i < sides; ++i) {
        const float r = radius * (0.72f + 0.28f * Rnd(seed));
        const float t = rot + kTwoPi * ((float)i + 0.3f * Rnd(seed)) / (float)sides;
        px[i] = x + std::cos(t) * r; py[i] = y + std::sin(t) * r;
    }
    miter::Loop(px, py, sides, thick, c.r, c.g, c.b, a);
}

// Lightning from (x0, y0) to (x1, y1): a jagged path, a wide dim line under a
// thin bright core, and up to two short side forks. The same seed gives the
// same bolt; change it a few times a second for flicker.
inline void Bolt(float x0, float y0, float x1, float y1, uint32_t seed, float width,
                 Rgb c, float a, bool forks = true) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 2.0f || a <= 0.0f) return;
    const float nx = -dy / len, ny = dx / len;
    const int n = std::clamp((int)(len / 24.0f), 3, 40);
    const float amp = std::min(len * 0.11f, 24.0f);
    float px[41], py[41];
    px[0] = x0; py[0] = y0;
    for (int i = 1; i < n; ++i) {
        const float u = (float)i / (float)n;
        const float env = std::pow(std::sin(kPi * u), 0.6f);
        const float off = (Rnd(seed) * 2.0f - 1.0f) * amp * env;
        px[i] = x0 + dx * u + nx * off; py[i] = y0 + dy * u + ny * off;
    }
    px[n] = x1; py[n] = y1;
    miter::Path(px, py, n + 1, width * 3.0f, c.r, c.g, c.b, a * 0.22f);
    const Rgb core = Lift(c, 0.55f);
    miter::Path(px, py, n + 1, width, core.r, core.g, core.b, a);
    if (!forks || n < 4) return;
    const int count = len > 220.0f ? 2 : 1;
    for (int k = 0; k < count; ++k) {
        const int i = 1 + (int)(Rnd(seed) * (float)(n - 2));
        const float side = Rnd(seed) < 0.5f ? -1.0f : 1.0f;
        const float turn = side * (0.45f + 0.45f * Rnd(seed));
        const float ang = std::atan2(dy, dx) + turn;
        const float bl = len * (0.14f + 0.16f * Rnd(seed));
        Bolt(px[i], py[i], px[i] + std::cos(ang) * bl, py[i] + std::sin(ang) * bl,
             Hash(seed + 31u * (uint32_t)k), width * 0.6f, c, a * 0.7f, false);
    }
}

}  // namespace line_fx
