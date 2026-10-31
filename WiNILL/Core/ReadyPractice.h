#pragma once
// ─────────────────────────────────────────────────────────────
// READY screen practice field: the player's ship and four Swarm training
// dummies in a small arena. No GL and no game headers, so
// tests/ready_screen_test.cpp can build it alone.
//   The arena runs on the real game's rules and units: world pixels, and a
//   Rules block the READY scene fills from the live game every frame (move
//   speed with the stat multiplier, dash distance/duration/cooldown, weapon
//   fire interval, bullet speed and damage, Static Field pulse, the Swarm's
//   hp and hit radius). Movement, the dash, firing and the swept bullet hit
//   test follow the same formulas as combat (PlayerRuntime.cpp,
//   CollisionSystem.h), so what the player learns here carries over.
//   Dummies never move from their spot; a hit only knocks them a little and
//   they settle back. A killed dummy comes back on the same spot after
//   kRespawnDelay. Input arrives already resolved through the key bindings.
//   Steps unlock in order (GateFor): an action of a later step does nothing
//   until its step comes, so the lesson cannot be cleared out of order.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ready_practice {

inline constexpr int   kDummyCount   = 4;
inline constexpr float kRespawnDelay = 3.0f;
inline constexpr float kFormDuration = 0.6f;   // last part of the delay: the Swarm grows back
inline constexpr float kKnockPerHit  = 12.0f;  // world px
inline constexpr float kKnockDecay   = 9.0f;   // per second, exponential
inline constexpr float kWobblePerHit = 0.6f;
inline constexpr float kWobbleDecay  = 5.0f;
inline constexpr float kHitFlash     = 0.1f;

// Dummy spots and the player's start, as fractions of the arena.
inline constexpr float kDummySpot[kDummyCount][2] = {
    { 0.188f, 0.289f }, { 0.388f, 0.722f },
    { 0.612f, 0.278f }, { 0.812f, 0.689f },
};
inline constexpr float kPlayerStart[2] = { 0.5f, 0.52f };

enum class Objective { Move, Shoot, Dash, Kill, Count };
inline constexpr int   kObjectiveCount = (int)Objective::Count;
inline constexpr float kMoveGoal = 0.5f;   // arena heights travelled
inline constexpr int   kShotGoal = 5;
inline constexpr int   kDashGoal = 1;
inline constexpr int   kKillGoal = kDummyCount;

// The live game's numbers, in world px and seconds.
struct Rules {
    float worldW = 1067.0f, worldH = 600.0f;
    float playerRadius = 12.5f;       // keeps the ship's centre this far inside
    float moveSpeed = 400.0f;         // already multiplied by the move stat
    float dashDist = 300.0f, dashDur = 0.11f, dashCooldown = 2.9f;
    float fireInterval = 0.2f;
    float bulletSpeed = 1200.0f;
    float bulletDamage = 10.0f;
    bool  fieldPulse = false;         // Static Field: periodic area damage
    float pulseInterval = 0.24f, pulseRadius = 190.0f, pulseDamage = 4.0f;
    float dummyHp = 20.0f;
    float dummyHitRadius = 15.0f;
    bool  dashAvailable = true;       // false when the dash shares the start key
};

struct Input {
    bool up = false, down = false, left = false, right = false;
    bool dashHeld = false;
    bool fireHeld = false;
    bool aimValid = false;
    float aimX = 0.0f, aimY = 0.0f;   // world px
};

struct Dummy {
    float homeX = 0.0f, homeY = 0.0f;
    float knockX = 0.0f, knockY = 0.0f;
    float wobble = 0.0f;
    float hitFlash = 0.0f;
    float hp = 0.0f, maxHp = 0.0f;
    float respawn = 0.0f;   // > 0 while dead: seconds until it is back
    bool Alive() const { return respawn <= 0.0f; }
};

struct Bullet { float x, y, prevX, prevY, dirX, dirY; };

struct Field {
    float worldW = 0.0f, worldH = 0.0f;
    float playerX = 0.0f, playerY = 0.0f;
    float aim = -1.5707963f;
    // Dash, as PlayerRuntime: a lerp from -> to over dashDur.
    bool  dashActive = false;
    float dashProgress = 0.0f, dashCooldown = 0.0f;
    float dashFromX = 0.0f, dashFromY = 0.0f, dashToX = 0.0f, dashToY = 0.0f;
    bool  dashPrev = false;
    float fireTimer = 0.0f;
    float pulseTimer = 0.0f;
    int   pulses = 0;                 // for the renderer's ring
    std::array<Dummy, kDummyCount> dummies{};
    std::vector<Bullet> bullets;
    float moved = 0.0f;
    int   shots = 0, dashes = 0, kills = 0;
};

