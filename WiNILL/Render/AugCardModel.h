#pragma once
// ─────────────────────────────────────────────────────────────
// Augment card draw view model (Docs/Design/Augment_Card_Draw_Design.md):
// card layout, draw/flip timing, hit test, the dissolve of unpicked cards
// and the 20 family constellations. No GL and no game headers, so
// tests/aug_card_model_test.cpp can build it alone.
//   Card-local units: the card is kCardW × kCardH centred on (0, 0) at
//   UiScale 1; Layout::s maps them to screen pixels.
// ─────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <cstring>

namespace aug_card {

inline constexpr int   kMaxCards = 5;
inline constexpr float kCardW = 280.0f, kCardH = 410.0f;
inline constexpr float kGap = 24.0f, kCut = 17.0f;

inline float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
inline float QuintOut(float t) { t = 1.0f - Clamp01(t); return 1.0f - t * t * t * t * t; }
inline float QuintIn(float t) { t = Clamp01(t); return t * t * t * t * t; }

// ── layout ──
struct Layout {
    int   count = 0;
    float s = 1.0f;                 // card-local unit → pixels
    float cardW = 0.0f, cardH = 0.0f;
    float cx[kMaxCards] = {};
    float cy = 0.0f;
    float deckX = 0.0f, deckY = 0.0f, deckS = 0.0f;
};

// UiScale (1640×910 base), then shrunk until the row fits 94% of the width
// and a card 62% of the height (the tutorial's mini card screen is small).
// A row of three or fewer (the level-up stat pick) has room, so its cards
// grow by kFewBoost.
inline constexpr float kFewBoost = 1.18f;
inline Layout MakeLayout(float sw, float sh, int count) {
    Layout L;
    L.count = std::max(0, std::min(kMaxCards, count));
    float s = std::max(0.70f, std::min(1.35f,
        std::min(sw * 0.94f / 1640.0f, sh * 0.90f / 910.0f)));
    if (L.count <= 3) s *= kFewBoost;
    const int n = std::max(1, L.count);
    const float rowW = n * kCardW + (n - 1) * kGap;
    s = std::min(s, sw * 0.94f / rowW);
    s = std::min(s, sh * 0.62f / kCardH);
    L.s = s;
    L.cardW = kCardW * s;
    L.cardH = kCardH * s;
    L.cy = sh * 0.47f;
    const float pitch = (kCardW + kGap) * s;
    for (int i = 0; i < L.count; ++i)
        L.cx[i] = sw * 0.5f + (i - (L.count - 1) * 0.5f) * pitch;
    const float bottom = L.cy + L.cardH * 0.5f;
    L.deckX = sw * 0.5f;
    L.deckY = bottom + (sh - bottom) * 0.40f;
    L.deckS = 0.30f;
    return L;
}

// Card under the pointer; `lift` is each card's hover rise in pixels.
inline int HitTest(const Layout& L, const float* lift, float mx, float my) {
    const float pad = 4.0f * L.s;
    for (int i = 0; i < L.count; ++i) {
        const float y = L.cy - (lift ? lift[i] : 0.0f);
        if (std::fabs(mx - L.cx[i]) <= L.cardW * 0.5f + pad &&
            std::fabs(my - y) <= L.cardH * 0.5f + pad)
            return i;
    }
    return -1;
}

// ── draw timeline (seconds since the screen opened) ──
inline constexpr float kDeckIn = 0.18f;
inline constexpr float kDrawStart = 0.20f, kDrawStep = 0.09f, kDrawDur = 0.42f;
inline constexpr float kFlipStep = 0.08f, kFlipDur = 0.24f, kChargeDur = 0.20f;
inline constexpr float kTraceDur = 0.35f, kRevealDur = 0.55f;
inline constexpr float kInputLead = 0.25f;   // input opens this long before the end

inline bool IsHighRarity(int rarity) { return rarity >= 3; }   // LEGENDARY, MYTHIC

struct Timeline {
    int   count = 0;
    float flipStart[kMaxCards] = {};
    float end = 0.0f;
    float InputReady() const { return end - kInputLead; }
};

// Flips start once the last card lands, left to right; legendary and mythic
// cards flip last, each after a kChargeDur hold.
inline Timeline MakeTimeline(const int* rarity, int count) {
    Timeline T;
    T.count = std::max(0, std::min(kMaxCards, count));
    float t = kDrawStart + kDrawStep * std::max(0, T.count - 1) + kDrawDur + 0.04f;
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < T.count; ++i) {
            if (IsHighRarity(rarity[i]) != (pass == 1)) continue;
            if (pass == 1) t += kChargeDur;
            T.flipStart[i] = t;
            t += kFlipStep;
        }
    float last = 0.0f;
    for (int i = 0; i < T.count; ++i) last = std::max(last, T.flipStart[i]);
    T.end = last + kFlipDur + kRevealDur;
    return T;
}

