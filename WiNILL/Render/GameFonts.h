#pragma once

void InitializeGameFonts(int screenWidth, int screenHeight);

// Releases the GL objects of every game font (g_TextL/S/XL). Call while the
// GL context is still current, before glfwTerminate.
void ShutdownGameFonts();

// Per-glyph dark halo that keeps text readable on any wallpaper without a
// backdrop plate. Call once per frame before UI text is drawn.
void UpdateGameTextHalo(int screenHeight);

// The combat HUD (wave header, level, skill keys, FPS) is drawn straight on the
// desktop with nothing darker behind it, unlike the menus: between these two
// calls every font draws a wider, full-strength halo (bright wallpapers).
void BeginCombatHudTextHalo(int screenHeight);
void EndCombatHudTextHalo(int screenHeight);