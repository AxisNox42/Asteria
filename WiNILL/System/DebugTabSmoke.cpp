// F1 debug console > 스모크 tab: screenshots, smoke runs in a separate process,
// and a thumbnail gallery of the results.
//
// A smoke run starts this same exe with ASTERIA_SMOKE (the steps), its own tag
// and ASTERIA_SAVE_DIR pointing at a copy of the save, so the player's save and
// the run in progress are never touched. The child moves itself to virtual
// desktop 2, writes Screenshots/<tag>_<LANG>_NN_<STEP>.png and closes.
#include "DebugToolkit.h"

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <cwctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "DebugUI.h"
#include "DrawPrim.h"
#include "TextRenderer.h"
#include "Settings.h"
#include "IconSystem.h"   // DrawIcon, stb_image (also brings <windows.h> on Windows)

#ifdef _WIN32
#  include <shellapi.h>
#  pragma comment(lib, "shell32.lib")
#endif

extern TextRenderer g_TextS;

struct DebugToolkit::SmokeRun {
#ifdef _WIN32
    HANDLE process = nullptr;
#endif
    double start = 0.0;
    std::wstring prefix;   // "<tag>_<LANG>_"
};

namespace {

namespace fs = std::filesystem;

constexpr double kSmokeTimeoutSec = 300.0;
constexpr int kThumbMaxW = 360;
const wchar_t* const kLangs[3] = { L"KR", L"EN", L"JP" };

struct SmokePreset {
    const wchar_t* label;
    const wchar_t* steps;
};
const SmokePreset kPresets[] = {
    { L"로비 · 설정 · 도감", L"MENU~2;SETTINGS~2;CODEX~2" },
    { L"준비 · 튜토리얼",    L"READY~2.5;TUTSTEP=0~2.5" },
    { L"전투 · 일시정지",    L"RUN~3;PAUSED~1.5" },
    { L"카드 선택",          L"AUGDEAL~2;UPDEAL~2;FRAMEDEAL~2" },
    { L"사망 · 게임 오버",   L"DEATH~2;GAMEOVER~3.5" },
    { L"디버그 콘솔",        L"DEBUG~1.5" },
    { L"보스 OCTANS",        L"NORMATEST=AIM~3;NORMATEST=DASH~3" },
    { L"보스 ERIDANUS",      L"CAELUMTEST=SPREAD~3;CAELUMTEST=SCAN~3" },
};

std::wstring ClockTag() {
    const std::time_t now = std::time(nullptr);
    std::tm t = {};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    wchar_t buf[32] = {};
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"f1_%02d%02d%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

bool StartsWithNoCase(const std::wstring& s, const wchar_t* prefix) {
    for (size_t i = 0; prefix[i]; ++i)
        if (i >= s.size() || std::towupper(s[i]) != std::towupper(prefix[i])) return false;
    return true;
}

// Reads a PNG (any path, Unicode file names included) into RGBA, alpha forced
// opaque: the window is a see-through overlay, so a shot's alpha would show
// the desktop through the thumbnail.
bool LoadPng(const fs::path& path, std::vector<unsigned char>& rgba, int& w, int& h) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    if (size <= 0) return false;
    std::vector<unsigned char> bytes((size_t)size);
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(bytes.data()), size)) return false;
    int n = 0;
    unsigned char* d = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!d) return false;
    rgba.assign(d, d + (size_t)w * (size_t)h * 4u);
    stbi_image_free(d);
    for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    return true;
}

void Downscale(std::vector<unsigned char>& rgba, int& w, int& h, int maxW) {
    if (w <= maxW) return;
    const int f = (w + maxW - 1) / maxW;
    const int nw = std::max(1, w / f), nh = std::max(1, h / f);
    std::vector<unsigned char> out((size_t)nw * (size_t)nh * 4u);
    for (int y = 0; y < nh; ++y)
        for (int x = 0; x < nw; ++x) {
            unsigned sum[4] = {};
            for (int dy = 0; dy < f; ++dy)
                for (int dx = 0; dx < f; ++dx) {
                    const unsigned char* p = &rgba[((size_t)(y * f + dy) * (size_t)w + (size_t)(x * f + dx)) * 4u];
                    for (int c = 0; c < 4; ++c) sum[c] += p[c];
                }
            for (int c = 0; c < 4; ++c)
                out[((size_t)y * (size_t)nw + (size_t)x) * 4u + (size_t)c] = (unsigned char)(sum[c] / (unsigned)(f * f));
        }
    rgba.swap(out);
    w = nw;
    h = nh;
}

