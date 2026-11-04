// MergeShip2 -- Termina's logic inside SoH's generator (MergeShipTerminaLogic.h). A port of 2Ship's Logic/Logic.h helpers
// and Logic/TimeLogic.cpp over the unified item model; the regions themselves are generated (location_access/termina).

#include "MergeShipTerminaLogic.h"
#include "MergeShipTerminaRando.h"
#include "MergeShipEntrances.h"
#include "MergeShipGoal.h"

#include <mergeship/MsDelivery.h>

#include <algorithm>
#include <array>
#include <cstring>

#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"

namespace MsTermina {

namespace {

TerminaLogic sLogic;
bool sUnified = false; // the unified mode, as of the last Logic::Reset
bool sCombined = false; // RSK_MS_COMBINE_SHARED_ITEMS, as of the last Logic::Reset

// Option values of the seed being generated, in gMsTerminaOptions order (= MMOption), as 2Ship stores them.
std::array<int, MMO_COUNT> sOptions{};

// Each Termina region's STAYs (2Ship timeStayRestrictions), by RandomizerRegion.
std::vector<std::vector<Stay>> sStays;
const std::vector<Stay> kNoStays;

constexpr uint64_t kTimeBitOne = 1ULL;
constexpr uint64_t kTimeAllSlices = (kTimeBitOne << MMT_COUNT) - 1;
static_assert(MMT_COUNT == 45, "2Ship's TimeSlice changed: check HALF_DAY_TIME_RANGES below");

// HALF_DAY_TIME_RANGES (Logic.h): the time slices of each half-day, Day 1 .. Night 3.
struct HalfDayRange {
    int startSlice;
    int endSlice;
};
constexpr HalfDayRange kHalfDays[6] = {
    { MMT_DAY1_AM_06_00, MMT_DAY1_PM_04_00 },   { MMT_NIGHT1_PM_06_00, MMT_NIGHT1_AM_05_00 },
    { MMT_DAY2_AM_06_00, MMT_DAY2_PM_04_00 },   { MMT_NIGHT2_PM_06_00, MMT_NIGHT2_AM_05_00 },
    { MMT_DAY3_AM_06_00, MMT_DAY3_PM_01_00 },   { MMT_NIGHT3_PM_06_00, MMT_NIGHT3_AM_05_00 },
};

constexpr uint64_t HalfDayMask(int halfDay) {
    uint64_t mask = 0;
    for (int slice = kHalfDays[halfDay].startSlice; slice <= kHalfDays[halfDay].endSlice; ++slice) {
        mask |= kTimeBitOne << slice;
    }
    return mask;
}

constexpr MsItemKey kClockItems[6] = { MSI_MM_TIME_DAY_1, MSI_MM_TIME_NIGHT_1, MSI_MM_TIME_DAY_2,
                                       MSI_MM_TIME_NIGHT_2, MSI_MM_TIME_DAY_3, MSI_MM_TIME_NIGHT_3 };

// The canonical items Termina's logic reads (progression in a unified seed). Consumables (MS_CYCLE) are left out:
// they only count when a Termina location gives them, so an assumed copy must not open anything.
constexpr MsItemKey kLogicItems[] = {
    // ocarina, bow, arrows, explosives, tools
    // (each game's copy of an item both games have is its own row: Termina's logic reads MM's, MsDelivery_OwnWorld;
    // with "Combine Shared Items" Hyrule's copy counts as MM's, MergeShip_TerminaLogic_ApplyItem)
    MSI_MM_OCARINA, MSI_MM_HEROS_BOW, MSI_MM_PROGRESSIVE_BOW, MSI_MM_FIRE_ARROWS, MSI_MM_ICE_ARROWS,
    MSI_MM_LIGHT_ARROWS, MSI_MM_BOMB_BAG, MSI_MM_BIG_BOMB_BAG, MSI_MM_BIGGEST_BOMB_BAG, MSI_MM_PROGRESSIVE_BOMB_BAG,
    MSI_PROGRESSIVE_BOMBCHU_BAG, MSI_MM_POWDER_KEG, MSI_MM_PICTOGRAPH_BOX, MSI_MM_LENS_OF_TRUTH, MSI_MM_HOOKSHOT,
    MSI_MM_ROOM_KEY,
    // masks
    MSI_MM_DEKU_MASK, MSI_MM_GORON_MASK, MSI_MM_ZORA_MASK, MSI_MM_FIERCE_DEITY_MASK, MSI_MM_MASK_OF_TRUTH,
    MSI_MM_KAFEIS_MASK, MSI_MM_ALL_NIGHT_MASK, MSI_MM_BUNNY_HOOD, MSI_MM_KEATON_MASK, MSI_MM_GAROS_MASK,
    MSI_MM_ROMANIS_MASK, MSI_MM_CIRCUS_LEADERS_MASK, MSI_MM_POSTMANS_HAT, MSI_MM_COUPLES_MASK,
    MSI_MM_GREAT_FAIRYS_MASK, MSI_MM_GIBDO_MASK, MSI_MM_DON_GEROS_MASK, MSI_MM_KAMAROS_MASK, MSI_MM_CAPTAINS_HAT,
    MSI_MM_STONE_MASK, MSI_MM_BREMEN_MASK, MSI_MM_BLAST_MASK, MSI_MM_MASK_OF_SCENTS, MSI_MM_GIANTS_MASK,
    // songs and ocarina buttons
    MSI_MM_SONATA_OF_AWAKENING, MSI_MM_GORONS_LULLABY, MSI_PROGRESSIVE_GORON_LULLABY, MSI_MM_GORON_LULLABY_INTRO,
    MSI_MM_NEW_WAVE_BOSSA_NOVA, MSI_MM_ELEGY_OF_EMPTINESS, MSI_MM_OATH_TO_ORDER, MSI_SARIAS_SONG, MSI_SONG_OF_TIME,
    MSI_MM_SONG_OF_HEALING, MSI_EPONAS_SONG, MSI_MM_SONG_OF_SOARING, MSI_SONG_OF_STORMS, MSI_SUNS_SONG,
    MSI_OCARINA_A_BUTTON, MSI_OCARINA_C_DOWN_BUTTON, MSI_OCARINA_C_RIGHT_BUTTON, MSI_OCARINA_C_LEFT_BUTTON,
    MSI_OCARINA_C_UP_BUTTON,
    // equipment and upgrades
    MSI_BRONZE_SCALE, MSI_MAGIC_SINGLE, MSI_MAGIC_DOUBLE, MSI_PROGRESSIVE_MAGIC_METER, MSI_MM_KOKIRI_SWORD,
    MSI_PROGRESSIVE_MM_SWORD, MSI_MM_RAZOR_SWORD, MSI_MM_GILDED_SWORD, MSI_MM_GREAT_FAIRYS_SWORD,
    MSI_MM_HEROS_SHIELD, MSI_MM_MIRROR_SHIELD, MSI_ADULT_WALLET, MSI_GIANT_WALLET, MSI_TYCOON_WALLET, MSI_PROGRESSIVE_WALLET,
    MSI_MM_GREAT_SPIN_ATTACK,
    // quest items, trade items, owls, dungeon items, collectibles
    MSI_MM_ODOLWAS_REMAINS, MSI_MM_GOHTS_REMAINS, MSI_MM_GYORGS_REMAINS, MSI_MM_TWINMOLDS_REMAINS,
    MSI_MM_LAND_TITLE_DEED, MSI_MM_MOUNTAIN_TITLE_DEED, MSI_MM_OCEAN_TITLE_DEED, MSI_MM_SWAMP_TITLE_DEED,
    MSI_MM_LETTER_TO_KAFEI, MSI_MM_LETTER_TO_MAMA, MSI_MM_PENDANT_OF_MEMORIES, MSI_MM_MOONS_TEAR,
    MSI_MM_OWL_CLOCK_TOWN_SOUTH, MSI_MM_OWL_GREAT_BAY_COAST, MSI_MM_OWL_IKANA_CANYON, MSI_MM_OWL_MILK_ROAD,
    MSI_MM_OWL_MOUNTAIN_VILLAGE, MSI_MM_OWL_SNOWHEAD, MSI_MM_OWL_SOUTHERN_SWAMP, MSI_MM_OWL_STONE_TOWER,
    MSI_MM_OWL_WOODFALL, MSI_MM_OWL_ZORA_CAPE, MSI_MM_WOODFALL_SMALL_KEY, MSI_MM_SNOWHEAD_SMALL_KEY,
    MSI_MM_GREAT_BAY_SMALL_KEY, MSI_MM_STONE_TOWER_SMALL_KEY, MSI_MM_WOODFALL_BOSS_KEY, MSI_MM_SNOWHEAD_BOSS_KEY,
    MSI_MM_GREAT_BAY_BOSS_KEY, MSI_MM_STONE_TOWER_BOSS_KEY, MSI_SKELETON_KEY, MSI_MM_CLOCK_TOWN_STRAY_FAIRY,
    MSI_MM_WOODFALL_STRAY_FAIRY, MSI_MM_SNOWHEAD_STRAY_FAIRY, MSI_MM_GREAT_BAY_STRAY_FAIRY,
    MSI_MM_STONE_TOWER_STRAY_FAIRY, MSI_MM_SWAMP_SKULLTULA_TOKEN, MSI_MM_OCEAN_SKULLTULA_TOKEN, MSI_MM_FROG_BLUE,
    MSI_MM_FROG_CYAN, MSI_MM_FROG_PINK, MSI_MM_FROG_WHITE,
    // souls and clocks
    MSI_MM_SOUL_BOSS_GOHT, MSI_MM_SOUL_BOSS_GYORG, MSI_MM_SOUL_BOSS_MAJORA, MSI_MM_SOUL_BOSS_ODOLWA,
    MSI_MM_SOUL_BOSS_TWINMOLD, MSI_MM_SOUL_ENEMY_ALIEN, MSI_MM_SOUL_ENEMY_ARMOS, MSI_MM_SOUL_ENEMY_BAD_BAT,
    MSI_MM_SOUL_ENEMY_BEAMOS, MSI_MM_SOUL_ENEMY_BOE, MSI_MM_SOUL_ENEMY_BUBBLE, MSI_MM_SOUL_ENEMY_CAPTAIN_KEETA,
    MSI_MM_SOUL_ENEMY_CHUCHU, MSI_MM_SOUL_ENEMY_DEATH_ARMOS, MSI_MM_SOUL_ENEMY_DEEP_PYTHON,
    MSI_MM_SOUL_ENEMY_DEKU_BABA, MSI_MM_SOUL_ENEMY_DEXIHAND, MSI_MM_SOUL_ENEMY_DINOLFOS, MSI_MM_SOUL_ENEMY_DODONGO,
    MSI_MM_SOUL_ENEMY_DRAGONFLY, MSI_MM_SOUL_ENEMY_EENO, MSI_MM_SOUL_ENEMY_EYEGORE, MSI_MM_SOUL_ENEMY_FREEZARD,
    MSI_MM_SOUL_ENEMY_GARO, MSI_MM_SOUL_ENEMY_GEKKO, MSI_MM_SOUL_ENEMY_GIANT_BEE, MSI_MM_SOUL_ENEMY_GOMESS,
    MSI_MM_SOUL_ENEMY_GUAY, MSI_MM_SOUL_ENEMY_HIPLOOP, MSI_MM_SOUL_ENEMY_IGOS_DU_IKANA,
    MSI_MM_SOUL_ENEMY_IRON_KNUCKLE, MSI_MM_SOUL_ENEMY_KEESE, MSI_MM_SOUL_ENEMY_LEEVER, MSI_MM_SOUL_ENEMY_LIKE_LIKE,
    MSI_MM_SOUL_ENEMY_MAD_SCRUB, MSI_MM_SOUL_ENEMY_NEJIRON, MSI_MM_SOUL_ENEMY_OCTOROK, MSI_MM_SOUL_ENEMY_PEAHAT,
    MSI_MM_SOUL_ENEMY_PIRATE, MSI_MM_SOUL_ENEMY_POE, MSI_MM_SOUL_ENEMY_REDEAD, MSI_MM_SOUL_ENEMY_SHELLBLADE,
    MSI_MM_SOUL_ENEMY_SKULLFISH, MSI_MM_SOUL_ENEMY_SKULLTULA, MSI_MM_SOUL_ENEMY_SNAPPER,
    MSI_MM_SOUL_ENEMY_STALCHILD, MSI_MM_SOUL_ENEMY_TAKKURI, MSI_MM_SOUL_ENEMY_TEKTITE,
    MSI_MM_SOUL_ENEMY_WALLMASTER, MSI_MM_SOUL_ENEMY_WART, MSI_MM_SOUL_ENEMY_WIZROBE, MSI_MM_SOUL_ENEMY_WOLFOS,
    MSI_MM_TIME_DAY_1, MSI_MM_TIME_NIGHT_1, MSI_MM_TIME_DAY_2, MSI_MM_TIME_NIGHT_2, MSI_MM_TIME_DAY_3,
    MSI_MM_TIME_NIGHT_3, MSI_MM_TIME_PROGRESSIVE,
};

// Consumables (MS_CYCLE) Termina's logic reads (TerminaLogic::Has): progression too, or the fill never makes sure
// Termina gets one (a Magic Bean opens the Deku Palace's upper rooms through the day-2 rain bean). They still count for
// Termina only where a Termina check gave them, or when applied without a location (the entrance validation's "all
// items", itemSource -1).
static const MsItemKey kLogicConsumables[] = {
    MSI_MAGIC_BEAN,     MSI_DEKU_STICK_1, MSI_MM_DEKU_STICKS_5, MSI_DEKU_NUTS_5, MSI_DEKU_NUTS_10,
    MSI_MM_DEKU_NUT_1,  MSI_BOMBCHU_5,    MSI_BOMBCHU_10,       MSI_BOMBCHU_20,  MSI_MM_BOMBCHU_1,
};

struct LogicTables {
    std::array<bool, MSI_MAX + 1> logicItem{};
    std::array<bool, MSI_MAX + 1> logicConsumable{};
    std::vector<MsItemKey> bottles; // MS_KIND_BOTTLE items MM can use
    LogicTables() {
        for (MsItemKey key : kLogicItems) {
            logicItem[key] = true;
        }
        for (MsItemKey key : kLogicConsumables) {
            logicItem[key] = true;
            logicConsumable[key] = true;
        }
        for (int key = 0; key < MSI_MAX; key++) {
            const MsItemInfo* info = MsItems_Get((MsItemKey)key);
            if (info != nullptr && info->kind == MS_KIND_BOTTLE && info->usableInMM) {
                bottles.push_back((MsItemKey)key);
                logicItem[key] = true;
            }
        }
    }
};
const LogicTables& Tables() {
    static const LogicTables tables;
    return tables;
}

// 2Ship's enemySoulMap (ActorBehavior/Souls.cpp): the soul an enemy needs; MSI_MAX = it has none.
MsItemKey SoulOf(MMActor actor) {
    switch (actor) {
        case MMA_EN_INVADEPOH:
            return MSI_MM_SOUL_ENEMY_ALIEN;
        case MMA_EN_AM:
            return MSI_MM_SOUL_ENEMY_ARMOS;
        case MMA_EN_BAT:
            return MSI_MM_SOUL_ENEMY_BAD_BAT;
        case MMA_EN_VM:
            return MSI_MM_SOUL_ENEMY_BEAMOS;
        case MMA_EN_BB:
        case MMA_EN_BBFALL:
            return MSI_MM_SOUL_ENEMY_BUBBLE;
        case MMA_EN_MKK:
            return MSI_MM_SOUL_ENEMY_BOE;
        case MMA_EN_BSB:
            return MSI_MM_SOUL_ENEMY_CAPTAIN_KEETA;
        case MMA_EN_SLIME:
            return MSI_MM_SOUL_ENEMY_CHUCHU;
        case MMA_EN_FAMOS:
            return MSI_MM_SOUL_ENEMY_DEATH_ARMOS;
        case MMA_EN_DRAGON:
            return MSI_MM_SOUL_ENEMY_DEEP_PYTHON;
        case MMA_EN_DEKUBABA:
        case MMA_EN_KAREBABA:
        case MMA_BOSS_05:
            return MSI_MM_SOUL_ENEMY_DEKU_BABA;
        case MMA_EN_WDHAND:
            return MSI_MM_SOUL_ENEMY_DEXIHAND;
        case MMA_EN_DINOFOS:
            return MSI_MM_SOUL_ENEMY_DINOLFOS;
        case MMA_EN_DODONGO:
            return MSI_MM_SOUL_ENEMY_DODONGO;
        case MMA_EN_GRASSHOPPER:
            return MSI_MM_SOUL_ENEMY_DRAGONFLY;
        case MMA_EN_SNOWMAN:
            return MSI_MM_SOUL_ENEMY_EENO;
        case MMA_EN_EGOL:
            return MSI_MM_SOUL_ENEMY_EYEGORE;
        case MMA_EN_FZ:
            return MSI_MM_SOUL_ENEMY_FREEZARD;
        case MMA_EN_JSO:
        case MMA_EN_JSO2:
            return MSI_MM_SOUL_ENEMY_GARO;
        case MMA_EN_BIGSLIME:
        case MMA_EN_PAMETFROG:
            return MSI_MM_SOUL_ENEMY_GEKKO;
        case MMA_EN_BEE:
            return MSI_MM_SOUL_ENEMY_GIANT_BEE;
        case MMA_EN_DEATH:
            return MSI_MM_SOUL_ENEMY_GOMESS;
        case MMA_EN_CROW:
            return MSI_MM_SOUL_ENEMY_GUAY;
        case MMA_EN_PP:
            return MSI_MM_SOUL_ENEMY_HIPLOOP;
        case MMA_EN_KNIGHT:
            return MSI_MM_SOUL_ENEMY_IGOS_DU_IKANA;
        case MMA_EN_IK:
            return MSI_MM_SOUL_ENEMY_IRON_KNUCKLE;
        case MMA_EN_FIREFLY:
            return MSI_MM_SOUL_ENEMY_KEESE;
        case MMA_EN_NEO_REEBA:
            return MSI_MM_SOUL_ENEMY_LEEVER;
        case MMA_EN_RR:
            return MSI_MM_SOUL_ENEMY_LIKE_LIKE;
        case MMA_EN_DEKUNUTS:
            return MSI_MM_SOUL_ENEMY_MAD_SCRUB;
        case MMA_EN_BAGUO:
            return MSI_MM_SOUL_ENEMY_NEJIRON;
        case MMA_EN_OKUTA:
            return MSI_MM_SOUL_ENEMY_OCTOROK;
        case MMA_EN_PEEHAT:
            return MSI_MM_SOUL_ENEMY_PEAHAT;
        case MMA_EN_KAIZOKU:
            return MSI_MM_SOUL_ENEMY_PIRATE;
        case MMA_EN_BIGPO:
        case MMA_EN_PO_SISTERS:
            return MSI_MM_SOUL_ENEMY_POE;
        case MMA_EN_RD:
            return MSI_MM_SOUL_ENEMY_REDEAD;
        case MMA_EN_SB:
            return MSI_MM_SOUL_ENEMY_SHELLBLADE;
        case MMA_EN_PR:
        case MMA_EN_PR2:
            return MSI_MM_SOUL_ENEMY_SKULLFISH;
        case MMA_EN_ST:
        case MMA_EN_SW:
            return MSI_MM_SOUL_ENEMY_SKULLTULA;
        case MMA_EN_BIGPAMET:
        case MMA_EN_KAME:
            return MSI_MM_SOUL_ENEMY_SNAPPER;
        case MMA_EN_SKB:
            return MSI_MM_SOUL_ENEMY_STALCHILD;
        case MMA_EN_THIEFBIRD:
            return MSI_MM_SOUL_ENEMY_TAKKURI;
        case MMA_EN_TITE:
            return MSI_MM_SOUL_ENEMY_TEKTITE;
        case MMA_EN_FLOORMAS:
        case MMA_EN_WALLMAS:
            return MSI_MM_SOUL_ENEMY_WALLMASTER;
        case MMA_BOSS_04:
            return MSI_MM_SOUL_ENEMY_WART;
        case MMA_EN_WIZ:
            return MSI_MM_SOUL_ENEMY_WIZROBE;
        case MMA_EN_WF:
            return MSI_MM_SOUL_ENEMY_WOLFOS;
        default:
            return MSI_MAX;
    }
}

} // namespace

TerminaLogic* mm = &sLogic;

void BeginRegionTable() {
    for (size_t i = 0; i < MMO_COUNT; i++) {
        sOptions[i] = MergeShip_Termina_OptionValue(i);
    }
    sStays.assign(RR_MAX, {});
}

void SetStays(RandomizerRegion region, std::vector<Stay> stays) {
    if (sStays.size() != RR_MAX) {
        sStays.assign(RR_MAX, {});
    }
    sStays[region] = std::move(stays);
}

// ---- State -----------------------------------------------------------------------------------------------------------

void TerminaLogic::Reset() {
    memset(mCount, 0, sizeof(mCount));
    time = 0;
    // 2Ship's computed starting items that the unified pool left out (maps, keys, stray fairies to start with, the
    // clock of clock shuffle): owned from the start.
    for (RandomizerGet rg : MergeShip_Termina_LogicStartingItems()) {
        ApplyItem(MsItems_KeyOfRG(rg), true);
    }
}

void TerminaLogic::ApplyItem(MsItemKey key, bool add) {
    if (key >= MSI_MAX) {
        return;
    }
    if (add) {
        mCount[key]++;
    } else if (mCount[key] > 0) {
        mCount[key]--;
    }
}

int TerminaLogic::Option(MMOption option) const {
    return option < MMO_COUNT ? sOptions[option] : 0;
}

// ---- Items (Logic.h) -------------------------------------------------------------------------------------------------

bool TerminaLogic::Has(MMItem item) const {
    switch (item) {
        case MMI_OCARINA_OF_TIME: // MM's own ocarina (Hyrule's are Hyrule's, the user's rule "all items except masks")
            return Owns(MSI_MM_OCARINA);
        case MMI_BOW:
            return Owns(MSI_MM_HEROS_BOW) || Owns(MSI_MM_PROGRESSIVE_BOW);
        case MMI_ARROW_FIRE:
            return Owns(MSI_MM_FIRE_ARROWS);
        case MMI_ARROW_ICE:
            return Owns(MSI_MM_ICE_ARROWS);
        case MMI_ARROW_LIGHT:
            return Owns(MSI_MM_LIGHT_ARROWS);
        case MMI_BOMB:
            return Owns(MSI_MM_BOMB_BAG) || Owns(MSI_MM_BIG_BOMB_BAG) || Owns(MSI_MM_BIGGEST_BOMB_BAG) ||
                   Owns(MSI_MM_PROGRESSIVE_BOMB_BAG);
        case MMI_BOMBCHU: // 2Ship's bomb bag also gives bombchus (GiveItem.cpp)
            return Has(MMI_BOMB) || Owns(MSI_PROGRESSIVE_BOMBCHU_BAG) || Owns(MSI_BOMBCHU_5) || Owns(MSI_BOMBCHU_10) ||
                   Owns(MSI_BOMBCHU_20) || Owns(MSI_MM_BOMBCHU_1);
        case MMI_DEKU_STICK:
            return Owns(MSI_DEKU_STICK_1) || Owns(MSI_MM_DEKU_STICKS_5);
        case MMI_DEKU_NUT:
            return Owns(MSI_DEKU_NUTS_5) || Owns(MSI_DEKU_NUTS_10) || Owns(MSI_MM_DEKU_NUT_1);
        case MMI_MAGIC_BEANS:
            return Owns(MSI_MAGIC_BEAN);
        case MMI_POWDER_KEG:
            return Owns(MSI_MM_POWDER_KEG);
        case MMI_PICTOGRAPH_BOX:
            return Owns(MSI_MM_PICTOGRAPH_BOX);
        case MMI_LENS_OF_TRUTH:
            return Owns(MSI_MM_LENS_OF_TRUTH);
        case MMI_HOOKSHOT:
            return Owns(MSI_MM_HOOKSHOT);
        case MMI_SWORD_GREAT_FAIRY:
            return Owns(MSI_MM_GREAT_FAIRYS_SWORD);
        case MMI_ROOM_KEY:
            return Owns(MSI_MM_ROOM_KEY);
        case MMI_MASK_DEKU:
            return Owns(MSI_MM_DEKU_MASK);
        case MMI_MASK_GORON:
            return Owns(MSI_MM_GORON_MASK);
        case MMI_MASK_ZORA:
            return Owns(MSI_MM_ZORA_MASK);
        case MMI_MASK_FIERCE_DEITY:
            return Owns(MSI_MM_FIERCE_DEITY_MASK);
        case MMI_MASK_TRUTH:
            return Owns(MSI_MM_MASK_OF_TRUTH);
        case MMI_MASK_KAFEIS_MASK:
            return Owns(MSI_MM_KAFEIS_MASK);
        case MMI_MASK_ALL_NIGHT:
            return Owns(MSI_MM_ALL_NIGHT_MASK);
        case MMI_MASK_BUNNY:
            return Owns(MSI_MM_BUNNY_HOOD);
        case MMI_MASK_KEATON:
            return Owns(MSI_MM_KEATON_MASK);
        case MMI_MASK_GARO:
            return Owns(MSI_MM_GAROS_MASK);
        case MMI_MASK_ROMANI:
            return Owns(MSI_MM_ROMANIS_MASK);
        case MMI_MASK_CIRCUS_LEADER:
            return Owns(MSI_MM_CIRCUS_LEADERS_MASK);
        case MMI_MASK_POSTMAN:
            return Owns(MSI_MM_POSTMANS_HAT);
        case MMI_MASK_COUPLE:
            return Owns(MSI_MM_COUPLES_MASK);
        case MMI_MASK_GREAT_FAIRY:
            return Owns(MSI_MM_GREAT_FAIRYS_MASK);
        case MMI_MASK_GIBDO:
            return Owns(MSI_MM_GIBDO_MASK);
        case MMI_MASK_DON_GERO:
            return Owns(MSI_MM_DON_GEROS_MASK);
        case MMI_MASK_KAMARO:
            return Owns(MSI_MM_KAMAROS_MASK);
        case MMI_MASK_CAPTAIN:
            return Owns(MSI_MM_CAPTAINS_HAT);
        case MMI_MASK_STONE:
            return Owns(MSI_MM_STONE_MASK);
        case MMI_MASK_BREMEN:
            return Owns(MSI_MM_BREMEN_MASK);
        case MMI_MASK_BLAST:
            return Owns(MSI_MM_BLAST_MASK);
        case MMI_MASK_SCENTS:
            return Owns(MSI_MM_MASK_OF_SCENTS);
        case MMI_MASK_GIANT:
            return Owns(MSI_MM_GIANTS_MASK);
        default:
            return false;
    }
}

// The logic is not tied to a current form: a form is available when its mask is (CAN_BE_*).
bool TerminaLogic::CanBeDeku() const {
    return Has(MMI_MASK_DEKU);
}
bool TerminaLogic::CanBeGoron() const {
    return Has(MMI_MASK_GORON);
}
bool TerminaLogic::CanBeZora() const {
    return Has(MMI_MASK_ZORA);
}
bool TerminaLogic::CanBeDeity() const {
    return Has(MMI_MASK_FIERCE_DEITY);
}

bool TerminaLogic::HasMagic() const {
    return Owns(MSI_MAGIC_SINGLE) || Owns(MSI_MAGIC_DOUBLE) || Owns(MSI_PROGRESSIVE_MAGIC_METER);
}

bool TerminaLogic::HasBottle() const {
    for (MsItemKey key : Tables().bottles) {
        if (Owns(key)) {
            return true;
        }
    }
    return false;
}

bool TerminaLogic::HasBottleItem(MMItem item) const {
    // HAS_BOTTLE_ITEM: no Termina region asks for a bottle's content yet (tools/port_mm_rando.py maps it here).
    return false;
}

bool TerminaLogic::CheckMaxHp(int hearts) const {
    // One heart budget, Hyrule's (UNIFIED_RANDO_DESIGN.md): 3 to start, containers and pieces.
    const int quarters = 12 + 4 * Count(MSI_HEART_CONTAINER) + Count(MSI_PIECE_OF_HEART);
    return hearts * 4 <= quarters;
}

int TerminaLogic::SwordLevel() const {
    // The progressive chain Kokiri -> Razor -> Gilded: Kokiri owned plus the progressive copies.
    int level = std::min(3, (int)Count(MSI_MM_KOKIRI_SWORD) + (int)Count(MSI_PROGRESSIVE_MM_SWORD));
    if (Owns(MSI_MM_RAZOR_SWORD)) {
        level = std::max(level, 2);
    }
    if (Owns(MSI_MM_GILDED_SWORD)) {
        level = 3;
    }
    return level;
}

int TerminaLogic::ShieldLevel() const {
    if (Owns(MSI_MM_MIRROR_SHIELD)) {
        return MM_SHIELD_MIRROR;
    }
    return Owns(MSI_MM_HEROS_SHIELD) ? MM_SHIELD_HERO : MM_SHIELD_NONE;
}

int TerminaLogic::WalletLevel() const {
    int level = std::min(3, (int)Count(MSI_PROGRESSIVE_WALLET));
    if (Owns(MSI_TYCOON_WALLET)) {
        level = 3;
    } else if (Owns(MSI_GIANT_WALLET)) {
        level = std::max(level, 2);
    } else if (Owns(MSI_ADULT_WALLET)) {
        level = std::max(level, 1);
    }
    return level;
}

bool TerminaLogic::CanUseHumanSword() const {
    return SwordLevel() >= 1;
}

bool TerminaLogic::CanUseSword() const {
    return CanUseHumanSword() || Has(MMI_SWORD_GREAT_FAIRY) || CanBeDeity();
}

bool TerminaLogic::CanUseExplosive() const {
    return Has(MMI_BOMB) || Has(MMI_BOMBCHU) || (Has(MMI_MASK_BLAST) && ShieldLevel() > MM_SHIELD_NONE);
}

bool TerminaLogic::CanUseProjectile() const {
    return Has(MMI_BOW) || Has(MMI_HOOKSHOT) || (CanBeDeku() && HasMagic()) || CanBeZora();
}

bool TerminaLogic::CanFullyCutKeatonGrass() const {
    return HasMagic() && SwordLevel() >= 2 && Owns(MSI_MM_GREAT_SPIN_ATTACK);
}

bool TerminaLogic::CanHookScarecrow() const {
    return Has(MMI_OCARINA_OF_TIME) && Has(MMI_HOOKSHOT) && CanPlayOcarinaSong(MMOS_SCARECROW_SPAWN);
}

bool TerminaLogic::CanLightTorchNearAnother() const {
    return Has(MMI_DEKU_STICK) || CanUseMagicArrow(MMI_ARROW_FIRE);
}

bool TerminaLogic::CanGrowBeanPlant() const {
    return Has(MMI_MAGIC_BEANS) &&
           (CanPlaySong(MMSONG_STORMS) ||
            (HasBottle() && (logic->Get(LOGIC_MM_ACCESS_SPRING_WATER) || logic->Get(LOGIC_MM_ACCESS_HOT_SPRING_WATER))));
}

bool TerminaLogic::CanUseDay2RainBean() const {
    return CanGrowBeanPlant() || (Has(MMI_MAGIC_BEANS) && OwnsHalfDayForMode(2));
}

bool TerminaLogic::CanRideEpona() const {
    return CanPlaySong(MMSONG_EPONA);
}

bool TerminaLogic::CanSwim() const {
    // 2Ship gives swimming at the start unless it is shuffled; shuffled, it is the shared Bronze Scale.
    return !Option(MMO_SHUFFLE_SWIM) || Owns(MSI_BRONZE_SCALE);
}

bool TerminaLogic::CanUseMagicArrow(MMItem arrow) const {
    return Has(MMI_BOW) && Has(arrow) && HasMagic();
}

bool TerminaLogic::CanPlaySong(MMSong song) const {
    bool hasSong = false;
    MMOcarinaSong buttons = MMOS_SONATA;
    // (QUEST_SONG_X - QUEST_SONG_SONATA) + OCARINA_SONG_SONATA, as 2Ship's CAN_PLAY_SONG computes it (for the lullaby
    // intro that lands on the Deku wind fish song's buttons; kept 1:1).
    switch (song) {
        case MMSONG_SONATA:
            hasSong = Owns(MSI_MM_SONATA_OF_AWAKENING);
            buttons = MMOS_SONATA;
            break;
        case MMSONG_LULLABY:
            hasSong = Owns(MSI_MM_GORONS_LULLABY) || Count(MSI_PROGRESSIVE_GORON_LULLABY) >= 2;
            buttons = MMOS_GORON_LULLABY;
            break;
        case MMSONG_LULLABY_INTRO:
            hasSong = Owns(MSI_MM_GORON_LULLABY_INTRO) || Owns(MSI_MM_GORONS_LULLABY) ||
                      Owns(MSI_PROGRESSIVE_GORON_LULLABY);
            buttons = MMOS_WIND_FISH_DEKU;
            break;
        case MMSONG_BOSSA_NOVA:
            hasSong = Owns(MSI_MM_NEW_WAVE_BOSSA_NOVA);
            buttons = MMOS_NEW_WAVE;
            break;
        case MMSONG_ELEGY:
            hasSong = Owns(MSI_MM_ELEGY_OF_EMPTINESS);
            buttons = MMOS_ELEGY;
            break;
        case MMSONG_OATH:
            hasSong = Owns(MSI_MM_OATH_TO_ORDER);
            buttons = MMOS_OATH;
            break;
        case MMSONG_SARIA:
            hasSong = Owns(MSI_SARIAS_SONG);
            buttons = MMOS_SARIAS;
            break;
        case MMSONG_TIME:
            hasSong = Owns(MSI_SONG_OF_TIME);
            buttons = MMOS_TIME;
            break;
        case MMSONG_HEALING:
            hasSong = Owns(MSI_MM_SONG_OF_HEALING);
            buttons = MMOS_HEALING;
            break;
        case MMSONG_EPONA:
            hasSong = Owns(MSI_EPONAS_SONG);
            buttons = MMOS_EPONAS;
            break;
        case MMSONG_SOARING:
            hasSong = Owns(MSI_MM_SONG_OF_SOARING);
            buttons = MMOS_SOARING;
            break;
        case MMSONG_STORMS:
            hasSong = Owns(MSI_SONG_OF_STORMS);
            buttons = MMOS_STORMS;
            break;
        case MMSONG_SUN:
            hasSong = Owns(MSI_SUNS_SONG);
            buttons = MMOS_SUNS;
            break;
        default:
            return false;
    }
    return Has(MMI_OCARINA_OF_TIME) && hasSong && CanPlayOcarinaSong(buttons);
}

bool TerminaLogic::OcarinaButton(int button) const {
    // 2Ship gives the buttons at the start unless they are shuffled; shuffled, they are the shared button items.
    if (!Option(MMO_SHUFFLE_OCARINA_BUTTONS)) {
        return true;
    }
    static constexpr MsItemKey kButtons[5] = { MSI_OCARINA_A_BUTTON, MSI_OCARINA_C_DOWN_BUTTON,
                                               MSI_OCARINA_C_RIGHT_BUTTON, MSI_OCARINA_C_LEFT_BUTTON,
                                               MSI_OCARINA_C_UP_BUTTON };
    return button >= 0 && button < 5 && Owns(kButtons[button]);
}

bool TerminaLogic::CanPlayOcarinaSong(MMOcarinaSong song) const {
    enum { A, DOWN, RIGHT, LEFT, UP };
    auto b = [this](int button) { return OcarinaButton(button); };
    switch (song) {
        case MMOS_SONATA:
            return b(UP) && b(LEFT) && b(A) && b(RIGHT);
        case MMOS_GORON_LULLABY:
        case MMOS_GORON_LULLABY_INTRO:
            return b(A) && b(RIGHT) && b(LEFT);
        case MMOS_NEW_WAVE:
        case MMOS_ELEGY:
            return b(LEFT) && b(UP) && b(RIGHT) && b(DOWN);
        case MMOS_OATH:
        case MMOS_WIND_FISH_ZORA:
            return b(RIGHT) && b(LEFT) && b(DOWN) && b(A) && b(UP);
        case MMOS_TIME:
        case MMOS_INVERTED_TIME:
        case MMOS_DOUBLE_TIME:
        case MMOS_WIND_FISH_GORON:
        case MMOS_EVAN_PART1:
            return b(RIGHT) && b(A) && b(DOWN);
        case MMOS_HEALING:
        case MMOS_SARIAS:
        case MMOS_EVAN_PART2:
            return b(LEFT) && b(RIGHT) && b(DOWN);
        case MMOS_EPONAS:
        case MMOS_WIND_FISH_HUMAN:
            return b(UP) && b(LEFT) && b(RIGHT);
        case MMOS_SOARING:
            return b(DOWN) && b(LEFT) && b(UP);
        case MMOS_STORMS:
            return b(A) && b(DOWN) && b(UP);
        case MMOS_SUNS:
            return b(RIGHT) && b(DOWN) && b(UP);
        case MMOS_WIND_FISH_DEKU:
            return b(RIGHT) && b(A) && b(DOWN) && b(LEFT);
        case MMOS_SCARECROW_SPAWN: {
            int found = 0;
            for (int i = A; i <= UP; i++) {
                found += b(i) ? 1 : 0;
            }
            return found >= 2;
        }
        case MMOS_TERMINA_WALL:
            return b(A) && b(DOWN) && b(RIGHT) && b(LEFT) && b(UP);
        default:
            return true;
    }
}

bool TerminaLogic::FoundAllFrogs() const {
    return Owns(MSI_MM_FROG_BLUE) && Owns(MSI_MM_FROG_CYAN) && Owns(MSI_MM_FROG_PINK) && Owns(MSI_MM_FROG_WHITE);
}

bool TerminaLogic::GbtCanReverseWaterFlow() const {
    return logic->Get(LOGIC_MM_GREAT_BAY_RED_SWITCH_1) && logic->Get(LOGIC_MM_GREAT_BAY_RED_SWITCH_2) &&
           Has(MMI_HOOKSHOT);
}

bool TerminaLogic::GbtGreenSwitchFlow() const {
    return logic->Get(LOGIC_MM_GREAT_BAY_GREEN_SWITCH_1) && logic->Get(LOGIC_MM_GREAT_BAY_GREEN_SWITCH_2) &&
           logic->Get(LOGIC_MM_GREAT_BAY_GREEN_SWITCH_3);
}

bool TerminaLogic::CanAfford(RandomizerCheck rc) const {
    const int price = MergeShip_Termina_GetPrice(rc);
    const int wallet = WalletLevel();
    return price < 100 || (price <= 200 && wallet >= 1) || wallet >= 2;
}

bool TerminaLogic::CanOwlWarp(MMOwl owl) const {
    // 2Ship activates an owl by giving its owl item (hitting the statue gives it when owls are not shuffled).
    static constexpr MsItemKey kOwls[MMOWL_COUNT] = {
        MSI_MM_OWL_GREAT_BAY_COAST,   MSI_MM_OWL_ZORA_CAPE,  MSI_MM_OWL_SNOWHEAD,       MSI_MM_OWL_MOUNTAIN_VILLAGE,
        MSI_MM_OWL_CLOCK_TOWN_SOUTH,  MSI_MM_OWL_MILK_ROAD,  MSI_MM_OWL_WOODFALL,       MSI_MM_OWL_SOUTHERN_SWAMP,
        MSI_MM_OWL_IKANA_CANYON,      MSI_MM_OWL_STONE_TOWER,
    };
    return owl < MMOWL_COUNT && Owns(kOwls[owl]);
}

bool TerminaLogic::Quest(MMQuest quest) const {
    switch (quest) {
        case MMQ_REMAINS_ODOLWA:
            return Owns(MSI_MM_ODOLWAS_REMAINS);
        case MMQ_REMAINS_GOHT:
            return Owns(MSI_MM_GOHTS_REMAINS);
        case MMQ_REMAINS_GYORG:
            return Owns(MSI_MM_GYORGS_REMAINS);
        case MMQ_REMAINS_TWINMOLD:
            return Owns(MSI_MM_TWINMOLDS_REMAINS);
        default:
            return false;
    }
}

bool TerminaLogic::BossKey(MMDungeon dungeon) const {
    static constexpr MsItemKey kBossKeys[MMD_COUNT] = { MSI_MM_WOODFALL_BOSS_KEY, MSI_MM_SNOWHEAD_BOSS_KEY,
                                                        MSI_MM_GREAT_BAY_BOSS_KEY, MSI_MM_STONE_TOWER_BOSS_KEY };
    return dungeon < MMD_COUNT && Owns(kBossKeys[dungeon]);
}

int TerminaLogic::KeyCount(MMDungeon dungeon) const {
    static constexpr MsItemKey kSmallKeys[MMD_COUNT] = { MSI_MM_WOODFALL_SMALL_KEY, MSI_MM_SNOWHEAD_SMALL_KEY,
                                                         MSI_MM_GREAT_BAY_SMALL_KEY, MSI_MM_STONE_TOWER_SMALL_KEY };
    static constexpr int kMaxKeys[MMD_COUNT] = { 1, 3, 1, 4 }; // the Skeleton Key gives them all (GiveItem.cpp)
    if (dungeon >= MMD_COUNT) {
        return 0;
    }
    const int keys = Count(kSmallKeys[dungeon]);
    return Owns(MSI_SKELETON_KEY) ? std::max(keys, kMaxKeys[dungeon]) : keys;
}

bool TerminaLogic::WeekEvent(MMWeekEvent event) const {
    switch (event) {
        case MMW_08_80: // set by the Clock Town stray fairy (GiveItem.cpp)
            return Owns(MSI_MM_CLOCK_TOWN_STRAY_FAIRY);
        default:
            return false;
    }
}

bool TerminaLogic::RandoInf(MMRandoInf flag) const {
    switch (flag) {
        case MMRI_OBTAINED_DEED_LAND:
            return Owns(MSI_MM_LAND_TITLE_DEED);
        case MMRI_OBTAINED_DEED_MOUNTAIN:
            return Owns(MSI_MM_MOUNTAIN_TITLE_DEED);
        case MMRI_OBTAINED_DEED_OCEAN:
            return Owns(MSI_MM_OCEAN_TITLE_DEED);
        case MMRI_OBTAINED_DEED_SWAMP:
            return Owns(MSI_MM_SWAMP_TITLE_DEED);
        case MMRI_OBTAINED_LETTER_TO_KAFEI:
            return Owns(MSI_MM_LETTER_TO_KAFEI);
        case MMRI_OBTAINED_LETTER_TO_MAMA:
            return Owns(MSI_MM_LETTER_TO_MAMA);
        case MMRI_OBTAINED_MOONS_TEAR:
            return Owns(MSI_MM_MOONS_TEAR);
        case MMRI_OBTAINED_PENDANT_OF_MEMORIES:
            return Owns(MSI_MM_PENDANT_OF_MEMORIES);
        case MMRI_OBTAINED_ROOM_KEY:
            return Owns(MSI_MM_ROOM_KEY);
        case MMRI_OBTAINED_SOUL_OF_ENEMY_CHUCHUS:
            return !Option(MMO_SHUFFLE_ENEMY_SOULS) || Owns(MSI_MM_SOUL_ENEMY_CHUCHU);
        case MMRI_OBTAINED_SOUL_OF_ENEMY_OCTOROKS:
            return !Option(MMO_SHUFFLE_ENEMY_SOULS) || Owns(MSI_MM_SOUL_ENEMY_OCTOROK);
        default:
            return false;
    }
}

bool TerminaLogic::HasEnoughStrayFairies(MMDungeon dungeon) const {
    static constexpr MsItemKey kFairies[MMD_COUNT] = { MSI_MM_WOODFALL_STRAY_FAIRY, MSI_MM_SNOWHEAD_STRAY_FAIRY,
                                                       MSI_MM_GREAT_BAY_STRAY_FAIRY, MSI_MM_STONE_TOWER_STRAY_FAIRY };
    return dungeon < MMD_COUNT && Count(kFairies[dungeon]) >= Option(MMO_STRAY_FAIRIES_REQUIRED);
}

bool TerminaLogic::HasEnoughSkulltulaTokens(MMSpiderHouse house) const {
    const MsItemKey token = house == MMSH_SWAMP ? MSI_MM_SWAMP_SKULLTULA_TOKEN : MSI_MM_OCEAN_SKULLTULA_TOKEN;
    return Count(token) >= Option(MMO_SKULLTULA_TOKENS_REQUIRED);
}

bool TerminaLogic::CanAccessDungeon(MMDungeon dungeon) const {
    bool hasSongAccess = false;
    bool hasFormAccess = false;
    switch (dungeon) {
        case MMD_WOODFALL_TEMPLE:
            hasSongAccess = CanPlaySong(MMSONG_SONATA);
            hasFormAccess = CanBeDeku() && Has(MMI_OCARINA_OF_TIME);
            break;
        case MMD_SNOWHEAD_TEMPLE:
            hasSongAccess = CanPlaySong(MMSONG_LULLABY);
            hasFormAccess = CanBeGoron() && Has(MMI_OCARINA_OF_TIME);
            break;
        case MMD_GREAT_BAY_TEMPLE:
            hasSongAccess = CanPlaySong(MMSONG_BOSSA_NOVA);
            hasFormAccess = CanBeZora() && Has(MMI_OCARINA_OF_TIME);
            break;
        default:
            break;
    }
    switch (Option(MMO_ACCESS_DUNGEONS)) {
        case MM_RO_ACCESS_DUNGEONS_FORM_OR_SONG:
            return hasSongAccess || hasFormAccess;
        case MM_RO_ACCESS_DUNGEONS_FORM_ONLY:
            return hasFormAccess;
        case MM_RO_ACCESS_DUNGEONS_SONG_ONLY:
            return hasSongAccess;
        case MM_RO_ACCESS_DUNGEONS_OPEN:
            return true;
        case MM_RO_ACCESS_DUNGEONS_FORM_AND_SONG:
        default:
            return hasSongAccess && hasFormAccess;
    }
}

bool TerminaLogic::HaveEnemySoul(MMActor actor) const {
    const MsItemKey soul = SoulOf(actor);
    return soul == MSI_MAX || Owns(soul); // an enemy without a soul acts as if it were obtained
}

bool TerminaLogic::CanKillEnemy(MMActor actor) const {
    // If enemy souls are shuffled, and the relevant soul is not obtained, we cannot kill that enemy.
    if (Option(MMO_SHUFFLE_ENEMY_SOULS) && !HaveEnemySoul(actor)) {
        return false;
    }
    const bool noBossSouls = !Option(MMO_SHUFFLE_BOSS_SOULS);
    switch (actor) {
        case MMA_BOSS_01: // Odolwa
            return (CanUseSword() || CanBeGoron() || CanBeZora() || CanUseExplosive() ||
                    CanUseMagicArrow(MMI_ARROW_FIRE) || CanUseMagicArrow(MMI_ARROW_LIGHT)) &&
                   (Owns(MSI_MM_SOUL_BOSS_ODOLWA) || noBossSouls);
        case MMA_BOSS_02: // Twinmold
            return (Has(MMI_BOW) || (Has(MMI_MASK_GIANT) && HasMagic() && CanUseHumanSword())) &&
                   (Owns(MSI_MM_SOUL_BOSS_TWINMOLD) || noBossSouls);
        case MMA_BOSS_03: // Gyorg
            return ((CanBeDeity() && HasMagic()) || (CanBeZora() && HasMagic())) &&
                   (Owns(MSI_MM_SOUL_BOSS_GYORG) || noBossSouls);
        case MMA_BOSS_04: // Wart
            return Has(MMI_BOW) || Has(MMI_HOOKSHOT) || CanBeZora();
        case MMA_BOSS_HAKUGIN: // Goht
            return CanUseMagicArrow(MMI_ARROW_FIRE) && (Owns(MSI_MM_SOUL_BOSS_GOHT) || noBossSouls);
        case MMA_EN_KNIGHT: // Igos du Ikana / IdI Lackey
            return CanUseMagicArrow(MMI_ARROW_FIRE) && ShieldLevel() >= MM_SHIELD_MIRROR &&
                   (CanUseSword() || CanBeDeku() || CanBeGoron() || CanBeZora());
        case MMA_EN_KAIZOKU: // Fighter Pirate
            return CanUseSword() || CanBeZora();
        case MMA_EN_PAMETFROG: // Woodfall Temple Gekko (and Snapper)
            return Has(MMI_BOW) && CanKillEnemy(MMA_EN_BIGPAMET);
        case MMA_EN_BIGSLIME: // Great Bay Temple Gekko
            return CanUseMagicArrow(MMI_ARROW_ICE);
        case MMA_EN_SW: // Gold Skulltula & Skullwalltula
            return CanUseProjectile() || CanBeDeku() || CanBeGoron() || CanUseSword() || CanUseExplosive();
        case MMA_EN_DINOFOS: // Dinolfos
            return CanUseSword() || CanBeGoron() || Has(MMI_BOW) || (CanBeDeku() && HasMagic());
        case MMA_EN_WIZ: // Wizrobe
            return Has(MMI_BOW) || Has(MMI_HOOKSHOT) || CanUseSword() || CanBeGoron();
        case MMA_EN_WF: // Wolfos
            return CanUseSword() || (CanBeDeku() && HasMagic()) || CanBeGoron() || CanBeZora();
        case MMA_EN_JSO: // Garo
            return Has(MMI_MASK_GARO) && (Has(MMI_BOW) || CanBeGoron() || CanUseSword());
        case MMA_EN_JSO2: // Garo Master
            return Has(MMI_BOW) || CanBeGoron() || CanUseSword();
        case MMA_EN_IK: // Iron Knuckle
            return CanUseSword() || CanBeGoron();
        case MMA_EN_GRASSHOPPER: // Dragonfly
            return (CanBeDeku() && HasMagic()) || CanUseExplosive() || Has(MMI_DEKU_NUT) || CanUseSword() ||
                   CanBeZora();
        case MMA_EN_MKK: // Boe
            return (CanBeDeku() && HasMagic()) || CanUseExplosive() || Has(MMI_DEKU_NUT) || CanUseSword() ||
                   CanBeZora() || CanBeGoron();
        case MMA_EN_BIGPAMET: // Snapper
            return CanBeDeku() || CanUseExplosive() || CanBeGoron();
        case MMA_EN_ST: // Large Skulltula
            return CanUseSword() || CanUseProjectile() || CanBeGoron() || CanUseExplosive();
        case MMA_EN_BAT: // Bad Bat
            return CanUseSword() || Has(MMI_HOOKSHOT) || Has(MMI_BOW) || CanUseExplosive() || CanBeGoron() ||
                   CanBeZora();
        case MMA_EN_DEKUBABA: // Neck bending Deku Baba
            return CanUseSword() || CanBeDeku() || CanBeGoron() || CanBeZora() || Has(MMI_BOW) || CanUseExplosive() ||
                   Has(MMI_DEKU_STICK);
        case MMA_OBJ_SNOWBALL: // Large Snowball
            return CanUseExplosive() || CanBeGoron() || CanUseMagicArrow(MMI_ARROW_FIRE);
        case MMA_EN_AM: // Armos
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || CanUseExplosive();
        case MMA_EN_VM: // Beamos
            return CanUseExplosive();
        case MMA_EN_BB:     // Blue Bubble
        case MMA_EN_BBFALL: // Red Bubble
            return CanUseSword() || CanBeGoron() || CanBeZora() || Has(MMI_BOW) || Has(MMI_DEKU_STICK);
        case MMA_EN_RAT:       // Real Bombchu
        case MMA_EN_TUBO_TRAP: // Flying Pot
            return true;
        case MMA_EN_FAMOS: // Death Armos
            return CanUseMagicArrow(MMI_ARROW_LIGHT);
        case MMA_EN_DODONGO: // Dodongo
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic());
        case MMA_EN_FLOORMAS: // Floormaster
        case MMA_EN_WALLMAS:  // Wallmaster
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK);
        case MMA_EN_FZ: // Freezard
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanUseMagicArrow(MMI_ARROW_FIRE) ||
                   Has(MMI_HOOKSHOT);
        case MMA_EN_CROW: // Guay
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_FIREFLY: // Keese
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_RR: // Like Like
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK);
        case MMA_EN_DEKUNUTS: // Mad Scrub
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_KAREBABA: // Wilted/Mini Babas
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku();
        case MMA_EN_PEEHAT: // Peahat
            return (CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || Has(MMI_DEKU_STICK)) && IsDay();
        case MMA_EN_RD: // Redead & Gibdos
            return CanUseSword() || CanBeDeku() || CanBeGoron() || CanBeZora() || Has(MMI_DEKU_STICK);
        case MMA_EN_BSB: // Captain Keeta
            return Has(MMI_BOW) &&
                   (CanUseSword() || Has(MMI_DEKU_STICK) || CanUseExplosive() || CanBeGoron() || CanBeZora());
        case MMA_EN_SKB: // Stalchild
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_TITE: // Tektite
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW);
        case MMA_EN_SLIME: // Chuchus
            return CanUseSword() || CanBeZora() || CanBeDeku() || Has(MMI_BOW) || Has(MMI_DEKU_STICK);
        case MMA_EN_SNOWMAN: // Eeno
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_WDHAND: // Dexihand
            return CanBeZora() && HasMagic();
        case MMA_EN_KAME: // Snapper (non Gekko Miniboss)
            return CanUseExplosive() || CanBeGoron();
        case MMA_EN_OKUTA: // Octorok
        case MMA_EN_EGOL:  // Eyegore
            return CanUseProjectile();
        case MMA_EN_BAGUO: // Nejiron
            return CanUseSword() || CanBeGoron() || CanBeZora() || Has(MMI_HOOKSHOT);
        case MMA_EN_NEO_REEBA: // Leever
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK);
        case MMA_EN_PP: // Hiploop
            return CanUseSword() || CanBeGoron() || CanBeZora() || (CanBeDeku() && HasMagic()) || Has(MMI_BOW) ||
                   Has(MMI_DEKU_STICK) || Has(MMI_HOOKSHOT);
        case MMA_EN_PR:  // Desbreko
        case MMA_EN_PR2: // Skull fish
        case MMA_EN_SB:  // Shellblade
            return CanBeZora() && HasMagic() && CanSwim();
        case MMA_BOSS_05: // Bio Deku Baba
            return CanBeZora() && CanSwim();
        case MMA_EN_BEE: // Giant Bee
            return CanUseSword() || CanBeGoron() || CanBeZora() || CanBeDeku() || Has(MMI_DEKU_STICK) ||
                   CanUseProjectile() || CanUseExplosive() || Has(MMI_DEKU_NUT);
        case MMA_EN_DRAGON: // Deep Python
            return CanBeZora() && HasMagic();
        case MMA_EN_BIGPO:
            return Has(MMI_BOW);
        case MMA_EN_PO_SISTERS:
            return CanUseProjectile() || Has(MMI_BOMB) || Has(MMI_BOMBCHU);
        case MMA_EN_INVADEPOH: // Them
            return Has(MMI_BOW);
        case MMA_EN_THIEFBIRD: // Takkuri
            return CanUseProjectile() || CanBeGoron() || CanBeZora() || CanUseExplosive() || CanUseSword();
        case MMA_EN_DEATH: // Gomess
            return CanUseSword() || Has(MMI_BOW) || Has(MMI_HOOKSHOT) || CanBeZora() || CanBeGoron() ||
                   (CanBeDeku() && HasMagic());
        default:
            return false;
    }
}

int TerminaLogic::MoonMaskCount() const {
    // ITEM_MASK_TRUTH .. ITEM_MASK_GIANT: the 20 masks that are not transformation masks.
    int count = 0;
    for (int item = MMI_MASK_TRUTH; item <= MMI_MASK_GIANT; item++) {
        count += Has((MMItem)item) ? 1 : 0;
    }
    return count;
}

int TerminaLogic::RemainsCount() const {
    int count = 0;
    for (int quest = MMQ_REMAINS_ODOLWA; quest <= MMQ_REMAINS_TWINMOLD; quest++) {
        count += Quest((MMQuest)quest) ? 1 : 0;
    }
    return count;
}

bool TerminaLogic::MeetsMoonRequirements() const {
    return RemainsCount() >= Option(MMO_ACCESS_MOON_REMAINS_COUNT) &&
           MoonMaskCount() >= Option(MMO_ACCESS_MOON_MASKS_COUNT);
}

bool TerminaLogic::CanDefeatMajora() const {
    // Majora's soul is shuffled with the boss souls (2Ship shouldMajoraRegister; with its Triforce hunt the pieces give
    // it instead, which Hyrule's Triforce hunt stands in for).
    const bool soul = !Option(MMO_SHUFFLE_BOSS_SOULS) || Option(MMO_SHUFFLE_TRIFORCE_PIECES) ||
                      Owns(MSI_MM_SOUL_BOSS_MAJORA);
    return soul && (CanUseSword() || CanBeZora() || CanBeGoron() || Has(MMI_BOW));
}

