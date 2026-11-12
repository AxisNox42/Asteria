#include "GameManager.h"
#include "Settings.h"
#include "PlayerStats.h"
#include "Weapons.h"
#include "GameContext.h"
#include "Scenes.h"
#include "ReadyLaunch.h"
#include "PassiveAugments.h"
#include "Input.h"
#include <algorithm>
#include <cstdlib>


static const char* gm_vert =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "uniform mat4 projection;\n"
    "void main() { gl_Position = projection * vec4(aPos, 0.0, 1.0); }\n";

static const char* gm_frag =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 color;\n"
    "void main() { FragColor = color; }\n";

GameManager::~GameManager() {
    ShutdownGlResources();
}

void GameManager::ShutdownGlResources() {
    if (VAO)         { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (VBO)         { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (EBO)         { glDeleteBuffers(1, &EBO); EBO = 0; }
    if (overlayVAO)  { glDeleteVertexArrays(1, &overlayVAO); overlayVAO = 0; }
    if (overlayVBO)  { glDeleteBuffers(1, &overlayVBO); overlayVBO = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
}

void GameManager::Init(int sw, int sh) {
    screenW = sw; screenH = sh;
    initShaders();
    initRenderData();
}

void GameManager::initShaders() {
    unsigned int v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &gm_vert, NULL);
    glCompileShader(v);

    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &gm_frag, NULL);
    glCompileShader(f);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, v);
    glAttachShader(shaderProgram, f);
    glLinkProgram(shaderProgram);
    glDeleteShader(v);
    glDeleteShader(f);

    projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    colorLoc = glGetUniformLocation(shaderProgram, "color");
}

void GameManager::initRenderData() {
    // Quad VAO (4 verts, EBO)
    float verts[] = { 0,0, 1,0, 1,1, 0,1 };
    unsigned int idx[] = { 0,1,2, 0,2,3 };
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Fullscreen NDC overlay VAO
    float ov[] = { -1,1, -1,-1, 1,-1, -1,1, 1,-1, 1,1 };
    glGenVertexArrays(1, &overlayVAO); glGenBuffers(1, &overlayVBO);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ov), ov, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void GameManager::setOrtho(int projLoc) {
    // Pixel-space ortho: (0,0) top-left, Y goes down
    float l = 0, r = (float)screenW, t = 0, b = (float)screenH;
    float m[16] = {
        2/(r-l), 0,       0,  0,
        0,       2/(t-b), 0,  0,
        0,       0,      -1,  0,
        -(r+l)/(r-l), -(t+b)/(t-b), 0, 1
    };
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, m);
}

