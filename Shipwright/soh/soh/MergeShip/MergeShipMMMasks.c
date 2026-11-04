// MergeShip2 -- Majora's Mask's other twenty masks on the player. See MergeShipMMMasks.h.
//
// Ported from 2ship2harkinian mm/src/code/z_player_lib.c (D_801C0B20, func_801229FC, Player_UpdateBunnyEars,
// Player_DrawCouplesMask, Player_DrawCircusLeadersMask, Player_DrawBlastMask, Player_DrawBunnyHood,
// Player_DrawStrayFairyParticles, func_80127B64, func_80127DA4, func_80128388, Player_DrawGreatFairysMask) and
// mm/src/overlays/actors/ovl_player_actor/z_player.c (Player_UseItem's mask branch).
//
// MM keeps each mask in its own object, DMA'd into player->maskObjectSegment when the mask changes; 2S2H
// considers them loaded at once, and mm.o2r resolves each display list by path, so no segment 0x0A is set up.

#include "global.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMMMasks.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include "soh/MergeShip/MergeShipTransformFx.h" // MergeShip_PlayMMSfxActor / MergeShip_PlayMMSfxCentered
#include "soh/MergeShip/MergeShipMMSfx.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipChildLink.h"
#include <mergeship/MaskFit.h> // MergeShip2: each MM mask on the adult's head, shared with Termina
#include "mm/objects/object_mask_truth/object_mask_truth.h"
#include "mm/objects/object_mask_kerfay/object_mask_kerfay.h"
#include "mm/objects/object_mask_yofukasi/object_mask_yofukasi.h"
#include "mm/objects/object_mask_rabit/object_mask_rabit.h"
#include "mm/objects/object_mask_ki_tan/object_mask_ki_tan.h"
#include "mm/objects/object_mask_json/object_mask_json.h"
#include "mm/objects/object_mask_romerny/object_mask_romerny.h"
#include "mm/objects/object_mask_zacho/object_mask_zacho.h"
#include "mm/objects/object_mask_posthat/object_mask_posthat.h"
#include "mm/objects/object_mask_meoto/object_mask_meoto.h"
#include "mm/objects/object_mask_bigelf/object_mask_bigelf.h"
#include "mm/objects/object_mask_gibudo/object_mask_gibudo.h"
#include "mm/objects/object_mask_gero/object_mask_gero.h"
#include "mm/objects/object_mask_dancer/object_mask_dancer.h"
#include "mm/objects/object_mask_skj/object_mask_skj.h"
#include "mm/objects/object_mask_stone/object_mask_stone.h"
#include "mm/objects/object_mask_bree/object_mask_bree.h"
#include "mm/objects/object_mask_bakuretu/object_mask_bakuretu.h"
#include "mm/objects/object_mask_bu_san/object_mask_bu_san.h"
#include "mm/objects/object_mask_kyojin/object_mask_kyojin.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"
#include "soh/frame_interpolation.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <math.h>
#include <stdio.h>

// MergeShip2: frame interpolation pairs a frame's matrix operations with the next frame's in order, so a block that is
// drawn on some frames only (a mask on the head, a sword fitted to a hand, a form's extra part) would shift every
// operation after it and stretch a limb for a frame. Each Matrix_Push .. Matrix_Pop here is its own interpolation
// child (SoH's FrameInterpolation_RecordOpenChild), keyed by where it is.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
#define MS_INTERP_PUSH() (FrameInterpolation_RecordOpenChild(__FILE__, __LINE__), Matrix_Push())
#define MS_INTERP_POP() (Matrix_Pop(), FrameInterpolation_RecordCloseChild())

u8 gMergeShipStrayFairyNearby = false;
u16 gMergeShipBlastMaskTimer = 0;

