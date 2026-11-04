// MergeShip2 -- the unified randomizer's Termina side of SoH's generator (MergeShipTerminaRando.h).
//
// The check pool follows 2Ship's GeneratePools (2ship2harkinian/mm/2s2h/Rando/Logic/GeneratePools.cpp) from the
// RSK_MM_* settings (2Ship's own options); the item pool adds each shuffled check's vanilla item as the same SoH item
// (MsItems.def) plus 2Ship's extra pool items, under the unified caps settled in UNIFIED_RANDO_DESIGN.md:
//   - hearts: one budget of 20, Hyrule's -- Termina's heart pieces and containers become junk;
//   - bottles: at most 6 bottle items in the whole pool (OoT shows the first 4);
//   - any item with a maxLevel: copies past it (counting the Hyrule pool) become junk.
// M4: every check 2Ship can shuffle is in the pool by default (the RSK_MM_* shuffles default on in the unified mode);
// a check 2Ship keeps vanilla whose vanilla item the logic needs (a song, a key, remains, an owl...) is in the pool too,
// with that item placed, so the logic finds it where the game has it. 2Ship's own-dungeon and song-location placements
// run in Fill (MergeShip_Termina_PlaceRestrictedItems); clock shuffle's starting clock is chosen here.
// Starting with Termina's dungeon items is 2Ship's own options ("Termina: Dungeon Items" in the Mergeship menu: the
// placements' "Start With" and Starting Maps And Compasses): the items leave the pool, the logic owns them from the
// start (MergeShip_Termina_LogicStartingItems), and 2Ship gives them when the Termina file is made (its
// GetComputedStartingItems, from the seed's "mm" options).
// Not yet: excluded Termina checks and starting items chosen one by one (settings UI, M5).

#include "MergeShipTerminaRando.h"
#include "MergeShipMsItems.h"
#include "MergeShipGoal.h"
#include "MergeShipTerminaLogic.h"
#include "MergeShipEntrances.h"
#include "MergeShipUnifiedHints.h"

#include <mergeship/MsDelivery.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/rng.h"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"
#include "soh/Enhancements/randomizer/3drando/pool_functions.hpp"
#include "soh/Enhancements/randomizer/3drando/fill.hpp"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"

