// MergeShip2 -- hints over both worlds (MergeShipUnifiedHints.h, the unified randomizer M6).

#include "soh/MergeShip/MergeShipUnifiedHints.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/hint.h"
#include "soh/Enhancements/randomizer/3drando/hints.hpp"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipMaskMessages.h"

extern "C" void MergeShipTest_Log(const char* fmt, ...);

namespace {

// ---- Termina's hint areas ------------------------------------------------------------------------------------------

struct TerminaArea {
    RandomizerArea area;
    RandomizerHintTextKey text;
    const char* english; // also the tracker's name and the save's key (hints are saved by area name)
    const char* german;
    const char* french;
    bool dungeon;
};

// clang-format off
const TerminaArea kAreas[] = {
    { RA_MM_CLOCK_TOWN,              RHT_MM_AREA_CLOCK_TOWN,              "Clock Town",                 "in Unruh-Stadt",                  "le Bourg-Clocher",               false },
    { RA_MM_TERMINA_FIELD,           RHT_MM_AREA_TERMINA_FIELD,           "Termina Field",              "in der Ebene von Termina",        "la Plaine de Termina",           false },
    { RA_MM_ROAD_TO_SOUTHERN_SWAMP,  RHT_MM_AREA_ROAD_TO_SOUTHERN_SWAMP,  "the Road to Southern Swamp", "auf dem Weg zum Sumpf",           "la Route du Marais",             false },
    { RA_MM_SOUTHERN_SWAMP,          RHT_MM_AREA_SOUTHERN_SWAMP,          "Southern Swamp",             "im Südlichen Sumpf",              "le Marais du Sud",               false },
    { RA_MM_DEKU_PALACE,             RHT_MM_AREA_DEKU_PALACE,             "Deku Palace",                "im Deku-Palast",                  "le Palais Mojo",                 false },
    { RA_MM_WOODFALL,                RHT_MM_AREA_WOODFALL,                "Woodfall",                   "im Dämmerwald",                   "le Bois-Cascade",                false },
    { RA_MM_MILK_ROAD,               RHT_MM_AREA_MILK_ROAD,               "Milk Road",                  "auf der Milchstraße",             "la Route Laitière",              false },
    { RA_MM_ROMANI_RANCH,            RHT_MM_AREA_ROMANI_RANCH,            "Romani Ranch",               "auf der Romani-Ranch",            "le Ranch Romani",                false },
    { RA_MM_MOUNTAIN_VILLAGE,        RHT_MM_AREA_MOUNTAIN_VILLAGE,        "Mountain Village",           "im Bergdorf",                     "le Village des Montagnes",       false },
    { RA_MM_GORON_VILLAGE,           RHT_MM_AREA_GORON_VILLAGE,           "Goron Village",              "im Goronen-Dorf",                 "le Village Goron",               false },
    { RA_MM_SNOWHEAD,                RHT_MM_AREA_SNOWHEAD,                "Snowhead",                   "am Pic Hibernia",                 "le Pic des Neiges",              false },
    { RA_MM_GREAT_BAY_COAST,         RHT_MM_AREA_GREAT_BAY_COAST,         "Great Bay Coast",            "an der Küste der Großen Bucht",   "la Côte de la Grande Baie",      false },
    { RA_MM_ZORA_CAPE,               RHT_MM_AREA_ZORA_CAPE,               "Zora Cape",                  "am Zora-Kap",                     "le Cap Zora",                    false },
    { RA_MM_ZORA_HALL,               RHT_MM_AREA_ZORA_HALL,               "Zora Hall",                  "in der Zora-Halle",               "la Salle Zora",                  false },
    { RA_MM_PIRATES_FORTRESS,        RHT_MM_AREA_PIRATES_FORTRESS,        "Pirates' Fortress",          "in der Piratenfestung",           "la Forteresse des Pirates",      false },
    { RA_MM_ROAD_TO_IKANA,           RHT_MM_AREA_ROAD_TO_IKANA,           "the Road to Ikana",          "auf dem Weg nach Ikana",          "la Route d'Ikana",               false },
    { RA_MM_IKANA_GRAVEYARD,         RHT_MM_AREA_IKANA_GRAVEYARD,         "Ikana Graveyard",            "auf dem Friedhof von Ikana",      "le Cimetière d'Ikana",           false },
    { RA_MM_IKANA_CANYON,            RHT_MM_AREA_IKANA_CANYON,            "Ikana Canyon",               "im Ikana-Canyon",                 "le Canyon Ikana",                false },
    { RA_MM_STONE_TOWER,             RHT_MM_AREA_STONE_TOWER,             "Stone Tower",                "am Felsenturm",                   "la Tour de Pierre",              false },
    { RA_MM_THE_MOON,                RHT_MM_AREA_THE_MOON,                "the Moon",                   "auf dem Mond",                    "la Lune",                        false },
    { RA_MM_WOODFALL_TEMPLE,         RHT_MM_AREA_WOODFALL_TEMPLE,         "Woodfall Temple",            "im Dämmerwald-Tempel",            "le Temple du Bois-Cascade",      true  },
    { RA_MM_SNOWHEAD_TEMPLE,         RHT_MM_AREA_SNOWHEAD_TEMPLE,         "Snowhead Temple",            "im Pic-Hibernia-Tempel",          "le Temple du Pic des Neiges",    true  },
    { RA_MM_GREAT_BAY_TEMPLE,        RHT_MM_AREA_GREAT_BAY_TEMPLE,        "Great Bay Temple",           "im Tempel der Großen Bucht",      "le Temple de la Grande Baie",    true  },
    { RA_MM_STONE_TOWER_TEMPLE,      RHT_MM_AREA_STONE_TOWER_TEMPLE,      "Stone Tower Temple",         "im Felsenturm-Tempel",            "le Temple de la Tour de Pierre", true  },
    { RA_MM_BENEATH_THE_WELL,        RHT_MM_AREA_BENEATH_THE_WELL,        "Beneath the Well",           "unter dem Brunnen",               "le Dessous du Puits",            true  },
    { RA_MM_ANCIENT_CASTLE_OF_IKANA, RHT_MM_AREA_ANCIENT_CASTLE_OF_IKANA, "the Ancient Castle of Ikana", "im Schloss Ikana",               "le Château d'Ikana",             true  },
};

// 2Ship's scenes (SceneId names, as MergeShipTerminaData.cpp has them) and the area each belongs to. Grottos
// (SCENE_KAKUSIANA), Great Fairy fountains (SCENE_YOUSEI_IZUMI) and region-less rows (SCENE_MAX) go by their region's
// name instead (kRegionPrefixes).
const std::pair<const char*, RandomizerArea> kScenes[] = {
    // Clock Town, its houses and shops, the Clock Tower, the Astral Observatory (under East Clock Town)
    { "SCENE_CLOCKTOWER", RA_MM_CLOCK_TOWN }, { "SCENE_TOWN", RA_MM_CLOCK_TOWN }, { "SCENE_BACKTOWN", RA_MM_CLOCK_TOWN },
    { "SCENE_ICHIBA", RA_MM_CLOCK_TOWN }, { "SCENE_ALLEY", RA_MM_CLOCK_TOWN }, { "SCENE_OKUJOU", RA_MM_CLOCK_TOWN },
    { "SCENE_INSIDETOWER", RA_MM_CLOCK_TOWN }, { "SCENE_YADOYA", RA_MM_CLOCK_TOWN }, { "SCENE_MILK_BAR", RA_MM_CLOCK_TOWN },
    { "SCENE_TAKARAYA", RA_MM_CLOCK_TOWN }, { "SCENE_BOWLING", RA_MM_CLOCK_TOWN }, { "SCENE_SONCHONOIE", RA_MM_CLOCK_TOWN },
    { "SCENE_SYATEKI_MIZU", RA_MM_CLOCK_TOWN }, { "SCENE_DOUJOU", RA_MM_CLOCK_TOWN }, { "SCENE_POSTHOUSE", RA_MM_CLOCK_TOWN },
    { "SCENE_AYASHIISHOP", RA_MM_CLOCK_TOWN }, { "SCENE_8ITEMSHOP", RA_MM_CLOCK_TOWN }, { "SCENE_BOMYA", RA_MM_CLOCK_TOWN },
    { "SCENE_TAKARAKUJI", RA_MM_CLOCK_TOWN }, { "SCENE_DEKUTES", RA_MM_CLOCK_TOWN }, { "SCENE_TENMON_DAI", RA_MM_CLOCK_TOWN },
    { "SCENE_00KEIKOKU", RA_MM_TERMINA_FIELD },
    { "SCENE_24KEMONOMITI", RA_MM_ROAD_TO_SOUTHERN_SWAMP }, { "SCENE_SYATEKI_MORI", RA_MM_ROAD_TO_SOUTHERN_SWAMP },
    { "SCENE_20SICHITAI", RA_MM_SOUTHERN_SWAMP }, { "SCENE_20SICHITAI2", RA_MM_SOUTHERN_SWAMP },
    { "SCENE_MAP_SHOP", RA_MM_SOUTHERN_SWAMP }, { "SCENE_WITCH_SHOP", RA_MM_SOUTHERN_SWAMP },
    { "SCENE_26SARUNOMORI", RA_MM_SOUTHERN_SWAMP }, { "SCENE_KINSTA1", RA_MM_SOUTHERN_SWAMP },
    { "SCENE_22DEKUCITY", RA_MM_DEKU_PALACE }, { "SCENE_DEKU_KING", RA_MM_DEKU_PALACE }, { "SCENE_DANPEI", RA_MM_DEKU_PALACE },
    { "SCENE_21MITURINMAE", RA_MM_WOODFALL },
    { "SCENE_MITURIN", RA_MM_WOODFALL_TEMPLE }, { "SCENE_MITURIN_BS", RA_MM_WOODFALL_TEMPLE },
    { "SCENE_ROMANYMAE", RA_MM_MILK_ROAD }, { "SCENE_KOEPONARACE", RA_MM_MILK_ROAD },
    { "SCENE_F01", RA_MM_ROMANI_RANCH }, { "SCENE_F01C", RA_MM_ROMANI_RANCH }, { "SCENE_F01_B", RA_MM_ROMANI_RANCH },
    { "SCENE_OMOYA", RA_MM_ROMANI_RANCH },
    { "SCENE_10YUKIYAMANOMURA", RA_MM_MOUNTAIN_VILLAGE }, { "SCENE_10YUKIYAMANOMURA2", RA_MM_MOUNTAIN_VILLAGE },
    { "SCENE_13HUBUKINOMITI", RA_MM_MOUNTAIN_VILLAGE }, { "SCENE_KAJIYA", RA_MM_MOUNTAIN_VILLAGE },
    { "SCENE_GORON_HAKA", RA_MM_MOUNTAIN_VILLAGE }, { "SCENE_GORONRACE", RA_MM_MOUNTAIN_VILLAGE },
    { "SCENE_11GORONNOSATO", RA_MM_GORON_VILLAGE }, { "SCENE_11GORONNOSATO2", RA_MM_GORON_VILLAGE },
    { "SCENE_16GORON_HOUSE", RA_MM_GORON_VILLAGE }, { "SCENE_GORONSHOP", RA_MM_GORON_VILLAGE },
    { "SCENE_17SETUGEN", RA_MM_GORON_VILLAGE }, { "SCENE_17SETUGEN2", RA_MM_GORON_VILLAGE },
    { "SCENE_12HAKUGINMAE", RA_MM_SNOWHEAD }, { "SCENE_14YUKIDAMANOMITI", RA_MM_SNOWHEAD },
    { "SCENE_HAKUGIN", RA_MM_SNOWHEAD_TEMPLE }, { "SCENE_HAKUGIN_BS", RA_MM_SNOWHEAD_TEMPLE },
    { "SCENE_30GYOSON", RA_MM_GREAT_BAY_COAST }, { "SCENE_LABO", RA_MM_GREAT_BAY_COAST },
    { "SCENE_FISHERMAN", RA_MM_GREAT_BAY_COAST }, { "SCENE_SINKAI", RA_MM_GREAT_BAY_COAST },
    { "SCENE_KINDAN2", RA_MM_GREAT_BAY_COAST },
    { "SCENE_31MISAKI", RA_MM_ZORA_CAPE }, { "SCENE_35TAKI", RA_MM_ZORA_CAPE },
    { "SCENE_33ZORACITY", RA_MM_ZORA_HALL }, { "SCENE_BANDROOM", RA_MM_ZORA_HALL },
    { "SCENE_TORIDE", RA_MM_PIRATES_FORTRESS }, { "SCENE_KAIZOKU", RA_MM_PIRATES_FORTRESS },
    { "SCENE_PIRATE", RA_MM_PIRATES_FORTRESS },
    { "SCENE_SEA", RA_MM_GREAT_BAY_TEMPLE }, { "SCENE_SEA_BS", RA_MM_GREAT_BAY_TEMPLE },
    { "SCENE_IKANAMAE", RA_MM_ROAD_TO_IKANA },
    { "SCENE_BOTI", RA_MM_IKANA_GRAVEYARD }, { "SCENE_HAKASHITA", RA_MM_IKANA_GRAVEYARD },
    { "SCENE_DANPEI2TEST", RA_MM_IKANA_GRAVEYARD },
    { "SCENE_IKANA", RA_MM_IKANA_CANYON }, { "SCENE_MUSICHOUSE", RA_MM_IKANA_CANYON }, { "SCENE_SECOM", RA_MM_IKANA_CANYON },
    { "SCENE_TOUGITES", RA_MM_IKANA_CANYON }, { "SCENE_RANDOM", RA_MM_IKANA_CANYON },
    { "SCENE_F40", RA_MM_STONE_TOWER }, { "SCENE_F41", RA_MM_STONE_TOWER },
    { "SCENE_INISIE_N", RA_MM_STONE_TOWER_TEMPLE }, { "SCENE_INISIE_R", RA_MM_STONE_TOWER_TEMPLE },
    { "SCENE_INISIE_BS", RA_MM_STONE_TOWER_TEMPLE },
    { "SCENE_REDEAD", RA_MM_BENEATH_THE_WELL },
    { "SCENE_CASTLE", RA_MM_ANCIENT_CASTLE_OF_IKANA }, { "SCENE_IKNINSIDE", RA_MM_ANCIENT_CASTLE_OF_IKANA },
    { "SCENE_SOUGEN", RA_MM_THE_MOON }, { "SCENE_LAST_DEKU", RA_MM_THE_MOON }, { "SCENE_LAST_GORON", RA_MM_THE_MOON },
    { "SCENE_LAST_ZORA", RA_MM_THE_MOON }, { "SCENE_LAST_LINK", RA_MM_THE_MOON }, { "SCENE_LAST_BS", RA_MM_THE_MOON },
};

// 2Ship region names (RandoRegionId) for the regions whose scene is shared by several areas. Longest prefix first
// where two could match.
const std::pair<const char*, RandomizerArea> kRegionPrefixes[] = {
    { "RR_MAX", RA_LINKS_POCKET }, // 2Ship's pseudo-root: Termina's starting items
    { "RR_STONE_TOWER_TEMPLE", RA_MM_STONE_TOWER_TEMPLE },
    { "RR_TERMINA_FIELD_", RA_MM_TERMINA_FIELD },
    { "RR_DEKU_PALACE_", RA_MM_DEKU_PALACE },
    { "RR_GREAT_BAY_COAST_", RA_MM_GREAT_BAY_COAST },
    { "RR_GREAT_BAY_GREAT_FAIRY", RA_MM_ZORA_CAPE },
    { "RR_IKANA_CANYON_", RA_MM_IKANA_CANYON },
    { "RR_IKANA_GRAVEYARD_", RA_MM_IKANA_GRAVEYARD },
    { "RR_IKANA_GREAT_FAIRY", RA_MM_IKANA_CANYON },
    { "RR_LONE_PEAK_SHRINE", RA_MM_GORON_VILLAGE },
    { "RR_MOUNTAIN_VILLAGE_", RA_MM_MOUNTAIN_VILLAGE },
    { "RR_PATH_TO_GORON_VILLAGE_", RA_MM_GORON_VILLAGE },
    { "RR_PATH_TO_SNOWHEAD_", RA_MM_SNOWHEAD },
    { "RR_ROAD_TO_IKANA_", RA_MM_ROAD_TO_IKANA },
    { "RR_ROAD_TO_SOUTHERN_SWAMP_", RA_MM_ROAD_TO_SOUTHERN_SWAMP },
    { "RR_SOUTHERN_SWAMP_", RA_MM_SOUTHERN_SWAMP },
    { "RR_WOODS_OF_MYSTERY_", RA_MM_SOUTHERN_SWAMP },
    { "RR_ZORA_CAPE_", RA_MM_ZORA_CAPE },
    { "RR_CLOCK_TOWN_", RA_MM_CLOCK_TOWN },
    { "RR_WOODFALL_GREAT_FAIRY", RA_MM_WOODFALL },
    { "RR_SNOWHEAD_GREAT_FAIRY", RA_MM_SNOWHEAD },
};
// clang-format on

const TerminaArea* FindArea(RandomizerArea area) {
    for (const TerminaArea& a : kAreas) {
        if (a.area == area) {
            return &a;
        }
    }
    return nullptr;
}

RandomizerArea SceneArea(const char* scene) {
    if (scene == nullptr) {
        return RA_NONE;
    }
    for (const auto& [name, area] : kScenes) {
        if (strcmp(name, scene) == 0) {
            return area;
        }
    }
    return RA_NONE;
}

RandomizerArea RegionNameArea(const char* region) {
    if (region == nullptr) {
        return RA_NONE;
    }
    for (const auto& [prefix, area] : kRegionPrefixes) {
        if (strncmp(region, prefix, strlen(prefix)) == 0) {
            return area;
        }
    }
    return RA_NONE;
}

const MsTerminaRegion* FindRegion(RandomizerRegion rr) {
    static std::unordered_map<int, size_t> index;
    if (index.empty()) {
        for (size_t i = 0; i < gMsTerminaRegionCount; i++) {
            index[(int)gMsTerminaRegions[i].rr] = i;
        }
    }
    auto it = index.find((int)rr);
    return (it != index.end()) ? &gMsTerminaRegions[it->second] : nullptr;
}

bool IsGenericScene(const char* scene) {
    return (scene == nullptr) || (strcmp(scene, "SCENE_KAKUSIANA") == 0) ||
           (strcmp(scene, "SCENE_YOUSEI_IZUMI") == 0) || (strcmp(scene, "SCENE_MAX") == 0);
}

RandomizerArea AreaOfRegionRow(const MsTerminaRegion& region) {
    if (!IsGenericScene(region.mmScene)) {
        const RandomizerArea area = SceneArea(region.mmScene);
        if (area != RA_NONE) {
            return area;
        }
    }
    return RegionNameArea(region.mmRegion);
}

// ---- Termina's generated items: named by their item name --------------------------------------------------------

#pragma push_macro("RANDO_ENUM_ITEM")
#undef RANDO_ENUM_ITEM
#define RANDO_ENUM_ITEM(x) x,
constexpr RandomizerGet kTerminaOnly[] = {
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerGetTermina.inc"
};
#undef RANDO_ENUM_ITEM
#pragma pop_macro("RANDO_ENUM_ITEM")

bool IsTerminaOnly(RandomizerGet rg) {
    return std::find(std::begin(kTerminaOnly), std::end(kTerminaOnly), rg) != std::end(kTerminaOnly);
}

// ---- Mask of Truth's Termina hints ---------------------------------------------------------------------------------

// Termina's locations holding a major item, in RandomizerCheck order: the same list for the same seed, so a stone
// always gives the same extra hint.
const std::vector<RandomizerCheck>& TruthCandidates() {
    static std::vector<RandomizerCheck> candidates;
    static uint32_t candidatesSeed = 0;
    static bool built = false;
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        candidates.clear();
        return candidates;
    }
    if (built && candidatesSeed == ctx->GetSeed()) {
        return candidates;
    }
    built = true;
    candidatesSeed = ctx->GetSeed();
    candidates.clear();
    for (size_t i = 0; i < gMsTerminaCheckCount; i++) {
        const RandomizerCheck rc = gMsTerminaChecks[i].rc;
        Rando::ItemLocation* loc = ctx->GetItemLocation(rc);
        if ((loc == nullptr) || (loc->GetPlacedRandomizerGet() == RG_NONE)) {
            continue;
        }
        const RandomizerArea area = MergeShip_Hints_TerminaCheckArea(rc);
        if (!MergeShip_Hints_IsTerminaArea(area) || !loc->GetPlacedItem().IsMajorItem()) {
            continue;
        }
        candidates.push_back(rc);
    }
    return candidates;
}

