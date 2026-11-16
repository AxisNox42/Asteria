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
#include "RunBuildPanel.h"
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

using namespace run_build;   // PickText, Snap, WrapLines

namespace {
enum class GameOverAction : int { Restart, ReturnToMenu, Quit };
constexpr int kGameOverActionCount = static_cast<int>(GameOverAction::Quit) + 1;

void ApplyGameOverAction(GameOverAction action, GLFWwindow* window,
                         const SceneCtx& scene,
                         const GameOverSceneContext& state) {
    // Leaving the first result that earned stardust: the lobby points at SHOP next.
    if (g_ShopIntroStage == SHOP_INTRO_RESULT) {
        g_ShopIntroStage = SHOP_INTRO_LOBBY;
        SaveGame();
    }
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
    // First play that earned stardust (the shop just opened): STARDUST GAINED
    // lights up once the journey lands, with a line pointing to the lobby shop.
    const bool dustIntro = g_ShopIntroStage == SHOP_INTRO_RESULT && g_RunStardust > 0;
    static const wchar_t* kDustHint[LANG_COUNT] = {
        L"처음 모은 별가루입니다 · 로비에 상점이 열렸어요",
        L"Your first stardust · the SHOP is now open in the lobby",
        L"初めての星屑 · ロビーにショップが開きました"
    };
    const TextPick hintT = PickText(state.text, 24.0f * uiS);
    const float hintGap = 12.0f * uiS;
    const float hintH = dustIntro ? hintT.r->Height(kDustHint[li], hintT.scale) + hintGap : 0.0f;
    const float statsBottom = rowValY + valH + hintH;

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
        if (s_curWave != 0 && !skippedCinematic) Audio::Play(Audio::Cue::ResultWaveTick);
        s_curWave = curWave;
        s_changeT = s_journeyT;
    }
    const bool arrived = journeyStarted && s_journeyT >= journeyTotal;
    const float burstT = arrived ? s_journeyT - journeyTotal : -1.0f;
    if (arrived && !s_burstSounded) {
        Audio::Play(state.victory ? Audio::Cue::ResultFinalVictory : Audio::Cue::ResultFinalDefeat);
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
        // Highlight clock: starts a beat after the journey lands.
        static float s_dustT = -1.0f;
        if (gof < 0.05f) s_dustT = -1.0f;
        float hi = 0.0f;       // 0..1 settled highlight
        float flare = 0.0f;    // the brief flare on top
        if (dustIntro && arrived) {
            const float prevT = s_dustT;
            s_dustT = burstT - 0.55f;
            if (prevT < 0.0f && s_dustT >= 0.0f) Audio::Play(Audio::Cue::ShopUnlock);
            if (s_dustT > 0.0f) {
                hi = Smoothstep(std::min(1.0f, s_dustT / 0.45f));
                // Two soft pulses over ~2.4 s, then it rests at the settled gold.
                if (s_dustT < 2.4f) {
                    const float k = 1.0f - s_dustT / 2.4f;
                    flare = k * (0.5f + 0.5f * sinf(s_dustT * 5.2f - 1.57f));
                }
            }
        }
        if (hi <= 0.0f) {
            drawLabel(kStardustLabel[li], col1X, top + rowLabY, false);
            drawValue(val, gainBuf, col1X, top + rowValY, false, 0.72f, 0.88f, 1.0f);
        } else {
            // Stardust gold, brightening toward white on each pulse.
            const float gr = 1.0f, gg = 0.84f + 0.14f * flare, gb = 0.34f + 0.50f * flare;
            const float labA = (0.88f + 0.12f * hi) * ge;
            lab.r->Draw(kStardustLabel[li], Snap(col1X), Snap(top + rowLabY), lab.scale,
                        0.64f + (gr - 0.64f) * hi, 0.76f + (gg - 0.76f) * hi,
                        0.88f + (gb - 0.88f) * hi, labA);
            const float vw = val.r->Width(gainBuf, val.scale);
            const float ox = flare * 1.5f * uiS;   // a hair of lift on the pulse
            val.r->Draw(gainBuf, Snap(col1X), Snap(top + rowValY - ox), val.scale,
                        0.72f + (gr - 0.72f) * hi, 0.88f + (gg - 0.88f) * hi,
                        1.0f + (gb - 1.0f) * hi, 0.94f * ge);
            BindMainShader();
            // Underline that draws out from the value, plus a diamond at its end.
            const float uy = top + rowValY + valH + 4.0f * uiS;
            const float lw = std::max(vw, lab.r->Width(kStardustLabel[li], lab.scale));
            const float ux1 = col1X + lw * hi;
            LogoLine(col1X, uy, ux1, uy, (1.2f + 1.4f * flare) * uiS,
                     1.0f, 0.84f, 0.34f, (0.55f + 0.40f * flare) * ge);
            drawDiamond(ux1 + 6.0f * uiS, uy, (4.0f + 3.0f * flare) * uiS,
                        1.0f, 0.86f, 0.40f, 0.85f * hi * ge);
            // Pointer line to the lobby shop.
            const float hintA = Smoothstep(std::clamp((s_dustT - 0.25f) / 0.45f, 0.0f, 1.0f));
            if (hintA > 0.001f) {
                DrawShadowedText(*hintT.r, kDustHint[li], Snap(leftX),
                                 Snap(top + rowValY + valH + hintGap), hintT.scale,
                                 1.0f, 0.88f, 0.48f, 0.92f * hintA * ge, 0.80f);
            }
        }
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
        static run_build::PanelState s_panel;
        if (gof < 0.05f) s_panel.Reset();
        run_build::PanelIn pin;
        pin.text = &state.text;
        pin.panelX = panelX; pin.panelW = panelW; pin.top = top;
        pin.bottomLimit = sh - 24.0f;
        pin.uiS = uiS; pin.alpha = ge; pin.delta = delta; pin.now = now;
        pin.mx = mx; pin.my = my;
        pin.click = lmb && !state.previousLeftMouseDown;
        pin.canHover = gof >= 0.9f && !skippedCinematic;
        pin.accentR = accentR; pin.accentG = accentG; pin.accentB = accentB;
        pin.labH = labH;
        run_build::DrawPanel(s_panel, pin);
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
    // Shop guide (first stardust): MAIN MENU leads to the shop, so it gets the
    // gold brackets and a line; RESTART and EXIT are faint and locked.
    static float s_goGuideT = 0.0f;
    static int s_guideAskGO = -1;   // RESTART/EXIT pressed during the guide: ask to end it
    if (gof < 0.05f) { s_goGuideT = 0.0f; s_guideAskGO = -1; }
    if (dustIntro && showButtons) s_goGuideT += delta;
    const float guideA = dustIntro ? Smoothstep(std::clamp((s_goGuideT - 0.6f) / 0.45f, 0.0f, 1.0f)) : 0.0f;
    static float s_goDim = 0.0f;
    s_goDim = UiApproach(s_goDim, dustIntro && showButtons ? 1.0f : 0.0f, delta, 6.0f);
    for (int i = 0; i < kGameOverActionCount; ++i) {
        const float by = bY0 + (float)i * (BH + BGAP);
        const bool isMenu = static_cast<GameOverAction>(i) == GameOverAction::ReturnToMenu;
        const bool guideLock = dustIntro && !isMenu;
        const bool hov = showButtons && s_confirmGO < 0 && s_guideAskGO < 0 &&
            mx >= bX0 - 26.0f && mx <= bX0 + BW + 32.0f &&
            my >= by && my <= by + BH;
        s_hovGO[i] = UpdateMenuCommandHover(s_hovGO[i], hov, delta);
        const float rowA = (showButtons ? 1.0f : 0.0f) * ge * (guideLock ? 1.0f - 0.70f * s_goDim : 1.0f);
        float hovT = s_hovGO[i];
        if (isMenu && dustIntro)   // MAIN MENU breathes at a light hover
            hovT = std::max(hovT, (0.30f + 0.22f * (0.5f + 0.5f * sinf(now * 3.2f))) * guideA);
        const wchar_t* routeVariants[LANG_COUNT] = {};
        for (int language = 0; language < LANG_COUNT; ++language)
            routeVariants[language] = kRoute[language][i];
        DrawUnifiedMenuCommand(kRoute[li][i],
                               bX0 - 10.0f * hovT, by, BW, BH,
                               accentR, accentG, accentB, rowA,
                               hovT, false, 0.0f,
                               now + (float)i * 0.17f,
                               uiS, false, true, false,
                               routeVariants, LANG_COUNT);
        if (isMenu && guideA > 0.001f) {
            // Gold corner brackets (the shop guide's mark) and the pointer line.
            static const wchar_t* kGoHint[LANG_COUNT] = {
                L"메인 메뉴로 나가 상점을 열어 보세요",
                L"Go to MAIN MENU and open the SHOP",
                L"メインメニューに戻ってショップを開いてみよう"
            };
            const float ga = (0.70f + 0.30f * (0.5f + 0.5f * sinf(now * 3.0f))) * guideA * ge;
            const float grow = (1.0f - guideA) * 14.0f * uiS;
            const float rx = bX0 - 22.0f * uiS - grow, ry = by - 8.0f * uiS - grow;
            const float rw = BW + 34.0f * uiS + grow * 2.0f, rh = BH + 16.0f * uiS + grow * 2.0f;
            const float arm = std::min(28.0f * uiS, rh * 0.30f);
            const float th = 1.8f * uiS;
            BindMainShader();
            for (int ix = 0; ix < 2; ++ix)
                for (int iy = 0; iy < 2; ++iy) {
                    const float cx0 = ix ? rx + rw : rx, cy0 = iy ? ry + rh : ry;
                    const float sx = ix ? -1.0f : 1.0f, sy = iy ? -1.0f : 1.0f;
                    LogoLine(cx0, cy0, cx0 + sx * arm, cy0, th * 3.2f, 1.0f, 0.84f, 0.34f, 0.14f * ga);
                    LogoLine(cx0, cy0, cx0, cy0 + sy * arm, th * 3.2f, 1.0f, 0.84f, 0.34f, 0.14f * ga);
                    LogoLine(cx0 - sx * th * 0.5f, cy0, cx0 + sx * arm, cy0, th, 1.0f, 0.86f, 0.40f, ga);
                    LogoLine(cx0, cy0 - sy * th * 0.5f, cx0, cy0 + sy * arm, th, 1.0f, 0.86f, 0.40f, ga);
                }
            const float hy = by + BH * 0.5f;
            const float hx = rx + rw + 22.0f * uiS;
            drawDiamond(hx, hy, 5.0f * uiS, 1.0f, 0.86f, 0.40f, 0.90f * ga);
            const float hh = hintT.r->Height(kGoHint[li], hintT.scale);
            DrawShadowedText(*hintT.r, kGoHint[li], Snap(hx + 14.0f * uiS), Snap(hy - hh * 0.5f),
                             hintT.scale, 1.0f, 0.88f, 0.48f, 0.95f * guideA * ge, 0.80f);
        }
        if (hov && lmb && !state.previousLeftMouseDown && guideLock) {
            s_guideAskGO = i;   // ask to end the shop guide first
        } else if (hov && lmb && !state.previousLeftMouseDown) {
            const GameOverAction action = static_cast<GameOverAction>(i);
            if (action == GameOverAction::ReturnToMenu)
                ApplyGameOverAction(action, window, c, state);
            else
                s_confirmGO = i;   // restart and exit ask first
        }
    }

    if (s_guideAskGO >= 0) {
        const ConfirmChoice answer = DrawConfirmDialog(
            true, ConfirmTextEndShopGuide(), sw, sh, mx, my, lmb, state.previousLeftMouseDown, delta);
        if (answer != ConfirmChoice::None) {
            const int pressed = s_guideAskGO;
            s_guideAskGO = -1;
            DrawConfirmDialog(false, ConfirmTextEndShopGuide(), sw, sh, mx, my, lmb,
                              state.previousLeftMouseDown, delta);
            if (answer == ConfirmChoice::Yes) {
                g_ShopIntroStage = SHOP_INTRO_NONE;   // guide over; the button then asks as usual
                SaveGame();
                s_confirmGO = pressed;
            }
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
