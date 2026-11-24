#include "DebugUI.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

#include "DrawPrim.h"
#include "TextRenderer.h"

extern TextRenderer g_TextS;

// Plain working style: neutral greys, flat controls, 1px solid lines, one
// accent (calm blue) for "selected". Category colours only appear as a thin
// bar on the left of a control. No corner brackets or other decoration.
namespace debugui_style {
const DebugColor kText     { 0.90f, 0.91f, 0.92f };
const DebugColor kTextDim  { 0.60f, 0.63f, 0.67f };
const DebugColor kTextOff  { 0.40f, 0.42f, 0.45f };
const DebugColor kFill     { 0.150f, 0.160f, 0.180f };
const DebugColor kFillHover{ 0.205f, 0.218f, 0.242f };
const DebugColor kFillOff  { 0.110f, 0.115f, 0.128f };
const DebugColor kSelected { 0.200f, 0.400f, 0.640f };
const DebugColor kLine     { 0.240f, 0.255f, 0.280f };
}
using namespace debugui_style;

DebugRect DebugUI::Row(float h, float gapAfter) {
    DebugRect r{ left, y, width, h };
    y += h + gapAfter;
    return r;
}

DebugRect DebugUI::Cell(const DebugRect& row, int count, int index, float gap) {
    return Span(row, count, index, 1, gap);
}

DebugRect DebugUI::Span(const DebugRect& row, int count, int index, int span, float gap) {
    count = std::max(1, count);
    const float cw = (row.w - gap * (float)(count - 1)) / (float)count;
    DebugRect r = row;
    r.x = row.x + (cw + gap) * (float)index;
    r.w = cw * (float)span + gap * (float)(span - 1);
    return r;
}

void DebugUI::Text(float x, float ty, const wchar_t* text, float scale,
                   float r, float g, float b, float a) const {
    if (!Drawing() || !text || !*text) return;
    if (ty < clipTop - 2.0f || ty + 14.0f > clipBottom + 2.0f) return;
    g_TextS.Draw(text, x, ty, scale, r, g, b, a);
}

void DebugUI::TextRow(const wchar_t* text, float scale,
                      float r, float g, float b, float a) {
    const float h = std::max(18.0f, g_TextS.Height(L"Ag", scale) + 6.0f);
    const DebugRect rc = Row(h, 4.0f);
    if (!Visible(rc)) return;
    Text(rc.x + 2.0f, rc.y + 2.0f,
         DebugFitText(text, scale, rc.w - 4.0f).c_str(), scale, r, g, b, a);
}

void DebugUI::TextCentered(const DebugRect& rc, const wchar_t* text, float scale,
                           float r, float g, float b, float a) const {
    if (!Drawing() || !text || !Visible(rc)) return;
    float s = scale;
    float tw = g_TextS.Width(text, s);
    const float maxW = std::max(10.0f, rc.w - 12.0f);
    if (tw > maxW) { s *= maxW / tw; tw = g_TextS.Width(text, s); }
    const float th = g_TextS.Height(text, s);
    g_TextS.Draw(text, rc.x + (rc.w - tw) * 0.5f,
                 rc.y + std::max(2.0f, (rc.h - th) * 0.5f), s, r, g, b, a);
}

void DebugUI::Fill(const DebugRect& rc, float r, float g, float b, float a) const {
    if (!Drawing() || !Visible(rc)) return;
    BindMainShader();
    drawRect(rc.x, rc.y, rc.w, rc.h, r, g, b, a);
}

// 1px solid outline (the corner / dot arguments are kept for old call sites).
void DebugUI::Frame(const DebugRect& rc, float r, float g, float b, float a,
                    float /*corner*/, float /*dot*/) const {
    if (!Drawing() || !Visible(rc)) return;
    BindMainShader();
    drawRect(rc.x, rc.y, rc.w, 1.0f, r, g, b, a);
    drawRect(rc.x, rc.y + rc.h - 1.0f, rc.w, 1.0f, r, g, b, a);
    drawRect(rc.x, rc.y + 1.0f, 1.0f, rc.h - 2.0f, r, g, b, a);
    drawRect(rc.x + rc.w - 1.0f, rc.y + 1.0f, 1.0f, rc.h - 2.0f, r, g, b, a);
}

void DebugUI::Section(const wchar_t* title, const wchar_t* hint) {
    y += 10.0f;
    const DebugRect rc = Row(22.0f, 6.0f);
    if (!Drawing() || !Visible(rc)) return;
    g_TextS.Draw(title, rc.x, rc.y + 1.0f, 0.50f, kText.r, kText.g, kText.b, 0.96f);
    const float tw = g_TextS.Width(title, 0.50f);
    if (hint && *hint) {
        const std::wstring h = DebugFitText(hint, 0.42f, rc.w - tw - 14.0f);
        g_TextS.Draw(h.c_str(), rc.x + tw + 12.0f, rc.y + 3.0f, 0.42f,
                     kTextDim.r, kTextDim.g, kTextDim.b, 0.90f);
    }
    BindMainShader();
    drawRect(rc.x, rc.y + rc.h + 1.0f, rc.w, 1.0f, kLine.r, kLine.g, kLine.b, 1.0f);
}

