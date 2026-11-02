// Marks every augment card as discovered in the save (Codex), using the
// game's own load/save code so the file stays signed and readable.
// Built and run by scripts/build_unlocked.ps1. ASTERIA_SAVE_DIR picks the
// save folder (default: %APPDATA%\Asteria).
#include "SaveSystem.h"
#include <cstdio>

int main() {
    LoadGame();
    int newly = 0;
    for (int i = 0; i < AUG_TOTAL; ++i) {
        if (!g_AugSeen[i]) ++newly;
        g_AugSeen[i] = true;
    }
    SaveGame();
    std::printf("save: %s\n", SaveFilePath());
    std::printf("augments discovered: %d / %d (%d newly unlocked)\n", AUG_TOTAL, AUG_TOTAL, newly);
    return 0;
}
