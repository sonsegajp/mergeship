#pragma once

// MergeShip2 (soh_fd) -- features carried over from the soh_fd / soh_fd_deku forks: the Fierce Deity options and
// area rules, the forms' restrictions and safeguards, trade masks on forms, the pause model per form, MM jump flips,
// ledge momentum and roll damage, dogs reacting to the forms, and the fork's own assets in mergeship2.o2r.
//
// Every option is a CVar under CVAR_ENHANCEMENT("MergeShip.*"), shown in the Mergeship menu's "Forms & Fierce Deity"
// page (SohMenuMergeShip.cpp). The accessors below carry each option's default, so no caller repeats it.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- options -----------------------------------------------------------------------------------------------------

// "MM Jump Flips": 0 off, 1 Majora's Mask's own ledge jump and flips (no jump attack or ledge grab out of a flip),
// 2 cosmetic (Ocarina of Time's jump, playing MM's flip animations). Young Link, Adult Link and the Fierce Deity only.
#define MERGESHIP_FLIPS_OFF 0
#define MERGESHIP_FLIPS_MM 1
#define MERGESHIP_FLIPS_COSMETIC 2
s32 MergeShip_SohFd_FlipMode(Player* player);

// "MM Ledge Momentum": 0 off, 1 everyone, 2 transformation forms only (default).
#define MERGESHIP_MOMENTUM_OFF 0
#define MERGESHIP_MOMENTUM_ALL 1
#define MERGESHIP_MOMENTUM_FORMS 2
s32 MergeShip_SohFd_LedgeMomentumApplies(Player* player);

// "MM Roll Attack Damage": 0 off, 1 human Link, 2 transformation forms (default), 3 everyone.
s32 MergeShip_SohFd_RollDamageApplies(Player* player);

// "Bunny Hood Fit": 0 off, 1 Adult Link, 2 Fierce Deity, 3 both (default). Deku, Goron and Zora heads still need
// their own placement; see MergeShip_SohFd_BunnyHoodRaise.
s32 MergeShip_SohFd_BunnyHoodFitApplies(Player* player);

// "Dogs react to <form>": 0 none, 1 all dogs, 2 all but Richard, 3 only Richard.
#define MERGESHIP_DOGS_NONE 0
#define MERGESHIP_DOGS_ALL 1
#define MERGESHIP_DOGS_ALL_BUT_RICHARD 2
#define MERGESHIP_DOGS_ONLY_RICHARD 3
s32 MergeShip_SohFd_DogReactionMode(u8 playerForm);

s32 MergeShip_SohFd_FormsWearTradeMasks(void);
s32 MergeShip_SohFd_StuckSafeguards(void);
s32 MergeShip_SohFd_PreventRestrictedActions(void);
s32 MergeShip_SohFd_SceneStartSafeguards(void);
s32 MergeShip_SohFd_InstrumentIcons(void);
s32 MergeShip_SohFd_AdultScream(void);
s32 MergeShip_SohFd_GetMaskFanfare(void);

s32 MergeShip_SohFd_FdCanPlayOcarina(void);
// "FD Can Use Transformation Masks": the Fierce Deity puts the Deku, Goron and Zora masks on directly (default on).
s32 MergeShip_SohFd_FdCanUseMasks(void);
s32 MergeShip_SohFd_FdCanSheathe(void);
s32 MergeShip_SohFd_FdBeamsBreakRocks(void);
s32 MergeShip_SohFd_FdBeamsLightFire(void);
s32 MergeShip_SohFd_FdMagicSpin(void);
s32 MergeShip_SohFd_FdIncreasedStrength(void);
// "Unrestrict Items for FD": 0 off, 1 Deku Nuts + bombs + spells (+ Lens), 2 everything except swords and shields.
s32 MergeShip_SohFd_FdUnrestrictItems(void);
// "Fierce Deity Anywhere" (the existing gGeneral.MergeShipFierceDeityAnywhere, or the D-pad quick-play masks).
s32 MergeShip_SohFd_FdAnywhere(void);

// ---- forms ---------------------------------------------------------------------------------------------------------

// The player (gPlayState's) is in one of Majora's Mask's four forms.
s32 MergeShip_SohFd_PlayerIsTransformed(void);
// Where the Fierce Deity may be: Hyrule's boss lairs and the fishing hole (as Majora's Mask 3D lets him fish), or
// anywhere with the option. `sceneNum` is a SceneID.
s32 MergeShip_SohFd_FierceDeityAllowedInScene(s32 sceneNum);

// ---- the Fierce Deity's areas and the forms' safeguards (soh_fd) --------------------------------------------------
// Play_Init, before the scene layer is chosen: a Fierce Deity loading a scene that is not his (the fishing hole's
// door, a boss lair left by anything but its blue warp) arrives as Link, silently. `destScene` is a SceneID.
void MergeShip_SohFd_OnSceneLoad(s32 destScene);
// Door_Warp1's idle functions, the player in range: a Fierce Deity who may not leave the lair as one turns back into
// Link first (the white flash). Returns true while the warp has to wait for him.
s32 MergeShip_SohFd_BlueWarpRevert(PlayState* play);
// En_Elf: Navi's deep-water offer (MERGESHIP_TEXT_FORM_WATER_WARNING) answered "Yes" -- turn back into Link once
// the text box has closed.
void MergeShip_SohFd_RequestRevertToHuman(PlayState* play);

// z_en_m_thunder.c: `actor` is the Fierce Deity's flying sword beam. The bosses whose Majora's Mask counterparts react
// to MM's DMG_SWORD_BEAM test for it with this (the beam cannot carry that flag in Ocarina of Time's damage tables).
s32 EnMThunder_IsFdSwordBeam(Actor* actor);

