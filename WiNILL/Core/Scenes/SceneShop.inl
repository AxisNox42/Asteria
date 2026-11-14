// Included by SceneMenus.cpp to keep shared menu state and helpers private.

// Shop copy in KR / EN / JP.
static const wchar_t* ShopText(const wchar_t* kr, const wchar_t* en,
                               const wchar_t* jp) {
    if (IsChinese()) return ZhTranslate(en);
    const int li = LangIndex();
    return li == 0 ? kr : (li == 2 ? jp : en);
}

// SHOP: a tabbed grid of everything on sale, grouped by tier on the left; the
// selected item's details and its purchase button on the right. Items have no
// tier of their own, so they are grouped by the price of their first purchase.
namespace shopgrid {

enum class Kind { Meta, Theme, Aug };

struct Entry {
    Kind      kind;
    int       id;
    int       tier;       // price band 0..2 (Codex entries: 0)
    long long cost;       // price of the first purchase (0 = not for sale)
    float     r, g, b;    // accent colour
};

struct Section {
    const wchar_t* label;
    float          r, g, b;
    int            first, count;
};

// Price bands by first-purchase cost: BASIC <= kTierMax[0] < ADVANCED <= kTierMax[1] < LEGENDARY.
// Change these two numbers to rebalance the tiers.
constexpr long long kTierMax[2] = { 150, 500 };

inline int TierOfCost(long long c) {
    return c <= kTierMax[0] ? 0 : (c <= kTierMax[1] ? 1 : 2);
}
inline const wchar_t* TierName(int t) {
    static const wchar_t* kName[3][3] = {
        { L"기본", L"고급", L"전설" },
        { L"BASIC", L"ADVANCED", L"LEGENDARY" },
        { L"基本", L"上級", L"伝説" },
    };
    const wchar_t* en = kName[1][std::max(0, std::min(2, t))];
    if (IsChinese()) return ZhTranslate(en);
    return kName[LangIndex()][std::max(0, std::min(2, t))];
}
inline void TierColor(int t, float& r, float& g, float& b) {
    static const float kC[3][3] = {
        { 0.50f, 0.84f, 1.00f }, { 0.74f, 0.58f, 1.00f }, { 1.00f, 0.80f, 0.30f } };
    const int i = std::max(0, std::min(2, t));
    r = kC[i][0]; g = kC[i][1]; b = kC[i][2];
}
inline int Key(Kind k, int id) { return (int)k * 1000 + id; }

constexpr int kMaxEntries = 128;
constexpr int kTabCount = 4;

} // namespace shopgrid

// 0 = rifle, 1 = field, -1 = neither (the Codex-only shop entries).
static int ShopAugWeaponGroup(AugType) {
    // The weapon augments (minigun, rifle stability, static field) left the card pool.
    return -1;
}

// Fills `out` and `secs` for a tab. Returns the entry count.
static int ShopBuildEntries(int tab, shopgrid::Entry* out, shopgrid::Section* secs,
                            int& secCount) {
    using namespace shopgrid;
    int n = 0;
    secCount = 0;
    if (tab <= 2) {
        Entry pool[32];
        int pn = 0;
        if (tab == 0 || tab == 1) {
            for (int i = 0; i < META_COUNT; ++i) {
                if (i == META_RESERVED_STARTAUG || i == META_RESERVED_VISION) continue;
                float r, g, b;
                const long long cost = META_DEFS[i].baseCost;
                TierColor(TierOfCost(cost), r, g, b);
                pool[pn++] = { Kind::Meta, i, TierOfCost(cost), cost, r, g, b };
            }
        }
        if (tab == 0 || tab == 2) {
            for (int i = 1; i < ACCENT_COUNT; ++i)
                pool[pn++] = { Kind::Theme, i, TierOfCost(ACCENT_THEMES[i].cost),
                               ACCENT_THEMES[i].cost, ACCENT_THEMES[i].r,
                               ACCENT_THEMES[i].g, ACCENT_THEMES[i].b };
        }
        for (int t = 0; t < 3; ++t) {
            // Cheapest first inside a tier.
            Entry inTier[32];
            int tn = 0;
            for (int i = 0; i < pn; ++i) if (pool[i].tier == t) inTier[tn++] = pool[i];
            std::stable_sort(inTier, inTier + tn,
                [](const Entry& a, const Entry& b) { return a.cost < b.cost; });
            if (tn == 0) continue;
            Section s;
            s.label = TierName(t);
            TierColor(t, s.r, s.g, s.b);
            s.first = n;
            s.count = tn;
            for (int i = 0; i < tn; ++i) out[n++] = inTier[i];
            secs[secCount++] = s;
        }
        return n;
    }
    // CODEX: weapon augments, one section per weapon system (not for sale).
    static const wchar_t* kGroupName[2][3] = {
        { L"소총 별자리", L"RIFLE CONSTELLATION", L"ライフル星座" },
        { L"위성 배열", L"FIELD ARRAY", L"フィールド配列" },
    };
    static const float kGroupColor[2][3] = {
        { 0.72f, 0.92f, 0.42f }, { 0.62f, 0.48f, 1.00f } };
    for (int grp = 0; grp < 2; ++grp) {
        Section s;
        s.label = kGroupName[grp][LangIndex()];
        s.r = kGroupColor[grp][0]; s.g = kGroupColor[grp][1]; s.b = kGroupColor[grp][2];
        s.first = n;
        s.count = 0;
        for (int i = 0; i < AUG_TOTAL && n < kMaxEntries; ++i) {
            if (ShopAugWeaponGroup(ALL_AUGS[i].type) != grp)
                continue;
            float r, g, b;
            GetRarityColor(ALL_AUGS[i].rarity, r, g, b);
            out[n++] = { Kind::Aug, i, 0, 0, r, g, b };
            ++s.count;
        }
        if (s.count > 0) secs[secCount++] = s;
    }
    return n;
}

