#pragma once
// ─────────────────────────────────────────────────────────────
// HOROLOGIUM (시계자리): the clock elite. Look = the "celestial clock" sheet
// (Docs/Plans/2026-11-18_새엘리트_재시작.md 5.1, option 3): a dial with a
// thin inner circle, four hour ticks, two kite hands turning like a clock,
// a tilted orbit ellipse precessing slowly with a small star riding it, a
// construction triangle and a small construction circle. The whole dial is
// the hit body. It never turns toward the player; the body's hands are
// decoration only and are never used as a warning.
//
// Patterns (plan sections 10-12): time itself, used on the player. Every
// warning is on the floor (or the HUD for the tempo).
//   Reverse volley (P1)   3 salvos, each re-aimed at the player; needles fly
//                         to the screen edge (no range), bending toward the
//                         player, stop, then fly straight back to the dial
//                         wherever it is now, crossing their way out
//   Stasis field   (P1)   a ring that follows the player, then locks; still
//                         inside when it closes and the player freezes, then
//                         a strike lands there
//   Winding rest          1.6 s still, takes x1.3 damage
//   Recall         (P2)   a mark is stamped where the player stands; 3 s
//                         later the player is pulled back onto it and a shock
//                         goes off there 0.5 s later. Hitting the clock hard
//                         enough during the countdown breaks the recall
//   Tempo          (P3)   the player's movement swings between slow and fast
//                         every 2 s; announced, shown round the player and on
//                         the HUD, with a countdown and a pre-switch blink
// Distances scale with the arena height (1080 = 1). Kept free of game
// headers so the regression test can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace horologium {

inline constexpr float kPi = 3.14159265f;
inline constexpr float kTwoPi = 6.2831853f;

// ── Body ──
inline constexpr float kBaseHp = 3000.0f;
inline constexpr float kSizeScale = 2.3f;
inline constexpr float kSpeed = 70.0f;            // px/s drift
inline constexpr float kKeepNear = 340.0f;
inline constexpr float kKeepFar = 480.0f;
inline constexpr float kContactDps = 4.0f;
inline constexpr float kDialRadius = 60.0f;       // the dial (hit body), world px
inline constexpr float kSheet = kDialRadius / 60.0f;   // sheet units -> world px
inline constexpr float kPlayerHitRadius = 10.0f;

// ── Decoration speeds ──
inline constexpr float kMinuteTurn = kTwoPi / 6.0f;
inline constexpr float kHourTurn = kTwoPi / 72.0f;
inline constexpr float kOrbitPrecess = kTwoPi / 40.0f;
inline constexpr float kRiderTurn = kTwoPi / 3.5f;

// ── Reverse volley ──
inline constexpr int kSalvos = 3;
inline constexpr float kSalvoGap = 0.35f;         // s between salvos
inline constexpr int kLanes = 7;                  // per salvo, fanned around the player's direction
inline constexpr float kLaneSpread = 0.24f;       // rad between lanes
inline constexpr float kVolleyWarn = 0.7f;        // first salvo's lanes track the player, then fire
inline constexpr float kNeedleSpeed = 520.0f;     // out
inline constexpr float kReturnSpeed = 600.0f;     // back to the dial
inline constexpr float kNeedleHoming = 0.9f;      // rad/s toward the player on the way out
inline constexpr float kNeedleHold = 0.35f;       // stopped at the edge, turning round
inline constexpr float kNeedleMaxOut = 2600.0f;   // without arena bounds (tests)
inline constexpr int kMaxNeedles = kSalvos * kLanes;
inline constexpr float kNeedleHalfLen = 12.0f;
inline constexpr float kNeedleHitRadius = 10.0f;
inline constexpr float kNeedleDamage = 9.0f;

// ── Stasis field ──
inline constexpr float kStasisRadius = 180.0f;
inline constexpr float kStasisWarn = 1.0f;
inline constexpr float kStasisFollow = 0.6f;      // follows the player this long, then locks
inline constexpr float kFreezeTime = 1.2f;
inline constexpr float kStasisStrikeAt = 0.7f;    // into the freeze
inline constexpr float kStasisDamage = 18.0f;

