// MergeShip2 -- Majora's Mask's equipment, items, songs, quest items and stray fairies as randomizer items (see
// MergeShipRandoItems.h). The wiring follows the masks' (MergeShipMaskItems.cpp): RandomizerGet, a found flag
// (RAND_INF_OBTAINED_MM_*), the item-receive hook that gives the item, and a re-grant from the flags on load.
//
// Decisions (2S2H's rando, mm/2s2h/Rando, is the reference for the progressive items):
//  * Swords: RG_PROGRESSIVE_MM_SWORD, 2S2H's RI_PROGRESSIVE_SWORD without its first step, which is Ocarina of Time's
//    own Kokiri Sword (RG_KOKIRI_SWORD): 1st copy the Razor Sword, 2nd the Gilded Sword. Both stay owned (the pause
//    screen switches the Kokiri Sword slot between them). The Razor Sword never wears out in a seed (MM equipment
//    module, as 2S2H).
//  * Goron Lullaby: RG_PROGRESSIVE_GORON_LULLABY, 2S2H's RI_PROGRESSIVE_LULLABY: the Intro, then the full Lullaby.
//  * Logic: the Hookshot (MM) counts as the Hookshot and the Hero's Bow as the Fairy Bow (logic.cpp HasItem). Owning
//    either fills the Ocarina of Time slot (ITEM_HOOKSHOT / ITEM_BOW with the first quiver) even with the MM
//    equipment option off, so Ocarina of Time's item is usable -- by adult Link, which is all CanUse assumes. The MM
//    Hookshot's Longshot reach and both items' child use need the option and mm.o2r, so logic never counts them as
//    the Longshot or for child. Nothing else here is needed by logic (LOGIC_NONE).
//  * Bottles: "Bottle with X" is a new bottle only while the Ocarina of Time bottles still to be found in the seed
//    (Ruto's Letter, which logic needs, among them) leave a free bottle slot; otherwise X fills an empty bottle, and
//    with neither it is only recorded as found. OoT has four bottle slots, so an MM bottle must never take the slot an
//    Ocarina of Time bottle is counted on.
//  * Trade items: MM keeps one slot per trade chain (Moon's Tear .. Ocean Title Deed, Room Key / Letter to Mama,
//    Letter to Kafei / Pendant of Memories) and each new one replaces the last. A seed hands them out in any order, so
//    the slot keeps the furthest of its chain found.
//  * Stray fairies: counted in their area (as MergeShip_StrayFairy_GiveItem). A pot, crate or chest whose check holds
//    one holds the fairy (MergeShip_StrayFairy_AttachToContainer) and the check is the fairy's touch (the collect
//    override below). The count lives in the MergeShip save; there is no re-grant of counts.

#include "soh/MergeShip/MergeShipRandoItems.h"
#include "soh/MergeShip/MergeShipUnified.h"

#include <unordered_map>

#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipMaskItems.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipOwnership.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipChateau.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipStrayFairy.h"
#include "soh/MergeShip/MergeShipTransformFx.h" // MergeShip_PlayMMSfxActor, MM_NA_SE_*
#include "mm/objects/object_gi_sword_2/object_gi_sword_2.h"
#include "mm/objects/object_gi_sword_3/object_gi_sword_3.h"
#include "mm/objects/object_gi_sword_4/object_gi_sword_4.h"
#include "mm/objects/object_gi_shield_2/object_gi_shield_2.h"
#include "mm/objects/object_gi_shield_3/object_gi_shield_3.h"
#include "mm/objects/object_gi_hookshot/object_gi_hookshot.h"
#include "mm/objects/object_gi_bow/object_gi_bow.h"
#include "mm/objects/object_gi_melody/object_gi_melody.h"
extern PlayState* gPlayState;
}

// hook_handlers.cpp: the check a flag belongs to.
RandomizerCheck GetRandomizerCheckFromFlag(int16_t flagType, int16_t flag);
RandomizerCheck GetRandomizerCheckFromSceneFlag(int16_t sceneNum, int16_t flagType, int16_t flag);