// ---- Clocks and time (Logic.h, TimeLogic.cpp) ------------------------------------------------------------------------

bool TerminaLogic::SettingClocks() const {
    return Option(MMO_CLOCK_SHUFFLE) != 0;
}

int TerminaLogic::ClockCount() const {
    int count = Count(MSI_MM_TIME_PROGRESSIVE);
    for (MsItemKey clock : kClockItems) {
        count += Owns(clock) ? 1 : 0;
    }
    return std::min(count, 6);
}

bool TerminaLogic::OwnsClockHalfDay(int halfDay) const {
    if (halfDay < 0 || halfDay >= 6) {
        return false;
    }
    // A progressive clock becomes the next half-day of its mode (ConvertItem.cpp), so its flags are the first (or
    // last) ones; a random clock is its own half-day.
    switch (Option(MMO_CLOCK_SHUFFLE_PROGRESSIVE)) {
        case MM_RO_CLOCK_SHUFFLE_ASCENDING:
            return ClockCount() > halfDay;
        case MM_RO_CLOCK_SHUFFLE_DESCENDING:
            return ClockCount() > 5 - halfDay;
        default:
            return Owns(kClockItems[halfDay]);
    }
}

bool TerminaLogic::OwnsHalfDayForMode(int halfDay) const {
    if (!SettingClocks() || halfDay < 0 || halfDay >= 6) {
        return !SettingClocks(); // If not shuffling clocks, all time is available
    }
    return OwnsClockHalfDay(halfDay);
}

bool TerminaLogic::IsTimeSliceOwned(int slice) const {
    if (!SettingClocks()) {
        return true;
    }
    for (int i = 0; i < 6; ++i) {
        if (slice >= kHalfDays[i].startSlice && slice <= kHalfDays[i].endSlice) {
            return OwnsClockHalfDay(i);
        }
    }
    return false;
}

uint64_t TerminaLogic::OwnedTimeSlices() const {
    if (!SettingClocks()) {
        return kTimeAllSlices;
    }
    uint64_t timeSlices = 0;
    for (int halfDay = 0; halfDay < 6; ++halfDay) {
        if (OwnsClockHalfDay(halfDay)) {
            timeSlices |= HalfDayMask(halfDay);
        }
    }
    // If no clocks are owned, ensure we at least have access to the start of the game (Day 1 6 AM)
    return timeSlices ? timeSlices : (kTimeBitOne << MMT_DAY1_AM_06_00);
}

uint64_t TerminaLogic::InitialTime() const {
    return SettingClocks() ? OwnedTimeSlices() : (kTimeBitOne << MMT_DAY1_AM_06_00);
}

uint64_t TerminaLogic::ExpandTimeForward(uint64_t timeSlices, const std::vector<Stay>& stays) const {
    // Fast path: unrestricted time expansion using bitwise fill (only when Clock Shuffle is off)
    if (stays.empty() && !SettingClocks()) {
        uint64_t expanded = timeSlices;
        expanded |= (expanded << 1);
        expanded |= (expanded << 2);
        expanded |= (expanded << 4);
        expanded |= (expanded << 8);
        expanded |= (expanded << 16);
        expanded |= (expanded << 32);
        return expanded & kTimeAllSlices;
    }

    // Slow path: sequential expansion; a failed stay restriction stops it for good
    uint64_t filtered = timeSlices;
    if (SettingClocks()) {
        filtered &= OwnedTimeSlices();
    }
    uint64_t expanded = filtered;
    bool canWait = false;
    for (int i = 0; i < MMT_COUNT; ++i) {
        const uint64_t mask = kTimeBitOne << i;
        if (filtered & mask) {
            canWait = true;
            expanded |= mask;
        } else if (canWait) {
            if (SettingClocks() && !IsTimeSliceOwned(i)) {
                canWait = false; // Can't expand into unowned time period
                continue;
            }
            bool restricted = false;
            bool allowed = true;
            for (const Stay& stay : stays) {
                if (stay.slice == i) {
                    restricted = true;
                    allowed = stay.condition();
                    break;
                }
            }
            if (!restricted || allowed) {
                expanded |= mask;
            } else {
                canWait = false; // Kicked out, STOP expansion
            }
        }
    }
    return expanded;
}

