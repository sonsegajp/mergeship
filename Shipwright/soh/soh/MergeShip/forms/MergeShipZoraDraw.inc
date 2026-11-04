// MergeShip2 -- Majora's Mask Zora form: drawing, ported from 2ship2harkinian/mm/src/code/z_player_lib.c.
// Included at the end of z_player_lib.c. See docs/PORTING_GUIDE.md.
//
// The Zora's fins are not part of its skeleton: MM draws them (and their trails, and the punch colliders
// on them) from the forearm limbs, hides the one that is out flying as a boomerang, puffs them up and
// down with keyframed scales on some animations, draws the guitar in the left hand while playing, and
// wraps the body in the barrier's crackling shell. Animation checks compare resource paths, as 2S2H's
// BEN_ANIM_EQUAL does, because each translation unit has its own copy of an animation's path string.

#include "soh/MergeShip/forms/MergeShipZora.h"

// MergeShip2: frame interpolation pairs a frame's matrix operations with the next frame's in order, so a block that is
// drawn on some frames only (a mask on the head, a sword fitted to a hand, a form's extra part) would shift every
// operation after it and stretch a limb for a frame. Each Matrix_Push .. Matrix_Pop here is its own interpolation
// child (SoH's FrameInterpolation_RecordOpenChild), keyed by where it is.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
#define MS_INTERP_PUSH() (FrameInterpolation_RecordOpenChild(__FILE__, __LINE__), Matrix_Push())
#define MS_INTERP_POP() (Matrix_Pop(), FrameInterpolation_RecordCloseChild())

#define ZORA_ANIM_IS(anim, name) MergeShip_AnimIs((anim), mm_##name)
#define ZORA_LERPIMP(v0, v1, t) ((v0) + (((v1) - (v0)) * (t)))

// MM struct_80124618: a keyframed xyz scale in percent.
typedef struct {
    /* 0x0 */ s16 unk_0;
    /* 0x2 */ Vec3s unk_2;
} MergeShipZoraScaleKey;

// MM func_80124618: interpolate a scale keyframe track at curFrame.
static void MergeShip_Zora_func_80124618(MergeShipZoraScaleKey arg0[], f32 curFrame, Vec3f* arg2) {
    s32 currentFrame = curFrame;
    f32 temp_f0;
    f32 temp_f14;
    f32 progress;
    s16 temp_v1;

    do {
        temp_v1 = arg0[1].unk_0;
        arg0++;
    } while (temp_v1 < currentFrame);

    temp_f0 = arg0[-1].unk_0;
    progress = (curFrame - temp_f0) / (temp_v1 - temp_f0);

    temp_f14 = arg0[-1].unk_2.x;
    arg2->x = ZORA_LERPIMP(temp_f14, arg0->unk_2.x, progress) * 0.01f;

    temp_f14 = arg0[-1].unk_2.y;
    arg2->y = ZORA_LERPIMP(temp_f14, arg0->unk_2.y, progress) * 0.01f;

    temp_f14 = arg0[-1].unk_2.z;
    arg2->z = ZORA_LERPIMP(temp_f14, arg0->unk_2.z, progress) * 0.01f;
}

