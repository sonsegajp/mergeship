// MergeShip2 -- Majora's Mask's Deku Link drawing, ported from 2ship2harkinian/mm/src/code/z_player_lib.c into
// Ocarina of Time's z_player_lib.c limb callbacks (see MergeShipDekuDraw.h for where each is called).
//
// MM's BEN_ANIM_EQUAL compares animation paths; each translation unit has its own copy of a path string, so the
// comparisons here use strcmp, as 2S2H does.

#include "global.h"
#include <string.h>
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "mm/objects/object_link_nuts/object_link_nuts.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/forms/MergeShipDekuDraw.h"
#include "src/overlays/actors/ovl_MM_En_Arrow/z_mm_en_arrow.h"

// MM struct_80124618: a keyframed scale (percent).
typedef struct MergeShipDekuScaleKey {
    /* 0x0 */ s16 frame;
    /* 0x2 */ Vec3s scale;
} MergeShipDekuScaleKey;

#define DEKU_ROT(binang) BINANG_TO_RAD((s16)(binang))

static Vec3f sMergeShipDekuZeroVec = { 0.0f, 0.0f, 0.0f };

static s32 MergeShip_Deku_AnimIs(LinkAnimationHeader* anim, const char* path) {
    return (anim != NULL) && (strcmp((const char*)anim, path) == 0);
}

// MM func_80124618
static void MergeShip_Deku_func_80124618(MergeShipDekuScaleKey arg0[], f32 curFrame, Vec3f* arg2) {
    s32 currentFrame = curFrame;
    f32 temp_f0;
    f32 temp_f14;
    f32 progress;
    s16 temp_v1;

    do {
        temp_v1 = arg0[1].frame;
        arg0++;
    } while (temp_v1 < currentFrame);

    temp_f0 = arg0[-1].frame;
    progress = (curFrame - temp_f0) / (temp_v1 - temp_f0);

    temp_f14 = arg0[-1].scale.x;
    arg2->x = (temp_f14 + ((arg0->scale.x - temp_f14) * progress)) * 0.01f;

    temp_f14 = arg0[-1].scale.y;
    arg2->y = (temp_f14 + ((arg0->scale.y - temp_f14) * progress)) * 0.01f;

    temp_f14 = arg0[-1].scale.z;
    arg2->z = (temp_f14 + ((arg0->scale.z - temp_f14) * progress)) * 0.01f;
}

// MM D_801C0340 / D_801C0368: the pipes' base and pipes while taking them out (pn_gakkistart).
static MergeShipDekuScaleKey D_801C0340[] = {
    { 0, { 0, 0, 0 } }, { 5, { 0, 0, 0 } }, { 7, { 100, 100, 100 } }, { 9, { 110, 110, 110 } }, { 11, { 100, 100, 100 } },
};
static MergeShipDekuScaleKey D_801C0368[] = {
    { 0, { 0, 0, 0 } },       { 4, { 0, 0, 0 } },      { 6, { 120, 150, 60 } },   { 8, { 130, 80, 160 } },
    { 9, { 100, 100, 100 } }, { 10, { 90, 100, 90 } }, { 11, { 100, 100, 100 } },
};
// MM D_801C03C0: the head while spitting the bubble (pn_tamahaki).
static MergeShipDekuScaleKey D_801C03C0[] = {
    { 0, { 200, 100, 110 } },
    { 2, { 90, 100, 100 } },
    { 3, { 100, 100, 100 } },
    { 7, { 100, 100, 100 } },
};
// MM D_801C03E0: the head at the end of a drink (pn_drinkend).
static MergeShipDekuScaleKey D_801C03E0[] = {
    { 0, { 100, 100, 110 } }, { 2, { 60, 100, 80 } },    { 3, { 130, 105, 110 } },
    { 7, { 130, 105, 110 } }, { 10, { 100, 100, 100 } }, { 19, { 100, 100, 100 } },
};
// MM D_801C0410: the guard shell (pn_gurd).
static MergeShipDekuScaleKey D_801C0410[] = {
    { 0, { 0, 0, 0 } },
    { 2, { 80, 110, 80 } },
    { 3, { 100, 100, 100 } },
};
// MM D_801C0678 ... D_801C0740: the flight petals (stem, flower) per animation.
static MergeShipDekuScaleKey D_801C0678[] = {
    { 0, { 0, 0, 0 } },
    { 6, { 0, 0, 0 } },
    { 8, { 100, 100, 100 } },
    { 11, { 100, 100, 100 } },
};
static MergeShipDekuScaleKey D_801C0698[] = {
    { 0, { 0, 0, 0 } },
    { 7, { 1120, 112, 112 } },
    { 8, { 140, 168, 168 } },
    { 11, { 140, 140, 140 } },
};
static MergeShipDekuScaleKey D_801C06B8[] = {
    { 0, { 100, 100, 100 } }, { 7, { 100, 100, 100 } }, { 8, { 70, 30, 70 } }, { 10, { 0, 0, 0 } }, { 14, { 0, 0, 0 } },
};
static MergeShipDekuScaleKey D_801C06E0[] = {
    { 0, { 140, 140, 140 } },
    { 1, { 0, 0, 0 } },
    { 14, { 0, 0, 0 } },
};
static MergeShipDekuScaleKey D_801C06F8[] = {
    { 0, { 100, 100, 100 } },
    { 5, { 100, 100, 100 } },
    { 6, { 0, 0, 0 } },
    { 10, { 0, 0, 0 } },
};
static MergeShipDekuScaleKey D_801C0718[] = {
    { 0, { 140, 140, 140 } },
    { 1, { 0, 0, 0 } },
    { 10, { 0, 0, 0 } },
};
static MergeShipDekuScaleKey D_801C0730[] = {
    { 0, { 100, 100, 100 } },
    { 13, { 100, 100, 100 } },
};
static MergeShipDekuScaleKey D_801C0740[] = {
    { 0, { 140, 140, 140 } },
    { 13, { 140, 140, 140 } },
};

static MergeShipDekuScaleKey* D_801C0AF4[] = { D_801C0678, D_801C0698 }; // pn_kakku
static MergeShipDekuScaleKey* D_801C0AFC[] = { D_801C06B8, D_801C06E0 }; // pn_kakkufinish
static MergeShipDekuScaleKey* D_801C0B04[] = { D_801C06F8, D_801C0718 }; // pn_rakkafinish
static MergeShipDekuScaleKey* D_801C0B0C[] = { D_801C0730, D_801C0740 }; // pn_batabata

static const char* D_801C0B14[] = {
    mm_object_link_nuts_DL_008760,
    mm_object_link_nuts_DL_008660,
};

static u8 D_801C0B1C[] = {
    PLAYER_BODYPART_L_HAND,
    PLAYER_BODYPART_R_HAND,
};

// MM D_801C0E2C: the five pipes.
static const char* D_801C0E2C[] = {
    mm_object_link_nuts_DL_007A28, mm_object_link_nuts_DL_0077D0, mm_object_link_nuts_DL_007548,
    mm_object_link_nuts_DL_007900, mm_object_link_nuts_DL_0076A0,
};

// MM D_801C0958 / func_80125340 / func_8012536C: the head's extra matrix, popped after its post-limb draw.
static s32 sDekuHeadMatrixPushed = false;

static void MergeShip_Deku_func_80125340(void) {
    Matrix_Push();
    sDekuHeadMatrixPushed = true;
}

static void MergeShip_Deku_func_8012536C(void) {
    if (sDekuHeadMatrixPushed) {
        Matrix_Pop();
        sDekuHeadMatrixPushed = false;
    }
}

// MM func_80125318
static void MergeShip_Deku_func_80125318(Vec3f* arg0, Vec3s* arg1) {
    arg0->x = 0.0f;
    arg0->y = 0.0f;
    arg0->z = 0.0f;
    arg1->x = 0;
    arg1->y = 0;
    arg1->z = 0;
}

// MM func_80124F18
static void MergeShip_Deku_func_80124F18(s16* arg0, f32* arg1, s16 arg2, f32 arg3, f32 arg4) {
    if (*arg0 < arg2) {
        *arg1 += arg3;
    } else {
        *arg1 -= arg3;
    }

    *arg1 = CLAMP(*arg1, -arg4, arg4);
    *arg0 += (s16)(s32)*arg1;
    if (((arg2 - *arg0) * (s16)(s32)*arg1) < 0) {
        *arg0 = arg2;
    }
}

