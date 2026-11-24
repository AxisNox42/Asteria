// F1 debug console: open/close, panel frame + tab rail, text boxes, number
// editing, screenshots. The tabs live in DebugTab*.cpp.
#include "DebugToolkit.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "DebugUI.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "Settings.h"
#include "Input.h"

extern TextRenderer g_TextS;
extern TextRenderer g_TextL;
extern float g_BatchAlpha;

DebugToolkit g_DebugToolkit;

namespace {

constexpr float RAIL_W = 76.0f;
constexpr float MARGIN = 10.0f;
constexpr float HEADER_H = 70.0f;
constexpr float FOOTER_H = 34.0f;

struct PanelLayout {
    float x, y, w, h;
};

// Docked on the right edge, full height. Collapsed = the tab rail only.
PanelLayout GetPanelLayout(float sw, float sh, bool collapsed) {
    float w = collapsed ? RAIL_W : std::min(520.0f, std::max(420.0f, sw * 0.32f));
    w = std::min(w, std::max(RAIL_W, sw - 2.0f * MARGIN));
    return { sw - w - MARGIN, MARGIN, w, std::max(200.0f, sh - 2.0f * MARGIN) };
}

uint32_t Crc32(const unsigned char* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1u)));
    }
    return ~crc;
}

uint32_t Adler32(const std::vector<unsigned char>& data) {
    uint32_t a = 1u;
    uint32_t b = 0u;
    for (unsigned char byte : data) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void AppendU32BE(std::vector<unsigned char>& out, uint32_t value) {
    out.push_back((unsigned char)((value >> 24) & 0xFFu));
    out.push_back((unsigned char)((value >> 16) & 0xFFu));
    out.push_back((unsigned char)((value >> 8) & 0xFFu));
    out.push_back((unsigned char)(value & 0xFFu));
}

void AppendPngChunk(std::vector<unsigned char>& png, const char type[4],
                    const std::vector<unsigned char>& payload) {
    AppendU32BE(png, (uint32_t)payload.size());
    const size_t typeOffset = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), payload.begin(), payload.end());
    const uint32_t crc = Crc32(png.data() + typeOffset,
                               4 + payload.size());
    AppendU32BE(png, crc);
}

bool SaveRgbaPng(const std::filesystem::path& path, int width, int height,
                 const std::vector<unsigned char>& pixelsBottomUp) {
    if (width <= 0 || height <= 0 ||
        pixelsBottomUp.size() != (size_t)width * (size_t)height * 4u)
        return false;

    std::vector<unsigned char> raw;
    raw.reserve((size_t)height * ((size_t)width * 4u + 1u));
    for (int y = height - 1; y >= 0; --y) {
        raw.push_back(0); // PNG filter: none
        const unsigned char* row = pixelsBottomUp.data() +
                                   (size_t)y * (size_t)width * 4u;
        raw.insert(raw.end(), row, row + (size_t)width * 4u);
    }

    // A valid zlib stream made entirely of stored DEFLATE blocks. This keeps
    // the toolkit self-contained and avoids adding a platform-specific image
    // library just for screenshots.
    std::vector<unsigned char> idat;
    idat.push_back(0x78);
    idat.push_back(0x01);
    size_t offset = 0;
    while (offset < raw.size()) {
        const size_t remaining = raw.size() - offset;
        const uint16_t blockSize = (uint16_t)std::min<size_t>(65535u, remaining);
        const bool finalBlock = offset + blockSize == raw.size();
        idat.push_back(finalBlock ? 0x01 : 0x00);
        idat.push_back((unsigned char)(blockSize & 0xFFu));
        idat.push_back((unsigned char)((blockSize >> 8) & 0xFFu));
        const uint16_t inverse = (uint16_t)~blockSize;
        idat.push_back((unsigned char)(inverse & 0xFFu));
        idat.push_back((unsigned char)((inverse >> 8) & 0xFFu));
        idat.insert(idat.end(), raw.begin() + (ptrdiff_t)offset,
                    raw.begin() + (ptrdiff_t)(offset + blockSize));
        offset += blockSize;
    }
    AppendU32BE(idat, Adler32(raw));

    std::vector<unsigned char> png = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };
    std::vector<unsigned char> ihdr;
    AppendU32BE(ihdr, (uint32_t)width);
    AppendU32BE(ihdr, (uint32_t)height);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // RGBA
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    AppendPngChunk(png, "IHDR", ihdr);
    AppendPngChunk(png, "IDAT", idat);
    AppendPngChunk(png, "IEND", {});

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(png.data()),
               (std::streamsize)png.size());
    return file.good();
}

std::wstring Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    const int millis = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000);
    std::tm localTime = {};
