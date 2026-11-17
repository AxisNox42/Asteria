#pragma once
// ─────────────────────────────────────────────────────────────
// 스모크 실행 창을 다른 가상 데스크톱으로 보낸다 (Windows 전용).
//   스모크 테스트가 작업 중인 화면을 덮지 않도록, 기본으로 "데스크톱 2" 에 띄운다.
//   ASTERIA_SMOKE_DESKTOP = <n>  : n번째 가상 데스크톱 (기본 2)
//                         = 0    : 옮기지 않음 (예전처럼 현재 화면에 띄움)
//   지정한 데스크톱이 지금 보고 있는 데스크톱이면, 보고 있지 않은 다른 데스크톱을 쓴다.
//   데스크톱이 하나뿐이면 아무것도 하지 않는다.
// 메인 창은 숨긴 채 만들어 옮긴 뒤 보여 주고 (SmokeDesktopPlaceMain),
// 이후 뜨는 보조 창(가장자리 가드, OS 창 등)은 나타나는 즉시 같은 데스크톱으로 옮긴다.
// 일반 실행에는 전혀 관여하지 않는다.
// 다른 데스크톱의 창이 포그라운드가 되면 Windows 가 사용자 화면을 그 데스크톱으로 넘긴다.
//   그래서 Active() 동안에는 SetForegroundWindow / SetFocus 를 부르지 않는다 (MayActivate()).
//   새 창은 보이기 전에 PlaceBeforeShow() 로 cloak 을 걸고, 표시·이동 뒤 Settle() 이 푼다
//   (현재 화면에 한 프레임도 비치지 않게).
// 여러 .cpp 에서 include 해도 되도록 다른 게임 헤더에 의존하지 않는다.
// ─────────────────────────────────────────────────────────────
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <shobjidl_core.h>
#include <dwmapi.h>
#include <cstdlib>
#include <string>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace smoke_desktop {

inline IVirtualDesktopManager*& Manager() { static IVirtualDesktopManager* m = nullptr; return m; }
inline GUID& Target() { static GUID g{}; return g; }
inline bool& Active() { static bool a = false; return a; }

inline bool ReadDesktops(std::vector<GUID>& ids, GUID& current) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops",
            0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    DWORD size = 0;
    bool ok = RegQueryValueExW(key, L"VirtualDesktopIDs", nullptr, nullptr, nullptr, &size) == ERROR_SUCCESS
              && size >= sizeof(GUID);
    if (ok) {
        ids.resize(size / sizeof(GUID));
        ok = RegQueryValueExW(key, L"VirtualDesktopIDs", nullptr, nullptr,
                              reinterpret_cast<BYTE*>(ids.data()), &size) == ERROR_SUCCESS;
    }
    current = GUID{};
    DWORD csize = sizeof(GUID);
    RegQueryValueExW(key, L"CurrentVirtualDesktop", nullptr, nullptr,
                     reinterpret_cast<BYTE*>(&current), &csize);
    RegCloseKey(key);
    return ok;
}

inline bool OnTarget(HWND hwnd) {
    GUID on{};
    return SUCCEEDED(Manager()->GetWindowDesktopId(hwnd, &on)) && IsEqualGUID(on, Target());
}

// true: 이제 대상 데스크톱에 있다.
inline bool MoveIfNeeded(HWND hwnd) {
    if (!Active() || !hwnd || GetAncestor(hwnd, GA_ROOT) != hwnd) return false;
    if (OnTarget(hwnd)) return true;
    Manager()->MoveWindowToDesktop(hwnd, Target());
    return OnTarget(hwnd);
}

// 스모크 데스크톱 모드에서는 false: 포커스를 가져가면 사용자 화면이 넘어간다.
inline bool MayActivate() { return !Active(); }

// Windows 는 한 번도 표시되지 않은 창을 다른 데스크톱으로 옮겨 주지 않는다. 그래서 새 창은
// 보이기 전에 DWM cloak(화면에 안 그려짐)을 걸어 두고, 표시된 뒤 옮겨지면 Settle() 이 푼다.
inline std::vector<HWND>& Cloaked() { static std::vector<HWND> v; return v; }

inline void PlaceBeforeShow(HWND hwnd) {
    if (!Active() || !hwnd || MoveIfNeeded(hwnd)) return;
    BOOL on = TRUE;
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &on, sizeof(on))))
        Cloaked().push_back(hwnd);
}

// 표시된 창을 옮기고, 옮겨졌으면 cloak 을 푼다 (다른 데스크톱이라 사용자 화면에는 안 보인다).
inline void Settle(HWND hwnd) {
    if (!Active() || !hwnd) return;
    const bool moved = MoveIfNeeded(hwnd);
    auto& v = Cloaked();
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != hwnd) continue;
        if (!moved && IsWindowVisible(hwnd)) return;   // 아직: 다음 표시 이벤트에서 다시
        BOOL off = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &off, sizeof(off));
        v.erase(v.begin() + (ptrdiff_t)i);
        return;
    }
}

inline void CALLBACK OnShow(HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD) {
    if (idObject == OBJID_WINDOW) Settle(hwnd);
}

// 스모크 실행일 때 한 번 호출. 옮길 데스크톱이 정해지면 true.
inline bool Begin() {
    char env[16] = {};
    const DWORD n = GetEnvironmentVariableA("ASTERIA_SMOKE_DESKTOP", env, sizeof(env));
    const int want = (n > 0 && n < sizeof(env)) ? std::atoi(env) : 2;
    if (want <= 0) return false;
    std::vector<GUID> ids;
    GUID current{};
    if (!ReadDesktops(ids, current) || ids.size() < 2) return false;
    size_t pick = (size_t)want - 1 < ids.size() ? (size_t)want - 1 : ids.size() - 1;
    if (IsEqualGUID(ids[pick], current)) {
        for (size_t i = 0; i < ids.size(); ++i)
            if (!IsEqualGUID(ids[i], current)) { pick = i; break; }
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // RPC_E_CHANGED_MODE 여도 사용 가능
    IVirtualDesktopManager* mgr = nullptr;
    // CLSID_VirtualDesktopManager (shobjidl_core.h 에는 선언만 있어 직접 적는다)
    static const CLSID kClsid = {0xAA509086, 0x5CA9, 0x4C25, {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};
    if (FAILED(CoCreateInstance(kClsid, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&mgr))) || !mgr) return false;
    Manager() = mgr;
    Target() = ids[pick];
    Active() = true;
    SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, OnShow,
                    GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
    return true;
}

} // namespace smoke_desktop
#endif