namespace {

// OPEN_DISPS / CLOSE_DISPS redeclare these at block scope; declared C here so that, inside this namespace, they
// still name the C functions in frame_interpolation.cpp.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

// ---- the items ----------------------------------------------------------------------------------------------------

bool IsMMRandoItem(int rg) {
    return (rg >= RG_PROGRESSIVE_MM_SWORD) && (rg <= RG_MM_STONE_TOWER_STRAY_FAIRY);
}

bool IsStrayFairyRG(int rg) {
    return (rg >= RG_MM_CLOCK_TOWN_STRAY_FAIRY) && (rg <= RG_MM_STONE_TOWER_STRAY_FAIRY);
}

bool FlagSet(RandomizerInf flag) {
    return Flags_GetRandomizerInf(flag) != 0;
}

// The found flag of a one-copy item (logic.cpp RandoGetToRandInf).
bool FoundFlagSet(RandomizerGet rg) {
    auto it = Rando::StaticData::RandoGetToRandInf.find(rg);
    return (it != Rando::StaticData::RandoGetToRandInf.end()) && FlagSet((RandomizerInf)it->second);
}

// The equipment's MM item (RG_MM_GREAT_FAIRYS_SWORD .. RG_MM_HOOKSHOT).
u8 EquipItemOf(int rg) {
    switch (rg) {
        case RG_MM_GREAT_FAIRYS_SWORD:
            return ITEM_MM_SWORD_GREAT_FAIRY;
        case RG_MM_HEROS_SHIELD:
            return ITEM_MM_SHIELD_HERO;
        case RG_MM_MIRROR_SHIELD:
            return ITEM_MM_SHIELD_MIRROR;
        case RG_MM_HEROS_BOW:
            return ITEM_MM_BOW_HERO;
        case RG_MM_HOOKSHOT:
            return ITEM_MM_HOOKSHOT;
        default:
            return ITEM_NONE;
    }
}

// The song or quest item of RG_MM_SONATA_OF_AWAKENING .. RG_MM_BOMBERS_NOTEBOOK (not the progressive lullaby).
u8 SongItemOf(int rg) {
    switch (rg) {
        case RG_MM_SONATA_OF_AWAKENING:
            return ITEM_MM_SONG_SONATA;
        case RG_MM_NEW_WAVE_BOSSA_NOVA:
            return ITEM_MM_SONG_NEW_WAVE;
        case RG_MM_ELEGY_OF_EMPTINESS:
            return ITEM_MM_SONG_ELEGY;
        case RG_MM_OATH_TO_ORDER:
            return ITEM_MM_SONG_OATH;
        case RG_MM_SONG_OF_HEALING:
            return ITEM_MM_SONG_HEALING;
        case RG_MM_SONG_OF_SOARING:
            return ITEM_MM_SONG_SOARING;
        case RG_MM_ODOLWAS_REMAINS:
        case RG_MM_GOHTS_REMAINS:
        case RG_MM_GYORGS_REMAINS:
        case RG_MM_TWINMOLDS_REMAINS:
            return (u8)(ITEM_MM_REMAINS_ODOLWA + (rg - RG_MM_ODOLWAS_REMAINS));
        case RG_MM_BOMBERS_NOTEBOOK:
            return ITEM_MM_BOMBERS_NOTEBOOK;
        default:
            return ITEM_NONE;
    }
}

// ---- equipment ----------------------------------------------------------------------------------------------------

// The progressive sword's copies found so far (the flags) that the MM equipment save lacks. Once the Gilded Sword is
// owned, a Razor Sword missing from the save is not given back (it would become the selected sword again).
void GiveMMSwords(PlayState* play) {
    if (FlagSet(RAND_INF_OBTAINED_MM_RAZOR_SWORD) && !MergeShip_MMEquip_Owns(MMEQUIP_RAZOR) &&
        !MergeShip_MMEquip_Owns(MMEQUIP_GILDED)) {
        MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_RAZOR);
    }
    if (FlagSet(RAND_INF_OBTAINED_MM_GILDED_SWORD) && !MergeShip_MMEquip_Owns(MMEQUIP_GILDED)) {
        MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_GILDED);
    }
}

void GiveMMEquip(PlayState* play, int rg) {
    u8 item = EquipItemOf(rg);
    if ((item != ITEM_NONE) && !MergeShip_MMEquip_Owns(MERGESHIP_MMEQUIP_ID(item))) {
        MergeShip_MMEquip_Give(play, item);
    }
}

// ---- bottles ------------------------------------------------------------------------------------------------------

// Bottles of Ocarina of Time's (Empty Bottle .. Bottle with Big Poe, Ruto's Letter among them) placed in the seed and
// not found yet: each needs a free bottle slot when it is found.
int PendingOoTBottles() {
    auto ctx = Rando::Context::GetInstance();
    int pending = 0;
    for (int rc = 1; rc < RC_MAX; rc++) {
        Rando::ItemLocation* loc = ctx->GetItemLocation((RandomizerCheck)rc);
        if (loc == nullptr) {
            continue;
        }
        RandomizerGet placed = loc->GetPlacedRandomizerGet();
        if ((placed >= RG_EMPTY_BOTTLE) && (placed <= RG_BOTTLE_WITH_BIG_POE) && !loc->HasObtained()) {
            pending++;
        }
    }
    return pending;
}

// MergeShipMMItems.c's button mapping: every C / D-pad button that holds bottle slot `slot` shows `item`.
void UpdateBottleButtons(PlayState* play, s16 slot, u8 item) {
    for (s32 button = 1; button < (s32)ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
        if (gSaveContext.equips.cButtonSlots[button - 1] == slot) {
            gSaveContext.equips.buttonItems[button] = item;
            if (play != nullptr) {
                Interface_LoadItemIcon1(play, button);
            }
            gSaveContext.buttonStatus[(button >= 4) ? (button + 1) : button] = BTN_ENABLED;
        }
    }
}