// Line glyph of a permanent upgrade (HP / ATK / MOVE / SLOT).
static void ShopDrawMetaGlyph(int id, float cx, float cy, float size,
                              float r, float g, float b, float a) {
    BindMainShader();
    const float h = size * 0.5f;
    const float th = std::max(1.2f, size * 0.05f);
    const float k = h * 0.52f;
    // Outer frame: an octagon-cut square.
    const float c = h * 0.34f;
    LogoLine(cx - h + c, cy - h, cx + h - c, cy - h, th, r, g, b, a);
    LogoLine(cx + h - c, cy - h, cx + h, cy - h + c, th, r, g, b, a);
    LogoLine(cx + h, cy - h + c, cx + h, cy + h - c, th, r, g, b, a);
    LogoLine(cx + h, cy + h - c, cx + h - c, cy + h, th, r, g, b, a);
    LogoLine(cx + h - c, cy + h, cx - h + c, cy + h, th, r, g, b, a);
    LogoLine(cx - h + c, cy + h, cx - h, cy + h - c, th, r, g, b, a);
    LogoLine(cx - h, cy + h - c, cx - h, cy - h + c, th, r, g, b, a);
    LogoLine(cx - h, cy - h + c, cx - h + c, cy - h, th, r, g, b, a);
    switch (id) {
    case META_HP:      // plus
        LogoLine(cx - k, cy, cx + k, cy, th * 1.4f, r, g, b, a);
        LogoLine(cx, cy - k, cx, cy + k, th * 1.4f, r, g, b, a);
        break;
    case META_DMG:     // up chevron over a bar
        LogoLine(cx - k, cy + k * 0.2f, cx, cy - k * 0.7f, th * 1.4f, r, g, b, a);
        LogoLine(cx + k, cy + k * 0.2f, cx, cy - k * 0.7f, th * 1.4f, r, g, b, a);
        LogoLine(cx - k * 0.7f, cy + k * 0.75f, cx + k * 0.7f, cy + k * 0.75f, th * 1.2f, r, g, b, a);
        break;
    case META_MOVE:    // two forward chevrons
        for (int i = 0; i < 2; ++i) {
            const float ox = (i == 0 ? -0.35f : 0.30f) * k;
            LogoLine(cx + ox - k * 0.5f, cy - k * 0.8f, cx + ox + k * 0.3f, cy, th * 1.3f, r, g, b, a);
            LogoLine(cx + ox - k * 0.5f, cy + k * 0.8f, cx + ox + k * 0.3f, cy, th * 1.3f, r, g, b, a);
        }
        break;
    default:           // identity slot: a slot frame with a node
        LogoLine(cx - k, cy - k * 0.6f, cx + k, cy - k * 0.6f, th, r, g, b, a);
        LogoLine(cx - k, cy + k * 0.6f, cx + k, cy + k * 0.6f, th, r, g, b, a);
        drawDiamond(cx, cy, size * 0.10f, r, g, b, a);
        break;
    }
}

// Colour profile glyph: a ring with a soft coloured core.
static void ShopDrawThemeGlyph(float cx, float cy, float size,
                               float r, float g, float b, float a) {
    DrawConstellationDisc(cx, cy, size * 0.58f, r, g, b, 0.14f * a, false);
    BindMainShader();
    const int seg = 28;
    const float rad = size * 0.40f;
    for (int i = 0; i < seg; ++i) {
        const float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
        LogoLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad,
                 cx + std::cos(a1) * rad, cy + std::sin(a1) * rad,
                 std::max(1.2f, size * 0.05f), r, g, b, a);
    }
    drawDiamond(cx, cy, size * 0.16f, r, g, b, a);
}

// Payload (augment) glyph. Some augments have no icon art; those get a hexagon
// frame with a core node so no entry is ever blank.
static void ShopDrawPayloadGlyph(AugType t, float cx, float cy, float size,
                                 float r, float g, float b, float a) {
    BindMainShader();
    const float rad = size * 0.5f;
    const float th = std::max(1.2f, size * 0.05f);
    for (int i = 0; i < 6; ++i) {
        const float a0 = 6.2831853f * i / 6.0f - 1.5707963f;
        const float a1 = 6.2831853f * (i + 1) / 6.0f - 1.5707963f;
        LogoLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad,
                 cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, th, r, g, b, a);
    }
    for (int i = 0; i < 3; ++i) {
        const float a0 = 6.2831853f * i / 3.0f - 1.5707963f;
        LogoLine(cx, cy, cx + std::cos(a0) * rad * 0.62f, cy + std::sin(a0) * rad * 0.62f,
                 th, r, g, b, a * 0.8f);
    }
    drawDiamond(cx, cy, size * 0.12f, r, g, b, a);
}

