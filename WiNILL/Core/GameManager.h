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
#include "RewardRules.h"

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
    // Elite wave (2026-11-23): its elite fell; the wave still waits for its kill quota.
    bool      eliteDown = false;
    float     eliteDownSeconds = 0.0f;    // the elite's kill time (the wave's reward speed)
    long long score      = 0;
    float     scoreAccum = 0.0f;

    XpAmount  xp           = 0;        // 현재 레벨 안의 EXP (0.001 XP 단위)
    int       playerLevel  = 1;        // 레벨 (시작 1)
    // Augment rework (2026-11-23, Core/RewardRules.h): a level-up offers new augments, a wave
    // clear upgrades owned ones (up to five cards), and the run opens with a frame pick.
    static constexpr int kAugChoiceMax = 5;   // cards per pick (card draw screen)
    int       augChoices[kAugChoiceMax] = {-1,-1,-1,-1,-1}; // indices into ALL_AUGS for current pick
    // Upgrade picks: the branch each slot offers (0 A, 1 B, 2 C; -1 on other picks). The same card
    // can sit in two slots with different branches.
    int       augChoiceBranch[kAugChoiceMax] = {-1,-1,-1,-1,-1};
    int       augChoiceCount = 0;
    int       augmentSelectionSerial = 0;
    bool      augmentKeyboardFocus = false;
    int       hoveredCard  = -1;       // AUG_SELECT 호버 인덱스 (-1=none)
    enum class RewardKind { NewCard, Upgrade, Frame };
    int       pendingAugmentRewards = 0;   // level-ups: new augment cards
    int       pendingUpgradeRewards = 0;   // wave clears: upgrades of owned cards
    int       pendingWaveNewCards = 0;     // wave clears with nothing to upgrade: a new augment instead
    bool      activeFromWave = false;      // the open new-augment pick is such a wave clear's
    bool      pendingFramePick = false;    // run start: one frame out of three
    bool      framePickOffered = false;    // this run's frame pick was queued (once per run)
    RewardKind activeRewardKind = RewardKind::NewCard;   // what the open pick is
    // Fastest-band wave clear (WaveRewardTier::Legendary): one more upgrade (RewardRules.h).
    int       rewardTier = -1;
    float     rewardSpeed = 0.0f;
    int       rewardTierCap = 4;           // kept for the logs (WAVE_CLEAR)
    int       rewardPityCount = 0;
    int       rewardLowStreak = 0;         // new-augment picks in a row with nothing Rare or better
    int       lastUpgraded = -1;           // the card upgraded last (always among the next upgrade cards)
    int       upgradeRerollsLeft = 0;      // rerolls left on the open upgrade pick
    // The cards of the last two picks (kept for the run): a card that was shown and not taken is
    // left out of the next picks while other cards of its tier can fill the slot.
    int       recentOffers[6] = { -1, -1, -1, -1, -1, -1 };
    int       recentOfferNext = 0;
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
    // Level-up pick: three new augments, their rarity by the player's level.
    void PickRewardChoices();
    // Wave-clear pick: up to five owned cards that can still be upgraded.
    void PickUpgradeChoices();
    // Run-start pick: three frames.
    void PickFrameChoices();
    // Legacy level-up stat cards (debug console only; no longer a reward).
    void PickStatChoices();
    void QueueAugmentReward();
    void QueueUpgradeReward(int count = 1);
    void QueueFramePick();
    // The open upgrade pick deals five new cards (once per pick). False when none is left.
    bool RerollUpgrade();
    bool CanRerollUpgrade() const;   // a reroll would show an open branch not on screen now
    void SetUpgradeOffers(const std::vector<reward::UpgradeOffer>& offers);
    void ResetCardScreenAfterReroll();
    bool ActiveIsUpgrade() const { return augmentRewardActive && activeRewardKind == RewardKind::Upgrade; }
    bool ActiveIsFrame() const { return augmentRewardActive && activeRewardKind == RewardKind::Frame; }
    bool ActiveIsWaveNewCard() const { return augmentRewardActive && activeRewardKind == RewardKind::NewCard && activeFromWave; }
    int  PendingRewards() const { return pendingAugmentRewards + pendingUpgradeRewards + (pendingFramePick ? 1 : 0); }
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
