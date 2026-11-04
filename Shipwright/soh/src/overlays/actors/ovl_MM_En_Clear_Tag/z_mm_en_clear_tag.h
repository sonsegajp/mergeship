#ifndef Z_MM_EN_CLEAR_TAG_H
#define Z_MM_EN_CLEAR_TAG_H

// MergeShip2 -- Majora's Mask's En_Clear_Tag (explosions and pops, light rays, splashes, smoke), ported from
// 2ship2harkinian/mm/src/overlays/actors/ovl_En_Clear_Tag. Ocarina of Time's En_Clear_Tag (ACTOR_EN_CLEAR_TAG) is an
// unrelated actor (the Arwing); this one is registered at run time (MergeShipActorMMEnClearTag.cpp) and spawned with
// gMergeShipActorId_MM_En_Clear_Tag. MM En_Bom spawns it for its explosions (the powder keg: the large explosion) and
// for the smoke of a bomb put out by water.
//
//   Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Clear_Tag, x, y, z, rotX, rotY, rotZ, type)
//
// type is a MMClearTagType; rotX/Y/Z are MM's colour (explosions: the flash's env colour, default when all zero;
// light rays: rotZ = colour index) or scale (smoke: rotZ).

#include <libultraship/libultra.h>
#include "global.h"

struct MMEnClearTag;

// MM CLEAR_TAG_PARAMS(type)
#define MMCLEARTAG_PARAMS(type) (type)

// MM ClearTagType
typedef enum {
    /* 0x00 */ MMCLEARTAG_SMALL_EXPLOSION,
    /* 0x01 */ MMCLEARTAG_LARGE_EXPLOSION,
    /* 0x02 */ MMCLEARTAG_POP,
    /* 0x03 */ MMCLEARTAG_SMALL_LIGHT_RAYS,
    /* 0x04 */ MMCLEARTAG_LARGE_LIGHT_RAYS,
    /* 0x23 */ MMCLEARTAG_SPLASH = 35,
    /* 0xC8 */ MMCLEARTAG_SMOKE = 200
} MMClearTagType;

// MM EnClearTagEffect
typedef struct MMEnClearTagEffect {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 actionTimer;
    /* 0x04 */ Vec3f pos;
    /* 0x10 */ Vec3f velocity;
    /* 0x1C */ Vec3f accel;
    /* 0x28 */ Color_RGBAf primColor;
    /* 0x38 */ Color_RGBAf envColor;
    /* 0x48 */ s16 bounces;
    /* 0x4A */ s16 effectsTimer;
    /* 0x4C */ s16 lightRayAlpha;
    /* 0x4E */ s16 lightRayAlphaDecrementSpeed;
    /* 0x50 */ f32 scale;
    /* 0x54 */ f32 maxScale;
    /* 0x58 */ f32 rotationY;
    /* 0x5C */ f32 rotationX;
    /* 0x60 */ f32 rotationZ;
    /* 0x64 */ f32 maxScaleTarget;
    /* 0x68 */ f32 smokeScaleY;
    /* 0x6C */ f32 smokeScaleX;
} MMEnClearTagEffect; // size = 0x70

#define MM_EN_CLEAR_TAG_EFFECT_COUNT 103

// MM EnClearTag. MM's sub-camera fields (cameraState, subCamId, subCamEye/At) belong to its negative-params camera
// mode, which is not ported (see the .c).
typedef struct MMEnClearTag {
    Actor actor;
    MMEnClearTagEffect effect[MM_EN_CLEAR_TAG_EFFECT_COUNT];
    s16 activeTimer; // Actor Marked for Death when timer runs out
    Color_RGBA8 flashEnvColor;
} MMEnClearTag;

#ifdef __cplusplus
extern "C" {
#endif

void MMEnClearTag_Init(Actor* thisx, PlayState* play);
void MMEnClearTag_Destroy(Actor* thisx, PlayState* play);
void MMEnClearTag_Update(Actor* thisx, PlayState* play);
void MMEnClearTag_Draw(Actor* thisx, PlayState* play);

// Run-time actor id assigned by ActorDB (MergeShipActorMMEnClearTag.cpp); -1 until registered.
extern s16 gMergeShipActorId_MM_En_Clear_Tag;

#ifdef __cplusplus
}
#endif

#endif // Z_MM_EN_CLEAR_TAG_H