void Scene_Shop(const SceneCtx& c) {
    using namespace shopgrid;
    const float sw = c.sw, sh = c.sh;
    const double mx = c.mx, my = c.my;
    const bool lmb = c.lmb;
    const float delta = c.delta;
    const float now = (float)glfwGetTime();
    float dt = delta; if (dt > 0.05f) dt = 0.05f;

    // ── State ──────────────────────────────────────────────────────────
    static int   s_tab = 0;
    static float s_tabHov[kTabCount] = {};
    static float s_cellHov[kMaxEntries] = {};
    static float s_cellSel[kMaxEntries] = {};
    static int   s_selKey = -1;
    static float s_scroll = 0.0f, s_scrollTarget = 0.0f;
    static float s_detailT = 1.0f;
    static float s_backHov = 0.0f, s_buyHov = 0.0f;
    static bool  s_backExit = false;
    static float s_backOutT = 0.0f;
    static bool  s_prevEsc = false;
    static bool  s_prevKeys[5] = {};          // up, down, left, right, enter
    static float s_buyFlashT = 1.0f;           // 0..1 after a purchase
    static int   s_buyFlashKey = -1;
    static bool  s_hadFocusKey = false;

    if (g_LobbyPanel != LobbyPanel::Shop)
        DrawMenuBackground(sw, sh, delta, false);

    if (s_ShopNeedsOpenInit) {
        s_ShopNeedsOpenInit = false;
        s_tab = 0; s_selKey = -1;
        s_scroll = s_scrollTarget = 0.0f;
        s_backExit = false; s_backOutT = 0.0f;
        s_detailT = 1.0f; s_buyFlashT = 1.0f;
        for (float& v : s_cellHov) v = 0.0f;
        for (float& v : s_cellSel) v = 0.0f;
    }
    g_ShopEntryT = std::min(1.0f, g_ShopEntryT + dt);
    // Same side contrast fields as SETTINGS, scaled by the menu darkness setting.
    DrawPersistentSceneSideVignettes(sw, sh, kWideSceneLinearAlpha *
                                     std::min(1.0f, g_ShopEntryT * 2.0f));

    // ── Input edges and exit ───────────────────────────────────────────
    const bool rawRmb = c.window && glfwGetMouseButton(c.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool rmbClick = rawRmb && !g_RmbPrev;
    const bool escClick = EscPressedEdge(c, s_prevEsc);
    const bool lmbClick = lmb && !g_LmbPrev;
    const bool inputReady = !s_backExit && g_ShopEntryT >= 0.55f;
    auto beginBack = [&]() {
        if (!inputReady) return;
        s_backExit = true;
        s_backOutT = 0.0f;
        s_ShopBackRequested = false;
    };
    if (s_ShopBackRequested) {
        if (inputReady) beginBack(); else s_ShopBackRequested = false;
    }
    if ((rmbClick || escClick) && inputReady) Audio::Play(Audio::Cue::UiBack);
    if (rmbClick || escClick) beginBack();
    bool finishBack = false;
    if (s_backExit) {
        s_backOutT += dt;
        if (s_backOutT >= kOutgameTransitionDuration) {
            s_backOutT = kOutgameTransitionDuration;
            finishBack = true;
        }
    }
    const float entryA = SceneTransitionEase(g_ShopEntryT / kOutgameTransitionDuration);
    const float backP = s_backExit ? SceneTransitionEase(s_backOutT / kOutgameTransitionDuration) : 0.0f;
    const float wake = s_backExit ? (1.0f - backP) : entryA;
    const float slideIn = (1.0f - wake) * 36.0f;
    SetSceneTextureReveal(1.0f);

    // ── Entries ─────────────────────────────────────────────────────────
    Entry entries[kMaxEntries];
    Section secs[4];
    int secCount = 0;
    const int entryCount = ShopBuildEntries(s_tab, entries, secs, secCount);
    int selIndex = -1;
    for (int i = 0; i < entryCount; ++i)
        if (Key(entries[i].kind, entries[i].id) == s_selKey) selIndex = i;
    if (selIndex < 0 && entryCount > 0) {
        selIndex = 0;
        s_selKey = Key(entries[0].kind, entries[0].id);
        s_detailT = 0.0f;
    }

    // ── Layout ──────────────────────────────────────────────────────────
    const float ui = UiScale(sw, sh);
    const float margin = std::max(58.0f * ui, sw * 0.075f);
    const float titleY = 40.0f * ui;
    const float tabsY = titleY + 74.0f * ui;
    const float gridX = margin;
    const float gridTop = tabsY + 62.0f * ui;
    const float gridBottom = sh - 190.0f * ui;
    const float gridW = std::max(420.0f * ui, sw * 0.575f - margin);
    const float gridH = std::max(120.0f * ui, gridBottom - gridTop);
    const float cellW = 122.0f * ui, cellH = 146.0f * ui;
    const int cols = std::max(1, (int)((gridW + 10.0f * ui) / cellW));
    const float secHdrH = 52.0f * ui, secGap = 22.0f * ui;
    const float panelX = std::min(sw - 360.0f * ui, gridX + gridW + 70.0f * ui);
    const float panelR = sw - std::max(48.0f * ui, sw * 0.05f);
    const float panelW = std::max(300.0f * ui, panelR - panelX);

    // Cell positions in content space (y relative to gridTop, before scrolling).
    float cellX[kMaxEntries], cellY[kMaxEntries];
    float secY[4] = {};
    float contentH = 0.0f;
    for (int s = 0; s < secCount; ++s) {
        secY[s] = contentH;
        contentH += secHdrH;
        const int rows = (secs[s].count + cols - 1) / cols;
        for (int k = 0; k < secs[s].count; ++k) {
            const int idx = secs[s].first + k;
            cellX[idx] = gridX + (float)(k % cols) * cellW;
            cellY[idx] = contentH + (float)(k / cols) * cellH;
        }
        contentH += rows * cellH + secGap;
    }
    const float maxScroll = std::max(0.0f, contentH - gridH);

    // ── Wheel / keyboard ────────────────────────────────────────────────
    const bool overGrid = inputReady && mx >= gridX - 20.0f * ui && mx < gridX + gridW + 30.0f * ui
                       && my >= gridTop && my < gridTop + gridH;
    if (overGrid && g_ScrollAccum != 0.0f) {
        if (maxScroll > 0.0f) Audio::Play(Audio::Cue::UiScroll);
        s_scrollTarget -= g_ScrollAccum * 78.0f * ui;
        g_ScrollAccum = 0.0f;
    }
    auto keyHeld = [&](int a, int b) {
        return c.window && (glfwGetKey(c.window, a) == GLFW_PRESS ||
                            glfwGetKey(c.window, b) == GLFW_PRESS);
    };
    const bool keysNow[5] = {
        keyHeld(GLFW_KEY_UP, GLFW_KEY_W), keyHeld(GLFW_KEY_DOWN, GLFW_KEY_S),
        keyHeld(GLFW_KEY_LEFT, GLFW_KEY_A), keyHeld(GLFW_KEY_RIGHT, GLFW_KEY_D),
        keyHeld(GLFW_KEY_ENTER, GLFW_KEY_KP_ENTER) };
    bool keyEdge[5];
    for (int i = 0; i < 5; ++i) keyEdge[i] = keysNow[i] && !s_prevKeys[i] && !c.inputFocusChanged;
    for (int i = 0; i < 5; ++i) s_prevKeys[i] = keysNow[i];
    bool keyMoved = false;
    if (inputReady && selIndex >= 0 && (keyEdge[0] || keyEdge[1] || keyEdge[2] || keyEdge[3])) {
        int best = -1;
        float bestScore = 1e30f;
        const float cx0 = cellX[selIndex], cy0 = cellY[selIndex];
        for (int i = 0; i < entryCount; ++i) {
            if (i == selIndex) continue;
            const float dx = cellX[i] - cx0, dy = cellY[i] - cy0;
            float score = -1.0f;
            if (keyEdge[2] && dy > -1.0f && dy < 1.0f && dx < 0.0f) score = -dx;
            else if (keyEdge[3] && dy > -1.0f && dy < 1.0f && dx > 0.0f) score = dx;
            else if (keyEdge[0] && dy < -1.0f) score = -dy * 4.0f + std::fabs(dx);
            else if (keyEdge[1] && dy > 1.0f) score = dy * 4.0f + std::fabs(dx);
            if (score >= 0.0f && score < bestScore) { bestScore = score; best = i; }
        }
        if (best >= 0) {
            selIndex = best;
            s_selKey = Key(entries[best].kind, entries[best].id);
            s_detailT = 0.0f;
            keyMoved = true;
        }
    }
    if (keyMoved && selIndex >= 0) {
        // Keep the keyboard selection inside the visible band.
        const float top = cellY[selIndex] - secHdrH * 0.6f, bottom = cellY[selIndex] + cellH;
        if (top < s_scrollTarget) s_scrollTarget = top;
        if (bottom > s_scrollTarget + gridH) s_scrollTarget = bottom - gridH;
    }
    s_scrollTarget = std::max(0.0f, std::min(maxScroll, s_scrollTarget));
    {
        const float gap = s_scrollTarget - s_scroll;
        if (std::fabs(gap) < 0.4f) s_scroll = s_scrollTarget;
        else s_scroll += gap * (1.0f - std::exp(-dt * 12.0f));
    }
    s_detailT = UiApproach(s_detailT, 1.0f, dt, 7.0f);
    s_buyFlashT = std::min(1.0f, s_buyFlashT + dt / 0.55f);

    // ── Drawing helpers ─────────────────────────────────────────────────
    auto drawText = [&](TextRenderer& tr, const wchar_t* text, float x, float y, float scale,
                        float r, float g, float b, float a, float maxW = 0.0f,
                        bool alignRight = false) {
        if (!text || !text[0] || a <= 0.002f) return;
        float sc = scale;
        float w = tr.Width(text, sc);
        if (maxW > 2.0f && w > maxW) { sc *= maxW / w; w = tr.Width(text, sc); }
        DrawShadowedText(tr, text, alignRight ? x - w : x, y, sc, r, g, b, a, 0.84f);
    };
    const float sSmall = UiTextScale(g_TextS, UiTextLevel::Supporting, ui);
    const float sBody = UiTextScale(g_TextS, UiTextLevel::Description, ui);
    const float sTitle = UiTextScale(g_TextL, UiTextLevel::Title, ui);
    BindMainShader();

    // Title and tabs.
    drawText(g_TextL, ShopText(L"상점", L"SHOP", L"ショップ"), gridX - slideIn, titleY, sTitle,
             1.0f, 1.0f, 1.0f, 0.95f * wake);
    {
        const float ruleY = titleY + 58.0f * ui;
        LogoLine(gridX - slideIn, ruleY, panelR, ruleY, 1.0f * ui, 0.34f, 0.72f, 1.0f, 0.16f * wake);
    }
    static const wchar_t* kTabName[3][kTabCount] = {
        { L"전체", L"영구 업그레이드", L"화면 프로필", L"도감" },
        { L"ALL", L"UPGRADES", L"PROFILES", L"CODEX" },
        { L"すべて", L"恒久強化", L"画面プロファイル", L"図鑑" },
    };
    {
        float tx = gridX - slideIn;
        const float tabScale = sBody * 1.15f;
        for (int t = 0; t < kTabCount; ++t) {
            const wchar_t* name = kTabName[LangIndex()][t];
            const float tw = g_TextS.Width(name, tabScale);
            const float th = g_TextS.Height(name, tabScale);
            const bool hov = inputReady && mx >= tx - 8.0f * ui && mx <= tx + tw + 8.0f * ui
                          && my >= tabsY - 6.0f * ui && my <= tabsY + th + 14.0f * ui;
            s_tabHov[t] = UpdateMenuCommandHover(s_tabHov[t], hov, dt);
            if (hov && lmbClick && s_tab != t) {
                Audio::Play(Audio::Cue::UiTab);
                s_tab = t; s_selKey = -1; s_scroll = s_scrollTarget = 0.0f;
                for (float& v : s_cellSel) v = 0.0f;
            }
            const bool sel = s_tab == t;
            const float a = (sel ? 1.0f : 0.58f + 0.30f * s_tabHov[t]) * wake;
            drawText(g_TextS, name, tx, tabsY, tabScale,
                     sel ? 1.0f : 0.72f, sel ? 1.0f : 0.84f, 1.0f, a);
            const float ulA = (sel ? 0.95f : 0.22f * s_tabHov[t]) * wake;
            if (ulA > 0.01f) {
                LogoLine(tx, tabsY + th + 8.0f * ui, tx + tw, tabsY + th + 8.0f * ui,
                         (sel ? 2.0f : 1.0f) * ui, 0.40f, 0.82f, 1.0f, ulA);
                if (sel) drawDiamond(tx + tw + 8.0f * ui, tabsY + th + 8.0f * ui, 2.6f * ui,
                                     0.40f, 0.82f, 1.0f, 0.8f * wake);
            }
            tx += tw + 40.0f * ui;
        }
    }

    // ── Grid (clipped) ──────────────────────────────────────────────────
    int clickedIndex = -1;
    {
        BatchFlush();
        glEnable(GL_SCISSOR_TEST);
        ScissorUnits(gridX - 24.0f * ui - slideIn, gridTop, gridW + 70.0f * ui, gridH);
        for (int s = 0; s < secCount; ++s) {
            const float sy = gridTop + secY[s] - s_scroll;
            if (sy + secHdrH < gridTop - 4.0f || sy > gridTop + gridH) { }
            else {
                const float hx = gridX - slideIn;
                drawText(g_TextS, secs[s].label, hx, sy + 6.0f * ui, sBody * 1.12f,
                         secs[s].r, secs[s].g, secs[s].b, 0.95f * wake);
                wchar_t cnt[16];
                swprintf_s(cnt, L"%d", secs[s].count);
                const float lw = g_TextS.Width(secs[s].label, sBody * 1.12f);
                LogoLine(hx + lw + 16.0f * ui, sy + 21.0f * ui, gridX + gridW - slideIn, sy + 21.0f * ui,
                         1.0f * ui, secs[s].r, secs[s].g, secs[s].b, 0.30f * wake);
                drawText(g_TextS, cnt, gridX + gridW - slideIn, sy + 4.0f * ui, sSmall,
                         secs[s].r, secs[s].g, secs[s].b, 0.70f * wake, 0.0f, true);
            }
            for (int k = 0; k < secs[s].count; ++k) {
                const int i = secs[s].first + k;
                const Entry& e = entries[i];
                const float x = cellX[i] - slideIn;
                const float y = gridTop + cellY[i] - s_scroll;
                if (y + cellH < gridTop - 4.0f || y > gridTop + gridH) {
                    s_cellHov[i] = UpdateMenuCommandHover(s_cellHov[i], false, dt);
                    continue;
                }
                const bool hov = inputReady && mx >= x && mx < x + cellW - 8.0f * ui
                              && my >= std::max(y, gridTop) && my < std::min(y + cellH - 6.0f * ui, gridTop + gridH);
                s_cellHov[i] = UpdateMenuCommandHover(s_cellHov[i], hov, dt);
                if (hov && lmbClick) clickedIndex = i;
                const bool sel = (i == selIndex);
                s_cellSel[i] = std::max(0.0f, std::min(1.0f,
                    s_cellSel[i] + (sel ? 1.0f : -1.0f) * dt / 0.28f));
                const float sa = s_cellSel[i] * s_cellSel[i] * (3.0f - 2.0f * s_cellSel[i]);
                const float hv = s_cellHov[i];

                // State.
                bool owned = false, maxed = false;
                int lv = 0, maxLv = 0;
                const wchar_t* name = L"";
                long long price = e.cost;
                if (e.kind == Kind::Meta) {
                    lv = g_MetaLv[e.id]; maxLv = META_DEFS[e.id].maxLv;
                    owned = lv > 0; maxed = lv >= maxLv;
                    name = MetaName(e.id);
                    price = MetaNextCost(e.id);
                } else if (e.kind == Kind::Theme) {
                    owned = ThemeOwned(e.id);
                    name = AccentName(e.id);
                } else {
                    name = ALL_AUGS[e.id].locName[LangIndex()];
                }

                const float cw = cellW - 8.0f * ui;
                const float cx = x + cw * 0.5f;
                const float gy = y + 42.0f * ui;
                const float gs = 52.0f * ui;
                const float a = (0.80f + 0.20f * hv + 0.2f * sa) * wake;

                // Selection: bracket corners that light up, with a soft glow.
                {
                    const float bx0 = x, bx1 = x + cw, by0 = y, by1 = y + cellH - 6.0f * ui;
                    const float arm = ((5.0f + 3.0f * hv) + (11.0f - (5.0f + 3.0f * hv)) * sa) * ui;
                    const float th = (0.9f + 0.8f * sa) * ui;
                    const float ba = ((0.36f * hv) + (1.0f - 0.36f * hv) * sa) * wake;
                    const float lift = 0.16f * sa;
                    const float br = e.r + (1.0f - e.r) * lift, bg = e.g + (1.0f - e.g) * lift,
                                bb = e.b + (1.0f - e.b) * lift;
                    for (int ix = 0; ix < 2; ++ix)
                        for (int iy = 0; iy < 2; ++iy) {
                            const float px = ix ? bx1 : bx0, py = iy ? by1 : by0;
                            const float sx = ix ? -1.0f : 1.0f, sy2 = iy ? -1.0f : 1.0f;
                            if (sa > 0.01f) {
                                const float gl = ba * sa * (0.88f + 0.12f * std::sin(now * 2.2f));
                                for (int layer = 0; layer < 2; ++layer) {
                                    const float gw = th * (layer == 0 ? 5.5f : 3.2f);
                                    const float ga = gl * (layer == 0 ? 0.10f : 0.18f);
                                    LogoLine(px - sx * th * 0.5f, py, px + sx * arm, py, gw, br, bg, bb, ga);
                                    LogoLine(px, py - sy2 * th * 0.5f, px, py + sy2 * arm, gw, br, bg, bb, ga);
                                }
                            }
                            LogoLine(px - sx * th * 0.5f, py, px + sx * arm, py, th, br, bg, bb, ba);
                            LogoLine(px, py - sy2 * th * 0.5f, px, py + sy2 * arm, th, br, bg, bb, ba);
                        }
                }
                // Purchase flash: a ring that opens from the glyph.
                if (s_buyFlashKey == Key(e.kind, e.id) && s_buyFlashT < 1.0f) {
                    const float f = s_buyFlashT;
                    DrawConstellationDisc(cx, gy, gs * (0.6f + 1.3f * f), e.r, e.g, e.b,
                                          0.35f * (1.0f - f) * wake, false);
                }

                // Glyph.
                const float ga = a * (e.kind == Kind::Aug ? 1.0f : (owned || e.kind == Kind::Meta ? 1.0f : 0.78f));
                if (e.kind == Kind::Meta) ShopDrawMetaGlyph(e.id, cx, gy, gs, e.r, e.g, e.b, ga);
                else if (e.kind == Kind::Theme) ShopDrawThemeGlyph(cx, gy, gs, e.r, e.g, e.b, ga);
                else ShopDrawPayloadGlyph(ALL_AUGS[e.id].type, cx, gy, gs, e.r, e.g, e.b, ga);
                if (owned && e.kind != Kind::Aug) {
                    // Owned mark: a small check at the glyph's corner.
                    const float mx0 = cx + gs * 0.46f, my0 = gy - gs * 0.50f;
                    LogoLine(mx0 - 5.0f * ui, my0 + 1.0f * ui, mx0 - 1.5f * ui, my0 + 5.0f * ui,
                             1.8f * ui, 0.60f, 1.0f, 0.70f, 0.95f * wake);
                    LogoLine(mx0 - 1.5f * ui, my0 + 5.0f * ui, mx0 + 6.0f * ui, my0 - 4.0f * ui,
                             1.8f * ui, 0.60f, 1.0f, 0.70f, 0.95f * wake);
                }

                // Name and price.
                drawText(g_TextS, name, cx - std::min(g_TextS.Width(name, sSmall), cw - 10.0f * ui) * 0.5f,
                         y + 76.0f * ui, sSmall, 0.88f, 0.94f, 1.0f, (0.72f + 0.2f * hv + 0.08f * sa) * wake,
                         cw - 10.0f * ui);
                wchar_t line[40] = L"";
                float pr = 1.0f, pg = 0.84f, pb = 0.34f;
                if (e.kind == Kind::Aug) {
                    swprintf_s(line, L"%ls", ShopText(L"도감", L"CODEX", L"図鑑"));
                    pr = 0.62f; pg = 0.74f; pb = 0.88f;
                } else if (maxed) {
                    swprintf_s(line, L"%ls", ShopText(L"최대", L"MAX", L"最大"));
                    pr = 0.60f; pg = 1.0f; pb = 0.70f;
                } else if (e.kind == Kind::Theme && owned) {
                    swprintf_s(line, L"%ls", ShopText(L"보유", L"OWNED", L"所持"));
                    pr = 0.60f; pg = 1.0f; pb = 0.70f;
                } else {
                    wchar_t num[24];
                    FormatGrouped(price, num, 24);
                    if (e.kind == Kind::Meta && maxLv > 0)
                        swprintf_s(line, L"%ls  Lv%d/%d", num, lv, maxLv);
                    else
                        swprintf_s(line, L"%ls", num);
                }
                drawText(g_TextS, line, cx - std::min(g_TextS.Width(line, sSmall), cw - 6.0f * ui) * 0.5f,
                         y + 76.0f * ui + g_TextS.Height(name, sSmall) + 8.0f * ui, sSmall,
                         pr, pg, pb, (0.80f + 0.2f * hv) * wake, cw - 6.0f * ui);
            }
        }
        BatchFlush();
        glDisable(GL_SCISSOR_TEST);
        // Scroll rail.
        if (maxScroll > 0.0f) {
            const float trackX = gridX + gridW + 26.0f * ui - slideIn;
            const float thumbH = std::max(30.0f * ui, gridH * gridH / (gridH + maxScroll));
            const float thumbY = gridTop + (gridH - thumbH) * (s_scroll / maxScroll);
            LogoLine(trackX, gridTop, trackX, gridTop + gridH, 0.7f * ui, 0.34f, 0.72f, 1.0f, 0.12f * wake);
            LogoLine(trackX, thumbY, trackX, thumbY + thumbH, 1.8f * ui, 0.34f, 0.82f, 1.0f, 0.55f * wake);
        }
    }
    if (clickedIndex >= 0) {
        {   // 이미 최대·보유 항목을 고름
            const Entry& ce = entries[clickedIndex];
            const bool ceMaxed = ce.kind == Kind::Meta ? g_MetaLv[ce.id] >= META_DEFS[ce.id].maxLv
                               : (ce.kind == Kind::Theme && ThemeOwned(ce.id));
            if (ceMaxed) Audio::Play(Audio::Cue::ShopMaxed);
        }
        s_selKey = Key(entries[clickedIndex].kind, entries[clickedIndex].id);
        selIndex = clickedIndex;
        s_detailT = 0.0f;
    }

    // ── Detail panel ────────────────────────────────────────────────────
    // Divider between the grid and the detail.
    {
        const float dx = panelX - 34.0f * ui + slideIn * 0.4f;
        LogoLine(dx, gridTop - 18.0f * ui, dx, gridTop + gridH + 40.0f * ui,
                 1.0f * ui, 0.34f, 0.72f, 1.0f, 0.14f * wake);
    }
    if (selIndex >= 0) {
        const Entry& e = entries[selIndex];
        const float da = wake * (0.35f + 0.65f * s_detailT);
        const float px = panelX + slideIn * 0.4f;
        const float pr = px + panelW;
        float y = gridTop - 6.0f * ui + (1.0f - s_detailT) * 8.0f * ui;

        // Kind tag.
        const wchar_t* kindText =
            e.kind == Kind::Meta ? ShopText(L"영구 업그레이드", L"PERMANENT UPGRADE", L"恒久強化")
          : e.kind == Kind::Theme ? ShopText(L"화면 프로필", L"DISPLAY PROFILE", L"画面プロファイル")
          : ShopText(L"도감 · 페이로드", L"CODEX · PAYLOAD", L"図鑑・ペイロード");
        // Dark tier colours (e.g. epic purple) are lifted toward white for small text.
        const float tl = 0.45f;
        const float lr = e.r + (1.0f - e.r) * tl, lg = e.g + (1.0f - e.g) * tl, lb = e.b + (1.0f - e.b) * tl;
        drawText(g_TextS, kindText, px, y, sSmall, lr, lg, lb, 0.96f * da);
        y += g_TextS.Height(kindText, sSmall) + 14.0f * ui;

        // Glyph and name.
        const float gs = 84.0f * ui;
        const float gcx = px + gs * 0.5f, gcy = y + gs * 0.5f;
        if (e.kind == Kind::Meta) ShopDrawMetaGlyph(e.id, gcx, gcy, gs, e.r, e.g, e.b, da);
        else if (e.kind == Kind::Theme) ShopDrawThemeGlyph(gcx, gcy, gs, e.r, e.g, e.b, da);
        else ShopDrawPayloadGlyph(ALL_AUGS[e.id].type, gcx, gcy, gs, e.r, e.g, e.b, da);
        const wchar_t* title = e.kind == Kind::Meta ? MetaName(e.id)
                             : e.kind == Kind::Theme ? AccentName(e.id)
                             : ALL_AUGS[e.id].locName[LangIndex()];
        const float tx = px + gs + 22.0f * ui;
        drawText(g_TextL, title, tx, y + 4.0f * ui, sTitle * 0.9f, 0.97f, 0.98f, 1.0f, 0.99f * da,
                 pr - tx);
        // Tier chip.
        wchar_t tierBuf[64];
        float tr_ = e.r, tg_ = e.g, tb_ = e.b;
        if (e.kind == Kind::Aug) {
            const AugRarity rr = ALL_AUGS[e.id].rarity;
            const wchar_t* rn =
                rr == AugRarity::COMMON ? ShopText(L"일반", L"COMMON", L"コモン")
              : rr == AugRarity::RARE ? ShopText(L"희귀", L"RARE", L"レア")
              : rr == AugRarity::EPIC ? ShopText(L"에픽", L"EPIC", L"エピック")
              : rr == AugRarity::LEGENDARY ? ShopText(L"전설", L"LEGENDARY", L"伝説")
              : ShopText(L"특수", L"SPECIAL", L"特殊");
            swprintf_s(tierBuf, L"%ls", rn);
        } else {
            TierColor(e.tier, tr_, tg_, tb_);
            swprintf_s(tierBuf, L"%ls  %ls", TierName(e.tier), ShopText(L"등급", L"TIER", L"ティア"));
        }
        drawDiamond(tx + 4.0f * ui, y + 4.0f * ui + g_TextL.Height(title, sTitle * 0.9f) + 22.0f * ui, 3.0f * ui,
                    tr_, tg_, tb_, 0.9f * da);
        drawText(g_TextS, tierBuf, tx + 16.0f * ui,
                 y + 4.0f * ui + g_TextL.Height(title, sTitle * 0.9f) + 12.0f * ui,
                 sSmall, tr_ + (1.0f - tr_) * tl, tg_ + (1.0f - tg_) * tl, tb_ + (1.0f - tb_) * tl, 0.95f * da);
        y += gs + 20.0f * ui;
        LogoLine(px, y, pr, y, 1.0f * ui, e.r, e.g, e.b, 0.30f * da);
        y += 18.0f * ui;

        // Description.
        const float lineH = g_TextS.Height(L"가", sBody) + 8.0f * ui;
        if (e.kind == Kind::Aug) {
            const wchar_t* desc = AugDesc(ALL_AUGS[e.id]);
            if (desc && desc[0]) {
                // Glossary words get their term icon in front, as on the cards.
                const IconDesc segs = WrapIconDesc(desc, sBody, panelW, ui, true);
                int line = 0;
                for (const DescLine& seg : segs.lines) {
                    if (line >= 6) break;
                    const float ly = y + line * lineH;
                    const float lh = g_TextS.Height(seg.text.c_str(), sBody);
                    DrawIconDescIcon(segs, seg, px, ly, lh, da);
                    DrawDescLinePieces(segs, seg, segs.TextX(seg, px), ly, lh, sBody,
                                       0.78f, 0.86f, 0.95f, 0.88f * da,
                                       [&](const wchar_t* s, float x, int) {
                                           drawText(g_TextS, s, x, ly, sBody, 0.78f, 0.86f, 0.95f, 0.88f * da,
                                                    panelW - (x - px));
                                       });
                    ++line;
                }
                y += std::max(1, line) * lineH;
            }
            y += 12.0f * ui;
            drawText(g_TextS, ShopText(L"플레이 중 획득 효과 · 도감 전용", L"IN-PLAY EFFECT · CODEX ONLY",
                                       L"プレイ中の獲得効果・図鑑専用"),
                     px, y, sSmall, 0.70f, 0.80f, 0.92f, 0.85f * da, panelW);
        } else if (e.kind == Kind::Meta) {
            const int lv = g_MetaLv[e.id], maxLv = META_DEFS[e.id].maxLv;
            drawText(g_TextS, ShopText(L"출격 전 적용되는 영구 효과", L"PERMANENT EFFECT APPLIED BEFORE LAUNCH",
                                       L"出撃前に適用される恒久効果"),
                     px, y, sBody, 0.78f, 0.86f, 0.95f, 0.88f * da, panelW);
            y += lineH + 6.0f * ui;
            // Level pips.
            for (int k = 0; k < maxLv; ++k) {
                const float cx2 = px + 10.0f * ui + k * 26.0f * ui;
                drawDiamond(cx2, y + 8.0f * ui, 10.0f * ui, e.r, e.g, e.b, (k < lv ? 1.0f : 0.26f) * da);
            }
            wchar_t lvBuf[40];
            swprintf_s(lvBuf, ShopText(L"레벨  %d / %d", L"LEVEL  %d / %d", L"レベル  %d / %d"), lv, maxLv);
            drawText(g_TextS, lvBuf, pr, y - 2.0f * ui, sBody, 0.92f, 0.96f, 1.0f, 0.9f * da, 0.0f, true);
            y += lineH + 6.0f * ui;
        } else {
            drawText(g_TextS, ShopText(L"메뉴와 플레이어 창에 쓰이는 색상", L"COLOUR USED BY MENUS AND THE PLAYER WINDOW",
                                       L"メニューとプレイヤー窓に使われる配色"),
                     px, y, sBody, 0.78f, 0.86f, 0.95f, 0.88f * da, panelW);
            y += lineH + 8.0f * ui;
            LogoLine(px, y + 6.0f * ui, px + panelW * 0.5f, y + 6.0f * ui, 5.0f * ui, e.r, e.g, e.b, 0.85f * da);
            y += lineH;
        }

        // Purchase block, anchored to the panel's bottom.
        if (e.kind != Kind::Aug) {
            const float bottom = gridTop + gridH + 20.0f * ui;
            const float btnH = 64.0f * ui;
            const float btnY = bottom + 10.0f * ui;
            const bool isMeta = e.kind == Kind::Meta;
            const int lv = isMeta ? g_MetaLv[e.id] : 0;
            const int maxLv = isMeta ? META_DEFS[e.id].maxLv : 0;
            const bool owned = isMeta ? lv > 0 : ThemeOwned(e.id);
            const bool maxed = isMeta ? lv >= maxLv : owned;
            const long long cost = isMeta ? MetaNextCost(e.id) : e.cost;
            const bool canBuy = !maxed && cost >= 0 && g_Coins >= cost;

            float ry = bottom - 112.0f * ui;
            wchar_t buf[96], num[32];
            if (!maxed) {
                FormatGrouped(cost, num, 32);
                swprintf_s(buf, ShopText(L"비용  %ls 별가루", L"COST  %ls STARDUST", L"費用  %ls 星屑"), num);
                drawText(g_TextS, buf, pr, ry, sBody, 1.0f, 0.84f, 0.34f, 0.98f * da, panelW, true);
                ry += lineH;
            } else {
                drawText(g_TextS, isMeta ? ShopText(L"최대 레벨", L"MAX LEVEL", L"最大レベル")
                                         : ShopText(L"구매 완료", L"OWNED", L"購入済み"),
                         pr, ry, sBody, 0.60f, 1.0f, 0.70f, 0.98f * da, panelW, true);
                ry += lineH;
            }
            FormatGrouped(g_Coins, num, 32);
            swprintf_s(buf, ShopText(L"보유 별가루  %ls", L"STARDUST  %ls", L"所持星屑  %ls"), num);
            drawText(g_TextS, buf, pr, ry, sBody, 0.88f, 0.94f, 1.0f, 0.80f * da, panelW, true);
            ry += lineH;
            if (canBuy) {
                FormatGrouped(g_Coins - cost, num, 32);
                swprintf_s(buf, ShopText(L"구매 후  %ls 별가루", L"AFTER  %ls STARDUST", L"購入後  %ls 星屑"), num);
                drawText(g_TextS, buf, pr, ry, sSmall, e.r, e.g, e.b, 0.86f * da, panelW, true);
            }

            const float btnW = std::min(panelW, 360.0f * ui);
            const float btnX = pr - btnW;
            const bool hovBtn = inputReady && mx >= btnX - 12.0f * ui && mx <= btnX + btnW + 12.0f * ui
                             && my >= btnY && my <= btnY + btnH;
            s_buyHov = UpdateMenuCommandHover(s_buyHov, hovBtn && canBuy, dt);
            const bool pressBuy = inputReady && ((hovBtn && lmbClick) || keyEdge[4]);
            const bool doBuy = canBuy && pressBuy;
            if (pressBuy && !canBuy) Audio::Play(Audio::Cue::UiDeny);   // 별가루 부족·최대 레벨
            if (doBuy) {
                Audio::Play(Audio::Cue::ShopBuy);
                g_Coins -= cost;
                if (isMeta) ++g_MetaLv[e.id];
                else g_ThemeOwned |= (1 << e.id);
                SaveGame();
                s_buyFlashT = 0.0f;
                s_buyFlashKey = Key(e.kind, e.id);
            }
            const wchar_t* label =
                maxed ? (isMeta ? ShopText(L"최대 레벨", L"MAX LEVEL", L"最大レベル")
                                : ShopText(L"구매 완료", L"OWNED", L"購入済み"))
                      : (!canBuy ? ShopText(L"별가루 부족", L"NOT ENOUGH", L"星屑不足")
                                 : (isMeta && lv > 0 ? ShopText(L"강화", L"UPGRADE", L"強化")
                                                     : ShopText(L"구매", L"BUY", L"購入")));
            const wchar_t* variants[3] = {
                maxed ? (isMeta ? L"최대 레벨" : L"구매 완료") : L"별가루 부족",
                maxed ? (isMeta ? L"MAX LEVEL" : L"OWNED") : L"NOT ENOUGH",
                maxed ? (isMeta ? L"最大レベル" : L"購入済み") : L"星屑不足" };
            const float cr = maxed ? 0.60f : (canBuy ? 0.34f : 0.96f);
            const float cg = maxed ? 1.00f : (canBuy ? 0.86f : 0.36f);
            const float cb = maxed ? 0.70f : (canBuy ? 1.00f : 0.38f);
            DrawUnifiedMenuCommand(label, btnX, btnY, btnW, btnH, cr, cg, cb, da,
                                   0.38f * (canBuy ? 1.0f : 0.5f) + 0.62f * s_buyHov, false,
                                   canBuy ? 0.10f + 0.08f * std::sin(now * 2.4f) : 0.0f, now,
                                   ui, true, true, true, variants, 3);
        }
    }

    // ── Back ────────────────────────────────────────────────────────────
    if (DrawBottomLeftBackButton(s_backHov, sw, sh, ui, mx, my, lmb && inputReady, g_LmbPrev,
                                 wake, now, dt))
        beginBack();

    if (finishBack) {
        s_backExit = false;
        s_backOutT = 0.0f;
        s_ShopBackRequested = false;
        g_LobbyPanel = LobbyPanel::None;
        s_MainMenuResumeFromPanel = true;
        g_MainMenuEntryT = 1.0f;
        g_GameManager.currentState = GameState::MAIN_MENU;
    }
}
