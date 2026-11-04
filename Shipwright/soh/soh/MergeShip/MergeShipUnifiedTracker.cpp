// MergeShip2 -- trackers over both worlds (MergeShipUnifiedTracker.h, the unified randomizer M7).

#include "soh/MergeShip/MergeShipUnifiedTracker.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/MsItems.h>
#include <mergeship/MsWorldCatalog.h>
#include <mergeship/SharedSave.h>
#include <spdlog/spdlog.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/MergeShip/MergeShipMsItems.h"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipUnified.h"
#include "soh/MergeShip/MergeShipUnifiedHints.h"

#include <ship/Context.h>
#include <fast/Fast3dGui.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h" // BUG-0131: IsSaveLoaded

extern "C" {
#include "z64.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/MergeShip/MergeShipAssets.h"      // BUG-0131: MergeShip_IsMajorasMaskMounted
#include "soh/MergeShip/MergeShipForms.h"       // MergeShip_HasMMMaskItem
#include "soh/MergeShip/MergeShipMMEquipment.h" // MergeShip_MMEquip_Owns
#include "soh/MergeShip/MergeShipMMItems.h"     // MergeShip_MMItems_Owns
#include "soh/MergeShip/MergeShipSongs.h"       // MergeShip_Songs_HasItem
void MergeShipTest_Log(const char* fmt, ...);
}

namespace {

// ---- the ledger, as trackers read it --------------------------------------------------------------------------------

// What the shared save's ledger says, kept in step with it (both engines only ever append).
struct LedgerView {
    uint32_t seed = 0;
    uint32_t count = 0;
    std::vector<uint16_t> terminaItem; // by Termina check index (2Ship RandoCheckId - 1): MsItemKey found, MSI_MAX none
    std::vector<uint16_t> itemCount;   // by MsItemKey: entries in either world (Anchor teammates' too)
} sLedger;

const MsSharedSave* Shared() {
    return static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());
}

void RefreshLedger() {
    const MsSharedSave* shared = Shared();
    const uint32_t count =
        (shared == nullptr) ? 0u : std::min<uint32_t>(shared->ledgerCount, (uint32_t)MS_SHARED_LEDGER_MAX);
    const uint32_t seed = (shared == nullptr) ? 0u : shared->ledgerSeed;
    if ((sLedger.terminaItem.size() != gMsTerminaCheckCount) || (seed != sLedger.seed) || (count < sLedger.count)) {
        sLedger.terminaItem.assign(gMsTerminaCheckCount, (uint16_t)MSI_MAX);
        sLedger.itemCount.assign(MSI_MAX, 0);
        sLedger.count = 0;
        sLedger.seed = seed;
    }
    for (uint32_t i = sLedger.count; i < count; i++) {
        const MsLedgerEntry& e = shared->ledger[i];
        if (e.item < MSI_MAX) {
            sLedger.itemCount[e.item]++;
        }
        if ((e.world == MS_WORLD_TERMINA) && (e.check >= 1) && (e.check <= gMsTerminaCheckCount)) {
            sLedger.terminaItem[e.check - 1] = e.item;
        }
    }
    sLedger.count = count;
}

// A Termina check's index in gMsTerminaChecks: 2Ship's RandoCheckId - 1, as the ledger numbers it.
size_t TerminaIndex(RandomizerCheck rc) {
    return (size_t)(rc - gMsTerminaChecks[0].rc);
}

bool TerminaFound(RandomizerCheck rc) {
    const size_t index = TerminaIndex(rc);
    return (index < sLedger.terminaItem.size()) && (sLedger.terminaItem[index] != (uint16_t)MSI_MAX);
}

const char* KeyLabel(uint16_t key) {
    const MsItemInfo* info = MsItems_Get((MsItemKey)key);
    return (info != nullptr) ? info->name : "?";
}

// ---- the check tracker's Termina section ----------------------------------------------------------------------------

struct TerminaTrackerArea {
    RandomizerArea area;
    std::vector<RandomizerCheck> checks;
};

// The seed's tracked Termina checks by area, built once per seed.
const std::vector<TerminaTrackerArea>& TrackerAreas() {
    static std::vector<TerminaTrackerArea> areas;
    static uint32_t areasSeed = 0;
    static bool built = false;
    auto ctx = Rando::Context::GetInstance();
    const uint32_t seed = (ctx != nullptr) ? ctx->GetSeed() : 0;
    if (built && (seed == areasSeed)) {
        return areas;
    }
    built = true;
    areasSeed = seed;
    areas.clear();
    if (ctx == nullptr) {
        return areas;
    }
    std::map<RandomizerArea, size_t> index;
    for (RandomizerArea area : MergeShip_Hints_TerminaAreas()) {
        index[area] = areas.size();
        areas.push_back({ area, {} });
    }
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        const RandomizerCheck rc = gMsTerminaChecks[i].rc;
        Rando::ItemLocation* loc = ctx->GetItemLocation(rc);
        if ((loc == nullptr) || (loc->GetPlacedRandomizerGet() == RG_NONE) || !MergeShip_Termina_IsTrackedCheck(rc)) {
            continue;
        }
        auto it = index.find(MergeShip_Hints_TerminaCheckArea(rc));
        if (it != index.end()) {
            areas[it->second].checks.push_back(rc); // Termina's starting items (no area) are not checks to find
        }
    }
    return areas;
}

