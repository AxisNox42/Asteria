#include "GravisField.h"

#include "Bullet.h"
#include "MonsterManager.h"
#include "SpaceRules.h"
#include "../System/Audio.h"
#include <cmath>

namespace GravisField {

void Apply(const MonsterManager& monsters, std::vector<Bullet>& bullets,
           float& playerX, float& playerY, bool playerControl, bool dashActive,
           float moveSpeed, float dt) {
    // Pull and time dilation both come from the same field; the pull
    // accelerates a drift capped against the player's move speed, dilation
    // keeps the strongest.
    FieldAccum field;
    for (const Monster* gravis : monsters.monsters) {
        if (!gravis || !gravis->alive || gravis->kind != MobKind::GRAVIS) continue;
        // The field stops at window borders like every other effect.
        if (!space::SameSpace(gravis->worldX, gravis->worldY, playerX, playerY)) continue;
        field.Add(gravis->worldX - playerX, gravis->worldY - playerY);
    }
    g_playerInfluence = playerControl ? field.maxInfluence : 0.0f;
    // 플레이어가 어느 중력장 안에든 있으면 웅웅 루프 (Start/Stop 은 중복 호출 무시)
    if (g_playerInfluence > 0.0f) Audio::StartLoop(Audio::Cue::EnemyGravisLoop);
    else Audio::StopLoop(Audio::Cue::EnemyGravisLoop);
    if (!playerControl || dashActive) {
        // A dash breaks free of the built-up drift; menus drop it too.
        g_playerDrift.Reset();
    } else {
        const float cap = PullCap(moveSpeed);
        field.ClampPull(cap);
        g_playerDrift.Step(field.pullVX, field.pullVY, field.maxInfluence > 0.0f, cap, dt);
        playerX += g_playerDrift.vx * dt;
        playerY += g_playerDrift.vy * dt;
    }

    // The field also curves every live bullet toward its Gravis.
    constexpr float fieldRadiusSq = RADIUS * RADIUS;
    for (const Monster* gravis : monsters.monsters) {
        if (!gravis || !gravis->alive || gravis->kind != MobKind::GRAVIS) continue;
        for (Bullet& bullet : bullets) {
            if (!bullet.active) continue;
            if (!space::SameSpace(gravis->worldX, gravis->worldY, bullet.x, bullet.y)) continue;
            const float bx = gravis->worldX - bullet.x;
            const float by = gravis->worldY - bullet.y;
            const float distanceSq = bx * bx + by * by;
            if (distanceSq <= 64.0f || distanceSq >= fieldRadiusSq) continue;

            const float distance = std::sqrt(distanceSq);
            const float influence = 1.0f - distance / RADIUS;
            const float curve = (0.50f + 1.80f * influence) * dt;
            bullet.dirX += bx / distance * curve;
            bullet.dirY += by / distance * curve;
            const float directionLength =
                std::sqrt(bullet.dirX * bullet.dirX + bullet.dirY * bullet.dirY);
            if (directionLength > 0.0001f) {
                bullet.dirX /= directionLength;
                bullet.dirY /= directionLength;
            }
        }
    }
}

}  // namespace GravisField
