#pragma once

// MergeShip2 -- "Masks Get Their Own Container Style": 2ship2harkinian's randomizer gives masks their own
// Container Style Matches Contents look (RITYPE_MASK: the white mask-faced chest trim, pot, crate, bush and grass).
// SoH's CSMC decides every container's look from Randomizer_AdjustItemCategory; that function asks this file whether
// the item is a mask and, if so, returns ITEM_CATEGORY_MASK, which each CSMC container draws with its mask variant.
// Gated by CVAR_ENHANCEMENT("MergeShip.MaskContainerStyle") (Mergeship menu, default on); off, masks stay Major.
//
// Stray fairies get their own style the same way (2ship's RITYPE_STRAY_FAIRY: the fairy chest trim, pot, crate, bush
// and grass; pink sparkles on the particle containers). Their item table category is ITEM_CATEGORY_STRAY_FAIRY, and
// Randomizer_AdjustItemCategory asks this file too, for an ice trap that looks like a stray fairy. Gated by
// CVAR_ENHANCEMENT("MergeShip.StrayFairyContainerStyle") (Mergeship menu, default on); off, stray fairies look like
// lesser items.

#include <stdbool.h>
#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"

#ifdef __cplusplus
extern "C" {
#endif

// The one place that decides "is this randomizer item a mask". The list lives in MergeShipRandoMasks.cpp.
bool MergeShip_RandoItemIsMask(RandomizerGet rg);

// True when a container holding this item should look like a mask container: the item is a mask, or it is an ice trap
// disguised as a mask (as in 2ship, a trap takes the look of the item it imitates, so a mask container never tells a
// real mask from a trap). Reads the item the entry is drawn as, so it follows SoH's ice-trap overrides.
bool MergeShip_RandoGIEntryIsMask(GetItemEntry item);

// Called at the end of Randomizer_AdjustItemCategory: returns ITEM_CATEGORY_MASK for masks while the option is on;
// with it off, ITEM_CATEGORY_MAJOR for an entry whose category is ITEM_CATEGORY_MASK. Otherwise `category` unchanged.
GetItemCategory MergeShip_RandoMaskAdjustCategory(GetItemEntry item, GetItemCategory category);

// True when the entry is drawn as one of Majora's Mask's stray fairies (the item itself, or an ice trap imitating one).
bool MergeShip_RandoGIEntryIsStrayFairy(GetItemEntry item);

// Called at the end of Randomizer_AdjustItemCategory: ITEM_CATEGORY_STRAY_FAIRY for stray fairies (and ice traps that
// look like one) while the option is on; with it off, ITEM_CATEGORY_LESSER for stray fairies. Otherwise `category`.
GetItemCategory MergeShip_RandoStrayFairyAdjustCategory(GetItemEntry item, GetItemCategory category);

#ifdef __cplusplus
}
#endif