// ---- the item tracker's Termina window ------------------------------------------------------------------------------

struct Cell {
    std::string text;
    bool owned;
};

struct Row {
    std::string label;
    std::vector<Cell> cells;
};

// How many copies of each MsItemKey the seed placed (both worlds), built once per seed: rows show only what is in it.
const std::vector<uint16_t>& PlacedCounts() {
    static std::vector<uint16_t> placed;
    static uint32_t placedSeed = 0;
    static bool built = false;
    auto ctx = Rando::Context::GetInstance();
    const uint32_t seed = (ctx != nullptr) ? ctx->GetSeed() : 0;
    if (built && (seed == placedSeed)) {
        return placed;
    }
    built = true;
    placedSeed = seed;
    placed.assign(MSI_MAX, 0);
    if (ctx == nullptr) {
        return placed;
    }
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        Rando::ItemLocation* loc = ctx->GetItemLocation((RandomizerCheck)rc);
        if ((loc == nullptr) || (loc->GetPlacedRandomizerGet() == RG_NONE)) {
            continue;
        }
        const MsItemKey key = MsItems_KeyOfRG(loc->GetPlacedRandomizerGet());
        if (key < MSI_MAX) {
            placed[key]++;
        }
    }
    return placed;
}

uint16_t Found(MsItemKey key) {
    return (key < sLedger.itemCount.size()) ? sLedger.itemCount[key] : 0;
}

// One cell per item: its label, owned once found (or held by the shared save's `sharedBit`).
Cell ItemCell(const char* label, MsItemKey key, bool shared = false) {
    return { label, shared || (Found(key) > 0) };
}

// A counted item: "label n/placed".
Cell CountCell(const char* label, const std::vector<MsItemKey>& keys, uint16_t sharedCount = 0, uint16_t max = 0) {
    uint32_t found = 0;
    uint32_t placed = 0;
    for (MsItemKey key : keys) {
        found += Found(key);
        placed += PlacedCounts()[key];
    }
    found = std::max<uint32_t>(found, sharedCount);
    const uint32_t total = (max != 0) ? max : placed;
    char text[96];
    snprintf(text, sizeof(text), "%s %u/%u", label, (unsigned)found, (unsigned)total);
    return { text, (total > 0) && (found >= total) };
}

std::vector<MsItemKey> KeysWithPrefix(const char* prefix) {
    std::vector<MsItemKey> keys;
    const size_t n = strlen(prefix);
    for (int k = 0; k < MSI_MAX; k++) {
        const MsItemInfo* info = MsItems_Get((MsItemKey)k);
        if ((info != nullptr) && (strncmp(info->keyName, prefix, n) == 0)) {
            keys.push_back((MsItemKey)k);
        }
    }
    return keys;
}

bool InSeed(const std::vector<MsItemKey>& keys) {
    for (MsItemKey key : keys) {
        if (PlacedCounts()[key] > 0) {
            return true;
        }
    }
    return false;
}

int PopCount(uint32_t bits) {
    int n = 0;
    for (; bits != 0; bits &= bits - 1) {
        n++;
    }
    return n;
}

std::vector<Row> TerminaItemRows() {
    RefreshLedger();
    const MsSharedSave* shared = Shared();
    const uint8_t remains = (shared != nullptr) ? shared->remains : 0;
    const uint32_t songs = (shared != nullptr) ? shared->songs : 0;
    const uint32_t masks = (shared != nullptr) ? shared->mmMasks : 0;
    auto song = [&](MsSong s) { return (songs & (1u << s)) != 0; };
    auto fairies = [&](int i) { return (uint16_t)((shared != nullptr) ? shared->strayFairies[i] : 0); };
    std::vector<Row> rows;

    rows.push_back({ "Remains",
                     { ItemCell("Odolwa", MSI_MM_ODOLWAS_REMAINS, (remains & 1) != 0), ItemCell("Goht", MSI_MM_GOHTS_REMAINS, (remains & 2) != 0),
                       ItemCell("Gyorg", MSI_MM_GYORGS_REMAINS, (remains & 4) != 0),
                       ItemCell("Twinmold", MSI_MM_TWINMOLDS_REMAINS, (remains & 8) != 0) } });
    rows.push_back({ "Masks",
                     { CountCell("Masks",
                                 { MSI_MM_MASK_OF_TRUTH, MSI_MM_KAFEIS_MASK, MSI_MM_ALL_NIGHT_MASK, MSI_MM_BUNNY_HOOD,
                                   MSI_MM_KEATON_MASK, MSI_MM_GAROS_MASK, MSI_MM_ROMANIS_MASK, MSI_MM_CIRCUS_LEADERS_MASK,
                                   MSI_MM_POSTMANS_HAT, MSI_MM_COUPLES_MASK, MSI_MM_GREAT_FAIRYS_MASK, MSI_MM_GIBDO_MASK,
                                   MSI_MM_DON_GEROS_MASK, MSI_MM_KAMAROS_MASK, MSI_MM_CAPTAINS_HAT, MSI_MM_STONE_MASK,
                                   MSI_MM_BREMEN_MASK, MSI_MM_BLAST_MASK, MSI_MM_MASK_OF_SCENTS, MSI_MM_GIANTS_MASK },
                                 (uint16_t)PopCount(masks & 0x1FFFFEu), 20) } });
    rows.push_back({ "Songs",
                     { ItemCell("Sonata", MSI_MM_SONATA_OF_AWAKENING, song(MS_SONG_SONATA)),
                       ItemCell("Lullaby", MSI_MM_GORONS_LULLABY, song(MS_SONG_GORON_LULLABY) ||
                                                                      Found(MSI_PROGRESSIVE_GORON_LULLABY) >= 2),
                       ItemCell("Bossa Nova", MSI_MM_NEW_WAVE_BOSSA_NOVA, song(MS_SONG_NEW_WAVE)),
                       ItemCell("Elegy", MSI_MM_ELEGY_OF_EMPTINESS, song(MS_SONG_ELEGY)),
                       ItemCell("Oath", MSI_MM_OATH_TO_ORDER, song(MS_SONG_OATH)),
                       ItemCell("Healing", MSI_MM_SONG_OF_HEALING, song(MS_SONG_HEALING)),
                       ItemCell("Soaring", MSI_MM_SONG_OF_SOARING, song(MS_SONG_SOARING)) } });
    struct Temple {
        const char* name;
        MsItemKey key, bossKey, map, compass, fairy;
        int fairyIndex;
    };
    const Temple temples[] = {
        { "Woodfall Temple", MSI_MM_WOODFALL_SMALL_KEY, MSI_MM_WOODFALL_BOSS_KEY, MSI_MM_WOODFALL_MAP,
          MSI_MM_WOODFALL_COMPASS, MSI_MM_WOODFALL_STRAY_FAIRY, 1 },
        { "Snowhead Temple", MSI_MM_SNOWHEAD_SMALL_KEY, MSI_MM_SNOWHEAD_BOSS_KEY, MSI_MM_SNOWHEAD_MAP,
          MSI_MM_SNOWHEAD_COMPASS, MSI_MM_SNOWHEAD_STRAY_FAIRY, 2 },
        { "Great Bay Temple", MSI_MM_GREAT_BAY_SMALL_KEY, MSI_MM_GREAT_BAY_BOSS_KEY, MSI_MM_GREAT_BAY_MAP,
          MSI_MM_GREAT_BAY_COMPASS, MSI_MM_GREAT_BAY_STRAY_FAIRY, 3 },
        { "Stone Tower Temple", MSI_MM_STONE_TOWER_SMALL_KEY, MSI_MM_STONE_TOWER_BOSS_KEY, MSI_MM_STONE_TOWER_MAP,
          MSI_MM_STONE_TOWER_COMPASS, MSI_MM_STONE_TOWER_STRAY_FAIRY, 4 },
    };
    for (const Temple& t : temples) {
        rows.push_back({ t.name,
                         { CountCell("Keys", { t.key }), ItemCell("Boss Key", t.bossKey), ItemCell("Map", t.map),
                           ItemCell("Compass", t.compass), CountCell("Fairies", { t.fairy }, fairies(t.fairyIndex), 15) } });
    }
    rows.push_back({ "Clock Town", { ItemCell("Stray Fairy", MSI_MM_CLOCK_TOWN_STRAY_FAIRY, fairies(0) > 0) } });
    rows.push_back({ "Termina",
                     { ItemCell("Hookshot", MSI_MM_HOOKSHOT), ItemCell("Powder Keg", MSI_MM_POWDER_KEG),
                       ItemCell("Pictograph Box", MSI_MM_PICTOGRAPH_BOX),
                       ItemCell("Great Fairy's Sword", MSI_MM_GREAT_FAIRYS_SWORD),
                       ItemCell("Bombers' Notebook", MSI_MM_BOMBERS_NOTEBOOK),
                       CountCell("Swords", { MSI_PROGRESSIVE_MM_SWORD, MSI_MM_RAZOR_SWORD, MSI_MM_GILDED_SWORD }) } });
    rows.push_back({ "Trade",
                     { ItemCell("Moon's Tear", MSI_MM_MOONS_TEAR), ItemCell("Land Deed", MSI_MM_LAND_TITLE_DEED),
                       ItemCell("Swamp Deed", MSI_MM_SWAMP_TITLE_DEED), ItemCell("Mountain Deed", MSI_MM_MOUNTAIN_TITLE_DEED),
                       ItemCell("Ocean Deed", MSI_MM_OCEAN_TITLE_DEED), ItemCell("Room Key", MSI_MM_ROOM_KEY),
                       ItemCell("Letter to Mama", MSI_MM_LETTER_TO_MAMA), ItemCell("Letter to Kafei", MSI_MM_LETTER_TO_KAFEI),
                       ItemCell("Pendant", MSI_MM_PENDANT_OF_MEMORIES) } });
    // Counted groups, each shown only when the seed places any of them.
    const std::pair<const char*, const char*> groups[] = {
        { "Owls", "MM_OWL_" },           { "Frogs", "MM_FROG_" },           { "Swamp Tokens", "MM_SWAMP_SKULLTULA" },
        { "Ocean Tokens", "MM_OCEAN_SKULLTULA" }, { "Boss Souls", "MM_SOUL_BOSS_" }, { "Enemy Souls", "MM_SOUL_ENEMY_" },
        { "Clocks", "MM_TIME_" },        { "Tingle Maps", "MM_TINGLE_MAP_" },
    };
    Row counted = { "Found", {} };
    for (const auto& [label, prefix] : groups) {
        const std::vector<MsItemKey> keys = KeysWithPrefix(prefix);
        if (InSeed(keys)) {
            counted.cells.push_back(CountCell(label, keys));
        }
    }
    if (!counted.cells.empty()) {
        rows.push_back(counted);
    }
    return rows;
}

// ---- the Hyrule catalog for 2Ship -----------------------------------------------------------------------------------

struct CatalogStore {
    uint32_t seed = 0;
    bool built = false;
    std::deque<std::string> strings; // stable c_str() as it grows
    std::vector<const char*> areaNames;
    std::vector<MsCatalogCheck> checks;
    MsWorldCatalog catalog = {};
} sCatalog;

void BuildCatalog() {
    auto ctx = Rando::Context::GetInstance();
    sCatalog.strings.clear();
    sCatalog.areaNames.clear();
    sCatalog.checks.clear();
    std::vector<int> areaIndex(RCAREA_INVALID + 1, -1);
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        if (MergeShip_Termina_IsTerminaCheck((RandomizerCheck)rc)) {
            continue;
        }
        const Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)rc);
        Rando::ItemLocation* itemLoc = ctx->GetItemLocation((RandomizerCheck)rc);
        // Every placement (hints may name any of them), and the checks the check tracker shows (MS_CATALOG_TRACKED).
        // Outside a randomizer file (the harness forcing the unified mode on its debug file) SoH's tracker shows
        // vanilla completion checks only: every placement counts as tracked there.
        const bool hasItem = itemLoc->GetPlacedRandomizerGet() != RG_NONE;
        const bool tracked = IS_RANDO ? CheckTracker::IsVisibleInCheckTracker((RandomizerCheck)rc) : hasItem;
        if (!tracked && !hasItem) {
            continue;
        }
        const RandomizerCheckArea area = loc->GetArea();
        if ((area < 0) || (area >= RCAREA_INVALID)) {
            continue;
        }
        if (areaIndex[area] < 0) {
            areaIndex[area] = (int)sCatalog.areaNames.size();
            sCatalog.strings.push_back(RandomizerCheckObjects::GetRCAreaName(area));
            sCatalog.areaNames.push_back(sCatalog.strings.back().c_str());
        }
        const RandomizerGet placed = itemLoc->GetPlacedRandomizerGet();
        const MsItemKey key = MsItems_KeyOfRG(placed);
        const MsItemInfo* info = MsItems_Get(key);
        uint8_t flags = 0;
        if ((placed != RG_NONE) && Rando::StaticData::RetrieveItem(placed).IsMajorItem() && (info != nullptr) &&
            (info->persist == MS_PERM) && (info->kind != MS_KIND_JUNK)) {
            flags |= MS_CATALOG_MAJOR;
        }
        if (loc->IsDungeon()) {
            flags |= MS_CATALOG_DUNGEON;
        }
        if (tracked) {
            flags |= MS_CATALOG_TRACKED;
        }
        sCatalog.strings.push_back(loc->GetShortName());
        MsCatalogCheck check = {};
        check.check = (uint16_t)rc;
        check.item = (uint16_t)key;
        check.area = (uint16_t)areaIndex[area];
        check.flags = flags;
        check.name = sCatalog.strings.back().c_str();
        sCatalog.checks.push_back(check);
    }
    // In the tracker's order: area by area, each area's checks as the location table lists them.
    std::stable_sort(sCatalog.checks.begin(), sCatalog.checks.end(),
                     [](const MsCatalogCheck& a, const MsCatalogCheck& b) { return a.area < b.area; });
    sCatalog.catalog.size = sizeof(MsWorldCatalog);
    sCatalog.catalog.version = MS_CATALOG_VERSION;
    sCatalog.catalog.seed = ctx->GetSeed();
    sCatalog.catalog.world = MS_WORLD_HYRULE;
    sCatalog.catalog.areaCount = (uint32_t)sCatalog.areaNames.size();
    sCatalog.catalog.areaNames = sCatalog.areaNames.data();
    sCatalog.catalog.checkCount = (uint32_t)sCatalog.checks.size();
    sCatalog.catalog.checks = sCatalog.checks.data();
    sCatalog.seed = ctx->GetSeed();
    sCatalog.built = true;
}