// ---- harness ------------------------------------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    char line[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip hints: {}", line);
    MergeShipTest_Log("%s", line);
}

std::string AreaLabel(RandomizerArea area) {
    const TerminaArea* a = FindArea(area);
    if (a != nullptr) {
        return a->english;
    }
    auto it = Rando::StaticData::areaNames.find(area);
    if (it == Rando::StaticData::areaNames.end()) {
        return std::string(EnumToString(area));
    }
    return Rando::StaticData::hintTextTable[it->second].GetClear().GetEnglish(MF_CLEAN);
}

// "termina", "hyrule" or "both": where a hint's areas are.
const char* HintWorld(const std::vector<RandomizerArea>& areas) {
    bool termina = false;
    bool hyrule = false;
    for (RandomizerArea area : areas) {
        if (MergeShip_Hints_IsTerminaArea(area)) {
            termina = true;
        } else if (area > RA_LINKS_POCKET) {
            hyrule = true;
        }
    }
    return (termina && hyrule) ? "both" : termina ? "termina" : hyrule ? "hyrule" : "none";
}

std::string Join(const std::set<std::string>& names) {
    std::string out;
    for (const std::string& n : names) {
        out += (out.empty() ? "" : "|") + n;
    }
    return out;
}

} // namespace

bool MergeShip_Hints_TerminaHintable() {
    auto ctx = Rando::Context::GetInstance();
    return MergeShip_Termina_Enabled() && (ctx != nullptr) && ctx->GetOption(RSK_MS_TERMINA_HINTS).Is(RO_GENERIC_ON);
}

