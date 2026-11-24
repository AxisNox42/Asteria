// Elite phase audit (Docs/Plans/2026-11-17_엘리트_페이즈패턴.md): every elite
// at phases 1-3, 60 s each, against a player who keeps circling (dodger) and
// one who stands still. Prints, per run: share of time some threat is live,
// the longest quiet gap, pattern counts and damage taken by pattern; and
// fails on a bug: a non-finite position, a state stuck for too long, an
// Antares outside the arena, a phase pattern that never ran.
// Run with scripts/test_elite_phase_audit.ps1.
#include "MonsterManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include "audio_stub.h"

static constexpr float DT = 1.0f / 120.0f;
static constexpr float RUN = 60.0f;
static constexpr float ARENA_W = 1920.0f, ARENA_H = 1080.0f;
static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static bool Threat(const MonsterManager& mm, const Monster* e) {
    switch (e->kind) {
    case MobKind::QUASAR:
        return e->QuasarPrecharge() > 0.0f || (e->quasarState >= 1 && e->quasarState <= 3) ||
               e->QuasarFallShots() || e->quasarEruptActive;
    case MobKind::ANTARES: {
        using S = Monster::AntaresState;
        const S s = e->antaresState;
        return s == S::CHASE && e->antaresMoveSpeed >= 200.0f;
    }
    case MobKind::GENESIS: {
        if (e->hivePhase != 0) return true;
        for (float a : e->genesisMarkAge) if (a >= 0.0f && a < Monster::GENESIS_MARK_TIME) return true;
        for (auto* c : mm.monsters)
            if (c->alive && c->thrownOwnerId == e->genesisId &&
                (c->genesisRotorPhase == 2 || c->thrownPhase == 1)) return true;
        return false;
    }
    case MobKind::REGULUS:
        return !mm.regulusPending.empty() || !mm.regulusWaves.empty() || e->regulusExposed > 0.0f;
    default: return false;
    }
}

static int StateKey(const Monster* e) {
    switch (e->kind) {
    case MobKind::QUASAR: return e->quasarState * 10 + (e->quasarSideFall ? 1 : 0);
    case MobKind::ANTARES: return (int)e->antaresState;
    case MobKind::GENESIS: return e->hivePhase;
    default: return 0;
    }
}

struct Result { float threat, gap, taken; std::string patterns, takenBy; };

static Result Run(MobKind kind, int phase, bool dodge) {
    antares_wake::Reset();
    MonsterManager mm;
    std::vector<Bullet> bullets;
    auto* e = new Monster(1300.0f, 540.0f);
    e->MakeKind(kind);
    e->hp = 1.0e9f;
    e->gatesPassed = phase - 1;
    e->spawnAnchorX = 1300.0f; e->spawnAnchorY = 540.0f;
    mm.monsters.push_back(e);
    // Regulus needs allies to buff (and, in phase 3, to lose).
    auto addAlly = [&](float a) {
        auto* m = new Monster(e->worldX + cosf(a) * 220.0f, e->worldY + sinf(a) * 220.0f);
        m->MakeKind(MobKind::ROTOR);
        m->contactDmg = 0.0f;      // allies walk at the player like real ones, but only the lasers count
        m->hp = 1.0e9f;
        mm.monsters.push_back(m);
    };
    if (kind == MobKind::REGULUS) for (int i = 0; i < 6; ++i) addAlly(i * 1.047f);

    const char* logPath = "elite_phase_audit_log.txt";
    std::ofstream log(logPath, std::ios::out | std::ios::trunc);
    gamelog::Attach(&log);
    gamelog::SetClock(0.0f, 4, 100.0f, 100.0f);
    gamelog::FightBegin(e, MobKindName(kind), 1000.0f);

    float hp = 1.0e9f, px = 700.0f, py = 540.0f, ang = 0.0f;
    float threatTime = 0.0f, gap = 0.0f, worstGap = 0.0f, still = 0.0f, worstStill = 0.0f;
    float killTimer = 0.0f;
    int key = StateKey(e);
    bool badPos = false, outside = false;
    for (float t = 0.0f; t < RUN; t += DT) {
        if (dodge) {   // a circle round the arena centre, 260 px/s
            ang += 260.0f / 380.0f * DT;
            px = 960.0f + cosf(ang) * 380.0f;
            py = 540.0f + sinf(ang) * 380.0f;
        }
        gamelog::SetClock(t, 4, 100.0f, 100.0f);
        mm.UpdateAll(px, py, DT, hp, bullets);
        e->ConfineAntares(0.0f, 0.0f, ARENA_W, ARENA_H);
        e->hp = 1.0e9f;
        // Regulus phase 3 needs a buffed ally to fall now and then.
        if (kind == MobKind::REGULUS && (killTimer += DT) >= 5.0f) {
            killTimer = 0.0f;
            for (auto* m : mm.monsters)
                if (m->alive && m != e && m->RegulusBuffed()) { m->alive = false; addAlly(t); break; }
        }
        if (Threat(mm, e)) { threatTime += DT; gap = 0.0f; }
        else worstGap = std::max(worstGap, gap += DT);
        const int k = StateKey(e);
        if (k == key) worstStill = std::max(worstStill, still += DT); else { key = k; still = 0.0f; }
        badPos = badPos || !std::isfinite(e->worldX) || !std::isfinite(e->worldY);
        if (kind == MobKind::ANTARES && e->antaresInArena)
            outside = outside || e->worldX < -1.0f || e->worldX > ARENA_W + 1.0f ||
                      e->worldY < -1.0f || e->worldY > ARENA_H + 1.0f;
    }
    gamelog::SetClock(RUN, 4, 100.0f, 100.0f);
    gamelog::FightEnd("audit");
    gamelog::Detach();
    log.close();
    std::ifstream in(logPath);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::remove(logPath);
    auto field = [&](const char* key) {
        const size_t at = text.find(std::string(",") + key + "=");
        if (at == std::string::npos) return std::string("-");
        const size_t s = at + std::strlen(key) + 2, end = text.find_first_of(",\n", s);
        return text.substr(s, end - s);
    };
    Result r{ threatTime / RUN, worstGap, 0.0f, field("patterns"), field("taken_by_pattern") };
    r.taken = std::strtof(field("taken").c_str(), nullptr);
    const char* who = MobKindName(kind);
    CHECK(!badPos, "%s p%d: non-finite position", who, phase);
    CHECK(!outside, "%s p%d: left the arena", who, phase);
    CHECK(worstStill < (kind == MobKind::REGULUS ? 1e9f : 12.0f), "%s p%d: state stuck %.1f s", who, phase, worstStill);
    return r;
}

