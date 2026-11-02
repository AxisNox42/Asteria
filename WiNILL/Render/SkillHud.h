#pragma once
// ─────────────────────────────────────────────────────────────
// Skill HUD (bottom centre): four keys (dash + three skill slots) as circles
// with a line icon.
//   cooldown = clockwise wedge filling the circle from 12 o'clock, the same
//   reading as the wave track's current node
// Icons are plain line pictograms (no dots), one per skill group. Text is
// only the key letter and the seconds left on a cooldown.
// State comes from SkillHudModel.h (BuildSkillHudView).
// ─────────────────────────────────────────────────────────────
#include <cmath>
#include <cstdio>

#include "DrawPrim.h"
#include "HudLines.h"
#include "SceneUI.h"
#include "Settings.h"
#include "SkillHudModel.h"
#include "TextRenderer.h"
#include "UiLayout.h"

extern TextRenderer g_TextL;
extern TextRenderer g_TextS;

inline skill_hud::SkillHudPulse g_SkillHudPulse;

namespace skill_hud {

using hud_lines::Rgb;

inline Rgb GlyphColor(SkillGlyph g) {
    switch (g) {
    case SkillGlyph::Dash:       return { 0.40f, 1.00f, 1.00f };
    case SkillGlyph::Projectile: return { 1.00f, 0.80f, 0.45f };
    case SkillGlyph::Area:       return { 1.00f, 0.55f, 0.35f };
    case SkillGlyph::Install:    return { 0.85f, 0.75f, 0.55f };
    case SkillGlyph::Summon:     return { 0.70f, 0.60f, 1.00f };
    case SkillGlyph::Move:       return { 0.55f, 0.90f, 1.00f };
    case SkillGlyph::Heal:       return { 0.55f, 1.00f, 0.60f };
    case SkillGlyph::Link:       return { 0.85f, 0.75f, 1.00f };
    case SkillGlyph::Channel:    return { 0.65f, 1.00f, 0.85f };
    case SkillGlyph::Random:     return { 1.00f, 0.85f, 0.40f };
    case SkillGlyph::Form:       return { 1.00f, 0.92f, 0.65f };
    }
    return { 0.36f, 0.41f, 0.53f };
}
inline constexpr Rgb kEmptyCol = { 0.36f, 0.41f, 0.53f };
inline constexpr Rgb kKeyText = { 0.86f, 0.91f, 1.00f };

// One stroke style, drawn in two passes so every keyline sits under every
// bright stroke of the glyph (no keyline crossing a joint).
struct Pen {
    Rgb c;
    float a, w, keyline;
    bool keyPass;

    Rgb Col() const { return keyPass ? hud_lines::KeyCol(c) : c; }
    float Alpha() const { return keyPass ? hud_lines::kKeyA * a : a; }
    float Width() const { return keyPass ? w + keyline : w; }

