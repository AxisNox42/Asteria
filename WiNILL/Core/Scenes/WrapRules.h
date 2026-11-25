#pragma once
// Line-break rules for text with no spaces (Japanese, Chinese): a break between
// any two characters is allowed, except where a line would start with closing
// punctuation or a unit ("、", ")", "%", "○"), end on an opening bracket, or
// split a number ("6." / "5○", "+1" / "5%"). Shared by the augment cards
// (SceneAugment.cpp WrapSegment) and the run build panel (RunBuildPanel.cpp WrapLines).
#include <cwchar>

namespace wrap_rules {

inline bool NoLineStart(wchar_t c) {
    return c != 0 &&
           std::wcschr(L"、。，．,.:;!?)]）］」』〕〉》%％○…ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ", c) != nullptr;
}
inline bool NoLineEnd(wchar_t c) { return c != 0 && std::wcschr(L"([（［「『〔〈《", c) != nullptr; }
inline bool NumJoin(wchar_t a, wchar_t b) {
    const bool da = (a >= L'0' && a <= L'9') || a == L'.' || a == L'+' || a == L'-';
    const bool db = (b >= L'0' && b <= L'9') || b == L'.';
    return da && db;
}
// True when a line may not break between `before` and `after`.
inline bool Glued(wchar_t before, wchar_t after) {
    return NoLineStart(after) || NumJoin(before, after) || NoLineEnd(before);
}

} // namespace wrap_rules
