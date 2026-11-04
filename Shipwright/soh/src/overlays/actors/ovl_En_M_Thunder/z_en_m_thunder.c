#include "z_en_m_thunder.h"
#include "src/overlays/actors/ovl_MS_Ganon_Magic/z_ms_ganon_magic.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipSohFd.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define FLAGS 0

void EnMThunder_Init(Actor* thisx, PlayState* play);
void EnMThunder_Destroy(Actor* thisx, PlayState* play);
void EnMThunder_Update(Actor* thisx, PlayState* play);
void EnMThunder_Draw(Actor* thisx, PlayState* play);

void EnMThunder_AdjustEnvLights(PlayState* play, f32 intensity);
void EnMThunder_ChargingSpinAttack(EnMThunder* this, PlayState* play);
void EnMThunder_SpinAttacking(EnMThunder* this, PlayState* play);
void EnMThunder_SwordBeamAttack(EnMThunder* this, PlayState* play); // MergeShip2 (soh_fd): the Fierce Deity's beam

// MergeShip2 (soh_fd): the Fierce Deity's sword beam hits as a Master Sword slash. Damage tables are read at the
// highest set bit of dmgFlags, and Ocarina of Time has no sword-beam type: MM's DMG_SWORD_BEAM is bit 31, which is
// Ocarina of Time's unused DMG_UNKNOWN_2 (0 damage in every table). DMG_SLASH_MASTER is the most universal enemy
// weakness, and what the Fierce Deity's blade itself deals.
#define DMG_SWORDBEAM DMG_SLASH_MASTER

const ActorInit En_M_Thunder_InitVars = {
    ACTOR_EN_M_THUNDER,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnMThunder),
    (ActorFunc)EnMThunder_Init,
    (ActorFunc)EnMThunder_Destroy,
    (ActorFunc)EnMThunder_Update,
    (ActorFunc)EnMThunder_Draw,
    NULL,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000001, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 200, 200, 0, { 0, 0, 0 } },
};

static u32 sSpinAttackDmgFlags[] = { 0x01000000, 0x00400000, 0x00800000 };
static u32 sJumpAttackDmgFlags[] = { 0x08000000, 0x02000000, 0x04000000 };

static u16 sSfxIds[] = {
    NA_SE_IT_ROLLING_CUT_LV2,
    NA_SE_IT_ROLLING_CUT_LV1,
    NA_SE_IT_ROLLING_CUT_LV2,
    NA_SE_IT_ROLLING_CUT_LV1,
};

