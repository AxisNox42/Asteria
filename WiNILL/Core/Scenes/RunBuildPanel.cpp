#include "RunBuildPanel.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "SceneUI.h"
#include "SceneInternal.h"
#include "UiLayout.h"
#include "Settings.h"
#include "GameContext.h"
#include "Translations.h"
#include "Augment.h"
#include "PassiveAugments.h"
#include "SynergyRuntime.h"
#include "../../Render/AugCardModel.h"
#include "PlayerStats.h"
#include "RangeUnit.h"
#include "RunStats.h"
#include "Weapons.h"
#include "DrawPrim.h"
#include "../../Render/UiColors.h"
#include "MainShader.h"
#include "TextRenderer.h"
#include "PlayerWeaponShell.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include "WrapRules.h"

namespace run_build {
// The glyph atlases are baked at fixed sizes (S, L, XL) and only ever scaled
// at draw time, so text blurs as soon as it is drawn above its baked size.
// Pick the smallest atlas that can reach the wanted line height without
// being enlarged.

TextPick PickText(const SceneTextContext& t, float lineHeightPx) {
    TextRenderer* order[3] = { &t.body, &t.title, &g_TextXL };
    for (TextRenderer* r : order) {
        const float lh = r->LineHeightPixels();
        if (lh > 0.0f && lineHeightPx <= lh)
            return { r, lineHeightPx / lh };
    }
    const float lh = std::max(1.0f, g_TextXL.LineHeightPixels());
    return { &g_TextXL, lineHeightPx / lh };
}



// Greedy wrap by measured width. Breaks at spaces when it can and between
// characters otherwise (Korean and Japanese run long without spaces).
int WrapLines(TextRenderer& r, float scale, const wchar_t* text, float maxW,
              wchar_t (*lines)[kWrapChars], int maxLines) {
    int count = 0;
    wchar_t cur[kWrapChars] = {};
    int len = 0, lastSpace = -1;
    auto emit = [&](int n) {
        if (count >= maxLines) return;
        while (n > 0 && cur[n - 1] == L' ') --n;
        wmemcpy(lines[count], cur, (size_t)n);
        lines[count][n] = 0;
        ++count;
    };
    for (const wchar_t* p = text; p && *p && count < maxLines; ++p) {
        if (len >= kWrapChars - 2) { emit(len); len = 0; lastSpace = -1; }
        if (*p == L' ') lastSpace = len;
        cur[len++] = *p;
        cur[len] = 0;
        if (len > 1 && r.Width(cur, scale) > maxW) {
            if (lastSpace > 0) {
                emit(lastSpace);
                const int rest = len - lastSpace - 1;
                wmemmove(cur, cur + lastSpace + 1, (size_t)rest);
                len = rest;
            } else {
                // Between characters: step back past places a line may not break
                // (WrapRules.h: "、" / "%" never start a line, numbers stay whole).
                int b = len - 1;   // cur[b..] moves down
                while (b > 0 && wrap_rules::Glued(cur[b - 1], cur[b])) --b;
                if (b == 0) b = len - 1;   // nowhere better: break where it overflowed
                emit(b);
                const int rest = len - b;
                wmemmove(cur, cur + b, (size_t)rest);
                len = rest;
            }
            cur[len] = 0;
            lastSpace = -1;
            for (int i = 0; i < len; ++i) if (cur[i] == L' ') lastSpace = i;
        }
    }
    if (len > 0 && count < maxLines) emit(len);
    return count;
}

// ---- Player stats: weapon and meta, plus trial ----------------------------

float MetricValue(const PlayerStats& s, int m) {
    switch (m) {
    case kAttack:      return s.GetBaseDamage() * s.damageMultiplier;
    case kFireRate:    return s.fireInterval > 0.0001f ? 1.0f / s.fireInterval : 0.0f;
    case kBulletSpeed: return s.bulletSpeed;
    case kMove:        return s.moveSpeedMult * 100.0f;
    case kMaxHp:       return s.maxHP;
    case kRegen:       return s.regenPerSec;
    case kCritChance:  return (float)std::min(100, s.critChance);   // the sum is uncapped; the roll caps it at 100
    case kCritDamage:  return s.critMult * 100.0f;
    default:           return 0.0f;
    }
}


// total = the run's final stats (augments and level-up cards included);
// base = weapon and meta. The gap splits into the run's gains and the trial cut.
void BuildStatRows(StatRow* rows) {
    const PlayerStats base = PreviewBaseStats(g_CurrentWeapon, g_Scale);
    PlayerStats trial = base;
    ApplyTrialMaxHp(trial);
    for (int m = 0; m < kMetricCount; ++m) {
        const float b = MetricValue(base, m);
        const float tr = MetricValue(trial, m);
        rows[m].base = b;
        rows[m].trial = tr - b;
        rows[m].total = MetricValue(g_Stats, m);
        // Attack and fire rate come from the fire path itself, so frames and
        // cards that act there (Minigun, Star Tempo, ...) show up too.
        if (m == kAttack) rows[m].total = passive::ShotDamageNow();
        if (m == kFireRate) {
            const float iv = passive::FireIntervalNow();
            rows[m].total = iv > 0.0001f ? 1.0f / iv : 0.0f;
        }
        rows[m].run = rows[m].total - tr;
    }
}

// Parts smaller than this are not worth a "+ x" term.
float MetricEps(int m) {
    if (m == kRegen) return 0.005f;
    if (m == kBulletSpeed) return 0.05f * kRangeUnit;   // shown in rings, one decimal
    return (m == kCritChance || m == kCritDamage) ? 0.5f : 0.05f;
}
// Values carry their unit so a bare number never reads two ways
// (fire rate 14.3 is shots per second, not one shot per 14 s).
void FormatMetric(wchar_t* out, int m, float v) {
    // Per-second units read "/s" in every language (the user's call).
    switch (m) {
    case kAttack:      swprintf_s(out, 32, L"%.1f", v); break;
    case kFireRate:    swprintf_s(out, 32, L"%.1f/s", v); break;
    case kBulletSpeed: swprintf_s(out, 32, L"%.1f○/s", v / kRangeUnit); break;   // in rings (○)
    case kRegen:       swprintf_s(out, 32, L"%.2f/s", v); break;
    case kMove:       swprintf_s(out, 32, L"%.0f%%", v); break;
    case kCritChance: swprintf_s(out, 32, L"%.0f%%", v); break;
    case kCritDamage: swprintf_s(out, 32, L"%.0f%%", v); break;
    default:          FormatGrouped(std::llround(v), out, 32); break;
    }
}

namespace {
const wchar_t* kBuildStatsLabel[LANG_COUNT] = {
    L"\uD50C\uB808\uC774\uC5B4 \uB2A5\uB825\uCE58", L"PLAYER STATS", L"\u30D7\u30EC\u30A4\u30E4\u30FC\u80FD\u529B\u5024"
};
const wchar_t* kMetricLabel[LANG_COUNT][kMetricCount] = {
    { L"\uACF5\uACA9\uB825", L"\uC5F0\uC0AC \uC18D\uB3C4", L"\uD0C4\uC18D", L"\uC774\uB3D9\uC18D\uB3C4", L"HP", L"\uCCB4\uB825 \uC7AC\uC0DD", L"\uCE58\uBA85\uD0C0 \uD655\uB960", L"\uCE58\uBA85\uD0C0 \uD53C\uD574" },
    { L"ATTACK", L"FIRE RATE", L"BULLET SPEED", L"MOVE SPEED", L"HP", L"HP REGEN", L"CRIT CHANCE", L"CRIT DAMAGE" },
    { L"\u653B\u6483\u529B", L"\u9023\u5C04\u901F\u5EA6", L"\u5F3E\u901F", L"\u79FB\u52D5\u901F\u5EA6", L"HP", L"HP\u56DE\u5FA9", L"\u30AF\u30EA\u30C6\u30A3\u30AB\u30EB\u7387", L"\u30AF\u30EA\u30C6\u30A3\u30AB\u30EB\u30C0\u30E1\u30FC\u30B8" }
};
// Hovering a stat row explains it: what the value means, then how this
// play's value is made up (base, gains, cuts). Fire rate adds its interval.
const wchar_t* kMetricDef[LANG_COUNT][kMetricCount] = {
    { L"일반 탄 1발의 피해. 증강·시너지의 탄 피해 배율은 들어가고, 치명타·거리·표식처럼 맞는 대상마다 다른 보너스는 빠집니다.",
      L"1초에 쏘는 탄 수. 높을수록 빠릅니다.",
      L"탄이 1초에 날아가는 거리. ○ = 체력 링의 반지름.",
      L"기본 이동속도를 100%로 본 비율.",
      L"최대 체력.",
      L"1초마다 저절로 회복하는 HP.",
      L"탄이 치명타가 될 확률. 합이 100%를 넘어도 100%로 칩니다.",
      L"치명타일 때 들어가는 피해. 공격력 대비 비율 (200% = 2배)." },
    { L"Damage of one plain shot, with augment and synergy bullet multipliers. Per-target bonuses (crits, range, marks) are left out.",
      L"Shots fired per second. Higher is faster.",
      L"How far a bullet flies in one second. ○ = the health ring's radius.",
      L"Move speed as a share of the base speed (100%).",
      L"Maximum health.",
      L"HP restored on its own every second.",
      L"Chance for a shot to crit. Past 100% it still counts as 100%.",
      L"Damage on a crit, as a share of attack (200% = double)." },
    { L"通常弾1発のダメージ。強化・シナジーの弾ダメージ倍率は含み、クリティカル・距離・マークなど対象ごとのボーナスは含みません。",
      L"1秒に撃つ弾の数。高いほど速い。",
      L"弾が1秒に飛ぶ距離。○ = 体力リングの半径。",
      L"基本移動速度を100%とした割合。",
      L"最大体力。",
      L"1秒ごとに自然回復するHP。",
      L"弾がクリティカルになる確率。合計が100%を超えても100%として扱います。",
      L"クリティカル時のダメージ。攻撃力に対する割合 (200% = 2倍)。" },
};
const wchar_t* kIntervalFmt[LANG_COUNT] = {   // fire rate: the same value as an interval
    L" 지금 %.2f초마다 1발.", L" Right now: one shot every %.2fs.", L" 現在 %.2f秒ごとに1発。" };
const wchar_t* kBaseFmt[LANG_COUNT] = {
    L"기본 %ls (무기·영구 업그레이드)", L"Base %ls (weapon + permanent upgrades)", L"基本 %ls (武器・恒久強化)" };
const wchar_t* kGainFmt[LANG_COUNT] = {
    L" +%ls (이번 판에 얻음)", L" +%ls gained this play", L" +%ls (このプレイで獲得)" };
const wchar_t* kLossFmt[LANG_COUNT] = {
    L" -%ls (이번 판에 줄어듦)", L" -%ls lost this play", L" -%ls (このプレイで減少)" };
const wchar_t* kTrialFmt[LANG_COUNT] = {
    L" %ls%ls (시련)", L" %ls%ls trial", L" %ls%ls (試練)" };
}  // namespace

void DrawPanel(PanelState& st, const PanelIn& in) {
    const float uiS = in.uiS, ge = in.alpha, delta = in.delta, now = in.now;
    const double mx = in.mx, my = in.my;
    const float panelX = in.panelX, panelW = in.panelW, top = in.top;
    const float accentR = in.accentR, accentG = in.accentG, accentB = in.accentB;
    const int li = std::clamp(LangIndex(), 0, LANG_COUNT - 1);
    const TextPick lab = PickText(*in.text, 26.0f * uiS);
    // Over a live (possibly bright) scene each glyph carries a dark keyline; no plate behind.
    auto drawT = [&](TextRenderer& tr, const wchar_t* str, float x, float y, float sc,
                     float r, float g, float b, float a) {
        if (in.keyline) DrawShadowedText(tr, str, x, y, sc, r, g, b, a, 0.84f);
        else tr.Draw(str, x, y, sc, r, g, b, a);
    };
    const float labH = in.labH > 0.0f ? in.labH : lab.r->Height(kBuildStatsLabel[li], lab.scale);
    const std::vector<int>& taken = passive::AcquiredOrder();
    const int nTaken = (int)taken.size();
    constexpr int kDescLines = 4;   // a stat's meaning plus its breakdown can take four
    if (!st.rowsValid) { BuildStatRows(st.rows); st.rowsValid = true; }

    const bool statRowShown[kMetricCount] = { true, true, true, true, true, true, true, true };
    int statRows = 0;
    for (int m = 0; m < kMetricCount; ++m) if (statRowShown[m]) ++statRows;

    // The panel is a vertical stack; when it would run off the bottom the
    // graphic parts (player, cells, gaps) shrink, text stays at its size.
    float k = 1.0f;
    float shellSize = 0, shellR = 0, shellCy = 0;
    float augLabY = 0, augRuleY = 0, gridY = 0, cell = 0;
    int cols = 1, shown = 0;
    float toggleY = 0, toggleRuleY = 0, tableY0 = 0, rowH = 0;
    for (int pass = 0; pass < 12; ++pass) {
        shellSize = 25.0f * 2.2f * uiS * k;
        shellR = shellSize * 1.2f;
        shellCy = top + shellR;
        // Below the outer orbit arc, so the label never sits on a ring.
        augLabY = in.showShell ? shellCy + shellR * 1.30f + 10.0f * uiS * k : top;
        augRuleY = augLabY + labH + 8.0f * uiS * k;
        gridY = augRuleY + 10.0f * uiS * k;
        cell = 64.0f * uiS * k;
        cols = std::max(1, (int)(panelW / cell));
        const int maxCells = cols * 3;
        shown = nTaken > maxCells ? maxCells - 1 : nTaken;
        const int cells = std::max(1, std::min(nTaken, maxCells));
        const float gridH = (float)((cells + cols - 1) / cols) * cell;
        toggleY = gridY + gridH + 12.0f * uiS * k;
        toggleRuleY = toggleY + labH + 8.0f * uiS * k;
        tableY0 = toggleRuleY + 12.0f * uiS * k;
        rowH = labH + 6.0f * uiS * k;
        const float bottom = tableY0 + (float)statRows * rowH + 14.0f * uiS * k +
                             labH * 1.3f + (float)kDescLines * (labH + 3.0f * uiS * k);
        if (bottom <= in.bottomLimit || k <= 0.50f) break;
        k -= 0.04f;
    }

    // The player's own look, with two slow orbit arcs behind it.
    const float shellCx = panelX + panelW * 0.5f;
    if (in.showShell) {
    BindMainShader();
    DrawSettingsOrbitArc(shellCx, shellCy, shellR, shellR,
                         now * 0.35f, now * 0.35f + 3.6f,
                         accentR, accentG, accentB, 1.4f * uiS, 0.35f * ge);
    DrawSettingsOrbitArc(shellCx, shellCy, shellR * 1.16f, shellR * 1.16f,
                         -now * 0.22f + 2.0f, -now * 0.22f + 4.6f,
                         accentR, accentG, accentB, 1.0f * uiS, 0.22f * ge);
    DrawPlayerWeaponShell(shellCx, shellCy, shellSize, now * 0.6f, now);
    BatchFlush();
    }

    // Cards taken this run, as their family constellations in the
    // rarity colour; hovering one explains it below the stats.
    static const wchar_t* kTakenLabel[LANG_COUNT] = {
        L"\uC5BB\uC740 \uC99D\uAC15", L"AUGMENTS TAKEN", L"\u7372\u5F97\u3057\u305F\u5F37\u5316" };
    static const wchar_t* kNoneLabel[LANG_COUNT] = {
        L"\uC5C6\uC74C", L"NONE", L"\u306A\u3057" };
    drawT(*lab.r, kTakenLabel[li], Snap(panelX), Snap(augLabY), lab.scale,
                accentR, accentG, accentB, 0.92f * ge);
    LogoLine(panelX, augRuleY, panelX + panelW, augRuleY,
             1.0f, accentR, accentG, accentB, 0.24f * ge);
    const bool canHoverCards = in.canHover;
    int hoverAug = -1;
    int hoverStat = -1;
    if (nTaken == 0) {
        drawT(*lab.r, kNoneLabel[li], Snap(panelX), Snap(gridY + cell * 0.5f - labH * 0.5f),
                    lab.scale, 0.72f, 0.82f, 0.92f, 0.90f * ge);
    }
    BindMainShader();
    for (int i = 0; i < shown; ++i) {
        const int idx = taken[i];
        const AugDef& def = ALL_AUGS[idx];
        const float ccx = panelX + ((float)(i % cols) + 0.5f) * cell;
        const float ccy = gridY + ((float)(i / cols) + 0.5f) * cell;
        const float dx = (float)mx - ccx, dy = (float)my - ccy;
        const bool hov = canHoverCards && std::fabs(dx) <= cell * 0.5f && std::fabs(dy) <= cell * 0.5f;
        if (hov) hoverAug = idx;
        float rr, rg, rb;
        GetRarityColor(def.rarity, rr, rg, rb);
        rr = std::min(1.0f, rr * 1.3f + 0.15f);
        rg = std::min(1.0f, rg * 1.3f + 0.15f);
        rb = std::min(1.0f, rb * 1.3f + 0.15f);
        const aug_card::Shape& shp =
            aug_card::ShapeOf(aug_card::FamilyFromCode(def.name));
        const float size = cell * (hov ? 0.78f : 0.70f);
        const float a = (hov ? 1.0f : 0.82f) * ge;
        auto P = [&](int v, float& x, float& y) {
            x = ccx + (shp.x[v] - 0.5f) * size;
            y = ccy + (shp.y[v] - 0.5f) * size;
        };
        for (int e = 0; e < shp.ne; ++e) {
            float x0, y0, x1, y1;
            P(shp.ea[e], x0, y0);
            P(shp.eb[e], x1, y1);
            LogoLine(x0, y0, x1, y1, (hov ? 1.6f : 1.2f) * uiS, rr, rg, rb, a);
        }
        for (int v = 0; v < shp.nv; ++v) {
            float x, y;
            P(v, x, y);
            drawDiamond(x, y, 2.6f * uiS, rr, rg, rb, a);
        }
        // Actives get a small ring under the shape so they read apart.
        if (AugIsActive(def))
            DrawSettingsOrbitArc(ccx, ccy, cell * 0.46f, cell * 0.46f, 0.0f, 6.2832f,
                                 rr, rg, rb, 1.0f * uiS, 0.45f * ge);
    }
    BatchFlush();
    for (int i = 0; i < shown; ++i) {
        // Upgraded cards (augment rework): their upgrade level, top left.
        const int stars = syn::Stage(taken[i]) - 1;
        if (stars > 0) {
            const std::wstring sb = L"+" + std::to_wstring(stars);
            const float scx = panelX + ((float)(i % cols) + 0.5f) * cell;
            const float scy = gridY + ((float)(i / cols) + 0.5f) * cell;
            drawT(*lab.r, sb.c_str(), Snap(scx - cell * 0.5f + 2.0f * uiS), Snap(scy - cell * 0.5f),
                  lab.scale * 0.6f, 1.0f, 0.86f, 0.45f, 0.95f * ge);
        }
        const int n = passive::AcquiredCount(taken[i]);
        if (n <= 1) continue;
        wchar_t cb[16];
        swprintf_s(cb, L"x%d", n);
        const float ccx = panelX + ((float)(i % cols) + 0.5f) * cell;
        const float ccy = gridY + ((float)(i / cols) + 0.5f) * cell;
        const float cw = lab.r->Width(cb, lab.scale * 0.65f);
        drawT(*lab.r, cb, Snap(ccx + cell * 0.5f - cw), Snap(ccy + cell * 0.5f - labH * 0.65f),
                    lab.scale * 0.65f, 1.0f, 1.0f, 1.0f, 0.95f * ge);
    }
    if (nTaken > shown) {
        wchar_t more[16];
        swprintf_s(more, L"+%d", nTaken - shown);
        const float ccx = panelX + ((float)(shown % cols) + 0.5f) * cell;
        const float ccy = gridY + ((float)(shown / cols) + 0.5f) * cell;
        const float mw = lab.r->Width(more, lab.scale);
        drawT(*lab.r, more, Snap(ccx - mw * 0.5f), Snap(ccy - labH * 0.5f), lab.scale,
                    0.64f, 0.76f, 0.88f, 0.90f * ge);
    }

    // Fold-out toggle: a label, a rule and a chevron.
    const bool canHover = in.canHover;
    if (in.statsAlwaysOpen) { st.statsOpen = true; st.statsT = 1.0f; }
    const bool toggleHover = canHover && !in.statsAlwaysOpen &&
        mx >= panelX && mx <= panelX + panelW &&
        my >= toggleY - 6.0f * uiS && my <= toggleRuleY + 6.0f * uiS;
    if (toggleHover && in.click)
        st.statsOpen = !st.statsOpen;
    const float target = st.statsOpen ? 1.0f : 0.0f;
    st.statsT += (target - st.statsT) * (1.0f - std::exp(-delta * 10.0f));
    if (fabsf(target - st.statsT) < 0.002f) st.statsT = target;
    const float openE = Smoothstep(st.statsT);
    const float tA = (toggleHover ? 1.0f : 0.82f) * ge;
    drawT(*lab.r, kBuildStatsLabel[li], Snap(panelX), Snap(toggleY), lab.scale,
                accentR, accentG, accentB, tA);
    const float chevX = panelX + panelW - 12.0f * uiS;
    const float chevY = toggleY + labH * 0.5f;
    const float arm = 7.0f * uiS;
    const float dir = 1.0f - 2.0f * openE;   // 1 = pointing down, -1 = up
    BindMainShader();
    if (!in.statsAlwaysOpen) {   // a table that never folds needs no chevron
        LogoLine(chevX - arm, chevY - arm * 0.5f * dir, chevX, chevY + arm * 0.5f * dir,
                 1.5f * uiS, accentR, accentG, accentB, tA);
        LogoLine(chevX + arm, chevY - arm * 0.5f * dir, chevX, chevY + arm * 0.5f * dir,
                 1.5f * uiS, accentR, accentG, accentB, tA);
    }
    LogoLine(panelX, toggleRuleY, panelX + panelW, toggleRuleY,
             1.0f, accentR, accentG, accentB, (toggleHover ? 0.40f : 0.24f) * ge);

    // Stat rows: total (white) = base (white) + trial (yellow). Every part sits in its own left-aligned column so the
    // rows line up under each other.
    if (openE > 0.01f) {
        const float tableY = tableY0 + (1.0f - openE) * -10.0f * uiS;
        const float a = openE * ge;
        const float wr = 0.95f, wg = 0.97f, wb = 1.0f;       // total, base
        const float yr = 1.0f, yg = 0.84f, yb = 0.30f;       // trial
        const float dr = 0.55f, dg = 0.65f, db = 0.78f;      // operators
        TextPick rp = lab;
        // A value in one colour, its % sign included.
        auto drawValueText = [&](const wchar_t* s, float x, float y,
                                 float cr, float cg, float cb, float al) {
            drawT(*rp.r, s, Snap(x), Snap(y), rp.scale, cr, cg, cb, al);
        };            float opW = 0.0f, eqW = 0.0f;
        float labelColW = 0.0f, totalColW = 0.0f, baseColW = 0.0f;
        // Parts follow the base one after another with the same gap between
        // any two, positives first and nerfs last, whatever kind they are.
        float xPart0 = 0.0f;
        const float partGap = 12.0f * uiS;
        float xTotal = 0.0f, xEq = 0.0f, xBase = 0.0f;
        const float opGap = 5.0f * uiS;
        auto partUsed = [&](int m, float v) { return fabsf(v) >= MetricEps(m); };
        // The row's parts in display order: run gains, then the trial cut.
        auto rowParts = [&](const StatRow& row, int m, float* vals) {
            int n = 0;
            if (partUsed(m, row.run))   vals[n++] = row.run;
            if (partUsed(m, row.trial)) vals[n++] = row.trial;
            return n;
        };
        auto partWidth = [&](int m, float v) {
            wchar_t pb[32];
            FormatMetric(pb, m, fabsf(v));
            return opW + opGap + rp.r->Width(pb, rp.scale);
        };
        auto drawPart = [&](int m, float v, float x, float y, float cr, float cg, float cb) {
            if (!partUsed(m, v)) return;
            // A nerf (a negative part) has a red value, whichever part it is;
            // the + and - signs share the operator colour.
            const bool nerf = v < 0.0f;
            const wchar_t opText[2] = { nerf ? L'-' : L'+', 0 };
            if (nerf) { cr = 1.0f; cg = 0.36f; cb = 0.38f; }
            drawT(*rp.r, opText, Snap(x), Snap(y), rp.scale, dr, dg, db, 0.9f * a);
            wchar_t pb[32];
            FormatMetric(pb, m, fabsf(v));
            drawValueText(pb, x + opW + opGap, y, cr, cg, cb, 0.95f * a);
        };
        // Shrink the text a little until the widest row fits the panel.
        float rowFit = 1.0f;
        for (int pass = 0; pass < 10; ++pass) {
            rp = PickText(*in.text, 26.0f * uiS * rowFit);
            opW = std::max(rp.r->Width(L"+", rp.scale), rp.r->Width(L"-", rp.scale));
            eqW = rp.r->Width(L"=", rp.scale);
            labelColW = totalColW = baseColW = 0.0f;
            for (int m = 0; m < kMetricCount; ++m) {
                if (!statRowShown[m]) continue;
                const StatRow& row = st.rows[m];
                labelColW = std::max(labelColW, rp.r->Width(kMetricLabel[li][m], rp.scale));
                wchar_t fb[32];
                FormatMetric(fb, m, row.total);
                totalColW = std::max(totalColW, rp.r->Width(fb, rp.scale));
                FormatMetric(fb, m, row.base);
                baseColW = std::max(baseColW, rp.r->Width(fb, rp.scale));
            }
            xTotal = panelX + labelColW + 14.0f * uiS;
            xEq = xTotal + totalColW + 10.0f * uiS;
            xBase = xEq + eqW + 8.0f * uiS;
            xPart0 = xBase + baseColW + 12.0f * uiS;
            float partsEnd = xBase + baseColW;
            for (int m = 0; m < kMetricCount; ++m) {
                if (!statRowShown[m]) continue;
                float pv[2];
                const int pn = rowParts(st.rows[m], m, pv);
                float ex = xPart0;
                for (int i = 0; i < pn; ++i) ex += partWidth(m, pv[i]) + partGap;
                if (pn > 0) partsEnd = std::max(partsEnd, ex - partGap);
            }
            if (partsEnd - panelX <= panelW || rowFit <= 0.62f) break;
            rowFit -= 0.05f;
        }
        int shownRows = 0;
        for (int m = 0; m < kMetricCount; ++m) {
            if (!statRowShown[m]) continue;
            const StatRow& row = st.rows[m];
            const float y = tableY + (float)shownRows * rowH;
            ++shownRows;
            const bool rowHov = in.canHover && openE > 0.9f &&
                mx >= panelX && mx <= panelX + panelW &&
                my >= y - 3.0f * uiS && my < y - 3.0f * uiS + rowH;
            if (rowHov) hoverStat = m;
            // The hovered row's name lights up; the meaning shows below the table.
            if (rowHov)
                drawT(*rp.r, kMetricLabel[li][m], Snap(panelX), Snap(y), rp.scale,
                      accentR, accentG, accentB, 1.0f * a);
            else
                drawT(*rp.r, kMetricLabel[li][m], Snap(panelX), Snap(y), rp.scale,
                      0.64f, 0.76f, 0.88f, 0.88f * a);
            wchar_t fb[32];
            FormatMetric(fb, m, row.total);
            drawValueText(fb, xTotal, y, wr, wg, wb, 0.98f * a);
            drawT(*rp.r, L"=", Snap(xEq), Snap(y), rp.scale, dr, dg, db, 0.9f * a);
            FormatMetric(fb, m, row.base);
            drawValueText(fb, xBase, y, wr, wg, wb, 0.95f * a);
            float pv[2];
            const int pn = rowParts(row, m, pv);
            float px = xPart0;
            for (int i = 0; i < pn; ++i) {
                drawPart(m, pv[i], px, y, yr, yg, yb);
                px += partWidth(m, pv[i]) + partGap;
            }
        }
    }
    // Hovering a card explains it right below the stats.
    const float descY = tableY0 + Smoothstep(st.statsT) * (float)statRows * rowH +
                        14.0f * uiS * k;
    // A card or a stat row: whichever the pointer is on (the last one while it fades).
    if (hoverAug >= 0) { st.descAug = hoverAug; st.descStat = -1; }
    if (hoverStat >= 0) st.descStat = hoverStat;
    const bool anyHover = hoverAug >= 0 || hoverStat >= 0;
    st.descT += ((anyHover ? 1.0f : 0.0f) - st.descT) * std::min(1.0f, delta * 12.0f);
    if (st.descStat >= 0 && st.descT > 0.01f) {
        const int m = st.descStat;
        const StatRow& row = st.rows[m];
        const float a = st.descT * ge;
        LogoLine(panelX, descY - 8.0f * uiS * k, panelX + panelW, descY - 8.0f * uiS * k,
                 1.0f, accentR, accentG, accentB, 0.45f * a);
        drawT(*lab.r, kMetricLabel[li][m], Snap(panelX), Snap(descY), lab.scale,
              accentR, accentG, accentB, 0.98f * a);
        // Meaning, then the breakdown: base + gains - cuts (- trial).
        std::wstring body = kMetricDef[li][m];
        wchar_t part[160], num[32];
        if (m == kFireRate && row.total > 0.0001f) {
            swprintf_s(part, kIntervalFmt[li], 1.0f / row.total);
            body += part;
        }
        // The breakdown is its own paragraph so a long meaning never cuts it off.
        std::wstring parts;
        FormatMetric(num, m, row.base);
        swprintf_s(part, kBaseFmt[li], num);
        parts += part;
        if (fabsf(row.run) >= MetricEps(m)) {
            FormatMetric(num, m, fabsf(row.run));
            swprintf_s(part, row.run > 0.0f ? kGainFmt[li] : kLossFmt[li], num);
            parts += part;
        }
        if (fabsf(row.trial) >= MetricEps(m)) {
            FormatMetric(num, m, fabsf(row.trial));
            swprintf_s(part, kTrialFmt[li], row.trial > 0.0f ? L"+" : L"-", num);
            parts += part;
        }
        float ly = descY + labH * 1.3f;
        auto paragraph = [&](const std::wstring& text, int maxLines, float cr, float cg, float cb) {
            wchar_t lines[kDescLines][kWrapChars];
            const int n = WrapLines(*lab.r, lab.scale, text.c_str(), panelW, lines,
                                    std::min(maxLines, kDescLines));
            for (int l = 0; l < n; ++l) {
                drawT(*lab.r, lines[l], Snap(panelX), Snap(ly), lab.scale, cr, cg, cb, 0.92f * a);
                ly += labH + 3.0f * uiS * k;
            }
        };
        paragraph(body, kDescLines, 0.82f, 0.88f, 0.96f);
        ly += 6.0f * uiS * k;
        paragraph(parts, 2, 0.64f, 0.76f, 0.88f);   // the breakdown, a step dimmer
    } else if (st.descAug >= 0 && st.descT > 0.01f) {
        const AugDef& def = ALL_AUGS[st.descAug];
        float rr, rg, rb;
        GetRarityColor(def.rarity, rr, rg, rb);
        const float a = st.descT * ge;
        LogoLine(panelX, descY - 8.0f * uiS * k, panelX + panelW, descY - 8.0f * uiS * k,
                 1.0f, rr, rg, rb, 0.45f * a);
        // An upgraded card: its stars after the name, and the branches it took before its text.
        const int stars = syn::Stage(st.descAug) - 1;
        std::wstring title = def.locName[li];
        if (stars > 0) title += L"  Lv." + std::to_wstring(stars);
        drawT(*lab.r, title.c_str(), Snap(panelX), Snap(descY), lab.scale,
                    0.45f + 0.55f * rr, 0.45f + 0.55f * rg, 0.45f + 0.55f * rb, 0.98f * a);
        std::wstring text;
        const int bits = syn::Branches(st.descAug);
        for (int b = 0; b < 3; ++b)
            if (bits & (1 << b)) text += (b == 2 ? L"◆ " : L"▲ ") + syn::UpgradeSummary(st.descAug, b, li) + L" · ";
        text += def.locDesc[li];
        wchar_t lines[kDescLines][kWrapChars];
        const int n = WrapLines(*lab.r, lab.scale, text.c_str(), panelW, lines, kDescLines);
        float ly = descY + labH * 1.3f;
        for (int l = 0; l < n; ++l) {
            drawT(*lab.r, lines[l], Snap(panelX), Snap(ly), lab.scale,
                        0.82f, 0.88f, 0.96f, 0.92f * a);
            ly += labH + 3.0f * uiS * k;
        }
    }
}

}  // namespace run_build
