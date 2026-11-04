// MergeShip2 -- Majora's Mask's transformation-mask sequence, inside Ocarina of Time's player.
//
// Included by z_player.c (not compiled on its own) so it can use the player's static helpers, the
// same way MM's code lives in MM's z_player.c. Everything here is ported from
// 2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/z_player.c and src/code/z_player_lib.c;
// each function names its MM original. Values, timings and ordering are MM's.
//
// The sequence, as in MM:
//   1. Player_UseItem accepts a transformation mask and defers it (unk_6AD = 4, MM's unk_AA5 = 5).
//   2. Player_ActionHandler_13 starts it: MergeShip_StartTransformation (MM func_808388B8) records the
//      new form in the save and enters MergeShip_Action_PutOnMask (MM Player_Action_86).
//   3. The screen fills white. The player stops updating and drawing (MM func_8012301C) and is then
//      rebuilt as the new form (MM Player_Init with shape.rot.z != 0): MergeShip_ReinitAsForm.
//   4. MergeShip_Action_TransformEnd (MM Player_Action_87) fades the white out and returns to idle.

#include "soh/MergeShip/MergeShipTransformFx.h"

#ifndef TRUNCF_BINANG
#define TRUNCF_BINANG(f) ((s16)(s32)(f)) // MM macros.h
#endif

// MM Player_Init: Deku is too light for floor switches; every other form presses them. (MM also gives
// the Goron ACTOR_FLAG_CAN_PRESS_HEAVY_SWITCHES, for switches Ocarina of Time does not have.)
static void MergeShip_SetFormActorFlags(Player* this, u8 form) {
    this->actor.flags &= ~ACTOR_FLAG_CAN_PRESS_SWITCHES;
    if (form != PLAYER_FORM_DEKU) {
        this->actor.flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    }
}

// MM Inventory_UpdateDeitySwordEquip: Fierce Deity's B button is its sword.
static void MergeShip_UpdateDeitySwordEquip(PlayState* play, u8 form) {
    if (form == PLAYER_FORM_FIERCE_DEITY) {
        gSaveContext.equips.buttonItems[0] = ITEM_MM_SWORD_FIERCE_DEITY;
    }
    if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) && (gSaveContext.equips.buttonItems[0] != ITEM_MM_FD)) {
        Interface_LoadItemIcon1(play, 0);
    }
}