namespace {

// State of the seed being generated.
struct TerminaPool {
    std::vector<bool> inPool;                    // by index into gMsTerminaChecks: shuffled
    std::vector<bool> vanillaShuffled;           // kept at its vanilla item, but "shuffled" for 2Ship (listed)
    std::vector<bool> vanillaPlaced;             // not shuffled, in SoH's pool holding its vanilla item (logic needs it)
    std::map<RandomizerCheck, uint16_t> prices;  // shops and Tingle
    std::vector<RandomizerGet> logicStarting;    // 2Ship's computed starting items the pool left out, the clock
    RandomizerGet startingClock = RG_NONE;       // clock shuffle: the half-day the player starts with
    size_t locations = 0;
    size_t vanillaPlacedCount = 0;
    size_t itemsAdded = 0;
    size_t junked = 0;
    size_t trimmed = 0;
    size_t restrictedItems = 0;                  // placed by PlaceRestrictedItems
    size_t songJunk = 0;                         // spare song checks junked (song-location mode)
    size_t startingFromTermina = 0;              // started-with items taken out of Termina's pool
    size_t startingFromHyrule = 0;               // ... out of Hyrule's pool (its shuffles of MM items)
} sPool;

const MsTerminaOption* FindOption(const char* mmOption) {
    for (size_t i = 0; i < gMsTerminaOptionCount; i++) {
        if (strcmp(gMsTerminaOptions[i].mmOption, mmOption) == 0) {
            return &gMsTerminaOptions[i];
        }
    }
    return nullptr;
}

// 2Ship's option values used below (Types.h RandoOption*).
constexpr int kSongShuffleVanilla = 2;        // RO_SONG_SHUFFLE_VANILLA
constexpr int kRemainsShuffleVanilla = 0;     // RO_REMAINS_SHUFFLE_VANILLA
constexpr int kDungeonItemOwnDungeon = 1;     // RO_DUNGEON_ITEM_OWN_DUNGEON
constexpr int kDungeonItemStartWith = 2;      // RO_DUNGEON_ITEM_START_WITH
constexpr int kDungeonItemVanilla = 3;        // RO_DUNGEON_ITEM_VANILLA
constexpr int kClockShuffleRandom = 0;        // RO_CLOCK_SHUFFLE_RANDOM
constexpr int kStrayFairiesPerTemple = 15;    // STRAY_FAIRY_SCATTERED_TOTAL

// A 2Ship option's value as the settings (or the loaded seed) hold it.
int RawMMOption(const MsTerminaOption& option) {
    auto ctx = Rando::Context::GetInstance();
    if (option.rsk == RSK_MAX || ctx == nullptr) {
        return option.defaultValue;
    }
    return ctx->GetOption(option.rsk).Get() + option.minValue;
}

int RawMMOption(const char* mmOption) {
    const MsTerminaOption* option = FindOption(mmOption);
    return option != nullptr ? RawMMOption(*option) : 0;
}

// The value 2Ship plays with: its OnFileCreate adjusts two options before it saves them (and so before its spoiler
// has them): starting with every Stray Fairy means the Great Fairies want all 15, and vanilla Stray Fairies keep
// every temple's full set. The unified seed's "mm" options, pool and logic use the same values.
int EffectiveMMOption(const MsTerminaOption& option) {
    // Termina's gossip stones give hints when Hyrule's do (BUG-0026: the seed's "Gossip Stone Hints"; what they need is
    // the shared save's gossipHints, MergeShipGoal.cpp).
    if (strcmp(option.mmOption, "RO_HINTS_GOSSIP_STONES") == 0) {
        return Rando::Context::GetInstance()->GetOption(RSK_GOSSIP_STONE_HINTS).IsNot(RO_GOSSIP_STONES_NONE) ? 1 : 0;
    }
    if (strcmp(option.mmOption, "RO_STRAY_FAIRIES_REQUIRED") == 0 &&
        RawMMOption("RO_PLACEMENT_STRAY_FAIRIES") == kDungeonItemStartWith) {
        return kStrayFairiesPerTemple;
    }
    if (strcmp(option.mmOption, "RO_STRAY_FAIRIES_MAX") == 0 &&
        RawMMOption("RO_PLACEMENT_STRAY_FAIRIES") == kDungeonItemVanilla) {
        return kStrayFairiesPerTemple;
    }
    return RawMMOption(option);
}

// A 2Ship option's value (RandoOptionId value, as 2Ship stores it) for the seed being generated.
int MMOption(const char* mmOption) {
    const MsTerminaOption* option = FindOption(mmOption);
    if (option == nullptr) {
        SPDLOG_ERROR("MergeShip Termina: unknown 2Ship option {}", mmOption);
        return 0;
    }
    return EffectiveMMOption(*option);
}

bool MMOn(const char* mmOption) {
    return MMOption(mmOption) != 0;
}

bool StartsWith(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
bool EndsWith(const char* s, const char* suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

// 2Ship's DungeonItemPlacementOption: which RO_PLACEMENT_* governs a dungeon item (nullptr for others).
const char* PlacementOption(const char* ri) {
    if (strcmp(ri, "RI_CLOCK_TOWN_STRAY_FAIRY") == 0) {
        return "RO_PLACEMENT_STRAY_FAIRIES";
    }
    static const char* const kDungeons[] = { "RI_WOODFALL_", "RI_SNOWHEAD_", "RI_GREAT_BAY_", "RI_STONE_TOWER_" };
    for (const char* d : kDungeons) {
        if (!StartsWith(ri, d)) {
            continue;
        }
        if (EndsWith(ri, "_SMALL_KEY")) {
            return "RO_PLACEMENT_SMALL_KEYS";
        }
        if (EndsWith(ri, "_BOSS_KEY")) {
            return "RO_PLACEMENT_BOSS_KEYS";
        }
        if (EndsWith(ri, "_STRAY_FAIRY")) {
            return "RO_PLACEMENT_STRAY_FAIRIES";
        }
    }
    return nullptr;
}

// 2Ship's StaysAtVanillaCheck.
bool StaysAtVanillaCheck(const char* ri) {
    const char* option = PlacementOption(ri);
    if (option == nullptr) {
        return false;
    }
    const int placement = MMOption(option);
    return placement == kDungeonItemVanilla ||
           (placement == kDungeonItemOwnDungeon && strcmp(ri, "RI_CLOCK_TOWN_STRAY_FAIRY") == 0);
}

// The generic "shuffle this kind of check" options, by 2Ship check type.
const char* TypeOption(MsMMCheckType type) {
    switch (type) {
        case MS_MMCT_OWL:
            return "RO_SHUFFLE_OWL_STATUES";
        case MS_MMCT_POT:
            return "RO_SHUFFLE_POT_DROPS";
        case MS_MMCT_CRATE:
            return "RO_SHUFFLE_CRATE_DROPS";
        case MS_MMCT_BARREL:
            return "RO_SHUFFLE_BARREL_DROPS";
        case MS_MMCT_GRASS:
            return "RO_SHUFFLE_GRASS_DROPS";
        case MS_MMCT_BEEHIVE:
            return "RO_SHUFFLE_HIVE_DROPS";
        case MS_MMCT_TREE:
            return "RO_SHUFFLE_TREE_DROPS";
        case MS_MMCT_BUTTERFLY:
            return "RO_SHUFFLE_BUTTERFLIES";
        case MS_MMCT_FREESTANDING:
            return "RO_SHUFFLE_FREESTANDING_ITEMS";
        case MS_MMCT_SNOWBALL:
            return "RO_SHUFFLE_SNOWBALL_DROPS";
        case MS_MMCT_FROG:
            return "RO_SHUFFLE_FROGS";
        case MS_MMCT_COW:
            return "RO_SHUFFLE_COWS";
        case MS_MMCT_ENEMY_DROP:
            return "RO_SHUFFLE_ENEMY_DROPS";
        case MS_MMCT_WONDER_ITEM:
            return "RO_SHUFFLE_WONDER_ITEMS";
        case MS_MMCT_TINGLE_SHOP:
            return "RO_SHUFFLE_TINGLE_SHOPS";
        default:
            return nullptr;
    }
}

RandomizerGet RGOfKey(const char* keyName) {
    for (int key = 0; key < MSI_MAX; key++) {
        const MsItemInfo* item = MsItems_Get((MsItemKey)key);
        if (strcmp(item->keyName, keyName) == 0) {
            return item->rg;
        }
    }
    SPDLOG_ERROR("MergeShip Termina: no item {} in MsItems.def", keyName);
    return RG_NONE;
}

// Every item whose key starts with `prefix` (e.g. the 47 enemy souls), in table order.
std::vector<RandomizerGet> RGsWithPrefix(const char* prefix) {
    std::vector<RandomizerGet> out;
    for (int key = 0; key < MSI_MAX; key++) {
        const MsItemInfo* item = MsItems_Get((MsItemKey)key);
        if (StartsWith(item->keyName, prefix)) {
            out.push_back(item->rg);
        }
    }
    return out;
}

bool RemoveOne(std::vector<RandomizerGet>& items, RandomizerGet rg) {
    auto it = std::find(items.begin(), items.end(), rg);
    if (it != items.end()) {
        items.erase(it);
        return true;
    }
    return false;
}

// Descriptions for 2Ship's dungeon item options in the "Mergeship" menu ("Termina: Dungeon Items"). Their names stay the
// generated ones ("Termina: Placement Small Keys" ...): the name is also the setting's key in a seed's spoiler, so
// seeds made before keep loading. The option values are 2Ship's own (RO_DUNGEON_ITEM_*: Anywhere, Own Dungeon, Start
// With, Vanilla).
const std::string* DungeonItemDescription(RandomizerSettingKey rsk) {
    static const std::map<RandomizerSettingKey, std::string> kTexts = {
        { RSK_MM_PLACEMENT_SMALL_KEYS,
          "Where Termina's temple small keys are (2 Ship 2 Harkinian's Small Keys placement).\n"
          "Anywhere: in either world.\n"
          "Own Dungeon: inside their own temple.\n"
          "Start With: every temple's keys (Woodfall 1, Snowhead 3, Great Bay 1, Stone Tower 4) are yours from the "
          "start and leave the item pool. The Song of Time gives them back, as it does every key 2Ship's randomizer "
          "has found.\n"
          "Vanilla: where Majora's Mask has them." },
        { RSK_MM_PLACEMENT_BOSS_KEYS,
          "Where Termina's four boss keys are (2 Ship 2 Harkinian's Boss Keys placement).\n"
          "Anywhere: in either world.\n"
          "Own Dungeon: inside their own temple.\n"
          "Start With: all four are yours from the start and leave the item pool.\n"
          "Vanilla: where Majora's Mask has them." },
        { RSK_MM_PLACEMENT_STRAY_FAIRIES,
          "Where Termina's stray fairies are (2 Ship 2 Harkinian's Stray Fairies placement).\n"
          "Anywhere: in either world, Clock Town's one too.\n"
          "Own Dungeon: a temple's fairies inside it (Clock Town's where Majora's Mask has it).\n"
          "Start With: all 15 of every temple and Clock Town's one are yours from the start and leave the item pool "
          "(Hyrule's copies too); each Great Fairy wants all 15.\n"
          "Vanilla: where Majora's Mask has them." },
        { RSK_MM_STARTING_MAPS_AND_COMPASSES,
          "Start with every temple's map and compass and Tingle's six maps of Termina (2 Ship 2 Harkinian's Maps and "
          "Compasses starting option); they leave the item pool." },
        { RSK_MM_SHUFFLE_SKELETON_KEY,
          "Adds 2 Ship 2 Harkinian's Skeleton Key to the item pool: every Termina temple's small keys at once. Not "
          "added when Termina's small keys are Start With." },
        { RSK_MM_STRAY_FAIRIES_REQUIRED,
          "Stray fairies a temple's Great Fairy wants for her check (2 Ship 2 Harkinian's Required Stray Fairies). "
          "All 15 when the stray fairies are Start With." },
        { RSK_MM_STRAY_FAIRIES_MAX,
          "Stray fairies of each temple in the item pool (2 Ship 2 Harkinian's Stray Fairies in Pool); the rest "
          "become other items. All 15 when the stray fairies are Vanilla." },
    };
    auto it = kTexts.find(rsk);
    return it != kTexts.end() ? &it->second : nullptr;
}

} // namespace

bool MergeShip_Termina_Enabled() {
    auto ctx = Rando::Context::GetInstance();
    return ctx != nullptr && ctx->GetOption(RSK_MERGESHIP_TERMINA).Is(RO_GENERIC_ON);
}

bool MergeShip_Termina_IsTerminaCheck(RandomizerCheck rc) {
    return rc > RC_UNKNOWN_CHECK && rc < RC_MAX && Rando::StaticData::GetLocation(rc)->GetWorld() == RW_TERMINA;
}

const MsTerminaCheck* MergeShip_Termina_GetCheck(RandomizerCheck rc) {
    if (gMsTerminaCheckCount == 0 || rc < gMsTerminaChecks[0].rc) {
        return nullptr;
    }
    const size_t index = (size_t)(rc - gMsTerminaChecks[0].rc);
    return (index < gMsTerminaCheckCount && gMsTerminaChecks[index].rc == rc) ? &gMsTerminaChecks[index] : nullptr;
}

bool MergeShip_Termina_IsTerminaSetting(RandomizerSettingKey rsk) {
    return rsk > RSK_MERGESHIP_TERMINA && rsk < RSK_MAX;
}

const std::string* MergeShip_Termina_SettingText(RandomizerSettingKey rsk, bool description) {
    static const std::string kName = "Mergeship: Unified Randomizer (Termina)";
    static const std::string kDesc =
        "One seed over Hyrule and Termina. Majora's Mask's checks join the seed behind a door in the Lost Woods (the "
        "exit that loops back to Kokiri Forest leads to Clock Town), and its items join the item pool; items found in "
        "either world are shared. Termina follows 2 Ship 2 Harkinian's logic, with its three days of time, and every "
        "check 2Ship can shuffle is shuffled unless one of the Termina options below leaves it out.";
    if (rsk == RSK_MERGESHIP_TERMINA) {
        return description ? &kDesc : &kName;
    }
    // The goal's settings, and friendlier names for 2Ship's moon and Majora options (MergeShipGoal.h)
    if (const std::string* goal = MergeShip_Goal_SettingText(rsk, description)) {
        return goal;
    }
    // Hints over both worlds (MergeShipUnifiedHints.h)
    if (const std::string* hints = MergeShip_Hints_SettingText(rsk, description)) {
        return hints;
    }
    if (!MergeShip_Termina_IsTerminaSetting(rsk)) {
        return nullptr;
    }
    if (const std::string* dungeonItems = description ? DungeonItemDescription(rsk) : nullptr) {
        return dungeonItems;
    }
    static std::map<RandomizerSettingKey, std::pair<std::string, std::string>> texts;
    if (texts.empty()) {
        for (size_t i = 0; i < gMsTerminaOptionCount; i++) {
            const MsTerminaOption& option = gMsTerminaOptions[i];
            if (option.rsk != RSK_MAX) {
                texts[option.rsk] = { option.name, std::string("2 Ship 2 Harkinian's randomizer option ") +
                                                       option.mmOption + " for Termina (unified randomizer)." };
            }
        }
    }
    auto it = texts.find(rsk);
    if (it == texts.end()) {
        return nullptr;
    }
    return description ? &it->second.second : &it->second.first;
}

bool MergeShip_Termina_SettingCounts(RandomizerSettingKey rsk) {
    if (rsk == RSK_MERGESHIP_TERMINA || MergeShip_Termina_IsTerminaSetting(rsk)) {
        return MergeShip_Termina_Enabled();
    }
    return true;
}

void MergeShip_Termina_PrepareLocationPool() {
    sPool = TerminaPool();
    MergeShip_TerminaLogic_Reset(); // Termina's logic follows this generation's mode from here on
    if (!MergeShip_Termina_Enabled()) {
        return; // no RNG, nothing: seeds without Termina are unchanged
    }
    sPool.inPool.assign(gMsTerminaCheckCount, false);
    sPool.vanillaShuffled.assign(gMsTerminaCheckCount, false);
    sPool.vanillaPlaced.assign(gMsTerminaCheckCount, false);

    // Skulltulas: in each Spider House, RO_SKULLTULA_SHUFFLED random ones are checks; the rest keep their token.
    const bool skulltulas = MMOn("RO_SHUFFLE_GOLD_SKULLTULAS");
    std::vector<bool> vanillaSkulltula(gMsTerminaCheckCount, false);
    if (skulltulas) {
        std::map<std::string, std::vector<size_t>> byScene;
        for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
            if (gMsTerminaChecks[i].type == MS_MMCT_SKULL_TOKEN) {
                byScene[gMsTerminaChecks[i].mmScene].push_back(i);
            }
        }
        const size_t shuffled = (size_t)MMOption("RO_SKULLTULA_SHUFFLED");
        for (auto& [scene, checks] : byScene) {
            if (shuffled >= checks.size()) {
                continue;
            }
            for (size_t i = 0; i < checks.size(); i++) {
                std::swap(checks[i], checks[Random(0, (uint32_t)checks.size())]);
            }
            for (size_t i = shuffled; i < checks.size(); i++) {
                vanillaSkulltula[checks[i]] = true;
            }
        }
    }

    const int songs = MMOption("RO_SHUFFLE_SONGS");
    const int remains = MMOption("RO_SHUFFLE_BOSS_REMAINS");
    const bool shops = MMOn("RO_SHUFFLE_SHOPS");
    // Whether 2Ship shuffles check i (its GeneratePools rules); `listed`: kept vanilla but listed as shuffled.
    auto shuffled = [&](size_t i, bool& listed) {
        const MsTerminaCheck& check = gMsTerminaChecks[i];
        listed = false;
        if (StaysAtVanillaCheck(check.mmVanillaItem)) {
            listed = true;
            return false;
        }
        if (check.type == MS_MMCT_SKULL_TOKEN) {
            if (!skulltulas) {
                return false;
            }
            if (vanillaSkulltula[i]) {
                listed = true;
                return false;
            }
        }
        if (check.type == MS_MMCT_SONG && songs == kSongShuffleVanilla) {
            return false;
        }
        if (check.type == MS_MMCT_REMAINS && remains == kRemainsShuffleVanilla) {
            return false;
        }
        const char* typeOption = TypeOption(check.type);
        if (typeOption != nullptr && !MMOn(typeOption)) {
            return false;
        }
        if (check.type == MS_MMCT_SHOP && !shops && strcmp(check.mmCheck, "RC_CURIOSITY_SHOP_SPECIAL_ITEM") != 0 &&
            strcmp(check.mmCheck, "RC_BOMB_SHOP_ITEM_03") != 0 &&
            strcmp(check.mmCheck, "RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM") != 0) {
            return false;
        }
        return true;
    };
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        const MsTerminaCheck& check = gMsTerminaChecks[i];
        if (strcmp(check.mmScene, "SCENE_LAST_BS") == 0) {
            continue; // 2Ship never shuffles Majora's two pots
        }
        bool listed = false;
        if (!shuffled(i, listed)) {
            sPool.vanillaShuffled[i] = listed;
            // 2Ship's logic collects a kept check's vanilla item; SoH's collects what a pool location holds, so a
            // vanilla item the logic needs (Termina's or Hyrule's) is placed there (AddItemsToPool).
            if (Rando::StaticData::RetrieveItem(check.vanillaItem).IsAdvancement()) {
                sPool.vanillaPlaced[i] = true;
                sPool.vanillaPlacedCount++;
            }
            continue;
        }
        if (check.type == MS_MMCT_SHOP || check.type == MS_MMCT_TINGLE_SHOP) {
            // As 2Ship: a random price chosen ahead of time; the logic accounts for it (CAN_AFFORD).
            sPool.prices[check.rc] = (uint16_t)Random(0, 200);
        }
        sPool.inPool[i] = true;
        sPool.locations++;
    }
    SPDLOG_INFO("MergeShip Termina: {} of {} Majora's Mask checks shuffled, {} kept vanilla ({} of them placed for the "
                "logic)",
                sPool.locations, gMsTerminaCheckCount, gMsTerminaCheckCount - sPool.locations,
                sPool.vanillaPlacedCount);
}