const std::string* MergeShip_Hints_SettingText(RandomizerSettingKey rsk, bool description) {
    static const std::string kName = "Mergeship: Hints Cover Termina";
    static const std::string kDesc =
        "Unified randomizer: Hyrule's Gossip Stones may hint Termina's checks and areas (Clock Town, Woodfall Temple, "
        "the Moon ...), and Way of the Hero and foolish hints weigh both worlds' areas alike. Termina's temples, "
        "Beneath the Well and the Ancient Castle of Ikana count as dungeons.\n\n"
        "Off, Hyrule's hints only name Hyrule's checks and areas (the items they name may still be Termina's).";
    if (rsk != RSK_MS_TERMINA_HINTS) {
        return nullptr;
    }
    return description ? &kDesc : &kName;
}

void MergeShip_Hints_InitTextTable() {
    for (const TerminaArea& a : kAreas) {
        Rando::StaticData::hintTextTable[a.text] = HintText(CustomMessage(a.english, a.german, a.french));
        Rando::StaticData::areaNames[a.area] = a.text;
    }
}

void MergeShip_Hints_AssignTerminaAreas() {
    if (!MergeShip_Hints_TerminaHintable()) {
        return; // Termina's regions keep RA_NONE: never hinted, never Way of the Hero or foolish (as before M6)
    }
    size_t assigned = 0;
    size_t propagated = 0;
    for (size_t i = 0; i < gMsTerminaRegionCount; i++) {
        const MsTerminaRegion& region = gMsTerminaRegions[i];
        const RandomizerArea area = AreaOfRegionRow(region);
        if (area == RA_NONE) {
            areaTable[region.rr].ReplaceAreas({}); // fill.cpp SetAreas takes it from the regions leading there
            propagated++;
        } else {
            areaTable[region.rr].ReplaceAreas({ area });
            assigned++;
        }
    }
    SPDLOG_INFO("MergeShip hints: Termina's regions got hint areas ({} by scene or name, {} from their neighbours)",
                assigned, propagated);
}