void EnMThunder_SetupAction(EnMThunder* this, EnMThunderActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnMThunder_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->swordType = (this->actor.params & 0xFF) - 1;
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, 255, 255, 255, 0);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    this->collider.dim.radius = 0;
    this->collider.dim.height = 40;
    this->collider.dim.yShift = -20;
    this->followPlayerTimer = 8;
    this->spinTrailTexScroll = 0.0f;
    this->actor.world.pos = player->bodyPartsPos[0];
    this->spinAttackTimer = 0.0f;
    this->dimmingIntensity = 0.0f;
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.room = -1;
    Actor_SetScale(&this->actor, 0.1f);
    this->isUsingMagic = 0;

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        // MergeShip2 (soh_fd): the Fierce Deity's beam has already paid its magic (MM's MAGIC_CONSUME_DEITY_BEAM, in
        // MergeShip_FierceDeity_func_808332A0), and his spin disk ("FD Magic Spin") is free, so neither is charged
        // here -- the charge would take the beam's magic again, or kill it. His spin disk still needs the magic meter,
        // as Link's does: without it his spin is a plain one.
        s32 noDisk;

        if (player->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) {
            noDisk = !gSaveContext.isMagicAcquired || (gSaveContext.magicState != MAGIC_STATE_IDLE) ||
                     (((this->actor.params & 0xFF00) >> 8) &&
                      !(Magic_RequestChange(play, (this->actor.params & 0xFF00) >> 8, MAGIC_CONSUME_NOW)));
        } else {
            noDisk = !gSaveContext.isMagicAcquired && (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H);
        }
        if (noDisk) {
            Audio_PlaySfxGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Audio_PlaySfxGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Actor_Kill(&this->actor);
            return;
        }

        player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
        this->isUsingMagic = 1;
        // MergeShip2 (soh_fd): a spin swing is Ocarina of Time's spin; the Fierce Deity's regular slash is his flying
        // sword beam. Anyone else only reaches here from a spin.
        if ((player->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) ||
            (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H)) {
            this->collider.info.toucher.dmgFlags = sSpinAttackDmgFlags[this->swordType];
            this->attackStrength = 1;
            this->targetScale = ((this->swordType == 1) ? 2 : 4);
            EnMThunder_SetupAction(this, EnMThunder_SpinAttacking);
            this->followPlayerTimer = 8;
        } else {
            this->subtype += ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT;
            EnMThunder_SetupAction(this, EnMThunder_SwordBeamAttack);
            this->followPlayerTimer = 1;
            this->targetScale = 12;
            // "FD Beams Light Fire": MM's beam lights nothing (it carries only DMG_SWORD_BEAM); the option adds the
            // fire arrow's bit, so torches and webs catch. Damage is read at the highest bit, so the beam then also
            // hits as a fire arrow.
            this->collider.info.toucher.dmgFlags =
                MergeShip_SohFd_FdBeamsLightFire() ? (DMG_SWORDBEAM | DMG_ARROW_FIRE) : DMG_SWORDBEAM;
            this->collider.info.toucher.damage = 3;
        }
        Audio_PlaySfxGeneral(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        this->spinAttackTimer = 1.0f;
    } else {
        EnMThunder_SetupAction(this, EnMThunder_ChargingSpinAttack);
    }
    this->actor.child = NULL;
}

// MergeShip2 (soh_fd): the Fierce Deity's flying sword beam (not his spin). The bosses whose Majora's Mask
// counterparts react to MM's DMG_SWORD_BEAM test for the beam actor with this instead, as the beam cannot carry
// that flag (see DMG_SWORDBEAM).
s32 EnMThunder_IsFdSwordBeam(Actor* actor) {
    // MergeShip2: Ganondorf mode -- his magic (ovl_MS_Ganon_Magic) is taken where the beam is: Queen Gohma's eye,
    // Barinade, Volvagia's head, Phantom Ganon, Koume and Kotake, King Dodongo's mouth, boulders, Skulltulas
    if (MsGanonMagic_IsMagic(actor)) {
        return true;
    }
    return (actor != NULL) && (actor->id == ACTOR_EN_M_THUNDER) && (actor->update != NULL) &&
           (((EnMThunder*)actor)->subtype >= ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT);
}

void EnMThunder_Destroy(Actor* thisx, PlayState* play) {
    EnMThunder* this = (EnMThunder*)thisx;

    if (this->isUsingMagic != 0) {
        Magic_Reset(play);
    }

    Collider_DestroyCylinder(play, &this->collider);
    EnMThunder_AdjustEnvLights(play, 0.0f);
    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
}

void EnMThunder_AdjustEnvLights(PlayState* play, f32 intensity) {
    Environment_AdjustLights(play, intensity, 850.0f, 0.2f, 0.0f);
}

void EnMThunder_EmptySpinAttack(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if (player->meleeWeaponAnimation >= 0x18) {
            Audio_PlaySfxGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Audio_PlaySfxGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }

        Actor_Kill(&this->actor);
        return;
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        Actor_Kill(&this->actor);
    }
}

