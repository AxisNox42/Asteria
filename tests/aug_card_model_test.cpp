// Run scripts/test_aug_card_model.ps1. Augment card draw model: layout,
// draw/flip timing, hit test, dissolve timing and family constellations.
#include "AugCardModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace aug_card;

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void layout_fits_every_size() {
    const float sizes[][2] = { {1280, 720}, {1920, 1080}, {2560, 1440}, {2560, 1080},
                               {2560, 1600}, {640, 360} };
    for (auto& z : sizes) {
        const Layout L = MakeLayout(z[0], z[1], kMaxCards);
        assert(L.count == kMaxCards);
        const float left = L.cx[0] - L.cardW * 0.5f, right = L.cx[4] + L.cardW * 0.5f;
        assert(left >= 0.0f && right <= z[0]);
        assert(L.cy - L.cardH * 0.5f >= 0.0f && L.cy + L.cardH * 0.5f <= z[1]);
        for (int i = 0; i + 1 < L.count; ++i)   // no overlap
            assert(L.cx[i] + L.cardW * 0.5f < L.cx[i + 1] - L.cardW * 0.5f);
        assert(Near(L.cx[2], z[0] * 0.5f));
        assert(L.deckY > L.cy + L.cardH * 0.5f && L.deckY < z[1]);
    }
    // 1920×1080 keeps the full UiScale.
    assert(Near(MakeLayout(1920, 1080, 5).s, std::min(1920 * 0.94f / 1640.0f, 1080 * 0.90f / 910.0f)));
}

static void fewer_cards_center() {
    const Layout L = MakeLayout(1920, 1080, 2);
    assert(L.count == 2);
    assert(Near((L.cx[0] + L.cx[1]) * 0.5f, 960.0f));
    assert(MakeLayout(1920, 1080, 9).count == kMaxCards);
}

static void hit_test() {
    const Layout L = MakeLayout(1920, 1080, 5);
    const float lift[kMaxCards] = {};
    for (int i = 0; i < 5; ++i) assert(HitTest(L, lift, L.cx[i], L.cy) == i);
    assert(HitTest(L, lift, (L.cx[0] + L.cx[1]) * 0.5f, L.cy) == -1);   // gap
    assert(HitTest(L, lift, L.cx[0], L.cy + L.cardH) == -1);
    float lifted[kMaxCards] = { 0, 0, 40, 0, 0 };
    assert(HitTest(L, lifted, L.cx[2], L.cy - L.cardH * 0.5f - 30.0f) == 2);
}

static void legendary_flips_last() {
    const int rar[5] = { 1, 3, 0, 2, 0 };
    const Timeline T = MakeTimeline(rar, 5);
    for (int i = 0; i < 5; ++i)
        if (i != 1) assert(T.flipStart[1] > T.flipStart[i]);
    // Left to right among the rest, kFlipStep apart, after the last card lands.
    const float landed = kDrawStart + kDrawStep * 4 + kDrawDur;
    assert(T.flipStart[0] > landed);
    assert(Near(T.flipStart[2] - T.flipStart[0], kFlipStep));
    assert(Near(T.flipStart[3] - T.flipStart[2], kFlipStep));
    assert(Near(T.flipStart[1] - T.flipStart[4], kFlipStep + kChargeDur));
    assert(Near(T.end, T.flipStart[1] + kFlipDur + kRevealDur));
    assert(T.InputReady() < T.end);
}

static void plain_draw_is_about_two_seconds() {
    const int rar[5] = { 0, 1, 2, 0, 1 };
    const Timeline T = MakeTimeline(rar, 5);
    assert(T.end > 1.9f && T.end < 2.3f);
}

