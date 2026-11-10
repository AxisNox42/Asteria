#pragma once
#include <glad/glad.h>
#include "Settings.h"
#include <algorithm>
#include <cmath>

// 메인 배치 셰이더 컴파일·링크 + CRT(uFx) 유니폼 위치
GLuint CompileGlShader(GLenum type, const char* src);
void   InitMainShaderPipeline(int screenW, int screenH);

// 줌/흔들림 전 기준 ortho (매 프레임 UI 패스에서 g_MainOrtho 로 복원)
extern float g_BaseOrtho[16];
extern GLint g_MainFxLoc;
extern GLint g_MainResLoc;

// Where the scene currently sits in the window when it is drawn into a
// sub-viewport (the tutorial's mini card screens): window-pixel origin of the
// viewport (GL, y up) and the scale from scene units to window pixels.
// Shaders that read gl_FragCoord (planet, nebula glow) undo it with this.
// The full-window map scales by g_PixelScale (logical units to pixels).
struct ViewportMap { float x = 0.0f, y = 0.0f, scale = 1.0f; };
inline ViewportMap g_ViewportMap;
inline ViewportMap FullViewportMap() { return { 0.0f, 0.0f, g_PixelScale }; }

// The whole window as the GL viewport, with the matching map.
inline void SetFullViewport() {
    glViewport(0, 0, g_FramebufferW, g_FramebufferH);
    g_ViewportMap = FullViewportMap();
}

// glScissor from a rectangle in scene units, y down from the top, the way the
// game lays things out. Goes through g_ViewportMap so it also clips right
// inside a sub-viewport.
inline void ScissorUnits(float x, float yTop, float w, float h) {
    const ViewportMap& m = g_ViewportMap;
    const float px0 = m.x + x * m.scale;
    const float px1 = m.x + (x + w) * m.scale;
    // Scene y runs down from the viewport's top edge; GL y up from the window's
    // bottom.
    GLint vp[4] = { 0, 0, g_FramebufferW, g_FramebufferH };
    glGetIntegerv(GL_VIEWPORT, vp);
    const float top = (float)(vp[1] + vp[3]);
    const float py1 = top - yTop * m.scale;
    const float py0 = top - (yTop + h) * m.scale;
    const GLint ix = (GLint)std::floor(px0), iy = (GLint)std::floor(py0);
    glScissor(ix, iy, (GLsizei)std::max(1.0f, std::ceil(px1) - (float)ix),
              (GLsizei)std::max(1.0f, std::ceil(py1) - (float)iy));
}

// 메인 VAO/VBO + g_BaseOrtho / g_MainOrtho 초기화
void InitMainBatchGeometry(int screenW, int screenH);

// 방사형/선형 그라데이션 — PNG 텍스처 기반 (배칭 불필요, 내부에서 BatchFlush+복원)
void DrawRadialGradient(float cx, float cy, float radius,
                        float r, float g, float b, float alpha);
void DrawRadialGradientRect(float x, float y, float w, float h,
                            float r, float g, float b, float alpha);
// Same soft radial falloff, stretched over a rotated quad (angle in radians).
void DrawRadialGradientRotated(float centerX, float centerY,
                               float halfW, float halfH, float angle,
                               float r, float g, float b, float alpha);
void DrawLinearGradient(float x, float y, float w, float h,
                        float r, float g, float b, float alpha,
                        bool mirrorX = false);
void DrawLinearGradientRibbon(float x, float y, float w, float h,
                              float cut, float r, float g, float b,
                              float alpha, bool mirrorX = false,
                              bool mirrorY = false,
                              bool flatLeft = false);