// MM D_8085D160: the animation each form starts a transformation with.
static LinkAnimationHeader* sMMTransformStartAnims[MM_FORM_MAX] = {
    (LinkAnimationHeader*)mm_gPlayerAnim_pz_maskoffstart, // MM_FORM_FIERCE_DEITY
    (LinkAnimationHeader*)mm_gPlayerAnim_pg_maskoffstart, // MM_FORM_GORON
    (LinkAnimationHeader*)mm_gPlayerAnim_pz_maskoffstart, // MM_FORM_ZORA
    (LinkAnimationHeader*)mm_gPlayerAnim_pn_maskoffstart, // MM_FORM_DEKU
    (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmask,      // MM_FORM_HUMAN
};
MERGESHIP_ASSERT_MM_FORM_TABLE(sMMTransformStartAnims);

// MM D_80862B50: the scene's light adjustment, restored when the transformation ends.
static s16 sMMSavedAdjAmbientColor[3];
static s16 sMMSavedAdjLight1Color[3];
static s16 sMMSavedAdjFogColor[3];
static s16 sMMSavedAdjFogNear;
static s16 sMMSavedAdjFogFar;

static void MergeShip_SaveEnvAdjust(PlayState* play) {
    s32 i;
    for (i = 0; i < 3; i++) {
        sMMSavedAdjAmbientColor[i] = play->envCtx.adjAmbientColor[i];
        sMMSavedAdjLight1Color[i] = play->envCtx.adjLight1Color[i];
        sMMSavedAdjFogColor[i] = play->envCtx.adjFogColor[i];
    }
    sMMSavedAdjFogNear = play->envCtx.adjFogNear;
    sMMSavedAdjFogFar = play->envCtx.adjFogFar;
}

static void MergeShip_RestoreEnvAdjust(PlayState* play) {
    s32 i;
    for (i = 0; i < 3; i++) {
        play->envCtx.adjAmbientColor[i] = sMMSavedAdjAmbientColor[i];
        play->envCtx.adjLight1Color[i] = sMMSavedAdjLight1Color[i];
        play->envCtx.adjFogColor[i] = sMMSavedAdjFogColor[i];
    }
    play->envCtx.adjFogNear = sMMSavedAdjFogNear;
    play->envCtx.adjFogFar = sMMSavedAdjFogFar;
}

// ---- lighting (MM func_80854CD0, func_80854EFC, func_808550D0, D_8085D848) --------------------

typedef struct MMTransformEnvStage {
    /* 0x0 */ s16 fogNear;
    /* 0x2 */ u8 fogColor[3];
    /* 0x5 */ u8 ambientColor[3];
} MMTransformEnvStage; // MM struct_8085D848_unk_00

typedef struct MMTransformLight {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ u8 color[3];
    /* 0x10 */ s16 radius;
} MMTransformLight; // MM struct_8085D848_unk_18

typedef struct MMTransformLighting {
    /* 0x00 */ MMTransformEnvStage env[3];
    /* 0x18 */ MMTransformLight light[3];
} MMTransformLighting; // MM struct_8085D848

// MM D_8085D848: [0] putting a mask on as a human, [1] changing from any other form.
static MMTransformLighting sMMTransformLighting[] = {
    {
        {
            { 650, { 0, 0, 0 }, { 10, 0, 30 } },
            { 300, { 200, 200, 255 }, { 0, 0, 0 } },
            { 600, { 0, 0, 0 }, { 0, 0, 200 } },
        },
        {
            { { -40.0f, 20.0f, -10.0f }, { 120, 200, 255 }, 1000 },
            { { 0.0f, -10.0f, 0.0f }, { 255, 255, 255 }, 5000 },
            { { -10.0f, 4.0f, 3.0f }, { 200, 200, 255 }, 5000 },
        },
    },
    {
        {
            { 650, { 0, 0, 0 }, { 10, 0, 30 } },
            { 300, { 200, 200, 255 }, { 0, 0, 0 } },
            { 600, { 0, 0, 0 }, { 0, 0, 200 } },
        },
        {
            { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
            { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
            { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
        },
    },
};

// MM func_80854CD0: interpolate three colours at once, each relative to the scene's own value.
static void MergeShip_LerpEnvColors(f32 t, s16* out1, u8* a1, u8* b1, u8* base1, s16* out2, u8* a2, u8* b2, u8* base2,
                                    s16* out3, u8* a3, u8* b3, u8* base3) {
    s32 i;

    for (i = 0; i < 3; i++) {
        *out1 = ((s32)((*a1 - *b1) * t) + *b1) - *base1;
        *out2 = ((s32)((*a2 - *b2) * t) + *b2) - *base2;
        *out3 = ((s32)((*a3 - *b3) * t) + *b3) - *base3;
        out1++, a1++, b1++, base1++;
        out2++, a2++, b2++, base2++;
        out3++, a3++, b3++, base3++;
    }
}

static u8 sMMBlack[] = { 0, 0, 0 }; // MM D_8085D844

// MM func_80854EFC. t runs 0..4 through the three stages and back to the scene's own settings.
static void MergeShip_TransformEnvStep(PlayState* play, f32 t, MMTransformEnvStage* stages) {
    MMTransformEnvStage scene;
    MMTransformEnvStage* to;
    MMTransformEnvStage* from;
    u8* lightTo;
    u8* lightFrom;
    u8* sceneLight1 = play->envCtx.lightSettings.light1Color;

    scene.fogNear = play->envCtx.lightSettings.fogNear;
    scene.fogColor[0] = play->envCtx.lightSettings.fogColor[0];
    scene.fogColor[1] = play->envCtx.lightSettings.fogColor[1];
    scene.fogColor[2] = play->envCtx.lightSettings.fogColor[2];
    scene.ambientColor[0] = play->envCtx.lightSettings.ambientColor[0];
    scene.ambientColor[1] = play->envCtx.lightSettings.ambientColor[1];
    scene.ambientColor[2] = play->envCtx.lightSettings.ambientColor[2];

    if (t <= 1.0f) {
        t -= 0.0f;
        to = &stages[0];
        from = &scene;
        lightTo = sMMBlack;
        lightFrom = sceneLight1;
    } else if (t <= 2.0f) {
        t -= 1.0f;
        to = &stages[1];
        from = &stages[0];
        lightTo = sMMBlack;
        lightFrom = sMMBlack;
    } else if (t <= 3.0f) {
        t -= 2.0f;
        to = &stages[2];
        from = &stages[1];
        lightTo = sMMBlack;
        lightFrom = sMMBlack;
    } else {
        t -= 3.0f;
        to = &scene;
        from = &stages[2];
        lightTo = sceneLight1;
        lightFrom = sMMBlack;
    }

    play->envCtx.adjFogNear =
        (TRUNCF_BINANG((to->fogNear - from->fogNear) * t) + from->fogNear) - play->envCtx.lightSettings.fogNear;

    MergeShip_LerpEnvColors(t, play->envCtx.adjFogColor, to->fogColor, from->fogColor,
                            play->envCtx.lightSettings.fogColor, play->envCtx.adjAmbientColor, to->ambientColor,
                            from->ambientColor, play->envCtx.lightSettings.ambientColor, play->envCtx.adjLight1Color,
                            lightTo, lightFrom, sceneLight1);
}

// MM func_808550D0. colorT drives the scene colours; lightT picks one of three point lights and
// scales its radius.
static void MergeShip_TransformLighting(PlayState* play, Player* this, f32 colorT, f32 lightT, s32 set) {
    MMTransformLighting* lighting = &sMMTransformLighting[set];
    MMTransformLight* light = lighting->light;
    Vec3f pos;

    MergeShip_TransformEnvStep(play, colorT, lighting->env);

    if (lightT > 2.0f) {
        lightT -= 2.0f;
        light += 2;
    } else if (lightT > 1.0f) {
        lightT -= 1.0f;
        light++;
    }

    Player_GetRelativePosition(this, &this->actor.world.pos, &light->pos, &pos);
    Lights_PointNoGlowSetInfo(&this->lightInfo, pos.x, pos.y, pos.z, light->color[0], light->color[1],
                              light->color[2], light->radius * lightT);
}

// ---- the sequence --------------------------------------------------------------------------

// MM D_8085D8F0 / D_8085D904: sounds keyed to the mask animations.
static AnimSfxEntry sMMSetMaskSfx[] = {
    { MM_NA_SE_PL_PUT_OUT_ITEM, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 2) },
    { MM_NA_SE_IT_SET_TRANSFORM_MASK, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 4) },
    { MM_NA_SE_PL_FREEZE_S, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 11) },
    { MM_NA_SE_PL_TRANSFORM_VOICE, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 30) },
    { MM_NA_SE_IT_TRANSFORM_MASK_BROKEN, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
};

static AnimSfxEntry sMMSetMaskEndSfx[] = {
    { MM_NA_SE_IT_SET_TRANSFORM_MASK, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 8) },
};

