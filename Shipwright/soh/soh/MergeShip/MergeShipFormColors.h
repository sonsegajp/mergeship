#pragma once

// MergeShip2 -- tunic colours on Majora's Mask's forms (see MergeShipFormColors.cpp).
//
// Every green the Deku, Goron and Zora wear follows Ocarina of Time's Kokiri Tunic colour (the cosmetics
// editor's Link.KokiriTunic, and Anchor's player colour, through VB_APPLY_TUNIC_COLOR); the Fierce Deity's
// tunic follows the cosmetics editor's Link.FierceDeityTunic. At the default colours the forms are drawn
// from Majora's Mask's own, untouched display lists.

#include "z64.h"

// The form display lists read their recoloured palettes and textures through this segment.
#define MERGESHIP_FORM_COLOR_SEGMENT 0x0E

#ifdef __cplusplus
extern "C" {
#endif

// Points MERGESHIP_FORM_COLOR_SEGMENT at `player`'s colours for everything drawn after it this frame.
// Play_Draw points it at the player's (GET_PLAYER) own at the start of each frame; another Player
// actor that draws a form (Anchor's remote players) calls this before its draw and again with the
// local player after it.
void MergeShip_FormColors_SetForPlayer(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif
