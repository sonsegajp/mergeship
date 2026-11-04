#pragma once

// MergeShip2 -- Majora's Mask's Pictograph Box in Hyrule.
//
//   MergeShipPicto.c    the item's MM code: Player_UseItem's func_80831814 (PLAYER_UNKAA5_2), the
//                       ACTORCTX_FLAG_PICTO_BOX_ON flag, Interface_UpdateButtonsPart1's picto state machine
//                       (sPictoState), Interface_Draw's viewfinder and photo, Play_TakePictoPhoto /
//                       Play_ConvertRgba16ToIntensityImage / Play_CompressI8ToI5 / Play_DecompressI5ToI8, and
//                       Snap_RecordPictographedActors.
//   MergeShipPicto.cpp  the SoH side: the "mergeShipPicto" save section (the photo and MM's QUEST_PICTOGRAPH),
//                       the GameInteractor hooks, MM text 0xF8 ("Keep this picture?").
//
// How it runs (MM's order, in Ocarina of Time's frame):
//   * Player_UseItem (z_player.c) -> MergeShip_Picto_UseItem: MM func_80831814 sets unk_6AD to
//     MERGESHIP_UNK6AD_PICTO_BOX (MM PLAYER_UNKAA5_2), or plays MM's error sound.
//   * Player_ActionHandler_13's first-person branch enters Player_Action_8084B1D8 (MM Player_Action_43) and calls
//     MergeShip_Picto_OnEnterFirstPerson, which raises the picto flag (MM ACTORCTX_FLAG_PICTO_BOX_ON).
//     Player_SetupAction lowers it (MM Player_SetAction). While the flag is up the first-person view is kept
//     (MergeShip_Picto_KeepsView); once it is lowered the view ends with MM's zoom sound.
//   * GameInteractor OnInterfaceUpdate -> MergeShip_Picto_InterfaceUpdate: MM's picto branches of
//     Interface_UpdateButtonsPart1 (A takes the photo, B leaves, the Keep? choice, a stored photo shown first).
//   * GameInteractor OnPlayDrawEnd (Play_Draw after the world, before the HUD) -> the frame is read back to the
//     CPU with SoH's FB_WriteFramebufferSliceToCPU, as 2S2H's Play_Draw does for PICTO_PHOTO_STATE_SETUP.
//   * Interface_Draw (z_parameter.c) -> MergeShip_Picto_Draw: the viewfinder, and the photo with MM text 0xF8.
//
// Options: CVAR_ENHANCEMENT("MergeShip.MMItems") (the MM items, default on) and
// CVAR_ENHANCEMENT("MergeShip.MMItems.Pictograph") (this item, default on). With either off the box cannot be used;
// the stored photo stays in the save.

#include <libultraship/libultra.h>

struct PlayState;
struct Player;

// MM PLAYER_UNKAA5_2 in Ocarina of Time's unk_6AD numbering (0 none, 1 C-up view, 2 aiming, 3 cutscene action,
// 4 cutscene item). It takes the first free value; func_8083C148 resets it as MM func_80839ED0 resets unk_AA5 < 5.
#define MERGESHIP_UNK6AD_PICTO_BOX 5

// MM text 0xF8 "Keep this picture?" (Yes / No), re-encoded for Ocarina of Time (MergeShipPicto.cpp). In the MM items'
// text range 0x93A0..0x93BF (MergeShipMMItems.h: item texts 0x93A0..0x93B2, 0x93BE, 0x93BF; free: 0x93B3..0x93BB).
#define MERGESHIP_TEXT_MM_PICTO_KEEP 0x93BC

// MM z64save.h PICTO_PHOTO_*.
#define MERGESHIP_PICTO_PHOTO_WIDTH 160
#define MERGESHIP_PICTO_PHOTO_HEIGHT 112
#define MERGESHIP_PICTO_PHOTO_TOPLEFT_X ((320 - MERGESHIP_PICTO_PHOTO_WIDTH) / 2)
#define MERGESHIP_PICTO_PHOTO_TOPLEFT_Y ((240 - MERGESHIP_PICTO_PHOTO_HEIGHT) / 2)
#define MERGESHIP_PICTO_PHOTO_SIZE (MERGESHIP_PICTO_PHOTO_WIDTH * MERGESHIP_PICTO_PHOTO_HEIGHT)
#define MERGESHIP_PICTO_PHOTO_COMPRESSED_SIZE (MERGESHIP_PICTO_PHOTO_SIZE * 5 / 8)