int main() {
    const MobKind kinds[] = { MobKind::QUASAR, MobKind::ANTARES, MobKind::GENESIS, MobKind::REGULUS };
    const char* needs[4][3] = {
        { "BEAM", "BEAM", "BEAM_STARFALL" },
        { nullptr, nullptr, nullptr },   // Antares only runs (stop-then-dash patterns removed)
        { "ROTOR", "ROTOR_LANE", "ROTOR_LANE" },
        { nullptr, "LASER_LINES", "EXPOSED" },
    };
    // HARD is the reference: every phase pattern on (EliteDifficulty.h).
    g_Difficulty = Difficulty::HARD;
    std::printf("%-8s ph  player  threat  gap(s)  taken  patterns | taken by\n", "elite");
    for (int i = 0; i < 4; ++i)
        for (int phase = 1; phase <= 3; ++phase)
            for (int dodge = 1; dodge >= 0; --dodge) {
                const Result r = Run(kinds[i], phase, dodge == 1);
                std::printf("%-8s %d   %-6s  %5.0f%%  %6.1f  %5.0f  %s | %s\n", MobKindName(kinds[i]), phase,
                            dodge ? "dodge" : "stand", r.threat * 100.0f, r.gap, r.taken,
                            r.patterns.c_str(), r.takenBy.c_str());
                if (needs[i][phase - 1])
                    CHECK(r.patterns.find(needs[i][phase - 1]) != std::string::npos,
                          "%s p%d: %s never ran", MobKindName(kinds[i]), phase, needs[i][phase - 1]);
            }
    // NIGHTMARE Antares: the same fight plus the friction spray.
    g_Difficulty = Difficulty::NIGHTMARE;
    for (int phase = 1; phase <= 3; ++phase)
        for (int dodge = 1; dodge >= 0; --dodge) {
            const Result r = Run(MobKind::ANTARES, phase, dodge == 1);
            std::printf("ANTARES! %d   %-6s  %5.0f%%  %6.1f  %5.0f  %s | %s\n", phase,
                        dodge ? "dodge" : "stand", r.threat * 100.0f, r.gap, r.taken,
                        r.patterns.c_str(), r.takenBy.c_str());
            // Printed only: the audit's slow circle is caught before a run gets
            // fast (the spray itself is in tests/antares_difficulty_test.cpp).
        }
    // NIGHTMARE Regulus: the royal wave from phase 2.
    for (int phase = 2; phase <= 3; ++phase)
        for (int dodge = 1; dodge >= 0; --dodge) {
            const Result r = Run(MobKind::REGULUS, phase, dodge == 1);
            std::printf("REGULUS! %d   %-6s  %5.0f%%  %6.1f  %5.0f  %s | %s\n", phase,
                        dodge ? "dodge" : "stand", r.threat * 100.0f, r.gap, r.taken,
                        r.patterns.c_str(), r.takenBy.c_str());
            CHECK(r.patterns.find("WAVE") != std::string::npos, "REGULUS nightmare p%d: WAVE never ran", phase);
        }
    g_Difficulty = Difficulty::HARD;
    if (g_fail) { std::printf("elite phase audit: %d problem(s)\n", g_fail); return 1; }
    std::printf("elite phase audit passed\n");
    return 0;
}
