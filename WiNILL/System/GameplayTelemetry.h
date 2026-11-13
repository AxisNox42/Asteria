#pragma once

// Runtime gameplay telemetry.  This intentionally has no _DEBUG/NDEBUG gate:
// development Release builds should produce the same data as Debug builds.

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "ExpSystem.h"
#include "GameLog.h"
#include "Platform.h"
#include "PlayerStats.h"

class GameplayTelemetry {
public:
    // Change this to 60.0f when a once-per-minute sample is more useful.
    static constexpr float kDefaultSampleIntervalSeconds = 10.0f;

    struct StatsSnapshot {
        float baseDamage = 0.0f;
        float effectiveDamage = 0.0f;
        float damageMultiplier = 0.0f;
        float maxHP = 0.0f;
        float bulletSpeed = 0.0f;
        float fireInterval = 0.0f;
        float moveSpeedMult = 0.0f;
        float regenPerSec = 0.0f;
        float xpMult = 0.0f;
        float windowSize = 0.0f;
        float bulletSpread = 0.0f;
        long long killCount = 0;
    };

    GameplayTelemetry()
        : m_sampleIntervalSeconds(kDefaultSampleIntervalSeconds) {}

    void SetSampleIntervalSeconds(float seconds) {
        if (!std::isfinite(seconds)) return;
        m_sampleIntervalSeconds = std::max(1.0f, std::min(seconds, 3600.0f));
    }

    float SampleIntervalSeconds() const { return m_sampleIntervalSeconds; }
    bool IsActive() const { return m_active; }

    static StatsSnapshot Capture(const PlayerStats& stats) {
        StatsSnapshot out;
        out.baseDamage = stats.GetBaseDamage();
        out.effectiveDamage = stats.GetBaseDamage() * stats.GetDamageMultiplier();
        out.damageMultiplier = stats.damageMultiplier;
        out.maxHP = stats.maxHP;
        out.bulletSpeed = stats.bulletSpeed;
        out.fireInterval = stats.fireInterval;
        out.moveSpeedMult = stats.moveSpeedMult;
        out.regenPerSec = stats.regenPerSec;
        out.xpMult = stats.xpMult;
        out.windowSize = stats.windowSize;
        out.bulletSpread = stats.bulletSpread;
        out.killCount = stats.killCount;
        return out;
    }

    void BeginRun(float gameTime, int level, XpAmount xp,
                  const PlayerStats& stats, const char* mode) {
        if (m_active) {
            // A run reset can happen from the menu without a separate end
            // event. Close the old section before starting the new one.
            gamelog::WriteWindow();
            gamelog::Detach();
            m_file << "RUN_END,reason=replaced_by_new_run\n";
            m_file.flush();
            m_file.close();
        }

        // Use an absolute path so this stays beside the executable even when
        // the resource loader changes the process working directory.
        RotateIfLarge(OutputFilePath());
        m_file.open(OutputFilePath(), std::ios::out | std::ios::app);
        if (!m_file.is_open()) {
            m_active = false;
            return;
        }

        m_active = true;
        gamelog::Attach(&m_file);
        ++m_runSerial;
        m_totalExperience = 0;
        m_lastSampleGameTime = gameTime;
        m_lastSampleTotalExperience = 0;
        m_nextSampleGameTime = gameTime + m_sampleIntervalSeconds;
        m_lastLevelUpGameTime = gameTime;
        m_lastLevelUpTotalExperience = 0;

        m_file << "\n============================================================\n";
        m_file << "RUN_BEGIN,serial=" << m_runSerial
               << ",timestamp=" << Timestamp()
               << ",build=" << BuildName()
               << ",mode=" << (mode ? mode : "unknown")
               << ",sample_interval_sec=" << std::fixed << std::setprecision(2)
               << m_sampleIntervalSeconds << "\n";
        m_file << "# SAMPLE fields: game_sec,level,current_xp,required_xp,"
                  "progress_pct,interval_xp,interval_xp_per_sec,"
                  "interval_xp_per_min,total_xp,stats...\n";
        WriteSample(gameTime, level, xp, RequiredForLevel(level), stats, 0, 0.0);
        m_file.flush();
    }