// Majora's Mask's sound ids index MM's own tables, so these lists only ever hold GENERAL entries and
// play through the MM path.
static void MergeShip_ProcessMMAnimSfxList(Player* this, AnimSfxEntry* entry) {
    s32 cont;

    do {
        s32 absData = ABS(entry->data);

        if (LinkAnimation_OnFrame(&this->skelAnime, fabsf(ANIMSFX_GET_FRAME(absData)))) {
            // MergeShip2 (soh_fd): Adult Link screams with his own voice ("Adult Link Transformation Scream").
            if (MergeShip_Ganondorf_TransformVoice(this, entry->sfxId)) {
                // MergeShip2: Ganondorf mode -- his own voice (MergeShipGanondorfPlayer.inc)
            } else if ((entry->sfxId != MM_NA_SE_PL_TRANSFORM_VOICE) || !MergeShip_SohFd_PlayAdultScream(this)) {
                MergeShip_PlayMMSfxActor(&this->actor, entry->sfxId);
            }
        }
        cont = (entry->data >= 0);
        entry++;
    } while (cont);
}

// MM func_80855218: the mask moving to the face, and the face deforming as it takes.
static void MergeShip_UpdateMaskAnim(PlayState* play, Player* this) {
    if (LinkAnimation_Update(play, &this->skelAnime) &&
        (this->skelAnime.animation == (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmask)) {
        Player_AnimPlayLoopAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmaskend);
    } else if ((this->skelAnime.animation == (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmask) ||
               (this->skelAnime.animation == (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmaskend)) {
        if (this->av1.actionVar1 >= 58) {
            Math_StepToS(&this->av2.actionVar2, 255, 50);
        }

        if (this->av1.actionVar1 >= 64) {
            Math_StepToF(&this->unk_B10[2], 0.0f, 0.015f);
        } else if (this->av1.actionVar1 >= 0xE) {
            Math_StepToF(&this->unk_B10[2], 0.3f, 0.3f);
        }

        if (this->av1.actionVar1 > 65) {
            Math_StepToF(&this->unk_B10[3], 0.0f, 0.02f);
        } else if (this->av1.actionVar1 >= 0x10) {
            Math_StepToF(&this->unk_B10[3], -0.1f, 0.1f);
        }

        if ((R_PLAY_FILL_SCREEN_ON == 0) &&
            (this->skelAnime.animation == (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmask)) {
            MergeShip_ProcessMMAnimSfxList(this, sMMSetMaskSfx);
        }
    } else {
        if (this->av1.actionVar1 >= 20) {
            Math_StepToS(&this->av2.actionVar2, 255, 20);
        }

        if (R_PLAY_FILL_SCREEN_ON == 0) {
            MergeShip_ProcessMMAnimSfxList(this, sMMSetMaskEndSfx);
            if (this->av1.actionVar1 == 15) {
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_FACE_CHANGE);
            }
        }
    }
}

// MM D_8085D910[0]: frames at which the lighting stages change.
static const u8 sMMTransformLightTiming[] = { 0x10, 0xA, 0x3B, 0x3F };

void MergeShip_PlayerRebuildUpdate(Actor* thisx, PlayState* play);

// MM Player_Action_86: putting the mask on, up to the white flash.
// MergeShip2: the swim actions keep Link afloat themselves; the transformation's actions don't swim, so in water
// they hold him where he is rather than letting the last action's gravity sink him.
static void MergeShip_HoldInWater(Player* this) {
    if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        this->actor.gravity = 0.0f;
        this->actor.velocity.y = 0.0f;
    }
}

void MergeShip_Action_PutOnMask(Player* this, PlayState* play) {
    const u8* timing = sMMTransformLightTiming;
    u8 fromMM = MergeShip_FormToMM(this->mergeShipForm);
    u8 toMM = gMergeShipSave.mmForm;
    s32 skipped = false;

    MergeShip_HoldInWater(this);
    if (!MergeShip_Ganondorf_NoTransformCam(this)) { // MergeShip2: Ganondorf mode -- Ganon is seen by the usual camera
        MergeShip_TransformCam_Request(play, &this->actor);
    }
    sControlInput = play->state.input;
    Player_ZeroSpeedXZ(this); // MergeShip2: MM's Player_Action_86 never moves Link

    MergeShip_TransformCam_SetMode(play, (fromMM == MM_FORM_HUMAN) ? MERGESHIP_TRANSFORM_CAM_HUMAN
                                                                   : MERGESHIP_TRANSFORM_CAM_FORM);
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    this->actor.shape.rot.y = MergeShip_TransformCam_GetYaw(play) + 0x8000;

    MergeShip_UpdateMaskAnim(play, this);
    MergeShip_Ganondorf_PutOnMaskFrame(this, play); // MergeShip2: Ganondorf mode -- Ganondorf's own transformation

    if (this->av1.actionVar1 == 0x14) {
        MergeShip_MotionBlur_EnablePriority(100);
    }

    if (R_PLAY_FILL_SCREEN_ON != 0) {
        R_PLAY_FILL_SCREEN_ALPHA += R_PLAY_FILL_SCREEN_ON;
        if (R_PLAY_FILL_SCREEN_ALPHA > 255) {
            R_PLAY_FILL_SCREEN_ALPHA = 255;
            if (MergeShip_AdultMask_Drawing(this)) {
                // MergeShip2: the Adult Mask -- at the flash Link becomes the real other age, in place (no scene
                // reload): gSaveContext.linkAge flips and Ocarina of Time's own age-equipment swap runs
                // (MergeShipAdultMask.h). The rebuild below, shared with every other transformation, then picks up
                // the new age's skeleton and buttons the same way it picks up a form's.
                MergeShip_AdultMask_SwitchAge(play, this);
            }
            MergeShip_Ganondorf_TransformFlash(this); // MergeShip2: Ganondorf mode -- his form changes here
            this->actor.update = MergeShip_PlayerRebuildUpdate;
            this->actor.draw = NULL;
            this->av1.actionVar1 = 0;
            MergeShip_MotionBlur_DisablePriority();
            // MM D_8085D908: the transformation into this form has been seen once, so from now on
            // it can be skipped. (MM indexes this out of bounds when returning to human; that sets
            // two unrelated MM event flags and has no counterpart here.)
            gMergeShipSave.transformSeen |= (1 << toMM);
        }
    } else if ((this->av1.actionVar1++ > ((fromMM == MM_FORM_HUMAN) ? 0x53 : 0x37)) ||
               ((this->av1.actionVar1 >= 5) &&
                (skipped = ((fromMM != MM_FORM_HUMAN) || (gMergeShipSave.transformSeen & (1 << toMM))) &&
                           CHECK_BTN_ANY(sControlInput->press.button, BTN_CRIGHT | BTN_CLEFT | BTN_CDOWN | BTN_CUP |
                                                                          BTN_B | BTN_A | BTN_DUP | BTN_DDOWN |
                                                                          BTN_DLEFT | BTN_DRIGHT)))) {
        R_PLAY_FILL_SCREEN_ON = 45;
        R_PLAY_FILL_SCREEN_R = 220;
        R_PLAY_FILL_SCREEN_G = 220;
        R_PLAY_FILL_SCREEN_B = 220;
        R_PLAY_FILL_SCREEN_ALPHA = 0;

        if (skipped) {
            MergeShip_TransformCam_Skip(play);

            if (fromMM == MM_FORM_HUMAN) {
                MergeShip_StopMMSfxById(MM_NA_SE_PL_TRANSFORM_VOICE);
                MergeShip_StopMMSfxById(MM_NA_SE_IT_TRANSFORM_MASK_BROKEN);
            } else {
                MergeShip_StopMMSfxById(MM_NA_SE_PL_FACE_CHANGE);
            }
        }

        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_SY_TRANSFORM_MASK_FLASH);
    }

    if (this->av1.actionVar1 >= timing[0]) {
        if (this->av1.actionVar1 < timing[2]) {
            Math_StepToF(&this->unk_B10[4], 1.0f, timing[1] / 100.0f);
        } else if (this->av1.actionVar1 < timing[3]) {
            if (this->av1.actionVar1 == timing[2]) {
                MergeShip_PlayMMSfxCentered(MM_NA_SE_EV_LIGHTNING_HARD);
            }
            Math_StepToF(&this->unk_B10[4], 2.0f, 0.5f);
        } else {
            Math_StepToF(&this->unk_B10[4], 3.0f, 0.2f);
        }
    }

    if (this->av1.actionVar1 >= 0x10) {
        if (this->av1.actionVar1 < 0x40) {
            Math_StepToF(&this->unk_B10[5], 1.0f, 0.2f);
        } else if (this->av1.actionVar1 < 0x37) {
            Math_StepToF(&this->unk_B10[5], 2.0f, 1.0f);
        } else {
            Math_StepToF(&this->unk_B10[5], 3.0f, 0.55f);
        }
    }

    MergeShip_TransformLighting(play, this, this->unk_B10[4], this->unk_B10[5], (fromMM == MM_FORM_HUMAN) ? 0 : 1);
}