// MM func_80124FF0 (MM Math_Atan2S_XY is Ocarina of Time's Math_Atan2S).
static void MergeShip_Deku_func_80124FF0(f32 arg0, s16 arg1, Vec3f* arg2, s16 arg3, Vec3f* arg4, Vec3f* arg5,
                                         s16* arg6, f32* arg7, f32 arg8, s16 arg9) {
    Vec3f sp44;
    f32 sp40;
    s16 temp_v0;
    s16 sp3C;
    f32 sp34 = Math_CosS(arg1) * arg0;
    f32 sp30 = Math_SinS(arg1) * -arg0;

    arg5->x = (Math_SinS(arg3) * sp30) + arg2->x;
    arg5->y = arg2->y + sp34;
    arg5->z = (Math_CosS(arg3) * sp30) + arg2->z;

    Math_Vec3f_Diff(arg5, arg4, &sp44);
    sp40 = sqrtf(SQ(sp44.x) + SQ(sp44.z));

    sp3C = (sp40 <= 1.0f) ? arg3 : Math_Atan2S(sp44.z, sp44.x);
    sp40 = (Math_CosS(sp3C - arg3) * sp40) + arg8;

    if (ABS((s16)(sp3C - arg3)) > 0x4000) {
        sp3C = (s16)(sp3C + 0x8000);
    }
    sp3C -= arg3;

    temp_v0 = Math_Atan2S(sp44.y, sp40);
    temp_v0 = CLAMP(temp_v0, (s16)-arg9, arg9);

    MergeShip_Deku_func_80124F18(arg6, arg7, temp_v0, 20.0f, 2000.0f);
}

// MM func_801251C4: the body hangs from the petals while flying.
static void MergeShip_Deku_func_801251C4(Player* player, Vec3f* arg1) {
    Vec3f sp4C;
    Vec3f sp40;

    sp4C.x = player->actor.world.pos.x;
    sp4C.y = player->actor.world.pos.y + 60.0f;
    sp4C.z = player->actor.world.pos.z;
    MergeShip_Deku_func_80124FF0(-20.0f, player->unk_B8C, &sp4C, player->actor.shape.rot.y,
                                 &player->bodyPartsPos[PLAYER_BODYPART_WAIST], arg1, &player->unk_B90,
                                 &player->unk_B10[0], 0.0f, 0x1F40);

    sp40.x = (player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].x + player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].x) / 2.0f;
    sp40.y = (player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y + player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].y) / 2.0f;
    sp40.z = (player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].z + player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].z) / 2.0f;
    MergeShip_Deku_func_80124FF0(-20.0f, player->unk_B90, arg1, (player->actor.shape.rot.y + player->unk_B92), &sp40,
                                 arg1, &player->unk_B94, &player->unk_B10[2], -1.9f, 0x1F40);
}

void MergeShip_Deku_OverrideLimbRoot(Player* player, Vec3f* pos, Vec3s* rot) {
    // MM Player_OverrideLimbDrawGameplayCommon, PLAYER_LIMB_ROOT, `else if (stateFlags3 & PLAYER_STATE3_2000)`.
    if (player->stateFlags3 & PLAYER_STATE3_2000) {
        Vec3f sp54;

        MergeShip_Deku_func_801251C4(player, &sp54);
        sp54.x -= player->actor.world.pos.x;
        sp54.y -= player->actor.world.pos.y;
        sp54.z -= player->actor.world.pos.z;
        Matrix_Translate(pos->x + sp54.x, pos->y + sp54.y, pos->z + sp54.z, MTXMODE_APPLY);
        Matrix_RotateX(DEKU_ROT(player->unk_B94), MTXMODE_APPLY);
        Matrix_RotateZ(DEKU_ROT(player->unk_B8E), MTXMODE_APPLY);
        player->upperLimbRot.x = player->unk_B90 - player->unk_B94;
        Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
        MergeShip_Deku_func_80125318(pos, rot);
    }
}

// The held bubble's size (MM EnArrow::bubble.unk_144).
static f32 MergeShip_Deku_GetBubbleSize(Actor* bubble) {
    if (bubble->id != gMergeShipActorId_MM_En_Arrow) {
        return 0.0f;
    }
    return ((MMEnArrow*)bubble)->bubble.unk_144;
}