struct CardPhase {
    float move = 0.0f;     // 0 in the deck → 1 in its slot (eased)
    float flip = 0.0f;     // 0 back → 1 face (0.5 = edge-on)
    float charge = 0.0f;   // legendary+: ring fill before the flip
    float trace = 0.0f;    // constellation edges drawn
    float title = 0.0f;    // title / rarity marks alpha
    float textT0 = 0.0f;   // description line j fades in from textT0 + 0.03·j
};

inline CardPhase PhaseAt(const Timeline& T, const int* rarity, int i, float t) {
    CardPhase P;
    P.move = QuintOut((t - (kDrawStart + kDrawStep * i)) / kDrawDur);
    const float fs = T.flipStart[i], fe = fs + kFlipDur;
    P.flip = Clamp01((t - fs) / kFlipDur);
    P.charge = IsHighRarity(rarity[i]) ? Clamp01((t - (fs - kChargeDur)) / kChargeDur) : 0.0f;
    P.trace = Clamp01((t - fe) / kTraceDur);
    P.title = Clamp01((t - fe - 0.05f) / 0.20f);
    P.textT0 = fe + 0.10f;
    return P;
}
inline float LineAlpha(const CardPhase& P, int line, float t) {
    return Clamp01((t - P.textT0 - 0.03f * line) / 0.15f);
}

// ── pick exit (g_AugExitT) ──
// Short on purpose: combat is frozen under the pick, so a long tail read as
// slow motion. Everything vanishes while the dim lifts, and the dim's end is
// the scene's end.
inline float Smooth01(float t) { t = Clamp01(t); return t * t * (3.0f - 2.0f * t); }
inline constexpr float kExitDur = 0.95f;
inline constexpr float kPickPulse = 0.15f;                 // 18 Hz blink on the pick
inline constexpr float kPickFade = 0.25f;                  // the pick's frame, after the pulse
inline constexpr float kAbsorbStart = 0.12f, kAbsorbDur = 0.38f, kAbsorbStep = 0.012f;
inline constexpr float kAbsorbRing = 0.30f;
// Card effects fade out over the last stretch, under whatever is still moving.
inline constexpr float kSceneFadeStart = 0.70f;
inline float SceneAlpha(float exitT) { return 1.0f - Smooth01((exitT - kSceneFadeStart) / (kExitDur - kSceneFadeStart)); }
// The dim (backdrop) fades in when the pick opens and out over the exit.
inline constexpr float kDimIn = 0.35f, kDimOutStart = 0.10f;
inline float DimIn(float t) { return Smooth01(t / kDimIn); }
inline float DimOut(float exitT) { return 1.0f - Smooth01((exitT - kDimOutStart) / (kExitDur - kDimOutStart)); }
// Combat HUD hidden under the pick fades back in as the dim lifts.
inline float HudReturn(float exitT) { return exitT < 0.0f ? 0.0f : 1.0f - DimOut(exitT); }

// Unpicked cards dissolve into nourishment for the stars: text fades, the
// frame breaks into fragments in a wave moving away from the pick, and each
// constellation node rises, whitens and goes out.
inline constexpr float kFragLen = 34.0f, kFragLife = 0.42f, kSeedLife = 0.45f;
inline float Hash01(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (float)(x & 0xFFFFFFu) / 16777215.0f;
}
inline float TextFade(float exitT) { return 1.0f - Clamp01((exitT - 0.03f) / 0.15f); }
inline float FragStart(int dist, unsigned seed) { return 0.05f + 0.04f * dist + Hash01(seed) * 0.08f; }
inline float ConstellFade(int dist, float exitT) { return 1.0f - Clamp01((exitT - 0.08f - 0.04f * dist) / 0.12f); }
inline float SeedStart(int dist, int node) { return 0.08f + 0.04f * dist + 0.01f * node; }

