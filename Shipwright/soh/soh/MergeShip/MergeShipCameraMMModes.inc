#include <stdlib.h> // getenv (MergeShip2 Android: z_camera.c does not include it)
// MergeShip2 -- Majora's Mask's camera modes for the transformation forms (see MergeShipCameraMM.inc).
// Included by MergeShipCameraMM.inc, at the end of z_camera.c, so the ported functions run inside OoT's
// Camera_Update with z_camera.c's statics. Ported from 2ship2harkinian/mm/src/code/z_camera.c,
// z_camera_data.inc and z_olib.c.

#include "soh/MergeShip/MergeShipCamera.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"

// MM z64math.h (OoT's BINANG_ROT180 subtracts 0x7FFF, MM's adds 0x8000: MM's is kept).
#define MM_TRUNCF_BINANG(f) (s16)(s32)(f)
#define MM_DEG_TO_BINANG(degrees) MM_TRUNCF_BINANG((degrees) * (0x8000 / 180.0f))
#define MM_BINANG_ROT180(angle) ((s16)((angle) + 0x8000))
#define MM_BINANG_SUB(a, b) ((s16)((a) - (b)))
#define MM_LERPIMP(v0, v1, t) ((v0) + (((v1) - (v0)) * (t)))
#define MM_ABS_ALT(x) ((x) < 0 ? -(x) : (x))

#define MM_GET_NEXT_RO_DATA(values) (*(values)++)
#define MM_GET_NEXT_SCALED_RO_DATA(values) (MM_GET_NEXT_RO_DATA(values) * 0.01f)

// MM player flags whose OoT names differ (the bits line up positionally, see PORTING_GUIDE.md).
#define MM_PLAYER_STATE1_4 (1 << 2) // MM: climbing a ledge (OoT names this bit HOOKSHOT_FALLING)
#define MM_PLAYER_STATE1_4000 PLAYER_STATE1_CLIMBING_LEDGE
#define MM_PLAYER_STATE1_200000 PLAYER_STATE1_CLIMBING_LADDER
#define MM_PLAYER_STATE3_10 PLAYER_STATE3_CHECK_FLOOR_WATER_COLLISION // same meaning in both games

// MM flag bits of the camera functions' interfaceFlags (low byte).
#define MM_NORMAL1_FLAG_0 (1 << 0)
#define MM_NORMAL1_FLAG_1 (1 << 1)
#define MM_NORMAL1_FLAG_2 (1 << 2)
#define MM_NORMAL1_FLAG_3 (1 << 3)
#define MM_NORMAL1_FLAG_4 (1 << 4)
#define MM_NORMAL1_FLAG_5 (1 << 5)
#define MM_NORMAL1_FLAG_6 (1 << 6)
#define MM_NORMAL1_FLAG_7 (1 << 7)
#define MM_NORMAL3_FLAG_1 (1 << 1)
#define MM_NORMAL3_FLAG_5 (1 << 5)
#define MM_NORMAL3_FLAG_6 (1 << 6)
#define MM_NORMAL3_FLAG_7 (1 << 7)
#define MM_PARALLEL1_FLAG_0 (1 << 0)
#define MM_PARALLEL1_FLAG_1 (1 << 1)
#define MM_PARALLEL1_FLAG_2 (1 << 2)
#define MM_PARALLEL1_FLAG_3 (1 << 3)
#define MM_PARALLEL1_FLAG_4 (1 << 4)
#define MM_PARALLEL1_FLAG_5 (1 << 5)
#define MM_PARALLEL1_FLAG_6 (1 << 6)
#define MM_PARALLEL1_FLAG_7 (1 << 7)
#define MM_JUMP2_FLAG_1 (1 << 1)
#define MM_JUMP3_FLAG_0 (1 << 0)
#define MM_JUMP3_FLAG_2 (1 << 2)
#define MM_JUMP3_FLAG_3 (1 << 3)
#define MM_JUMP3_FLAG_5 (1 << 5)
#define MM_JUMP3_FLAG_6 (1 << 6)
#define MM_JUMP3_FLAG_7 (1 << 7)
#define MM_SUBJECT1_FLAG_4 (1 << 4)

// MM CAM_STATE_DISABLE_MODE_CHANGE (OoT unk_14C 0x20), MM CAM_BEHAVIOR_MODE_VALID / _MODE_1 (unk_14A).
#define MM_CAM_STATE_DISABLE_MODE_CHANGE 0x20
#define MM_CAM_BEHAVIOR_MODE_VALID 0x20
#define MM_CAM_BEHAVIOR_MODE_1 0x2

// MM CameraCollision
typedef struct {
    Vec3f pos;
    Vec3f norm;
    CollisionPoly* poly;
    MMVecGeo geoNorm;
    s32 bgId;
} MMCamCollision;

// MM SwingAnimation
typedef struct {
    Vec3f collisionClosePoint;
    MMCamCollision atEyeColChk;
    MMCamCollision eyeAtColChk;
    f32 swingUpdateRate;
    s16 pitch;
    s16 yaw;
    s16 unk_64;
    s16 timer;
} MMSwingAnimation;

typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C;
        s16 unk_20;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00, unk_04;
        s16 unk_08, unk_0A, unk_0C, unk_0E;
        f32 unk_10;
    } rw;
} MMNormal1Data;

typedef struct {
    struct {
        f32 yOffset, distMin, distMax, yawUpdateRateInv, pitchUpdateRateInv, fovTarget, maxAtLERPScale;
        s16 pitchTarget;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 isZero, yPosOffset;
        s16 curPitch, yawUpdateRate, yawTimer, distTimer, flag, is1200;
    } rw;
} MMNormal3Data;

typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C;
        s16 unk_20, unk_22, unk_24;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00, unk_04, unk_08, timer1;
        Vec3f unk_10;
        s16 unk_1C, unk_1E, unk_20, timer2, timer3, unk_26;
    } rw;
} MMParallel1Data;

// MM Jump2ReadOnlyData / Jump2ReadWriteData
typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00;
        s16 unk_04, unk_06, unk_08, unk_0A, timer;
    } rw;
} MMJump2Data;

typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C;
        s16 unk_20;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00;
        s16 unk_04, unk_06, timer1, unk_0A;
        s32 timer2;
        s16 unk_10, unk_12, unk_1C;
    } rw;
} MMJump3Data;

typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00;
        s16 unk_04, unk_06, timer;
    } rw;
} MMSubject1Data;

/*--------------------------------------------------------------------------------------------------
 * MM camera data (MM z_camera_data.inc), kept as the raw values MM's functions read in order.
 *------------------------------------------------------------------------------------------------*/

typedef enum {
    MMCAM_FUNC_NONE,
    MMCAM_FUNC_NORMAL1,
    MMCAM_FUNC_NORMAL3,
    MMCAM_FUNC_PARALLEL1,
    MMCAM_FUNC_JUMP3,
    MMCAM_FUNC_SUBJECT1,
    // MM functions of MM's shared-setting modes (MergeShipCameraMMData.inc) that are not ported: while one of
    // these modes is active, an MM form keeps Ocarina of Time's function for it.
    MMCAM_FUNC_JUMP2,
    MMCAM_FUNC_KEEPON1,
    MMCAM_FUNC_KEEPON3,
    MMCAM_FUNC_BATTLE1,
    MMCAM_FUNC_SPECIAL5,
} MMCamFunc;

typedef struct {
    s16 funcId;
    const s16* values;
} MMCamModeData;

typedef struct {
    u32 validModes;
    const MMCamModeData* modes; // indexed by MM CameraModeType
} MMCamSetting;

#define MM_CAM_MODE_TABLE_SIZE 0x1E // MM CAM_MODE_MAX

// MM CameraModeType values of the modes both games have (MM z64camera.h).
#define MMCAM_MODE_NORMAL 0x00
#define MMCAM_MODE_JUMP 0x01
#define MMCAM_MODE_BOWARROWZ 0x04
#define MMCAM_MODE_FIRSTPERSON 0x06
#define MMCAM_MODE_FOLLOWBOOMERANG 0x07
#define MMCAM_MODE_FOLLOWTARGET 0x09
#define MMCAM_MODE_TARGET 0x0A
#define MMCAM_MODE_TALK 0x0B
#define MMCAM_MODE_SLINGSHOT 0x0C
#define MMCAM_MODE_BOWARROW 0x0D
#define MMCAM_MODE_BATTLE 0x0E
#define MMCAM_MODE_STILL 0x10
#define MMCAM_MODE_CHARGE 0x11
#define MMCAM_MODE_CLIMB 0x12
#define MMCAM_MODE_CLIMBZ 0x13
#define MMCAM_MODE_HOOKSHOT 0x14
#define MMCAM_MODE_FREEFALL 0x15
#define MMCAM_MODE_HANG 0x16
#define MMCAM_MODE_HANGZ 0x17
#define MMCAM_MODE_PUSHPULL 0x18
#define MMCAM_MODE_BOOMERANG 0x1B

// MM's cameraModes of the settings Ocarina of Time shares with MM by name (NORMAL0..4, DUNGEON0..2).
#include "soh/MergeShip/MergeShipCameraMMData.inc"

// Interface flags are CAM_INTERFACE_FLAGS(letterbox, hudVisibility, funcFlags) = letterbox | hud |
// funcFlags. OoT's Camera_UpdateInterface reads the same layout, and MM's letterbox sizes and
// HUD_VISIBILITY numbers are OoT's, so MM's values are used unchanged.

// CAM_FUNCDATA_NORM1(50, 160, 255, -2, 12, 10, 40, 92, 88, (NONE, ALL, FLAG_7 | FLAG_2 | FLAG_1 | FLAG_0))
static const s16 sMMSetNormal0ModeGoronDashData[] = { 50, 160, 255, -2, 12, 10, 40, 92, 88, 0x0087 };
// CAM_FUNCDATA_SUBJ1(0, 5, 50, 10, 0, 50, 65, 45, (NONE, ALL, 0))
static const s16 sMMSetNormal0ModeDekuShootData[] = { 0, 5, 50, 10, 0, 50, 65, 45, 0x0000 };
// CAM_FUNCDATA_NORM1(0xFFB0, 280, 320, 40, 20, 12, 40, 80, 80, (NONE, ALL, FLAG_7 | FLAG_1 | FLAG_0))
static const s16 sMMSetNormal0ModeDekuFlyData[] = { (s16)0xFFB0, 280, 320, 40, 20, 12, 40, 80, 80, 0x0083 };
// CAM_FUNCDATA_SUBJ1(5, 100, 100, 10, 0, 0, 0, 45, (MEDIUM, ALL, 0))
static const s16 sMMSetNormal0ModeZoraFinData[] = { 5, 100, 100, 10, 0, 0, 0, 45, 0x2000 };
// CAM_FUNCDATA_PARA1(100, 300, -20, 40, 0, 4, 5, 80, 60, (IGNORE, IGNORE, FLAG_5 | FLAG_1), 80, 20)
static const s16 sMMSetNormal0ModeDekuHideData[] = { 100, 300, -20, 40, 0, 4, 5, 80, 60, (s16)0xFF22, 80, 20 };
// CAM_FUNCDATA_PARA1(0xFFBA, 350, 10, 10, 0, 5, 5, 70, 90, (MEDIUM, ALL, FLAG_3 | FLAG_2 | FLAG_1), 90, 0)
static const s16 sMMSetNormal0ModeDekuFlyZData[] = { (s16)0xFFBA, 350, 10, 10, 0, 5, 5, 70, 90, 0x200E, 90, 0 };
// CAM_FUNCDATA_NORM1(-10, 160, 400, 0, 18, 12, 40, 80, 60, (NONE, ALL, FLAG_7|FLAG_3|FLAG_2|FLAG_1|FLAG_0))
static const s16 sMMSetNormal0ModeGoronJumpData[] = { -10, 160, 400, 0, 18, 12, 40, 80, 60, 0x008F };
// CAM_FUNCDATA_PARA1(0xFFB0, 300, -20, 40, 0, 50, 5, 80, 20, (NONE, ALL, FLAG_1), 60, 0)
static const s16 sMMSetNormal0ModeChargeZData[] = { (s16)0xFFB0, 300, -20, 40, 0, 50, 5, 80, 20, 0x0002, 60, 0 };
// CAM_FUNCDATA_PARA1(0, 350, 50, 0, 0, 5, 5, 45, 50, (MEDIUM, ALL, FLAG_1), 60, 0)
static const s16 sMMSetNormal0ModeTargetData[] = { 0, 350, 50, 0, 0, 5, 5, 45, 50, 0x2002, 60, 0 };
// CAM_FUNCDATA_PARA1(50, 250, -20, 40, 0, 50, 5, 80, 20, (NONE, ALL, FLAG_1), 60, 0)
static const s16 sMMSetDungeon0ModeChargeZData[] = { 50, 250, -20, 40, 0, 50, 5, 80, 20, 0x0002, 60, 0 };
// CAM_FUNCDATA_JUMP3(-40, 180, 200, 5, 20, 5, 40, 80, 35, (NONE, ALL, FLAG_3 | FLAG_2))
static const s16 sMMSetNormal3ModeGoronDashData[] = { -40, 180, 200, 5, 20, 5, 40, 80, 35, 0x000C };
// CAM_FUNCDATA_JUMP3(-40, 150, 250, 10, 18, 5, 60, 60, 60, (NONE, ALL, FLAG_2 | FLAG_0))
static const s16 sMMSetNormal3ModeDekuFlyData[] = { -40, 150, 250, 10, 18, 5, 60, 60, 60, 0x0005 };
// CAM_FUNCDATA_JUMP3(0xFFC4, 150, 250, -15, 18, 5, 60, 60, 40, (NONE, ALL, FLAG_7 | FLAG_2 | FLAG_0))
static const s16 sMMSetNormal3ModeDekuHideData[] = { (s16)0xFFC4, 150, 250, -15, 18, 5, 60, 60, 40, 0x0085 };
// CAM_FUNCDATA_JUMP3(-40, 150, 240, -5, 18, 5, 40, 70, 40, (NONE, ALL, FLAG_3 | FLAG_2))
static const s16 sMMSetNormal3ModeGoronJumpData[] = { -40, 150, 240, -5, 18, 5, 40, 70, 40, 0x000C };
// CAM_FUNCDATA_NORM3(-20, 250, 250, 0, 20, 20, 60, 50,
//                    (NONE, A_HEARTS_MAGIC_MINIMAP_WITH_OVERWRITE, NORMAL3_FLAG_7))
static const s16 sMMSetZoraDivingModeNormalData[] = { -20, 250, 250, 0, 20, 20, 60, 50, 0x0680 };

// MergeShip2: MM CAM_SET_GIANT (Camera_RequestGiantsMaskSetting), the Giant's Mask's camera. Its values are, like
// all of MM's, percentages of Player_GetHeight, which is the grown Link's (MergeShipGiantsMask.inc).
// CAM_FUNCDATA_NORM1(-40, 150, 160, -15, 25, 14, 50, 50, 70, (NONE, ALL, FLAG_7 | FLAG_1 | FLAG_0))
static const s16 sMMSetGiantModeNormalData[] = { -40, 150, 160, -15, 25, 14, 50, 50, 70, 0x0083 };
// CAM_FUNCDATA_PARA1(-40, 180, 80, 0, 0, 5, 5, 50, 50, (MEDIUM, ALL, FLAG_1), 60, 0)
static const s16 sMMSetGiantModeTargetData[] = { -40, 180, 80, 0, 0, 5, 5, 50, 50, 0x2002, 60, 0 };
// CAM_FUNCDATA_NORM1(-10, 100, 150, 10, 30, 10, 40, 70, 50, (NONE, ALL, FLAG_7 | FLAG_3 | FLAG_1 | FLAG_0))
static const s16 sMMSetGiantModeJumpData[] = { -10, 100, 150, 10, 30, 10, 40, 70, 50, 0x008B };
// CAM_FUNCDATA_NORM1(-40, 150, 160, 0xFFF4, 25, 14, 50, 50, 50, (IGNORE, ALL, FLAG_5 | FLAG_1 | FLAG_0))
static const s16 sMMSetGiantModeStillData[] = { -40, 150, 160, (s16)0xFFF4, 25, 14, 50, 50, 50, (s16)0xF023 };

// MM sCamSetGiantModes: sCamSetNormal0Modes with its own NORMAL, JUMP, TARGET and STILL.
static const MMCamModeData sMMCamDataGiant[MM_CAM_MODE_TABLE_SIZE] = {
    [0] = { MMCAM_FUNC_NORMAL1, sMMSetGiantModeNormalData },                           // CAM_MODE_NORMAL
    [1] = { MMCAM_FUNC_NORMAL1, sMMSetGiantModeJumpData },                             // CAM_MODE_JUMP
    [2] = { MMCAM_FUNC_NORMAL1, sMMData_sSetNormal0ModeGoronDashData },                // CAM_MODE_GORONDASH
    [3] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeDekuShootData },               // CAM_MODE_DEKUSHOOT
    [4] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeBowArrowZData },               // CAM_MODE_BOWARROWZ
    [5] = { MMCAM_FUNC_NORMAL1, sMMData_sSetNormal0ModeDekuFlyData },                  // CAM_MODE_DEKUFLY
    [6] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeFirstPersonData },             // CAM_MODE_FIRSTPERSON
    [7] = { MMCAM_FUNC_KEEPON1, sMMData_sSetNormal0ModeFollowBoomerangData },          // CAM_MODE_FOLLOWBOOMERANG
    [8] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeZoraFinData },                 // CAM_MODE_ZORAFIN
    [9] = { MMCAM_FUNC_KEEPON1, sMMData_sSetNormal0ModeFollowTargetData },             // CAM_MODE_FOLLOWTARGET
    [10] = { MMCAM_FUNC_PARALLEL1, sMMSetGiantModeTargetData },                        // CAM_MODE_TARGET
    [11] = { MMCAM_FUNC_KEEPON3, sMMData_sSetNormal0ModeTalkData },                    // CAM_MODE_TALK
    [12] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeSlingshotData },              // CAM_MODE_SLINGSHOT
    [13] = { MMCAM_FUNC_SUBJECT1, sMMData_sSetNormal0ModeBowArrowData },               // CAM_MODE_BOWARROW
    [14] = { MMCAM_FUNC_BATTLE1, sMMData_sSetNormal0ModeBattleData },                  // CAM_MODE_BATTLE
    [15] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeDekuHideData },              // CAM_MODE_DEKUHIDE
    [16] = { MMCAM_FUNC_NORMAL1, sMMSetGiantModeStillData },                           // CAM_MODE_STILL
    [17] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeChargeData },                // CAM_MODE_CHARGE
    [18] = { MMCAM_FUNC_JUMP2, sMMData_sSetNormal0ModeClimbData },                     // CAM_MODE_CLIMB
    [19] = { MMCAM_FUNC_JUMP2, sMMData_sSetNormal0ModeClimbZData },                    // CAM_MODE_CLIMBZ
    [20] = { MMCAM_FUNC_SPECIAL5, sMMData_sSetNormal0ModeHookshotData },               // CAM_MODE_HOOKSHOT
    [21] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeFreeFallData },              // CAM_MODE_FREEFALL
    [22] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeHangData },                  // CAM_MODE_HANG
    [23] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeHangZData },                 // CAM_MODE_HANGZ
    [24] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModePushPullData },              // CAM_MODE_PUSHPULL
    [25] = { MMCAM_FUNC_PARALLEL1, sMMData_sSetNormal0ModeDekuFlyZData },              // CAM_MODE_DEKUFLYZ
};

// MergeShip2: MM Giant's Mask, defined in MergeShipGiantsMask.inc (z_player.c).
s32 MergeShip_GiantsMask_IsWornBy(Player* player);

// MM Camera_RequestGiantsMaskSetting: the main camera, following the player, while the Giant's Mask is worn, is MM's
// CAM_SET_GIANT (the scene's camera data is not read then). Ocarina of Time's setting is left as it is; the giant's
// modes run MM's functions with MM's CAM_SET_GIANT data instead (MMCam_GetSharedRunMode).
s32 MergeShip_CameraMM_RequestGiantsMaskSetting(Camera* camera) {
    return (camera->thisIdx == CAM_ID_MAIN) && (camera->player != NULL) &&
           MergeShip_GiantsMask_IsWornBy(camera->player);
}

// MM sCamSetNormal0Modes (its MM-only entries).
static const MMCamModeData sMMCamSetNormal0Modes[MM_CAM_MODE_TABLE_SIZE] = {
    [MM_CAM_MODE_GORONDASH] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeGoronDashData },
    [MM_CAM_MODE_DEKUSHOOT] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeDekuShootData },
    [MM_CAM_MODE_DEKUFLY] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeDekuFlyData },
    [MM_CAM_MODE_ZORAFIN] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeZoraFinData },
    [MM_CAM_MODE_DEKUHIDE] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuHideData },
    [MM_CAM_MODE_DEKUFLYZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuFlyZData },
    [MM_CAM_MODE_GORONJUMP] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeGoronJumpData },
    [MM_CAM_MODE_CHARGEZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeChargeZData },
    [MM_CAM_MODE_ZORAFINZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeTargetData },
};

// MM sCamSetDungeon0Modes: Normal0's, with Dungeon0's CHARGEZ data.
static const MMCamModeData sMMCamSetDungeon0Modes[MM_CAM_MODE_TABLE_SIZE] = {
    [MM_CAM_MODE_GORONDASH] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeGoronDashData },
    [MM_CAM_MODE_DEKUSHOOT] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeDekuShootData },
    [MM_CAM_MODE_DEKUFLY] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeDekuFlyData },
    [MM_CAM_MODE_ZORAFIN] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeZoraFinData },
    [MM_CAM_MODE_DEKUHIDE] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuHideData },
    [MM_CAM_MODE_DEKUFLYZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuFlyZData },
    [MM_CAM_MODE_GORONJUMP] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeGoronJumpData },
    [MM_CAM_MODE_CHARGEZ] = { MMCAM_FUNC_PARALLEL1, sMMSetDungeon0ModeChargeZData },
    [MM_CAM_MODE_ZORAFINZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeTargetData },
};

// MM sCamSetNormal1Modes, which every other MM free-roaming setting with validModes 0x03FFFFFF shares
// for these modes (DUNGEON1..4, NORMAL2, NORMAL4, BIRDS_EYE_VIEW_0/1, TOWER_ASCENT, MINI_BOSS,
// BOSS_ODOLWA/TWINMOLD/MAJORA, ...): GORONJUMP, CHARGEZ and ZORAFINZ are not valid there.
static const MMCamModeData sMMCamSetNormal1Modes[MM_CAM_MODE_TABLE_SIZE] = {
    [MM_CAM_MODE_GORONDASH] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeGoronDashData },
    [MM_CAM_MODE_DEKUSHOOT] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeDekuShootData },
    [MM_CAM_MODE_DEKUFLY] = { MMCAM_FUNC_NORMAL1, sMMSetNormal0ModeDekuFlyData },
    [MM_CAM_MODE_ZORAFIN] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeZoraFinData },
    [MM_CAM_MODE_DEKUHIDE] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuHideData },
    [MM_CAM_MODE_DEKUFLYZ] = { MMCAM_FUNC_PARALLEL1, sMMSetNormal0ModeDekuFlyZData },
};

// MM sCamSetNormal3Modes (swimming at the water surface): Jump3 for the form modes.
static const MMCamModeData sMMCamSetNormal3Modes[MM_CAM_MODE_TABLE_SIZE] = {
    [MM_CAM_MODE_GORONDASH] = { MMCAM_FUNC_JUMP3, sMMSetNormal3ModeGoronDashData },
    [MM_CAM_MODE_DEKUFLY] = { MMCAM_FUNC_JUMP3, sMMSetNormal3ModeDekuFlyData },
    [MM_CAM_MODE_ZORAFIN] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeZoraFinData },
    [MM_CAM_MODE_DEKUHIDE] = { MMCAM_FUNC_JUMP3, sMMSetNormal3ModeDekuHideData },
    [MM_CAM_MODE_GORONJUMP] = { MMCAM_FUNC_JUMP3, sMMSetNormal3ModeGoronJumpData },
};

// MM sCamSetFixed0Modes / sCamSetCirc0Modes / sCamSetCirc3Modes: only the Subject1 aiming modes.
static const MMCamModeData sMMCamSetFixed0Modes[MM_CAM_MODE_TABLE_SIZE] = {
    [MM_CAM_MODE_DEKUSHOOT] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeDekuShootData },
    [MM_CAM_MODE_ZORAFIN] = { MMCAM_FUNC_SUBJECT1, sMMSetNormal0ModeZoraFinData },
};

static const MMCamModeData sMMCamSetNoFormModes[MM_CAM_MODE_TABLE_SIZE] = { { 0 } };

static const MMCamSetting sMMCamSettingNormal0 = { 0x3FFFFFFF, sMMCamSetNormal0Modes };
static const MMCamSetting sMMCamSettingDungeon0 = { 0x3FFFFFFF, sMMCamSetDungeon0Modes };
static const MMCamSetting sMMCamSettingNormal1 = { 0x03FFFFFF, sMMCamSetNormal1Modes };
static const MMCamSetting sMMCamSettingNormal3 = { 0x05FFEFF7, sMMCamSetNormal3Modes };
static const MMCamSetting sMMCamSettingFixed0 = { 0x000011D9, sMMCamSetFixed0Modes };
// MM settings whose validModes hold none of the form modes (validModes 0x00000001: doors, FIXED1..3,
// CIRCLE1/2/4/6/7, pre-rendered, cutscene and demo settings, PIVOT_DIVING, ZORA_DIVING, ...).
static const MMCamSetting sMMCamSettingNone = { 0x00000001, sMMCamSetNoFormModes };

/*--------------------------------------------------------------------------------------------------
 * State
 *------------------------------------------------------------------------------------------------*/

static struct {
    s16 mode;         // the main camera's active MM-only mode, MM_CAM_MODE_NONE while OoT's mode runs
    u8 zoraDiving;    // CAM_SET_PIVOT_WATER_SURFACE was entered by a diving Zora: MM's CAM_SET_ZORA_DIVING
    u32 requestFrame; // play->state.frames of the last request that kept `mode`
    // Values MM's Camera_Update computes for its focal actor, where OoT's differ (see TrackFocalActor).
    Vec3f unk_0F0;
    f32 xzSpeed;
    f32 speedRatio;
    f32 waterYPos;
    Vec3f meshActorPos;
    union {
        MMNormal1Data norm1;
        MMNormal3Data norm3;
        MMParallel1Data para1;
        MMJump3Data jump3;
        MMJump2Data jump2;
        MMSubject1Data subj1;
    } params;               // MM camera->paramData
    MMSwingAnimation swing; // MM D_801EDC30[CAM_ID_MAIN]
    s32 mmMeleeWeaponAnimation; // see MergeShip_Camera_SetMMMeleeWeaponAnimation
    s16 runMode;                // MM CameraModeType of the MM function running this frame (MM camera->mode)
    u8 mmFuncRan;               // last frame ran an MM function (a change restarts the next one, animState 0)
} sMMCam = { MM_CAM_MODE_NONE };

