#pragma once

// ─────────────────────────────────────────────────────────────
// Window spaces for the NORMA boss: every open pane is a separate space.
//   Movement and aggro cross pane borders; visible attacks and effects do not.
//   Space 0 is the open field and a pane's space id is its pane id. Regions
//   are world rectangles, front-most first, so overlapping panes give the
//   overlap to the one in front. With no regions every check passes and the
//   rest of the game is unaffected.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <vector>

namespace space {

struct Region {
    int id = 0;
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float bornAt = 0.0f;   // g_Clock when the pane first stood
};

inline std::vector<Region> g_Regions;
inline int g_PlayerSpace = 0;
inline float g_Clock = 0.0f;   // seconds; shots stamp it when first seen

constexpr int kPlayerSource = -2;    // "whatever space the player is in"
constexpr int kUntracked = -1;       // source not tracked: always allowed

inline bool Active() { return !g_Regions.empty(); }

// Space of an entity whose centre is at (x, y).
inline int SpaceAt(float x, float y) {
    for (const Region& r : g_Regions)
        if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) return r.id;
    return 0;
}

inline int Resolve(int source) {
    return source == kPlayerSource ? g_PlayerSpace : source;
}

// Can an attack or effect from `source` touch something centred at (x, y)?
inline bool Reaches(int source, float x, float y) {
    if (!Active() || source == kUntracked) return true;
    return Resolve(source) == SpaceAt(x, y);
}

inline bool SameSpace(float ax, float ay, float bx, float by) {
    return !Active() || SpaceAt(ax, ay) == SpaceAt(bx, by);
}

// Enemy damage reaches the player through HurtPlayer, which has no attacker
// argument; the monster update loop names the attacker's space here.
inline int g_HurtSource = kUntracked;

struct HurtSourceScope {
    int previous;
    explicit HurtSourceScope(int source) : previous(g_HurtSource) { g_HurtSource = source; }
    ~HurtSourceScope() { g_HurtSource = previous; }
    HurtSourceScope(const HurtSourceScope&) = delete;
    HurtSourceScope& operator=(const HurtSourceScope&) = delete;
};

inline bool PlayerExposed() {
    return !Active() || g_HurtSource == kUntracked || g_HurtSource == g_PlayerSpace;
}

// A shot stopped by a pane border this frame. The pane it hit loses
// durability; the open field (0) never absorbs.
struct Absorb {
    int paneId = 0;
    float x = 0.0f, y = 0.0f;
};
inline std::vector<Absorb> g_Absorbed;

// Crossing from one space to another is stopped by the pane being entered,
// or by the pane being left when stepping out into the open field.
inline int AbsorbingPane(int from, int to) { return to != 0 ? to : from; }

// A shot keeps the space it was fired in. True when it now stands in another
// space (it crossed a border, or a pane was dragged over it or closed), with
// the pane that stops it.
inline bool CrossedBorder(int shotSpace, float x, float y, int& absorbingPane) {
    const int now = SpaceAt(x, y);
    if (now == shotSpace) return false;
    absorbingPane = AbsorbingPane(shotSpace, now);
    return true;
}

// A pane only stops shots fired after it stood: a pane that opens over shots
// already in flight (Detonate reopening its panes) lets them through.
inline bool Absorbs(int paneId, float firedAt) {
    if (paneId <= 0) return true;
    for (const Region& r : g_Regions)
        if (r.id == paneId) return firedAt >= r.bornAt;
    return true;
}

// Solid panes (the Wall pattern's stack): a rectangle nothing may stand in.
struct Box { float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };

inline bool InsideBoxes(const std::vector<Box>& boxes, float margin, float x, float y) {
    for (const Box& b : boxes)
        if (x > b.x - margin && x < b.x + b.w + margin && y > b.y - margin && y < b.y + b.h + margin)
            return true;
    return false;
}

// Moves (x, y) out of every box, to the nearest point outside all of them.
// The boxes may overlap each other (a cascade) and the nearest edge of the one
// a point is in may lie inside the next, or off the screen, so candidates are
// straight out from the point and the corners of the stack's outline, and
// those on the screen win over those off it. False when it was not inside.
inline bool PushOutOfBoxes(const std::vector<Box>& boxes, float margin,
                           float screenW, float screenH, float& x, float& y) {
    if (!InsideBoxes(boxes, margin, x, y)) return false;
    std::vector<float> xs, ys;
    for (const Box& b : boxes) {
        xs.push_back(b.x - margin - 1.0f);
        xs.push_back(b.x + b.w + margin + 1.0f);
        ys.push_back(b.y - margin - 1.0f);
        ys.push_back(b.y + b.h + margin + 1.0f);
    }
    float bestD = -1.0f, bx = x, by = y;
    for (int onScreenOnly = 1; onScreenOnly >= 0 && bestD < 0.0f; --onScreenOnly) {
        const auto consider = [&](float cx, float cy) {
            if (onScreenOnly && (cx < 0.0f || cx > screenW || cy < 0.0f || cy > screenH)) return;
            if (InsideBoxes(boxes, margin, cx, cy)) return;
            const float d = (cx - x) * (cx - x) + (cy - y) * (cy - y);
            if (bestD < 0.0f || d < bestD) { bestD = d; bx = cx; by = cy; }
        };
        for (float cx : xs) {
            consider(cx, y);
            for (float cy : ys) consider(cx, cy);
        }
        for (float cy : ys) consider(x, cy);
    }
    if (bestD < 0.0f) return false;
    x = bx;
    y = by;
    return true;
}

// Pane durability: enemy hits a pane can stop before it self-destructs.
// Smaller panes are sturdier, up to three times the base; larger ones keep
// the base.
constexpr float kBaseDurability = 120.0f;
constexpr float kReferenceArea = 520.0f * 360.0f;

// A worn-out pane does not close at once: it burns a fuse for kFuseSeconds
// (a bright head running round its frame), then folds shut. There is no blast.
constexpr float kFuseSeconds = 1.4f;
// Below this share of durability the pane's contents start to waver.
constexpr float kUnsteadyShare = 0.30f;

inline float MaxDurability(float area) {
    if (area <= 1.0f) return kBaseDurability * 3.0f;
    return kBaseDurability * std::clamp(kReferenceArea / area, 1.0f, 3.0f);
}

} // namespace space