#ifdef _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    wchar_t buf[64] = {};
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]),
                  L"%04d-%02d-%02d_%02d%02d%02d%03d",
                  localTime.tm_year + 1900, localTime.tm_mon + 1,
                  localTime.tm_mday, localTime.tm_hour,
                  localTime.tm_min, localTime.tm_sec, millis);
    return buf;
}

std::wstring FormatLongLong(long long value) {
    wchar_t buf[64] = {};
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%lld", value);
    return buf;
}

} // namespace

// ── Tables ──────────────────────────────────────────────────────────────────

const std::vector<DebugToolkit::Field>& DebugToolkit::Fields() {
    static const std::vector<Field> fields = {
        // 공격
        { L"기본 피해",       FieldType::Float, offsetof(PlayerStats, baseDamage),       10.0,  0 },
        { L"피해 배율",       FieldType::Float, offsetof(PlayerStats, damageMultiplier), 0.1,   0 },
        { L"발사 간격(초)",   FieldType::Float, offsetof(PlayerStats, fireInterval),     0.01,  0 },
        { L"기본 발사 간격",  FieldType::Float, offsetof(PlayerStats, baseFireInterval), 0.01,  0 },
        { L"탄속",            FieldType::Float, offsetof(PlayerStats, bulletSpeed),      100.0, 0 },
        { L"탄 퍼짐(rad)",    FieldType::Float, offsetof(PlayerStats, bulletSpread),     0.02,  0 },
        { L"치명타 확률(%)",  FieldType::Int,   offsetof(PlayerStats, critChance),       5.0,   0 },
        { L"치명타 배율",     FieldType::Float, offsetof(PlayerStats, critMult),         0.25,  0 },
        // 생존
        { L"최대 체력",       FieldType::Float, offsetof(PlayerStats, maxHP),            25.0,  1 },
        { L"초당 회복",       FieldType::Float, offsetof(PlayerStats, regenPerSec),      0.25,  1 },
        // 이동·크기
        { L"이동 속도 배율",  FieldType::Float, offsetof(PlayerStats, moveSpeedMult),    0.1,   2 },
        { L"창 크기",         FieldType::Float, offsetof(PlayerStats, windowSize),       50.0,  2 },
        { L"플레이어 크기 배율", FieldType::Float, offsetof(PlayerStats, playerSizeMult), 0.1, 2 },
        // 성장
        { L"경험치 배율",     FieldType::Float, offsetof(PlayerStats, xpMult),           0.25,  3 },
        { L"처치 수",         FieldType::LongLong, offsetof(PlayerStats, killCount),     10.0,  3 },
    };
    return fields;
}

const std::vector<GameState>& DebugToolkit::CaptureStates() {
    static const std::vector<GameState> states = {
        GameState::MAIN_MENU,
        GameState::CODEX,
        GameState::TUTORIAL,
        GameState::CREATIVE_CONFIG,
        GameState::SETTINGS,
        GameState::READY,
        GameState::RUNNING,
        GameState::PAUSED,
        GameState::DYING,
        GameState::GAMEOVER,
        GameState::AUG_SELECT
    };
    return states;
}

const wchar_t* DebugToolkit::MobLabel(int index) {
    static const wchar_t* labels[] = {
        L"로터", L"제네시스", L"스웜", L"그라비티", L"퀘이사", L"스코프",
        L"Gimbal", L"Regulus", L"안타레스", L"MAGNETAR",
        L"OCTANS", L"OCTANS 테스트 창", L"ERIDANUS", L"LYNX", L"MIRA", L"TRIGON",
        L"허수아비", L"허수아비 모두 제거", L"HOROLOGIUM"
    };
    if (index >= 0 && index < (int)(sizeof(labels) / sizeof(labels[0]))) return labels[index];
    return L"알 수 없음";
}

const wchar_t* DebugToolkit::SceneLabel(GameState state) {
    switch (state) {
    case GameState::MAIN_MENU:       return L"메인 메뉴";
    case GameState::CODEX:           return L"도감";
    case GameState::TUTORIAL:        return L"튜토리얼";
    case GameState::CREATIVE_CONFIG: return L"크리에이티브 설정";
    case GameState::SETTINGS:        return L"설정";
    case GameState::READY:           return L"준비";
    case GameState::RUNNING:         return L"진행 중";
    case GameState::PAUSED:          return L"일시정지";
    case GameState::DYING:           return L"사망 처리";
    case GameState::GAMEOVER:        return L"게임 오버";
    case GameState::AUG_SELECT:      return L"카드 선택";
    default:                         return L"장면";
    }
}

// ── Basics ──────────────────────────────────────────────────────────────────

void DebugToolkit::Configure(const DebugToolkitContext& context) {
    context_ = context;
    configured_ = context_.stats && context_.game && context_.monsters;
    spawnCount_ = 1;
    fieldDelta_.assign(Fields().size(), 0.0);
    ResetTransientInput();
}