/*--------------------------------------------------------------------------------------------------
 * MM z_olib.c / z_camera.c helpers
 *------------------------------------------------------------------------------------------------*/

// MM OLib_VecGeoToVec3f
static Vec3f MMOLib_VecGeoToVec3f(MMVecGeo* geo) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    return MMOLib_AddVecGeoToVec3f(&zero, geo);
}

// MM OLib_Vec3fToVecGeo
static MMVecGeo MMOLib_Vec3fToVecGeo(Vec3f* vec) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    return MMOLib_Vec3fDiffToVecGeo(&zero, vec);
}

// MM OLib_Vec3fDistNormalize (same body as OoT's)
static Vec3f MMOLib_Vec3fDistNormalize(Vec3f* a, Vec3f* b) {
    Vec3f v;

    OLib_Vec3fDistNormalize(&v, a, b);
    return v;
}

static f32 MMCam_fabsf(f32 f) {
    return ABS(f);
}

static f32 MMCam_Vec3fMagnitude(Vec3f* vec) {
    return sqrtf(SQ(vec->x) + SQ(vec->y) + SQ(vec->z));
}

// MM Camera_QuadraticAttenuation
static f32 MMCam_QuadraticAttenuation(f32 xRange, f32 x) {
    f32 y;
    f32 absX = MMCam_fabsf(x);
    f32 percent40 = 0.4f;
    f32 percent60;
    f32 xQuadratic;
    f32 xMaxQuadratic;

    if (absX > xRange) {
        y = 1.0f;
    } else {
        percent60 = 1.0f - percent40;

        if (absX < (xRange * percent60)) {
            xQuadratic = SQ(x) * (1.0f - percent40);
            xMaxQuadratic = SQ(xRange * percent60);
            y = xQuadratic / xMaxQuadratic;
        } else {
            xQuadratic = SQ(xRange - absX) * percent40;
            xMaxQuadratic = SQ(0.4f * xRange);
            y = 1.0f - (xQuadratic / xMaxQuadratic);
        }
    }
    return y;
}

// MM Camera_SetUpdateRatesFastPitch
static void MMCam_SetUpdateRatesFastPitch(Camera* camera) {
    camera->yawUpdateRateInv = 100.0f;
    camera->pitchUpdateRateInv = 16.0f;
    camera->rUpdateRateInv = 20.0f;
    camera->yOffsetUpdateRate = 0.05f;
    camera->xzOffsetUpdateRate = 0.05f;
    camera->fovUpdateRate = 0.05f;
}

// MM Camera_SetUpdateRatesFastYaw
static void MMCam_SetUpdateRatesFastYaw(Camera* camera) {
    camera->rUpdateRateInv = 50.0f;
    camera->pitchUpdateRateInv = 100.0f;
    camera->yawUpdateRateInv = 50.0f;
    camera->yOffsetUpdateRate = 0.01f;
    camera->xzOffsetUpdateRate = 0.1f;
    camera->fovUpdateRate = 0.01f;
    if (camera->speedRatio > 1.0f) {
        camera->speedRatio = 1.0f;
    }
}

// MM Camera_AngleDiffAndScale
static s16 MMCam_AngleDiffAndScale(s16 angle1, s16 angle2, f32 scale) {
    return MM_BINANG_SUB(angle1, angle2) * scale;
}

// MM Camera_SetFocalActorAtOffset
static void MMCam_SetFocalActorAtOffset(Camera* camera, Vec3f* focalActorPos) {
    camera->posOffset.x = camera->at.x - focalActorPos->x;
    camera->posOffset.y = camera->at.y - focalActorPos->y;
    camera->posOffset.z = camera->at.z - focalActorPos->z;
}

// MM Camera_GetFocalActorHeight. Player_GetHeight returns MM's heights for the MM forms (z_actor.c).
static f32 MMCam_GetFocalActorHeight(Camera* camera) {
    return Player_GetHeight(camera->player);
}

// MM Camera_GetRunSpeedLimit -> MM Player_GetRunSpeedLimit
static f32 MMCam_GetRunSpeedLimit(Camera* camera) {
    Player* player = camera->player;

    if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        return 15.0f;
    } else if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        return (R_RUN_SPEED_LIMIT / 100.0f) * 0.6f;
    } else {
        return R_RUN_SPEED_LIMIT / 100.0f;
    }
}

// MM func_800CB7CC
static s32 MMCam_func_800CB7CC(Camera* camera) {
    return camera->player->stateFlags3 & MM_PLAYER_STATE3_10;
}

// (MM Camera_IsSwimming's Zora case is ported into OoT's Camera_GetWaterBoxDataIdx, its only caller.)

// MM func_800CB924 (Goron curled)
static s32 MMCam_func_800CB924(Camera* camera) {
    return camera->player->stateFlags3 & PLAYER_STATE3_1000;
}

// MM func_800CB950
static s32 MMCam_func_800CB950(Camera* camera) {
    return (MMCam_fabsf(camera->playerPosRot.pos.y - camera->playerGroundY) < 11.0f) ||
           (camera->player->actor.gravity > -0.1f) || (camera->player->stateFlags1 & MM_PLAYER_STATE1_200000);
}

// MM Camera_IsClimbingLedge
static s32 MMCam_IsClimbingLedge(Camera* camera) {
    return camera->player->stateFlags1 & MM_PLAYER_STATE1_4;
}

// MM func_800CBA7C tests MM's PLAYER_STATE2_800000, which only MM's knob-door action sets. OoT gives that
// bit to "near an ocarina actor", and doors stay OoT's here (their camera is OoT's door setting, where
// no MM mode runs), so MM's door state can never coincide with an MM mode: the test is always false.
static s32 MMCam_func_800CBA7C(Camera* camera) {
    return false;
}

// MM Camera_GetFocalActorPos
static Vec3f MMCam_GetFocalActorPos(Camera* camera) {
    return camera->player->bodyPartsPos[PLAYER_BODYPART_WAIST];
}

// MM Camera_IsUnderwaterAsZora
s32 MergeShip_CameraMM_IsUnderwaterAsZora(Camera* camera) {
    return (camera->player != NULL) && (camera->player->mergeShipForm == PLAYER_FORM_ZORA) &&
           (gMergeShipPlayerExt.mmBoots == MM_BOOTS_ZORA_UNDERWATER);
}

// MM Camera_IsDekuHovering (PLAYER_STATE3_2000 is only ever set by the Deku form)
s32 MergeShip_CameraMM_IsDekuHovering(Camera* camera) {
    return (camera->player != NULL) && (camera->player->stateFlags3 & PLAYER_STATE3_2000);
}

// MM func_800CBC84
static s32 MMCam_func_800CBC84(Camera* camera, Vec3f* from, MMCamCollision* to, s32 arg3) {
    CollisionContext* colCtx = &camera->play->colCtx;
    Vec3f toNewPos;
    Vec3f toPoint;
    Vec3f fromToNorm = MMOLib_Vec3fDistNormalize(from, &to->pos);
    f32 floorPolyY;
    s32 floorBgId;

    toPoint.x = to->pos.x + fromToNorm.x;
    toPoint.y = to->pos.y + fromToNorm.y;
    toPoint.z = to->pos.z + fromToNorm.z;

    if (!BgCheck_CameraLineTest1(colCtx, from, &toPoint, &toNewPos, &to->poly, (arg3 & 1) ? false : true, true,
                                 (arg3 & 2) ? false : true, -1, &floorBgId)) {
        toNewPos = to->pos;
        toNewPos.y += 5.0f;

        // (2S2H's guard: a NULL floorPoly takes the raycast branch.)
        if ((arg3 != 0) && MMCam_func_800CB7CC(camera) && (camera->player->actor.floorPoly != NULL)) {
            to->poly = camera->player->actor.floorPoly;
            floorBgId = camera->player->actor.floorBgId;
            to->norm.x = COLPOLY_GET_NORMAL(to->poly->normal.x);
            to->norm.y = COLPOLY_GET_NORMAL(to->poly->normal.y);
            to->norm.z = COLPOLY_GET_NORMAL(to->poly->normal.z);

            if (!Math3D_LineSegVsPlane(to->norm.x, to->norm.y, to->norm.z, to->poly->dist, from, &toPoint, &toNewPos,
                                       1)) {
                floorPolyY = to->pos.y - 20.0f;
            } else {
                toNewPos.y += 5.0f;
                floorPolyY = to->pos.y;
            }
        } else {
            floorPolyY = BgCheck_CameraRaycastFloor2(colCtx, &to->poly, &floorBgId, &toNewPos);
        }

        if ((to->pos.y - floorPolyY) > 5.0f) {
            to->norm.x = -fromToNorm.x;
            to->norm.y = -fromToNorm.y;
            to->norm.z = -fromToNorm.z;
            return 0;
        }
    }

    to->bgId = floorBgId;
    to->norm.x = COLPOLY_GET_NORMAL(to->poly->normal.x);
    to->norm.y = COLPOLY_GET_NORMAL(to->poly->normal.y);
    to->norm.z = COLPOLY_GET_NORMAL(to->poly->normal.z);
    to->pos.x = to->norm.x + toNewPos.x;
    to->pos.y = to->norm.y + toNewPos.y;
    to->pos.z = to->norm.z + toNewPos.z;

    return floorBgId + 1;
}

// MM func_800CBFA4
static void MMCam_func_800CBFA4(Camera* camera, Vec3f* arg1, Vec3f* arg2, s32 arg3) {
    MMCamCollision sp20;

    sp20.pos = *arg2;
    MMCam_func_800CBC84(camera, arg1, &sp20, arg3);
    *arg2 = sp20.pos;
}

// MM Camera_BgCheckInfo
static s32 MMCam_BgCheckInfo(Camera* camera, Vec3f* from, MMCamCollision* to) {
    Vec3f toNewPos;
    Vec3f fromToNorm;

    if (BgCheck_CameraLineTest1(&camera->play->colCtx, from, &to->pos, &toNewPos, &to->poly, true, true, true, -1,
                                &to->bgId)) {
        to->norm.x = COLPOLY_GET_NORMAL(to->poly->normal.x);
        to->norm.y = COLPOLY_GET_NORMAL(to->poly->normal.y);
        to->norm.z = COLPOLY_GET_NORMAL(to->poly->normal.z);
        to->pos = toNewPos;
        return to->bgId + 1;
    }

    fromToNorm = MMOLib_Vec3fDistNormalize(from, &to->pos);
    to->norm.x = -fromToNorm.x;
    to->norm.y = -fromToNorm.y;
    to->norm.z = -fromToNorm.z;
    return 0;
}

// MM Camera_BgCheck
static s32 MMCam_BgCheck(Camera* camera, Vec3f* from, Vec3f* to) {
    Vec3f intersect;
    s32 bgId;
    CollisionPoly* poly = NULL;

    if (BgCheck_CameraLineTest1(&camera->play->colCtx, from, to, &intersect, &poly, 1, 1, 1, -1, &bgId)) {
        *to = intersect;
        return true;
    }
    return false;
}

// MM Camera_CheckOOB
static s32 MMCam_CheckOOB(Camera* camera, Vec3f* from, Vec3f* to) {
    Vec3f intersect;
    s32 bgId;
    CollisionPoly* poly = NULL;

    if (BgCheck_CameraLineTest1(&camera->play->colCtx, from, to, &intersect, &poly, true, true, true, 0, &bgId) &&
        (CollisionPoly_GetPointDistanceFromPlane(poly, from) < 0.0f)) {
        return true;
    }
    return false;
}

// MM Camera_GetFloorYLayer. MM raycasts with BgCheck_EntityRaycastFloor5_3 (flags 0x3C). OoT's
// BgCheck_RaycastFloorImpl has no 0x20 flag (MM: skip polys whose surface data[0] bit 30 is set), so the
// closest call OoT has, BgCheck_EntityRaycastFloor5 (0x1C), is used.
static f32 MMCam_GetFloorYLayer(Camera* camera, Vec3f* norm, Vec3f* pos, s32* bgId) {
    CollisionPoly* floorPoly;
    f32 floorY = BgCheck_EntityRaycastFloor5(camera->play, &camera->play->colCtx, &floorPoly, bgId,
                                             &camera->player->actor, pos);

    if ((floorY == BGCHECK_Y_MIN) ||
        ((camera->playerGroundY < floorY) && !(COLPOLY_GET_NORMAL(floorPoly->normal.y) > 0.5f))) {
        norm->x = 0.0f;
        norm->y = 1.0f;
        norm->z = 0.0f;
        floorY = BGCHECK_Y_MIN;
    } else {
        norm->x = COLPOLY_GET_NORMAL(floorPoly->normal.x);
        norm->y = COLPOLY_GET_NORMAL(floorPoly->normal.y);
        norm->z = COLPOLY_GET_NORMAL(floorPoly->normal.z);
    }
    return floorY;
}

// MM Camera_GetPitchAdjFromFloorHeightDiffs
static s16 MMCam_GetPitchAdjFromFloorHeightDiffs(Camera* camera, s16 viewYaw, s16 shouldInit) {
    static f32 sFloorYNear;
    static f32 sFloorYFar;
    static MMCamCollision sFarColChk;
    Vec3f focalActorPos;
    Vec3f nearPos;
    Vec3f floorNorm;
    f32 checkOffsetY;
    s16 pitchNear;
    s16 pitchFar;
    f32 floorYDiffFar;
    f32 viewForwardsUnitX = Math_SinS(viewYaw);
    f32 viewForwardsUnitZ = Math_CosS(viewYaw);
    s32 bgId;
    f32 nearDist;
    f32 farDist;
    f32 floorYDiffNear;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    checkOffsetY = focalActorHeight * 1.2f;
    nearDist = focalActorHeight * 1.0f;
    farDist = focalActorHeight * 2.5f;

    focalActorPos.x = camera->playerPosRot.pos.x;
    focalActorPos.y = camera->playerGroundY + checkOffsetY;
    focalActorPos.z = camera->playerPosRot.pos.z;

    nearPos.x = focalActorPos.x + (nearDist * viewForwardsUnitX);
    nearPos.y = focalActorPos.y;
    nearPos.z = focalActorPos.z + (nearDist * viewForwardsUnitZ);

    if (shouldInit || ((camera->play->state.frames % 2) == 0)) {
        sFarColChk.pos.x = focalActorPos.x + (farDist * viewForwardsUnitX);
        sFarColChk.pos.y = focalActorPos.y;
        sFarColChk.pos.z = focalActorPos.z + (farDist * viewForwardsUnitZ);

        MMCam_BgCheckInfo(camera, &focalActorPos, &sFarColChk);

        if (shouldInit) {
            sFloorYNear = sFloorYFar = camera->playerGroundY;
        }
    } else {
        farDist = OLib_Vec3fDistXZ(&focalActorPos, &sFarColChk.pos);

        sFarColChk.pos.x += sFarColChk.norm.x * 5.0f;
        sFarColChk.pos.y += sFarColChk.norm.y * 5.0f;
        sFarColChk.pos.z += sFarColChk.norm.z * 5.0f;

        if (nearDist > farDist) {
            nearDist = farDist;
            sFloorYNear = sFloorYFar = MMCam_GetFloorYLayer(camera, &floorNorm, &sFarColChk.pos, &bgId);
        } else {
            sFloorYNear = MMCam_GetFloorYLayer(camera, &floorNorm, &nearPos, &bgId);
            sFloorYFar = MMCam_GetFloorYLayer(camera, &floorNorm, &sFarColChk.pos, &bgId);
        }

        if (sFloorYNear == BGCHECK_Y_MIN) {
            sFloorYNear = camera->playerGroundY;
        }
        if (sFloorYFar == BGCHECK_Y_MIN) {
            sFloorYFar = sFloorYNear;
        }
    }

    floorYDiffNear = (sFloorYNear - camera->playerGroundY) * 0.8f;
    floorYDiffFar = (sFloorYFar - camera->playerGroundY) * (20.0f * 0.01f);

    pitchNear = MM_CAM_DEG_TO_BINANG(MM_RAD_TO_DEG(Math_FAtan2F(floorYDiffNear, nearDist)));
    pitchFar = MM_CAM_DEG_TO_BINANG(MM_RAD_TO_DEG(Math_FAtan2F(floorYDiffFar, farDist)));

    return pitchNear + pitchFar;
}

// MM func_800CCCEC
static f32 MMCam_func_800CCCEC(Camera* camera, s16 reset) {
    static s32 D_801B9E5C = 0;
    static f32 D_801B9E60 = 0.0f;
    Vec3f offsetForwards;
    Vec3f angledOffsetForwards;
    PosRot focalActorPosRot;
    MMCamCollision camCollision;
    f32 forwardsUnitZ;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    f32 forwardsUnitX;
    f32 distResult;
    s16 yawForwardsOffset;
    f32 distResultAdj;

    if (reset) {
        D_801B9E5C = 0;
        D_801B9E60 = 0.0f;
    }

    focalActorPosRot = camera->playerPosRot;

    forwardsUnitX = Math_SinS(focalActorPosRot.rot.y);
    forwardsUnitZ = Math_CosS(focalActorPosRot.rot.y);

    offsetForwards.x = focalActorPosRot.pos.x + (30.0f * forwardsUnitX);
    offsetForwards.y = focalActorPosRot.pos.y + focalActorHeight;
    offsetForwards.z = focalActorPosRot.pos.z + (30.0f * forwardsUnitZ);

    camCollision.pos.x = focalActorPosRot.pos.x + (12.0f * forwardsUnitX);
    camCollision.pos.y = offsetForwards.y;
    camCollision.pos.z = focalActorPosRot.pos.z + (12.0f * forwardsUnitZ);

    if ((camera->play->state.frames % 2) != 0) {
        // Turn and project 65.92 degrees left
        yawForwardsOffset = focalActorPosRot.rot.y + MM_DEG_TO_BINANG(65.92f);

        angledOffsetForwards.x = (Math_SinS(yawForwardsOffset) * 50.0f) + offsetForwards.x;
        angledOffsetForwards.y = offsetForwards.y;
        angledOffsetForwards.z = (Math_CosS(yawForwardsOffset) * 50.0f) + offsetForwards.z;

        if ((MMCam_BgCheckInfo(camera, &angledOffsetForwards, &camCollision) != 0) &&
            MMCam_CheckOOB(camera, &offsetForwards, &angledOffsetForwards)) {
            distResult = OLib_Vec3fDistXZ(&offsetForwards, &camCollision.pos);

            if (!((D_801B9E5C == 2) && (-D_801B9E60 >= (50.0f - distResult)))) {
                D_801B9E5C = 1;
                distResult = 50.0f - distResult;
                D_801B9E60 = distResult;
                distResultAdj = (distResult == -distResult) ? 0.0f : distResult;
                return distResultAdj;
            }
        }
    } else {
        // Turn and project 90 degrees right
        yawForwardsOffset = focalActorPosRot.rot.y - MM_DEG_TO_BINANG(90);

        angledOffsetForwards.x = (Math_SinS(yawForwardsOffset) * 50.0f) + offsetForwards.x;
        angledOffsetForwards.y = offsetForwards.y;
        angledOffsetForwards.z = (Math_CosS(yawForwardsOffset) * 50.0f) + offsetForwards.z;

        if ((MMCam_BgCheckInfo(camera, &angledOffsetForwards, &camCollision) != 0) &&
            MMCam_CheckOOB(camera, &offsetForwards, &angledOffsetForwards)) {
            distResult = OLib_Vec3fDistXZ(&offsetForwards, &camCollision.pos);

            if (!((D_801B9E5C == 1) && (D_801B9E60 >= -(distResult - 50.0f)))) {
                D_801B9E5C = 2;
                distResult = distResult - 50.0f;
                D_801B9E60 = distResult;
                distResultAdj = (distResult == -distResult) ? 0.0f : distResult;
                return distResultAdj;
            }
        }
    }

    distResult = (D_801B9E5C != 0) ? D_801B9E60 : 0.0f;
    D_801B9E5C = 0;
    D_801B9E60 = 0.0f;
    return distResult;
}

// MM Camera_ClampLerpScale
static f32 MMCam_ClampLerpScale(Camera* camera, f32 maxLerpScale) {
    f32 ret;

    if (camera->atLERPStepScale < 0.12f) {
        ret = 0.12f;
    } else if (camera->atLERPStepScale >= maxLerpScale) {
        ret = maxLerpScale;
    } else {
        ret = 1.1f * camera->atLERPStepScale;
    }
    return ret;
}

// MM Camera_BgCheckCorner (MM func_800CAA14 is OoT's func_800427B4)
static Vec3f MMCam_BgCheckCorner(Vec3f* linePointA, Vec3f* linePointB, MMCamCollision* pointAColChk,
                                 MMCamCollision* pointBColChk) {
    Vec3f closestPoint;

    func_800427B4(pointAColChk->poly, pointBColChk->poly, linePointA, linePointB, &closestPoint);
    return closestPoint;
}

// MM func_800CD44C
static s32 MMCam_func_800CD44C(Camera* camera, MMVecGeo* diffGeo, MMCamCollision* camEyeCollision,
                               MMCamCollision* camAtCollision, s16 checkEye) {
    Vec3f* at = &camera->at;
    s32 atEyeBgId;
    s32 eyeAtBgId;
    s32 ret;
    f32 cosEyeAt;
    MMCamCollision camCollision;

    camEyeCollision->pos = camera->eyeNext;
    ret = 0;

    atEyeBgId = MMCam_func_800CBC84(camera, at, camEyeCollision, 0);
    if (atEyeBgId != 0) {
        camAtCollision->pos = *at;
        camEyeCollision->geoNorm = MMOLib_Vec3fToVecGeo(&camEyeCollision->norm);

        if (camEyeCollision->geoNorm.pitch > 0x2EE0) {
            camEyeCollision->geoNorm.yaw = diffGeo->yaw;
        }
        if (checkEye & 1) {
            memcpy(&camCollision, camAtCollision, sizeof(MMCamCollision));
        }

        eyeAtBgId = MMCam_BgCheckInfo(camera, &camera->eye, camAtCollision);

        if (eyeAtBgId == 0) {
            if (checkEye & 1) {
                memcpy(camAtCollision, &camCollision, sizeof(MMCamCollision));
            } else {
                return 3;
            }
        }

        if (camEyeCollision->poly == camAtCollision->poly) {
            return 3;
        }

        camAtCollision->geoNorm = MMOLib_Vec3fToVecGeo(&camAtCollision->norm);

        if (camAtCollision->geoNorm.pitch > 0x2EE0) {
            camAtCollision->geoNorm.yaw = MM_BINANG_ROT180(diffGeo->yaw);
        }

        if (atEyeBgId != eyeAtBgId) {
            ret = 3;
        } else {
            cosEyeAt = Math3D_Cos(&camEyeCollision->norm, &camAtCollision->norm);
            if (cosEyeAt < -0.5f) {
                ret = 6;
            } else if ((cosEyeAt > 0.5f) || (checkEye & 2)) {
                ret = 3;
            } else {
                ret = 1;
            }
        }
    }
    return ret;
}

// MM Camera_CalcSlopeYAdj
static f32 MMCam_CalcSlopeYAdj(Vec3f* floorNorm, s16 playerYRot, s16 eyeAtYaw, f32 adjAmt) {
    f32 tmp;
    MMVecGeo floorNormGeo = MMOLib_Vec3fToVecGeo(floorNorm);

    tmp = Math_CosS(floorNormGeo.pitch) * Math_CosS(playerYRot - floorNormGeo.yaw);
    return (MMCam_fabsf(tmp) * adjAmt) * Math_CosS(playerYRot - eyeAtYaw);
}

// MM Camera_CalcAtDefault
static s32 MMCam_CalcAtDefault(Camera* camera, MMVecGeo* eyeAtDir, f32 yOffset, s16 calcSlope) {
    Vec3f* at = &camera->at;
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    focalActorAtOffsetTarget.y = focalActorHeight + yOffset;
    focalActorAtOffsetTarget.x = 0.0f;
    focalActorAtOffsetTarget.z = 0.0f;

    if (calcSlope) {
        focalActorAtOffsetTarget.y -= OLib_ClampMaxDist(
            MMCam_CalcSlopeYAdj(&camera->floorNorm, focalActorPosRot->rot.y, eyeAtDir->yaw, 25.0f), focalActorHeight);
    }

    MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                   camera->yOffsetUpdateRate, 0.1f);

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;

    if (atTarget.y < (camera->playerGroundY + 10.0f)) {
        atTarget.y = camera->playerGroundY + 10.0f;
    }

    MMCamera_ScaledStepToCeilVec3f(&atTarget, at, camera->atLERPStepScale, camera->atLERPStepScale, 0.2f);
    return 1;
}

// MM Camera_CalcAtForScreen
static s32 MMCam_CalcAtForScreen(Camera* camera, MMVecGeo* eyeAtDir, f32 yOffset, f32* focalActorPosY,
                                 f32 deltaYMax) {
    f32 deltaY;
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    f32 clampedDeltaY;
    f32 clampedAbsScreenY;
    s16 absScreenPosY;
    s16 screenPosY;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    focalActorAtOffsetTarget.y = focalActorHeight + yOffset;
    focalActorAtOffsetTarget.x = 0.0f;
    focalActorAtOffsetTarget.z = 0.0f;

    Actor_GetScreenPos(camera->play, &camera->player->actor, &absScreenPosY, &screenPosY);
    screenPosY -= SCREEN_HEIGHT / 2;
    absScreenPosY = ABS(screenPosY);

    // result unused
    clampedAbsScreenY = OLib_ClampMaxDist(absScreenPosY / (f32)(SCREEN_HEIGHT / 2), 1.0f);

    deltaY = focalActorPosRot->pos.y - *focalActorPosY;
    clampedDeltaY = OLib_ClampMaxDist(deltaY, deltaYMax);

    if (absScreenPosY > (SCREEN_HEIGHT / 4)) {
        absScreenPosY = SCREEN_HEIGHT / 4;
    }

    clampedAbsScreenY = OLib_ClampMaxDist(absScreenPosY / (f32)(SCREEN_HEIGHT / 4), 1.0f);

    focalActorAtOffsetTarget.y -= clampedDeltaY * clampedAbsScreenY * clampedAbsScreenY;
    MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                   camera->yOffsetUpdateRate, 0.1f);

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;
    atTarget.y = CLAMP_MIN(atTarget.y, camera->playerGroundY + 10.0f);

    MMCamera_ScaledStepToCeilVec3f(&atTarget, &camera->at, camera->atLERPStepScale, camera->atLERPStepScale, 0.1f);
    return 1;
}