// MergeShip2 M7: the deterministic half of PrepareLocationPool's `shuffled` rule, for trackers at run time (the seed's
// own RNG is gone by then). Partly shuffled Spider Houses count as shuffled: which of their tokens stayed is RNG.
bool MergeShip_Termina_IsTrackedCheck(RandomizerCheck rc) {
    const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
    if ((check == nullptr) || !MergeShip_Termina_Enabled() || (strcmp(check->mmScene, "SCENE_LAST_BS") == 0)) {
        return false;
    }
    if (StaysAtVanillaCheck(check->mmVanillaItem)) {
        return true; // listed as shuffled: 2Ship gives it through its randomizer, which writes the ledger
    }
    if (check->type == MS_MMCT_SKULL_TOKEN) {
        return MMOn("RO_SHUFFLE_GOLD_SKULLTULAS");
    }
    if (check->type == MS_MMCT_SONG && MMOption("RO_SHUFFLE_SONGS") == kSongShuffleVanilla) {
        return false;
    }
    if (check->type == MS_MMCT_REMAINS && MMOption("RO_SHUFFLE_BOSS_REMAINS") == kRemainsShuffleVanilla) {
        return false;
    }
    const char* typeOption = TypeOption(check->type);
    if (typeOption != nullptr && !MMOn(typeOption)) {
        return false;
    }
    if (check->type == MS_MMCT_SHOP && !MMOn("RO_SHUFFLE_SHOPS") &&
        strcmp(check->mmCheck, "RC_CURIOSITY_SHOP_SPECIAL_ITEM") != 0 &&
        strcmp(check->mmCheck, "RC_BOMB_SHOP_ITEM_03") != 0 &&
        strcmp(check->mmCheck, "RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM") != 0) {
        return false;
    }
    return true;
}

