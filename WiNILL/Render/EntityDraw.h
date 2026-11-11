#pragma once
#include <vector>
#include "Bullet.h"
#include "Monster.h"

class RangedMob;
class MonsterManager;

void drawBullet(const Bullet& b);
void DrawBulletHalos(const std::vector<Bullet>& bullets);
void drawMob(const Monster* m, float visualTime = -1.0f);
void drawRangedMob(const RangedMob* r, float visualTime = -1.0f);
void DrawRegulusBuffMarks(const Monster* m, float visualTime);
void DrawLynxRay(const Monster* m);
void DrawMiraZones();
void DrawRegulusBeams(const MonsterManager& manager, float visualTime);
// Render an Astral Log entity preview with the same silhouette as gameplay.
void drawCodexMobPreview(int codexId, float x, float y, float scale = 3.0f);
void DrawEnemySightRear(float x, float y, float foregroundRadius,
                        float alpha = 0.7f);
struct EnemySightRearMarker {
    float x;
    float y;
    float foregroundRadius;
};
void DrawEnemySightRearBatch(const std::vector<EnemySightRearMarker>& markers,
                             float alpha = 0.7f);
void BeginEnemySightFrontBatch();
void QueueMonsterSightFront(const Monster* m);
void QueueRangedMobSightFront(const RangedMob* r);
void FlushEnemySightFrontBatch();
void DrawCombatSightFields(const MonsterManager& monsters,
                           float playerX, float playerY,
                           float playerSize, bool renderRearFields);
void DrawMonsterSightRear(const Monster* m);
void DrawRangedMobSightRear(const RangedMob* r);

void DrawApproachOrb(float x, float y, float visualTime);
