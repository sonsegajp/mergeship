#pragma once

// MergeShip2 -- Termina's logic inside SoH's generator (unified randomizer, milestone M4;
// Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md "Porting MM's logic into SoH").
//
// The world graph is generated: tools/port_mm_rando.py translates 2Ship's Logic/Regions/*.cpp into
// soh/Enhancements/randomizer/location_access/termina/*.cpp, rewriting 2Ship's logic macros into calls on `mm`
// (HAS_ITEM(ITEM_BOW) -> mm->Has(MMI_BOW), CAN_BE_DEKU -> mm->CanBeDeku(), RANDO_EVENTS[RE_X] -> logic->Get(LOGIC_MM_X),
// BETWEEN(TIME_A, TIME_B) -> mm->Between(MMT_A, MMT_B), ...). This file is the hand-written half: what those calls
// mean, as 2Ship's Logic.h defines them over MM's save context, re-expressed over the unified item model:
//   - State: how many of each canonical item (MsItems.def, MsItemKey) the search holds. An item counts for Termina only
//     when MM can use it (usableInMM); a consumable (MS_CYCLE) only when a Termina location gave it, since it goes to
//     the engine whose check gave it. Hyrule's logic likewise ignores items Ocarina of Time cannot use.
//   - Side effects of 2Ship's GiveItem/ConvertItem that its logic reads (week-event bits, RandoInf flags, owl flags,
//     dungeon keys, stray fairy and token counts, progressive levels) are derived from those counts.
//   - Events are SoH LogicVals (LOGIC_MM_*, generated), set by the regions' EVENT_ACCESS like Hyrule's.
//   - Time: every Termina region carries MM's 45-slice time mask (Region::mmTime). ProcessRegion (fill.cpp) runs the
//     ported ExpandTimeForward on it and sets the current time before evaluating the region's conditions; exits carry
//     the mask on (2Ship's FindReachableRegions). Hyrule's regions never touch it.
// The enumerations below name the 2Ship identifiers the regions use; tools/port_mm_rando.py --check fails when 2Ship's
// logic starts using one that is not here, so upstream changes are caught instead of silently mistranslated.

#include <cstdint>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "MergeShipMsItems.h"
#include "MergeShipTerminaLogicGen.h" // generated: MMTimeSlice (MMT_*), MMOption (MMO_*), MM_RO_* option values

class Region;
namespace Rando {
class Entrance;
}

