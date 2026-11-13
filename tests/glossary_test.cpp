// Run scripts/test_glossary.ps1. Every glossary term has text in each language
// and links somewhere; with "dump" it prints each card's linked words for review.
#include "Augment.h"
#include "Glossary.h"

#include <cstdio>
#include <cstring>
#include <string>

static std::string Utf8(const std::wstring& w) {
    std::string out;
    for (wchar_t c : w) {
        unsigned u = (unsigned)c;
        if (u < 0x80) out += (char)u;
        else if (u < 0x800) { out += (char)(0xC0 | (u >> 6)); out += (char)(0x80 | (u & 0x3F)); }
        else { out += (char)(0xE0 | (u >> 12)); out += (char)(0x80 | ((u >> 6) & 0x3F)); out += (char)(0x80 | (u & 0x3F)); }
    }
    return out;
}

int main(int argc, char** argv) {
    const bool dump = argc > 1 && std::strcmp(argv[1], "dump") == 0;
    int bad = 0;
    int hits[3][glossary::kTermCount] = {};
    for (int t = 0; t < glossary::kTermCount; ++t) {
        const glossary::Entry& e = glossary::kEntries[t];
        for (int l = 0; l < 3; ++l)
            if (!e.name[l] || !e.name[l][0] || !e.def[l] || !e.def[l][0] || !e.phrases[l] || !e.phrases[l][0]) {
                std::printf("term %d has empty text in language %d\n", t, l);
                ++bad;
            }
        for (int r = 0; r < e.relCount; ++r)
            if ((int)e.rel[r] == t || e.rel[r] == glossary::Term::Count) { std::printf("term %d: bad related term\n", t); ++bad; }
    }
    for (int i = 0; i < AUG_TOTAL; ++i) {
        const AugDef& d = ALL_AUGS[i];
        for (int l = 0; l < 3; ++l) {
            uint64_t used = 0;
            const std::wstring s = d.locDesc[l];
            const auto links = glossary::Link(s, l, used);
            for (const auto& sp : links) ++hits[l][(int)sp.term];
            if (dump && !links.empty()) {
                std::string line = std::string(d.name) + " [" + std::to_string(l) + "] ";
                for (const auto& sp : links)
                    line += "{" + Utf8(s.substr((size_t)sp.start, (size_t)sp.len)) + "=" + Utf8(glossary::kEntries[(int)sp.term].name[0]) + "} ";
                std::printf("%s\n", line.c_str());
            }
        }
    }
    for (int l = 0; l < 3; ++l)
        for (int t = 0; t < glossary::kTermCount; ++t)
            if (hits[l][t] == 0) std::printf("note: term '%s' never links in language %d\n", Utf8(glossary::kEntries[t].name[0]).c_str(), l);
    std::printf(bad ? "FAILED (%d)\n" : "Glossary OK\n", bad);
    return bad ? 1 : 0;
}
