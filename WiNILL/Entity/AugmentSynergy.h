#pragma once
// Augment synergy redesign (Docs/Design/Augment_Synergy_Redesign.md):
// every card has a role, tags it gives and tags it needs. This header is the
// data + the pure evaluator, with no game dependencies, so the balance test
// (tests/augment_synergy_test.cpp) and the Excel export
// (scripts/build_synergy_xlsx.py) read the same numbers the game will.
//
// The evaluator is an expected-value model, not a simulation of the arena:
// it turns a loadout (one frame + cards) into damage per second against a
// reference crowd and into a survival factor for a 100 HP bar (an average
// enemy hit costs 10 HP). A shield is an amount of damage it blocks, and the
// Fx::shield value is that amount divided by 10.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace synergy {

enum Tag : uint64_t {
    T_NONE     = 0,
    T_PIERCE   = 1ull << 0,  T_BOUNCE   = 1ull << 1,  T_SPLIT    = 1ull << 2,
    T_HOMING   = 1ull << 3,  T_BLAST    = 1ull << 4,  T_BEAM     = 1ull << 5,
    T_ORBIT    = 1ull << 6,  T_CHAIN    = 1ull << 7,  T_RAPID    = 1ull << 8,
    T_CHARGE   = 1ull << 9,  T_SPREAD   = 1ull << 10, T_NTH      = 1ull << 11,
    T_ON_KILL  = 1ull << 12, T_ON_CRIT  = 1ull << 13, T_ON_BOUNCE= 1ull << 14,
    T_ON_HURT  = 1ull << 15, T_ON_STILL = 1ull << 16, T_ON_DASH  = 1ull << 17,
    T_ON_WAVE  = 1ull << 18, T_SHIELD   = 1ull << 19, T_HP      = 1ull << 20,
    T_TRAIL    = 1ull << 21, T_RETURN   = 1ull << 22, T_GLASS    = 1ull << 23,
    T_ON_WALL  = 1ull << 24, T_HOVER    = 1ull << 25, T_BURST    = 1ull << 26,
    T_BURN     = 1ull << 27, T_MARK     = 1ull << 28, T_DICE     = 1ull << 29,
    T_PULL     = 1ull << 30, T_ERASE    = 1ull << 31, T_TRAP     = 1ull << 32,
    T_GROW     = 1ull << 33, T_SWAP     = 1ull << 34, T_STORE    = 1ull << 35,
    T_COMBO    = 1ull << 36, T_RHYTHM   = 1ull << 37, T_LOB      = 1ull << 38,
};
inline constexpr int kTagCount = 39;

// Groups the "variety" cards count.
inline constexpr uint64_t kVerbTags = T_PIERCE | T_BOUNCE | T_SPLIT | T_HOMING | T_BLAST | T_ORBIT | T_CHAIN |
    T_TRAIL | T_RETURN | T_HOVER | T_PULL | T_BURN | T_MARK | T_SPREAD | T_GROW | T_TRAP | T_SWAP | T_ERASE;
inline constexpr uint64_t kRiskTags = T_GLASS | T_HP | T_ON_HURT | T_STORE;
inline constexpr uint64_t kEventTags = T_ON_KILL | T_ON_CRIT | T_ON_BOUNCE | T_ON_HURT | T_ON_STILL |
    T_ON_DASH | T_ON_WAVE | T_ON_WALL;

enum class Role { Frame, Verb, Trigger, Amp, Cost, Guard, Chance };
enum class Rar  { Common, Rare, Epic, Legendary, Mythic };

// What a card changes. Multipliers start at 1, everything else at 0.
struct Fx {
    float dmg = 1, rate = 1, acc = 1, move = 1, bullets = 1;   // multiplied together
    float dmgAdd = 0;                  // summed, applied as (1 + sum)
    int   pierce = 0, bounce = 0, split = 0, nth = 0;
    float splitDmg = 0, homing = 0, blast = 0, chain = 0, orbit = 0;
    float crit = 0, critMul = 0, critEcho = 0;
    float perBounce = 0, perPierce = 0, pierceBlast = 0, extraPass = 0;
    float proc = 0;                    // extra damage as a share of the reference dps
    float hpDelta = 0, hpSet = 0;      // hpSet > 0 caps max HP
    float shield = 0, evade = 0, hitCost = 1, taken = 0;
    float perTag = 0;                  // dmgAdd per distinct tag counted by Aug::scales (max 6)

#define FX_SET(n, t) Fx n##_(t v) const { Fx f = *this; f.n = v; return f; }
    FX_SET(dmg, float) FX_SET(rate, float) FX_SET(acc, float) FX_SET(move, float)
    FX_SET(bullets, float) FX_SET(dmgAdd, float)
    FX_SET(pierce, int) FX_SET(bounce, int) FX_SET(split, int) FX_SET(nth, int)
    FX_SET(splitDmg, float) FX_SET(homing, float) FX_SET(blast, float)
    FX_SET(chain, float) FX_SET(orbit, float) FX_SET(crit, float)
    FX_SET(critMul, float) FX_SET(critEcho, float) FX_SET(perBounce, float)
    FX_SET(perPierce, float) FX_SET(pierceBlast, float) FX_SET(extraPass, float)
    FX_SET(proc, float) FX_SET(hpDelta, float) FX_SET(hpSet, float)
    FX_SET(shield, float) FX_SET(evade, float) FX_SET(hitCost, float)
    FX_SET(taken, float) FX_SET(perTag, float)
#undef FX_SET
};