void GameManager::drawQuad(float x, float y, float w, float h,
                           float r, float g, float b, float a) {
    float verts[] = { x,y, x+w,y, x+w,y+h, x,y+h };
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glUniform4f(colorLoc, r, g, b, a);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void GameManager::HandleInput(GLFWwindow* window) {
    // Confirm (READY launch) and Pause are bound keys (KeyBindings.h); they may
    // share one key since they act in different states. One press does one
    // thing, then both wait for the release.
    const bool confirmDown = ActionHeld(GameAction::Confirm);
    const bool pauseDown = ActionHeld(GameAction::Pause);
    int esc = glfwGetKey(window, GLFW_KEY_ESCAPE);

    const bool confirmHit = confirmDown && currentState == GameState::READY;
    const bool pauseHit = pauseDown && (currentState == GameState::RUNNING ||
                                        currentState == GameState::PAUSED);
    if ((confirmHit || pauseHit) && confirmPauseReleased) {
        if (confirmHit) {
            // Lobby tutorial: Space launches an Easy run with the loadout the
            // TUTORIAL button already set up (ESC opens the tutorial pause menu).
            // The practice wall squeezes and swells out first; the READY scene
            // starts the run when that animation ends. Locked until every
            // tutorial step is cleared.
            if (ready_launch::LaunchUnlocked())
                ready_launch::StartLaunch();
            else
                ready_launch::g_LaunchLockedT = ready_launch::kLaunchLockedSec;
        } else if (currentState == GameState::RUNNING) {
            // Pause key. Augment selection opens by itself after a level-up.
            pauseResumeState = GameState::RUNNING;
            currentState = GameState::PAUSED;
        } else if (currentState == GameState::PAUSED) {
            currentState = pauseResumeState;
            pauseResumeState = GameState::RUNNING;
        }
        // MAIN_MENU/SETTINGS 는 마우스 버튼으로만 진행
        confirmPauseReleased = false;
    }
    if (!confirmDown && !pauseDown) confirmPauseReleased = true;

    if (esc == GLFW_PRESS && escReleased) {
        if (!TryEscNavigateBack()) {
            if (currentState == GameState::RUNNING) {
                pauseResumeState = GameState::RUNNING;
                currentState = GameState::PAUSED;
            }
            else if (currentState == GameState::PAUSED) {
                currentState = pauseResumeState;
                pauseResumeState = GameState::RUNNING;
            }
        }
        escReleased = false;
    }
    if (esc == GLFW_RELEASE) escReleased = true;
}

void GameManager::UpdateStateSystem(MonsterManager& mm, std::vector<Bullet>& bullets) {
    if (lastState == GameState::GAMEOVER && currentState == GameState::READY) {
        ResetRunProgress(mm, bullets);
    }
    lastState = currentState;
}

void GameManager::ResetRunProgress(MonsterManager& mm,
                                   std::vector<Bullet>& bullets,
                                   float initialHp,
                                   long long initialScore) {
    playerHP = initialHp;
    maxHP = initialHp;
    score = initialScore;
    scoreAccum = static_cast<float>(initialScore);
    currentWave = 1;
    waveStartTime = 0.0f;
    waveKillStart = 0;
    waveKills = 0;
    waveTransitionFrom = 0;
    waveTransitionTimer = 0.0f;
    waveAdvancePending = false;
    waveClearDelay = 0.0f;
    rewardTier = -1;
    rewardSpeed = 0.0f;
    rewardTierCap = 4;
    rewardPityCount = 0;
    for (int& r : recentOffers) r = -1;
    recentOfferNext = 0;
    waveVictory = false;
    xp = 0;
    playerLevel = 1;
    g_PlayDebugTainted = g_DebugMode;
    bullets.clear();
    mm.Clear();
    lastState = GameState::READY;
}

// Rarity weight by run progress (0 = wave 1, 1 = final wave): the odds slide
// from mostly Common/Rare early to Epic/Legendary/Mythic late. Tiers above the
// wave's cap (set by how far the run has come) never show.
static float RarityWeight(AugRarity rarity, int cap, float progress) {
    if ((int)rarity > cap) return 0.0f;
    static const float kStart[5] = { 45.0f, 33.0f, 16.0f,  5.0f,  1.0f };
    static const float kEnd[5]   = {  3.0f, 14.0f, 32.0f, 33.0f, 18.0f };
    const int r = (int)rarity;
    if (r < 0 || r > 4) return 0.0f;
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return kStart[r] + (kEnd[r] - kStart[r]) * t;
}

// Three different cards: a rarity by weight (among rarities that still have
// an offerable card), then a card of that rarity at random. A wave clear's
// earned tier puts one card of that tier (or the nearest lower one with an
// offerable card) in a random slot, and its speed raises the other cards'
// non-common weights (up to x3).
void GameManager::PickRewardChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    bool used[AUG_TOTAL] = {};
    bool recent[AUG_TOTAL] = {};
    for (const int r : recentOffers) if (r >= 0 && r < AUG_TOTAL) recent[r] = true;
    const float luck = passive::LuckChanceMult() * (1.0f + 2.0f * std::clamp(rewardSpeed, 0.0f, 1.0f));
    int guaranteed = -1;
    const int cap = std::clamp(rewardTierCap, 0, (int)AugRarity::MYTHIC);
    const float progress = (float)(std::max(1, currentWave) - 1) /
                           (float)std::max(1, FinalWaveCount(g_Difficulty) - 1);
    // The guaranteed card may sit one tier above the cap (a lucky bump).
    for (int r = std::min(rewardTier, (int)AugRarity::MYTHIC); r >= 0 && guaranteed < 0; --r) {
        int pool[AUG_TOTAL], n = 0;
        for (int pass = 0; pass < 2 && n == 0; ++pass)   // pass 0 skips the cards shown last time
            for (int i = 0; i < AUG_TOTAL; ++i)
                if ((int)ALL_AUGS[i].rarity == r && !AugIsStatCard(ALL_AUGS[i]) && passive::CanOffer(i) &&
                    (pass == 1 || !recent[i]))
                    pool[n++] = i;
        if (n > 0) guaranteed = pool[std::rand() % n];
    }
    rewardTier = -1;
    rewardSpeed = 0.0f;
    rewardTierCap = (int)AugRarity::MYTHIC;
    if (guaranteed >= 0) used[guaranteed] = true;
    for (int pass = 0; pass < 2; ++pass) {
    if (pass == 1) {
        // Not enough fresh cards: the ones shown last time may come back.
        if (augChoiceCount + (guaranteed >= 0 ? 1 : 0) >= kAugChoiceMax) break;
        for (int i = 0; i < AUG_TOTAL; ++i) recent[i] = false;
    }
    for (int slot = augChoiceCount + (guaranteed >= 0 ? 1 : 0); slot < kAugChoiceMax; ++slot) {
        float weights[5] = {};
        float total = 0.0f;
        for (int r = 0; r < 5; ++r) {
            bool any = false;
            for (int i = 0; i < AUG_TOTAL && !any; ++i)
                any = !used[i] && !recent[i] && (int)ALL_AUGS[i].rarity == r &&
                      !AugIsStatCard(ALL_AUGS[i]) && passive::CanOffer(i);
            if (!any) continue;
            float w = RarityWeight((AugRarity)r, cap, progress);
            if (r != (int)AugRarity::COMMON) w *= luck;
            weights[r] = w;
            total += w;
        }
        if (total <= 0.0f) break;
        // MSVC's RAND_MAX is 32767, so `rand() % 100000` only reached the first
        // third of the weight sum and Rare and above almost never came up.
        float roll = (float)std::rand() / ((float)RAND_MAX + 1.0f) * total;
        int rarity = 0;
        for (; rarity < 4 && roll >= weights[rarity]; ++rarity) roll -= weights[rarity];
        while (weights[rarity] <= 0.0f && rarity > 0) --rarity;
        int pool[AUG_TOTAL], n = 0;
        for (int i = 0; i < AUG_TOTAL; ++i)
            if (!used[i] && !recent[i] && (int)ALL_AUGS[i].rarity == rarity &&
                !AugIsStatCard(ALL_AUGS[i]) && passive::CanOffer(i))
                pool[n++] = i;
        if (n == 0) break;
        const int pick = pool[std::rand() % n];
        used[pick] = true;
        augChoices[augChoiceCount++] = pick;
    }
    }
    if (guaranteed >= 0 && augChoiceCount < kAugChoiceMax) {
        const int at = std::rand() % (augChoiceCount + 1);
        for (int i = augChoiceCount; i > at; --i) augChoices[i] = augChoices[i - 1];
        augChoices[at] = guaranteed;
        ++augChoiceCount;
    }
    for (int i = 0; i < augChoiceCount; ++i) {          // remember this pick for the next two
        recentOffers[recentOfferNext] = augChoices[i];
        recentOfferNext = (recentOfferNext + 1) % 6;
    }
}

