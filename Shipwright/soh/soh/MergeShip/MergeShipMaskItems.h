#pragma once

// MergeShip2 -- Majora's Mask's transformation masks as items of Ocarina of Time and its randomizer.
//
// * Get-item models: MM GetItem_Draw's GID_MASK_DEKU / GID_MASK_GORON / GID_MASK_ZORA / GID_MASK_FIERCE_DEITY,
//   drawn from mm.o2r. They are the draw functions of the randomizer's RG_MM_*_MASK items, so the same model is
//   what Link holds up, what lies in the world (freestanding items, shop shelves, chests' contents), and what an
//   ice trap may look like.
// * Ocarina of Time's Goron Mask and Zora Mask *are* the Goron and Zora transformation masks
//   (CVAR_ENHANCEMENT("MergeShip.MergeOoTMasks"), on by default): getting or borrowing one also gives the
//   transformation mask, and wearing it from a C button transforms. See MergeShipMaskItems.cpp for how this
//   meets the Happy Mask Shop and the randomizer.

#include "soh/Enhancements/item-tables/ItemTableTypes.h"

typedef struct PlayState PlayState;

#ifdef __cplusplus
extern "C" {
#endif

void MergeShip_DrawGiDekuMask(PlayState* play, GetItemEntry* getItemEntry);
void MergeShip_DrawGiGoronMask(PlayState* play, GetItemEntry* getItemEntry);
void MergeShip_DrawGiZoraMask(PlayState* play, GetItemEntry* getItemEntry);
void MergeShip_DrawGiFierceDeityMask(PlayState* play, GetItemEntry* getItemEntry);
// Majora's Mask's other 20 masks (RG_MM_MASK_OF_TRUTH..RG_MM_GIANTS_MASK): MM's get-item model of each, from mm.o2r.
void MergeShip_DrawGiMMMask(PlayState* play, GetItemEntry* getItemEntry);

// MM z_draw.c GetItem_DrawOpa0Xlu1 (the first list opaque, the second translucent) and GetItem_DrawOpa01 (both
// opaque), at the current matrix, for get-item models drawn from mm.o2r.
void MergeShip_GiDrawOpa0Xlu1(PlayState* play, const char* opa, const char* xlu);
void MergeShip_GiDrawOpa01(PlayState* play, const char* first, const char* second);

// True while Ocarina of Time's Goron and Zora masks are the transformation masks: the option is on, mm.o2r is
// mounted, and this is not a randomizer seed that shuffles the transformation masks as items of their own.
int MergeShip_OoTMasksAreTransformationMasks(void);

// ITEM_MASK_GORON / ITEM_MASK_ZORA -> ITEM_MM_MASK_GORON / ITEM_MM_MASK_ZORA while they are merged; any other
// item is returned unchanged. For the code that decides what a C button does and whether a form may press it.
unsigned char MergeShip_ResolveMaskItem(unsigned char item);

#ifdef __cplusplus
}
#endif
