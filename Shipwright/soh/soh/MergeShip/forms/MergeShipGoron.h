#pragma once

// MergeShip2 -- Majora's Mask's Goron Link, ported from 2ship2harkinian/mm into Ocarina of Time's player.
//
// The player half lives in MergeShipGoron.inc (included at the end of z_player.c, so it can use the
// player's static helpers); the drawing half that z_player_lib.c needs lives in MergeShipGoronLib.c.
// Only the player's own translation units include this header.

#include "z64.h"
#include "soh/MergeShip/MergeShipBombWalls.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- MM PlayerMeleeWeaponAnimation values the Goron uses (MM z64player.h numbering). ----
// As for the Zora, the Goron keeps MM's value in gMergeShipGoron.meleeWeaponAnimation. Player's own
// meleeWeaponAnimation holds PLAYER_MWA_FORWARD_SLASH_1H while a punch is out, so Ocarina of Time's code
// and actors that index tables by it (EnKanban's sCutTypes, the camera's STILL range) stay in bounds.
#define MM_PLAYER_MWA_GORON_PUNCH_LEFT 24
#define MM_PLAYER_MWA_GORON_PUNCH_RIGHT 25
#define MM_PLAYER_MWA_GORON_PUNCH_BUTT 26

// ---- Majora's Mask damage types the Goron deals, as Ocarina of Time damage flags. ----
// MM's DMG_* bits index MM's enemy damage tables; Hyrule's enemies read Ocarina of Time's. Each Goron
// attack is given the Hyrule damage type that plays the same role: the punch and the spiked roll hit like
// the Megaton Hammer's swing (they break what MM's punch breaks), the ground pound like its ground slam
// (it flips Tektites and stuns what MM's pound stuns, together with actorCtx.unk_02 = 4), and the plain
// roll uses the unused "unknown 1" bit, which Hyrule's damage tables leave at no effect, as MM's tables do
// for DMG_NORMAL_ROLL.
#define MERGESHIP_GORON_DMG_PUNCH DMG_HAMMER_SWING   // MM DMG_GORON_PUNCH
#define MERGESHIP_GORON_DMG_POUND DMG_HAMMER_JUMP    // MM DMG_GORON_POUND
#define MERGESHIP_GORON_DMG_SPIKES DMG_HAMMER_SWING  // MM DMG_GORON_SPIKES
#define MERGESHIP_GORON_DMG_NORMAL_ROLL DMG_UNKNOWN_1 // MM DMG_NORMAL_ROLL

// Goron state MM keeps on its Player that Ocarina of Time's has no field for.
typedef struct MergeShipGoronState {
    s8 meleeWeaponAnimation; // MM Player::meleeWeaponAnimation (MM numbering) while a punch is out
    u8 bgCheckFlag800;       // MM BGCHECKFLAG_PLAYER_800: rolling, do not snap to a floor below
    u8 magicDrain;           // MM MAGIC_STATE_CONSUME_GORON_ZORA(_SETUP) for the spiked roll: 1 setup, 2 consume
    s16 magicTimer;          // MM interfaceCtx->magicConsumptionTimer while the spikes drain magic
    u8 tireMarkActive;       // the tire mark effect has been initialised for this player
    s16 ocarinaStaffPos;     // last seen playing-staff position, to detect a new note (MM ocarinaButtonIndex)
} MergeShipGoronState;

extern MergeShipGoronState gMergeShipGoron;

// ---- player (MergeShipGoron.inc, in z_player.c) ----
void MergeShip_Goron_Action_96(Player* this, PlayState* play);
void MergeShip_Goron_InitCommon(Player* this, PlayState* play);
void MergeShip_Goron_OnSetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc);
s32 MergeShip_Goron_ActionHandler_7(Player* this, PlayState* play);
void MergeShip_Goron_Action_84(Player* this, PlayState* play);
void MergeShip_Goron_func_8082FA5C(PlayState* play, Player* this, s32 meleeWeaponState);
s32 MergeShip_Goron_ActionHandler_6(Player* this, PlayState* play);
void MergeShip_Goron_func_80836B3C(PlayState* play, Player* this);
s32 MergeShip_Goron_ActionHandler_11(Player* this, PlayState* play);
void MergeShip_Goron_Action_18(Player* this, PlayState* play);
s32 MergeShip_Goron_EnterDeepWater(PlayState* play, Player* this);
void MergeShip_Goron_AdjustBgCheckFlags(Player* this, u32* flags);
s32 MergeShip_Goron_UpdateBgCheckInfo(PlayState* play, Player* this, f32 wallCheckHeight, f32 wallCheckRadius,
                                      f32 ceilingCheckHeight, u32 flags);
s32 MergeShip_Goron_HandleSlopes(PlayState* play, Player* this, CollisionPoly* floorPoly, s32* result);
void MergeShip_Goron_UpdateCommon(Player* this, PlayState* play);
void MergeShip_Goron_UpdatePreAction(PlayState* play, Player* this);
s32 MergeShip_Goron_RequestCamera(PlayState* play, Player* this);
void MergeShip_Goron_UpdateColliders(PlayState* play, Player* this);
void MergeShip_Goron_SetColliderAT(PlayState* play, Player* this);
s32 MergeShip_Goron_IgnoresFloorDamage(PlayState* play, Player* this);
s32 MergeShip_Goron_ShieldBounced(Player* this);
LinkAnimationHeader* MergeShip_Goron_DoorOpenAnim(Player* this, s32 doorDirection);
s32 MergeShip_Goron_Draw(PlayState* play, Player* this);
void MergeShip_Goron_DrawEffects(PlayState* play, Player* this);

// ---- drawing (MergeShipGoronLib.c, used by z_player_lib.c and the player) ----
// MM struct_80122D44 afterimages (Player::unk_3D0): func_80122BA4 records the current matrix, func_80122C20
// fades them, func_80122D44 draws them.
void MergeShip_Goron_func_80122BA4(PlayState* play, struct_80122D44_arg1* arg1, s32 arg2, s32 alpha);
void MergeShip_Goron_func_80122C20(PlayState* play, struct_80122D44_arg1* arg1);
void MergeShip_Goron_func_80122D44(PlayState* play, struct_80122D44_arg1* arg1);
// MM AnimatedMat_DrawXlu for object_link_goron_Matanimheader_013138 / _014684.
void MergeShip_Goron_DrawMatAnim013138(PlayState* play);
void MergeShip_Goron_DrawMatAnim014684(PlayState* play);
// MM func_80124618: keyframed scale.
typedef struct MergeShipScaleKey {
    s16 frame;
    Vec3s scale;
} MergeShipScaleKey;
void MergeShip_Goron_func_80124618(MergeShipScaleKey* keys, f32 curFrame, Vec3f* out);
// MM Player_PostLimbDrawGameplay's Goron parts. Returns true for the left hand when the Goron's fists
// replaced the sword collider update.
s32 MergeShip_Goron_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex, Gfx** dList);
// MM sPlayerStrengths[PLAYER_FORM_GORON] as an Ocarina of Time strength.
s32 MergeShip_Goron_GetStrength(void);

#ifdef __cplusplus
}
#endif
