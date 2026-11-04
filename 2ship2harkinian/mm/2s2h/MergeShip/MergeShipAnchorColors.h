#pragma once

// MergeShip2 -- Anchor's player colours on Link and Majora's Mask's forms in Termina (MergeShipAnchorColors.cpp).
// The port of SoH's soh/MergeShip/MergeShipFormColors.cpp and Anchor's VB_APPLY_TUNIC_COLOR (HookHandlers.cpp).

#include "z64.h"

// The form display lists read their recoloured palettes and textures (and the human's tunic colour) through this
// segment while Anchor colours the players. SoH uses 0x0E; in 2Ship 0x0E is gGfxMasterDL (graph.c, z_rcp.c) and must
// not move. 0x07 is the only segment the world draw leaves alone: MM sets it only right before its own use (the hilite
// tile of func_800B8050 / func_800B8118 and En_Test6 on the world lists, the textbox and story segments on the
// overlay list), never for a later draw, and Player passes 0 to the hilite functions (z_player.c), so nothing
// between our gSPSegment and Link's lists changes it. 0x08-0x0D are Player's own (eyes, mouth, mask, matrices, cull
// list), 0x01-0x06 the billboard, scene, room, keeps and object, 0x0F the frame buffer.
#define MERGESHIP_ANCHOR_COLOR_SEGMENT 0x07

#ifdef __cplusplus
extern "C" {
#endif

// While Anchor colours the players (connected, or its loopback test): `player`'s (the local player or a dummy)
// Anchor colour in `color`, true. Otherwise false and `color` is left alone.
s32 MergeShip_AnchorColors_TunicColor(Actor* player, Color_RGB8* color);
// Points the segment at `player`'s colours on POLY_OPA / POLY_XLU, for a Player drawn outside its actor draw (the pause
// screen's Link, MergeShipPauseLink.c). No-op while Anchor doesn't colour the players.
void MergeShip_AnchorColors_SetForPlayer(struct PlayState* play, Actor* player);

#ifdef __cplusplus
}
#endif
