// Key bindings: conflicts leave the other action unbound, required keys, load validation,
// save -> reload, and the settings snapshot. Run with
// scripts/test_key_bindings.ps1 from an empty temporary folder (the save path
// is relative, so the real save is never near).
#include "SaveSystem.h"
#include "SettingsSnapshot.h"
#include "TutorialText.h"
#include <cwchar>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

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

static void CheckDefaults() {
    ResetKeyBindings();
    assert(ActionKey(GameAction::MoveUp) == kBindKeyW);
    assert(ActionKey(GameAction::MoveDown) == kBindKeyS);
    assert(ActionKey(GameAction::MoveLeft) == kBindKeyA);
    assert(ActionKey(GameAction::MoveRight) == kBindKeyD);
    assert(ActionKey(GameAction::Dash) == kBindKeyLeftShift);
    assert(ActionKey(GameAction::Skill1) == kBindKeyQ);
    assert(ActionKey(GameAction::Skill2) == kBindKeyE);
    assert(ActionKey(GameAction::Skill3) == kBindKeyR);
}

static void CheckAssign() {
    KeyBindSet s = kDefaultKeyBinds;
    int out[GAME_ACTION_COUNT];
    // Free key: simply assigned, nothing else changes.
    assert(KeyBindingConflicts(s, GameAction::Skill1, 'F', out) == 0);
    ApplyKeyBinding(s, GameAction::Skill1, 'F');
    assert(s.key[(int)GameAction::Skill1] == 'F');
    // Key used by another action: that action is named, then left unbound.
    assert(KeyBindingConflicts(s, GameAction::MoveUp, kBindKeyS, out) == 1);
    assert(out[0] == (int)GameAction::MoveDown);
    ApplyKeyBinding(s, GameAction::MoveUp, kBindKeyS);
    assert(s.key[(int)GameAction::MoveUp] == kBindKeyS);
    assert(s.key[(int)GameAction::MoveDown] == kKeyUnbound);
    // Right Shift is the same key as Left Shift.
    assert(KeyBindingConflicts(s, GameAction::Skill2, kBindKeyRightShift, out) == 1);
    assert(out[0] == (int)GameAction::Dash);
    // SPACE is no longer fixed: binding it to Dash unbinds Confirm and Pause.
    s = kDefaultKeyBinds;
    assert(KeyBindable(kBindKeySpace) && KeyBindable('1') && KeyBindable(301));
    assert(KeyBindingConflicts(s, GameAction::Dash, kBindKeySpace, out) == 2);
    ApplyKeyBinding(s, GameAction::Dash, kBindKeySpace);
    assert(s.key[(int)GameAction::Confirm] == kKeyUnbound);
    assert(s.key[(int)GameAction::Pause] == kKeyUnbound);
    // Confirm is required, Pause optional: one missing required key.
    int missing[GAME_ACTION_COUNT];
    assert(MissingRequiredKeys(s, missing) == 1 && missing[0] == (int)GameAction::Confirm);
    // Confirm and Pause may share a key.
    assert(KeyBindingConflicts(s, GameAction::Confirm, 'P', out) == 0);
    ApplyKeyBinding(s, GameAction::Confirm, 'P');
    assert(KeyBindingConflicts(s, GameAction::Pause, 'P', out) == 0);
    ApplyKeyBinding(s, GameAction::Pause, 'P');
    assert(MissingRequiredKeys(s) == 0);
    // Card keys: '1' moves from Card1 to Skill1, Card1 is optional.
    ApplyKeyBinding(s, GameAction::Skill1, '1');
    assert(s.key[(int)GameAction::Card1] == kKeyUnbound && MissingRequiredKeys(s) == 0);
    // Only ESC and unknown keys are refused.
    assert(!KeyBindable(kBindKeyEscape));
    assert(!KeyBindable(-1) && !KeyBindable(9999));
    // Defaults: every required action bound, only Confirm/Pause share.
    assert(MissingRequiredKeys(kDefaultKeyBinds) == 0);
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        for (int j = i + 1; j < GAME_ACTION_COUNT; ++j)
            assert(kDefaultKeyBinds.key[i] != kDefaultKeyBinds.key[j] || ActionsMayShareKey(i, j));
    assert(!ActionRequired(GameAction::Pause) && !ActionRequired(GameAction::Card3));
    assert(ActionRequired(GameAction::Confirm) && ActionRequired(GameAction::Dash));
}