GLuint Upload(const std::vector<unsigned char>& rgba, int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

fs::path ShotDir() { return fs::path("Screenshots"); }

struct ThumbPixels {
    bool ok = false;
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;        // after downscale
    int srcW = 0, srcH = 0;  // the shot's own size
};

} // namespace

// ── Smoke process ───────────────────────────────────────────────────────────

void DebugToolkit::StartSmoke(const std::wstring& steps) {
    if (smoke_) { SetMessage(L"스모크가 이미 실행 중입니다"); return; }
    if (steps.empty()) { SetMessage(L"실행할 단계가 없습니다"); return; }
#ifdef _WIN32
    wchar_t exe[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) { SetMessage(L"실행 파일 경로를 찾지 못했습니다"); return; }
    const fs::path exePath(exe);
    const fs::path exeDir = exePath.parent_path();
    const std::wstring tag = ClockTag();
    const std::wstring lang = kLangs[std::max(0, std::min(2, smokeLang_))];

    // The run's own save folder, holding a copy of the player's save so menus
    // look as they do now. Smoke runs never save, but the copy keeps even a
    // stray write away from the real file.
    std::error_code ec;
    const fs::path saveDir = fs::temp_directory_path(ec) / L"AsteriaF1Smoke" / L"save";
    fs::remove_all(saveDir, ec);
    fs::create_directories(saveDir, ec);
    if (context_.saveFilePath) {
        const fs::path src(context_.saveFilePath());
        if (fs::exists(src, ec))
            fs::copy_file(src, saveDir / src.filename(), fs::copy_options::overwrite_existing, ec);
    }

    // Environment: ours without any ASTERIA_SMOKE* / ASTERIA_SAVE_DIR, plus the run's.
    std::vector<std::wstring> env;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* p = block; *p; p += std::wcslen(p) + 1) {
            const std::wstring e(p);
            if (StartsWithNoCase(e, L"ASTERIA_SMOKE") || StartsWithNoCase(e, L"ASTERIA_SAVE_DIR="))
                continue;
            env.push_back(e);
        }
        FreeEnvironmentStringsW(block);
    }
    env.push_back(L"ASTERIA_SMOKE=" + steps);
    env.push_back(L"ASTERIA_SMOKE_LANG=" + lang);
    env.push_back(L"ASTERIA_SMOKE_TAG=" + tag);
    env.push_back(L"ASTERIA_SAVE_DIR=" + saveDir.wstring());
    std::sort(env.begin(), env.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    std::wstring blockOut;
    for (const auto& e : env) { blockOut += e; blockOut.push_back(L'\0'); }
    blockOut.push_back(L'\0');

    std::wstring cmd = L"\"" + exePath.wstring() + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exePath.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_UNICODE_ENVIRONMENT, blockOut.data(),
                        exeDir.c_str(), &si, &pi)) {
        SetMessage(L"스모크를 시작하지 못했습니다 (CreateProcess 실패)");
        return;
    }
    CloseHandle(pi.hThread);
    smoke_ = new SmokeRun();
    smoke_->process = pi.hProcess;
    smoke_->start = glfwGetTime();
    smoke_->prefix = tag + L"_" + lang + L"_";
    smokeStatus_ = L"실행 중 · 데스크톱 2 (" + tag + L")";
    SetMessage(L"스모크를 시작했습니다 - 끝나면 결과가 아래에 뜹니다");
#else
    SetMessage(L"스모크 실행은 Windows 에서만 됩니다");
#endif
}

void DebugToolkit::PollSmoke() {
    if (!smoke_) return;
#ifdef _WIN32
    const bool timedOut = glfwGetTime() - smoke_->start > kSmokeTimeoutSec;
    if (WaitForSingleObject(smoke_->process, 0) != WAIT_OBJECT_0 && !timedOut) return;
    DWORD code = 0;
    if (timedOut) TerminateProcess(smoke_->process, 1);
    else GetExitCodeProcess(smoke_->process, &code);
    CloseHandle(smoke_->process);
#endif
    thumbsPrefix_ = smoke_->prefix;
    RefreshThumbs();
    const int shots = (int)thumbs_.size();
#ifdef _WIN32
    if (timedOut) smokeStatus_ = L"시간 초과로 중지 · 스크린샷 " + std::to_wstring(shots) + L"장";
    else
#endif
    smokeStatus_ = shots > 0 ? L"완료 · 스크린샷 " + std::to_wstring(shots) + L"장"
                             : L"끝났지만 스크린샷이 없습니다 (단계 이름을 확인하세요)";
    SetMessage(smokeStatus_);
    delete smoke_;
    smoke_ = nullptr;
}

