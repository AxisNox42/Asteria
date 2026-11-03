#include "Scenes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "GameContext.h"
#include "SceneUI.h"
#include "SceneInternal.h"
#include "UiLayout.h"
#include "Settings.h"
#include "Augment.h"
#include "../../System/Audio.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "../../Render/HudLines.h"
#include "../../Render/AugCardModel.h"
#include "../../Render/Juice.h"
#include "../../Render/StatIcons.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cwchar>

// One description segment wrapped to maxWidth, appended to lines.
static void WrapSegment(std::wstring segment, float scale, float maxWidth,
                        std::vector<std::wstring>& lines) {
    while (!segment.empty() && segment.front() == L' ') segment.erase(segment.begin());
    std::wstring line;
    size_t start = 0;
    while (start < segment.size()) {
        const size_t end = segment.find(L' ', start);
        const std::wstring word = segment.substr(start,
            end == std::wstring::npos ? std::wstring::npos : end - start);
        if (word.empty()) {
            if (end == std::wstring::npos) break;
            start = end + 1;
            continue;
        }
        const std::wstring candidate = line.empty() ? word : line + L" " + word;
        if (g_TextS.Width(word.c_str(), scale) > maxWidth) {
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            std::wstring part;
            for (wchar_t ch : word) {
                const std::wstring next = part + ch;
                if (!part.empty() &&
                    g_TextS.Width(next.c_str(), scale) > maxWidth) {
                    lines.push_back(part);
                    part.clear();
                }
                part += ch;
            }
            line = std::move(part);
        } else if (!line.empty() &&
                   g_TextS.Width(candidate.c_str(), scale) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    if (!line.empty()) lines.push_back(line);
}

// Description segments: split at '·' and at a spaced " / ".
static std::vector<std::wstring> DescSegments(const wchar_t* src) {
    std::vector<std::wstring> segments;
    const wchar_t* text = src ? src : L"";
    std::wstring segment;
    for (const wchar_t* p = text; *p; ++p) {
        const bool spacedSlash = *p == L'/' && p > text && p[1] != L'\0'
                              && p[-1] == L' ' && p[1] == L' ';
        if (*p == L'\xB7' || spacedSlash) {
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

IconDesc WrapIconDesc(const wchar_t* desc, float scale, float maxWidth, float ui) {
    IconDesc out;
    out.iconSize = g_TextS.Height(L"가Ag", scale) * 1.15f;
    out.iconIndent = out.iconSize + 8.0f * ui;
    std::vector<std::wstring> wrapped;
    for (std::wstring& segment : DescSegments(desc)) {
        stat_icon::Stat stat;
        const bool icon = stat_icon::FromText(segment, stat);
        wrapped.clear();
        WrapSegment(std::move(segment), scale, icon ? maxWidth - out.iconIndent : maxWidth, wrapped);
        for (size_t j = 0; j < wrapped.size(); ++j)
            out.lines.push_back({ std::move(wrapped[j]), icon && j == 0 ? (int)stat : -1, icon });
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

struct Rect { float x0, y0, x1, y1; };
bool InRects(const std::vector<Rect>& rects, float x, float y) {
    for (const Rect& r : rects)
        if (x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1) return true;
    return false;
}

// Node k of a family constellation, card-local.
void ConNode(const ac::Shape& sh, int k, float& lx, float& ly) {
    lx = ac::kConX + sh.x[k] * ac::kConW;
    ly = ac::kConY + sh.y[k] * ac::kConH;
}

// Semi-transparent constellation: lines that cross a text line drop to ×0.45
// instead of putting a plate behind the text. Nodes in `hidden` have already
// lifted off as dissolve seeds.
void DrawConstellation(const Xf& X, const ac::Shape& sh, Rgb col, float alpha, float trace,
                       const std::vector<Rect>& textRects, float now, float phase,
                       float ui, float key, const bool* hidden) {
    if (alpha <= 0.002f || trace <= 0.0f) return;
    const float prog = trace * sh.ne;
    for (int e = 0; e < sh.ne; ++e) {
        const float f = ac::Clamp01(prog - e);
        if (f <= 0.0f) break;
        float ax, ay, bx, by;
        ConNode(sh, sh.ea[e], ax, ay);
        ConNode(sh, sh.eb[e], bx, by);
        float x0, y0, x1, y1;
        X.P(ax, ay, x0, y0);
        X.P(Lerp(ax, bx, f), Lerp(ay, by, f), x1, y1);
        constexpr int kPieces = 10;
        for (int p = 0; p < kPieces; ++p) {
            const float t0 = (float)p / kPieces, t1 = (float)(p + 1) / kPieces;
            const float px0 = Lerp(x0, x1, t0), py0 = Lerp(y0, y1, t0);
            const float px1 = Lerp(x0, x1, t1), py1 = Lerp(y0, y1, t1);
            const float a = alpha * (InRects(textRects, (px0 + px1) * 0.5f, (py0 + py1) * 0.5f) ? 0.45f : 1.0f);
            SLine(px0, py0, px1, py1, 1.25f * ui, col, a, key);
        }
    }
    for (int k = 0; k < sh.nv; ++k) {
        if (hidden && hidden[k]) continue;
        const int first = sh.FirstEdge(k);
        const float lit = ac::Clamp01(prog - first + (first >= sh.ne ? 0.0f : 0.2f));
        if (lit <= 0.0f) continue;
        float lx, ly, x, y;
        ConNode(sh, k, lx, ly);
        X.P(lx, ly, x, y);
        const float tw = 0.72f + 0.28f * std::sin(now * 6.2832f / (2.4f + (k % 4) * 0.4f) + phase + k * 1.7f);
        const float a = std::min(1.0f, alpha * 1.5f) * lit * tw * (InRects(textRects, x, y) ? 0.45f : 1.0f);
        hud_lines::Ring(x, y, 2.6f * ui, 1.0f * ui, col, a, key);
        drawCircle(x, y, 1.0f * ui, col.r, col.g, col.b, a);
    }
}

// Back: gapped ring + eight spokes (ASTERIA sigil); legendary+ fills the
// ring from 12 o'clock while charging.
void DrawBack(const Xf& X, Rgb rarity, float a, float charge, float ui, float key) {
    const Rgb ring = Mix(kUiBlue, rarity, 0.28f);
    LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.4f * ui, kUiBlue, a * 0.62f, key);
    LOutline(X, ac::kCardW - 14.0f, ac::kCardH - 14.0f, ac::kCut - 5.0f, 1.0f * ui, kUiBlue, a * 0.14f, 0.0f);
    LCornerMarks(X, kUiBlue, a * 0.60f, 1.2f * ui, key);
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
    wchar_t meta[96];
    if (AugIsActive(def)) swprintf_s(meta, kActive[CurLangIdx()], def.cooldown);
    else swprintf_s(meta, L"%ls", kPassive[CurLangIdx()]);
    t.meta = meta;
    t.metaScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    const float room = (kDescBottom - kDescY) * ui;
    const float floorScale = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    float d = ui;
    for (;;) {
        t.descScale = UiTextScale(g_TextS, UiTextLevel::Description, d);
        t.desc = WrapIconDesc(AugDesc(def), t.descScale, inner, ui);
        float h = 0.0f;
        for (const auto& l : t.desc.lines) h += g_TextS.Height(l.text.c_str(), t.descScale) + 4.0f * ui;
        if (h <= room || t.descScale <= floorScale) break;
        d -= 0.04f * ui;
    }
    return t;
}

struct Shard { float x, y, vx, vy, life; Rgb c; };

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
    const AugDef* defs[ac::kMaxCards] = {};
    for (int i = 0; i < count; ++i) {
        const int idx = g_GameManager.augChoices[i];
        defs[i] = (idx >= 0 && idx < AUG_TOTAL) ? &ALL_AUGS[idx] : nullptr;
        rarity[i] = defs[i] ? (int)defs[i]->rarity : 0;
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
    static bool flashed = false;
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
        flashed = false;
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
            Audio::Play(Audio::Cue::CardFlip);
        }
    }
    if (inExit && !wasExit) Audio::Play(Audio::Cue::CardPick);
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
    if (!inExit) {
        for (int k = 3; k >= 0; --k) {
            const Xf X = MakeXf(L.deckX + k * 2.5f * ui, L.deckY - k * 2.5f * ui, L.deckS * ui, L.deckS * ui, 0.0f);
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, 1.3f * ui, kUiBlue,
                     frameA * (k == 0 ? 0.60f : 0.22f), k == 0 ? key : 0.0f);
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
    std::vector<Rect> rects[ac::kMaxCards];
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
            LOutline(X, ac::kCardW, ac::kCardH, ac::kCut, (1.4f + 0.8f * h) * ui, d.col,
                     d.alpha * (0.62f + 0.32f * h), key);
            LOutline(X, ac::kCardW - 14.0f, ac::kCardH - 14.0f, ac::kCut - 5.0f, 1.0f * ui, d.col, d.alpha * 0.14f, 0.0f);
            LCornerMarks(X, d.col, d.alpha * (0.62f + 0.32f * h), 1.2f * ui, key);
        }
        if (d.P.flip < 1.0f) continue;

        // Text line boxes (screen) the constellation steps around.
        const CardText& tx = texts[i];
        const float left = d.x - (ac::kCardW * 0.5f - 22.0f) * ui;
        const float right = d.x + (ac::kCardW * 0.5f - 22.0f) * ui;
        rects[i].push_back({ d.x - (kIconSize * 0.5f + 8.0f) * ui, d.y + (kIconY - kIconSize * 0.5f - 6.0f) * ui,
                             d.x + (kIconSize * 0.5f + 8.0f) * ui, d.y + (kIconY + kIconSize * 0.5f + 6.0f) * ui });
        rects[i].push_back({ left, d.y + (kTitleY - 26.0f) * ui, right, d.y + (kTitleY + 26.0f) * ui });
        float y = d.y + kMetaY * ui;
        const float metaH = g_TextS.Height(tx.meta.c_str(), tx.metaScale);
        rects[i].push_back({ left, y - 2.0f * ui, left + g_TextS.Width(tx.meta.c_str(), tx.metaScale) + 6.0f * ui, y + metaH + 2.0f * ui });
        y = d.y + kDescY * ui;
        for (const DescLine& l : tx.desc.lines) {
            const float lh = g_TextS.Height(l.text.c_str(), tx.descScale);
            rects[i].push_back({ left, y - 2.0f * ui, tx.desc.TextX(l, left) + g_TextS.Width(l.text.c_str(), tx.descScale) + 6.0f * ui, y + lh + 2.0f * ui });
            y += lh + 4.0f * ui;
        }

        // Nodes that already lifted off as dissolve seeds.
        const ac::Shape& shp = ac::ShapeOf(ac::FamilyFromCode(defs[i]->name));
        bool hidden[ac::Shape::kMaxV] = {};
        if (inExit && i != pick)
            for (int k = 0; k < shp.nv; ++k) hidden[k] = exitT >= ac::SeedStart(std::abs(i - pick), k);
        const float trace = (inExit && i == pick) ? d.alpha / std::max(0.001f, sceneA) : d.P.trace;
        DrawConstellation(X, shp, d.col, d.conA * (0.30f + 0.16f * h), trace, rects[i],
                          now, phase[i], ui, key, hidden);

        // Rarity marks (1~5 diamonds) and the divider under the title.
        const int n = rarity[i] + 1;
        for (int k = 0; k < n; ++k)
            SDiamond(d.x + (k - (n - 1) * 0.5f) * 14.0f * ui, d.y + kRarityY * ui, 4.0f * ui, d.col,
                     d.textA * 0.9f * d.P.title, 1.2f * ui, key, true);
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
            DrawShadowedText(g_TextS, l.text.c_str(), tx.desc.TextX(l, left), y,
                             tx.descScale, kTextSub.r, kTextSub.g, kTextSub.b, lineA, 0.70f);
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

    // Guide line, only once the cards can be picked.
    if (inputReady) {
        // The key range follows the card count (a stat pick has three).
        static const wchar_t* kGuide[3] = {
            L"카드를 클릭하거나 1~%d 키로 고른 뒤 Space로 확정",
            L"CLICK A CARD, OR PRESS 1-%d AND CONFIRM WITH SPACE",
            L"カードをクリック、または1〜%dキーで選んでSpaceで確定" };
        wchar_t guide[96];
        std::swprintf(guide, 96, kGuide[CurLangIdx()], std::max(1, count));
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
                if (!flashed) {
                    flashed = true;
                    TriggerFlash(d.col.r, d.col.g, d.col.b, 0.30f);
                }
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
                        SLine(cx + x0 * ui, cy + y0 * ui, cx + x1 * ui, cy + y1 * ui, 1.4f * ui, col, 0.62f * sceneA, key);
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
