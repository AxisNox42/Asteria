// MIRA mob: the zone's reach, the dead zone's lifetime and fade, the slow.
#include <cstdio>

#include "MiraMob.h"

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
    using namespace mira;
    // Reach.
    EXPECT(InZone(0, 0, 0, 0), "centre is inside");
    EXPECT(InZone(0, 0, kZoneRadius, 0), "edge is inside");
    EXPECT(!InZone(0, 0, kZoneRadius + 20.0f, 0), "past the edge is outside");
    EXPECT(InZone(0, 0, kZoneRadius + 12.0f, 0, 1.5f), "a bigger player reaches further");

    // A dead zone lasts kDeathLife, hurts the whole time while it shrinks away.
    Reset();
    AddZone(100, 100);
    EXPECT(g_zones.size() == 1, "one zone");
    bool hurtEveryStep = true;
    float t = 0.0f;
    const float dt = 0.1f;
    while (t < kDeathLife - 0.15f) { hurtEveryStep &= Tick(dt, 100, 100); t += dt; }
    EXPECT(hurtEveryStep, "hurts for the whole life");
    EXPECT(ZoneRadius(g_zones.front()) < kZoneRadius * 0.2f, "nearly shrunk away at the end");
    for (int i = 0; i < 5; ++i) Tick(dt, 100, 100);
    EXPECT(g_zones.empty(), "gone after %.1fs", kDeathLife);
    EXPECT(!Tick(dt, 100, 100), "nothing left to stand in");

    // Shrinking: the hurt area follows the radius.
    Reset();
    AddZone(0, 0);
    Tick(1.5f, 999, 999);
    EXPECT(ZoneRadius(g_zones.front()) < kZoneRadius * 0.6f && ZoneRadius(g_zones.front()) > kZoneRadius * 0.4f, "half way: radius %.1f", ZoneRadius(g_zones.front()));
    EXPECT(!Tick(0.0f, kZoneRadius * 0.7f + 30.0f, 0), "shrunk zone no longer reaches the old edge");
    EXPECT(Tick(0.0f, kZoneRadius * 0.3f, 0), "but still holds the middle");
    Reset();
    AddTrail(0, 0, 0);
    Tick(kTrailLife * 0.5f, 999, 999);
    EXPECT(ZoneRadius(g_zones.front()) < kTrailRadius * 0.6f, "trail puddle shrinks too");
    Tick(kTrailLife, 999, 999);
    EXPECT(g_zones.empty(), "trail puddle is gone");

    // Joined puddles hurt in between; separate ones do not.
    Reset();
    const int c = NewChain();
    AddTrail(0, 0, c);
    AddTrail(200, 0, c);
    EXPECT(Tick(0.0f, 100, 0), "between two joined puddles is hurt");
    EXPECT(!Tick(0.0f, 100, 200), "beside the ribbon is not");
    Reset();
    AddTrail(0, 0, 0);
    AddTrail(200, 0, 0);
    EXPECT(!Tick(0.0f, 100, 0), "unjoined puddles leave a gap");
    Reset();
    AddTrail(0, 0, c);
    AddTrail(200, 0, c);
    Tick(kTrailLife * 0.5f, 999, 999);
    EXPECT(!Tick(0.0f, 100, 40), "the ribbon narrows as it shrinks");

    // The walking MIRA is the ribbon's head: the stretch from the newest puddle to it hurts.
    Reset();
    const int ch = NewChain();
    AddTrail(0, 0, ch);
    Tick(0.0f, 999, 999);
    SetHead(ch, 200, 0);
    EXPECT(Tick(0.0f, 100, 0), "between the newest puddle and the head is hurt");
    EXPECT(HeadOf(ch) == nullptr, "a head not refreshed this tick is stale");
    Tick(0.0f, 999, 999);
    EXPECT(g_heads.empty(), "stale head dropped");

    // Standing outside never hurts; the slow lingers then ends.
    Reset();
    AddZone(0, 0);
    EXPECT(!Tick(dt, 500, 0), "far player is not hurt");
    EXPECT(MoveFactor() == 1.0f, "no slow outside");
    Tick(dt, 0, 0);
    EXPECT(MoveFactor() == kZoneMove, "slowed inside");
    Reset();
    Touch();
    EXPECT(MoveFactor() == kZoneMove, "slowed after a living zone touch");
    for (int i = 0; i < 12; ++i) Tick(dt, 999, 999);
    EXPECT(MoveFactor() == 1.0f, "slow ends after the linger");

    if (g_failures == 0) std::printf("mira_mob_test: all passed\n");
    return g_failures ? 1 : 0;
}
