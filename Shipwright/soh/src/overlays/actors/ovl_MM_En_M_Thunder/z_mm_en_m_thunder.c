/*
 * File: z_mm_en_m_thunder.c
 * Overlay: ovl_MM_En_M_Thunder
 * Description: Majora's Mask's spin attack wave and sword beams (MM ovl_En_M_Thunder), for the Majora's Mask
 *              forms in Ocarina of Time. Ported from 2ship2harkinian/mm/src/overlays/actors/ovl_En_M_Thunder.
 *
 * Name mapping from MM:
 *   player->unk_B08                  -> player->unk_858 (the spin-attack charge, same meaning in both players)
 *   PLAYER_STATE2_20000              -> PLAYER_STATE2_SPIN_ATTACKING (same bit)
 *   player->leftHandMf               -> player->mf_9E0 (Ocarina of Time's left-hand matrix)
 *   Magic_Consume(..., MAGIC_CONSUME_NOW / WAIT_PREVIEW) -> Magic_RequestChange (same semantics)
 *   WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK -> magic acquired: Hyrule has no separate Great Spin Attack
 *     reward, its great spin comes with the magic meter, which the paths below already require.
 *   Damage flags: MM's DMG_SPIN_ATTACK / DMG_SWORD_BEAM do not exist in Ocarina of Time, whose enemies read
 *     damage from their tables by flag. Each MM sword type is given Ocarina of Time's flag for the same
 *     sword class (regular wave: DMG_SPIN_*, great wave: DMG_JUMP_* as Ocarina of Time's own En_M_Thunder
 *     does; beam: DMG_SPIN_GIANT, the Great Fairy's / Fierce Deity class wave). MM's damage value is kept
 *     in the toucher for colliders without a damage table.
 *   Cosmetic editor overrides (2S2H) are not ported: MM's default colours are used.
 */

#include "z_mm_en_m_thunder.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipTransformFx.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)

#define MMENMTHUNDER_TYPE_MAX 4

#define MM_SFX_FLAG 0x800
#define MM_F32_LERPIMP(v0, v1, t) ((v0) + (((v1) - (v0)) * (t)))

s16 gMergeShipActorId_En_M_Thunder = -1;

void MMEnMThunder_UnkType_Update(Actor* thisx, PlayState* play);
void MMEnMThunder_AdjustLights(PlayState* play, f32 arg1);
void MMEnMThunder_Charge(MMEnMThunder* this, PlayState* play);
void MMEnMThunder_Spin_Attack(MMEnMThunder* this, PlayState* play);
void MMEnMThunder_SwordBeam_Attack(MMEnMThunder* this, PlayState* play);
void MMEnMThunder_UnkType_Attack(MMEnMThunder* this, PlayState* play);

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
        { DMG_SPIN_MASTER, 0x00, 0x00 }, // MM 0x01000000 (MM DMG_SPIN_ATTACK), replaced in Init
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 200, 200, 0, { 0, 0, 0 } },
};

static u8 sDamages[] = {
    1, 2, 3, 4, // Regular
    1, 2, 3, 4, // Great Spin
};

// Ocarina of Time damage flags for MM's per-sword-type waves (see the header comment).
// MergeShip2: the Kokiri Sword's wave hits with the Master Sword's flags too: its damage is MM's, the enemy's entry
// for the Master Sword its multiplier (MergeShip_MMEquip_AdjustDamage).
static u32 sRegularSpinDmgFlags[MMENMTHUNDER_TYPE_MAX] = {
    DMG_SPIN_MASTER, // MMENMTHUNDER_TYPE_KOKIRI_SWORD
    DMG_SPIN_MASTER, // MMENMTHUNDER_TYPE_RAZOR_SWORD
    DMG_SPIN_MASTER, // MMENMTHUNDER_TYPE_GILDED_SWORD
    DMG_SPIN_GIANT,  // MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD
};
static u32 sGreatSpinDmgFlags[MMENMTHUNDER_TYPE_MAX] = {
    DMG_JUMP_MASTER, // MMENMTHUNDER_TYPE_KOKIRI_SWORD
    DMG_JUMP_MASTER, // MMENMTHUNDER_TYPE_RAZOR_SWORD
    DMG_JUMP_MASTER, // MMENMTHUNDER_TYPE_GILDED_SWORD
    DMG_JUMP_GIANT,  // MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD
};
// MM DMG_SWORD_BEAM. The Fierce Deity's beam is the strongest attack there is, so it carries every flag an
// Ocarina of Time weak point is opened by -- the slingshot (Gohma's eye), the arrows, light -- and, as its sword
// class, only the Biggoron jump bit: CollisionCheck_GetSwordDamage takes the weakest sword tier present, and
// damage tables read the highest bit. Hits are further boosted in CollisionCheck_ApplyDamage.
#define MMENMTHUNDER_SWORD_BEAM_DMG_FLAGS     (DMG_JUMP_GIANT | DMG_SLINGSHOT | DMG_ARROW_NORMAL | DMG_ARROW_FIRE | DMG_ARROW_LIGHT | DMG_MAGIC_LIGHT)

