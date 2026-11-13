#pragma once
// The game's distance unit: one "ring" is the radius of the blue health ring
// around the ship (Render/VitalsHud.h), seen from the READY practice field on.
// Card texts give ranges in rings ("주변 4○ 안"), and augment radii are whole
// or half rings so a text never needs more than one decimal.
inline constexpr float kRangeUnit = 80.0f;

// n rings in world units (n may be a half: Rings(2.5f) = 200).
inline constexpr float Rings(float n) { return n * kRangeUnit; }
