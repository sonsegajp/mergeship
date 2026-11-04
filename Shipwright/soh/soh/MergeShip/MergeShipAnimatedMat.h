#pragma once

// MergeShip2 -- Majora's Mask's animated material system (z_scene_proc.c AnimatedMat_*), which Ocarina of Time
// does not have. MM objects' display lists read segments 0x08..0x0D for scrolling textures and animated colours;
// an AnimatedMaterial list fills those segments for one frame of the animation.
//
// The lists come from mm.o2r as TextureAnimation resources (soh/resource/type/TextureAnimation.h) and may be
// passed as their "__OTR__mm/..." path. The types below are MM's z64scene.h ones; they have the same layout as
// the resource's data.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// MM F3DPrimColor
typedef struct {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 a;
    /* 0x4 */ u8 lodFrac;
} F3DPrimColor; // size = 0x5

// MM F3DEnvColor
typedef struct {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 a;
} F3DEnvColor; // size = 0x4

// MM AnimatedMatColorParams
typedef struct {
    /* 0x0 */ u16 keyFrameLength;
    /* 0x2 */ u16 keyFrameCount;
    /* 0x4 */ F3DPrimColor* primColors;
    /* 0x8 */ F3DEnvColor* envColors;
    /* 0xC */ u16* keyFrames;
} AnimatedMatColorParams; // size = 0x10

// MM AnimatedMatTexScrollParams
typedef struct {
    /* 0x0 */ s8 xStep;
    /* 0x1 */ s8 yStep;
    /* 0x2 */ u8 width;
    /* 0x3 */ u8 height;
} AnimatedMatTexScrollParams; // size = 0x4

// MM AnimatedMatTexCycleParams
typedef struct {
    /* 0x0 */ u16 keyFrameLength;
    /* 0x4 */ void** textureList;
    /* 0x8 */ u8* textureIndexList;
} AnimatedMatTexCycleParams; // size = 0xC

// MM AnimatedMaterial. segment is 1-based from segment 0x08; the last entry of a list is negative.
typedef struct {
    /* 0x0 */ s8 segment;
    /* 0x2 */ s16 type;
    /* 0x4 */ void* params;
} AnimatedMaterial; // size = 0x8

// MM AnimatedMat_DrawMain and its wrappers, same arguments. flags: 1 = OPA, 2 = XLU.
// matAnim is an mm.o2r TextureAnimation path such as mm_gameplay_keep_Matanimheader_06B6A0 (the MM call's
// Lib_SegmentedToVirtual(header)), or an already-resolved AnimatedMaterial array.
void MergeShip_AnimatedMat_DrawMain(PlayState* play, void* matAnim, f32 alphaRatio, u32 step, u32 flags);
void MergeShip_AnimatedMat_Draw(PlayState* play, void* matAnim);
void MergeShip_AnimatedMat_DrawOpa(PlayState* play, void* matAnim);
void MergeShip_AnimatedMat_DrawXlu(PlayState* play, void* matAnim);
void MergeShip_AnimatedMat_DrawAlpha(PlayState* play, void* matAnim, f32 alphaRatio);
void MergeShip_AnimatedMat_DrawAlphaOpa(PlayState* play, void* matAnim, f32 alphaRatio);
void MergeShip_AnimatedMat_DrawAlphaXlu(PlayState* play, void* matAnim, f32 alphaRatio);
void MergeShip_AnimatedMat_DrawStep(PlayState* play, void* matAnim, u32 step);
void MergeShip_AnimatedMat_DrawStepOpa(PlayState* play, void* matAnim, u32 step);
void MergeShip_AnimatedMat_DrawStepXlu(PlayState* play, void* matAnim, u32 step);
void MergeShip_AnimatedMat_DrawAlphaStep(PlayState* play, void* matAnim, f32 alphaRatio, u32 step);
void MergeShip_AnimatedMat_DrawAlphaStepOpa(PlayState* play, void* matAnim, f32 alphaRatio, u32 step);
void MergeShip_AnimatedMat_DrawAlphaStepXlu(PlayState* play, void* matAnim, f32 alphaRatio, u32 step);

#ifdef __cplusplus
}
#endif
