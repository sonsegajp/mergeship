#ifndef RANDO_MERGESHIP_OOT_GI_DRAW_H
#define RANDO_MERGESHIP_OOT_GI_DRAW_H

// MergeShip2 -- the get-item models of the unified randomizer's Hyrule items in Termina (the proxies RI_OOT_<key>).
//
// Each is drawn the way SoH draws the item's RandomizerGet (Shipwright soh/Enhancements/randomizer/item_list.cpp): Ocarina
// of Time's GetItem_Draw of its GID (a port of OoT z_draw.c, the display lists read from oot.o2r under "oot/"), tinted
// where SoH tints it (dungeon maps and compasses, silver rupees), or the oot.o2r part of SoH's own model (the boss
// souls' flame, the bean souls' sprout, the Scarecrow's Song note). SoH's models that exist only in soh.o2r, which
// 2Ship does not mount (its emblem keys, key rings, bags, abilities), are drawn as the Ocarina of Time item closest to
// them; tools/gen_cross_gi.py --list names each such choice. The table is generated from both ports' sources by
// tools/gen_cross_gi.py into OoTGiData.inc. Rando::MergeShip::DrawProxy (UnifiedDelivery.cpp) calls this for every
// proxy it does not draw level by level itself; every place 2Ship draws a randomizer item (held up, chests,
// freestanding / dropped, shop shelves) goes through Rando::DrawItem -> DrawProxy.

#include <mergeship/MsItems.h>

struct PlayState;

// Draws proxy `key`'s Ocarina of Time model at the current matrix. False (nothing drawn) without oot.o2r or a row.
bool MergeShip_OoTGi_DrawKey(PlayState* play, MsItemKey key);
// True when `key` has a row (tests and the fallback audit).
bool MergeShip_OoTGi_HasKey(MsItemKey key);

#endif
