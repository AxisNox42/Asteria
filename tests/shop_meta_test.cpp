// Shop upgrades (Core/Meta.h, 2026-11-16 batches 1-3): each one's number, read
// back through the real game code where that code is header-only (stats,
// damage, kill rewards, run reward, combo, codex, save), and through the
// Meta.h helpers for the ones whose call site lives in a .cpp with heavy game
// dependencies (cooldown, duration, luck, wave hooks, first card).
// Run with scripts/test_shop_meta.ps1 (it gives the test an empty save folder).
//
// Expected values are literals worked out from the shop text, not recomputed
// through the helpers.
#include "CollisionSystem.h"
#include "RunResult.h"
#include "RunStats.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace Audio { void PlaySfx(Sfx) {} void Play(Cue) {} }   // no audio in tests
float g_BaseOrtho[16] = {};
bool g_DevUnlocked = false;   // main.cpp's codex dev unlock (off: the decoder must do it)

static int g_fail = 0;
static void Check(bool ok, const char* what, double got, double want) {
    std::printf("%-46s got %10.4f  want %10.4f  %s\n", what, got, want, ok ? "ok" : "FAIL");
    if (!ok) ++g_fail;
}
static void Near(const char* what, double got, double want, double eps = 1e-3) {
    Check(std::fabs(got - want) <= eps, what, got, want);
}

static void ClearMeta() { for (int i = 0; i < META_COUNT; ++i) g_MetaLv[i] = 0; }

// One player hit of 100 on a fresh, very healthy mob of this kind.
static float Hit(MobKind kind, int source = space::kPlayerSource) {
    MonsterManager mm;
    Monster* m = new Monster(500.0f, 300.0f, 1.0f);
    m->MakeKind(kind);
    m->hp = 1.0e6f;
    if (IsEliteMob(kind)) m->eliteMaxHp = m->hp;
    mm.monsters.push_back(m);
    const float before = m->hp;
    mm.ApplyDamage(*m, 100.0f, source);
    return before - m->hp;
}

