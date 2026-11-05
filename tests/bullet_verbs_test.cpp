// Run scripts/test_bullet_verbs.ps1. Gun verbs on Bullet: bounce, pierce count.
#include "Bullet.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    // A shot flying left bounces off the left edge and keeps its speed.
    {
        Bullet b(10, 100, -100, 100);
        b.bouncesLeft = 2;
        b.x = -6.0f;
        assert(b.BounceInRect(0, 800, 0, 600));
        assert(b.x == 6.0f && b.dirX > 0.99f && b.bouncesLeft == 1);
        assert(!b.BounceInRect(0, 800, 0, 600));   // inside: nothing
        b.x = 805.0f;
        assert(b.BounceInRect(0, 800, 0, 600) && b.dirX < -0.99f && b.bouncesLeft == 0);
        b.x = 900.0f;
        assert(!b.BounceInRect(0, 800, 0, 600));   // out of bounces: flies on
    }
    // A corner turns both axes at once but costs one bounce.
    {
        Bullet b(10, 10, 1, 1);
        b.bouncesLeft = 3;
        b.x = -4.0f; b.y = 604.0f;
        assert(b.BounceInRect(0, 800, 0, 600));
        assert(b.dirX > 0 && b.dirY < 0 && b.bouncesLeft == 2);
    }
    // A bounce lets the shot hit the same enemy again.
    {
        Bullet b(0, 0, 1, 0);
        int target;
        b.bouncesLeft = 1; b.pierceLeft = 5;
        b.RememberHit(&target);
        assert(b.HasHit(&target));
        b.x = -1.0f;
        b.BounceInRect(0, 100, 0, 100);
        assert(!b.HasHit(&target));
    }
    // pierceLeft = 2 passes two enemies and ends at the third.
    {
        Bullet b(0, 0, 1, 0);
        b.pierceLeft = 2;
        assert(b.SurvivesHit() && b.SurvivesHit());
        assert(!b.SurvivesHit());
        Bullet p(0, 0, 1, 0);
        p.piercing = true;
        for (int i = 0; i < 20; ++i) assert(p.SurvivesHit());   // unlimited, as before
        Bullet plain(0, 0, 1, 0);
        assert(!plain.SurvivesHit());
    }
    // A step longer than the arena is still turned back inside it, once.
    {
        Bullet b(10, 10, 100, 10);
        b.bouncesLeft = 2;
        b.x = -900.0f;
        assert(b.BounceInRect(0, 100, 0, 100));
        assert(b.x >= 0.0f && b.x <= 100.0f && b.bouncesLeft == 1);
    }
    // Without bounces nothing changes.
    {
        Bullet b(0, 0, 1, 0);
        b.x = -50.0f;
        assert(!b.BounceInRect(0, 100, 0, 100) && b.x == -50.0f);
    }
    std::puts("Bullet verbs: bounce, corner, re-hit after bounce, pierce count OK");
}