void MergeShip_Deku_OverrideLimbHead(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot) {
    Actor* bubble = NULL;

    // MM PLAYER_STATE3_40 (aiming) is Ocarina of Time's PLAYER_STATE1_READY_TO_FIRE; OoT gives bit 6 of
    // stateFlags3 to Nayru's Love.
    if (MergeShip_Deku_AnimIs(player->skelAnime.animation, mm_gPlayerAnim_pn_drinkend) ||
        MergeShip_Deku_AnimIs(player->upperSkelAnime.animation, mm_gPlayerAnim_pn_tamahaki) ||
        ((player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && ((bubble = player->heldActor) != NULL))) {
        Matrix_TranslateRotateZYX(pos, rot);
        MergeShip_Deku_func_80125340();
        MergeShip_Deku_func_80125318(pos, rot);

        if (bubble != NULL) {
            f32 size = MergeShip_Deku_GetBubbleSize(bubble);

            player->unk_AF0[0].x = 1.0f - (size * 0.03f);
            player->unk_AF0[0].y = 1.0f - (size * 0.01f);
            player->unk_AF0[0].z = 1.0f - (size * 0.04f);
            rot->z = size * 320.0f;
        } else if (MergeShip_Deku_AnimIs(player->skelAnime.animation, mm_gPlayerAnim_pn_drinkend)) {
            MergeShip_Deku_func_80124618(D_801C03E0, player->skelAnime.curFrame, player->unk_AF0);
        } else {
            MergeShip_Deku_func_80124618(D_801C03C0, player->upperSkelAnime.curFrame, player->unk_AF0);
        }

        Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);
    }
}

void MergeShip_Deku_OverrideLimbUpper(Player* player) {
    // MM: rotX += bubble.unk_144 * -470 while aiming. Ocarina of Time has already applied upperLimbRot.x; the Deku's
    // upperLimbRot.z is never set while aiming (MM Player_UpperAction_7 skips it for the Deku), so the order holds.
    if ((player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (player->heldActor != NULL)) {
        Matrix_RotateX(DEKU_ROT((s16)(s32)(MergeShip_Deku_GetBubbleSize(player->heldActor) * -470.0f)), MTXMODE_APPLY);
    }
}

// MM func_801271B0: the propeller petals held in each hand while flying.
static s32 MergeShip_Deku_func_801271B0(PlayState* play, Player* player, s32 arg2) {
    LinkAnimationHeader* anim = player->skelAnime.animation;

    if (MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_kakku) || MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_kakkufinish) ||
        MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_rakkafinish) ||
        MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_batabata)) {
        MergeShipDekuScaleKey** sp3C = D_801C0AF4;

        OPEN_DISPS(play->state.gfxCtx);

        if (MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_kakkufinish)) {
            sp3C = D_801C0AFC;
        } else if (MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_batabata)) {
            sp3C = D_801C0B0C;
        } else if (MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_rakkafinish)) {
            sp3C = D_801C0B04;
        }

        Matrix_Push();
        Matrix_Translate(0.0f, 150.0f, 0.0f, MTXMODE_APPLY);
        MergeShip_Deku_func_80124618(sp3C[0], player->skelAnime.curFrame, &player->unk_AF0[1]);
        Matrix_Scale(player->unk_AF0[1].x, player->unk_AF0[1].y, player->unk_AF0[1].z, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)D_801C0B14[arg2]);

        Matrix_Translate(2150.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(DEKU_ROT(player->unk_B8A), MTXMODE_APPLY);
        MergeShip_Deku_func_80124618(sp3C[1], player->skelAnime.curFrame, &player->unk_AF0[1]);
        Matrix_Scale(player->unk_AF0[1].x, player->unk_AF0[1].y, player->unk_AF0[1].z, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

        // Close flower / Open flower
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)((player->actor.velocity.y < -6.0f) ? mm_gLinkDekuClosedFlowerDL
                                                                                   : mm_gLinkDekuOpenFlowerDL));

        Matrix_MultVec3f(&sMergeShipDekuZeroVec, &player->bodyPartsPos[D_801C0B1C[arg2]]);
        Matrix_Pop();

        CLOSE_DISPS(play->state.gfxCtx);

        return true;
    }

    return false;
}

