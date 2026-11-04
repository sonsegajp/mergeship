// MergeShip2 -- Majora's Mask's Goron Link: the drawing half that Ocarina of Time's z_player_lib.c calls.
//
// Ported from 2ship2harkinian/mm/src/code/z_player_lib.c (and z_scene_proc.c for the two texture
// animations). Each function names its MM original. The player half is MergeShipGoron.inc.

#include "soh/MergeShip/MergeShipMMScene.h"
#include "global.h"
#include "mm/objects/object_link_goron/object_link_goron.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/forms/MergeShipGoron.h"
#include "soh/MergeShip/MergeShipBombWalls.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include <string.h>

MergeShipGoronState gMergeShipGoron;

// z_player_lib.c (Ocarina of Time's func_80090480 is MM's func_80126440, unchanged).
u8 func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase);

// 2S2H's BEN_ANIM_EQUAL: each translation unit has its own copy of an animation's path string.
static s32 MergeShip_Goron_AnimIs(LinkAnimationHeader* anim, const char* path) {
    return (anim != NULL) && (strcmp((const char*)anim, path) == 0);
}

// ---- MM func_80124618 --------------------------------------------------------------------------

void MergeShip_Goron_func_80124618(MergeShipScaleKey* arg0, f32 curFrame, Vec3f* arg2) {
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
    arg2->x = F32_LERPIMP(temp_f14, arg0->scale.x, progress) * 0.01f;

    temp_f14 = arg0[-1].scale.y;
    arg2->y = F32_LERPIMP(temp_f14, arg0->scale.y, progress) * 0.01f;

    temp_f14 = arg0[-1].scale.z;
    arg2->z = F32_LERPIMP(temp_f14, arg0->scale.z, progress) * 0.01f;
}

// ---- MM AnimatedMat_DrawXlu, for the Goron object's two texture animations -------------------------
// Ocarina of Time has no importer for a standalone TextureAnimation resource, so these two are drawn
// from their data. The values are object_link_goron's own, read from mm.o2r:
//   Matanimheader_013138: one entry, segment 8 (stored -1), type 1 (two-layer scroll),
//                         layers { 0, 0, 64x64 } and { 2, 2, 64x64 }.
//   Matanimheader_014684: one entry, segment 8, type 2 (colour, no interpolation), keyFrameLength 2,
//                         prim { 255,0,0,255 lod 128 } / { 255,155,0,255 lod 128 }, env { 100,0,0,255 } /
//                         { 200,0,0,255 }.
// MM draws both with sMatAnimStep = play->gameplayFrames to the XLU buffer only.

void MergeShip_Goron_DrawMatAnim013138(PlayState* play) {
    u32 step = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    // MM AnimatedMat_TwoLayerTexScroll.
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0 * step, -(s32)(0 * step), 64, 64, 1, 2 * step, -(s32)(2 * step), 64,
                                  64, 0, 0, 2, -2));

    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_Goron_DrawMatAnim014684(PlayState* play) {
    static const u8 sPrimColors[2][5] = { { 255, 0, 0, 255, 128 }, { 255, 155, 0, 255, 128 } };
    static const u8 sEnvColors[2][4] = { { 100, 0, 0, 255 }, { 200, 0, 0, 255 } };
    s32 curFrame = play->gameplayFrames % 2;
    Gfx* gfx = Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));

    OPEN_DISPS(play->state.gfxCtx);

    // MM AnimatedMat_DrawColor / AnimatedMat_SetColor.
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gfx);
    gDPSetPrimColor(gfx++, 0, sPrimColors[curFrame][4], sPrimColors[curFrame][0], sPrimColors[curFrame][1],
                    sPrimColors[curFrame][2], sPrimColors[curFrame][3]);
    gDPSetEnvColor(gfx++, sEnvColors[curFrame][0], sEnvColors[curFrame][1], sEnvColors[curFrame][2],
                   sEnvColors[curFrame][3]);
    gSPEndDisplayList(gfx++);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- MM struct_80122D44 afterimages (z_player_lib.c) ------------------------------------------------