// ── Winding rest ──
inline constexpr float kIdleTime = 1.0f;
inline constexpr float kRestTime = 1.6f;
inline constexpr float kRestDamageMult = 1.3f;

// ── Recall (phase 2+) ──
inline constexpr float kRecallAim = 1.0f;         // the mark follows the player, then stamps
inline constexpr float kRecallCount = 3.0f;
inline constexpr float kRecallShockDelay = 0.5f;
inline constexpr float kRecallShockRadius = 170.0f;
inline constexpr float kRecallDamage = 16.0f;
inline constexpr float kRecallBreakShare = 0.08f; // of the elite's max HP, dealt during the countdown
inline constexpr float kRecallCooldown = 10.0f;

// ── Tempo (phase 3) ──
inline constexpr float kTempoBeat = 2.0f;
inline constexpr float kTempoSlow = 0.55f;
inline constexpr float kTempoFast = 1.5f;
inline constexpr float kTempoWarn = 0.5f;         // the ring blinks the next state this long before
inline constexpr float kTempoAnnounce = 3.0f;     // the HUD explains it this long when it starts

inline float Wrap(float a) { return std::remainder(a, kTwoPi); }

// What the clock does to the player this frame (read by the game's player
// movement and HUD, refreshed every frame by a living HOROLOGIUM, worn by Tick).
struct PlayerTime {
    float freeze = 0.0f;      // > 0: frozen
    float tempoLeft = 0.0f;   // > 0: tempo active
    float tempo = 1.0f;       // current tempo factor
    float beatLeft = 0.0f;    // seconds to the next switch
    float announce = 0.0f;    // > 0: the HUD explains the tempo
    bool recall = false;      // pull the player to (recallX, recallY) now
    float recallX = 0.0f, recallY = 0.0f;
};
inline PlayerTime g_player;
inline void Tick(float dt) {
    g_player.freeze = std::max(0.0f, g_player.freeze - dt);
    g_player.tempoLeft = std::max(0.0f, g_player.tempoLeft - dt);
    g_player.announce = std::max(0.0f, g_player.announce - dt);
}
inline bool TempoOn() { return g_player.tempoLeft > 0.0f; }
inline bool TempoSlow() { return g_player.tempo < 1.0f; }
inline float MoveFactor() {
    if (g_player.freeze > 0.0f) return 0.0f;
    return TempoOn() ? g_player.tempo : 1.0f;
}
// The game moves the player onto the mark once and clears the request.
inline bool TakeRecall(float& x, float& y) {
    if (!g_player.recall) return false;
    g_player.recall = false;
    x = g_player.recallX; y = g_player.recallY;
    return true;
}

enum class State { Idle, VolleyWarn, Volley, StasisWarn, Freeze, Rest };
enum class Recall { Off, Aim, Count, Shock };
enum class Leg { Out, Hold, Back };

struct Needle {
    float x = 0, y = 0, a = 0;   // position, heading
    float hold = 0;
    Leg leg = Leg::Out;
    bool active = false;
    bool hitOut = false, hitBack = false;
};
struct Burst { float x = 0, y = 0; bool big = false; };
struct Flash { float x = 0, y = 0, r = 0, age = -1.0f; };

struct Brain {
    // Decoration.
    float minute = 0.0f, hour = 0.0f, tilt = -0.349f, rider = 0.0f;

    State state = State::Idle;
    float timer = kIdleTime;
    float clock = 0.0f;
    float scale = 1.0f;
    const char* pattern = nullptr;
    bool tickCue = false;            // the owner plays a tick (tempo pre-switch)

    Burst bursts[16];
    int burstCount = 0;
    Flash flashes[12];

    // Reverse volley.
    Needle needles[kMaxNeedles];
    float volleyAim = 0.0f;          // the next salvo's centre lane
    int salvosFired = 0;
    float salvoTimer = 0.0f;

