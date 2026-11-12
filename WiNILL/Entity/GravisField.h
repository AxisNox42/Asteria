#pragma once
// Gravis field: a pull on the player (capped against move speed so it
// never becomes a cage) plus time dilation that slows fire and dash recovery.
// The pull is an acceleration: drift velocity builds up while the player
// stays in the field and coasts out after leaving it.
// Pure math lives here so it can be regression-tested headlessly.
#include <algorithm>
#include <cmath>
#include <vector>

class Bullet;
class MonsterManager;

namespace GravisField {

inline constexpr float RADIUS            = 570.0f;   // 2x the original 285; strength below is softened to match
inline constexpr float CORE_DEAD_ZONE_SQ = 36.0f;   // no pull right on the core
inline constexpr float PULL_BASE         = 50.0f;   // px/s at the rim
inline constexpr float PULL_GAIN         = 240.0f;  // extra px/s toward the core
inline constexpr float PULL_CAP_RATIO    = 0.80f;   // of the player's move speed
inline constexpr float FIRE_SLOW         = 0.40f;   // +40% fire interval at the core
inline constexpr float DASH_RECOVER_SLOW = 0.50f;   // -50% dash cooldown regen
inline constexpr float PULL_RESPONSE     = 1.6f;    // 1/s: drift settles on the local pull in ~0.6 s
inline constexpr float PULL_COAST_DRAG   = 2.4f;    // 1/s: drift bleeds off outside every field

// Entrance: a new Gravis glides in from its spawn edge with no field; once it
// is on screen the field grows out of the body from 0 px to RADIUS.
inline constexpr float ENTRY_SPEED       = 220.0f;  // px/s from the spawn edge into the view
inline constexpr float GROW_TIME         = 1.4f;    // s for the field to reach RADIUS

// Field radius scale for a Gravis this far (0..1) into its growth; eases out
// so the ring opens quickly and settles onto the rim.
inline float GrowScale(float progress) {
    const float t = std::clamp(progress, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// 0 at the rim (and beyond), 1 on the core.
inline float Influence(float distSq, float radius = RADIUS) {
    if (radius <= 1.0f || distSq >= radius * radius) return 0.0f;
    return 1.0f - std::sqrt(distSq) / radius;
}

inline float PullSpeed(float influence) {
    return (PULL_BASE + PULL_GAIN * influence) * influence;
}

inline float PullCap(float moveSpeed) { return moveSpeed * PULL_CAP_RATIO; }

inline float FireIntervalMult(float influence)  { return 1.0f + FIRE_SLOW * influence; }
inline float DashRecoveryMult(float influence)  { return 1.0f - DASH_RECOVER_SLOW * influence; }

// Sums the pull of every Gravis on the player; dilation keeps only the
// strongest influence so overlapping fields never stack.
struct FieldAccum {
    float pullVX = 0.0f;
    float pullVY = 0.0f;
    float maxInfluence = 0.0f;

    // (gx, gy) = vector from the player to a Gravis; radius = its field
    // radius right now (smaller while the field is still growing).
    void Add(float gx, float gy, float radius = RADIUS) {
        const float distSq = gx * gx + gy * gy;
        const float influence = Influence(distSq, radius);
        if (influence <= 0.0f) return;
        maxInfluence = std::max(maxInfluence, influence);
        if (distSq <= CORE_DEAD_ZONE_SQ) return;
        const float distance = std::sqrt(distSq);
        const float pull = PullSpeed(influence);
        pullVX += gx / distance * pull;
        pullVY += gy / distance * pull;
    }

    void ClampPull(float cap) {
        const float length = std::sqrt(pullVX * pullVX + pullVY * pullVY);
        if (length > cap && length > 0.0001f) {
            pullVX *= cap / length;
            pullVY *= cap / length;
        }
    }
};

// Drift velocity the field has built up on the player. Gravity accelerates
// it toward the local pull (PullSpeed, i.e. the terminal speed) and drag
// bleeds it off, so the pull ramps up over time and carries past the rim.
struct PullDrift {
    float vx = 0.0f;
    float vy = 0.0f;

    void Reset() { vx = vy = 0.0f; }

    // (targetVX, targetVY) = summed, already-capped pull of every field;
    // inField = false switches to coasting drag.
    void Step(float targetVX, float targetVY, bool inField, float cap, float dt) {
        const float rate = inField ? PULL_RESPONSE : PULL_COAST_DRAG;
        const float keep = std::exp(-rate * dt);
        vx = targetVX + (vx - targetVX) * keep;
        vy = targetVY + (vy - targetVY) * keep;
        const float length = std::sqrt(vx * vx + vy * vy);
        if (length > cap && length > 0.0001f) {
            vx *= cap / length;
            vy *= cap / length;
        }
        if (length < 0.5f && !inField) Reset();
    }
};

inline PullDrift g_playerDrift;

// Latest field influence on the player; written by the fixed step, read by the
// fire and dash-cooldown code.
inline float g_playerInfluence = 0.0f;

// One fixed step of every live Gravis: accelerates the player's drift
// (capped; a dash cuts it), updates g_playerInfluence and curves bullets.
void Apply(const MonsterManager& monsters, std::vector<Bullet>& bullets,
           float& playerX, float& playerY, bool playerControl, bool dashActive,
           float moveSpeed, float dt);

}  // namespace GravisField
