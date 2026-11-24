// F1 debug console > 소환 tab: one click on a card spawns it.
#include "DebugToolkit.h"

#include <algorithm>
#include <cwchar>
#include <string>

#include "DebugUI.h"
#include "DrawPrim.h"
#include "TextRenderer.h"

extern TextRenderer g_TextS;

namespace {

// Spawn targets (ids match DebugToolkit::MobLabel):
//   0-9 mobs, 10 NORMA, 11 NORMA's test window, 12 CAELUM, 13 LYNX, 14 MIRA,
//   15 TRIANGULUM, 16 dummy, 17 remove dummies, 18 HOROLOGIUM.
struct SpawnEntry {
    int id;
    const wchar_t* sub;   // code name / role under the label
};

const SpawnEntry kRegular[] = {
    { 0, L"ROTOR" }, { 2, L"SWARM" }, { 3, L"GRAVIS" }, { 5, L"SCOPE · 원거리" },
    { 6, L"GIMBAL" }, { 9, L"MAGNETAR" }, { 13, L"저격" }, { 14, L"MIRA" },
    { 15, L"TRIANGULUM" } };
const SpawnEntry kElite[] = {
    { 1, L"GENESIS" }, { 4, L"QUASAR" }, { 7, L"지원 엘리트" }, { 8, L"ANTARES" },
    { 18, L"시계 엘리트" } };
const SpawnEntry kBoss[] = {
    { 10, L"1보스 · NORMA" }, { 12, L"2보스 · CAELUM" }, { 11, L"NORMA 테스트 창" } };
const SpawnEntry kTest[] = {
    { 16, L"피해 측정용" }, { 17, L"전부 없앰" } };

int Entries(int category, const SpawnEntry*& out) {
    switch (category) {
    case 1: out = kElite; return (int)(sizeof(kElite) / sizeof(kElite[0]));
    case 2: out = kBoss;  return (int)(sizeof(kBoss) / sizeof(kBoss[0]));
    case 3: out = kTest;  return (int)(sizeof(kTest) / sizeof(kTest[0]));
    default: out = kRegular; return (int)(sizeof(kRegular) / sizeof(kRegular[0]));
    }
}

const DebugColor kCategoryColor[4] = {
    { 0.45f, 0.85f, 1.00f },   // regular
    { 1.00f, 0.72f, 0.40f },   // elite
    { 1.00f, 0.45f, 0.55f },   // boss
    { 0.60f, 0.90f, 0.60f },   // test
};

bool CountsMatter(int id) { return id != 10 && id != 11 && id != 12 && id != 17; }

} // namespace

void DebugToolkit::SpawnTarget(int id) {
    const DebugToolkitContext& c = context_;
    auto mob = [&](MobKind k) { return c.spawnMob ? c.spawnMob((int)k, spawnCount_) : 0; };
    int spawned = 0;
    switch (id) {
    case 0:  spawned = mob(MobKind::ROTOR); break;
    case 1:  spawned = mob(MobKind::GENESIS); break;
    case 2:  spawned = mob(MobKind::SWARM); break;
    case 3:  spawned = mob(MobKind::GRAVIS); break;
    case 4:  spawned = mob(MobKind::QUASAR); break;
    case 5:  spawned = c.spawnRanged ? c.spawnRanged(spawnCount_) : 0; break;
    case 6:  spawned = mob(MobKind::GIMBAL); break;
    case 7:  spawned = mob(MobKind::REGULUS); break;
    case 8:  spawned = mob(MobKind::ANTARES); break;
    case 9:  spawned = mob(MobKind::MAGNETAR); break;
    case 13: spawned = mob(MobKind::LYNX); break;
    case 14: spawned = mob(MobKind::MIRA); break;
    case 15: spawned = mob(MobKind::TRIANGULUM); break;
    case 18: spawned = mob(MobKind::HOROLOGIUM); break;
    case 10: spawned = c.spawnBoss ? c.spawnBoss(0) : 0; break;
    case 12: spawned = c.spawnBoss ? c.spawnBoss(1) : 0; break;
    case 16: spawned = c.spawnDummy ? c.spawnDummy(spawnCount_) : 0; break;
    case 17: {
        const int removed = c.clearDummies ? c.clearDummies() : 0;
        SetMessage(removed > 0 ? L"허수아비를 모두 제거했습니다 x" + std::to_wstring(removed)
                               : L"제거할 허수아비가 없습니다");
        return;
    }
    case 11:
        SetMessage(c.openTestPane && c.openTestPane() ? L"테스트 창을 열었습니다"
                                                      : L"테스트 창을 열 수 없습니다");
        return;
    default: return;
    }
    if (spawned <= 0) {
        const GameState st = c.game ? c.game->currentState : GameState::MAIN_MENU;
        if (st != GameState::RUNNING) SetMessage(L"소환 실패: 게임 진행 중에만 소환됩니다");
        else if (id == 10 || id == 12) SetMessage(L"소환 실패: 보스가 이미 필드에 있습니다");
        else SetMessage(L"소환 실패: 몬스터 수용 한도에 도달했습니다");
        return;
    }
    SetMessage(std::wstring(L"소환: ") + MobLabel(id) + L" x" + std::to_wstring(spawned));
}