namespace MsTermina {

// 2Ship ItemId (ITEM_X -> MMI_X): what HAS_ITEM asks about.
enum MMItem {
    MMI_OCARINA_OF_TIME,
    MMI_BOW,
    MMI_ARROW_FIRE,
    MMI_ARROW_ICE,
    MMI_ARROW_LIGHT,
    MMI_BOMB,
    MMI_BOMBCHU,
    MMI_DEKU_STICK,
    MMI_DEKU_NUT,
    MMI_MAGIC_BEANS,
    MMI_POWDER_KEG,
    MMI_PICTOGRAPH_BOX,
    MMI_LENS_OF_TRUTH,
    MMI_HOOKSHOT,
    MMI_SWORD_GREAT_FAIRY,
    MMI_ROOM_KEY,
    MMI_MASK_DEKU,
    MMI_MASK_GORON,
    MMI_MASK_ZORA,
    MMI_MASK_FIERCE_DEITY,
    MMI_MASK_TRUTH,
    MMI_MASK_KAFEIS_MASK,
    MMI_MASK_ALL_NIGHT,
    MMI_MASK_BUNNY,
    MMI_MASK_KEATON,
    MMI_MASK_GARO,
    MMI_MASK_ROMANI,
    MMI_MASK_CIRCUS_LEADER,
    MMI_MASK_POSTMAN,
    MMI_MASK_COUPLE,
    MMI_MASK_GREAT_FAIRY,
    MMI_MASK_GIBDO,
    MMI_MASK_DON_GERO,
    MMI_MASK_KAMARO,
    MMI_MASK_CAPTAIN,
    MMI_MASK_STONE,
    MMI_MASK_BREMEN,
    MMI_MASK_BLAST,
    MMI_MASK_SCENTS,
    MMI_MASK_GIANT,
    MMI_COUNT
};

// 2Ship ActorId (ACTOR_X -> MMA_X): what CanKillEnemy asks about.
enum MMActor {
    MMA_BOSS_01, // Odolwa
    MMA_BOSS_02, // Twinmold
    MMA_BOSS_03, // Gyorg
    MMA_BOSS_04, // Wart
    MMA_BOSS_05, // Bio Deku Baba
    MMA_BOSS_HAKUGIN, // Goht
    MMA_EN_AM,
    MMA_EN_BAGUO,
    MMA_EN_BAT,
    MMA_EN_BB,
    MMA_EN_BBFALL,
    MMA_EN_BEE,
    MMA_EN_BIGPAMET,
    MMA_EN_BIGPO,
    MMA_EN_BIGSLIME,
    MMA_EN_BSB,
    MMA_EN_CROW,
    MMA_EN_DEATH,
    MMA_EN_DEKUBABA,
    MMA_EN_DEKUNUTS,
    MMA_EN_DINOFOS,
    MMA_EN_DODONGO,
    MMA_EN_DRAGON,
    MMA_EN_EGOL,
    MMA_EN_FAMOS,
    MMA_EN_FIREFLY,
    MMA_EN_FLOORMAS,
    MMA_EN_FZ,
    MMA_EN_GRASSHOPPER,
    MMA_EN_IK,
    MMA_EN_INVADEPOH,
    MMA_EN_JSO,
    MMA_EN_JSO2,
    MMA_EN_KAIZOKU,
    MMA_EN_KAME,
    MMA_EN_KAREBABA,
    MMA_EN_KNIGHT,
    MMA_EN_MKK,
    MMA_EN_NEO_REEBA,
    MMA_EN_OKUTA,
    MMA_EN_PAMETFROG,
    MMA_EN_PEEHAT,
    MMA_EN_PO_SISTERS,
    MMA_EN_PP,
    MMA_EN_PR,
    MMA_EN_PR2,
    MMA_EN_RAT,
    MMA_EN_RD,
    MMA_EN_RR,
    MMA_EN_SB,
    MMA_EN_SKB,
    MMA_EN_SLIME,
    MMA_EN_SNOWMAN,
    MMA_EN_ST,
    MMA_EN_SW,
    MMA_EN_THIEFBIRD,
    MMA_EN_TITE,
    MMA_EN_TUBO_TRAP,
    MMA_EN_VM,
    MMA_EN_WALLMAS,
    MMA_EN_WDHAND,
    MMA_EN_WF,
    MMA_EN_WIZ,
    MMA_OBJ_SNOWBALL,
    MMA_COUNT
};

// CAN_PLAY_SONG's argument (QUEST_SONG_X without the prefix -> MMSONG_X).
enum MMSong {
    MMSONG_SONATA,
    MMSONG_LULLABY,
    MMSONG_BOSSA_NOVA,
    MMSONG_ELEGY,
    MMSONG_OATH,
    MMSONG_SARIA,
    MMSONG_TIME,
    MMSONG_HEALING,
    MMSONG_EPONA,
    MMSONG_SOARING,
    MMSONG_STORMS,
    MMSONG_SUN,
    MMSONG_LULLABY_INTRO,
    MMSONG_COUNT
};

// 2Ship OcarinaSongId (OCARINA_SONG_X -> MMOS_X), in MM's order: canPlaySong's argument (which buttons a song needs).
enum MMOcarinaSong {
    MMOS_SONATA,
    MMOS_GORON_LULLABY,
    MMOS_NEW_WAVE,
    MMOS_ELEGY,
    MMOS_OATH,
    MMOS_SARIAS,
    MMOS_TIME,
    MMOS_HEALING,
    MMOS_EPONAS,
    MMOS_SOARING,
    MMOS_STORMS,
    MMOS_SUNS,
    MMOS_INVERTED_TIME,
    MMOS_DOUBLE_TIME,
    MMOS_GORON_LULLABY_INTRO,
    MMOS_WIND_FISH_HUMAN,
    MMOS_WIND_FISH_GORON,
    MMOS_WIND_FISH_ZORA,
    MMOS_WIND_FISH_DEKU,
    MMOS_EVAN_PART1,
    MMOS_EVAN_PART2,
    MMOS_ZELDAS_LULLABY,
    MMOS_SCARECROW_SPAWN,
    MMOS_TERMINA_WALL,
    MMOS_COUNT
};

// DUNGEON_SCENE_INDEX_X / KEY_COUNT(X) -> MMD_X.
enum MMDungeon {
    MMD_WOODFALL_TEMPLE,
    MMD_SNOWHEAD_TEMPLE,
    MMD_GREAT_BAY_TEMPLE,
    MMD_STONE_TOWER_TEMPLE,
    MMD_COUNT
};

// CHECK_QUEST_ITEM(QUEST_X) -> MMQ_X.
enum MMQuest {
    MMQ_REMAINS_ODOLWA,
    MMQ_REMAINS_GOHT,
    MMQ_REMAINS_GYORG,
    MMQ_REMAINS_TWINMOLD,
    MMQ_COUNT
};

// CHECK_WEEKEVENTREG(WEEKEVENTREG_X) -> MMW_X.
enum MMWeekEvent {
    MMW_08_80, // the Clock Town stray fairy was returned
    MMW_COUNT
};

// Flags_GetRandoInf(RANDO_INF_X) -> MMRI_X.
enum MMRandoInf {
    MMRI_OBTAINED_DEED_LAND,
    MMRI_OBTAINED_DEED_MOUNTAIN,
    MMRI_OBTAINED_DEED_OCEAN,
    MMRI_OBTAINED_DEED_SWAMP,
    MMRI_OBTAINED_LETTER_TO_KAFEI,
    MMRI_OBTAINED_LETTER_TO_MAMA,
    MMRI_OBTAINED_MOONS_TEAR,
    MMRI_OBTAINED_PENDANT_OF_MEMORIES,
    MMRI_OBTAINED_ROOM_KEY,
    MMRI_OBTAINED_SOUL_OF_ENEMY_CHUCHUS,
    MMRI_OBTAINED_SOUL_OF_ENEMY_OCTOROKS,
    MMRI_COUNT
};

// CAN_OWL_WARP(OWL_WARP_X) -> MMOWL_X.
enum MMOwl {
    MMOWL_GREAT_BAY_COAST,
    MMOWL_ZORA_CAPE,
    MMOWL_SNOWHEAD,
    MMOWL_MOUNTAIN_VILLAGE,
    MMOWL_CLOCK_TOWN,
    MMOWL_MILK_ROAD,
    MMOWL_WOODFALL,
    MMOWL_SOUTHERN_SWAMP,
    MMOWL_IKANA_CANYON,
    MMOWL_STONE_TOWER,
    MMOWL_COUNT
};

// HAS_ENOUGH_SKULLTULA_TOKENS(SCENE_KINSTA1 / SCENE_KINDAN2).
enum MMSpiderHouse {
    MMSH_SWAMP, // SCENE_KINSTA1
    MMSH_OCEAN, // SCENE_KINDAN2
    MMSH_COUNT
};

// GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) values (EQUIP_VALUE_SHIELD_X -> MM_SHIELD_X).
constexpr int MM_SHIELD_NONE = 0;
constexpr int MM_SHIELD_HERO = 1;
constexpr int MM_SHIELD_MIRROR = 2;

typedef bool (*Condition)();

// A region's STAY(timeSlice, condition): when the player may wait there into timeSlice (2Ship timeStayRestrictions).
struct Stay {
    MMTimeSlice slice;
    Condition condition;
};

// Termina's item state for one search (reset with SoH's Logic), and 2Ship's logic helpers over it.
class TerminaLogic {
  public:
    void Reset();
    void ApplyItem(MsItemKey key, bool add);
    uint16_t Count(MsItemKey key) const {
        return key < MSI_MAX ? mCount[key] : 0;
    }
    bool Owns(MsItemKey key) const {
        return Count(key) > 0;
    }

