/*
 * File: z_mm_en_test.c
 * Overlay: ovl_MM_En_Test
 * Description: Majora's Mask's crater marks (MM ovl_En_Test: Goron Link's punch and ground pound, the Moon's Tear
 *              crash), for the Majora's Mask forms in Ocarina of Time. Ported from
 *              2ship2harkinian/mm/src/overlays/actors/ovl_En_Test.
 *
 * The crater is MM's gameplay_keep keyframe skeleton gGameplayKeepKFSkel_6EB70 playing gGameplayKeepKFAnim_6BB0C,
 * with its colours and scrolls from gameplay_keep_Matanimheader_06B6A0 (06B730 on snow), plus 20 pieces of
 * debris (gameplay_keep_DL_06AB30), all from mm.o2r.
 *
 * Name mapping from MM:
 *   Keyframe_* / FrameCtrl_*        -> the same functions, soh/MergeShip/MergeShipKeyframe.c
 *   AnimatedMat_DrawStep            -> MergeShip_AnimatedMat_DrawStep (soh/MergeShip/MergeShipAnimatedMat.c)
 *   func_800C0094                   -> MergeShip_func_800C0094 (below)
 *   Matrix_MtxFToYXZRot             -> MergeShip_Matrix_MtxFToYXZRot (below)
 *   SurfaceType_GetMaterial         -> func_80041F10: the same bits (SurfaceType data word 1, & 0xF), and Ocarina of
 *     Time's surface sound ids 0..13 are MM's SurfaceMaterial 0..13 (dirt ... carpet). Ocarina of Time has no
 *     SURFACE_MATERIAL_SNOW (14): its floors never use that value, so the crater always takes the regular material
 *     animation there, as MM's does on every non-snow floor.
 *   Gfx_SetupDL25_Opa / _Xlu        -> Gfx_SetupDL_25Opa / _25Xlu
 *   Matrix_Finalize                 -> MATRIX_NEWMTX
 *   GRAPH_ALLOC                     -> Graph_Alloc
 */

#include "z_mm_en_test.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)

// MM SurfaceMaterial values EnTest compares against (z64bgcheck.h).
#define MM_SURFACE_MATERIAL_DIRT 0
#define MM_SURFACE_MATERIAL_SNOW 14
#define MM_SURFACE_MATERIAL_MAX 15

s16 gMergeShipActorId_En_Test = -1;

// Defined in z_bgcheck.c, not declared in a header.
void CollisionPoly_GetNormalF(CollisionPoly* poly, f32* nx, f32* ny, f32* nz);

// MM func_800C0094 (z_bgcheck.c): compute the transform matrix mapping +y (up) to the collision poly's normal.
void MergeShip_func_800C0094(CollisionPoly* poly, f32 tx, f32 ty, f32 tz, MtxF* dest) {
    f32 nx;
    f32 ny;
    f32 nz;
    f32 z_f14;
    f32 phi_f14;
    f32 phi_f12;
    f32 inv_z_f14;

    if (poly == NULL) {
        return;
    }
    CollisionPoly_GetNormalF(poly, &nx, &ny, &nz);

    z_f14 = sqrtf(SQ(ny) + SQ(nz));
    if (!IS_ZERO(z_f14)) {
        inv_z_f14 = 1.0f / z_f14;
        phi_f14 = ny * inv_z_f14;
        phi_f12 = nz * inv_z_f14;
    } else {
        phi_f14 = 1.0f;
        phi_f12 = 0.0f;
    }
    dest->xx = z_f14;
    dest->yx = (-nx) * phi_f14;
    dest->zx = (-nx) * phi_f12;
    dest->xy = nx;
    dest->yy = ny;
    dest->zy = nz;
    dest->xz = 0.0f;
    dest->yz = -phi_f12;
    dest->zz = phi_f14;
    dest->xw = tx;
    dest->yw = ty;
    dest->zw = tz;
    dest->wx = 0.0f;
    dest->wy = 0.0f;
    dest->wz = 0.0f;
    dest->ww = 1.0f;
}

