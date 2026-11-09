#pragma once
// ─────────────────────────────────────────────────────────────
//  Audio — miniaudio 기반 사운드 시스템 (가벼운 선언부)
//   · Resource/Audio/sfx, bgm 에서 로드. 파일이 없으면 조용히 무시(크래시 X).
//   · Cue = 게임 이벤트 하나 = 전용 파일 1개 이상(여러 개면 무작위).
//     파일마다 라우드니스 등급에 맞춰 크기를 맞춰 두었으므로 코드 게인은 모두 1.0,
//     피치·속도는 바꾸지 않고 원본 그대로 재생한다.
//   · 목록은 AudioCues.inc / AudioBgm.inc (scratchpad tools/install_audio.py 가 생성).
// ─────────────────────────────────────────────────────────────
namespace Audio {

// 예전 호출 호환용 — 같은 이름의 Cue 로 재생된다
enum class Sfx {
    Shoot, Kill, Hurt, Death, UiHover, UiConfirm, UiDeny, LevelUp,
    CardFlip, Dash, EnemyHit, Explosion, Zap, Pickup, SkillCast, UiBack,
    COUNT
};

enum class Cue {
#define CUE(name, ...) name,
#include "AudioCues.inc"
#undef CUE
    COUNT
};

enum class Bgm {
#define BGM(name, ...) name,
#include "AudioBgm.inc"
#undef BGM
    COUNT,
    None = COUNT
};

void Init(bool monoOutput = false);
void Shutdown();
void Update();            // 매 프레임: 덕킹 복귀, 끝난 BGM 정리
void Play(Cue c);
void PlaySfx(Sfx s);      // 예전 호출 호환
// 루프 효과음(…_loop): Start 는 이미 돌고 있으면 무시, Stop 은 짧게 페이드아웃
void StartLoop(Cue c);
void StopLoop(Cue c);
// BGM: 같은 곡이면 그대로 두고, 다른 곡이면 fadeMs 동안 크로스페이드
void PlayBgm(Bgm b, unsigned fadeMs = 800);
Bgm  CurrentBgm();
void PlayBgmMain();       // = PlayBgm(Bgm::Main)
void StopBgm();           // 즉시 정지
void FadeOutBgm(unsigned fadeMs);
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
