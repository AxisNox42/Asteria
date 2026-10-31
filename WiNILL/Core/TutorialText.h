#pragma once
#include <cwchar>
#include <string>
#include "Settings.h"
#include "KeyBindings.h"

// 튜토리얼 — '/' 로 줄바꿈 (증강 설명과 동일 파서)
inline constexpr int TUTORIAL_PAGE_COUNT = 7;

inline int LangIndexTutorial() {
    int li = (int)g_Language;
    if (li < 0 || li >= LANG_COUNT) li = 0;
    return li;
}

// Key tokens in the texts below follow the current key bindings:
//   {MOVE} = movement keys ("WASD"), {DASH} = dash key, {SKILLS} = skill keys ("Q/E/R")
//   {START} / {QUIT} = the fixed READY keys (Space / Esc, not bindable)
inline const wchar_t* ExpandKeyTokens(const wchar_t* src, std::wstring& out) {
    const std::wstring move = MoveKeysLabel();
    const std::wstring dash = KeyName(ActionKey(GameAction::Dash));
    const std::wstring skills = SkillKeysLabel();
    out.clear();
    for (const wchar_t* p = src; *p;) {
        if (!std::wcsncmp(p, L"{MOVE}", 6)) { out += move; p += 6; }
        else if (!std::wcsncmp(p, L"{DASH}", 6)) { out += dash; p += 6; }
        else if (!std::wcsncmp(p, L"{SKILLS}", 8)) { out += skills; p += 8; }
        else if (!std::wcsncmp(p, L"{START}", 7)) { out += KeyName(kBindKeySpace); p += 7; }
        else if (!std::wcsncmp(p, L"{QUIT}", 6)) { out += KeyName(kBindKeyEscape); p += 6; }
        else out += *p++;
    }
    return out.c_str();
}

// ── READY 화면 (짧은 요약) ──
inline const wchar_t* kReadyTitle[LANG_COUNT] = {
    L"튜토리얼",
    L"Tutorial",
    L"チュートリアル",
};
inline const wchar_t* kReadyBrief[LANG_COUNT] = {
    L"연습장에서 조작을 익히세요 — {START} 또는 ESC로 로비로 돌아갑니다",
    L"Learn the controls in the practice field — press {START} or ESC to return to the lobby",
    L"練習場で操作に慣れましょう — {START}またはESCでロビーに戻ります",
};

inline const wchar_t* ReadyTitle()  { return kReadyTitle[LangIndexTutorial()]; }
inline const wchar_t* ReadyBrief() {
    static std::wstring expanded;
    return ExpandKeyTokens(kReadyBrief[LangIndexTutorial()], expanded);
}

// ── READY 연습장 + 키보드 패널 ──
inline const wchar_t* kReadyFieldTitle = L"PRACTICE FIELD";
inline const wchar_t* kReadyObjectivesTitle = L"OBJECTIVES";
inline const wchar_t* kReadyFieldNote[LANG_COUNT] = {
    L"연습 모드 · 피해 없음",
    L"Practice · no damage",
    L"練習モード · ダメージなし",
};
inline const wchar_t* kReadyKills[LANG_COUNT] = { L"처치", L"Kills", L"撃破" };
inline constexpr int READY_OBJECTIVE_COUNT = 4;   // ready_practice::Objective order
inline const wchar_t* kReadyObjective[LANG_COUNT][READY_OBJECTIVE_COUNT] = {
    { L"이동하기", L"사격하기", L"대시하기", L"스웜 처치" },
    { L"Move", L"Shoot", L"Dash", L"Defeat Swarms" },
    { L"移動する", L"射撃する", L"ダッシュする", L"スウォーム撃破" },
};
// Instruction shown inside the practice field for the current step,
// by ready_practice::Objective (Move, Shoot, Dash, Kill) and then "done".
inline constexpr int READY_STEP_HINT_COUNT = READY_OBJECTIVE_COUNT + 1;
inline const wchar_t* kReadyStepHint[LANG_COUNT][READY_STEP_HINT_COUNT] = {
    { L"{MOVE} 키로 함선을 움직여 보세요",
      L"마우스로 조준하고, 왼쪽 버튼을 누르고 있으면 사격합니다",
      L"{DASH} 키로 대시 — 순간 무적과 함께 빠르게 피합니다",
      L"스웜 4마리를 처치하세요 — 3초 뒤 같은 자리에 다시 나타납니다",
      L"준비 완료! {START} 키로 출격하세요" },
    { L"Move your ship with {MOVE}",
      L"Aim with the mouse — hold the left button to fire",
      L"Press {DASH} to dash — a quick dodge with brief invulnerability",
      L"Defeat 4 Swarms — they come back on the same spot after 3 s",
      L"Ready! Press {START} to launch" },
    { L"{MOVE}で艦を動かしてみましょう",
      L"マウスで照準 — 左ボタン長押しで射撃",
      L"{DASH}でダッシュ — 一瞬の無敵で素早く回避",
      L"スウォームを4体撃破 — 3秒後に同じ場所へ再出現",
      L"準備完了！{START}で出撃" },
};
// Step counter above the instruction ("STEP 2 / 4").
inline const wchar_t* kReadyStepLabel = L"STEP";

