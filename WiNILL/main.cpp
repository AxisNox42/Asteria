// Windows ?�더�?glad보다 먼�? ??APIENTRY 매크�?중복 ?�의 방�?
#ifdef _WIN32
  #include <windows.h>
  #include <dwmapi.h>   // DwmIsCompositionEnabled (진단??
  #include <timeapi.h>  // timeBeginPeriod / timeEndPeriod (FPS �??��???
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
  #define GLFW_EXPOSE_NATIVE_WIN32
  #include <GLFW/glfw3native.h>
#endif

#include "Platform.h"
#include "CrashHandler.h"

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <cstring>
#include <cwctype>
#include <algorithm>
#include <functional>

#include "SpatialBounds.h"
#include "GameManager.h"
#include "GameVersion.h"
#include "Monster.h"
#include "GravisField.h"
#include "Bullet.h"
#include "MonsterManager.h"
#include "EnemyProgression.h"
#include "EnemySpawner.h"
#include "BalanceTuning.h"
#include "ProjectileSystem.h"
#include "GameplayTime.h"
#include "Codex.h"
#include "CollisionSystem.h"
#include "RunStats.h"         // stat builds: new run, rebuild, restart, Chaos
#include "RunResult.h"        // run result commit: abandon, game over
#include "Augment.h"
#include "PassiveAugments.h"
#include "SynergyRuntime.h"
#include "ActiveSkills.h"
#include "PlayerStats.h"
#include "TextRenderer.h"
#include "PlanetShader.h"
#include "NebulaGlowShader.h"
#include "Settings.h"
#include "UiColors.h"
#include "ExpSystem.h"
#include "DrawPrim.h"
#include "WindowFx.h"
#include "Translations.h"
#include "Weapons.h"
#include "SaveSystem.h"
#include "IconSystem.h"
#include "Camera.h"
#include "MainShader.h"
#include "EntityDraw.h"
#include "CaelumDraw.h"
#include "NormaFx.h"         // OCTANS's own seals and pane frames
#include "CaelumFx.h"        // ERIDANUS's own torn crop-mark rings
#include "WaveHud.h"
#include "SkillHud.h"
#include "AugCardModel.h"
#include "VitalsHud.h"
#include "PlayerWeaponShell.h"
#include "GameFonts.h"
#include "PlayerRuntime.h"
#include "Scenes.h"
#include "ReadyLaunch.h"
#include "SceneContext.h"
#include "SceneInternal.h"
#include "Input.h"
#include "DebugToolkit.h"
#include "SmokeCapture.h"
#include "OsPane.h"
#include "UiLayout.h"
#include "SceneUI.h"
#include "WindowChrome.h"
#include "GameplayTelemetry.h"

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")   // RegOpenKeyExA / RegQueryValueExA / RegCloseKey
#pragma comment(lib, "winmm.lib")      // timeBeginPeriod / timeEndPeriod (FPS �??��???
// dwmapi.lib ?�?WindowFx.cpp ?�서 링크
#endif

#ifdef _MSC_VER
extern "C++" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
#endif

// ?�명???�퍼??WindowFx.h/cpp �??�동
// (EnableWindowTransparency ?�?TransparencyLog 가 ?�일 기능)
#define DBG TransparencyLog

GameManager    g_GameManager;
MonsterManager g_MonsterManager;
std::vector<Bullet> g_Bullets;

// ?�?�?개발???�리?�이?�브) 모드 ???�감 검?�창 ?�스?�에그로�??�금 ?�?�?
//    출시 빌드???�리?�이?�브 진입?�이 ?�겨???�고, ?�감 검?�에 ?�크�?코드�?//    ?�력?�면 ?�금?�어 ?�이???�면???��????�장?�다. (?�반 ?�레?�어??�?�?
bool    g_DevUnlocked   = false;
float   g_DevToastTimer = 0.0f;            // ?�금 ?�인 ?�스??(�?
// ?�정�?볼륨 ?�자 직접?�력 ?�태
bool    g_VolEdit = false;
wchar_t g_VolBuf[8] = {0};
int     g_VolLen = 0;
PlayerStats    g_Stats;
GameplayTelemetry g_GameplayTelemetry;
bool g_aug1Released = true, g_aug2Released = true, g_aug3Released = true;
float g_AugExitT    = -1.0f;
int   g_AugExitSlot = -1;
TextRenderer   g_TextL;   // ??글??(증강 ?�름, ?�태 ?�?��?)
TextRenderer   g_TextS;   // ?��? 글??(?�명, ?�트)
TextRenderer   g_TextXL;  // 초�????�?��?(?�작�?로고) ?�용 ??고해?�도 ?�스??
#ifdef _WIN32
HANDLE         g_FontMemHandle   = nullptr;
#endif

// Game speed while Ctrl-arranging windows mid-fight.
constexpr float kArrangeSlow = 0.18f;

// Defined further down; the pane and boss helpers below read them.
extern float g_GameTime;
extern float g_ShakeTime;
extern float g_ShakeMag;

// NORMA OS-pane prototype, driven by ASTERIA_SMOKE "PANE=..." steps.
static bool g_PaneDebug = false;         // outline panes/desktop windows, opaque backdrop
static bool g_PaneArrangeForce = false;   // smoke stand-in for holding Ctrl while paused
static int  g_PaneTestId = 0;

// Rectangles of the panes in front of the one being decorated: its brackets,
// glow and marks are not drawn where a front pane covers them.
static std::vector<os_pane::Rect> g_PaneCover;


// hud_lines::Segment minus the parts inside g_PaneCover.
static void PaneSeg(float x0, float y0, float x1, float y1, float w, hud_lines::Rgb c, float a) {
    if (g_PaneCover.empty()) { hud_lines::Segment(x0, y0, x1, y1, w, c, a); return; }
    // Pieces of [0, 1] along the segment still in sight.
    std::vector<std::pair<float, float>> keep{ { 0.0f, 1.0f } };
    const float dx = x1 - x0, dy = y1 - y0;
    for (const os_pane::Rect& r : g_PaneCover) {
        // Where the segment runs inside r (Liang-Barsky).
        float t0 = 0.0f, t1 = 1.0f;
        const float ps[4] = { -dx, dx, -dy, dy };
        const float qs[4] = { x0 - r.x, r.x + r.w - x0, y0 - r.y, r.y + r.h - y0 };
        bool hit = true;
        for (int i = 0; i < 4 && hit; ++i) {
            if (std::fabs(ps[i]) < 1e-6f) { if (qs[i] < 0.0f) hit = false; continue; }
            const float t = qs[i] / ps[i];
            if (ps[i] < 0.0f) t0 = std::max(t0, t); else t1 = std::min(t1, t);
            if (t0 > t1) hit = false;
        }
        if (!hit) continue;
        std::vector<std::pair<float, float>> next;
        for (const auto& k : keep) {
            if (k.second <= t0 || k.first >= t1) { next.push_back(k); continue; }
            if (k.first < t0) next.push_back({ k.first, t0 });
            if (k.second > t1) next.push_back({ t1, k.second });
        }
        keep.swap(next);
        if (keep.empty()) return;
    }
    for (const auto& k : keep)
        hud_lines::Segment(x0 + dx * k.first, y0 + dy * k.first, x0 + dx * k.second,
                           y0 + dy * k.second, w, c, a);
}

static void DrawPaneOutline(const os_pane::Rect& r, hud_lines::Rgb c, float w, float a) {
    PaneSeg(r.x, r.y, r.x + r.w, r.y, w, c, a);
    PaneSeg(r.x + r.w, r.y, r.x + r.w, r.y + r.h, w, c, a);
    PaneSeg(r.x + r.w, r.y + r.h, r.x, r.y + r.h, w, c, a);
    PaneSeg(r.x, r.y + r.h, r.x, r.y, w, c, a);
}

static void DrawPaneGlow(const os_pane::Rect& r, hud_lines::Rgb c, float alpha,
                         int rings, float step, float outward);

// A lit fuse wraps the whole pane: the full frame throbs (faster as it burns)
// with glow lines inside and out, and two burning heads leave the top middle,
// run both ways round and meet at the bottom middle as it blows. At the end
// the frame locks solid white and the pane folds shut.
static void DrawPaneFuse(const os_pane::Rect& pane, float share) {
    const hud_lines::Rgb warn{ 0.86f, 0.80f, 1.0f }, white{ 0.92f, 0.99f, 1.0f };
    // The beat speeds up from ~3 to ~12 per second.
    const float beatPhase = 6.2831853f * (3.0f * share + 4.5f * share * share) * space::kFuseSeconds;
    const float beat = 0.5f + 0.5f * sinf(beatPhase);
    const float heat = 0.45f + 0.55f * share;
    DrawPaneOutline(pane, warn, 2.0f + 1.5f * beat, (0.55f + 0.45f * beat) * heat);
    DrawPaneGlow(pane, warn, (0.35f + 0.35f * beat) * heat, 4, 4.0f, -1.0f);
    DrawPaneGlow(pane, warn, (0.25f + 0.30f * beat) * heat, 3 + (int)(3.0f * share), 4.0f, 1.0f);
    // Point at fraction s (0..1) round the frame, clockwise from the top-left.
    const float perim = 2.0f * (pane.w + pane.h);
    auto at = [&](float s, float& x, float& y) {
        s -= std::floor(s);
        float d = s * perim;
        if (d < pane.w) { x = pane.x + d; y = pane.y; return; }
        d -= pane.w;
        if (d < pane.h) { x = pane.x + pane.w; y = pane.y + d; return; }
        d -= pane.h;
        if (d < pane.w) { x = pane.x + pane.w - d; y = pane.y + pane.h; return; }
        d -= pane.w;
        x = pane.x; y = pane.y + pane.h - d;
    };
    // The burnt part: from the top middle, half the frame each way at the end.
    const float start = pane.w * 0.5f / perim;
    const float reach = 0.5f * std::min(1.0f, share * 1.08f);
    constexpr int kSteps = 96;   // short enough steps that corners stay square
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < kSteps; ++i) {
            const float t0 = reach * (float)i / kSteps, t1 = reach * (float)(i + 1) / kSteps;
            float x0, y0, x1, y1;
            at(start + side * t0, x0, y0);
            at(start + side * t1, x1, y1);
            const bool head = i >= kSteps - 4;
            PaneSeg(x0, y0, x1, y1, head ? 5.0f : 3.0f, head ? white : warn, head ? 1.0f : 0.9f);
        }
        float hx, hy;   // the burning head: a short white cross-bar
        at(start + side * reach, hx, hy);
        if (reach > 0.0f && reach < 0.5f) {
            const float k = 7.0f + 4.0f * beat;
            PaneSeg(hx - k, hy, hx + k, hy, 2.0f, white, 0.9f);
            PaneSeg(hx, hy - k, hx, hy + k, 2.0f, white, 0.9f);
        }
    }
    if (share > 0.92f) DrawPaneOutline(pane, white, 4.0f, 1.0f);
}

// A gradient made of lines: outlines stepping away from (outward > 0) or into
// the frame, each fainter than the last.
static void DrawPaneGlow(const os_pane::Rect& r, hud_lines::Rgb c, float alpha,
                         int rings, float step, float outward) {
    for (int i = 1; i <= rings; ++i) {
        const float d = step * (float)i * outward;
        const os_pane::Rect ring{ r.x - d, r.y - d, r.w + d * 2.0f, r.h + d * 2.0f };
        const float fade = 1.0f - (float)(i - 1) / (float)rings;
        DrawPaneOutline(ring, c, 2.0f, alpha * fade * fade);
    }
}

// Screen-space debug view: desktop windows grey, pane frame ice, pane client
// cyan, plus the client's world extent under the current camera zoom.
static void DrawPaneDebug() {
    BindMainShader();
    for (const os_pane::Rect& r : os_pane::DesktopWindows())
        DrawPaneOutline(r, { 0.54f, 0.56f, 0.66f }, 1.5f, 0.85f);
    for (const os_pane::Pane& p : os_pane::Panes()) {
        if (!p.open || p.minimized) continue;
        DrawPaneOutline(p.frame, { 0.72f, 0.94f, 1.0f }, 2.0f, 0.95f);
        DrawPaneOutline(p.client, { 0.20f, 0.88f, 0.94f }, 1.2f, 0.90f);
        const float zoom = g_ViewZoom < 0.01f ? 0.01f : g_ViewZoom;
        wchar_t label[160];
        swprintf_s(label, L"pane %d  world %.0f,%.0f %.0fx%.0f  zoom %.3f  moves %d  refused %d",
                   p.id, ScreenToWorldX(p.client.x), ScreenToWorldY(p.client.y),
                   p.client.w / zoom, p.client.h / zoom, g_ViewZoom, p.moves, p.refusals);
        g_TextS.Draw(label, p.frame.x, p.frame.y + p.frame.h + 8.0f, 0.5f,
                     0.72f, 0.94f, 1.0f, 0.95f);
    }
    BatchFlush();
}

// NORMA's verdict. When the boss wave's clock runs out with NORMA still
// standing, a countdown starts; at zero the player is struck dead and the
// run is lost. Felling NORMA first wins as usual. Seconds left, -1 = idle.
static float g_NormaJudgment = -1.0f;
static bool  g_NormaDoom = false;   // the coming death is the verdict
static constexpr float kNormaJudgmentSeconds = 10.0f;

// A stroke along a polyline as one triangle strip: each point gets one pair
// of vertices, offset along the average of its two segments' normals and
// lengthened by 1/cos of half the turn (a mitre, capped at 3x on sharp turns).
// Neighbouring quads share those vertices, so a curve shows no gap on the
// outside of a bend and no darker overlap on the inside, at any alpha.
static void StrokeSmoothPath(const float* px, const float* py, int n, bool closed,
                             float thick, hud_lines::Rgb c, float a) {
    if (n < 2) return;
    const float half = thick * 0.5f;
    std::vector<float> ox(n), oy(n);
    const auto dir = [&](int i, int j, float& dx, float& dy) {
        dx = px[j] - px[i]; dy = py[j] - py[i];
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l < 1e-4f) return false;
        dx /= l; dy /= l;
        return true;
    };
    for (int i = 0; i < n; ++i) {
        const int p = closed ? (i + n - 1) % n : std::max(0, i - 1);
        const int q = closed ? (i + 1) % n : std::min(n - 1, i + 1);
        float d0x = 0, d0y = 0, d1x = 0, d1y = 0;
        const bool h0 = p != i && dir(p, i, d0x, d0y);
        const bool h1 = q != i && dir(i, q, d1x, d1y);
        if (!h0) { d0x = d1x; d0y = d1y; }
        if (!h1) { d1x = d0x; d1y = d0y; }
        float mx = -(d0y + d1y), my = d0x + d1x;   // sum of both normals
        float ml = std::sqrt(mx * mx + my * my);
        if (ml < 1e-4f) { mx = -d1y; my = d1x; ml = 1.0f; }   // a full turn back
        mx /= ml; my /= ml;
        const float cosHalf = std::max(1.0f / 3.0f, mx * -d1y + my * d1x);
        ox[i] = mx * half / cosHalf; oy[i] = my * half / cosHalf;
    }
    const int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; ++i) {
        const int j = (i + 1) % n;
        BatchTri(px[i] + ox[i], py[i] + oy[i], px[j] + ox[j], py[j] + oy[j], px[j] - ox[j], py[j] - oy[j],
                 c.r, c.g, c.b, a);
        BatchTri(px[i] + ox[i], py[i] + oy[i], px[j] - ox[j], py[j] - oy[j], px[i] - ox[i], py[i] - oy[i],
                 c.r, c.g, c.b, a);
    }
}

// OCTANS' health is a seal chain, the same seal its panes wear (DrawPaneSeal):
// eight links of one unbroken line, joined by nine diamonds, so it never
// reads as a dashed line. Each link is an eighth of the phase's bar.
//   - the health: the lit line from the left; what is lost stays as a thin,
//     faint line, still joined at every diamond
//   - a diamond is lit while the link before it (to its left) holds any
//     health; when a link empties its diamond flashes white and swells
//   - one link left: its diamond throbs violet (the last star)
//   - damage leaves a white stretch that catches up after kSealTrailHold
//   - phase II fills back link by link, its head white
//   - "5/8" at the right end: links still holding health
static constexpr float kSealTrailHold = 0.45f;
static void DrawSealChain(float x0, float y, float w, float scale, float ratio, float refill, int phase,
                          hud_lines::Rgb ice) {
    static float s_trail = 1.0f, s_lastHit = -10.0f, s_lastT = 0.0f, s_flashAt = -10.0f;
    static int s_phase = 1, s_links = 8, s_flashUpTo = 0;
    const float dt = std::clamp(g_GameTime - s_lastT, 0.0f, 0.1f);
    s_lastT = g_GameTime;
    if (phase != s_phase || ratio > s_trail) { s_trail = ratio; s_phase = phase; }
    if (ratio < s_trail - 0.0005f && g_GameTime - s_lastHit > kSealTrailHold + 0.5f) s_lastHit = g_GameTime;
    if (g_GameTime - s_lastHit > kSealTrailHold) s_trail += (ratio - s_trail) * std::min(1.0f, dt * 5.0f);
    // Links still holding health; a drop flashes the diamonds that went out.
    const int links = std::clamp((int)std::ceil(ratio * 8.0f - 0.0001f), 0, 8);
    if (links < s_links && refill <= 0.0f) { s_flashUpTo = s_links; s_flashAt = g_GameTime; }
    s_links = links;
    const float flash = std::clamp(1.0f - (g_GameTime - s_flashAt) / 0.35f, 0.0f, 1.0f);

    const hud_lines::Rgb white{ 0.92f, 0.99f, 1.0f }, warn{ 0.86f, 0.80f, 1.0f };
    const float r = 5.5f * scale, step = w / 8.0f;
    const auto nodeX = [&](int k) { return x0 + step * (float)k; };
    const auto line = [&](float a, float b, float width, hud_lines::Rgb c, float al) {
        // Along the bar from a to b (0..1), stopping at the diamonds' points.
        for (int k = 0; k < 8; ++k) {
            const float s0 = std::max(a, (float)k / 8.0f), s1 = std::min(b, (float)(k + 1) / 8.0f);
            if (s1 <= s0) continue;
            const float l = std::max(x0 + w * s0, nodeX(k) + r), rr = std::min(x0 + w * s1, nodeX(k + 1) - r);
            if (rr > l) hud_lines::Segment(l, y, rr, y, width, c, al);
        }
    };
    const auto diamond = [&](float cx, float rad, float width, hud_lines::Rgb c, float a) {
        hud_lines::Segment(cx, y - rad, cx + rad, y, width, c, a);
        hud_lines::Segment(cx + rad, y, cx, y + rad, width, c, a);
        hud_lines::Segment(cx, y + rad, cx - rad, y, width, c, a);
        hud_lines::Segment(cx - rad, y, cx, y - rad, width, c, a);
    };
    line(ratio, 1.0f, 1.0f * scale, ice, 0.25f);                                  // lost
    if (s_trail > ratio) line(ratio, s_trail, 3.0f * scale, white, 0.85f);        // damage lag
    line(0.0f, ratio, 3.0f * scale, ice, 0.95f);                                  // health
    if (refill > 0.0f && ratio > 0.0f) {                                          // phase II rushing back
        const float hx = x0 + w * ratio;
        hud_lines::Segment(hx - 14.0f * scale, y, hx, y, 3.0f * scale, white, refill);
    }
    const float beat = 0.5f + 0.5f * sinf(g_GameTime * 9.0f);
    for (int k = 0; k <= 8; ++k) {
        const float cx = nodeX(k);
        // Diamond k closes link k (to its left); the first one anchors the chain.
        const bool lit = k == 0 ? links > 0 : k <= links;
        if (lit && links == 1 && k == 1 && refill <= 0.0f) {
            diamond(cx, r * (1.0f + 0.25f * beat), (1.8f + 0.8f * beat) * scale, warn, 0.6f + 0.4f * beat);
        } else if (lit) {
            diamond(cx, r, 2.0f * scale, ice, 0.95f);
            diamond(cx, r * 0.4f, 1.4f * scale, ice, 0.8f);      // inset contour, the lit core
        } else {
            diamond(cx, r * 0.7f, 1.0f * scale, ice, 0.25f);
            if (flash > 0.0f && k > links && k <= s_flashUpTo)
                diamond(cx, r * (1.0f + 0.9f * (1.0f - flash)), 2.0f * scale, white, flash);
        }
    }
    BatchFlush();
    wchar_t count[8];
    swprintf_s(count, L"%d/8", links);
    const float ts = 0.42f * scale;
    g_TextS.Draw(count, x0 + w + 12.0f * scale, y - g_TextS.Height(count, ts) * 0.5f, ts,
                 ice.r, ice.g, ice.b, 0.8f);
    BindMainShader();
}

// An elite's health: a short star-chart line in the elite's own colour.
//   - a faint full line, the lit health over it, a four-point star riding the
//     lit end (it flares white when hit and throbs under a quarter)
//   - damage leaves a white stretch that catches up after kEliteTrailHold
//   - REGULUS' shield is a second pale line laid just over the bar; while it
//     is down, a faint line re-grows along it as the regrowth timer runs
//   - ANTARES counts its whole family: splitting does not refill the bar
//   - on entry the name swells in over the bar and settles; on death the
//     star blooms and the line lets go
static constexpr float kEliteTrailHold = 0.40f;
static void DrawEliteBar(float sw, float scale, float topY) {
    static int s_key = 0;
    static float s_ratio = 1.0f, s_trail = 1.0f, s_lastHit = -10.0f, s_lastT = 0.0f;
    static float s_peak = 1.0f, s_born = -10.0f, s_gone = -10.0f;
    static float s_shield = 0.0f, s_regen = 0.0f, s_shieldFlash = 0.0f;
    static MobKind s_kind = MobKind::REGULUS;
    static glm::vec3 s_color(1.0f);
    static int s_frags = 1;

    const Monster* lead = nullptr;
    float sum = 0.0f;
    int frags = 0;
    for (const Monster* m : g_MonsterManager.monsters) {
        if (!m->alive || !IsEliteMob(m->kind)) continue;
        if (!lead) lead = m;
        if (m->kind == MobKind::ANTARES && lead->kind == MobKind::ANTARES &&
            m->antaresFamily == lead->antaresFamily) { sum += m->hp; ++frags; }
    }
    const float dt = std::clamp(g_GameTime - s_lastT, 0.0f, 0.1f);
    s_lastT = g_GameTime;
    if (lead) {
        const int key = lead->kind == MobKind::ANTARES ? 1000 + lead->antaresFamily
                                                       : (int)lead->kind;
        if (key != s_key) {
            s_key = key; s_born = g_GameTime; s_gone = -10.0f;
            s_ratio = s_trail = 1.0f; s_lastHit = -10.0f;
            s_peak = 1.0f; s_shield = 0.0f;
        } else if (s_gone > s_born) {
            s_gone = -10.0f;   // an Antares between split and fragments: carry on
        }
        s_kind = lead->kind;
        s_color = lead->color;
        s_frags = frags;
        const float hpNow = lead->kind == MobKind::ANTARES ? sum : lead->hp;
        s_peak = lead->kind == MobKind::ANTARES ? std::max(s_peak, hpNow)
                                                : std::max(1.0f, lead->eliteMaxHp);
        const float ratio = std::clamp(hpNow / std::max(1.0f, s_peak), 0.0f, 1.0f);
        if (ratio < s_ratio - 0.0005f) {
            if (g_GameTime - s_lastHit > kEliteTrailHold + 0.5f) s_lastHit = g_GameTime;
            else s_lastHit = std::max(s_lastHit, g_GameTime - 0.12f);
        }
        s_ratio = ratio;
        if (ratio > s_trail) s_trail = ratio;
        if (g_GameTime - s_lastHit > kEliteTrailHold)
            s_trail += (ratio - s_trail) * std::min(1.0f, dt * 5.0f);
        if (lead->kind == MobKind::REGULUS) {
            const float now = lead->regulusShield.hp / std::max(1.0f, lead->eliteMaxHp);
            if (s_shield > 0.0f && now <= 0.0f) s_shieldFlash = g_GameTime;
            s_shield = now;
            s_regen = lead->regulusShieldRegenTimer > 0.0f
                ? 1.0f - lead->regulusShieldRegenTimer / Monster::REGULUS_SHIELD_REGEN_DELAY : 0.0f;
        } else {
            s_shield = s_regen = 0.0f;
        }
    } else if (s_key != 0 && s_gone < s_born) {
        s_gone = g_GameTime;   // the last elite just fell
    }
    if (s_key == 0) return;
    const float gone = s_gone < s_born ? 0.0f : (g_GameTime - s_gone);
    if (gone > 0.7f) { s_key = 0; return; }
    const float fade = 1.0f - std::clamp(gone / 0.7f, 0.0f, 1.0f);

    // Entry: the line draws outward from its middle.
    const float age = g_GameTime - s_born;
    const float open = std::clamp(age / 0.55f, 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - open) * (1.0f - open) * (1.0f - open);
    const float fullW = sw * 0.28f;
    const float w = fullW * ease, cx = sw * 0.5f, x0 = cx - w * 0.5f;
    const float y = topY + 104.0f * scale;
    // The elite's own colour: a deeper edge, a lighter head.
    const hud_lines::Rgb base{ s_color.r, s_color.g, s_color.b };
    const hud_lines::Rgb white{ 0.97f, 0.97f, 1.0f };
    const auto mix = [](hud_lines::Rgb a, hud_lines::Rgb b, float t) {
        return hud_lines::Rgb{ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
    };
    const hud_lines::Rgb deep = mix(base, hud_lines::Rgb{ 0, 0, 0 }, 0.28f);
    const hud_lines::Rgb hot = mix(base, white, 0.38f);
    const float ratio = s_ratio;
    const float hit = std::clamp(1.0f - (g_GameTime - s_lastHit) / 0.25f, 0.0f, 1.0f);
    const float low = ratio < 0.25f && ratio > 0.0f ? 0.5f + 0.5f * sinf(g_GameTime * 12.0f) : 0.0f;
    const float tt = g_GameTime;

    BindMainShader();
    const auto span = [&](float a, float b, float width, hud_lines::Rgb c, float al, float dy = 0.0f) {
        if (b > a + 0.0005f) hud_lines::Segment(x0 + w * a, y + dy, x0 + w * b, y + dy, width, c, al);
    };
    const auto burn = [&](float to, float width, float al) {
        constexpr int kRuns = 36;
        for (int i = 0; i < kRuns; ++i) {
            const float a = to * (float)i / kRuns, b = to * (float)(i + 1) / kRuns;
            const float t = (float)(i + 1) / kRuns;
            span(a, b + 0.002f, width, mix(deep, hot, t * t), al);
        }
    };
    // A closed polygon (or one with a gap at each corner) around (px, py).
    const auto poly = [&](float px, float py, float r, int n, float rot, float width, hud_lines::Rgb c,
                          float al, float gap) {
        for (int i = 0; i < n; ++i) {
            const float a0 = rot + 6.2831853f * (float)i / n, a1 = rot + 6.2831853f * (float)(i + 1) / n;
            const float xa = px + r * cosf(a0), ya = py + r * sinf(a0);
            const float xb = px + r * cosf(a1), yb = py + r * sinf(a1);
            hud_lines::Segment(xa + (xb - xa) * gap, ya + (yb - ya) * gap,
                               xb - (xb - xa) * gap, yb - (yb - ya) * gap, width, c, al);
        }
    };
    const auto ellipse = [&](float px, float py, float rx, float ry, float rot, float width,
                             hud_lines::Rgb c, float al) {
        constexpr int kN = 28;
        const float cs = cosf(rot), sn = sinf(rot);
        float lx = 0, ly = 0;
        for (int i = 0; i <= kN; ++i) {
            const float a = 6.2831853f * (float)i / kN;
            const float ex = rx * cosf(a), ey = ry * sinf(a);
            const float X = px + ex * cs - ey * sn, Y = py + ex * sn + ey * cs;
            if (i > 0) hud_lines::Segment(lx, ly, X, Y, width, c, al);
            lx = X; ly = Y;
        }
    };

    // The health: the same line for everyone (faint lost part, damage lag, a
    // lit run that goes from the deep edge to the light head, a thin sheen).
    const float lostA = s_kind == MobKind::QUASAR ? 0.55f : 0.38f;   // QUASAR: the beam runs on past the orb
    span(ratio, 1.0f, (s_kind == MobKind::QUASAR ? 1.8f : 1.4f) * scale, deep, lostA * fade);
    if (s_trail > ratio) span(ratio, s_trail, 5.0f * scale, white, 0.88f * fade);
    span(0.0f, ratio, 9.0f * scale, hot, 0.12f * fade);
    burn(ratio, 4.6f * scale, 0.96f * fade);
    span(0.0f, ratio, 1.2f * scale, white, 0.45f * fade, -1.2f * scale);
    const float tk = 6.0f * scale;
    hud_lines::Segment(x0, y - tk, x0, y + tk, 1.8f * scale, deep, 0.75f * fade);
    hud_lines::Segment(x0 + w, y - tk, x0 + w, y + tk, 1.8f * scale, deep, 0.55f * fade);

    const float hx = x0 + w * ratio;
    const hud_lines::Rgb hc = hit > 0.0f || gone > 0.0f ? white : hot;
    const float ha = (0.92f + 0.08f * low) * fade;
    const float bloom = gone > 0.0f ? 1.0f + gone * 5.0f : 1.0f;
    const float pulse = (1.0f + 0.28f * hit + 0.18f * low) * bloom;

    if (s_kind == MobKind::REGULUS) {
        // The shield it has gained, laid over the health: an inlaid pale line
        // from the left whose length is the shield's share of the elite's max
        // HP, so the health shows as a rim round it. A white flash when it
        // breaks; while it is down a thin line re-grows just above the bar.
        if (open >= 1.0f) {
            const float sf = std::clamp(1.0f - (g_GameTime - s_shieldFlash) / 0.4f, 0.0f, 1.0f);
            const hud_lines::Rgb sc = mix(base, white, 0.85f);
            if (s_shield > 0.0f) {
                const float e = std::min(1.0f, s_shield);
                span(0.0f, e, 7.5f * scale, sc, 0.22f * fade);
                span(0.0f, e, 2.8f * scale, sc, 0.98f * fade);
                hud_lines::Segment(x0 + w * e, y - 6.5f * scale, x0 + w * e, y + 6.5f * scale,
                                   1.8f * scale, sc, 0.95f * fade);
            } else if (s_regen > 0.0f) {
                span(0.0f, 0.9f * s_regen, 1.2f * scale, sc, 0.4f * fade, -7.0f * scale);
            }
            if (sf > 0.0f) {
                span(0.0f, 0.9f, 6.0f * scale, white, sf * fade);
                span(0.0f, 0.9f, 12.0f * scale, white, 0.25f * sf * fade);
            }
        }
        // The head: a small square heart inside curved shield arcs (two thin
        // layers, turning slowly). The arcs are the shield: bright while it
        // holds, a faint ghost once broken that thickens back as it regrows,
        // a white flash when it breaks.
        const float R = 9.0f * scale * pulse;
        const float ph = tt * 0.7f;
        poly(hx, y, R, 4, 0.7853982f, 2.2f * scale, hc, ha, 0.0f);
        poly(hx, y, R * 0.34f, 12, 0.0f, 1.8f * scale, hc, ha, 0.0f);
        {
            const bool holds = s_shield > 0.0f;
            const float brk = std::clamp(1.0f - (g_GameTime - s_shieldFlash) / 0.4f, 0.0f, 1.0f);
            // Broken: the arcs flash white and are gone (they come back with the shield).
            const float sa = holds ? std::min(1.0f, 0.95f + brk) : brk;
            const float wd = (holds ? 2.4f : 2.4f + 1.6f * (1.0f - brk)) * scale;
            const hud_lines::Rgb ac = brk > 0.0f ? white : mix(base, white, 0.55f);
            const auto arcs = [&](float r, float rotOff, float trim, float width, float al) {
                for (int k = 0; k < 4; ++k) {
                    const float a0 = ph + rotOff + 1.5707963f * (float)k + 0.22f + trim;
                    const float sweep = 1.5707963f - 0.44f - 2.0f * trim;
                    constexpr int kN = 10;
                    for (int i = 0; i < kN; ++i) {
                        const float u0 = a0 + sweep * (float)i / kN, u1 = a0 + sweep * (float)(i + 1) / kN;
                        hud_lines::Segment(hx + r * cosf(u0), y + r * sinf(u0),
                                           hx + r * cosf(u1), y + r * sinf(u1), width, ac, al * fade);
                    }
                }
            };
            if (sa > 0.0f) {
                const float burst = holds ? 1.0f : 1.0f + 0.5f * (1.0f - brk);   // the arcs fly outward as they break
                arcs(25.0f * scale * pulse * burst, 0.0f, 0.0f, wd, sa);
                arcs(20.5f * scale * pulse * burst, 0.0f, 0.08f, 1.0f * scale, sa * 0.5f);
            }
        }
    } else if (s_kind == MobKind::ANTARES) {
        // A pentagon split at every corner, as its body is: it spins as the
        // bull speeds up and leaves afterimages behind. Fragments alive past
        // the first show as small pentagons past the bar's end.
        const float spin = tt * 1.6f;
        for (int k = 2; k >= 1; --k)
            poly(hx - 22.0f * scale * (float)k, y, 12.0f * scale, 5, spin - 0.5f * k, 1.8f * scale, base,
                 (0.55f - 0.2f * (float)k) * fade, 0.16f);
        poly(hx, y, 16.0f * scale * pulse, 5, spin, 2.8f * scale, hc, ha, 0.14f);
        poly(hx, y, 6.5f * scale * pulse, 5, -spin * 1.4f, 1.6f * scale, hc, 0.75f * ha, 0.0f);
        for (int k = 1; k < std::min(s_frags, 5); ++k)
            poly(x0 + w + 30.0f * scale + 16.0f * scale * (float)(k - 1), y, 6.0f * scale, 5, -1.5708f,
                 1.8f * scale, hot, 0.9f * fade, 0.12f);
    } else if (s_kind == MobKind::QUASAR) {
        // The bar is the beam axis; the head is the orb under its two gyro
        // bands, which turn against each other.
        const float r = 9.0f * scale * pulse;
        ellipse(hx, y, r, r, 0.0f, 2.6f * scale, hc, ha);
        ellipse(hx, y, 24.0f * scale * pulse, 8.0f * scale, tt * 1.5f, 2.0f * scale, hc, 0.95f * ha);
        ellipse(hx, y, 24.0f * scale * pulse, 8.0f * scale, -tt * 1.1f + 1.05f, 2.0f * scale, hot, 0.8f * ha);
        hud_lines::Segment(hx - 4.5f * scale, y, hx + 4.5f * scale, y, 2.6f * scale, white, 0.9f * ha);
    } else if (s_kind == MobKind::GENESIS) {
        // The core, and a Swarm triangle circling it. Along the bar, five
        // forged Swarm triangles go out one by one as health falls.
        for (int q = 1; q <= 5; ++q) {
            const float u = 0.17f * (float)q;
            const bool lit = u <= ratio;
            poly(x0 + w * u, y - 14.0f * scale, 5.6f * scale, 3, -1.5708f, (lit ? 1.9f : 1.2f) * scale,
                 lit ? hot : deep, (lit ? 0.9f : 0.3f) * fade, 0.0f);
        }
        const float r = 11.0f * scale * pulse;
        poly(hx, y, r, 24, 0.0f, 2.6f * scale, hc, ha, 0.0f);
        poly(hx, y, 4.5f * scale * pulse, 24, 0.0f, 1.7f * scale, hc, 0.85f * ha, 0.0f);
        const float oa = tt * 2.2f;
        poly(hx + 23.0f * scale * cosf(oa), y + 23.0f * scale * sinf(oa), 5.6f * scale, 3, oa + 1.57f,
             1.9f * scale, hot, 0.95f * ha, 0.0f);
    } else {
        const float a = 9.0f * scale * pulse;
        hud_lines::Segment(hx - a, y, hx + a, y, 2.2f * scale, hc, ha);
        hud_lines::Segment(hx, y - a, hx, y + a, 2.2f * scale, hc, ha);
    }
    BatchFlush();

    // The name, over the bar's left end.
    const CodexMobId cid = CodexMobIdForKind(s_kind);
    if (cid != CM_COUNT) {
        const wchar_t* name = CODEX_MOB_PROFILES[cid].name[std::max(0, std::min(2, LangIndex()))];
        const float ts = 0.55f * scale;   // the boss bar's name size
        const float nh = g_TextS.Height(name, ts);
        const float rise = std::clamp((age - 0.25f) / 0.4f, 0.0f, 1.0f);
        const hud_lines::Rgb nc = mix(base, white, 0.2f);
        g_TextS.Draw(name, cx - fullW * 0.5f, y - 14.0f * scale - nh, ts,
                     nc.r, nc.g, nc.b, rise * fade);
    }
}

// The boss's health under the wave track: a dim full-length line and the
// bright remaining share of the current phase's bar over it (ERIDANUS: a
// filled river instead, OCTANS: a seal chain).
static void DrawNormaBossBar(float sw, float scale, float topY) {
    const Monster* boss = nullptr;
    for (const Monster* m : g_MonsterManager.monsters)
        if (IsBossMob(m->kind) && m->alive) { boss = m; break; }
    if (!boss) return;
    float ratio = std::clamp(boss->hp / std::max(1.0f, boss->normaMaxHp), 0.0f, 1.0f);
    const float w = sw * 0.36f, x0 = (sw - w) * 0.5f;
    const float y = topY + 92.0f * scale;
    const bool caelumBoss = boss->kind == MobKind::CAELUM;
    const hud_lines::Rgb ice = caelumBoss ? hud_lines::Rgb{ 1.0f, 0.82f, 0.50f }
                                          : hud_lines::Rgb{ 0.72f, 0.94f, 1.0f };
    // ERIDANUS (CAELUM): one bar per phase. Phase 1's share drains to empty,
    // then a fresh bar fills up through the phase shift and phase 2 drains it.
    float refill = 0.0f;
    if (caelumBoss) {
        const float split = caelum::kPhase2Ratio;
        if (boss->normaPhase == 2) {
            ratio = std::clamp(ratio / split, 0.0f, 1.0f);
            const float u = 1.0f - std::clamp(caelum::g.invuln / caelum::kPhaseInvuln, 0.0f, 1.0f);
            const float fill = u * u * (3.0f - 2.0f * u);
            ratio *= fill;
            refill = 1.0f - fill;
        } else {
            ratio = std::clamp((ratio - split) / (1.0f - split), 0.0f, 1.0f);
        }
    } else {
        // OCTANS (NORMA) the same way: its bar refills while it is untouchable.
        ratio = norma::BarShareFor(boss->hp, boss->normaMaxHp, boss->normaPhase);
        if (boss->normaPhase == 2 && boss->normaInvuln > 0.0f) {
            const float fill = norma::BarRefillFor(boss->normaInvuln);
            ratio *= fill;
            refill = 1.0f - fill;
        }
    }
    BindMainShader();
    if (caelumBoss) {
        // ERIDANUS, the river (Docs/Concepts/Eridanus_BossBar_Design_v1, A):
        // the health is a stretch of flowing water, filled, between two banks
        // that meander together, rounded at both ends like a puddle's edge.
        // Drained health shows nothing. Through the phase shift the water
        // rushes back in, its head white.
        // The whole river is one closed outline (top bank -> round head ->
        // bottom bank -> round tail), filled inside and stroked as one mitred
        // strip, so nothing in it breaks at a joint.
        const auto river = [&](float from, float share, hud_lines::Rgb col, float fa, float la, bool head) {
            const float xa = x0 + w * from, e = x0 + w * share;
            if (e - xa > 1.0f) {
                const float k = 0.045f / scale, flow = g_GameTime * 2.4f;
                const float step = 3.0f * scale, gap = 5.0f * scale;
                const float capR = std::min(7.0f * scale, 0.5f * (e - xa));   // how far each round end reaches
                const float s0 = xa + capR, s1 = std::max(s0 + 0.01f, e - capR);
                const auto wave = [&](float x, float ph) {
                    return y + 2.2f * scale * sinf(x * k + ph - flow);
                };
                // Both banks sampled at the same x, from s0 to exactly s1.
                std::vector<float> xs;
                for (float x = s0; x < s1 - 0.5f * step; x += step) xs.push_back(x);
                xs.push_back(s1);
                const size_t m = xs.size();
                std::vector<float> ty(m), by(m);
                for (size_t i = 0; i < m; ++i) {
                    ty[i] = wave(xs[i], 0.0f) - gap;
                    by[i] = wave(xs[i], 0.9f) + gap;
                }
                // A round end: half an ellipse joining the two banks' ends at x
                // (dir +1 bulges right from top to bottom, -1 left from bottom to top).
                constexpr int kCap = 12;
                std::vector<float> px, py;
                const auto cap = [&](float x, float yTop, float yBot, float dir) {
                    const float cy = 0.5f * (yTop + yBot), ry = 0.5f * (yBot - yTop);
                    for (int i = 1; i < kCap; ++i) {
                        const float u = -1.5707963f + 3.1415927f * (float)i / (float)kCap;
                        px.push_back(x + dir * capR * cosf(u)); py.push_back(cy + dir * ry * sinf(u));
                    }
                };
                for (size_t i = 0; i < m; ++i) { px.push_back(xs[i]); py.push_back(ty[i]); }
                const size_t headFrom = px.size() - 1;
                cap(s1, ty[m - 1], by[m - 1], 1.0f);
                const size_t headTo = px.size();
                for (size_t i = m; i-- > 0;) { px.push_back(xs[i]); py.push_back(by[i]); }
                const size_t tailFrom = px.size() - 1;
                cap(s0, ty[0], by[0], -1.0f);
                const size_t n = px.size();

                // The water: quads between the banks, a fan for each round end.
                for (size_t i = 0; i + 1 < m; ++i) {
                    BatchTri(xs[i], ty[i], xs[i + 1], ty[i + 1], xs[i + 1], by[i + 1], col.r, col.g, col.b, fa);
                    BatchTri(xs[i], ty[i], xs[i + 1], by[i + 1], xs[i], by[i], col.r, col.g, col.b, fa);
                }
                const auto fan = [&](size_t from, size_t to, float cx, float cy) {
                    for (size_t i = from; i < to; ++i) {
                        const size_t j = (i + 1) % n;
                        BatchTri(cx, cy, px[i], py[i], px[j], py[j], col.r, col.g, col.b, fa);
                    }
                };
                fan(headFrom, headTo, s1, 0.5f * (ty[m - 1] + by[m - 1]));
                fan(tailFrom, n, s0, 0.5f * (ty[0] + by[0]));
                // The banks and both ends: one unbroken outline.
                StrokeSmoothPath(px.data(), py.data(), (int)n, true, 1.6f * scale, col, la);
                // The head turns white while the water rushes back.
                if (head && refill > 0.0f)
                    StrokeSmoothPath(px.data() + headFrom, py.data() + headFrom, (int)(headTo - headFrom + 1), false,
                                     1.9f * scale, hud_lines::Rgb{ 1.0f, 1.0f, 1.0f }, refill);
                if (head && refill > 0.0f) {   // the rush: two short streaks behind the head
                    const hud_lines::Rgb white{ 1.0f, 0.95f, 0.82f };
                    hud_lines::Segment(e - 24.0f * scale, y - 2.0f * scale, e - 14.0f * scale, y - 2.0f * scale,
                                       1.2f * scale, white, 0.8f * refill);
                    hud_lines::Segment(e - 36.0f * scale, y + 2.0f * scale, e - 26.0f * scale, y + 2.0f * scale,
                                       1.2f * scale, white, 0.8f * refill);
                }
            }
        };
        river(0.0f, ratio, ice, 0.34f, 0.95f, true);
    } else {
        DrawSealChain(x0, y, w, scale, ratio, refill, boss->normaPhase, ice);
    }
    BatchFlush();
    const wchar_t* name = caelumBoss ? (boss->normaPhase == 2 ? L"ERIDANUS  II" : L"ERIDANUS")
                        : boss->normaPhase == 2 ? L"OCTANS  II" : L"OCTANS";
    const float ts = 0.55f * scale;
    g_TextS.Draw(name, x0, y - 10.0f * scale - g_TextS.Height(name, ts), ts,
                 ice.r, ice.g, ice.b, 0.95f);

    // The verdict countdown: a red line that drains under the health bar.
    if (g_NormaJudgment >= 0.0f) {
        const float left = std::clamp(g_NormaJudgment / kNormaJudgmentSeconds, 0.0f, 1.0f);
        const float pulse = g_NormaJudgment < 3.0f
            ? 0.65f + 0.35f * sinf(g_NormaJudgment * 14.0f) : 1.0f;
        const hud_lines::Rgb red{ 1.0f, 0.30f, 0.32f };
        const float y2 = y + (caelumBoss ? 16.0f : 20.0f) * scale;   // clear of the seal's diamonds
        BindMainShader();
        hud_lines::Segment(x0, y2, x0 + w, y2, 2.0f * scale, red, 0.22f);
        hud_lines::Segment(x0, y2, x0 + w * left, y2, 3.0f * scale, red, 0.95f * pulse);
        BatchFlush();
        static const wchar_t* const FMT[3] = {
            L"심판까지  %.1f", L"Judgment in  %.1f", L"裁きまで  %.1f" };
        wchar_t line[64];
        swprintf_s(line, FMT[std::max(0, std::min(2, LangIndex()))], g_NormaJudgment);
        g_TextS.Draw(line, x0, y2 + 8.0f * scale, ts, 1.0f, 0.36f, 0.38f, 0.95f * pulse);
    }
}

// Ice corner brackets on every open pane, drawn by the game above the pane's
// own frame. They thin and dim as the pane's durability wears down. A pane
// NORMA just claimed gets the claim laser and brackets that close in from
// 1.6x; the pane under a CloseWindow count shows the time left along its top.
static float PaneDurabilityLeft(const os_pane::Pane& p);
struct NormaFlash;
static const NormaFlash* FlashFor(int id);
static float FlashAge(const NormaFlash& f);
static bool CountdownFor(int id, float& left);
static float RamRedFor(int id);
static void DrawPaneHop(const os_pane::Pane& p);
static void DrawWarpCracks(const os_pane::Pane& p);
static bool DrawWallWarning();
static bool ThrowPayloadFor(int id, int& count, float& turn);
static void DrawThrowAppear();
static bool FuseFor(int id, float& share);
struct PaneCollapse;
static void DrawPaneCollapses(float dt);
// The seal under a pane: kSealTicks small diamonds just below its bottom
// edge, one going out (to a faint small outline) for each eighth of
// durability lost, like OCTANS' eight stars. A diamond that just went out
// flashes white and swells; the last one left throbs violet, since the fuse
// lights when it goes.
static constexpr int kSealTicks = 8;
static constexpr float kSealFlash = 0.35f;
struct PaneSealMark { int id = 0; int lit = kSealTicks; int flashUpTo = 0; float flashAt = -10.0f; };
static std::vector<PaneSealMark> g_PaneSeals;
static void DrawPaneSeal(const os_pane::Rect& box, int id, float left, hud_lines::Rgb tint) {
    const int lit = std::clamp((int)std::ceil(left * kSealTicks - 0.001f), 0, kSealTicks);
    PaneSealMark* mark = nullptr;
    for (PaneSealMark& m : g_PaneSeals) if (m.id == id) mark = &m;
    if (!mark) { g_PaneSeals.push_back({ id, lit, 0, -10.0f }); mark = &g_PaneSeals.back(); }
    if (lit < mark->lit) { mark->flashUpTo = mark->lit; mark->flashAt = g_GameTime; }
    mark->lit = lit;
    const int lost = mark->flashUpTo;   // ticks [lit, lost) just went out
    const float flash = std::clamp(1.0f - (g_GameTime - mark->flashAt) / kSealFlash, 0.0f, 1.0f);
    const float span = std::min(box.w * 0.5f, 8.0f * 30.0f);
    const float step = span / kSealTicks, gap = std::min(7.0f, step * 0.3f);
    const float x0 = box.x + (box.w - span) * 0.5f, y = box.y + box.h + 10.0f;
    const hud_lines::Rgb white{ 0.92f, 0.99f, 1.0f }, warn{ 0.86f, 0.80f, 1.0f };
    const float beat = 0.5f + 0.5f * sinf(g_GameTime * 9.0f);
    (void)gap;
    // Each tick is a small diamond outline (a star of the seal), not a dash.
    const auto diamond = [&](float cx, float r, float w, hud_lines::Rgb c, float a) {
        PaneSeg(cx, y - r, cx + r, y, w, c, a);
        PaneSeg(cx + r, y, cx, y + r, w, c, a);
        PaneSeg(cx, y + r, cx - r, y, w, c, a);
        PaneSeg(cx - r, y, cx, y - r, w, c, a);
    };
    const float r = std::min(6.0f, step * 0.28f);
    for (int i = 0; i < kSealTicks; ++i) {
        const float cx = x0 + step * (i + 0.5f);
        if (i < lit) {
            if (lit == 1) diamond(cx, r * (1.0f + 0.25f * beat), 1.8f + 0.8f * beat, warn, 0.6f + 0.4f * beat);
            else {
                diamond(cx, r, 2.0f, tint, 0.95f);
                diamond(cx, r * 0.4f, 1.4f, tint, 0.8f);   // inset contour, the lit core
            }
        } else {
            diamond(cx, r * 0.7f, 1.0f, tint, 0.25f);
            if (flash > 0.0f && i < lost) diamond(cx, r * (1.0f + 0.8f * (1.0f - flash)), 2.0f, white, flash);
        }
    }
}

// Where a pane border stopped a shot: a ripple running both ways along the
// edge and a small diamond swelling out, white fading to the pane's colour
// (violet for the boss's shots). Marks sit on an edge by their distance from
// its start, so they follow a dragged pane; a hit close to a fresh mark on the
// same edge relights that mark instead of adding one.
static constexpr float kPaneHitSeconds = 0.28f;
static constexpr float kPaneHitReach = 28.0f;
static constexpr float kPaneHitMerge = 12.0f;
static constexpr size_t kPaneHitCap = 48;
struct PaneHitMark { int id = 0; int edge = 0; float along = 0.0f; float at = 0.0f; bool enemy = true; };
static std::vector<PaneHitMark> g_PaneHits;

// edge: 0 top, 1 bottom (along runs right), 2 left, 3 right (along runs down).
static void AddPaneHit(const os_pane::Rect& r, int id, float sx, float sy, bool enemy) {
    const float d[4] = { std::fabs(sy - r.y), std::fabs(sy - (r.y + r.h)),
                         std::fabs(sx - r.x), std::fabs(sx - (r.x + r.w)) };
    int edge = 0;
    for (int i = 1; i < 4; ++i) if (d[i] < d[edge]) edge = i;
    const float along = edge < 2 ? std::clamp(sx - r.x, 0.0f, r.w) : std::clamp(sy - r.y, 0.0f, r.h);
    for (PaneHitMark& m : g_PaneHits)
        if (m.id == id && m.edge == edge && std::fabs(m.along - along) < kPaneHitMerge &&
            g_GameTime - m.at < 0.06f) {
            m.at = g_GameTime;
            m.enemy = m.enemy || enemy;
            return;
        }
    if (g_PaneHits.size() >= kPaneHitCap) g_PaneHits.erase(g_PaneHits.begin());
    g_PaneHits.push_back({ id, edge, along, g_GameTime, enemy });
}

static void DrawPaneHits(const os_pane::Pane& p, const os_pane::Rect& box, hud_lines::Rgb tint) {
    const hud_lines::Rgb white{ 1.0f, 1.0f, 1.0f }, violet{ 0.86f, 0.80f, 1.0f };
    for (const PaneHitMark& m : g_PaneHits) {
        if (m.id != p.id) continue;
        const float t = (g_GameTime - m.at) / kPaneHitSeconds;
        if (t < 0.0f || t >= 1.0f) continue;
        const float fade = (1.0f - t) * (1.0f - t);
        const hud_lines::Rgb base = m.enemy ? violet : tint;
        const hud_lines::Rgb c{ white.r + (base.r - white.r) * t, white.g + (base.g - white.g) * t,
                                white.b + (base.b - white.b) * t };
        const bool across = m.edge < 2;
        const float len = across ? box.w : box.h;
        const float a = std::min(m.along, len);
        const float hx = across ? box.x + a : (m.edge == 2 ? box.x : box.x + box.w);
        const float hy = across ? (m.edge == 0 ? box.y : box.y + box.h) : box.y + a;
        const float ux = across ? 1.0f : 0.0f, uy = across ? 0.0f : 1.0f;
        // The ripple: out both ways along the edge, never past its corners.
        const float reach = kPaneHitReach * (1.0f - (1.0f - t) * (1.0f - t));
        const float back = std::min(reach, a), fwd = std::min(reach, len - a);
        const float w = 3.0f - 2.0f * t;
        PaneSeg(hx - ux * back, hy - uy * back, hx, hy, w, c, 0.95f * fade);
        PaneSeg(hx, hy, hx + ux * fwd, hy + uy * fwd, w, c, 0.95f * fade);
        // The dent: a small diamond outline swelling out of the hit point.
        const float r = 5.0f * (1.0f + 0.8f * t);
        PaneSeg(hx, hy - r, hx + r, hy, 1.6f, c, 0.9f * fade);
        PaneSeg(hx + r, hy, hx, hy + r, 1.6f, c, 0.9f * fade);
        PaneSeg(hx, hy + r, hx - r, hy, 1.6f, c, 0.9f * fade);
        PaneSeg(hx - r, hy, hx, hy - r, 1.6f, c, 0.9f * fade);
    }
}

static void DrawCrashCharge();
static bool CrashActive();
static void DrawPaneBrackets() {
    g_PaneHits.erase(std::remove_if(g_PaneHits.begin(), g_PaneHits.end(), [](const PaneHitMark& m) {
        if (g_GameTime - m.at >= kPaneHitSeconds) return true;
        for (const os_pane::Pane& p : os_pane::Panes()) if (p.open && p.id == m.id) return false;
        return true; }), g_PaneHits.end());
    g_PaneSeals.erase(std::remove_if(g_PaneSeals.begin(), g_PaneSeals.end(), [](const PaneSealMark& m) {
        for (const os_pane::Pane& p : os_pane::Panes()) if (p.open && p.id == m.id) return false;
        return true; }), g_PaneSeals.end());
    DrawPaneCollapses(1.0f / 60.0f);
    DrawThrowAppear();
    bool any = false;
    const hud_lines::Rgb ice{ 0.72f, 0.94f, 1.0f };
    for (const os_pane::Pane& p : os_pane::Panes()) {
        if (!p.open || p.minimized || p.covered || p.decor) continue;
        if (!any) { BindMainShader(); any = true; }
        g_PaneCover.clear();
        for (const os_pane::Pane& q : os_pane::Panes())
            if (q.open && !q.minimized && !q.decor && q.zRank < p.zRank) g_PaneCover.push_back(q.shown);
        os_pane::Rect box = p.shown;
        if (const NormaFlash* flash = FlashFor(p.id)) {
            const float age = FlashAge(*flash);
            if (age < norma::kSnapSeconds) {
                const float t = 1.0f - age / norma::kSnapSeconds;
                const float grow = 1.0f + norma::kSnapGrow * t * t;
                box = { box.x + box.w * 0.5f * (1.0f - grow), box.y + box.h * 0.5f * (1.0f - grow),
                        box.w * grow, box.h * grow };
            }
        }
        int payload = 0;
        float payloadTurn = 0.0f;
        if (ThrowPayloadFor(p.id, payload, payloadTurn)) {
            // The shots the box holds, circling its middle.
            const float cx = p.shown.x + p.shown.w * 0.5f, cy = p.shown.y + p.shown.h * 0.55f;
            const float rad = std::min(p.shown.w, p.shown.h) * 0.30f;
            for (int k = 0; k < payload; ++k) {
                const float a = payloadTurn + 6.2831853f * (float)k / (float)std::max(1, payload);
                const float r2 = rad * (k % 2 == 0 ? 1.0f : 0.6f);
                const float qx = cx + cosf(a) * r2, qy = cy + sinf(a) * r2, q = 5.0f;
                const os_pane::Rect sq{ qx - q, qy - q, q * 2.0f, q * 2.0f };
                DrawPaneOutline(sq, ice, 1.6f, 0.9f);
            }
        }
        float fuseShare = 0.0f;
        if (FuseFor(p.id, fuseShare)) DrawPaneFuse(p.shown, fuseShare);
        float countdown = 0.0f;
        if (CountdownFor(p.id, countdown)) {
            const float share = std::clamp(countdown / norma::kCloseCountdown, 0.0f, 1.0f);
            PaneSeg(p.shown.x, p.shown.y - 7.0f, p.shown.x + p.shown.w * share,
                               p.shown.y - 7.0f, 3.0f, { 0.86f, 0.80f, 1.0f }, 0.95f);
            // The border throbs faster as the count runs out.
            const float beat = 0.5f + 0.5f * sinf(g_GameTime * (6.0f + 18.0f * (1.0f - share)));
            DrawPaneOutline(p.shown, { 0.86f, 0.80f, 1.0f }, 1.5f + 2.0f * beat, 0.35f + 0.5f * beat);
        }
        // Afterimages of the in-game rectangle flying after its window.
        for (const os_pane::Ghost& g : p.trail) {
            const float fade = 1.0f - g.age / os_pane::kGhostSeconds;
            DrawPaneOutline(g.frame, ice, 1.4f, 0.45f * fade * fade);
        }
        const float left = PaneDurabilityLeft(p);
        // A pane the boss is ramming with glows red, and stronger.
        const float red = RamRedFor(p.id);
        const hud_lines::Rgb tint{ ice.r + (1.0f - ice.r) * red, ice.g + (0.30f - ice.g) * red,
                                   ice.b + (0.26f - ice.b) * red };
        // A held space breathes: a soft gradient out from and into the frame.
        const float breathe = 0.75f + 0.25f * sinf(g_GameTime * 2.2f + (float)p.id);
        const float glow = (0.5f + 0.5f * left) * (1.0f + 1.6f * red);
        DrawPaneGlow(box, tint, 0.30f * breathe * glow, 5 + (int)(3.0f * red), 4.0f, 1.0f);
        DrawPaneGlow(box, tint, 0.18f * breathe * glow, 3, 5.0f, -1.0f);
        const float width = 2.0f + 2.6f * left;
        const float alpha = 0.45f + 0.55f * left;
        const float arm = std::min(90.0f, std::min(box.w, box.h) * 0.22f);
        const auto brackets = [&](float ox, float oy, hud_lines::Rgb c, float a, float w) {
            const float xs[2] = { box.x + ox, box.x + box.w + ox };
            const float ys[2] = { box.y + oy, box.y + box.h + oy };
            for (int cx = 0; cx < 2; ++cx) {
                for (int cy = 0; cy < 2; ++cy) {
                    const float x = xs[cx], y = ys[cy];
                    const float sx = cx == 0 ? 1.0f : -1.0f, sy = cy == 0 ? 1.0f : -1.0f;
                    PaneSeg(x, y, x + sx * arm, y, w, c, a);
                    PaneSeg(x, y, x, y + sy * arm, w, c, a);
                }
            }
        };
        // While the contents glitch, the brackets break in the same beat: a
        // jump sideways (and a little up or down) with red and cyan copies
        // pulled apart, sized by the burst's seed.
        float burstSeed = 0.0f;
        if (os_pane::GlitchBurstNow(p.id, burstSeed)) {
            const auto h = [&](float k) { const float v = sinf(burstSeed * 12.9898f + k) * 43758.5453f; return v - std::floor(v); };
            const float jx = (h(1.0f) - 0.5f) * 2.0f * 16.0f, jy = (h(2.0f) - 0.5f) * 2.0f * 4.0f;
            const float split = 3.0f + 5.0f * h(3.0f);
            brackets(jx - split, jy, { 1.0f, 0.25f, 0.30f }, alpha * 0.8f, width);
            brackets(jx + split, jy, { 0.25f, 0.86f, 0.88f }, alpha * 0.8f, width);
            brackets(jx, jy, tint, alpha * 0.6f, width * 0.7f);
        } else {
            brackets(0.0f, 0.0f, tint, alpha, width);
        }
        if (!FuseFor(p.id, fuseShare)) DrawPaneSeal(box, p.id, left, tint);
        DrawPaneHits(p, box, tint);
        DrawPaneHop(p);
        DrawWarpCracks(p);
    }
    if (DrawWallWarning() && !any) { BindMainShader(); any = true; }
    g_PaneCover.clear();
    if (CrashActive()) {
        if (!any) { BindMainShader(); any = true; }
        DrawCrashCharge();
    }
    // While arranging: the window a drag would grab glows, pulsing.
    os_pane::Rect target;
    if (os_pane::ArrangeTarget(target)) {
        if (!any) { BindMainShader(); any = true; }
        const float pulse = 0.6f + 0.4f * sinf((float)glfwGetTime() * 7.0f);
        DrawPaneGlow(target, { 0.92f, 0.99f, 1.0f }, 0.85f * pulse, 6, 3.5f, 1.0f);
        DrawPaneOutline(target, { 0.92f, 0.99f, 1.0f }, 2.5f, 0.95f);
    }
    if (any) BatchFlush();
}

static void DumpPaneState(const char* label, GLFWwindow* window) {
    std::filesystem::create_directories("Screenshots");
    FILE* f = nullptr;
#ifdef _MSC_VER
    if (fopen_s(&f, "Screenshots/pane_state.txt", "a") != 0) f = nullptr;
#else
    f = std::fopen("Screenshots/pane_state.txt", "a");
#endif
    if (!f) return;
    std::fprintf(f, "%s fg=%d focused=%d arrange=%d passthrough=%d state=%d desktop=%zu",
                 label, os_pane::OverlayIsForeground() ? 1 : 0,
                 glfwGetWindowAttrib(window, GLFW_FOCUSED),
                 os_pane::ArrangeMode() ? 1 : 0,
                 glfwGetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH),
                 (int)g_GameManager.currentState, os_pane::DesktopWindows().size());
    for (const os_pane::Pane& p : os_pane::Panes())
        std::fprintf(f, " [id=%d open=%d min=%d locked=%d frame=%.0f,%.0f,%.0fx%.0f "
                        "client=%.0f,%.0f,%.0fx%.0f moves=%d refused=%d]",
                     p.id, p.open ? 1 : 0, p.minimized ? 1 : 0, p.locked ? 1 : 0,
                     p.frame.x, p.frame.y, p.frame.w, p.frame.h,
                     p.client.x, p.client.y, p.client.w, p.client.h,
                     p.moves, p.refusals);
    std::fputc('\n', f);
    std::fclose(f);
}

//   1초마???�레?�어 ?�치??1개씩 배치, �?5�?지????맵에 ~5�??�시
// LIGHT_STEP ?�격 감�????�전 HP 기록

// 초당 EXP ?�적??(?�버?????��??�는 죽음, ?�몹 가??

static void GrantExperience(XpAmount amount) {
    if (amount <= 0) return;
    g_GameManager.xp = XpAdd(g_GameManager.xp, amount);
    g_GameplayTelemetry.RecordExperience(amount);
}

// 게임 ?�작 ??경과 ?�간 (?�폭�?보스 ?�장 ?�?�밍)
float g_GameTime         = 0.0f;

// 검�?근접 ?�윙 ?�상 (조�? 방향 ??
float g_ShotKickTimer = 0.0f;
// Smoke step WALK=L|R|U|D|0: hold that move direction (0 releases). Smoke runs only.
static char g_SmokeWalk = 0;
// F1 debug console > Stats > 즉사: the next frame kills the player outright.
static bool g_DebugKillPlayer = false;   // weapon shell recoil after a shot (PlayerWeaponShell)

// NORMA window spaces (SpaceRules.h): open panes become world regions each
// frame, front-most first, and the player's space is resolved from its centre.
static void RebuildPaneSpaces(const SpatialBounds& player) {
    std::vector<const os_pane::Pane*> order;
    for (const os_pane::Pane& p : os_pane::Panes())
        if (p.open && !p.minimized && !p.covered && !p.decor) order.push_back(&p);
    std::sort(order.begin(), order.end(),
              [](const os_pane::Pane* a, const os_pane::Pane* b) { return a->zRank < b->zRank; });
    space::g_Regions.clear();
    // Last frame's in-game rectangle per pane, for the border sweep.
    static std::vector<std::pair<int, os_pane::Rect>> s_Last;
    std::vector<std::pair<int, os_pane::Rect>> last;
    last.swap(s_Last);
    static std::vector<std::pair<int, float>> s_Born;   // pane id -> first seen
    space::g_Clock = (float)glfwGetTime();
    s_Born.erase(std::remove_if(s_Born.begin(), s_Born.end(), [&](const std::pair<int, float>& b) {
        for (const os_pane::Pane& q : os_pane::Panes()) if (q.open && q.id == b.first) return false;
        return true; }), s_Born.end());
    for (const os_pane::Pane* p : order) {
        float born = space::g_Clock;
        bool known = false;
        for (const auto& b : s_Born) if (b.first == p->id) { born = b.second; known = true; }
        if (!known) s_Born.push_back({ p->id, born });
        space::Region r{ p->id,
            ScreenToWorldX(p->shown.x), ScreenToWorldY(p->shown.y),
            ScreenToWorldX(p->shown.x + p->shown.w), ScreenToWorldY(p->shown.y + p->shown.h), born };
        r.px0 = r.x0; r.py0 = r.y0; r.px1 = r.x1; r.py1 = r.y1;
        for (const auto& l : last) {
            if (l.first != p->id) continue;
            const os_pane::Rect& q = l.second;
            const float step = std::max({ std::fabs(q.x - p->shown.x), std::fabs(q.y - p->shown.y),
                                          std::fabs(q.x + q.w - p->shown.x - p->shown.w),
                                          std::fabs(q.y + q.h - p->shown.y - p->shown.h) });
            r.moved = step > 0.01f && step <= space::kSweepMaxStep;
            r.px0 = ScreenToWorldX(q.x); r.py0 = ScreenToWorldY(q.y);
            r.px1 = ScreenToWorldX(q.x + q.w); r.py1 = ScreenToWorldY(q.y + q.h);
        }
        space::g_Regions.push_back(r);
        s_Last.push_back({ p->id, p->shown });
    }
    space::g_PlayerSpace = space::SpaceAt(player.x + player.width * 0.5f,
                                          player.y + player.height * 0.5f);
}

// Pane durability: every shot a border stops wears that pane down; a worn
// out pane self-destructs. What the blast does is still undecided, so for
// now it only closes the pane with a shockwave.
struct PaneWear { int id = 0; float hits = 0.0f; };
static std::vector<PaneWear> g_PaneWear;

static PaneWear& WearOf(int id) {
    for (PaneWear& w : g_PaneWear) if (w.id == id) return w;
    g_PaneWear.push_back({ id, 0.0f });
    return g_PaneWear.back();
}

static float PaneMaxDurability(const os_pane::Pane& p) {
    const float zoom = g_ViewZoom < 0.01f ? 0.01f : g_ViewZoom;
    return space::MaxDurability((p.shown.w / zoom) * (p.shown.h / zoom));
}

// 0..1 durability left, 1 for panes nothing has hit yet.
static float PaneDurabilityLeft(const os_pane::Pane& p) {
    for (const PaneWear& w : g_PaneWear)
        if (w.id == p.id) return std::max(0.0f, 1.0f - w.hits / PaneMaxDurability(p));
    return 1.0f;
}

// NORMA's windows (NormaBoss.h). Panes the boss claimed or opened are
// released when its pattern ends or it falls; every claim also leaves a
// flash record for the laser and the bracket snap.
struct NormaFlash { int id = 0; float bornAt = 0.0f; float fromX = 0.0f, fromY = 0.0f; };
static std::vector<int> g_NormaPanes;
static std::vector<NormaFlash> g_NormaFlashes;
struct NormaCountdown { int id = 0; float left = 0.0f; };
static NormaCountdown g_NormaCountdown;

static Monster* FindNorma() {
    for (Monster* m : g_MonsterManager.monsters)
        if (m->kind == MobKind::NORMA && m->alive) return m;
    return nullptr;
}

static const os_pane::Pane* PaneById(int id) {
    for (const os_pane::Pane& p : os_pane::Panes())
        if (p.id == id && p.open) return &p;
    return nullptr;
}

static void FlashNormaPane(int id, const Monster& boss) {
    g_NormaFlashes.push_back({ id, g_GameTime, boss.worldX, boss.worldY });
}

struct PaneCollapse;
static void CollapsePane(const os_pane::Pane& pane, bool quiet = false);

// Lets go of the real windows the boss claimed. Panes it opened itself stay
// until the player closes them or they wear out.
static void ReleaseNormaClaims() {
    std::vector<int> kept;
    for (int id : g_NormaPanes) {
        const os_pane::Pane* pane = PaneById(id);
        if (!pane) continue;
        if (pane->adopted) os_pane::Close(id);
        else kept.push_back(id);
    }
    g_NormaPanes.swap(kept);
}

// The boss fell: every pane it opened folds shut with it.
static void EndPaneWall();
static void Shake(float seconds, float magnitude);
static void ReleaseNormaPanes() {
    for (int id : g_NormaPanes)
        if (const os_pane::Pane* pane = PaneById(id)) {
            if (!pane->adopted) CollapsePane(*pane);
            os_pane::Close(id);
        }
    g_NormaPanes.clear();
}

// Wandering of the panes the boss opened (screen pixels).
struct PaneDrift {
    int id = 0;
    int stage = 0;             // 0 holding, 1 marking the next spot, 2 moving
    float timer = 0.0f;
    float holdFor = 6.0f;
    // Sub-pixel position: windows sit on whole pixels.
    float x = 0.0f, y = 0.0f;
    bool placed = false;
    float hopFromX = 0.0f, hopFromY = 0.0f, hopToX = 0.0f, hopToY = 0.0f;
    // Refuge pull: glides from (fromX, fromY) to (toX, toY) while pullLeft > 0.
    float pullLeft = 0.0f;
    float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;
};
static std::vector<PaneDrift> g_PaneDrifts;

static float RandRange(float lo, float hi) {
    return lo + (hi - lo) * (float)(rand() % 1000) / 999.0f;
}

static void StartPaneDrift(int id) {
    PaneDrift d;
    d.id = id;
    d.holdFor = RandRange(norma::kPaneHoldMin, norma::kPaneHoldMax);
    g_PaneDrifts.push_back(d);
}

static bool PaneRamBusy(int id);
static bool PaneWarpBusy(int id);
static void LightPaneFuse(int id);
static void PickPaneSpot(float w, float h, float psx, float psy, float& x, float& y, int selfId = 0);
static float PaneOverlapScore(float x, float y, float w, float h, int selfId);

// The boss's panes hold still, then every few seconds hop: the next spot is
// marked for a moment, then the pane glides there (smoothstep). A refuge
// pull or a ram takes the pane over meanwhile. psx, psy: the player on screen.
static void DriftNormaPanes(float dt, float psx, float psy) {
    for (PaneDrift& d : g_PaneDrifts) {
        const os_pane::Pane* pane = PaneById(d.id);
        if (!pane || pane->flying || pane->minimized || PaneRamBusy(d.id) || PaneWarpBusy(d.id)) continue;
        // Resync when the window was put elsewhere (a snap, a ram); that
        // also cancels a pull and restarts the hold.
        if (!d.placed || std::fabs(d.x - pane->frame.x) > 2.0f ||
            std::fabs(d.y - pane->frame.y) > 2.0f) {
            d.x = pane->frame.x;
            d.y = pane->frame.y;
            d.placed = true;
            d.pullLeft = 0.0f;
            d.stage = 0;
            d.timer = 0.0f;
        }
        if (d.pullLeft > 0.0f) {
            d.pullLeft = std::max(0.0f, d.pullLeft - dt);
            const float t = 1.0f - d.pullLeft / norma::kRefugePull;
            const float s = t * t * (3.0f - 2.0f * t);
            d.x = d.fromX + (d.toX - d.fromX) * s;
            d.y = d.fromY + (d.toY - d.fromY) * s;
            os_pane::Place(d.id, std::round(d.x), std::round(d.y));
            if (d.pullLeft <= 0.0f) { d.stage = 0; d.timer = 0.0f; }
            continue;
        }
        d.timer += dt;
        if (d.stage == 0) {
            if (d.timer < d.holdFor) continue;
            float tx = d.x, ty = d.y;
            PickPaneSpot(pane->frame.w, pane->frame.h, psx, psy, tx, ty, d.id);
            d.hopFromX = d.x;
            d.hopFromY = d.y;
            d.hopToX = tx;
            d.hopToY = ty;
            d.stage = 1;
            d.timer = 0.0f;
        } else if (d.stage == 1) {
            if (d.timer < norma::kPaneHopWarn) continue;
            d.stage = 2;
            d.timer = 0.0f;
        } else {
            const float t = std::min(1.0f, d.timer / norma::kPaneHopMove);
            const float s = t * t * (3.0f - 2.0f * t);
            d.x = d.hopFromX + (d.hopToX - d.hopFromX) * s;
            d.y = d.hopFromY + (d.hopToY - d.hopFromY) * s;
            os_pane::Place(d.id, std::round(d.x), std::round(d.y));
            if (t >= 1.0f) {
                d.stage = 0;
                d.timer = 0.0f;
                d.holdFor = RandRange(norma::kPaneHoldMin, norma::kPaneHoldMax);
            }
        }
    }
    g_PaneDrifts.erase(std::remove_if(g_PaneDrifts.begin(), g_PaneDrifts.end(),
        [](const PaneDrift& d) { return PaneById(d.id) == nullptr; }), g_PaneDrifts.end());
}

// The next spot of a pane about to hop: its brackets, fading in.
static void DrawPaneHop(const os_pane::Pane& p) {
    for (const PaneDrift& d : g_PaneDrifts) {
        if (d.id != p.id || d.stage != 1) continue;
        const float k = std::clamp(d.timer / norma::kPaneHopWarn, 0.0f, 1.0f);
        const hud_lines::Rgb ice{ 0.72f, 0.94f, 1.0f };
        const float arm = std::min(40.0f, std::min(p.shown.w, p.shown.h) * 0.18f);
        const float xs[2] = { d.hopToX, d.hopToX + p.shown.w };
        const float ys[2] = { d.hopToY, d.hopToY + p.shown.h };
        for (int cx = 0; cx < 2; ++cx)
            for (int cy = 0; cy < 2; ++cy) {
                const float x = xs[cx], y = ys[cy];
                const float sx = cx == 0 ? 1.0f : -1.0f, sy = cy == 0 ? 1.0f : -1.0f;
                PaneSeg(x, y, x + sx * arm, y, 1.6f, ice, 0.25f + 0.55f * k);
                PaneSeg(x, y, x, y + sy * arm, 1.6f, ice, 0.25f + 0.55f * k);
            }
    }
}

// The boss's own open panes (not claimed real windows).
static std::vector<const os_pane::Pane*> NormaOwnPanes() {
    std::vector<const os_pane::Pane*> own;
    for (int id : g_NormaPanes)
        if (const os_pane::Pane* pane = PaneById(id))
            if (!pane->adopted) own.push_back(pane);
    return own;
}

// The player's centre in world space, kept current by the main loop for the
// pane patterns that aim at it.
static float g_NormaPlayerX = 0.0f, g_NormaPlayerY = 0.0f;
// Smoothed player velocity (world px/s) for the boss's predictive aim.
static float g_NormaPlayerVX = 0.0f, g_NormaPlayerVY = 0.0f;
static float playerCenterXForNorma() { return g_NormaPlayerX; }
static float playerCenterYForNorma() { return g_NormaPlayerY; }

// 8 Throw: the box the boss hurls (screen pixels).
struct ThrowBox {
    bool active = false;
    int id = 0;
    int stage = 0;             // -1 growing out of the boss, 0 filling, 1 flying, 2 fuse
    float timer = 0.0f;
    float x = 0.0f, y = 0.0f;  // sub-pixel top-left
    float tx = 0.0f, ty = 0.0f;
    os_pane::Rect spot;        // where the box opens
};
static ThrowBox g_ThrowBox;

// 10 Ram: the pane the boss hurls (screen pixels).
struct PaneRam {
    int id = 0;
    int stage = 0;               // 0 aim, 1 flying, 2 settling
    float timer = 0.0f;
    float fromX = 0.0f, fromY = 0.0f, fromW = 0.0f, fromH = 0.0f;
    float x = 0.0f, y = 0.0f;    // sub-pixel top-left
    float w = 0.0f, h = 0.0f;    // current size (squeezing during the aim)
    float dirX = 1.0f, dirY = 0.0f;
    bool playerInside = false;   // inside when last checked (no hit then)
    bool hitPlayer = false;
    std::vector<const void*> hitMobs;
};
// Several panes can be rammed at once.
static std::vector<PaneRam> g_PaneRams;

static PaneRam* RamOf(int id) {
    if (id == 0) return nullptr;
    for (PaneRam& r : g_PaneRams) if (r.id == id) return &r;
    return nullptr;
}
static bool PaneRamBusy(int id) { return RamOf(id) != nullptr; }

// 13 Wall: panes popped open in a cascade, standing still (screen px).
struct WallPane { int id = 0; float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };
struct PaneWall {
    bool active = false;
    int stage = 0;               // 0 popping open, 1 standing, 2 fuses lit
    float timer = 0.0f;
    int opened = 0;
    bool hitPlayer = false;
    std::vector<WallPane> panes;
};
static PaneWall g_Wall;

// 14 Warp: the pane the boss hides in and the one it will smash out of.
struct PaneWarp {
    int srcId = 0, dstId = 0;
    float dirX = 1.0f, dirY = 0.0f;           // the way the boss went in (screen)
    float srcX = 0.0f, srcY = 0.0f;           // where each pane stands (frame top-left)
    float dstX = 0.0f, dstY = 0.0f;
    float recoilT = -1.0f;                    // since the boss went in; -1 not yet
    float kick = 0.0f, kickX = 0.0f, kickY = 0.0f;   // a shot's push-back on the turret pane
    float fireTimer = 0.0f;
};
static PaneWarp g_Warp;
static bool PaneWarpBusy(int id) { return id != 0 && (g_Warp.srcId == id || g_Warp.dstId == id); }
// Phase change: panes spraying open one after another.
struct PhaseSpray { int left = 0; float timer = 0.0f; };
static PhaseSpray g_Spray;

// The blue screen (norma::Pattern::Crash): the storm of decor error panes,
// the one safe-mode pane, and the timeline from CrashStart (see NormaBoss.h).
struct CrashState {
    bool active = false;
    float t = 0.0f;              // seconds since CrashStart
    int toOpen = 0;              // storm panes still to open
    int total = 0;
    std::vector<int> storm;      // decor pane ids
    int safeId = 0;
    bool blue = false;           // the blue screen has fallen
    bool shattered = false;      // and broken
    float ix = 0.0f, iy = 0.0f;  // where the glass was hit (random, screen px)
    std::vector<float> px, py;   // the crack web's points (BuildCrashCracks)
    std::vector<int> rayLen;     // the ring each ray's crack stops at
    std::vector<char> ringVis;   // which ring cracks show
    std::vector<float> branches; // side cracks, 4 floats each
};
static CrashState g_Crash;
static bool CrashActive() { return g_Crash.active; }
static bool CrashScreenUp() { return g_Crash.active && g_Crash.blue && !g_Crash.shattered; }
static bool CrashSafePane(int id) { return g_Crash.active && id != 0 && id == g_Crash.safeId; }

// Opens the safe-mode pane within the player's reach (the Refuge reach), its
// near edge a little short of it, at a random bearing that stays on screen.
static int OpenCrashSafe(const Monster& boss, float psx, float psy) {
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float reach = std::clamp(norma::kRefugeReach * g_Stats.moveSpeedMult,
                                   norma::kRefugeReachMin, norma::kRefugeReachMax) * g_ViewZoom;
    const float w = std::min(sw * 0.45f, norma::kCrashSafeW), h = std::min(sh * 0.45f, norma::kCrashSafeH);
    float x = psx - w * 0.5f, y = psy - h * 0.5f;
    for (int tries = 0; tries < 12; ++tries) {
        const float a = RandRange(0.0f, 6.2831853f);
        const float along = reach * RandRange(0.45f, 0.8f) + std::fabs(cosf(a)) * w * 0.5f +
                            std::fabs(sinf(a)) * h * 0.5f;
        const float cx = psx + cosf(a) * along, cy = psy + sinf(a) * along;
        x = cx - w * 0.5f;
        y = cy - h * 0.5f;
        if (x >= 0.0f && y >= 0.0f && x + w <= sw && y + h <= sh) break;
    }
    x = std::clamp(x, 0.0f, std::max(0.0f, sw - w));
    y = std::clamp(y, 0.0f, std::max(0.0f, sh - h));
    const int id = os_pane::Open(x, y, w, h);
    if (id <= 0) return 0;
    os_pane::SetRole(id, os_pane::PaneRole::Safe, norma::kCrashCount, norma::kCrashCount);
    os_pane::RaiseToFront(id);
    Audio::Play(Audio::Cue::BossOctansPaneOpen);
    FlashNormaPane(id, boss);
    return id;
}


// The wall's footprints, red brackets that sharpen until the panes pop open.
// True when anything was queued (the caller binds the shader first).
static bool DrawWallWarning() {
    if (!g_Wall.active || g_Wall.stage != 0 || g_Wall.timer >= norma::kWallWarn + 0.05f) return false;
    const float k = std::clamp(g_Wall.timer / norma::kWallWarn, 0.0f, 1.0f);
    const hud_lines::Rgb red{ 1.0f, 0.30f, 0.26f };
    for (const WallPane& p : g_Wall.panes) {
        if (p.id > 0) continue;
        const float arm = std::min(70.0f, std::min(p.w, p.h) * 0.2f);
        const float xs[2] = { p.x, p.x + p.w }, ys[2] = { p.y, p.y + p.h };
        for (int cx = 0; cx < 2; ++cx)
            for (int cy = 0; cy < 2; ++cy) {
                const float sx = cx == 0 ? 1.0f : -1.0f, sy = cy == 0 ? 1.0f : -1.0f;
                PaneSeg(xs[cx], ys[cy], xs[cx] + sx * arm, ys[cy], 2.0f + 1.5f * k, red, 0.25f + 0.65f * k);
                PaneSeg(xs[cx], ys[cy], xs[cx], ys[cy] + sy * arm, 2.0f + 1.5f * k, red, 0.25f + 0.65f * k);
            }
        PaneSeg(p.x, p.y, p.x + p.w, p.y, 1.0f, red, 0.12f + 0.2f * k);
        PaneSeg(p.x, p.y + p.h, p.x + p.w, p.y + p.h, 1.0f, red, 0.12f + 0.2f * k);
        PaneSeg(p.x, p.y, p.x, p.y + p.h, 1.0f, red, 0.12f + 0.2f * k);
        PaneSeg(p.x + p.w, p.y, p.x + p.w, p.y + p.h, 1.0f, red, 0.12f + 0.2f * k);
    }
    return true;
}

// 0..1: how red the pane glows (control by the boss).
static float RamRedFor(int id) {
    if (PaneWarpBusy(id)) {
        const Monster* b = FindNorma();
        if (!b || b->normaPattern != norma::Pattern::Warp) return 0.0f;
        if (id == g_Warp.srcId) return b->normaStage == norma::Stage::Act ? 0.55f : 0.3f;
        // The pane to be smashed flickers harder as the time runs out.
        const float k = b->normaStage == norma::Stage::Act
            ? std::clamp(b->normaTimer / norma::kWarpHide, 0.0f, 1.0f) : 0.0f;
        const float flicker = 0.5f + 0.5f * sinf(g_GameTime * (10.0f + 22.0f * k));
        return std::clamp(0.25f + 0.55f * k + 0.2f * flicker * k, 0.0f, 1.0f);
    }
    if (g_Wall.active && g_Wall.stage < 2)   // wall panes are red: no one can stand inside
        for (const WallPane& wp : g_Wall.panes)
            if (wp.id == id) return 1.0f;
    const PaneRam* ram = RamOf(id);
    if (!ram) return 0.0f;
    if (ram->stage == 0) {
        const Monster* boss = FindNorma();
        const float aim = norma::RamAimFor(boss ? boss->normaPhase : 1);
        return 0.4f + 0.6f * std::clamp(ram->timer / aim, 0.0f, 1.0f);
    }
    if (ram->stage == 1) return 1.0f;
    return 1.0f - std::clamp(ram->timer / norma::kRamSettle, 0.0f, 1.0f);
}

static void SpawnNormaShot(float x, float y, float angle, float speed) {
    Bullet shot(x, y, x + cosf(angle) * 100.0f, y + sinf(angle) * 100.0f);
    shot.isEnemy = true;
    const Monster* boss = FindNorma();
    shot.speed = speed * (boss && boss->normaPhase == 2 ? norma::kPhase2ShotSpeed : 1.0f);
    shot.enemyDmg = norma::kShotDamage;
    shot.sizeScale = 1.05f;
    shot.color = glm::vec3(0.72f, 0.94f, 1.0f);
    shot.normaShot = true;
    g_Bullets.push_back(shot);
}

// A random spot anywhere on screen for a new boss pane, preferring one that
// overlaps the open panes least, so panes do not pile up (and blow up in a
// chain). A spot that would open right over the player scores worse too.
// Area a w x h pane at (x, y) would share with the other open panes, counting
// where the ones on the move are headed rather than where they stand.
static float PaneOverlapScore(float x, float y, float w, float h, int selfId) {
    float score = 0.0f;
    for (const os_pane::Pane& p : os_pane::Panes()) {
        if (!p.open || p.minimized || p.decor || p.id == selfId) continue;
        float px = p.frame.x, py = p.frame.y;
        for (const PaneDrift& d : g_PaneDrifts) {
            if (d.id != p.id) continue;
            if (d.pullLeft > 0.0f) { px = d.toX; py = d.toY; }
            else if (d.stage == 1 || d.stage == 2) { px = d.hopToX; py = d.hopToY; }
        }
        const float ox = std::max(0.0f, std::min(x + w, px + p.frame.w) - std::max(x, px));
        const float oy = std::max(0.0f, std::min(y + h, py + p.frame.h) - std::max(y, py));
        score += ox * oy;
    }
    return score;
}

static void PickPaneSpot(float w, float h, float psx, float psy, float& x, float& y, int selfId) {
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    float best = -1.0f;
    for (int tries = 0; tries < 120; ++tries) {
        const float cx = RandRange(0.0f, std::max(0.0f, sw - w));
        const float cy = RandRange(0.0f, std::max(0.0f, sh - h));
        float score = PaneOverlapScore(cx, cy, w, h, selfId);
        constexpr float kPlayerMargin = 80.0f;
        if (psx > cx - kPlayerMargin && psx < cx + w + kPlayerMargin &&
            psy > cy - kPlayerMargin && psy < cy + h + kPlayerMargin)
            score += w * h;
        if (best < 0.0f || score < best) {
            best = score;
            x = cx;
            y = cy;
            if (score <= 0.0f) return;
        }
    }
}

// Serves the boss's Asks.
// A new boss pane of random size at a random spot (see PickPaneSpot).
static int OpenNormaPane(Monster& boss, float psx, float psy) {
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float w = std::min(sw * norma::kPaneMaxShareW, RandRange(norma::kRefugeMinW, norma::kRefugeMaxW));
    const float h = std::min(sh * norma::kPaneMaxShareH, RandRange(norma::kRefugeMinH, norma::kRefugeMaxH));
    float x = 0.0f, y = 0.0f;
    PickPaneSpot(w, h, psx, psy, x, y);
    const int id = os_pane::Open(x, y, w, h);
    if (id <= 0) return 0;
    Audio::Play(Audio::Cue::BossOctansPaneOpen);   // 증원 창
    g_NormaPanes.push_back(id);
    StartPaneDrift(id);
    (void)boss;   // opens quietly: no claim laser for a timed pane
    return id;
}

static void ServeNormaAsks(const SpatialBounds& player) {
    Monster* boss = FindNorma();
    std::vector<norma::Ask> asks;
    asks.swap(norma::g_Asks);
    if (!boss) {
        ReleaseNormaPanes();
        g_NormaCountdown = {};
        if (g_ThrowBox.id != 0) os_pane::Close(g_ThrowBox.id);
        g_ThrowBox = {};
        g_PaneRams.clear();
        return;
    }
    // Forget panes the player closed or that wore out.
    g_NormaPanes.erase(std::remove_if(g_NormaPanes.begin(), g_NormaPanes.end(),
        [](int id) { return PaneById(id) == nullptr; }), g_NormaPanes.end());
    norma::g_PaneCount = (int)NormaOwnPanes().size();
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float psx = W2SX(player.x + player.width * 0.5f);
    const float psy = W2SY(player.y + player.height * 0.5f);
    for (norma::Ask ask : asks) {
        switch (ask) {
        case norma::Ask::DetonatePanes:
            // Every pane of the boss's blows on purpose, except one a ram or
            // a countdown is using.
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                if (PaneRamBusy(pane->id) || pane->id == g_NormaCountdown.id) continue;
                FlashNormaPane(pane->id, *boss);
                LightPaneFuse(pane->id);
            }
            break;
        case norma::Ask::ReopenPanes: {
            // Fresh panes at fresh spots, up to the phase's count.
            const int want = norma::MaxBossPanesFor(boss->normaPhase);
            while ((int)NormaOwnPanes().size() < want) {
                const int before = (int)NormaOwnPanes().size();
                const int id = OpenNormaPane(*boss, psx, psy);
                if (id <= 0) break;
                FlashNormaPane(id, *boss);
                if ((int)NormaOwnPanes().size() == before) break;
            }
            break;
        }
        case norma::Ask::OpenRefuges: {
            Audio::Play(Audio::Cue::BossOctansRefuge);   // 1.5초 예고 → 범람
            // The shelters are the panes already open (opened on a timer,
            // not by this pattern): they light up before the flood. One must
            // be reachable in the telegraph, so when none is within reach
            // the nearest glides over (or, with none open, one opens there).
            const float reach = std::clamp(norma::kRefugeReach * g_Stats.moveSpeedMult,
                                           norma::kRefugeReachMin, norma::kRefugeReachMax) * g_ViewZoom;
            const os_pane::Pane* nearest = nullptr;
            float nearestGap = 0.0f;
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                FlashNormaPane(pane->id, *boss);
                if (pane->minimized || pane->flying) continue;
                const os_pane::Rect& r = pane->frame;
                const float gx = std::max({ r.x - psx, 0.0f, psx - (r.x + r.w) });
                const float gy = std::max({ r.y - psy, 0.0f, psy - (r.y + r.h) });
                const float gap = std::sqrt(gx * gx + gy * gy);
                if (!nearest || gap < nearestGap) { nearest = pane; nearestGap = gap; }
            }
            if (nearest && nearestGap <= reach) break;
            // Where a w x h pane sits with its near edge kRefugeGap of reach
            // from the player, toward (ux, uy).
            auto spotToward = [&](float w, float h, float ux, float uy, float& x, float& y) {
                const float along = reach * norma::kRefugeGap +
                                    std::fabs(ux) * w * 0.5f + std::fabs(uy) * h * 0.5f;
                x = std::clamp(psx + ux * along - w * 0.5f, 0.0f, std::max(0.0f, sw - w));
                y = std::clamp(psy + uy * along - h * 0.5f, 0.0f, std::max(0.0f, sh - h));
            };
            if (nearest) {
                const os_pane::Rect& r = nearest->frame;
                float ux = r.x + r.w * 0.5f - psx, uy = r.y + r.h * 0.5f - psy;
                const float len = std::max(1.0f, std::sqrt(ux * ux + uy * uy));
                ux /= len;
                uy /= len;
                for (PaneDrift& d : g_PaneDrifts) {
                    if (d.id != nearest->id) continue;
                    d.fromX = r.x;
                    d.fromY = r.y;
                    d.x = r.x;
                    d.y = r.y;
                    d.placed = true;
                    // Try fanned directions and keep the one clear of other panes.
                    float bestScore = -1.0f;
                    for (int k = 0; k < 9; ++k) {
                        const float turn = ((k + 1) / 2) * (k % 2 ? 0.35f : -0.35f);
                        const float c = cosf(turn), sn = sinf(turn);
                        float tx = 0.0f, ty = 0.0f;
                        spotToward(r.w, r.h, ux * c - uy * sn, ux * sn + uy * c, tx, ty);
                        const float sc = PaneOverlapScore(tx, ty, r.w, r.h, nearest->id);
                        if (bestScore < 0.0f || sc < bestScore) { bestScore = sc; d.toX = tx; d.toY = ty; }
                        if (sc <= 0.0f) break;
                    }
                    d.pullLeft = norma::kRefugePull;
                }
            } else if ((int)NormaOwnPanes().size() < norma::MaxBossPanesFor(boss->normaPhase)) {
                const float w = std::min(sw * norma::kPaneMaxShareW, RandRange(norma::kRefugeMinW, norma::kRefugeMaxW));
                const float h = std::min(sh * norma::kPaneMaxShareH, RandRange(norma::kRefugeMinH, norma::kRefugeMaxH));
                const float a0 = RandRange(0.0f, 6.2831853f);
                float x = 0.0f, y = 0.0f, bestScore = -1.0f;
                for (int k = 0; k < 12; ++k) {
                    const float a = a0 + 6.2831853f * (float)k / 12.0f;
                    float tx = 0.0f, ty = 0.0f;
                    spotToward(w, h, cosf(a), sinf(a), tx, ty);
                    const float sc = PaneOverlapScore(tx, ty, w, h, 0);
                    if (bestScore < 0.0f || sc < bestScore) { bestScore = sc; x = tx; y = ty; }
                    if (sc <= 0.0f) break;
                }
                const int id = os_pane::Open(x, y, w, h);
                if (id > 0) {
                    Audio::Play(Audio::Cue::BossOctansPaneOpen);
                    g_NormaPanes.push_back(id);
                    StartPaneDrift(id);
                    FlashNormaPane(id, *boss);
                }
            }
            break;
        }
        case norma::Ask::ClosePlayerWindow: {
            const int id = space::g_PlayerSpace;
            if (id <= 0 || !PaneById(id)) {
                boss->normaPaneId = -1;
                break;
            }
            os_pane::SetLocked(id, true);
            g_NormaCountdown = { id, norma::kCloseCountdown };
            Audio::Play(Audio::Cue::BossOctansCloseCount);   // 3초 틱 + 압착이 한 파일
            FlashNormaPane(id, *boss);
            boss->normaPaneId = id;
            break;
        }
        case norma::Ask::ReleaseWindows:
            ReleaseNormaClaims();
            break;
        case norma::Ask::ThrowBox: {
            if (g_ThrowBox.active) break;
            // The box opens beside the boss, on the player's side.
            const float bsx = W2SX(boss->worldX), bsy = W2SY(boss->worldY);
            const float ddx = psx - bsx, ddy = psy - bsy;
            const float len = std::max(1.0f, std::sqrt(ddx * ddx + ddy * ddy));
            const float w = norma::kThrowW, h = norma::kThrowH;
            const float cx = std::clamp(bsx + ddx / len * 220.0f, w * 0.5f, sw - w * 0.5f);
            const float cy = std::clamp(bsy + ddy / len * 220.0f, h * 0.5f, sh - h * 0.5f);
            // It opens after growing out of the boss (TickThrowBox).
            g_ThrowBox = {};
            g_ThrowBox.active = true;
            g_ThrowBox.stage = -1;
            g_ThrowBox.spot = { cx - w * 0.5f, cy - h * 0.5f, w, h };
            break;
        }
        case norma::Ask::WarpPlan: {
            // Two panes of the boss's: it hides in the one nearest it and
            // will smash out of the one nearest the player.
            const auto usable = [&](const os_pane::Pane* pane) {
                float fuse = 0.0f;
                return !(pane->minimized || pane->flying || FuseFor(pane->id, fuse) ||
                         pane->id == g_NormaCountdown.id || PaneRamBusy(pane->id));
            };
            for (int guard = 0; guard < 3; ++guard) {
                int ok = 0;
                for (const os_pane::Pane* pane : NormaOwnPanes()) if (usable(pane)) ++ok;
                if (ok >= 2) break;
                const int before = (int)NormaOwnPanes().size();
                if (before >= norma::MaxBossPanesFor(boss->normaPhase) + 1 ||
                    OpenNormaPane(*boss, psx, psy) <= 0) break;
                if ((int)NormaOwnPanes().size() == before) break;
            }
            const os_pane::Pane* src = nullptr;
            const os_pane::Pane* dst = nullptr;
            float srcD = 1.0e12f, dstD = 1.0e12f;
            const float bsx = W2SX(boss->worldX), bsy = W2SY(boss->worldY);
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                if (!usable(pane)) continue;
                const float cx = pane->frame.x + pane->frame.w * 0.5f, cy = pane->frame.y + pane->frame.h * 0.5f;
                const float d = (cx - bsx) * (cx - bsx) + (cy - bsy) * (cy - bsy);
                if (d < srcD) { srcD = d; src = pane; }
            }
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                if (!usable(pane) || pane == src) continue;
                const float cx = pane->frame.x + pane->frame.w * 0.5f, cy = pane->frame.y + pane->frame.h * 0.5f;
                const float d = (cx - psx) * (cx - psx) + (cy - psy) * (cy - psy);
                if (d < dstD) { dstD = d; dst = pane; }
            }
            if (!src || !dst) {
                boss->normaPaneId = -1;   // falls back on a direct attack
                break;
            }
            g_Warp = {};
            g_Warp.srcId = src->id;
            g_Warp.dstId = dst->id;
            {
                const float ex = src->frame.x + src->frame.w * 0.5f - bsx;
                const float ey = src->frame.y + src->frame.h * 0.5f - bsy;
                const float el = std::max(1.0f, std::sqrt(ex * ex + ey * ey));
                g_Warp.dirX = ex / el;
                g_Warp.dirY = ey / el;
            }
            g_Warp.srcX = src->frame.x;
            g_Warp.srcY = src->frame.y;
            g_Warp.dstX = dst->frame.x;
            g_Warp.dstY = dst->frame.y;
            boss->normaWarpSrcX = ScreenToWorldX(src->frame.x + src->frame.w * 0.5f);
            boss->normaWarpSrcY = ScreenToWorldY(src->frame.y + src->frame.h * 0.5f);
            FlashNormaPane(src->id, *boss);
            FlashNormaPane(dst->id, *boss);
            boss->normaPaneId = src->id;
            Audio::Play(Audio::Cue::BossOctansWarpCharge);   // 0.7초 동안 창으로 빨려 들어가려 함
            break;
        }
        case norma::Ask::WarpSmash: {
            // Out through the other pane: it is smashed, the boss stands in
            // its place.
            const os_pane::Pane* dst = PaneById(g_Warp.dstId);
            if (!dst) dst = PaneById(g_Warp.srcId);
            if (dst) {
                const os_pane::Rect frame = dst->frame;
                const float cx = ScreenToWorldX(frame.x + frame.w * 0.5f);
                const float cy = ScreenToWorldY(frame.y + frame.h * 0.5f);
                boss->worldX = cx;
                boss->worldY = cy;
                boss->normaVelX = boss->normaVelY = 0.0f;
                // The player caught inside is hurt and thrown out the near edge.
                if (psx >= frame.x && psx <= frame.x + frame.w && psy >= frame.y && psy <= frame.y + frame.h) {
                    const float toL = psx - frame.x, toR = frame.x + frame.w - psx;
                    const float toT = psy - frame.y, toB = frame.y + frame.h - psy;
                    const float m = std::min(std::min(toL, toR), std::min(toT, toB));
                    const float kx = m == toL ? -1.0f : m == toR ? 1.0f : 0.0f;
                    const float ky = m == toT ? -1.0f : m == toB ? 1.0f : 0.0f;
                    {
                        space::HurtSourceScope source(space::kUntracked);
                        HurtPlayer(g_GameManager.playerHP, norma::kWarpDamage, "NORMA_WARP");
                    }
                    ApplyPlayerKnockback(kx * norma::kWarpKnock, ky * norma::kWarpKnock);
                    Audio::Play(Audio::Cue::SfxKnockback);
                    SpawnSparks(g_NormaPlayerX, g_NormaPlayerY, 14, 1.0f, 0.40f, 0.34f, 380.0f);
                }
                CollapsePane(*dst, true);
                os_pane::Close(dst->id);
                Audio::Play(Audio::Cue::BossOctansPaneBurst);   // 숨은 창을 깨고 나옴
                // The glass: a ring of shots, shards flying from the frame's
                // edges, a shockwave and a jolt.
                const int ring = (int)std::round(norma::kThrowRing * norma::kWarpBurstShare);
                const int inner = (int)std::round(norma::kThrowInner * norma::kWarpBurstShare);
                const float spin = RandRange(0.0f, 6.2831853f);
                for (int i = 0; i < ring; ++i)
                    SpawnNormaShot(cx, cy, spin + 6.2831853f * (float)i / ring, norma::kThrowRingSpeed);
                for (int i = 0; i < inner; ++i)
                    SpawnNormaShot(cx, cy, spin + 6.2831853f * ((float)i + 0.5f) / inner, norma::kThrowInnerSpeed);
                for (int i = 0; i < 24; ++i) {
                    const float t = (float)i / 24.0f;
                    const float ex = frame.x + frame.w * t, ey = (i % 2 == 0) ? frame.y : frame.y + frame.h;
                    SpawnSparks(ScreenToWorldX(ex), ScreenToWorldY(ey), 3, 0.92f, 0.99f, 1.0f, 520.0f);
                }
                for (int i = 0; i < 8; ++i) {
                    const float t = (float)i / 8.0f;
                    SpawnSparks(ScreenToWorldX(frame.x), ScreenToWorldY(frame.y + frame.h * t), 3, 0.92f, 0.99f, 1.0f, 520.0f);
                    SpawnSparks(ScreenToWorldX(frame.x + frame.w), ScreenToWorldY(frame.y + frame.h * t), 3, 0.92f, 0.99f, 1.0f, 520.0f);
                }
                norma_fx::Frame(ScreenToWorldX(frame.x), ScreenToWorldY(frame.y),
                                ScreenToWorldX(frame.x + frame.w), ScreenToWorldY(frame.y + frame.h),
                                40.0f, 0.40f, norma_fx::kWhite);
                norma_fx::Seal(cx, cy, 220.0f, 0.35f, norma_fx::kWhite);
                SpawnEnemyExplosion(cx, cy, 0.72f, 0.94f, 1.0f, true);
                Shake(0.55f, 26.0f);
                TriggerHitStop(0.08f);
                TriggerFlash(0.72f, 0.94f, 1.0f, 0.18f);
            }
            g_Warp = {};
            break;
        }
        case norma::Ask::PhaseBlow: {
            // The phase changes: whatever the old pattern had running ends and
            // every pane the boss has blows.
            EndPaneWall();
            g_PaneRams.clear();
            g_Warp = {};
            if (g_NormaCountdown.id != 0) os_pane::SetLocked(g_NormaCountdown.id, false);
            g_NormaCountdown = {};
            if (g_ThrowBox.id != 0) os_pane::Close(g_ThrowBox.id);
            g_ThrowBox = {};
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                FlashNormaPane(pane->id, *boss);
                LightPaneFuse(pane->id);
            }
            break;
        }
        case norma::Ask::PhaseSpray:
            g_Spray = { norma::MaxBossPanesFor(boss->normaPhase), 0.0f };
            break;
        case norma::Ask::CrashStart: {
            if (g_Crash.active) break;
            g_Crash = {};
            g_Crash.active = true;
            g_Crash.total = g_Crash.toOpen = norma::CrashPanesFor((int)g_Difficulty);
            g_Crash.storm.reserve((size_t)g_Crash.total);
            norma::g_CrashOver = false;
            Audio::FadeOutBgm(400);
            // Nothing else may shelter or fire: the boss's own panes, its shot
            // box and rams fold away quietly (no bursts), the countdown ends.
            ReleaseNormaPanes();
            if (g_ThrowBox.id != 0) os_pane::Close(g_ThrowBox.id);
            g_ThrowBox = {};
            g_PaneRams.clear();
            g_NormaCountdown = {};
            g_Crash.safeId = OpenCrashSafe(*boss, psx, psy);
            break;
        }
        case norma::Ask::BuildWall: {
            if (g_Wall.active) { boss->normaPaneId = -1; break; }
            // A cascade of big panes over the ground between the boss and the
            // player, leaning one way or the other.
            const float bsx = W2SX(boss->worldX), bsy = W2SY(boss->worldY);
            const int n = norma::WallCountFor(boss->normaPhase, (int)g_Difficulty);
            const float spread = norma::WallSpreadFor(n);
            // Panes of random size, strewn across the line from the boss to the
            // player (spread sideways and at random depth), so each run's
            // barricade and footprints differ.
            const float ddx = psx - bsx, ddy = psy - bsy;
            const float len = std::max(1.0f, std::sqrt(ddx * ddx + ddy * ddy));
            const float ux = ddx / len, uy = ddy / len, nx = -uy, ny = ux;
            const auto frand = [](float a, float b) { return a + (b - a) * ((float)(rand() % 1001) / 1000.0f); };
            const float maxW = sw * norma::kWallMaxShareW, maxH = sh * norma::kWallMaxShareH;
            g_Wall = {};
            g_Wall.active = true;
            boss->normaPaneId = 1;
            for (int i = 0; i < n; ++i) {
                WallPane p;
                p.w = std::min(maxW, frand(norma::kWallMinW, norma::kWallMaxW));
                p.h = std::min(maxH, frand(norma::kWallMinH, norma::kWallMaxH));
                const float along = len * frand(0.25f, 0.75f);
                const float side = ((float)i - (float)(n - 1) * 0.5f) * spread +
                                   frand(-norma::kWallJitter, norma::kWallJitter);
                const float cx = bsx + ux * along + nx * side, cy = bsy + uy * along + ny * side;
                p.x = std::clamp(cx - p.w * 0.5f, 0.0f, std::max(0.0f, sw - p.w));
                p.y = std::clamp(cy - p.h * 0.5f, 0.0f, std::max(0.0f, sh - p.h));
                g_Wall.panes.push_back(p);
            }
            break;
        }
        case norma::Ask::RamWindow: {
            // Up to the phase's count of panes are hurled together, farthest
            // from the player first (the longest run). Fusing and minimized
            // panes are left alone.
            if (!g_PaneRams.empty()) {
                boss->normaPaneId = -1;
                break;
            }
            std::vector<std::pair<float, const os_pane::Pane*>> cands;
            for (const os_pane::Pane* pane : NormaOwnPanes()) {
                float fuse = 0.0f;
                if (pane->minimized || pane->flying || FuseFor(pane->id, fuse)) continue;
                if (pane->id == g_NormaCountdown.id) continue;
                const float ddx = pane->frame.x + pane->frame.w * 0.5f - psx;
                const float ddy = pane->frame.y + pane->frame.h * 0.5f - psy;
                cands.push_back({ ddx * ddx + ddy * ddy, pane });
            }
            std::sort(cands.begin(), cands.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
            if (cands.empty()) {
                boss->normaPaneId = -1;   // falls back on a direct attack
                break;
            }
            const size_t take = std::min(cands.size(), (size_t)norma::RamCountFor(boss->normaPhase));
            for (size_t i = 0; i < take; ++i) {
                const os_pane::Pane* pick = cands[i].second;
                PaneRam r;
                r.id = pick->id;
                r.fromX = r.x = pick->frame.x;
                r.fromY = r.y = pick->frame.y;
                r.fromW = r.w = pick->frame.w;
                r.fromH = r.h = pick->frame.h;
                g_PaneRams.push_back(r);
                FlashNormaPane(pick->id, *boss);
                os_pane::RaiseToFront(pick->id);   // never hidden behind another pane
            }
            boss->normaPaneId = cands[0].second->id;
            Audio::Play(Audio::Cue::BossOctansRam);   // 뒤로 당겨지는 순간부터
            break;
        }
        }
    }
    g_NormaFlashes.erase(std::remove_if(g_NormaFlashes.begin(), g_NormaFlashes.end(),
        [](const NormaFlash& f) { return g_GameTime - f.bornAt > 1.0f; }), g_NormaFlashes.end());
}

// A pane folding shut: its rectangle drawn shrinking to the centre.
struct PaneCollapse { os_pane::Rect rect; float age = 0.0f; };
static std::vector<PaneCollapse> g_PaneCollapses;
constexpr float kPaneCollapseSeconds = 0.35f;

static void Shake(float seconds, float magnitude) {
    g_ShakeTime = std::max(g_ShakeTime, seconds);
    g_ShakeMag = std::max(g_ShakeMag, magnitude);
}

// Turns the boss's effect cues into shockwaves, sparks and shake; and gives
// its fall a send-off.
// CAELUM's cues (CaelumBoss.h): shake, sparks and rings in its amber.
static void PlayCaelumEvents() {
    const float ar = 1.0f, ag = 0.82f, ab = 0.50f;
    for (const caelum::Event& e : caelum::g_Events) {
        switch (e.fx) {
        case caelum::Fx::Seed:
            SpawnSparks(e.x, e.y, 8, 1.0f, 0.95f, 0.82f, 300.0f);
            Shake(0.08f, 3.0f);
            break;
        case caelum::Fx::Window:
            SpawnSparks(e.x, e.y, e.a > 0.5f ? 18 : 10, 1.0f, 0.95f, 0.82f, 340.0f);
            Shake(0.12f, 5.0f);
            break;
        case caelum::Fx::Fire:
            SpawnSparks(e.x + cosf(e.a) * 70.0f, e.y + sinf(e.a) * 70.0f, 4, 1.0f, 0.95f, 0.82f, 260.0f);
            if (g_ShakeTime < 0.05f) Shake(0.06f, 3.0f);
            break;
        case caelum::Fx::Wave:
            Shake(0.22f, 10.0f);
            SpawnSparks(e.x, e.y, 16, 1.0f, 0.95f, 0.82f, 420.0f);
            break;
        case caelum::Fx::Morph:
            SpawnSparks(e.x, e.y, 14, ar, ag, ab, 360.0f);
            Shake(0.15f, 6.0f);
            break;
        case caelum::Fx::Overflow:
            SpawnSparks(e.x, e.y, 24, 1.0f, 0.95f, 0.82f, 520.0f);
            Shake(0.35f, 14.0f);
            TriggerHitStop(0.06f);
            TriggerFlash(1.0f, 0.9f, 0.7f, 0.18f);
            break;
        case caelum::Fx::Plug:       // the cursor presses the core: the drinking starts
            caelum_fx::Close(e.x, e.y, 160.0f, 0.30f, caelum_fx::kHot, 0.5f);
            Shake(0.10f, 4.0f);
            break;
        case caelum::Fx::Swallow:    // the last stream is in: a gulp, then it is wide open
            SpawnSparks(e.x, e.y, 12 + 2 * (int)e.a, 1.0f, 0.95f, 0.82f, 380.0f);
            caelum_fx::Close(e.x, e.y, 300.0f, 0.40f, caelum_fx::kAmber, 1.0f);   // the gulp
            Shake(0.25f, 9.0f);
            TriggerHitStop(0.04f);
            break;
        case caelum::Fx::Spout:      // phase 2: what it drank bursts back out
            SpawnSparks(e.x, e.y, 10 + 2 * (int)e.a, ar, ag, ab, 420.0f);
            caelum_fx::Open(e.x, e.y, 240.0f, 0.35f, caelum_fx::kHot, 0.8f);
            Shake(0.20f, 8.0f);
            break;
        case caelum::Fx::PhaseShift:
            SpawnSparks(e.x, e.y, 36, ar, ag, ab, 480.0f);
            caelum_fx::Open(e.x, e.y, 260.0f, 0.35f, caelum_fx::kHot, 1.0f);
            caelum_fx::Open(e.x, e.y, 520.0f, 0.70f, caelum_fx::kAmber, 1.0f, 0.08f);
            Shake(0.6f, 22.0f);
            TriggerHitStop(0.10f);
            TriggerFlash(1.0f, 0.88f, 0.6f, 0.22f);
            Audio::Play(Audio::Cue::BossPhaseChange);   // 2페이즈 곡은 UpdateBgmFlow 가 바꿈
            break;
        }
    }
    caelum::g_Events.clear();
}

// CAELUM's fall: called from the death pass (a dead mob is gone by the next frame).
static void PlayCaelumFall(float x, float y) {
    const float ar = 1.0f, ag = 0.82f, ab = 0.50f;
    SpawnEnemyExplosion(x, y, ar, ag, ab, true);
    for (int i = 0; i < 3; ++i)
        caelum_fx::Open(x, y, 280.0f + 240.0f * (float)i, 0.5f + 0.25f * (float)i,
                        i == 1 ? caelum_fx::kHot : caelum_fx::kAmber, 1.0f, 0.10f * (float)i);
    SpawnSparks(x, y, 50, 1.0f, 0.95f, 0.82f, 600.0f);
    Audio::Play(Audio::Cue::BossDeath);
    Shake(0.7f, 26.0f);
    TriggerHitStop(0.16f);
    TriggerFlash(1.0f, 0.88f, 0.6f, 0.26f);
}

static float g_NormaLastX = 0.0f, g_NormaLastY = 0.0f;
static bool g_NormaWasAlive = false;
static void PlayNormaEvents() {
    const float ir = 0.72f, ig = 0.94f, ib = 1.0f;
    for (const norma::Event& e : norma::g_Events) {
        switch (e.fx) {
        case norma::Fx::Volley:
            Audio::Play(Audio::Cue::BossOctansShot);   // 난사: 네 갈래 한 발씩
            for (int k = 0; k < 4; ++k) {
                const float a = e.a + 1.5707963f * (float)k;
                SpawnSparks(e.x + cosf(a) * 62.0f, e.y + sinf(a) * 62.0f, 2, ir, ig, ib, 220.0f);
            }
            break;
        case norma::Fx::Muzzle:
            Audio::Play(Audio::Cue::BossOctansShot);   // 조준 연사 한 발
            SpawnSparks(e.x + cosf(e.a) * 70.0f, e.y + sinf(e.a) * 70.0f, 2, ir, ig, ib, 260.0f);
            if (g_ShakeTime < 0.05f) Shake(0.05f, 2.5f);
            break;
        case norma::Fx::Burst:
            Audio::Play(Audio::Cue::BossOctansAlert);
            SpawnSparks(e.x, e.y, (int)(10.0f + 12.0f * e.a), 0.92f, 0.99f, 1.0f, 380.0f);
            norma_fx::Seal(e.x, e.y, 180.0f + 160.0f * e.a, 0.45f, norma_fx::kIce);
            Shake(0.18f + 0.10f * e.a, 6.0f + 6.0f * e.a);
            break;
        case norma::Fx::StarOut:
            // A health star went dark: a short flare and a beat of hit stop.
            Audio::Play(Audio::Cue::BossOctansStarout);
            SpawnSparks(e.x, e.y, 16, ir, ig, ib, 420.0f);
            norma_fx::Seal(e.x, e.y, 150.0f, 0.30f, norma_fx::kWhite);
            Shake(0.16f, 7.0f);
            TriggerHitStop(0.05f);
            break;
        case norma::Fx::Wave:
            Audio::Play(Audio::Cue::BossOctansWave);
            SpawnSparks(e.x, e.y, 6, ir, ig, ib, 260.0f);
            norma_fx::Wave(e.x, e.y, e.a, 1.6f);   // e.a: the wave's shot speed
            Shake(0.10f, 5.0f);
            break;
        case norma::Fx::EnterPane:
        case norma::Fx::LeavePane:
            Audio::Play(e.fx == norma::Fx::EnterPane ? Audio::Cue::BossOctansEnterPane
                                                     : Audio::Cue::BossOctansLeavePane);
            // Into a pane the seal folds shut; out of one it opens.
            if (e.fx == norma::Fx::EnterPane) norma_fx::Implode(e.x, e.y, 150.0f, 0.35f, norma_fx::kIce);
            else norma_fx::Seal(e.x, e.y, 150.0f, 0.35f, norma_fx::kIce);
            SpawnEnemyExplosion(e.x, e.y, ir, ig, ib, false);
            Shake(0.20f, 9.0f);
            break;
        case norma::Fx::PhaseShift:
            SpawnEnemyExplosion(e.x, e.y, ir, ig, ib, true);
            SpawnSparks(e.x, e.y, 40, 0.92f, 0.99f, 1.0f, 520.0f);
            // Three seals open one after another, small to large.
            norma_fx::Seal(e.x, e.y, 160.0f, 0.35f, norma_fx::kWhite);
            norma_fx::Seal(e.x, e.y, 320.0f, 0.50f, norma_fx::kIce, 0.06f);
            norma_fx::Seal(e.x, e.y, 520.0f, 0.70f, norma_fx::kWhite, 0.12f);
            Shake(0.6f, 24.0f);
            TriggerHitStop(0.12f);
            TriggerFlash(0.72f, 0.94f, 1.0f, 0.22f);
            Audio::Play(Audio::Cue::BossPhaseChange);   // 2페이즈 곡은 UpdateBgmFlow 가 바꿈
            break;
        }
    }
    norma::g_Events.clear();

    const Monster* boss = FindNorma();
    if (boss) {
        g_NormaLastX = boss->worldX;
        g_NormaLastY = boss->worldY;
        g_NormaWasAlive = true;
    } else if (g_NormaWasAlive) {
        g_NormaWasAlive = false;
        for (int i = 0; i < 3; ++i)
            norma_fx::Seal(g_NormaLastX, g_NormaLastY, 300.0f + 260.0f * (float)i, 0.5f + 0.25f * (float)i,
                           i == 1 ? norma_fx::kIce : norma_fx::kWhite, 0.10f * (float)i);
        SpawnEnemyExplosion(g_NormaLastX, g_NormaLastY, 0.72f, 0.94f, 1.0f, true);
        SpawnSparks(g_NormaLastX, g_NormaLastY, 60, 0.92f, 0.99f, 1.0f, 640.0f);
        Audio::Play(Audio::Cue::BossDeath);
        Shake(0.8f, 30.0f);
        TriggerHitStop(0.18f);
        TriggerFlash(0.92f, 0.99f, 1.0f, 0.30f);
    }
}

// 8 Throw: fill, fly, fuse, burst.
static void TickThrowBox(float dt) {
    if (!g_ThrowBox.active) return;
    if (g_ThrowBox.stage == -1) {
        g_ThrowBox.timer += dt;
        if (g_ThrowBox.timer < norma::kThrowAppear) return;
        const os_pane::Rect& r = g_ThrowBox.spot;
        const int id = os_pane::Open(r.x, r.y, r.w, r.h);
        const Monster* boss = FindNorma();
        if (id <= 0 || !boss) { g_ThrowBox = {}; return; }
        g_ThrowBox.id = id;
        g_ThrowBox.stage = 0;
        g_ThrowBox.timer = 0.0f;
        Audio::Play(Audio::Cue::BossOctansPaneOpen);
        FlashNormaPane(id, *boss);
        SpawnSparks(ScreenToWorldX(r.x + r.w * 0.5f), ScreenToWorldY(r.y + r.h * 0.5f),
                    14, 0.92f, 0.99f, 1.0f, 320.0f);
        Shake(0.12f, 6.0f);
        return;
    }
    const os_pane::Pane* pane = PaneById(g_ThrowBox.id);
    if (!pane || pane->minimized) {           // closed in time: no burst
        if (pane) os_pane::Close(g_ThrowBox.id);
        g_ThrowBox = {};
        return;
    }
    ThrowBox& box = g_ThrowBox;
    box.timer += dt;
    if (box.stage == 0) {
        if (box.timer < norma::kThrowFill) return;
        // Aim where the player will be when the box lands (about 0.8 s of
        // flight), keeping the box on screen.
        const float pcx = W2SX(playerCenterXForNorma() + g_NormaPlayerVX * 0.8f);
        const float pcy = W2SY(playerCenterYForNorma() + g_NormaPlayerVY * 0.8f);
        box.tx = std::clamp(pcx - pane->frame.w * 0.5f, 0.0f, (float)screenWidth - pane->frame.w);
        box.ty = std::clamp(pcy - pane->frame.h * 0.5f, 0.0f, (float)screenHeight - pane->frame.h);
        box.x = pane->frame.x;
        box.y = pane->frame.y;
        box.stage = 1;
        box.timer = 0.0f;
        Audio::Play(Audio::Cue::BossOctansThrow);   // 채운 상자가 날아감
        return;
    }
    if (box.stage == 1) {
        const float ddx = box.tx - box.x, ddy = box.ty - box.y;
        const float remaining = std::sqrt(ddx * ddx + ddy * ddy);
        const float step = norma::kThrowStartSpeed * std::exp(box.timer / norma::kThrowRamp) * dt;
        if (step >= remaining) {
            box.x = box.tx;
            box.y = box.ty;
            box.stage = 2;
            box.timer = 0.0f;
            Shake(0.15f, 8.0f);
        } else {
            box.x += ddx / remaining * step;
            box.y += ddy / remaining * step;
        }
        os_pane::Place(box.id, std::round(box.x), std::round(box.y));
        return;
    }
    if (box.timer < norma::kThrowFuse) return;
    // Burst: the shots it held fly out from where it landed.
    const float cx = ScreenToWorldX(pane->shown.x + pane->shown.w * 0.5f);
    const float cy = ScreenToWorldY(pane->shown.y + pane->shown.h * 0.5f);
    CollapsePane(*pane, true);
    os_pane::Close(box.id);
    Audio::Play(Audio::Cue::BossOctansPaneBurst);
    g_ThrowBox = {};
    const float spin = RandRange(0.0f, 6.2831853f);
    for (int i = 0; i < norma::kThrowRing; ++i)
        SpawnNormaShot(cx, cy, spin + 6.2831853f * (float)i / norma::kThrowRing,
                       norma::kThrowRingSpeed);
    for (int i = 0; i < norma::kThrowInner; ++i)
        SpawnNormaShot(cx, cy, spin + 6.2831853f * ((float)i + 0.5f) / norma::kThrowInner,
                       norma::kThrowInnerSpeed);
    SpawnEnemyExplosion(cx, cy, 0.72f, 0.94f, 1.0f, true);
    SpawnSparks(cx, cy, 30, 0.92f, 0.99f, 1.0f, 460.0f);
    norma_fx::Seal(cx, cy, 360.0f, 0.50f, norma_fx::kIce);
    Shake(0.35f, 16.0f);
}

// 10 Ram: draw the pane back while aiming at the player, then hurl it until
// it meets the screen edge. Its frame sweeping over the player or a mob
// hurts and shoves it; whoever was already inside when it set off is safe.
static void TickOneRam(PaneRam& ram, float dt) {
    if (ram.id == 0) return;
    const os_pane::Pane* pane = PaneById(ram.id);
    if (!pane || pane->minimized) { ram = {}; return; }
    if (pane->flying && ram.stage < 2) {   // dragged away: control broken
        ram.stage = 2;
        ram.timer = 0.0f;
    }
    ram.timer += dt;
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float psx = W2SX(g_NormaPlayerX), psy = W2SY(g_NormaPlayerY);
    const auto inside = [&](float x, float y) {
        return x >= ram.x && x <= ram.x + ram.w && y >= ram.y && y <= ram.y + ram.h;
    };
    if (ram.stage == 0) {
        const Monster* boss = FindNorma();
        const float aim = norma::RamAimFor(boss ? boss->normaPhase : 1);
        const float k = std::clamp(ram.timer / aim, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - k) * (1.0f - k);
        // The pane keeps its own size all the way.
        ram.w = ram.fromW;
        ram.h = ram.fromH;
        const float mx = ram.fromX + ram.fromW * 0.5f, my = ram.fromY + ram.fromH * 0.5f;
        if (k < norma::kRamLock) {
            const float ddx = psx - mx, ddy = psy - my;
            const float len = std::max(1.0f, std::sqrt(ddx * ddx + ddy * ddy));
            ram.dirX = ddx / len;
            ram.dirY = ddy / len;
        }
        // Draws back away from the player, easing out.
        const float back = norma::kRamPullBack * ease;
        ram.x = std::clamp(mx - ram.w * 0.5f - ram.dirX * back, 0.0f, std::max(0.0f, sw - ram.w));
        ram.y = std::clamp(my - ram.h * 0.5f - ram.dirY * back, 0.0f, std::max(0.0f, sh - ram.h));
        os_pane::Resize(ram.id, std::round(ram.x), std::round(ram.y), std::round(ram.w), std::round(ram.h));
        if (k >= 1.0f) {
            ram.stage = 1;
            ram.timer = 0.0f;
            ram.playerInside = inside(psx, psy);
            os_pane::RaiseToFront(ram.id);   // in front as it sets off, whatever was stacked since
        }
        return;
    }
    if (ram.stage == 1) {
        const float speed = std::min(norma::kRamMaxSpeed,
                                     norma::kRamStartSpeed * std::exp(ram.timer / norma::kRamRamp));
        ram.x += ram.dirX * speed * dt;
        ram.y += ram.dirY * speed * dt;
        bool hitEdge = false;
        if (ram.x < 0.0f) { ram.x = 0.0f; hitEdge = true; }
        if (ram.y < 0.0f) { ram.y = 0.0f; hitEdge = true; }
        if (ram.x + ram.w > sw) { ram.x = std::max(0.0f, sw - ram.w); hitEdge = true; }
        if (ram.y + ram.h > sh) { ram.y = std::max(0.0f, sh - ram.h); hitEdge = true; }
        os_pane::Place(ram.id, std::round(ram.x), std::round(ram.y));
        // The frame sweeping over the player.
        const bool now = inside(psx, psy);
        if (now && !ram.playerInside && !ram.hitPlayer) {
            ram.hitPlayer = true;
            {
                space::HurtSourceScope source(space::kUntracked);
                HurtPlayer(g_GameManager.playerHP, norma::kRamDamage, "NORMA_RAM");
            }
            ApplyPlayerKnockback(ram.dirX * norma::kRamKnock, ram.dirY * norma::kRamKnock);
            Audio::Play(Audio::Cue::SfxKnockback);
            SpawnSparks(g_NormaPlayerX, g_NormaPlayerY, 14, 1.0f, 0.40f, 0.34f, 380.0f);
            Shake(0.25f, 12.0f);
            TriggerHitStop(0.05f);
        }
        ram.playerInside = now;
        // ...and over mobs: they are crushed once each.
        const auto crush = [&](auto& m) {
            const void* key = &m;
            if (std::find(ram.hitMobs.begin(), ram.hitMobs.end(), key) != ram.hitMobs.end()) return;
            if (!inside(W2SX(m.worldX), W2SY(m.worldY))) return;
            ram.hitMobs.push_back(key);
            g_MonsterManager.ApplyDamage(m, norma::kRamMobDamage, space::kUntracked);
            SpawnSparks(m.worldX, m.worldY, 6, 1.0f, 0.40f, 0.34f, 260.0f);
        };
        for (Monster* m : g_MonsterManager.monsters)
            if (m->alive && m->kind != MobKind::NORMA) crush(*m);
        for (RangedMob* r : g_MonsterManager.rangedMobs)
            if (r->alive) crush(*r);
        if (hitEdge || ram.timer > 3.0f) {
            // Impact at the leading edge.
            const float ex = ram.x + ram.w * 0.5f + ram.dirX * ram.w * 0.5f;
            const float ey = ram.y + ram.h * 0.5f + ram.dirY * ram.h * 0.5f;
            const float wx = ScreenToWorldX(ex), wy = ScreenToWorldY(ey);
            SpawnSparks(wx, wy, 22, 1.0f, 0.45f, 0.38f, 460.0f);
            norma_fx::Seal(wx, wy, 220.0f, 0.35f, norma_fx::kRed);
            if (hitEdge) {
                // The wall cracks the box open: a thrown box's load sprays all the way round
                // (evenly, not just back off the wall).
                // The box itself bursts (as a thrown one does): a shot born
                // inside a pane that stays open would die on its border.
                const float cx = ScreenToWorldX(pane->shown.x + pane->shown.w * 0.5f);
                const float cy = ScreenToWorldY(pane->shown.y + pane->shown.h * 0.5f);
                const int ring = (int)std::round(norma::kThrowRing * norma::kRamBurstShare);
                const int inner = (int)std::round(norma::kThrowInner * norma::kRamBurstShare);
                const float back = std::atan2(-ram.dirY, -ram.dirX);
                const float half = norma::kRamBurstFan * 0.5f;
                CollapsePane(*pane, true);
                os_pane::Close(ram.id);
                Audio::Play(Audio::Cue::BossOctansPaneBurst);
                for (int i = 0; i < ring; ++i)
                    SpawnNormaShot(cx, cy, back - half + norma::kRamBurstFan * ((float)i + 0.5f) / ring,
                                   norma::kThrowRingSpeed);
                for (int i = 0; i < inner; ++i)
                    SpawnNormaShot(cx, cy, back - half + norma::kRamBurstFan * ((float)i + 0.5f) / inner,
                                   norma::kThrowInnerSpeed);
                SpawnEnemyExplosion(cx, cy, 0.72f, 0.94f, 1.0f, true);
                SpawnSparks(cx, cy, 30, 0.92f, 0.99f, 1.0f, 460.0f);
                Shake(0.5f, 24.0f);
                TriggerHitStop(0.06f);
                if (Monster* boss = FindNorma()) boss->normaExposed = norma::kExposeAfterRam;
                ram = {};
                return;
            }
            Shake(0.35f, 16.0f);
            if (Monster* boss = FindNorma()) boss->normaExposed = norma::kExposeAfterRam;
            ram.stage = 2;
            ram.timer = 0.0f;
        }
        return;
    }
    if (ram.timer >= norma::kRamSettle) ram = {};
}

static void TickPaneRam(float dt) {
    for (PaneRam& r : g_PaneRams) TickOneRam(r, dt);
    g_PaneRams.erase(std::remove_if(g_PaneRams.begin(), g_PaneRams.end(),
        [](const PaneRam& r) { return r.id == 0; }), g_PaneRams.end());
}

// 14 Warp: 0..1 how far through its hide the boss is (the smash-to-be builds).
static float WarpHideShare() {
    const Monster* b = FindNorma();
    if (!b || b->normaPattern != norma::Pattern::Warp || b->normaStage != norma::Stage::Act) return 0.0f;
    return std::clamp(b->normaTimer / norma::kWarpHide, 0.0f, 1.0f);
}

// Cracks spreading from the middle of the pane about to be smashed: more of
// them, and longer, as the hide runs out (thin solid lines, no fill).
static void DrawWarpCracks(const os_pane::Pane& p) {
    if (g_Warp.dstId == 0 || p.id != g_Warp.dstId) return;
    const float k = WarpHideShare();
    if (k < 0.12f) return;
    const hud_lines::Rgb c{ 1.0f, 0.62f, 0.56f };
    const float cx = p.shown.x + p.shown.w * 0.5f, cy = p.shown.y + p.shown.h * 0.5f;
    const float reach = std::min(p.shown.w, p.shown.h) * (0.18f + 0.62f * k);
    const int n = 3 + (int)(7.0f * k);
    unsigned seed = (unsigned)p.id * 2654435761u;
    const auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(seed >> 8) / 16777216.0f; };
    for (int i = 0; i < n; ++i) {
        float a = 6.2831853f * ((float)i + rnd() * 0.6f) / (float)n;
        float x = cx + cosf(a) * 6.0f, y = cy + sinf(a) * 6.0f;
        const int segs = 3;
        for (int sgi = 0; sgi < segs; ++sgi) {
            a += (rnd() - 0.5f) * 0.9f;
            const float len = reach * (0.28f + 0.2f * rnd());
            // The cracks stay inside the frame: a crack that meets the edge stops there.
            const float nx = std::clamp(x + cosf(a) * len, p.shown.x + 3.0f, p.shown.x + p.shown.w - 3.0f);
            const float ny = std::clamp(y + sinf(a) * len, p.shown.y + 3.0f, p.shown.y + p.shown.h - 3.0f);
            PaneSeg(x, y, nx, ny, 1.4f, c, (0.35f + 0.5f * k) * (1.0f - 0.25f * (float)sgi));
            x = nx;
            y = ny;
        }
    }
}

// 14 Warp: the pane the boss went into jolts the way it went in, then fires
// an aimed stream from its edge at the player while the boss is inside; the
// pane to be smashed trembles harder as its time runs out.
static void TickPaneWarp(float dt) {
    if (g_Warp.srcId == 0) return;
    Monster* boss = FindNorma();
    if (!boss || boss->normaPattern != norma::Pattern::Warp) { g_Warp = {}; return; }
    const os_pane::Pane* src = PaneById(g_Warp.srcId);
    const os_pane::Pane* dst = PaneById(g_Warp.dstId);
    const float psx = W2SX(g_NormaPlayerX), psy = W2SY(g_NormaPlayerY);
    if (boss->normaHidden && g_Warp.recoilT < 0.0f && src) {
        g_Warp.recoilT = 0.0f;
        const float cx = ScreenToWorldX(src->frame.x + src->frame.w * 0.5f);
        const float cy = ScreenToWorldY(src->frame.y + src->frame.h * 0.5f);
        SpawnSparks(cx, cy, 16, 0.92f, 0.99f, 1.0f, 420.0f);
        norma_fx::Seal(cx, cy, 200.0f, 0.35f, norma_fx::kWhite);
        Shake(0.22f, 10.0f);
    }
    if (src && !src->flying) {
        float ox = 0.0f, oy = 0.0f;
        if (g_Warp.recoilT >= 0.0f) {
            g_Warp.recoilT += dt;
            const float t = g_Warp.recoilT;
            const float a = norma::kWarpRecoil * std::exp(-7.0f * t) * std::cos(16.0f * t);
            ox += g_Warp.dirX * a;
            oy += g_Warp.dirY * a;
        }
        g_Warp.kick *= std::exp(-16.0f * dt);
        ox -= g_Warp.kickX * 9.0f * g_Warp.kick;
        oy -= g_Warp.kickY * 9.0f * g_Warp.kick;
        os_pane::Place(g_Warp.srcId, std::round(g_Warp.srcX + ox), std::round(g_Warp.srcY + oy));
    }
    const float k = WarpHideShare();
    if (dst && !dst->flying && k > 0.0f) {
        const float amp = norma::kWarpTremble * k * k;
        os_pane::Place(g_Warp.dstId,
                       std::round(g_Warp.dstX + amp * sinf(g_GameTime * 97.0f)),
                       std::round(g_Warp.dstY + amp * cosf(g_GameTime * 83.0f)));
    }
}

// Phase change: fresh panes pop open one after another.
static void TickPhaseSpray(float dt) {
    if (g_Spray.left <= 0) return;
    Monster* boss = FindNorma();
    if (!boss) { g_Spray = {}; return; }
    g_Spray.timer -= dt;
    while (g_Spray.left > 0 && g_Spray.timer <= 0.0f) {
        g_Spray.timer += norma::kPhaseSprayStagger;
        --g_Spray.left;
        const int id = OpenNormaPane(*boss, W2SX(g_NormaPlayerX), W2SY(g_NormaPlayerY));
        if (id <= 0) continue;
        FlashNormaPane(id, *boss);
        if (const os_pane::Pane* pane = PaneById(id))
            SpawnSparks(ScreenToWorldX(pane->frame.x + pane->frame.w * 0.5f),
                        ScreenToWorldY(pane->frame.y + pane->frame.h * 0.5f), 10, 0.92f, 0.99f, 1.0f, 340.0f);
        Shake(0.08f, 4.0f);
    }
}

// 13 Wall: the panes pop open one by one, the line stands, then every pane
// is lit to blow (the same fuse and burst as Detonate).
static void EndPaneWall() {
    for (const WallPane& p : g_Wall.panes)
        if (p.id > 0)
            if (const os_pane::Pane* pane = PaneById(p.id)) {
                CollapsePane(*pane);
                os_pane::Close(p.id);
            }
    g_Wall = {};
}

// Nothing can stand inside a wall pane: whatever is gets pushed out through
// the nearest edge (a few passes, as the cascade's panes overlap). With a
// `slide`, it is also carried along that edge toward the side (playerSX,
// playerSY) the player is on, so it goes round the corner rather than sticks.
static std::vector<os_pane::Rect> WallRects() {
    std::vector<os_pane::Rect> rects;
    for (const WallPane& p : g_Wall.panes)
        if (p.id > 0)
            if (const os_pane::Pane* pane = PaneById(p.id)) rects.push_back(pane->frame);
    return rects;
}

static bool PushOutOfWall(const std::vector<os_pane::Rect>& rects, float& wx, float& wy,
                          float margin, float slide, float playerSX, float playerSY) {
    std::vector<space::Box> boxes;
    for (const os_pane::Rect& r : rects) boxes.push_back({ r.x, r.y, r.w, r.h });
    const float sx = W2SX(wx), sy = W2SY(wy);
    float bx = sx, by = sy;
    if (!space::PushOutOfBoxes(boxes, margin, (float)screenWidth, (float)screenHeight, bx, by))
        return false;
    // Carried along the edge it came out of, toward the player's side.
    if (slide > 0.0f) {
        const bool horizontal = std::fabs(bx - sx) >= std::fabs(by - sy);
        const float nx = horizontal ? bx : bx + (playerSX >= bx ? 1.0f : -1.0f) * slide;
        const float ny = horizontal ? by + (playerSY >= by ? 1.0f : -1.0f) * slide : by;
        if (!space::InsideBoxes(boxes, margin, nx, ny)) { bx = nx; by = ny; }
    }
    wx = ScreenToWorldX(bx);
    wy = ScreenToWorldY(by);
    return true;
}

static void KeepMobsOutOfWall(float dt) {
    const std::vector<os_pane::Rect> rects = WallRects();
    if (rects.empty()) return;
    const float playerSX = W2SX(g_NormaPlayerX), playerSY = W2SY(g_NormaPlayerY);
    const float slide = norma::kWallDetourSpeed * dt;
    for (Monster* m : g_MonsterManager.monsters)
        if (m->alive && m->kind != MobKind::NORMA)   // the boss walks through its own wall
            PushOutOfWall(rects, m->worldX, m->worldY, 0.0f, slide, playerSX, playerSY);
    for (RangedMob* r : g_MonsterManager.rangedMobs)
        if (r->alive) PushOutOfWall(rects, r->worldX, r->worldY, 0.0f, slide, playerSX, playerSY);
}

// The player is kept out the same way, right after its move each step (world
// centre in, world centre out): walking or dashing at the wall stops at its
// edge, and knockback stops there too.
static void KeepPlayerOutOfWall(float& pcx, float& pcy) {
    if (!g_Wall.active) return;
    const std::vector<os_pane::Rect> rects = WallRects();
    if (rects.empty()) return;
    PushOutOfWall(rects, pcx, pcy, norma::kWallPlayerMargin, 0.0f, 0.0f, 0.0f);
}

static void TickPaneWall(float dt) {
    PaneWall& wall = g_Wall;
    if (!wall.active) return;
    if (!FindNorma()) { EndPaneWall(); return; }
    wall.timer += dt;
    KeepMobsOutOfWall(dt);
    if (wall.stage == 0) {
        const float t = wall.timer - norma::kWallWarn;   // the footprints show first
        if (t < 0.0f) return;
        const float psx = W2SX(g_NormaPlayerX), psy = W2SY(g_NormaPlayerY);
        while (wall.opened < (int)wall.panes.size() &&
               t >= (float)wall.opened * norma::kWallStagger) {
            WallPane& p = wall.panes[wall.opened];
            p.id = os_pane::Open(p.x, p.y, p.w, p.h);
            if (p.id > 0) {
                Audio::Play(Audio::Cue::BossOctansPaneOpen);   // 벽 창이 하나씩 열림
                const float cx = ScreenToWorldX(p.x + p.w * 0.5f), cy = ScreenToWorldY(p.y + p.h * 0.5f);
                SpawnSparks(cx, cy, 8, 1.0f, 0.45f, 0.38f, 300.0f);
                // The pane bursts open: its frame pushes out a little past the footprint.
                norma_fx::Frame(ScreenToWorldX(p.x), ScreenToWorldY(p.y), ScreenToWorldX(p.x + p.w),
                                ScreenToWorldY(p.y + p.h), 26.0f, 0.40f, norma_fx::kRed);
                Shake(0.1f, 6.0f);
                // Caught in the footprint as it pops open: hurt, and thrown
                // out through the nearest edge.
                if (!wall.hitPlayer && psx >= p.x && psx <= p.x + p.w && psy >= p.y && psy <= p.y + p.h) {
                    wall.hitPlayer = true;
                    const float toL = psx - p.x, toR = p.x + p.w - psx, toT = psy - p.y, toB = p.y + p.h - psy;
                    const float m = std::min(std::min(toL, toR), std::min(toT, toB));
                    const float kx = m == toL ? -1.0f : m == toR ? 1.0f : 0.0f;
                    const float ky = m == toT ? -1.0f : m == toB ? 1.0f : 0.0f;
                    {
                        space::HurtSourceScope source(space::kUntracked);
                        HurtPlayer(g_GameManager.playerHP, norma::kWallDamage, "NORMA_WALL");
                    }
                    ApplyPlayerKnockback(kx * norma::kWallKnock, ky * norma::kWallKnock);
                    Audio::Play(Audio::Cue::SfxKnockback);
                    SpawnSparks(g_NormaPlayerX, g_NormaPlayerY, 14, 1.0f, 0.40f, 0.34f, 380.0f);
                    Shake(0.25f, 12.0f);
                    TriggerHitStop(0.05f);
                }
            }
            ++wall.opened;
        }
        if (wall.opened == (int)wall.panes.size() &&
            t >= (float)wall.panes.size() * norma::kWallStagger + norma::kWallHold) {
            wall.stage = 1;
            wall.timer = 0.0f;
        }
        return;
    }
    if (wall.stage == 1) {
        if (wall.timer < norma::kWallStand) return;
        for (const WallPane& p : wall.panes)
            if (p.id > 0 && PaneById(p.id)) LightPaneFuse(p.id);
        wall.stage = 2;
        wall.timer = 0.0f;
        return;
    }
    // Fuses burning: done once every pane has gone (or the player closed them).
    bool any = false;
    for (const WallPane& p : wall.panes)
        if (p.id > 0 && PaneById(p.id)) any = true;
    if (!any || wall.timer > 3.0f) EndPaneWall();
}

// CloseWindow: when the count runs out, everything left in the pane is
// crushed and the pane closes.
static void TickNormaCountdown(float dt) {
    if (g_NormaCountdown.id == 0) return;
    const os_pane::Pane* pane = PaneById(g_NormaCountdown.id);
    if (!pane) { g_NormaCountdown = {}; return; }
    g_NormaCountdown.left -= dt;
    if (g_NormaCountdown.left > 0.0f) return;
    const int id = g_NormaCountdown.id;
    {
        space::HurtSourceScope source(id);
        HurtPlayer(g_GameManager.playerHP, norma::kCloseDamage, "NORMA_CLOSE");
    }
    for (Monster* m : g_MonsterManager.monsters)
        if (m->alive && m->kind != MobKind::NORMA && space::SpaceAt(m->worldX, m->worldY) == id)
            g_MonsterManager.ApplyDamage(*m, norma::kCloseMobDamage, id);
    for (RangedMob* r : g_MonsterManager.rangedMobs)
        if (r->alive && space::SpaceAt(r->worldX, r->worldY) == id)
            g_MonsterManager.ApplyDamage(*r, norma::kCloseMobDamage, id);
    const float cx = ScreenToWorldX(pane->shown.x + pane->shown.w * 0.5f);
    const float cy = ScreenToWorldY(pane->shown.y + pane->shown.h * 0.5f);
    SpawnEnemyExplosion(cx, cy, 0.86f, 0.80f, 1.0f, true);
    SpawnSparks(cx, cy, 30, 0.92f, 0.99f, 1.0f, 480.0f);
    norma_fx::Frame(ScreenToWorldX(pane->shown.x), ScreenToWorldY(pane->shown.y),
                    ScreenToWorldX(pane->shown.x + pane->shown.w), ScreenToWorldY(pane->shown.y + pane->shown.h),
                    34.0f, 0.45f, norma_fx::kLilac);
    Shake(0.35f, 18.0f);
    TriggerHitStop(0.08f);
    g_PaneCollapses.push_back({ pane->shown, 0.0f });
    Audio::Play(Audio::Cue::OsPaneClose);
    os_pane::SetLocked(id, false);
    os_pane::Close(id);
    g_NormaPanes.erase(std::remove(g_NormaPanes.begin(), g_NormaPanes.end(), id), g_NormaPanes.end());
    g_NormaCountdown = {};
}

static const NormaFlash* FlashFor(int id) {
    for (auto it = g_NormaFlashes.rbegin(); it != g_NormaFlashes.rend(); ++it)
        if (it->id == id) return &*it;
    return nullptr;
}
static float FlashAge(const NormaFlash& f) { return g_GameTime - f.bornAt; }
static bool ThrowPayloadFor(int id, int& count, float& turn) {
    if (g_ThrowBox.id != id || id == 0) return false;
    const int total = norma::kThrowRing / 2;   // drawn squares stand for the load
    count = g_ThrowBox.stage == 0
        ? std::max(1, (int)(total * std::min(1.0f, g_ThrowBox.timer / norma::kThrowFill)))
        : total;
    turn = g_GameTime * (g_ThrowBox.stage == 2 ? 9.0f : 3.0f);
    return true;
}
// The box growing out of the boss: a square leaves the body, swells and
// slides to its spot, trailing two fainter copies and lines back to the boss.
static void DrawThrowAppear() {
    if (!g_ThrowBox.active || g_ThrowBox.stage != -1) return;
    const Monster* boss = FindNorma();
    if (!boss) return;
    const float bx = W2SX(boss->worldX), by = W2SY(boss->worldY);
    const float bh = 18.0f * boss->sizeScale * norma::kAimMuzzle * g_ViewZoom;   // the star points
    const os_pane::Rect from{ bx - bh, by - bh, bh * 2.0f, bh * 2.0f };
    const os_pane::Rect& to = g_ThrowBox.spot;
    const hud_lines::Rgb ice{ 0.72f, 0.94f, 1.0f };
    const auto at = [&](float t) {
        const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        return os_pane::Rect{ from.x + (to.x - from.x) * e, from.y + (to.y - from.y) * e,
                              from.w + (to.w - from.w) * e, from.h + (to.h - from.h) * e };
    };
    const float t = std::clamp(g_ThrowBox.timer / norma::kThrowAppear, 0.0f, 1.0f);
    BindMainShader();
    for (int g = 2; g >= 1; --g) {
        const float tg = std::max(0.0f, t - 0.12f * (float)g);
        DrawPaneOutline(at(tg), ice, 1.2f, 0.25f / (float)g);
    }
    const os_pane::Rect r = at(t);
    DrawPaneOutline(r, ice, 2.4f, 0.95f);
    const float cxs[2] = { r.x, r.x + r.w }, cys[2] = { r.y, r.y + r.h };
    for (float cx : cxs)
        for (float cy : cys)
            hud_lines::Segment(bx, by, cx, cy, 1.0f, ice, 0.35f * (1.0f - t));
    BatchFlush();
}

static bool CountdownFor(int id, float& left) {
    if (g_NormaCountdown.id != id || id == 0) return false;
    left = g_NormaCountdown.left;
    return true;
}

static void CollapsePane(const os_pane::Pane& pane, bool quiet) {
    g_PaneCollapses.push_back({ pane.shown, 0.0f });
    if (!quiet) Audio::Play(Audio::Cue::OsPaneClose);   // 창이 수축하며 닫힘 (터진 창은 폭발음만)
}

static void DrawPaneCollapses(float dt) {
    if (g_PaneCollapses.empty()) return;
    BindMainShader();
    for (PaneCollapse& c : g_PaneCollapses) {
        c.age += dt;
        const float t = std::clamp(c.age / kPaneCollapseSeconds, 0.0f, 1.0f);
        const float k = 1.0f - t * t;
        const float cx = c.rect.x + c.rect.w * 0.5f, cy = c.rect.y + c.rect.h * 0.5f;
        const os_pane::Rect r{ cx - c.rect.w * 0.5f * k, cy - c.rect.h * 0.5f * k,
                               c.rect.w * k, c.rect.h * k };
        DrawPaneOutline(r, { 0.92f, 0.99f, 1.0f }, 2.5f, 1.0f - t);   // just the frame folding in
    }
    g_PaneCollapses.erase(std::remove_if(g_PaneCollapses.begin(), g_PaneCollapses.end(),
        [](const PaneCollapse& c) { return c.age >= kPaneCollapseSeconds; }), g_PaneCollapses.end());
    BatchFlush();
}

// Worn-out panes counting down to their blast.
struct PaneFuse { int id = 0; float timer = 0.0f; };
static std::vector<PaneFuse> g_PaneFuses;

// The blue screen's timeline (norma::Pattern::Crash, started by CrashStart),
// in real time so it runs through its own hit-stop: the storm opens a few
// panes a frame over kCrashSpawn and glitches harder through the countdown;
// then the game crashes to a blue screen (everything outside the safe pane is
// hit, the storm closes, play freezes), cracks spread over it from the boss,
// and it shatters back to the game; the safe pane closes after a moment.
static void EndCrash() {
    for (int id : g_Crash.storm) os_pane::Close(id);
    if (g_Crash.safeId != 0) os_pane::Close(g_Crash.safeId);
    g_Crash = {};
}

// The crack web over the blue screen, like glass hit by a stone: from a random
// impact point, kCrashRays jagged rays wander outward (each stops at its own
// length), crossed near the impact by many short ring cracks and farther out
// by few, with short side branches. Point (i, k) is ray i at ring k; the
// cells between rays and rings are the shards (the inner ones fall first).
static constexpr int kCrashRays = 22, kCrashRings = 11;
static void BuildCrashCracks() {
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float diag = std::sqrt(sw * sw + sh * sh);
    static const float kRing[kCrashRings] = { 0.0f, 0.022f, 0.045f, 0.075f, 0.115f, 0.17f,
                                              0.25f, 0.37f, 0.55f, 0.85f, 1.4f };
    const float ix = RandRange(sw * 0.28f, sw * 0.72f), iy = RandRange(sh * 0.3f, sh * 0.7f);
    g_Crash.ix = ix;
    g_Crash.iy = iy;
    g_Crash.px.assign(kCrashRays * kCrashRings, 0.0f);
    g_Crash.py.assign(kCrashRays * kCrashRings, 0.0f);
    g_Crash.ringVis.assign(kCrashRays * kCrashRings, 0);
    g_Crash.rayLen.assign(kCrashRays, kCrashRings - 1);
    g_Crash.branches.clear();
    // Uneven spokes: random gaps between the rays' bearings.
    float gaps[kCrashRays], sum = 0.0f;
    for (float& g : gaps) { g = RandRange(0.4f, 1.6f); sum += g; }
    float bearing = RandRange(0.0f, 6.2831853f);
    for (int i = 0; i < kCrashRays; ++i) {
        float a = bearing;
        bearing += 6.2831853f * gaps[i] / sum;
        for (int k = 0; k < kCrashRings; ++k) {
            if (k > 0) a += RandRange(-0.07f, 0.07f);   // the ray wanders as it goes
            const float r = kRing[k] * diag * (k > 0 ? RandRange(0.82f, 1.18f) : 0.0f);
            g_Crash.px[i * kCrashRings + k] = ix + cosf(a) * r;
            g_Crash.py[i * kCrashRings + k] = iy + sinf(a) * r;
        }
        // Most rays run off the screen; some stop short.
        g_Crash.rayLen[i] = RandRange(0.0f, 1.0f) < 0.35f ? (int)RandRange(4.0f, 8.0f) : kCrashRings - 1;
        // Ring cracks: dense near the impact, rare far out.
        for (int k = 1; k < kCrashRings - 1; ++k) {
            const float p = k <= 3 ? 0.9f : k <= 5 ? 0.55f : k <= 7 ? 0.25f : 0.08f;
            g_Crash.ringVis[i * kCrashRings + k] = RandRange(0.0f, 1.0f) < p ? 1 : 0;
        }
        // A side branch or two off the ray.
        const int nb = (int)RandRange(0.0f, 2.99f);
        for (int b = 0; b < nb; ++b) {
            const int k = (int)RandRange(2.0f, 7.0f);
            const float x0 = g_Crash.px[i * kCrashRings + k], y0 = g_Crash.py[i * kCrashRings + k];
            const float ba = atan2f(y0 - iy, x0 - ix) + (RandRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f) * RandRange(0.3f, 0.7f);
            const float bl = RandRange(0.04f, 0.12f) * diag;
            const float mx = x0 + cosf(ba) * bl * 0.5f + RandRange(-6.0f, 6.0f), my = y0 + sinf(ba) * bl * 0.5f + RandRange(-6.0f, 6.0f);
            g_Crash.branches.insert(g_Crash.branches.end(), { x0, y0, mx, my });
            g_Crash.branches.insert(g_Crash.branches.end(), { mx, my, x0 + cosf(ba + 0.15f) * bl, y0 + sinf(ba + 0.15f) * bl });
        }
    }
}

static void TickCrash(float dt) {
    if (!g_Crash.active) return;
    Monster* boss = FindNorma();
    if (!boss) { EndCrash(); return; }
    g_Crash.t += dt;
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    const float countAt = norma::kCrashBuild, blueAt = norma::kCrashBuild + norma::kCrashCount;
    const float shatterAt = blueAt + norma::kCrashBlue + norma::kCrashCrack;
    // The storm: as many panes as the time asks for, at most a few a frame.
    if (g_Crash.toOpen > 0) {
        const int due = (int)std::ceil(g_Crash.total * std::min(1.0f, g_Crash.t / norma::kCrashSpawn));
        int opened = g_Crash.total - g_Crash.toOpen;
        for (int k = 0; k < norma::kCrashOpenPerFrame && opened < due; ++k, ++opened) {
            const float w = std::min(sw * 0.4f, RandRange(norma::kCrashMinW, norma::kCrashMaxW));
            const float h = std::min(sh * 0.4f, RandRange(norma::kCrashMinH, norma::kCrashMaxH));
            const int id = os_pane::Open(RandRange(0.0f, std::max(0.0f, sw - w)),
                                         RandRange(0.0f, std::max(0.0f, sh - h)), w, h, true);
            --g_Crash.toOpen;
            if (id > 0) g_Crash.storm.push_back(id);
            if (id > 0) Audio::Play(Audio::Cue::BossOctansErrorPop);   // 오류창 폭풍 (쓰로틀이 솎아 냄)
        }
        if (g_Crash.safeId != 0) os_pane::RaiseToFront(g_Crash.safeId);   // the shelter stays on top
    }
    // 카운트다운 소리: 시작할 때 과부하 상승음, 1초마다 시스템 비프 (3, 2, 1)
    if (!g_Crash.blue) {
        const float before = g_Crash.t - dt;
        if (before < countAt && g_Crash.t >= countAt) Audio::Play(Audio::Cue::BossOctansCrashCharge);
        for (int k = 0; k < 3; ++k) {
            const float at = countAt + (float)k;
            if (before < at && g_Crash.t >= at) Audio::Play(Audio::Cue::BossOctansCrashTick);
        }
    }
    // The glitch grows through the countdown, in steps so panes repaint only on a change.
    if (!g_Crash.blue) {
        float u = g_Crash.t < countAt ? 0.35f * g_Crash.t / countAt
                                      : 0.35f + 0.65f * (g_Crash.t - countAt) / norma::kCrashCount;
        u = std::round(std::clamp(u, 0.0f, 1.0f) * 20.0f) / 20.0f;
        for (int id : g_Crash.storm) os_pane::SetUnstable(id, u);
    }
    if (g_Crash.safeId != 0)
        os_pane::SetRole(g_Crash.safeId, os_pane::PaneRole::Safe, std::max(0.0f, blueAt - g_Crash.t),
                         norma::kCrashCount);
    // The crash: the blue screen falls.
    if (!g_Crash.blue && g_Crash.t >= blueAt) {
        g_Crash.blue = true;
        Audio::Play(Audio::Cue::BossOctansCrash);   // 블루스크린이 떨어짐
        const os_pane::Pane* safe = PaneById(g_Crash.safeId);
        const float scx = safe ? safe->frame.x + safe->frame.w * 0.5f : sw * 0.5f;
        const float scy = safe ? safe->frame.y + safe->frame.h * 0.5f : sh * 0.5f;
        if (!(safe && space::g_PlayerSpace == g_Crash.safeId)) {
            HurtPlayer(g_GameManager.playerHP, norma::kCrashDamage, "NORMA_CRASH");
            float kx = W2SX(g_NormaPlayerX) - scx, ky = W2SY(g_NormaPlayerY) - scy;
            const float len = std::max(1.0f, std::sqrt(kx * kx + ky * ky));
            ApplyPlayerKnockback(kx / len * norma::kCrashKnock, ky / len * norma::kCrashKnock);
            Audio::Play(Audio::Cue::SfxKnockback);
        }
        for (int id : g_Crash.storm) os_pane::Close(id);   // hidden under the blue anyway
        g_Crash.storm.clear();
        BuildCrashCracks();
        TriggerHitStop(norma::kCrashBlue + norma::kCrashCrack);
    }
    // 재부팅 소리: 팬이 돌고 디스크가 읽히고 '삑' 한 뒤 화면이 깨지며 게임이 돌아온다
    if (g_Crash.blue && !g_Crash.shattered) {
        const float rebootAt = shatterAt - 1.2f;
        if (g_Crash.t - dt < rebootAt && g_Crash.t >= rebootAt) Audio::Play(Audio::Cue::BossOctansReboot);
    }
    // The shattering: the game shows again and the boss turns open.
    if (g_Crash.blue && !g_Crash.shattered && g_Crash.t >= shatterAt) {
        g_Crash.shattered = true;
        norma::g_CrashOver = true;
        Audio::Play(Audio::Cue::BossOctansCrashShatter);   // 화면이 깨지며 게임이 다시 보임
        // Mobs outside the safe pane are hit as the screen breaks, not when it
        // falls: kills (and a level-up's card) wait until the game shows again.
        for (Monster* m : g_MonsterManager.monsters)
            if (m->alive && m->kind != MobKind::NORMA && space::SpaceAt(m->worldX, m->worldY) != g_Crash.safeId)
                g_MonsterManager.ApplyDamage(*m, norma::kCrashMobDamage, space::kUntracked);
        for (RangedMob* r : g_MonsterManager.rangedMobs)
            if (r->alive && space::SpaceAt(r->worldX, r->worldY) != g_Crash.safeId)
                g_MonsterManager.ApplyDamage(*r, norma::kCrashMobDamage, space::kUntracked);
        Shake(0.45f, 22.0f);
    }
    if (g_Crash.shattered && g_Crash.t >= shatterAt + norma::kCrashShatter + norma::kCrashSafeLinger) {
        if (const os_pane::Pane* p = PaneById(g_Crash.safeId)) CollapsePane(*p);
        EndCrash();
    }
}

// The charge before the crash: rings closing in on the boss (drawn with the
// pane brackets, in screen space).
static void DrawCrashCharge() {
    if (!g_Crash.active || g_Crash.blue || g_Crash.t <= norma::kCrashBuild) return;
    const Monster* boss = FindNorma();
    if (!boss) return;
    const hud_lines::Rgb ice{ 0.72f, 0.94f, 1.0f };
    const float k = std::clamp((g_Crash.t - norma::kCrashBuild) / norma::kCrashCount, 0.0f, 1.0f);
    const float bx = W2SX(boss->worldX), by = W2SY(boss->worldY);
    for (int ring = 0; ring < 3; ++ring) {
        const float f = std::fmod(g_GameTime * 1.6f + ring / 3.0f, 1.0f);
        const float rad = (60.0f + 260.0f * (1.0f - f)) * (1.0f - 0.4f * k);
        constexpr int kSeg = 40;
        for (int i = 0; i < kSeg; ++i) {
            const float q0 = 6.2831853f * i / kSeg, q1 = 6.2831853f * (i + 1) / kSeg;
            hud_lines::Segment(bx + cosf(q0) * rad, by + sinf(q0) * rad, bx + cosf(q1) * rad, by + sinf(q1) * rad,
                               1.4f + 1.6f * k, ice, (0.15f + 0.6f * k) * f);
        }
    }
}

// The blue screen over the whole game (HUD and the safe pane included): its
// text, then the cracks spreading from the impact, then the shards falling
// out, the inner ones first.
static void DrawCrashScreen(float sw, float sh, float scale) {
    if (!g_Crash.active || !g_Crash.blue) return;
    const float blueAt = norma::kCrashBuild + norma::kCrashCount;
    const float since = g_Crash.t - blueAt;
    const float crackAt = norma::kCrashBlue, shatterAt = norma::kCrashBlue + norma::kCrashCrack;
    if (since > shatterAt + norma::kCrashShatter) return;
    const float br = 0.0f, bg = 0.47f, bb = 0.84f;   // the blue
    const hud_lines::Rgb white{ 0.97f, 0.99f, 1.0f };
    const auto P = [&](int i, int k, float& x, float& y) {
        x = g_Crash.px[i * kCrashRings + k];
        y = g_Crash.py[i * kCrashRings + k];
    };
    // The cracks, grown to `k` (0..1): rays first, then their rings and branches.
    const auto cracks = [&](float k, float alpha) {
        const float reach = 1.0f + k * 1.6f * (kCrashRings - 1);   // in rings, rays race ahead
        for (int i = 0; i < kCrashRays; ++i)
            for (int r = 0; r < g_Crash.rayLen[i] && (float)r < reach; ++r) {
                float x0, y0, x1, y1;
                P(i, r, x0, y0);
                P(i, r + 1, x1, y1);
                const float part = std::min(1.0f, reach - (float)r);
                const float w = (r < 3 ? 2.2f : r < 6 ? 1.6f : 1.1f) * scale;
                hud_lines::Segment(x0, y0, x0 + (x1 - x0) * part, y0 + (y1 - y0) * part, w, white, 0.95f * alpha);
            }
        const float ringReach = k * (kCrashRings - 1);
        for (int i = 0; i < kCrashRays; ++i)
            for (int r = 1; r < kCrashRings - 1 && (float)r < ringReach; ++r) {
                if (!g_Crash.ringVis[i * kCrashRings + r]) continue;
                const int j = (i + 1) % kCrashRays;
                if (r > g_Crash.rayLen[i] && r > g_Crash.rayLen[j]) continue;
                float x0, y0, x1, y1;
                P(i, r, x0, y0);
                P(j, r, x1, y1);
                // A slight kink mid-way, so rings are not straight chords.
                const float mx = (x0 + x1) * 0.5f + (y1 - y0) * 0.06f, my = (y0 + y1) * 0.5f - (x1 - x0) * 0.06f;
                const float w = (r < 4 ? 1.5f : 1.0f) * scale;
                hud_lines::Segment(x0, y0, mx, my, w, white, 0.85f * alpha);
                hud_lines::Segment(mx, my, x1, y1, w, white, 0.85f * alpha);
            }
        if (k > 0.5f)
            for (size_t b = 0; b + 3 < g_Crash.branches.size(); b += 4)
                hud_lines::Segment(g_Crash.branches[b], g_Crash.branches[b + 1], g_Crash.branches[b + 2],
                                   g_Crash.branches[b + 3], 1.0f * scale, white, 0.8f * alpha * std::min(1.0f, (k - 0.5f) * 4.0f));
    };
    BindMainShader();
    if (since < shatterAt) {
        const float a = std::min(1.0f, since / 0.06f);
        if (since < crackAt) {
            BatchTri(0.0f, 0.0f, sw, 0.0f, sw, sh, br, bg, bb, a);
            BatchTri(0.0f, 0.0f, sw, sh, 0.0f, sh, br, bg, bb, a);
        } else {
            // Cracked: the blue is its shards (they tile all but the hole at the impact).
            for (int i = 0; i < kCrashRays; ++i)
                for (int r = 1; r + 1 < kCrashRings; ++r) {
                    float x[4], y[4];
                    P(i, r, x[0], y[0]);
                    P((i + 1) % kCrashRays, r, x[1], y[1]);
                    P((i + 1) % kCrashRays, r + 1, x[2], y[2]);
                    P(i, r + 1, x[3], y[3]);
                    BatchTri(x[0], y[0], x[1], y[1], x[2], y[2], br, bg, bb, 1.0f);
                    BatchTri(x[0], y[0], x[2], y[2], x[3], y[3], br, bg, bb, 1.0f);
                }
        }
        BatchFlush();
        // The text, under the cracks.
        const int li = std::max(0, std::min(2, LangIndex()));
        static const wchar_t* const kLine1[3] = { L"하늘에 문제가 발생하여 다시 시작해야 합니다.",
            L"Your sky ran into a problem and needs to restart.", L"空に問題が発生したため、再起動する必要があります。" };
        static const wchar_t* const kLine2[3] = { L"오류 정보를 수집하는 중입니다. %d%% 완료",
            L"We're just collecting some error info. %d%% complete", L"エラー情報を収集しています。%d%% 完了" };
        static const wchar_t* const kLine3[3] = { L"중지 코드: SEAL_BREACH", L"Stop code: SEAL_BREACH",
                                                  L"停止コード: SEAL_BREACH" };
        static const wchar_t* const kLine4[3] = { L"실패한 항목: octans.sys", L"What failed: octans.sys",
                                                  L"失敗した内容: octans.sys" };
        const float x = sw * 0.1f, y = sh * 0.18f;
        float body = 1.45f * scale;
        const float room = sw - x - 24.0f * scale, w1 = g_TextS.Width(kLine1[li], body);
        if (w1 > room && w1 > 0.0f) body *= room / w1;
        const float big = 6.0f * scale, fine = std::min(0.85f * scale, body * 0.6f);
        g_TextS.Draw(L":(", x, y, big, white.r, white.g, white.b, a);
        float yy = y + g_TextS.Height(L":(", big) + 44.0f * scale;
        g_TextS.Draw(kLine1[li], x, yy, body, white.r, white.g, white.b, a);
        yy += g_TextS.Height(kLine1[li], body) + 34.0f * scale;
        wchar_t line2[128];
        swprintf_s(line2, kLine2[li], std::min(100, (int)(100.0f * since / norma::kCrashBlue)));
        g_TextS.Draw(line2, x, yy, body, white.r, white.g, white.b, a);
        yy += g_TextS.Height(line2, body) + 56.0f * scale;
        g_TextS.Draw(kLine3[li], x, yy, fine, white.r, white.g, white.b, 0.9f * a);
        yy += g_TextS.Height(kLine3[li], fine) + 10.0f * scale;
        g_TextS.Draw(kLine4[li], x, yy, fine, white.r, white.g, white.b, 0.9f * a);
        BindMainShader();
        if (since >= crackAt) {
            // The impact: the cracks race out in the first fifth of the crack time.
            const float k = std::clamp((since - crackAt) / (norma::kCrashCrack * 0.2f), 0.0f, 1.0f);
            cracks(k, 1.0f);
            // The hole: the innermost ring of shards is already gone, its rim a ragged line.
            for (int i = 0; i < kCrashRays; ++i) {
                float x0, y0, x1, y1;
                P(i, 1, x0, y0);
                P((i + 1) % kCrashRays, 1, x1, y1);
                hud_lines::Segment(x0, y0, x1, y1, 2.4f * scale, white, k);
            }
            BatchFlush();
        }
        return;
    }
    // Shards: each cell falls out from the impact, the inner ones first,
    // turning, dropping and fading; their edges are the cracks.
    const float diag = std::sqrt(sw * sw + sh * sh);
    const float tShatter = since - shatterAt;
    for (int i = 0; i < kCrashRays; ++i)
        for (int r = 1; r + 1 < kCrashRings; ++r) {
            float x[4], y[4];
            P(i, r, x[0], y[0]);
            P((i + 1) % kCrashRays, r, x[1], y[1]);
            P((i + 1) % kCrashRays, r + 1, x[2], y[2]);
            P(i, r + 1, x[3], y[3]);
            const float cx = (x[0] + x[1] + x[2] + x[3]) * 0.25f, cy = (y[0] + y[1] + y[2] + y[3]) * 0.25f;
            float dx = cx - g_Crash.ix, dy = cy - g_Crash.iy;
            const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            dx /= len;
            dy /= len;
            // Inner shards let go first; each a little different (from its cell index).
            const float jitter = std::fmod((float)(i * 37 + r * 91) * 0.6180339f, 1.0f);
            const float delay = 0.35f * norma::kCrashShatter * std::min(1.0f, len / (diag * 0.6f)) + 0.05f * jitter;
            const float u = std::clamp((tShatter - delay) / (norma::kCrashShatter - delay), 0.0f, 1.0f);
            const float fade = 1.0f - u * u;
            const float speed = 260.0f + 0.5f * len, spin = (jitter - 0.5f) * 2.4f * u;
            const float ox = dx * speed * u, oy = dy * speed * u + 1400.0f * u * u;
            const float cs = cosf(spin), sn = sinf(spin), shrink = 1.0f - 0.3f * u;
            for (int c = 0; c < 4; ++c) {
                const float lx = (x[c] - cx) * shrink, ly = (y[c] - cy) * shrink;
                x[c] = cx + lx * cs - ly * sn + ox;
                y[c] = cy + lx * sn + ly * cs + oy;
            }
            if (fade <= 0.0f) continue;
            BatchTri(x[0], y[0], x[1], y[1], x[2], y[2], br, bg, bb, fade);
            BatchTri(x[0], y[0], x[2], y[2], x[3], y[3], br, bg, bb, fade);
            for (int c = 0; c < 4; ++c)
                hud_lines::Segment(x[c], y[c], x[(c + 1) % 4], y[(c + 1) % 4], 1.2f * scale, white, 0.85f * fade);
        }
    BatchFlush();
}

// The countdown over the storm: "fatal error" and the seconds left, centred
// near the top (text only, no plate).
static void DrawCrashCountdown(float sw, float scale, float topY) {
    if (!g_Crash.active || g_Crash.blue || g_Crash.t < norma::kCrashBuild * 0.5f) return;
    const float left = norma::kCrashBuild + norma::kCrashCount - g_Crash.t;
    const int li = std::max(0, std::min(2, LangIndex()));
    static const wchar_t* const kHead[3] = { L"치명적인 오류", L"FATAL ERROR", L"致命的なエラー" };
    static const wchar_t* const kHint[3] = { L"안전 모드 창으로 피하세요", L"Get inside the Safe Mode window",
                                             L"セーフ モードのウィンドウへ" };
    const float beat = 0.6f + 0.4f * sinf(g_GameTime * (left < 1.0f ? 26.0f : 12.0f));
    const float ts = 1.1f * scale, ts2 = 0.5f * scale;
    wchar_t head[64];
    swprintf_s(head, L"%ls  %d", kHead[li], std::max(1, (int)std::ceil(std::min(left, norma::kCrashCount))));
    const float y = topY + 150.0f * scale;
    const float hw = g_TextS.Width(head, ts);
    g_TextS.Draw(head, (sw - hw) * 0.5f, y, ts, 1.0f, 0.36f, 0.40f, 0.95f * beat);
    const float iw = g_TextS.Width(kHint[li], ts2);
    g_TextS.Draw(kHint[li], (sw - iw) * 0.5f, y + g_TextS.Height(head, ts) + 10.0f * scale, ts2,
                 0.72f, 0.94f, 1.0f, 0.9f);
}

static const PaneFuse* FuseOf(int id) {
    for (const PaneFuse& f : g_PaneFuses) if (f.id == id) return &f;
    return nullptr;
}

// Lights a pane's fuse (once). The player can still close or minimize the
// pane to defuse it.
static void LightPaneFuse(int id) {
    const os_pane::Pane* p = PaneById(id);
    if (!p || FuseOf(id)) return;
    g_PaneFuses.push_back({ id, 0.0f });
    Audio::Play(Audio::Cue::BossOctansFuse);
    SpawnSparks(ScreenToWorldX(p->shown.x + p->shown.w * 0.5f),
                ScreenToWorldY(p->shown.y + p->shown.h * 0.5f), 10, 0.86f, 0.80f, 1.0f, 300.0f);
}

static void ApplyPaneAbsorbs() {
    for (const space::Absorb& a : space::g_Absorbed) {
        if (a.enemy) SpawnSparks(a.x, a.y, 5, 0.86f, 0.80f, 1.0f, 240.0f);
        else SpawnSparks(a.x, a.y, 5, 0.92f, 0.99f, 1.0f, 240.0f);
        if (a.paneId <= 0) continue;
        WearOf(a.paneId).hits += 1.0f;
        if (const os_pane::Pane* p = PaneById(a.paneId))
            AddPaneHit(p->shown, a.paneId, W2SX(a.x), W2SY(a.y), a.enemy);
    }
    space::g_Absorbed.clear();
    for (const os_pane::Pane& p : os_pane::Panes()) {
        if (!p.open || p.decor) continue;   // the blue screen's storm sets its own glitch
        const float left = PaneDurabilityLeft(p);
        // Worn out: light the fuse once. Low: the contents start to waver.
        if (left <= 0.0f) LightPaneFuse(p.id);
        float unsteady = 0.0f;
        if (const PaneFuse* f = FuseOf(p.id))
            unsteady = 0.6f + 0.4f * std::min(1.0f, f->timer / space::kFuseSeconds);
        else if (left < space::kUnsteadyShare)
            unsteady = 0.35f * (1.0f - left / space::kUnsteadyShare);
        os_pane::SetUnstable(p.id, unsteady);
        os_pane::SetWear(p.id, left, FuseOf(p.id) != nullptr);
        // The job a pattern gives this pane shows as a system dialog.
        bool wall = false;
        for (const WallPane& w : g_Wall.panes) wall = wall || (g_Wall.active && w.id == p.id);
        if (CrashSafePane(p.id)) {
            // TickCrash keeps the safe-mode dialog's count.
        } else if (g_NormaCountdown.id == p.id && p.id != 0) {
            os_pane::SetRole(p.id, os_pane::PaneRole::Shutdown, g_NormaCountdown.left, norma::kCloseCountdown);
        } else if (g_ThrowBox.id == p.id && p.id != 0) {
            const float total = (float)(norma::kThrowRing + norma::kThrowInner);
            const float share = g_ThrowBox.stage == 0 ? std::min(1.0f, g_ThrowBox.timer / norma::kThrowFill) : 1.0f;
            os_pane::SetRole(p.id, os_pane::PaneRole::Copy, std::floor(total * share), total);
        } else if (wall) {
            os_pane::SetRole(p.id, os_pane::PaneRole::Error);
        } else {
            os_pane::SetRole(p.id, os_pane::PaneRole::Plain);
        }
    }
    g_PaneWear.erase(std::remove_if(g_PaneWear.begin(), g_PaneWear.end(),
        [](const PaneWear& w) {
            for (const os_pane::Pane& p : os_pane::Panes())
                if (p.id == w.id && p.open) return false;
            return true;
        }), g_PaneWear.end());
}

// A lit fuse burns for space::kFuseSeconds of play, then the pane folds shut
// and scatters its load like a thrown box (no blast of its own). Closing or
// minimizing the pane while it burns defuses it: no shots.
static void TickPaneFuses(float dt) {
    for (PaneFuse& fuse : g_PaneFuses) {
        const os_pane::Pane* pane = PaneById(fuse.id);
        if (!pane) { fuse.timer = -1.0f; continue; }
        if (pane->minimized) {
            os_pane::Close(fuse.id);
            fuse.timer = -1.0f;
            continue;
        }
        fuse.timer += dt;
        if (fuse.timer < space::kFuseSeconds) continue;
        const float cx = ScreenToWorldX(pane->shown.x + pane->shown.w * 0.5f);
        const float cy = ScreenToWorldY(pane->shown.y + pane->shown.h * 0.5f);
        SpawnSparks(cx, cy, 18, 0.92f, 0.99f, 1.0f, 320.0f);
        g_PaneCollapses.push_back({ pane->shown, 0.0f });
        Audio::Play(Audio::Cue::BossOctansPaneBurst);   // 퓨즈가 다 타 창이 터짐 (벽·폭파·페이즈)
        os_pane::Close(fuse.id);
        fuse.timer = -1.0f;
        const int ring = (int)std::round(norma::kThrowRing * norma::kDetonateBurstShare);
        const int inner = (int)std::round(norma::kThrowInner * norma::kDetonateBurstShare);
        const float spin = RandRange(0.0f, 6.2831853f);
        const size_t first = g_Bullets.size();
        for (int i = 0; i < ring; ++i)
            SpawnNormaShot(cx, cy, spin + 6.2831853f * (float)i / ring, norma::kThrowRingSpeed);
        for (int i = 0; i < inner; ++i)
            SpawnNormaShot(cx, cy, spin + 6.2831853f * ((float)i + 0.5f) / inner,
                           norma::kThrowInnerSpeed);
        // Every pane open now stops the burst, even ones opened after it.
        for (size_t i = first; i < g_Bullets.size(); ++i) g_Bullets[i].anyPaneStops = true;
        Shake(0.3f, 14.0f);
    }
    g_PaneFuses.erase(std::remove_if(g_PaneFuses.begin(), g_PaneFuses.end(),
        [](const PaneFuse& f) { return f.timer < 0.0f; }), g_PaneFuses.end());
}

static bool FuseFor(int id, float& share) {
    const PaneFuse* fuse = FuseOf(id);
    const os_pane::Pane* pane = PaneById(id);
    if (!fuse || !pane) return false;
    share = std::clamp(fuse->timer / space::kFuseSeconds, 0.0f, 1.0f);
    return true;
}
// Static Field's weapon pulse is gameplay state, not just a player-shell
// visual.  Keeping its timer outside the render path makes the area weapon
// behave consistently at every frame rate and during fixed-step simulation.
float g_StaticFieldPulseTimer = 0.0f;
inline void TriggerShotKick() { g_ShotKickTimer = 0.05f; }

// ?�면 ?�들�?(보스 ?�폰, 충격??
float g_ShakeTime = 0.0f;
float g_ShakeMag  = 0.0f;

// 보스 보상 ???��? 버프 ????(?�버???�이지 skip)
// ?�?�?배드 ?�터 ?�망 ?�류�????�시 감속 구역(?�상 ?�역). ?�에 ?�으�??�동?�도 -10% ?�?�?
//   즉시 ?�기지 ?�고 ZONE_OPEN(0.7�???걸쳐 ?�점 부?�되???�짐(grow factor = age/OPEN).


// ?�?�?백신 ?�캔 (증강) ??주기?�으�??�레?�어 주�????�화 ?�스(범위 ?�소) ?�?�?

// ?�?�?보스 ?�장 ?�조(증상) ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
//   보스 ?�폰??"결정 ??2.5�??�조(?�마 증상 + 경고 배너) ???�제 ?�성" ?�로 분리.
//   ?�조 ?�안 게임?�레?�는 계속(?�레그래??. 만료 ??결정??보스�??�제�??�성.
bool  g_LastRunRecord   = false;  // 직전 ?�이 ?�기록이?�는지 (GAMEOVER ?�시??

float g_WindowSizeCur = 0.0f;

static float EffectivePlayerWinSize() {
    if (g_WindowSizeCur >= 64.0f) return g_WindowSizeCur;
    if (g_Stats.windowSize >= 64.0f) return g_Stats.windowSize;
    return std::max(400.0f * g_Scale, 64.0f);
}

static float ClampPlayerWinSize(float sz) {
    if (sz != sz || sz < 64.0f) sz = EffectivePlayerWinSize();
    return sz;
}

float g_WinPrevHP     = -1.0f;    // Previous HP used to detect damage.
float g_HurtVignette  = 0.0f;     // Remaining hit vignette time.
float g_HpBarPop      = 0.0f;     // Remaining time to show the circular HP ring.

// Level-up signal: two thin rings in the XP colour expand from the
// player while LevelUpTransition runs (lines only, no fill or veil).
static void DrawLevelUpRings(float cx, float cy, float playerSize,
                             float progress) {
    if (progress <= 0.0f) return;
    constexpr float kTwoPi = 6.2831853f;
    for (int ring = 0; ring < 2; ++ring) {
        const float delay = ring * 0.18f;
        const float t = std::clamp((progress - delay) / (1.0f - delay), 0.0f, 1.0f);
        if (t <= 0.0f) continue;
        const float radius = playerSize * (1.2f + (ring == 0 ? 5.2f : 3.4f) * EaseOutCubic(t));
        const float alpha = (ring == 0 ? 0.85f : 0.60f) * (1.0f - t);
        DrawSettingsOrbitArc(cx, cy, radius, radius, 0.0f, kTwoPi,
                             0.30f, 0.82f, 1.0f,
                             ring == 0 ? 2.2f : 1.4f, alpha);
    }
}

static void DrawEdgeVignette(float width, float intensity,
                             float r, float g, float b) {
    if (width <= 0.0f || intensity <= 0.0f) return;
    constexpr int kBands = 10;
    const float bandWidth = width / kBands;
    const float sw = (float)screenWidth, sh = (float)screenHeight;
    BindMainShader();
    for (int band = 0; band < kBands; ++band) {
        const float inset = band * bandWidth;
        const float alpha = intensity * (1.0f -
            ((float)band + 0.5f) / kBands);
        drawRect(0.0f, inset, sw, bandWidth, r, g, b, alpha);
        drawRect(0.0f, sh - inset - bandWidth,
                 sw, bandWidth, r, g, b, alpha);
        const float sideY = inset + bandWidth;
        const float sideH = std::max(0.0f, sh - 2.0f * sideY);
        drawRect(inset, sideY, bandWidth, sideH, r, g, b, alpha);
        drawRect(sw - inset - bandWidth, sideY,
                 bandWidth, sideH, r, g, b, alpha);
    }
}

static void DrawGameplayCrosshair(float x, float y) {
    // A hollow cyan reticle. Each stroke carries only a thin keyline in a
    // dark shade of the same cyan, so it reads on bright wallpapers without
    // the black disc that used to sit behind the whole reticle.
    const UiColor3 accent = UiCol::ACCENT_CYAN;
    const float kr = accent.r * 0.22f, kg = accent.g * 0.22f, kb = accent.b * 0.22f;
    const float ka = 0.55f * CombatKeylineStrength() + 0.15f;
    auto ring = [&](float rOuter, float rInner, float r, float g, float b, float a) {
        constexpr int kSegs = 40;
        for (int i = 0; i < kSegs; ++i) {
            const float a0 = 6.2831853f * (float)i / kSegs;
            const float a1 = 6.2831853f * (float)(i + 1) / kSegs;
            const float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            BatchTri(x + c0 * rOuter, y + s0 * rOuter, x + c0 * rInner, y + s0 * rInner,
                     x + c1 * rInner, y + s1 * rInner, r, g, b, a);
            BatchTri(x + c0 * rOuter, y + s0 * rOuter, x + c1 * rInner, y + s1 * rInner,
                     x + c1 * rOuter, y + s1 * rOuter, r, g, b, a);
        }
    };
    auto tick = [&](float rx, float ry, float w, float h) {
        drawRect(rx - 0.8f, ry - 0.8f, w + 1.6f, h + 1.6f, kr, kg, kb, ka);
        drawRect(rx, ry, w, h, accent.r, accent.g, accent.b, 0.96f);
    };
    ring(9.3f, 6.0f, kr, kg, kb, ka);
    ring(8.5f, 6.8f, accent.r, accent.g, accent.b, 0.92f);
    drawCircle(x, y, 2.6f, kr, kg, kb, ka);
    drawCircle(x, y, 1.8f, 0.96f, 0.99f, 1.0f, 1.0f);
    tick(x - 15.0f, y - 1.0f, 6.0f, 2.0f);
    tick(x + 9.0f, y - 1.0f, 6.0f, 2.0f);
    tick(x - 1.0f, y - 15.0f, 2.0f, 6.0f);
    tick(x - 1.0f, y + 9.0f, 2.0f, 6.0f);
}

void SyncPlayerBoundsSize(SpatialBounds& pw, float delta, bool animate) {
    if (g_Stats.windowSize < 64.0f)
        g_Stats.windowSize = std::max(400.0f * g_Scale, 64.0f);
    float targetWin = g_Stats.windowSize;
    if (g_WindowSizeCur < 64.0f) g_WindowSizeCur = g_Stats.windowSize;
    if (animate) {
        float winStep = std::min(1.0f, delta * 5.5f);
        g_WindowSizeCur += (targetWin - g_WindowSizeCur) * winStep;
    } else {
        g_WindowSizeCur = targetWin;
    }
    float pwSz = ClampPlayerWinSize(std::max(g_WindowSizeCur, 64.0f));
    float cx = pw.x + pw.width * 0.5f;
    float cy = pw.y + pw.height * 0.5f;
    if (cx != cx || cy != cy || pw.width < 64.0f || pw.height < 64.0f ||
        pw.width != pw.width || pw.height != pw.height) {
        cx = (float)screenWidth * 0.5f;
        cy = (float)screenHeight * 0.5f;
    }
    pw.width = pw.height = pwSz;
    pw.x = cx - pwSz * 0.5f;
    pw.y = cy - pwSz * 0.5f;
}

static void EnsurePlayerBounds(SpatialBounds& pw) {
    SyncPlayerBoundsSize(pw, 1.0f, false);
}

#if defined(_DEBUG)
static constexpr bool kCompileDebugBuild = true;
#else
static constexpr bool kCompileDebugBuild = false;
#endif

// ?�?�??�티�??�킬 ?�스?????�??기본) + ?�롯 3�?증강 ?�득, �?차면 교체) ?�?�?
// HUD: ?�재 측정 FPS (?�단 ?�측 ?�시)
int    g_CurrentFPS  = 0;
double g_FpsLastTime = 0.0;
int    g_FpsFrames   = 0;

// ���� ���� �ε��� ���?(���� �������? �ߺ� ���� ����)

// ũ������Ƽ�� ���?���� ���� ���� ���� (�ε���). ���� ���� �� �ϰ� ����.

// 증강 ?�택 hover state (-1 = 미선?? 0/1/2 = 카드 ?�덱??
int  g_HoveredAug    = -1;

// 마우???�릭 edge 감�? (?�전 ?�레??left button ?�태)
bool g_LmbPrev = false;
bool g_RmbPrev = false;

// PAUSED ?�태 ??보유 증강 ?�릭 ???�명 ?�시 (-1 = ?�음, 0..AUG_TOTAL-1 = ?�덱??

// ?�정 ?�면 진입 ???�전 ?�태 (?�로 가�???복�?)
GameState g_SettingsReturnTo = GameState::MAIN_MENU;

int g_CurrentWeapon = -1;

// 변??카드 ??AUG_SELECT ??25% ?�률�?4번째 카드 ?�장
// ?�재 무기�??�른 StartWeapon ?�로 ?�환 (기존 무기 ?�과 ?�거 ????무기 ?�용)
// �?= ?�환??StartWeapon ?�덱?? -1 = ?�번 ?�운?�는 변??카드 ?�음

// DYING ?�망 ?�출 ?�태
float g_DyingTimer    = 0.0f;
bool  g_DeathBoomDone = false;
float g_DeathWinW0    = 0.0f;
float g_GameOverFade  = 0.0f;    // 게임?�버 메뉴 ?�이?�인 (0??, ?�?�크�???2.5�?
// ?�망 ?�출 = 즉시 ?�??��(?�티?? ????�� ?�파 ??GAMEOVER (�??�네마틱 ?�음, ?�순)
static const float DYING_DUR      = 0.8f;   // ??�� ?�파가 ?�생?�는 ?�간 (?�이???�까지)
static const float GAMEOVER_FADE  = 2.5f;   // �޴� 100%���� �ɸ��� �ð�

struct DeathParticle {
    float x, y, vx, vy;
    float size;       // ??변 길이(px)
    float r, g, b;    // ?�상
    bool  active = false;
};
static const int MAX_DEBRIS = 48;
DeathParticle g_Debris[MAX_DEBRIS] = {};
float g_DeathCX = 0, g_DeathCY = 0;
float g_DeathFlash = 0.0f; // ??�� ?�광 (1.0 ??0.0)
wchar_t g_DeathReason[96] = {0};   // ?�망 ?�인 ("?�○ ???�해 종료??)

// Short-lived combat effects that live in the world: hit sparks and the
// player's death debris. Drawn over the enemies and bullets.
static void DrawCombatFx() {
    const float debrisFade = std::clamp(g_DyingTimer / DYING_DUR, 0.0f, 1.0f);
    for (int i = 0; i < MAX_DEBRIS && debrisFade > 0.0f; ++i) {
        const DeathParticle& d = g_Debris[i];
        if (!d.active) continue;
        const float hs = d.size * 0.5f;
        drawRect(d.x - hs, d.y - hs, d.size, d.size, d.r, d.g, d.b, debrisFade);
    }
    // Plain alpha blending, not additive: the game sits over the desktop, and
    // added light vanishes on a bright wallpaper.
    for (const auto& sp : g_Sparks) {
        const float k = std::clamp(sp.life / sp.maxLife, 0.0f, 1.0f);
        drawCircle(sp.x, sp.y, sp.size * (0.5f + 0.5f * k), sp.r, sp.g, sp.b, k);
    }
}

// Movement trail: small particles drifting back from the ship, shrinking as they fade.
static void DrawPlayerTrail() {
    for (const auto& tr : g_Trail) {
        const float f = std::clamp(tr.life / tr.maxLife, 0.0f, 1.0f);
        drawCircle(tr.x, tr.y, tr.size * (0.4f + 0.6f * f), tr.r, tr.g, tr.b, 0.75f * f);
    }
}

static SpatialBounds* s_PlayerBoundsRef = nullptr;

// Ends the run as a win. reason holds the KR/EN/JP line shown with the
// final wave number.
static void FinishRunVictory(int finalWave, const wchar_t* const (&reason)[3],
                             const char* telemetryTag) {
    if (g_GameManager.waveVictory || g_GameManager.currentState != GameState::RUNNING)
        return;
    g_GameManager.waveVictory = true;
    g_GameplayTelemetry.EndRun(g_GameTime, g_GameManager.playerLevel,
                               g_GameManager.xp, g_Stats, telemetryTag);
    const int li = std::max(0, std::min(2, LangIndex()));
    (void)finalWave;
    swprintf_s(g_DeathReason, L"%ls", reason[li]);
    Audio::StopBgm();
    g_GameOverFade = 0.0f;
    g_LastRunRecord = CommitRunResult(
        !PlayRecordsProgress(), static_cast<int>(g_Difficulty), g_GameManager.score,
        g_Stats.killCount, (g_SelectedJob == JOB_STATIC_FIELD ? 1 : 0),
        g_GameManager.currentWave);
    // The battle is over: the report sits on an empty field, not on a frozen
    // snapshot of the last wave's mobs, shots and summons.
    g_MonsterManager.Clear();
    g_Bullets.clear();
    g_StardustPickups.clear();
    norma_fx::Clear();
    caelum_fx::Clear();
    g_GameManager.currentState = GameState::GAMEOVER;
}

// Felling the final wave's boss (NORMA or CAELUM) wins the run at once.
static void CompleteBossRun(int finalWave) {
    static const wchar_t* const norma[3] = {
        L"OCTANS 처치", L"OCTANS Defeated", L"OCTANS撃破"
    };
    static const wchar_t* const caelumWin[3] = {
        L"ERIDANUS 처치", L"ERIDANUS Defeated", L"ERIDANUS撃破"
    };
    FinishRunVictory(finalWave, FinalBossKind() == MobKind::CAELUM ? caelumWin : norma, "boss_defeated");
}

// Time-out verdict: starts when the boss wave's clock hits zero with NORMA
// alive, and kills the player (no revive) when the countdown ends.
static void TickNormaJudgment(float dt) {
    bool bossAlive = false;
    for (const Monster* m : g_MonsterManager.monsters)
        if (IsBossMob(m->kind) && m->alive) { bossAlive = true; break; }
    const int wave = g_GameManager.currentWave;
    const bool timeOut = IsFinalBossWave(wave, g_Difficulty) &&
        WaveFightSeconds(wave, g_GameTime) >= BOSS_WAVE_DURATION_SECONDS;
    if (!bossAlive || !timeOut || NormaDefeated()) {
        g_NormaJudgment = -1.0f;
        g_NormaDoom = false;
        return;
    }
    if (g_NormaJudgment < 0.0f) {
        g_NormaJudgment = kNormaJudgmentSeconds;
        g_ShakeTime = 0.5f; g_ShakeMag = 8.0f;
        Audio::Play(Audio::Cue::WaveJudgmentTick);   // 10
        return;
    }
    const float judgmentBefore = g_NormaJudgment;
    g_NormaJudgment -= dt;
    // 남은 초의 정수가 바뀔 때마다 한 번 (9 … 1)
    if (g_NormaJudgment > 0.0f && std::ceil(g_NormaJudgment) < std::ceil(judgmentBefore))
        Audio::Play(Audio::Cue::WaveJudgmentTick);
    if (g_NormaJudgment > 0.0f) return;
    g_NormaJudgment = -1.0f;
    g_NormaDoom = true;
    g_GameManager.playerHP = 0.0f;
    g_ShakeTime = 0.6f; g_ShakeMag = 26.0f;
    TriggerFlash(1.0f, 0.3f, 0.32f, 0.9f);
}

// "ROTOR:3|GRAVIS:1": live mobs by kind, for the balance log's WINDOW lines.
static std::string AliveMobCensus() {
    std::map<std::string, int> count;
    for (const Monster* m : g_MonsterManager.monsters)
        if (m && m->alive) ++count[MobKindName(m->kind)];
    int scopes = 0;
    for (const RangedMob* r : g_MonsterManager.rangedMobs)
        if (r && r->alive) ++scopes;
    if (scopes > 0) count["SCOPE"] = scopes;
    std::string out;
    for (const auto& kv : count) {
        if (!out.empty()) out += '|';
        out += kv.first + ":" + std::to_string(kv.second);
    }
    return out;
}

// Boss rush: a wave ends only when its elite (or mid-run boss) falls. The
// clear queues a reward pick and starts the next wave.
static void ProcessWaveTransitions(int scrW, int scrH) {
    if (g_GameManager.currentState != GameState::RUNNING) return;
    GameManager& gm = g_GameManager;
    const WaveClearInfo clear = ConsumeWaveClear();
    // A regular wave clears on its kill quota; elite and mid-boss waves when
    // their target falls (ConsumeWaveClear).
    gm.waveKills = (int)std::max(0LL, g_Stats.killCount - gm.waveKillStart);
    const int quota = RegularWaveKillQuota(gm.currentWave, g_Difficulty);
    const bool quotaMet = quota > 0 && gm.waveKills >= quota && !g_SmokeNoQuota;
    if (((clear.cleared && clear.wave == gm.currentWave) || quotaMet) &&
        !gm.waveAdvancePending && gm.currentWave < FinalWaveCount(g_Difficulty)) {
        // The faster the clear, the higher the guaranteed tier and the odds.
        const WaveRewardTimes times = quotaMet
            ? RegularWaveRewardTimes(quota) : WaveRewardTimesFor(clear.boss);
        const float seconds = quotaMet ? g_GameTime - gm.waveStartTime : clear.killSeconds;
        gm.rewardTierCap = WaveRewardTierCap(gm.currentWave, g_Difficulty);
        const WaveRewardTier earned = WaveRewardTierFor(seconds, times);
        gm.rewardTier = WaveRewardLuckyBump(WaveRewardGuaranteedTier(earned, gm.rewardTierCap), earned,
                                            (float)std::rand() / ((float)RAND_MAX + 1.0f),
                                            passive::LuckChanceMult(), gm.rewardPityCount);
        gm.rewardSpeed = WaveRewardSpeed(seconds, times);
        gamelog::Event("WAVE_CLEAR", "cleared_wave=%d,clear_sec=%.2f,by=%s,kills=%d,quota=%d,"
                       "earned_tier=%d,reward_tier=%d,tier_cap=%d,reward_speed=%.2f,hp=%.1f",
                       gm.currentWave, seconds, quotaMet ? "quota" : "objective", gm.waveKills, quota,
                       (int)earned, gm.rewardTier, gm.rewardTierCap, gm.rewardSpeed, gm.playerHP);
        gm.QueueAugmentReward();
        passive::OnWaveCleared();
        Audio::Play(Audio::Cue::WaveClear);
        // Wave clear: every drop left on the map flies to the player.
        g_StardustCollectAll = true;
        // The next wave waits until the reward is picked.
        gm.waveAdvancePending = true;
        gm.waveClearDelay = 0.8f;
    }
    // The reward opens a beat after the kill, once nothing else holds the
    // selection screen (a level-up slow-motion may be mid-flight).
    // Not while OCTANS' blue screen is up either (the cards would open hidden under it).
    if (gm.PendingRewards() > 0 && gm.waveClearDelay <= 0.0f && gm.waveTransitionTimer <= 1.2f &&
        !gm.levelUpTransition.Active() && !gm.augmentRewardActive && !CrashScreenUp())
        gm.ActivateNextAugmentReward();
    // Every pick taken: the next wave begins.
    if (gm.waveAdvancePending && gm.waveClearDelay <= 0.0f && gm.PendingRewards() == 0 &&
        !gm.augmentRewardActive && !gm.levelUpTransition.Active()) {
        gm.waveAdvancePending = false;
        g_StardustCollectAll = false;
        gm.waveTransitionFrom = gm.currentWave;
        ++gm.currentWave;
        gm.waveStartTime = g_GameTime;
        gm.waveKillStart = g_Stats.killCount;
        gm.waveKills = 0;
        gm.waveTransitionTimer = 2.0f;
        gamelog::SetClock(g_GameTime, gm.currentWave, gm.playerHP, gm.maxHP);
        gamelog::Event("WAVE_START", "quota=%d,level=%d,score=%lld,hp=%.1f",
                       RegularWaveKillQuota(gm.currentWave, g_Difficulty),
                       gm.playerLevel, (long long)gm.score, gm.playerHP);
    }
    // The run is won only by felling the final boss (CompleteBossRun).
}

// Plain values the skill HUD reads each frame (SkillHudModel.h).
static skill_hud::SkillHudInputs GatherSkillHudInputs() {
    skill_hud::SkillHudInputs in{};
    in.dashCd = g_PlayerRuntime.dashCooldown;
    in.dashCdMax = g_PlayerRuntime.dashCooldownMax;
    in.dashActive = g_PlayerRuntime.dashActive;
    in.dashProgress = g_PlayerRuntime.dashProgress;
    in.dashSealed = passive::DashSealed();
    for (int i = 0; i < 3; ++i) {
        const active::Slot& slot = active::GetSlot(i);
        in.sealed[i] = active::SlotSealed(i);
        in.equipped[i] = slot.aug >= 0;
        in.glyph[i] = slot.aug >= 0
            ? (skill_hud::SkillGlyph)(int)ALL_AUGS[slot.aug].kind
            : skill_hud::SkillGlyph::Projectile;
        in.cd[i] = slot.cd;
        in.cdMax[i] = slot.cdMax;
    }
    return in;
}

enum class LevelUpSource {
    EXPERIENCE,   // XP crossed a threshold in the fixed step
    MANUAL,       // creative/debug F key
};

// Records one level gained (playerLevel/xp already advanced) and queues a
// reward pick.
static void GrantLevelUpReward(int oldLevel, XpAmount need, XpAmount oldXp,
                               LevelUpSource source) {
    const bool manual = source == LevelUpSource::MANUAL;
    g_GameplayTelemetry.RecordLevelUp(g_GameTime, oldLevel,
                                      g_GameManager.playerLevel, need,
                                      oldXp, g_GameManager.xp, manual);
    g_GameManager.QueueStatReward();
    Audio::Play(Audio::Cue::LevelUp);

    // XP level-ups play a short full-speed signal and then open the
    // selection on their own. Extra levels gained meanwhile only queue;
    // they open back to back once the first selection finishes.
    LevelUpTransition& transition = g_GameManager.levelUpTransition;
    if (!manual && g_GameManager.currentState == GameState::RUNNING &&
        !g_GameManager.augmentRewardActive && !transition.Active())
        transition.Begin();
}

// Inputs for GameplayTime.h from the current run state.
static GameplayTimeInputs CurrentGameplayTime() {
    GameplayTimeInputs in;
    in.hitStop = g_HitStopTimer > 0.0f;
    in.dying = g_GameManager.currentState == GameState::DYING;
    return in;
}

// Hits taken while this is true are undone at the end of the fixed step.
static bool IsPlayerDamageProtected() {
    return g_PlayerRuntime.dashInvulnerability > 0.0f ||
           g_PlayerRuntime.postPickGrace > 0.0f ||
           g_GameManager.levelUpTransition.Active();
}

// Every reward pick returns to combat here.
static void ReturnToRunningAfterPick() {
    if (ready_launch::g_TutorialMode) {
        // A tutorial pick goes back to the practice screen, not into combat.
        ready_launch::g_PicksTaken = true;
        g_GameManager.currentState = GameState::READY;
        return;
    }
    g_GameManager.currentState = GameState::RUNNING;
    g_PlayerRuntime.postPickGrace = POST_PICK_GRACE;
}

static void FinishCurrentAugmentReward() {
    if (g_GameManager.augmentRewardActive)
        g_GameManager.ConsumeActiveReward();
    g_GameManager.augmentRewardActive = false;
    if (!g_GameManager.ActivateNextAugmentReward())
        ReturnToRunningAfterPick();
}

static void FinishAugmentEffects() {
    if (g_GameManager.augmentRewardActive) {
        FinishCurrentAugmentReward();
        return;
    }

    ReturnToRunningAfterPick();
}


// The play log is always on: it needs no debug switch, so a normal Release run
// leaves asteria_gameplay_telemetry.txt beside the exe.
static void BeginGameplayTelemetryIfNeeded() {
    if (g_GameplayTelemetry.IsActive()) return;

    const GameState state = g_GameManager.currentState;
    const bool inRunFlow = state == GameState::READY ||
                           state == GameState::RUNNING ||
                           state == GameState::PAUSED ||
                           state == GameState::AUG_SELECT;
    if (!inRunFlow) return;

    // FinalizeLoadout() runs from the scene layer.
    const bool hasLoadout = g_CurrentWeapon >= 0;
    if (!hasLoadout) return;

    g_GameplayTelemetry.BeginRun(
        g_GameTime,
        g_GameManager.playerLevel,
        g_GameManager.xp,
        g_Stats,
        g_CreativeMode ? "creative" : "normal");
}

static void SyncGameplayTelemetry() {
    BeginGameplayTelemetryIfNeeded();
}

// Experiment (Docs/QA/Asteria도파민 시스템 변경 계획.md 3-1): a short hit stop
// when a Genesis or Quasar dies while running. Runtime toggle only (Debug
// Toolkit Stats tab), on at launch and never saved; it does not affect any
// other hit stop. Remove this block, its call and the toolkit binding to
// revert.
static bool g_EliteKillHitStop = true;
// 0.03s alone was not felt, so it is 0.06s.
static constexpr float ELITE_KILL_HIT_STOP = 0.06f;   // boss: revisit once bosses exist

static void TriggerEliteKillHitStop(Monster& m) {
    if (!g_EliteKillHitStop) return;
    if (g_GameManager.currentState != GameState::RUNNING) return;
    if (m.kind != MobKind::GENESIS && m.kind != MobKind::QUASAR) return;
    TriggerHitStop(ELITE_KILL_HIT_STOP);   // longest remaining wins; no stacking
}

// ── BGM 흐름 + 런 상태 효과음 (매 프레임 1회) ──
// 지금 장면·런 순간에 맞는 곡을 고른다 (설정에서 BGM 을 다시 켜도 이 곡이 나옴).
// 보스 등장: 런 곡을 0.5초에 걷어 내고 경고음 → 등장음 → 등장음의 '쾅' 에 보스 곡.
// 그 밖에 저체력 심장 박동 루프, 업적 토스트, 엘리트 등장 경고, 보상 등급 하락.
static int   g_BgmBossKind  = -1;      // 등장 연출을 시작한 보스 (MobKind), -1 = 없음
static float g_BgmBossEntry = -1.0f;   // 보스 등장 경과 초, >= 0 동안 보스 곡 대기
static bool  g_BgmRunEnded  = false;   // 이번 런 종료 징글을 이미 틀었음

static void UpdateBgmFlow(float realDelta) {
    GameManager& gm = g_GameManager;

    // 저체력 심장 박동: 런 전투 중 HP 25% 미만일 때만 (일시정지·사망·메뉴에선 끔)
    const float hpFrac = gm.playerHP / std::max(1.0f, g_Stats.maxHP);
    if (gm.currentState == GameState::RUNNING && vitals_hud::IsDanger(hpFrac))
        Audio::StartLoop(Audio::Cue::SfxLowhpLoop);
    else
        Audio::StopLoop(Audio::Cue::SfxLowhpLoop);
    // 런 밖(사망·결과·메뉴)에서는 남은 루프 효과음을 모두 끔
    if (gm.currentState != GameState::RUNNING && gm.currentState != GameState::PAUSED &&
        gm.currentState != GameState::AUG_SELECT) {
        Audio::StopLoop(Audio::Cue::EnemyGravisLoop);
        Audio::StopLoop(Audio::Cue::SkillVortexLoop);
        Audio::StopLoop(Audio::Cue::SkillChargeLoop);
        Audio::StopLoop(Audio::Cue::SkillBoomerangLoop);
    }

    // 업적 토스트가 새로 뜸 (TryUnlockAch 가 타이머를 4초로 되돌림)
    static float s_achToastSeen = 0.0f;
    if (g_AchToastTimer > s_achToastSeen) Audio::Play(Audio::Cue::MetaAchievement);
    s_achToastSeen = g_AchToastTimer;

    if (gm.currentState == GameState::RUNNING) {
        // 엘리트가 막 들어옴 (웨이브 시작 1.5초 뒤)
        static bool s_eliteSpawned = false;
        const bool eliteSpawned = GetScheduledElite(gm.currentWave, g_Difficulty).spawned;
        if (eliteSpawned && !s_eliteSpawned) Audio::Play(Audio::Cue::WaveEliteWarn);
        s_eliteSpawned = eliteSpawned;
        // 처치 속도 창이 끝나 보상 등급이 한 단계 떨어짐
        static int s_tierWave = 0, s_tier = -1;
        const int wave = gm.currentWave;
        const int quota = RegularWaveKillQuota(wave, g_Difficulty);
        const float fight = WaveFightSeconds(wave, g_GameTime);
        int tier = -1;
        if (!gm.waveAdvancePending && !IsFinalBossWave(wave, g_Difficulty)) {
            if (quota > 0)
                tier = (int)WaveRewardTierFor(g_GameTime - gm.waveStartTime, RegularWaveRewardTimes(quota));
            else if (fight > 0.0f)
                tier = (int)WaveRewardTierFor(fight, IsBossWave(wave, g_Difficulty));
        }
        if (wave == s_tierWave && tier >= 0 && s_tier >= 0 && tier < s_tier)
            Audio::Play(Audio::Cue::WaveTierDrop);
        s_tierWave = wave;
        s_tier = tier;
    }

    // 곡을 고를 장면: 일시정지(와 거기서 연 설정)는 돌아갈 장면의 곡을 이어 간다
    GameState scene = gm.currentState;
    if (scene == GameState::SETTINGS && g_SettingsReturnTo == GameState::PAUSED) scene = GameState::PAUSED;
    if (scene == GameState::PAUSED) scene = gm.pauseResumeState;
    if (scene == GameState::AUG_SELECT && ready_launch::g_TutorialMode) scene = GameState::READY;

    // 런 종료: 사망 연출 동안은 조용히, 결과 화면에서 승리/패배 징글 한 번 (반복 없음)
    if (scene == GameState::DYING || scene == GameState::GAMEOVER) {
        g_BgmBossKind = -1;
        g_BgmBossEntry = -1.0f;
        if (scene == GameState::GAMEOVER && !g_BgmRunEnded) {
            g_BgmRunEnded = true;
            Audio::PlayBgm(gm.waveVictory ? Audio::Bgm::Victory : Audio::Bgm::Defeat, 0);
        }
        return;
    }
    g_BgmRunEnded = false;

    if (scene == GameState::AUG_SELECT) {   // 증강 고르는 화면은 전용 곡
        Audio::PlayBgm(Audio::Bgm::Augment, 600);
        return;
    }
    if (scene != GameState::RUNNING && scene != GameState::AUG_SELECT) {
        g_BgmBossKind = -1;
        g_BgmBossEntry = -1.0f;
        // 튜토리얼·READY 연습장 / 로비·메뉴·상점·도감·설정
        const bool practice = scene == GameState::READY || scene == GameState::TUTORIAL;
        Audio::PlayBgm(practice ? Audio::Bgm::Tutorial : Audio::Bgm::Main, 800);
        return;
    }

    const Monster* boss = nullptr;
    bool eliteAlive = false;
    for (const Monster* m : g_MonsterManager.monsters) {
        if (!m || !m->alive) continue;
        if (IsBossMob(m->kind)) { boss = m; break; }
        if (IsEliteMob(m->kind)) eliteAlive = true;
    }
    if (!boss) {
        g_BgmBossKind = -1;
        g_BgmBossEntry = -1.0f;
        // 엘리트가 살아 있는 동안은 엘리트 곡, 처치하면 전투 곡으로 돌아감
        Audio::PlayBgm(eliteAlive ? Audio::Bgm::Elite : Audio::Bgm::Run, eliteAlive ? 1000 : 1500);
        return;
    }
    const bool eridanus = boss->kind == MobKind::CAELUM;
    if ((int)boss->kind != g_BgmBossKind) {   // 보스가 막 들어옴 (보스 웨이브 시작)
        g_BgmBossKind = (int)boss->kind;
        g_BgmBossEntry = 0.0f;
        Audio::FadeOutBgm(500);
        Audio::Play(Audio::Cue::WaveBossWarn);
        return;
    }
    if (g_BgmBossEntry >= 0.0f) {
        constexpr float kAppearAt = 0.5f;   // 런 곡이 빠진 뒤 등장음
        // 등장음 파일에서 가장 큰 '쾅' 위치 (OCTANS ≈1.75초, ERIDANUS ≈2.25초)
        const float impactAt = kAppearAt + (eridanus ? 2.25f : 1.75f);
        const float before = g_BgmBossEntry;
        if (gm.currentState == GameState::RUNNING) g_BgmBossEntry += realDelta;
        if (before < kAppearAt && g_BgmBossEntry >= kAppearAt)
            Audio::Play(eridanus ? Audio::Cue::BossEridanusAppear : Audio::Cue::BossOctansAppear);
        if (g_BgmBossEntry < impactAt) return;
        g_BgmBossEntry = -1.0f;
    }
    static bool s_crashBefore = false;
    const bool crashJustEnded = s_crashBefore && !g_Crash.active;
    s_crashBefore = g_Crash.active;
    if (g_Crash.active) return;             // 블루스크린 패턴: 곡을 끊고 패턴 소리만
    if (crashJustEnded) {                   // 재부팅 뒤 곡이 천천히 돌아옴
        Audio::PlayBgm(boss->normaPhase == 2 ? (eridanus ? Audio::Bgm::BossEridanusPhase2 : Audio::Bgm::BossOctansPhase2)
                                             : (eridanus ? Audio::Bgm::BossEridanus : Audio::Bgm::BossOctans), 2500);
        return;
    }
    if (g_NormaJudgment >= 0.0f)            // '심판까지' 10초
        Audio::PlayBgm(Audio::Bgm::Judgment, 300);
    else if (boss->normaPhase == 2)         // 이름에 II 가 붙는 2페이즈
        Audio::PlayBgm(eridanus ? Audio::Bgm::BossEridanusPhase2 : Audio::Bgm::BossOctansPhase2, 1500);
    else
        Audio::PlayBgm(eridanus ? Audio::Bgm::BossEridanus : Audio::Bgm::BossOctans, 200);
}

// The resume cue comes from the state change itself, so every way out of the
// pause menu sounds the same. Pausing is silent (by request). Settings opened
// from the pause menu and back is not a resume.
static void PlayStateTransitionAudio() {
    static GameState prev = GameState::MAIN_MENU;
    const GameState cur = g_GameManager.currentState;
    if (cur == prev) return;
    if (prev == GameState::PAUSED && cur == g_GameManager.pauseResumeState)
        Audio::Play(Audio::Cue::Resume);
    prev = cur;
}

// The tutorial's pause menu: PAUSED whose resume target is the practice field.
static bool TutorialPaused() {
    return g_GameManager.currentState == GameState::PAUSED &&
           g_GameManager.pauseResumeState == GameState::READY;
}

// ---- Overlay pieces drawn from the frame loop (kept out of main() body) ----
static void DrawAchievementToast(float sw, float sh, float delta) {
    if (g_AchToastTimer > 0.0f && g_AchToastId >= 0 &&
        g_AchToastId < ACH_COUNT) {
        g_AchToastTimer -= delta;
        float a = g_AchToastTimer > 3.0f ? (4.0f - g_AchToastTimer)
                : std::min(1.0f, g_AchToastTimer);
        if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
        int li2 = LangIndex();
        const wchar_t* LBL[3] = { L"\uC5C5\uC801 \uB2EC\uC131!", L"Achievement!", L"\u5B9F\u7E3E\u9054\u6210!" };
        wchar_t tb[160];
        wchar_t rewardText[32];
        FormatGrouped(ACH_DEFS[g_AchToastId].coinReward, rewardText, 32);
        swprintf_s(tb, L"%ls  %ls  (+%ls)", LBL[li2],
                   AchName(g_AchToastId), rewardText);
        const float toastScale = UiTextScale(g_TextS, UiTextLevel::Description,
                                             UiScale(sw, sh));
        float bw = g_TextS.Width(tb, toastScale) + 48.0f;
        float bx0 = (sw - bw) * 0.5f, by0 = sh * 0.10f;
        BindMainShader();
        drawRect(bx0, by0, bw, 52.0f, 0.12f, 0.10f, 0.02f, 0.85f * a);
        drawRect(bx0, by0, bw, 4.0f, 1.0f, 0.85f, 0.25f, 0.95f * a);
        g_TextS.Draw(tb, bx0 + 24.0f, by0 + 16.0f, toastScale,
                     1.0f, 0.9f, 0.4f, a);
    }
}

static void DrawModeLabels(GameState st, float sw, float sh) {
    // ?�?�??�리?�이?�브 HUD (게임 �? ??F:증강  G:무적 ?�?�?
    if (g_CreativeMode &&
        (st == GameState::RUNNING || st == GameState::READY ||
         st == GameState::AUG_SELECT)) {
        int li3 = LangIndex();
        const wchar_t* CH[3] = {
            L"CREATIVE   F: \uC99D\uAC15   G: \uBB34\uC801",
            L"CREATIVE   F: Card   G: Godmode",
            L"CREATIVE   F: \u5F37\u5316   G: \u30B4\u30C3\u30C9" };
        g_TextS.Draw(CH[li3], 20.0f, HudY(sh, Hud::CREATIVE_LABEL),
                     UiTextScale(g_TextS, UiTextLevel::Supporting, UiScale(sw, sh)),
                     0.7f, 0.85f, 1.0f, 0.85f);
        if (g_CreativeGodmode) {
            const wchar_t* GOD[3] = { L"* \uBB34\uC801 ON", L"* GODMODE ON", L"* \u30B4\u30C3\u30C9 ON" };
            float blink = 0.65f + 0.35f * sinf((float)glfwGetTime() * 5.0f);
            g_TextL.Draw(GOD[li3], 20.0f, 24.0f,
                         UiTextScale(g_TextL, UiTextLevel::Description, UiScale(sw, sh)),
                         1.0f, 0.85f, 0.2f, blink);
        }
    }
#if defined(_DEBUG)
    if (g_DebugMode && !g_CreativeMode &&
        (st == GameState::RUNNING || st == GameState::READY ||
         st == GameState::AUG_SELECT)) {
        int li3 = LangIndex();
        const wchar_t* DBG[3] = {
            L"\uB514\uBC84\uADF8   F: \uB808\uBCA8\uC5C5   G: \uBB34\uC801",
            L"DEBUG   F: LEVEL UP   G: GODMODE",
            L"\u30C7\u30D0\u30C3\u30B0   F: \u30EC\u30D9\u30EB\u30A2\u30C3\u30D7   G: \u7121\u6575"
        };
        g_TextS.Draw(DBG[li3], 20.0f, HudY(sh, Hud::CREATIVE_LABEL),
                     UiTextScale(g_TextS, UiTextLevel::Supporting, UiScale(sw, sh)),
                     0.7f, 0.85f, 1.0f, 0.85f);
        if (g_CreativeGodmode) {
            const wchar_t* GOD[3] = { L"* \uBB34\uC801 ON", L"* GODMODE ON", L"* \u30B4\u30C3\u30C9 ON" };
            float blink = 0.65f + 0.35f * sinf((float)glfwGetTime() * 5.0f);
            g_TextL.Draw(GOD[li3], 20.0f, 24.0f,
                         UiTextScale(g_TextL, UiTextLevel::Description, UiScale(sw, sh)),
                         1.0f, 0.85f, 0.2f, blink);
        }
    }
#endif
}

// Over each damage dummy: its name, DPS over the last 5 s, total and last hit.
static void DrawDummyReadouts(float sw, float sh) {
    for (const Monster* m : g_MonsterManager.monsters) {
        if (!m->alive || !m->trainingDummy) continue;
        const dummy::Log* log = dummy::Find(m);
        wchar_t dps[32] = L"0", total[32] = L"0", last[32] = L"0";
        if (log) {
            FormatGrouped(std::llround(dummy::Dps(*log)), dps, 32);
            FormatGrouped(std::llround(log->total), total, 32);
            FormatGrouped(std::llround(log->last), last, 32);
        }
        wchar_t line1[64], line2[96];
        swprintf(line1, 64, L"허수아비  DPS %ls", dps);
        swprintf(line2, 96, L"누적 %ls  ·  마지막 %ls", total, last);
        const float sc1 = UiTextScale(g_TextS, UiTextLevel::Description, 0.62f * UiScale(sw, sh));
        const float sc2 = UiTextScale(g_TextS, UiTextLevel::Supporting, 0.85f);
        // Under the dummy: the damage numbers float up from it and would cover text above.
        const float sx = W2SX(m->worldX), sy = W2SY(m->worldY) + 26.0f * g_ViewZoom;
        const float w1 = g_TextS.Width(line1, sc1), w2 = g_TextS.Width(line2, sc2);
        const float h1 = g_TextS.Height(line1, sc1);
        g_TextS.Draw(line1, sx - w1 * 0.5f, sy, sc1, 1.0f, 0.86f, 0.40f, 0.95f);
        g_TextS.Draw(line2, sx - w2 * 0.5f, sy + h1 + 4.0f, sc2, 0.80f, 0.88f, 0.96f, 0.85f);
    }
}

static void DrawFloatingCombatText(GameState st, float sw, float sh) {
    if (st == GameState::RUNNING || st == GameState::DYING) {
        DrawDummyReadouts(sw, sh);
        // ?��?지 ?�자 (?�드 ???�크�?변?????�스?? ???�정 ?��?
        for (auto& d : g_DmgNumbers) {
            float t  = d.life / d.maxLife;                   // 1 ??0
            float sx = roundf(W2SX(d.x)), sy = W2SY(d.y);
            wchar_t nb[32]; FormatGrouped(d.amount, nb, 32);
            float sc = UiTextScale(g_TextS, UiTextLevel::Description,
                (d.crit ? 1.05f : 0.72f) * g_ViewZoom
                * (0.7f + 0.3f * t) * UiScale(sw, sh));
            float a  = (t > 0.55f) ? 1.0f : (t / 0.55f);
            float w  = g_TextS.Width(nb, sc);
            if (d.crit) {
                static const LStr kCritLabel = { { L"치명타", L"CRIT", L"クリティカル" } };
                const wchar_t* critLabel = T(kCritLabel);
                const float critSc = UiTextScale(g_TextS, UiTextLevel::Supporting,
                                                 0.80f + 0.20f * t);
                const float critW = g_TextS.Width(critLabel, critSc);
                g_TextS.Draw(critLabel, sx - critW * 0.5f,
                             sy - g_TextS.Height(critLabel, critSc) - 3.0f,
                             critSc, 1.0f, 0.74f, 0.18f, a * 0.92f);
                g_TextS.Draw(nb, sx - w * 0.5f, sy, sc,
                             1.0f, 0.85f, 0.2f, a);
            } else {
                g_TextS.Draw(nb, sx - w*0.5f, sy, sc,
                             1.0f, 1.0f, 1.0f, a*0.9f);
            }
        }
        // a card was taken: its name floats up from the ship
        for (auto& w : g_WorldTexts) {
            const float t = w.life / w.maxLife;
            const float a = t > 0.7f ? 1.0f : t / 0.7f;
            const float sc = UiTextScale(g_TextS, UiTextLevel::Description, 0.95f * g_ViewZoom * UiScale(sw, sh));
            const float tw = g_TextS.Width(w.text, sc);
            g_TextS.Draw(w.text, roundf(W2SX(w.x)) - tw * 0.5f, W2SY(w.y), sc, w.r, w.g, w.b, a);
        }
        // 콤보 카운??(5콤보 ?�상부?? ?�이 콤보???�라 강해�? ???�정 ?��?
        if (g_ShowCombo && st == GameState::RUNNING && g_Combo >= 5) {
            bool  ms      = (g_ComboMilestone > 0.0f);
            float msBoost = ms ? (g_ComboMilestone / 0.7f) : 0.0f;
            wchar_t cb[48], comboText[32]; FormatGrouped(g_Combo, comboText, 32);
            swprintf_s(cb, L"%ls COMBO", comboText);
            float sc = UiTextScale(g_TextS, UiTextLevel::Subtitle,
                (1.0f + g_ComboPulse * 0.4f + msBoost * 0.55f) * UiScale(sw, sh));
            sc *= 1.0f + (g_Combo > 30 ? 0.12f : 0.0f);
            float cr = 1.0f, cg = 1.0f, cbl = 1.0f;
            if      (g_Combo >= 50) { cg = 0.25f; cbl = 0.2f; }
            else if (g_Combo >= 25) { cg = 0.55f; cbl = 0.15f; }
            else if (g_Combo >= 12) { cg = 0.9f;  cbl = 0.3f; }
            if (ms) { cr = 1.0f; cg = 0.85f; cbl = 0.30f; }   // 마일?�톤 = 골드 ?�스
            float w = g_TextS.Width(cb, sc);
            g_TextS.Draw(cb, (sw - w) * 0.5f, sh * 0.115f, sc, cr, cg, cbl, 0.95f);
        }
    }
}

int main() {
    CrashHandler::Install();   // 강종(E23) 추적 ??처리 ?????�외 ??로그+미니?�프
    srand((unsigned)time(NULL));

    // ?�행 ?�일 ?�더�??�업 ?�렉?�리 ?�동 (Resource/ ?��?경로 로드 보장)
    PlatformChdirToExeDir();
    LoadBalanceOverrides();   // balance.cfg / ASTERIA_BALANCE over the shipped balance table
#if defined(_WIN32)
    // One game at a time: two copies would each save over the other's
    // progress. Smoke captures never save, so they may run alongside.
    if (SmokeEnv("ASTERIA_SMOKE").empty()) {
        CreateMutexW(nullptr, TRUE, L"Local\\ASTERIA_SingleInstance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    }
#endif
    LoadGame();   // �����?����/���?�ҷ����� (������ �⺻�� ����)
    if (g_SmokeRun.Init()) os_pane::SetCoverCheck(false);   // ASTERIA_SMOKE 스모크 캡처 세션 설정
    if (g_SmokeCapture && SmokeEnv("ASTERIA_SMOKE_RECORDS") == "1") {
        // Smoke-only PLAY panel records (never saved).
        g_BestWave[0] = 10;
        g_BestWave[1] = 12;
        g_BestWave[2] = 20;
    }
#if defined(__APPLE__)
    // ���� �� ���� 1ȸ: CRT OFF + VFX �淮 (�������� �ٽ� �� �� ����)
    if (!g_MacOptV1) {
        g_MacOptV1 = true;
        g_ShaderFx = false;
        if (g_VfxDensity == VfxDensity::FULL)
            g_VfxDensity = VfxDensity::REDUCED;
        SaveGame();
    }
#endif
    ApplyAccentTheme();   // �����?�׼�Ʈ �׸� �� g_Accent* �ݿ�

    if (!glfwInit()) return -1;
    // Sleep ?�상??1ms �?(FPS �??��??�용). Windows �??��? ?�음
    PlatformTimerBegin();

    GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
    screenWidth  = mode->width;
#ifdef _WIN32
    // One row short of the monitor. A foreground window whose rect covers the
    // whole monitor reads as a full-screen app, and live wallpapers (Wallpaper
    // Engine) pause behind it, so the overlay must not cover the bottom row.
    // That row is covered by a click-eating guard window instead (see
    // CoverEdgeGap in WindowFx.cpp), so it cannot click through to the desktop.
    screenHeight = mode->height - 1;
#else
    screenHeight = mode->height;
#endif
    if (g_SmokeRun.Active() && g_SmokeRun.Width() > 0 && g_SmokeRun.Height() > 0) {
        screenWidth  = g_SmokeRun.Width();
        screenHeight = g_SmokeRun.Height();
    }

#if defined(__APPLE__)
    {
        int wx = 0, wy = 0, ww = 0, wh = 0;
        glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
        if (ww > 0 && wh > 0) {
            screenWidth  = ww;
            screenHeight = wh;
            if (mode->height > wh)
                g_TaskbarH = mode->height - (wy + wh);
        }
    }
#endif

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    // macOS ??3.2+ Core ?�서 forward-compatible 컨텍?�트 ?�수
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    // ?�티??HiDPI) 2× 백버???�기 ???�레?�버??= �??�기(?? 1:1
    //   좌표/?�크issor/마우?��? ?��? ???�위�??�치 ??Windows ?�??�일?�게 ?�작
    //   (???�면 게임???�면 좌하??1/4 ?�만 그려지�??�릭 ?�치가 ?�긋??
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
    glfwWindowHint(GLFW_DECORATED,             GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    // GLFW_FLOATING ?�거: WS_EX_TOPMOST + ?�체?�면 ?�기 조합??DWM ?�립 모드�??�발
    // ??DWM 컴포지???�회 ???�파 ?�명??무효?? TOPMOST ?�이 ?�성 ???�동?�로 ?�정.
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_FALSE);
    glfwWindowHint(GLFW_ALPHA_BITS,            8);
    // Text and thin constellation rules are rendered from glyph textures and
    // line primitives.  Request a modest MSAA surface so the same UI path
    // remains readable on integrated and discrete adapters alike; drivers
    // that cannot provide it simply fall back to the default framebuffer.
    // The game is a full-screen desktop overlay. 2x MSAA keeps thin UI and
    // circular VFX clean while avoiding the 4x full-screen sample cost.
    glfwWindowHint(GLFW_SAMPLES,                2);

    // On Windows the window stops one row above the monitor's bottom edge.
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight,
                                          "Asteria", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

#if defined(__APPLE__)
    {
        int wx = 0, wy = 0, ww = 0, wh = 0;
        glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
        if (ww > 0 && wh > 0)
            glfwSetWindowPos(window, wx, wy);
        else
            glfwSetWindowPos(window, 0, 0);
    }
#else
    glfwSetWindowPos(window, 0, 0);
#endif

    // ?�단 ?�업?�시�??�이 계산 ???�?�크�??�에 ???�는 ?�업?�시줄에 ?�단 UI 가
    //   가?��?지 ?�도�? (?�업 ?�역???�면보다 ?�으�?�?차이가 ?�업?�시�??�이)
#ifdef _WIN32
    // The Shell keeps the taskbar behind our active borderless window.
    g_TaskbarH = 0;
#endif

    // ?�상??기�? ?��??????��? ?�면?�서 �??�티?��? 비�? 축소?�도�?계산 ???�괄 ?�용.
    //   (?��? ?��??�라 ?��? ?�면?�수�??��??�으�?컸던 문제 ?�결 + ?�체?�으�?�?축소)
    // The window keeps its physical size; from here on screenWidth/Height are
    // logical units (see kLogicalH). Smoke runs use the same rule, so a small
    // capture window shows the full-size layout scaled down.
    g_FramebufferW = screenWidth;
    g_FramebufferH = screenHeight;
    {
        const int refH = (g_SmokeRun.Active() && g_SmokeRun.Height() > 0)
            ? screenHeight : mode->height;
        g_PixelScale = refH > 0 ? (float)refH / kLogicalH : 1.0f;
        screenWidth  = (int)std::lround(g_FramebufferW / g_PixelScale);
        screenHeight = (int)std::lround(g_FramebufferH / g_PixelScale);
        g_TaskbarH   = (int)std::lround(g_TaskbarH / g_PixelScale);
    }
    g_Scale = (float)screenHeight / SCALE_REF_H;
    if (g_Scale > 1.0f) g_Scale = 1.0f;
    if (g_Scale < 0.5f) g_Scale = 0.5f;
    glfwMakeContextCurrent(window);
    InputRegisterCallbacks(window);
    glfwSwapInterval((g_FpsCap == 0) ? 1 : 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    GLint msaaSamples = 0;
    glGetIntegerv(GL_SAMPLES, &msaaSamples);
    if (msaaSamples > 0)
        glEnable(GL_MULTISAMPLE);

    // ��Ʈ: exe ��ġ�� ���� Resource ���?��ΰ�?�޶��� �� �־� ���� �����ϴ� ��θ�?������.
    InitializeGameFonts(screenWidth, screenHeight);

    BatchFlush(); glEnable(GL_BLEND);
    // RGB: ?��? ?�파블렌??/ Alpha: ?�레?�버???�파�??�바르게 ?�적
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);

    // ?�이�??�토그램) ?�스�??�이?�라??+ 증강 ?�이�?로드
    InitIconGL();
    LoadIcons();   // ?�환 ?��? 로켓 ?�프?�이???�배�??�거+?�트 가??
    passive::InstallHooks();

    EnableWindowTransparency(window);
    os_pane::Init(window);

    // --- �?GL/?��??�트�?종합 진단 (Windows ?�용) ---
#ifdef _WIN32
    {
        HWND hWnd = glfwGetWin32Window(window);
        // GL 3.3 Core Profile: GL_ALPHA_BITS deprecated ??glGetFramebufferAttachmentParameteriv ?�용
        GLint alphaBits = 0, rBits = 0, gBits = 0, bBits = 0;
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alphaBits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,   &rBits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &gBits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,  &bBits);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        const char* glVer    = (const char*)glGetString(GL_VERSION);
        const char* vendor   = (const char*)glGetString(GL_VENDOR);
        // AMD/ATI 감�? ??AMD ?�라?�버??GLSL 컴파?�이 ???�격?�고 ?�명 FBO 처리가
        //   NVIDIA ?�??�라, 벤더�?로그�??�겨 검?�?�면/?�명?�패 ?�인 추적???�용.
        bool isAMD = false;
        if (vendor) {
            std::string vlow = vendor;
            for (auto& ch : vlow) ch = (char)tolower((unsigned char)ch);
            isAMD = (vlow.find("amd") != std::string::npos) ||
                    (vlow.find("ati") != std::string::npos) ||
                    (vlow.find("advanced micro") != std::string::npos);
        }

        // Windows ?�명???�과 ?�정 (?��??�트�?
        DWORD enableTrans = 1;
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD sz = sizeof(DWORD);
            RegQueryValueExA(hKey, "EnableTransparency", nullptr, nullptr,
                             (LPBYTE)&enableTrans, &sz);
            RegCloseKey(hKey);
        }

        BOOL dwmOn = FALSE;
        DwmIsCompositionEnabled(&dwmOn);
        LONG ex    = GetWindowLong(hWnd, GWL_EXSTYLE);
        int  trans = glfwGetWindowAttrib(window, GLFW_TRANSPARENT_FRAMEBUFFER);

        DBG("=== 종합 진단 ===\n");
        DBG("  [GL] Vendor           : %s%s\n", vendor ? vendor : "NULL",
            isAMD ? "  (AMD 감�? ???�격 GLSL/?�명 FBO 경로)" : "");
        DBG("  [GL] Renderer         : %s\n", renderer ? renderer : "NULL");
        DBG("  [GL] Version          : %s\n", glVer    ? glVer    : "NULL");
        DBG("  [GL] Framebuffer bits : R=%d G=%d B=%d A=%d\n",
            rBits, gBits, bBits, alphaBits);
        DBG("       ??(Core Profile ?�확??쿼리. A=0?�면 ?�라?�버가 ?�파 채널 거�?)\n");
        DBG("\n");
        DBG("  [GLFW] transparent_framebuffer : %d [%s]\n",
            trans, trans ? "SUPPORTED" : "NOT SUPPORTED");
        DBG("  [DWM]  Composition enabled     : %s\n", dwmOn ? "YES" : "NO");
        DBG("  [Win]  GWL_EXSTYLE             : 0x%08lX\n", ex);
        DBG("  [Win]  WS_EX_LAYERED           : %s\n",
            (ex & WS_EX_LAYERED)     ? "SET"           : "NOT SET");
        DBG("  [Win]  WS_EX_TRANSPARENT       : %s\n",
            (ex & WS_EX_TRANSPARENT) ? "SET (?�릭?�과!)" : "NOT SET (?�상)");
        DBG("\n");
        DBG("  [REG]  EnableTransparency      : %lu [%s]\n",
            enableTrans,
            enableTrans ? "ON (?�상)"
                        : "OFF ???�게 문제! ?�정>개인?�정>???�명???�과 켜기");
        DBG("=================\n\n");
    }
#endif // _WIN32 (진단 블록)

    InitMainShaderPipeline(screenWidth, screenHeight);
    InitMainBatchGeometry(screenWidth, screenHeight);
    InitPlanetShader(screenWidth, screenHeight);
    InitNebulaGlowShader(screenWidth, screenHeight);

    // --- 게임 ?�브?�트 초기??---
    SpatialBounds playerWin(
        (screenWidth  - g_Stats.windowSize) * 0.5f,
        (screenHeight - g_Stats.windowSize) * 0.5f,
        g_Stats.windowSize, g_Stats.windowSize);

    const float PLAYER_SIZE = PLAYER_SIZE_PX;
    const float MOVE_SPEED  = PLAYER_MOVE_SPEED;

    g_GameManager.Init(screenWidth, screenHeight);

    // Runtime debug toolkit bindings. The toolkit is inert unless the
    // persisted DEBUG MODE setting is enabled, but its callbacks are wired
    // once here so the normal game loop remains the single owner of state.
    const auto debugSpawnMob = [&](int mobKind, int count) {
        if ((mobKind < 0 || mobKind > (int)MobKind::MAGNETAR) &&
            mobKind != (int)MobKind::LYNX && mobKind != (int)MobKind::MIRA &&
            mobKind != (int)MobKind::TRIANGULUM) return 0;
        int spawned = 0;
        for (int i = 0; i < count; ++i) {
            const size_t before = g_MonsterManager.monsters.size();
            g_MonsterManager.SpawnMob(screenWidth, screenHeight, 160, 1.0f);
            if (g_MonsterManager.monsters.size() <= before) break;
            Monster* mob = g_MonsterManager.monsters.back();
            mob->MakeKind((MobKind)mobKind);
            if (mob->kind == MobKind::REGULUS) {
                const float playerX = playerWin.x + playerWin.width * 0.5f;
                const float playerY = playerWin.y + playerWin.height * 0.5f;
                const float angle = (float)i * 2.39996323f;
                mob->worldX = playerX + std::cos(angle) * 360.0f;
                mob->worldY = playerY + std::sin(angle) * 360.0f;
            }
            ++spawned;
        }
        return spawned;
    };
    g_DebugToolkit.Configure({
        &g_Stats,
        &g_GameManager,
        &g_MonsterManager,
        &playerWin,
        &g_WindowSizeCur,
        screenWidth,
        screenHeight,
        &g_BalanceTestMode,
        [&]() {
            if (!std::isfinite(g_Stats.maxHP) || g_Stats.maxHP < 1.0f)
                g_Stats.maxHP = 1.0f;
            if (!std::isfinite(g_Stats.windowSize) || g_Stats.windowSize < 64.0f)
                g_Stats.windowSize = 64.0f;
            g_GameManager.maxHP = g_Stats.maxHP;
            if (!std::isfinite(g_GameManager.playerHP))
                g_GameManager.playerHP = g_Stats.maxHP;
            g_GameManager.playerHP = std::max(0.0f,
                                               std::min(g_Stats.maxHP,
                                                        g_GameManager.playerHP));
            g_GameManager.scoreAccum = (float)g_GameManager.score;
            const float cx = playerWin.x + playerWin.width * 0.5f;
            const float cy = playerWin.y + playerWin.height * 0.5f;
            playerWin.width  = g_Stats.windowSize;
            playerWin.height = g_Stats.windowSize;
            playerWin.x = cx - playerWin.width * 0.5f;
            playerWin.y = cy - playerWin.height * 0.5f;
            g_WindowSizeCur = g_Stats.windowSize;
        },
        debugSpawnMob,
        [&](int count) {
            int spawned = 0;
            for (int i = 0; i < count; ++i)
            {
                const size_t before = g_MonsterManager.rangedMobs.size();
                g_MonsterManager.SpawnRangedMob(
                    screenWidth, screenHeight, 1.0f, 160,
                    0.0f, 0.0f, -1, -1);
                if (g_MonsterManager.rangedMobs.size() <= before) break;
                ++spawned;
            }
            return spawned;
        },
        &g_EliteKillHitStop,
        &g_PlayConstellationOrbits,
        [&]() {
            if (g_GameManager.currentState != GameState::RUNNING) return;
            // Skip: the wave's target falls now (no score), or a regular
            // wave's quota counts as met, so the wave clears through the
            // normal flow and its reward opens.
            GameManager& gm = g_GameManager;
            const int quota = RegularWaveKillQuota(gm.currentWave, g_Difficulty);
            if (quota > 0) gm.waveKillStart = g_Stats.killCount - quota;
            if (quota > 0 || DebugClearWaveObjective(g_MonsterManager, g_GameTime))
                ProcessWaveTransitions(screenWidth, screenHeight);
        },
        // F1 > Spawn > Boss: brings a boss in as its wave would (replaces F9).
        [&](int boss) {
            if (g_GameManager.currentState != GameState::RUNNING) return 0;
            if (FindNorma()) return 0;
            const float debugHp = GetDifficultyParams(g_Difficulty).enemyHpMultiplier;
            if (boss == 1) {
                // CAELUM, the mid-run boss.
                for (const Monster* cm : g_MonsterManager.monsters)
                    if (cm->kind == MobKind::CAELUM && cm->alive) return 0;
                DebugSpawnCaelum(g_MonsterManager, screenWidth, debugHp, g_Difficulty);
                return 1;
            }
            DebugSpawnNorma(g_MonsterManager, screenWidth, debugHp, g_Difficulty);
            return FindNorma() ? 1 : 0;
        },
        // F1 > Spawn > Boss > NORMA test window (replaces F8).
        [&]() {
            const GameState ps = g_GameManager.currentState;
            if (ps != GameState::RUNNING && ps != GameState::PAUSED) return false;
            g_PaneTestId = os_pane::Open(screenWidth * 0.5f - 260.0f + 60.0f * (g_PaneTestId % 5),
                                         screenHeight * 0.5f - 180.0f + 40.0f * (g_PaneTestId % 5),
                                         520.0f, 360.0f);
            return g_PaneTestId > 0;
        },
        // F1 > Augment: takes the card as a normal pick would (no codex mark).
        [&](int augIdx) {
            if (augIdx < 0 || augIdx >= AUG_TOTAL) return false;
            passive::Acquire(augIdx);
            return true;
        },
        // F1 > Stats > 즉사: dies at once, past godmode and revive cards.
        [&]() { g_DebugKillPlayer = true; },
        // F1 > Spawn > 테스트 > 허수아비: dummies in a column to the right of the ship.
        [&](int count) {
            if (g_GameManager.currentState != GameState::RUNNING) return 0;
            const float pX = playerWin.x + playerWin.width * 0.5f;
            const float pY = playerWin.y + playerWin.height * 0.5f;
            int existing = 0;
            for (const Monster* m : g_MonsterManager.monsters) existing += m->trainingDummy;
            int spawned = 0;
            for (int i = 0; i < count && existing < 12; ++i, ++existing) {
                const size_t before = g_MonsterManager.monsters.size();
                g_MonsterManager.SpawnMob(screenWidth, screenHeight, 400, 1.0f);
                if (g_MonsterManager.monsters.size() <= before) break;
                Monster* mob = g_MonsterManager.monsters.back();
                mob->MakeKind(MobKind::SWARM);
                mob->trainingDummy = true;
                const int col = existing / 4, row = existing % 4;
                mob->dummyHomeX = pX + 260.0f + 200.0f * (float)col;
                mob->dummyHomeY = pY + 160.0f * ((float)row - 1.5f);   // room for the readout under each
                mob->worldX = mob->dummyHomeX; mob->worldY = mob->dummyHomeY;
                dummy::Forget(mob);   // a fresh log, even if the address was used before
                ++spawned;
            }
            return spawned;
        },
        // F1 > Spawn > 테스트 > 허수아비 모두 제거: gone without dying (no kill, no rewards).
        [&]() {
            int removed = 0;
            auto& mobs = g_MonsterManager.monsters;
            for (auto it = mobs.begin(); it != mobs.end();) {
                if (!(*it)->trainingDummy) { ++it; continue; }
                syn::ForgetEnemy(*it);
                dummy::Forget(*it);
                delete *it;
                it = mobs.erase(it);
                ++removed;
            }
            return removed;
        }
    });

    float lastFrame        = 0.0f;
    float fireTimer        = g_Stats.fireInterval;  // ready to fire immediately
    float accumulator      = 0.0f;
    const float FIXED_DT = 1.0f / 60.0f;

    // Smoke runs are silent: with no engine, every Audio call is a no-op, as on a
    // machine without a sound device.
    if (!g_SmokeCapture) Audio::Init(g_AudioMonoOutput);   // Uses the saved stereo/mono output preference.
    g_AudioMonoOutput = Audio::IsMonoOutput();
    Audio::SetBgmVolume(g_BgmVol / 100.0f);
    Audio::SetSfxVolume(g_SfxVol / 100.0f);
    Audio::SetBgmEnabled(g_BgmEnabled && g_BgmVol > 0);
    Audio::SetSfxEnabled(g_SfxEnabled && g_SfxVol > 0);

    // ============================================================
    // 메인 루프
    // ============================================================
    while (!glfwWindowShouldClose(window)) {
        PlayStateTransitionAudio();
        s_PlayerBoundsRef = &playerWin;
        const double nowPrecise = glfwGetTime();   // FPS counter and cap need the full precision
        float now   = (float)nowPrecise;
        float delta = now - lastFrame;
        if (delta > 0.1f) delta = 0.1f;
        const float realDelta = delta;   // panes run on wall time
        if (g_DebugMode) {
            const GameState st = g_GameManager.currentState;
            if (st == GameState::READY || st == GameState::RUNNING ||
                st == GameState::PAUSED || st == GameState::AUG_SELECT ||
                st == GameState::DYING)
                g_PlayDebugTainted = true;
        }
        delta *= g_SmokeTimeScale;   // 1 outside smoke captures
        // After the tutorial launch: the HUD fades in and the ship is briefly safe.
        if (ready_launch::g_RunIntro < 1.0f)
            ready_launch::g_RunIntro = std::min(1.0f,
                ready_launch::g_RunIntro + delta / ready_launch::kRunIntroSec);
        if (ready_launch::g_StartGraceT > 0.0f &&
            g_GameManager.currentState == GameState::RUNNING)
            ready_launch::g_StartGraceT -= delta;
        // Arranging windows mid-fight slows the game down.
        if (os_pane::ArrangeMode() && g_GameManager.currentState == GameState::RUNNING)
            delta *= kArrangeSlow;
        lastFrame = now;
        // Scenes animate on a lightly smoothed frame time: the frame cap's sleep
        // and spin leave +-1-2 ms of jitter that reads as stutter in eased motion.
        // Long frames (hitches) pass through so nothing falls behind.
        float uiDelta = delta;
        {
            static float s_uiDt = 1.0f / 60.0f;
            if (realDelta > 0.0f && realDelta < 0.05f) {
                s_uiDt += (realDelta - s_uiDt) * 0.2f;
                if (delta > 0.0f) uiDelta = delta * (s_uiDt / realDelta);
            } else if (realDelta >= 0.05f) {
                s_uiDt = realDelta;
            }
        }

        // ?�래??추적 브레?�크????마�?�??�태�??�겨 강종(E23) ??로그�??�치 ?�정
        {
            char bc[200];
            std::snprintf(bc, sizeof(bc),
                "st=%d score=%lld lv=%d mobs=%u",
                (int)g_GameManager.currentState,
                (long long)g_GameManager.score,
                g_GameManager.playerLevel,
                (unsigned)g_MonsterManager.monsters.size());
            CrashHandler::SetBreadcrumb(bc);
        }

        // �??�커?��? ?�으�??�른 ?�으�??�환) ?�동 ?�시?��? ??        //   ?�버?�이???�커???�어??루프가 계속 ?��?�? ??막으�?게임??        //   백그?�운?�에??계속 진행??콤보 3�?창이 ?�러가 리셋?�는 버그 ??.
        const InputFocusTransition focus = InputUpdateWindowFocus(window);
        const bool inputFocusChanged = focus.lost || focus.gained;
        if (inputFocusChanged) {
            ResetPlayerInputEdges();
            g_aug1Released = g_aug2Released = g_aug3Released = true;
        }
        if (focus.lost) {
            g_GameManager.confirmPauseReleased = true;
            g_GameManager.escReleased = true;
            // Smoke captures run unattended; a stray focus change must not
            // pause them mid-sequence.
            // Clicking another program while arranging windows is part of
            // the fight, not a reason to pause.
            if (g_GameManager.currentState == GameState::RUNNING && !g_SmokeRun.Active() &&
                !os_pane::ArrangeMode()) {
                g_GameManager.pauseResumeState = GameState::RUNNING;
                g_GameManager.currentState = GameState::PAUSED;
            }
        }
        if (focus.gained) {
            g_GameManager.confirmPauseReleased = true;
            g_GameManager.escReleased = true;
        }
        // Away from the game (Alt-Tab), its panes hide; back, they return
        // under it. Smoke captures run unfocused, so they keep theirs.
        if (!g_SmokeRun.Active()) {
            if (focus.lost) os_pane::SetAway(true);
            if (focus.gained) os_pane::SetAway(false);
        }

        // FPS 측정 (1�??�위)
        // Frames over the real elapsed time: counting "frames until 1 s passed" saw
        // the cap + 1 (61 at 60), since the window always runs a little past 1 s.
        ++g_FpsFrames;
        if (nowPrecise - g_FpsLastTime >= 1.0) {
            g_CurrentFPS  = (int)std::lround((double)g_FpsFrames / (nowPrecise - g_FpsLastTime));
            g_FpsFrames   = 0;
            g_FpsLastTime = nowPrecise;
        }

        InputUpdateTextInput(window);
        glfwPollEvents();
        UpdateWindowTaskbarPolicy(window);
        {   // HDR can be switched, or the window moved to another display, at any time
            static double s_hdrNext = 0.0;
            if (nowPrecise >= s_hdrNext) {
                s_hdrNext = nowPrecise + 2.0;
                g_DesktopHdr = IsWindowOnHdrDisplay(window);
            }
        }
        {
            // Moved windows' in-game rectangles wait for the game to resume.
            const GameState hs = g_GameManager.currentState;
            os_pane::SetHold(hs != GameState::RUNNING && hs != GameState::DYING);
        }
        os_pane::Tick(realDelta);
        // Panes belong to a run: leaving it (game over, menus, a fresh start)
        // closes them all.
        {
            const GameState ps = g_GameManager.currentState;
            if (ps == GameState::GAMEOVER || ps == GameState::READY ||
                ps == GameState::MAIN_MENU || ps == GameState::CREATIVE_CONFIG ||
                ps == GameState::CODEX || ps == GameState::TUTORIAL) {
                os_pane::CloseAll();
                g_Crash = {};   // a blue screen ends with the run
            }
        }
        {
            const float nx = playerWin.x + playerWin.width * 0.5f;
            const float ny = playerWin.y + playerWin.height * 0.5f;
            if (g_GameManager.currentState == GameState::RUNNING && delta > 0.0f) {
                // Short smoothing: a turn shows up within about 0.15 s.
                const float k = std::min(1.0f, delta / 0.15f);
                g_NormaPlayerVX += ((nx - g_NormaPlayerX) / delta - g_NormaPlayerVX) * k;
                g_NormaPlayerVY += ((ny - g_NormaPlayerY) / delta - g_NormaPlayerVY) * k;
                // A teleport (new run, dash reset) is not a velocity.
                const float sp = std::sqrt(g_NormaPlayerVX * g_NormaPlayerVX +
                                           g_NormaPlayerVY * g_NormaPlayerVY);
                if (sp > 1500.0f) {
                    g_NormaPlayerVX *= 1500.0f / sp;
                    g_NormaPlayerVY *= 1500.0f / sp;
                }
            }
            g_NormaPlayerX = nx;
            g_NormaPlayerY = ny;
            if (Monster* boss = FindNorma()) {
                boss->normaPlayerVX = g_NormaPlayerVX;
                boss->normaPlayerVY = g_NormaPlayerVY;
            }
        }
        ServeNormaAsks(playerWin);
        PlayNormaEvents();
        PlayCaelumEvents();
        if (caelum::g.pushX != 0.0f || caelum::g.pushY != 0.0f) {   // CAELUM's wave shoves the player
            ApplyPlayerKnockback(caelum::g.pushX, caelum::g.pushY);
            Audio::Play(Audio::Cue::SfxKnockback);
            caelum::g.pushX = caelum::g.pushY = 0.0f;
        }
        if (g_GameManager.currentState == GameState::RUNNING) {
            TickNormaCountdown(delta);
            TickPaneFuses(delta);
            TickThrowBox(delta);
            TickPaneRam(delta);
            TickPaneWall(delta);
            TickPaneWarp(delta);
            TickPhaseSpray(delta);
            TickCrash(delta);
            DriftNormaPanes(delta, W2SX(playerWin.x + playerWin.width * 0.5f),
                            W2SY(playerWin.y + playerWin.height * 0.5f));
        }
        ApplyPaneAbsorbs();
        RebuildPaneSpaces(playerWin);
        // Hand testing: ASTERIA_PANE_TEST=1 keeps one test pane open during runs.
        {
            static const bool paneHandTest = !SmokeEnv("ASTERIA_PANE_TEST").empty();
            if (paneHandTest && g_GameManager.currentState == GameState::RUNNING) {
                bool anyOpen = false;
                for (const os_pane::Pane& p : os_pane::Panes()) anyOpen = anyOpen || p.open;
                if (!anyOpen)
                    g_PaneTestId = os_pane::Open(screenWidth * 0.5f + 80.0f,
                                                 screenHeight * 0.5f - 180.0f, 520.0f, 360.0f);
            }
        }
        // Hand testing now lives in F1 > Spawn (mobs, elites, bosses, test window).
        // Window arranging is no longer part of play: the boss alone moves its
        // panes. Only smoke captures still force it on.
        os_pane::SetArrangeMode(g_PaneArrangeForce);
        if (inputFocusChanged) {
            g_GameManager.confirmPauseReleased =
                !ActionHeld(GameAction::Confirm) && !ActionHeld(GameAction::Pause);
            g_GameManager.escReleased =
                glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE;
            g_aug1Released = !ActionHeld(GameAction::Card1);
            g_aug2Released = !ActionHeld(GameAction::Card2);
            g_aug3Released = !ActionHeld(GameAction::Card3);
            g_PlayerRuntime.dashInputHeld = ActionHeld(GameAction::Dash);
            active::ResetInputEdges();
        }

        // �?부?�럽�?보간 (?�이�? ?�면 ?�장 ??
        //   ?�투/?�투?�버?�이(RUNNING/DYING/PAUSED/증강·?�버???�택)?�선 �??��? ??        //   ?�시?��? ??줌인?�는 �?부?�연?�럽?�는 ?�드�? �??�제???�작�???        // Restore the base view after leaving the active combat states.
        {
            GameState zs = g_GameManager.currentState;
            bool inFight = (zs == GameState::RUNNING || zs == GameState::DYING ||
                            zs == GameState::PAUSED  || zs == GameState::AUG_SELECT);
            if (!inFight) { g_ViewZoom = g_ViewZoomTarget = 1.0f; }
            else {
                // ?�수 비�? 줌아?????�장???�서???�어�?(?�한 1.4�?= zoom 0.714).
                //   200�� ������ �ִ�ġ ����.
                float t = std::min(1.0f, (float)g_GameManager.score / 2000000.0f);
                g_ViewZoomTarget = 1.0f - 0.286f * t;   // 1.0 �� 0.714
            }
        }
        g_ViewZoom += (g_ViewZoomTarget - g_ViewZoom) * std::min(1.0f, delta * 4.0f);

        // ?�장 ?�레????줌아?�된 만큼 보이???�역???�어지므�??�티??배회/?�폰 ?�역???�장
        //   (?�수 줌아?�·폴리모??줌아??공통 처리)
        {
            float zb = (g_ViewZoom < 0.01f) ? 0.01f : g_ViewZoom;
            g_ArenaExX = (float)screenWidth  * 0.5f * (1.0f / zb - 1.0f);
            g_ArenaExY = (float)screenHeight * 0.5f * (1.0f / zb - 1.0f);
        }

        // 마우???�태 (mx,my = ?�면 ?��? / wmx,wmy = �?보정???�드 좌표 = 조�???
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        mx /= g_PixelScale;   // window pixels to logical units
        my /= g_PixelScale;
        const bool rawLmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool rawRmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (inputFocusChanged) {
            g_LmbPrev = rawLmb;
            g_RmbPrev = rawRmb;
        }
        bool lmb = rawLmb;
        float wmx = ScreenToWorldX((float)mx);
        float wmy = ScreenToWorldY((float)my);

        // --- ?�력 처리 ---
        GameState prevState = g_GameManager.currentState;
        const bool debugInputCaptured = g_DebugToolkit.BeginInput(
            window, (float)screenWidth, (float)screenHeight, mx, my,
            lmb, g_LmbPrev);
        if (!debugInputCaptured) {
            g_GameManager.HandleInput(window);
        } else {
            // Opening the toolkit also pauses simulation and consumes the
            // pointer so gameplay and scene controls cannot leak through.
            delta = 0.0f;
            lmb = false;
            wmx = ScreenToWorldX(-10000.0f);
            wmy = ScreenToWorldY(-10000.0f);
        }

        // Keep the OS pointer out of the combat view while the custom
        // crosshair is active. Pause/settings screens use normal input, so
        // the pointer must remain available for their clickable controls.
        InputUpdateGameplayCursor(window, g_GameManager.currentState,
                                  g_ShowCrosshair, g_DebugToolkit.IsVisible());

        // ??게임 리셋 ?�다 (GAMEOVER ??READY, ?�이???�택 ?? ?�시?�기 버튼 ?�에???�출)
        auto ResetForNewGame = [&]() {
            g_GameplayTelemetry.EndRun(g_GameTime,
                                       g_GameManager.playerLevel,
                                       g_GameManager.xp,
                                       g_Stats,
                                       "reset_or_restart");
            g_Stats        = ResetRunStats(g_Scale);   // weapon/trial: FinalizeLoadout
            g_CreativeGodmode  = false;
            g_CreativeFreeGrab = false;
            g_HoveredAug       = -1;
            g_CurrentWeapon    = -1;
            g_GameTime         = 0.0f;
            g_GameManager.ClearAugmentRewardQueue();
            g_ShotKickTimer = 0.0f;
            g_StaticFieldPulseTimer = 0.0f;
            g_ShakeTime = 0.0f; g_ShakeMag = 0.0f;
            ResetTrials();
            g_RunStardust       = 0;
            g_StardustPickups.clear(); g_StardustHudPulse = 0.0f; g_StardustCollectAll = false;

            ResetJuice();
            norma_fx::Clear();
            caelum_fx::Clear();
            ResetPlayerSkills();   // dash and shield runtime
            passive::Reset();
            g_SkillHudPulse.Reset();
            ResetPlayerInputEdges();
            g_PlayerRuntime.postPickGrace = 0.0f;
            g_WindowSizeCur = g_Stats.windowSize;
            g_WinPrevHP = -1.0f;
            g_PlayerDamagePulse = 0.0f;
            g_HurtVignette = 0.0f; g_HpBarPop = 0.0f;
            g_HpGhost.Reset(1.0f);
            g_ViewZoom = g_ViewZoomTarget = 1.0f;   // �??�복
            g_ZoomCX = g_ZoomCY = 0.0f;
            g_Difficulty = Difficulty::NORMAL;
            // Reset wave spawn timers and scheduled elite state for this run.
            ResetEnemySpawnState();
            g_AntaresFamilies.clear();
            g_DyingTimer      = 0.0f;
            g_DeathBoomDone   = false;
            g_GameOverFade    = 0.0f;
            g_DeathFlash      = 0.0f;
            for (int i = 0; i < MAX_DEBRIS; i++) g_Debris[i].active = false;
            fireTimer = g_Stats.fireInterval;
            playerWin.width  = g_Stats.windowSize;
            playerWin.height = g_Stats.windowSize;
            playerWin.x = (screenWidth  - g_Stats.windowSize) * 0.5f;
            playerWin.y = (screenHeight - g_Stats.windowSize) * 0.5f;
            const long long initialScore = g_CreativeMode
                ? g_CreativeStartScore : 0;
            g_GameManager.ResetRunProgress(g_MonsterManager, g_Bullets,
                                           100.0f, initialScore);
        };

        // Restart the current run without returning to the PLAY screen.
        // Keep the selected loadout and run modifiers, then rebuild the
        // combat state so the next frame starts from a clean arena.
        auto RestartCurrentRun = [&]() {
            const int savedWeapon = g_CurrentWeapon;
            const bool savedGodmode = g_CreativeGodmode;
            const Difficulty savedDifficulty = g_Difficulty;
            const bool savedTrialPoolReady = g_TrialPoolReady;
            int savedTrialPool[TRIAL_SLOT_COUNT] = {};
            bool savedTrialSelected[TRIAL_SLOT_COUNT] = {};
            for (int i = 0; i < TRIAL_SLOT_COUNT; ++i) {
                savedTrialPool[i] = g_TrialPool[i];
                savedTrialSelected[i] = g_TrialSelected[i];
            }

            // This clears enemies, projectiles, drops,
            // timers, run currencies, score/XP and the death presentation.
            ResetForNewGame();

            // Restore the setup that ResetForNewGame intentionally clears.
            g_CurrentWeapon = savedWeapon;
            g_Difficulty = savedDifficulty;
            g_CreativeGodmode = savedGodmode;
            g_TrialPoolReady = savedTrialPoolReady;
            for (int i = 0; i < TRIAL_SLOT_COUNT; ++i) {
                g_TrialPool[i] = savedTrialPool[i];
                g_TrialSelected[i] = savedTrialSelected[i];
            }

            // Rebuild the weapon build the run started with.
            if (g_CurrentWeapon >= 0 && g_CurrentWeapon < (int)StartWeapon::_COUNT)
                ApplyLoadoutStats(g_Stats, g_CurrentWeapon);
            g_GameManager.maxHP = g_Stats.maxHP;
            g_GameManager.playerHP = g_Stats.maxHP;

            g_GameManager.hoveredCard = -1;
            g_AugExitT = -1.0f;
            g_AugExitSlot = -1;
            g_GameManager.ClearAugmentRewardQueue();
            g_GameManager.confirmPauseReleased = true;
            g_GameManager.escReleased = true;
            g_GameManager.pauseResumeState = GameState::RUNNING;
            g_GameManager.currentState = GameState::RUNNING;
            g_GameManager.lastState = GameState::RUNNING;
        };

        // End the current run without rebuilding it. Abandoning from pause or
        // settings should use the same report screen as a normal death while
        // preserving the score, build, level, kills, and earned stardust.
        auto AbandonCurrentRun = [&]() {
            if (g_GameManager.currentState == GameState::GAMEOVER)
                return;

            g_GameplayTelemetry.EndRun(g_GameTime,
                                       g_GameManager.playerLevel,
                                       g_GameManager.xp,
                                       g_Stats,
                                       "abandoned");

            g_MonsterManager.Clear();
            g_Bullets.clear();
            g_StardustPickups.clear(); g_StardustCollectAll = false;
            g_ShotKickTimer = 0.0f;
            g_ShakeTime = 0.0f;
            g_ShakeMag = 0.0f;
            ResetJuice();
            norma_fx::Clear();
            caelum_fx::Clear();

            g_GameManager.playerHP = 0.0f;
            g_GameManager.pauseResumeState = GameState::RUNNING;
            g_DyingTimer = 0.0f;
            g_DeathBoomDone = true;
            g_DeathFlash = 0.0f;
            g_GameOverFade = 0.0f;
            Audio::StopBgm();

            const int li = std::max(0, std::min(2, LangIndex()));
            const wchar_t* abandoned[3] = {
                L"플레이 포기", L"PLAY ABANDONED", L"プレイを放棄"
            };
            wcscpy_s(g_DeathReason, abandoned[li]);

            g_LastRunRecord = CommitRunResult(!PlayRecordsProgress(), (int)g_Difficulty,
                                              g_GameManager.score, g_Stats.killCount,
                                              (g_SelectedJob == JOB_STATIC_FIELD ? 1 : 0),
                                              g_GameManager.currentWave);

            g_GameManager.currentState = GameState::GAMEOVER;
        };

        // Scripted UI smoke capture (ASTERIA_SMOKE). Each step opens a scene
        // through the same entry points the menus use, then the frame is
        // captured once the scene has had time to settle.
        if (g_SmokeRun.Active()) {
            g_SmokeRun.Frame(window, screenWidth, screenHeight, mx, my, lmb,
                [&](const std::string& step) {
                    GameManager& gm = g_GameManager;
                    if (step == "MENU" || step == "PLAY" || step == "SHOP" ||
                        step == "CODEX" || step == "SETTINGS") {
                        const int panel = step == "PLAY" ? 0
                                        : step == "SHOP" ? 1
                                        : step == "CODEX" ? 2
                                        : step == "SETTINGS" ? 3 : -1;
                        gm.currentState = GameState::MAIN_MENU;
                        SmokeOpenMenuPanel(panel);
                    } else if (step == "RUN") {
                        RestartCurrentRun();
                    } else if (step == "READY") {
                        // A fresh run held on the READY screen (practice field).
                        if (gm.currentState != GameState::READY) RestartCurrentRun();
                        gm.currentState = GameState::READY;
                    } else if (step.rfind("CAPTUREKEY=", 0) == 0) {
                        // Settings key capture: feed this GLFW key code as if pressed.
                        if (g_KeyCaptureActive) g_CapturedKey = std::atoi(step.c_str() + 11);
                    } else if (step == "TUTDONE") {
                        g_TutorialDone = true;   // unlock the lobby for captures (not saved in smoke)
                    } else if (step.rfind("COINS=", 0) == 0) {
                        g_Coins = std::atoll(step.c_str() + 6);   // stardust for shop captures
                    } else if (step == "TUTVANISH") {
                        g_TutorialDone = true;   // finished just now: the lobby plays the glitch-out
                        g_TutorialVanishPending = true;
                    } else if (step == "TUTPAUSE") {
                        // The tutorial's pause menu over the practice field (same as ESC there).
                        if (gm.currentState != GameState::READY) RestartCurrentRun();
                        ready_launch::g_TutorialMode = true;
                        gm.pauseResumeState = GameState::READY;
                        gm.currentState = GameState::PAUSED;
                    } else if (step == "LAUNCH") {
                        // READY -> RUNNING launch animation (same as pressing Space).
                        if (gm.currentState != GameState::READY) RestartCurrentRun();
                        gm.currentState = GameState::READY;
                        ready_launch::StartLaunch();
                    } else if (step.rfind("TUTSTEP=", 0) == 0) {
                        // The lobby tutorial with the first N practice objectives done.
                        if (gm.currentState != GameState::READY) RestartCurrentRun();
                        gm.currentState = GameState::READY;
                        ready_launch::g_TutorialMode = true;
                        ready_launch::BeginTutorial();
                        Ready_SmokeCompleteSteps(std::atoi(step.c_str() + 8));
                    } else if (step.rfind("READYSTEP=", 0) == 0) {
                        // READY with the first N practice objectives done.
                        if (gm.currentState != GameState::READY) RestartCurrentRun();
                        gm.currentState = GameState::READY;
                        Ready_SmokeCompleteSteps(std::atoi(step.c_str() + 10));
                    } else if (step == "DEBUG=0") {
                        g_DebugToolkit.SetVisible(false);
                    } else if (step == "DEBUG") {
                        if (gm.currentState != GameState::RUNNING &&
                            gm.currentState != GameState::READY &&
                            gm.currentState != GameState::PAUSED &&
                            gm.currentState != GameState::AUG_SELECT)
                            RestartCurrentRun();
                        g_DebugToolkit.SetVisible(true);
                    } else if (step.rfind("STARDUST_NEAR", 0) == 0) {
                        // Simulate a contact-range loot drop for the smoke
                        // capture without depending on combat input timing.
                        RestartCurrentRun();
                        const float cx = playerWin.x + playerWin.width * 0.5f;
                        const float cy = playerWin.y + playerWin.height * 0.5f;
                        const size_t equals = step.find('=');
                        const XpAmount smokeXp = equals == std::string::npos
                            ? XpFromWhole(1) : XpFromReward(std::atof(step.c_str() + equals + 1));
                        SpawnStardust(cx + 42.0f, cy, 1, cx, cy, smokeXp);
                    } else if (step.rfind("PANE=", 0) == 0) {
                        // NORMA OS-pane prototype:
                        //   PANE=OPEN|OVERPLAYER|LOCK|UNLOCK|CLOSE|MIN|RESTORE|MOVE|ARRANGE|STEAL|CALM
                        //   PANE=SHOT=<name>  save the composed screen as pane_<name>.bmp
                        // Every step appends a line to Screenshots/pane_state.txt.
                        const std::string action = step.substr(5);
                        if (action == "OPEN") {
                            if (gm.currentState != GameState::RUNNING) RestartCurrentRun();
                            g_PaneDebug = true;
                            const float pw = 520.0f, ph = 360.0f;
                            g_PaneTestId = os_pane::Open(screenWidth * 0.5f + 80.0f,
                                                         screenHeight * 0.5f - ph * 0.5f, pw, ph);
                        } else if (action == "OVERPLAYER") {
                            // Title bar straight through the player at screen centre.
                            if (gm.currentState != GameState::RUNNING) RestartCurrentRun();
                            g_PaneDebug = true;
                            g_PaneTestId = os_pane::Open(screenWidth * 0.5f - 260.0f,
                                                         screenHeight * 0.5f - 14.0f, 520.0f, 360.0f);
                        } else if (action == "LOCK") {
                            os_pane::SetLocked(g_PaneTestId, true);
                        } else if (action == "UNLOCK") {
                            os_pane::SetLocked(g_PaneTestId, false);
                        } else if (action == "CLOSE") {
                            os_pane::DebugRequestClose(g_PaneTestId);
                        } else if (action == "MIN") {
                            os_pane::DebugRequestMinimize(g_PaneTestId);
                        } else if (action == "RESTORE") {
                            os_pane::DebugRestore(g_PaneTestId);
                        } else if (action == "MOVE") {
                            os_pane::DebugMoveBy(g_PaneTestId, -360.0f, 120.0f);
                        } else if (action == "ARRANGE") {
                            g_PaneArrangeForce = true;
                        } else if (action == "STEAL") {
                            os_pane::DebugActivateProbe();
                        } else if (action == "CALM") {
                            g_PaneArrangeForce = false;
                        } else if (action.rfind("PROBE=", 0) == 0) {
                            // PANE=PROBE=x,y  who receives a click at that overlay point
                            float px = 0.0f, py = 0.0f;
#ifdef _MSC_VER
                            sscanf_s(action.c_str() + 6, "%f,%f", &px, &py);
#else
                            std::sscanf(action.c_str() + 6, "%f,%f", &px, &py);
#endif
                            bool overlayHit = false;
                            const unsigned long pid = os_pane::DebugProcessAt(px, py, &overlayHit);
                            std::filesystem::create_directories("Screenshots");
                            FILE* pf = nullptr;
#ifdef _MSC_VER
                            if (fopen_s(&pf, "Screenshots/pane_state.txt", "a") != 0) pf = nullptr;
#else
                            pf = std::fopen("Screenshots/pane_state.txt", "a");
#endif
                            if (pf) {
                                std::fprintf(pf, "PROBE %.0f,%.0f pid=%lu overlay=%d exstyle=0x%08lX\n",
                                             px, py, pid, overlayHit ? 1 : 0,
                                             os_pane::DebugOverlayExStyle());
                                std::fclose(pf);
                            }
                        } else if (action.rfind("SHOT=", 0) == 0) {
                            const std::string name = action.substr(5);
                            const std::wstring path = L"Screenshots/pane_" +
                                std::wstring(name.begin(), name.end()) + L".bmp";
                            os_pane::DebugCaptureScreen(path.c_str());
                        }
                        DumpPaneState(action.c_str(), window);
                    } else if (step == "PAUSED") {
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        gm.pauseResumeState = GameState::RUNNING;
                        gm.currentState = GameState::PAUSED;
                    } else if (step == "GAMEOVER") {
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        AbandonCurrentRun();
                    } else if (step.rfind("REG04", 0) == 0) {
                        // REG-04 (scripts/test_reg04_run_reset.ps1): fill every
                        // piece of run state, run one reset path, and append a
                        // snapshot line to Screenshots/reg04_state.txt.
                        //   REG04=NEWRUN|RESTART|ABANDON  fill, run, dump
                        //   REG04=READY   fill, then enter READY; when the frame
                        //                 started in GAMEOVER the frame's own
                        //                 GAMEOVER->READY handling resets
                        //   REG04DUMP=<label>  dump only
                        auto fill = [&]() {
                            g_PlayerRuntime.dashCooldown = 1.5f;
                            g_PlayerShield = 25.0f; g_PlayerShieldTimer = 4.0f;
                            g_PlayerRuntime.dashInputHeld = true;
                            g_PlayerRuntime.postPickGrace = 0.4f;
                            for (int i = 0; i < 5; ++i)
                                g_Bullets.push_back(Bullet(0.0f, 0.0f, 10.0f, 0.0f));
                            for (int i = 0; i < 4; ++i)
                                g_MonsterManager.monsters.push_back(new Monster(-5000.0f, -5000.0f, 1.0f));
                            g_MonsterManager.rangedMobs.push_back(
                                new RangedMob(-5000.0f, -5000.0f, screenWidth, screenHeight));
                            for (int i = 0; i < 6; ++i) g_StardustPickups.push_back(StardustPickup{});
                            for (int i = 0; i < 3; ++i) g_Debris[i].active = true;
                            gm.score = 4321; gm.scoreAccum = 4321.0f; gm.xp = XpFromWhole(55); gm.playerLevel = 3;
                            gm.playerHP = 42.0f;
                            g_Stats.killCount = 777;
                            g_CurrentWeapon = (int)StartWeapon::SMG;
                            g_GameTime = 99.0f;
                            gm.currentWave = 1;
                            gm.waveStartTime = g_GameTime;
                            gm.waveKillStart = g_Stats.killCount;
                        };
                        auto dump = [&](const std::string& label) {
                            int debris = 0, tier3 = 0, tier4 = 0;
                            for (const auto& p : g_Debris) debris += p.active;
                            for (const Monster* mob : g_MonsterManager.monsters) {
                                if (!mob->alive) continue;
                                tier3 += mob->kind == MobKind::GRAVIS ||
                                         mob->kind == MobKind::REGULUS ||
                                         mob->kind == MobKind::MAGNETAR;
                                tier4 += mob->kind == MobKind::GENESIS ||
                                         mob->kind == MobKind::QUASAR;
                            }
                            std::filesystem::create_directories("Screenshots");
                            FILE* f = nullptr;
#ifdef _MSC_VER
                            if (fopen_s(&f, "Screenshots/reg04_state.txt", "a") != 0) f = nullptr;
#else
                            f = std::fopen("Screenshots/reg04_state.txt", "a");
#endif
                            if (!f) return;
                            std::fprintf(f,
                                "%s state=%d "
                                "dashCd=%.2f shield=%.1f dashHeld=%d grace=%.2f "
                                "bullets=%d monsters=%d ranged=%d stardust=%d debris=%d "
                                "score=%lld xp=%.3f level=%d hp=%.1f "
                                "kills=%lld weapon=%d time=%.1f rewards=%d levelup=%d "
                                "wave=%d waveVictory=%d tier3=%d tier4=%d "
                                "damage=%.5f choice0=%s\n",
                                label.c_str(), (int)gm.currentState,
                                g_PlayerRuntime.dashCooldown, g_PlayerShield,
                                (int)g_PlayerRuntime.dashInputHeld,
                                g_PlayerRuntime.postPickGrace, (int)g_Bullets.size(),
                                (int)g_MonsterManager.monsters.size(),
                                (int)g_MonsterManager.rangedMobs.size(),
                                (int)g_StardustPickups.size(), debris,
                                gm.score, XpToDouble(gm.xp), gm.playerLevel, gm.playerHP,
                                g_Stats.killCount, g_CurrentWeapon, g_GameTime,
                                gm.PendingRewards(),
                                (int)gm.levelUpTransition.Active(),
                                gm.currentWave,
                                (int)gm.waveVictory, tier3, tier4,
                                g_Stats.damageMultiplier,
                                gm.augChoiceCount > 0 && gm.augChoices[0] >= 0 && gm.augChoices[0] < AUG_TOTAL
                                    ? ALL_AUGS[gm.augChoices[0]].name : "NONE");
                            std::fclose(f);
                        };
                        const size_t eq = step.find('=');
                        const std::string arg = eq == std::string::npos ? "" : step.substr(eq + 1);
                        if (step.rfind("REG04DUMP=", 0) == 0) {
                            dump(arg);
                        } else if (arg == "READY") {
                            fill();
                            gm.currentState = GameState::READY;
                        } else {
                            fill();
                            if (arg == "NEWRUN") ResetForNewGame();
                            else if (arg == "RESTART") RestartCurrentRun();
                            else if (arg == "ABANDON") AbandonCurrentRun();
                            dump(arg);
                        }
                    } else if (step == "AUGSEL") {
                        // AUGSEL: opens the reward pick with its card
                        // keyboard-focused so the detail panel is visible in
                        // the capture.
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        gm.QueueAugmentReward();
                        gm.ActivateNextAugmentReward();
                        g_SmokeAugFocus = 0;
                    } else if (step.rfind("AUGGLOSS=", 0) == 0) {
                        // AUGGLOSS=<levels>: AUGSEL, then hover the first card's
                        // first glossary word (and its related words, per level).
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        gm.QueueAugmentReward();
                        gm.ActivateNextAugmentReward();
                        g_SmokeAugFocus = 0;
                        g_SmokeGlossDepth = std::max(1, std::atoi(step.c_str() + 9));
                    } else if (step == "AUGDEAL") {
                        // AUGDEAL: opens the reward pick and lets the deal play (dim fade-in).
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        gm.QueueAugmentReward();
                        gm.ActivateNextAugmentReward();
                    } else if (step == "STATDEAL") {
                        // STATDEAL: opens a level-up stat pick (three cards).
                        if (gm.currentState != GameState::RUNNING)
                            RestartCurrentRun();
                        gm.QueueStatReward();
                        gm.ActivateNextAugmentReward();
                    } else if (step.rfind("AUGPICK=", 0) == 0) {
                        // AUGPICK=<slot>: confirm a card on the open pick (the exit animation).
                        if (gm.currentState == GameState::AUG_SELECT && g_AugExitT < 0.0f) {
                            g_HoveredAug = std::clamp(std::atoi(step.c_str() + 8), 0,
                                                      std::max(0, gm.augChoiceCount - 1));
                            g_AugExitT = 0.0f;
                            g_AugExitSlot = g_HoveredAug;
                        }
                    } else if (step.rfind("GOD=", 0) == 0) {
                        g_SmokeGod = step.size() > 4 && step[4] == '1';
                    } else if (step.rfind("NOQUOTA=", 0) == 0) {
                        // NOQUOTA=1: mass kills do not end the wave (no card screen mid-test).
                        g_SmokeNoQuota = step.size() > 8 && step[8] == '1';
                    } else if (step.rfind("WALK=", 0) == 0) {
                        g_SmokeWalk = step.size() > 5 && step[5] != '0' ? step[5] : 0;
                    } else if (step.rfind("FIRE=", 0) == 0) {
                        g_SmokeFire = step.size() > 5 && step[5] == '1';
                    } else if (step.rfind("GIVE=", 0) == 0) {
                        // GIVE=<AugDef::name>|ALL: take cards as if picked (ALL:
                        // every augment card, not the level-up stat cards).
                        const std::string want = step.substr(5);
                        for (int i = 0; i < AUG_TOTAL; ++i)
                            if (want == "ALL" ? !AugIsStatCard(ALL_AUGS[i])
                                              : want == ALL_AUGS[i].name)
                                passive::Acquire(i);
                    }
                    if (step.rfind("LEVEL=", 0) == 0) {
                        gm.playerLevel = std::max(1, std::atoi(step.c_str() + 6));
                    } else if (step.rfind("WAVE=", 0) == 0) {
                        // HUD-only smoke fixture: jump to a wave without replaying combat.
                        gm.currentWave = std::clamp(std::atoi(step.c_str() + 5),
                                                    1, FinalWaveCount(g_Difficulty));
                        gm.waveStartTime = g_GameTime;
                        gm.waveKillStart = g_Stats.killCount;
                    } else if (step.rfind("XP=", 0) == 0) {
                        // XP=<n>: one large XP gain, levelled by the next tick.
                        GrantExperience(XpFromReward(std::atof(step.c_str() + 3)));
                    } else if (step.rfind("DUST=", 0) == 0) {
                        // DUST=<n>: stardust picked up this run (wallet too,
                        // as a real pickup does). Never saved.
                        const long long n = std::atoll(step.c_str() + 5);
                        AddSaveValue(g_RunStardust, n);
                        AddSaveValue(g_Coins, n);
                    } else if (step.rfind("COINS=", 0) == 0) {
                        // COINS=<n>: wallet for shop states (never saved).
                        g_Coins = std::atoll(step.c_str() + 6);
                    } else if (step.rfind("WHEEL=", 0) == 0) {
                        // WHEEL=<notches>: same sign as the GLFW wheel
                        // callback (yoffset); negative scrolls down.
                        g_ScrollAccum += (float)std::atof(step.c_str() + 6);
                    } else if (step == "HURT") {
                        // Same timers a real hit sets (red edge + HP pop).
                        g_HurtVignette = 0.5f;
                        g_HpBarPop = 0.35f;
                    } else if (step.rfind("SLOW=", 0) == 0) {
                        // SLOW=x: scale game time (e.g. 0.2) to capture fast
                        // animations frame by frame.
                        g_SmokeTimeScale = std::clamp((float)atof(step.c_str() + 5), 0.02f, 1.0f);
                    } else if (step.rfind("GENESISROTOR=", 0) == 0 || step.rfind("GENESISCYCLE=",0)==0) {
                        const bool fullCycle=step.rfind("GENESISCYCLE=",0)==0;
                        if (gm.currentState != GameState::RUNNING) RestartCurrentRun();
                        g_MonsterManager.Clear();
                        g_Bullets.clear();
                        std::srand(1234);
                        const float px = screenWidth * 0.58f, py = screenHeight * 0.57f;
                        playerWin.x = px - playerWin.width*0.5f;
                        playerWin.y = py - playerWin.height*0.5f;
                        auto* g = new Monster(screenWidth*0.30f,screenHeight*0.35f);
                        g->MakeKind(MobKind::GENESIS);
                        g->hp = 1000000.0f; g->speed = 0.0f;
                        g->genesisEntry = Monster::GENESIS_ENTRY_TIME;   // replay the cycle, not the entrance
                        g->genesisNextRotor = !fullCycle;
                        g->hiveOrbitAngle=atan2f(py-g->worldY,px-g->worldX);
                        g->spawnTimer = Monster::GENESIS_TRACK_TIME;
                        g_MonsterManager.monsters.push_back(g);
                        const glm::vec4 bounds(0,BROWSER_CHROME_H,screenWidth,
                                               screenHeight-g_GameBarH-g_TaskbarH);
                        float qaHp = 10000.0f;
                        g_MonsterManager.UpdateAll(px,py,0.0f,qaHp,g_Bullets,1,1,1,-1,-1,-1,-1,bounds);
                        const float age = std::clamp(std::strtof(step.c_str()+13,nullptr),0.0f,fullCycle ? 10.0f : 3.8f);
                        for (float t=0;t<age;) {
                            const float dt = std::min(1.0f/120.0f,age-t); t += dt;
                            g_MonsterManager.UpdateAll(px+sinf(t*2.0f)*100.0f,py+sinf(t*1.5f)*80.0f,
                                                      dt,qaHp,g_Bullets,1,1,1,-1,-1,-1,-1,bounds);
                        }
                        playerWin.x+=sinf(age*2.0f)*100.0f;
                        playerWin.y+=sinf(age*1.5f)*80.0f;
                        g_SmokeTimeScale = 0.0f;
                    } else if (step.rfind("QUASARFALL=", 0) == 0 || step.rfind("QUASARBEAM=", 0) == 0 ||
                               step.rfind("QUASARERUPT=", 0) == 0) {
                        // Deterministic motion QA for both charge animations.
                        if (gm.currentState != GameState::RUNNING) RestartCurrentRun();
                        g_MonsterManager.Clear();
                        playerWin.x = screenWidth * 0.61f - playerWin.width * 0.5f;
                        playerWin.y = screenHeight * 0.64f - playerWin.height * 0.5f;
                        std::srand(1234); // repeatable motion replay, before random initial angles
                        auto* q = new Monster(screenWidth * 0.30f, screenHeight * 0.42f);
                        q->MakeKind(MobKind::QUASAR);
                        q->hp = 1000000.0f;
                        q->speed = 0.0f;
                        // ASTERIA_QREFLECT=<n>: beam wall bounces for this QA quasar.
                        { const std::string qr = SmokeEnv("ASTERIA_QREFLECT"); if (!qr.empty()) q->quasarReflections = std::atoi(qr.c_str()); }
                        const bool erupt = step.rfind("QUASARERUPT=", 0) == 0;
                        q->quasarBounds = glm::vec4(0, BROWSER_CHROME_H, screenWidth,
                                                    screenHeight - g_GameBarH - g_TaskbarH);
                        if (erupt) q->BeginQuasarEruption();
                        else if (step.rfind("QUASARFALL=", 0) == 0) q->BeginQuasarFall();
                        else q->quasarTimer = Monster::QUASAR_COOLDOWN_TIME - Monster::QUASAR_PRECHARGE_TIME;
                        const float age = std::clamp(std::strtof(step.c_str() + (erupt ? 12 : 11), nullptr),
                                                    0.0f, 6.35f);
                        float qaHp = 10000.0f;
                        const glm::vec4 bounds(0, BROWSER_CHROME_H, screenWidth,
                                               screenHeight - g_GameBarH - g_TaskbarH);
                        for (float t = 0.0f; t < age; ) {
                            const float dt = std::min(1.0f / 120.0f, age - t);
                            t += dt;
                            q->Update(screenWidth * 0.61f + sinf(t * 1.5f) * 90.0f,
                                      screenHeight * 0.64f + cosf(t * 1.5f) * 55.0f,
                                      dt, qaHp, 1.0f, -1, -1, -1, -1, nullptr, bounds);
                        }
                        playerWin.x += sinf(age * 1.5f) * 90.0f;
                        playerWin.y += cosf(age * 1.5f) * 55.0f;
                        g_MonsterManager.monsters.push_back(q);
                        g_SmokeTimeScale = 0.02f;
                    } else if (step == "TRIAL10") {
                        // TRIAL10: the max HP trial is on, to see trial values in the report.
                        g_TrialPool[0] = 10;
                        g_TrialSelected[0] = true;
                    } else if (step == "DEATH") {
                        // DEATH: the player dies now through the normal
                        // HP <= 0 path, so the game-over report is a real one.
                        gm.playerHP = 0.0f;
                    } else if (step == "KILL") {
                        // KILL: every living enemy dies now through the normal
                        // death pass (rewards, death animations).
                        for (auto* mob : g_MonsterManager.monsters)
                            if (mob->alive) { mob->hp = 0.0f; mob->alive = false; }
                    } else if (step == "SHIELD") {
                        // SHIELD: give every living non-Regulus enemy a full
                        // Regulus shield, to check the shield mark on each kind.
                        auto grant = [](EnemyShieldState& shield) {
                            shield = {};
                            shield.acquiredOnce = true;
                            shield.hp = shield.maxHp = 200.0f;
                        };
                        for (auto* mob : g_MonsterManager.monsters)
                            if (mob->alive && mob->kind != MobKind::REGULUS)
                                grant(mob->regulusShield);
                        for (auto* mob : g_MonsterManager.rangedMobs)
                            if (mob->alive) grant(mob->regulusShield);
                    } else if (step == "MOBCLEAR") {
                        // MOBCLEAR: drop every mob and bullet so a MOBDUMP run
                        // only sees the mobs it spawns itself (the wave's own
                        // elite stays out too).
                        g_MonsterManager.Clear();
                        DebugHoldWave(gm.currentWave);
                        g_Bullets.clear();
                    } else if (step.rfind("MOBPLACE=", 0) == 0) {
                        // MOBPLACE=<dx>: put every live Gravis / Regulus / Antares / Quasar dx px
                        // right of the player, on the player's row.
                        const float dx = std::strtof(step.c_str() + 9, nullptr);
                        for (Monster* mob : g_MonsterManager.monsters) {
                            if (!mob->alive) continue;
                            if (mob->kind != MobKind::GRAVIS && mob->kind != MobKind::REGULUS &&
                                mob->kind != MobKind::ANTARES && mob->kind != MobKind::QUASAR &&
                                mob->kind != MobKind::LYNX) continue;
                            mob->worldX = playerWin.x + playerWin.width * 0.5f + dx;
                            mob->worldY = playerWin.y + playerWin.height * 0.5f;
                        }
                    } else if (step == "MOBGRID") {
                        // MOBGRID: lay every live non-Regulus mob out in two rows
                        // around the player (buff-mark overlap checks).
                        const float cx = playerWin.x + playerWin.width * 0.5f;
                        const float cy = playerWin.y + playerWin.height * 0.5f;
                        int i = 0;
                        const int total = (int)g_MonsterManager.monsters.size() +
                                          (int)g_MonsterManager.rangedMobs.size();
                        const int perRow = (total + 1) / 2;
                        const auto place = [&](float& x, float& y) {
                            const int row = i / perRow, col = i % perRow;
                            x = cx + ((float)col - (perRow - 1) * 0.5f) * 200.0f;
                            y = cy + (row == 0 ? -150.0f : 150.0f);
                            ++i;
                        };
                        for (Monster* mob : g_MonsterManager.monsters) place(mob->worldX, mob->worldY);
                        for (RangedMob* mob : g_MonsterManager.rangedMobs) place(mob->worldX, mob->worldY);
                    } else if (step.rfind("MOBBUFF=", 0) == 0) {
                        // MOBBUFF=<mask>: 1 shield, 2 speed, 4 stealth on every
                        // live mob (Scope only takes the shield); 0 clears them.
                        const int mask = std::atoi(step.c_str() + 8);
                        for (Monster* mob : g_MonsterManager.monsters) {
                            if (!mob->alive) continue;
                            mob->regulusSpeedAge = (mask & 2) ? 5.0f : -1.0f;
                            mob->regulusStealthAge = (mask & 4) ? 5.0f : -1.0f;
                            mob->regulusShield = {};
                            if (mask & 1) {
                                mob->regulusShield.acquiredOnce = true;
                                mob->regulusShield.hp = mob->regulusShield.maxHp = 200.0f;
                            }
                        }
                        for (RangedMob* mob : g_MonsterManager.rangedMobs) {
                            mob->regulusShield = {};
                            if (mask & 1) {
                                mob->regulusShield.acquiredOnce = true;
                                mob->regulusShield.hp = mob->regulusShield.maxHp = 200.0f;
                            }
                        }
                    } else if (step == "GENESISRANDOM") {
                        // GENESISRANDOM: every live Genesis summons random regular enemies (as on NIGHTMARE).
                        for (Monster* mob : g_MonsterManager.monsters)
                            if (mob->alive && mob->kind == MobKind::GENESIS) mob->genesisRandomSummons = true;
                    } else if (step.rfind("MOBHP=", 0) == 0) {
                        // MOBHP=<n>: set every live mob's HP, so an elite under test is not
                        // killed by the player's auto weapons halfway through a pattern.
                        const float hpValue = std::strtof(step.c_str() + 6, nullptr);
                        for (Monster* mob : g_MonsterManager.monsters) if (mob->alive) mob->hp = hpValue;
                    } else if (step == "MOBGRACE") {
                        // MOBGRACE: mobs pass through the (stationary) player instead
                        // of ramming, so a charge can be watched to its end.
                        for (Monster* mob : g_MonsterManager.monsters) mob->singularityGrace = 1.0e9f;
                    } else if (step == "MOBKILL") {
                        // MOBKILL: kill every live mob through the normal damage path.
                        for (Monster* mob : g_MonsterManager.monsters)
                            if (mob->alive) mob->ApplyDamage(mob->hp);
                    } else if (step.rfind("MOBDUMP=", 0) == 0) {
                        // MOBDUMP=<label>: append one line per live Gravis /
                        // Regulus plus the player to Screenshots/mob_state.txt
                        // (read by scripts/test_gravis_regulus_smoke.ps1).
                        std::filesystem::create_directories("Screenshots");
                        FILE* mf = nullptr;
#ifdef _MSC_VER
                        if (fopen_s(&mf, "Screenshots/mob_state.txt", "a") != 0) mf = nullptr;
#else
                        mf = std::fopen("Screenshots/mob_state.txt", "a");
#endif
                        if (mf) {
                            const char* label = step.c_str() + 8;
                            std::fprintf(mf, "%s player x=%.1f y=%.1f hp=%.2f influence=%.3f time=%.3f\n",
                                label, playerWin.x + playerWin.width * 0.5f,
                                playerWin.y + playerWin.height * 0.5f, gm.playerHP,
                                GravisField::g_playerInfluence, (double)glfwGetTime());
                            for (const RangedMob* sc : g_MonsterManager.rangedMobs)
                                if (sc->alive && sc->genesisSummoned)
                                    std::fprintf(mf, "%s summonedscope x=%.1f y=%.1f rgb=%.2f,%.2f,%.2f owner=%d hp=%.0f\n", label,
                                        sc->worldX, sc->worldY, sc->color.r, sc->color.g, sc->color.b,
                                        sc->summonOwnerId, sc->hp);
                            for (const Monster* mob : g_MonsterManager.monsters) {
                                if (!mob->alive) continue;
                                if (mob->kind == MobKind::GRAVIS)
                                    std::fprintf(mf, "%s gravis x=%.1f y=%.1f elite=%d\n", label,
                                        mob->worldX, mob->worldY, (int)IsEliteMob(mob->kind));
                                else if (mob->kind == MobKind::ANTARES)
                                    std::fprintf(mf, "%s antares x=%.1f y=%.1f state=%d speed=%.0f bounces=%d gen=%d elite=%d hp=%.0f\n",
                                        label, mob->worldX, mob->worldY, (int)mob->antaresState,
                                        mob->antaresMoveSpeed, mob->antaresBounces, mob->antaresGen,
                                        (int)IsEliteMob(mob->kind), mob->hp);
                                else if (mob->kind == MobKind::QUASAR)
                                    std::fprintf(mf, "%s quasar x=%.2f y=%.2f state=%d timer=%.2f aim=%.4f near=%.2f cd=%.2f zones=%d\n",
                                        label, mob->worldX, mob->worldY, mob->quasarState, mob->quasarTimer,
                                        std::atan2(mob->quasarAimY, mob->quasarAimX), mob->quasarNearTimer,
                                        mob->quasarEruptCooldown, mob->quasarEruptCount);
                                else if (mob->kind == MobKind::LYNX) {
                                    std::fprintf(mf, "%s lynx x=%.1f y=%.1f heading=%.3f snipe=%d timer=%.2f\n",
                                        label, mob->worldX, mob->worldY, mob->lynxHeading, mob->lynxSnipe, mob->lynxSnipeTimer);
                                }
                                else if (mob->kind == MobKind::REGULUS)
                                    std::fprintf(mf, "%s regulus x=%.1f y=%.1f shield=%.1f elite=%d\n",
                                        label, mob->worldX, mob->worldY,
                                        mob->regulusShield.hp, (int)IsEliteMob(mob->kind));
                                if (mob->genesisControlled)
                                    std::fprintf(mf, "%s ctrl kind=%d x=%.1f y=%.1f rgb=%.2f,%.2f,%.2f thrownPhase=%d vel=%.0f,%.0f carrier=%d summoned=%d\n",
                                        label, (int)mob->kind, mob->worldX, mob->worldY,
                                        mob->color.r, mob->color.g, mob->color.b, mob->thrownPhase,
                                        mob->ctrlPlayerVel.x, mob->ctrlPlayerVel.y,
                                        (int)mob->genesisSummonCarrier, (int)mob->genesisSummoned);
                                if (mob->RegulusBuffed())
                                    std::fprintf(mf, "%s buffed kind=%d speedAge=%.2f stealthAge=%.2f shield=%d speedx=%.2f alpha=%.3f\n",
                                        label, (int)mob->kind, mob->regulusSpeedAge,
                                        mob->regulusStealthAge, (int)(mob->regulusShield.hp > 0.0f), mob->RegulusSpeedBuff(),
                                        mob->RegulusCloakAlpha());
                            }
                            std::fclose(mf);
                        }
                    } else if (step == "SPAWN=SCOPE") {
                        // SPAWN=SCOPE: one ranged Scope 600px right of the player.
                        const size_t before = g_MonsterManager.rangedMobs.size();
                        g_MonsterManager.SpawnRangedMob(screenWidth, screenHeight,
                                                        1.0f, (int)before + 1);
                        if (g_MonsterManager.rangedMobs.size() > before) {
                            RangedMob* mob = g_MonsterManager.rangedMobs.back();
                            mob->worldX = playerWin.x + playerWin.width * 0.5f + 600.0f;
                            mob->worldY = playerWin.y + playerWin.height * 0.5f;
                        }
                    } else if (step.rfind("SPAWN=", 0) == 0) {
                        // SPAWN=<MobKind index>: one mob 600px right of the
                        // player, for capturing a specific enemy's visuals.
                        const int kind = std::atoi(step.c_str() + 6);
                        const size_t before = g_MonsterManager.monsters.size();
                        if ((kind >= 0 && kind <= (int)MobKind::MAGNETAR) || kind == (int)MobKind::LYNX || kind == (int)MobKind::MIRA || kind == (int)MobKind::TRIANGULUM)
                            g_MonsterManager.SpawnMob(screenWidth, screenHeight, 160);
                        if (g_MonsterManager.monsters.size() > before) {
                            Monster* mob = g_MonsterManager.monsters.back();
                            mob->worldX = playerWin.x + playerWin.width * 0.5f + 600.0f;
                            mob->worldY = playerWin.y + playerWin.height * 0.5f;
                            mob->MakeKind((MobKind)kind);
                        }
                    } else if (step.rfind("DEBUGSPAWN=", 0) == 0) {
                        // DEBUGSPAWN=<MobKind index>: the F1 > Spawn path, from
                        // the screen edge as a hand-spawned mob arrives.
                        debugSpawnMob(std::atoi(step.c_str() + 11), 1);
                    } else if (step.rfind("SKILLHUD=", 0) == 0) {
                        // SKILLHUD=MIX|EMPTY|PULSE: dash HUD states (cooling
                        // down, ready, ready pulse) on a fresh run.
                        const std::string preset = step.substr(9);
                        RestartCurrentRun();
                        if (preset == "MIX") {
                            g_PlayerRuntime.dashCooldown = DASH_CD;   // ~half left at the ~1.5 s capture
                            g_PlayerRuntime.dashCooldownMax = DASH_CD;
                        } else if (preset == "PULSE") {
                            g_PlayerRuntime.dashCooldown = 0.05f;
                            g_PlayerRuntime.dashCooldownMax = DASH_CD;
                            g_SmokeTimeScale = 0.2f;
                        }
                    } else if (step.rfind("NORMADUMP=", 0) == 0) {
                        // NORMADUMP=<label>: one line of the boss, its shots and
                        // every pane to Screenshots/norma_state.txt.
                        std::filesystem::create_directories("Screenshots");
                        FILE* nf = nullptr;
#ifdef _MSC_VER
                        if (fopen_s(&nf, "Screenshots/norma_state.txt", "a") != 0) nf = nullptr;
#else
                        nf = std::fopen("Screenshots/norma_state.txt", "a");
#endif
                        if (nf) {
                            const Monster* nb = FindNorma();
                            int shots = 0;
                            for (const Bullet& b : g_Bullets) if (b.active && b.normaShot) ++shots;
                            int open = 0, locked = 0;
                            float ovl = 0.0f;
                            for (const os_pane::Pane& p : os_pane::Panes())
                                for (const os_pane::Pane& q : os_pane::Panes()) {
                                    if (!p.open || !q.open || p.id >= q.id) continue;
                                    ovl += std::max(0.0f, std::min(p.frame.x + p.frame.w, q.frame.x + q.frame.w) - std::max(p.frame.x, q.frame.x)) *
                                           std::max(0.0f, std::min(p.frame.y + p.frame.h, q.frame.y + q.frame.h) - std::max(p.frame.y, q.frame.y));
                                }
                            for (const os_pane::Pane& p : os_pane::Panes()) {
                                if (!p.open) continue;
                                ++open;
                                if (p.locked) ++locked;
                            }
                            std::fprintf(nf, "%s ovl=%.0f state=%d hp=%.0f boss=%d pattern=%d stage=%d exposed=%.2f shots=%d panes=%d locked=%d fuses=%d box=%d/%d ram=%d/%d wall=%d\n",
                                step.c_str() + 10, ovl, (int)gm.currentState, gm.playerHP, nb ? 1 : 0, nb ? (int)nb->normaPattern : -1,
                                nb ? (int)nb->normaStage : -1, nb ? nb->normaExposed : 0.0f, shots,
                                open, locked, (int)g_PaneFuses.size(), g_ThrowBox.id, g_ThrowBox.stage,
                                g_PaneRams.empty() ? 0 : g_PaneRams[0].id, g_PaneRams.empty() ? 0 : g_PaneRams[0].stage, (int)g_Wall.panes.size());
                            std::fclose(nf);
                        }
                    } else if (step == "NORMACLOSE") {
                        // NORMACLOSE: the title-bar X on every open pane, as the
                        // player would press it.
                        for (const os_pane::Pane& p : os_pane::Panes())
                            if (p.open) os_pane::DebugRequestClose(p.id);
                    } else if (step.rfind("NORMATEST=", 0) == 0) {
                        // NORMATEST=AIM|RAM|WALL|WARP|HIDE|P2: bring NORMA in (to the
                        // player's right) and start that pattern now; P2 drops it
                        // into phase 2. Motion captures follow as plain steps.
                        const std::string arg = step.substr(10);
                        // A new run opens on its starting pick: take the first
                        // card so the fight actually runs.
                        if (gm.currentState == GameState::AUG_SELECT && gm.augChoiceCount > 0)
                            FinishAugmentEffects();
                        if (!FindNorma())
                            DebugSpawnNorma(g_MonsterManager, screenWidth,
                                            GetDifficultyParams(g_Difficulty).enemyHpMultiplier,
                                            g_Difficulty);
                        // Clear the stage so the capture shows NORMA alone.
                        for (Monster* m : g_MonsterManager.monsters)
                            if (m->alive && m->kind != MobKind::NORMA) m->alive = false;
                        if (g_ThrowBox.id != 0) os_pane::Close(g_ThrowBox.id);
                        g_ThrowBox = {};
                        if (arg != "PICK") {   // PICK keeps HP, so a hit stays readable
                            gm.maxHP = g_Stats.maxHP = 100000.0f;   // so a test run outlives the bursts
                            gm.playerHP = gm.maxHP;
                        }
                        if (Monster* boss = FindNorma()) {
                            if (arg.rfind("HP", 0) == 0 && arg.size() > 2) {
                                // HP<n>: the boss at n% of its max HP (for the boss bar).
                                boss->hp = boss->normaMaxHp * std::strtof(arg.c_str() + 2, nullptr) / 100.0f;
                            } else if (arg == "PICK") {
                                // PICK: only take the open card (above), so a long capture
                                // is not stalled by a level-up.
                            } else if (arg == "WEAR") {
                                // WEAR: wear the open panes 15%, 45%, 75%... so the seal
                                // ticks show several states at once.
                                int k = 0;
                                for (const os_pane::Pane& pane : os_pane::Panes())
                                    if (pane.open && !pane.minimized)
                                        WearOf(pane.id).hits = PaneMaxDurability(pane) *
                                                               std::min(0.97f, 0.15f + 0.30f * (float)k++);
                            } else if (arg == "P2") {
                                boss->hp = boss->normaMaxHp * 0.35f;
                            } else {
                                boss->worldX = playerWin.x + playerWin.width * 0.5f + 380.0f;
                                boss->worldY = playerWin.y + playerWin.height * 0.5f - 60.0f;
                                const norma::Pattern p = arg == "AIM" ? norma::Pattern::Aim
                                                       : arg == "RAM" ? norma::Pattern::Ram
                                                       : arg == "THROW" ? norma::Pattern::Throw
                                                       : arg == "DETONATE" ? norma::Pattern::Detonate
                                                       : arg == "WALL" ? norma::Pattern::Wall
                                                       : arg == "REINFORCE" ? norma::Pattern::Reinforce
                                                       : arg == "WARP" ? norma::Pattern::Warp
                                                       : arg == "REFUGE" ? norma::Pattern::Refuge
                                                       : arg == "CRASH" ? norma::Pattern::Crash
                                                       : norma::Pattern::None;
                                if (arg == "PANES") norma::g_Asks.push_back(norma::Ask::ReopenPanes);
                                if (p != norma::Pattern::None) boss->StartNormaPattern(p);
                            }
                        }
                    } else if (step.rfind("CAELUMTEST=", 0) == 0) {
                        // CAELUMTEST=SPREAD|SCAN|SMEAR|SHOT|SIPHON|SPOUT|WAVE|LINE|BAD|P2|ERODE|OVERFLOW: bring CAELUM
                        // in mid-screen and start that pattern now (P2: phase 2 by
                        // health; ERODE: phase 2 rules at once; SPOUT: the siphon in
                        // phase 2, spouting back; OVERFLOW: corrupt enough cells for
                        // the file to break).
                        const std::string arg = step.substr(11);
                        if (gm.currentState == GameState::AUG_SELECT && gm.augChoiceCount > 0)
                            FinishAugmentEffects();
                        const auto findCaelum = [&]() -> Monster* {
                            for (Monster* cm : g_MonsterManager.monsters)
                                if (cm->kind == MobKind::CAELUM && cm->alive) return cm;
                            return nullptr;
                        };
                        if (!findCaelum())
                            DebugSpawnCaelum(g_MonsterManager, screenWidth,
                                             GetDifficultyParams(g_Difficulty).enemyHpMultiplier,
                                             g_Difficulty);
                        for (Monster* m : g_MonsterManager.monsters)
                            if (m->alive && m->kind != MobKind::CAELUM) m->alive = false;
                        gm.maxHP = g_Stats.maxHP = 100000.0f;
                        gm.playerHP = gm.maxHP;
                        if (Monster* boss = findCaelum()) {
                            boss->worldX = (float)screenWidth * 0.5f;
                            boss->worldY = 220.0f;
                            const float pcx = playerWin.x + playerWin.width * 0.5f;
                            const float pcy = playerWin.y + playerWin.height * 0.5f;
                            if (arg == "P2") {
                                boss->hp = boss->normaMaxHp * 0.35f;
                            } else if (arg == "BAD") {
                                for (int k = 0; k < 4; ++k) caelum::AddBadSector(pcx, pcy);   // four handles' worth
                                caelum::g.exposed = 4.0f;
                            } else if (arg == "WAVE" || arg == "LINE") {
                                caelum::g.waveCd = 0.0f;   // the shockwave's warning starts now
                                caelum::g.waves = arg == "LINE" ? 1 : 0;   // which kind comes next
                                caelum::g.exposed = 4.0f;  // and no pattern gets in the way
                            } else if (arg == "ERODE") {
                                caelum::g.phase = 2;
                                caelum::g.exposed = 6.0f;   // hold the patterns off to watch erosion alone
                            } else if (arg == "SIPHON" || arg == "SPOUT") {
                                if (arg == "SPOUT") {   // phase 2 without the shift (its health under the line)
                                    boss->hp = boss->normaMaxHp * caelum::kPhase2Ratio * 0.9f;
                                    caelum::g.phase = 2;
                                }
                                // A board to drink: cells in a loose ring round the player.
                                const int cols = caelum::Cols(), here = caelum::CellAt(pcx, pcy);
                                for (int k = 0; k < caelum::CellCount(); ++k) {
                                    if (k == here || ((k % cols) + (k / cols) * 2) % 3 != 0) continue;
                                    if (std::abs(k % cols - here % cols) + std::abs(k / cols - here / cols) > 3) continue;
                                    caelum::g.cells[k].state = caelum::CellState::Corrupt;
                                    caelum::g.cells[k].life = 9.0f;
                                    caelum::g.cells[k].glitch = (unsigned char)(rand() % 256);
                                }
                                caelum::g.bossX = boss->worldX; caelum::g.bossY = boss->worldY;
                                caelum::StartPattern(pcx, pcy, caelum::Pattern::Siphon);
                            } else if (arg == "OVERFLOW") {
                                const int here = caelum::CellAt(pcx, pcy);
                                for (int i = 0, n = 0; i < caelum::CellCount() &&
                                     n < (int)(caelum::kOverflowShare * caelum::CellCount()) + 1; ++i) {
                                    const int k = (i * 7) % caelum::CellCount();
                                    if (k == here) continue;
                                    caelum::g.cells[k].state = caelum::CellState::Corrupt;
                                    caelum::g.cells[k].life = 8.0f;
                                    ++n;
                                }
                            } else {
                                const caelum::Pattern p = arg == "SPREAD" ? caelum::Pattern::Spread
                                                        : arg == "SCAN" ? caelum::Pattern::Scanline
                                                        : arg == "SMEAR" ? caelum::Pattern::Smear
                                                        : arg == "SHOT" ? caelum::Pattern::Shot
                                                        : caelum::Pattern::None;
                                if (p != caelum::Pattern::None) caelum::StartPattern(pcx, pcy, p);
                            }
                        }
                    } else if (step.rfind("HP=", 0) == 0) {
                        // HP=<0..1>: set the player's health fraction.
                        const float f = std::clamp(
                            (float)std::atof(step.c_str() + 3), 0.01f, 1.0f);
                        gm.playerHP = g_Stats.maxHP * f;
                    } else if (step.rfind("DIM=", 0) == 0) {
                        // DIM=<0..100>: menu darkness (settings > DISPLAY), for A/B captures.
                        g_MenuDim = std::clamp(std::atoi(step.c_str() + 4), 0, 100);
                    } else if (step.rfind("DIFF=", 0) == 0) {
                        // DIFF=EASY|NORMAL|HARD|NIGHTMARE: run difficulty (wave count,
                        // elite schedule) for wave HUD captures.
                        const std::string d = step.substr(5);
                        g_Difficulty = d == "EASY" ? Difficulty::EASY
                                     : d == "HARD" ? Difficulty::HARD
                                     : d == "NIGHTMARE" ? Difficulty::NIGHTMARE
                                     : Difficulty::NORMAL;
                    } else if (step.rfind("CODEXMOB=", 0) == 0) {
                        // CODEXMOB=UNSEEN|SEEN|KNOWN: Astral Log state of
                        // every enemy, for the elite name tag. Never saved.
                        const std::string st = step.substr(9);
                        for (int id = 0; id < CM_COUNT; ++id) {
                            g_MobSeen[id] = st != "UNSEEN";
                            g_MobKillCounts[id] = st == "FULL" ? 1000000 : st == "KNOWN" ? 1 : 0;
                            g_RunMobKillCounts[id] = 0;
                        }
                    }
                    if (step.rfind("SEARCH=", 0) == 0) {
                        // SEARCH=<ascii text>: fill the Codex search field
                        // as if typed (keyboard input is not scripted).
                        const std::string q = step.substr(7);
                        CodexSearchClear();
                        for (char ch : q) {
                            if (g_CodexSearchLen >= 63) break;
                            g_CodexSearch[g_CodexSearchLen++] = (wchar_t)ch;
                        }
                        g_CodexSearch[g_CodexSearchLen] = L'\0';
                        g_CodexSearchCaret = g_CodexSearchLen;
                    }
                    // "WAIT" (or any other name) keeps the current scene.
                });
            if (g_SmokeGod && g_GameManager.currentState == GameState::RUNNING)
                g_GameManager.playerHP = g_Stats.maxHP;
            if (g_SmokeFire) {
                // FIRE=1: hold fire, and take the first card of every pick.
                lmb = true;
                GameManager& fgm = g_GameManager;
                if (fgm.currentState == GameState::AUG_SELECT && fgm.augChoiceCount > 0 &&
                    g_AugExitT < 0.0f) {
                    passive::Acquire(fgm.augChoices[0]);
                    MarkAugSeen(fgm.augChoices[0]);
                    FinishAugmentEffects();
                }
            }
            // Scripted pointer events arrive after the normal input pass.
            // Route them through the same toolkit hitboxes as physical clicks.
            if (g_DebugToolkit.IsVisible() && lmb)
                g_DebugToolkit.BeginInput(nullptr, (float)screenWidth,
                    (float)screenHeight, mx, my, lmb, false);
            wmx = ScreenToWorldX((float)mx);
            wmy = ScreenToWorldY((float)my);
        }

        // GAMEOVER?�READY ?�동 감�? (ESC ?�으�?직접 ?�환??경우)
        if (prevState == GameState::GAMEOVER &&
            g_GameManager.currentState == GameState::READY) {
            ResetForNewGame();
        }
        // The tutorial flag lives while the READY screen is up, including the
        // card selections it opens.
        {
            const GameState ts = g_GameManager.currentState;
            // The tutorial's pause menu (and the settings opened from it) also
            // keeps it: resuming returns to the practice field, where ESC must
            // open the pause menu again instead of leaving for the lobby.
            const bool tutorialPauseFlow =
                (ts == GameState::PAUSED || ts == GameState::SETTINGS) &&
                g_GameManager.pauseResumeState == GameState::READY;
            if (ts != GameState::READY && ts != GameState::AUG_SELECT &&
                !tutorialPauseFlow)
                ready_launch::g_TutorialMode = false;
        }
        // Launching from the tutorial after taking its card starts a clean run;
        // Scene_Ready does it when the launch animation ends (in the render pass).
        // PAUSED ???�른 ?�태: 증강 ?�명 박스 ?�기
        if (!debugInputCaptured)
            g_GameManager.UpdateStateSystem(g_MonsterManager, g_Bullets);
        SyncGameplayTelemetry();


        // Keep the main loop music active during gameplay and stop it in menus.
        {
            Audio::SetEnabled(g_AudioEngineEnabled && g_SoundVol > 0);
            g_BgmEnabled = g_BgmVol > 0;
            g_SfxEnabled = g_SfxVol > 0;
            Audio::SetBgmVolume(g_BgmVol / 100.0f);
            Audio::SetSfxVolume(g_SfxVol / 100.0f);
            Audio::SetBgmEnabled(g_BgmEnabled);
            Audio::SetSfxEnabled(g_SfxEnabled);
            Audio::SetVolume(g_SoundVol / 100.0f);        // 마스??볼륨
            // 장면·런 상태에 맞는 곡 (BGM 을 다시 켜면 그 곡부터)
            UpdateBgmFlow(realDelta);
            Audio::Update();   // 덕킹 복귀, 끝난 곡 정리 — 매 프레임 (메뉴 포함)
        }

        // ?�리?�이?�브 무적 ??�??�레??체력 ?�?고정 (?��? 죽�? ?�음)
        if (((g_CreativeGodmode && (g_CreativeMode || g_DebugMode)) ||
             ready_launch::g_StartGraceT > 0.0f) &&
            g_GameManager.currentState == GameState::RUNNING) {
            g_GameManager.playerHP = g_Stats.maxHP;
        }

        // HP 0 ??MK2 부??OR DYING (1�??�로??모션 ??GAMEOVER)
        // Phoenix Charm revives (full HP) instead of dying, once per run.
        const bool debugKill = g_DebugKillPlayer && g_GameManager.currentState == GameState::RUNNING;
        g_DebugKillPlayer = false;
        if (debugKill ||
            (g_GameManager.playerHP <= 0.0f &&
             g_GameManager.currentState == GameState::RUNNING &&
             !passive::TryPhoenix())) {
            // MK2: 1??부????공격??비�? ??�� + ?�?HP (?�널???�음)
            // ?�반 ?�망 ???�레?�어 기점 ?�??��(모든 ???�짐) ??메뉴 ?�이?�인
            g_GameManager.currentState = GameState::DYING;
            g_GameManager.playerHP     = 0.0f;
            Audio::StopBgm();
            Audio::Play(Audio::Cue::Death);
            float pCX = playerWin.x + playerWin.width  * 0.5f;
            float pCY = playerWin.y + playerWin.height * 0.5f;
            g_DeathCX = pCX; g_DeathCY = pCY;
            g_DyingTimer    = DYING_DUR;
            g_DeathBoomDone = false;
            g_DeathFlash    = 0.0f;
            // ?�리모프 ??�??�복 (?�망 ?�출??�??�음)
            g_ZoomCX = g_ZoomCY = 0.0f;
            g_ViewZoom = 1.0f; g_ViewZoomTarget = 1.0f;
            // ?�망 ?�인 ???�티????�� ?? 가??가까운 ?�협(?�로?�스)??기록 (결과창에 ?�시??
            {
                int li = LangIndex();
                g_DeathReason[0] = L'\0';   // no "signal dispersed" line; the result screen falls back to the outcome label
                if (g_NormaDoom) {
                    static const wchar_t* const DOOM[3] = {
                        L"OCTANS의 심판: 시간 초과", L"Judged by OCTANS: time ran out",
                        L"OCTANSの裁き: 時間切れ" };
                    wcscpy_s(g_DeathReason, DOOM[li]);
                }
            }
        }       // close outer if (HP <= 0)

        // DYING ?�망 ?�출 ???�레?�어 기점 ?�??��(모든 ???�짐) ??GAMEOVER (�?변???�음)
        if (g_GameManager.currentState == GameState::DYING) {
            g_DyingTimer -= delta;
            g_DeathFlash -= delta * 4.0f;
            if (g_DeathFlash < 0.0f) g_DeathFlash = 0.0f;

            // ?�??�� ???�레?�어 기점, 모든 ?�이 ?�짐 (즉시, �??�네마틱 ?�음)
            if (!g_DeathBoomDone) {
                g_DeathBoomDone = true;
                float pCX = g_DeathCX, pCY = g_DeathCY;
                // 모든 ????��?�키�??�거 (scored/noBlast ?�시 ???�수/?�쇄 ?�산 ????
                for (auto m : g_MonsterManager.monsters) if (m->alive) {
                    m->SpawnDeathEffect((float)now, false, false);
                    m->alive = false; m->scored = true; m->noBlast = true;
                }
                for (auto r : g_MonsterManager.rangedMobs) if (r->alive) {
                    r->SpawnDeathEffect();
                    r->alive = false; r->scored = true;
                }
                // 모든 ?�·보?�·분?�체·총알·?�탑??즉시 ?�전 ??�� ??UpdateAll(RUNNING ?�용)??                //   맡기�?DYING/GAMEOVER ?�안 ?�거�?�?�??�이 ?��? ?�태�??�으므�??�기???�거.
                g_MonsterManager.Clear();   // monsters, ranged mobs, and projectiles are deleted and cleared
                g_Bullets.clear();
                // ?�캔 ?�이?�?�??�리
                   // 배드 ?�터 감속 구역/출혈 ?�리
                // ?�레?�어 중심 ?�??�� + 충격??+ ?�광 + ?�들�?+ 방사???�편
                // Kept tight round the ship: two small bursts, not four big ones.
                for (int k = 0; k < 2; k++)
                    SpawnEnemyExplosion(pCX, pCY, 1.0f, 0.85f - (k%2)*0.4f, 0.3f, false);
                g_DeathFlash = 1.0f;
                g_ShakeTime = 0.5f; g_ShakeMag = 30.0f;
                TriggerFlash(0.6f, 0.85f, 1.0f, 0.8f);
                for (int i = 0; i < MAX_DEBRIS; i++) {
                    float angle = (float)i / (float)MAX_DEBRIS * 2.0f * (float)M_PI
                                + ((float)(rand() % 100) - 50.0f) * 0.01f;
                    // ~70-160 px of travel with the drag below; 3-8 px pieces.
                    float spd = 180.0f + (float)(rand() % 200);
                    float sz  = 3.0f + (float)(rand() % 6);
                    int   tint = rand() % 10;
                    float cr, cg, cb;
                    if      (tint < 5) { cr = 0.10f; cg = 0.85f; cb = 1.00f; }
                    else if (tint < 8) { cr = 1.00f; cg = 1.00f; cb = 1.00f; }
                    else               { cr = 1.00f; cg = 0.85f; cb = 0.20f; }
                    g_Debris[i] = { pCX, pCY, cosf(angle)*spd, sinf(angle)*spd, sz, cr, cg, cb, true };
                }
            }

            float sd = delta;
            for (int i = 0; i < MAX_DEBRIS; i++) {
                if (!g_Debris[i].active) continue;
                g_Debris[i].x  += g_Debris[i].vx * sd;
                g_Debris[i].y  += g_Debris[i].vy * sd;
                g_Debris[i].vx *= std::max(0.0f, 1.0f - 2.4f * sd);
                g_Debris[i].vy *= std::max(0.0f, 1.0f - 2.4f * sd);
            }
            if (g_DyingTimer <= 0.0f) {
                g_GameplayTelemetry.EndRun(g_GameTime,
                                           g_GameManager.playerLevel,
                                           g_GameManager.xp,
                                           g_Stats,
                                           "gameover");
                g_ViewZoom = g_ViewZoomTarget = 1.0f;   // �??�복 (메뉴 ?�상??
                g_ZoomCX = g_ZoomCY = 0.0f;             // �?중심 ?�면 중앙?�로
                g_GameManager.currentState = GameState::GAMEOVER;
                g_DyingTimer = 0.0f;
                g_GameOverFade = 0.0f;   // 결과 메뉴 ?�이?�인 ?�작 (2.5�?
                // 기록 ?�?????�이?�별 최고???�적/코인 갱신 (?�기록이�??�시)
                //   ?�리?�이?�브 모드(?�드박스)??코인·기록 ?�외 (?�밍 방�?)
                g_LastRunRecord = CommitRunResult(!PlayRecordsProgress(), (int)g_Difficulty,
                                                  g_GameManager.score, g_Stats.killCount,
                                                  (g_SelectedJob == JOB_STATIC_FIELD ? 1 : 0),
                                                  g_GameManager.currentWave);
            }
        }

        if (g_GameManager.currentState == GameState::GAMEOVER && g_GameOverFade < 1.0f) {
            g_GameOverFade += delta / GAMEOVER_FADE;
            if (g_GameOverFade > 1.0f) g_GameOverFade = 1.0f;
        }

        // ?�망 ??�� ?�여�??�티?�·파?�·충격파·?�파????게임?�버 ?�면??굳어버리지 ?�도�?        //   DYING/GAMEOVER ?�안?�도 계속 갱신???�연?�럽�??�라지�??�다.
        {
            GameState gvs = g_GameManager.currentState;
            if (gvs == GameState::DYING || gvs == GameState::GAMEOVER) {
                UpdateEnemyParticles(delta);
                                for (auto& sp : g_Sparks) {
                    sp.x += sp.vx * delta; sp.y += sp.vy * delta;
                    float drag = std::max(0.0f, 1.0f - 6.0f * delta);
                    sp.vx *= drag; sp.vy *= drag; sp.life -= delta;
                }
                g_Sparks.erase(std::remove_if(g_Sparks.begin(), g_Sparks.end(),
                    [](const Spark& s){ return s.life <= 0.0f; }), g_Sparks.end());
                norma_fx::Tick(delta);
                caelum_fx::Tick(delta);
                if (gvs == GameState::GAMEOVER) {
                    for (int i = 0; i < MAX_DEBRIS; i++) { if (!g_Debris[i].active) continue;
                        g_Debris[i].x += g_Debris[i].vx * delta; g_Debris[i].y += g_Debris[i].vy * delta;
                        g_Debris[i].vx *= std::max(0.0f, 1.0f - 2.4f * delta); g_Debris[i].vy *= std::max(0.0f, 1.0f - 2.4f * delta);
                    }
                }
            }
        }

        // --- AUG_SELECT: Card1~3 keys focus, Confirm key confirms (KeyBindings.h) ---
        static bool s_augSpaceReleased = true;
        if (inputFocusChanged)
            s_augSpaceReleased = !ActionHeld(GameAction::Confirm);
        if (!ActionHeld(GameAction::Confirm)) s_augSpaceReleased = true;
        if (!debugInputCaptured &&
            g_GameManager.currentState == GameState::AUG_SELECT) {
            const int k1 = ActionHeld(GameAction::Card1) ? GLFW_PRESS : GLFW_RELEASE;
            const int k2 = ActionHeld(GameAction::Card2) ? GLFW_PRESS : GLFW_RELEASE;
            const int k3 = ActionHeld(GameAction::Card3) ? GLFW_PRESS : GLFW_RELEASE;
            // The placeholder card has no effect: taking it only closes the pick.
            auto applyAug = [&](int slot) {
                if (slot < 0 || slot >= g_GameManager.augChoiceCount) return;
                const int selectedIdx = g_GameManager.augChoices[slot];
                if (selectedIdx < 0 || selectedIdx >= AUG_TOTAL) return;
                passive::Acquire(selectedIdx);
                MarkAugSeen(selectedIdx);
                FinishAugmentEffects();
            };

            // 선택 펄스 + 흡수, 남은 카드는 별로 해체 (AugCardModel.h)
            constexpr float AUG_EXIT_DUR = aug_card::kExitDur;

            // exit anim timer -- fires applyAug when done
            bool augExitFired = false;
            if (g_AugExitT >= 0.0f) {
                g_AugExitT += delta;
                if (g_AugExitT >= AUG_EXIT_DUR) {
                    applyAug(g_AugExitSlot);
                    g_HoveredAug  = -1;
                    g_AugExitT    = -1.0f;
                    g_AugExitSlot = -1;
                    s_augSpaceReleased      = false;
                    g_GameManager.confirmPauseReleased = false;
                    augExitFired = true;
                }
            }

            if (!augExitFired) {
                // 1~5 focuses a card; SPACE starts the confirmation exit.
                if (g_AugExitT < 0.0f) {
                    if (k1 == GLFW_PRESS && g_aug1Released && g_GameManager.augChoiceCount > 0) {
                        g_HoveredAug = 0; g_GameManager.augmentKeyboardFocus = true;
                        g_aug1Released = false;
                    }
                    if (k2 == GLFW_PRESS && g_aug2Released && g_GameManager.augChoiceCount > 1) {
                        g_HoveredAug = 1; g_GameManager.augmentKeyboardFocus = true;
                        g_aug2Released = false;
                    }
                    if (k3 == GLFW_PRESS && g_aug3Released && g_GameManager.augChoiceCount > 2) {
                        g_HoveredAug = 2; g_GameManager.augmentKeyboardFocus = true;
                        g_aug3Released = false;
                    }
                }
                if (k1 == GLFW_RELEASE) g_aug1Released = true;
                if (k2 == GLFW_RELEASE) g_aug2Released = true;
                if (k3 == GLFW_RELEASE) g_aug3Released = true;

                // Confirm key = confirm the focused constellation.
                const int kSp = ActionHeld(GameAction::Confirm) ? GLFW_PRESS : GLFW_RELEASE;
                if (kSp == GLFW_PRESS && s_augSpaceReleased &&
                    g_HoveredAug >= 0 && g_HoveredAug < g_GameManager.augChoiceCount &&
                    g_AugExitT < 0.0f) {
                    g_AugExitT    = 0.0f;
                    g_AugExitSlot = g_HoveredAug;
                    s_augSpaceReleased = false;
                    g_GameManager.confirmPauseReleased = false;
                }


            }
        }

        // --- �� ���� (���� Ŭ���� ��) ---
        // --- ?�리?�이?�브 모드: F = 증강 그랩(?�버???�함 ?�드박스), G = 무적 ?��? ---
        if (!debugInputCaptured && (g_CreativeMode || g_DebugMode || kCompileDebugBuild)) {
            static bool s_fkeyReleased = true;
            int kF = glfwGetKey(window, GLFW_KEY_F);
            if (inputFocusChanged) s_fkeyReleased = kF == GLFW_RELEASE;
            if (kF == GLFW_RELEASE) s_fkeyReleased = true;
            if (kF == GLFW_PRESS && s_fkeyReleased &&
                g_GameManager.currentState == GameState::RUNNING) {
                // ?�드박스: ?�버?�도 카드 ?�???�어??무엇?�든 집을 ???�게
                const int oldLevel = g_GameManager.playerLevel;
                const XpAmount oldXp = g_GameManager.xp;
                ++g_GameManager.playerLevel;
                g_GameManager.xp = 0;
                GrantLevelUpReward(oldLevel, 0, oldXp, LevelUpSource::MANUAL);
                g_GameManager.ActivateNextAugmentReward();
                g_CreativeFreeGrab = false;
                s_fkeyReleased = false;
            }
            // G ??무적 ON/OFF ?��?
            static bool s_gkeyReleased = true;
            int kG = glfwGetKey(window, GLFW_KEY_G);
            if (inputFocusChanged) s_gkeyReleased = kG == GLFW_RELEASE;
            if (kG == GLFW_RELEASE) s_gkeyReleased = true;
            if (kG == GLFW_PRESS && s_gkeyReleased &&
                g_GameManager.currentState == GameState::RUNNING) {
                g_CreativeGodmode = !g_CreativeGodmode;
                s_gkeyReleased = false;
            }
        }

        // --- ?�트?�톱: ???�벤??직후 ?�깐 ?��??�이???��? (?�더??계속) ---
        if (g_HitStopTimer > 0.0f) g_HitStopTimer -= delta;

        // --- Level-up signal: real time; opens the selection when done ---
        {
            LevelUpTransition& transition = g_GameManager.levelUpTransition;
            const GameState st = g_GameManager.currentState;
            if (st == GameState::RUNNING) {
                // Not under OCTANS' blue screen: the cards would open hidden
                // beneath it and stop its clock; they wait for the shatter.
                if (!CrashScreenUp() && transition.Update(delta))
                    g_GameManager.ActivateNextAugmentReward();  // none left: keep playing
            } else if (st != GameState::PAUSED) {
                transition.Cancel();   // death or leaving the run wins
            }
        }

        // --- Fixed timestep: speed comes from GameplayTime.h ---
        const float physDelta = ComputeGameplayDelta(delta, CurrentGameplayTime());
        accumulator += physDelta;
        while (accumulator >= FIXED_DT) {
            if (g_GameManager.ShouldUpdate()) {
                const bool playerControl = g_GameManager.currentState == GameState::RUNNING;
                if (!playerControl) {
                    g_PlayerRuntime.dashActive = false;
                }
                const float zoomNow = (g_ViewZoom < 0.01f) ? 0.01f : g_ViewZoom;
                float mvX = 0.0f, mvY = 0.0f;
                MoveInput mv = ResolvedMoveHeld();
                if (g_SmokeWalk) mv = { g_SmokeWalk == 'U', g_SmokeWalk == 'D', g_SmokeWalk == 'L', g_SmokeWalk == 'R' };
                const float mlen = UpdatePlayerMovement(
                    playerWin.x, playerWin.y,
                    mv.up, mv.down, mv.left, mv.right,
                    playerControl, MOVE_SPEED * g_Stats.moveSpeedMult *
                        caelum::MoveFactor() * mira::MoveFactor(),   // weakened by corruption
                    FIXED_DT, g_PlayerRuntime.dashActive, mvX, mvY);
                // CAELUM's siphon draws the player toward its body while it drinks.
                playerWin.x += caelum::g.pullX * FIXED_DT;
                playerWin.y += caelum::g.pullY * FIXED_DT;
                if (playerControl && mlen > 0.001f && !g_PlayerRuntime.dashActive) {
                    // ?�동 ?�상(afterimage) ???�정 간격?�로 ?�레?�어 중심???��? ?�안 ?�상
                    static float s_trailAcc = 0.0f;
                    s_trailAcc += FIXED_DT;
                    if (s_trailAcc >= 0.028f) {
                        s_trailAcc = 0.0f;
                        SpawnTrail(playerWin.x + playerWin.width  * 0.5f,
                                   playerWin.y + playerWin.height * 0.5f,
                                   mvX, mvY, 0.35f, 0.85f, 1.0f);
                    }
                }

                const PlayerViewportBounds viewport = GetPlayerViewportBounds(
                    (float)screenWidth, (float)screenHeight, zoomNow,
                    BROWSER_CHROME_H, g_GameBarH + (float)g_TaskbarH);
                ClampPlayerToViewport(playerWin.x, playerWin.y,
                                      playerWin.width, playerWin.height,
                                      viewport);
                const float ccX = viewport.centerX;
                const float ccY = viewport.centerY;
                const float halfW = viewport.halfWidth;
                const float halfH = viewport.halfHeight;
                const float topLimit = viewport.top;
                const float bottomLimit = viewport.bottom;
                float pCX = playerWin.x + playerWin.width * 0.5f;
                float pCY = playerWin.y + playerWin.height * 0.5f;

                // ?�?�??�티�??�킬 ??쿨다??지??갱신 + ?�력(Shift ?�??/ Q·E·R ?�롯) ?�?�?
                UpdatePlayerRuntimeTimers(
                    FIXED_DT, GravisField::DashRecoveryMult(GravisField::g_playerInfluence));
                const float dashPulseBefore = g_SkillHudPulse.slot[0];
                g_SkillHudPulse.Update(GatherSkillHudInputs(), FIXED_DT);
                // 대시 슬롯 펄스가 새로 켜짐 = 대시 쿨다운이 끝남
                if (g_SkillHudPulse.slot[0] > dashPulseBefore) Audio::Play(Audio::Cue::SfxDashReady);


                // �÷��� ���?? ���� �̵� + ª�� ����
                if (playerControl && g_PlayerRuntime.dashActive) {
                    AdvancePlayerDash(FIXED_DT, pCX, pCY);
                    playerWin.x = pCX - playerWin.width * 0.5f;
                    playerWin.y = pCY - playerWin.height * 0.5f;
                }
                bool cDash = ActionHeld(GameAction::Dash);
                if (playerControl && cDash && !g_PlayerRuntime.dashInputHeld && g_PlayerRuntime.dashCooldown <= 0.0f && !g_PlayerRuntime.dashActive &&
                    !passive::DashSealed()) {
                    float ddx = mvX, ddy = mvY;
                    if (mlen <= 0.001f) {
                        float ax = wmx - pCX, ay = wmy - pCY;
                        float al = std::sqrt(ax*ax+ay*ay)+1e-3f; ddx = ax/al; ddy = ay/al;
                    }
                    const float topLimit = ccY + (BROWSER_CHROME_H - ccY) / zoomNow;
                    BeginPlayerDash(pCX, pCY, ddx, ddy,
                                    ccX - halfW, ccX + halfW,
                                    topLimit, ccY + halfH);
                    passive::OnDashStarted();
                    Audio::Play(Audio::Cue::Dash);
                    TriggerFlash(0.35f, 0.85f, 1.0f, 0.12f);
                }
                g_PlayerRuntime.dashInputHeld = cDash;

                // Apply each station once per fixed step.
                GravisField::Apply(g_MonsterManager, g_Bullets, pCX, pCY,
                                   playerControl, g_PlayerRuntime.dashActive,
                                   MOVE_SPEED * g_Stats.moveSpeedMult, FIXED_DT);
                AdvancePlayerKnockback(FIXED_DT, pCX, pCY);
                pCX = std::max(ccX - halfW, std::min(ccX + halfW, pCX));
                pCY = std::max(ccY + (BROWSER_CHROME_H - ccY) / zoomNow,
                               std::min(bottomLimit, pCY));
                KeepPlayerOutOfWall(pCX, pCY);
                playerWin.x = pCX - playerWin.width * 0.5f;
                playerWin.y = pCY - playerWin.height * 0.5f;

                UpdateProjectiles(g_Bullets, g_MonsterManager, playerWin,
                                  FIXED_DT, (float)screenWidth, (float)screenHeight,
                                  g_ViewZoom);

                // Passive augments: timers, counters, stillness, grazes.
                if (playerControl) {
                    passive::StepInput pin;
                    pin.dt = FIXED_DT;
                    pin.px = pCX; pin.py = pCY;
                    pin.aimX = wmx; pin.aimY = wmy;
                    pin.moving = mv.up || mv.down || mv.left || mv.right;
                    pin.dashActive = g_PlayerRuntime.dashActive;
                    pin.damageProtected = IsPlayerDamageProtected();
                    pin.viewL = ScreenToWorldX(0.0f);
                    pin.viewT = ScreenToWorldY(0.0f);
                    pin.viewR = ScreenToWorldX((float)screenWidth);
                    pin.viewB = ScreenToWorldY((float)screenHeight);
                    pin.viewZoom = g_ViewZoom;
                    pin.screenW = (float)screenWidth;
                    passive::Step(pin);

                    active::StepInput ain;
                    ain.dt = FIXED_DT;
                    ain.px = pCX; ain.py = pCY;
                    ain.aimX = wmx; ain.aimY = wmy;
                    ain.moving = pin.moving;
                    ain.keys[0] = ActionHeld(GameAction::Skill1);
                    ain.keys[1] = ActionHeld(GameAction::Skill2);
                    ain.keys[2] = ActionHeld(GameAction::Skill3);
                    ain.autoUse = g_AutoSkill || g_SmokeFire;
                    ain.viewL = pin.viewL; ain.viewT = pin.viewT;
                    ain.viewR = pin.viewR; ain.viewB = pin.viewB;
                    ain.viewZoom = pin.viewZoom;
                    ain.screenW = pin.screenW;
                    active::Step(ain);
                    active::StepSkillBullets();
                    float teleX, teleY;
                    if (active::TakeTeleport(teleX, teleY)) {
                        pCX = teleX; pCY = teleY;
                        playerWin.x = pCX - playerWin.width * 0.5f;
                        playerWin.y = pCY - playerWin.height * 0.5f;
                        g_PlayerRuntime.dashActive = false;
                    }
                }

                // ?�??무적 ???�번 ?�텝 ?�작 HP ?�??(???�해??무효, ?�복?�??��?)
                float hpAtStep = g_GameManager.playerHP;
                TickPlayerShield(FIXED_DT);
                //   ???��? = ?�간?��? ?�킬 OR 증강 ??직후 ~0.05s("?�이 ?�?, 짧게)
                bool  timeStopped = g_PlayerRuntime.postPickGrace > 0.45f;
                float enemyDt = FIXED_DT;

                // 몬스???�데?�트 (?�버??multiplier ?�용) ???�간 ?��? 중엔 ??멈춤
                float rmobMoveMult = 1.0f;
                // Time-based speed ramp. Density is lower now, so enemy pressure should not depend
                // only on score gained from kills.
                float mobSpdRamp = EnemySpeedRamp(g_GameTime);
                // Ư���� ? �� AI ���� �����衤�Ұ�, AI �� �÷��̾� �о��
                if (!timeStopped) {
                    g_PlayerSizeMult = g_Stats.playerSizeMult;
                    g_MonsterManager.UpdateAll(pCX, pCY, enemyDt,
                                               g_GameManager.playerHP, g_Bullets,
                                               mobSpdRamp,
                                               rmobMoveMult * mobSpdRamp,
                                               1.0f,
                                               -1.0f, -1.0f, -1.0f, -1.0f,
                                               glm::vec4(ccX - halfW, topLimit,
                                                         ccX + halfW, bottomLimit));
                    // Held kill stardust (Quasar, Genesis) comes out now.
                    ReleasePendingStardust(g_MonsterManager, pCX, pCY);

                    // Antares rams shove the player; dash i-frames ignore the shove.
                    // A run that hits the map edge stuns it there instead.
                    for (auto* m : g_MonsterManager.monsters) {
                        if (m) m->ConfineAntares(viewport.left, viewport.top,
                                                 viewport.right, viewport.bottom);
                        if (!m || m->kind != MobKind::ANTARES ||
                            (m->antaresKnockX == 0.0f && m->antaresKnockY == 0.0f))
                            continue;
                        if (g_PlayerRuntime.dashInvulnerability <= 0.0f) {
                            ApplyPlayerKnockback(m->antaresKnockX, m->antaresKnockY);
                            Audio::Play(Audio::Cue::SfxKnockback);
                        }
                        m->antaresKnockX = m->antaresKnockY = 0.0f;
                    }
                    // Large controller enemies must remain readable on-screen.
                    // Reflect their free-drift heading when the safe viewport is
                    // reached so they do not idle beyond the visible arena.
                    for (auto* m : g_MonsterManager.monsters) {
                        if (!m || !m->alive ||
                            (m->kind != MobKind::GRAVIS && m->kind != MobKind::QUASAR)) continue;
                        if (m->QuasarHoldsPosition() || m->LynxHoldsPosition()) continue;         // no clamp slide during its attack
                        const float margin = m->kind == MobKind::GRAVIS ? 92.0f : 76.0f;
                        const float minX = margin;
                        const float maxX = std::max(minX, (float)screenWidth - margin);
                        const float minY = margin;
                        const float maxY = std::max(minY, (float)screenHeight - margin);
                        if (m->kind == MobKind::GRAVIS && !m->gravisInArena) {
                            // A new Gravis glides in from its spawn edge instead
                            // of snapping onto the screen with its field.
                            const float gapX = std::clamp(m->worldX, minX, maxX) - m->worldX;
                            const float gapY = std::clamp(m->worldY, minY, maxY) - m->worldY;
                            const float gap = std::sqrt(gapX * gapX + gapY * gapY);
                            const float step = GravisField::ENTRY_SPEED * FIXED_DT;
                            m->gravisInArena = gap <= step;
                            const float t = m->gravisInArena ? 1.0f : step / gap;
                            m->worldX += gapX * t;
                            m->worldY += gapY * t;
                            continue;
                        }
                        const bool hitX = m->worldX < minX || m->worldX > maxX;
                        const bool hitY = m->worldY < minY || m->worldY > maxY;
                        m->worldX = std::max(minX, std::min(maxX, m->worldX));
                        m->worldY = std::max(minY, std::min(maxY, m->worldY));
                        if (m->kind == MobKind::GRAVIS) {
                            if (hitX) m->gravisDriftAngle = 3.14159265f - m->gravisDriftAngle;
                            if (hitY) m->gravisDriftAngle = -m->gravisDriftAngle;
                        } else {
                            if (hitX) m->quasarMoveAngle = 3.14159265f - m->quasarMoveAngle;
                            if (hitY) m->quasarMoveAngle = -m->quasarMoveAngle;
                        }
                    }
                }

                // Static Field is an area weapon with a real fixed-step pulse.
                // It supplements the loadout's short-range field identity and
                // gives the weapon a reliable combat effect instead of leaving
                // the field geometry as a purely cosmetic shell.
                if (g_CurrentWeapon == (int)StartWeapon::SMG && playerControl) {
                    g_StaticFieldPulseTimer -= FIXED_DT;
                    if (g_StaticFieldPulseTimer <= 0.0f && !timeStopped) {
                        const float radius2 = STATIC_FIELD_PULSE_RADIUS * STATIC_FIELD_PULSE_RADIUS;
                        const float pulseDamage = g_Stats.GetBaseDamage()
                            * g_Stats.GetDamageMultiplier() * STATIC_FIELD_PULSE_DAMAGE_MULT;
                        auto pulseTarget = [&](float tx, float ty, auto& target) {
                            if (!target.alive) return;
                            const float dx = tx - pCX, dy = ty - pCY;
                            if (dx * dx + dy * dy > radius2) return;
                            const float dealt = g_MonsterManager.ApplyDamage(
                                target, pulseDamage);
                            SpawnDamageNumber(tx, ty, dealt, false);
                        };
                        for (auto* m : g_MonsterManager.monsters)
                            pulseTarget(m->worldX, m->worldY, *m);
                        for (auto* r : g_MonsterManager.rangedMobs)
                            pulseTarget(r->worldX, r->worldY, *r);
                        Audio::Play(Audio::Cue::FieldPulse);
                        g_StaticFieldPulseTimer += STATIC_FIELD_PULSE_INTERVAL;
                    }
                } else {
                    g_StaticFieldPulseTimer = 0.0f;
                }

                // 리로???�너 ?�데?�트 (무기 ?�태머신 + ?�전 질주, ??총알 push)
CollisionSystem::Update(pCX, pCY,
                    g_MonsterManager, g_Bullets,
                    g_GameManager.playerHP,
                    g_GameManager.scoreAccum, g_GameManager.score,
                    g_Stats);
                // Apply after crowding, pulls and weapon knockback so these
                // enemies remain inside the camera's zoom-adjusted viewport.
                const float viewportLeft = ScreenToWorldX(0.0f);
                const float viewportTop = ScreenToWorldY(0.0f);
                const float viewportRight = ScreenToWorldX((float)screenWidth);
                const float viewportBottom = ScreenToWorldY((float)screenHeight);
                for (auto* m : g_MonsterManager.monsters)
                    if (m->alive) {
                        m->ConstrainGenesisToViewport(viewportLeft, viewportTop,
                                                      viewportRight, viewportBottom);
                        m->ConstrainGimbalToViewport(viewportLeft, viewportTop,
                                                     viewportRight, viewportBottom);
                        m->ConstrainRegulusToViewport(viewportLeft, viewportTop,
                                                      viewportRight, viewportBottom);
                        m->ConstrainLynxToViewport(viewportLeft, viewportTop,
                                                     viewportRight, viewportBottom, enemyDt);
                    }
                if (g_Bullets.size() > 2800) {
                    g_Bullets.erase(
                        std::remove_if(g_Bullets.begin(), g_Bullets.end(),
                            [](const Bullet& b){ return !b.active; }),
                        g_Bullets.end());
                    if (g_Bullets.size() > 2800)
                        g_Bullets.erase(g_Bullets.begin() + 2800, g_Bullets.end());
                }
                // ?�??무적 / 증강???�예(C14) ???�번 ?�텝?????�해 무효 (?�복?�??��?)
                ApplyPlayerDamageProtection(
                    hpAtStep, g_GameManager.playerHP,
                    IsPlayerDamageProtected());

                // Regular enemy kill rewards (KillRewards.h).
                KillRewardSink killSink{ g_GameManager.playerHP, g_GameManager.scoreAccum,
                                         g_GameManager.score, g_Stats, pCX, pCY };

                // ?�망 ??�� spawn
                //   + 분열�??�망 ???��? ?�식 2마리 (�?2?��?까�?)
                for (auto m : g_MonsterManager.monsters) {
                    if (!m->alive && !m->exploded) {
                        CollectMonsterKill(killSink, *m);
                        if (m->kind == MobKind::MIRA) mira::AddZone(m->worldX, m->worldY, mira::kDeathLife, mira::kZoneRadius, m->miraChain);   // its zone stays, joined to its trail
                        if (m->kind == MobKind::MIRA && !m->noBlast) Audio::Play(Audio::Cue::EnemyMiraDeath);
                        if (m->kind == MobKind::TRIANGULUM && !m->noBlast) Audio::Play(Audio::Cue::EnemyTrigonUnstable);
                        // A CAELUM that withdrew for NORMA still has health: no send-off.
                        if (m->kind == MobKind::CAELUM && m->hp <= 0.0f) PlayCaelumFall(m->worldX, m->worldY);
                        m->SpawnDeathEffect((float)now);
                        if (IsEliteMob(m->kind)) Audio::Play(Audio::Cue::EliteKill);
                        else if (!IsBossMob(m->kind)) Audio::Play(Audio::Cue::SfxKill);   // 사망 버스트
                        TriggerEliteKillHitStop(*m);
                    }
                }
                for (auto r : g_MonsterManager.rangedMobs) {
                    if (!r->alive && !r->exploded) {
                        CollectScopeKill(killSink, *r);
                        r->SpawnDeathEffect();
                        Audio::Play(Audio::Cue::SfxKill);
                    }
                }
                    const bool atMainCap = !g_CreativeMode
                                          && g_GameManager.playerLevel >= MAIN_LEVEL_CAP;
                    if (atMainCap) {
                        g_GameManager.xp = 0;
                    } else {
                    // Consume every level threshold available in the same
                    // fixed tick.  A large pickup must not leave the player
                    // permanently one level behind or discard XP.
                    constexpr int kMaxLevelUpsPerTick = 64;
                    int levelUps = 0;
                    // atMainCap is only the state before this tick; a large
                    // XP gain must still stop at the cap on the way up.
                    while (levelUps++ < kMaxLevelUpsPerTick &&
                           (g_CreativeMode ||
                            g_GameManager.playerLevel < MAIN_LEVEL_CAP)) {
                        const int oldLevel = g_GameManager.playerLevel;
                        const XpAmount oldXp = g_GameManager.xp;
                        if (!g_ExpSystem.ConsumeNextLevel(g_GameManager.xp,
                                                          g_GameManager.playerLevel)) break;
                        GrantLevelUpReward(oldLevel, oldXp - g_GameManager.xp,
                                           oldXp, LevelUpSource::EXPERIENCE);
                    }
                    } // !atMainCap

                // �ð� ���� (�޽ġ����� �߿��� ����)
                g_GameManager.AddScore(FIXED_DT * 100.0f);
            }
            accumulator -= FIXED_DT;
        }

        gamelog::SetClock(g_GameTime, g_GameManager.currentWave,
                          g_GameManager.playerHP, g_GameManager.maxHP);
        if (!gamelog::g_aliveFn) gamelog::g_aliveFn = &AliveMobCensus;
        {
            static GameState s_loggedState = GameState::MAIN_MENU;
            if (g_GameManager.currentState != s_loggedState) {
                static const char* const kNames[] = { "MAIN_MENU", "CREATIVE_CONFIG", "CODEX", "TUTORIAL",
                    "SETTINGS", "READY", "RUNNING", "PAUSED", "GAMEOVER", "AUG_SELECT", "DYING" };
                const int from = (int)s_loggedState, to = (int)g_GameManager.currentState;
                if (from >= 0 && from < 11 && to >= 0 && to < 11)
                    gamelog::Event("STATE", "from=%s,to=%s", kNames[from], kNames[to]);
                s_loggedState = g_GameManager.currentState;
            }
        }
        g_GameplayTelemetry.Update(
            g_GameTime,
            g_GameManager.playerLevel,
            g_GameManager.xp,
            g_ExpSystem.RequiredAmount(g_GameManager.playerLevel),
            g_Stats);

        // ?�?�??�맛: ?��?지 ?�자 / 콤보 / ?�래??�??�레??갱신 (게임 진행 중에�?감쇠) ?�?�?
        if (g_GameManager.currentState == GameState::RUNNING ||
            g_GameManager.currentState == GameState::DYING) {
            for (auto& d : g_DmgNumbers) {
                d.x  += d.vx * delta;
                d.y  += d.vy * delta;
                d.vy += 70.0f * delta;        // ?�한 중력 (?�구쳤다 처짐)
                d.life -= delta;
            }
            g_DmgNumbers.erase(std::remove_if(g_DmgNumbers.begin(), g_DmgNumbers.end(),
                [](const DamageNumber& d){ return d.life <= 0.0f; }), g_DmgNumbers.end());
            for (auto& w : g_WorldTexts) { w.y += w.vy * delta; w.vy *= std::max(0.0f, 1.0f - 1.6f * delta); w.life -= delta; }
            g_WorldTexts.erase(std::remove_if(g_WorldTexts.begin(), g_WorldTexts.end(),
                [](const WorldText& w){ return w.life <= 0.0f; }), g_WorldTexts.end());
            if (g_ComboTimer > 0.0f) {
                g_ComboTimer -= delta;
                if (g_ComboTimer <= 0.0f) g_Combo = 0;
            }
            g_ComboPulse -= delta * 4.0f;
            if (g_ComboPulse < 0.0f) g_ComboPulse = 0.0f;
            if (g_ComboMilestone > 0.0f) g_ComboMilestone -= delta;
        }
        if (g_FlashIntensity > 0.0f) {
            g_FlashIntensity -= delta * 3.5f;
            if (g_FlashIntensity < 0.0f) g_FlashIntensity = 0.0f;
        }
        if (g_GameManager.currentState == GameState::RUNNING &&
            g_GameManager.waveTransitionTimer > 0.0f) {
            g_GameManager.waveTransitionTimer = std::max(
                0.0f, g_GameManager.waveTransitionTimer - delta);
            if (g_GameManager.waveTransitionTimer <= 0.0f) Audio::Play(Audio::Cue::WaveStart);
        }
        if (g_GameManager.currentState == GameState::RUNNING &&
            g_GameManager.waveClearDelay > 0.0f) {
            g_GameManager.waveClearDelay = std::max(
                0.0f, g_GameManager.waveClearDelay - delta);
        }

        {
            GameState ws = g_GameManager.currentState;
            if (ws == GameState::RUNNING || ws == GameState::PAUSED ||
                ws == GameState::READY  || ws == GameState::AUG_SELECT ||
                ws == GameState::DYING) {
                SyncPlayerBoundsSize(playerWin, delta, ws == GameState::RUNNING);
            }
        }

        // Detect damage for the hit feedback. Non-combat screens synchronize
        // immediately so upgrades or resets cannot be mistaken for damage.
        {
            const GameState hpState = g_GameManager.currentState;
            const bool hpAnimActive = hpState == GameState::RUNNING ||
                                      hpState == GameState::DYING;
            const float hpNow = std::max(0.0f, g_GameManager.playerHP);
            const float hpNowFrac = std::clamp(hpNow / std::max(1.0f, g_Stats.maxHP), 0.0f, 1.0f);
            if (!hpAnimActive) {
                g_WinPrevHP = hpNow;
                g_HpGhost.Reset(hpNowFrac);
            } else {
                g_HpGhost.Update(hpNowFrac, delta);
                if (g_WinPrevHP < 0.0f) {
                    g_WinPrevHP = hpNow;
                }

                const float hpBefore = g_WinPrevHP;
                const float hpDelta = hpNow - hpBefore;
                if (g_PlayerDamagePulse > 0.0f) {
                    g_HpBarPop = std::max(g_HpBarPop, g_PlayerDamagePulse);
                    g_PlayerDamagePulse = 0.0f;
                }
                if (hpDelta < -0.0001f) {
                    g_HurtVignette = 0.5f;
                    g_HpBarPop = 2.2f;
                    if (hpNow > 0.0f) Audio::Play(Audio::Cue::Hurt);
                }
                g_WinPrevHP = hpNow;

                if (g_HurtVignette > 0.0f) {
                    g_HurtVignette -= delta * 1.6f;
                    if (g_HurtVignette < 0.0f) g_HurtVignette = 0.0f;
                }
                if (g_HpBarPop > 0.0f) {
                    g_HpBarPop -= delta;
                    if (g_HpBarPop < 0.0f) g_HpBarPop = 0.0f;
                }
            }
        }

        if (g_GameManager.currentState == GameState::RUNNING) {
            float pCX = playerWin.x + playerWin.width  * 0.5f;
            float pCY = playerWin.y + playerWin.height * 0.5f;
            bool  moving = AnyMoveHeld();

            // ?�?�??�격 ?��???HP 감소 ??빨간 비네?�만 (?�야 변???�거, �??�기 고정) ?�?�?
            // HP ���?(REGEN_UP, �Ŵ�ȭ, ���?II �� regenPerSec �ջ�)
            if (g_Stats.regenPerSec > 0.0f) {
                g_GameManager.playerHP += g_Stats.regenPerSec * delta;
                if (g_GameManager.playerHP > g_Stats.maxHP)
                    g_GameManager.playerHP = g_Stats.maxHP;
            }
            // 배드 ?�터 출혈 ??감속 구역 ?�이�?출혈 ?�?�머 2초로 갱신, 빠져?��????�류


            // ?�감 발견 ???�거�??�폭�?(존재?�면 발견 처리)
            for (const auto* ranged : g_MonsterManager.rangedMobs)
                if (ranged->alive)
                    MarkMobSeenId(CM_SCOPE);

            // ?�적 조건 체크 (???�수/??보유 증강 기�? ??보스 ?�적?�?처치 ?�점?�서 처리)
            if (PlayRecordsProgress()) {
                long long runScore = g_GameManager.score;
                long long runKills = g_Stats.killCount;
                if (runScore >= 300000)  TryUnlockAch(ACH_SCORE_300K);
                if (runScore >= 500000)  TryUnlockAch(ACH_SCORE_500K);
                if (runScore >= 1000000) TryUnlockAch(ACH_SCORE_1M);
                if (runKills >= 500)     TryUnlockAch(ACH_KILLS_500);
            }

            UpdateEnemyFx(delta);
            // Death animations advance inside MonsterManager::UpdateAll, on the
            // gameplay clock; advancing them here too ran them twice as fast.


            // 검�??�윙 ?�상 ?�데?�트


            // 별가루: 스폰 직후 약간 퍼진 뒤 매 프레임 플레이어 방향으로 재조향 — 무조건 수집.
            g_StardustHudPulse = std::max(0.0f, g_StardustHudPulse - delta * 3.8f);
            constexpr float STARDUST_SCATTER_SECONDS = 0.40f;
            constexpr float STARDUST_MIN_HOMING_SECONDS = 0.18f;
            constexpr float STARDUST_PICKUP_RADIUS = 24.0f;
            for (auto& dust : g_StardustPickups) {
                if (!dust.alive) continue;
                dust.age += delta;

                // 초기 0.4s: 퍼짐 (초기 vx/vy 방향 유지하되 감속)
                if (dust.age < STARDUST_SCATTER_SECONDS) {
                    const float drag = 1.0f - delta * 4.5f;
                    dust.vx *= drag;
                    dust.vy *= drag;
                } else {
                    // 이후: 플레이어 반경 100px 이내이거나 웨이브 클리어 상태면
                    // 추적을 시작(래치)하고, 아니면 제자리에 멈춰 있는다.
                    const float dx = pCX - dust.x, dy = pCY - dust.y;
                    const float dist = sqrtf(dx * dx + dy * dy);
                    if (!dust.homing &&
                        (g_StardustCollectAll || dist <= kStardustCollectRadius))
                        dust.homing = true;
                    if (dust.homing) {
                        dust.homeTime += delta;
                        // 웨이브 클리어 회수는 훨씬 빠르게 빨려 들어온다.
                        float speed = g_StardustCollectAll
                            ? 1500.0f + std::min(1500.0f, dust.homeTime * 3000.0f)
                            : 520.0f + std::min(280.0f, dust.homeTime * 400.0f);
                        if (delta > 0.0f) speed = std::min(speed, dist / delta);  // 지나치지 않게
                        if (dist > 0.001f) {
                            dust.vx = (dx / dist) * speed;
                            dust.vy = (dy / dist) * speed;
                        }
                    } else {
                        dust.vx = 0.0f;
                        dust.vy = 0.0f;
                    }
                }

                dust.x += dust.vx * delta;
                dust.y += dust.vy * delta;

                // Don't collect contact-range drops during scatter or on the
                // first homing frame; let the return flight remain visible.
                const float dx = pCX - dust.x, dy = pCY - dust.y;
                const float dist = sqrtf(dx * dx + dy * dy);
                if (dust.age >= STARDUST_SCATTER_SECONDS +
                                STARDUST_MIN_HOMING_SECONDS &&
                    dist < STARDUST_PICKUP_RADIUS) {
                    GrantExperience(dust.xpValue);
                    Audio::Play(Audio::Cue::Pickup);
                    if (PlayEarnsStardust()) {
                        AddSaveValue(g_Coins, dust.value);
                        AddSaveValue(g_RunStardust, dust.value);
                        // The first stardust ever picked up opens the lobby shop.
                        if (!g_ShopUnlocked) Audio::Play(Audio::Cue::ShopUnlock);
                        g_ShopUnlocked = true;
                    }
                    g_StardustHudPulse = 1.0f;
                    SpawnSparks(dust.x, dust.y, dust.value >= 5 ? 6 : 3,
                                1.0f, 0.85f, 0.30f, 180.0f);
                    dust.alive = false;
                }
            }
            g_StardustPickups.erase(std::remove_if(g_StardustPickups.begin(), g_StardustPickups.end(),
                [](const StardustPickup& d) { return !d.alive; }),
                g_StardustPickups.end());

            // ?��??�파???�데?�트 (?�동 + 감속 + ?�명)
            for (auto& sp : g_Sparks) {
                sp.x += sp.vx * delta;
                sp.y += sp.vy * delta;
                float drag = std::max(0.0f, 1.0f - 6.0f * delta);
                sp.vx *= drag; sp.vy *= drag;
                sp.life -= delta;
            }
            g_Sparks.erase(std::remove_if(g_Sparks.begin(), g_Sparks.end(),
                [](const Spark& s){ return s.life <= 0.0f; }), g_Sparks.end());
            norma_fx::Tick(delta);
            caelum_fx::Tick(delta);
            // ?�동 ?�상 ?�명 갱신
            for (auto& tr : g_Trail) {
                tr.x += tr.vx * delta;
                tr.y += tr.vy * delta;
                const float drag = std::max(0.0f, 1.0f - 3.0f * delta);
                tr.vx *= drag; tr.vy *= drag;
                tr.life -= delta;
            }
            g_Trail.erase(std::remove_if(g_Trail.begin(), g_Trail.end(),
                [](const Trail& t){ return t.life <= 0.0f; }), g_Trail.end());
            if (g_ShotKickTimer > 0.0f) g_ShotKickTimer -= delta;

            if (g_ShakeTime > 0.0f) g_ShakeTime -= delta;


            UpdateEnemySpawns(g_MonsterManager, g_Stats,
                              screenWidth, screenHeight, delta, g_GameTime,
                              g_GameManager.currentWave,
                              g_GameTime - g_GameManager.waveStartTime,
                              g_GameManager.score, g_Difficulty);
            g_GameTime += delta;
            ProcessWaveTransitions(screenWidth, screenHeight);
            TickNormaJudgment(delta);
            if (NormaDefeated()) CompleteBossRun(FinalWaveCount(g_Difficulty));

            // Fire bullets and cache the active damage modifiers.
            // ?�탑 모드: ?�레?�어 ?�동 발사 비활??(?�탑???�??발사)
            float effInterval = passive::FireInterval(g_Stats.fireInterval) *
                                GravisField::FireIntervalMult(GravisField::g_playerInfluence);
            float effSpeed    = g_Stats.bulletSpeed;
            fireTimer += delta;
            auto spawnOne = [&](float tx, float ty) {
                float ftx = tx, fty = ty;
                if (g_Stats.bulletSpread > 0.0f) {
                    float dx = tx - pCX, dy = ty - pCY;
                    float ang = atan2f(dy, dx);
                    float jitter = ((float)(rand() % 200) - 100.0f) / 100.0f
                                 * g_Stats.bulletSpread;
                    ang += jitter;
                    ftx = pCX + cosf(ang) * 200.0f;
                    fty = pCY + sinf(ang) * 200.0f;
                }
                Bullet nb(pCX, pCY, ftx, fty);
                nb.speed = effSpeed;
                passive::ModifyShot(nb);
                static std::vector<Bullet> volley;
                syn::Shape(nb, pCX, pCY, tx, ty, volley);
                for (const Bullet& vb : volley) {
                    g_Bullets.push_back(vb);
                    active::OnPlayerBullet(vb);   // Twin Star Clone copies the shot
                }
            };

            auto spawnAimed = [&](float tx, float ty) {
                TriggerShotKick();
                Audio::Play(Audio::Cue::Shoot);
                spawnOne(tx, ty);
                passive::OnShot();
                gamelog::Shot();
            };

            auto aimTarget = [&](float& tx, float& ty) -> bool {
                if (lmb && !g_SmokeFire) { tx = wmx; ty = wmy; return true; }
                return g_MonsterManager.FindNearestEnemy(pCX, pCY, tx, ty);
            };

            bool fireHeld = lmb && (g_PlayerRuntime.postPickGrace <= 0.0f);
            if (fireHeld && fireTimer >= effInterval) {
                float tx, ty;
                if (aimTarget(tx, ty)) {
                    spawnAimed(tx, ty);
                    fireTimer = 0.0f;
                }
            }
        }

        // GameManager 가 ?�버/변???�태�??????�게 ?�기??(Render ?�서 ?�용)


        g_GameManager.hoveredCard = g_HoveredAug;
        const bool debugCaptureOverride = g_DebugToolkit.CaptureInProgress();
        const GameState debugRestoreState = g_GameManager.currentState;
        if (debugCaptureOverride) {
            // Render each requested scene through the normal dispatch while
            // keeping the real run state untouched between capture frames.
            g_GameManager.currentState =
                g_DebugToolkit.CaptureRenderState(debugRestoreState);
            lmb = false;
            mx = -10000.0;
            my = -10000.0;
        }
        // ============================================================
        // ?�더�?        // ============================================================
        // The tutorial card screen is drawn mini-sized inside the
        // practice field: the rest of the frame treats the state as READY and
        // the real state is put back at the end of the frame.
        GameState tutRealState = g_GameManager.currentState;
        const bool tutSelectionRender = ready_launch::g_TutorialMode &&
            (tutRealState == GameState::AUG_SELECT);
        ready_launch::g_SelectionUp = tutSelectionRender;
        if (tutSelectionRender) g_GameManager.currentState = GameState::READY;
        SetFullViewport();
        // The pane prototype hides the desktop so its screen captures stay private.
        glClearColor(0.0f, 0.0f, 0.0f, g_PaneDebug ? 1.0f : 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(g_MainShader);
        glUniform1i(g_MainFxLoc, g_ShaderFx ? 1 : 0);   // CRT ?�이???�과 ?��? (G)
        // ?�면 ?�들�?+ �??�용 ??game world �? HUD/text(별도 ortho)???�향 ?�음
        float orthoShake[16];
        memcpy(orthoShake, g_BaseOrtho, sizeof(g_BaseOrtho));
        // �?중심 = ZCX/ZCY, 기본 ?�면 중앙). z=1 ?�면 base ortho ?�??�일(identity)
        {
            float z = g_ViewZoom;
            float zcx = ZCX(), zcy = ZCY();
            orthoShake[0]  =  2.0f * z / (float)screenWidth;
            orthoShake[5]  = -2.0f * z / (float)screenHeight;
            orthoShake[12] =  2.0f * zcx * (1.0f - z) / (float)screenWidth  - 1.0f;
            orthoShake[13] =  1.0f - 2.0f * zcy * (1.0f - z) / (float)screenHeight;
        }
        // 게임?�레???�망?�출 ???�태(메뉴·게임?�버 ???�선 ?�면 ?�들�??�여 ?�거
        //   ???�망 ???�작창으�?갔을 ???�면??계속 ?�리??문제 방�?
        {
            GameState gs = g_GameManager.currentState;
            if (gs != GameState::RUNNING && gs != GameState::DYING &&
                gs != GameState::PAUSED && gs != GameState::AUG_SELECT) {
                g_ShakeTime = 0.0f; g_ShakeMag = 0.0f;
            }
        }
        // Shake only decays while the game clock runs, so a paused or
        // choosing screen must not keep rolling the dice for the held value.
        const GameState shakeState = g_GameManager.currentState;
        if (g_ShakeTime > 0.0f &&
            (shakeState == GameState::RUNNING || shakeState == GameState::DYING)) {
            float intensity = g_ShakeMag * (g_ShakeTime / 0.6f);
            if (intensity > g_ShakeMag) intensity = g_ShakeMag;
            float sx = ((float)(rand() % 200) - 100.0f) / 100.0f * intensity;
            float sy = ((float)(rand() % 200) - 100.0f) / 100.0f * intensity;
            orthoShake[12] -= 2.0f * sx / (float)screenWidth;
            orthoShake[13] += 2.0f * sy / (float)screenHeight;
        }
        {   // ease the menu darkness between screens (menu <-> in-run)
            static double s_dimLastT = 0.0;
            const double dimNow = glfwGetTime();
            const GameState ds = g_GameManager.currentState;
            TickMenuDim(ds == GameState::RUNNING || ds == GameState::DYING ||
                        ds == GameState::PAUSED || ds == GameState::READY ||
                        ds == GameState::AUG_SELECT,
                        // the tutorial page; the launch swell eases it to combat
                        ready_launch::g_TutorialMode && !ready_launch::Launching(),
                        (float)(dimNow - s_dimLastT));
            s_dimLastT = dimNow;
        }
        if (g_UiShakePx > 0.0f) {
            const float ux = ((float)(rand() % 200) - 100.0f) / 100.0f * g_UiShakePx;
            const float uy = ((float)(rand() % 200) - 100.0f) / 100.0f * g_UiShakePx;
            orthoShake[12] -= 2.0f * ux / (float)screenWidth;
            orthoShake[13] += 2.0f * uy / (float)screenHeight;
            g_UiShakePx = 0.0f;   // the page asks again next frame while it still needs it
        }
        BatchFlush();   // ortho(�? 바꾸�??????�전 매트�?���??�인 ?�형 먼�? 그림
        glUniformMatrix4fv(g_MainProjLoc, 1, GL_FALSE, orthoShake);
        // 글로벌?�도 ?�기????BindMainShader() 가 ??�??�용
        memcpy(g_MainOrtho, orthoShake, sizeof(orthoShake));
        glBindVertexArray(g_MainVAO);

        // ?�드/?�티???�더??게임?�레???�태?�서�?그린????메뉴(?�작�?????        //   직전 게임???�레?�어 창·잔???�티?��? ?��? ?�태�?비치??문제 방�?.
        //   (g_MainOrtho ???�에???��? 갱신?�으므�?메뉴 ?�스???�모 ?�더???�상)
        {
        GameState wgs = g_GameManager.currentState;
        bool inWorldRender = (wgs == GameState::RUNNING || wgs == GameState::DYING ||
                              wgs == GameState::PAUSED  || wgs == GameState::READY  ||
                              wgs == GameState::AUG_SELECT ||

                              wgs == GameState::GAMEOVER ||
                              (wgs == GameState::SETTINGS &&
                               g_SettingsReturnTo == GameState::PAUSED));
        if (inWorldRender) {

        EnsurePlayerBounds(playerWin);

        // Screen-edge contrast for the HUD. Keep the combat center clear and
        // darken only the perimeter beneath entities, projectiles, and HUD.
        BatchFlush();
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glUniformMatrix4fv(g_MainProjLoc, 1, GL_FALSE, g_BaseOrtho);
        memcpy(g_MainOrtho, g_BaseOrtho, sizeof(g_BaseOrtho));

        constexpr int kVignetteSteps = 24;
        constexpr float kEdgeAlpha = 0.16f;
        const float sideW = std::min(180.0f, std::max(110.0f, screenWidth * 0.09f));
        const float topH = std::min(220.0f, std::max(140.0f, screenHeight * 0.17f));
        for (int i = 0; i < kVignetteSteps; ++i) {
            const float t = (i + 0.5f) / (float)kVignetteSteps;
            const float fade = 1.0f - t * t * (3.0f - 2.0f * t);
            const float a = kEdgeAlpha * fade * ready_launch::g_RunIntro;
            const float x0 = sideW * i / (float)kVignetteSteps;
            const float x1 = sideW * (i + 1) / (float)kVignetteSteps;
            const float y0 = topH * i / (float)kVignetteSteps;
            const float y1 = topH * (i + 1) / (float)kVignetteSteps;
            drawRect(x0, 0.0f, x1 - x0, (float)screenHeight, 0.0f, 0.0f, 0.0f, a);
            drawRect((float)screenWidth - x1, 0.0f, x1 - x0,
                     (float)screenHeight, 0.0f, 0.0f, 0.0f, a);
            drawRect(0.0f, y0, (float)screenWidth, y1 - y0, 0.0f, 0.0f, 0.0f, a);
            drawRect(0.0f, (float)screenHeight - y1, (float)screenWidth,
                     y1 - y0, 0.0f, 0.0f, 0.0f, a);
        }
        // Menu darkness setting: combat and the other in-run screens take 20%
        // of it. Drawn with the vignette, under entities and the HUD.
        {
            const float combatDim = g_CombatDimLive;
            if (combatDim > 0.002f)
                drawRect(0.0f, 0.0f, (float)screenWidth, (float)screenHeight,
                         0.0f, 0.0f, 0.0f, combatDim);
        }
        BatchFlush();
        glUniformMatrix4fv(g_MainProjLoc, 1, GL_FALSE, orthoShake);
        memcpy(g_MainOrtho, orthoShake, sizeof(orthoShake));

        // (b) Global combat pass: every entity draws straight to the scene
        // (no per-window scissor passes).
        BatchFlush();
        BindMainShader();
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);

        // Rear and front sight fields stay before entity bodies to preserve
        // the existing combat render order.
        // CAELUM's corrupted cells lie under everything else it draws.
        {
            bool caelumAlive = false;
            for (const Monster* cm : g_MonsterManager.monsters)
                if (cm->kind == MobKind::CAELUM && cm->alive) { caelumAlive = true; break; }
            caelum_draw::BeginFrame(caelumAlive, (float)screenWidth, (float)screenHeight);
            caelum_draw::DrawCorruptionLayer();
        }
        const float sightPlayerX = playerWin.x + playerWin.width * 0.5f;
        const float sightPlayerY = playerWin.y + playerWin.height * 0.5f;
        const float sightPlayerSize = PLAYER_SIZE * g_Stats.playerSizeMult;
        DrawCombatSightFields(
            g_MonsterManager, sightPlayerX, sightPlayerY, sightPlayerSize,
            g_GameManager.currentState != GameState::DYING &&
                g_GameManager.currentState != GameState::GAMEOVER &&
                g_GameManager.currentState != GameState::READY &&   // no world ship on READY
                !TutorialPaused());

        DrawRegulusBeams(g_MonsterManager, g_GameTime);

        // Mobs in another window space than the player read dim (lines
        // stay, only brightness drops), so the player sees who can reach them.
        const float spaceBaseAlpha = g_BatchAlpha;
        const auto dimIfElsewhere = [&](float x, float y) {
            g_BatchAlpha = space::Active() && space::SpaceAt(x, y) != space::g_PlayerSpace
                ? spaceBaseAlpha * 0.35f : spaceBaseAlpha;
        };
        DrawMiraZones();   // under the bodies, so the cores stay readable
        // Lynx aim lines first, apart from the bodies: a telegraph must stay
        // readable under the Regulus stealth buff.
        for (auto m : g_MonsterManager.monsters) {
            if (!m->alive || m->kind != MobKind::LYNX) continue;
            dimIfElsewhere(m->worldX, m->worldY);
            DrawLynxRay(m);
        }
        for (auto m : g_MonsterManager.monsters) {
            if (!(m->alive || m->DeathEffectVisible())) continue;
            dimIfElsewhere(m->worldX, m->worldY);
            const float mobAlpha = g_BatchAlpha;
            g_BatchAlpha *= m->RegulusCloakAlpha();   // Regulus stealth buff
            drawMob(m, g_GameTime);
            g_BatchAlpha = mobAlpha;
            if (m->alive) DrawRegulusBuffMarks(m, g_GameTime);
        }
        for (auto r : g_MonsterManager.rangedMobs) {
            dimIfElsewhere(r->worldX, r->worldY);
            drawRangedMob(r, g_GameTime);
        }
        g_BatchAlpha = spaceBaseAlpha;
        caelum_draw::DrawOverlay(g_GameTime);
        DrawBulletHalos(g_Bullets);
        for (auto& b : g_Bullets) {
            if (b.active) drawBullet(b);
        }
        for (int i = 0; i < EnemyParticleCap(); ++i) {
            auto& p = g_EnemyParts[i];
            if (!p.active || p.delay > 0.0f) continue;
            float a = p.life / p.maxLife;
            float hs = p.size * 0.5f;
            switch (p.shape) {
            case EnemyParticleShape::CIRCLE:
                drawCircle(p.x, p.y, hs, p.r, p.g, p.b, a);
                break;
            case EnemyParticleShape::TRIANGLE: {
                const float heading = std::atan2(p.vy, p.vx);
                const float radius = p.size * 0.58f;
                const float a0 = heading - 1.5707963f;
                const float a1 = a0 + 2.0943951f;
                const float a2 = a1 + 2.0943951f;
                BatchTri(p.x + std::cos(a0) * radius,
                         p.y + std::sin(a0) * radius,
                         p.x + std::cos(a1) * radius,
                         p.y + std::sin(a1) * radius,
                         p.x + std::cos(a2) * radius,
                         p.y + std::sin(a2) * radius, p.r, p.g, p.b, a);
                break;
            }
            case EnemyParticleShape::SQUARE:
            default:
                drawRect(p.x - hs, p.y - hs, p.size, p.size,
                         p.r, p.g, p.b, a);
                break;
            }
        }
        DrawCombatFx();
        norma_fx::Draw();
        caelum_fx::Draw();
        DrawStardustPickups();
        active::DrawWorld(g_GameTime);
        syn::DrawWorld();
        BatchFlush();

        // READY draws its own ship inside the practice field (SceneReady.cpp).
        if (g_GameManager.currentState != GameState::DYING &&
            g_GameManager.currentState != GameState::GAMEOVER &&
            g_GameManager.currentState != GameState::READY &&
            !TutorialPaused()) {
            const float pCX = playerWin.x + playerWin.width * 0.5f;
            const float pCY = playerWin.y + playerWin.height * 0.5f;
            const float playerSize = PLAYER_SIZE * g_Stats.playerSizeMult;
            BindMainShader();
            DrawPlayerTrail();
            const GameState healthState = g_GameManager.currentState;
            if (healthState == GameState::RUNNING ||
                (healthState == GameState::PAUSED && !TutorialPaused()) ||
                healthState == GameState::AUG_SELECT) {
                const float hpFraction = std::clamp(g_GameManager.playerHP /
                    std::max(1.0f, g_Stats.maxHP), 0.0f, 1.0f);
                const float hpHit = std::min(1.0f, g_HpBarPop / 0.35f);
                // Keep the blue health arc readable even at full HP.
                const float hpRest = hpFraction >= 1.0f ? 0.85f : 0.92f;
                vitals_hud::DrawHpArc(pCX, pCY, playerSize, hpFraction,
                                      g_HpGhost.frac,
                                      std::min(1.0f, hpRest + 0.30f * hpHit),
                                      g_GameTime);
                vitals_hud::DrawShieldArc(pCX, pCY, playerSize,
                    (g_PlayerShield + passive::PlateShieldLeft()) / std::max(1.0f, g_Stats.maxHP),
                    g_GameTime);
            }
            DrawPlayerWeaponShell(pCX, pCY, playerSize,
                                  atan2f(wmy - pCY, wmx - pCX), g_GameTime);
            DrawLevelUpRings(pCX, pCY, playerSize,
                             g_GameManager.levelUpTransition.Progress());
            BatchFlush();
        }
        caelum_draw::DrawCursor(g_GameTime);   // over the player

        // (c) Player bounds stay a plain gameplay rectangle; nothing is drawn for them.
        BatchFlush(); glDisable(GL_SCISSOR_TEST); glEnable(GL_BLEND);
        EnsurePlayerBounds(playerWin);

        BatchFlush();   // Restore the base orthographic projection for the HUD.
        glUniformMatrix4fv(g_MainProjLoc, 1, GL_FALSE, g_BaseOrtho);
        memcpy(g_MainOrtho, g_BaseOrtho, sizeof(g_BaseOrtho));









        // (h3) ?�면 ?�래?????�벨??보스처치/부???�간 번쩍
        if (g_FlashIntensity > 0.001f) {
            BindMainShader();
            float a = g_FlashIntensity; if (a > 0.85f) a = 0.85f;
            drawRect(0, 0, (float)screenWidth, (float)screenHeight,
                     g_FlashColor.r, g_FlashColor.g, g_FlashColor.b, a);
        }
        // FORK.worm ?�피 ???��???RGB 분리 글리치



        // (h5) ?�격 ??빨간 가?�자�?비네??(?�격 ?��?
        if (g_HurtVignette > 0.001f) {
            const float t = std::clamp(g_HurtVignette / 0.5f, 0.0f, 1.0f);
            DrawEdgeVignette(52.0f * t + 8.0f, 0.20f * t,
                             0.95f, 0.10f, 0.10f);
        }


        BatchFlush();
        glDisable(GL_SCISSOR_TEST);

        }   // if (inWorldRender)
        }   // world render game block

        // [6] HUD
        g_GameManager.Render();

        // ?�?�?[7] ?�국???�스??+ 메뉴 ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
        {
            float sw = (float)screenWidth, sh = (float)screenHeight;
            auto  st = g_GameManager.currentState;

            // ���� �� ���̵� ������Ʈ ��������������������������������������������������������������������������������
            if (g_FadeDir == 1) {               // ���̵� �ƿ� (��������)
                g_FadeAlpha += delta / FADE_OUT_DUR;
                if (g_FadeAlpha >= 1.0f) {
                    g_FadeAlpha = 1.0f;
                    g_GameManager.currentState = g_FadeTarget;
                    st = g_FadeTarget;
                    g_AppOpen = 0.0f;
                    g_FadeDir = -1;
                }
            } else if (g_FadeDir == -1) {       // ���̵� �� (���� �� ����)
                g_FadeAlpha -= delta / FADE_IN_DUR;
                if (g_FadeAlpha <= 0.0f) {
                    g_FadeAlpha = 0.0f;
                    g_FadeDir   = 0;
                }
            }


            // ??�?shop/codex/config) 진입 ???�림 ?�니메이???�작 ??�??�레??진행
            {
                static GameState s_prevWinSt = GameState::MAIN_MENU;
                bool isApp = (st == GameState::CODEX ||
                              st == GameState::TUTORIAL || st == GameState::SETTINGS);
                if (st != s_prevWinSt) {
                    if (isApp) g_AppOpen = 0.0f;   // ??�????�기 0?�서 ?�기
                    s_prevWinSt = st;
                }
                if (isApp && g_AppOpen < 1.0f) {
                    g_AppOpen += delta / APP_OPEN_DUR;
                    if (g_AppOpen > 1.0f) g_AppOpen = 1.0f;
                }
            }

            // ?�?�??�게???�업?�시�???메뉴?�??�일??OS ?�레???��? (?�스?�톱 방어 ?��??? ?�?�?
            // ?�?�??�적 ?�금 ?�스??(?�단 중앙 배너, 4�??�시 ???�이?? ?�?�?
            DrawAchievementToast(sw, sh, delta);

            DrawModeLabels(st, sw, sh);

            // ?�?�?[7b] UI ???�스?�치 ??메뉴/�??�태??Scene_* ?�수�?분리 ?�?�?
            //    RUNNING/DYING(?�수 ?�게?????�이 ?�으??컨텍?�트 구성 ?�체�?건너?�?
            UpdateGameTextHalo(screenHeight);

            // Scene_* dispatch still includes READY and the in-run menus;
            // this flag only controls which state gets the scene composition.
            const bool outgameBackdrop =
                st != GameState::RUNNING && st != GameState::DYING;
            if (outgameBackdrop) {
                std::function<void()> resetFn = ResetForNewGame;
                std::function<void()> restartRunFn = RestartCurrentRun;
                std::function<void()> abandonRunFn = AbandonCurrentRun;
                // ������ ũ�� ����(���?64px) ���� Ŭ���� ���� �������� ����
                bool sceneLmb = (!g_DebugToolkit.IsInputCaptured() &&
                                 my >= BROWSER_CHROME_H) ? lmb : false;
                SceneCtx ctx{ sw, sh, mx, my, sceneLmb, uiDelta, window, &fireTimer,
                              resetFn, restartRunFn, abandonRunFn,
                              inputFocusChanged };
                const SceneTextContext sceneText{ g_TextL, g_TextS };
                switch (st) {
                case GameState::MAIN_MENU:         Scene_MainMenu(ctx);         break;
                case GameState::CODEX:             Scene_Codex(ctx);            break;
                case GameState::TUTORIAL: {
                    const TutorialSceneContext tutorialState{
                        g_GameManager.currentState,
                        g_LmbPrev,
                        g_AppOpen,
                        sceneText
                    };
                    Scene_Tutorial(ctx, tutorialState);
                    break;
                }
                case GameState::CREATIVE_CONFIG:   Scene_CreativeConfig(ctx);   break;
                case GameState::SETTINGS:          Scene_Settings(ctx);         break;
                case GameState::READY:
                    Scene_Ready(ctx, sceneText);
                    if (tutSelectionRender) {
                        // The card screen keeps its full-screen layout; the
                        // viewport squeezes it into the practice field and the
                        // pointer is mapped into that space.
                        float fx, fy, fw, fh;
                        ready_launch::FieldRect(sw, sh, fx, fy, fw, fh);
                        // The scene sees a window of field / s (16:9, like the
                        // field) at its top-left; the viewport maps that back to
                        // the field and everything outside it is clipped.
                        const float s = ready_launch::kCardScreenScale;
                        const float k = g_PixelScale;
                        const float vglY = ((float)screenHeight - (fy + sh * s)) * k;
                        glViewport((GLint)(fx * k), (GLint)vglY,
                                   (GLsizei)(sw * s * k), (GLsizei)(sh * s * k));
                        g_ViewportMap = { fx * k, vglY, s * k };
                        glEnable(GL_SCISSOR_TEST);
                        glScissor((GLint)(fx * k), (GLint)(((float)screenHeight - (fy + fh)) * k),
                                  (GLsizei)(fw * k), (GLsizei)(fh * k));
                        SceneCtx mini = ctx;
                        mini.sw = fw / s;
                        mini.sh = fh / s;
                        mini.mx = (mx - fx) / s;
                        mini.my = (my - fy) / s;
                        g_GameManager.currentState = tutRealState;
                        Scene_AugSelect(mini);
                        tutRealState = g_GameManager.currentState;
                        g_GameManager.currentState = GameState::READY;
                        glDisable(GL_SCISSOR_TEST);
                        SetFullViewport();
                    }
                    break;
                // Pause is drawn after the HUD and version so its veil dims
                // the entire gameplay view, not just the world geometry.
                case GameState::PAUSED:
                    // The tutorial's pause menu sits over its (frozen) practice page.
                    if (TutorialPaused()) Scene_Ready(ctx, sceneText);
                    break;
                case GameState::GAMEOVER: {
                    const GameOverSceneContext reportState{
                        g_GameManager.currentState,
                        g_GameOverFade,
                        g_LmbPrev,
                        g_DeathReason,
                        g_GameManager.waveVictory,
                        g_LastRunRecord,
                        g_GameManager.score,
                        g_GameManager.playerLevel,
                        g_Stats.killCount,
                        sceneText
                    };
                    Scene_GameOver(ctx, reportState);
                    break;
                }
                case GameState::AUG_SELECT:        Scene_AugSelect(ctx);        break;
                default: break;
                }
            }
            // Refresh the cursor after scene actions change the state.
            InputUpdateGameplayCursor(window, g_GameManager.currentState,
                                      g_ShowCrosshair, g_DebugToolkit.IsVisible());

            // ?�단 HUD ???�제 게임 진행 ?�태?�서�?(메뉴/?�감/?�점?????�게)
            // The HUD fades in over the first moments of a run (tutorial launch).
            const float hudAlphaKeep = g_BatchAlpha;
            g_BatchAlpha *= ready_launch::g_RunIntro;
            if ((st == GameState::RUNNING || st == GameState::PAUSED ||
                 st == GameState::DYING   || st == GameState::AUG_SELECT) && !TutorialPaused()) {
                const float hudScale = UiScale(sw, sh);
                if (st == GameState::RUNNING) {
                    const float hpFrac = g_Stats.maxHP > 0.0f
                        ? g_GameManager.playerHP / g_Stats.maxHP : 1.0f;
                    if (hpFrac > 0.0f && hpFrac < 0.25f) {
                        const float pulse = 0.5f + 0.5f * sinf(g_GameTime * 6.0f);
                        const float severity = 1.0f - hpFrac / 0.25f;
                        const float alpha = (0.12f + 0.22f * pulse)
                                          * (0.6f + 0.4f * severity);
                        DrawEdgeVignette(72.0f * hudScale, alpha * 0.85f,
                                         0.92f, 0.08f, 0.10f);
                    }
                }
#ifdef __APPLE__
                const float hudTopY = BROWSER_CHROME_H + 8.0f + 30.0f;
#else
                const float hudTopY = BROWSER_CHROME_H + 4.0f;
#endif
                // Level + XP: one line along the bottom edge, stopping short
                // of the version label in the bottom-right corner.
                if (st != GameState::DYING) {
                    wchar_t lvBuf2[64];
                    swprintf_s(lvBuf2, L"%ls%d",
                               T(StrId::LV_PREFIX), g_GameManager.playerLevel);
                    const bool xpAtCap = !g_CreativeMode &&
                        g_GameManager.playerLevel >= MAIN_LEVEL_CAP;
                    const float xpFrac = vitals_hud::XpFill(g_GameManager.xp,
                        g_ExpSystem.RequiredAmount(g_GameManager.playerLevel), xpAtCap);
                    static const std::wstring xpVersionLabel = std::wstring(L"v") + ASTERIA_VERSION;
                    const float versionScale = vitals_hud::BottomHudVersionScale(hudScale);
                    const float xpRightX = sw - vitals_hud::BottomHudPadding(hudScale)
                        - g_TextS.Width(xpVersionLabel.c_str(), versionScale) - vitals_hud::BottomHudGap(hudScale);
                    vitals_hud::DrawXpEdgeLine(sh, hudScale, xpFrac, xpAtCap, lvBuf2,
                        g_GameManager.levelUpTransition.Progress(), xpRightX);

                    // HP arc text: LOW HP beside the arc, numbers after a hit.
                    const float hpFrac = std::clamp(g_GameManager.playerHP /
                        std::max(1.0f, g_Stats.maxHP), 0.0f, 1.0f);
                    vitals_hud::DrawHpArcText(playerWin.x + playerWin.width * 0.5f,
                        playerWin.y + playerWin.height * 0.5f,
                        PLAYER_SIZE * g_Stats.playerSizeMult, hpFrac,
                        g_GameManager.playerHP, g_Stats.maxHP,
                        std::min(1.0f, g_HpBarPop / 0.6f) * 0.9f, hudScale, g_GameTime,
                        st == GameState::RUNNING || st == GameState::PAUSED);
                }
                DrawWaveHud(sw, hudScale, hudTopY, g_GameTime, g_Difficulty, g_GameManager);
                DrawNormaBossBar(sw, hudScale, hudTopY);
                DrawEliteBar(sw, hudScale, hudTopY);
                DrawCrashCountdown(sw, hudScale, hudTopY);

                // ?�상?? FPS
                wchar_t fpsBuf[32];
                swprintf_s(fpsBuf, L"%ls  %d", T(StrId::FPS), g_CurrentFPS);
                const float fpsScale = UiTextScale(g_TextS, UiTextLevel::Supporting, hudScale);
                float fpsW = g_TextS.Width(fpsBuf, fpsScale);
                g_TextS.Draw(fpsBuf, sw - fpsW - 12.0f, hudTopY, fpsScale,
                             0.7f, 0.9f, 1.0f, 0.85f);



                // (HP/EXP ?�각 바는 ?�레?�어 �??�단??부착됨 ????(c2) 참고)
                // ?�단 조작 ?�내 (?��??�게 ??��) ?????�레?�어가 HP/?�킬 ?�치�??�게
            }

            // ?�?�??�맛: ?��?지 ?�자 ?�업 + 콤보 카운??(?�제 ?�레??중에�? ?�?�?
            //    메뉴/?�시?��??�선 ?��? ???�스?��? 메뉴 ?�에 ?�던 버그 fix
            DrawFloatingCombatText(st, sw, sh);

            // Skill HUD (bottom centre): the dash cooldown.
            if (st == GameState::RUNNING ||
                (st == GameState::PAUSED && !TutorialPaused())) {
                DrawSkillHud(skill_hud::BuildSkillHudView(GatherSkillHudInputs(), g_SkillHudPulse),
                             UiScale(sw, sh), sw, sh);
            } else if (st == GameState::AUG_SELECT && !ready_launch::g_TutorialMode &&
                       !(g_GameManager.augmentRewardActive && g_GameManager.PendingRewards() > 1)) {
                // Closing pick that returns to combat: the skill row fades back in under it.
                const float back = aug_card::HudReturn(g_AugExitT);
                if (back > 0.004f) {
                    const float keep = g_BatchAlpha;
                    g_BatchAlpha *= back;
                    DrawSkillHud(skill_hud::BuildSkillHudView(GatherSkillHudInputs(), g_SkillHudPulse),
                                 UiScale(sw, sh), sw, sh);
                    g_BatchAlpha = keep;
                }
            }
            g_BatchAlpha = hudAlphaKeep;

        }

        if (!g_DebugToolkit.SuppressOverlayForCapture()) {
            BindMainShader();
            static const std::wstring versionLabel = std::wstring(L"v") + ASTERIA_VERSION;
            const float versionUiScale = UiScale((float)screenWidth, (float)screenHeight);
            const float versionScale = vitals_hud::BottomHudVersionScale(versionUiScale);
            const float versionPad = vitals_hud::BottomHudPadding(versionUiScale);
            g_TextS.Draw(versionLabel.c_str(),
                         screenWidth - versionPad - g_TextS.Width(versionLabel.c_str(), versionScale),
                         HudY((float)screenHeight, versionPad) - g_TextS.Height(versionLabel.c_str(), versionScale),
                         versionScale, 0.62f, 0.76f, 0.88f, 0.92f);
        }

        // Pane marks stay off the augment screens, which cover the field.
        {
            const GameState bs = g_GameManager.currentState;
            if (bs != GameState::AUG_SELECT)
                DrawPaneBrackets();
        }
        if (g_PaneDebug) DrawPaneDebug();

        if (g_GameManager.currentState == GameState::PAUSED) {
            const bool sceneLmb = !g_DebugToolkit.IsInputCaptured() &&
                                  my >= BROWSER_CHROME_H && lmb;
            const SceneCtx ctx{ (float)screenWidth, (float)screenHeight, mx, my,
                                sceneLmb, uiDelta, window, &fireTimer,
                                ResetForNewGame, RestartCurrentRun, AbandonCurrentRun,
                                inputFocusChanged };
            const SceneTextContext sceneText{ g_TextL, g_TextS };
            const PauseSceneContext pauseState{
                g_GameManager.currentState, g_GameManager.pauseResumeState,
                g_SettingsReturnTo, g_LmbPrev,
                sceneText, ResetSettingsUi
            };
            Scene_Paused(ctx, pauseState);
            InputUpdateGameplayCursor(window, g_GameManager.currentState,
                                      g_ShowCrosshair, g_DebugToolkit.IsVisible());
        }

        if (!g_DebugToolkit.SuppressOverlayForCapture())
            g_DebugToolkit.Render((float)screenWidth, (float)screenHeight,
                                  g_GameManager.currentState);

        const GameState crosshairState = g_GameManager.currentState;
        if (g_ShowCrosshair && !g_DebugToolkit.IsVisible() &&
            (crosshairState == GameState::RUNNING ||
             crosshairState == GameState::DYING) &&
            glfwGetWindowAttrib(window, GLFW_FOCUSED) &&
            mx >= 0.0 && my >= 0.0 && mx < screenWidth && my < screenHeight) {
            BindMainShader();
            DrawGameplayCrosshair((float)mx, (float)my);
        }

        // OCTANS' blue screen covers everything the game drew, HUD included.
        DrawCrashScreen((float)screenWidth, (float)screenHeight,
                        UiScale((float)screenWidth, (float)screenHeight));

        // Keep the physical mouse state for edge detection. `lmb` may be
        // cleared above when the debug toolkit captures input; storing that
        // cleared value makes a held button look like a new click every frame.
        g_LmbPrev = rawLmb;
        g_RmbPrev = rawRmb;

        // ?�적 ?�금 / ?�감 발견 발생 ???�??(게임 �?즉시 ?�구??
        if (g_AchSaveNeeded || g_CodexDirty) {
            SaveGame(); g_AchSaveNeeded = false; g_CodexDirty = false;
        }

        BatchFlush();   // ?�레??마�?�????��? ?�형 모두 그림
        if (g_DebugToolkit.SuppressOverlayForCapture())
            g_DebugToolkit.CaptureFrameAfterRender(
                g_FramebufferW, g_FramebufferH, g_GameManager.currentState);
        if (tutSelectionRender && g_GameManager.currentState == GameState::READY)
            g_GameManager.currentState = tutRealState;
        glfwSwapBuffers(window);
        if (debugCaptureOverride)
            g_GameManager.currentState = debugRestoreState;

        // ?�?�?FPS �?(g_FpsCap > 0 ???�만) ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?
        // timeBeginPeriod(1) �?Sleep ?�상??1ms. 마�?�?~1ms ??busy-wait
        // C18: 과거 '무제??(-1) ?�이브는 300 ?�로 ?�램??(진짜 무제???�거).
        int capFps = (g_FpsCap < 0) ? 300 : g_FpsCap;
        if (capFps > 0) {
            double target = 1.0 / (double)capFps;
            double frameStart = nowPrecise;   // the float `now` loses precision late in a session
            double remain = target - (glfwGetTime() - frameStart);
            if (remain > 0.001) {
                unsigned ms = (unsigned)((remain - 0.001) * 1000.0);
                PlatformSleepMs(ms);
            }
#ifndef _WIN32
            // macOS/Linux: ������ ���� busy-spin�� CPU���߿��� �� ª�� sleep
            while (glfwGetTime() - frameStart < target) {
                double left = target - (glfwGetTime() - frameStart);
                if (left > 0.003)
                    PlatformSleepMs(1);
                else
                    break;
            }
#else
            while (glfwGetTime() - frameStart < target) { /* spin */ }
#endif
        }
    }

    g_GameplayTelemetry.EndRun(g_GameTime,
                               g_GameManager.playerLevel,
                               g_GameManager.xp,
                               g_Stats,
                               "application_exit");
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glDeleteVertexArrays(1, &g_MainVAO);
    glDeleteBuffers(1, &g_VBO);
    glDeleteProgram(g_MainShader);
    ShutdownGameFonts();
    g_GameManager.ShutdownGlResources();
#ifdef _WIN32
    if (g_FontMemHandle) {
        RemoveFontMemResourceEx(g_FontMemHandle);
        g_FontMemHandle = nullptr;
    }
#endif
    Audio::Shutdown();
    PlatformTimerEnd();
    os_pane::Shutdown();
    ReleaseWindowTaskbarPolicy(window);
    InputUnregisterCallbacks(window);
    glfwTerminate();
    return 0;
}
