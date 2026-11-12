// Player frames must be one continuous stroke: mitred outer corners, no notches.
#include "PlayerWeaponShell.h"
#include "DrawPrim.h"
#include "MonsterManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

MonsterManager g_MonsterManager;
int g_CurrentWeapon = 0;
float g_ShotKickTimer = 0.0f;

static constexpr size_t kStride = 6;   // x, y, r, g, b, a

#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAILED line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

static bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

static bool HasVertex(float x, float y) {
    for (size_t i = 0; i + 1 < g_Batch.size(); i += kStride)
        if (Near(g_Batch[i], x) && Near(g_Batch[i + 1], y)) return true;
    return false;
}

static float MaxX() {
    float out = -1e9f;
    for (size_t i = 0; i + 1 < g_Batch.size(); i += kStride)
        out = std::max(out, g_Batch[i]);
    return out;
}

int main() {
    // Closed square: all four outer corners reach the exact miter point, and the
    // inner corners meet without overlap.
    {
        const float xs[4] = { 0, 100, 100, 0 }, ys[4] = { 0, 0, 100, 100 };
        g_Batch.clear();
        DrawPlayerPolyline(xs, ys, 4, true, 10.0f, 1, 1, 1, 1);
        CHECK(g_Batch.size() == 4 * 6 * kStride);
        for (float x : { -5.0f, 105.0f })
            for (float y : { -5.0f, 105.0f }) CHECK(HasVertex(x, y));
        for (float x : { 5.0f, 95.0f })
            for (float y : { 5.0f, 95.0f }) CHECK(HasVertex(x, y));
    }
    // Open bracket: the bend is mitred and both free ends stay flat at the end points.
    {
        const float xs[3] = { 0, 50, 50 }, ys[3] = { 0, 0, 50 };
        g_Batch.clear();
        DrawPlayerPolyline(xs, ys, 3, false, 4.0f, 1, 1, 1, 1);
        CHECK(g_Batch.size() == 2 * 6 * kStride);
        CHECK(HasVertex(52.0f, -2.0f));
        CHECK(HasVertex(0.0f, 2.0f) && HasVertex(0.0f, -2.0f));
        CHECK(HasVertex(52.0f, 50.0f) && HasVertex(48.0f, 50.0f));
    }
    // A sharp star tip is cut flat instead of throwing a needle past its node,
    // in either turning direction, and the cut is filled.
    for (float flip : { 1.0f, -1.0f }) {
        const float xs[3] = { 0, 100, 0 }, ys[3] = { -10 * flip, 0, 10 * flip };
        g_Batch.clear();
        DrawPlayerPolyline(xs, ys, 3, false, 4.0f, 1, 1, 1, 1);
        CHECK(g_Batch.size() == (2 * 6 + 3) * kStride);
        CHECK(MaxX() <= 100.0f + 2.0f);
        CHECK(MaxX() >= 100.0f + 0.1f);
    }
    // Duplicate points draw nothing rather than NaNs.
    {
        const float xs[2] = { 5, 5 }, ys[2] = { 5, 5 };
        g_Batch.clear();
        DrawPlayerPolyline(xs, ys, 2, false, 4.0f, 1, 1, 1, 1);
        CHECK(g_Batch.empty());
    }
    // The shells themselves produce only finite geometry.
    for (int kind = 0; kind < 2; ++kind) {
        g_Batch.clear();
        DrawPlayerShellPreview(kind, 300.0f, 300.0f, 120.0f, 1.7f, 0.3f, 0.9f, 1.0f, 1.0f);
        CHECK(!g_Batch.empty());
        for (float v : g_Batch) CHECK(std::isfinite(v));
    }
    std::puts("player_polyline_test: OK");
}