inline void Reset(Field& f, const Rules& r) {
    f = Field{};
    f.worldW = r.worldW;
    f.worldH = r.worldH;
    f.playerX = r.worldW * kPlayerStart[0];
    f.playerY = r.worldH * kPlayerStart[1];
    f.fireTimer = r.fireInterval;     // ready to fire at once, as in a run
    for (int i = 0; i < kDummyCount; ++i) {
        Dummy& d = f.dummies[i];
        d.homeX = r.worldW * kDummySpot[i][0];
        d.homeY = r.worldH * kDummySpot[i][1];
        d.hp = d.maxHp = r.dummyHp;
    }
}

// Where the dummy is drawn and hit: its spot plus the knockback.
inline float DummyX(const Dummy& d) { return d.homeX + d.knockX; }
inline float DummyY(const Dummy& d) { return d.homeY + d.knockY; }

// 0 → 1 over the whole respawn delay (1 once alive).
inline float RespawnProgress(const Dummy& d) {
    if (d.Alive()) return 1.0f;
    return std::clamp(1.0f - d.respawn / kRespawnDelay, 0.0f, 1.0f);
}
// 0 until the last kFormDuration of the delay, then 0 → 1 as it grows back.
inline float FormProgress(const Dummy& d) {
    if (d.Alive()) return 1.0f;
    return std::clamp(1.0f - d.respawn / kFormDuration, 0.0f, 1.0f);
}

inline bool ObjectiveDone(const Field& f, Objective o) {
    switch (o) {
    case Objective::Move:  return f.moved >= kMoveGoal * f.worldH;
    case Objective::Shoot: return f.shots >= kShotGoal;
    case Objective::Dash:  return f.dashes >= kDashGoal;
    case Objective::Kill:  return f.kills >= kKillGoal;
    default:               return false;
    }
}

// The step the screen is teaching now: the first objective, in order, that
// is not done yet. An objective the player cannot practise here (the dash
// when it is bound to the start key) is passed over. Objective::Count once
// every step is done: time to launch.
inline Objective CurrentObjective(const Field& f, bool dashAvailable) {
    for (int i = 0; i < kObjectiveCount; ++i) {
        const Objective o = (Objective)i;
        if (o == Objective::Dash && !dashAvailable) continue;
        if (!ObjectiveDone(f, o)) return o;
    }
    return Objective::Count;
}

// What the player may do during a step: only the actions of the steps
// reached so far, so no step can be cleared ahead of its turn.
//   Move: walk only.  Shoot: + fire (shots hit but deal no damage).
//   Dash: + dash.     Kill: + damage, so dummies can die.
//   A dash that cannot be practised here stays locked throughout.
struct Gate { bool fire, dash, damage; };
inline Gate GateFor(Objective current, bool dashAvailable) {
    const int step = (int)current;
    return { step >= (int)Objective::Shoot,
             dashAvailable && step >= (int)Objective::Dash,
             step >= (int)Objective::Kill };
}

namespace detail {
inline float SegDistSq(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float qx = ax + dx * t - px, qy = ay + dy * t - py;
    return qx * qx + qy * qy;
}

inline void Damage(Field& f, Dummy& d, float amount, float pushX, float pushY) {
    d.hp -= amount;   // 0 before the kill step: the hit still flashes and knocks
    d.knockX += pushX * kKnockPerHit;
    d.knockY += pushY * kKnockPerHit;
    d.wobble += kWobblePerHit;
    d.hitFlash = kHitFlash;
    if (d.hp <= 0.0f) {
        d.respawn = kRespawnDelay;
        d.knockX = d.knockY = d.wobble = 0.0f;
        ++f.kills;
    }
}
} // namespace detail

