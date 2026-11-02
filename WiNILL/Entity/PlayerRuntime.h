#pragma once

struct PlayerRuntimeState {
    float dashCooldown = 0.0f;
    float dashCooldownMax = 0.0f;   // cooldown the last dash started with (HUD)
    float dashInvulnerability = 0.0f;
    float postPickGrace = 0.0f;
    bool dashActive = false;
    float dashProgress = 0.0f;
    float dashFromX = 0.0f, dashFromY = 0.0f;
    float dashToX = 0.0f, dashToY = 0.0f;
    float knockbackVX = 0.0f, knockbackVY = 0.0f; // decaying shove from rams
    bool dashInputHeld = false;
};

extern PlayerRuntimeState g_PlayerRuntime;

struct PlayerViewportBounds {
    float centerX = 0.0f, centerY = 0.0f;
    float halfWidth = 0.0f, halfHeight = 0.0f;
    float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
};

// Ship size and base move speed (world px, px/s). Shared with the READY
// practice field so it moves exactly like a run.
inline constexpr float PLAYER_SIZE_PX = 25.0f;
inline constexpr float PLAYER_MOVE_SPEED = 400.0f;
inline constexpr float DASH_CD = 2.9f;
inline constexpr float DASH_DIST = 300.0f;
inline constexpr float DASH_INVULN = 0.20f;
inline constexpr float DASH_DUR = 0.11f;
// Damage protection and fire lock after a card pick returns to combat.
inline constexpr float POST_PICK_GRACE = 0.5f;

void ResetPlayerSkills();
void ResetPlayerInputEdges();
// dashRecoveryMult < 1 slows only the dash cooldown (Gravis time dilation).
void UpdatePlayerRuntimeTimers(float delta, float dashRecoveryMult = 1.0f);
float UpdatePlayerMovement(float& playerX, float& playerY,
                           bool moveUp, bool moveDown,
                           bool moveLeft, bool moveRight,
                           bool canMove, float moveSpeed, float delta,
                           bool dashActive, float& directionX, float& directionY);
PlayerViewportBounds GetPlayerViewportBounds(float screenWidth,
                                              float screenHeight,
                                              float zoom, float topInset,
                                              float bottomInset);
void ClampPlayerToViewport(float& playerX, float& playerY,
                           float playerWidth, float playerHeight,
                           const PlayerViewportBounds& bounds);
void ApplyPlayerDamageProtection(float hpBefore, float& hpAfter,
                                 bool invulnerable);
void BeginPlayerDash(float playerX, float playerY, float directionX, float directionY,
                     float minX, float maxX, float minY, float maxY);
void AdvancePlayerDash(float delta, float& playerX, float& playerY);
// Shove the player by roughly (offsetX, offsetY) over a short ease-out.
void ApplyPlayerKnockback(float offsetX, float offsetY);
void AdvancePlayerKnockback(float delta, float& playerX, float& playerY);