void EnMThunder_ChargingSpinAttack(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* child = this->actor.child;

    this->spinChargePercent = player->unk_858;
    this->actor.world.pos = player->bodyPartsPos[0];
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;

    if (this->isUsingMagic == 0) {
        if (player->unk_858 >= 0.1f) {
            if ((gSaveContext.magicState != MAGIC_STATE_IDLE) ||
                (((this->actor.params & 0xFF00) >> 8) &&
                 !(Magic_RequestChange(play, (this->actor.params & 0xFF00) >> 8, MAGIC_CONSUME_WAIT_PREVIEW)))) {
                EnMThunder_EmptySpinAttack(this, play);
                EnMThunder_SetupAction(this, EnMThunder_EmptySpinAttack);
                this->chargeAlpha = 0;
                this->dimmingIntensity = 0.0;
                this->spinAttackTimer = 0.0f;
                return;
            }

            this->isUsingMagic = 1;
        }
    }

    if (player->unk_858 >= 0.1f) {
        Rumble_Request(0.0f, (s32)(player->unk_858 * 150.0f) & 0xFF, 2, (s32)(player->unk_858 * 150.0f) & 0xFF);
    }

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if ((child != NULL) && (child->update != NULL)) {
            child->parent = NULL;
        }

        if (player->unk_858 <= 0.15f) {
            if ((player->unk_858 >= 0.1f) && (player->meleeWeaponAnimation >= 0x18)) {
                Audio_PlaySfxGeneral(NA_SE_IT_ROLLING_CUT, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                Audio_PlaySfxGeneral(NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos, 4,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
            Actor_Kill(&this->actor);
            return;
        } else {
            player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
            if ((this->actor.params & 0xFF00) >> 8) {
                gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
            }
            if (player->unk_858 < 0.85f) {
                this->collider.info.toucher.dmgFlags = sSpinAttackDmgFlags[this->swordType];
                this->attackStrength = 1;
                this->targetScale = ((this->swordType == 1) ? 2 : 4);
            } else {
                this->collider.info.toucher.dmgFlags = sJumpAttackDmgFlags[this->swordType];
                this->attackStrength = 0;
                this->targetScale = ((this->swordType == 1) ? 4 : 8);
            }

            EnMThunder_SetupAction(this, EnMThunder_SpinAttacking);
            this->followPlayerTimer = 8;
            Audio_PlaySfxGeneral(sSfxIds[this->attackStrength], &player->actor.projectedPos, 4,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->spinAttackTimer = 1.0f;
            return;
        }
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        if (this->actor.child != NULL) {
            this->actor.child->parent = NULL;
        }
        Actor_Kill(&this->actor);
        return;
    }

    if (player->unk_858 > 0.15f) {
        this->chargeAlpha = 255;
        if (this->actor.child == NULL) {
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EFF_DUST, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0,
                               this->swordType + 2);
        }
        this->dimmingIntensity += ((((player->unk_858 - 0.15f) * 1.5f) - this->dimmingIntensity) * 0.5f);

    } else if (player->unk_858 > .1f) {
        this->chargeAlpha = (s32)((player->unk_858 - .1f) * 255.0f * 20.0f);
        this->spinAttackTimer = (player->unk_858 - .1f) * 10.0f;
    } else {
        this->chargeAlpha = 0;
    }

    if (player->unk_858 > 0.85f) {
        func_800F4254(&player->actor.projectedPos, 2);
    } else if (player->unk_858 > 0.15f) {
        func_800F4254(&player->actor.projectedPos, 1);
    } else if (player->unk_858 > 0.1f) {
        func_800F4254(&player->actor.projectedPos, 0);
    }

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

void EnMThunder_UpdateSpinAttack(EnMThunder* this, PlayState* play) {
    if (this->followPlayerTimer < 2) {
        if (this->chargeAlpha < 40) {
            this->chargeAlpha = 0;
        } else {
            this->chargeAlpha -= 40;
        }
    }

    this->spinTrailTexScroll += 2.0f * this->spinAttackAlpha;

    if (this->dimmingIntensity < this->spinAttackTimer) {
        this->dimmingIntensity += ((this->spinAttackTimer - this->dimmingIntensity) * 0.1f);
    } else {
        this->dimmingIntensity = this->spinAttackTimer;
    }
}

void EnMThunder_SpinAttacking(EnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Math_StepToF(&this->spinAttackTimer, 0.0f, 1 / 16.0f)) {
        Actor_Kill(&this->actor);
    } else {
        Math_SmoothStepToF(&this->actor.scale.x, (s32)this->targetScale, 0.6f, 0.8f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);
        this->collider.dim.radius = (this->actor.scale.x * 25.0f);
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->followPlayerTimer > 0) {
        this->actor.world.pos.x = player->bodyPartsPos[0].x;
        this->actor.world.pos.z = player->bodyPartsPos[0].z;
        this->followPlayerTimer--;
    }

    if (this->spinAttackTimer > 0.6f) {
        this->spinAttackAlpha = 1.0f;
    } else {
        this->spinAttackAlpha = this->spinAttackTimer * (5.0f / 3.0f);
    }

    EnMThunder_UpdateSpinAttack(this, play);

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

// MergeShip2 (soh_fd): the Fierce Deity's flying sword beam (MM EnMThunder_SwordBeam_Attack). It flies forward along
// its facing and pitch, grows, keeps its attack at its tip, and fades out over 20 frames.
void EnMThunder_SwordBeamAttack(EnMThunder* this, PlayState* play) {
    f32 forwardStep;

    this->actor.shape.rot.x = -this->actor.world.rot.x;

    if (this->spinAttackTimer > (9.0f / 10.0f)) {
        this->spinAttackAlpha = 1.0f;
    } else {
        this->spinAttackAlpha = this->spinAttackTimer * (10.0f / 9.0f);
    }

    if (Math_StepToF(&this->spinAttackTimer, 0.0f, 0.05f)) {
        Actor_Kill(&this->actor);
    } else {
        forwardStep = -80.0f * Math_CosS(this->actor.world.rot.x);

        this->actor.world.pos.x += forwardStep * Math_SinS(this->actor.shape.rot.y);
        this->actor.world.pos.z += forwardStep * Math_CosS(this->actor.shape.rot.y);
        this->actor.world.pos.y += -80.0f * Math_SinS(this->actor.world.rot.x);

        Math_SmoothStepToF(&this->actor.scale.x, this->targetScale, 0.6f, 2.0f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);

        this->collider.dim.radius = this->actor.scale.x * 5.0f;
        this->collider.dim.pos.x =
            (Math_SinS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.x) + this->actor.world.pos.x;
        this->collider.dim.pos.y = this->actor.world.pos.y;
        this->collider.dim.pos.z =
            (Math_CosS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.z) + this->actor.world.pos.z;

        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->followPlayerTimer > 0) {
        this->followPlayerTimer--;
    }

    EnMThunder_UpdateSpinAttack(this, play);
}

void EnMThunder_Update(Actor* thisx, PlayState* play) {
    EnMThunder* this = (EnMThunder*)thisx;
    f32 blueRadius;
    s32 redGreen;

    this->actionFunc(this, play);
    // MergeShip2 (soh_fd): the scene dims for a spin, not for every swing of the Fierce Deity's beam.
    EnMThunder_AdjustEnvLights(play,
                               (this->subtype >= ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT) ? 0.0f : this->dimmingIntensity);
    blueRadius = this->spinAttackTimer;
    redGreen = (u32)(blueRadius * 255.0f) & 0xFF;
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, redGreen, redGreen, (u32)(blueRadius * 100.0f),
                              (s32)(blueRadius * 800.0f));
}

// MergeShip2 (soh_fd): the Fierce Deity's beam is a blade (MM EnMThunder_Draw's sword beam), drawn instead of the
// spin's trail and charge glow; it has no charge.
static void EnMThunder_DrawSwordBeam(EnMThunder* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0,
                                0x1FF - ((u16)(s32)(this->spinTrailTexScroll * 10.0f) & 0x1FF), 32, 128));
    if (this->subtype == ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 255, (u16)(this->spinAttackAlpha * 255.0f));
        gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 128);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u16)(this->spinAttackAlpha * 255.0f));
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    }
    gSPDisplayList(POLY_XLU_DISP++, gUnusedBeamBladeDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnMThunder_Draw(Actor* thisx, PlayState* play2) {
    static f32 sSpinChargeScale[] = { 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.25f, 0.2f, 0.15f };
    PlayState* play = play2;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);
    f32 phi_f14;
    s32 phi_t1;

    if (this->subtype >= ENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT) { // MergeShip2 (soh_fd)
        EnMThunder_DrawSwordBeam(this, play);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    switch (this->attackStrength) {
        case 0:
        case 1:
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(
                           play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 30) & 0xFF), 0, 0x40,
                           0x20, 1, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0));
            break;
    }

    switch (this->attackStrength) {
        case 0:
            if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Primary.Changed"), 0)) {
                Color_RGB8 color =
                    CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Primary.Value"), (Color_RGB8){ 255, 255, 170 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack3DL);
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack4DL);
            break;
        case 1:
            if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Primary.Changed"), 0)) {
                Color_RGB8 color =
                    CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Primary.Value"), (Color_RGB8){ 170, 255, 255 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack1DL);
            gSPDisplayList(POLY_XLU_DISP++, gSpinAttack2DL);
            break;
    }

    Matrix_Mult(&player->mf_9E0, MTXMODE_NEW);

    switch (this->swordType) {
        case 1:
            Matrix_Translate(0.0f, 220.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(-0.7f, -0.6f, -0.4f, MTXMODE_APPLY);
            Matrix_RotateX(16384.0f, MTXMODE_APPLY);
            break;
        case 0:
            Matrix_Translate(0.0f, 300.0f, -100.0f, MTXMODE_APPLY);
            Matrix_Scale(-1.2f, -1.0f, -0.7f, MTXMODE_APPLY);
            Matrix_RotateX(16384.0f, MTXMODE_APPLY);
            break;
        case 2:
            Matrix_Translate(200.0f, 350.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(-1.8f, -1.4f, -0.7f, MTXMODE_APPLY);
            Matrix_RotateX(16384.0f, MTXMODE_APPLY);
            break;
    }

    if (this->spinChargePercent >= 0.85f) {
        phi_f14 = (sSpinChargeScale[(play->gameplayFrames & 7)] * 6.0f) + 1.0f;
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Primary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Primary.Value"), (Color_RGB8){ 255, 255, 170 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, this->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, this->chargeAlpha);
        }
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level2Secondary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level2Secondary.Value"), (Color_RGB8){ 255, 100, 0 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
        }
        phi_t1 = 0x28;
    } else {
        phi_f14 = (sSpinChargeScale[play->gameplayFrames & 7] * 2.0f) + 1.0f;
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Primary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Primary.Value"), (Color_RGB8){ 170, 255, 255 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, this->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, this->chargeAlpha);
        }
        if (CVarGetInteger(CVAR_COSMETIC("SpinAttack.Level1Secondary.Changed"), 0)) {
            Color_RGB8 color =
                CVarGetColor24(CVAR_COSMETIC("SpinAttack.Level1Secondary.Value"), (Color_RGB8){ 0, 100, 255 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        }
        phi_t1 = 0x14;
    }
    Matrix_Scale(1.0f, phi_f14, phi_f14, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->gameplayFrames * 5) & 0xFF, 0, 0x20, 0x20, 1,
                                  (play->gameplayFrames * 20) & 0xFF, (play->gameplayFrames * phi_t1) & 0xFF, 8, 8, 5,
                                  0, 20, phi_t1));

    gSPDisplayList(POLY_XLU_DISP++, gSpinAttackChargingDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
