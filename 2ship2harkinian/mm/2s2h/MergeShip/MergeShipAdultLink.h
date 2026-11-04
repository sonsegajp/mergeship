#pragma once

// MergeShip2 -- Ocarina of Time's adult Link in Majora's Mask (2Ship), the reverse of SoH's MergeShipChildLink (MM's
// human as Ocarina of Time's child).
//
// Design: a flag, not a sixth PlayerTransformation. MM's player, its actors and its save index forms by
// PLAYER_FORM_FIERCE_DEITY..PLAYER_FORM_HUMAN (PLAYER_FORM_MAX 5): 57 tables are sized by PLAYER_FORM_MAX and 302
// `== PLAYER_FORM_HUMAN` checks in 107 files decide what the human may do (doors, NPC reactions, items, C buttons, the
// B button set in ItemEquips::buttonItems[4], mask rules, the pause screen). Adult Link is MM's human in every one of
// those rules -- he uses the human's items, B sword and C buttons, and MM's transformation masks turn him into the four
// forms and back, exactly as they do the young human. Only his body differs, so what differs is the human's *row* of the
// form-indexed tables that describe the body: MergeShip_AdultLink_Sync swaps those rows (skeleton, PlayerAgeProperties,
// hand / sheath / waist / first-person display lists, eye and mouth textures, leg IK data) between MM's young human and
// Ocarina of Time's adult (object_link_boy and sAgeProperties[LINK_AGE_ADULT] from oot-assets.o2r), the way SoH's
// MergeShip_MMChildLink_Sync swaps its child's. PLAYER_FORM_MAX does not grow, so no table can be read past its end; the
// swapped rows are named one by one in MergeShipAdultLink.c and each swap asserts its table's size.
//
// Adult Link's body:
//   - model: gLinkAdultSkel and his own hands, sheath, waist, eyes and mouth; Ocarina of Time's equipment in his hands
//     (the Master Sword, Biggoron's Sword / the Giant's Knife, the Hylian and Mirror Shields, the Master Sword's scabbard
//     with the shield on it, the Iron and Hover Boots, the gauntlets) as Ocarina of Time draws it; the tunic's colour
//     is set as Ocarina of Time's Player_DrawImpl sets it (sTunicColors, the cosmetic editor's human tunic colour);
//   - measurements: Ocarina of Time's sAgeProperties[LINK_AGE_ADULT] (ceiling, ledge, climb and water heights, wall
//     radius, 1.0 root translation), OoT's adult boot data (sBootData[PLAYER_BOOTS_KOKIRI]: run speed 6.0), OoT's
//     Player_GetHeight (68), OoT's adult leg IK and door animations (KNOB_ANIM_ADULT);
//   - animations: every player animation Ocarina of Time also has (same name, same length) is Ocarina of Time's
//     (oot/objects/gameplay_keep, as OoT's adult plays it); the animations only MM has stay MM's;
//   - voice: MM's voice bank 0 and footstep bank 0x80, which are Ocarina of Time's adult Link (MM keeps them for the
//     Fierce Deity).
//
// State: ShipSaveInfo::mergeShipHumanAge ("mergeShipHumanAge"), so a save, the owl save and the Song of Time's new
// cycle keep the age. The host's Lost Woods -> Clock Tower transfer sets it with MergeShip_Hyrule_SetArrivalAge before
// the player spawns (MergeShipHyrule.h). Gated by MERGESHIP_ADULT_LINK_CVAR (default on) and oot-assets.o2r.

#include "ultra64.h"
#include "z64math.h" // Vec3f

#define MERGESHIP_ADULT_LINK_CVAR "gEnhancements.MergeShip.AdultLink"
// Debug: 0 the save decides (default), 1 the human is adult, 2 the human is young. Written into the save when set.
#define MERGESHIP_ADULT_LINK_DEBUG_CVAR "gGeneral.MergeShipAdultLink"

// ShipSaveInfo::mergeShipHumanAge.
#define MERGESHIP_HUMAN_AGE_YOUNG 0 // MM's own human
#define MERGESHIP_HUMAN_AGE_ADULT 1 // Ocarina of Time's adult Link

// Ocarina of Time's LinkAge numbering (OoT z64save.h), as the host passes it.
#define MERGESHIP_OOT_LINK_AGE_ADULT 0
#define MERGESHIP_OOT_LINK_AGE_CHILD 1

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;
struct Actor;
struct KnobDoorActor;

// ---- state (MergeShipAdultLink.c) ---------------------------------------------------------------------------------------
s32 MergeShip_AdultLink_Enabled(void); // the option is on and the Ocarina of Time archive is mounted
// The save's age with the debug override applied: true when the human is to be the adult.
s32 MergeShip_AdultLink_WantsAdult(void);
void MergeShip_AdultLink_SetSaveAge(u8 age); // MERGESHIP_HUMAN_AGE_*
u8 MergeShip_AdultLink_GetSaveAge(void);
// The human's form tables hold the adult's rows (the player was built as the adult).
s32 MergeShip_AdultLink_TablesAdult(void);
// An Anchor ghost's age: switch the rows to it (adult only while the adult is enabled); returns the age before.
s32 MergeShip_AdultLink_UseTables(s32 adult);
// `player` is the human, built as Ocarina of Time's adult.
s32 MergeShip_AdultLink_IsActive(struct Player* player);
// Player_Init: swaps the human's rows to what MergeShip_AdultLink_WantsAdult says, before the skeleton is chosen.
void MergeShip_AdultLink_Sync(void);
// Rebuilds the player (MM's own re-init after a transformation, func_8012301C) when the wanted age differs from the
// built one. Every frame from MergeShipAdultLink.cpp; returns true when it started a rebuild.
s32 MergeShip_AdultLink_UpdateRebuild(struct PlayState* play);
// Player_PlayAnimSfx, before a general sound: the transformation voice (NA_SE_PL_TRANSFORM_VOICE) from the adult is
// Adult Link's own scream, as in Hyrule ("Adult Link Transformation Scream", MergeShipAdultLink.cpp).
void MergeShip_AdultLink_TransformVoice(struct Player* player, u16 sfxId);
// The hookshot pulling the adult: Ocarina of Time's adult "hyah!" (BUG-0020). False: play MM's own voice.
s32 MergeShip_AdultLink_HookshotVoice(struct Player* player);

// ---- player (z_player.c, z_actor.c, z_player_lib.c) ---------------------------------------------------------------------
f32 MergeShip_AdultLink_Height(struct Player* player, f32 height); // Player_GetHeight (OoT: 68 for the adult)
// Player_DrawGetItem: where the adult holds a get-item up -- as Ocarina of Time places it (between his hands, or his
// left hand for an exchange item), from his hands' own positions this frame (MergeShipAdultLink.c). No-op for anyone
// else.
void MergeShip_AdultLink_GetItemRefPos(struct Player* player, Vec3f* refPos);
// func_80123140: OoT's adult boot data row for the human's own boots (sBootData[PLAYER_BOOTS_KOKIRI]).
s16* MergeShip_AdultLink_BootRegs(struct Player* player, s16* bootRegs);
// Player_Door_Knob: the adult opens knob doors as Ocarina of Time's adult (KNOB_ANIM_ADULT: MM's doorA/B_free and the
// door's Fierce Deity animation). Returns true when it applied (the caller then plays doorA/B_free).
s32 MergeShip_AdultLink_KnobDoor(struct Player* player, s32 doorDirection, u8* doorAnimIndex);

// ---- drawing (MergeShipAdultLink.c) --------------------------------------------------------------------------------------
// Player_DrawImpl, before the skeleton: the tunic colour (OoT Player_DrawImpl).
// Before each limb (Player_OverrideLimbDrawGameplayCommon): the adult's tunic colour again (the environment colour).
void MergeShip_AdultLink_LimbTunicColor(struct PlayState* play, struct Player* player);
void MergeShip_AdultLink_DrawSetup(struct PlayState* play, struct Actor* actor, s32 playerForm);
// End of Player_OverrideLimbDrawGameplayDefault: Ocarina of Time's hands, swords, shields and scabbard.
void MergeShip_AdultLink_OverrideLimb(struct PlayState* play, struct Player* player, s32 limbIndex, Gfx** dList);
// End of Player_OverrideLimbDrawGameplayFirstPerson (OoT's first-person arms, incl. the right forearm MM leaves out).
void MergeShip_AdultLink_OverrideLimbFirstPerson(struct PlayState* play, struct Player* player, s32 limbIndex,
                                                 Gfx** dList);
// MM's Hookshot on the adult is fitted to his hand: its hand point / hook (MM D_801C0DD8 / D_801C0DE4) and the flying
// hook's scale follow (as Hyrule's MergeShip_MMEquip_HookshotOffset / HookshotTipScale).
// BUG-0059: `mmPoint` (MM's young-Link right hand) under the adult's right-hand fit; returns the fit's scale.
f32 MergeShip_AdultLink_RightFitPoint(Vec3f* mmPoint, Vec3f* out);
Vec3f* MergeShip_AdultLink_HookshotPoint(struct Player* player, Vec3f* mmPoint);
f32 MergeShip_AdultLink_HookshotTipScale(struct Player* player);
// Player_PostLimbDrawGameplay's shield on the back: none for the adult's Ocarina of Time shields (they are part of his
// scabbard list), MM's shields fitted to his back.
Gfx* MergeShip_AdultLink_BackShieldDL(struct PlayState* play, struct Player* player, Gfx* dList);
// GameInteractor OnPlayerPostLimbDraw: boots and gauntlets (OoT Player_DrawImpl after the skeleton).
void MergeShip_AdultLink_PostLimbDraw(struct PlayState* play, struct Player* player, s32 limbIndex);
// Player_PostLimbDrawGameplay's head, around a mask's draw: the adult human's head is not the young head MM's masks are
// modelled on, so the mask is drawn with its fit (include/mergeship/MaskFit.h, shared with Hyrule) -- on the matrix stack
// for the masks' moving parts, and as the skeleton matrix list (segment 0x0D) the mask's display list loads its head
// matrix from. Begin returns whether it fitted (End restores the skeleton's list only then).
s32 MergeShip_AdultLink_MaskFitBegin(struct PlayState* play, struct Player* player);
void MergeShip_AdultLink_MaskFitEnd(struct PlayState* play, s32 fitted);
// SkelAnime_DrawFlexLod: the matrix list of the skeleton being drawn (segment 0x0D), put back after a fitted mask.
extern Mtx* gMergeShipFlexMtx;

// ---- the host (MergeShipHyrule.h) ---------------------------------------------------------------------------------------
// Ocarina of Time's age of the player arriving from Hyrule (MERGESHIP_OOT_LINK_AGE_*): the human becomes that age.
void MergeShip_AdultLink_SetArrivalAge(u8 ootAge);

// Diagnostics (MergeShipTest.cpp); no-op unless a harness script runs.
void MergeShipTest_Log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
