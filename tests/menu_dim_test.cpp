// Menu darkness setting: 0..100 -> 0..4 factor, clamped load, save -> reload,
// and the settings snapshot. Run with scripts/test_menu_dim.ps1 from an empty
// temporary folder (the save path is relative).
#include "SaveSystem.h"
#include "SettingsSnapshot.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Audio {
void SetBgmEnabled(bool) {}
void SetSfxEnabled(bool) {}
void SetEnabled(bool) {}
void SetVolume(float) {}
void SetBgmVolume(float) {}
void SetSfxVolume(float) {}
bool SetMonoOutput(bool mono) { return mono; }
bool IsMonoOutput() { return false; }
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

static void CheckFactor() {
    assert(Near(MenuDimFactor(0), 0.0f));
    assert(Near(MenuDimFactor(50), 1.0f));                // 50 = the original look
    assert(kMenuDimDefault == 80);
    assert(Near(MenuDimFactor(100), kMenuDimMax));
    assert(Near(MenuDimFactor(75), 2.5f));
    assert(Near(MenuDimFactor(-20), 0.0f));                // clamped
    assert(Near(MenuDimFactor(900), kMenuDimMax));
}

static void CheckCombatShare() {
    assert(Near(CombatDimAlpha(0), 0.0f));                          // 0 = no extra dark anywhere
    assert(Near(MenuDimEquivAlpha(50), 0.16f));                     // default: the navy base only
    assert(Near(CombatDimAlpha(50), 0.2f * 0.16f));                 // combat = 20% of it
    assert(CombatDimAlpha(100) > CombatDimAlpha(75) && CombatDimAlpha(75) > CombatDimAlpha(50));
    assert(CombatDimAlpha(100) < 0.2f);                             // never more than 20% black
}

static void CheckSaveRoundTrip() {
    g_SmokeCapture = false;
    g_MenuDim = 80;
    SaveGame();
    g_MenuDim = kMenuDimDefault;
    LoadGame();
    assert(g_MenuDim == 80);
}

static void CheckSnapshot() {
    g_MenuDim = 50;
    const SettingsSnapshot snap = CaptureSettingsSnapshot();
    assert(!SettingsSnapshotChanged(snap));
    g_MenuDim = 10;
    assert(SettingsSnapshotChanged(snap));
    RestoreSettingsSnapshot(snap);
    assert(g_MenuDim == 50 && !SettingsSnapshotChanged(snap));
}

int main() {
    _putenv_s("ASTERIA_SAVE_DIR", ".");
    CheckFactor();
    CheckCombatShare();
    CheckSaveRoundTrip();
    CheckSnapshot();
    std::puts("menu dim: all checks passed");
    return 0;
}
