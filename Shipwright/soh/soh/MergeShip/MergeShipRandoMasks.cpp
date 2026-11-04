// MergeShip2 -- "Masks Get Their Own Container Style" and "Stray Fairies Get Their Own Container Style" (see
// MergeShipRandoMasks.h).
// Port of 2ship2harkinian's RITYPE_MASK container style (mm/2s2h/Rando/ActorBehavior/EnBox.cpp, ObjTsubo.cpp,
// ObjKibako.cpp, ObjGrass.cpp) onto SoH's Container Style Matches Contents.

#include "MergeShipRandoMasks.h"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64item.h"
#include "soh/cvar_prefixes.h"
}

// Every randomizer item that counts as a mask for the container style. Add new masks here (one line each).
static const RandomizerGet sMergeShipRandoMasks[] = {
    // Majora's Mask's transformation masks
    RG_MM_DEKU_MASK,
    RG_MM_GORON_MASK,
    RG_MM_ZORA_MASK,
    RG_MM_FIERCE_DEITY_MASK,
    // Majora's Mask's other masks
    RG_MM_MASK_OF_TRUTH,
    RG_MM_KAFEIS_MASK,
    RG_MM_ALL_NIGHT_MASK,
    RG_MM_BUNNY_HOOD,
    RG_MM_KEATON_MASK,
    RG_MM_GAROS_MASK,
    RG_MM_ROMANIS_MASK,
    RG_MM_CIRCUS_LEADERS_MASK,
    RG_MM_POSTMANS_HAT,
    RG_MM_COUPLES_MASK,
    RG_MM_GREAT_FAIRYS_MASK,
    RG_MM_GIBDO_MASK,
    RG_MM_DON_GEROS_MASK,
    RG_MM_KAMAROS_MASK,
    RG_MM_CAPTAINS_HAT,
    RG_MM_STONE_MASK,
    RG_MM_BREMEN_MASK,
    RG_MM_BLAST_MASK,
    RG_MM_MASK_OF_SCENTS,
    RG_MM_GIANTS_MASK,

    // Ocarina of Time's masks
    RG_KEATON_MASK,
    RG_SKULL_MASK,
    RG_SPOOKY_MASK,
    RG_BUNNY_HOOD,
    RG_GORON_MASK,
    RG_ZORA_MASK,
    RG_GERUDO_MASK,
    RG_MASK_OF_TRUTH,
};

// Vanilla (MOD_NONE) entries carry an ItemID instead of a RandomizerGet: OoT's masks by ItemID.
static const ItemID sMergeShipVanillaMasks[] = {
    ITEM_MASK_KEATON, ITEM_MASK_SKULL,  ITEM_MASK_SPOOKY, ITEM_MASK_BUNNY,
    ITEM_MASK_GORON,  ITEM_MASK_ZORA,   ITEM_MASK_GERUDO, ITEM_MASK_TRUTH,
};

extern "C" bool MergeShip_RandoItemIsMask(RandomizerGet rg) {
    for (RandomizerGet mask : sMergeShipRandoMasks) {
        if (mask == rg) {
            return true;
        }
    }
    return false;
}

extern "C" bool MergeShip_RandoGIEntryIsMask(GetItemEntry item) {
    // drawModIndex/drawItemId name the item the entry is shown as: the item itself, or for an ice trap the item
    // Context::GetFinalGIEntry copied from its override (LooksLike). Randomizer entries hold a RandomizerGet there,
    // vanilla entries an ItemID.
    if (item.drawModIndex == MOD_RANDOMIZER) {
        return MergeShip_RandoItemIsMask((RandomizerGet)item.drawItemId);
    }
    if (item.drawModIndex == MOD_NONE) {
        for (ItemID mask : sMergeShipVanillaMasks) {
            if (mask == item.drawItemId) {
                return true;
            }
        }
    }
    return false;
}

extern "C" GetItemCategory MergeShip_RandoMaskAdjustCategory(GetItemEntry item, GetItemCategory category) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MaskContainerStyle"), 1)) {
        // Off: a mask looks like a major item (also one whose item table entry says ITEM_CATEGORY_MASK, as
        // tools/port_mm_rando.py writes for a mask 2ship shows as RITYPE_MASK).
        return (category == ITEM_CATEGORY_MASK) ? ITEM_CATEGORY_MAJOR : category;
    }
    // As in 2ship every mask is RITYPE_MASK, whatever category its item table entry gives it (all Major today).
    return MergeShip_RandoGIEntryIsMask(item) ? ITEM_CATEGORY_MASK : category;
}

extern "C" bool MergeShip_RandoGIEntryIsStrayFairy(GetItemEntry item) {
    // As for masks: drawItemId is the item the entry is shown as (an ice trap's LooksLike item). The five stray fairy
    // RandomizerGets are contiguous (MergeShipRandoItems.cpp IsStrayFairyRG).
    return (item.drawModIndex == MOD_RANDOMIZER) && (item.drawItemId >= RG_MM_CLOCK_TOWN_STRAY_FAIRY) &&
           (item.drawItemId <= RG_MM_STONE_TOWER_STRAY_FAIRY);
}

extern "C" GetItemCategory MergeShip_RandoStrayFairyAdjustCategory(GetItemEntry item, GetItemCategory category) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.StrayFairyContainerStyle"), 1)) {
        // Off: a stray fairy (whose table category is ITEM_CATEGORY_STRAY_FAIRY) looks like a lesser item.
        return (category == ITEM_CATEGORY_STRAY_FAIRY) ? ITEM_CATEGORY_LESSER : category;
    }
    // On: 2ship's RITYPE_STRAY_FAIRY, also for an ice trap imitating a stray fairy (2ship draws a trap's container as
    // the item it imitates, see MergeShip_RandoMaskAdjustCategory).
    return MergeShip_RandoGIEntryIsStrayFairy(item) ? ITEM_CATEGORY_STRAY_FAIRY : category;
}
