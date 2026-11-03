// Headless regression for the elite (Quasar, Genesis) death: no body
// animation, so the held kill reward is released at once, exactly once, and
// the dead mob is removed on the next update.
// Run with scripts/test_quasar_nova.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static constexpr float DT = 1.0f / 120.0f;

static Monster* KillElite(MonsterManager& mm, MobKind kind, bool animate) {
    auto* q = new Monster(0.0f, 0.0f);
    q->MakeKind(kind);
    q->alive = false;
    q->scored = true;
    q->dustPending = true;
    q->pendingDustValue = 7;
    q->pendingDustXp = XpFromWhole(123);
    q->SpawnDeathEffect(0.0f, true, animate);
    mm.monsters.push_back(q);
    return q;
}

static void CheckReleasedAtOnce(MobKind kind, bool animate) {
    MonsterManager mm;
    std::vector<Bullet> bullets;
    float hp = 100.0f;
    Monster* q = KillElite(mm, kind, animate);
    assert(!q->DeathEffectVisible());                      // no body animation
    assert(q->TakePendingDust());
    assert(q->pendingDustValue == 7 && q->pendingDustXp == XpFromWhole(123));
    assert(!q->TakePendingDust());
    mm.UpdateAll(5000.0f, 0.0f, DT, hp, bullets);
    assert(mm.monsters.empty());
}

static void CheckPendingBlocksRemoval() {
    MonsterManager mm;
    std::vector<Bullet> bullets;
    float hp = 100.0f;
    Monster* q = KillElite(mm, MobKind::QUASAR, true);
    mm.UpdateAll(5000.0f, 0.0f, DT, hp, bullets);
    assert(mm.monsters.size() == 1);                       // kept until released
    assert(q->TakePendingDust());
    mm.UpdateAll(5000.0f, 0.0f, DT, hp, bullets);
    assert(mm.monsters.empty());
    std::printf("pending reward blocks removal: ok\n");
}

int main() {
    for (const auto kind : {MobKind::QUASAR, MobKind::GENESIS})
        for (bool animate : {true, false})
            CheckReleasedAtOnce(kind, animate);
    std::printf("elite death releases the reward at once: ok\n");
    CheckPendingBlocksRemoval();
    std::printf("quasar nova regression passed\n");
    return 0;
}