    // 2Ship's options for the seed being generated (RANDO_SAVE_OPTIONS[RO_X] -> Option(MMO_X)).
    int Option(MMOption option) const;

    // Items (Logic.h)
    bool Has(MMItem item) const;
    bool CanBeDeku() const;
    bool CanBeGoron() const;
    bool CanBeZora() const;
    bool CanBeDeity() const;
    bool HasMagic() const;
    bool HasBottle() const;
    bool HasBottleItem(MMItem item) const;
    bool CheckMaxHp(int hearts) const;
    int SwordLevel() const; // 0 none, 1 Kokiri, 2 Razor, 3 Gilded
    int ShieldLevel() const;
    int WalletLevel() const; // CUR_UPG_VALUE(UPG_WALLET): 0 child, 1 adult, 2 giant, 3 tycoon
    bool CanUseHumanSword() const;
    bool CanUseSword() const;
    bool CanUseExplosive() const;
    bool CanUseProjectile() const;
    bool CanFullyCutKeatonGrass() const;
    bool CanHookScarecrow() const;
    bool CanLightTorchNearAnother() const;
    bool CanGrowBeanPlant() const;
    bool CanUseDay2RainBean() const;
    bool CanRideEpona() const;
    bool CanSwim() const; // CAN_USE_ABILITY(SWIM)
    bool CanUseMagicArrow(MMItem arrow) const;
    bool CanPlaySong(MMSong song) const;
    bool CanPlayOcarinaSong(MMOcarinaSong song) const; // canPlaySong: the ocarina buttons a song needs
    bool OcarinaButton(int button) const;              // 0 A, 1 C-down, 2 C-right, 3 C-left, 4 C-up
    bool FoundAllFrogs() const;
    bool GbtCanReverseWaterFlow() const;
    bool GbtGreenSwitchFlow() const;
    bool CanAfford(RandomizerCheck rc) const;
    bool CanOwlWarp(MMOwl owl) const;
    bool Quest(MMQuest quest) const;
    bool BossKey(MMDungeon dungeon) const;
    int KeyCount(MMDungeon dungeon) const;
    bool WeekEvent(MMWeekEvent event) const;
    bool RandoInf(MMRandoInf flag) const;
    bool HasEnoughStrayFairies(MMDungeon dungeon) const;
    bool HasEnoughSkulltulaTokens(MMSpiderHouse house) const;
    bool CanAccessDungeon(MMDungeon dungeon) const;
    bool HaveEnemySoul(MMActor actor) const;
    bool CanKillEnemy(MMActor actor) const;
    int MoonMaskCount() const;
    int RemainsCount() const;
    bool MeetsMoonRequirements() const;
    bool CanDefeatMajora() const; // MergeShip2: the unified randomizer's Majora goal (2Ship has no check for it)

