#pragma once
// Pictures from the player's own PC for the boss panes' image viewer
// (PaneContent.h): the Windows wallpapers that ship with every PC, the
// player's current wallpaper and, only when the player turns it on in the
// settings (g_PanePhotos, off by default: private photos would show up on
// streams), their Pictures folder. A worker thread finds the files and
// decodes a small pool with WIC, scaled down, so the battle never waits on a
// disk read. Header-only so tests/pane_content_preview.cpp can load
// synchronously.
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace pane_images {

// One decoded picture: 32-bit BGRA, top-down, at most kMaxW x kMaxH.
struct Picture {
    std::wstring name;     // file name shown in the viewer's title and status line
    int srcW = 0, srcH = 0;
    int w = 0, h = 0;
    std::vector<unsigned char> bgra;
};

constexpr int kMaxW = 800, kMaxH = 600;   // ~1.9 MB each
constexpr int kPoolSize = 20;          // pictures kept decoded
constexpr int kMaxCandidates = 600;    // files looked at per folder sweep
constexpr int kMaxDepth = 3;           // folder depth searched under each root
constexpr uintmax_t kMaxFileBytes = 40u * 1024u * 1024u;

namespace detail {
inline std::mutex g_lock;
inline std::vector<std::shared_ptr<const Picture>> g_pool;   // guarded by g_lock
inline std::thread g_worker;
inline std::atomic<bool> g_stop{ false };
inline int g_mode = -1;                // -1 not started, 0 wallpapers, 1 + photos

inline bool IsImageFile(const std::filesystem::path& p) {
    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp";
}

inline void Sweep(const std::filesystem::path& root, std::vector<std::filesystem::path>& out) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return;
    int seen = 0;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (g_stop || ++seen > kMaxCandidates * 4) break;
        if (it.depth() >= kMaxDepth) it.disable_recursion_pending();
        if (!it->is_regular_file(ec) || !IsImageFile(it->path())) continue;
        const uintmax_t bytes = it->file_size(ec);
        if (ec || bytes == 0 || bytes > kMaxFileBytes) { ec.clear(); continue; }
        out.push_back(it->path());
        if ((int)out.size() >= kMaxCandidates) break;
    }
}

inline std::filesystem::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::filesystem::path p;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw)) && raw) p = raw;
    CoTaskMemFree(raw);
    return p;
}

// The current wallpaper: the file the user picked, or the copy Windows keeps.
inline std::filesystem::path CurrentWallpaper() {
    wchar_t buf[MAX_PATH] = {};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0) && buf[0]) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(buf, ec)) return buf;
    }
    const std::filesystem::path roaming = KnownFolder(FOLDERID_RoamingAppData);
    const std::filesystem::path copy = roaming / L"Microsoft" / L"Windows" / L"Themes" / L"TranscodedWallpaper";
    std::error_code ec;
    return std::filesystem::is_regular_file(copy, ec) ? copy : std::filesystem::path{};
}

// Decodes one file scaled into kMaxW x kMaxH, muted a little toward grey so a
// bright photo does not fight the battle drawn over it.
inline std::shared_ptr<Picture> Decode(IWICImagingFactory* f, const std::filesystem::path& path,
                                       const std::wstring& name) {
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* conv = nullptr;
    std::shared_ptr<Picture> pic;
    UINT sw = 0, sh = 0;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&sw, &sh)) &&
        sw >= 640 && sh >= 400) {   // no thumbnails or icons
        const double k = std::min(1.0, std::min((double)kMaxW / sw, (double)kMaxH / sh));
        const UINT w = std::max(1u, (UINT)(sw * k)), h = std::max(1u, (UINT)(sh * k));
        if (SUCCEEDED(f->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(frame, w, h, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(f->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                                       nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            auto p = std::make_shared<Picture>();
            p->name = name;
            p->srcW = (int)sw; p->srcH = (int)sh;
            p->w = (int)w; p->h = (int)h;
            p->bgra.resize((size_t)w * h * 4);
            if (SUCCEEDED(conv->CopyPixels(nullptr, w * 4, (UINT)p->bgra.size(), p->bgra.data()))) {
                for (size_t i = 0; i < p->bgra.size(); i += 4) {
                    unsigned char* px = &p->bgra[i];
                    const int grey = (px[0] + px[1] + px[2]) / 3;
                    for (int c = 0; c < 3; ++c) {   // 25% toward its own grey, 15% toward mid grey
                        const int desat = px[c] + (grey - px[c]) / 4;
                        px[c] = (unsigned char)(desat + (110 - desat) * 15 / 100);
                    }
                    px[3] = 255;
                }
                pic = std::move(p);
            }
        }
    }
    if (conv) conv->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    return pic;
}

// Finds candidates and decodes up to kPoolSize into the pool.
inline void Load(bool photos) {
    const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)))) {
        std::vector<std::filesystem::path> files;
        const std::filesystem::path windows = KnownFolder(FOLDERID_Windows);
        Sweep(windows / L"Web" / L"Wallpaper", files);
        Sweep(windows / L"Web" / L"4K" / L"Wallpaper", files);
        if (photos) Sweep(KnownFolder(FOLDERID_Pictures), files);
        std::mt19937 rng((unsigned)GetTickCount64());
        std::shuffle(files.begin(), files.end(), rng);
        // The player's own wallpaper goes first: the most "this is my PC" picture.
        const std::filesystem::path current = CurrentWallpaper();
        if (!current.empty()) files.insert(files.begin(), current);
        int made = 0;
        for (const auto& path : files) {
            if (g_stop || made >= kPoolSize) break;
            std::wstring name = path.filename().wstring();
            if (name == L"TranscodedWallpaper") name = L"wallpaper.jpg";
            auto pic = Decode(factory, path, name);
            if (!pic) continue;
            std::lock_guard<std::mutex> hold(g_lock);
            g_pool.push_back(std::move(pic));
            ++made;
        }
        factory->Release();
    }
    if (com) CoUninitialize();
}
}   // namespace detail

// Stops and joins the worker (game shutdown, or before a reload).
inline void Stop() {
    detail::g_stop = true;
    if (detail::g_worker.joinable()) detail::g_worker.join();
    detail::g_stop = false;
}

// Starts (or restarts, when the photos setting changed) the background load.
inline void Ensure(bool photos) {
    const int mode = photos ? 1 : 0;
    if (detail::g_mode == mode) return;
    Stop();
    {
        std::lock_guard<std::mutex> hold(detail::g_lock);
        detail::g_pool.clear();
    }
    detail::g_mode = mode;
    detail::g_worker = std::thread([photos] { detail::Load(photos); });
}

// Loads on the calling thread (preview tool).
inline void LoadNow(bool photos) {
    Stop();
    {
        std::lock_guard<std::mutex> hold(detail::g_lock);
        detail::g_pool.clear();
    }
    detail::g_mode = photos ? 1 : 0;
    detail::Load(photos);
}

// A picture for a pane, or nullptr while nothing has loaded yet. Panes take
// the pool in turn (from a random start) so open panes rarely repeat one.
inline std::shared_ptr<const Picture> Pick(unsigned seed) {
    static unsigned next = seed;
    std::lock_guard<std::mutex> hold(detail::g_lock);
    if (detail::g_pool.empty()) return nullptr;
    return detail::g_pool[next++ % detail::g_pool.size()];
}

}   // namespace pane_images
#endif