// MM D_801C0B20[PLAYER_MASK_TRUTH - 1 .. PLAYER_MASK_GIANT - 1], indexed by MMMask - 1.
static const char* sMMMaskDLs[MM_MASK_MAX - 1] = {
    mm_object_mask_truth_DL_0001A0,    // MM_MASK_TRUTH
    mm_gKafeisMaskDL,                  // MM_MASK_KAFEIS_MASK
    mm_object_mask_yofukasi_DL_000490, // MM_MASK_ALL_NIGHT
    mm_object_mask_rabit_DL_000610,    // MM_MASK_BUNNY
    mm_object_mask_ki_tan_DL_0004A0,   // MM_MASK_KEATON
    mm_object_mask_json_DL_0004C0,     // MM_MASK_GARO
    mm_object_mask_romerny_DL_0007A0,  // MM_MASK_ROMANI
    mm_object_mask_zacho_DL_000700,    // MM_MASK_CIRCUS_LEADER
    mm_object_mask_posthat_DL_000290,  // MM_MASK_POSTMAN
    mm_object_mask_meoto_DL_0005A0,    // MM_MASK_COUPLE
    mm_object_mask_bigelf_DL_0016F0,   // MM_MASK_GREAT_FAIRY
    mm_object_mask_gibudo_DL_000250,   // MM_MASK_GIBDO
    mm_gDonGeroMaskDL,                 // MM_MASK_DON_GERO
    mm_object_mask_dancer_DL_000EF0,   // MM_MASK_KAMARO
    mm_object_mask_skj_DL_0009F0,      // MM_MASK_CAPTAIN
    mm_object_mask_stone_DL_000820,    // MM_MASK_STONE
    mm_object_mask_bree_DL_0003C0,     // MM_MASK_BREMEN
    mm_object_mask_bakuretu_DL_0005C0, // MM_MASK_BLAST
    mm_object_mask_bu_san_DL_000710,   // MM_MASK_SCENTS
    mm_object_mask_kyojin_DL_000380,   // MM_MASK_GIANT
};

u8 MergeShip_MMMasks_ItemOfMask(u8 currentMask) {
    if (!PLAYER_MASK_IS_MM_REGULAR(currentMask)) {
        return ITEM_NONE;
    }
    return ITEM_MM_MASK_TRUTH + (PLAYER_MASK_TO_MM_MASK(currentMask) - MM_MASK_TRUTH);
}

// MM Math_Atan2S_XY(x, y) = MM Math_Atan2S(y, x). OoT's Math_Atan2S has MM's body with its parameters named the
// other way round (OoT Math_Atan2S(a, b) == MM Math_Atan2S(b, a)), so MM's Math_Atan2S_XY(x, y) is OoT's
// Math_Atan2S(x, y). (MM's extra y == 0 / x == 0 cases give the same angles.)
static s16 MM_Atan2S_XY(f32 x, f32 y) {
    return Math_Atan2S(x, y);
}

// MM Math_Vec3f_Lerp.
static void MM_Vec3f_Lerp(Vec3f* a, Vec3f* b, f32 t, Vec3f* dest) {
    dest->x = (b->x - a->x) * t + a->x;
    dest->y = (b->y - a->y) * t + a->y;
    dest->z = (b->z - a->z) * t + a->z;
}

// ---------------------------------------------------------------------------------------------------------
// Putting a mask on

void MergeShip_MMMasks_Use(PlayState* play, Player* player, s32 itemAction) {
    u8 maskId = PLAYER_MASK_MM_REGULAR(itemAction - PLAYER_IA_MM_MASK_TRUTH + MM_MASK_TRUTH);

    (void)play;
    // MM gPlayerFormItemRestrictions: these masks are the human's (their buttons are greyed out for a form).
    if (MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return;
    }

    // CVAR_ENHANCEMENT("MergeShip.MMMasks") off: a mask can still be taken off but none can be put on.
    if ((maskId != player->currentMask) && !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMMasks"), 1)) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return;
    }

    // MM Player_UseItem: the Giant's Mask is not put on here but by its own action (MergeShipGiantsMask.inc).
    if (maskId == PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT)) {
        MergeShip_GiantsMask_Use(play, player);
        return;
    }

    // MM Player_UseItem: `(PLAYER_IA_MASK_MIN <= itemAction < PLAYER_IA_MASK_GIANT) && human`.
    player->prevMask = player->currentMask;
    if (maskId == player->currentMask) {
        player->currentMask = PLAYER_MASK_NONE;
        MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_PL_TAKE_OUT_SHIELD); // MM func_8082E1F0
    } else {
        player->currentMask = maskId;
        MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_PL_CHANGE_ARMS);
    }
    player->stateFlags2 |= PLAYER_STATE2_FOOTSTEP; // MM func_8082E1F0's PLAYER_STATE2_8
    gMergeShipSave.equippedMMMask = PLAYER_MASK_TO_MM_MASK(player->currentMask);
}

void MergeShip_MMMasks_RestoreOnSceneLoad(Player* player) {
    u8 mmMask = gMergeShipSave.equippedMMMask;

    // MM Player_Init: `if (currentMask == PLAYER_MASK_GIANT) currentMask = PLAYER_MASK_NONE`.
    if ((mmMask == MM_MASK_NONE) || (mmMask == MM_MASK_GIANT) || !MergeShip_HasMMMask(mmMask) ||
        MERGESHIP_FORM_IS_MM(player->mergeShipForm) || !MergeShip_IsMajorasMaskMounted()) {
        return;
    }
    player->currentMask = PLAYER_MASK_MM_REGULAR(mmMask);
    player->prevMask = player->currentMask;
}

