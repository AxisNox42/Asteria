#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "SpatialBounds.h"
#include "GameManager.h"
#include "MonsterManager.h"
#include "PlayerStats.h"

class DebugUI;

// Runtime-only developer tooling (F1, debug mode only). The toolkit owns no
// gameplay state; it edits the references supplied by the main loop and reports
// actions back through these callbacks.
//
// Layout: a panel docked on the right edge with a tab rail (소환 / 증강 / 능력치 /
// 스모크). Each tab is one function run in an input pass and a draw pass
// (DebugUI.h), split across DebugTab*.cpp.

struct DebugToolkitContext {
    PlayerStats* stats = nullptr;
    GameManager* game = nullptr;
    MonsterManager* monsters = nullptr;
    SpatialBounds* playerBounds = nullptr;
    float* windowSizeCur = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;
    bool* balanceTestMode = nullptr;

    std::function<void()> syncRuntime;
    std::function<int(int mobKind, int count)> spawnMob;
    std::function<int(int count)> spawnRanged;
    // Elite-kill hit stop experiment toggle (Stats tab); not saved.
    bool* eliteKillHitStop = nullptr;
    // PLAY constellation orbit rings toggle (Stats tab); not saved.
    bool* constellationOrbits = nullptr;
    std::function<void()> advanceWave;
    // Boss category of the spawn tab: 0 NORMA, 1 CAELUM (returns how many came in).
    std::function<int(int boss)> spawnBoss;
    // NORMA's test window (Boss category); true when one opened.
    std::function<bool()> openTestPane;
    // Augment tab: takes the card as if it were picked (returns false when refused).
    std::function<bool(int augIdx)> grantAugment;
    // Stats tab: the player dies at once (ignores godmode and revives).
    std::function<void()> killPlayer;
    // Spawn > 테스트: damage dummies in front of the player / remove them all.
    std::function<int(int count)> spawnDummy;
    std::function<int()> clearDummies;
    // Spawn tab: where hand-spawned mobs appear (0 arena edge, 1 ring around the
    // player, 2 the point set by setSpawnPoint).
    int* spawnPlacement = nullptr;
    // Spawn tab > 필드: every mob and bullet gone (no kills, no rewards).
    std::function<void()> clearField;
    // Spawn tab > 필드: 1 shield, 2 speed, 4 stealth on every live mob (0 clears).
    std::function<void(int mask)> buffMobs;
    // Smoke tab: the player's save file, copied into the smoke run's own save folder.
    std::function<std::string()> saveFilePath;
    // Spawn tab: a click on the field (screen pixels) becomes the spawn point.
    std::function<void(float screenX, float screenY)> setSpawnPoint;
};

class DebugToolkit {
public:
    enum class Tab { Spawn, Augment, Stats, Smoke, Count };

    void Configure(const DebugToolkitContext& context);

    // Returns true when the toolkit consumed the current frame's input. The
    // caller should skip normal scene/game input in that case, and freeze the
    // simulation only while PausesGame() is true.
    bool BeginInput(GLFWwindow* window, float screenW, float screenH,
                    double mouseX, double mouseY,
                    bool lmb, bool lmbPrev);

    void Render(float screenW, float screenH, GameState sceneState);

    bool IsVisible() const { return visible_; }
    void SetVisible(bool visible) {
        visible_ = visible;
        if (!visible_) {
            ResetTransientInput();
            textFocus_ = TextFocus::None;
            viewer_ = -1;
            armedMob_ = -1;
        }
    }
    // The game stands still while the console is open (header toggle, default on).
    bool PausesGame() const { return visible_ && pauseGame_; }
    bool IsInputCaptured() const { return (visible_ && inputCaptured_) || captureActive_; }
    bool SuppressOverlayForCapture() const {
        return (captureActive_ || captureCurrent_) && !captureKeepOverlay_;
    }
    bool CapturePending() const { return captureActive_ || captureCurrent_; }

    void RequestCurrentScreenshot();
    void RequestAllScreenshots();
    // Captures the next frame as Screenshots/<name>.png (smoke sessions).
    // keepOverlay: the console panel stays in the shot (smoke steps with F1 open).
    void RequestNamedScreenshot(const std::wstring& name, bool keepOverlay = false);

    bool CaptureInProgress() const { return captureActive_; }
    GameState CaptureRenderState(GameState restoreState) const;
    void CaptureFrameAfterRender(int width, int height, GameState renderedState);

    const std::wstring& LastMessage() const { return lastMessage_; }

    // Text boxes (증강 search, 스모크 custom steps). Typed text comes from
    // Input.cpp's char / IME hooks and goes to whichever box has focus.
    bool AugSearchActive() const { return visible_ && textFocus_ != TextFocus::None; }
    void AugSearchInsert(const wchar_t* text);
    void AugSearchBackspace();
    void AugSearchSetComposition(const wchar_t* text) { textComp_ = text ? text : L""; }
    void AugSearchClearComposition() { textComp_.clear(); }
    bool AugSearchComposing() const { return !textComp_.empty(); }
    // Esc / Enter while a box is focused (Enter handled next BeginInput).
    void AugSearchEscape();
    void AugSearchEnter() { textEnterPending_ = true; }
    // Smoke: click row `row` of the 증강 list (right = right click).
    void SmokeAugClick(int row, bool right) {
        const auto& list = AugVisibleList();
        if (row >= 0 && augOffset_ + row < (int)list.size())
            ClickAugmentFromConsole(list[(size_t)(augOffset_ + row)], right);
    }
    // Smoke: open the 증강 tab with this query typed into a focused box.
    void SmokeAugSearch(const std::wstring& query) {
        tab_ = Tab::Augment;
        collapsed_ = false;
        textFocus_ = TextFocus::AugSearch;
        augSearch_ = query;
        textComp_.clear();
        lastMessage_.clear();
    }

private:
    enum class FieldType { Float, Int, Bool, LongLong };
    enum class TextFocus { None, AugSearch, SmokeSteps };
    struct Field {
        const wchar_t* name;
        FieldType type;
        size_t offset;
        double step;    // − / + amount
        int group;      // 0 공격, 1 생존, 2 이동·크기, 3 성장
    };
    struct Thumb {
        std::wstring file;     // under Screenshots/
        GLuint tex = 0;
        int w = 0, h = 0;
        bool tried = false;    // finished (loaded or failed)
        std::shared_ptr<void> job;   // decode running on a worker thread
    };