    void RecordExperience(XpAmount amount) {
        if (!m_active || amount <= 0) return;
        m_totalExperience = XpAdd(m_totalExperience, amount);
    }

    void RecordLevelUp(float gameTime, int oldLevel, int newLevel,
                       XpAmount requiredXp, XpAmount xpBefore,
                       XpAmount xpAfter, bool manualShortcut = false) {
        if (!m_active) return;
        const double levelIntervalSeconds = std::max(
            0.0, (double)gameTime - m_lastLevelUpGameTime);
        const XpAmount levelIntervalXp =
            m_totalExperience - m_lastLevelUpTotalExperience;
        const double levelIntervalPerMinute = levelIntervalSeconds > 0.001
            ? XpToDouble(levelIntervalXp) * 60.0 / levelIntervalSeconds : 0.0;
        const double averagePerMinute = gameTime > 0.001f
            ? XpToDouble(m_totalExperience) * 60.0 / (double)gameTime : 0.0;
        m_file << "LEVEL_UP,game_sec=" << std::fixed << std::setprecision(2)
               << gameTime
               << ",level_before=" << oldLevel
               << ",level_after=" << newLevel
               << ",required_xp=" << FormatXp(requiredXp)
               << ",xp_before=" << FormatXp(xpBefore)
               << ",xp_after=" << FormatXp(xpAfter)
               << ",level_interval_sec=" << levelIntervalSeconds
               << ",level_interval_xp=" << FormatXp(levelIntervalXp)
               << ",level_interval_xp_per_min=" << std::setprecision(3) << levelIntervalPerMinute
               << ",run_total_xp=" << FormatXp(m_totalExperience)
               << ",run_average_xp_per_min=" << std::setprecision(3)
               << averagePerMinute
               << ",source=" << (manualShortcut ? "manual_shortcut" : "experience")
               << "\n";
        m_lastLevelUpGameTime = gameTime;
        m_lastLevelUpTotalExperience = m_totalExperience;
        m_file.flush();
    }

    void Update(float gameTime, int level, XpAmount xp, XpAmount requiredXp,
                const PlayerStats& stats) {
        if (!m_active) return;
        // Event lines are buffered; push them out about once a second so a
        // crash loses little.
        if (gameTime - m_lastFlushGameTime >= 1.0f || gameTime < m_lastFlushGameTime) {
            m_file.flush();
            m_lastFlushGameTime = gameTime;
        }
        if (gameTime + 0.0001f < m_nextSampleGameTime) return;

        // Normally only one sample is due per frame. The loop keeps the
        // cadence correct if a debugger or a long frame skips an interval.
        while (gameTime + 0.0001f >= m_nextSampleGameTime) {
            const double intervalSeconds = std::max(
                0.001, (double)gameTime - m_lastSampleGameTime);
            const XpAmount intervalXp = m_totalExperience - m_lastSampleTotalExperience;
            WriteSample(gameTime, level, xp, requiredXp, stats,
                        intervalXp, (double)intervalSeconds);
            m_lastSampleGameTime = gameTime;
            m_lastSampleTotalExperience = m_totalExperience;
            m_nextSampleGameTime += m_sampleIntervalSeconds;
            gamelog::WriteWindow();
        }
        m_file.flush();
    }