// ── Gallery ─────────────────────────────────────────────────────────────────

void DebugToolkit::FreeThumbs() {
    for (auto& t : thumbs_)
        if (t.tex) glDeleteTextures(1, &t.tex);
    thumbs_.clear();
    viewer_ = -1;
    if (viewerTex_) { glDeleteTextures(1, &viewerTex_); viewerTex_ = 0; }
    viewerW_ = viewerH_ = 0;
}

// Rereads the folder. Shots already shown keep their textures (and the large
// view stays open) so a new screenshot does not reload the whole gallery.
void DebugToolkit::RefreshThumbs() {
    std::vector<Thumb> old;
    old.swap(thumbs_);
    const std::wstring viewerFile =
        (viewer_ >= 0 && viewer_ < (int)old.size()) ? old[(size_t)viewer_].file : std::wstring();
    viewer_ = -1;
    thumbsDirty_ = false;
    std::error_code ec;
    struct Item { std::wstring name; fs::file_time_type time; };
    std::vector<Item> items;
    for (fs::directory_iterator it(ShotDir(), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const fs::path& p = it->path();
        std::wstring ext = p.extension().wstring();
        for (auto& c : ext) c = (wchar_t)std::towlower(c);
        if (ext != L".png") continue;
        const std::wstring name = p.filename().wstring();
        if (!thumbsPrefix_.empty() && name.rfind(thumbsPrefix_, 0) != 0) continue;
        items.push_back({ name, it->last_write_time(ec) });
    }
    if (thumbsPrefix_.empty()) {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.time > b.time; });
        if (items.size() > 12) items.resize(12);
    } else {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.name < b.name; });
        if (items.size() > 40) items.resize(40);
    }
    for (const auto& i : items) {
        Thumb t;
        t.file = i.name;
        for (auto& o : old)
            if (o.file == i.name) { t = std::move(o); o.tex = 0; o.file.clear(); break; }
        if (t.file == viewerFile && !viewerFile.empty()) viewer_ = (int)thumbs_.size();
        thumbs_.push_back(std::move(t));
    }
    for (auto& o : old)
        if (o.tex) glDeleteTextures(1, &o.tex);
    if (viewer_ < 0 && viewerTex_) {
        glDeleteTextures(1, &viewerTex_);
        viewerTex_ = 0;
        viewerW_ = viewerH_ = 0;
    }
}

// Decoding runs on worker threads (two at a time); the GL upload of a finished
// one happens here on the main thread, so the game never stalls on a PNG.
void DebugToolkit::LoadOneThumb() {
    if (thumbsDirty_) RefreshThumbs();
    int running = 0;
    for (auto& t : thumbs_) {
        if (t.tried || !t.job) continue;
        auto& fut = *static_cast<std::future<ThumbPixels>*>(t.job.get());
        if (fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++running; continue; }
        ThumbPixels px = fut.get();
        t.job.reset();
        t.tried = true;
        if (!px.ok) continue;
        t.w = px.srcW;
        t.h = px.srcH;
        t.tex = Upload(px.rgba, px.w, px.h);
    }
    for (auto& t : thumbs_) {
        if (running >= 2) break;
        if (t.tried || t.job) continue;
        const fs::path path = ShotDir() / t.file;
        auto* fut = new std::future<ThumbPixels>(std::async(std::launch::async, [path] {
            ThumbPixels px;
            px.ok = LoadPng(path, px.rgba, px.w, px.h);
            if (px.ok) {
                px.srcW = px.w;
                px.srcH = px.h;
                Downscale(px.rgba, px.w, px.h, kThumbMaxW);
            }
            return px;
        }));
        // Dropping a job waits for its thread (std::future from std::async).
        t.job = std::shared_ptr<void>(fut, [](void* p) { delete static_cast<std::future<ThumbPixels>*>(p); });
        ++running;
    }
}

