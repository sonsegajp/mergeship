#pragma once

// MergeShip2 -- the get-item models of the unified randomizer's Termina items in Hyrule.
//
// tools/port_mm_rando.py generates a RandomizerGet RG_MM_* for every item of 2Ship's randomizer that SoH had none
// for (item_list_termina.cpp). Each is drawn the way 2Ship's randomizer draws its RandoItemId (2s2h/Rando/DrawItem.cpp
// Rando::DrawItem): MM's GetItem_Draw of the item's drawId, or 2Ship's special draw (songs, keys, tokens, owl statues,
// clocks, frogs, enemy and boss souls), from mm.o2r. Which is which is generated from 2Ship's sources by
// tools/gen_cross_gi.py into MergeShipTerminaGiData.inc; the draws are MergeShipTerminaGiDraw.cpp's. Everywhere SoH
// draws a get-item entry -- held up over Link, in a chest, freestanding / dropped (EnItem00), on a shop shelf
// (EnGirlA) -- goes through the entry's drawFunc, which item_list_termina.cpp sets to MergeShip_DrawGiTerminaItem.

#include "soh/Enhancements/item-tables/ItemTableTypes.h"

typedef struct PlayState PlayState;

#ifdef __cplusplus
extern "C" {
#endif

// The get-item model of a generated Termina item (the entry's draw item: the item itself, or the item an ice trap
// looks like). Without mm.o2r, the entry's own Ocarina of Time gid.
void MergeShip_DrawGiTerminaItem(PlayState* play, GetItemEntry* getItemEntry);

// True when `rg` is one of the generated Termina items this file draws (tests, trackers).
int MergeShip_TerminaGi_Has(int rg);

// MM's GetItem_Draw of Majora's Mask get-item draw id `mmGid` (MM's GetItemDrawId value), from mm.o2r.
void MergeShip_DrawMMGetItem(PlayState* play, int mmGid);

#ifdef __cplusplus
}
#endif