// MM Player_Action_87: the new form, as the white fades.
void MergeShip_Action_TransformEnd(Player* this, PlayState* play) {
    // MM: the cutscene Player_Action_86 played is a form's when prevMask is set. The camera and the lights follow that;
    // the animation stays with prevMask, as MergeShip_ReinitAsForm chose it.
    s32 formCs = (this->prevMask != PLAYER_MASK_NONE);
    s32 ganonBusy = MergeShip_Ganondorf_TransformEndFrame(this, play); // MergeShip2: Ganondorf mode's entrances

    MergeShip_HoldInWater(this);
    MergeShip_TransformCam_SetMode(play, formCs ? MERGESHIP_TRANSFORM_CAM_FORM : MERGESHIP_TRANSFORM_CAM_HUMAN);

    if (R_PLAY_FILL_SCREEN_ON != 0) {
        R_PLAY_FILL_SCREEN_ALPHA -= R_PLAY_FILL_SCREEN_ON;
        if (R_PLAY_FILL_SCREEN_ALPHA < 0) {
            R_PLAY_FILL_SCREEN_ON = 0;
            R_PLAY_FILL_SCREEN_ALPHA = 0;
        }
    }

    if (this->av1.actionVar1++ < 4) {
        if ((this->prevMask == PLAYER_MASK_NONE) && (this->av1.actionVar1 == 4)) {
            LinkAnimation_Change(play, &this->skelAnime, Player_GetIdleAnim(this), 1.0f, 0.0f, 20.0f, ANIMMODE_ONCE,
                                 20.0f);
        }
    } else {
        f32 dist = sqrtf(SQ((f32)play->state.input[0].rel.stick_x) + SQ((f32)play->state.input[0].rel.stick_y));

        // MM Lib_GetControlStickData: only whether the stick is held matters here.
        if (dist > 60.0f) {
            dist = 60.0f;
        }

        if ((LinkAnimation_Update(play, &this->skelAnime) || ((this->av1.actionVar1 > 10) && (dist != 0.0f))) &&
            !ganonBusy) {
            if (R_PLAY_FILL_SCREEN_ON == 0) {
                this->mergeShipTransforming = false;
                this->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
                this->prevMask = this->currentMask;
                MergeShip_TransformCam_Stop(play);
                MergeShip_RestoreEnvAdjust(play);
                func_80853080(this, play); // MM func_8085B384
                return;
            }
        }

        Math_StepToF(&this->unk_B10[5], 4.0f, 0.2f);
    }

    MergeShip_TransformLighting(play, this, 0, this->unk_B10[5], formCs ? 1 : 0);
}

