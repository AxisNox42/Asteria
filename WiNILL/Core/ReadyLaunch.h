#pragma once
// ─────────────────────────────────────────────────────────────
// READY practice-field state. The READY screen is the lobby's TUTORIAL: it is
// opened from the lobby button; Space launches an Easy run, ESC returns to the
// lobby. PLAY goes straight to RUNNING and never shows it.
//   Steps 1-4 are the practice objectives (Core/ReadyPractice.h). The one that
//   follows uses the game's real selection screen:
//     5  AugPick   the fourth kill levels the ship up and the card screen opens
// No GL and no game headers.
// ─────────────────────────────────────────────────────────────

namespace ready_launch {

// True while the READY practice field is open as the lobby tutorial.
inline bool g_TutorialMode = false;

enum class TutorialStage { Practice, AugPick, Done };
inline TutorialStage g_Stage = TutorialStage::Practice;

// A card was taken during the tutorial, so the run launched from here must
// start from a clean loadout instead of carrying it over.
inline bool g_PicksTaken = false;

// Set when the tutorial opens from the lobby: the practice field starts over.
// Pause, settings and the card pick leave the field as it was.
inline bool g_FieldResetPending = false;

// Space on the practice field does not start the run at once: the field's wall
// first shrinks a little (kLaunchShrinkSec), then swells out to the screen edge
// (kLaunchExpandSec), and the run starts when it arrives. g_LaunchT counts the
// seconds of that animation; negative = not launching.
inline constexpr float kLaunchShrinkSec = 0.22f;
inline constexpr float kLaunchExpandSec = 0.73f;
inline constexpr float kLaunchShrinkTo = 0.93f;   // field scale at the bottom of the squeeze
inline float g_LaunchT = -1.0f;
// After the launch the run's HUD fades in over kRunIntroSec (g_RunIntro 0 -> 1)
// and the ship cannot be hurt for kStartGraceSec, so the first frames are calm.
inline constexpr float kRunIntroSec = 0.45f;
inline constexpr float kStartGraceSec = 0.8f;
inline float g_RunIntro = 1.0f;
inline float g_StartGraceT = 0.0f;
inline bool Launching() { return g_LaunchT >= 0.0f; }
inline void StartLaunch() {
    if (!Launching()) g_LaunchT = 0.0f;
    else g_LaunchT = kLaunchShrinkSec + kLaunchExpandSec;   // a second press skips ahead
}
inline void CancelLaunch() { g_LaunchT = -1.0f; }

inline void BeginTutorial() {
    g_LaunchT = -1.0f;
    g_Stage = TutorialStage::Practice;
    g_PicksTaken = false;
    g_FieldResetPending = true;
}

// Set by the main loop for the frames the tutorial card screen is up. The READY scene then only draws, and the card screen is drawn
// mini-sized inside the practice field.
inline bool g_SelectionUp = false;

// READY layout: authored on a canvas that is fitted to the window. The practice
// field is centred horizontally; SceneReady.cpp lays everything else out from
// these same values.
inline constexpr float kCanvasW = 1920.0f, kCanvasH = 1080.0f;
inline constexpr float kFieldW = 800.0f, kFieldH = 450.0f;
inline constexpr float kFieldX = (kCanvasW - kFieldW) * 0.5f, kFieldY = 180.0f;

// The practice field's rectangle on a window of sw x sh.
inline void FieldRect(float sw, float sh, float& x, float& y, float& w, float& h) {
    const float kx = sw / kCanvasW, ky = sh / kCanvasH;
    const float k = kx < ky ? kx : ky;
    x = (sw - kCanvasW * k) * 0.5f + kFieldX * k;
    y = (sh - kCanvasH * k) * 0.5f + kFieldY * k;
    w = kFieldW * k;
    h = kFieldH * k;
}

// The card screens are laid out as if the window were the practice field
// divided by this (bigger text than a straight shrink of the full screen).
inline constexpr float kCardScreenScale = 0.68f;

// True while a selection screen opened by the tutorial is up (or about to be).
inline bool SelectionOpen() {
    return g_Stage == TutorialStage::AugPick;
}

} // namespace ready_launch