// "Bottle with X": a new bottle while Ocarina of Time's bottles still to come leave a slot free, else X into an empty
// bottle (see the top of the file). Without the MM items option (or mm.o2r), X cannot be used: an empty bottle, on the
// same terms.
void GiveMMBottle(PlayState* play, u8 content) {
    bool usable = (content == ITEM_MM_CHATEAU) ? (MergeShip_IsMajorasMaskMounted() != 0)
                                               : (MergeShip_MMItems_Enabled() != 0);
    if (!usable) {
        content = ITEM_BOTTLE;
    }
    // Majora's Mask's own bottle (MergeShipOwnership.h): one of its six, on the MM item menu and usable here -- never
    // one of Ocarina of Time's four.
    if (MergeShip_Own_MMBottlesHere()) {
        if (MergeShip_Own_GiveMMBottle(content)) {
            SPDLOG_INFO("MergeShip: MM bottle 0x{:02X}, one of Majora's Mask's own", content);
        } else {
            SPDLOG_INFO("MergeShip: MM bottle 0x{:02X} found, Majora's Mask has six", content);
        }
        return;
    }

    int freeSlots = 0;
    for (s16 i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
            freeSlots++;
        }
    }
    if (freeSlots > PendingOoTBottles()) {
        for (s16 i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = content;
                if (content == ITEM_MM_HOT_SPRING_WATER) {
                    MergeShip_MMItems_StartBottleTimer(i);
                }
                SPDLOG_INFO("MergeShip: MM bottle 0x{:02X} in a new bottle (slot {})", content, i);
                return;
            }
        }
    }
    if (content == ITEM_BOTTLE) {
        SPDLOG_INFO("MergeShip: MM bottle found, no bottle slot to spare");
        return;
    }
    for (s16 i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = content;
            if (content == ITEM_MM_HOT_SPRING_WATER) {
                MergeShip_MMItems_StartBottleTimer(i);
            }
            UpdateBottleButtons(play, (s16)(SLOT_BOTTLE_1 + i), content);
            SPDLOG_INFO("MergeShip: MM bottle 0x{:02X} poured into empty bottle {}", content, i);
            return;
        }
    }
    SPDLOG_INFO("MergeShip: MM bottle 0x{:02X} found, no free or empty bottle", content);
}

// ---- trade items --------------------------------------------------------------------------------------------------

// The items of MergeShipMMItems.h and the stray fairies are written to their save data here, as their modules'
// Item_Give branches (MergeShip_MMItems_Give, MergeShip_StrayFairy_GiveItem) write it, but without those branches'
// item-receive hooks: this runs inside the randomizer item's own receive hooks, and a second, nested receive of the
// MM item would be seen as another item (Anchor would hand it to the other players a second time).

// MergeShip_MMItems_Give's keg and Pictograph Box branches.
void GiveKegOrPictoBox(u8 item) {
    if (item == ITEM_MM_POWDER_KEG) {
        gMergeShipMMItemsSave.powderKeg = ITEM_MM_POWDER_KEG;
        gMergeShipMMItemsSave.powderKegAmmo = MERGESHIP_POWDER_KEG_MAX_AMMO;
    } else if (item == ITEM_MM_PICTOGRAPH_BOX) {
        gMergeShipMMItemsSave.pictographBox = ITEM_MM_PICTOGRAPH_BOX;
    }
}

struct TradeChain {
    u8 MergeShipMMItemsSave::*slot;
    u8 first;
    u8 last;
};
const TradeChain kTradeChains[] = {
    { &MergeShipMMItemsSave::tradeDeed, ITEM_MM_MOONS_TEAR, ITEM_MM_DEED_OCEAN },
    { &MergeShipMMItemsSave::tradeKeyMama, ITEM_MM_ROOM_KEY, ITEM_MM_LETTER_TO_MAMA },
    { &MergeShipMMItemsSave::tradeCouple, ITEM_MM_LETTER_TO_KAFEI, ITEM_MM_PENDANT_OF_MEMORIES },
};

const TradeChain* ChainOf(u8 item) {
    for (const TradeChain& chain : kTradeChains) {
        if ((item >= chain.first) && (item <= chain.last)) {
            return &chain;
        }
    }
    return nullptr;
}

// MergeShip_MMItems_Give's trade branch (MM: the slot takes the new item, and a button that held the slot's previous
// item holds this one), unless the slot holds a later item of the same chain.
void GiveTradeItem(PlayState* play, u8 item) {
    const TradeChain* chain = ChainOf(item);
    if (chain == nullptr) {
        return;
    }
    u8& slot = gMergeShipMMItemsSave.*(chain->slot);
    u8 previous = slot;
    gMergeShipMMItemsSave.tradeOwned |= MERGESHIP_MM_TRADE_BIT(item); // every one found stays owned (A cycles)
    if ((previous != ITEM_NONE) && (previous >= item) && (previous <= chain->last)) {
        return;
    }
    slot = item;
    if (previous == ITEM_NONE) {
        return;
    }
    for (s32 button = 1; button < (s32)ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
        if (gSaveContext.equips.buttonItems[button] == previous) {
            gSaveContext.equips.buttonItems[button] = item;
            if (play != nullptr) {
                Interface_LoadItemIcon1(play, button);
            }
        }
    }
}

