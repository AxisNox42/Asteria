#pragma once
// What a boss pane's client shows: a random memo, a random code file or a
// picture from the player's PC (PaneImages.h), painted with GDI (OsPane.cpp's
// WM_PAINT). Header-only and free of game state so
// tests/pane_content_preview.cpp can paint it to files.
// A worn pane (unstable > 0) breaks up as one picture (Glitch): red and cyan
// pull apart, slabs slide sideways and thin streaks run along the rows.
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "PaneImages.h"

namespace pane_content {

enum class Kind { None, Memo, Code, Image };

struct Content {
    Kind kind = Kind::None;
    unsigned seed = 0;
    bool dark = false;
    std::wstring title;
    float unstable = 0.0f;
    std::shared_ptr<const pane_images::Picture> picture;   // image kind; null = line-art chart
};

// Client colours per theme: paper and ink, like a plain text editor.
constexpr COLORREF kPaperLight = RGB(255, 255, 255), kInkLight = RGB(60, 64, 80);
constexpr COLORREF kPaperDark = RGB(30, 31, 36), kInkDark = RGB(190, 196, 214);

inline unsigned NextRandom(unsigned& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

inline COLORREF Mix(COLORREF a, COLORREF b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto ch = [&](int x, int y) { return (int)(x + (y - x) * t + 0.5f); };
    return RGB(ch(GetRValue(a), GetRValue(b)), ch(GetGValue(a), GetGValue(b)),
               ch(GetBValue(a), GetBValue(b)));
}

inline COLORREF Paper(const Content& c) { return c.dark ? kPaperDark : kPaperLight; }
inline COLORREF Ink(const Content& c) { return c.dark ? kInkDark : kInkLight; }

// ---------------------------------------------------------------- sources
// A memo: someone living inside the programmed sky, writing things down.
inline const wchar_t* const kMemoLines[] = {
    L"할 일", L"- 별 12개 다시 세기", L"- 창 닫지 말 것 (절대)", L"- 좌표 백업",
    L"[x] 북쪽 경계 확인", L"[ ] 03:14 로그 다시 읽기", L"[ ] 이 창을 누가 열었는지 찾기",
    L"오늘도 하늘이 한 번 재시작됐다.", L"별들이 선으로만 이어진다. 원래 그랬나?",
    L"창 안으로는 탄이 못 들어온다고 들었다.", L"누가 내 창을 또 옮겼다...",
    L"OCTANS 근처에는 가지 말 것.", L"여덟 번째 별이 꺼지면 끝이라고 했다.",
    L"memo: the sky compiles at night", L"remember: windows are walls",
    L"grocery: stardust x3, ink, more ink", L"p.s. 마우스 커서가 또 하늘을 지나갔다",
    L"8개의 별, 4개의 창, 1개의 심장", L"내일 할 일: 아무 창도 열지 않기",
    L"Q. 이 우주는 누가 컴파일했을까?", L"A. ...", L"(여기서부터 글씨가 번져 있다)",
};
// A code file: the sky's own source. C-like and Python-like pools.
inline const wchar_t* const kCodeC[] = {
    L"#include \"sky.h\"", L"", L"// constellation.cfg - do not edit",
    L"static const int kStars = 8;", L"void Octans::Seal(Pane& pane) {",
    L"    pane.boundary = WINDOW;", L"    for (int i = 0; i < kStars; ++i)",
    L"        stars[i].Link(Nearest(stars[i]));", L"    if (player.space != pane.space)",
    L"        return;   // attacks do not cross", L"    CountDown(3);", L"}", L"",
    L"bool Pane::Contains(const Core& core) const {",
    L"    return core.x >= x && core.x <= x + w;", L"}", L"",
    L"while (universe.running) { Tick(dt); }", L"float durability = 250.0f;",
    L"auto sky = Render(stars, /*linesOnly=*/true);", L"assert(window.Contains(core));",
    L"return sky;",
};
inline const wchar_t* const kCodePy[] = {
    L"import sky", L"from cosmos import Star, Pane", L"", L"STARS = 8",
    L"# the sky's own source - do not edit", L"def seal(pane):",
    L"    pane.boundary = \"WINDOW\"", L"    for s in pane.stars:",
    L"        s.link(nearest(s))", L"    return pane", L"",
    L"class Octans(Star):", L"    def __init__(self):", L"        self.hearts = 1",
    L"        self.windows = []", L"", L"    def judge(self, player):",
    L"        if player.space != self.space:", L"            return None  # walls hold",
    L"        return countdown(3)", L"", L"while universe.running:", L"    tick()",
};
inline const wchar_t* const kCodeFilesC[] = { L"sky_render.cpp", L"octans.cpp", L"pane.c", L"core.h", L"stars.js" };
inline const wchar_t* const kCodeFilesPy[] = { L"constellation.py", L"seal.py", L"universe.py" };
inline const wchar_t* const kMemoFiles[] = { L"todo.txt", L"memo_%02u.txt", L"observations.txt",
                                             L"readme.txt", L"diary_%02u.txt" };
// Names for the line-art chart shown when no picture has loaded.
inline const wchar_t* const kImageFiles[] = { L"star_chart_%02u.png", L"sky_%02u.png", L"capture_%04u.png" };

template <class T, size_t N> constexpr int Count(const T (&)[N]) { return (int)N; }

// Kind and file title from the seed (4 memo : 4 code : 12 image). An image
// pane takes one of the pictures loaded from this PC and its real file name.
inline void Roll(Content& c, const wchar_t* base) {
    unsigned s = c.seed;
    const unsigned pick = NextRandom(s) % 20;
    c.kind = pick < 4 ? Kind::Memo : pick < 8 ? Kind::Code : Kind::Image;
    const unsigned num = NextRandom(s);
    wchar_t file[64] = {};
    if (c.kind == Kind::Image) c.picture = pane_images::Pick(NextRandom(s));
    if (c.kind == Kind::Memo) {
        swprintf_s(file, kMemoFiles[num % Count(kMemoFiles)], (num >> 4) % 100);
    } else if (c.kind == Kind::Code) {
        const bool py = (c.seed >> 3) % 3 == 0;
        if (py) swprintf_s(file, L"%ls", kCodeFilesPy[num % Count(kCodeFilesPy)]);
        else swprintf_s(file, L"%ls", kCodeFilesC[num % Count(kCodeFilesC)]);
    } else if (!c.picture) {
        const wchar_t* fmt = kImageFiles[num % Count(kImageFiles)];
        const unsigned n = (num >> 4) % 10000;
        swprintf_s(file, fmt, wcsstr(fmt, L"%04u") ? n : n % 100);
    }
    c.title = std::wstring(base) + L" - " + (c.picture ? c.picture->name : std::wstring(file));
}

// ---------------------------------------------------------------- helpers
inline HFONT MakeFont(int height, int weight, const wchar_t* face, bool fixed) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       (fixed ? FIXED_PITCH | FF_MODERN : VARIABLE_PITCH | FF_SWISS), face);
}

