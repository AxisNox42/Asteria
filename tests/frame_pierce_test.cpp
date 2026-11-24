// Run scripts/test_frame_pierce.ps1. 저격총 위력 관통 and 석궁 박힌 화살
// (Docs/Plans/2026-11-24_석궁_저격총_개편_기획.docx): the plan's example tables, hit by hit.
#include "Bullet.h"
#include "CrossbowBolts.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// One hit of a sniper shot whose first-hit power is `power` on an enemy with `hp` left
// (no shield, no other multipliers): what it deals, and whether it flies on.
static bool Hit(Bullet& b, float power, float& hp, float& dealtOut) {
    const float damage = power * b.powerLeft;
    const float dealt = std::fmin(hp, damage);
    hp -= dealt;
    dealtOut = dealt;
    return SniperFliesOn(b, damage, dealt, hp <= 0.0f);
}

int main() {
    // 3.2: power 1000, no pierce card: A(300) dies -> 700; B(500) dies -> 200; C(400) takes 200, stops.
    {
        Bullet b(0, 0, 1, 0); b.powerPierce = true;
        float hp = 300, d = 0;
        assert(Hit(b, 1000, hp, d) && Near(d, 300) && Near(b.powerLeft * 1000, 700));
        hp = 500;
        assert(Hit(b, 1000, hp, d) && Near(d, 500) && Near(b.powerLeft * 1000, 200));
        hp = 400;
        assert(!Hit(b, 1000, hp, d) && Near(d, 200) && Near(hp, 200));
    }
    // 3.3: power 1000 with 겹겹 관통탄 (pierce 2): A(300) free; B(1500) free though it lives;
    // C(500) dies -> 500 left; D(800) takes 500, stops.
    {
        Bullet b(0, 0, 1, 0); b.powerPierce = true; b.pierceLeft = 2;
        float hp = 300, d = 0;
        assert(Hit(b, 1000, hp, d) && Near(d, 300) && b.pierceLeft == 1 && Near(b.powerLeft, 1.0f));
        hp = 1500;
        assert(Hit(b, 1000, hp, d) && Near(d, 1000) && b.pierceLeft == 0 && Near(b.powerLeft, 1.0f));
        hp = 500;
        assert(Hit(b, 1000, hp, d) && Near(b.powerLeft * 1000, 500));
        hp = 800;
        assert(!Hit(b, 1000, hp, d) && Near(d, 500) && Near(hp, 300));
    }
    // Under 10% of its first power it ends, even on a kill.
    {
        Bullet b(0, 0, 1, 0); b.powerPierce = true;
        float hp = 950, d = 0;
        assert(!Hit(b, 1000, hp, d) && hp <= 0.0f);
        Bullet c(0, 0, 1, 0); c.powerPierce = true;
        hp = 900;
        assert(Hit(c, 1000, hp, d) && Near(c.powerLeft, 0.1f));
    }
    // An execute is not the shot's kill: it stops. A shield took part of the hit: that part is used too.
    {
        Bullet b(0, 0, 1, 0); b.powerPierce = true;
        assert(!SniperFliesOn(b, 1000, 1000, false));
        Bullet c(0, 0, 1, 0); c.powerPierce = true;
        assert(SniperFliesOn(c, 1000, 400 /* 100 shield + 300 hp */, true) && Near(c.powerLeft, 0.6f));
    }
    // A shot that pierces everything (집요한 사냥탄) never loses power.
    {
        Bullet b(0, 0, 1, 0); b.powerPierce = true; b.piercing = true;
        assert(SniperFliesOn(b, 1000, 10, false) && Near(b.powerLeft, 1.0f));
    }

    // 석궁: the first bolt marks (+0%), then each bolt into the mark +15% more, capped at +75%.
    {
        crossbow::Mark a;
        assert(Near(crossbow::DamageMult(a), 1.0f));
        const float want[] = { 1.15f, 1.30f, 1.45f, 1.60f, 1.75f, 1.75f, 1.75f };
        for (float w : want) { crossbow::Hit(a); assert(Near(crossbow::DamageMult(a), w)); }
        assert(a.n == crossbow::kMaxStacks && Near(a.t, crossbow::kHold));
        // Held 4 s after the last hit, then all gone at once; a hit refills the 4 s.
        assert(crossbow::Age(a, 3.0f));
        crossbow::Hit(a);
        assert(crossbow::Age(a, 3.9f) && a.n == 5);
        assert(!crossbow::Age(a, 0.2f));
    }
    std::printf("frame pierce: OK\n");
    return 0;
}
