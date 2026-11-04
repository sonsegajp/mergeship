/*
 * File: z_mm_en_torch2.c
 * Overlay: ovl_MM_En_Torch2
 * Description: Majora's Mask's Elegy of Emptiness shell (MM ovl_En_Torch2) and the beam of light that makes it
 *              (MM ovl_Eff_Change), for the Elegy played in Hyrule (BUG-0144). Ported from
 *              2ship2harkinian/mm/src/overlays/actors/ovl_En_Torch2 and ovl_Eff_Change.
 *
 * The shells (gElegyShell*DL) and the beam (gGameplayKeepKFSkel_2900C, gGameplayKeepKFAnim_281DC,
 * gameplay_keep_Matanimheader_028FEC) are MM's gameplay_keep, from mm.o2r.
 *
 * Name mapping from MM:
 *   play->actorCtx.elegyShells      -> gMergeShipElegyShells (Ocarina of Time's ActorContext has no such array)
 *   Actor_MoveWithGravity           -> Actor_MoveXZGravity
 *   ACTOR_FLAG_CAN_PRESS_SWITCHES   -> none: Ocarina of Time's floor switches feel any actor standing on them
 *   ACELEM_ON | ACELEM_HOOKABLE     -> BUMP_ON | BUMP_HOOKABLE
 *   Scene_SetRenderModeXlu          -> MMEnTorch2_SetRenderMode (MM z_scene_proc.c's lists, below)
 *   Gfx_SetupDL25_Xlu               -> Gfx_SetupDL_25Xlu
 *   AnimatedMat_DrawStepXlu         -> MergeShip_AnimatedMat_DrawStepXlu (soh/MergeShip/MergeShipAnimatedMat.c)
 *   Keyframe_* / Keyframe_DrawFlex  -> soh/MergeShip/MergeShipKeyframe.c
 *   Matrix_RotateYS                 -> Matrix_RotateY(BINANG_TO_RAD(...))
 *   GRAPH_ALLOC                     -> Graph_Alloc
 *   CutsceneManager (CS_ID_GLOBAL_ELEGY) -> none: the camera stays where it is (Hyrule has no MM cutscene manager)
 *   Play_SetRespawnData / Play_SetupRespawnPoint (the shells' places kept for MM's scene reloads) -> none: a shell
 *     lasts while the scene does.
 */

#include "z_mm_en_torch2.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"

s16 gMergeShipActorId_En_Torch2 = -1;
s16 gMergeShipActorId_Eff_Change = -1;
MMEnTorch2* gMergeShipElegyShells[MM_TORCH2_PARAM_MAX] = { NULL };

void MMEnTorch2_UpdateIdle(Actor* thisx, PlayState* play);
void MMEnTorch2_UpdateDeath(Actor* thisx, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER | OC1_TYPE_1 | OC1_TYPE_2,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00100000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 20, 60, 0, { 0, 0, 0 } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_S8(colChkInfo.mass, MASS_IMMOVABLE, ICHAIN_STOP),
};

// Shells for each of Link's different forms, by MM transformation (playing the Elegy as the Fierce Deity puts down a
// human shell).
static const char* sShellDLists[] = {
    mm_gElegyShellHumanDL, mm_gElegyShellGoronDL, mm_gElegyShellZoraDL, mm_gElegyShellDekuDL, mm_gElegyShellHumanDL,
};

