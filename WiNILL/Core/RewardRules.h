#pragma once

// ─── Reward rules (augment structure rework, 2026-11-23) ─────────────────────
// Pure rules for the three reward picks, kept free of game state so the tests
// can check them without linking the game:
//   * run start  : one frame out of three (the bare gun among them);
//   * level-up   : one new augment out of three, its rarity set by the level;
//   * wave clear : take one of three upgrade offers (different cards, drawn by
//                  weight; a card just upgraded comes up less), with one reroll.
// Docs/QA/2026-11-17_구조분석/Asteria_증강구조_개선안_2026-11-17.docx, 2·3·8장.

#include <algorithm>
#include <cmath>
#include <vector>

namespace reward {

inline constexpr int kNewCardChoices = 3;      // a level-up's new augments
inline constexpr int kUpgradeChoices = 3;      // a wave clear's upgrade offers (card, branch)
inline constexpr int kFrameChoices = 3;        // the run-start frame pick
inline constexpr int kUpgradeRerolls = 1;      // per upgrade pick

// Highest rarity (0 Common .. 4 Mythic) a level-up may offer. A run of about
// 12~14 level-ups: Rare tops the first three, then Epic, Legendary, and Mythic
// from level 11.
inline int LevelTierCap(int level) {
    return level < 5 ? 1 : level < 8 ? 2 : level < 11 ? 3 : 4;
}

// 0 at the first level-up (level 2) .. 1 at level 14: slides the rarity odds
// from mostly Common/Rare to Epic and above.
inline constexpr int kLevelForTopOdds = 14;
inline float LevelProgress(int level) {
    return std::clamp((float)(level - 2) / (float)(kLevelForTopOdds - 2), 0.0f, 1.0f);
}

// Rarity weight by progress (0..1); rarities above the cap never show.
inline float RarityWeight(int rarity, int cap, float progress) {
    if (rarity < 0 || rarity > 4 || rarity > cap) return 0.0f;
    static const float kStart[5] = { 45.0f, 33.0f, 16.0f,  5.0f,  1.0f };
    static const float kEnd[5]   = {  3.0f, 14.0f, 32.0f, 33.0f, 18.0f };
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return kStart[rarity] + (kEnd[rarity] - kStart[rarity]) * t;
}

// Streak guard: after this many new-augment picks in a row with nothing of
// Rare or better, the next pick holds one Rare-or-better card for sure.
inline constexpr int kLowStreakGuard = 2;
inline bool GuaranteeRare(int lowStreak) { return lowStreak >= kLowStreakGuard; }
// The streak after a pick whose best card had `bestRarity`.
inline int NextLowStreak(int lowStreak, int bestRarity) {
    return bestRarity >= 1 ? 0 : lowStreak + 1;
}

// Upgrades a wave clear pays: always one (2026-11-24: no extra pick for elite / boss waves
// or a fast clear). None when nothing can be upgraded.
inline int UpgradePicks(int upgradable) { return upgradable > 0 ? 1 : 0; }

// An upgrade offer: one branch (0 A, 1 B, 2 C) of one owned card. Branches can be taken
// again and again (2026-11-24).
struct UpgradeOffer {
    int card = -1, branch = 0;
    bool operator==(const UpgradeOffer& o) const { return card == o.card && branch == o.branch; }
};

// A card's weight in the upgrade draw: halved by every upgrade it already has, so a card
// just upgraded comes up less (100% → 50% → 25% …).
inline float UpgradeWeight(int upgrades) { return std::pow(0.5f, (float)std::max(0, upgrades)); }

// Up to `count` offers out of `candidates` (the open branches of the owned cards):
//   * three or more cards: `count` different cards, drawn by weight, one random branch each;
//   * fewer cards: each card once, then the slots left go to another open branch of a card
//     drawn by weight (one card alone shows its three branches).
// `weightOf(card)` gives a card's weight, `roll01()` a number in [0, 1).
template <class Weight, class Roll01>
std::vector<UpgradeOffer> PickUpgradeOffers(const std::vector<UpgradeOffer>& candidates, int count,
                                            Weight&& weightOf, Roll01&& roll01) {
    std::vector<UpgradeOffer> out;
    if (count <= 0 || candidates.empty()) return out;
    // Open branches per card.
    std::vector<int> cards;
    std::vector<std::vector<int>> branches;
    for (const UpgradeOffer& o : candidates) {
        size_t k = 0;
        while (k < cards.size() && cards[k] != o.card) ++k;
        if (k == cards.size()) { cards.push_back(o.card); branches.emplace_back(); }
        branches[k].push_back(o.branch);
    }
    auto rollIndex = [&](int n) { return std::min(n - 1, (int)(roll01() * (float)n)); };
    auto takeBranch = [&](size_t k) {
        std::vector<int>& open = branches[k];
        const int j = rollIndex((int)open.size());
        out.push_back({ cards[k], open[(size_t)j] });
        open.erase(open.begin() + j);
    };
    auto drawCard = [&](const std::vector<size_t>& pool) {
        float total = 0.0f;
        for (size_t k : pool) total += std::max(1e-6f, weightOf(cards[k]));
        float r = roll01() * total;
        for (size_t k : pool) {
            r -= std::max(1e-6f, weightOf(cards[k]));
            if (r < 0.0f) return k;
        }
        return pool.back();
    };
    if ((int)cards.size() >= count) {
        std::vector<size_t> pool;
        for (size_t k = 0; k < cards.size(); ++k) pool.push_back(k);
        while ((int)out.size() < count && !pool.empty()) {
            const size_t k = drawCard(pool);
            takeBranch(k);
            pool.erase(std::find(pool.begin(), pool.end(), k));
        }
    } else {
        for (size_t k = 0; k < cards.size(); ++k) takeBranch(k);
        while ((int)out.size() < count) {
            std::vector<size_t> pool;
            for (size_t k = 0; k < cards.size(); ++k) if (!branches[k].empty()) pool.push_back(k);
            if (pool.empty()) break;
            takeBranch(drawCard(pool));
        }
        // Shuffle so the extra branch is not always last.
        for (int i = (int)out.size() - 1; i > 0; --i) std::swap(out[(size_t)i], out[(size_t)rollIndex(i + 1)]);
    }
    return out;
}

// Level-ups no longer offer stat cards; each one adds a small, automatic
// damage step instead (the new augment and the upgrades carry the growth).
inline constexpr float kLevelDamageStep = 0.02f;   // +2% damage per level gained
inline float LevelDamageMult(int level) {
    return 1.0f + kLevelDamageStep * (float)std::max(0, level - 1);
}

}  // namespace reward
