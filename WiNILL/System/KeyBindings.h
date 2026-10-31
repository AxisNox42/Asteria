#pragma once

#include <string>

// ── 게임플레이 키 바인딩 (데이터 + 규칙, 입력 상태에는 의존하지 않음) ──
//   게임 로직은 GLFW 키 코드를 직접 읽지 않고 GameAction 으로만 입력을 묻는다
//   (Input.h 의 ActionHeld). 메뉴 탐색/ESC/증강 선택/디버그 키는 바인딩 대상이 아님.

enum class GameAction {
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Dash,
    Skill1, Skill2, Skill3,
    Count
};
inline constexpr int GAME_ACTION_COUNT = (int)GameAction::Count;

// GLFW_KEY_* 와 같은 값 (GLFW 헤더 없이 쓰기 위해 값만 둔다).
inline constexpr int kBindKeySpace = 32;    // READY 시작 / 일시정지 (바인딩 대상 아님, GameManager)
inline constexpr int kBindKeyEscape = 256;
inline constexpr int kBindKeyA = 65, kBindKeyD = 68, kBindKeyE = 69, kBindKeyQ = 81;
inline constexpr int kBindKeyR = 82, kBindKeyS = 83, kBindKeyW = 87;
inline constexpr int kBindKeyF8 = 297, kBindKeyF9 = 298;
inline constexpr int kBindKeyLeftShift = 340, kBindKeyLeftCtrl = 341, kBindKeyLeftAlt = 342;
inline constexpr int kBindKeyRightShift = 344, kBindKeyRightCtrl = 345, kBindKeyRightAlt = 346;
inline constexpr int kBindKeyLast = 348;   // GLFW_KEY_MENU

struct KeyBindSet {
    int key[GAME_ACTION_COUNT];
    bool operator==(const KeyBindSet& o) const {
        for (int i = 0; i < GAME_ACTION_COUNT; ++i)
            if (key[i] != o.key[i]) return false;
        return true;
    }
    bool operator!=(const KeyBindSet& o) const { return !(*this == o); }
};

inline constexpr KeyBindSet kDefaultKeyBinds = {{
    kBindKeyW, kBindKeyS, kBindKeyA, kBindKeyD,
    kBindKeyLeftShift,
    kBindKeyQ, kBindKeyE, kBindKeyR,
}};

inline KeyBindSet g_KeyBinds = kDefaultKeyBinds;

inline int ActionKey(GameAction a) { return g_KeyBinds.key[(int)a]; }

inline void ResetKeyBindings() { g_KeyBinds = kDefaultKeyBinds; }

// 좌우 구분이 없는 키는 왼쪽 값으로 통일한다 (오른쪽 Shift == 왼쪽 Shift).
inline int NormalizeBindKey(int key) {
    switch (key) {
    case kBindKeyRightShift: return kBindKeyLeftShift;
    case kBindKeyRightCtrl:  return kBindKeyLeftCtrl;
    case kBindKeyRightAlt:   return kBindKeyLeftAlt;
    default:             return key;
    }
}

// 바인딩할 수 있는 키: 알려진 GLFW 키 범위 (스페이스 이상), 예약 키 제외.
//   예약: ESC (취소/일시정지), F8/F9 (디버그).
inline bool KeyBindable(int key) {
    key = NormalizeBindKey(key);
    if (key < 32 || key > kBindKeyLast) return false;
    return key != kBindKeyEscape && key != kBindKeyF8 && key != kBindKeyF9;
}

enum class BindResult { Rejected, Unchanged, Assigned, Swapped };

// action 에 key 를 바인딩한다. 이미 다른 액션이 쓰는 키면 두 액션의 키를 맞바꾼다.
inline BindResult AssignKeyBinding(KeyBindSet& set, GameAction action, int key) {
    if (!KeyBindable(key)) return BindResult::Rejected;
    key = NormalizeBindKey(key);
    const int self = (int)action;
    if (set.key[self] == key) return BindResult::Unchanged;
    for (int i = 0; i < GAME_ACTION_COUNT; ++i) {
        if (i != self && set.key[i] == key) {
            set.key[i] = set.key[self];
            set.key[self] = key;
            return BindResult::Swapped;
        }
    }
    set.key[self] = key;
    return BindResult::Assigned;
}

