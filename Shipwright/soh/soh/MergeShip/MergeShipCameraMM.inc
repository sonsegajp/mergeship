// MergeShip2 -- Majora's Mask's mask-transformation camera (CAM_SET_MASK_TRANSFORMATION).
//
// Included at the end of z_camera.c, so these run inside Ocarina of Time's own Camera_Update exactly
// where MM's run inside MM's: the camera's roll, field of view, up vector and frame interpolation are
// applied by the engine, not by the player. Ported from 2ship2harkinian/mm/src/code/z_camera.c
// (Camera_Demo4, Camera_Demo5) and z_olib.c; the helpers below are MM's, kept with MM's argument
// order (OoT's Camera_LERPCeilVec3f takes its y and xz scales the other way round).

#include "soh/MergeShip/MergeShipTransformFx.h"

// MM z64camera.h / z64math.h.
#define MM_CAM_DEG_TO_BINANG(degrees) (s16)((degrees) * ((f32)0xFFFF / 360) + .5f)
#define MM_M_PIf 3.14159265358979323846f
#define MM_DEG_TO_RAD(degrees) ((degrees) * (MM_M_PIf / 180.0f))
#define MM_RAD_TO_DEG(radians) ((radians) * (180.0f / MM_M_PIf))

typedef struct {
    f32 r;
    s16 pitch;
    s16 yaw;
} MMVecGeo;

// MM z_olib.c: OLib_Vec3fToVecSph, OLib_Vec3fToVecGeo, OLib_Vec3fDiffToVecGeo.
static MMVecGeo MMOLib_Vec3fDiffToVecGeo(Vec3f* a, Vec3f* b) {
    Vec3f vec;
    MMVecGeo geo;
    f32 distXZSq;
    f32 distXZ;

    vec.x = b->x - a->x;
    vec.y = b->y - a->y;
    vec.z = b->z - a->z;
    distXZSq = SQ(vec.x) + SQ(vec.z);
    distXZ = sqrtf(distXZSq);

    // VecSph
    if ((distXZ == 0.0f) && (vec.y == 0.0f)) {
        geo.pitch = 0;
    } else {
        geo.pitch = MM_CAM_DEG_TO_BINANG(MM_RAD_TO_DEG(Math_FAtan2F(distXZ, vec.y)));
    }
    geo.r = sqrtf(SQ(vec.y) + distXZSq);
    if ((vec.x == 0.0f) && (vec.z == 0.0f)) {
        geo.yaw = 0;
    } else {
        geo.yaw = MM_CAM_DEG_TO_BINANG(MM_RAD_TO_DEG(Math_FAtan2F(vec.x, vec.z)));
    }
    // VecSph -> VecGeo
    geo.pitch = 0x4000 - geo.pitch;
    return geo;
}

// MM z_olib.c: OLib_VecGeoToVec3f + OLib_AddVecGeoToVec3f.
static Vec3f MMOLib_AddVecGeoToVec3f(Vec3f* a, MMVecGeo* geo) {
    Vec3f v;
    s16 pitch = 0x4000 - geo->pitch; // VecGeo -> VecSph
    f32 sinPitch = Math_SinS(pitch);
    f32 cosPitch = Math_CosS(pitch);
    f32 sinYaw = Math_SinS(geo->yaw);
    f32 cosYaw = Math_CosS(geo->yaw);
    f32 r = geo->r;

    v.x = r * sinPitch * sinYaw + a->x;
    v.y = r * cosPitch + a->y;
    v.z = r * sinPitch * cosYaw + a->z;
    return v;
}

static f32 MMCamera_ScaledStepToCeilF(f32 target, f32 cur, f32 stepScale, f32 minDiff) {
    f32 diff = target - cur;
    f32 step = diff * stepScale;

    return (fabsf(diff) >= minDiff) ? cur + step : target;
}

static s16 MMCamera_ScaledStepToCeilS(s16 target, s16 cur, f32 stepScale, s16 minDiff) {
    s16 diff = target - cur;
    s16 step = diff * stepScale + 0.5f;

    return (ABS(diff) >= minDiff) ? cur + step : target;
}

static void MMCamera_ScaledStepToCeilVec3f(Vec3f* target, Vec3f* cur, f32 xzStepScale, f32 yStepScale, f32 minDiff) {
    cur->x = MMCamera_ScaledStepToCeilF(target->x, cur->x, xzStepScale, minDiff);
    cur->y = MMCamera_ScaledStepToCeilF(target->y, cur->y, yStepScale, minDiff);
    cur->z = MMCamera_ScaledStepToCeilF(target->z, cur->z, xzStepScale, minDiff);
}

