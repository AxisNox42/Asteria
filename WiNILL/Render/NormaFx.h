#pragma once
// OCTANS's own blast effects, in the language of its body (EntityDraw.cpp
// DrawNormaBody): two square frames crossed into an eight-point seal, each
// edge stopping short of its corners, with a thin inset contour. No shared
// shockwave pool: every effect here belongs to OCTANS and only OCTANS spawns
// them. Lines only (no fills, no dashes), sharp corners, world space.
//
//   Seal     the seal opens out to R (bursts, star out, phase, death, impacts)
//   Implode  the seal folds in from R to a point (slipping into a pane)
//   Wave     a thin ring riding with a flood wave of shots, eight bright marks
//   Frame    a pane's rectangle pushing out from its border, corner brackets
//            flying off (a pane bursting open or blowing shut)
//   Blast    a worn-out pane blowing up: octagon blast rings racing out,
//            rays thrown ahead of them, a hot core that burns down
#include <algorithm>
#include <cmath>
#include <vector>

#include "LineFx.h"

namespace norma_fx {

using line_fx::Rgb;

inline constexpr Rgb kIce{ 0.72f, 0.94f, 1.0f };     // the boss's colour
inline constexpr Rgb kWhite{ 0.92f, 0.99f, 1.0f };
inline constexpr Rgb kRed{ 1.0f, 0.36f, 0.30f };     // pane control (wall, ram)
inline constexpr Rgb kLilac{ 0.86f, 0.80f, 1.0f };   // a pane closing on whatever is inside
inline constexpr Rgb kUnder{ 0.04f, 0.07f, 0.11f };  // dark under-stroke (bright wallpapers)

enum class Kind { Seal, Implode, Wave, Frame, Blast };

struct Fx {
    Kind kind = Kind::Seal;
    float x = 0.0f, y = 0.0f;     // centre (Frame: min corner)
    float x1 = 0.0f, y1 = 0.0f;   // Frame: max corner
    float size = 0.0f;            // Seal/Implode: reach; Wave: speed; Frame: push-out
    float rot = 0.0f;
    float delay = 0.0f, age = 0.0f, life = 0.5f;
    Rgb c = kIce;
};

inline std::vector<Fx> g_Fx;
inline constexpr size_t kMaxFx = 96;

inline void Push(const Fx& f) {
    if (g_Fx.size() < kMaxFx) g_Fx.push_back(f);
}
// Every seal starts on the same turn (a point straight up), so seals that
// overlap (a phase shift, the death) nest into one echo instead of crossing.
inline constexpr float kSealTurn = -line_fx::kPi * 0.5f;

// The seal opening out to reach R over `life` seconds, after `delay`.
inline void Seal(float x, float y, float R, float life, Rgb c, float delay = 0.0f) {
    Fx f; f.kind = Kind::Seal; f.x = x; f.y = y; f.size = R; f.life = life; f.delay = delay;
    f.c = c; f.rot = kSealTurn;
    Push(f);
}
// The seal folding in from R (slipping into a pane).
inline void Implode(float x, float y, float R, float life, Rgb c) {
    Fx f; f.kind = Kind::Implode; f.x = x; f.y = y; f.size = R; f.life = life;
    f.c = c; f.rot = kSealTurn;
    Push(f);
}
// A ring at the front of a flood wave whose shots fly at `speed` px/s.
inline void Wave(float x, float y, float speed, float life) {
    Fx f; f.kind = Kind::Wave; f.x = x; f.y = y; f.size = speed; f.life = life;
    f.c = kIce; f.rot = kSealTurn;
    Push(f);
}
// A pane rectangle (world corners) pushing out by `grow` px.
inline void Frame(float x0, float y0, float x1, float y1, float grow, float life, Rgb c) {
    Fx f; f.kind = Kind::Frame;
    f.x = std::min(x0, x1); f.y = std::min(y0, y1);
    f.x1 = std::max(x0, x1); f.y1 = std::max(y0, y1);
    f.size = grow; f.life = life; f.c = c;
    Push(f);
}

// An explosion at (x, y) out to R (a worn-out pane self-destructing).
inline void Blast(float x, float y, float R, float life, Rgb c) {
    Fx f; f.kind = Kind::Blast; f.x = x; f.y = y; f.size = R; f.life = life; f.c = c;
    f.rot = kSealTurn + line_fx::kPi / 8.0f;   // a flat top, like the seal's frames
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

// A square frame (circumradius R, a corner at ang) whose edges stop `gap` short
// of each corner, as on the body.
inline void GapSquare(float x, float y, float R, float ang, float gap, float w, Rgb c, float a) {
    float vx[4], vy[4];
    for (int k = 0; k < 4; ++k) {
        const float t = ang + line_fx::kPi * 0.5f * (float)k;
        vx[k] = x + std::cos(t) * R; vy[k] = y + std::sin(t) * R;
    }
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        const float ax = vx[i] + (vx[j] - vx[i]) * gap, ay = vy[i] + (vy[j] - vy[i]) * gap;
        const float bx = vx[j] + (vx[i] - vx[j]) * gap, by = vy[j] + (vy[i] - vy[j]) * gap;
        line_fx::Line(ax, ay, bx, by, w, c, a);
    }
}

// The eight-point seal at reach r: frame A, frame B turned 45 degrees, A's inset.
inline void DrawSealAt(float x, float y, float r, float rotA, float rotB, float w, Rgb c, float a) {
    const Rgb lit = line_fx::Lift(c, 0.35f);
    GapSquare(x, y, r, rotA, 0.06f, w + 2.0f, kUnder, a * 0.30f);
    GapSquare(x, y, r, rotB, 0.06f, w + 2.0f, kUnder, a * 0.30f);
    GapSquare(x, y, r, rotA, 0.06f, w, lit, a);
    GapSquare(x, y, r, rotB, 0.06f, w, c, a * 0.85f);
    GapSquare(x, y, r * 0.84f, rotA, 0.10f, std::max(0.8f, w * 0.4f), c, a * 0.5f);
}

inline void Draw() {
    for (const Fx& f : g_Fx) {
        if (f.age < f.delay) continue;
        const float t = std::clamp((f.age - f.delay) / f.life, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);   // fast out, settling
        switch (f.kind) {
        case Kind::Seal: {
            // Opens from a quarter of its reach without turning, so seals that
            // overlap stay nested; the outer points stop exactly on R.
            const float r = f.size * (0.22f + 0.78f * ease);
            const float a = std::pow(1.0f - t, 1.3f);
            const float w = 3.2f - 2.2f * t;
            DrawSealAt(f.x, f.y, r, f.rot, f.rot + line_fx::kPi * 0.25f, w, f.c, a);
            break;
        }
        case Kind::Implode: {
            // Comes in fast, folds to a point while turning shut.
            const float r = f.size * (1.0f - 0.85f * ease);
            const float a = std::min(1.0f, t * 8.0f) * (1.0f - t);
            const float turn = 0.6f * ease;
            DrawSealAt(f.x, f.y, r, f.rot - turn, f.rot + line_fx::kPi * 0.25f - turn, 2.4f, f.c, a);
            break;
        }
        case Kind::Wave: {
            // Rides with the shots (radius = speed * time), fading as it spreads.
            const float r = f.size * (f.age - f.delay);
            const float a = 0.55f * (1.0f - t);
            line_fx::Circle(f.x, f.y, r, 1.4f, f.c, a);
            for (int k = 0; k < 8; ++k)
                line_fx::ArcLine(f.x, f.y, r, f.rot + line_fx::kPi * 0.25f * (float)k - 0.05f, 0.10f,
                                 3.0f, line_fx::Lift(f.c, 0.4f), a * 1.4f);
            break;
        }
        case Kind::Frame: {
            // The border pushes out and thins; the corner brackets run ahead of it.
            const float g = f.size * ease;
            const float a = 1.0f - t;
            const float w = 3.0f - 2.0f * t;
            const float x0 = f.x - g, y0 = f.y - g, x1 = f.x1 + g, y1 = f.y1 + g;
            const float under = w + 2.0f;
            line_fx::Line(x0, y0, x1, y0, under, kUnder, a * 0.3f);
            line_fx::Line(x1, y0, x1, y1, under, kUnder, a * 0.3f);
            line_fx::Line(x1, y1, x0, y1, under, kUnder, a * 0.3f);
            line_fx::Line(x0, y1, x0, y0, under, kUnder, a * 0.3f);
            line_fx::Line(x0, y0, x1, y0, w, f.c, a * 0.8f);
            line_fx::Line(x1, y0, x1, y1, w, f.c, a * 0.8f);
            line_fx::Line(x1, y1, x0, y1, w, f.c, a * 0.8f);
            line_fx::Line(x0, y1, x0, y0, w, f.c, a * 0.8f);
            const float b = f.size * 0.6f * ease;   // brackets fly further than the border
            const float arm = std::min(f.x1 - f.x, f.y1 - f.y) * 0.18f + 6.0f;
            const Rgb lit = line_fx::Lift(f.c, 0.4f);
            const float cx[4] = { x0 - b, x1 + b, x1 + b, x0 - b };
            const float cy[4] = { y0 - b, y0 - b, y1 + b, y1 + b };
            const float sx[4] = { 1.0f, -1.0f, -1.0f, 1.0f };
            const float sy[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
            for (int k = 0; k < 4; ++k) {
                line_fx::Line(cx[k], cy[k], cx[k] + sx[k] * arm, cy[k], w + 1.0f, lit, a);
                line_fx::Line(cx[k], cy[k], cx[k], cy[k] + sy[k] * arm, w + 1.0f, lit, a);
            }
            break;
        }
        case Kind::Blast: {
            // Two octagon rings race out (the second a beat behind), sixteen
            // rays run ahead of the first, and a hot octagon core burns down.
            const float R = f.size;
            const float a = std::pow(1.0f - t, 1.1f);
            const Rgb lit = line_fx::Lift(f.c, 0.5f);
            const float e1 = 1.0f - std::pow(1.0f - std::min(1.0f, t / 0.55f), 3.0f);
            const float t2 = std::clamp((t - 0.10f) / 0.75f, 0.0f, 1.0f);
            const float e2 = 1.0f - std::pow(1.0f - t2, 3.0f);
            const float r1 = R * (0.12f + 0.88f * e1), r2 = R * 0.78f * (0.10f + 0.90f * e2);
            const float w1 = 5.0f - 3.6f * t;
            line_fx::Polygon(f.x, f.y, r1, 8, f.rot, w1 + 2.5f, kUnder, a * 0.30f);
            line_fx::Polygon(f.x, f.y, r1, 8, f.rot, w1, kWhite, a);
            if (t2 > 0.0f) {
                line_fx::Polygon(f.x, f.y, r2, 8, f.rot + line_fx::kPi / 8.0f, 2.6f + 2.0f, kUnder, a * 0.25f);
                line_fx::Polygon(f.x, f.y, r2, 8, f.rot + line_fx::kPi / 8.0f, 2.6f, lit, a * (1.0f - t2 * 0.5f));
            }
            for (int k = 0; k < 16; ++k) {
                const float ang = f.rot + line_fx::kPi * 2.0f * (float)k / 16.0f;
                const float ca = std::cos(ang), sa = std::sin(ang);
                const float lenK = (k % 2 == 0) ? 0.34f : 0.20f;
                const float r0 = r1 * 0.92f + R * 0.04f, r3 = r0 + R * lenK * (1.0f - t);
                line_fx::Line(f.x + ca * r0, f.y + sa * r0, f.x + ca * r3, f.y + sa * r3, 4.0f, kUnder, a * 0.3f);
                line_fx::Line(f.x + ca * r0, f.y + sa * r0, f.x + ca * r3, f.y + sa * r3, 2.0f,
                              (k % 2 == 0) ? lit : f.c, a);
            }
            const float core = R * 0.22f * (1.0f - t) * (0.6f + 0.4f * std::min(1.0f, t * 6.0f));
            if (core > 2.0f) {
                line_fx::Polygon(f.x, f.y, core, 8, f.rot, 2.4f, kWhite, a);
                line_fx::Polygon(f.x, f.y, core * 0.55f, 8, f.rot + line_fx::kPi / 8.0f, 1.6f, lit, a);
            }
            break;
        }
        }
    }
}

}  // namespace norma_fx
