#include "Scenes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "GameContext.h"

#include "SceneUI.h"
#include "SceneInternal.h"
#include "UiLayout.h"
#include "WindowChrome.h"
#include "Settings.h"
#include "Translations.h"
#include "TutorialText.h"
#include "Meta.h"
#include "Achievements.h"
#include "Codex.h"
#include "Augment.h"
#include "PassiveAugments.h"
#include "../../Render/AugCardModel.h"
#include "PlayerStats.h"
#include "RunStats.h"
#include "Weapons.h"
#include "ExpSystem.h"
#include "SaveSystem.h"
#include "DrawPrim.h"
#include "../../Render/UiColors.h"
#include "../../Render/PlanetShader.h"
#include "../../Render/NebulaGlowShader.h"
#include "MainShader.h"
#include "../../System/Audio.h"
#include "TextRenderer.h"
#include "IconSystem.h"
#include "../System/SystemInfo.h"
#include "Camera.h"
#include "EntityDraw.h"
#include "PlayerWeaponShell.h"
#include "Monster.h"
#include "../../Render/Juice.h"
#include "../../Render/WaveHud.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <functional>

#include "SceneInternal.h"

extern float g_GameTime;   // main.cpp: seconds of the run

namespace {
enum class GameOverAction : int { Restart, ReturnToMenu, Quit };
constexpr int kGameOverActionCount = static_cast<int>(GameOverAction::Quit) + 1;

void ApplyGameOverAction(GameOverAction action, GLFWwindow* window,
                         const SceneCtx& scene,
                         const GameOverSceneContext& state) {
    switch (action) {
    case GameOverAction::Restart:
        if (scene.reset) scene.reset();
        FinalizeLoadout(scene, FixedWeaponForSelectedJob());
        break;
    case GameOverAction::ReturnToMenu:
        state.currentState = GameState::MAIN_MENU;
        break;
    case GameOverAction::Quit:
        if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    }
}

// The glyph atlases are baked at fixed sizes (S, L, XL) and only ever scaled
// at draw time, so text blurs as soon as it is drawn above its baked size.
// Pick the smallest atlas that can reach the wanted line height without
// being enlarged.
struct TextPick {
    TextRenderer* r;
    float scale;
};

TextPick PickText(const SceneTextContext& t, float lineHeightPx) {
    TextRenderer* order[3] = { &t.body, &t.title, &g_TextXL };
    for (TextRenderer* r : order) {
        const float lh = r->LineHeightPixels();
        if (lh > 0.0f && lineHeightPx <= lh)
            return { r, lineHeightPx / lh };
    }
    const float lh = std::max(1.0f, g_TextXL.LineHeightPixels());
    return { &g_TextXL, lineHeightPx / lh };
}

float Snap(float v) { return std::floor(v + 0.5f); }

constexpr int kWrapChars = 160;

// Greedy wrap by measured width. Breaks at spaces when it can and between
// characters otherwise (Korean and Japanese run long without spaces).
int WrapLines(TextRenderer& r, float scale, const wchar_t* text, float maxW,
              wchar_t (*lines)[kWrapChars], int maxLines) {
    int count = 0;
    wchar_t cur[kWrapChars] = {};
    int len = 0, lastSpace = -1;
    auto emit = [&](int n) {
        if (count >= maxLines) return;
        while (n > 0 && cur[n - 1] == L' ') --n;
        wmemcpy(lines[count], cur, (size_t)n);
        lines[count][n] = 0;
        ++count;
    };
    for (const wchar_t* p = text; p && *p && count < maxLines; ++p) {
        if (len >= kWrapChars - 2) { emit(len); len = 0; lastSpace = -1; }
        if (*p == L' ') lastSpace = len;
        cur[len++] = *p;
        cur[len] = 0;
        if (len > 1 && r.Width(cur, scale) > maxW) {
            if (lastSpace > 0) {
                emit(lastSpace);
                const int rest = len - lastSpace - 1;
                wmemmove(cur, cur + lastSpace + 1, (size_t)rest);
                len = rest;
            } else {
                emit(len - 1);
                cur[0] = cur[len - 1];
                len = 1;
            }
            cur[len] = 0;
            lastSpace = -1;
            for (int i = 0; i < len; ++i) if (cur[i] == L' ') lastSpace = i;
        }
    }
    if (len > 0 && count < maxLines) emit(len);
    return count;
}

// ---- Player stats: weapon and meta, plus trial ----------------------------
enum Metric { kAttack, kFireRate, kBulletSpeed, kMove, kMaxHp, kRegen,
              kMetricCount };

float MetricValue(const PlayerStats& s, int m) {
    switch (m) {
    case kAttack:      return s.GetBaseDamage() * s.damageMultiplier;
    case kFireRate:    return s.fireInterval > 0.0001f ? 1.0f / s.fireInterval : 0.0f;
    case kBulletSpeed: return s.bulletSpeed;
    case kMove:        return s.moveSpeedMult * 100.0f;
    case kMaxHp:       return s.maxHP;
    case kRegen:       return s.regenPerSec;
    default:           return 0.0f;
    }
}

struct StatRow {
    float total = 0.0f, base = 0.0f, run = 0.0f, trial = 0.0f;
};

// total = the run's final stats (augments and level-up cards included);
// base = weapon and meta. The gap splits into the run's gains and the trial cut.
void BuildStatRows(StatRow* rows) {
    const PlayerStats base = PreviewBaseStats(g_CurrentWeapon, g_Scale);
    PlayerStats trial = base;
    ApplyTrialMaxHp(trial);
    for (int m = 0; m < kMetricCount; ++m) {
        const float b = MetricValue(base, m);
        const float tr = MetricValue(trial, m);
        rows[m].base = b;
        rows[m].trial = tr - b;
        rows[m].total = MetricValue(g_Stats, m);
        rows[m].run = rows[m].total - tr;
    }
}

// Parts smaller than this are not worth a "+ x" term.
float MetricEps(int m) {
    return (m == kFireRate || m == kRegen) ? 0.005f : 0.05f;
}
void FormatMetric(wchar_t* out, int m, float v) {
    switch (m) {
    case kAttack:     swprintf_s(out, 32, L"%.1f", v); break;
    case kFireRate:   swprintf_s(out, 32, L"%.2f", v); break;
    case kRegen:      swprintf_s(out, 32, L"%.2f/s", v); break;
    case kMove:       swprintf_s(out, 32, L"%.0f%%", v); break;
    default:          FormatGrouped(std::llround(v), out, 32); break;
    }
}
}