// MM Demo4ReadWriteData / Demo5ReadWriteData. Only one transformation camera exists at a time.
static struct {
    Vec3f unk_00;
    f32 unk_0C;
    f32 unk_10;
    f32 unk_14;
    MMVecGeo unk_18;
    s16 timer;
    s16 unk_20;
} sMMDemo4;

static struct {
    Vec3f unk_00;
    f32 unk_0C;
    f32 unk_10;
    f32 unk_14;
    f32 unk_18;
    MMVecGeo unk_1C;
    s16 timer;
    s16 unk_24;
} sMMDemo5;

#define MM_TRANSFORM_CAM_INTERFACE_FLAGS 0xFF00 // CAM_LETTERBOX_IGNORE | CAM_HUD_VISIBILITY_IGNORE

static s32 MMCamera_ReloadParams(Camera* camera) {
    return (camera->animState == 0) || (camera->animState == 10) || (camera->animState == 20);
}

// MM Camera_Demo4: mask transformation starting as a human.
s32 Camera_MMDemo4(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMVecGeo atToEye;
    PosRot focalActorFocus;
    f32 sp58;
    f32 sin;

    atToEye = MMOLib_Vec3fDiffToVecGeo(at, eye);

    if (MMCamera_ReloadParams(camera)) {
        camera->animState = 0;
        sMMDemo4.unk_00 = *at;
        sMMDemo4.unk_18 = atToEye;
        sMMDemo4.unk_14 = camera->fov;
    }

    focalActorFocus = camera->player->actor.focus;
    sCameraInterfaceFlags = MM_TRANSFORM_CAM_INTERFACE_FLAGS;

    switch (camera->animState) {
        default:
            break;

        case 0:
            camera->animState++;
            sMMDemo4.timer = 0;

            if (!(atToEye.r < 40.0f)) {
                atToEye.r = 40.0f;
            }

            camera->fov = 80.0f;
            sMMDemo4.unk_10 = (Rand_ZeroOne() - 0.5f) * 40.0f;
            // fallthrough
        case 1:
            // Camera fixed on human player as the mask moves from the pocket to the face
            // Camera rolls left and right
            if (sMMDemo4.timer >= 12) {
                sMMDemo4.unk_0C = (sMMDemo4.timer - 12) * (135.0f / 13.0f);
                sin = Math_SinF(MM_DEG_TO_RAD(sMMDemo4.unk_0C));
                sMMDemo4.unk_0C = ((sMMDemo4.unk_10 < 0.0f) ? -1.0f : 1.0f) * sin;
                if (sMMDemo4.timer == 12) {
                    MergeShip_Distortion_Request(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
                    MergeShip_Distortion_SetDuration(26);
                }
            } else {
                sMMDemo4.unk_0C = 0.0f;
            }

            sp58 = sMMDemo4.timer * (6.0f / 19.0f);
            sMMDemo4.unk_20 = focalActorPosRot->rot.y + 0x4000;
            focalActorFocus.pos.x =
                (Math_SinS(sMMDemo4.unk_20) * sp58 * sMMDemo4.unk_0C) + focalActorPosRot->pos.x;
            focalActorFocus.pos.z =
                (Math_CosS(sMMDemo4.unk_20) * sp58 * sMMDemo4.unk_0C) + focalActorPosRot->pos.z;
            focalActorFocus.pos.y -= (focalActorFocus.pos.y - focalActorPosRot->pos.y) * 0.099999994f;
            MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.2f, 0.2f, 0.1f);

            sp58 = (sMMDemo4.timer * (30.0f / 19.0f));
            camera->roll = MM_CAM_DEG_TO_BINANG(sp58 * sMMDemo4.unk_0C);

            sp58 = 1.0f / (38 - sMMDemo4.timer);
            sMMDemo4.timer++;
            atToEye.r = MMCamera_ScaledStepToCeilF(30.0f, atToEye.r, sp58, 0.1f);
            atToEye.pitch = 0;

            if (sMMDemo4.timer >= 38) {
                sMMDemo4.timer = 24;
                camera->animState++;
                sMMDemo4.unk_0C = (32.0f - camera->fov) / 24.0f;
                MergeShip_Distortion_Request(MERGESHIP_DISTORTION_MASK_TRANSFORM_2);
            }
            break;

        case 2:
            // Camera steadies as human player is fully crouched down and hiding face
            if (sMMDemo4.timer == 24) {
                at->x = (Math_SinS(focalActorPosRot->rot.y) * -7.0f) + focalActorPosRot->pos.x;
                at->y = focalActorFocus.pos.y - ((focalActorFocus.pos.y - focalActorPosRot->pos.y) * 0.1f);
                at->z = (Math_CosS(focalActorPosRot->rot.y) * -7.0f) + focalActorPosRot->pos.z;
            } else {
                focalActorFocus.pos.x = (Math_SinS(focalActorPosRot->rot.y) * -7.0f) + focalActorPosRot->pos.x;
                focalActorFocus.pos.y -= (focalActorFocus.pos.y - camera->playerPosRot.pos.y) * 0.1f;
                focalActorFocus.pos.z = (Math_CosS(focalActorPosRot->rot.y) * -7.0f) + focalActorPosRot->pos.z;
                MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.25f, 0.25f, 0.1f);
            }

            if (sMMDemo4.timer > 0) {
                camera->fov += sMMDemo4.unk_0C;
            }

            sMMDemo4.timer--;

            atToEye.r = 35.0f;
            atToEye.pitch = 0x2000;
            camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.1f, 5);

            if (sMMDemo4.timer <= 0) {
                sMMDemo4.unk_20 = 630;
                sMMDemo4.timer = 0;
                sMMDemo4.unk_0C = (60.0f - camera->fov) / sMMDemo4.unk_20;
                camera->animState = 3;
            }
            break;

        case 3:
            // Camera zooms into human players face with bulging eyes
            focalActorFocus.pos.x = focalActorPosRot->pos.x;
            focalActorFocus.pos.y -= (focalActorFocus.pos.y - camera->playerPosRot.pos.y) * 0.1f;
            focalActorFocus.pos.z = focalActorPosRot->pos.z;

            MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.25f, 0.25f, 0.1f);

            camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.1f, 5);
            sMMDemo4.timer++;
            camera->fov += sMMDemo4.unk_0C * sMMDemo4.timer;
            atToEye.pitch = 0x2000;
            atToEye.r = 35.0f;

            if (sMMDemo4.timer >= 35) {
                MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
                MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_2);
                camera->animState = 4;
            }
            break;

        case 999:
            focalActorFocus = camera->player->actor.focus;
            MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
            MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_2);
            camera->animState = 4;
            break;

        case 4:
            // Camera backs up as player is now in a transformed state
            focalActorFocus.pos.y -= (focalActorFocus.pos.y - camera->playerPosRot.pos.y) * 0.1f;
            MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.1f, 0.1f, 0.1f);
            atToEye = sMMDemo4.unk_18;
            camera->fov = sMMDemo4.unk_14;
            camera->roll = 0;
            break;
    }

    camera->eyeNext = MMOLib_AddVecGeoToVec3f(at, &atToEye);
    *eye = camera->eyeNext;

    return true;
}

