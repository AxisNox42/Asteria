#pragma once
// The play's build at a glance: augments taken (as their constellations, a
// hover explains one) and the player's stats (total = base + this play's gains).
// The results screen and the pause screen draw the same panel.

#include <cmath>
#include "SceneTextContext.h"

class TextRenderer;
struct PlayerStats;

namespace run_build {

// The glyph atlases are baked at fixed sizes; pick the smallest that reaches
// the wanted line height without being enlarged (see the .cpp).
struct TextPick {
    TextRenderer* r;
    float scale;
};
TextPick PickText(const SceneTextContext& t, float lineHeightPx);

inline float Snap(float v) { return std::floor(v + 0.5f); }

constexpr int kWrapChars = 160;
// Greedy wrap by measured width; returns the number of lines written.
int WrapLines(TextRenderer& r, float scale, const wchar_t* text, float maxW,
              wchar_t (*lines)[kWrapChars], int maxLines);

enum Metric { kAttack, kFireRate, kBulletSpeed, kMove, kMaxHp, kRegen,
              kCritChance, kCritDamage, kMetricCount };

struct StatRow {
    float total = 0.0f, base = 0.0f, run = 0.0f, trial = 0.0f;
};

// Per-screen state: each screen keeps its own and resets it when it opens.
struct PanelState {
    bool statsOpen = false;
    float statsT = 0.0f;
    bool rowsValid = false;
    StatRow rows[kMetricCount];
    int descAug = -1;
    int descStat = -1;   // the stat row last hovered; wins over descAug while >= 0
    float descT = 0.0f;
    void Reset() { *this = PanelState(); }
};

struct PanelIn {
    const SceneTextContext* text = nullptr;
    float panelX = 0.0f, panelW = 0.0f;
    float top = 0.0f;           // top of the panel
    float bottomLimit = 0.0f;   // the panel shrinks its graphics to end above this
    float uiS = 1.0f;
    float alpha = 1.0f;         // overall fade
    float delta = 0.0f, now = 0.0f;
    double mx = 0.0, my = 0.0;
    bool click = false;         // a fresh left click this frame
    bool canHover = true;       // cards and the stats toggle react to the pointer
    bool showShell = true;      // the player's look above the augments
    bool statsAlwaysOpen = false;   // no fold-out toggle; the table stays open
    float accentR = 0.32f, accentG = 0.82f, accentB = 1.0f;
    float labH = 0.0f;          // label line height; 0 = measure it here
    bool keyline = false;       // dark outline per glyph, for text over the live play
};

void DrawPanel(PanelState& st, const PanelIn& in);

}  // namespace run_build