bool TerminaLogic::RawAt(MMTimeSlice slice) const {
    return (time & (kTimeBitOne << slice)) != 0;
}

bool TerminaLogic::RawBefore(MMTimeSlice slice) const {
    if (slice == 0) {
        return false;
    }
    return (time & ((kTimeBitOne << slice) - 1)) != 0;
}

bool TerminaLogic::RawAfter(MMTimeSlice slice) const {
    return (time & (~((kTimeBitOne << slice) - 1) & kTimeAllSlices)) != 0;
}

bool TerminaLogic::RawBetween(MMTimeSlice start, MMTimeSlice end) const {
    return (time & (((kTimeBitOne << end) - 1) & ~((kTimeBitOne << start) - 1))) != 0;
}

bool TerminaLogic::IsDay1() const {
    return RawBefore(MMT_NIGHT1_PM_06_00) && OwnsHalfDayForMode(0);
}
bool TerminaLogic::IsNight1() const {
    return RawBetween(MMT_NIGHT1_PM_06_00, MMT_DAY2_AM_06_00) && OwnsHalfDayForMode(1);
}
bool TerminaLogic::IsDay2() const {
    return RawBetween(MMT_DAY2_AM_06_00, MMT_NIGHT2_PM_06_00) && OwnsHalfDayForMode(2);
}
bool TerminaLogic::IsNight2() const {
    return RawBetween(MMT_NIGHT2_PM_06_00, MMT_DAY3_AM_06_00) && OwnsHalfDayForMode(3);
}
bool TerminaLogic::IsDay3() const {
    return RawBetween(MMT_DAY3_AM_06_00, MMT_NIGHT3_PM_06_00) && OwnsHalfDayForMode(4);
}
bool TerminaLogic::IsNight3() const {
    return RawAfter(MMT_NIGHT3_PM_06_00) && OwnsHalfDayForMode(5);
}