// MM Camera_Demo5: mask transformation starting as a non-human form.
s32 Camera_MMDemo5(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    MMVecGeo atToEye;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    PosRot focalActorFocus;
    f32 new_var;
    f32 sp58;
    f32 sin;

    atToEye = MMOLib_Vec3fDiffToVecGeo(at, eye);

    if (MMCamera_ReloadParams(camera)) {
        camera->animState = 0;
        sMMDemo5.unk_00 = *at;
        sMMDemo5.unk_1C = atToEye;
        sMMDemo5.unk_18 = camera->fov;
    }

    focalActorFocus = camera->player->actor.focus;
    sCameraInterfaceFlags = MM_TRANSFORM_CAM_INTERFACE_FLAGS;

    switch (camera->animState) {
        case 0:
            camera->animState++;
            atToEye.pitch = 0;
            sMMDemo5.timer = 0x12;
            sMMDemo5.unk_14 = 80.0f;
            atToEye.r = 30.0f;
            camera->fov = 80.0f;
            sMMDemo5.unk_10 = (Rand_ZeroOne() - 0.5f) * 40.0f;
            camera->roll = 0;
            focalActorFocus.pos.x = focalActorPosRot->pos.x;
            focalActorFocus.pos.z = focalActorPosRot->pos.z;
            *at = focalActorFocus.pos;
            // fallthrough
        case 1:
            // Camera remains still as player moves hands to face
            sMMDemo5.timer--;
            if (sMMDemo5.timer <= 0) {
                sMMDemo5.timer = 0;
                camera->animState = 2;
                sMMDemo5.unk_24 = camera->playerPosRot.rot.y + 0x4000;
                sMMDemo5.timer = 46;
                MergeShip_Distortion_Request(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
                MergeShip_Distortion_SetDuration(46);
            }
            break;

        case 2:
            // Camera zooms out while rolling back and forth
            sMMDemo5.unk_0C = sMMDemo5.timer * (180.0f / 23.0f);
            sp58 = MM_DEG_TO_RAD(sMMDemo5.unk_0C);
            sin = Math_SinF(sp58);
            sMMDemo5.unk_0C = ((sMMDemo5.unk_10 < 0.0f) ? -1.0f : 1.0f) * sin;
            new_var = (46 - sMMDemo5.timer) * (5.0f / 46.0f);
            focalActorFocus.pos.x =
                (Math_SinS(sMMDemo5.unk_24) * new_var * sMMDemo5.unk_0C) + focalActorPosRot->pos.x;
            focalActorFocus.pos.z =
                (Math_CosS(sMMDemo5.unk_24) * new_var * sMMDemo5.unk_0C) + focalActorPosRot->pos.z;
            focalActorFocus.pos.y -= (focalActorFocus.pos.y - camera->playerPosRot.pos.y) * 0.2f;
            MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.1f, 0.1f, 0.1f);
            new_var = sMMDemo5.timer * (10.0f / 23.0f);
            camera->roll = MM_CAM_DEG_TO_BINANG(sMMDemo5.unk_0C * new_var);
            new_var = 1.0f / sMMDemo5.timer;
            atToEye.r = MMCamera_ScaledStepToCeilF(sMMDemo5.unk_14, atToEye.r, new_var, 0.1f);
            sMMDemo5.timer--;
            atToEye.pitch = 0;
            if (sMMDemo5.timer <= 0) {
                camera->animState = 3;
                MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
            }
            break;

        case 999:
            focalActorFocus = camera->player->actor.focus;
            camera->animState = 3;
            MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_MASK_TRANSFORM_1);
            break;

        case 3:
            // Player is in new form
            focalActorFocus.pos.y -= (focalActorFocus.pos.y - camera->playerPosRot.pos.y) * 0.1f;
            MMCamera_ScaledStepToCeilVec3f(&focalActorFocus.pos, at, 0.1f, 0.1f, 0.1f);
            camera->roll = 0;
            atToEye = sMMDemo5.unk_1C;
            camera->fov = sMMDemo5.unk_18;
            break;

        default:
            break;
    }

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &atToEye);
    *eye = *eyeNext;

    return true;
}