    // Clocks (clock shuffle) and time (Logic.h, TimeLogic.cpp)
    bool SettingClocks() const;
    int ClockCount() const;
    bool OwnsClockHalfDay(int halfDay) const;
    bool OwnsHalfDayForMode(int halfDay) const; // CLOCK_DAY1() .. CLOCK_NIGHT3()
    bool IsTimeSliceOwned(int slice) const;
    uint64_t OwnedTimeSlices() const;
    uint64_t InitialTime() const; // the time Termina is entered at: Day 1 06:00 (clock shuffle: the owned time)
    uint64_t ExpandTimeForward(uint64_t timeSlices, const std::vector<Stay>& stays) const;

    bool RawAt(MMTimeSlice slice) const;
    bool RawBefore(MMTimeSlice slice) const;
    bool RawAfter(MMTimeSlice slice) const;
    bool RawBetween(MMTimeSlice start, MMTimeSlice end) const;
    bool ClockFilter() const;
    bool At(MMTimeSlice slice) const;
    bool Before(MMTimeSlice slice) const;
    bool After(MMTimeSlice slice) const;
    bool Between(MMTimeSlice start, MMTimeSlice end) const;
    bool IsDay1() const;
    bool IsNight1() const;
    bool IsDay2() const;
    bool IsNight2() const;
    bool IsDay3() const;
    bool IsNight3() const;
    bool IsDay() const;
    bool IsNight() const;
    bool FirstDay() const;
    bool SecondDay() const;
    bool FinalDay() const;
    bool Midnight() const;