// Spawns the armed mob where the field was clicked (screen pixels).
void DebugToolkit::SpawnArmedAt(float screenX, float screenY) {
    if (armedMob_ < 0) return;
    if (!context_.setSpawnPoint || !context_.spawnPlacement) { SpawnTarget(armedMob_); return; }
    context_.setSpawnPoint(screenX, screenY);
    const int prev = *context_.spawnPlacement;
    *context_.spawnPlacement = 2;
    SpawnTarget(armedMob_);
    *context_.spawnPlacement = prev;
}

void DebugToolkit::TabSpawn(DebugUI& ui) {
    using namespace debugui_style;
    // Armed: what a field click will do, and how to stop.
    if (armedMob_ >= 0) {
        const DebugRect band = ui.Row(34.0f, 4.0f);
        if (ui.Drawing() && ui.Visible(band)) {
            ui.Fill(band, kSelected.r, kSelected.g, kSelected.b, 1.0f);
            std::wstring t = std::wstring(MobLabel(armedMob_));
            if (spawnCount_ > 1) t += L" x" + std::to_wstring(spawnCount_);
            t += L" · 필드 클릭/끌기 · 우클릭 취소";
            ui.Text(band.x + 10.0f, band.y + 8.0f, DebugFitText(t.c_str(), 0.48f, band.w - 20.0f).c_str(),
                    0.48f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        const DebugRect row = ui.Row(30.0f);
        if (ui.Button(DebugUI::Cell(row, 2, 0), L"가장자리에 소환 (웨이브처럼)", false, true, nullptr, 0.46f))
            SpawnTarget(armedMob_);
        if (ui.Button(DebugUI::Cell(row, 2, 1), L"대기 취소", false, true, nullptr, 0.46f)) {
            armedMob_ = -1;
            SetMessage(L"소환 대기를 취소했습니다");
        }
    }

    // Category.
    ui.Section(L"분류");
    {
        static const wchar_t* const kCat[4] = { L"일반", L"엘리트", L"보스", L"테스트" };
        const DebugRect row = ui.Row(32.0f);
        for (int i = 0; i < 4; ++i)
            if (ui.Button(DebugUI::Cell(row, 4, i), kCat[i], spawnCategory_ == i, true,
                          &kCategoryColor[i])) {
                spawnCategory_ = i;
                lastMessage_.clear();
            }
    }

    // Count.
    ui.Section(L"수량");
    {
        const DebugRect row = ui.Row(32.0f);
        if (ui.Button(DebugUI::Cell(row, 7, 0), L"-")) spawnCount_ = std::max(1, spawnCount_ - 1);
        const DebugRect value = DebugUI::Cell(row, 7, 1);
        if (ui.Drawing()) {
            ui.Frame(value, kLine.r, kLine.g, kLine.b, 1.0f);
            ui.TextCentered(value, std::to_wstring(spawnCount_).c_str(), 0.56f,
                            kText.r, kText.g, kText.b, 1.0f);
        }
        if (ui.Button(DebugUI::Cell(row, 7, 2), L"+")) spawnCount_ = std::min(20, spawnCount_ + 1);
        static const int kQuick[] = { 1, 3, 5, 10 };
        for (int i = 0; i < 4; ++i)
            if (ui.Button(DebugUI::Cell(row, 7, 3 + i), (L"x" + std::to_wstring(kQuick[i])).c_str(),
                          spawnCount_ == kQuick[i]))
                spawnCount_ = kQuick[i];
    }

    // Targets: a click arms the mob; a field click then spawns it there.
    // Bosses, the test window and "remove dummies" have no position: they act at once.
    ui.Section(L"대상", L"누른 뒤 필드 클릭 = 그 자리에 소환");
    {
        const SpawnEntry* list = nullptr;
        const int n = Entries(spawnCategory_, list);
        const DebugColor& accent = kCategoryColor[spawnCategory_];
        for (int i = 0; i < n; i += 2) {
            const DebugRect row = ui.Row(50.0f);
            for (int k = 0; k < 2 && i + k < n; ++k) {
                const SpawnEntry& e = list[i + k];
                const DebugRect rc = DebugUI::Cell(row, 2, k);
                const bool armed = armedMob_ == e.id;
                if (ui.Button(rc, nullptr, armed, true, &accent)) {
                    if (!CountsMatter(e.id) || e.id == 16) {
                        // Immediate: bosses, test window, dummy wall (they have their own spots).
                        SpawnTarget(e.id);
                    } else if (armed) {
                        armedMob_ = -1;
                        SetMessage(L"소환 대기를 취소했습니다");
                    } else {
                        armedMob_ = e.id;
                        SetMessage(std::wstring(MobLabel(e.id)) + L" - 필드를 클릭하세요");
                    }
                }
                if (ui.Drawing() && ui.Visible(rc)) {
                    std::wstring name = MobLabel(e.id);
                    if (CountsMatter(e.id) && spawnCount_ > 1)
                        name += L"  x" + std::to_wstring(spawnCount_);
                    ui.Text(rc.x + 14.0f, rc.y + 6.0f,
                            DebugFitText(name.c_str(), 0.52f, rc.w - 22.0f).c_str(), 0.52f,
                            armed ? 1.0f : kText.r, armed ? 1.0f : kText.g, armed ? 1.0f : kText.b, 1.0f);
                    ui.Text(rc.x + 14.0f, rc.y + 28.0f,
                            DebugFitText(e.sub, 0.42f, rc.w - 22.0f).c_str(), 0.42f,
                            armed ? 0.85f : kTextDim.r, armed ? 0.90f : kTextDim.g,
                            armed ? 0.95f : kTextDim.b, 1.0f);
                }
            }
        }
    }

    // Field.
    int mobs = 0;
    if (context_.monsters) {
        for (const Monster* m : context_.monsters->monsters) mobs += m->alive ? 1 : 0;
        mobs += (int)context_.monsters->rangedMobs.size();
    }
    ui.Section(L"필드", (L"살아 있는 적 " + std::to_wstring(mobs)).c_str());
    {
        const DebugRect row = ui.Row(32.0f);
        if (ui.Button(DebugUI::Cell(row, 2, 0), L"적 · 탄 모두 제거", false,
                      context_.clearField != nullptr)) {
            context_.clearField();
            SetMessage(L"필드의 적과 탄을 모두 지웠습니다 (처치·보상 없음)");
        }
        if (ui.Button(DebugUI::Cell(row, 2, 1), L"다음 웨이브", false,
                      context_.advanceWave && context_.game &&
                      context_.game->currentState == GameState::RUNNING)) {
            context_.advanceWave();
            SetMessage(context_.game->waveVictory ? L"마지막 웨이브 완료 - 승리"
                                                  : L"다음 웨이브로 이동했습니다");
            if (context_.game->waveVictory) SetVisible(false);
        }
    }
    if (context_.buffMobs) {
        ui.TextRow(L"지금 필드의 적에게 레굴루스 버프 (켤 때마다 다시 걸림)");
        const DebugRect row = ui.Row(30.0f);
        static int buffMask = 0;
        static const wchar_t* const kBuff[4] = { L"보호막", L"가속", L"은신", L"버프 해제" };
        static const int kBit[3] = { 1, 2, 4 };
        for (int i = 0; i < 4; ++i) {
            const bool on = i < 3 && (buffMask & kBit[i]) != 0;
            if (!ui.Button(DebugUI::Cell(row, 4, i), kBuff[i], on)) continue;
            buffMask = i < 3 ? (buffMask ^ kBit[i]) : 0;
            context_.buffMobs(buffMask);
            SetMessage(buffMask ? L"버프를 다시 걸었습니다" : L"버프를 모두 해제했습니다");
        }
    }
}