// Card outline corners (card-local), clockwise from the top-left cut.
inline void ChamferCorners(float w, float h, float cut, float* px, float* py) {
    const float x = -w * 0.5f, y = -h * 0.5f;
    const float cx[8] = { x + cut, x + w - cut, x + w, x + w, x + w - cut, x + cut, x, x };
    const float cy[8] = { y, y, y + cut, y + h - cut, y + h, y + h, y + h - cut, y + cut };
    for (int i = 0; i < 8; ++i) { px[i] = cx[i]; py[i] = cy[i]; }
}

// ── family constellations ──
enum class Family { NIGHT, CHRONO, GALE, SHADOW, STONE, FLAME, GEM, STORM, LUCK, LIFE,
                    ABYSS, SPARK, FLIGHT, EDGE, HERESY, HUNTER, PERFORM, STILL, VEIN, NOVA,
                    COUNT };
inline constexpr int kFamilyCount = (int)Family::COUNT;

inline const char* FamilyCode(Family f) {
    static const char* k[kFamilyCount] = {
        "NIGHT", "CHRONO", "GALE", "SHADOW", "STONE", "FLAME", "GEM", "STORM", "LUCK", "LIFE",
        "ABYSS", "SPARK", "FLIGHT", "EDGE", "HERESY", "HUNTER", "PERFORM", "STILL", "VEIN", "NOVA" };
    return k[(int)f];
}

// "P_FLAME_LIMIT_BREAK" / "A_FLAME_PROMINENCE_SHELL" → FLAME (AugDef::name); COUNT if unknown.
inline Family FamilyFromCode(const char* code) {
    if (!code || (code[0] != 'P' && code[0] != 'A') || code[1] != '_') return Family::COUNT;
    const char* fam = code + 2;
    const char* end = std::strchr(fam, '_');
    const size_t n = end ? (size_t)(end - fam) : std::strlen(fam);
    for (int i = 0; i < kFamilyCount; ++i) {
        const char* k = FamilyCode((Family)i);
        if (std::strlen(k) == n && std::strncmp(k, fam, n) == 0) return (Family)i;
    }
    return Family::COUNT;
}

