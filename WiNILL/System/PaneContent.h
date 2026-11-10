#pragma once
// What a boss pane's client shows: a random memo, a random code file or a
// picture from the player's PC (PaneImages.h), painted with GDI (OsPane.cpp's
// WM_PAINT). Header-only and free of game state so
// tests/pane_content_preview.cpp can paint it to files.
// A worn pane (unstable > 0) glitches in short bursts (Glitch): bands of the
// client slide sideways and red and blue split apart.
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
// A pattern's pane shows a system dialog for its job instead of its own
// contents, and gets them back when the job ends: the wall's error windows,
// the countdown's closing notice, the shot box's copy progress.
enum class Role { None, Error, Shutdown, Copy, Safe };

// A worn pane's glitch timer (see Glitch below).
struct GlitchBurst {
    ULONGLONG endAt = 0, nextAt = 0;   // GetTickCount64 ms; nextAt 0 = not scheduled
    float seed = 0.0f, bands = 6.0f;
};

struct Content {
    Kind kind = Kind::None;
    unsigned seed = 0;
    bool dark = false;
    std::wstring title;
    float unstable = 0.0f;
    GlitchBurst glitch;
    Role role = Role::None;
    float roleA = 0.0f, roleB = 0.0f;   // Shutdown: seconds left, total; Copy: loaded, total
    int lang = 0;                       // 0 KR, 1 EN, 2 JP (role dialogs)
    std::wstring status;                // title suffix: worn / not responding
    std::wstring windowTitle;           // what the title bar shows now
    // Decor panes (the blue screen's storm) paint once into `cache` and
    // glitch by shifting bands of it with BitBlt (GlitchFast), so dozens of
    // them cost little. OsPane owns the bitmap.
    bool fast = false;
    HBITMAP cache = nullptr;
    int cacheW = 0, cacheH = 0;
    bool cacheDirty = true;
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

// ---------------------------------------------------------------- roles
// Title for a role dialog (none: the pane's own file title).
inline std::wstring RoleTitle(const Content& c, const wchar_t* base) {
    const int l = std::clamp(c.lang, 0, 2);
    static const wchar_t* const kError[3] = { L".exe - 응용 프로그램 오류", L".exe - Application Error",
                                              L".exe - アプリケーション エラー" };
    static const wchar_t* const kShut[3] = { L" - 창 닫는 중", L" - Closing window", L" - ウィンドウを閉じています" };
    static const wchar_t* const kCopy[3] = { L" - 탄 복사 중", L" - Copying shots", L" - 弾をコピー中" };
    static const wchar_t* const kSafe[3] = { L" - 안전 모드", L" - Safe Mode", L" - セーフ モード" };
    switch (c.role) {
    case Role::Error: return std::wstring(base) + kError[l];
    case Role::Shutdown: return std::wstring(base) + kShut[l];
    case Role::Copy: return std::wstring(base) + kCopy[l];
    case Role::Safe: return std::wstring(base) + kSafe[l];
    default: return c.title;
    }
}

inline void Text(HDC dc, int x, int y, const std::wstring& s, COLORREF col) {
    SetTextColor(dc, col);
    TextOutW(dc, x, y, s.c_str(), (int)s.size());
}

inline void Box(HDC dc, int l, int t, int w, int h, COLORREF edge) {
    Line(dc, l, t, l + w, t, edge);
    Line(dc, l + w, t, l + w, t + h, edge);
    Line(dc, l + w, t + h, l, t + h, edge);
    Line(dc, l, t + h, l, t, edge);
}

// A dialog button: an outlined box with its label centred.
inline void Button(HDC dc, int l, int t, int w, int h, const wchar_t* label, COLORREF edge,
                   COLORREF ink, COLORREF fill) {
    Fill(dc, l, t, l + w, t + h, fill);
    Box(dc, l, t, w, h, edge);
    RECT rc{ l, t, l + w, t + h };
    SetTextColor(dc, ink);
    DrawTextW(dc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// The round dialog icon: a disc with a cross (error) or a bar and dot (notice).
inline void DialogIcon(HDC dc, int cx, int cy, int r, COLORREF disc, bool cross) {
    HBRUSH br = CreateSolidBrush(disc);
    HPEN edge = CreatePen(PS_SOLID, 1, disc);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, edge);
    Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(edge);
    const COLORREF w = RGB(255, 255, 255);
    const int k = r * 2 / 5;
    if (cross) {
        Line(dc, cx - k, cy - k, cx + k + 1, cy + k + 1, w, 3);
        Line(dc, cx + k, cy - k, cx - k - 1, cy + k + 1, w, 3);
    } else {
        Line(dc, cx, cy - k, cx, cy + k / 3, w, 3);
        Fill(dc, cx - 1, cy + k * 2 / 3, cx + 2, cy + k * 2 / 3 + 3, w);
    }
}

// A progress bar: an outlined track and a filled share.
inline void Progress(HDC dc, int l, int t, int w, int h, float share, COLORREF edge, COLORREF bar) {
    Box(dc, l, t, w, h, edge);
    const int fw = (int)((w - 3) * std::clamp(share, 0.0f, 1.0f));
    if (fw > 0) Fill(dc, l + 2, t + 2, l + 2 + fw, t + h - 1, bar);
}

// Shared dialog layout: icon at the left, lines of text beside it, a strip
// of buttons along the bottom. The caller frees `font`.
struct DialogFrame { int textX, textY, lineH, stripTop; COLORREF paper, ink, faint, strip; };
inline DialogFrame BeginDialog(HDC dc, const RECT& r, const Content& c, HFONT& font) {
    DialogFrame f{};
    f.paper = Paper(c);
    f.ink = Ink(c);
    f.faint = Mix(f.paper, f.ink, 0.55f);
    f.strip = Mix(f.paper, f.ink, 0.07f);
    font = MakeFont(15, FW_NORMAL, c.lang == 2 ? L"Yu Gothic UI" : L"Malgun Gothic", false);
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    f.stripTop = (int)r.bottom - 52;
    Fill(dc, r.left, f.stripTop, r.right, r.bottom, f.strip);
    Line(dc, r.left, f.stripTop, r.right, f.stripTop, Mix(f.paper, f.ink, 0.15f));
    f.textX = (int)r.left + 84;
    f.textY = (int)r.top + 26;
    f.lineH = 22;
    return f;
}

// The wall's error window: Windows' crash dialog, addressed to the sky.
inline void PaintError(HDC dc, const RECT& r, const Content& c) {
    HFONT font = nullptr;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    const DialogFrame f = BeginDialog(dc, r, c, font);
    DialogIcon(dc, r.left + 44, r.top + 44, 18, RGB(232, 17, 35), true);
    const int l = std::clamp(c.lang, 0, 2);
    static const wchar_t* const kLines[3][3] = {
        { L"\"0x0000008\" 위치의 명령이 \"SEAL_BREACH\"", L"메모리를 참조했습니다. 이 영역은 벽이므로",
          L"아무도 들어올 수 없습니다." },
        { L"The instruction at \"0x0000008\" referenced", L"memory at \"SEAL_BREACH\". This region is a",
          L"wall: nothing may enter it." },
        { L"\"0x0000008\" の命令が \"SEAL_BREACH\" の", L"メモリを参照しました。この領域は壁なので",
          L"誰も入ることはできません。" } };
    for (int i = 0; i < 3; ++i) Text(dc, f.textX, f.textY + f.lineH * i, kLines[l][i], f.ink);
    static const wchar_t* const kHint[3] = { L"프로그램을 종료하려면 [확인]을 클릭하십시오.",
        L"Click OK to terminate the program.", L"プログラムを終了するには [OK] をクリックしてください。" };
    Text(dc, f.textX, f.textY + f.lineH * 3 + 10, kHint[l], f.faint);
    static const wchar_t* const kOk[3] = { L"확인", L"OK", L"OK" };
    static const wchar_t* const kCancel[3] = { L"취소", L"Cancel", L"キャンセル" };
    Button(dc, r.right - 196, f.stripTop + 12, 84, 28, kOk[l], RGB(0, 103, 192), f.ink, f.paper);
    Button(dc, r.right - 100, f.stripTop + 12, 84, 28, kCancel[l], Mix(f.paper, f.ink, 0.35f), f.ink, f.paper);
    SelectObject(dc, old);
    DeleteObject(font);
}

// The countdown pane: a closing notice with the seconds left and a draining bar.
inline void PaintShutdown(HDC dc, const RECT& r, const Content& c) {
    HFONT font = nullptr;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    const DialogFrame f = BeginDialog(dc, r, c, font);
    DialogIcon(dc, r.left + 44, r.top + 44, 18, RGB(124, 92, 214), false);
    const int l = std::clamp(c.lang, 0, 2);
    const int secs = std::max(0, (int)std::ceil(c.roleA - 0.001f));
    wchar_t line[128];
    static const wchar_t* const kFmt[3] = { L"이 창은 %d초 후에 닫힙니다.", L"This window closes in %d s.",
                                            L"このウィンドウは %d 秒後に閉じます。" };
    swprintf_s(line, kFmt[l], secs);
    Text(dc, f.textX, f.textY, line, f.ink);
    static const wchar_t* const kWarn[3] = { L"안에 있는 모든 것이 함께 닫힙니다.",
        L"Everything inside closes with it.", L"中にあるものはすべて一緒に閉じられます。" };
    Text(dc, f.textX, f.textY + f.lineH, kWarn[l], f.faint);
    const int bw = std::max(40, (int)r.right - f.textX - 24);
    Progress(dc, f.textX, f.textY + f.lineH * 2 + 14, bw, 14,
             c.roleB > 0.0f ? c.roleA / c.roleB : 0.0f, Mix(f.paper, f.ink, 0.35f), RGB(124, 92, 214));
    static const wchar_t* const kNow[3] = { L"지금 닫기", L"Close now", L"今すぐ閉じる" };
    Button(dc, r.right - 120, f.stripTop + 12, 104, 28, kNow[l], Mix(f.paper, f.ink, 0.25f), f.faint, f.strip);
    SelectObject(dc, old);
    DeleteObject(font);
}

// The shot box: a copy dialog loading shots instead of files.
inline void PaintCopy(HDC dc, const RECT& r, const Content& c) {
    HFONT font = nullptr;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    const DialogFrame f = BeginDialog(dc, r, c, font);
    const int l = std::clamp(c.lang, 0, 2);
    const int x = (int)r.left + 18, y = (int)r.top + 16;
    const int loaded = (int)(c.roleA + 0.5f), total = std::max(1, (int)(c.roleB + 0.5f));
    wchar_t line[128];
    static const wchar_t* const kFmt[3] = { L"탄 %d개 복사 중", L"Copying %d shots", L"弾を %d 個コピー中" };
    swprintf_s(line, kFmt[l], total);
    Text(dc, x, y, line, f.ink);
    static const wchar_t* const kPath[3] = { L"OCTANS 에서 플레이어 위치 로", L"from OCTANS to your position",
                                             L"OCTANS から プレイヤーの位置 へ" };
    Text(dc, x, y + f.lineH, kPath[l], f.faint);
    const int bw = std::max(40, (int)(r.right - r.left) - 36);
    Progress(dc, x, y + f.lineH * 2 + 8, bw, 14, (float)loaded / total, Mix(f.paper, f.ink, 0.35f),
             RGB(6, 176, 37));
    static const wchar_t* const kLeft[3] = { L"남은 항목: %d개", L"Items remaining: %d", L"残りの項目: %d" };
    swprintf_s(line, kLeft[l], std::max(0, total - loaded));
    Text(dc, x, y + f.lineH * 3 + 12, line, f.faint);
    static const wchar_t* const kCancel[3] = { L"취소", L"Cancel", L"キャンセル" };
    Button(dc, r.right - 100, f.stripTop + 12, 84, 28, kCancel[l], Mix(f.paper, f.ink, 0.35f), f.ink, f.paper);
    SelectObject(dc, old);
    DeleteObject(font);
}

// The blue screen's one shelter: "restart in safe mode", with the seconds
// left before the crash and a bar draining toward it.
inline void PaintSafe(HDC dc, const RECT& r, const Content& c) {
    HFONT font = nullptr;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    const DialogFrame f = BeginDialog(dc, r, c, font);
    DialogIcon(dc, r.left + 44, r.top + 44, 18, RGB(0, 120, 212), false);
    const int l = std::clamp(c.lang, 0, 2);
    static const wchar_t* const kHead[3] = { L"안전 모드로 다시 시작하는 중", L"Restarting in safe mode",
                                             L"セーフ モードで再起動しています" };
    Text(dc, f.textX, f.textY, kHead[l], f.ink);
    static const wchar_t* const kBody[3] = { L"이 창 안에 있으면 오류에서 보호됩니다.",
        L"Stay inside this window to be protected.", L"このウィンドウの中にいれば保護されます。" };
    Text(dc, f.textX, f.textY + f.lineH, kBody[l], f.faint);
    wchar_t line[96];
    static const wchar_t* const kFmt[3] = { L"남은 시간: %d초", L"Time left: %d s", L"残り時間: %d 秒" };
    swprintf_s(line, kFmt[l], std::max(0, (int)std::ceil(c.roleA - 0.001f)));
    Text(dc, f.textX, f.textY + f.lineH * 2 + 6, line, f.ink);
    const int bw = std::max(40, (int)r.right - f.textX - 24);
    Progress(dc, f.textX, f.textY + f.lineH * 3 + 14, bw, 14,
             c.roleB > 0.0f ? c.roleA / c.roleB : 0.0f, Mix(f.paper, f.ink, 0.35f), RGB(0, 120, 212));
    SelectObject(dc, old);
    DeleteObject(font);
}

// ---------------------------------------------------------------- glitch
// A worn pane glitches in short bursts (the user's band-shift shader, run on
// the CPU over the client's pixels). Between bursts the pane is clean.
//   - a burst lasts kGlitchBurstMs; the wait before the next one shrinks with
//     the burst frequency, and at frequency 1 bursts run back to back
//   - each burst rolls a new seed and band count; a share of the horizontal
//     bands (fill) slides sideways (wrapping round) and a little up or down
//   - in a sliding band red and blue split apart, outward from the centre
// Intensity is the pane's unstable level (about 0.35 at most while worn,
// 0.6..1 while its fuse burns); a burning fuse glitches back to back.
constexpr unsigned kGlitchBurstMs = 140;

inline float GlitchHash(float n) {
    const double v = std::sin((double)n) * 43758.5453123;
    return (float)(v - std::floor(v));
}

// Burst frequency 0..1 for an unstable level: worn panes glitch now and
// then, a burning fuse all the time.
inline float GlitchFrequency(float u) { return u >= 0.6f ? 1.0f : u; }

// Advances the burst timer (call once per frame while the pane is unstable).
inline void AdvanceGlitch(GlitchBurst& g, float u, unsigned salt) {
    const ULONGLONG now = GetTickCount64();
    if (u <= 0.0f) { g = {}; return; }
    unsigned noise = salt ^ (unsigned)now * 2654435761u;
    const auto frand = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(NextRandom(noise) % 10001) / 10000.0f;
    };
    const float f = GlitchFrequency(u);
    const auto gap = [&]() -> ULONGLONG {
        if (f >= 1.0f) return 0;
        const float lo = 4.0f + (0.25f - 4.0f) * f, hi = 8.0f + (0.9f - 8.0f) * f;
        return (ULONGLONG)(1000.0f * frand(lo, hi));
    };
    if (g.nextAt == 0) g.nextAt = now + gap();
    if (now >= g.nextAt && now >= g.endAt) {
        g.endAt = now + kGlitchBurstMs;
        g.seed = frand(0.0f, 1000.0f);
        g.bands = frand(6.0f, 8.0f + u * 14.0f);
        g.nextAt = g.endAt + gap();
    }
}

inline bool GlitchActive(const GlitchBurst& g) { return GetTickCount64() < g.endAt; }

inline void Glitch(unsigned* px, int w, int h, float u, const GlitchBurst& g) {
    if (u <= 0.0f || w < 16 || h < 16 || !GlitchActive(g)) return;
    const float fill = std::min(0.85f, 0.15f + u * 0.16f);
    const float shift = std::min(0.22f, 0.02f + u * 0.05f);
    const float vshift = std::min(0.09f, 0.01f + u * 0.025f);
    const float chroma = u * 0.7f;
    const std::vector<unsigned> src(px, px + (size_t)w * h);
    const auto sample = [&](float sx, float sy) {
        const int x = std::clamp((int)(sx * w), 0, w - 1), y = std::clamp((int)(sy * h), 0, h - 1);
        return src[(size_t)y * w + x];
    };
    for (int y = 0; y < h; ++y) {
        const float v = (y + 0.5f) / h;
        const float row = std::floor(v * g.bands);
        const float hit = GlitchHash(row * 12.9898f + g.seed) <= fill ? 1.0f : 0.0f;
        const float dir = GlitchHash(row * 78.233f + g.seed) >= 0.5f ? 1.0f : -1.0f;
        const float mag = 0.3f + 0.7f * GlitchHash(row * 37.719f + g.seed);
        const float vdir = GlitchHash(row * 91.345f + g.seed) >= 0.5f ? 1.0f : -1.0f;
        const float boost = 1.0f + hit * chroma * mag;
        const float ca = (boost - 1.0f) * 0.015f;
        const float sv = std::clamp(v + hit * vdir * vshift * mag, 0.0f, 1.0f);
        for (int x = 0; x < w; ++x) {
            const float uu = (x + 0.5f) / w;
            float su = uu + hit * dir * shift * mag;
            su -= std::floor(su);   // slides off one side, back in on the other
            // Red and blue pulled apart along the line from the centre.
            const float cx = uu - 0.5f, cy = v - 0.5f;
            const float len = std::sqrt(cx * cx + cy * cy) + 0.0001f;
            const float dx = cx / len * ca, dy = cy / len * ca;
            const unsigned r = sample(su + dx, sv + dy) & 0x00FF0000u;
            const unsigned gch = sample(su, sv) & 0x0000FF00u;
            const unsigned b = sample(su - dx, sv - dy) & 0x000000FFu;
            px[(size_t)y * w + x] = 0xFF000000u | r | gch | b;
        }
    }
}

// The cheap glitch for decor panes: the same bursts and band rolls as
// Glitch, but each sliding band is copied from the cached picture with two
// BitBlts (wrapping round) and edged with a red line above and a cyan line
// below instead of splitting channels per pixel.
inline void GlitchFast(HDC dst, HDC src, int w, int h, float u, const GlitchBurst& g) {
    BitBlt(dst, 0, 0, w, h, src, 0, 0, SRCCOPY);
    if (u <= 0.0f || w < 16 || h < 16 || !GlitchActive(g)) return;
    const float fill = std::min(0.85f, 0.15f + u * 0.16f);
    const float shift = std::min(0.22f, 0.02f + u * 0.05f);
    const float vshift = std::min(0.09f, 0.01f + u * 0.025f);
    const int bands = std::max(1, (int)g.bands);
    for (int row = 0; row < bands; ++row) {
        const float fr = (float)row;
        if (GlitchHash(fr * 12.9898f + g.seed) > fill) continue;
        const float dir = GlitchHash(fr * 78.233f + g.seed) >= 0.5f ? 1.0f : -1.0f;
        const float mag = 0.3f + 0.7f * GlitchHash(fr * 37.719f + g.seed);
        const float vdir = GlitchHash(fr * 91.345f + g.seed) >= 0.5f ? 1.0f : -1.0f;
        const int y0 = h * row / bands, y1 = h * (row + 1) / bands, bh = y1 - y0;
        int dx = (int)(dir * shift * mag * w) % w;
        if (dx < 0) dx += w;
        const int sy = std::clamp(y0 - (int)(vdir * vshift * mag * h), 0, h - bh);
        // The band read from x - dx: its right part lands at dx, the rest wraps to 0.
        BitBlt(dst, dx, y0, w - dx, bh, src, 0, sy, SRCCOPY);
        if (dx > 0) BitBlt(dst, 0, y0, dx, bh, src, w - dx, sy, SRCCOPY);
        Fill(dst, 0, y0, w, y0 + 1, RGB(232, 40, 52));
        Fill(dst, 0, y1 - 1, w, y1, RGB(40, 210, 214));
    }
}

// The whole client: paper, then the content, then the glitch. Double-buffered
// in a 32-bit DIB so the glitch can work on the pixels and a worn pane's
// repaint does not flash.
// scale: window pixels per layout unit. The painters lay out in units (fixed
// line heights, fonts, margins), so a pane shows the same lines at every
// monitor resolution; GDI's world transform draws them at the pane's real size.
inline void Paint(HDC target, const RECT& r, const Content* c, bool darkIfNone,
                  float scale = 1.0f) {
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
    if (!(scale > 0.0f)) scale = 1.0f;
    const bool scaled = std::fabs(scale - 1.0f) > 0.001f;
    if (scaled) {
        SetGraphicsMode(dc, GM_ADVANCED);
        const XFORM xf{ scale, 0.0f, 0.0f, scale, 0.0f, 0.0f };
        SetWorldTransform(dc, &xf);
    }
    const RECT local{ 0, 0, (LONG)std::ceil(w / scale), (LONG)std::ceil(h / scale) };
    const bool dark = c ? c->dark : darkIfNone;
    Fill(dc, 0, 0, local.right, local.bottom, dark ? kPaperDark : kPaperLight);
    if (c) {
        if (c->role == Role::Error) PaintError(dc, local, *c);
        else if (c->role == Role::Shutdown) PaintShutdown(dc, local, *c);
        else if (c->role == Role::Copy) PaintCopy(dc, local, *c);
        else if (c->role == Role::Safe) PaintSafe(dc, local, *c);
        else if (c->kind == Kind::Memo) PaintMemo(dc, local, *c);
        else if (c->kind == Kind::Code) PaintCode(dc, local, *c);
        else if (c->kind == Kind::Image) PaintImage(dc, local, *c);
        if (scaled) ModifyWorldTransform(dc, nullptr, MWT_IDENTITY);
        if (c->unstable > 0.0f) {
            GdiFlush();
            Glitch(static_cast<unsigned*>(bits), w, h, c->unstable, c->glitch);
        }
    }
    if (scaled) ModifyWorldTransform(dc, nullptr, MWT_IDENTITY);   // BitBlt reads raw pixels
    BitBlt(target, r.left, r.top, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

}   // namespace pane_content
#endif
