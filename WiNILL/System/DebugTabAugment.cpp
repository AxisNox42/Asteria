// F1 debug console > 증강 tab: search, category chips, held-only view,
// release-all, and three session-only sets (save what you hold, load it later).
#include "DebugToolkit.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>
#include <vector>

#include "DebugUI.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "Settings.h"
#include "PassiveAugments.h"
#include "AugmentSynergy.h"
#include "DebugAugSearch.h"

extern TextRenderer g_TextS;

namespace {

// Categories: level-up stat cards, the synergy roles, then the legacy cards
// (each group only listed when its kLegacy* switch is on). Filter 0 is "all",
// filter i is category i-1.
enum AugCat { AC_STAT, AC_FRAME, AC_VERB, AC_TRIGGER, AC_AMP, AC_COST, AC_GUARD, AC_CHANCE,
              AC_LEGACY_PASSIVE, AC_LEGACY_ACTIVE, AC_COUNT };
constexpr int AUG_FILTER_COUNT = AC_COUNT + 1;
constexpr float AUG_ROW_H = 52.0f;

int AugCategory(int aug) {
    const AugDef& d = ALL_AUGS[aug];
    if (AugIsStatCard(d)) return AC_STAT;
    if (AugIsSynergy(d.type)) {
        const auto& t = synergy::Table();
        const int si = SynergyIndex(d.type);
        if (si >= 0 && si < (int)t.size()) {
            switch (t[(size_t)si].role) {
            case synergy::Role::Frame:   return AC_FRAME;
            case synergy::Role::Verb:    return AC_VERB;
            case synergy::Role::Trigger: return AC_TRIGGER;
            case synergy::Role::Amp:     return AC_AMP;
            case synergy::Role::Cost:    return AC_COST;
            case synergy::Role::Guard:   return AC_GUARD;
            case synergy::Role::Chance:  return AC_CHANCE;
            }
        }
        return AC_VERB;
    }
    return AugIsActive(d) ? AC_LEGACY_ACTIVE : AC_LEGACY_PASSIVE;
}

const wchar_t* AugCategoryLabel(int cat) {
    static const wchar_t* const n[AC_COUNT] = {
        L"스탯", L"프레임", L"동사", L"트리거", L"증폭", L"대가", L"보호", L"확률",
        L"패시브(구)", L"액티브(구)" };
    return (cat >= 0 && cat < AC_COUNT) ? n[cat] : L"?";
}

const wchar_t* AugCategoryHint(int cat) {
    static const wchar_t* const n[AC_COUNT] = {
        L"레벨업 스탯 카드 (중복 가능)",
        L"무기 형태를 바꾸는 카드 (한 판에 하나)",
        L"탄의 행동을 바꾸는 카드 (관통·튕김·분열·유도 등)",
        L"조건이 맞으면 터지는 카드 (처치·치명타·피격·정지 등)",
        L"가진 태그 수만큼 강해지는 카드",
        L"대가를 치르고 힘을 얻는 카드 (체력·유리 등)",
        L"보호막·회피 등 생존 카드",
        L"확률로 발동하는 카드",
        L"구버전 패시브 카드",
        L"구버전 액티브 스킬 카드" };
    return (cat >= 0 && cat < AC_COUNT) ? n[cat] : L"";
}

DebugColor AugCategoryColor(int cat) {
    static const DebugColor c[AC_COUNT] = {
        { 0.70f, 0.78f, 0.86f },   // stat
        { 1.00f, 0.80f, 0.40f },   // frame
        { 0.45f, 0.85f, 1.00f },   // verb
        { 1.00f, 0.55f, 0.45f },   // trigger
        { 0.80f, 0.60f, 1.00f },   // amp
        { 1.00f, 0.40f, 0.60f },   // cost
        { 0.50f, 1.00f, 0.65f },   // guard
        { 1.00f, 0.92f, 0.45f },   // chance
        { 0.60f, 0.62f, 0.66f },
        { 0.60f, 0.62f, 0.66f } };
    return c[(cat >= 0 && cat < AC_COUNT) ? cat : 0];
}

// Every card in the build, grouped by category (catalog order inside a group).
const std::vector<int>& AugmentList(int filter) {
    static const std::array<std::vector<int>, AUG_FILTER_COUNT> lists = [] {
        std::array<std::vector<int>, AUG_FILTER_COUNT> out;
        for (int cat = 0; cat < AC_COUNT; ++cat)
            for (int i = 0; i < AUG_TOTAL; ++i) {
                if (!AugInBuild(ALL_AUGS[i]) || AugCategory(i) != cat) continue;
                out[0].push_back(i);
                out[(size_t)cat + 1].push_back(i);
            }
        return out;
    }();
    return lists[(size_t)std::max(0, std::min(AUG_FILTER_COUNT - 1, filter))];
}

// Filters shown: "all" plus the categories that have cards in this build.
const std::vector<int>& AugmentFilters() {
    static const std::vector<int> v = [] {
        std::vector<int> f{ 0 };
        for (int i = 1; i < AUG_FILTER_COUNT; ++i)
            if (!AugmentList(i).empty()) f.push_back(i);
        return f;
    }();
    return v;
}

bool AugMatchesQuery(int aug, const std::wstring& query) {
    const AugDef& d = ALL_AUGS[aug];
    return debugsearch::Matches(d.locName, LANG_COUNT, d.locDesc, LANG_COUNT, query);
}

int HeldTotal() {
    int n = 0;
    for (int i = 0; i < AUG_TOTAL; ++i) n += passive::HeldCount(i);
    return n;
}

} // namespace

