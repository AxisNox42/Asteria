#pragma once
// F1 debug console widgets (immediate mode).
// The same tab function runs twice a frame: once in the input pass (answers
// clicks, no drawing) and once in the draw pass (draws, never acts). Each widget
// computes its own rectangle from the layout cursor, so what is drawn and what is
// clickable can never drift apart. Debug-only: the console keeps its dark plate
// (an agreed exception to the no-plate UI rule). Style: plain and practical —
// flat grey controls, 1px solid lines, one accent, no corner brackets.
#include <string>

struct DebugRect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

struct DebugColor {
    float r = 0.45f, g = 0.85f, b = 1.0f;
};

// The console's palette (DebugUI.cpp): neutral greys and one accent.
namespace debugui_style {
extern const DebugColor kText, kTextDim, kTextOff;
extern const DebugColor kFill, kFillHover, kFillOff, kSelected, kLine;
}

class DebugUI {
public:
    enum class Pass { Input, Draw };

    Pass pass = Pass::Draw;
    float mx = -10000.0f, my = -10000.0f;
    bool click = false;    // left press edge (input pass only)
    bool rclick = false;   // right press edge (input pass only)
    float scroll = 0.0f;   // wheel this frame; a scroll area that uses it zeroes it

    // Layout cursor: rows go down from y inside [left, left + width].
    float left = 0.0f, width = 0.0f, y = 0.0f;
    // Rows outside [clipTop, clipBottom] are neither drawn nor clickable.
    float clipTop = -1e9f, clipBottom = 1e9f;

    bool Drawing() const { return pass == Pass::Draw; }
    bool Hover(const DebugRect& r) const {
        return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
    }
    bool Visible(const DebugRect& r) const {
        return r.y >= clipTop - 0.5f && r.y + r.h <= clipBottom + 0.5f;
    }
    bool Clicked(const DebugRect& r) const {
        return pass == Pass::Input && click && Visible(r) && Hover(r);
    }
    bool RClicked(const DebugRect& r) const {
        return pass == Pass::Input && rclick && Visible(r) && Hover(r);
    }

    // Layout.
    DebugRect Row(float h, float gapAfter = 6.0f);
    void Space(float h) { y += h; }
    static DebugRect Cell(const DebugRect& row, int count, int index, float gap = 6.0f);
    static DebugRect Span(const DebugRect& row, int count, int index, int span, float gap = 6.0f);

    // Text and decoration.
    void Section(const wchar_t* title, const wchar_t* hint = nullptr);
    void Text(float x, float y, const wchar_t* text, float scale,
              float r, float g, float b, float a) const;
    void TextRow(const wchar_t* text, float scale = 0.46f,
                 float r = 0.60f, float g = 0.63f, float b = 0.67f, float a = 1.0f);
    void TextCentered(const DebugRect& rc, const wchar_t* text, float scale,
                      float r, float g, float b, float a) const;
    void Fill(const DebugRect& rc, float r, float g, float b, float a) const;
    void Frame(const DebugRect& rc, float r, float g, float b, float a,
               float corner = 9.0f, float dot = 3.0f) const;

    // Widgets. Return true on a left click (input pass).
    bool Button(const DebugRect& rc, const wchar_t* label,
                bool selected = false, bool enabled = true,
                const DebugColor* accent = nullptr, float textScale = 0.52f);
    // 0 = nothing, 1 = left click, 2 = right click.
    int ButtonLR(const DebugRect& rc, const wchar_t* label,
                 bool selected = false, bool enabled = true,
                 const DebugColor* accent = nullptr, float textScale = 0.52f);
    bool Toggle(const DebugRect& rc, const wchar_t* label, bool on, bool enabled = true);
    // Chips that wrap onto new rows. Returns the clicked index or -1.
    int Chips(const wchar_t* const* labels, int count, int selected,
              float h = 28.0f, float textScale = 0.44f,
              const DebugColor* colors = nullptr);

    // Vertical list of `count` rows of itemH in `area`. Uses the wheel when the
    // pointer is over the area; returns how many rows fit.
    int ScrollRows(const DebugRect& area, float itemH, int count, int& offset);
};

// Shortens text with "..." to fit maxWidth at the small font's scale.
std::wstring DebugFitText(const wchar_t* text, float scale, float maxWidth);
std::wstring DebugFormatFloat(float value);