// MM func_808388B8.
void MergeShip_StartTransformation(PlayState* play, Player* this, u8 targetForm) {
    func_80832440(play, this); // MM func_8082DD2C
    Player_DetachHeldActor(play, this);
    Player_SetupActionPreserveItemAction(play, this, MergeShip_Action_PutOnMask, 0);
    Player_ZeroSpeedXZ(this); // MergeShip2: Link stays planted for the whole transformation, as in MM
    Player_AnimChangeOnceMorphAdjusted(play, this, sMMTransformStartAnims[MergeShip_FormToMM(this->mergeShipForm)]);
    MergeShip_SetPlayerForm(targetForm);
    // MM PLAYER_STATE1_2. Ocarina of Time's actors read "the player is in a cutscene" from
    // PLAYER_STATE1_IN_CUTSCENE where MM's read it from PLAYER_STATE1_2, so both are set.
    this->mergeShipTransforming = true;
    this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;

    MergeShip_SaveEnvAdjust(play);
    this->actor.velocity.y = 0.0f;
    Actor_DisableLens(play);
}

// MM func_80838A20: taking an ordinary mask off as a human. Ocarina of Time removes its own masks
// instantly and keeps doing so; this is only reached from a transformation mask.
// (Human -> transformation and form -> anything both go through MergeShip_StartTransformation.)