int DebugUI::ButtonLR(const DebugRect& rc, const wchar_t* label, bool selected,
                      bool enabled, const DebugColor* accent, float textScale) {
    if (!Visible(rc)) return 0;
    if (pass == Pass::Input) {
        // A handled click is used up, so a widget that changes the layout
        // cannot hand the same click to whatever moves under the pointer.
        if (!enabled) return 0;
        if (Clicked(rc)) { click = false; return 1; }
        if (RClicked(rc)) { rclick = false; return 2; }
        return 0;
    }
    const bool hover = enabled && Hover(rc);
    const DebugColor& f = !enabled ? kFillOff : selected ? kSelected : hover ? kFillHover : kFill;
    const float lift = selected && hover ? 0.04f : 0.0f;
    BindMainShader();
    drawRect(rc.x, rc.y, rc.w, rc.h, f.r + lift, f.g + lift, f.b + lift, 1.0f);
    if (accent && enabled)   // category colour: a thin bar on the left only
        drawRect(rc.x, rc.y, 3.0f, rc.h, accent->r, accent->g, accent->b, 0.95f);
    if (label) {
        const DebugColor& t = !enabled ? kTextOff : selected ? DebugColor{ 1.0f, 1.0f, 1.0f } : kText;
        TextCentered(rc, label, textScale, t.r, t.g, t.b, 1.0f);
    }
    return 0;
}

bool DebugUI::Button(const DebugRect& rc, const wchar_t* label, bool selected,
                     bool enabled, const DebugColor* accent, float textScale) {
    return ButtonLR(rc, label, selected, enabled, accent, textScale) == 1;
}

bool DebugUI::Toggle(const DebugRect& rc, const wchar_t* label, bool on, bool enabled) {
    if (!Visible(rc)) return false;
    if (pass == Pass::Input) return ButtonLR(rc, nullptr, false, enabled) == 1;
    ButtonLR(rc, nullptr, false, enabled);
    // Check box on the left, label after it.
    const float box = 14.0f;
    const float bx = rc.x + 10.0f, by = rc.y + (rc.h - box) * 0.5f;
    BindMainShader();
    if (on && enabled) {
        drawRect(bx, by, box, box, kSelected.r + 0.1f, kSelected.g + 0.12f, kSelected.b + 0.15f, 1.0f);
        drawRect(bx + 4.0f, by + 4.0f, box - 8.0f, box - 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        Frame({ bx, by, box, box }, kTextDim.r, kTextDim.g, kTextDim.b, enabled ? 0.9f : 0.4f);
    }
    if (label) {
        const float s = 0.48f;
        const float th = g_TextS.Height(label, s);
        const std::wstring t = DebugFitText(label, s, rc.w - box - 28.0f);
        const DebugColor& c = !enabled ? kTextOff : kText;
        g_TextS.Draw(t.c_str(), bx + box + 10.0f, rc.y + std::max(2.0f, (rc.h - th) * 0.5f), s,
                     c.r, c.g, c.b, 1.0f);
    }
    return false;
}

int DebugUI::Chips(const wchar_t* const* labels, int count, int selected,
                   float h, float textScale, const DebugColor* colors) {
    int clicked = -1;
    float x = left;
    DebugRect row{ left, y, width, h };
    for (int i = 0; i < count; ++i) {
        const float w = std::min(width, g_TextS.Width(labels[i], textScale) + 24.0f);
        if (x > left && x + w > left + width) {
            x = left;
            y += h + 5.0f;
            row.y = y;
        }
        const DebugRect rc{ x, row.y, w, h };
        if (Button(rc, labels[i], i == selected, true, colors ? &colors[i] : nullptr, textScale))
            clicked = i;
        x += w + 5.0f;
    }
    y += h + 6.0f;
    return clicked;
}

int DebugUI::ScrollRows(const DebugRect& area, float itemH, int count, int& offset) {
    const int rows = std::max(1, (int)(area.h / itemH));
    if (pass == Pass::Input && scroll != 0.0f && Hover(area)) {
        offset -= (int)std::round(scroll * 2.0f);
        scroll = 0.0f;
    }
    offset = std::max(0, std::min(std::max(0, count - rows), offset));
    if (Drawing() && count > rows) {
        const float trackH = area.h;
        const float thumbH = std::max(18.0f, trackH * (float)rows / (float)count);
        const float t = (float)offset / (float)std::max(1, count - rows);
        BindMainShader();
        drawRect(area.x + area.w + 3.0f, area.y, 3.0f, trackH, kFill.r, kFill.g, kFill.b, 1.0f);
        drawRect(area.x + area.w + 3.0f, area.y + (trackH - thumbH) * t, 3.0f, thumbH,
                 kTextDim.r, kTextDim.g, kTextDim.b, 1.0f);
    }
    return rows;
}

std::wstring DebugFitText(const wchar_t* text, float scale, float maxWidth) {
    std::wstring s = text ? text : L"";
    for (auto& ch : s) if (ch == L'\n') ch = L' ';
    if (g_TextS.Width(s.c_str(), scale) <= maxWidth) return s;
    while (!s.empty()) {
        s.pop_back();
        const std::wstring t = s + L"...";
        if (g_TextS.Width(t.c_str(), scale) <= maxWidth) return t;
    }
    return s;
}

std::wstring DebugFormatFloat(float value) {
    wchar_t buf[64] = {};
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%.5g", value);
    return buf;
}