typedef enum {
    /* 0 */ MMENMTHUNDER_SUBTYPE_SPIN_GREAT,
    /* 1 */ MMENMTHUNDER_SUBTYPE_SPIN_REGULAR,
    /* 2 */ MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT,
    /* 3 */ MMENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR,
    /* 4 */ MMENMTHUNDER_SUBTYPE_MAX
} MMEnMThunderSubType;

static u16 sChargingSfxIds[MMENMTHUNDER_SUBTYPE_MAX] = {
    MM_NA_SE_IT_ROLLING_CUT_LV2, // MMENMTHUNDER_SUBTYPE_SPIN_GREAT
    MM_NA_SE_IT_ROLLING_CUT_LV1, // MMENMTHUNDER_SUBTYPE_SPIN_REGULAR
    MM_NA_SE_IT_ROLLING_CUT_LV2, // MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT
    MM_NA_SE_IT_ROLLING_CUT_LV1, // MMENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR
};

static f32 sScales[] = { 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.25f, 0.2f, 0.15f, 0.0f };

static u32 MMEnMThunder_TypeIndex(MMEnMThunder* this) {
    return (this->type < MMENMTHUNDER_TYPE_MAX) ? this->type : MMENMTHUNDER_TYPE_KOKIRI_SWORD;
}

// MM Audio_PlaySfx_SwordCharge (code_8019AF00.c). The MergeShip audio API has no per-call frequency
// scale, so MM's rising pitch per charge level (sChargeLevelsSfxFreq) is not reproduced; the sounds and
// their timing are MM's.
static u8 sPrevChargeLevel = 0;
static void MMEnMThunder_PlaySwordChargeSfx(Vec3f* pos, u8 chargeLevel) {
    chargeLevel %= 4U;
    if (chargeLevel != sPrevChargeLevel) {
        switch (chargeLevel) {
            case 1:
            case 2:
                MergeShip_PlayMMSfxAtPos(pos, MM_NA_SE_PL_SWORD_CHARGE);
                break;

            default:
                break;
        }
        sPrevChargeLevel = chargeLevel;
    }

    if (chargeLevel != 0) {
        MergeShip_PlayMMSfxAtPos(pos, MM_NA_SE_IT_SWORD_CHARGE - MM_SFX_FLAG);
    }
}

// MM EnMThunder_UnkType_Setup
void MMEnMThunder_UnkType_Setup(MMEnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.update = MMEnMThunder_UnkType_Update;
    this->isCharging = false;
    this->subtype = MMENMTHUNDER_SUBTYPE_SPIN_REGULAR;
    this->scaleTarget = 2;
    this->actionFunc = MMEnMThunder_UnkType_Attack;
    this->timer = 8;
    this->lightColorFrac = 1.0f;
    MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_ROLLING_CUT_LV1);
    this->actor.child = NULL;
}

