#pragma once
// Shared hit effects for augments, skills and synergies (world space).
// Lines only (no fills, no dashes), sharp corners. Each bright stroke has a
// dark under-stroke so it still reads on a bright wallpaper.
//
//   Blast     a range burst out to the damage radius, drawn by family
//             (BlastStyle.h): Impact, Gravity, Discharge, Stellar, Crystal
//   Bolt      lightning between two points (chains)
//   Zap       lightning out of the ship onto a target, a small blast where it lands
//   Star      a four-point star launched from the ship, curving onto a target, landing as a Blast
//   Nova      a ring sweeping out from a point across the whole screen
//   Sweep     a radar line turning once round the ship, a fan of fading lines behind it
//   Implode   a ring closing in on a point, shards drawn inward with it
//   Vortex    a small black hole: the pull's reach, spiral arms winding into
//             a tilted accretion disk and a dark event horizon (gravity shots)
//             (pulls, charges, cooldowns, the gather before a revive)
//
// Damage is applied by the caller at once; these only show it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "BlastStyle.h"
#include "LineFx.h"

namespace strike_fx {

using line_fx::Rgb;

inline constexpr Rgb kUnder{ 0.04f, 0.06f, 0.10f };

enum class Kind { Blast, Bolt, Star, Nova, Implode, Sweep, Vortex };

struct Fx {
    Kind kind = Kind::Blast;
    float x = 0.0f, y = 0.0f;     // target / centre
    float x0 = 0.0f, y0 = 0.0f;   // Bolt, Star: start
    float r = 0.0f;               // Blast: radius; Nova: reach; Star: landing blast radius; Implode: start radius
    float width = 2.0f;
    float delay = 0.0f, age = 0.0f, life = 0.4f;
    uint32_t seed = 1;
    Rgb c{ 1.0f, 0.6f, 0.25f };
    bool fromShip = false;        // Zap: the bolt starts at the ship wherever it is now
    BlastStyle style = BlastStyle::Impact;   // Blast: which family draws it
};

// The ship's centre this frame (Tick), for bolts that leave from it.
inline float g_ShipX = 0.0f, g_ShipY = 0.0f;

inline std::vector<Fx> g_Fx;
inline constexpr size_t kCap = 700;

inline uint32_t NextSeed() {
    static uint32_t n = 0x1234567u;
    n = line_fx::Hash(n + 0x9e3779b9u);
    return n;
}

inline void Push(const Fx& f) {
    if (g_Fx.size() < kCap) g_Fx.push_back(f);
}

inline void Blast(float x, float y, float radius, Rgb c, float delay = 0.0f,
                  BlastStyle style = BlastStyle::Impact) {
    if (radius < 4.0f) return;
    Fx f; f.kind = Kind::Blast; f.x = x; f.y = y; f.r = radius; f.c = c;
    f.delay = delay; f.style = style;
    f.life = 0.36f + std::min(0.30f, radius / 1400.0f);
    f.width = radius >= 200.0f ? 3.0f : 2.2f;
    f.seed = NextSeed();
    Push(f);
}

inline void Bolt(float x0, float y0, float x1, float y1, Rgb c, float width = 2.2f,
                 float delay = 0.0f, float life = 0.30f) {
    Fx f; f.kind = Kind::Bolt; f.x0 = x0; f.y0 = y0; f.x = x1; f.y = y1;
    f.c = c; f.width = width; f.delay = delay; f.life = life; f.seed = NextSeed();
    Push(f);
}

// Lightning out of the ship at (sx, sy) onto (x, y), leaving from the ship's
// edge rather than its centre, with a small blast where it lands.
inline void Zap(float sx, float sy, float x, float y, Rgb c, float width = 2.4f,
                float delay = 0.0f, float blastR = 46.0f) {
    const float dx = x - sx, dy = y - sy, d = std::sqrt(dx * dx + dy * dy);
    const float edge = std::min(22.0f, d * 0.5f);
    if (d > 1.0f) { sx += dx / d * edge; sy += dy / d * edge; }
    Bolt(sx, sy, x, y, c, width, delay, 0.32f);
    if (!g_Fx.empty() && g_Fx.back().kind == Kind::Bolt) g_Fx.back().fromShip = true;
    if (blastR > 0.0f) Blast(x, y, blastR, c, delay, BlastStyle::Discharge);
}

// A star launched from the ship at (sx, sy) onto (x, y), curving off to one
// side (bend > 0 left, < 0 right) and speeding up, landing as a blast.
// Returns the seconds until it lands (delay + flight), so the caller can land
// the hit with it.
inline float Star(float sx, float sy, float x, float y, Rgb c, float delay, float bend,
                  float blastR = 70.0f) {
    const float dx = x - sx, dy = y - sy, d = std::sqrt(dx * dx + dy * dy);
    Fx f; f.kind = Kind::Star; f.x = x; f.y = y; f.c = c; f.r = blastR;
    f.x0 = sx; f.y0 = sy; f.width = bend;   // width carries the bend for a star
    f.delay = delay; f.life = 0.20f + std::min(0.30f, d / 2400.0f); f.seed = NextSeed();
    Push(f);
    return f.delay + f.life;
}

// Where a star is at progress u (0 ship .. 1 target): a curve that leaves the
// ship sideways and straightens onto the target.
inline void StarAt(const Fx& f, float u, float& px, float& py) {
    const float dx = f.x - f.x0, dy = f.y - f.y0;
    const float d = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    const float side = f.width * std::sin(line_fx::kPi * u);
    px = f.x0 + dx * u - dy / d * side;
    py = f.y0 + dy * u + dx / d * side;
}

// A small black hole at (x, y) pulling from `reach`, over `life` seconds.
inline void Vortex(float x, float y, float reach, Rgb c, float life = 0.42f) {
    Fx f; f.kind = Kind::Vortex; f.x = x; f.y = y; f.r = reach; f.c = c;
    f.life = life; f.width = 1.6f; f.seed = NextSeed();
    Push(f);
}

// One clockwise turn from straight up, `life` seconds long, out to `reach`.
inline void Sweep(float x, float y, float reach, Rgb c, float life = 0.45f) {
    Fx f; f.kind = Kind::Sweep; f.x = x; f.y = y; f.r = reach; f.c = c;
    f.life = life; f.width = 2.0f; f.seed = NextSeed();
    Push(f);
}

inline void Implode(float x, float y, float radius, Rgb c, float delay = 0.0f, float life = 0.35f) {
    Fx f; f.kind = Kind::Implode; f.x = x; f.y = y; f.r = radius; f.c = c;
    f.delay = delay; f.life = life; f.width = 2.2f; f.seed = NextSeed();
    Push(f);
}

// A revive: everything gathers into the ship, then a nova bursts out of it.
inline void Revive(float x, float y, Rgb c) {
    Implode(x, y, 260.0f, c, 0.0f, 0.32f);
    Implode(x, y, 150.0f, line_fx::Lift(c, 0.5f), 0.08f, 0.26f);
    Fx f; f.kind = Kind::Nova; f.x = x; f.y = y; f.r = 900.0f; f.c = c;
    f.delay = 0.30f; f.life = 0.80f; f.width = 4.0f; f.seed = NextSeed();
    Push(f);
}

inline void Nova(float x, float y, float reach, Rgb c, float delay = 0.0f) {
    Fx f; f.kind = Kind::Nova; f.x = x; f.y = y; f.r = reach; f.c = c;
    f.delay = delay; f.life = 0.70f; f.width = 3.5f; f.seed = NextSeed();
    Push(f);
}

inline void Clear() { g_Fx.clear(); }

inline void Tick(float dt, float shipX, float shipY) {
    g_ShipX = shipX; g_ShipY = shipY;
    std::vector<Fx> landed;
    for (Fx& f : g_Fx) {
        if (f.delay > 0.0f) { f.delay -= dt; continue; }
        const bool was = f.age < f.life;
        f.age += dt;
        if (f.kind == Kind::Star && was && f.age >= f.life) {
            Fx b; b.kind = Kind::Blast; b.x = f.x; b.y = f.y; b.r = f.r; b.c = f.c;
            b.life = 0.40f; b.width = 2.4f; b.seed = f.seed; b.style = BlastStyle::Stellar;
            landed.push_back(b);
        }
    }
    g_Fx.erase(std::remove_if(g_Fx.begin(), g_Fx.end(),
        [](const Fx& f) { return f.delay <= 0.0f && f.age >= f.life; }), g_Fx.end());
    for (const Fx& b : landed) Push(b);
}

// A stroke over its dark under-stroke.
inline void Stroke(float x0, float y0, float x1, float y1, float w, Rgb c, float a) {
    line_fx::Line(x0, y0, x1, y1, w + 2.0f, kUnder, a * 0.30f);
    line_fx::Line(x0, y0, x1, y1, w, c, a);
}
inline void Ring(float x, float y, float r, float w, Rgb c, float a) {
    line_fx::Circle(x, y, r, w + 2.0f, kUnder, a * 0.28f);
    line_fx::Circle(x, y, r, w, c, a);
}

inline float EaseOut(float u) { u = std::clamp(u, 0.0f, 1.0f); return 1.0f - (1.0f - u) * (1.0f - u); }

// Impact: a ring with a rough edge (no two corners alike), rock chips thrown out.
inline void DrawImpact(const Fx& f, float t) {
    const float e = EaseOut(t / 0.45f);
    const float fade = std::pow(1.0f - t, 1.3f);
    const Rgb lit = line_fx::Lift(f.c, 0.40f);
    uint32_t s = f.seed;
    const float R = f.r * (0.30f + 0.70f * e);
    const int n = std::clamp((int)(f.r / 9.0f), 12, 40);
    float px[40], py[40];
    for (int i = 0; i < n; ++i) {
        const float a = line_fx::kTwoPi * (float)i / (float)n;
        const float rr = R * (0.955f + 0.07f * line_fx::Rnd(s));   // a slightly rough edge, not a blob
        px[i] = f.x + std::cos(a) * rr; py[i] = f.y + std::sin(a) * rr;
    }
    const float w = f.width * (1.0f - 0.45f * t);
    miter::Loop(px, py, n, w + 2.0f, kUnder.r, kUnder.g, kUnder.b, 0.28f * fade);
    miter::Loop(px, py, n, w, lit.r, lit.g, lit.b, 0.95f * fade);
    // Rock chips: small outlines flying out past the edge, turning as they go.
    const int chips = std::clamp((int)(f.r / 22.0f), 5, 12);
    for (int i = 0; i < chips; ++i) {
        const float a = line_fx::kTwoPi * ((float)i + 0.7f * line_fx::Rnd(s)) / (float)chips;
        const float d = f.r * (0.25f + (0.75f + 0.30f * line_fx::Rnd(s)) * e);
        const float size = (3.0f + 4.0f * line_fx::Rnd(s)) * (1.0f - 0.6f * t);
        line_fx::Rock(f.x + std::cos(a) * d, f.y + std::sin(a) * d, size, 5, a + t * 6.0f,
                      line_fx::Hash(f.seed + (uint32_t)i), 1.4f, lit, 0.9f * fade);
    }
    const float core = std::min(18.0f, f.r * 0.16f) * (1.0f - t);
    if (core > 1.0f) line_fx::Diamond(f.x, f.y, core, 1.6f, line_fx::Lift(f.c, 0.7f), 0.95f * fade);
}

// Gravity: everything is drawn in first, then two thin rings go out with a swirl.
inline void DrawGravity(const Fx& f, float t) {
    const Rgb lit = line_fx::Lift(f.c, 0.40f);
    constexpr float kIn = 0.28f;
    if (t < kIn) {
        const float u = t / kIn;
        Ring(f.x, f.y, f.r * (0.55f - 0.40f * u * u), 1.8f, lit, 0.4f + 0.5f * u);
        return;
    }
    const float u = (t - kIn) / (1.0f - kIn);
    const float fade = std::pow(1.0f - u, 1.2f);
    Ring(f.x, f.y, f.r * (0.15f + 0.85f * EaseOut(u / 0.6f)), f.width * (1.0f - 0.4f * u), lit, 0.95f * fade);
    Ring(f.x, f.y, f.r * (0.12f + 0.75f * EaseOut(u / 0.8f)), 1.2f, f.c, 0.6f * fade);
    // Swirl: three arcs turning round the centre.
    const float sr = f.r * (0.20f + 0.35f * EaseOut(u));
    for (int i = 0; i < 3; ++i) {
        const float a0 = line_fx::kTwoPi * (float)i / 3.0f + u * 3.0f;
        line_fx::ArcLine(f.x, f.y, sr, a0, 1.1f, 1.6f, lit, 0.8f * fade);
    }
}

// Discharge: no solid ring; bolts fork out from the centre, a faint edge behind.
inline void DrawDischarge(const Fx& f, float t) {
    const float e = EaseOut(t / 0.35f);
    const float fade = 1.0f - t * t;
    uint32_t s = f.seed;
    const int n = std::clamp((int)(f.r / 16.0f), 4, 10);
    const uint32_t flick = line_fx::Hash(f.seed + (uint32_t)(f.age / 0.05f));
    for (int i = 0; i < n; ++i) {
        const float a = line_fx::kTwoPi * ((float)i + 0.6f * line_fx::Rnd(s)) / (float)n;
        const float d = f.r * (0.55f + 0.45f * line_fx::Rnd(s)) * e;
        const float ca = std::cos(a), sa = std::sin(a);
        line_fx::Bolt(f.x + ca * 5.0f, f.y + sa * 5.0f, f.x + ca * d, f.y + sa * d,
                      line_fx::Hash(flick + (uint32_t)i), 1.5f, f.c, fade, f.r >= 60.0f);
    }
    line_fx::Circle(f.x, f.y, f.r * e, 1.0f, f.c, 0.30f * fade);
    line_fx::Diamond(f.x, f.y, 5.0f * (1.0f - t), 1.4f, line_fx::Lift(f.c, 0.7f), fade);
}

// Stellar: an eight-point star outline (two four-point stars) opening out, a thin ring.
inline void DrawStellar(const Fx& f, float t) {
    const float e = EaseOut(t / 0.45f);
    const float fade = std::pow(1.0f - t, 1.2f);
    const Rgb lit = line_fx::Lift(f.c, 0.45f);
    const float rot = (float)(f.seed % 628u) * 0.01f + t * 0.5f;
    const float R = f.r * e;
    line_fx::Star4(f.x, f.y, R * 0.95f, R * 0.28f, rot, 4.0f, kUnder, 0.28f * fade);
    line_fx::Star4(f.x, f.y, R * 0.95f, R * 0.28f, rot, 2.0f, lit, 0.95f * fade);
    line_fx::Star4(f.x, f.y, R * 0.60f, R * 0.20f, rot + line_fx::kPi * 0.25f, 1.4f, f.c, 0.75f * fade);
    line_fx::Circle(f.x, f.y, R, 1.0f, f.c, 0.45f * fade);
}

// Crystal: a hexagon whose edges break away outward one by one, chips at the corners.
inline void DrawCrystal(const Fx& f, float t) {
    const float e = EaseOut(t / 0.30f);
    const float fade = std::pow(1.0f - t, 1.1f);
    const Rgb lit = line_fx::Lift(f.c, 0.45f);
    const float rot = (float)(f.seed % 628u) * 0.01f;
    const float R = f.r * (0.55f + 0.45f * e);
    float vx[6], vy[6];
    for (int i = 0; i < 6; ++i) {
        const float a = rot + line_fx::kTwoPi * (float)i / 6.0f;
        vx[i] = std::cos(a) * R; vy[i] = std::sin(a) * R;
    }
    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        const float mx = (vx[i] + vx[j]) * 0.5f, my = (vy[i] + vy[j]) * 0.5f;
        const float ml = std::max(1.0f, std::sqrt(mx * mx + my * my));
        const float push = f.r * 0.30f * t * (0.6f + 0.4f * (float)((i * 7) % 5) / 4.0f);
        const float ox = mx / ml * push, oy = my / ml * push;
        const float keep = 1.0f - 0.45f * t;   // each edge shortens as it flies off
        const float ax = f.x + ox + mx + (vx[i] - mx) * keep, ay = f.y + oy + my + (vy[i] - my) * keep;
        const float bx = f.x + ox + mx + (vx[j] - mx) * keep, by = f.y + oy + my + (vy[j] - my) * keep;
        Stroke(ax, ay, bx, by, f.width * (1.0f - 0.4f * t), lit, 0.95f * fade);
        line_fx::Polygon(f.x + vx[i] * (1.0f + 0.35f * t), f.y + vy[i] * (1.0f + 0.35f * t),
                         4.0f * (1.0f - 0.5f * t), 3, rot + t * 4.0f + (float)i, 1.2f, f.c, 0.8f * fade);
    }
    line_fx::Polygon(f.x, f.y, f.r * 0.30f * (1.0f - t), 6, rot, 1.2f, f.c, 0.6f * fade);
}