bool TerminaLogic::ClockFilter() const {
    if (!SettingClocks()) {
        return true;
    }
    return IsDay1() || IsNight1() || IsDay2() || IsNight2() || IsDay3() || IsNight3();
}

bool TerminaLogic::At(MMTimeSlice slice) const {
    return RawAt(slice) && ClockFilter();
}
bool TerminaLogic::Before(MMTimeSlice slice) const {
    return RawBefore(slice) && ClockFilter();
}
bool TerminaLogic::After(MMTimeSlice slice) const {
    return RawAfter(slice) && ClockFilter();
}
bool TerminaLogic::Between(MMTimeSlice start, MMTimeSlice end) const {
    return RawBetween(start, end) && ClockFilter();
}

bool TerminaLogic::IsDay() const {
    return IsDay1() || IsDay2() || IsDay3();
}
bool TerminaLogic::IsNight() const {
    return IsNight1() || IsNight2() || IsNight3();
}
bool TerminaLogic::FirstDay() const {
    return IsDay1() || IsNight1();
}
bool TerminaLogic::SecondDay() const {
    return IsDay2() || IsNight2();
}
bool TerminaLogic::FinalDay() const {
    return IsDay3() || IsNight3();
}
bool TerminaLogic::Midnight() const {
    return Between(MMT_NIGHT1_AM_12_00, MMT_DAY2_AM_06_00) || Between(MMT_NIGHT2_AM_12_00, MMT_DAY3_AM_06_00) ||
           After(MMT_NIGHT3_AM_12_00);
}

} // namespace MsTermina