RandomizerArea MergeShip_Hints_TerminaRegionArea(RandomizerRegion rr) {
    const MsTerminaRegion* region = FindRegion(rr);
    return (region != nullptr) ? AreaOfRegionRow(*region) : RA_NONE;
}

RandomizerArea MergeShip_Hints_TerminaCheckArea(RandomizerCheck rc) {
    const MsTerminaCheck* check = MergeShip_Termina_GetCheck(rc);
    if (check == nullptr) {
        return RA_NONE;
    }
    const RandomizerArea regionArea = MergeShip_Hints_TerminaRegionArea(check->region);
    if (regionArea != RA_NONE) {
        return regionArea;
    }
    return IsGenericScene(check->mmScene) ? RA_NONE : SceneArea(check->mmScene);
}

bool MergeShip_Hints_IsTerminaArea(RandomizerArea area) {
    return FindArea(area) != nullptr;
}

bool MergeShip_Hints_IsTerminaDungeonArea(RandomizerArea area) {
    const TerminaArea* a = FindArea(area);
    return (a != nullptr) && a->dungeon;
}

const char* MergeShip_Hints_TerminaAreaName(RandomizerArea area) {
    const TerminaArea* a = FindArea(area);
    return (a != nullptr) ? a->english : "";
}

const std::vector<RandomizerArea>& MergeShip_Hints_TerminaAreas() {
    static std::vector<RandomizerArea> areas;
    if (areas.empty()) {
        for (const TerminaArea& a : kAreas) {
            areas.push_back(a.area);
        }
    }
    return areas;
}

