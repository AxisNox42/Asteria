// 자동 생성 — scratchpad tools/install_audio.py
// BGM(이름, 파일, 반복 여부, 루프 시작 프레임, 루프 끝 프레임) — miniaudio 디코드 기준 44.1 kHz
//   MP3 디코더가 앞에 1105 프레임 지연을 붙이므로 그 구간을 빼고 반복해야 이음매가 끊기지 않는다
BGM(Main, "Resource/Audio/bgm/bgm_main.mp3", true, 1105, 4420177)
BGM(Tutorial, "Resource/Audio/bgm/bgm_tutorial.mp3", true, 1105, 3975505)
BGM(Run, "Resource/Audio/bgm/bgm_run.mp3", true, 1105, 7940689)
BGM(Elite, "Resource/Audio/bgm/bgm_elite.mp3", true, 1105, 3298412)
BGM(BossOctans, "Resource/Audio/bgm/bgm_boss_octans.mp3", true, 1105, 4707025)
BGM(BossOctansPhase2, "Resource/Audio/bgm/bgm_boss_octans_phase2.mp3", true, 1105, 4137937)
BGM(BossEridanus, "Resource/Audio/bgm/bgm_boss_eridanus.mp3", true, 1105, 4235857)
BGM(BossEridanusPhase2, "Resource/Audio/bgm/bgm_boss_eridanus_phase2.mp3", true, 1105, 5445457)
BGM(Judgment, "Resource/Audio/bgm/stinger_judgment.mp3", true, 1105, 888145)
BGM(Victory, "Resource/Audio/bgm/stinger_victory.mp3", false, 0, 0)
BGM(Defeat, "Resource/Audio/bgm/stinger_defeat.mp3", false, 0, 0)