struct Aug {
    const char* code;      // 영문 코드명
    const char* kr;        // 카드 이름
    const char* rule;      // 카드에 적는 자기 규칙 (조합 결과는 적지 않음)
    Rar         rar;
    Role        role;
    uint64_t    gives;     // 이 카드가 붙이는 태그
    uint64_t    needs;     // 효과에 필요한 태그 (모두 있어야 함, 0 = 단독)
    bool        stack;     // 중복 획득 가능
    Fx          fx;
    uint64_t    scales = 0;  // 0 = none; else the effect grows with each distinct tag of this mask the loadout has
};

inline const char* RarName(Rar r) {
    static const char* n[] = { "일반", "희귀", "에픽", "전설", "신화" };
    return n[(int)r];
}
inline const char* RoleName(Role r) {
    static const char* n[] = { "프레임", "동사", "트리거", "증폭", "대가", "보호", "확률" };
    return n[(int)r];
}
inline const char* TagName(int bit) {
    static const char* n[kTagCount] = {
        "PIERCE","BOUNCE","SPLIT","HOMING","BLAST","BEAM","ORBIT","CHAIN","RAPID",
        "CHARGE","SPREAD","NTH","ON_KILL","ON_CRIT","ON_BOUNCE","ON_HURT","ON_STILL",
        "ON_DASH","ON_WAVE","SHIELD","HP","TRAIL","RETURN","GLASS","ON_WALL","HOVER",
        "BURST","BURN","MARK","DICE","PULL","ERASE","TRAP","GROW","SWAP","STORE","COMBO","RHYTHM","LOB" };
    return n[bit];
}

// ── Reference model constants (rough; tuned by the balance test) ──────────
inline constexpr float kBaseDamage   = 50.0f;
inline constexpr float kBaseInterval = 0.20f;   // s, the gun without cards
inline constexpr float kBaseAcc      = 0.75f;   // share of shots that hit a moving crowd
inline constexpr float kLane         = 1.5f;    // enemies lined up along a piercing shot
inline constexpr float kBounceBack   = 0.3f;    // share of bounces that hit again
inline constexpr float kChild        = 0.7f;    // share of split children that hit
inline constexpr float kCluster      = 1.8f;    // neighbours inside a blast
inline constexpr float kBaseHp       = 100.0f;
inline constexpr float kHpPerHit     = 10.0f;   // average damage of one enemy hit
inline constexpr float kHitsPerMin   = 10.0f;   // hits the player takes per minute, no cards
inline constexpr float kRefDps       = kBaseDamage * (1.0f / kBaseInterval) * kBaseAcc * 1.05f;   // 1.05: base crit

