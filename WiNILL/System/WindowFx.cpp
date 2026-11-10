#include "WindowFx.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cwchar>
#include <vector>

// --- 디버그 로그 — 비활성화(no-op). 과거 transparency_debug.txt 를 만들던 기능 제거.
//     (F26: 배포본에 디버그 파일이 생기지 않도록. 호출부는 그대로 두고 함수만 비움.)
void TransparencyLog(const char* /*fmt*/, ...) {
    // intentionally empty
}

#ifdef _WIN32
// ═══════════════════════ Windows: DWM 합성 ═════════════════════════════════════
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "Settings.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {
// Shell fullscreen classification is independent of the swapchain dimensions.
// This preserves the 1px DWM-composition workaround without forcing TOPMOST
// or changing/hiding Explorer's taskbar windows globally.
bool MarkShellFullscreen(HWND hwnd, bool fullscreen) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    ITaskbarList2* taskbar = nullptr;
    HRESULT result = CoCreateInstance(CLSID_TaskbarList, nullptr,
                                     CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&taskbar));
    if (SUCCEEDED(result)) {
        result = taskbar->HrInit();
        if (SUCCEEDED(result))
            result = taskbar->MarkFullscreenWindow(hwnd, fullscreen ? TRUE : FALSE);
        taskbar->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return SUCCEEDED(result);
}
}

static LRESULT CALLBACK EdgeGuardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;   // a click never takes focus
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK:
        return 0;                                    // and does nothing
    case WM_ERASEBKGND: return 1;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// The game window stops short of the monitor's bottom edge (see main.cpp) so
// live wallpapers do not take it for a full-screen app. This covers the rows
// it leaves with a window that cannot be seen (alpha 1/255), never takes
// focus and swallows clicks, so slamming the mouse to the bottom edge and
// clicking does not hit the desktop. It is owned by the game window: it stays
// just above it in the z-order and hides with it.
static void CoverEdgeGap(HWND game) {
    RECT wr{};
    if (!GetWindowRect(game, &wr)) return;
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(MonitorFromWindow(game, MONITOR_DEFAULTTOPRIMARY), &mi)) return;
    const RECT& m = mi.rcMonitor;
    const int gap = m.bottom - wr.bottom;
    // Only the deliberate few-row gap; smoke runs use small windows.
    if (gap <= 0 || gap > 4 || wr.left > m.left || wr.right < m.right) return;

    static const wchar_t* kClass = L"AsteriaEdgeGuard";
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = EdgeGuardProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = kClass;
        registered = RegisterClassW(&wc) != 0;
        if (!registered) return;
    }
    HWND guard = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 kClass, L"", WS_POPUP,
                                 m.left, wr.bottom, m.right - m.left, gap,
                                 game, nullptr, inst, nullptr);
    if (!guard) return;
    SetLayeredWindowAttributes(guard, 0, 1, LWA_ALPHA);   // alpha 0 would let clicks through
    ShowWindow(guard, SW_SHOWNOACTIVATE);
}

void UpdateWindowTaskbarPolicy(GLFWwindow* window) {
    if (!window) return;
    HWND hwnd = glfwGetWin32Window(window);
    const bool active = GetForegroundWindow() == hwnd && !IsIconic(hwnd);
    // Reapply on focus changes and Explorer restart; retry failures without
    // issuing a COM request every rendered frame.
    static HWND lastWindow = nullptr, lastShell = nullptr;
    static bool lastActive = false;
    static bool applied = false;
    static double nextRetry = 0.0;
    HWND shell = FindWindowW(L"Shell_TrayWnd", nullptr);
    const double now = glfwGetTime();
    if (hwnd == lastWindow && shell == lastShell && active == lastActive &&
        (applied || now < nextRetry)) return;
    applied = MarkShellFullscreen(hwnd, active);
    lastWindow = hwnd;
    lastShell = shell;
    lastActive = active;
    nextRetry = now + 2.0;
}

