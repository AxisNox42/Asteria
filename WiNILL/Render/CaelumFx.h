#pragma once
// ERIDANUS (code CAELUM)'s own blast effects, in the language of its body
// (CaelumDraw.h DrawBody): a faint ring with four crop-mark arcs bent onto it,
// each with short arms turned inward. The ring is torn like its corruption:
// cut into arcs that each slip in or out a little, joined by short radial
// steps, re-rolled many times a second. No shared shockwave pool: only
// ERIDANUS spawns these. Lines only (no fills, no dashes), pale amber, world space.
//
//   Open   the torn ring spreads out to radius R (spout, phase, fall)
//   Close  the ring snaps in from R to the core (plug, swallow: it drinks)
#include <algorithm>
#include <cmath>
#include <vector>

#include "LineFx.h"

namespace caelum_fx {

using line_fx::Rgb;

inline constexpr Rgb kAmber{ 1.00f, 0.82f, 0.50f };   // the boss's colour
inline constexpr Rgb kHot{ 1.00f, 0.95f, 0.82f };     // near-white, danger
inline constexpr Rgb kUnder{ 0.10f, 0.06f, 0.02f };   // dark under-stroke (bright wallpapers)
inline constexpr int kTears = 18;                      // arcs the ring is torn into

enum class Kind { Open, Close };

struct Fx {
    Kind kind = Kind::Open;
    float x = 0.0f, y = 0.0f;
    float size = 0.0f;   // radius at full reach
    float tear = 0.0f;   // 0..1 how far the strips slip
    float delay = 0.0f, age = 0.0f, life = 0.5f;
    Rgb c = kAmber;
    uint32_t seed = 0;
};

inline std::vector<Fx> g_Fx;
inline constexpr size_t kMaxFx = 64;
inline uint32_t g_Next = 1;

inline void Push(Fx f) {
    if (g_Fx.size() >= kMaxFx) return;
    f.seed = line_fx::Hash(g_Next++ * 2654435761u);
    g_Fx.push_back(f);
}
// The torn ring spreading out to radius R.
inline void Open(float x, float y, float R, float life, Rgb c, float tear, float delay = 0.0f) {
    Fx f; f.kind = Kind::Open; f.x = x; f.y = y; f.size = R; f.life = life;
    f.c = c; f.tear = tear; f.delay = delay;
    Push(f);
}
// The ring snapping in from radius R to the core.
inline void Close(float x, float y, float R, float life, Rgb c, float tear) {
    Fx f; f.kind = Kind::Close; f.x = x; f.y = y; f.size = R; f.life = life;
    f.c = c; f.tear = tear;
    Push(f);
}

inline void Tick(float dt) {
    if (g_Fx.empty()) return;
    for (Fx& f : g_Fx) f.age += dt;
    g_Fx.erase(std::remove_if(g_Fx.begin(), g_Fx.end(),
        [](const Fx& f) { return f.age >= f.delay + f.life; }), g_Fx.end());
}
inline void Clear() { g_Fx.clear(); }
inline int Count() { return (int)g_Fx.size(); }

// The torn ring at radius r: kTears arcs, each slipped in or out by up to
// `slip` px, joined by radial steps (one unbroken line); four bright crop-mark
// arcs ride just outside, turned `spin`, with arms pointing in at each end.
inline void DrawTornRing(float x, float y, float r, float slip, float spin, uint32_t seed,
                         float w, Rgb c, float a) {
    if (r < 2.0f || a <= 0.0f) return;
    float off[kTears];
    for (int i = 0; i < kTears; ++i)
        off[i] = (line_fx::Rnd(seed) * 2.0f - 1.0f) * slip;
    const float step = line_fx::kTwoPi / (float)kTears;
    const Rgb lit = line_fx::Lift(c, 0.45f);
    const float arm = 0.42f;                 // half-sweep of a crop arc (rad)
    const float RB = r + 4.0f + w + slip;    // crop arcs sit outside the tears
    for (int pass = 0; pass < 2; ++pass) {
        const bool under = pass == 0;
        const Rgb col = under ? kUnder : c;
        const float lw = under ? w + 2.0f : w;
        const float al = under ? a * 0.30f : a * 0.70f;
        for (int i = 0; i < kTears; ++i) {
            const float a0 = spin + step * (float)i;
            const float ri = r + off[i];
            line_fx::ArcLine(x, y, ri, a0, step, lw, col, al);
            // Step across to the next arc's radius.
            const float a1 = a0 + step, rn = r + off[(i + 1) % kTears];
            line_fx::Line(x + std::cos(a1) * ri, y + std::sin(a1) * ri,
                          x + std::cos(a1) * rn, y + std::sin(a1) * rn, lw, col, al);
        }
        // Crop marks, as on the body: an arc and two inward arms.
        const Rgb cc = under ? kUnder : lit;
        const float cw = under ? w + 3.5f : w + 1.5f;
        const float ca = under ? a * 0.30f : a;
        const float inR = RB - std::max(8.0f, RB * 0.14f);
        for (int k = 0; k < 4; ++k) {
            const float m = spin + line_fx::kPi * 0.25f + line_fx::kPi * 0.5f * (float)k;
            line_fx::ArcLine(x, y, RB, m - arm, arm * 2.0f, cw, cc, ca);
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                const float e = m + (float)sgn * arm;
                line_fx::Line(x + std::cos(e) * RB, y + std::sin(e) * RB,
                              x + std::cos(e) * inR, y + std::sin(e) * inR, cw, cc, ca);
            }
        }
    }
}

inline void Draw() {
    for (const Fx& f : g_Fx) {
        if (f.age < f.delay) continue;
        const float t = std::clamp((f.age - f.delay) / f.life, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        // The glitch re-rolls about 20 times a second.
        const uint32_t seed = line_fx::Hash(f.seed + (uint32_t)((f.age - f.delay) * 20.0f));
        if (f.kind == Kind::Open) {
            // Spreads from a fifth of its reach, turning a little; the tears are
            // widest at the start and settle as it fades. It stops on R.
            const float r = f.size * (0.2f + 0.8f * ease);
            const float a = std::pow(1.0f - t, 1.2f);
            const float slip = f.tear * (6.0f + r * 0.05f) * (1.0f - 0.7f * t);
            DrawTornRing(f.x, f.y, r, slip, 0.30f * ease, seed, 2.6f - 1.6f * t, f.c, a);
        } else {
            // Snaps in to the core, spinning shut; it tears harder as it closes.
            const float r = f.size * (1.0f - 0.85f * ease);
            const float a = std::min(1.0f, t * 8.0f) * (1.0f - t * t);
            const float slip = f.tear * (4.0f + f.size * 0.03f) * (0.4f + 0.6f * t);
            DrawTornRing(f.x, f.y, r, slip, -0.9f * ease, seed, 2.2f, f.c, a);
        }
    }
}

}  // namespace caelum_fx