inline void Fill(HDC dc, int l, int t, int r, int b, COLORREF col) {
    RECT rc{ l, t, r, b };
    HBRUSH br = CreateSolidBrush(col);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

inline void Line(HDC dc, int x0, int y0, int x1, int y1, COLORREF col, int w = 1) {
    HPEN pen = CreatePen(PS_SOLID, w, col);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x0, y0, nullptr);
    LineTo(dc, x1, y1);
    SelectObject(dc, old);
    DeleteObject(pen);
}

// ---------------------------------------------------------------- memo
inline void PaintMemo(HDC dc, const RECT& r, const Content& c) {
    HFONT font = MakeFont(16, FW_NORMAL, L"Malgun Gothic", false);
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Ink(c));
    unsigned state = c.seed;
    const int lineHeight = 22;
    constexpr int n = Count(kMemoLines);
    bool used[n] = {};
    int at = (int)(NextRandom(state) % n), left = n;
    for (int y = r.top + 10; y + lineHeight <= r.bottom - 4 && left > 0; y += lineHeight) {
        if (NextRandom(state) % 7 == 0) continue;   // a blank line now and then
        // Mostly in order (it reads like a memo), now and then a jump; never
        // the same line twice.
        if (NextRandom(state) % 4 == 0) at = (int)(NextRandom(state) % n);
        while (used[at]) at = (at + 1) % n;
        used[at] = true;
        --left;
        const wchar_t* text = kMemoLines[at];
        TextOutW(dc, r.left + 12, y, text, lstrlenW(text));
    }
    SelectObject(dc, old);
    DeleteObject(font);
}

// ---------------------------------------------------------------- code
inline bool IsKeyword(const std::wstring& w) {
    static const wchar_t* const kWords[] = {
        L"if", L"for", L"while", L"return", L"void", L"int", L"float", L"bool", L"static",
        L"const", L"auto", L"class", L"def", L"import", L"from", L"in", L"true", L"false",
        L"None", L"self", L"struct", L"include", L"assert" };
    for (const wchar_t* k : kWords) if (w == k) return true;
    return false;
}

