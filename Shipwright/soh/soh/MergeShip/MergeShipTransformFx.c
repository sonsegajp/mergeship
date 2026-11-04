#include "global.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
#include "soh/framebuffer_effects.h"
#include "soh/OTRGlobals.h"

#include <string.h>

// MergeShip2 -- engine pieces from Majora's Mask that its transformations need. See the header.

// =============================================================================================
// The transformation cutscene: MM's CutsceneManager, reduced to the one cutscene the player
// starts for a mask transformation.
//
// MM gives that cutscene its settings per scene (Play_AssignPlayerCsIdsFromScene). Every MM scene
// uses the same ones -- read from all 101 scene files in mm.o2r: priority 400, length -1 (until
// stopped), HUD CS_HUD_VISIBILITY_NONE, letterbox 32, and CS_END_CAM_1 (the camera returns to exactly
// where it was) in 89 of them, CS_END_CAM_0 in the other 12. Those values are used here.
//
// Timing is MM's: the player queues the cutscene on the first frame, it becomes "next" after that
// frame is drawn (CutsceneManager_Update runs after Play_Draw), and it starts on the second frame.
// Stopping it takes effect after the frame that asked for it is drawn.
// =============================================================================================

#define MM_TRANSFORM_CS_LETTERBOX 32

static struct {
    u8 queued;     // MM sWaitingCutsceneList
    u8 next;       // MM sNextCutsceneList (after CutsceneManager_MarkNextCutscenes)
    u8 active;     // MM sCutsceneMgr.csId == this cutscene
    u8 ending;     // MM sCutsceneMgr.endCsId
    u8 hadCs;      // CutsceneManager_Update's sp1E
    s16 retCamId;
    s16 subCamId;
    u8 cameraStored;
    Camera storedCamera; // MM CutsceneManager_StoreCamera (subCameras[2])
} sTransformCs;

static void MergeShip_TransformCam_Start(PlayState* play, Actor* actor) {
    Camera* subCam;
    Camera* retCam;

    // MM CutsceneManager_Start, csType 2 (neither door nor talk, no script).
    Letterbox_SetSizeTarget(MM_TRANSFORM_CS_LETTERBOX);
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING_ALT); // MM HUD_VISIBILITY_NONE_ALT

    sTransformCs.retCamId = Play_GetActiveCamId(play);
    // MergeShip2 (soh_fd): a room drawn as a prerendered image (mesh header type 1: the Market, the Temple of Time's
    // front, the town's houses) keeps its fixed camera, as with no subcamera below. A 3D camera swung onto Link there
    // shows the room's real geometry behind the image.
    if ((play->roomCtx.curRoom.meshHeader != NULL) && (play->roomCtx.curRoom.meshHeader->base.type == 1)) {
        sTransformCs.subCamId = SUBCAM_NONE;
    } else {
        sTransformCs.subCamId = Play_CreateSubCamera(play);
    }
    if (sTransformCs.subCamId == SUBCAM_NONE) {
        sTransformCs.active = true;
        return;
    }

    subCam = Play_GetCamera(play, sTransformCs.subCamId);
    retCam = Play_GetCamera(play, sTransformCs.retCamId);

    memcpy(subCam, retCam, sizeof(Camera));
    subCam->thisIdx = sTransformCs.subCamId;

    Play_ChangeCameraStatus(play, sTransformCs.retCamId, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, sTransformCs.subCamId, CAM_STAT_ACTIVE);

    // MM: Camera_UnsetStateFlag(subCam, CAM_STATE_6 | CAM_STATE_0) and subCam->behaviorFlags = 0.
    // Ocarina of Time splits MM's two words differently: MM's stateFlags are OoT's unk_14C and MM's
    // behaviorFlags are OoT's unk_14A. Left as copied from the main camera, unk_14C bit 0 makes
    // Camera_Update re-apply the floor's scene camera every frame (overriding the transformation camera)
    // and unk_14A bit 0 makes Camera_ChangeSettingFlags refuse the new setting on priority.
    subCam->unk_14C &= ~(0x40 | 0x1);
    subCam->unk_14A = 0;
    subCam->target = actor;
    Camera_RequestSetting(subCam, CAM_SET_MM_MASK_TRANSFORMATION);

    sTransformCs.active = true;
}