static void phase_progression() {
    const int rar[5] = { 0, 0, 0, 3, 0 };
    const Timeline T = MakeTimeline(rar, 5);
    CardPhase P = PhaseAt(T, rar, 4, 0.0f);
    assert(P.move == 0.0f && P.flip == 0.0f && P.title == 0.0f);
    P = PhaseAt(T, rar, 4, kDrawStart + kDrawStep * 4 + kDrawDur);
    assert(Near(P.move, 1.0f) && P.flip == 0.0f);
    P = PhaseAt(T, rar, 3, T.flipStart[3] - kChargeDur * 0.5f);
    assert(P.charge > 0.4f && P.charge < 0.6f && P.flip == 0.0f);
    P = PhaseAt(T, rar, 0, T.end);
    assert(P.flip == 1.0f && P.trace == 1.0f && P.title == 1.0f);
    assert(LineAlpha(P, 0, T.end) == 1.0f);
    assert(PhaseAt(T, rar, 0, T.flipStart[0]).charge == 0.0f);   // only legendary+ charges
}

static void dissolve_wave_moves_outward() {
    for (unsigned seed = 0; seed < 50; ++seed) {
        assert(FragStart(0, seed) >= 0.05f && FragStart(0, seed) <= 0.17f + 1e-4f);
        assert(FragStart(3, seed) >= FragStart(1, 999) - 0.12f);
    }
    assert(SeedStart(1, 0) < SeedStart(2, 0));
    assert(SeedStart(1, 0) < SeedStart(1, 5));
    assert(ConstellFade(1, 0.0f) == 1.0f && ConstellFade(1, 0.5f) == 0.0f);
    assert(TextFade(0.0f) == 1.0f && TextFade(0.2f) == 0.0f);
    // Everything finishes before the scene fades out.
    const float lastFrag = FragStart(kMaxCards - 1, 0) + 0.12f + kFragLife;
    const float lastSeed = SeedStart(kMaxCards - 1, Shape::kMaxV - 1) + kSeedLife;
    assert(lastFrag < kSceneFadeStart && lastSeed < kSceneFadeStart);
    assert(kAbsorbStart + kAbsorbDur + kAbsorbStep * Shape::kMaxV < kSceneFadeStart);
    assert(SceneAlpha(0.0f) == 1.0f && SceneAlpha(kExitDur) == 0.0f);
}

static void family_codes() {
    assert(FamilyFromCode("P_FLAME_DEMON_HORNS") == Family::FLAME);
    assert(FamilyFromCode("A_FLAME_FIREBALL") == Family::FLAME);
    assert(FamilyFromCode("P_STILL_FLORAL_BOW") == Family::STILL);
    assert(FamilyFromCode("A_NOVA_STARBURST") == Family::NOVA);
    assert(FamilyFromCode("P_STORM_CROWN_OF_STORMS") == Family::STORM);
    assert(FamilyFromCode("X_FLAME") == Family::COUNT);
    assert(FamilyFromCode("P_FLAM_X") == Family::COUNT);
    assert(FamilyFromCode(nullptr) == Family::COUNT);
    for (int i = 0; i < kFamilyCount; ++i) {
        char code[32];
        std::snprintf(code, sizeof code, "P_%s_TEST", FamilyCode((Family)i));
        assert(FamilyFromCode(code) == (Family)i);
    }
}

static void shapes_are_valid() {
    for (int f = 0; f <= kFamilyCount; ++f) {
        const Shape& s = ShapeOf((Family)f);
        assert(s.nv >= 4 && s.nv <= Shape::kMaxV);
        assert(s.ne >= 3 && s.ne <= Shape::kMaxE);
        for (int v = 0; v < s.nv; ++v)
            assert(s.x[v] >= 0.0f && s.x[v] <= 1.0f && s.y[v] >= 0.0f && s.y[v] <= 1.0f);
        for (int e = 0; e < s.ne; ++e) {
            assert(s.ea[e] < s.nv && s.eb[e] < s.nv && s.ea[e] != s.eb[e]);
        }
        for (int v = 0; v < s.nv; ++v) assert(s.FirstEdge(v) <= s.ne);
    }
    // STILL's lone star has no edge: it lights after the last edge.
    const Shape& still = ShapeOf(Family::STILL);
    assert(still.FirstEdge(5) == still.ne);
}

int main() {
    layout_fits_every_size();
    fewer_cards_center();
    hit_test();
    legendary_flips_last();
    plain_draw_is_about_two_seconds();
    phase_progression();
    dissolve_wave_moves_outward();
    family_codes();
    shapes_are_valid();
    std::puts("aug_card_model_test: all passed");
    return 0;
}
