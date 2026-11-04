#pragma once

// MergeShip2 -- Majora's Mask's human Link as Ocarina of Time's child.
//
// With CVAR_ENHANCEMENT("MergeShip.MMChildLink") on (the default) and mm.o2r mounted, PLAYER_FORM_CHILD
// is drawn, voiced and flips like MM's PLAYER_FORM_HUMAN (2ship2harkinian/mm):
//   - model: MM's gLinkHumanSkel, MM's bare hands, waist and face textures (MM z_player_lib.c), wearing and
//            holding Ocarina of Time's equipment by Ocarina of Time's rules (its swords, shields, sheath and
//            items, attached to MM's hands), in MergeShipChildLinkDraw.inc (at the end of z_player_lib.c); the
//            adult's gear the child may use (Master Sword, Biggoron's Sword, gauntlets) as 2Ship's MM human wears it;
//   - sound: every player sound goes through MM's sound engine and MM's human voice bank, as the MM
//            forms' do (MergeShipMMSfx.c), falling back to Ocarina of Time's sound only where MM has none;
//   - moves: MM func_808373F8's ledge jump and MM's landing from it, in MergeShipChildLink.inc (included at
//            the end of z_player.c). Its flips follow "MM Jump Flips" (MergeShip.MMFlips, MergeShipSohFd.h), which
//            also gives them to Ocarina of Time's child, Adult Link and the Fierce Deity.
// Adult Link is untouched. Turning the CVar off rebuilds the child as Ocarina of Time's on the next frame.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif
#include "soh/MergeShip/MergeShipForms.h"

#ifdef __cplusplus
extern "C" {
#endif

// True while the child's skeleton, display lists and textures are MM's human ones. Only
// MergeShip_MMChildLink_Sync changes it, so drawing never mixes the two models.
extern u8 gMergeShipMMChildLink;

// Reads the CVar (and whether mm.o2r is mounted) and swaps the child's form tables to match.
// Returns true when the model changed; the player then rebuilds its skeleton.
s32 MergeShip_MMChildLink_Sync(void);

// Drawing (MergeShipChildLinkDraw.inc, z_player_lib.c).
void MergeShip_MMChildLink_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* pos);
void MergeShip_MMChildLink_OverrideLimbFirstPerson(Player* player, s32 limbIndex, Gfx** dList);
void MergeShip_MMChildLink_SetTunicColor(Color_RGB8* color);
// `swordAndShield`: Player_OverrideLimbDrawPause's PlayerSword and PlayerShield.
void MergeShip_MMChildLink_OverrideLimbPause(PlayState* play, s32 limbIndex, s32 type, Gfx** dList, Vec3f* pos,
                                             u8* swordAndShield);
// The adult's gear on MM's young Link (CVAR_ENHANCEMENT("MergeShip.MMChildAdultGear"), default on): the Master
// Sword and Biggoron's Sword in MM's bare fist and the Master Sword's scabbard on the back (the overrides above), and
// the Silver and Golden Gauntlets' plates on MM's forearms. CaptureArm is called by the gameplay and pause limb
// overrides with each limb's final pos/rot (it keeps the fitted forearm and hand matrices); DrawGauntlets by
// Player_DrawImpl after the skeleton (true when it drew the plates, which replace the Goron's Bracelet).
void MergeShip_MMChildLink_CaptureArm(PlayState* play, s32 isMMChild, s32 limbIndex, Vec3f* pos, Vec3s* rot);
s32 MergeShip_MMChildLink_DrawGauntlets(PlayState* play, s32 form, s32 strength);
// BUG-0099: the Iron and Hover Boots (`boots` 1 / 2) on MM's young Link, after the skeleton (CaptureArm keeps the feet).
void MergeShip_MMChildLink_DrawBoots(PlayState* play, s32 form, s32 boots);
// The adult's bow string (sBowStringData[`stringModel`] 0) under the fit MM's young Link holds the adult's bow with.
void MergeShip_MMChildLink_FitAdultBowString(Player* player, s32 stringModel);

// Behaviour (MergeShipChildLink.inc, z_player.c).
s32 MergeShip_MMChild_func_808373F8(PlayState* play, Player* player, u16 sfxId);
void MergeShip_MMChild_Action_25(PlayState* play, Player* player);
s32 MergeShip_MMChild_LandingAnim(Player* player, LinkAnimationHeader** anim);
LinkAnimationHeader* MergeShip_MMFlips_CosmeticAnim(LinkAnimationHeader* runJumpAnim);
LinkAnimationHeader* MergeShip_MMChild_Anim(Player* player, LinkAnimationHeader* anim);
void MergeShip_MMChildLink_Update(Player* player, PlayState* play);

#ifdef __cplusplus
}
#endif

// The player is Ocarina of Time's child, currently built as Majora's Mask's human.
#define MERGESHIP_MM_CHILD(player) (((player)->mergeShipForm == PLAYER_FORM_CHILD) && gMergeShipMMChildLink)

// The player's sounds come from Majora's Mask: the four MM forms, and the child while it is MM's human.
#define MERGESHIP_PLAYER_SOUNDS_MM(player) (MERGESHIP_FORM_IS_MM((player)->mergeShipForm) || MERGESHIP_MM_CHILD(player))
