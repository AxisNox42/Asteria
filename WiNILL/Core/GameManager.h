#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cstring>
#include "MonsterManager.h"
#include "Bullet.h"
#include "Augment.h"
#include "Experience.h"
#include "LevelUpTransition.h"
#include "WaveSystem.h"

enum class GameState {
    MAIN_MENU,         // 시작 메뉴 (시작/설정/종료)
    CREATIVE_CONFIG,   // 크리에이티브 설정 (시작점수/시작증강)
    CODEX,             // 도감 (적/증강 발견 목록)
    TUTORIAL,          // 플레이 가이드 (페이지형)
    SETTINGS,          // 설정 화면
    READY, RUNNING, PAUSED, GAMEOVER, AUG_SELECT, DYING
};

class GameManager {
public:
    GameState currentState = GameState::MAIN_MENU;
    GameState lastState    = GameState::MAIN_MENU;
    GameState pauseResumeState = GameState::RUNNING;

    float     playerHP   = 100.0f;
    float     maxHP      = 100.0f;   // mirrors PlayerStats::maxHP for HUD
    int       currentWave = 1;
    float     waveStartTime = 0.0f;   // run clock when currentWave began
    long long waveKillStart = 0;      // run kill count when currentWave began
    int       waveKills = 0;          // kills this wave (regular waves' quota)
    int       waveTransitionFrom = 0;
    float     waveTransitionTimer = 0.0f;
    // The wave's target fell; the next wave starts once the reward is picked.
    bool      waveAdvancePending = false;
    float     waveClearDelay = 0.0f;      // beat between the kill and the reward
    bool      waveVictory = false;
    long long score      = 0;
    float     scoreAccum = 0.0f;

    XpAmount  xp           = 0;        // 현재 레벨 안의 EXP (0.001 XP 단위)
    int       playerLevel  = 1;        // 레벨 (시작 1)
    static constexpr int kAugChoiceMax = 3;   // cards per pick (card draw screen)
    int       augChoices[kAugChoiceMax] = {-1,-1,-1}; // indices into ALL_AUGS for current pick
    int       augChoiceCount = 0;
    int       augmentSelectionSerial = 0;
    bool      augmentKeyboardFocus = false;
    int       hoveredCard  = -1;       // AUG_SELECT 호버 인덱스 (-1=none)
    int       pendingAugmentRewards = 0;   // wave clears: augment cards
    // The wave clear's earned tier (AugRarity, -1 = none): the next augment
    // pick guarantees one card of it, and rewardSpeed (0..1) tilts the rest up.
    int       rewardTier = -1;
    float     rewardSpeed = 0.0f;
    int       rewardTierCap = 4;           // highest tier the pick may show (AugRarity)
    int       rewardPityCount = 0;         // fastest clears since the last lucky bump
    // The cards of the last two picks (kept for the run): a card that was shown and not taken is
    // left out of the next picks while other cards of its tier can fill the slot.
    int       recentOffers[6] = { -1, -1, -1, -1, -1, -1 };
    int       recentOfferNext = 0;
    int       pendingStatRewards = 0;      // level-ups: stat cards
    bool      activeRewardIsStat = false;  // the open pick is a level-up's
    bool      augmentRewardActive = false;
    LevelUpTransition levelUpTransition;  // slow-mo before the reward opens

    bool confirmPauseReleased = true;
    bool escReleased   = true;

    int screenW = 0, screenH = 0;

    GameManager() = default;
    ~GameManager();

    void Init(int sw, int sh);
    void ShutdownGlResources();
    void HandleInput(GLFWwindow* window);
    void UpdateStateSystem(MonsterManager& mm, std::vector<Bullet>& bullets);
    void ResetRunProgress(MonsterManager& mm, std::vector<Bullet>& bullets,
                          float initialHp = 100.0f,
                          long long initialScore = 0);
    void AddScore(float amount);
    // Wave-clear pick: augment cards by rarity weight.
    void PickRewardChoices();
    // Level-up pick: three different stat cards.
    void PickStatChoices();
    void QueueAugmentReward();
    void QueueStatReward();
    int  PendingRewards() const { return pendingAugmentRewards + pendingStatRewards; }
    bool ActivateNextAugmentReward();
    // The open reward pick was taken: drop it from its queue.
    void ConsumeActiveReward();
    // Opens the card screen outside the reward queue (the lobby tutorial).
    void OpenTutorialChoice();
    void ClearAugmentRewardQueue();
    bool ShouldUpdate()   const {
        return currentState == GameState::RUNNING ||
               currentState == GameState::DYING;
    }
    GameState GetState()  const { return currentState; }
    void Render();

private:
    unsigned int shaderProgram = 0;
    int projectionLoc = -1;
    int colorLoc = -1;
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int overlayVAO = 0, overlayVBO = 0;

    void initShaders();
    void initRenderData();
    void drawQuad(float x, float y, float w, float h, float r, float g, float b, float a);
    void setOrtho(int projLoc);
};
