#pragma once

// MergeShip2 -- Majora's Mask's other twenty masks (Mask of Truth .. Giant's Mask) on the player.
//
// MM z_player_lib.c: the mask worn on the head (D_801C0B20 and the per-mask draws of the PLAYER_LIMB_HEAD
// block of Player_PostLimbDrawGameplay: Player_DrawCouplesMask, Player_DrawCircusLeadersMask,
// Player_DrawBlastMask, Player_DrawBunnyHood, Player_DrawGreatFairysMask) and the state they keep
// (func_801229FC's Great Fairy hair reset and Circus Leader tear counters, Player_UpdateBunnyEars).
// MM's masks are modelled for its child Link's head; adult Link and Ocarina of Time's child get a fit.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// MM Player_UseItem's branch for these masks (a human puts them on and takes them off at once).
void MergeShip_MMMasks_Use(PlayState* play, Player* player, s32 itemAction);
// MM Player_UseItem's Giant's Mask branch (magic and ceiling checks, then the put-on / take-off action), defined in
// MergeShipGiantsMask.inc at the end of z_player.c.
void MergeShip_GiantsMask_Use(PlayState* play, Player* player);
// Per frame, from Player_UpdateCommon: ear/hair/tear state, the Blast Mask's cooldown, and the mask the save
// remembers (MM gSaveContext.save.equippedMask).
void MergeShip_MMMasks_Update(Player* player, PlayState* play);
// From the head limb's post-draw: keeps the head's matrix for the mask.
void MergeShip_MMMasks_DrawOnHead(PlayState* play, Player* player);
// After the skeleton (where Ocarina of Time draws its masks): the worn mask.
void MergeShip_MMMasks_DrawAfterSkeleton(PlayState* play, Player* player);
// MM Player_Init: a human wears the remembered mask again when the scene loads (never the Giant's Mask).
void MergeShip_MMMasks_RestoreOnSceneLoad(Player* player);
// The item of a currentMask value that is one of these masks, or ITEM_NONE.
u8 MergeShip_MMMasks_ItemOfMask(u8 currentMask);

// MM's ACTORCTX_FLAG_3: something in the room holds a stray fairy (the Great Fairy's Mask's hair lifts and
// sparkles). Set every frame by whatever holds one; cleared after the player is drawn.
extern u8 gMergeShipStrayFairyNearby;

// MM Player::blastMaskTimer (frames until the Blast Mask can explode again).
extern u16 gMergeShipBlastMaskTimer;

#ifdef __cplusplus
}
#endif
