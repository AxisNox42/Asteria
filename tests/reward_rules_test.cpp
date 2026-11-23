// Augment rework (2026-11-23): the pure reward rules (Core/RewardRules.h) and the
// upgrade table (Entity/SynergyUpgrade.h). Run: scripts/test_reward_rules.ps1
#include "RewardRules.h"
#include "AugmentSynergy.h"
#include "SynergyUpgrade.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <string>
#include <vector>

static int g_fail = 0;
#define EXPECT(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); ++g_fail; } } while (0)

int main() {
    using namespace reward;

    // ── rarity cap and odds by level ──
    EXPECT(LevelTierCap(2) == 1 && LevelTierCap(4) == 1, "levels 2-4 top out at Rare");
    EXPECT(LevelTierCap(5) == 2 && LevelTierCap(7) == 2, "levels 5-7 top out at Epic");
    EXPECT(LevelTierCap(8) == 3 && LevelTierCap(10) == 3, "levels 8-10 top out at Legendary");
    EXPECT(LevelTierCap(11) == 4 && LevelTierCap(30) == 4, "Mythic from level 11");
    EXPECT(LevelProgress(2) == 0.0f && LevelProgress(kLevelForTopOdds) == 1.0f && LevelProgress(40) == 1.0f,
           "progress 0 at the first level-up, 1 at the top");
    for (int lv = 2; lv <= 20; ++lv)
        for (int r = 0; r < 5; ++r)
            if (r > LevelTierCap(lv)) EXPECT(RarityWeight(r, LevelTierCap(lv), LevelProgress(lv)) == 0.0f,
                                             "a rarity above the cap never weighs");

    // The alpha build bug (0.3.14 and before): `rand() % 100000` against MSVC's RAND_MAX
    // 32767 only reached the first third of the weight sum. Rolling over the full 0..1 range
    // (as GameManager does) must hit each rarity in proportion to its weight.
    {
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        for (int lv : { 3, 6, 9, 12, 14 }) {
            const int cap = LevelTierCap(lv);
            float w[5], total = 0.0f;
            for (int r = 0; r < 5; ++r) { w[r] = RarityWeight(r, cap, LevelProgress(lv)); total += w[r]; }
            int hits[5] = {};
            const int n = 100000;
            for (int k = 0; k < n; ++k) {
                float roll = u(rng) * total;
                int r = 0;
                for (; r < 4 && roll >= w[r]; ++r) roll -= w[r];
                while (w[r] <= 0.0f && r > 0) --r;
                ++hits[r];
            }
            for (int r = 0; r < 5; ++r) {
                const float want = w[r] / total, got = (float)hits[r] / (float)n;
                char msg[96];
                std::snprintf(msg, sizeof msg, "level %d rarity %d: %.3f vs weight %.3f", lv, r, got, want);
                EXPECT(std::fabs(got - want) < 0.02f, msg);
            }
            if (lv >= 5) EXPECT(hits[1] + hits[2] + hits[3] + hits[4] > n / 2, "past level 5 most cards are Rare or better");
        }
    }

    // ── streak guard ──
    EXPECT(!GuaranteeRare(0) && !GuaranteeRare(1) && GuaranteeRare(2), "two low picks in a row guarantee a Rare");
    EXPECT(NextLowStreak(1, 0) == 2 && NextLowStreak(2, 1) == 0 && NextLowStreak(0, 4) == 0, "streak counting");

    // ── upgrade picks ──
    EXPECT(UpgradePicks(false, false, 9) == 1, "regular wave: one upgrade");
    EXPECT(UpgradePicks(true, false, 9) == 2, "elite / boss wave: two upgrades");
    EXPECT(UpgradePicks(true, true, 9) == 3, "fastest clear: one more");
    EXPECT(UpgradePicks(true, true, 1) == 1 && UpgradePicks(false, false, 0) == 0, "never more than can be upgraded");
    {
        std::mt19937 rng(3);
        auto roll = [&](int n) { return (int)(rng() % (unsigned)n); };
        std::vector<int> cands = { 10, 11, 12, 13, 14, 15, 16, 17 };
        for (int k = 0; k < 200; ++k) {
            const std::vector<int> got = PickUpgradeCards(cands, 15, kUpgradeChoices, roll);
            EXPECT((int)got.size() == kUpgradeChoices, "five cards when there are enough");
            EXPECT(std::set<int>(got.begin(), got.end()).size() == got.size(), "no card twice");
            bool kept = false;
            for (int a : got) kept = kept || a == 15;
            EXPECT(kept, "the last upgraded card is always offered");
        }
        const std::vector<int> few = PickUpgradeCards({ 4, 5 }, -1, kUpgradeChoices, roll);
        EXPECT(few.size() == 2, "fewer cards than slots: all of them");
        const std::vector<int> gone = PickUpgradeCards({ 4, 5 }, 9, kUpgradeChoices, roll);
        EXPECT(gone.size() == 2, "a last card that cannot be upgraded any more is not forced in");
    }
    EXPECT(std::fabs(LevelDamageMult(1) - 1.0f) < 1e-6f && std::fabs(LevelDamageMult(14) - 1.26f) < 1e-4f,
           "+2% damage per level gained");

    // ── upgrade table ──
    int n = 0;
    const synergy::UpRow* rows = synergy::UpRows(n);
    std::set<std::string> codes;
    for (int i = 0; i < n; ++i) {
        EXPECT(codes.insert(rows[i].code).second, "one row per card");
        EXPECT(rows[i].a1 != rows[i].b1, "an upgrade changes its key number");
        EXPECT(synergy::FindCode(rows[i].code) >= 0, "every row names a real card");
    }
    const auto& t = synergy::Table();
    int upgradable = 0;
    for (size_t i = 1; i < t.size(); ++i) {
        const synergy::Aug& a = t[i];
        const std::string c = a.code;
        const bool none = a.role == synergy::Role::Frame || a.rar == synergy::Rar::Mythic ||
                          a.role == synergy::Role::Chance || c == "A_LASER_FOCUS" || c == "A_LASER_OVERLOAD";
        char msg[96];
        std::snprintf(msg, sizeof msg, "%s: a row exactly when it is upgraded by the table", a.code);
        EXPECT(none == (synergy::FindUp(a.code) == nullptr), msg);
        if (!none || a.role == synergy::Role::Chance) ++upgradable;
    }
    EXPECT(upgradable >= 90, "most of the pool can be upgraded");
    // Text for every row in every language.
    for (int i = 0; i < n; ++i)
        for (int lang = 0; lang < 3; ++lang)
            EXPECT(!synergy::UpPart(rows[i].l1, rows[i].u1, rows[i].a1, rows[i].b1, lang).empty(), "upgrade text");
    EXPECT(synergy::UpNumber(synergy::UpUnit::Pct, 0.12f, 0) == L"12%", "pct text");
    EXPECT(synergy::UpNumber(synergy::UpUnit::Sec, 0.45f, 0) == L"0.45초", "sec text");
    EXPECT(synergy::UpNumber(synergy::UpUnit::Ring, 2.25f, 1) == L"2.25○", "ring text");

    if (g_fail) { std::printf("%d failed\n", g_fail); return 1; }
    std::printf("reward rules and upgrade table passed (%d upgrade rows, %d upgradable cards)\n", n, upgradable);
    return 0;
}
