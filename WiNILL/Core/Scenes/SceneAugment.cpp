#include "Scenes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "GameContext.h"
#include "SceneUI.h"
#include "SceneInternal.h"
#include "UiLayout.h"
#include "Settings.h"
#include "KeyBindings.h"
#include "Input.h"
#include "Augment.h"
#include "SynergyRuntime.h"
#include "../../System/Audio.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "../../Render/HudLines.h"
#include "../../Render/AugCardModel.h"
#include "../../Render/Juice.h"
#include "../../Render/StatIcons.h"
#include "Glossary.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cwchar>

// A glossary phrase keeps its words on one line: its spaces become no-break
// spaces (U+00A0) while wrapping, measured as spaces, and turn back afterwards.
static constexpr wchar_t kNbsp = L'\x00A0';
// The range unit "○" (one health-ring radius, Entity/RangeUnit.h) is drawn as
// a thin ring of its own instead of the font's heavy circle glyph.
static constexpr wchar_t kRingChar = L'\x25CB';
struct RingMark { float x, y, r; hud_lines::Rgb c; float a; };
static float RingAdvance(float scale) { return g_TextS.Height(L"가", scale) * 0.74f; }
static float RingTextWidth(const std::wstring& s, float scale) {
    float w = 0.0f;
    size_t a = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i < s.size() && s[i] != kRingChar) continue;
        if (i > a) w += g_TextS.Width(s.substr(a, i - a).c_str(), scale);
        if (i < s.size()) w += RingAdvance(scale);
        a = i + 1;
    }
    return w;
}
// Draws s at (x, y) (top-left) with every "○" left for rings; returns the width.
static float DrawRingText(const std::wstring& s, float x, float y, float scale, hud_lines::Rgb c,
                          float a, float shadow, std::vector<RingMark>& rings) {
    const float h = g_TextS.Height(L"가", scale), adv = RingAdvance(scale);
    float px = x;
    size_t from = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i < s.size() && s[i] != kRingChar) continue;
        if (i > from) {
            const std::wstring part = s.substr(from, i - from);
            DrawShadowedText(g_TextS, part.c_str(), px, y, scale, c.r, c.g, c.b, a, shadow);
            px += g_TextS.Width(part.c_str(), scale);
        }
        if (i < s.size()) {
            rings.push_back({ px + adv * 0.5f, y + h * 0.54f, h * 0.30f, c, a });
            px += adv;
        }
        from = i + 1;
    }
    return px - x;
}
// A glossary word on a card has its term icon in front ("75% 확률로 ↗벽 튕김").
// While wrapping, a private-use mark stands in for the icon, glued to the word
// so the two never split across lines; it takes the icon's width.
static constexpr wchar_t kTermMark = L'\xE000';
static float TermIconSize(float scale) { return g_TextS.Height(L"가Ag", scale) * 1.05f; }
static float TermMarkWidth(float scale) { return TermIconSize(scale) * 1.18f; }
static float WrapWidth(const std::wstring& s, float scale) {
    std::wstring t;
    t.reserve(s.size());
    int marks = 0;
    for (wchar_t ch : s) {
        if (ch == kTermMark) { ++marks; continue; }
        t += ch == kNbsp ? L' ' : ch;
    }
    const float w = t.find(kRingChar) == std::wstring::npos ? g_TextS.Width(t.c_str(), scale) : RingTextWidth(t, scale);
    return w + (float)marks * TermMarkWidth(scale);
}
static void DrawRingMarks(const std::vector<RingMark>& rings, float ui) {
    if (rings.empty()) return;
    BindMainShader();
    // A ring with the ship's dot in it, so it does not read as the letter o.
    for (const RingMark& m : rings) {
        hud_lines::Ring(m.x, m.y, m.r, 1.3f * ui, m.c, m.a, 0.0f);
        drawCircle(m.x, m.y, std::max(1.0f, m.r * 0.26f), m.c.r, m.c.g, m.c.b, m.a);
    }
    BatchFlush();
}

