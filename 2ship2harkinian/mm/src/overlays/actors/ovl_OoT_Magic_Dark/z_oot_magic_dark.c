/*
 * File: z_oot_magic_dark.c
 * Overlay: ovl_OoT_Magic_Dark
 * Description: MergeShip2 -- Ocarina of Time's Nayru's Love (OoT ovl_Magic_Dark), ported 1:1 to Majora's Mask.
 *
 * Source: soh/src/overlays/actors/ovl_Magic_Dark/z_magic_dark.c. Every function keeps OoT's name with the OoT_ prefix
 * (MagicDark_X -> OoT_Magic_Dark_X). OoT -> MM API differences:
 *   gSaveContext.nayrusLoveTimer                 -> gMergeShipNayrusLoveTimer (MergeShipOoTSpells.h; MM's save has no
 *                                                   such field; defined here)
 *   MSGMODE_OCARINA_CORRECT_PLAYBACK (OoT 0x0D)  -> MSGMODE_E (MM 0x0E; see z_oot_magic_fire.c)
 *   LINK_IS_ADULT                                -> Fierce Deity (MM's LINK_IS_ADULT is a constant: MM keeps linkAge
 *                                                   at 0. MM's Link is OoT child-sized, Fierce Deity adult-sized)
 *   roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_5 -> roomCtx.curRoom.type == ROOM_TYPE_BOSS (same field 0x03)
 *   envCtx.adjFogNear / adjFogColor              -> envCtx.adjLightSettings.fogNear / .fogColor
 *   Matrix_RotateY / Matrix_RotateZ (radians)    -> Matrix_RotateYF / Matrix_RotateZF
 *   MATRIX_NEWMTX + gSPMatrix(NOPUSH|LOAD|MV)    -> MATRIX_FINALIZE_AND_LOAD
 *   Gfx_SetupDL_25Xlu                            -> Gfx_SetupDL25_Xlu
 * OoT's assets (sDiamondMaterialDL, sDiamondModelDL and gameplay_keep's gEffFlash1DL) are used as-is from the Ocarina
 * archive ("oot/" prefix). SoH's cosmetic-editor colour overrides are not part of OoT: OoT's colours are used.
 */

#include "z_oot_magic_dark.h"
#include "align_asset_macro.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOoTSpells.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

// OoT's M_PI is the double from <math.h>; keep OoT's double-precision angle conversions.
#define OOT_M_PI 3.14159265358979323846

void OoT_Magic_Dark_Init(Actor* thisx, PlayState* play);
void OoT_Magic_Dark_Destroy(Actor* thisx, PlayState* play);
void OoT_Magic_Dark_OrbUpdate(Actor* thisx, PlayState* play);
void OoT_Magic_Dark_OrbDraw(Actor* thisx, PlayState* play);
void OoT_Magic_Dark_DiamondUpdate(Actor* thisx, PlayState* play);
void OoT_Magic_Dark_DiamondDraw(Actor* thisx, PlayState* play);

void OoT_Magic_Dark_DimLighting(PlayState* play, f32 intensity);

// OoT gSaveContext.nayrusLoveTimer. See MergeShipOoTSpells.h.
s16 gMergeShipNayrusLoveTimer = 0;

ActorProfile OoT_Magic_Dark_Profile = {
    /**/ ACTOR_OOT_MAGIC_DARK,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OoTMagicDark),
    /**/ OoT_Magic_Dark_Init,
    /**/ OoT_Magic_Dark_Destroy,
    /**/ OoT_Magic_Dark_OrbUpdate,
    /**/ OoT_Magic_Dark_OrbDraw,
};

// OoT soh/assets/overlays/ovl_Magic_Dark/ovl_Magic_Dark.h and gameplay_keep, in the Ocarina archive's "oot/" namespace.
static const ALIGN_ASSET(2) char sDiamondMaterialDL[] = OOT_ASSET("overlays/ovl_Magic_Dark/sDiamondMaterialDL");
static const ALIGN_ASSET(2) char sDiamondModelDL[] = OOT_ASSET("overlays/ovl_Magic_Dark/sDiamondModelDL");
static const ALIGN_ASSET(2) char sOoTEffFlash1DL[] = OOT_ASSET("objects/gameplay_keep/gEffFlash1DL");

// OoT's LINK_IS_ADULT for the player that cast the spell.
static s32 OoT_Magic_Dark_LinkIsAdult(Player* player) {
    return player->transformation == PLAYER_FORM_FIERCE_DEITY;
}