/*=====================================================================================================
 * MergeShip2 -- Majora's Mask's camera modes for the transformation forms.
 *
 * MM's player asks for camera modes Ocarina of Time has no slot for (GORONDASH, DEKUFLY, DEKUSHOOT,
 * DEKUHIDE, ZORAFIN, DEKUFLYZ, GORONJUMP, CHARGEZ, ZORAFINZ). OoT settings keep a 21-bit valid-mode
 * mask that is already full, so those modes live here instead of in OoT's CameraModeType:
 *
 *  - MergeShip_Camera_RequestMMMode (called by the player where MM calls Camera_ChangeMode) runs MM's
 *    Camera_ChangeModeFlags against MM's mode table for the OoT setting in use (MMCam_GetSetting holds
 *    the OoT->MM setting mapping) and records the MM mode as the main camera's mode.
 *  - While that mode is active and requested again each frame, OoT's Camera_Update runs MM's update
 *    function for it (Camera_MMNormal1/Parallel1/Jump3/Subject1 below, ported from MM z_camera.c)
 *    with MM's parameters (MM z_camera_data.inc) instead of the OoT function for camera->mode.
 *  - An OoT mode request (Camera_RequestModeImpl), a setting change (Camera_ChangeSettingFlags) or a
 *    frame without an MM request leaves the MM mode with MM's mode-change semantics, and OoT's own
 *    mode resumes from animState 0 (RELOAD_PARAMS), as MM resets on every mode change.
 *
 * The MM functions keep their own paramData (sMMCam.params) and swing state (MM D_801EDC30), so the
 * OoT functions' camera->paramData is never touched. MM Camera fields map to OoT's as: focalActor ->
 * player, focalActorPosRot -> playerPosRot, focalActorAtOffset -> posOffset, unk_0F0 ->
 * playerPosDelta, atLerpStepScale -> atLERPStepScale, focalActorFloorHeight -> playerGroundY, bgId ->
 * bgCheckId, bgCamIndex -> camDataIdx, stateFlags -> unk_14C, behaviorFlags -> unk_14A, camId ->
 * thisIdx, view.unk164 -> view.unk_124, CAM_STATUS_ACTIVE -> CAM_STAT_ACTIVE.
 *===================================================================================================*/

#include "soh/MergeShip/MergeShipCameraMMModes.inc"