// MM Camera_CalcAtForNormal1
static s32 MMCam_CalcAtForNormal1(Camera* camera, MMVecGeo* arg1, f32 yOffset, f32 forwardDist) {
    PosRot* focalActorPosRot = &camera->playerPosRot;
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    Vec3f collisionFromPos;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    focalActorAtOffsetTarget.x = Math_SinS(focalActorPosRot->rot.y) * forwardDist;
    focalActorAtOffsetTarget.z = Math_CosS(focalActorPosRot->rot.y) * forwardDist;
    focalActorAtOffsetTarget.y = focalActorHeight + yOffset;

    MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                   camera->yOffsetUpdateRate, 0.1f);

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;

    collisionFromPos.x = focalActorPosRot->pos.x;
    collisionFromPos.y = atTarget.y;
    collisionFromPos.z = focalActorPosRot->pos.z;

    if (MMCam_BgCheck(camera, &collisionFromPos, &atTarget)) {
        atTarget.x -= camera->posOffset.x - (atTarget.x - collisionFromPos.x);
        atTarget.z -= camera->posOffset.z - (atTarget.z - collisionFromPos.z);
    }

    MMCamera_ScaledStepToCeilVec3f(&atTarget, &camera->at, camera->atLERPStepScale, camera->atLERPStepScale, 0.2f);
    return 1;
}

// MM Camera_CalcAtForParallel. MM's slope term is gated on the debug register PREG(76), 0 in both games.
static s32 MMCam_CalcAtForParallel(Camera* camera, MMVecGeo* arg1, f32 yOffset, f32 xzOffsetMax,
                                   f32* focalActorPosY, s16 flags) {
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    f32 fovHeight;
    Vec3f* at = &camera->at;
    f32 deltaY;
    f32 eyeAtDistXZ;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    MMVecGeo focalActorAtOffsetTargetGeo;

    if (flags & 0x40) {
        focalActorAtOffsetTargetGeo.r = MMCam_func_800CCCEC(camera, flags & 0x10);
        focalActorAtOffsetTargetGeo.yaw = focalActorPosRot->rot.y + 0x4000;
        focalActorAtOffsetTargetGeo.pitch = 0;
        focalActorAtOffsetTarget = MMOLib_VecGeoToVec3f(&focalActorAtOffsetTargetGeo);
    } else {
        f32 xOffset = camera->posOffset.x + camera->playerPosDelta.x;
        f32 zOffset = camera->posOffset.z + camera->playerPosDelta.z;

        if (sqrtf(SQ(xOffset) + SQ(zOffset)) < xzOffsetMax) {
            focalActorAtOffsetTarget.x = xOffset;
            focalActorAtOffsetTarget.z = zOffset;
        } else {
            focalActorAtOffsetTarget.x = camera->posOffset.x;
            focalActorAtOffsetTarget.z = camera->posOffset.z;
        }
    }

    focalActorAtOffsetTarget.y = focalActorHeight + yOffset;

    if (PREG(76) && flags) {
        focalActorAtOffsetTarget.y -=
            MMCam_CalcSlopeYAdj(&camera->floorNorm, focalActorPosRot->rot.y, arg1->yaw, 25.0f);
    }

    if (MMCam_func_800CB950(camera)) {
        *focalActorPosY = MMCamera_ScaledStepToCeilF(focalActorPosRot->pos.y, *focalActorPosY, 0.4f, 0.1f);
        deltaY = focalActorPosRot->pos.y - *focalActorPosY;
        focalActorAtOffsetTarget.y -= deltaY;
        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                       camera->yOffsetUpdateRate, 0.1f);
    } else {
        deltaY = focalActorPosRot->pos.y - *focalActorPosY;
        eyeAtDistXZ = OLib_Vec3fDistXZ(at, &camera->eye);

        // Get the height based on 80% of the fov
        fovHeight = Math_FTanF(MM_DEG_TO_RAD(camera->fov * (0.8f * 0.5f))) * eyeAtDistXZ;

        if (deltaY > fovHeight) {
            *focalActorPosY += deltaY - fovHeight;
            deltaY = fovHeight;
        } else if (deltaY < -fovHeight) {
            *focalActorPosY += deltaY + fovHeight;
            deltaY = -fovHeight;
        }

        focalActorAtOffsetTarget.y -= deltaY;
        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, 0.3f, 0.2f, 0.1f);
        camera->xzOffsetUpdateRate = 0.3f;
        camera->yOffsetUpdateRate = 0.2f;
    }

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;
    MMCamera_ScaledStepToCeilVec3f(&atTarget, at, camera->atLERPStepScale, camera->atLERPStepScale, 0.2f);
    return 1;
}

// MM Camera_ClampDist1
static f32 MMCam_ClampDist1(Camera* camera, f32 dist, f32 minDist, f32 maxDist, s16 timer) {
    f32 distTarget;

    if ((dist / maxDist) > 1.2f) {
        distTarget = maxDist;
        camera->rUpdateRateInv = 20.0f / (dist / maxDist);
        if ((20.0f / (dist / maxDist)) < 10) {
            camera->rUpdateRateInv = 10;
        }
    } else if (dist < minDist) {
        distTarget = minDist;
        camera->rUpdateRateInv =
            MMCamera_ScaledStepToCeilF((timer != 0) ? 10.0f : 20.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    } else if (maxDist < dist) {
        distTarget = maxDist;
        camera->rUpdateRateInv =
            MMCamera_ScaledStepToCeilF((timer != 0) ? 10.0f : 20.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    } else {
        distTarget = dist;
        camera->rUpdateRateInv =
            MMCamera_ScaledStepToCeilF((timer != 0) ? 20.0f : 1.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    }

    return MMCamera_ScaledStepToCeilF(distTarget, camera->dist, 1.0f / camera->rUpdateRateInv, 0.1f);
}

// MM Camera_ClampDist2
static f32 MMCam_ClampDist2(Camera* camera, f32 dist, f32 minDist, f32 maxDist, s16 timer) {
    f32 distTarget;

    if (timer == 0) {
        distTarget = ((maxDist * 0.25f) > 80.0f) ? maxDist * 0.25f : 80.0f;
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(1000.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    } else if ((dist / maxDist) > 1.2f) {
        distTarget = maxDist;
        camera->rUpdateRateInv = 20.0f / (dist / maxDist);
        if ((20.0f / (dist / maxDist)) < 10.0f) {
            camera->rUpdateRateInv = 10.0f;
        }
    } else if (dist < minDist) {
        distTarget = minDist;
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    } else if (maxDist < dist) {
        distTarget = maxDist;
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    } else {
        distTarget = dist;
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(1.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
    }

    return MMCamera_ScaledStepToCeilF(distTarget, camera->dist, 1.0f / camera->rUpdateRateInv, 0.1f);
}

// MM Camera_CalcDefaultPitch
static s16 MMCam_CalcDefaultPitch(Camera* camera, s16 pitch, s16 flatSurfacePitchTarget, s16 slopePitchAdj) {
    f32 attenuation;
    f32 pitchStepScale;
    f32 t;
    s16 slopePitchAdjAttenuated;
    s16 pitchMag = ABS(pitch);
    s16 pitchTarget;

    slopePitchAdjAttenuated =
        (slopePitchAdj > 0) ? MM_TRUNCF_BINANG(Math_CosS(slopePitchAdj) * slopePitchAdj) : slopePitchAdj;
    pitchTarget = flatSurfacePitchTarget - slopePitchAdjAttenuated;

    if (ABS(pitchTarget) < pitchMag) {
        pitchStepScale = (1.0f / camera->pitchUpdateRateInv) * 3.0f;
    } else {
        t = pitchMag * (1.0f / MM_DEG_TO_BINANG(79.655f));
        attenuation = MMCam_QuadraticAttenuation(0.8f, 1.0f - t);
        pitchStepScale = (1.0f / camera->pitchUpdateRateInv) * attenuation;
    }

    return MMCamera_ScaledStepToCeilS(pitchTarget, pitch, pitchStepScale, 5);
}

// MM Camera_CalcDefaultYaw
static s16 MMCam_CalcDefaultYaw(Camera* camera, s16 yaw, s16 target, f32 attenuationYawDiffRange,
                                f32 attenuationYawDiffInterpParam) {
    f32 attenuationYawDiffAdj;
    f32 attenuationYawDiff;
    f32 attenuationYawDiffParam;
    f32 attenuationSpeedRatio;
    f32 yawUpdRate;
    s16 yawDiffToTarget;

    if (camera->xzSpeed > 0.001f) {
        yawDiffToTarget = target - MM_BINANG_ROT180(yaw);
        attenuationYawDiffParam = MM_BINANG_ROT180(yawDiffToTarget) / (f32)0x8000;
    } else {
        yawDiffToTarget = target - MM_BINANG_ROT180(yaw);
        attenuationYawDiffParam = 0.3f;
    }

    attenuationYawDiff = MMCam_QuadraticAttenuation(attenuationYawDiffRange, attenuationYawDiffParam);
    attenuationYawDiffAdj = MM_LERPIMP(attenuationYawDiff, 1.0f, attenuationYawDiffInterpParam);
    attenuationYawDiffAdj = CLAMP_MIN(attenuationYawDiffAdj, 0.0f);

    attenuationSpeedRatio = MMCam_QuadraticAttenuation(0.5f, camera->speedRatio);

    yawUpdRate = 1.0f / camera->yawUpdateRateInv;
    return yaw + MM_TRUNCF_BINANG(yawDiffToTarget * attenuationYawDiffAdj * attenuationSpeedRatio * yawUpdRate);
}

// MM Camera_CalcDefaultSwing
static void MMCam_CalcDefaultSwing(Camera* camera, MMVecGeo* arg1, MMVecGeo* arg2, f32 arg3, f32 arg4,
                                   MMSwingAnimation* swing, s16* flags) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f peekAroundPoint;
    s32 sp8C = 0;
    f32 sp88;
    s32 checkEyeBit1;
    s32 checkEyeBit2;
    MMCamCollision sp58;
    MMVecGeo sp50;
    Vec3f* eyeNext = &camera->eyeNext;

    if (swing->unk_64 == 1) {
        // (MM Math3D_PlaneF(n, dist, p) = n.p + dist)
        if (arg3 < (sp88 = OLib_Vec3fDist(at, &swing->collisionClosePoint))) {
            swing->unk_64 = 0;
        } else if ((sp88 = (swing->eyeAtColChk.norm.x * at->x) + (swing->eyeAtColChk.norm.y * at->y) +
                           (swing->eyeAtColChk.norm.z * at->z) + swing->eyeAtColChk.poly->dist) > 0.0f) {
            swing->unk_64 = 0;
        } else if ((sp88 = OLib_Vec3fDist(eye, &swing->eyeAtColChk.pos)) < 10.0f) {
            swing->unk_64 = 0;
        } else if ((sp88 = (swing->atEyeColChk.norm.x * eye->x) + (swing->atEyeColChk.norm.y * eye->y) +
                           (swing->atEyeColChk.norm.z * eye->z) + swing->atEyeColChk.poly->dist) > 0.0f) {
            swing->unk_64 = 0;
        } else if (swing->atEyeColChk.norm.y > 0.50f) {
            swing->unk_64 = 0;
        } else {
            Math3D_CosOut(&camera->playerPosDelta, &swing->eyeAtColChk.norm, &sp88);
            if (sp88 > 0.0f) {
                swing->unk_64 = 0;
            }
        }

        if (swing->unk_64 == 1) {
            sp8C = 2;
        }
    } else if (swing->unk_64 == 2) {
        swing->unk_64 = 0;
    }

    if (sp8C == 0) {
        checkEyeBit1 = (*flags & 2) ? (1 << 1) : 0;
        checkEyeBit2 = (swing->unk_64 != 1) ? (1 << 0) : 0;
        sp8C = MMCam_func_800CD44C(camera, arg1, &swing->atEyeColChk, &swing->eyeAtColChk,
                                   ((s16)checkEyeBit2 | checkEyeBit1));
    }

    switch (sp8C) {
        case 1:
            swing->collisionClosePoint =
                MMCam_BgCheckCorner(&camera->at, &camera->eyeNext, &swing->atEyeColChk, &swing->eyeAtColChk);
            // fallthrough
        case 2:
            peekAroundPoint.x = swing->collisionClosePoint.x + (swing->atEyeColChk.norm.x + swing->eyeAtColChk.norm.x);
            peekAroundPoint.y = swing->collisionClosePoint.y + (swing->atEyeColChk.norm.y + swing->eyeAtColChk.norm.y);
            peekAroundPoint.z = swing->collisionClosePoint.z + (swing->atEyeColChk.norm.z + swing->eyeAtColChk.norm.z);

            sp50 = MMOLib_Vec3fDiffToVecGeo(at, &peekAroundPoint);
            sp50.r = arg1->r;
            swing->unk_64 = 1;
            swing->swingUpdateRate = 1.5f;
            sp58.pos = MMOLib_AddVecGeoToVec3f(at, &sp50);

            if (MMCam_func_800CBC84(camera, &swing->eyeAtColChk.pos, &sp58, 0) == 0) {
                sp50.yaw =
                    MMCam_AngleDiffAndScale(arg1->yaw, arg2->yaw, (camera->speedRatio * 0.5f) + 0.5f) + arg2->yaw;
                sp50.pitch = MMCam_AngleDiffAndScale(arg1->pitch, arg2->pitch, (camera->speedRatio * 0.5f) + 0.5f) +
                             arg2->pitch;
                if (swing->atEyeColChk.geoNorm.pitch < 0x2AA8) { // 60 degrees
                    swing->yaw = sp50.yaw;
                    swing->pitch = sp50.pitch;
                } else {
                    swing->yaw = arg1->yaw;
                    swing->pitch = arg1->pitch;
                }
            }
            *eye = *eyeNext = sp58.pos;
            break;

        case 3:
        case 6:
            eyeNext = &swing->atEyeColChk.pos;
            sp88 = OLib_Vec3fDist(at, eyeNext);
            if (sp88 < 60.0f) {
                sp50.yaw = MMCam_AngleDiffAndScale(arg1->yaw, arg2->yaw, camera->speedRatio) + arg2->yaw;
                if (*flags & 0x1000) {
                    sp50.pitch = arg2->pitch;
                } else {
                    sp50.pitch = Math_CosS(swing->atEyeColChk.geoNorm.pitch) * 0x3FFC;
                }
                sp50.r = 60.0f - sp88;
                sp58.pos = MMOLib_AddVecGeoToVec3f(eyeNext, &sp50);
            } else {
                sp50.yaw = MMCam_AngleDiffAndScale(arg1->yaw, arg2->yaw, camera->speedRatio) + arg2->yaw;
                sp50.pitch = MMCam_AngleDiffAndScale(arg1->pitch, arg2->pitch, camera->speedRatio) + arg2->pitch;
                sp50.r = arg1->r;
                sp58.pos = MMOLib_AddVecGeoToVec3f(at, &sp50);
            }
            MMCam_func_800CBC84(camera, at, &sp58, 0);
            *eye = sp58.pos;
            break;

        default:
            eyeNext = &swing->atEyeColChk.pos;
            *flags &= ~0x1000;
            swing->swingUpdateRate = arg4;
            *eye = *eyeNext;
            break;
    }
}

/*--------------------------------------------------------------------------------------------------
 * Settings: which MM setting's mode table applies to the OoT setting in use
 *------------------------------------------------------------------------------------------------*/

// OoT setting -> MM setting whose form-mode entries apply there. Settings both games inherit map to
// themselves (NORMAL0/1, DUNGEON0/1, NORMAL3); OoT-only settings map to the MM setting of the same kind:
//   - free-roaming settings (NORMAL2/4, DUNGEON2, TOWER_*, BOSS_*, BIG_OCTO, JABU_TENTACLE, BEAN_*,
//     FISHING, DIRECTED_YAW) and the birds-eye views (FOREST/MEADOW/FIRE_BIRDS_EYE, MEADOW_UNUSED)
//     -> MM NORMAL1's table, which MM's own free-roaming, boss and BIRDS_EYE_VIEW settings all share;
//   - OoT's FIXED0 (MARKET_BALCONY), CIRCLE0 (PIVOT_CRAWLSPACE) and CIRCLE3 (PIVOT_IN_FRONT) -> MM's
//     FIXED0/CIRCLE0/CIRCLE3, where only the Subject1 aiming modes (DEKUSHOOT, ZORAFIN) are valid;
//   - everything else (doors, crawlspaces, pre-rendered, cutscene/demo, FIXED1, CIRCLE2/4/5/7, ITEM*,
//     START*, FREE*, HORSE, PIVOT_VERTICAL, SCENE*, ...) -> MM's validModes-0x1 settings: none of
//     the form modes is valid, so requesting one forces CAM_MODE_NORMAL as MM does;
//   - PIVOT_WATER_SURFACE entered by a diving Zora is MM's ZORA_DIVING (no form modes valid).
static const MMCamSetting* MMCam_GetSetting(Camera* camera) {
    if (sMMCam.zoraDiving && (camera->setting == CAM_SET_PIVOT_WATER_SURFACE)) {
        return &sMMCamSettingNone; // MM CAM_SET_ZORA_DIVING
    }

    switch (camera->setting) {
        case CAM_SET_NORMAL0:
            return &sMMCamSettingNormal0;

        case CAM_SET_DUNGEON0:
            return &sMMCamSettingDungeon0;

        case CAM_SET_NORMAL3:
            return &sMMCamSettingNormal3;

        case CAM_SET_NORMAL1:
        case CAM_SET_DUNGEON1:
        case CAM_SET_BOSS_GOHMA:
        case CAM_SET_BOSS_DODONGO:
        case CAM_SET_BOSS_BARINADE:
        case CAM_SET_BOSS_PHANTOM_GANON:
        case CAM_SET_BOSS_VOLVAGIA:
        case CAM_SET_BOSS_BONGO:
        case CAM_SET_BOSS_MORPHA:
        case CAM_SET_BOSS_TWINROVA_PLATFORM:
        case CAM_SET_BOSS_TWINROVA_FLOOR:
        case CAM_SET_BOSS_GANONDORF:
        case CAM_SET_BOSS_GANON:
        case CAM_SET_TOWER_CLIMB:
        case CAM_SET_TOWER_UNUSED:
        case CAM_SET_FOREST_BIRDS_EYE:
        case CAM_SET_BEAN_GENERIC:
        case CAM_SET_BEAN_LOST_WOODS:
        case CAM_SET_BIG_OCTO:
        case CAM_SET_MEADOW_BIRDS_EYE:
        case CAM_SET_MEADOW_UNUSED:
        case CAM_SET_FIRE_BIRDS_EYE:
        case CAM_SET_NORMAL2:
        case CAM_SET_FISHING:
        case CAM_SET_JABU_TENTACLE:
        case CAM_SET_DUNGEON2:
        case CAM_SET_DIRECTED_YAW:
        case CAM_SET_NORMAL4:
            return &sMMCamSettingNormal1;

        case CAM_SET_MARKET_BALCONY:
        case CAM_SET_PIVOT_CRAWLSPACE:
        case CAM_SET_PIVOT_IN_FRONT:
            return &sMMCamSettingFixed0;

        default:
            return &sMMCamSettingNone;
    }
}

// The MM setting of the same name as OoT's current setting, with all its modes (MM's cameraModes table), for
// the settings MM inherited from OoT and retuned: NORMAL0..4 and DUNGEON0..2. An MM form runs MM's camera
// function with MM's data there, in every mode whose MM function is ported. OoT-only settings (boss rooms,
// the birds-eye views, towers, beanstalks, ...) have no MM counterpart and keep OoT's camera for OoT's modes:
// their data is tuned to those rooms (e.g. the top-down Forest Temple ceiling room).
static const MMCamModeData* MMCam_GetSharedSettingModes(Camera* camera) {
    switch (camera->setting) {
        case CAM_SET_NORMAL0:
            return sMMCamDataNormal0;
        case CAM_SET_NORMAL1:
            return sMMCamDataNormal1;
        case CAM_SET_NORMAL2:
            return sMMCamDataNormal2;
        case CAM_SET_NORMAL3:
            return sMMCamDataNormal3;
        case CAM_SET_NORMAL4:
            return sMMCamDataNormal4;
        case CAM_SET_DUNGEON0:
            return sMMCamDataDungeon0;
        case CAM_SET_DUNGEON1:
            return sMMCamDataDungeon1;
        case CAM_SET_DUNGEON2:
            return sMMCamDataDungeon2;
        default:
            return NULL;
    }
}

// MM sCameraSettings[camera->setting].cameraModes[mode].values
static const s16* MMCam_GetModeValues(Camera* camera, s16 mode) {
    const MMCamModeData* shared;

    if (sMMCam.zoraDiving && (camera->setting == CAM_SET_PIVOT_WATER_SURFACE)) {
        return sMMSetZoraDivingModeNormalData; // MM sCamSetZoraDivingModes[CAM_MODE_NORMAL]
    }
    if ((mode < 0) || (mode >= MM_CAM_MODE_TABLE_SIZE)) {
        return NULL;
    }
    // MergeShip2: MM CAM_SET_GIANT while the Giant's Mask is worn (MMCam_GetSharedRunMode).
    if (MergeShip_CameraMM_RequestGiantsMaskSetting(camera) && (MMCam_GetSetting(camera) != &sMMCamSettingNone)) {
        return sMMCamDataGiant[mode].values;
    }
    if (MMCam_GetSetting(camera)->modes[mode].values != NULL) {
        return MMCam_GetSetting(camera)->modes[mode].values;
    }
    shared = MMCam_GetSharedSettingModes(camera);
    return (shared != NULL) ? shared[mode].values : NULL;
}

// OoT CameraModeType -> the same mode in MM's numbering.
static const s8 sMMCamModeForOoTMode[] = {
    MMCAM_MODE_NORMAL,          // CAM_MODE_NORMAL
    MMCAM_MODE_TARGET,          // CAM_MODE_TARGET ("PARALLEL")
    MMCAM_MODE_FOLLOWTARGET,    // CAM_MODE_FOLLOWTARGET ("KEEPON")
    MMCAM_MODE_TALK,            // CAM_MODE_TALK
    MMCAM_MODE_BATTLE,          // CAM_MODE_BATTLE
    MMCAM_MODE_CLIMB,           // CAM_MODE_CLIMB
    MMCAM_MODE_FIRSTPERSON,     // CAM_MODE_FIRST_PERSON ("SUBJECT")
    MMCAM_MODE_BOWARROW,        // CAM_MODE_AIM_ADULT ("BOWARROW")
    MMCAM_MODE_BOWARROWZ,       // CAM_MODE_Z_AIM ("BOWARROWZ")
    MMCAM_MODE_HOOKSHOT,        // CAM_MODE_HOOKSHOT
    MMCAM_MODE_BOOMERANG,       // CAM_MODE_AIM_BOOMERANG
    MMCAM_MODE_SLINGSHOT,       // CAM_MODE_AIM_CHILD ("PACHINCO")
    MMCAM_MODE_CLIMBZ,          // CAM_MODE_CLIMBZ
    MMCAM_MODE_JUMP,            // CAM_MODE_JUMP
    MMCAM_MODE_HANG,            // CAM_MODE_HANG
    MMCAM_MODE_HANGZ,           // CAM_MODE_HANGZ
    MMCAM_MODE_FREEFALL,        // CAM_MODE_FREEFALL
    MMCAM_MODE_CHARGE,          // CAM_MODE_CHARGE
    MMCAM_MODE_STILL,           // CAM_MODE_STILL
    MMCAM_MODE_PUSHPULL,        // CAM_MODE_PUSHPULL
    MMCAM_MODE_FOLLOWBOOMERANG, // CAM_MODE_FOLLOWBOOMERANG ("BOOKEEPON")
};

static s32 MMCam_IsFuncPorted(s16 funcId) {
    switch (funcId) {
        case MMCAM_FUNC_NORMAL1:
        case MMCAM_FUNC_NORMAL3:
        case MMCAM_FUNC_PARALLEL1:
        case MMCAM_FUNC_JUMP2:
        case MMCAM_FUNC_JUMP3:
        case MMCAM_FUNC_SUBJECT1:
        case MMCAM_FUNC_BATTLE1:
        case MMCAM_FUNC_KEEPON1:
        case MMCAM_FUNC_KEEPON3:
            return true;
        default:
            return false;
    }
}

// For an MM form on the main camera in one of OoT's modes: the MM mode whose MM function (and MM data) runs
// in place of OoT's, or -1 to keep OoT's function (Link, an OoT-only setting, an MM function not ported, or
// SoH's free-look camera taking over).
static s16 MMCam_GetSharedRunMode(Camera* camera, const MMCamModeData** modeData) {
    static s32 sTestOoTCamera = -1;
    const MMCamModeData* shared;
    s16 mmMode;
    // MergeShip2: MM CAM_SET_GIANT -- while the Giant's Mask is worn, MM's main camera takes that setting in any
    // free-roaming setting (not a door's or a cutscene's), and runs MM's functions with its data.
    s32 giant = MergeShip_CameraMM_RequestGiantsMaskSetting(camera) && (MMCam_GetSetting(camera) != &sMMCamSettingNone);

    // Harness A/B switch: MERGESHIP_TEST_OOT_FORM_CAMERA set in the environment keeps OoT's functions for the
    // forms, so one build can record the camera before and after (never set outside the test harness).
    if (sTestOoTCamera < 0) {
        sTestOoTCamera = (getenv("MERGESHIP_TEST_OOT_FORM_CAMERA") != NULL);
    }
    if (sTestOoTCamera) {
        return -1;
    }

    if ((!giant && !MERGESHIP_FORM_IS_MM(camera->player->mergeShipForm)) || (camera->mode < 0) ||
        (camera->mode >= ARRAY_COUNT(sMMCamModeForOoTMode))) {
        return -1;
    }
    shared = giant ? sMMCamDataGiant : MMCam_GetSharedSettingModes(camera);
    if (shared == NULL) {
        return -1;
    }
    mmMode = sMMCamModeForOoTMode[camera->mode];
    if ((shared[mmMode].values == NULL) || !MMCam_IsFuncPorted(shared[mmMode].funcId)) {
        return -1;
    }
    // SoH's free-look camera replaces OoT's functions while the right stick has moved it (Camera_Free).
    if (CVarGetInteger(CVAR_SETTING("FreeLook.Enabled"), 0) && (SetCameraManual(camera) == 1)) {
        return -1;
    }
    *modeData = &shared[mmMode];
    return mmMode;
}

/*--------------------------------------------------------------------------------------------------
 * MM camera update functions
 *------------------------------------------------------------------------------------------------*/

// MM Camera_Normal1 (GORONDASH, DEKUFLY, GORONJUMP)
s32 Camera_MMNormal1(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f spD8;
    f32 spD4;
    f32 spD0;
    f32 spC8;
    f32 spC4;
    f32 spC0;
    f32 phi_f0_4;
    MMVecGeo spB4;
    MMVecGeo spAC;
    MMVecGeo spA4;
    MMVecGeo sp9C;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMNormal1Data* data = &sMMCam.params.norm1;
    s16 phi_v1_2;
    s16 temp_a0_3;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);
    f32 phi_f2;
    f32 rand;

    data->ro.unk_00 =
        MM_GET_NEXT_RO_DATA(values) * (focalActorHeight * 0.01f * (0.8f - ((68.0f / focalActorHeight) * -0.2f)));
    data->ro.unk_04 =
        MM_GET_NEXT_RO_DATA(values) * (focalActorHeight * 0.01f * (0.8f - ((68.0f / focalActorHeight) * -0.2f)));
    data->ro.unk_08 =
        MM_GET_NEXT_RO_DATA(values) * (focalActorHeight * 0.01f * (0.8f - ((68.0f / focalActorHeight) * -0.2f)));
    data->ro.unk_04 = data->ro.unk_08 - (data->ro.unk_08 - data->ro.unk_04);

    if (MMCamera_ReloadParams(camera)) {
        data->ro.unk_20 = MM_CAM_DEG_TO_BINANG(MM_GET_NEXT_RO_DATA(values));
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_0C = 40.0f - (40.0f - data->ro.unk_0C);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->ro.unk_14 = 1.0f - (1.0f - data->ro.unk_14);
        data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_1C = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
    }

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    spA4 = MMOLib_Vec3fDiffToVecGeo(at, eye);
    sp9C = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    switch (camera->animState) {
        case 20:
            MMCam_SetUpdateRatesFastYaw(camera);
            // fallthrough
        case 0:
            data->rw.unk_0C = 1;
            if (!(data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) && (camera->animState != 20)) {
                data->rw.unk_0C |= 0x1000;
            }
            // fallthrough
        case 10:
            if (camera->animState == 10) {
                data->rw.unk_0C = 0;
            }
            data->rw.unk_08 = 0;
            sMMCam.swing.yaw = sMMCam.swing.pitch = sMMCam.swing.unk_64 = 0;
            data->rw.unk_0A = 0x514;
            sMMCam.swing.swingUpdateRate = data->ro.unk_0C;
            data->rw.unk_00 = focalActorPosRot->pos.y;
            data->rw.unk_04 = camera->xzSpeed;
            sMMCam.swing.timer = 0;
            sUpdateCameraDirection = false;
            data->rw.unk_10 = 120.0f;
            break;

        default:
            break;
    }

    camera->animState = 1;
    sUpdateCameraDirection = true;

    if ((camera->speedRatio < 0.01f) || (data->rw.unk_0A > 0x4B0)) {
        if (data->rw.unk_0A > -0x4B0) {
            data->rw.unk_0A--;
        }
    } else {
        data->rw.unk_0A = 0x4B0;
    }

    if (MMCam_func_800CB950(camera)) {
        data->rw.unk_00 = focalActorPosRot->pos.y;
    }

    if (data->rw.unk_0C & 0x1000) {
        spC8 = camera->speedRatio;
    } else {
        spC8 = ((camera->speedRatio * 3.0f) + 1.0f) * 0.25f;
    }

    spD8 = camera->posOffset;
    spD8.y -= focalActorHeight + data->ro.unk_00;
    spC4 = MMCam_Vec3fMagnitude(&spD8);

    if (spC4 > (data->ro.unk_04 + data->ro.unk_08)) {
        spC4 = 1.0f;
    } else {
        spC4 /= data->ro.unk_04 + data->ro.unk_08;
    }

    spD0 = 0.2f;

    phi_f0_4 = (camera->xzSpeed - data->rw.unk_04) * (0.2f * 1.0f);
    if (phi_f0_4 < 0.0f) {
        phi_f0_4 = 0.0f;
    }

    spC0 = OLib_ClampMaxDist(SQ(phi_f0_4), 1.0f);

    camera->yOffsetUpdateRate =
        MMCamera_ScaledStepToCeilF(0.05f, camera->yOffsetUpdateRate, (0.5f * spC8) + (0.5f * spC4), 0.0001f);
    camera->xzOffsetUpdateRate =
        MMCamera_ScaledStepToCeilF(0.05f, camera->xzOffsetUpdateRate, (0.5f * spC8) + (0.5f * spC4), 0.0001f);
    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(0.05f, camera->fovUpdateRate, (0.5f * spC8) + (0.5f * spC4), 0.0001f);

    if (sMMCam.swing.unk_64 == 1) {
        phi_f2 = 0.5f;
    } else {
        phi_f2 = (0.5f * spC8) + (0.5f * spC4);
    }

    data->rw.unk_04 = camera->xzSpeed;

    if (sMMCam.swing.timer != 0) {
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(
            sMMCam.swing.swingUpdateRate + (sMMCam.swing.timer * 2), camera->yawUpdateRateInv, phi_f2, 0.1f);
        if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) {
            camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(100.0f, camera->pitchUpdateRateInv, 0.5f, 0.1f);
        } else {
            camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF((sMMCam.swing.timer * 2) + 16.0f,
                                                                    camera->pitchUpdateRateInv, 0.2f, 0.1f);
        }
        sMMCam.swing.timer--;
    } else {
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(
            sMMCam.swing.swingUpdateRate - (sMMCam.swing.swingUpdateRate * 0.7f * spC0), camera->yawUpdateRateInv,
            phi_f2, 0.1f);
        if ((data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) && (camera->speedRatio > 0.01f)) {
            camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(100.0f, camera->pitchUpdateRateInv, 0.5f, 0.1f);
        } else {
            // (MM's gCameraDriftActor branch: that global is only set by MM actors, NULL in Hyrule.)
            camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(16.0f, camera->pitchUpdateRateInv, 0.2f, 0.1f);
        }
    }

    if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_0) {
        temp_a0_3 = MMCam_GetPitchAdjFromFloorHeightDiffs(camera, spA4.yaw + 0x8000, data->rw.unk_0C & 1);
        phi_f2 = (1.0f / data->ro.unk_10) * 0.7f;
        spD0 = (1.0f / data->ro.unk_10) * 0.3f * (1.0f - camera->speedRatio);
        data->rw.unk_08 = MMCamera_ScaledStepToCeilS(temp_a0_3, data->rw.unk_08, phi_f2 + spD0, 5);
    } else {
        data->rw.unk_08 = 0;
    }

    if ((sMMCam.swing.unk_64 == 1) && (data->ro.unk_00 > -40.0f)) {
        spD0 = Math_SinS(sMMCam.swing.pitch);
        phi_f2 = (-40.0f * spD0) + data->ro.unk_00 * (1.0f - spD0);
        camera->yawUpdateRateInv = 80.0f;
        camera->pitchUpdateRateInv = 80.0f;
    } else {
        phi_f2 = data->ro.unk_00;
    }

    if (data->ro.interfaceFlags & (MM_NORMAL1_FLAG_6 | MM_NORMAL1_FLAG_5)) {
        if (camera->dist < data->ro.unk_04) {
            spD0 = 0.0f;
        } else if (data->ro.unk_08 < camera->dist) {
            spD0 = 1.0f;
        } else if (data->ro.unk_08 == data->ro.unk_04) {
            spD0 = 1.0f;
        } else {
            spD0 = (camera->dist - data->ro.unk_04) / (data->ro.unk_08 - data->ro.unk_04);
        }

        MMCam_CalcAtForNormal1(camera, &sp9C, phi_f2, 25.0f * spD0 * camera->speedRatio);
        data->rw.unk_10 = 120.0f;
    } else if ((data->ro.interfaceFlags & MM_NORMAL1_FLAG_7) && (data->rw.unk_0A < 0)) {
        phi_f0_4 = data->rw.unk_0A / -1200.0f;
        MMCam_CalcAtForNormal1(
            camera, &sp9C,
            phi_f2 - ((phi_f2 - ((0.8f - ((68.0f / focalActorHeight) * -0.2f)) * focalActorHeight * -0.45f)) *
                      phi_f0_4 * 0.75f),
            10.0f * phi_f0_4);
        data->rw.unk_10 = 120.0f;
    } else if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) {
        MMCam_CalcAtForScreen(camera, &sp9C, data->ro.unk_00, &data->rw.unk_00, data->rw.unk_10);
        if (data->rw.unk_10 > 20.0f) {
            data->rw.unk_10 -= 0.2f;
        }
    } else {
        MMCam_CalcAtDefault(camera, &sp9C, phi_f2, data->ro.interfaceFlags & MM_NORMAL1_FLAG_0);
        data->rw.unk_10 = 120.0f;
    }

    spB4 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    if ((data->ro.interfaceFlags & MM_NORMAL1_FLAG_7) && (data->rw.unk_0A < 0)) {
        switch (camera->player->mergeShipForm) {
            case PLAYER_FORM_ADULT: // MM PLAYER_FORM_HUMAN
            case PLAYER_FORM_CHILD:
                spD0 = 66.0f;
                break;

            case PLAYER_FORM_DEKU:
                spD0 = 66.0f;
                break;

            case PLAYER_FORM_GORON:
                spD0 = 115.0f;
                break;

            case PLAYER_FORM_ZORA:
                spD0 = 115.0f;
                break;

            case PLAYER_FORM_FIERCE_DEITY:
                spD0 = data->ro.unk_04;
                break;

            default:
                spD0 = data->ro.unk_04;
                break;
        }
        phi_f0_4 = MMCam_ClampDist2(camera, spB4.r, spD0, spD0, 0);
    } else if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_7) {
        phi_f0_4 = MMCam_ClampDist2(camera, spB4.r, data->ro.unk_04, data->ro.unk_08, 1);
    } else {
        phi_f0_4 = MMCam_ClampDist1(camera, spB4.r, data->ro.unk_04, data->ro.unk_08, data->rw.unk_0A > 0);
    }

    camera->dist = spB4.r = phi_f0_4;

    if (sMMCam.swing.unk_64 != 0) {
        spB4.pitch =
            MMCamera_ScaledStepToCeilS(sMMCam.swing.pitch, sp9C.pitch, 1.0f / camera->yawUpdateRateInv, 5);
        spB4.yaw = MMCamera_ScaledStepToCeilS(sMMCam.swing.yaw, sp9C.yaw, 1.0f / camera->yawUpdateRateInv, 5);
    } else if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_5) {
        spB4.yaw = sp9C.yaw;
        spB4.pitch = sp9C.pitch;
        camera->animState = 20;
    } else if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_1) {
        // (MM's gCameraDriftActor branch precedes this one; that global is NULL in Hyrule.)
        MMVecGeo sp64;

        if ((camera->speedRatio > 0.1f) || (data->rw.unk_0A > 0x4B0)) {
            sp64 = MMOLib_Vec3fToVecGeo(&camera->playerPosDelta);
            if (!(data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) || !MMCam_func_800CB924(camera)) {
                spB4.yaw = MMCam_CalcDefaultYaw(camera, sp9C.yaw, sp64.yaw, data->ro.unk_14, spC0);
            }
            if (!(data->ro.interfaceFlags & MM_NORMAL1_FLAG_3)) {
                spB4.pitch = MMCam_CalcDefaultPitch(camera, sp9C.pitch, data->ro.unk_20, data->rw.unk_08);
            } else if ((camera->playerPosDelta.y > 0.0f) && MMCam_func_800CB924(camera)) {
                spB4.pitch = MMCam_CalcDefaultPitch(camera, sp9C.pitch, data->ro.unk_20, data->rw.unk_08);
            }
        } else {
            spB4.yaw = sp9C.yaw;
            spB4.pitch = sp9C.pitch;
        }
    } else {
        spB4.yaw = MMCam_CalcDefaultYaw(camera, sp9C.yaw, focalActorPosRot->rot.y, data->ro.unk_14, spC0);
        if (!(data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) || (camera->speedRatio < 0.1f)) {
            spB4.pitch = MMCam_CalcDefaultPitch(camera, sp9C.pitch, data->ro.unk_20, data->rw.unk_08);
        }
    }

    // 76.9 degrees
    if (spB4.pitch > 0x36B0) {
        spB4.pitch = 0x36B0;
    }
    // -76.9 degrees
    if (spB4.pitch < -0x36B0) {
        spB4.pitch = -0x36B0;
    }

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &spB4);

    if ((camera->status == CAM_STAT_ACTIVE) && !(data->ro.interfaceFlags & MM_NORMAL1_FLAG_4) && (spC4 <= 0.9f)) {
        if (!MMCam_func_800CBA7C(camera)) {
            CollisionPoly* sp60;
            s32 sp5C;
            f32 sp58;

            MMCam_CalcDefaultSwing(camera, &spB4, &sp9C, data->ro.unk_04, data->ro.unk_0C, &sMMCam.swing,
                                   &data->rw.unk_0C);
            sp58 = BgCheck_CameraRaycastFloor2(&camera->play->colCtx, &sp60, &sp5C, eye);
            if ((data->ro.interfaceFlags & MM_NORMAL1_FLAG_3) && MMCam_func_800CB924(camera)) {
                spD0 = 25.0f;
            } else {
                spD0 = 5.0f;
            }

            phi_f2 = eye->y - sp58;
            if ((sp58 != BGCHECK_Y_MIN) && (phi_f2 < spD0)) {
                eye->y = sp58 + spD0;
            } else if ((camera->waterYPos != camera->playerGroundY) && ((eye->y - camera->waterYPos) < 5.0f) &&
                       ((eye->y - camera->waterYPos) > -5.0f)) {
                eye->y = camera->waterYPos + 5.0f;
            }
        }

        spAC = MMOLib_Vec3fDiffToVecGeo(eye, at);
        camera->inputDir.x = spAC.pitch;
        camera->inputDir.y = spAC.yaw;
        camera->inputDir.z = 0;

        // crit wiggle
        if (gSaveContext.health <= 0x10) {
            phi_v1_2 = ((s32)(camera->play->state.frames << 0x18) >> 0x15) & 0xFD68;
            camera->inputDir.y += phi_v1_2;
        }
    } else {
        sMMCam.swing.swingUpdateRate = data->ro.unk_0C;
        sMMCam.swing.unk_64 = 0;
        sUpdateCameraDirection = false;
        *eye = *eyeNext;
    }

    phi_f2 = (gSaveContext.health <= 0x10) ? 0.8f : 1.0f;
    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_18 * phi_f2, camera->fov, camera->fovUpdateRate, 0.1f);

    if (data->ro.interfaceFlags & MM_NORMAL1_FLAG_2) {
        spD4 = Math_SinS((s16)(spA4.yaw - spB4.yaw));
        rand = Rand_ZeroOne() - 0.5f;
        camera->roll = MMCamera_ScaledStepToCeilS(
            (rand * 500.0f * camera->speedRatio) + (spD4 * spD4 * spD4 * 10000.0f), camera->roll, 0.1f, 5);
    } else {
        if (gSaveContext.health <= 0x10) {
            rand = Rand_ZeroOne() - 0.5f;
            phi_v1_2 = rand * 100.0f * camera->speedRatio;
        } else {
            phi_v1_2 = 0.0f;
        }
        camera->roll = MMCamera_ScaledStepToCeilS(phi_v1_2, camera->roll, 0.2f, 5);
    }

    camera->atLERPStepScale = MMCam_ClampLerpScale(camera, data->ro.unk_1C);
    data->rw.unk_0C &= ~1;

    return true;
}