bool MergeShip_Termina_IsInLocationPool(RandomizerCheck rc) {
    const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
    if (check == nullptr || sPool.inPool.empty()) {
        return false;
    }
    return sPool.inPool[check - gMsTerminaChecks] || sPool.vanillaPlaced[check - gMsTerminaChecks];
}

int MergeShip_Termina_OptionValue(size_t optionIndex) {
    if (optionIndex >= gMsTerminaOptionCount) {
        return 0;
    }
    return EffectiveMMOption(gMsTerminaOptions[optionIndex]);
}

uint16_t MergeShip_Termina_GetPrice(RandomizerCheck rc) {
    auto it = sPool.prices.find(rc);
    return it != sPool.prices.end() ? it->second : 0;
}

const std::vector<RandomizerGet>& MergeShip_Termina_LogicStartingItems() {
    return sPool.logicStarting;
}

bool MergeShip_Termina_StartingStrayFairies(uint8_t counts[5]) {
    if (!MergeShip_Termina_Enabled() || (MMOption("RO_PLACEMENT_STRAY_FAIRIES") != kDungeonItemStartWith)) {
        return false;
    }
    counts[0] = 1; // Clock Town's one (MM WEEKEVENTREG_08_80)
    for (int area = 1; area < 5; area++) {
        counts[area] = (uint8_t)kStrayFairiesPerTemple; // Woodfall, Snowhead, Great Bay, Stone Tower
    }
    return true;
}

void MergeShip_Termina_AddItemsToPool(std::vector<RandomizerGet>& itemPool,
                                      const std::vector<const std::vector<RandomizerGet>*>& others) {
    if (!MergeShip_Termina_Enabled() || sPool.inPool.empty()) {
        return;
    }

    // 2Ship's pool: the vanilla item of every shuffled check...
    std::vector<RandomizerGet> items;
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        if (sPool.inPool[i]) {
            items.push_back(gMsTerminaChecks[i].vanillaItem);
        }
    }
    // ...the sword and shield, which have no vanilla location...
    items.push_back(RGOfKey("PROGRESSIVE_MM_SWORD"));
    items.push_back(RGOfKey("HYLIAN_SHIELD"));
    // ...and the items of 2Ship's optional shuffles.
    if (MMOn("RO_SHUFFLE_BOSS_SOULS")) {
        for (RandomizerGet rg : RGsWithPrefix("MM_SOUL_BOSS_")) {
            items.push_back(rg);
        }
    }
    if (MMOn("RO_SHUFFLE_ENEMY_SOULS")) {
        for (RandomizerGet rg : RGsWithPrefix("MM_SOUL_ENEMY_")) {
            items.push_back(rg);
        }
    }
    if (MMOn("RO_CLOCK_SHUFFLE")) {
        if (MMOption("RO_CLOCK_SHUFFLE_PROGRESSIVE") == kClockShuffleRandom) {
            for (const char* key : { "MM_TIME_DAY_1", "MM_TIME_NIGHT_1", "MM_TIME_DAY_2", "MM_TIME_NIGHT_2",
                                     "MM_TIME_DAY_3", "MM_TIME_NIGHT_3" }) {
                items.push_back(RGOfKey(key));
            }
        } else {
            items.insert(items.end(), 6, RGOfKey("MM_TIME_PROGRESSIVE"));
        }
    }
    if (MMOn("RO_SHUFFLE_SWIM")) {
        items.push_back(RGOfKey("BRONZE_SCALE"));
    }
    if (MMOn("RO_SHUFFLE_OCARINA_BUTTONS")) {
        for (const char* key : { "OCARINA_A_BUTTON", "OCARINA_C_DOWN_BUTTON", "OCARINA_C_RIGHT_BUTTON",
                                 "OCARINA_C_LEFT_BUTTON", "OCARINA_C_UP_BUTTON" }) {
            items.push_back(RGOfKey(key));
        }
    }
    if (MMOn("RO_SHUFFLE_SONG_SUN")) {
        items.push_back(RGOfKey("SUNS_SONG"));
    }
    if (MMOn("RO_SHUFFLE_SONG_DOUBLE_TIME")) {
        items.push_back(RGOfKey("MM_SONG_OF_DOUBLE_TIME"));
    }
    if (MMOn("RO_SHUFFLE_SONG_INVERTED_TIME")) {
        items.push_back(RGOfKey("MM_INVERTED_SONG_OF_TIME"));
    }
    if (MMOn("RO_SHUFFLE_SONG_SARIA")) {
        items.push_back(RGOfKey("SARIAS_SONG"));
    }
    if (MMOn("RO_SHUFFLE_TYCOON_WALLET")) {
        items.push_back(RGOfKey("PROGRESSIVE_WALLET"));
    }
    if (MMOn("RO_SHUFFLE_SKELETON_KEY") && MMOption("RO_PLACEMENT_SMALL_KEYS") != kDungeonItemStartWith) {
        items.push_back(RGOfKey("SKELETON_KEY"));
    }
    // Triforce pieces: Hyrule's Triforce hunt settings decide (RO_SHUFFLE_TRIFORCE_PIECES is passed through as is).

    // Stray fairies past RO_STRAY_FAIRIES_MAX per temple leave the pool (2Ship).
    const size_t fairiesMax = (size_t)MMOption("RO_STRAY_FAIRIES_MAX");
    for (const char* key : { "MM_WOODFALL_STRAY_FAIRY", "MM_SNOWHEAD_STRAY_FAIRY", "MM_GREAT_BAY_STRAY_FAIRY",
                             "MM_STONE_TOWER_STRAY_FAIRY" }) {
        const RandomizerGet rg = RGOfKey(key);
        while ((size_t)std::count(items.begin(), items.end(), rg) > fairiesMax) {
            RemoveOne(items, rg);
        }
    }
    // 2Ship's computed starting items are given at file creation, so they leave the pool (one per copy).
    std::vector<RandomizerGet> starting;
    if (MMOn("RO_STARTING_MAPS_AND_COMPASSES")) {
        for (RandomizerGet rg : RGsWithPrefix("MM_TINGLE_MAP_")) {
            starting.push_back(rg);
        }
        for (const char* key : { "MM_GREAT_BAY_COMPASS", "MM_GREAT_BAY_MAP", "MM_SNOWHEAD_COMPASS", "MM_SNOWHEAD_MAP",
                                 "MM_STONE_TOWER_COMPASS", "MM_STONE_TOWER_MAP", "MM_WOODFALL_COMPASS",
                                 "MM_WOODFALL_MAP" }) {
            starting.push_back(RGOfKey(key));
        }
    }
    if (MMOption("RO_PLACEMENT_SMALL_KEYS") == kDungeonItemStartWith) {
        starting.insert(starting.end(), 1, RGOfKey("MM_WOODFALL_SMALL_KEY"));
        starting.insert(starting.end(), 3, RGOfKey("MM_SNOWHEAD_SMALL_KEY"));
        starting.insert(starting.end(), 1, RGOfKey("MM_GREAT_BAY_SMALL_KEY"));
        starting.insert(starting.end(), 4, RGOfKey("MM_STONE_TOWER_SMALL_KEY"));
    }
    if (MMOption("RO_PLACEMENT_BOSS_KEYS") == kDungeonItemStartWith) {
        for (const char* key :
             { "MM_WOODFALL_BOSS_KEY", "MM_SNOWHEAD_BOSS_KEY", "MM_GREAT_BAY_BOSS_KEY", "MM_STONE_TOWER_BOSS_KEY" }) {
            starting.push_back(RGOfKey(key));
        }
    }
    if (MMOption("RO_PLACEMENT_STRAY_FAIRIES") == kDungeonItemStartWith) {
        for (const char* key : { "MM_WOODFALL_STRAY_FAIRY", "MM_SNOWHEAD_STRAY_FAIRY", "MM_GREAT_BAY_STRAY_FAIRY",
                                 "MM_STONE_TOWER_STRAY_FAIRY" }) {
            starting.insert(starting.end(), kStrayFairiesPerTemple, RGOfKey(key));
        }
        starting.push_back(RGOfKey("MM_CLOCK_TOWN_STRAY_FAIRY"));
    }
    // Clock shuffle: 2Ship gives one half-day to start with when the player chose none; the unified seed chooses it
    // here and tells 2Ship (the "mm" section's startingItems) and the logic.
    if (MMOn("RO_CLOCK_SHUFFLE")) {
        if (MMOption("RO_CLOCK_SHUFFLE_PROGRESSIVE") == kClockShuffleRandom) {
            static const char* const kClocks[6] = { "MM_TIME_DAY_1", "MM_TIME_NIGHT_1", "MM_TIME_DAY_2",
                                                    "MM_TIME_NIGHT_2", "MM_TIME_DAY_3", "MM_TIME_NIGHT_3" };
            sPool.startingClock = RGOfKey(kClocks[Random(0, 6)]);
        } else {
            sPool.startingClock = RGOfKey("MM_TIME_PROGRESSIVE");
        }
        starting.push_back(sPool.startingClock);
    }
    // A started-with item leaves the whole unified pool: Termina's copy first, else a copy Hyrule's pool holds (Hyrule's
    // own shuffles of Majora's Mask's items, e.g. RSK_SHUFFLE_STRAY_FAIRIES), so no one is found twice.
    for (RandomizerGet rg : starting) {
        if (RemoveOne(items, rg)) {
            sPool.startingFromTermina++;
        } else if (RemoveOne(itemPool, rg)) {
            sPool.startingFromHyrule++;
        }
    }
    // Starting with every stray fairy, the copies Hyrule's own shuffle adds (RSK_SHUFFLE_STRAY_FAIRIES) would only count
    // past MM's 15 (Clock Town's past its one): they leave the pool too.
    if (MMOption("RO_PLACEMENT_STRAY_FAIRIES") == kDungeonItemStartWith) {
        const size_t before = itemPool.size();
        std::erase_if(itemPool, [](RandomizerGet rg) {
            const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
            return info != nullptr && info->kind == MS_KIND_STRAY_FAIRY;
        });
        sPool.startingFromHyrule += before - itemPool.size();
    }
    sPool.logicStarting = starting;

    // Kept checks whose vanilla item the logic needs hold it (PrepareLocationPool).
    auto ctx = Rando::Context::GetInstance();
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        if (sPool.vanillaPlaced[i]) {
            ctx->PlaceItemInLocation(gMsTerminaChecks[i].rc, gMsTerminaChecks[i].vanillaItem);
        }
    }

    // The unified caps, counting what Hyrule's pool already holds. Each game's copy of an item both games have is its
    // own row (MsDelivery_OwnWorld), so Hyrule's copies never make Termina's junk: the pool holds each game's. With the
    // seed's "Combine Shared Items" a copy is the other game's too (MsDelivery_Counterpart): the pool holds one.
    const bool combined = MergeShip_Goal_CombineItems();
    std::vector<int> owned(MSI_MAX + 1, 0);
    int bottles = 0;
    auto count = [&](const std::vector<RandomizerGet>& pool) {
        for (RandomizerGet rg : pool) {
            const MsItemKey key = MsItems_KeyOfRG(rg);
            owned[key]++;
            if (combined && (key < MSI_MAX) && (MsDelivery_Counterpart(key) < MSI_MAX)) {
                owned[MsDelivery_Counterpart(key)]++;
            }
            const MsItemInfo* info = MsItems_Get(key);
            if (info != nullptr && info->kind == MS_KIND_BOTTLE) {
                bottles++;
            }
        }
    };
    count(itemPool);
    for (const std::vector<RandomizerGet>* pool : others) {
        count(*pool);
    }
    // Started-with items are owned too: a second copy of one is past its cap (Tingle sells each of his maps at two
    // places, so with Starting Maps And Compasses the other copy becomes junk instead of a map owned twice).
    for (RandomizerGet rg : starting) {
        owned[MsItems_KeyOfRG(rg)]++;
    }
    constexpr int kBottleCap = 6;
    for (RandomizerGet& rg : items) {
        const MsItemKey key = MsItems_KeyOfRG(rg);
        const MsItemInfo* info = MsItems_Get(key);
        bool junk = (info == nullptr);
        if (!junk && info->kind == MS_KIND_HEALTH && info->maxLevel == 0) {
            junk = true; // heart pieces and containers: Hyrule's budget of 20 hearts is the only one
        } else if (!junk && info->kind == MS_KIND_BOTTLE) {
            junk = bottles >= kBottleCap;
            bottles += junk ? 0 : 1;
        } else if (!junk && info->persist == MS_PERM && info->maxLevel > 0) {
            junk = owned[key] >= info->maxLevel;
        }
        if (junk) {
            rg = GetJunkItem();
            sPool.junked++;
        } else {
            owned[key]++;
        }
    }

    // As many items as locations: trim junk first, then the extras 2Ship added last.
    while (items.size() > sPool.locations) {
        auto it = std::find_if(items.rbegin(), items.rend(), [](RandomizerGet rg) {
            const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
            return info == nullptr || info->kind == MS_KIND_JUNK || info->kind == MS_KIND_TRAP;
        });
        if (it != items.rend()) {
            items.erase(std::next(it).base());
        } else {
            items.pop_back();
        }
        sPool.trimmed++;
    }
    sPool.itemsAdded = items.size();
    itemPool.insert(itemPool.end(), items.begin(), items.end());
    // The goal's own items: the Triforce Hunt's pieces over both worlds (MergeShipGoal.h)
    MergeShip_Goal_AddItemsToPool(itemPool);
    SPDLOG_INFO("MergeShip Termina: {} items for {} locations ({} made junk by the unified caps, {} trimmed)",
                sPool.itemsAdded, sPool.locations, sPool.junked, sPool.trimmed);
}

