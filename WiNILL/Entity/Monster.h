#pragma once

#include "NormaBoss.h"
#include "CaelumBoss.h"
#include "LynxMob.h"
#include "MiraMob.h"
#include "TriangulumMob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <glm/glm.hpp>

#include "Bullet.h"
#include "PlayerStats.h"
#include "GravisField.h"
#include "EnemyShield.h"
#include "Experience.h"
#include "BalanceTuning.h"
#include "Settings.h"   // g_Difficulty: a Genesis picks its summon style from it when it is made

class RangedMob;
class Monster;

// Magnetar's ward lattice: a regular icosahedron (12 vertices, 20 faces).
// Shared by the gameplay (which face a shot meets) and the drawing.
namespace MagnetarLattice {
inline constexpr float kPhi = 1.6180340f;
inline constexpr float kV[12][3] = {
    {-1, kPhi, 0}, {1, kPhi, 0}, {-1, -kPhi, 0}, {1, -kPhi, 0},
    {0, -1, kPhi}, {0, 1, kPhi}, {0, -1, -kPhi}, {0, 1, -kPhi},
    {kPhi, 0, -1}, {kPhi, 0, 1}, {-kPhi, 0, -1}, {-kPhi, 0, 1}};
inline constexpr int kF[20][3] = {
    {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
    {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8},
    {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};

struct Proj { float x, y, z; };

// Rotates a point by yaw (about the vertical) then pitch (about the
// horizontal) and projects it orthographically: x, y on screen, z = depth
// (positive faces the viewer).
inline Proj Project(float px, float py, float pz, float yaw, float pitch) {
    const float cy = cosf(yaw), sy = sinf(yaw);
    const float x1 = px * cy + pz * sy, z1 = -px * sy + pz * cy;
    const float cp = cosf(pitch), sp = sinf(pitch);
    return { x1, py * cp - z1 * sp, py * sp + z1 * cp };
}

// The face a shot meets when it crosses the ward rim at screen angle `ang`:
// the one whose turned normal points most toward that side of the rim,
// leaning to the viewer so it is a visible face drawn on that side.
inline int FaceToward(float ang, float yaw, float pitch) {
    const float tx = cosf(ang) * 0.857f, ty = sinf(ang) * 0.857f, tz = 0.514f;
    int best = 0;
    float bestDot = -1e9f;
    for (int f = 0; f < 20; ++f) {
        const float nx = kV[kF[f][0]][0] + kV[kF[f][1]][0] + kV[kF[f][2]][0];
        const float ny = kV[kF[f][0]][1] + kV[kF[f][1]][1] + kV[kF[f][2]][1];
        const float nz = kV[kF[f][0]][2] + kV[kF[f][1]][2] + kV[kF[f][2]][2];
        const Proj n = Project(nx, ny, nz, yaw, pitch);
        const float d = n.x * tx + n.y * ty + n.z * tz;
        if (d > bestDot) { bestDot = d; best = f; }
    }
    return best;
}
}  // namespace MagnetarLattice

// The close-range roster contains six live signals. Scope remains the
// separate ranged mob; retired mutation variants have no runtime type anymore.
enum class MobKind {
    ROTOR,
    GENESIS,
    SWARM,
    GRAVIS,
    QUASAR,
    GIMBAL,
    REGULUS,
    ANTARES,
    MAGNETAR,
    NORMA,      // window boss (NormaBoss.h); keep last so XP slots stay put
    CAELUM,     // wallpaper boss (CaelumBoss.h): the mid-run boss, after NORMA
    LYNX,     // regular sniper (LynxMob.h)
    MIRA,     // regular corruption walker (MiraMob.h)
    TRIANGULUM, // regular passive carrier: three Swarms burst out when it dies (TriangulumMob.h)
};

// Short names for the balance log (System/GameLog.h).
inline const char* MobKindName(MobKind kind) {
    switch (kind) {
    case MobKind::ROTOR:    return "ROTOR";
    case MobKind::GENESIS:  return "GENESIS";
    case MobKind::SWARM:    return "SWARM";
    case MobKind::GRAVIS:   return "GRAVIS";
    case MobKind::QUASAR:   return "QUASAR";
    case MobKind::GIMBAL:   return "GIMBAL";
    case MobKind::REGULUS:  return "REGULUS";
    case MobKind::ANTARES:  return "ANTARES";
    case MobKind::MAGNETAR: return "MAGNETAR";
    case MobKind::NORMA:    return "NORMA";
    case MobKind::CAELUM:   return "CAELUM";
    case MobKind::LYNX:   return "LYNX";
    case MobKind::MIRA:   return "MIRA";
    case MobKind::TRIANGULUM: return "TRIANGULUM";
    }
    return "?";
}

// The run-deciding bosses: no stun, no crowding, no regular-mob buffs.
inline constexpr bool IsBossMob(MobKind kind) {
    return kind == MobKind::NORMA || kind == MobKind::CAELUM;
}

// Elite mobs carry a second ability and an elite frame.
inline constexpr bool IsEliteMob(MobKind kind) {
    return kind == MobKind::REGULUS || kind == MobKind::GENESIS ||
           kind == MobKind::ANTARES ||
           kind == MobKind::QUASAR;
}

inline constexpr float ANTARES_STUN_TIME_SEC = 0.6f;

inline void MobKillReward(MobKind kind, float& xpBase, float& scoreBase) {
    xpBase = 1.0f;
    scoreBase = 100.0f;
    switch (kind) {
    case MobKind::ROTOR:   xpBase = 2.0f;  scoreBase = 100.0f; break;
    case MobKind::GIMBAL:  xpBase = 4.0f;  scoreBase = 220.0f; break;
    case MobKind::GENESIS: xpBase = 15.0f; scoreBase = 450.0f; break;
    case MobKind::SWARM:   xpBase = 1.0f;  scoreBase = 12.0f;  break;
    case MobKind::GRAVIS:  xpBase = 16.0f; scoreBase = 520.0f; break;
    case MobKind::QUASAR:  xpBase = 12.0f; scoreBase = 480.0f; break;
    case MobKind::REGULUS: xpBase = 6.0f;  scoreBase = 260.0f; break;
    case MobKind::ANTARES: xpBase = 8.0f;  scoreBase = 350.0f; break;
    case MobKind::MAGNETAR: xpBase = 10.0f; scoreBase = 520.0f; break;
    case MobKind::LYNX:  xpBase = 5.0f;  scoreBase = 240.0f; break;
    case MobKind::MIRA:  xpBase = 4.0f;  scoreBase = 200.0f; break;
    case MobKind::TRIANGULUM: xpBase = 6.0f; scoreBase = 240.0f; break;
    case MobKind::CAELUM:  xpBase = 40.0f; scoreBase = 2500.0f; break;   // the mid-run boss
    default: break;
    }
}

inline int StardustRewardFor(MobKind kind) {
    switch (kind) {
    case MobKind::ROTOR:  return 1;
    case MobKind::SWARM:    return 1;
    case MobKind::GENESIS: return 20;
    case MobKind::GRAVIS:  return 15;
    case MobKind::QUASAR:  return 7;
    case MobKind::GIMBAL:  return 2;
    case MobKind::REGULUS: return 3;
    case MobKind::ANTARES: return 4;
    case MobKind::MAGNETAR: return 6;
    case MobKind::LYNX:  return 3;
    case MobKind::MIRA:  return 2;
    case MobKind::TRIANGULUM: return 3;
    case MobKind::CAELUM:  return 30;
    default:               return 0;
    }
}

inline float MobRewardMult(MobKind kind) {
    return kind == MobKind::SWARM ? 0.20f : 1.0f;
}

inline XpAmount MobExperienceReward(MobKind kind, const PlayerStats& stats) {
    float xpBase, scoreBase;
    MobKillReward(kind, xpBase, scoreBase);
    // Currency/score reductions must not turn a tier-1 kill into zero XP.
    return XpFromReward((double)xpBase * stats.xpMult *
                        BalanceFor((int)g_Difficulty).xpMult);
}

class Monster {
public:
    static constexpr float GIMBAL_CHARGE_ROTATIONS = 2.0f;
    // Elite HP (before the run's HP scaling). Boss rush: every wave's elite
    // must fall to move on, and a fast kill earns a better reward, so a wave-1
    // elite is about 3,000 effective HP (~12 s of starting fire, 20-25 s in
    // play) and grows with the per-elite-wave HP ramp (WaveHpRamp). The kinds
    // are matched on effective HP: Regulus counts its own 90% shield, Antares
    // its split fragments, Genesis sits a little lower for its summons.
    static constexpr float QUASAR_BASE_HP = 3200.0f;
    static constexpr float GENESIS_BASE_HP = 2800.0f;
    static constexpr float REGULUS_BASE_HP = 1650.0f;
    static constexpr float GRAVIS_BASE_HP = 1400.0f;
    // Regulus fires an instant shield laser at a random eligible ally in
    // range; the shield is granted at once and nothing stays connected.
    static constexpr float REGULUS_SHOT_RANGE = 750.0f;
    static constexpr float REGULUS_SHOT_INTERVAL = 3.0f;   // one volley
    static constexpr int REGULUS_VOLLEY_COUNT = 3;         // distinct allies per volley
    static constexpr float REGULUS_BEAM_TIME = 0.25f;   // laser afterglow
    static constexpr float REGULUS_SHIELD_REGEN_DELAY = 8.0f;
    // Regulus elite is a support mob: it only drifts and fires buff lasers.
    // Each shot picks random allies that carry no buff yet and gives each one
    // random buff; buffs are permanent and a mob only ever gets one (a broken
    // shield does not free it for another). Every buff has its own laser /
    // marker colour (see RegulusBuffColor).
    // The cloak only dims the drawing; bodies stay solid.
    static constexpr int REGULUS_BUFF_SHIELD = 0;
    static constexpr int REGULUS_BUFF_SPEED = 1;
    static constexpr int REGULUS_BUFF_STEALTH = 2;
    static constexpr float REGULUS_SPEED_BUFF_MULT = 1.55f;
    static constexpr float REGULUS_STEALTH_ALPHA = 0.36f;
    static constexpr float REGULUS_BUFF_FADE = 0.4f;     // buff fades in
    // Genesis and the enemies it controls share one colour family (magenta), clearly
    // apart from the red/orange of free Swarms and Rotors. A controlled enemy uses it
    // until its Genesis dies; then it falls back to its own colour and plain AI.
    static inline const glm::vec3 LYNX_COLOR = glm::vec3(0.39f, 0.90f, 0.77f);
    static inline const glm::vec3 TRIANGULUM_COLOR = glm::vec3(0.74f, 0.10f, 0.52f);    // Genesis's body colour: a lesser Genesis
    static inline const glm::vec3 MIRA_COLOR = glm::vec3(1.0f, 0.59f, 0.38f);           // the core; the zone is ERIDANUS amber
    static inline const glm::vec3 LYNX_HOT =glm::vec3(1.0f, 0.58f, 0.39f);            // the aim line and the ray
    static inline const glm::vec3 GENESIS_COLOR = glm::vec3(0.74f, 0.10f, 0.52f);        // body
    static inline const glm::vec3 GENESIS_PRODUCTION = glm::vec3(0.98f, 0.28f, 0.74f);   // shells, glow
    static inline const glm::vec3 GENESIS_HEAT = glm::vec3(1.0f, 0.72f, 0.92f);          // heated edge
    static inline const glm::vec3 GENESIS_CONTROLLED_COLOR = glm::vec3(1.0f, 0.34f, 0.78f);
    // Controlled-enemy AI: it leads the player (aims at where they will be) and
    // sidesteps incoming player shots, in short bursts so it can still be hit.
    static constexpr float GENESIS_CTRL_LEAD_MAX = 0.7f;          // furthest it looks ahead, seconds
    // A pure lead is dumb against a player who jukes back and forth, so the lead is only
    // trusted while the player keeps one heading: a sharp reversal drops the confidence to
    // zero and it comes back slowly. While it is low the enemy closes on the CENTRE of the
    // player's recent movement instead of their current spot, and the lead can never be
    // more than GENESIS_CTRL_LEAD_FRAC of the distance ahead of the player.
    static constexpr float GENESIS_CTRL_REVERSAL_COS = 0.2f;      // heading change sharper than this = a reversal
    static constexpr float GENESIS_CTRL_STEADY_COS = 0.85f;       // heading this close to before = steady
    static constexpr float GENESIS_CTRL_REVERSAL_GAP = 0.15f;     // reversals closer than this count once
    static constexpr float GENESIS_CTRL_HEAT_PER_REVERSAL = 1.0f; // "juke heat" each reversal adds
    static constexpr float GENESIS_CTRL_HEAT_COOL = 0.55f;        // heat lost per second of steady movement
    static constexpr float GENESIS_CTRL_HEAT_FOR_ZERO = 1.2f;     // this much heat = no trust in the lead at all
    static constexpr float GENESIS_CTRL_MOVING_SPEED = 80.0f;     // slower than this the player counts as standing
    static constexpr float GENESIS_CTRL_CENTRE_TAU = 0.5f;        // seconds: memory of the player's movement centre
    static constexpr float GENESIS_CTRL_CENTRE_BLEND = 0.5f;      // how far toward that centre it leans at zero confidence
    static constexpr float GENESIS_CTRL_LEAD_FRAC = 0.65f;
    static constexpr float GENESIS_CTRL_LEAD_MIN_DIST = 70.0f;    // closer than this it just closes in
    static constexpr float GENESIS_CTRL_VEL_MAX = 900.0f;         // cap on the player speed it believes
    // Quasar beam: when the cooldown ends the axis snaps straight onto the
    // player and locks; the beam fires QUASAR_LOCK_TIME later on that fixed
    // angle (no sweep), so the lock window is the dodge window.
    static constexpr float QUASAR_LOCK_TIME = 0.55f;    // elite-wave pass: was 0.7
    static constexpr float QUASAR_BEAM_TIME = 3.0f;     // was 2.5
    // The beam does not cut out: over its last QUASAR_BEAM_FADE_OUT seconds it
    // thins to nothing (what you see is what hurts, so the hit width follows).
    static constexpr float QUASAR_BEAM_FADE_OUT = 0.45f;
    static constexpr float QUASAR_BEAM_GROW = 0.70f;    // time for the beam to reach full length
    // The aim is frozen until the beam has reached full length; only then does the
    // slow turn toward the player ease in over QUASAR_BEAM_TURN_EASE.
    static constexpr float QUASAR_BEAM_TURN_EASE = 0.45f;
    // Fire-onset rift (drawing only): at the instant of firing a seam runs out
    // from the Quasar along the axis (QUASAR_BEAM_RIFT_PROP to cover the full length) and
    // the band opens behind its tip, so the picture tears open from the Quasar
    // outward; the whole opening is done by QUASAR_BEAM_RIFT_TIME. The lock's
    // last QUASAR_BEAM_RIFT_LEAD seconds already show a short seam at the Quasar.
    static constexpr float QUASAR_BEAM_RIFT_PROP = 0.32f;
    static constexpr float QUASAR_BEAM_RIFT_OPEN = 0.18f;      // a point of the band opening
    static constexpr float QUASAR_BEAM_RIFT_TIME = QUASAR_BEAM_RIFT_PROP + QUASAR_BEAM_RIFT_OPEN;
    static constexpr float QUASAR_BEAM_RIFT_LEAD = 0.25f;
    // The last QUASAR_PRECHARGE_TIME of the cooldown is the wind-up: the
    // Quasar stops drifting and its bands fold and draw energy in, before the
    // direction is chosen.
    static constexpr float QUASAR_COOLDOWN_TIME = 2.7f;   // was 3.2
    static constexpr float QUASAR_PRECHARGE_TIME = 1.0f;
    // Death: particles only, burst at the kill (no body animation).
    // Blast debris life and reach (in base units; halo is 1.95).
    static constexpr float QUASAR_NOVA_WAVE_TIME = 0.60f;
    static constexpr float QUASAR_NOVA_REACH = 3.64f;
    // Genesis death debris: GENESIS_WAVE_COUNT bursts, each reaching
    // GENESIS_WAVE_FALLOFF of the one before.
    static constexpr int GENESIS_WAVE_COUNT = 1;
    static constexpr float GENESIS_WAVE_REACH = 4.76f;    // wave radius / base (plates reach ~2.45)
    static constexpr float GENESIS_WAVE_FALLOFF = 0.75f;  // each next wave's share of the last
    static constexpr float QUASAR_BEAM_RADIUS = 55.0f;
    static constexpr float QUASAR_BEAM_DPS = 45.0f;
    // While firing, the beam turns very slowly (at most this rate, rad/s)
    // toward the player. The beam runs both ways, so it turns whichever way
    // brings the nearer end onto the player.
    static constexpr float QUASAR_BEAM_TURN = 0.14f;   // was 0.08
    // Beam reflection: the beam bounces off the arena edge this many times
    // (none below HARD, 1 on HARD, 2 on NIGHTMARE). d = Difficulty enum value.
    static constexpr int QuasarReflectionsFor(int d) { return d >= 3 ? 2 : (d == 2 ? 1 : 0); }
    static constexpr int QUASAR_BEAM_MAX_SEGMENTS = 6;   // 2 ends x (2 bounces + 1)
    static constexpr float QUASAR_BEAM_LENGTH = 6000.0f;   // each end, at least (see QuasarBeamLength)
    // Starfall: every QUASAR_PREDICT_EVERY-th shot is aimed ahead of the player
    // along their direction of travel instead of at a random spot near them.
    // The lead is a vector: direction = normalised velocity, distance =
    // speed x QUASAR_PREDICT_LEAD_TIME, capped at QUASAR_PREDICT_MAX_LEAD so
    // the shot lands just in front of the player and never far off their path.
    static constexpr int QUASAR_PREDICT_EVERY = 3;
    static constexpr float QUASAR_PREDICT_MAX_SPEED = 900.0f;   // velocity estimate cap
    static constexpr float QUASAR_PREDICT_MIN_SPEED = 40.0f;    // slower = standing still
    static constexpr float QUASAR_PREDICT_LEAD_TIME = 0.5f;
    static constexpr float QUASAR_PREDICT_MAX_LEAD = 140.0f;
    static constexpr int QUASAR_FALL_COUNT = 18;
    // Eruption: the Quasar lays a field of danger zones over the arena in
    // concentric rings around itself (a guard ring close in, more rings out to
    // the arena edge), each ring with two doors. Everything is shown at once,
    // and QUASAR_ERUPT_BLAST_TIME after the start every zone goes off together.
    // It is staged so it never springs on the player: the body gathers for
    // QUASAR_ERUPT_WINDUP with no zones, then the rings unfold one after the
    // other from the inside out, and each ring stays up at least
    // QUASAR_ERUPT_MIN_WARNING before the blast. The bands between rings (and
    // the pocket next to the body) are the safe places.
    static constexpr float QUASAR_ERUPT_WINDUP = 0.6f;
    static constexpr float QUASAR_ERUPT_RING_GAP = 0.30f;      // between one ring and the next
    static constexpr float QUASAR_ERUPT_RING_GROW = 0.35f;     // a ring unfolding
    static constexpr float QUASAR_ERUPT_MIN_WARNING = 1.0f;    // every zone is visible this long
    static constexpr float QUASAR_ERUPT_BLAST_TIME = 2.2f;
    // The eruption is a condition, not part of the rotation, and it is an overlay:
    // it runs on its own timer (quasarEruptTimer) next to whatever the Quasar is
    // doing (wind-up, lock, beam, starfall) instead of waiting for it. It goes off
    // when the player has stayed within QUASAR_ERUPT_TRIGGER_RADIUS of the Quasar for
    // QUASAR_ERUPT_TRIGGER_TIME (the timer drains at QUASAR_ERUPT_NEAR_DRAIN x
    // real time while the player is away), and only covers the ground around the
    // Quasar (rings up to QUASAR_ERUPT_MAX_REACH). After it, QUASAR_ERUPT_COOLDOWN.
    static constexpr float QUASAR_ERUPT_TRIGGER_RADIUS = 440.0f;
    static constexpr float QUASAR_ERUPT_TRIGGER_TIME = 1.5f;
    static constexpr float QUASAR_ERUPT_NEAR_DRAIN = 1.0f;
    static constexpr float QUASAR_ERUPT_MAX_REACH = 700.0f;
    static constexpr float QUASAR_ERUPT_COOLDOWN = 5.0f;
    static constexpr float QUASAR_ERUPT_ZONE_RADIUS = 49.0f;   // was 70 (x0.7)
    static constexpr float QUASAR_ERUPT_FIRST_RING = 190.0f;
    static constexpr float QUASAR_ERUPT_RING_STEP = 230.0f;   // lane = step - 2 * zone radius
    static constexpr float QUASAR_ERUPT_DAMAGE = 16.0f;       // per beat (two beats per eruption)
    // Two beats: beat 0 detonates the rings at QUASAR_ERUPT_BLAST_TIME; beat 1
    // (the lane between the rings and a zone on the body itself) detonates
    // QUASAR_ERUPT_BEAT_GAP later, so the safe spot of the first beat is the
    // danger of the second. Beat 1's zones are up from QUASAR_ERUPT_SECOND_SHOW.
    static constexpr float QUASAR_ERUPT_BEAT_GAP = 0.5f;
    static constexpr float QUASAR_ERUPT_SECOND_SHOW = 1.6f;
    static constexpr float QUASAR_ERUPT_SECOND_BLAST = QUASAR_ERUPT_BLAST_TIME + QUASAR_ERUPT_BEAT_GAP;
    static constexpr float QUASAR_ERUPT_CENTER_RADIUS = 90.0f;
    static constexpr unsigned char QUASAR_ERUPT_CENTER_RING = 255;   // ring id of the body zone
    static constexpr int QUASAR_ERUPT_MAX_ZONES = 200;
    static constexpr int QUASAR_FALL_STATE = 5;
    static constexpr int QUASAR_FALL_RECOVER_STATE = 6;
    static constexpr float QUASAR_FALL_SPLIT = 1.1f;
    static constexpr float QUASAR_FALL_WARNING = 0.75f;   // the same for every shot, near or far
    // Time between starfall shots: a random 0.1-0.3 s each, never stretched.
    static constexpr float QUASAR_FALL_GAP_MIN = 0.10f;
    static constexpr float QUASAR_FALL_GAP_MAX = 0.30f;
    // Starfall power grows with the distance between the Quasar and the
    // landing point: at or below QUASAR_FALL_NEAR_DIST a shot is the base size
    // and damage, at QUASAR_FALL_FAR_DIST and beyond it is the maximum.
    static constexpr float QUASAR_FALL_RADIUS = 64.0f;     // base (near) radius
    static constexpr float QUASAR_FALL_DAMAGE = 6.0f;      // base (near) damage
    static constexpr float QUASAR_FALL_NEAR_DIST = 100.0f;
    static constexpr float QUASAR_FALL_FAR_DIST = 1100.0f;   // wide span: sizes grade smoothly in between
    static constexpr float QUASAR_FALL_MAX_RADIUS_SCALE = 1.7f;
    static constexpr float QUASAR_FALL_MAX_DAMAGE_SCALE = 2.2f;
    static constexpr float QUASAR_FALL_RECOVER = 0.8f;
    static constexpr float QUASAR_FALL_IMPACT_FX = 0.36f;   // impact effect length
    static constexpr float QUASAR_FALL_HIT_GRACE = 0.35f;
    // x ANTARES_ELITE_HP_MULT for the first body; with both split generations
    // the whole elite is ~3.2x this (see elite HP note above).
    static constexpr float ANTARES_BASE_HP = 1000.0f;
    static constexpr int ANTARES_TRAIL_COUNT = 6;
    // Genesis shell core: three shells lock in one by one while tracking
    // (one stack each); at 3/3 it aims 0.8s and throws all three Swarms, then
    // regroups 0.2s with the shells empty. About 6s per volley. Thrown
    // Swarms fly, then stun like Antares.
    static constexpr float GENESIS_TRACK_TIME = 4.6f;
    static constexpr int GENESIS_STACKS = 3;              // shells = Swarms per volley
    static constexpr float GENESIS_STACK_TIME = GENESIS_TRACK_TIME / GENESIS_STACKS;
    static constexpr float GENESIS_AIM_TIME = 0.8f;
    // Throw: each Swarm is fed down its spoke into the core (FEED), then
    // leaves; shots are THROW_GAP apart and the phase ends THROW_END after
    // the last one.
    static constexpr float GENESIS_THROW_GAP = 0.20f;
    static constexpr float GENESIS_FEED_TIME = 0.12f;
    static constexpr float GENESIS_THROW_END = 0.10f;
    static constexpr float GENESIS_RECOVER_TIME = 0.2f;
    static constexpr float GENESIS_ROTOR_WAIT = 0.25f;
    static constexpr float GENESIS_ROTOR_PREP = 1.0f;
    static constexpr float GENESIS_ROTOR_GAP = 0.4f;
    static constexpr float GENESIS_ROTOR_FORM = 0.24f;
    static constexpr float GENESIS_ROTOR_RECOVER = 0.55f;
    static constexpr float GENESIS_ROTOR_SPEED = 2200.0f;
    static constexpr float GENESIS_ROTOR_BLAST_RADIUS = 80.0f;
    static constexpr float GENESIS_ROTOR_BLAST_DAMAGE = 10.0f;
    static constexpr int GENESIS_ROTOR_COUNT = 6;
    static constexpr float GENESIS_TURN_RATE = 0.9f;      // rad/s while tracking
    // Thrown Swarm speed by share of the flight distance: fast launch,
    // braking into the landing (0-60% / 60-80% / 80-100%).
    static constexpr float GENESIS_THROW_SPEED_LAUNCH = 1400.0f;  // px/s
    static constexpr float GENESIS_THROW_SPEED_MID = 1100.0f;
    static constexpr float GENESIS_THROW_SPEED_LAND = 600.0f;
    static constexpr float GENESIS_THROW_SPLIT_MID = 0.60f;
    static constexpr float GENESIS_THROW_SPLIT_LAND = 0.80f;
    // Each Genesis may have at most this many of its own thrown Swarms
    // alive (in flight, stunned or chasing); separate from swarmCap.
    static constexpr int GENESIS_THROWN_CAP = 15;
    // Thrown Rotors have their own per-Genesis cap, independent of Swarms.
    static constexpr int GENESIS_ROTOR_CAP = GENESIS_ROTOR_COUNT;
    // The first Swarm of a volley lands on the player's position when the aim
    // locks; the rest land at random points within this radius of it. There
    // is no range limit.
    static constexpr float GENESIS_SCATTER_RADIUS = 110.0f;
    // A thrown Swarm spins in proportion to its flight speed (rad per px):
    // fast off the muzzle, slowing as it brakes into the landing.
    static constexpr float THROWN_SWARM_SPIN_PER_PX = 0.016f;
    // Landing shockwave around each thrown Swarm.
    static constexpr float THROWN_SWARM_BLAST_RADIUS = 80.0f;
    static constexpr float THROWN_SWARM_BLAST_DAMAGE = 6.0f;
    static constexpr float THROWN_SWARM_DAMAGE = 10.0f;
    static constexpr float THROWN_SWARM_STUN = ANTARES_STUN_TIME_SEC;
    static constexpr float MAGNETAR_BASE_HP = 450.0f;
    // Each of the 20 lattice faces is its own shield: a face stops
    // MAGNETAR_FACE_HITS shots from its side, then breaks and lets shots
    // through there until it restores MAGNETAR_FACE_RESTORE seconds later.
    static constexpr int MAGNETAR_FACES = 20;
    static constexpr int MAGNETAR_FACE_HITS = 4;
    static constexpr float MAGNETAR_FACE_RESTORE = 8.0f;
    static constexpr float MAGNETAR_FIELD_RADIUS = 145.0f;
    // A face that stops a shot flashes white for this long.
    static constexpr float MAGNETAR_HIT_FLASH = 0.20f;

    // Antares is a bull: it keeps accelerating while it chases, turns worse
    // the faster it goes, and drops back to stage 1 once it overshoots.
    // Ramming the player knocks them away and stuns Antares; a finished run
    // (skid) ends in a short rest. Every rush ricochets off the map edge
    // (ConfineAntares), so a missed run never carries it off-map.
    //
    // Elite (generation 0): when killed it splits into smaller Antares
    // (generation 1), which split once more into the smallest (generation 2,
    // ANTARES_MAX_GEN); any two Antares that touch bounce off each other and
    // speed up.
    enum class AntaresState { CHASE, SKID, STUNNED, REST };
    static constexpr float ANTARES_ELITE_HP_MULT = 1.5f;
    static constexpr int ANTARES_SPLIT_COUNT = 2;
    static constexpr int ANTARES_MAX_GEN = 2;
    static constexpr float ANTARES_CHILD_SCALE = 0.80f;   // per generation
    static constexpr float ANTARES_CHILD_HP = 0.45f;      // per generation, of a fresh Antares
    static constexpr float ANTARES_BOUNCE_BOOST = 1.20f;
    static constexpr float ANTARES_BOUNCE_MIN_SPEED = 180.0f;
    static constexpr float ANTARES_BOUNCE_COOLDOWN = 0.25f;
    static constexpr float ANTARES_STUN_TIME = ANTARES_STUN_TIME_SEC;
    static constexpr float ANTARES_REST_TIME = 0.8f;
    static constexpr float ANTARES_MIN_SPEED = 60.0f;
    static constexpr float ANTARES_MAX_SPEED = 900.0f;
    // Only a stage-4 run (500 px/s and up) counts as overshooting the player.
    static constexpr float ANTARES_PASS_SPEED = 500.0f;

    float worldX = 0.0f;
    float worldY = 0.0f;
    float speed = 150.0f;
    float hp = 90.0f;
    bool alive = true;
    bool exploded = false;
    bool scored = false;
    bool noBlast = false;
    bool summoned = false;
    float hitFlashTimer = 0.0f;
    // Brief hit-stun from a player shot: frozen in place, shoved along the shot.
    static constexpr float HIT_STUN_TIME = 0.07f;
    static constexpr float HIT_KNOCK_SPEED = 57.0f;     // ~4px over HIT_STUN_TIME
    // A fast fire rate must not keep a mob pinned: a new stun only lands after the
    // cooldown (counted from the last stun's start), i.e. at most 1/cooldown per second.
    static constexpr float HIT_STUN_COOLDOWN = 0.25f;
    float hitStunTimer = 0.0f;
    float hitStunCooldown = 0.0f;
    float hitKnockX = 0.0f, hitKnockY = 0.0f;
    void TryHitStun(float dirX, float dirY) {
        if (hitStunCooldown > 0.0f || LynxHoldsPosition()) return;
        hitStunTimer = HIT_STUN_TIME;
        hitStunCooldown = HIT_STUN_COOLDOWN;
        hitKnockX = dirX * HIT_KNOCK_SPEED;
        hitKnockY = dirY * HIT_KNOCK_SPEED;
    }
    // Elite hit feel: no stun or shove; the body swells a little (pulse ->
    // swell, which rises fast and follows the pulse down) and, on a critical
    // shot, the side it came from dents in along the shot and the body
    // rattles briefly (drawn only). A hit inside a running pulse
    // only tops it up, so rapid fire keeps it beating instead of pinned big.
    static constexpr float ELITE_HIT_PULSE_TIME = 0.11f;
    static constexpr float ELITE_HIT_RISE_TIME = 0.02f;
    static constexpr float ELITE_HIT_REKICK = 0.35f;
    static constexpr float ELITE_HIT_DENT_TIME = 0.09f;
    static constexpr float ELITE_HIT_SHAKE_TIME = 0.14f;   // crit rattle (drawn only)
    float eliteHitShake = 0.0f;
    float eliteHitPulse = 0.0f;
    float eliteHitSwell = 0.0f;
    float eliteHitDent = 0.0f;
    float eliteHitDirX = 0.0f, eliteHitDirY = 0.0f;
    void EliteHitFeel(float dirX, float dirY) {
        eliteHitPulse = eliteHitPulse > 0.0f
            ? std::min(1.0f, eliteHitPulse + ELITE_HIT_REKICK) : 1.0f;
        if (dirX == 0.0f && dirY == 0.0f) return;
        eliteHitDirX = dirX;
        eliteHitDirY = dirY;
        eliteHitDent = eliteHitDent > 0.0f ? std::min(1.0f, eliteHitDent + 0.5f) : 1.0f;
        eliteHitShake = ELITE_HIT_SHAKE_TIME;
    }
    float deathEffectTimer = -1.0f;
    // Kill reward held on the mob until the kill is processed (Quasar, Genesis).
    bool dustPending = false;
    int pendingDustValue = 0;
    XpAmount pendingDustXp = 0;
    glm::vec3 color = glm::vec3(1.0f, 0.27f, 0.0f);

    MobKind kind = MobKind::ROTOR;
    float sizeScale = 1.0f;
    float contactDmg = 5.0f;
    float singularityGrace = 0.0f;

    // NORMA boss state (NormaBoss.h).
    float normaMaxHp = 0.0f;
    int normaPhase = 1;
    float normaPhaseFlash = 0.0f;   // > 0 right after entering phase 2
    float normaInvuln = 0.0f;       // > 0: holds still, takes no damage (phase change)
    bool normaHidden = false;       // Warp: inside a pane, out of the fight
    float normaHide = 0.0f;         // 0..1 how far it has slipped into the pane (drawing)
    float normaWarpSrcX = 0.0f, normaWarpSrcY = 0.0f;   // the pane it hides in, world
    float normaVisualAngle = 0.0f;
    // Pattern state machine (NormaBoss.h).
    norma::Pattern normaPattern = norma::Pattern::None;
    norma::Stage normaStage = norma::Stage::Recover;
    float normaTimer = 0.0f;        // seconds in the current stage
    int normaCount = 0;             // patterns started so far
    norma::Pattern normaLastPattern = norma::Pattern::None;   // the one before, never repeated back to back
    float normaFireTimer = 0.0f;
    float normaSpin = 0.0f;         // stream and ring angle
    // Written by the game loop when it serves an Ask: the pane the boss uses
    // (> 0), or -1 when none could be had; its centre in world space; and
    // whether that pane has since gone (closed or minimized by the player).
    int normaPaneId = 0;
    float normaRecoil = 0.0f;       // > 0 just after a volley
    int normaWaveIndex = 0;         // Refuge: flood waves fired so far
    float normaAimAngle = 0.0f;     // Aim: where the stream points
    int normaAimShot = 0;
    float normaLastPlayerAngle = 0.0f;
    // The player's velocity, written by the game loop each frame. No boss shot
    // leads the player any more; kept for norma::LeadAngle.
    float normaPlayerVX = 0.0f, normaPlayerVY = 0.0f;
    std::array<float, norma::kGhostCount> normaGhostX{}, normaGhostY{};
    float normaGhostTimer = 0.0f;
    int normaGhostHead = 0;
    float normaGhostLife = 0.0f;    // > 0 while afterimages show
    float normaExposed = 0.0f;      // > 0: holding still, open to extra damage
    // The drawn tesseract's orientation. It turns slowly; while the boss aims
    // or charges it stops turning and rolls the nearest outline corner
    // (normaCubeLead) onto that direction, always the short way round.
    float normaCubeYaw = 0.0f, normaCubePitch = 0.55f, normaCubeRoll = 0.0f;
    int normaCubeLead = 6;
    int normaLitStars = norma::kHealthStars;   // see norma::LitStarsFor
    float normaStarFlash = 0.0f;    // > 0 while the star that just went dark flares
    // 0..1 how far the pane-bracket ring is out: only while the boss handles
    // panes (hiding in one, summoning through them, ramming with one).
    float normaRingShow = 0.0f;
    float normaVelX = 0.0f, normaVelY = 0.0f;   // eased drift velocity, world px/s

    EnemyShieldState regulusShield;
    float regulusBaseHp = 0.0f;
    float regulusShieldRegenTimer = 0.0f;
    float regulusVisualAngle = 0.0f;
    float regulusDriftAngle = 0.0f;
    float regulusDriftTimer = 0.0f;
    float regulusShotTimer = 0.5f;    // time until the next buff volley
    bool regulusHasTarget = true;     // last look found someone to buff (draw only)
    // Permanent buffs a Regulus laser put on this mob (shield is regulusShield).
    // Seconds since the buff landed (drives the fade-in); < 0 = not buffed.
    float regulusSpeedAge = -1.0f;
    float regulusStealthAge = -1.0f;

    // LYNX sniper (LynxMob.h).
    bool lynxInArena = false;           // reached the view; only then it aims
    float lynxHeading = 0.0f;           // where the plates and the ray point
    int lynxSnipe = lynx::SnipeRest;
    float lynxSnipeTimer = 0.0f;
    float lynxHoldX = 0.0f, lynxHoldY = 0.0f;   // where it stands from aim to shot
    float lynxCharge = 0.0f;            // 0..1 how hot the outline is drawn
    float miraTrailDist = 0.0f;         // MIRA: walked since its last trail puddle
    int miraChain = 0;                  // MIRA: the ribbon its trail puddles belong to
    float triangulumSpin = 0.0f;        // TRIANGULUM: angle of its inner triangle (drawn only)

    // Genesis station state.
    float spawnTimer = 0.0f;
    bool anchored = false;
    float spawnAnchorX = 0.0f;
    float spawnAnchorY = 0.0f;
    int hivePhase = 0;
    float hiveOpenFactor = 0.0f;
    bool hiveSpawned = false;
    int hiveSpawnCount = 0;
    float hiveOrbitAngle = 0.0f;
    float hivePulseTimer = 0.0f;
    bool genesisEgress = false;
    float genesisEgressX = 0.0f;
    float genesisEgressY = 0.0f;
    float genesisEgressTimer = 0.0f;
    // Genesis shell core. hiveOrbitAngle is the facing (muzzle direction),
    // hivePhase 0 track+charge / 1 aim / 2 throw / 3 regroup, spawnTimer
    // times the phase and hiveSpawnCount counts throws in this volley.
    // Phases 4/5/6 use the rear-production Rotor cycle below.
    float genesisShellSpin = 0.0f;    // shell ring offset from the facing
    float genesisSpinBase = 0.0f;     // spin at the last lock (muzzle gap on the facing)
    float genesisAimAngle = 0.0f;
    std::array<float, 3> genesisShotX{};   // landing points, rolled at aim lock
    std::array<float, 3> genesisShotY{};
    float genesisRecoil = 0.0f;       // 1 at a throw, decays back to 0
    float genesisChildHpMult = 1.0f;  // spawn-time HP ramp, passed to thrown Swarms
    float antaresHpMult = 1.0f;       // spawn-time HP ramp, passed to split fragments
    int antaresFamily = 0;            // shared by an Antares and all its fragments
    int genesisId = 0;                // owner key for the per-Genesis thrown cap
    // Alternate the existing Swarm volley with rear-produced Rotor launches.
    // Owner phases: 4 prepare, 5 burst, 6 recover. Child: 1 feed, 2 fly, 3 settle.
    bool genesisNextRotor = false;
    bool genesisRotorLoaded = false;
    int genesisRotorPhase = 0;
    bool genesisRotorDetonated = false;
    float genesisRotorFullScale = 1.0f;
    float genesisRotorShotAge = 1.0f;
    glm::vec2 genesisRotorRecoilDir{1.0f, 0.0f};
    inline static int s_NextGenesisId = 1;

    // Thrown Swarm: 1 flight, 2 landing stun, 0 ordinary chase. The Antares
    // trail buffer doubles as this Swarm's afterimage history.
    bool genesisThrown = false;       // summoned Swarm/Rotor; owner cap and no-farm rewards
    int thrownOwnerId = 0;            // genesisId of the Genesis that threw it
    bool genesisControlled = false;   // still driven by its Genesis (smart AI, controlled colour)
    // Nightmare: this Genesis' thrown units are capsules that open into a random non-elite,
    // non-boss enemy when they have landed (genesisRandomSummons is set on the Genesis).
    bool genesisRandomSummons = false;
    bool genesisSummonCarrier = false;    // a capsule in flight / landing, not yet an enemy
    bool genesisSummoned = false;         // a real enemy a Genesis summoned (half rewards, own cap slot)
    float genesisSummonHpMult = 1.0f;     // the Genesis' child HP multiplier, for the enemy it becomes
    bool genesisCarryScope = false;       // this capsule opens into a Scope (a different class), drawn as one in flight
    // The Genesis' own body grows with the difficulty: 1.0x / 1.1x / 1.2x / 1.3x.
    static float GenesisBodySizeScale() { return 1.0f + 0.10f * (float)(int)g_Difficulty; }
    bool genesisFromLauncher = false;     // made by the Rotor launcher (counts against its cap, any kind)
    float genesisNaturalScale = 0.0f;     // a random launcher unit's own size, reached when it lands (0 = none)
    float summonRamp = 1.0f;              // a summoned melee enemy speeds up while it chases, up to SUMMON_RAMP_MAX
    static constexpr float SUMMON_RAMP_RATE = 0.10f;   // speed multiplier gained per second
    static constexpr float SUMMON_RAMP_MAX = 1.8f;     // the limit, so it stays dodgeable
    glm::vec3 freeColor{0.0f};        // its own colour before it was taken over
    glm::vec2 ctrlPlayerPrev{0.0f};
    glm::vec2 ctrlPlayerVel{0.0f};
    bool ctrlPlayerSeen = false;
    float ctrlConfidence = 1.0f;      // 0..1: how far the player's heading can be trusted (from the heat)
    float ctrlJukeHeat = 0.0f;        // grows with every sharp reversal, cools while the player holds a heading
    float ctrlSinceReversal = 9.0f;
    glm::vec2 ctrlLastHeading{0.0f};  // last direction the player moved in at a real speed
    bool ctrlHasHeading = false;
    glm::vec2 ctrlPlayerAvg{0.0f};    // smoothed centre of the player's recent positions
    int thrownPhase = 0;
    float thrownTimer = 0.0f;
    float thrownDuration = 0.0f;
    float thrownDistance = 0.0f;
    float thrownStartX = 0.0f, thrownStartY = 0.0f;
    float thrownTargetX = 0.0f, thrownTargetY = 0.0f;
    float thrownHeading = 0.0f;
    float thrownSpin = 0.0f;          // body angle; turns with flight speed

    // Quasar state.
    int quasarState = 0;
    float quasarTimer = 0.0f;
    float quasarAimX = 1.0f;
    float quasarAimY = 0.0f;
    bool quasarAimSet = false;        // aim chosen for this attack (at wind-up start)
    float quasarVisualAngle = 0.0f;
    float quasarSpinSpeed = 2.2f;
    float quasarCoreAngle = 0.0f;
    float quasarShellSpin = 0.0f;   // accretion shell spin; speeds up with heat
    float quasarMoveAngle = 0.0f;
    float quasarMoveTurn = 0.0f;
    float quasarMoveTimer = 0.0f;
    bool quasarNextFall = false;
    float quasarNearTimer = 0.0f;           // time the player has stayed close (0..TRIGGER_TIME)
    float quasarEruptCooldown = 0.0f;       // no eruption before this runs out
    bool quasarEruptActive = false;         // an eruption is running (alongside any other attack)
    float quasarEruptTimer = 0.0f;          // seconds since it started
    bool quasarEruptBlasted = false;        // beat 0 went off
    bool quasarEruptBlasted2 = false;       // beat 1 went off
    int quasarEruptCount = 0;
    float quasarEruptRingGap = QUASAR_ERUPT_RING_GAP;
    std::array<unsigned char, QUASAR_ERUPT_MAX_ZONES> quasarEruptRing{};
    std::array<unsigned char, QUASAR_ERUPT_MAX_ZONES> quasarEruptBeat{};
    std::array<glm::vec2, QUASAR_ERUPT_MAX_ZONES> quasarEruptZones{};
    int quasarFallFired = 0;
    float quasarFallNextFire = QUASAR_FALL_SPLIT;
    float quasarFallHitUntil = 0.0f;
    unsigned quasarFallTargetsMask = 0;
    unsigned quasarFallImpacts = 0;
    unsigned quasarFallPredictedMask = 0;   // shots aimed at the player's predicted spot
    int quasarReflections = 0;              // beam wall bounces (set at spawn from the difficulty)
    glm::vec4 quasarBounds{0.0f};           // arena (minX, minY, maxX, maxY) seen last frame
    glm::vec2 quasarPlayerPrev{0.0f};
    glm::vec2 quasarPlayerVel{0.0f};
    bool quasarPlayerSeen = false;
    std::array<glm::vec2, QUASAR_FALL_COUNT> quasarFallOrigins{};
    std::array<glm::vec2, QUASAR_FALL_COUNT> quasarFallTargets{};
    std::array<float, QUASAR_FALL_COUNT> quasarFallPower{};   // 0..1 by distance from the Quasar
    std::array<float, QUASAR_FALL_COUNT> quasarFallLaunchTimes{};
    std::array<glm::vec2, QUASAR_FALL_COUNT> quasarFallControl1{};
    std::array<glm::vec2, QUASAR_FALL_COUNT> quasarFallControl2{};

    // Gravis state.
    float gravisVisualAngle = 0.0f;
    float gravisDriftAngle = 0.0f;
    float gravisDriftTimer = 0.0f;
    float gravisOrbitPhase = 0.0f;      // decorative orbiting node
    // Gimbal mirrors Scope's accelerating two-rotation charge for one shot.
    float gimbalAttackTimer = 0.0f;
    float gimbalChargeAngle = 0.0f;
    float gimbalChargeSpeed = 0.0f;
    float gimbalBurstTimer = 0.0f;
    float gimbalVisualAngle = 0.0f;
    float gimbalWanderTimer = 0.0f;     // time left on the current random heading
    float gimbalWanderAngle = 0.0f;
    bool gimbalCharging = false;
    bool gimbalBursting = false;
    bool gimbalFired = false;

    // Antares chase state. Trail samples (x, y, angle) drive afterimages.
    AntaresState antaresState = AntaresState::CHASE;
    float antaresStateTimer = 0.0f;
    float antaresHeadingX = 0.0f;
    float antaresHeadingY = 0.0f;
    float antaresMoveSpeed = 0.0f;
    float antaresSpinSpeed = 0.0f;
    float antaresVisualAngle = 0.0f;
    float antaresPrevFacing = 1.0f;   // dot(heading, to-player) last frame
    float antaresSkidDecel = 0.0f;
    // Player displacement requested by a ram; the game loop applies and clears it.
    float antaresKnockX = 0.0f;
    float antaresKnockY = 0.0f;
    float antaresTrailTimer = 0.0f;
    std::array<glm::vec3, ANTARES_TRAIL_COUNT> antaresTrail{};
    // Spawns sit off-map; the edge only holds Antares once it has come in.
    bool antaresInArena = false;
    int antaresGen = 0;               // 0 elite, 1 and 2 split descendants
    bool triangulumReleased = false;  // TRIANGULUM: its children are out
    float triangulumChildHpMult = 1.0f;   // TRIANGULUM: spawn-time HP ramp handed to its Swarms
    bool antaresSplit = false;        // children already released
    int antaresBounces = 0;           // edge reflections so far
    float antaresBounceCd = 0.0f;     // after bouncing off another Antares
    glm::vec4 antaresBounds{0.0f};    // map rect inset by the body radius
    // Per face: shots it can still stop, seconds since it broke (< 0 while
    // intact) and seconds since it last restored (drives the snap-back look).
    std::array<int, MAGNETAR_FACES> magnetarFaceHits{};
    std::array<float, MAGNETAR_FACES> magnetarFaceBroken{};
    std::array<float, MAGNETAR_FACES> magnetarFaceRestored{};
    float magnetarVisualAngle = 0.0f;
    float magnetarFacing = 0.0f;       // magnetic axis; the N pole tracks the player
    float magnetarInterceptTimer = 0.0f;
    float magnetarInterceptX = 0.0f;
    float magnetarInterceptY = 0.0f;
    // Per face: hit flash time left (MAGNETAR_HIT_FLASH .. 0).
    std::array<float, MAGNETAR_FACES> magnetarFaceHitFlash{};

    // Damage over time.
    float burnTimer = 0.0f;
    float burnDps = 0.0f;

    Monster(float startX, float startY, float hpMul = 1.0f,
            float speedMul = 1.0f, bool isSummoned = false)
        : worldX(startX), worldY(startY), summoned(isSummoned) {
        hp *= hpMul;
        speed = (120.0f + (float)(rand() % 61)) * speedMul;
        color = summoned ? glm::vec3(0.9f, 0.3f, 0.3f)
                         : glm::vec3(1.0f, 0.27f, 0.0f);
    }

    float ApplyDamage(float damage) {
        if (kind == MobKind::NORMA && (normaHidden || normaInvuln > 0.0f)) return 0.0f;
        if (kind == MobKind::NORMA && normaExposed > 0.0f) damage *= norma::kExposeDamage;
        // Phase 1 is its own bar for NORMA too: no hit takes it past the line.
        if (kind == MobKind::NORMA && normaPhase == 1)
            damage = std::min(damage, std::max(0.0f, hp - normaMaxHp * norma::kPhase2Ratio));
        if (kind == MobKind::CAELUM) {
            if (caelum::g.invuln > 0.0f) return 0.0f;
            if (caelum::g.exposed > 0.0f) damage *= caelum::kExposeDamage;
            damage *= caelum::DealtFactor();   // the player is weakened by corruption
            // Phase 1 is its own bar: it drains to the phase-2 line and no
            // further, so no hit skips phase 2.
            if (caelum::g.phase == 1)
                damage = std::min(damage, std::max(0.0f, hp - normaMaxHp * caelum::kPhase2Ratio));
        }
        const bool hadShield = regulusShield.hp > 0.0f;
        const float dealt = ApplyEnemyDamageWithShield(
            hp, alive, hitFlashTimer, regulusShield, damage,
            worldX, worldY, (summoned ? 28.0f : 18.0f) * sizeScale,
            kind == MobKind::REGULUS);
        if (hadShield && regulusShield.hp <= 0.0f && kind == MobKind::REGULUS)
            regulusShieldRegenTimer = REGULUS_SHIELD_REGEN_DELAY;
        return dealt;
    }

    void MakeKind(MobKind requested) {
        kind = requested;
        sizeScale = 1.0f;
        if (kind == MobKind::NORMA) {
            color = glm::vec3(0.72f, 0.94f, 1.0f);
            hp = norma::kBaseHp * (hp / 90.0f);   // keeps the run's HP scaling
            normaMaxHp = hp;
            normaPhase = 1;
            speed = norma::kMoveSpeed;
            sizeScale = norma::kSizeScale;
            contactDmg = norma::kContactDps;
            return;
        }
        if (kind == MobKind::CAELUM) {
            caelum::Reset();
            color = glm::vec3(1.0f, 0.82f, 0.50f);
            hp = caelum::kBaseHp * (hp / 90.0f);   // keeps the run's HP scaling
            normaMaxHp = hp;                       // the boss bar reads these two
            normaPhase = 1;
            speed = norma::kMoveSpeed;
            sizeScale = caelum::kSizeScale;
            contactDmg = caelum::kContactDps;
            return;
        }
        if (kind == MobKind::LYNX) {
            color = LYNX_COLOR;
            hp *= lynx::kHpMult;
            speed *= lynx::kSpeedMult;
            sizeScale = lynx::kSizeScale;
            contactDmg = lynx::kContactDps;
            lynxHeading = lynx::WrapAngle((float)(rand() % 628) * 0.01f);
            lynxSnipeTimer = (float)(rand() % 100) * 0.01f;     // a group never fires as one
            return;
        }
        if (kind == MobKind::TRIANGULUM) {
            color = TRIANGULUM_COLOR;
            triangulumChildHpMult = hp / 90.0f;
            hp *= triangulum::kHpMult;
            speed *= triangulum::kSpeedMult;
            sizeScale = triangulum::kSizeScale;
            contactDmg = 0.0f;                  // it never attacks
            triangulumSpin = (float)(rand() % 628) * 0.01f;
            return;
        }
        if (kind == MobKind::MIRA) {
            color = MIRA_COLOR;
            hp *= mira::kHpMult;
            speed *= mira::kSpeedMult;
            sizeScale = mira::kSizeScale;
            contactDmg = 0.0f;                  // the zone hurts instead
            return;
        }
        if (kind == MobKind::GENESIS) {
            color = GENESIS_COLOR;
            // From NIGHTMARE its Swarm throws summon random regular enemies. Decided here, from
            // the run's difficulty, so every way of making a Genesis (waves, debug spawn) agrees.
            genesisRandomSummons = (g_Difficulty == Difficulty::NIGHTMARE);
            genesisChildHpMult = hp / 90.0f;   // progression x trial ramp at spawn
            hp *= GENESIS_BASE_HP / 90.0f;
            speed *= 0.35f;
            sizeScale = 1.7f * GenesisBodySizeScale();   // tier-4 body, bigger on higher difficulties
            genesisId = s_NextGenesisId++;
            spawnTimer = 0.0f;
            hivePhase = 0;
            hiveOpenFactor = 0.0f;
            hiveSpawned = false;
            hiveSpawnCount = 0;
            genesisShellSpin = 0.0f;
            genesisSpinBase = 0.0f;
            hiveOrbitAngle = (float)(rand() % 628) * 0.01f;
            spawnAnchorX = worldX;
            spawnAnchorY = worldY;
        } else if (kind == MobKind::SWARM) {
            color = glm::vec3(0.55f, 0.0f, 0.0f);
            hp *= 0.35f;
            speed *= 1.05f;
            sizeScale = 1.0f;
        } else if (kind == MobKind::GRAVIS) {
            color = glm::vec3(0.58f, 0.42f, 1.0f);
            hp *= GRAVIS_BASE_HP / 90.0f;
            speed *= 0.24f;
            sizeScale = 2.1f;
            contactDmg = 6.0f;
            gravisVisualAngle = (float)(rand() % 628) * 0.01f;
            gravisDriftAngle = (float)(rand() % 628) * 0.01f;
            gravisDriftTimer = 1.0f + (float)(rand() % 120) * 0.01f;
            gravisOrbitPhase = (float)(rand() % 628) * 0.01f;
        } else if (kind == MobKind::QUASAR) {
            color = glm::vec3(0.42f, 0.56f, 1.0f);
            hp *= QUASAR_BASE_HP / 90.0f;
            speed *= 0.52f;
            sizeScale = 2.4f;   // tier 4: outer halo + tilted orbit add the rest
            contactDmg = 4.0f;
            quasarReflections = QuasarReflectionsFor((int)g_Difficulty);   // every spawn path gets the run's bounces
            quasarState = 0;
            quasarSpinSpeed = 2.2f;
            quasarCoreAngle = 0.0f;
            quasarNextFall = false;
            quasarNearTimer = 0.0f;
            quasarEruptCooldown = 0.0f;
            quasarEruptActive = false;
            quasarEruptTimer = 0.0f;
            quasarEruptBlasted = false;
            quasarEruptBlasted2 = false;
            quasarEruptCount = 0;
            quasarFallFired = 0;
            quasarFallNextFire = QUASAR_FALL_SPLIT;
            quasarFallHitUntil = 0.0f;
            quasarFallTargetsMask = quasarFallImpacts = 0;
            quasarFallPredictedMask = 0;
            quasarPlayerSeen = false;
            quasarPlayerVel = glm::vec2(0.0f);
            quasarTimer = (float)(rand() % 120) * 0.01f;
            quasarVisualAngle = (float)(rand() % 628) * 0.01f;
            quasarMoveAngle = (float)(rand() % 628) * 0.01f;
            quasarMoveTurn = ((float)(rand() % 121) - 60.0f) * 0.01f;
            quasarMoveTimer = 0.8f + (float)(rand() % 101) * 0.01f;
        } else if (kind == MobKind::GIMBAL) {
            color = glm::vec3(0.62f, 0.95f, 0.28f);
            hp *= 1.65f;
            speed *= 0.58f;
            sizeScale = 1.12f;
            contactDmg = 4.0f;
            gimbalAttackTimer = 1.25f + (float)(rand() % 90) * 0.01f;
            gimbalChargeAngle = 0.0f;
            gimbalChargeSpeed = 0.4f;
            gimbalBurstTimer = 0.0f;
            gimbalVisualAngle = (float)(rand() % 628) * 0.01f;
            gimbalWanderTimer = (float)(rand() % 150) * 0.01f;
            gimbalWanderAngle = (float)(rand() % 628) * 0.01f;
            gimbalCharging = false;
            gimbalBursting = false;
            gimbalFired = false;
        } else if (kind == MobKind::REGULUS) {
            color = glm::vec3(0.722f, 0.937f, 1.0f);
            hp *= REGULUS_BASE_HP / 90.0f;
            speed *= 0.42f;
            sizeScale = 1.9f;   // elite: bigger than the old support body
            contactDmg = 0.0f;
            regulusBaseHp = hp;
            regulusShield = {};
            regulusShield.maxHp = regulusBaseHp * 0.9f;
            regulusShield.hp = regulusShield.maxHp;
            regulusShieldRegenTimer = 0.0f;
            regulusVisualAngle = (float)(rand() % 628) * 0.01f;
            regulusDriftAngle = (float)(rand() % 628) * 0.01f;
            regulusDriftTimer = 0.8f + (float)(rand() % 120) * 0.01f;
            regulusShotTimer = 0.5f;
        } else if (kind == MobKind::ANTARES) {
            color = glm::vec3(0.91f, 0.275f, 0.227f);
            antaresHpMult = hp / 90.0f;   // progression x trial ramp at spawn
            static int s_nextAntaresFamily = 0;
            antaresFamily = ++s_nextAntaresFamily;   // fragments take their parent's
            hp *= ANTARES_BASE_HP / 90.0f * ANTARES_ELITE_HP_MULT;
            sizeScale = 2.4f;   // last split generation (x0.8^2) still reads bigger than a Rotor
            antaresGen = 0;
            antaresSplit = false;
            antaresBounces = 0;
            antaresBounceCd = 0.0f;
            antaresState = AntaresState::CHASE;
            antaresStateTimer = 0.0f;
            antaresHeadingX = antaresHeadingY = 0.0f;
            antaresMoveSpeed = ANTARES_MIN_SPEED;
            antaresSpinSpeed = 0.5236f;
            antaresVisualAngle = (float)(rand() % 628) * 0.01f;
            antaresPrevFacing = 1.0f;
            antaresSkidDecel = 0.0f;
            antaresKnockX = antaresKnockY = 0.0f;
            antaresTrailTimer = 0.0f;
            antaresTrail.fill(glm::vec3(worldX, worldY, antaresVisualAngle));
            antaresInArena = false;
        } else if (kind == MobKind::MAGNETAR) {
            color = glm::vec3(1.0f, 0.84f, 0.28f);
            hp *= MAGNETAR_BASE_HP / 90.0f;
            speed *= 0.78f;
            sizeScale = 1.8f;
            contactDmg = 5.0f;
            magnetarFaceHits.fill(MAGNETAR_FACE_HITS);
            magnetarFaceBroken.fill(-1.0f);
            magnetarFaceRestored.fill(99.0f);
            magnetarVisualAngle = (float)(rand() % 628) * 0.01f;
            magnetarInterceptTimer = 0.0f;
        }
    }

    // Stages 1-4 are read from the current speed, so they rise continuously
    // and fall back together with the speed after an overshoot.
    int AntaresStage() const {
        if (antaresMoveSpeed >= ANTARES_PASS_SPEED) return 4;
        if (antaresMoveSpeed >= 350.0f) return 3;
        if (antaresMoveSpeed >= 200.0f) return 2;
        return 1;
    }

    // Frame edge plus the player's body, so a ram lands on touch, not overlap.
    float AntaresRamRange() const { return 26.0f * sizeScale + 18.0f; }

    // Keeps Antares inside the map (world-space edges). Until its frame is
    // fully inside it is left alone; after that a run (stage 2 and up) that
    // hits an edge ricochets: the component that left the map reverses and
    // the speed is kept, so bouncing is part of every rush. A stage 1 crawl
    // just slides along the edge.
    void ConfineAntares(float left, float top, float right, float bottom) {
        if (!alive || kind != MobKind::ANTARES || thrownPhase != 0) return;
        const float radius = 26.0f * sizeScale;
        const float minX = left + radius, maxX = right - radius;
        const float minY = top + radius, maxY = bottom - radius;
        if (minX >= maxX || minY >= maxY) return;
        antaresBounds = glm::vec4(minX, minY, maxX, maxY);
        const bool inside = worldX >= minX && worldX <= maxX &&
                            worldY >= minY && worldY <= maxY;
        if (!antaresInArena) {
            antaresInArena = inside;
            return;
        }
        if (inside) return;
        const bool outX = worldX < minX || worldX > maxX;
        const bool outY = worldY < minY || worldY > maxY;
        worldX = std::clamp(worldX, minX, maxX);
        worldY = std::clamp(worldY, minY, maxY);
        if ((antaresState == AntaresState::CHASE ||
             antaresState == AntaresState::SKID) && AntaresStage() >= 2) {
            if (outX) antaresHeadingX = -std::fabs(antaresHeadingX) * (worldX >= maxX ? 1.0f : -1.0f);
            if (outY) antaresHeadingY = -std::fabs(antaresHeadingY) * (worldY >= maxY ? 1.0f : -1.0f);
            ++antaresBounces;
        }
    }

    // 0-1 position between the slowest and the top speed.
    float AntaresSpeedRatio() const {
        return std::clamp((antaresMoveSpeed - ANTARES_MIN_SPEED) /
                          (ANTARES_MAX_SPEED - ANTARES_MIN_SPEED), 0.0f, 1.0f);
    }

    // Afterimages grow with speed (0 at the slowest, 6 at the top) and fade
    // one by one as a skid bleeds the speed off.
    int AntaresGhostCount() const {
        return (int)std::ceil((float)ANTARES_TRAIL_COUNT * AntaresSpeedRatio());
    }

    void SpawnRotorDeathEffect(float base, float visualTime, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 20; burst.directions = 4;
        burst.life = 0.44f; burst.speedMin = radius * 3.5f; burst.speedMax = radius * 5.2f;
        burst.phase = visualTime * 0.78f + (float)((size_t)this % 628) * 0.01f;
        burst.startRadius = 0.16f; burst.jitter = 0.45f;
        burst.tangent = 0.4f; burst.colorLift = 0.10f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 6, reduced);
    }

    void SpawnSwarmDeathEffect(float base, float visualTime, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 16; burst.directions = 3;
        burst.life = 0.40f; burst.speedMin = radius * 3.5f; burst.speedMax = radius * 5.2f;
        burst.sizeMin = 2.0f; burst.sizeMax = 4.0f;
        burst.phase = visualTime * 0.62f + (float)((size_t)this % 628) * 0.01f - 1.5708f;
        burst.startRadius = 0.12f; burst.jitter = 0.55f; burst.colorLift = 0.08f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 4, reduced);
    }