// MM func_80122BA4.
void MergeShip_Goron_func_80122BA4(PlayState* play, struct_80122D44_arg1* arg1, s32 arg2, s32 alpha) {
    if (arg2 == arg1->unk_00) {
        s32 index;

        arg1->unk_01--;
        if (arg1->unk_01 < 0) {
            arg1->unk_01 = 3;
        }

        index = arg1->unk_01;
        arg1->unk_04[index].unk_00 = arg1->unk_00;
        arg1->unk_04[index].alpha = alpha;
        Matrix_Get(&arg1->unk_04[index].mf);

        arg1->unk_00 = 0;
    }
}

// MM func_80122C20.
void MergeShip_Goron_func_80122C20(PlayState* play, struct_80122D44_arg1* arg1) {
    struct_80122D44_arg1_unk_04* temp_v1 = &arg1->unk_04[0];
    s32 phi_a1;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(arg1->unk_04); i++, temp_v1++) {
        if (temp_v1->alpha) {
            phi_a1 = (temp_v1->unk_00 == 3) ? (255 / 3) : (255 / 5);
            if (phi_a1 >= temp_v1->alpha) {
                temp_v1->alpha = 0;
            } else {
                temp_v1->alpha -= phi_a1;
            }
        }
    }
}

// MM Scene_SetRenderModeXlu's renderModeSetXluSingleCycleDL.
typedef struct {
    Color_RGB8 color;
    const char* dList;
} MergeShipGoronAfterimage;

// MM D_801BFDD0.
static MergeShipGoronAfterimage sAfterimages[] = {
    { { 180, 200, 255 }, mm_gLinkGoronCurledDL },
    { { 155, 0, 0 }, mm_gLinkGoronRollingSpikesAndEffectDL },
    { { 255, 0, 0 }, mm_gLinkGoronGoronPunchEffectDL },
};

// MM func_80122D44.
void MergeShip_Goron_func_80122D44(PlayState* play, struct_80122D44_arg1* arg1) {
    MergeShipGoronAfterimage* temp_s3;
    struct_80122D44_arg1_unk_04* phi_s2 = arg1->unk_04;
    s32 phi_s6 = false;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    for (i = 0; i < ARRAY_COUNT(arg1->unk_04); i++) {
        if ((phi_s2->alpha != 0) && (phi_s2->alpha != 255) && (phi_s2->unk_00 >= 1) && (phi_s2->unk_00 <= 3)) {
            temp_s3 = &sAfterimages[phi_s2->unk_00 - 1];
            Matrix_Put(&phi_s2->mf);

            gDPPipeSync(POLY_XLU_DISP++);

            if (!phi_s6 && (phi_s2->unk_00 == 2)) {
                MergeShip_Goron_DrawMatAnim013138(play);
                phi_s6 = true;
            }

            // MM Scene_SetRenderModeXlu(play, 1, 2).
            MergeShip_Scene_SetRenderModeXlu(play, 1, 2);
            gDPSetEnvColor(POLY_XLU_DISP++, temp_s3->color.r, temp_s3->color.g, temp_s3->color.b, phi_s2->alpha);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)temp_s3->dList);
        }

        phi_s2++;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- MM Player_PostLimbDrawGameplay's Goron parts ---------------------------------------------------

// MM D_801C0778 / D_801C07AC: the punch effect's alpha on each frame of pg_punchA / pg_punchB.
static u8 sPunchAAlphas[] = {
    0, 0, 0, 0, 0, 100, 200, 255, 255, 255, 200, 100,
};
static u8 sPunchBAlphas[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 200, 255, 255, 255, 200, 100, 0,
};

