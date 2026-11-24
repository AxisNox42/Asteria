#pragma once
// ─────────────────────────────────────────────────────────────
// How a fired Regulus laser looks (all Regulus lasers, 2026-11-23): a white
// core with rainbow fringes flashes for kFlash, then it is wiped from its tail
// (start) toward its head over kErase.
// Kept free of game headers so tests can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>

namespace regulus_look {

inline constexpr float kFlash = 0.06f;
inline constexpr float kErase = 0.20f;
inline constexpr float kFringeHue = 0.17f;   // turns either side of the beam colour
inline constexpr float kLife = kFlash + kErase;

// A colour turned by `hue` turns (r, g, b in 0-1, in place).
inline void ShiftHue(float& r, float& g, float& b, float hue) {
    const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    const float d = mx - mn;
    float h = 0.0f;
    if (d > 1e-6f) {
        if (mx == r) h = std::fmod((g - b) / d + 6.0f, 6.0f);
        else if (mx == g) h = (b - r) / d + 2.0f;
        else h = (r - g) / d + 4.0f;
        h /= 6.0f;
    }
    const float s = mx > 1e-6f ? d / mx : 0.0f, v = mx;
    h = std::fmod(h + hue + 10.0f, 1.0f) * 6.0f;
    const int i = (int)h % 6;
    const float f = h - std::floor(h);
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    switch (i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
}

}  // namespace regulus_look