// MergeShip_StrayFairy_GiveItem's count (MM EnElforg_FreeFloating: strayFairies[area]++, Clock Town's one a flag).
void GiveStrayFairy(s32 area) {
    if ((area < MM_STRAY_FAIRY_AREA_CLOCK_TOWN) || (area >= MM_STRAY_FAIRY_AREA_MAX)) {
        return;
    }
    if (area == MM_STRAY_FAIRY_AREA_CLOCK_TOWN) {
        gMergeShipSave.strayFairies[area] = 1;
    } else if (gMergeShipSave.strayFairies[area] < 0xFF) {
        gMergeShipSave.strayFairies[area]++;
    }
}

// ---- songs --------------------------------------------------------------------------------------------------------

void GiveSongItem(u8 item) {
    if ((item != ITEM_NONE) && !MergeShip_Songs_HasItem(item)) {
        MergeShip_Songs_GiveItem(item);
    }
}

void GiveLullabies() {
    if (FlagSet(RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO)) {
        GiveSongItem(ITEM_MM_SONG_GORON_LULLABY_INTRO);
    }
    if (FlagSet(RAND_INF_OBTAINED_MM_GORON_LULLABY)) {
        GiveSongItem(ITEM_MM_SONG_GORON_LULLABY);
    }
}

// ---- receiving ----------------------------------------------------------------------------------------------------

void GiveRandoItem(PlayState* play, int rg) {
    if (rg == RG_PROGRESSIVE_MM_SWORD) {
        GiveMMSwords(play);
    } else if ((rg >= RG_MM_GREAT_FAIRYS_SWORD) && (rg <= RG_MM_HOOKSHOT)) {
        GiveMMEquip(play, rg);
    } else if (rg == RG_MM_POWDER_KEG) {
        GiveKegOrPictoBox(ITEM_MM_POWDER_KEG);
    } else if (rg == RG_MM_PICTOGRAPH_BOX) {
        GiveKegOrPictoBox(ITEM_MM_PICTOGRAPH_BOX);
    } else if ((rg >= RG_MM_BOTTLE_WITH_DEKU_PRINCESS) && (rg <= RG_MM_BOTTLE_WITH_HOT_SPRING_WATER)) {
        GiveMMBottle(play, (u8)(ITEM_MM_DEKU_PRINCESS + (rg - RG_MM_BOTTLE_WITH_DEKU_PRINCESS)));
    } else if (rg == RG_MM_BOTTLE_WITH_CHATEAU_ROMANI) {
        GiveMMBottle(play, ITEM_MM_CHATEAU);
    } else if ((rg >= RG_MM_MOONS_TEAR) && (rg <= RG_MM_PENDANT_OF_MEMORIES)) {
        GiveTradeItem(play, (u8)(ITEM_MM_MOONS_TEAR + (rg - RG_MM_MOONS_TEAR)));
    } else if (rg == RG_PROGRESSIVE_GORON_LULLABY) {
        GiveLullabies();
    } else if ((rg >= RG_MM_SONATA_OF_AWAKENING) && (rg <= RG_MM_BOMBERS_NOTEBOOK)) {
        GiveSongItem(SongItemOf(rg));
    } else if (IsStrayFairyRG(rg)) {
        GiveStrayFairy(rg - RG_MM_CLOCK_TOWN_STRAY_FAIRY);
    }
}

// Every randomizer item found (Randomizer_Item_Give has set its flag). Item_Give's own receive entries for these
// items are MOD_NONE and not handled here.
void OnItemReceive(GetItemEntry entry) {
    if ((entry.modIndex != MOD_RANDOMIZER) || !IsMMRandoItem(entry.getItemId)) {
        return;
    }
    // Termina's alone in this unified seed (MM's Hookshot, Hero's Bow and shields unless the seed combines them,
    // MergeShipUnified.h): Randomizer_Item_Give gave nothing, and neither does this.
    if (MergeShipUnified_ForTermina(static_cast<RandomizerGet>(entry.getItemId))) {
        return;
    }
    SPDLOG_INFO("MergeShip: randomizer MM item {} received", entry.getItemId);
    GiveRandoItem(gPlayState, entry.getItemId);
}

// ---- stray fairies in containers ----------------------------------------------------------------------------------

// userData of a randomizer stray fairy: the tag and the check.
constexpr u32 kRandoFairyTag = 0x80000000u;

// Checks whose fairy is held by a container (or flying) in the current scene, and the RandomizerInf that marks a pot's
// or crate's check (RAND_INF_MAX for a chest, whose treasure flag is its own).
std::unordered_map<int, RandomizerInf> sDeferredChecks;

// The stray fairy area a check holds, or -1 (not a stray fairy, found already, not a seed).
s32 CheckFairyArea(RandomizerCheck rc) {
    if (!IS_RANDO || (rc <= RC_UNKNOWN_CHECK) || (rc >= RC_MAX)) {
        return -1;
    }
    Rando::ItemLocation* loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if ((loc == nullptr) || loc->HasObtained()) {
        return -1;
    }
    RandomizerGet placed = loc->GetPlacedRandomizerGet();
    return IsStrayFairyRG(placed) ? (s32)(placed - RG_MM_CLOCK_TOWN_STRAY_FAIRY) : -1;
}