    void Line(float x0, float y0, float x1, float y1) const {
        // Extend by half the stroke so joints close without gaps.
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;
        const float ex = dx / len * Width() * 0.5f, ey = dy / len * Width() * 0.5f;
        hud_lines::Segment(x0 - ex, y0 - ey, x1 + ex, y1 + ey, Width(), Col(), Alpha());
    }
    void Circle(float cx, float cy, float r, float alphaMul = 1.0f, float wMul = 1.0f) const {
        const float wv = keyPass ? w * wMul + keyline : w * wMul;
        hud_lines::Arc(cx, cy, r, wv, 0.0f, 2.0f * hud_lines::kPi, Col(), Alpha() * alphaMul);
    }
};

// Two chevrons centred on (x, y).
inline void GlyphDash(const Pen& p, float x, float y, float r) {
    const float pitch = 0.56f, half = 0.26f;
    const int n = 2;
    for (int k = 0; k < n; ++k) {
        const float c = (k - (n - 1) * 0.5f) * pitch;
        const float backX = x + (c - half) * r, tipX = x + (c + half) * r;
        p.Line(backX, y - 0.55f * r, tipX, y);
        p.Line(tipX, y, backX, y + 0.55f * r);
    }
}

inline void GlyphPoly(const Pen& p, float x, float y, float r, int n, float rot) {
    for (int k = 0; k < n; ++k) {
        const float a0 = rot + 2.0f * hud_lines::kPi * k / n;
        const float a1 = rot + 2.0f * hud_lines::kPi * (k + 1) / n;
        p.Line(x + std::cos(a0) * r, y + std::sin(a0) * r,
               x + std::cos(a1) * r, y + std::sin(a1) * r);
    }
}

inline void DrawSkillGlyph(Pen p, SkillGlyph g, float x, float y, float r) {
    using hud_lines::kPi;
    for (int pass = p.keyline > 0.0f ? 0 : 1; pass < 2; ++pass) {
        p.keyPass = pass == 0;
        switch (g) {
        case SkillGlyph::Dash: GlyphDash(p, x, y, r); break;
        case SkillGlyph::Projectile:          // an arrow
            p.Line(x - 0.8f * r, y + 0.8f * r, x + 0.8f * r, y - 0.8f * r);
            p.Line(x + 0.8f * r, y - 0.8f * r, x + 0.2f * r, y - 0.8f * r);
            p.Line(x + 0.8f * r, y - 0.8f * r, x + 0.8f * r, y - 0.2f * r);
            break;
        case SkillGlyph::Area:                // a ring with rays
            p.Circle(x, y, 0.40f * r);
            for (int k = 0; k < 4; ++k) {
                const float t = kPi * (0.25f + 0.5f * k);
                const float c = std::cos(t), s = std::sin(t);
                p.Line(x + 0.62f * r * c, y + 0.62f * r * s, x + 0.98f * r * c, y + 0.98f * r * s);
            }
            break;
        case SkillGlyph::Install:             // a square on a base line
            GlyphPoly(p, x, y - 0.1f * r, 0.6f * r, 4, kPi * 0.25f);
            p.Line(x - 0.9f * r, y + 0.75f * r, x + 0.9f * r, y + 0.75f * r);
            break;
        case SkillGlyph::Summon:              // a figure inside a ring
            p.Circle(x, y, 0.85f * r, 1.0f, 0.8f);
            p.Circle(x, y - 0.25f * r, 0.22f * r);
            p.Line(x - 0.4f * r, y + 0.5f * r, x + 0.4f * r, y + 0.5f * r);
            break;
        case SkillGlyph::Move:                // a jump arc with an arrow head
            hud_lines::Arc(x, y + 0.3f * r, 0.75f * r, p.Width(), kPi, kPi, p.Col(), p.Alpha());
            p.Line(x + 0.75f * r, y + 0.3f * r, x + 0.45f * r, y - 0.05f * r);
            p.Line(x + 0.75f * r, y + 0.3f * r, x + 1.0f * r, y - 0.05f * r);
            break;
        case SkillGlyph::Heal:                // a cross
            p.Line(x, y - 0.75f * r, x, y + 0.75f * r);
            p.Line(x - 0.75f * r, y, x + 0.75f * r, y);
            break;
        case SkillGlyph::Link:                // two linked rings
            p.Circle(x - 0.32f * r, y, 0.45f * r);
            p.Circle(x + 0.32f * r, y, 0.45f * r);
            break;
        case SkillGlyph::Channel:             // nested rings
            p.Circle(x, y, 0.80f * r, 1.0f, 0.8f);
            p.Circle(x, y, 0.45f * r);
            p.Circle(x, y, 0.12f * r);
            break;
        case SkillGlyph::Random:              // a die face (diamond)
            GlyphPoly(p, x, y, 0.85f * r, 4, 0.0f);
            p.Circle(x, y, 0.15f * r);
            break;
        case SkillGlyph::Form:                // a half-filled moon: sun and night
            p.Circle(x, y, 0.75f * r);
            p.Line(x, y - 0.75f * r, x, y + 0.75f * r);
            break;
        }
    }
}

inline float PulseT(float pulse) {
    return pulse > 0.0f ? 1.0f - pulse / kPulseDur : 1.0f;
}

// Cooldown / charge reads by brightness, not hue (a same-hue pale wedge
// vanished on busy wallpapers): a faint track, a dark wedge for the elapsed
// part (shows on bright wallpapers), and a bright progress line with a hand
// at its leading edge (shows on dark ones).
inline constexpr float kTrackA = 0.30f;
inline constexpr float kWedgeA = 0.55f;

inline void DrawCooldownCircle(float cx, float cy, float r, float frac, Rgb col,
                               float hs, float keyline) {
    using hud_lines::kPi;
    const float a0 = -0.5f * kPi, sweep = 2.0f * kPi * frac;
    const float w = 2.0f * hs;
    hud_lines::Ring(cx, cy, r, 1.0f * hs, col, kTrackA, keyline);
    hud_lines::SectorFill(cx, cy, r - 1.5f * hs, hud_lines::FillShape::Circle, frac,
                          hud_lines::KeyCol(col), kWedgeA);
    if (keyline > 0.0f)
        hud_lines::Arc(cx, cy, r, w + keyline, a0, sweep, hud_lines::KeyCol(col),
                       hud_lines::kKeyA);
    hud_lines::Arc(cx, cy, r, w, a0, sweep, col, 1.0f);
    const float t = a0 + sweep;
    const float hx[2] = { cx + 0.35f * r * std::cos(t), cx + (r - 1.0f * hs) * std::cos(t) };
    const float hy[2] = { cy + 0.35f * r * std::sin(t), cy + (r - 1.0f * hs) * std::sin(t) };
    hud_lines::Polyline(hx, hy, 2, 1.4f * hs, col, 0.95f, keyline);
}

}  // namespace skill_hud