// ---- Hooks ------------------------------------------------------------------------------------------------------------

using MsTermina::sLogic;

void MergeShip_TerminaLogic_Reset() {
    MsTermina::sUnified = MergeShip_Termina_Enabled();
    MsTermina::sCombined = MsTermina::sUnified && MergeShip_Goal_CombineItems();
    if (MsTermina::sUnified) {
        sLogic.Reset();
    }
}

bool MergeShip_TerminaLogic_ApplyItem(RandomizerGet rg, bool state) {
    if (!MsTermina::sUnified) {
        return false;
    }
    const MsItemKey key = MsItems_KeyOfRG(rg);
    const MsItemInfo* info = MsItems_Get(key);
    if (info == nullptr) {
        return false;
    }
    const bool foundInTermina = sLogic.itemSource == RW_TERMINA;
    // -1: applied without a location (the entrance validation's "all items", starting items): both worlds have it.
    const bool noLocation = sLogic.itemSource == -1;
    const bool consumable = info->persist == MS_CYCLE;
    // Each game's copy of an item both games have is a row of its own, usable only by its own game (MsItems.def's
    // columns; MsDelivery_OwnWorld), wherever it is found (the user, 2026-09-24: "Every item should be available at
    // every check in both games", and separate unless combined). With "Combine Shared Items" it is the other game's
    // copy too (MsDelivery_Counterpart): Termina counts MM's for Hyrule's, Hyrule Ocarina of Time's for MM's
    // (MergeShip_TerminaLogic_HyruleItem).
    const MsItemKey counterpart = MsTermina::sCombined ? MsDelivery_Counterpart(key) : MSI_MAX;
    // Termina counts what MM can use; a consumable only where a Termina check gave it (it goes to that engine).
    if (info->usableInMM && (!consumable || foundInTermina || noLocation)) {
        sLogic.ApplyItem(key, state);
    } else if ((counterpart < MSI_MAX) && MsItems_Get(counterpart)->usableInMM) {
        sLogic.ApplyItem(counterpart, state);
    }
    // Hyrule ignores what Ocarina of Time cannot use, and consumables Termina's checks gave.
    return !info->usableInOoT || (consumable && foundInTermina);
}