// MM EnBox: a chest whose check holds a stray fairy lets it out when opened (MergeShip_StrayFairy_AttachToContainer).
// A chest already open whose fairy was never touched (the player left) has it floating above it again.
void OnChestInit(void* actorRef) {
    Actor* chest = static_cast<Actor*>(actorRef);
    if (!IS_RANDO || (gPlayState == nullptr) || !MergeShip_StrayFairy_IsEnabled()) {
        return;
    }
    RandomizerCheck rc =
        OTRGlobals::Instance->gRandomizer->GetCheckFromActor(chest->id, gPlayState->sceneNum, chest->params);
    if ((rc == RC_UNKNOWN_CHECK) || !OTRGlobals::Instance->gRandoContext->IsLocationShuffled(rc) ||
        (Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_CHEST_GAME)) {
        return;
    }
    s32 area = CheckFairyArea(rc);
    if (area < 0) {
        return;
    }
    u32 userData = kRandoFairyTag | (u32)rc;
    if (Flags_GetTreasure(gPlayState, chest->params & 0x1F)) {
        Vec3f pos = chest->world.pos;
        pos.y += 40.0f;
        if (MergeShip_StrayFairy_SpawnEx(gPlayState, &pos, area, MM_STRAY_FAIRY_TYPE_FREE_FLOATING,
                                         MERGESHIP_STRAY_FAIRY_FLAG_NONE, nullptr, userData) != nullptr) {
            sDeferredChecks[rc] = RAND_INF_MAX;
        }
        return;
    }
    if (MergeShip_StrayFairy_AttachToContainer(chest, area, MERGESHIP_STRAY_FAIRY_FLAG_NONE, userData)) {
        sDeferredChecks[rc] = RAND_INF_MAX;
    }
}

// ---- stray fairies dropped or lying free (EnItem00) ---------------------------------------------------------------

// Checks whose fairy flies free in the current scene and whose check is a scene collectible flag (a freestanding
// heart piece's or key's), set when it is touched.
std::unordered_map<int, s16> sDeferredCollectibles;

// Every other way a randomizer check becomes an item in the world -- a grass tuft, a bush, a rock, a tree, a beehive,
// an icicle, red ice, a wonder item, a freestanding item, anything the randomizer turns into an EnItem00 holding the
// check's item (ITEM00_SOH_DUMMY) -- is, for a stray fairy, MM's own fairy instead: before the item's first update an
// En_Elforg takes its place (MM Item_DropCollectible's ITEM00_BIG_FAIRY: STRAY_FAIRY_TYPE_COLLECTIBLE 40 units up with
// NA_SE_EV_BUTTERFRY_TO_FAIRY; a freestanding one floats where it lies, STRAY_FAIRY_TYPE_FREE_FLOATING). It flies,
// sparkles and chimes as in MM, and touching it is the check (OnStrayFairyCollected). Pots, crates and chests hold the
// fairy themselves (MergeShip_RandoItems_OnContainerInit, OnChestInit) and never drop it; shops and get-items show it.
void OnItem00ShouldUpdate(void* actorRef, bool* result) {
    EnItem00* item00 = static_cast<EnItem00*>(actorRef);
    if (!IS_RANDO || (gPlayState == nullptr) || (item00->actor.params != ITEM00_SOH_DUMMY) ||
        (item00->itemEntry.modIndex != MOD_RANDOMIZER) || !IsStrayFairyRG(item00->itemEntry.getItemId) ||
        !MergeShip_StrayFairy_IsEnabled()) {
        return;
    }
    // A drop is spawned as ITEM00_SOH_DUMMY; a freestanding item became one after its own init (ogParams).
    const bool freestanding = ((item00->ogParams & 0xFF) != ITEM00_SOH_DUMMY);
    RandomizerCheck rc = item00->randoCheck;
    if ((rc == RC_UNKNOWN_CHECK) && (item00->randoInf != RAND_INF_MAX)) {
        rc = GetRandomizerCheckFromFlag(FLAG_RANDOMIZER_INF, item00->randoInf);
    }
    if ((rc == RC_UNKNOWN_CHECK) && (item00->randoInf == RAND_INF_MAX)) {
        // A freestanding heart piece or key (VB_ITEM00_DESPAWN finds its check the same way), else its scene flag.
        rc = OTRGlobals::Instance->gRandomizer->GetCheckFromActor(item00->actor.id, gPlayState->sceneNum,
                                                                  item00->ogParams);
        if (rc == RC_UNKNOWN_CHECK) {
            rc = GetRandomizerCheckFromSceneFlag(gPlayState->sceneNum, FLAG_SCENE_COLLECTIBLE,
                                                 item00->collectibleFlag);
        }
    }
    s32 area = CheckFairyArea(rc);
    if ((area < 0) || (sDeferredChecks.find(rc) != sDeferredChecks.end())) {
        return;
    }
    Vec3f pos = item00->actor.world.pos;
    pos.y += freestanding ? 20.0f : 40.0f;
    Actor* fairy = MergeShip_StrayFairy_SpawnEx(
        gPlayState, &pos, area, freestanding ? MM_STRAY_FAIRY_TYPE_FREE_FLOATING : MM_STRAY_FAIRY_TYPE_COLLECTIBLE,
        MERGESHIP_STRAY_FAIRY_FLAG_NONE, nullptr, kRandoFairyTag | (u32)rc);
    if (fairy == nullptr) {
        return; // no fairy: the item stays, and is the check as any other item
    }
    if (!freestanding) {
        MergeShip_PlayMMSfxActor(fairy, MM_NA_SE_EV_BUTTERFRY_TO_FAIRY);
    }
    sDeferredChecks[rc] = item00->randoInf;
    if (item00->randoInf == RAND_INF_MAX) {
        sDeferredCollectibles[rc] = item00->collectibleFlag;
    }
    SPDLOG_INFO("MergeShip: check {} drops a flying stray fairy of area {}", (int)rc, area);
    Actor_Kill(&item00->actor);
    *result = false;
}

