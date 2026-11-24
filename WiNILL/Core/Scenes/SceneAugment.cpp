#include "Scenes.h"
#include <glad/glad.h>
#include <cwchar>
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
#include "SynergyUpgrade.h"
#include "../../System/Audio.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "../../Render/HudLines.h"
#include "../../Render/AugCardModel.h"
#include "../../Render/Juice.h"
#include "../../Render/StatIcons.h"
#include "Glossary.h"
#include "TutorialText.h"
#include "../ReadyLaunch.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cwchar>
#include <cstdlib>

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
            // A run with no spaces (Japanese, Chinese) breaks between any two
            // characters, but not where a line would start with closing
            // punctuation or a unit ("、", ")", "%", "○"), end on an opening
            // bracket, or split a number ("6." / "5○", "+1" / "5%").
            const auto noStart = [](wchar_t c) {
                return std::wcschr(L"、。，．,.:;!?)]）］」』〕〉》%％○…ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ", c) != nullptr;
            };
            const auto noEnd = [](wchar_t c) { return std::wcschr(L"([（［「『〔〈《", c) != nullptr; };
            const auto numJoin = [](wchar_t a, wchar_t b) {
                const bool da = (a >= L'0' && a <= L'9') || a == L'.' || a == L'+' || a == L'-';
                const bool db = (b >= L'0' && b <= L'9') || b == L'.';
                return da && db;
            };
            std::wstring part;
            for (wchar_t ch : word) {
                const std::wstring next = part + ch;
                if (!part.empty() &&
                    WrapWidth(next, scale) > maxWidth) {
                    size_t b = part.size();   // next[b..] moves down
                    while (b > 0 && (noStart(next[b]) || numJoin(next[b - 1], next[b]) || noEnd(next[b - 1])))
                        --b;
                    if (b == 0) b = part.size();   // nowhere better: break where it overflowed
                    lines.push_back(part.substr(0, b));
                    part.erase(0, b);
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

void DrawDescLinePieces(const IconDesc& desc, const DescLine& line, float x, float y, float lineH,
                        float scale, float r, float g, float b, float a,
                        const std::function<void(const wchar_t*, float, int, int)>& drawText,
                        std::vector<GlossWord>* words) {
    float px = x;
    int pos = 0, piece = 0;
    auto run = [&](int end, int term) {
        if (end <= pos) return;
        const std::wstring s = line.text.substr((size_t)pos, (size_t)(end - pos));
        drawText(s.c_str(), px, piece++, term);
        const float w = g_TextS.Width(s.c_str(), scale);
        if (term >= 0 && words) words->push_back({ term, px, y, px + w, y + lineH });
        px += w;
        pos = end;
    };
    for (const DescLink& k : line.links) {
        run(k.start, -1);
        if (k.icon) {
            const float is = desc.termIconSize;
            DrawIcon(TermIconTex(k.term), px, y + lineH * 0.5f - is * 0.5f, is, is, r, g, b, a);
            px += desc.termMarkW;
        }
        run(k.start + k.len, k.term);
    }
    run((int)line.text.size(), -1);
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
// The pick guide's "rarity & text" page: the middle card's tier stars, large.
constexpr float kGuideMarkR = 10.0f, kGuideMarkGap = 24.0f;
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
    "Pull", "Return", "Erase", "Graze", "Shield", "Still", "Crit", "CritMult", "HitCount", "DmgKinds",
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
// Five-point star outline; `fill` also fills it (rarity and upgrade stars).
void SStar(float x, float y, float r, Rgb c, float a, float w, float key, bool fill) {
    if (a <= 0.002f) return;
    float px[11], py[11];
    ac::StarCorners(x, y, r, px, py);
    hud_lines::ClosedOutline(px, py, 10, w, c, a, key);
    if (fill) {
        px[10] = px[0]; py[10] = py[0];
        hud_lines::FanFill(x, y, px, py, 11, c, a * 0.85f);
    }
}

// The card's tier row (AugCardModel.h): 8 empty stars, the rarity's stars
// filled, taken upgrades as whitened stars that breathe slowly, and on an
// upgrade pick the slot this branch fills twinkling with a turning cross glint.
// `title` (0..1) fades the row in, the filled stars lighting left to right.
void DrawTierStars(float cx, float cy, float ui, float gap, float r, int rarity, int upgrades,
                   bool preview, Rgb col, float a, float title, float now, float key) {
    constexpr float kTau = 6.2831853f;
    constexpr float kBreath = 1.6f, kTwinkle = 0.9f;   // seconds per cycle
    const Rgb upCol = Mix(col, kWhite, 0.80f);
    const float rr = r * ui, w = 1.1f * ui;
    // Tier colours for repainted slots (2026-11-24): the rarity colours, then the supernova.
    auto tierCol = [&](int tier) {
        return tier >= ac::kTierSupernova ? kWhite : RarityLine((AugRarity)std::max(0, std::min(4, tier)));
    };
    for (int k = 0; k < ac::kTierSlots; ++k) {
        const float x = cx + ac::TierSlotX(k, gap) * ui;
        const float on = ac::Clamp01(title * 1.8f - 0.10f * k);
        const ac::TierSlotInfo slot = ac::TierSlotAt(k, rarity, upgrades, preview);
        switch (slot.kind) {
        case ac::TierSlot::Refill: {
            const Rgb c = tierCol(slot.tier);
            if (slot.tier >= ac::kTierSupernova) {
                // Supernova: a white star with a short cross of light that breathes.
                const float breath = 0.5f + 0.5f * std::sin(now * kTau / kBreath + k * 0.9f);
                SStar(x, cy, rr, c, a * on, w, key, true);
                const float g0 = rr * 1.15f, g1 = rr * (1.55f + 0.35f * breath);
                for (int q = 0; q < 4; ++q) {
                    const float ang = (45.0f + q * 90.0f) * kDeg, cs = std::cos(ang), sn = std::sin(ang);
                    SLine(x + cs * g0, cy + sn * g0, x + cs * g1, cy + sn * g1, 0.8f * ui, c,
                          a * on * (0.35f + 0.45f * breath), key);
                }
            } else {
                SStar(x, cy, rr, c, a * 0.95f * on, w, key, true);
            }
            break; }
        case ac::TierSlot::Empty:
            SStar(x, cy, rr, col, a * 0.32f * title, w, key, false);
            break;
        case ac::TierSlot::Rarity:
            SStar(x, cy, rr, col, a * 0.92f * on, w, key, true);
            break;
        case ac::TierSlot::Upgrade: {
            const float breath = 0.86f + 0.14f * std::sin(now * kTau / kBreath + k * 0.7f);
            SStar(x, cy, rr, upCol, std::min(1.0f, a * breath * on * 1.08f), w, key, true);
            break; }
        case ac::TierSlot::Preview: {
            // The slot it fills (or repaints, in the next tier's colour) twinkles.
            const Rgb pc = slot.tier >= 0 ? tierCol(slot.tier) : kWhite;
            SStar(x, cy, rr, col, a * 0.32f * title, w, key, false);
            const float p = 0.5f + 0.5f * std::sin(now * kTau / kTwinkle);
            SStar(x, cy, rr * (0.70f + 0.45f * p), pc, a * (0.25f + 0.75f * p) * on, w, key, true);
            const float u = std::fmod(now / kTwinkle, 1.0f);
            const float ga = a * on * (u < 0.4f ? u / 0.4f : (1.0f - u) / 0.6f);
            const float g0 = rr * (1.25f + 0.30f * u), g1 = rr * (1.80f + 0.40f * u);
            for (int q = 0; q < 4; ++q) {
                const float ang = (u * 90.0f + q * 90.0f) * kDeg, cs = std::cos(ang), sn = std::sin(ang);
                SLine(x + cs * g0, cy + sn * g0, x + cs * g1, cy + sn * g1, 0.9f * ui, kWhite, ga, key);
            }
            break; }
        }
    }
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
CardText LayoutCardText(const AugDef& def, float ui, int slot = -1) {
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
    // Augment rework: on a wave clear's upgrade pick the card shows which branch it offers and
    // what it does first ("강화 · 수치 A", "▲ 튕김 2 → 3" / "◆ <shape>"), then its own text. The
    // star it adds twinkles in the tier row at the top of the card.
    const int augIdx = (int)(&def - ALL_AUGS);
    std::wstring descText = AugDesc(def);
    const int branch = slot >= 0 && slot < GameManager::kAugChoiceMax ? g_GameManager.augChoiceBranch[slot] : -1;
    if (g_GameManager.ActiveIsUpgrade() && branch >= 0) {
        static const wchar_t* kUp[3] = { L"강화", L"Upgrade", L"強化" };
        const int li = CurLangIdx();
        // The level shows in the star row (and its "+n" once the stars are full), not here.
        t.meta = std::wstring(kUp[li]) + L" · " + synergy::BranchName(branch, li);
        if (branch == 2)
            t.meta += std::wstring(L" (") + synergy::FamilyName((synergy::Family)syn::CardFamilyOf(augIdx), li) + L")";
        const std::wstring line = syn::UpgradeLine(augIdx, branch, li);
        if (!line.empty()) descText = (branch == 2 ? L"◆ " : L"▲ ") + line + L" · " + descText;
    } else if (g_GameManager.ActiveIsFrame()) {
        static const wchar_t* kFrame[3] = { L"시작 무기 · 이번 런 동안 유지", L"Starting frame · kept all run", L"開始武器 · このラン中ずっと" };
        t.meta = kFrame[CurLangIdx()];
    }
    t.metaScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    const float room = (kDescBottom - kDescY) * ui;
    const float floorScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    float d = ui;
    for (;;) {
        t.descScale = UiTextScale(g_TextS, UiTextLevel::Description, d);
        t.desc = WrapIconDesc(descText.c_str(), t.descScale, inner, ui, true);
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
    float age = 0.0f;                 // seconds since it opened (the reveal below)
};
constexpr int kTipLevels = 3;
// Opening a definition: the leader draws out from the word, the title fades in
// and its underline grows, the definition types itself in, then the related
// words appear. About a third of a second in all; a level that stays open does
// not replay it.
constexpr float kTipLeaderSec = 0.14f, kTipTitleAt = 0.08f, kTipTitleSec = 0.14f;
constexpr float kTipTypeAt = 0.14f, kTipTypeCps = 240.0f, kTipRelSec = 0.15f;
inline float TipEase(float t) { t = std::clamp(t, 0.0f, 1.0f); return 1.0f - (1.0f - t) * (1.0f - t); }
// How open the definitions are (0..1): the deck fades out while they show.
float g_GlossaryOpenA = 0.0f;
// Off the cards (DrawGlossaryHover: codex, shop) the definitions open under a
// block of text instead of a card.
bool g_TipOffCards = false;

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
    if (g_SmokeCapture && g_SmokeGlossDepth > 0 && keyCard >= 0) {
        // Smoke capture: rest the pointer on the focused card's first word,
        // then on the first related word of each open level, down to the asked
        // depth. While the next word is not drawn yet (still typing in), the
        // pointer stays where it was.
        static float lastX = -1.0f, lastY = -1.0f;
        const LinkBox* aim = nullptr;
        const int want = std::min(open, g_SmokeGlossDepth - 1);
        for (const LinkBox& b : (want == 0 ? cardLinks : relLinks))
            if (want == 0 ? b.owner == keyCard : b.owner == want - 1) { aim = &b; break; }
        if (aim) { lastX = (aim->x0 + aim->x1) * 0.5f; lastY = (aim->y0 + aim->y1) * 0.5f; }
        if (lastX >= 0.0f) { mx = lastX; my = lastY; }
    }

    auto inBox = [&](const LinkBox& b) {
        return mx >= b.x0 - 2.0f * ui && mx <= b.x1 + 2.0f * ui && my >= b.y0 - 2.0f * ui && my <= b.y1 + 4.0f * ui;
    };
    // On the augment pick a definition is as wide as a card, so the three
    // levels line up under the three card slots.
    const float tipW = (g_TipOffCards ? ac::kCardW + 40.0f : ac::kCardW) * ui;
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
    // The cards keep moving after a word opens (the deal, the focused card
    // growing): keep each open word's bold underline on the word as drawn now,
    // not where it was in the frame it was hovered (it hung over the next line).
    for (int k = 0; k < open; ++k) {
        TipLevel& tl = tips[k];
        const std::vector<LinkBox>& src = k == 0 ? cardLinks : relLinks;
        const LinkBox* best = nullptr;
        float bestD = 1e30f;
        for (const LinkBox& b : src) {
            if (b.term != tl.term || b.owner != (k == 0 ? tl.card : k - 1)) continue;
            const float d = std::fabs(b.x0 - tl.ax0) + std::fabs(b.y1 - tl.ay);   // the same occurrence
            if (d < bestD) { bestD = d; best = &b; }
        }
        if (best) {
            tl.ax0 = best->x0;
            tl.ax1 = best->x1;
            tl.ay = best->y1 + (k == 0 ? 1.0f : 2.0f) * ui;
        }
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
    struct IconCmd { GLuint tex; float x, y, size; Rgb c; float a = 1.0f; };
    std::vector<TextCmd> texts;
    std::vector<IconCmd> icons;
    BindMainShader();
    for (int k = 0; k < open; ++k) {
        TipLevel& tl = tips[k];
        tl.age += std::max(0.0f, delta);
        if (g_SmokeCapture) {
            // Smoke: ASTERIA_SMOKE_TIPAGE=<sec> holds every definition at that
            // point of its opening, to capture the reveal mid-way.
            static const float frozen = [] {
                char* v = nullptr; size_t len = 0;
                float f = -1.0f;
                if (_dupenv_s(&v, &len, "ASTERIA_SMOKE_TIPAGE") == 0 && v) f = (float)std::atof(v);
                std::free(v);
                return f;
            }();
            if (frozen >= 0.0f) tl.age = frozen;
        }
        const float leaderT = TipEase(tl.age / kTipLeaderSec);
        const float titleA = TipEase((tl.age - kTipTitleAt) / kTipTitleSec);
        // A polyline drawn from its start up to leaderT of its length.
        auto leader = [&](const float* px, const float* py, int n, Rgb lc) {
            float total = 0.0f;
            for (int i = 1; i < n; ++i) total += std::hypot(px[i] - px[i - 1], py[i] - py[i - 1]);
            float left = total * leaderT;
            for (int i = 1; i < n && left > 0.0f; ++i) {
                const float seg = std::hypot(px[i] - px[i - 1], py[i] - py[i - 1]);
                const float f = seg > 0.0f ? std::min(1.0f, left / seg) : 1.0f;
                SLine(px[i - 1], py[i - 1], px[i - 1] + (px[i] - px[i - 1]) * f, py[i - 1] + (py[i] - py[i - 1]) * f,
                      1.0f * ui, lc, 0.70f, 0.0f);
                left -= seg;
            }
        };
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
        // One row read left to right: level 0 first, each related level to
        // the right of the one it came from. On the pick the row has fixed
        // columns under the card slots; off the cards it starts under the
        // text and slides left as a whole when it would not fit.
        {
            const float m = 24.0f * ui, gap = 24.0f * ui;
            // Off the cards (codex, shop) the row only makes room for the levels
            // open, so one definition stays right under the text it explains.
            const int rowN = g_TipOffCards ? open : kTipLevels;
            const float rowW = (float)rowN * tl.w + (float)(rowN - 1) * gap;
            if (g_TipOffCards) {
                float startX = cardX[tl.card] - ac::kCardW * 0.5f * ui;
                startX = std::max(m, std::min(startX, sw - m - rowW));
                tl.x = startX + (float)k * (tl.w + gap);
            } else {
                // Fixed columns on the pick, whichever card was hovered: level
                // 0 under the first card slot, 1 under the second, 2 under the
                // third (past the last card when fewer are dealt).
                const float pitch = (ac::kCardW + ac::kGap) * ui;
                const int last = std::max(0, count - 1);
                const float cx = k <= last ? cardX[k] : cardX[last] + (float)(k - last) * pitch;
                tl.x = std::clamp(cx - tl.w * 0.5f, m, std::max(m, sw - m - tl.w));
            }
            tl.y = cardY[tl.card] + (ac::kCardH * 0.5f + 52.0f) * ui;
            tl.side = k > 0;
        }
        tl.y = std::min(tl.y, sh - 24.0f * ui - tl.h);

        // The word that opened it gets a bold underline. The leader leaves a
        // card from its bottom edge (a line through the card would cross the
        // lines under the word), a related word from its own underline.
        const float ax = (tl.ax0 + tl.ax1) * 0.5f;
        SLine(tl.ax0, tl.ay, tl.ax1, tl.ay, 2.2f * ui, col, 1.0f, 0.0f);
        const float ly = tl.y - 6.0f * ui;
        if (k == 0) {
            const float y0 = cardY[tl.card] + (ac::kCardH * 0.5f + 4.0f) * ui;
            const float px[3] = { ax, ax, tl.x + pad }, py[3] = { y0, ly, ly };
            leader(px, py, 3, col);
            drawCircle(ax, y0, 2.0f * ui, col.r, col.g, col.b, 0.9f);
        } else if (!tl.side) {
            const float px[3] = { ax, ax, tl.x + pad }, py[3] = { tl.ay, ly, ly };
            leader(px, py, 3, col);
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
            const float px[5] = { ax, ax, gx, gx, onLeft ? tEnd : tl.x + pad - 4.0f * ui };
            const float py[5] = { tl.ay, ry, ry, titleMid, titleMid };
            leader(px, py, 5, col);
        }
        // Title, underlined; then the definition; then related words.
        const wchar_t* title = e.name[lang];
        const float tw = g_TextS.Width(title, titleScale);
        float ty = tl.y + 2.0f * ui;
        const GLuint iconTex = TermIconTex(tl.term);
        const float iconSize = iconTex ? titleH * 1.25f : 0.0f;
        const float titleX = tl.x + pad + (iconTex ? iconSize + 6.0f * ui : 0.0f);
        const Rgb titleCol = Mix(col, kText, 0.45f);
        if (iconTex) icons.push_back({ iconTex, tl.x + pad, ty + titleH * 0.5f - iconSize * 0.5f, iconSize, titleCol, titleA });
        if (titleA > 0.0f) {
            texts.push_back({ title, titleX, ty, titleScale, titleCol, titleA });
            const float ux0 = tl.x + pad, ux1 = titleX + tw + 6.0f * ui;
            SLine(ux0, ty + titleH + 3.0f * ui, ux0 + (ux1 - ux0) * titleA, ty + titleH + 3.0f * ui,
                  1.0f * ui, col, 0.65f, 0.0f);
        }
        ty += titleH + 10.0f * ui;
        // The definition types in, line after line.
        float budget = (tl.age - kTipTypeAt) * kTipTypeCps;
        size_t bodyChars = 0;
        for (const std::wstring& line : body) {
            bodyChars += line.size();
            if (budget > 0.0f) {
                const size_t n = std::min(line.size(), (size_t)budget);
                if (n > 0) texts.push_back({ line.substr(0, n), tl.x + pad, ty, bodyScale, kText, 0.95f });
                budget -= (float)line.size();
            }
            ty += bodyH + 3.0f * ui;
        }
        const float relA = TipEase((tl.age - kTipTypeAt - (float)bodyChars / kTipTypeCps) / kTipRelSec);
        if (e.relCount > 0 && relA > 0.0f) {
            ty += 7.0f * ui;
            float rx = tl.x + pad;
            texts.push_back({ kRelated[lang], rx, ty, bodyScale, kTextSub, 0.70f * relA });
            rx += g_TextS.Width(kRelated[lang], bodyScale) + 12.0f * ui;
            for (int r = 0; r < e.relCount; ++r) {
                const int rt = (int)e.rel[r];
                const wchar_t* rn = glossary::Get(e.rel[r]).name[lang];
                const float rw = g_TextS.Width(rn, bodyScale);
                const bool hot = k + 1 < open && tips[k + 1].term == rt;
                texts.push_back({ rn, rx, ty, bodyScale, Mix(col, kText, hot ? 0.55f : 0.35f), relA });
                SLine(rx, ty + bodyH + 2.0f * ui, rx + rw, ty + bodyH + 2.0f * ui,
                      (hot ? 1.4f : 1.0f) * ui, col, (hot ? 0.9f : 0.6f) * relA, 0.0f);
                relLinks.push_back({ k, rt, rx, ty, rx + rw, ty + bodyH, col, 1.0f });
                rx += rw + 14.0f * ui;
            }
        }
    }
    BatchFlush();
    for (const IconCmd& ic : icons) DrawIcon(ic.tex, ic.x, ic.y, ic.size, ic.size, ic.c.r, ic.c.g, ic.c.b, ic.a);
    std::vector<RingMark> rings;
    for (const TextCmd& tc : texts) DrawRingText(tc.s, tc.x, tc.y, tc.scale, tc.c, tc.a, 0.85f, rings);
    DrawRingMarks(rings, ui);
}

}  // namespace

void DrawGlossaryHover(int context, const std::vector<GlossWord>& words, float tipX, float tipY,
                       float r, float g, float b, bool active, float mx, float my, float delta,
                       float sw, float sh, float ui) {
    // The definitions keep their state between frames; a different screen
    // (or the cards) starts closed instead of showing what was open there.
    static int lastContext = -1;
    const std::vector<LinkBox> none;
    if (context != lastContext) {
        lastContext = context;
        DrawGlossaryTips(none, nullptr, nullptr, nullptr, 0, false, 0, 0, 0, -1, false, false, sw, sh, ui);
    }
    const Rgb col{ r, g, b };
    std::vector<LinkBox> links;
    links.reserve(words.size());
    for (const GlossWord& w : words) links.push_back({ 0, w.term, w.x0, w.y0, w.x1, w.y1, col, 1.0f });
    if (active) {
        BindMainShader();
        for (const LinkBox& lb : links)
            SLine(lb.x0, lb.y1 + 1.0f * ui, lb.x1, lb.y1 + 1.0f * ui, 1.0f * ui, col, 0.70f, 0.0f);
        BatchFlush();
    }
    // One "card" whose first level opens at tipX, tipY (DrawGlossaryTips
    // hangs it kCardH/2 + 52 under the card centre, kCardW/2 left of it). The
    // leader leaves from that card's bottom edge, 48 above tipY, so callers
    // keep tipY at least 48 under their last line of text: the leader never
    // crosses it.
    const float cx = tipX + aug_card::kCardW * 0.5f * ui;
    const float cy = tipY - (aug_card::kCardH * 0.5f + 52.0f) * ui;
    g_TipOffCards = true;
    DrawGlossaryTips(links, &cx, &cy, &col, 1, active, mx, my, delta, -1, false, true, sw, sh, ui);
    g_TipOffCards = false;
}

static void Scene_AugSelectCards(const SceneCtx& c) {
    const float sw = c.sw, sh = c.sh, delta = c.delta;
    const int count = std::max(0, std::min(ac::kMaxCards, g_GameManager.augChoiceCount));
    const bool inExit = g_AugExitT >= 0.0f;
    const float exitT = inExit ? g_AugExitT : 0.0f;
    const float now = (float)glfwGetTime();
    const ac::Layout L = ac::MakeLayout(sw, sh, count, ready_launch::g_SelectionUp);
    const float ui = L.s;
    // The tutorial draws its cards larger (kTutorialBoost); words that are not
    // on a card (the guide's caption, the key line) keep the plain size.
    const bool tutorialPick = ready_launch::g_SelectionUp;
    const float baseUi = tutorialPick ? ac::MakeLayout(sw, sh, count).s : ui;
    const float key = 2.6f * ui;

    int rarity[ac::kMaxCards] = {};
    int tierUps[ac::kMaxCards] = {};        // upgrade branches the card has taken
    bool tierPreview[ac::kMaxCards] = {};   // an upgrade pick: the star it adds twinkles
    bool hasIcon[ac::kMaxCards] = {};
    const AugDef* defs[ac::kMaxCards] = {};
    for (int i = 0; i < count; ++i) {
        const int idx = g_GameManager.augChoices[i];
        defs[i] = (idx >= 0 && idx < AUG_TOTAL) ? &ALL_AUGS[idx] : nullptr;
        rarity[i] = defs[i] ? (int)defs[i]->rarity : 0;
        if (defs[i]) {
            tierUps[i] = std::max(0, syn::Stage(idx) - 1);
            tierPreview[i] = g_GameManager.ActiveIsUpgrade() && i < GameManager::kAugChoiceMax &&
                             g_GameManager.augChoiceBranch[i] >= 0;
        }
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
            texts.push_back(defs[i] ? LayoutCardText(*defs[i], ui, i) : CardText{});
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
    // Tutorial card pick: its guide cards come up once the cards are dealt and
    // hold the pick until they are read (ReadyLaunch.h PickGuide).
    const bool pickGuide = ready_launch::PickGuideActive() && !inExit;
    const bool pickGuideUp = pickGuide && inputReady;
    if (pickGuideUp) ready_launch::g_PickGuideT += delta;
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
    if (inputReady && !pickGuide && pointerMoved && !g_GameManager.augmentKeyboardFocus) {
        const int hovered = ac::HitTest(L, lift, (float)c.mx, (float)c.my);
        if (hovered >= 0) g_HoveredAug = hovered;
    }
    if (pickGuideUp && lmbClick) {
        // A click moves the guide on; it never takes a card.
        const ready_launch::PickGuide before = ready_launch::g_PickGuide;
        if (ready_launch::PickGuideConfirm() && ready_launch::g_PickGuide != before)
            Audio::Play(Audio::Cue::UiConfirm);
    } else if (inputReady && !pickGuide && lmbClick) {
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
    // Tutorial guide highlight on the cards themselves: on "pick a card" they
    // rise and brighten one after another in a slow wave; on "rarity & text"
    // the middle card stays lit.
    float guideGlow[ac::kMaxCards] = {};
    const float guideIn = pickGuideUp ? ac::Clamp01(ready_launch::g_PickGuideT / 0.3f) : 0.0f;
    if (pickGuideUp) {
        for (int i = 0; i < count; ++i) {
            if (ready_launch::g_PickGuide == ready_launch::PickGuide::Cards) {
                const float w = 0.5f + 0.5f * std::sin(now * 2.4f - 1.1f * (float)i);
                guideGlow[i] = w * guideIn;
                lift[i] += 10.0f * ui * w * guideIn;
            } else if (ready_launch::g_PickGuide == ready_launch::PickGuide::Detail && i == count / 2) {
                guideGlow[i] = 0.8f * guideIn;
            }
        }
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
    // (The tutorial's single level-up pick has no deck: the key line sits there.)
    if (!inExit && !pickGuideUp && !tutorialPick && g_GlossaryOpenA < 0.99f) {   // the guide's caption takes the deck's place
        // Picks still queued after this one. With none left the deck has only
        // dealt this hand, so it fades once the cards are face up.
        const int queued = std::min(6, std::max(0, g_GameManager.PendingRewards() - 1));
        const float deckLife = queued > 0 ? 1.0f : 1.0f - ac::Smooth01((t - T.end) / 0.40f);
        const float frameA = frameA0 * (1.0f - g_GlossaryOpenA) * deckLife;   // the deck steps back for a definition
        for (int k = 3; k >= 0; --k) {
            const Xf X = MakeXf(L.deckX + k * 2.5f * ui, L.deckY - k * 2.5f * ui, L.deckS * ui, L.deckS * ui, 0.0f);
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.2f * ui, kUiBlue,
                     frameA * (k == 0 ? 0.26f : 0.14f), k == 0 ? key : 0.0f);
            if (k == 0) LCornerBrackets(X, kUiBlue, frameA * 0.80f, 3.6f * ui, 30.0f, key);
        }
        for (int q = 0; q < queued; ++q)
            SDiamond(L.deckX + ac::kCardW * L.deckS * ui * 0.5f + (22.0f + q * 16.0f) * ui, L.deckY,
                     5.0f * ui, kUiBlue, frameA * 0.85f, 1.3f * ui, key, true);
        // Says what the deck is: the picks waiting after this one.
        if (queued > 0) {
            static const wchar_t* kLeft[3] = { L"남은 선택 %d", L"PICKS LEFT %d", L"残り %d" };
            wchar_t buf[48];
            swprintf_s(buf, kLeft[CurLangIdx()], queued);
            const float ls = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
            const float lw = g_TextS.Width(buf, ls);
            const Rgb lc = Mix(kUiBlue, kText, 0.45f);
            BatchFlush();
            DrawShadowedText(g_TextS, buf, L.deckX - lw * 0.5f,
                             L.deckY + (ac::kCardH * L.deckS * 0.5f + 12.0f) * ui, ls,
                             lc.r, lc.g, lc.b, frameA * 0.90f, 0.70f);
            BindMainShader();
        }
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
        // Unfocused cards: the frame steps back, the words stay readable.
        const float unfocus = focusMax - focusT[i];
        d.alpha = sceneA * ac::Clamp01(1.0f - 0.45f * unfocus);
        d.textA = sceneA * ac::Clamp01(1.0f - 0.15f * unfocus);
        // The guide's rarity-and-text card is about the middle card: the
        // others step back (alpha, no veil).
        if (pickGuideUp && ready_launch::g_PickGuide == ready_launch::PickGuide::Detail && i != count / 2) {
            const float back = 1.0f - 0.7f * ac::Clamp01(ready_launch::g_PickGuideT / 0.3f);
            d.alpha *= back;
            d.textA *= back;
        }
        d.conA = d.alpha;
        // "Choose": the card faces step back so the [n] keys read first.
        if (pickGuideUp && ready_launch::g_PickGuide == ready_launch::PickGuide::Keys)
            d.textA *= 1.0f - 0.55f * guideIn;
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
        const float h = std::max(focusT[i], guideGlow[i]);
        if (!inExit || i == pick) {
            // A dissolving card's frame is drawn as fragments instead.
            // The frame itself stays a quiet hairline; the corner brackets carry it.
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.2f * ui, d.col,
                     d.alpha * (0.26f + 0.16f * h), key);
            LCornerBrackets(X, d.col, d.alpha * (0.80f + 0.20f * h), (3.6f + 1.0f * h) * ui, 30.0f, key);
        }
        if (d.P.flip < 1.0f) continue;

        // Tier stars (8 slots: rarity + upgrades). On the pick guide's "rarity & text"
        // page the middle card's row is drawn large in gold further down.
        const bool guideMarks = pickGuideUp && ready_launch::g_PickGuide == ready_launch::PickGuide::Detail &&
                                i == count / 2;
        if (!guideMarks)
            DrawTierStars(d.x, d.y + kRarityY * ui, ui, ac::kTierGap, ac::kTierR, rarity[i], tierUps[i],
                          tierPreview[i], d.col, d.textA, d.P.title, now, key);

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
        if (!inExit && focusT[i] > 0.02f) {   // hover only, not the guide's glow
            const float h = focusT[i];
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
        bool glossaryPress = held && !glossaryHeld && !pickGuide;
        if (g_SmokeCapture && inputReady && g_SmokeGlossKeyPresses > 0) { --g_SmokeGlossKeyPresses; glossaryPress = true; }
        glossaryHeld = held;
        DrawGlossaryTips(linkBoxes, cardX, cardY, cardCol, count, inputReady && !inExit && !pickGuide,
                         (float)c.mx, (float)c.my, delta, focus, glossaryPress, pointerMoved && c.mx >= 0.0, sw, sh, ui);
    }

    // Guide line, only once the cards can be picked (the tutorial's guide
    // cards say it while they are up).
    if (inputReady && !pickGuide) {
        // The card and confirm keys follow the bindings (KeyBindings.h); unbound
        // card keys drop out, leaving the click.
        static const GameAction kCardKeys[5] = {
            GameAction::Card1, GameAction::Card2, GameAction::Card3, GameAction::Card4, GameAction::Card5 };
        std::wstring cardKeys;
        for (int i = 0; i < std::min(5, std::max(1, count)); ++i) {
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
        // Augment rework: what this pick is, and the upgrade pick's one reroll.
        if (g_GameManager.ActiveIsUpgrade()) {
            std::wstring head = li == 0 ? L"웨이브 클리어 · 강화할 카드 1장" : li == 2 ? L"ウェーブクリア · 強化するカード1枚" : L"WAVE CLEAR · UPGRADE ONE CARD";
            if (g_GameManager.CanRerollUpgrade() && ActionBound(GameAction::Reroll)) {
                const std::wstring rk = KeyName(ActionKey(GameAction::Reroll));
                head += li == 0 ? L" · " + rk + L" 다시 굴리기 (1회)" : li == 2 ? L" · " + rk + L"で引き直し (1回)" : L" · " + rk + L" TO REROLL (ONCE)";
            }
            guideText = head + L"   " + guideText;
        } else if (g_GameManager.ActiveIsFrame()) {
            const std::wstring head = li == 0 ? L"런 시작 · 이번 런의 무기를 고르세요" : li == 2 ? L"ラン開始 · このランの武器を選んでください" : L"RUN START · CHOOSE THIS RUN'S FRAME";
            guideText = head + L"   " + guideText;
        } else if (g_GameManager.ActiveIsWaveNewCard()) {
            // A wave clear with nothing left to upgrade gives a new augment instead.
            const std::wstring head = li == 0 ? L"웨이브 클리어 · 강화할 카드가 없어 새 증강 1장" : li == 2 ? L"ウェーブクリア · 強化できるカードがないので新しい増強1枚" : L"WAVE CLEAR · NOTHING TO UPGRADE: ONE NEW AUGMENT";
            guideText = head + L"   " + guideText;
        } else if (!tutorialPick) {
            const std::wstring head = li == 0 ? L"레벨 업 · 새 증강 1장" : li == 2 ? L"レベルアップ · 新しい増強1枚" : L"LEVEL UP · ONE NEW AUGMENT";
            guideText = head + L"   " + guideText;
        }
        const wchar_t* guide = guideText.c_str();
        // In the tutorial the line goes under the cards (the field's title sits above them).
        const float gs = UiTextScale(g_TextS, UiTextLevel::Supporting, baseUi);
        const float gw = g_TextS.Width(guide, gs);
        const float gy = tutorialPick ? L.cy + L.cardH * 0.5f + 34.0f * baseUi
                                      : L.cy - L.cardH * 0.5f - 64.0f * ui;
        DrawShadowedText(g_TextS, guide, sw * 0.5f - gw * 0.5f, gy, gs,
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

    // ── tutorial guide cards (the practice field's look: gold corners on the
    //    part in focus, "n / 3", title, text, the key to go on) ──
    if (pickGuideUp && count > 0) {
        using ready_launch::PickGuide;
        const PickGuide g = ready_launch::g_PickGuide;
        const int idx = (int)g;
        float in = ac::Clamp01(ready_launch::g_PickGuideT / 0.30f);
        in = in * in * (3.0f - 2.0f * in);
        const float pulse = 0.5f + 0.5f * std::sin(now * 3.0f);
        const Rgb gold{ 1.00f, 0.86f, 0.40f };
        const float halfW = ac::kCardW * 0.5f * ui, halfH = ac::kCardH * 0.5f * ui;
        auto corners = [&](float x0, float y0, float x1, float y1, float pad = 8.0f) {
            const float grow = (1.0f - in) * 14.0f * ui + pad * ui;
            const float rw = x1 - x0 + 2.0f * grow, rh = y1 - y0 + 2.0f * grow;
            DrawGuideCorners(x0 - grow, y0 - grow, rw, rh, std::min(26.0f * ui, std::min(rw, rh) * 0.3f),
                             1.8f * ui, (0.70f + 0.30f * pulse) * in);
        };
        BindMainShader();
        const CardDraw& first = cd[0];
        const CardDraw& last = cd[count - 1];
        const CardDraw& mid = cd[count / 2];
        if (g == PickGuide::Cards) {
            corners(first.x - halfW, L.cy - halfH, last.x + halfW, L.cy + halfH);
        } else if (g == PickGuide::Detail) {
            const float ry = mid.y + kRarityY * ui;
            const float half = ((ac::kTierSlots - 1) * kGuideMarkGap * 0.5f + kGuideMarkR + 6.0f) * ui;
            corners(mid.x - half, ry - (kGuideMarkR + 4.0f) * ui, mid.x + half, ry + (kGuideMarkR + 4.0f) * ui);
            // Around the words themselves (the meta line and the description
            // lines), well inside the card's own corner brackets and above [n].
            // Across the card's text column (icons and term marks make the
            // measured width unreliable), down to the last description line.
            const CardText& tx = texts[count / 2];
            const float textL = mid.x - (ac::kCardW * 0.5f - 22.0f) * ui;
            const float textR = mid.x + (ac::kCardW * 0.5f - 22.0f) * ui;
            float textB = mid.y + kDescY * ui;
            for (const DescLine& l : tx.desc.lines)
                textB += g_TextS.Height(l.text.c_str(), tx.descScale) + 4.0f * ui;
            textB = std::min(textB, mid.y + 150.0f * ui);
            corners(textL - 4.0f * ui, mid.y + kMetaY * ui - 8.0f * ui, textR + 4.0f * ui, textB + 2.0f * ui, 4.0f);
        } else {
            const float ky = first.y + 182.0f * ui;
            corners(first.x - 30.0f * ui, ky - 14.0f * ui, last.x + 30.0f * ui, ky + 14.0f * ui);
        }
        if (g == PickGuide::Detail) {
            // The tier stars and the glossary words of the middle card
            // pulse in gold: the two things this card talks about.
            // The row grows from its card size to about twice it.
            const int m = count / 2;
            const float ry = mid.y + kRarityY * ui;
            const float gap = ac::kTierGap + (kGuideMarkGap - ac::kTierGap) * in;
            const float r = ac::kTierR + (kGuideMarkR - ac::kTierR + 1.2f * pulse) * in;
            DrawTierStars(mid.x, ry, ui, gap, r, rarity[m], tierUps[m], tierPreview[m], gold,
                          0.65f + 0.35f * pulse, 1.0f, now, key);
            for (const LinkBox& b : linkBoxes) {
                if (b.owner != count / 2) continue;
                SLine(b.x0, b.y1 + 2.0f * ui, b.x1, b.y1 + 2.0f * ui, 2.2f * ui, gold,
                      (0.55f + 0.40f * pulse) * in, key);
            }
        }
        BatchFlush();
        if (g == PickGuide::Keys) {
            // The [n] keys, in gold and larger, breathing.
            const float ks = UiTextScale(g_TextS, UiTextLevel::Description, ui) * (1.25f + 0.08f * pulse);
            for (int i = 0; i < count; ++i) {
                wchar_t keyLabel[8];
                swprintf_s(keyLabel, L"[%d]", i + 1);
                const float kw = g_TextS.Width(keyLabel, ks), kh = g_TextS.Height(keyLabel, ks);
                DrawShadowedText(g_TextS, keyLabel, cd[i].x - kw * 0.5f, cd[i].y + 182.0f * ui - kh * 0.5f, ks,
                                 gold.r, gold.g, gold.b, (0.75f + 0.25f * pulse) * in, 0.70f);
            }
        }

        // Caption above the cards (below them if there is no room).
        const wchar_t* title = ReadyPickGuide(idx, 0);
        // "Rarity & text" talks about underlined words only if the middle card has one.
        bool midTerms = false;
        for (const LinkBox& lb : linkBoxes) midTerms = midTerms || lb.owner == count / 2;
        const wchar_t* body = ReadyPickGuide(idx, 1, midTerms);
        const wchar_t* footer = ReadyPickGuideGo(g == PickGuide::Keys);
        // The tutorial draws this screen shrunk into the practice field
        // (kCardScreenScale); the words keep a readable size on the real screen.
        const float toScreen = ready_launch::g_SelectionUp ? ready_launch::kCardScreenScale : 1.0f;
        auto atLeast = [&](TextRenderer& tr, float scale, float screenPx) {
            return std::max(scale, screenPx / toScreen / std::max(1.0f, tr.LineHeightPixels()));
        };
        // Larger than the screen's own text levels: these are the words to read.
        const float titleS = atLeast(g_TextL, 1.25f * UiTextScale(g_TextL, UiTextLevel::Subtitle, baseUi), 32.0f);
        const float bodyS = atLeast(g_TextS, 1.45f * UiTextScale(g_TextS, UiTextLevel::Description, baseUi), 24.0f);
        const float smallS = atLeast(g_TextS, 1.30f * UiTextScale(g_TextS, UiTextLevel::Supporting, baseUi), 19.0f);
        const float wrapW = std::min(sw * 0.92f, 900.0f * baseUi);
        std::vector<std::wstring> lines;
        {   // Greedy wrap at spaces (between characters when a word runs long).
            std::wstring cur;
            size_t lastSpace = std::wstring::npos;
            for (const wchar_t* p = body; *p && lines.size() < 3; ++p) {
                if (*p == L' ') lastSpace = cur.size();
                cur += *p;
                if (cur.size() > 1 && g_TextS.Width(cur.c_str(), bodyS) > wrapW) {
                    std::wstring rest;
                    if (lastSpace != std::wstring::npos && lastSpace > 0) { rest = cur.substr(lastSpace + 1); cur.resize(lastSpace); }
                    else { rest = cur.substr(cur.size() - 1); cur.pop_back(); }
                    lines.push_back(cur);
                    cur = rest;
                    lastSpace = cur.find_last_of(L' ');
                }
            }
            if (!cur.empty() && lines.size() < 3) lines.push_back(cur);
        }
        const float titleH = g_TextL.Height(title, titleS) * 1.35f;
        const float lineH = g_TextS.Height(L"가A", bodyS) * 1.55f;
        const float footH = g_TextS.Height(footer, smallS) * 1.6f;
        const float blockH = titleH + 8.0f * ui + lines.size() * lineH + 6.0f * ui + footH;
        // Under the cards, where the deck sits (it steps aside while the guide is up).
        float top = L.cy + halfH + 16.0f * ui;
        top = std::min(top, sh - 14.0f * ui - blockH);
        top += (1.0f - in) * 8.0f * ui;
        const float cx = sw * 0.5f;
        // Row 1: "n / 3" and the title, centred together.
        wchar_t stepBuf[16];
        swprintf_s(stepBuf, L"%d / %d", idx + 1, ready_launch::kPickGuideCards);
        const float stepW = g_TextS.Width(stepBuf, smallS), gap = 14.0f * ui;
        const float tW = g_TextL.Width(title, titleS);
        float x = cx - (stepW + gap + tW) * 0.5f;
        const float rowY = top + titleH * 0.5f;
        DrawShadowedText(g_TextS, stepBuf, x, rowY - g_TextS.Height(stepBuf, smallS) * 0.5f, smallS,
                         gold.r, gold.g, gold.b, 0.90f * in, 0.70f);
        x += stepW + gap;
        DrawShadowedText(g_TextL, title, x, rowY - g_TextL.Height(title, titleS) * 0.5f, titleS,
                         gold.r, gold.g, gold.b, in, 0.70f);
        float y = top + titleH + 8.0f * ui;
        for (const std::wstring& l : lines) {
            const float w = g_TextS.Width(l.c_str(), bodyS);
            DrawShadowedText(g_TextS, l.c_str(), cx - w * 0.5f, y, bodyS, 1.0f, 1.0f, 1.0f, in, 0.70f);
            y += lineH;
        }
        y += 6.0f * ui;
        const float fw = g_TextS.Width(footer, smallS);
        DrawShadowedText(g_TextS, footer, cx - fw * 0.5f, y, smallS, gold.r, gold.g, gold.b,
                         (0.72f + 0.25f * pulse) * in, 0.70f);
    }
    BatchFlush();
}

void Scene_AugSelect(const SceneCtx& c) {
    Scene_AugSelectCards(c);
}