// ---- Restricted placements (2Ship's PlacementConstraints.cpp) ------------------------------------------------------

namespace {

constexpr int kSongShuffleSongLocations = 1; // RO_SONG_SHUFFLE_SONG_LOCATIONS
constexpr int kRemainsShuffleOwnDungeon = 2; // RO_REMAINS_SHUFFLE_OWN_DUNGEON

// SceneIdToDungeon: the temple a check's scene belongs to (0 Woodfall .. 3 Stone Tower), -1 for none.
int DungeonOfScene(const char* scene) {
    static const char* const kScenes[][3] = {
        { "SCENE_MITURIN", "SCENE_MITURIN_BS", nullptr },
        { "SCENE_HAKUGIN", "SCENE_HAKUGIN_BS", nullptr },
        { "SCENE_SEA", "SCENE_SEA_BS", nullptr },
        { "SCENE_INISIE_N", "SCENE_INISIE_R", "SCENE_INISIE_BS" },
    };
    for (int d = 0; d < 4; d++) {
        for (const char* s : kScenes[d]) {
            if (s != nullptr && strcmp(scene, s) == 0) {
                return d;
            }
        }
    }
    return -1;
}

// RandoItemIdToDungeon: the temple an item is confined to by the placement options, -1 for none.
int ConfinedDungeon(RandomizerGet rg) {
    const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
    if (info == nullptr) {
        return -1;
    }
    static const char* const kPrefixes[4] = { "MM_WOODFALL_", "MM_SNOWHEAD_", "MM_GREAT_BAY_", "MM_STONE_TOWER_" };
    static const char* const kRemains[4] = { "MM_ODOLWAS_REMAINS", "MM_GOHTS_REMAINS", "MM_GYORGS_REMAINS",
                                             "MM_TWINMOLDS_REMAINS" };
    for (int d = 0; d < 4; d++) {
        if (strcmp(info->keyName, kRemains[d]) == 0) {
            return MMOption("RO_SHUFFLE_BOSS_REMAINS") == kRemainsShuffleOwnDungeon ? d : -1;
        }
        if (!StartsWith(info->keyName, kPrefixes[d])) {
            continue;
        }
        const char* option = EndsWith(info->keyName, "_SMALL_KEY")     ? "RO_PLACEMENT_SMALL_KEYS"
                             : EndsWith(info->keyName, "_BOSS_KEY")    ? "RO_PLACEMENT_BOSS_KEYS"
                             : EndsWith(info->keyName, "_STRAY_FAIRY") ? "RO_PLACEMENT_STRAY_FAIRIES"
                                                                       : nullptr;
        return option != nullptr && MMOption(option) == kDungeonItemOwnDungeon ? d : -1;
    }
    return -1;
}

bool IsEmptyTerminaLocation(RandomizerCheck rc) {
    return MergeShip_Termina_GetCheck(rc) != nullptr &&
           Rando::Context::GetInstance()->GetItemLocation(rc)->GetPlacedRandomizerGet() == RG_NONE;
}

// Up to `count` items fit: the rest go back to the item pool (2Ship: "dungeon out of room: leave it for the general
// pool").
void ReturnOverflow(std::vector<RandomizerGet>& items, size_t count) {
    while (items.size() > count) {
        itemPool.push_back(items.back());
        items.pop_back();
    }
}

} // namespace