    void SpawnGimbalDeathEffect(float base, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 38; burst.directions = 0; burst.life = 0.58f;
        burst.phase = gimbalVisualAngle; burst.randomPlacement = true;
        burst.startRadius = 0.10f; burst.jitter = 0.0f;
        burst.speedMin = radius * 3.2f; burst.speedMax = radius * 5.0f;
        burst.sizeMin = 2.8f; burst.sizeMax = 7.0f; burst.colorLift = 0.20f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        burst.count = 12; burst.life = 0.48f; burst.delay = 0.035f;
        burst.speedMin = radius * 3.8f; burst.speedMax = radius * 5.2f;
        burst.sizeMin = 5.0f; burst.sizeMax = 10.0f; burst.colorLift = 0.54f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 10, reduced);
    }

    void SpawnAntaresDeathEffect(float base, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 30; burst.directions = 5; burst.life = 0.50f;
        burst.phase = antaresVisualAngle;
        burst.startRadius = 0.18f; burst.jitter = 0.40f;
        burst.speedMin = radius * 3.6f; burst.speedMax = radius * 5.4f;
        burst.sizeMin = 2.5f; burst.sizeMax = 8.0f; burst.colorLift = 0.30f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 12, reduced);
    }

    void SpawnRegulusDeathEffect(float base, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 40;
        burst.life = 0.54f;
        burst.randomPlacement = true;
        burst.startRadius = 0.16f;
        burst.speedMin = radius * 3.2f;
        burst.speedMax = radius * 5.0f;
        burst.sizeMin = 2.8f;
        burst.sizeMax = 8.5f;
        burst.colorLift = 0.38f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 12, reduced);
    }

    bool DeathEffectVisible() const {
        if (deathEffectTimer < 0.0f) return false;
        const float duration = kind == MobKind::GRAVIS ? 0.40f
                             : kind == MobKind::TRIANGULUM ? triangulum::kHoldTime + triangulum::kShatterTime
                             : 0.0f;
        return deathEffectTimer < duration;
    }

    void UpdateDeathEffect(float delta) {
        if (!alive && DeathEffectVisible())
            deathEffectTimer += std::max(0.0f, delta);
    }

    // True once, when a held kill reward should be released (the Quasar and
    // Genesis hold it until the kill is processed).
    bool TakePendingDust() {
        if (!dustPending) return false;
        dustPending = false;
        return true;
    }

    // Debris for blast wave k, spawned at the kill; it stays inside the
    // wave's reach.
    void SpawnGenesisWaveBurst(float base, int k, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        static constexpr int kCount[] = { 44, 28 };
        static_assert(GENESIS_WAVE_COUNT <= 2, "kCount needs a burst per wave");
        const float f = GENESIS_WAVE_COUNT > 1
            ? (float)k / (float)(GENESIS_WAVE_COUNT - 1) : 0.0f;        // 0..1
        const float falloff = powf(GENESIS_WAVE_FALLOFF, (float)k);
        EnemyParticleBurst burst;
        burst.count = kCount[k]; burst.phase = hiveOrbitAngle;
        burst.life = 0.60f - 0.20f * f; burst.tangent = 0.18f;
        burst.startRadius = 0.12f; burst.jitter = 0.35f;
        burst.rangeScale = GENESIS_WAVE_REACH * falloff * base / radius;
        burst.speedMin = radius * 6.0f * falloff;
        burst.speedMax = radius * 8.2f * falloff;
        burst.sizeMin = 2.0f; burst.sizeMax = 9.0f - 5.0f * f;
        burst.colorLift = 0.45f - 0.25f * f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
    }

    template <typename Line, typename Disc, typename Hexagon>
    void DrawGravisDeathEffect(Line line, Disc disc, Hexagon hexagon) const {
        const float base = (summoned ? 28.0f : 18.0f) * sizeScale;
        const float collapse = std::clamp(deathEffectTimer / 0.08f, 0.0f, 1.0f);
        const float pull = collapse * collapse;
        const float fade = 1.0f - std::clamp((deathEffectTimer - 0.08f) / 0.10f, 0.0f, 1.0f);
        const glm::vec3 bright = color + (glm::vec3(1.0f) - color) * (pull * 0.65f);
        if (deathEffectTimer < 0.08f) {
            const float orbit = base * 1.05f * (1.0f - pull * 0.95f);
            const float spin = gravisVisualAngle + pull * 1.8f;
            for (int i = 0; i < 6; ++i) {
                const float a = spin + (float)i * 1.0471976f;
                const float b = a + 1.0471976f;
                const float x = worldX + cosf(a) * orbit, y = worldY + sinf(a) * orbit;
                line(x, y, worldX + cosf(b) * orbit, worldY + sinf(b) * orbit,
                     1.6f, bright.r, bright.g, bright.b, 0.85f);
                line(worldX, worldY, x, y, 1.0f,
                     bright.r, bright.g, bright.b, 0.48f);
                hexagon(x, y, base * 0.18f * (1.0f - pull * 0.80f), a,
                        bright.r, bright.g, bright.b, 0.90f);
            }
        }
        const float coreRadius = base * (deathEffectTimer < 0.08f
                                      ? 0.58f * (1.0f - pull * 0.85f) : 0.30f * fade);
        disc(worldX, worldY, coreRadius * 1.4f,
             bright.r, bright.g, bright.b, fade * 0.18f);
        hexagon(worldX, worldY, coreRadius * 0.65f, gravisVisualAngle,
                bright.r, bright.g, bright.b, fade);
    }

    // Each recipe lives beside its mob; the renderer supplies batched primitives.
    template <typename Line, typename Disc, typename Hexagon>
    void DrawDeathEffect(Line line, Disc disc, Hexagon hexagon) const {
        if (!DeathEffectVisible()) return;
        switch (kind) {
        case MobKind::GRAVIS: DrawGravisDeathEffect(line, disc, hexagon); break;
        default: break;
        }
    }

    void SpawnGravisDeathEffect(float base, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.count = 12; burst.life = 0.10f; burst.phase = gravisVisualAngle;
        burst.startRadius = 0.85f; burst.inward = true;
        burst.speedMin = radius * 8.0f; burst.speedMax = radius * 10.0f;
        burst.sizeMin = 2.5f; burst.sizeMax = 5.0f; burst.colorLift = 0.08f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        burst.count = 36; burst.life = 0.70f; burst.delay = 0.08f;
        burst.startRadius = 0.10f; burst.inward = false; burst.tangent = 0.40f;
        burst.speedMin = radius * 3.8f; burst.speedMax = radius * 5.3f;
        burst.sizeMin = 3.0f; burst.sizeMax = 13.0f; burst.colorLift = 0.22f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        SpawnMobDeathSparks(worldX, worldY, color, radius, 14, reduced);
    }

    // Blast (at the kill): a fast blue-white shell, then slower heavier
    // debris, both kept inside QUASAR_NOVA_REACH.
    void SpawnQuasarBlastBurst(float base, bool reduced) const {
        const float radius = MobDeathParticleRadius(base);
        EnemyParticleBurst burst;
        burst.phase = quasarVisualAngle;
        burst.rangeScale = QUASAR_NOVA_REACH * base / radius;
        burst.count = 56; burst.life = QUASAR_NOVA_WAVE_TIME;
        burst.startRadius = 0.06f; burst.sizeMin = 2.0f; burst.sizeMax = 6.0f;
        burst.speedMin = radius * 2.4f; burst.speedMax = radius * 3.4f;
        burst.colorLift = 0.65f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
        burst.count = 30; burst.life = QUASAR_NOVA_WAVE_TIME + 0.10f; burst.delay = 0.04f;
        burst.tangent = 0.25f; burst.sizeMin = 4.0f; burst.sizeMax = 10.0f;
        burst.speedMin = radius * 1.2f; burst.speedMax = radius * 2.0f;
        burst.colorLift = 0.35f;
        SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
    }

    void SpawnDeathEffect(float visualTime, bool reduced = false,
                           bool animateBody = true) {
        if (exploded) return;
        exploded = true;
        if (animateBody && (kind == MobKind::GRAVIS || kind == MobKind::TRIANGULUM))
            deathEffectTimer = 0.0f;
        const float base = (summoned ? 28.0f : 18.0f) * sizeScale;
        switch (kind) {
        case MobKind::ROTOR: SpawnRotorDeathEffect(base, visualTime, reduced); break;
        case MobKind::SWARM: SpawnSwarmDeathEffect(base, visualTime, reduced); break;
        // Elites burst into particles only, at the kill (no body animation).
        case MobKind::QUASAR: SpawnQuasarBlastBurst(base, reduced); break;
        case MobKind::GENESIS:
            for (int k = 0; k < GENESIS_WAVE_COUNT; ++k)
                SpawnGenesisWaveBurst(base, k, reduced);
            break;
        case MobKind::GRAVIS: SpawnGravisDeathEffect(base, reduced); break;
        case MobKind::GIMBAL: SpawnGimbalDeathEffect(base, reduced); break;
        case MobKind::REGULUS: SpawnRegulusDeathEffect(base, reduced); break;
        case MobKind::ANTARES: SpawnAntaresDeathEffect(base, reduced); break;
        case MobKind::TRIANGULUM:
        case MobKind::MIRA: {
            const float radius = MobDeathParticleRadius(base);
            EnemyParticleBurst burst;
            burst.count = 14; burst.life = 0.40f;
            burst.startRadius = 0.25f; burst.jitter = 0.35f;
            burst.speedMin = radius * 1.6f; burst.speedMax = radius * 3.0f;
            burst.sizeMin = 2.0f; burst.sizeMax = 4.0f;
            burst.colorLift = 0.25f;
            SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
            break;
        }
        case MobKind::LYNX: {
            const float radius = MobDeathParticleRadius(base);
            EnemyParticleBurst burst;
            burst.count = 16; burst.life = 0.34f;
            burst.phase = lynxHeading;
            burst.startRadius = 0.25f; burst.jitter = 0.30f;
            burst.speedMin = radius * 2.2f; burst.speedMax = radius * 3.8f;
            burst.sizeMin = 2.0f; burst.sizeMax = 4.5f;
            burst.colorLift = 0.25f;
            SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
            SpawnMobDeathSparks(worldX, worldY, color, radius, 5, reduced);
            break;
        }
        case MobKind::MAGNETAR: {
            const float radius = MobDeathParticleRadius(base);
            EnemyParticleBurst burst;
            burst.count = 32; burst.directions = 4; burst.life = 0.48f;
            burst.phase = magnetarVisualAngle;
            burst.startRadius = 0.10f; burst.jitter = 0.28f;
            burst.speedMin = radius * 3.0f; burst.speedMax = radius * 5.4f;
            burst.sizeMin = 2.5f; burst.sizeMax = 8.0f;
            burst.colorLift = 0.46f;
            SpawnEnemyParticleBurst(worldX, worldY, color, radius, burst, reduced);
            SpawnMobDeathSparks(worldX, worldY, color, radius, 10, reduced);
            break;
        }
        }
    }

    void ConstrainGenesisToViewport(float left, float top, float right, float bottom) {
        if (kind != MobKind::GENESIS) return;
        // Include the hull, rear damper plates and node glow, with a gap.
        const float margin = (summoned ? 28.0f : 18.0f) * sizeScale
                           * 2.75f + 8.0f;
        const float minX = left + std::min(margin, std::max(0.0f, right - left) * 0.5f);
        const float minY = top + std::min(margin, std::max(0.0f, bottom - top) * 0.5f);
        const float maxX = std::max(minX, right - margin);
        const float maxY = std::max(minY, bottom - margin);
        spawnAnchorX = std::clamp(spawnAnchorX, minX, maxX);
        spawnAnchorY = std::clamp(spawnAnchorY, minY, maxY);
        worldX = std::clamp(worldX, minX, maxX);
        worldY = std::clamp(worldY, minY, maxY);
    }

    void ConstrainGimbalToViewport(float left, float top, float right, float bottom) {
        if (kind != MobKind::GIMBAL) return;
        // Keep the complete rotating body and node glow inside the camera view.
        const float margin = 34.0f * sizeScale;
        const float minX = left + std::min(margin, std::max(0.0f, right - left) * 0.5f);
        const float minY = top + std::min(margin, std::max(0.0f, bottom - top) * 0.5f);
        const float maxX = std::max(minX, right - margin);
        const float maxY = std::max(minY, bottom - margin);
        worldX = std::clamp(worldX, minX, maxX);
        worldY = std::clamp(worldY, minY, maxY);
    }

    // 0..1 strength of a buff: fades in after it lands, then stays.
    static float RegulusBuffLevel(float age) {
        if (age < 0.0f) return 0.0f;
        return std::clamp(age / REGULUS_BUFF_FADE, 0.0f, 1.0f);
    }
    float RegulusSpeedLevel() const { return RegulusBuffLevel(regulusSpeedAge); }
    float RegulusStealthLevel() const { return RegulusBuffLevel(regulusStealthAge); }
    bool RegulusBuffed() const {
        return regulusShield.acquiredOnce || regulusSpeedAge >= 0.0f || regulusStealthAge >= 0.0f;
    }
    // Speed factor the manager applies on top of the mob's own speed.
    float RegulusSpeedBuff() const {
        return 1.0f + (REGULUS_SPEED_BUFF_MULT - 1.0f) * RegulusSpeedLevel();
    }
    // Drawing opacity of a cloaked mob.
    float RegulusCloakAlpha() const {
        return 1.0f - (1.0f - REGULUS_STEALTH_ALPHA) * RegulusStealthLevel();
    }
    // Laser / marker colour of each buff: shield ice-blue, speed amber, stealth violet.
    static glm::vec3 RegulusBuffColor(int buff) {
        switch (buff) {
        case REGULUS_BUFF_SPEED:   return glm::vec3(1.0f, 0.72f, 0.22f);
        case REGULUS_BUFF_STEALTH: return glm::vec3(0.72f, 0.50f, 1.0f);
        default:                   return glm::vec3(0.722f, 0.937f, 1.0f);
        }
    }

    // A Lynx flies straight into the view from its spawn edge, wherever the
    // player is, then stays inside it with the hull fully visible.
    void ConstrainLynxToViewport(float left, float top, float right, float bottom,
                                   float deltaTime) {
        if (kind != MobKind::LYNX) return;
        if (LynxHoldsPosition()) {
            worldX = lynxHoldX;
            worldY = lynxHoldY;
            return;
        }
        const float margin = 120.0f * lynx::HullScale(sizeScale);
        const float minX = left + std::min(margin, std::max(0.0f, right - left) * 0.5f);
        const float minY = top + std::min(margin, std::max(0.0f, bottom - top) * 0.5f);
        const float maxX = std::max(minX, right - margin);
        const float maxY = std::max(minY, bottom - margin);
        if (!lynxInArena) {
            const float gapX = std::clamp(worldX, minX, maxX) - worldX;
            const float gapY = std::clamp(worldY, minY, maxY) - worldY;
            const float gap = sqrtf(gapX * gapX + gapY * gapY);
            const float step = lynx::kEntrySpeed * deltaTime;
            lynxInArena = gap <= step;
            const float t = lynxInArena ? 1.0f : step / gap;
            worldX += gapX * t;
            worldY += gapY * t;
            return;
        }
        worldX = std::clamp(worldX, minX, maxX);
        worldY = std::clamp(worldY, minY, maxY);
    }

    void ConstrainRegulusToViewport(float left, float top,
                                    float right, float bottom) {
        if (kind != MobKind::REGULUS) return;
        // Keep the enlarged outer shield ring fully visible and turn its drift
        // back into the arena when it reaches a screen edge.
        const float base = (summoned ? 28.0f : 18.0f) * sizeScale;
        const float margin = base * 2.2f + 14.0f;
        const float halfWidth = std::max(0.0f, right - left) * 0.5f;
        const float halfHeight = std::max(0.0f, bottom - top) * 0.5f;
        const float minX = left + std::min(margin, halfWidth);
        const float minY = top + std::min(margin, halfHeight);
        const float maxX = std::max(minX, right - margin);
        const float maxY = std::max(minY, bottom - margin);
        const bool hitX = worldX < minX || worldX > maxX;
        const bool hitY = worldY < minY || worldY > maxY;
        worldX = std::clamp(worldX, minX, maxX);
        worldY = std::clamp(worldY, minY, maxY);
        if (hitX) regulusDriftAngle = 3.14159265f - regulusDriftAngle;
        if (hitY) regulusDriftAngle = -regulusDriftAngle;
    }

    // 0 at rest, rising through search/charge, 1 while firing, then decaying
    // over the 1.4s recovery. Drives the shell spin rate and disc flattening.
    // Ward lattice orientation, shared by the hit test and the drawing.
    float MagnetarYaw() const { return magnetarVisualAngle * 0.6f; }
    float MagnetarPitch() const { return 0.45f + 0.15f * sinf(magnetarVisualAngle * 0.3f); }
    int MagnetarFaceToward(float ang) const {
        return MagnetarLattice::FaceToward(ang, MagnetarYaw(), MagnetarPitch());
    }

    // Wind-up progress 0..1 over the last QUASAR_PRECHARGE_TIME of the
    // cooldown (state 0); 0 otherwise.
    float QuasarPrecharge() const {
        if (quasarState != 0 || quasarNextFall) return 0.0f;
        const float start = QUASAR_COOLDOWN_TIME - QUASAR_PRECHARGE_TIME;
        return std::clamp((quasarTimer - start) / QUASAR_PRECHARGE_TIME, 0.0f, 1.0f);
    }

    float QuasarHeat() const {
        float h = QuasarStateHeat();
        if (quasarEruptActive)
            h = std::max(h, 0.85f * QuasarEase(quasarEruptTimer / (QUASAR_ERUPT_BLAST_TIME * 0.6f)));
        return h;
    }
    float QuasarStateHeat() const {
        switch (quasarState) {
        case 0: return 0.15f * QuasarPrecharge();
        case 1: return 0.15f;
        case 2: return 0.15f + 0.85f * std::min(1.0f, quasarTimer / QUASAR_LOCK_TIME);
        case 3: return 1.0f;
        case 4: return 1.0f - QuasarEase(quasarTimer / 1.4f);
        case QUASAR_FALL_STATE: return 0.85f * QuasarEase(quasarTimer / QUASAR_FALL_SPLIT);
        case QUASAR_FALL_RECOVER_STATE: return 0.85f * QuasarFallOpen();
        default: return 0.0f;
        }
    }

    bool QuasarFalling() const {
        return alive && quasarState == QUASAR_FALL_STATE;
    }

    static float QuasarEase(float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * x * (10.0f + x * (-15.0f + x * 6.0f));
    }

    float QuasarFallOpen() const {
        if (QuasarFalling()) return QuasarEase((quasarTimer / QUASAR_FALL_SPLIT - 0.28f) / 0.72f);
        if (quasarState == QUASAR_FALL_RECOVER_STATE)
            return 1.0f - QuasarEase(quasarTimer / (QUASAR_FALL_RECOVER - QUASAR_FALL_IMPACT_FX));
        return 0.0f;
    }

    float QuasarFallGather() const {
        if (!QuasarFalling()) return 0.0f;
        const float p = quasarTimer / QUASAR_FALL_SPLIT;
        return QuasarEase(p / 0.28f) * (1.0f - QuasarEase((p - 0.28f) / 0.5f));
    }

    float QuasarSpinTarget() const {
        if (QuasarFalling()) return 2.2f - 1.8f * QuasarFallGather() + 9.3f * QuasarFallOpen();
        if (quasarState == QUASAR_FALL_RECOVER_STATE) return 2.2f + 9.3f * QuasarFallOpen();
        if (quasarState == 0) return 2.2f * (1.0f - 0.8f * QuasarEase(QuasarPrecharge() / 0.5f));
        if (quasarState == 1) return 0.44f;
        if (quasarState == 2) return 0.44f + 6.16f * QuasarEase(quasarTimer / QUASAR_LOCK_TIME);
        if (quasarState == 3) return 6.6f;
        return 2.2f + 4.4f * (1.0f - QuasarEase(quasarTimer / 1.4f));
    }

    float QuasarFallImpactTime(int i) const {
        return quasarFallLaunchTimes[i] + QuasarFallWarning(i);
    }

    float QuasarFallPulse() const {
        if (!QuasarFalling() || quasarFallFired == 0) return 0.0f;
        return std::clamp(1.0f - (quasarTimer - quasarFallLaunchTimes[quasarFallFired - 1]) / 0.16f, 0.0f, 1.0f);
    }

    float QuasarPortVisibility(int port) const {
        if (!QuasarFalling()) return 1.0f;
        for (int i = quasarFallFired - 1; i >= 0; --i)
            if (i % 4 == port)
                return std::clamp((quasarTimer - quasarFallLaunchTimes[i] - 0.08f) / 0.15f, 0.0f, 1.0f);
        return 1.0f;
    }

    float QuasarFlangeAngle() const {
        return quasarVisualAngle + (float)((size_t)this % 97u) * 0.37f;
    }

    // The same four flange nodes are used by the body and the falling debris.
    glm::vec2 QuasarFlangeNode(int i) const {
        const float base = (summoned ? 28.0f : 18.0f) * sizeScale;
        const float a = quasarShellSpin * 0.45f + 1.5707963f * i;
        const float f = QuasarFlangeAngle();
        const float u = cosf(a) * base * (1.55f - 0.18f * QuasarFallGather() + 0.65f * QuasarFallOpen());
        const float v = sinf(a) * base * (0.40f - 0.10f * QuasarFallGather() + 0.70f * QuasarFallOpen()) * (1.0f - 0.30f * QuasarHeat());
        return {worldX + cosf(f) * u + sinf(f) * v,
                worldY + sinf(f) * u - cosf(f) * v};
    }

    // Full length of each end of the beam. Every leg is at most the arena
    // diagonal, so on a big (or zoomed-out) arena a fixed 6000 px ran out
    // before the last bounced leg reached the edge and the beam stopped short
    // in the middle of the screen. The length now always covers the trip to
    // the far side of the arena, every bounce, and a last leg out past the edge.
    float QuasarBeamLength() const {
        float len = QUASAR_BEAM_LENGTH;
        if (quasarReflections > 0 && quasarBounds.z > quasarBounds.x && quasarBounds.w > quasarBounds.y) {
            const float w = quasarBounds.z - quasarBounds.x, h = quasarBounds.w - quasarBounds.y;
            const float diag = std::sqrt(w * w + h * h);
            const float toCenter = std::hypot(worldX - 0.5f * (quasarBounds.x + quasarBounds.z),
                                              worldY - 0.5f * (quasarBounds.y + quasarBounds.w));
            len = std::max(len, toCenter + 0.5f * diag + (float)quasarReflections * diag + 600.0f);
        }
        return len;
    }

    // The beam runs both ways from the Quasar along the aim axis. Each end
    // travels `reach`, bouncing off the arena edge up to quasarReflections
    // times. Returns the segments as (x0, y0, x1, y1); `bounce` (optional)
    // gets how many bounces precede each segment.
    int QuasarBeamSegments(float reach, std::array<glm::vec4, QUASAR_BEAM_MAX_SEGMENTS>& out,
                           std::array<int, QUASAR_BEAM_MAX_SEGMENTS>* bounce = nullptr) const {
        int n = 0;
        const bool arena = quasarBounds.z > quasarBounds.x && quasarBounds.w > quasarBounds.y;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            glm::vec2 p(worldX, worldY), d(quasarAimX * (float)sgn, quasarAimY * (float)sgn);
            float left = reach;
            for (int b = 0; b <= quasarReflections && left > 0.0f; ++b) {
                float t = left;
                bool hitX = false, hitY = false;
                if (arena && b < quasarReflections) {
                    float tx = 1.0e9f, ty = 1.0e9f;
                    if (d.x > 1e-6f) tx = (quasarBounds.z - p.x) / d.x;
                    else if (d.x < -1e-6f) tx = (quasarBounds.x - p.x) / d.x;
                    if (d.y > 1e-6f) ty = (quasarBounds.w - p.y) / d.y;
                    else if (d.y < -1e-6f) ty = (quasarBounds.y - p.y) / d.y;
                    tx = std::max(tx, 0.0f); ty = std::max(ty, 0.0f);
                    if (tx < t && tx <= ty) { t = tx; hitX = true; }
                    else if (ty < t) { t = ty; hitY = true; }
                }
                const glm::vec2 q = p + d * t;
                out[n] = glm::vec4(p.x, p.y, q.x, q.y);
                if (bounce) (*bounce)[n] = b;
                ++n;
                left -= t;
                if (!hitX && !hitY) break;
                if (hitX) d.x = -d.x;
                if (hitY) d.y = -d.y;
                p = q;
            }
        }
        return n;
    }

    // The Quasar stands still from the start of the wind-up until the attack is
    // over (lock, beam, starfall, recovery), and while an eruption runs. Nothing
    // may move it then: not crowding, not the player's pulls and pushes, not the
    // viewport clamp.
    // Aiming through firing: nothing shoves it, so the ray leaves from where
    // the player saw the line.
    bool LynxHoldsPosition() const {
        return kind == MobKind::LYNX && alive && lynxSnipe != lynx::SnipeRest;
    }

    bool QuasarHoldsPosition() const {
        return kind == MobKind::QUASAR && alive &&
               (quasarState != 0 || QuasarPrecharge() > 0.0f || quasarEruptActive);
    }

    // Put an enemy under a Genesis' control: smart AI and the controlled colour.
    void TakeGenesisControl(int ownerId) {
        thrownOwnerId = ownerId;
        if (!genesisControlled) freeColor = color;
        genesisControlled = true;
        color = GENESIS_CONTROLLED_COLOR;
    }
    // The Genesis is gone: the plain chase again. It keeps the controlled colour, so what the
    // Genesis made stays recognisable after it dies.
    void ReleaseGenesisControl() {
        if (!genesisControlled) return;
        genesisControlled = false;
        genesisSummonCarrier = false;       // a capsule still in the air lands as a plain Swarm
        ctrlConfidence = 1.0f;
        ctrlJukeHeat = 0.0f;
    }

    // Where to run to meet the player: the point `target` the mob can reach at the
    // same moment as a player who keeps their velocity (the classic intercept), capped
    // to GENESIS_CTRL_LEAD_MAX seconds ahead. Falls back to the player's position.
    static glm::vec2 GenesisInterceptPoint(glm::vec2 mob, glm::vec2 player, glm::vec2 vel, float mobSpeed) {
        const glm::vec2 r = player - mob;
        const float dist = glm::length(r);
        if (mobSpeed < 1.0f || dist < GENESIS_CTRL_LEAD_MIN_DIST) return player;
        const float a = glm::dot(vel, vel) - mobSpeed * mobSpeed;
        const float b = 2.0f * glm::dot(r, vel);
        const float c = glm::dot(r, r);
        float t = dist / mobSpeed;                              // plain pursuit time as the fallback
        if (std::fabs(a) < 1e-3f) {
            if (b < -1e-3f) t = -c / b;
        } else {
            const float disc = b * b - 4.0f * a * c;
            if (disc >= 0.0f) {
                const float sq = std::sqrt(disc);
                const float t1 = (-b - sq) / (2.0f * a), t2 = (-b + sq) / (2.0f * a);
                float best = 1.0e9f;
                if (t1 > 0.0f) best = std::min(best, t1);
                if (t2 > 0.0f) best = std::min(best, t2);
                if (best < 1.0e8f) t = best;
            }
        }
        t = std::clamp(t, 0.0f, GENESIS_CTRL_LEAD_MAX);
        return player + vel * t;
    }

    // Where a controlled enemy runs to, given what it has learned about the player.
    // Steady mover: the intercept point. Juking player: the centre of their movement.
    glm::vec2 GenesisAimPoint(glm::vec2 player, float distance, float baseSpeed) const {
        const glm::vec2 mob(worldX, worldY);
        glm::vec2 lead = GenesisInterceptPoint(mob, player, ctrlPlayerVel, baseSpeed) - player;
        lead *= ctrlConfidence;                                         // distrust the lead after a juke
        const float maxLead = GENESIS_CTRL_LEAD_FRAC * distance;        // never aim far beyond the player
        const float len = glm::length(lead);
        if (len > maxLead) lead *= maxLead / std::max(len, 1e-4f);
        const float lean = GENESIS_CTRL_CENTRE_BLEND * (1.0f - ctrlConfidence);
        return player + (ctrlPlayerAvg - player) * lean + lead;
    }

    // Every controlled enemy learns how the player moves, whatever it is doing this frame
    // (a Gimbal needs it for its aim, the rest for their chase).
    void GenesisTrackPlayer(float playerCX, float playerCY, float deltaTime) {
        const glm::vec2 pnow(playerCX, playerCY);
        if (ctrlPlayerSeen && deltaTime > 1e-5f) {
            glm::vec2 inst = (pnow - ctrlPlayerPrev) / deltaTime;
            float sp = glm::length(inst);
            if (sp > GENESIS_CTRL_VEL_MAX) { inst *= GENESIS_CTRL_VEL_MAX / sp; sp = GENESIS_CTRL_VEL_MAX; }
            // Reversal = moving again in (nearly) the opposite direction of the last real
            // heading. The stop in between (a sine-wave juke slows to nothing) does not hide it.
            if (sp > GENESIS_CTRL_MOVING_SPEED) {
                const glm::vec2 heading = inst / sp;
                if (ctrlHasHeading) {
                    const float c = glm::dot(heading, ctrlLastHeading);
                    if (c < GENESIS_CTRL_REVERSAL_COS) {                    // a juke
                        if (ctrlSinceReversal > GENESIS_CTRL_REVERSAL_GAP) ctrlJukeHeat += GENESIS_CTRL_HEAT_PER_REVERSAL;
                        ctrlSinceReversal = 0.0f;
                    } else if (c > GENESIS_CTRL_STEADY_COS) {               // one heading: it cools down
                        ctrlJukeHeat = std::max(0.0f, ctrlJukeHeat - GENESIS_CTRL_HEAT_COOL * deltaTime);
                    }
                }
                ctrlLastHeading = heading;
                ctrlHasHeading = true;
            }
            ctrlSinceReversal += deltaTime;
            ctrlConfidence = std::clamp(1.0f - ctrlJukeHeat / GENESIS_CTRL_HEAT_FOR_ZERO, 0.0f, 1.0f);
            ctrlPlayerVel += (inst - ctrlPlayerVel) * (1.0f - expf(-deltaTime * 10.0f));
            ctrlPlayerAvg += (pnow - ctrlPlayerAvg) * (1.0f - expf(-deltaTime / GENESIS_CTRL_CENTRE_TAU));
        } else {
            ctrlPlayerAvg = pnow;
        }
        ctrlPlayerPrev = pnow;
        ctrlPlayerSeen = true;
    }

    // Where a controlled shooter aims: the player's position led by the time the shot takes
    // to arrive, trusted as much as the lead is (a juking player gets shot at where they are).
    glm::vec2 GenesisShotAimPoint(float playerCX, float playerCY, float shotSpeed) const {
        const glm::vec2 p(playerCX, playerCY);
        if (shotSpeed < 1.0f) return p;
        float t = glm::length(p - glm::vec2(worldX, worldY)) / shotSpeed;
        for (int i = 0; i < 2; ++i)
            t = glm::length(p + ctrlPlayerVel * t - glm::vec2(worldX, worldY)) / shotSpeed;
        t = std::clamp(t, 0.0f, 1.0f);
        return p + ctrlPlayerVel * t;                       // forced: a summoned shooter always leads
    }

    // Smart chase for a controlled enemy: runs to GenesisAimPoint
    // (no sidestepping of incoming shots).
    void GenesisControlledMove(float dx, float dy, float distance, float playerCX, float playerCY,
                               float deltaTime, float speedMult, const std::vector<Bullet>* bullets) {
        const glm::vec2 pnow(playerCX, playerCY);
        const float baseSpeed = speed * speedMult;
        (void)bullets;
        if (distance > 5.0f) {
            glm::vec2 to = GenesisAimPoint(pnow, distance, baseSpeed) - glm::vec2(worldX, worldY);
            const float len = glm::length(to);
            if (len > 1e-3f) {
                to /= len;
                worldX += to.x * baseSpeed * deltaTime;
                worldY += to.y * baseSpeed * deltaTime;
            }
        }
        (void)dx; (void)dy;
    }

    // Width of the firing beam as a share of QUASAR_BEAM_RADIUS: 1 while it
    // burns, easing to 0 over the last QUASAR_BEAM_FADE_OUT of the fire window.
    float QuasarBeamWidth() const {
        if (quasarState != 3) return 1.0f;
        const float start = QUASAR_BEAM_TIME - QUASAR_BEAM_FADE_OUT;
        return 1.0f - QuasarEase((quasarTimer - start) / QUASAR_BEAM_FADE_OUT);
    }

    // Lead vector for an aimed shot: direction of travel x capped distance.
    glm::vec2 QuasarPredictLead() const {
        const float speed = glm::length(quasarPlayerVel);
        if (speed < QUASAR_PREDICT_MIN_SPEED) return glm::vec2(0.0f);
        const glm::vec2 dir = quasarPlayerVel / speed;
        return dir * std::min(speed * QUASAR_PREDICT_LEAD_TIME, QUASAR_PREDICT_MAX_LEAD);
    }

    // Flight / warning time of shot i. One value for every shot: the starfall
    // never slows down when the player is close to the Quasar.
    float QuasarFallWarning(int) const { return QUASAR_FALL_WARNING; }

    // Next shot gap: a random 0.1-0.3 s, the same wherever the player stands.
    static float QuasarFallNextGap() {
        const float u = (float)(rand() % 10001) / 10000.0f;
        return QUASAR_FALL_GAP_MIN + (QUASAR_FALL_GAP_MAX - QUASAR_FALL_GAP_MIN) * u;
    }
    float QuasarFallRadius(int i) const {
        return QUASAR_FALL_RADIUS * (1.0f + (QUASAR_FALL_MAX_RADIUS_SCALE - 1.0f) * quasarFallPower[i]);
    }
    float QuasarFallDamage(int i) const {
        return QUASAR_FALL_DAMAGE * (1.0f + (QUASAR_FALL_MAX_DAMAGE_SCALE - 1.0f) * quasarFallPower[i]);
    }

    // Zones of the eruption: concentric rings centred on the Quasar. Each ring
    // keeps two doors (one zone left out) so the player can cross between the
    // safe lanes; the whole pattern is turned by a random angle per cast.
    void BeginQuasarEruption() {
        quasarEruptActive = true;
        quasarEruptTimer = 0.0f;
        quasarEruptBlasted = false;
        quasarEruptBlasted2 = false;
        quasarEruptCount = 0;
        const glm::vec2 c(worldX, worldY);
        const bool arena = quasarBounds.z > quasarBounds.x && quasarBounds.w > quasarBounds.y;
        float reachMax = 900.0f;
        if (arena) {
            reachMax = 0.0f;
            for (const glm::vec2 corner : { glm::vec2(quasarBounds.x, quasarBounds.y), glm::vec2(quasarBounds.z, quasarBounds.y),
                                            glm::vec2(quasarBounds.x, quasarBounds.w), glm::vec2(quasarBounds.z, quasarBounds.w) })
                reachMax = std::max(reachMax, glm::length(corner - c));
        }
        reachMax = std::min(reachMax, QUASAR_ERUPT_MAX_REACH);      // only the ground around the Quasar
        const float Z = QUASAR_ERUPT_ZONE_RADIUS;
        const float turn = (float)(rand() % 10000) * 0.0006283185f;
        int rings = 0;
        for (int k = 0; ; ++k) {
            const float r = QUASAR_ERUPT_FIRST_RING + QUASAR_ERUPT_RING_STEP * (float)k;
            if (r - Z > reachMax) break;
            rings = k + 1;
            const int n = std::max(6, (int)std::ceil(6.2831853f * r / (2.0f * Z * 1.04f)));
            const float door = turn + 1.1f * (float)k;
            for (int i = 0; i < n; ++i) {
                const float ang = turn * 0.5f + 0.37f * (float)k + 6.2831853f * (float)i / (float)n;
                bool isDoor = false;
                for (int d = 0; d < 2; ++d) {
                    float gap = std::fabs(std::remainder(ang - (door + 3.1415927f * (float)d), 6.2831853f));
                    if (gap <= 3.1415927f / (float)n * 1.01f) isDoor = true;
                }
                if (isDoor) continue;
                const glm::vec2 p = c + glm::vec2(cosf(ang), sinf(ang)) * r;
                if (arena && (p.x < quasarBounds.x - Z * 0.5f || p.x > quasarBounds.z + Z * 0.5f ||
                              p.y < quasarBounds.y - Z * 0.5f || p.y > quasarBounds.w + Z * 0.5f)) continue;
                if (quasarEruptCount < QUASAR_ERUPT_MAX_ZONES) {
                    quasarEruptRing[quasarEruptCount] = (unsigned char)k;
                    quasarEruptBeat[quasarEruptCount] = 0;
                    quasarEruptZones[quasarEruptCount++] = p;
                }
            }
        }
        // Beat 1: a continuous band of zones in the lane between rings k and k+1
        // (the ground that was safe during beat 0) and one zone on the body.
        for (int k = 0; k + 1 < rings; ++k) {
            const float r = QUASAR_ERUPT_FIRST_RING + QUASAR_ERUPT_RING_STEP * ((float)k + 0.5f);
            const int n = std::max(6, (int)std::ceil(6.2831853f * r / (2.0f * Z * 1.04f)));
            const float spin = turn * 0.5f + 0.37f * (float)k + 3.1415927f / (float)n;
            for (int i = 0; i < n; ++i) {
                const float ang = spin + 6.2831853f * (float)i / (float)n;
                const glm::vec2 p = c + glm::vec2(cosf(ang), sinf(ang)) * r;
                if (arena && (p.x < quasarBounds.x - Z * 0.5f || p.x > quasarBounds.z + Z * 0.5f ||
                              p.y < quasarBounds.y - Z * 0.5f || p.y > quasarBounds.w + Z * 0.5f)) continue;
                if (quasarEruptCount < QUASAR_ERUPT_MAX_ZONES) {
                    quasarEruptRing[quasarEruptCount] = (unsigned char)k;
                    quasarEruptBeat[quasarEruptCount] = 1;
                    quasarEruptZones[quasarEruptCount++] = p;
                }
            }
        }
        if (quasarEruptCount < QUASAR_ERUPT_MAX_ZONES) {
            quasarEruptRing[quasarEruptCount] = QUASAR_ERUPT_CENTER_RING;
            quasarEruptBeat[quasarEruptCount] = 1;
            quasarEruptZones[quasarEruptCount++] = c;
        }
        // Many rings (a big arena): tighten the stagger so the last one still
        // stands for QUASAR_ERUPT_MIN_WARNING.
        quasarEruptRingGap = QUASAR_ERUPT_RING_GAP;
        if (rings > 1)
            quasarEruptRingGap = std::min(QUASAR_ERUPT_RING_GAP,
                (QUASAR_ERUPT_BLAST_TIME - QUASAR_ERUPT_MIN_WARNING - QUASAR_ERUPT_WINDUP -
                 QUASAR_ERUPT_RING_GROW) / (float)(rings - 1));
    }

    // When ring k starts to unfold.
    float QuasarEruptShowTime(int ring) const {
        return QUASAR_ERUPT_WINDUP + quasarEruptRingGap * (float)ring;
    }

    // When zone i starts to show / goes off (beat 0: its ring's time and the first
    // blast; beat 1: QUASAR_ERUPT_SECOND_SHOW and the second blast).
    float QuasarEruptZoneShow(int i) const {
        return quasarEruptBeat[i] ? QUASAR_ERUPT_SECOND_SHOW : QuasarEruptShowTime(quasarEruptRing[i]);
    }
    float QuasarEruptZoneBlast(int i) const {
        return quasarEruptBeat[i] ? QUASAR_ERUPT_SECOND_BLAST : QUASAR_ERUPT_BLAST_TIME;
    }
    float QuasarEruptZoneRadius(int i) const {
        return quasarEruptRing[i] == QUASAR_ERUPT_CENTER_RING ? QUASAR_ERUPT_CENTER_RADIUS : QUASAR_ERUPT_ZONE_RADIUS;
    }

    // beat: 0 or 1 to test one beat only, -1 for either.
    bool QuasarEruptCovers(const glm::vec2& p, int beat = -1) const {
        for (int i = 0; i < quasarEruptCount; ++i) {
            if (beat >= 0 && quasarEruptBeat[i] != beat) continue;
            const float Z = QuasarEruptZoneRadius(i);
            const glm::vec2 d = p - quasarEruptZones[i];
            if (glm::dot(d, d) <= Z * Z) return true;
        }
        return false;
    }

    // Each beat's zones go off together: one hit per beat however many overlap.
    void UpdateQuasarEruption(float playerX, float playerY, float& playerHP) {
        if (!quasarEruptBlasted && quasarEruptTimer >= QUASAR_ERUPT_BLAST_TIME) {
            quasarEruptBlasted = true;
            if (QuasarEruptCovers(glm::vec2(playerX, playerY), 0))
                HurtPlayer(playerHP, QUASAR_ERUPT_DAMAGE, "QUASAR_ERUPT");
        }
        if (!quasarEruptBlasted2 && quasarEruptTimer >= QUASAR_ERUPT_SECOND_BLAST) {
            quasarEruptBlasted2 = true;
            if (QuasarEruptCovers(glm::vec2(playerX, playerY), 1))
                HurtPlayer(playerHP, QUASAR_ERUPT_DAMAGE, "QUASAR_ERUPT");
        }
        if (quasarEruptBlasted2 && quasarEruptTimer >= QUASAR_ERUPT_SECOND_BLAST + QUASAR_FALL_IMPACT_FX) {
            quasarEruptActive = false;
            quasarNearTimer = 0.0f;
            quasarEruptCooldown = QUASAR_ERUPT_COOLDOWN;
        }
    }

    void BeginQuasarFall() {
        quasarState = QUASAR_FALL_STATE;
        quasarTimer = 0.0f;
        quasarAimSet = false;
        quasarFallFired = 0;
        quasarFallNextFire = QUASAR_FALL_SPLIT;
        quasarFallHitUntil = 0.0f;
        quasarFallTargetsMask = quasarFallImpacts = 0;
        quasarFallPredictedMask = 0;
    }

    // Each shot samples the player's current location once. Most land near
    // the player; a third scatter farther out to interrupt a single easy path.
    void LaunchQuasarFall(float playerX, float playerY, const glm::vec4& bounds) {
        const int i = quasarFallFired++;
        quasarFallOrigins[i] = QuasarFlangeNode(i % 4);
        quasarFallLaunchTimes[i] = quasarTimer;
        const bool predicted = (i % QUASAR_PREDICT_EVERY) == QUASAR_PREDICT_EVERY - 1;
        glm::vec2 target;
        if (predicted) {
            // Aimed shot: ahead of the player along their direction of travel.
            target = glm::vec2(playerX, playerY) + QuasarPredictLead();
            quasarFallPredictedMask |= 1u << i;
        } else {
            const float angle = (float)(rand() % 10000) * 0.0006283185f;
            const float u = (float)(rand() % 10000) / 10000.0f;
            const bool wide = rand() % 3 == 0;
            const float distance = std::sqrt(wide ? 8100.0f + u * 24300.0f : u * 8100.0f);
            target = glm::vec2(playerX, playerY) + glm::vec2(cosf(angle), sinf(angle)) * distance;
        }
        const float r = QUASAR_FALL_RADIUS;
        if (bounds.z > bounds.x && bounds.w > bounds.y) {
            if (bounds.z - bounds.x < 2.0f * r || bounds.w - bounds.y < 2.0f * r) return;
            target.x = std::clamp(target.x, bounds.x + r, bounds.z - r);
            target.y = std::clamp(target.y, bounds.y + r, bounds.w - r);
        }
        quasarFallTargets[i] = target;
        // Farther from the Quasar = wider and harder hitting.
        quasarFallPower[i] = std::clamp(
            (glm::length(target - glm::vec2(worldX, worldY)) - QUASAR_FALL_NEAR_DIST) /
                (QUASAR_FALL_FAR_DIST - QUASAR_FALL_NEAR_DIST), 0.0f, 1.0f);
        const glm::vec2 origin = quasarFallOrigins[i];
        const glm::vec2 d = target - origin;
        const float distanceToTarget = glm::length(d);
        const glm::vec2 forward = distanceToTarget > 0.001f ? d / distanceToTarget : glm::vec2(1,0);
        const glm::vec2 side(-forward.y, forward.x);
        const float turn = (i & 1) ? -1.0f : 1.0f;
        const float arc = std::clamp(distanceToTarget * 0.55f, 90.0f, 290.0f);
        // A single bowed path, with a tangential launch and curved final approach.
        const glm::vec2 radial = origin - glm::vec2(worldX, worldY);
        const float radialLength = glm::length(radial);
        const glm::vec2 tangent = radialLength > 0.001f
            ? glm::vec2(radial.y, -radial.x) / radialLength : side;
        quasarFallControl1[i] = origin + tangent * arc + side * (turn * arc * 0.45f);
        quasarFallControl2[i] = target - forward * (distanceToTarget * 0.18f) + side * (turn * arc);
        quasarFallTargetsMask |= 1u << i;
    }

    // Continuous bowed trajectory. Its nose follows the tangent, with a
    // separate tumble in drawing. Ground telegraphs remain fixed after launch.
    glm::vec2 QuasarFallPosition(int i, float age) const {
        const float t = std::clamp((age - quasarFallLaunchTimes[i]) / QuasarFallWarning(i), 0.0f, 1.0f);
        const float u = 0.25f * t + 0.75f * t * t;
        const float v = 1.0f - u;
        return quasarFallOrigins[i] * (v * v * v) + quasarFallControl1[i] * (3 * v * v * u) +
               quasarFallControl2[i] * (3 * v * u * u) + quasarFallTargets[i] * (u * u * u);
    }

    void UpdateQuasarFall(float playerX, float playerY, float& playerHP,
                          const glm::vec4& bounds) {
        if (quasarFallFired < QUASAR_FALL_COUNT && quasarTimer + 0.00001f >= quasarFallNextFire) {
            LaunchQuasarFall(playerX, playerY, bounds);
            // No burst of overdue shots after a hitch: every shot still gets
            // its own full warning and a minimum firing gap.
            quasarFallNextFire = quasarTimer + QuasarFallNextGap();
        }
        for (int i = 0; i < quasarFallFired; ++i) {
            const unsigned bit = 1u << i;
            if (!(quasarFallTargetsMask & bit) || (quasarFallImpacts & bit) ||
                quasarTimer < QuasarFallImpactTime(i)) continue;
            quasarFallImpacts |= bit;
            const glm::vec2 d = glm::vec2(playerX, playerY) - quasarFallTargets[i];
            if (quasarTimer >= quasarFallHitUntil &&
                glm::dot(d, d) <= QuasarFallRadius(i) * QuasarFallRadius(i)) {
                HurtPlayer(playerHP, QuasarFallDamage(i), "QUASAR_FALL");
                quasarFallHitUntil = quasarTimer + QUASAR_FALL_HIT_GRACE;
            }
        }
        if (quasarFallFired == QUASAR_FALL_COUNT &&
            quasarTimer >= QuasarFallImpactTime(QUASAR_FALL_COUNT - 1) + QUASAR_FALL_IMPACT_FX) {
            quasarState = QUASAR_FALL_RECOVER_STATE;
            quasarTimer = 0.0f;
            quasarFallTargetsMask = 0;
        }
    }

    // ---- Gravis ------------------------------------------------------------
    // No pattern of its own: the body drifts and the standing field
    // (GravisField) does the work.
    void UpdateGravis(float deltaTime, float speedMult) {
        gravisVisualAngle += deltaTime * 0.72f;
        gravisOrbitPhase += deltaTime * 1.35f;
        gravisDriftTimer -= deltaTime;
        if (gravisDriftTimer <= 0.0f) {
            gravisDriftAngle += ((float)(rand() % 101) - 50.0f) * 0.012f;
            gravisDriftTimer = 1.2f + (float)(rand() % 140) * 0.01f;
        }
        worldX += cosf(gravisDriftAngle) * speed * speedMult * deltaTime;
        worldY += sinf(gravisDriftAngle) * speed * speedMult * deltaTime;
    }

    // Shell stacks as a continuous 0..3: shell k is full once this reaches
    // k + 1. They charge while tracking, stay full through the aim, empty one
    // per throw and are empty while regrouping.
    float GenesisStackFill() const {
        if(hivePhase==0 && genesisNextRotor) return 0.0f;
        switch (hivePhase) {
        case 0: return std::clamp(spawnTimer / GENESIS_STACK_TIME, 0.0f, (float)GENESIS_STACKS);
        case 1: return (float)GENESIS_STACKS;
        case 2: return (float)std::max(0, GENESIS_STACKS - hiveSpawnCount);
        default: return 0.0f;
        }
    }
    int GenesisStacks() const { return (int)GenesisStackFill(); }

    // Seconds since throw `shot` (0..2) left the core, or -1 if it has not.
    float GenesisThrowAge(int shot) const {
        if (hivePhase == 2) {
            if (shot >= hiveSpawnCount) return -1.0f;
            return spawnTimer + (float)(hiveSpawnCount - 1 - shot) * GENESIS_THROW_GAP;
        }
        if (hivePhase == 3)
            return GENESIS_THROW_END + spawnTimer +
                   (float)(GENESIS_STACKS - 1 - shot) * GENESIS_THROW_GAP;
        return -1.0f;
    }

    // Time to cover `fraction` (0..1) of a `dist` px throw under the
    // three-stage speed profile.
    static float ThrownFlightTime(float dist, float fraction) {
        const float d1 = dist * GENESIS_THROW_SPLIT_MID;
        const float d2 = dist * (GENESIS_THROW_SPLIT_LAND - GENESIS_THROW_SPLIT_MID);
        const float d = dist * std::clamp(fraction, 0.0f, 1.0f);
        if (d <= d1) return d / GENESIS_THROW_SPEED_LAUNCH;
        float t = d1 / GENESIS_THROW_SPEED_LAUNCH;
        if (d <= d1 + d2) return t + (d - d1) / GENESIS_THROW_SPEED_MID;
        t += d2 / GENESIS_THROW_SPEED_MID;
        return t + (d - d1 - d2) / GENESIS_THROW_SPEED_LAND;
    }

    // Share of the distance covered after `t` seconds (inverse of the above).
    static float ThrownFlightFraction(float dist, float t) {
        if (dist <= 0.001f) return 1.0f;
        const float t1 = ThrownFlightTime(dist, GENESIS_THROW_SPLIT_MID);
        const float t2 = ThrownFlightTime(dist, GENESIS_THROW_SPLIT_LAND);
        float d;
        if (t <= t1) d = t * GENESIS_THROW_SPEED_LAUNCH;
        else if (t <= t2) d = dist * GENESIS_THROW_SPLIT_MID + (t - t1) * GENESIS_THROW_SPEED_MID;
        else d = dist * GENESIS_THROW_SPLIT_LAND + (t - t2) * GENESIS_THROW_SPEED_LAND;
        return std::min(1.0f, d / dist);
    }

    static float GenesisSmooth(float u) {
        u = std::clamp(u, 0.0f, 1.0f); return u*u*(3.0f-2.0f*u);
    }
    bool GenesisRotorWaiting() const { return hivePhase==0 && genesisNextRotor; }
    bool GenesisRotorMode() const { return GenesisRotorWaiting() || hivePhase>=4; }
    float GenesisRotorPose() const {
        // Finish assembly early, then hold the readable ready pose before feed.
        if (hivePhase == 4) return GenesisSmooth((spawnTimer / GENESIS_ROTOR_PREP-0.12f)/0.58f);
        if (hivePhase == 5) return 1.0f;
        if (hivePhase == 6) return 1.0f-GenesisSmooth(spawnTimer/GENESIS_ROTOR_RECOVER);
        return 0.0f;
    }
    float GenesisRotorKick() const {
        if (genesisRotorShotAge < 0.03f) return GenesisSmooth(genesisRotorShotAge/0.03f);
        return 1.0f-GenesisSmooth((genesisRotorShotAge-0.03f)/0.25f);
    }
    float GenesisRotorWindup() const {
        if(hivePhase!=4) return 0.0f;
        const float u=std::clamp(spawnTimer/(GENESIS_ROTOR_PREP*0.7f),0.0f,1.0f);
        return sinf(3.14159265f*GenesisSmooth(u));
    }
    glm::vec2 GenesisRotorCenter() const {
        const float r = 18.0f*sizeScale*2.05f;
        const glm::vec2 aim(cosf(hiveOrbitAngle),sinf(hiveOrbitAngle));
        return glm::vec2(worldX,worldY)-genesisRotorRecoilDir*(r*0.32f*GenesisRotorKick())-
               aim*(r*0.10f*GenesisRotorWindup());
    }
    glm::vec2 GenesisRotorFeedPoint(float u) const {
        const float r = 18.0f*sizeScale*2.05f;
        // One real unit forms inside the body and accelerates toward the exit.
        return GenesisRotorCenter()+glm::vec2(cosf(hiveOrbitAngle),sinf(hiveOrbitAngle))*
               (r*(-0.35f+1.70f*u*u));
    }
    float GenesisRotorAperture() const {
        const float r=18.0f*sizeScale*2.05f;
        const float shellGap=2.0f*r*0.90f*sinf(1.0471976f-(0.9076f-0.40f));
        return std::min(shellGap,r*0.96f);
    }
    float GenesisRotorOutputScale() const {
        // Include the square's diagonal AND corner nodes, at any rotation.
        const float envelope=18.0f*(2.0f*0.78f*1.4142136f+0.32f);
        return std::clamp((GenesisRotorAperture()-12.0f)/envelope,0.10f,1.0f);
    }
    static float GenesisRotorFormScale(float full,float u) {
        return full*(0.15f+0.85f*GenesisSmooth(u));
    }
    void LaunchGenesisRotor(float px,float py,float aimAngle) {
        genesisRotorPhase=2; thrownTimer=0; genesisRotorDetonated=false;
        thrownStartX=worldX; thrownStartY=worldY;
        const float dx=px-worldX,dy=py-worldY;
        // No gameplay range clamp: every shot lands at its release-time target.
        thrownDistance=std::sqrt(dx*dx+dy*dy);
        thrownHeading=thrownDistance>0.00001f ? atan2f(dy,dx) : aimAngle;
        thrownSpin=thrownHeading;
        thrownTargetX=px; thrownTargetY=py;
        thrownDuration=std::max(0.00001f,1.2f*thrownDistance/GENESIS_ROTOR_SPEED);
        for(auto& p:antaresTrail) p=glm::vec3(worldX,worldY,thrownSpin);
        antaresTrailTimer=0;
    }
    void UpdateGenesisRotor(float dt,float px,float py,float& hp) {
        const glm::vec2 before(worldX,worldY);
        thrownTimer+=dt;
        if(genesisRotorPhase==2) {
            const float fastTime=0.8f*thrownDistance/GENESIS_ROTOR_SPEED;
            float distance;
            if(thrownTimer<=fastTime) distance=thrownTimer*GENESIS_ROTOR_SPEED;
            else {
                const float u=std::clamp((thrownTimer-fastTime)/(thrownDuration-fastTime),0.0f,1.0f);
                distance=thrownDistance*(0.8f+0.4f*(u-0.5f*u*u));
            }
            worldX=thrownStartX+cosf(thrownHeading)*distance;
            worldY=thrownStartY+sinf(thrownHeading)*distance;
            const glm::vec2 step=glm::vec2(worldX,worldY)-before;
            // Continuous angle; spin slows with actual translation on landing.
            thrownSpin+=glm::length(step)*0.006f;
            if(dt>0 && thrownTimer>=thrownDuration) {
                worldX=thrownTargetX; worldY=thrownTargetY;
                genesisRotorPhase=3; thrownTimer=0; genesisRotorDetonated=true;
                // One landing hit, never direct flight damage or repeated pulse damage.
                const float dx=px-worldX,dy=py-worldY;
                if(singularityGrace<=0 && dx*dx+dy*dy<GENESIS_ROTOR_BLAST_RADIUS*GENESIS_ROTOR_BLAST_RADIUS)
                    HurtPlayer(hp,GENESIS_ROTOR_BLAST_DAMAGE,"GENESIS_ROTOR_BLAST");
            }
        } else {
            if(genesisNaturalScale>0.0f)                // a random unit settles into its own size on landing
                sizeScale=genesisRotorFullScale+(genesisNaturalScale-genesisRotorFullScale)*
                          GenesisSmooth(thrownTimer/0.25f);
            if(thrownTimer>=0.25f) {
                genesisRotorPhase=0;
                if(genesisNaturalScale>0.0f) sizeScale=genesisNaturalScale;
            }
        }
        antaresTrailTimer+=dt;
        if(antaresTrailTimer>=0.016f) {
            antaresTrailTimer=0;
            for(int i=ANTARES_TRAIL_COUNT-1;i>0;--i) antaresTrail[i]=antaresTrail[i-1];
            antaresTrail[0]=glm::vec3(worldX,worldY,thrownSpin);
        }
    }

    void BeginThrownFlight(float targetX, float targetY) {
        genesisThrown = true;
        thrownPhase = 1;
        thrownTimer = 0.0f;
        thrownStartX = worldX;
        thrownStartY = worldY;
        thrownTargetX = targetX;
        thrownTargetY = targetY;
        const float dx = targetX - worldX, dy = targetY - worldY;
        const float dist = std::sqrt(dx * dx + dy * dy);
        thrownDistance = dist;
        thrownDuration = std::max(0.001f, ThrownFlightTime(dist, 1.0f));
        thrownHeading = atan2f(dy, dx);
        thrownSpin = thrownHeading;
        for (auto& sample : antaresTrail)
            sample = glm::vec3(worldX, worldY, thrownSpin);
        antaresTrailTimer = 0.0f;
    }

    // Returns true when the Swarm vanished on the player (no reward).
    bool UpdateThrownSwarm(float deltaTime, float distance, float& playerHP) {
        thrownTimer += deltaTime;
        if (thrownPhase == 1) {
            const float prevU = ThrownFlightFraction(thrownDistance, thrownTimer - deltaTime);
            const float u = ThrownFlightFraction(thrownDistance, thrownTimer);
            thrownSpin += (u - prevU) * thrownDistance * THROWN_SWARM_SPIN_PER_PX;
            worldX = thrownStartX + (thrownTargetX - thrownStartX) * u;
            worldY = thrownStartY + (thrownTargetY - thrownStartY) * u;
            if (singularityGrace <= 0.0f && distance < 26.0f * sizeScale + 6.0f) {
                HurtPlayer(playerHP, THROWN_SWARM_DAMAGE, "THROWN_SWARM");
                alive = false;
                scored = true;      // vanished on impact: no kill reward
                noBlast = true;
                return true;
            }
            if (u >= 1.0f) {
                thrownPhase = 2;
                thrownTimer = 0.0f;
                // A miss is still dangerous: the landing sends out a shockwave.
                if (singularityGrace <= 0.0f && distance < THROWN_SWARM_BLAST_RADIUS)
                    HurtPlayer(playerHP, THROWN_SWARM_BLAST_DAMAGE, "THROWN_SWARM_BLAST");
            }
        } else if (thrownTimer >= THROWN_SWARM_STUN) {
            thrownPhase = 0;
        }
        // Keep sampling through the stun so the trail collapses onto the body.
        antaresTrailTimer += deltaTime;
        if (antaresTrailTimer >= 0.03f) {
            antaresTrailTimer = 0.0f;
            for (int i = ANTARES_TRAIL_COUNT - 1; i > 0; --i)
                antaresTrail[i] = antaresTrail[i - 1];
            antaresTrail[0] = glm::vec3(worldX, worldY, thrownSpin);
        }
        return false;
    }

    void BeginGenesisEgress(float dirX, float dirY) {
        const float length = std::sqrt(dirX * dirX + dirY * dirY);
        if (length <= 0.0001f) return;
        genesisEgressX = dirX / length;
        genesisEgressY = dirY / length;
        genesisEgressTimer = 0.0f;
        genesisEgress = true;
    }

    void TryContact(float distance, float& playerHP, float dps,
                    float deltaTime, float threshold = -1.0f,
                    float gateWX = -1.0f, float gateWY = -1.0f,
                    float gateWW = -1.0f, float gateWH = -1.0f) const {
        if (singularityGrace > 0.0f) return;
        if (gateWW > 0.0f) {
            const float margin = 14.0f * sizeScale;
            if (worldX < gateWX - margin || worldX > gateWX + gateWW + margin ||
                worldY < gateWY - margin || worldY > gateWY + gateWH + margin)
                return;
        }
        const float contactRange = threshold >= 0.0f
            ? threshold : 26.0f * sizeScale;
        if (distance < contactRange)
            HurtPlayer(playerHP, dps * deltaTime, MobKindName(kind));
    }

    // TRIANGULUM: walks at the player and does nothing else (TriangulumMob.h).
    void UpdateTriangulum(float dx, float dy, float distance, float deltaTime, float speedMult) {
        triangulumSpin += deltaTime * triangulum::kOrbitSpeed;
        if (distance > 24.0f) {
            worldX += dx / distance * speed * speedMult * deltaTime;
            worldY += dy / distance * speed * speedMult * deltaTime;
        }
    }

    // MIRA: walks at the player; its own zone (MiraMob.h) is the only danger.
    void UpdateMira(float dx, float dy, float distance, float deltaTime, float& playerHP, float speedMult) {
        if (!miraChain) miraChain = mira::NewChain();
        mira::SetHead(miraChain, worldX, worldY);
        if (!miraChain) miraChain = mira::NewChain();
        mira::SetHead(miraChain, worldX, worldY);
        if (distance > 24.0f) {
            const float step = speed * speedMult * deltaTime;
            worldX += dx / distance * step;
            worldY += dy / distance * step;
            miraTrailDist += step;
            if (miraTrailDist >= mira::kTrailSpacing) {   // a puddle that shrinks away behind it
                miraTrailDist = 0.0f;
                mira::AddTrail(worldX, worldY, miraChain);
            }
        }
        if (singularityGrace <= 0.0f && mira::InZone(worldX, worldY, worldX + dx, worldY + dy, g_PlayerSizeMult)) {
            HurtPlayer(playerHP, mira::kZoneDps * deltaTime, "MIRA_ZONE");
            mira::Touch();
        }
    }

    // Keeps its distance while resting, stands still to aim. The line tracks
    // the player, freezes, and the ray fires down it from the core.
    void UpdateLynx(float dx, float dy, float distance, float deltaTime,
                      float& playerHP, float speedMult,
                      float gateWX, float gateWY, float gateWW, float gateWH) {
        const auto turnToPlayer = [&](float rate) {
            const float turn = lynx::WrapAngle(atan2f(dy, dx) - lynxHeading);
            lynxHeading = lynx::WrapAngle(lynxHeading + std::clamp(turn, -rate * deltaTime, rate * deltaTime));
        };
        if (LynxHoldsPosition()) {
            worldX = lynxHoldX;
            worldY = lynxHoldY;
        }
        if (lynxInArena) lynxSnipeTimer += deltaTime;     // the rest starts once it has arrived
        switch (lynxSnipe) {
        case lynx::SnipeRest: {
            turnToPlayer(lynx::kRestTurnRate);
            lynxCharge = 0.0f;
            const float ux = dx / distance, uy = dy / distance;
            const float pace = speed * speedMult * deltaTime;
            if (lynxInArena) {
                if (distance > lynx::kFarRange) {
                    worldX += ux * pace;
                    worldY += uy * pace;
                } else if (distance < lynx::kNearRange) {
                    worldX -= ux * pace * lynx::kRetreatMult;
                    worldY -= uy * pace * lynx::kRetreatMult;
                }
            }
            if (lynxInArena && lynxSnipeTimer >= lynx::kRestTime) {
                lynxSnipe = lynx::SnipeAim;
                lynxSnipeTimer = 0.0f;
                lynxHoldX = worldX;
                lynxHoldY = worldY;
            }
            break;
        }
        case lynx::SnipeAim:
            turnToPlayer(lynx::kTurnRate);
            lynxCharge = lynxSnipeTimer / lynx::kAimTime;
            if (lynxSnipeTimer >= lynx::kAimTime) {
                lynxSnipe = lynx::SnipeLock;
                lynxSnipeTimer = 0.0f;
                lynxCharge = 1.0f;
            }
            break;
        case lynx::SnipeLock:
            if (lynxSnipeTimer >= lynx::kLockTime) {
                if (singularityGrace <= 0.0f && lynx::RayHits(worldX, worldY, lynxHeading, worldX + dx, worldY + dy, g_PlayerSizeMult))
                    HurtPlayer(playerHP, lynx::kRayDamage, "LYNX_RAY");
                lynxSnipe = lynx::SnipeFlash;
                lynxSnipeTimer = 0.0f;
            }
            break;
        case lynx::SnipeFlash:
            lynxCharge = 1.0f - lynxSnipeTimer / lynx::kFlashTime;
            if (lynxSnipeTimer >= lynx::kFlashTime) {
                lynxSnipe = lynx::SnipeRest;
                lynxSnipeTimer = 0.0f;
                lynxCharge = 0.0f;
            }
            break;
        }
        TryContact(distance, playerHP, contactDmg, deltaTime, lynx::kContactRange,
                   gateWX, gateWY, gateWW, gateWH);
    }

    // CAELUM (CaelumBoss.h): drifts at a distance like NORMA and lets the
    // pattern state machine in caelum::Step do the rest. It holds still while
    // phase-changing and while open (after each pattern).
    void UpdateCaelum(float dx, float dy, float distance, float deltaTime,
                      float& playerHP, float speedMult) {
        const float px = worldX + dx, py = worldY + dy;
        caelum::Step(deltaTime, hp, normaMaxHp, worldX, worldY, px, py, playerHP);
        normaPhase = caelum::g.phase;
        normaVisualAngle += deltaTime * (normaPhase == 2 ? 0.8f : 0.3f);
        if (caelum::g.invuln > 0.0f || caelum::g.exposed > 0.0f) {
            const float settle = std::exp(-8.0f * deltaTime);
            normaVelX *= settle;
            normaVelY *= settle;
            worldX += normaVelX * deltaTime;
            worldY += normaVelY * deltaTime;
            return;
        }
        if (worldY < 130.0f) {          // coming in from above the screen
            worldY += 220.0f * deltaTime;
            return;
        }
        // Dragged by its cursor: exactly where the rules put it.
        if (caelum::g.bodyExact) {
            // Its velocity still tracks the drag (eased), for the pixels it sheds.
            if (deltaTime > 0.0f) {
                const float k = std::min(1.0f, 10.0f * deltaTime);
                normaVelX += ((caelum::g.bodyX - worldX) / deltaTime - normaVelX) * k;
                normaVelY += ((caelum::g.bodyY - worldY) / deltaTime - normaVelY) * k;
            }
            worldX = caelum::g.bodyX; worldY = caelum::g.bodyY;
            TryContact(distance, playerHP, contactDmg, deltaTime);
            return;
        }
        // Otherwise it glides to the cell the rules picked (or holds still).
        float wantX = 0.0f, wantY = 0.0f;
        if (!caelum::g.bodyHold) {
            const float mx = caelum::g.moveX - worldX, my = caelum::g.moveY - worldY;
            const float md = std::sqrt(mx * mx + my * my);
            if (md > 1.0f) {
                const float sp = std::min(caelum::kMoveSpeed, md * 3.0f) * speedMult;
                wantX = mx / md * sp; wantY = my / md * sp;
            }
        }
        const float step = norma::kHoverAccel * deltaTime;
        const float ex = wantX - normaVelX, ey = wantY - normaVelY;
        const float el = std::sqrt(ex * ex + ey * ey);
        if (el > step) { normaVelX += ex / el * step; normaVelY += ey / el * step; }
        else { normaVelX = wantX; normaVelY = wantY; }
        worldX = std::clamp(worldX + normaVelX * deltaTime, caelum::kBodyMinX, caelum::g_W - caelum::kBodyMinX);
        worldY = std::clamp(worldY + normaVelY * deltaTime, caelum::kBodyMinY, caelum::g_H - caelum::kBodyMinY);
        TryContact(distance, playerHP, contactDmg, deltaTime);
    }

    // NORMA (NormaBoss.h): hovers at a distance, switches to phase 2 at
    // norma::kPhase2Ratio of its max HP, and runs its patterns in turn.
    void UpdateNorma(float dx, float dy, float distance, float deltaTime,
                     float& playerHP, float speedMult, std::vector<Bullet>* bullets) {
        using norma::Pattern;
        using norma::Stage;
        normaVisualAngle += deltaTime * (normaPhase == 2 ? 0.9f : 0.35f);
        TurnNormaCube(deltaTime);
        // A health star goes dark each eighth of HP lost.
        normaStarFlash = std::max(0.0f, normaStarFlash - deltaTime);
        const int lit = norma::LitStarsFor(hp, normaMaxHp);
        if (lit < normaLitStars) {
            normaLitStars = lit;
            normaStarFlash = norma::kStarOutFlash;
            norma::g_Events.push_back({ norma::Fx::StarOut, worldX, worldY, 0.0f });
        }
        {
            const bool panes =
                (normaPattern == Pattern::Ram && normaStage != Stage::Recover && normaExposed <= 0.0f);
            normaRingShow += ((panes ? 1.0f : 0.0f) - normaRingShow) * std::min(1.0f, 6.0f * deltaTime);
        }
        normaPhaseFlash = std::max(0.0f, normaPhaseFlash - deltaTime);
        if (normaPhase == 1 && norma::PhaseFor(hp, normaMaxHp) == 2) {
            normaPhase = 2;
            // Phase 2 is a fresh bar: damage past the first bar's end is dropped.
            hp = normaMaxHp * norma::kPhase2Ratio;
            normaPhaseFlash = norma::kPhaseFlashSeconds;
            norma::g_Events.push_back({ norma::Fx::PhaseShift, worldX, worldY, 0.0f });
            // Whatever it was doing ends: every pane blows, it holds still,
            // then new panes spray open (the game loop serves both Asks).
            normaHidden = false;
            normaExposed = 0.0f;
            normaInvuln = norma::kPhaseInvuln;
            normaPattern = Pattern::None;
            normaStage = Stage::Recover;
            normaTimer = 0.0f;
            norma::g_Asks.push_back(norma::Ask::PhaseBlow);
        }
        if (normaInvuln > 0.0f) {
            normaInvuln = std::max(0.0f, normaInvuln - deltaTime);
            if (normaInvuln <= 0.0f) {
                norma::g_Asks.push_back(norma::Ask::PhaseSpray);
                norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 1.4f });
            }
            // Holds still meanwhile: the drift eases to a stop.
            const float settle = std::exp(-8.0f * deltaTime);
            normaVelX *= settle;
            normaVelY *= settle;
            worldX += normaVelX * deltaTime;
            worldY += normaVelY * deltaTime;
            normaPhaseFlash = std::max(normaPhaseFlash, 0.05f);
            return;
        }
        {
            const float hideTo = normaHidden ? 1.0f
                : (normaPattern == Pattern::Warp && normaStage == Stage::Telegraph)
                  ? std::clamp(normaTimer / norma::kWarpEnter, 0.0f, 1.0f) : 0.0f;
            normaHide += (hideTo - normaHide) * std::min(1.0f, 14.0f * deltaTime);
        }
        normaLastPlayerAngle = atan2f(dy, dx);
        normaRecoil = std::max(0.0f, normaRecoil - deltaTime);
        if (normaExposed > 0.0f) {
            // An opening: everything holds while it lasts, and the still body
            // is safe to stand against (no contact damage). Any drift in
            // flight eases to a stop instead of freezing in place.
            const float settle = std::exp(-8.0f * deltaTime);
            normaVelX *= settle;
            normaVelY *= settle;
            worldX += normaVelX * deltaTime;
            worldY += normaVelY * deltaTime;
            normaExposed = std::max(0.0f, normaExposed - deltaTime);
            normaGhostLife = std::max(0.0f, normaGhostLife - deltaTime);
            return;
        }
        normaTimer += deltaTime;
        normaGhostLife = std::max(0.0f, normaGhostLife - deltaTime);
        // Whether anything this frame took charge of the body's motion; if
        // not, its last drift coasts to a stop after the switch below.
        bool moved = false;
        const auto hover = [&]() {
            moved = true;
            const float want = norma::HoverSpeedSmooth(distance) * speedMult;
            const float wantX = distance > 1.0f ? dx / distance * want : 0.0f;
            const float wantY = distance > 1.0f ? dy / distance * want : 0.0f;
            const float step = norma::kHoverAccel * deltaTime;
            const float ex = wantX - normaVelX, ey = wantY - normaVelY;
            const float el = std::sqrt(ex * ex + ey * ey);
            if (el > step) { normaVelX += ex / el * step; normaVelY += ey / el * step; }
            else { normaVelX = wantX; normaVelY = wantY; }
            worldX += normaVelX * deltaTime;
            worldY += normaVelY * deltaTime;
        };
        const auto enter = [&](Stage stage) {
            normaStage = stage;
            normaTimer = 0.0f;
            normaFireTimer = 0.0f;
        };
        // Afterimages while it really travels.
        const auto leaveGhosts = [&](float stepX, float stepY) {
            if (stepX * stepX + stepY * stepY <= 1.0f) return;
            normaGhostLife = 0.25f;
            normaGhostTimer -= deltaTime;
            if (normaGhostTimer <= 0.0f) {
                normaGhostTimer = norma::kGhostSpacing;
                normaGhostHead = (normaGhostHead + 1) % norma::kGhostCount;
                normaGhostX[normaGhostHead] = worldX;
                normaGhostY[normaGhostHead] = worldY;
            }
        };
        const auto glideTo = [&](float x, float y, float rate) {
            const float k = std::min(1.0f, rate * deltaTime);
            const float stepX = (x - worldX) * k, stepY = (y - worldY) * k;
            worldX += stepX;
            worldY += stepY;
            moved = true;
            // Hand the glide's speed on, so the hover after it starts from
            // there rather than from a standstill.
            normaVelX = std::clamp(stepX / deltaTime, -300.0f, 300.0f);
            normaVelY = std::clamp(stepY / deltaTime, -300.0f, 300.0f);
            leaveGhosts(stepX, stepY);
        };

        switch (normaStage) {
        case Stage::Recover:
            hover();
            if (normaTimer >= (normaPhase == 2 ? norma::kRecoverPhase2 : norma::kRecoverSeconds))
                StartNormaPattern();
            break;
        case Stage::Wait:
            // The game loop answers within a frame or two.
            hover();
            if (normaPaneId > 0) {
                enter(Stage::Telegraph);
            } else if (normaPaneId < 0 || normaTimer > 0.5f) {
                // (An unanswered request counts as none, so the boss never stalls.)
                // No pane to be had (spent by rams, closed by the player, or
                // a Wall already up): blow what is left and open fresh ones
                // instead, so the fallback is not a free extra barrage.
                normaPattern = Pattern::Reinforce;
                normaLastPattern = normaPattern;
                enter(Stage::Telegraph);
            }
            break;
        case Stage::Telegraph:
            if (normaPattern == Pattern::Aim) {
                TurnNormaAim(atan2f(dy, dx),
                             norma::kAimTrackTelegraph * deltaTime);
                if (normaTimer >= norma::kAimTelegraph) {
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.5f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Throw) {
                // The box grows out of the boss and fills; then it is hurled.
                if (normaTimer >= norma::kThrowAppear + norma::kThrowFill) {
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.7f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Detonate) {
                // The panes in use are fusing; the boss waits out the blasts.
                hover();
                if (normaTimer >= norma::kDetonateFuseWait) {
                    norma::g_Asks.push_back(norma::Ask::ReopenPanes);
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.8f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Crash) {
                // The storm opens (the game loop); the boss drifts to a stop.
                if (normaTimer >= norma::kCrashBuild) enter(Stage::Act);
            } else if (normaPattern == Pattern::Reinforce) {
                hover();
                if (normaTimer >= norma::kReinforceTelegraph) {
                    norma::g_Asks.push_back(norma::Ask::ReopenPanes);
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.8f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Refuge) {
                if (normaTimer >= norma::kRefugeTelegraph) {
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 1.4f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Ram) {
                // The game loop draws the pane back and aims it.
                hover();
                if (normaTimer >= norma::RamAimFor(normaPhase)) {
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.9f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Warp) {
                // Slips into the pane the game loop picked.
                glideTo(normaWarpSrcX, normaWarpSrcY, 7.0f);
                if (normaTimer >= norma::kWarpEnter) {
                    worldX = normaWarpSrcX;
                    worldY = normaWarpSrcY;
                    normaHidden = true;
                    normaHide = 1.0f;
                    normaVelX = normaVelY = 0.0f;
                    norma::g_Events.push_back({ norma::Fx::EnterPane, worldX, worldY, 0.0f });
                    enter(Stage::Act);
                }
            } else if (normaPattern == Pattern::Wall) {
                // The game loop stands the panes in a line, one by one.
                hover();
                if (normaTimer >= norma::kWallBuildSeconds) {
                    norma::g_Events.push_back({ norma::Fx::Burst, worldX, worldY, 0.9f });
                    enter(Stage::Act);
                }
            } else {   // CloseWindow: the game loop runs the countdown
                hover();
                if (normaTimer >= norma::kCloseCountdown + 0.3f) enter(Stage::Recover);
            }
            break;
        case Stage::Act:
            if (normaPattern == Pattern::Aim) {
                TurnNormaAim(atan2f(dy, dx),
                             norma::kAimTrackRate * deltaTime);
                normaFireTimer += deltaTime;
                while (normaFireTimer >= norma::kAimStreamInterval) {
                    normaFireTimer -= norma::kAimStreamInterval;
                    // Centre, left, centre, right: every other shot runs dead on
                    // the aim, so a player who stands still is hit (an even
                    // +/- split left a gap round them, like a double shot).
                    static constexpr float kSpread[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
                    const float jitter = kSpread[normaAimShot++ % 4] * norma::kAimJitter;
                    // From the muzzle star, the star point the body holds on the aim.
                    const float muzzle = 18.0f * sizeScale * norma::kAimMuzzle;
                    FireNormaShot(bullets, normaAimAngle, norma::kAimStreamSpeed, muzzle, jitter);
                    normaRecoil = norma::kRecoilSeconds * 0.6f;
                    norma::g_Events.push_back({ norma::Fx::Muzzle, worldX + cosf(normaAimAngle) * muzzle,
                                                worldY + sinf(normaAimAngle) * muzzle, normaAimAngle });
                }
                if (normaTimer >= norma::kAimSeconds) enter(Stage::Recover);
            } else if (normaPattern == Pattern::Throw) {
                // The game loop flies and bursts the box; the boss holds fire.
                hover();
                if (normaTimer >= norma::kThrowSeconds) enter(Stage::Recover);
            } else if (normaPattern == Pattern::Detonate || normaPattern == Pattern::Reinforce) {
                // New panes snap open (the game loop); then back to business.
                hover();
                if (normaTimer >= norma::kDetonateReopen) enter(Stage::Recover);
            } else if (normaPattern == Pattern::Warp) {
                // Gone. Then the game loop smashes the other pane, puts the
                // boss in its place, and the boss is left open.
                moved = true;
                normaVelX = normaVelY = 0.0f;
                if (normaTimer >= norma::kWarpHide) {
                    normaHidden = false;
                    normaHide = 0.0f;
                    norma::g_Asks.push_back(norma::Ask::WarpSmash);
                    normaExposed = norma::kWarpExpose;
                    enter(Stage::Recover);
                }
            } else if (normaPattern == Pattern::Crash) {
                // The countdown and the blue screen are the game loop's (in
                // real time, through its hit-stop); the boss turns open as the
                // screen shatters.
                if (norma::g_CrashOver || normaTimer >= norma::kCrashCount + 8.0f) {
                    norma::g_CrashOver = false;
                    normaExposed = norma::kCrashExpose;
                    enter(Stage::Recover);
                }
            } else if (normaPattern == Pattern::Wall) {
                // The game loop pushes the wall; the boss holds fire.
                hover();
                if (normaTimer >= norma::kWallSeconds) enter(Stage::Recover);
            } else if (normaPattern == Pattern::Ram) {
                // The game loop flies the pane; the boss holds fire.
                hover();
                if (normaTimer >= norma::kRamSeconds) enter(Stage::Recover);
            } else if (normaPattern == Pattern::Refuge) {
                normaFireTimer += deltaTime;
                while (normaFireTimer >= norma::kRefugeWaveInterval) {
                    normaFireTimer -= norma::kRefugeWaveInterval;
                    const float offset = normaSpin;
                    normaSpin += 0.5f * 6.2831853f / (float)norma::kRefugeShots;
                    for (int i = 0; i < norma::kRefugeShots; ++i)
                        FireNormaShot(bullets, offset + 6.2831853f * (float)i /
                                      (float)norma::kRefugeShots, norma::kRefugeShotSpeed);
                    // Every other wave carries a slower inner ring on the gaps.
                    if (++normaWaveIndex % 2 == 0)
                        for (int i = 0; i < norma::kRefugeShots / 2; ++i)
                            FireNormaShot(bullets, offset + 6.2831853f * ((float)i + 0.5f) /
                                          (float)(norma::kRefugeShots / 2),
                                          norma::kRefugeShotSpeed * 0.62f);
                    norma::g_Events.push_back({ norma::Fx::Wave, worldX, worldY,
                                                norma::kRefugeShotSpeed });
                    normaRecoil = norma::kRecoilSeconds * 1.4f;
                }
                if (normaTimer >= norma::kRefugeSeconds) enter(Stage::Exit);
            }
            break;
        case Stage::Exit:
            if (normaTimer >= norma::kRefugeLinger) {
                norma::g_Asks.push_back(norma::Ask::ReleaseWindows);
                enter(Stage::Recover);
            }
            break;
        }
        if (!moved) {
            const float coast = std::exp(-norma::kCoastDecay * deltaTime);
            normaVelX *= coast;
            normaVelY *= coast;
            worldX += normaVelX * deltaTime;
            worldY += normaVelY * deltaTime;
        }
        if (!normaHidden) TryContact(distance, playerHP, contactDmg, deltaTime);
    }

    // forced: a pattern to run instead of the next in turn (smoke captures).
    void StartNormaPattern(norma::Pattern forced = norma::Pattern::None) {
        using norma::Pattern;
        normaPattern = forced != Pattern::None ? forced
                     : norma::NextPattern(normaCount++, normaPhase, rand(), normaLastPattern);
        normaLastPattern = normaPattern;
        normaTimer = 0.0f;
        normaFireTimer = 0.0f;
        normaPaneId = 0;
        switch (normaPattern) {
        case Pattern::Detonate:
            norma::g_Asks.push_back(norma::Ask::DetonatePanes);
            normaStage = norma::Stage::Telegraph;
            break;
        case Pattern::Reinforce:
            normaStage = norma::Stage::Telegraph;
            break;
        case Pattern::Refuge:
            norma::g_Asks.push_back(norma::Ask::OpenRefuges);
            normaStage = norma::Stage::Telegraph;
            break;
        case Pattern::CloseWindow:
            norma::g_Asks.push_back(norma::Ask::ClosePlayerWindow);
            normaStage = norma::Stage::Wait;
            break;
        case Pattern::Throw:
            norma::g_Asks.push_back(norma::Ask::ThrowBox);
            normaStage = norma::Stage::Telegraph;
            break;
        case Pattern::Warp:
            norma::g_Asks.push_back(norma::Ask::WarpPlan);
            normaStage = norma::Stage::Wait;
            break;
        case Pattern::Crash:
            norma::g_Asks.push_back(norma::Ask::CrashStart);
            normaStage = norma::Stage::Telegraph;
            break;
        case Pattern::Wall:
            norma::g_Asks.push_back(norma::Ask::BuildWall);
            normaStage = norma::Stage::Wait;   // refused while the last wall still stands
            break;
        case Pattern::Ram:
            norma::g_Asks.push_back(norma::Ask::RamWindow);
            normaStage = norma::Stage::Wait;
            break;
        case Pattern::Aim:
            normaAimAngle = normaLastPlayerAngle;
            normaAimShot = 0;
            normaStage = norma::Stage::Telegraph;
            break;
        default:
            normaStage = norma::Stage::Telegraph;
            break;
        }
    }

    void TurnNormaCube(float deltaTime) {
        using norma::Pattern;
        using norma::Stage;
        const bool facing =
            normaPattern == Pattern::Aim && (normaStage == Stage::Telegraph || normaStage == Stage::Act);
        if (!facing) {
            normaCubeYaw += deltaTime * (normaPhase == 2 ? 0.9f : 0.35f);
            normaCubePitch = 0.55f + 0.25f * sinf(normaCubeYaw * 0.7f);
            return;
        }
        const float want = normaAimAngle;
        float sx[8], sy[8], reach = 0.0f;
        for (int i = 0; i < 8; ++i) {
            norma::CubeCornerOnScreen(i, normaCubeYaw, normaCubePitch, sx[i], sy[i]);
            reach = std::max(reach, sx[i] * sx[i] + sy[i] * sy[i]);
        }
        // The outline corner needing the least turn; the current one is kept
        // unless another is clearly closer, so the lead never flickers.
        auto turnFor = [&](int i) {
            const float d = want - (atan2f(sy[i], sx[i]) + normaCubeRoll);
            return atan2f(sinf(d), cosf(d));
        };
        int best = -1;
        float bestTurn = 0.0f;
        for (int i = 0; i < 8; ++i) {
            if (sx[i] * sx[i] + sy[i] * sy[i] < reach * 0.64f) continue;   // inner corners
            const float t = turnFor(i);
            const float bias = i == normaCubeLead ? 0.3f : 0.0f;
            if (best < 0 || std::fabs(t) - bias < std::fabs(bestTurn) - (best == normaCubeLead ? 0.3f : 0.0f)) {
                best = i;
                bestTurn = t;
            }
        }
        if (best < 0) return;
        normaCubeLead = best;
        normaCubeRoll += bestTurn * std::min(1.0f, 6.0f * deltaTime);
    }

    // Turns the aimed stream toward `target` by at most `maxStep` radians.
    void TurnNormaAim(float target, float maxStep) {
        float diff = target - normaAimAngle;
        while (diff > 3.14159265f) diff -= 6.28318531f;
        while (diff < -3.14159265f) diff += 6.28318531f;
        normaAimAngle += std::clamp(diff, -maxStep, maxStep);
    }

    // muzzle: how far out along `angle` the shot appears (world px).
    // Leaves `muzzle` px out along `angle`, heading `angle + spread`.
    void FireNormaShot(std::vector<Bullet>* bullets, float angle, float shotSpeed,
                       float muzzle = 0.0f, float spread = 0.0f) const {
        if (!bullets) return;
        const float ox = worldX + cosf(angle) * muzzle, oy = worldY + sinf(angle) * muzzle;
        const float heading = angle + spread;
        Bullet shot(ox, oy, ox + cosf(heading) * 100.0f, oy + sinf(heading) * 100.0f);
        shot.isEnemy = true;
        shot.homing = false;
        shot.speed = shotSpeed * (normaPhase == 2 ? norma::kPhase2ShotSpeed : 1.0f);
        shot.enemyDmg = norma::kShotDamage;
        shot.sizeScale = 1.05f;
        shot.color = glm::vec3(0.72f, 0.94f, 1.0f);
        shot.normaShot = true;
        bullets->push_back(shot);
    }

    // Four streams on the turning cross, one shot each per interval.
    void FireNormaStreams(std::vector<Bullet>* bullets, float interval, float shotSpeed,
                          float deltaTime) {
        normaFireTimer += deltaTime;
        while (normaFireTimer >= interval) {
            normaFireTimer -= interval;
            for (int k = 0; k < 4; ++k)
                FireNormaShot(bullets, normaSpin + 1.5707963f * (float)k, shotSpeed);
            normaRecoil = norma::kRecoilSeconds;
            norma::g_Events.push_back({ norma::Fx::Volley, worldX, worldY, normaSpin });
        }
    }

    void UpdateGimbal(float playerCX, float playerCY, float deltaTime,
                      float distance, float dx, float dy, float& playerHP,
                      float speedMult, std::vector<Bullet>* bullets,
                      float gateWX, float gateWY,
                      float gateWW, float gateWH) {
        static constexpr float IDLE_ROT = 0.40f;
        static constexpr float CHARGE_ROT_MAX = 9.0f;
        static constexpr float SHOT_DELAY = 0.08f;
        static constexpr float BURST_END = SHOT_DELAY + 0.35f;
        static constexpr float SHOT_COOLDOWN = 2.85f;

        if (!gimbalCharging && !gimbalBursting) {
            gimbalVisualAngle += IDLE_ROT * deltaTime;
            gimbalAttackTimer -= deltaTime;
            if (gimbalAttackTimer <= 0.0f) {
                gimbalCharging = true;
                gimbalChargeAngle = 0.0f;
                gimbalChargeSpeed = IDLE_ROT;
            }
        }
        if (gimbalCharging) {
            gimbalChargeSpeed += (CHARGE_ROT_MAX - gimbalChargeSpeed)
                               * 5.0f * deltaTime;
            const float da = gimbalChargeSpeed * deltaTime;
            gimbalVisualAngle += da;
            gimbalChargeAngle += da;
            if (gimbalChargeAngle >= GIMBAL_CHARGE_ROTATIONS * 6.2831853f) {
                gimbalCharging = false;
                gimbalBursting = true;
                gimbalBurstTimer = 0.0f;
                gimbalFired = false;
            }
        } else if (gimbalBursting) {
            gimbalBurstTimer += deltaTime;
            if (!gimbalFired && gimbalBurstTimer >= SHOT_DELAY) {
                if (bullets) {
                    Bullet shot(worldX, worldY, playerCX, playerCY);
                    if (genesisControlled) {                      // a controlled Gimbal aims where you will be
                        const glm::vec2 aim = GenesisShotAimPoint(playerCX, playerCY, 360.0f);
                        shot = Bullet(worldX, worldY, aim.x, aim.y);
                    }
                    shot.isEnemy = true;
                    shot.homing = false;
                    shot.speed = 360.0f;
                    shot.enemyDmg = 8.0f;
                    shot.sizeScale = 0.95f;
                    shot.color = glm::vec3(0.70f, 1.0f, 0.36f);
                    bullets->push_back(shot);
                }
                gimbalFired = true;
            }
            if (gimbalBurstTimer >= BURST_END) {
                gimbalBursting = false;
                gimbalBurstTimer = 0.0f;
                gimbalChargeAngle = 0.0f;
                gimbalChargeSpeed = IDLE_ROT;
                gimbalAttackTimer = SHOT_COOLDOWN + (float)(rand() % 70) * 0.01f;
            }
        }

        // Wanders on random headings regardless of distance (so a pack spreads
        // out instead of settling on one ring); now and then the new heading
        // leans toward the player so it does not hug the view edge. It shoots
        // from wherever it is, and eases off while charging.
        gimbalWanderTimer -= deltaTime;
        if (gimbalWanderTimer <= 0.0f) {
            gimbalWanderTimer = 1.2f + (float)(rand() % 160) * 0.01f;
            gimbalWanderAngle = (float)(rand() % 628) * 0.01f;
            if (rand() % 100 < 35 && distance > 1.0f)
                gimbalWanderAngle = atan2f(dy, dx) + ((float)(rand() % 141) - 70.0f) * 0.01f;
        }
        const float moveSpeed = std::min(speed * speedMult, 112.0f);
        const float rate = moveSpeed * (gimbalCharging ? 0.35f : 0.75f);
        worldX += cosf(gimbalWanderAngle) * rate * deltaTime;
        worldY += sinf(gimbalWanderAngle) * rate * deltaTime;
        TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                   gateWX, gateWY, gateWW, gateWH);
    }

    void UpdateAntares(float deltaTime, float distance, float dx, float dy,
                       float& playerHP, float speedMult,
                       float gateWX, float gateWY, float gateWW, float gateWH) {
        static constexpr float ACCEL = 128.0f;           // 60 -> 900 px/s in ~6.6s
        static constexpr float TURN_SLOW = 3.1416f;      // 180 deg/s at stage 1
        static constexpr float TURN_FAST = 0.6981f;      //  40 deg/s at top speed
        static constexpr float SPIN_SLOW = 0.5236f;      //  30 deg/s
        static constexpr float SPIN_FAST = 9.4248f;      // 540 deg/s
        static constexpr float PASS_RANGE = 320.0f;
        static constexpr float FLYOFF_RANGE = 700.0f;    // a wide miss also ends the run
        static constexpr float SKID_TIME = 0.45f;
        static constexpr float IMPACT_DAMAGE = 12.0f;    // at top speed
        static constexpr float KNOCK_PER_SPEED = 0.55f;  // 33px at 60, 495px at 900

        const float toX = dx / distance, toY = dy / distance;
        if (antaresHeadingX == 0.0f && antaresHeadingY == 0.0f) {
            antaresHeadingX = toX;
            antaresHeadingY = toY;
        }
        antaresStateTimer += deltaTime;
        const float facing = antaresHeadingX * toX + antaresHeadingY * toY;

        if (antaresState == AntaresState::CHASE) {
            const float ratio = AntaresSpeedRatio();
            // Faster means wider turns, so a sidestep makes it overshoot.
            const float current = atan2f(antaresHeadingY, antaresHeadingX);
            float turn = atan2f(toY, toX) - current;
            while (turn > 3.14159265f) turn -= 6.28318531f;
            while (turn < -3.14159265f) turn += 6.28318531f;
            const float maxTurn = (TURN_SLOW + (TURN_FAST - TURN_SLOW) * ratio)
                                * deltaTime;
            const float next = current + std::clamp(turn, -maxTurn, maxTurn);
            antaresHeadingX = cosf(next);
            antaresHeadingY = sinf(next);
            antaresMoveSpeed = std::min(ANTARES_MAX_SPEED,
                                        antaresMoveSpeed + ACCEL * deltaTime);

            // Touching the player is a ram at any speed: damage and knockback
            // scale with speed, and the impact stuns Antares instead of
            // letting the two bodies overlap.
            if (singularityGrace <= 0.0f && distance < AntaresRamRange()) {
                // Each split generation hits softer than the elite.
                HurtPlayer(playerHP, IMPACT_DAMAGE * antaresMoveSpeed /
                                     ANTARES_MAX_SPEED *
                                     powf(ANTARES_CHILD_SCALE, (float)antaresGen),
                           "ANTARES_RAM");
                const float knock = KNOCK_PER_SPEED * antaresMoveSpeed;
                antaresKnockX += toX * knock;
                antaresKnockY += toY * knock;
                antaresState = AntaresState::STUNNED;
                antaresStateTimer = 0.0f;
                antaresMoveSpeed = 0.0f;
            } else if (antaresMoveSpeed >= ANTARES_PASS_SPEED) {
                // At 500 px/s or more, the player crossing behind it nearby,
                // or a wide miss carrying it far away, means it ran past:
                // skid straight ahead and fall back to stage 1.
                const bool crossed = antaresPrevFacing > 0.0f && facing <= 0.0f &&
                                     distance < PASS_RANGE;
                const bool flewOff = facing < 0.0f && distance > FLYOFF_RANGE;
                if (crossed || flewOff) {
                    antaresState = AntaresState::SKID;
                    antaresStateTimer = 0.0f;
                    antaresSkidDecel = (antaresMoveSpeed - ANTARES_MIN_SPEED) / SKID_TIME;
                }
            }
        } else if (antaresState == AntaresState::SKID) {
            antaresMoveSpeed = std::max(ANTARES_MIN_SPEED,
                                        antaresMoveSpeed - antaresSkidDecel * deltaTime);
            if (antaresStateTimer >= SKID_TIME) {
                antaresState = AntaresState::REST;
                antaresStateTimer = 0.0f;
                antaresMoveSpeed = 0.0f;
            }
        } else {
            // STUNNED and REST hold still, then turn to the player and start
            // a fresh run from stage 1.
            const float hold = antaresState == AntaresState::STUNNED
                ? ANTARES_STUN_TIME : ANTARES_REST_TIME;
            antaresMoveSpeed = 0.0f;
            if (antaresStateTimer >= hold) {
                antaresState = AntaresState::CHASE;
                antaresStateTimer = 0.0f;
                antaresMoveSpeed = ANTARES_MIN_SPEED;
                antaresHeadingX = toX;
                antaresHeadingY = toY;
            }
        }
        antaresPrevFacing = antaresHeadingX * toX + antaresHeadingY * toY;
        antaresBounceCd = std::max(0.0f, antaresBounceCd - deltaTime);

        // Spin tracks speed, so the frame visibly winds up and down with it.
        antaresSpinSpeed = SPIN_SLOW + (SPIN_FAST - SPIN_SLOW) * AntaresSpeedRatio();
        antaresVisualAngle += antaresSpinSpeed * deltaTime;

        const float step = antaresMoveSpeed * speedMult * deltaTime;
        worldX += antaresHeadingX * step;
        worldY += antaresHeadingY * step;

        antaresTrailTimer += deltaTime;
        // Fixed-interval samples space the ghosts by distance travelled, so a
        // faster run draws a longer, more spread-out trail.
        if (antaresTrailTimer >= 0.04f) {
            antaresTrailTimer = 0.0f;
            for (int i = ANTARES_TRAIL_COUNT - 1; i > 0; --i)
                antaresTrail[i] = antaresTrail[i - 1];
            antaresTrail[0] = glm::vec3(worldX, worldY, antaresVisualAngle);
        }

        TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                   gateWX, gateWY, gateWW, gateWH);
    }

    void Update(float playerCX, float playerCY, float deltaTime,
                float& playerHP, float speedMult = 1.0f,
                float gateWX = -1.0f, float gateWY = -1.0f,
                float gateWW = -1.0f, float gateWH = -1.0f,
                std::vector<Bullet>* bullets = nullptr,
                const glm::vec4& attackBounds = glm::vec4(0.0f)) {
        if (!alive) return;
        hitFlashTimer = std::max(0.0f, hitFlashTimer - deltaTime);
        hitStunCooldown = std::max(0.0f, hitStunCooldown - deltaTime);
        eliteHitPulse = std::max(0.0f, eliteHitPulse - deltaTime / ELITE_HIT_PULSE_TIME);
        eliteHitSwell = std::min(eliteHitPulse,
                                 eliteHitSwell + deltaTime / ELITE_HIT_RISE_TIME);
        eliteHitDent = std::max(0.0f, eliteHitDent - deltaTime / ELITE_HIT_DENT_TIME);
        eliteHitShake = std::max(0.0f, eliteHitShake - deltaTime);
        magnetarInterceptTimer = std::max(0.0f,
                                          magnetarInterceptTimer - deltaTime);
        for (float& flash : magnetarFaceHitFlash)
            flash = std::max(0.0f, flash - deltaTime);
        regulusShield.flashTimer = std::max(0.0f,
                                            regulusShield.flashTimer - deltaTime);
        regulusShield.grantTimer = std::max(0.0f,
                                            regulusShield.grantTimer - deltaTime);
        if (kind == MobKind::REGULUS && regulusShieldRegenTimer > 0.0f) {
            regulusShieldRegenTimer = std::max(0.0f,
                                               regulusShieldRegenTimer - deltaTime);
            if (regulusShieldRegenTimer <= 0.0f) {
                regulusShield.hp = regulusShield.maxHp;
                regulusShield.flashTimer = MOB_HIT_FLASH_TIME;
            }
        }
        if (singularityGrace > 0.0f)
            singularityGrace = std::max(0.0f, singularityGrace - deltaTime);
        if (burnTimer > 0.0f) {
            ApplyDamage(burnDps * deltaTime);
            burnTimer -= deltaTime;
            if (burnTimer <= 0.0f) { burnTimer = 0.0f; burnDps = 0.0f; }
            if (!alive) return;
        }

        if (hitStunTimer > 0.0f) {
            worldX += hitKnockX * deltaTime;
            worldY += hitKnockY * deltaTime;
            hitStunTimer = std::max(0.0f, hitStunTimer - deltaTime);
            return;
        }

        const float dx = playerCX - worldX;
        const float dy = playerCY - worldY;
        const float distance = std::sqrt(dx * dx + dy * dy) + 1e-4f;

        if (genesisControlled) GenesisTrackPlayer(playerCX, playerCY, deltaTime);
        if (genesisSummoned && thrownPhase == 0 &&
            (kind == MobKind::ROTOR || kind == MobKind::SWARM || kind == MobKind::GRAVIS)) {
            // A summoned melee enemy gets a little faster the longer it chases, up to a limit.
            summonRamp = std::min(SUMMON_RAMP_MAX, summonRamp + SUMMON_RAMP_RATE * deltaTime);
            speedMult *= summonRamp;
        }

        if (kind == MobKind::NORMA) {
            UpdateNorma(dx, dy, distance, deltaTime, playerHP, speedMult, bullets);
            return;
        }
        if (kind == MobKind::CAELUM) {
            UpdateCaelum(dx, dy, distance, deltaTime, playerHP, speedMult);
            return;
        }
        if (kind == MobKind::LYNX) {
            UpdateLynx(dx, dy, distance, deltaTime, playerHP, speedMult, gateWX, gateWY, gateWW, gateWH);
            return;
        }
        if (kind == MobKind::MIRA) {
            UpdateMira(dx, dy, distance, deltaTime, playerHP, speedMult);
            return;
        }
        if (kind == MobKind::TRIANGULUM) {
            UpdateTriangulum(dx, dy, distance, deltaTime, speedMult);
            return;
        }

        if(genesisThrown && (genesisFromLauncher || !genesisSummoned) && kind==MobKind::ROTOR && genesisRotorPhase==0 && thrownDuration>0) {
            thrownTimer+=deltaTime;
            speedMult*=GenesisSmooth((thrownTimer-0.25f)/0.3f);
        }

        if (genesisRotorPhase == 1) return; // owner moves ammunition through the cradle
        if (genesisRotorPhase > 1) {
            UpdateGenesisRotor(deltaTime, playerCX, playerCY, playerHP);
            return;
        }

        // A thrown Swarm neither chases nor deals contact damage until its
        // flight and landing stun are over.
        if (thrownPhase != 0) {
            UpdateThrownSwarm(deltaTime, distance, playerHP);
            return;
        }

        if (kind == MobKind::MAGNETAR) {
            magnetarVisualAngle += deltaTime * 0.72f;
            {
                // The N pole swings toward the player at a limited rate.
                float turn = atan2f(dy, dx) - magnetarFacing;
                while (turn > 3.14159265f) turn -= 6.28318531f;
                while (turn < -3.14159265f) turn += 6.28318531f;
                const float maxTurn = 3.0f * deltaTime;
                magnetarFacing += std::clamp(turn, -maxTurn, maxTurn);
            }
            // Broken faces restore on their own, MAGNETAR_FACE_RESTORE after
            // they broke.
            for (int f = 0; f < MAGNETAR_FACES; ++f) {
                magnetarFaceRestored[f] += deltaTime;
                if (magnetarFaceBroken[f] < 0.0f) continue;
                magnetarFaceBroken[f] += deltaTime;
                if (magnetarFaceBroken[f] >= MAGNETAR_FACE_RESTORE) {
                    magnetarFaceBroken[f] = -1.0f;
                    magnetarFaceHits[f] = MAGNETAR_FACE_HITS;
                    magnetarFaceRestored[f] = 0.0f;
                }
            }
            if (bullets) {
                const float fieldRadiusSq = MAGNETAR_FIELD_RADIUS *
                                            MAGNETAR_FIELD_RADIUS;
                for (auto& bullet : *bullets) {
                    if (!bullet.active || bullet.isEnemy ||
                        bullet.wardPassed == this) continue;
                    const float sx = bullet.x - bullet.prevX;
                    const float sy = bullet.y - bullet.prevY;
                    const float segLenSq = sx * sx + sy * sy;
                    float t = segLenSq > 0.001f
                        ? ((worldX - bullet.prevX) * sx +
                           (worldY - bullet.prevY) * sy) / segLenSq : 0.0f;
                    t = std::max(0.0f, std::min(1.0f, t));
                    const float nearestX = bullet.prevX + sx * t;
                    const float nearestY = bullet.prevY + sy * t;
                    const float bx = nearestX - worldX;
                    const float by = nearestY - worldY;
                    if (bx * bx + by * by > fieldRadiusSq) continue;
                    // Hit point on the rim, on the side the shot came from.
                    float ex = bullet.prevX - worldX, ey = bullet.prevY - worldY;
                    if (ex * ex + ey * ey < 1.0f) { ex = bx; ey = by; }
                    const float hitA = atan2f(ey, ex);
                    // Only the face on that side decides: a broken one lets
                    // the shot through (and it is not checked again).
                    const int face = MagnetarFaceToward(hitA);
                    if (magnetarFaceBroken[face] >= 0.0f) {
                        bullet.wardPassed = this;
                        continue;
                    }
                    bullet.active = false;
                    magnetarFaceHitFlash[face] = MAGNETAR_HIT_FLASH;
                    if (--magnetarFaceHits[face] <= 0) {
                        magnetarFaceHits[face] = 0;
                        magnetarFaceBroken[face] = 0.0f;
                    }
                    const float nx = cosf(hitA), ny = sinf(hitA);
                    magnetarInterceptX = worldX + nx * MAGNETAR_FIELD_RADIUS;
                    magnetarInterceptY = worldY + ny * MAGNETAR_FIELD_RADIUS;
                    magnetarInterceptTimer = 0.16f;
                }
            }
            worldX += dx / distance * speed * speedMult * deltaTime;
            worldY += dy / distance * speed * speedMult * deltaTime;
            TryContact(distance, playerHP, contactDmg, deltaTime,
                       -1.0f, gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (kind == MobKind::GRAVIS) {
            UpdateGravis(deltaTime, speedMult);
            TryContact(std::sqrt((playerCX - worldX) * (playerCX - worldX) +
                                 (playerCY - worldY) * (playerCY - worldY)) + 1e-4f,
                       playerHP, contactDmg, deltaTime);
            return;
        }

        if (kind == MobKind::QUASAR) {
            static constexpr float COOLDOWN_TIME = QUASAR_COOLDOWN_TIME;
            static constexpr float LOCK_TIME = QUASAR_LOCK_TIME;
            static constexpr float BEAM_TIME = QUASAR_BEAM_TIME;
            static constexpr float RECOVER_TIME = 1.4f;
            static constexpr float BEAM_RADIUS = QUASAR_BEAM_RADIUS;
            static constexpr float BEAM_DPS = QUASAR_BEAM_DPS;

            quasarTimer += deltaTime;
            quasarBounds = attackBounds;
            // The eruption condition: the player staying close for QUASAR_ERUPT_TRIGGER_TIME.
            quasarEruptCooldown = std::max(0.0f, quasarEruptCooldown - deltaTime);
            if (!quasarEruptActive) {
                if (distance <= QUASAR_ERUPT_TRIGGER_RADIUS)
                    quasarNearTimer = std::min(QUASAR_ERUPT_TRIGGER_TIME, quasarNearTimer + deltaTime);
                else
                    quasarNearTimer = std::max(0.0f, quasarNearTimer - QUASAR_ERUPT_NEAR_DRAIN * deltaTime);
            }
            if (quasarPlayerSeen && deltaTime > 1e-5f) {
                glm::vec2 inst = (glm::vec2(playerCX, playerCY) - quasarPlayerPrev) / deltaTime;
                const float sp = glm::length(inst);
                if (sp > QUASAR_PREDICT_MAX_SPEED) inst *= QUASAR_PREDICT_MAX_SPEED / sp;
                quasarPlayerVel += (inst - quasarPlayerVel) * (1.0f - expf(-deltaTime * 12.0f));
            }
            quasarPlayerPrev = glm::vec2(playerCX, playerCY);
            quasarPlayerSeen = true;
            quasarVisualAngle += deltaTime * 0.24f;
            quasarSpinSpeed += (QuasarSpinTarget() - quasarSpinSpeed) * (1.0f - expf(-deltaTime * 7.0f));
            quasarShellSpin += deltaTime * quasarSpinSpeed;
            float coreTurn = atan2f(quasarAimY, quasarAimX) - quasarCoreAngle;
            coreTurn = std::remainder(coreTurn, 6.2831853f);
            quasarCoreAngle += coreTurn * (1.0f - expf(-deltaTime * 10.0f));
            // Camped on: the eruption goes off at once, whatever the Quasar is doing
            // (wind-up, lock, beam, starfall): it is an overlay with its own timer.
            if (!quasarEruptActive && quasarEruptCooldown <= 0.0f &&
                quasarNearTimer >= QUASAR_ERUPT_TRIGGER_TIME)
                BeginQuasarEruption();
            if (quasarEruptActive) {
                quasarEruptTimer += deltaTime;
                UpdateQuasarEruption(playerCX, playerCY, playerHP);
            }
            if (quasarState == 0) {
                quasarMoveTimer -= deltaTime;
                if (quasarMoveTimer <= 0.0f) {
                    quasarMoveTurn = ((float)(rand() % 121) - 60.0f) * 0.01f;
                    quasarMoveTimer = 0.8f + (float)(rand() % 101) * 0.01f;
                }
                // Drift until the wind-up starts, then hold position. The
                // axis is chosen once, at the start of the wind-up, and stays
                // put through the wind-up and the lock (1.7s of fixed
                // telegraph) so the band never chases the player.
                if (QuasarPrecharge() <= 0.0f) {
                    quasarAimSet = false;
                    if (!quasarEruptActive) {       // the zones are centred here: stay put while they run
                        quasarMoveAngle += quasarMoveTurn * deltaTime;
                        worldX += cosf(quasarMoveAngle) * speed * speedMult * deltaTime;
                        worldY += sinf(quasarMoveAngle) * speed * speedMult * deltaTime;
                    }
                } else if (!quasarAimSet) {
                    if (distance > 0.001f) {
                        quasarAimX = dx / distance;
                        quasarAimY = dy / distance;
                    }
                    quasarAimSet = true;
                }
                if (quasarTimer >= COOLDOWN_TIME) {
                    if (quasarNextFall) BeginQuasarFall();
                    else {
                        quasarState = 1;
                        quasarTimer = 0.0f;
                    }
                }
            } else if (quasarState == 1) {
                // Lock. The aim was chosen at the start of the wind-up; only
                // if that was skipped (e.g. a forced state) is it taken now.
                if (!quasarAimSet) {
                    if (distance > 0.001f) {
                        quasarAimX = dx / distance;
                        quasarAimY = dy / distance;
                    }
                }
                quasarAimSet = true;
                quasarState = 2;
                quasarTimer = 0.0f;
            } else if (quasarState == 2) {
                if (quasarTimer >= LOCK_TIME) {
                    quasarState = 3;
                    quasarTimer = 0.0f;
                }
            } else if (quasarState == 3) {
                // Very slow turn toward the player while firing. The axis
                // repeats every pi (both ends fire), so the gap is folded into
                // [-pi/2, pi/2] and the nearer end swings onto the player; the
                // step is capped so it stops on the player instead of jittering.
                if (distance > 0.001f) {
                    const float aim = atan2f(quasarAimY, quasarAimX);
                    float gap = atan2f(dy, dx) - aim;
                    gap -= 3.1415927f * floorf(gap / 3.1415927f + 0.5f);
                    const float maxStep = QUASAR_BEAM_TURN * deltaTime *
                        QuasarEase((quasarTimer - QUASAR_BEAM_GROW) / QUASAR_BEAM_TURN_EASE);   // frozen while it extends
                    const float next = aim + std::clamp(gap, -maxStep, maxStep);
                    quasarAimX = cosf(next);
                    quasarAimY = sinf(next);
                }
                const float growT = std::min(1.0f, quasarTimer / QUASAR_BEAM_GROW);
                const float reach = QuasarBeamLength() * growT * growT * growT;
                std::array<glm::vec4, QUASAR_BEAM_MAX_SEGMENTS> seg;
                const int segCount = QuasarBeamSegments(reach, seg);
                for (int k = 0; k < segCount; ++k) {
                    const float ax = seg[k].x, ay = seg[k].y;
                    const float abx = seg[k].z - ax, aby = seg[k].w - ay;
                    const float length2 = abx * abx + aby * aby;
                    if (length2 <= 0.001f) continue;
                    float u = ((playerCX - ax) * abx + (playerCY - ay) * aby) / length2;
                    u = std::max(0.0f, std::min(1.0f, u));
                    const float hx = ax + abx * u - playerCX;
                    const float hy = ay + aby * u - playerCY;
                    const float hitR = BEAM_RADIUS * QuasarBeamWidth();      // thins out with the beam
                    if (hx * hx + hy * hy <= hitR * hitR) {
                        HurtPlayer(playerHP, BEAM_DPS * deltaTime, "QUASAR_BEAM");   // once per frame
                        break;
                    }
                }
                if (quasarTimer >= BEAM_TIME) {
                    quasarState = 4;
                    quasarTimer = 0.0f;
                }
            } else if (quasarState == QUASAR_FALL_STATE) {
                UpdateQuasarFall(playerCX, playerCY, playerHP, attackBounds);
            } else if (quasarState == QUASAR_FALL_RECOVER_STATE) {
                if (quasarTimer >= QUASAR_FALL_RECOVER - QUASAR_FALL_IMPACT_FX) {
                    quasarNextFall = false;
                    quasarState = 0;
                    quasarTimer = 0.0f;
                }
            } else if (quasarTimer >= RECOVER_TIME) {
                quasarNextFall = true;
                quasarState = 0;
                quasarTimer = 0.0f;
            }
            TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                       gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (kind == MobKind::GIMBAL) {
            UpdateGimbal(playerCX, playerCY, deltaTime, distance, dx, dy,
                         playerHP, speedMult, bullets,
                         gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (kind == MobKind::ANTARES) {
            UpdateAntares(deltaTime, distance, dx, dy, playerHP, speedMult,
                          gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (kind == MobKind::REGULUS) {
            regulusVisualAngle += deltaTime * 0.32f;
            regulusDriftTimer -= deltaTime;
            if (regulusDriftTimer <= 0.0f) {
                regulusDriftAngle += ((float)(rand() % 201) - 100.0f) * 0.012f;
                regulusDriftTimer = 0.8f + (float)(rand() % 150) * 0.01f;
            }
            const float movementSpeed = std::min(speed * speedMult, 90.0f);
            worldX += cosf(regulusDriftAngle) * movementSpeed * deltaTime;
            worldY += sinf(regulusDriftAngle) * movementSpeed * deltaTime;
            return;
        }

        if (genesisEgress) {
            genesisEgressTimer += deltaTime;
            const float egressSpeed = speed * 1.55f * speedMult;
            worldX += genesisEgressX * egressSpeed * deltaTime;
            worldY += genesisEgressY * egressSpeed * deltaTime;
            if (genesisEgressTimer >= 0.62f) {
                genesisEgressTimer = 0.0f;
                genesisEgress = false;
            }
            TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                       gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (kind == MobKind::GENESIS) {
            if (hivePhase >= 4) return;
            const float ax = spawnAnchorX - worldX;
            const float ay = spawnAnchorY - worldY;
            const float anchorDistance = std::sqrt(ax * ax + ay * ay);
            if (anchorDistance > 12.0f) {
                anchored = false;
                const float step = std::min(anchorDistance, speed * speedMult * deltaTime);
                worldX += (ax / anchorDistance) * step;
                worldY += (ay / anchorDistance) * step;
            } else {
                worldX = spawnAnchorX;
                worldY = spawnAnchorY;
                anchored = true;
            }
            TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                       gateWX, gateWY, gateWW, gateWH);
            return;
        }

        if (genesisControlled) {
            GenesisControlledMove(dx, dy, distance, playerCX, playerCY, deltaTime, speedMult, bullets);
        } else if (distance > 5.0f) {
            worldX += (dx / distance) * speed * speedMult * deltaTime;
            worldY += (dy / distance) * speed * speedMult * deltaTime;
        }
        TryContact(distance, playerHP, contactDmg, deltaTime, -1.0f,
                   gateWX, gateWY, gateWW, gateWH);
    }
};
