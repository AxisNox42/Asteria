#pragma once
// Combat helpers shared by the passive (PassiveAugments.cpp) and active
// (ActiveSkills.cpp) augments. Damage is given as a percentage of attack
// (공격력 N%), and every helper applies the "모든 피해" multipliers; `extra`
// is for multipliers of one kind only (active skill damage).
#include "Augment.h"
#include "BlastStyle.h"

namespace passive {

// How a screen-wide hit shows on each enemy it reaches (Render/StrikeFx.h).
enum class ScreenFx { Sparks, Zap, BlackBolt, CritChain, StarFall, Nova, Scan };

bool  Has(AugType type);
float Rand01();
bool  Chance(float p);                // luck-adjusted
float AttackPower();
float AllDamageMult();
bool  OnScreen(float x, float y);
void  DamageAll(float pct, float r, float g, float b, bool canCrit = false,
                float extra = 1.0f, ScreenFx fx = ScreenFx::Sparks);
void  DamageRadius(float x, float y, float radius, float pct,
                   float r, float g, float b, float extra = 1.0f,
                   strike_fx::BlastStyle style = strike_fx::BlastStyle::Impact);
// radius < 0: all. show: an erase ring out to the radius (radius >= 100) or a nova
// for a full clear; off for per-shot clears and for callers that draw their own.
int   ClearEnemyBullets(float x, float y, float radius, bool show = true);
void  AddStardust(long long n);
void  ResetDash();
void  ChanceTriggered();              // Crown of Storms follow-up
void  CountHit();                     // one hit for the hit counters

bool  RubyIntact();                   // Glass Star owned and not broken
bool  FirescaleOn();                  // not switched off by a dash this wave
bool  DarkWingsOn();                  // not switched off by a repeat this wave
void  SwitchOffDarkWings();

}  // namespace passive