void MergeShip_Termina_PlaceRestrictedItems(
    const std::function<void(const std::vector<RandomizerGet>&, const std::vector<RandomizerCheck>&)>& assumedFill) {
    if (!MergeShip_Termina_Enabled() || sPool.inPool.empty()) {
        return;
    }
    auto ctx = Rando::Context::GetInstance();

    // Own dungeon: each temple's keys, boss key, stray fairies (and remains) inside it.
    for (int d = 0; d < 4; d++) {
        std::vector<RandomizerGet> items =
            FilterAndEraseFromPool(itemPool, [d](const RandomizerGet rg) { return ConfinedDungeon(rg) == d; });
        if (items.empty()) {
            continue;
        }
        const std::vector<RandomizerCheck> locations = FilterFromPool(ctx->allLocations, [d](const RandomizerCheck rc) {
            const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
            return check != nullptr && DungeonOfScene(check->mmScene) == d && IsEmptyTerminaLocation(rc);
        });
        ReturnOverflow(items, locations.size());
        sPool.restrictedItems += items.size();
        assumedFill(items, locations);
    }

    // Songs on song locations: MM's song checks hold the items MM's song checks had (Hyrule's own songs are Hyrule's
    // song setting's). Spare song checks get junk, the Song of Healing's first (2Ship: the player starts with the Song
    // of Time, so no two free songs at the start).
    if (MMOption("RO_SHUFFLE_SONGS") == kSongShuffleSongLocations) {
        std::vector<RandomizerGet> songItems;
        for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
            if (gMsTerminaChecks[i].type == MS_MMCT_SONG) {
                songItems.push_back(gMsTerminaChecks[i].vanillaItem);
            }
        }
        std::vector<RandomizerGet> items = FilterAndEraseFromPool(itemPool, [&songItems](const RandomizerGet rg) {
            return std::find(songItems.begin(), songItems.end(), rg) != songItems.end() &&
                   Rando::StaticData::RetrieveItem(rg).GetItemType() != ITEMTYPE_SONG;
        });
        std::vector<RandomizerCheck> locations = FilterFromPool(ctx->allLocations, [](const RandomizerCheck rc) {
            const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
            return check != nullptr && check->type == MS_MMCT_SONG && IsEmptyTerminaLocation(rc);
        });
        ReturnOverflow(items, locations.size());
        Shuffle(locations);
        auto healing = std::find(locations.begin(), locations.end(), RC_MM_STARTING_ITEM_SONG_OF_HEALING);
        if (healing != locations.end()) {
            std::swap(locations[0], *healing);
        }
        const size_t spare = locations.size() - items.size();
        for (size_t i = 0; i < spare; i++) {
            // A junk item of the pool, so the pool still has as many items as empty locations.
            auto junk = std::find_if(itemPool.begin(), itemPool.end(), [](RandomizerGet rg) {
                const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
                return info != nullptr && info->kind == MS_KIND_JUNK;
            });
            RandomizerGet rg = GetJunkItem();
            if (junk != itemPool.end()) {
                rg = *junk;
                itemPool.erase(junk);
            }
            ctx->PlaceItemInLocation(locations[i], rg);
            sPool.songJunk++;
        }
        locations.erase(locations.begin(), locations.begin() + spare);
        sPool.restrictedItems += items.size();
        assumedFill(items, locations);
    }
}

// ---- MERGESHIP_SEED ------------------------------------------------------------------------------------------------

namespace {

nlohmann::ordered_json MMSection(const nlohmann::ordered_json& ootSpoiler) {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::ordered_json mm;
    mm["type"] = "2S2H_RANDO_SPOILER";
    mm["commitHash"] = "MergeShip unified randomizer";
    mm["finalSeed"] = ctx->GetSeed();
    // The seed's "Combine Shared Items" (MergeShipGoal.h): 2Ship writes it to the shared save (itemsCombined).
    mm["combineSharedItems"] = MergeShip_Goal_CombineItems();
    mm["inputSeed"] = ctx->GetSeedString();
    mm["options"] = nlohmann::ordered_json::object();
    for (size_t i = 0; i < gMsTerminaOptionCount; i++) {
        mm["options"][gMsTerminaOptions[i].mmOption] = (uint32_t)MMOption(gMsTerminaOptions[i].mmOption);
    }
    mm["startingItems"] = nlohmann::ordered_json::array();
    if (sPool.startingClock != RG_NONE && MsItems_RIName(sPool.startingClock) != nullptr) {
        mm["startingItems"].push_back(MsItems_RIName(sPool.startingClock)); // clock shuffle's starting half-day
    }
    mm["sariaPriorityItems"] = nlohmann::ordered_json::array();
    mm["checks"] = nlohmann::ordered_json::object();
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        const MsTerminaCheck& check = gMsTerminaChecks[i];
        if (!sPool.inPool.empty() && sPool.vanillaShuffled[i]) {
            mm["checks"][check.mmCheck] = check.mmVanillaItem;
            continue;
        }
        if (sPool.inPool.empty() || !sPool.inPool[i]) {
            continue; // not shuffled: 2Ship leaves it vanilla
        }
        const RandomizerGet placed = ctx->GetItemLocation(check.rc)->GetPlacedRandomizerGet();
        const char* ri = MsItems_RIName(placed);
        if (ri == nullptr) {
            SPDLOG_ERROR("MergeShip Termina: {} holds {}, which has no 2Ship item", check.mmCheck, (int)placed);
            ri = "RI_JUNK";
        }
        auto price = sPool.prices.find(check.rc);
        if (price != sPool.prices.end()) {
            mm["checks"][check.mmCheck] = { { "randoItemId", ri }, { "price", price->second } };
        } else {
            mm["checks"][check.mmCheck] = ri;
        }
    }
    return mm;
}

nlohmann::ordered_json SharedSection(const nlohmann::ordered_json& ootSpoiler) {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::ordered_json shared;
    shared["msItemsVersion"] = MS_ITEMS_VERSION;
    shared["msItemCount"] = (int)MSI_MAX;
    shared["seed"] = ctx->GetSeedString();
    shared["finalSeed"] = ctx->GetSeed();
    // The door: in SoH the Lost Woods exit that loops back to Kokiri Forest, in 2Ship the Clock Tower (host M3).
    shared["door"] = { { "oot", "RR_LW_TERMINA_DOOR" }, { "mm", "RR_CLOCK_TOWER_INTERIOR" } };
    shared["winCondition"] = MergeShip_Goal_Name();
    shared["goal"] = MergeShip_Goal_SeedSection(); // MergeShipGoal.h
    shared["combineSharedItems"] = MergeShip_Goal_CombineItems(); // MsSharedSave.itemsCombined
    // Every placement with its world and canonical item, for the host's shared inventory (M3) and Anchor (M8).
    shared["placements"] = nlohmann::ordered_json::object();
    for (RandomizerCheck rc : ctx->allLocations) {
        const Rando::Location* location = Rando::StaticData::GetLocation(rc);
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        const MsItemInfo* item = MsItems_Get(MsItems_KeyOfRG(placed));
        const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
        shared["placements"][location->GetName()] = {
            { "world", location->GetWorld() == RW_TERMINA ? "mm" : "oot" },
            { "check", check != nullptr ? std::string(check->mmCheck) : std::string(EnumToString(rc)) },
            { "item", item != nullptr ? item->keyName : "NONE" },
        };
    }
    return shared;
}

} // namespace

