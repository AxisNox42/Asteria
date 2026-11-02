#pragma once
// ─────────────────────────────────────────────────────────────
// Balance table: every difficulty-tuned number of a run in one place, one
// field per row and one column per difficulty (EASY, NORMAL, HARD,
// NIGHTMARE), the same layout as the balance sheet
// (Docs/QA/밸런스_초안.xlsx, "최종 수치"). Field names are the sheet's
// "항목 (코드 필드)" names.
//
// The defaults below are the shipped values. A text file can override any
// row at startup (LoadBalanceOverrides), so a simulator or a sheet export
// retunes the game without touching code:
//     # comment
//     xpMult        = 4                       one value: every difficulty
//     wv3           = 0.951, 0.424, 0.61, 0.481
//     wvFirst       = 1, 4, 7, 10             first wave of wv1..wv4
// Commas or whitespace (a pasted sheet row) both separate values.
// Kept free of game headers so tests can include it on its own.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

inline constexpr int kBalanceDifficulties = 4;   // EASY, NORMAL, HARD, NIGHTMARE
inline constexpr int kBalanceWaveBands = 4;      // wv1..wv4

struct BalanceTuning {
    float xpMult;            // experience multiplier
    float wv1, wv2, wv3, wv4;   // spawn rate x per wave band (see g_BalanceWaveBandFirst)
    float kSwarm, kGimbal, kGravis, kMagnetar;   // regular spawn odds x per kind
    float scopeInterval;     // Scope spawn interval x (larger = rarer)
    float bossHp;            // boss HP x
    float hpScale;           // regular enemy HP scale
    float hpLate;            // late-run HP rise (after 300 s)
    float rangedDmg;         // enemy bullet damage x
    float spawnInterval;     // every spawn interval x
    float spawnPressureWave; // spawn pressure added per wave
    float cvStart, cvPeak, cvEnd;   // in-wave spawn curve: start / peak / end
    float cvPeakAt;          // where in the wave the curve peaks (0..1)
};

// Shipped values (sheet "최종 수치", columns EASY / NORMAL / HARD / NIGHTMARE).
inline constexpr BalanceTuning kBalanceDefaults[kBalanceDifficulties] = {
    // xpMult  wv1    wv2    wv3    wv4    kSwarm kGimbal kGravis kMagnetar scopeInt bossHp hpScale hpLate rangedDmg spawnInt pressure cvStart cvPeak cvEnd cvPeakAt
    /* EASY      */ { 4.0f, 0.726f, 0.730f, 0.951f, 0.931f, 1.328f, 0.783f, 0.538f, 0.696f, 1.547f, 0.973f, 0.80f, 0.70f, 1.0f, 1.0f, 0.10f, 1.10f, 1.50f, 0.50f, 0.70f },
    /* NORMAL    */ { 4.0f, 0.851f, 0.890f, 0.424f, 0.609f, 0.716f, 0.530f, 0.540f, 0.755f, 2.159f, 0.425f, 0.80f, 0.70f, 1.0f, 1.0f, 0.10f, 1.10f, 1.50f, 0.50f, 0.70f },
    /* HARD      */ { 4.0f, 0.769f, 0.873f, 0.610f, 0.491f, 0.835f, 1.359f, 0.795f, 1.202f, 1.204f, 0.979f, 0.80f, 0.70f, 1.0f, 1.0f, 0.10f, 1.10f, 1.50f, 0.50f, 0.70f },
    /* NIGHTMARE */ { 4.0f, 0.782f, 0.690f, 0.481f, 0.337f, 0.686f, 1.322f, 0.680f, 0.343f, 1.487f, 0.814f, 0.80f, 0.70f, 1.0f, 1.0f, 0.10f, 1.10f, 1.50f, 0.50f, 0.70f },
};
inline constexpr int kBalanceWaveBandFirstDefault[kBalanceWaveBands] = { 1, 4, 7, 10 };

// The live table: the defaults plus any overrides loaded this session.
inline BalanceTuning g_Balance[kBalanceDifficulties] = {
    kBalanceDefaults[0], kBalanceDefaults[1], kBalanceDefaults[2], kBalanceDefaults[3]
};
inline int g_BalanceWaveBandFirst[kBalanceWaveBands] = { 1, 4, 7, 10 };

// difficulty: 0 EASY, 1 NORMAL, 2 HARD, 3 NIGHTMARE.
inline BalanceTuning& BalanceFor(int difficulty) {
    return g_Balance[std::clamp(difficulty, 0, kBalanceDifficulties - 1)];
}

inline void ResetBalance() {
    for (int d = 0; d < kBalanceDifficulties; ++d) g_Balance[d] = kBalanceDefaults[d];
    for (int i = 0; i < kBalanceWaveBands; ++i)
        g_BalanceWaveBandFirst[i] = kBalanceWaveBandFirstDefault[i];
}