inline void DrawBlast(const Fx& f, float t) {
    switch (f.style) {
    case BlastStyle::Gravity:   DrawGravity(f, t); return;
    case BlastStyle::Discharge: DrawDischarge(f, t); return;
    case BlastStyle::Stellar:   DrawStellar(f, t); return;
    case BlastStyle::Crystal:   DrawCrystal(f, t); return;
    case BlastStyle::Impact:    DrawImpact(f, t); return;
    }
}

inline void DrawBolt(const Fx& f, float t, float x0, float y0) {
    // A new jag a few times a second; fades out over its life.
    const uint32_t seed = line_fx::Hash(f.seed + (uint32_t)(f.age / 0.06f));
    const float a = 1.0f - t * t;
    line_fx::Bolt(x0, y0, f.x, f.y, seed, f.width + 2.0f, kUnder, a * 0.35f);
    line_fx::Bolt(x0, y0, f.x, f.y, seed, f.width, f.c, a);
}

inline void DrawStar(const Fx& f, float t) {
    const float k = 0.25f * t + 0.75f * t * t;   // leaves quickly, speeds up onto the target
    const Rgb lit = line_fx::Lift(f.c, 0.45f);
    // Tail: the path just behind the star, thinning toward the ship.
    const float back = 0.30f;
    float px[9], py[9];
    for (int i = 0; i < 9; ++i) StarAt(f, std::max(0.0f, k - back * (float)(8 - i) / 8.0f), px[i], py[i]);
    for (int i = 0; i < 8; ++i) {
        const float a = 0.15f + 0.65f * (float)(i + 1) / 8.0f;
        Stroke(px[i], py[i], px[i + 1], py[i + 1], 0.8f + 1.0f * (float)i / 7.0f, f.c, a);
    }
    const float hx = px[8], hy = py[8];
    line_fx::Star4(hx, hy, 13.0f, 5.0f, f.age * 9.0f, 3.6f, kUnder, 0.30f);
    line_fx::Star4(hx, hy, 13.0f, 5.0f, f.age * 9.0f, 1.8f, lit, 1.0f);
}