    void EndRun(float gameTime, int level, XpAmount xp,
                const PlayerStats& stats, const char* reason) {
        if (!m_active) return;

        if (gameTime > m_lastSampleGameTime + 0.01f) {
            const double intervalSeconds = (double)gameTime - m_lastSampleGameTime;
            const XpAmount intervalXp = m_totalExperience - m_lastSampleTotalExperience;
            WriteSample(gameTime, level, xp, RequiredForLevel(level), stats,
                        intervalXp, intervalSeconds);
        }

        const double averagePerMinute = gameTime > 0.001f
            ? XpToDouble(m_totalExperience) * 60.0 / (double)gameTime : 0.0;
        gamelog::WriteWindow();
        gamelog::Detach();
        m_file << "RUN_END,game_sec=" << std::fixed << std::setprecision(2)
               << gameTime << ",level=" << level << ",current_xp=" << FormatXp(xp)
               << ",total_xp=" << FormatXp(m_totalExperience)
               << ",average_xp_per_min=" << std::setprecision(3) << averagePerMinute
               << ",reason=" << (reason ? reason : "unknown") << "\n";
        m_file << "============================================================\n";
        m_file.flush();
        m_file.close();
        m_active = false;
    }

private:
    static std::string OutputFilePath() {
        const std::string exeDir = PlatformExeDirectory();
        if (exeDir.empty()) return "asteria_gameplay_telemetry.txt";
        return exeDir + "/asteria_gameplay_telemetry.txt";
    }

    // The log is always on and appends across runs, so past 16 MB the old file
    // moves to ".old" (replacing the previous one) and a fresh file starts.
    static void RotateIfLarge(const std::string& path) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size < 16u * 1024u * 1024u) return;
        std::filesystem::rename(path, path + ".old", ec);
        if (ec) {   // rename over an existing .old fails on some systems
            std::filesystem::remove(path + ".old", ec);
            std::filesystem::rename(path, path + ".old", ec);
        }
    }

    static const char* BuildName() {
#ifdef NDEBUG
        return "Release";
#else
        return "Debug";
#endif
    }

    static std::string Timestamp() {
        const std::time_t now = std::time(nullptr);
        std::tm local = {};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        std::ostringstream out;
        out << std::put_time(&local, "%Y-%m-%d_%H-%M-%S");
        return out.str();
    }

    static XpAmount RequiredForLevel(int level) {
        if (level < 1) return 0;
        return g_ExpSystem.RequiredAmount(level);
    }

    void WriteSample(float gameTime, int level, XpAmount xp,
                     XpAmount requiredXp, const PlayerStats& stats,
                     XpAmount intervalXp, double intervalSeconds) {
        const double progress = requiredXp > 0
            ? std::max(0.0, std::min(100.0,
                (double)xp * 100.0 / (double)requiredXp)) : 0.0;
        const double xpPerSecond = intervalSeconds > 0.0
            ? XpToDouble(intervalXp) / intervalSeconds : 0.0;
        const double xpPerMinute = xpPerSecond * 60.0;

        const StatsSnapshot s = Capture(stats);
        m_file << "SAMPLE,game_sec=" << std::fixed << std::setprecision(2)
               << gameTime << ",level=" << level
               << ",current_xp=" << FormatXp(xp)
               << ",required_xp=" << FormatXp(requiredXp)
               << ",progress_pct=" << std::setprecision(2) << progress
               << ",interval_xp=" << FormatXp(intervalXp)
               << ",interval_xp_per_sec=" << std::setprecision(3) << xpPerSecond
               << ",interval_xp_per_min=" << xpPerMinute
               << ",total_xp=" << FormatXp(m_totalExperience)
               << ",max_hp=" << s.maxHP
               << ",damage_multiplier=" << s.damageMultiplier
               << ",effective_damage=" << s.effectiveDamage
               << ",fire_interval_sec=" << s.fireInterval
               << ",move_speed_multiplier=" << s.moveSpeedMult
               << ",regen_per_sec=" << s.regenPerSec
               << ",xp_multiplier=" << s.xpMult
               << ",kill_count=" << s.killCount << "\n";
    }

    std::ofstream m_file;
    bool m_active = false;
    unsigned long long m_runSerial = 0;
    float m_sampleIntervalSeconds = kDefaultSampleIntervalSeconds;
    float m_nextSampleGameTime = 0.0f;
    double m_lastSampleGameTime = 0.0;
    XpAmount m_totalExperience = 0;
    XpAmount m_lastSampleTotalExperience = 0;
    float m_lastFlushGameTime = 0.0f;
    double m_lastLevelUpGameTime = 0.0;
    XpAmount m_lastLevelUpTotalExperience = 0;
};
