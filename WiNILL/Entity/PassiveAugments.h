#pragma once
// Passive augments: what each owned card does during a run. The cards
// themselves (names, rarity, text) are in Augment.h / AugmentCatalog.cpp.
// main.cpp calls the event functions below; the shared combat headers report
// through PassiveHooks.h (InstallHooks wires them).
#include "Augment.h"
#include <vector>

class Bullet;

namespace passive {

// Visual effects owned by main.cpp.
struct Fx {
    void (*shockwave)(float x, float y, float radius, float life,
                      float r, float g, float b) = nullptr;
};
inline Fx g_Fx;

void InstallHooks();
void Reset();                         // a new run

int  Owned(AugType type);
bool CanOffer(int augIdx);            // may this card appear in a pick?
int  Luck();
float LuckChanceMult();               // chance multiplier from luck
void Acquire(int augIdx);             // a card was taken

// Cards taken this run, not the level-up stat cards: each card once, in the
// order first taken, and how many times it was taken (run report).
const std::vector<int>& AcquiredOrder();
int AcquiredCount(int augIdx);

struct StepInput {
    float dt = 0.0f;
    float px = 0.0f, py = 0.0f;       // player centre (world)
    float aimX = 0.0f, aimY = 0.0f;   // cursor (world)
    bool  moving = false;             // any move input held
    bool  dashActive = false;
    bool  damageProtected = false;    // hits this step are undone
    float viewL = 0.0f, viewT = 0.0f, viewR = 0.0f, viewB = 0.0f;  // world
    float viewZoom = 1.0f;
    float screenW = 1920.0f;
};
void Step(const StepInput& in);       // once per fixed step while playing

float FireInterval(float base);       // seconds between shots
void  ModifyShot(Bullet& bullet);     // each bullet the gun fires
void  OnShot();                       // one shot (trigger pull) fired
bool  DashSealed();
float PlateShieldLeft();              // Buffer Plating damage still blockable (0 if not owned)
void  OnDashStarted();                // right after BeginPlayerDash
void  OnWaveCleared();
bool  TryPhoenix();                   // lethal hit: true when it revives

}  // namespace passive