inline void DrawVortex(const Fx& f, float t) {
    const float e = 1.0f - (1.0f - t) * (1.0f - t);
    const float in = std::min(1.0f, t * 6.0f);              // a quick fade in
    const float fade = in * (t < 0.7f ? 1.0f : (1.0f - t) / 0.3f);
    const Rgb lit = line_fx::Lift(f.c, 0.45f);
    const float R = f.r;
    const float spin = (float)(f.seed % 628u) * 0.01f + e * 2.4f;   // turning the way the arms wind
    const float core = R * 0.11f;
    // The pull's reach: a thin ring tightening a little.
    line_fx::Circle(f.x, f.y, R * (1.0f - 0.18f * e), 1.1f, f.c, 0.45f * fade);
    // Spiral arms: from the reach down into the disk, brighter toward the centre.
    constexpr int kArms = 4, kPts = 16;
    for (int arm = 0; arm < kArms; ++arm) {
        const float a0 = spin + line_fx::kTwoPi * (float)arm / (float)kArms;
        float px[kPts], py[kPts];
        for (int k = 0; k < kPts; ++k) {
            const float s = (float)k / (float)(kPts - 1);
            const float rr = core * 1.6f + (R * (1.0f - 0.25f * e) - core * 1.6f) * (1.0f - s) * (1.0f - s * 0.15f);
            const float a = a0 + s * 2.6f;   // about 150 degrees of wind from the reach inward
            px[k] = f.x + std::cos(a) * rr; py[k] = f.y + std::sin(a) * rr;
        }
        for (int k = 0; k + 1 < kPts; ++k) {
            const float s = (float)k / (float)(kPts - 1);
            line_fx::Line(px[k], py[k], px[k + 1], py[k + 1], 1.0f + 1.2f * s, lit, (0.20f + 0.75f * s) * fade);
        }
    }
    // Accretion disk: a tilted ellipse, bright on the side turning toward us.
    const float tilt = (float)((f.seed >> 10) % 314u) * 0.01f;
    const float dr = R * 0.30f, flat = 0.36f;
    constexpr int kDisk = 40;
    float dx[kDisk + 1], dy[kDisk + 1];
    for (int k = 0; k <= kDisk; ++k) {
        const float a = line_fx::kTwoPi * (float)k / (float)kDisk;
        const float ex = std::cos(a) * dr, ey = std::sin(a) * dr * flat;
        dx[k] = f.x + ex * std::cos(tilt) - ey * std::sin(tilt);
        dy[k] = f.y + ex * std::sin(tilt) + ey * std::cos(tilt);
    }
    for (int k = 0; k < kDisk; ++k) {
        const float a = line_fx::kTwoPi * ((float)k + 0.5f) / (float)kDisk;
        const float beam = 0.35f + 0.65f * (0.5f + 0.5f * std::cos(a - spin * 1.5f));
        line_fx::Line(dx[k], dy[k], dx[k + 1], dy[k + 1], 2.4f, kUnder, 0.35f * fade);
        line_fx::Line(dx[k], dy[k], dx[k + 1], dy[k + 1], 1.6f, beam > 0.8f ? line_fx::Lift(f.c, 0.75f) : lit,
                      beam * fade);
    }
    // Event horizon: a dark thick ring under a thin bright rim.
    line_fx::Circle(f.x, f.y, core, 4.0f, kUnder, 0.85f * fade);
    line_fx::Circle(f.x, f.y, core + 2.0f, 1.2f, line_fx::Lift(f.c, 0.6f), 0.9f * fade);
}