// MM D_801C0538: the guitar appearing in the hand (pz_gakkistart).
static MergeShipZoraScaleKey D_801C0538[] = {
    { 0, { 100, 100, 100 } }, { 5, { 100, 100, 100 } },  { 6, { 0, 0, 0 } },
    { 8, { 100, 100, 100 } }, { 14, { 100, 100, 100 } },
};
// MM D_801C0560: a strum (pz_gakkiplay), with 2S2H's two extra stops against the read past its end.
static MergeShipZoraScaleKey D_801C0560[] = {
    { 0, { 100, 100, 100 } }, { 2, { 95, 95, 100 } },   { 3, { 105, 105, 100 } },
    { 5, { 102, 102, 102 } }, { 0, { 100, 100, 100 } }, { 9, { 100, 100, 100 } },
};
// MM D_801C0580: the fins shrinking away as the guitar is taken out.
static MergeShipZoraScaleKey D_801C0580[] = {
    { 0, { 100, 100, 100 } }, { 9, { 100, 100, 100 } }, { 10, { 150, 150, 150 } },
    { 12, { 0, 0, 0 } },      { 14, { 0, 0, 0 } },
};
// MM D_801C05A8: the fins folding away for the fast swim's roll.
static MergeShipZoraScaleKey D_801C05A8[] = {
    { 0, { 100, 100, 100 } },
    { 6, { 100, 100, 100 } },
    { 7, { 0, 0, 0 } },
    { 17, { 0, 0, 0 } },
};
// MM D_801C05C8: the swimming fins growing in during the roll.
static MergeShipZoraScaleKey D_801C05C8[] = {
    { 0, { 0, 0, 0 } },
    { 17, { 50, 50, 50 } },
};
// MM D_801C05D8: the arm fins coming back after the fast swim.
static MergeShipZoraScaleKey D_801C05D8[] = {
    { 0, { 0, 0, 0 } },
    { 5, { 0, 0, 0 } },
    { 9, { 100, 100, 100 } },
};
// MM D_801C05F0: the swimming fins going away after the fast swim (its first stop is rewritten per frame).
static MergeShipZoraScaleKey D_801C05F0[] = {
    { 0, { 100, 100, 100 } },
    { 5, { 100, 100, 100 } },
    { 9, { 0, 0, 0 } },
};
// MM D_801C0608: the Zora's breathing on pz_wait.
static MergeShipZoraScaleKey D_801C0608[] = {
    { 0, { 100, 100, 100 } },
    { 10, { 100, 100, 100 } },
    { 32, { 100, 115, 105 } },
    { 58, { 100, 101, 100 } },
};
// MM D_801C07F0: the fins drawn out (pz_bladeon).
static MergeShipZoraScaleKey D_801C07F0[] = {
    { 0, { 40, 60, 70 } },    { 3, { 40, 60, 70 } },    { 4, { 75, 90, 85 } },
    { 5, { 110, 120, 100 } }, { 7, { 100, 100, 100 } }, { 8, { 100, 100, 100 } },
};
// MM D_801C0820 / D_801C0838: the fins during the jump kick.
static MergeShipZoraScaleKey D_801C0820[] = {
    { 0, { 100, 100, 100 } },
    { 7, { 100, 100, 100 } },
    { 12, { 40, 60, 70 } },
};
static MergeShipZoraScaleKey D_801C0838[] = {
    { 0, { 40, 60, 70 } },
    { 7, { 40, 60, 70 } },
    { 12, { 100, 100, 100 } },
};

// MM D_801C0AB4 (arm fins), D_801C0ABC (swimming fins), and the swimming fins' trail edges.
static const char* D_801C0AB4[] = { mm_object_link_zora_DL_00CC38, mm_object_link_zora_DL_00CDA0 };
static const char* D_801C0ABC[] = { mm_object_link_zora_DL_010868, mm_object_link_zora_DL_010978 };
static Vec3f D_801C0AC4[] = { { 5400.0f, 1700.0f, 1800.0f }, { 5400.0f, 1700.0f, -1800.0f } };
static Vec3f D_801C0ADC[] = { { 5250.0f, 570.0f, 2400.0f }, { 5250.0f, 570.0f, -2400.0f } };

// MM D_801C09DC / D_801C0A00: the punch colliders along a fin; D_801C0A24 / D_801C0A48: the kick's, along the
// right shin.
static Vec3f D_801C09DC[] = {
    { 900.0f, 300.0f, 100.0f },
    { 1300.0f, 700.0f, -300.0f },
    { 500.0f, -100.0f, 500.0f },
};
static Vec3f D_801C0A00[] = {
    { -2500.0f, 1400.0f, 1100.0f },
    { -2900.0f, 1000.0f, 1500.0f },
    { -2100.0f, 1800.0f, 700.0f },
};
static Vec3f D_801C0A24[] = {
    { 0.0f, 0.0f, 0.0f },
    { -800.0f, 800.0f, 800.0f },
    { -800.0f, -800.0f, -800.0f },
};
static Vec3f D_801C0A48[] = {
    { 2000.0f, 0.0f, 0.0f },
    { 2800.0f, -800.0f, -800.0f },
    { 2800.0f, 800.0f, 800.0f },
};

static Gfx* MergeShip_Zora_Gfx(const char* path) {
    return ResourceMgr_LoadGfxByName(path);
}