void Scene_GameOver(const SceneCtx& c, const GameOverSceneContext& state) {
    const float sw = c.sw, sh = c.sh;
    const double mx = c.mx, my = c.my;
    const bool lmb = c.lmb;
    const float delta = c.delta;
    GLFWwindow* window = c.window;
    float gof = std::max(0.0f, std::min(1.0f, state.fade));
    bool skippedCinematic = false;
    if (gof < 0.999f && lmb && !state.previousLeftMouseDown) {
        // A click during the collapse is a skip request, never a menu action.
        state.fade = 1.0f;
        gof = 1.0f;
        skippedCinematic = true;
    }
    const float ge = Smoothstep(gof);
    const float now = (float)glfwGetTime();
    const int li = std::clamp(LangIndex(), 0, LANG_COUNT - 1);
    const float uiS = UiScale(sw, sh, 1.0f, 1.0f, 0.72f);

    static const wchar_t* kDeathLabel[LANG_COUNT] = {
        L"\uC0AC\uB9DD\uC6D0\uC778", L"DEATH CAUSE", L"\u6B7B\u56E0"
    };
    static const wchar_t* kVictoryLabel[LANG_COUNT] = {
        L"\uC2B9\uB9AC", L"VICTORY", L"\u52DD\u5229"
    };
    const wchar_t* outcomeLabel = state.victory ? kVictoryLabel[li] : kDeathLabel[li];
    static const wchar_t* kReportLabel[LANG_COUNT] = {
        L"\uD50C\uB808\uC774 \uD1B5\uACC4", L"PLAY STATS", L"\u30D7\u30EC\u30A4\u7D71\u8A08"
    };
    static const wchar_t* kWaveLabel[LANG_COUNT] = {
        L"\uB3C4\uB2EC \uC6E8\uC774\uBE0C", L"WAVE REACHED", L"\u5230\u9054\u30A6\u30A7\u30FC\u30D6"
    };
    static const wchar_t* kLevelLabel[LANG_COUNT] = {
        L"\uB808\uBCA8", L"LEVEL", L"\u30EC\u30D9\u30EB"
    };
    static const wchar_t* kStardustLabel[LANG_COUNT] = {
        L"\uD68D\uB4DD \uBCC4\uAC00\uB8E8", L"STARDUST GAINED", L"\u7372\u5F97\u30B9\u30BF\u30FC\u30C0\u30B9\u30C8"
    };
    static const wchar_t* kBuildStatsLabel[LANG_COUNT] = {
        L"\uD50C\uB808\uC774\uC5B4 \uB2A5\uB825\uCE58", L"PLAYER STATS", L"\u30D7\u30EC\u30A4\u30E4\u30FC\u80FD\u529B\u5024"
    };
    static const wchar_t* kMetricLabel[LANG_COUNT][kMetricCount] = {
        { L"\uACF5\uACA9\uB825", L"\uC5F0\uC0AC\uB825", L"\uD0C4\uC18D", L"\uC774\uB3D9\uC18D\uB3C4", L"HP", L"\uCCB4\uB825 \uC7AC\uC0DD" },
        { L"ATTACK", L"FIRE RATE", L"BULLET SPEED", L"MOVE SPEED", L"HP", L"HP REGEN" },
        { L"\u653B\u6483\u529B", L"\u9023\u5C04\u529B", L"\u5F3E\u901F", L"\u79FB\u52D5\u901F\u5EA6", L"HP", L"HP\u56DE\u5FA9" }
    };
    static const wchar_t* kRoute[LANG_COUNT][kGameOverActionCount] = {
        { L"\uC7AC\uC2DC\uC791", L"\uBA54\uC778 \uBA54\uB274", L"\uC885\uB8CC" },
        { L"RESTART", L"MAIN MENU", L"EXIT" },
        { L"\u3082\u3046\u4E00\u5EA6", L"\u30E1\u30A4\u30F3\u30E1\u30CB\u30E5\u30FC", L"\u7D42\u4E86" }
    };

    // The report owns the full frame.
    SetSceneTextureReveal(1.0f);
    BindMainShader();
    drawRect(0.0f, 0.0f, sw, sh, 0.012f, 0.026f, 0.060f, 0.52f * ge);
    DrawSceneLeftVignette(sw, sh, 0.34f * ge);
    DrawSceneRadialVignette(sw * 0.72f, sh * 0.44f,
                            std::max(sw, sh) * 0.76f, 0.12f * ge);

    const float accentR = 0.32f, accentG = 0.82f, accentB = 1.0f;
    const float margin = std::max(42.0f, sw * 0.085f);
    const float leftX = margin;
    const float leftW = std::min(680.0f, std::max(300.0f, sw * 0.40f));

    // ---- Type sizes (line height in px at uiS = 1). Every size is resolved
    // to the atlas that can draw it without enlarging glyphs. ---------------
    const TextPick hdr  = PickText(state.text, 46.0f * uiS);
    const TextPick lab  = PickText(state.text, 26.0f * uiS);
    const TextPick hero = PickText(state.text, 76.0f * uiS);
    const TextPick val  = PickText(state.text, 40.0f * uiS);

    // Outcome line: the reason is the small line now, shrunk to fit.
    const bool hasDeathReason = state.deathReason && state.deathReason[0];
    float reasonPx = 32.0f * uiS;
    TextPick reasonT = PickText(state.text, reasonPx);
    while (hasDeathReason && reasonPx > 20.0f * uiS &&
           reasonT.r->Width(state.deathReason, reasonT.scale) > leftW) {
        reasonPx -= 2.0f * uiS;
        reasonT = PickText(state.text, reasonPx);
    }

    const float hdrH = hdr.r->Height(kReportLabel[li], hdr.scale);
    const float labH = lab.r->Height(kDeathLabel[li], lab.scale);
    const float reasonH = reasonT.r->Height(L"A", reasonT.scale);
    const float heroH = hero.r->Height(L"0123456789", hero.scale);
    const float valH = val.r->Height(L"0123456789", val.scale);

    // Vertical stack, relative to the top of the block.
    const float ruleY = hdrH + 8.0f * uiS;
    const float deathLabY = ruleY + 20.0f * uiS;
    const float reasonY = deathLabY + labH + 2.0f * uiS;
    const float heroLabY = reasonY + reasonH + 28.0f * uiS;
    const float heroY = heroLabY + labH + 4.0f * uiS;
    const float rule2Y = heroY + heroH + 16.0f * uiS;
    const float rowLabY = rule2Y + 18.0f * uiS;
    const float rowValY = rowLabY + labH + 6.0f * uiS;
    const float statsBottom = rowValY + valH;

    static float s_hovGO[kGameOverActionCount] = {};
    static int   s_confirmGO = -1;   // Restart / Quit awaiting an answer
    if (gof < 0.999f) s_confirmGO = -1;
    const float BW = std::min(520.0f, std::max(280.0f, sw * 0.38f));
    const float BH = std::max(48.0f, std::min(68.0f, sh * 0.075f));
    const float BGAP = std::max(10.0f, sh * 0.013f);
    const float totalBH = (float)kGameOverActionCount * BH
                        + (float)(kGameOverActionCount - 1) * BGAP;
    // Header near the top, commands at the bottom left.
    const float top = Snap(std::max(58.0f, sh * 0.14f));
    const float buttonsAbsY = std::max(top + statsBottom + 30.0f,
        sh - totalBH - std::max(44.0f, sh * 0.06f));

    // Header.
    hdr.r->Draw(kReportLabel[li], Snap(leftX), Snap(top), hdr.scale,
                accentR, accentG, accentB, 0.92f * ge);
    LogoLine(leftX, top + ruleY, leftX + leftW, top + ruleY,
             1.0f, accentR, accentG, accentB, 0.24f * ge);

    // Outcome: small label, then the reason.
    const wchar_t* reasonText = hasDeathReason ? state.deathReason : outcomeLabel;
    if (hasDeathReason) {
        lab.r->Draw(outcomeLabel, Snap(leftX), Snap(top + deathLabY), lab.scale,
                    0.64f, 0.76f, 0.88f, 0.88f * ge);
    }
    reasonT.r->Draw(reasonText, Snap(leftX), Snap(top + reasonY), reasonT.scale,
                    1.0f,
                    state.victory ? 0.80f : 0.46f,
                    state.victory ? 0.28f : 0.43f, 0.92f * ge);

    auto drawLabel = [&](const wchar_t* label, float x, float y, bool rightAlign) {
        const float w = lab.r->Width(label, lab.scale);
        lab.r->Draw(label, Snap(rightAlign ? x - w : x), Snap(y), lab.scale,
                    0.64f, 0.76f, 0.88f, 0.88f * ge);
    };
    auto drawValue = [&](const TextPick& p, const wchar_t* text, float x, float y,
                         bool rightAlign, float rr, float gg, float bb) {
        const float w = p.r->Width(text, p.scale);
        p.r->Draw(text, Snap(rightAlign ? x - w : x), Snap(y), p.scale,
                  rr, gg, bb, 0.94f * ge);
    };

    // ---- Journey: the wave track, played forward ---------------------------
    // The same seven-node track as the in-run HUD. Waves 1 to 3 go by at a
    // steady pace; from wave 3 on the current wave steps up faster and faster
    // to the wave the run ended on, then bursts and the whole line shakes.
    const int finalWave = std::max(1, FinalWaveCount(g_Difficulty));
    const int waveAtDeath = std::clamp(g_GameManager.currentWave, 1, finalWave);
    static float s_journeyT = 0.0f;
    static int s_curWave = 0;
    static float s_changeT = 0.0f;
    static bool s_burstSounded = false;
    if (gof < 0.05f) { s_journeyT = 0.0f; s_curWave = 0; s_changeT = 0.0f; s_burstSounded = false; }
    constexpr float kStepSlow = 0.40f;                   // seconds per wave while steady
    const float v0 = 1.0f / kStepSlow;                   // waves per second while steady
    const float steadyTime = ((float)std::min(waveAtDeath, 3) - 1.0f) * kStepSlow;
    const float fastWaves = (float)std::max(0, waveAtDeath - 3);
    float fastTime = fastWaves > 0.0f ? 0.6f + 0.07f * fastWaves : 0.0f;
    float accel = fastWaves > 0.0f
        ? 2.0f * (fastWaves - v0 * fastTime) / (fastTime * fastTime) : 0.0f;
    if (accel < 0.0f) { accel = 0.0f; fastTime = fastWaves / v0; }   // too few waves to speed up
    const float journeyTotal = steadyTime + fastTime;
    const bool journeyStarted = gof >= 0.40f || skippedCinematic;
    if (skippedCinematic) {
        s_journeyT = journeyTotal + 2.0f;
        s_burstSounded = true;
    } else if (journeyStarted) {
        s_journeyT += delta;
    }
    // Position along the track, in waves: steady up to wave 3, then it
    // accelerates (constant acceleration from the steady pace).
    float wp = 1.0f + std::min(s_journeyT, steadyTime) * v0;
    if (s_journeyT > steadyTime) {
        const float s = std::min(s_journeyT - steadyTime, fastTime);
        wp = 3.0f + v0 * s + 0.5f * accel * s * s;
    }
    wp = std::min(wp, (float)waveAtDeath);
    const int curWave = std::clamp((int)floorf(wp + 0.5f), 1, waveAtDeath);
    if (curWave != s_curWave) {
        if (s_curWave != 0 && !skippedCinematic) Audio::PlaySfx(Audio::Sfx::Shoot);
        s_curWave = curWave;
        s_changeT = s_journeyT;
    }
    const bool arrived = journeyStarted && s_journeyT >= journeyTotal;
    const float burstT = arrived ? s_journeyT - journeyTotal : -1.0f;
    if (arrived && !s_burstSounded) {
        if (!state.victory) Audio::PlaySfx(Audio::Sfx::Kill);
        s_burstSounded = true;
    }
    float shakeX = 0.0f, shakeY = 0.0f;
    if (burstT >= 0.0f && burstT < 0.9f && !state.victory) {
        const float k = 1.0f - burstT / 0.9f;
        const float amp = k * k * 7.0f * uiS;
        shakeX = sinf(burstT * 71.0f) * amp;
        shakeY = cosf(burstT * 97.0f) * amp * 0.7f;
    }

    wchar_t waveBuf[16], finalBuf[24], levelBuf[16], gainBuf[48];
    swprintf_s(waveBuf, L"%d", curWave);
    swprintf_s(finalBuf, L"/ %d", finalWave);
    swprintf_s(levelBuf, L"%d", state.playerLevel);
    FormatGrouped(std::max(0LL, g_RunStardust), gainBuf, 48);

    // Hero: the wave counter, with the final wave beside it.
    const bool heroHot = arrived && !state.victory;
    drawLabel(kWaveLabel[li], leftX, top + heroLabY, false);
    drawValue(hero, waveBuf, leftX + shakeX, top + heroY + shakeY, false,
              1.0f, heroHot ? 0.58f : 0.88f, heroHot ? 0.50f : 0.34f);
    const float heroW = hero.r->Width(L"00", hero.scale);
    const float finalW = val.r->Width(L"/ 00", val.scale);
    drawValue(val, finalBuf, leftX + heroW + 12.0f * uiS,
              top + heroY + heroH - valH, false, 0.64f, 0.76f, 0.88f);

    {
        const float trackX0 = leftX + heroW + 12.0f * uiS + finalW + 36.0f * uiS;
        const float availW = std::max(60.0f, leftX + leftW - trackX0);
        const float hsT = std::clamp(availW / 216.0f, 0.5f, uiS * 1.3f);
        const float tcx = trackX0 + availW * 0.5f + shakeX;
        const float tcy = top + heroY + heroH * 0.62f + shakeY;
        const float slide = (float)curWave - wp;
        // The current node fills clockwise as the track passes through it; on
        // the last wave it settles at how far into the wave the run got.
        float fillShare = std::clamp(wp - (float)curWave + 0.5f, 0.0f, 1.0f);
        if (arrived) {
            // Waves have no clock: a fight that ended mid-wave shows half.
            const float share = 0.5f;
            fillShare = 0.5f + (share - 0.5f) * std::clamp(burstT / 0.4f, 0.0f, 1.0f);
        }
        const float pulse = std::clamp((s_journeyT - s_changeT) / 0.30f, 0.0f, 1.0f);
        DrawWaveTrack(tcx, tcy, hsT, curWave, finalWave, g_Difficulty,
                                1.0f - fillShare, pulse, slide, ge, true,
                                arrived && !state.victory);
        BindMainShader();
        // One soft ripple where the run ended.
        if (burstT >= 0.0f && burstT < 0.8f) {
            const float p = burstT / 0.8f;
            const float fade = (1.0f - p) * (1.0f - p);
            const float ringR = (11.0f + 24.0f * (1.0f - (1.0f - p) * (1.0f - p))) * hsT;
            DrawSettingsOrbitArc(tcx, tcy, ringR, ringR, 0.0f, 6.2832f,
                                 1.0f, state.victory ? 0.84f : 0.46f,
                                 state.victory ? 0.30f : 0.40f,
                                 1.3f * hsT, 0.45f * fade * ge);
        }    }
    LogoLine(leftX, top + rule2Y, leftX + leftW, top + rule2Y,
             1.0f, accentR, accentG, accentB, 0.16f * ge);

    // Level | stardust gained.
    {
        const float lvW = std::max(
            MaxLocalizedTextWidth(*lab.r, kLevelLabel, LANG_COUNT, lab.scale),
            val.r->Width(L"999", val.scale));
        const float col1X = leftX + lvW + 56.0f * uiS;
        drawLabel(kLevelLabel[li], leftX, top + rowLabY, false);
        drawValue(val, levelBuf, leftX, top + rowValY, false, 0.68f, 0.92f, 1.0f);
        drawLabel(kStardustLabel[li], col1X, top + rowLabY, false);
        drawValue(val, gainBuf, col1X, top + rowValY, false, 0.72f, 0.88f, 1.0f);
    }

    // ---- Right panel: the player and stats ------------------------------------
    float panelW = std::clamp(sw * 0.30f, 300.0f, 520.0f);
    float panelX = sw - margin - panelW;
    const float panelMinX = leftX + leftW + 48.0f * uiS;
    if (panelX < panelMinX) {
        panelX = panelMinX;
        panelW = sw - margin - panelX;
    }
    if (panelW >= 240.0f) {
        static bool s_statsOpen = false;
        static float s_statsT = 0.0f;
        static bool s_rowsValid = false;
        static StatRow s_rows[kMetricCount];
        static int s_descAug = -1;
        static float s_descT = 0.0f;
        if (gof < 0.05f) {
            s_statsOpen = false; s_statsT = 0.0f; s_rowsValid = false;
            s_descAug = -1; s_descT = 0.0f;
        }
        // Cards taken this run (the level-up stat cards are left out).
        const std::vector<int>& taken = passive::AcquiredOrder();
        const int nTaken = (int)taken.size();
        constexpr int kDescLines = 3;
        if (!s_rowsValid) { BuildStatRows(s_rows); s_rowsValid = true; }

        const bool statRowShown[kMetricCount] = { true, true, true, true, true, true };
        int statRows = 0;
        for (int m = 0; m < kMetricCount; ++m) if (statRowShown[m]) ++statRows;

        // The panel is a vertical stack; when it would run off the bottom the
        // graphic parts (player, cells, gaps) shrink, text stays at its size.
        // The panel is a vertical stack; when it would run off the bottom the
        // graphic parts (player, cells) shrink, text stays at its size.
        float k = 1.0f;
        float shellSize = 0, shellR = 0, shellCy = 0;
        float augLabY = 0, augRuleY = 0, gridY = 0, cell = 0;
        int cols = 1, shown = 0;
        float toggleY = 0, toggleRuleY = 0, tableY0 = 0, rowH = 0;
        for (int pass = 0; pass < 12; ++pass) {
            shellSize = 25.0f * 2.2f * uiS * k;
            shellR = shellSize * 1.2f;
            shellCy = top + shellR;
            // Below the outer orbit arc, so the label never sits on a ring.
            augLabY = shellCy + shellR * 1.30f + 10.0f * uiS * k;
            augRuleY = augLabY + labH + 8.0f * uiS * k;
            gridY = augRuleY + 10.0f * uiS * k;
            cell = 64.0f * uiS * k;
            cols = std::max(1, (int)(panelW / cell));
            const int maxCells = cols * 3;
            shown = nTaken > maxCells ? maxCells - 1 : nTaken;
            const int cells = std::max(1, std::min(nTaken, maxCells));
            const float gridH = (float)((cells + cols - 1) / cols) * cell;
            toggleY = gridY + gridH + 12.0f * uiS * k;
            toggleRuleY = toggleY + labH + 8.0f * uiS * k;
            tableY0 = toggleRuleY + 12.0f * uiS * k;
            rowH = labH + 6.0f * uiS * k;
            const float bottom = tableY0 + (float)statRows * rowH + 14.0f * uiS * k +
                                 labH * 1.3f + (float)kDescLines * (labH + 3.0f * uiS * k);
            if (bottom <= sh - 24.0f || k <= 0.50f) break;
            k -= 0.04f;
        }

        // The player's own look, with two slow orbit arcs behind it.
        const float shellCx = panelX + panelW * 0.5f;
        BindMainShader();
        DrawSettingsOrbitArc(shellCx, shellCy, shellR, shellR,
                             now * 0.35f, now * 0.35f + 3.6f,
                             accentR, accentG, accentB, 1.4f * uiS, 0.35f * ge);
        DrawSettingsOrbitArc(shellCx, shellCy, shellR * 1.16f, shellR * 1.16f,
                             -now * 0.22f + 2.0f, -now * 0.22f + 4.6f,
                             accentR, accentG, accentB, 1.0f * uiS, 0.22f * ge);
        DrawPlayerWeaponShell(shellCx, shellCy, shellSize, now * 0.6f, now);
        BatchFlush();

        // Cards taken this run, as their family constellations in the
        // rarity colour; hovering one explains it below the stats.
        static const wchar_t* kTakenLabel[LANG_COUNT] = {
            L"\uC5BB\uC740 \uC99D\uAC15", L"AUGMENTS TAKEN", L"\u7372\u5F97\u3057\u305F\u5F37\u5316" };
        static const wchar_t* kNoneLabel[LANG_COUNT] = {
            L"\uC5C6\uC74C", L"NONE", L"\u306A\u3057" };
        lab.r->Draw(kTakenLabel[li], Snap(panelX), Snap(augLabY), lab.scale,
                    accentR, accentG, accentB, 0.92f * ge);
        LogoLine(panelX, augRuleY, panelX + panelW, augRuleY,
                 1.0f, accentR, accentG, accentB, 0.24f * ge);
        const bool canHoverCards = gof >= 0.9f && !skippedCinematic;
        int hoverAug = -1;
        if (nTaken == 0) {
            lab.r->Draw(kNoneLabel[li], Snap(panelX), Snap(gridY + cell * 0.5f - labH * 0.5f),
                        lab.scale, 0.64f, 0.76f, 0.88f, 0.70f * ge);
        }
        BindMainShader();
        for (int i = 0; i < shown; ++i) {
            const int idx = taken[i];
            const AugDef& def = ALL_AUGS[idx];
            const float ccx = panelX + ((float)(i % cols) + 0.5f) * cell;
            const float ccy = gridY + ((float)(i / cols) + 0.5f) * cell;
            const float dx = (float)mx - ccx, dy = (float)my - ccy;
            const bool hov = canHoverCards && std::fabs(dx) <= cell * 0.5f && std::fabs(dy) <= cell * 0.5f;
            if (hov) hoverAug = idx;
            float rr, rg, rb;
            GetRarityColor(def.rarity, rr, rg, rb);
            rr = std::min(1.0f, rr * 1.3f + 0.15f);
            rg = std::min(1.0f, rg * 1.3f + 0.15f);
            rb = std::min(1.0f, rb * 1.3f + 0.15f);
            const aug_card::Shape& shp =
                aug_card::ShapeOf(aug_card::FamilyFromCode(def.name));
            const float size = cell * (hov ? 0.78f : 0.70f);
            const float a = (hov ? 1.0f : 0.82f) * ge;
            auto P = [&](int v, float& x, float& y) {
                x = ccx + (shp.x[v] - 0.5f) * size;
                y = ccy + (shp.y[v] - 0.5f) * size;
            };
            for (int e = 0; e < shp.ne; ++e) {
                float x0, y0, x1, y1;
                P(shp.ea[e], x0, y0);
                P(shp.eb[e], x1, y1);
                LogoLine(x0, y0, x1, y1, (hov ? 1.6f : 1.2f) * uiS, rr, rg, rb, a);
            }
            for (int v = 0; v < shp.nv; ++v) {
                float x, y;
                P(v, x, y);
                drawDiamond(x, y, 2.6f * uiS, rr, rg, rb, a);
            }
            // Actives get a small ring under the shape so they read apart.
            if (AugIsActive(def))
                DrawSettingsOrbitArc(ccx, ccy, cell * 0.46f, cell * 0.46f, 0.0f, 6.2832f,
                                     rr, rg, rb, 1.0f * uiS, 0.45f * ge);
        }
        BatchFlush();
        for (int i = 0; i < shown; ++i) {
            const int n = passive::AcquiredCount(taken[i]);
            if (n <= 1) continue;
            wchar_t cb[16];
            swprintf_s(cb, L"x%d", n);
            const float ccx = panelX + ((float)(i % cols) + 0.5f) * cell;
            const float ccy = gridY + ((float)(i / cols) + 0.5f) * cell;
            const float cw = lab.r->Width(cb, lab.scale * 0.65f);
            lab.r->Draw(cb, Snap(ccx + cell * 0.5f - cw), Snap(ccy + cell * 0.5f - labH * 0.65f),
                        lab.scale * 0.65f, 1.0f, 1.0f, 1.0f, 0.95f * ge);
        }
        if (nTaken > shown) {
            wchar_t more[16];
            swprintf_s(more, L"+%d", nTaken - shown);
            const float ccx = panelX + ((float)(shown % cols) + 0.5f) * cell;
            const float ccy = gridY + ((float)(shown / cols) + 0.5f) * cell;
            const float mw = lab.r->Width(more, lab.scale);
            lab.r->Draw(more, Snap(ccx - mw * 0.5f), Snap(ccy - labH * 0.5f), lab.scale,
                        0.64f, 0.76f, 0.88f, 0.90f * ge);
        }

        // Fold-out toggle: a label, a rule and a chevron.
        const bool canHover = gof >= 0.9f && !skippedCinematic;
        const bool toggleHover = canHover &&
            mx >= panelX && mx <= panelX + panelW &&
            my >= toggleY - 6.0f * uiS && my <= toggleRuleY + 6.0f * uiS;
        if (toggleHover && lmb && !state.previousLeftMouseDown)
            s_statsOpen = !s_statsOpen;
        const float target = s_statsOpen ? 1.0f : 0.0f;
        s_statsT += (target - s_statsT) * (1.0f - std::exp(-delta * 10.0f));
        if (fabsf(target - s_statsT) < 0.002f) s_statsT = target;
        const float openE = Smoothstep(s_statsT);
        const float tA = (toggleHover ? 1.0f : 0.82f) * ge;
        lab.r->Draw(kBuildStatsLabel[li], Snap(panelX), Snap(toggleY), lab.scale,
                    accentR, accentG, accentB, tA);
        const float chevX = panelX + panelW - 12.0f * uiS;
        const float chevY = toggleY + labH * 0.5f;
        const float arm = 7.0f * uiS;
        const float dir = 1.0f - 2.0f * openE;   // 1 = pointing down, -1 = up
        BindMainShader();
        LogoLine(chevX - arm, chevY - arm * 0.5f * dir, chevX, chevY + arm * 0.5f * dir,
                 1.5f * uiS, accentR, accentG, accentB, tA);
        LogoLine(chevX + arm, chevY - arm * 0.5f * dir, chevX, chevY + arm * 0.5f * dir,
                 1.5f * uiS, accentR, accentG, accentB, tA);
        LogoLine(panelX, toggleRuleY, panelX + panelW, toggleRuleY,
                 1.0f, accentR, accentG, accentB, (toggleHover ? 0.40f : 0.24f) * ge);

        // Stat rows: total (white) = base (white) + trial (yellow). Every part sits in its own left-aligned column so the
        // rows line up under each other.
        if (openE > 0.01f) {
            const float tableY = tableY0 + (1.0f - openE) * -10.0f * uiS;
            const float a = openE * ge;
            const float wr = 0.95f, wg = 0.97f, wb = 1.0f;       // total, base
            const float yr = 1.0f, yg = 0.84f, yb = 0.30f;       // trial
            const float dr = 0.55f, dg = 0.65f, db = 0.78f;      // operators
            TextPick rp = lab;
            // A value in one colour, its % sign included.
            auto drawValueText = [&](const wchar_t* s, float x, float y,
                                     float cr, float cg, float cb, float al) {
                rp.r->Draw(s, Snap(x), Snap(y), rp.scale, cr, cg, cb, al);
            };            float opW = 0.0f, eqW = 0.0f;
            float labelColW = 0.0f, totalColW = 0.0f, baseColW = 0.0f;
            // Parts follow the base one after another with the same gap between
            // any two, positives first and nerfs last, whatever kind they are.
            float xPart0 = 0.0f;
            const float partGap = 12.0f * uiS;
            float xTotal = 0.0f, xEq = 0.0f, xBase = 0.0f;
            const float opGap = 5.0f * uiS;
            auto partUsed = [&](int m, float v) { return fabsf(v) >= MetricEps(m); };
            // The row's parts in display order: run gains, then the trial cut.
            auto rowParts = [&](const StatRow& row, int m, float* vals) {
                int n = 0;
                if (partUsed(m, row.run))   vals[n++] = row.run;
                if (partUsed(m, row.trial)) vals[n++] = row.trial;
                return n;
            };
            auto partWidth = [&](int m, float v) {
                wchar_t pb[32];
                FormatMetric(pb, m, fabsf(v));
                return opW + opGap + rp.r->Width(pb, rp.scale);
            };
            auto drawPart = [&](int m, float v, float x, float y, float cr, float cg, float cb) {
                if (!partUsed(m, v)) return;
                // A nerf (a negative part) has a red value, whichever part it is;
                // the + and - signs share the operator colour.
                const bool nerf = v < 0.0f;
                const wchar_t opText[2] = { nerf ? L'-' : L'+', 0 };
                if (nerf) { cr = 1.0f; cg = 0.36f; cb = 0.38f; }
                rp.r->Draw(opText, Snap(x), Snap(y), rp.scale, dr, dg, db, 0.9f * a);
                wchar_t pb[32];
                FormatMetric(pb, m, fabsf(v));
                drawValueText(pb, x + opW + opGap, y, cr, cg, cb, 0.95f * a);
            };
            // Shrink the text a little until the widest row fits the panel.
            float rowFit = 1.0f;
            for (int pass = 0; pass < 10; ++pass) {
                rp = PickText(state.text, 26.0f * uiS * rowFit);
                opW = std::max(rp.r->Width(L"+", rp.scale), rp.r->Width(L"-", rp.scale));
                eqW = rp.r->Width(L"=", rp.scale);
                labelColW = totalColW = baseColW = 0.0f;
                for (int m = 0; m < kMetricCount; ++m) {
                    if (!statRowShown[m]) continue;
                    const StatRow& row = s_rows[m];
                    labelColW = std::max(labelColW, rp.r->Width(kMetricLabel[li][m], rp.scale));
                    wchar_t fb[32];
                    FormatMetric(fb, m, row.total);
                    totalColW = std::max(totalColW, rp.r->Width(fb, rp.scale));
                    FormatMetric(fb, m, row.base);
                    baseColW = std::max(baseColW, rp.r->Width(fb, rp.scale));
                }
                xTotal = panelX + labelColW + 14.0f * uiS;
                xEq = xTotal + totalColW + 10.0f * uiS;
                xBase = xEq + eqW + 8.0f * uiS;
                xPart0 = xBase + baseColW + 12.0f * uiS;
                float partsEnd = xBase + baseColW;
                for (int m = 0; m < kMetricCount; ++m) {
                    if (!statRowShown[m]) continue;
                    float pv[2];
                    const int pn = rowParts(s_rows[m], m, pv);
                    float ex = xPart0;
                    for (int i = 0; i < pn; ++i) ex += partWidth(m, pv[i]) + partGap;
                    if (pn > 0) partsEnd = std::max(partsEnd, ex - partGap);
                }
                if (partsEnd - panelX <= panelW || rowFit <= 0.62f) break;
                rowFit -= 0.05f;
            }
            int shownRows = 0;
            for (int m = 0; m < kMetricCount; ++m) {
                if (!statRowShown[m]) continue;
                const StatRow& row = s_rows[m];
                const float y = tableY + (float)shownRows * rowH;
                ++shownRows;
                rp.r->Draw(kMetricLabel[li][m], Snap(panelX), Snap(y), rp.scale,
                           0.64f, 0.76f, 0.88f, 0.88f * a);
                wchar_t fb[32];
                FormatMetric(fb, m, row.total);
                drawValueText(fb, xTotal, y, wr, wg, wb, 0.98f * a);
                rp.r->Draw(L"=", Snap(xEq), Snap(y), rp.scale, dr, dg, db, 0.9f * a);
                FormatMetric(fb, m, row.base);
                drawValueText(fb, xBase, y, wr, wg, wb, 0.95f * a);
                float pv[2];
                const int pn = rowParts(row, m, pv);
                float px = xPart0;
                for (int i = 0; i < pn; ++i) {
                    drawPart(m, pv[i], px, y, yr, yg, yb);
                    px += partWidth(m, pv[i]) + partGap;
                }
            }
        }
        // Hovering a card explains it right below the stats.
        const float descY = tableY0 + Smoothstep(s_statsT) * (float)statRows * rowH +
                            14.0f * uiS * k;
        if (hoverAug >= 0) s_descAug = hoverAug;
        s_descT += ((hoverAug >= 0 ? 1.0f : 0.0f) - s_descT) * std::min(1.0f, delta * 12.0f);
        if (s_descAug >= 0 && s_descT > 0.01f) {
            const AugDef& def = ALL_AUGS[s_descAug];
            float rr, rg, rb;
            GetRarityColor(def.rarity, rr, rg, rb);
            const float a = s_descT * ge;
            LogoLine(panelX, descY - 8.0f * uiS * k, panelX + panelW, descY - 8.0f * uiS * k,
                     1.0f, rr, rg, rb, 0.45f * a);
            lab.r->Draw(def.locName[li], Snap(panelX), Snap(descY), lab.scale,
                        0.45f + 0.55f * rr, 0.45f + 0.55f * rg, 0.45f + 0.55f * rb, 0.98f * a);
            wchar_t lines[kDescLines][kWrapChars];
            const int n = WrapLines(*lab.r, lab.scale, def.locDesc[li], panelW, lines, kDescLines);
            float ly = descY + labH * 1.3f;
            for (int l = 0; l < n; ++l) {
                lab.r->Draw(lines[l], Snap(panelX), Snap(ly), lab.scale,
                            0.82f, 0.88f, 0.96f, 0.92f * a);
                ly += labH + 3.0f * uiS * k;
            }
        }
    }

    // ---- Commands ----------------------------------------------------------
    const float bX0 = leftX;
    const float bY0 = Snap(buttonsAbsY);
    const bool showButtons = gof >= 0.999f && !skippedCinematic;

    if (showButtons) {
        const float ancX = bX0 - 36.0f;
        const float ancY = bY0 - 8.0f;
        const float ancH = totalBH + 16.0f;
        BindMainShader();
        drawRect(ancX, ancY, 1.2f, ancH, accentR, accentG, accentB,
                 0.15f * ge);
        drawDiamond(ancX + 0.6f, ancY + 4.0f, 2.8f,
                    accentR, accentG, accentB, 0.20f * ge);
        drawDiamond(ancX + 0.6f, ancY + ancH - 4.0f, 2.8f,
                    accentR, accentG, accentB, 0.16f * ge);
    }
    for (int i = 0; i < kGameOverActionCount; ++i) {
        const float by = bY0 + (float)i * (BH + BGAP);
        const bool hov = showButtons && s_confirmGO < 0 &&
            mx >= bX0 - 26.0f && mx <= bX0 + BW + 32.0f &&
            my >= by && my <= by + BH;
        s_hovGO[i] = UpdateMenuCommandHover(s_hovGO[i], hov, delta);
        const float rowA = (showButtons ? 1.0f : 0.0f) * ge;
        const wchar_t* routeVariants[LANG_COUNT] = {};
        for (int language = 0; language < LANG_COUNT; ++language)
            routeVariants[language] = kRoute[language][i];
        DrawUnifiedMenuCommand(kRoute[li][i],
                               bX0 - 10.0f * s_hovGO[i], by, BW, BH,
                               accentR, accentG, accentB, rowA,
                               s_hovGO[i], false, 0.0f,
                               now + (float)i * 0.17f,
                               uiS, false, true, false,
                               routeVariants, LANG_COUNT);
        if (hov && lmb && !state.previousLeftMouseDown) {
            const GameOverAction action = static_cast<GameOverAction>(i);
            if (action == GameOverAction::ReturnToMenu)
                ApplyGameOverAction(action, window, c, state);
            else
                s_confirmGO = i;   // restart and exit ask first
        }
    }

    if (s_confirmGO >= 0) {
        const GameOverAction pending = static_cast<GameOverAction>(s_confirmGO);
        const ConfirmText& prompt = pending == GameOverAction::Quit
            ? ConfirmTextQuit() : ConfirmTextRestart();
        const ConfirmChoice answer = DrawConfirmDialog(
            true, prompt, sw, sh, mx, my, lmb, state.previousLeftMouseDown, delta);
        if (answer != ConfirmChoice::None) {
            s_confirmGO = -1;
            DrawConfirmDialog(false, prompt, sw, sh, mx, my, lmb,
                              state.previousLeftMouseDown, delta);
            if (answer == ConfirmChoice::Yes)
                ApplyGameOverAction(pending, window, c, state);
        }
    }
}