void GameManager::PickStatChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    int pool[AUG_TOTAL], n = 0;
    for (int i = 0; i < AUG_TOTAL; ++i)
        if (AugIsStatCard(ALL_AUGS[i])) pool[n++] = i;
    for (int k = 0; k < 3 && n > 0; ++k) {
        const int j = std::rand() % n;
        augChoices[augChoiceCount++] = pool[j];
        pool[j] = pool[--n];
    }
}

void GameManager::QueueAugmentReward() {
    ++pendingAugmentRewards;
}

void GameManager::QueueStatReward() {
    ++pendingStatRewards;
}

void GameManager::ConsumeActiveReward() {
    int& queue = activeRewardIsStat ? pendingStatRewards : pendingAugmentRewards;
    if (queue > 0) --queue;
}

static void ResetCardScreen(GameManager& gm) {
    gm.augmentKeyboardFocus = false;
    ++gm.augmentSelectionSerial;
    gm.currentState = GameState::AUG_SELECT;
    g_HoveredAug = -1;
    g_AugExitT = -1.0f;
    g_AugExitSlot = -1;
}

bool GameManager::ActivateNextAugmentReward() {
    // Opening a reward (or finding none) ends any level-up slow-motion.
    levelUpTransition.Cancel();
    if (augmentRewardActive) return true;
    // A wave clear's augment pick opens ahead of queued level-ups.
    while (pendingAugmentRewards > 0) {
        activeRewardIsStat = false;
        PickRewardChoices();
        if (augChoiceCount > 0) break;
        ConsumeActiveReward();   // nothing left to offer
    }
    if (pendingAugmentRewards <= 0) {
        if (pendingStatRewards <= 0) return false;
        activeRewardIsStat = true;
        PickStatChoices();
    }
    augmentRewardActive = true;
    {
        std::string cards;
        for (int i = 0; i < augChoiceCount; ++i) {
            const AugDef& d = ALL_AUGS[augChoices[i]];
            cards += (i ? "|" : "");
            cards += d.name;
            cards += ":";
            cards += AugRarityName(d.rarity);
        }
        gamelog::Event("AUG_OFFER", "reward=%s,cards=%s,pending_aug=%d,pending_stat=%d",
                       activeRewardIsStat ? "level_up" : "wave_clear", cards.c_str(),
                       pendingAugmentRewards, pendingStatRewards);
    }
    ResetCardScreen(*this);
    return true;
}