// MM D_801C0970 (weapon base) and D_801C09B8 (the Goron's fist, in place of a sword tip).
static Vec3f sFistBase[] = {
    { 0.0f, 400.0f, 0.0f },
    { 0.0f, 1400.0f, -1000.0f },
    { 0.0f, -400.0f, 1000.0f },
};
static Vec3f sFistTip[] = {
    { 0.0f, 750.0f, 750.0f },
    { 1500.0f, 1500.0f, 1500.0f },
    { -2500.0f, -2000.0f, -3000.0f },
};
// MM D_801C0A90 / D_801C0A6C: the butt attack, from the waist.
static Vec3f sButtTip[] = {
    { -400.0f, 1800.0f, 0.0f },
    { 5000.0f, 8000.0f, 4000.0f },
    { 5000.0f, -500.0f, -4000.0f },
};
static Vec3f sButtBase[] = {
    { -400.0f, 800.0f, 0.0f },
    { -5000.0f, -500.0f, -4000.0f },
    { -5000.0f, 8000.0f, 4000.0f },
};

// MM func_80127488: the red punch flash, recorded as an afterimage too.
static void MergeShip_Goron_func_80127488(PlayState* play, Player* player, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gLinkGoronGoronPunchEffectDL);

    MergeShip_Goron_func_80122BA4(play, &player->unk_3D0, 3, alpha);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM func_8012669C. The Goron's fists leave no sword trail (MM skips EffectBlure_AddVertex for the Goron).
