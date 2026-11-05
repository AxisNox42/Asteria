// Run scripts/test_augment_synergy.ps1. Rough balance check of the fixed-HP
// augment table (Entity/AugmentSynergy.h): random loadouts, single cards,
// named builds and conditional (needs) cards. Pass --csv <dir> to also write
// the tables scripts/build_synergy_xlsx.py reads.
#include "AugmentSynergy.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

using namespace synergy;

static int g_fail = 0;
static std::vector<float> g_lift;   // per card: power of runs holding it / average run
static void Check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_fail;
}

static int Id(const char* c) {
    const int i = FindCode(c);
    if (i < 0) { std::printf("unknown card %s\n", c); std::exit(2); }
    return i;
}
static std::vector<int> Cards(std::initializer_list<const char*> l) {
    std::vector<int> v;
    for (const char* c : l) v.push_back(Id(c));
    return v;
}

// Naming rules of Docs/QA/Asteria_증강기획_RnS기반_100.xlsx (작명 원칙): R2 only
// Hangul and spaces, 2-7 syllables; no two cards share a name.
static int Syllables(const char* s) {
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p;) {
        if (*p == ' ') { ++p; continue; }
        if (p[0] >= 0xEA && p[0] <= 0xED && p[1] && p[2]) {   // 3-byte UTF-8
            const unsigned cp = ((p[0] & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
            if (cp < 0xAC00u || cp > 0xD7A3u) return -1;
            ++n; p += 3;
        } else return -1;
    }
    return n;
}
static void CheckNames(const std::vector<Aug>& t) {
    float sum[5] = {}; int cnt[5] = {};
    for (size_t i = 0; i < t.size(); ++i) {
        const int n = Syllables(t[i].kr);
        char m[128]; std::snprintf(m, sizeof m, "name of %s is Hangul only, 2-7 syllables", t[i].code);
        Check(n >= 2 && n <= 7, m);
        if (t[i].role != Role::Frame) { sum[(int)t[i].rar] += (float)n; ++cnt[(int)t[i].rar]; }
        for (size_t j = 0; j < i; ++j)
            if (std::string(t[i].kr) == t[j].kr) {
                std::snprintf(m, sizeof m, "name of %s is unique", t[i].code);
                Check(false, m);
            }
    }
    for (int r = 0; r < 5; ++r)
        if (cnt[r]) std::printf("  names %-4s avg %.2f syllables (%d)\n", RarName((Rar)r), sum[r] / (float)cnt[r], cnt[r]);
    // R4: the higher the rarity, the longer the name (frames excluded).
    for (int r = 1; r < 5; ++r)
        if (cnt[r] && cnt[r - 1]) {
            char m[128]; std::snprintf(m, sizeof m, "average name length of %s is not below %s",
                                       RarName((Rar)r), RarName((Rar)(r - 1)));
            Check(sum[r] / (float)cnt[r] >= sum[r - 1] / (float)cnt[r - 1] - 0.05f, m);
        }
}

// What a card adds in the loadout that switches it on: a frame (or the bare
// gun plus the cheapest verbs) that gives every tag the card needs. Cards with
// no need are measured on the bare gun. Returns the power ratio with / without.
static void Provide(uint64_t need, uint64_t& have, std::vector<int>& ctx, int skip, int depth) {
    const auto& t = Table();
    while ((have & need) != need && depth < 4) {
        int best = -1;
        for (int i = kFrameCount; i < (int)t.size(); ++i) {
            if (i == skip) continue;
            if (!(t[i].gives & need & ~have)) continue;
            if (best < 0 || (int)t[i].rar < (int)t[best].rar) best = i;
        }
        if (best < 0) return;
        if (t[best].needs) Provide(t[best].needs, have, ctx, skip, depth + 1);
        ctx.push_back(best); have |= t[best].gives;
    }
}
static float ContextGain(int card, std::vector<int>* ctxOut = nullptr) {
    const auto& t = Table();
    const uint64_t need = t[card].needs;
    int frame = 0;
    std::vector<int> ctx;
    if (t[card].scales) {
        uint64_t have = 0; int got = 0;
        for (int i = kFrameCount; i < (int)t.size() && got < 3; ++i) {
            if (i == card || t[i].needs || t[i].scales) continue;
            const uint64_t add = t[i].gives & t[card].scales & ~have;
            if (add && (int)t[i].rar <= (int)Rar::Rare) { ctx.push_back(i); have |= add; ++got; }
        }
    }
    if (need) {
        bool framed = false;
        for (int f = 1; f < kFrameCount && !framed; ++f)
            if ((t[f].gives & need) == need) { frame = f; framed = true; }
        if (!framed) { uint64_t have = 0; Provide(need, have, ctx, card, 0); }
    }
    auto with = ctx; with.push_back(card);
    if (ctxOut) *ctxOut = ctx;
    return Evaluate(frame, with).power / Evaluate(frame, ctx).power;
}

struct Build { const char* name; const char* frame; std::vector<int> cards; };

// A random run: a frame and `n` cards. biased = the card draw of the design
// doc (cards whose needs the loadout already has come up twice as often).
static std::vector<int> RandomCards(std::mt19937& rng, int frame, int n, bool biased) {
    const auto& t = Table();
    std::vector<int> cards;
    std::vector<int> count(t.size(), 0);
    uint64_t tags = t[frame].gives;
    for (int k = 0; k < n; ++k) {
        std::vector<int> pool; std::vector<float> w;
        for (int i = kFrameCount; i < (int)t.size(); ++i) {
            if (count[i] > 0 && !(t[i].stack && count[i] < 3)) continue;
            // Guards/costs are rarer than the rest in the real draw.
            float wt = t[i].rar == Rar::Legendary ? 0.4f : t[i].rar == Rar::Epic ? 0.7f : 1.0f;
            if (biased && t[i].needs && (t[i].needs & tags) == t[i].needs) wt *= 2.0f;
            pool.push_back(i); w.push_back(wt);
        }
        std::discrete_distribution<int> d(w.begin(), w.end());
        const int pick = pool[d(rng)];
        cards.push_back(pick); ++count[pick]; tags |= t[pick].gives;
    }
    return cards;
}

static float Pct(std::vector<float>& v, float p) {
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, (size_t)(p * (float)v.size()))];
}

int main(int argc, char** argv) {
    std::string csv;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--csv") csv = argv[i + 1];
    const auto& t = Table();
    std::printf("cards: %d (frames %d)\n", (int)t.size(), kFrameCount);
    Check((int)t.size() >= 100, "the table has at least 100 cards");
    for (const Aug& a : t) {
        char m[96]; std::snprintf(m, sizeof m, "%s is not a plain stat card", a.code);
        Check(a.code[0] != 'N' || a.code[1] != '_', m);
    }
    CheckNames(t);

    // ── baseline ──────────────────────────────────────────────────────────
    std::printf("\n[baseline]\n");
    const Result base = Evaluate(0, {});
    std::printf("  bare gun: dps %.1f surv %.2f power %.2f\n", base.dps, base.surv, base.power);
    Check(std::fabs(base.power - 1.0f) < 0.01f, "bare gun is power 1.0");

    // ── frames alone ──────────────────────────────────────────────────────
    std::printf("\n[frames alone]\n");
    for (int f = 0; f < kFrameCount; ++f) {
        const Result r = Evaluate(f, {});
        std::printf("  %-10s power %.2f\n", t[f].code, r.power);
        Check(r.power > 0.80f && r.power < 1.35f, "frame within 0.8x-1.35x of the bare gun");
    }

    // ── single cards on the bare gun, by rarity ───────────────────────────
    std::printf("\n[single card on F_BASIC]\n");
    float rarSum[5] = {}; int rarN[5] = {};
    struct Row { int i; float p; };
    std::vector<Row> rows;
    for (int i = kFrameCount; i < (int)t.size(); ++i) {
        const Result r = Evaluate(0, { i });
        rows.push_back({ i, r.power });
        if (r.dead == 0) { rarSum[(int)t[i].rar] += r.power; ++rarN[(int)t[i].rar]; }
    }
    for (const Row& r : rows)
        std::printf("  %-16s %-6s %-6s power %.2f%s\n", t[r.i].code, RarName(t[r.i].rar),
                    RoleName(t[r.i].role), r.p, t[r.i].needs ? "  (needs a tag)" : "");
    for (int k = 0; k < 5; ++k)
        if (rarN[k]) std::printf("  avg %-4s %.2f (%d)\n", RarName((Rar)k), rarSum[k] / (float)rarN[k], rarN[k]);
    for (int k = 1; k < 5; ++k)
        if (rarN[k] && rarN[k - 1]) {
            char m[96]; std::snprintf(m, sizeof m, "average %s is not below average %s by more than 0.06",
                                      RarName((Rar)k), RarName((Rar)(k - 1)));
            Check(rarSum[k] / (float)rarN[k] > rarSum[k - 1] / (float)rarN[k - 1] - 0.06f, m);
        }
    for (const Row& r : rows) {
        const Aug& a = t[r.i];
        if (a.needs) {
            char m[96]; std::snprintf(m, sizeof m, "%s alone is a dead card (power ~1)", a.code);
            Check(std::fabs(r.p - 1.0f) < 0.05f, m);
        } else if (a.role != Role::Cost && a.role != Role::Guard && a.rar != Rar::Mythic) {
            char m[96]; std::snprintf(m, sizeof m, "%s alone stays under 1.9x", a.code);
            Check(r.p < 1.9f, m);
        }
    }
    {
        // The strongest card alone must not exceed twice the weakest working one.
        float lo = 99, hi = 0;
        for (const Row& r : rows) if (!t[r.i].needs && t[r.i].rar != Rar::Mythic) { lo = std::min(lo, r.p); hi = std::max(hi, r.p); }
        std::printf("  single-card range %.2f - %.2f\n", lo, hi);
        Check(hi / lo < 2.6f, "single cards within 2.6x of each other");
    }

    // ── rarity bands: what a card adds where it works ────────────────────
    std::printf("\n[rarity bands: gain of each card in the loadout that switches it on]\n");
    {
        // Common +5..20%, Rare +20..45%, Epic +45..70%, Legendary +70..100%, Mythic +100%.. (the bands do not overlap).
        const float lo[5] = { 1.05f, 1.20f, 1.45f, 1.70f, 2.00f };
        const float hi[5] = { 1.20f, 1.45f, 1.70f, 2.00f, 3.50f };
        for (int i = kFrameCount; i < (int)t.size(); ++i) {
            const float g = ContextGain(i);
            const int r = (int)t[i].rar;
            const bool ok = g >= lo[r] - 0.005f && g < hi[r] - 0.005f + (r == 4 ? 0.5f : 0.0f);
            std::printf("  %-16s %-4s gain %.2f%s\n", t[i].code, RarName(t[i].rar), g, ok ? "" : "  <-- outside band");
            char m[128]; std::snprintf(m, sizeof m, "%s (%s) gain %.2f is inside its rarity band", t[i].code, RarName(t[i].rar), g);
            Check(ok, m);
        }
    }

    // ── random runs ───────────────────────────────────────────────────────
    std::printf("\n[random runs: frame + 6 cards]\n");
    std::mt19937 rng(20261104);
    const int N = 200000;
    for (const bool biased : { false, true }) {
        std::vector<float> pw; pw.reserve(N);
        double deadSum = 0; float bestP = 0; int bestF = 0; std::vector<int> bestCards; for (int k = 0; k < N; ++k) {
            const int f = (int)(rng() % kFrameCount);
            const auto cards = RandomCards(rng, f, 6, biased);
            const Result r = Evaluate(f, cards);
            pw.push_back(r.power); deadSum += r.dead;
            if (r.power > bestP) { bestP = r.power; bestF = f; bestCards = cards; }
        }
        if (biased) std::printf("  best run power %.2f: frame %s", bestP, t[bestF].code);
        if (biased) { for (int c : bestCards) std::printf(" + %s", t[c].code); std::printf("\n"); }
        const float p10 = Pct(pw, 0.10f), p50 = Pct(pw, 0.50f), p90 = Pct(pw, 0.90f),
                    p99 = Pct(pw, 0.99f), p999 = Pct(pw, 0.999f), mx = pw.back(), mn = pw.front();
        std::printf("  %s: min %.2f p10 %.2f p50 %.2f p90 %.2f p99 %.2f p99.9 %.2f max %.2f, dead cards/run %.2f\n",
                    biased ? "biased draw" : "uniform draw", mn, p10, p50, p90, p99, p999, mx, deadSum / N);
        Check(p10 / p50 > 0.40f, "p10 is at least 0.40x the median");
        Check(p99 / p50 < 3.5f, "p99 is at most 3.5x the median");
        Check(mx / p50 < 12.0f, "the single best run is at most 12x the median");
        if (biased) {
            Check(p50 > 1.5f && p50 < 6.0f, "median six-card run is 1.5x-6x a bare gun");
        }
    }

    // ── named builds ──────────────────────────────────────────────────────
    std::printf("\n[named builds]\n");
    std::vector<Build> builds = {
        { "벽에 튕기는 레이저",  "F_LASER",   Cards({ "V_BOUNCE", "V_HOMING", "V_MARK", "G_CHARGER", "P_KILL_PIERCE", "P_MOVING_EXTRA" }) },
        { "미친 저격수",         "F_SNIPER",  Cards({ "C_GLASS", "V_PIERCE", "V_BOUNCE", "P_STILL_CRIT", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "뽕맛 조합",           "F_MINIGUN", Cards({ "T_NTH", "V_BOUNCE", "V_HOMING", "G_CHARGER", "P_CLEAN_BOUNCE", "P_BOSS_CRIT" }) },
        { "총알 비",             "F_SHOTGUN", Cards({ "V_SPLIT", "V_BLAST", "G_CHARGER", "P_SHIELD_PIERCE", "P_KILL_PIERCE", "P_MOVING_EXTRA" }) },
        { "차지 포탑",           "F_SNIPER",  Cards({ "T_STILL", "G_STILLGUARD", "V_BOUNCE", "P_STILL_CRIT", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "유도 폭우",           "F_MINIGUN", Cards({ "V_HOMING", "V_MARK", "G_CHARGER", "T_KILLBURST", "P_CLEAN_BOUNCE", "P_BOSS_CRIT" }) },
        { "번개 전도체",         "F_BASIC",   Cards({ "V_CHAIN", "T_KILLBURST", "T_CRITECHO", "G_CIRCUIT", "P_SHIELD_PIERCE", "P_KILL_PIERCE" }) },
        { "공전 요새",           "F_BASIC",   Cards({ "V_ORBIT", "G_PARRY", "G_CHARGER", "T_AFTERIMAGE", "P_MOVING_EXTRA", "P_STILL_CRIT" }) },
        { "되돌이 부메랑",       "F_BASIC",   Cards({ "V_RETURN", "V_PIERCE", "V_GROW", "G_CHARGER", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "빈 방 격자",          "F_LASER",   Cards({ "V_BOUNCE", "V_TRAIL", "G_CHARGER", "P_CLEAN_BOUNCE", "P_BOSS_CRIT", "P_SHIELD_PIERCE" }) },
        { "불바다 점사",         "F_BURST",   Cards({ "V_BURN", "V_MINE", "G_CHARGER", "P_KILL_PIERCE", "P_MOVING_EXTRA", "P_STILL_CRIT" }) },
        { "산탄 폭죽",           "F_SHOTGUN", Cards({ "V_SPRAY", "V_BLAST", "T_FIREWORK", "G_CHARGER", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "석궁 저격",           "F_CROSSBOW",Cards({ "V_MARK", "V_BOUNCE", "G_CHARGER", "P_CLEAN_BOUNCE", "P_BOSS_CRIT", "P_SHIELD_PIERCE" }) },
        { "중력 지옥",           "F_BASIC",   Cards({ "V_GRAVITY", "V_BLAST", "V_BURN", "G_CHARGER", "P_KILL_PIERCE", "P_MOVING_EXTRA" }) },
        { "유리 파편",           "F_SNIPER",  Cards({ "C_GLASS", "V_SPLIT", "V_PIERCE", "P_STILL_CRIT", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "방벽 전사",           "F_BASIC",   Cards({ "G_CHARGER", "G_LASTWALL", "G_MIRROR", "V_BLAST", "P_CLEAN_BOUNCE", "P_BOSS_CRIT" }) },
        { "메아리 도탄",         "F_MINIGUN", Cards({ "Y_ECHO", "Y_ETERNAL", "G_CHARGER", "V_HOMING", "P_SHIELD_PIERCE", "P_KILL_PIERCE" }) },
        { "무결 사수",           "F_BASIC",   Cards({ "C_RITUAL", "G_CHARGER", "T_AFTERIMAGE", "T_COMBO", "G_PARRY", "P_MOVING_EXTRA" }) },
        { "파동 흡수",           "F_WAVE",    Cards({ "V_ABSORB", "V_MARK", "G_CHARGER", "T_RHYTHM", "P_STILL_CRIT", "P_CROWD_PIERCE" }) },
        { "박격 지뢰밭",         "F_MORTAR",  Cards({ "V_MINE", "V_BURN", "G_CHARGER", "P_LONG_SPLIT", "P_CLEAN_BOUNCE", "P_BOSS_CRIT" }) },
        { "복수의 불꽃",         "F_BASIC",   Cards({ "T_GRUDGE", "T_BERSERK", "T_LASTSTAND", "C_HERESY", "P_SHIELD_PIERCE", "P_KILL_PIERCE" }) },
        { "주사위 도박",         "F_BASIC",   Cards({ "V_DICE", "A_COLLECTOR", "V_MARK", "V_HOMING", "P_MOVING_EXTRA", "P_STILL_CRIT" }) },
        { "자리바꿈 암살",       "F_BASIC",   Cards({ "V_SWAP", "T_FIREWORK", "T_AFTERIMAGE", "G_PARRY", "P_CROWD_PIERCE", "P_LONG_SPLIT" }) },
        { "거대 성장탄",         "F_BASIC",   Cards({ "V_GROW", "V_BOUNCE", "T_ANCHOR", "G_STILLGUARD", "P_CLEAN_BOUNCE", "P_BOSS_CRIT" }) },
        { "개전 일제사격",       "F_MINIGUN", Cards({ "T_OPENING", "T_COMBO", "G_CHARGER", "P_SHIELD_PIERCE", "P_KILL_PIERCE", "P_MOVING_EXTRA" }) },
    };
    // Median of the biased random runs, for the ratio column.
    std::vector<float> ref; ref.reserve(50000);
    for (int k = 0; k < 50000; ++k) {
        const int f = (int)(rng() % kFrameCount);
        ref.push_back(Evaluate(f, RandomCards(rng, f, 6, true)).power);
    }
    const float med = Pct(ref, 0.5f);
    std::printf("  median biased random run: %.2f\n", med);
    std::vector<float> buildPower;
    for (const Build& b : builds) {
        const Result r = Evaluate(Id(b.frame), b.cards);
        buildPower.push_back(r.power);
        std::printf("  %-18s dps %7.0f surv %.2f power %.2f (%.2fx median) dead %d\n",
                    b.name, r.dps, r.surv, r.power, r.power / med, r.dead);
        char m[128]; std::snprintf(m, sizeof m, "%s has no dead card", b.name);
        Check(r.dead == 0, m);
        std::snprintf(m, sizeof m, "%s is 0.75x-6x the median run", b.name);
        Check(r.power / med > 0.75f && r.power / med < 6.0f, m);
    }

    // ── glass cannon is a real trade ──────────────────────────────────────
    std::printf("\n[glass cannon]\n");
    {
        const int g = Id("C_GLASS");
        const auto core = Cards({ "V_PIERCE", "V_BOUNCE", "V_MARK", "V_HOMING", "P_KILL_PIERCE", "P_MOVING_EXTRA" });
        auto withGlass = core; withGlass.push_back(g);
        auto withShield = core; withShield.push_back(Id("G_CHARGER"));
        const Result a = Evaluate(Id("F_SNIPER"), withGlass), b = Evaluate(Id("F_SNIPER"), withShield);
        std::printf("  glass: dps %.0f surv %.2f power %.2f | shield: dps %.0f surv %.2f power %.2f\n",
                    a.dps, a.surv, a.power, b.dps, b.surv, b.power);
        Check(a.dps > b.dps * 1.8f, "glass cannon more than doubles the damage");
        Check(a.surv < 0.35f, "glass cannon keeps survival under 0.35 of the full bar");
        Check(a.power > b.power && a.power < b.power * 2.2f, "glass wins on power but by less than 2.2x");
    }

    // ── duplicates, synergy partners, usefulness ─────────────────────────
    std::printf("\n[overlap / synergy / usefulness]\n");
    {
        // Two cards overlap when they play the same part in the same way: same role,
        // same tags given and needed, and the same set of Fx fields in use.
        auto fxMask = [](const Fx& f) {
            const Fx d;
            uint64_t m = 0; int b = 0;
            auto add = [&](bool differs) { if (differs) m |= 1ull << b; ++b; };
            add(f.dmg != d.dmg); add(f.rate != d.rate); add(f.acc != d.acc); add(f.move != d.move);
            add(f.bullets != d.bullets); add(f.dmgAdd != d.dmgAdd); add(f.pierce != d.pierce);
            add(f.bounce != d.bounce); add(f.split != d.split); add(f.nth != d.nth);
            add(f.splitDmg != d.splitDmg); add(f.homing != d.homing); add(f.blast != d.blast);
            add(f.chain != d.chain); add(f.orbit != d.orbit); add(f.crit != d.crit);
            add(f.critMul != d.critMul); add(f.critEcho != d.critEcho); add(f.perBounce != d.perBounce);
            add(f.perPierce != d.perPierce); add(f.pierceBlast != d.pierceBlast);
            add(f.extraPass != d.extraPass); add(f.proc != d.proc); add(f.hpDelta != d.hpDelta);
            add(f.hpSet != d.hpSet); add(f.shield != d.shield); add(f.evade != d.evade);
            add(f.hitCost != d.hitCost); add(f.taken != d.taken);
            return m;
        };
        int dup = 0;
        for (size_t i = 0; i < t.size(); ++i)
            for (size_t j = i + 1; j < t.size(); ++j)
                if (t[i].role != Role::Chance && t[i].role == t[j].role && t[i].gives == t[j].gives && t[i].needs == t[j].needs && t[i].scales == t[j].scales &&
                    fxMask(t[i].fx) == fxMask(t[j].fx)) {
                    std::printf("  overlap: %s / %s\n", t[i].code, t[j].code);
                    ++dup;
                }
        Check(dup == 0, "no two cards play the same part in the same way");

        // Every card has partners: cards it switches on, and cards that switch it on.
        int lonely = 0;
        for (int i = 0; i < (int)t.size(); ++i) {
            int feeds = 0, fed = 0;
            for (int j = 0; j < (int)t.size(); ++j) {
                if (j == i) continue;
                if ((t[j].needs | t[j].scales) & t[i].gives) ++feeds;
                if ((t[i].needs | t[i].scales) & t[j].gives) ++fed;
            }
            const int deg = feeds + fed;
            const int want = i == 0 ? 0 : (t[i].role == Role::Frame || t[i].role == Role::Amp) ? 1 : 2;
            if (deg < want) {
                std::printf("  few partners: %-16s feeds %d, fed by %d\n", t[i].code, feeds, fed);
                ++lonely;
            }
        }
        std::printf("  cards with few partners (information only: no card needs another any more): %d\n", lonely);

        // A card must be worth taking where it works: runs that contain it (and where it is
        // switched on) are not weaker than the average run.
        std::mt19937 rr(77);
        const int R = 150000;
        std::vector<double> sum(t.size(), 0.0); std::vector<int> cnt(t.size(), 0);
        double all = 0; int allN = 0;
        for (int k = 0; k < R; ++k) {
            const int f = (int)(rr() % kFrameCount);
            const auto cards = RandomCards(rr, f, 6, true);
            const Result r = Evaluate(f, cards);
            all += r.power; ++allN;
            for (int c : cards)
            {
                int variety = 0;   // a "variety" card only counts once it has something to count
                for (uint64_t m = r.tags & t[c].scales; m; m &= m - 1) ++variety;
                if ((t[c].needs & r.tags) == t[c].needs && (!t[c].scales || variety >= 2)) {
                    sum[c] += r.power; ++cnt[c];
                }
            }
        }
        const double mean = all / allN;
        std::vector<std::pair<float, int>> lifts;
        for (int c = kFrameCount; c < (int)t.size(); ++c)
            if (cnt[c] > 50) lifts.push_back({ (float)(sum[c] / cnt[c] / mean), c });
        std::sort(lifts.begin(), lifts.end());
        std::printf("  weakest cards by lift (power of runs holding them / average run):\n");
        for (size_t k = 0; k < std::min<size_t>(10, lifts.size()); ++k)
            std::printf("    %-16s lift %.2f\n", t[lifts[k].second].code, lifts[k].first);
        int useless = 0;
        for (auto& l : lifts)
            if (l.first < 0.85f) { ++useless; }
        Check(useless == 0, "no card makes a run weaker than average where it is switched on");
        g_lift.assign(t.size(), 0.0f);
        for (auto& l : lifts) g_lift[l.second] = l.first;
    }

    // ── csv for the spreadsheet ───────────────────────────────────────────
    if (!csv.empty()) {
        std::FILE* f = std::fopen((csv + "/cards.csv").c_str(), "wb");
        if (f) {
            std::fputs("code,kr,rule,rarity,role,gives,needs,stack,single_power,gain,lift,scales\n", f);
            for (int i = 0; i < (int)t.size(); ++i) {
                const Aug& a = t[i];
                auto tagStr = [](uint64_t m) {
                    std::string s;
                    for (int b = 0; b < kTagCount; ++b)
                        if (m & (1ull << b)) { if (!s.empty()) s += "|"; s += TagName(b); }
                    return s;
                };
                const float p = i < kFrameCount ? Evaluate(i, {}).power : Evaluate(0, { i }).power;
                const float gain = i < kFrameCount ? p : ContextGain(i);
                std::fprintf(f, "%s,%s,\"%s\",%s,%s,%s,%s,%d,%.3f,%.3f,%.3f,%s\n", a.code, a.kr, a.rule,
                             RarName(a.rar), RoleName(a.role), tagStr(a.gives).c_str(),
                             tagStr(a.needs).c_str(), a.stack ? 1 : 0, p, gain, i < (int)g_lift.size() ? g_lift[i] : 0.0f, tagStr(a.scales).c_str());
            }
            std::fclose(f);
        }
        f = std::fopen((csv + "/builds.csv").c_str(), "wb");
        if (f) {
            std::fputs("name,frame,cards,dps,survival,power,vs_median,dead\n", f);
            for (const Build& b : builds) {
                const Result r = Evaluate(Id(b.frame), b.cards);
                std::string cs;
                for (int c : b.cards) { if (!cs.empty()) cs += " + "; cs += t[c].kr; }
                std::fprintf(f, "%s,%s,\"%s\",%.0f,%.3f,%.3f,%.3f,%d\n", b.name, t[Id(b.frame)].kr,
                             cs.c_str(), r.dps, r.surv, r.power, r.power / med, r.dead);
            }
            std::fclose(f);
        }
    }

    // ── what the game may offer ───────────────────────────────────────────
    // The game offers a card only when its needs are met (SynergyRuntime CanOffer). Every run built
    // from such offers must have no card that does nothing, and the offers must never run dry.
    {
        std::printf("\n[offer rule]\n");
        const auto& t = Table();
        std::mt19937 ro(4242);
        int dead = 0, dry = 0, firstMin = 999;
        for (int k = 0; k < 20000; ++k) {
            const int f = (int)(ro() % kFrameCount);
            std::vector<int> cards;
            std::vector<int> count(t.size(), 0);
            uint64_t tags = t[f].gives;
            for (int n = 0; n < 12; ++n) {
                std::vector<int> pool;
                for (int i = kFrameCount; i < (int)t.size(); ++i)
                    if (count[i] == 0 && (t[i].needs & tags) == t[i].needs) pool.push_back(i);
                if (n == 0) firstMin = std::min(firstMin, (int)pool.size());
                if (pool.empty()) { ++dry; break; }
                const int pick = pool[ro() % pool.size()];
                cards.push_back(pick); ++count[pick]; tags |= t[pick].gives;
            }
            if (Evaluate(f, cards).dead > 0) ++dead;
        }
        std::printf("  dead cards in %d offered runs: %d, runs that ran dry: %d, smallest first offer pool: %d\n", 20000, dead, dry, firstMin);
        Check(dead == 0, "offered cards are never dead");
        Check(dry == 0, "the offers never run dry");
        Check(firstMin >= 12, "the first pick always has at least 12 cards to draw from");
    }

    std::printf("\n%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
