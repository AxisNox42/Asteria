// Balance table: shipped values match the balance sheet, the in-wave curve
// keeps a wave's total, and override files retune rows without code.
// Run with scripts/test_balance_tuning.ps1.
#include <cmath>
#include <cstdio>
#include <fstream>

#include "BalanceTuning.h"
#include "EnemyRoster.h"

static int g_failures = 0;
#define EXPECT(cond, ...)                                   \
    do {                                                    \
        if (!(cond)) {                                      \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                       \
            std::printf("\n");                              \
            ++g_failures;                                   \
        }                                                   \
    } while (0)

static bool Near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) <= eps; }

int main() {
    // Sheet "최종 수치" spot checks.
    EXPECT(Near(BalanceFor(0).xpMult, 4.0f) && Near(BalanceFor(3).xpMult, 4.0f), "xpMult 4");
    EXPECT(Near(BalanceFor(1).wv3, 0.424f), "NORMAL wv3");
    EXPECT(Near(BalanceFor(2).kGimbal, 1.359f), "HARD kGimbal");
    EXPECT(Near(BalanceFor(1).scopeInterval, 2.159f), "NORMAL scopeInterval");
    EXPECT(Near(BalanceFor(3).bossHp, 0.814f), "NIGHTMARE bossHp");
    EXPECT(Near(BalanceFor(0).hpScale, 0.80f) && Near(BalanceFor(0).hpLate, 0.70f), "hp rows");

    // Wave bands: 1-3, 4-6, 7-9, 10+.
    const BalanceTuning& hard = BalanceFor(2);
    EXPECT(Near(WaveBandMult(hard, 1), hard.wv1) && Near(WaveBandMult(hard, 3), hard.wv1), "band 1");
    EXPECT(Near(WaveBandMult(hard, 4), hard.wv2) && Near(WaveBandMult(hard, 6), hard.wv2), "band 2");
    EXPECT(Near(WaveBandMult(hard, 7), hard.wv3) && Near(WaveBandMult(hard, 9), hard.wv3), "band 3");
    EXPECT(Near(WaveBandMult(hard, 10), hard.wv4) && Near(WaveBandMult(hard, 18), hard.wv4), "band 4");

    // Sheet "곡선 설계": 0.909 / 1.240 at 70% / 0.413, average 1.
    const BalanceTuning& normal = BalanceFor(1);
    EXPECT(Near(WaveCurveMult(normal, 0.0f), 0.909f), "curve start %.3f", WaveCurveMult(normal, 0.0f));
    EXPECT(Near(WaveCurveMult(normal, 0.5f), 1.145f), "curve 50%%");
    EXPECT(Near(WaveCurveMult(normal, 0.7f), 1.240f), "curve peak");
    EXPECT(Near(WaveCurveMult(normal, 0.9f), 0.689f), "curve 90%%");
    EXPECT(Near(WaveCurveMult(normal, 1.0f), 0.413f), "curve end");
    double sum = 0.0;
    const int steps = 100000;
    for (int i = 0; i < steps; ++i) sum += WaveCurveMult(normal, (i + 0.5f) / steps);
    EXPECT(std::fabs(sum / steps - 1.0) < 0.001, "curve average %.4f", sum / steps);

    // Kind multipliers scale the roster odds; ROTOR keeps the remainder.
    // NIGHTMARE wave 11: Swarm 12% x 0.686 = 8%.
    EXPECT(RosterBaseChancePct(RosterKind::SWARM, 11, 3) == 12, "base swarm odds");
    EXPECT(RosterChancePct(RosterKind::SWARM, 11, 3) == 8, "swarm odds x kSwarm");
    EXPECT(RosterChancePct(RosterKind::ROTOR, 11, 3) == 0, "rotor has no weighted odds");

    // Overrides: one value fills a row, pasted sheet rows (tabs) work,
    // comments and unknown keys are skipped.
    EXPECT(ApplyBalanceLine("xpMult = 2   # all"), "single value");
    for (int d = 0; d < kBalanceDifficulties; ++d) EXPECT(Near(BalanceFor(d).xpMult, 2.0f), "xp row %d", d);
    EXPECT(ApplyBalanceLine("bossHp\t0.5\t0.6\t0.7\t0.8"), "tab row");
    EXPECT(Near(BalanceFor(0).bossHp, 0.5f) && Near(BalanceFor(3).bossHp, 0.8f), "tab row values");
    EXPECT(ApplyBalanceLine("wvFirst = 1, 3, 5, 8"), "band starts");
    EXPECT(Near(WaveBandMult(BalanceFor(2), 5), BalanceFor(2).wv3), "moved band");
    EXPECT(!ApplyBalanceLine("# only a comment"), "comment line");
    EXPECT(!ApplyBalanceLine("notAField = 3"), "unknown key");
    EXPECT(!ApplyBalanceLine("cvPeak ="), "no values");

    const char* path = "balance_tuning_test.cfg";
    {
        std::ofstream out(path);
        out << "# test\ncvPeak = 2.0\nkSwarm = 0, 1, 1, 1\nbogus = 1\n";
    }
    EXPECT(LoadBalanceFile(path) == 2, "file rows applied");
    EXPECT(Near(BalanceFor(1).cvPeak, 2.0f), "file cvPeak");
    EXPECT(RosterChancePct(RosterKind::SWARM, 1, 0) == 0, "kSwarm 0 removes swarm odds");
    std::remove(path);
    EXPECT(LoadBalanceFile(path) == -1, "missing file");

    ResetBalance();
    EXPECT(Near(BalanceFor(1).cvPeak, 1.5f) && Near(BalanceFor(0).xpMult, 4.0f), "reset");
    EXPECT(Near(WaveBandMult(BalanceFor(2), 5), BalanceFor(2).wv2), "reset bands");

    if (g_failures == 0) std::printf("balance tuning: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