static void CheckSanitize() {
    KeyBindSet s = kDefaultKeyBinds;
    s.key[(int)GameAction::Skill3] = kBindKeyEscape;       // unbindable -> that action's default
    SanitizeKeyBindings(s);
    assert(s == kDefaultKeyBinds);
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Skill1] = 'Z';
    SanitizeKeyBindings(s);                             // valid change survives
    assert(s.key[(int)GameAction::Skill1] == 'Z');
    s.key[(int)GameAction::Skill2] = 'Z';               // required duplicate -> everything resets
    SanitizeKeyBindings(s);
    assert(s == kDefaultKeyBinds);
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Dash] = kBindKeyRightShift;      // normalised to Left Shift
    SanitizeKeyBindings(s);
    assert(s.key[(int)GameAction::Dash] == kBindKeyLeftShift);
    // Optional unbound stays unbound; required unbound gets its default.
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Pause] = kKeyUnbound;
    s.key[(int)GameAction::Confirm] = kKeyUnbound;
    SanitizeKeyBindings(s);
    assert(s.key[(int)GameAction::Pause] == kKeyUnbound);
    assert(s.key[(int)GameAction::Confirm] == kBindKeySpace);
    // An old save with Skill1 on '1' (before card keys existed): Card1 yields.
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Skill1] = '1';
    SanitizeKeyBindings(s);
    assert(s.key[(int)GameAction::Skill1] == '1');
    assert(s.key[(int)GameAction::Card1] == kKeyUnbound);
}

static void CheckNames() {
    assert(KeyName('W') == L"W");
    assert(KeyName(kBindKeyRightShift) == L"SHIFT");
    assert(KeyName(32) == L"SPACE");
    assert(KeyName(kKeyUnbound) != L"KEY-1");
    assert(KeyName(290) == L"F1");
    assert(KeyName(265) == L"UP");
    // Group labels used by the tutorial texts.
    ResetKeyBindings();
    assert(MoveKeysLabel() == L"WASD");
    assert(SkillKeysLabel() == L"Q/E/R");
    ApplyKeyBinding(g_KeyBinds, GameAction::MoveUp, 265);
    assert(MoveKeysLabel() == L"UP/A/S/D");
    ResetKeyBindings();
}

static void CheckTutorialTokens() {
    ResetKeyBindings();
    for (int lang = 0; lang < LANG_COUNT; ++lang) {
        g_Language = (Language)lang;
        std::wstring all = ReadyBrief();
        all += TutorialControls();
        for (int page = 0; page < TUTORIAL_PAGE_COUNT; ++page) all += TutorialPageBody(page);
        assert(all.find(L'{') == std::wstring::npos);   // every token expanded
        assert(all.find(L"WASD") != std::wstring::npos);
        assert(all.find(L"SHIFT") != std::wstring::npos);
    }
    ApplyKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
    g_Language = Language::EN;
    assert(std::wstring(TutorialControls()).find(L"F dash") != std::wstring::npos);
    ResetKeyBindings();
    g_Language = Language::KR;
}

static void CheckSaveRoundTrip() {
    g_SmokeCapture = false;
    ResetKeyBindings();
    ApplyKeyBinding(g_KeyBinds, GameAction::Skill1, 'Z');
    ApplyKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
    g_KeyBinds.key[(int)GameAction::Pause] = kKeyUnbound;   // optional: stays unbound
    const KeyBindSet wanted = g_KeyBinds;
    SaveGame();
    ResetKeyBindings();
    LoadGame();
    assert(g_KeyBinds == wanted);
}

static void CheckSnapshot() {
    ResetKeyBindings();
    const SettingsSnapshot snap = CaptureSettingsSnapshot();
    assert(!SettingsSnapshotChanged(snap));
    ApplyKeyBinding(g_KeyBinds, GameAction::Skill1, 'Z');
    assert(SettingsSnapshotChanged(snap));
    RestoreSettingsSnapshot(snap);
    assert(g_KeyBinds == kDefaultKeyBinds && !SettingsSnapshotChanged(snap));
}

int main() {
    // Keep the test off the real AppData save (SaveGame writes every option).
    _putenv_s("ASTERIA_SAVE_DIR", ".");
    CheckDefaults();
    CheckAssign();
    CheckSanitize();
    CheckNames();
    CheckTutorialTokens();
    CheckSaveRoundTrip();
    CheckSnapshot();
    std::puts("key bindings: all checks passed");
    return 0;
}