// ---------------------------------------------------------------------------------------------------------
// Per-frame state

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f vel;
    /* 0x18 */ s16 yaw;
    /* 0x1A */ s16 pitch;
} MMHairNode; // MM struct_801F58B0

typedef struct {
    /* 0x00 */ f32 length;
    /* 0x04 */ s16 yawOffset;
    /* 0x06 */ s16 pitchOffset;
    /* 0x08 */ Vec3f unk_08;
    /* 0x14 */ f32 unk_14;
    /* 0x18 */ s16 yawLimit;
    /* 0x1A */ s16 pitchLimit;
} MMHairSegment; // MM struct_80128388_arg1

// MM D_801F58B0: the Great Fairy's Mask's three strands of hair, three nodes each.
static MMHairNode sGreatFairyHair[3][3];

// MM D_801F59B0 / D_801F59C8: the Circus Leader's Mask's two tears.
#define CIRCUS_TEAR_COUNT 2
static Vec3f sCircusTearPos[CIRCUS_TEAR_COUNT];
static s32 sCircusTearSize[CIRCUS_TEAR_COUNT];

// MM sBunnyEarKinematics.
typedef struct {
    Vec3s rot;
    Vec3s angVel;
} MMBunnyEarKinematics;
static MMBunnyEarKinematics sMMBunnyEars;

// MM player->maskId: the mask whose state was last set up (func_801229FC).
static u8 sLoadedMask = PLAYER_MASK_NONE;

// MM func_80127B64.
static void MergeShip_ResetHair(MMHairNode nodes[], s32 count, Vec3f* pos) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f }; // MM gZeroVec3f
    s32 i;

    for (i = 0; i < count; i++, nodes++) {
        Math_Vec3f_Copy(&nodes->pos, pos);
        Math_Vec3f_Copy(&nodes->vel, &sZero);
        nodes->yaw = 0;
        nodes->pitch = 0;
    }
}

// MM Player_UpdateBunnyEars.
static void MergeShip_UpdateMMBunnyEars(Player* player) {
    Vec3s force;
    s16 angle;

    sMMBunnyEars.angVel.x -= sMMBunnyEars.angVel.x >> 3;
    sMMBunnyEars.angVel.y -= sMMBunnyEars.angVel.y >> 3;
    sMMBunnyEars.angVel.x += -sMMBunnyEars.rot.x >> 2;
    sMMBunnyEars.angVel.y += -sMMBunnyEars.rot.y >> 2;

    angle = player->actor.world.rot.y - player->actor.shape.rot.y;
    force.x = (s32)(player->actor.speedXZ * -200.0f * Math_CosS(angle) * (Rand_CenteredFloat(2.0f) + 10.0f)) & 0xFFFF;
    force.y = (s32)(player->actor.speedXZ * 100.0f * Math_SinS(angle) * (Rand_CenteredFloat(2.0f) + 10.0f)) & 0xFFFF;

    sMMBunnyEars.angVel.x += force.x >> 2;
    sMMBunnyEars.angVel.y += force.y >> 2;

    sMMBunnyEars.angVel.x = CLAMP(sMMBunnyEars.angVel.x, -0x1770, 0x1770);
    sMMBunnyEars.angVel.y = CLAMP(sMMBunnyEars.angVel.y, -0x1770, 0x1770);

    sMMBunnyEars.rot.x += sMMBunnyEars.angVel.x;
    sMMBunnyEars.rot.y += sMMBunnyEars.angVel.y;

    if (sMMBunnyEars.rot.x < 0) {
        sMMBunnyEars.rot.z = sMMBunnyEars.rot.x >> 1;
    } else {
        sMMBunnyEars.rot.z = 0;
    }
}

void MergeShip_MMMasks_Update(Player* player, PlayState* play) {
    s32 i;

    (void)play;
    if (player != GET_PLAYER(play)) {
        return;
    }

    // MM Player_UpdateCommon: `if (this->blastMaskTimer != 0) this->blastMaskTimer--`.
    if (gMergeShipBlastMaskTimer != 0) {
        gMergeShipBlastMaskTimer--;
    }

    // MM func_801229FC, without the DMA (a mask's state is set up the frame it changes).
    if ((player->currentMask != PLAYER_MASK_NONE) && (player->currentMask != sLoadedMask)) {
        sLoadedMask = player->currentMask;
        if (player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_GREAT_FAIRY)) {
            for (i = 0; i < ARRAY_COUNT(sGreatFairyHair); i++) {
                MergeShip_ResetHair(sGreatFairyHair[i], ARRAY_COUNT(sGreatFairyHair[i]),
                                    &player->bodyPartsPos[PLAYER_BODYPART_HEAD]);
            }
        }
    } else if (player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_CIRCUS_LEADER)) {
        for (i = 0; i < CIRCUS_TEAR_COUNT; i++) {
            sCircusTearSize[i] += Rand_S16Offset(4, 23) + (s32)(fabsf(player->linearVelocity) * 50.0f);
        }
    }

    // MM Player_UpdateCommon: `if (this->currentMask == PLAYER_MASK_BUNNY) Player_UpdateBunnyEars(this)`.
    if (player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_BUNNY)) {
        MergeShip_UpdateMMBunnyEars(player);
    }

    // MM gSaveContext.save.equippedMask, for a human only (a form's mask is the form). Ocarina of Time's own
    // mask memory (SoH's Persistent Masks) is for its own masks, so it forgets them when one of these goes on.
    if (!MERGESHIP_FORM_IS_MM(player->mergeShipForm) && !player->mergeShipTransforming) {
        u8 mmMask = PLAYER_MASK_TO_MM_MASK(player->currentMask);

        gMergeShipSave.equippedMMMask = mmMask;
        if (mmMask != MM_MASK_NONE) {
            gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------
// Drawing

// MM D_801C0B90: where the Circus Leader's Mask's tears hang.
static Vec3f sCircusTearOffsets[CIRCUS_TEAR_COUNT] = {
    { 950.0f, -800.0f, 300.0f },
    { 950.0f, -800.0f, -300.0f },
};

// MM Player_DrawCircusLeadersMask.
static void MergeShip_DrawCircusLeadersMask(PlayState* play, Player* player) {
    static Vec3f sBubbleVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f sBubbleAccel = { 0.0f, 0.0f, 0.0f };
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    for (i = 0; i < CIRCUS_TEAR_COUNT; i++) {
        f32 scaleY = (sCircusTearSize[i] / 400.0f) * 0.1f;

        Matrix_MultVec3f(&sCircusTearOffsets[i], &sCircusTearPos[i]);
        sCircusTearPos[i].y += -10.0f * scaleY;

        if (sCircusTearSize[i] < 400) {
            f32 scaleXZ = CLAMP_MAX(scaleY, 0.05f);

            MS_INTERP_PUSH();
            Matrix_Translate(sCircusTearPos[i].x, sCircusTearPos[i].y, sCircusTearPos[i].z, MTXMODE_NEW);
            Matrix_Scale(scaleXZ, scaleY, scaleXZ, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)mm_gEffBubble1Tex);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_XLU_DISP++, 150, 150, 150, 0);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gEffBubbleDL);
            gSPPopMatrix(POLY_XLU_DISP++, G_MTX_MODELVIEW);
            MS_INTERP_POP();
        } else {
            f32 speedXZ = sqrtf(SQ(player->actor.velocity.x) + SQ(player->actor.velocity.z));
            s16 yaw = speedXZ * 2000.0f;
            f32 speed;

            sBubbleVelocity.y = speedXZ * 0.4f;
            sBubbleAccel.y = -0.3f;

            if (yaw > 0x3E80) {
                yaw = 0x3E80;
            }
            yaw = player->actor.focus.rot.y + ((i != 0) ? yaw : -yaw);
            speed = speedXZ * 0.2f;
            speed = CLAMP_MAX(speed, 4.0f);

            sBubbleVelocity.x = -Math_SinS(yaw) * speed;
            sBubbleVelocity.z = -Math_CosS(yaw) * speed;

            EffectSsDtBubble_SpawnColorProfile(play, &sCircusTearPos[i], &sBubbleVelocity, &sBubbleAccel, 20, 20, 3,
                                               0);
            sCircusTearSize[i] -= 400;
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM Player_DrawBlastMask. MM also runs the object's animated material (the fuse) while the mask cools down.
static void MergeShip_DrawBlastMask(PlayState* play) {
    static Gfx sOpaqueEnv[] = {
        gsDPSetEnvColor(0, 0, 0, 255),
        gsSPEndDisplayList(),
    };
    static Gfx sSootRenderMode[] = {
        gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              G_RM_FOG_SHADE_A,
                          AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
        gsSPEndDisplayList(),
    };

    OPEN_DISPS(play->state.gfxCtx);

    if (gMergeShipBlastMaskTimer != 0) {
        s32 alpha;

        MergeShip_AnimatedMat_DrawOpa(play, (void*)mm_object_mask_bakuretu_Matanimheader_0011F8);

        if (gMergeShipBlastMaskTimer <= 10) {
            alpha = (gMergeShipBlastMaskTimer / 10.0f) * 255;
        } else {
            alpha = 255;
        }

        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)alpha);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_mask_bakuretu_DL_000440);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSootRenderMode);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)(255 - alpha));
    } else {
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sOpaqueEnv);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM Player_DrawBunnyHood (2S2H's ear length and spread cosmetics are SoH's BunnyHood ones here).
static void MergeShip_DrawMMBunnyHood(PlayState* play) {
    Mtx* mtx = Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Mtx));
    Vec3s earRot;
    f32 earLength = CVarGetFloat(CVAR_COSMETIC("BunnyHood.EarLength"), 0.0f);
    f32 earSpread = CVarGetFloat(CVAR_COSMETIC("BunnyHood.EarSpread"), 0.0f);

    OPEN_DISPS(play->state.gfxCtx);

    // (No FrameInterpolation_RecordActorPosRotMatrix here, unlike Ocarina of Time's Bunny Hood: this runs inside the
    // head limb's post-draw, mid-skeleton, and would replace the player's actor matrix -- and, when its interpolation
    // child closes, turn it off -- for every limb after the head, which then interpolate in the wrong space: at
    // frame rates above 20 the upper body flew off. The ears are recorded against the player's own actor matrix.)
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)mtx);

    // BUG-0041: the ears' matrices are the head's children (segment 0x0B, multiplied in the hood's lists), built directly
    // rather than recorded: blending their swinging angles between game frames (Link turning fast) whipped the ears
    // round; they ride on the head, which is interpolated already.
    {
        MtxF mf;

        earRot.x = sMMBunnyEars.rot.y + 0x3E2;
        earRot.y = sMMBunnyEars.rot.z + 0xDBE;
        earRot.z = sMMBunnyEars.rot.x - 0x348A;
        SkinMatrix_SetTranslateRotateYXZScale(&mf, 1.0f, 1.0f, 1.0f, earRot.x, earRot.y, earRot.z, 97.0f,
                                              -1203.0f - earLength, -240.0f - earSpread);
        guMtxF2L(mf.mf, mtx++);

        earRot.x = sMMBunnyEars.rot.y - 0x3E2;
        earRot.y = -sMMBunnyEars.rot.z - 0xDBE;
        earRot.z = sMMBunnyEars.rot.x - 0x348A;
        SkinMatrix_SetTranslateRotateYXZScale(&mf, 1.0f, 1.0f, 1.0f, earRot.x, earRot.y, earRot.z, 97.0f,
                                              -1203.0f - earLength, 240.0f + earSpread);
        guMtxF2L(mf.mf, mtx);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM D_801C0BE0..D_801C0C54.
static Vec3f sSparkleVelocity = { 0.0f, 0.3f, 0.0f };
static Vec3f sSparkleAccel = { 0.0f, -0.025f, 0.0f };
static Color_RGBA8 sSparklePrimColor = { 250, 100, 100, 0 };
static Color_RGBA8 sSparkleEnvColor = { 0, 0, 100, 0 };
static Vec3f sHairRootOffset = { 0.0f, 20.0f, 0.0f };
static Vec3f sHairAnchors[] = {
    { 174.0f, -1269.0f, -1.0f },
    { 401.0f, -729.0f, -701.0f },
    { 401.0f, -729.0f, 699.0f },
};
static Vec3f sHairAnchorDirs[] = {
    { 74.0f, -1269.0f, -1.0f },
    { 301.0f, -729.0f, -701.0f },
    { 301.0f, -729.0f, 699.0f },
};
static MMHairSegment sHairSegments[] = {
    { 0.0f, 0x0000, 0x8000, { 0.0f, 0.0f, 0.0f }, 0.0f, 0x0000, 0x0000 },
    { 16.8f, 0x0000, 0x0000, { 0.0f, 0.0f, 0.0f }, 20.0f, 0x1388, 0x1388 },
    { 30.0f, 0x0000, 0x0000, { 0.0f, 0.0f, 0.0f }, 20.0f, 0x1F40, 0x2EE0 },
};

// MM Player_DrawStrayFairyParticles.
static void MergeShip_DrawStrayFairyParticles(PlayState* play, Vec3f* pos) {
    Vec3f sparklePos;
    f32 sign;

    sSparkleVelocity.y = Rand_ZeroFloat(0.07f) + -0.1f;
    sSparkleAccel.y = Rand_ZeroFloat(0.1f) + 0.04f;
    sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;
    sSparkleVelocity.x = (Rand_ZeroFloat(0.2f) + 0.1f) * sign;
    sSparkleAccel.x = 0.1f * sign;
    sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;
    sSparkleVelocity.z = (Rand_ZeroFloat(0.2f) + 0.1f) * sign;
    sSparkleAccel.z = 0.1f * sign;
    sparklePos.x = pos->x;
    sparklePos.y = Rand_ZeroFloat(15.0f) + pos->y;
    sparklePos.z = pos->z;
    EffectSsKiraKira_SpawnDispersed(play, &sparklePos, &sSparkleVelocity, &sSparkleAccel, &sSparklePrimColor,
                                    &sSparkleEnvColor, -50, 11);
}

// MM func_80127DA4: one strand's physics.
static void MergeShip_UpdateHairStrand(PlayState* play, MMHairNode nodes[], MMHairSegment segments[], s32 count,
                                       Vec3f* root, Vec3f* dir, u32* sparkleTimer) {
    MMHairNode* node = &nodes[1];
    Vec3f diff;
    Vec3f offset;
    f32 f22;
    f32 stretch;
    f32 f24;
    f32 f20;
    f32 dist;
    f32 lift = -1.0f;
    s32 i;
    s16 pitchDiff;
    s16 yawDiff;

    Math_Vec3f_Copy(&nodes->pos, root);
    Math_Vec3f_Diff(dir, root, &diff);
    nodes->yaw = MM_Atan2S_XY(diff.z, diff.x);
    nodes->pitch = MM_Atan2S_XY(sqrtf(SQXZ(diff)), diff.y);
    i = 1;
    segments++;

    while (i < count) {
        if (gMergeShipStrayFairyNearby) {
            lift = (*sparkleTimer & 0x20) ? -0.2f : 0.2f;
            *sparkleTimer += 0x16;
            if (!(*sparkleTimer & 1)) {
                MergeShip_DrawStrayFairyParticles(play, &node->pos);
            }
        }
        Math_Vec3f_Sum(&node->pos, &node->vel, &node->pos);

        dist = Math_Vec3f_DistXYZAndStoreDiff(&nodes->pos, &node->pos, &diff);
        stretch = dist - segments->length;
        if (dist == 0.0f) {
            diff.x = 0.0f;
            diff.y = segments->length;
            diff.z = 0.0f;
        }
        f20 = sqrtf(SQXZ(diff));

        if (f20 > 4.0f) {
            node->yaw = MM_Atan2S_XY(diff.z, diff.x);
            yawDiff = node->yaw - nodes->yaw;
            if (ABS(yawDiff) > 0x4000) {
                node->yaw = (s16)(node->yaw + 0x8000);
                f20 = -f20;
            }
        }

        node->pitch = MM_Atan2S_XY(f20, diff.y);

        yawDiff = node->yaw - nodes->yaw;
        yawDiff = CLAMP(yawDiff, -segments->yawLimit, segments->yawLimit);
        node->yaw = nodes->yaw + yawDiff;

        pitchDiff = node->pitch - nodes->pitch;
        pitchDiff = CLAMP(pitchDiff, -segments->pitchLimit, segments->pitchLimit);
        node->pitch = nodes->pitch + pitchDiff;

        f20 = Math_CosS(node->pitch) * segments->length;
        offset.x = Math_SinS(node->yaw) * f20;
        offset.z = Math_CosS(node->yaw) * f20;
        offset.y = Math_SinS(node->pitch) * segments->length;
        Math_Vec3f_Sum(&nodes->pos, &offset, &node->pos);
        node->vel.x *= 0.9f;
        node->vel.z *= 0.9f;

        f22 = Math_CosS(pitchDiff) * stretch;
        f24 = Math_SinS(pitchDiff) * stretch;
        node->vel.y += lift;

        if (gMergeShipStrayFairyNearby) {
            node->vel.y = CLAMP(node->vel.y, -0.8f, 0.8f);
        } else {
            f20 = Math_SinS(nodes->pitch);
            node->vel.y += (((f22 * Math_CosS(nodes->pitch)) + (f24 * f20)) * 0.2f);
            node->vel.y = CLAMP(node->vel.y, -2.0f, 4.0f);
        }

        f20 = (f24 * Math_CosS(nodes->pitch)) - (Math_SinS(nodes->pitch) * f22);
        f22 = Math_CosS(yawDiff) * f20;
        f24 = Math_SinS(yawDiff) * f20;

        f20 = Math_SinS(nodes->yaw);
        node->vel.x += (((f24 * Math_CosS(nodes->yaw)) - (f22 * f20)) * 0.1f);
        node->vel.x = CLAMP(node->vel.x, -4.0f, 4.0f);

        f20 = Math_SinS(nodes->yaw);
        node->vel.z += (((f22 * Math_CosS(nodes->yaw)) + (f24 * f20)) * -0.1f);
        node->vel.z = CLAMP(node->vel.z, -4.0f, 4.0f);

        nodes++;
        node++;
        i++;
        segments++;
    }
}

// MM func_80128388: one strand's matrices.
static void MergeShip_HairStrandMatrices(MMHairNode nodes[], MMHairSegment segments[], s32 count, Mtx** mtx) {
    MMHairNode* node = &nodes[1];
    Vec3f translation;
    Vec3s rot;
    s32 i;

    translation.y = 0.0f;
    translation.z = 0.0f;
    rot.x = 0;

    for (i = 1; i < count; i++) {
        translation.x = segments->length * 100.0f;
        rot.z = segments->pitchOffset + (s16)(node->pitch - nodes->pitch);
        rot.y = segments->yawOffset + (s16)(node->yaw - nodes->yaw);
        Matrix_TranslateRotateZYX(&translation, &rot);
        MATRIX_TOMTX(*mtx);
        (*mtx)++;
        nodes++;
        node++;
        segments++;
    }
}

// MM Player_DrawGreatFairysMask.
static void MergeShip_DrawGreatFairysMask(PlayState* play, Player* player) {
    Mtx* mtx = Graph_Alloc(play->state.gfxCtx, 6 * sizeof(Mtx));
    Vec3f root;
    Vec3f dir;
    Vec3f* anchor = sHairAnchors;
    Vec3f* anchorDir = sHairAnchorDirs;
    u32 sparkleTimer = play->gameplayFrames;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)mtx);

    Matrix_MultVec3f(&sHairRootOffset, &sHairSegments[1].unk_08);
    MM_Vec3f_Lerp(&player->bodyPartsPos[PLAYER_BODYPART_HEAD], &player->bodyPartsPos[PLAYER_BODYPART_WAIST], 0.2f,
                  &sHairSegments[2].unk_08);

    for (i = 0; i < ARRAY_COUNT(sHairAnchors); i++) {
        Matrix_MultVec3f(anchor, &root);
        Matrix_MultVec3f(anchorDir, &dir);

        MergeShip_UpdateHairStrand(play, sGreatFairyHair[i], sHairSegments, 3, &root, &dir, &sparkleTimer);
        sparkleTimer += 11;

        MS_INTERP_PUSH();
        Matrix_Translate(anchor->x, anchor->y, anchor->z, MTXMODE_APPLY);
        MergeShip_HairStrandMatrices(sGreatFairyHair[i], sHairSegments, 3, &mtx);
        MS_INTERP_POP();
        anchor++;
        anchorDir++;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// How an MM mask sits on a head it was not modelled for. MM's masks fit MM's child Link (MergeShip's MM
// child); Ocarina of Time's adult and child heads are different limbs. Offsets are in the head limb's space.
// The adult's: one placement per mask (include/mergeship/MaskFit.h, shared with Termina's adult), each value a CVar
// (gMergeShip.MaskFit.Adult.<mask>.<field>). Ocarina of Time's child: one for every mask, its CVars
// gMergeShip.MaskFit.Child.<X|Y|Z|Scale>.
static MsMaskFit sOoTChildMaskFit = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };

static float MergeShip_MaskFitCVar(const char* name, float defaultValue) {
    return CVarGetFloat(name, defaultValue);
}

// The fit of MM mask `mmMask` on this player's head, or NULL for MM's child (the head the masks were made for).
static const MsMaskFit* MergeShip_MaskFitFor(Player* player, u8 mmMask, MsMaskFit* out) {
    if (player->mergeShipForm == PLAYER_FORM_ADULT) {
        *out = MsMaskFit_Adult(mmMask, MergeShip_MaskFitCVar);
        return out;
    }
    if ((player->mergeShipForm == PLAYER_FORM_CHILD) && !MERGESHIP_MM_CHILD(player)) {
        *out = sOoTChildMaskFit;
        out->x = CVarGetFloat("gMergeShip.MaskFit.Child.X", out->x);
        out->y = CVarGetFloat("gMergeShip.MaskFit.Child.Y", out->y);
        out->z = CVarGetFloat("gMergeShip.MaskFit.Child.Z", out->z);
        out->sx = out->sy = out->sz = CVarGetFloat("gMergeShip.MaskFit.Child.Scale", out->sx);
        return out;
    }
    return NULL;
}

// The head's matrices, built in the head limb's post-draw for the mask drawn after the skeleton. They are built
// there, from the matrix stack, so SoH's frame interpolation replays them with the skeleton's (a matrix saved with
// Matrix_Get and put back later would be the game frame's, and the mask would trail Link at high frame rates).
static Mtx* sHeadMtx = NULL;
static Mtx* sHeadMtxList = NULL;

