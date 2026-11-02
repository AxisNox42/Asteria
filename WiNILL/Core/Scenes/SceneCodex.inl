// Included by SceneMenus.cpp to keep shared menu state and helpers private.

// The Astral Log lives in the lobby as an inline panel (Scene_CodexInline). The
// old stand-alone page that served GameState::CODEX was unreachable from the
// menus; the state now just opens the same panel (debug capture jumps use it).
void Scene_Codex(const SceneCtx& c) {
    if (g_LobbyPanel != LobbyPanel::Codex) {
        ResetCodexUi(0.0f);
        g_LobbyPanel = LobbyPanel::Codex;
        g_GameManager.currentState = GameState::MAIN_MENU;
    }
    Scene_CodexInline(c);
}
