#ifndef Z_MM_EN_TEST_H
#define Z_MM_EN_TEST_H

// MergeShip2 -- Majora's Mask's En_Test (crater marks: Goron Link's punch and pound, and in MM also the Moon's Tear
// crash), ported from 2ship2harkinian/mm/src/overlays/actors/ovl_En_Test. Registered at run time
// (soh/MergeShip/MergeShipActorMMEnTest.cpp) and spawned by the Goron with gMergeShipActorId_En_Test.
//
// params: > 0 -> scale params / 100000 on the surface the spawner oriented it to (the punch uses 500);
//         0   -> find the floor under the spawn point and lie on it (the ground pound);
//         -1  -> no debris and no fade-out (MM's Moon's Tear crater); -2 -> hides limb 3.

#include <libultraship/libultra.h>
#include "global.h"
#include "soh/MergeShip/MergeShipKeyframe.h"

struct MMEnTest;

// MM EnTestStruct: one piece of flying debris.
typedef struct {
    /* 0x00 */ s32 unk_00;
    /* 0x04 */ s32 unk_04;
    /* 0x08 */ Vec3f unk_08;
    /* 0x14 */ Vec3f unk_14;
    /* 0x20 */ Vec3f unk_20;
    /* 0x2C */ f32 unk_2C;
    /* 0x30 */ Vec3s unk_30;
    /* 0x36 */ Vec3s unk_36;
} MMEnTestStruct; // size = 0x3C

typedef struct MMEnTest {
    /* 0x000 */ Actor actor;
    /* 0x144 */ KFSkelAnimeFlex kfSkelAnime;
    /* 0x174 */ s32 unk_174;
    /* 0x178 */ Vec3s jointTable[12];
    /* 0x1C0 */ Vec3s morphTable[12];
    /* 0x208 */ u8 unk_208;
    /* 0x209 */ u8 unk_209;
    /* 0x20A */ u8 surfaceMaterial;
    /* 0x20C */ MMEnTestStruct unk_20C[20];
} MMEnTest; // size = 0x6BC

#ifdef __cplusplus
extern "C" {
#endif

void MMEnTest_Init(Actor* thisx, PlayState* play2);
void MMEnTest_Destroy(Actor* thisx, PlayState* play);
void MMEnTest_Update(Actor* thisx, PlayState* play);
void MMEnTest_Draw(Actor* thisx, PlayState* play);

// Run-time actor id assigned by ActorDB (MergeShipActorMMEnTest.cpp); -1 until registered.
extern s16 gMergeShipActorId_En_Test;

// MM func_800C0094 (z_bgcheck.c): the transform that maps +y onto the poly's normal, translated to (tx, ty, tz).
// Ocarina of Time's counterpart, func_80038A28, builds a different basis, so MM's is ported.
void MergeShip_func_800C0094(CollisionPoly* poly, f32 tx, f32 ty, f32 tz, MtxF* dest);

// MM Matrix_MtxFToYXZRot (sys_matrix.c), with MM's table arctangent (Ocarina of Time's Math_Atan2S, arguments
// in Ocarina of Time's order). Ocarina of Time's Matrix_MtxFToYXZRotS computes the same angles with Math_FAtan2F.
void MergeShip_Matrix_MtxFToYXZRot(MtxF* src, Vec3s* dest, s32 nonUniformScale);

#ifdef __cplusplus
}
#endif

#endif // Z_MM_EN_TEST_H
