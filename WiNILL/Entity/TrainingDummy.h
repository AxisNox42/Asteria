#pragma once
// Damage-test dummy (F1 debug console > Spawn > 테스트). A dummy is an ordinary
// Monster flagged trainingDummy, so every damage path (bullets, augments,
// skills, synergies) reaches it; it never dies, moves, attacks or touches the
// player. This log keeps what each dummy took, for the readout over it.
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <utility>

namespace dummy {

inline constexpr float kWindow = 5.0f;   // DPS over the last 5 seconds

struct Log {
    float clock = 0.0f;                          // the dummy's own time
    std::deque<std::pair<float, float>> hits;    // (time, damage) inside the window
    double total = 0.0;
    float last = 0.0f;
    int count = 0;
};
inline std::unordered_map<const void*, Log> g_Logs;

inline void Hit(const void* who, float damage) {
    Log& l = g_Logs[who];
    l.hits.push_back({ l.clock, damage });
    l.total += damage;
    l.last = damage;
    ++l.count;
}

inline void Tick(const void* who, float dt) {
    Log& l = g_Logs[who];
    l.clock += dt;
    while (!l.hits.empty() && l.clock - l.hits.front().first > kWindow) l.hits.pop_front();
}

// Damage per second over the window; a burst shorter than a second counts as one second.
inline float Dps(const Log& l) {
    if (l.hits.empty()) return 0.0f;
    float sum = 0.0f;
    for (const auto& h : l.hits) sum += h.second;
    const float span = std::clamp(l.clock - l.hits.front().first, 1.0f, kWindow);
    return sum / span;
}

inline const Log* Find(const void* who) {
    auto it = g_Logs.find(who);
    return it == g_Logs.end() ? nullptr : &it->second;
}
inline void Forget(const void* who) { g_Logs.erase(who); }
inline void Clear() { g_Logs.clear(); }

}  // namespace dummy