// Extra items a transformed form may use on top of Majora's Mask's table (MergeShip_FormCanUseItem):
// returns 1 to allow, 0 to deny, -1 to leave it to Majora's Mask's table.
s32 MergeShip_SohFd_FormItemOverride(u8 form, u8 item);

// ---- Hyrule's masks over a form ("Forms Wear Trade Masks") and the Bunny Hood Fit ---------------------------------
// The Hyrule mask (PLAYER_MASK_KEATON..PLAYER_MASK_TRUTH) `player` wears: a human's currentMask, or the one the player
// wears over a form. PLAYER_MASK_NONE otherwise (a form's own mask is not one of Hyrule's).
u8 MergeShip_SohFd_WornHyruleMask(PlayState* play, Player* player);
// The fishing pond's HUD without the rod (z_parameter.c): Ocarina of Time disables every button there, but the
// transformation masks the player may use stay enabled, as soh_fd kept the Fierce Deity's Mask.
void MergeShip_SohFd_PondMaskButtons(PlayState* play);
// The pause menu's item page: an item the current form may not use is drawn grayed, as MM's pause does.
s32 MergeShip_SohFd_PauseItemRefused(u8 item);
// The Hyrule mask worn over the player's form (PLAYER_MASK_NONE when none, or with the option off).
u8 MergeShip_SohFd_FormTradeMask(void);
void MergeShip_SohFd_SetFormTradeMask(u8 mask);
// z_player_lib.c, the head limb's post-draw: the head's matrices a Hyrule mask is drawn from, when it cannot use the
// skeleton's own list (a form's skeleton, or a Bunny Hood lifted by "Bunny Hood Fit").
void MergeShip_SohFd_DrawOnHead(PlayState* play, Player* player, Gfx** dList);
// Player_DrawGameplay, after the skeleton: those matrices, for segment 0x0D (NULL: keep the skeleton's). Clears them.
Mtx* MergeShip_SohFd_TakeMaskMtxList(void);
// z_player_lib.c, the right hand's post-draw: the Fierce Deity's ocarina ("FD Can Play Ocarina", mergeship2.o2r).
void MergeShip_SohFd_DrawFdOcarina(PlayState* play, Player* player);

// ---- "Adult Link Transformation Scream" ---------------------------------------------------------------------------
// The transformation's scream (MM NA_SE_PL_TRANSFORM_VOICE), for `player`: Adult Link screams with his own voice
// (mergeship2.o2r) instead of Majora's Mask's young Link. Returns false when MM's scream should play.
s32 MergeShip_SohFd_PlayAdultScream(Player* player);

// ---- "Get-a-Mask Fanfare" ------------------------------------------------------------------------------------------
// func_8084DFF4, an item got: plays Majora's Mask's "Get a Mask" fanfare (mm.o2r's sequence and instruments) for a
// transformation mask -- `isTransformationMask`, which the caller knows from the item and the randomizer's items, or
// Hyrule's Goron / Zora Mask (`ootItemId`) while they are the transformation masks. Returns true once it plays.
s32 MergeShip_SohFd_PlayGetMaskFanfare(s32 isTransformationMask, s16 ootItemId);

// ---- the equipment screen's Link ("Forms on the Equipment Screen") -------------------------------------------------
// The form the pause menu's Link is drawn as: the player's, or Link's age with the option off.
u8 MergeShip_SohFd_PauseForm(void);
// The pause Link is placed as the child (the child and the Deku) rather than as the adult.
s32 MergeShip_SohFd_PauseIsChildSized(void);
// Player_DrawPauseImpl: how far below Ocarina of Time's placement the form stands (soh_fd's heights for the Deku and
// the Fierce Deity), and whether it takes the adult's animated-pose shift to the left.
f32 MergeShip_SohFd_PauseDropY(u8 form, s32 animated);
s32 MergeShip_SohFd_PauseShiftsLeft(u8 form, s32 animated);

// ---- the fishing hole ------------------------------------------------------------------------------------------
// Fishing_DrawRod: where a form's fist holds the rod, in its left hand's space (Ocarina of Time: 400 up the adult's
// hand, 230 up the child's). Returns false for Link himself.
s32 MergeShip_SohFd_RodGrip(u8 form, Vec3f* grip);

// ---- the ocarina item icon of a form's instrument (mergeship2.o2r) -----------------------------------------------
// The Deku pipes / Goron drums / Zora guitar icon for `item` while the player is that form and the option is on,
// else `baseTex`. For the HUD's C buttons and the pause menu's item grid.
void* MergeShip_SohFd_InstrumentIconSwap(void* baseTex, u8 item);

// ---- mergeship2.o2r (resource paths in MergeShipSohFdAssets.h) ---------------------------------------------------
// True once mergeship2.o2r is mounted (OTRGlobals::Initialize).
s32 MergeShip_SohFd_AssetsMounted(void);

// ---- Adult Link's transformation voice (MergeShipSohFdAudio.cpp) ---------------------------------------------------
// Puts Adult Link's recording (mergeship2.o2r) into the three drums of MM's sound font 0 that Majora's Mask's
// transformation voice plays, so the next MM NA_SE_PL_TRANSFORM_VOICE is his. Returns false if it could not.
s32 MergeShip_SohFd_SwapInAdultVoice(void);
// Once a frame: puts MM's own drums back once the voice has started all three of its notes.
void MergeShip_SohFd_UpdateAdultVoice(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <memory>
namespace Ship {
class ArchiveManager;
}
// OTRGlobals::Initialize, after mm.o2r: mount mergeship2.o2r if it is beside the executable.
void MergeShip_SohFd_MountAssets(const std::shared_ptr<Ship::ArchiveManager>& archiveManager);
#endif