// MM's two blures: [0] is Ocarina of Time's single one, [1] the Zora's second (MergeShip_Zora_Init).
static EffectBlure* MergeShip_Zora_Blure(Player* player, s32 index) {
    if (index == 0) {
        return Effect_GetByIndex(player->meleeWeaponEffectIndex);
    }
    return gMergeShipZora.blureValid ? Effect_GetByIndex(gMergeShipZora.blureIndex2) : NULL;
}

// MM func_8012669C: the attack's trail and its two colliders from three points along a limb.
static void MergeShip_Zora_func_8012669C(PlayState* play, Player* player, Vec3f* arg2, Vec3f* arg3) {
    Vec3f sp3C;
    Vec3f sp30;

    Matrix_MultVec3f(arg2, &sp3C);
    Matrix_MultVec3f(arg3, &sp30);

    if (player->meleeWeaponState != 0) {
        if (func_80090480(play, NULL, &player->meleeWeaponInfo[0], &sp3C, &sp30) &&
            !(player->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
            EffectBlure* blure = MergeShip_Zora_Blure(player, 0);

            if (blure != NULL) {
                EffectBlure_AddVertex(blure, &player->meleeWeaponInfo[0].tip, &player->meleeWeaponInfo[0].base);
            }
        }

        if ((player->meleeWeaponState > 0) &&
            ((gMergeShipZora.meleeWeaponAnimation < MM_PLAYER_MWA_SPIN_ATTACK_1H) ||
             (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
            Matrix_MultVec3f(&arg2[1], &sp3C);
            Matrix_MultVec3f(&arg3[1], &sp30);
            func_80090480(play, &player->meleeWeaponQuads[0], &player->meleeWeaponInfo[1], &sp3C, &sp30);
            Matrix_MultVec3f(&arg2[2], &sp3C);
            Matrix_MultVec3f(&arg3[2], &sp30);
            func_80090480(play, &player->meleeWeaponQuads[1], &player->meleeWeaponInfo[2], &sp3C, &sp30);
        }
    } else {
        Math_Vec3f_Copy(&player->meleeWeaponInfo[0].tip, &sp3C);
        Math_Vec3f_Copy(&player->meleeWeaponInfo[0].base, &sp30);
    }
}

// MM Player_DrawZoraShield: the barrier's shell. Its vertex alphas are MM's object_link_zora_U8_011710 scaled
// by the barrier's strength, written into the shell's vertices each frame as MM does; its textures scroll by
// MM's object_link_zora_Matanimheader_012A80 (two two-layer scrolls, segments 0x0A and 0x0B), whose values
// are those in mm.o2r.
void MergeShip_Zora_DrawShield(PlayState* play, Player* player) {
    static u8 sAlphas[80] = {
        0,   0,   126, 126, 176, 176, 0,   0,   0,   126, 176, 0,   0,   126, 176, 0,   0,   126, 176, 0,
        0,   126, 176, 0,   0,   126, 176, 0,   0,   126, 176, 0,   0,   0,   126, 126, 176, 176, 0,   0,
        0,   0,   176, 176, 255, 255, 0,   0,   0,   176, 255, 0,   0,   176, 255, 0,   0,   176, 255, 0,
        0,   176, 255, 0,   0,   176, 255, 0,   0,   176, 255, 0,   0,   0,   176, 176, 255, 255, 0,   0,
    };
    Vtx* vtx;
    f32 scale = player->unk_B62 * (10.0f / 51.0f);
    u32 step = play->gameplayFrames;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    // MM AnimatedMat_DrawXlu(object_link_zora_Matanimheader_012A80).
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, -1 * step, -(20 * step), 32, 64, 1, -2 * step, -(10 * step),
                                  32, 64, -1, -20, -2, -10));
    gSPSegment(POLY_XLU_DISP++, 0x0B,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 3 * step, -(20 * step), 32, 64, 1, -12 * step, -(10 * step),
                                  64, 32, 3, -20, -12, -10));

    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    vtx = ResourceMgr_LoadVtxByName((char*)mm_object_link_zora_Vtx_011210);
    if (vtx != NULL) {
        for (i = 0; i < ARRAY_COUNT(sAlphas); i++) {
            vtx[i].v.cn[3] = (sAlphas[i] * player->unk_B62) >> 8;
        }
    }

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, mm_object_link_zora_DL_011760);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM Player_Draw: the shell around the body, when not in the fast swim (there it follows the swimming body,
// see MergeShip_Zora_OverrideRootLimb).
void MergeShip_Zora_DrawBarrierBody(PlayState* play, Player* player) {
    if ((player->mergeShipForm == PLAYER_FORM_ZORA) && (player->unk_B62 != 0) &&
        !(player->stateFlags3 & PLAYER_STATE3_8000)) {
        MS_INTERP_PUSH();
        Matrix_RotateX(BINANG_TO_RAD(-0x4000), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -1800.0f, MTXMODE_APPLY);
        MergeShip_Zora_DrawShield(play, player);
        MS_INTERP_POP();
    }
}

// MM Player_OverrideLimbDrawGameplayCommon, root limb, the Zora's fast swim: the body pitches with unk_AAA
// (Ocarina of Time's unk_6C2) and rolls with unk_B86[1]; the barrier shell follows it. Returns true when it
// placed the root (Ocarina of Time's own pitch then does not run).
s32 MergeShip_Zora_OverrideRootLimb(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot) {
    if ((player->mergeShipForm != PLAYER_FORM_ZORA) || !(player->stateFlags3 & PLAYER_STATE3_8000)) {
        return false;
    }

    Matrix_Translate(pos->x, ((Math_CosS(player->unk_6C2) - 1.0f) * 200.0f) + pos->y, pos->z, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(player->unk_6C2), MTXMODE_APPLY);

    if (player->unk_B62 != 0) {
        MS_INTERP_PUSH();
        Matrix_RotateZ(BINANG_TO_RAD(player->unk_B8E), MTXMODE_APPLY);
        Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(-0x8000), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -4000.0f, MTXMODE_APPLY);
        MergeShip_Zora_DrawShield(play, player);
        MS_INTERP_POP();
    }

    Matrix_RotateZ(BINANG_TO_RAD(player->unk_B86[1]), MTXMODE_APPLY);
    Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
    // MM func_80125318.
    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0;
    return true;
}

