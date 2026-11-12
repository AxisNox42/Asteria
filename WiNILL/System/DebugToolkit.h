#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "SpatialBounds.h"
#include "GameManager.h"
#include "MonsterManager.h"
#include "PlayerStats.h"

// Runtime-only developer tooling. The toolkit deliberately owns no gameplay
// state; it edits the references supplied by the main loop and reports
// actions back through these callbacks.

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
};

class DebugToolkit {
public:
    void Configure(const DebugToolkitContext& context);

    // Returns true when the toolkit consumed the current frame's input. The
    // caller should skip normal scene/game input and simulation in that case.
    bool BeginInput(GLFWwindow* window, float screenW, float screenH,
                    double mouseX, double mouseY,
                    bool lmb, bool lmbPrev);

    void Render(float screenW, float screenH, GameState sceneState);

    bool IsVisible() const { return visible_; }
    void SetVisible(bool visible) {
        visible_ = visible;
        if (!visible_) ResetTransientInput();
    }
    bool IsInputCaptured() const { return visible_ || captureActive_; }
    bool SuppressOverlayForCapture() const {
        return captureActive_ || captureCurrent_;
    }

    void RequestCurrentScreenshot();
    void RequestAllScreenshots();
    // Captures the next frame as Screenshots/<name>.png (smoke sessions).
    void RequestNamedScreenshot(const std::wstring& name);

    bool CaptureInProgress() const { return captureActive_; }
    GameState CaptureRenderState(GameState restoreState) const;
    void CaptureFrameAfterRender(int width, int height, GameState renderedState);

    const std::wstring& LastMessage() const { return lastMessage_; }

private:
    enum class FieldType { Float, Int, Bool, LongLong };
    struct Field {
        const wchar_t* name;
        FieldType type;
        size_t offset;
    };

    DebugToolkitContext context_;
    bool configured_ = false;
    bool visible_ = false;
    int tab_ = 0;
    int fieldOffset_ = 0;
    int augOffset_ = 0;       // first visible row of the augment tab
    int selectedField_ = -1;
    int selectedMob_ = -1;
    int spawnCategory_ = 0;   // 0 regular, 1 elite, 2 boss
    int spawnCount_ = 1;

    bool editing_ = false;
    bool replaceEditBuffer_ = false;
    std::wstring editBuffer_;
    int editingRuntime_ = -1; // 0=HP, 1=level, 2=XP, 3=score

    bool captureCurrent_ = false;
    bool captureActive_ = false;
    size_t captureIndex_ = 0;
    std::wstring captureName_;
    std::wstring lastMessage_;

    static const std::vector<Field>& Fields();
    static const std::vector<GameState>& CaptureStates();
    static const wchar_t* FieldTypeLabel(FieldType type);
    static const wchar_t* MobLabel(int index);
    static const wchar_t* SceneLabel(GameState state);

    void ResetTransientInput();
    void BeginFieldEdit(int fieldIndex);
    void BeginRuntimeEdit(int runtimeIndex);
    void ApplyFieldEdit();
    void HandleNumericKeys(GLFWwindow* window);
    void RenderStatsTab(float x, float y, float w, float h,
                        float mouseX, float mouseY,
                        bool click);
    void RenderSpawnTab(float x, float y, float w, float h,
                        float mouseX, float mouseY,
                        bool click);
    void RenderAugmentTab(float x, float y, float w, float h);
    void RenderCaptureTab(float x, float y, float w, float h,
                          float mouseX, float mouseY,
                          bool click);
    void DrawButton(float x, float y, float w, float h,
                    const wchar_t* label, bool selected = false,
                    bool enabled = true) const;
    bool Hit(float x, float y, float w, float h,
             float mouseX, float mouseY) const;
    void SetMessage(const std::wstring& message);
};

extern DebugToolkit g_DebugToolkit;
