#pragma once
// Player stat builds of the run paths, as functions of their inputs (weapon,
// display scale; meta levels via ApplyMeta and trial state via
// TrialPlayerMaxHpMult).

#include "PlayerStats.h"
#include "Weapons.h"
#include "Meta.h"
#include "Settings.h"

// Trial max-HP cut, applied after the weapon.
inline void ApplyTrialMaxHp(PlayerStats& s) {
    float trialHpMul = TrialPlayerMaxHpMult();
    if (trialHpMul < 0.999f)
        s.maxHP *= trialHpMul;
}

// New run, step 1 (ResetForNewGame): no weapon chosen yet.
inline PlayerStats ResetRunStats(float scale) {
    PlayerStats s;
    ApplyMeta(s);
    s.windowSize *= scale;
    return s;
}

// PLAY screen preview: base stats with the weapon, before the trial cut.
inline PlayerStats PreviewBaseStats(int weapon, float scale) {
    PlayerStats base;
    ApplyMeta(base);
    base.windowSize *= scale;
    if (weapon >= 0 && weapon < (int)StartWeapon::_COUNT)
        ApplyWeapon(base, (StartWeapon)weapon);
    base.baseFireInterval = base.fireInterval;
    return base;
}

// New run, step 2 (FinalizeLoadout): the chosen weapon and the trial cut.
inline void ApplyLoadoutStats(PlayerStats& s, int weapon) {
    ApplyWeapon(s, (StartWeapon)weapon);
    s.baseFireInterval = s.fireInterval;
    ApplyTrialMaxHp(s);
}