inline void DrawSkillHud(const skill_hud::SkillHudView& view, float hudScale,
                         float sw, float sh) {
    using namespace skill_hud;
    const float hs = hudScale;
    const float keyline = 3.2f * hs * CombatKeylineStrength();
    const float radius = 30.0f * hs, gap = 10.0f * hs, pitch = 2.0f * radius + gap;
    const float rowW = 4.0f * 2.0f * radius + 3.0f * gap;
    const float x0 = sw * 0.5f - rowW * 0.5f;          // left edge of the key row
    const float top = HudY(sh, 2.0f * radius + Hud::SKILL_KEYS_Y * hs);
    const float cy = top + radius;
    const float keySc = UiTextScale(g_TextS, UiTextLevel::Supporting, hs);
    const float secSc = UiTextScale(g_TextL, UiTextLevel::Description, hs);
    const float keyH = g_TextS.Height(L"SHIFT", keySc);
    const float keyTop = top - 5.0f * hs - keyH;
    auto keyX = [&](int i) { return x0 + radius + i * pitch; };

    // ── shapes ──
    BindMainShader();
    for (int i = 0; i < 4; ++i) {
        const SkillSlotView& s = view.slots[i];
        const float cx = keyX(i);
        if (s.empty) {
            hud_lines::Ring(cx, cy, radius, 1.0f * hs, kEmptyCol, 0.35f, keyline);
            for (int pass = keyline > 0.0f ? 0 : 1; pass < 2; ++pass) {
                const Pen mark{ kEmptyCol, 0.55f, 1.2f * hs, keyline, pass == 0 };
                if (s.sealed) {   // a sealed slot: an X
                    mark.Line(cx - 7.0f * hs, cy - 7.0f * hs, cx + 7.0f * hs, cy + 7.0f * hs);
                    mark.Line(cx - 7.0f * hs, cy + 7.0f * hs, cx + 7.0f * hs, cy - 7.0f * hs);
                } else {          // an open slot: a plus
                    mark.Line(cx - 7.0f * hs, cy, cx + 7.0f * hs, cy);
                    mark.Line(cx, cy - 7.0f * hs, cx, cy + 7.0f * hs);
                }
            }
            continue;
        }
        const Rgb col = GlyphColor(s.glyph);
        const bool ready = s.elapsedFrac >= 1.0f;
        if (ready)
            hud_lines::Ring(cx, cy, radius, 2.0f * hs, col, 1.0f, keyline);
        else
            DrawCooldownCircle(cx, cy, radius, s.elapsedFrac, col, hs, keyline);
        if (s.effectFrac > 0.0f)
            hud_lines::Ring(cx, cy, radius + 5.0f * hs, 1.1f * hs, col, s.effectFrac, keyline);
        if (s.pulse > 0.0f) {
            const float t = PulseT(s.pulse);
            hud_lines::Ring(cx, cy, radius + 14.0f * hs * t, 1.4f * hs, col,
                            0.9f * (1.0f - t), 0.0f);
        }
        const Pen pen{ col, s.iconBright ? 1.0f : 0.30f, 1.8f * hs, keyline, false };
        DrawSkillGlyph(pen, s.glyph, cx, cy, radius * 0.55f);
    }
    BatchFlush();

    // ── text: key letters centred above each circle, seconds inside ──
    for (int i = 0; i < 4; ++i) {
        const SkillSlotView& s = view.slots[i];
        const float cx = keyX(i);
        const bool ready = !s.empty && s.elapsedFrac >= 1.0f;
        const float kw = g_TextS.Width(s.key, keySc);
        g_TextS.Draw(s.key, cx - kw * 0.5f, keyTop, keySc,
                     kKeyText.r, kKeyText.g, kKeyText.b, ready ? 0.95f : 0.55f);
        if (s.showSeconds) {
            wchar_t buf[8];
            swprintf_s(buf, L"%d", (int)std::ceil(s.cdRemain));
            const float w = g_TextL.Width(buf, secSc);
            const float h = g_TextL.Height(buf, secSc);
            g_TextL.Draw(buf, cx - w * 0.5f, cy - h * 0.5f, secSc,
                         kKeyText.r, kKeyText.g, kKeyText.b, 0.95f);
        }
    }
}
