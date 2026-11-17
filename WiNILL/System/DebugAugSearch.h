#pragma once
// F1 debug console > 증강 tab search: which cards a query keeps.
// Pure text helpers (no GL / game state) so tests can include this alone.
#include <string>
#include <cwctype>

namespace debugsearch {

// Drops whitespace and lowercases ASCII, so "Pierce Shot", "pierceshot" and
// "관통 탄" / "관통탄" all compare the same.
inline std::wstring Fold(const wchar_t* text) {
    std::wstring out;
    if (!text) return out;
    for (const wchar_t* p = text; *p; ++p) {
        const wchar_t c = *p;
        if (std::iswspace(c) || c == 0x3000) continue;
        out.push_back(c < 128 ? (wchar_t)std::towlower(c) : c);
    }
    return out;
}

inline bool Contains(const wchar_t* text, const std::wstring& foldedQuery) {
    if (foldedQuery.empty()) return true;
    return Fold(text).find(foldedQuery) != std::wstring::npos;
}

// True when the query is empty, or it appears in any of the names or descriptions.
inline bool Matches(const wchar_t* const* names, int nameCount,
                    const wchar_t* const* descs, int descCount,
                    const std::wstring& query) {
    const std::wstring q = Fold(query.c_str());
    if (q.empty()) return true;
    for (int i = 0; i < nameCount; ++i)
        if (names && Contains(names[i], q)) return true;
    for (int i = 0; i < descCount; ++i)
        if (descs && Contains(descs[i], q)) return true;
    return false;
}

} // namespace debugsearch
