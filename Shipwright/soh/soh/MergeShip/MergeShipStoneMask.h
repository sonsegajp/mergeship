#pragma once

// MergeShip2 -- Majora's Mask's Stone Mask in Ocarina of Time.
//
// MM has no generic invisibility: each enemy that ignores a Stone Mask wearer checks
// `Player_GetMask(play) != PLAYER_MASK_STONE` in its own detection code. The OoT counterparts of those
// actors make the same check, in the same places, through this one gate. Gated by
// CVAR_ENHANCEMENT("MergeShip.StoneMask"), default on.

#include <libultraship/libultra/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// True while the player wears MM's Stone Mask and the feature is on: MM's `Player_GetMask(play) == PLAYER_MASK_STONE`.
s32 MergeShip_StoneMaskHides(struct PlayState* play);

#ifdef __cplusplus
}
#endif
