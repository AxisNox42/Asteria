#pragma once
// ─────────────────────────────────────────────────────────────
// Skill HUD view model: plain game values → what each key shows (cooldown
// wedge, effect frame, ready pulse). No GL and no game headers, so
// tests/skill_hud_model_test.cpp can build it alone.
//   slot 0 = dash, slots 1..3 = skills 1..3 (inputs index 0..2); the key
//   labels follow the current key bindings (System/KeyBindings.h, which has
//   no GL or input dependencies either)
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <string>
#include "../System/KeyBindings.h"

namespace skill_hud {

// Dash, then one glyph per active skill group (SkillKind in Augment.h).
enum class SkillGlyph { Dash, Projectile, Area, Install, Summon, Move, Heal,
                        Link, Channel, Random, Form };

inline constexpr float kPulseDur = 0.45f;

struct SkillHudInputs {
    bool       equipped[3];
    bool       sealed[3];
    SkillGlyph glyph[3];
    float      cd[3];
    float      cdMax[3];
    float dashCd, dashCdMax;
    bool  dashActive;
    float dashProgress;
    bool  dashSealed;
};

struct SkillSlotView {
    bool empty;
    bool sealed;
    SkillGlyph glyph;
    float cdRemain, cdMax;
    float elapsedFrac;   // 1 = ready
    float effectFrac;    // remaining effect time, 0 = none
    float pulse;         // remaining pulse time, 0 .. kPulseDur
    const wchar_t* key;
    bool showSeconds;    // cooldown seconds drawn over the icon
    bool iconBright;     // full-strength icon; dimmed whenever seconds show
};

struct SkillHudView {
    SkillSlotView slots[4];
};

inline float ElapsedFrac(float cd, float cdMax) {
    if (cd <= 0.0f || !(cdMax > 0.0f)) return 1.0f;
    return std::clamp(1.0f - cd / cdMax, 0.0f, 1.0f);
}

// Ready pulses: a key whose cooldown crosses zero while it keeps the same
// skill. Reset() at a run start; the first Update after it only records the
// previous values.
struct SkillHudPulse {
    float slot[4];
    float prevCd[4];
    bool prevEquipped[4];
    SkillGlyph prevGlyph[4];
    bool primed;

    void Reset() { *this = SkillHudPulse{}; }

    void Update(const SkillHudInputs& in, float delta) {
        if (delta <= 0.0f) return;
        for (float& p : slot) p = std::max(0.0f, p - delta);
        float cd[4];
        bool equipped[4];
        SkillGlyph glyph[4];
        cd[0] = in.dashCd;
        equipped[0] = !in.dashSealed;
        glyph[0] = SkillGlyph::Dash;
        for (int i = 0; i < 3; ++i) {
            cd[i + 1] = in.cd[i];
            equipped[i + 1] = in.equipped[i];
            glyph[i + 1] = in.glyph[i];
        }
        if (primed) {
            for (int i = 0; i < 4; ++i) {
                const bool same = equipped[i] && prevEquipped[i] && glyph[i] == prevGlyph[i];
                if (same && prevCd[i] > 0.0f && cd[i] <= 0.0f) slot[i] = kPulseDur;
            }
        }
        for (int i = 0; i < 4; ++i) {
            prevCd[i] = cd[i];
            prevEquipped[i] = equipped[i];
            prevGlyph[i] = glyph[i];
        }
        primed = true;
    }
};

inline SkillHudView BuildSkillHudView(const SkillHudInputs& in, const SkillHudPulse& pulse) {
    // Labels outlive this call (the view only holds pointers); refreshed every
    // build so a rebind shows up on the next frame.
    static std::wstring keyLabels[4];
    static const GameAction kKeyActions[4] = {
        GameAction::Dash, GameAction::Skill1, GameAction::Skill2, GameAction::Skill3 };
    for (int i = 0; i < 4; ++i) keyLabels[i] = KeyName(ActionKey(kKeyActions[i]));
    SkillHudView v{};

    SkillSlotView& dash = v.slots[0];
    dash.glyph = SkillGlyph::Dash;
    dash.sealed = in.dashSealed;
    dash.empty = in.dashSealed;
    dash.cdRemain = std::max(0.0f, in.dashCd);
    dash.cdMax = in.dashCdMax;
    dash.elapsedFrac = ElapsedFrac(in.dashCd, in.dashCdMax);
    dash.effectFrac = in.dashActive ? std::clamp(1.0f - in.dashProgress, 0.0f, 1.0f) : 0.0f;
    dash.pulse = pulse.slot[0];
    dash.key = keyLabels[0].c_str();

    for (int i = 0; i < 3; ++i) {
        SkillSlotView& s = v.slots[i + 1];
        s.key = keyLabels[i + 1].c_str();
        s.sealed = in.sealed[i];
        if (!in.equipped[i] || in.sealed[i]) {
            s.empty = true;
            s.elapsedFrac = 1.0f;
            continue;
        }
        s.glyph = in.glyph[i];
        s.cdRemain = std::max(0.0f, in.cd[i]);
        s.cdMax = in.cdMax[i];
        s.elapsedFrac = ElapsedFrac(in.cd[i], in.cdMax[i]);
        s.pulse = pulse.slot[i + 1];
    }
    // Seconds sit on the icon, so the icon dims under them.
    for (SkillSlotView& s : v.slots) {
        s.showSeconds = !s.empty && s.elapsedFrac < 1.0f && s.cdRemain > 0.0f;
        s.iconBright = !s.empty && !s.showSeconds;
    }
    return v;
}

}  // namespace skill_hud