// MM func_80125CE0: place the limb, then scale it by a keyframe track.
static void MergeShip_Zora_func_80125CE0(Player* player, MergeShipZoraScaleKey* arg1, Vec3f* pos, Vec3s* rot) {
    Matrix_TranslateRotateZYX(pos, rot);
    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0;
    MergeShip_Zora_func_80124618(arg1, player->skelAnime.curFrame, &player->unk_AF0[0]);
    Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);
}

// MM Player_OverrideLimbDrawGameplayCommon (torso: breathing and the swimming stretch) and
// Player_OverrideLimbDrawGameplayDefault (hat: widened while swimming), for the Zora.
void MergeShip_Zora_OverrideTorsoLimb(PlayState* play, Player* player, Vec3f* pos, Vec3s* rot) {
    LinkAnimationHeader* anim = player->skelAnime.animation;

    Matrix_TranslateRotateZYX(pos, rot);
    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0;

    if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_wait)) {
        MergeShip_Zora_func_80124618(D_801C0608, player->skelAnime.curFrame, &player->unk_AF0[0]);
    } else {
        player->unk_AF0[0].y = 1.0f;
    }

    player->unk_AF0[0].y *= 1.0f + (0.29999995f * player->unk_B10[0]);
    player->unk_AF0[0].x = 1.0f;
    player->unk_AF0[0].z = 1.0f;
    Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);
}

void MergeShip_Zora_OverrideHatLimb(Player* player) {
    Matrix_Scale((player->unk_B10[0] * 1) + 1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
}

// MM Player_OverrideLimbDrawGameplayDefault, hands, the Zora's paths: open hands while transforming, getting
// an item or swimming; the guitar in the left hand while playing.
void MergeShip_Zora_OverrideHandLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                     Vec3s* rot) {
    s32 openHands = player->mergeShipTransforming || (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) ||
                    ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
                     (gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER));

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        if (openHands) {
            *dList = MergeShip_Zora_Gfx(mm_gLinkZoraLeftHandOpenDL);
        } else {
            LinkAnimationHeader* anim = player->skelAnime.animation;
            s32 phi_a1 = ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkistart) && (player->skelAnime.curFrame >= 6.0f);

            if (phi_a1 || ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkiplay)) {
                *dList = MergeShip_Zora_Gfx(mm_object_link_zora_DL_00E2A0);
                MergeShip_Zora_func_80125CE0(player, phi_a1 ? D_801C0538 : D_801C0560, pos, rot);
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        if (openHands) {
            *dList = MergeShip_Zora_Gfx(mm_gLinkZoraRightHandOpenDL);
        }
    }
}