inline void Step(Field& f, const Rules& r, const Input& in, float dt) {
    if (!(dt > 0.0f)) return;
    const Gate gate = GateFor(CurrentObjective(f, r.dashAvailable), r.dashAvailable);
    const float minX = r.playerRadius, maxX = f.worldW - r.playerRadius;
    const float minY = r.playerRadius, maxY = f.worldH - r.playerRadius;

    if (in.aimValid) {
        const float ax = in.aimX - f.playerX, ay = in.aimY - f.playerY;
        if (ax * ax + ay * ay > 1e-6f) f.aim = std::atan2(ay, ax);
    }

    // Movement (UpdatePlayerMovement): normalised input, no walking mid-dash.
    float dirX = (in.right ? 1.0f : 0.0f) - (in.left ? 1.0f : 0.0f);
    float dirY = (in.down ? 1.0f : 0.0f) - (in.up ? 1.0f : 0.0f);
    const float len = std::sqrt(dirX * dirX + dirY * dirY);
    if (len > 0.001f) { dirX /= len; dirY /= len; }
    const float ox = f.playerX, oy = f.playerY;
    if (len > 0.001f && !f.dashActive) {
        f.playerX += dirX * r.moveSpeed * dt;
        f.playerY += dirY * r.moveSpeed * dt;
    }

    // Dash (BeginPlayerDash / AdvancePlayerDash): on the press, toward the
    // movement keys or else the aim, clamped to the arena.
    f.dashCooldown = std::max(0.0f, f.dashCooldown - dt);
    if (f.dashActive) {
        f.dashProgress = std::min(1.0f, f.dashProgress + dt / r.dashDur);
        f.playerX = f.dashFromX + (f.dashToX - f.dashFromX) * f.dashProgress;
        f.playerY = f.dashFromY + (f.dashToY - f.dashFromY) * f.dashProgress;
        if (f.dashProgress >= 1.0f) f.dashActive = false;
    }
    if (gate.dash && in.dashHeld && !f.dashPrev && f.dashCooldown <= 0.0f && !f.dashActive) {
        float ddx = dirX, ddy = dirY;
        if (len <= 0.001f) { ddx = std::cos(f.aim); ddy = std::sin(f.aim); }
        f.dashFromX = f.playerX;
        f.dashFromY = f.playerY;
        f.dashToX = std::clamp(f.playerX + ddx * r.dashDist, minX, maxX);
        f.dashToY = std::clamp(f.playerY + ddy * r.dashDist, minY, maxY);
        f.dashActive = true;
        f.dashProgress = 0.0f;
        f.dashCooldown = r.dashCooldown;
        ++f.dashes;
    }
    f.dashPrev = in.dashHeld;

    f.playerX = std::clamp(f.playerX, minX, maxX);
    f.playerY = std::clamp(f.playerY, minY, maxY);
    f.moved += std::hypot(f.playerX - ox, f.playerY - oy);

    // Firing (main loop): the timer always runs; a held button fires once
    // it reaches the interval, then the timer restarts.
    f.fireTimer += dt;
    if (gate.fire && in.fireHeld && f.fireTimer >= r.fireInterval) {
        const float c = std::cos(f.aim), s = std::sin(f.aim);
        f.bullets.push_back({ f.playerX, f.playerY, f.playerX, f.playerY, c, s });
        ++f.shots;
        f.fireTimer = 0.0f;
    }

    for (Dummy& d : f.dummies) {
        const float knockKeep = std::exp(-dt * kKnockDecay);
        d.knockX *= knockKeep;
        d.knockY *= knockKeep;
        d.wobble *= std::exp(-dt * kWobbleDecay);
        d.hitFlash = std::max(0.0f, d.hitFlash - dt);
        if (!d.Alive()) {
            d.respawn -= dt;
            if (d.Alive()) { d.respawn = 0.0f; d.hp = d.maxHp; }
        }
    }

    // Static Field pulse: area damage around the ship on a fixed beat.
    const float pulseDamage = gate.damage ? r.pulseDamage : 0.0f;
    const float bulletDamage = gate.damage ? r.bulletDamage : 0.0f;
    if (r.fieldPulse && gate.fire) {
        f.pulseTimer -= dt;
        if (f.pulseTimer <= 0.0f) {
            ++f.pulses;
            for (Dummy& d : f.dummies) {
                if (!d.Alive()) continue;
                const float dx = DummyX(d) - f.playerX, dy = DummyY(d) - f.playerY;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > r.pulseRadius) continue;
                detail::Damage(f, d, pulseDamage, dist > 0.0f ? dx / dist : 0.0f,
                               dist > 0.0f ? dy / dist : 0.0f);
            }
            f.pulseTimer += r.pulseInterval;
        }
    } else {
        f.pulseTimer = 0.0f;
    }

    // Bullets: swept hit test against each dummy's hit radius (CollisionSystem).
    for (Bullet& b : f.bullets) {
        b.prevX = b.x; b.prevY = b.y;
        b.x += b.dirX * r.bulletSpeed * dt;
        b.y += b.dirY * r.bulletSpeed * dt;
    }
    f.bullets.erase(std::remove_if(f.bullets.begin(), f.bullets.end(), [&](const Bullet& b) {
        for (Dummy& d : f.dummies) {
            if (!d.Alive()) continue;
            if (detail::SegDistSq(DummyX(d), DummyY(d), b.prevX, b.prevY, b.x, b.y) >=
                r.dummyHitRadius * r.dummyHitRadius) continue;
            detail::Damage(f, d, bulletDamage, b.dirX, b.dirY);
            return true;
        }
        return b.x < 0.0f || b.x > f.worldW || b.y < 0.0f || b.y > f.worldH;
    }), f.bullets.end());
}

} // namespace ready_practice
