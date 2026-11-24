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
// Distance from a mob's centre to just outside its silhouette (where shield brackets and buff marks sit).
float MobOutlineExtent(const Monster* m);
float MobOutlineExtent(const RangedMob* r);
void DrawLynxRay(const Monster* m);
void DrawMiraZones();
// Nightmare Antares wake and clash rings (AntaresWake.h).
void DrawAntaresWake();
void DrawRegulusBeams(const MonsterManager& manager, float visualTime);
// 레이저 frame card: the player's beams (LaserFx.h).
void DrawPlayerLasers();
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