static void MergeShip_Goron_func_8012669C(PlayState* play, Player* player, Vec3f* arg2, Vec3f* arg3) {
    Vec3f sp3C;
    Vec3f sp30;

    Matrix_MultVec3f(arg2, &sp3C);
    Matrix_MultVec3f(arg3, &sp30);

    if (player->meleeWeaponState != 0) {
        func_80090480(play, NULL, &player->meleeWeaponInfo[0], &sp3C, &sp30);

        if (player->meleeWeaponState >= 1) {
            // (MM: meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H, true for every Goron punch.)
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

// MM func_80126B8C / func_80126AB4 for the Goron.
static void MergeShip_Goron_func_80126B8C(PlayState* play, Player* player) {
    Math_Vec3f_Copy(&player->unk_AF0[1], &player->meleeWeaponInfo[0].base);
    MergeShip_Goron_func_8012669C(play, player, sFistTip, sFistBase);
}

// MM D_801C0428 (the drums rising out during pg_gakkistart) and D_801C0510 (a drum struck).
static MergeShipScaleKey sDrumsStartKeys[] = {
    { 0, { 0, 0, 0 } },      { 6, { 0, 0, 0 } },        { 7, { 60, 60, 50 } },     { 8, { 120, 130, 100 } },
    { 9, { 100, 120, 80 } }, { 11, { 100, 100, 100 } }, { 13, { 100, 100, 100 } },
};
static MergeShipScaleKey sDrumHitKeys[] = {
    { 0, { 100, 100, 100 } }, { 4, { 100, 100, 100 } }, { 5, { 90, 110, 100 } },
    { 6, { 110, 105, 100 } }, { 8, { 100, 100, 100 } },
};

// MM D_801C0DF0: the five drums around the Goron.
static const char* sDrumDLs[] = {
    mm_object_link_goron_DL_010590, mm_object_link_goron_DL_010368, mm_object_link_goron_DL_010140,
    mm_object_link_goron_DL_00FF18, mm_object_link_goron_DL_00FCF0,
};

s32 MergeShip_Goron_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex, Gfx** dList) {
    LinkAnimationHeader* anim = player->skelAnime.animation;
    s8 mwa = gMergeShipGoron.meleeWeaponAnimation;

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        if ((*dList != NULL) && (player->leftHandType != PLAYER_MODELTYPE_LH_BOTTLE) &&
            MergeShip_Goron_AnimIs(anim, mm_gPlayerAnim_pg_punchA)) {
            s32 frame = (s32)player->skelAnime.curFrame;

            if ((frame >= 0) && (frame < ARRAY_COUNT(sPunchAAlphas))) {
                MergeShip_Goron_func_80127488(play, player, sPunchAAlphas[frame]);
            }
        }

        if ((player->actor.scale.y >= 0.0f) && (player->heldActor == NULL)) {
            if ((player->meleeWeaponState != 0) && (mwa != MM_PLAYER_MWA_GORON_PUNCH_RIGHT) &&
                (mwa != MM_PLAYER_MWA_GORON_PUNCH_BUTT)) {
                MergeShip_Goron_func_80126B8C(play, player);
            }
        }
        // The Goron has no sword: Ocarina of Time's sword collider update is replaced.
        return true;
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        if ((*dList != NULL) && MergeShip_Goron_AnimIs(anim, mm_gPlayerAnim_pg_punchB)) {
            s32 frame = (s32)player->skelAnime.curFrame;

            if ((frame >= 0) && (frame < ARRAY_COUNT(sPunchBAlphas))) {
                MergeShip_Goron_func_80127488(play, player, sPunchBAlphas[frame]);
            }
        }

        if ((player->actor.scale.y >= 0.0f) && (player->meleeWeaponState != 0) &&
            (mwa == MM_PLAYER_MWA_GORON_PUNCH_RIGHT)) {
            MergeShip_Goron_func_80126B8C(play, player);
        }
    } else if (limbIndex == PLAYER_LIMB_TORSO) {
        s32 isStart = MergeShip_Goron_AnimIs(anim, mm_gPlayerAnim_pg_gakkistart);
        s32 isWait = MergeShip_Goron_AnimIs(anim, mm_gPlayerAnim_pg_gakkiwait);

        if (isStart || isWait || MergeShip_Goron_AnimIs(anim, mm_gPlayerAnim_pg_gakkiplay)) {
            Vec3f sp178[ARRAY_COUNT(sDrumDLs)];
            s32 i;

            OPEN_DISPS(play->state.gfxCtx);

            if (isWait) {
                f32* var_v0 = player->unk_B10;

                for (i = 0; i < ARRAY_COUNT(sp178); i++, var_v0++) {
                    MergeShip_Goron_func_80124618(sDrumHitKeys, *var_v0, &sp178[i]);
                }
            } else {
                if (isStart) {
                    MergeShip_Goron_func_80124618(sDrumsStartKeys, player->skelAnime.curFrame, &player->unk_AF0[1]);
                }

                for (i = 0; i < ARRAY_COUNT(sp178); i++) {
                    Math_Vec3f_Copy(&sp178[i], &player->unk_AF0[1]);
                }
            }

            Matrix_Push();
            Matrix_Scale(player->unk_AF0[1].x, player->unk_AF0[1].y, player->unk_AF0[1].z, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_link_goron_DL_00FC18);
            Matrix_Pop();

            for (i = 0; i < ARRAY_COUNT(sp178); i++) {
                Matrix_Push();
                Matrix_Scale(sp178[i].x, sp178[i].y, sp178[i].z, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDrumDLs[i]);
                Matrix_Pop();
            }

            CLOSE_DISPS(play->state.gfxCtx);
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        if ((player->meleeWeaponState != 0) && (mwa == MM_PLAYER_MWA_GORON_PUNCH_BUTT)) {
            Math_Vec3f_Copy(&player->unk_AF0[1], &player->meleeWeaponInfo[0].base);
            MergeShip_Goron_func_8012669C(play, player, sButtTip, sButtBase);
        }
    }

    return false;
}

// MM sPlayerStrengths: the Goron is PLAYER_STRENGTH_GORON, the strongest of MM's forms -- it lifts the
// boulders and powder kegs no other form can. Hyrule's strongest grip is the Golden Gauntlets'.
s32 MergeShip_Goron_GetStrength(void) {
    return PLAYER_STR_GOLD_G;
}

// ---- MergeShip option "MergeShip.GoronBreaksBombWalls" (MergeShipBombWalls.h) ----------------------------

static s32 sGoronStruckBgId = -1;
static u32 sGoronStruckFrame;

static s32 MergeShip_Goron_BreaksBombWalls(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (player != NULL) && (player->mergeShipForm == PLAYER_FORM_GORON) &&
           CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GoronBreaksBombWalls"), 1);
}

// Dodongo's Cavern's entrance keeps Ocarina of Time's rule (a bomb or a bomb flower), for the Goron too: the
// boulder on Death Mountain Trail (Bg_Spot16_Bombstone) and the wall in the cavern's mouth (the Bg_Breakwall at
// (0, 0, 20), switch 7, which the Megaton Hammer's bit would otherwise let the Goron's punch and spikes break).
static s32 MergeShip_IsDodongosCavernEntrance(PlayState* play, Actor* wall) {
    if (wall->id == ACTOR_BG_SPOT16_BOMBSTONE) {
        return true;
    }
    return (play->sceneNum == SCENE_DODONGOS_CAVERN) && (wall->id == ACTOR_BG_BREAKWALL) &&
           (((wall->params & 0x3F) == 7) || ((fabsf(wall->home.pos.x) < 150.0f) &&
                                              (fabsf(wall->home.pos.z - 20.0f) < 150.0f)));
}

u32 MergeShip_BombWallBumper(PlayState* play, Actor* wall, u32 flags) {
    Player* player = GET_PLAYER(play);

    if ((player == NULL) || (player->mergeShipForm != PLAYER_FORM_GORON)) {
        return flags;
    }
    if (!MergeShip_Goron_BreaksBombWalls(play) || MergeShip_IsDodongosCavernEntrance(play, wall)) {
        return flags & ~MERGESHIP_GORON_DMG_PUNCH;
    }
    return flags | MERGESHIP_GORON_DMG_PUNCH;
}

void MergeShip_BombWallSetBumper(PlayState* play, Actor* wall, Collider* collider, u32 flags) {
    u32 dmgFlags = MergeShip_BombWallBumper(play, wall, flags);
    s32 i;

    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            ColliderJntSph* jntSph = (ColliderJntSph*)collider;

            for (i = 0; i < jntSph->count; i++) {
                jntSph->elements[i].info.bumper.dmgFlags = dmgFlags;
            }
            break;
        }
        case COLSHAPE_CYLINDER:
            ((ColliderCylinder*)collider)->info.bumper.dmgFlags = dmgFlags;
            break;
        case COLSHAPE_TRIS: {
            ColliderTris* tris = (ColliderTris*)collider;

            for (i = 0; i < tris->count; i++) {
                tris->elements[i].info.bumper.dmgFlags = dmgFlags;
            }
            break;
        }
        case COLSHAPE_QUAD:
            ((ColliderQuad*)collider)->info.bumper.dmgFlags = dmgFlags;
            break;
        default:
            break;
    }
}