const std::vector<int>& DebugToolkit::AugVisibleList() {
    const std::wstring q = AugSearchQuery();
    const bool sameKey = augVisibleFilter_ == augFilter_ && augVisibleQuery_ == q &&
                         augVisibleHeld_ == augHeldOnly_;
    // The held-only view follows every grant / release, so it is rebuilt each time.
    if (sameKey && !augHeldOnly_) return augVisible_;
    if (!sameKey) augOffset_ = 0;
    augVisibleFilter_ = augFilter_;
    augVisibleQuery_ = q;
    augVisibleHeld_ = augHeldOnly_;
    augVisible_.clear();
    for (int aug : AugmentList(augFilter_)) {
        if (augHeldOnly_ && passive::HeldCount(aug) <= 0) continue;
        if (AugMatchesQuery(aug, q)) augVisible_.push_back(aug);
    }
    return augVisible_;
}

bool DebugToolkit::AugEditableNow() {
    const GameState st = context_.game ? context_.game->currentState : GameState::MAIN_MENU;
    return st == GameState::RUNNING || st == GameState::PAUSED || st == GameState::READY;
}

// Left click: take the card, or switch it OFF when it is held and cannot stack.
// Right click: take one copy back.
void DebugToolkit::ClickAugmentFromConsole(int aug, bool rightClick) {
    const bool held = passive::HeldCount(aug) > 0;
    if (!rightClick && !(held && !passive::CanOffer(aug))) {
        GrantAugmentFromConsole(aug);
        return;
    }
    if (!held) {
        SetMessage(L"가지고 있지 않은 증강입니다");
        return;
    }
    if (!AugEditableNow()) {
        SetMessage(L"증강은 진행 중/일시정지/준비 화면에서만 바꿀 수 있습니다");
    } else if (passive::Release(aug)) {
        std::wstring m = L"증강 해제: ";
        m += AugName(ALL_AUGS[aug]);
        const int left = passive::HeldCount(aug);
        if (left > 0) m += L" (남은 수 " + std::to_wstring(left) + L")";
        SetMessage(m);
    } else {
        SetMessage(L"이 증강은 해제할 수 없습니다");
    }
}

void DebugToolkit::GrantAugmentFromConsole(int aug) {
    if (!AugEditableNow()) {
        SetMessage(L"증강은 진행 중/일시정지/준비 화면에서만 받을 수 있습니다");
    } else if (!passive::CanOffer(aug)) {
        SetMessage(L"받을 수 없는 증강입니다 (이미 보유했거나 슬롯 부족)");
    } else if (context_.grantAugment && context_.grantAugment(aug)) {
        std::wstring m = L"증강 획득: ";
        m += AugName(ALL_AUGS[aug]);
        SetMessage(m);
    } else {
        SetMessage(L"증강을 적용하지 못했습니다");
    }
}

