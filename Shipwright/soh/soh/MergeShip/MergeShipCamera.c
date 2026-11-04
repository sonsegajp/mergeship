#include "global.h"
#include "soh/MergeShip/MergeShipCamera.h"

// MergeShip2 -- MM's form camera modes. The camera side (MM's Camera_ChangeModeFlags and the ported
// update functions) lives in z_camera.c's MergeShipCameraMMModes.inc; this is the player-facing entry.

// MM Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), mmCamMode)
void MergeShip_Camera_RequestMMMode(PlayState* play, s32 mmCamMode) {
    Camera* camera;

    if ((play == NULL) || (mmCamMode == MM_CAM_MODE_NONE)) {
        return;
    }
    camera = play->cameraPtrs[CAM_ID_MAIN];
    if (camera == NULL) {
        return;
    }
    MergeShip_CameraMM_ChangeMode(camera, (s16)mmCamMode, false);
}