RandomizerGet MergeShip_TerminaLogic_HyruleItem(RandomizerGet rg) {
    if (!MsTermina::sCombined) {
        return RG_NONE;
    }
    const MsItemKey counterpart = MsDelivery_Counterpart(MsItems_KeyOfRG(rg));
    const MsItemInfo* info = (counterpart < MSI_MAX) ? MsItems_Get(counterpart) : nullptr;
    return ((info != nullptr) && info->usableInOoT) ? info->rg : RG_NONE;
}

void MergeShip_TerminaLogic_SetItemSource(int world) {
    sLogic.itemSource = world;
}

bool MergeShip_TerminaLogic_IsAdvancement(RandomizerGet rg) {
    const MsItemKey key = MsItems_KeyOfRG(rg);
    if (key >= MSI_MAX || !MsTermina::Tables().logicItem[key]) {
        return false;
    }
    const MsItemInfo* info = MsItems_Get(key);
    return info != nullptr && info->usableInMM &&
           ((info->persist == MS_PERM) || MsTermina::Tables().logicConsumable[key]) && MergeShip_Termina_Enabled();
}

bool MergeShip_TerminaLogic_EnterRegion(Region* region) {
    if (region->world != RW_TERMINA || !region->HasAccess()) {
        return false;
    }
    const uint64_t before = region->mmTime;
    if (region->randomizerRegionKey == RR_MM_ROOT) {
        // 2Ship's pseudo-root (save warp, owl warps): always the start of a cycle, and it cannot wait
        region->mmTime |= sLogic.InitialTime();
    } else if (region->mmTime != 0) {
        // 2Ship gives every visited region the owned time when a clock is found (GlitchlessLogic.cpp)
        if (sLogic.SettingClocks()) {
            region->mmTime |= sLogic.OwnedTimeSlices();
        }
        sLogic.time = region->mmTime;
        const std::vector<MsTermina::Stay>& stays = MsTermina::sStays.size() == RR_MAX
                                                        ? MsTermina::sStays[region->randomizerRegionKey]
                                                        : MsTermina::kNoStays;
        region->mmTime |= sLogic.ExpandTimeForward(region->mmTime, stays);
    }
    sLogic.time = region->mmTime;
    return region->mmTime != before;
}

void MergeShip_TerminaLogic_SetRegionTime(const Region* region) {
    if (region->world == RW_TERMINA) {
        sLogic.time = region->mmTime;
    }
}

bool MergeShip_TerminaLogic_PropagateTime(Region* parent, const Rando::Entrance& exit, Region* target, bool* closed) {
    if (closed != nullptr) {
        *closed = false;
    }
    const bool fromTermina = parent->world == RW_TERMINA;
    // Entering Termina from Hyrule is entering MM's cycle: its start (the Song of Time, or the moon, always brings it)
    sLogic.time = fromTermina ? parent->mmTime : sLogic.InitialTime();
    if (target->world != RW_TERMINA || !parent->HasAccess()) {
        return false;
    }
    uint64_t incoming =
        target->randomizerRegionKey == RR_MM_ROOT ? sLogic.InitialTime() : sLogic.time; // the root never carries time
    // Entrance shuffle: a shuffled door keeps its opening hours wherever it leads, and a house or shop behind it keeps
    // its own; only the slices at which both allow it get through (the current time is left as it was).
    if (fromTermina && (incoming != 0) && (target->randomizerRegionKey != RR_MM_ROOT)) {
        incoming = MergeShip_Entrances_OpenTime(exit, incoming);
        if (incoming == 0) {
            if (closed != nullptr) {
                *closed = true;
            }
            return false;
        }
    }
    if (incoming == 0 || (target->mmTime | incoming) == target->mmTime) {
        return false;
    }
    if (!exit.GetConditionsMet()) {
        return false;
    }
    target->mmTime |= incoming;
    return true;
}