// MM draws the mask from the head limb's post-draw, in the middle of the skeleton. Here only its matrices and its
// moving parts (Bunny Hood ears, Great Fairy's Mask hair, Circus Leader's Mask tears) are set up there; the mask
// itself is drawn after the skeleton, as Ocarina of Time draws its own masks. MM's masks leave their env colour
// and segment 0x09 set (the Blast Mask), which would carry into the limbs drawn after the head (collar, arms,
// torso), and they reload the head's matrix from the skeleton's matrix list (`G_MTX 0x0D0001C0`, entry 7), whose
// order is MM's, not necessarily that of Ocarina of Time's skeletons.
void MergeShip_MMMasks_DrawOnHead(PlayState* play, Player* player) {
    u8 mmMask = PLAYER_MASK_TO_MM_MASK(player->currentMask);
    MsMaskFit fitStorage;
    const MsMaskFit* fit;
    s32 i;

    sHeadMtx = NULL;
    if ((mmMask == MM_MASK_NONE) || MERGESHIP_FORM_IS_MM(player->mergeShipForm) || !MergeShip_IsMajorasMaskMounted()) {
        return;
    }

    fit = MergeShip_MaskFitFor(player, mmMask, &fitStorage);
    MS_INTERP_PUSH();
    if (fit != NULL) {
        Matrix_Translate(fit->x, fit->y, fit->z, MTXMODE_APPLY);
        if (fit->pitch != 0.0f) {
            // A backward tilt is a pitch about world X, which is the head's local Z.
            Matrix_RotateZ(BINANG_TO_RAD((s16)fit->pitch), MTXMODE_APPLY);
        }
        Matrix_Scale(fit->sx, fit->sy, fit->sz, MTXMODE_APPLY);
    }
    sHeadMtx = MATRIX_NEWMTX(play->state.gfxCtx);
    // MM's skeleton matrix list as the masks read it: entry 7 is the head (every entry is, to be safe).
    sHeadMtxList = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Mtx));
    for (i = 0; i < 8; i++) {
        MATRIX_TOMTX(&sHeadMtxList[i]);
    }

    // MM's head-limb block: the masks with moving parts set them up before the mask is drawn.
    switch (mmMask) {
        case MM_MASK_CIRCUS_LEADER:
            MergeShip_DrawCircusLeadersMask(play, player);
            break;
        case MM_MASK_BUNNY:
            MergeShip_DrawMMBunnyHood(play);
            break;
        case MM_MASK_GREAT_FAIRY:
            MergeShip_DrawGreatFairysMask(play, player);
            break;
        default:
            break;
    }
    MS_INTERP_POP();
}