// One source line in editor colours: keywords, strings, numbers, comments.
inline void PaintCodeLine(HDC dc, int x, int y, const std::wstring& line, const Content& c,
                          bool py) {
    const COLORREF ink = Ink(c);
    const COLORREF kw = c.dark ? RGB(110, 160, 235) : RGB(30, 80, 190);
    const COLORREF str = c.dark ? RGB(214, 157, 120) : RGB(170, 70, 30);
    const COLORREF num = c.dark ? RGB(170, 205, 150) : RGB(40, 130, 80);
    const COLORREF com = c.dark ? RGB(106, 150, 100) : RGB(80, 140, 80);
    const auto out = [&](const std::wstring& s, COLORREF col) {
        SetTextColor(dc, col);
        TextOutW(dc, x, y, s.c_str(), (int)s.size());
        SIZE sz{};
        GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
        x += sz.cx;
    };
    size_t i = 0;
    while (i < line.size()) {
        const wchar_t ch = line[i];
        if ((ch == L'/' && i + 1 < line.size() && line[i + 1] == L'/') || (py && ch == L'#') ||
            (ch == L'/' && i + 1 < line.size() && line[i + 1] == L'*')) {
            size_t end = line.find(L"*/", i + 2);
            end = (ch == L'/' && line[i + 1] == L'*' && end != std::wstring::npos) ? end + 2 : line.size();
            out(line.substr(i, end - i), com);
            i = end;
        } else if (ch == L'"') {
            size_t end = line.find(L'"', i + 1);
            end = end == std::wstring::npos ? line.size() : end + 1;
            out(line.substr(i, end - i), str);
            i = end;
        } else if (iswalpha(ch) || ch == L'_') {
            size_t end = i;
            while (end < line.size() && (iswalnum(line[end]) || line[end] == L'_')) ++end;
            const std::wstring w = line.substr(i, end - i);
            out(w, IsKeyword(w) ? kw : ink);
            i = end;
        } else if (iswdigit(ch)) {
            size_t end = i;
            while (end < line.size() && (iswalnum(line[end]) || line[end] == L'.')) ++end;
            out(line.substr(i, end - i), num);
            i = end;
        } else {
            size_t end = i + 1;
            while (end < line.size() && !iswalnum(line[end]) && line[end] != L'_' &&
                   line[end] != L'"' && line[end] != L'/' && line[end] != L'#') ++end;
            out(line.substr(i, end - i), ink);
            i = end;
        }
    }
}

inline void PaintCode(HDC dc, const RECT& r, const Content& c) {
    HFONT font = MakeFont(15, FW_NORMAL, L"Consolas", true);
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    const bool py = c.title.find(L".py") != std::wstring::npos;
    const wchar_t* const* lines = py ? kCodePy : kCodeC;
    const int n = py ? Count(kCodePy) : Count(kCodeC);
    unsigned state = c.seed;
    const int lineHeight = 19, gutter = 44;
    // Gutter: a quieter strip with line numbers and a rule beside it.
    Fill(dc, r.left, r.top, r.left + gutter, r.bottom, Mix(Paper(c), Ink(c), 0.06f));
    Line(dc, r.left + gutter, r.top, r.left + gutter, r.bottom, Mix(Paper(c), Ink(c), 0.18f));
    const int first = (int)(NextRandom(state) % n);
    const int startNo = 1 + (int)(NextRandom(state) % 180);
    int k = 0;
    for (int y = r.top + 6; y + lineHeight <= r.bottom - 2; y += lineHeight, ++k) {
        wchar_t no[8];
        swprintf_s(no, L"%3d", startNo + k);
        SetTextColor(dc, Mix(Paper(c), Ink(c), 0.45f));
        TextOutW(dc, r.left + 8, y, no, lstrlenW(no));
        PaintCodeLine(dc, r.left + gutter + 10, y, lines[(first + k) % n], c, py);
    }
    SelectObject(dc, old);
    DeleteObject(font);
}

