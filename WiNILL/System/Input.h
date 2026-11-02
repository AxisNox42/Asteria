#pragma once

#include "KeyBindings.h"

struct GLFWwindow;
enum class GameState;

struct InputFocusTransition {
    bool lost = false;
    bool gained = false;
};

extern bool  keys[1024];
extern float g_ScrollAccum;

// Gameplay actions read through the bindings (KeyBindings.h). Shift/Ctrl/Alt
// count for either side.
inline bool KeyHeld(int key) {
    if (key < 0 || key >= 1024) return false;
    if (keys[key]) return true;
    switch (key) {
    case kBindKeyLeftShift:  return keys[kBindKeyRightShift];
    case kBindKeyLeftCtrl:   return keys[kBindKeyRightCtrl];
    case kBindKeyLeftAlt:    return keys[kBindKeyRightAlt];
    default:             return false;
    }
}
inline bool ActionHeld(GameAction a) { return KeyHeld(ActionKey(a)); }
inline bool AnyMoveHeld() {
    return ActionHeld(GameAction::MoveUp)   || ActionHeld(GameAction::MoveDown) ||
           ActionHeld(GameAction::MoveLeft) || ActionHeld(GameAction::MoveRight);
}

// Press order: g_KeyPressSeq[key] is stamped from a running counter on every
// press, so a larger value means "pressed more recently".
extern unsigned g_KeyPressSeq[1024];
inline unsigned KeyPressSeq(int key) {
    if (key < 0 || key >= 1024) return 0;
    unsigned s = g_KeyPressSeq[key];
    int other = -1;
    switch (key) {
    case kBindKeyLeftShift: other = kBindKeyRightShift; break;
    case kBindKeyLeftCtrl:  other = kBindKeyRightCtrl;  break;
    case kBindKeyLeftAlt:   other = kBindKeyRightAlt;   break;
    default: break;
    }
    if (other >= 0 && keys[other] && g_KeyPressSeq[other] > s) s = g_KeyPressSeq[other];
    return s;
}

// Movement with opposite keys resolved: holding W then pressing S moves down
// (the last pressed key wins) instead of the two cancelling out.
struct MoveInput { bool up, down, left, right; };
inline MoveInput ResolvedMoveHeld() {
    MoveInput m{ ActionHeld(GameAction::MoveUp),   ActionHeld(GameAction::MoveDown),
                 ActionHeld(GameAction::MoveLeft), ActionHeld(GameAction::MoveRight) };
    if (m.up && m.down) {
        const bool downLast = KeyPressSeq(ActionKey(GameAction::MoveDown)) >
                              KeyPressSeq(ActionKey(GameAction::MoveUp));
        m.up = !downLast; m.down = downLast;
    }
    if (m.left && m.right) {
        const bool rightLast = KeyPressSeq(ActionKey(GameAction::MoveRight)) >
                               KeyPressSeq(ActionKey(GameAction::MoveLeft));
        m.left = !rightLast; m.right = rightLast;
    }
    return m;
}

// Key capture for the controls settings: while active, the next key press is
// stored in g_CapturedKey (-1 = none yet) instead of being used as a gameplay
// or text key. The caller starts/stops it and consumes the key.
extern bool g_KeyCaptureActive;
extern int  g_CapturedKey;

void InputRegisterCallbacks(GLFWwindow* window);
void InputUnregisterCallbacks(GLFWwindow* window);
void InputUpdateTextInput(GLFWwindow* window);
void InputSetCodexImeAnchor(float compositionX, float caretX,
                            float y, float textHeight);
// Clear callback state when the native window loses focus.  GLFW may not
// deliver release events while another application owns the keyboard.
void InputClearState();
InputFocusTransition InputUpdateWindowFocus(GLFWwindow* window);
void InputUpdateGameplayCursor(GLFWwindow* window, GameState state,
                               bool crosshairEnabled, bool debugVisible);
