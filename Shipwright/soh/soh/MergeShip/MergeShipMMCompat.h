#pragma once

// MergeShip2 -- compile Majora's Mask's player against Ocarina of Time's engine.
//
// The two decompilations describe the same engine with different vocabulary. MM's player calls
// 1058 distinct functions and macros; 654 of those it defines itself, 199 already exist in SoH
// under the same name, and the rest are overwhelmingly renames and small macros rather than
// missing functionality -- LinkAnimation_* became PlayerAnimation_*, Player_SetupAction became
// Player_SetAction, and so on. This header is that vocabulary, so MM's source can be compiled
// rather than transcribed.
//
// It is force-included into MM-derived translation units only. Nothing here is visible to
// Ocarina of Time's own code.

// ---- animation ------------------------------------------------------------------------------
#define PlayerAnimationHeader LinkAnimationHeader
#define PlayerAnimation_Update LinkAnimation_Update
#define PlayerAnimation_Change LinkAnimation_Change
#define PlayerAnimation_OnFrame LinkAnimation_OnFrame
#define PlayerAnimation_PlayOnce LinkAnimation_PlayOnce
#define PlayerAnimation_PlayOnceSetSpeed LinkAnimation_PlayOnceSetSpeed
#define PlayerAnimation_PlayLoop LinkAnimation_PlayLoop
#define PlayerAnimation_PlayLoopSetSpeed LinkAnimation_PlayLoopSetSpeed
#define PlayerAnimation_LoadToJoint LinkAnimation_LoadToJoint
#define PlayerAnimation_CopyJointToMorph LinkAnimation_CopyJointToMorph
#define PlayerAnimation_End LinkAnimation_EndFrame
#define PlayerAnimation_GetFrameCount Animation_GetLastFrame

// ---- player action machinery ---------------------------------------------------------------
#define Player_SetAction Player_SetupAction
#define Player_SetUpperAction Player_SetUpperActionFunc
#define Player_Anim_PlayOnce Player_AnimPlayOnce
#define Player_Anim_PlayOnceAdjusted Player_AnimPlayOnceAdjusted
#define Player_Anim_PlayLoop Player_AnimPlayLoop
#define Player_Anim_PlayLoopAdjusted Player_AnimPlayLoopAdjusted
#define Player_Anim_PlayOnceMorphAdjusted Player_AnimPlayOnceAdjusted
#define Player_AnimReplace_Setup Player_AnimReplaceApplyFlags

// ---- small arithmetic macros MM adds -------------------------------------------------------
#ifndef ABS_ALT
#define ABS_ALT(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef BINANG_ADD
#define BINANG_ADD(a, b) ((s16)((a) + (b)))
#endif
#ifndef TRUNCF_BINANG
#define TRUNCF_BINANG(f) ((s16)(s32)(f))
#endif

// ---- collision ------------------------------------------------------------------------------
// The two decompilations renamed these; the values are identical.
#define ATELEM_NONE TOUCH_NONE
#define ATELEM_ON TOUCH_ON
#define ATELEM_SFX_NORMAL TOUCH_SFX_NORMAL
#define ACELEM_NONE BUMP_NONE
#define ACELEM_ON BUMP_ON
#define ELEM_MATERIAL_UNK1 ELEMTYPE_UNK1
#define ELEM_MATERIAL_UNK2 ELEMTYPE_UNK2
#define COL_MATERIAL_HIT5 COLTYPE_HIT5
#define COL_MATERIAL_METAL COLTYPE_METAL
#define COL_MATERIAL_NONE COLTYPE_NONE

// ---- player state flags ---------------------------------------------------------------------
// MM names the flags it never documented after their value, and Ocarina of Time names the ones
// *it* documented after their meaning, so the two headers disagree on which bits have names
// rather than on the bits themselves. Defining MM's by value keeps MM's player self-consistent.
//
// CAUTION: this is only safe within MM's player. Ocarina of Time's other actors read
// stateFlags1 through their own names, and the two games do not agree on what every bit means.
// Anywhere an OoT actor tests a flag MM's player sets, the meaning has to be checked by hand.
#define PLAYER_STATE1_4 (1 << 2)
#define PLAYER_STATE1_2000 (1 << 13)
#define PLAYER_STATE1_4000 (1 << 14)
#define PLAYER_STATE1_100000 (1 << 20)
#define PLAYER_STATE1_200000 (1 << 21)
#define PLAYER_STATE2_8 (1 << 3)
#define PLAYER_STATE2_10 (1 << 4)
#define PLAYER_STATE2_80 (1 << 7)
#define PLAYER_STATE2_400 (1 << 10)
#define PLAYER_STATE2_800 (1 << 11)
#define PLAYER_STATE2_20000 (1 << 17)

// ---- animation speeds -----------------------------------------------------------------------
#define PLAYER_ANIM_NORMAL_SPEED (3.0f / 3.0f)
#define PLAYER_ANIM_ADJUSTED_SPEED (2.0f / 3.0f)

// ---- small constants MM's player expects ----------------------------------------------------
#define LIMB_ROOT_ROT 1
#define ITEM_FE 0xFE
#define PLAYER_IA_LAST_USED 0x01
#define PLAYER_MELEE_WEAPON_STATE_0 0
#define PLAYER_UNKAA5_0 0
#define PLAYER_EXPLOSIVE_NONE (-1)
#define PLAYER_MASK_SCENTS 0x13
#define PLAYER_MASK_GIANT 0x14

// ---- sounds ---------------------------------------------------------------------------------
// Termina's own player sounds. They resolve through mm.o2r's audio; until MM's sequence and
// soundfont tables are wired into SoH these fall back to the nearest Hyrule equivalent so the
// player is not silent.
#ifndef NA_SE_PL_JUMP_GROUND
#define NA_SE_PL_JUMP_GROUND NA_SE_PL_JUMP
#endif
#ifndef NA_SE_PL_LAND_GROUND
#define NA_SE_PL_LAND_GROUND NA_SE_PL_LAND
#endif
#ifndef NA_SE_PL_GIANT_WALK
#define NA_SE_PL_GIANT_WALK NA_SE_PL_WALK_GROUND
#endif
