#include "Scenes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "GameContext.h"
#include "SceneUI.h"
#include "UiLayout.h"
#include "WindowChrome.h"
#include "Settings.h"
#include "Input.h"
#include "Translations.h"
#include "TutorialText.h"
#include "Meta.h"
#include "Achievements.h"
#include "Codex.h"
#include "Augment.h"
#include "PlayerStats.h"
#include "Weapons.h"
#include "ExpSystem.h"
#include "SaveSystem.h"
#include "SettingsSnapshot.h"
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
#include "Monster.h"
#include "../../Render/Juice.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <functional>

#include "SceneInternal.h"

float g_SettingsEntryT = 0.0f;
bool s_SettingsInlineNeedsReset = false;

void ResetSettingsUi(float entryStart) {
    g_SettingsEntryT = std::max(0.0f, std::min(1.0f, entryStart));
    s_SettingsInlineNeedsReset = true;
}


// Settings copy in all three languages. A plain "korean ? kr : en" let
// Japanese fall back to English for most labels (QA #36).
static const wchar_t* SettingsText(const wchar_t* kr, const wchar_t* en,
                                   const wchar_t* jp) {
    if (IsChinese()) return ZhTranslate(en);
    const int li = LangIndex();
    return li == 0 ? kr : (li == 2 ? jp : en);
}

// CONTROLS row names, indexed by GameAction then [KR, EN, JP].
static const wchar_t* const kKeyActionLabels[GAME_ACTION_COUNT][3] = {
    { L"위로 이동",     L"MOVE UP",    L"上へ移動" },
    { L"아래로 이동",   L"MOVE DOWN",  L"下へ移動" },
    { L"왼쪽 이동",     L"MOVE LEFT",  L"左へ移動" },
    { L"오른쪽 이동",   L"MOVE RIGHT", L"右へ移動" },
    { L"대시",          L"DASH",       L"ダッシュ" },
    { L"스킬 1",        L"SKILL 1",    L"スキル1" },
    { L"스킬 2",        L"SKILL 2",    L"スキル2" },
    { L"스킬 3",        L"SKILL 3",    L"スキル3" },
};