void MergeShip_TransformCam_Request(PlayState* play, Actor* actor) {
    if (sTransformCs.active) {
        return;
    }
    if (!sTransformCs.next) {
        sTransformCs.queued = true;
        return;
    }
    MergeShip_TransformCam_Start(play, actor);
}

void MergeShip_TransformCam_SetMode(PlayState* play, s32 mode) {
    Camera* cam = Play_GetCamera(play, Play_GetActiveCamId(play));

    // Before the cutscene starts MM applies this to the main camera; CS_END_CAM_1 then restores that
    // camera wholesale, so only the cutscene camera's mode has any lasting effect.
    if (!sTransformCs.active || (sTransformCs.subCamId == SUBCAM_NONE) || (cam == NULL)) {
        return;
    }
    Camera_RequestMode(cam, (mode == MERGESHIP_TRANSFORM_CAM_FORM) ? CAM_MODE_JUMP : CAM_MODE_NORMAL);
}

s16 MergeShip_TransformCam_GetYaw(PlayState* play) {
    return Camera_GetCamDirYaw(Play_GetCamera(play, Play_GetActiveCamId(play)));
}

void MergeShip_TransformCam_Skip(PlayState* play) {
    // MM func_800E0348: camera->animState = 999.
    if (sTransformCs.active && (sTransformCs.subCamId != SUBCAM_NONE)) {
        Play_GetCamera(play, sTransformCs.subCamId)->animState = 999;
    }
}

void MergeShip_TransformCam_Stop(PlayState* play) {
    if (sTransformCs.active) {
        sTransformCs.ending = true;
    }
    sTransformCs.queued = false;
}

// MM CutsceneManager_End with CS_END_CAM_1.
static void MergeShip_TransformCam_End(PlayState* play) {
    if (sTransformCs.subCamId != SUBCAM_NONE) {
        Camera* retCam = Play_GetCamera(play, sTransformCs.retCamId);
        Camera* subCam = Play_GetCamera(play, sTransformCs.subCamId);

        // MergeShip2 (soh_fd): Camera_UpdateWater turns the underwater lights, sound filter and distortion on and off
        // as a camera's eye crosses the surface, and keeps that state in the camera itself (unk_14C & 0x100). The
        // cutscene camera is thrown away and the stored camera restored over the returning one, so whatever water
        // state the cutscene camera left is undone here (a transformation at the surface -- Navi's deep-water offer
        // -- could leave the world in underwater lighting), and the returning camera checks the water afresh.
        if ((subCam != NULL) && (subCam->unk_14C & 0x100)) {
            subCam->unk_14C &= ~0x100;
            Environment_DisableUnderwaterLights(play);
            if (subCam->waterQuakeId != -1) {
                Quake_RemoveFromIdx(subCam->waterQuakeId);
                subCam->waterQuakeId = -1;
            }
            subCam->distortionFlags = 0;
            Audio_SetExtraFilter(0);
        }

        if (sTransformCs.cameraStored && (retCam != NULL)) {
            s16 oldIdx = retCam->thisIdx;

            memcpy(retCam, &sTransformCs.storedCamera, sizeof(Camera));
            retCam->thisIdx = oldIdx;
            sTransformCs.cameraStored = false;
        }
        if ((retCam != NULL) && (retCam->unk_14C & 0x100)) {
            retCam->unk_14C &= ~0x100; // Camera_UpdateWater turns the underwater state back on if the eye is under
        }
        Play_ClearCamera(play, sTransformCs.subCamId);
        Play_ChangeCameraStatus(play, sTransformCs.retCamId, CAM_STAT_ACTIVE);
    }

    sTransformCs.active = false;
    sTransformCs.ending = false;
    sTransformCs.subCamId = SUBCAM_NONE;

    // BUG-0038: the HUD comes back with the cutscene's end (MM CutsceneManager_End -> Interface_SetHudVisibility
    // HUD_VISIBILITY_ALL); left to the next thing that happens to restore it, taking off a mask could leave the HUD
    // faded out.
    if (gSaveContext.hudVisibilityMode == HUD_VISIBILITY_NOTHING_ALT) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
    }
}

