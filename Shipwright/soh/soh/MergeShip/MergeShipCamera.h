#pragma once

// MergeShip2 -- Majora's Mask camera modes its forms use that Ocarina of Time's camera lacks.
//
// OoT's camera settings keep a 21-bit valid-mode mask, and every bit is taken, so MM's extra modes
// cannot be added to OoT's CameraModeType. Instead, while the player requests one of these, the
// main camera runs MM's camera function for that mode with MM's parameters (z_camera.c ->
// MergeShipCameraMM.inc -> MergeShipCameraMMModes.inc).

#include "z64.h"

// MM CameraModeType values (MM z64camera.h), for the modes Ocarina of Time does not have.
typedef enum MMCameraMode {
    MM_CAM_MODE_NONE = -1,
    MM_CAM_MODE_GORONDASH = 0x02,
    MM_CAM_MODE_DEKUSHOOT = 0x03,
    MM_CAM_MODE_DEKUFLY = 0x05,
    MM_CAM_MODE_ZORAFIN = 0x08,
    MM_CAM_MODE_DEKUHIDE = 0x0F,
    MM_CAM_MODE_DEKUFLYZ = 0x19,
    MM_CAM_MODE_GORONJUMP = 0x1A,
    MM_CAM_MODE_CHARGEZ = 0x1C,
    MM_CAM_MODE_ZORAFINZ = 0x1D,
} MMCameraMode;

// MM PlayerMeleeWeaponAnimation values the camera tests (MM z64player.h, MM numbering).
#define MM_PLAYER_MWA_GORON_PUNCH_BUTT 26
#define MM_PLAYER_MWA_ZORA_PUNCH_KICK 29

#ifdef __cplusplus
extern "C" {
#endif

// Call this where MM's player calls Camera_ChangeMode(camera, CAM_MODE_<one of the above>), *instead of*
// OoT's Camera_RequestMode for that frame. It runs MM's Camera_ChangeModeFlags on the main camera (mode
// validity per setting, the disable-mode-change flag, MM's reset of the camera function) and must be
// renewed every frame the mode is wanted, as MM's player does. The first frame without a request (or any
// OoT Camera_RequestMode call) returns the camera to Ocarina of Time's own mode.
void MergeShip_Camera_RequestMMMode(PlayState* play, s32 mmCamMode);

// MM's camera reads player->meleeWeaponAnimation in MM's numbering (the Goron butt punch and the Zora
// kick start screen distortions, MM Camera_SetSwordDistortion). OoT's PlayerMeleeWeaponAnimation has
// no such values, so form code that sets an MM melee animation reports it here, in MM numbering, each
// time it sets player->meleeWeaponAnimation (MM_PLAYER_MWA_*; any other value for other attacks).
void MergeShip_Camera_SetMMMeleeWeaponAnimation(s32 mmMeleeWeaponAnimation);

// Implemented in MergeShipCameraMMModes.inc (z_camera.c).
s32 MergeShip_CameraMM_ChangeMode(Camera* camera, s16 mmMode, u8 forceChange);

#ifdef __cplusplus
}
#endif