// Under the dash objective when the dash is bound to the start key.
inline const wchar_t* kReadyDashOnStart[LANG_COUNT] = {
    L"{DASH}는 로비 복귀 키 — 전투에서 사용",
    L"{DASH} returns to the lobby here — use it in combat",
    L"{DASH}はロビー復帰キー — 戦闘で使用",
};
// Under the checklist once every step is done.
inline const wchar_t* kReadyAllDone[LANG_COUNT] = {
    L"완료 — {START}로 로비 복귀",
    L"Done — return to the lobby with {START}",
    L"完了 — {START}でロビーに戻る",
};
inline const wchar_t* kReadyAim[LANG_COUNT] = {
    L"조준 · 좌클릭 유지 사격",
    L"Aim · hold left click to fire",
    L"照準 · 左クリック長押しで射撃",
};
// Legend beside the keyboard, one line per key group (keys follow the bindings).
inline constexpr int READY_LEGEND_COUNT = 5;
inline const wchar_t* kReadyLegend[LANG_COUNT][READY_LEGEND_COUNT] = {
    { L"{MOVE}  이동", L"{DASH}  대시", L"{SKILLS}  스킬 (증강 후)", L"{START}  로비로", L"{QUIT}  나가기" },
    { L"{MOVE}  move", L"{DASH}  dash", L"{SKILLS}  skills (from augments)", L"{START}  lobby", L"{QUIT}  leave" },
    { L"{MOVE}  移動", L"{DASH}  ダッシュ", L"{SKILLS}  スキル (強化後)", L"{START}  ロビー", L"{QUIT}  戻る" },
};
// Small caption under a single-action key cap, by keyboard_panel::KeyRole
// (None, Move, Dash, Skill, Start, Quit). Empty = no caption.
inline constexpr int READY_KEY_ROLE_COUNT = 6;
inline const wchar_t* kReadyKeyCaption[LANG_COUNT][READY_KEY_ROLE_COUNT] = {
    { L"", L"", L"대시", L"", L"로비로", L"나가기" },
    { L"", L"", L"dash", L"", L"lobby", L"leave" },
    { L"", L"", L"ダッシュ", L"", L"ロビー", L"戻る" },
};

inline const wchar_t* ReadyFieldNote() { return kReadyFieldNote[LangIndexTutorial()]; }
inline const wchar_t* ReadyKills()     { return kReadyKills[LangIndexTutorial()]; }
inline const wchar_t* ReadyAim()       { return kReadyAim[LangIndexTutorial()]; }
inline const wchar_t* ReadyObjective(int i) {
    if (i < 0 || i >= READY_OBJECTIVE_COUNT) return L"";
    return kReadyObjective[LangIndexTutorial()][i];
}
inline const wchar_t* ReadyStepHint(int step) {
    static std::wstring expanded;
    if (step < 0 || step >= READY_STEP_HINT_COUNT) return L"";
    return ExpandKeyTokens(kReadyStepHint[LangIndexTutorial()][step], expanded);
}
inline const wchar_t* ReadyDashOnStart() {
    static std::wstring expanded;
    return ExpandKeyTokens(kReadyDashOnStart[LangIndexTutorial()], expanded);
}
inline const wchar_t* ReadyAllDone() {
    static std::wstring expanded;
    return ExpandKeyTokens(kReadyAllDone[LangIndexTutorial()], expanded);
}
inline const wchar_t* ReadyLegend(int i) {
    static std::wstring expanded[READY_LEGEND_COUNT];
    if (i < 0 || i >= READY_LEGEND_COUNT) return L"";
    return ExpandKeyTokens(kReadyLegend[LangIndexTutorial()][i], expanded[i]);
}
inline const wchar_t* ReadyKeyCaption(int role) {
    if (role < 0 || role >= READY_KEY_ROLE_COUNT) return L"";
    return kReadyKeyCaption[LangIndexTutorial()][role];
}

