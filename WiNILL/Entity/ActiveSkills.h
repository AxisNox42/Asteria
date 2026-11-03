#pragma once
// Active augments: three skill slots (Skill1-3 keys), their cooldowns, and
// the objects they leave on the field (walls, traps, turrets, the black
// hole...). The cards are in Augment.h / AugmentCatalog.cpp; shared damage
// helpers in AugmentCombat.h.
#include "Augment.h"

class Bullet;

namespace active {

constexpr int kSlots = 3;
struct Slot {
    int   aug = -1;                   // ALL_AUGS index, -1 = empty
    float cd = 0.0f;                  // seconds left
    float cdMax = 0.0f;               // the cooldown it started with (HUD)
    int   order = 0;                  // equip order, oldest is replaced first
};

void Reset();                         // a new run
const Slot& GetSlot(int i);
bool SlotSealed(int i);
int  EquippedCount();
bool CanOffer(int augIdx);            // not equipped already
void Acquire(int augIdx);             // equip (replaces the oldest when full)
void SealSlot();                      // Slot Sacrifice
float CooldownFor(int augIdx);        // after the passive modifiers
int  DayNightUses();                  // Day-Night Boundary: 0-2 day, 3-5 night

struct StepInput {
    float dt = 0.0f;
    float px = 0.0f, py = 0.0f;
    float aimX = 0.0f, aimY = 0.0f;
    bool  moving = false;
    bool  keys[kSlots] = {};          // skill keys held
    bool  autoUse = false;            // settings: use skills when ready
    float viewL = 0.0f, viewT = 0.0f, viewR = 0.0f, viewB = 0.0f;
    float viewZoom = 1.0f;
    float screenW = 1920.0f;
};
void Step(const StepInput& in);       // once per fixed step while playing
void ResetInputEdges();               // key edges after focus changes
void DrawWorld(float time);           // field objects, in world space

// Where a movement skill moved the player this step (main moves the window).
bool TakeTeleport(float& x, float& y);

float DamageMult();                   // active skill damage multiplier
void ReduceAllCooldowns(float seconds);
void ResetLongestCooldown();

// Events from the passive layer and the main loop.
void RecordDamage(float dmg);         // Orbital Return
void OnKill();                        // Nova Burst's wave kill count
void OnWaveCleared();
void OnDash();                        // Eclipse Combo window
void OnPlayerBullet(const Bullet& shot);   // Twin Star Clone copies the gun
void OnSkillBulletHit(const Bullet& bullet, float x, float y, bool killed);
void StepSkillBullets();              // Meteor Lance erasing enemy bullets

}  // namespace active