// Player_ActionHandler_13's transformation-mask branch (MM Player_ActionHandler_13, the
// `itemAction >= PLAYER_IA_MASK_MIN` block). Returns true once the item has been handled.
s32 MergeShip_UseTransformationMask(PlayState* play, Player* this) {
    u8 mmForm = this->itemAction - PLAYER_IA_MM_MASK_FIERCE_DEITY;
    s32 maskId = PLAYER_MASK_MM_FIERCE_DEITY + mmForm;

    if (MergeShip_AdultMask_TakeUsing()) {
        // MergeShip2: the Adult Mask (MergeShipAdultMask.h), used as the Fierce Deity's Mask: it goes on (drawn as
        // itself) and the transformation plays; at its flash Link's age changes (MergeShip_AdultMask_SwitchAge).
        // The adult puts it on the same way, as the Child Mask (the user, 2026-10-01: "Every time Adult puts it on,
        // give him a transformation cutscene"): MM's human cutscene, the mask on his face, his scream.
        this->prevMask = this->currentMask;
        this->currentMask = PLAYER_MASK_MM_FIERCE_DEITY;
        MergeShip_AdultMask_BeginTransform();
        MergeShip_StartTransformation(play, this, (u8)gSaveContext.linkAge);
        return true;
    }

    MergeShip_SohFd_CarryTradeMask(this); // MergeShip2 (soh_fd): "Forms Wear Trade Masks"
    this->prevMask = this->currentMask;
    if (maskId == this->currentMask) {
        // Already this form: the mask comes off and the player returns to human.
        this->currentMask = PLAYER_MASK_NONE;
        MergeShip_StartTransformation(play, this, (u8)gSaveContext.linkAge);
    } else {
        this->currentMask = maskId;
        MergeShip_StartTransformation(play, this, MergeShip_FormFromMM(mmForm));
    }
    return true;
}

// ---- the rebuild ---------------------------------------------------------------------------

static void MergeShip_ReinitAsForm(Player* this, PlayState* play);

// MM func_8012301C (z_player_lib.c): the player neither updates nor draws while the new form's
// object loads, then is re-initialised. MM re-runs Player_Init through actor.init, which the actor
// loop calls the frame after it is set; the same frame is used here, but the re-init is called
// directly so SoH's on-player-init hooks (persistent masks among them) do not treat it as a scene
// load.
void MergeShip_PlayerRebuildUpdate(Actor* thisx, PlayState* play) {
    Player* this = (Player*)thisx;

    this->av1.actionVar1++;

    if (this->av1.actionVar1 >= 4) {
        this->actor.shape.rot.z = MergeShip_FormToMM(MergeShip_GetPlayerForm()) + 1;
        this->actor.update = Player_Update;
        this->actor.draw = Player_Draw;
        MergeShip_ReinitAsForm(this, play);
    }
}

