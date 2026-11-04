#pragma once

// MergeShip2 option CVAR_ENHANCEMENT("MergeShip.GoronHammerObjects") (default on): the Goron's punch and ground
// pound work what Ocarina of Time's Megaton Hammer works. Implemented in soh/MergeShip/MergeShipHammerObjects.cpp.
//
// The Goron's punch already carries the hammer-swing damage bit and its pound the hammer-jump bit
// (MergeShipGoron.h), so the hammer objects whose collider faces the player (Obj_Hamishi's bronze boulder) take them
// as they are. What this adds:
// - Hammer targets that face up (Obj_Switch's rusted floor switch, Bg_Hidan_Hamstep's hammer step,
//   Bg_Hidan_Hrock's sinking pillars): the hammer comes down on them, the Goron's punch goes straight ahead over
//   them. MergeShip_GoronHammer_Strikes lets the fist reaching over the face, or the pound landing on or beside
//   it, count as the hammer's blow.
// - Bg_Hidan_Dalm's totems break only for Player::meleeWeaponAnimation's hammer swings; an OnVanillaBehavior hook
//   (VB_HAMMER_TOTEM_BREAK) lets the Goron's punch break them too.
// - Option off: the Goron's attacks do not register on the hammer-only objects at all (MergeShip_GoronHammer_SkipHit,
//   called by CollisionCheck_AC). Objects a bomb opens too (boulders, large crates, hidden grottos, small rocks)
//   are skipped only when "MergeShip.GoronBreaksBombWalls" is off as well.
// Nothing changes for enemies: the Goron's damage flags are not touched.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The player is the Goron and the option is on.
s32 MergeShip_GoronHammer_Enabled(PlayState* play);

// For hammer targets whose struck face is horizontal: true when (option on) the Goron's punch reaches over the face
// centred on `top` (radius `radius`) or its ground pound lands on it or beside it. Call where the actor tests its
// collider's AC_HIT, and treat true as that hit.
s32 MergeShip_GoronHammer_Strikes(PlayState* play, Actor* actor, const Vec3f* top, f32 radius);

// CollisionCheck_AC: true when the AT collider `colAT` (the Goron's) must not hit the AC collider `colAC` (a hammer
// object) because the option is off.
s32 MergeShip_GoronHammer_SkipHit(Collider* colAT, Collider* colAC);

// The Goron's punch struck the dynamic poly `bgId` last frame (MergeShip_Goron_RecordStruckBg); consumed when true.
// Implemented in soh/MergeShip/forms/MergeShipGoronLib.c.
s32 MergeShip_Goron_PunchStruckBg(PlayState* play, s32 bgId);

#ifdef __cplusplus
}
#endif