bool MergeShip_Hints_IsDungeonLocation(RandomizerCheck rc) {
    const Rando::Location* loc = Rando::StaticData::GetLocation(rc);
    if (loc->GetWorld() != RW_TERMINA) {
        return loc->IsDungeon();
    }
    return MergeShip_Hints_IsTerminaDungeonArea(MergeShip_Hints_TerminaCheckArea(rc));
}

HintText MergeShip_Hints_TerminaLocationText(RandomizerCheck rc) {
    const std::string name = Rando::StaticData::GetLocation(rc)->GetShortName();
    return HintText(CustomMessage("They say that #" + name + "# in Termina holds #[[1]]#.",
                                  /*german*/ "Man erzählt sich, daß #" + name + "# in Termina #[[1]]# berge.",
                                  /*french*/ "Selon moi, #" + name + "# à Termina recèle #[[1]]#.",
                                  { QM_RED, QM_GREEN }));
}

const HintText* MergeShip_Hints_ItemNameText(RandomizerGet rg) {
    if (!IsTerminaOnly(rg)) {
        return nullptr;
    }
    static std::unordered_map<int, HintText> texts; // node-based: references stay valid as it grows
    auto it = texts.find((int)rg);
    if (it == texts.end()) {
        // The generated items' names already carry " (MM)" (item_list_termina.cpp).
        it = texts.emplace((int)rg, HintText(CustomMessage(Rando::StaticData::RetrieveItem(rg).GetName()))).first;
    }
    return &it->second;
}