void MergeShip_TransformCam_EndOfFrame(PlayState* play) {
    s32 hadCs = 0;

    if (play == NULL) {
        return;
    }

    // MM CutsceneManager_Update.
    if (sTransformCs.active && !sTransformCs.ending) {
        hadCs = 1;
    }
    if (sTransformCs.ending) {
        MergeShip_TransformCam_End(play);
        hadCs = 2;
    }

    // CutsceneManager_ClearNextCutscenes + MarkNextCutscenes: a queued cutscene becomes next.
    sTransformCs.next = false;
    if (!sTransformCs.active) {
        if (sTransformCs.queued) {
            sTransformCs.next = true;
        } else if (hadCs != 0) {
            Letterbox_SetSizeTarget(0);
        } else {
            // While no cutscene runs MM stores the camera every frame, for CS_END_CAM_1 to restore.
            Camera* cam = Play_GetCamera(play, Play_GetActiveCamId(play));
            if (cam != NULL) {
                memcpy(&sTransformCs.storedCamera, cam, sizeof(Camera));
                sTransformCs.cameraStored = true;
            }
        }
    }

    // CutsceneManager_ClearWaiting.
    sTransformCs.queued = false;
}

// =============================================================================================
// MM Distortion (src/code/z_quake.c), whole.
// =============================================================================================

typedef enum {
    MM_DISTORTION_INACTIVE,
    MM_DISTORTION_ACTIVE,
    MM_DISTORTION_SETUP
} MMDistortionState;

static struct {
    s16 type;
    s16 state;
    s16 timer;
} sDistortionRequest;

#define MM_CAM_DEG_TO_BINANG(degrees) (s16)((degrees) * ((f32)0xFFFF / 360) + .5f)
#define MM_DEG_TO_RAD(degrees) ((degrees) * (3.14159265358979323846f / 180.0f))

void MergeShip_Distortion_SetDuration(s16 duration) {
    sDistortionRequest.timer = duration;
    sDistortionRequest.state = MM_DISTORTION_SETUP;
}

s16 MergeShip_Distortion_GetType(void) {
    return sDistortionRequest.type;
}

void MergeShip_Distortion_Request(s32 type) {
    if (sDistortionRequest.type < type) {
        sDistortionRequest.type = type;
    }
}

void MergeShip_Distortion_RemoveRequest(s32 type) {
    if (sDistortionRequest.type == type) {
        sDistortionRequest.type = 0;
    }
}

// MM Distortion_GetUnderwaterCurrentSpeed. OoT's conveyor speed has the same encoding.
static s32 MergeShip_Distortion_GetUnderwaterCurrentSpeed(PlayState* play, Player* player) {
    if (!SurfaceType_IsConveyor(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId)) {
        return SurfaceType_GetConveyorSpeed(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId);
    }
    return 0; // CONVEYOR_SPEED_DISABLED
}

