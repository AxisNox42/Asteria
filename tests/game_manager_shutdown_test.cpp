#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GameManager.h"
#include "PlayerStats.h"
#include <cstdio>
#include <cstdlib>
#include "audio_stub.h"   // GameManager now plays sounds; the test stays silent

// Unrelated GameManager entry points are linked but unused by this GL test.
PlayerStats g_Stats;
int g_CurrentWeapon = 0;
bool keys[1024] = {};   // KeyHeld() reads this (Input.cpp is not linked)
GameState g_SettingsReturnTo = GameState::MAIN_MENU;
int g_HoveredAug = -1, g_AugExitSlot = -1;
float g_AugExitT = 0.0f;
bool TryEscNavigateBack() { return false; }
namespace passive {   // the card draw's filter (PassiveAugments.cpp)
bool CanOffer(int) { return true; }
float LuckChanceMult() { return 1.0f; }
int HeldCount(int) { return 0; }
}
namespace syn {       // upgrades and the frame pick (SynergyRuntime.cpp)
bool IsFrame(int) { return false; }
void SetFramePick(bool) {}
int Stage(int) { return 0; }
bool CanTakeBranch(int, int) { return false; }
}

static int lateDeletes = 0;
static void APIENTRY CountVertexArrays(GLsizei, const GLuint*) { ++lateDeletes; }
static void APIENTRY CountBuffers(GLsizei, const GLuint*) { ++lateDeletes; }
static void APIENTRY CountProgram(GLuint) { ++lateDeletes; }

static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main() {
    Check(glfwInit() == GLFW_TRUE, "GLFW init");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(256, 256, "GameManager shutdown",
                                         nullptr, nullptr);
    Check(window != nullptr, "window");
    glfwMakeContextCurrent(window);
    Check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, "GL loader");

    GameManager manager;
    manager.Init(256, 256);
    manager.ShutdownGlResources();
    manager.ShutdownGlResources();  // idempotent before context shutdown

    glfwDestroyWindow(window);
    glfwTerminate();
    glad_glDeleteVertexArrays = CountVertexArrays;
    glad_glDeleteBuffers = CountBuffers;
    glad_glDeleteProgram = CountProgram;
    manager.ShutdownGlResources();
    Check(lateDeletes == 0, "no GL delete after context shutdown");
    std::puts("GameManager GL shutdown passed");
}
