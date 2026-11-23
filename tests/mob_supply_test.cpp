// Mob supply follows the kill rate (WaveSystem.h, RegularWaveFieldTarget /
// KillRateMeter; Docs/Plans/2026-11-17_보스엘리트_게임구조_개선.md, stage 3).
// Run with scripts/test_mob_supply.ps1.
#include <cmath>
#include <cstdio>

#include "WaveSystem.h"

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

static void CheckTarget() {
    const Difficulty d = Difficulty::NORMAL;
    const int floor = RegularWaveMinAlive(1, d);
    EXPECT(RegularWaveFieldTarget(1, d, 0.0f) == floor, "no kills -> floor");
    EXPECT(RegularWaveFieldTarget(1, d, 0.8f) == floor, "a slow build keeps the old floor");
    EXPECT(RegularWaveFieldTarget(1, d, 2.0f) == 12, "2/s -> 12, got %d", RegularWaveFieldTarget(1, d, 2.0f));
    EXPECT(RegularWaveFieldTarget(1, d, 50.0f) == RegularWaveFieldCap(1, d), "capped");
    // Later sets: floor and cap both climb.
    EXPECT(RegularWaveFieldCap(5, d) == kRegularFieldCap + kRegularFieldCapStep, "cap step");
    EXPECT(RegularWaveFieldTarget(5, d, 0.0f) == RegularWaveMinAlive(5, d), "later floor");
}

static void CheckMeter() {
    KillRateMeter m;
    m.Reset(100);
    long long kills = 100;
    const float dt = 1.0f / 60.0f;
    float t = 0.0f;
    for (; t < 0.8f; t += dt) { m.Step(dt, kills); }
    EXPECT(m.PerSecond(kills + 5) == 0.0f, "under 1 s: no reading yet");
    // 2 kills a second for 10 s: the reading settles near 2.
    float acc = 0.0f;
    for (float s = 0.0f; s < 10.0f; s += dt) {
        acc += dt * 2.0f;
        while (acc >= 1.0f) { acc -= 1.0f; ++kills; }
        m.Step(dt, kills);
    }
    const float r = m.PerSecond(kills);
    EXPECT(std::fabs(r - 2.0f) < 0.35f, "2/s reads %.2f", r);
    // Kills stop: within the window the reading falls to 0.
    for (float s = 0.0f; s < kSupplyWindowSeconds + 0.6f; s += dt) m.Step(dt, kills);
    EXPECT(m.PerSecond(kills) == 0.0f, "stopped reads %.2f", m.PerSecond(kills));
}

// A rough fight model: kills a second = firepower * alive / (alive + 8)
// (with few mobs the player spends time finding them). The old fixed floor
// vs the kill-rate floor, time to clear a 60-kill quota.
static float Clear(float firepower, bool linked) {
    const Difficulty d = Difficulty::NORMAL;
    KillRateMeter m;
    m.Reset(0);
    const float dt = 1.0f / 60.0f;
    float alive = 0.0f, kills = 0.0f, t = 0.0f;
    while (kills < 60.0f && t < 600.0f) {
        m.Step(dt, (long long)kills);
        const int target = linked ? RegularWaveFieldTarget(1, d, m.PerSecond((long long)kills))
                                  : RegularWaveMinAlive(1, d);
        if (alive < target) alive = (float)target;
        const float k = firepower * alive / (alive + 8.0f) * dt;
        kills += k; alive -= k; t += dt;
    }
    return t;
}

static void CheckWaveTime() {
    const float weakOld = Clear(1.6f, false), weakNew = Clear(1.6f, true);
    const float strongOld = Clear(3.2f, false), strongNew = Clear(3.2f, true);
    const float strongerNew = Clear(6.4f, true);
    std::printf("clear 60: weak %.1f -> %.1f s, strong %.1f -> %.1f s, 2x strong %.1f s\n",
                weakOld, weakNew, strongOld, strongNew, strongerNew);
    EXPECT(std::fabs(weakNew - weakOld) < 1.0f, "a weak build's wave is unchanged");
    EXPECT(strongNew < strongOld * 0.92f, "a strong build clears faster than before");
    EXPECT(strongerNew < strongNew * 0.6f, "double firepower -> about 0.6x the time or less");
}

int main() {
    CheckTarget();
    CheckMeter();
    CheckWaveTime();
    if (g_failures) { std::printf("mob supply test: %d failure(s)\n", g_failures); return 1; }
    std::printf("mob supply test passed\n");
    return 0;
}
