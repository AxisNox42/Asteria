#pragma once
// Thick polyline strokes with clean joints, for character and mob wireframes.
//
// A stroke drawn as one rectangle per edge leaves a notch on the outside of
// every bend and doubles the colour where two rectangles overlap on the
// inside. These helpers build one outline per path instead: a bend shares a
// single vertex pair, the outside is a sharp mitre, and a bend too sharp for a
// mitre (a star tip) is cut off with a bevel so it never spikes past the
// stroke. Nothing is drawn twice, so a translucent stroke is even.
#include <algorithm>
#include <cmath>
#include "DrawPrim.h"

namespace miter {

// Longest mitre (tip distance / half width) kept before a bend is bevelled.
constexpr float kMiterLimit = 2.0f;
constexpr int kMaxPoints = 96;

// Stroke through n points. closed = join the last point back to the first;
// open paths end square. Points closer than ~0.01 are skipped.
inline void Stroke(const float* px, const float* py, int n, bool closed, float width,
                   float r, float g, float b, float a) {
    if (n < 2 || a <= 0.0f || width <= 0.0f) return;
    float x[kMaxPoints], y[kMaxPoints];
    int m = 0;
    for (int i = 0; i < n && m < kMaxPoints; ++i) {
        if (m > 0 && std::fabs(px[i] - x[m - 1]) + std::fabs(py[i] - y[m - 1]) < 0.01f) continue;
        x[m] = px[i]; y[m] = py[i]; ++m;
    }
    if (closed && m > 1 && std::fabs(x[0] - x[m - 1]) + std::fabs(y[0] - y[m - 1]) < 0.01f) --m;
    if (m < 2) return;
    if (m == 2) closed = false;

    const float h = width * 0.5f;
    // Per vertex: start/end point on each side (left = +normal), so an outer
    // bevel can have two points where a mitre has one.
    float lsx[kMaxPoints], lsy[kMaxPoints], lex[kMaxPoints], ley[kMaxPoints];
    float rsx[kMaxPoints], rsy[kMaxPoints], rex[kMaxPoints], rey[kMaxPoints];
    bool bevelL[kMaxPoints] = {}, bevelR[kMaxPoints] = {};

    auto segDir = [&](int i, float& dx, float& dy) {
        const int j = (i + 1) % m;
        dx = x[j] - x[i]; dy = y[j] - y[i];
        const float l = std::max(0.0001f, std::sqrt(dx * dx + dy * dy));
        dx /= l; dy /= l;
    };
    for (int i = 0; i < m; ++i) {
        float d1x, d1y, d2x, d2y;
        const bool hasIn = closed || i > 0, hasOut = closed || i < m - 1;
        if (hasOut) segDir(i, d2x, d2y);
        if (hasIn) segDir((i + m - 1) % m, d1x, d1y);
        if (!hasIn) { d1x = d2x; d1y = d2y; }
        if (!hasOut) { d2x = d1x; d2y = d1y; }
        const float cross = d1x * d2y - d1y * d2x;
        for (int side = 0; side < 2; ++side) {
            const float s = side == 0 ? 1.0f : -1.0f;       // left, right
            const float n1x = -d1y * s, n1y = d1x * s, n2x = -d2y * s, n2y = d2x * s;
            float mx = n1x + n2x, my = n1y + n2y;
            const float ml = std::sqrt(mx * mx + my * my);
            float sx, sy, ex, ey;
            bool bevel = false;
            if (ml < 0.0001f) {                              // a full reversal
                sx = x[i] + n2x * h; sy = y[i] + n2y * h;
                ex = x[i] + n1x * h; ey = y[i] + n1y * h;
                bevel = true;
            } else {
                mx /= ml; my /= ml;
                const float cosHalf = std::max(0.05f, mx * n1x + my * n1y);
                const bool outer = s * cross < 0.0f;
                if (outer && 1.0f / cosHalf > kMiterLimit) {
                    sx = x[i] + n2x * h; sy = y[i] + n2y * h;
                    ex = x[i] + n1x * h; ey = y[i] + n1y * h;
                    bevel = true;
                } else {
                    const float reach = h / cosHalf;
                    // The inside point must not run past a short edge.
                    sx = ex = x[i] + mx * std::min(reach, h * 4.0f);
                    sy = ey = y[i] + my * std::min(reach, h * 4.0f);
                }
            }
            if (side == 0) { lsx[i] = sx; lsy[i] = sy; lex[i] = ex; ley[i] = ey; bevelL[i] = bevel; }
            else           { rsx[i] = sx; rsy[i] = sy; rex[i] = ex; rey[i] = ey; bevelR[i] = bevel; }
        }
    }
    const int segs = closed ? m : m - 1;
    for (int i = 0; i < segs; ++i) {
        const int j = (i + 1) % m;
        // Quad: left start -> left end(j) -> right end(j) -> right start.
        BatchTri(lsx[i], lsy[i], lex[j], ley[j], rex[j], rey[j], r, g, b, a);
        BatchTri(lsx[i], lsy[i], rex[j], rey[j], rsx[i], rsy[i], r, g, b, a);
    }
    for (int i = 0; i < m; ++i) {
        if (!closed && (i == 0 || i == m - 1)) continue;
        if (bevelL[i]) BatchTri(x[i], y[i], lex[i], ley[i], lsx[i], lsy[i], r, g, b, a);
        if (bevelR[i]) BatchTri(x[i], y[i], rex[i], rey[i], rsx[i], rsy[i], r, g, b, a);
    }
}

inline void Loop(const float* px, const float* py, int n, float width,
                 float r, float g, float b, float a) {
    Stroke(px, py, n, true, width, r, g, b, a);
}
inline void Path(const float* px, const float* py, int n, float width,
                 float r, float g, float b, float a) {
    Stroke(px, py, n, false, width, r, g, b, a);
}

}  // namespace miter