nlohmann::ordered_json MergeShip_Termina_WrapSeed(nlohmann::ordered_json ootSpoiler) {
    nlohmann::ordered_json seed;
    seed["type"] = MS_SEED_TYPE;
    seed["version"] = MS_SEED_VERSION;
    seed["shared"] = SharedSection(ootSpoiler);
    seed["mm"] = MMSection(ootSpoiler);
    MergeShip_Goal_PatchMMOptions(seed["mm"]["options"]); // 2Ship leaves the Triforce and Majora to the goal
    // Hints and playthrough of both worlds: the fill is one fill, so SoH's already span Termina (M6 adds Termina's
    // own hints).
    seed["hints"] = { { "Gossip Stone Hints", ootSpoiler.value("Gossip Stone Hints", nlohmann::ordered_json::object()) },
                      { "Static Hints", ootSpoiler.value("Static Hints", nlohmann::ordered_json::object()) } };
    seed["playthrough"] = ootSpoiler.value("playthrough", nlohmann::ordered_json::object());
    // M8: the overrides of Termina's entrances, for 2Ship (include/mergeship/MsEntrances.h)
    seed["entrances"] = MergeShip_Entrances_SeedSection(ootSpoiler);
    seed["oot"] = std::move(ootSpoiler);
    return seed;
}

const nlohmann::json& MergeShip_Termina_UnwrapSeed(const nlohmann::json& file) {
    if (file.is_object() && file.contains("type") && file["type"] == MS_SEED_TYPE && file.contains("oot")) {
        return file.at("oot");
    }
    return file;
}

namespace {

// Termina's dungeon items by kind: 0 maps and compasses (Tingle's maps too), 1 small keys, 2 boss keys, 3 stray
// fairies, 4 the Skeleton Key; -1 for anything else (Hyrule's items included).
int TerminaDungeonItemKind(RandomizerGet rg) {
    const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
    if (info == nullptr) {
        return -1;
    }
    if (strcmp(info->keyName, "SKELETON_KEY") == 0) {
        return 4;
    }
    if (!StartsWith(info->keyName, "MM_")) {
        return -1;
    }
    switch (info->kind) {
        case MS_KIND_DUNGEON_MAP:
            return 0;
        case MS_KIND_SMALL_KEY:
            return 1;
        case MS_KIND_BOSS_KEY:
            return 2;
        case MS_KIND_STRAY_FAIRY:
            return 3;
        default:
            return StartsWith(info->keyName, "MM_TINGLE_MAP_") ? 0 : -1;
    }
}

// randoterm's start-with evidence (build/tests/rando_mm_start_with): Termina's dungeon item options as 2Ship plays
// them, the items the logic starts with and those still placed in either world (0 for a "Start With" kind), and the
// same for Hyrule's own start-with dungeon items (SoH's settings, its starting inventory).
std::vector<std::string> StartWithSummary() {
    auto ctx = Rando::Context::GetInstance();
    std::vector<std::string> out;
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "M4STARTOPT smallKeys=%d bossKeys=%d strayFairies=%d mapsCompasses=%d skeletonKey=%d fairiesRequired=%d "
             "fairiesMax=%d (2Ship RO_DUNGEON_ITEM_*: 0 anywhere 1 own dungeon 2 start with 3 vanilla)",
             MMOption("RO_PLACEMENT_SMALL_KEYS"), MMOption("RO_PLACEMENT_BOSS_KEYS"),
             MMOption("RO_PLACEMENT_STRAY_FAIRIES"), MMOption("RO_STARTING_MAPS_AND_COMPASSES"),
             MMOption("RO_SHUFFLE_SKELETON_KEY"), MMOption("RO_STRAY_FAIRIES_REQUIRED"),
             MMOption("RO_STRAY_FAIRIES_MAX"));
    out.push_back(buf);

    size_t starting[5] = {}, placed[5] = {};
    for (RandomizerGet rg : sPool.logicStarting) {
        const int kind = TerminaDungeonItemKind(rg);
        if (kind >= 0) {
            starting[kind]++;
        }
    }
    // Hyrule's dungeon items (SoH's dungeon list); Ganon's boss key has its own setting.
    std::map<RandomizerGet, int> hyrule; // 0 maps, 1 compasses, 2 small keys and key rings, 3 boss keys
    for (Rando::DungeonInfo* dungeon : ctx->GetDungeons()->GetDungeonList()) {
        if (dungeon == nullptr) {
            continue;
        }
        const std::pair<RandomizerGet, int> items[] = { { dungeon->GetMap(), 0 },      { dungeon->GetCompass(), 1 },
                                                        { dungeon->GetSmallKey(), 2 }, { dungeon->GetKeyRing(), 2 },
                                                        { dungeon->GetBossKey(), 3 } };
        for (const auto& [rg, kind] : items) {
            if (rg != RG_NONE && rg != RG_GANONS_CASTLE_BOSS_KEY) {
                hyrule[rg] = kind;
            }
        }
    }
    size_t hyruleStarting[4] = {}, hyrulePlaced[4] = {};
    for (RandomizerGet rg : StartingInventory) {
        auto it = hyrule.find(rg);
        if (it != hyrule.end()) {
            hyruleStarting[it->second]++;
        }
    }
    for (RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet rg = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        const int kind = TerminaDungeonItemKind(rg);
        if (kind >= 0) {
            placed[kind]++;
        }
        auto it = hyrule.find(rg);
        if (it != hyrule.end()) {
            hyrulePlaced[it->second]++;
        }
    }
    snprintf(buf, sizeof(buf),
             "M4START termina starting mapsCompasses=%zu smallKeys=%zu bossKeys=%zu strayFairies=%zu | placed "
             "mapsCompasses=%zu smallKeys=%zu bossKeys=%zu strayFairies=%zu skeletonKey=%zu | leftPool termina=%zu "
             "hyrule=%zu",
             starting[0], starting[1], starting[2], starting[3], placed[0], placed[1], placed[2], placed[3], placed[4],
             sPool.startingFromTermina, sPool.startingFromHyrule);
    out.push_back(buf);
    snprintf(buf, sizeof(buf),
             "M4START hyrule settings mapsCompasses=%d keys=%d bossKeys=%d (SoH RO_DUNGEON_ITEM_LOC_*: 0 start with) | "
             "starting maps=%zu compasses=%zu smallKeys=%zu bossKeys=%zu | placed maps=%zu compasses=%zu smallKeys=%zu "
             "bossKeys=%zu",
             (int)ctx->GetOption(RSK_SHUFFLE_MAPANDCOMPASS).Get(), (int)ctx->GetOption(RSK_KEYSANITY).Get(),
             (int)ctx->GetOption(RSK_BOSS_KEYSANITY).Get(), hyruleStarting[0], hyruleStarting[1], hyruleStarting[2],
             hyruleStarting[3], hyrulePlaced[0], hyrulePlaced[1], hyrulePlaced[2], hyrulePlaced[3]);
    out.push_back(buf);
    return out;
}

} // namespace