void MergeShip_MMMasks_DrawAfterSkeleton(PlayState* play, Player* player) {
    u8 mmMask = PLAYER_MASK_TO_MM_MASK(player->currentMask);

    // MM Player_Draw: play->actorCtx.flags &= ~ACTORCTX_FLAG_3 once the player is drawn. The Great Fairy's Mask's
    // hair read it during the skeleton (MergeShip_MMMasks_DrawOnHead); stray fairies and their holders raise it again
    // in their next update (MergeShipStrayFairy.h). Cleared first so every return below clears it too.
    gMergeShipStrayFairyNearby = false;

    if (sHeadMtx == NULL) {
        return;
    }
    if ((mmMask == MM_MASK_NONE) || MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        sHeadMtx = NULL;
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, sHeadMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)sHeadMtxList);

    switch (mmMask) {
        case MM_MASK_COUPLE:
            // MM Player_DrawCouplesMask: the object's animated material.
            MergeShip_AnimatedMat_DrawOpa(play, (void*)mm_object_mask_meoto_Matanimheader_001CD8);
            break;
        case MM_MASK_BLAST:
            MergeShip_DrawBlastMask(play);
            break;
        default:
            break;
    }

    if (!GameInteractor_InvisibleLinkActive() &&
        !((mmMask == MM_MASK_BUNNY) && CVarGetInteger(CVAR_ENHANCEMENT("HideBunnyHood"), 0))) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMMMaskDLs[mmMask - 1]);
    }
    sHeadMtx = NULL;

    CLOSE_DISPS(play->state.gfxCtx);
}