// ---- harness ------------------------------------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    char line[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip trackers: {}", line);
    MergeShipTest_Log("%s", line);
}

} // namespace

bool MergeShip_CheckTracker_TerminaActive() {
    return MergeShipUnified_Active() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TrackTermina"), 1);
}

bool MergeShip_CheckTracker_TerminaTotals(uint16_t* gotten, uint16_t* total) {
    *gotten = 0;
    *total = 0;
    if (!MergeShip_CheckTracker_TerminaActive()) {
        return false;
    }
    RefreshLedger();
    uint32_t found = 0;
    uint32_t count = 0;
    for (const TerminaTrackerArea& area : TrackerAreas()) {
        for (RandomizerCheck rc : area.checks) {
            count++;
            found += TerminaFound(rc) ? 1 : 0;
        }
    }
    *gotten = (uint16_t)found;
    *total = (uint16_t)count;
    return true;
}

void MergeShip_CheckTracker_DrawTermina(const ImGuiTextFilter& search, bool hideCollected, const ImVec4& collectedColor) {
    if (!MergeShip_CheckTracker_TerminaActive()) {
        return;
    }
    RefreshLedger();
    uint16_t gotten = 0;
    uint16_t total = 0;
    MergeShip_CheckTracker_TerminaTotals(&gotten, &total);
    ImGui::PushID("MergeShipTermina");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Termina (%u / %u)", (unsigned)gotten, (unsigned)total);
    for (const TerminaTrackerArea& area : TrackerAreas()) {
        if (area.checks.empty()) {
            continue;
        }
        const char* areaName = MergeShip_Hints_TerminaAreaName(area.area);
        const bool areaMatches = search.PassFilter(areaName);
        size_t found = 0;
        bool anyMatches = areaMatches;
        for (RandomizerCheck rc : area.checks) {
            found += TerminaFound(rc) ? 1 : 0;
            anyMatches = anyMatches || search.PassFilter(Rando::StaticData::GetLocation(rc)->GetShortName().c_str());
        }
        if (!anyMatches) {
            continue;
        }
        const bool complete = (found == area.checks.size());
        std::string label = std::string(areaName) + "##MsTerminaArea";
        ImGui::SetNextItemOpen(false, ImGuiCond_Once);
        const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen);
        ImGui::SameLine();
        ImGui::TextColored(complete ? collectedColor : ImVec4(1, 1, 1, 1), "(%zu / %zu)", found, area.checks.size());
        if (!open) {
            continue;
        }
        for (RandomizerCheck rc : area.checks) {
            const std::string& name = Rando::StaticData::GetLocation(rc)->GetShortName();
            if (!areaMatches && !search.PassFilter(name.c_str())) {
                continue;
            }
            const bool isFound = TerminaFound(rc);
            if (isFound && hideCollected) {
                continue;
            }
            ImGui::Indent(20.0f);
            if (isFound) {
                ImGui::TextColored(collectedColor, "%s (%s)", name.c_str(), KeyLabel(sLedger.terminaItem[TerminaIndex(rc)]));
            } else {
                ImGui::Text("%s", name.c_str());
            }
            ImGui::Unindent(20.0f);
        }
    }
    ImGui::PopID();
}

bool MergeShip_ItemTracker_ShowTermina() {
    return MergeShipUnified_Active() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ItemTrackerTermina"), 1);
}

void MergeShip_ItemTracker_DrawTermina() {
    const ImVec4 owned(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 missing(0.45f, 0.45f, 0.45f, 1.0f);
    for (const Row& row : TerminaItemRows()) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s:", row.label.c_str());
        for (const Cell& cell : row.cells) {
            ImGui::SameLine();
            ImGui::TextColored(cell.owned ? owned : missing, "%s", cell.text.c_str());
        }
    }
}