// One description segment wrapped to maxWidth, appended to lines. Lines break
// at spaces, and also after a '·' or before a '(' inside a run with no spaces
// ("효과(관통·튕김·분열)"), so a long list keeps filling the line instead of
// jumping whole to the next one.
static void WrapSegment(std::wstring segment, float scale, float maxWidth,
                        std::vector<std::wstring>& lines) {
    while (!segment.empty() && segment.front() == L' ') segment.erase(segment.begin());
    struct Piece { std::wstring text; bool spaceBefore; };
    std::vector<Piece> pieces;
    {
        std::wstring cur;
        bool space = false;
        auto flush = [&](bool nextSpace) {
            if (!cur.empty()) pieces.push_back({ cur, space });
            cur.clear();
            space = nextSpace;
        };
        for (size_t i = 0; i < segment.size(); ++i) {
            const wchar_t ch = segment[i];
            if (ch == L' ') { flush(true); continue; }
            if (ch == L'(' && !cur.empty()) flush(false);
            cur += ch;
            if (ch == L'\xB7' && i + 1 < segment.size() && segment[i + 1] != L' ') flush(false);
        }
        flush(false);
    }
    std::wstring line;
    for (const Piece& pc : pieces) {
        const std::wstring& word = pc.text;
        const std::wstring candidate = line.empty() ? word : line + (pc.spaceBefore ? L" " : L"") + word;
        if (WrapWidth(word, scale) > maxWidth) {
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            std::wstring part;
            for (wchar_t ch : word) {
                const std::wstring next = part + ch;
                if (!part.empty() &&
                    WrapWidth(next, scale) > maxWidth) {
                    lines.push_back(part);
                    part.clear();
                }
                part += ch;
            }
            line = std::move(part);
        } else if (!line.empty() &&
                   WrapWidth(candidate, scale) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.empty()) lines.push_back(line);
}

// Description segments: split at '·' and at a spaced " / ".
static std::vector<std::wstring> DescSegments(const wchar_t* src) {
    std::vector<std::wstring> segments;
    const wchar_t* text = src ? src : L"";
    std::wstring segment;
    int depth = 0;   // a '·' inside brackets is a list ("(관통·튕김·분열)"), not a break
    for (const wchar_t* p = text; *p; ++p) {
        if (*p == L'(' || *p == L'[') ++depth;
        else if ((*p == L')' || *p == L']') && depth > 0) --depth;
        const bool spacedSlash = *p == L'/' && p > text && p[1] != L'\0'
                              && p[-1] == L' ' && p[1] == L' ';
        if ((*p == L'\xB7' && depth == 0) || spacedSlash) {
            segments.push_back(std::move(segment));
            segment.clear();
            continue;
        }
        segment.push_back(*p);
    }
    segments.push_back(std::move(segment));
    return segments;
}

std::vector<std::wstring> TarotWrap(const wchar_t* src, float scale,
                                            float maxWidth) {
    std::vector<std::wstring> lines;
    for (std::wstring& segment : DescSegments(src))
        WrapSegment(std::move(segment), scale, maxWidth, lines);
    return lines;
}

namespace { GLuint TermIconTex(int term); }

// A linked word gets its term icon in front unless it is a number ("2.5○"
// links every time and already has its ring) or the term has no icon.
static bool TermGetsIcon(const std::wstring& s, const glossary::Span& sp) {
    for (int k = sp.start; k < sp.start + sp.len; ++k)
        if (std::iswdigit(s[(size_t)k])) return false;
    return TermIconTex((int)sp.term) != 0;
}

IconDesc WrapIconDesc(const wchar_t* desc, float scale, float maxWidth, float ui, bool termIcons) {
    IconDesc out;
    out.iconSize = g_TextS.Height(L"가Ag", scale) * 1.15f;
    out.iconIndent = out.iconSize + 8.0f * ui;
    out.termIconSize = TermIconSize(scale);
    out.termMarkW = TermMarkWidth(scale);
    std::vector<std::wstring> wrapped;
    const int lang = CurLangIdx();
    uint64_t used = 0;   // each glossary term links once per card
    uint64_t usedPre = 0;   // the same walk over whole segments, to place the icon marks
    for (std::wstring& segment : DescSegments(desc)) {
        stat_icon::Stat stat;
        const bool icon = stat_icon::FromText(segment, stat);
        // Links found before the no-break spaces go in ("벽 튕김" only matches with its space).
        const std::vector<glossary::Span> preLinks =
            termIcons ? glossary::Link(segment, lang, usedPre) : std::vector<glossary::Span>();
        for (const glossary::Span& sp : glossary::FindAll(segment, lang))
            for (int k = sp.matchStart; k < sp.matchStart + sp.matchLen; ++k)
                if (segment[(size_t)k] == L' ') segment[(size_t)k] = kNbsp;
        for (auto it = preLinks.rbegin(); it != preLinks.rend(); ++it)
            if (TermGetsIcon(segment, *it)) segment.insert((size_t)it->matchStart, 1, kTermMark);
        wrapped.clear();
        WrapSegment(std::move(segment), scale, icon ? maxWidth - out.iconIndent : maxWidth, wrapped);
        for (size_t j = 0; j < wrapped.size(); ++j) {
            DescLine line{ std::wstring(), icon && j == 0 ? (int)stat : -1, icon, {} };
            std::vector<int> marks;   // where an icon mark stood, in the cleaned text
            for (wchar_t ch : wrapped[j]) {
                if (ch == kTermMark) { marks.push_back((int)line.text.size()); continue; }
                line.text += ch == kNbsp ? L' ' : ch;
            }
            for (const glossary::Span& sp : glossary::Link(line.text, lang, used)) {
                const bool hasIcon = std::find(marks.begin(), marks.end(), sp.matchStart) != marks.end();
                line.links.push_back({ sp.start, sp.len, (int)sp.term, hasIcon });   // icon words: start == matchStart
            }
            out.lines.push_back(std::move(line));
        }
    }
    return out;
}

void DrawIconDescIcon(const IconDesc& desc, const DescLine& line,
                      float x, float y, float lineH, float alpha) {
    if (line.stat < 0) return;
    stat_icon::Draw((stat_icon::Stat)line.stat, x + desc.iconSize * 0.5f, y + lineH * 0.5f,
                    desc.iconSize, alpha);
}

// ─────────────────────────────────────────────────────────────
// Augment card draw (Docs/Design/Augment_Card_Draw_Design.md). Every
// level-up deals up to five cards from a deck at the bottom; they flip, the
// family constellation traces itself behind the text, and the pick flies to
// the skill row while the other cards dissolve into stars. Cards are outlines
// only: no filled face, the gameplay stays visible behind them. Timing,
// layout and the constellations live in Render/AugCardModel.h.
// ─────────────────────────────────────────────────────────────
namespace {

using hud_lines::Rgb;
namespace ac = aug_card;

constexpr Rgb kUiBlue  = { 0.18f, 0.62f, 0.96f };
constexpr Rgb kText    = { 0.92f, 0.96f, 1.00f };
constexpr Rgb kTextSub = { 0.76f, 0.84f, 0.94f };
constexpr Rgb kWhite   = { 1.0f, 1.0f, 1.0f };
constexpr float kDeg = 3.14159265f / 180.0f;

// Card-local layout, top to bottom: rarity marks, augment icon, title,
// divider, kind line, description (card spans -205..205).
constexpr float kRarityY = -187.0f;
constexpr float kIconY = -134.0f, kIconSize = 66.0f;
constexpr float kTitleY = -84.0f, kDivY = -58.0f;
constexpr float kMetaY = -48.0f, kDescY = -24.0f, kDescBottom = 160.0f;

// Outlined RPG Awesome icon of an augment (Resource/Icons/AUG_<code>.png),
// loaded on first use. 0 when the file is missing.
GLuint AugIconTex(const AugDef& def) {
    static GLuint tex[AUG_TOTAL] = {};
    static bool tried[AUG_TOTAL] = {};
    const int idx = (int)(&def - ALL_AUGS);
    if (idx < 0 || idx >= AUG_TOTAL) return 0;
    if (!tried[idx]) {
        tried[idx] = true;
        char name[96];
        std::snprintf(name, sizeof(name), "AUG_%s", def.name);
        tex[idx] = IconLoad(name);
        IconEnableMipmaps(tex[idx]);
    }
    return tex[idx];
}

// Outlined RPG Awesome icon of a glossary term (Resource/Icons/TERM_<name>.png),
// loaded on first use. 0 for a term without one.
constexpr const char* kTermIconNames[] = {
    "Verb", "Pierce", "Bounce", "Split", "Homing", "Blast", "Burn", "Mark", "Orbit", "Trail", "Mine", "Hover",
    "Pull", "Return", "Swap", "Erase", "Graze", "Shield", "Still", "Crit", "CritMult", "HitCount", "DmgKinds",
    "FireRate", "Range", "Distance", "Stardust", "Luck", "Broken", "Stack", "RunWave",
    "ActiveSkill", "Dash", "SkillSlot", "Reaction", "Risky", "ChanceAug", "Enemies" };
static_assert(sizeof(kTermIconNames) / sizeof(kTermIconNames[0]) == glossary::kTermCount,
              "kTermIconNames follows glossary::Term");
GLuint TermIconTex(int term) {
    static GLuint tex[glossary::kTermCount] = {};
    static bool tried[glossary::kTermCount] = {};
    if (term < 0 || term >= glossary::kTermCount) return 0;
    if (!tried[term]) {
        tried[term] = true;
        char name[64];
        std::snprintf(name, sizeof(name), "TERM_%s", kTermIconNames[term]);
        tex[term] = IconLoad(name);
        IconEnableMipmaps(tex[term]);
    }
    return tex[term];
}

Rgb Mix(Rgb a, Rgb b, float t) {
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}
float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// Rarity colour lifted toward white so thin lines stay readable.
Rgb RarityLine(AugRarity r) {
    float cr, cg, cb;
    GetRarityColor(r, cr, cg, cb);
    return { cr * 0.68f + 0.32f, cg * 0.68f + 0.32f, cb * 0.68f + 0.32f };
}

// Card-local → screen: centre, per-axis scale (x narrows during the flip),
// rotation.
struct Xf {
    float cx, cy, sx, sy, c, s;
    void P(float lx, float ly, float& x, float& y) const {
        const float ax = lx * sx, ay = ly * sy;
        x = cx + ax * c - ay * s;
        y = cy + ax * s + ay * c;
    }
};
Xf MakeXf(float cx, float cy, float sx, float sy, float rot) {
    return { cx, cy, sx, sy, std::cos(rot), std::sin(rot) };
}

// Solid stroke with its keyline underneath (UI guide §3).
void SLine(float x0, float y0, float x1, float y1, float w, Rgb c, float a, float key) {
    if (a <= 0.002f) return;
    if (key > 0.0f)
        hud_lines::Segment(x0, y0, x1, y1, w + key, hud_lines::KeyCol(c), hud_lines::kKeyA * a);
    hud_lines::Segment(x0, y0, x1, y1, w, c, a);
}
void LLine(const Xf& X, float x0, float y0, float x1, float y1, float w, Rgb c, float a, float key) {
    float ax, ay, bx, by;
    X.P(x0, y0, ax, ay);
    X.P(x1, y1, bx, by);
    SLine(ax, ay, bx, by, w, c, a, key);
}
void SDiamond(float x, float y, float r, Rgb c, float a, float w, float key, bool dot = false) {
    SLine(x, y - r, x + r, y, w, c, a, key);
    SLine(x + r, y, x, y + r, w, c, a, key);
    SLine(x, y + r, x - r, y, w, c, a, key);
    SLine(x - r, y, x, y - r, w, c, a, key);
    if (dot) drawCircle(x, y, std::max(0.8f, r * 0.30f), c.r, c.g, c.b, a);
}
void LDiamond(const Xf& X, float lx, float ly, float r, Rgb c, float a, float w, float key) {
    LLine(X, lx, ly - r, lx + r, ly, w, c, a, key);
    LLine(X, lx + r, ly, lx, ly + r, w, c, a, key);
    LLine(X, lx, ly + r, lx - r, ly, w, c, a, key);
    LLine(X, lx - r, ly, lx, ly - r, w, c, a, key);
}
void LArc(const Xf& X, float cx, float cy, float r, float a0, float sweep,
          float w, Rgb c, float a, float key) {
    const int n = std::max(2, (int)(std::fabs(sweep) / (2.0f * 3.14159265f) * 36.0f));
    for (int i = 0; i < n; ++i) {
        const float t0 = a0 + sweep * i / n, t1 = a0 + sweep * (i + 1) / n;
        LLine(X, cx + std::cos(t0) * r, cy + std::sin(t0) * r,
                 cx + std::cos(t1) * r, cy + std::sin(t1) * r, w, c, a, key);
    }
}
void LOutline(const Xf& X, float w, float h, float cut, float thick, Rgb c, float a, float key) {
    if (a <= 0.002f) return;
    float lx[8], ly[8], px[8], py[8];
    ac::ChamferCorners(w, h, cut, lx, ly);
    for (int i = 0; i < 8; ++i) X.P(lx[i], ly[i], px[i], py[i]);
    hud_lines::ClosedOutline(px, py, 8, thick, c, a, key);
}
void LCornerMarks(const Xf& X, Rgb c, float a, float w, float key) {
    const float cx = ac::kCardW * 0.5f - ac::kCut * 0.5f, cy = ac::kCardH * 0.5f - ac::kCut * 0.5f;
    const float k[4][2] = { {-cx, -cy}, {cx, -cy}, {-cx, cy}, {cx, cy} };
    for (auto& p : k) LDiamond(X, p[0], p[1], 4.0f, c, a, w, key);
}

// Open thick polyline with mitred joints, one shared vertex pair per bend, so
// nothing is drawn twice: a faded stroke shows no brighter overlap at the
// bends and the inner corner has no nub. Keyline pass first.
void SMiterPath(const float* px, const float* py, int n, float w, Rgb c, float a, float key) {
    constexpr int kMax = 8;
    if (n < 2 || n > kMax || a <= 0.002f) return;
    for (int pass = key > 0.0f ? 0 : 1; pass < 2; ++pass) {
        const float tw = pass == 0 ? w + key : w;
        const Rgb col = pass == 0 ? hud_lines::KeyCol(c) : c;
        const float alpha = pass == 0 ? hud_lines::kKeyA * a : a;
        float ox[kMax], oy[kMax], ix[kMax], iy[kMax];
        for (int i = 0; i < n; ++i) {
            const int h = std::max(0, i - 1), j = std::min(n - 1, i + 1);
            float d1x = px[i] - px[h], d1y = py[i] - py[h];
            float d2x = px[j] - px[i], d2y = py[j] - py[i];
            if (i == 0) { d1x = d2x; d1y = d2y; }
            if (i == n - 1) { d2x = d1x; d2y = d1y; }
            const float l1 = std::max(0.001f, std::hypot(d1x, d1y)), l2 = std::max(0.001f, std::hypot(d2x, d2y));
            d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
            float mx = -d1y - d2y, my = d1x + d2x;
            const float ml = std::max(0.001f, std::hypot(mx, my));
            mx /= ml; my /= ml;
            const float cosHalf = std::max(0.25f, mx * -d1y + my * d1x);
            const float reach = tw * 0.5f / cosHalf;
            ox[i] = px[i] + mx * reach; oy[i] = py[i] + my * reach;
            ix[i] = px[i] - mx * reach; iy[i] = py[i] - my * reach;
        }
        for (int i = 0; i + 1 < n; ++i) {
            BatchTri(ox[i], oy[i], ox[i + 1], oy[i + 1], ix[i + 1], iy[i + 1], col.r, col.g, col.b, alpha);
            BatchTri(ox[i], oy[i], ix[i + 1], iy[i + 1], ix[i], iy[i], col.r, col.g, col.b, alpha);
        }
    }
}

// Corner brackets that follow the chamfer: a run along each edge into the cut.
void LCornerBrackets(const Xf& X, Rgb c, float a, float w, float len, float key) {
    const float hx = ac::kCardW * 0.5f, hy = ac::kCardH * 0.5f, cut = ac::kCut;
    for (int q = 0; q < 4; ++q) {
        const float sx = (q & 1) ? 1.0f : -1.0f, sy = (q & 2) ? 1.0f : -1.0f;
        const float lx[4] = { sx * (hx - cut - len), sx * (hx - cut), sx * hx, sx * hx };
        const float ly[4] = { sy * hy, sy * hy, sy * (hy - cut), sy * (hy - cut - len) };
        float px[4], py[4];
        for (int k = 0; k < 4; ++k) X.P(lx[k], ly[k], px[k], py[k]);
        SMiterPath(px, py, 4, w, c, a, key);
    }
}

// Node k of a family constellation, card-local.
void ConNode(const ac::Shape& sh, int k, float& lx, float& ly) {
    lx = ac::kConX + sh.x[k] * ac::kConW;
    ly = ac::kConY + sh.y[k] * ac::kConH;
}

// Back: gapped ring + eight spokes (ASTERIA sigil); legendary+ fills the
// ring from 12 o'clock while charging.
void DrawBack(const Xf& X, Rgb rarity, float a, float charge, float ui, float key) {
    const Rgb ring = Mix(kUiBlue, rarity, 0.28f);
    // Same frame as the face: a quiet hairline with the corner brackets.
    LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.2f * ui, kUiBlue, a * 0.26f, key);
    LCornerBrackets(X, kUiBlue, a * 0.80f, 3.6f * ui, 30.0f, key);
    const float sy = -14.0f, rr = 72.0f;
    for (int k = 0; k < 4; ++k)
        LArc(X, 0.0f, sy, rr, (k * 90.0f - 35.0f) * kDeg, 70.0f * kDeg, 1.4f * ui, ring, a * 0.50f, key);
    LArc(X, 0.0f, sy, rr - 14.0f, 0.0f, 360.0f * kDeg, 1.0f * ui, kUiBlue, a * 0.16f, 0.0f);
    for (int k = 0; k < 8; ++k) {
        const float ang = (-90.0f + k * 45.0f) * kDeg, r1 = rr + 10.0f, r2 = rr + ((k % 2) ? 26.0f : 40.0f);
        LLine(X, std::cos(ang) * r1, sy + std::sin(ang) * r1, std::cos(ang) * r2, sy + std::sin(ang) * r2,
              1.0f * ui, kUiBlue, a * 0.30f, key);
        LDiamond(X, std::cos(ang) * (r2 + 5.0f), sy + std::sin(ang) * (r2 + 5.0f),
                 (k % 2) ? 2.6f : 3.6f, ring, a * 0.55f, 1.1f * ui, key);
    }
    LDiamond(X, 0.0f, sy, 26.0f, ring, a * 0.55f, 1.3f * ui, key);
    LDiamond(X, 0.0f, sy, 12.0f, kUiBlue, a * 0.30f, 1.0f * ui, key);
    if (charge > 0.0f)
        LArc(X, 0.0f, sy, rr, -90.0f * kDeg, 360.0f * kDeg * charge, 3.0f * ui, rarity, a * 0.95f, key);
}

struct CardText {
    std::wstring meta;
    IconDesc desc;   // description lines with their stat icons
    float titleScale = 1.0f, metaScale = 1.0f, descScale = 1.0f;
};

// Icon on top, then the title; everything under it is description text: the
// kind and cooldown line first, then the effect. Shrinks the description
// until it fits above the key hint.
CardText LayoutCardText(const AugDef& def, float ui) {
    CardText t;
    const float inner = (ac::kCardW - 44.0f) * ui;
    float f = ui;
    t.titleScale = UiTextScale(g_TextL, UiTextLevel::Subtitle, f);
    while (f > ui * 0.55f && g_TextL.Width(AugName(def), t.titleScale) > inner) {
        f -= 0.04f * ui;
        t.titleScale = UiTextScale(g_TextL, UiTextLevel::Subtitle, f);
    }
    static const wchar_t* kActive[3] = { L"액티브 · 쿨다운 %.0f초", L"Active · %.0fs cooldown", L"アクティブ · CT %.0f秒" };
    static const wchar_t* kPassive[3] = { L"패시브", L"Passive", L"パッシブ" };
    wchar_t meta[160];
    const std::wstring synMeta = AugIsSynergy(def.type) ? syn::MetaLine((int)(&def - ALL_AUGS), CurLangIdx()) : std::wstring();
    if (AugIsActive(def)) swprintf_s(meta, kActive[CurLangIdx()], def.cooldown);
    else if (!synMeta.empty()) swprintf_s(meta, L"%ls", synMeta.c_str());
    else swprintf_s(meta, L"%ls", kPassive[CurLangIdx()]);
    t.meta = meta;
    t.metaScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    const float room = (kDescBottom - kDescY) * ui;
    const float floorScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    float d = ui;
    for (;;) {
        t.descScale = UiTextScale(g_TextS, UiTextLevel::Description, d);
        t.desc = WrapIconDesc(AugDesc(def), t.descScale, inner, ui, true);
        float h = 0.0f;
        for (const auto& l : t.desc.lines) h += g_TextS.Height(l.text.c_str(), t.descScale) + 4.0f * ui;
        if (h <= room || t.descScale <= floorScale) break;
        d -= 0.04f * ui;
    }
    return t;
}

struct Shard { float x, y, vx, vy, life; Rgb c; };

// A glossary word on screen: the card it sits on (or the definition level it
// sits in, for related words), the term and its box.
struct LinkBox { int owner, term; float x0, y0, x1, y1; Rgb col; float a; };

// Definitions under the cards, opened by hovering a glossary word (Glossary.h).
// Level 0 hangs under the card; a related word in a definition opens the next
// level below it, like following wiki links. Nothing shows without a hover.
struct TipLevel {
    int term = -1, card = -1;
    float ax0 = 0, ax1 = 0, ay = 0;   // the word that opened it (underline y)
    float x = 0, y = 0, w = 0, h = 0;
    bool side = false;                // placed beside the level above, not under it
};
constexpr int kTipLevels = 3;
// How open the definitions are (0..1): the deck fades out while they show.
float g_GlossaryOpenA = 0.0f;

void DrawGlossaryTips(const std::vector<LinkBox>& cardLinks, const float* cardX, const float* cardY,
                      const Rgb* cardCol, int count, bool active, float mx, float my, float delta,
                      int keyCard, bool keyPress, bool pointerMoved,
                      float sw, float sh, float ui) {
    static TipLevel tips[kTipLevels];
    static int open = 0;
    static float grace = 0.0f, shrink = 0.0f;
    static std::vector<LinkBox> relLinks;   // related words drawn last frame (owner = level)
    static int keyIndex = -1, keyOwner = -1;   // the glossary key's word on the focused card
    if (!active) { open = 0; relLinks.clear(); g_GlossaryOpenA = 0.0f; keyIndex = -1; return; }
    // Glossary key (bindable, Tab by default): each press opens the next word
    // of the focused card, the press after the last one closes. Moving the
    // pointer or another card hands back to the mouse.
    if (pointerMoved || keyCard != keyOwner) keyIndex = -1;
    if (keyPress && keyCard >= 0) {
        int words = 0;
        for (const LinkBox& b : cardLinks) words += b.owner == keyCard ? 1 : 0;
        keyIndex = keyOwner == keyCard ? keyIndex + 1 : 0;
        keyOwner = keyCard;
        if (keyIndex >= words) { keyIndex = -1; open = 0; }
    }
    keyOwner = keyCard;
    if (keyIndex >= 0) {
        int k = 0;
        const LinkBox* word = nullptr;
        for (const LinkBox& b : cardLinks)
            if (b.owner == keyCard && k++ == keyIndex) { word = &b; break; }
        if (word) { mx = (word->x0 + word->x1) * 0.5f; my = (word->y0 + word->y1) * 0.5f; }
        else keyIndex = -1;
    }
    if (g_SmokeCapture && g_SmokeGlossDepth > 0) {
        // Smoke capture: rest the pointer on the first word, then on the
        // first related word of each open level, down to the asked depth.
        const LinkBox* aim = nullptr;
        const int want = std::min(open, g_SmokeGlossDepth - 1);
        for (const LinkBox& b : (want == 0 ? cardLinks : relLinks))
            if (want == 0 ? (keyCard < 0 || b.owner == keyCard) : b.owner == want - 1) { aim = &b; break; }
        if (!aim && !cardLinks.empty()) aim = &cardLinks.front();
        if (aim) { mx = (aim->x0 + aim->x1) * 0.5f; my = (aim->y0 + aim->y1) * 0.5f; }
    }

    auto inBox = [&](const LinkBox& b) {
        return mx >= b.x0 - 2.0f * ui && mx <= b.x1 + 2.0f * ui && my >= b.y0 - 2.0f * ui && my <= b.y1 + 4.0f * ui;
    };
    const float tipW = (ac::kCardW + 40.0f) * ui;
    // What the pointer is on: a card word opens level 0, a related word opens
    // the level after its own; inside an open level keeps it.
    int keep = -1;
    for (const LinkBox& b : cardLinks) {
        if (!inBox(b)) continue;
        if (open == 0 || tips[0].term != b.term || tips[0].card != b.owner) {
            tips[0] = TipLevel{ b.term, b.owner, b.x0, b.x1, b.y1 + 1.0f * ui };
            open = 1;
        }
        keep = 0;
        break;
    }
    if (keep < 0)
        for (const LinkBox& b : relLinks) {
            if (b.owner >= open || !inBox(b)) continue;
            const int next = b.owner + 1;
            // A word already open further up (a -> b -> a) stays where it is;
            // opening it again would stack a copy on top of the first one.
            bool shown = false;
            for (int k = 0; k <= b.owner; ++k) shown = shown || tips[k].term == b.term;
            if (shown) { keep = b.owner; break; }
            if (next < kTipLevels) {
                if (open <= next || tips[next].term != b.term) {
                    tips[next] = TipLevel{ b.term, tips[b.owner].card, b.x0, b.x1, b.y1 + 2.0f * ui };
                    open = next + 1;
                }
                keep = next;
            } else {
                keep = b.owner;
            }
            break;
        }
    if (keep < 0)
        for (int k = open - 1; k >= 0; --k) {
            const TipLevel& tl = tips[k];
            if (mx >= tl.x - 12.0f * ui && mx <= tl.x + tl.w + 12.0f * ui &&
                my >= tl.y - 24.0f * ui && my <= tl.y + tl.h + 12.0f * ui) { keep = k; break; }
        }
    // Leaving everything closes after a short grace, so the pointer can travel
    // from a word down to its definition; leaving only a deeper level folds it.
    if (keep >= 0) {
        grace = 0.35f;
        if (keep + 1 < open) {
            shrink += delta;
            if (shrink > 0.35f) { open = keep + 1; shrink = 0.0f; }
        } else {
            shrink = 0.0f;
        }
    } else if (open > 0) {
        grace -= delta;
        if (grace <= 0.0f) open = 0;
    }
    relLinks.clear();
    g_GlossaryOpenA += ((open > 0 ? 1.0f : 0.0f) - g_GlossaryOpenA) * (1.0f - std::exp(-14.0f * std::max(0.0f, delta)));
    if (open == 0) return;

    const int lang = CurLangIdx();
    const float titleScale = UiTextScale(g_TextS, UiTextLevel::Description, ui);
    const float bodyScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    const float pad = 8.0f * ui;
    static const wchar_t* kRelated[3] = { L"관련", L"See also", L"関連" };
    struct TextCmd { std::wstring s; float x, y, scale; Rgb c; float a; };
    struct IconCmd { GLuint tex; float x, y, size; Rgb c; };
    std::vector<TextCmd> texts;
    std::vector<IconCmd> icons;
    BindMainShader();
    for (int k = 0; k < open; ++k) {
        TipLevel& tl = tips[k];
        if (tl.term < 0 || tl.term >= glossary::kTermCount || tl.card < 0 || tl.card >= count) { open = k; break; }
        const glossary::Entry& e = glossary::Get((glossary::Term)tl.term);
        const Rgb col = cardCol[tl.card];
        // Place: level 0 under its card, deeper levels stepped down and right;
        // past the screen bottom a level sits beside the one above instead.
        tl.w = std::min(tipW, sw - 48.0f * ui);
        std::vector<std::wstring> body;
        WrapSegment(e.def[lang], bodyScale, tl.w - 2.0f * pad, body);
        const float titleH = g_TextS.Height(L"가Ag", titleScale), bodyH = g_TextS.Height(L"가Ag", bodyScale);
        tl.h = titleH + 10.0f * ui + (float)body.size() * (bodyH + 3.0f * ui) +
               (e.relCount > 0 ? bodyH + 10.0f * ui : 0.0f);
        if (k == 0) {
            tl.x = cardX[tl.card] - ac::kCardW * 0.5f * ui;
            tl.y = cardY[tl.card] + (ac::kCardH * 0.5f + 52.0f) * ui;
        } else {
            // First free spot, in order: under the level above, beside it
            // (right, left), then past every open level (right, left). Free
            // means on screen and clear of every open level, so two
            // definitions never draw over each other.
            const TipLevel& up = tips[k - 1];
            const float m = 24.0f * ui, gap = 24.0f * ui;
            float minX = up.x, maxX = up.x + up.w;
            for (int j = 0; j < k; ++j) { minX = std::min(minX, tips[j].x); maxX = std::max(maxX, tips[j].x + tips[j].w); }
            const struct { float x, y; bool side; } spots[5] = {
                { up.x + 26.0f * ui, up.y + up.h + 26.0f * ui, false },
                { up.x + up.w + gap, up.y, true },
                { up.x - tl.w - gap, up.y, true },
                { maxX + gap, up.y, true },
                { minX - tl.w - gap, up.y, true },
            };
            auto clear = [&](float x, float y) {
                if (x < m || x + tl.w > sw - m || y + tl.h > sh - m) return false;
                for (int j = 0; j < k; ++j) {
                    const TipLevel& o = tips[j];
                    if (x < o.x + o.w + 8.0f * ui && x + tl.w + 8.0f * ui > o.x &&
                        y < o.y + o.h + 8.0f * ui && y + tl.h + 8.0f * ui > o.y) return false;
                }
                return true;
            };
            int pick = 0;
            while (pick < 5 && !clear(spots[pick].x, spots[pick].y)) ++pick;
            if (pick == 5) pick = 0;   // nothing free: under the level above, kept on screen
            tl.x = spots[pick].x;
            tl.y = spots[pick].y;
            tl.side = spots[pick].side;
        }
        if (k == 0 && tl.x + tl.w > sw - 24.0f * ui) tl.x = sw - 24.0f * ui - tl.w;
        tl.x = std::clamp(tl.x, 24.0f * ui, std::max(24.0f * ui, sw - 24.0f * ui - tl.w));
        tl.y = std::min(tl.y, sh - 24.0f * ui - tl.h);

        // The word that opened it gets a bold underline. The leader leaves a
        // card from its bottom edge (a line through the card would cross the
        // lines under the word), a related word from its own underline.
        const float ax = (tl.ax0 + tl.ax1) * 0.5f;
        SLine(tl.ax0, tl.ay, tl.ax1, tl.ay, 2.2f * ui, col, 1.0f, 0.0f);
        const float ly = tl.y - 6.0f * ui;
        if (k == 0) {
            const float y0 = cardY[tl.card] + (ac::kCardH * 0.5f + 4.0f) * ui;
            SLine(ax, y0, ax, ly, 1.0f * ui, col, 0.70f, 0.0f);
            SLine(ax, ly, tl.x + pad, ly, 1.0f * ui, col, 0.70f, 0.0f);
            drawCircle(ax, y0, 2.0f * ui, col.r, col.g, col.b, 0.9f);
        } else if (!tl.side) {
            SLine(ax, tl.ay, ax, ly, 1.0f * ui, col, 0.70f, 0.0f);
            SLine(ax, ly, tl.x + pad, ly, 1.0f * ui, col, 0.70f, 0.0f);
        } else {
            // Beside the others (right or left): the leader only runs through
            // empty space. Down from the word to below every open level,
            // across to the gap next to the new level, up the gap to the title
            // row, then in to the title's near end (a title on the left is met
            // at its right end, so the line never crosses text).
            const TipLevel& up = tips[k - 1];
            const bool onLeft = tl.x < up.x;
            float below = tl.ay + 8.0f * ui;
            for (int j = 0; j < k; ++j) below = std::max(below, tips[j].y + tips[j].h + 6.0f * ui);
            const float ry = std::min(below, sh - 6.0f * ui);
            const float gx = onLeft ? tl.x + tl.w + 12.0f * ui : tl.x - 12.0f * ui;
            const float titleMid = tl.y + 2.0f * ui + g_TextS.Height(L"가Ag", titleScale) * 0.5f;
            const GLuint tIcon = TermIconTex(tl.term);
            const float tEnd = tl.x + pad + (tIcon ? titleH * 1.25f + 6.0f * ui : 0.0f) +
                               g_TextS.Width(e.name[lang], titleScale) + 8.0f * ui;
            SLine(ax, tl.ay, ax, ry, 1.0f * ui, col, 0.70f, 0.0f);
            SLine(ax, ry, gx, ry, 1.0f * ui, col, 0.70f, 0.0f);
            SLine(gx, ry, gx, titleMid, 1.0f * ui, col, 0.70f, 0.0f);
            SLine(gx, titleMid, onLeft ? tEnd : tl.x + pad - 4.0f * ui, titleMid, 1.0f * ui, col, 0.70f, 0.0f);
        }
        // Title, underlined; then the definition; then related words.
        const wchar_t* title = e.name[lang];
        const float tw = g_TextS.Width(title, titleScale);
        float ty = tl.y + 2.0f * ui;
        const GLuint iconTex = TermIconTex(tl.term);
        const float iconSize = iconTex ? titleH * 1.25f : 0.0f;
        const float titleX = tl.x + pad + (iconTex ? iconSize + 6.0f * ui : 0.0f);
        const Rgb titleCol = Mix(col, kText, 0.45f);
        if (iconTex) icons.push_back({ iconTex, tl.x + pad, ty + titleH * 0.5f - iconSize * 0.5f, iconSize, titleCol });
        texts.push_back({ title, titleX, ty, titleScale, titleCol, 1.0f });
        SLine(tl.x + pad, ty + titleH + 3.0f * ui, titleX + tw + 6.0f * ui, ty + titleH + 3.0f * ui,
              1.0f * ui, col, 0.65f, 0.0f);
        ty += titleH + 10.0f * ui;
        for (const std::wstring& line : body) {
            texts.push_back({ line, tl.x + pad, ty, bodyScale, kText, 0.95f });
            ty += bodyH + 3.0f * ui;
        }
        if (e.relCount > 0) {
            ty += 7.0f * ui;
            float rx = tl.x + pad;
            texts.push_back({ kRelated[lang], rx, ty, bodyScale, kTextSub, 0.70f });
            rx += g_TextS.Width(kRelated[lang], bodyScale) + 12.0f * ui;
            for (int r = 0; r < e.relCount; ++r) {
                const int rt = (int)e.rel[r];
                const wchar_t* rn = glossary::Get(e.rel[r]).name[lang];
                const float rw = g_TextS.Width(rn, bodyScale);
                const bool hot = k + 1 < open && tips[k + 1].term == rt;
                texts.push_back({ rn, rx, ty, bodyScale, Mix(col, kText, hot ? 0.55f : 0.35f), 1.0f });
                SLine(rx, ty + bodyH + 2.0f * ui, rx + rw, ty + bodyH + 2.0f * ui,
                      (hot ? 1.4f : 1.0f) * ui, col, hot ? 0.9f : 0.6f, 0.0f);
                relLinks.push_back({ k, rt, rx, ty, rx + rw, ty + bodyH, col, 1.0f });
                rx += rw + 14.0f * ui;
            }
        }
    }
    BatchFlush();
    for (const IconCmd& ic : icons) DrawIcon(ic.tex, ic.x, ic.y, ic.size, ic.size, ic.c.r, ic.c.g, ic.c.b, 1.0f);
    std::vector<RingMark> rings;
    for (const TextCmd& tc : texts) DrawRingText(tc.s, tc.x, tc.y, tc.scale, tc.c, tc.a, 0.85f, rings);
    DrawRingMarks(rings, ui);
}

}  // namespace