// MM Matrix_MtxFToYXZRot (sys_matrix.c). MM's Math_Atan2S(y, x) is Ocarina of Time's Math_Atan2S(x, y).
void MergeShip_Matrix_MtxFToYXZRot(MtxF* src, Vec3s* dest, s32 nonUniformScale) {
    f32 temp;
    f32 temp2;
    f32 temp3;

    temp = src->xz;
    temp *= temp;
    temp += SQ(src->zz);
    dest->x = Math_Atan2S(sqrtf(temp), -src->yz);

    if ((dest->x == 0x4000) || (dest->x == -0x4000)) {
        // Gimbal lock: fix z to make y well-defined.
        dest->z = 0;

        dest->y = Math_Atan2S(src->xx, -src->zx);
    } else {
        dest->y = Math_Atan2S(src->zz, src->xz);

        if (!nonUniformScale) {
            // assume the columns have the same normalisation
            dest->z = Math_Atan2S(src->yy, src->yx);
        } else {
            temp = src->xx;
            temp2 = src->zx;
            temp3 = src->zy;

            // find norm of the first column
            temp *= temp;
            temp += SQ(temp2);
            temp2 = src->yx;
            temp += SQ(temp2);
            // temp = xx^2+zx^2+yx^2 == 1 for a rotation matrix
            temp = sqrtf(temp);
            temp = temp2 / temp; // yx in normalised column

            // find norm of the second column
            temp2 = src->xy;
            temp2 *= temp2;
            temp2 += SQ(temp3);
            temp3 = src->yy;
            temp2 += SQ(temp3);
            // temp2 = xy^2+zy^2+yy^2 == 1 for a rotation matrix
            temp2 = sqrtf(temp2);
            temp2 = temp3 / temp2; // yy in normalised column

            // for a rotation matrix, temp == yx and temp2 == yy which is the same as in the !nonUniformScale branch
            dest->z = Math_Atan2S(temp2, temp);
        }
    }
}

// MM func_80862B70
static void MMEnTest_func_80862B70(MMEnTestStruct* arg0) {
    s32 i;

    for (i = 0; i < 20; i++) {
        MMEnTestStruct* ptr = &arg0[i];

        ptr->unk_00 = false;
        ptr->unk_04 = 0;
        ptr->unk_08.x = 0.0f;
        ptr->unk_08.y = 0.0f;
        ptr->unk_08.z = 0.0f;
        ptr->unk_14.x = 0.0f;
        ptr->unk_14.y = 0.0f;
        ptr->unk_14.z = 0.0f;
        ptr->unk_20.x = 0.0f;
        ptr->unk_20.y = 0.0f;
        ptr->unk_20.z = 0.0f;
        ptr->unk_2C = 0.001f;
        ptr->unk_30.x = 0;
        ptr->unk_30.y = 0;
        ptr->unk_30.z = 0;
        ptr->unk_36.x = 0;
        ptr->unk_36.y = 0;
        ptr->unk_36.z = 0;
    }
}

// MM func_80862CBC: launch one piece of debris.
static void MMEnTest_func_80862CBC(MMEnTestStruct* arg0, Vec3f* arg1) {
    MMEnTestStruct* ptr;
    s32 i;

    for (i = 0; i < 20; i++) {
        ptr = &arg0[i];

        if (!ptr->unk_00) {
            s16 sp26 = Rand_ZeroOne() * 0xFFFF;
            f32 sp20;

            ptr->unk_00 = true;

            ptr->unk_08.x = (Math_CosS(sp26) * 20.0f) + arg1->x;
            ptr->unk_08.y = arg1->y;
            ptr->unk_08.z = (Math_SinS(sp26) * 20.0f) + arg1->z;

            sp20 = Rand_ZeroOne();

            ptr->unk_14.x = Math_CosS(sp26) * 13.0f * sp20;
            ptr->unk_14.y = (Rand_ZeroOne() * 5.0f) + 8.0f;
            ptr->unk_14.z = (Math_SinS(sp26) * 13.0f) * sp20;

            ptr->unk_20.x = 0.0f;
            ptr->unk_20.z = 0.0f;
            ptr->unk_20.y = -2.0f;

            ptr->unk_2C = (Rand_ZeroOne() * 0.0004f) + 0.0004f;

            ptr->unk_30.x = Rand_ZeroOne() * 0x7FFE;
            ptr->unk_30.y = Rand_ZeroOne() * 0x7FFE;
            ptr->unk_30.z = Rand_ZeroOne() * 0x7FFE;

            ptr->unk_36.x = Rand_ZeroOne() * 0x2000;
            ptr->unk_36.y = Rand_ZeroOne() * 0x2000;
            ptr->unk_36.z = Rand_ZeroOne() * 0x2000;

            ptr->unk_04 = 10;
            break;
        }
    }
}