// ---- BUG-0131: the item tracker's Termina column -----------------------------------------------------------------

namespace {

struct TerminaIcon {
    u8 item;         // ITEM_MM_*: the icon (gItemIcons) and, for most, what owning it means
    const char* name;
    Color_RGB8 tint; // the songs' notes take Majora's Mask's quest-page colours (z_kaleido_collect.c)
};

bool TerminaOwns(u8 item) {
    if (!GameInteractor::IsSaveLoaded()) {
        return false;
    }
    if (MERGESHIP_ITEM_IS_MM_EQUIPMENT(item)) {
        return MergeShip_MMEquip_Owns(MERGESHIP_MMEQUIP_ID(item));
    }
    if (MergeShip_HasMMMaskItem(item)) {
        return true;
    }
    return MergeShip_Songs_HasItem(item) || MergeShip_MMItems_Owns(item);
}

// The texture of `path` for the tracker, registered on first use (full, and faded as Ship of Harkinian fades what is
// not owned).
const char* TerminaTexture(const char* path, bool owned, Color_RGB8 tint) {
    static std::map<std::string, std::string> sRegistered;
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    const std::string key = std::string(path) + "#" + std::to_string(tint.r) + "," + std::to_string(tint.g) + "," +
                            std::to_string(tint.b);
    if ((gui != nullptr) && (sRegistered.find(key) == sRegistered.end())) {
        const ImVec4 color(tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, 1.0f);
        const ImVec4 faded(color.x, color.y, color.z, 0.3f);
        gui->LoadGuiTexture(key, path, "", color);
        gui->LoadGuiTexture(key + "_Faded", path, "", faded);
        sRegistered[key] = key;
    }
    static std::string sName;
    sName = owned ? key : (key + "_Faded");
    return sName.c_str();
}

} // namespace

bool MergeShip_ItemTracker_BothWorlds() {
    return MergeShip_IsMajorasMaskMounted() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ItemTrackerBothWorlds"), 1);
}