int main() {
    // ── Stats at full level (ApplyMeta through the new-run stat build) ──────
    ClearMeta();
    const PlayerStats base = ResetRunStats(1.0f);
    g_MetaLv[META_HP] = 5; g_MetaLv[META_DMG] = 5; g_MetaLv[META_MOVE] = 3;
    g_MetaLv[META_ATKSPD] = 5; g_MetaLv[META_REGEN] = 5; g_MetaLv[META_SHOTSPD] = 3;
    g_MetaLv[META_CRIT] = 5; g_MetaLv[META_CRITDMG] = 5;
    const PlayerStats full = ResetRunStats(1.0f);
    std::printf("== stats (all at max level)\n");
    Near("max HP 100 -> +100", full.maxHP, base.maxHP + 100.0f);
    Near("attack x1.30", full.damageMultiplier / base.damageMultiplier, 1.30);
    Near("move speed x1.15", full.moveSpeedMult / base.moveSpeedMult, 1.15);
    Near("fire interval 0.20 / 1.25 = 0.16", full.fireInterval, 0.16);
    Near("shots per second x1.25", base.fireInterval / full.fireInterval, 1.25);
    Near("regen 1/3 + 0.5 = 0.8333 /s", full.regenPerSec, 1.0 / 3.0 + 0.5);
    Near("bullet speed 1200 x1.24 = 1488", full.bulletSpeed, 1488.0, 0.01);
    Near("crit chance 0 + 15", full.critChance, 15.0);
    Near("crit multiplier 2.0 + 0.5 = 2.5", full.critMult, 2.5);
    {   // the weapon step keeps the shop fire rate (baseFireInterval follows it)
        PlayerStats s = ResetRunStats(1.0f);
        ApplyLoadoutStats(s, (int)StartWeapon::RIFLE);
        Near("rifle loadout keeps 0.16 interval", s.baseFireInterval, 0.16);
    }

    // ── Damage on hit (MonsterManager::ApplyDamage) ────────────────────────
    std::printf("== damage (one player hit of 100)\n");
    ClearMeta();
    g_MetaPlayerHpFrac = 1.0f; g_MetaComboNow = 0;
    const float rotor0 = Hit(MobKind::ROTOR), antares0 = Hit(MobKind::ANTARES),
                norma0 = Hit(MobKind::NORMA);
    Near("baseline regular hit", rotor0, 100.0, 0.01);
    g_MetaLv[META_ELITEDMG] = 3; g_MetaLv[META_BOSSDMG] = 3;
    Near("Elite Damage Lv3: elite x1.24", Hit(MobKind::ANTARES) / antares0, 1.24);
    Near("Elite Damage Lv3: regular x1.00", Hit(MobKind::ROTOR) / rotor0, 1.00);
    Near("Boss Damage Lv3: boss x1.24", Hit(MobKind::NORMA) / norma0, 1.24);
    Near("not on a hit the player did not make", Hit(MobKind::ANTARES, space::kUntracked) / antares0, 1.00);
    ClearMeta();
    g_MetaLv[META_BERSERK] = 3;
    g_MetaPlayerHpFrac = 0.31f;
    Near("Last Stand Lv3 at 31% HP: x1.00", Hit(MobKind::ROTOR) / rotor0, 1.00);
    g_MetaPlayerHpFrac = 0.30f;
    Near("Last Stand Lv3 at 30% HP: x1.24", Hit(MobKind::ROTOR) / rotor0, 1.24);
    g_MetaPlayerHpFrac = 1.0f;
    ClearMeta();
    g_MetaLv[META_COMBO] = 3;
    g_MetaComboNow = 24;
    Near("Combo Focus Lv3 at 24 combo: x1.00", Hit(MobKind::ROTOR) / rotor0, 1.00);
    g_MetaComboNow = 25;
    Near("Combo Focus Lv3 at 25 combo: x1.09", Hit(MobKind::ROTOR) / rotor0, 1.09);
    g_MetaComboNow = 0;

    // ── Kill rewards (CollisionSystem.h) ─────────────────────────────────
    std::printf("== kill rewards\n");
    {
        ClearMeta();
        PlayerStats stats; float hp = 10.0f, acc = 0.0f; long long score = 0;
        KillRewardSink sink{ hp, acc, score, stats, 0.0f, 0.0f };
        std::srand(7);
        for (int i = 0; i < 20000; ++i) CreditRegularKill(sink, CM_SWARM, 0.0f);
        Near("Kill Mend Lv0: no healing", hp, 10.0);
        g_MetaLv[META_KILLHEAL] = 3;
        stats.maxHP = 1.0e9f;
        hp = 0.0f;
        for (int i = 0; i < 20000; ++i) CreditRegularKill(sink, CM_SWARM, 0.0f);
        const double heals = hp / 3.0;
        Near("Kill Mend Lv3: each heal is 3 HP", std::fmod(hp, 3.0), 0.0);
        Near("Kill Mend Lv3: 9% of 20000 kills heal", heals / 20000.0, 0.09, 0.006);
        stats.maxHP = 100.0f; hp = 99.0f;
        for (int i = 0; i < 2000; ++i) CreditRegularKill(sink, CM_SWARM, 0.0f);
        Near("Kill Mend never passes max HP", hp, 100.0);
    }
    auto eliteDust = [](int bountyLv) {
        g_MetaLv[META_BOUNTY] = bountyLv;
        g_StardustPickups.clear();
        PlayerStats stats; float hp = 50.0f, acc = 0.0f; long long score = 0;
        KillRewardSink sink{ hp, acc, score, stats, 0.0f, 0.0f };
        Monster m(500.0f, 300.0f, 1.0f);
        m.MakeKind(MobKind::REGULUS);
        m.hp = 0.0f; m.alive = false;
        CollectMonsterKill(sink, m);
        int dust = 0;
        for (const auto& p : g_StardustPickups) dust += p.value;
        return dust;
    };
    ClearMeta();
    const int dust0 = eliteDust(0), dust3 = eliteDust(3);
    Near("Elite Bounty Lv3: Regulus drops +15 stardust", dust3 - dust0, 15.0);
    {
        g_MetaLv[META_BOUNTY] = 3;
        g_StardustPickups.clear();
        PlayerStats stats; float hp = 50.0f, acc = 0.0f; long long score = 0;
        KillRewardSink sink{ hp, acc, score, stats, 0.0f, 0.0f };
        Monster m(500.0f, 300.0f, 1.0f);
        m.MakeKind(MobKind::ROTOR); m.hp = 0.0f; m.alive = false;
        CollectMonsterKill(sink, m);
        int dust = 0;
        for (const auto& p : g_StardustPickups) dust += p.value;
        Near("Elite Bounty: a regular Rotor drops its usual 1", dust, 1.0);
    }

    // ── End-of-run stardust (RecordRunResult through CommitRunResult) ──────
    std::printf("== run reward (score 12345, kills 77, difficulty x1.2 -> 60)\n");
    g_SmokeCapture = true;   // no disk writes for these
    auto runCoins = [](long long held) {
        g_Coins = held; g_LastRunCoins = 0; g_RunStardust = 0;
        CommitRunResult(false, 1, 12345, 77, 1);
        return g_Coins - held;
    };
    ClearMeta();
    Near("no upgrade: 60", runCoins(0), 60.0);
    g_MetaLv[META_REWARD] = 5;
    Near("Run Reward Lv5: 60 x1.5 = 90", runCoins(0), 90.0);
    Near("result screen shows the same 90", g_LastRunCoins, 90.0);
    g_MetaLv[META_INTEREST] = 1;
    Near("+Interest, 1000 held: 90 + (1090/50=21) = 111", runCoins(1000), 111.0);
    Near("+Interest, 10000 held: 90 + 50 (cap) = 140", runCoins(10000), 140.0);
    ClearMeta();
    g_MetaLv[META_INTEREST] = 1;
    Near("Interest only, 0 held: 60 + (60/50=1) = 61", runCoins(0), 61.0);

    // ── Combo window (Juice.h AddKillCombo) ───────────────────────────────
    std::printf("== combo window\n");
    ClearMeta();
    g_ComboWindowBonus = MetaComboWindowBonus();
    AddKillCombo();
    Near("no upgrade: 3.0 s", g_ComboTimer, 3.0);
    g_MetaLv[META_COMBO] = 3;
    g_ComboWindowBonus = MetaComboWindowBonus();
    AddKillCombo();
    Near("Combo Focus Lv3: 3.0 + 1.2 = 4.2 s", g_ComboTimer, 4.2);
    g_ComboWindowBonus = 0.0f;

    // ── Codex Decoder (Codex.h) ──────────────────────────────────────────
    std::printf("== codex decoder\n");
    ClearMeta();
    for (int i = 0; i < CM_COUNT; ++i) g_MobKillCounts[i] = 0;
    g_MobKillCounts[CM_ROTOR] = 1;
    Near("Rotor, 1 kill, no decoder: stage 1", CodexMobDataStage(CM_ROTOR), 1.0);
    g_MetaLv[META_CODEX] = 1;
    Near("Rotor, 1 kill, decoder: stage 3 (full)", CodexMobDataStage(CM_ROTOR), 3.0);
    g_MobKillCounts[CM_ROTOR] = 0;
    Near("Rotor, never killed, decoder: stage 0", CodexMobDataStage(CM_ROTOR), 0.0);

    // ── Helpers whose call sites are in .cpp files ─────────────────────────
    std::printf("== helpers (call sites in ActiveSkills / PassiveAugments / main / GameManager)\n");
    ClearMeta();
    g_MetaLv[META_COOLDOWN] = 5; Near("Skill Cooldown Lv5: x0.80", MetaCooldownMult(), 0.80);
    g_MetaLv[META_DURATION] = 3; Near("Duration Lv3: x1.24 (turret 10 s -> 12.4 s)", 10.0 * MetaDurationMult(), 12.4);
    g_MetaLv[META_LUCK] = 3;     Near("Luck Lv3: +3 points (odds x1.3)", MetaLuck(), 3.0);
    g_MetaLv[META_WAVEHEAL] = 3; Near("Wave Mend Lv3: 9% of max HP", MetaWaveMendFrac(), 0.09);
    g_MetaLv[META_BOSSPREP] = 1; Near("Boss Prep: 20% of max HP", MetaBossPrepFrac(), 0.20);
    g_MetaLv[META_WARD] = 1;     Near("Wave Ward on", MetaWaveWard() ? 1.0 : 0.0, 1.0);
    g_MetaLv[META_KILLHEAL] = 3; Near("Kill Mend Lv3 chance", MetaKillHealChance(), 0.09);
    g_MetaLv[META_BOUNTY] = 3;   Near("Elite Bounty Lv3", MetaEliteBounty(), 15.0);

    // ── Prices and refund ─────────────────────────────────────────────────
    std::printf("== prices and refund\n");
    ClearMeta();
    int refundBad = 0;
    for (int id = 0; id < META_COUNT; ++id) {
        if (MetaSlotRemoved(id)) continue;
        long long paid = 0;
        while (MetaNextCost(id) >= 0) { paid += MetaNextCost(id); ++g_MetaLv[id]; }
        if (MetaSpentOn(id) != paid) {
            std::printf("refund mismatch on meta %d: %lld vs %lld\n", id, MetaSpentOn(id), paid);
            ++refundBad;
        }
    }
    Near("refund = stardust paid, all 25 upgrades at max", refundBad, 0.0);
    ClearMeta();
    g_MetaLv[META_CRIT] = 2;
    Near("Crit Chance: Lv1 200 + Lv2 320 = 520 back", (double)MetaSpentOn(META_CRIT), 520.0);

    // ── Save round trip of every new level ─────────────────────────────────
    std::printf("== save round trip\n");
    {
        const std::string dir = SaveDir();
        const std::string want = std::filesystem::absolute(std::getenv("ASTERIA_SAVE_DIR")).string();
        if (dir.empty() || std::filesystem::absolute(dir).string() != want ||
            want.find("shop-meta") == std::string::npos) {
            std::printf("save folder is not the test sandbox (%s): skipping save\n", dir.c_str());
            ++g_fail;
        } else {
            g_SmokeCapture = false;
            ClearMeta();
            int lv[META_COUNT] = {};
            for (int id = META_ATKSPD; id < META_COUNT; ++id)
                lv[id] = g_MetaLv[id] = std::max(1, META_DEFS[id].maxLv - (id % 2));
            g_Coins = 4321;
            SaveGame();
            ClearMeta(); g_Coins = 0;
            LoadGame();
            int bad = 0;
            for (int id = META_ATKSPD; id < META_COUNT; ++id) if (g_MetaLv[id] != lv[id]) ++bad;
            Near("all 21 new levels come back from the save", bad, 0.0);
            Near("stardust comes back", (double)g_Coins, 4321.0);
            g_SmokeCapture = true;
        }
    }

    std::printf("\n%s (%d failed)\n", g_fail ? "SHOP META TEST FAILED" : "shop meta test passed", g_fail);
    return g_fail ? 1 : 0;
}
