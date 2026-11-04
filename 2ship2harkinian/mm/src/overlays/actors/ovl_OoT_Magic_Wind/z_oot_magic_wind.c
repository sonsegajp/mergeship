/*
 * File: z_oot_magic_wind.c
 * Overlay: ovl_OoT_Magic_Wind
 * Description: MergeShip2 -- Ocarina of Time's Farore's Wind (OoT ovl_Magic_Wind), ported 1:1 to Majora's Mask.
 *
 * Source: soh/src/overlays/actors/ovl_Magic_Wind/z_magic_wind.c. Every function keeps OoT's name with the OoT_ prefix
 * (MagicWind_X -> OoT_Magic_Wind_X). OoT -> MM API differences:
 *   SkelAnimeCurve                               -> SkelCurve (z64curve.h; same layout, same SkelCurve_* functions,
 *                                                   same update/draw logic, MM's framerateDivisor = OoT R_UPDATE_RATE)
 *   MSGMODE_OCARINA_CORRECT_PLAYBACK (OoT 0x0D)  -> MSGMODE_E (MM 0x0E; see z_oot_magic_fire.c)
 *   Player_PlaySfx(&player->actor, sfx)          -> Player_PlaySfx(player, sfx)
 *   Gfx_SetupDL(POLY_XLU_DISP, 25)               -> Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25)
 * OoT's curve skeleton (sSkel, limbs sRootLimb/sInnerCylinderLimb/sOuterCylinderLimb) and curve animation (sAnim)
 * are loaded from the Ocarina archive by 2Ship's own Skeleton/SkeletonLimb/Animation factories, whose binary readers
 * are identical to SoH's for curve skeletons, curve limbs and curve animations.
 *
 * params: 0 = cast (grow, hold, fade out); 1 = shrink (the "warp" variant). Vanilla OoT only ever spawns params 0
 * (Player_SpawnMagicSpell); params 1 is kept because OoT has it.
 */

#include "z_oot_magic_wind.h"
#include "align_asset_macro.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/BenPort.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void OoT_Magic_Wind_Init(Actor* thisx, PlayState* play);
void OoT_Magic_Wind_Destroy(Actor* thisx, PlayState* play);
void OoT_Magic_Wind_Update(Actor* thisx, PlayState* play);
void OoT_Magic_Wind_Draw(Actor* thisx, PlayState* play);

void OoT_Magic_Wind_Shrink(OoTMagicWind* this, PlayState* play);
void OoT_Magic_Wind_WaitForTimer(OoTMagicWind* this, PlayState* play);
void OoT_Magic_Wind_FadeOut(OoTMagicWind* this, PlayState* play);
void OoT_Magic_Wind_WaitAtFullSize(OoTMagicWind* this, PlayState* play);
void OoT_Magic_Wind_Grow(OoTMagicWind* this, PlayState* play);

ActorProfile OoT_Magic_Wind_Profile = {
    /**/ ACTOR_OOT_MAGIC_WIND,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OoTMagicWind),
    /**/ OoT_Magic_Wind_Init,
    /**/ OoT_Magic_Wind_Destroy,
    /**/ OoT_Magic_Wind_Update,
    /**/ OoT_Magic_Wind_Draw,
};

// OoT soh/assets/overlays/ovl_Magic_Wind/ovl_Magic_Wind.h, in the Ocarina archive's "oot/" namespace.
static const ALIGN_ASSET(2) char sSkel[] = OOT_ASSET("overlays/ovl_Magic_Wind/sSkel");
static const ALIGN_ASSET(2) char sAnim[] = OOT_ASSET("overlays/ovl_Magic_Wind/sAnim");
static const ALIGN_ASSET(2) char sCylinderVtx[] = OOT_ASSET("overlays/ovl_Magic_Wind/sCylinderVtx");

static u8 sAlphaUpdVals[] = {
    0x00, 0x03, 0x04, 0x07, 0x09, 0x0A, 0x0D, 0x0F, 0x11, 0x12, 0x15, 0x16, 0x19, 0x1B, 0x1C, 0x1F, 0x21, 0x23,
};