// Normalised 0..1 points (y down) and edges; lines and nodes only.
struct Shape {
    static constexpr int kMaxV = 16, kMaxE = 24;
    float x[kMaxV] = {}, y[kMaxV] = {};
    int   nv = 0;
    unsigned char ea[kMaxE] = {}, eb[kMaxE] = {};
    int   ne = 0;
    int V(float px, float py) { x[nv] = px; y[nv] = py; return nv++; }
    void E(int a, int b) { ea[ne] = (unsigned char)a; eb[ne] = (unsigned char)b; ++ne; }
    void Chain(int from, int n) { for (int i = 0; i + 1 < n; ++i) E(from + i, from + i + 1); }
    void Loop(int from, int n, int skip = -1) { for (int i = 0; i < n; ++i) if (i != skip) E(from + i, from + (i + 1) % n); }
    int Poly(float cx, float cy, float r, int n, float a0Deg = -90.0f) {
        const int first = nv;
        for (int i = 0; i < n; ++i) {
            const float a = (a0Deg + i * 360.0f / n) * 3.14159265f / 180.0f;
            V(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        return first;
    }
    // First edge that touches node v (ne when none): the node lights as that edge is drawn.
    int FirstEdge(int v) const {
        for (int e = 0; e < ne; ++e) if (ea[e] == v || eb[e] == v) return e;
        return ne;
    }
};

inline Shape BuildShape(Family f) {
    constexpr float d2r = 3.14159265f / 180.0f;
    Shape s;
    switch (f) {
    case Family::NIGHT: {   // crescent + three stars
        for (int a = 120; a <= 240; a += 24) s.V(0.62f + 0.40f * std::cos(a * d2r), 0.5f + 0.40f * std::sin(a * d2r));
        for (int a = 130; a <= 230; a += 25) s.V(0.76f + 0.32f * std::cos(a * d2r), 0.5f + 0.32f * std::sin(a * d2r));
        s.Chain(0, 6); s.Chain(6, 5); s.E(0, 6); s.E(5, 10);
        s.V(0.80f, 0.18f); s.V(0.92f, 0.34f); s.V(0.86f, 0.52f); s.E(11, 12); s.E(12, 13);
        break; }
    case Family::CHRONO:    // gapped dial + two hands
        s.Poly(0.5f, 0.5f, 0.42f, 8); s.Loop(0, 8, 2);
        s.V(0.5f, 0.5f); s.V(0.70f, 0.62f); s.E(8, 0); s.E(8, 9);
        break;
    case Family::GALE:
        s.V(0.06f, 0.32f); s.V(0.32f, 0.16f); s.V(0.54f, 0.36f); s.V(0.34f, 0.54f);
        s.V(0.60f, 0.68f); s.V(0.88f, 0.56f); s.V(0.74f, 0.88f); s.Chain(0, 7);
        break;
    case Family::SHADOW:    // two offset triangles
        s.V(0.12f, 0.78f); s.V(0.44f, 0.18f); s.V(0.76f, 0.78f);
        s.V(0.26f, 0.88f); s.V(0.58f, 0.28f); s.V(0.90f, 0.88f);
        s.Loop(0, 3); s.Loop(3, 3); s.E(1, 4);
        break;
    case Family::STONE:     // hex crystal
        s.Poly(0.5f, 0.5f, 0.42f, 6); s.Loop(0, 6);
        s.V(0.5f, 0.5f); s.E(6, 0); s.E(6, 2); s.E(6, 4);
        break;
    case Family::FLAME:
        s.V(0.5f, 0.94f); s.V(0.30f, 0.56f); s.V(0.20f, 0.28f); s.V(0.5f, 0.48f);
        s.V(0.5f, 0.06f); s.V(0.70f, 0.56f); s.V(0.80f, 0.30f);
        s.E(0, 1); s.E(1, 2); s.E(0, 3); s.E(3, 4); s.E(0, 5); s.E(5, 6); s.E(1, 3); s.E(3, 5);
        break;
    case Family::GEM:       // brilliant cut
        s.V(0.30f, 0.20f); s.V(0.70f, 0.20f); s.V(0.08f, 0.40f); s.V(0.92f, 0.40f);
        s.V(0.5f, 0.94f); s.V(0.5f, 0.40f);
        s.E(0, 1); s.E(0, 2); s.E(1, 3); s.E(2, 3); s.E(2, 4); s.E(3, 4); s.E(0, 5); s.E(1, 5); s.E(5, 4);
        break;
    case Family::STORM:     // bolt
        s.V(0.64f, 0.04f); s.V(0.32f, 0.46f); s.V(0.56f, 0.46f); s.V(0.28f, 0.96f); s.V(0.80f, 0.66f);
        s.E(0, 1); s.E(1, 2); s.E(2, 3); s.E(2, 4);
        break;
    case Family::LUCK: {    // four leaves
        s.V(0.5f, 0.5f);
        for (int k = 0; k < 4; ++k) {
            const float a = (-90.0f + k * 90.0f) * d2r;
            auto P = [&](float r, float dDeg) { s.V(0.5f + r * std::cos(a + dDeg * d2r), 0.5f + r * std::sin(a + dDeg * d2r)); };
            const int b = s.nv;
            P(0.26f, -30.0f); P(0.44f, 0.0f); P(0.26f, 30.0f);
            s.E(0, b); s.E(b, b + 1); s.E(b + 1, b + 2); s.E(b + 2, 0);
        }
        break; }
    case Family::LIFE:      // branching tree
        s.V(0.5f, 0.96f); s.V(0.5f, 0.62f); s.V(0.5f, 0.32f); s.V(0.22f, 0.44f);
        s.V(0.80f, 0.48f); s.V(0.28f, 0.12f); s.V(0.72f, 0.14f); s.V(0.5f, 0.04f);
        s.E(0, 1); s.E(1, 2); s.E(1, 3); s.E(1, 4); s.E(2, 5); s.E(2, 6); s.E(2, 7);
        break;
    case Family::ABYSS:     // inward spiral
        for (int k = 0; k < 11; ++k) {
            const float a = k / 10.0f * 3.4f * 3.14159265f, r = 0.46f * (1.0f - k / 11.5f) + 0.03f;
            s.V(0.5f + r * std::cos(a), 0.5f + r * std::sin(a));
        }
        s.Chain(0, 11);
        break;
    case Family::SPARK:
        s.V(0.5f, 0.5f); s.Poly(0.5f, 0.5f, 0.18f, 6); s.Poly(0.5f, 0.5f, 0.44f, 6, -70.0f);
        for (int i = 1; i <= 6; ++i) { s.E(0, i); s.E(i, i + 6); }
        break;
    case Family::FLIGHT:    // wings
        s.V(0.5f, 0.62f); s.V(0.34f, 0.50f); s.V(0.18f, 0.36f); s.V(0.04f, 0.18f); s.V(0.14f, 0.56f);
        s.V(0.66f, 0.50f); s.V(0.82f, 0.36f); s.V(0.96f, 0.18f); s.V(0.86f, 0.56f); s.V(0.5f, 0.88f);
        s.E(0, 1); s.E(1, 2); s.E(2, 3); s.E(2, 4); s.E(0, 5); s.E(5, 6); s.E(6, 7); s.E(6, 8); s.E(0, 9);
        break;
    case Family::EDGE:      // sword
        s.V(0.5f, 0.03f); s.V(0.5f, 0.66f); s.V(0.26f, 0.66f); s.V(0.74f, 0.66f); s.V(0.5f, 0.84f); s.V(0.5f, 0.96f);
        s.E(0, 1); s.E(2, 1); s.E(1, 3); s.E(1, 4); s.E(4, 5);
        break;
    case Family::HERESY:    // halo + skewed cross
        s.Poly(0.5f, 0.28f, 0.22f, 8); s.Loop(0, 8);
        s.V(0.5f, 0.58f); s.V(0.5f, 0.98f); s.V(0.24f, 0.74f); s.V(0.76f, 0.70f);
        s.E(4, 8); s.E(8, 9); s.E(10, 11);
        break;
    case Family::HUNTER:    // aim ring + arrow
        s.Poly(0.5f, 0.58f, 0.28f, 8); s.Loop(0, 8);
        s.V(0.5f, 0.02f); s.V(0.36f, 0.20f); s.V(0.64f, 0.20f); s.V(0.5f, 0.98f);
        s.E(9, 8); s.E(8, 10); s.E(8, 11);
        break;
    case Family::PERFORM:   // hoop + ribbon
        s.Poly(0.42f, 0.36f, 0.30f, 10); s.Loop(0, 10);
        s.V(0.62f, 0.72f); s.V(0.80f, 0.80f); s.V(0.66f, 0.90f); s.V(0.92f, 0.97f);
        s.E(5, 10); s.Chain(10, 4);
        break;
    case Family::STILL:     // horizon + peak + lone star
        s.V(0.04f, 0.78f); s.V(0.28f, 0.78f); s.V(0.72f, 0.78f); s.V(0.96f, 0.78f);
        s.V(0.5f, 0.36f); s.V(0.5f, 0.08f);
        s.E(0, 1); s.E(1, 2); s.E(2, 3); s.E(1, 4); s.E(4, 2);
        break;
    case Family::VEIN:      // splitting ore vein
        s.V(0.04f, 0.16f); s.V(0.34f, 0.34f); s.V(0.50f, 0.60f); s.V(0.94f, 0.78f);
        s.V(0.44f, 0.04f); s.V(0.26f, 0.94f); s.V(0.76f, 0.42f);
        s.E(0, 1); s.E(1, 2); s.E(2, 3); s.E(1, 4); s.E(2, 5); s.E(2, 6);
        break;
    case Family::NOVA:
    default:                // centre + eight rays
        s.V(0.5f, 0.5f);
        for (int k = 0; k < 8; ++k) {
            const float a = (-90.0f + k * 45.0f) * d2r, r = (k % 2) ? 0.28f : 0.46f;
            s.V(0.5f + r * std::cos(a), 0.5f + r * std::sin(a));
            s.E(0, k + 1);
        }
        break;
    }
    return s;
}

inline const Shape& ShapeOf(Family f) {
    static Shape cache[kFamilyCount + 1];
    static bool built = false;
    if (!built) {
        for (int i = 0; i <= kFamilyCount; ++i) cache[i] = BuildShape((Family)i);
        built = true;
    }
    return cache[std::max(0, std::min(kFamilyCount, (int)f))];
}

// Where the constellation sits inside the card (card-local units): the lower
// part of the card, under the title.
inline constexpr float kConX = -110.0f, kConY = -18.0f, kConW = 220.0f, kConH = 186.0f;

}  // namespace aug_card
