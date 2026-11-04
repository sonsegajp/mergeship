#pragma once

// MergeShip2 -- Majora's Mask's Deku Link, ported from 2ship2harkinian/mm into Ocarina of Time's player.
//
// Prototypes for the hooks z_player.c calls before MergeShipDeku.inc (which is included at the end of
// z_player.c). Only z_player.c includes this header.

struct Player;
struct PlayState;

// MM Player_SetAction's Deku resets (flower actions restore shadow/scale, MM-only state is cleared).
void MergeShip_Deku_OnSetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc);
// MM func_80839A84: A as Deku -- burrow into a flower, otherwise the spin attack.
s32 MergeShip_Deku_func_80839A84(PlayState* play, Player* this);
// MM Player_ActionHandler_11 (R guard) for the Deku.
s32 MergeShip_Deku_ActionHandler_11(Player* this, PlayState* play);
// MM Player_Action_18 (guarding) for the Deku.
void MergeShip_Deku_Action_18(Player* this, PlayState* play);
// MM Player_Action_25 (midair) for the Deku.
void MergeShip_Deku_Action_25(Player* this, PlayState* play);
// MM func_8083827C (leaving the ground) for the Deku.
void MergeShip_Deku_func_8083827C(Player* this, PlayState* play);
// MM func_8083BB4C's Deku branch (entering water).
void MergeShip_Deku_EnterWater(PlayState* play, Player* this);
// MM func_8083784C: about to touch water while hops remain.
s32 MergeShip_Deku_func_8083784C(Player* this);
// MM Player_UpdateBodyBurn's Deku branch.
void MergeShip_Deku_UpdateBodyBurn(PlayState* play, Player* this);
// MM Player_Action_95's tire mark (func_800AE930), in MergeShipGoron.inc with the EffectTireMark port.
void MergeShip_Deku_LayTireMark(PlayState* play, Player* this);
// MM func_80834600: the Deku guard's shield cylinder bounced a hit.
s32 MergeShip_Deku_GuardBounced(Player* this);
// Per-frame, before the action function runs (MM Player_UpdateCommon's Deku parts).
void MergeShip_Deku_UpdatePreAction(PlayState* play, Player* this);
// MM Player_UpdateCommon's collider block for the Deku. Returns true when it replaced OoT's.
s32 MergeShip_Deku_UpdateColliders(PlayState* play, Player* this);
// End of Player_UpdateCommon (MM's shape.yOffset = unk_ABC).
void MergeShip_Deku_UpdatePostCommon(PlayState* play, Player* this);
// MM Player_UpdateCamAndSeqModes's Deku modes (DEKUHIDE, DEKUFLY, guard STILL). Returns true when an MM-only mode
// was requested, in which case Ocarina of Time's Camera_RequestMode must not be called this frame.
s32 MergeShip_Deku_UpdateCamMode(PlayState* play, Player* this, s32* camMode);
// MM func_80837134 (landing) for the Deku.
void MergeShip_Deku_func_80837134(PlayState* play, Player* this);
// Registers ovl_MM_En_Arrow / ovl_MM_Obj_Etcetera if boot registration has not run (MergeShipDekuActors.cpp).
void MergeShip_Deku_EnsureActorsRegistered(void);
// MM func_8083868C's Deku aim (CAM_MODE_DEKUSHOOT).
s32 MergeShip_Deku_RequestAimCamera(PlayState* play, Player* this);
// MM Player_InitBowOrDekuNutIA for the Deku's bubble.
void MergeShip_Deku_InitBubbleIA(PlayState* play, Player* this);
// MM Player_UpperAction_6 for the Deku's bubble.
s32 MergeShip_Deku_UpperAction_6(Player* this, PlayState* play);
// MM Player_Draw's Deku flower. Returns true when the player itself must not be drawn.
s32 MergeShip_Deku_DrawFlower(PlayState* play, Player* this);
