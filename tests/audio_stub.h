#pragma once
// 테스트용 무음 Audio — 게임 코드의 Audio::Play/StartLoop 호출을 링크만 되게 한다.
// 테스트 .cpp 하나에서만 include (정의를 담고 있음).
#include "../WiNILL/System/Audio.h"

namespace Audio {
void Init(bool) {}
void Shutdown() {}
void Update() {}
void Play(Cue) {}
void PlaySfx(Sfx) {}
void StartLoop(Cue) {}
void StopLoop(Cue) {}
void PlayBgm(Bgm, unsigned) {}
Bgm CurrentBgm() { return Bgm::None; }
void PlayBgmMain() {}
void StopBgm() {}
void FadeOutBgm(unsigned) {}
void SetEnabled(bool) {}
bool IsEnabled() { return false; }
void SetBgmEnabled(bool) {}
void SetSfxEnabled(bool) {}
bool SetMonoOutput(bool) { return true; }
bool IsMonoOutput() { return false; }
void SetVolume(float) {}
void SetBgmVolume(float) {}
void SetSfxVolume(float) {}
} // namespace Audio