#define MM_NORMAL3_RW_FLAG (1 << 0)

// MM Camera_Normal3, run for MM's CAM_SET_ZORA_DIVING (a Zora diving/swimming underwater).
s32 Camera_MMNormal3(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    f32 sp94;
    f32 sp90;
    f32 temp_f2;
    f32 sp88;
    MMVecGeo sp80;
    MMVecGeo sp78;
    MMVecGeo sp70;
    MMVecGeo sp68;
    f32 phi_f2;
    s16 sp62;
    s16 phi_v1_2;
    Player* player = camera->player;
    Vec3f* eyeNext = &camera->eyeNext;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMNormal3Data* data = &sMMCam.params.norm3;

    temp_f2 = MMCam_GetFocalActorHeight(camera);

    // (MM returns to the previous setting here when CAM_SET_HORSE has no horse; this is never HORSE.)

    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, 0);

        data->ro.yOffset = MM_GET_NEXT_RO_DATA(values) * (temp_f2 * 0.01f);
        data->ro.distMin = MM_GET_NEXT_RO_DATA(values) * (temp_f2 * 0.01f);
        data->ro.distMax = MM_GET_NEXT_RO_DATA(values) * (temp_f2 * 0.01f);
        data->ro.pitchTarget = MM_CAM_DEG_TO_BINANG(MM_GET_NEXT_RO_DATA(values));
        data->ro.yawUpdateRateInv = MM_GET_NEXT_RO_DATA(values);
        data->ro.pitchUpdateRateInv = MM_GET_NEXT_RO_DATA(values);
        data->ro.fovTarget = MM_GET_NEXT_RO_DATA(values);
        data->ro.maxAtLERPScale = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
    }

    sp70 = MMOLib_Vec3fDiffToVecGeo(at, eye);
    sp68 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    sUpdateCameraDirection = true;
    sCameraInterfaceFlags = data->ro.interfaceFlags;

    switch (camera->animState) {
        case 0:
        case 10:
        case 20:
            data->rw.isZero = 0;
            data->rw.curPitch = 0;
            data->rw.yPosOffset = camera->playerGroundY;

            sMMCam.swing.yaw = sMMCam.swing.pitch = sMMCam.swing.unk_64 = 0;
            sMMCam.swing.swingUpdateRate = data->ro.yawUpdateRateInv;
            data->rw.yawUpdateRate =
                MM_BINANG_SUB(MM_BINANG_ROT180(camera->playerPosRot.rot.y), sp70.yaw) * (1.0f / 6.0f);
            data->rw.distTimer = 0;
            data->rw.is1200 = 1200;

            if (data->ro.interfaceFlags & MM_NORMAL3_FLAG_1) {
                data->rw.yawTimer = 6;
                camera->unk_14C |= MM_CAM_STATE_DISABLE_MODE_CHANGE;
            } else {
                data->rw.yawTimer = 0;
            }

            camera->animState = 1;
            sMMCam.swing.timer = 0;
            data->rw.flag = MM_NORMAL3_RW_FLAG;
            break;

        default:
            break;
    }

    if (data->rw.distTimer != 0) {
        data->rw.distTimer--;
    }

    sp90 = ((camera->speedRatio * 3.0f) + 1.0f) * 0.25f * 0.5f;
    sp94 = temp_f2 = camera->speedRatio * 0.2f;

    if (sMMCam.swing.timer != 0) {
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(
            (sMMCam.swing.timer * 2) + data->ro.yawUpdateRateInv, camera->yawUpdateRateInv, sp90, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF((sMMCam.swing.timer * 2) + 16.0f,
                                                                camera->pitchUpdateRateInv, sp94, 0.1f);
        sMMCam.swing.timer--;
    } else {
        camera->yawUpdateRateInv =
            MMCamera_ScaledStepToCeilF(data->ro.yawUpdateRateInv, camera->yawUpdateRateInv, sp90, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(16.0f, camera->pitchUpdateRateInv, sp94, 0.1f);
    }

    camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->yOffsetUpdateRate, sp90, 0.001f);
    camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->xzOffsetUpdateRate, sp94, 0.0001f);
    camera->fovUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->fovUpdateRate, sp94, 0.0001f);

    phi_v1_2 = MMCam_GetPitchAdjFromFloorHeightDiffs(camera, MM_BINANG_ROT180(sp70.yaw),
                                                     data->rw.flag & MM_NORMAL3_RW_FLAG);
    temp_f2 = ((1.0f / data->ro.pitchUpdateRateInv) * 0.5f) * (1.0f - camera->speedRatio);
    data->rw.curPitch = MMCamera_ScaledStepToCeilS(phi_v1_2, data->rw.curPitch,
                                                   ((1.0f / data->ro.pitchUpdateRateInv) * 0.5f) + temp_f2, 5);

    // MM calls Camera_CalcAtForHorse when the player rides a horse without NORMAL3_FLAG_6; a diving Zora
    // never rides one, so it is always Camera_CalcAtDefault here.
    MMCam_CalcAtDefault(camera, &sp68, data->ro.yOffset, 1);

    sp88 = (data->ro.distMax + data->ro.distMin) * 0.5f;
    sp80 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    temp_f2 = MMCam_ClampDist1(camera, sp80.r, data->ro.distMin, data->ro.distMax, data->rw.distTimer);

    phi_f2 = sp88 - temp_f2;
    phi_f2 *= 0.002f;
    camera->dist = sp80.r = temp_f2 + phi_f2;

    if (data->ro.interfaceFlags & MM_NORMAL3_FLAG_7) {
        sp80.pitch = MMCamera_ScaledStepToCeilS(player->actor.focus.rot.x - data->rw.curPitch, sp68.pitch, 0.25f, 5);
    } else {
        sp62 = data->ro.pitchTarget - data->rw.curPitch;
        sp80.pitch = MMCamera_ScaledStepToCeilS(sp62, sp68.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
    }

    sp80.pitch = CLAMP_MAX(sp80.pitch, MM_DEG_TO_BINANG(79.655f));

    if (sp80.pitch < -MM_DEG_TO_BINANG(29.995f)) {
        sp80.pitch = -MM_DEG_TO_BINANG(29.995f);
    }

    if (data->ro.interfaceFlags & MM_NORMAL3_FLAG_7) {
        sp62 = MM_BINANG_SUB(player->actor.focus.rot.y, MM_BINANG_ROT180(sp68.yaw));
        temp_f2 = 1.0f;
    } else {
        sp62 = MM_BINANG_SUB(focalActorPosRot->rot.y, MM_BINANG_ROT180(sp68.yaw));
        sp78 = MMOLib_Vec3fToVecGeo(&camera->playerPosDelta);
        phi_v1_2 = focalActorPosRot->rot.y - sp78.yaw;
        phi_v1_2 = MM_ABS_ALT(phi_v1_2);

        if (phi_v1_2 < 0x555A) {
            temp_f2 = 1.0f;
        } else {
            temp_f2 = ((f32)0x8000 - phi_v1_2) / (f32)0x2AA6;
        }
    }

    sp90 = (sp62 * ((SQ(camera->speedRatio) * 0.8f) + 0.2f) * temp_f2) / camera->yawUpdateRateInv;
    if ((MMCam_fabsf(sp90) > 150.0f) && (camera->speedRatio > 0.05f)) {
        sp80.yaw = sp68.yaw + sp90;
    }

    if (data->rw.yawTimer > 0) {
        sp80.yaw += data->rw.yawUpdateRate;
        data->rw.yawTimer--;
        if (data->rw.yawTimer == 0) {
            camera->unk_14C &= ~MM_CAM_STATE_DISABLE_MODE_CHANGE;
        }
    }

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &sp80);

    if (camera->status == CAM_STAT_ACTIVE) {
        *eye = *eyeNext;
        MMCam_func_800CBFA4(camera, at, eye, 0);
    } else {
        *eye = *eyeNext;
    }

    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.fovTarget, camera->fov, camera->fovUpdateRate, 0.1f);

    if (data->ro.interfaceFlags & MM_NORMAL3_FLAG_5) {
        camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.05f, 5);
    } else {
        camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.1f, 5);
    }

    camera->atLERPStepScale = MMCam_ClampLerpScale(camera, data->ro.maxAtLERPScale);
    data->rw.flag &= ~MM_NORMAL3_RW_FLAG;

    return 1;
}

