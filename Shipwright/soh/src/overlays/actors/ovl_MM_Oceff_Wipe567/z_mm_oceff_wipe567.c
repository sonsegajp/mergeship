/*
 * File: z_mm_oceff_wipe567.c
 * Overlay: ovl_MM_Oceff_Wipe567
 * Description: Majora's Mask's song effects for its own songs, ported from 2ship2harkinian/mm
 *              src/overlays/actors/ovl_Oceff_Wipe5 (Sonata / Goron Lullaby / New Wave Bossa Nova / Elegy / Oath),
 *              ovl_Oceff_Wipe6 (Song of Soaring) and ovl_Oceff_Wipe7 (Song of Healing).
 *
 * Name mapping from MM:
 *   Camera_GetQuakeOffset(cam)          -> Camera_GetSkyboxOffset(&v, cam) (Ocarina of Time's name, as its own
 *                                          Oceff_Wipe uses it)
 *   Matrix_RotateXS(0x708, ...)         -> Matrix_RotateX(BINANG_TO_RAD(0x708), ...)
 *   MATRIX_FINALIZE_AND_LOAD            -> gSPMatrix(MATRIX_NEWMTX)
 *   Gfx_SetupDL25_Xlu                   -> Gfx_SetupDL_25Xlu
 *   AnimatedMat_Draw                    -> MergeShip_AnimatedMat_Draw (MM's animated materials, MergeShipAnimatedMat.h)
 *   ResourceMgr_GetVtxArraySizeByName   -> MergeShip_GetVtxCount (MergeShipActorMMOceff.cpp)
 *   msgCtx->ocarinaSongEffectActive     -> gMergeShipSongEffectActive (Ocarina of Time's MessageContext has no such
 *                                          field)
 *   Wipe5's New Wave Bossa Nova distance in MM's Marine Research Lab cutscene (SCENE_LABO) is Termina's and is
 *   not ported.
 */

#include "z_mm_oceff_wipe567.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/OTRGlobals.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"
#include "mm/overlays/ovl_Oceff_Wipe5/ovl_Oceff_Wipe5.h"
#include "mm/overlays/ovl_Oceff_Wipe6/ovl_Oceff_Wipe6.h"
#include "mm/overlays/ovl_Oceff_Wipe7/ovl_Oceff_Wipe7.h"

size_t MergeShip_GetVtxCount(const char* path);

s16 gMergeShipActorId_Oceff_Wipe5 = -1;
s16 gMergeShipActorId_Oceff_Wipe6 = -1;
s16 gMergeShipActorId_Oceff_Wipe7 = -1;
s32 gMergeShipSongEffectActive = false;

// 2S2H [Widescreen] Ocarina Effects: the frustum is pushed back on screens wider than 4:3 (MM's 1220 / 1150).
static f32 MMOceff_EffectDistance(f32 vanilla) {
    if ((OTRGetRectDimensionFromLeftEdge(0) << 2) < 0) {
        return vanilla / (OTRGetAspectRatio() * 0.85f);
    }
    return vanilla;
}

