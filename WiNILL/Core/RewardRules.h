#pragma once

// ─── Reward rules (augment structure rework, 2026-11-23) ─────────────────────
// Pure rules for the three reward picks, kept free of game state so the tests
// can check them without linking the game:
//   * run start  : one frame out of three (the bare gun among them);
//   * level-up   : one new augment out of three, its rarity set by the level;
//   * wave clear : take one of three upgrade branches of the cards you hold
//                  (two picks on an elite or boss wave), with one reroll.
// Docs/QA/2026-11-17_구조분석/Asteria_증강구조_개선안_2026-11-17.docx, 2·3·8장.

#include <algorithm>
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

// Upgrades a wave clear pays: one, two on an elite or boss wave, one more for
// the fastest clear band. Never more than the cards that can still be upgraded.
inline int UpgradePicks(bool specialWave, bool fastest, int upgradable) {
    const int n = (specialWave ? 2 : 1) + (fastest ? 1 : 0);
    return std::max(0, std::min(n, upgradable));
}

// An upgrade offer: one branch (0 A, 1 B, 2 C) of one owned card (2026-11-23: every card
// has three branches, each taken once).
struct UpgradeOffer {
    int card = -1, branch = 0;
    bool operator==(const UpgradeOffer& o) const { return card == o.card && branch == o.branch; }
};
inline constexpr int kMaxOffersPerCard = 2;   // when more than one card can be upgraded

// Up to `count` offers out of `candidates` (every branch still open). One branch of the
// card upgraded last (`lastCard`, -1 = none) is among them when it has one left, so a
// build that digs into one card is not left to luck. With several cards, one card shows at
// most kMaxOffersPerCard branches while others can fill the slots; with a single card all
// of its branches show. `roll(n)` returns 0..n-1.
template <class Roll>
std::vector<UpgradeOffer> PickUpgradeOffers(std::vector<UpgradeOffer> candidates, int lastCard, int count, Roll&& roll) {
    std::vector<UpgradeOffer> out;
    if (count <= 0 || candidates.empty()) return out;
    auto perCard = [&](int card) {
        int n = 0;
        for (const UpgradeOffer& o : out) n += o.card == card ? 1 : 0;
        return n;
    };
    auto take = [&](size_t j) { out.push_back(candidates[j]); candidates.erase(candidates.begin() + (long)j); };
    {
        std::vector<size_t> mine;
        for (size_t j = 0; j < candidates.size(); ++j)
            if (lastCard >= 0 && candidates[j].card == lastCard) mine.push_back(j);
        if (!mine.empty()) take(mine[(size_t)roll((int)mine.size())]);
    }
    for (int pass = 0; pass < 2 && (int)out.size() < count; ++pass) {
        // pass 0 keeps to kMaxOffersPerCard per card; pass 1 fills what is left with anything.
        while ((int)out.size() < count) {
            std::vector<size_t> open;
            for (size_t j = 0; j < candidates.size(); ++j)
                if (pass == 1 || perCard(candidates[j].card) < kMaxOffersPerCard) open.push_back(j);
            if (open.empty()) break;
            take(open[(size_t)roll((int)open.size())]);
        }
    }
    // The kept offer takes a random slot rather than always the first.
    if (out.size() > 1) {
        const int at = roll((int)out.size());
        std::swap(out[0], out[(size_t)at]);
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
