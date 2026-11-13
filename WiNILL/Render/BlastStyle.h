#pragma once
// Which family a range blast belongs to (StrikeFx.h draws each its own way).
// Kept apart from StrikeFx.h so combat headers can name it without pulling
// in the drawing code.
//
//   Impact     fire, bombs, plain force: a jagged ring, rock chips flying out
//   Gravity    the abyss: pulls in first, then two thin rings and a swirl
//   Discharge  lightning: forked bolts branching out, a faint edge
//   Stellar    light and stars: an eight-point star outline opening out
//   Crystal    crystals and mines: a hexagon breaking apart edge by edge
namespace strike_fx {

enum class BlastStyle : unsigned char { Impact, Gravity, Discharge, Stellar, Crystal };

}  // namespace strike_fx