// ── 튜토리얼 창 (페이지별) ──
inline const wchar_t* kTutorialWinTitle[LANG_COUNT] = {
    L"ASTRAL FIELD GUIDE",
    L"ASTRAL FIELD GUIDE",
    L"ASTRAL FIELD GUIDE",
};

inline const wchar_t* kTutorialPageTitle[LANG_COUNT][TUTORIAL_PAGE_COUNT] = {
    /* KR */
    { L"1. 세계관", L"2. 조작", L"3. 증강", L"4. 디버프", L"5. 플레이 준비", L"6. 플레이 경제", L"7. 메타·팁" },
    /* EN */
    { L"1. World", L"2. Controls", L"3. Augments", L"4. Debuffs", L"5. Play setup", L"6. Play economy", L"7. Meta & tips" },
    /* JP */
    { L"1. 世界観", L"2. 操作", L"3. 強化", L"4. デバフ", L"5. プレイ準備", L"6. プレイ経済", L"7. メタ·コツ" },
};

inline const wchar_t* kTutorialPageBody[LANG_COUNT][TUTORIAL_PAGE_COUNT] = {
    /* KR */
    {
        L"Asteria는 별빛 위에 펼쳐지는 투명한 천문 관측 게임입니다 / "
        L"별빛과 성운이 비치고, 게임은 항해 가능한 별자리 영역 안에서 펼쳐집니다 / "
        L"당신은 항성 함선 — 관측 영역을 이동하며 침입한 신호(적)를 막아냅니다 / "
        L"영역 밖은 시야 밖입니다. 보이는 별자리 영역 안에서만 전투·피격·이펙트가 일어납니다 / "
        L"목표: 웨이브를 버티며 최대한 오래 생존하고 점수·코인을 모으세요",

        L"{MOVE} — 항성 함선 이동 / 마우스 — 조준 방향, 좌클릭 유지 시 연사 / "
        L"{DASH} — 대시(짧은 무적·쿨다운). 위험한 탄막을 뚫을 때 사용 / "
        L"Q · E · R — 액티브 스킬 슬롯. 증강으로 해금되며 최대 3개까지 장착 / "
        L"ESC — 일시정지(재개·설정·메뉴) / Space — READY 화면에서 전투 시작, 일시정지 중 재개 / "
        L"마우스를 카드·아이콘 위에 올리면 상세 설명이 표시됩니다",

        L"적 처치 → 경험치(EXP) → 레벨업 / "
        L"레벨업하면 증강 카드 3장이 자동으로 열립니다 / "
        L"숫자키 1·2·3으로 후보를 고르고 Space로 확정합니다 / "
        L"증강은 공격·방어·이동·스킬·경제 등 빌드를 결정합니다. 중복·시너지를 고려하세요 / "
        L"일부 증강은 {SKILLS} 액티브를 해금하거나 기존 스킬을 강화합니다 / "
        L"도감에서 이미 본 증강은 이름·효과를 미리 확인할 수 있습니다(미발견은 ???)",

        L"레벨업마다 증강과 함께 디버프도 반드시 1개 받습니다 — 피할 수 없습니다 / "
        L"대신 디버프 구간에서는 EXP·코인 보상이 올라가 리스크=리턴 구조입니다 / "
        L"디버프는 이동·사격·체력·쿨다운 등에 불리한 효과를 줍니다 / "
        L"증강으로 상쇄하거나, 패턴에 맞춰 플레이 스타일을 바꿔 대응하세요 / "
        L"난이도에 따라 적 출현량·체력·등장 티어가 달라집니다 / "
        L"시련은 추가 도전을 더합니다",

        L"플레이 준비 화면에서 난이도와 무기를 선택합니다 / "
        L"각 웨이브는 3분이며, 쉬움 10·보통 15·어려움 20웨이브를 생존하세요 / "
        L"소총은 직접 조준해 연사하고, 전기장은 주변을 지속 공격합니다 / "
        L"선택 화면 설명을 읽고 플레이에 맞는 무기를 고르세요 / "
        L"크리에이티브 모드에서는 시작 점수와 플레이 조건을 설정할 수 있습니다",

        L"플레이 중 획득한 스타더스트는 HUD에 표시되고 플레이 종료 시 정산됩니다 / "
        L"레벨업 증강 선택으로 빌드를 보완합니다 / "
        L"플레이가 끝나면 일부 성과가 코인으로 정산되어 메타 상점에 쓰입니다 / "
        L"메인 메뉴 상점 — 코인으로 영구 스탯과 테마를 해금 / "
        L"업적 달성 시 코인·해금 보상이 주어집니다",

        L"재료 증강 3종을 모으고 Lv10 이상이면 「조합 증강」이 등장합니다 / "
        L"조합은 강력하지만 재료를 차지하므로 계획적으로 모으세요 / "
        L"메인 메뉴 도감 — 적·증강 정보 검색(위키 스타일) / "
        L"설정에서 언어·볼륨·관측 필드 동작을 바꿀 수 있습니다 / "
        L"패배 후에도 코인·도감·업적 진행은 유지됩니다. 반복 플레이로 빌드를 완성하세요",
    },
    /* EN */
    {
        L"Asteria is a transparent observatory spread across the starfield / "
        L"The cosmos shows through; combat happens inside navigable constellation zones / "
        L"You are the astral vessel — move through the field and stop invading signals (enemies) / "
        L"Outside the visible constellation zones is out of play: no combat, hits, or effects there / "
        L"Goal: survive waves and collect score·coins for as long as you can",

        L"{MOVE} — move your astral vessel / Mouse — aim; hold LMB to fire / "
        L"{DASH} — dash with brief i-frames and cooldown / "
        L"Q · E · R — active skills from augments (max 3 slots) / "
        L"ESC — pause menu / Space — start from READY, resume from pause / "
        L"Hover cards and icons for full descriptions",

        L"Kills → XP → level-up / "
        L"On level-up, 3 augment cards open on their own / "
        L"Keys 1·2·3 select; Space confirms / "
        L"Augments define your build: damage, defense, mobility, skills, economy / "
        L"Some unlock or upgrade {SKILLS} actives / "
        L"Codex shows discovered augments in detail; unknown ones stay hidden",

        L"Every level-up also forces one debuff — cannot skip / "
        L"Debuff periods grant more XP and coins (risk vs reward) / "
        L"Debuffs hinder move speed, fire rate, HP, cooldowns, etc. / "
        L"Counter with augments or adapt your playstyle / "
        L"Enemy count, health, and tiers change with difficulty / "
        L"Trials add another challenge",

        L"Choose a difficulty and weapon in play setup / "
        L"Survive 10·15·20 waves on Easy·Normal·Hard; each lasts 3 minutes / "
        L"Rifle fires aimed shots; Static Field continuously attacks nearby / "
        L"Read the select-screen text and pick what fits your style / "
        L"Creative mode lets you set the starting score and play conditions",

        L"Stardust earned mid-play is shown on the HUD and settled at play end / "
        L"Use level-up augment choices to patch your build / "
        L"End of play converts some progress into meta coins / "
        L"Main menu shop — permanent stats and display themes / "
        L"Achievements grant coins and unlocks",

        L"Collect 3 combo ingredient augments + reach Lv10 → combo augment appears / "
        L"Combos are strong but cost ingredient slots — plan ahead / "
        L"Main menu Codex — searchable wiki for enemies and augments / "
        L"Settings: language, volume, observatory behavior / "
        L"Death keeps coins, codex, and achievements — iterate your build",
    },
    /* JP */
    {
        L"Asteriaは星空に広がる透明な天文観測ゲームです / "
        L"星雲が透けて見え、戦闘は航行可能な星座領域で行われます / "
        L"あなたは星の船 — フィールドを移動し侵入信号(敵)を阻止します / "
        L"星座領域の外は戦闘範囲外 — 攻撃·被弾·演出は領域内のみ / "
        L"目標: ウェーブを凌ぎ、できるだけ長くスコア·コインを集める",

        L"{MOVE} — 星の船(プレイヤー)移動 / マウス — 照準、左クリック長押しで連射 / "
        L"{DASH} — 短い無敵付きダッシュ(クールダウンあり) / "
        L"Q · E · R — 強化で解放するアクティブスキル(最大3) / "
        L"ESC — 一時停止 / Space — READYから開始、停止中は再開 / "
        L"カード·アイコンにホバーで詳細表示",

        L"撃破→経験値→レベルアップ / "
        L"レベルアップすると強化カード3枚が自動で開きます / "
        L"1·2·3で選択、Spaceで確定 / "
        L"強化は攻撃·防御·移動·スキル·経済などビルドを決めます / "
        L"一部は{SKILLS}アクティブの解放·強化になります / "
        L"図鑑で発見済み強化の説明を確認(未発見は???)",

        L"レベルアップごとにデバフも必ず1つ — 回避不可 / "
        L"その代わりEXP·コイン報酬が増えるリスク·リターン設計 / "
        L"デバフは移動·射撃·HP·クールダウンなどに不利 / "
        L"強化で相殺するかプレイを変えて対応 / "
        L"難易度が上がるほどデバフと敵密度が厳しくなります",

        L"プレイ前: 難易度を選択 / "
        L"イージー10·ノーマル15·ハード20ウェーブ(各3分) / "
        L"プレイ準備画面でライフルか静電場を選択 / "
        L"ライフルは照準連射、静電場は周囲を継続攻撃 / "
        L"選択画面の説明を読み、自分に合う武器を選ぶ / "
        L"クリエイティブモードで開始条件·開始強化を設定可能",

        L"プレイ中に得たスターダストはHUDに表示され、終了時に精算 / "
        L"レベルアップ時の強化選択でビルドを補強 / "
        L"プレイ終了後、一部の成果がメタコインに換算 / "
        L"メニューショップ — 永久ステータス・テーマなど / "
        L"実績でコインと解禁報酬",

        L"素材強化3種+Lv10以上で「組合強化」出現 / "
        L"強力だが素材枠を使う — 計画的に収集 / "
        L"メニュー図鑑 — 敵·強化を検索(ウィキ風) / "
        L"設定で言語·音量·窓動作を変更 / "
        L"敗北後もコイン·図鑑·実績は保持 — 繰り返しビルドを完成させよう",
    },
};

inline const wchar_t* kTutorialControls[LANG_COUNT] = {
    L"{MOVE} 이동  |  마우스 사격  |  {DASH} 대시  |  {SKILLS} 스킬  |  ESC 일시정지",
    L"{MOVE}  |  Mouse fire  |  {DASH} dash  |  {SKILLS} skills  |  ESC pause",
    L"{MOVE}  |  マウス射撃  |  {DASH}ダッシュ  |  {SKILLS}スキル  |  ESC停止",
};

inline const wchar_t* TutorialWinTitle() { return kTutorialWinTitle[LangIndexTutorial()]; }
inline const wchar_t* TutorialPageTitle(int page) {
    if (page < 0 || page >= TUTORIAL_PAGE_COUNT) return L"";
    return kTutorialPageTitle[LangIndexTutorial()][page];
}
inline const wchar_t* TutorialPageBody(int page) {
    if (page < 0 || page >= TUTORIAL_PAGE_COUNT) return L"";
    static std::wstring expanded;
    return ExpandKeyTokens(kTutorialPageBody[LangIndexTutorial()][page], expanded);
}
inline const wchar_t* TutorialControls() {
    static std::wstring expanded;
    return ExpandKeyTokens(kTutorialControls[LangIndexTutorial()], expanded);
}