void DebugToolkit::ResetTransientInput() {
    editing_ = false;
    replaceEditBuffer_ = false;
    editBuffer_.clear();
    editingRuntime_ = -1;
}

void DebugToolkit::SetMessage(const std::wstring& message) {
    lastMessage_ = message;
}

// ── Text boxes ──────────────────────────────────────────────────────────────

std::wstring* DebugToolkit::TextOf(TextFocus which) {
    switch (which) {
    case TextFocus::AugSearch:  return &augSearch_;
    case TextFocus::SmokeSteps: return &smokeSteps_;
    default:                    return nullptr;
    }
}

void DebugToolkit::AugSearchInsert(const wchar_t* text) {
    std::wstring* s = TextOf(textFocus_);
    if (!text || !s) return;
    const size_t cap = textFocus_ == TextFocus::SmokeSteps ? 400u : 40u;
    for (const wchar_t* p = text; *p && s->size() < cap; ++p)
        if (*p >= 32 && *p != 127) s->push_back(*p);
    lastMessage_.clear();
}

void DebugToolkit::AugSearchBackspace() {
    std::wstring* s = TextOf(textFocus_);
    if (!s || s->empty()) return;
    s->pop_back();
    // Keep surrogate pairs whole.
    if (!s->empty() && s->back() >= 0xD800 && s->back() <= 0xDBFF) s->pop_back();
    lastMessage_.clear();
}

void DebugToolkit::AugSearchEscape() {
    std::wstring* s = TextOf(textFocus_);
    if (s && !s->empty()) s->clear();
    else textFocus_ = TextFocus::None;
    textComp_.clear();
    lastMessage_.clear();
}

// Input pass: focus / clear-button clicks. Draw pass: frame, text, caret.
void DebugToolkit::DrawTextBox(DebugUI& ui, float x, float y, float w, float h,
                               const std::wstring& text, const wchar_t* placeholder,
                               TextFocus which) {
    const DebugRect rc{ x, y, w, h };
    if (!ui.Visible(rc)) return;
    const bool focus = textFocus_ == which;
    const float clearW = 26.0f;
    const DebugRect clearRc{ x + w - clearW, y, clearW, h };
    if (ui.pass == DebugUI::Pass::Input) {
        if (!ui.click) return;
        if (ui.Hover(rc)) {
            if (!text.empty() && ui.Hover(clearRc)) {
                if (std::wstring* s = TextOf(which)) s->clear();
                textComp_.clear();
            }
            textFocus_ = which;
            lastMessage_.clear();
            ui.click = false;
        } else if (focus) {
            textFocus_ = TextFocus::None;   // the text stays; only the focus goes
            textComp_.clear();
        }
        return;
    }
    const std::wstring comp = focus ? textComp_ : std::wstring();
    const std::wstring full = text + comp;
    using namespace debugui_style;
    ui.Fill(rc, 0.095f, 0.100f, 0.110f, 1.0f);
    if (focus) ui.Frame(rc, kSelected.r + 0.15f, kSelected.g + 0.2f, kSelected.b + 0.25f, 1.0f);
    else       ui.Frame(rc, kLine.r, kLine.g, kLine.b, 1.0f);
    const float ts = 0.48f;
    const float tx = x + 10.0f;
    const float textMaxW = w - 20.0f - clearW;
    const float th = g_TextS.Height(L"Ag", ts);
    const float ty = y + std::max(3.0f, (h - th) * 0.5f);
    if (full.empty()) {
        g_TextS.Draw(placeholder, tx, ty, ts, kTextDim.r, kTextDim.g, kTextDim.b, 0.85f);
    } else {
        // Long text keeps its tail visible.
        std::wstring shown = full;
        while (shown.size() > 1 && g_TextS.Width(shown.c_str(), ts) > textMaxW)
            shown.erase(shown.begin());
        g_TextS.Draw(shown.c_str(), tx, ty, ts, kText.r, kText.g, kText.b, 1.0f);
        if (!comp.empty()) {
            // Underline the IME preedit part.
            const std::wstring done = shown.size() > comp.size()
                ? shown.substr(0, shown.size() - comp.size()) : L"";
            const float ux = tx + g_TextS.Width(done.c_str(), ts);
            const float uw = g_TextS.Width(shown.c_str(), ts) - (ux - tx);
            BindMainShader();
            drawRect(ux, ty + th + 1.0f, uw, 1.5f, kText.r, kText.g, kText.b, 0.85f);
        }
        g_TextS.Draw(L"X", clearRc.x + 8.0f, ty, ts, kTextDim.r, kTextDim.g, kTextDim.b, 1.0f);
    }
    if (focus) {
        std::wstring shown = text;
        while (shown.size() > 1 && g_TextS.Width((shown + comp).c_str(), ts) > textMaxW)
            shown.erase(shown.begin());
        const float caretX = tx + g_TextS.Width(shown.c_str(), ts);
        const float fullX = tx + g_TextS.Width((shown + comp).c_str(), ts);
        const bool blinkOn = std::fmod((float)glfwGetTime(), 1.0f) < 0.55f;
        if (blinkOn && comp.empty()) {
            BindMainShader();
            drawRect(caretX + 1.0f, ty, 1.5f, th, kText.r, kText.g, kText.b, 1.0f);
        }
        InputSetCodexImeAnchor(caretX, fullX, ty, th);
    }
}

