#include "OsPane.h"

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <string>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdio>
#include <filesystem>
#include "Settings.h"
#include "PaneContent.h"
#include "Audio.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace os_pane {
namespace {

constexpr wchar_t kClassName[] = L"AsteriaPane";
constexpr float kRefusalSeconds = 1.6f;
// Below this share of durability the title bar shows what is left.
constexpr float kWornTitleShare = 0.30f;
constexpr float kDesktopScanSeconds = 0.5f;
// Windows smaller than this, or covering nearly the whole game, are not
// candidates for a combat space.
constexpr float kMinDesktopW = 200.0f, kMinDesktopH = 120.0f;
constexpr float kMaxDesktopCover = 0.90f;
// Windows mostly hidden behind others in front of them (a maximized window,
// say) are not candidates either: at least this share must be in sight,
// measured on a kVisibleSamples x kVisibleSamples grid.
constexpr float kMinVisibleShare = 0.5f;
constexpr int   kVisibleSamples = 8;
constexpr DWORD kDarkModeAttribute = 20;   // DWMWA_USE_IMMERSIVE_DARK_MODE

GLFWwindow* s_overlay = nullptr;
HWND s_overlayHwnd = nullptr;
// Hidden owner for every pane: keeps panes off the taskbar without tying
// their z-order to the game the way owning them by the overlay would.
HWND s_ownerHwnd = nullptr;
std::vector<Pane> s_panes;
std::vector<HWND> s_handles;               // parallel to s_panes

// What a pane's client shows (parallel to s_panes): a random memo, code
// file or image (PaneContent.h). Adopted windows paint themselves and keep an
// empty entry.
using pane_content::Content;
std::vector<Content> s_content;

bool UserPrefersDark() {
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return light == 0;
}

std::vector<Rect> s_desktop;
std::vector<HWND> s_desktopHandles;        // parallel to s_desktop
float s_scanTimer = 0.0f;
bool s_arrange = false;
bool s_hold = false;
bool s_away = false;          // the player switched to another program
bool s_coverCheck = true;     // off for smoke captures (see SetCoverCheck)
bool s_sunk = false;          // arrange mode lowered the game below other windows
bool s_passthrough = false;   // arrange mode fell back to mouse pass-through
bool s_classReady = false;
int s_nextId = 1;

int IndexOfHandle(HWND hwnd) {
    for (size_t i = 0; i < s_handles.size(); ++i)
        if (s_handles[i] == hwnd) return (int)i;
    return -1;
}

int IndexOfId(int id) {
    for (size_t i = 0; i < s_panes.size(); ++i)
        if (s_panes[i].id == id && s_panes[i].open) return (int)i;
    return -1;
}

const wchar_t* BaseTitle() { return L"OCTANS"; }

// A pane's own title (its "file name", or its role dialog's), or the bare base title.
std::wstring TitleOf(int i) {
    if (i >= 0 && i < (int)s_content.size()) {
        const std::wstring t = pane_content::RoleTitle(s_content[i], BaseTitle());
        if (!t.empty()) return t;
    }
    return BaseTitle();
}

const wchar_t* RefusalSuffix() {
    const int li = LangIndex();
    return li == 0 ? L" (응답 없음)" : (li == 2 ? L" (応答なし)" : L" (Not Responding)");
}

POINT OverlayOrigin() {
    POINT origin{ 0, 0 };
    if (s_overlayHwnd) ClientToScreen(s_overlayHwnd, &origin);
    return origin;
}

// Screen pixels to overlay units (the game's logical space, see g_PixelScale).
Rect ToOverlay(const RECT& r) {
    const POINT o = OverlayOrigin();
    return { FromPx((float)(r.left - o.x)), FromPx((float)(r.top - o.y)),
             FromPx((float)(r.right - r.left)), FromPx((float)(r.bottom - r.top)) };
}

// The overlay's client size in overlay units.
void OverlaySize(float& w, float& h) {
    RECT oc{};
    GetClientRect(s_overlayHwnd, &oc);
    w = FromPx((float)(oc.right - oc.left));
    h = FromPx((float)(oc.bottom - oc.top));
}

// Visible bounds: GetWindowRect includes the invisible resize border.
RECT FrameBounds(HWND hwnd) {
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
        GetWindowRect(hwnd, &r);
    return r;
}

void RefreshRects(int i) {
    HWND hwnd = s_handles[i];
    if (!hwnd) return;
    Pane& p = s_panes[i];
    p.frame = ToOverlay(FrameBounds(hwnd));
    RECT cr{};
    GetClientRect(hwnd, &cr);
    POINT tl{ cr.left, cr.top }, br{ cr.right, cr.bottom };
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &br);
    p.client = ToOverlay(RECT{ tl.x, tl.y, br.x, br.y });
    p.minimized = IsIconic(hwnd) != FALSE;
}