// MM func_80862EDC: move the debris.
static void MMEnTest_func_80862EDC(MMEnTestStruct* arg0) {
    s32 i;
    MMEnTestStruct* ptr;

    for (i = 0; i < 20; i++) {
        ptr = &arg0[i];

        if (ptr->unk_00) {
            ptr->unk_04--;
            if (ptr->unk_04 < 0) {
                ptr->unk_00 = false;
            }

            ptr->unk_08.x += ptr->unk_14.x;
            ptr->unk_08.y += ptr->unk_14.y;
            ptr->unk_08.z += ptr->unk_14.z;

            ptr->unk_14.x += ptr->unk_20.x;
            ptr->unk_14.y += ptr->unk_20.y;
            ptr->unk_14.z += ptr->unk_20.z;

            ptr->unk_30.x += ptr->unk_36.x;
            ptr->unk_30.y += ptr->unk_36.y;
            ptr->unk_30.z += ptr->unk_36.z;
        }
    }
}

// MM func_80863048: draw the debris.
static void MMEnTest_func_80863048(PlayState* play, MMEnTestStruct* arg1) {
    MMEnTestStruct* ptr;
    s32 i;
    Mtx* mtx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);

    for (i = 0; i < 20; i++) {
        ptr = &arg1[i];

        if (!ptr->unk_00) {
            continue;
        }

        Matrix_Translate(ptr->unk_08.x, ptr->unk_08.y, ptr->unk_08.z, MTXMODE_NEW);
        Matrix_RotateZYX(ptr->unk_30.x, ptr->unk_30.y, ptr->unk_30.z, MTXMODE_APPLY);
        Matrix_Scale(ptr->unk_2C, ptr->unk_2C, ptr->unk_2C, MTXMODE_APPLY);

        mtx = MATRIX_NEWMTX(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gameplay_keep_DL_06AB30);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void MMEnTest_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    MMEnTest* this = (MMEnTest*)thisx;
    MtxF sp38;
    s32 bgId;

    this->unk_209 = 0;
    this->unk_174 = 0;

    if (thisx->params > 0) {
        Actor_SetScale(thisx, thisx->params / 100000.0f);
        this->surfaceMaterial = MM_SURFACE_MATERIAL_DIRT;
    } else {
        thisx->floorPoly = NULL;
        thisx->world.pos.y += 10.0f;
        thisx->floorHeight = BgCheck_EntityRaycastFloor3(&play->colCtx, &thisx->floorPoly, &bgId, &thisx->world.pos);

        if ((thisx->floorPoly == NULL) || (thisx->floorHeight == BGCHECK_Y_MIN)) {
            Actor_Kill(thisx);
            return;
        }

        thisx->world.pos.y = thisx->floorHeight;
        MergeShip_func_800C0094(thisx->floorPoly, thisx->world.pos.x, thisx->floorHeight, thisx->world.pos.z, &sp38);
        MergeShip_Matrix_MtxFToYXZRot(&sp38, &thisx->shape.rot, true);
        thisx->world.rot = thisx->shape.rot;
        // MM SurfaceType_GetMaterial (see the header comment).
        this->surfaceMaterial = func_80041F10(&play->colCtx, thisx->floorPoly, bgId);
    }

    Keyframe_InitFlex(&this->kfSkelAnime, (KeyFrameFlexSkeleton*)mm_gGameplayKeepKFSkel_6EB70,
                      (KeyFrameAnimation*)mm_gGameplayKeepKFAnim_6BB0C, this->jointTable, this->morphTable, NULL);
    // MergeShip2: without mm.o2r's keyframe data there is nothing to animate or draw.
    if ((this->kfSkelAnime.skeleton == NULL) || (this->kfSkelAnime.animation == NULL)) {
        Actor_Kill(thisx);
        return;
    }
    Keyframe_FlexPlayOnce(&this->kfSkelAnime, (KeyFrameAnimation*)mm_gGameplayKeepKFAnim_6BB0C);
    this->kfSkelAnime.frameCtrl.curTime = 9.0f;
    MMEnTest_func_80862B70(this->unk_20C);
}

