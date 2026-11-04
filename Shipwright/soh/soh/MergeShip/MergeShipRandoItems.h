#pragma once

// MergeShip2 -- Majora's Mask's equipment, items, songs, quest items and stray fairies as randomizer items
// (MergeShipRandoItems.cpp). The masks are MergeShipMaskItems.cpp's.
//
//   RSK_SHUFFLE_MM_EQUIPMENT   RG_PROGRESSIVE_MM_SWORD (Razor, then Gilded) .. RG_MM_HOOKSHOT
//   RSK_SHUFFLE_MM_ITEMS       RG_MM_POWDER_KEG .. RG_MM_PENDANT_OF_MEMORIES (the bottles, Chateau Romani, trade items)
//   RSK_SHUFFLE_MM_SONGS       RG_MM_SONATA_OF_AWAKENING .. RG_MM_BOMBERS_NOTEBOOK (RG_PROGRESSIVE_GORON_LULLABY:
//                              Intro, then full; the four remains; the notebook)
//   RSK_SHUFFLE_STRAY_FAIRIES  RG_MM_CLOCK_TOWN_STRAY_FAIRY .. RG_MM_STONE_TOWER_STRAY_FAIRY (1 + 4 x 15 copies)
//
// Finding one sets its RAND_INF_OBTAINED_MM_* flag (Randomizer_Item_Give) and the item-receive hook here gives it to
// its module (MergeShipMMEquipment.h, MergeShipMMItems.h, MergeShipChateau.h, MergeShipSongs.h,
// MergeShipStrayFairy.h): its give function where that runs no item-receive hooks of its own, else its save data as
// its Item_Give branch writes it. A loaded file gets back any owned item its module's save lacks, from the flags.
//
// A pot, crate or chest whose check holds a stray fairy holds the fairy itself
// (MergeShip_StrayFairy_AttachToContainer): it flies out as in MM, and touching it is the check. Until then the check is deferred: the container's flag (the
// chest's treasure flag set as it opens) does not queue the randomizer's item. Any other check that becomes an item in
// the world (EnItem00: grass, rocks, trees, beehives, icicles, wonder items, freestanding items) is MM's flying fairy
// too, spawned in the item's place before its first update.

#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

typedef struct PlayState PlayState;
struct Actor;

#ifdef __cplusplus
extern "C" {
#endif

// The get-item model of every item above: the module's own get-item model where it has one (MM items, Chateau
// Romani, remains, notebook, stray fairies), else MM's GetItem_Draw from mm.o2r (equipment, songs). Without mm.o2r,
// the item's Ocarina of Time object / gid.
void MergeShip_DrawGiMMRandoItem(PlayState* play, GetItemEntry* getItemEntry);

#ifdef __cplusplus
}

// A container's flag was set for check `rc`, whose stray fairy has not been touched yet: the randomizer does not
// queue the item (hook_handlers.cpp RandomizerOnFlagSetHandler / RandomizerOnSceneFlagSetHandler).
bool MergeShip_RandoItems_DefersCheck(RandomizerCheck rc);
// A pot or crate identified as check `rc` (its RandomizerInf `randoInf`) was spawned: when the check holds a stray
// fairy not found yet, the container holds the fairy (ShufflePots.cpp, ShuffleCrates.cpp).
void MergeShip_RandoItems_OnContainerInit(Actor* container, RandomizerCheck rc, RandomizerInf randoInf);
// The container (pot, crate or chest) holds a randomizer stray fairy: it drops / gives nothing of its own.
bool MergeShip_RandoItems_ContainerHoldsStrayFairy(Actor* container);
#endif