inline void DrawSweep(const Fx& f, float t) {
    const float a0 = -line_fx::kPi * 0.5f + line_fx::kTwoPi * t;
    const float fade = t < 0.9f ? 1.0f : (1.0f - t) / 0.1f;
    const Rgb lit = line_fx::Lift(f.c, 0.45f);
    for (int i = 0; i < 7; ++i) {   // the line, then a fan of fainter ones behind it
        const float a = a0 - 0.07f * (float)i;
        const float ca = std::cos(a), sa = std::sin(a);
        const float al = (i == 0 ? 0.95f : 0.45f * (1.0f - (float)i / 7.0f)) * fade;
        if (i == 0) Stroke(f.x + ca * 22.0f, f.y + sa * 22.0f, f.x + ca * f.r, f.y + sa * f.r, f.width, lit, al);
        else line_fx::Line(f.x + ca * 22.0f, f.y + sa * 22.0f, f.x + ca * f.r, f.y + sa * f.r, 1.0f, f.c, al);
    }
}

inline void DrawImplode(const Fx& f, float t) {
    const float e = t * t;                       // slow start, snaps shut
    const float fade = t < 0.85f ? 1.0f : (1.0f - t) / 0.15f;
    const Rgb lit = line_fx::Lift(f.c, 0.40f);
    const float rr = f.r * (1.0f - e);
    Ring(f.x, f.y, std::max(1.0f, rr), f.width, lit, 0.9f * fade * (0.4f + 0.6f * t));
    // Shards riding in just outside the ring, pointing at the centre.
    uint32_t s = f.seed;
    const int n = std::clamp((int)(f.r / 18.0f), 6, 14);
    for (int i = 0; i < n; ++i) {
        const float ang = line_fx::kTwoPi * ((float)i + 0.5f * line_fx::Rnd(s)) / (float)n;
        const float r0 = rr + f.r * (0.06f + 0.10f * line_fx::Rnd(s));
        const float len = std::min(r0, f.r * 0.12f);
        const float ca = std::cos(ang), sa = std::sin(ang);
        Stroke(f.x + ca * r0, f.y + sa * r0, f.x + ca * (r0 - len), f.y + sa * (r0 - len),
               1.5f, lit, 0.85f * fade);
    }
    // Core: a diamond growing as it all arrives.
    line_fx::Diamond(f.x, f.y, 4.0f + 10.0f * e, 1.6f, line_fx::Lift(f.c, 0.7f), 0.9f * fade * t);
}

