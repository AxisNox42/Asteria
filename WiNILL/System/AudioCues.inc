// 자동 생성 — scratchpad tools/install_audio.py (손으로 고치지 말 것: 다시 생성하면 덮어씀)
// CUE(이름, 최소 간격 ms, 동시 재생 수, BGM 덕킹, 루프, 파일 개수, 파일...)
//   최소 간격·언제 재생은 Docs/QA 사운드 리스트 기준. 파일이 여러 개면 무작위로 하나.
// ui_hover — 모든 메뉴 버튼·리스트 항목에 마우스 호버 또는 방향키 이동
CUE(UiHover, 40, 3, false, false, 1, "Resource/Audio/ui_hover.wav")
// ui_confirm — 버튼 클릭·Enter
CUE(UiConfirm, 60, 3, false, false, 1, "Resource/Audio/ui_confirm.wav")
// ui_back — ESC·우클릭·BACK 레일
CUE(UiBack, 60, 3, false, false, 1, "Resource/Audio/ui_back.wav")
// ui_deny — 별가루 부족, 잠긴 항목, 예약 키, 잠긴 창 클릭
CUE(UiDeny, 120, 3, false, false, 1, "Resource/Audio/ui_deny.wav")
// sys_pause — ESC 로 플레이/튜토리얼 일시정지
CUE(Pause, 60, 3, false, false, 1, "Resource/Audio/sfx/sys_pause.wav")
// sys_resume — 일시정지 해제
CUE(Resume, 60, 3, false, false, 1, "Resource/Audio/sfx/sys_resume.wav")
// sfx_shoot — 소총 조준 사격 (머즐 플래시)
CUE(Shoot, 25, 6, false, false, 1, "Resource/Audio/sfx_shoot.wav")
// sfx_field_pulse — 전기장(Static Field) 0.24초마다 원형 펄스
CUE(FieldPulse, 200, 6, false, false, 1, "Resource/Audio/sfx/sfx_field_pulse.wav")
// sfx_dash — 대시 플래시 + 충격파
CUE(Dash, 60, 3, false, false, 1, "Resource/Audio/sfx_dash.wav")
// sfx_hurt — 플레이어 피해 (붉은 비네트·HP바 팝)
CUE(Hurt, 150, 3, false, false, 1, "Resource/Audio/sfx_hurt.wav")
// sfx_death — 대폭발 + 파편·충격파 (BGM 정지 직후)
CUE(Death, 60, 2, true, false, 1, "Resource/Audio/sfx_death.wav")
// sfx_pickup_stardust — 별가루 조각이 플레이어에 흡수 (XP+별가루)
CUE(Pickup, 35, 6, false, false, 1, "Resource/Audio/sfx_pickup_stardust.wav")
// sfx_levelup — 레벨 업 → XP 링 2개 (0.35초 전환)
CUE(LevelUp, 60, 3, false, false, 1, "Resource/Audio/sfx_levelup.wav")
// shop_buy — 구매·강화 성공 (링 플래시)
CUE(ShopBuy, 60, 3, false, false, 1, "Resource/Audio/sfx/shop_buy.wav")
// aug_deal — 덱에서 카드 최대 5장 배분
CUE(CardDeal, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_deal.wav")
// aug_flip — 카드가 순차로 뒤집힘
CUE(CardFlip, 70, 3, false, false, 1, "Resource/Audio/sfx/aug_flip.wav")
// aug_pick — 선택 카드 펄스 → 노드가 스킬 줄로 날아감 → 링·플래시
CUE(CardPick, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_pick.wav")
// wave_start — 웨이브 시작 / HUD 노드 도착 링
CUE(WaveStart, 60, 3, false, false, 1, "Resource/Audio/sfx/wave_start.wav")
// wave_clear — 대상 처치 → 보상 확정 → 2초 전환
CUE(WaveClear, 60, 3, false, false, 1, "Resource/Audio/sfx/wave_clear.wav")
// enemy_hit — 탄환이 적에게 명중 (히트 플래시)
CUE(EnemyHit, 30, 6, false, false, 1, "Resource/Audio/enemy_hit.wav")
// enemy_hit — 탄환이 적에게 명중 (히트 플래시)
CUE(Kill, 30, 6, false, false, 1, "Resource/Audio/enemy_hit.wav")
// enemy_kill_elite — 엘리트 사망 (히트스톱)
CUE(EliteKill, 60, 2, true, false, 1, "Resource/Audio/sfx/enemy_kill_elite.wav")
// boss_death — 충격파 3회 + 큰 흔들림·플래시
CUE(BossDeath, 60, 2, true, false, 1, "Resource/Audio/sfx/boss_death.wav")
// skill_cast — 액티브 증강 발동 순간 (Q/E/R 또는 자동 사용)
CUE(SkillCast, 60, 3, false, false, 1, "Resource/Audio/skill_cast.wav")
// sfx_explode_s — 작은 폭발 공용 (분열·지뢰·파편)
CUE(ExplodeS, 50, 6, false, false, 1, "Resource/Audio/sfx/sfx_explode_s.wav")
// sfx_explode_m — 중형 폭발 공용 (홍염탄·시한 폭탄)
CUE(ExplodeM, 80, 3, false, false, 1, "Resource/Audio/sfx_explode_m.wav")
// sfx_explode_l — 대형 폭발 공용 (운석·블랙홀·응축·신성)
CUE(ExplodeL, 60, 2, true, false, 1, "Resource/Audio/sfx/sfx_explode_l.wav")
// sfx_lightning — 낙뢰 1발 (삼연 낙뢰, 폭풍 방전, 뇌운 계열 패시브)
CUE(Lightning, 70, 3, false, false, 1, "Resource/Audio/sfx/sfx_lightning.wav")
// aug_flip_epic — 에픽 등급 카드 공개
CUE(AugFlipEpic, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_flip_epic.wav")
// aug_flip_legend — 전설 이상 카드 공개 (파편 추가 연출)
CUE(AugFlipLegend, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_flip_legend.wav")
// aug_flip_mythic — 신화 카드 공개
CUE(AugFlipMythic, 60, 2, true, false, 1, "Resource/Audio/sfx/aug_flip_mythic.wav")
// aug_flip_rare — 희귀 등급 카드 공개
CUE(AugFlipRare, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_flip_rare.wav")
// aug_shatter — 선택되지 않은 카드가 조각·별로 부서짐
CUE(AugShatter, 60, 3, false, false, 1, "Resource/Audio/sfx/aug_shatter.wav")
// boss_eridanus_appear — 
CUE(BossEridanusAppear, 60, 2, true, false, 1, "Resource/Audio/sfx/boss_eridanus_appear.wav")
// boss_eridanus_badsector — HP 손잡이를 잃을 때 불량 섹터 발생
CUE(BossEridanusBadsector, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_eridanus_badsector.wav")
// boss_eridanus_click — Spread: 커서가 클릭하며 씨앗 뿌림
CUE(BossEridanusClick, 80, 6, false, false, 1, "Resource/Audio/sfx/boss_eridanus_click.wav")
// boss_eridanus_corrupt — 셀 오염 (피해·약화)
CUE(BossEridanusCorrupt, 700, 1, false, false, 5, "Resource/Audio/sfx/boss_eridanus_corrupt_01.wav", "Resource/Audio/sfx/boss_eridanus_corrupt_02.wav", "Resource/Audio/sfx/boss_eridanus_corrupt_03.wav", "Resource/Audio/sfx/boss_eridanus_corrupt_04.wav", "Resource/Audio/sfx/boss_eridanus_corrupt_05.wav")
// boss_eridanus_heal — 오염 셀 회복
CUE(BossEridanusHeal, 900, 1, false, false, 1, "Resource/Audio/sfx/boss_eridanus_heal.wav")
// boss_eridanus_morph — Morph / Erode (2페이즈)
CUE(BossEridanusMorph, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_eridanus_morph.wav")
// boss_eridanus_scan — Scanline · Smear 패턴
CUE(BossEridanusScan, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_eridanus_scan.wav")
// boss_eridanus_shove — 경고 후 주기적 밀침 Wave
CUE(BossEridanusShove, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_eridanus_shove.wav")
// boss_eridanus_siphon — Siphon: Plug → Swallow → Spout
CUE(BossEridanusSiphon, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_eridanus_siphon.wav")
// boss_eridanus_warn — 화면 셀이 깜빡이며 오염 예고
CUE(BossEridanusWarn, 700, 1, false, false, 5, "Resource/Audio/sfx/boss_eridanus_warn_01.wav", "Resource/Audio/sfx/boss_eridanus_warn_02.wav", "Resource/Audio/sfx/boss_eridanus_warn_03.wav", "Resource/Audio/sfx/boss_eridanus_warn_04.wav", "Resource/Audio/sfx/boss_eridanus_warn_05.wav")
// boss_octans_aim — 추적 스트림 / Volley / Burst
CUE(BossOctansAim, 70, 6, false, false, 1, "Resource/Audio/sfx/boss_octans_aim.wav")
// boss_octans_appear — 
CUE(BossOctansAppear, 60, 2, true, false, 1, "Resource/Audio/sfx/boss_octans_appear.wav")
// boss_octans_close_count — 3초 종료 카운트 → 안에 있으면 압착
CUE(BossOctansCloseCount, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_close_count.wav")
// boss_octans_crash — 오류창 폭풍 → 3초 카운트 → 블루스크린 → 균열·산산조각
CUE(BossOctansCrash, 60, 2, true, false, 1, "Resource/Audio/sfx/boss_octans_crash.wav")
// boss_octans_fuse — '(Not Responding)' 퓨즈 → 폭파 → 레이저·브래킷
CUE(BossOctansFuse, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_fuse.wav")
// boss_octans_pane_burst — 창이 터짐 (투척·돌진·벽·폭파)
CUE(BossOctansPaneBurst, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_pane_burst.wav")
// boss_octans_pane_open — OS 창(pane) 열림 · 증원 · 벽 생성
CUE(BossOctansPaneOpen, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_pane_open.wav")
// boss_octans_ram — 창이 뒤로 당겨졌다 가장자리로 던져짐
CUE(BossOctansRam, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_ram.wav")
// boss_octans_refuge — 1.5초 예고 → 범람 링
CUE(BossOctansRefuge, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_refuge.wav")
// boss_octans_starout — HP 별 8개 중 하나가 꺼짐
CUE(BossOctansStarout, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_starout.wav")
// boss_octans_throw — 상자 창 생성 → 채움 → 비행 → 착지
CUE(BossOctansThrow, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_throw.wav")
// boss_octans_warp — 창 안으로 미끄러져 숨었다가 다른 창을 깨고 나옴
CUE(BossOctansWarp, 60, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_warp.wav")
// boss_phase_change — 2페이즈 진입 (OCTANS: 모든 창 폭파 / ERIDANUS: PhaseShift)
CUE(BossPhaseChange, 60, 2, true, false, 1, "Resource/Audio/sfx/boss_phase_change.wav")
// codex_locked — '미발견/???' 항목 선택
CUE(CodexLocked, 60, 3, false, false, 1, "Resource/Audio/sfx/codex_locked.wav")
// codex_open_entry — 리스트에서 항목 선택
CUE(CodexOpenEntry, 60, 3, false, false, 1, "Resource/Audio/sfx/codex_open_entry.wav")
// elite_antares_charge — 가속하며 추격 → 돌진 (발굽·엔진)
CUE(EliteAntaresCharge, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_antares_charge.wav")
// elite_antares_impact — 벽/다른 Antares 에 부딪혀 튕김·기절
CUE(EliteAntaresImpact, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_antares_impact.wav")
// elite_antares_split — 2마리로 분열
CUE(EliteAntaresSplit, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_antares_split.wav")
// elite_enemy_shield_break — 적 보호막 깨짐
CUE(EliteEnemyShieldBreak, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_enemy_shield_break.wav")
// elite_genesis_aim — 껍질 3개 충전 → 0.8초 조준
CUE(EliteGenesisAim, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_genesis_aim.wav")
// elite_genesis_death — 불안정 → 수축 → 폭발파
CUE(EliteGenesisDeath, 60, 2, true, false, 1, "Resource/Audio/sfx/elite_genesis_death.wav")
// elite_genesis_land — 던져진 Swarm 착지 충격파·기절
CUE(EliteGenesisLand, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_genesis_land.wav")
// elite_genesis_throw — Swarm/Rotor/캡슐 투척
CUE(EliteGenesisThrow, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_genesis_throw.wav")
// elite_quasar_beam — 3초 빔 (어려움 이상 벽 반사)
CUE(EliteQuasarBeam, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_quasar_beam.wav")
// elite_quasar_charge — 1초 예열 → 0.55초 잠금 (균열 이음새)
CUE(EliteQuasarCharge, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_quasar_charge.wav")
// elite_quasar_eruption — 가까이 있으면 링 단계 → 2박자 폭발
CUE(EliteQuasarEruption, 60, 2, false, false, 1, "Resource/Audio/sfx/elite_quasar_eruption.wav")
// elite_quasar_starfall_hit — 유성 착탄
CUE(EliteQuasarStarfallHit, 80, 6, false, false, 3, "Resource/Audio/sfx/elite_quasar_starfall_hit_01.wav", "Resource/Audio/sfx/elite_quasar_starfall_hit_02.wav", "Resource/Audio/sfx/elite_quasar_starfall_hit_03.wav")
// elite_quasar_starfall_warn — 유성 18발, 각 0.75초 경고
CUE(EliteQuasarStarfallWarn, 100, 6, false, false, 1, "Resource/Audio/sfx/elite_quasar_starfall_warn.wav")
// elite_quasar_supernova — 사망: 붕괴 → 정지 → 폭발
CUE(EliteQuasarSupernova, 60, 2, true, false, 1, "Resource/Audio/sfx/elite_quasar_supernova.wav")
// elite_regulus_beam — 3초마다 아군 3명에게 보호막/가속/은신 레이저
CUE(EliteRegulusBeam, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_regulus_beam.wav")
// elite_regulus_shield — 적에게 보호막 생성
CUE(EliteRegulusShield, 60, 3, false, false, 1, "Resource/Audio/sfx/elite_regulus_shield.wav")
// enemy_gimbal_fire — 1발 발사
CUE(EnemyGimbalFire, 40, 6, false, false, 1, "Resource/Audio/sfx/enemy_gimbal_fire.wav")
// enemy_gimbal_spin — 렌즈 2회전 스핀업
CUE(EnemyGimbalSpin, 300, 2, false, false, 1, "Resource/Audio/sfx/enemy_gimbal_spin.wav")
// enemy_gravis_loop — 중력장(반경 570) 안에 있는 동안 저음 웅웅
CUE(EnemyGravisLoop, 60, 1, false, true, 1, "Resource/Audio/sfx/enemy_gravis_loop.wav")
// enemy_lynx_aim — LYNX 조준선이 플레이어를 따라가는 1.2초 (차오르는 낮은 웅)
CUE(EnemyLynxAim, 150, 2, false, false, 1, "Resource/Audio/sfx/enemy_lynx_aim.wav")
// enemy_lynx_lock — 각도가 잠김 (철컥) — 이때 비키면 피한다
CUE(EnemyLynxLock, 80, 2, false, false, 1, "Resource/Audio/sfx/enemy_lynx_lock.wav")
// enemy_lynx_fire — 폭 32px 레이저 발사
CUE(EnemyLynxFire, 80, 2, false, false, 3, "Resource/Audio/sfx/enemy_lynx_fire_01.wav", "Resource/Audio/sfx/enemy_lynx_fire_02.wav", "Resource/Audio/sfx/enemy_lynx_fire_03.wav")
// enemy_mira_death — MIRA 가 죽고 지대가 남음 (낮은 베이스 '둥', ERIDANUS 와 같은 F 단조)
CUE(EnemyMiraDeath, 150, 2, false, false, 3, "Resource/Audio/sfx/enemy_mira_death_01.wav", "Resource/Audio/sfx/enemy_mira_death_02.wav", "Resource/Audio/sfx/enemy_mira_death_03.wav")
// enemy_mira_touch — 플레이어가 오염 지대에 들어섬 (낮게 타는 소리, 들어갈 때 한 번)
CUE(EnemyMiraTouch, 900, 1, false, false, 1, "Resource/Audio/sfx/enemy_mira_touch.wav")
// enemy_trigon_unstable — TRIGON 이 죽고 0.45초 우리가 덜컹임
CUE(EnemyTrigonUnstable, 120, 2, false, false, 1, "Resource/Audio/sfx/enemy_trigon_unstable.wav")
// enemy_trigon_shatter — 껍질이 터지고 새끼 Swarm 셋이 튀어나옴
CUE(EnemyTrigonShatter, 120, 2, false, false, 1, "Resource/Audio/sfx/enemy_trigon_shatter.wav")
// enemy_magnetar_block — 탄이 방패 면에 막힘
CUE(EnemyMagnetarBlock, 40, 6, false, false, 1, "Resource/Audio/sfx/enemy_magnetar_block.wav")
// enemy_magnetar_break — 4회 피격으로 면 파괴
CUE(EnemyMagnetarBreak, 60, 3, false, false, 1, "Resource/Audio/sfx/enemy_magnetar_break.wav")
// enemy_magnetar_restore — 8초 뒤 면 복구
CUE(EnemyMagnetarRestore, 60, 3, false, false, 1, "Resource/Audio/sfx/enemy_magnetar_restore.wav")
// enemy_scope_burst — 유도탄 3발 버스트
CUE(EnemyScopeBurst, 60, 3, false, false, 1, "Resource/Audio/sfx/enemy_scope_burst.wav")
// enemy_scope_charge — 스핀업 차지
CUE(EnemyScopeCharge, 60, 3, false, false, 1, "Resource/Audio/sfx/enemy_scope_charge.wav")
// enemy_shot — 적 일반 탄 발사 공용
CUE(EnemyShot, 60, 6, false, false, 1, "Resource/Audio/sfx/enemy_shot.wav")
// enemy_telegraph — 적 공격 차지/조준 예고 공용 (짧은 상승음)
CUE(EnemyTelegraph, 120, 6, false, false, 1, "Resource/Audio/sfx/enemy_telegraph.wav")
// meta_achievement — 4초 '업적 달성!' 토스트 + 코인 보상
CUE(MetaAchievement, 60, 3, false, false, 1, "Resource/Audio/sfx/meta_achievement.wav")
// os_arrange — 일시정지 중 Ctrl: 창 드래그/최소화/닫기
CUE(OsArrange, 60, 3, false, false, 1, "Resource/Audio/sfx/os_arrange.wav")
// os_pane_close — 창 닫힘(수축 애니메이션)
CUE(OsPaneClose, 60, 3, false, false, 1, "Resource/Audio/sfx/os_pane_close.wav")
// os_pane_glitch — 불안정 창, 글리치 버스트, 장식 오류창
CUE(OsPaneGlitch, 120, 3, false, false, 1, "Resource/Audio/sfx/os_pane_glitch.wav")
// os_pane_open — 런 중 OS 창 열림
CUE(OsPaneOpen, 60, 3, false, false, 1, "Resource/Audio/sfx/os_pane_open.wav")
// proc_auto_shot — 10초마다 강화탄 등 주기 자동 발사
CUE(ProcAutoShot, 40, 6, false, false, 1, "Resource/Audio/sfx/proc_auto_shot.wav")
// proc_counter_full — N발/N타/N킬 카운터가 차서 발동하는 순간 공용
CUE(ProcCounterFull, 200, 6, false, false, 1, "Resource/Audio/sfx/proc_counter_full.wav")
// proc_extra_hit — 추가 타격 3회 등 연속 타격
CUE(ProcExtraHit, 40, 6, false, false, 1, "Resource/Audio/sfx/proc_extra_hit.wav")
// proc_grow — 자라는 별 등 런 단위 누적 증가
CUE(ProcGrow, 500, 6, false, false, 1, "Resource/Audio/sfx/proc_grow.wav")
// proc_seal — 대시 봉인, 웨이브 동안 꺼지는 증강의 꺼짐/켜짐
CUE(ProcSeal, 40, 6, false, false, 1, "Resource/Audio/sfx/proc_seal.wav")
// proc_shockwave — 스킬 사용 자리 충격파, 대시 후 첫 사격 범위 피해 등
CUE(ProcShockwave, 150, 6, false, false, 1, "Resource/Audio/sfx/proc_shockwave.wav")
// proc_stardust_gain — 웨이브 클리어·처치 보너스 별가루
CUE(ProcStardustGain, 40, 6, false, false, 1, "Resource/Audio/sfx/proc_stardust_gain.wav")
// result_final_defeat — 
CUE(ResultFinalDefeat, 60, 2, true, false, 1, "Resource/Audio/sfx/result_final_defeat.wav")
// result_final_victory — 
CUE(ResultFinalVictory, 60, 2, true, false, 1, "Resource/Audio/sfx/result_final_victory.wav")
// result_tally — 레벨·획득 별가루 숫자 카운트업
CUE(ResultTally, 60, 3, false, false, 1, "Resource/Audio/sfx/result_tally.wav")
// result_wave_tick — 결과 화면 웨이브 트랙 재생, 웨이브당 1틱 (3웨이브 이후 가속)
CUE(ResultWaveTick, 40, 3, false, false, 1, "Resource/Audio/sfx/result_wave_tick.wav")
// run_launch_burst — 필드가 확장 + UI 흔들림 → 런 인트로
CUE(RunLaunchBurst, 60, 2, true, false, 1, "Resource/Audio/sfx/run_launch_burst.wav")
// run_launch_charge — Space 출격 — 필드가 조여듦
CUE(RunLaunchCharge, 60, 3, false, false, 1, "Resource/Audio/sfx/run_launch_charge.wav")
// set_hold_reset — 초기화 홀드 진행 중 (상승음) → 완료
CUE(SetHoldReset, 60, 3, false, false, 1, "Resource/Audio/sfx/set_hold_reset.wav")
// set_key_listen — 키 리바인딩 캡처 시작
CUE(SetKeyListen, 60, 3, false, false, 1, "Resource/Audio/sfx/set_key_listen.wav")
// set_key_ok — 키 수락 (키 플래시) / 'SWAPPED KEYS'
CUE(SetKeyOk, 60, 3, false, false, 1, "Resource/Audio/sfx/set_key_ok.wav")
// sfx_bullet_erase — 증강/스킬로 적탄이 지워짐
CUE(SfxBulletErase, 80, 6, false, false, 1, "Resource/Audio/sfx/sfx_bullet_erase.wav")
// sfx_chain_zap — 적 사이를 튕기는 전격 1회
CUE(SfxChainZap, 45, 6, false, false, 1, "Resource/Audio/sfx_chain_zap.wav")
// sfx_combo_milestone — 콤보 10/25/50/100
CUE(SfxComboMilestone, 60, 3, false, false, 1, "Resource/Audio/sfx/sfx_combo_milestone.wav")
// sfx_combo_tick — 처치 콤보 카운트 증가
CUE(SfxComboTick, 55, 6, false, false, 1, "Resource/Audio/sfx/sfx_combo_tick.wav")
// sfx_crit — 치명타 피해 숫자 표시
CUE(SfxCrit, 60, 6, false, false, 1, "Resource/Audio/sfx/sfx_crit.wav")
// sfx_dash_ready — 스킬 HUD 대시 슬롯 펄스
CUE(SfxDashReady, 40, 6, false, false, 1, "Resource/Audio/sfx/sfx_dash_ready.wav")
// sfx_heal_big — 별빛 치유, 궤도 회귀 회복, 운명 주사위 회복
CUE(SfxHealBig, 60, 3, false, false, 1, "Resource/Audio/sfx/sfx_heal_big.wav")
// sfx_heal_small — 재생·스침 회복 등 작은 회복
CUE(SfxHealSmall, 300, 6, false, false, 1, "Resource/Audio/sfx/sfx_heal_small.wav")
// sfx_kill — 적 사망 버스트
CUE(SfxKill, 55, 6, false, false, 1, "Resource/Audio/sfx_kill.wav")
// sfx_knockback — Antares 돌진, OCTANS 벽/크래시, ERIDANUS 밀침
CUE(SfxKnockback, 60, 3, false, false, 1, "Resource/Audio/sfx/sfx_knockback.wav")
// sfx_lowhp_loop — HP 25% 미만 동안 (심장 박동)
CUE(SfxLowhpLoop, 60, 1, false, true, 1, "Resource/Audio/sfx/sfx_lowhp_loop.wav")
// sfx_revive — 불멸의 재점화: 전체 회복 + 화면 전체 폭발
CUE(SfxRevive, 60, 2, true, false, 1, "Resource/Audio/sfx/sfx_revive.wav")
// sfx_shield_block — 보호 계열 증강이 피해를 막음
CUE(SfxShieldBlock, 100, 3, false, false, 1, "Resource/Audio/sfx/sfx_shield_block.wav")
// sfx_shield_break — 보호막/충전 소모 끝 파손
CUE(SfxShieldBreak, 60, 3, false, false, 1, "Resource/Audio/sfx/sfx_shield_break.wav")
// sfx_stardust_drop — 엘리트 사망 링에서 별가루가 쏟아짐
CUE(SfxStardustDrop, 60, 3, false, false, 1, "Resource/Audio/sfx/sfx_stardust_drop.wav")
// shop_maxed — 이미 최대·보유 항목 선택
CUE(ShopMaxed, 60, 3, false, false, 1, "Resource/Audio/sfx/shop_maxed.wav")
// shop_unlock — 런 중 첫 별가루 획득으로 상점이 열림
CUE(ShopUnlock, 60, 3, false, false, 1, "Resource/Audio/sfx/shop_unlock.wav")
// skill_absorb — 굴절 수정체 흡수, 탄막 환원 변환
CUE(SkillAbsorb, 40, 3, false, false, 1, "Resource/Audio/sfx/skill_absorb.wav")
// skill_beam — 낮과 밤의 경계 광선 3줄
CUE(SkillBeam, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_beam.wav")
// skill_boomerang_loop — 궤도 고리가 나갔다 돌아오는 동안
CUE(SkillBoomerangLoop, 60, 1, false, true, 1, "Resource/Audio/sfx/skill_boomerang_loop.wav")
// skill_charge_loop — 응축 대폭발 충전(최대 2초), 초정밀 저격 조준
CUE(SkillChargeLoop, 60, 1, false, true, 1, "Resource/Audio/sfx/skill_charge_loop.wav")
// skill_cooldown_cut — 공명 가속, 메아리 시전, 쿨다운 초기화 발동
CUE(SkillCooldownCut, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_cooldown_cut.wav")
// skill_dice — 운명 주사위 굴림 → 8가지 결과
CUE(SkillDice, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_dice.wav")
// skill_execute — 일점 처형 적중 (처치 시 skill_cooldown_cut)
CUE(SkillExecute, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_execute.wav")
// skill_fuse — 시한 폭탄 투척 후 1초 도화선
CUE(SkillFuse, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_fuse.wav")
// skill_hp_cost — 생명 연소 폭발 체력 30% 소모
CUE(SkillHpCost, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_hp_cost.wav")
// skill_proj_launch — 단발 대형 투사체 (유성 관통창, 홍염탄)
CUE(SkillProjLaunch, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_proj_launch.wav")
// skill_proj_multi — 여러 발 산개 (별조각 난사, 별빛 산개)
CUE(SkillProjMulti, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_proj_multi.wav")
// skill_ready — 스킬 슬롯 사용 가능해짐
CUE(SkillReady, 40, 6, false, false, 1, "Resource/Audio/sfx/skill_ready.wav")
// skill_rush — 혜성 돌진
CUE(SkillRush, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_rush.wav")
// skill_screenwide — 화면 안 모든 적 타격 (성단 폭격, 생명 연소 폭발, 자기 폭풍 등)
CUE(SkillScreenwide, 60, 2, true, false, 1, "Resource/Audio/sfx/skill_screenwide.wav")
// skill_slam — 중력 강타 (지면 강타 + 적탄 소거)
CUE(SkillSlam, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_slam.wav")
// skill_snipe — 초정밀 저격 관통 치명탄
CUE(SkillSnipe, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_snipe.wav")
// skill_summon — 쌍성 분신, 별빛 포탑, 폭풍 방전, 굴절 수정체, 성간 지뢰 설치
CUE(SkillSummon, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_summon.wav")
// skill_summon_end — 분신·포탑·성운·장벽 지속시간 종료
CUE(SkillSummonEnd, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_summon_end.wav")
// skill_telegraph — 운석 1초 예고 링, 저격 0.6초 조준선
CUE(SkillTelegraph, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_telegraph.wav")
// skill_teleport — 워프 점프, 궤도 회귀
CUE(SkillTeleport, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_teleport.wav")
// skill_turret_shot — 별빛 포탑 사격
CUE(SkillTurretShot, 60, 6, false, false, 1, "Resource/Audio/sfx/skill_turret_shot.wav")
// skill_vortex_loop — 블랙홀 포획 3초 흡입
CUE(SkillVortexLoop, 60, 1, false, true, 1, "Resource/Audio/sfx/skill_vortex_loop.wav")
// skill_wall — 소행성 장벽 생성, 적탄 막힘
CUE(SkillWall, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_wall.wav")
// skill_wave — 별바람 파동(부채꼴 밀쳐내기), 조석 대해일, 강제 방전
CUE(SkillWave, 60, 3, false, false, 1, "Resource/Audio/sfx/skill_wave.wav")
// sys_boot_log — '> 모듈 마운트 중 …' 줄이 한 줄씩 찍힐 때
CUE(SysBootLog, 50, 3, false, false, 1, "Resource/Audio/sfx/sys_boot_log.wav")
// sys_boot_open — 부팅 시 SceneAppWindow 프레임이 커지며 열림
CUE(SysBootOpen, 60, 3, false, false, 1, "Resource/Audio/sfx/sys_boot_open.wav")
// sys_boot_ready — '> 준비 완료.' + 진행 바 완료
CUE(SysBootReady, 60, 3, false, false, 1, "Resource/Audio/sfx/sys_boot_ready.wav")
// tut_dummy_hit — 연습용 허수아비 피격
CUE(TutDummyHit, 40, 6, false, false, 1, "Resource/Audio/sfx/tut_dummy_hit.wav")
// tut_dummy_respawn — 처치 3초 뒤 재생성
CUE(TutDummyRespawn, 60, 3, false, false, 1, "Resource/Audio/sfx/tut_dummy_respawn.wav")
// tut_step_clear — 이동/사격/대시/처치 단계 완료 → 다음 단계 해금
CUE(TutStepClear, 60, 3, false, false, 1, "Resource/Audio/sfx/tut_step_clear.wav")
// ui_button_in — 버튼이 하나씩 들어올 때
CUE(UiButtonIn, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_button_in.wav")
// ui_class_pick — 소총 / 전기장 선택
CUE(UiClassPick, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_class_pick.wav")
// ui_dialog_open — DrawConfirmDialog 표시 (종료·포기·재시작·메인 메뉴)
CUE(UiDialogOpen, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_dialog_open.wav")
// ui_diff_pick — 쉬움/보통/어려움/악몽
CUE(UiDiffPick, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_diff_pick.wav")
// ui_glitch — 튜토리얼 완료 후 TUTORIAL 버튼이 1회 글리치로 사라짐
CUE(UiGlitch, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_glitch.wav")
// ui_page — 페이지형 튜토리얼 넘김
CUE(UiPage, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_page.wav")
// ui_run_start — 런 시작 확정
CUE(UiRunStart, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_run_start.wav")
// ui_scene_out — 메뉴 선택 후 퇴장 트랜지션
CUE(UiSceneOut, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_scene_out.wav")
// ui_scroll — 상점 그리드·도감 리스트 스크롤
CUE(UiScroll, 50, 3, false, false, 1, "Resource/Audio/sfx/ui_scroll.wav")
// ui_slider_tick — 볼륨 등 슬라이더 드래그 중 값이 5 단위로 바뀔 때
CUE(UiSliderTick, 35, 3, false, false, 1, "Resource/Audio/sfx/ui_slider_tick.wav")
// ui_tab — 상점 탭, 도감 카테고리, 설정 아코디언
CUE(UiTab, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_tab.wav")
// ui_title_in — 로비 타이틀 표시 + 버튼 순차 슬라이드인
CUE(UiTitleIn, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_title_in.wav")
// ui_toggle — 설정 옵션 토글
CUE(UiToggle, 60, 3, false, false, 1, "Resource/Audio/sfx/ui_toggle.wav")
// ui_type — 도감 검색창 타이핑
CUE(UiType, 30, 3, false, false, 1, "Resource/Audio/sfx/ui_type.wav")
// wave_boss_warn — 보스 웨이브 시작 (현재 등장 연출 없음)
CUE(WaveBossWarn, 60, 2, true, false, 1, "Resource/Audio/sfx/wave_boss_warn.wav")
// wave_elite_warn — 웨이브 시작 1.5초 뒤 엘리트 등장 직전
CUE(WaveEliteWarn, 60, 3, false, false, 1, "Resource/Audio/sfx/wave_elite_warn.wav")
// wave_judgment_tick — '심판까지' 10초 카운트 매초
CUE(WaveJudgmentTick, 60, 3, false, false, 1, "Resource/Audio/sfx/wave_judgment_tick.wav")
// wave_tier_drop — 처치 속도 창이 끝나 보상 등급이 한 단계 떨어짐
CUE(WaveTierDrop, 60, 3, false, false, 1, "Resource/Audio/sfx/wave_tier_drop.wav")
// boss_octans_alert — 패턴이 열릴 때 OS 경고음
CUE(BossOctansAlert, 500, 2, false, false, 1, "Resource/Audio/sfx/boss_octans_alert.wav")
// boss_octans_error_pop — 블루스크린 패턴: 오류창 폭풍, 창마다
CUE(BossOctansErrorPop, 70, 4, false, false, 6, "Resource/Audio/sfx/boss_octans_error_pop_01.wav", "Resource/Audio/sfx/boss_octans_error_pop_02.wav", "Resource/Audio/sfx/boss_octans_error_pop_03.wav", "Resource/Audio/sfx/boss_octans_error_pop_04.wav", "Resource/Audio/sfx/boss_octans_error_pop_05.wav", "Resource/Audio/sfx/boss_octans_error_pop_06.wav")
// boss_octans_crash_charge — 블루스크린 패턴: 카운트다운 시작 과부하 상승음
CUE(BossOctansCrashCharge, 1000, 1, false, false, 1, "Resource/Audio/sfx/boss_octans_crash_charge.wav")
// boss_octans_crash_tick — 블루스크린 패턴: 카운트다운 매초 시스템 비프
CUE(BossOctansCrashTick, 500, 2, false, false, 1, "Resource/Audio/sfx/boss_octans_crash_tick.wav")
// boss_octans_crash_shatter — 블루스크린 패턴: 화면이 깨지며 게임이 다시 보임
CUE(BossOctansCrashShatter, 1000, 1, true, false, 1, "Resource/Audio/sfx/boss_octans_crash_shatter.wav")
// elite_quasar_starfall_launch — QUASAR 성단 폭격 시작: 성단이 터지며 파편이 솟구침
CUE(EliteQuasarStarfallLaunch, 500, 1, false, false, 1, "Resource/Audio/sfx/elite_quasar_starfall_launch.wav")
// elite_quasar_starfall_fly — QUASAR 성단 폭격: 유성 하나가 날아감 (착탄까지 0.75초)
CUE(EliteQuasarStarfallFly, 90, 4, false, false, 3, "Resource/Audio/sfx/elite_quasar_starfall_fly_01.wav", "Resource/Audio/sfx/elite_quasar_starfall_fly_02.wav", "Resource/Audio/sfx/elite_quasar_starfall_fly_03.wav")
// boss_octans_enter_pane — OCTANS 가 창 안으로 미끄러져 숨음
CUE(BossOctansEnterPane, 200, 2, false, false, 1, "Resource/Audio/sfx/boss_octans_enter_pane.wav")
// boss_octans_leave_pane — OCTANS 가 창을 깨고 나옴
CUE(BossOctansLeavePane, 200, 2, false, false, 1, "Resource/Audio/sfx/boss_octans_leave_pane.wav")
// boss_octans_wave — OCTANS 범람 파동이 퍼짐
CUE(BossOctansWave, 300, 2, false, false, 1, "Resource/Audio/sfx/boss_octans_wave.wav")
// boss_octans_volley — OCTANS 네 갈래 사격 반동
CUE(BossOctansVolley, 150, 3, false, false, 1, "Resource/Audio/sfx/boss_octans_volley.wav")
// boss_octans_reboot — 블루스크린 뒤 재부팅: 팬·디스크·삑
CUE(BossOctansReboot, 1000, 1, false, false, 1, "Resource/Audio/sfx/boss_octans_reboot.wav")
// boss_octans_shot — OCTANS 난사·조준 연사 한 발
CUE(BossOctansShot, 60, 3, false, false, 3, "Resource/Audio/sfx/boss_octans_shot_01.wav", "Resource/Audio/sfx/boss_octans_shot_02.wav", "Resource/Audio/sfx/boss_octans_shot_03.wav")
// boss_octans_warp_charge — OCTANS 2페이즈 창 속으로 빨려 들어가려 함 (0.7초)
CUE(BossOctansWarpCharge, 500, 1, false, false, 1, "Resource/Audio/sfx/boss_octans_warp_charge.wav")