// MM Scene_SetRenderModeXlu (z_scene_proc.c): the shells' lists take their render mode from segment 0x0C. Copied
// whole: the shells call the second half of each list (entry 2, G_DL_INDEX 0x0C000002), the half MM's comment calls
// never executed.
static void MMEnTorch2_SetRenderMode(PlayState* play, s32 index, u32 flags) {
    static Gfx renderModeSetNoneDL[] = {
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
        gsSPEndDisplayList(),
    };
    static Gfx renderModeSetXluSingleCycleDL[] = {
        gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                          G_RM_AA_ZB_XLU_SURF2),
        gsSPEndDisplayList(),
        gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                          G_RM_AA_ZB_XLU_SURF2),
        gsSPEndDisplayList(),
    };
    static Gfx* dLists[] = { renderModeSetNoneDL, renderModeSetXluSingleCycleDL };

    OPEN_DISPS(play->state.gfxCtx);
    if (flags & 1) {
        gSPSegment(POLY_OPA_DISP++, 0x0C, dLists[index]);
    }
    if (flags & 2) {
        gSPSegment(POLY_XLU_DISP++, 0x0C, dLists[index]);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void MMEnTorch2_Init(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;

    if ((this->actor.params < 0) || (this->actor.params >= MM_TORCH2_PARAM_MAX)) {
        this->actor.params = MM_TORCH2_PARAM_HUMAN;
    }
    Actor_ProcessInitChain(&this->actor, sInitChain);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->framesUntilNextState = 20;
}

void MMEnTorch2_Destroy(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    if (gMergeShipElegyShells[this->actor.params] == this) {
        gMergeShipElegyShells[this->actor.params] = NULL;
    }
}

void MMEnTorch2_Update(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;
    u16 targetAlpha;
    u16 remainingFrames;

    if (this->state == MM_TORCH2_STATE_IDLE) {
        this->actor.update = MMEnTorch2_UpdateIdle;
        return;
    }

    this->actor.gravity = -1.0f;
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 20.0f, 70.0f, 1 | 4);

    if (this->framesUntilNextState == 0) {
        remainingFrames = 0;
    } else {
        remainingFrames = --this->framesUntilNextState;
    }

    if (remainingFrames == 0) {
        if (this->state == MM_TORCH2_STATE_INITIALIZED) {
            // Spawn in
            if (this->alpha == 0) {
                Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.home.pos);
                this->actor.shape.rot.y = this->actor.home.rot.y;
                this->state = MM_TORCH2_STATE_FADING_IN;
            }
            targetAlpha = 0;
        } else if (this->state == MM_TORCH2_STATE_FADING_IN) {
            // Stay semitransparent until the player moves away
            if ((this->actor.xzDistToPlayer > 32.0f) || (fabsf(this->actor.yDistToPlayer) > 70.0f)) {
                this->state = MM_TORCH2_STATE_SOLID;
            }
            targetAlpha = 60;
        } else {
            // Once the player has moved away, update collision and become opaque
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            targetAlpha = 255;
        }
        Math_StepToS(&this->alpha, targetAlpha, 8);
    }
}

void MMEnTorch2_UpdateIdle(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;

    if (this->state == MM_TORCH2_STATE_DYING) {
        // Start death animation
        this->actor.update = MMEnTorch2_UpdateDeath;
        this->actor.velocity.y = 0.0f;
    }
}

void MMEnTorch2_UpdateDeath(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;

    // Fall down and become transparent, then delete once invisible
    if (Math_StepToS(&this->alpha, 0, 8)) {
        Actor_Kill(&this->actor);
        return;
    }

    this->actor.gravity = -1.0f;
    Actor_MoveXZGravity(&this->actor);
}