// A randomizer stray fairy was touched: it is the check. The check is marked found (as RandomizerOnItemReceiveHandler
// marks a queued one), a pot's or crate's flag is set, and MM's own give follows (the count, "You found a Stray
// Fairy!", Clock Town's plea) -- the item is the fairy of this area.
s32 OnStrayFairyCollected(PlayState* play, Actor* fairy, s32 area, s32 type, u32 userData) {
    (void)play;
    (void)fairy;
    (void)type;
    if (!(userData & kRandoFairyTag) || !IS_RANDO) {
        return false;
    }
    RandomizerCheck rc = (RandomizerCheck)(userData & 0xFFFF);
    RandomizerInf randoInf = RAND_INF_MAX;
    auto it = sDeferredChecks.find(rc);
    if (it != sDeferredChecks.end()) {
        randoInf = it->second;
    }

    Rando::ItemLocation* loc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if ((loc != nullptr) && !loc->HasObtained()) {
        loc->SetCheckStatus(RCSHOW_COLLECTED);
        CheckTracker::SpoilAreaFromCheck(rc);
        CheckTracker::RecalculateAllAreaTotals();
        CheckTracker::RecalculateAvailableChecks();
        SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_TRACKER_DATA, true);
    }
    if (randoInf != RAND_INF_MAX) {
        Flags_SetRandomizerInf(randoInf);
    }
    auto collectible = sDeferredCollectibles.find(rc);
    if ((collectible != sDeferredCollectibles.end()) && (gPlayState != nullptr)) {
        // A freestanding item's own collectible flag, as touching the item would have set it.
        Flags_SetCollectible(gPlayState, collectible->second);
        sDeferredCollectibles.erase(collectible);
    }
    if ((area >= MM_STRAY_FAIRY_AREA_CLOCK_TOWN) && (area < MM_STRAY_FAIRY_AREA_MAX)) {
        Flags_SetRandomizerInf((RandomizerInf)(RAND_INF_OBTAINED_MM_CLOCK_TOWN_STRAY_FAIRY + area));
    }
    sDeferredChecks.erase(rc);
    SPDLOG_INFO("MergeShip: stray fairy of area {} from check {} touched", area, (int)rc);
    return false;
}

// ---- loading ------------------------------------------------------------------------------------------------------

// A file whose MergeShip save lacks an item its flags say was found (a save from before the item's module saved it).
// Bottles and stray fairies are consumed or counted, so they are not given again.
void OnLoadGame(int32_t fileNum) {
    (void)fileNum;
    sDeferredChecks.clear();
    sDeferredCollectibles.clear();
    if (!IS_RANDO) {
        return;
    }
    GiveMMSwords(nullptr);
    for (int rg = RG_MM_GREAT_FAIRYS_SWORD; rg <= RG_MM_HOOKSHOT; rg++) {
        if (FoundFlagSet((RandomizerGet)rg)) {
            GiveMMEquip(nullptr, rg);
        }
    }
    if (FlagSet(RAND_INF_OBTAINED_MM_POWDER_KEG) && !MergeShip_MMItems_Owns(ITEM_MM_POWDER_KEG)) {
        GiveKegOrPictoBox(ITEM_MM_POWDER_KEG);
    }
    if (FlagSet(RAND_INF_OBTAINED_MM_PICTOGRAPH_BOX) && !MergeShip_MMItems_Owns(ITEM_MM_PICTOGRAPH_BOX)) {
        GiveKegOrPictoBox(ITEM_MM_PICTOGRAPH_BOX);
    }
    // An empty trade slot gets the furthest of its chain found.
    for (const TradeChain& chain : kTradeChains) {
        if (gMergeShipMMItemsSave.*(chain.slot) != ITEM_NONE) {
            continue;
        }
        for (int item = chain.last; item >= chain.first; item--) {
            if (FlagSet((RandomizerInf)(RAND_INF_OBTAINED_MM_MOONS_TEAR + (item - ITEM_MM_MOONS_TEAR)))) {
                GiveTradeItem(nullptr, (u8)item);
                break;
            }
        }
    }
    GiveLullabies();
    for (int rg = RG_MM_SONATA_OF_AWAKENING; rg <= RG_MM_BOMBERS_NOTEBOOK; rg++) {
        if ((rg != RG_PROGRESSIVE_GORON_LULLABY) && FoundFlagSet((RandomizerGet)rg)) {
            GiveSongItem(SongItemOf(rg));
        }
    }
}