size_t MergeShip_Hints_TruthTerminaCount() {
    if (!MergeShip_Termina_Enabled() || !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TruthTerminaHint"), 1)) {
        return 0;
    }
    return TruthCandidates().size();
}

CustomMessage MergeShip_Hints_TruthTerminaHint(uint32_t pick) {
    const std::vector<RandomizerCheck>& candidates = TruthCandidates();
    if (candidates.empty()) {
        return CustomMessage("");
    }
    auto ctx = Rando::Context::GetInstance();
    const RandomizerCheck rc = candidates[pick % candidates.size()];
    const RandomizerArea area = MergeShip_Hints_TerminaCheckArea(rc);
    // As an item-and-area hint (hint.cpp HINT_TYPE_ITEM_AREA): "They say that #<item># can be found at #<area>#."
    CustomMessage msg = Rando::StaticData::hintTextTable[MergeShip_Hints_IsTerminaDungeonArea(area) ? RHT_HOARDS
                                                                                                     : RHT_CAN_BE_FOUND_AT]
                            .GetHintMessage();
    const CustomMessage item = ctx->GetItemLocation(rc)->GetPlacedItem().GetHint().GetHintMessage();
    const CustomMessage place = Rando::StaticData::hintTextTable[Rando::StaticData::areaNames[area]].GetHintMessage();
    msg.InsertNames({ item, place });
    msg.SetSingularPlural();
    msg.AutoFormat();
    return msg;
}

