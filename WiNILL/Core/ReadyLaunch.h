#pragma once
// ─────────────────────────────────────────────────────────────
// READY practice-field state. The READY screen is the lobby's TUTORIAL: it is
// opened from the lobby button; Space launches an Easy run, ESC returns to the lobby. PLAY goes
// straight to RUNNING and never shows it.
// No GL and no game headers.
// ─────────────────────────────────────────────────────────────

namespace ready_launch {

// True while the READY practice field is open as the lobby tutorial.
inline bool g_TutorialMode = false;

} // namespace ready_launch