// The part the three MM effects share (their Draw functions are the same but for the assets and Wipe5's colours).
static void MMOceff_DrawFrustum(Actor* thisx, PlayState* play, s16 counter, f32 distance, const char* vtxPath,
                                const char* texAnim, const char* dl) {
    Vec3f activeCamEye = GET_ACTIVE_CAM(play)->eye;
    Vec3f quakeOffset;
    Vtx* vtx = ResourceMgr_LoadVtxByName((char*)vtxPath);
    size_t vtxCount = MergeShip_GetVtxCount(vtxPath);
    f32 z;
    u8 alpha;
    size_t i;

    Camera_GetSkyboxOffset(&quakeOffset, GET_ACTIVE_CAM(play));

    if (counter < 32) {
        z = Math_SinS(counter << 9) * distance;
    } else {
        z = distance;
    }

    if (counter >= 80) {
        alpha = 12 * (100 - counter);
    } else {
        alpha = 255;
    }

    if (vtx != NULL) {
        for (i = 1; i < vtxCount; i += 2) {
            vtx[i].v.cn[3] = alpha;
        }
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(activeCamEye.x + quakeOffset.x, activeCamEye.y + quakeOffset.y, activeCamEye.z + quakeOffset.z,
                     MTXMODE_NEW);
    Matrix_Scale(0.1f, 0.1f, 0.1f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_RotateX(BINANG_TO_RAD(0x708), MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, -z, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    MergeShip_AnimatedMat_Draw(play, (void*)texAnim);

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dl);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void MMOceff_Init(Actor* thisx, PlayState* play, s16* counter) {
    Actor_SetScale(thisx, 1.0f);
    *counter = 0;
    thisx->world.pos = GET_ACTIVE_CAM(play)->eye;
}

static void MMOceff_Destroy(PlayState* play) {
    Magic_Reset(play);
    gMergeShipSongEffectActive = false;
}

static void MMOceff_Update(Actor* thisx, PlayState* play, s16* counter) {
    thisx->world.pos = GET_ACTIVE_CAM(play)->eye;
    if (*counter < 100) {
        (*counter)++;
    } else {
        Actor_Kill(thisx);
    }
}

// ---- Oceff_Wipe5: Sonata / Lullaby / Bossa Nova / Elegy / Oath -----------------------------------------------

void MMOceffWipe5_Init(Actor* thisx, PlayState* play) {
    MMOceffWipe5* this = (MMOceffWipe5*)thisx;

    MMOceff_Init(thisx, play, &this->counter);
}

void MMOceffWipe5_Destroy(Actor* thisx, PlayState* play) {
    MMOceff_Destroy(play);
}

void MMOceffWipe5_Update(Actor* thisx, PlayState* play) {
    MMOceffWipe5* this = (MMOceffWipe5*)thisx;

    MMOceff_Update(thisx, play, &this->counter);
}

static u8 sWipe5PrimColors[] = {
    255, 255, 200, 255, 255, 200, 200, 255, 255, 255, 255, 200, 255, 200, 255,
};

static u8 sWipe5EnvColors[] = {
    50, 200, 0, 255, 50, 0, 0, 100, 255, 255, 150, 0, 255, 0, 200,
};

void MMOceffWipe5_Draw(Actor* thisx, PlayState* play) {
    MMOceffWipe5* this = (MMOceffWipe5*)thisx;
    s32 colorIndex = MM_OCEFF_WIPE5_GET_SONG_TYPE(thisx) * 3;

    if (colorIndex >= 13) {
        colorIndex = 0;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, sWipe5PrimColors[colorIndex], sWipe5PrimColors[colorIndex + 1],
                    sWipe5PrimColors[colorIndex + 2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sWipe5EnvColors[colorIndex], sWipe5EnvColors[colorIndex + 1],
                   sWipe5EnvColors[colorIndex + 2], 255);

    CLOSE_DISPS(play->state.gfxCtx);

    MMOceff_DrawFrustum(thisx, play, this->counter, MMOceff_EffectDistance(1220.0f), mm_gOceff5Vtx, mm_gOceff5TexAnim,
                        mm_gOceff5DL);
}

// ---- Oceff_Wipe6: Song of Soaring -------------------------------------------------------------------------------

void MMOceffWipe6_Init(Actor* thisx, PlayState* play) {
    MMOceffWipe6* this = (MMOceffWipe6*)thisx;

    MMOceff_Init(thisx, play, &this->counter);
}

void MMOceffWipe6_Destroy(Actor* thisx, PlayState* play) {
    MMOceff_Destroy(play);
}

void MMOceffWipe6_Update(Actor* thisx, PlayState* play) {
    MMOceffWipe6* this = (MMOceffWipe6*)thisx;

    MMOceff_Update(thisx, play, &this->counter);
}

void MMOceffWipe6_Draw(Actor* thisx, PlayState* play) {
    MMOceffWipe6* this = (MMOceffWipe6*)thisx;

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MMOceff_DrawFrustum(thisx, play, this->counter, MMOceff_EffectDistance(1220.0f), mm_gOceff6Vtx,
                        mm_ovl_Oceff_Wipe6_Matanimheader_000338, mm_gOceff6DL);
}

// ---- Oceff_Wipe7: Song of Healing -------------------------------------------------------------------------------

void MMOceffWipe7_Init(Actor* thisx, PlayState* play) {
    MMOceffWipe7* this = (MMOceffWipe7*)thisx;

    MMOceff_Init(thisx, play, &this->counter);
}

void MMOceffWipe7_Destroy(Actor* thisx, PlayState* play) {
    MMOceff_Destroy(play);
}

void MMOceffWipe7_Update(Actor* thisx, PlayState* play) {
    MMOceffWipe7* this = (MMOceffWipe7*)thisx;

    MMOceff_Update(thisx, play, &this->counter);
}

void MMOceffWipe7_Draw(Actor* thisx, PlayState* play) {
    MMOceffWipe7* this = (MMOceffWipe7*)thisx;

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MMOceff_DrawFrustum(thisx, play, this->counter, MMOceff_EffectDistance(1220.0f),
                        mm_sSongofHealingEffectFrustrumVtx, mm_sSongofHealingEffectTexAnim,
                        mm_sSongOfHealingEffectFrustumDL);
}
