// Paints boss-pane contents (WiNILL/System/PaneContent.h) to BMP files so the
// memo / code / image kinds can be looked at without running the game.
// Usage: pane_content_preview.exe <out_dir> [photos]
// Image panes use pictures loaded from this PC (PaneImages.h); pass
// "photos" to include the Pictures folder.
// Writes <kind>_<seed>_<light|dark>[_worn].bmp at 420x300.
#include "PaneContent.h"

#include <cstdio>
#include <string>

static void SaveBmp(HDC dc, HBITMAP bmp, int w, int h, const std::wstring& path) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    const int stride = ((w * 3 + 3) & ~3);
    std::string pixels((size_t)stride * h, '\0');
    GetDIBits(dc, bmp, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh{};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(bi.bmiHeader);
    fh.bfSize = fh.bfOffBits + (DWORD)pixels.size();
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&bi.bmiHeader, sizeof(bi.bmiHeader), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
}

int wmain(int argc, wchar_t** argv) {
    const std::wstring out = argc > 1 ? argv[1] : L".";
    const bool photos = argc > 2 && std::wstring(argv[2]) == L"photos";
    pane_images::LoadNow(photos);
    for (unsigned i = 0; i < 16; ++i)
        if (auto pic = pane_images::Pick(i))
            std::wprintf(L"loaded: %ls (%dx%d)\n", pic->name.c_str(), pic->srcW, pic->srcH);
    const int w = 420, h = 300;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    SelectObject(dc, bmp);
    int made = 0;
    // Seeds are searched so each kind (and each image style) shows up.
    for (unsigned seed = 1; seed < 4000 && made < 14; seed += 97) {
        pane_content::Content c;
        c.seed = seed * 2654435761u;
        pane_content::Roll(c, L"OCTANS");
        const wchar_t* kind = c.kind == pane_content::Kind::Memo ? L"memo"
                            : c.kind == pane_content::Kind::Code ? L"code" : L"image";
        for (int variant = 0; variant < 3; ++variant) {
            c.dark = variant != 0;
            c.unstable = variant == 2 ? 0.8f : 0.0f;
            const RECT r{ 0, 0, w, h };
            pane_content::Paint(dc, r, &c, c.dark);
            wchar_t name[160];
            swprintf_s(name, L"%ls\\%ls_%02d_%ls%ls.bmp", out.c_str(), kind, made,
                       c.dark ? L"dark" : L"light", variant == 2 ? L"_worn" : L"");
            SaveBmp(dc, bmp, w, h, name);
        }
        std::wprintf(L"%02d %ls  %ls\n", made, kind, c.title.c_str());
        ++made;
    }
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return 0;
}