void Scene_SettingsInline(const SceneCtx& c) {
    const float sw = c.sw, sh = c.sh;
    const float dt = std::min(c.delta, 0.05f);
    const double mx = c.mx, my = c.my;
    const bool lmb = c.lmb;
    const float now = (float)glfwGetTime();
    const bool inGameSettings = (g_SettingsReturnTo == GameState::PAUSED);
    // All of the page's state lives in one struct so it has a name and a single
    // place to look at (it still persists between frames).
    struct SettingsUiState {
        int tab = 0; // 0=DISPLAY 1=AUDIO 2=CONTROL 3=SYSTEM
        float entry = 0.0f;
        float exitT = 0.0f;
        bool exiting = false;
        bool prevEsc = false;
        float pulse = 0.0f;
        int pulseRow = -1;
        float tabSwitchT = 0.0f;
        int s_volDrag = -1; // category*8+row of the slider being dragged
        float listHover[32] = {};
        float catOpen[5] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };   // accordion openness per category
        float rowHover[5][10] = {};
        float optHover[5][10][8] = {};
        float holdT = 0.0f;
        bool resetLatched = false; // reset fired; wait for the button release
        float resetDoneT = 0.0f; // seconds left on the "reset done" label
        int listCursor = 1;
        bool accordionFocusPending = false;
        // Category whose header should be scrolled to the top of the board.
        int scrollToCategory = -1;
        // True after a wheel scroll: the selected category tracks the right board.
        bool scrollFollowsBoard = false;
        // Shared normalized scroll position for the left index and right board.
        // Each pane maps the same 0..1 value to its own row geometry.
        float settingsScroll = 0.0f;
        float settingsScrollTarget = 0.0f;
        bool prevListW = false;
        bool prevListS = false;
        int detailRow = 0;
        // SettingsSnapshot.h: capture / restore / compare (REG-26).
        SettingsSnapshot savedSettings = {};
        bool savedSettingsValid = false;
        bool confirmBack = false;
        bool showCredits = false;   // CREDITS page open over the settings board
        float creditsT = 0.0f;      // seconds it has been open (ignores the opening click)
        bool settingsDirty = false;
        // CONTROLS: action whose key is being captured (-1 = none), fade of the
        // other rows while waiting, per-key confirm flash and the status line.
        int captureAction = -1;
        float captureBlend = 0.0f;
        float keyFlash[GAME_ACTION_COUNT] = {};
        std::wstring keyMsg;
        float keyMsgT = 0.0f;
        bool wasInline = false;
    };
    static SettingsUiState S;
    auto& tab = S.tab;
    auto& entry = S.entry;
    auto& exitT = S.exitT;
    auto& exiting = S.exiting;
    auto& prevEsc = S.prevEsc;
    auto& pulse = S.pulse;
    auto& pulseRow = S.pulseRow;
    auto& tabSwitchT = S.tabSwitchT;
    auto& s_volDrag = S.s_volDrag;
    auto& listHover = S.listHover;
    auto& catOpen = S.catOpen;
    auto& rowHover = S.rowHover;
    auto& optHover = S.optHover;
    auto& holdT = S.holdT;
    auto& resetLatched = S.resetLatched;
    auto& resetDoneT = S.resetDoneT;
    auto& listCursor = S.listCursor;
    auto& accordionFocusPending = S.accordionFocusPending;
    auto& scrollToCategory = S.scrollToCategory;
    auto& scrollFollowsBoard = S.scrollFollowsBoard;
    auto& settingsScroll = S.settingsScroll;
    auto& settingsScrollTarget = S.settingsScrollTarget;
    auto& prevListW = S.prevListW;
    auto& prevListS = S.prevListS;
    auto& detailRow = S.detailRow;
    auto& savedSettings = S.savedSettings;
    auto& savedSettingsValid = S.savedSettingsValid;
    auto& confirmBack = S.confirmBack;
    auto& showCredits = S.showCredits;
    auto& creditsT = S.creditsT;
    auto& settingsDirty = S.settingsDirty;
    auto& captureAction = S.captureAction;
    auto& captureBlend = S.captureBlend;
    auto& keyFlash = S.keyFlash;
    auto& keyMsg = S.keyMsg;
    auto& keyMsgT = S.keyMsgT;
    auto& wasInline = S.wasInline;
    if (!wasInline || s_SettingsInlineNeedsReset) {
        entry = g_SettingsEntryT;
        exitT = 0.0f;
        exiting = false;
        prevEsc = false;
        pulse = 0.0f;
        pulseRow = -1;
        tabSwitchT = 0.0f;
        s_volDrag  = -1;
        holdT = 0.0f;
        resetLatched = false;
        resetDoneT = 0.0f;
        listCursor = 1;
        accordionFocusPending = false;
        tab = 0;   // always open on the first category, scrolled to the top
        scrollToCategory = -1;
        settingsScroll = 0.0f;
        settingsScrollTarget = 0.0f;
        scrollFollowsBoard = false;
        prevListW = false;
        prevListS = false;
        detailRow = 0;
        savedSettings = CaptureSettingsSnapshot();
        savedSettingsValid = true;
        confirmBack = false;
        settingsDirty = false;
        captureAction = -1;
        captureBlend = 0.0f;
        for (auto& f : keyFlash) f = 0.0f;
        keyMsgT = 0.0f;
        g_KeyCaptureActive = false;
        g_CapturedKey = -1;
        for (auto& h : listHover) h = 0.0f;
        for (int c2 = 0; c2 < 5; ++c2) catOpen[c2] = (c2 == tab) ? 1.0f : 0.0f;
        for (auto& category : rowHover)
            for (auto& h : category) h = 0.0f;
        for (auto& category : optHover)
            for (auto& row : category)
                for (auto& h : row) h = 0.0f;
        wasInline = true;
        s_SettingsInlineNeedsReset = false;
    }
    entry = std::min(1.0f, entry + dt);
    g_SettingsEntryT = entry;
    pulse = std::max(0.0f, pulse - dt * 2.0f);
    tabSwitchT += dt;

    auto restoreSettings = [&]() {
        if (!savedSettingsValid) return;
        RestoreSettingsSnapshot(savedSettings);
    };
    auto settingsChanged = [&]() {
        if (!savedSettingsValid) return false;
        return SettingsSnapshotChanged(savedSettings);
    };

    const bool rmb = c.window && glfwGetMouseButton(c.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (c.inputFocusChanged) s_volDrag = -1;
    // While a key is being captured, ESC / right-click cancel the capture
    // instead of leaving the screen (decided from the frame-start state).
    const bool keyCapturing = captureAction >= 0;
    const bool escEdge = EscPressedEdge(c, prevEsc);
    const bool backRaw = !keyCapturing && (escEdge || (rmb && !g_RmbPrev));
    // While the credits page is up, back closes it instead of leaving settings.
    const bool backInput = backRaw && !showCredits;
    if (backRaw && showCredits) showCredits = false;
    const float entryOldOut = SceneTransitionEase(entry / kOutgameTransitionDuration);
    const float entryTreeIn = SceneTransitionEase(
        (entry - 0.08f) / (kOutgameTransitionDuration - 0.08f));
    if (backInput && !exiting && entry >= 0.55f) {
        if (settingsDirty) confirmBack = true;
        else exiting = true;
    }
    if (exiting) exitT = std::min(kOutgameTransitionDuration, exitT + dt);
    const float outP = exiting
        ? SceneTransitionEase(exitT / kOutgameTransitionDuration) : 0.0f;
    const float treeA = entryTreeIn * (1.0f - outP);
    const float oldA = exiting ? outP : (1.0f - entryOldOut);
    const bool ready = !exiting && entry >= 0.55f;
    const bool inputReady = ready && !confirmBack && !showCredits;

    // ── Key capture (CONTROLS): the Input key callback stores the next press
    // in g_CapturedKey while g_KeyCaptureActive is set.
    auto endKeyCapture = [&]() {
        captureAction = -1;
        g_KeyCaptureActive = false;
        g_CapturedKey = -1;
    };
    keyMsgT = std::max(0.0f, keyMsgT - dt);
    for (auto& f : keyFlash) f = std::max(0.0f, f - dt * 2.5f);
    if (captureAction >= 0) {
        if (c.inputFocusChanged || (rmb && !g_RmbPrev) || !inputReady) {
            endKeyCapture();
        } else {
            g_KeyCaptureActive = true;
            if (g_CapturedKey >= 0) {
                const int pressed = g_CapturedKey;
                g_CapturedKey = -1;
                if (pressed == kBindKeyEscape) {
                    endKeyCapture();
                } else {
                    int other = -1;
                    const int norm = NormalizeBindKey(pressed);
                    for (int i = 0; i < GAME_ACTION_COUNT; ++i)
                        if (i != captureAction && g_KeyBinds.key[i] == norm) other = i;
                    const BindResult result = AssignKeyBinding(
                        g_KeyBinds, (GameAction)captureAction, pressed);
                    if (result == BindResult::Rejected) {
                        // Stay in capture so another key can be tried.
                        keyMsg = SettingsText(L"사용할 수 없는 키입니다",
                                              L"THAT KEY IS RESERVED",
                                              L"使用できないキーです");
                        keyMsgT = 2.2f;
                    } else {
                        keyFlash[captureAction] = 1.0f;
                        if (result == BindResult::Swapped && other >= 0) {
                            keyFlash[other] = 1.0f;
                            const std::wstring a = kKeyActionLabels[captureAction][LangIndex()];
                            const std::wstring b = kKeyActionLabels[other][LangIndex()];
                            keyMsg = LangIndex() == 0 ? a + L" · " + b + L" 키를 서로 바꿨습니다"
                                   : LangIndex() == 2 ? a + L" と " + b + L" のキーを入れ替えました"
                                   : L"SWAPPED KEYS: " + a + L" / " + b;
                            keyMsgT = 2.2f;
                        }
                        endKeyCapture();
                    }
                }
            }
        }
    }
    captureBlend += ((captureAction >= 0 ? 1.0f : 0.0f) - captureBlend)
                  * std::min(1.0f, dt * 10.0f);
    SetSceneTextureReveal(exiting
        ? std::max(0.0f, 1.0f - outP)
        : SceneTransitionEase(entry / kOutgameTransitionDuration));

    const float uiS = UiScale(sw, sh);
    auto settingsLevel = [&](TextRenderer& renderer) {
        return &renderer == &g_TextL
            ? UiTextLevel::Description : UiTextLevel::Supporting;
    };
    auto settingsScale = [&](TextRenderer& renderer, float scale,
                             UiTextLevel level) {
        return UiTextScale(renderer, level, scale * uiS);
    };
    auto drawSettingsText = [&](TextRenderer& renderer, const wchar_t* text,
                                float x, float y, float scale,
                                float r, float g, float b, float alpha,
                                float shadowAlpha = 0.68f) {
        DrawShadowedText(renderer, text, x, y,
                         settingsScale(renderer, scale, settingsLevel(renderer)),
                         r, g, b, alpha, shadowAlpha);
    };
    auto settingsButtonTextWidth = [&](const wchar_t* text, float scale,
                                       UiTextLevel level = UiTextLevel::Title) {
        return g_TextL.Width(text,
            settingsScale(g_TextL, scale, level));
    };
    auto drawSettingsTextAtLevel = [&](TextRenderer& renderer, const wchar_t* text,
                                       float x, float y, float scale,
                                       float r, float g, float b, float alpha,
                                       UiTextLevel level, float shadowAlpha) {
        DrawShadowedText(renderer, text, x, y,
                         settingsScale(renderer, scale, level),
                         r, g, b, alpha, shadowAlpha);
    };
    const float mainBH = 70.0f * uiS, mainGap = 15.0f * uiS;
    const float mainX = BottomLeftActionX(uiS);
    // Settings uses its own full-canvas composition. The generic inline
    // layout is intentionally not used here because it compresses the page
    // into a small center panel.
    const float depthX = sw * 0.30f;
    const float depthY = std::max(112.0f * uiS, sh * 0.13f);
    const float detailX = sw * 0.55f;
    const float detailY = depthY;
    const float detailW = std::max(300.0f * uiS, sw - detailX - 64.0f * uiS);
    const float detailH = sh - depthY - 112.0f * uiS;

    // ── Large atmospheric backdrop ─────────────────────────────────
    // The field is intentionally left open; the live background is the page.

    // Canvas panel — solid core + feathered edges
    // Tab accent color — shared by sphere motif and option chips
    const float tabR = tab == 0 ? 0.35f : tab == 1 ? 1.00f : tab == 2 ? 0.35f : tab == 3 ? 1.00f : 0.72f;
    const float tabG = tab == 0 ? 0.72f : tab == 1 ? 0.72f : tab == 2 ? 0.92f : tab == 3 ? 0.45f : 0.52f;
    const float tabB = tab == 0 ? 1.00f : tab == 1 ? 0.30f : tab == 2 ? 0.55f : tab == 3 ? 0.62f : 1.00f;

    DrawPersistentSceneSideVignettes(sw, sh, kWideSceneLinearAlpha);
    {
        const float cW  = detailX + detailW - depthX;
        const float cH  = detailH;
        // Center at 78% of the panel width — large enough radius that rings bleed outside
        const float mCX = depthX + cW * 0.24f;
        const float mCY = depthY + cH * 0.50f;
        const float mR  = std::min(cH * 0.28f, cW * 0.17f);

        // Settings is a calibration screen, not a constellation browser.
        // Keep only a very soft focal field behind the values; constellation
        // nodes and the globe are intentionally omitted here.
        DrawSceneRadialVignette(mCX, mCY, mR * 2.2f, 0.035f * treeA);
    }

    if ((g_LobbyPanel == LobbyPanel::Settings))
        DrawMainAsteriaLogo(sw, sh, 0.42f * oldA,
                           LogoClamp01(oldA + 0.20f * treeA), sw * kLobbyLogoCenterXFrac - 160.0f * (1.0f - oldA),
                           kLobbyLogoHeightMul);

    const float settingsTitleY = 42.0f * uiS;
    drawSettingsTextAtLevel(g_TextL, SettingsText(L"설정", L"SETTINGS", L"設定"), mainX, settingsTitleY,
                            1.0f, 1.0f, 1.0f, 1.0f, 0.94f * treeA,
                            UiTextLevel::Title, 0.72f);
    drawSettingsTextAtLevel(g_TextS, SettingsText(L"시스템 보정", L"SYSTEM CALIBRATION", L"システム調整"),
                            mainX + 4.0f * uiS,
                            settingsTitleY + g_TextL.Height(
                                SettingsText(L"설정", L"SETTINGS", L"設定"),
                                settingsScale(g_TextL, 1.0f, UiTextLevel::Title)) + 8.0f * uiS,
                            1.0f, 0.35f, 0.76f, 1.0f, 0.72f * treeA,
                            UiTextLevel::Subtitle, 1.0f);

    // \uC88C\uCE21 ghost \uBC84\uD2BC \u2014 \uCEE8\uD14D\uC2A4\uD2B8\uC5D0 \uB530\uB77C \uBA54\uC778\uBA54\uB274 vs \uC77C\uC2DC\uC815\uC9C0 \uBA54\uB274
    if ((g_LobbyPanel == LobbyPanel::Settings)) {
        const float mainMenuGap = 10.0f * uiS;
        for (int i = 0; i < kMainMenuRouteCount; ++i) {
            if (i == kMainMenuTutorialIndex && TutorialRouteHidden()) continue;
            const float y = MainMenuButtonRailStartY(sh, uiS) + i * (mainBH + mainMenuGap);
            const bool focus = i == kMainMenuSettingsIndex;
            const float x = mainX - 250.0f * (1.0f - oldA) - (focus ? 0.0f : 24.0f * (1.0f - oldA));
            const float a = (focus ? 0.82f : 0.34f) * oldA;
            const wchar_t* route = MainMenuRouteLabel(LangIndex(), i);
            const float routeScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            const float routeY = y + (mainBH - g_TextL.Height(route, routeScale)) * 0.5f;
            drawSettingsTextAtLevel(g_TextL, route, x, routeY, 1.0f,
                                    1.0f, 1.0f, 1.0f, a,
                                    UiTextLevel::Title, 0.70f);
        }
    } else if (!inGameSettings) {
        const wchar_t* menu[4] = { SettingsText(L"계속하기", L"CONTINUE", L"続ける"),
                                   SettingsText(L"설정", L"CALIBRATION", L"設定"),
                                   SettingsText(L"\uD3EC\uAE30\uD558\uAE30", L"ABANDON PLAY", L"放棄する"),
                                   SettingsText(L"종료", L"TERMINATE", L"終了") };
        for (int i = 0; i < 4; ++i) {
            const float y = MainMenuButtonRailStartY(sh, uiS) + i * (mainBH + mainGap);
            const bool focus = i == 1;
            const float x = mainX - 250.0f * (1.0f - oldA) - (focus ? 0.0f : 24.0f * (1.0f - oldA));
            const float a = (focus ? 0.82f : 0.34f) * oldA;
            const float routeScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            const float routeH = g_TextL.Height(menu[i], routeScale);
            drawSettingsTextAtLevel(g_TextL, menu[i], x,
                                    y + (mainBH - routeH) * 0.5f, 1.0f,
                                    1.0f, 1.0f, 1.0f, a,
                                    UiTextLevel::Title, 0.70f);
        }
    }

    // \u2550\u2550\u2550 TAB RAIL (DISPLAY / AUDIO / CONTROL / SYSTEM / BACK) \u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550
    struct SettingsTab { const wchar_t* en; const wchar_t* kr; const wchar_t* jp; };
    static const SettingsTab tabs[5] = {
        { L"DISPLAY",  L"\uD654\uBA74", L"画面"       },
        { L"AUDIO",    L"\uC18C\uB9AC", L"サウンド"       },
        { L"GAMEPLAY", L"\uAC8C\uC784\uD50C\uB808\uC774", L"ゲームプレイ" },
        { L"OTHER",    L"\uAE30\uD0C0", L"その他" },
        { L"BACK",     L"\uB4A4\uB85C", L"戻る"       },
    };
    if (tab >= 5) tab = 0;

    auto activateSettingsCategory = [&](int nextCategory, int nextRow) {
        if (nextCategory < 0 || nextCategory >= 5) return;
        const bool categoryChanged = tab != nextCategory;
        tab = nextCategory;
        detailRow = std::max(0, nextRow);
        if (!categoryChanged) return;

        scrollToCategory = nextCategory;
        pulse = 1.0f;
        pulseRow = -1;
        tabSwitchT = 0.0f;
        for (auto& category : rowHover)
            for (auto& h : category) h = 0.0f;
        for (auto& category : optHover)
            for (auto& row : category)
                for (auto& h : row) h = 0.0f;
    };

    // The integrated list below is the only settings navigation surface.
    // The former orbit-tab controls were non-interactive and duplicated it.
    // The open field intentionally has no active-tab bridge line.

    // \u2550\u2550\u2550 RIGHT PANEL \u2014 SETTINGS ROWS \u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550
    // While the unsaved-changes dialog is open, the rows fade back so their
    // labels do not show through behind the dialog copy (QA #45).
    const float dA   = treeA * (0.78f + 0.22f * (1.0f - pulse))
                     * (confirmBack ? 0.18f : 1.0f);
    // The settings body owns the full area to the right of the category
    // rail, split into a readable list column and a dedicated readout column.
    const float rpX  = depthX;
    const float rpW  = detailX + detailW - rpX;
    const float categoryReveal = Smoothstep(
        LogoClamp01(tabSwitchT / 0.22f));

    // The board starts right below the top of the panel; each category's own
    // header row (inside the scrolling list) is the only category title.
    const float headerAreaH = 20.0f * uiS;
    const float sepY = depthY + headerAreaH;

    // Row layout constants (5행 탭은 높이 줄임)
    // Use the available vertical field instead of compressing every tab into
    // the same short block. Sparse tabs therefore breathe, while DISPLAY
    // still keeps enough rows visible without creating a second duplicate UI.
    const float rRowH   = 220.0f;
    const float rFirstY = detailY + 92.0f;
    const float rLabelX = rpX;
    // Let the control column use the full available width. The old 900px cap
    // forced FPS options to shrink and made read-only values collide with the
    // description column on wider layouts.
    // Keep the interaction/readout panel inside its own right-side bounds.
    // The old minimum width could push the last option past the canvas on
    // narrow windows.
    const float rClickW = std::max(1.0f, rpW - 20.0f * uiS);

    if (pulse <= 0.01f) pulseRow = -1;

    // \u2500\u2500 Row data per tab \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    struct SRow {
        const wchar_t* label     = nullptr;
        const wchar_t* value     = nullptr; // nullptr = opts or hold bar
        bool readOnly  = false;
        bool isVolume  = false;             // left-half=-10, right-half=+10
        bool isDanger  = false;
        bool isHold    = false;
        const wchar_t* opts[8]  = {};       // 선택지 전체 목록 (없으면 optCount=0)
        int optCount   = 0;
        int optCur     = 0;                 // 현재 선택된 인덱스
        int* volRef    = nullptr;           // isVolume 슬라이더가 편집하는 0~100 값
        int  keyAction = -1;                // CONTROLS: GameAction this row rebinds
        bool isKeyReset = false;            // CONTROLS: restore-defaults row
    };
    SRow settingsRows[5][10] = {};
    const int rowCounts[5] = { 4, 5, kDebugSettingsVisible ? 4 : 3,
                               GAME_ACTION_COUNT + 1, 3 };

    static wchar_t s_volBuf[8];

    // HOLD TO RESET: the fill rushes in and slows toward the end (ease-out),
    // while the screen shake grows with the held time so it peaks at the end.
    constexpr float kResetHoldSec = 2.2f;
    auto resetFill = [&](float held) {
        const float x = std::max(0.0f, std::min(1.0f, held / kResetHoldSec));
        const float r = 1.0f - x;
        return 1.0f - r * r * r;
    };

    // \u2500\u2500 Render rows \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    // Keep every category's controls expanded on the right settings board.
    swprintf_s(s_volBuf, L"%d", g_SoundVol);
    const int allLangCur = (int)g_Language;   // KR 0, EN 1, JP 2, ZH 3
    const int allFpsCur =
        g_FpsCap == 0 ? 0 : g_FpsCap == 30 ? 1 : g_FpsCap == 60 ? 2 :
        g_FpsCap == 144 ? 3 : g_FpsCap == 300 ? 4 : 5;
    static const wchar_t* const kToggleLabels[3][2] = {
        { L"켜짐", L"꺼짐" }, { L"ON", L"OFF" }, { L"オン", L"オフ" }
    };
    const int toggleLanguage = std::max(0, std::min(2, LangIndex()));
    const wchar_t* const toggleOn = kToggleLabels[toggleLanguage][0];
    const wchar_t* const toggleOff = kToggleLabels[toggleLanguage][1];
    settingsRows[0][0] = { SettingsText(L"FPS 제한", L"FPS CAP", L"FPS上限"), nullptr, false, false, false, false,
                           { SettingsText(L"동기화", L"VSYNC", L"同期"), L"30", L"60", L"144", L"300",
                             SettingsText(L"무제한", L"UNLIM", L"無制限") }, 6, allFpsCur };
    settingsRows[0][1] = { SettingsText(L"그래픽 품질", L"GRAPHICS", L"グラフィック品質"), nullptr, false, false, false, false,
                           { SettingsText(L"낮음", L"LOW", L"低"), SettingsText(L"보통", L"MEDIUM", L"中"),
                             SettingsText(L"높음", L"HIGH", L"高") },
                           3, g_VfxDensity == VfxDensity::REDUCED ? 0 :
                              g_VfxDensity == VfxDensity::MEDIUM ? 1 : 2 };
    settingsRows[0][2] = { SettingsText(L"콤보 HUD", L"COMBO HUD", L"コンボHUD"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2, g_ShowCombo ? 0 : 1 };
    // Menu darkness: drag slider, 50 = the original look. Menus only.
    settingsRows[0][3] = { SettingsText(L"화면 어둡기", L"MENU DIMMING", L"画面の暗さ"), nullptr, false, true, false, false,
                           {}, 0, 0, &g_MenuDim };
    settingsRows[1][0] = { SettingsText(L"마스터 볼륨", L"MASTER VOL", L"マスター音量"), s_volBuf, false, true, false, false,
                           {}, 0, 0, &g_SoundVol };
    settingsRows[1][1] = { SettingsText(L"BGM 채널", L"BGM BUS", L"BGM"), nullptr, false, true, false, false,
                           {}, 0, 0, &g_BgmVol };
    settingsRows[1][2] = { SettingsText(L"효과음 채널", L"SFX BUS", L"効果音"), nullptr, false, true, false, false,
                           {}, 0, 0, &g_SfxVol };
    settingsRows[1][3] = { SettingsText(L"출력", L"OUTPUT", L"出力"), nullptr, false, false, false, false,
                           { SettingsText(L"스테레오", L"STEREO", L"ステレオ"),
                             SettingsText(L"모노", L"MONO", L"モノ") },
                           2, g_AudioMonoOutput ? 1 : 0 };
    settingsRows[1][4] = { SettingsText(L"오디오 엔진", L"AUDIO ENGINE", L"オーディオエンジン"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2,
                           g_AudioEngineEnabled ? 0 : 1 };
    settingsRows[2][0] = { SettingsText(L"자동 스킬", L"AUTO SKILL", L"自動スキル"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2, g_AutoSkill ? 0 : 1 };
    settingsRows[2][1] = { SettingsText(L"조준선", L"CROSSHAIR", L"照準"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2, g_ShowCrosshair ? 0 : 1 };
    // Off by default: the boss panes then show only Windows wallpapers.
    settingsRows[2][2] = { SettingsText(L"보스 창에 내 사진", L"MY PHOTOS IN PANES", L"ボス窓に写真"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2, g_PanePhotos ? 0 : 1 };
    settingsRows[2][3] = { SettingsText(L"\uB514\uBC84\uAE45 \uBAA8\uB4DC", L"DEBUG MODE", L"デバッグモード"), nullptr, false, false, false, false,
                           { toggleOn, toggleOff }, 2, g_DebugMode ? 0 : 1 };
    for (int a = 0; a < GAME_ACTION_COUNT; ++a) {
        settingsRows[3][a].label = kKeyActionLabels[a][LangIndex()];
        settingsRows[3][a].keyAction = a;
    }
    settingsRows[3][GAME_ACTION_COUNT].label = SettingsText(
        L"기본값 복원", L"RESTORE DEFAULTS", L"初期設定に戻す");
    settingsRows[3][GAME_ACTION_COUNT].isKeyReset = true;
    settingsRows[4][0] = { SettingsText(L"언어", L"LANGUAGE", L"言語"), nullptr, false, false, false, false,
                           { L"한국어", L"English", L"日本語", L"中文" },
                           4, allLangCur };
    // CREDITS: one chip that opens the credits page.
    settingsRows[4][2] = { SettingsText(L"크레딧", L"CREDITS", L"クレジット"), nullptr, false, false, false, false,
                           { SettingsText(L"보기", L"VIEW", L"表示") }, 1, 0 };
    settingsRows[4][1] = { SettingsText(L"데이터 초기화", L"RESET DATA", L"データ初期化"), nullptr, false, false, true, true };

    auto settingsOptionTextLevel = [&](int category, int row) {
        const SRow& setting = settingsRows[category][row];
        const bool isToggle = setting.optCount == 2
            && ((setting.opts[0] == toggleOn && setting.opts[1] == toggleOff)
                || (setting.opts[0] == toggleOff && setting.opts[1] == toggleOn));
        return isToggle ? UiTextLevel::Subtitle : UiTextLevel::Title;
    };

    auto settingChanged = [&](int category, int row) {
        if (!savedSettingsValid) return false;
        if (category == 0) {
            if (row == 0) return g_FpsCap != savedSettings.fps;
            if (row == 1) return g_VfxDensity != savedSettings.density;
            if (row == 2) return g_ShowCombo != savedSettings.combo;
            if (row == 3) return g_MenuDim != savedSettings.menuDim;
        } else if (category == 1) {
            if (row == 0) return g_SoundVol != savedSettings.soundVol;
            if (row == 1) return g_BgmVol != savedSettings.bgmVol;
            if (row == 2) return g_SfxVol != savedSettings.sfxVol;
            if (row == 3) return g_AudioMonoOutput != savedSettings.audioMonoOutput;
            if (row == 4) return g_AudioEngineEnabled != savedSettings.audioEngineEnabled;
        } else if (category == 2) {
            if (row == 0) return g_AutoSkill != savedSettings.autoSkill;
            if (row == 1) return g_ShowCrosshair != savedSettings.crosshair;
            if (row == 2) return g_PanePhotos != savedSettings.panePhotos;
            if (row == 3) return g_DebugMode != savedSettings.debugMode;
        } else if (category == 3) {
            return row < GAME_ACTION_COUNT
                && g_KeyBinds.key[row] != savedSettings.keyBinds.key[row];
        } else if (category == 4) {
            return row == 0 && g_Language != savedSettings.language;
        }
        return false;
    };

    // The left catalogue keeps every category visible and expands only the
    // selected one. The right panel uses that same selection.
    struct SettingsListEntry { int category; int row; bool header; const wchar_t* label; };
    SettingsListEntry list[32] = {};
    int listCount = 0;
    auto addHeader = [&](int cat, const wchar_t* label) {
        list[listCount++] = { cat, -1, true, label };
    };
    auto addItem = [&](int cat, int row, const wchar_t* label) {
        list[listCount++] = { cat, row, false, label };
    };
    const wchar_t* displayLabels[4] = {
        SettingsText(L"FPS 제한", L"FPS CAP", L"FPS上限"),
        SettingsText(L"그래픽 품질", L"GRAPHICS", L"グラフィック品質"),
        SettingsText(L"콤보 HUD", L"COMBO HUD", L"コンボHUD"),
        SettingsText(L"화면 어둡기", L"MENU DIMMING", L"画面の暗さ") };
    const wchar_t* audioLabels[5] = {
        SettingsText(L"마스터 볼륨", L"MASTER VOL", L"マスター音量"),
        SettingsText(L"BGM 채널", L"BGM BUS", L"BGM"),
        SettingsText(L"효과음 채널", L"SFX BUS", L"効果音"),
        SettingsText(L"출력", L"OUTPUT", L"出力"),
        SettingsText(L"오디오 엔진", L"AUDIO ENGINE", L"オーディオエンジン") };
    const wchar_t* gameplayLabels[4] = {
        SettingsText(L"자동 스킬", L"AUTO SKILL", L"自動スキル"),
        SettingsText(L"조준선", L"CROSSHAIR", L"照準"),
        SettingsText(L"보스 창에 내 사진", L"MY PHOTOS IN PANES", L"ボス窓に写真"),
        SettingsText(L"\uB514\uBC84\uAE45 \uBAA8\uB4DC", L"DEBUG MODE", L"デバッグモード") };
    const wchar_t* controlsLabels[GAME_ACTION_COUNT + 1] = {};
    for (int a = 0; a < GAME_ACTION_COUNT; ++a)
        controlsLabels[a] = kKeyActionLabels[a][LangIndex()];
    controlsLabels[GAME_ACTION_COUNT] = settingsRows[3][GAME_ACTION_COUNT].label;
    const wchar_t* archiveLabels[3] = {
        SettingsText(L"언어", L"LANGUAGE", L"言語"),
        SettingsText(L"데이터 초기화", L"RESET DATA", L"データ初期化"),
        SettingsText(L"크레딧", L"CREDITS", L"クレジット") };
    // The left category index is an accordion; keep every right-side option
    // visible so changing categories does not collapse the settings board.
    addHeader(0, SettingsText(L"화면", L"DISPLAY", L"画面"));
    for (int i = 0; i < 4; ++i) addItem(0, i, displayLabels[i]);
    addHeader(1, SettingsText(L"소리", L"AUDIO", L"サウンド"));
    for (int i = 0; i < 5; ++i) addItem(1, i, audioLabels[i]);
    addHeader(2, SettingsText(L"게임플레이", L"GAMEPLAY", L"ゲームプレイ"));
    for (int i = 0; i < (kDebugSettingsVisible ? 4 : 3); ++i)
        addItem(2, i, gameplayLabels[i]);
    addHeader(3, SettingsText(L"조작", L"CONTROLS", L"操作"));
    for (int i = 0; i < GAME_ACTION_COUNT + 1; ++i)
        addItem(3, i, controlsLabels[i]);
    addHeader(4, SettingsText(L"기타", L"OTHER", L"その他"));
    for (int i = 0; i < 3; ++i) addItem(4, i, archiveLabels[i]);

    SettingsListEntry rightList[32] = {};
    int rightListCount = 0;
    auto addRightHeader = [&](int cat, const wchar_t* label) {
        rightList[rightListCount++] = { cat, -1, true, label };
    };
    auto addRightItem = [&](int cat, int row, const wchar_t* label) {
        rightList[rightListCount++] = { cat, row, false, label };
    };
    const wchar_t* categoryLabels[5] = {
        SettingsText(L"화면", L"DISPLAY", L"画面"),
        SettingsText(L"소리", L"AUDIO", L"サウンド"),
        SettingsText(L"게임플레이", L"GAMEPLAY", L"ゲームプレイ"),
        SettingsText(L"조작", L"CONTROLS", L"操作"),
        SettingsText(L"기타", L"OTHER", L"その他") };
    const wchar_t* const* categoryRows[5] = {
        displayLabels, audioLabels, gameplayLabels, controlsLabels, archiveLabels };
    for (int category = 0; category < 5; ++category) {
        addRightHeader(category, categoryLabels[category]);
        for (int row = 0; row < rowCounts[category]; ++row)
            addRightItem(category, row, categoryRows[category][row]);
    }

    bool focusChanged = accordionFocusPending;
    // The frame after a category click focuses its first row; that must keep
    // the category header at the top instead of centering the row.
    if (accordionFocusPending) scrollToCategory = tab;
    accordionFocusPending = false;

    auto isListItem = [&](int index) {
        return index >= 0 && index < listCount && !list[index].header
            && list[index].category == tab;
    };
    auto stepListCursor = [&](int direction) {
        int next = listCursor;
        do {
            next += direction;
            if (next < 0) next = listCount - 1;
            if (next >= listCount) next = 0;
        } while (!isListItem(next));
        listCursor = next;
    };
    if (!isListItem(listCursor) || list[listCursor].category != tab) {
        listCursor = -1;
        for (int i = 0; i < listCount; ++i) {
            if (isListItem(i) && list[i].category == tab) {
                listCursor = i;
                break;
            }
        }
        if (listCursor < 0) listCursor = 0;
    }

    const float listX = mainX + 14.0f * uiS;
    const float listTop = depthY + 8.0f * uiS;
    const float listW = std::max(250.0f * uiS, depthX - listX - 38.0f * uiS);
    const float listBottom = sh - 154.0f * uiS;
    const float listH = std::max(1.0f, listBottom - listTop);
    const float listRowH = 52.0f * uiS;
    // The rendered y positions are pixels, so their scroll range must be
    // pixels too.  Row-count values here capped the previous list movement
    // to only a handful of pixels.
    // Accordion: every category's rows exist, but a category's rows only take
    // height as far as it is open. The openness eases toward the selected
    // category, so changing tab folds one group away while the next unfolds.
    float listRowOff[32] = {};
    float listRowHt[32] = {};
    float leftTotalH = 0.0f;
    {
        const float openStep = 1.0f - std::exp(-dt * 12.0f);
        for (int c2 = 0; c2 < 5; ++c2) {
            const float goal = (c2 == tab) ? 1.0f : 0.0f;
            if (std::fabs(goal - catOpen[c2]) < 0.004f) catOpen[c2] = goal;
            else catOpen[c2] += (goal - catOpen[c2]) * openStep;
        }
        for (int i = 0; i < listCount; ++i) {
            listRowOff[i] = leftTotalH;
            listRowHt[i] = list[i].header ? listRowH
                                          : listRowH * catOpen[list[i].category];
            leftTotalH += listRowHt[i];
        }
    }
    const float leftMaxScroll = std::max(0.0f, leftTotalH - listH);
    const float rightListTop = sepY + 34.0f * uiS;
    const float rightListBottom = sh - 206.0f * uiS;
    const float rightListH = std::max(1.0f, rightListBottom - rightListTop);
    const float rightRowH = 112.0f * uiS;
    // Extend the range so even the last category's header can reach the top.
    int lastHeaderIndex = 0;
    for (int i = 0; i < rightListCount; ++i)
        if (rightList[i].header) lastHeaderIndex = i;
    const float rightContentH = std::max(
        (float)rightListCount * rightRowH,
        (float)lastHeaderIndex * rightRowH + rightListH);
    const float rightMaxScroll = std::max(0.0f, rightContentH - rightListH);
    settingsScrollTarget = std::max(0.0f, std::min(1.0f, settingsScrollTarget));
    // The current scroll value deliberately trails the requested position.
    // This single shared easing drives the left index, right board and both
    // scroll thumbs together.
    // Exponential approach: the same glide at any frame rate (a plain dt * k
    // step moves faster per second at high FPS), snapped once it is within
    // a sliver of the target so the last frames do not crawl.
    {
        const float gap = settingsScrollTarget - settingsScroll;
        if (std::fabs(gap) < 0.0004f) settingsScroll = settingsScrollTarget;
        else settingsScroll += gap * (1.0f - std::exp(-dt * 10.0f));
    }
    settingsScroll = std::max(0.0f, std::min(1.0f, settingsScroll));
    const float leftScroll = settingsScroll * leftMaxScroll;
    const float rightScroll = settingsScroll * rightMaxScroll;
    const float listRailTopX = listX - 4.0f * uiS;
    const float listRailDiagonal = std::min(72.0f * uiS, listW * 0.18f);
    const float listRailBottomX = listRailTopX + listRailDiagonal;
    auto listRailXAt = [&](float y) {
        const float t = std::max(0.0f,
            std::min(1.0f, (y - listTop) / std::max(1.0f, listH)));
        return listRailTopX + listRailDiagonal * t;
    };
    const bool overList = inputReady && mx >= listX - 24.0f * uiS
                       && mx < listX + listW + 28.0f * uiS
                       && my >= listTop && my < listBottom;
    const bool categoryInteractionReady = tabSwitchT >= 0.22f;
    const bool overRightList = inputReady && categoryInteractionReady
                            && mx >= rpX - 10.0f * uiS
                            && mx < rpX + rClickW
                            && my >= rightListTop && my < rightListBottom;
    // Let the wheel scroll the visible settings area, including the left
    // catalogue and empty space around shorter categories.
    const bool overSettingsCanvas = inputReady
                                 && mx >= listX - 30.0f * uiS
                                 && mx < rpX + rpW + 24.0f * uiS
                                 && my >= listTop && my < rightListBottom;
    constexpr float kWheelNotchPx = 106.0f;
    if (keyCapturing) g_ScrollAccum = 0.0f;
    if (overSettingsCanvas && g_ScrollAccum != 0.0f) {
        // One wheel notch moves a fixed, small pixel distance (about half a row)
        // on the right board, not a share of the whole range: the old share
        // grew with the page and jumped several rows per notch.
        if (rightMaxScroll > 0.0f)
            settingsScrollTarget -= g_ScrollAccum * (kWheelNotchPx * uiS) / rightMaxScroll;
        settingsScrollTarget = std::max(0.0f, std::min(1.0f, settingsScrollTarget));
        g_ScrollAccum = 0.0f;
        scrollFollowsBoard = true;
    }
    // While the board is wheel-scrolled, the left index follows the category
    // whose header is at (or has just passed) the top of the right board.
    // Clicks and W/S focus changes clear the flag via revealFocus, so their
    // own scroll animation is never overridden.
    if (scrollFollowsBoard && rightMaxScroll > 0.0f) {
        // Pick the category covering the largest share of the visible board.
        const float viewTop = settingsScrollTarget * rightMaxScroll;
        const float viewBottom = viewTop + rightListH;
        int followCategory = tab;
        float bestShare = -1.0f;
        for (int i = 0; i < rightListCount; ++i) {
            if (!rightList[i].header) continue;
            int end = i + 1;
            while (end < rightListCount && !rightList[end].header) ++end;
            const float share = std::min(viewBottom, (float)end * rightRowH)
                              - std::max(viewTop, (float)i * rightRowH);
            if (share > bestShare) {
                bestShare = share;
                followCategory = rightList[i].category;
            }
        }
        if (followCategory != tab) {
            tab = followCategory;
            detailRow = 0;
            listCursor = -1;    // re-resolved to the new category next frame
        }
    }
    // Hover and wheel scrolling never change the selected category.
    const bool listWKey = c.window && glfwGetKey(c.window, GLFW_KEY_W) == GLFW_PRESS;
    const bool listSKey = c.window && glfwGetKey(c.window, GLFW_KEY_S) == GLFW_PRESS;
    if (c.inputFocusChanged) {
        prevListW = listWKey;
        prevListS = listSKey;
    }
    const int cursorBeforeKeys = listCursor;
    if (!keyCapturing && (overList || overRightList) && listWKey && !prevListW) stepListCursor(-1);
    if (!keyCapturing && (overList || overRightList) && listSKey && !prevListS) stepListCursor(1);
    focusChanged = focusChanged || listCursor != cursorBeforeKeys;
    prevListW = listWKey;
    prevListS = listSKey;
    auto revealFocus = [&]() {
        scrollFollowsBoard = false;
        if (rightMaxScroll <= 0.0f) {
            settingsScrollTarget = 0.0f;
            return;
        }
        if (scrollToCategory >= 0) {
            for (int i = 0; i < rightListCount; ++i) {
                if (rightList[i].header
                    && rightList[i].category == scrollToCategory) {
                    settingsScrollTarget = std::max(0.0f, std::min(1.0f,
                        (float)i * rightRowH / rightMaxScroll));
                    break;
                }
            }
            return;   // cleared at the end of the frame, after every reveal
        }
        int rightFocus = -1;
        if (isListItem(listCursor)) {
            const SettingsListEntry& focused = list[listCursor];
            for (int i = 0; i < rightListCount; ++i) {
                if (!rightList[i].header && rightList[i].category == focused.category
                    && rightList[i].row == focused.row) {
                    rightFocus = i;
                    break;
                }
            }
        }
        if (rightFocus < 0) return;
        const float target = std::max(0.0f, std::min(rightMaxScroll,
            (float)rightFocus * rightRowH - rightListH * 0.42f));
        settingsScrollTarget = target / rightMaxScroll;
    };
    if (focusChanged) revealFocus();
    if (isListItem(listCursor)) {
        const SettingsListEntry& selectedEntry = list[listCursor];
        if (selectedEntry.category != tab || selectedEntry.row != detailRow) {
            activateSettingsCategory(selectedEntry.category,
                                      selectedEntry.row);
            focusChanged = true;
        }
    }
    if (focusChanged) revealFocus();

    // The left catalogue uses the same diagonal constellation language as the
    // PLAY trial list. Rows grow from one restrained  rail instead of sitting
    // on a rigid text column.
    BatchFlush();
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)(listX - 34.0f * uiS),
              (GLint)(sh - listBottom),
              (GLint)(listW + 72.0f * uiS),
              (GLint)(listBottom - listTop));
    LogoLine(listRailTopX, listTop - 8.0f * uiS,
             listRailBottomX, listBottom + 8.0f * uiS,
             0.82f * uiS, 0.34f, 0.72f, 1.0f, 0.24f * dA);
    LogoLine(listRailTopX + 13.0f * uiS, listTop - 8.0f * uiS,
             listRailBottomX + 13.0f * uiS, listBottom + 8.0f * uiS,
             0.40f * uiS, 0.34f, 0.72f, 1.0f, 0.08f * dA);
    DrawVisibleConstellNode(listRailTopX, listTop - 8.0f * uiS,
                            2.2f * uiS, 0.34f, 0.72f, 1.0f,
                            0.48f * dA, false, false);
    DrawVisibleConstellNode(listRailBottomX, listBottom + 8.0f * uiS,
                            2.2f * uiS, 0.34f, 0.72f, 1.0f,
                            0.48f * dA, false, false);
    // Hover/active plate for the category index. Its left edge lies on the
    // leftmost rail line and its right edge runs parallel to it, so both ends
    // lean with the rail; top and bottom sit on the row dividers. `width` is
    // measured from the rail and is always wide enough to cover the label. A
    // brighter bar closes the right end.
    auto slantPlate = [&](float yTop, float yBottom, float width,
                          float pr, float pg, float pb, float plateA, float edgeA) {
        if (width < 2.0f || plateA + edgeA <= 0.002f) return;
        BindMainShader();
        const float lt = listRailXAt(yTop), lb = listRailXAt(yBottom);
        BatchTri(lt, yTop, lt + width, yTop, lb + width, yBottom, pr, pg, pb, plateA);
        BatchTri(lt, yTop, lb + width, yBottom, lb, yBottom, pr, pg, pb, plateA);
        const float bar = 5.0f * uiS;
        BatchTri(lt + width - bar, yTop, lt + width, yTop, lb + width, yBottom,
                 0.40f, 0.78f, 1.0f, edgeA);
        BatchTri(lt + width - bar, yTop, lb + width, yBottom, lb + width - bar, yBottom,
                 0.40f, 0.78f, 1.0f, edgeA);
    };
    for (int i = 0; i < listCount; ++i) {
        const float y = listTop + listRowOff[i] - leftScroll;
        const float rowHt = listRowHt[i];
        // Rows of a folding category fade and slide in with their openness.
        const float rowOpen = list[i].header ? 1.0f : catOpen[list[i].category];
        if (rowOpen < 0.01f || y < listTop - listRowH || y > listBottom) {
            listHover[i] = UpdateMenuCommandHover(listHover[i], false, dt);
            continue;
        }
        const float dAr = dA * rowOpen * rowOpen;
        const float rowCenter = y + rowHt * 0.50f;
        const float railX = listRailXAt(rowCenter)
            - (1.0f - rowOpen) * 18.0f * uiS;
        const bool hov = overList && mx >= listX - 10.0f * uiS
                      && mx < listX + listW + 18.0f * uiS
                      && my >= y && my < y + rowHt
                      && (list[i].header || list[i].category == tab);
        if (hov && lmb && !g_LmbPrev) {
            int targetIndex = i;
            if (list[i].header) {
                // Clicking a collapsed category expands it on the next
                // frame; the first row becomes the keyboard/detail focus.
                activateSettingsCategory(list[i].category, 0);
                listCursor = -1;
                accordionFocusPending = true;
                focusChanged = true;
                continue;
            }
            if (!list[targetIndex].header) {
                listCursor = targetIndex;
                detailRow = list[targetIndex].row;
                focusChanged = true;
                if (tab != list[targetIndex].category) {
                    activateSettingsCategory(list[targetIndex].category,
                                             list[targetIndex].row);
                }
            }
        }
        listHover[i] = UpdateMenuCommandHover(listHover[i], hov, dt);
        if (list[i].header) {
            const float headerX = railX + 20.0f * uiS;
            const bool activeCategory = list[i].category == tab;
            const bool categoryHover = hov;
            {
                const float hv = listHover[i];
                // The plate slides in from the left along its slant as the
                // hover eases up; the active category keeps a resting plate.
                const float reach = std::max(hv, activeCategory ? 1.0f : 0.0f);
                // Wide enough for the whole label, never narrower than the
                // divider extent (headerX .. 78% of the list).
                const float labelEnd = headerX - railX
                    + g_TextS.Width(list[i].label, settingsScale(
                          g_TextS, 1.0f, UiTextLevel::Subtitle)) + 24.0f * uiS;
                const float fullW = std::max(labelEnd,
                                             listX + listW * 0.78f - railX);
                slantPlate(y - 2.0f * uiS, y + rowHt - 2.0f * uiS,
                           std::max(labelEnd, fullW * (0.55f + 0.45f * reach)),
                           0.12f, 0.42f, 0.72f,
                           std::max(activeCategory ? 0.10f : 0.0f, 0.13f * hv) * dAr,
                           (activeCategory ? 0.10f : 0.0f) * dAr + 0.34f * hv * dAr);
            }
            const float categoryScale = settingsScale(
                g_TextS, 1.0f, UiTextLevel::Subtitle);
            const float categoryH = g_TextS.Height(list[i].label, categoryScale);
            drawSettingsTextAtLevel(
                g_TextS, list[i].label, headerX,
                y + (rowHt - categoryH) * 0.5f, 1.0f,
                activeCategory || categoryHover ? 0.58f : 0.34f,
                activeCategory || categoryHover ? 0.86f : 0.72f,
                1.0f,
                (activeCategory ? 1.0f :
                 categoryHover ? 0.92f : 0.68f) * dAr,
                UiTextLevel::Subtitle, 0.56f);
            LogoLine(headerX, y + rowHt - 2.0f * uiS,
                     listX + listW * 0.78f, y + rowHt - 2.0f * uiS,
                     activeCategory ? 1.15f * uiS : 0.7f * uiS,
                     0.34f, 0.72f, 1.0f,
                     (activeCategory ? 0.38f : 0.16f) * dAr);
        } else {
            const bool changed = settingChanged(list[i].category, list[i].row);
            // Clicking is navigation only.  Persistent colour is reserved
            // for an actually changed value; hover remains transient.
            const float visual = std::max(listHover[i], changed ? 0.72f : 0.0f);
            const float itemX = railX + 22.0f * visual;
            const float nodeX = itemX + 5.0f * uiS;
            const float a = changed ? (0.86f + 0.14f * visual)
                                    : (0.42f + 0.20f * visual);
            const float r = changed ? 1.0f : 0.68f + 0.10f * visual;
            const float g = changed ? 0.68f : 0.76f + 0.12f * visual;
            const float b = changed ? 0.32f : 1.0f;
            if (listHover[i] > 0.02f) {
                const float hv = listHover[i];
                const float labelEnd = 42.0f * uiS
                    + g_TextL.Width(list[i].label, settingsScale(
                          g_TextL, 1.0f, UiTextLevel::Description)) + 24.0f * uiS;
                const float fullW = std::max(labelEnd,
                                             listX + listW * 0.78f - railX);
                slantPlate(y - 2.0f * uiS, y + rowHt - 2.0f * uiS,
                           std::max(labelEnd, fullW * (0.55f + 0.45f * hv)),
                           0.12f, 0.42f, 0.72f, 0.10f * hv * dAr, 0.26f * hv * dAr);
            }
            if (visual > 0.02f)
                DrawConstellationDisc(nodeX, rowCenter,
                                      (11.0f + 7.0f * visual) * uiS,
                                      r, g, b, 0.07f * visual * dAr);
            DrawVisibleConstellLine(railX, rowCenter, nodeX, rowCenter,
                                    0.56f * uiS, r, g, b,
                                    (0.08f + 0.22f * visual) * dAr);
            DrawVisibleConstellNode(nodeX, rowCenter,
                                    (2.8f + 1.0f * visual) * uiS,
                                    r, g, b, a * dAr,
                                    false, false);
            const float textA = changed
                ? (0.98f + 0.02f * visual)
                : (0.92f + 0.08f * visual);
            const float itemTextX = itemX + 20.0f * uiS;
            float itemTextScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Description);
            const float itemTextMaxW = std::max(
                1.0f, listX + listW - itemTextX - 8.0f * uiS);
            const float itemTextW = g_TextL.Width(list[i].label, itemTextScale);
            if (itemTextW > itemTextMaxW && itemTextW > 0.0f)
                itemTextScale *= itemTextMaxW / itemTextW;
            const float itemTextH = g_TextL.Height(list[i].label, itemTextScale);
            drawSettingsTextAtLevel(
                g_TextL, list[i].label, itemTextX,
                y + (rowHt - itemTextH) * 0.5f, 1.0f,
                1.0f, changed ? 0.68f : 1.0f,
                changed ? 0.32f : 1.0f, textA * dAr,
                UiTextLevel::Description, 0.62f);
        }
    }
    BatchFlush();
    glDisable(GL_SCISSOR_TEST);
    if (leftMaxScroll > 0.0f) {
        const float trackX = listX + listW + 12.0f * uiS;
        const float thumbH = std::max(28.0f * uiS, listH * (listH / leftTotalH));
        const float thumbY = listTop + (listH - thumbH) * settingsScroll;
        LogoLine(trackX, listTop, trackX, listBottom,
                 0.7f * uiS, 0.34f, 0.72f, 1.0f, 0.12f * dA);
        LogoLine(trackX, thumbY, trackX, thumbY + thumbH,
                 1.8f * uiS, 0.34f, 0.82f, 1.0f, 0.55f * dA);
    }


    // Left-category navigation never collapses the right settings board.
    int hoverCategory = -1;
    int hoverSettingRow = -1;
    int hoverSettingOpt = -1;
    bool resetPressed = false;
    bool keyChipClicked = false;
    const float rightRowX = rpX + 18.0f * uiS;
    // The scroll track sits in its own gutter at the board's right edge; row
    // containers and controls stop short of it so they never overlap the bar.
    const float rightTrackX = rpX + rClickW - 8.0f * uiS;
    const float rightRowRight = rightTrackX - 22.0f * uiS;
    const float rightRowW = rightRowRight - rightRowX;
    // Controls are a left-anchored column inside the settings panel. This
    // leaves a comfortable readout area while avoiding the old far-right,
    // centered-looking option placement.
    const float rightControlInset = std::min(
        rpW * 0.34f, std::max(0.0f, rClickW - 160.0f * uiS));
    // The whole control column sits a little left of the board edge.
    const float kControlShiftX = 52.0f * uiS;
    const float rightControlX = rpX + rightControlInset - kControlShiftX;
    const float rightControlEnd = rightRowRight - 18.0f * uiS - kControlShiftX;
    const float rightControlW = std::max(1.0f,
                                         rightControlEnd - rightControlX);
    auto categoryColor = [&](int category, float& r, float& g, float& b) {
        if (category == 0) { r = 0.35f; g = 0.72f; b = 1.00f; }
        else if (category == 1) { r = 1.00f; g = 0.72f; b = 0.30f; }
        else if (category == 2) { r = 0.35f; g = 0.92f; b = 0.55f; }
        else if (category == 3) { r = 1.00f; g = 0.45f; b = 0.62f; }
        else { r = 0.72f; g = 0.52f; b = 1.00f; }
    };

    // Every option row shares one control grid. Measuring each row on its own
    // made the second option drift whenever a label such as "부드럽게" was
    // wider than ON/OFF. Measure the longest localized label per slot once,
    // then reuse those columns for FPS, toggles, and language choices.
    auto optionLabelForLanguage = [&](int category, int row,
                                      int optionLanguage, int optionIndex)
                                      -> const wchar_t* {
        if (category == 0) {
            if (row == 0) {
                static const wchar_t* fps[3][6] = {
                    { L"동기화", L"30", L"60", L"144", L"300", L"무제한" },
                    { L"VSYNC", L"30", L"60", L"144", L"300", L"UNLIM" },
                    { L"同期", L"30", L"60", L"144", L"300", L"無制限" }
                };
                return fps[optionLanguage][optionIndex];
            }
            if (row == 1) {
                static const wchar_t* graphics[3][3] = {
                    { L"낮음", L"보통", L"높음" }, { L"LOW", L"MEDIUM", L"HIGH" },
                    { L"低", L"中", L"高" }
                };
                return graphics[optionLanguage][optionIndex];
            }
            if (row == 2) {
                return kToggleLabels[optionLanguage][optionIndex];
            }
        } else if (category == 1) {
            if (row == 1 || row == 2 || row == 4)
                return kToggleLabels[optionLanguage][optionIndex];
            if (row == 3) {
                static const wchar_t* output[3][2] = {
                    { L"스테레오", L"모노" },
                    { L"STEREO", L"MONO" },
                    { L"ステレオ", L"モノ" }
                };
                return output[optionLanguage][optionIndex];
            }
        } else if (category == 2 && row < 4) {
            return kToggleLabels[optionLanguage][optionIndex];
        } else if (category == 4 && row == 0) {
            static const wchar_t* language[] = {
                L"한국어", L"English", L"日本語", L"中文"
            };
            return language[optionIndex];
        }
        return nullptr;
    };

    auto localizedOptionWidth = [&](int category, int row, int optionIndex,
                                    float scale, const wchar_t* fallback) {
        const UiTextLevel level = settingsOptionTextLevel(category, row);
        float widest = settingsButtonTextWidth(fallback, scale, level);
        for (int language = 0; language < 3; ++language) {
            const wchar_t* localized = optionLabelForLanguage(
                category, row, language, optionIndex);
            if (localized)
                widest = std::max(widest,
                                  settingsButtonTextWidth(localized, scale,
                                                          level));
        }
        return widest;
    };

    const float commonOptionSc = 1.0f;
    const float commonChipPadX = 8.0f * uiS;
    const float commonChipGap = 12.0f * uiS;
    float commonOptionCellW[8] = {};
    int commonOptionCount = 0;
    for (int category = 0; category < 5; ++category) {
        for (int row = 0; row < 10; ++row) {
            if (category == 0 && row == 0) continue;
            const SRow& setting = settingsRows[category][row];
            if (setting.optCount <= 0) continue;
            commonOptionCount = std::max(commonOptionCount, setting.optCount);
            for (int j = 0; j < setting.optCount; ++j) {
                const float widest = localizedOptionWidth(
                    category, row, j, commonOptionSc, setting.opts[j]);
                commonOptionCellW[j] = std::max(
                    commonOptionCellW[j], widest + commonChipPadX * 2.0f);
            }
        }
    }
    // Every toggle/choice row uses one cell width so ON/OFF, LOW/MEDIUM/HIGH
    // and the FPS row share the same column rhythm (QA #1, #2).
    auto equalizeCommonCells = [&]() {
        const float widest = *std::max_element(std::begin(commonOptionCellW),
                                               std::end(commonOptionCellW));
        std::fill(std::begin(commonOptionCellW), std::end(commonOptionCellW),
                  widest);
    };
    equalizeCommonCells();
    float commonOptionTotalW = 0.0f;
    for (int j = 0; j < commonOptionCount; ++j)
        commonOptionTotalW += commonOptionCellW[j];
    if (commonOptionCount > 1)
        commonOptionTotalW += commonChipGap * (commonOptionCount - 1);
    float commonOptionScale = commonOptionSc;
    if (commonOptionTotalW > rightControlW && commonOptionTotalW > 1.0f) {
        commonOptionScale *= std::max(0.55f, rightControlW / commonOptionTotalW);
        std::fill(std::begin(commonOptionCellW), std::end(commonOptionCellW), 0.0f);
        for (int category = 0; category < 5; ++category) {
            for (int row = 0; row < 10; ++row) {
                if (category == 0 && row == 0) continue;
                const SRow& setting = settingsRows[category][row];
                for (int j = 0; j < setting.optCount; ++j) {
                    const float widest = localizedOptionWidth(
                        category, row, j, commonOptionScale, setting.opts[j]);
                    commonOptionCellW[j] = std::max(
                        commonOptionCellW[j], widest + commonChipPadX * 2.0f);
                }
            }
        }
        equalizeCommonCells();
    }

    // FPS and language rows used to size their own cells; they now share
    // the grid computed below and only keep their per-row storage.
    float fpsOptionScale = commonOptionSc;
    float fpsOptionCellW[8] = {};
    float languageOptionScale = commonOptionSc;
    float languageOptionCellW[8] = {};

    // One control grid for every option row (QA #37): FPS, toggles, quality
    // and language choices all start at the same x with the same cell width
    // and gap, sized for the widest label in any language and the six-slot
    // FPS row, so each column lines up down the whole board.
    {
        constexpr int kGridSlots = 6;
        float gridScale = commonOptionSc;
        auto measureGrid = [&](float scale) {
            float widest = 0.0f;
            for (int category = 0; category < 5; ++category) {
                for (int row = 0; row < 10; ++row) {
                    const SRow& setting = settingsRows[category][row];
                    for (int k = 0; k < setting.optCount; ++k)
                        widest = std::max(widest, localizedOptionWidth(
                            category, row, k, scale, setting.opts[k]));
                }
            }
            return widest + commonChipPadX * 2.0f;
        };
        float gridCellW = measureGrid(gridScale);
        const float gridTotalW = kGridSlots * gridCellW
                               + commonChipGap * (kGridSlots - 1);
        // Leave clear air before the scroll rail at the column's right edge.
        const float gridRoomW = std::max(1.0f, rightControlW - 28.0f * uiS);
        if (gridTotalW > gridRoomW && gridTotalW > 1.0f) {
            gridScale *= std::max(0.55f, gridRoomW / gridTotalW);
            gridCellW = measureGrid(gridScale);
        }
        commonOptionScale = fpsOptionScale = languageOptionScale = gridScale;
        std::fill(std::begin(commonOptionCellW), std::end(commonOptionCellW), gridCellW);
        std::fill(std::begin(fpsOptionCellW), std::end(fpsOptionCellW), gridCellW);
        std::fill(std::begin(languageOptionCellW), std::end(languageOptionCellW), gridCellW);
    }
    BatchFlush();
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)rpX,
              (GLint)(sh - rightListBottom),
              (GLint)rClickW,
              (GLint)(rightListBottom - rightListTop));
    for (int i = 0; i < rightListCount; ++i) {
        const float categoryOffset = (1.0f - categoryReveal) * 14.0f * uiS;
        const float y = rightListTop + i * rightRowH - rightScroll
                      + categoryOffset;
        if (y < rightListTop - rightRowH || y > rightListBottom) continue;

        const SettingsListEntry& entry = rightList[i];
        float catR = 0.35f, catG = 0.72f, catB = 1.0f;
        categoryColor(entry.category, catR, catG, catB);
        if (entry.header) {
            const bool headerHover = overRightList
                                  && mx >= rightRowX - 12.0f * uiS
                                  && mx < rightRowX + rightRowW
                                  && my >= y && my < y + rightRowH;
            // Clicking a board header neither scrolls nor changes the left
            // accordion; the board only follows wheel scrolling.
            (void)headerHover;
            const float categoryHeaderScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Subtitle);
            const float categoryHeaderH = g_TextL.Height(
                entry.label, categoryHeaderScale);
            drawSettingsTextAtLevel(g_TextL, entry.label, rightRowX,
                                    y + (rightRowH - categoryHeaderH) * 0.5f,
                                    1.0f,
                                    catR, catG, catB, 1.0f * dA,
                                    UiTextLevel::Subtitle, 0.56f);
            const float headerRuleY = y + rightRowH - 10.0f * uiS;
            LogoLine(rightRowX, headerRuleY,
                     rightRowRight - 18.0f * uiS, headerRuleY,
                     1.05f * uiS, catR, catG, catB, 0.34f * dA);
            DrawVisibleConstellNode(rightRowX, headerRuleY,
                                    2.6f * uiS, catR, catG, catB,
                                    0.62f * dA, false, false);
            continue;
        }

        const int category = entry.category;
        const int row = entry.row;
        SRow& setting = settingsRows[category][row];
        const bool changed = settingChanged(category, row);
        const bool itemHover = overRightList
                            && mx >= rightRowX - 12.0f * uiS
                            && mx < rightRowX + rightRowW
                            && my >= y && my < y + rightRowH;
        if (itemHover) {
            hoverCategory = category;
            hoverSettingRow = row;
        }
        rowHover[category][row] = UpdateMenuCommandHover(
            rowHover[category][row], itemHover && !setting.readOnly, dt);
        const float focus = rowHover[category][row];
        // While a key is being captured, every other row recedes (glyph alpha
        // only, no backdrop) so the waiting row reads as the active one.
        const bool isCaptureRow = category == 3 && setting.keyAction >= 0
                               && setting.keyAction == captureAction;
        const float captureDim = isCaptureRow ? 1.0f : 1.0f - 0.65f * captureBlend;
        const float rowA = dA * categoryReveal * captureDim * (0.72f + 0.28f * Smoothstep(
            LogoClamp01((rightListTop + rightListH - y) / std::max(1.0f, rightListH))));

        if (changed || focus > 0.01f) {
            const float bgA = changed ? 0.12f : 0.035f;
            drawRect(rightRowX, y + 4.0f * uiS, rightRowW,
                     rightRowH - 14.0f * uiS,
                     changed ? 0.52f : 0.20f,
                     changed ? 0.30f : 0.34f,
                     changed ? 0.08f : 0.44f,
                     bgA * rowA);
        }
        if (focus > 0.01f) {
            drawRect(rightRowX, y + rightRowH - 10.0f * uiS,
                     rightRowW, 1.0f,
                     0.34f, 0.72f, 1.0f, 0.16f * focus * rowA);
        }

        const float titleR = changed ? 1.0f : 0.82f;
        const float titleG = changed ? 0.68f : 0.86f;
        const float titleB = changed ? 0.32f : 1.0f;
        const bool compactFpsRow = category == 0 && row == 0;
        const bool compactLanguageRow = category == 4 && row == 0;
        const bool compactAlignedRow = compactFpsRow || compactLanguageRow;
        const float optSc = compactFpsRow ? fpsOptionScale
            : compactLanguageRow ? languageOptionScale : commonOptionScale;
        const UiTextLevel optionTextLevel = setting.optCount > 0
            ? settingsOptionTextLevel(category, row) : UiTextLevel::Title;
        const float settingLabelScale = settingsScale(
            g_TextL, 1.0f, UiTextLevel::Title);
        const float settingLabelH = g_TextL.Height(
            setting.label, settingLabelScale);
        // The label and its control occupy separate columns of the same row.
        // Center each text/control in that row rather than stacking the
        // control below the label inside a shared vertical content block.
        const float settingLabelY = y + (rightRowH - settingLabelH) * 0.5f;
        const float controlHitY = y;
        const float controlHitH = rightRowH;
        const float controlY = y + rightRowH * 0.5f;
        drawSettingsTextAtLevel(
            g_TextL, setting.label, rightRowX + 20.0f * uiS,
            settingLabelY, 1.0f, titleR, titleG, titleB,
            1.0f * rowA, UiTextLevel::Title, 0.66f);
        if (changed) {
            DrawConstellationDisc(rightRowX + 5.0f * uiS,
                                  y + rightRowH * 0.50f,
                                  13.0f * uiS, 1.0f, 0.56f, 0.18f,
                                  0.10f * rowA);
            DrawVisibleConstellNode(rightRowX + 5.0f * uiS,
                                    y + rightRowH * 0.50f, 3.6f * uiS,
                                    1.0f, 0.64f, 0.26f,
                                    0.88f * rowA, false, true);
        }

        if (setting.isVolume) {
            // Reserve the measured width of "100" so the readout ends at the
            // control column's edge instead of running into the scroll rail.
            const float volumeScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            const float volumeValueW = g_TextL.Width(L"100", volumeScale);
            // The knob is large, so leave room for its half-width on both ends.
            const float volumeValueGap = 30.0f * uiS;
            const float knobR = 17.0f * uiS;
            int& volValue = *setting.volRef;
            const float slX = rightControlX + knobR;
            const float slW = std::max(120.0f * uiS,
                rightControlW - volumeValueW - volumeValueGap - knobR);
            const float vol01 = volValue / 100.0f;
            const bool inBar = inputReady
                && mx >= slX - knobR - 4.0f * uiS
                && mx <= slX + slW + knobR + 4.0f * uiS
                && my >= controlY - 24.0f * uiS && my <= controlY + 24.0f * uiS;
            const int dragId = category * 16 + row;
            if (inputReady && lmb && ((inBar && s_volDrag < 0) || s_volDrag == dragId)) {
                s_volDrag = dragId;
                const float t = std::max(0.0f, std::min(1.0f,
                    ((float)mx - slX) / std::max(1.0f, slW)));
                volValue = (int)(t * 100.0f + 0.5f);
            }
            const float glow = s_volDrag == dragId ? 1.0f : focus;
            drawRect(slX, controlY - 2.0f * uiS, slW, 4.0f * uiS,
                     0.18f, 0.24f, 0.34f, 0.50f * rowA);
            drawRect(slX, controlY - 2.0f * uiS, slW * vol01, 4.0f * uiS,
                     0.48f, 0.82f, 1.0f, (0.60f + 0.28f * glow) * rowA);
            drawDiamond(slX + slW * vol01, controlY,
                        (knobR - 2.0f * uiS) + 3.0f * uiS * glow,
                        0.76f + 0.24f * glow, 0.90f + 0.10f * glow,
                        1.0f, (0.86f + 0.14f * glow) * rowA);
            wchar_t vbuf[8];
            swprintf_s(vbuf, L"%d", volValue);
            drawSettingsTextAtLevel(
                g_TextL, vbuf,
                slX + slW + volumeValueGap + volumeValueW
                    - g_TextL.Width(vbuf, volumeScale),
                controlY - g_TextL.Height(vbuf, volumeScale) * 0.5f,
                1.0f, 0.48f, 0.82f, 1.0f,
                (0.72f + 0.24f * glow) * rowA,
                UiTextLevel::Title, 0.58f);
        } else if (setting.isHold) {
            const float barW = std::min(250.0f * uiS, rightControlW);
            const float barH = 46.0f * uiS;
            const float barX = rightControlX;
            const float barY = controlY - barH * 0.5f;
            const bool resetHover = inputReady
                && mx >= barX && mx < barX + barW
                && my >= controlHitY && my < controlHitY + controlHitH;
            if (resetHover) {
                hoverCategory = category;
                hoverSettingRow = row;
                resetPressed = lmb;
            }
            const float showA = std::max(rowHover[category][row],
                                        holdT > 0.01f ? 1.0f : 0.0f);
            // Feathered rect: stacked layers shrink toward the centre so the
            // edge fades out instead of ending on a hard line.
            auto softRect = [&](float x, float y, float w, float h,
                                float r, float g, float b, float a, float feather) {
                constexpr int kLayers = 7;
                for (int i = 0; i < kLayers; ++i) {
                    const float e = feather * (0.5f - (float)i / (kLayers - 1));
                    const float lw = w + 2.0f * e, lh = h + 2.0f * e;
                    if (lw <= 0.0f || lh <= 0.0f) continue;
                    drawRect(x - e, y - e, lw, lh, r, g, b, a / (float)kLayers * 1.6f);
                }
            };
            softRect(barX, barY, barW, barH,
                     0.18f, 0.04f, 0.04f, 0.55f * showA * rowA, 10.0f * uiS);
            if (holdT > 0.01f) {
                const float fillW = (barW - 4.0f) * resetFill(holdT);
                softRect(barX + 2.0f, barY + 2.0f, fillW, barH - 4.0f,
                         1.0f, 0.22f, 0.14f, 0.92f * rowA, 8.0f * uiS);
            }
            {   // Corner brackets: faint at rest, widen and brighten on hover / hold.
                const float bo = (3.0f + 6.0f * showA) * uiS;
                const float arm = (12.0f + 6.0f * showA) * uiS;
                const float bw = 1.6f * uiS;
                const float ba = (0.42f + 0.50f * showA) * rowA;
                const float bx0 = barX - bo, bx1 = barX + barW + bo;
                const float by0 = barY - bo, by1 = barY + barH + bo;
                const float br = 1.0f, bg = 0.36f, bb = 0.30f;
                LogoLine(bx0, by0, bx0 + arm, by0, bw, br, bg, bb, ba);
                LogoLine(bx0, by0, bx0, by0 + arm, bw, br, bg, bb, ba);
                LogoLine(bx1, by0, bx1 - arm, by0, bw, br, bg, bb, ba);
                LogoLine(bx1, by0, bx1, by0 + arm, bw, br, bg, bb, ba);
                LogoLine(bx0, by1, bx0 + arm, by1, bw, br, bg, bb, ba);
                LogoLine(bx0, by1, bx0, by1 - arm, bw, br, bg, bb, ba);
                LogoLine(bx1, by1, bx1 - arm, by1, bw, br, bg, bb, ba);
                LogoLine(bx1, by1, bx1, by1 - arm, bw, br, bg, bb, ba);
            }
            static const wchar_t* resetLabelVariants[] = {
                L"길게 눌러 초기화", L"HOLD TO RESET", L"長押しでリセット"
            };
            static const wchar_t* erasingLabelVariants[] = {
                L"삭제 중...", L"ERASING...", L"削除中..."
            };
            static const wchar_t* doneLabelVariants[] = {
                L"초기화 완료", L"RESET COMPLETE", L"初期化完了"
            };
            const bool showDone = resetDoneT > 0.0f && (holdT <= 0.01f || exiting);
            const wchar_t* const* labelVariants = showDone ? doneLabelVariants
                : holdT > 0.01f ? erasingLabelVariants : resetLabelVariants;
            const wchar_t* resetLabel = labelVariants[std::clamp(LangIndex(), 0, 2)];
            float resetTextScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            const float widestLabel = MaxLocalizedTextWidth(
                g_TextL, labelVariants, 3, resetTextScale);
            const float resetTextMaxW = std::max(1.0f, barW - 24.0f * uiS);
            if (widestLabel > resetTextMaxW && widestLabel > 0.0f)
                resetTextScale *= resetTextMaxW / widestLabel;
            const float resetTextW = g_TextL.Width(resetLabel, resetTextScale);
            const float resetTextH = g_TextL.Height(resetLabel, resetTextScale);
            DrawShadowedText(g_TextL, resetLabel,
                             barX + (barW - resetTextW) * 0.5f,
                             controlY - resetTextH * 0.5f,
                             resetTextScale, 1.0f, 0.36f, 0.30f,
                             1.0f * rowA, 0.58f);
        } else if (setting.keyAction >= 0 || setting.isKeyReset) {
            // One outlined key chip (lines only). Waiting for a key swaps the
            // key name for a prompt and breathes the outline; a fresh bind
            // flashes it white.
            const bool isReset = setting.isKeyReset;
            const bool capturingThis = !isReset && captureAction == setting.keyAction;
            static const wchar_t* promptVariants[] = {
                L"키를 누르세요...", L"PRESS A KEY...", L"キーを押してください..." };
            static const wchar_t* resetVariants[] = { L"복원", L"RESET", L"リセット" };
            const float chipH = 46.0f * uiS;
            const float chipPad = 16.0f * uiS;
            const float chipTextScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            const float chipW = std::min(rightControlW, std::max(150.0f * uiS,
                MaxLocalizedTextWidth(g_TextL,
                    isReset ? resetVariants : promptVariants, 3, chipTextScale)
                + chipPad * 2.0f));
            const float chipX = rightControlX;
            const float chipY = controlY - chipH * 0.5f;
            const bool chipHover = overRightList
                && mx >= chipX && mx < chipX + chipW
                && my >= chipY && my < chipY + chipH;
            const float wave = 0.5f + 0.5f * std::sin(now * 5.0f);
            const float flash = isReset ? 0.0f : keyFlash[setting.keyAction];
            const float hoverT = rowHover[category][row];
            // Outline colour: category pink at rest, towards white when
            // waiting or just changed.
            const float whiteMix = std::max(flash, capturingThis ? 0.45f * wave : 0.0f);
            const float edgeR = catR + (1.0f - catR) * whiteMix;
            const float edgeG = catG + (1.0f - catG) * whiteMix;
            const float edgeB = catB + (1.0f - catB) * whiteMix;
            const float edgeA = std::max(
                capturingThis ? 0.55f + 0.45f * wave
                              : 0.34f + 0.40f * (chipHover ? 1.0f : hoverT),
                flash);
            const float edgeW = (capturingThis ? 1.8f : 1.2f) * uiS;
            LogoLine(chipX, chipY, chipX + chipW, chipY, edgeW,
                     edgeR, edgeG, edgeB, edgeA * rowA);
            LogoLine(chipX, chipY + chipH, chipX + chipW, chipY + chipH, edgeW,
                     edgeR, edgeG, edgeB, edgeA * rowA);
            LogoLine(chipX, chipY, chipX, chipY + chipH, edgeW,
                     edgeR, edgeG, edgeB, edgeA * rowA);
            LogoLine(chipX + chipW, chipY, chipX + chipW, chipY + chipH, edgeW,
                     edgeR, edgeG, edgeB, edgeA * rowA);
            if (capturingThis) {
                // A second, inset outline while waiting.
                const float inset = 5.0f * uiS;
                const float innerA = 0.22f * wave * rowA;
                LogoLine(chipX + inset, chipY + inset, chipX + chipW - inset,
                         chipY + inset, 0.8f * uiS, edgeR, edgeG, edgeB, innerA);
                LogoLine(chipX + inset, chipY + chipH - inset,
                         chipX + chipW - inset, chipY + chipH - inset,
                         0.8f * uiS, edgeR, edgeG, edgeB, innerA);
            }
            const std::wstring keyText = isReset ? std::wstring()
                : KeyName(g_KeyBinds.key[setting.keyAction]);
            const wchar_t* chipText = isReset
                ? resetVariants[std::clamp(LangIndex(), 0, 2)]
                : capturingThis ? promptVariants[std::clamp(LangIndex(), 0, 2)]
                                : keyText.c_str();
            float textScale = chipTextScale;
            const float textW0 = g_TextL.Width(chipText, textScale);
            const float textMaxW = std::max(1.0f, chipW - chipPad * 2.0f);
            if (textW0 > textMaxW && textW0 > 0.0f) textScale *= textMaxW / textW0;
            const float textW = g_TextL.Width(chipText, textScale);
            const float textH = g_TextL.Height(chipText, textScale);
            const float textA = capturingThis ? 0.55f + 0.45f * wave : 1.0f;
            DrawShadowedText(g_TextL, chipText,
                             chipX + (chipW - textW) * 0.5f,
                             controlY - textH * 0.5f, textScale,
                             0.82f + 0.18f * flash, 0.90f + 0.10f * flash,
                             1.0f, textA * rowA, 0.58f);

            if (chipHover && lmb && !g_LmbPrev) {
                keyChipClicked = true;
                if (isReset) {
                    ResetKeyBindings();
                    for (auto& f : keyFlash) f = 1.0f;
                    keyMsgT = 0.0f;
                    endKeyCapture();
                } else if (capturingThis) {
                    endKeyCapture();
                } else {
                    captureAction = setting.keyAction;
                    g_CapturedKey = -1;
                    g_KeyCaptureActive = true;
                    keyMsgT = 0.0f;
                }
            }
        } else if (setting.optCount > 0) {
            // Option cells are equal width within each row family and the
            // label is centered, so spacing never depends on label length.
            const float chipGap = commonChipGap;   // shared grid (QA #37)
            float ox = rightControlX;
            for (int j = 0; j < setting.optCount; ++j) {
                const float cellW = compactFpsRow ? fpsOptionCellW[j]
                    : compactLanguageRow ? languageOptionCellW[j]
                    : commonOptionCellW[j];
                const bool optionHover = inputReady
                    && mx >= ox && mx < ox + cellW
                    && my >= controlHitY && my < controlHitY + controlHitH;
                optHover[category][row][j] = UpdateMenuCommandHover(
                    optHover[category][row][j], optionHover, dt);
                if (optionHover) {
                    hoverCategory = category;
                    hoverSettingRow = row;
                    hoverSettingOpt = j;
                }
                const bool current = j == setting.optCur;
                const float optionHoverT = optHover[category][row][j];
                // The selected value must read as state, not just as a tiny
                // marker. Keep it near-white at full alpha; non-selected
                // values remain visible but recede until hovered.
                const float optionA = current
                    ? 1.0f
                    : 0.32f + 0.44f * optionHoverT;
                const float optionR = current
                    ? catR * 0.42f + 0.58f
                    : catR * (0.68f + 0.18f * optionHoverT);
                const float optionG = current
                    ? catG * 0.42f + 0.58f
                    : catG * (0.68f + 0.18f * optionHoverT);
                const float optionB = current
                    ? catB * 0.42f + 0.58f
                    : catB * (0.68f + 0.18f * optionHoverT);
                // DrawShadowedText takes the top of the glyph box. Lift the
                // labels to align their visual centers with the option hit row.
                const float optionScale = settingsScale(
                    g_TextL, optSc, optionTextLevel);
                const float optionTextY = controlY
                    - g_TextL.Height(setting.opts[j], optionScale) * 0.5f;
                const float optionTextW = g_TextL.Width(
                    setting.opts[j], optionScale);
                const float optionTextX = ox + (cellW - optionTextW) * 0.5f;
                drawSettingsTextAtLevel(g_TextL, setting.opts[j],
                                        optionTextX,
                                        optionTextY, optSc,
                                        optionR, optionG, optionB,
                                        optionA * rowA,
                                        optionTextLevel,
                                        current ? 0.44f : 0.30f);
                ox += cellW + chipGap;
            }
        } else if (setting.value) {
            const float valueScale = settingsScale(
                g_TextL, 1.0f, UiTextLevel::Title);
            drawSettingsTextAtLevel(
                g_TextL, setting.value, rightControlX,
                controlY - g_TextL.Height(setting.value, valueScale) * 0.5f,
                1.0f, 0.70f, 0.78f, 0.88f, 1.0f * rowA,
                UiTextLevel::Title, 0.58f);
        }
    }
    if (!lmb) s_volDrag = -1;
    // A click anywhere other than a key chip abandons the capture.
    if (captureAction >= 0 && lmb && !g_LmbPrev && !keyChipClicked)
        endKeyCapture();
    BatchFlush();
    glDisable(GL_SCISSOR_TEST);
    if (rightMaxScroll > 0.0f && rightListCount > 0) {
        const float rightTotalH = rightContentH;
        const float thumbH = std::max(32.0f * uiS,
                                      rightListH * (rightListH / rightTotalH));
        const float thumbY = rightListTop
            + (rightListH - thumbH) * settingsScroll;
        const float trackX = rightTrackX;
        LogoLine(trackX, rightListTop, trackX, rightListBottom,
                 0.7f * uiS, 0.34f, 0.72f, 1.0f, 0.12f * dA);
        LogoLine(trackX, thumbY, trackX, thumbY + thumbH,
                 1.8f * uiS, 0.34f, 0.82f, 1.0f, 0.55f * dA);
    }

    // Status line under the board: the cancel hint while waiting for a key,
    // or the result of the last bind (swap / reserved key).
    {
        const wchar_t* statusLine = nullptr;
        float sr = 1.0f, sg = 0.45f, sb = 0.62f, sa = 0.0f;
        const float statusWave = 0.5f + 0.5f * std::sin(now * 5.0f);
        if (keyMsgT > 0.0f) {
            statusLine = keyMsg.c_str();
            sr = 1.0f; sg = 0.78f; sb = 0.40f;
            sa = std::min(1.0f, keyMsgT * 2.0f);
        } else if (captureAction >= 0) {
            statusLine = SettingsText(
                L"ESC 또는 우클릭으로 취소",
                L"ESC OR RIGHT-CLICK TO CANCEL",
                L"ESC または右クリックでキャンセル");
            sa = 0.55f + 0.40f * statusWave;
        }
        if (statusLine)
            drawSettingsTextAtLevel(g_TextL, statusLine,
                                    rightRowX + 20.0f * uiS,
                                    rightListBottom + 14.0f * uiS, 1.0f,
                                    sr, sg, sb, sa * dA,
                                    UiTextLevel::Description, 0.60f);
    }

    if (focusChanged) revealFocus();
    scrollToCategory = -1;

    // Commit controls live below the detail readout. Changes are applied
    // provisionally while browsing, but only this button writes them.
    settingsDirty = settingsChanged();
    const float backX = BottomLeftActionX(uiS);
    const float backH = BackRailH(uiS);
    const float backY = BottomLeftActionY(sh, backH, uiS);
    const float backW = BackRailW(uiS);
    const float saveW = std::min(486.0f * uiS, detailW * 0.82f);
    const float saveX = detailX + detailW - saveW - 24.0f * uiS;
    const float saveY = backY;
    const float saveH = 64.0f * uiS;
    const bool backHit = ready && !confirmBack
                       && mx >= backX && mx < backX + backW
                       && my >= backY && my < backY + backH;
    const bool saveHit = ready && !confirmBack
                       && mx >= saveX && mx < saveX + saveW
                       && my >= saveY && my < saveY + saveH;

    static const wchar_t* settingsBackVariants[] = { L"뒤로", L"BACK", L"戻る" };
    static const wchar_t* settingsSaveVariants[] = {
        L"변경 저장", L"SAVE CHANGES", L"変更を保存"
    };
    const int settingsLanguage = std::max(0, std::min(2, LangIndex()));
    DrawUnifiedMenuCommand(settingsBackVariants[settingsLanguage],
                           backX, backY, backW, backH,
                           0.34f, 0.72f, 1.0f, treeA,
                           0.0f, backHit, 0.0f, now,
                           uiS, true, true, true,
                           settingsBackVariants, 3);
    DrawUnifiedMenuCommand(settingsSaveVariants[settingsLanguage],
                           saveX, saveY, saveW, saveH,
                           0.34f, 0.82f, 1.0f,
                           treeA * (settingsDirty ? 1.0f : 0.24f),
                           0.0f, saveHit && settingsDirty, 0.0f, now + 0.2f,
                           uiS, true, true, true,
                           settingsSaveVariants, 3);

    if (ready && !confirmBack && lmb && !g_LmbPrev && backHit) {
        if (settingsDirty) confirmBack = true;
        else exiting = true;
    }
    if (ready && !confirmBack && lmb && !g_LmbPrev && saveHit && settingsDirty) {
        SaveGame();
        savedSettings = CaptureSettingsSnapshot();
        savedSettingsValid = true;
        settingsDirty = false;
        pulse = 1.0f;
    }

    if (confirmBack) {
        const float cx = sw * 0.50f;
        const float cy = sh * 0.52f;
        drawRect(0.0f, 0.0f, sw, sh, 0.01f, 0.02f, 0.04f, 0.56f * treeA);
        const float dialogTitleScale = settingsScale(
            g_TextL, 1.0f, UiTextLevel::Title);
        const wchar_t* dialogTitle = SettingsText(
            L"저장하지 않은 변경 사항", L"UNSAVED CHANGES", L"未保存の変更");
        const float dialogTitleY = cy - 92.0f * uiS;
        drawSettingsTextAtLevel(
            g_TextL, dialogTitle,
            cx - g_TextL.Width(dialogTitle, dialogTitleScale) * 0.5f,
            dialogTitleY, 1.0f,
            1.0f, 1.0f, 1.0f, 0.98f * treeA,
            UiTextLevel::Title, 0.72f);
        const wchar_t* dialogPrompt = SettingsText(
            L"나가기 전에 설정을 저장할까요?", L"SAVE SETTINGS BEFORE EXIT?",
            L"終了する前に設定を保存しますか？");
        const float dialogPromptScale = settingsScale(
            g_TextL, 1.0f, UiTextLevel::Description);
        // Stack the prompt under the measured title height; the old fixed
        // offsets were tighter than a Title line and the two overlapped.
        drawSettingsTextAtLevel(
            g_TextL, dialogPrompt,
            cx - g_TextL.Width(dialogPrompt, dialogPromptScale) * 0.5f,
            dialogTitleY + g_TextL.Height(dialogTitle, dialogTitleScale) + 10.0f * uiS,
            1.0f,
            0.60f, 0.72f, 0.86f, 0.82f * treeA,
            UiTextLevel::Description, 0.58f);
        const float dialogY = cy + 26.0f * uiS;
        const float dialogW = 210.0f * uiS;
        const float dialogH = 48.0f * uiS;
        const bool saveBackHit = mx >= cx - dialogW - 12.0f * uiS
                              && mx < cx - 12.0f * uiS
                              && my >= dialogY && my < dialogY + dialogH;
        const bool discardHit = mx >= cx + 12.0f * uiS
                              && mx < cx + dialogW + 12.0f * uiS
                              && my >= dialogY && my < dialogY + dialogH;
        static const wchar_t* saveBackVariants[] = {
            L"저장 후 뒤로", L"SAVE & BACK", L"保存して戻る"
        };
        static const wchar_t* discardVariants[] = { L"저장 안 함", L"DON'T SAVE", L"保存しない" };
        DrawUnifiedMenuCommand(saveBackVariants[settingsLanguage],
                               cx - dialogW - 12.0f * uiS, dialogY,
                               dialogW, dialogH, 0.34f, 0.82f, 1.0f,
                               treeA, 0.0f, saveBackHit, 0.0f, now,
                               uiS, true, true, true,
                               saveBackVariants, 3);
        DrawUnifiedMenuCommand(discardVariants[settingsLanguage],
                               cx + 12.0f * uiS, dialogY,
                               dialogW, dialogH, 1.0f, 0.32f, 0.28f,
                               treeA, 0.0f, discardHit, 0.0f, now + 0.2f,
                               uiS, true, true, true,
                               discardVariants, 3);
        if (lmb && !g_LmbPrev && saveBackHit) {
            SaveGame();
            savedSettings = CaptureSettingsSnapshot();
            settingsDirty = false;
            confirmBack = false;
            exiting = true;
        } else if (lmb && !g_LmbPrev && discardHit) {
            restoreSettings();
            glfwSwapInterval(g_FpsCap == 0 ? 1 : 0);
            settingsDirty = false;
            confirmBack = false;
            exiting = true;
        }
    }

    // -- Click handling --------------------------------------------------
     // Apply option clicks using the category/row that owns the hovered
     // control; the left category index remains independent of this board.
     if (inputReady && lmb && !g_LmbPrev
         && hoverCategory >= 0 && hoverSettingRow >= 0
         && hoverSettingOpt >= 0) {
         const int category = hoverCategory;
         const int row = hoverSettingRow;
         const int option = hoverSettingOpt;
         pulseRow = row;
         pulse = 1.0f;
         if (category == 0) {
             if (row == 0) {
                 const int fps[] = { 0, 30, 60, 144, 300, -1 };
                 g_FpsCap = fps[option];
                 glfwSwapInterval(g_FpsCap == 0 ? 1 : 0);
             } else if (row == 1) {
                 g_VfxDensity = option == 0 ? VfxDensity::REDUCED :
                                option == 1 ? VfxDensity::MEDIUM : VfxDensity::FULL;
            } else if (row == 2) {
                g_ShowCombo = option == 0;
             }
         } else if (category == 1) {
             if (row == 3) {
                 Audio::SetMonoOutput(option == 1);
                 g_AudioMonoOutput = Audio::IsMonoOutput();
             } else if (row == 4) {
                 g_AudioEngineEnabled = option == 0;
                 Audio::SetEnabled(g_AudioEngineEnabled && g_SoundVol > 0);
             }
         } else if (category == 2) {
             if (row == 0) g_AutoSkill = option == 0;
             else if (row == 1) g_ShowCrosshair = option == 0;
             else if (row == 2) g_PanePhotos = option == 0;
             else if (row == 3) g_DebugMode = option == 0;
         } else if (category == 4 && row == 2) {
             showCredits = true;
         } else if (category == 4 && row == 0) {
             g_Language = option == 0 ? Language::KR : option == 1 ? Language::EN
                        : option == 2 ? Language::JP : Language::ZH;
         }
     }

     // RESET DATA is armed only while the visible reset control is held.
     resetDoneT = std::max(0.0f, resetDoneT - dt);
     if (!resetPressed) resetLatched = false;   // released: a new hold may start
     if (exiting && resetDoneT > 0.0f) {
         holdT = kResetHoldSec;   // committed: the bar stays full through the exit fade
     } else if (inputReady && resetPressed && !resetLatched) {
         holdT = std::min(holdT + dt, kResetHoldSec);
         if (holdT >= kResetHoldSec) {
             ResetSaveProgress();
             SaveGame();
             resetLatched = true;   // keep holding: no second fill until released
             resetDoneT = 2.5f;
             exiting = true;        // the normal exit transition returns to the lobby
         }
     } else if (inputReady) {
         holdT = std::max(0.0f, holdT - dt * 3.0f);
     } else {
         holdT = std::max(0.0f, holdT - dt * 4.0f);
     }
     g_UiShakePx = (holdT > 0.01f && !(exiting && resetDoneT > 0.0f))
         ? 9.0f * uiS * std::pow(holdT / kResetHoldSec, 2.5f) : 0.0f;

    // ── CREDITS page ────────────────────────────────────────────────────
    // Drawn over the whole board. Back (ESC / right-click) or a click closes it.
    if (showCredits) {
        BindMainShader();
        drawRect(0.0f, 0.0f, sw, sh, 0.004f, 0.010f, 0.026f, 0.90f);
        struct CreditLine { const wchar_t* text; int kind; };   // 0 heading, 1 name, 2 note, 3 gap
        const CreditLine lines[] = {
            { SettingsText(L"제작", L"TEAM PINATA", L"制作"), 0 },
            { L"AxisNox42", 1 }, { L"Rid", 1 },
            { L"", 3 },
            { SettingsText(L"사용한 소프트웨어", L"THIRD-PARTY SOFTWARE", L"使用ソフトウェア"), 0 },
            { L"GLFW   zlib / libpng", 1 }, { L"glad   MIT / public domain", 1 },
            { L"GLM   MIT", 1 }, { L"stb_image · stb_truetype   public domain / MIT", 1 },
            { L"miniaudio   public domain / MIT-0", 1 },
            { L"", 3 },
            { SettingsText(L"글꼴", L"FONTS", L"フォント"), 0 },
            { L"Chakra Petch · Orbit · Jua · Kosugi Maru · ZCOOL QingKe HuangYou", 1 },
            { L"SIL Open Font License 1.1", 2 },
            { L"", 3 },
            { SettingsText(L"ESC 또는 클릭으로 닫기", L"ESC OR CLICK TO CLOSE", L"ESCまたはクリックで閉じる"), 2 },
        };
        const float headSc = settingsScale(g_TextL, 1.0f, UiTextLevel::Subtitle);
        const float nameSc = settingsScale(g_TextL, 1.0f, UiTextLevel::Description);
        float total = 0.0f;
        for (const CreditLine& l : lines)
            total += l.kind == 3 ? 28.0f * uiS
                   : (l.kind == 0 ? g_TextL.Height(l.text, headSc) + 12.0f * uiS
                                  : g_TextL.Height(l.text, nameSc) + 8.0f * uiS);
        float y = std::max(60.0f * uiS, (sh - total) * 0.5f);
        const wchar_t* title = SettingsText(L"크레딧", L"CREDITS", L"クレジット");
        const float titleSc = settingsScale(g_TextL, 1.0f, UiTextLevel::Title);
        drawSettingsTextAtLevel(g_TextL, title, (sw - g_TextL.Width(title, titleSc)) * 0.5f,
                                std::max(24.0f * uiS, y - g_TextL.Height(title, titleSc) - 26.0f * uiS), 1.0f,
                                1.0f, 1.0f, 1.0f, 0.96f, UiTextLevel::Title, 0.70f);
        for (const CreditLine& l : lines) {
            if (l.kind == 3) { y += 28.0f * uiS; continue; }
            const float sc = l.kind == 0 ? headSc : nameSc;
            const float w = g_TextL.Width(l.text, sc);
            const float x = (sw - w) * 0.5f;
            if (l.kind == 0) {
                DrawShadowedText(g_TextL, l.text, x, y, sc, 0.48f, 0.82f, 1.0f, 0.96f, 0.7f);
                const float ry = y + g_TextL.Height(l.text, sc) + 4.0f * uiS;
                LogoLine(x, ry, x + w, ry, 1.0f * uiS, 0.34f, 0.72f, 1.0f, 0.30f);
                y += g_TextL.Height(l.text, sc) + 12.0f * uiS;
            } else {
                const float a = l.kind == 1 ? 0.92f : 0.62f;
                DrawShadowedText(g_TextL, l.text, x, y, sc, 0.90f, 0.95f, 1.0f, a, 0.7f);
                y += g_TextL.Height(l.text, sc) + 8.0f * uiS;
            }
        }
        creditsT += dt;
        if (lmb && !g_LmbPrev && creditsT > 0.20f) showCredits = false;
    } else {
        creditsT = 0.0f;
    }

    if (exiting && exitT >= kOutgameTransitionDuration) {
        // Settings are persisted only by SAVE CHANGES or SAVE & BACK.
        if ((g_LobbyPanel == LobbyPanel::Settings)) {
            g_LobbyPanel = LobbyPanel::None;
            s_MainMenuResumeFromPanel = true;
            g_MainMenuEntryT = 1.0f;
        } else {
            g_GameManager.currentState = GameState::PAUSED;
        }
        endKeyCapture();
        ResetSettingsUi();
        wasInline = false;
    }
}

void Scene_Settings(const SceneCtx& c) {
    Scene_SettingsInline(c);
}

