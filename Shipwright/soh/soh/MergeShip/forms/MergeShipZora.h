#pragma once

// MergeShip2 -- the Zora form's entry points, called from Ocarina of Time's player (z_player.c) and player
// drawing (z_player_lib.c). The bodies are Majora's Mask's (2ship2harkinian/mm), in
// soh/MergeShip/forms/MergeShipZora.inc (player) and MergeShipZoraDraw.inc (drawing). Only the player's
// own translation units include this header.

#include "z64.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- MM PlayerMeleeWeaponAnimation values the Zora uses (MM z64player.h numbering). ----
// The Zora keeps MM's value in gMergeShipZora.meleeWeaponAnimation; Player::meleeWeaponAnimation holds an
// Ocarina of Time value with no special meaning to Hyrule's actors (see MergeShip_Zora_OoTMWA).
#define MM_PLAYER_MWA_FLIPSLASH_START 16
#define MM_PLAYER_MWA_JUMPSLASH_START 17
#define MM_PLAYER_MWA_ZORA_JUMPKICK_START 18
#define MM_PLAYER_MWA_FLIPSLASH_FINISH 19
#define MM_PLAYER_MWA_JUMPSLASH_FINISH 20
#define MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH 21
#define MM_PLAYER_MWA_ZORA_PUNCH_LEFT 27
#define MM_PLAYER_MWA_ZORA_PUNCH_COMBO 28
#define MM_PLAYER_MWA_ZORA_PUNCH_KICK 29
#define MM_PLAYER_MWA_SPIN_ATTACK_1H 30

// Zora state MM keeps on its Player that Ocarina of Time's has no field for.
typedef struct MergeShipZoraState {
    s8 meleeWeaponAnimation; // MM Player::meleeWeaponAnimation (MM numbering)
    u8 prevBoots;            // MM Player::prevBoots (MM PlayerBoots)
    s32 blureIndex2;         // MM meleeWeaponEffectIndex[1]: the right fin's trail
    u8 blureValid;
    s16 magicTimer;          // MM interfaceCtx->magicConsumptionTimer while the barrier drains magic
    u8 barrierDraining;      // MM MAGIC_STATE_CONSUME_GORON_ZORA(_SETUP) is active for the barrier (2 = SETUP)
    u8 barrierRequest;       // MM PLAYER_STATE1_10: the barrier button is held this frame
    Vec3f unkAF0;            // MM Player::unk_AF0[0], the Zora's per-limb scale for the current frame
    u32 lastUpdateFrame;     // play->gameplayFrames of the last Zora update (detects a fresh transformation)
    u8 ocarinaStaffPos;      // last seen position of the playing staff (a new note strums the guitar)
} MergeShipZoraState;

extern MergeShipZoraState gMergeShipZora;

// MM `currentBoots >= PLAYER_BOOTS_ZORA_UNDERWATER`: the Zora sunk to walk on the bottom. Where Ocarina of Time
// tests PLAYER_BOOTS_IRON for walking underwater, MM tests this for the Zora.
static inline s32 MergeShip_Zora_WalksOnBottom(Player* player) {
    return (player->mergeShipForm == PLAYER_FORM_ZORA) && (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER);
}

// ---- items / fin boomerangs ----
void MergeShip_Zora_InitZoraBoomerangIA(PlayState* play, Player* this);
s32 MergeShip_Zora_func_80830B88(PlayState* play, Player* this);
s32 MergeShip_Zora_func_80830E30(Player* this, PlayState* play);
void MergeShip_Zora_StartChangingHeldItem(Player* this, PlayState* play);
s32 MergeShip_Zora_WaitToFinishItemChange(PlayState* play, Player* this);
LinkAnimationHeader* MergeShip_Zora_BoomerangWaitAnim(Player* this, s32 which);
s32 MergeShip_Zora_func_8083868C(PlayState* play, Player* this);

// ---- melee ----
s32 MergeShip_Zora_func_808335F4(Player* this);
void MergeShip_Zora_func_80833864(PlayState* play, Player* this, s32 meleeWeaponAnim);
void MergeShip_Zora_func_808395F0(PlayState* play, Player* this, s32 meleeWeaponAnim, f32 linearVelocity,
                                  f32 yVelocity);