// MM Player_PostLimbDrawGameplay, PLAYER_LIMB_HEAD, Deku branch: the guard shell and the pipes.
static void MergeShip_Deku_DrawHeadAttachments(PlayState* play, Player* player) {
    LinkAnimationHeader* anim = player->skelAnime.animation;

    if (MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_gurd)) {
        OPEN_DISPS(play->state.gfxCtx);

        MergeShip_Deku_func_80124618(D_801C0410, player->skelAnime.curFrame, player->unk_AF0);
        Matrix_Push();

        Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_link_nuts_DL_00A348);

        Matrix_Pop();

        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        s32 temp_v1_5 = MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_gakkistart);

        if (temp_v1_5 || MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_pn_gakkiplay) ||
            MergeShip_Deku_AnimIs(anim, mm_gPlayerAnim_dl_kokeru)) {
            Vec3f spF0[ARRAY_COUNT(D_801C0E2C)];
            s32 i;
            f32* temp;

            OPEN_DISPS(play->state.gfxCtx);

            // MM keeps these in unk_AF0/arr_AF0, which alias there; MergeShip's Player keeps arr_AF0 separate, so
            // the pipe scales live in arr_AF0 only (arr_AF0[0] the base, arr_AF0[1..5] the pipes).
            if (temp_v1_5) {
                Vec3f spD4;

                MergeShip_Deku_func_80124618(D_801C0340, player->skelAnime.curFrame, &spD4);
                player->arr_AF0[0] = spD4.x;
                MergeShip_Deku_func_80124618(D_801C0368, player->skelAnime.curFrame, spF0);

                for (i = 0; i < ARRAY_COUNT(spF0) - 1; i++) {
                    Math_Vec3f_Copy(&spF0[i + 1], spF0);
                }

                temp = &player->arr_AF0[1];
                for (i = 0; i < ARRAY_COUNT(spF0); i++, temp++) {
                    *temp = spF0[0].x;
                }
            } else {
                temp = &player->arr_AF0[1];
                for (i = 0; i < ARRAY_COUNT(spF0); i++, temp++) {
                    spF0[i].x = *temp;
                    spF0[i].y = *temp;
                    spF0[i].z = *temp;
                }
            }

            Matrix_Push();
            Matrix_Scale(player->arr_AF0[0], player->arr_AF0[0], player->arr_AF0[0], MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_link_nuts_DL_007390);

            Matrix_Pop();

            for (i = 0; i < ARRAY_COUNT(spF0); i++) {
                Matrix_Push();

                Matrix_Scale(spF0[i].x, spF0[i].y, spF0[i].z, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)D_801C0E2C[i]);

                Matrix_Pop();
            }

            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

void MergeShip_Deku_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* curBodyPartPos) {
    if (player->mergeShipForm != PLAYER_FORM_DEKU) {
        MergeShip_Deku_func_8012536C();
        return;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        if (*dList != NULL) {
            MergeShip_Deku_func_801271B0(play, player, 0);
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        MergeShip_Deku_func_801271B0(play, player, 1);
    } else if (limbIndex == PLAYER_LIMB_HEAD) {
        MergeShip_Deku_DrawHeadAttachments(play, player);

        if (player->actor.scale.y >= 0.0f) {
            static Vec3f D_801C0E94 = { 1800.0f, -300.0f, 0.0f };
            Actor* spA8 = NULL;

            // (Ocarina of Time has already set focus.pos from the head.)
            if (curBodyPartPos != NULL) {
                Matrix_MultVec3f(&D_801C0E94, curBodyPartPos);
            }
            if (MergeShip_Deku_AnimIs(player->skelAnime.animation, mm_gPlayerAnim_pn_drinkend) ||
                MergeShip_Deku_AnimIs(player->upperSkelAnime.animation, mm_gPlayerAnim_pn_tamahaki) ||
                ((player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && ((spA8 = player->heldActor) != NULL))) {
                if (spA8 != NULL) {
                    static Vec3f D_801C0EA0 = { 1300.0f, -400.0f, 0.0f };
                    MtxF sp68;

                    Matrix_Push();
                    Matrix_MultVec3f(&D_801C0EA0, &spA8->world.pos);
                    Matrix_RotateZYX(0, 0x4000, 0, MTXMODE_APPLY);
                    Matrix_Get(&sp68);
                    Matrix_MtxFToYXZRotS(&sp68, &spA8->world.rot, 0);
                    spA8->shape.rot = spA8->world.rot;
                    Matrix_Pop();
                }
            }
        }
    } else if ((limbIndex == PLAYER_LIMB_HAT) && (player->stateFlags3 & PLAYER_STATE3_100000)) {
        // MM: the spin attack's trail follows the Deku's hat (meleeWeaponEffectIndex[0]; Ocarina of Time's player
        // has one blure).
        Vec3f sp5C;
        Vec3f sp50;
        static Vec3f sTip = { 3000.0f, 0.0f, 0.0f };
        static Vec3f sBase = { 2300.0f, 0.0f, 0.0f };

        Matrix_MultVec3f(&sTip, &sp5C);
        Matrix_MultVec3f(&sBase, &sp50);
        if (func_80090480(play, NULL, player->meleeWeaponInfo, &sp5C, &sp50)) {
            EffectBlure_AddVertex(Effect_GetByIndex(player->meleeWeaponEffectIndex), &player->meleeWeaponInfo[0].tip,
                                  &player->meleeWeaponInfo[0].base);
        }
    }

    // MM calls func_8012536C at the end of every Player_PostLimbDrawGameplay.
    MergeShip_Deku_func_8012536C();
}