void OoT_Magic_Wind_SetupAction(OoTMagicWind* this, OoTMagicWindFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_Magic_Wind_Init(Actor* thisx, PlayState* play) {
    OoTMagicWind* this = (OoTMagicWind*)thisx;
    Player* player = GET_PLAYER(play);

    // MergeShip2: the spell's models are Ocarina of Time's; without the Ocarina archive there is nothing to draw.
    if (!MergeShip_IsOcarinaMounted()) {
        Actor_Kill(thisx);
        return;
    }

    if (SkelCurve_Init(play, &this->skelCurve, (CurveSkeletonHeader*)sSkel, (CurveAnimationHeader*)sAnim) == 0) {
        // "Magic_Wind_Actor_ct (): Construct failed"
    }
    this->actor.room = -1;
    switch (this->actor.params) {
        case 0:
            SkelCurve_SetAnim(&this->skelCurve, (CurveAnimationHeader*)sAnim, 0.0f, 60.0f, 0.0f, 1.0f);
            this->timer = 29;
            OoT_Magic_Wind_SetupAction(this, OoT_Magic_Wind_WaitForTimer);
            break;
        case 1:
            SkelCurve_SetAnim(&this->skelCurve, (CurveAnimationHeader*)sAnim, 60.0f, 0.0f, 60.0f, -1.0f);
            OoT_Magic_Wind_SetupAction(this, OoT_Magic_Wind_Shrink);
            // "Means start"
            Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
            break;
    }
}

void OoT_Magic_Wind_Destroy(Actor* thisx, PlayState* play) {
    OoTMagicWind* this = (OoTMagicWind*)thisx;

    SkelCurve_Destroy(play, &this->skelCurve);
    Magic_Reset(play);
    // "wipe out"
}

void OoT_Magic_Wind_UpdateAlpha(f32 alpha) {
    s32 i;
    Vtx* vtx = ResourceMgr_LoadVtxByName((char*)sCylinderVtx);

    for (i = 0; i < ARRAY_COUNT(sAlphaUpdVals); i++) {
        vtx[sAlphaUpdVals[i]].n.a = alpha * 255.0f;
    }
}

void OoT_Magic_Wind_WaitForTimer(OoTMagicWind* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->timer > 0) {
        this->timer--;
        return;
    }

    // "Means start"
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_NORMAL);
    OoT_Magic_Wind_UpdateAlpha(1.0f);
    OoT_Magic_Wind_SetupAction(this, OoT_Magic_Wind_Grow);
    SkelCurve_Update(play, &this->skelCurve);
}

void OoT_Magic_Wind_Grow(OoTMagicWind* this, PlayState* play) {
    if (SkelCurve_Update(play, &this->skelCurve)) {
        OoT_Magic_Wind_SetupAction(this, OoT_Magic_Wind_WaitAtFullSize);
        this->timer = 50;
    }
}

void OoT_Magic_Wind_WaitAtFullSize(OoTMagicWind* this, PlayState* play) {
    if (this->timer > 0) {
        this->timer--;
    } else {
        OoT_Magic_Wind_SetupAction(this, OoT_Magic_Wind_FadeOut);
        this->timer = 30;
    }
}

void OoT_Magic_Wind_FadeOut(OoTMagicWind* this, PlayState* play) {
    if (this->timer > 0) {
        OoT_Magic_Wind_UpdateAlpha((f32)this->timer * (1.0f / 30.0f));
        this->timer--;
    } else {
        Actor_Kill(&this->actor);
    }
}

void OoT_Magic_Wind_Shrink(OoTMagicWind* this, PlayState* play) {
    if (SkelCurve_Update(play, &this->skelCurve)) {
        Actor_Kill(&this->actor);
    }
}

void OoT_Magic_Wind_Update(Actor* thisx, PlayState* play) {
    OoTMagicWind* this = (OoTMagicWind*)thisx;

    if ((play->msgCtx.msgMode == MSGMODE_E) || (play->msgCtx.msgMode == MSGMODE_SONG_PLAYED)) {
        Actor_Kill(thisx);
        return;
    }

    this->actionFunc(this, play);
}

s32 OoT_Magic_Wind_OverrideLimbDraw(PlayState* play, SkelCurve* skelCurve, s32 limbIndex, Actor* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    if (limbIndex == 1) {
        gSPSegment(POLY_XLU_DISP++, 8,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->state.frames * 9) & 0xFF,
                                      0xFF - ((play->state.frames * 0xF) & 0xFF), 0x40, 0x40, 1,
                                      (play->state.frames * 0xF) & 0xFF, 0xFF - ((play->state.frames * 0x1E) & 0xFF),
                                      0x40, 0x40, 9, -0xF, 0xF, -0x1E));

    } else if (limbIndex == 2) {
        gSPSegment(POLY_XLU_DISP++, 9,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->state.frames * 3) & 0xFF,
                                      0xFF - ((play->state.frames * 5) & 0xFF), 0x40, 0x40, 1,
                                      (play->state.frames * 6) & 0xFF, 0xFF - ((play->state.frames * 0xA) & 0xFF), 0x40,
                                      0x40, 3, -5, 6, -0xA));
    }

    CLOSE_DISPS(play->state.gfxCtx);

    return true;
}

void OoT_Magic_Wind_Draw(Actor* thisx, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OoTMagicWind* this = (OoTMagicWind*)thisx;

    OPEN_DISPS(gfxCtx);

    if (this->actionFunc != OoT_Magic_Wind_WaitForTimer) {
        POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);
        SkelCurve_Draw(thisx, play, &this->skelCurve, OoT_Magic_Wind_OverrideLimbDraw, NULL, 1, NULL);
    }

    CLOSE_DISPS(gfxCtx);
}
