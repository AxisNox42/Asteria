# Asteria 작업 규칙

## 작업 시작 전: 원격 변경 확인 (필수)

여러 사람과 여러 Claude Code 세션이 같은 저장소(`origin/main`)·같은 폴더·같은 브랜치에서 작업하므로, **작업(코드·문서 수정, 빌드, 릴리스 등)을 시작하기 전에 항상** 아래를 먼저 한다.

1. `git fetch --prune` 으로 원격 변경을 확인한다.
2. `git status -sb` 에서 `behind N` 이 보이거나 `git log --oneline HEAD..@{u}` 에 커밋이 있으면, **작업 전에 받는다**.
   - 로컬 변경이 없거나 겹치지 않으면: `git pull --ff-only`
   - 로컬에 커밋하지 않은 변경이 있어 pull 이 거절되면: 변경을 지우지 말고(`reset --hard`, `checkout --` 금지) 사용자에게 상황을 알리고 어떻게 할지 묻는다. (`git stash` 로 잠시 치우고 pull 한 뒤 복구하는 방법도 제안할 수 있다.)
   - 갈라졌거나(diverged) 충돌이 나면: 임의로 해결하지 말고 사용자에게 알린다.
3. 받은 커밋이 있으면 무엇이 바뀌었는지(`git log --oneline`) 한두 줄로 알려 준 뒤 작업을 시작한다.
4. 받을 것이 없으면 "최신 상태"라고 짧게만 알리고 바로 작업한다.

작업 중인 브랜치뿐 아니라 `main` 도 `origin` 보다 뒤처졌는지 함께 본다 (`git log HEAD..origin/main --oneline`).

릴리스(버전 올리기, 빌드, 푸시) 직전에도 한 번 더 확인한다. 그 사이에 다른 사람이나 세션이 먼저 릴리스를 올렸을 수 있다 (0.3.6 이 먼저 올라가 있어 0.3.7 로 올린 적이 있음).

## 릴리스

- 버전은 `WiNILL/Core/GameVersion.h` 의 `ASTERIA_VERSION` 한 곳에서 올린다.
- `scripts/release_windows.ps1` 로 빌드하고 `bin/Asteria.zip` 을 만든다 (README 의 Version 도 자동 갱신).
- 커밋 메시지는 `release: Asteria X.Y.Z`, GitHub 릴리스 제목은 `ASTERIA vX.Y.Z`, 태그는 `vX.Y.Z`.
- 바이너리(exe, zip)는 커밋하지 않는다. ZIP 은 릴리스에만 첨부한다.
- 릴리스 노트는 한국어로, 바뀐 점과 다운로드 방법을 적는다.
