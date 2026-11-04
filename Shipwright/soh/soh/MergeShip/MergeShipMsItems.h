#pragma once

// MergeShip2 -- SoH's view of the unified randomizer's canonical item table (include/mergeship/MsItems.def):
// every RandomizerGet's canonical item, and the 2Ship RandoItemId that names it in a MERGESHIP_SEED's "mm" section.
// MergeShipMsItems.cpp checks at compile time that the table covers every RandomizerGet exactly once.

#include <mergeship/MsItems.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"

struct MsItemInfo {
    MsItemKey key;
    RandomizerGet rg;      // SoH's RandomizerGet of the item (its main one; MS_ALIAS_RG rows add others)
    const char* ri;        // 2Ship's RandoItemId name: native, or the proxy "RI_OOT_<key>"
    MsKind kind;
    MsPersist persist;
    bool usableInOoT;
    bool usableInMM;
    int maxLevel;          // copies that count; 0 = no limit
    const char* name;      // English name
    const char* keyName;   // "<key>"
    bool proxy;            // ri is a proxy (2Ship has no RandoItemId for it)
};

// The row of `key`; nullptr for MSI_MAX.
const MsItemInfo* MsItems_Get(MsItemKey key);
// The canonical item of a RandomizerGet (aliases included); MSI_MAX for values that are not items (MS_SKIP_RG).
MsItemKey MsItems_KeyOfRG(RandomizerGet rg);
// 2Ship's RandoItemId name for a RandomizerGet; nullptr when it is not an item.
const char* MsItems_RIName(RandomizerGet rg);
// The first RandomizerGet the table does not cover exactly once (RG_MAX when it covers all): a runtime echo of the
// static_assert, for logs.
RandomizerGet MsItems_FirstUncoveredRG();