// MM func_80126BD0: the fin on a forearm (arg2 0 left, 1 right).
static void MergeShip_Zora_func_80126BD0(PlayState* play, Player* player, s32 arg2) {
    LinkAnimationHeader* anim = player->skelAnime.animation;

    if (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT) {
        return;
    }

    if ((arg2 != 0) && (player->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        // The right fin raised as a guard.
        OPEN_DISPS(play->state.gfxCtx);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, mm_object_link_zora_DL_0110A8);

        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        Actor* zoraBoomerangActor = player->zoraBoomerangActor;
        Vec3f sp58;
        Vec3f sp4C;
        s32 drawSwimFins;

        // A fin out flying as a boomerang is not drawn on the arm.
        if (player->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) {
            if (zoraBoomerangActor == NULL) {
                return;
            }
            if ((zoraBoomerangActor->params == arg2) ||
                ((zoraBoomerangActor->child != NULL) && (zoraBoomerangActor->child->params == arg2))) {
                return;
            }
        }

        OPEN_DISPS(play->state.gfxCtx);

        if (!ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkiplay) && !ZORA_ANIM_IS(anim, gPlayerAnim_pz_fishswim)) {
            if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkistart)) {
                MergeShip_Zora_func_80124618(D_801C0580, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_waterroll)) {
                MergeShip_Zora_func_80124618(D_801C05A8, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_swimtowait)) {
                MergeShip_Zora_func_80124618(D_801C05D8, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_bladeon)) {
                MergeShip_Zora_func_80124618(D_801C07F0, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_jumpAT)) {
                MergeShip_Zora_func_80124618(D_801C0820, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_jumpATend)) {
                MergeShip_Zora_func_80124618(D_801C0838, player->skelAnime.curFrame, &player->unk_AF0[0]);
            } else if (player->heldItemAction == PLAYER_IA_MM_ZORA_BOOMERANG) {
                player->unk_AF0[0].x = 1.0f;
                player->unk_AF0[0].y = 1.0f;
                player->unk_AF0[0].z = 1.0f;
            } else {
                // Put away: the fins lie folded.
                player->unk_AF0[0].x = 0.4f;
                player->unk_AF0[0].y = 0.6f;
                player->unk_AF0[0].z = 0.7f;
            }
            MS_INTERP_PUSH();

            Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_OPA_DISP++, D_801C0AB4[arg2]);

            // The punch colliders ride the fin that punches.
            if (player->meleeWeaponState != 0) {
                if (((gMergeShipZora.meleeWeaponAnimation == MM_PLAYER_MWA_ZORA_PUNCH_LEFT) && (arg2 == 0)) ||
                    ((gMergeShipZora.meleeWeaponAnimation == MM_PLAYER_MWA_ZORA_PUNCH_COMBO) && (arg2 != 0))) {
                    MergeShip_Zora_func_8012669C(play, player, D_801C0A00, D_801C09DC);
                }
            }

            MS_INTERP_POP();
        }

        // The swimming fins (MM skips them, and returns, on every other animation).
        drawSwimFins = true;
        if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_waterroll)) {
            MergeShip_Zora_func_80124618(D_801C05C8, player->skelAnime.curFrame, &player->unk_AF0[0]);
            D_801C05F0[0].unk_2.x = 50;
            D_801C05F0[0].unk_2.y = 50;
            D_801C05F0[0].unk_2.z = 50;
            D_801C05F0[1].unk_2 = D_801C05F0[0].unk_2;
        } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_swimtowait)) {
            MergeShip_Zora_func_80124618(D_801C05F0, player->skelAnime.curFrame, &player->unk_AF0[0]);
        } else if (ZORA_ANIM_IS(anim, gPlayerAnim_pz_fishswim)) {
            player->unk_AF0[0].x = (ABS(player->unk_B8A) * 0.00003f) + 0.5f;
            player->unk_AF0[0].y = player->unk_AF0[0].x;
            player->unk_AF0[0].z = player->unk_AF0[0].x;

            D_801C05F0[0].unk_2.x = (player->unk_AF0[0].x * 100.0f);
            D_801C05F0[0].unk_2.y = D_801C05F0[0].unk_2.x;
            D_801C05F0[0].unk_2.z = D_801C05F0[0].unk_2.x;

            D_801C05F0[1].unk_2 = D_801C05F0[0].unk_2;
        } else {
            drawSwimFins = false;
        }

        if (drawSwimFins) {
            MS_INTERP_PUSH();
            Matrix_Scale(player->unk_AF0[0].x, player->unk_AF0[0].y, player->unk_AF0[0].z, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, D_801C0ABC[arg2]);

            Matrix_MultVec3f(&D_801C0AC4[arg2], &sp58);
            Matrix_MultVec3f(&D_801C0ADC[arg2], &sp4C);

            // The swimming fins leave trails in the water.
            if (func_80090480(play, NULL, &player->meleeWeaponInfo[arg2], &sp58, &sp4C) &&
                (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                EffectBlure* blure = MergeShip_Zora_Blure(player, arg2);

                if (blure != NULL) {
                    EffectBlure_AddVertex(blure, &player->meleeWeaponInfo[arg2].tip, &player->meleeWeaponInfo[arg2].base);
                }
            }
            MS_INTERP_POP();
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// MM func_80128640, the Zora guitar case: with the guitar in the left hand, the strumming hand is drawn over it,
// shaken by each strum (unk_B86, MergeShip_Instrument_func_80852290) as far as the body leans.
static void MergeShip_Zora_DrawGuitarHand(PlayState* play, Player* player) {
    LinkAnimationHeader* anim = player->skelAnime.animation;
    s32 guitar;

    if (player->mergeShipTransforming || (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) ||
        ((player->stateFlags1 & PLAYER_STATE1_IN_WATER) && (gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER))) {
        return;
    }

    guitar = (ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkistart) && (player->skelAnime.curFrame >= 6.0f)) ||
             ZORA_ANIM_IS(anim, gPlayerAnim_pz_gakkiplay);
    if (guitar) {
        s16 sp26 = Math_SinS(player->unk_B86[0]) * (ABS(player->upperLimbRot.x) * ((f32)(IREG(52) + 20)) / 100.0f);
        s16 sp24 = Math_SinS(player->unk_B86[1]) * (ABS(player->upperLimbRot.y) * ((f32)(IREG(53) + 15)) / 100.0f);

        OPEN_DISPS(play->state.gfxCtx);

        MS_INTERP_PUSH();
        Matrix_RotateX(BINANG_TO_RAD(sp26), MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(sp24), MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, mm_object_link_zora_DL_00E088);

        MS_INTERP_POP();

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// MM Player_PostLimbDrawGameplay, the Zora's limbs: the fins on the forearms and the kick's colliders on the
// right shin.
void MergeShip_Zora_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex) {
    if (player->mergeShipForm != PLAYER_FORM_ZORA) {
        return;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MergeShip_Zora_DrawGuitarHand(play, player);
    } else if (limbIndex == PLAYER_LIMB_L_FOREARM) {
        MergeShip_Zora_func_80126BD0(play, player, 0);
    } else if (limbIndex == PLAYER_LIMB_R_FOREARM) {
        MergeShip_Zora_func_80126BD0(play, player, 1);
    } else if (limbIndex == PLAYER_LIMB_R_SHIN) {
        if (player->meleeWeaponState != 0) {
            s32 mwa = gMergeShipZora.meleeWeaponAnimation;

            if ((mwa == MM_PLAYER_MWA_ZORA_PUNCH_KICK) || (mwa == MM_PLAYER_MWA_ZORA_JUMPKICK_START) ||
                (mwa == MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH)) {
                MergeShip_Zora_func_8012669C(play, player, D_801C0A48, D_801C0A24);
            }
        }
    }
}

// MM Player_PostLimbDrawGameplay, left hand: the Zora's colliders come from its fins and shin, never from the
// sword placement.
s32 MergeShip_Zora_SkipSwordQuads(Player* this) {
    return this->mergeShipForm == PLAYER_FORM_ZORA;
}

// The Zora's limb overrides in Player_OverrideLimbDrawGameplayCommon/Default (torso and hat).
void MergeShip_Zora_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Vec3f* pos, Vec3s* rot) {
    if (player->mergeShipForm != PLAYER_FORM_ZORA) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_TORSO) {
        MergeShip_Zora_OverrideTorsoLimb(play, player, pos, rot);
    } else if (limbIndex == PLAYER_LIMB_HAT) {
        MergeShip_Zora_OverrideHatLimb(player);
    }
}