// ── Number editing (능력치 tab) ─────────────────────────────────────────────

double DebugToolkit::ReadField(const Field& f) const {
    const char* raw = reinterpret_cast<const char*>(context_.stats) + f.offset;
    switch (f.type) {
    case FieldType::Float:    return *reinterpret_cast<const float*>(raw);
    case FieldType::Int:      return *reinterpret_cast<const int*>(raw);
    case FieldType::LongLong: return (double)*reinterpret_cast<const long long*>(raw);
    case FieldType::Bool:     return *reinterpret_cast<const bool*>(raw) ? 1.0 : 0.0;
    }
    return 0.0;
}

void DebugToolkit::WriteField(const Field& f, double v) {
    char* raw = reinterpret_cast<char*>(context_.stats) + f.offset;
    switch (f.type) {
    case FieldType::Float:
        *reinterpret_cast<float*>(raw) = (float)std::max(-1000000.0, std::min(1000000.0, v));
        break;
    case FieldType::Int:
        *reinterpret_cast<int*>(raw) = (int)std::llround(std::max(-1e9, std::min(1e9, v)));
        break;
    case FieldType::LongLong:
        *reinterpret_cast<long long*>(raw) = std::llround(std::max(-1e15, std::min(1e15, v)));
        break;
    case FieldType::Bool:
        *reinterpret_cast<bool*>(raw) = v != 0.0;
        break;
    }
}

// Adds `amount` to a field's value. Cards and level-ups keep adding on top of
// it; the amount is remembered so the row's undo takes back only this.
void DebugToolkit::NudgeField(int index, double amount) {
    if (!context_.stats || index < 0 || index >= (int)Fields().size()) return;
    if (fieldDelta_.size() != Fields().size()) fieldDelta_.assign(Fields().size(), 0.0);
    const Field& f = Fields()[(size_t)index];
    const double before = ReadField(f);
    WriteField(f, before + amount);
    if (context_.syncRuntime) context_.syncRuntime();
    fieldDelta_[(size_t)index] += ReadField(f) - before;
    if (std::fabs(fieldDelta_[(size_t)index]) < 1e-6) fieldDelta_[(size_t)index] = 0.0;
}

void DebugToolkit::BeginFieldEdit(int fieldIndex) {
    if (!context_.stats || fieldIndex < 0 ||
        fieldIndex >= (int)Fields().size()) return;
    selectedField_ = fieldIndex;
    editingRuntime_ = -1;
    const Field& field = Fields()[(size_t)fieldIndex];
    const double v = ReadField(field);
    editBuffer_ = field.type == FieldType::Float ? DebugFormatFloat((float)v)
                                                 : FormatLongLong(std::llround(v));
    editing_ = field.type != FieldType::Bool;
    replaceEditBuffer_ = editing_;
}

void DebugToolkit::BeginRuntimeEdit(int runtimeIndex) {
    if (!context_.game || runtimeIndex < 0 || runtimeIndex > 3) return;
    selectedField_ = -1;
    editingRuntime_ = runtimeIndex;
    editing_ = true;
    replaceEditBuffer_ = true;
    if (runtimeIndex == 0) editBuffer_ = DebugFormatFloat(context_.game->playerHP);
    else if (runtimeIndex == 1) editBuffer_ = FormatLongLong(context_.game->playerLevel);
    else if (runtimeIndex == 2) editBuffer_ = FormatLongLong(context_.game->xp);
    else editBuffer_ = FormatLongLong(context_.game->score);
}

void DebugToolkit::ApplyFieldEdit() {
    if (!editing_ || !context_.stats || !context_.game) return;
    double value = 0.0;
    try {
        size_t used = 0;
        value = std::stod(editBuffer_, &used);
        if (used != editBuffer_.size() || !std::isfinite(value)) {
            SetMessage(L"잘못된 숫자입니다");
            return;
        }
    } catch (...) {
        SetMessage(L"잘못된 숫자입니다");
        return;
    }
    if (editingRuntime_ >= 0) {
        GameManager& gm = *context_.game;
        if (editingRuntime_ == 0) {
            gm.playerHP = (float)std::max(-1000000.0, std::min(1000000.0, value));
        } else if (editingRuntime_ == 1) {
            gm.playerLevel = (int)std::max(1.0, std::min(1000000000.0, value));
        } else if (editingRuntime_ == 2) {
            gm.xp = std::max(0LL, (long long)value);
        } else {
            gm.score = std::max(0LL, (long long)value);
            gm.scoreAccum = (float)gm.score;
        }
        if (context_.syncRuntime) context_.syncRuntime();
        SetMessage(L"적용했습니다");
        ResetTransientInput();
        return;
    }
    if (selectedField_ < 0 || selectedField_ >= (int)Fields().size()) return;
    const Field& field = Fields()[(size_t)selectedField_];
    NudgeField(selectedField_, value - ReadField(field));
    SetMessage(std::wstring(field.name) + L" 적용");
    ResetTransientInput();
}