void OoT_Magic_Dark_Init(Actor* thisx, PlayState* play) {
    OoTMagicDark* this = (OoTMagicDark*)thisx;
    Player* player = GET_PLAYER(play);

    // MergeShip2: the spell's models are Ocarina of Time's; without the Ocarina archive there is nothing to draw.
    if (!MergeShip_IsOcarinaMounted()) {
        Actor_Kill(thisx);
        return;
    }

    if (!OoT_Magic_Dark_LinkIsAdult(player)) {
        this->scale = 0.4f;
    } else {
        this->scale = 0.6f;
    }

    thisx->world.pos = player->actor.world.pos;
    Actor_SetScale(&this->actor, 0.0f);
    thisx->room = -1;

    if (gMergeShipNayrusLoveTimer != 0) {
        thisx->update = OoT_Magic_Dark_DiamondUpdate;
        thisx->draw = OoT_Magic_Dark_DiamondDraw;
        thisx->scale.x = thisx->scale.z = this->scale * 1.6f;
        thisx->scale.y = this->scale * 0.8f;
        this->timer = 0;
        this->primAlpha = 0;
    } else {
        this->timer = 0;
        gMergeShipNayrusLoveTimer = 0;
    }
}

void OoT_Magic_Dark_Destroy(Actor* thisx, PlayState* play) {
    if (gMergeShipNayrusLoveTimer == 0) {
        Magic_Reset(play);
    }
}

void OoT_Magic_Dark_DiamondUpdate(Actor* thisx, PlayState* play) {
    OoTMagicDark* this = (OoTMagicDark*)thisx;
    u8 phi_a0;
    Player* player = GET_PLAYER(play);
    s16 pad;
    s16 nayrusLoveTimer = gMergeShipNayrusLoveTimer;
    s32 msgMode = play->msgCtx.msgMode;

    if ((msgMode == MSGMODE_E) || (msgMode == MSGMODE_SONG_PLAYED)) {
        Actor_Kill(thisx);
        return;
    }

    if (nayrusLoveTimer >= 1200) {
        player->invincibilityTimer = 0;
        gMergeShipNayrusLoveTimer = 0;
        Actor_Kill(thisx);
        return;
    }

    player->invincibilityTimer = -100;
    thisx->scale.x = thisx->scale.z = this->scale;

    if (this->timer < 20) {
        thisx->scale.x = thisx->scale.z = (1.6f - (this->timer * 0.03f)) * this->scale;
        thisx->scale.y = ((this->timer * 0.01f) + 0.8f) * this->scale;
    } else {
        thisx->scale.x = thisx->scale.z = this->scale;
        thisx->scale.y = this->scale;
    }

    thisx->scale.x *= 1.3f;
    thisx->scale.z *= 1.3f;

    phi_a0 = (this->timer < 20) ? (this->timer * 12) : 255;

    if (nayrusLoveTimer >= 1180) {
        this->primAlpha = 15595 - (nayrusLoveTimer * 13);
        if (nayrusLoveTimer & 1) {
            this->primAlpha = this->primAlpha >> 1;
        }
    } else if (nayrusLoveTimer >= 1100) {
        this->primAlpha = (u8)(nayrusLoveTimer << 7) + 127;
    } else {
        this->primAlpha = 255;
    }

    if (this->primAlpha > phi_a0) {
        this->primAlpha = phi_a0;
    }

    thisx->world.rot.y += 0x3E8;
    thisx->shape.rot.y = thisx->world.rot.y + Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    this->timer++;
    gMergeShipNayrusLoveTimer = nayrusLoveTimer + 1;

    if (nayrusLoveTimer < 1100) {
        Actor_PlaySfx_Flagged(thisx, NA_SE_PL_MAGIC_SOUL_NORMAL - SFX_FLAG);
    } else {
        Actor_PlaySfx_Flagged(thisx, NA_SE_PL_MAGIC_SOUL_FLASH - SFX_FLAG);
    }
}

void OoT_Magic_Dark_DimLighting(PlayState* play, f32 intensity) {
    s32 i;
    f32 colorScale;
    f32 fogScale;

    if (play->roomCtx.curRoom.type != ROOM_TYPE_BOSS) {
        intensity = CLAMP_MIN(intensity, 0.0f);
        intensity = CLAMP_MAX(intensity, 1.0f);
        fogScale = intensity - 0.2f;

        if (intensity < 0.2f) {
            fogScale = 0.0f;
        }

        play->envCtx.adjLightSettings.fogNear = (850.0f - play->envCtx.lightSettings.fogNear) * fogScale;

        if (intensity == 0.0f) {
            for (i = 0; i < ARRAY_COUNT(play->envCtx.adjLightSettings.fogColor); i++) {
                play->envCtx.adjLightSettings.fogColor[i] = 0;
            }
        } else {
            colorScale = intensity * 5.0f;

            if (colorScale > 1.0f) {
                colorScale = 1.0f;
            }

            for (i = 0; i < ARRAY_COUNT(play->envCtx.adjLightSettings.fogColor); i++) {
                play->envCtx.adjLightSettings.fogColor[i] = -(s16)(play->envCtx.lightSettings.fogColor[i] * colorScale);
            }
        }
    }
}