s32 MergeShip_Zora_func_808396B8(PlayState* play, Player* this);
s32 MergeShip_Zora_func_80839770(Player* this, PlayState* play);
s32 MergeShip_Zora_ActionHandler_7(Player* this, PlayState* play);
s32 MergeShip_Zora_ActionHandler_8(Player* this, PlayState* play);
s32 MergeShip_Zora_ActionHandler_10(Player* this, PlayState* play);
s32 MergeShip_Zora_ActionHandler_11(Player* this, PlayState* play);
s32 MergeShip_Zora_ActionHandler_Roll(Player* this, PlayState* play);
s32 MergeShip_Zora_SkipSwordQuads(Player* this);

// ---- guard / barrier ----
void MergeShip_Zora_func_8082F164(Player* this, u16 button);
void MergeShip_Zora_UpdateBarrier(Player* this, PlayState* play);
void MergeShip_Zora_ResetBarrierColliders(Player* this, PlayState* play);

// ---- swimming ----
void MergeShip_Zora_func_808353DC(PlayState* play, Player* this);
s32 MergeShip_Zora_func_8083B3B4(PlayState* play, Player* this, Input* input);
void MergeShip_Zora_func_8083B73C(PlayState* play, Player* this, s16 yaw);
void MergeShip_Zora_func_8083B930(PlayState* play, Player* this);
void MergeShip_Zora_func_8083BB4C(PlayState* play, Player* this);
void MergeShip_Zora_func_8083BF54(PlayState* play, Player* this);
void MergeShip_Zora_UpdateCommonEarly(Player* this, PlayState* play);
void MergeShip_Zora_UpdateBootsChange(Player* this, PlayState* play);
s32 MergeShip_Zora_IsSwimAction(Player* this);
void MergeShip_Zora_WaterfallGate(PlayState* play, Player* this); // option MergeShip.ZoraWaterfallGate
void MergeShip_Zora_Buoyancy(Player* this); // MM func_808475B4
void MergeShip_Zora_SwimSpeed(Player* this, f32* speed, f32 speedTarget, s16 yawTarget); // MM func_8084748C

// ---- camera ----
void MergeShip_Zora_UpdateCamAndSeqModes(PlayState* play, Player* this);

// ---- other MM form branches ----
LinkAnimationHeader* MergeShip_Zora_DoorOpenAnim(Player* this, s32 left);
s32 MergeShip_Zora_UpdateBodyBurn(PlayState* play, Player* this);
s32 MergeShip_Zora_DamageCheck(PlayState* play, Player* this, s32 hotFloor);
s32 MergeShip_MM_BurnOnFireHit(PlayState* play, Player* this); // MM func_8083456C for the Deku and the Zora
s32 MergeShip_Zora_RunningDive(PlayState* play, Player* this);
s32 MergeShip_Zora_Frozen(PlayState* play, Player* this);
s32 MergeShip_Zora_IsAirAction(Player* this);
s32 MergeShip_Zora_IsAction28(Player* this);
void MergeShip_Zora_OnSetupAction(PlayState* play, Player* this);
void MergeShip_Zora_UpdateBlinking(Player* this);
void MergeShip_Zora_Init(Player* this, PlayState* play);
void MergeShip_Zora_Destroy(Player* this, PlayState* play);

// ---- MM Player_ActionHandler_12's climb and MM Player_Action_33, for every MM form (climbing up a ledge, and
// out of the water onto one) ----
void MergeShip_MM_ClimbLedgeSetup(PlayState* play, Player* this, LinkAnimationHeader** anim);
void MergeShip_MM_Action_33(Player* this, PlayState* play);

// ---- drawing (MergeShipZoraDraw.inc, in z_player_lib.c) ----
s32 MergeShip_Zora_OverrideRootLimb(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot);
void MergeShip_Zora_OverrideTorsoLimb(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot);
void MergeShip_Zora_OverrideHandLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                     Vec3s* rot);
void MergeShip_Zora_OverrideHatLimb(Player* player);
void MergeShip_Zora_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex);
void MergeShip_Zora_DrawShield(PlayState* play, Player* player);
void MergeShip_Zora_DrawBarrierBody(PlayState* play, Player* player);
void MergeShip_Zora_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Vec3f* pos, Vec3s* rot);

#ifdef __cplusplus
}
#endif
