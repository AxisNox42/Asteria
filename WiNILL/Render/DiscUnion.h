#pragma once
// ─────────────────────────────────────────────────────────────
// The exact union of a set of discs, walked with marching squares, so discs
// that touch fuse into one shape with no inner outlines. Shared by MIRA's
// corruption puddles (filled + outlined, EntityDraw.cpp) and the Afterimage
// Trail ribbon (outline + scanlines only, SynergyRuntime.cpp).
// Pure geometry: the caller draws through the callbacks.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <vector>

namespace disc_union {

struct Disc { float x, y, r; };

// One chain of joined discs as a dense run along a smooth (Catmull-Rom) curve
// through them; the radius follows the straight blend (a spline can overshoot).
inline void ChainDiscs(const std::vector<Disc>& ctl, std::vector<Disc>& out, int sub = 6) {
    const int m = (int)ctl.size();
    if (m == 0) return;
    if (m == 1) { out.push_back(ctl[0]); return; }
    const auto cr = [](float p0, float p1, float p2, float p3, float u) {
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
    };
    for (int i = 0; i + 1 < m; ++i) {
        const Disc& p0 = ctl[std::max(0, i - 1)]; const Disc& p1 = ctl[i];
        const Disc& p2 = ctl[i + 1]; const Disc& p3 = ctl[std::min(m - 1, i + 2)];
        for (int k = 0; k < sub; ++k) {
            const float u = (float)k / (float)sub;
            out.push_back({ cr(p0.x, p1.x, p2.x, p3.x, u), cr(p0.y, p1.y, p2.y, p3.y, u),
                            std::max(0.0f, p1.r + (p2.r - p1.r) * u) });
        }
    }
    out.push_back(ctl[m - 1]);
}

// The field: value = (radius - distance), max over discs, sampled every `step` px.
struct Field {
    float minX = 0, minY = 0, step = 6.0f;
    int nx = 0, ny = 0;
    std::vector<float> v;
    float At(int i, int j) const { return v[(size_t)j * nx + i]; }
};

inline bool Build(const std::vector<Disc>& d, Field& f, float step = 6.0f, float maxCells = 60000.0f) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    for (const Disc& q : d) {
        if (q.r < 0.5f) continue;
        minX = std::min(minX, q.x - q.r); maxX = std::max(maxX, q.x + q.r);
        minY = std::min(minY, q.y - q.r); maxY = std::max(maxY, q.y + q.r);
    }
    if (minX > maxX) return false;
    while (((maxX - minX) / step + 3.0f) * ((maxY - minY) / step + 3.0f) > maxCells) step *= 1.5f;
    minX -= step; minY -= step;
    f.minX = minX; f.minY = minY; f.step = step;
    f.nx = (int)((maxX - minX) / step) + 3; f.ny = (int)((maxY - minY) / step) + 3;
    f.v.assign((size_t)f.nx * f.ny, -1.0f);
    for (const Disc& q : d) {
        if (q.r < 0.5f) continue;
        const int i0 = std::max(0, (int)std::floor((q.x - q.r - minX) / step));
        const int i1 = std::min(f.nx - 1, (int)std::ceil((q.x + q.r - minX) / step));
        const int j0 = std::max(0, (int)std::floor((q.y - q.r - minY) / step));
        const int j1 = std::min(f.ny - 1, (int)std::ceil((q.y + q.r - minY) / step));
        for (int j = j0; j <= j1; ++j)
            for (int i = i0; i <= i1; ++i) {
                const float dx = minX + (float)i * step - q.x, dy = minY + (float)j * step - q.y;
                const float val = q.r - std::sqrt(dx * dx + dy * dy);
                float& cell = f.v[(size_t)j * f.nx + i];
                if (val > cell) cell = val;
            }
    }
    return true;
}

// Marching squares over the field at level 0: tri(x0,y0,x1,y1,x2,y2) for the
// inside (pass a no-op to skip the fill), seg(x0,y0,x1,y1) for the outline.
template <class Tri, class Seg>
void March(const Field& f, Tri&& tri, Seg&& seg) {
    static const int ci[4] = { 0, 1, 1, 0 }, cj[4] = { 0, 0, 1, 1 };
    for (int j = 0; j + 1 < f.ny; ++j)
        for (int i = 0; i + 1 < f.nx; ++i) {
            float px[8], py[8], cx[4], cy[4];
            int np = 0, nc = 0;
            for (int k = 0; k < 4; ++k) {
                const int n = (k + 1) & 3;
                const float va = f.At(i + ci[k], j + cj[k]);
                const float vb = f.At(i + ci[n], j + cj[n]);
                const float ax = f.minX + (float)(i + ci[k]) * f.step, ay = f.minY + (float)(j + cj[k]) * f.step;
                const float bx = f.minX + (float)(i + ci[n]) * f.step, by = f.minY + (float)(j + cj[n]) * f.step;
                if (va >= 0.0f) { px[np] = ax; py[np] = ay; ++np; }
                if ((va >= 0.0f) != (vb >= 0.0f)) {
                    const float u = (0.0f - va) / (vb - va);
                    px[np] = ax + (bx - ax) * u; py[np] = ay + (by - ay) * u;
                    cx[nc] = px[np]; cy[nc] = py[np]; ++nc; ++np;
                }
            }
            for (int k = 1; k + 1 < np; ++k) tri(px[0], py[0], px[k], py[k], px[k + 1], py[k + 1]);
            for (int k = 0; k + 1 < nc; k += 2) seg(cx[k], cy[k], cx[k + 1], cy[k + 1]);
        }
}

// Horizontal scanlines across the inside, on every `every`-th sample row, kept
// `inset` px inside the outline: seg(x0,y,x1,y) for each run.
template <class Seg>
void Rows(const Field& f, int every, float inset, Seg&& seg) {
    for (int j = 1; j + 1 < f.ny; j += every) {
        int run = -1;
        for (int i = 0; i < f.nx; ++i) {
            const bool in = f.At(i, j) >= inset;
            if (in && run < 0) run = i;
            if ((!in || i == f.nx - 1) && run >= 0) {
                const int end = in ? i : i - 1;
                if (end > run) {
                    const float y = f.minY + (float)j * f.step;
                    seg(f.minX + (float)run * f.step, y, f.minX + (float)end * f.step, y);
                }
                run = -1;
            }
        }
    }
}

}  // namespace disc_union