bool MergeShip_Hints_TestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) -> std::string { return (i < args.size()) ? args[i] : std::string(); };
    auto ctx = Rando::Context::GetInstance();

    if (op == "hints") {
        // hints: after randogen (or with a unified file loaded), every hint of the seed and the world it points into,
        // then the Way of the Hero and foolish areas of both worlds.
        if (ctx == nullptr) {
            Log("HINTS no randomizer context");
            return true;
        }
        size_t total = 0;
        size_t termina = 0;
        size_t hyrule = 0;
        for (int rh = RH_NONE + 1; rh < RH_MAX; rh++) {
            Rando::Hint* hint = ctx->GetHint((RandomizerHint)rh);
            if ((hint == nullptr) || !hint->IsEnabled()) {
                continue;
            }
            std::vector<RandomizerArea> areas = hint->GetHintedAreas();
            for (RandomizerCheck rc : hint->GetHintedLocations()) {
                if (MergeShip_Termina_IsTerminaCheck(rc)) {
                    areas.push_back(MergeShip_Hints_TerminaCheckArea(rc));
                }
            }
            const char* world = HintWorld(areas);
            std::string areaNames;
            for (RandomizerArea area : hint->GetHintedAreas()) {
                areaNames += (areaNames.empty() ? "" : "|") + AreaLabel(area);
            }
            std::string checks;
            for (RandomizerCheck rc : hint->GetHintedLocations()) {
                checks += std::string(checks.empty() ? "" : "|") + std::string(EnumToString(rc));
            }
            const std::string text = hint->GetHintMessage(MF_CLEAN).GetEnglish(MF_CLEAN);
            const std::string type =
                Rando::StaticData::hintTypeNames[hint->GetHintType()].GetEnglish(MF_CLEAN);
            Log("HINT %s type=%s dist=%s world=%s areas=%s checks=%s text=\"%s\"",
                std::string(EnumToString((RandomizerHint)rh)).c_str(), type.c_str(), hint->GetDistribution().c_str(),
                world, areaNames.c_str(), checks.c_str(), text.c_str());
            total++;
            termina += (strcmp(world, "termina") == 0 || strcmp(world, "both") == 0) ? 1 : 0;
            hyrule += (strcmp(world, "hyrule") == 0 || strcmp(world, "both") == 0) ? 1 : 0;
        }
        // Way of the Hero and foolish candidates (fill.cpp), by area and world.
        std::set<std::string> wothHyrule, wothTermina, foolishHyrule, foolishTermina;
        size_t terminaHintable = 0;
        for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
            Rando::ItemLocation* loc = ctx->GetItemLocation((RandomizerCheck)rc);
            if ((loc == nullptr) || (loc->GetPlacedRandomizerGet() == RG_NONE)) {
                continue;
            }
            const bool isTermina = MergeShip_Termina_IsTerminaCheck((RandomizerCheck)rc);
            terminaHintable += (isTermina && loc->IsHintable()) ? 1 : 0;
            for (RandomizerArea area : loc->GetAreas()) {
                if (area <= RA_LINKS_POCKET) {
                    continue;
                }
                const std::string name = AreaLabel(area);
                if (loc->IsWothCandidate()) {
                    (MergeShip_Hints_IsTerminaArea(area) ? wothTermina : wothHyrule).insert(name);
                }
                if (loc->IsFoolishCandidate()) {
                    (MergeShip_Hints_IsTerminaArea(area) ? foolishTermina : foolishHyrule).insert(name);
                }
            }
        }
        Log("HINTS total=%zu hyrule=%zu termina=%zu terminaHintableLocations=%zu terminaHints=%d", total, hyrule,
            termina, terminaHintable, MergeShip_Hints_TerminaHintable() ? 1 : 0);
        Log("WOTH hyrule=%zu termina=%zu areas=%s | %s", wothHyrule.size(), wothTermina.size(), Join(wothHyrule).c_str(),
            Join(wothTermina).c_str());
        Log("FOOLISH hyrule=%zu termina=%zu areas=%s | %s", foolishHyrule.size(), foolishTermina.size(),
            Join(foolishHyrule).c_str(), Join(foolishTermina).c_str());
        return true;
    }
    if (op == "hintarea") {
        // hintarea <RC_MM_...>: a Termina check's hint area, and an item hint's text about it.
        RandomizerCheck rc = RC_UNKNOWN_CHECK;
        for (int i = RC_UNKNOWN_CHECK + 1; i < RC_MAX; i++) {
            if (EnumToString((RandomizerCheck)i) == arg(0)) {
                rc = (RandomizerCheck)i;
                break;
            }
        }
        if (!MergeShip_Termina_IsTerminaCheck(rc)) {
            Log("HINTAREA %s: not a Termina check", arg(0).c_str());
            return true;
        }
        const RandomizerArea area = MergeShip_Hints_TerminaCheckArea(rc);
        CustomMessage text = MergeShip_Hints_TerminaLocationText(rc).GetClear();
        RandomizerGet placed = (ctx != nullptr) ? ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() : RG_NONE;
        text.InsertNames({ Rando::StaticData::RetrieveItem(placed == RG_NONE ? RG_GREEN_RUPEE : placed)
                               .GetHint()
                               .GetClear() });
        Log("HINTAREA %s area=%s dungeon=%d text=\"%s\"", arg(0).c_str(), MergeShip_Hints_TerminaAreaName(area),
            MergeShip_Hints_IsTerminaDungeonArea(area) ? 1 : 0, text.GetEnglish(MF_CLEAN).c_str());
        return true;
    }
    if (op == "hintitem") {
        // hintitem <RG_...>: the name an item hint gives an item (" (MM)" for Majora's Mask's).
        for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
            if (EnumToString((RandomizerGet)rg) == arg(0)) {
                Log("HINTITEM %s \"%s\"", arg(0).c_str(),
                    Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetHint().GetClear().GetEnglish(MF_CLEAN).c_str());
                return true;
            }
        }
        Log("HINTITEM %s: unknown item", arg(0).c_str());
        return true;
    }
    if (op == "truthhint") {
        // truthhint [n]: the Mask of Truth's extra hint of the first n (default 12) Gossip Stones of the seed.
        const int n = arg(0).empty() ? 12 : atoi(arg(0).c_str());
        int shown = 0;
        int intoTermina = 0;
        for (const auto& [params, rh] : Rando::StaticData::stoneParamsToHint) {
            if (shown >= n) {
                break;
            }
            const std::string text = MergeShip_TruthExtraHintText(rh);
            bool termina = false;
            for (RandomizerArea area : MergeShip_Hints_TerminaAreas()) {
                if (text.find(MergeShip_Hints_TerminaAreaName(area)) != std::string::npos) {
                    termina = true;
                }
            }
            intoTermina += termina ? 1 : 0;
            Log("TRUTHHINT %s termina=%d \"%s\"", std::string(EnumToString(rh)).c_str(), termina ? 1 : 0,
                text.c_str());
            shown++;
        }
        Log("TRUTHHINTS stones=%d intoTermina=%d candidates=%zu", shown, intoTermina,
            MergeShip_Hints_TruthTerminaCount());
        return true;
    }
    if (op == "truthcandidates") {
        const std::vector<RandomizerCheck>& candidates = TruthCandidates();
        Log("TRUTHCANDIDATES count=%zu enabled=%zu", candidates.size(), MergeShip_Hints_TruthTerminaCount());
        for (size_t i = 0; i < candidates.size() && i < 5; i++) {
            Log("TRUTHCANDIDATE %s area=%s \"%s\"", std::string(EnumToString(candidates[i])).c_str(),
                MergeShip_Hints_TerminaAreaName(MergeShip_Hints_TerminaCheckArea(candidates[i])),
                MergeShip_Hints_TruthTerminaHint((uint32_t)i).GetEnglish(MF_CLEAN).c_str());
        }
        return true;
    }
    return false;
}
