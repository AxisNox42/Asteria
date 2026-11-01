#pragma once
// ─────────────────────────────────────────────────────────────
// READY practice-field state. The READY screen is the lobby's TUTORIAL: it is
// opened from the lobby button; Space launches an Easy run, ESC returns to the
// lobby. PLAY goes straight to RUNNING and never shows it.
//   Steps 1-4 are the practice objectives (Core/ReadyPractice.h). The two that
//   follow use the game's real selection screens:
//     5  AugPick   the fourth kill levels the ship up and the augment cards open
//     6  NerfWait  a wave clock with kNerfWaitSeconds left runs out, then the
//        NerfPick  wave-boundary nerf cards open exactly as at a real wave end
// No GL and no game headers.
// ─────────────────────────────────────────────────────────────

namespace ready_launch {

// True while the READY practice field is open as the lobby tutorial.
inline bool g_TutorialMode = false;

enum class TutorialStage { Practice, AugPick, NerfWait, NerfPick, Done };
inline TutorialStage g_Stage = TutorialStage::Practice;

// Seconds the step-6 wave clock has left when it starts.
inline constexpr float kNerfWaitSeconds = 4.0f;
inline float g_NerfClock = kNerfWaitSeconds;

// Set by the READY scene when the wave clock runs out; the main loop opens the
// nerf pick (the same call a real wave boundary makes) and clears it.
inline bool g_RequestNerfPick = false;

// A card or nerf was taken during the tutorial, so the run launched from here
// must start from a clean loadout instead of carrying them over.
inline bool g_PicksTaken = false;

inline void BeginTutorial() {
    g_Stage = TutorialStage::Practice;
    g_NerfClock = kNerfWaitSeconds;
    g_RequestNerfPick = false;
    g_PicksTaken = false;
}

// Set by the main loop for the frames a tutorial card screen (augment or nerf
// pick) is up. The READY scene then only draws, and the card screen is drawn
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
    return g_Stage == TutorialStage::AugPick || g_Stage == TutorialStage::NerfPick;
}

} // namespace ready_launch