s32 MergeShip_BombWallHitByGoron(PlayState* play, Collider* collider) {
    Player* player = GET_PLAYER(play);

    if ((collider->acFlags & AC_HIT) && (collider->ac == &player->actor) && MergeShip_Goron_BreaksBombWalls(play)) {
        collider->acFlags &= ~AC_HIT;
        return true;
    }
    return false;
}

void MergeShip_Goron_RecordStruckBg(PlayState* play, s32 bgId) {
    sGoronStruckBgId = bgId;
    sGoronStruckFrame = play->gameplayFrames;
}

// MergeShip option "MergeShip.GoronHammerObjects" (MergeShipHammerObjects.h).
s32 MergeShip_Goron_PunchStruckBg(PlayState* play, s32 bgId) {
    if ((bgId >= 0) && (sGoronStruckBgId == bgId) && ((u32)(play->gameplayFrames - sGoronStruckFrame) <= 1)) {
        sGoronStruckBgId = -1;
        return true;
    }
    return false;
}

s32 MergeShip_Goron_StruckBombWall(PlayState* play, DynaPolyActor* wall) {
    Player* player = GET_PLAYER(play);

    if (!MergeShip_Goron_BreaksBombWalls(play) || MergeShip_IsDodongosCavernEntrance(play, &wall->actor)) {
        return false;
    }
    // (Background actors update before the player, so the punch recorded last frame is read this frame.)
    if ((sGoronStruckBgId == wall->bgId) && ((u32)(play->gameplayFrames - sGoronStruckFrame) <= 1)) {
        sGoronStruckBgId = -1;
        return true;
    }
    // The spiked roll (MM PLAYER_STATE3_80000) running into the wall.
    return (player->stateFlags3 & PLAYER_STATE3_80000) && (player->actor.bgCheckFlags & BGCHECKFLAG_WALL) &&
           (player->actor.wallBgId == wall->bgId);
}