// MM Camera_Parallel1 (DEKUHIDE, DEKUFLYZ, CHARGEZ, ZORAFINZ). 2S2H's opt-in "FixTargettingCameraSnap"
// enhancement (off by default) is not part of MM and is left out.
s32 Camera_MMParallel1(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f spB0;
    Vec3f spA4;
    f32 spA0;
    f32 sp9C;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMVecGeo sp90;
    MMVecGeo sp88;
    MMVecGeo sp80;
    MMVecGeo sp78;
    s16 sp72;
    s16 tangle;
    MMParallel1Data* data = &sMMCam.params.para1;
    s32 parallelFlagCond;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    s16 new_var2;
    s16 phi_a0;
    Player* player = camera->player;

    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);

        data->ro.unk_00 =
            MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_04 =
            MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_08 =
            MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_20 = MM_CAM_DEG_TO_BINANG(MM_GET_NEXT_RO_DATA(values));
        data->ro.unk_22 = MM_CAM_DEG_TO_BINANG(MM_GET_NEXT_RO_DATA(values));
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_18 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_1C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_24 = MM_GET_NEXT_RO_DATA(values);
        data->rw.unk_00 = data->ro.unk_04;
    }

    sp80 = MMOLib_Vec3fDiffToVecGeo(at, eye);
    sp78 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    spA4 = MMCam_GetFocalActorPos(camera);

    switch (camera->animState) {
        case 20:
            if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) == 0) {
                MMCam_SetUpdateRatesFastYaw(camera);
            }
            // fallthrough
        case 0:
        case 10:
            if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
                (MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) {
                data->rw.unk_10 = focalActorPosRot->pos;
            } else {
                camera->xzOffsetUpdateRate = 0.5f;
                camera->yOffsetUpdateRate = 0.5f;
            }

            if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
                MM_PARALLEL1_FLAG_3) {
                data->rw.unk_10 = camera->playerPosRot.pos;
            }

            data->rw.timer1 = 200.0f;

            if ((2.0f * data->ro.unk_04) < camera->dist) {
                camera->dist = 2.0f * data->ro.unk_04;
                sp78.r = camera->dist;
                sp80.r = sp78.r;
                *eye = MMOLib_AddVecGeoToVec3f(at, &sp80);
                *eyeNext = *eye;
            }

            data->rw.unk_1C = 0;

            if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_2) {
                data->rw.timer2 = 20;
            } else {
                data->rw.timer2 = 6;
            }

            if (sMMCam.runMode == MMCAM_MODE_CHARGE) {
                data->rw.timer2 = 30;
                if (camera->player->mergeShipForm == PLAYER_FORM_DEKU) {
                    data->ro.unk_24 = -1;
                }
            }

            if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
                (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_1)) {
                f32 yNormal;
                Vec3s* bgCamFuncData = Camera_GetCamBGData(camera);

                data->rw.timer2 = 1;
                yNormal = 0.8f - ((68.0f / focalActorHeight) * -0.2f);

                data->rw.unk_20 = BGCAM_ROT(bgCamFuncData).x;
                data->rw.unk_1E = BGCAM_ROT(bgCamFuncData).y;
                data->rw.unk_08 = (BGCAM_FOV(bgCamFuncData) == -1)   ? data->ro.unk_14
                                  : (BGCAM_FOV(bgCamFuncData) > 360) ? (BGCAM_FOV(bgCamFuncData) * 0.01f)
                                                                     : BGCAM_FOV(bgCamFuncData);
                data->rw.unk_00 = (BGCAM_JFIFID(bgCamFuncData) == -1)
                                      ? data->ro.unk_04
                                      : (BGCAM_JFIFID(bgCamFuncData) * 0.01f) * focalActorHeight * yNormal;
            } else {
                data->rw.unk_08 = data->ro.unk_14;
                data->rw.unk_00 = data->ro.unk_04;
            }

            data->rw.timer3 = data->ro.unk_24;
            data->rw.unk_04 = focalActorPosRot->pos.y - camera->playerPosDelta.y;
            data->rw.unk_26 = 1;
            camera->animState = 1;
            sCameraInterfaceFlags = data->ro.interfaceFlags;
            break;

        default:
            break;
    }

    if (data->rw.timer2 != 0) {
        switch (data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) {
            case MM_PARALLEL1_FLAG_1:
            case (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1):
                data->rw.unk_1E = MM_BINANG_ROT180(camera->playerPosRot.rot.y) + data->ro.unk_22;
                data->rw.unk_20 = data->ro.unk_20;
                break;

            case MM_PARALLEL1_FLAG_2:
                data->rw.unk_1E = data->ro.unk_22;
                data->rw.unk_20 = data->ro.unk_20;
                break;

            case (MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1):
                if (data->rw.timer3 == 1) {
                    sp88 = MMOLib_Vec3fDiffToVecGeo(&data->rw.unk_10, &spA4);
                    data->rw.unk_1E =
                        ((ABS(MM_BINANG_SUB(sp88.yaw, sp80.yaw)) < 0x3A98) || MMCam_IsClimbingLedge(camera))
                            ? sp80.yaw
                            : sp80.yaw + (s16)((MM_BINANG_SUB(sp88.yaw, sp80.yaw) >> 2) * 3);
                }
                data->rw.unk_20 = data->ro.unk_20;
                break;

            case MM_PARALLEL1_FLAG_3:
                data->rw.unk_1E = sp80.yaw;
                data->rw.unk_20 = data->ro.unk_20;
                break;

            case (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_1):
                break;

            default:
                data->rw.unk_1E = sp78.yaw + data->ro.unk_22;
                data->rw.unk_20 = data->ro.unk_20;
                break;
        }
    } else if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_5) {
        data->rw.unk_1E = MM_BINANG_ROT180(camera->playerPosRot.rot.y) + data->ro.unk_22;
    }

    if (camera->animState == 21) {
        camera->animState = 1;
    } else if (camera->animState == 11) {
        camera->animState = 1;
    }

    spA0 = camera->speedRatio * 0.5f;
    sp9C = camera->speedRatio * 0.2f;

    if (((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
         (MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ||
        ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
         MM_PARALLEL1_FLAG_3) ||
        (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_5)) {
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.5f, 0.1f);
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(data->ro.unk_0C, camera->yawUpdateRateInv, 0.5f, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->pitchUpdateRateInv, 0.5f, 0.1f);
    } else {
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, spA0, 0.1f);
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(data->ro.unk_0C, camera->yawUpdateRateInv, spA0, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(2.0f, camera->pitchUpdateRateInv, sp9C, 0.1f);
    }

    if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
        (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) {
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.1f, camera->yOffsetUpdateRate, spA0, 0.0001f);
        camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.1f, camera->xzOffsetUpdateRate, sp9C, 0.0001f);
    } else if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_7) {
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.5f, camera->yOffsetUpdateRate, spA0, 0.0001f);
        camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.5f, camera->xzOffsetUpdateRate, sp9C, 0.0001f);
    } else {
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->yOffsetUpdateRate, spA0, 0.0001f);
        camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->xzOffsetUpdateRate, sp9C, 0.0001f);
    }

    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(0.050f, camera->fovUpdateRate, camera->speedRatio * 0.05f, 0.0001f);

    if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_0) {
        tangle = MMCam_GetPitchAdjFromFloorHeightDiffs(camera, MM_BINANG_ROT180(sp80.yaw), data->rw.unk_26 = 1);
        spA0 = ((1.0f / data->ro.unk_10));
        spA0 *= 0.6f;
        sp9C = ((1.0f / data->ro.unk_10) * 0.4f) * (1.0f - camera->speedRatio);
        data->rw.unk_1C = MMCamera_ScaledStepToCeilS(tangle, data->rw.unk_1C, spA0 + sp9C, 5);
    } else {
        data->rw.unk_1C = 0;
    }

    if (MMCam_func_800CB950(camera) || (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) ||
        (player->stateFlags3 & PLAYER_STATE3_100)) {
        data->rw.unk_04 = camera->playerPosRot.pos.y;
        sp72 = false;
    } else {
        sp72 = true;
    }

    if ((player->stateFlags1 & MM_PLAYER_STATE1_4000) || (player->stateFlags1 & MM_PLAYER_STATE1_4) ||
        ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
         (MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1))) {
        spB0 = spA4;
        spB0.y += ((focalActorHeight * 0.6f) + data->ro.unk_00);
        MMCamera_ScaledStepToCeilVec3f(&spB0, at, camera->xzOffsetUpdateRate, camera->yOffsetUpdateRate, 0.0001f);
        MMCam_SetFocalActorAtOffset(camera, &focalActorPosRot->pos);
    } else if ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
               (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) {
        spB0 = focalActorPosRot->pos;
        spB0.y += focalActorHeight + data->ro.unk_00;
        MMCamera_ScaledStepToCeilVec3f(&spB0, at, camera->xzOffsetUpdateRate, camera->yOffsetUpdateRate, 0.0001f);
        MMCam_SetFocalActorAtOffset(camera, &focalActorPosRot->pos);
    } else if (data->rw.timer2 != 0) {
        MMCam_CalcAtDefault(camera, &sp78, data->ro.unk_00, 0);
        data->rw.timer1 = 200.0f;
    } else if (!(data->ro.interfaceFlags & MM_PARALLEL1_FLAG_7) && !sp72) {
        MMCam_CalcAtForParallel(camera, &sp78, data->ro.unk_00, data->ro.unk_08, &data->rw.unk_04,
                                data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_6 | MM_PARALLEL1_FLAG_0));
        data->rw.timer1 = 200.0f;
    } else {
        MMCam_CalcAtForScreen(camera, &sp78, data->ro.unk_00, &data->rw.unk_04, data->rw.timer1);
        if (data->rw.timer1 > 10.0f) {
            data->rw.timer1--;
        }
    }

    camera->dist = MMCamera_ScaledStepToCeilF(data->rw.unk_00, camera->dist, 1.0f / camera->rUpdateRateInv, 0.1f);

    if (data->rw.timer2 != 0) {
        if (data->rw.timer3 <= 0) {
            if (data->rw.timer3 == 0) {
                camera->unk_14C |= MM_CAM_STATE_DISABLE_MODE_CHANGE;
            }

            tangle = ((data->rw.timer2 + 1) * data->rw.timer2) >> 1;
            sp90.yaw = sp80.yaw + ((MM_BINANG_SUB(data->rw.unk_1E, sp80.yaw) / tangle) * data->rw.timer2);
            phi_a0 = ((data->ro.interfaceFlags & MM_PARALLEL1_FLAG_0) ? MM_BINANG_SUB(data->rw.unk_20, data->rw.unk_1C)
                                                                      : data->rw.unk_20);
            new_var2 = MM_BINANG_SUB(phi_a0, sp80.pitch);
            sp90.pitch = sp80.pitch + ((new_var2 / tangle) * data->rw.timer2);
            sp90.r = camera->dist;
            data->rw.timer2--;
        } else {
            sp90 = sp80;
            sp90.r = camera->dist;
        }
    } else {
        sp90 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
        sp90.r = camera->dist;

        if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_1) {
            sp90.yaw = MMCamera_ScaledStepToCeilS(data->rw.unk_1E, sp78.yaw, 1.0f / camera->yawUpdateRateInv, 0xC8);
        }

        parallelFlagCond = (data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1));

        if (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_0) {
            phi_a0 = (data->rw.unk_20 - data->rw.unk_1C);
        } else {
            phi_a0 = data->rw.unk_20;
        }

        if (parallelFlagCond == (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) {
            spA0 = CLAMP_MAX(camera->speedRatio, 1.0f);
            phi_a0 = (sp90.pitch * spA0) + (phi_a0 * (1.0f - spA0));
            sp90.pitch = MMCamera_ScaledStepToCeilS(phi_a0, sp78.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
        } else if (parallelFlagCond != MM_PARALLEL1_FLAG_3) {
            sp90.pitch = MMCamera_ScaledStepToCeilS(phi_a0, sp78.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
        }

        if (sp90.pitch > MM_DEG_TO_BINANG(79.655f)) {
            sp90.pitch = MM_DEG_TO_BINANG(79.655f);
        }
        if (sp90.pitch < -MM_DEG_TO_BINANG(29.995f)) {
            sp90.pitch = -MM_DEG_TO_BINANG(29.995f);
        }
    }

    if (data->rw.timer3 > 0) {
        data->rw.timer3--;
    }

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &sp90);

    if (camera->status == CAM_STAT_ACTIVE) {
        if ((camera->play->envCtx.skyboxDisabled == 0) || (data->ro.interfaceFlags & MM_PARALLEL1_FLAG_4)) {
            spB0 = *at;
            if ((player->stateFlags1 & MM_PLAYER_STATE1_4000) || (player->stateFlags1 & MM_PLAYER_STATE1_4) ||
                ((data->ro.interfaceFlags & (MM_PARALLEL1_FLAG_3 | MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1)) ==
                 (MM_PARALLEL1_FLAG_2 | MM_PARALLEL1_FLAG_1))) {
                spB0.y += focalActorHeight;
            }
            *eye = *eyeNext;
            MMCam_func_800CBFA4(camera, &spB0, eye, 0);
        } else {
            *eye = *eyeNext;
            MMCam_func_800CBFA4(camera, at, eye, 3);
        }

        if (data->rw.timer2 != 0) {
            sUpdateCameraDirection = true;
        } else {
            sUpdateCameraDirection = false;
        }
    }

    camera->fov = MMCamera_ScaledStepToCeilF(data->rw.unk_08, camera->fov, camera->fovUpdateRate, 0.1f);
    camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.5f, 5);
    camera->atLERPStepScale = MMCam_ClampLerpScale(camera, sp72 ? data->ro.unk_1C : data->ro.unk_18);
    data->rw.unk_26 &= ~1;

    return 1;
}

// MM Camera_ScaledStepToFloorS
static s16 MMCamera_ScaledStepToFloorS(s16 target, s16 cur, f32 stepScale, s16 minDiff) {
    s16 diff = target - cur;
    s16 step = diff * stepScale + 0.5f;

    return (ABS(diff) >= minDiff) ? cur + step : cur;
}

// MM Camera_GetFloorYNorm
static f32 MMCam_GetFloorYNorm(Camera* camera, Vec3f* floorNorm, Vec3f* chkPos, s32* bgId) {
    CollisionPoly* floorPoly;
    f32 floorY = BgCheck_EntityRaycastFloor3(&camera->play->colCtx, &floorPoly, bgId, chkPos);

    if (floorY == BGCHECK_Y_MIN) {
        floorNorm->x = 0.0f;
        floorNorm->y = 1.0f;
        floorNorm->z = 0.0f;
    } else {
        floorNorm->x = COLPOLY_GET_NORMAL(floorPoly->normal.x);
        floorNorm->y = COLPOLY_GET_NORMAL(floorPoly->normal.y);
        floorNorm->z = COLPOLY_GET_NORMAL(floorPoly->normal.z);
    }
    return floorY;
}

// MM Camera_GetFloorY
static f32 MMCam_GetFloorY(Camera* camera, Vec3f* pos) {
    Vec3f posCheck = *pos;
    Vec3f floorNorm;
    s32 bgId;

    posCheck.y += 80.0f;
    return MMCam_GetFloorYNorm(camera, &floorNorm, &posCheck, &bgId);
}

// MM Camera_Jump2 (CLIMB, CLIMBZ)
s32 Camera_MMJump2(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f spC8;
    Vec3f spBC;
    MMVecGeo spB4;
    MMVecGeo spAC;
    MMVecGeo spA4;
    MMVecGeo sp9C;
    s16 temp_t2;
    s16 yawDiff;
    f32 tmp;
    f32 sp90;
    f32 sp8C;
    s32 sp88;
    MMCamCollision sp60;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMJump2Data* data = &sMMCam.params.jump2;
    f32 phi_f2;
    f32 yNormal; // used twice
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    f32 temp_f16;

    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);

        yNormal = 0.8f - (-0.2f * (68.0f / focalActorHeight));

        if (camera->playerPosDelta.y > 0.0f) {
            phi_f2 = -10.0f;
        } else {
            phi_f2 = 10.0f;
        }

        data->ro.unk_00 = ((phi_f2 + MM_GET_NEXT_RO_DATA(values)) * 0.01f) * focalActorHeight * yNormal;
        data->ro.unk_04 = MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * yNormal;
        data->ro.unk_08 = MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * yNormal;
        data->ro.unk_0C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_1C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
    }

    sp9C = MMOLib_Vec3fDiffToVecGeo(at, eye);
    spA4 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    if (MMCamera_ReloadParams(camera)) {
        spC8 = focalActorPosRot->pos;
        data->rw.unk_00 = MMCam_GetFloorY(camera, &spC8);
        data->rw.unk_04 = spA4.yaw;
        data->rw.unk_06 = 0;

        if (data->rw.unk_00 == BGCHECK_Y_MIN) {
            data->rw.unk_0A = -1;
            data->rw.unk_00 = focalActorPosRot->pos.y - 1000.0f;
        } else if ((focalActorPosRot->pos.y - data->rw.unk_00) < focalActorHeight) {
            data->rw.unk_0A = 1;
        } else {
            data->rw.unk_0A = -1;
        }

        yawDiff = MM_BINANG_SUB(MM_BINANG_ROT180(focalActorPosRot->rot.y), spA4.yaw);
        data->rw.unk_06 = ((yawDiff / 6) / 4) * 3;

        if (data->ro.interfaceFlags & MM_JUMP2_FLAG_1) {
            data->rw.unk_08 = 10;
        } else {
            data->rw.unk_08 = 10000;
        }

        focalActorPosRot->pos.x -= camera->playerPosDelta.x;
        focalActorPosRot->pos.y -= camera->playerPosDelta.y;
        focalActorPosRot->pos.z -= camera->playerPosDelta.z;

        data->rw.timer = 6;
        camera->animState++;
        camera->atLERPStepScale = data->ro.unk_1C;
    }

    sp90 = camera->speedRatio * 0.5f;
    sp8C = camera->speedRatio * 0.2f;

    camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(data->ro.unk_10, camera->yawUpdateRateInv, sp90, 0.1f);
    camera->yOffsetUpdateRate =
        MMCamera_ScaledStepToCeilF(data->ro.unk_14, camera->yOffsetUpdateRate, sp90, 0.0001f);
    camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->xzOffsetUpdateRate, sp8C, 0.0001f);
    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(0.05f, camera->fovUpdateRate, camera->speedRatio * .05f, 0.0001f);
    camera->rUpdateRateInv = 1800.0f;

    MMCam_CalcAtDefault(camera, &spA4, data->ro.unk_00, 0);
    spB4 = MMOLib_Vec3fDiffToVecGeo(at, eye);

    tmp = data->ro.unk_04;
    phi_f2 = data->ro.unk_08 + (data->ro.unk_08 * data->ro.unk_0C);
    temp_f16 = tmp - (data->ro.unk_04 * data->ro.unk_0C);

    if (spB4.r > phi_f2) {
        spB4.r = phi_f2;
    } else if (spB4.r < temp_f16) {
        spB4.r = temp_f16;
    }

    yawDiff = MM_BINANG_SUB(MM_BINANG_ROT180(focalActorPosRot->rot.y), spB4.yaw);
    if (data->rw.timer != 0) {
        data->rw.unk_04 = MM_BINANG_ROT180(focalActorPosRot->rot.y);
        data->rw.timer--;
        spB4.yaw = MMCamera_ScaledStepToCeilS(data->rw.unk_04, spA4.yaw, 0.5f, 5);
    } else if (data->rw.unk_08 < ABS(yawDiff)) {
        temp_t2 = MM_BINANG_ROT180(focalActorPosRot->rot.y);
        spB4.yaw = MMCamera_ScaledStepToFloorS(
            (yawDiff < 0) ? (temp_t2 + data->rw.unk_08) : (temp_t2 - data->rw.unk_08), spA4.yaw, 0.1f, 1);
    } else {
        spB4.yaw = MMCamera_ScaledStepToCeilS(spB4.yaw, spA4.yaw, 0.25f, 5);
    }

    spC8.x = focalActorPosRot->pos.x + (Math_SinS(focalActorPosRot->rot.y) * 25.0f);
    spC8.y = focalActorPosRot->pos.y + (focalActorHeight * 2.2f);
    spC8.z = focalActorPosRot->pos.z + (Math_CosS(focalActorPosRot->rot.y) * 25.0f);

    yNormal = MMCam_GetFloorYNorm(camera, &spBC, &spC8, &sp88);
    if (camera->player->actor.bgCheckFlags & 0x10) {
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->pitchUpdateRateInv, 0.2f, 0.1f);
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.2f, 0.1f);
        spB4.pitch = MMCamera_ScaledStepToCeilS(-MM_DEG_TO_BINANG(27.47f), spA4.pitch, 0.2f, 5);
    } else if ((yNormal != BGCHECK_Y_MIN) && (focalActorPosRot->pos.y < yNormal)) {
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->pitchUpdateRateInv, 0.2f, 0.1f);
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.2f, 0.1f);
        if (camera->playerPosDelta.y > 1.0f) {
            spB4.pitch = MMCamera_ScaledStepToCeilS(0x1F4, spA4.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
        }
    } else if ((focalActorPosRot->pos.y - data->rw.unk_00) < focalActorHeight) {
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->pitchUpdateRateInv, 0.2f, 0.1f);
        camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(20.0f, camera->rUpdateRateInv, 0.2f, 0.1f);
        if (camera->playerPosDelta.y > 1.0f) {
            spB4.pitch = MMCamera_ScaledStepToCeilS(0x1F4, spA4.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
        }
    } else {
        camera->pitchUpdateRateInv = 100.0f;
        camera->rUpdateRateInv = 100.0f;
    }

    spB4.pitch = CLAMP_MAX(spB4.pitch, MM_DEG_TO_BINANG(60.43f));
    spB4.pitch = CLAMP_MIN(spB4.pitch, -MM_DEG_TO_BINANG(60.43f));

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &spB4);
    sp60.pos = *eyeNext;

    if (MMCam_func_800CBC84(camera, at, &sp60, 0) != 0) {
        spC8 = sp60.pos;
        spAC.pitch = 0;
        spAC.r = spB4.r;
        spAC.yaw = spB4.yaw;
        sp60.pos = MMOLib_AddVecGeoToVec3f(at, &spAC);
        if (MMCam_func_800CBC84(camera, at, &sp60, 0) != 0) {
            *eye = spC8;
        } else {
            spB4.pitch = MMCamera_ScaledStepToCeilS(0, spB4.pitch, 0.2f, 5);
            *eye = MMOLib_AddVecGeoToVec3f(at, &spB4);
            MMCam_func_800CBFA4(camera, at, eye, 0);
        }
    } else {
        *eye = *eyeNext;
    }

    camera->dist = spB4.r;
    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_18, camera->fov, camera->fovUpdateRate, 0.1f);
    camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.5f, 5);

    return true;
}

/*--------------------------------------------------------------------------------------------------
 * MM lock-on and talk cameras: Camera_Battle1 (BATTLE), Camera_KeepOn1 (FOLLOWTARGET, FOLLOWBOOMERANG),
 * Camera_KeepOn3 (TALK), with the helpers only they use.
 *------------------------------------------------------------------------------------------------*/

#define MM_CAM_BINANG_TO_DEG(binang) ((f32)(binang) * (360.0001525f / 0xFFFF))
#define MM_F32_LERPIMP(v0, v1, t) ((f32)(v0) + (((f32)(v1) - (f32)(v0)) * (t)))
#define MM_BATTLE1_FLAG_0 (1 << 0)
#define MM_BATTLE1_FLAG_1 (1 << 1)
#define MM_BATTLE1_FLAG_4 (1 << 4)
#define MM_BATTLE1_FLAG_7 (1 << 7)
#define MM_KEEPON1_FLAG_0 (1 << 0)
#define MM_KEEPON1_FLAG_1 (1 << 1)
#define MM_KEEPON1_FLAG_4 (1 << 4)
#define MM_KEEPON1_FLAG_7 (1 << 7)
#define MM_KEEPON3_FLAG_5 (1 << 5)
#define MM_KEEPON3_FLAG_6 (1 << 6)
#define MM_KEEPON3_FLAG_7 (1 << 7)
#define MM_CAM_STATE_2 (1 << 2)
#define MM_CAM_STATE_3 (1 << 3)
#define MM_CAM_STATE_4 (1 << 4)
#define MM_CAM_STATE_10 (1 << 10)
#define MM_FLG_ADJSLOPE (1 << 0)
#define MM_FLG_OFFGROUND (1 << 7)

// OoT's actor attention ranges (z_actor.c D_80115FF8, MM gAttentionRanges): same layout as MM's.
typedef struct {
    f32 rangeSq;
    f32 leashScale;
} MMCamAttentionRange;
extern MMCamAttentionRange D_80115FF8[];

// MM D_801B9E18 / D_801B9E34 (z_camera_data.inc): VecGeo yaw / pitch offsets tried around a blocked view.
static s16 sMMCamAngleYaws[] = {
    0x0AAA, (s16)0xF556, 0x1555, (s16)0xEAAB, 0x2AAA, (s16)0xD556, 0x3FFF,
    (s16)0xC001, 0x5555, (s16)0xAAAB, 0x6AAA, (s16)0x9556, 0x7FFF, 0x0000,
};
static s16 sMMCamAnglePitches[] = {
    0x0000, 0x02C6, 0x058C, 0x0000, 0x0000, (s16)0xFD3A, 0x0000,
    0x0852, 0x0000, 0x0000, 0x0B18, 0x02C6, (s16)0xFA74, 0x0000,
};

// MM Camera_SetUpdateRatesSlow
static void MMCam_SetUpdateRatesSlow(Camera* camera) {
    camera->rUpdateRateInv = 1800.0f;
    camera->yawUpdateRateInv = 1800.0f;
    camera->pitchUpdateRateInv = 1800.0f;
    camera->yOffsetUpdateRate = 0.01f;
    camera->xzOffsetUpdateRate = 0.01f;
    camera->fovUpdateRate = 0.01f;
}

// MM Camera_IsChargingSpinAttackOrDekuFlowerDive
static s32 MMCam_IsChargingSpinAttackOrDekuFlowerDive(Camera* camera) {
    return (camera->player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) ||
           (camera->player->stateFlags3 & PLAYER_STATE3_100);
}

// MM Camera_GetAttentionActorRange
static f32 MMCam_GetAttentionActorRange(Actor* actor) {
    return sqrtf(D_80115FF8[actor->targetMode].rangeSq / D_80115FF8[actor->targetMode].leashScale);
}

// MM Camera_CalcAtForFriendlyLockOn
static s32 MMCam_CalcAtForFriendlyLockOn(Camera* camera, MMVecGeo* eyeAtDir, Vec3f* targetPos, f32 yOffset,
                                         f32 distance, f32* yPosOffset, MMVecGeo* outPlayerToTargetDir, s16 flags) {
    Vec3f* at = &camera->at;
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    Vec3f sp68;
    f32 temp_f0_6;
    MMVecGeo sp5C;
    f32 deltaY;
    f32 fovHeight;
    f32 phi_f16;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    f32 sp50;
    PosRot* focalActorPosRot = &camera->playerPosRot;

    focalActorAtOffsetTarget.y = focalActorHeight + yOffset;
    focalActorAtOffsetTarget.x = 0.0f;
    focalActorAtOffsetTarget.z = 0.0f;

    // (MM's slope term is gated on the debug register PREG(76), 0 in both games.)

    atTarget = focalActorPosRot->pos;
    atTarget.y += focalActorHeight;

    *outPlayerToTargetDir = MMOLib_Vec3fDiffToVecGeo(&atTarget, targetPos);
    sp5C = *outPlayerToTargetDir;

    sp5C.r = (distance < sp5C.r) ? (sp5C.r * 0.2f) : ((sp5C.r * 0.9f) - (sp5C.r * 0.7f * (sp5C.r / distance)));

    if (flags & MM_FLG_OFFGROUND) {
        sp5C.r *= 0.2f;
        camera->yOffsetUpdateRate = camera->xzOffsetUpdateRate = 0.01f;
    }

    sp68 = MMOLib_VecGeoToVec3f(&sp5C);

    focalActorAtOffsetTarget.x += sp68.x;
    focalActorAtOffsetTarget.y += sp68.y;
    focalActorAtOffsetTarget.z += sp68.z;

    if (MMCam_func_800CB950(camera)) {
        *yPosOffset = MMCamera_ScaledStepToCeilF(focalActorPosRot->pos.y, *yPosOffset, 0.4f, 0.1f);
        deltaY = focalActorPosRot->pos.y - *yPosOffset;
        focalActorAtOffsetTarget.y -= deltaY;
        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                       camera->yOffsetUpdateRate, 0.1f);
    } else {
        if (!(flags & 0x80)) {
            deltaY = focalActorPosRot->pos.y - *yPosOffset;
            sp50 = OLib_Vec3fDistXZ(at, &camera->eye);
            phi_f16 = sp50;
            Math_FAtan2F(deltaY, sp50);
            fovHeight = Math_FTanF(MM_DEG_TO_RAD(camera->fov * 0.4f)) * phi_f16;

            if (fovHeight < deltaY) {
                *yPosOffset += deltaY - fovHeight;
                deltaY = fovHeight;
            } else if (deltaY < -fovHeight) {
                *yPosOffset += deltaY + fovHeight;
                deltaY = -fovHeight;
            }
            focalActorAtOffsetTarget.y -= deltaY;
        } else {
            deltaY = focalActorPosRot->pos.y - *yPosOffset;
            temp_f0_6 = Math_FAtan2F(deltaY, OLib_Vec3fDistXZ(at, &camera->eye));

            if (temp_f0_6 > 0.34906584f) { // (M_PI / 9)
                phi_f16 = 1.0f - Math_SinF(temp_f0_6 - 0.34906584f);
            } else if (temp_f0_6 < -0.17453292f) { // (M_PI / 18)
                phi_f16 = 1.0f - Math_SinF(-0.17453292f - temp_f0_6);
            } else {
                phi_f16 = 1.0f;
            }

            focalActorAtOffsetTarget.y -= deltaY * phi_f16;
        }

        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, 0.5f, 0.5f, 0.1f);
        camera->xzOffsetUpdateRate = 0.5f;
        camera->yOffsetUpdateRate = 0.5f;
    }

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;
    MMCamera_ScaledStepToCeilVec3f(&atTarget, at, camera->atLERPStepScale, camera->atLERPStepScale, 0.2f);

    return 1;
}