void MergeShip_ItemTracker_DrawTerminaColumn(float originX, float originY) {
    static const Color_RGB8 kWhite = { 255, 255, 255 };
    // (ITEM_NONE: an empty cell, so that each group starts a row)
    static const TerminaIcon kIcons[] = {
        { ITEM_MM_SWORD_KOKIRI, "Kokiri Sword (MM)", kWhite },
        { ITEM_MM_SWORD_RAZOR, "Razor Sword", kWhite },
        { ITEM_MM_SWORD_GILDED, "Gilded Sword", kWhite },
        { ITEM_MM_SWORD_GREAT_FAIRY, "Great Fairy's Sword", kWhite },
        { ITEM_MM_SHIELD_HERO, "Hero's Shield", kWhite },
        { ITEM_MM_SHIELD_MIRROR, "Mirror Shield (MM)", kWhite },
        { ITEM_MM_BOW_HERO, "Hero's Bow", kWhite },
        { ITEM_MM_HOOKSHOT, "Hookshot (MM)", kWhite },
        { ITEM_MM_POWDER_KEG, "Powder Keg", kWhite },
        { ITEM_MM_PICTOGRAPH_BOX, "Pictograph Box", kWhite },
        { ITEM_MM_BOMBERS_NOTEBOOK, "Bombers' Notebook", kWhite },
        { ITEM_MM_MOONS_TEAR, "Moon's Tear", kWhite },
        { ITEM_MM_DEED_LAND, "Land Title Deed", kWhite },
        { ITEM_MM_DEED_SWAMP, "Swamp Title Deed", kWhite },
        { ITEM_MM_DEED_MOUNTAIN, "Mountain Title Deed", kWhite },
        { ITEM_MM_DEED_OCEAN, "Ocean Title Deed", kWhite },
        { ITEM_MM_ROOM_KEY, "Room Key", kWhite },
        { ITEM_MM_LETTER_TO_KAFEI, "Letter to Kafei", kWhite },
        { ITEM_MM_LETTER_TO_MAMA, "Express Mail to Mama", kWhite },
        { ITEM_MM_PENDANT_OF_MEMORIES, "Pendant of Memories", kWhite },
        { ITEM_MM_REMAINS_ODOLWA, "Odolwa's Remains", kWhite },
        { ITEM_MM_REMAINS_GOHT, "Goht's Remains", kWhite },
        { ITEM_MM_REMAINS_GYORG, "Gyorg's Remains", kWhite },
        { ITEM_MM_REMAINS_TWINMOLD, "Twinmold's Remains", kWhite },
        { ITEM_MM_SONG_SONATA, "Sonata of Awakening", { 150, 255, 100 } },
        { ITEM_MM_SONG_GORON_LULLABY, "Goron's Lullaby", { 255, 80, 40 } },
        { ITEM_MM_SONG_NEW_WAVE, "New Wave Bossa Nova", { 100, 150, 255 } },
        { ITEM_MM_SONG_ELEGY, "Elegy of Emptiness", { 255, 160, 0 } },
        { ITEM_MM_SONG_OATH, "Oath to Order", { 255, 100, 255 } },
        { ITEM_MM_SONG_HEALING, "Song of Healing", kWhite },
        { ITEM_MM_SONG_SOARING, "Song of Soaring", kWhite },
        { ITEM_NONE, "", kWhite },
        { ITEM_NONE, "", kWhite },
        { ITEM_NONE, "", kWhite },
        { ITEM_NONE, "", kWhite },
        { ITEM_NONE, "", kWhite },
        { ITEM_MM_MASK_DEKU, "Deku Mask", kWhite },
        { ITEM_MM_MASK_GORON, "Goron Mask (MM)", kWhite },
        { ITEM_MM_MASK_ZORA, "Zora Mask (MM)", kWhite },
        { ITEM_MM_MASK_FIERCE_DEITY, "Fierce Deity's Mask", kWhite },
        { ITEM_MM_MASK_TRUTH, "Mask of Truth (MM)", kWhite },
        { ITEM_MM_MASK_KAFEIS_MASK, "Kafei's Mask", kWhite },
        { ITEM_MM_MASK_ALL_NIGHT, "All-Night Mask", kWhite },
        { ITEM_MM_MASK_BUNNY, "Bunny Hood (MM)", kWhite },
        { ITEM_MM_MASK_KEATON, "Keaton Mask (MM)", kWhite },
        { ITEM_MM_MASK_GARO, "Garo's Mask", kWhite },
        { ITEM_MM_MASK_ROMANI, "Romani's Mask", kWhite },
        { ITEM_MM_MASK_CIRCUS_LEADER, "Circus Leader's Mask", kWhite },
        { ITEM_MM_MASK_POSTMAN, "Postman's Hat", kWhite },
        { ITEM_MM_MASK_COUPLE, "Couple's Mask", kWhite },
        { ITEM_MM_MASK_GREAT_FAIRY, "Great Fairy's Mask", kWhite },
        { ITEM_MM_MASK_GIBDO, "Gibdo Mask", kWhite },
        { ITEM_MM_MASK_DON_GERO, "Don Gero's Mask", kWhite },
        { ITEM_MM_MASK_KAMARO, "Kamaro's Mask", kWhite },
        { ITEM_MM_MASK_CAPTAIN, "Captain's Hat", kWhite },
        { ITEM_MM_MASK_STONE, "Stone Mask", kWhite },
        { ITEM_MM_MASK_BREMEN, "Bremen Mask", kWhite },
        { ITEM_MM_MASK_BLAST, "Blast Mask", kWhite },
        { ITEM_MM_MASK_SCENTS, "Mask of Scents", kWhite },
        { ITEM_MM_MASK_GIANT, "Giant's Mask", kWhite },
    };
    const float iconSize = static_cast<float>(CVarGetInteger(CVAR_TRACKER_ITEM("IconSize"), 36));
    const int iconSpacing = CVarGetInteger(CVAR_TRACKER_ITEM("IconSpacing"), 12);
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());

    if (gui == nullptr) {
        return;
    }
    for (size_t i = 0; i < std::size(kIcons); i++) {
        const TerminaIcon& icon = kIcons[i];
        const char* path = (icon.item != ITEM_NONE) ? (const char*)gItemIcons[icon.item] : nullptr;
        if ((path == nullptr) || (path[0] == '\0')) {
            continue;
        }
        ImGui::SetCursorPos(ImVec2(originX + (i % 6) * (iconSize + iconSpacing), originY + (i / 6) * (iconSize + iconSpacing)));
        const bool owned = TerminaOwns(icon.item);
        ImGui::Image(gui->GetTextureByName(TerminaTexture(path, owned, icon.tint)), ImVec2(iconSize, iconSize));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", icon.name);
        }
    }
}

const MsWorldCatalog* MergeShip_Catalog_Hyrule() {
    auto ctx = Rando::Context::GetInstance();
    if (!MergeShipUnified_Active() || (ctx == nullptr)) {
        return nullptr;
    }
    // Rebuilt for every trip: the check tracker's options (shops, tokens ...) decide which checks are shown. 2Ship
    // copies what it needs on arrival, so the previous one may go.
    BuildCatalog();
    return &sCatalog.catalog;
}