// M4 evidence (randoterm): the pool per world and per 2Ship check type, the playthrough's Termina spheres, and what a
// search with every placed item reaches (Majora, the bosses, how many Termina locations).
std::vector<std::string> MergeShip_Termina_LogicSummary() {
    auto ctx = Rando::Context::GetInstance();
    std::vector<std::string> out;
    char buf[1024];
    size_t majoraPots = 0;
    std::vector<size_t> typeTotal(MS_MMCT_MAX, 0), typeShuffled(MS_MMCT_MAX, 0), typePlaced(MS_MMCT_MAX, 0);
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        const MsTerminaCheck& check = gMsTerminaChecks[i];
        majoraPots += strcmp(check.mmScene, "SCENE_LAST_BS") == 0 ? 1 : 0;
        typeTotal[check.type]++;
        typeShuffled[check.type] += sPool.inPool[i] ? 1 : 0;
        typePlaced[check.type] += sPool.vanillaPlaced[i] ? 1 : 0;
    }
    snprintf(buf, sizeof(buf),
             "M4POOL termina checks=%zu randomizable=%zu shuffled=%zu vanillaPlaced=%zu outOfPool=%zu restricted=%zu "
             "songJunk=%zu startingClock=%s logicStartingItems=%zu",
             gMsTerminaCheckCount, gMsTerminaCheckCount - majoraPots, sPool.locations, sPool.vanillaPlacedCount,
             gMsTerminaCheckCount - sPool.locations - sPool.vanillaPlacedCount, sPool.restrictedItems, sPool.songJunk,
             sPool.startingClock != RG_NONE ? MsItems_RIName(sPool.startingClock) : "none",
             sPool.logicStarting.size());
    out.push_back(buf);
    for (const std::string& line : StartWithSummary()) {
        out.push_back(line);
    }
    std::string types = "M4TYPES";
    for (int t = 1; t < MS_MMCT_MAX; t++) {
        if (typeTotal[t] > 0) {
            types += " " + std::string(gMsTerminaCheckTypeNames[t] + 7) + "=" + std::to_string(typeShuffled[t]) + "/" +
                     std::to_string(typeTotal[t]);
        }
    }
    out.push_back(types);

    // Where progression went.
    size_t hyruleLocs = 0, terminaLocs = 0, advInTermina = 0, advInHyrule = 0, mmAdvInHyrule = 0, ootAdvInTermina = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        const bool inTermina = Rando::StaticData::GetLocation(rc)->GetWorld() == RW_TERMINA;
        (inTermina ? terminaLocs : hyruleLocs)++;
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        if (!Rando::StaticData::RetrieveItem(placed).IsAdvancement()) {
            continue;
        }
        const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(placed));
        const bool mmItem = info != nullptr && StartsWith(info->keyName, "MM_");
        if (inTermina) {
            advInTermina++;
            ootAdvInTermina += mmItem ? 0 : 1;
        } else {
            advInHyrule++;
            mmAdvInHyrule += mmItem ? 1 : 0;
        }
    }
    snprintf(buf, sizeof(buf),
             "M4WORLD hyruleLocations=%zu terminaLocations=%zu progressionInHyrule=%zu (MM items %zu) "
             "progressionInTermina=%zu (Hyrule/shared items %zu)",
             hyruleLocs, terminaLocs, advInHyrule, mmAdvInHyrule, advInTermina, ootAdvInTermina);
    out.push_back(buf);

    // The playthrough: its spheres, and how many hold Termina locations.
    size_t spheres = ctx->playthroughLocations.size(), terminaSpheres = 0, terminaItems = 0, items = 0;
    std::string lastTermina;
    for (const auto& sphere : ctx->playthroughLocations) {
        bool any = false;
        for (RandomizerCheck rc : sphere) {
            items++;
            if (MergeShip_Termina_GetCheck(rc) != nullptr) {
                any = true;
                terminaItems++;
                lastTermina = Rando::StaticData::GetLocation(rc)->GetName();
            }
        }
        terminaSpheres += any ? 1 : 0;
    }
    snprintf(buf, sizeof(buf),
             "M4PLAYTHROUGH beatable=%d spheres=%zu items=%zu terminaSpheres=%zu terminaItems=%zu goal=%s "
             "lastTerminaItem=\"%s\"",
             (int)ctx->playthroughBeatable, spheres, items, terminaSpheres, terminaItems, MergeShip_Goal_Name().c_str(),
             lastTermina.c_str());
    out.push_back(buf);

    // Every placed item collected: what the world graph reaches.
    auto logic = ctx->GetLogic();
    logic->Reset();
    ReachabilitySearch(ctx->allLocations);
    size_t terminaReached = 0, hyruleReached = 0, regionsReached = 0, regions = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        if (!ctx->GetItemLocation(rc)->IsAddedToPool()) {
            continue;
        }
        (Rando::StaticData::GetLocation(rc)->GetWorld() == RW_TERMINA ? terminaReached : hyruleReached)++;
    }
    for (int rr = RR_NONE + 1; rr < RR_MAX; rr++) {
        const Region* region = RegionTable((RandomizerRegion)rr);
        if (region->world == RW_TERMINA) {
            regions++;
            regionsReached += region->HasAccess() ? 1 : 0;
        }
    }
    const Region* lair = RegionTable(RR_MM_MOON_MAJORAS_LAIR);
    snprintf(buf, sizeof(buf),
             "M4REACH terminaLocations=%zu/%zu hyruleLocations=%zu/%zu terminaRegions=%zu/%zu majorasLair=%d "
             "majoraDefeated=%d odolwa=%d goht=%d gyorg=%d twinmold=%d lairTime=%llX",
             terminaReached, terminaLocs, hyruleReached, hyruleLocs, regionsReached, regions, (int)lair->HasAccess(),
             (int)logic->Get(LOGIC_MM_MAJORA_DEFEATED), (int)logic->Get(LOGIC_MM_ODOLWA_DEFEATED),
             (int)logic->Get(LOGIC_MM_GOHT_DEFEATED), (int)logic->Get(LOGIC_MM_GYORG_DEFEATED),
             (int)logic->Get(LOGIC_MM_TWINMOLD_DEFEATED), (unsigned long long)lair->mmTime);
    out.push_back(buf);
    // Unreached Termina locations, for the eye (a few).
    int shown = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        if (shown < 8 && MergeShip_Termina_GetCheck(rc) != nullptr && !ctx->GetItemLocation(rc)->IsAddedToPool()) {
            out.push_back("M4UNREACHED " + Rando::StaticData::GetLocation(rc)->GetName());
            shown++;
        }
    }
    return out;
}

std::vector<std::string> MergeShip_Termina_Summary() {
    auto ctx = Rando::Context::GetInstance();
    std::vector<std::string> out;
    const bool enabled = MergeShip_Termina_Enabled();
    size_t termina = 0, hyrule = 0, ootItemsInTermina = 0, mmItemsInHyrule = 0, proxies = 0, empty = 0;
    uint32_t digest = 2166136261u; // FNV-1a over "location=item" of every placement, in pool order
    for (RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        const MsItemInfo* item = MsItems_Get(MsItems_KeyOfRG(placed));
        const bool inTermina = Rando::StaticData::GetLocation(rc)->GetWorld() == RW_TERMINA;
        (inTermina ? termina : hyrule)++;
        if (placed == RG_NONE) {
            empty++;
        }
        if (item != nullptr && inTermina && item->proxy) {
            proxies++;
        }
        if (item != nullptr && inTermina && !item->usableInMM) {
            ootItemsInTermina++;
        }
        if (item != nullptr && !inTermina && !item->usableInOoT) {
            mmItemsInHyrule++;
        }
        const std::string line = std::to_string((int)rc) + "=" + std::to_string((int)placed) + ";";
        for (char c : line) {
            digest = (digest ^ (uint8_t)c) * 16777619u;
        }
    }
    char buf[512];
    snprintf(buf, sizeof(buf), "enabled=%d locations=%zu hyrule=%zu termina=%zu empty=%zu digest=%08X", (int)enabled,
             ctx->allLocations.size(), hyrule, termina, empty, digest);
    out.push_back(buf);
    snprintf(buf, sizeof(buf),
             "terminaPool=%zu itemsAdded=%zu junkedByCaps=%zu trimmed=%zu proxiesInTermina=%zu "
             "mmUnusableInTermina=%zu ootUnusableInHyrule=%zu",
             sPool.locations, sPool.itemsAdded, sPool.junked, sPool.trimmed, proxies, ootItemsInTermina,
             mmItemsInHyrule);
    out.push_back(buf);
    const RandomizerGet uncovered = MsItems_FirstUncoveredRG();
    snprintf(buf, sizeof(buf), "msItems=%d version=%d uncoveredRG=%d terminaChecks=%zu terminaOptions=%zu", (int)MSI_MAX,
             MS_ITEMS_VERSION, (int)uncovered, gMsTerminaCheckCount, gMsTerminaOptionCount);
    out.push_back(buf);
    if (enabled && !sPool.inPool.empty()) {
        for (const std::string& line : MergeShip_Termina_LogicSummary()) {
            out.push_back(line);
        }
    }
    // A few Termina placements, for the eye.
    int shown = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
        if (check == nullptr || shown >= 12) {
            continue;
        }
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        const char* ri = MsItems_RIName(placed);
        snprintf(buf, sizeof(buf), "place %s (%s) = %s -> %s", check->mmCheck,
                 Rando::StaticData::GetLocation(rc)->GetName().c_str(),
                 Rando::StaticData::RetrieveItem(placed).GetName().GetEnglish().c_str(), ri != nullptr ? ri : "?");
        out.push_back(buf);
        shown++;
    }
    return out;
}
