WiNILL Resources
================

이 폴더가 게임의 유일한 런타임 리소스 기준 위치다.

Audio/  — 효과음·배경음 (WAV/MP3)
Font/   — 임베디드/폴백 폰트 (TTF/OTF)
Icons/  — 증강 아이콘과 UI 텍스처 (PNG)
Image/  — 기타 게임 이미지 (PNG)
Licenses/ — 외부 에셋 라이선스 전문 (배포본에 같이 들어간다)

Icons/STAT_*.png — Tabler Icons (MIT, https://tabler.io/icons) outline SVG 를
  흰색 선 마스크로 렌더링한 것. *_KEY.png 는 같은 모양의 굵은 키라인.
  라이선스: Licenses/TablerIcons-LICENSE.txt

실행 파일 옆에 Resource/를 배치한다. Windows 빌드의 Resource.rc와
CMake/Visual Studio의 post-build 복사도 모두 이 폴더를 사용한다.