void MergeShip_Distortion_Update(PlayState* play) {
    static s16 depthPhase = 0x3F0;
    static s16 screenPlanePhase = 0x156;
    static s16 duration = 1;
    f32 xyScaleFactor;
    f32 zScaleFactor;
    f32 speedScaleFactor;
    f32 countdownRatio;
    f32 waterYScaleFactor;
    f32 depthPhaseStep;
    f32 screenPlanePhaseStep;
    f32 rotX;
    f32 rotY;
    f32 rotZ;
    f32 xScale;
    f32 yScale;
    f32 zScale;
    f32 speed;
    Player* player;
    Camera* camera = Play_GetCamera(play, Play_GetActiveCamId(play));
    f32 speedRatio = CLAMP_MAX(camera->speedRatio, 1.0f);

    if (sDistortionRequest.type != 0) {
        if (sDistortionRequest.type & MERGESHIP_DISTORTION_MASK_TRANSFORM_2) {
            sDistortionRequest.timer = 2;
            depthPhase = 0x3F0;
            screenPlanePhase = 0x156;
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 170.0f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.0f;
            xScale = -0.01f;
            yScale = 0.01f;
            zScale = 0.0f;
            speed = 0.6f;
            xyScaleFactor = zScaleFactor = sDistortionRequest.timer / 60.0f;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_BOSS_WARP) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x3F0;
                screenPlanePhase = 0x156;
            }
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 50.0f / duration;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.0f;
            xScale = 0.01f;
            yScale = 100.0f;
            zScale = 0.0f;
            speed = 0.4f;
            xyScaleFactor = zScaleFactor = ((f32)duration - sDistortionRequest.timer) / (f32)duration;
            speedScaleFactor = 0.5f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_MASK_TRANSFORM_1) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x1FC;
                screenPlanePhase = 0x156;
            }
            depthPhaseStep = -5.0f;
            screenPlanePhaseStep = 5.0f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 2.0f;
            xScale = 0.3f;
            yScale = 0.3f;
            zScale = 0.0f;
            speed = 0.1f;
            xyScaleFactor = zScaleFactor = ((f32)duration - sDistortionRequest.timer) / (f32)duration;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_GORON_BUTT) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x2710;
                screenPlanePhase = 0x3E8;
            }
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 150.0f;
            rotX = 0;
            rotY = 0;
            rotZ = 0.2f;
            xScale = 0.025f;
            yScale = 0.02f;
            zScale = 0.01f;
            speed = 1.5f;
            if (sDistortionRequest.timer < 5) {
                xyScaleFactor = zScaleFactor = ((f32)duration - sDistortionRequest.timer) / (f32)duration;
            } else {
                xyScaleFactor = zScaleFactor = 0.0f;
            }
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_UNK_ATTACK) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x4B0;
                screenPlanePhase = 0x7D0;
            }
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 150.0f;
            rotX = 0;
            rotY = 0;
            rotZ = 0;
            xScale = 0.03f;
            yScale = 0.02f;
            zScale = 0.01f;
            speed = 1.5f;
            xyScaleFactor = zScaleFactor = sDistortionRequest.timer / (f32)duration;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_ZORA_KICK) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x9C4;
                screenPlanePhase = 0xBB8;
            }
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 150.0f;
            rotX = 0;
            rotY = 0;
            rotZ = 0;
            xScale = 0.03f;
            yScale = 0.03f;
            zScale = 0.01f;
            speed = 1.3f;
            if (sDistortionRequest.timer < 4) {
                xyScaleFactor = zScaleFactor = sDistortionRequest.timer / (f32)duration;
            } else {
                xyScaleFactor = zScaleFactor = 0.0f;
            }
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_SONG_OF_TIME) {
            sDistortionRequest.timer = 2;
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                depthPhase = 0x9C4;
                screenPlanePhase = 0xBB8;
            }
            depthPhase += 0xB1;
            screenPlanePhase -= 0x2B;
            depthPhaseStep = -107.0f;
            screenPlanePhaseStep = 158.0f;
            rotX = 0.2f;
            rotY = 1.7f;
            rotZ = -2.9f;
            xScale = -0.6f;
            yScale = -0.7f;
            zScale = 0.6f;
            speed = 0.2f;
            zScaleFactor = 1.0f;
            xyScaleFactor = 1.0f;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_UNDERWATER_ENTRY) {
            if (sDistortionRequest.state == MM_DISTORTION_SETUP) {
                duration = sDistortionRequest.timer;
                depthPhase = 0x760;
                screenPlanePhase = 0x1BC;
            }
            depthPhaseStep = 248.0f;
            screenPlanePhaseStep = -90.0f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.0f;
            xScale = -0.4f;
            yScale = 0.4f;
            zScale = 0.2f;
            speed = 0.25f;
            countdownRatio = sDistortionRequest.timer / (f32)duration;
            zScaleFactor = xyScaleFactor = countdownRatio;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_ZORA_SWIMMING) {
            depthPhase = 0x3F0;
            screenPlanePhase = 0x156;
            sDistortionRequest.timer = 2;
            player = GET_PLAYER(play);
            depthPhaseStep = 359.2f;
            screenPlanePhaseStep = -18.5f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.3f;
            switch (MergeShip_Distortion_GetUnderwaterCurrentSpeed(play, player)) {
                case 3: // CONVEYOR_SPEED_FAST
                case 2: // CONVEYOR_SPEED_MEDIUM
                case 1: // CONVEYOR_SPEED_SLOW
                    xScale = -0.06f;
                    yScale = 0.1f;
                    zScale = 0.03f;
                    speed = 0.33f;
                    break;
                default:
                    xScale = -0.06f;
                    yScale = 0.1f;
                    zScale = 0.03f;
                    speed = 0.23f;
                    break;
            }
            if (player->unk_B86[1] < 0) {
                xyScaleFactor = (player->unk_B86[1] - (f32)0x4000) / (f32)0xC000;
            } else {
                xyScaleFactor = (player->unk_B86[1] + (f32)0x4000) / (f32)0xC000;
            }
            zScaleFactor = -xyScaleFactor;
            speedScaleFactor = 1.0f;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_NON_ZORA_SWIMMING) {
            depthPhase = 0x3F0;
            screenPlanePhase = 0x156;
            sDistortionRequest.timer = 2;
            player = GET_PLAYER(play);
            depthPhaseStep = 359.2f;
            screenPlanePhaseStep = -18.5f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.0f;
            switch (MergeShip_Distortion_GetUnderwaterCurrentSpeed(play, player)) {
                case 3:
                    xScale = 0.12f;
                    yScale = 0.12f;
                    zScale = 0.08f;
                    speed = 0.18f;
                    break;
                case 2:
                    xScale = 0.12f;
                    yScale = 0.12f;
                    zScale = 0.08f;
                    speed = 0.12f;
                    break;
                case 1:
                    xScale = 0.12f;
                    yScale = 0.12f;
                    zScale = 0.08f;
                    speed = 0.08f;
                    break;
                default:
                    xScale = 0.12f;
                    yScale = 0.12f;
                    zScale = 0.08f;
                    speed = 0.05f;
                    break;
            }
            if ((camera->waterYPos - camera->eye.y) > 2000.0f) {
                waterYScaleFactor = 1.0f;
            } else {
                waterYScaleFactor = (camera->waterYPos - camera->eye.y) / 2000.0f;
            }
            xyScaleFactor = speedScaleFactor = (waterYScaleFactor * 0.15f) + 0.35f + (speedRatio * 0.4f);
            zScaleFactor = 0.9f - xyScaleFactor;
        } else if (sDistortionRequest.type & MERGESHIP_DISTORTION_HOT_ROOM) {
            depthPhase = 0x3F0;
            screenPlanePhase = 0x156;
            sDistortionRequest.timer = 2;
            depthPhaseStep = 0.0f;
            screenPlanePhaseStep = 150.0f;
            rotX = 0.0f;
            rotY = 0.0f;
            rotZ = 0.0f;
            xScale = -0.01f;
            yScale = 0.01f;
            zScale = 0.01f;
            speed = 0.6f;
            speedScaleFactor = 1.0f;
            zScaleFactor = 1.0f;
            xyScaleFactor = 1.0f;
        } else {
            return;
        }

        depthPhase += MM_CAM_DEG_TO_BINANG(depthPhaseStep);
        screenPlanePhase += MM_CAM_DEG_TO_BINANG(screenPlanePhaseStep);

        View_SetDistortionOrientation(&play->view, Math_CosS(depthPhase) * (MM_DEG_TO_RAD(rotX) * xyScaleFactor),
                                      Math_SinS(depthPhase) * (MM_DEG_TO_RAD(rotY) * xyScaleFactor),
                                      Math_SinS(screenPlanePhase) * (MM_DEG_TO_RAD(rotZ) * zScaleFactor));
        View_SetDistortionScale(&play->view, (Math_SinS(screenPlanePhase) * (xScale * xyScaleFactor)) + 1.0f,
                                (Math_CosS(screenPlanePhase) * (yScale * xyScaleFactor)) + 1.0f,
                                (Math_CosS(depthPhase) * (zScale * zScaleFactor)) + 1.0f);
        View_SetDistortionSpeed(&play->view, speed * speedScaleFactor);

        sDistortionRequest.state = MM_DISTORTION_ACTIVE;
    } else if (sDistortionRequest.state != MM_DISTORTION_INACTIVE) {
        // Ocarina of Time's camera rewrites its own water/heat distortion every frame it is active,
        // so clearing here only releases MM's.
        View_ClearDistortion(&play->view);

        sDistortionRequest.state = MM_DISTORTION_INACTIVE;
        sDistortionRequest.timer = 0;
    }

    if (sDistortionRequest.timer != 0) {
        sDistortionRequest.timer--;
        if (sDistortionRequest.timer == 0) {
            sDistortionRequest.type = 0;
        }
    }
}

