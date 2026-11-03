#pragma once
// Hooks the passive augments (PassiveAugments.cpp) install, so the shared
// combat headers (CollisionSystem.h, PlayerStats.h) can report events without
// depending on the game layer. They stay null in unit tests; every call site
// checks before calling.

class Bullet;
class Monster;
class RangedMob;

namespace passive_hooks {

// Damage multiplier for one player-bullet hit (crit included); sets crit.
inline float (*bulletDamageMult)(const Bullet& bullet, const void* target,
                                 float targetX, float targetY, bool& crit) = nullptr;
// A player bullet hit a target for dealt damage (after it was applied);
// killed: the hit took the target's last HP.
inline void (*bulletHit)(const Bullet& bullet, const void* target,
                         float x, float y, float dealt, bool crit, bool killed) = nullptr;
// True when this still-alive regular target should die at once.
inline bool (*executes)(const void* target, float hp, bool regular) = nullptr;
// A paying kill was credited.
inline void (*kill)() = nullptr;
// Applies a hit of dmg to hp (after shield); replaces the plain hp -= dmg.
inline void (*hurt)(float& hp, float dmg) = nullptr;

}  // namespace passive_hooks