    // Stasis.
    float stasisX = 0.0f, stasisY = 0.0f;
    bool caught = false, struck = false;

    // Recall.
    Recall recall = Recall::Off;
    float recallTimer = 0.0f, recallCooldown = 4.0f;
    float markX = 0.0f, markY = 0.0f;
    float recallDealt = 0.0f, maxHp = 1.0f;

    // Tempo.
    float tempoClock = -1.0f;        // < 0: not started yet
    float lastPX = 0.0f, lastPY = 0.0f;   // the player this frame (drawing)

    void Begin(State s, float t, const char* name = nullptr) {
        state = s; timer = t;
        if (name) pattern = name;
    }
    bool Holding() const { return state != State::Idle; }
    bool Resting() const { return state == State::Rest; }
    float DamageMult() const { return Resting() ? kRestDamageMult : 1.0f; }

    void OnDamaged(float dealt) { if (recall == Recall::Count) recallDealt += dealt; }
    float RecallBreakNeed() const { return maxHp * kRecallBreakShare; }
    float RecallProgress() const { return std::clamp(recallDealt / std::max(1.0f, RecallBreakNeed()), 0.0f, 1.0f); }

    void AddBurst(float x, float y, bool big) { if (burstCount < 16) bursts[burstCount++] = { x, y, big }; }
    void AddFlash(float x, float y, float r) {
        for (auto& f : flashes) if (f.age < 0.0f) { f = { x, y, r, 0.0f }; return; }
    }

    float LaneAngle(int lane) const {
        return volleyAim + kLaneSpread * ((float)lane - (float)(kLanes - 1) * 0.5f);
    }
    bool AnyNeedle() const {
        for (const auto& n : needles) if (n.active) return true;
        return false;
    }

    void FireSalvo(float x, float y) {
        for (int l = 0; l < kLanes; ++l) {
            Needle* n = nullptr;
            for (auto& c : needles) if (!c.active) { n = &c; break; }
            if (!n) return;
            *n = {};
            n->a = LaneAngle(l);
            n->x = x + std::cos(n->a) * kDialRadius;
            n->y = y + std::sin(n->a) * kDialRadius;
            n->active = true;
        }
        ++salvosFired;
    }

    template <class HurtFn>
    void StepNeedles(float x, float y, float px, float py, float dt,
                     float bx0, float by0, float bx1, float by1, HurtFn& hurt) {
        const bool bounded = bx1 > bx0 && by1 > by0;
        for (auto& n : needles) {
            if (!n.active) continue;
            if (n.leg == Leg::Out) {
                // Bend toward the player while the player is still ahead;
                // once passed it flies straight on to the edge.
                const float want = std::atan2(py - n.y, px - n.x);
                const float d = Wrap(want - n.a), step = kNeedleHoming * dt;
                if (std::fabs(d) < kPi * 0.5f) n.a = Wrap(n.a + std::clamp(d, -step, step));
                n.x += std::cos(n.a) * kNeedleSpeed * scale * dt;
                n.y += std::sin(n.a) * kNeedleSpeed * scale * dt;
                const bool out = bounded ? (n.x <= bx0 || n.x >= bx1 || n.y <= by0 || n.y >= by1)
                                         : std::hypot(n.x - x, n.y - y) >= kNeedleMaxOut;
                if (out) {
                    if (bounded) { n.x = std::clamp(n.x, bx0, bx1); n.y = std::clamp(n.y, by0, by1); }
                    n.leg = Leg::Hold; n.hold = kNeedleHold;
                    AddFlash(n.x, n.y, 18.0f);
                }
            } else if (n.leg == Leg::Hold) {
                n.hold -= dt;
                if (n.hold <= 0.0f) n.leg = Leg::Back;
            } else {
                // Straight back to the dial, wherever it is now.
                n.a = std::atan2(y - n.y, x - n.x);
                n.x += std::cos(n.a) * kReturnSpeed * scale * dt;
                n.y += std::sin(n.a) * kReturnSpeed * scale * dt;
                if (std::hypot(n.x - x, n.y - y) <= kDialRadius) { n.active = false; continue; }
            }
            if (n.leg == Leg::Hold) continue;
            const float hx = px - n.x, hy = py - n.y, hr = kNeedleHitRadius + kPlayerHitRadius;
            const bool back = n.leg == Leg::Back;
            bool& done = back ? n.hitBack : n.hitOut;
            if (!done && hx * hx + hy * hy <= hr * hr) {
                done = true;
                hurt(kNeedleDamage, back ? "HOROLOGIUM_REVERSE" : "HOROLOGIUM_NEEDLE");
                AddBurst(n.x, n.y, false);
                AddFlash(n.x, n.y, 26.0f);
            }
        }
    }

