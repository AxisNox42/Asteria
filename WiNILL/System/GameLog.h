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
    std::map<std::string, double> dealtBy;   // target kind -> damage
    std::map<std::string, double> takenBy;   // damage source -> damage
    std::map<std::string, int>    killsBy;   // mob kind -> kills
};
inline Window g_win;

inline bool On() { return g_out != nullptr && g_out->is_open(); }

inline void Attach(std::ofstream* out) { g_out = out; g_win = Window{}; }
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
             "shots=%d,dashes=%d,casts=%d,kills=%d",
             g_time, g_wave, hp, maxHp, g_win.dmgDealt, g_win.dmgTaken,
             g_win.shots, g_win.dashes, g_win.casts, g_win.kills);
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