// MM EnMThunder_Init
void MMEnMThunder_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    MMEnMThunder* this = (MMEnMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->type = MMENMTHUNDER_GET_TYPE(&this->actor);
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, 255, 255, 255, 0);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);

    if (this->type == MMENMTHUNDER_TYPE_UNK) {
        MMEnMThunder_UnkType_Setup(this, play);
        return;
    }

    this->collider.dim.radius = 0;
    this->collider.dim.height = 40;
    this->collider.dim.yShift = -20;
    this->timer = 8;
    this->scroll = 0.0f;
    this->actor.world.pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    this->lightColorFrac = 0.0f;
    this->adjustLightsArg1 = 0.0f;
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.shape.rot.x = -this->actor.world.rot.x;
    this->actor.room = -1;
    Actor_SetScale(&this->actor, 0.1f);
    this->isCharging = false;

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if (!gSaveContext.isMagicAcquired || (gSaveContext.magicState != MAGIC_STATE_IDLE) ||
            ((MMENMTHUNDER_GET_MAGIC_COST(&this->actor) != 0) &&
             !Magic_RequestChange(play, MMENMTHUNDER_GET_MAGIC_COST(&this->actor), MAGIC_CONSUME_NOW))) {
            MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_ROLLING_CUT);
            MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_SWORD_SWING_HARD);
            Actor_Kill(&this->actor);
            return;
        }

        player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
        this->isCharging = false;

        // MM: CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK); see the header comment.
        if (gSaveContext.isMagicAcquired) {
            player->unk_858 = 1.0f;
            this->collider.info.toucher.damage = sDamages[MMEnMThunder_TypeIndex(this) + MMENMTHUNDER_TYPE_MAX];
            this->collider.info.toucher.dmgFlags = sGreatSpinDmgFlags[MMEnMThunder_TypeIndex(this)];
            this->subtype = MMENMTHUNDER_SUBTYPE_SPIN_GREAT;
            if (this->type == MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 6;
            } else if (this->type == MMENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 4;
            } else {
                this->scaleTarget = 3;
            }
        } else {
            player->unk_858 = 0.5f;
            this->collider.info.toucher.damage = sDamages[MMEnMThunder_TypeIndex(this)];
            this->collider.info.toucher.dmgFlags = sRegularSpinDmgFlags[MMEnMThunder_TypeIndex(this)];
            this->subtype = MMENMTHUNDER_SUBTYPE_SPIN_REGULAR;
            if (this->type == MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 4;
            } else if (this->type == MMENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 3;
            } else {
                this->scaleTarget = 2;
            }
        }

        if (player->meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H) {
            this->subtype += MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT;
            this->actionFunc = MMEnMThunder_SwordBeam_Attack;
            this->timer = 1;
            this->scaleTarget = 12;
            this->collider.info.toucher.dmgFlags = MMENMTHUNDER_SWORD_BEAM_DMG_FLAGS;
            this->collider.info.toucher.damage = 8;
        } else {
            this->actionFunc = MMEnMThunder_Spin_Attack;
            this->timer = 8;
        }

        MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_ROLLING_CUT_LV1);

        this->lightColorFrac = 1.0f;
    } else {
        this->actionFunc = MMEnMThunder_Charge;
    }

    this->actor.child = NULL;
}

// MM EnMThunder_Destroy
void MMEnMThunder_Destroy(Actor* thisx, PlayState* play) {
    MMEnMThunder* this = (MMEnMThunder*)thisx;

    if (this->isCharging) {
        Magic_Reset(play);
    }

    Collider_DestroyCylinder(play, &this->collider);
    MMEnMThunder_AdjustLights(play, 0.0f);
    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
}

// MM EnMThunder_AdjustLights
void MMEnMThunder_AdjustLights(PlayState* play, f32 arg1) {
    Environment_AdjustLights(play, arg1, 850.0f, 0.2f, 0.0f);
}

// MM EnMThunder_Spin_AttackNoMagic
void MMEnMThunder_Spin_AttackNoMagic(MMEnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H) {
            MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_ROLLING_CUT);
            MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_SWORD_SWING_HARD);
        }
        Actor_Kill(&this->actor);
        return;
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        Actor_Kill(&this->actor);
    }
}