// MM Camera_CalcAtForEnemyLockOn
static s32 MMCam_CalcAtForEnemyLockOn(Camera* camera, MMVecGeo* arg1, Vec3f* arg2, f32 yOffset, f32 arg4, f32 arg5,
                                      f32* arg6, MMVecGeo* arg7, s16 flags) {
    PosRot* focalActorPosRot = &camera->playerPosRot;
    Vec3f focalActorAtOffsetTarget;
    Vec3f atTarget;
    Vec3f sp60;
    MMVecGeo sp58;
    f32 temp_f0_3;
    f32 deltaY;
    f32 temp;
    f32 fovHeight;

    focalActorAtOffsetTarget.y = MMCam_GetFocalActorHeight(camera) + yOffset;
    focalActorAtOffsetTarget.x = 0.0f;
    focalActorAtOffsetTarget.z = 0.0f;

    sp58 = *arg7;
    sp58.r = arg7->r * (arg5 * arg4);

    if (flags & 0x80) {
        camera->yOffsetUpdateRate = 0.01f;
        camera->xzOffsetUpdateRate = 0.01f;
    }

    sp60 = MMOLib_VecGeoToVec3f(&sp58);

    focalActorAtOffsetTarget.x += sp60.x;
    focalActorAtOffsetTarget.y += sp60.y;
    focalActorAtOffsetTarget.z += sp60.z;

    if (MMCam_func_800CB950(camera)) {
        *arg6 = MMCamera_ScaledStepToCeilF(focalActorPosRot->pos.y, *arg6, 0.4f, 0.1f);
        deltaY = focalActorPosRot->pos.y - *arg6;
        focalActorAtOffsetTarget.y -= deltaY;
        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, camera->xzOffsetUpdateRate,
                                       camera->yOffsetUpdateRate, 0.1f);
    } else {
        deltaY = focalActorPosRot->pos.y - *arg6;

        temp = arg1->r;
        temp_f0_3 = Math_FAtan2F(deltaY, temp);

        if (!(flags & 0x80)) {
            fovHeight = Math_FTanF(MM_DEG_TO_RAD(camera->fov * 0.4f)) * temp;

            if (fovHeight < deltaY) {
                *arg6 += deltaY - fovHeight;
                deltaY = fovHeight;
            } else if (deltaY < -fovHeight) {
                *arg6 += deltaY + fovHeight;
                deltaY = -fovHeight;
            }

            focalActorAtOffsetTarget.y -= deltaY;
        } else {
            if (temp_f0_3 > 0.34906584f) { // (M_PI / 9)
                temp = 1.0f - Math_SinF(temp_f0_3 - 0.34906584f);
            } else if (temp_f0_3 < -0.17453292f) { // (M_PI / 18)
                temp = 1.0f - Math_SinF(-0.17453292f - temp_f0_3);
            } else {
                temp = 1.0f;
            }

            focalActorAtOffsetTarget.y -= deltaY * temp;
        }
        MMCamera_ScaledStepToCeilVec3f(&focalActorAtOffsetTarget, &camera->posOffset, 0.5f, 0.5f, 0.1f);
        camera->xzOffsetUpdateRate = 0.5f;
        camera->yOffsetUpdateRate = 0.5f;
    }

    atTarget.x = focalActorPosRot->pos.x + camera->posOffset.x;
    atTarget.y = focalActorPosRot->pos.y + camera->posOffset.y;
    atTarget.z = focalActorPosRot->pos.z + camera->posOffset.z;
    MMCamera_ScaledStepToCeilVec3f(&atTarget, &camera->at, camera->atLERPStepScale, camera->atLERPStepScale, 0.2f);

    return true;
}

// MM Battle1ReadOnlyData / Battle1ReadWriteData
typedef struct {
    struct {
        f32 yOffset, unk_04, unk_08, unk_0C, unk_10, swingPitchInitial, swingPitchFinal, swingPitchAdj, fov, unk_24,
            unk_28, unk_2C;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00, unk_04;
        Actor* unk_08;
        s16 unk_10, unk_12, unk_14, unk_16, chargeTimer, unk_1A;
    } rw;
} MMBattle1Data;

// MM KeepOn1ReadOnlyData / KeepOn1ReadWriteData
typedef struct {
    struct {
        f32 yOffset, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C, unk_20, unk_24, unk_28;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00, unk_04, unk_08;
        Actor* unk_0C;
        s16 unk_10, unk_12, unk_14, unk_16, unk_18;
    } rw;
} MMKeepOn1Data;

// MM KeepOn3ReadOnlyData / KeepOn3ReadWriteData
typedef struct {
    struct {
        f32 unk_00, unk_04, unk_08, unk_0C, unk_10, unk_14, unk_18, unk_1C, unk_20, unk_24, unk_28;
        s16 timer;
        s16 interfaceFlags;
    } ro;
    struct {
        f32 unk_00, unk_04, unk_08;
        Actor* unk_0C;
        Vec3f unk_10;
        s16 timer;
    } rw;
} MMKeepOn3Data;

// The lock-on functions' state lives beside sMMCam.params (their structs are declared after it).
static union {
    MMBattle1Data batt1;
    MMKeepOn1Data keep1;
    MMKeepOn3Data keep3;
} sMMCamLockOnParams;

// MM Camera_Battle1 (BATTLE: locked on to an enemy)
s32 Camera_MMBattle1(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f sp120;
    Vec3f sp114;
    f32 temp_f14;
    f32 temp_f2_3;
    f32 sp104;
    f32 var2;
    f32 spFC;
    f32 spF8;
    f32 spF4;
    s32 skipEyeAtCalc = false;
    f32 distRatio;
    MMCamCollision spC4;
    MMVecGeo spBC;
    MMVecGeo spB4;
    MMVecGeo atToTargetDir;
    MMVecGeo spA4;
    MMVecGeo atToEyeDir;
    MMVecGeo atToEyeNextDir;
    PosRot* focalActorFocus = &camera->player->actor.focus;
    s16 sp8A;
    s16 sp88;
    s16 sp86;
    s16 isOffGround;
    f32 swingPitchInitial;
    f32 swingPitchFinal;
    f32 fov;
    f32 temp_f12;
    MMBattle1Data* data = &sMMCamLockOnParams.batt1;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);

        data->ro.yOffset =
            MM_GET_NEXT_RO_DATA(values) * 0.01f * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_04 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_08 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_0C = 80.0f - (80.0f - data->ro.unk_0C);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_10 = 0.0f - (0.0f - data->ro.unk_10);
        data->ro.swingPitchInitial = MM_GET_NEXT_RO_DATA(values);
        data->ro.swingPitchInitial = 40.0f - (40.0f - data->ro.swingPitchInitial);
        data->ro.swingPitchFinal = MM_GET_NEXT_RO_DATA(values);
        data->ro.swingPitchFinal = 20.0f - (20.0f - data->ro.swingPitchFinal);
        data->ro.swingPitchAdj = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->ro.swingPitchAdj = 1.0f - (1.0f - data->ro.swingPitchAdj);
        data->ro.fov = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_24 = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->ro.unk_24 = 1.0f - (1.0f - data->ro.unk_24);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_28 = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->ro.unk_28 = 0.2f - (0.2f - data->ro.unk_28);
        data->ro.unk_2C = MM_GET_NEXT_RO_DATA(values) * 0.01f;
        data->rw.chargeTimer = 40;
        camera->pitchUpdateRateInv = 9.0f;
    }

    swingPitchInitial = data->ro.swingPitchInitial;
    swingPitchFinal = data->ro.swingPitchFinal;
    fov = data->ro.fov;

    if (MMCam_IsChargingSpinAttackOrDekuFlowerDive(camera)) {
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(18.0f, camera->pitchUpdateRateInv, 0.5f, 0.1f);
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.2f, camera->yOffsetUpdateRate, 0.5f, 0.0001f);
        camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.2f, camera->xzOffsetUpdateRate, 0.5f, 0.0001f);
        if (data->rw.chargeTimer > -20) {
            data->rw.chargeTimer--;
        } else {
            swingPitchInitial = 50.0f;
            swingPitchFinal = 40.0f;
            fov = 60.0f;
        }
    } else {
        if (data->rw.chargeTimer < 0) {
            swingPitchInitial = 50.0f;
            swingPitchFinal = 40.0f;
            fov = 60.0f;
            data->rw.chargeTimer++;
        } else {
            data->rw.chargeTimer = 40;
            camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(9.0f, camera->pitchUpdateRateInv, 0.5f, 0.1f);
            camera->yOffsetUpdateRate =
                MMCamera_ScaledStepToCeilF(0.6f, camera->yOffsetUpdateRate, camera->speedRatio * 0.5f, 0.0001f);
            camera->xzOffsetUpdateRate =
                MMCamera_ScaledStepToCeilF(0.6f, camera->xzOffsetUpdateRate, camera->speedRatio * 0.2f, 0.0001f);
        }
    }

    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(0.050f, camera->fovUpdateRate, camera->speedRatio * 0.05f, 0.0001f);

    focalActorHeight += data->ro.yOffset;

    atToEyeDir = MMOLib_Vec3fDiffToVecGeo(at, eye);
    atToEyeNextDir = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    if ((camera->target == NULL) || (camera->target->update == NULL)) {
        camera->target = NULL;
        Camera_RequestMode(camera, CAM_MODE_TARGET);
        return true;
    }

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    if (MMCamera_ReloadParams(camera)) {
        data->rw.unk_10 = 0;
        data->rw.unk_1A = 0;
        data->rw.unk_08 = camera->target;
        camera->animState++;
        data->rw.unk_16 = 7;
        data->rw.unk_12 = atToEyeDir.yaw;
        data->rw.unk_14 = atToEyeDir.pitch;
        data->rw.unk_00 = atToEyeDir.r;
        data->rw.unk_04 = camera->playerPosRot.pos.y - camera->playerPosDelta.y;
        if (camera->dist > (2.0f * data->ro.unk_04)) {
            camera->dist = 2.0f * data->ro.unk_04;
            atToEyeNextDir.r = camera->dist;
            atToEyeDir.r = atToEyeNextDir.r;
            *eye = MMOLib_AddVecGeoToVec3f(at, &atToEyeDir);
            *eyeNext = *eye;
        }
    }

    if (camera->status == CAM_STAT_ACTIVE) {
        sUpdateCameraDirection = true;
        camera->inputDir.x = -atToEyeDir.pitch;
        camera->inputDir.y = atToEyeDir.yaw + 0x8000;
        camera->inputDir.z = 0;
    }

    if (MMCam_func_800CB950(camera)) {
        data->rw.unk_04 = camera->playerPosRot.pos.y;
        isOffGround = false;
    } else {
        isOffGround = true;
    }

    if (data->rw.unk_16 == 0) {
        camera->atLERPStepScale = MMCam_ClampLerpScale(camera, isOffGround ? data->ro.unk_28 : data->ro.unk_24);
    }

    Actor_GetFocus(&camera->targetPosRot, camera->target);

    if (data->rw.unk_08 != camera->target) {
        camera->animState = 0;
        return true;
    }

    sp120 = camera->playerPosRot.pos;
    sp120.y += focalActorHeight;
    spA4 = MMOLib_Vec3fDiffToVecGeo(&sp120, &camera->targetPosRot.pos);

    sp104 = MMCam_GetAttentionActorRange(camera->target);
    if (sp104 > (PREG(86) + 800.0f)) {
        sp104 = PREG(86) + 800.0f;
    }

    if ((spA4.r > sp104) || MMCam_IsChargingSpinAttackOrDekuFlowerDive(camera)) {
        distRatio = 1.0f;
        spF8 = 10.0f;
    } else {
        distRatio = spA4.r / sp104;
        spF8 = 2.0f;
    }

    MMCam_CalcAtForEnemyLockOn(camera, &atToEyeNextDir, &camera->targetPosRot.pos, data->ro.yOffset, data->ro.unk_2C,
                               1.0f - distRatio, &data->rw.unk_04, &spA4,
                               (isOffGround ? (MM_BATTLE1_FLAG_7 | MM_BATTLE1_FLAG_0) : MM_BATTLE1_FLAG_0) |
                                   data->ro.interfaceFlags);

    sp88 = spA4.yaw;
    spBC = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    spF4 = MM_F32_LERPIMP(data->ro.unk_04, data->ro.unk_08, distRatio);
    camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(spF8, camera->rUpdateRateInv, 0.5f, 0.1f);
    spBC.r = camera->dist = MMCamera_ScaledStepToCeilF(spF4, camera->dist, 1.0f / camera->rUpdateRateInv, 0.1f);

    atToTargetDir = MMOLib_Vec3fDiffToVecGeo(at, &camera->targetPosRot.pos);

    atToTargetDir.r = spBC.r - (((spBC.r >= atToTargetDir.r) ? atToTargetDir.r : spBC.r) * 0.5f);

    if (data->rw.unk_1A & 0x1000) {
        spFC = 15.0f;
    } else {
        spFC = data->ro.unk_0C + ((data->ro.unk_10 - data->ro.unk_0C) * (1.1f - distRatio));
    }

    spBC.yaw = atToEyeNextDir.yaw;
    sp8A = (s16)(atToTargetDir.yaw - (s16)(atToEyeNextDir.yaw + 0x8000));

    if (data->rw.unk_16 != 0) {
        if (data->rw.unk_16 > 0) {
            sp86 = data->rw.unk_16 - 1;
            spB4 = MMOLib_Vec3fDiffToVecGeo(at, eye);
            spB4.yaw = sp88 + 0x8000;
            spF8 = (data->rw.unk_00 - spB4.r) * (1.0f / 6.0f);
            sp8A = (s16)(data->rw.unk_12 - spB4.yaw) * (1.0f / 6.0f);
            sp88 = (s16)(data->rw.unk_14 - spB4.pitch) * (1.0f / 6.0f);
            spBC.r = MMCamera_ScaledStepToCeilF((sp86 * spF8) + spB4.r, atToEyeDir.r, 0.5f, 1.0f);
            spBC.yaw = MMCamera_ScaledStepToCeilS(spB4.yaw + (sp8A * sp86), atToEyeDir.yaw, 0.5f, 5);
            spBC.pitch = MMCamera_ScaledStepToCeilS(spB4.pitch + (sp88 * sp86), atToEyeDir.pitch, 0.5f, 5);
        } else {
            skipEyeAtCalc = true;
        }
        data->rw.unk_16--;
    } else if ((ABS(sp8A) > MM_CAM_DEG_TO_BINANG(spFC)) && !isOffGround) {
        sp104 = MM_CAM_BINANG_TO_DEG(sp8A);
        temp_f2_3 = spFC + (((spFC + 10.0f) - spFC) * (OLib_ClampMaxDist(atToTargetDir.r, spBC.r) / spBC.r));
        temp_f12 = (SQ(temp_f2_3) - 2.0f) / (temp_f2_3 - 360.0f);
        var2 = ((temp_f12 * sp104) + (2.0f - (360.0f * temp_f12)));
        temp_f14 = SQ(sp104) / var2;

        if (sp8A >= 0) {
            sp88 = MM_CAM_DEG_TO_BINANG(temp_f14);
        } else {
            sp88 = -MM_CAM_DEG_TO_BINANG(temp_f14);
        }

        spBC.yaw = (s16)((s16)(atToEyeNextDir.yaw + 0x8000) + sp88) + 0x8000;
    } else {
        sp104 = (1.0f - camera->speedRatio) * 0.05f;
        sp88 = (sp8A >= 0) ? MM_CAM_DEG_TO_BINANG(spFC) : -MM_CAM_DEG_TO_BINANG(spFC);
        spBC.yaw = atToEyeNextDir.yaw - MM_TRUNCF_BINANG((sp88 - sp8A) * sp104);
    }

    if (!skipEyeAtCalc) {
        spF8 = atToTargetDir.pitch * data->ro.swingPitchAdj;
        var2 = swingPitchInitial + ((swingPitchFinal - swingPitchInitial) * distRatio);
        sp8A = MM_CAM_DEG_TO_BINANG(var2) - MM_TRUNCF_BINANG((spA4.pitch * (0.5f + (distRatio * (1.0f - 0.5f)))) + 0.5f);
        sp8A += MM_TRUNCF_BINANG(spF8);

        if (sp8A < -0x2AA8) {
            sp8A = -0x2AA8;
        } else if (sp8A > 0x2AA8) {
            sp8A = 0x2AA8;
        }

        spBC.pitch = MMCamera_ScaledStepToCeilS(sp8A, atToEyeNextDir.pitch, 1.0f / camera->pitchUpdateRateInv, 5);
        *eyeNext = MMOLib_AddVecGeoToVec3f(at, &spBC);
        spC4.pos = *eyeNext;

        if (camera->status == CAM_STAT_ACTIVE) {
            if (!(data->ro.interfaceFlags & MM_BATTLE1_FLAG_4)) {
                if (!camera->play->envCtx.skyboxDisabled || (data->ro.interfaceFlags & MM_BATTLE1_FLAG_0)) {
                    if (MMCam_func_800CBC84(camera, at, &spC4, 0) != 0) {
                        data->rw.unk_1A |= 0x1000;
                    } else {
                        data->rw.unk_1A &= ~0x1000;
                    }
                } else if (data->ro.interfaceFlags & MM_BATTLE1_FLAG_1) {
                    MMCam_func_800CBC84(camera, at, &spC4, 3);
                } else {
                    sp114 = MMOLib_Vec3fDistNormalize(at, &spC4.pos);
                    spC4.pos.x -= sp114.x;
                    spC4.pos.y -= sp114.y;
                    spC4.pos.z -= sp114.z;
                }
                *eye = spC4.pos;
                data->rw.unk_1A &= ~0x10;
            } else if (!camera->play->envCtx.skyboxDisabled || (data->ro.interfaceFlags & MM_BATTLE1_FLAG_0)) {
                if (MMCam_func_800CBC84(camera, at, &spC4, 0) != 0) {
                    s16 screenPosX;
                    s16 screenPosY;

                    data->rw.unk_1A |= 0x1000;
                    spF8 = OLib_Vec3fDist(at, &focalActorFocus->pos);
                    spF4 = OLib_Vec3fDist(at, &spC4.pos);
                    spF8 += (data->rw.unk_1A & 0x10) ? 40.0f : 0.0f;
                    Actor_GetScreenPos(camera->play, &camera->player->actor, &screenPosX, &screenPosY);

                    if ((spF4 < spF8) || ((screenPosX >= 0) && (screenPosX <= SCREEN_WIDTH) && (screenPosY >= 0) &&
                                          (screenPosY <= SCREEN_HEIGHT))) {
                        data->rw.unk_1A |= 0x10;
                        spB4.yaw = spA4.yaw + 0x8000;
                        spB4.pitch = -spA4.pitch;
                        spB4.r = 40.0f;
                        sp114 = focalActorFocus->pos;
                        sp114.y += 40.0f;
                        sp120 = MMOLib_AddVecGeoToVec3f(&sp114, &spB4);
                        MMCamera_ScaledStepToCeilVec3f(&sp120, eye, 0.15f, 0.15f, 0.2f);
                    } else if (data->rw.unk_1A & 0x10) {
                        if (OLib_Vec3fDist(&spC4.pos, eye) < 20.0f) {
                            data->rw.unk_1A &= ~0x10;
                            *eye = spC4.pos;
                        } else {
                            MMCamera_ScaledStepToCeilVec3f(&spC4.pos, eye, 0.15f, 0.15f, 0.2f);
                        }
                    } else {
                        data->rw.unk_1A &= ~0x10;
                        *eye = spC4.pos;
                    }
                } else {
                    if (data->rw.unk_1A & 0x10) {
                        if (OLib_Vec3fDist(&spC4.pos, eye) < 20.0f) {
                            data->rw.unk_1A &= ~0x10;
                            *eye = spC4.pos;
                        } else {
                            MMCamera_ScaledStepToCeilVec3f(&spC4.pos, eye, 0.15f, 0.15f, 0.2f);
                        }
                    } else {
                        data->rw.unk_1A &= ~0x10;
                        *eye = spC4.pos;
                    }
                    data->rw.unk_1A &= ~0x1000;
                }
            } else if (data->ro.interfaceFlags & MM_BATTLE1_FLAG_1) {
                data->rw.unk_1A &= ~0x10;
                MMCam_func_800CBC84(camera, at, &spC4, 3);
                *eye = spC4.pos;
            } else {
                data->rw.unk_1A &= ~0x10;
                sp114 = MMOLib_Vec3fDistNormalize(at, &spC4.pos);
                spC4.pos.x -= sp114.x;
                spC4.pos.y -= sp114.y;
                spC4.pos.z -= sp114.z;
                *eye = spC4.pos;
            }
        } else {
            data->rw.unk_1A &= ~0x10;
            *eye = *eyeNext;
        }
    }

    if (data->rw.unk_16 != 0) {
        sp88 = 0;
    } else {
        sp88 = (s16)(atToEyeNextDir.yaw - spBC.yaw) * 0.50f;
    }
    camera->roll = MMCamera_ScaledStepToCeilS(sp88, camera->roll, 0.06f, 5);

    // MM func_800CBAAC: the player's meleeWeaponState.
    if (camera->player->meleeWeaponState != 0) {
        temp_f12 = ((camera->play->state.frames & 8) != 0) ? data->ro.fov - (data->ro.fov * 0.5f) : data->ro.fov;
    } else {
        temp_f12 = ((gSaveContext.health <= 16) ? 0.8f : 1.0f) * (fov - (fov * 0.05f * distRatio));
    }
    camera->fov = MMCamera_ScaledStepToCeilF(temp_f12, camera->fov, camera->fovUpdateRate, 0.1f);

    return true;
}

