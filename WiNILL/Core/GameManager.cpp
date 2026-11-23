#include "GameManager.h"
#include "Settings.h"
#include "PlayerStats.h"
#include "Meta.h"          // shop First Card Boon
#include "Weapons.h"
#include "GameContext.h"
#include "Scenes.h"
#include "ReadyLaunch.h"
#include "PassiveAugments.h"
#include "SynergyRuntime.h"
#include "RewardRules.h"
#include "../System/GameLog.h"
#include "Input.h"
#include "../System/Audio.h"
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
            // While the tutorial's HUD guide is up, the press moves the guide on.
            // The launch card says "Space: launch", so its press closes it and launches.
            const ready_launch::HudGuide guideBefore = ready_launch::g_HudGuide;
            if (ready_launch::HudGuideConfirm()) {
                if (ready_launch::g_HudGuide != guideBefore) {
                    if (guideBefore == ready_launch::HudGuide::Launch && ready_launch::LaunchUnlocked())
                        ready_launch::StartLaunch();
                    else
                        Audio::Play(Audio::Cue::UiConfirm);
                }
            } else if (ready_launch::LaunchUnlocked())
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
    rewardLowStreak = 0;
    framePickOffered = false;
    eliteDown = false;
    eliteDownSeconds = 0.0f;
    lastUpgraded = -1;
    upgradeRerollsLeft = 0;
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

// ─── Reward picks (augment rework 2026-11-23, RewardRules.h) ──────────────────

static float Roll01() { return (float)std::rand() / ((float)RAND_MAX + 1.0f); }
static int RollN(int n) { return n > 0 ? std::min(n - 1, (int)(Roll01() * (float)n)) : 0; }

static bool NewCardOfferable(int i) {
    return !AugIsStatCard(ALL_AUGS[i]) && passive::CanOffer(i);
}

static void RememberOffer(GameManager& gm) {
    for (int i = 0; i < gm.augChoiceCount; ++i) {          // remember this pick for the next two
        gm.recentOffers[gm.recentOfferNext] = gm.augChoices[i];
        gm.recentOfferNext = (gm.recentOfferNext + 1) % 6;
    }
}

// Level-up: three different new augments. Each slot rolls a rarity by weight (the
// player's level sets the odds and the cap; only rarities that still have an offerable
// card count), then a card of that rarity at random. After two picks in a row with
// nothing Rare or better, one slot is Rare or better for sure (and the shop's First
// Card Boon does the same for the run's first pick).
void GameManager::PickRewardChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    bool used[AUG_TOTAL] = {};
    bool recent[AUG_TOTAL] = {};
    for (const int r : recentOffers) if (r >= 0 && r < AUG_TOTAL) recent[r] = true;
    const int level = std::max(2, playerLevel);
    const int cap = std::clamp(reward::LevelTierCap(level), 0, (int)AugRarity::MYTHIC);
    const float progress = reward::LevelProgress(level);
    const float luck = passive::LuckChanceMult();
    bool needRare = reward::GuaranteeRare(rewardLowStreak);
    if (g_MetaFirstCardPending) { g_MetaFirstCardPending = false; needRare = true; }

    auto pickOfRarity = [&](int minRarity, int maxRarity, bool skipRecent) {
        float weights[5] = {};
        float total = 0.0f;
        for (int r = minRarity; r <= maxRarity; ++r) {
            bool any = false;
            for (int i = 0; i < AUG_TOTAL && !any; ++i)
                any = !used[i] && !(skipRecent && recent[i]) && (int)ALL_AUGS[i].rarity == r && NewCardOfferable(i);
            if (!any) continue;
            float w = reward::RarityWeight(r, cap, progress);
            if (r != (int)AugRarity::COMMON) w *= luck;
            weights[r] = w;
            total += w;
        }
        if (total <= 0.0f) return -1;
        float roll = Roll01() * total;
        int rarity = minRarity;
        for (; rarity < maxRarity && roll >= weights[rarity]; ++rarity) roll -= weights[rarity];
        while (weights[rarity] <= 0.0f && rarity > minRarity) --rarity;
        int pool[AUG_TOTAL], n = 0;
        for (int i = 0; i < AUG_TOTAL; ++i)
            if (!used[i] && !(skipRecent && recent[i]) && (int)ALL_AUGS[i].rarity == rarity && NewCardOfferable(i))
                pool[n++] = i;
        return n > 0 ? pool[RollN(n)] : -1;
    };

    int guaranteed = -1;
    if (needRare)
        for (int pass = 0; pass < 2 && guaranteed < 0; ++pass)
            guaranteed = pickOfRarity((int)AugRarity::RARE, cap, pass == 0);
    if (guaranteed >= 0) used[guaranteed] = true;
    const int want = reward::kNewCardChoices - (guaranteed >= 0 ? 1 : 0);
    for (int pass = 0; pass < 2 && augChoiceCount < want; ++pass)   // pass 1: cards shown last time may come back
        while (augChoiceCount < want) {
            const int pick = pickOfRarity(0, cap, pass == 0);
            if (pick < 0) break;
            used[pick] = true;
            augChoices[augChoiceCount++] = pick;
        }
    if (guaranteed >= 0) {
        const int at = RollN(augChoiceCount + 1);
        for (int i = augChoiceCount; i > at; --i) augChoices[i] = augChoices[i - 1];
        augChoices[at] = guaranteed;
        ++augChoiceCount;
    }
    int best = -1;
    for (int i = 0; i < augChoiceCount; ++i) best = std::max(best, (int)ALL_AUGS[augChoices[i]].rarity);
    if (augChoiceCount > 0) rewardLowStreak = reward::NextLowStreak(rewardLowStreak, best);
    RememberOffer(*this);
}