// Cards; index 0..4 are the frames (one per run).
inline const std::vector<Aug>& Table() {
    static const std::vector<Aug> t = {
    // ── 프레임 ──
    {"F_BASIC",   "기본 사격",  "지금의 기본 총", Rar::Common, Role::Frame, T_NONE, 0, false, Fx{}},
    {"F_MINIGUN", "미니건",     "연사 간격 0.07초, 탄 피해 -60%, 이동 -10%", Rar::Rare, Role::Frame,
        T_RAPID, 0, false, Fx{}.rate_(2.857f).dmg_(0.40f).acc_(0.92f).move_(0.9f)},
    {"F_LASER",   "레이저",     "빠른 광선 탄을 연달아 쏨 (모두 관통, 벽에서 반사 가능)", Rar::Rare, Role::Frame,
        T_BEAM | T_PIERCE, 0, false, Fx{}.rate_(2.0f).dmg_(0.27f).acc_(1.27f).pierce_(9)},
    {"F_SHOTGUN", "샷건",       "한 번에 7발 산개, 사거리 짧음", Rar::Rare, Role::Frame,
        T_SPREAD, 0, false, Fx{}.rate_(0.36f).dmg_(0.52f).acc_(0.66f).bullets_(7.0f)},
    {"F_SNIPER",  "저격총",     "느리지만 묵직한 관통탄을 쏨 (적 2명 관통)", Rar::Epic, Role::Frame,
        T_CHARGE | T_PIERCE, 0, false, Fx{}.rate_(0.35f).dmg_(1.5f).acc_(1.27f).pierce_(2)},
    {"F_BURST",   "점사총",     "세 발씩 끊어 쏘고 잠깐 쉼", Rar::Rare, Role::Frame,
        T_BURST, 0, false, Fx{}.rate_(0.55f).bullets_(3.0f).dmg_(0.68f).acc_(0.9f)},
    {"F_CROSSBOW","석궁",       "느리게 장전, 굵은 화살이 적 3명을 꿰뚫음", Rar::Epic, Role::Frame,
        T_PIERCE, 0, false, Fx{}.rate_(0.4f).dmg_(1.35f).acc_(1.27f).pierce_(3)},
    {"F_WAVE",    "파동포",     "느리고 거대한 파동을 쏨 — 적을 모두 뚫고 적탄을 지움", Rar::Epic, Role::Frame,
        T_PIERCE | T_ERASE, 0, false, Fx{}.rate_(0.22f).dmg_(2.6f).acc_(1.5f).pierce_(9)},
    {"F_MORTAR",  "박격포",     "조준한 자리에 포탄이 떨어져 폭발 — 벽을 넘음. 표식·화상·분열·중력은 착탄 지점에서 터짐", Rar::Epic, Role::Frame,
        T_BLAST | T_LOB, 0, false, Fx{}.rate_(0.40f).dmg_(0.85f).acc_(1.2f).blast_(0.9f)},
    // ── 동사 ──
    {"V_BOUNCE",  "벽 튕김 도탄",       "탄이 벽에서 2회 튕김", Rar::Epic, Role::Verb,
        T_BOUNCE, 0, false, Fx{}.bounce_(2)},
    {"V_RICOCHET","표적 반사탄",     "적에게 맞으면 다른 적 쪽으로 1회 튕김", Rar::Rare, Role::Verb,
        T_BOUNCE | T_ON_BOUNCE, 0, false, Fx{}.bounce_(1)},
    {"V_PIERCE",  "겹겹 관통탄",     "적을 2회 뚫음", Rar::Epic, Role::Verb,
        T_PIERCE, 0, false, Fx{}.pierce_(2)},
    {"V_SPLIT",   "갈래 분열탄",     "명중 시 3방향으로 분열 (피해 28%)", Rar::Epic, Role::Verb,
        T_SPLIT, 0, false, Fx{}.split_(3).splitDmg_(0.28f)},
    {"V_HOMING",  "추적 유도탄",     "가까운 적 쪽으로 완만히 휨", Rar::Rare, Role::Verb,
        T_HOMING, 0, false, Fx{}.homing_(0.26f)},
    {"V_BLAST",   "착탄 파열탄",     "맞힌 자리 주변 1○ 안에 작은 폭발 (공격력 30%)", Rar::Epic, Role::Verb,
        T_BLAST, 0, false, Fx{}.blast_(0.30f)},
    {"V_ORBIT",   "궤도 공전탄",     "5발마다 1발이 플레이어 주위를 돌며 접촉 피해 (최대 2개)", Rar::Epic, Role::Verb,
        T_ORBIT, 0, false, Fx{}.orbit_(0.60f)},
    {"V_CHAIN",   "번개 연쇄탄",     "처치 시 근처 적 3명에게 번개 (공격력 80%)", Rar::Epic, Role::Verb,
        T_CHAIN | T_ON_KILL, 0, false, Fx{}.chain_(0.3f)},
    {"V_TRAIL",   "잔상 궤적탄",     "탄이 지나간 길에 잔상이 1.2초간 남아, 닿은 적에게 초당 공격력 40%", Rar::Rare, Role::Verb,
        T_TRAIL, 0, false, Fx{}.proc_(0.35f)},
    {"V_HOVER",   "공중 정지탄",     "피해 +70%, 연사 -10% — 4○ 날아가면 0.8초 멈췄다가 다시 날아감", Rar::Epic, Role::Verb,
        T_HOVER, 0, false, Fx{}.rate_(0.9f).dmg_(1.7f)},
    {"V_RETURN",  "왕복 되돌이탄",   "6.5○ 날아가면 플레이어에게 되돌아옴 (돌아올 때도 적을 맞힘)", Rar::Legendary, Role::Verb,
        T_RETURN, 0, false, Fx{}.extraPass_(0.8f)},
    // ── 트리거 / 페이오프 ──
    {"T_NTH",     "박자 추가탄",   "6번째 사격마다 탄 1발 추가 (추가 탄도 관통·튕김 등 지금 탄의 효과를 그대로 가짐)", Rar::Rare, Role::Trigger,
        T_NTH, 0, false, Fx{}.nth_(5)},
    {"T_KILLBURST","처치 반응", "처치 시 12% 확률로 탄 4발 방사", Rar::Rare, Role::Trigger,
        T_ON_KILL, 0, false, Fx{}.proc_(0.26f)},
    {"T_CRITECHO","치명 반응",  "치명타 시 같은 방향으로 추가 탄 1발", Rar::Common, Role::Trigger,
        T_ON_CRIT, 0, false, Fx{}.critEcho_(3.0f)},
    {"T_REVENGE", "피격 후 복수",  "피해를 20 받을 때마다 주변 4○ 안의 적탄을 지우고 적에게 공격력 300% 피해", Rar::Epic, Role::Trigger,
        T_ON_HURT, 0, false, Fx{}.proc_(0.5f).evade_(0.08f)},
    {"T_STILL",   "정지 반응",  "1초 정지 시 다음 탄 피해 ×3", Rar::Rare, Role::Trigger,
        T_ON_STILL, 0, false, Fx{}.proc_(0.35f).move_(0.94f)},
    {"T_GRAZE",   "스침 충전",  "스침 30회마다 피해 25를 막는 보호막 충전", Rar::Common, Role::Trigger,
        T_SHIELD, 0, false, Fx{}.shield_(3.2f)},
    {"T_CAKE",    "회복 조각","피해를 20 받으면 다음 웨이브 시작에 체력 20 회복 (런 2회)", Rar::Common, Role::Trigger,
        T_HP, 0, false, Fx{}.shield_(3.8f)},
    // ── 증폭 (조건부) ──
    // ── 대가 ──
    {"C_GLASS",   "유리대포",   "모든 피해 ×3, 치명 피해 +50% — 최대 체력 30으로 고정", Rar::Legendary, Role::Cost,
        T_GLASS, 0, false, Fx{}.dmg_(3.1f).critMul_(0.5f).hpSet_(30.0f)},
    {"C_HERESY",  "이단의 서약","최대 체력 -20, 연사 +45%, 피해 +15%", Rar::Epic, Role::Cost,
        T_HP, 0, false, Fx{}.hpDelta_(-20.0f).rate_(1.45f).dmgAdd_(0.15f)},
    {"C_REVIVE",  "연명 서약",       "사망 대신 체력 20으로 부활 (1회), 이후 받는 피해 +8%", Rar::Rare, Role::Cost,
        T_HP, 0, false, Fx{}.shield_(7.0f).taken_(0.08f)},
    {"C_RECKLESS","무모한 질주","이동 +25%, 피해 +12%, 대시 쿨 -1.5초 — 받는 피해 2배", Rar::Rare, Role::Cost,
        T_HP, 0, false, Fx{}.dmgAdd_(0.12f).move_(1.25f).evade_(0.20f)},
    {"C_RITUAL",  "정밀한 의식","피해를 받지 않은 웨이브마다 공격 +8% 누적 (최대 +40%), 피해를 10 받으면 소멸", Rar::Rare, Role::Cost,
        T_ON_WAVE, 0, false, Fx{}.dmgAdd_(0.30f)},
    // ── 보호 ──
    {"G_CHARGER", "방벽 충전","웨이브를 클리어할 때마다 피해 40을 막는 보호막", Rar::Common, Role::Guard,
        T_SHIELD, 0, false, Fx{}.shield_(4.0f)},
    {"G_STILLGUARD","정지 방호","2초 이상 정지하면 피해 30을 막는 보호막 (재충전 20초)", Rar::Common, Role::Guard,
        T_SHIELD | T_ON_STILL, 0, false, Fx{}.shield_(3.6f).move_(0.97f)},
    {"G_PARRY",   "회피 반격",  "대시 시작 후 0.45초간 피격 면제, 대시가 끝나면 주변 3○ 안 적탄 소거", Rar::Common, Role::Guard,
        T_ON_DASH, 0, false, Fx{}.evade_(0.28f)},
    {"G_CIRCUIT", "순환 방벽",  "처치 40회마다 보호막 충전 (최대 2회)", Rar::Common, Role::Guard,
        T_SHIELD | T_ON_KILL, 0, false, Fx{}.shield_(3.2f)},
    // ── 동사 (추가) ──
    {"V_TWIN",    "쌍발 사격탄",     "한 번에 두 발씩 나란히 발사 (탄당 피해 감소)", Rar::Rare, Role::Verb,
        T_SPREAD, 0, false, Fx{}.bullets_(2.0f).dmg_(0.65f).acc_(0.95f)},
    {"V_MARK",    "낙인 표식탄",     "맞힌 적에게 5초간 표식, 표식 적은 받는 피해 +25%", Rar::Rare, Role::Verb,
        T_MARK, 0, false, Fx{}.dmgAdd_(0.2f)},
    {"V_BURN",    "불씨 점화탄",     "맞힌 적이 3초간 불탐 (초당 공격력 20%)", Rar::Rare, Role::Verb,
        T_BURN, 0, false, Fx{}.proc_(0.35f)},
    {"V_HUNTER",  "집요한 사냥탄",     "강하게 휘는 유도탄 — 6.5○ 날아가면 되돌아오며 다시 맞힘", Rar::Legendary, Role::Verb,
        T_HOMING | T_RETURN, 0, false, Fx{}.extraPass_(0.5f).homing_(0.10f)},
    {"V_SPRAY",   "부채꼴 산개탄",     "사격마다 좌우로 탄 2발 추가 (피해 35%)", Rar::Rare, Role::Verb,
        T_SPREAD, 0, false, Fx{}.bullets_(1.6f).dmg_(0.76f)},
    {"V_GRAVITY", "소형 중력탄",     "맞은 자리 주변 1.5○ 안의 적을 그쪽으로 최대 1○ 끌어당김 (보스·엘리트·원거리 적 제외)", Rar::Epic, Role::Verb,
        T_PULL, 0, false, Fx{}.proc_(0.55f)},
    // ── 증폭 (조건부, 추가) ──
    // ── 트리거 (추가) ──
    {"T_LASTSTAND","최후의 배수진",    "체력 30% 이하에서 피해 +50%", Rar::Rare, Role::Trigger,
        T_ON_HURT | T_HP, 0, false, Fx{}.dmgAdd_(0.3f).taken_(0.05f)},
    // ── 대가 (추가) ──
    {"C_ASCETIC", "극한의 고행",       "피해 +90%, 받는 피해 +20%", Rar::Legendary, Role::Cost,
        T_HP, 0, false, Fx{}.dmg_(1.9f).taken_(0.20f)},
    // ── 보호 (추가) ──
    {"G_FORCEFIELD","역장 방호","주변 2○ 안의 적탄 속도 -20%, 웨이브를 클리어할 때마다 피해 15를 막는 보호막", Rar::Common, Role::Guard,
        T_SHIELD, 0, false, Fx{}.evade_(0.15f).shield_(1.5f)},
    {"G_MIRROR",  "거울 반사 방벽",  "피해 90을 막는 보호막 1회 (재충전 없음) · 보호막이 깨질 때마다 주변 3.5○ 안의 적탄을 내 탄으로 되돌림", Rar::Epic, Role::Guard,
        T_SHIELD, 0, false, Fx{}.shield_(9.0f).proc_(0.10f)},
    {"G_LASTWALL","최후의 방벽","피해 220을 막는 보호막 (런당 1회, 치명 피해도 한 번 막음)", Rar::Legendary, Role::Guard,
        T_SHIELD | T_ON_HURT, 0, false, Fx{}.shield_(22.0f).proc_(0.10f)},
    // ── 특이한 동사 (새 규칙) ──
    {"V_DICE",    "주사위탄",   "발사할 때마다 관통·분열·튕김·폭발 중 하나를 무작위로 얻음", Rar::Epic, Role::Verb,
        T_PIERCE | T_BOUNCE | T_SPLIT | T_BLAST | T_DICE, 0, false, Fx{}.proc_(0.45f)},
    {"V_SWAP",    "자리바꿈탄", "맞힌 적과 위치를 바꿈 (1.5초에 한 번, 대시 효과도 발동)", Rar::Rare, Role::Verb,
        T_SWAP | T_ON_DASH, 0, false, Fx{}.evade_(0.12f).proc_(0.3f)},
    {"V_MINE",    "매설탄",     "3번째 사격마다 1발이 조준한 곳 쪽으로 날아가 지뢰가 됨 (8초 유지, 적이 1○ 안에 오면 공격력 220% 폭발)", Rar::Epic, Role::Verb,
        T_TRAP | T_BLAST, 0, false, Fx{}.proc_(0.18f).blast_(0.15f)},
    {"V_GROW",    "거리 성장탄",     "멀리 날아갈수록 탄이 커지고 피해도 늘어남 (최대 2.5배)", Rar::Rare, Role::Verb,
        T_GROW, 0, false, Fx{}.dmgAdd_(0.3f)},
    {"V_ABSORB",  "적탄 흡수탄",     "탄에 닿은 적탄을 지움 — 지운 적탄 1개당 다음 사격 피해 +5% (최대 +50%)", Rar::Rare, Role::Verb,
        T_ERASE, 0, false, Fx{}.evade_(0.12f).proc_(0.3f)},
    // ── 특이한 트리거 ──
    {"T_GRUDGE",  "원한 저장",  "받은 피해를 최대 60까지 저장, 다음 사격 때 피해 1당 공격력 10%의 폭발로 돌려줌", Rar::Rare, Role::Trigger,
        T_STORE | T_ON_HURT, 0, false, Fx{}.proc_(0.4f)},
    {"T_COMBO",   "연속 처치 콤보","3초 안에 처치를 이으면 피해가 쌓임 (최대 +40%)", Rar::Rare, Role::Trigger,
        T_COMBO | T_ON_KILL, 0, false, Fx{}.dmgAdd_(0.22f)},
    {"T_RHYTHM",  "박자 사격",  "4번째 사격마다 탄이 3배 피해", Rar::Rare, Role::Trigger,
        T_RHYTHM | T_NTH, 0, false, Fx{}.proc_(0.4f)},
    {"T_OPENING", "개전 일제사격","웨이브 시작 3초간 모든 사격이 두 발씩 나감", Rar::Rare, Role::Trigger,
        T_ON_WAVE, 0, false, Fx{}.proc_(0.3f)},
    {"T_AFTERIMAGE","잔영 탄막","대시한 길에 잔상이 2초간 남아, 닿은 적에게 초당 공격력 35%", Rar::Rare, Role::Trigger,
        T_ON_DASH | T_TRAIL, 0, false, Fx{}.proc_(0.3f).evade_(0.04f)},
    {"T_BERSERK", "빈사 폭주",  "체력이 낮을수록 연사 속도 증가 (최대 +60%)", Rar::Rare, Role::Trigger,
        T_HP, 0, false, Fx{}.rate_(1.35f).taken_(0.08f)},
    {"T_ANCHOR",  "닻 내리기",  "0.5초 이상 멈추면 탄이 커지고 (최대 +60%), 1초 이상 멈추면 관통 +1", Rar::Rare, Role::Trigger,
        T_ON_STILL | T_GROW, 0, false, Fx{}.proc_(0.35f).move_(0.96f)},
    {"T_FIREWORK","대시 폭죽",  "대시가 끝나면 사방으로 탄 12발", Rar::Rare, Role::Trigger,
        T_ON_DASH, 0, false, Fx{}.proc_(0.32f)},
    // ── 특이한 증폭 ──
    // ── 다양성 (가진 종류가 많을수록 강해짐) ──
    {"A_COLLECTOR","동사 수집가","가진 탄 효과(관통·튕김·분열·유도·폭발·화상 등)가 한 종류 늘 때마다 피해 +9% (최대 6종)", Rar::Rare, Role::Amp,
        T_NONE, 0, false, Fx{}.perTag_(0.09f), kVerbTags},
    {"A_REACTOR", "반응 공명",  "발동 조건이 다른 반응 카드(처치·치명타·튕김·피격·정지·대시·웨이브) 1종마다 피해 +14% (최대 6종)", Rar::Rare, Role::Amp,
        T_NONE, 0, false, Fx{}.perTag_(0.14f), kEventTags},
    {"A_GAMBLER", "위험 수집","위험 증강(체력·유리대포·피격·저장) 1장마다 피해 +10% (최대 6장)", Rar::Common, Role::Amp,
        T_NONE, 0, false, Fx{}.perTag_(0.10f), kRiskTags},
    // ── 신화 ──
    {"Y_ECHO",    "겹겹 메아리 사격","모든 사격을 0.2초 뒤 같은 방향으로 한 번 더 쏨 (피해 동일)", Rar::Mythic, Role::Verb,
        T_NTH, 0, false, Fx{}.bullets_(2.0f).dmg_(1.1f).acc_(0.95f)},
    {"Y_SINGULAR","특이점 붕괴 탄두","명중할 때마다 맞은 자리 주변 2.5○ 안의 적을 끌어모은 뒤 폭발 (공격력 150%, 보스·엘리트·원거리 적은 끌려오지 않음)", Rar::Mythic, Role::Verb,
        T_BLAST | T_PULL, 0, false, Fx{}.blast_(0.60f).proc_(0.25f)},
    {"Y_HALO",    "별빛 후광 궤도탄","3발마다 1발이 후광이 되어 플레이어를 돌며 적탄을 지움 (최대 4개)", Rar::Mythic, Role::Verb,
        T_ORBIT | T_ERASE, 0, false, Fx{}.orbit_(1.2f)},
    {"Y_ETERNAL", "영겁의 반사탄","탄이 벽에서 3번 더 튕김, 튕길 때마다 피해 +22%", Rar::Mythic, Role::Verb,
        T_BOUNCE | T_ON_BOUNCE, 0, false, Fx{}.bounce_(3).perBounce_(0.22f)},
    {"Y_FATE",    "운명의 필살 일격","치명타 확률 +30%, 치명타 피해 +100%", Rar::Mythic, Role::Trigger,
        T_ON_CRIT, 0, false, Fx{}.crit_(0.30f).critMul_(1.0f).critEcho_(1.0f)},
    // ── 확률 카드: 다른 카드 없이 혼자 작동, 조건을 만족하는 동안 한 가지 확률이 오름 ──
    {"P_STILL_CRIT", "정지 치명", "0.5초 이상 멈춰 있을 때 치명타 확률 +65%", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.163f)},
    {"P_STILL_PIERCE", "정지 관통운", "0.5초 이상 멈춰 있을 때 탄이 100% 확률로 관통 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.275f)},
    {"P_STILL_SPLIT", "정지 분열운", "0.5초 이상 멈춰 있을 때 탄이 100% 확률로 3갈래 분열 (피해 28%)", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.325f)},
    {"P_MOVING_EXTRA", "질주 연발운", "움직이는 동안 사격마다 50% 확률로 탄 1발 추가", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.375f)},
    {"P_MOVING_BOUNCE", "질주 도탄운", "움직이는 동안 탄이 35% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.262f)},
    {"P_MOVING_BLAST", "질주 폭발", "움직이는 동안 탄이 20% 확률로 착탄 시 폭발", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.165f)},
    {"P_LOWHP_PIERCE", "위기 관통", "체력이 50% 이하일 때 탄이 65% 확률로 관통 +1", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.179f)},
    {"P_LOWHP_SPLIT", "위기 분열운", "체력이 50% 이하일 때 탄이 100% 확률로 3갈래 분열 (피해 28%)", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.325f)},
    {"P_LOWHP_CRIT", "위기 치명운", "체력이 50% 이하일 때 치명타 확률 +100%", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.250f)},
    {"P_FULLHP_BOUNCE", "만전 도탄운", "체력이 가득 찼을 때 탄이 85% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.383f)},
    {"P_FULLHP_BLAST", "만전 폭발운", "체력이 가득 찼을 때 탄이 60% 확률로 착탄 시 폭발", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.297f)},
    {"P_FULLHP_EXTRA", "만전 연발", "체력이 가득 찼을 때 사격마다 35% 확률로 탄 1발 추가", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.157f)},
    {"P_SHIELD_SPLIT", "방벽 분열운", "보호막이 있을 때 탄이 40% 확률로 3갈래 분열 (피해 28%)", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.208f)},
    {"P_SHIELD_CRIT", "방벽 치명운", "보호막이 있을 때 치명타 확률 +100%", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.400f)},
    {"P_SHIELD_PIERCE", "방벽 관통운", "보호막이 있을 때 탄이 95% 확률로 관통 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.418f)},
    {"P_DASH_BLAST", "도약 폭발", "대시 후 2초 동안 탄이 100% 확률로 착탄 시 폭발", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.165f)},
    {"P_DASH_EXTRA", "도약 연발", "대시 후 2초 동안 사격마다 100% 확률로 탄 1발 추가", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.150f)},
    {"P_DASH_BOUNCE", "도약 도탄", "대시 후 2초 동안 탄이 100% 확률로 벽 튕김 +1", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.150f)},
    {"P_KILL_CRIT", "연승 치명", "처치 후 3초 동안 치명타 확률 +20%", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.160f)},
    {"P_KILL_PIERCE", "연승 관통기운", "처치 후 3초 동안 탄이 65% 확률로 관통 +1", Rar::Epic, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.572f)},
    {"P_KILL_SPLIT", "연승 분열기운", "처치 후 3초 동안 탄이 50% 확률로 3갈래 분열 (피해 28%)", Rar::Epic, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.520f)},
    {"P_WAVE_EXTRA", "개막 연발운", "웨이브 시작 후 5초 동안 사격마다 100% 확률로 탄 1발 추가", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.200f)},
    {"P_WAVE_BOUNCE", "개막 도탄운", "웨이브 시작 후 5초 동안 탄이 100% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.200f)},
    {"P_WAVE_BLAST", "개막 폭발", "웨이브 시작 후 5초 동안 탄이 80% 확률로 착탄 시 폭발", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.176f)},
    {"P_CROWD_PIERCE", "혼전 관통", "주변 4○ 안에 적이 8명 이상일 때 탄이 30% 확률로 관통 +1", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.165f)},
    {"P_CROWD_SPLIT", "혼전 분열기운", "주변 4○ 안에 적이 8명 이상일 때 탄이 100% 확률로 3갈래 분열 (피해 28%)", Rar::Epic, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.650f)},
    {"P_FEW_BOUNCE", "소수 도탄운", "남은 적이 3명 이하일 때 탄이 100% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.300f)},
    {"P_FEW_BLAST", "소수 폭발운", "남은 적이 3명 이하일 때 탄이 100% 확률로 착탄 시 폭발", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.330f)},
    {"P_BOSS_SPLIT", "거물 분열", "보스가 있을 때 탄이 100% 확률로 3갈래 분열 (피해 28%)", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.195f)},
    {"P_BOSS_CRIT", "거물 치명", "보스가 있을 때 치명타 확률 +100%", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.150f)},
    {"P_FAR_BLAST", "원거리 폭발운", "가장 가까운 적이 4.5○ 이상 멀 때 탄이 75% 확률로 착탄 시 폭발", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.289f)},
    {"P_FAR_EXTRA", "원거리 연발", "가장 가까운 적이 4.5○ 이상 멀 때 사격마다 45% 확률로 탄 1발 추가", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.157f)},
    {"P_BARRAGE_CRIT", "탄막 치명", "주변 2.5○ 안에 적탄이 3발 이상일 때 치명타 확률 +30%", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.150f)},
    {"P_BARRAGE_PIERCE", "탄막 관통기운", "주변 2.5○ 안에 적탄이 3발 이상일 때 탄이 100% 확률로 관통 +1", Rar::Epic, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.550f)},
    {"P_CLEAN_EXTRA", "무결 연발운", "이번 웨이브에 피해를 받지 않았을 때 사격마다 100% 확률로 탄 1발 추가", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.bullets_(1.400f)},
    {"P_CLEAN_BOUNCE", "무결 도탄운", "이번 웨이브에 피해를 받지 않았을 때 탄이 95% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.380f)},
    {"P_HURT_PIERCE", "분노 관통운", "피해를 받은 후 3초 동안 탄이 100% 확률로 관통 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.330f)},
    {"P_HURT_SPLIT", "분노 분열운", "피해를 받은 후 3초 동안 탄이 85% 확률로 3갈래 분열 (피해 28%)", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.332f)},
    {"P_COMBO_BOUNCE", "고조 도탄운", "처치 콤보가 5 이상일 때 탄이 75% 확률로 벽 튕김 +1", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.262f)},
    {"P_COMBO_BLAST", "고조 폭발", "처치 콤보가 5 이상일 때 탄이 45% 확률로 착탄 시 폭발", Rar::Common, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.173f)},
    {"P_LONG_SPLIT", "지구전 분열운", "웨이브 시작 20초 뒤부터 탄이 40% 확률로 3갈래 분열 (피해 28%)", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.dmgAdd_(0.208f)},
    {"P_LONG_CRIT", "지구전 치명운", "웨이브 시작 20초 뒤부터 치명타 확률 +100%", Rar::Rare, Role::Chance,
        T_NONE, 0, false, Fx{}.crit_(0.400f)},
    };
    return t;
}