// MM Camera_KeepOn1 (FOLLOWTARGET: locked on to a friendly actor; FOLLOWBOOMERANG)
s32 Camera_MMKeepOn1(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f sp130;
    Vec3f sp124;
    Vec3f sp118;
    f32 sp114;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    f32 var2;
    f32 sp104;
    f32 temp_f2_3;
    f32 spFC;
    f32 spF8;
    f32 spF4;
    s16 spF2;
    s16 spF0;
    MMVecGeo spE8;
    MMVecGeo spE0;
    MMVecGeo spD8;
    MMVecGeo spD0;
    MMVecGeo atToEye;
    MMVecGeo atToEyeNext;
    PosRot spA8;
    PosRot* focalActorFocus = &camera->player->actor.focus;
    MMCamCollision sp7C;
    s32 skipEyeAtCalc = false;
    f32 sp74;
    s16 sp72;
    s16 sp70;
    MMKeepOn1Data* data = &sMMCamLockOnParams.keep1;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    if ((camera->target == NULL) || (camera->target->update == NULL)) {
        camera->target = NULL;
        Camera_RequestMode(camera, CAM_MODE_TARGET);
        return 1;
    }

    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);

        data->ro.yOffset =
            MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_04 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_08 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = 40.0f - (40.0f - data->ro.unk_14);
        data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_18 = 20.0f - (20.0f - data->ro.unk_18);
        data->ro.unk_1C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_1C = 1.000f - (1.00f - data->ro.unk_1C);
        data->ro.unk_20 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_24 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_24 = 1.0f - (1.0f - data->ro.unk_24);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_28 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_28 = 0.2f - (0.2f - data->ro.unk_28);
    }

    focalActorHeight += data->ro.yOffset;

    atToEye = MMOLib_Vec3fDiffToVecGeo(at, eye);
    atToEyeNext = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    if (MMCamera_ReloadParams(camera)) {
        camera->animState++;
        data->rw.unk_18 = 0;
        data->rw.unk_10 = 0;
        data->rw.unk_04 = 0.0f;
        data->rw.unk_0C = camera->target;
        data->rw.unk_16 = 7;
        data->rw.unk_12 = atToEye.yaw;
        data->rw.unk_14 = atToEye.pitch;
        data->rw.unk_00 = atToEye.r;
        data->rw.unk_08 = focalActorPosRot->pos.y - camera->playerPosDelta.y;
        if (camera->dist > (2.0f * data->ro.unk_04)) {
            camera->dist = 2.0f * data->ro.unk_04;
            atToEyeNext.r = camera->dist;
            atToEye.r = atToEyeNext.r;
            *eye = MMOLib_AddVecGeoToVec3f(at, &atToEye);
            *eyeNext = *eye;
        }
    }

    if (camera->status == CAM_STAT_ACTIVE) {
        sUpdateCameraDirection = true;
        camera->inputDir.x = -atToEye.pitch;
        camera->inputDir.y = atToEye.yaw + 0x8000;
        camera->inputDir.z = 0;
    }

    if (MMCam_func_800CB950(camera)) {
        data->rw.unk_08 = focalActorPosRot->pos.y;
    }

    sp114 = data->ro.unk_04;

    if (camera->target->id != ACTOR_EN_BOOM) {
        Actor_GetWorldPosShapeRot(&camera->targetPosRot, camera->target);
        Actor_GetFocus(&spA8, camera->target);
        camera->targetPosRot.pos.y = spA8.pos.y;
    } else {
        Actor_GetFocus(&camera->targetPosRot, camera->target);
    }
    if (data->rw.unk_0C != camera->target) {
        data->rw.unk_0C = camera->target;
        camera->atLERPStepScale = 0.0f;
    }

    camera->yOffsetUpdateRate =
        MMCamera_ScaledStepToCeilF(1.0f, camera->yOffsetUpdateRate, camera->speedRatio * 0.5f, 0.0001f);
    camera->xzOffsetUpdateRate =
        MMCamera_ScaledStepToCeilF(1.0f, camera->xzOffsetUpdateRate, camera->speedRatio * 0.2f, 0.0001f);

    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(.05f, camera->fovUpdateRate, camera->speedRatio * 0.05f, 0.0001f);

    if (MMCam_func_800CB950(camera)) {
        data->rw.unk_08 = focalActorPosRot->pos.y;
        sp70 = false;
    } else {
        sp70 = true;
    }

    MMCam_CalcAtForFriendlyLockOn(camera, &atToEyeNext, &camera->targetPosRot.pos, data->ro.yOffset, data->ro.unk_08,
                                  &data->rw.unk_08, &spD0, data->ro.interfaceFlags | (sp70 ? MM_KEEPON1_FLAG_7 : 0));

    sp124 = focalActorPosRot->pos;
    sp124.y += focalActorHeight;

    spD0 = MMOLib_Vec3fDiffToVecGeo(&sp124, &camera->targetPosRot.pos);

    if (spD0.r > sp114) {
        sp74 = 1.0f;
    } else {
        sp74 = spD0.r / sp114;
    }

    spE8 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    if (spE8.r < data->ro.unk_04) {
        sp114 = data->ro.unk_04;
        spF8 = 20.0f;
    } else if (spE8.r > data->ro.unk_08) {
        sp114 = data->ro.unk_08;
        spF8 = 20.0f;
    } else {
        sp114 = spE8.r;
        spF8 = 1.0f;
    }

    camera->rUpdateRateInv = MMCamera_ScaledStepToCeilF(spF8, camera->rUpdateRateInv, .5f, 0.1f);
    spE8.r = spF8 = camera->dist =
        MMCamera_ScaledStepToCeilF(sp114, camera->dist, 1.0f / camera->rUpdateRateInv, 0.1f);

    sp118 = camera->targetPosRot.pos;
    spD8 = MMOLib_Vec3fDiffToVecGeo(at, &sp118);

    spD8.r = spF8 - (((spD8.r <= spF8) ? spD8.r : spF8) * 0.5f);
    spE8.r = camera->dist = MMCamera_ScaledStepToCeilF(spF8, camera->dist, 0.06f, 0.1f);

    spFC = MM_F32_LERPIMP(data->ro.unk_0C, data->ro.unk_10, 1.1f - sp74);

    spE8.yaw = atToEyeNext.yaw;
    spF2 = spD8.yaw - (s16)(atToEyeNext.yaw + 0x8000);

    if (data->rw.unk_16 != 0) {
        if (data->rw.unk_16 > 0) {
            sp72 = data->rw.unk_16 - 1;
            spF2 = spD0.yaw;
            spD0 = MMOLib_Vec3fDiffToVecGeo(at, eye);

            spD0.yaw = spF2 + 0x8000;

            spF8 = (data->rw.unk_00 - spD0.r) * (1.0f / 6.0f);
            spF2 = (s16)(data->rw.unk_12 - spD0.yaw) * (1.0f / 6.0f);
            spF0 = (s16)(data->rw.unk_14 - spD0.pitch) * (1.0f / 6.0f);

            spE8.r = MMCamera_ScaledStepToCeilF(spD0.r + (spF8 * sp72), atToEye.r, .5f, 0.1f);
            spE8.yaw = MMCamera_ScaledStepToCeilS(spD0.yaw + (spF2 * sp72), atToEye.yaw, .5f, 5);
            spE8.pitch = MMCamera_ScaledStepToCeilS(spD0.pitch + (spF0 * sp72), atToEye.pitch, .5f, 5);
        }
        skipEyeAtCalc = true;
        data->rw.unk_16--;
    } else if (ABS(spF2) > MM_CAM_DEG_TO_BINANG(spFC)) {
        sp104 = MM_CAM_BINANG_TO_DEG(spF2);
        temp_f2_3 = ((OLib_ClampMaxDist(spD8.r, spE8.r) / spE8.r) * ((spFC + 10.0f) - spFC)) + spFC;
        temp_f2_3 = (SQ(temp_f2_3) - 2.0f) / (temp_f2_3 - 360.0f);
        var2 = (temp_f2_3 * sp104) + (2.0f - (360.0f * temp_f2_3));
        temp_f2_3 = SQ(sp104) / var2;

        if (spF2 >= 0) {
            spF0 = MM_CAM_DEG_TO_BINANG(temp_f2_3);
        } else {
            spF0 = -MM_CAM_DEG_TO_BINANG(temp_f2_3);
        }
        spE8.yaw = (s16)((s16)(atToEyeNext.yaw + 0x8000) + spF0) + 0x8000;
    } else {
        sp104 = (1.0f - camera->speedRatio) * 0.05f;
        spF0 = (spF2 >= 0) ? MM_CAM_DEG_TO_BINANG(spFC) : -MM_CAM_DEG_TO_BINANG(spFC);

        spE8.yaw = atToEyeNext.yaw - MM_TRUNCF_BINANG((spF0 - spF2) * sp104);
    }

    if (!skipEyeAtCalc) {
        spF2 = MM_CAM_DEG_TO_BINANG(MM_F32_LERPIMP(data->ro.unk_14, data->ro.unk_18, sp74));
        spF2 -= MM_TRUNCF_BINANG((spD0.pitch * (0.5f + (sp74 * 0.5f))) + 0.5f);

        spF8 = spD8.pitch * data->ro.unk_1C;
        spF2 += MM_TRUNCF_BINANG(spF8);

        if (spF2 < -0x3200) {
            spF2 = -0x3200;
        } else if (spF2 > 0x3200) {
            spF2 = 0x3200;
        }

        spE8.pitch = MMCamera_ScaledStepToCeilS(spF2, atToEyeNext.pitch, 1.0f / 9.0f, 5);
        *eyeNext = MMOLib_AddVecGeoToVec3f(at, &spE8);
        sp7C.pos = *eyeNext;

        if (camera->status == CAM_STAT_ACTIVE) {
            if (!(data->ro.interfaceFlags & MM_KEEPON1_FLAG_4)) {
                if (!camera->play->envCtx.skyboxDisabled || (data->ro.interfaceFlags & MM_KEEPON1_FLAG_0)) {
                    if (MMCam_func_800CBC84(camera, at, &sp7C, 0) != 0) {
                        data->rw.unk_18 |= 0x1000;
                    } else {
                        data->rw.unk_18 &= ~0x1000;
                    }
                } else if (data->ro.interfaceFlags & MM_KEEPON1_FLAG_1) {
                    MMCam_func_800CBC84(camera, at, &sp7C, 3);
                } else {
                    sp130 = MMOLib_Vec3fDistNormalize(at, &sp7C.pos);
                    sp7C.pos.x -= sp130.x;
                    sp7C.pos.y -= sp130.y;
                    sp7C.pos.z -= sp130.z;
                }
                *eye = sp7C.pos;
                data->rw.unk_18 &= ~0x10;
            } else if (!camera->play->envCtx.skyboxDisabled || (data->ro.interfaceFlags & MM_KEEPON1_FLAG_0)) {
                if (MMCam_func_800CBC84(camera, at, &sp7C, 0) != 0) {
                    s16 screenPosX;
                    s16 screenPosY;

                    data->rw.unk_18 |= 0x1000;
                    spF8 = OLib_Vec3fDist(at, &focalActorFocus->pos);
                    spF4 = OLib_Vec3fDist(at, &sp7C.pos);
                    spF8 += (data->rw.unk_18 & 0x10) ? 40 : 0.0f;

                    Actor_GetScreenPos(camera->play, &camera->player->actor, &screenPosX, &screenPosY);

                    if ((spF4 < spF8) || ((screenPosX >= 0) && (screenPosX <= SCREEN_WIDTH) && (screenPosY >= 0) &&
                                          (screenPosY <= SCREEN_HEIGHT))) {
                        data->rw.unk_18 |= 0x10;
                        spE0.yaw = (s16)(spD0.yaw + 0x8000);
                        spE0.pitch = -spD0.pitch;
                        spE0.r = 40.0f;
                        sp130 = focalActorFocus->pos;
                        sp130.y += 40.0f;
                        sp124 = MMOLib_AddVecGeoToVec3f(&sp130, &spE0);
                        MMCamera_ScaledStepToCeilVec3f(&sp124, eye, 0.15f, 0.15f, 0.2f);
                    } else if (data->rw.unk_18 & 0x10) {
                        if (OLib_Vec3fDist(&sp7C.pos, eye) < 20.0f) {
                            data->rw.unk_18 &= ~0x10;
                            *eye = sp7C.pos;
                        } else {
                            MMCamera_ScaledStepToCeilVec3f(&sp7C.pos, eye, 0.15f, 0.15f, 0.2f);
                        }
                    } else {
                        data->rw.unk_18 &= ~0x10;
                        *eye = sp7C.pos;
                    }
                } else {
                    if (data->rw.unk_18 & 0x10) {
                        if (OLib_Vec3fDist(&sp7C.pos, eye) < 20.0f) {
                            data->rw.unk_18 &= ~0x10;
                            *eye = sp7C.pos;
                        } else {
                            MMCamera_ScaledStepToCeilVec3f(&sp7C.pos, eye, 0.15f, 0.15f, 0.2f);
                        }
                    } else {
                        data->rw.unk_18 &= ~0x10;
                        *eye = sp7C.pos;
                    }
                    data->rw.unk_18 &= ~0x1000;
                }
            } else if (data->ro.interfaceFlags & MM_KEEPON1_FLAG_1) {
                data->rw.unk_18 &= ~0x10;
                MMCam_func_800CBC84(camera, at, &sp7C, 3);
                *eye = sp7C.pos;
            } else {
                data->rw.unk_18 &= ~0x10;
                sp130 = MMOLib_Vec3fDistNormalize(at, &sp7C.pos);
                sp7C.pos.x -= sp130.x;
                sp7C.pos.y -= sp130.y;
                sp7C.pos.z -= sp130.z;
                *eye = sp7C.pos;
            }
        } else {
            data->rw.unk_18 &= ~0x10;
            *eye = *eyeNext;
        }
    }

    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_20, camera->fov, camera->fovUpdateRate, 0.1f);
    camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.2f, 5);
    camera->atLERPStepScale = MMCam_ClampLerpScale(camera, sp70 ? data->ro.unk_28 : data->ro.unk_24);

    return true;
}

// MM Camera_KeepOn3 (TALK)
s32 Camera_MMKeepOn3(Camera* camera) {
    Vec3f* eye = &camera->eye;
    s32 temp_f0;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f spD8;
    Vec3f spCC;
    Vec3f spC0;
    f32 timer;
    f32 spB8;
    f32 spB4;
    f32 swingAngle;
    Actor* spA8[2];
    MMVecGeo spA0;
    MMVecGeo sp98;
    MMVecGeo sp90;
    MMVecGeo sp88;
    MMVecGeo sp80;
    Vec3f* at = &camera->at;
    s32 sp78;
    f32 phi_f14;
    PosRot* focalActorFocus = &camera->player->actor.focus;
    s16 sp6E;
    s16 sp6C;
    s16 sp6A = 0;
    s16 phi_a3;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    MMKeepOn3Data* data = &sMMCamLockOnParams.keep3;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    s32 i;

    if ((camera->target == NULL) || (camera->target->update == NULL)) {
        camera->target = NULL;
        Camera_RequestMode(camera, CAM_MODE_TARGET);
        return 1;
    }

    if (MMCamera_ReloadParams(camera)) {
        // MM view.unk164 is OoT's view.unk_124 (the frame's pending view update).
        if (camera->play->view.unk_124 == 0) {
            camera->unk_14C |= MM_CAM_STATE_DISABLE_MODE_CHANGE;
            camera->play->view.unk_124 = camera->thisIdx | 0x50;
            return 1;
        }
        camera->unk_14C &= ~MM_CAM_STATE_DISABLE_MODE_CHANGE;
    }

    camera->unk_14C &= ~MM_CAM_STATE_4;
    if (MMCamera_ReloadParams(camera)) {
        const s16* values = MMCam_GetModeValues(camera, sMMCam.runMode);

        data->ro.unk_00 =
            MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight * (0.8f - ((68.0f / focalActorHeight) * -0.2f));
        data->ro.unk_04 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_08 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_1C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_20 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_24 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.timer = MM_GET_NEXT_RO_DATA(values);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_28 = MM_GET_NEXT_SCALED_RO_DATA(values);
    }

    if (camera->animState == 10) {
        data->ro.timer >>= 1;
        if (data->ro.timer < 20) {
            data->ro.timer = 20;
        }
    }

    sp88 = MMOLib_Vec3fDiffToVecGeo(at, eye);
    sp80 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    Actor_GetFocus(&camera->targetPosRot, camera->target);
    spD8 = focalActorPosRot->pos;
    spD8.y += focalActorHeight;
    spA0 = MMOLib_Vec3fDiffToVecGeo(&spD8, &camera->targetPosRot.pos);

    if (MMCamera_ReloadParams(camera)) {
        camera->animState++;
        spA8[0] = camera->target;
        spA8[1] = &camera->player->actor;
        data->rw.unk_0C = camera->target;
        phi_f14 = (data->ro.unk_08 < spA0.r) ? 1.0f : (spA0.r / data->ro.unk_08);

        data->rw.timer = data->ro.timer;
        spB8 = ((1.0f - phi_f14) * spA0.r) / data->rw.timer;

        if (data->ro.interfaceFlags & MM_KEEPON3_FLAG_6) {
            spA0.pitch = 0;
        }

        swingAngle = MM_LERPIMP(data->ro.unk_14, data->ro.unk_18, phi_f14);
        sp98.pitch = MM_CAM_DEG_TO_BINANG(swingAngle) + MM_TRUNCF_BINANG(-(spA0.pitch * data->ro.unk_1C));
        swingAngle = MM_LERPIMP(data->ro.unk_0C, data->ro.unk_10, phi_f14);

        phi_a3 = MM_CAM_DEG_TO_BINANG(swingAngle);

        sp78 = 1;
        // (MM also treats its ACTOR_EN_TALK, an MM-only actor, like a door here.)
        if (camera->target->category == ACTORCAT_DOOR) {
            if ((s16)(camera->targetPosRot.rot.y - sp80.yaw) > 0) {
                phi_a3 = -phi_a3;
            }

            if (ABS(spA0.pitch) < 0x157C) { // 30.2 degrees
                sp98.yaw = MM_BINANG_ROT180(spA0.yaw) + phi_a3;
                sp6A = (s16)(MM_BINANG_ROT180(spA0.yaw) - phi_a3);
            } else {
                sp78 = 0;
                sp90.r = 30.0f;
                sp90.yaw = MM_BINANG_ROT180(spA0.yaw) + (phi_a3 >> 2);
                sp90.pitch = -spA0.pitch;
                spCC = focalActorFocus->pos;
                spCC.y += 30.0f;
                spC0 = MMOLib_AddVecGeoToVec3f(&spCC, &sp90);
                spCC.x = (camera->targetPosRot.pos.x + spD8.x) * 0.5f;
                spCC.y = (camera->targetPosRot.pos.y + spD8.y) * 0.5f;
                spCC.z = (camera->targetPosRot.pos.z + spD8.z) * 0.5f;
                sp98 = MMOLib_Vec3fDiffToVecGeo(&spCC, &spC0);
                if (sp98.pitch < -0x2328) { // 49.4 degrees
                    sp98.pitch = -0x2328;
                }
            }
        } else if (!(data->ro.interfaceFlags & MM_KEEPON3_FLAG_5) && (ABS((s16)(spA0.yaw - sp80.yaw)) < 0x4000)) {
            if ((s16)(spA0.yaw - sp80.yaw) > 0) {
                phi_a3 = -phi_a3;
            }
            sp98.yaw = (s16)(spA0.yaw + phi_a3);
            sp6A = (s16)(spA0.yaw - phi_a3);
        } else {
            if ((s16)(MM_BINANG_ROT180(spA0.yaw) - sp80.yaw) < 0) {
                phi_a3 = -phi_a3;
            }

            sp98.yaw = MM_BINANG_ROT180(spA0.yaw) + phi_a3;
            sp6A = MM_BINANG_ROT180(spA0.yaw) - phi_a3;
        }

        sp6C = sp98.yaw;
        sp6E = sp98.pitch;

        spB4 = spA0.r;

        phi_f14 = data->ro.unk_28;
        spA0.r = spB8 * phi_f14 + (spB4 * (1.0f - phi_f14));
        spD8 = focalActorPosRot->pos;
        spD8.y += focalActorHeight;

        data->rw.unk_10 = MMOLib_AddVecGeoToVec3f(&spD8, &spA0);
        data->rw.unk_10.y += data->ro.unk_00;
        spA0.r = spB4;

        if (sp78 != 0) {
            sp98.r = ((data->ro.unk_04 + (spA0.r * 0.5f)) - sp80.r) + sp80.r;
            i = 0;
            sp78 = 14;
            spCC = MMOLib_AddVecGeoToVec3f(&data->rw.unk_10, &sp98);
            if (!(data->ro.interfaceFlags & MM_KEEPON3_FLAG_7)) {
                if (CollisionCheck_LineOCCheck(camera->play, &camera->play->colChkCtx, &data->rw.unk_10, &spCC, spA8,
                                               2) ||
                    MMCam_BgCheck(camera, &data->rw.unk_10, &spCC)) {
                    sp98.yaw = sp6A;
                    spCC = MMOLib_AddVecGeoToVec3f(&data->rw.unk_10, &sp98);
                }
                while (i < sp78) {
                    if (!CollisionCheck_LineOCCheck(camera->play, &camera->play->colChkCtx, &data->rw.unk_10, &spCC,
                                                    spA8, 2) &&
                        !MMCam_BgCheck(camera, &data->rw.unk_10, &spCC)) {
                        break;
                    }
                    sp98.yaw = sp6C + sMMCamAngleYaws[i];
                    sp98.pitch = sp6E + sMMCamAnglePitches[i];
                    spCC = MMOLib_AddVecGeoToVec3f(&data->rw.unk_10, &sp98);
                    i++;
                }
            }
        }

        camera->unk_14C &= ~(MM_CAM_STATE_3 | MM_CAM_STATE_2);
        temp_f0 = ((data->rw.timer + 1) * data->rw.timer) >> 1;
        data->rw.unk_04 = (f32)(s16)(sp98.yaw - sp80.yaw) / temp_f0;
        data->rw.unk_08 = (f32)(s16)(sp98.pitch - sp80.pitch) / temp_f0;
        data->rw.unk_00 = (sp98.r - sp80.r) / temp_f0;
        sCameraInterfaceFlags = 0xFF00; // CAM_INTERFACE_FLAGS(CAM_LETTERBOX_IGNORE, CAM_HUD_VISIBILITY_IGNORE, 0)

        return 1;
    }

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    if (data->rw.timer != 0) {
        timer = data->rw.timer;

        at->x += (data->rw.unk_10.x - at->x) / timer;
        at->y += (data->rw.unk_10.y - at->y) / timer;
        at->z += (data->rw.unk_10.z - at->z) / timer;

        sp98.r = (data->rw.unk_00 * timer) + sp80.r + 1.0f;
        sp98.yaw = sp80.yaw + MM_TRUNCF_BINANG(data->rw.unk_04 * timer);
        sp98.pitch = sp80.pitch + MM_TRUNCF_BINANG(data->rw.unk_08 * timer);
        *eyeNext = MMOLib_AddVecGeoToVec3f(at, &sp98);
        *eye = *eyeNext;
        camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_20, camera->fov, 0.5f, 0.1f);
        camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, 0.5f, 5);
        camera->atLERPStepScale = MMCam_ClampLerpScale(camera, data->ro.unk_24);
        MMCam_func_800CBFA4(camera, at, eye, 3);
        data->rw.timer--;
    } else {
        camera->unk_14C |= (MM_CAM_STATE_10 | MM_CAM_STATE_4);
    }

    MMCam_SetFocalActorAtOffset(camera, &focalActorPosRot->pos);
    camera->dist = OLib_Vec3fDist(at, eye);
    if (camera->unk_14C & MM_CAM_STATE_3) {
        Input* input = &camera->play->state.input[0];

        sCameraInterfaceFlags = 0; // CAM_INTERFACE_FLAGS(CAM_LETTERBOX_NONE, CAM_HUD_VISIBILITY_ALL, 0)
        MMCam_SetUpdateRatesSlow(camera);
        camera->atLERPStepScale = 0.0f;

        if ((camera->xzSpeed > 0.001f) || CHECK_BTN_ALL(input->press.button, BTN_A) ||
            CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_CUP) ||
            CHECK_BTN_ALL(input->press.button, BTN_CDOWN) || CHECK_BTN_ALL(input->press.button, BTN_CLEFT) ||
            CHECK_BTN_ALL(input->press.button, BTN_CRIGHT) || CHECK_BTN_ALL(input->press.button, BTN_Z) ||
            CHECK_BTN_ALL(input->press.button, BTN_L) || CHECK_BTN_ALL(input->press.button, BTN_R)) {
            camera->unk_14C |= MM_CAM_STATE_2;
            camera->unk_14C &= ~MM_CAM_STATE_3;
        }
    }

    if (data->rw.unk_0C != camera->target) {
        data->rw.unk_0C = camera->target;
        camera->animState = 10;
    }

    return 1;
}

// MM Camera_Jump3 (the form modes of MM's NORMAL3 setting: GORONDASH, DEKUFLY, DEKUHIDE, GORONJUMP)
s32 Camera_MMJump3(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    f32 spD0;
    f32 spCC;
    PosRot* focalActorPosRot = &camera->playerPosRot;
    f32 spC0;
    Vec3f spB4;
    MMVecGeo spAC;
    const s16* values;
    MMVecGeo sp9C;
    MMVecGeo sp94;
    f32 temp_f0;
    MMJump3Data* data = &sMMCam.params.jump3;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);
    PosRot focalActorFocus;
    f32 sp60 = camera->waterYPos - eye->y;
    f32 sp5C;
    s32 sp58 = false;

    Actor_GetFocus(&focalActorFocus, &camera->player->actor);

    if (MMCamera_ReloadParams(camera)) {
        data->rw.unk_0A = sMMCam.runMode;
        data->rw.timer2 = 0;
    }

    if (sMMCam.runMode == MMCAM_MODE_NORMAL) {
        if ((camera->player->actor.bgCheckFlags & 0x10) || (data->rw.timer2 != 0)) {
            if (data->rw.unk_0A != 0xF) {
                data->rw.unk_0A = 0xF;
                sp58 = true;
                data->rw.timer2 = 10;
            }
        } else if (sp60 < 50.0f) {
            if (data->rw.unk_0A != 0) {
                data->rw.unk_0A = 0;
                sp58 = true;
            }
        } else if (MMCam_fabsf(camera->playerPosRot.pos.y - camera->playerGroundY) < 11.0f) {
            if (data->rw.unk_0A != 5) {
                data->rw.unk_0A = 5;
                sp58 = true;
            }
        } else if ((sp60 > 250.0f) && (data->rw.unk_0A != 0x1A)) {
            data->rw.unk_0A = 0x1A;
            sp58 = true;
        }
    }

    if (data->rw.timer2 != 0) {
        data->rw.timer2--;
    }

    sp9C = MMOLib_Vec3fDiffToVecGeo(at, eye);
    sp94 = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);

    if (MMCamera_ReloadParams(camera) || sp58) {
        values = MMCam_GetModeValues(camera, data->rw.unk_0A);

        sp5C = 0.8f - (-0.2f * (68.0f / focalActorHeight));
        spD0 = focalActorHeight * 0.01f * sp5C;

        data->ro.unk_00 = MM_GET_NEXT_RO_DATA(values) * spD0;
        data->ro.unk_04 = MM_GET_NEXT_RO_DATA(values) * spD0;
        data->ro.unk_08 = MM_GET_NEXT_RO_DATA(values) * spD0;
        data->ro.unk_20 = MM_CAM_DEG_TO_BINANG(MM_GET_NEXT_RO_DATA(values));
        data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_14 = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values);
        data->ro.unk_1C = MM_GET_NEXT_SCALED_RO_DATA(values);
        data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);
    }

    sCameraInterfaceFlags = data->ro.interfaceFlags;

    switch (camera->animState) {
        case 0:
            data->rw.unk_10 = 0x1000;
            // fallthrough
        case 10:
        case 20:
            data->rw.unk_00 = camera->playerGroundY;
            sMMCam.swing.yaw = sMMCam.swing.pitch = sMMCam.swing.unk_64 = 0;
            data->rw.timer1 = 10;
            sMMCam.swing.swingUpdateRate = data->ro.unk_0C;
            camera->animState++;
            sMMCam.swing.timer = 0;
            break;

        default:
            if (data->rw.timer1 != 0) {
                data->rw.timer1--;
            }
            break;
    }

    spC0 = focalActorFocus.pos.y - focalActorPosRot->pos.y;
    spB4 = *eye;

    spD0 = camera->speedRatio * 0.5f;
    spCC = camera->speedRatio * 0.2f;

    temp_f0 = (sMMCam.swing.unk_64 == 1) ? 0.5f : spD0;

    if (sMMCam.swing.timer != 0) {
        camera->yawUpdateRateInv = MMCamera_ScaledStepToCeilF(
            (sMMCam.swing.swingUpdateRate + sMMCam.swing.timer * 2), camera->yawUpdateRateInv, spD0, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF((40.0f + sMMCam.swing.timer * 2),
                                                                camera->pitchUpdateRateInv, spCC, 0.1f);
        sMMCam.swing.timer--;
    } else {
        camera->yawUpdateRateInv =
            MMCamera_ScaledStepToCeilF(sMMCam.swing.swingUpdateRate, camera->yawUpdateRateInv, temp_f0, 0.1f);
        camera->pitchUpdateRateInv = MMCamera_ScaledStepToCeilF(40.0f, camera->pitchUpdateRateInv, spCC, 0.1f);
    }

    if (data->ro.interfaceFlags & MM_JUMP3_FLAG_7) {
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.01f, camera->yOffsetUpdateRate, spD0, 0.0001f);
        sp5C = sqrtf((camera->playerPosDelta.x * camera->playerPosDelta.x) +
                     (camera->playerPosDelta.z * camera->playerPosDelta.z)) /
               MMCam_GetRunSpeedLimit(camera);
        camera->speedRatio = OLib_ClampMaxDist(sp5C / MMCam_GetRunSpeedLimit(camera), 1.8f);
        spCC = camera->speedRatio * 0.2f;
    } else {
        camera->yOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->yOffsetUpdateRate, spD0, 0.0001f);
    }

    camera->xzOffsetUpdateRate = MMCamera_ScaledStepToCeilF(0.05f, camera->xzOffsetUpdateRate, spCC, 0.0001f);
    camera->fovUpdateRate =
        MMCamera_ScaledStepToCeilF(0.050f, camera->fovUpdateRate, camera->speedRatio * 0.05f, 0.0001f);

    if (sp60 < 50.0f) {
        sp5C = camera->waterYPos - spC0;

        MMCam_CalcAtForScreen(camera, &sp94, data->ro.unk_00, &sp5C,
                              ((sp60 < 0.0f) ? 1.0f : 1.0f - (sp60 / 50.0f)) * 50.0f);
    } else {
        MMCam_CalcAtDefault(camera, &sp94, data->ro.unk_00, data->ro.interfaceFlags);
    }

    spAC = MMOLib_Vec3fDiffToVecGeo(at, eyeNext);
    spAC.r = MMCam_ClampDist1(camera, spAC.r, data->ro.unk_04, data->ro.unk_08, data->rw.timer1);
    camera->dist = spAC.r;

    if (!(MMCam_fabsf(focalActorPosRot->pos.y - camera->playerGroundY) < 10.0f) &&
        !(MMCam_fabsf(focalActorFocus.pos.y - camera->waterYPos) < 50.f)) {
        camera->pitchUpdateRateInv = 100.0f;
    }

    if (data->ro.interfaceFlags & MM_JUMP3_FLAG_5) {
        spD0 = CLAMP_MAX(camera->speedRatio * 1.3f, 0.6f);

        spAC.pitch = MMCamera_ScaledStepToCeilS(
            (spAC.pitch * spD0) + (data->ro.unk_20 * (1.0f - spD0)), sp94.pitch,
            1.0f / (spCC = ((camera->pitchUpdateRateInv + 1.0f) - (camera->pitchUpdateRateInv * spD0))), 5);
    } else if (sMMCam.swing.unk_64 == 1) {
        spAC.yaw = MMCamera_ScaledStepToCeilS(sMMCam.swing.yaw, sp94.yaw, 1.0f / camera->yawUpdateRateInv, 5);
        // (MM steps the pitch with yawUpdateRateInv too)
        spAC.pitch = MMCamera_ScaledStepToCeilS(sMMCam.swing.pitch, sp94.pitch, 1.0f / camera->yawUpdateRateInv, 5);
    } else if (data->ro.interfaceFlags & (MM_JUMP3_FLAG_7 | MM_JUMP3_FLAG_3)) {
        spAC.yaw = MMCam_CalcDefaultYaw(camera, sp94.yaw, focalActorPosRot->rot.y, data->ro.unk_14, 0.0f);

        spD0 = CLAMP_MAX(camera->speedRatio * 1.3f, 1.0f);

        spAC.pitch = MMCamera_ScaledStepToCeilS(
            (spAC.pitch * spD0) + (data->ro.unk_20 * (1.0f - spD0)), sp94.pitch,
            1.0f / (spCC = (camera->pitchUpdateRateInv + 1.0f) - (camera->pitchUpdateRateInv * spD0)), 5);
    } else {
        spAC.yaw = MMCam_CalcDefaultYaw(camera, sp94.yaw, focalActorPosRot->rot.y, data->ro.unk_14, 0.0f);
        spAC.pitch = MMCam_CalcDefaultPitch(camera, sp94.pitch, data->ro.unk_20, 0);
    }

    if (spAC.pitch > MM_DEG_TO_BINANG(79.655f)) {
        spAC.pitch = MM_DEG_TO_BINANG(79.655f);
    }
    if (spAC.pitch < -MM_DEG_TO_BINANG(29.995f)) {
        spAC.pitch = -MM_DEG_TO_BINANG(29.995f);
    }

    *eyeNext = MMOLib_AddVecGeoToVec3f(at, &spAC);

    if ((camera->status == CAM_STAT_ACTIVE) && !(data->ro.interfaceFlags & MM_JUMP3_FLAG_6)) {
        if (MMCam_func_800CBA7C(camera) == 0) {
            MMCam_CalcDefaultSwing(camera, &spAC, &sp9C, data->ro.unk_04, data->ro.unk_0C, &sMMCam.swing,
                                   &data->rw.unk_10);
        }

        if (data->ro.interfaceFlags & MM_JUMP3_FLAG_2) {
            camera->inputDir.x = -sp9C.pitch;
            camera->inputDir.y = sp9C.yaw + 0x8000;
            camera->inputDir.z = 0;
        } else {
            spAC = MMOLib_Vec3fDiffToVecGeo(eye, at);
            camera->inputDir.x = spAC.pitch;
            camera->inputDir.y = spAC.yaw;
            camera->inputDir.z = 0;
        }
    } else {
        sMMCam.swing.swingUpdateRate = data->ro.unk_0C;
        sMMCam.swing.unk_64 = 0;
        sUpdateCameraDirection = false;
        *eye = *eyeNext;
    }

    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_18, camera->fov, camera->fovUpdateRate, 0.1f);
    camera->roll = MMCamera_ScaledStepToCeilS(0, camera->roll, .5f, 5);
    camera->atLERPStepScale = MMCam_ClampLerpScale(camera, data->ro.unk_1C);

    (void)spB4;
    return 1;
}

