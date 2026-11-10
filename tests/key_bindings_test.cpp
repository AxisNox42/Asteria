// Key bindings: swap-on-conflict assignment, reserved keys, load validation,
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
    // Free key: simply assigned.
    assert(AssignKeyBinding(s, GameAction::Skill1, 'F') == BindResult::Assigned);
    assert(s.key[(int)GameAction::Skill1] == 'F');
    // Same key again: nothing changes.
    assert(AssignKeyBinding(s, GameAction::Skill1, 'F') == BindResult::Unchanged);
    // Key used by another action: the two swap, nothing is left unbound.
    assert(AssignKeyBinding(s, GameAction::MoveUp, kBindKeyS) == BindResult::Swapped);
    assert(s.key[(int)GameAction::MoveUp] == kBindKeyS);
    assert(s.key[(int)GameAction::MoveDown] == kBindKeyW);
    // Right Shift is the same key as Left Shift.
    assert(AssignKeyBinding(s, GameAction::Skill2, kBindKeyRightShift) == BindResult::Swapped);
    assert(s.key[(int)GameAction::Skill2] == kBindKeyLeftShift);
    assert(s.key[(int)GameAction::Dash] == kBindKeyE);
    // Reserved and unknown keys are refused and leave the set untouched.
    const KeyBindSet before = s;
    assert(AssignKeyBinding(s, GameAction::Dash, kBindKeyEscape) == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Dash, kBindKeyF8) == BindResult::Rejected);
    // Keys the game reads directly: SPACE (pause / confirm), 1-5 (card picks), F12.
    assert(AssignKeyBinding(s, GameAction::Dash, kBindKeySpace) == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Skill1, '1') == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Skill1, '5') == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Skill1, kBindKeyF12) == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Skill1, '6') != BindResult::Rejected);
    s = before;
    assert(AssignKeyBinding(s, GameAction::Dash, -1) == BindResult::Rejected);
    assert(AssignKeyBinding(s, GameAction::Dash, 9999) == BindResult::Rejected);
    assert(s == before);
    // No two actions ever share a key.
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        for (int j = i + 1; j < GAME_ACTION_COUNT; ++j)
            assert(s.key[i] != s.key[j]);
}

static void CheckSanitize() {
    KeyBindSet s = kDefaultKeyBinds;
    s.key[(int)GameAction::Skill3] = kBindKeyEscape;       // reserved -> that action's default
    SanitizeKeyBindings(s);
    assert(s == kDefaultKeyBinds);
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Skill1] = 'Z';
    SanitizeKeyBindings(s);                             // valid change survives
    assert(s.key[(int)GameAction::Skill1] == 'Z');
    s.key[(int)GameAction::Skill2] = 'Z';               // duplicate -> everything resets
    SanitizeKeyBindings(s);
    assert(s == kDefaultKeyBinds);
    s = kDefaultKeyBinds;
    s.key[(int)GameAction::Dash] = kBindKeyRightShift;      // normalised to Left Shift
    SanitizeKeyBindings(s);
    assert(s.key[(int)GameAction::Dash] == kBindKeyLeftShift);
}

static void CheckNames() {
    assert(KeyName('W') == L"W");
    assert(KeyName(kBindKeyRightShift) == L"SHIFT");
    assert(KeyName(32) == L"SPACE");
    assert(KeyName(290) == L"F1");
    assert(KeyName(265) == L"UP");
    // Group labels used by the tutorial texts.
    ResetKeyBindings();
    assert(MoveKeysLabel() == L"WASD");
    assert(SkillKeysLabel() == L"Q/E/R");
    AssignKeyBinding(g_KeyBinds, GameAction::MoveUp, 265);
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
    AssignKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
    g_Language = Language::EN;
    assert(std::wstring(TutorialControls()).find(L"F dash") != std::wstring::npos);
    ResetKeyBindings();
    g_Language = Language::KR;
}

static void CheckSaveRoundTrip() {
    g_SmokeCapture = false;
    ResetKeyBindings();
    AssignKeyBinding(g_KeyBinds, GameAction::Skill1, 'Z');
    AssignKeyBinding(g_KeyBinds, GameAction::Dash, 'F');
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
    AssignKeyBinding(g_KeyBinds, GameAction::Skill1, 'Z');
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