void OoT_Magic_Dark_OrbUpdate(Actor* thisx, PlayState* play) {
    OoTMagicDark* this = (OoTMagicDark*)thisx;
    s32 pad;
    Player* player = GET_PLAYER(play);

    Actor_PlaySfx_Flagged(&this->actor, NA_SE_PL_MAGIC_SOUL_BALL - SFX_FLAG);
    if (this->timer < 35) {
        OoT_Magic_Dark_DimLighting(play, this->timer * (1 / 45.0f));
        Math_SmoothStepToF(&thisx->scale.x, this->scale * (1 / 12.000001f), 0.05f, 0.01f, 0.0001f);
        Actor_SetScale(&this->actor, thisx->scale.x);
    } else if (this->timer < 55) {
        Actor_SetScale(&this->actor, thisx->scale.x * 0.9f);
        Math_SmoothStepToF(&this->orbOffset.y, player->bodyPartsPos[PLAYER_BODYPART_WAIST].y, 0.5f, 3.0f, 1.0f);
        if (this->timer > 48) {
            OoT_Magic_Dark_DimLighting(play, (54 - this->timer) * 0.2f);
        }
    } else {
        thisx->update = OoT_Magic_Dark_DiamondUpdate;
        thisx->draw = OoT_Magic_Dark_DiamondDraw;
        thisx->scale.x = thisx->scale.z = this->scale * 1.6f;
        thisx->scale.y = this->scale * 0.8f;
        this->timer = 0;
        this->primAlpha = 0;
    }

    this->timer++;
}

void OoT_Magic_Dark_DiamondDraw(Actor* thisx, PlayState* play) {
    OoTMagicDark* this = (OoTMagicDark*)thisx;
    s32 pad;
    u16 gameplayFrames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);

    {
        Player* player = GET_PLAYER(play);
        f32 heightDiff;

        this->actor.world.pos.x = player->bodyPartsPos[PLAYER_BODYPART_WAIST].x;
        this->actor.world.pos.z = player->bodyPartsPos[PLAYER_BODYPART_WAIST].z;
        heightDiff = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y - this->actor.world.pos.y;
        if (heightDiff < -2.0f) {
            this->actor.world.pos.y = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y + 2.0f;
        } else if (heightDiff > 2.0f) {
            this->actor.world.pos.y = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y - 2.0f;
        }
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
        Matrix_RotateYF(this->actor.shape.rot.y * (OOT_M_PI / 0x8000), MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 255, 255, (s32)(this->primAlpha * 0.6f) & 0xFF);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        gSPDisplayList(POLY_XLU_DISP++, sDiamondMaterialDL);
        gSPDisplayList(POLY_XLU_DISP++,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, gameplayFrames * 2, gameplayFrames * -4, 32, 32, 1, 0,
                                          gameplayFrames * -16, 64, 32, 2, -4, 0, -16));
        gSPDisplayList(POLY_XLU_DISP++, sDiamondModelDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void OoT_Magic_Dark_OrbDraw(Actor* thisx, PlayState* play) {
    OoTMagicDark* this = (OoTMagicDark*)thisx;
    Vec3f pos;
    Player* player = GET_PLAYER(play);
    s32 pad;
    f32 sp6C = play->state.frames & 0x1F;

    if (this->timer < 32) {
        pos.x = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].x + player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].x) *
                0.5f;
        pos.y = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].y + player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].y) *
                0.5f;
        pos.z = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].z + player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].z) *
                0.5f;
        if (this->timer > 20) {
            pos.y += (this->timer - 20) * 1.4f;
        }
        this->orbOffset = pos;
    } else if (this->timer < 130) {
        pos = this->orbOffset;
    } else {
        return;
    }

    pos.x -= (this->actor.scale.x * 300.0f * Math_SinS(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))) *
              Math_CosS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play))));
    pos.y -= (this->actor.scale.x * 300.0f * Math_SinS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play))));
    pos.z -= (this->actor.scale.x * 300.0f * Math_CosS(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))) *
              Math_CosS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play))));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 150, 255, 255);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Push();
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    Matrix_RotateZF(sp6C * (OOT_M_PI / 32), MTXMODE_APPLY);
    gSPDisplayList(POLY_XLU_DISP++, sOoTEffFlash1DL);
    Matrix_Pop();
    Matrix_RotateZF(-sp6C * (OOT_M_PI / 32), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sOoTEffFlash1DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