    DebugToolkitContext context_;
    bool configured_ = false;
    bool visible_ = false;
    bool collapsed_ = false;
    bool pauseGame_ = true;
    bool inputCaptured_ = false;
    Tab tab_ = Tab::Spawn;
    std::array<float, (size_t)Tab::Count> pageScroll_ = {};
    std::array<float, (size_t)Tab::Count> pageContentH_ = {};
    float lastMx_ = -10000.0f, lastMy_ = -10000.0f;
    bool rmbPrev_ = false;
    GameState lastState_ = GameState::MAIN_MENU;

    // Text boxes.
    TextFocus textFocus_ = TextFocus::None;
    std::wstring textComp_;        // IME preedit of the focused box
    bool textEnterPending_ = false;
    void DrawTextBox(DebugUI& ui, float x, float y, float w, float h,
                     const std::wstring& text, const wchar_t* placeholder,
                     TextFocus which);
    std::wstring* TextOf(TextFocus which);

    // 증강 tab.
    int augOffset_ = 0;
    int augFilter_ = 0;
    bool augHeldOnly_ = false;
    std::wstring augSearch_;
    std::vector<int> augVisible_;
    std::wstring augVisibleQuery_;
    int augVisibleFilter_ = -1;
    bool augVisibleHeld_ = false;
    std::array<std::vector<int>, 3> augSets_;   // saved held sets (this session only)
    const std::vector<int>& AugVisibleList();
    std::wstring AugSearchQuery() const {
        return augSearch_ + (textFocus_ == TextFocus::AugSearch ? textComp_ : std::wstring());
    }
    void GrantAugmentFromConsole(int aug);
    void ClickAugmentFromConsole(int aug, bool rightClick);
    bool AugEditableNow();
    int ReleaseAllAugments();

    // 소환 tab.
    int spawnCategory_ = 0;   // 0 regular, 1 elite, 2 boss, 3 test
    int spawnCount_ = 1;
    // Mob waiting for a field click (-1 none). Clicks outside the panel spawn it
    // there; right click / Esc / the same card again / another tab cancel.
    int armedMob_ = -1;
    bool escPrev_ = false;
    // Holding the button and dragging keeps spawning, one batch per kDragSpacing.
    bool dragSpawn_ = false;
    float dragLastX_ = 0.0f, dragLastY_ = 0.0f;
    double scriptedLmbT_ = -1.0;   // smoke-scripted presses (no real button state)
    void SpawnTarget(int id);
    void SpawnArmedAt(float screenX, float screenY);

    // 능력치 tab.
    int selectedField_ = -1;
    bool editing_ = false;
    bool replaceEditBuffer_ = false;
    std::wstring editBuffer_;
    int editingRuntime_ = -1; // 0=HP, 1=level, 2=XP, 3=score
    std::vector<double> fieldDelta_;   // what the console added to each field this run
    double ReadField(const Field& f) const;
    void WriteField(const Field& f, double v);
    void NudgeField(int index, double amount);

    // 스모크 tab.
    struct SmokeRun;
    SmokeRun* smoke_ = nullptr;
    std::wstring smokeSteps_;
    int smokeLang_ = 0;   // KR, EN, JP
    std::wstring smokeStatus_;
    std::vector<Thumb> thumbs_;
    bool thumbsDirty_ = true;
    std::wstring thumbsPrefix_;   // empty = latest files of the folder
    int viewer_ = -1;             // thumb shown large, -1 none
    GLuint viewerTex_ = 0;
    int viewerW_ = 0, viewerH_ = 0;
    void StartSmoke(const std::wstring& steps);
    void PollSmoke();
    void RefreshThumbs();
    void FreeThumbs();
    void LoadOneThumb();
    void OpenScreenshotFolder();

    // Capture.
    bool captureCurrent_ = false;
    bool captureKeepOverlay_ = false;   // named shot that keeps the console panel
    bool captureActive_ = false;
    size_t captureIndex_ = 0;
    std::wstring captureName_;
    std::wstring lastMessage_;

    static const std::vector<Field>& Fields();
    static const std::vector<GameState>& CaptureStates();
    static const wchar_t* MobLabel(int index);
    static const wchar_t* SceneLabel(GameState state);

    void ResetTransientInput();
    void BeginFieldEdit(int fieldIndex);
    void BeginRuntimeEdit(int runtimeIndex);
    void ApplyFieldEdit();
    void HandleNumericKeys(GLFWwindow* window);

    // One frame of the console, shared by both passes.
    void RunUI(DebugUI& ui, float screenW, float screenH, GameState scene);
    void TabSpawn(DebugUI& ui);
    void TabAugment(DebugUI& ui, float bottom);
    void TabStats(DebugUI& ui);
    void TabSmoke(DebugUI& ui);
    void DrawViewer(float screenW, float screenH, float panelX);
    void SetMessage(const std::wstring& message);
};

extern DebugToolkit g_DebugToolkit;