inline constexpr int kFrameCount = 9;

inline int FindCode(const char* code) {
    const auto& t = Table();
    for (int i = 0; i < (int)t.size(); ++i) {
        const char* a = t[i].code;
        const char* b = code;
        while (*a && *a == *b) { ++a; ++b; }
        if (!*a && !*b) return i;
    }
    return -1;
}

struct Result {
    float dps = 0;          // expected damage per second
    float surv = 1;         // time-to-death relative to the bare 100 HP bar
    float power = 1;        // (dps / reference dps) * surv^0.4
    int   dead = 0;         // cards whose needs are not met
    uint64_t tags = 0;
};

// frame: index 0..kFrameCount-1; cards: indices into Table() (frames excluded).
inline Result Evaluate(int frame, const std::vector<int>& cards) {
    const auto& t = Table();
    Result r;
    r.tags = t[frame].gives;
    for (int c : cards) r.tags |= t[c].gives;

    Fx a = t[frame].fx;
    for (int c : cards) {
        const Aug& g = t[c];
        if ((g.needs & r.tags) != g.needs) { ++r.dead; continue; }
        const Fx& f = g.fx;
        if (g.scales) {
            int n = 0;
            for (uint64_t m = r.tags & g.scales; m; m &= m - 1) ++n;
            a.dmgAdd += f.perTag * (float)std::min(n, 6);
        }
        a.dmg *= f.dmg; a.rate *= f.rate; a.acc *= f.acc; a.move *= f.move; a.bullets *= f.bullets;
        a.dmgAdd += f.dmgAdd; a.pierce += f.pierce; a.bounce += f.bounce; a.split += f.split;
        if (f.nth > 0) a.nth = a.nth > 0 ? std::min(a.nth, f.nth) : f.nth;
        a.splitDmg += f.splitDmg; a.homing += f.homing; a.blast += f.blast; a.chain += f.chain;
        a.orbit += f.orbit; a.crit += f.crit; a.critMul += f.critMul; a.critEcho += f.critEcho;
        a.perBounce += f.perBounce; a.perPierce += f.perPierce; a.pierceBlast += f.pierceBlast;
        a.extraPass += f.extraPass; a.proc += f.proc; a.hpDelta += f.hpDelta;
        if (f.hpSet > 0) a.hpSet = a.hpSet > 0 ? std::min(a.hpSet, f.hpSet) : f.hpSet;
        a.shield += f.shield; a.evade += f.evade; a.hitCost *= f.hitCost; a.taken += f.taken;
    }
    a.pierce = std::min(a.pierce, 5);
    a.bounce = std::min(a.bounce, 4);
    a.split  = std::min(a.split, 5);

    const float shotsPerSec  = a.rate / kBaseInterval;
    const float nthExtra     = a.nth > 0 ? 1.0f / (float)a.nth : 0.0f;
    const float bulletsPerSec = shotsPerSec * std::min(a.bullets, 12.0f) * (1.0f + nthExtra);
    const float acc    = std::clamp(kBaseAcc * a.acc + a.homing, 0.05f, 0.98f);
    const float lane   = std::min(1.0f + (float)a.pierce, kLane);
    const float passes = std::min(2.2f, 1.0f + (float)a.bounce * kBounceBack + a.extraPass);
    const float critC  = std::clamp(0.05f + a.crit, 0.0f, 0.9f);
    const float critF  = 1.0f + critC * (1.0f + a.critMul);
    const float dmgHit = kBaseDamage * a.dmg * (1.0f + a.dmgAdd) * critF;
    const float bounceF = 1.0f + a.perBounce * ((float)a.bounce * 0.5f + a.extraPass * 0.5f);
    const float pierceF = 1.0f + a.perPierce * (lane - 1.0f) * 0.5f;

    // Hits per bullet are capped: a crowd is only so deep and the arena only so wide.
    const float reach = std::min(passes * lane, 3.0f);
    float dps = bulletsPerSec * acc * reach * dmgHit * bounceF * pierceF;
    const float extra = (float)a.split * a.splitDmg * kChild
                      + a.blast * kCluster
                      + a.pierceBlast * (lane - 1.0f)
                      + critC * a.critEcho;
    dps *= 1.0f + extra;
    dps += kRefDps * (a.orbit + a.chain * 2.0f + a.proc);
    r.dps = dps;

    float hp = kBaseHp + a.hpDelta;
    if (a.hpSet > 0) hp = std::min(hp, a.hpSet);
    hp = std::max(kHpPerHit, hp);
    const float hitRate = kHitsPerMin * (1.0f - std::min(a.evade, 0.6f)) * (1.0f + a.taken)
                        / std::sqrt(std::max(0.3f, a.move));
    const float lives = hp / kHpPerHit / a.hitCost + a.shield;
    r.surv  = (lives / hitRate) / (kBaseHp / kHpPerHit / kHitsPerMin);
    r.power = (dps / kRefDps) * std::pow(r.surv, 0.5f);
    // Overkill: damage past what a screen of enemies can absorb is worth less.
    if (r.power > 6.0f) r.power = 6.0f + (r.power - 6.0f) * 0.4f;
    return r;
}

}  // namespace synergy
