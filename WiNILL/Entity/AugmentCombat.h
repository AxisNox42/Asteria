#pragma once
// Combat helpers shared by the passive (PassiveAugments.cpp) and active
// (ActiveSkills.cpp) augments. Damage is given as a percentage of attack
// (공격력 N%), and every helper applies the "모든 피해" multipliers; `extra`
// is for multipliers of one kind only (active skill damage).
#include "Augment.h"

namespace passive {

bool  Has(AugType type);
float Rand01();
bool  Chance(float p);                // luck-adjusted
float AttackPower();
float AllDamageMult();
bool  OnScreen(float x, float y);
void  Ring(float x, float y, float radius, float r, float g, float b);
void  DamageAll(float pct, float r, float g, float b, bool canCrit = false,
                float extra = 1.0f);
void  DamageRadius(float x, float y, float radius, float pct,
                   float r, float g, float b, float extra = 1.0f);
int   ClearEnemyBullets(float x, float y, float radius);   // radius < 0: all
void  AddStardust(long long n);
void  ResetDash();
void  ChanceTriggered();              // Crown of Storms follow-up
void  CountHit();                     // one hit for the hit counters

bool  RubyIntact();                   // Glass Star owned and not broken
bool  FirescaleOn();                  // not switched off by a dash this wave
bool  DarkWingsOn();                  // not switched off by a repeat this wave
void  SwitchOffDarkWings();

}  // namespace passive