void DebugToolkit::HandleNumericKeys(GLFWwindow* window) {
    if (!editing_ || !window) return;
    static std::array<bool, 32> latched = {};
    auto pressedOnce = [&](int key, int slot) {
        const int state = glfwGetKey(window, key);
        if (state == GLFW_RELEASE) latched[(size_t)slot] = false;
        if (state == GLFW_PRESS && !latched[(size_t)slot]) {
            latched[(size_t)slot] = true;
            return true;
        }
        return false;
    };
    auto append = [&](wchar_t ch) {
        if (replaceEditBuffer_) {
            editBuffer_.clear();
            replaceEditBuffer_ = false;
        }
        if (editBuffer_.size() < 30) editBuffer_.push_back(ch);
    };
    for (int i = 0; i < 10; ++i) {
        if (pressedOnce(GLFW_KEY_0 + i, i)) append((wchar_t)(L'0' + i));
        if (pressedOnce(GLFW_KEY_KP_0 + i, 10 + i))
            append((wchar_t)(L'0' + i));
    }
    const bool ctrlDown = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool aPressed = pressedOnce(GLFW_KEY_A, 28);
    if (ctrlDown && aPressed) {
        editBuffer_.clear();
        replaceEditBuffer_ = false;
    }
    if (pressedOnce(GLFW_KEY_MINUS, 20) || pressedOnce(GLFW_KEY_KP_SUBTRACT, 29)) append(L'-');
    if (pressedOnce(GLFW_KEY_PERIOD, 21) || pressedOnce(GLFW_KEY_KP_DECIMAL, 22))
        append(L'.');
    if (pressedOnce(GLFW_KEY_BACKSPACE, 23)) {
        if (replaceEditBuffer_) {
            editBuffer_.clear();
            replaceEditBuffer_ = false;
        } else if (!editBuffer_.empty()) {
            editBuffer_.pop_back();
        }
    }
    if (pressedOnce(GLFW_KEY_DELETE, 27)) {
        editBuffer_.clear();
        replaceEditBuffer_ = false;
    }
    if (pressedOnce(GLFW_KEY_ENTER, 24) || pressedOnce(GLFW_KEY_KP_ENTER, 25)) {
        if (editBuffer_.empty())
            SetMessage(L"값을 입력하세요");
        else
            ApplyFieldEdit();
    }
    if (pressedOnce(GLFW_KEY_ESCAPE, 26)) {
        ResetTransientInput();
        SetMessage(L"편집을 취소했습니다");
    }
}

// ── Frame ───────────────────────────────────────────────────────────────────

