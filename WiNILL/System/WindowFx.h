#pragma once

// ─────────────────────────────────────────────────────────────
// 윈도우 투명도 헬퍼 (크로스플랫폼)
//   Windows : DWM per-pixel 알파 합성 (DwmExtendFrameIntoClientArea)
//   macOS/Linux : GLFW_TRANSPARENT_FRAMEBUFFER 힌트로 처리 → 이 함수는 no-op
// ─────────────────────────────────────────────────────────────
struct GLFWwindow;

// Whole-window alpha on Windows: 254/255 (GLFW truncates 255 * opacity).
// Below 255 so apps behind do not treat the overlay as an opaque cover and
// stop painting (see EnableWindowTransparency).
constexpr float kSeeThroughOpacity = 254.5f / 255.0f;

void EnableWindowTransparency(GLFWwindow* window);
// Mark the borderless window fullscreen for the Shell while it is active.
void UpdateWindowTaskbarPolicy(GLFWwindow* window);
void ReleaseWindowTaskbarPolicy(GLFWwindow* window);
// True when the display under the window has HDR on (Windows only). The menu
// darkness veil is compensated for it: DWM blends HDR in linear light.
// ASTERIA_FORCE_HDR=1/0 overrides it for checks.
bool IsWindowOnHdrDisplay(GLFWwindow* window);

// transparency_debug.txt 에 로그 기록 (printf 스타일, 크로스플랫폼)
void TransparencyLog(const char* fmt, ...);
