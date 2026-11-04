// MergeShip2 -- the canonical item table on SoH's side (MergeShipMsItems.h, include/mergeship/MsItems.def).

#include "MergeShipMsItems.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace {

// clang-format off
constexpr MsItemInfo kItems[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) \
    { MSI_##key, rg, #ri, kind, persist, (oot) != 0, (mm) != 0, maxLevel, name, #key, false },
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) \
    { MSI_##key, rg, "RI_OOT_" #key, kind, persist, (oot) != 0, (mm) != 0, maxLevel, name, #key, true },
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
};

struct Alias {
    RandomizerGet rg;
    MsItemKey key;
};
constexpr Alias kAliases[] = {
#define MS_ALIAS_RG(rg, key) { rg, MSI_##key },
#include <mergeship/MsItems.def>
#undef MS_ALIAS_RG
};

constexpr RandomizerGet kSkipped[] = {
#define MS_SKIP_RG(rg, why) rg,
#include <mergeship/MsItems.def>
#undef MS_SKIP_RG
};
// clang-format on

constexpr size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);
static_assert(kItemCount == MSI_MAX, "MsItems.def: one row per MsItemKey");

// Every RandomizerGet but RG_MAX appears exactly once as an item, an alias or a skipped value.
constexpr std::array<uint8_t, RG_MAX> CountRGs() {
    std::array<uint8_t, RG_MAX> count{};
    for (const MsItemInfo& item : kItems) {
        count[item.rg]++;
    }
    for (const Alias& alias : kAliases) {
        count[alias.rg]++;
    }
    for (RandomizerGet rg : kSkipped) {
        count[rg]++;
    }
    return count;
}
constexpr bool EveryRGOnce() {
    const std::array<uint8_t, RG_MAX> count = CountRGs();
    for (size_t i = 0; i < RG_MAX; i++) {
        if (count[i] != 1) {
            return false;
        }
    }
    return true;
}
static_assert(EveryRGOnce(), "MsItems.def must cover every RandomizerGet exactly once (run tools/port_mm_rando.py "
                             "--check for the list)");

// Rows are in MsItemKey order, and each key is its RandomizerGet without RG_ (the generator relies on it).
constexpr bool RowsInKeyOrder() {
    for (size_t i = 0; i < kItemCount; i++) {
        if (kItems[i].key != (MsItemKey)i) {
            return false;
        }
    }
    return true;
}
static_assert(RowsInKeyOrder(), "MsItems.def rows out of MsItemKey order");

// Proxies really are proxies: a native 2Ship name never starts with RI_OOT_.
constexpr bool ProxyNamesConsistent() {
    for (const MsItemInfo& item : kItems) {
        const bool proxyName = std::string_view(item.ri).substr(0, 7) == "RI_OOT_";
        if (proxyName != item.proxy) {
            return false;
        }
    }
    return true;
}
static_assert(ProxyNamesConsistent(), "MsItems.def: MS_ITEM rows must name a native 2Ship RandoItemId");

constexpr std::array<uint16_t, RG_MAX> BuildKeyOfRG() {
    std::array<uint16_t, RG_MAX> keys{};
    for (size_t i = 0; i < RG_MAX; i++) {
        keys[i] = MSI_MAX;
    }
    for (const MsItemInfo& item : kItems) {
        keys[item.rg] = (uint16_t)item.key;
    }
    for (const Alias& alias : kAliases) {
        keys[alias.rg] = (uint16_t)alias.key;
    }
    return keys;
}
constexpr std::array<uint16_t, RG_MAX> kKeyOfRG = BuildKeyOfRG();

} // namespace

const MsItemInfo* MsItems_Get(MsItemKey key) {
    return (key >= 0 && key < MSI_MAX) ? &kItems[key] : nullptr;
}

MsItemKey MsItems_KeyOfRG(RandomizerGet rg) {
    return (rg >= 0 && rg < RG_MAX) ? (MsItemKey)kKeyOfRG[rg] : MSI_MAX;
}

const char* MsItems_RIName(RandomizerGet rg) {
    const MsItemInfo* item = MsItems_Get(MsItems_KeyOfRG(rg));
    return (item != nullptr) ? item->ri : nullptr;
}

RandomizerGet MsItems_FirstUncoveredRG() {
    static const std::array<uint8_t, RG_MAX> count = CountRGs();
    for (size_t i = 0; i < RG_MAX; i++) {
        if (count[i] != 1) {
            return (RandomizerGet)i;
        }
    }
    return RG_MAX;
}