// MM EnMThunder_Charge
void MMEnMThunder_Charge(MMEnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* child = this->actor.child;

    this->unk1B0 = player->unk_858;
    this->actor.world.pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;

    if (!this->isCharging && (player->unk_858 >= 0.1f)) {
        if ((gSaveContext.magicState != MAGIC_STATE_IDLE) ||
            ((MMENMTHUNDER_GET_MAGIC_COST(&this->actor) != 0) &&
             !Magic_RequestChange(play, MMENMTHUNDER_GET_MAGIC_COST(&this->actor), MAGIC_CONSUME_WAIT_PREVIEW))) {
            MMEnMThunder_Spin_AttackNoMagic(this, play);
            this->actionFunc = MMEnMThunder_Spin_AttackNoMagic;
            this->chargingAlpha = 0;
            this->adjustLightsArg1 = 0.0f;
            this->lightColorFrac = 0.0f;
            return;
        }
        this->isCharging = true;
    }

    if (player->unk_858 >= 0.1f) {
        Rumble_Request(0.0f, (s32)(player->unk_858 * 150.0f), 2, (s32)(player->unk_858 * 150.0f));
    }

    if (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING) {
        if ((child != NULL) && (child->update != NULL)) {
            child->parent = NULL;
        }

        if (player->unk_858 <= 0.15f) {
            if ((player->unk_858 >= 0.1f) && (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H)) {
                MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_ROLLING_CUT);
                MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, MM_NA_SE_IT_SWORD_SWING_HARD);
            }
            Actor_Kill(&this->actor);
            return;
        }

        player->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;

        if (MMENMTHUNDER_GET_MAGIC_COST(&this->actor) != 0) {
            gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
        }

        if (player->unk_858 < 0.85f) {
            this->collider.info.toucher.damage = sDamages[MMEnMThunder_TypeIndex(this)];
            this->collider.info.toucher.dmgFlags = sRegularSpinDmgFlags[MMEnMThunder_TypeIndex(this)];
            this->subtype = MMENMTHUNDER_SUBTYPE_SPIN_REGULAR;
            if (this->type == MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 4;
            } else if (this->type == MMENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 3;
            } else {
                this->scaleTarget = 2;
            }
        } else {
            this->collider.info.toucher.damage = sDamages[MMEnMThunder_TypeIndex(this) + MMENMTHUNDER_TYPE_MAX];
            this->collider.info.toucher.dmgFlags = sGreatSpinDmgFlags[MMEnMThunder_TypeIndex(this)];
            this->subtype = MMENMTHUNDER_SUBTYPE_SPIN_GREAT;
            if (this->type == MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) {
                this->scaleTarget = 6;
            } else if (this->type == MMENMTHUNDER_TYPE_GILDED_SWORD) {
                this->scaleTarget = 4;
            } else {
                this->scaleTarget = 3;
            }
        }

        if (player->meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H) {
            this->subtype += MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT;
            this->actionFunc = MMEnMThunder_SwordBeam_Attack;
            this->timer = 1;
        } else {
            this->actionFunc = MMEnMThunder_Spin_Attack;
            this->timer = 8;
        }

        MergeShip_PlayMMSfxAtPos(&player->actor.projectedPos, sChargingSfxIds[this->subtype]);

        this->lightColorFrac = 1.0f;

        return;
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        if (this->actor.child != NULL) {
            this->actor.child->parent = NULL;
        }
        Actor_Kill(&this->actor);
        return;
    }

    if (player->unk_858 > 0.15f) {
        this->chargingAlpha = 255;
        if (this->actor.child == NULL) {
            // MM EFF_DUST_TYPE_SPIN_ATTACK_CHARGE is 2 in both games' Eff_Dust.
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EFF_DUST, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0, 2);
        }
        this->adjustLightsArg1 += (((player->unk_858 - 0.15f) * 1.5f) - this->adjustLightsArg1) * 0.5f;
    } else if (player->unk_858 > .1f) {
        this->chargingAlpha = (s32)((player->unk_858 - .1f) * 255.0f * 20.0f);
        this->lightColorFrac = (player->unk_858 - .1f) * 10.0f;
    } else {
        this->chargingAlpha = 0;
    }

    if (player->unk_858 > 0.85f) {
        MMEnMThunder_PlaySwordChargeSfx(&player->actor.projectedPos, 2);
    } else if (player->unk_858 > 0.15f) {
        MMEnMThunder_PlaySwordChargeSfx(&player->actor.projectedPos, 1);
    } else if (player->unk_858 > 0.1f) {
        MMEnMThunder_PlaySwordChargeSfx(&player->actor.projectedPos, 0);
    }

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