bool DebugToolkit::BeginInput(GLFWwindow* window, float screenW, float screenH,
                              double mouseX, double mouseY,
                              bool lmb, bool lmbPrev) {
    if (!configured_) return false;
    PollSmoke();
    if (!g_DebugMode) {
        visible_ = false;
        captureCurrent_ = false;
        captureActive_ = false;
        inputCaptured_ = false;
        ResetTransientInput();
        return false;
    }

    // A new run rebuilds the stats, so the console's changes are gone with it.
    if (context_.game) {
        const GameState st = context_.game->currentState;
        if (st != lastState_ && (st == GameState::READY || st == GameState::MAIN_MENU))
            fieldDelta_.assign(Fields().size(), 0.0);
        lastState_ = st;
    }

    const bool wasVisible = visible_;
    static bool f1Released = true;
    static bool f12Released = true;
    if (window) {
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) f1Released = true;
        if (glfwGetKey(window, GLFW_KEY_F12) == GLFW_RELEASE) f12Released = true;
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && f1Released) {
            SetVisible(!visible_);
            f1Released = false;
            if (visible_ && tab_ == Tab::Augment) textFocus_ = TextFocus::AugSearch;
        }
        if (glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS && f12Released) {
            RequestCurrentScreenshot();
            f12Released = false;
        }
    }

    if (!visible_) {
        inputCaptured_ = false;
        return wasVisible;
    }

    lastMx_ = (float)mouseX;
    lastMy_ = (float)mouseY;
    const bool click = lmb && !lmbPrev;
    bool rclick = false;
    if (window) {
        const bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        rclick = rmb && !rmbPrev_;
        rmbPrev_ = rmb;
    }

    // Enter in a text box.
    if (textEnterPending_) {
        textEnterPending_ = false;
        if (textFocus_ == TextFocus::AugSearch && tab_ == Tab::Augment) {
            const auto& list = AugVisibleList();
            if (list.size() == 1) GrantAugmentFromConsole(list[0]);
            else if (list.empty()) SetMessage(L"맞는 증강이 없습니다");
            else SetMessage(L"결과가 하나일 때만 ENTER 로 받을 수 있습니다");
        } else if (textFocus_ == TextFocus::SmokeSteps && !smokeSteps_.empty()) {
            StartSmoke(smokeSteps_);
        }
    }

    // A large screenshot closes on any click.
    if (viewer_ >= 0 && (click || rclick)) {
        viewer_ = -1;
        inputCaptured_ = true;
        return true;
    }

    const PanelLayout p = GetPanelLayout(screenW, screenH, collapsed_);
    const bool overPanel = lastMx_ >= p.x && lastMx_ <= p.x + p.w &&
                           lastMy_ >= p.y && lastMy_ <= p.y + p.h;

    // Field-click spawn: while a mob is armed the field belongs to the console.
    if (window) {
        const bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (esc && !escPrev_ && armedMob_ >= 0 && !editing_ &&
            textFocus_ == TextFocus::None) {
            armedMob_ = -1;
            SetMessage(L"소환 대기를 취소했습니다");
        }
        escPrev_ = esc;
    }
    // A smoke script only reports "pressed" frames (no real button), so presses
    // less than 0.1 s apart count as one held press.
    bool freshPress = click;
    if (!window && lmb) {
        const double now = glfwGetTime();
        if (now - scriptedLmbT_ < 0.1) freshPress = false;
        scriptedLmbT_ = now;
    }
    if (!lmb && window && glfwGetTime() - scriptedLmbT_ > 0.1) dragSpawn_ = false;
    if (armedMob_ >= 0 && !overPanel) {
        constexpr float kDragSpacing = 70.0f;   // screen px between batches while dragging
        if (rclick) {
            armedMob_ = -1;
            dragSpawn_ = false;
            SetMessage(L"소환 대기를 취소했습니다");
        } else if (freshPress) {
            SpawnArmedAt(lastMx_, lastMy_);
            dragSpawn_ = true;
            dragLastX_ = lastMx_;
            dragLastY_ = lastMy_;
        } else if (lmb && dragSpawn_) {
            const float dx = lastMx_ - dragLastX_, dy = lastMy_ - dragLastY_;
            if (dx * dx + dy * dy >= kDragSpacing * kDragSpacing) {
                SpawnArmedAt(lastMx_, lastMy_);
                dragLastX_ = lastMx_;
                dragLastY_ = lastMy_;
            }
        }
        inputCaptured_ = true;
        return true;
    }

    DebugUI ui;
    ui.pass = DebugUI::Pass::Input;
    ui.mx = lastMx_;
    ui.my = lastMy_;
    ui.click = click;
    ui.rclick = rclick;
    ui.scroll = overPanel ? g_ScrollAccum : 0.0f;
    RunUI(ui, screenW, screenH, context_.game ? context_.game->currentState
                                              : GameState::MAIN_MENU);
    if (overPanel) g_ScrollAccum = 0.0f;

    HandleNumericKeys(window);
    if (!visible_) {   // closed from the panel
        inputCaptured_ = false;
        return true;
    }
    inputCaptured_ = pauseGame_ || overPanel || editing_ ||
                     textFocus_ != TextFocus::None || viewer_ >= 0 || armedMob_ >= 0;
    return inputCaptured_;
}