#ifdef __cplusplus
extern "C" {
#endif

// ---- save data (MergeShipPicto.cpp's "mergeShipPicto" section) ------------------------------------------------

// MM gSaveContext.pictoPhotoI5. One byte more than MM's buffer: MM Play_DecompressI5ToI8 reads one byte past the end.
extern u8 gMergeShipPictoPhotoI5[MERGESHIP_PICTO_PHOTO_COMPRESSED_SIZE + 1];
// MM CHECK_QUEST_ITEM(QUEST_PICTOGRAPH): a photo is kept.
extern u8 gMergeShipPictoHasPhoto;
// MM saveInfo.pictoFlags0 / pictoFlags1 (Snap_RecordPictographedActors).
extern u32 gMergeShipPictoFlags[2];

// ---- options ----------------------------------------------------------------------------------------------------

// CVAR_ENHANCEMENT("MergeShip.MMItems") (MergeShip_MMItems_Enabled) and CVAR_ENHANCEMENT("MergeShip.MMItems.Pictograph").
s32 MergeShip_Picto_Enabled(void);
// MM CHECK_QUEST_ITEM(QUEST_PICTOGRAPH), for the C-button count MM draws on the box (0 grey, 1 green).
s32 MergeShip_Picto_HasPhoto(void);

// ---- the player (z_player.c) ------------------------------------------------------------------------------------

// MM Player_UseItem's PLAYER_IA_PICTOGRAPH_BOX branches. `isSwimming` is func_808332B8 (MM func_801242B4).
void MergeShip_Picto_UseItem(struct PlayState* play, struct Player* player, s32 isSwimming);
// MM Player_ActionHandler_13: `if (this->unk_AA5 == PLAYER_UNKAA5_2) play->actorCtx.flags |= ACTORCTX_FLAG_PICTO_BOX_ON`.
void MergeShip_Picto_OnEnterFirstPerson(struct Player* player);
// MM Player_SetAction: `play->actorCtx.flags &= ~ACTORCTX_FLAG_PICTO_BOX_ON`.
void MergeShip_Picto_OnSetupAction(void);
// MM Player_Action_43: the picto view (PLAYER_UNKAA5_2) is kept while the picto flag is up.
s32 MergeShip_Picto_KeepsView(struct Player* player);
// MM Player_UpdateInterface: DO_ACTION_SNAP on A while the picto flag is up; `doAction` otherwise.
s32 MergeShip_Picto_DoActionA(s32 doAction, s32 snapDoAction);
// MM ACTORCTX_FLAG_PICTO_BOX_ON.
s32 MergeShip_Picto_IsBoxOn(void);

// ---- the interface ------------------------------------------------------------------------------------------------

// MM Interface_UpdateButtonsPart1's pictograph branches (GameInteractor OnInterfaceUpdate).
void MergeShip_Picto_InterfaceUpdate(struct PlayState* play);
// Message_Update, MSGMODE_TEXT_DONE with a two-choice box: "Keep this picture?" answered (MM Interface_UpdateButtonsPart1
// answers it before its Message_Update; Ocarina of Time's Message_Update would close the box on A first). True: ours.
s32 MergeShip_Picto_OnChoice(struct PlayState* play);
// MM Play_Draw's PICTO_PHOTO_STATE_SETUP capture (GameInteractor OnPlayDrawEnd).
void MergeShip_Picto_OnPlayDrawEnd(struct PlayState* play);
// MM Interface_Draw's pictograph viewfinder and photo (z_parameter.c Interface_Draw).
void MergeShip_Picto_Draw(struct PlayState* play);
// MM Play_Init: R_PICTO_PHOTO_STATE = PICTO_PHOTO_STATE_OFF and the actor context flags cleared (a new scene).
void MergeShip_Picto_OnSceneInit(void);
// A file was loaded or left: nothing is in progress.
void MergeShip_Picto_Reset(void);

// ---- MM z_play.c / z_snap.c --------------------------------------------------------------------------------------

void MergeShip_Picto_CompressI8ToI5(void* srcI8, void* destI5, size_t size);
void MergeShip_Picto_DecompressI5ToI8(void* srcI5, void* destI8, size_t size);
void MergeShip_Picto_ConvertRgba16ToIntensityImage(void* destI, u16* srcRgba16, s32 rgba16Width, s32 pixelLeft,
                                                   s32 pixelTop, s32 pixelRight, s32 pixelBottom, s32 bitDepth);
s32 MergeShip_Picto_RecordPictographedActors(struct PlayState* play);

#ifdef __cplusplus
}
#endif
