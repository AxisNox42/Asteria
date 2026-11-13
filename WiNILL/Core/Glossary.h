#pragma once
// Augment card glossary: words in a card description that a new player may not
// know become links (like a wiki). Hovering one shows its definition under the
// card (SceneAugment.cpp). Texts: KR / EN / JP, the card language order.
//
// Each term lists the phrases that link to it, '|' separated, per language. A
// '#' in a phrase stands for a run of digits, and then only the digits become
// the link (distance numbers: "주변 # 안" links "300"). Only the first phrase of
// a term in a card links, so a card is not covered in underlines.
#include <cstdint>
#include <cwctype>
#include <string>
#include <vector>

namespace glossary {

enum class Term : int {
    Verb, Pierce, Bounce, Split, Homing, Blast, Burn, Mark, Orbit, Trail, Mine, Hover,
    Pull, Return, Swap, Erase, Graze, Shield, Still, Crit, CritMult, HitCount, DmgKinds,
    FireRate, Range, Distance, Stardust, Luck, LightYear, Broken, Stack, RunWave,
    ActiveSkill, Dash, SkillSlot, Reaction, Risky, ChanceAug, Enemies, Count
};
inline constexpr int kTermCount = (int)Term::Count;
static_assert(kTermCount <= 64, "Link uses a 64-bit mask of terms");

struct Entry {
    const wchar_t* name[3];
    const wchar_t* def[3];
    const wchar_t* phrases[3];
    Term rel[3];
    int relCount;
};

// Rows follow the Term order.
inline const Entry kEntries[kTermCount] = {
    { { L"탄 효과", L"Bullet effect", L"弾の効果" },
      { L"탄이 날아가거나 맞을 때 하는 행동입니다. 관통·튕김·분열·유도·폭발·화상·표식 등이 있고, 같은 종류는 한 번만 셉니다.",
        L"What a bullet does as it flies or hits: pierce, bounce, split, homing, burst, burn, mark and so on. Each kind counts once.",
        L"弾が飛ぶとき・当たるときの動きです。貫通・跳弾・分裂・誘導・爆発・火傷・刻印などがあり、同じ種類は1回だけ数えます。" },
      { L"탄 효과|탄 동사", L"bullet effect", L"弾の効果" },
      { Term::Pierce, Term::Bounce, Term::Split }, 3 },
    { { L"관통", L"Pierce", L"貫通" },
      { L"탄이 적을 맞혀도 사라지지 않고 계속 날아갑니다. 관통 +1이면 적 1명을 더 뚫습니다.",
        L"The bullet keeps flying after it hits an enemy. +1 pierce goes through one more enemy.",
        L"弾が敵に当たっても消えずに飛び続けます。貫通+1なら敵をもう1体貫きます。" },
      { L"관통|꿰뚫음|뚫음", L"pierces|pierce|piercing|pass through", L"貫通|貫く" },
      { Term::Verb }, 1 },
    { { L"벽 튕김", L"Wall bounce", L"壁跳ね" },
      { L"탄이 화면 끝 벽에 닿으면 반사되어 다시 날아갑니다.",
        L"The bullet reflects off the screen edge and keeps flying.",
        L"弾が画面端の壁に当たると反射して飛び続けます。" },
      { L"벽 튕김|벽에서 2회 튕김|벽에서 3번 더 튕김|벽에서 반사",
        L"wall bounce|bounce off the arena edge|bounce off walls|bounce 3 more times off walls",
        L"壁跳ね|マップの端で2回跳ね返る|壁で反射|壁でさらに3回跳ね" },
      { Term::Verb }, 1 },
    { { L"분열", L"Split", L"分裂" },
      { L"탄이 처음 적을 맞힐 때 3갈래로 갈라집니다. 갈라진 탄은 원래 피해의 28%입니다.",
        L"On its first hit the bullet splits into 3. Each split bullet deals 28% of the original damage.",
        L"最初に敵に当たったとき弾が3方向に分かれます。分かれた弾は元のダメージの28%です。" },
      { L"분열", L"splits|split", L"分裂" },
      { Term::Verb }, 1 },
    { { L"유도", L"Homing", L"誘導" },
      { L"탄이 가장 가까운 적 쪽으로 휘어 날아갑니다.",
        L"The bullet curves toward the nearest enemy.",
        L"弾が最も近い敵の方へ曲がって飛びます。" },
      { L"유도탄|유도|완만히 휨", L"homing|curve gently toward", L"誘導|緩やかに曲がる" },
      { Term::Verb }, 1 },
    { { L"착탄 폭발", L"Impact burst", L"着弾爆発" },
      { L"탄이 맞은 자리에서 터져 주변 80 안의 적에게 공격력 30% 피해를 줍니다.",
        L"The bullet bursts where it hits, dealing 30% attack to enemies within 80.",
        L"弾が当たった場所で弾け、周囲80以内の敵に攻撃力30%のダメージを与えます。" },
      { L"착탄 시 폭발|맞힌 자리에 작은 폭발", L"burst where they land|small burst where they land", L"着弾時に爆発|命中した場所で小爆発" },
      { Term::Verb, Term::Distance }, 2 },
    { { L"화상", L"Burn", L"火傷" },
      { L"3초 동안 초당 공격력 20% 피해를 받습니다.",
        L"The enemy takes 20% attack per second for 3s.",
        L"3秒間、毎秒攻撃力20%のダメージを受けます。" },
      { L"불탐|화상", L"burn", L"燃える|火傷" },
      { Term::Verb }, 1 },
    { { L"표식", L"Mark", L"刻印" },
      { L"5초 동안 그 적이 받는 피해가 25% 늘어납니다.",
        L"For 5s the enemy takes 25% more damage.",
        L"5秒間、その敵が受けるダメージが25%増えます。" },
      { L"표식", L"marks|mark", L"刻印" },
      { Term::Verb }, 1 },
    { { L"공전", L"Orbit", L"周回" },
      { L"탄이 플레이어 주위 90 거리를 돌며 닿는 적에게 피해를 줍니다. 같은 적은 0.4초마다 다시 맞습니다.",
        L"The bullet circles you at 90 and hurts enemies it touches. The same enemy can be hit again every 0.4s.",
        L"弾がプレイヤーの周囲90を回り、触れた敵にダメージを与えます。同じ敵には0.4秒ごとに当たります。" },
      { L"주위를 돌며|플레이어를 돌며", L"circles the player", L"周囲を回り|プレイヤーを回って" },
      { Term::Verb, Term::Distance }, 2 },
    { { L"잔상", L"Afterimage", L"残像" },
      { L"탄이나 대시가 지나간 자리에 잠깐 남는 영역입니다. 닿아 있는 적에게 계속 피해를 줍니다.",
        L"A short-lived zone left where a bullet or dash passed. It keeps hurting enemies inside it.",
        L"弾やダッシュが通った場所にしばらく残る領域です。触れている敵にダメージを与え続けます。" },
      { L"잔상", L"afterimage", L"残像" },
      { Term::Verb }, 1 },
    { { L"지뢰", L"Mine", L"地雷" },
      { L"땅에 남아 있다가 적이 다가오면 터집니다.",
        L"Stays on the ground and explodes when an enemy comes close.",
        L"地面に残り、敵が近づくと爆発します。" },
      { L"지뢰|덫", L"mine|trap", L"地雷|罠" },
      { Term::Verb }, 1 },
    { { L"체공", L"Hover", L"滞空" },
      { L"탄이 일정 거리를 날아간 뒤 그 자리에 잠깐 멈췄다가 다시 날아갑니다.",
        L"The bullet stops in the air for a moment after flying a set distance, then flies on.",
        L"弾が一定距離を飛んだ後、その場で少し止まってから再び飛びます。" },
      { L"멈췄다가", L"pause for", L"止まり" },
      { Term::Verb }, 1 },
    { { L"끌어당김", L"Pull", L"引き寄せ" },
      { L"주변 적을 한 점으로 끌어당깁니다. 보스·엘리트·원거리 적은 끌려오지 않습니다.",
        L"Drags nearby enemies toward one point. Bosses, elites and ranged enemies are not pulled.",
        L"周囲の敵を1点へ引き寄せます。ボス・エリート・遠距離の敵は引き寄せられません。" },
      { L"끌어당김|끌어모은", L"pulls enemies|gathers enemies", L"引き寄せ" },
      { Term::Verb, Term::Enemies }, 2 },
    { { L"왕복", L"Return", L"往復" },
      { L"일정 거리를 날아간 뒤 플레이어에게 돌아오며, 돌아오는 길에도 적을 맞힙니다.",
        L"After flying a set distance it comes back to you, hitting enemies on the way back too.",
        L"一定距離を飛んだ後プレイヤーへ戻り、戻る途中も敵に当たります。" },
      { L"되돌아옴|되돌아오며", L"turn back|come back", L"戻る|戻ってきて" },
      { Term::Verb }, 1 },
    { { L"자리바꿈", L"Swap", L"位置交換" },
      { L"맞힌 적과 플레이어의 위치를 서로 바꿉니다. 대시로 취급되어 대시 효과도 발동합니다.",
        L"You and the enemy hit trade places. It counts as a dash, so dash effects trigger too.",
        L"命中した敵とプレイヤーの位置を入れ替えます。ダッシュ扱いなのでダッシュ効果も発動します。" },
      { L"위치를 바꿈", L"swaps places", L"位置を入れ替える" },
      { Term::Verb, Term::Dash }, 2 },
    { { L"적탄 소거", L"Bullet clear", L"敵弾消去" },
      { L"범위 안의 적 탄환을 지웁니다. 지워진 탄은 피해를 주지 않습니다.",
        L"Removes enemy bullets in the area. Removed bullets deal no damage.",
        L"範囲内の敵の弾を消します。消えた弾はダメージを与えません。" },
      { L"적탄 소거|적탄을 지우고|적탄을 지움|적탄을 지워|적탄 전부 소거",
        L"clears enemy bullets|clears nearby enemy bullets|clears all enemy bullets|clear all enemy bullets|clears enemy bullets|erases enemy bullets|erases nearby enemy bullets|erase enemy bullets|erases bullets",
        L"敵弾を消去|敵弾を消す|敵弾を消し|敵弾消去|敵弾全消去" },
      { }, 0 },
    { { L"스침", L"Graze", L"かすり" },
      { L"적탄이 피격 판정에는 닿지 않고 바로 옆(판정 바깥 18 안)으로 지나가는 것입니다.",
        L"An enemy bullet passing just outside your hitbox (within 18 of it) without hitting you.",
        L"敵弾が被弾判定に触れず、すぐ横(判定の外側18以内)を通り過ぎることです。" },
      { L"스칠|스침", L"grazes|graze", L"かすめる|かすると" },
      { }, 0 },
    { { L"보호막", L"Shield", L"シールド" },
      { L"체력보다 먼저 피해를 받아 주는 막입니다. 막을 수 있는 양을 다 쓰면 깨집니다.",
        L"Takes damage before your HP does. It breaks once its amount is used up.",
        L"体力より先にダメージを受ける膜です。防げる量を使い切ると壊れます。" },
      { L"보호막", L"shield", L"シールド" },
      { }, 0 },
    { { L"정지", L"Standing still", L"静止" },
      { L"이동 입력 없이 제자리에 있는 상태입니다. 움직이거나 대시하면 바로 풀립니다.",
        L"Not moving at all. Moving or dashing ends it at once.",
        L"移動入力なしでその場にいる状態です。動くかダッシュするとすぐ解けます。" },
      { L"멈춰 서|멈춰 있|정지하면|정지 시|멈추면", L"standing still|stand still", L"立ち止ま|止まっている|静止|止まると" },
      { }, 0 },
    { { L"치명타", L"Critical hit", L"クリティカル" },
      { L"확률로 터지는 강한 타격입니다. 피해에 치명타 피해 배율(기본 2배)을 곱합니다.",
        L"A chance-based strong hit. Its damage is multiplied by the critical damage multiplier (x2 by default).",
        L"確率で発生する強い一撃です。ダメージにクリティカルダメージ倍率(基本2倍)を掛けます。" },
      { L"치명타|치명", L"critical|crits|crit", L"クリティカル|会心" },
      { Term::CritMult }, 1 },
    { { L"치명타 피해 배율", L"Critical damage multiplier", L"クリティカルダメージ倍率" },
      { L"치명타일 때 피해에 곱하는 값입니다. 기본 2배이며, '+0.5'나 '+50%'는 배율에 0.5를 더합니다 (2배 → 2.5배).",
        L"What a critical hit multiplies damage by. It is x2 by default; '+0.5' or '+50%' adds 0.5 to it (x2 -> x2.5).",
        L"クリティカル時にダメージに掛ける値です。基本2倍で、「+0.5」や「+50%」は倍率に0.5を足します (2倍 → 2.5倍)。" },
      { L"치명타 피해 배율|치명타 피해|치명 피해", L"critical damage multiplier|crit damage", L"クリティカルダメージ倍率|会心ダメージ" },
      { Term::Crit }, 1 },
    { { L"명중 횟수", L"Hit count", L"ヒット数" },
      { L"내 탄이 적에게 맞은 횟수입니다. 삼중 타격 같은 일부 추가 타격도 셉니다.",
        L"How many times your bullets have hit enemies. Some extra strikes, like Triple Strike, count too.",
        L"自分の弾が敵に当たった回数です。三重打撃など一部の追加攻撃も数えます。" },
      { L"명중 10회|명중 30회|명중 50회|명중 횟수", L"every 10 hits|every 30 hits|every 50 hits|hit count", L"10ヒット|30ヒット|50ヒット|ヒット数" },
      { }, 0 },
    { { L"피해 종류", L"Damage types", L"ダメージの種類" },
      { L"모든 피해: 탄·스킬·폭발 등 내가 주는 피해 전부. 탄환 피해: 기본 사격 탄만. 액티브 스킬 피해: 액티브 스킬만.",
        L"All damage: everything you deal (bullets, skills, blasts). Bullet damage: your normal shots only. Active skill damage: active skills only.",
        L"全ダメージ: 弾・スキル・爆発など自分が与えるダメージ全て。弾のダメージ: 通常射撃の弾だけ。アクティブスキルのダメージ: アクティブスキルだけ。" },
      { L"모든 피해|탄환 피해|액티브 스킬 피해", L"all damage|bullet damage|active skill damage", L"全ダメージ|弾のダメージ|アクティブスキルのダメージ" },
      { Term::ActiveSkill }, 1 },
    { { L"연사", L"Fire rate", L"連射" },
      { L"연사 간격: 한 발과 다음 발 사이의 시간 (짧을수록 빠름). 연사 속도: 1초에 쏘는 횟수 (높을수록 빠름).",
        L"Fire interval: time between two shots (shorter is faster). Fire rate: shots per second (higher is faster).",
        L"連射間隔: 1発と次の1発の間の時間 (短いほど速い)。連射速度: 1秒に撃つ回数 (高いほど速い)。" },
      { L"연사 간격|연사 속도|연사", L"fire interval|fire rate", L"連射間隔|連射速度|連射" },
      { }, 0 },
    { { L"사거리", L"Range", L"射程" },
      { L"탄이 사라지기 전까지 날아가는 거리입니다.",
        L"How far a bullet flies before it disappears.",
        L"弾が消えるまでに飛ぶ距離です。" },
      { L"사거리", L"range", L"射程" },
      { Term::Distance }, 1 },
    { { L"거리 숫자", L"Distance", L"距離の数値" },
      { L"거리 단위입니다. 플레이어 기체 크기가 25, 화면 가로가 1920입니다 (기본 확대 기준).",
        L"The distance unit. Your ship is 25 across and the screen is 1920 wide (at default zoom).",
        L"距離の単位です。自機の大きさが25、画面の横幅が1920です (基本の拡大率)。" },
      { L"주변 #|# 날아가|# 이상 멀", L"within #|flying #|#+ away", L"周囲#以内|#飛ぶ|#以上離れ" },
      { Term::LightYear }, 1 },
    { { L"별가루", L"Stardust", L"星屑" },
      { L"적이 떨어뜨리는 재화입니다. 주우면 모이고, 메인 메뉴 상점에서 영구 강화에 씁니다.",
        L"Currency that enemies drop. Pick it up to collect it, then spend it on permanent upgrades in the main menu shop.",
        L"敵が落とす通貨です。拾うと貯まり、メインメニューのショップで永久強化に使います。" },
      { L"별가루", L"stardust", L"星屑" },
      { }, 0 },
    { { L"행운", L"Luck", L"幸運" },
      { L"행운 1당 확률로 발동하는 패시브(삼중 타격·코로나 탄두 등)의 발동 확률과 높은 등급 증강이 나올 확률이 5%씩 높아집니다. 치명타 확률에는 영향이 없습니다.",
        L"Each point of luck makes chance-based passives (Triple Strike, Corona Round and so on) and higher-rarity augment offers 5% more likely. It does not change crit chance.",
        L"幸運1につき、確率で発動するパッシブ(三重打撃・コロナ弾頭など)の発動率と高レア強化の出現率が5%ずつ上がります。クリティカル率には影響しません。" },
      { L"행운", L"luck", L"幸運" },
      { Term::Stardust }, 1 },
    { { L"광년", L"Light-year", L"光年" },
      { L"이동 거리 단위입니다. 1광년 = 화면 가로의 1/4 (기본 확대에서 480). 대시·순간이동한 거리도 셉니다.",
        L"A travel distance unit. 1 light-year = 1/4 of the screen width (480 at default zoom). Dashes and teleports count.",
        L"移動距離の単位です。1光年 = 画面の横幅の1/4 (基本の拡大率で480)。ダッシュ・瞬間移動した距離も数えます。" },
      { L"광년", L"light-years|light-year", L"光年" },
      { Term::Distance }, 1 },
    { { L"효과 꺼짐", L"Stops working", L"効果が消える" },
      { L"이번 런에서 받은 피해를 모두 더해 기준을 넘으면, 그 증강의 효과가 런이 끝날 때까지 꺼집니다. 카드는 그대로 남습니다.",
        L"Once the damage you have taken this run adds up past the limit, the augment's effect is off for the rest of the run. You keep the card.",
        L"このランで受けたダメージの合計が基準を超えると、その強化の効果はラン終了まで消えます。カードはそのまま残ります。" },
      { L"효과가 꺼짐", L"stops working", L"効果が消える" },
      { Term::RunWave }, 1 },
    { { L"중첩 가능", L"Stackable", L"重複可" },
      { L"같은 카드를 여러 번 고를 수 있고, 고를 때마다 효과가 더해집니다.",
        L"You can pick the same card again; each pick adds its effect again.",
        L"同じカードを何度も選べ、選ぶたびに効果が加算されます。" },
      { L"중첩 가능", L"stackable", L"重複可" },
      { }, 0 },
    { { L"런 · 웨이브", L"Run · Wave", L"ラン · ウェーブ" },
      { L"런: 게임을 시작해서 끝날 때까지의 한 판. 웨이브: 한 판 안의 전투 단위입니다 (난이도에 따라 9~21웨이브).",
        L"Run: one game from start to end. Wave: one fight within a run (9 to 21 waves by difficulty).",
        L"ラン: ゲーム開始から終了までの1回分。ウェーブ: ラン内の戦闘単位です (難易度により9~21ウェーブ)。" },
      { L"이번 런|런당|런 동안|웨이브", L"this run|per run|for the run|the run|a wave|the wave|this wave|per wave|each wave|next wave|into a wave|wave cleared|wave ends|wave start", L"このラン|1ラン|ラン中|ウェーブ" },
      { }, 0 },
    { { L"액티브 스킬 · 쿨다운", L"Active skill · Cooldown", L"アクティブスキル · クールダウン" },
      { L"액티브 스킬: 키를 눌러 직접 쓰는 스킬 (증강으로 얻음). 쿨다운: 쓴 뒤 다시 쓸 수 있을 때까지 기다리는 시간.",
        L"Active skill: a skill you use with its key (gained from augments). Cooldown: the wait before you can use it again.",
        L"アクティブスキル: キーを押して自分で使うスキル (強化で得る)。クールダウン: 使ってから再び使えるまでの待ち時間。" },
      { L"액티브 스킬|쿨다운", L"active skills|active skill|cooldowns|cooldown", L"アクティブスキル|クールダウン" },
      { Term::SkillSlot }, 1 },
    { { L"대시", L"Dash", L"ダッシュ" },
      { L"짧은 거리를 빠르게 이동하는 회피 동작입니다. 쓰고 나면 쿨다운이 돕니다.",
        L"A quick short move to dodge. It goes on cooldown after use.",
        L"短い距離を素早く移動する回避動作です。使うとクールダウンに入ります。" },
      { L"대시", L"dashing|dashes|dash", L"ダッシュ" },
      { }, 0 },
    { { L"액티브 스킬 칸", L"Active skill slot", L"アクティブスキル枠" },
      { L"액티브 스킬을 장착하는 자리입니다. 칸 수만큼 액티브 스킬을 들 수 있습니다.",
        L"Where active skills are equipped. You can hold as many active skills as you have slots.",
        L"アクティブスキルを装備する枠です。枠の数だけアクティブスキルを持てます。" },
      { L"액티브 스킬 칸", L"active skill slot", L"アクティブスキル枠" },
      { Term::ActiveSkill }, 1 },
    { { L"반응 카드", L"Reaction card", L"反応カード" },
      { L"처치·치명타·튕김·피격·정지·대시·웨이브 같은 상황에서 발동하는 카드입니다. 발동 상황이 다르면 다른 종류로 셉니다.",
        L"A card that triggers on a situation: a kill, crit, bounce, getting hit, standing still, dash or wave. Different situations count as different kinds.",
        L"撃破・会心・跳弾・被弾・静止・ダッシュ・ウェーブなどの状況で発動するカードです。発動する状況が違えば別の種類として数えます。" },
      { L"반응 카드", L"reaction cards|reaction card", L"反応カード" },
      { }, 0 },
    { { L"위험 카드", L"Risky card", L"危険なカード" },
      { L"체력을 대가로 하거나 피격에 반응하는 카드: 유리대포·이단의 서약·연명 서약·무모한 질주·극한의 고행·회복 조각·빈사 폭주·최후의 배수진·피격 후 복수·원한 저장·최후의 방벽.",
        L"Cards that cost HP or react to getting hit: Glass Cannon, Heretic's Oath, Pact of Endurance, Reckless Dash, Extreme Austerity, Recovery Shard, Dying Frenzy, Last Stand, Post-Hit Revenge, Stored Grudge, Final Barrier.",
        L"体力を代償にするか被弾に反応するカード: ガラス砲・異端の誓約・延命の誓約・無謀な疾走・極限の苦行・回復の欠片・瀕死の暴走・背水の陣・被弾後の復讐・恨みの蓄積・最後の障壁。" },
      { L"위험 증강", L"risky card", L"危険な増強" },
      { }, 0 },
    { { L"확률로 발동하는 증강", L"Chance-based augments", L"確率で発動する強化" },
      { L"삼중 타격·코로나 탄두·크레이터 탄·별똥 수확·이중 시전·자기 폭풍입니다. 변광 사격과 확률 시너지 카드는 들어가지 않습니다.",
        L"Triple Strike, Corona Round, Crater Round, Stardust Harvest, Double Cast and Magnetic Storm. Variable Fire and the chance synergy cards are not included.",
        L"三重打撃・コロナ弾頭・クレーター弾・流れ星収穫・二重詠唱・磁気嵐です。変光射撃と確率シナジーカードは含みません。" },
      { L"확률로 발동하는 다른 증강|확률로 발동하는 증강", L"chance-based augment", L"確率で発動する他の強化|確率で発動する強化" },
      { Term::Luck }, 1 },
    { { L"적 분류", L"Enemy types", L"敵の分類" },
      { L"일반 적: 로터·스웜·스코프·그라비티·짐벌·마그네타·링크스·트라이곤. 엘리트: 제네시스·퀘이사·레굴루스·안타레스. 보스: 옥탄스·에리다누스.",
        L"Regular: Rotor, Swarm, Scope, Gravity, Gimbal, Magnetar, Lynx, Trigon. Elite: Genesis, Quasar, Regulus, Antares. Boss: Octans, Eridanus.",
        L"通常の敵: ローター・スウォーム・スコープ・グラビティ・ジンバル・マグネター・リンクス・トライゴン。エリート: ジェネシス・クエーサー・レグルス・アンタレス。ボス: オクタンス・エリダヌス。" },
      { L"일반 적|엘리트|보스", L"regular enemy|elites|elite|bosses|boss", L"通常の敵|雑魚|エリート|ボス" },
      { }, 0 },
};

inline const Entry& Get(Term t) { return kEntries[(int)t]; }

// Phrases that look like a term but are not one (card names, other words);
// they are skipped whole so nothing inside them links.
inline const wchar_t* const kBlocked[3] = {
    L"",
    L"Critical Burst|Critical Chain Burst|Stardust Harvest|Static Buildup|Stellar Dynamo",
    L"ランダム|を連射",
};

struct Span {
    int start = 0, len = 0;   // the linked characters
    int matchStart = 0, matchLen = 0;   // the whole phrase that matched
    Term term = Term::Count;
};

inline bool IsAsciiAlnum(wchar_t c) { return (c < 128) && std::iswalnum(c); }

// Matches phrase p (length n) at s[pos]. English ignores case and needs word
// edges. '#' takes one or more digits, which become the link.
inline bool MatchAt(const std::wstring& s, size_t pos, const wchar_t* p, size_t n, bool english, Span& out) {
    if (english && pos > 0 && IsAsciiAlnum(s[pos - 1])) return false;
    if (n > 0 && p[0] == L'#' && pos > 0 && std::iswdigit(s[pos - 1])) return false;
    size_t i = pos;
    int digStart = -1, digLen = 0;
    for (size_t k = 0; k < n; ++k) {
        if (p[k] == L'#') {
            const size_t d0 = i;
            while (i < s.size() && std::iswdigit(s[i])) ++i;
            if (i == d0) return false;
            digStart = (int)d0;
            digLen = (int)(i - d0);
            continue;
        }
        if (i >= s.size()) return false;
        const wchar_t a = s[i], b = p[k];
        if (english ? std::towlower(a) != std::towlower(b) : a != b) return false;
        ++i;
    }
    if (english && i < s.size() && IsAsciiAlnum(s[i]) && std::iswalpha(s[i - 1])) return false;
    if (digStart < 0 && n > 0 && p[n - 1] == L'#') return false;
    if (n > 0 && p[n - 1] == L'#' && i < s.size() && s[i] != L' ' && s[i] != L',' && s[i] != L')') return false;
    out.matchStart = (int)pos;
    out.matchLen = (int)(i - pos);
    out.start = digStart >= 0 ? digStart : (int)pos;
    out.len = digStart >= 0 ? digLen : (int)(i - pos);
    return true;
}

// Every phrase in s, left to right, longest first, not overlapping.
inline std::vector<Span> FindAll(const std::wstring& s, int lang) {
    std::vector<Span> out;
    if (lang < 0 || lang > 2) lang = 0;
    const bool english = lang == 1;
    size_t pos = 0;
    while (pos < s.size()) {
        Span best;
        best.matchLen = 0;
        for (int t = 0; t <= kTermCount; ++t) {   // kTermCount: the blocked phrases
            const wchar_t* list = t < kTermCount ? kEntries[t].phrases[lang] : kBlocked[lang];
            if (!list) continue;
            const wchar_t* a = list;
            for (const wchar_t* c = list;; ++c) {
                if (*c == L'|' || *c == 0) {
                    Span sp;
                    if (c > a && MatchAt(s, pos, a, (size_t)(c - a), english, sp) && sp.matchLen > best.matchLen) {
                        sp.term = (Term)t;
                        best = sp;
                    }
                    if (*c == 0) break;
                    a = c + 1;
                }
            }
        }
        if (best.matchLen > 0) {
            if (best.term != Term::Count) out.push_back(best);
            pos += (size_t)best.matchLen;
        } else {
            ++pos;
        }
    }
    return out;
}

// The links of one line: each term links only the first time in a card (used
// carries over between the lines of one card).
inline std::vector<Span> Link(const std::wstring& s, int lang, uint64_t& used) {
    std::vector<Span> out;
    for (const Span& sp : FindAll(s, lang)) {
        const uint64_t bit = 1ull << (int)sp.term;
        if (used & bit) continue;
        used |= bit;
        out.push_back(sp);
    }
    return out;
}

}  // namespace glossary