void DebugToolkit::RunUI(DebugUI& ui, float screenW, float screenH, GameState scene) {
    const PanelLayout p = GetPanelLayout(screenW, screenH, collapsed_);
    using namespace debugui_style;
    if (ui.Drawing()) {
        BindMainShader();
        drawRect(p.x, p.y, p.w, p.h, 0.075f, 0.080f, 0.090f, 0.97f);
        if (!collapsed_)
            drawRect(p.x + RAIL_W, p.y, 1.0f, p.h, kLine.r, kLine.g, kLine.b, 1.0f);
        ui.Frame({ p.x, p.y, p.w, p.h }, kLine.r, kLine.g, kLine.b, 1.0f);
    }

    // Tab rail.
    {
        const float rx = p.x + 9.0f, rw = RAIL_W - 18.0f;
        if (ui.Button({ rx, p.y + 12.0f, rw, 30.0f }, collapsed_ ? L"<<" : L">>")) {
            collapsed_ = !collapsed_;
            return;
        }
        static const wchar_t* const kTabs[] = { L"소환", L"증강", L"능력치", L"스모크" };
        float ty = p.y + 58.0f;
        for (int t = 0; t < (int)Tab::Count; ++t) {
            const Tab tab = (Tab)t;
            if (ui.Button({ rx, ty, rw, 52.0f }, kTabs[t], !collapsed_ && tab_ == tab,
                          true, nullptr, 0.48f)) {
                if (tab_ != tab) {
                    tab_ = tab;
                    armedMob_ = -1;
                    ResetTransientInput();
                    textFocus_ = tab == Tab::Augment ? TextFocus::AugSearch : TextFocus::None;
                    textComp_.clear();
                    lastMessage_.clear();
                }
                collapsed_ = false;
                return;
            }
            ty += 60.0f;
        }
        static const DebugColor kPause{ 1.0f, 0.80f, 0.40f };
        if (ui.Button({ rx, p.y + p.h - 96.0f, rw, 40.0f },
                      pauseGame_ ? L"멈춤" : L"진행", pauseGame_, true, &kPause, 0.46f)) {
            pauseGame_ = !pauseGame_;
            SetMessage(pauseGame_ ? L"콘솔을 여는 동안 게임이 멈춥니다"
                                  : L"콘솔을 연 채로 게임이 진행됩니다 (패널 위 입력만 막음)");
        }
        if (ui.Button({ rx, p.y + p.h - 48.0f, rw, 36.0f }, L"닫기", false, true, nullptr, 0.46f)) {
            SetVisible(false);
            return;
        }
    }
    if (collapsed_) return;

    // Header.
    const float cx = p.x + RAIL_W + 14.0f;
    const float cw = p.w - RAIL_W - 30.0f;
    if (ui.Drawing()) {
        g_TextS.Draw(L"디버그 도구", cx, p.y + 14.0f, 0.62f, kText.r, kText.g, kText.b, 1.0f);
        const wchar_t* sceneName = SceneLabel(scene);
        const float sw = g_TextS.Width(sceneName, 0.46f);
        g_TextS.Draw(sceneName, cx + cw - sw, p.y + 17.0f, 0.46f,
                     kTextDim.r, kTextDim.g, kTextDim.b, 1.0f);
        g_TextS.Draw(L"F1 닫기 · F12 캡처 · 휠 스크롤", cx, p.y + 44.0f, 0.42f,
                     kTextDim.r, kTextDim.g, kTextDim.b, 0.90f);
    }

    // Content (page-scrolled, rows outside the area are skipped).
    const float top = p.y + HEADER_H;
    const float bottom = p.y + p.h - FOOTER_H;
    const size_t ti = (size_t)tab_;
    float& scroll = pageScroll_[ti];
    if (tab_ == Tab::Augment) scroll = 0.0f;   // its list scrolls itself
    ui.left = cx;
    ui.width = cw;
    ui.clipTop = top;
    ui.clipBottom = bottom;
    ui.y = top - scroll;
    switch (tab_) {
    case Tab::Spawn:   TabSpawn(ui); break;
    case Tab::Augment: TabAugment(ui, bottom); break;
    case Tab::Stats:   TabStats(ui); break;
    case Tab::Smoke:   TabSmoke(ui); break;
    default: break;
    }
    if (!visible_) return;
    pageContentH_[ti] = ui.y + scroll - top;
    const float maxScroll = std::max(0.0f, pageContentH_[ti] - (bottom - top));
    if (ui.pass == DebugUI::Pass::Input && ui.scroll != 0.0f &&
        ui.Hover({ cx, top, cw, bottom - top })) {
        scroll -= ui.scroll * 48.0f;
        ui.scroll = 0.0f;
    }
    scroll = std::max(0.0f, std::min(maxScroll, scroll));

    // Footer: the latest message, or the number being typed.
    if (ui.Drawing()) {
        BindMainShader();
        drawRect(cx, bottom + 2.0f, cw, 1.0f, kLine.r, kLine.g, kLine.b, 1.0f);
        if (maxScroll > 0.0f) {
            const float trackH = bottom - top;
            const float thumbH = std::max(24.0f, trackH * trackH / pageContentH_[ti]);
            const float t = scroll / maxScroll;
            drawRect(cx + cw + 7.0f, top, 3.0f, trackH, kFill.r, kFill.g, kFill.b, 1.0f);
            drawRect(cx + cw + 7.0f, top + (trackH - thumbH) * t, 3.0f, thumbH,
                     kTextDim.r, kTextDim.g, kTextDim.b, 1.0f);
        }
        std::wstring footer = lastMessage_;
        DebugColor fc = kText;
        if (editing_) {
            footer = L"입력 중  " + (editBuffer_.empty() ? std::wstring(L"_") : editBuffer_) +
                     L"   ENTER 적용 · ESC 취소 · BACKSPACE 지우기";
            fc = { 1.0f, 0.85f, 0.45f };
        }
        if (!footer.empty()) {
            const std::wstring t = DebugFitText(footer.c_str(), 0.46f, cw);
            g_TextS.Draw(t.c_str(), cx, bottom + 9.0f, 0.46f, fc.r, fc.g, fc.b, 1.0f);
        }
    }
}