// =============================================================================================
// MM motion blur (src/code/z_play.c, as 2S2H renders it through framebuffer_effects).
// =============================================================================================

typedef enum {
    MM_MOTION_BLUR_OFF,
    MM_MOTION_BLUR_SETUP,
    MM_MOTION_BLUR_PROCESS
} MMMotionBlurStatus;

static struct {
    u8 enabled;          // MM R_MOTION_BLUR_ENABLED
    u8 priorityEnabled;  // MM R_MOTION_BLUR_PRIORITY_ENABLED
    u32 alpha;           // MM R_MOTION_BLUR_ALPHA
    u32 priorityAlpha;   // MM R_MOTION_BLUR_PRIORITY_ALPHA
    MMMotionBlurStatus status;
} sMotionBlur;

void MergeShip_MotionBlur_Enable(u32 alpha) {
    sMotionBlur.alpha = alpha;
    sMotionBlur.enabled = true;
}

void MergeShip_MotionBlur_Disable(void) {
    sMotionBlur.enabled = false;
}

void MergeShip_MotionBlur_EnablePriority(u32 alpha) {
    sMotionBlur.priorityAlpha = alpha;
    sMotionBlur.priorityEnabled = true;
}

void MergeShip_MotionBlur_DisablePriority(void) {
    sMotionBlur.priorityEnabled = false;
}

