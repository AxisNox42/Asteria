// F1 debug console > 능력치 tab.
// The console changes the stats themselves; cards and level-ups keep adding on
// top. What the console added is remembered per row so 원래 takes back only that.
#include "DebugToolkit.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

#include "DebugUI.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "Settings.h"

extern TextRenderer g_TextS;

namespace {

std::wstring FormatValue(double v, bool integral) {
    wchar_t buf[64] = {};
    if (integral) std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%lld", (long long)std::llround(v));
    else std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%.4g", v);
    return buf;
}

std::wstring FormatDelta(double v, bool integral) {
    std::wstring s = FormatValue(v, integral);
    return v > 0.0 ? L"+" + s : s;
}

} // namespace

void DebugToolkit::TabStats(DebugUI& ui) {
    if (!context_.stats || !context_.game) return;
    GameManager& gm = *context_.game;
    if (fieldDelta_.size() != Fields().size()) fieldDelta_.assign(Fields().size(), 0.0);

    // Runtime values: click a cell to type a number.
    ui.Section(L"현재 상태", L"칸을 누르고 숫자 입력");
    {
        const std::wstring v[4] = {
            L"체력  " + FormatValue(gm.playerHP, false) + L" / " + FormatValue(context_.stats->maxHP, false),
            L"레벨  " + std::to_wstring(gm.playerLevel),
            L"경험치  " + std::to_wstring(gm.xp),
            L"점수  " + std::to_wstring(gm.score) };
        for (int r = 0; r < 2; ++r) {
            const DebugRect row = ui.Row(32.0f);
            for (int k = 0; k < 2; ++k) {
                const int i = r * 2 + k;
                const bool active = editing_ && editingRuntime_ == i;
                const std::wstring label = active ? v[i].substr(0, v[i].find(L"  ")) + L"  > " +
                    (editBuffer_.empty() ? std::wstring(L"_") : editBuffer_) : v[i];
                if (ui.Button(DebugUI::Cell(row, 2, k), label.c_str(), active, true, nullptr, 0.46f))
                    BeginRuntimeEdit(i);
            }
        }
        const DebugRect row = ui.Row(30.0f);
        if (ui.Button(DebugUI::Cell(row, 2, 0), L"체력 가득")) {
            gm.playerHP = context_.stats->maxHP;
            if (context_.syncRuntime) context_.syncRuntime();
            SetMessage(L"체력을 채웠습니다");
        }
        bool anyChanged = false;
        for (double d : fieldDelta_) anyChanged |= d != 0.0;
        if (ui.Button(DebugUI::Cell(row, 2, 1), L"능력치 전부 되돌리기", false, anyChanged)) {
            for (int i = 0; i < (int)Fields().size(); ++i)
                if (fieldDelta_[(size_t)i] != 0.0) NudgeField(i, -fieldDelta_[(size_t)i]);
            SetMessage(L"콘솔에서 바꾼 능력치를 모두 되돌렸습니다");
        }
    }

    // Stats by group: name | − | value | + | ↺
    static const wchar_t* const kGroup[4] = { L"공격", L"생존", L"이동 · 크기", L"성장" };
    const auto& fields = Fields();
    for (int g = 0; g < 4; ++g) {
        ui.Section(kGroup[g], g == 0 ? L"-/+ 한 칸 · 값 누르면 직접 입력 · 원래 = 콘솔 변경만 되돌림" : nullptr);
        for (int i = 0; i < (int)fields.size(); ++i) {
            const Field& f = fields[(size_t)i];
            if (f.group != g) continue;
            const DebugRect row = ui.Row(30.0f, 4.0f);
            const bool integral = f.type != FieldType::Float;
            const double delta = fieldDelta_[(size_t)i];
            const float nameW = row.w * 0.42f;
            const float btnW = 30.0f;
            const float resetW = 44.0f;
            const float valueW = row.w - nameW - btnW * 2.0f - resetW - 4.0f * 6.0f;
            float x = row.x + nameW + 6.0f;
            const DebugRect minus{ x, row.y, btnW, row.h };   x += btnW + 4.0f;
            const DebugRect value{ x, row.y, valueW, row.h }; x += valueW + 4.0f;
            const DebugRect plus{ x, row.y, btnW, row.h };    x += btnW + 4.0f;
            const DebugRect reset{ x, row.y, resetW, row.h };

            if (ui.Drawing() && ui.Visible(row)) {
                const bool changed = delta != 0.0;
                ui.Text(row.x + 2.0f, row.y + 6.0f,
                        DebugFitText(f.name, 0.48f, nameW - 4.0f).c_str(), 0.48f,
                        changed ? 1.0f : debugui_style::kText.r, changed ? 0.85f : debugui_style::kText.g,
                        changed ? 0.45f : debugui_style::kText.b, 1.0f);
            }
            if (ui.Button(minus, L"-")) NudgeField(i, -f.step);
            const bool active = editing_ && editingRuntime_ < 0 && selectedField_ == i;
            std::wstring shown = active ? (editBuffer_.empty() ? std::wstring(L"_") : editBuffer_)
                                        : FormatValue(ReadField(f), integral);
            if (!active && delta != 0.0) shown += L"  (" + FormatDelta(delta, integral) + L")";
            static const DebugColor kChanged{ 1.0f, 0.82f, 0.40f };
            if (ui.Button(value, shown.c_str(), active, true, delta != 0.0 ? &kChanged : nullptr, 0.48f))
                BeginFieldEdit(i);
            if (ui.Button(plus, L"+")) NudgeField(i, f.step);
            if (ui.Button(reset, L"원래", false, delta != 0.0, nullptr, 0.46f)) {
                NudgeField(i, -delta);
                SetMessage(std::wstring(f.name) + L" 되돌림");
            }
        }
    }

    // Cheats and switches.
    ui.Section(L"치트 · 토글");
    {
        const DebugRect row = ui.Row(30.0f);
        if (ui.Toggle(DebugUI::Cell(row, 2, 0), L"무적", g_CreativeGodmode)) {
            g_CreativeGodmode = !g_CreativeGodmode;
            SetMessage(g_CreativeGodmode ? L"무적 켜짐" : L"무적 꺼짐");
        }
        const bool hs = context_.eliteKillHitStop && *context_.eliteKillHitStop;
        if (ui.Toggle(DebugUI::Cell(row, 2, 1), L"엘리트 처치 히트스톱", hs,
                      context_.eliteKillHitStop != nullptr))
            *context_.eliteKillHitStop = !hs;
    }
    {
        const DebugRect row = ui.Row(30.0f);
        const bool orb = context_.constellationOrbits && *context_.constellationOrbits;
        if (ui.Toggle(DebugUI::Cell(row, 2, 0), L"무기 별자리 궤도", orb,
                      context_.constellationOrbits != nullptr))
            *context_.constellationOrbits = !orb;
        const bool bal = context_.balanceTestMode && *context_.balanceTestMode;
        if (ui.Toggle(DebugUI::Cell(row, 2, 1), L"밸런스 로그 기록", bal,
                      context_.balanceTestMode != nullptr)) {
            *context_.balanceTestMode = !bal;
            SetMessage(!bal ? L"밸런스 테스트 시작 - 로그 기록 켜짐"
                            : L"밸런스 테스트 중지 - 로그 기록 꺼짐");
        }
    }
    {
        const DebugRect row = ui.Row(32.0f);
        const bool running = gm.currentState == GameState::RUNNING;
        if (ui.Button(DebugUI::Cell(row, 2, 0), L"다음 웨이브", false,
                      context_.advanceWave && running)) {
            context_.advanceWave();
            SetMessage(gm.waveVictory ? L"마지막 웨이브 완료 - 승리" : L"다음 웨이브로 이동했습니다");
            if (gm.waveVictory) SetVisible(false);
        }
        static const DebugColor kDanger{ 1.0f, 0.42f, 0.42f };
        if (visible_ && ui.Button(DebugUI::Cell(row, 2, 1), L"즉사", false,
                                  context_.killPlayer && running, &kDanger)) {
            context_.killPlayer();
            SetMessage(L"즉사 처리했습니다");
            SetVisible(false);   // close so the death plays out
        }
    }
}