void GameManager::OpenTutorialChoice() {
    levelUpTransition.Cancel();
    PickStatChoices();          // the tutorial's level-up
    activeRewardIsStat = true;
    pendingStatRewards = 1;
    augmentRewardActive = true;
    ResetCardScreen(*this);
}

void GameManager::ClearAugmentRewardQueue() {
    pendingAugmentRewards = 0;
    pendingStatRewards = 0;
    activeRewardIsStat = false;
    levelUpTransition.Cancel();
    augmentRewardActive = false;
    augChoiceCount = 0;
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augmentKeyboardFocus = false;
}

void GameManager::AddScore(float amount) {
    if (currentState != GameState::RUNNING) return;
    scoreAccum += amount * TrialScoreMult();
    score = (long long)scoreAccum;
}

void GameManager::Render() {
    glEnable(GL_BLEND);
    // Alpha 채널도 올바르게 누적 (DWM이 프레임버퍼 알파로 투명도 결정)
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(shaderProgram);

    // --- 상태별 전체화면 어두운 오버레이 ---
    //   GAMEOVER 는 main.cpp 가 페이드인 딤을 직접 그림.
    //   MAIN_MENU 는 "진짜 바탕화면"을 비추기 위해 어둡게 덮지 않음.
    if (currentState != GameState::RUNNING && currentState != GameState::DYING &&
        currentState != GameState::PAUSED &&
        currentState != GameState::GAMEOVER &&
        currentState != GameState::AUG_SELECT &&   // the card scene eases its own dim
        currentState != GameState::MAIN_MENU &&
        !(currentState == GameState::SETTINGS &&
          g_SettingsReturnTo == GameState::PAUSED)) {
        float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, identity);

        float oR = 0.0f, oG = 0.0f, oB = 0.0f, overlayA = 0.5f;
        if (currentState == GameState::GAMEOVER)     { overlayA = 0.60f; }
        glUniform4f(colorLoc, oR, oG, oB, overlayA);
        glBindVertexArray(overlayVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // --- 픽셀 좌표계 UI ---
    setOrtho(projectionLoc);

    // (HP 바는 이제 플레이어 창 하단에 부착됨 — main.cpp 의 (c2) 참고. 여기선 안 그림)

    // (GAMEOVER 표시는 main.cpp [7] 텍스트 블록의 한국어 라벨이 담당)

    glBindVertexArray(0);
    glUseProgram(0);
}