void DebugToolkit::Render(float screenW, float screenH, GameState sceneState) {
    if (!configured_ || !g_DebugMode || !visible_ || SuppressOverlayForCapture())
        return;
    const float oldAlpha = g_BatchAlpha;
    g_BatchAlpha = 1.0f;
    if (tab_ == Tab::Smoke) LoadOneThumb();
    DebugUI ui;
    ui.pass = DebugUI::Pass::Draw;
    ui.mx = lastMx_;
    ui.my = lastMy_;
    RunUI(ui, screenW, screenH, sceneState);
    const PanelLayout p = GetPanelLayout(screenW, screenH, collapsed_);
    if (viewer_ >= 0) DrawViewer(screenW, screenH, p.x);
    // Armed spawn: a cross and the mob's name follow the pointer on the field.
    if (armedMob_ >= 0 && lastMx_ < p.x && lastMx_ > -1000.0f) {
        const float mx = lastMx_, my = lastMy_;
        BindMainShader();
        drawRect(mx - 12.0f, my - 1.0f, 24.0f, 2.0f, 0.10f, 0.10f, 0.12f, 0.95f);
        drawRect(mx - 1.0f, my - 12.0f, 2.0f, 24.0f, 0.10f, 0.10f, 0.12f, 0.95f);
        std::wstring label = MobLabel(armedMob_);
        if (spawnCount_ > 1) label += L" x" + std::to_wstring(spawnCount_);
        const float tw = g_TextS.Width(label.c_str(), 0.48f);
        const float th = g_TextS.Height(label.c_str(), 0.48f);
        drawRect(mx + 14.0f, my + 10.0f, tw + 14.0f, th + 10.0f, 0.075f, 0.080f, 0.090f, 0.92f);
        g_TextS.Draw(label.c_str(), mx + 21.0f, my + 15.0f, 0.48f, 0.95f, 0.95f, 0.96f, 1.0f);
    }
    BindMainShader();
    g_BatchAlpha = oldAlpha;
}

// ── Screenshots ─────────────────────────────────────────────────────────────

void DebugToolkit::RequestCurrentScreenshot() {
    if (!g_DebugMode || captureActive_) return;
    captureCurrent_ = true;
    SetMessage(L"스크린샷을 대기열에 추가했습니다");
}

void DebugToolkit::RequestNamedScreenshot(const std::wstring& name, bool keepOverlay) {
    if (!g_DebugMode || captureActive_) return;
    captureCurrent_ = true;
    captureKeepOverlay_ = keepOverlay;
    captureName_ = name;
}

void DebugToolkit::RequestAllScreenshots() {
    if (!g_DebugMode || captureActive_) return;
    captureCurrent_ = false;
    captureKeepOverlay_ = false;
    captureActive_ = true;
    captureIndex_ = 0;
    SetMessage(L"전체 장면 캡처를 대기열에 추가했습니다");
}

GameState DebugToolkit::CaptureRenderState(GameState restoreState) const {
    if (!captureActive_ || captureIndex_ >= CaptureStates().size())
        return restoreState;
    return CaptureStates()[captureIndex_];
}

void DebugToolkit::CaptureFrameAfterRender(int width, int height,
                                           GameState renderedState) {
    if (!captureActive_ && !captureCurrent_) return;

    std::vector<unsigned char> pixels((size_t)width * (size_t)height * 4u);
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const std::filesystem::path directory = std::filesystem::path("Screenshots");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    const std::wstring stamp = Timestamp();
    std::wstring fileName = captureName_.empty()
        ? stamp + L"_" + SceneLabel(renderedState) : captureName_;
    captureName_.clear();
    if (captureActive_) {
        wchar_t index[32] = {};
        std::swprintf(index, sizeof(index) / sizeof(index[0]), L"_%02d",
                      (int)captureIndex_ + 1);
        fileName += index;
    }
    fileName += L".png";

    const bool ok = SaveRgbaPng(directory / std::filesystem::path(fileName),
                                width, height, pixels);
    SetMessage(ok ? L"스크린샷을 저장했습니다" : L"스크린샷 저장에 실패했습니다");
    captureCurrent_ = false;
    captureKeepOverlay_ = false;
    thumbsDirty_ = true;
    if (captureActive_) {
        ++captureIndex_;
        if (captureIndex_ >= CaptureStates().size()) {
            captureActive_ = false;
            captureIndex_ = 0;
            SetMessage(ok ? L"모든 장면을 저장했습니다" : L"장면 캡처에 실패했습니다");
        }
    }
}
