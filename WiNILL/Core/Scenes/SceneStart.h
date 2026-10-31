#pragma once

#include "SceneTextContext.h"

enum class GameState;
struct SceneCtx;

struct TutorialSceneContext {
    GameState& currentState;
    bool previousLeftMouseDown;
    float appOpen;
    SceneTextContext text;
};

void Scene_Tutorial(const SceneCtx& scene, const TutorialSceneContext& state);
void Scene_Ready(const SceneCtx& scene, const SceneTextContext& text);
// Smoke capture: mark the first `steps` practice objectives done (in order).
void Ready_SmokeCompleteSteps(int steps);
