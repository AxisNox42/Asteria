// Run scripts/test_augment_catalog.ps1. ALL_AUGS must list the augments in AugType order:
// the code indexes the table by the enum, so a row in the wrong place gives a card
// another card's effect. Also checks the texts every language needs.
#include "Augment.h"
#include "AugmentSynergy.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

int main() {
    int bad = 0;
    std::set<std::string> names;
    for (int i = 0; i < AUG_TOTAL; ++i) {
        const AugDef& d = ALL_AUGS[i];
        if ((int)d.type != i) {
            std::printf("row %d is %s, the enum says %d\n", i, d.name, (int)d.type);
            ++bad;
        }
        if (!names.insert(d.name).second) {
            std::printf("duplicate code name %s\n", d.name);
            ++bad;
        }
        for (int l = 0; l < LANG_COUNT; ++l) {
            if (!d.locName[l] || !d.locName[l][0] || !d.locDesc[l] || !d.locDesc[l][0]) {
                std::printf("%s has an empty name or description in language %d\n", d.name, l);
                ++bad;
            }
        }
        // The code name prefix says what kind of card it is.
        const bool active = std::strncmp(d.name, "A_", 2) == 0;
        const bool stat = std::strncmp(d.name, "S_", 2) == 0;
        if (active != AugIsActive(d)) {
            std::printf("%s: active flag does not match its name\n", d.name);
            ++bad;
        }
        if (stat != AugIsStatCard(d)) {
            std::printf("%s: stat-card flag does not match its name\n", d.name);
            ++bad;
        }
    }
    // The generated registration (SynergyCodes.inc) must follow the table: same cards, same order.
    const auto& table = synergy::Table();
    const int synCards = (int)table.size() - 1;               // table index 0 is the bare gun
    if (AUG_TOTAL - ((int)AugType::S_CRIT + 1) != synCards) {
        std::printf("the game registers %d synergy cards, the table has %d (run scripts/gen_synergy_catalog.py)\n",
                    AUG_TOTAL - ((int)AugType::S_CRIT + 1), synCards);
        ++bad;
    }
    for (int i = 1; i <= synCards; ++i) {
        const int idx = (int)AugType::S_CRIT + i;
        if (idx >= AUG_TOTAL) break;
        const std::string want = std::string("SYN_") + table[i].code;
        if (want != ALL_AUGS[idx].name) {
            std::printf("synergy card %d: the table says %s, the catalog row is %s\n", i, want.c_str(), ALL_AUGS[idx].name);
            ++bad;
        }
    }
    if (bad) { std::printf("%d problems\n", bad); return 1; }
    std::printf("Augment catalog: %d rows in enum order, unique codes, texts in every language OK\n", AUG_TOTAL);
    return 0;
}