static void Scene_AugSelectCards(const SceneCtx& c) {
    const float sw = c.sw, sh = c.sh, delta = c.delta;
    const int count = std::max(0, std::min(ac::kMaxCards, g_GameManager.augChoiceCount));
    const bool inExit = g_AugExitT >= 0.0f;
    const float exitT = inExit ? g_AugExitT : 0.0f;
    const float now = (float)glfwGetTime();
    const ac::Layout L = ac::MakeLayout(sw, sh, count);
    const float ui = L.s;
    const float key = 2.6f * ui;

    int rarity[ac::kMaxCards] = {};
    bool hasIcon[ac::kMaxCards] = {};
    const AugDef* defs[ac::kMaxCards] = {};
    for (int i = 0; i < count; ++i) {
        const int idx = g_GameManager.augChoices[i];
        defs[i] = (idx >= 0 && idx < AUG_TOTAL) ? &ALL_AUGS[idx] : nullptr;
        rarity[i] = defs[i] ? (int)defs[i]->rarity : 0;
        hasIcon[i] = defs[i] && AugIconTex(*defs[i]) != 0;   // loads before any batching
    }
    {
        static bool termIconsLoaded = false;   // like the card icons: loaded before any batching
        if (!termIconsLoaded) {
            termIconsLoaded = true;
            for (int k = 0; k < glossary::kTermCount; ++k) TermIconTex(k);
        }
    }
    const ac::Timeline T = ac::MakeTimeline(rarity, count);

    static int seenSerial = -1;
    static float t = 0.0f;
    static bool chained = false;
    static float lastFrame = -10.0f;
    static float focusT[ac::kMaxCards] = {};
    static float phase[ac::kMaxCards] = {};
    static bool burst[ac::kMaxCards] = {};
    static bool dealtSfx[ac::kMaxCards] = {};
    static bool flippedSfx[ac::kMaxCards] = {};
    static bool wasExit = false;
    static Shard shards[ac::kMaxCards * 8];
    static double previousMx = -1.0, previousMy = -1.0;
    static std::vector<CardText> texts;
    static int textLang = -1;
    static float textUi = 0.0f;

    if (seenSerial != g_GameManager.augmentSelectionSerial) {
        seenSerial = g_GameManager.augmentSelectionSerial;
        // A pick that follows straight on from the last one keeps the deck
        // on screen and deals at once.
        chained = now - lastFrame < 0.3f;
        t = chained ? ac::kDrawStart : 0.0f;
        for (int i = 0; i < ac::kMaxCards; ++i) {
            focusT[i] = 0.0f;
            phase[i] = ac::Hash01((unsigned)(seenSerial * 31 + i)) * 6.2832f;
            burst[i] = false;
            dealtSfx[i] = flippedSfx[i] = false;
        }
        wasExit = false;
        for (Shard& s : shards) s.life = 0.0f;
        previousMx = c.mx;
        previousMy = c.my;
        g_HoveredAug = -1;
        texts.clear();
    }
    lastFrame = now;
    if (texts.size() != (size_t)count || textLang != CurLangIdx() || std::fabs(textUi - ui) > 1e-4f) {
        texts.clear();
        for (int i = 0; i < count; ++i)
            texts.push_back(defs[i] ? LayoutCardText(*defs[i], ui) : CardText{});
        textLang = CurLangIdx();
        textUi = ui;
    }

    // Intro clock. A confirm during the deal (keyboard) or a smoke capture
    // jumps straight to the dealt state.
    if (inExit || (g_SmokeCapture && g_SmokeAugFocus >= 0)) t = std::max(t, T.end);
    else t += delta;
    const bool lmbClick = c.lmb && !g_LmbPrev;
    const bool inputReady = !inExit && t >= T.InputReady();
    if (!inExit && !inputReady && lmbClick) t = T.end;   // click skips the deal
    if (t >= T.end) for (int i = 0; i < count; ++i) burst[i] = true;
    // Deal and flip sounds follow the same timeline as the drawing; a skipped
    // deal fires them together and the cue throttle thins them out.
    for (int i = 0; i < count; ++i) {
        if (!dealtSfx[i] && t >= ac::kDrawStart + ac::kDrawStep * (float)i) {
            dealtSfx[i] = true;
            Audio::Play(Audio::Cue::CardDeal);
        }
        if (!flippedSfx[i] && t >= T.flipStart[i]) {
            flippedSfx[i] = true;
            // 희귀 이상은 기본 뒤집기 대신 등급 전용 소리
            switch ((AugRarity)rarity[i]) {
            case AugRarity::RARE:      Audio::Play(Audio::Cue::AugFlipRare);   break;
            case AugRarity::EPIC:      Audio::Play(Audio::Cue::AugFlipEpic);   break;
            case AugRarity::LEGENDARY: Audio::Play(Audio::Cue::AugFlipLegend); break;
            case AugRarity::MYTHIC:    Audio::Play(Audio::Cue::AugFlipMythic); break;
            default:                   Audio::Play(Audio::Cue::CardFlip);      break;
            }
        }
    }
    if (inExit && !wasExit) Audio::Play(Audio::Cue::CardPick);
    if (inExit && !wasExit && count > 1) Audio::Play(Audio::Cue::AugShatter);   // 안 고른 카드가 부서짐
    wasExit = inExit;

    if (g_SmokeCapture && g_SmokeAugFocus >= 0) {
        g_HoveredAug = std::min(g_SmokeAugFocus, count - 1);
        g_GameManager.augmentKeyboardFocus = true;
        g_SmokeAugFocus = -1;
    }

    // Hover: the pointer focuses a card unless the last focus came from 1~5.
    float lift[ac::kMaxCards] = {};
    for (int i = 0; i < count; ++i) lift[i] = 14.0f * ui * focusT[i];
    const bool pointerMoved = previousMx < 0.0 ||
        std::fabs(c.mx - previousMx) > 3.0 || std::fabs(c.my - previousMy) > 3.0;
    if (pointerMoved) g_GameManager.augmentKeyboardFocus = false;
    previousMx = c.mx;
    previousMy = c.my;
    // Leaving the cards keeps the last focus, so hover-then-Space still works.
    if (inputReady && pointerMoved && !g_GameManager.augmentKeyboardFocus) {
        const int hovered = ac::HitTest(L, lift, (float)c.mx, (float)c.my);
        if (hovered >= 0) g_HoveredAug = hovered;
    }
    if (inputReady && lmbClick) {
        const int clicked = ac::HitTest(L, lift, (float)c.mx, (float)c.my);
        if (clicked >= 0) {
            g_HoveredAug = clicked;
            g_GameManager.augmentKeyboardFocus = false;
            if (g_AugExitT < 0.0f) {
                g_AugExitT = 0.0f;
                g_AugExitSlot = clicked;
            }
        }
    }
    const int focus = g_HoveredAug >= 0 && g_HoveredAug < count ? g_HoveredAug : -1;
    if (!inExit)
        for (int i = 0; i < count; ++i) {
            const float target = inputReady && focus == i ? 1.0f : 0.0f;
            focusT[i] += (target - focusT[i]) * (1.0f - std::exp(-10.0f * std::max(0.0f, delta)));
        }
    float focusMax = 0.0f;
    for (int i = 0; i < count; ++i) {
        lift[i] = 14.0f * ui * focusT[i];
        focusMax = std::max(focusMax, focusT[i]);
    }

    const int pick = inExit ? std::max(0, std::min(count - 1, g_AugExitSlot)) : -1;
    const float sceneA = ac::SceneAlpha(exitT);
    const float enterA = chained ? 1.0f : ac::Clamp01(t / ac::kDeckIn);
    // Another pick follows straight on: keep the backdrop up for it.
    const bool followUp = g_GameManager.augmentRewardActive && g_GameManager.PendingRewards() > 1;
    const float frameA = enterA * (followUp ? 1.0f : sceneA);
    // Dim: fades in as the pick opens, lifts while the cards vanish.
    const float dimA = (chained ? 1.0f : ac::DimIn(t)) * (followUp ? 1.0f : ac::DimOut(exitT));

    // ── backdrop: the screen's only dim (GameManager::Render skips this state),
    //    as dark as the old fixed 0.45 overlay plus the 0.52 tint together ──
    BindMainShader();
    drawRect(0.0f, 0.0f, sw, sh, 0.006f, 0.013f, 0.028f, dimA * 0.74f);
    BatchFlush();
    drawConstellFrame(18.0f * ui, 18.0f * ui, sw - 36.0f * ui, sh - 36.0f * ui,
                      kUiBlue.r, kUiBlue.g, kUiBlue.b, frameA * 0.16f,
                      24.0f * ui, 7.0f * ui, frameA * 0.10f, enterA);
    BatchFlush();

    // ── deck ──
    BindMainShader();
    const float frameA0 = frameA;
    if (!inExit && g_GlossaryOpenA < 0.99f) {
        const float frameA = frameA0 * (1.0f - g_GlossaryOpenA);   // the deck steps back for a definition
        for (int k = 3; k >= 0; --k) {
            const Xf X = MakeXf(L.deckX + k * 2.5f * ui, L.deckY - k * 2.5f * ui, L.deckS * ui, L.deckS * ui, 0.0f);
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.2f * ui, kUiBlue,
                     frameA * (k == 0 ? 0.26f : 0.14f), k == 0 ? key : 0.0f);
            if (k == 0) LCornerBrackets(X, kUiBlue, frameA * 0.80f, 3.6f * ui, 30.0f, key);
        }
        // Picks still queued after this one.
        const int queued = std::min(6, std::max(0, g_GameManager.PendingRewards() - 1));
        for (int q = 0; q < queued; ++q)
            SDiamond(L.deckX + ac::kCardW * L.deckS * ui * 0.5f + (22.0f + q * 16.0f) * ui, L.deckY,
                     5.0f * ui, kUiBlue, frameA * 0.85f, 1.3f * ui, key, true);
    }

    // ── cards: geometry ──
    struct CardDraw { float x, y, alpha, textA, conA; ac::CardPhase P; Rgb col; bool face; };
    CardDraw cd[ac::kMaxCards] = {};
    for (int i = 0; i < count; ++i) {
        if (!defs[i]) continue;
        CardDraw& d = cd[i];
        d.P = ac::PhaseAt(T, rarity, i, t);
        d.col = RarityLine(defs[i]->rarity);
        if (d.P.move <= 0.0f) continue;
        const float e = d.P.move;
        const float slotY = L.cy - lift[i];
        const float ctrlX = Lerp(L.deckX, L.cx[i], 0.75f), ctrlY = L.cy - 150.0f * ui;
        const float u = 1.0f - e;
        d.x = u * u * L.deckX + 2.0f * u * e * ctrlX + e * e * L.cx[i];
        d.y = u * u * L.deckY + 2.0f * u * e * ctrlY + e * e * slotY;
        const float scale = Lerp(L.deckS, 1.0f, e) * ui;
        const float rot = Lerp(-8.0f, 0.0f, e) * kDeg;
        d.alpha = sceneA * ac::Clamp01(1.0f - 0.45f * (focusMax - focusT[i]));
        d.textA = d.alpha;
        d.conA = d.alpha;
        if (inExit && i == pick) {
            if (exitT < ac::kPickPulse) d.alpha *= std::sin(2.0f * 3.14159265f * 18.0f * exitT) > 0.0f ? 1.0f : 0.55f;
            else d.alpha *= 1.0f - ac::Smooth01((exitT - ac::kPickPulse) / ac::kPickFade);
            d.textA = d.conA = d.alpha;
        } else if (inExit) {
            const int dist = std::abs(i - pick);
            d.textA *= ac::TextFade(exitT);
            d.conA *= ac::ConstellFade(dist, exitT);
        }

        const float flipX = std::fabs(std::cos(3.14159265f * d.P.flip));
        const float bulge = 1.0f + 0.05f * std::sin(3.14159265f * d.P.flip);
        const Xf X = MakeXf(d.x, d.y, scale * std::max(0.02f, flipX), scale * bulge, rot);
        d.face = d.P.flip >= 0.5f;
        if (!d.face) {
            DrawBack(X, d.col, d.alpha, d.P.charge, ui, key);
            continue;
        }
        const float h = focusT[i];
        if (!inExit || i == pick) {
            // A dissolving card's frame is drawn as fragments instead.
            // The frame itself stays a quiet hairline; the corner brackets carry it.
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.2f * ui, d.col,
                     d.alpha * (0.26f + 0.16f * h), key);
            LCornerBrackets(X, d.col, d.alpha * (0.80f + 0.20f * h), (3.6f + 1.0f * h) * ui, 30.0f, key);
        }
        if (d.P.flip < 1.0f) continue;

        // Rarity marks (1~5 diamonds).
        const int n = rarity[i] + 1;
        for (int k = 0; k < n; ++k)
            SDiamond(d.x + (k - (n - 1) * 0.5f) * 14.0f * ui, d.y + kRarityY * ui, 4.0f * ui, d.col,
                     d.textA * 0.9f * d.P.title, 1.2f * ui, key, true);

        // The icon sits on a node: hairline ring with four ticks. Cards without an
        // icon file get a diamond sigil in the same place instead of a hole.
        {
            const float iy = d.y + kIconY * ui, ir = (kIconSize * 0.5f + 4.0f) * ui, ia = d.textA * d.P.title;
            hud_lines::Ring(d.x, iy, ir, 1.0f * ui, d.col, ia * 0.30f, key);
            for (int k = 0; k < 4; ++k) {
                const float ang = k * 90.0f * kDeg, cs = std::cos(ang), sn = std::sin(ang);
                SLine(d.x + cs * ir, iy + sn * ir, d.x + cs * (ir + 6.0f * ui), iy + sn * (ir + 6.0f * ui),
                      1.0f * ui, d.col, ia * 0.55f, key);
            }
            if (!hasIcon[i]) {
                SDiamond(d.x, iy, 19.0f * ui, d.col, ia * 0.80f, 1.4f * ui, key);
                SDiamond(d.x, iy, 9.0f * ui, d.col, ia * 0.55f, 1.1f * ui, key, true);
            }
        }
        const float dv = ac::Clamp01(d.P.title * 1.4f);
        SLine(d.x - 92.0f * ui * dv, d.y + kDivY * ui, d.x + 92.0f * ui * dv, d.y + kDivY * ui,
              0.9f * ui, d.col, d.textA * 0.34f, key);
        SDiamond(d.x, d.y + kDivY * ui, 3.2f * ui, d.col, d.textA * 0.6f * dv, 1.0f * ui, key, true);

        // Hover: a pair of slide bars under the card with end diamonds.
        if (!inExit && h > 0.02f) {
            const float by = d.y + (ac::kCardH * 0.5f + 22.0f) * ui, ext = (18.0f + 66.0f * h) * ui;
            SLine(d.x - 14.0f * ui, by, d.x - ext, by, 1.4f * ui, d.col, 0.8f * h * d.alpha, key);
            SLine(d.x + 14.0f * ui, by, d.x + ext, by, 1.4f * ui, d.col, 0.8f * h * d.alpha, key);
            SDiamond(d.x - ext - 5.0f * ui, by, 4.0f * ui, d.col, h * d.alpha, 1.2f * ui, key);
            SDiamond(d.x + ext + 5.0f * ui, by, 4.0f * ui, d.col, h * d.alpha, 1.2f * ui, key);
        }
    }
    BatchFlush();

    // ── cards: text ──
    std::vector<LinkBox> linkBoxes;   // glossary words drawn this frame
    std::vector<RingMark> ringMarks;  // range-unit rings in the card texts
    for (int i = 0; i < count; ++i) {
        const CardDraw& d = cd[i];
        if (!defs[i] || d.P.move <= 0.0f || d.P.flip < 1.0f || d.textA <= 0.004f) continue;
        const CardText& tx = texts[i];
        const wchar_t* name = AugName(*defs[i]);
        const float titleA = d.textA * d.P.title;
        const float nameW = g_TextL.Width(name, tx.titleScale), nameH = g_TextL.Height(name, tx.titleScale);
        const float iconPx = kIconSize * ui;
        DrawIcon(AugIconTex(*defs[i]), d.x - iconPx * 0.5f, d.y + kIconY * ui - iconPx * 0.5f, iconPx, iconPx,
                 d.col.r, d.col.g, d.col.b, titleA * 0.95f);
        DrawShadowedText(g_TextL, name, d.x - nameW * 0.5f, d.y + kTitleY * ui - nameH * 0.5f,
                         tx.titleScale, kText.r, kText.g, kText.b, titleA, 0.70f);
        const float left = d.x - (ac::kCardW * 0.5f - 22.0f) * ui;
        const Rgb metaCol = Mix(d.col, kText, 0.35f);
        DrawShadowedText(g_TextS, tx.meta.c_str(), left, d.y + kMetaY * ui, tx.metaScale,
                         metaCol.r, metaCol.g, metaCol.b, d.textA * 0.92f * ac::LineAlpha(d.P, 0, t), 0.70f);
        float y = d.y + kDescY * ui;
        for (size_t j = 0; j < tx.desc.lines.size(); ++j) {
            const DescLine& l = tx.desc.lines[j];
            const float lineA = d.textA * 0.90f * ac::LineAlpha(d.P, (int)j + 1, t);
            const float lh = g_TextS.Height(l.text.c_str(), tx.descScale);
            DrawIconDescIcon(tx.desc, l, left, y, lh, lineA / 0.90f);
            // Glossary words: drawn in the card colour, underlined below.
            float px = tx.desc.TextX(l, left);
            const Rgb linkCol = Mix(d.col, kText, 0.30f);
            auto piece = [&](int a, int b, int term) {
                if (b <= a) return;
                const std::wstring s = l.text.substr((size_t)a, (size_t)(b - a));
                const Rgb pc = term >= 0 ? linkCol : kTextSub;
                const float w = DrawRingText(s, px, y, tx.descScale, pc, lineA, 0.70f, ringMarks);
                if (term >= 0) linkBoxes.push_back({ i, term, px, y, px + w, y + lh, d.col, lineA });
                px += w;
            };
            int pos = 0;
            for (const DescLink& k : l.links) {
                piece(pos, k.start, -1);
                if (k.icon) {
                    // Term icon in front of the word, in the link colour.
                    const float is = tx.desc.termIconSize;
                    DrawIcon(TermIconTex(k.term), px, y + lh * 0.5f - is * 0.5f, is, is,
                             linkCol.r, linkCol.g, linkCol.b, lineA);
                    px += tx.desc.termMarkW;
                }
                piece(k.start, k.start + k.len, k.term);
                pos = k.start + k.len;
            }
            piece(pos, (int)l.text.size(), -1);
            y += lh + 4.0f * ui;
        }
        wchar_t keyLabel[8];
        swprintf_s(keyLabel, L"[%d]", i + 1);
        const float keyScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
        const float kw = g_TextS.Width(keyLabel, keyScale), kh = g_TextS.Height(keyLabel, keyScale);
        const Rgb keyCol = Mix(kUiBlue, kText, 0.4f);
        DrawShadowedText(g_TextS, keyLabel, d.x - kw * 0.5f, d.y + 182.0f * ui - kh * 0.5f, keyScale,
                         keyCol.r, keyCol.g, keyCol.b, titleA * 0.75f, 0.70f);
    }

    // Glossary underlines, then the hovered word's definition under its card.
    {
        BindMainShader();
        for (const LinkBox& b : linkBoxes)
            SLine(b.x0, b.y1 + 1.0f * ui, b.x1, b.y1 + 1.0f * ui, 1.0f * ui, b.col, b.a * 0.70f, 0.0f);
        BatchFlush();
        DrawRingMarks(ringMarks, ui);
        float cardX[ac::kMaxCards] = {}, cardY[ac::kMaxCards] = {};
        Rgb cardCol[ac::kMaxCards] = {};
        for (int i = 0; i < count; ++i) { cardX[i] = cd[i].x; cardY[i] = cd[i].y; cardCol[i] = cd[i].col; }
        static bool glossaryHeld = false;
        const bool held = ActionBound(GameAction::Glossary) && ActionHeld(GameAction::Glossary);
        bool glossaryPress = held && !glossaryHeld;
        if (g_SmokeCapture && inputReady && g_SmokeGlossKeyPresses > 0) { --g_SmokeGlossKeyPresses; glossaryPress = true; }
        glossaryHeld = held;
        DrawGlossaryTips(linkBoxes, cardX, cardY, cardCol, count, inputReady && !inExit,
                         (float)c.mx, (float)c.my, delta, focus, glossaryPress, pointerMoved && c.mx >= 0.0, sw, sh, ui);
    }

    // Guide line, only once the cards can be picked.
    if (inputReady) {
        // The card and confirm keys follow the bindings (KeyBindings.h); unbound
        // card keys drop out, leaving the click.
        static const GameAction kCardKeys[3] = {
            GameAction::Card1, GameAction::Card2, GameAction::Card3 };
        std::wstring cardKeys;
        for (int i = 0; i < std::min(3, std::max(1, count)); ++i) {
            if (!ActionBound(kCardKeys[i])) continue;
            if (!cardKeys.empty()) cardKeys += L"/";
            cardKeys += KeyName(ActionKey(kCardKeys[i]));
        }
        const std::wstring confirmKey = KeyName(ActionKey(GameAction::Confirm));
        const int li = CurLangIdx();
        std::wstring guideText;
        if (cardKeys.empty()) {
            guideText = li == 0 ? L"카드를 클릭해 고른 뒤 " + confirmKey + L"(으)로 확정"
                      : li == 2 ? L"カードをクリックして選び、" + confirmKey + L"で確定"
                                : L"CLICK A CARD AND CONFIRM WITH " + confirmKey;
        } else {
            guideText = li == 0 ? L"카드를 클릭하거나 " + cardKeys + L" 키로 고른 뒤 " + confirmKey + L"(으)로 확정"
                      : li == 2 ? L"カードをクリック、または" + cardKeys + L"キーで選んで" + confirmKey + L"で確定"
                                : L"CLICK A CARD, OR PRESS " + cardKeys + L" AND CONFIRM WITH " + confirmKey;
        }
        if (ActionBound(GameAction::Glossary)) {
            const std::wstring termKey = KeyName(ActionKey(GameAction::Glossary));
            guideText += li == 0 ? L" · " + termKey + L" 용어 설명"
                       : li == 2 ? L" · " + termKey + L"で用語説明"
                                 : L" · " + termKey + L" FOR TERMS";
        }
        const wchar_t* guide = guideText.c_str();
        const float gs = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
        const float gw = g_TextS.Width(guide, gs);
        DrawShadowedText(g_TextS, guide, sw * 0.5f - gw * 0.5f, L.cy - L.cardH * 0.5f - 64.0f * ui, gs,
                         0.58f, 0.70f, 0.84f, 0.80f, 0.70f);
    }

    // ── legendary+ flip shards ──
    BindMainShader();
    for (int i = 0; i < count; ++i) {
        if (burst[i] || !ac::IsHighRarity(rarity[i]) || t < T.flipStart[i] + 0.12f) continue;
        burst[i] = true;
        for (int k = 0; k < 8; ++k) {
            const float a = k * 45.0f * kDeg + 0.2f;
            shards[i * 8 + k] = { L.cx[i] + std::cos(a) * 110.0f * ui, L.cy + std::sin(a) * 160.0f * ui,
                                  std::cos(a) * 320.0f * ui, std::sin(a) * 320.0f * ui, 0.4f, cd[i].col };
        }
    }
    for (Shard& s : shards) {
        if (s.life <= 0.0f) continue;
        s.life -= delta;
        s.x += s.vx * delta;
        s.y += s.vy * delta;
        SLine(s.x, s.y, s.x - s.vx * 0.06f, s.y - s.vy * 0.06f, 1.6f * ui, s.c,
              ac::Clamp01(s.life / 0.4f) * sceneA, key);
    }

    if (inExit) {
        // ── the pick: its nodes fly to the skill row ──
        const float tx = sw * 0.5f, ty = sh - BottomInset() - 70.0f * ui;
        if (defs[pick]) {
            const CardDraw& d = cd[pick];
            const ac::Shape& shp = ac::ShapeOf(ac::FamilyFromCode(defs[pick]->name));
            for (int k = 0; k < shp.nv; ++k) {
                const float p = ac::Clamp01((exitT - ac::kAbsorbStart - ac::kAbsorbStep * k) / ac::kAbsorbDur);
                if (p <= 0.0f || p >= 1.0f) continue;
                float lx, ly;
                ConNode(shp, k, lx, ly);
                const float x0 = L.cx[pick] + lx * ui, y0 = L.cy - lift[pick] + ly * ui;
                const float e1 = ac::QuintIn(p), e2 = ac::QuintIn(p - 0.12f);
                const float x = Lerp(x0, tx, e1), y = Lerp(y0, ty, e1);
                SLine(Lerp(x0, tx, e2), Lerp(y0, ty, e2), x, y, 1.2f * ui, d.col, 0.7f * sceneA, key);
                SDiamond(x, y, 3.2f * ui, d.col, 0.95f * sceneA, 1.2f * ui, key, true);
            }
            const float arrive = ac::kAbsorbStart + ac::kAbsorbDur;
            if (exitT >= arrive) {
                const float ft = (exitT - arrive) / ac::kAbsorbRing;
                if (ft < 1.0f)
                    hud_lines::Ring(tx, ty, (24.0f + ft * 50.0f) * ui, 1.6f * ui, d.col, (1.0f - ft) * 0.9f * sceneA, key);
            }
        }

        // ── the rest: frame fragments and rising node seeds ──
        float corx[8], cory[8];
        ac::ChamferCorners(ac::kCardW, ac::kCardH, ac::kCut, corx, cory);
        for (int i = 0; i < count; ++i) {
            if (i == pick || !defs[i] || cd[i].P.move <= 0.0f) continue;
            const int dist = std::abs(i - pick);
            const Rgb col = cd[i].col;
            const float cx = L.cx[i], cy = L.cy - lift[i];
            unsigned frag = 0;
            for (int k = 0; k < 8; ++k) {
                const float ax = corx[k], ay = cory[k], bx = corx[(k + 1) % 8], by = cory[(k + 1) % 8];
                const float len = std::hypot(bx - ax, by - ay);
                const int pieces = std::max(1, (int)std::ceil(len / ac::kFragLen));
                for (int m = 0; m < pieces; ++m, ++frag) {
                    const float x0 = Lerp(ax, bx, (float)m / pieces), y0 = Lerp(ay, by, (float)m / pieces);
                    const float x1 = Lerp(ax, bx, (float)(m + 1) / pieces), y1 = Lerp(ay, by, (float)(m + 1) / pieces);
                    const unsigned seed = (unsigned)seenSerial * 7919u + (unsigned)i * 131u + frag * 17u;
                    const float age = exitT - ac::FragStart(dist, seed);
                    const float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
                    if (age < 0.0f) {   // still part of the frame
                        SLine(cx + x0 * ui, cy + y0 * ui, cx + x1 * ui, cy + y1 * ui, 1.2f * ui, col, 0.26f * sceneA, key);
                        continue;
                    }
                    if (age >= ac::kFragLife) continue;
                    const float u = age / ac::kFragLife;
                    const float nl = std::max(1.0f, std::hypot(mx, my));
                    const float vx = mx / nl * 70.0f + (ac::Hash01(seed + 1) - 0.5f) * 40.0f;
                    const float vy = my / nl * 50.0f - 40.0f - ac::Hash01(seed + 2) * 50.0f;
                    const float ang = std::atan2(y1 - y0, x1 - x0) + (ac::Hash01(seed + 3) - 0.5f) * 5.0f * age;
                    const float half = std::hypot(x1 - x0, y1 - y0) * 0.5f * (1.0f - 0.6f * u);
                    const float px = cx + (mx + vx * age) * ui, py = cy + (my + vy * age - 40.0f * age * age) * ui;
                    SLine(px - std::cos(ang) * half * ui, py - std::sin(ang) * half * ui,
                          px + std::cos(ang) * half * ui, py + std::sin(ang) * half * ui,
                          1.4f * (1.0f - 0.4f * u) * ui, Mix(col, kWhite, u * 0.7f),
                          0.62f * std::pow(1.0f - u, 1.4f) * sceneA, key);
                }
            }
            // Each node lifts off, rises and settles as a star.
            const ac::Shape& shp = ac::ShapeOf(ac::FamilyFromCode(defs[i]->name));
            for (int k = 0; k < shp.nv; ++k) {
                const float age = exitT - ac::SeedStart(dist, k);
                if (age < 0.0f) continue;
                float lx, ly;
                ConNode(shp, k, lx, ly);
                const unsigned seed = (unsigned)seenSerial * 104729u + (unsigned)i * 977u + (unsigned)k * 31u;
                const float vx = (ac::Hash01(seed) - 0.5f) * 50.0f, vy = -70.0f - ac::Hash01(seed + 1) * 70.0f;
                if (age >= ac::kSeedLife) continue;
                const float u = age / ac::kSeedLife;
                const float x = cx + (lx + vx * age) * ui, y = cy + (ly + vy * age) * ui;
                SLine(x - vx * 0.08f * ui, y - vy * 0.08f * ui, x, y, 1.0f * ui, col, 0.45f * (1.0f - u) * sceneA, key);
                // Whitens as it rises, then goes out into the stars.
                const Rgb sc = Mix(col, kWhite, u);
                drawCircle(x, y, (2.4f - 1.2f * u) * ui, sc.r, sc.g, sc.b,
                           0.9f * (1.0f - ac::Smooth01((u - 0.55f) / 0.45f)) * sceneA);
            }
        }
    }
    BatchFlush();
}

void Scene_AugSelect(const SceneCtx& c) {
    Scene_AugSelectCards(c);
}
