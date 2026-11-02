#pragma once

#include "SceneContext.h"
#include <string>
#include <vector>

inline constexpr float kWideSceneLinearWidth = 0.58f;
inline constexpr float kWideSceneLinearAlpha = 0.68f;

extern float g_MainMenuEntryT;
// Which lobby page is open inline over the main menu (None = the lobby itself).
enum class LobbyPanel { None, Run, Shop, Codex, Settings };
extern LobbyPanel g_LobbyPanel;
extern bool s_MainMenuResumeFromPanel;
extern float g_SettingsEntryT;
extern bool s_SettingsInlineNeedsReset;

// Lobby rail (top to bottom): TUTORIAL, PLAY, ASTRAL LOG, SHOP, SETTINGS, EXIT.
inline constexpr int kMainMenuRouteCount = 6;
inline constexpr int kMainMenuTutorialIndex = 0;
inline constexpr int kMainMenuPlayIndex = 1;
inline constexpr int kMainMenuCodexIndex = 2;
inline constexpr int kMainMenuShopIndex = 3;
inline constexpr int kMainMenuSettingsIndex = 4;
inline constexpr int kMainMenuExitIndex = 5;
const wchar_t* MainMenuRouteLabel(int language, int index);
std::vector<std::wstring> TarotWrap(const wchar_t* src, float scale,
                                    float maxWidth);
void DrawArchiveConstellation(float cx, float cy, float radius,
                              int category, int key, float now,
                              float r, float g, float b,
                              float alpha, float uiS,
                              bool drawNodeFields = true);
void ResetRunConfigUi(float entryStart = 0.0f);
void ResetSettingsUi(float entryStart = 0.0f);
void Scene_RunConfigInline(const SceneCtx& c);
void Scene_SettingsInline(const SceneCtx& c);
// Starts the run straight away (no practice screen). The lobby TUTORIAL button
// passes tutorial=true to land on the READY practice field instead.
void FinalizeLoadout(const SceneCtx& c, int wIdx, bool tutorial = false);
int FixedWeaponForSelectedJob();

float UiApproach(float value, float target, float dt, float speed);
void DrawMenuBackground(float sw, float sh, float delta,
                        float darkenAmount = 1.0f,
                        float skyStrength = 0.30f);
float LogoClamp01(float value);
void SetSceneTextureReveal(float reveal);
void LogoLine(float x1, float y1, float x2, float y2,
              float thick, float r, float g, float b, float alpha);
void DrawPersistentSceneSideVignettes(float sw, float sh, float alpha);
void DrawSceneLeftVignette(float sw, float sh, float alpha);
void DrawSceneRadialVignette(float cx, float cy, float radius, float alpha);
void DrawSettingsOrbitArc(float cx, float cy, float radiusX, float radiusY,
                          float startAngle, float endAngle,
                          float r, float g, float b, float thickness, float alpha);
// Lobby title mark: size multiplier on MainLogoHeight and its centre as a share
// of the screen width. The lobby and the page ghosts must use the same pair.
inline constexpr float kLobbyLogoHeightMul = 1.25f;
inline constexpr float kLobbyLogoCenterXFrac = 0.24f;
float MainMenuCommandStartY(float sh, float uiScale = 1.0f);
float MainMenuButtonRailStartY(float sh, float uiScale = 1.0f);
void DrawVisibleConstellLine(float x1, float y1, float x2, float y2,
                             float thick, float r, float g, float b, float alpha);
void DrawConstellationDisc(float x, float y, float radius,
                           float r, float g, float b, float alpha,
                           bool darkenBlend = true);
void DrawConstellLineBatched(float x1, float y1, float x2, float y2,
                             float thick, float r, float g, float b, float alpha);
void DrawConstellationDiscEllipse(float x, float y, float rx, float ry,
                                  float r, float g, float b, float alpha,
                                  bool darkenBlend = true);
void DrawVisibleConstellNode(float x, float y, float size,
                             float r, float g, float b, float alpha,
                             bool whiteSpark = true,
                             bool drawField = true);
void DrawMainAsteriaLogo(float sw, float sh, float alpha, float reveal,
                        float centerXOverride = -1.0f,
                        float heightMul = 1.0f);