inline void DrawNova(const Fx& f, float t) {
    const float e = 1.0f - (1.0f - t) * (1.0f - t);
    const float fade = 1.0f - t;
    const Rgb lit = line_fx::Lift(f.c, 0.35f);
    Ring(f.x, f.y, 40.0f + f.r * e, f.width * (1.0f - 0.5f * t), lit, 0.9f * fade);
    const float e2 = std::max(0.0f, (t - 0.12f) / 0.88f);
    Ring(f.x, f.y, 30.0f + f.r * 0.80f * (1.0f - (1.0f - e2) * (1.0f - e2)), 1.5f, f.c, 0.6f * fade);
    // Eight bright marks riding the front ring.
    const float rr = 40.0f + f.r * e;
    for (int i = 0; i < 8; ++i) {
        const float ang = line_fx::kTwoPi * (float)i / 8.0f + 0.2f;
        const float ca = std::cos(ang), sa = std::sin(ang);
        Stroke(f.x + ca * (rr - 16.0f), f.y + sa * (rr - 16.0f), f.x + ca * (rr + 10.0f),
               f.y + sa * (rr + 10.0f), 2.0f, line_fx::Lift(f.c, 0.7f), 0.95f * fade);
    }
}

inline void Draw() {
    for (const Fx& f : g_Fx) {
        if (f.delay > 0.0f) continue;
        const float t = std::clamp(f.age / f.life, 0.0f, 1.0f);
        switch (f.kind) {
        case Kind::Blast:   DrawBlast(f, t); break;
        case Kind::Bolt:
            if (f.fromShip) {   // from the ship's edge toward the target, as the ship moves
                const float dx = f.x - g_ShipX, dy = f.y - g_ShipY;
                const float d = std::sqrt(dx * dx + dy * dy);
                const float edge = d > 1.0f ? std::min(22.0f, d * 0.5f) / d : 0.0f;
                DrawBolt(f, t, g_ShipX + dx * edge, g_ShipY + dy * edge);
            } else {
                DrawBolt(f, t, f.x0, f.y0);
            }
            break;
        case Kind::Star:    DrawStar(f, t); break;
        case Kind::Nova:    DrawNova(f, t); break;
        case Kind::Implode: DrawImplode(f, t); break;
        case Kind::Sweep:   DrawSweep(f, t); break;
        case Kind::Vortex:  DrawVortex(f, t); break;
        }
    }
}

}  // namespace strike_fx
