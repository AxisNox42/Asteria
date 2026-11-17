// F1 debug console > 증강 search matching.
#include "DebugAugSearch.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++g_fail; } } while (0)

int main() {
    const wchar_t* names[] = { L"관통 탄", L"Pierce Shot", L"貫通弾" };
    const wchar_t* descs[] = { L"탄이 적 2명을 뚫고 지나감", L"Bullets pass through 2 enemies", L"弾が敵2体を貫通" };
    auto m = [&](const wchar_t* q) { return debugsearch::Matches(names, 3, descs, 3, q); };

    CHECK(m(L""));                 // empty query keeps everything
    CHECK(m(L"   "));              // whitespace only = empty
    CHECK(m(L"관통"));
    CHECK(m(L"관통탄"));            // spaces ignored
    CHECK(m(L"PIERCE"));           // ASCII case ignored
    CHECK(m(L"pierce shot"));
    CHECK(m(L"貫通"));              // other language names searched too
    CHECK(m(L"뚫고"));              // description searched
    CHECK(m(L"through"));
    CHECK(!m(L"유도"));
    CHECK(!m(L"homing"));
    CHECK(!debugsearch::Matches(nullptr, 0, nullptr, 0, L"x"));
    CHECK(debugsearch::Matches(nullptr, 0, nullptr, 0, L""));

    if (g_fail) { std::printf("%d failure(s)\n", g_fail); return 1; }
    std::printf("debug_aug_search_test: all passed\n");
    return 0;
}