// Spawn rate x of the band `wave` falls in (wv1 before the second band).
inline float WaveBandMult(const BalanceTuning& b, int wave) {
    const float bands[kBalanceWaveBands] = { b.wv1, b.wv2, b.wv3, b.wv4 };
    int band = 0;
    for (int i = 1; i < kBalanceWaveBands; ++i)
        if (wave >= g_BalanceWaveBandFirst[i]) band = i;
    return bands[band];
}

// Spawn rate x at `progress` (0..1) through a wave: start -> peak at
// cvPeakAt -> end, joined by straight lines and scaled so the wave average is
// 1, so a wave's total spawns stay the same whatever the shape.
inline float WaveCurveMult(const BalanceTuning& b, float progress) {
    const float p = std::clamp(progress, 0.0f, 1.0f);
    const float at = std::clamp(b.cvPeakAt, 0.01f, 0.99f);
    const float raw = p < at
        ? b.cvStart + (b.cvPeak - b.cvStart) * (p / at)
        : b.cvPeak + (b.cvEnd - b.cvPeak) * ((p - at) / (1.0f - at));
    const float mean = 0.5f * at * (b.cvStart + b.cvPeak) +
                       0.5f * (1.0f - at) * (b.cvPeak + b.cvEnd);
    return mean > 0.0f ? std::max(0.0f, raw / mean) : 1.0f;
}

// ─── Overrides ───────────────────────────────────────────────────────────────
struct BalanceField { const char* name; float BalanceTuning::* member; };
inline constexpr BalanceField kBalanceFields[] = {
    { "xpMult", &BalanceTuning::xpMult },
    { "wv1", &BalanceTuning::wv1 }, { "wv2", &BalanceTuning::wv2 },
    { "wv3", &BalanceTuning::wv3 }, { "wv4", &BalanceTuning::wv4 },
    { "kSwarm", &BalanceTuning::kSwarm }, { "kGimbal", &BalanceTuning::kGimbal },
    { "kGravis", &BalanceTuning::kGravis }, { "kMagnetar", &BalanceTuning::kMagnetar },
    { "scopeInterval", &BalanceTuning::scopeInterval },
    { "bossHp", &BalanceTuning::bossHp },
    { "hpScale", &BalanceTuning::hpScale }, { "hpLate", &BalanceTuning::hpLate },
    { "rangedDmg", &BalanceTuning::rangedDmg },
    { "spawnInterval", &BalanceTuning::spawnInterval },
    { "spawnPressureWave", &BalanceTuning::spawnPressureWave },
    { "cvStart", &BalanceTuning::cvStart }, { "cvPeak", &BalanceTuning::cvPeak },
    { "cvEnd", &BalanceTuning::cvEnd }, { "cvPeakAt", &BalanceTuning::cvPeakAt },
};

// Applies one "key = values" line. Returns false for an unknown key or a
// row without values; the table is left as it was then.
inline bool ApplyBalanceLine(const std::string& line) {
    std::string text = line.substr(0, line.find('#'));
    for (char& c : text) if (c == ',' || c == '=' || c == '\t') c = ' ';
    std::istringstream in(text);
    std::string key;
    if (!(in >> key)) return false;
    float v[kBalanceDifficulties];
    int n = 0;
    while (n < kBalanceDifficulties && in >> v[n]) ++n;
    if (n == 0) return false;
    for (int i = n; i < kBalanceDifficulties; ++i) v[i] = v[n - 1];   // one value fills the row

    if (key == "wvFirst") {
        for (int i = 0; i < kBalanceWaveBands; ++i) g_BalanceWaveBandFirst[i] = (int)v[i];
        return true;
    }
    for (const BalanceField& f : kBalanceFields) {
        if (key != f.name) continue;
        for (int d = 0; d < kBalanceDifficulties; ++d) g_Balance[d].*f.member = v[d];
        return true;
    }
    return false;
}

// Reads overrides from `path`. Returns the rows applied, or -1 without the file.
inline int LoadBalanceFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return -1;
    int applied = 0;
    for (std::string line; std::getline(file, line);)
        if (ApplyBalanceLine(line)) ++applied;
    return applied;
}

// Startup: ASTERIA_BALANCE names the file, otherwise balance.cfg in the
// working directory (the exe folder). No file = the shipped defaults.
inline int LoadBalanceOverrides() {
    std::string path;
#ifdef _MSC_VER
    char* v = nullptr; size_t n = 0;
    if (_dupenv_s(&v, &n, "ASTERIA_BALANCE") == 0 && v) { path = v; std::free(v); }
#else
    if (const char* v = std::getenv("ASTERIA_BALANCE")) path = v;
#endif
    if (path.empty()) path = "balance.cfg";
    return LoadBalanceFile(path);
}