void MMEnTest_Destroy(Actor* thisx, PlayState* play) {
    MMEnTest* this = (MMEnTest*)thisx;

    Keyframe_DestroyFlex(&this->kfSkelAnime);
}

void MMEnTest_Update(Actor* thisx, PlayState* play) {
    MMEnTest* this = (MMEnTest*)thisx;
    s32 i;

    this->unk_208 = this->kfSkelAnime.frameCtrl.curTime;

    if (Keyframe_UpdateFlex(&this->kfSkelAnime) && (this->actor.parent == NULL) && (this->actor.params != -1)) {
        this->unk_209++;
        if (this->unk_209 > 20) {
            Actor_Kill(&this->actor);
        }
    }

    if ((this->actor.params != -1) && (this->unk_208 >= 2)) {
        if (!(this->unk_174 & 1)) {
            this->unk_174 |= 1;

            for (i = 0; i < ARRAY_COUNT(this->unk_20C); i++) {
                MMEnTest_func_80862CBC(this->unk_20C, &this->actor.world.pos);
            }
        }
    }
    MMEnTest_func_80862EDC(this->unk_20C);
}

static s32 MMEnTest_OverrideLimbDraw(struct PlayState* play, KFSkelAnimeFlex* kfSkelAnime, s32 limbIndex,
                                     Gfx** dList, u8* flags, void* thisx, Vec3f* scale, Vec3s* rot, Vec3f* pos) {
    MMEnTest* this = (MMEnTest*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    if ((this->actor.params == -1) && ((limbIndex == 1) || (limbIndex == 2))) {
        *dList = NULL;
    }

    if ((this->actor.params == -2) && (limbIndex == 3)) {
        *dList = NULL;
    }

    if (limbIndex == 3) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 0, 0, ((20 - this->unk_209) * 255.0f) / 20.0f);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, ((20 - this->unk_209) * 255.0f) / 20.0f);
    }

    CLOSE_DISPS(play->state.gfxCtx);

    return true;
}

void MMEnTest_Draw(Actor* thisx, PlayState* play) {
    MMEnTest* this = (MMEnTest*)thisx;
    Mtx* mtxStack;
    s32 sp2C = this->unk_208 - 1;

    if (sp2C >= 29) {
        sp2C = 29;
    }

    //! @bug Checks for non-existent SURFACE_MATERIAL_MAX material
    if ((this->surfaceMaterial == MM_SURFACE_MATERIAL_MAX) || (this->surfaceMaterial == MM_SURFACE_MATERIAL_SNOW)) {
        MergeShip_AnimatedMat_DrawStep(play, (AnimatedMaterial*)mm_gameplay_keep_Matanimheader_06B730, sp2C);
    } else {
        MergeShip_AnimatedMat_DrawStep(play, (AnimatedMaterial*)mm_gameplay_keep_Matanimheader_06B6A0, sp2C);
    }

    mtxStack = Graph_Alloc(play->state.gfxCtx, this->kfSkelAnime.skeleton->dListCount * sizeof(Mtx));

    if (mtxStack != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Keyframe_DrawFlex(play, &this->kfSkelAnime, mtxStack, MMEnTest_OverrideLimbDraw, NULL, thisx);
        MMEnTest_func_80863048(play, this->unk_20C);
    }
}
