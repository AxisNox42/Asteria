// Augment rework (2026-11-23): the pure reward rules (Core/RewardRules.h) and the
// upgrade table (Entity/SynergyUpgrade.h). Run: scripts/test_reward_rules.ps1
#include "RewardRules.h"
#include "AugmentSynergy.h"
#include "SynergyUpgrade.h"

#include <cmath>
#include <cwchar>
#include <cwctype>
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

    // ── upgrade picks (2026-11-24: one per wave clear, repeatable branches, weighted cards) ──
    EXPECT(UpgradePicks(9) == 1 && UpgradePicks(1) == 1 && UpgradePicks(0) == 0, "one upgrade per wave clear");
    EXPECT(UpgradeWeight(0) == 1.0f && UpgradeWeight(1) == 0.5f && UpgradeWeight(2) == 0.25f, "weight halves per upgrade");
    {
        std::mt19937 rng(3);
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        auto roll01 = [&]() { return u01(rng); };
        auto flat = [](int) { return 1.0f; };
        auto all3 = [](int card) { std::vector<UpgradeOffer> v; for (int b = 0; b < 3; ++b) v.push_back({ card, b }); return v; };
        // One card: its three branches.
        const std::vector<UpgradeOffer> one = PickUpgradeOffers(all3(7), kUpgradeChoices, flat, roll01);
        EXPECT(one.size() == 3, "a single card shows its three branches");
        // Two cards: both shown, the third slot another branch of one of them.
        std::vector<UpgradeOffer> twoC = all3(3); for (const UpgradeOffer& o : all3(4)) twoC.push_back(o);
        for (int k = 0; k < 200; ++k) {
            const std::vector<UpgradeOffer> two = PickUpgradeOffers(twoC, kUpgradeChoices, flat, roll01);
            std::set<int> cards, keys;
            for (const UpgradeOffer& o : two) { cards.insert(o.card); keys.insert(o.card * 4 + o.branch); }
            EXPECT(two.size() == 3 && cards.size() == 2 && keys.size() == 3, "two cards: both shown, no offer twice");
        }
        // Six cards A..F: three different cards every time.
        std::vector<UpgradeOffer> six;
        for (int c = 10; c < 16; ++c) for (const UpgradeOffer& o : all3(c)) six.push_back(o);
        for (int k = 0; k < 500; ++k) {
            const std::vector<UpgradeOffer> got = PickUpgradeOffers(six, kUpgradeChoices, flat, roll01);
            std::set<int> cards;
            for (const UpgradeOffer& o : got) cards.insert(o.card);
            EXPECT(got.size() == 3 && cards.size() == 3, "three different cards when three or more are owned");
        }
        // Weight: a card upgraded once shows up about as often as the model says (B at 50%, others 100%).
        auto weighted = [](int card) { return card == 11 ? UpgradeWeight(1) : 1.0f; };
        int seenB = 0, seenC = 0;
        const int runs = 20000;
        for (int k = 0; k < runs; ++k) {
            const std::vector<UpgradeOffer> got = PickUpgradeOffers(six, kUpgradeChoices, weighted, roll01);
            bool b = false, c = false;
            for (const UpgradeOffer& o : got) { b = b || o.card == 11; c = c || o.card == 12; }
            seenB += b; seenC += c;
        }
        char msg[96];
        std::snprintf(msg, sizeof msg, "an upgraded card comes up less (%.2f vs %.2f)", (float)seenB / runs, (float)seenC / runs);
        EXPECT(seenB < seenC * 0.75f && seenB > seenC * 0.35f, msg);
        std::printf("offers: 6 cards, one upgraded once: shown %.0f%% of picks vs %.0f%% for a fresh card\n",
                    100.0f * seenB / runs, 100.0f * seenC / runs);
    }
    EXPECT(std::fabs(LevelDamageMult(1) - 1.0f) < 1e-6f && std::fabs(LevelDamageMult(14) - 1.26f) < 1e-4f,
           "+2% damage per level gained");

    // ── upgrade table ──
    int n = 0;
    const synergy::UpRow* rows = synergy::UpRows(n);
    std::set<std::string> codes;
    for (int i = 0; i < n; ++i) {
        char msg[128];
        std::snprintf(msg, sizeof msg, "%s: one row, real card, A and B change, a family", rows[i].code);
        EXPECT(codes.insert(rows[i].code).second, msg);
        EXPECT(synergy::FindCode(rows[i].code) >= 0, msg);
        EXPECT(rows[i].A.on() && rows[i].A.a != rows[i].A.b, msg);
        EXPECT(rows[i].B.on() && rows[i].B.a != rows[i].B.b, msg);
        EXPECT(rows[i].fam != synergy::Family::None, msg);
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
    for (int l = 1; l < (int)synergy::UpLabel::Count_; ++l)
        for (int lang = 0; lang < 3; ++lang)
            EXPECT(*synergy::UpLabelText((synergy::UpLabel)l, lang) != 0, "every label has text in every language");
    for (int f = 1; f < (int)synergy::Family::Count; ++f)
        for (int lang = 0; lang < 3; ++lang)
            EXPECT(*synergy::ShapeText((synergy::Family)f, lang) != 0 && *synergy::FamilyName((synergy::Family)f, lang) != 0,
                   "every shape has text");

    // ── every branch line, every language: nothing that makes no sense ──
    // (user 2026-11-23: "95% → 142.5%"; review: "결과 2 → 3", "×1 → ×1.6", an empty label)
    struct Ch { const char* code; synergy::ChKind kind; synergy::ChCond cond; int pct; };
    static const Ch kChance[] = {
#define CH(c, k, n_, p) { #c, synergy::ChKind::k, synergy::ChCond::n_, p },
#include "SynergyChance.inc"
#undef CH
    };
    int lines = 0, grows = 0;
    auto audit = [&](const char* code, const std::wstring& s, int branch, int lang) {
        char msg[160];
        std::snprintf(msg, sizeof msg, "%s branch %c lang %d", code, "ABC"[branch], lang);
        ++lines;
        EXPECT(!s.empty(), msg);
        EXPECT(!s.empty() && s[0] != L' ', msg);                     // a number without a label
        EXPECT(s.find(L"×1 →") == std::wstring::npos, msg);          // a multiplier that starts at 1
        // No percentage past 100 after "확률" / "Chance" / "確率".
        for (const wchar_t* key : { L"확률", L"Chance", L"確率" }) {
            const size_t at = s.find(key);
            if (at == std::wstring::npos) continue;
            for (size_t p = at; p < s.size(); ++p)
                if (iswdigit(s[p])) {
                    const int v = (int)std::wcstol(s.c_str() + p, nullptr, 10);
                    EXPECT(v <= 100, msg);
                    while (p < s.size() && (iswdigit(s[p]) || s[p] == L'.')) ++p;
                }
        }
    };
    for (int i = 0; i < n; ++i)
        for (int b = 0; b < 3; ++b)
            for (int lv = 0; lv < 6; ++lv)
                for (int lang = 0; lang < 3; ++lang) {
                    // A number at its limit is not offered any more (SynergyRuntime CanTakeBranch).
                    if (b == 0 && !synergy::UpCanGrow(rows[i].A, lv)) continue;
                    if (b == 1 && !synergy::UpCanGrow(rows[i].B, lv)) continue;
                    audit(rows[i].code, synergy::BranchLine(&rows[i], false, synergy::ChKind::Crit, synergy::ChCond::Still, 0, b, lang, lv), b, lang);
                }
    for (const Ch& c : kChance) {
        const synergy::ChanceUp up = synergy::ChanceUpgrade(c.pct);
        char msg[96];
        std::snprintf(msg, sizeof msg, "%s: %d%% upgrades to %d%% (<= 100)", c.code, c.pct, up.pct);
        EXPECT(up.pct <= 100, msg);
        EXPECT(up.effectGrows ? up.pct == c.pct : up.pct > c.pct, "either the chance rises or the effect grows");
        grows += up.effectGrows ? 1 : 0;
        for (int b = 0; b < 3; ++b)
            for (int lv = 0; lv < 6; ++lv)
                for (int lang = 0; lang < 3; ++lang) {
                    if (b == 1 && !synergy::ChanceCondCanGrow(c.cond, lv)) continue;
                    audit(c.code, synergy::BranchLine(nullptr, true, c.kind, c.cond, c.pct, b, lang, lv), b, lang);
                }
        // Repeated A never reads past 100%, and once the effect grows the chance stays.
        for (int lv = 0; lv < 8; ++lv) EXPECT(synergy::ChanceAAt(c.pct, lv).pct <= 100, "repeated chance <= 100%");
    }
    EXPECT(synergy::ChanceUpgrade(65).pct == 98 && !synergy::ChanceUpgrade(65).effectGrows, "65% -> 98%");
    EXPECT(synergy::ChanceUpgrade(95).pct == 95 && synergy::ChanceUpgrade(95).effectGrows, "95% keeps 95%, effect grows");
    EXPECT(synergy::ChanceUpgrade(66).pct == 99 && synergy::ChanceUpgrade(67).effectGrows, "the 100% line");
    // Sample lines read as meant.
    const synergy::ChKind k0 = synergy::ChKind::Crit;
    const synergy::ChCond c0 = synergy::ChCond::Still;
    EXPECT(synergy::BranchLine(synergy::FindUp("V_DICE"), false, k0, c0, 0, 0, 0) == L"관통·튕김 결과 +2 → +3", "dice A");
    EXPECT(synergy::BranchLine(synergy::FindUp("V_HOMING"), false, k0, c0, 0, 0, 0) == L"유도 세기 +60%", "homing A");
    EXPECT(synergy::BranchLine(synergy::FindUp("C_GLASS"), false, k0, c0, 0, 0, 0) == L"피해 ×3.1 → ×3.6 · 최대 체력 30 → 25", "glass A");
    EXPECT(synergy::BranchLine(synergy::FindUp("G_CHARGER"), false, k0, c0, 0, 1, 0) == L"보호막 동안 받는 피해 -10%", "charger B");
    // Repeated picks read from the current value to the next one.
    EXPECT(synergy::BranchLine(synergy::FindUp("V_BOUNCE"), false, k0, c0, 0, 0, 0, 2) == L"튕김 4 → 5", "bounce A third pick");
    EXPECT(synergy::BranchLine(synergy::FindUp("V_HOMING"), false, k0, c0, 0, 0, 0, 1) == L"유도 세기 +60% → +120%", "homing A second pick");
    EXPECT(!synergy::UpCanGrow(synergy::FindUp("T_NTH")->A, 2), "every 6 → 4 → 2 stops at 2");
    EXPECT(synergy::ChanceAAt(40, 3).pct == 90 && synergy::ChanceAAt(40, 3).effect == 1, "40% → 60% → 90% → effect");
    EXPECT(synergy::BranchLine(nullptr, true, synergy::ChKind::Pierce, c0, 95, 0, 0, 2) == L"관통 +3 → +4", "pierce effect third pick");
    EXPECT(synergy::BranchLine(nullptr, true, k0, synergy::ChCond::Still, 65, 1, 0, 1) == L"멈춰 있는 시간 0.3초 → 0.1초", "still eased twice");
    EXPECT(!synergy::ChanceCondCanGrow(synergy::ChCond::Still, 2), "stillness stops at 0.1s");
    EXPECT(synergy::BranchLine(synergy::FindUp("V_BOUNCE"), false, k0, c0, 0, 2, 0, 1).find(L"겹침 1 → 2") != std::wstring::npos, "shape stack text");
    std::printf("upgrade text: %d lines checked; chance cards: %d grow their effect, %d raise their chance\n",
                lines, grows, (int)(sizeof(kChance) / sizeof(kChance[0])) - grows);
    EXPECT(synergy::UpNumber(synergy::UpUnit::Pct, 0.12f, 0) == L"12%", "pct text");
    EXPECT(synergy::UpNumber(synergy::UpUnit::Sec, 0.45f, 0) == L"0.45초", "sec text");
    EXPECT(synergy::UpNumber(synergy::UpUnit::Ring, 2.25f, 1) == L"2.25○", "ring text");

    if (g_fail) { std::printf("%d failed\n", g_fail); return 1; }
    std::printf("reward rules and upgrade table passed (%d upgrade rows, %d upgradable cards)\n", n, upgradable);
    return 0;
}