// Puts the title bar in line: its title, then the refusal note while one
// shows, else its status (worn, not responding). Only sets it when it changed.
void ApplyTitle(int i) {
    if (i < 0 || i >= (int)s_content.size() || s_panes[i].adopted) return;
    Content& c = s_content[i];
    const std::wstring title = TitleOf(i) + (s_panes[i].refusalTimer > 0.0f ? RefusalSuffix() : c.status);
    if (title == c.windowTitle) return;
    c.windowTitle = title;
    SetWindowTextW(s_handles[i], title.c_str());
}

void Refuse(int i) {
    Pane& p = s_panes[i];
    ++p.refusals;
    p.refusalTimer = kRefusalSeconds;
    ApplyTitle(i);
}

// Win32 title bars stay light unless asked; follow the user's app theme.
void ApplyTheme(HWND hwnd) {
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    const BOOL dark = light == 0 ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDarkModeAttribute, &dark, sizeof(dark));
}

void PaintClient(HWND hwnd, HDC dc) {
    RECT r{};
    GetClientRect(hwnd, &r);
    const int i = IndexOfHandle(hwnd);
    Content* c = (i >= 0 && i < (int)s_content.size()) ? &s_content[i] : nullptr;
    if (c && c->fast) {
        // Decor: paint the dialog once into the cache, then copy (and glitch) it.
        const int w = r.right - r.left, h = r.bottom - r.top;
        if (w <= 0 || h <= 0) return;
        HDC mem = CreateCompatibleDC(dc);
        if (!c->cache || c->cacheW != w || c->cacheH != h || c->cacheDirty) {
            if (c->cache) DeleteObject(c->cache);
            c->cache = CreateCompatibleBitmap(dc, w, h);
            c->cacheW = w;
            c->cacheH = h;
            c->cacheDirty = false;
            HGDIOBJ prev = SelectObject(mem, c->cache);
            Content still = *c;
            still.unstable = 0.0f;
            still.cache = nullptr;
            pane_content::Paint(mem, RECT{ 0, 0, w, h }, &still, UserPrefersDark(), g_PixelScale);
            SelectObject(mem, prev);
        }
        HDC back = CreateCompatibleDC(dc);
        HBITMAP backBmp = CreateCompatibleBitmap(dc, w, h);
        HGDIOBJ prevBack = SelectObject(back, backBmp);
        HGDIOBJ prevMem = SelectObject(mem, c->cache);
        pane_content::GlitchFast(back, mem, w, h, c->unstable, c->glitch);
        BitBlt(dc, r.left, r.top, w, h, back, 0, 0, SRCCOPY);
        SelectObject(mem, prevMem);
        SelectObject(back, prevBack);
        DeleteObject(backBmp);
        DeleteDC(back);
        DeleteDC(mem);
        return;
    }
    // A worn pane's content breaks up and tints as its warning.
    pane_content::Paint(dc, r, c, UserPrefersDark(), g_PixelScale);
}

LRESULT CALLBACK PaneProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    const int i = IndexOfHandle(hwnd);
    switch (msg) {
    case WM_MOUSEACTIVATE:
        // Dragging a pane must not take keyboard focus from the game.
        return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd, msg, wp, lp);
        return hit == HTCLIENT ? HTTRANSPARENT : hit;
    }
    case WM_ERASEBKGND:
        return 1;   // WM_PAINT covers the whole client
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        PaintClient(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SYSCOMMAND: {
        const WPARAM cmd = wp & 0xFFF0;
        if (i >= 0 && s_panes[i].locked && (cmd == SC_MINIMIZE || cmd == SC_CLOSE)) {
            Refuse(i);
            return 0;
        }
        if (i >= 0 && (cmd == SC_MINIMIZE || cmd == SC_CLOSE)) Audio::Play(Audio::Cue::OsArrange);   // 플레이어가 창 최소화/닫기
        break;
    }
    case WM_ENTERSIZEMOVE:
        if (i >= 0) Audio::Play(Audio::Cue::OsArrange);   // 플레이어가 창을 잡아 끌기 시작
        break;
    case WM_CLOSE:
        if (i >= 0 && s_panes[i].locked) {
            Refuse(i);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_WINDOWPOSCHANGED:
        if (i >= 0) {
            // Restacking below the game changes z-order only; not a move.
            const WINDOWPOS* pos = reinterpret_cast<const WINDOWPOS*>(lp);
            if ((pos->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE))
                ++s_panes[i].moves;
            RefreshRects(i);
        }
        break;
    case WM_SIZE:
        if (i >= 0) RefreshRects(i);
        break;
    case WM_DESTROY:
        if (i >= 0) {
            s_panes[i].open = false;
            s_handles[i] = nullptr;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool EnsureClass() {
    if (s_classReady) return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PaneProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    s_classReady = RegisterClassExW(&wc) != 0;
    return s_classReady;
}

// Panes live just below the game so combat draws over their title bars and
// clicks reach the game. Only arrange mode lets them rise above it.
bool AboveGame(HWND pane) {
    for (HWND h = GetWindow(s_overlayHwnd, GW_HWNDPREV); h; h = GetWindow(h, GW_HWNDPREV))
        if (h == pane) return true;
    return false;
}

void StackBelowGame(HWND pane) {
    SetWindowPos(pane, s_overlayHwnd, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool SameRect(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Moves a pane's in-game rectangle after its window once nothing holds it.
void UpdateShown(Pane& p, float dt) {
    if (!p.flying) {
        if (s_hold || SameRect(p.shown, p.frame)) return;
        p.flying = true;
        p.flightTime = 0.0f;
    }
    // Afterimages only where the rectangle really travelled, so the slow
    // creep does not pile them on one spot.
    const bool travelled = p.trail.empty() ||
        std::fabs(p.trail.back().frame.x - p.shown.x) +
        std::fabs(p.trail.back().frame.y - p.shown.y) >= 6.0f;
    if (travelled) p.trail.push_back({ p.shown, 0.0f });
    p.flightTime += dt;
    // Step toward the window's current rectangle (position and size as one
    // vector), so a window moved again mid-flight is still chased.
    const float dx = p.frame.x - p.shown.x, dy = p.frame.y - p.shown.y;
    const float dw = p.frame.w - p.shown.w, dh = p.frame.h - p.shown.h;
    const float remaining = std::sqrt(dx * dx + dy * dy + dw * dw + dh * dh);
    const float step = kStartSpeed * std::exp(p.flightTime / kSpeedRamp) * dt;
    if (step >= remaining) {
        p.shown = p.frame;
        p.flying = false;
        return;
    }
    const float k = step / remaining;
    p.shown = { p.shown.x + dx * k, p.shown.y + dy * k,
                p.shown.w + dw * k, p.shown.h + dh * k };
}

bool IsShellWindow(HWND hwnd) {
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    return lstrcmpW(cls, L"Progman") == 0 || lstrcmpW(cls, L"WorkerW") == 0 ||
           lstrcmpW(cls, L"Shell_TrayWnd") == 0 ||
           lstrcmpW(cls, L"Shell_SecondaryTrayWnd") == 0;
}

// Frames of the other programs' windows met so far in one scan, front first.
std::vector<Rect> s_scanFront;

// Share of r not covered by any of the rectangles in front of it.
float VisibleShare(const Rect& r, const std::vector<Rect>& front) {
    if (front.empty()) return 1.0f;
    int seen = 0;
    for (int j = 0; j < kVisibleSamples; ++j) {
        for (int i = 0; i < kVisibleSamples; ++i) {
            const float x = r.x + r.w * ((float)i + 0.5f) / (float)kVisibleSamples;
            const float y = r.y + r.h * ((float)j + 0.5f) / (float)kVisibleSamples;
            bool covered = false;
            for (const Rect& f : front)
                if (x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h) { covered = true; break; }
            if (!covered) ++seen;
        }
    }
    return (float)seen / (float)(kVisibleSamples * kVisibleSamples);
}

// Only rectangles are kept: titles and contents of other programs are never read.
BOOL CALLBACK CollectDesktopWindow(HWND hwnd, LPARAM) {
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    if (IsShellWindow(hwnd)) return TRUE;
    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;

    const Rect r = ToOverlay(FrameBounds(hwnd));
    // Every window seen so far is in front of this one (EnumWindows walks
    // front to back); it counts as hidden where they cover it.
    const float share = VisibleShare(r, s_scanFront);
    s_scanFront.push_back(r);
    if (share < kMinVisibleShare) return TRUE;
    if (r.w < kMinDesktopW || r.h < kMinDesktopH) return TRUE;
    float ow = 0.0f, oh = 0.0f;
    OverlaySize(ow, oh);
    const float ix = (std::max)(0.0f, (std::min)(r.x + r.w, ow) - (std::max)(r.x, 0.0f));
    const float iy = (std::max)(0.0f, (std::min)(r.y + r.h, oh) - (std::max)(r.y, 0.0f));
    if (ix * iy <= 0.0f) return TRUE;
    if (ix * iy >= ow * oh * kMaxDesktopCover) return TRUE;
    s_desktop.push_back(r);   // EnumWindows walks front to back
    s_desktopHandles.push_back(hwnd);
    return TRUE;
}

// The top-level window hosting the desktop icons (Progman, or a WorkerW
// when a wallpaper engine reparents it).
BOOL CALLBACK FindShellView(HWND hwnd, LPARAM out) {
    if (FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr)) {
        *reinterpret_cast<HWND*>(out) = hwnd;
        return FALSE;
    }
    return TRUE;
}

// The front-most window of another program that covers (nearly) the whole
// game area: a maximized or full-screen window. Sinking the game below it
// would hide the game, so arrange mode stops just above it.
HWND CoveringWindow() {
    if (!s_overlayHwnd) return nullptr;
    float ow = 0.0f, oh = 0.0f;
    OverlaySize(ow, oh);
    const float area = ow * oh;
    if (area <= 0.0f) return nullptr;
    for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (!IsWindowVisible(h) || IsIconic(h) || IsShellWindow(h)) continue;
        if (GetWindowLongW(h, GWL_EXSTYLE) & (WS_EX_TOOLWINDOW | WS_EX_TOPMOST)) continue;
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == GetCurrentProcessId()) continue;
        BOOL cloaked = FALSE;
        DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (cloaked) continue;
        const Rect r = ToOverlay(FrameBounds(h));
        const float ix = (std::max)(0.0f, (std::min)(r.x + r.w, ow) - (std::max)(r.x, 0.0f));
        const float iy = (std::max)(0.0f, (std::min)(r.y + r.h, oh) - (std::max)(r.y, 0.0f));
        if (ix * iy >= area * 0.95f) return h;
    }
    return nullptr;
}

HWND DesktopShellWindow() {
    HWND shell = nullptr;
    EnumWindows(FindShellView, reinterpret_cast<LPARAM>(&shell));
    return shell;
}

// Brings the game back on top and gives it keyboard focus after the player
// clicked another program during arrange mode. Plain HWND_TOP is ignored
// while that program owns the foreground, so pass through the topmost band
// (z-order only) and borrow the foreground thread's input to take focus.
void RaiseOverlay() {
    const UINT keep = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    SetWindowPos(s_overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, keep);
    SetWindowPos(s_overlayHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, keep);
    HWND foreground = GetForegroundWindow();
    if (foreground == s_overlayHwnd) return;
    const DWORD self = GetCurrentThreadId();
    const DWORD other = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const bool attached = other && other != self && AttachThreadInput(self, other, TRUE);
    SetForegroundWindow(s_overlayHwnd);
    SetFocus(s_overlayHwnd);
    if (attached) AttachThreadInput(self, other, FALSE);
}

void ScanDesktop() {
    s_desktop.clear();
    s_desktopHandles.clear();
    s_scanFront.clear();
    if (s_overlayHwnd) EnumWindows(CollectDesktopWindow, 0);
}

} // namespace

void Init(GLFWwindow* overlay) {
    s_overlay = overlay;
    s_overlayHwnd = overlay ? glfwGetWin32Window(overlay) : nullptr;
    // Decode a few pictures from this PC in the background for image panes.
    pane_images::Ensure(g_PanePhotos);
}

void Shutdown() {
    SetArrangeMode(false);
    for (size_t i = 0; i < s_handles.size(); ++i)
        if (s_handles[i] && !s_panes[i].adopted) DestroyWindow(s_handles[i]);
    if (s_ownerHwnd) DestroyWindow(s_ownerHwnd);
    s_ownerHwnd = nullptr;
    s_panes.clear();
    s_handles.clear();
    for (Content& c : s_content) if (c.cache) DeleteObject(c.cache);
    s_content.clear();
    s_desktop.clear();
    pane_images::Stop();
}

int Open(float x, float y, float w, float h, bool decor) {
    if (!s_overlayHwnd || !EnsureClass()) return 0;
    if (!s_ownerHwnd)
        s_ownerHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"", WS_POPUP,
                                      0, 0, 0, 0, nullptr, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
    const POINT o = OverlayOrigin();
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                        WS_MINIMIZEBOX | WS_THICKFRAME;
    const DWORD exStyle = WS_EX_NOACTIVATE;
    const int px = ToPx(x), py = ToPx(y), pw = ToPx(w), ph = ToPx(h);
    HWND hwnd = CreateWindowExW(exStyle, kClassName, BaseTitle(), style,
                                o.x + px, o.y + py, pw, ph,
                                s_ownerHwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) return 0;
    ApplyTheme(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    // Place the visible frame, not the invisible resize border, at (x, y, w, h).
    RECT wr{};
    GetWindowRect(hwnd, &wr);
    const RECT fr = FrameBounds(hwnd);
    const int left = fr.left - wr.left, top = fr.top - wr.top;
    const int right = wr.right - fr.right, bottom = wr.bottom - fr.bottom;
    SetWindowPos(hwnd, nullptr, o.x + px - left, o.y + py - top,
                 pw + left + right, ph + top + bottom,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (!s_arrange) StackBelowGame(hwnd);

    Pane pane;
    pane.id = s_nextId++;
    pane.open = true;
    pane.decor = decor;
    s_panes.push_back(pane);
    s_handles.push_back(hwnd);
    Content content;
    content.seed = (unsigned)rand() * 2654435761u + (unsigned)pane.id;
    content.dark = UserPrefersDark();
    if (decor) {
        content.kind = pane_content::Kind::Memo;
        content.role = pane_content::Role::Error;
        content.fast = true;
    } else {
        pane_images::Ensure(g_PanePhotos);   // reloads if the photos setting changed
        pane_content::Roll(content, BaseTitle());   // memo, code or image, and its file title
    }
    content.lang = LangIndex();
    s_content.push_back(content);
    ApplyTitle((int)s_content.size() - 1);
    InvalidateRect(hwnd, nullptr, TRUE);
    RefreshRects((int)s_panes.size() - 1);
    s_panes.back().shown = s_panes.back().frame;
    return pane.id;
}

int Adopt(int desktopIndex) {
    if (desktopIndex < 0 || desktopIndex >= (int)s_desktopHandles.size()) return 0;
    HWND hwnd = s_desktopHandles[desktopIndex];
    if (!IsWindow(hwnd)) return 0;
    for (size_t i = 0; i < s_handles.size(); ++i)
        if (s_handles[i] == hwnd && s_panes[i].open) return s_panes[i].id;
    Pane pane;
    pane.id = s_nextId++;
    pane.open = true;
    pane.adopted = true;
    s_panes.push_back(pane);
    s_handles.push_back(hwnd);
    s_content.push_back({});
    RefreshRects((int)s_panes.size() - 1);
    s_panes.back().shown = s_panes.back().frame;
    return pane.id;
}

void Close(int id) {
    const int i = IndexOfId(id);
    if (i < 0) return;
    s_panes[i].locked = false;
    if (s_panes[i].adopted) {
        // Not ours to close: just stop treating it as a space.
        s_panes[i].open = false;
        s_handles[i] = nullptr;
        return;
    }
    DestroyWindow(s_handles[i]);
}

void CloseAll() {
    for (const Pane& p : s_panes)
        if (p.open) Close(p.id);
}

void Place(int id, float x, float y) {
    const int i = IndexOfId(id);
    if (i < 0) return;
    Pane& p = s_panes[i];
    if (p.adopted || p.flying || p.minimized) return;
    RECT wr{};
    GetWindowRect(s_handles[i], &wr);
    const RECT fr = FrameBounds(s_handles[i]);
    const POINT o = OverlayOrigin();
    SetWindowPos(s_handles[i], nullptr,
                 o.x + ToPx(x) - (fr.left - wr.left), o.y + ToPx(y) - (fr.top - wr.top), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    RefreshRects(i);
    p.shown = p.frame;
}

void Resize(int id, float x, float y, float w, float h) {
    const int i = IndexOfId(id);
    if (i < 0) return;
    Pane& p = s_panes[i];
    if (p.adopted || p.flying || p.minimized) return;
    RECT wr{};
    GetWindowRect(s_handles[i], &wr);
    const RECT fr = FrameBounds(s_handles[i]);
    const int left = fr.left - wr.left, top = fr.top - wr.top;
    const int right = wr.right - fr.right, bottom = wr.bottom - fr.bottom;
    const POINT o = OverlayOrigin();
    SetWindowPos(s_handles[i], nullptr, o.x + ToPx(x) - left, o.y + ToPx(y) - top,
                 ToPx(w) + left + right, ToPx(h) + top + bottom, SWP_NOZORDER | SWP_NOACTIVATE);
    RefreshRects(i);
    p.shown = p.frame;
}

void SetUnstable(int id, float level) {
    const int i = IndexOfId(id);
    if (i < 0 || i >= (int)s_content.size()) return;
    Content& c = s_content[i];
    if (c.unstable == level && level <= 0.0f) return;
    const bool wasGlitching = pane_content::GlitchActive(c.glitch);
    const bool changed = c.unstable != level;
    c.unstable = level;
    pane_content::AdvanceGlitch(c.glitch, level, c.seed);
    // 글리치 버스트가 새로 시작될 때만. 퓨즈·블루스크린처럼 쉴 새 없이 튀는 단계(빈도 1)는 그쪽 Cue 가 맡는다.
    if (!wasGlitching && pane_content::GlitchFrequency(level) < 1.0f && pane_content::GlitchActive(c.glitch))
        Audio::Play(Audio::Cue::OsPaneGlitch);
    // Repaint while a burst runs, once more as it ends, or when the level moved.
    if (changed || wasGlitching || pane_content::GlitchActive(c.glitch))
        InvalidateRect(s_handles[i], nullptr, FALSE);
}

void SetRole(int id, PaneRole role, float a, float b) {
    const int i = IndexOfId(id);
    if (i < 0 || i >= (int)s_content.size() || s_panes[i].adopted) return;
    Content& c = s_content[i];
    const pane_content::Role r = role == PaneRole::Error ? pane_content::Role::Error
                               : role == PaneRole::Shutdown ? pane_content::Role::Shutdown
                               : role == PaneRole::Copy ? pane_content::Role::Copy
                               : role == PaneRole::Safe ? pane_content::Role::Safe
                               : pane_content::Role::None;
    // Repaint only for what the dialog shows: whole seconds, whole shots, the bar's pixels.
    const bool roleChanged = r != c.role;
    const bool shown = (int)std::ceil(a) != (int)std::ceil(c.roleA) || (int)(a * 100.0f) != (int)(c.roleA * 100.0f) ||
                       (int)b != (int)c.roleB;
    if (!roleChanged && !shown) return;
    c.role = r;
    c.roleA = a;
    c.roleB = b;
    c.lang = LangIndex();
    c.cacheDirty = true;
    if (roleChanged) ApplyTitle(i);
    InvalidateRect(s_handles[i], nullptr, FALSE);
}

void SetWear(int id, float left, bool fusing) {
    const int i = IndexOfId(id);
    if (i < 0 || i >= (int)s_content.size() || s_panes[i].adopted) return;
    Content& c = s_content[i];
    std::wstring status;
    if (fusing) {
        status = RefusalSuffix();
    } else if (left < kWornTitleShare) {
        const int li = LangIndex();
        const int pct = std::clamp((int)std::ceil(left * 100.0f), 0, 100);
        wchar_t buf[48];
        swprintf_s(buf, li == 0 ? L" (내구도 %d%%)" : (li == 2 ? L" (耐久 %d%%)" : L" (Integrity %d%%)"), pct);
        status = buf;
    }
    if (status == c.status) return;
    c.status = status;
    ApplyTitle(i);
}

bool GlitchBurstNow(int id, float& seed) {
    const int i = IndexOfId(id);
    if (i < 0 || i >= (int)s_content.size()) return false;
    const Content& c = s_content[i];
    if (c.unstable <= 0.0f || !pane_content::GlitchActive(c.glitch)) return false;
    seed = c.glitch.seed;
    return true;
}

void SetLocked(int id, bool locked) {
    const int i = IndexOfId(id);
    if (i >= 0) s_panes[i].locked = locked;
}

void RaiseToFront(int id) {
    const int i = IndexOfId(id);
    if (i < 0 || !s_panes[i].open || s_panes[i].adopted || !s_handles[i] || s_arrange) return;
    StackBelowGame(s_handles[i]);
}

void SetHold(bool hold) { s_hold = hold; }

// Another program's window that can hide what lies under it: visible, not
// minimized or cloaked, not the shell. Its frame in overlay pixels.
bool ForeignCover(HWND h, Rect& frame) {
    if (!IsWindowVisible(h) || IsIconic(h) || IsShellWindow(h)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId()) return false;
    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return false;
    frame = ToOverlay(FrameBounds(h));
    return frame.w > 1.0f && frame.h > 1.0f;
}

// The game's own panes back right under the game, keeping their order.
void RestackPanes() {
    std::vector<int> order;
    for (size_t i = 0; i < s_panes.size(); ++i)
        if (s_panes[i].open && !s_panes[i].adopted && s_handles[i]) order.push_back((int)i);
    std::sort(order.begin(), order.end(),
              [](int a, int b) { return s_panes[a].zRank > s_panes[b].zRank; });   // back first
    for (int i : order) StackBelowGame(s_handles[i]);
}

// True when another program's window sits between the game and one of its
// panes (left there by Alt-Tab, a click elsewhere, a new window...).
bool PaneStackBroken() {
    bool foreign = false;
    Rect frame;
    for (HWND h = GetWindow(s_overlayHwnd, GW_HWNDNEXT); h; h = GetWindow(h, GW_HWNDNEXT)) {
        const int i = IndexOfHandle(h);
        if (i >= 0 && !s_panes[i].adopted) {
            if (foreign) return true;
        } else if (ForeignCover(h, frame)) {
            foreign = true;
        }
    }
    return false;
}

// Marks panes mostly hidden by other programs' windows above them.
void UpdateCovered() {
    std::vector<Rect> front;
    Rect frame;
    for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT)) {
        const int i = IndexOfHandle(h);
        if (i >= 0) {
            Pane& p = s_panes[i];
            p.covered = s_coverCheck && p.open && !p.minimized &&
                        VisibleShare(p.frame, front) < kMinVisibleShare;
        } else if (h != s_overlayHwnd && ForeignCover(h, frame)) {
            front.push_back(frame);
        }
    }
}

void Tick(float dt) {
    // Forget closed panes whose afterimages are gone, so a blue screen's
    // dozens of panes do not pile up in every per-pane loop.
    for (size_t i = s_panes.size(); i-- > 0;) {
        if (s_panes[i].open || !s_panes[i].trail.empty()) continue;
        if (i < s_content.size() && s_content[i].cache) DeleteObject(s_content[i].cache);
        s_panes.erase(s_panes.begin() + (long long)i);
        s_handles.erase(s_handles.begin() + (long long)i);
        if (i < s_content.size()) s_content.erase(s_content.begin() + (long long)i);
    }
    // Front-to-back order among panes, for overlapping spaces.
    int rank = 0;
    for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT)) {
        const int i = IndexOfHandle(h);
        if (i >= 0) s_panes[i].zRank = rank++;
    }
    for (size_t i = 0; i < s_panes.size(); ++i) {
        Pane& p = s_panes[i];
        for (Ghost& g : p.trail) g.age += dt;
        p.trail.erase(std::remove_if(p.trail.begin(), p.trail.end(),
                                     [](const Ghost& g) { return g.age >= kGhostSeconds; }),
                      p.trail.end());
        if (!p.open) continue;
        if (p.adopted && !IsWindow(s_handles[i])) {   // the player closed it
            p.open = false;
            s_handles[i] = nullptr;
            continue;
        }
        if (!p.adopted && !s_arrange && AboveGame(s_handles[i])) StackBelowGame(s_handles[i]);
        // Decor panes stand still unless the player is arranging windows.
        if (p.decor && !s_arrange) continue;
        RefreshRects((int)i);
        UpdateShown(p, dt);
        if (p.refusalTimer > 0.0f) {
            p.refusalTimer -= dt;
            if (p.refusalTimer <= 0.0f) {
                p.refusalTimer = 0.0f;
                ApplyTitle((int)i);
            }
        }
    }
    s_scanTimer -= dt;
    if (s_scanTimer <= 0.0f) {
        s_scanTimer = kDesktopScanSeconds;
        ScanDesktop();
        if (!s_away && !s_arrange && s_overlayHwnd) {
            // Bring back panes something minimized (Win+D, the taskbar),
            // put them back under the game if another window slipped in,
            // and note any still hidden.
            for (size_t i = 0; i < s_panes.size(); ++i)
                if (s_panes[i].open && !s_panes[i].adopted && s_handles[i] && IsIconic(s_handles[i]))
                    ShowWindow(s_handles[i], SW_SHOWNOACTIVATE);
            if (PaneStackBroken()) RestackPanes();
            UpdateCovered();
        }
    }
}

void SetCoverCheck(bool on) { s_coverCheck = on; }

void SetAway(bool away) {
    if (away == s_away || !s_overlayHwnd) return;
    s_away = away;
    for (size_t i = 0; i < s_panes.size(); ++i) {
        if (!s_panes[i].open || s_panes[i].adopted || !s_handles[i]) continue;
        ShowWindow(s_handles[i], away ? SW_HIDE : SW_SHOWNOACTIVATE);
    }
    if (!away) {
        RestackPanes();
        UpdateCovered();
        s_scanTimer = kDesktopScanSeconds;
    }
}

void SetArrangeMode(bool on) {
    if (on == s_arrange || !s_overlayHwnd) return;
    s_arrange = on;
    const UINT keep = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (on) {
        // Sink the game as far back as it can go while staying in sight:
        // just above the front-most window that fills the screen, or else
        // just above the desktop. Real windows then sit on top, visible and
        // draggable, and the game (shots included) still shows around them.
        HWND floor = CoveringWindow();
        if (!floor) floor = DesktopShellWindow();
        HWND above = floor ? GetWindow(floor, GW_HWNDPREV) : nullptr;
        if (above == s_overlayHwnd) {
            s_sunk = true;   // already right there
        } else if (above) {
            SetWindowPos(s_overlayHwnd, above, 0, 0, 0, 0, keep);
            s_sunk = true;
        } else if (floor) {
            // The covering window is the front-most one: stay on top of it.
            s_sunk = false;
        } else {
            glfwSetWindowAttrib(s_overlay, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
            s_passthrough = true;
        }
    } else {
        if (s_sunk) RaiseOverlay();
        if (s_passthrough)
            glfwSetWindowAttrib(s_overlay, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
        s_sunk = s_passthrough = false;
    }
}

bool ArrangeMode() { return s_arrange; }

bool ArrangeTarget(Rect& frame) {
    if (!s_arrange || !s_overlayHwnd) return false;
    POINT pt{};
    GetCursorPos(&pt);
    HWND root = GetAncestor(WindowFromPoint(pt), GA_ROOT);
    if (!root || root == s_overlayHwnd || IsShellWindow(root)) return false;
    frame = ToOverlay(FrameBounds(root));
    return true;
}

bool AnyOpen() {
    for (const Pane& p : s_panes)
        if (p.open) return true;
    return false;
}
const std::vector<Pane>& Panes() { return s_panes; }
const std::vector<Rect>& DesktopWindows() { return s_desktop; }

bool OverlayIsForeground() {
    return s_overlayHwnd && GetForegroundWindow() == s_overlayHwnd;
}

void DebugRequestClose(int id) {
    const int i = IndexOfId(id);
    if (i >= 0) SendMessageW(s_handles[i], WM_SYSCOMMAND, SC_CLOSE, 0);
}

void DebugRequestMinimize(int id) {
    const int i = IndexOfId(id);
    if (i >= 0) SendMessageW(s_handles[i], WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void DebugRestore(int id) {
    const int i = IndexOfId(id);
    if (i >= 0) ShowWindow(s_handles[i], SW_SHOWNOACTIVATE);
}

void DebugMoveBy(int id, float dx, float dy) {
    const int i = IndexOfId(id);
    if (i < 0) return;
    RECT wr{};
    GetWindowRect(s_handles[i], &wr);
    SetWindowPos(s_handles[i], nullptr, wr.left + ToPx(dx), wr.top + ToPx(dy), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

unsigned long DebugProcessAt(float x, float y, bool* isOverlay) {
    const POINT o = OverlayOrigin();
    POINT pt{ o.x + (LONG)ToPx(x), o.y + (LONG)ToPx(y) };
    HWND root = GetAncestor(WindowFromPoint(pt), GA_ROOT);
    if (isOverlay) *isOverlay = root == s_overlayHwnd;
    DWORD pid = 0;
    if (root) GetWindowThreadProcessId(root, &pid);
    return pid;
}

unsigned long DebugOverlayExStyle() {
    return s_overlayHwnd ? (unsigned long)GetWindowLongW(s_overlayHwnd, GWL_EXSTYLE) : 0;
}

bool DebugActivateProbe() {
    HWND probe = FindWindowW(nullptr, L"PaneProbe");
    return probe && SetForegroundWindow(probe);
}

bool ArrangeKeyHeld() {
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool DebugCaptureScreen(const wchar_t* path) {
    if (!s_overlayHwnd) return false;
    RECT oc{};
    GetClientRect(s_overlayHwnd, &oc);
    const int w = oc.right - oc.left, h = oc.bottom - oc.top;
    if (w <= 0 || h <= 0) return false;
    const POINT o = OverlayOrigin();

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;              // bottom-up, as BMP files store rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (dib && bits) {
        HGDIOBJ old = SelectObject(mem, dib);
        ok = BitBlt(mem, 0, 0, w, h, screen, o.x, o.y, SRCCOPY | CAPTUREBLT) != FALSE;
        SelectObject(mem, old);
        if (ok) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
            BITMAPFILEHEADER fh{};
            const DWORD imageBytes = (DWORD)w * (DWORD)h * 4u;
            fh.bfType = 0x4D42;   // "BM"
            fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fh.bfSize = fh.bfOffBits + imageBytes;
            FILE* f = nullptr;
            ok = _wfopen_s(&f, path, L"wb") == 0 && f;
            if (ok) {
                std::fwrite(&fh, sizeof(fh), 1, f);
                std::fwrite(&bi.bmiHeader, sizeof(bi.bmiHeader), 1, f);
                std::fwrite(bits, 1, imageBytes, f);
                std::fclose(f);
            }
        }
    }
    if (dib) DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return ok;
}

} // namespace os_pane

#else

namespace os_pane {
namespace { std::vector<Pane> s_none; std::vector<Rect> s_noRects; }
void Init(GLFWwindow*) {}
void Shutdown() {}
int  Open(float, float, float, float, bool) { return 0; }
int  Adopt(int) { return 0; }
void Place(int, float, float) {}
void Resize(int, float, float, float, float) {}
void Close(int) {}
void CloseAll() {}
void SetLocked(int, bool) {}
void SetUnstable(int, float) {}
void SetRole(int, PaneRole, float, float) {}
void SetWear(int, float, bool) {}
bool GlitchBurstNow(int, float&) { return false; }
void SetHold(bool) {}
void Tick(float) {}
void SetAway(bool) {}
void SetCoverCheck(bool) {}
void SetArrangeMode(bool) {}
bool ArrangeMode() { return false; }
bool ArrangeTarget(Rect&) { return false; }
bool AnyOpen() { return false; }
const std::vector<Pane>& Panes() { return s_none; }
const std::vector<Rect>& DesktopWindows() { return s_noRects; }
bool OverlayIsForeground() { return true; }
void DebugRequestClose(int) {}
void DebugRequestMinimize(int) {}
void DebugRestore(int) {}
void DebugMoveBy(int, float, float) {}
bool ArrangeKeyHeld() { return false; }
bool DebugCaptureScreen(const wchar_t*) { return false; }
unsigned long DebugProcessAt(float, float, bool* isOverlay) { if (isOverlay) *isOverlay = true; return 0; }
unsigned long DebugOverlayExStyle() { return 0; }
bool DebugActivateProbe() { return false; }
} // namespace os_pane

#endif