// MM func_808B5EEC
void MMEnMThunder_func_808B5EEC(MMEnMThunder* this, PlayState* play) {
    if (this->timer < 2) {
        if (this->chargingAlpha < 40) {
            this->chargingAlpha = 0;
        } else {
            this->chargingAlpha -= 40;
        }
    }

    this->scroll += 2.0f * this->alphaFrac;

    if (this->adjustLightsArg1 < this->lightColorFrac) {
        this->adjustLightsArg1 = MM_F32_LERPIMP(this->adjustLightsArg1, this->lightColorFrac, 0.1f);
    } else {
        this->adjustLightsArg1 = this->lightColorFrac;
    }
}

// MM EnMThunder_Spin_Attack
void MMEnMThunder_Spin_Attack(MMEnMThunder* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Math_StepToF(&this->lightColorFrac, 0.0f, 0.0625f)) {
        Actor_Kill(&this->actor);
    } else {
        Math_SmoothStepToF(&this->actor.scale.x, (s32)this->scaleTarget, 0.6f, 0.8f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);
        this->collider.dim.radius = this->actor.scale.x * 30.0f;
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->timer > 0) {
        this->actor.world.pos.x = player->bodyPartsPos[PLAYER_BODYPART_WAIST].x;
        this->actor.world.pos.z = player->bodyPartsPos[PLAYER_BODYPART_WAIST].z;
        this->timer--;
    }

    if (this->lightColorFrac > (6.0f / 10.0f)) {
        this->alphaFrac = 1.0f;
    } else {
        this->alphaFrac = this->lightColorFrac * (10.0f / 6.0f);
    }

    MMEnMThunder_func_808B5EEC(this, play);

    if (Play_InCsMode(play)) {
        Actor_Kill(&this->actor);
    }
}

// MM EnMThunder_SwordBeam_Attack
// The Fierce Deity's sword beam, for CollisionCheck_ApplyDamage's boost.
s32 MMEnMThunder_IsSwordBeam(Actor* actor) {
    return (actor != NULL) && (gMergeShipActorId_En_M_Thunder >= 0) && (actor->id == gMergeShipActorId_En_M_Thunder) &&
           (((MMEnMThunder*)actor)->actionFunc == MMEnMThunder_SwordBeam_Attack);
}

void MMEnMThunder_SwordBeam_Attack(MMEnMThunder* this, PlayState* play) {
    s32 pad[2];
    f32 sp2C;

    if (this->lightColorFrac > (9.0f / 10.0f)) {
        this->alphaFrac = 1.0f;
    } else {
        this->alphaFrac = this->lightColorFrac * (10.0f / 9.0f);
    }

    if (Math_StepToF(&this->lightColorFrac, 0.0f, 0.05f)) {
        Actor_Kill(&this->actor);
    } else {
        sp2C = -80.0f * Math_CosS(this->actor.world.rot.x);

        this->actor.world.pos.x += sp2C * Math_SinS(this->actor.shape.rot.y);
        this->actor.world.pos.z += sp2C * Math_CosS(this->actor.shape.rot.y);
        this->actor.world.pos.y += -80.0f * Math_SinS(this->actor.world.rot.x);

        Math_SmoothStepToF(&this->actor.scale.x, this->scaleTarget, 0.6f, 2.0f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);

        this->collider.dim.radius = this->actor.scale.x * 5.0f;

        this->collider.dim.pos.x = this->actor.world.pos.x;
        this->collider.dim.pos.y = this->actor.world.pos.y;
        this->collider.dim.pos.z = this->actor.world.pos.z;

        this->collider.dim.pos.x =
            (Math_SinS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.x) + this->actor.world.pos.x;
        this->collider.dim.pos.y = this->actor.world.pos.y;
        this->collider.dim.pos.z =
            (Math_CosS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.z) + this->actor.world.pos.z;

        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }

    if (this->timer > 0) {
        this->timer--;
    }

    MMEnMThunder_func_808B5EEC(this, play);
}

// MM EnMThunder_UnkType_Attack
void MMEnMThunder_UnkType_Attack(MMEnMThunder* this, PlayState* play) {
    if (Math_StepToF(&this->lightColorFrac, 0.0f, 0.0625f)) {
        Actor_Kill(&this->actor);
    } else {
        Math_SmoothStepToF(&this->actor.scale.x, (s32)this->scaleTarget, 0.6f, 0.8f, 0.0f);
        Actor_SetScale(&this->actor, this->actor.scale.x);
    }

    if (this->lightColorFrac > (6.0f / 10.0f)) {
        this->alphaFrac = 1.0f;
    } else {
        this->alphaFrac = this->lightColorFrac * (10.0f / 6.0f);
    }

    MMEnMThunder_func_808B5EEC(this, play);
}