// ---- get-item models ----------------------------------------------------------------------------------------------

// The player is holding up this randomizer item (it has been given: its flags already count it).
bool HoldingUp(PlayState* play, int rg) {
    Player* player = GET_PLAYER(play);
    return (player != nullptr) && (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) &&
           (player->getItemEntry.modIndex == MOD_RANDOMIZER) && (player->getItemEntry.getItemId == rg);
}

// The level a progressive item shows: the one just found while held up, else the next one to find.
bool ShowsSecondCopy(PlayState* play, int rg, RandomizerInf first, RandomizerInf second) {
    if (!IS_RANDO) {
        return false;
    }
    return HoldingUp(play, rg) ? FlagSet(second) : FlagSet(first);
}

// 2S2H DrawSong (mm/2s2h/Rando/DrawItem.cpp): MM's song note in the song's colour.
void DrawSongNote(PlayState* play, u8 r, u8 g, u8 b) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gDPSetEnvColor(POLY_XLU_DISP++, r, g, b, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGiSongNoteDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// A module's own get-item entry, drawn by its own drawFunc (which reads that entry).
void DrawModuleEntry(PlayState* play, GetItemEntry entry) {
    if (entry.drawFunc != nullptr) {
        entry.drawFunc(play, &entry);
    } else {
        GetItem_Draw(play, (s16)entry.gid);
    }
}

void DrawMMRandoItem(PlayState* play, int rg) {
    switch (rg) {
        // MM z_draw.c sDrawItemTable.
        case RG_PROGRESSIVE_MM_SWORD:
            if (ShowsSecondCopy(play, rg, RAND_INF_OBTAINED_MM_RAZOR_SWORD, RAND_INF_OBTAINED_MM_GILDED_SWORD)) {
                // GID_SWORD_GILDED, OBJECT_GI_SWORD_3
                MergeShip_GiDrawOpa01(play, mm_gGiGildedSwordDL, mm_gGiGildedSwordEmptyDL);
            } else {
                // GID_SWORD_RAZOR, OBJECT_GI_SWORD_2
                MergeShip_GiDrawOpa01(play, mm_gGiRazorSwordDL, mm_gGiRazorSwordEmptyDL);
            }
            return;
        case RG_MM_GREAT_FAIRYS_SWORD: // GID_SWORD_GREAT_FAIRY, OBJECT_GI_SWORD_4
            MergeShip_GiDrawOpa0Xlu1(play, mm_gGiGreatFairysSwordBladeDL, mm_gGiGreatFairysSwordHiltEmblemDL);
            return;
        case RG_MM_HEROS_SHIELD: // GID_SHIELD_HERO, OBJECT_GI_SHIELD_2
            MergeShip_GiDrawOpa0Xlu1(play, mm_gGiHerosShieldEmblemDL, mm_gGiHerosShieldDL);
            return;
        case RG_MM_MIRROR_SHIELD: // GID_SHIELD_MIRROR, OBJECT_GI_SHIELD_3
            MergeShip_GiDrawOpa0Xlu1(play, mm_gGiMirrorShieldEmptyDL, mm_gGiMirrorShieldDL);
            return;
        case RG_MM_HEROS_BOW: // GID_BOW, OBJECT_GI_BOW
            MergeShip_GiDrawOpa01(play, mm_gGiBowHandleDL, mm_gGiBowStringDL);
            return;
        case RG_MM_HOOKSHOT: // GID_HOOKSHOT, OBJECT_GI_HOOKSHOT
            MergeShip_GiDrawOpa01(play, mm_gGiHookshotEmptyDL, mm_gGiHookshotDL);
            return;
        // 2S2H DrawSong's colours.
        case RG_MM_SONATA_OF_AWAKENING:
            DrawSongNote(play, 98, 255, 98);
            return;
        case RG_PROGRESSIVE_GORON_LULLABY:
            if (ShowsSecondCopy(play, rg, RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO,
                                RAND_INF_OBTAINED_MM_GORON_LULLABY)) {
                DrawSongNote(play, 255, 20, 20);
            } else {
                DrawSongNote(play, 255, 100, 100);
            }
            return;
        case RG_MM_NEW_WAVE_BOSSA_NOVA:
            DrawSongNote(play, 20, 20, 255);
            return;
        case RG_MM_ELEGY_OF_EMPTINESS:
            DrawSongNote(play, 255, 98, 0);
            return;
        case RG_MM_OATH_TO_ORDER:
            DrawSongNote(play, 98, 0, 98);
            return;
        case RG_MM_SONG_OF_HEALING:
            DrawSongNote(play, 255, 150, 230);
            return;
        case RG_MM_SONG_OF_SOARING:
            DrawSongNote(play, 200, 160, 255);
            return;
        // The modules' own get-item models.
        case RG_MM_BOTTLE_WITH_CHATEAU_ROMANI:
            DrawModuleEntry(play, MergeShip_Chateau_GetItemEntry());
            return;
        default:
            break;
    }
    if ((rg >= RG_MM_POWDER_KEG) && (rg <= RG_MM_PICTOGRAPH_BOX)) {
        DrawModuleEntry(play, MergeShip_MMItems_GetItemEntry((u8)(ITEM_MM_POWDER_KEG + (rg - RG_MM_POWDER_KEG))));
    } else if ((rg >= RG_MM_BOTTLE_WITH_DEKU_PRINCESS) && (rg <= RG_MM_BOTTLE_WITH_HOT_SPRING_WATER)) {
        DrawModuleEntry(play, MergeShip_MMItems_GetItemEntry(
                                  (u8)(ITEM_MM_DEKU_PRINCESS + (rg - RG_MM_BOTTLE_WITH_DEKU_PRINCESS))));
    } else if ((rg >= RG_MM_MOONS_TEAR) && (rg <= RG_MM_PENDANT_OF_MEMORIES)) {
        DrawModuleEntry(play, MergeShip_MMItems_GetItemEntry((u8)(ITEM_MM_MOONS_TEAR + (rg - RG_MM_MOONS_TEAR))));
    } else if ((rg >= RG_MM_ODOLWAS_REMAINS) && (rg <= RG_MM_BOMBERS_NOTEBOOK)) {
        DrawModuleEntry(play, MergeShip_Songs_GetItemEntry(SongItemOf(rg)));
    } else if (IsStrayFairyRG(rg)) {
        DrawModuleEntry(play, MergeShip_StrayFairy_GetItemEntry(rg - RG_MM_CLOCK_TOWN_STRAY_FAIRY));
    }
}

// ---- registration -------------------------------------------------------------------------------------------------

void RegisterMergeShipRandoItems() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemReceive>(OnItemReceive);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t sceneNum) {
            (void)sceneNum;
            sDeferredChecks.clear();
            sDeferredCollectibles.clear();
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(ACTOR_EN_BOX, OnChestInit);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorUpdate>(ACTOR_EN_ITEM00,
                                                                                        OnItem00ShouldUpdate);
    MergeShip_StrayFairy_SetCollectOverride(OnStrayFairyCollected);
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipRandoItems, {});

} // namespace

