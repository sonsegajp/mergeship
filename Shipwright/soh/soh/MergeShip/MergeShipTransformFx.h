#pragma once

// MergeShip2 -- engine pieces Majora's Mask's transformations use that Ocarina of Time lacks:
// MM's cutscene camera for the transformation, MM's screen distortion, MM's motion blur, and a
// path for MM's sound effects. Each is ported from 2ship2harkinian/mm and names its MM original.

#include "z64.h"

// MM R_PLAY_FILL_SCREEN_* (regs.h). Ocarina of Time reads the very same registers in Play_Draw
// (the MREG(64) block), so MM's white transformation flash needs no drawing code of its own.
#define R_PLAY_FILL_SCREEN_ON MREG(64)
#define R_PLAY_FILL_SCREEN_R MREG(65)
#define R_PLAY_FILL_SCREEN_G MREG(66)
#define R_PLAY_FILL_SCREEN_B MREG(67)
#define R_PLAY_FILL_SCREEN_ALPHA MREG(68)

// MM z64quake.h DISTORTION_TYPE_*. The values are priorities as well as flags: a request only
// replaces a lower one (Distortion_Request).
#define MERGESHIP_DISTORTION_HOT_ROOM (1 << 0)
#define MERGESHIP_DISTORTION_NON_ZORA_SWIMMING (1 << 2)
#define MERGESHIP_DISTORTION_ZORA_SWIMMING (1 << 3)
#define MERGESHIP_DISTORTION_UNDERWATER_ENTRY (1 << 4)
#define MERGESHIP_DISTORTION_SONG_OF_TIME (1 << 5)
#define MERGESHIP_DISTORTION_ZORA_KICK (1 << 6)
#define MERGESHIP_DISTORTION_UNK_ATTACK (1 << 7)
#define MERGESHIP_DISTORTION_GORON_BUTT (1 << 8)
#define MERGESHIP_DISTORTION_MASK_TRANSFORM_1 (1 << 9)
#define MERGESHIP_DISTORTION_BOSS_WARP (1 << 10)
#define MERGESHIP_DISTORTION_MASK_TRANSFORM_2 (1 << 11)

// Camera_ChangeMode values for CAM_SET_MM_MASK_TRANSFORMATION.
#define MERGESHIP_TRANSFORM_CAM_HUMAN 0 // CAM_MODE_NORMAL -> MM Camera_Demo4
#define MERGESHIP_TRANSFORM_CAM_FORM 1  // CAM_MODE_JUMP   -> MM Camera_Demo5

#ifdef __cplusplus
extern "C" {
#endif

// ---- the transformation cutscene (MM CutsceneManager, as used by the player's
// ---- PLAYER_CS_ID_MASK_TRANSFORMATION with the values every MM scene gives it) -----------------

// MM func_808323C0: queue the cutscene; start it once it is next. Called every frame.
void MergeShip_TransformCam_Request(PlayState* play, Actor* actor);
// MM Camera_ChangeMode(GET_ACTIVE_CAM(play), ...).
void MergeShip_TransformCam_SetMode(PlayState* play, s32 mode);
// MM Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)).
s16 MergeShip_TransformCam_GetYaw(PlayState* play);
// MM func_800E0348: skip the camera to its final state.
void MergeShip_TransformCam_Skip(PlayState* play);
// MM Player_StopCutscene -> CutsceneManager_Stop.
void MergeShip_TransformCam_Stop(PlayState* play);
// MM CutsceneManager_Update + CutsceneManager_ClearWaiting, after Play_Draw.
void MergeShip_TransformCam_EndOfFrame(PlayState* play);
// Scene start/end: drop any cutscene state.
void MergeShip_TransformFx_Reset(PlayState* play);

// ---- MM Distortion (z_quake.c) -----------------------------------------------------------------
void MergeShip_Distortion_Request(s32 type);
void MergeShip_Distortion_SetDuration(s16 duration);
void MergeShip_Distortion_RemoveRequest(s32 type);
s16 MergeShip_Distortion_GetType(void);
// After the cameras update, as MM's Play_UpdateMain calls Distortion_Update.
void MergeShip_Distortion_Update(PlayState* play);

// ---- MM motion blur (z_play.c) -----------------------------------------------------------------
void MergeShip_MotionBlur_Enable(u32 alpha);
void MergeShip_MotionBlur_Disable(void);
void MergeShip_MotionBlur_EnablePriority(u32 alpha);
void MergeShip_MotionBlur_DisablePriority(void);
// In Play_Draw after DebugDisplay_DrawObjects, where MM draws it.
void MergeShip_MotionBlur_Draw(PlayState* play);

// ---- MM sound effects --------------------------------------------------------------------------
// MM sfx ids (MM_NA_SE_*) index MM's sound tables, read from mm.o2r.
void MergeShip_PlayMMSfxActor(Actor* actor, u16 mmSfxId);  // MM Actor_PlaySfx / Player_PlaySfx
void MergeShip_PlayMMSfxAtPos(Vec3f* pos, u16 mmSfxId);    // MM SoundSource / Audio_PlaySfx_AtPos
void MergeShip_PlayMMSfxCentered(u16 mmSfxId);             // MM Audio_PlaySfx / Lib_PlaySfx_2
void MergeShip_StopMMSfxById(u16 mmSfxId);                 // MM AudioSfx_StopById

#ifdef __cplusplus
}
#endif

#include "soh/MergeShip/MergeShipMMSfx.h"
// The MM sound engine's player-side entry points (form sound translation, MM ocarina instruments).
#include "soh/MergeShip/MergeShipMMAudio.h"