// Wave clear: up to five owned cards that can still go up a stage, the last one
// upgraded always among them.
void GameManager::PickUpgradeChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    std::vector<int> cands;
    for (int i = 0; i < AUG_TOTAL; ++i)
        if (syn::CanUpgrade(i)) cands.push_back(i);
    const std::vector<int> picked = reward::PickUpgradeCards(cands, lastUpgraded,
                                                             std::min((int)kAugChoiceMax, reward::kUpgradeChoices), RollN);
    for (const int a : picked) augChoices[augChoiceCount++] = a;
}

// Run start: three frames.
void GameManager::PickFrameChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    syn::SetFramePick(true);
    int pool[AUG_TOTAL], n = 0;
    for (int i = 0; i < AUG_TOTAL; ++i)
        if (syn::IsFrame(i) && passive::CanOffer(i)) pool[n++] = i;
    syn::SetFramePick(false);
    for (int k = 0; k < reward::kFrameChoices && n > 0; ++k) {
        const int j = RollN(n);
        augChoices[augChoiceCount++] = pool[j];
        pool[j] = pool[--n];
    }
}

void GameManager::PickStatChoices() {
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    int pool[AUG_TOTAL], n = 0;
    // Glass Cannon fixes max HP at 30: Max HP Up would do nothing there.
    bool glass = false;
    for (int i = 0; i < AUG_TOTAL; ++i)
        if (ALL_AUGS[i].type == AugType::SYN_C_GLASS && passive::HeldCount(i) > 0) glass = true;
    for (int i = 0; i < AUG_TOTAL; ++i)
        if (AugIsStatCard(ALL_AUGS[i]) && !(glass && ALL_AUGS[i].type == AugType::S_MAX_HP)) pool[n++] = i;
    for (int k = 0; k < 3 && n > 0; ++k) {
        const int j = RollN(n);
        augChoices[augChoiceCount++] = pool[j];
        pool[j] = pool[--n];
    }
}

void GameManager::QueueAugmentReward() {
    ++pendingAugmentRewards;
}

void GameManager::QueueUpgradeReward(int count) {
    pendingUpgradeRewards += std::max(0, count);
}

void GameManager::QueueFramePick() {
    pendingFramePick = true;
}

bool GameManager::RerollUpgrade() {
    if (!ActiveIsUpgrade() || upgradeRerollsLeft <= 0) return false;
    // Five fresh cards where there are enough; the kept card stays (it is always offered).
    int shown[kAugChoiceMax];
    const int shownCount = augChoiceCount;
    for (int i = 0; i < shownCount; ++i) shown[i] = augChoices[i];
    std::vector<int> fresh, all;
    for (int i = 0; i < AUG_TOTAL; ++i) {
        if (!syn::CanUpgrade(i)) continue;
        all.push_back(i);
        bool was = false;
        for (int k = 0; k < shownCount; ++k) was = was || shown[k] == i;
        if (!was) fresh.push_back(i);
    }
    if (all.size() <= (size_t)shownCount && fresh.empty()) return false;   // nothing else to show
    std::vector<int> picked = reward::PickUpgradeCards(fresh, lastUpgraded, reward::kUpgradeChoices, RollN);
    // Too few fresh cards: fill with the ones shown before.
    for (int k = 0; k < shownCount && (int)picked.size() < reward::kUpgradeChoices; ++k)
        if (std::find(picked.begin(), picked.end(), shown[k]) == picked.end()) picked.push_back(shown[k]);
    if (lastUpgraded >= 0 && syn::CanUpgrade(lastUpgraded) &&
        std::find(picked.begin(), picked.end(), lastUpgraded) == picked.end()) {
        if ((int)picked.size() >= reward::kUpgradeChoices) picked.back() = lastUpgraded;
        else picked.push_back(lastUpgraded);
    }
    --upgradeRerollsLeft;
    for (int i = 0; i < kAugChoiceMax; i++) augChoices[i] = -1;
    augChoiceCount = 0;
    for (const int a : picked) if (augChoiceCount < kAugChoiceMax) augChoices[augChoiceCount++] = a;
    gamelog::Event("AUG_REROLL", "cards=%d,rerolls_left=%d", augChoiceCount, upgradeRerollsLeft);
    ResetCardScreenAfterReroll();
    return true;
}