// Releases every held copy. Later cards in a chain may refuse while an earlier
// one is held (or the other way round), so it sweeps until nothing moves.
int DebugToolkit::ReleaseAllAugments() {
    int released = 0;
    for (int pass = 0; pass < 8; ++pass) {
        bool moved = false;
        for (int i = AUG_TOTAL - 1; i >= 0; --i)
            while (passive::HeldCount(i) > 0 && passive::Release(i)) {
                ++released;
                moved = true;
            }
        if (!moved) break;
    }
    return released;
}

void DebugToolkit::TabAugment(DebugUI& ui, float bottom) {
    // Search.
    {
        const DebugRect row = ui.Row(30.0f);
        DrawTextBox(ui, row.x, row.y, row.w, row.h, augSearch_,
                    L"검색 (이름·설명, 한/영)", TextFocus::AugSearch);
    }

    // Category chips (wrap).
    {
        const auto& filters = AugmentFilters();
        std::vector<std::wstring> labels;
        std::vector<const wchar_t*> ptrs;
        std::vector<DebugColor> colors;
        int selected = 0;
        for (size_t i = 0; i < filters.size(); ++i) {
            const int f = filters[i];
            labels.push_back((f == 0 ? std::wstring(L"전체") : std::wstring(AugCategoryLabel(f - 1))) +
                             L" " + std::to_wstring(AugmentList(f).size()));
            colors.push_back(f == 0 ? DebugColor{} : AugCategoryColor(f - 1));
            if (augFilter_ == f) selected = (int)i;
        }
        for (const auto& l : labels) ptrs.push_back(l.c_str());
        const int hit = ui.Chips(ptrs.data(), (int)ptrs.size(), selected, 28.0f, 0.46f, colors.data());
        if (hit >= 0 && augFilter_ != filters[(size_t)hit]) {
            augFilter_ = filters[(size_t)hit];
            augOffset_ = 0;
            lastMessage_.clear();
        }
    }

    // Held-only / release all / sets.
    const int heldTotal = HeldTotal();
    {
        const DebugRect row = ui.Row(30.0f);
        if (ui.Toggle(DebugUI::Span(row, 2, 0, 1),
                      (L"보유만 보기 (" + std::to_wstring(heldTotal) + L")").c_str(), augHeldOnly_)) {
            augHeldOnly_ = !augHeldOnly_;
            augOffset_ = 0;
        }
        if (ui.Button(DebugUI::Span(row, 2, 1, 1), L"전부 해제", false, heldTotal > 0)) {
            if (!AugEditableNow()) SetMessage(L"증강은 진행 중/일시정지/준비 화면에서만 바꿀 수 있습니다");
            else {
                const int n = ReleaseAllAugments();
                const int left = HeldTotal();
                SetMessage(L"증강 " + std::to_wstring(n) + L"장 해제" +
                           (left > 0 ? L" (해제할 수 없는 " + std::to_wstring(left) + L"장 남음)"
                                     : std::wstring()));
            }
        }
    }
    {
        // Sets: left click = load (replaces what you hold), right click = save.
        const DebugRect row = ui.Row(30.0f);
        for (int s = 0; s < 3; ++s) {
            auto& set = augSets_[(size_t)s];
            const std::wstring label = L"세트 " + std::to_wstring(s + 1) +
                (set.empty() ? std::wstring(L"  비어 있음") : L"  " + std::to_wstring(set.size()) + L"장");
            const int b = ui.ButtonLR(DebugUI::Cell(row, 3, s), label.c_str(), false, true,
                                      nullptr, 0.46f);
            if (b == 2) {
                set.clear();
                for (int i = 0; i < AUG_TOTAL; ++i)
                    for (int k = passive::HeldCount(i); k > 0; --k) set.push_back(i);
                SetMessage(L"세트 " + std::to_wstring(s + 1) + L"에 지금 증강 " +
                           std::to_wstring(set.size()) + L"장을 저장했습니다");
            } else if (b == 1) {
                if (set.empty()) SetMessage(L"빈 세트입니다 - 우클릭으로 지금 증강을 저장하세요");
                else if (!AugEditableNow()) SetMessage(L"증강은 진행 중/일시정지/준비 화면에서만 바꿀 수 있습니다");
                else {
                    ReleaseAllAugments();
                    int got = 0;
                    for (int aug : set)
                        if (passive::CanOffer(aug) && context_.grantAugment &&
                            context_.grantAugment(aug)) ++got;
                    SetMessage(L"세트 " + std::to_wstring(s + 1) + L" 불러옴: " +
                               std::to_wstring(got) + L"/" + std::to_wstring(set.size()) + L"장");
                }
            }
        }
    }

    // Status line.
    const auto& list = AugVisibleList();
    const std::wstring query = AugSearchQuery();
    {
        std::wstring status;
        if (!query.empty() && list.empty()) status = L"'" + query + L"' 에 맞는 증강이 없습니다";
        else if (!query.empty())
            status = L"'" + query + L"' 결과 " + std::to_wstring(list.size()) + L"개" +
                     (list.size() == 1 ? L"  ·  ENTER = 획득" : L"");
        else if (augFilter_ > 0) status = AugCategoryHint(augFilter_ - 1);
        else status = L"클릭 = 획득/끄기 · 우클릭 = 1장 빼기 · 세트: 우클릭 저장, 클릭 불러오기";
        ui.TextRow(status.c_str());
    }

    // List.
    const float listTop = ui.y;
    const DebugRect area{ ui.left, listTop, ui.width - 8.0f, std::max(AUG_ROW_H, bottom - listTop - 4.0f) };
    const int rows = ui.ScrollRows(area, AUG_ROW_H, (int)list.size(), augOffset_);
    for (int row = 0; row < rows; ++row) {
        const int idx = augOffset_ + row;
        if (idx >= (int)list.size()) break;
        const int aug = list[(size_t)idx];
        const AugDef& d = ALL_AUGS[aug];
        const int cat = AugCategory(aug);
        const DebugRect rc{ area.x, area.y + row * AUG_ROW_H, area.w, AUG_ROW_H - 4.0f };
        const int owned = passive::HeldCount(aug);
        const bool offerable = passive::CanOffer(aug) || owned > 0;
        const DebugColor kc = AugCategoryColor(cat);
        DebugColor rarity{};
        GetRarityColor(d.rarity, rarity.r, rarity.g, rarity.b);
        const int b = ui.ButtonLR(rc, nullptr, owned > 0, true, &rarity);
        if (b) { ClickAugmentFromConsole(aug, b == 2); continue; }
        if (!ui.Drawing()) continue;

        float cr = 1, cg = 1, cb = 1;
        GetRarityColor(d.rarity, cr, cg, cb);
        BindMainShader();
                const float a = offerable ? 0.95f : 0.45f;
        // Line 1: [category] name ... rarity, owned.
        const std::wstring tag = L"[" + std::wstring(AugCategoryLabel(cat)) + L"]";
        g_TextS.Draw(tag.c_str(), rc.x + 14.0f, rc.y + 6.0f, 0.44f, kc.r, kc.g, kc.b, a);
        const float nameX = rc.x + 14.0f + std::max(66.0f, g_TextS.Width(tag.c_str(), 0.44f) + 8.0f);
        const float rightW = 118.0f;
        const std::wstring name = DebugFitText(AugName(d), 0.50f, rc.x + rc.w - rightW - nameX);
        g_TextS.Draw(name.c_str(), nameX, rc.y + 5.0f, 0.50f, debugui_style::kText.r, debugui_style::kText.g, debugui_style::kText.b, a);
        g_TextS.Draw(GetRarityLabel(d.rarity), rc.x + rc.w - rightW, rc.y + 5.0f, 0.40f,
                     std::min(1.0f, cr + 0.25f), std::min(1.0f, cg + 0.25f),
                     std::min(1.0f, cb + 0.25f), a);
        if (owned > 0) {
            const std::wstring own = L"x" + std::to_wstring(owned);
            g_TextS.Draw(own.c_str(), rc.x + rc.w - 10.0f - g_TextS.Width(own.c_str(), 0.46f),
                         rc.y + 4.0f, 0.46f, 1.0f, 0.88f, 0.42f, 0.98f);
        }
        // Line 2: description.
        const std::wstring desc = DebugFitText(AugDesc(d), 0.42f, rc.w - 28.0f);
        g_TextS.Draw(desc.c_str(), rc.x + 14.0f, rc.y + 28.0f, 0.42f,
                     owned > 0 ? 0.85f : debugui_style::kTextDim.r, owned > 0 ? 0.90f : debugui_style::kTextDim.g,
                     owned > 0 ? 0.95f : debugui_style::kTextDim.b, a);
    }
    ui.y = area.y + area.h;
}
