#include "ProjectileSystem.h"

#include "Bullet.h"
#include "MonsterManager.h"
#include "SpaceRules.h"
#include "PassiveHooks.h"
#include "../System/SpatialBounds.h"
#include <algorithm>
#include <cmath>

namespace {
float TurnAngleToward(float currentAngle, float targetAngle, float maxStep) {
    constexpr float kPi = 3.14159265f;
    constexpr float kTau = 6.2831853f;
    float difference = targetAngle - currentAngle;
    while (difference > kPi) difference -= kTau;
    while (difference < -kPi) difference += kTau;
    difference = std::max(-maxStep, std::min(maxStep, difference));
    return currentAngle + difference;
}
}

void UpdateProjectiles(std::vector<Bullet>& bullets,
                       const MonsterManager& monsters,
                       const SpatialBounds& playerBounds,
                       float fixedDelta,
                       float screenWidth, float screenHeight,
                       float viewZoom) {
    const float playerX = playerBounds.x + playerBounds.width * 0.5f;
    const float playerY = playerBounds.y + playerBounds.height * 0.5f;
    const float homingDelta = fixedDelta;

    for (auto& bullet : bullets) {
        ClampPlayerBulletSize(bullet);
        const float bulletDelta = fixedDelta;
        if (bullet.homing && !bullet.isEnemy && bullet.active) {
            float targetX = 0.0f, targetY = 0.0f;
            if (monsters.FindNearestEnemy(bullet.x, bullet.y, targetX, targetY)) {
                const float dx = targetX - bullet.x;
                const float dy = targetY - bullet.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length > 0.001f) {
                    const float currentAngle = std::atan2(bullet.dirY, bullet.dirX);
                    const float targetAngle = std::atan2(dy / length, dx / length);
                    const float nextAngle = TurnAngleToward(
                        currentAngle, targetAngle, bullet.homingTurn * fixedDelta);
                    bullet.dirX = std::cos(nextAngle);
                    bullet.dirY = std::sin(nextAngle);
                }
            }
        }
        if (bullet.homing && bullet.isEnemy && bullet.active) {
            if (bullet.homingDuration > 0.0f) {
                bullet.homingElapsed += bulletDelta;
                if (bullet.homingElapsed >= bullet.homingDuration)
                    bullet.homing = false;
            }
        }
        if (bullet.homing && bullet.isEnemy && bullet.active) {
            const float dx = playerX - bullet.x;
            const float dy = playerY - bullet.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 300.0f) {
                const float currentAngle = std::atan2(bullet.dirY, bullet.dirX);
                const float targetAngle = std::atan2(dy / length, dx / length);
                float turnRate = bullet.homingTurn;
                if (bullet.homingTurnStart > 0.0f &&
                    bullet.homingTurnRampStart < 1.0f) {
                    const float rampRange = 1.0f - bullet.homingTurnRampStart;
                    const float turnProgress = std::max(0.0f, std::min(1.0f,
                        (bullet.launchRamp - bullet.homingTurnRampStart) /
                            rampRange));
                    turnRate = bullet.homingTurnStart +
                        (bullet.homingTurn - bullet.homingTurnStart) * turnProgress;
                }
                const float nextAngle = TurnAngleToward(
                    currentAngle, targetAngle, turnRate * homingDelta);
                bullet.dirX = std::cos(nextAngle);
                bullet.dirY = std::sin(nextAngle);
            }
        }

        if (bullet.space < 0) {
            bullet.space = space::SpaceAt(bullet.x, bullet.y);
            bullet.firedAt = space::g_Clock;
        }
        const float prevX = bullet.x, prevY = bullet.y;
        bullet.Update(bulletDelta);
        // A shot whose space changed under it was swept by a moving pane's
        // border (stopped there), or a pane closed, was released or jumped
        // away (it just takes the space it now stands in).
        if (bullet.active &&
            space::SpaceAt(prevX, prevY) == space::SpaceAt(bullet.x, bullet.y)) {
            int sweepingPane = 0;
            if (space::SweptBy(bullet.space, prevX, prevY, bullet.x, bullet.y, sweepingPane)) {
                bullet.active = false;
                space::g_Absorbed.push_back({ sweepingPane, bullet.x, bullet.y, bullet.isEnemy });
                continue;
            }
            bullet.space = space::SpaceAt(bullet.x, bullet.y);
        }
        int absorbingPane = 0;
        if (bullet.active &&
            space::CrossedBorder(bullet.space, bullet.x, bullet.y, absorbingPane) &&
            !bullet.anyPaneStops && !space::Absorbs(absorbingPane, bullet.firedAt)) {
            bullet.space = space::SpaceAt(bullet.x, bullet.y);   // a pane born after the shot: pass
        } else if (bullet.active &&
            space::CrossedBorder(bullet.space, bullet.x, bullet.y, absorbingPane)) {
            bullet.active = false;
            // Every shot a border stops wears that pane, the player's own too.
            space::g_Absorbed.push_back({ absorbingPane, bullet.x, bullet.y, bullet.isEnemy });
            continue;
        }

        const float safeZoom = std::max(viewZoom, 0.01f);
        // 도탄: the arena is the area the zoomed-out view shows.
        if (bullet.active && !bullet.isEnemy && bullet.bouncesLeft > 0) {
            const float exX = screenWidth * 0.5f * (1.0f / safeZoom - 1.0f);
            const float exY = screenHeight * 0.5f * (1.0f / safeZoom - 1.0f);
            if (bullet.BounceInRect(-exX, screenWidth + exX, -exY, screenHeight + exY) &&
                passive_hooks::bulletBounce)
                passive_hooks::bulletBounce(bullet);
        }
        const float marginX = screenWidth * 0.5f * (1.0f / safeZoom - 1.0f) + 200.0f;
        const float marginY = screenHeight * 0.5f * (1.0f / safeZoom - 1.0f) + 200.0f;
        if (bullet.x < -marginX || bullet.x > screenWidth + marginX ||
            bullet.y < -marginY || bullet.y > screenHeight + marginY)
            bullet.active = false;
    }
}