    uint64_t time = 0;      // gCurrentRegionTime: the time mask of the region being evaluated
    int itemSource = -1;    // RandomizerWorld of the location whose item is being applied; -1 unknown (assumed)

  private:
    uint16_t mCount[MSI_MAX + 1] = {};
};

// The one instance the generated regions call.
extern TerminaLogic* mm;

// Generated regions (location_access/termina/root.cpp): start of RegionTable_Init_Termina, and each region's STAYs.
void BeginRegionTable();
void SetStays(RandomizerRegion region, std::vector<Stay> stays);

} // namespace MsTermina

// ---- Hooks into SoH's generator (each a no-op without the unified mode) --------------------------------------------
// logic.cpp Logic::Reset: reset Termina's item state with Hyrule's.
void MergeShip_TerminaLogic_Reset();
// logic.cpp Logic::ApplyItemEffect: apply an item to Termina's state. True when Hyrule's logic must ignore it (Ocarina of
// Time cannot use it, or it is a consumable a Termina location gave).
bool MergeShip_TerminaLogic_ApplyItem(RandomizerGet rg, bool state);
// logic.cpp Logic::ApplyItemEffect, for an item Hyrule's logic ignores: with "Combine Shared Items", the Ocarina of Time
// copy Hyrule's logic counts for Majora's Mask's copy `rg` (MsDelivery_Counterpart), else RG_NONE.
RandomizerGet MergeShip_TerminaLogic_HyruleItem(RandomizerGet rg);
// item_location.cpp ItemLocation::ApplyPlacedItemEffect: the world of the location whose item is applied (-1: none).
void MergeShip_TerminaLogic_SetItemSource(int world);
// item.cpp Item::IsAdvancement: an item Termina's logic reads is progression in a unified seed.
bool MergeShip_TerminaLogic_IsAdvancement(RandomizerGet rg);
// fill.cpp ProcessRegion: a Termina region's time expands (ExpandTimeForward) and becomes the current time. True when
// its time grew.
bool MergeShip_TerminaLogic_EnterRegion(Region* region);
// fill.cpp ProcessRegion: the region's time is the current time again (after exits processed other regions).
void MergeShip_TerminaLogic_SetRegionTime(const Region* region);
// fill.cpp ProcessExits: MM time flows through an exit into a Termina region (from Hyrule: the entry time). Leaves the
// parent's time current for the exit's own evaluation. True when the target's time grew.
// A shuffled door of Termina lets only the time slices through at which it is open (MergeShip_Entrances_OpenTime);
// *closed (when given) is set when it is open at none of the parent's: the exit gives no access at all then.
bool MergeShip_TerminaLogic_PropagateTime(Region* parent, const Rando::Entrance& exit, Region* target,
                                          bool* closed = nullptr);