// 불러온 값 검증: 잘못된 키는 해당 액션만 기본값으로, 중복이 있으면 전체를 기본값으로.
inline void SanitizeKeyBindings(KeyBindSet& set) {
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        set.key[i] = KeyBindable(set.key[i]) ? NormalizeBindKey(set.key[i])
                                             : kDefaultKeyBinds.key[i];
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        for (int j = i + 1; j < GAME_ACTION_COUNT; ++j)
            if (set.key[i] == set.key[j]) { set = kDefaultKeyBinds; return; }
}

// 표시용 키 이름 (HUD/설정 화면 공용).
inline std::wstring KeyName(int key) {
    key = NormalizeBindKey(key);
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::wstring(1, (wchar_t)key);
    if (key >= 290 && key <= 301)   // F1..F12
        return L"F" + std::to_wstring(key - 289);
    if (key >= 320 && key <= 329)   // keypad 0..9
        return L"NUM" + std::to_wstring(key - 320);
    switch (key) {
    case 330: return L"NUM.";
    case 331: return L"NUM/";
    case 332: return L"NUM*";
    case 333: return L"NUM-";
    case 334: return L"NUM+";
    case 335: return L"NUMENTER";
    case 336: return L"NUM=";
    case 32:  return L"SPACE";
    case 39:  return L"'";
    case 44:  return L",";
    case 45:  return L"-";
    case 46:  return L".";
    case 47:  return L"/";
    case 59:  return L";";
    case 61:  return L"=";
    case 91:  return L"[";
    case 92:  return L"\\";
    case 93:  return L"]";
    case 96:  return L"`";
    case 256: return L"ESC";
    case 257: return L"ENTER";
    case 258: return L"TAB";
    case 259: return L"BACKSPACE";
    case 260: return L"INSERT";
    case 261: return L"DELETE";
    case 262: return L"RIGHT";
    case 263: return L"LEFT";
    case 264: return L"DOWN";
    case 265: return L"UP";
    case 266: return L"PGUP";
    case 267: return L"PGDN";
    case 268: return L"HOME";
    case 269: return L"END";
    case 280: return L"CAPS";
    case 281: return L"SCRLK";
    case 282: return L"NUMLOCK";
    case 283: return L"PRTSC";
    case 284: return L"PAUSE";
    case 340: return L"SHIFT";
    case 341: return L"CTRL";
    case 342: return L"ALT";
    case 343: return L"WIN";
    case 347: return L"WIN";
    case 348: return L"MENU";
    default:  return L"KEY" + std::to_wstring(key);
    }
}

// "WASD"-style label for the four movement keys (up, left, down, right):
// run together when every name is one character, otherwise joined by "/".
inline std::wstring MoveKeysLabel() {
    static const GameAction order[4] = {
        GameAction::MoveUp, GameAction::MoveLeft,
        GameAction::MoveDown, GameAction::MoveRight };
    std::wstring names[4];
    bool allSingle = true;
    for (int i = 0; i < 4; ++i) {
        names[i] = KeyName(ActionKey(order[i]));
        if (names[i].size() != 1) allSingle = false;
    }
    std::wstring label;
    for (int i = 0; i < 4; ++i) {
        if (i > 0 && !allSingle) label += L"/";
        label += names[i];
    }
    return label;
}

// "Q/E/R" for the three skill keys.
inline std::wstring SkillKeysLabel() {
    return KeyName(ActionKey(GameAction::Skill1)) + L"/" +
           KeyName(ActionKey(GameAction::Skill2)) + L"/" +
           KeyName(ActionKey(GameAction::Skill3));
}
