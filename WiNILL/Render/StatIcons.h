#pragma once
// ─────────────────────────────────────────────────────────────
// 스탯 아이콘 — 스탯 : 기본 아이콘(색) + 배지(색) 표 한 곳에서 관리.
//   아이콘은 Tabler Icons(MIT) outline 을 흰색 선 마스크로 렌더링한 PNG
//   (Resource/Icons/STAT_*.png, 라이선스 Resource/Licenses/TablerIcons-LICENSE.txt).
//   색은 그릴 때 틴트로 입히고, 같은 모양의 굵은 *_KEY 마스크를 어두운
//   키라인으로 먼저 깔아 판 없이도 배경 위에서 읽히게 한다.
// ─────────────────────────────────────────────────────────────
#include "IconSystem.h"
#include "HudLines.h"
#include <cwchar>
#include <string>

namespace stat_icon {

using hud_lines::Rgb;

// 아이콘 모양 — 파일 이름은 "STAT_<이름>" (Tabler 이름을 대문자·밑줄로).
enum class Glyph {
    None = -1,
    Sword, Heart, Plus, Bolt, Shoe, Target, ChevronsUp, Wand, Hourglass,
    Shield, ShieldCheck, Gauge, ArrowsSplit, ArrowsMaximize, TrendingUp,
    Sparkles, Clover, Skull,
    Count
};

inline const char* GlyphFile(Glyph g) {
    static const char* const k[(int)Glyph::Count] = {
        "STAT_SWORD", "STAT_HEART", "STAT_PLUS", "STAT_BOLT", "STAT_SHOE",
        "STAT_TARGET", "STAT_CHEVRONS_UP", "STAT_WAND", "STAT_HOURGLASS",
        "STAT_SHIELD", "STAT_SHIELD_CHECK", "STAT_GAUGE", "STAT_ARROWS_SPLIT",
        "STAT_ARROWS_MAXIMIZE", "STAT_TRENDING_UP", "STAT_SPARKLES",
        "STAT_CLOVER", "STAT_SKULL" };
    return (g > Glyph::None && g < Glyph::Count) ? k[(int)g] : nullptr;
}

enum class Stat {
    Attack, MaxHp, Regen, FireRate, MoveSpeed, CritChance,
    CritDamage, AllDamage, SkillDamage, Cooldown, DamageTaken, Shield,
    BulletSpeed, BulletSpread, PlayerSize, XpGain, Stardust, Luck, Kills,
    Count
};

struct Def {
    Glyph base;  Rgb baseCol;
    Glyph badge; Rgb badgeCol;   // badge = Glyph::None 이면 없음
};

inline constexpr Rgb kRed    = { 0.98f, 0.40f, 0.38f };
inline constexpr Rgb kGreen  = { 0.36f, 0.92f, 0.52f };
inline constexpr Rgb kYellow = { 1.00f, 0.86f, 0.36f };
inline constexpr Rgb kSky    = { 0.42f, 0.82f, 1.00f };
inline constexpr Rgb kOrange = { 1.00f, 0.62f, 0.28f };
inline constexpr Rgb kPurple = { 0.76f, 0.56f, 1.00f };
inline constexpr Rgb kBlue   = { 0.36f, 0.62f, 1.00f };
inline constexpr Rgb kGrey   = { 0.74f, 0.80f, 0.88f };
inline constexpr Rgb kLime   = { 0.70f, 0.96f, 0.40f };
inline constexpr Rgb kGold   = { 1.00f, 0.80f, 0.42f };
inline constexpr Rgb kWhite  = { 1.00f, 1.00f, 1.00f };

// 스탯 : 아이콘 — 인덱스 = Stat 순서
inline const Def& DefOf(Stat s) {
    static const Def k[(int)Stat::Count] = {
        { Glyph::Sword,          kRed,    Glyph::None,       kWhite },  // 공격력
        { Glyph::Heart,          kGreen,  Glyph::None,       kWhite },  // 최대 체력
        { Glyph::Heart,          kGreen,  Glyph::Plus,       kWhite },  // 체력 재생
        { Glyph::Bolt,           kYellow, Glyph::None,       kWhite },  // 연사 속도
        { Glyph::Shoe,           kSky,    Glyph::None,       kWhite },  // 이동속도
        { Glyph::Target,         kOrange, Glyph::None,       kWhite },  // 치명타 확률
        { Glyph::Target,         kOrange, Glyph::Bolt,       kWhite },  // 치명타 피해 배율
        { Glyph::Sword,          kRed,    Glyph::ChevronsUp, kWhite },  // 모든 피해 증가
        { Glyph::Wand,           kPurple, Glyph::None,       kWhite },  // 액티브 스킬 피해
        { Glyph::Hourglass,      kSky,    Glyph::None,       kWhite },  // 쿨다운 감소
        { Glyph::Shield,         kGrey,   Glyph::None,       kWhite },  // 받는 피해 감소
        { Glyph::ShieldCheck,    kBlue,   Glyph::None,       kWhite },  // 보호막
        { Glyph::Gauge,          kYellow, Glyph::None,       kWhite },  // 탄환 속도
        { Glyph::ArrowsSplit,    kYellow, Glyph::None,       kWhite },  // 탄환 퍼짐
        { Glyph::ArrowsMaximize, kWhite,  Glyph::None,       kWhite },  // 플레이어 크기
        { Glyph::TrendingUp,     kLime,   Glyph::None,       kWhite },  // 경험치 배율
        { Glyph::Sparkles,       kGold,   Glyph::None,       kWhite },  // 별가루
        { Glyph::Clover,         kGreen,  Glyph::None,       kWhite },  // 행운
        { Glyph::Skull,          kWhite,  Glyph::None,       kWhite },  // 처치 수
    };
    return k[(int)s];
}

// 설명 문구 한 토막("최대 체력 +12")이 스탯 이름으로 시작하면 그 스탯.
//   카드 설명은 '·' 로 나뉜 토막마다 앞에 아이콘을 붙인다. 긴 이름을 먼저 둔다.
inline bool FromText(const std::wstring& seg, Stat& out) {
    struct Key { const wchar_t* text; Stat stat; };
    static const Key k[] = {
        // KR
        { L"최대 체력", Stat::MaxHp }, { L"체력 재생", Stat::Regen },
        { L"공격력", Stat::Attack }, { L"탄환 피해", Stat::Attack },
        { L"연사 속도", Stat::FireRate }, { L"연사 간격", Stat::FireRate },
        { L"이동속도", Stat::MoveSpeed },
        { L"치명타 확률", Stat::CritChance }, { L"치명타 피해", Stat::CritDamage },
        { L"모든 피해", Stat::AllDamage }, { L"액티브 스킬 피해", Stat::SkillDamage },
        { L"모든 액티브 스킬 쿨다운", Stat::Cooldown }, { L"모든 쿨다운", Stat::Cooldown },
        { L"대시 쿨다운", Stat::Cooldown },
        // EN
        { L"Max HP", Stat::MaxHp }, { L"HP regen", Stat::Regen },
        { L"Attack", Stat::Attack }, { L"Bullet damage", Stat::Attack },
        { L"Fire rate", Stat::FireRate }, { L"Fire interval", Stat::FireRate },
        { L"Move speed", Stat::MoveSpeed },
        { L"Critical chance", Stat::CritChance }, { L"Critical damage", Stat::CritDamage },
        { L"All damage", Stat::AllDamage }, { L"Active skill damage", Stat::SkillDamage },
        { L"All active skill cooldowns", Stat::Cooldown }, { L"All cooldowns", Stat::Cooldown },
        { L"Dash cooldown", Stat::Cooldown },
        // JP
        { L"最大体力", Stat::MaxHp }, { L"体力再生", Stat::Regen },
        { L"攻撃力", Stat::Attack }, { L"弾のダメージ", Stat::Attack },
        { L"連射速度", Stat::FireRate }, { L"連射間隔", Stat::FireRate },
        { L"移動速度", Stat::MoveSpeed },
        { L"クリティカル率", Stat::CritChance }, { L"クリティカルダメージ", Stat::CritDamage },
        { L"全ダメージ", Stat::AllDamage }, { L"アクティブスキルのダメージ", Stat::SkillDamage },
        { L"全アクティブスキルのクールダウン", Stat::Cooldown }, { L"全クールダウン", Stat::Cooldown },
        { L"ダッシュのクールダウン", Stat::Cooldown },
    };
    const size_t start = seg.find_first_not_of(L' ');
    if (start == std::wstring::npos) return false;
    for (const Key& key : k)
        if (seg.compare(start, std::wcslen(key.text), key.text) == 0) {
            out = key.stat;
            return true;
        }
    return false;
}

// 모양별 텍스처 [main, key] — 첫 사용 때 로드 (없으면 0 → 미표시).
inline GLuint Tex(Glyph g, bool key) {
    static GLuint tex[(int)Glyph::Count][2] = {};
    static bool tried[(int)Glyph::Count] = {};
    const char* file = GlyphFile(g);
    if (!file) return 0;
    const int i = (int)g;
    if (!tried[i]) {
        tried[i] = true;
        char keyName[64];
        std::snprintf(keyName, sizeof(keyName), "%s_KEY", file);
        tex[i][0] = IconLoad(file);
        tex[i][1] = IconLoad(keyName);
        IconEnableMipmaps(tex[i][0]);
        IconEnableMipmaps(tex[i][1]);
    }
    return tex[i][key ? 1 : 0];
}

inline void DrawGlyph(Glyph g, float cx, float cy, float size, Rgb c, float a) {
    if (a <= 0.002f) return;
    const float x = cx - size * 0.5f, y = cy - size * 0.5f;
    const Rgb k = hud_lines::KeyCol(c);
    DrawIcon(Tex(g, true), x, y, size, size, k.r, k.g, k.b, hud_lines::kKeyA * a);
    DrawIcon(Tex(g, false), x, y, size, size, c.r, c.g, c.b, a);
}

// (cx, cy) 중심, size 한 변. 배지는 오른쪽 아래에 작게 겹친다.
inline void Draw(Stat s, float cx, float cy, float size, float a) {
    const Def& d = DefOf(s);
    DrawGlyph(d.base, cx, cy, size, d.baseCol, a);
    if (d.badge != Glyph::None) {
        // Text-sized icons: the badge stays big enough to read and inside the line box.
        const float bs = size * 0.80f;
        DrawGlyph(d.badge, cx + size * 0.36f, cy + size * 0.20f, bs, d.badgeCol, a);
    }
}

}  // namespace stat_icon
