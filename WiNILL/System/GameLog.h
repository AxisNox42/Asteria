#pragma once

// Balance event log. Lines go into the same per-run section of
// asteria_gameplay_telemetry.txt that GameplayTelemetry writes (it attaches
// its stream for the length of a run), so one file holds a whole run:
//   EVENT,t=12.34,type=KILL,mob=ROTOR,...      one line per discrete action
//   WINDOW,t=20.00,dmg_dealt=...,dmg_taken=...  totals over each sample interval
// Nothing is written outside a run, and every call is a cheap no-op then.
// Header-only with no game includes, so any module can call it.

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>

namespace gamelog {

inline std::ofstream* g_out = nullptr;
inline float g_time = 0.0f;       // gameplay seconds, set once a frame
inline int   g_wave = 0;          // current wave, set once a frame
inline float g_hp = 0.0f, g_maxHp = 0.0f;   // player health, set once a frame
// Census of live mobs ("ROTOR:3|GRAVIS:1"), supplied by the game; called only
// when a WINDOW line is written.
inline std::string (*g_aliveFn)() = nullptr;

// Per-window totals; WriteWindow() prints and clears them.
struct Window {
    double dmgDealt = 0.0;
    double dmgTaken = 0.0;
    int    shots = 0;
    int    dashes = 0;
    int    casts = 0;
    int    kills = 0;
    int    refills = 0;                      // regular-wave spawns from the min-alive refill
    int    fieldTarget = 0;                  // highest refill floor this window (kill-rate driven)
    std::map<std::string, double> dealtBy;   // target kind -> damage
    std::map<std::string, double> takenBy;   // damage source -> damage
    std::map<std::string, int>    killsBy;   // mob kind -> kills
};
inline Window g_win;

// One boss or elite fight, from its entry to its death (or the run's end):
//   FIGHT_BEGIN / FIGHT_GATE / FIGHT_PATTERN events while it runs, then one
//   FIGHT_END line with the totals. Damage the player takes is filed under
//   the pattern running at the time (Docs/Plans/2026-11-17_보스엘리트_게임구조_개선.md, 0단계).
struct Fight {
    bool on = false;
    const void* who = nullptr;          // the fought monster (identity only)
    std::string kind;
    float begin = 0.0f;
    float maxHp = 0.0f;
    float lastGate = 0.0f;              // time of the last gate (or the begin)
    int gates = 0;
    std::string pattern = "-";          // the pattern running now
    double dealt = 0.0, dealtExposed = 0.0, taken = 0.0;
    std::map<std::string, double> takenBy;     // pattern -> damage taken
    std::map<std::string, int>    patterns;    // pattern -> times run
    std::string gateTimes;              // "12.3|30.1" seconds into the fight
    // How much the player moves under each pattern (FightMove): seconds the
    // pattern ran, px travelled, and seconds spent nearly still.
    std::map<std::string, double> secBy, moveBy, stillBy;
};
// Under this speed (px/s) the player counts as standing still.
inline constexpr float kStillSpeed = 40.0f;
inline Fight g_fight;

inline bool On() { return g_out != nullptr && g_out->is_open(); }

inline void Attach(std::ofstream* out) { g_out = out; g_win = Window{}; g_fight = Fight{}; }
inline void Detach() { g_out = nullptr; }
inline void SetClock(float gameTime, int wave, float hp, float maxHp) {
    g_time = gameTime; g_wave = wave; g_hp = hp; g_maxHp = maxHp;
}

// One event line: type plus printf-style "key=value,key=value" fields.
inline void Event(const char* type, const char* fmt = "", ...) {
    if (!On()) return;
    char body[512];
    body[0] = 0;
    if (fmt && *fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(body, sizeof(body), fmt, args);
        va_end(args);
    }
    char head[128];
    snprintf(head, sizeof(head), "EVENT,t=%.2f,wave=%d,type=%s", g_time, g_wave, type);
    *g_out << head;
    if (body[0]) *g_out << ',' << body;
    *g_out << '\n';
}

// ---- window counters -----------------------------------------------------
inline void Dealt(const char* target, float amount) {
    if (!On() || amount <= 0.0f) return;
    g_win.dmgDealt += amount;
    g_win.dealtBy[target ? target : "?"] += amount;
}
inline void Taken(const char* source, float amount) {
    if (!On() || amount <= 0.0f) return;
    g_win.dmgTaken += amount;
    g_win.takenBy[source ? source : "?"] += amount;
    if (g_fight.on) {
        g_fight.taken += amount;
        // Under the running pattern; fights without pattern names (most
        // elites) file it under the damage source instead.
        g_fight.takenBy[g_fight.pattern != "-" ? g_fight.pattern : (source ? source : "?")] += amount;
    }
}
inline void Refill() { if (On()) ++g_win.refills; }
inline void FieldTarget(int n) { if (On() && n > g_win.fieldTarget) g_win.fieldTarget = n; }

// ---- boss / elite fights --------------------------------------------------
inline void FightBegin(const void* who, const char* kind, float maxHp) {
    if (!On()) return;
    g_fight = Fight{};
    g_fight.on = true;
    g_fight.who = who;
    g_fight.kind = kind ? kind : "?";
    g_fight.begin = g_time;
    g_fight.lastGate = g_time;
    g_fight.maxHp = maxHp;
    Event("FIGHT_BEGIN", "mob=%s,max_hp=%.0f", g_fight.kind.c_str(), maxHp);
}
// A pattern started; later damage taken is filed under it.
inline void FightPattern(const char* name) {
    if (!On() || !g_fight.on) return;
    g_fight.pattern = name ? name : "?";
    ++g_fight.patterns[g_fight.pattern];
    Event("FIGHT_PATTERN", "mob=%s,pattern=%s,fight_sec=%.2f",
          g_fight.kind.c_str(), g_fight.pattern.c_str(), g_time - g_fight.begin);
}
// A pattern that runs on top of the current one (the nightmare Antares wake):
// counted and logged, but the running pattern keeps filing the damage.
inline void FightOverlay(const char* name) {
    if (!On() || !g_fight.on) return;
    ++g_fight.patterns[name ? name : "?"];
    Event("FIGHT_PATTERN", "mob=%s,pattern=%s,overlay=1,fight_sec=%.2f",
          g_fight.kind.c_str(), name ? name : "?", g_time - g_fight.begin);
}
// The fight reached a phase gate (or its phase change).
inline void FightGate(int gate, float hpRatio) {
    if (!On() || !g_fight.on) return;
    ++g_fight.gates;
    const float sec = g_time - g_fight.begin;
    char item[32];
    snprintf(item, sizeof(item), "%s%.1f", g_fight.gateTimes.empty() ? "" : "|", sec);
    g_fight.gateTimes += item;
    Event("FIGHT_GATE", "mob=%s,gate=%d,hp_ratio=%.3f,fight_sec=%.2f,phase_sec=%.2f",
          g_fight.kind.c_str(), gate, hpRatio, sec, g_time - g_fight.lastGate);
    g_fight.lastGate = g_time;
}
inline void FightDealt(const void* who, float amount, bool exposed) {
    if (!On() || !g_fight.on || who != g_fight.who || amount <= 0.0f) return;
    g_fight.dealt += amount;
    if (exposed) g_fight.dealtExposed += amount;
}
// The player moved `px` in `dt` seconds of play: filed under the running
// pattern ("-" between patterns), so FIGHT_END shows which patterns let the
// player stand still (Docs/Plans/2026-11-17_보스전_15분_집중도.md, 1단계).
inline void FightMove(float px, float dt) {
    if (!On() || !g_fight.on || dt <= 0.0f) return;
    g_fight.secBy[g_fight.pattern] += dt;
    g_fight.moveBy[g_fight.pattern] += px;
    if (px < kStillSpeed * dt) g_fight.stillBy[g_fight.pattern] += dt;
}
inline void FightEnd(const char* reason) {
    if (!On() || !g_fight.on) return;
    const float sec = g_time - g_fight.begin;
    char head[320];
    snprintf(head, sizeof(head),
             "EVENT,t=%.2f,wave=%d,type=FIGHT_END,mob=%s,reason=%s,fight_sec=%.2f,"
             "last_phase_sec=%.2f,gates=%d,gate_sec=%s,dealt=%.0f,dealt_exposed_pct=%.1f,"
             "taken=%.1f,taken_pct_max_hp=%.1f",
             g_time, g_wave, g_fight.kind.c_str(), reason ? reason : "?", sec,
             g_time - g_fight.lastGate, g_fight.gates,
             g_fight.gateTimes.empty() ? "-" : g_fight.gateTimes.c_str(), g_fight.dealt,
             g_fight.dealt > 0.0 ? 100.0 * g_fight.dealtExposed / g_fight.dealt : 0.0,
             g_fight.taken, g_maxHp > 0.0f ? 100.0 * g_fight.taken / g_maxHp : 0.0);
    *g_out << head;
    auto dump = [&](const char* key, const auto& m, const char* fmt) {
        *g_out << ',' << key << '=';
        if (m.empty()) { *g_out << '-'; return; }
        bool first = true;
        for (const auto& kv : m) {
            char item[96];
            snprintf(item, sizeof(item), fmt, kv.first.c_str(), (double)kv.second);
            *g_out << (first ? "" : "|") << item;
            first = false;
        }
    };
    dump("patterns", g_fight.patterns, "%s:%.0f");
    dump("taken_by_pattern", g_fight.takenBy, "%s:%.1f");
    dump("pattern_sec", g_fight.secBy, "%s:%.1f");
    dump("move_px_by_pattern", g_fight.moveBy, "%s:%.0f");
    dump("still_sec_by_pattern", g_fight.stillBy, "%s:%.1f");
    *g_out << '\n';
    g_fight = Fight{};
}
inline void Shot()  { if (On()) ++g_win.shots; }
inline void Dash()  { if (On()) ++g_win.dashes; }
inline void Cast()  { if (On()) ++g_win.casts; }
inline void Kill(const char* mob) {
    if (!On()) return;
    ++g_win.kills;
    ++g_win.killsBy[mob ? mob : "?"];
}

// Prints the window totals (one WINDOW line) and starts a new window.
inline void WriteWindow() {
    if (!On()) return;
    const std::string alive = g_aliveFn ? g_aliveFn() : std::string();
    const float hp = g_hp, maxHp = g_maxHp;
    char head[256];
    snprintf(head, sizeof(head),
             "WINDOW,t=%.2f,wave=%d,hp=%.1f,max_hp=%.1f,dmg_dealt=%.1f,dmg_taken=%.1f,"
             "shots=%d,dashes=%d,casts=%d,kills=%d,refills=%d,field_target=%d",
             g_time, g_wave, hp, maxHp, g_win.dmgDealt, g_win.dmgTaken,
             g_win.shots, g_win.dashes, g_win.casts, g_win.kills, g_win.refills,
             g_win.fieldTarget);
    *g_out << head;
    auto dump = [&](const char* key, const auto& m, const char* fmt) {
        if (m.empty()) return;
        *g_out << ',' << key << '=';
        bool first = true;
        for (const auto& kv : m) {
            char item[96];
            snprintf(item, sizeof(item), fmt, kv.first.c_str(), (double)kv.second);
            *g_out << (first ? "" : "|") << item;
            first = false;
        }
    };
    dump("dealt_by", g_win.dealtBy, "%s:%.1f");
    dump("taken_by", g_win.takenBy, "%s:%.1f");
    dump("kills_by", g_win.killsBy, "%s:%.0f");
    if (!alive.empty()) *g_out << ",alive=" << alive;
    *g_out << '\n';
    g_win = Window{};
}

}  // namespace gamelog