// MergeShip2: a form wears no Hylian tunic, so taking one on puts the Goron or Zora Tunic away and equips the
// Kokiri Tunic -- in the save, so the pause menu shows it and the Kokiri Tunic is still on after the mask comes off.
static void MergeShip_RevertTunicForForm(PlayState* play, Player* this, u8 form) {
    if (MERGESHIP_FORM_IS_MM(form) && (this == GET_PLAYER(play)) &&
        (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) != EQUIP_VALUE_TUNIC_KOKIRI)) {
        Inventory_ChangeEquipment(EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI);
    }
}

// MM Player_Init, taking the `shape.rot.z != 0` path: a player rebuilt mid-scene as a new form.
// Only what depends on the form is redone; the scene-entry work of Player_Init (title card, start
// mode, fairy, respawn data, allocations) is not, exactly as in MM.
static void MergeShip_ReinitAsForm(Player* this, PlayState* play) {
    u8 form = MergeShip_GetPlayerForm();
    u8 oldForm = this->mergeShipForm;
    u8 mmForm = MergeShip_FormToMM(form);

    this->actor.room = -1;

    if (mmForm == MM_FORM_HUMAN) {
        this->currentMask = PLAYER_MASK_NONE; // MM: gSaveContext.save.equippedMask, cleared by func_8012301C
    } else {
        this->currentMask = PLAYER_MASK_MM_FIERCE_DEITY + mmForm;
    }

    // MM func_8012301C clears gSaveContext.save.equippedMask: a human's mask does not survive.
    gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
    MergeShip_SohFd_AfterFormChange(this, form); // MergeShip2 (soh_fd): unless it is worn over the form

    // Each MM form has its own B button.
    MergeShip_SwapFormEquips(oldForm, form);
    MergeShip_UpdateDeitySwordEquip(play, form);

    this->unk_B28 = 0;
    this->unk_B90 = 0;
    this->unk_B92 = 0;
    this->unk_B94 = 0;
    this->unk_B96 = 0;
    this->stateFlags1 &= ~(PLAYER_STATE1_ITEM_IN_HAND | PLAYER_STATE1_CHARGING_SPIN_ATTACK |
                           PLAYER_STATE1_USING_BOOMERANG | PLAYER_STATE1_BOOMERANG_THROWN);
    this->stateFlags2 &= ~(PLAYER_STATE2_SPIN_ATTACKING | PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR |
                           PLAYER_STATE2_SWORD_LUNGE);
    this->stateFlags3 &= ~(PLAYER_STATE3_FINISHED_ATTACKING | PLAYER_STATE3_RESTORE_NAYRUS_LOVE |
                           PLAYER_STATE3_FLYING_WITH_HOOKSHOT | PLAYER_STATE3_100 | PLAYER_STATE3_200 |
                           PLAYER_STATE3_800 | PLAYER_STATE3_1000 | PLAYER_STATE3_2000 | PLAYER_STATE3_8000 |
                           PLAYER_STATE3_10000 | PLAYER_STATE3_40000 | PLAYER_STATE3_80000 | PLAYER_STATE3_100000 |
                           PLAYER_STATE3_200000 | PLAYER_STATE3_ZORA_BOOMERANG_CAUGHT | PLAYER_STATE3_1000000 |
                           PLAYER_STATE3_2000000);
    this->unk_B08 = 0.0f;
    this->unk_B0C = 0.0f;

    if (form == PLAYER_FORM_ZORA) {
        this->unk_B10[0] = (this->stateFlags1 & PLAYER_STATE1_IN_WATER) ? 1.0f : 0.0f;
    }

    MergeShip_SetFormActorFlags(this, form);

    this->itemAction = PLAYER_IA_NONE;
    this->heldItemAction = PLAYER_IA_NONE;
    this->heldItemId = ITEM_NONE;

    MergeShip_RevertTunicForForm(play, this, form);

    // Player_InitCommon's form-dependent half: property row, both skeletons, shadow.
    MergeShip_ApplyFormChange(this, play, form);
    Player_UseItem(play, this, ITEM_NONE);
    Player_SetEquipmentData(play, this);
    this->prevBoots = this->currentBoots;

    this->actor.shape.rot.z = 0;

    Player_SetupAction(play, this, MergeShip_Action_TransformEnd, 0);
    this->actor.shape.rot.y = this->yaw;

    if (this->prevMask != PLAYER_MASK_NONE) {
        Player_AnimPlayOnceAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_cl_maskoff);
    } else if (mmForm == MM_FORM_HUMAN) {
        LinkAnimation_Change(play, &this->skelAnime, sMMTransformStartAnims[MM_FORM_HUMAN], -2.0f / 3.0f, 9.0f, 0.0f,
                             ANIMMODE_ONCE, 0.0f);
    } else {
        Player_AnimPlayLoopAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmaskend);
    }

    // MM PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000 | PLAYER_STATE3_20000.
    this->stateFlags1 |= (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE);
    this->stateFlags3 |= PLAYER_STATE3_20000;
    this->unk_B10[5] = 3.0f;
}

