#pragma once

// MergeShip2 -- Ocarina of Time's magic spells (Din's Fire, Farore's Wind, Nayru's Love) in Majora's Mask (2Ship).
//
// The spell actors are ports of OoT's ovl_Magic_Fire / ovl_Magic_Wind / ovl_Magic_Dark:
//   src/overlays/actors/ovl_OoT_Magic_Fire, ovl_OoT_Magic_Wind, ovl_OoT_Magic_Dark
// They are spawned exactly as OoT's Player_SpawnMagicSpell spawns them (params 0, at the player's position):
//   spell 0 = ACTOR_OOT_MAGIC_WIND, 1 = ACTOR_OOT_MAGIC_DARK, 2 = ACTOR_OOT_MAGIC_FIRE.

#include "ultra64.h"

#ifdef __cplusplus
extern "C" {
#endif

// OoT gSaveContext.nayrusLoveTimer (OoT z64save.h 0x13C8), which MM's SaveContext does not have. Same semantics:
// 0 = Nayru's Love is off; OoT_Magic_Dark counts it up by one per frame while the diamond is up and clears it (and
// kills itself) once it reaches 1200. Defined in ovl_OoT_Magic_Dark/z_oot_magic_dark.c.
extern s16 gMergeShipNayrusLoveTimer;

#ifdef __cplusplus
}
#endif