// ---------------------------------------------------------------- image
// No picture loaded yet (or none on this PC): a star chart in the game's own
// line language - one ink colour, thin lines, nodes, no fills.
inline void PaintChart(HDC dc, const RECT& p, const Content& c, COLORREF stage, unsigned state) {
    const int w = p.right - p.left, h = p.bottom - p.top;
    const COLORREF accent = c.dark ? RGB(150, 205, 235) : RGB(50, 95, 145);
    const COLORREF faint = Mix(stage, accent, 0.18f), mid = Mix(stage, accent, 0.45f);
    const auto rnd = [&](int n) { return n > 0 ? (int)(NextRandom(state) % (unsigned)n) : 0; };
    const int cx = p.left + w / 2, cy = p.top + h / 2, R = std::min(w, h) * 9 / 20;
    HPEN pen = CreatePen(PS_SOLID, 1, faint);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    for (int k = 1; k <= 3; ++k)   // declination rings
        Ellipse(dc, cx - R * k / 3, cy - R * k / 3, cx + R * k / 3, cy + R * k / 3);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
    for (int k = 0; k < 12; ++k) {   // hour lines
        const float a = 6.2831853f * k / 12.0f;
        Line(dc, cx, cy, cx + (int)(cosf(a) * R), cy + (int)(sinf(a) * R), faint);
    }
    for (int i = 0; i < w * h / 500; ++i) {   // field stars
        const int x = p.left + rnd(w), y = p.top + rnd(h);
        Fill(dc, x, y, x + 1, y + 1, Mix(stage, accent, 0.3f + 0.4f * (rnd(10) / 9.0f)));
    }
    // One constellation: a chain with a branch, nodes as small rings.
    POINT pts[8];
    const int n = 5 + rnd(3);
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * (rnd(1000) / 1000.0f), d = R * (0.15f + 0.75f * rnd(1000) / 1000.0f);
        pts[i] = { cx + (int)(cosf(a) * d), cy + (int)(sinf(a) * d) };
    }
    for (int i = 1; i < n; ++i) {
        const POINT& from = pts[i == n - 1 ? n / 2 : i - 1];
        Line(dc, from.x, from.y, pts[i].x, pts[i].y, mid);
    }
    HPEN ring = CreatePen(PS_SOLID, 1, accent);
    op = SelectObject(dc, ring); ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    for (int i = 0; i < n; ++i) {
        Ellipse(dc, pts[i].x - 3, pts[i].y - 3, pts[i].x + 4, pts[i].y + 4);
        Fill(dc, pts[i].x, pts[i].y, pts[i].x + 1, pts[i].y + 1, accent);
    }
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(ring);
}

inline void PaintImage(HDC dc, const RECT& r, const Content& c) {
    // Viewer chrome: a darker stage, the picture fitted with a margin, a
    // status line under it.
    const COLORREF stage = c.dark ? RGB(22, 23, 27) : RGB(243, 244, 247);
    Fill(dc, r.left, r.top, r.right, r.bottom, stage);
    const int status = 22;
    const int aw = r.right - r.left - 24, ah = r.bottom - r.top - 24 - status;
    if (aw < 20 || ah < 20) return;
    const pane_images::Picture* src = c.picture.get();
    // Fit the picture's own aspect (a chart is 4:3).
    const int iw = src ? src->w : 4, ih = src ? src->h : 3;
    int pw = aw, ph = (int)((long long)aw * ih / iw);
    if (ph > ah) { ph = ah; pw = (int)((long long)ah * iw / ih); }
    RECT pic{ r.left + (r.right - r.left - pw) / 2, r.top + 12 + (ah - ph) / 2, 0, 0 };
    pic.right = pic.left + pw;
    pic.bottom = pic.top + ph;
    if (src) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = src->w;
        bi.bmiHeader.biHeight = -src->h;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, pic.left, pic.top, pw, ph, 0, 0, src->w, src->h, src->bgra.data(), &bi,
                      DIB_RGB_COLORS, SRCCOPY);
    } else {
        PaintChart(dc, pic, c, stage, c.seed * 2654435761u + 7u);
    }
    // Status line: file name, size, zoom.
    HFONT font = MakeFont(13, FW_NORMAL, L"Segoe UI", false);
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Mix(Paper(c), Ink(c), 0.6f));
    const size_t dash = c.title.find(L" - ");
    const std::wstring file = dash == std::wstring::npos ? c.title : c.title.substr(dash + 3);
    wchar_t info[400];
    const int sw = src ? src->srcW : 640 + (int)(c.seed % 5) * 320;
    const int sh = src ? src->srcH : 480 + (int)(c.seed % 5) * 240;
    const int zoom = std::max(1, (int)(100.0f * pw / std::max(1, sw) + 0.5f));
    swprintf_s(info, L"%ls    %d x %d    %d%%", file.c_str(), sw, sh, zoom);
    TextOutW(dc, r.left + 12, r.bottom - status, info, lstrlenW(info));
    SelectObject(dc, old);
    DeleteObject(font);
}

// ---------------------------------------------------------------- glitch
// A worn pane's whole client breaks up as one picture (u 0..1, about 0.35 at
// most while worn and 0.6..1 while its fuse burns), in the red / cyan
// anaglyph look:
//   - red pulled one way and cyan (green + blue) the other, wider with u
//   - short slabs (a few px tall, part of a row) slid sideways, their split
//     wider still
//   - thin streaks: one pixel dragged along its row, tinted red or cyan
// The pattern holds for kGlitchSliceMs at a time, so it jumps in steps
// rather than flickering.
constexpr unsigned kGlitchSliceMs = 80;
inline void Glitch(unsigned* px, int w, int h, float u, unsigned seed) {
    if (u <= 0.0f || w < 16 || h < 16) return;
    unsigned noise = seed ^ (unsigned)(GetTickCount64() / kGlitchSliceMs) * 2654435761u;
    const auto rnd = [&](int n) { return n > 0 ? (int)(NextRandom(noise) % (unsigned)n) : 0; };
    const std::vector<unsigned> src(px, px + (size_t)w * h);
    const auto at = [&](int x, int y) {
        x = std::clamp(x, 0, w - 1);
        return src[(size_t)y * w + x];
    };
    // Red from x + split, cyan from x - split, both read at x - shift.
    const auto pull = [&](int x, int y, int shift, int split) {
        return 0xFF000000u | (at(x - shift + split, y) & 0x00FF0000u) | (at(x - shift - split, y) & 0x0000FFFFu);
    };
    const int split = 2 + (int)(9.0f * u);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) px[(size_t)y * w + x] = pull(x, y, 0, split);
    const int slabs = 6 + (int)(44.0f * u);
    for (int k = 0; k < slabs; ++k) {
        const int sh = 2 + rnd(4 + (int)(10.0f * u));
        const int sw = 20 + rnd(w / 3 + (int)(w * 0.4f * u));
        const int y0 = rnd(h), x0 = rnd(w) - sw / 4;
        const int shift = (rnd(2) ? 1 : -1) * (6 + rnd(10 + (int)(50.0f * u)));
        const int wide = split + 2 + rnd(3 + (int)(8.0f * u));
        for (int y = y0; y < std::min(h, y0 + sh); ++y)
            for (int x = std::max(0, x0); x < std::min(w, x0 + sw); ++x)
                px[(size_t)y * w + x] = pull(x, y, shift, wide);
    }
    const int streaks = 10 + (int)(70.0f * u);
    for (int k = 0; k < streaks; ++k) {
        const int y = rnd(h), x0 = rnd(w), len = 30 + rnd(60 + (int)(w * 0.5f * u));
        unsigned c = at(x0, y);
        const bool red = rnd(2) == 0;   // red streak, or cyan
        int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        if (red) { r = std::min(255, r + 90); g = g * 2 / 3; b = b * 2 / 3; }
        else { r = r / 2; g = std::min(255, g + 50); b = std::min(255, b + 60); }
        c = 0xFF000000u | (unsigned)r << 16 | (unsigned)g << 8 | (unsigned)b;
        const int th = 1 + (rnd(4) == 0 ? 1 : 0);
        for (int yy = y; yy < std::min(h, y + th); ++yy)
            for (int x = x0; x < std::min(w, x0 + len); ++x) {
                // Fades out along its length, blended over what is there.
                const int a = 255 - 255 * (x - x0) / len;
                unsigned& d = px[(size_t)yy * w + x];
                const int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
                d = 0xFF000000u | (unsigned)(dr + ((r - dr) * a >> 8)) << 16 |
                    (unsigned)(dg + ((g - dg) * a >> 8)) << 8 | (unsigned)(db + ((b - db) * a >> 8));
            }
    }
}

// The whole client: paper, then the content, then the glitch. Double-buffered
// in a 32-bit DIB so the glitch can work on the pixels and a worn pane's
// repaint does not flash.
inline void Paint(HDC target, const RECT& r, const Content* c, bool darkIfNone) {
    const int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    HDC dc = CreateCompatibleDC(target);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(target, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return; }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    const RECT local{ 0, 0, w, h };
    const bool dark = c ? c->dark : darkIfNone;
    Fill(dc, 0, 0, w, h, dark ? kPaperDark : kPaperLight);
    if (c) {
        if (c->kind == Kind::Memo) PaintMemo(dc, local, *c);
        else if (c->kind == Kind::Code) PaintCode(dc, local, *c);
        else if (c->kind == Kind::Image) PaintImage(dc, local, *c);
        if (c->unstable > 0.0f) {
            GdiFlush();
            Glitch(static_cast<unsigned*>(bits), w, h, c->unstable, c->seed);
        }
    }
    BitBlt(target, r.left, r.top, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

}   // namespace pane_content
#endif
