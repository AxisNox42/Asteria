#pragma once
// ─────────────────────────────────────────────────────────────
//  Audio — miniaudio 기반 사운드 시스템 (가벼운 선언부)
//   · Sounds/ 폴더에서 로드. 파일이 없으면 조용히 무시(크래시 X).
//   · 파일을 하나씩 떨궈 넣으면 그때부터 자동으로 소리남.
//   · 볼륨은 Audio.cpp 의 게인 테이블에서 코드로 조절(파일 안 건드림).
//   · Sfx = 원본 파일, Cue = 게임 이벤트. Cue 하나가 원본 파일을
//     피치·볼륨·지연·컷으로 바꿔 재생한다 (Docs/QA/Asteria_사운드리스트.xlsx
//     '재활용 그룹' 시트 기준).
// ─────────────────────────────────────────────────────────────
namespace Audio {

enum class Sfx {
    Shoot,      // sfx_shoot.wav
    Kill,       // sfx_kill.wav
    Hurt,       // sfx_hurt.wav
    Death,      // sfx_death.wav
    UiHover,    // ui_hover.wav
    UiConfirm,  // ui_confirm.wav
    UiDeny,     // ui_deny.wav
    LevelUp,    // sfx_levelup.wav
    CardFlip,   // aug_flip.wav
    Dash,       // sfx_dash.wav
    EnemyHit,   // enemy_hit.wav
    Explosion,  // sfx_explode_m.wav
    Zap,        // sfx_chain_zap.wav
    Pickup,     // sfx_pickup_stardust.wav
    SkillCast,  // skill_cast.wav
    UiBack,     // ui_back.wav
    COUNT
};

enum class Cue {
    // UI
    UiHover, UiConfirm, UiBack, UiDeny, Pause, Resume,
    // 플레이어
    Shoot, FieldPulse, Dash, Hurt, Death,
    // 성장·픽업
    Pickup, LevelUp, ShopBuy,
    // 증강 카드
    CardDeal, CardFlip, CardPick,
    // 웨이브
    WaveStart, WaveClear,
    // 적
    EnemyHit, Kill, EliteKill, BossDeath,
    // 스킬·폭발
    SkillCast, ExplodeS, ExplodeM, ExplodeL, Lightning,
    COUNT
};

void Init(bool monoOutput = false);
void Shutdown();
void Play(Cue c);
void PlaySfx(Sfx s);      // 원본 그대로 (기존 호출 호환)
void PlayBgmMain();       // bgm_main.mp3 루프
void StopBgm();
void SetEnabled(bool on); // 마스터 ON/OFF (OFF 시 BGM 정지)
bool IsEnabled();
void SetBgmEnabled(bool on);
void SetSfxEnabled(bool on);
bool SetMonoOutput(bool mono);
bool IsMonoOutput();
void SetVolume(float v);  // 마스터 볼륨 0.0~1.0
void SetBgmVolume(float v);  // BGM 채널 볼륨 0.0~1.0
void SetSfxVolume(float v);  // 효과음 채널 볼륨 0.0~1.0

} // namespace Audio
