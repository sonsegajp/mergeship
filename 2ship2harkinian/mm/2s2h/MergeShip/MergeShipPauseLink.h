#pragma once

// MergeShip2 -- Link on the pause screen's equipment views (the Hyrule Equipment view, MergeShipOoTEquipPause.c, and the
// Termina Equipment view, MergeShipTerminaEquipPause.c), where Ocarina of Time's equipment page draws him
// (OoT z_kaleido_equipment.c KaleidoScope_DrawPlayerWork -> z_player_lib.c Player_DrawPause).
//
// Majora's Mask has no pause-screen Link, so this draws the player as he is now -- MM's young human, the Deku, Goron,
// Zora or Fierce Deity, or MergeShip's adult Link (MergeShipAdultLink.h) -- with his own skeleton and his own limb
// drawing (Player_DrawImpl with Player_OverrideLimbDrawGameplayDefault / Player_PostLimbDrawGameplay, so the sword in
// the sheath and the shield on the back are the ones in use: MM's, or Ocarina of Time's through MergeShipOoTEquip.h),
// standing still in frame 0 of his idle animation (MM Player_GetIdleAnim), turned as Ocarina of Time turns him
// (rot.y 32300 for a camera behind him), lit by Ocarina of Time's pause light (80 ambient, one white light).
//
// Placement: Ocarina of Time draws Link into a 64 x 112 image with its own camera (eye z -400, fovy 60) at the page's
// (-64, 50) and puts the image on the page, so he turns with the page. Here he is drawn straight on the page, in the
// page's own matrix, with the same size and place that image gives him: each form takes Ocarina of Time's framing for
// its size (OoT KaleidoScope_DrawPlayerWork, as SoH's MergeShip forms take it: the young human and the Deku the child's;
// the Goron, the Zora and the adult with the Master Sword the adult's; the Fierce Deity and the adult with a long sword
// the long-sword adult's), converted from the image's camera to page units. Depth-tested against himself; the pages
// write no depth. The page's matrix, segments and render state are put back after him.
//
// The player struct is saved before and restored after the draw (the preview's pose, models and the limb positions the
// limb callbacks record never reach gameplay). Nothing is drawn when the player or his skeleton isn't there.
//
// Option: MERGESHIP_PAUSE_LINK_CVAR (Enhancements > Mergeship "Link on the Equipment Views", default on).

#include "ultra64.h"

#define MERGESHIP_PAUSE_LINK_CVAR "gEnhancements.MergeShip.PauseLink"

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Draws Link in the equipment view's centre panel. Call from a Quest-page view's draw (POLY_OPA, the page's matrix
// loaded); `view` names the caller in the harness log ("hyrule" / "termina").
void MergeShip_PauseLink_Draw(struct PlayState* play, const char* view);

#ifdef __cplusplus
}
#endif
