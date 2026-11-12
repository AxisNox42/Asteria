#pragma once
// What the synergy cards do during a run (Docs/Design/Augment_Synergy_Redesign.md).
// The cards' names, tags and numbers are the table in AugmentSynergy.h; the game
// registers them as AugType::SYN_* (SynergyCodes.inc, generated). This module is the
// only place that knows how a synergy card changes the game. PassiveAugments.cpp and
// the shared combat code call into it at a few points; nothing else touches it.
#include <vector>

#include "Augment.h"
#include "PassiveAugments.h"

class Bullet;

namespace syn {

void Reset();                          // a new run
void DrawWorld();                      // zones, mines, marks, burns and ring pulses (world space)
void ForgetEnemy(const void* enemy);   // removed without dying (a debug dummy): no kill event
void PickupFx(int augIdx);             // a ring, sparks and the name when any card is taken
bool CanOffer(int augIdx);             // may this synergy card appear in a pick?
void Acquire(int augIdx);              // a synergy card was taken
// The small line under a card's name: its role, the tags it gives and the tags it needs (lang 0 KR, 1 EN, 2 JP).
std::wstring MetaLine(int augIdx, int lang);

// Fire path (main.cpp).
float FireIntervalMult();              // the gun's interval is divided by this
// Turns the gun's one bullet into this shot's volley (frames, twin, spray, nth-shot ...).
// `base` is the bullet the gun made, aimed at (tx, ty). Always returns at least one bullet.
void  Shape(const Bullet& base, float px, float py, float tx, float ty, std::vector<Bullet>& out);
float DamageMult(const Bullet& b, const void* target);    // on top of the passive multipliers
float DamageTakenMult();               // g_PlayerDmgMult

// Once per fixed step.
void  Step(const passive::StepInput& in);

// Events from the shared combat code.
void  OnBulletHit(const Bullet& b, const void* target, float x, float y, bool crit, bool killed);
bool  Redirect(Bullet& b, const void* hitTarget);         // ricochet: true = the shot flies on
void  OnBounce(Bullet& b, bool wall);                     // a shot turned back at the arena edge
void  OnHurt(float dmgTaken);
void  OnDashStarted();
void  OnWaveCleared();
bool  TryRevive();                     // lethal hit: true when a card saves you

}  // namespace syn