    // One frame. x, y move; hurt(damage, source) hurts the player. phase:
    // 1..3. bounds: arena (minX, minY, maxX, maxY) or all zero for none.
    template <class HurtFn>
    void Step(float& x, float& y, float px, float py, float dt, int phase,
              float bx0, float by0, float bx1, float by1, float speedMult, HurtFn hurt) {
        clock += dt;
        burstCount = 0;
        tickCue = false;
        lastPX = px; lastPY = py;
        scale = (by1 > by0) ? std::clamp((by1 - by0) / 1080.0f, 0.6f, 2.0f) : 1.0f;
        minute = Wrap(minute + kMinuteTurn * dt);
        hour = Wrap(hour + kHourTurn * dt);
        tilt = Wrap(tilt + kOrbitPrecess * dt);
        rider = Wrap(rider + kRiderTurn * dt);
        for (auto& f : flashes) if (f.age >= 0.0f && (f.age += dt) > 0.45f) f.age = -1.0f;

        // Tempo (phase 3): announced once, then the speed swings each beat.
        if (phase >= 3) {
            if (tempoClock < 0.0f) {
                tempoClock = 0.0f;
                g_player.announce = kTempoAnnounce;
                AddFlash(x, y, std::hypot(px - x, py - y));   // a wave from the clock to the player
                pattern = "TEMPO";
            }
            const float before = std::fmod(tempoClock, kTempoBeat);
            tempoClock += dt;
            const float beat = std::fmod(tempoClock, kTempoBeat);
            const bool slow = std::fmod(tempoClock, kTempoBeat * 2.0f) < kTempoBeat;
            g_player.tempo = slow ? kTempoSlow : kTempoFast;
            g_player.tempoLeft = 0.2f;
            g_player.beatLeft = kTempoBeat - beat;
            if (before < kTempoBeat - kTempoWarn && beat >= kTempoBeat - kTempoWarn) tickCue = true;
            if (beat < before) AddFlash(px, py, 44.0f);   // switched
        }

        StepNeedles(x, y, px, py, dt, bx0, by0, bx1, by1, hurt);

        // Recall (phase 2+), alongside the cycle.
        if (recallCooldown > 0.0f) recallCooldown -= dt;
        switch (recall) {
        case Recall::Off:
            if (phase >= 2 && recallCooldown <= 0.0f && state != State::Freeze) {
                recall = Recall::Aim; recallTimer = kRecallAim;
                pattern = "RECALL";
            }
            break;
        case Recall::Aim:
            markX = px; markY = py;
            recallTimer -= dt;
            if (recallTimer <= 0.0f) {
                recall = Recall::Count; recallTimer = kRecallCount; recallDealt = 0.0f;
                AddFlash(markX, markY, 70.0f);
            }
            break;
        case Recall::Count:
            recallTimer -= dt;
            if (recallDealt >= RecallBreakNeed()) {
                recall = Recall::Off; recallCooldown = kRecallCooldown;
                AddFlash(markX, markY, 90.0f);
                AddBurst(markX, markY, true);
                AddBurst(x, y, true);
            } else if (recallTimer <= 0.0f) {
                g_player.recall = true; g_player.recallX = markX; g_player.recallY = markY;
                recall = Recall::Shock; recallTimer = kRecallShockDelay;
                AddFlash(markX, markY, 50.0f);
                AddBurst(px, py, false);
            }
            break;
        case Recall::Shock:
            recallTimer -= dt;
            if (recallTimer <= 0.0f) {
                const float hx = px - markX, hy = py - markY, hr = kRecallShockRadius * scale + kPlayerHitRadius;
                if (hx * hx + hy * hy <= hr * hr) hurt(kRecallDamage, "HOROLOGIUM_RECALL");
                AddFlash(markX, markY, kRecallShockRadius * scale);
                AddBurst(markX, markY, true);
                recall = Recall::Off; recallCooldown = kRecallCooldown;
            }
            break;
        }

        // Main cycle.
        timer -= dt;
        switch (state) {
        case State::Idle:
            if (timer <= 0.0f) {
                volleyAim = std::atan2(py - y, px - x);
                salvosFired = 0;
                Begin(State::VolleyWarn, kVolleyWarn, "REVERSE_VOLLEY");
            }
            break;
        case State::VolleyWarn:
            volleyAim = std::atan2(py - y, px - x);   // tracks until it fires
            if (timer <= 0.0f) {
                FireSalvo(x, y);
                salvoTimer = kSalvoGap;
                Begin(State::Volley, kSalvoGap * (float)(kSalvos - 1) + 0.6f);
            }
            break;
        case State::Volley:
            volleyAim = std::atan2(py - y, px - x);   // the next salvo re-aims
            if (salvosFired < kSalvos) {
                salvoTimer -= dt;
                if (salvoTimer <= 0.0f) { FireSalvo(x, y); salvoTimer += kSalvoGap; }
            }
            if (timer <= 0.0f) {
                stasisX = px; stasisY = py; caught = false; struck = false;
                Begin(State::StasisWarn, kStasisWarn, "STASIS");
            }
            break;
        case State::StasisWarn:
            if (timer > kStasisWarn - kStasisFollow) { stasisX = px; stasisY = py; }   // follows, then locks
            if (timer <= 0.0f) {
                const float hx = px - stasisX, hy = py - stasisY, r = kStasisRadius * scale;
                caught = hx * hx + hy * hy <= r * r;
                if (caught) g_player.freeze = kFreezeTime;
                AddFlash(stasisX, stasisY, r);
                Begin(State::Freeze, kFreezeTime);
            }
            break;
        case State::Freeze:
            if (caught) g_player.freeze = std::max(g_player.freeze, timer);
            if (!struck && timer <= kFreezeTime - kStasisStrikeAt) {
                struck = true;
                const float hx = px - stasisX, hy = py - stasisY, r = kStasisRadius * scale + kPlayerHitRadius;
                if (hx * hx + hy * hy <= r * r) hurt(kStasisDamage, "HOROLOGIUM_STASIS");
                AddFlash(stasisX, stasisY, kStasisRadius * scale);
                AddBurst(stasisX, stasisY, true);
            }
            if (timer <= 0.0f) Begin(State::Rest, kRestTime);
            break;
        case State::Rest:
            if (timer <= 0.0f) Begin(State::Idle, kIdleTime);
            break;
        }

        // Drift to keep a middle distance, only between patterns.
        if (!Holding()) {
            const float dx = px - x, dy = py - y;
            const float dist = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            float dir = 0.0f;
            if (dist > kKeepFar) dir = 1.0f;
            else if (dist < kKeepNear) dir = -1.0f;
            x += dx / dist * kSpeed * speedMult * dir * dt;
            y += dy / dist * kSpeed * speedMult * dir * dt;
        }
        if (bx1 > bx0 && by1 > by0) {
            const float m = kDialRadius * 1.35f;
            x = std::clamp(x, bx0 + m, std::max(bx0 + m, bx1 - m));
            y = std::clamp(y, by0 + m, std::max(by0 + m, by1 - m));
        }
    }
};

}  // namespace horologium