void MMEnTorch2_Draw(Actor* thisx, PlayState* play) {
    MMEnTorch2* this = (MMEnTorch2*)thisx;
    Gfx* gfx = (Gfx*)sShellDLists[this->actor.params];

    OPEN_DISPS(play->state.gfxCtx);

    if (this->alpha == 255) {
        MMEnTorch2_SetRenderMode(play, 0, 0x01);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        Gfx_DrawDListOpa(play, gfx);
    } else {
        MMEnTorch2_SetRenderMode(play, 1, 0x02);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, this->alpha);
        Gfx_DrawDListXlu(play, gfx);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- MM ovl_Eff_Change: the beam of light when the shell is made ------------------------------------------------

void MMEffChange_SetColors(MMEffChange* this, s32 arg1);
void MMEffChange_Main(MMEffChange* this, PlayState* play);

static u8 sEffChangeColors[] = {
    // prim r g b   env r g b
    255, 255, 170, 0,   100, 0,   // COLOR_0
    255, 255, 170, 200, 0,   0,   // COLOR_1
    170, 255, 255, 0,   100, 255, // COLOR_2
    255, 255, 170, 200, 150, 0,   // COLOR_3
    255, 255, 170, 0,   100, 0,   // COLOR_4
    255, 255, 170, 0,   100, 0,   // COLOR_5
    255, 255, 170, 0,   100, 0,   // COLOR_6
    255, 255, 170, 0,   100, 0    // COLOR_7
};

void MMEffChange_Init(Actor* thisx, PlayState* play) {
    MMEffChange* this = (MMEffChange*)thisx;

    this->actionFunc = MMEffChange_Main;
    MMEffChange_SetColors(this, MM_EFFCHANGE_GET_COLORS(thisx));
    Actor_SetScale(&this->actor, 0.075f);
    this->primColors[3] = 0;
    Keyframe_InitFlex(&this->kfSkelAnime, (KeyFrameFlexSkeleton*)mm_gGameplayKeepKFSkel_2900C,
                      (KeyFrameAnimation*)mm_gGameplayKeepKFAnim_281DC, this->jointTable, this->morphTable, NULL);
    // MergeShip2: without mm.o2r's keyframe data there is nothing to animate or draw.
    if ((this->kfSkelAnime.skeleton == NULL) || (this->kfSkelAnime.animation == NULL)) {
        Actor_Kill(thisx);
        return;
    }
    Keyframe_FlexPlayOnce(&this->kfSkelAnime, (KeyFrameAnimation*)mm_gGameplayKeepKFAnim_281DC);
    this->step = 0;
    this->actor.shape.rot.y = 0;
    this->kfSkelAnime.frameCtrl.speed = (2.0f / 3.0f);
}

void MMEffChange_Destroy(Actor* thisx, PlayState* play) {
    MMEffChange* this = (MMEffChange*)thisx;

    if (this->kfSkelAnime.skeleton != NULL) {
        Keyframe_DestroyFlex(&this->kfSkelAnime);
    }
}

void MMEffChange_SetColors(MMEffChange* this, s32 arg1) {
    arg1 *= 6;
    this->primColors[0] = sEffChangeColors[arg1];
    this->primColors[1] = sEffChangeColors[arg1 + 1];
    this->primColors[2] = sEffChangeColors[arg1 + 2];
    this->envColors[0] = sEffChangeColors[arg1 + 3];
    this->envColors[1] = sEffChangeColors[arg1 + 4];
    this->envColors[2] = sEffChangeColors[arg1 + 5];
}

void MMEffChange_Main(MMEffChange* this, PlayState* play) {
    f32 lightRatio;

    if (Keyframe_UpdateFlex(&this->kfSkelAnime)) {
        Actor_Kill(&this->actor);
        Environment_AdjustLights(play, 0.0f, 850.0f, 0.2f, 0.0f);
        return;
    }
    this->step++;
    if (this->kfSkelAnime.frameCtrl.curTime < 20.0f) {
        if ((this->primColors[3]) < 242) {
            this->primColors[3] += 13;
        } else {
            this->primColors[3] = 255;
        }
    } else if (this->kfSkelAnime.frameCtrl.curTime > 70.0f) {
        if ((this->primColors[3]) >= 14) {
            this->primColors[3] -= 13;
        } else {
            this->primColors[3] = 0;
        }
    } else {
        this->primColors[3] = 255;
    }
    lightRatio = this->primColors[3] * (1.0f / 255.0f);
    if (lightRatio > 1.0f) {
        lightRatio = 1.0f;
    } else if (lightRatio < 0.0f) {
        lightRatio = 0.0f;
    }
    Environment_AdjustLights(play, lightRatio, 850.0f, 0.2f, 0.0f);
}

void MMEffChange_Update(Actor* thisx, PlayState* play) {
    MMEffChange* this = (MMEffChange*)thisx;

    this->actionFunc(this, play);
}

void MMEffChange_Draw(Actor* thisx, PlayState* play) {
    MMEffChange* this = (MMEffChange*)thisx;
    Mtx* mtxStack;

    if (this->kfSkelAnime.skeleton == NULL) {
        return;
    }
    MergeShip_AnimatedMat_DrawStepXlu(play, (void*)mm_gameplay_keep_Matanimheader_028FEC, this->step);
    mtxStack = Graph_Alloc(play->state.gfxCtx, this->kfSkelAnime.skeleton->dListCount * sizeof(Mtx));
    if (mtxStack != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_RotateY(BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000)), MTXMODE_APPLY);
        OPEN_DISPS(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, this->primColors[0], this->primColors[1], this->primColors[2],
                        this->primColors[3]);
        gDPSetEnvColor(POLY_XLU_DISP++, this->envColors[0], this->envColors[1], this->envColors[2], 255);
        Keyframe_DrawFlex(play, &this->kfSkelAnime, mtxStack, NULL, NULL, &this->actor);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}