void DebugToolkit::OpenScreenshotFolder() {
#ifdef _WIN32
    std::error_code ec;
    fs::create_directories(ShotDir(), ec);
    const fs::path dir = fs::absolute(ShotDir(), ec);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

void DebugToolkit::DrawViewer(float screenW, float screenH, float panelX) {
    if (viewer_ < 0 || viewer_ >= (int)thumbs_.size()) { viewer_ = -1; return; }
    const Thumb& t = thumbs_[(size_t)viewer_];
    if (!viewerTex_) {
        std::vector<unsigned char> rgba;
        int w = 0, h = 0;
        if (!LoadPng(ShotDir() / t.file, rgba, w, h)) { viewer_ = -1; SetMessage(L"이미지를 열지 못했습니다"); return; }
        viewerTex_ = Upload(rgba, w, h);
        viewerW_ = w;
        viewerH_ = h;
    }
    const float ax = 20.0f, ay = 20.0f;
    const float aw = std::max(100.0f, panelX - 40.0f), ah = std::max(100.0f, screenH - 70.0f);
    const float s = std::min(aw / (float)std::max(1, viewerW_), ah / (float)std::max(1, viewerH_));
    const float w = viewerW_ * s, h = viewerH_ * s;
    const float x = ax + (aw - w) * 0.5f, y = ay + (ah - h) * 0.5f;
    BindMainShader();
    drawRect(0.0f, 0.0f, panelX - 4.0f, screenH, 0.0f, 0.0f, 0.0f, 0.78f);
    DrawIcon(viewerTex_, x, y, w, h, 1.0f, 1.0f, 1.0f, 1.0f);
    BindMainShader();
    drawRect(x - 1.0f, y - 1.0f, w + 2.0f, 1.0f, 0.5f, 0.52f, 0.55f, 1.0f);
    drawRect(x - 1.0f, y + h, w + 2.0f, 1.0f, 0.5f, 0.52f, 0.55f, 1.0f);
    drawRect(x - 1.0f, y, 1.0f, h, 0.5f, 0.52f, 0.55f, 1.0f);
    drawRect(x + w, y, 1.0f, h, 0.5f, 0.52f, 0.55f, 1.0f);
    const std::wstring cap = t.file + L"   ·   클릭하면 닫힘";
    g_TextS.Draw(DebugFitText(cap.c_str(), 0.48f, aw).c_str(), x, y + h + 12.0f, 0.48f,
                 0.90f, 0.91f, 0.92f, 1.0f);
    (void)screenW;
}

// ── Tab ─────────────────────────────────────────────────────────────────────

void DebugToolkit::TabSmoke(DebugUI& ui) {
    if (thumbsDirty_ && ui.pass == DebugUI::Pass::Input) RefreshThumbs();

    ui.Section(L"스크린샷", L"./Screenshots");
    {
        const DebugRect row = ui.Row(34.0f);
        if (ui.Button(DebugUI::Cell(row, 2, 0), L"현재 장면 (F12)", false, !captureActive_))
            RequestCurrentScreenshot();
        if (ui.Button(DebugUI::Cell(row, 2, 1), L"모든 장면 캡처", false, !captureActive_))
            RequestAllScreenshots();
        if (captureActive_)
            ui.TextRow((L"캡처 중 " + std::to_wstring(captureIndex_ + 1) + L" / " +
                        std::to_wstring(CaptureStates().size())).c_str(),
                       0.44f, 1.0f, 0.84f, 0.30f, 0.96f);
    }

    ui.Section(L"스모크 테스트", L"별도 창 · 데스크톱 2 · 세이브 사본");
    const bool running = smoke_ != nullptr;
    {
        const int n = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
        for (int i = 0; i < n; i += 2) {
            const DebugRect row = ui.Row(32.0f);
            for (int k = 0; k < 2 && i + k < n; ++k)
                if (ui.Button(DebugUI::Cell(row, 2, k), kPresets[i + k].label, false, !running,
                              nullptr, 0.46f)) {
                    smokeSteps_ = kPresets[i + k].steps;
                    StartSmoke(smokeSteps_);
                }
        }
    }
    {
        const DebugRect row = ui.Row(30.0f);
        DrawTextBox(ui, row.x, row.y, row.w, row.h, smokeSteps_,
                    L"단계 직접 입력 (예: MENU~2;SETTINGS) · ENTER 실행", TextFocus::SmokeSteps);
    }
    {
        const DebugRect row = ui.Row(32.0f);
        for (int i = 0; i < 3; ++i)
            if (ui.Button(DebugUI::Cell(row, 6, i), kLangs[i], smokeLang_ == i, !running, nullptr, 0.46f))
                smokeLang_ = i;
        static const DebugColor kRun{ 0.50f, 1.0f, 0.65f };
        static const DebugColor kStop{ 1.0f, 0.45f, 0.45f };
        if (!running) {
            if (ui.Button(DebugUI::Span(row, 6, 3, 3), L"실행", false, !smokeSteps_.empty(), &kRun))
                StartSmoke(smokeSteps_);
        } else if (ui.Button(DebugUI::Span(row, 6, 3, 3), L"중지", false, true, &kStop)) {
#ifdef _WIN32
            TerminateProcess(smoke_->process, 1);
#endif
        }
    }
    if (running) {
        const int sec = (int)(glfwGetTime() - smoke_->start);
        ui.TextRow((smokeStatus_ + L" · " + std::to_wstring(sec) + L"초").c_str(),
                   0.42f, 1.0f, 0.84f, 0.36f, 0.96f);
    } else if (!smokeStatus_.empty()) {
        ui.TextRow(smokeStatus_.c_str(), 0.42f, 0.60f, 0.92f, 0.70f, 0.94f);
    }

    // Gallery.
    ui.Section(thumbsPrefix_.empty() ? L"최근 스크린샷" : L"이번 실행 결과",
               L"누르면 크게 보기");
    {
        const DebugRect row = ui.Row(30.0f);
        if (ui.Button(DebugUI::Cell(row, 3, 0), L"새로고침", false, true, nullptr, 0.44f)) thumbsDirty_ = true;
        if (ui.Button(DebugUI::Cell(row, 3, 1), L"최근 보기", thumbsPrefix_.empty(), true, nullptr, 0.44f)) {
            thumbsPrefix_.clear();
            thumbsDirty_ = true;
        }
        if (ui.Button(DebugUI::Cell(row, 3, 2), L"폴더 열기", false, true, nullptr, 0.44f))
            OpenScreenshotFolder();
    }
    if (thumbs_.empty()) {
        ui.TextRow(L"스크린샷이 없습니다");
    } else {
        const float cellW = (ui.width - 8.0f) * 0.5f;
        const float imgH = cellW * 9.0f / 16.0f;
        for (size_t i = 0; i < thumbs_.size(); i += 2) {
            const DebugRect row = ui.Row(imgH + 22.0f, 8.0f);
            for (size_t k = 0; k < 2 && i + k < thumbs_.size(); ++k) {
                const Thumb& t = thumbs_[i + k];
                const DebugRect cell = DebugUI::Cell(row, 2, (int)k, 8.0f);
                const DebugRect img{ cell.x, cell.y, cell.w, imgH };
                if (ui.Clicked(img)) {
                    viewer_ = (int)(i + k);
                    if (viewerTex_) { glDeleteTextures(1, &viewerTex_); viewerTex_ = 0; }
                    viewerW_ = viewerH_ = 0;
                    ui.click = false;
                }
                if (!ui.Drawing() || !ui.Visible(row)) continue;
                ui.Fill(img, 0.11f, 0.115f, 0.128f, 1.0f);
                if (t.tex && t.w > 0 && t.h > 0) {
                    const float s = std::min(img.w / (float)t.w, img.h / (float)t.h);
                    const float w = t.w * s, h = t.h * s;
                    DrawIcon(t.tex, img.x + (img.w - w) * 0.5f, img.y + (img.h - h) * 0.5f, w, h,
                             1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    ui.TextCentered(img, t.tried ? L"읽기 실패" : L"불러오는 중", 0.40f,
                                    0.60f, 0.63f, 0.67f, 1.0f);
                }
                const bool hover = ui.Hover(img);
                if (hover) ui.Frame(img, 0.75f, 0.78f, 0.82f, 1.0f);
                // File name without the run prefix.
                std::wstring name = t.file;
                if (!thumbsPrefix_.empty() && name.rfind(thumbsPrefix_, 0) == 0)
                    name = name.substr(thumbsPrefix_.size());
                ui.Text(cell.x + 2.0f, cell.y + imgH + 4.0f,
                        DebugFitText(name.c_str(), 0.42f, cell.w - 4.0f).c_str(), 0.42f,
                        0.60f, 0.63f, 0.67f, 1.0f);
            }
        }
    }

    ui.Section(L"기타");
    {
        ui.TextRow(L"타이틀 로고 스타일");
        const DebugRect row = ui.Row(30.0f);
        static const wchar_t* const kStyle[4] = { L"별자리 선", L"곡선 (A)", L"뾰족 (B)", L"기본 폰트" };
        for (int s = 0; s < 4; ++s)
            if (ui.Button(DebugUI::Cell(row, 4, s), kStyle[s], g_TitleStyle == s, true, nullptr, 0.44f)) {
                g_TitleStyle = s;
                SetMessage(std::wstring(L"타이틀: ") + kStyle[s]);
            }
    }
}
