#pragma once

// MergeShip2 -- Majora's Mask's Deku Link drawing (MM src/code/z_player_lib.c), called from Ocarina of Time's
// z_player_lib.c limb callbacks. Implemented in MergeShipDekuDraw.c.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Player_OverrideLimbDrawGameplayCommon, PLAYER_LIMB_ROOT, after the root position is adjusted: MM's petal-flight
// body pose (PLAYER_STATE3_2000, func_801251C4).
void MergeShip_Deku_OverrideLimbRoot(Player* player, Vec3f* pos, Vec3s* rot);
// PLAYER_LIMB_HEAD, after headLimbRot is applied: MM's Deku head squash (bubble, pipes, drink).
void MergeShip_Deku_OverrideLimbHead(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot);
// PLAYER_LIMB_UPPER, after upperLimbRot: MM tilts the Deku back as the bubble grows.
void MergeShip_Deku_OverrideLimbUpper(Player* player);
// End of Player_PostLimbDrawGameplay: flight petals (func_801271B0), guard shell, pipes, head body part and the
// bubble's position, the spin's trail, and the head matrix pop.
void MergeShip_Deku_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* curBodyPartPos);

#ifdef __cplusplus
}
#endif