void ReleaseWindowTaskbarPolicy(GLFWwindow* window) {
    if (window) MarkShellFullscreen(glfwGetWin32Window(window), false);
}

// Is the display under the window running with HDR (advanced color) on?
// Matches the window's monitor to a display-config source by GDI device name.
bool IsWindowOnHdrDisplay(GLFWwindow* window) {
    {   // A/B captures and tests
        char* f = nullptr;
        size_t n = 0;
        if (_dupenv_s(&f, &n, "ASTERIA_FORCE_HDR") == 0 && f) {
            const bool on = f[0] == '1';
            free(f);
            return on;
        }
    }
    if (!window) return false;
    HWND hwnd = glfwGetWin32Window(window);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
        return false;

    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
        return false;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(),
                           &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
        return false;
    for (UINT32 i = 0; i < pathCount; ++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME src{};
        src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        src.header.size = sizeof(src);
        src.header.adapterId = paths[i].sourceInfo.adapterId;
        src.header.id = paths[i].sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS) continue;
        if (wcscmp(src.viewGdiDeviceName, mi.szDevice) != 0) continue;

        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO col{};
        col.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        col.header.size = sizeof(col);
        col.header.adapterId = paths[i].targetInfo.adapterId;
        col.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&col.header) != ERROR_SUCCESS) return false;
        return col.advancedColorEnabled != 0;
    }
    return false;
}

void EnableWindowTransparency(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    TransparencyLog("=== EnableWindowTransparency ===\n");

    BOOL dwmOn = FALSE;
    DwmIsCompositionEnabled(&dwmOn);
    TransparencyLog("  DwmIsCompositionEnabled = %s\n", dwmOn ? "YES" : "NO");
    // GLFW_TRANSPARENT_FRAMEBUFFER already handles per-pixel alpha.
    // Extending DWM glass over the whole client area darkens the desktop behind UI.

    if (dwmOn) {
        // 클라이언트 영역 전체를 유리 프레임으로 → per-pixel 알파가 데스크탑에 합성됨
        MARGINS m = { 0, 0, 0, 0 };
        HRESULT hrM = DwmExtendFrameIntoClientArea(hwnd, &m);
        TransparencyLog("  DwmExtendFrameIntoClientArea → 0x%08lX (%s)\n",
            (unsigned long)hrM, SUCCEEDED(hrM) ? "OK" : "FAILED");
    }

    // Apps behind the game (Chrome, Edge, Discord and other Chromium/Electron
    // apps) pause video and stop painting when a visible, fully opaque window
    // covers them: their occlusion check only sees Win32 styles, not our DWM
    // per-pixel alpha, so the full-screen overlay read as an opaque cover and
    // froze everything behind it. A layered window with whole-window alpha
    // below 255 is skipped by that check. 254/255 is not visible on screen.
    glfwSetWindowOpacity(window, kSeeThroughOpacity);
    CoverEdgeGap(hwnd);

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    TransparencyLog("  GWL_EXSTYLE   = 0x%08lX\n", ex);
    TransparencyLog("  WS_EX_LAYERED = %s\n", (ex & WS_EX_LAYERED) ? "SET" : "NOT SET");
    TransparencyLog("================================\n\n");
}

#else
void UpdateWindowTaskbarPolicy(GLFWwindow*) {}
void ReleaseWindowTaskbarPolicy(GLFWwindow*) {}
bool IsWindowOnHdrDisplay(GLFWwindow*) { return false; }
// ═══════════════════════ macOS / Linux ═════════════════════════════════════════
//   투명도는 glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE) 가 처리.
//   (Cocoa: NSWindow opaque=NO / X11: 32bit visual). 추가 OS 호출 불필요.
void EnableWindowTransparency(GLFWwindow* /*window*/) {
    TransparencyLog("=== EnableWindowTransparency (non-Windows: GLFW hint) ===\n");
}
#endif
