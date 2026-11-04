#pragma once

// MergeShip2 -- the Bremen Mask's followers (MergeShipBremen.c).
//
// In Majora's Mask nothing watches the mask itself: Player_Action_11 (MergeShip_MMMasks_Action_March here) sets
// PLAYER_STATE3_20000000 while Link marches, and the followers read that flag. MM's dogs (ovl_En_Dg) pick one
// follower that trots after Link barking, and MM's cucco chicks (ovl_En_Nwc) line up behind him on a chain of
// points Grog (ovl_En_Hs) keeps. Here Ocarina of Time's dogs (ovl_En_Dog) and cuccos (ovl_En_Niw) do the same.
// Hyrule has no Grog, so the chain he keeps lives in this file. Gated by
// CVAR_ENHANCEMENT("MergeShip.BremenFollowers"), default on.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The player is marching with the Bremen Mask (PLAYER_STATE3_20000000) and followers are enabled.
s32 MergeShip_BremenMarching(PlayState* play);

// MM func_801A46F8 (code_8019AF00.c): true on one of the Bremen March's beats, which the sequence signals through
// channel 15's io port 0 of the fanfare player (the port is consumed). Not 1:1: also true while the fanfare player
// is not playing at all, so a dog is not left waiting for a march that could not be played.
s32 MergeShip_Bremen_OnMarchBeat(void);

// ---- MM Grog's chick chain (z_en_hs.c: nwcPos, home.rot.z) ---------------------------------------------------
#define MERGESHIP_BREMEN_CHAIN_COUNT 20 // MM EnHs::nwcPos[20]

// MM EnHs_Update's func_80952F00: moves the chain after the player. Safe to call from every follower every
// frame: it runs once per frame. MM func_80952C50 (EnHs_Init) starts the chain on the player at a new scene.
void MergeShip_Bremen_UpdateChain(PlayState* play);
// Starts the chain over on the player at its next update (Ocarina of Time's En_Niw overlay reset: a new scene).
void MergeShip_Bremen_ResetChain(void);
// MM EnHs::nwcPos.
Vec3f* MergeShip_Bremen_GetChain(void);
// MM Grog's actor.home.rot.z: the count of followers in line, incremented by 2.
s16 MergeShip_Bremen_GetFollowCount(void);

// MM EnNwc_IsFound: the player marches (with followers enabled) and `actor` is within 100 units of him.
// Not 1:1: false once the line is full (10 followers, MM has exactly 10 chicks), which MM never needed to check.
s32 MergeShip_Bremen_IsFound(Actor* actor, PlayState* play);
// MM EnNwc_CheckFound's bookkeeping once found: plays MM's NA_SE_SY_CHICK_JOIN_CHIME at the line's pitch, bumps
// the count and returns the follower's place in line (MM stores it in its own actor.home.rot.z: odd, 1..19).
s16 MergeShip_Bremen_JoinLine(void);
// MM EnNwc_PlayerReleasedBremanMarch: the march is over (or followers were disabled).
s32 MergeShip_Bremen_PlayerReleasedMarch(PlayState* play);
// MM EnNwc_Follow on release: grog->actor.home.rot.x = grog->actor.home.rot.z = 0.
void MergeShip_Bremen_ResetLine(void);

#ifdef __cplusplus
}
#endif