// MM Play_DrawMotionBlur.
void MergeShip_MotionBlur_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 alpha;
    Gfx* gfx;
    Gfx* gfxHead;
    static u32 lastBlurWidth;
    static u32 lastBlurHeight;
    static u8 hasCopiedForFrame;

    if (sMotionBlur.priorityEnabled) {
        alpha = sMotionBlur.priorityAlpha;
        if (sMotionBlur.status == MM_MOTION_BLUR_OFF) {
            sMotionBlur.status = MM_MOTION_BLUR_SETUP;
        }
    } else if (sMotionBlur.enabled) {
        alpha = sMotionBlur.alpha;
        if (sMotionBlur.status == MM_MOTION_BLUR_OFF) {
            sMotionBlur.status = MM_MOTION_BLUR_SETUP;
        }
    } else {
        alpha = 0;
        sMotionBlur.status = MM_MOTION_BLUR_OFF;
    }

    if (sMotionBlur.status != MM_MOTION_BLUR_OFF) {
        OPEN_DISPS(gfxCtx);

        gfxHead = POLY_OPA_DISP;
        gfx = Graph_GfxPlusOne(gfxHead);

        gSPDisplayList(OVERLAY_DISP++, gfx);

        // 2S2H: skip the blur for a frame when the render size changes, so a blank buffer is not
        // blended in.
        if ((sMotionBlur.status == MM_MOTION_BLUR_PROCESS) &&
            ((lastBlurWidth != OTRGetGameRenderWidth()) || (lastBlurHeight != OTRGetGameRenderHeight()))) {
            sMotionBlur.status = MM_MOTION_BLUR_SETUP;
        }

        if (sMotionBlur.status == MM_MOTION_BLUR_PROCESS) {
            FB_DrawFromFramebuffer(&gfx, gBlurFrameBuffer, alpha);
        } else {
            sMotionBlur.status = MM_MOTION_BLUR_PROCESS;
            lastBlurWidth = OTRGetGameRenderWidth();
            lastBlurHeight = OTRGetGameRenderHeight();
        }

        hasCopiedForFrame = false;
        // 2S2H's default: copy once per game frame (MotionBlur.Interpolate off).
        FB_CopyToFramebuffer(&gfx, 0, gBlurFrameBuffer, true, &hasCopiedForFrame);

        gSPEndDisplayList(gfx++);
        Graph_BranchDlist(gfxHead, gfx);
        POLY_OPA_DISP = gfx;

        CLOSE_DISPS(gfxCtx);
    }
}

// =============================================================================================

void MergeShip_TransformFx_Reset(PlayState* play) {
    memset(&sTransformCs, 0, sizeof(sTransformCs));
    sTransformCs.subCamId = SUBCAM_NONE;
    sDistortionRequest.type = 0;
    sDistortionRequest.timer = 0;
    sDistortionRequest.state = MM_DISTORTION_INACTIVE;
    sMotionBlur.enabled = false;
    sMotionBlur.priorityEnabled = false;
    sMotionBlur.status = MM_MOTION_BLUR_OFF;
    R_PLAY_FILL_SCREEN_ON = 0;
    R_PLAY_FILL_SCREEN_ALPHA = 0;
}