// MM EnMThunder_Update
void MMEnMThunder_Update(Actor* thisx, PlayState* play) {
    MMEnMThunder* this = (MMEnMThunder*)thisx;

    this->actionFunc(this, play);
    MMEnMThunder_AdjustLights(play, this->adjustLightsArg1);
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, this->lightColorFrac * 255.0f, this->lightColorFrac * 255.0f,
                              this->lightColorFrac * 100.0f, this->lightColorFrac * 800.0f);
}

// MM EnMThunder_UnkType_Update
void MMEnMThunder_UnkType_Update(Actor* thisx, PlayState* play) {
    MMEnMThunder* this = (MMEnMThunder*)thisx;

    this->actionFunc(this, play);
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, this->lightColorFrac * 255.0f, this->lightColorFrac * 255.0f,
                              this->lightColorFrac * 100.0f, this->lightColorFrac * 800.0f);
}

// MM EnMThunder_Draw
void MMEnMThunder_Draw(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    MMEnMThunder* this = (MMEnMThunder*)thisx;
    Player* player = GET_PLAYER(play);
    f32 scale;
    s32 y2Scroll;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    switch (this->subtype) {
        case MMENMTHUNDER_SUBTYPE_SPIN_GREAT:
        case MMENMTHUNDER_SUBTYPE_SPIN_REGULAR:
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0xFF - ((u16)(s32)(this->scroll * 30.0f) & 0xFF), 0,
                                          64, 32, 1, 0xFF - ((u16)(s32)(this->scroll * 20.0f) & 0xFF), 0, 8, 8, -30, 0,
                                          -20, 0));
            break;

        case MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT:
        case MMENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR:
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0,
                                          0x1FF - ((u16)(s32)(this->scroll * 10.0f) & 0x1FF), 32, 128, 0, 0, 0, -10));
            break;

        default:
            break;
    }

    switch (this->subtype) {
        case MMENMTHUNDER_SUBTYPE_SPIN_GREAT:
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, (u16)(this->alphaFrac * 255.0f));
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGreatSpinAttackDiskDL);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGreatSpinAttackCylinderDL);
            break;

        case MMENMTHUNDER_SUBTYPE_SPIN_REGULAR:
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u16)(this->alphaFrac * 255.0f));
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gSpinAttackDiskDL);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gSpinAttackCylinderDL);
            break;

        case MMENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR:
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u16)(this->alphaFrac * 255.0f));
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gSwordBeamDL);
            break;

        case MMENMTHUNDER_SUBTYPE_SWORDBEAM_GREAT:
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 0, 255, 255, (u16)(this->alphaFrac * 255.0f));
            gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 128);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gSwordBeamDL);
            break;

        default:
            break;
    }

    Matrix_Mult(&player->mf_9E0, MTXMODE_NEW);

    if (this->type == MMENMTHUNDER_TYPE_GILDED_SWORD) {
        Matrix_Translate(0.0f, 220.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(-1.2f, -0.8f, -0.6f, MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2, MTXMODE_APPLY); // MM Matrix_RotateXS(0x4000)
    } else {
        Matrix_Translate(0.0f, 220.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(-0.7f, -0.6f, -0.4f, MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2, MTXMODE_APPLY); // MM Matrix_RotateXS(0x4000)
    }

    if (this->unk1B0 >= 0.85f) {
        scale = (sScales[play->gameplayFrames & 7] * 6.0f) + 1.0f;
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, this->chargingAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
        y2Scroll = 40;
    } else {
        scale = (sScales[play->gameplayFrames & 7] * 2.0f) + 1.0f;
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, this->chargingAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        y2Scroll = 20;
    }

    Matrix_Scale(1.0f, scale, scale, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->gameplayFrames * 5) & 0xFF, 0, 32, 32, 1,
                                  (play->gameplayFrames * 20) & 0xFF, (play->gameplayFrames * y2Scroll) & 0xFF, 8, 8, 5,
                                  0, 20, y2Scroll));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gSpinAttackChargingDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