bool MergeShip_RandoItems_DefersCheck(RandomizerCheck rc) {
    // "Stray Fairies" turned off after the container took its fairy: the container gives its check the normal way
    // (MergeShip_StrayFairy_ContainerHoldsFairy and the fairy's own spawn follow the option as well).
    return IS_RANDO && MergeShip_StrayFairy_IsEnabled() && (sDeferredChecks.find(rc) != sDeferredChecks.end());
}

void MergeShip_RandoItems_OnContainerInit(Actor* container, RandomizerCheck rc, RandomizerInf randoInf) {
    if ((container == nullptr) || !MergeShip_StrayFairy_IsEnabled()) {
        return;
    }
    s32 area = CheckFairyArea(rc);
    if (area < 0) {
        return;
    }
    if (MergeShip_StrayFairy_AttachToContainer(container, area, MERGESHIP_STRAY_FAIRY_FLAG_NONE,
                                               kRandoFairyTag | (u32)rc)) {
        sDeferredChecks[rc] = randoInf;
    }
}

bool MergeShip_RandoItems_ContainerHoldsStrayFairy(Actor* container) {
    return IS_RANDO && MergeShip_StrayFairy_ContainerHoldsFairy(container);
}

extern "C" void MergeShip_DrawGiMMRandoItem(PlayState* play, GetItemEntry* getItemEntry) {
    // The entry's draw item: the item itself, or the item an ice trap looks like (MergeShipRandoMasks.cpp).
    int rg = (getItemEntry->drawModIndex == MOD_RANDOMIZER) ? getItemEntry->drawItemId : -1;
    if (!IsMMRandoItem(rg)) {
        rg = getItemEntry->getItemId;
    }
    // The equipment and songs are drawn from mm.o2r here; the modules' own models fall back themselves.
    bool drawnHere = (rg >= RG_PROGRESSIVE_MM_SWORD && rg <= RG_MM_HOOKSHOT) ||
                     (rg >= RG_MM_SONATA_OF_AWAKENING && rg <= RG_MM_SONG_OF_SOARING);
    if (!IsMMRandoItem(rg) || (drawnHere && !MergeShip_IsMajorasMaskMounted())) {
        GetItem_Draw(play, (s16)getItemEntry->gid);
        return;
    }
    DrawMMRandoItem(play, rg);
}