bool MergeShip_Tracker_TestCommand(const std::string& op, const std::vector<std::string>& args) {
    if (op == "trackterm") {
        // trackterm: the check tracker's Termina section as it would draw now (from the ledger).
        uint16_t gotten = 0;
        uint16_t total = 0;
        const bool active = MergeShip_CheckTracker_TerminaTotals(&gotten, &total);
        Log("TRACKTERM active=%d found=%u total=%u ledger=%u", active ? 1 : 0, (unsigned)gotten, (unsigned)total,
            sLedger.count);
        if (!active) {
            return true;
        }
        for (const TerminaTrackerArea& area : TrackerAreas()) {
            size_t found = 0;
            for (RandomizerCheck rc : area.checks) {
                if (TerminaFound(rc)) {
                    found++;
                    Log("TRACKTERM FOUND %s (%s) area=\"%s\" item=%s", std::string(EnumToString(rc)).c_str(),
                        Rando::StaticData::GetLocation(rc)->GetShortName().c_str(),
                        MergeShip_Hints_TerminaAreaName(area.area), KeyLabel(sLedger.terminaItem[TerminaIndex(rc)]));
                }
            }
            Log("TRACKTERM AREA \"%s\" %zu/%zu", MergeShip_Hints_TerminaAreaName(area.area), found, area.checks.size());
        }
        return true;
    }
    if (op == "trackitems") {
        // trackitems: the item tracker's Termina window, one line per row ("*" marks what is owned).
        Log("TRACKITEMS active=%d", MergeShip_ItemTracker_ShowTermina() ? 1 : 0);
        for (const Row& row : TerminaItemRows()) {
            std::string line;
            for (const Cell& cell : row.cells) {
                line += (line.empty() ? "" : ", ") + cell.text + (cell.owned ? "*" : "");
            }
            Log("TRACKITEMS %s: %s", row.label.c_str(), line.c_str());
        }
        return true;
    }
    if (op == "catalog") {
        // catalog: the Hyrule catalog 2Ship gets with the next trip.
        const MsWorldCatalog* catalog = MergeShip_Catalog_Hyrule();
        if (catalog == nullptr) {
            Log("CATALOG none (not a unified seed)");
            return true;
        }
        size_t major = 0;
        size_t tracked = 0;
        for (uint32_t i = 0; i < catalog->checkCount; i++) {
            major += (catalog->checks[i].flags & MS_CATALOG_MAJOR) ? 1 : 0;
            tracked += (catalog->checks[i].flags & MS_CATALOG_TRACKED) ? 1 : 0;
        }
        Log("CATALOG seed=%u world=%d areas=%u checks=%u major=%zu tracked=%zu", catalog->seed, catalog->world,
            catalog->areaCount, catalog->checkCount, major, tracked);
        for (uint32_t i = 0; i < catalog->checkCount && i < 3; i++) {
            const MsCatalogCheck& c = catalog->checks[i];
            Log("CATALOG #%u check=%u (%s) area=\"%s\" item=%s flags=0x%02X", i, c.check, c.name,
                catalog->areaNames[c.area], KeyLabel(c.item), c.flags);
        }
        // catalog <key...>: where the seed placed each item (MsItems.def key), in either world, for the check script
        // to compare with what 2Ship's hints say (hyrulehint).
        auto ctx = Rando::Context::GetInstance();
        for (const std::string& keyName : args) {
            bool any = false;
            for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
                Rando::ItemLocation* loc = ctx->GetItemLocation((RandomizerCheck)rc);
                const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(loc->GetPlacedRandomizerGet()));
                if ((loc->GetPlacedRandomizerGet() == RG_NONE) || (info == nullptr) || (keyName != info->keyName)) {
                    continue;
                }
                const bool termina = MergeShip_Termina_IsTerminaCheck((RandomizerCheck)rc);
                const Rando::Location* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)rc);
                std::string area = termina ? MergeShip_Hints_TerminaAreaName(
                                                 MergeShip_Hints_TerminaCheckArea((RandomizerCheck)rc))
                                           : RandomizerCheckObjects::GetRCAreaName(staticLoc->GetArea());
                bool inCatalog = false;
                for (uint32_t i = 0; i < catalog->checkCount; i++) {
                    inCatalog = inCatalog || (catalog->checks[i].check == (uint16_t)rc);
                }
                Log("CATALOG ITEM %s world=%s check=%s name=\"%s\" area=\"%s\" inCatalog=%d", keyName.c_str(),
                    termina ? "termina" : "hyrule", std::string(EnumToString((RandomizerCheck)rc)).c_str(),
                    staticLoc->GetShortName().c_str(), area.c_str(), inCatalog ? 1 : 0);
                any = true;
            }
            if (!any) {
                Log("CATALOG ITEM %s world=none (not placed)", keyName.c_str());
            }
        }
        return true;
    }
    return false;
}
