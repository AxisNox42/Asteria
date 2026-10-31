#pragma once
// ─────────────────────────────────────────────────────────────
// READY screen keyboard panel model: a full ANSI keyboard (function row,
// main block, navigation cluster, arrows, numpad) as key caps in key units,
// and what each cap means right now. No GL, so
// tests/keyboard_panel_test.cpp can build it alone.
//   The layout only says where each physical key sits. Which caps light up
//   as move / dash / skill comes from the current key bindings
//   (System/KeyBindings.h) every time RoleOf is asked, so a rebind in the
//   settings moves the highlight with it.
//   Space (start) and Esc (leave) are fixed by GameManager, not bindable.
// ─────────────────────────────────────────────────────────────
#include <initializer_list>
#include <string>
#include <vector>
#include "../System/KeyBindings.h"

namespace keyboard_panel {

// GLFW_KEY_* values not already named in KeyBindings.h.
namespace key {
inline constexpr int Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47;
inline constexpr int Semicolon = 59, Equal = 61;
inline constexpr int LeftBracket = 91, Backslash = 92, RightBracket = 93, Grave = 96;
inline constexpr int Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261;
inline constexpr int Right = 262, Left = 263, Down = 264, Up = 265;
inline constexpr int PageUp = 266, PageDown = 267, Home = 268, End = 269;
inline constexpr int CapsLock = 280, ScrollLock = 281, NumLock = 282;
inline constexpr int PrintScreen = 283, Pause = 284;
inline constexpr int F1 = 290;
inline constexpr int Kp0 = 320, KpDecimal = 330, KpDivide = 331, KpMultiply = 332;
inline constexpr int KpSubtract = 333, KpAdd = 334, KpEnter = 335;
inline constexpr int LeftSuper = 343, RightSuper = 347, Menu = 348;
}

struct KeyCap {
    int   key;          // GLFW key code
    float x, y, w, h;   // key units; one standard key is 1 x 1
};

inline constexpr float kLayoutW = 22.5f;
inline constexpr float kLayoutH = 6.5f;

namespace detail {
struct Item { int key; float w = 1.0f; float h = 1.0f; };
inline Item Gap(float w) { return { 0, w, 1.0f }; }

inline std::vector<KeyCap> BuildLayout() {
    using namespace key;
    std::vector<KeyCap> caps;
    auto row = [&](float y, std::initializer_list<Item> items) {
        float x = 0.0f;
        for (const Item& it : items) {
            if (it.key != 0) caps.push_back({ it.key, x, y, it.w, it.h });
            x += it.w;
        }
    };
    auto fn = [](int n) { return Item{ F1 + n - 1 }; };
    auto kp = [](int n) { return Item{ Kp0 + n }; };
    row(0.0f, { { kBindKeyEscape }, Gap(1.0f), fn(1), fn(2), fn(3), fn(4), Gap(0.5f),
                fn(5), fn(6), fn(7), fn(8), Gap(0.5f), fn(9), fn(10), fn(11), fn(12),
                Gap(0.25f), { PrintScreen }, { ScrollLock }, { Pause } });
    row(1.5f, { { Grave }, { '1' }, { '2' }, { '3' }, { '4' }, { '5' }, { '6' }, { '7' },
                { '8' }, { '9' }, { '0' }, { Minus }, { Equal }, { Backspace, 2.0f },
                Gap(0.25f), { Insert }, { Home }, { PageUp },
                Gap(0.25f), { NumLock }, { KpDivide }, { KpMultiply }, { KpSubtract } });
    row(2.5f, { { Tab, 1.5f }, { 'Q' }, { 'W' }, { 'E' }, { 'R' }, { 'T' }, { 'Y' }, { 'U' },
                { 'I' }, { 'O' }, { 'P' }, { LeftBracket }, { RightBracket }, { Backslash, 1.5f },
                Gap(0.25f), { Delete }, { End }, { PageDown },
                Gap(0.25f), kp(7), kp(8), kp(9), { KpAdd, 1.0f, 2.0f } });
    row(3.5f, { { CapsLock, 1.75f }, { 'A' }, { 'S' }, { 'D' }, { 'F' }, { 'G' }, { 'H' },
                { 'J' }, { 'K' }, { 'L' }, { Semicolon }, { Apostrophe }, { Enter, 2.25f },
                Gap(3.5f), kp(4), kp(5), kp(6) });
    row(4.5f, { { kBindKeyLeftShift, 2.25f }, { 'Z' }, { 'X' }, { 'C' }, { 'V' }, { 'B' },
                { 'N' }, { 'M' }, { Comma }, { Period }, { Slash }, { kBindKeyRightShift, 2.75f },
                Gap(1.25f), { Up }, Gap(1.25f), kp(1), kp(2), kp(3), { KpEnter, 1.0f, 2.0f } });
    row(5.5f, { { kBindKeyLeftCtrl, 1.25f }, { LeftSuper, 1.25f }, { kBindKeyLeftAlt, 1.25f },
                { kBindKeySpace, 6.25f }, { kBindKeyRightAlt, 1.25f }, { RightSuper, 1.25f },
                { Menu, 1.25f }, { kBindKeyRightCtrl, 1.25f },
                Gap(0.25f), { Left }, { Down }, { Right }, Gap(0.25f), { Kp0, 2.0f }, { KpDecimal } });
    return caps;
}
} // namespace detail

inline const std::vector<KeyCap>& Layout() {
    static const std::vector<KeyCap> caps = detail::BuildLayout();
    return caps;
}

enum class KeyRole { None, Move, Dash, Skill, Start, Quit };

// What pressing this key does on the READY screen, by the current bindings.
inline KeyRole RoleOf(int glfwKey) {
    const int k = NormalizeBindKey(glfwKey);
    if (k == kBindKeySpace)  return KeyRole::Start;   // GameManager starts the run on Space
    if (k == kBindKeyEscape) return KeyRole::Quit;
    for (int i = 0; i < GAME_ACTION_COUNT; ++i) {
        if (ActionKey((GameAction)i) != k) continue;
        switch ((GameAction)i) {
        case GameAction::MoveUp: case GameAction::MoveDown:
        case GameAction::MoveLeft: case GameAction::MoveRight: return KeyRole::Move;
        case GameAction::Dash: return KeyRole::Dash;
        default:               return KeyRole::Skill;
        }
    }
    return KeyRole::None;
}

// Text printed on the cap. Short forms where the settings name is long;
// numpad caps show only their symbol because the cluster says "numpad".
inline std::wstring CapLabel(int glfwKey) {
    using namespace key;
    if (glfwKey >= Kp0 && glfwKey <= Kp0 + 9) return std::wstring(1, (wchar_t)(L'0' + glfwKey - Kp0));
    switch (glfwKey) {
    case kBindKeyEscape: return L"Esc";
    case Backspace:      return L"Back";
    case Tab:            return L"Tab";
    case CapsLock:       return L"Caps";
    case Enter:          return L"Enter";
    case kBindKeyLeftShift: case kBindKeyRightShift: return L"Shift";
    case kBindKeyLeftCtrl:  case kBindKeyRightCtrl:  return L"Ctrl";
    case kBindKeyLeftAlt:   case kBindKeyRightAlt:   return L"Alt";
    case LeftSuper: case RightSuper: return L"Win";
    case Menu:        return L"Menu";
    case Insert:      return L"Ins";
    case Delete:      return L"Del";
    case Home:        return L"Home";
    case End:         return L"End";
    case PageUp:      return L"PgUp";
    case PageDown:    return L"PgDn";
    case PrintScreen: return L"PrtSc";
    case ScrollLock:  return L"ScrLk";
    case Pause:       return L"Pause";
    case NumLock:     return L"Num";
    case KpDivide:    return L"/";
    case KpMultiply:  return L"*";
    case KpSubtract:  return L"-";
    case KpAdd:       return L"+";
    case KpEnter:     return L"Ent";
    case KpDecimal:   return L".";
    case Up: case Down: case Left: case Right: return L"";   // drawn as an arrow
    default:          return KeyName(glfwKey);
    }
}

// Arrow caps draw a line arrow instead of text: the direction it points,
// as a unit vector in screen space (y down); {0, 0} for every other key.
struct ArrowDir { float x, y; };
inline ArrowDir CapArrow(int glfwKey) {
    switch (glfwKey) {
    case key::Up:    return { 0.0f, -1.0f };
    case key::Down:  return { 0.0f,  1.0f };
    case key::Left:  return { -1.0f, 0.0f };
    case key::Right: return { 1.0f,  0.0f };
    default:         return { 0.0f,  0.0f };
    }
}

} // namespace keyboard_panel
