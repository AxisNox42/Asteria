#pragma once

#include <string>

// ── 게임플레이 키 바인딩 (데이터 + 규칙, 입력 상태에는 의존하지 않음) ──
//   게임 로직은 GLFW 키 코드를 직접 읽지 않고 GameAction 으로만 입력을 묻는다
//   (Input.h 의 ActionHeld). 메뉴 탐색/ESC/디버그 키는 바인딩 대상이 아님.
//   새 액션은 반드시 끝에 붙인다: 저장 파일의 keybind<N> 이 이 순서를 쓴다.

enum class GameAction {
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Dash,
    Skill1, Skill2, Skill3,
    Confirm,               // READY 출격 · 증강 카드 확정
    Pause,                 // 일시정지 / 재개 (ESC 는 항상 일시정지)
    Card1, Card2, Card3,   // 증강 카드 고르기 (마우스로도 고를 수 있음)
    Glossary,              // 증강 카드: 고른 카드의 용어 설명을 차례로 열기 (마우스는 올려서 봄)
    Count
};
inline constexpr int GAME_ACTION_COUNT = (int)GameAction::Count;

// 지정되지 않은 액션의 키 값.
inline constexpr int kKeyUnbound = -1;

// GLFW_KEY_* 와 같은 값 (GLFW 헤더 없이 쓰기 위해 값만 둔다).
inline constexpr int kBindKeySpace = 32;
inline constexpr int kBindKeyEscape = 256;
inline constexpr int kBindKeyA = 65, kBindKeyD = 68, kBindKeyE = 69, kBindKeyQ = 81;
inline constexpr int kBindKeyR = 82, kBindKeyS = 83, kBindKeyW = 87;
inline constexpr int kBindKeyTab = 258;
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
    kBindKeySpace,       // Confirm
    kBindKeySpace,       // Pause (Confirm 과 같은 키를 함께 쓸 수 있음)
    '1', '2', '3',
    kBindKeyTab,         // Glossary
}};

inline KeyBindSet g_KeyBinds = kDefaultKeyBinds;

inline int ActionKey(GameAction a) { return g_KeyBinds.key[(int)a]; }
inline bool ActionBound(GameAction a) { return ActionKey(a) != kKeyUnbound; }

inline void ResetKeyBindings() { g_KeyBinds = kDefaultKeyBinds; }

// 필수 액션: 지정되지 않으면 설정 화면을 나갈 수 없다.
//   선택 액션: 일시정지 (ESC 로도 됨), 카드 1~3 (마우스로도 고름), 용어 설명 (마우스로도 봄).
inline bool ActionRequired(GameAction a) {
    switch (a) {
    case GameAction::Pause:
    case GameAction::Card1: case GameAction::Card2: case GameAction::Card3:
    case GameAction::Glossary:
        return false;
    default:
        return true;
    }
}

// 같은 키를 함께 써도 되는 짝: 확정(READY·카드 화면)과 일시정지(전투 중)는
// 쓰이는 상태가 겹치지 않는다.
inline bool ActionsMayShareKey(int a, int b) {
    const int c = (int)GameAction::Confirm, p = (int)GameAction::Pause;
    return (a == c && b == p) || (a == p && b == c);
}

// 좌우 구분이 없는 키는 왼쪽 값으로 통일한다 (오른쪽 Shift == 왼쪽 Shift).
inline int NormalizeBindKey(int key) {
    switch (key) {
    case kBindKeyRightShift: return kBindKeyLeftShift;
    case kBindKeyRightCtrl:  return kBindKeyLeftCtrl;
    case kBindKeyRightAlt:   return kBindKeyLeftAlt;
    default:             return key;
    }
}

// ESC 만 고정: 키 지정 취소 · 뒤로 · 일시정지에 항상 쓰인다.
inline bool KeyReserved(int key) {
    return NormalizeBindKey(key) == kBindKeyEscape;
}

// 바인딩할 수 있는 키: 알려진 GLFW 키 범위 (스페이스 이상), ESC 제외.
inline bool KeyBindable(int key) {
    key = NormalizeBindKey(key);
    if (key < 32 || key > kBindKeyLast) return false;
    return !KeyReserved(key);
}

// action 에 key 를 지정할 때 '지정 안 됨' 으로 바뀌는 다른 액션들.
// out 에 액션 번호를 채우고 개수를 돌려준다.
inline int KeyBindingConflicts(const KeyBindSet& set, GameAction action, int key,
                               int out[GAME_ACTION_COUNT]) {
    key = NormalizeBindKey(key);
    const int self = (int)action;
    int n = 0;
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        if (i != self && set.key[i] == key && !ActionsMayShareKey(self, i))
            out[n++] = i;
    return n;
}

// action 에 key 를 지정하고, 같은 키를 쓰던 (함께 쓸 수 없는) 액션은 지정 해제한다.
inline void ApplyKeyBinding(KeyBindSet& set, GameAction action, int key) {
    key = NormalizeBindKey(key);
    int conflicts[GAME_ACTION_COUNT];
    const int n = KeyBindingConflicts(set, action, key, conflicts);
    for (int i = 0; i < n; ++i) set.key[conflicts[i]] = kKeyUnbound;
    set.key[(int)action] = key;
}

// 지정되지 않은 필수 액션 수 (out 이 있으면 액션 번호를 채운다).
inline int MissingRequiredKeys(const KeyBindSet& set, int* out = nullptr) {
    int n = 0;
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        if (set.key[i] == kKeyUnbound && ActionRequired((GameAction)i)) {
            if (out) out[n] = i;
            ++n;
        }
    return n;
}

// 불러온 값 검증: 잘못된 키는 해당 액션만 기본값으로 (선택 액션은 '지정 안 됨'
// 도 유지). 함께 쓸 수 없는 중복은 선택 액션 쪽을 해제하고, 둘 다 필수면 전체를
// 기본값으로 되돌린다.
inline void SanitizeKeyBindings(KeyBindSet& set) {
    for (int i = 0; i < GAME_ACTION_COUNT; ++i) {
        const bool optional = !ActionRequired((GameAction)i);
        if (set.key[i] == kKeyUnbound && optional) continue;
        set.key[i] = KeyBindable(set.key[i]) ? NormalizeBindKey(set.key[i])
                                             : kDefaultKeyBinds.key[i];
    }
    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
        for (int j = i + 1; j < GAME_ACTION_COUNT; ++j) {
            if (set.key[i] == kKeyUnbound || set.key[i] != set.key[j]) continue;
            if (ActionsMayShareKey(i, j)) continue;
            if (!ActionRequired((GameAction)j))      set.key[j] = kKeyUnbound;
            else if (!ActionRequired((GameAction)i)) set.key[i] = kKeyUnbound;
            else { set = kDefaultKeyBinds; return; }
        }
}

// 표시용 키 이름 (HUD/설정 화면 공용).
inline std::wstring KeyName(int key) {
    if (key == kKeyUnbound) return L"—";
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