// Harness evidence (no-op outside a test script): what the Goron's punch or roll hit, and every bomb wall that
// disappears from the scene.
void MergeShipTest_Log(const char* fmt, ...);
int MergeShipTest_IsActive(void);

void MergeShip_BombWalls_DebugLog(PlayState* play, Player* player) {
    static const s16 sWallIds[] = { ACTOR_BG_BOMBWALL,          ACTOR_BG_BREAKWALL,           ACTOR_BG_HIDAN_KOWARERUKABE,
                                    ACTOR_BG_JYA_BOMBCHUIWA,    ACTOR_BG_JYA_BOMBIWA,         ACTOR_BG_SPOT08_BAKUDANKABE,
                                    ACTOR_BG_SPOT11_BAKUDANKABE, ACTOR_BG_SPOT17_BAKUDANKABE, ACTOR_BG_MIZU_BWALL,
                                    ACTOR_BG_HAKA_ZOU,          ACTOR_BG_SPOT16_BOMBSTONE };
    static Actor* sSeen[16];
    static s32 sSeenCount = -1;
    static PlayState* sSeenPlay;
    Actor* now[16];
    s32 nowCount = 0;
    s32 i;
    s32 j;
    Actor* actor;

    if (!MergeShipTest_IsActive()) {
        return;
    }
    for (i = 0; i < 2; i++) {
        if ((player->meleeWeaponQuads[i].base.atFlags & AT_HIT) && (player->meleeWeaponQuads[i].base.at != NULL)) {
            MergeShipTest_Log("W goron punch hit actor 0x%X", player->meleeWeaponQuads[i].base.at->id);
        }
    }
    if ((player->cylinder.base.atFlags & AT_HIT) && (player->cylinder.base.at != NULL)) {
        MergeShipTest_Log("W goron body hit actor 0x%X", player->cylinder.base.at->id);
    }

    for (actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        for (j = 0; j < ARRAY_COUNT(sWallIds); j++) {
            if ((actor->id == sWallIds[j]) && (nowCount < ARRAY_COUNT(now))) {
                now[nowCount++] = actor;
            }
        }
    }
    if ((sSeenPlay != play) || (sSeenCount < 0)) {
        for (i = 0; i < nowCount; i++) {
            MergeShipTest_Log("W bomb wall present id=0x%X params=%04X pos=(%.0f,%.0f,%.0f)", now[i]->id,
                              (u16)now[i]->params, now[i]->world.pos.x, now[i]->world.pos.y, now[i]->world.pos.z);
        }
    } else {
        for (i = 0; i < sSeenCount; i++) {
            for (j = 0; j < nowCount; j++) {
                if (sSeen[i] == now[j]) {
                    break;
                }
            }
            if (j == nowCount) {
                MergeShipTest_Log("W bomb wall GONE (%p)", (void*)sSeen[i]);
            }
        }
    }
    for (i = 0; i < nowCount; i++) {
        sSeen[i] = now[i];
    }
    sSeenCount = nowCount;
    sSeenPlay = play;
}