void GameManager::ConsumeActiveReward() {
    switch (activeRewardKind) {
    case RewardKind::Frame:   pendingFramePick = false; break;
    case RewardKind::Upgrade: if (pendingUpgradeRewards > 0) --pendingUpgradeRewards; break;
    case RewardKind::NewCard: if (pendingAugmentRewards > 0) --pendingAugmentRewards; break;
    }
}

static void ResetCardScreen(GameManager& gm) {
    gm.augmentKeyboardFocus = false;
    ++gm.augmentSelectionSerial;
    gm.currentState = GameState::AUG_SELECT;
    g_HoveredAug = -1;
    g_AugExitT = -1.0f;
    g_AugExitSlot = -1;
}

void GameManager::ResetCardScreenAfterReroll() { ResetCardScreen(*this); }

bool GameManager::ActivateNextAugmentReward() {
    // Opening a reward (or finding none) ends any level-up slow-motion.
    levelUpTransition.Cancel();
    if (augmentRewardActive) return true;
    bool found = false;
    // The run-start frame pick opens first, then a wave clear's upgrades, then queued level-ups.
    if (pendingFramePick) {
        activeRewardKind = RewardKind::Frame;
        PickFrameChoices();
        if (augChoiceCount > 0) found = true;
        else pendingFramePick = false;   // every frame owned or none in the build
    }
    while (!found && pendingUpgradeRewards > 0) {
        activeRewardKind = RewardKind::Upgrade;
        PickUpgradeChoices();
        if (augChoiceCount > 0) { found = true; upgradeRerollsLeft = reward::kUpgradeRerolls; break; }
        // Nothing to upgrade (an early clear, or every card at its top stage): a new augment instead.
        --pendingUpgradeRewards;
        ++pendingAugmentRewards;
    }
    while (!found && pendingAugmentRewards > 0) {
        activeRewardKind = RewardKind::NewCard;
        PickRewardChoices();
        if (augChoiceCount > 0) { found = true; break; }
        ConsumeActiveReward();   // nothing left to offer
    }
    if (!found) return false;
    augmentRewardActive = true;
    {
        std::string cards;
        for (int i = 0; i < augChoiceCount; ++i) {
            const AugDef& d = ALL_AUGS[augChoices[i]];
            cards += (i ? "|" : "");
            cards += d.name;
            cards += ":";
            cards += AugRarityName(d.rarity);
            if (activeRewardKind == RewardKind::Upgrade) { cards += ":S"; cards += std::to_string(syn::Stage(augChoices[i])); }
        }
        static const char* const kKind[3] = { "level_up", "wave_upgrade", "run_frame" };
        gamelog::Event("AUG_OFFER", "reward=%s,cards=%s,level=%d,pending_new=%d,pending_up=%d",
                       kKind[(int)activeRewardKind], cards.c_str(), playerLevel,
                       pendingAugmentRewards, pendingUpgradeRewards);
    }
    ResetCardScreen(*this);
    return true;
}

void GameManager::OpenTutorialChoice() {
    levelUpTransition.Cancel();
    PickRewardChoices();        // the tutorial's level-up: new augments, as in a run
    activeRewardKind = RewardKind::NewCard;
    pendingAugmentRewards = 1;
    augmentRewardActive = true;
    ResetCardScreen(*this);
}

void GameManager::ClearAugmentRewardQueue() {
    pendingAugmentRewards = 0;
    pendingUpgradeRewards = 0;
    pendingFramePick = false;
    activeRewardKind = RewardKind::NewCard;
    upgradeRerollsLeft = 0;
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
