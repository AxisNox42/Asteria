# SFX 후보 풀 도구

효과음을 '검색해서 바로 가져오지 않고', SFX 마다 후보 20개 이상을 모아 분석한 뒤 서로 느낌이 비슷한 세트로 고르는 도구.
작업 폴더(예: 임시 폴더 `pool/`)에서 차례로 실행한다. 받은 후보 파일은 저장소에 넣지 않는다.

1. `python collect.py pool/candidates.json` — SFX 마다 검색어 6~9개 × 2쪽, Freesound CC0 후보 수집 (중복 제거)
2. 후보 미리듣기(HQ MP3)를 `pool/mp3/<id>.mp3` 로 받고 48 kHz WAV 로 `pool/wav/` 에 변환
3. `python features.py <저장소 루트>` (pool 안에서) — 길이·어택·밝기·저역·몸통·음높이 방향·날카로움·스펙트럼 기울기
4. `python pick_set.py` (pool 안에서) — 역할 적합 + 이미 고른 소리와의 질감 유사도로 차례로 선택,
   전투음 구별·크기 ↔ 밝기 순서는 제약. 결과 `selection.json`
5. 고른 소리를 `Docs/Audio/sfx_src/<파일>__<id>.wav` 로 옮기고 `scripts/build_sfx.py` 의 EXTERNAL 갱신
6. `python scripts/build_sfx.py` → `python scripts/check_sfx_fit.py`

주의: 파일 이름을 `select.py` 로 두면 파이썬 표준 모듈 `select` 를 가려 urllib 가 깨진다 (그래서 pick_set.py).