// Test/debug: become a form immediately, as MM's Player_Init does on a scene load in that form.
static void MergeShip_ApplyDebugForm(Player* this, PlayState* play, u8 form) {
    u8 oldForm = this->mergeShipForm;
    u8 mmForm = MergeShip_FormToMM(form);

    MergeShip_SetPlayerForm(form);
    form = MergeShip_GetPlayerForm();
    mmForm = MergeShip_FormToMM(form);
    this->currentMask = (mmForm == MM_FORM_HUMAN) ? PLAYER_MASK_NONE : (PLAYER_MASK_MM_FIERCE_DEITY + mmForm);
    this->prevMask = this->currentMask;
    MergeShip_SwapFormEquips(oldForm, form);
    MergeShip_UpdateDeitySwordEquip(play, form);
    MergeShip_SetFormActorFlags(this, form);
    this->itemAction = PLAYER_IA_NONE;
    this->heldItemAction = PLAYER_IA_NONE;
    this->heldItemId = ITEM_NONE;
    MergeShip_RevertTunicForForm(play, this, form);
    MergeShip_ApplyFormChange(this, play, form);
    Player_UseItem(play, this, ITEM_NONE);
    Player_SetEquipmentData(play, this);
    this->prevBoots = this->currentBoots;
    func_80853080(this, play);
}

// MM Player_UseItem, the `var_v1` block for a transformation mask: refuse (with MM's error sound) if the
// new form would not fit under the ceiling. MM checks the height of the mask's form even when the mask
// is being taken off.
s32 MergeShip_CanUseTransformationMask(PlayState* play, Player* this, s32 itemAction) {
    u8 form = MergeShip_FormFromMM(itemAction - PLAYER_IA_MM_MASK_FIERCE_DEITY);
    f32 ceilingY;
    CollisionPoly* poly;
    s32 bgId;

    // MergeShip2 menu: with "Transformation Masks" off, a mask can still be taken off but none put on.
    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TransformationMasks"), 1) &&
        !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return false;
    }

    if (!MergeShip_IsMajorasMaskMounted() ||
        (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
         BgCheck_EntityCheckCeiling(&play->colCtx, &ceilingY, &this->actor.world.pos,
                                    sAgeProperties[form].ceilingCheckHeight, &poly, &bgId, &this->actor))) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return false;
    }
    return true;
}

// MM Player_Init, the part that depends on the saved form (the `shape.rot.x == 0` branch).
void MergeShip_InitFormOnSceneLoad(Player* this, PlayState* play) {
    u8 form = this->mergeShipForm;
    u8 mmForm = MergeShip_FormToMM(form);

    this->mergeShipTransforming = false;
    if (mmForm != MM_FORM_HUMAN) {
        this->currentMask = PLAYER_MASK_MM_FIERCE_DEITY + mmForm;
        MergeShip_UpdateDeitySwordEquip(play, form);
    } else if (PLAYER_MASK_IS_MM_TRANSFORMATION(this->currentMask)) {
        this->currentMask = PLAYER_MASK_NONE;
    } else if (this->currentMask == PLAYER_MASK_NONE) {
        MergeShip_MMMasks_RestoreOnSceneLoad(this); // MM Player_Init: `currentMask = equippedMask`
    }
    this->prevMask = this->currentMask;

    if (form == PLAYER_FORM_ZORA) {
        this->unk_B10[0] = (this->stateFlags1 & PLAYER_STATE1_IN_WATER) ? 1.0f : 0.0f;
    }
    MergeShip_SetFormActorFlags(this, form);
    MergeShip_TransformFx_Reset(play);
}

// MM Player_UpdateCommon: `if (curRoom.enablePosLights) white radius 60, else radius -1`. Ocarina of
// Time's rooms carry no positional-light flag, so outside a transformation the light is off.
void MergeShip_UpdatePlayerLight(Player* this, PlayState* play) {
    if (!this->mergeShipTransforming) {
        this->lightInfo.params.point.radius = -1;
    }
}

void MergeShip_ConsumeDebugForm(Player* this, PlayState* play) {
    u8 form = MergeShip_TakeDebugForm();

    if (form < PLAYER_FORM_MAX) {
        MergeShip_ApplyDebugForm(this, play, form);
    }
}
