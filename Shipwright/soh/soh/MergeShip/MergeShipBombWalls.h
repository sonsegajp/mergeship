#pragma once

// MergeShip2 option CVAR_ENHANCEMENT("MergeShip.GoronBreaksBombWalls") (default on): the Goron breaks Hyrule's
// bombable walls and wall boulders with its punch and its spiked roll (MM's DMG_GORON_PUNCH / DMG_GORON_SPIKES,
// carried as MERGESHIP_GORON_DMG_PUNCH, Ocarina of Time's hammer-swing bit). Dodongo's Cavern's entrance -- the
// boulder on Death Mountain Trail and the wall inside the cavern's mouth -- keeps Ocarina of Time's rule: a bomb or
// a bomb flower. Implemented in soh/MergeShip/forms/MergeShipGoronLib.c.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The AC bumper flags a bomb wall's collider uses this frame: `flags` (its own) plus the Goron's attack bit
// while the player is the Goron and the option is on; without that bit when the player is the Goron and the
// option is off, or the wall is Dodongo's Cavern's entrance. Call before CollisionCheck_SetAC.
u32 MergeShip_BombWallBumper(PlayState* play, Actor* wall, u32 flags);

// MergeShip_BombWallBumper applied to every element of `collider` (cylinder, quad, tris or joint spheres), whose
// own bumper flags are `flags`. Call every frame before CollisionCheck_SetAC.
void MergeShip_BombWallSetBumper(PlayState* play, Actor* wall, Collider* collider, u32 flags);

// For walls that accept only an explosive's hit (Actor_GetCollidedExplosive): true (and the hit consumed) when
// the collider was hit by the Goron.
s32 MergeShip_BombWallHitByGoron(PlayState* play, Collider* collider);

// For walls that break on a nearby explosion rather than a hit (Death Mountain Crater's): true when the Goron's
// punch struck this wall's collision last frame, or its spiked roll ran into it (option on).
s32 MergeShip_Goron_StruckBombWall(PlayState* play, DynaPolyActor* wall);

// The Goron's punch struck a dynamic poly `bgId` (MM func_808401F4's wall test); recorded for the check above.
void MergeShip_Goron_RecordStruckBg(PlayState* play, s32 bgId);

// Harness-only evidence log (the Goron's hits, bomb walls disappearing); called by the Goron every frame.
void MergeShip_BombWalls_DebugLog(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif
