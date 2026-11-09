// TRIANGULUM mob: the three children's burst directions and the burst's reach.
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "TriangulumMob.h"

static int g_failures = 0;
#define EXPECT(cond, ...)                                    \
    do {                                                     \
        if (!(cond)) {                                       \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                        \
            std::printf("\n");                               \
            ++g_failures;                                    \
        }                                                    \
    } while (0)

int main() {
    using namespace triangulum;
    EXPECT(kChildCount == 3, "three children");

    // Directions are unit length, 120 degrees apart, the first one toward the player.
    float dx[kChildCount], dy[kChildCount];
    const float toward = 0.7f;   // ring angle
    for (int k = 0; k < kChildCount; ++k) ChildDir(k, toward, dx[k], dy[k]);
    EXPECT(std::fabs(dx[0] - std::cos(toward)) < 1e-5f && std::fabs(dy[0] - std::sin(toward)) < 1e-5f,
           "child 0 sits at the ring angle");
    for (int k = 0; k < kChildCount; ++k)
        EXPECT(std::fabs(dx[k] * dx[k] + dy[k] * dy[k] - 1.0f) < 1e-4f, "child %d is a unit direction", k);
    for (int k = 0; k < kChildCount; ++k) {
        const int n = (k + 1) % kChildCount;
        const float dot = dx[k] * dx[n] + dy[k] * dy[n];
        EXPECT(std::fabs(dot - std::cos(2.0f * kPi / 3.0f)) < 1e-4f, "children %d and %d sit 120 degrees apart", k, n);
    }
    // The three directions cancel out: the burst has no net push.
    EXPECT(std::fabs(dx[0] + dx[1] + dx[2]) < 1e-4f && std::fabs(dy[0] + dy[1] + dy[2]) < 1e-4f,
           "directions sum to zero");

    // Freed Swarms land between their cage spot and kScatterRadius, near their corner.
    for (int k = 0; k < kChildCount; ++k)
        for (float an : { -1.0f, 0.0f, 1.0f })
            for (float rn : { 0.0f, 0.5f, 1.0f }) {
                float ox, oy;
                ScatterOffset(k, toward, an, rn, ox, oy);
                const float r = std::sqrt(ox * ox + oy * oy);
                EXPECT(r <= kScatterRadius + 1e-3f && r > kOrbitRadius, "child %d lands within the scatter ring (%.1f)", k, r);
                const float cosOff = (ox * dx[k] + oy * dy[k]) / r;
                EXPECT(cosOff >= std::cos(kScatterJitter) - 1e-4f, "child %d stays near its corner", k);
            }
    EXPECT(kHoldTime > 0.0f && kShatterTime > 0.0f && kBurstTime > 0.0f, "death timings");
    EXPECT(kHpMult > 1.0f && kSpeedMult < 1.0f, "slow and tough");

    if (g_failures == 0) std::printf("triangulum mob regression passed\n");
    return g_failures == 0 ? 0 : 1;
}