// MM Camera_Subject1 (DEKUSHOOT, ZORAFIN)
s32 Camera_MMSubject1(Camera* camera) {
    Vec3f* eye = &camera->eye;
    Vec3f* at = &camera->at;
    Vec3f* eyeNext = &camera->eyeNext;
    Vec3f sp90;
    Vec3f sp84;
    MMVecGeo sp7C;
    MMVecGeo sp74;
    MMVecGeo tgeo;
    PosRot focalActorFocus;
    f32 sp54;
    f32 temp_f0_2;
    s16 sp4E;
    s16 sp4C;
    MMSubject1Data* data = &sMMCam.params.subj1;
    const s16* values;
    f32 temp_f0;
    f32 focalActorHeight = MMCam_GetFocalActorHeight(camera);

    Actor_GetFocus(&focalActorFocus, &camera->player->actor);

    MMCam_SetUpdateRatesFastPitch(camera);

    values = MMCam_GetModeValues(camera, sMMCam.runMode);
    data->ro.unk_00 = MM_GET_NEXT_SCALED_RO_DATA(values) * focalActorHeight;
    data->ro.unk_04 = MM_GET_NEXT_RO_DATA(values);
    data->ro.unk_08 = MM_GET_NEXT_RO_DATA(values);
    data->ro.unk_0C = MM_GET_NEXT_RO_DATA(values);
    data->ro.unk_10 = MM_GET_NEXT_RO_DATA(values) * 0.1f;
    data->ro.unk_14 = MM_GET_NEXT_RO_DATA(values) * 0.1f;
    data->ro.unk_18 = MM_GET_NEXT_RO_DATA(values) * 0.1f;
    data->ro.unk_1C = MM_GET_NEXT_RO_DATA(values);
    data->ro.interfaceFlags = MM_GET_NEXT_RO_DATA(values);

    sp7C.r = data->ro.unk_08;
    sp7C.yaw = MM_BINANG_ROT180(focalActorFocus.rot.y);
    sp7C.pitch = focalActorFocus.rot.x;
    sp90 = focalActorFocus.pos;
    sp90.y += data->ro.unk_00;

    sp84 = MMOLib_AddVecGeoToVec3f(&sp90, &sp7C);
    sp74 = MMOLib_Vec3fDiffToVecGeo(&camera->at, eye);
    sCameraInterfaceFlags = data->ro.interfaceFlags;

    // Update again after the player is drawn (Play_Draw's view.unk_124 camera update, MM view.unk164).
    if (camera->play->view.unk_124 == 0) {
        camera->play->view.unk_124 = camera->thisIdx | 0x50;
        return true;
    }

    if (MMCamera_ReloadParams(camera)) {
        data->rw.unk_00 = sp74.r;
        data->rw.unk_04 = sp74.yaw;
        data->rw.unk_06 = sp74.pitch;

        if (data->ro.interfaceFlags & MM_SUBJECT1_FLAG_4) {
            camera->fov = data->ro.unk_1C;
            data->rw.timer = 0;
        } else {
            data->rw.timer = 6;
        }
        camera->dist = data->ro.unk_08;
        camera->rUpdateRateInv = 1.0f;
        camera->animState++;
        camera->dist = data->ro.unk_08;
    }

    tgeo.r = data->rw.unk_00;
    tgeo.pitch = data->rw.unk_04;
    tgeo.yaw = data->rw.unk_06;

    if (data->rw.timer != 0) {
        temp_f0 = (1.0f / data->rw.timer);

        // VEC3F_LERPIMPDST(at, at, &sp90, temp_f0)
        at->x = at->x + (sp90.x - at->x) * temp_f0;
        at->y = at->y + (sp90.y - at->y) * temp_f0;
        at->z = at->z + (sp90.z - at->z) * temp_f0;

        sp54 = ((tgeo.r - sp7C.r) * (1.0f / 6.0f));
        sp4E = MM_BINANG_SUB(tgeo.pitch, sp7C.yaw) * (1.0f / 6.0f);
        sp4C = MM_BINANG_SUB(tgeo.yaw, sp7C.pitch) * (1.0f / 6.0f);

        sp74.r = MMCamera_ScaledStepToCeilF((data->rw.timer * sp54) + sp7C.r, sp74.r, 0.5f, 1.0f);
        sp74.yaw = MMCamera_ScaledStepToCeilS(sp7C.yaw + (sp4E * data->rw.timer), sp74.yaw, 0.5f, 5);
        sp74.pitch = MMCamera_ScaledStepToCeilS(sp7C.pitch + (sp4C * data->rw.timer), sp74.pitch, 0.5f, 5);

        *eyeNext = MMOLib_AddVecGeoToVec3f(at, &sp74);
        *eye = *eyeNext;
        data->rw.timer--;
    } else {
        sp54 = Math_SinS(-focalActorFocus.rot.x);
        temp_f0_2 = Math_CosS(-focalActorFocus.rot.x);

        sp90.x = data->ro.unk_10;
        sp90.y = (data->ro.unk_14 * temp_f0_2) - (data->ro.unk_18 * sp54);
        sp90.z = (data->ro.unk_14 * sp54) + (data->ro.unk_18 * temp_f0_2);

        sp54 = Math_SinS(MM_BINANG_ROT180(focalActorFocus.rot.y));
        temp_f0_2 = Math_CosS(MM_BINANG_ROT180(focalActorFocus.rot.y));

        data->ro.unk_10 = (sp90.z * sp54) + (sp90.x * temp_f0_2);
        data->ro.unk_14 = sp90.y;
        data->ro.unk_18 = (sp90.z * temp_f0_2) - (sp90.x * sp54);

        at->x = data->ro.unk_10 + focalActorFocus.pos.x;
        at->y = data->ro.unk_14 + focalActorFocus.pos.y;
        at->z = data->ro.unk_18 + focalActorFocus.pos.z;

        sp74.r = data->ro.unk_08;
        sp74.yaw = MM_BINANG_ROT180(focalActorFocus.rot.y);
        sp74.pitch = focalActorFocus.rot.x;

        camera->eyeNext = MMOLib_AddVecGeoToVec3f(at, &sp74);
        sp74.r = data->ro.unk_04;
        *eye = MMOLib_AddVecGeoToVec3f(at, &sp74);
    }

    if (camera->play->envCtx.skyboxDisabled == 0) {
        MMCam_BgCheck(camera, at, eye);
    } else {
        MMCam_func_800CBFA4(camera, at, eye, 3);
    }

    camera->fov = MMCamera_ScaledStepToCeilF(data->ro.unk_1C, camera->fov, 0.25f, 0.1f);
    camera->roll = 0;
    camera->atLERPStepScale = 0.0f;

    (void)sp84;
    return true;
}

/*--------------------------------------------------------------------------------------------------
 * Engine: MM mode changes and the per-frame hooks called from z_camera.c
 *------------------------------------------------------------------------------------------------*/

static void MMCam_RenewRequest(Camera* camera) {
    sMMCam.requestFrame = camera->play->state.frames;
}

// MM Camera_ChangeModeFlags for an MM-only mode on the main camera. The current mode is the active MM mode
// if there is one, else OoT's camera->mode.
s32 MergeShip_CameraMM_ChangeMode(Camera* camera, s16 mmMode, u8 forceChange) {
    const MMCamSetting* setting;
    s32 modeChangeFlags;

    if ((camera == NULL) || (camera->thisIdx != CAM_ID_MAIN) || (camera->player == NULL) || (mmMode < 0) ||
        (mmMode >= MM_CAM_MODE_TABLE_SIZE)) {
        return -1;
    }

    setting = MMCam_GetSetting(camera);

    // Mode change rejected by flag
    if ((camera->unk_14C & MM_CAM_STATE_DISABLE_MODE_CHANGE) && !forceChange) {
        camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_VALID;
        if (sMMCam.mode != MM_CAM_MODE_NONE) {
            MMCam_RenewRequest(camera); // the current MM mode keeps running
        }
        return -1;
    }

    // Mode change rejected by validModes
    if (!(setting->validModes & (1 << mmMode)) || (setting->modes[mmMode].funcId == MMCAM_FUNC_NONE)) {
        if ((sMMCam.mode != MM_CAM_MODE_NONE) || (camera->mode != CAM_MODE_NORMAL)) {
            sMMCam.mode = MM_CAM_MODE_NONE;
            camera->mode = CAM_MODE_NORMAL;
            Camera_CopyDataToRegs(camera, camera->mode); // MM Camera_ResetActionFuncState
            func_8005A02C(camera);                       // MM func_800DF498
            return mmMode | 0xC0000000;
        } else {
            camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_VALID;
            camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_1;
            return 0;
        }
    }

    // Mode change rejected due to mode already being set
    if ((mmMode == sMMCam.mode) && !forceChange) {
        camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_VALID;
        MMCam_RenewRequest(camera);
        return -1;
    }

    camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_VALID;
    camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_1;

    camera->animState = 0; // MM Camera_ResetActionFuncState

    // The requested mode is MM-only: none of MM's requested-mode cases apply. Current mode:
    modeChangeFlags = 0;
    if (sMMCam.mode == MM_CAM_MODE_NONE) {
        switch (camera->mode) {
            case CAM_MODE_JUMP:
            case CAM_MODE_HANG:
            case CAM_MODE_CHARGE:
            case CAM_MODE_FOLLOWTARGET:
            case CAM_MODE_BATTLE:
            case CAM_MODE_Z_AIM: // MM CAM_MODE_BOWARROWZ
            case CAM_MODE_HANGZ:
            case CAM_MODE_PUSHPULL:
                modeChangeFlags |= 1; // MM CAM_CHANGE_MODE_0
                break;

            default:
                break;
        }
    }

    if ((camera->status == CAM_STAT_ACTIVE) && (modeChangeFlags == 1)) {
        Sfx_PlaySfxCentered(0);
    }

    func_8005A02C(camera); // MM func_800DF498
    sMMCam.mode = mmMode;
    MMCam_RenewRequest(camera);

    return mmMode | 0x80000000;
}

// Camera_RequestModeImpl hook: an OoT mode is requested. Returns true when MM rejects the change (the
// MM mode stays); otherwise the MM mode is left and OoT's code continues as a change away from a mode
// none of its cases name, which is what MM's Camera_ChangeModeFlags does for an MM-only current mode.
s32 MergeShip_CameraMM_OnRequestMode(Camera* camera, u8* forceModeChange) {
    if ((camera->thisIdx != CAM_ID_MAIN) || (sMMCam.mode == MM_CAM_MODE_NONE)) {
        return false;
    }

    if ((camera->unk_14C & MM_CAM_STATE_DISABLE_MODE_CHANGE) && !*forceModeChange) {
        camera->unk_14A |= MM_CAM_BEHAVIOR_MODE_VALID;
        MMCam_RenewRequest(camera);
        return true;
    }

    sMMCam.mode = MM_CAM_MODE_NONE;
    camera->mode = -1;        // the MM mode: never equal to the requested mode, and not NORMAL
    *forceModeChange = true; // (the rejection by flag was checked above)
    return false;
}

// Camera_ChangeSettingFlags hook, where MM calls Camera_ChangeModeFlags(camera, camera->mode, true).
// Returns true when an MM mode was re-checked against the new setting (OoT's call is then skipped).
s32 MergeShip_CameraMM_OnSettingChanged(Camera* camera) {
    if (camera->thisIdx != CAM_ID_MAIN) {
        return false;
    }
    if (camera->setting != CAM_SET_PIVOT_WATER_SURFACE) {
        sMMCam.zoraDiving = false;
    }
    if (sMMCam.mode == MM_CAM_MODE_NONE) {
        return false;
    }
    MergeShip_CameraMM_ChangeMode(camera, sMMCam.mode, true);
    return true;
}

// Camera_UpdateWater hook, after the dive switches to CAM_SET_PIVOT_WATER_SURFACE. MM Camera_UpdateWater:
// a diving Zora gets CAM_SET_ZORA_DIVING (Camera_Normal3, following the Zora's swim direction) instead of
// the PIVOT_DIVING setting everyone else gets.
void MergeShip_CameraMM_OnDive(Camera* camera) {
    if ((camera->thisIdx == CAM_ID_MAIN) && (camera->player != NULL) &&
        (camera->player->mergeShipForm == PLAYER_FORM_ZORA) && (camera->setting == CAM_SET_PIVOT_WATER_SURFACE) &&
        !sMMCam.zoraDiving) {
        sMMCam.zoraDiving = true;
        sMMCam.mode = MM_CAM_MODE_NONE; // ZORA_DIVING holds no form mode (MM forces CAM_MODE_NORMAL)
        camera->mode = CAM_MODE_NORMAL;
        Camera_CopyDataToRegs(camera, camera->mode);
        func_8005A02C(camera);
    }
}

// Camera_Update hook, before camera->playerPosRot takes the new position: MM Camera_Update's focal actor
// speed (3D, dyna-mesh motion removed, limit = MM run speed limit * 1.5, ratio up to 1.8) and MM's
// waterYPos (the water surface above the player, MM Camera_UpdateWater), which the MM functions read
// where OoT computes these differently.
void MergeShip_CameraMM_TrackFocalActor(Camera* camera, PosRot* curPlayerPosRot, s32 floorFound) {
    DynaPolyActor* meshActor;
    f32 runSpeedLimit;
    f32 speed;
    f32 waterY;
    WaterBox* waterBox;

    if ((camera->thisIdx != CAM_ID_MAIN) || (camera->player == NULL)) {
        return;
    }

    sMMCam.unk_0F0.x = curPlayerPosRot->pos.x - camera->playerPosRot.pos.x;
    sMMCam.unk_0F0.y = curPlayerPosRot->pos.y - camera->playerPosRot.pos.y;
    sMMCam.unk_0F0.z = curPlayerPosRot->pos.z - camera->playerPosRot.pos.z;

    if (floorFound && (MMCam_fabsf(camera->playerPosRot.pos.y - camera->playerGroundY) < 11.0f)) {
        meshActor = DynaPoly_GetActor(&camera->play->colCtx, camera->bgCheckId);
        if (meshActor != NULL) {
            sMMCam.unk_0F0.x -= meshActor->actor.world.pos.x - sMMCam.meshActorPos.x;
            sMMCam.unk_0F0.y -= meshActor->actor.world.pos.y - sMMCam.meshActorPos.y;
            sMMCam.unk_0F0.z -= meshActor->actor.world.pos.z - sMMCam.meshActorPos.z;
            sMMCam.meshActorPos = meshActor->actor.world.pos;
        }
    }

    runSpeedLimit = MMCam_GetRunSpeedLimit(camera) * 1.5f;
    speed = MMCam_Vec3fMagnitude(&sMMCam.unk_0F0);
    sMMCam.xzSpeed = OLib_ClampMaxDist(speed, runSpeedLimit);
    sMMCam.speedRatio = OLib_ClampMaxDist(speed / runSpeedLimit, 1.8f);

    // MM Camera_GetWaterBoxBgCamSetting's waterY (not refreshed while diving, MM CAM_STATE_15)
    if (!(camera->unk_14C & (s16)0x8000)) {
        waterY = curPlayerPosRot->pos.y;
        if (!WaterBox_GetSurface1(camera->play, &camera->play->colCtx, curPlayerPosRot->pos.x,
                                  curPlayerPosRot->pos.z, &waterY, &waterBox)) {
            waterY = curPlayerPosRot->pos.y;
        }
        sMMCam.waterYPos = waterY;
    }
}

// OoT Camera_Normal1 hooks for the MM forms (CAM_MODE_NORMAL of the free-roaming settings runs OoT's
// Camera_Normal1). OoT's Normal1 turns the eye toward the player's *facing*, camera->playerPosRot.rot.y
// (shape.rot.y), and when the player stands still it swings the eye behind that facing. MM's
// Camera_Normal1 does neither: every MM Normal1 mode's data sets NORMAL1_FLAG_1 (all 79 CAM_FUNCDATA_NORM1 entries
// in MM's z_camera_data.inc), whose branch steers the yaw toward the focal actor's direction of *travel*
// (camera->unk_0F0, its displacement this frame) and only while it moves (speedRatio > 0.1), and holds
// yaw and pitch otherwise; MM's Normal1 has no idle swing at all. MM forms spin their shape.rot.y
// (Deku spin, MM Player_Action_95: 19200 binang/frame), which OoT's target turned into a +-6 deg/frame
// camera wobble.

// OoT Camera_Normal1's idle swing (startSwingTimer) is not started for an MM form.
s32 MergeShip_CameraMM_Normal1NoIdleSwing(Camera* camera) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMFormCamera"), 1) && (camera->thisIdx == CAM_ID_MAIN) &&
           (camera->player != NULL) && MERGESHIP_FORM_IS_MM(camera->player->mergeShipForm);
}

// MM Camera_Normal1's NORMAL1_FLAG_1 yaw/pitch branch, in place of OoT Camera_Normal1's "follow the player's
// facing" branch, for an MM form. Returns false for Link (OoT's branch runs).
s32 MergeShip_CameraMM_Normal1FollowTravel(Camera* camera, VecSph* eyeAdjustment, VecSph* atEyeNextGeo,
                                          f32 yawDiffRange, f32 accel, s16 pitchTarget, s16 slopePitchAdj) {
    MMVecGeo travel;

    if (!MergeShip_CameraMM_Normal1NoIdleSwing(camera)) {
        return false;
    }

    // MM: if ((camera->speedRatio > 0.1f) || (rwData->unk_0A > 0x4B0)). unk_0A exceeds 0x4B0 only in MM
    // Normal1's first 100 frames with the actor still, where speedRatio ~0 makes CalcDefaultYaw a no-op.
    if (sMMCam.speedRatio > 0.1f) {
        travel = MMOLib_Vec3fToVecGeo(&sMMCam.unk_0F0);
        eyeAdjustment->yaw = Camera_CalcDefaultYaw(camera, atEyeNextGeo->yaw, travel.yaw, yawDiffRange, accel);
        eyeAdjustment->pitch = Camera_CalcDefaultPitch(camera, atEyeNextGeo->pitch, pitchTarget, slopePitchAdj);
    } else {
        eyeAdjustment->yaw = atEyeNextGeo->yaw;
        eyeAdjustment->pitch = atEyeNextGeo->pitch;
    }
    return true;
}

// Switching between OoT's camera function and MM's without a mode change (the player became or stopped being
// an MM form, a setting MM shares was entered or left in the same mode): the new function starts from its
// initial state, as after any mode change (their state in camera->paramData / sMMCam.params is not shared).
static void MMCam_SetFuncRan(Camera* camera, u8 mmFuncRan) {
    if (sMMCam.mmFuncRan != mmFuncRan) {
        sMMCam.mmFuncRan = mmFuncRan;
        camera->animState = 0;
    }
}

static void MMCam_LeaveMode(Camera* camera) {
    // As MM's Camera_ChangeModeFlags from the MM mode: OoT's mode restarts from animState 0.
    sMMCam.mode = MM_CAM_MODE_NONE;
    Camera_CopyDataToRegs(camera, camera->mode);
    func_8005A02C(camera);
}

// Camera_Update hook, in place of the sCameraFunctions dispatch. Returns true when an MM function ran.
s32 MergeShip_CameraMM_Update(Camera* camera) {
    const MMCamModeData* modeData;
    Vec3f savedPosDelta;
    f32 savedXZSpeed;
    f32 savedSpeedRatio;
    f32 savedWaterYPos;
    s32 funcId;

    if ((camera->thisIdx != CAM_ID_MAIN) || (camera->player == NULL)) {
        return false;
    }

    if (sMMCam.zoraDiving && (camera->setting != CAM_SET_PIVOT_WATER_SURFACE)) {
        sMMCam.zoraDiving = false;
    }

    if (sMMCam.zoraDiving) {
        funcId = MMCAM_FUNC_NORMAL3;
        sMMCam.runMode = MMCAM_MODE_NORMAL;
    } else {
        s16 sharedMode;

        modeData = NULL;
        if (sMMCam.mode != MM_CAM_MODE_NONE) {
            // The player renews its request every frame it wants the mode (MM calls Camera_ChangeMode each
            // frame). A frame without one -- the player did not update, or asked for nothing -- hands the
            // camera back to OoT's mode; a frozen player (hit stop) keeps it, as MM's would.
            if ((sMMCam.requestFrame != camera->play->state.frames) && (camera->player->actor.freezeTimer == 0)) {
                MMCam_LeaveMode(camera);
            } else if (MMCam_GetSetting(camera)->modes[sMMCam.mode].funcId == MMCAM_FUNC_NONE) {
                MMCam_LeaveMode(camera);
            } else {
                modeData = &MMCam_GetSetting(camera)->modes[sMMCam.mode];
                sMMCam.runMode = sMMCam.mode;
            }
        }

        if (modeData == NULL) {
            // One of OoT's modes: an MM form runs MM's function for it with MM's data, in the settings MM
            // shares with OoT (MM's Camera_Update: sCameraFunctions[sCameraSettings[setting].cameraModes[mode]]).
            sharedMode = MMCam_GetSharedRunMode(camera, &modeData);
            if (sharedMode < 0) {
                MMCam_SetFuncRan(camera, false);
                return false;
            }
            sMMCam.runMode = sharedMode;
        }
        funcId = modeData->funcId;
    }
    MMCam_SetFuncRan(camera, true);

    savedPosDelta = camera->playerPosDelta;
    savedXZSpeed = camera->xzSpeed;
    savedSpeedRatio = camera->speedRatio;
    savedWaterYPos = camera->waterYPos;
    camera->playerPosDelta = sMMCam.unk_0F0;
    camera->xzSpeed = sMMCam.xzSpeed;
    camera->speedRatio = sMMCam.speedRatio;
    camera->waterYPos = sMMCam.waterYPos;

    switch (funcId) {
        case MMCAM_FUNC_NORMAL1:
            Camera_MMNormal1(camera);
            break;
        case MMCAM_FUNC_NORMAL3:
            Camera_MMNormal3(camera);
            break;
        case MMCAM_FUNC_PARALLEL1:
            Camera_MMParallel1(camera);
            break;
        case MMCAM_FUNC_JUMP2:
            Camera_MMJump2(camera);
            break;
        case MMCAM_FUNC_BATTLE1:
            Camera_MMBattle1(camera);
            break;
        case MMCAM_FUNC_KEEPON1:
            Camera_MMKeepOn1(camera);
            break;
        case MMCAM_FUNC_KEEPON3:
            Camera_MMKeepOn3(camera);
            break;
        case MMCAM_FUNC_JUMP3:
            Camera_MMJump3(camera);
            break;
        case MMCAM_FUNC_SUBJECT1:
            Camera_MMSubject1(camera);
            break;
        default:
            break;
    }

    camera->playerPosDelta = savedPosDelta;
    camera->xzSpeed = savedXZSpeed;
    camera->speedRatio = savedSpeedRatio;
    camera->waterYPos = savedWaterYPos;
    return true;
}

void MergeShip_Camera_SetMMMeleeWeaponAnimation(s32 mmMeleeWeaponAnimation) {
    sMMCam.mmMeleeWeaponAnimation = mmMeleeWeaponAnimation;
}

// MM Camera_SetSwordDistortion (with MM func_800CBB88), for the MM forms: the Goron butt punch and the
// Zora kick / MM spin attack distortions. Human Link keeps OoT's (none).
void MergeShip_CameraMM_UpdateFormEffects(Camera* camera) {
    Player* player = camera->player;
    s32 type = 0;

    if ((camera->thisIdx != CAM_ID_MAIN) || (player == NULL) || !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return;
    }

    // MM func_800CBB88
    if ((player->meleeWeaponState != 0) && (sMMCam.mmMeleeWeaponAnimation == MM_PLAYER_MWA_GORON_PUNCH_BUTT)) {
        type = 3;
    } else if ((player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) ||
               ((player->meleeWeaponState != 0) &&
                (sMMCam.mmMeleeWeaponAnimation == MM_PLAYER_MWA_ZORA_PUNCH_KICK))) {
        type = 1;
    }

    switch (type) {
        case 1: // non-magic spin attack / Zora kick
            if (MergeShip_Distortion_GetType() != MERGESHIP_DISTORTION_ZORA_KICK) {
                MergeShip_Distortion_Request(MERGESHIP_DISTORTION_ZORA_KICK);
                MergeShip_Distortion_SetDuration(12);
            }
            break;

        case 3:
            if (MergeShip_Distortion_GetType() != MERGESHIP_DISTORTION_GORON_BUTT) {
                MergeShip_Distortion_Request(MERGESHIP_DISTORTION_GORON_BUTT);
                MergeShip_Distortion_SetDuration(15);
            }
            break;

        default:
            break;
    }
}
