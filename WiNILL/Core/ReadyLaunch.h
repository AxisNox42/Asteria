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
// The tutorial's Easy launch opens only once every step (practice and the card
// pick) is cleared. A press before that shows a notice for g_LaunchLockedT s.
inline bool LaunchUnlocked() { return !g_TutorialMode || g_Stage == TutorialStage::Done; }
inline constexpr float kLaunchLockedSec = 1.8f;
inline float g_LaunchLockedT = 0.0f;

// Guide cards: when the tutorial opens, two cards point at the health ring and
// then the XP line. After that every step opens with its own card (move, shoot,
// dash, defeat, and the launch once the card pick is done) with the same look:
// gold corners on what it is about, the rest of the page faint, and a picture
// of the key or the enemy on the card. The field stands still while a card is
// up; the Confirm key (Space) or a left click moves on. Not saved: every
// tutorial entry shows them again.
enum class HudGuide { Hp, Xp, Move, Shoot, Dash, Kill, Launch, Done };
inline constexpr int kHudGuideCards = 2;         // the opening HUD cards (Hp, Xp)
inline HudGuide g_HudGuide = HudGuide::Done;
inline float g_HudGuideT = 0.0f;                 // seconds on the current card
inline constexpr float kHudGuideDelay = 0.50f;   // the first card waits for the page to settle
inline constexpr float kHudGuideReady = 0.35f;   // a card can be closed this long after it shows
// The pause before each step card, in seconds (Move, Shoot, Dash, Kill,
// Launch): after the step before it is cleared (the HUD cards, for Move) the
// field keeps running this long, so the move that cleared it is seen to land
// and the player gets a breath before the next card stops the field. Tune
// the tutorial's pacing here.
inline constexpr float kStepGap[] = { 0.6f, 1.2f, 1.2f, 1.2f, 0.5f };
// Step cards already shown this tutorial (bit per HudGuide value).
inline unsigned g_StepCardsShown = 0;
inline bool IsStepCard(HudGuide g) { return g >= HudGuide::Move && g <= HudGuide::Launch; }
static_assert(sizeof(kStepGap) / sizeof(kStepGap[0]) == (int)HudGuide::Launch - (int)HudGuide::Move + 1,
              "a gap per step card");
inline float StepGap(HudGuide g) { return IsStepCard(g) ? kStepGap[(int)g - (int)HudGuide::Move] : 0.0f; }
inline bool HudGuideActive() { return g_TutorialMode && g_HudGuide != HudGuide::Done; }
// The card is drawn (past the first card's opening delay).
inline bool HudGuideShown() {
    return HudGuideActive() && (g_HudGuide != HudGuide::Hp || g_HudGuideT >= kHudGuideDelay);
}
// Opens a step's card once per tutorial. Returns true when it opened.
inline bool OpenStepCard(HudGuide card) {
    if (!g_TutorialMode || !IsStepCard(card) || g_HudGuide != HudGuide::Done) return false;
    const unsigned bit = 1u << (unsigned)card;
    if (g_StepCardsShown & bit) return false;
    g_StepCardsShown |= bit;
    g_HudGuide = card;
    g_HudGuideT = 0.0f;
    return true;
}
// Confirm pressed while the guide is up. Returns true when the press was the
// guide's (a card closed, or it was too early to close one).
inline bool HudGuideConfirm() {
    if (!HudGuideActive()) return false;
    const float since = g_HudGuide == HudGuide::Hp ? g_HudGuideT - kHudGuideDelay : g_HudGuideT;
    if (since < kHudGuideReady) return true;
    g_HudGuide = g_HudGuide == HudGuide::Hp ? HudGuide::Xp : HudGuide::Done;
    g_HudGuideT = 0.0f;
    return true;
}

// Card pick guide (step 5): three cards over the real card screen, same look
// as the others: the cards (pick 1 of 3), one card's rarity and text (glossary
// words), then how to choose. No card can be taken while it is up; Space or a
// click moves it on. Not saved: every tutorial shows it.
enum class PickGuide { Cards, Detail, Keys, Done };
inline constexpr int kPickGuideCards = 3;
inline PickGuide g_PickGuide = PickGuide::Done;
inline float g_PickGuideT = 0.0f;   // seconds on the current card, once the cards are dealt
inline bool PickGuideActive() { return g_TutorialMode && g_PickGuide != PickGuide::Done; }
inline void OpenPickGuide() { g_PickGuide = PickGuide::Cards; g_PickGuideT = 0.0f; }
// Confirm (Space or a click) while it is up. True when the press was the guide's.
inline bool PickGuideConfirm() {
    if (!PickGuideActive()) return false;
    if (g_PickGuideT < kHudGuideReady) return true;
    g_PickGuide = (PickGuide)((int)g_PickGuide + 1);
    g_PickGuideT = 0.0f;
    return true;
}

inline void BeginTutorial() {
    g_HudGuide = HudGuide::Hp;
    g_HudGuideT = 0.0f;
    g_StepCardsShown = 0;
    g_PickGuide = PickGuide::Done;
    g_PickGuideT = 0.0f;
    g_LaunchT = -1.0f;
    g_Stage = TutorialStage::Practice;
    g_PicksTaken = false;
    g_FieldResetPending = true;
    g_LaunchLockedT = 0.0f;
}

// Set by the main loop for the frames the tutorial card screen is up. The READY scene then only draws, and the card screen is drawn
// mini-sized inside the practice field.
inline bool g_SelectionUp = false;

// READY layout: authored on a canvas that is fitted to the window. The practice
// field is centred horizontally; SceneReady.cpp lays everything else out from
// these same values.
inline constexpr float kCanvasW = 1920.0f, kCanvasH = 1080.0f;
inline constexpr float kFieldW = 960.0f, kFieldH = 540.0f;
inline constexpr float kFieldX = (kCanvasW - kFieldW) * 0.5f, kFieldY = 158.0f;

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
