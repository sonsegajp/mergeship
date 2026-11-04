// MergeShip2 -- Majora's Mask Deku form, ported from 2ship2harkinian/mm into Ocarina of Time's player.
// Included at the end of z_player.c. See docs/PORTING_GUIDE.md.
//
// Every function names the MM function it ports (2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/
// z_player.c unless stated). Field mapping beyond the guide's table, used throughout:
//   MM unk_ADC -> OoT unk_844 (combo/spin timer, decremented by OoT's Player_UpdateCommon)
//   MM unk_B28 -> OoT unk_860 (held item's shot state), MM unk_ACC -> OoT unk_834, MM unk_ACE -> OoT unk_836
//   MM unk_D57 -> OoT unk_A73 ("just fired" timer read by the arrow actor)
//   MM unk_B3C -> OoT unk_86C, MM unk_AA5 -> OoT unk_6AD with MM PLAYER_UNKAA5_3 (aiming) = OoT 2
//   MM PLAYER_STATE3_40 (aiming a projectile) -> OoT PLAYER_STATE1_READY_TO_FIRE; OoT gives stateFlags3 bit 6 to
//   Nayru's Love.
//   MM PLAYER_STATE2_20 / 40 -> OoT PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET / _ALWAYS.
// MM-only fields MM keeps on its Player (unk_ABC, unk_B48, unk_B10, unk_B86, unk_B8A.., unk_AF0,
// remainingHopsCounter, zoraBoomerangActor, shieldCylinder) are the ones appended to Ocarina of Time's Player.
//
// Damage: Ocarina of Time's enemies read Ocarina of Time's DMG_ bits, so MM's Deku-only damage types become the
// Ocarina of Time type that the Hyrule damage tables treat like MM's tables treat the Deku's:
//   MM DMG_DEKU_SPIN   (1 damage, "cut" on Deku Babas)   -> DMG_SLASH_KOKIRI (1 damage, cuts)
//   MM DMG_DEKU_LAUNCH (2 damage)                         -> DMG_DEKU_STICK   (2 damage)
//   MM guard shield 0x100000 (DMG_NORMAL_SHIELD)          -> DMG_SHIELD (the same bit)

#include "mm/objects/object_link_nuts/object_link_nuts.h"
#include "src/overlays/actors/ovl_MM_En_Arrow/z_mm_en_arrow.h"
#include "src/overlays/actors/ovl_MM_Obj_Etcetera/z_mm_obj_etcetera.h"

// MergeShip2: frame interpolation pairs a frame's matrix operations with the next frame's in order, so a block that is
// drawn on some frames only (a mask on the head, a sword fitted to a hand, a form's extra part) would shift every
// operation after it and stretch a limb for a frame. Each Matrix_Push .. Matrix_Pop here is its own interpolation
// child (SoH's FrameInterpolation_RecordOpenChild), keyed by where it is.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
#define MS_INTERP_PUSH() (FrameInterpolation_RecordOpenChild(__FILE__, __LINE__), Matrix_Push())
#define MS_INTERP_POP() (Matrix_Pop(), FrameInterpolation_RecordCloseChild())

#define MERGESHIP_DEKU_DMG_SPIN DMG_SLASH_KOKIRI
#define MERGESHIP_DEKU_DMG_LAUNCH DMG_DEKU_STICK

#define DEKU_ANIM(name) ((LinkAnimationHeader*)mm_gPlayerAnim_##name)
#define DEKU_ANIM_IS(anim, name) ((anim) == DEKU_ANIM(name))
#define DEKU_TRUNCF_BINANG(f) ((s16)(s32)(f))

// MM Player_Anim_* helpers not in Ocarina of Time's player (MM PLAYER_ANIM_ADJUSTED_SPEED is 2/3).
static void MergeShip_Deku_AnimPlayOnceFreezeAdjusted(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    LinkAnimation_Change(play, &this->skelAnime, anim, 2.0f / 3.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
}
static void MergeShip_Deku_AnimPlayLoopMorphAdjusted(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    LinkAnimation_Change(play, &this->skelAnime, anim, 2.0f / 3.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -6.0f);
}

// MM Math_AsymStepToS (z_lib.c).
static s32 MergeShip_Deku_AsymStepToS(s16* pValue, s16 target, s16 incrStep, s16 decrStep) {
    s16 step = (target >= *pValue) ? incrStep : decrStep;

    if (step != 0) {
        if (target < *pValue) {
            step = -step;
        }
        *pValue += step;
        if (((*pValue - target) * step) >= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }
    return false;
}

// MM Math_Vec3f_StepToXZ (z_lib.c).
static void MergeShip_Deku_Vec3fStepToXZ(Vec3f* start, Vec3f* target, f32 speed) {
    f32 diffX = target->x - start->x;
    f32 diffZ = target->z - start->z;
    f32 dist = sqrtf(SQ(diffX) + SQ(diffZ));

    if (dist > speed) {
        start->x += (diffX / dist) * speed;
        start->z += (diffZ / dist) * speed;
    } else {
        start->x = target->x;
        start->z = target->z;
    }
}

// MM Lib_Vec3f_TranslateAndRotateY / Player_TranslateAndRotateY.
static void MergeShip_Deku_TranslateAndRotateY(Player* this, Vec3f* translation, Vec3f* src, Vec3f* dst) {
    f32 cos = Math_CosS(this->actor.shape.rot.y);
    f32 sin = Math_SinS(this->actor.shape.rot.y);

    dst->x = translation->x + (src->x * cos + src->z * sin);
    dst->y = translation->y + src->y;
    dst->z = translation->z + (src->z * cos - src->x * sin);
}

// MM uses the D_801BFE14 run speed (R_RUN_SPEED_LIMIT) as unk_B50 when the floor is level.
static f32 MergeShip_Deku_GetRunSpeedLimit(void) {
    return REG(45) / 100.0f;
}

// ---- colliders ------------------------------------------------------------------------------------------------

// MM Player_ResetCylinder. The AC mask is Ocarina of Time's (0xFFCFFFFF, OoT's D_80854624) rather than MM's
// 0xF7CFFFFF, because the bits describe Ocarina of Time's attacks.
static void MergeShip_Deku_ResetCylinder(Player* this) {
    this->cylinder.base.colType = COLTYPE_HIT5;
    this->cylinder.base.atFlags = AT_NONE;
    this->cylinder.base.acFlags = AC_ON | AC_TYPE_ENEMY;
    this->cylinder.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL;
    this->cylinder.info.elemType = ELEMTYPE_UNK1;
    this->cylinder.info.toucher.dmgFlags = 0;
    this->cylinder.info.bumper.dmgFlags = 0xFFCFFFFF;
    this->cylinder.info.toucherFlags = TOUCH_NONE | TOUCH_SFX_NORMAL;
    this->cylinder.dim.radius = 12;
}

// MM Player_SetCylinderForAttack.
static void MergeShip_Deku_SetCylinderForAttack(Player* this, u32 dmgFlags, s32 damage, s32 radius) {
    this->cylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    if (radius > 30) {
        this->cylinder.base.ocFlags1 = OC1_NONE;
    } else {
        this->cylinder.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL;
    }

    this->cylinder.info.elemType = ELEMTYPE_UNK2;
    this->cylinder.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL;
    this->cylinder.dim.radius = radius;
    this->cylinder.info.toucher.dmgFlags = dmgFlags;
    this->cylinder.info.toucher.damage = damage;

    // (MM's DMG_GORON_POUND and DMG_ZORA_BARRIER cases are other forms'.)
    this->cylinder.base.colType = COLTYPE_NONE;
    this->cylinder.info.bumper.dmgFlags = 0xFFCFFFFF;
    this->cylinder.base.acFlags = AC_ON | AC_TYPE_ENEMY;
}

// MM D_8085C318: the player's shield cylinder, which MM's Player_InitCommon sets up and Ocarina of Time's does not.
static ColliderCylinderInit sMergeShipDekuShieldCylinderInit = {
    {
        COLTYPE_METAL,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_HARD | AC_TYPE_ENEMY,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00100000, 0x00, 0x02 },
        { 0xD7CFFFFF, 0x00, 0x00 },
        TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 25, 60, 0, { 0, 0, 0 } },
};

static void MergeShip_Deku_InitShieldCylinder(PlayState* play, Player* this) {
    // A freshly spawned Player is zeroed, so an uninitialised shield cylinder has no actor.
    if (this->shieldCylinder.base.actor != &this->actor) {
        Collider_InitCylinder(play, &this->shieldCylinder);
        Collider_SetCylinder(play, &this->shieldCylinder, &this->actor, &sMergeShipDekuShieldCylinderInit);
    }
}

// ---- small MM helpers ----------------------------------------------------------------------------------------

void MergeShip_Deku_Action_25(Player* this, PlayState* play);
void MergeShip_Deku_Action_93(Player* this, PlayState* play);
void MergeShip_Deku_Action_94(Player* this, PlayState* play);
void MergeShip_Deku_Action_95(Player* this, PlayState* play);
void MergeShip_Deku_Action_1(Player* this, PlayState* play);
void MergeShip_Deku_Action_77(Player* this, PlayState* play);
s32 MergeShip_Deku_UpperAction_7(Player* this, PlayState* play);
s32 MergeShip_Deku_UpperAction_8(Player* this, PlayState* play);
s32 MergeShip_Deku_UpperAction_9(Player* this, PlayState* play);

// MM func_80834CD0 / func_80834D50: jump into Player_Action_25 (Ocarina of Time's Player_Action_8084411C, which
// hands the Deku to MergeShip_Deku_Action_25).
static void MergeShip_Deku_func_80834CD0(Player* this, f32 arg1, u16 sfxId) {
    this->actor.velocity.y = arg1 * sWaterSpeedFactor;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;

    if (sfxId != MM_NA_SE_NONE) {
        MergeShip_PlayMMFloorJumpSfx(this);
        MergeShip_PlayMMVoiceSfx(this, sfxId);
    }

    this->stateFlags1 |= PLAYER_STATE1_JUMPING;
    this->fallStartHeight = this->actor.world.pos.y;
}

static void MergeShip_Deku_func_80834D50(PlayState* play, Player* this, LinkAnimationHeader* anim, f32 speed,
                                         u16 sfxId) {
    Player_SetupAction(play, this, Player_Action_8084411C, 1);
    if (anim != NULL) {
        Player_AnimPlayOnceAdjusted(play, this, anim);
    }
    this->hoverBootsTimer = 0;
    MergeShip_Deku_func_80834CD0(this, speed, sfxId);
}

// MM func_80833AA0: fall, in MM's landing_wait pose.
static void MergeShip_Deku_func_80833AA0(Player* this, PlayState* play) {
    if (Player_SetupAction(play, this, Player_Action_8084411C, 0)) {
        Player_AnimPlayLoop(play, this, DEKU_ANIM(link_normal_landing_wait));
        this->av2.actionVar2 = 1;
    }
    // MM PLAYER_UNKAA5_4 (cutscene) is Ocarina of Time's unk_6AD == 3.
    if (this->unk_6AD != 3) {
        this->unk_6AD = 0;
    }
}

// MM func_80836C70: bark chips from a hand.
static void MergeShip_Deku_func_80836C70(PlayState* play, Player* this, s32 bodyPartIndex) {
    static Vec3f D_8085D130 = { 0, 0, 0 };
    s32 i;

    for (i = 0; i < 4; i++) {
        Vec3f velocity;

        velocity.x = Rand_CenteredFloat(4.0f);
        velocity.y = Rand_ZeroFloat(2.0f);
        velocity.z = Rand_CenteredFloat(4.0f);
        D_8085D130.y = -0.2f;
        // MM passes OBJECT_LINK_NUTS for the chip's segment; the DL is an mm.o2r path, so any loaded object serves.
        EffectSsHahen_Spawn(play, &this->bodyPartsPos[bodyPartIndex], &velocity, &D_8085D130, 0, 10,
                            OBJECT_GAMEPLAY_KEEP, 16, (Gfx*)mm_object_link_nuts_DL_008860);
    }
}

// MM func_80836D8C
static void MergeShip_Deku_func_80836D8C(Player* this) {
    this->actor.focus.rot.x = 0;
    this->actor.focus.rot.z = 0;
    this->headLimbRot.x = 0;
    this->headLimbRot.y = 0;
    this->headLimbRot.z = 0;
    this->upperLimbRot.x = 0;
    this->upperLimbRot.y = 0;
    this->upperLimbRot.z = 0;
    this->actor.shape.rot.y = this->actor.focus.rot.y;
    this->yaw = this->actor.focus.rot.y;
}

// MM func_80836DC0: burrow into the Deku Flower underfoot. (MM also allows it anywhere while its debug register
// MREG(48) is set; that register means something else in Ocarina of Time and is not consulted.)
static s32 MergeShip_Deku_func_80836DC0(PlayState* play, Player* this) {
    if ((this->actor.floorPoly != NULL) &&
        MergeShip_MM_func_800C9DDC(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId)) {
        Player_SetupAction(play, this, MergeShip_Deku_Action_93, 0);
        this->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
        Player_AnimChangeOnceMorph(play, this, DEKU_ANIM(pn_attack));
        Player_ZeroSpeedXZ(this);
        MergeShip_Deku_func_80836D8C(this);
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->unk_B48 = -2000.0f;
        this->actor.shape.shadowScale = 13.0f;
        MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_PL_DEKUNUTS_IN_GRD);
        return true;
    }

    return false;
}

// MM func_80836F10: fall damage on landing.
static s32 MergeShip_Deku_func_80836F10(PlayState* play, Player* this) {
    static struct {
        s8 damage;
        u8 sourceIntensity;
        u8 decayTimer;
        u8 decayStep;
        u16 sfxId;
    } sFallImpactInfos[] = {
        { -8, 180, 40, 100, MM_NA_SE_VO_LI_LAND_DAMAGE_S },
        { -16, 255, 140, 150, MM_NA_SE_VO_LI_LAND_DAMAGE_S },
    };
    s32 fallDistance;

    if ((sFloorType == 6) || (sFloorType == 9) || (this->csAction != 0)) {
        fallDistance = 0;
    } else {
        fallDistance = this->fallDistance;
    }

    Math_StepToF(&this->linearVelocity, 0.0f, 1.0f);
    this->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);

    if (fallDistance >= 400) {
        s32 index = (this->fallDistance < 800) ? 0 : 1;

        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BODY_HIT);
        MergeShip_PlayMMVoiceSfx(this, sFallImpactInfos[index].sfxId);

        if (Player_InflictDamage(play, sFallImpactInfos[index].damage)) {
            return -1;
        }

        Player_SetIntangibility(this, 40);
        Player_RequestQuake(play, 32967, 2, 30);
        Player_RequestRumble(this, sFallImpactInfos[index].sourceIntensity, sFallImpactInfos[index].decayTimer,
                             sFallImpactInfos[index].decayStep, 0);

        return index + 1;
    }

    if (fallDistance > 200) {
        fallDistance *= 2;
        fallDistance = CLAMP_MAX(fallDistance, 255);

        Player_RequestRumble(this, fallDistance, fallDistance * 0.1f, fallDistance, 0);
    }

    MergeShip_PlayMMFloorLandSfx(this);
    return 0;
}

// MM func_80837134: landing, Deku branch (MM's other branches are for the other forms).
void MergeShip_Deku_func_80837134(PlayState* play, Player* this) {
    LinkAnimationHeader* anim = GET_PLAYER_ANIM(PLAYER_ANIMGROUP_landing, this->modelAnimType);
    s32 var_v1 = false;
    s32 temp_v0_2;

    this->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);

    if (DEKU_ANIM_IS(this->skelAnime.animation, pn_rakkafinish) ||
        DEKU_ANIM_IS(this->skelAnime.animation, pn_kakkufinish)) {
        MergeShip_Deku_func_80836C70(play, this, PLAYER_BODYPART_L_HAND);
        MergeShip_Deku_func_80836C70(play, this, PLAYER_BODYPART_R_HAND);
        var_v1 = true;
    }

    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_A) && MergeShip_Deku_func_80836DC0(play, this)) {
        return;
    }

    if (var_v1) {
        func_8083A098(this, anim, play);
        MergeShip_PlayMMFloorLandSfx(this);
        return;
    }

    temp_v0_2 = MergeShip_Deku_func_80836F10(play, this);
    if (temp_v0_2 > 0) {
        func_8083A098(this, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_landing, this->modelAnimType), play);
        this->skelAnime.endFrame = 8.0f;
        if (temp_v0_2 == 1) {
            this->av2.fallDamageStunTimer = 10;
        } else {
            this->av2.fallDamageStunTimer = 20;
        }
    } else if (temp_v0_2 == 0) {
        func_8083A098(this, anim, play);
    }
}

// MM func_808373A4: the spin's animation and speed.
static void MergeShip_Deku_func_808373A4(PlayState* play, Player* this) {
    Player_AnimChangeOnceMorph(play, this, DEKU_ANIM(pn_attack));
    this->unk_B10[0] = 20000.0f;
    this->unk_B10[1] = 0x30000;
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_DEKUNUTS_ATTACK);
}

// MM func_808373F8: the Deku's jump, and its hop off water.
static s32 MergeShip_Deku_func_808373F8(PlayState* play, Player* this, u16 sfxId) {
    LinkAnimationHeader* anim;
    f32 speed;
    s16 yawDiff = this->yaw - this->actor.shape.rot.y;

    if ((IREG(66) / 100.0f) < this->linearVelocity) {
        speed = IREG(67) / 100.0f;
    } else {
        speed = (IREG(68) / 100.0f + (IREG(69) * this->linearVelocity) / 1000.0f);

        if (speed < 8.0f) {
            speed = 8.0f;
        }
    }

    if ((ABS(yawDiff) >= 0x1000) || (this->linearVelocity <= 4.0f)) {
        anim = DEKU_ANIM(link_normal_jump);
    } else {
        // (MM's jump off FLOOR_PROPERTY_1/2 edges excludes the Deku.)
        anim = DEKU_ANIM(link_normal_run_jump);
    }

    // Deku hopping
    speed *= 0.3f + ((5 - this->remainingHopsCounter) * 0.18f);
    if (speed < 4.0f) {
        speed = 4.0f;
    }

    if ((this->actor.yDistToWater > 0.0f) && (this->remainingHopsCounter != 0)) {
        this->actor.world.pos.y += this->actor.yDistToWater;
        MergeShip_Deku_func_80834D50(play, this, anim, speed, MM_NA_SE_NONE);
        this->av2.actionVar2 = 1;
        this->stateFlags3 |= PLAYER_STATE3_200000;
        MergeShip_PlayMMSfxActor(&this->actor, (MM_NA_SE_PL_DEKUNUTS_JUMP5 + 1 - this->remainingHopsCounter));
        MergeShip_PlayMMVoiceSfx(this, sfxId);
        this->remainingHopsCounter--;
        // MM (2S2H VB_DEKU_LINK_SPIN_ON_LAST_HOP, default true): the last hop is a spin.
        if (this->remainingHopsCounter == 0) {
            this->stateFlags2 |= PLAYER_STATE2_HOPPING;
            MergeShip_Deku_func_808373A4(play, this);
        }

        return true;
    }

    if (this->actor.velocity.y > 0.0f) {
        sfxId = MM_NA_SE_NONE;
    }

    MergeShip_Deku_func_80834D50(play, this, anim, speed, sfxId);
    this->av2.actionVar2 = 1;

    return true;
}

// MM func_80837730: a splash where the body meets the water.
static s32 MergeShip_Deku_func_80837730(PlayState* play, Player* this, f32 arg2, s32 scale) {
    f32 sp3C = fabsf(arg2);

    if (sp3C > 2.0f) {
        WaterBox* waterBox;
        f32 sp34;
        Vec3f pos;

        Math_Vec3f_Copy(&pos, &this->bodyPartsPos[PLAYER_BODYPART_WAIST]);
        pos.y += 20.0f;
        if (WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &pos.y, &waterBox)) {
            sp34 = pos.y - this->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y;
            if ((sp34 > -2.0f) && (sp34 < 100.0f)) {
                EffectSsGSplash_Spawn(play, &pos, NULL, NULL, (sp3C <= 10.0f) ? 0 : 1, scale);
                return true;
            }
        }
    }

    return false;
}

// MM func_8083784C: about to touch the water, with hops left.
s32 MergeShip_Deku_func_8083784C(Player* this) {
    if (this->actor.velocity.y < 0.0f) {
        if ((this->actor.yDistToWater > 0.0f) &&
            ((this->ageProperties->unk_2C - this->actor.yDistToWater) < sYDistToFloor)) {
            if ((this->remainingHopsCounter != 0) && (gSaveContext.health != 0) &&
                !(this->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
                if ((this->talkActor == NULL) || !(this->talkActor->flags & ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// MM func_808378FC: hop off the water surface.
static void MergeShip_Deku_func_808378FC(PlayState* play, Player* this) {
    if (!Player_IsZTargetingWithHostileUpdate(this)) {
        this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    }

    if (MergeShip_Deku_func_8083784C(this) && MergeShip_Deku_func_808373F8(play, this, MM_NA_SE_VO_LI_AUTO_JUMP)) {
        MergeShip_Deku_func_80837730(play, this, 20.0f, this->actor.velocity.y * 50.0f);
    }
}

// MM func_808355D8: fold the petals and fall.
static void MergeShip_Deku_func_808355D8(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    MergeShip_Deku_func_80833AA0(this, play);
    this->av2.actionVar2 = -2;
    MergeShip_Deku_AnimPlayOnceFreezeAdjusted(play, this, anim);
    MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_IT_DEKUNUTS_FLOWER_CLOSE);
}

// MM func_8083CBC4: midair steering.
static s32 MergeShip_Deku_func_8083CBC4(Player* this, f32 arg1, s16 arg2, f32 arg3, f32 arg4, f32 arg5, s16 arg6) {
    s16 temp_v0 = this->yaw - arg2;

    if ((MergeShip_Deku_GetRunSpeedLimit() * 1.5f) < fabsf(this->linearVelocity)) {
        arg5 *= 4.0f;
        arg3 *= 4.0f;
    }

    if (ABS(temp_v0) > 0x6000) {
        if (!Math_StepToF(&this->linearVelocity, 0.0f, arg3)) {
            return false;
        }

        this->yaw = arg2;
    } else {
        Math_AsymStepToF(&this->linearVelocity, arg1, arg4, arg5);
        Math_ScaledStepToS(&this->yaw, arg2, arg6);
    }

    return true;
}

// MM func_8083A4A4
static s32 MergeShip_Deku_func_8083A4A4(Player* this, f32* speedTarget, s16* yawTarget, f32 decelerationRate) {
    s16 yawDiff = this->yaw - *yawTarget;

    if (ABS(yawDiff) > 0x6000) {
        if (Math_StepToF(&this->linearVelocity, 0.0f, decelerationRate)) {
            *speedTarget = 0.0f;
            *yawTarget = this->yaw;
        } else {
            return true;
        }
    }
    return false;
}

// MM func_8083F8A8: dust (or snow, or grass) kicked up by the Deku. MM scales the count by the frame time left
// (func_80173B48, 50 ms per frame at 20 fps minus RDP time); a PC frame always has the time, so the full counts
// MM gets on an idle RDP are used.
static s32 MergeShip_Deku_func_8083F8A8(PlayState* play, Player* this, f32 radius, s32 countMax, f32 randAccelWeight,
                                        s32 scale, s32 scaleStep, s32 useLighting) {
    static Color_RGBA8 D_8085D26C = { 255, 255, 255, 255 };
    static Vec3f D_8085D270 = { 0.0f, 0.04f, 0.0f };
    static Vec3f D_8085D27C = { 0.0f, 0.0f, 0.0f };
    static Vec3f D_8085D288 = { 0.0f, 0.0f, 0.0f };
    const s32 frameTimeNs = 50000000;
    u16 floorSfx = this->floorSfxOffset;

    if ((countMax < 0) || (floorSfx == (MM_NA_SE_PL_WALK_SNOW - MM_NA_SE_PL_WALK_GROUND))) {
        s32 count = frameTimeNs / 20000000;
        Vec3f pos;
        s32 i;

        count = (count >= ABS(countMax)) ? ABS(countMax) : count;
        for (i = 0; i < count; i++) {
            pos.x = Rand_CenteredFloat(40.0f) + this->actor.world.pos.x;
            pos.y = Rand_CenteredFloat(10.0f) + this->actor.world.pos.y;
            pos.z = Rand_CenteredFloat(40.0f) + this->actor.world.pos.z;
            D_8085D27C.x = Rand_CenteredFloat(3.0f);
            D_8085D27C.z = Rand_CenteredFloat(3.0f);
            EffectSsDust_Spawn(play, 0, &pos, &D_8085D27C, &D_8085D270, &D_8085D26C, &D_8085D26C, scale, scaleStep,
                               42, 0);
        }

        return true;
    } else if ((floorSfx == 0) || (floorSfx == (MM_NA_SE_PL_WALK_SAND - MM_NA_SE_PL_WALK_GROUND))) {
        s32 count = frameTimeNs / 12000000;

        if (count > 0) {
            Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, radius,
                                     (count < countMax) ? count : countMax, randAccelWeight, scale, scaleStep,
                                     useLighting);
            return true;
        }
    } else if (floorSfx == (MM_NA_SE_PL_WALK_GRASS - MM_NA_SE_PL_WALK_GROUND)) {
        s32 count = frameTimeNs / 12000000;
        Vec3f velocity;
        Vec3f pos;
        s32 i;

        count = (count >= countMax) ? countMax : count;
        for (i = 0; i < count; i++) {
            pos.x = Rand_CenteredFloat(20.0f) + this->actor.world.pos.x;
            pos.y = Rand_CenteredFloat(20.0f) + this->actor.world.pos.y;
            pos.z = Rand_CenteredFloat(20.0f) + this->actor.world.pos.z;
            velocity.x = Rand_CenteredFloat(3.0f);
            velocity.y = Rand_ZeroFloat(2.0f);
            velocity.z = Rand_CenteredFloat(3.0f);
            D_8085D288.y = -0.1f;
            EffectSsHahen_Spawn(play, &pos, &velocity, &D_8085D288, 0, 0x96, OBJECT_GAMEPLAY_KEEP, 0x10,
                                (Gfx*)mm_gKakeraLeafTipDL);
        }
    }

    return false;
}

// ---- A: the spin, and burrowing into a flower -------------------------------------------------------------------

// MM func_80839A84
s32 MergeShip_Deku_func_80839A84(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return false;
    }
    if (MergeShip_Deku_func_80836DC0(play, this)) {
        return true;
    }

    Player_SetupAction(play, this, MergeShip_Deku_Action_95, 0);
    this->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    this->unk_844 = 4;
    MergeShip_Deku_func_808373A4(play, this);
    return true;
}

// MM func_808566C0: sparkles thrown off a body part.
static void MergeShip_Deku_func_808566C0(PlayState* play, Player* this, s32 bodyPartIndex, f32 arg3, f32 arg4, f32 arg5,
                                         s32 life) {
    Color_RGBA8 primColor = { 255, 200, 200, 0 };
    Color_RGBA8 envColor = { 255, 255, 0, 0 };
    static Vec3f D_8085D940 = { 0.0f, 0.3f, 0.0f };
    static Vec3f D_8085D94C = { 0.0f, -0.025f, 0.0f };
    Vec3f pos;
    s32 scale;
    f32 sp34;
    Vec3f* temp_v0;

    if (Rand_ZeroOne() < 0.5f) {
        sp34 = -1.0f;
    } else {
        sp34 = 1.0f;
    }

    D_8085D940.x = (Rand_ZeroFloat(arg4) + arg3) * sp34;
    D_8085D94C.x = arg5 * sp34;
    if (Rand_ZeroOne() < 0.5f) {
        sp34 = -1.0f;
    } else {
        sp34 = 1.0f;
    }

    temp_v0 = &this->bodyPartsPos[bodyPartIndex];
    D_8085D940.z = (Rand_ZeroFloat(arg4) + arg3) * sp34;
    D_8085D94C.z = arg5 * sp34;
    pos.x = temp_v0->x;
    pos.y = Rand_ZeroFloat(15.0f) + temp_v0->y;
    pos.z = temp_v0->z;
    if (Rand_ZeroOne() < 0.5f) {
        scale = 2000;
    } else {
        scale = -150;
    }

    EffectSsKiraKira_SpawnDispersed(play, &pos, &D_8085D940, &D_8085D94C, &primColor, &envColor, scale, life);
}

// MM Player_Action_95: the Deku spin.
void MergeShip_Deku_Action_95(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;

    LinkAnimation_Update(play, &this->skelAnime);
    MergeShip_Deku_SetCylinderForAttack(this, MERGESHIP_DEKU_DMG_SPIN, 1, 30);

    if (!Player_ActionHandler_13(this, play)) {
        s16 prevYaw = this->yaw;
        f32 speedTarget;
        s16 yawTarget;

        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_CURVED, play);
        speedTarget *= 1.0f - (0.9f * ((11100.0f - this->unk_B10[0]) / 11100.0f));

        if (!MergeShip_Deku_func_8083A4A4(this, &speedTarget, &yawTarget, REG(43) / 100.0f)) {
            func_8083DF68(this, speedTarget, yawTarget);
        }

        this->unk_B10[0] += -800.0f;
        this->actor.shape.rot.y += (s16)(DEKU_TRUNCF_BINANG(this->unk_B10[0]) + (s16)(this->yaw - prevYaw));

        if (Math_StepToF(&this->unk_B10[1], 0.0f, this->unk_B10[0])) {
            this->actor.shape.rot.y = this->yaw;
            func_8083CF10(this, play);
            // MergeShip2 (BUG-0018): Z still held and nothing locked on -- back to Parallel (z_player.c keeps the
            // Z-targeting through the spin).
            if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_Z) && (this->focusActor == NULL) &&
                (this->stateFlags1 & PLAYER_STATE1_Z_TARGETING)) {
                Player_SetParallel(this);
            }
        } else if (DEKU_ANIM_IS(this->skelAnime.animation, pn_attack)) {
            this->stateFlags3 |= PLAYER_STATE3_100000;

            if (this->unk_B10[1] < 0.0f) {
                Player_AnimChangeOnceMorph(play, this, Player_GetIdleAnim(this));
            }
        }

        MergeShip_Deku_func_808566C0(play, this, PLAYER_BODYPART_WAIST, 1.0f, 0.5f, 0.0f, 32);

        if (this->unk_B10[0] > 9500.0f) {
            MergeShip_Deku_func_8083F8A8(play, this, 2.0f, 1, 2.5f, 10, 18, true);
        }

        // MM func_800AE930: the tire mark along the floor (MM EffectTireMark, kept with the Goron's port).
        MergeShip_Deku_LayTireMark(play, this);
        MergeShip_PlayMMSfxActor(&this->actor, MergeShip_GetMMFloorSfx(this, MM_NA_SE_PL_SLIP_LEVEL - SFX_FLAG));
    }
}

// ---- falling and landing ----------------------------------------------------------------------------------

// MM Player_Action_25 (midair), for the Deku.
void MergeShip_Deku_Action_25(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;

    if (Player_CheckHostileLockOn(this)) {
        this->actor.gravity = -1.2f;
    }

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        s16 prevYaw;

        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

        // (MM's thrown-cucco branch needs a carried actor; the Deku cannot carry.)
        LinkAnimation_Update(play, &this->skelAnime);
        if (DEKU_ANIM_IS(this->skelAnime.animation, link_normal_newroll_jump_20f) &&
            LinkAnimation_OnFrame(&this->skelAnime, 4.0f)) {
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_ROLL);
        }

        prevYaw = this->yaw;

        MergeShip_Deku_func_808378FC(play, this);
        MergeShip_Deku_func_8083CBC4(this, speedTarget * 0.5f, yawTarget, 2.0f, 0.2f, 0.1f, 0x190);

        if (DEKU_ANIM_IS(this->skelAnime.animation, pn_attack)) {
            this->stateFlags2 |= (PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS);

            this->unk_B10[0] += -800.0f;
            this->actor.shape.rot.y += (s16)(DEKU_TRUNCF_BINANG(this->unk_B10[0]) + (s16)(this->yaw - prevYaw));
            Math_StepToF(&this->unk_B10[1], 0.0f, this->unk_B10[0]);
        }

        Player_UpdateUpperBody(this, play);
        // MM also starts a jump slash here (func_80839770); it needs a melee weapon, which the Deku has not.
        if (this->actor.velocity.y < 0.0f) {
            if (this->av2.actionVar2 >= 0) {
                if ((this->actor.bgCheckFlags & BGCHECKFLAG_WALL) || (this->av2.actionVar2 == 0) ||
                    (this->fallDistance > 0)) {
                    if ((sYDistToFloor > 800.0f) || (this->stateFlags1 & PLAYER_STATE1_HOOKSHOT_FALLING)) {
                        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_FALL_S);
                        this->stateFlags1 &= ~PLAYER_STATE1_HOOKSHOT_FALLING;
                    }
                    LinkAnimation_Change(play, &this->skelAnime, DEKU_ANIM(link_normal_landing), 1.0f, 0.0f, 0.0f,
                                         ANIMMODE_ONCE, 8.0f);
                    this->av2.actionVar2 = -1;
                }
            } else {
                if ((this->av2.actionVar2 == -1) && (this->fallDistance > 120) && (sYDistToFloor > 280.0f)) {
                    this->av2.actionVar2 = -2;
                    MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_FALL_L);
                }

                if ((this->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) &&
                    !(this->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_WATER)) &&
                    (this->linearVelocity > 0.0f)) {
                    // MM: the Deku only grabs ledges while it still has hops.
                    if (this->remainingHopsCounter != 0) {
                        if ((this->yDistToLedge >= 150.0f) &&
                            (this->controlStickDirections[this->controlStickDataIndex] == 0)) {
                            if (func_8083EC18(this, play, sTouchedWallFlags)) {
                                if (MergeShip_Deku_func_80837730(play, this, 3.0f, 500)) {
                                    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER);
                                }
                            }
                        } else if ((this->ledgeClimbType >= 2) &&
                                   ((this->yDistToLedge < (150.0f * this->ageProperties->unk_08)) &&
                                    (((this->actor.world.pos.y - this->actor.floorHeight) + this->yDistToLedge)) >
                                        (70.0f * this->ageProperties->unk_08))) {
                            AnimationContext_DisableQueue(play);
                            if (this->stateFlags1 & PLAYER_STATE1_HOOKSHOT_FALLING) {
                                MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_HOOKSHOT_HANG);
                            } else {
                                MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_HANG);
                            }

                            this->actor.world.pos.y += this->yDistToLedge;
                            func_8083A5C4(play, this, this->actor.wallPoly, this->distToInteractWall,
                                          GET_PLAYER_ANIM(PLAYER_ANIMGROUP_jump_climb_hold, this->modelAnimType));
                            this->yaw += 0x8000;
                            this->actor.shape.rot.y = this->yaw;
                            this->stateFlags1 |= PLAYER_STATE1_HANGING_OFF_LEDGE;

                            if (MergeShip_Deku_func_80837730(play, this, 3.0f, 500)) {
                                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER);
                            }
                        }
                    }
                }
            }
        }
    } else {
        MergeShip_Deku_func_80837134(play, this);
        Player_UpdateUpperBody(this, play);
    }

    Player_ActionHandler_13(this, play);
}

// MM func_8083827C (leaving the ground), for the Deku: a hop off a ledge while hops remain, and the hop
// counter's reset on the ground.
void MergeShip_Deku_func_8083827C(Player* this, PlayState* play) {
    s32 temp_t0;

    this->fallDistance = this->fallStartHeight - (s32)this->actor.world.pos.y;
    if (!(this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE)) &&
        ((this->stateFlags1 & PLAYER_STATE1_FLOOR_DISABLED) ||
         !(this->stateFlags3 & (PLAYER_STATE3_200 | PLAYER_STATE3_2000))) &&
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        // MM func_80835428 is Ocarina of Time's func_80838FB8.
        if (func_80838FB8(play, this)) {
            return;
        }

        if (sPrevFloorProperty == FLOOR_PROPERTY_8) {
            this->actor.world.pos.x = this->actor.prevPos.x;
            this->actor.world.pos.z = this->actor.prevPos.z;
            return;
        }

        if ((this->stateFlags3 & PLAYER_STATE3_MIDAIR) || (this->skelAnime.movementFlags & 0x80)) {
            return;
        }

        if ((Player_Action_8084411C == this->actionFunc) || (Player_Action_80844A44 == this->actionFunc)) {
            return;
        }

        if ((sPrevFloorProperty == FLOOR_PROPERTY_7) || (this->meleeWeaponState != 0)) {
            Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
            if (this->linearVelocity > 0.0f) {
                Player_ZeroSpeedXZ(this);
            }
            this->actor.bgCheckFlags |= BGCHECKFLAG_GROUND_TOUCH;
            return;
        }

        temp_t0 = (s16)(this->yaw - this->actor.shape.rot.y);
        Player_SetupAction(play, this, Player_Action_8084411C, 1);
        func_80832440(play, this);

        this->floorSfxOffset = this->prevFloorSfxOffset;
        if ((this->remainingHopsCounter != 0) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_LEAVE)) {
            if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                if ((sPrevFloorProperty != FLOOR_PROPERTY_6) && (sPrevFloorProperty != FLOOR_PROPERTY_9) &&
                    (sYDistToFloor > 20.0f) && (this->meleeWeaponState == 0)) {
                    if ((ABS(temp_t0) < 0x2000) && (this->linearVelocity > 3.0f)) {
                        MergeShip_Deku_func_808373F8(play, this, MM_NA_SE_VO_LI_AUTO_JUMP);
                        return;
                    }
                }
            }
        }

        // MM func_80837DEC (hang from the ledge just walked off) excludes the Deku.
        Player_AnimPlayLoop(play, this, DEKU_ANIM(link_normal_landing_wait));
    } else {
        this->fallStartHeight = this->actor.world.pos.y;
        this->remainingHopsCounter = 5;
    }
}

// MM func_8083B8D0
static void MergeShip_Deku_func_8083B8D0(PlayState* play, Player* this) {
    if (MergeShip_Deku_func_80837730(play, this, this->actor.velocity.y, 500)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER);
        if (this->fallDistance > 800) {
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
        }
    }
}

// MM func_8083BB4C, Deku branch: a hop while hops remain; otherwise the Deku sinks and is put back where it last
// stood (MM's Southern Swamp / Great Bay scene exits have no Hyrule counterpart, so MM's general case applies).
void MergeShip_Deku_EnterWater(PlayState* play, Player* this) {
    if (this->remainingHopsCounter != 0) {
        MergeShip_Deku_func_808373F8(play, this, MM_NA_SE_VO_LI_AUTO_JUMP);
    } else {
        Player_SetupAction(play, this, MergeShip_Deku_Action_1, 0);
        this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
        MergeShip_Deku_func_8083B8D0(play, this);
    }
}

// MM Player_Action_1: fade to black and respawn at the room's entry point (RESPAWN_MODE_DOWN), without a scene
// reload.
void MergeShip_Deku_Action_1(Player* this, PlayState* play) {
    this->stateFlags3 |= PLAYER_STATE3_10000000;
    LinkAnimation_Update(play, &this->skelAnime);
    Player_UpdateUpperBody(this, play);

    if (R_PLAY_FILL_SCREEN_ON == 0) {
        R_PLAY_FILL_SCREEN_ON = 20;
        R_PLAY_FILL_SCREEN_ALPHA = 0;
        R_PLAY_FILL_SCREEN_R = R_PLAY_FILL_SCREEN_G = R_PLAY_FILL_SCREEN_B = R_PLAY_FILL_SCREEN_ALPHA;
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_DEKUNUTS_JUMP_FAILED);
    } else if (R_PLAY_FILL_SCREEN_ON > 0) {
        R_PLAY_FILL_SCREEN_ALPHA += R_PLAY_FILL_SCREEN_ON;
        if (R_PLAY_FILL_SCREEN_ALPHA > 255) {
            R_PLAY_FILL_SCREEN_ALPHA = 255;
            if (this->unk_B86[0] == 0) {
                this->unk_B86[0] = 1;
                func_80832564(play, this);
            } else {
                RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_DOWN];

                R_PLAY_FILL_SCREEN_ON = -20;
                this->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
                this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                Player_SetEquipmentData(play, this);
                this->prevBoots = this->currentBoots;

                // MM keeps its own room-entry point (unk_3C0, func_80841A50) and falls back to RESPAWN_MODE_DOWN;
                // Ocarina of Time records its room entries in RESPAWN_MODE_DOWN, so that is used.
                Math_Vec3f_Copy(&this->actor.world.pos, &respawn->pos);
                this->actor.shape.rot.y = respawn->yaw;

                Math_Vec3f_Copy(&this->actor.prevPos, &this->actor.world.pos);
                this->linearVelocity = 0.0f;
                this->yaw = this->actor.shape.rot.y;
                this->actor.velocity.y = 0.0f;
                Player_AnimPlayOnce(play, this, Player_GetIdleAnim(this));

                if ((play->roomCtx.curRoom.num == respawn->roomIndex) && (play->roomCtx.prevRoom.num < 0)) {
                    this->av2.actionVar2 = 5;
                } else {
                    play->roomCtx.curRoom.num = -1;
                    play->roomCtx.prevRoom.num = -1;
                    play->roomCtx.curRoom.segment = NULL;
                    play->roomCtx.prevRoom.segment = NULL;

                    Room_FinishRoomChange(play, &play->roomCtx);
                    this->av2.actionVar2 = -1;
                    this->av1.actionVar1 = respawn->roomIndex;
                }
            }
        }
    } else if (this->av2.actionVar2 < 0) {
        if (Room_RequestNewRoom(play, &play->roomCtx, this->av1.actionVar1)) {
            Map_InitRoomData(play, play->roomCtx.curRoom.num);
            this->av2.actionVar2 = 5;
        }
    } else if (this->av2.actionVar2 > 0) {
        this->av2.actionVar2--;
    } else {
        R_PLAY_FILL_SCREEN_ALPHA += R_PLAY_FILL_SCREEN_ON;
        if (R_PLAY_FILL_SCREEN_ALPHA < 0) {
            R_PLAY_FILL_SCREEN_ALPHA = 0;
            R_PLAY_FILL_SCREEN_ON = 0;
            Player_SetInvulnerability(this, -40);
            // MM func_8085B384
            Player_SetupAction(play, this, Player_Action_Idle, 1);
            Player_AnimChangeOnceMorph(play, this, Player_GetIdleAnim(this));
            this->yaw = this->actor.shape.rot.y;
            this->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
        }
    }
}

// ---- burning ---------------------------------------------------------------------------------------------

// MM Player_Action_77: a burnt Deku voids out.
void MergeShip_Deku_Action_77(Player* this, PlayState* play) {
    if (this->skelAnime.animation == NULL) {
        this->stateFlags2 |= PLAYER_STATE2_FROZEN;
    } else {
        LinkAnimation_Update(play, &this->skelAnime);
        if (DEKU_ANIM_IS(this->skelAnime.animation, link_derth_rebirth) &&
            LinkAnimation_OnFrame(&this->skelAnime, 60.0f)) {
            MergeShip_PlayMMFloorSfx(this, MM_NA_SE_PL_BOUND);
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_DAMAGE_S);
        }
    }

    if ((this->av2.actionVar2++ >= 9) && (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
        if (this->av1.actionVar1 != 0) {
            // MM func_80169EFC is Ocarina of Time's void out.
            Play_TriggerVoidOut(play);
            play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
            MergeShip_PlayMMSfxCentered(MM_NA_SE_OC_ABYSS);
        }
    }
}

// MM func_80834140
static void MergeShip_Deku_func_80834140(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    if (!(this->stateFlags1 & PLAYER_STATE1_DEAD)) {
        // MM func_80834104
        Player_SetupAction(play, this, MergeShip_Deku_Action_77, 0);
        this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED;
        this->stateFlags1 |= PLAYER_STATE1_DEAD;
        LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, 84.0f, ANIMMODE_ONCE, -6.0f);
        this->av1.actionVar1 = 1;
        this->linearVelocity = 0.0f;
    }
}

// MM Player_UpdateBodyBurn, Deku branch: the flames never die down on a Deku; once it is on the ground it collapses
// and voids out.
void MergeShip_Deku_UpdateBodyBurn(PlayState* play, Player* this) {
    f32 temp_fv0;
    f32 flameScale;
    f32 flameIntensity;
    s32 i;
    s32 timerStep = 0;
    s32 spawnedFlame = false;
    u8* timerPtr = this->bodyFlameTimers;

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        if (this->cylinder.base.ocFlags1 & OC1_HIT) {
            Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
            this->linearVelocity = 0.0f;
        }
        MergeShip_Deku_func_80834140(play, this, DEKU_ANIM(link_derth_rebirth));
    }

    for (i = 0; i < PLAYER_BODYPART_MAX; i++, timerPtr++) {
        if (*timerPtr <= timerStep) {
            *timerPtr = 0;
        } else {
            spawnedFlame = true;
            *timerPtr -= timerStep;
            if (*timerPtr > 20.0f) {
                temp_fv0 = (*timerPtr - 20.0f) * 0.01f;
                flameScale = CLAMP(temp_fv0, 0.19999999f, 0.2f);
            } else {
                flameScale = *timerPtr * 0.01f;
            }

            flameIntensity = (*timerPtr - 25.0f) * 0.02f;
            flameIntensity = CLAMP(flameIntensity, 0.0f, 1.0f);
            EffectSsFireTail_SpawnFlameOnPlayer(play, flameScale, i, flameIntensity);
        }
    }

    if (spawnedFlame) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_TORCH - SFX_FLAG);
        if ((play->gameplayFrames % 4) == 0) {
            Player_InflictDamage(play, -1);
        }
    } else {
        this->bodyIsBurning = false;
    }
}

// ---- R: the guard ----------------------------------------------------------------------------------------

// MM Player_ActionHandler_11, Deku branch (MM Player_IsGoronOrDeku: no shield needed, no lock-on restriction).
s32 MergeShip_Deku_ActionHandler_11(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R) && (this->unk_6AD == 0) &&
        (play->shootingGalleryStatus == 0)) {
        func_80832318(this);
        Player_DetachHeldActor(play, this);
        if (Player_SetupAction(play, this, Player_Action_80843188, 0)) {
            LinkAnimationHeader* anim = DEKU_ANIM(pn_gurd);
            f32 endFrame;

            this->stateFlags1 |= PLAYER_STATE1_SHIELDING;

            if (anim != this->skelAnime.animation) {
                if (Player_CheckHostileLockOn(this)) {
                    this->unk_86C = 1.0f;
                } else {
                    this->unk_86C = 0.0f;
                    func_80833C3C(this);
                }
                this->upperLimbRot.x = 0;
                this->upperLimbRot.y = 0;
                this->upperLimbRot.z = 0;
            }

            endFrame = Animation_GetLastFrame(anim);
            LinkAnimation_Change(play, &this->skelAnime, anim, 2.0f / 3.0f, 0.0f, endFrame, ANIMMODE_ONCE, 0.0f);
            // MM func_80830AE8
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_CHANGE_ARMS);
        }

        return true;
    }

    return false;
}

// MM Player_Action_18, Deku path.
void MergeShip_Deku_Action_18(Player* this, PlayState* play) {
    Player_DecelerateToZero(this);

    if (LinkAnimation_Update(play, &this->skelAnime)) {
        this->av2.actionVar2 = 1;
        this->av1.actionVar1 = 0;
    }

    if (this->av2.actionVar2 != 0) {
        f32 yStick = sControlInput->rel.stick_y * 180;
        f32 xStick = sControlInput->rel.stick_x * -120;
        s16 temp_a0 = this->actor.shape.rot.y - Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
        s16 var_a1;
        s16 temp_ft5;
        s16 var_a2;
        s16 var_a3;

        var_a1 = (yStick * Math_CosS(temp_a0)) + (Math_SinS(temp_a0) * xStick);
        temp_ft5 = (xStick * Math_CosS(temp_a0)) - (Math_SinS(temp_a0) * yStick);

        var_a1 = CLAMP_MAX(var_a1, 0xDAC);
        var_a2 = ABS(var_a1 - this->actor.focus.rot.x) * 0.25f;
        var_a2 = CLAMP_MIN(var_a2, 0x64);

        var_a3 = ABS(temp_ft5 - this->upperLimbRot.y) * 0.25f;
        var_a3 = CLAMP_MIN(var_a3, 0x32);
        Math_ScaledStepToS(&this->actor.focus.rot.x, var_a1, var_a2);

        this->upperLimbRot.x = this->actor.focus.rot.x;
        Math_ScaledStepToS(&this->upperLimbRot.y, temp_ft5, var_a3);

        if (this->av1.actionVar1 != 0) {
            if (!func_80842DF4(play, this)) {
                if (this->skelAnime.curFrame < 2.0f) {
                    func_80833A20(this, 1);
                }
            } else {
                this->av2.actionVar2 = 1;
                this->av1.actionVar1 = 0;
            }
        } else if (!func_80842964(this, play)) {
            if (MergeShip_Deku_ActionHandler_11(this, play)) {
                func_808428D8(this, play);
            } else {
                this->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
                func_80832318(this);

                func_8083A060(this, play);
                LinkAnimation_Change(play, &this->skelAnime, this->skelAnime.animation, 1.0f,
                                     Animation_GetLastFrame(this->skelAnime.animation), 0.0f, ANIMMODE_ONCE, 0.0f);

                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_SHIELD_REMOVE);
                return;
            }
        } else {
            return;
        }
    }

    this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
    Player_SetModelsForHoldingShield(this);
    this->unk_6AE_rotFlags |= UNK6AE_ROT_FOCUS_X | UNK6AE_ROT_UPPER_X | UNK6AE_ROT_UPPER_Y;
}

// MM func_80834600: the guard's shield cylinder bounced an attack (MM !Player_IsGoronOrDeku skips the recoil
// animation; the push back applies).
s32 MergeShip_Deku_GuardBounced(Player* this) {
    if ((this->mergeShipForm != PLAYER_FORM_DEKU) || (this->shieldCylinder.base.actor != &this->actor) ||
        !(this->shieldCylinder.base.acFlags & AC_BOUNCED)) {
        return false;
    }

    Player_RequestRumble(this, 180, 20, 100, 0);

    if (!(this->stateFlags1 & ((1 << 2) | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                               PLAYER_STATE1_CLIMBING_LADDER))) {
        this->linearVelocity = -18.0f;
        this->yaw = this->actor.shape.rot.y;
    }

    return true;
}

// ---- the Deku Flower ----------------------------------------------------------------------------------------

// MM func_80856000: is there room above to launch?
static s32 MergeShip_Deku_func_80856000(PlayState* play, Player* this) {
    CollisionPoly* poly;
    s32 bgId;
    Vec3f pos;
    f32 sp28;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.world.pos.y - 20.0f;
    pos.z = this->actor.world.pos.z;
    return BgCheck_EntityCheckCeiling(&play->colCtx, &sp28, &pos, 30.0f, &poly, &bgId, &this->actor);
}

// MM func_80856074
static void MergeShip_Deku_func_80856074(PlayState* play, Player* this) {
    if (MergeShip_Deku_func_8083F8A8(play, this, 12.0f, 4, 0.0f, 10, 50, true)) {
        EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 3.0f, 0, 4, 8, 2, -1, 10, NULL);
    }
}

// MM func_80856110: pollen puff.
static void MergeShip_Deku_func_80856110(PlayState* play, Player* this, f32 arg2, f32 arg3, f32 arg4, s16 scale,
                                         s16 scaleStep, s16 life) {
    static Vec3f D_8085D918 = { 0.0f, 0.5f, 0.0f };
    static Vec3f D_8085D924 = { 0.0f, 0.5f, 0.0f };
    static Color_RGBA8 D_8085D930 = { 255, 255, 55, 255 };
    static Color_RGBA8 D_8085D934 = { 100, 50, 0, 0 };
    Vec3f pos;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.world.pos.y + arg2;
    pos.z = this->actor.world.pos.z;

    D_8085D918.y = arg3;
    D_8085D924.y = arg4;

    // MM func_800B0EB0 is Ocarina of Time's func_8002836C (EffectSsDust, draw flag 0).
    func_8002836C(play, &pos, &D_8085D918, &D_8085D924, &D_8085D930, &D_8085D934, scale, scaleStep, life);
}

// MM func_80855F9C: turning while burrowed.
static void MergeShip_Deku_func_80855F9C(PlayState* play, Player* this) {
    f32 speedTarget;
    s16 yawTarget;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_CURVED, play);
    Math_ScaledStepToS(&this->yaw, yawTarget, 0x258);
}

// MM Player_Action_93: burrow into the flower, wait, and launch.
// MergeShip2 (BUG-0019, the Haunted Wasteland's sand): frames spent sinking in, and held in by a ceiling after A was let
// go. MM sinks in about 20 frames; whatever holds the Deku longer (the sand moving him off the flower), he finishes
// sinking / comes out anyway, so he can never be left in the flower.
static s32 sMergeShipBurrowFrames = 0;
static s32 sMergeShipBurrowHeldFrames = 0;

void MergeShip_Deku_Action_93(Player* this, PlayState* play) {
    DynaPolyActor* dyna;
    s32 aux = 0xAE;
    f32 temp_fv0_2;
    s32 sp38;
    s32 var_v1;

    LinkAnimation_Update(play, &this->skelAnime);

    if (Player_ActionHandler_13(this, play)) {
        return;
    }

    if (this->av1.actionVar1 >= 2) {
        sMergeShipBurrowFrames = 0;
    } else if (++sMergeShipBurrowFrames > 90) {
        // Finish sinking in at once (the end of phase 1).
        sMergeShipBurrowFrames = 0;
        this->unk_ABC = -3900.0f;
        this->unk_B48 = 0.0f;
        this->av1.actionVar1 = 2;
        this->av2.actionVar2 = 0;
        this->actor.scale.y = 0.01f;
        this->actor.scale.x = this->actor.scale.z = 0.01f;
    }

    if (this->av1.actionVar1 == 0) {
        this->unk_ABC += this->unk_B48;
        if (this->unk_ABC < -1000.0f) {
            this->unk_ABC = -1000.0f;
            this->av1.actionVar1 = 1;
            this->unk_B48 = 0.0f;
        }
        MergeShip_Deku_func_80856074(play, this);
    } else if (this->av1.actionVar1 == 1) {
        this->unk_B48 += -22.0f;
        if (this->unk_B48 < -170.0f) {
            this->unk_B48 = -170.0f;
        }
        this->unk_ABC += this->unk_B48;
        if (this->unk_ABC < -3900.0f) {
            this->unk_ABC = -3900.0f;
            this->av1.actionVar1 = 2;
            this->actor.shape.rot.y = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
            this->actor.scale.y = 0.01f;
            this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
        } else {
            temp_fv0_2 = Math_SinS((1000.0f + this->unk_ABC) * (-30.0f)) * 0.004f;
            this->actor.scale.y = 0.01f + temp_fv0_2;
            this->actor.scale.z = this->actor.scale.x = 0.01f - (this->unk_B48 * -0.000015f);

            this->actor.shape.rot.y += DEKU_TRUNCF_BINANG(this->unk_B48 * 130.0f);
            if (this->actor.floorBgId != BGCHECK_SCENE) {
                dyna = DynaPoly_GetActor(&play->colCtx, this->actor.floorBgId);

                if (dyna != NULL) {
                    MergeShip_Deku_Vec3fStepToXZ(&this->actor.world.pos, &dyna->actor.world.pos, 1.0f);
                }
            }
        }

        MergeShip_Deku_func_80856074(play, this);
    } else if (this->av1.actionVar1 == 2) {
        if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_A)) {
            if (MergeShip_Deku_func_80856000(play, this) && (++sMergeShipBurrowHeldFrames <= 60)) {
                this->av2.actionVar2 = 0;
            } else {
                sMergeShipBurrowHeldFrames = 0;
                this->av1.actionVar1 = 3;
                if (this->av2.actionVar2 >= 10) {
                    this->unk_B48 = 2700.0f;
                } else {
                    this->unk_B48 = 1450.0f;
                }
                MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_PL_DEKUNUTS_OUT_GRD);
            }
        } else if (this->av2.actionVar2 < 15) {
            this->av2.actionVar2++;
            if (this->av2.actionVar2 == 10) {
                MergeShip_Deku_func_80856110(play, this, 20.0f, 3.8f, -0.1f, 140, 23, 15);
            }
        }
        MergeShip_Deku_func_80855F9C(play, this);
    } else {
        this->unk_ABC += this->unk_B48;

        temp_fv0_2 = this->unk_ABC;
        if (temp_fv0_2 >= 0.0f) {
            f32 speed;

            sp38 = (this->av2.actionVar2 >= 10);
            var_v1 = -1;
            speed = this->unk_B48 * this->actor.scale.y;
            if (this->actor.floorBgId != BGCHECK_SCENE) {
                dyna = DynaPoly_GetActor(&play->colCtx, this->actor.floorBgId);
                var_v1 = 0;
                // MM: a gold flower (params 0x100) launches higher.
                if ((dyna != NULL) && (dyna->actor.id == gMergeShipActorId_MM_Obj_Etcetera) &&
                    (dyna->actor.params & 0x100)) {
                    var_v1 = 1;
                    speed *= aux / 100.0f;
                }
            }

            Math_Vec3f_Copy(this->unk_AF0, &this->actor.world.pos);
            this->unk_ABC = 0.0f;
            this->actor.world.pos.y += temp_fv0_2 * this->actor.scale.y;
            // MM func_80834DB8
            MergeShip_Deku_func_80834D50(play, this, DEKU_ANIM(pn_kakku), speed, MM_NA_SE_VO_LI_SWORD_N);
            Player_SetupAction(play, this, MergeShip_Deku_Action_94, 1);
            this->zoraBoomerangActor = NULL;

            this->stateFlags3 |= PLAYER_STATE3_200;
            if (sp38 != 0) {
                this->stateFlags3 |= PLAYER_STATE3_2000;
            }
            if (var_v1 < 0) {
                this->stateFlags3 |= PLAYER_STATE3_40000;
            }

            this->av1.actionVar1 = var_v1;
            this->av2.actionVar2 = 9999;
            MergeShip_Deku_SetCylinderForAttack(this, MERGESHIP_DEKU_DMG_LAUNCH, 2, 20);
        } else if (this->unk_ABC < 0.0f) {
            MergeShip_Deku_func_80856074(play, this);
        }
    }

    if (this->unk_ABC < -1500.0f) {
        this->stateFlags3 |= PLAYER_STATE3_100;
        if (this->unk_B86[0] < 8) {
            this->unk_B86[0]++;
            if (this->unk_B86[0] == 8) {
                MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_PL_DEKUNUTS_BUD);
            }
        }
    }
}

// MM func_80856888
static s32 MergeShip_Deku_func_80856888(f32* arg0, f32 arg1, f32 arg2) {
    if (arg2 != 0.0f) {
        if (arg1 < *arg0) {
            arg2 = -arg2;
        }

        *arg0 += arg2;
        if (((*arg0 - arg1) * arg2) >= 0.0f) {
            *arg0 = arg1;
            return true;
        }
    } else if (arg1 == *arg0) {
        return true;
    }

    return false;
}

// MM Player_Action_94: launched out of the flower, then gliding on the petals.
void MergeShip_Deku_Action_94(Player* this, PlayState* play) {
    static f32 D_8085D958[] = { 600.0f, 960.0f };
    static Vec3f D_8085D960 = { -30.0f, 50.0f, 0.0f };
    static Vec3f D_8085D96C = { 30.0f, 50.0f, 0.0f };

    if ((this->zoraBoomerangActor != NULL) && (this->zoraBoomerangActor->update == NULL)) {
        this->zoraBoomerangActor = NULL;
    }

    if (Player_ActionHandler_13(this, play)) {
        return;
    }

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        MergeShip_Deku_func_80837134(play, this);
        return;
    }

    if ((this->actor.velocity.y > 0.0f) && (this->stateFlags3 & PLAYER_STATE3_200)) {
        this->actor.minVelocityY = -20.0f;
        this->actor.gravity = -5.5f;
        MergeShip_Deku_SetCylinderForAttack(this, MERGESHIP_DEKU_DMG_LAUNCH, 2, 20);
        MergeShip_Deku_func_80856110(play, this, 0.0f, 0.0f, -1.0f, 500, 0, 8);

        if (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING) {
            MergeShip_Deku_func_80833AA0(this, play);
        }
    } else if (!(this->stateFlags3 & PLAYER_STATE3_2000)) {
        MergeShip_Deku_func_80833AA0(this, play);
    } else if (this->stateFlags3 & PLAYER_STATE3_200) {
        if (this->actor.velocity.y < 0.0f) {
            if (this->av1.actionVar1 < 0) {
                MergeShip_Deku_func_80833AA0(this, play);
            } else {
                LinkAnimation_Update(play, &this->skelAnime);
                if (this->skelAnime.curFrame > 6.0f) {
                    this->actor.velocity.y = 6.0f;
                    this->stateFlags3 &= ~PLAYER_STATE3_200;
                    this->stateFlags3 |= PLAYER_STATE3_1000000;
                    MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_IT_DEKUNUTS_FLOWER_OPEN);
                    // (MM Audio_SetSfxTimerLerpInterval(4, 2): MM audio's own timing, the audio port's.)
                }
            }
        }

        this->actor.minVelocityY = -10.0f;
        this->actor.gravity = -0.5f;
        MergeShip_Deku_ResetCylinder(this);
    } else if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A)) {
        MergeShip_Deku_func_808355D8(play, this, DEKU_ANIM(pn_kakkufinish));
    } else {
        s16 temp_a0;
        f32 temp_fv1;
        s16 sp76;
        s16 var_a1;
        f32 speedTarget;
        f32 sp68;
        s16 yawTarget;
        s16 temp_ft0;
        s32 temp;

        this->linearVelocity = sqrtf(SQ(this->actor.velocity.x) + SQ(this->actor.velocity.z));
        if (this->linearVelocity != 0.0f) {
            var_a1 = Math_Atan2S(this->actor.velocity.z, this->actor.velocity.x);

            temp_a0 = this->actor.shape.rot.y - var_a1;
            if (ABS(temp_a0) > 0x4000) {
                this->linearVelocity = -this->linearVelocity;
                var_a1 += 0x8000;
            }
            this->yaw = var_a1;
        }

        if (this->windSpeed != 0.0f) {
            Math_SmoothStepToS(&this->unk_B8C, this->windAngleX, 3, 0x1F40, 0x190);
        }

        if (this->av2.actionVar2 != 0) {
            this->av2.actionVar2--;
        }

        temp_fv1 = D_8085D958[this->av1.actionVar1] - Math_Vec3f_DistXZ(&this->actor.world.pos, this->unk_AF0);
        LinkAnimation_Update(play, &this->skelAnime);

        if ((this->av2.actionVar2 != 0) && (temp_fv1 > 300.0f)) {
            sp76 = 0x1770;
            if (!DEKU_ANIM_IS(this->skelAnime.animation, pn_kakku)) {
                MergeShip_Deku_AnimPlayOnceFreezeAdjusted(play, this, DEKU_ANIM(pn_kakkufinish));
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 8.0f)) {
                s32 i;

                MergeShip_Deku_TranslateAndRotateY(this, &this->actor.world.pos, &D_8085D960,
                                                   &this->bodyPartsPos[PLAYER_BODYPART_L_HAND]);
                MergeShip_Deku_TranslateAndRotateY(this, &this->actor.world.pos, &D_8085D96C,
                                                   &this->bodyPartsPos[PLAYER_BODYPART_R_HAND]);

                for (i = 0; i < 13; i++) {
                    MergeShip_Deku_func_808566C0(play, this, PLAYER_BODYPART_L_HAND, 0.6f, 1.0f, 0.8f, 17);
                    MergeShip_Deku_func_808566C0(play, this, PLAYER_BODYPART_R_HAND, 0.6f, 1.0f, 0.8f, 17);
                }
            }
        } else if ((this->av2.actionVar2 == 0) || (temp_fv1 < 0.0f)) {
            sp76 = 0;
            MergeShip_Deku_func_808355D8(play, this, DEKU_ANIM(pn_rakkafinish));
        } else {
            sp76 = 0x1770 - (s32)((300.0f - temp_fv1) * 10.0f);

            if (!DEKU_ANIM_IS(this->skelAnime.animation, pn_batabata)) {
                MergeShip_Deku_AnimPlayLoopMorphAdjusted(play, this, DEKU_ANIM(pn_batabata));
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 6.0f)) {
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_DEKUNUTS_STRUGGLE);
            }
        }

        MergeShip_Deku_AsymStepToS(&this->unk_B86[1], sp76, 0x190, 0x190);

        this->unk_B8A += this->unk_B86[1];
        temp = ABS(this->unk_B86[1]);
        if (temp > 0xFA0) {
            this->unk_B66 += (u8)(ABS(this->unk_B86[1]) * 0.01f);
        }

        if (this->unk_B66 > 200) {
            this->unk_B66 -= 200;
            MergeShip_Deku_func_808566C0(play, this, PLAYER_BODYPART_L_HAND, 0.0f, 1.0f, 0.0f, 32);
            MergeShip_Deku_func_808566C0(play, this, PLAYER_BODYPART_R_HAND, 0.0f, 1.0f, 0.0f, 32);
        }

        // MM Audio_PlaySfx_AtPosWithTimer(.., NA_SE_IT_DEKUNUTS_FLOWER_ROLL, propeller speed): the timer (pitch by
        // propeller speed) is the audio port's.
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_DEKUNUTS_FLOWER_ROLL);
        if ((this->zoraBoomerangActor == NULL) && CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
            if (AMMO(ITEM_NUT) == 0) {
                MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
            } else {
                MergeShip_Deku_EnsureActorsRegistered();
                this->zoraBoomerangActor =
                    (gMergeShipActorId_MM_En_Arrow < 0)
                        ? NULL
                        : Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Arrow,
                                      this->bodyPartsPos[PLAYER_BODYPART_WAIST].x,
                                      this->bodyPartsPos[PLAYER_BODYPART_WAIST].y,
                                      this->bodyPartsPos[PLAYER_BODYPART_WAIST].z, -1, 0, 0, MM_ARROW_TYPE_DEKU_NUT);
                if (this->zoraBoomerangActor != NULL) {
                    this->zoraBoomerangActor->velocity.x = this->actor.velocity.x * 1.5f;
                    this->zoraBoomerangActor->velocity.z = this->actor.velocity.z * 1.5f;
                    Inventory_ChangeAmmo(ITEM_NUT, -1);
                    MergeShip_PlayMMSfxActor(this->zoraBoomerangActor, MM_NA_SE_PL_DEKUNUTS_DROP_BOMB);
                }
            }
        }

        if (this->actor.velocity.y < 0.0f) {
            if (sp76 != 0) {
                this->actor.minVelocityY = -0.38f;
                this->actor.gravity = -0.2f;
            } else {
                this->actor.minVelocityY = (this->unk_B86[1] * 0.0033f) + -20.0f;
                this->actor.gravity = (this->unk_B86[1] * 0.00004f) + (REG(68) / 100.0f);
            }
        }

        this->fallStartHeight = this->actor.world.pos.y;
        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

        if (speedTarget == 0.0f) {
            sp68 = 0.1f;
        } else {
            s16 temp_v0_6 = this->yaw - yawTarget;

            if (ABS(temp_v0_6) > 0x4000) {
                speedTarget = -speedTarget;
                yawTarget += 0x8000;
            }
            sp68 = 0.25f;
        }

        Math_SmoothStepToS(&this->unk_B8C, speedTarget * 600.0f, 8, 0xFA0, 0x64);
        Math_ScaledStepToS(&this->yaw, yawTarget, 0xFA);

        temp_ft0 = (s16)(yawTarget - this->yaw) * -2.0f;
        temp_ft0 = CLAMP(temp_ft0, -0x1F40, 0x1F40);
        Math_SmoothStepToS(&this->unk_B8E, temp_ft0, 0x14, 0x320, 0x14);
        speedTarget = (speedTarget * (this->unk_B86[1] * 0.0004f)) * fabsf(Math_SinS(this->unk_B8C));
        MergeShip_Deku_func_80856888(&this->linearVelocity, speedTarget, sp68);

        speedTarget = sqrtf(SQ(this->linearVelocity) + SQ(this->actor.velocity.y));
        if (speedTarget > 8.0f) {
            speedTarget = 8.0f / speedTarget;
            this->linearVelocity *= speedTarget;
            this->actor.velocity.y *= speedTarget;
        }
    }

    MergeShip_Deku_func_808378FC(play, this);
}

// ---- B: the magic bubble ----------------------------------------------------------------------------------

// MM Player_InitBowOrDekuNutIA, Deku nut path.
void MergeShip_Deku_InitBubbleIA(PlayState* play, Player* this) {
    this->stateFlags1 |= PLAYER_STATE1_ITEM_IN_HAND;
    this->unk_860 = -2;
    this->unk_834 = 0;
}

// MM func_808305BC, Deku path: the bubble needs 2 magic (MM's Honey & Darling exception has no Hyrule scene).
static s32 MergeShip_Deku_func_808305BC(void) {
    return (gSaveContext.magic >= MM_ARROW_MAGIC_DEKU_BUBBLE_COST) ? 1 : 0;
}

// MM func_808306F8, Deku path: start blowing a bubble.
static s32 MergeShip_Deku_func_808306F8(Player* this, PlayState* play) {
    Player_SetUpperActionFunc(this, MergeShip_Deku_UpperAction_7);

    this->stateFlags1 |= PLAYER_STATE1_READY_TO_FIRE;
    this->unk_834 = 14;

    if (this->unk_860 >= 0) {
        // (MM's draw sfx for ABS(unk_B28) == 2, the Deku, is NA_SE_NONE.)
        if (MergeShip_Deku_func_808305BC() > 0) {
            if (this->unk_860 >= 0) {
                MergeShip_Deku_EnsureActorsRegistered();
                if (gMergeShipActorId_MM_En_Arrow >= 0) {
                    this->heldActor = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play,
                                                         gMergeShipActorId_MM_En_Arrow, this->actor.world.pos.x,
                                                         this->actor.world.pos.y, this->actor.world.pos.z, 0,
                                                         this->actor.shape.rot.y, 0, MM_ARROW_TYPE_DEKU_BUBBLE);
                }

                if (this->heldActor != NULL) {
                    // MM Magic_Consume(play, 2, MAGIC_CONSUME_NOW).
                    Magic_RequestChange(play, MM_ARROW_MAGIC_DEKU_BUBBLE_COST, MAGIC_CONSUME_NOW);
                }
            }
        }
    }

    return true;
}

// MM func_80830E30, Deku path.
static s32 MergeShip_Deku_func_80830E30(Player* this, PlayState* play) {
    if (!MergeShip_Deku_func_808306F8(this, play)) {
        return false;
    }

    LinkAnimation_PlayOnce(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahakidf));

    if (this->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        Player_AnimPlayLoop(play, this, DEKU_ANIM(link_uma_anim_walk));
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Player_AnimPlayLoop(play, this, Player_GetIdleAnim(this));
    }

    return true;
}

// MM func_80831010: MM's PLAYER_UNKAA5_3 (aiming) is Ocarina of Time's unk_6AD == 2; MM CAM_MODE_BOWARROW is
// Ocarina of Time's mode 7 (as OoT's func_80834EB8).
static s32 MergeShip_Deku_func_80831010(Player* this, PlayState* play) {
    if ((this->unk_6AD == 0) || (this->unk_6AD == 2)) {
        if (Player_IsZTargeting(this) || (this->focusActor != NULL) ||
            (Camera_CheckValidMode(Play_GetCamera(play, CAM_ID_MAIN), 7) == 0)) {
            return true;
        }
        this->unk_6AD = 2;
    }
    return false;
}

// MM func_80831094
static s32 MergeShip_Deku_func_80831094(Player* this, PlayState* play) {
    if ((this->doorType == PLAYER_DOORTYPE_NONE) && !(this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN)) {
        if (sUseHeldItem || func_80834E44(play)) {
            if (MergeShip_Deku_func_80830E30(this, play)) {
                return MergeShip_Deku_func_80831010(this, play);
            }
        }
    }
    return false;
}

// MM func_80831194, Deku path: release the bubble (it costs magic, not ammo).
static s32 MergeShip_Deku_func_80831194(PlayState* play, Player* this) {
    if (this->heldActor != NULL) {
        if (play->shootingGalleryStatus == 1) {
            play->shootingGalleryStatus = -10;
        }

        Player_RequestRumble(this, 150, 10, 150, 0);

        this->unk_A73 = 20; // MM unk_D57 = 20 for the Deku
        this->heldActor->parent = NULL;
        this->actor.child = NULL;
        this->heldActor = NULL;
        return true;
    }

    return false;
}

// MM Player_UpperAction_6, Deku path (MM func_80830B88 never shields for the Deku).
s32 MergeShip_Deku_UpperAction_6(Player* this, PlayState* play) {
    if (this->unk_860 >= 0) {
        this->unk_860 = -this->unk_860;
    }

    if (!MergeShip_Deku_func_80831094(this, play)) {
        return false;
    }
    return true;
}

// MM Player_UpperAction_7, Deku path (index 2).
s32 MergeShip_Deku_UpperAction_7(Player* this, PlayState* play) {
    if ((this->unk_836 == 0) && (Player_CheckForIdleAnim(this) == 0) &&
        DEKU_ANIM_IS(this->skelAnime.animation, link_bow_side_walk)) {
        LinkAnimation_PlayOnce(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahakidf));
        this->unk_836 = -1;
    } else if (LinkAnimation_Update(play, &this->upperSkelAnime)) {
        LinkAnimation_PlayLoop(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahakidf));
        this->unk_836 = 1;
    } else if (this->unk_836 == 1) {
        this->unk_836 = 2;
    }

    if (this->unk_834 >= 0xB) {
        this->unk_834--;
    }

    MergeShip_Deku_func_80831010(this, play);
    if ((this->unk_836 > 0) && ((this->unk_860 < 0) || (!sHeldItemButtonIsHeldDown && !func_80834E7C(play)))) {
        Player_SetUpperActionFunc(this, MergeShip_Deku_UpperAction_8);
        if (this->unk_860 >= 0) {
            if (!MergeShip_Deku_func_80831194(play, this)) {
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_DEKUNUTS_MISS_FIRE);
            }

            LinkAnimation_PlayOnceSetSpeed(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahaki), 2.0f / 3.0f);
        }
        this->unk_834 = 0xA;
        Player_ZeroSpeedXZ(this);
    } else {
        this->stateFlags1 |= PLAYER_STATE1_READY_TO_FIRE;
    }

    return true;
}

// MM Player_UpperAction_8, Deku path.
s32 MergeShip_Deku_UpperAction_8(Player* this, PlayState* play) {
    s32 animFinished = LinkAnimation_Update(play, &this->upperSkelAnime);

    if ((((this->unk_860 < 0) && sHeldItemButtonIsHeldDown) || (animFinished && sUseHeldItem)) ||
        func_80834E44(play)) {
        this->unk_860 = ABS(this->unk_860);
        if (MergeShip_Deku_func_808306F8(this, play)) {
            LinkAnimation_PlayOnce(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahakidf));
        }
    } else {
        if (this->unk_834 != 0) {
            this->unk_834--;
        }

        if (Player_IsZTargeting(this) || (this->unk_6AD != 0) || (this->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
            if (this->unk_834 == 0) {
                this->unk_834++;
            }
            return true;
        }

        Player_SetUpperActionFunc(this, MergeShip_Deku_UpperAction_9);
        LinkAnimation_PlayOnce(play, &this->upperSkelAnime, DEKU_ANIM(pn_tamahakidf));
        this->unk_834 = 0;
    }

    return true;
}

// MM Player_UpperAction_9
s32 MergeShip_Deku_UpperAction_9(Player* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || LinkAnimation_Update(play, &this->upperSkelAnime)) {
        Player_SetUpperActionFunc(this, MergeShip_Deku_UpperAction_6);
    }
    return true;
}

// MM func_8083868C, Deku aiming: CAM_MODE_DEKUSHOOT, requested instead of Ocarina of Time's aim mode.
s32 MergeShip_Deku_RequestAimCamera(PlayState* play, Player* this) {
    MergeShip_Camera_RequestMMMode(play, MM_CAM_MODE_DEKUSHOOT);
    return true;
}

// ---- per-frame hooks --------------------------------------------------------------------------------------

// MM Player_SetAction's parts that concern the Deku.
void MergeShip_Deku_OnSetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc) {
    s32 i;

    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return;
    }

    if (MergeShip_Deku_Action_93 == this->actionFunc) {
        this->actor.shape.shadowDraw = ActorShadow_DrawFeet;
        this->actor.shape.shadowScale = this->ageProperties->shadowScale;
        this->unk_ABC = 0.0f;
        this->actor.shape.yOffset = 0.0f;
        Actor_SetScale(&this->actor, 0.01f);
    }

    this->stateFlags3 &= ~(PLAYER_STATE3_200 | PLAYER_STATE3_2000 | PLAYER_STATE3_40000 | PLAYER_STATE3_200000 |
                           PLAYER_STATE3_1000000);
    this->unk_B86[0] = 0;
    this->unk_B86[1] = 0;
    this->unk_B8A = 0;
    this->unk_B8C = 0;
    this->unk_B8E = 0;
    for (i = 0; i < ARRAY_COUNT(this->unk_B10); i++) {
        this->unk_B10[i] = 0.0f;
    }
    this->actor.shape.rot.z = 0;
    MergeShip_Deku_ResetCylinder(this);
}

void MergeShip_Deku_UpdatePreAction(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return;
    }

    MergeShip_Deku_InitShieldCylinder(play, this);

    // MM Player_UpdateCommon clears these every frame before the action runs.
    this->stateFlags3 &= ~(PLAYER_STATE3_100 | PLAYER_STATE3_100000 | PLAYER_STATE3_10000000);
    // (MM's Deku B button rules -- nuts with magic, FD without, nuts while gliding -- are the HUD port's,
    // MergeShip_UpdateButtonsMM.)
}

// MM Player_UpdateCommon's collider block for the Deku: the guard's shield cylinder (PLAYER_STATE1_400000 Goron/
// Deku branch), the attack cylinder's AT, and no collisions while burrowed.
s32 MergeShip_Deku_UpdateColliders(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return false;
    }

    MergeShip_Deku_InitShieldCylinder(play, this);
    // MM resets these at the end of Player_UpdateCommon; their last hits were read this frame already.
    Collider_ResetCylinderAC(play, &this->shieldCylinder.base);
    Collider_ResetCylinderAT(play, &this->shieldCylinder.base);

    if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        this->shieldCylinder.base.acFlags = AC_ON | AC_HARD | AC_TYPE_ENEMY;
        this->shieldCylinder.info.toucher.dmgFlags = DMG_SHIELD;
        this->shieldCylinder.info.toucherFlags = TOUCH_NONE;
        this->shieldCylinder.info.bumperFlags = BUMP_ON;
        this->shieldCylinder.dim.height = 30;
        this->shieldCylinder.dim.radius = 20;
        this->shieldCylinder.dim.yShift = 0;
        Collider_UpdateCylinder(&this->actor, &this->shieldCylinder);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->shieldCylinder.base);
        this->cylinder.dim.yShift = 0;
        this->cylinder.dim.height = this->shieldCylinder.dim.height;
        Collider_UpdateCylinder(&this->actor, &this->cylinder);
    }

    if (!(this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                               PLAYER_STATE1_ON_HORSE)) &&
        !(this->stateFlags3 & PLAYER_STATE3_10000000)) {
        if ((MergeShip_Deku_Action_93 != this->actionFunc) && (Player_Action_SlideOnSlope != this->actionFunc) &&
            (this->actor.draw != NULL)) {
            CollisionCheck_SetOC(play, &play->colChkCtx, &this->cylinder.base);
        }
    }
    if (!(this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_DAMAGED)) && (this->invincibilityTimer <= 0)) {
        if (MergeShip_Deku_Action_93 != this->actionFunc) {
            if (this->cylinder.base.atFlags != AT_NONE) {
                CollisionCheck_SetAT(play, &play->colChkCtx, &this->cylinder.base);
            }
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->cylinder.base);
        }
    }

    return true;
}

void MergeShip_Deku_UpdatePostCommon(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return;
    }

    // MM: shape.yOffset = unk_ABC + unk_AC0 every frame. Ocarina of Time uses yOffset for its own ledge climbs,
    // and the Deku only moves unk_ABC inside the flower, so it is applied there.
    if (MergeShip_Deku_Action_93 == this->actionFunc) {
        this->actor.shape.yOffset = this->unk_ABC;
    }
}

// MM Player_UpdateCamAndSeqModes: the Deku's MM-only modes, at MM's priority (after the hookshot, push/pull,
// lock-on and spin charge, which Ocarina of Time's chain has already chosen).
s32 MergeShip_Deku_UpdateCamMode(PlayState* play, Player* this, s32* camMode) {
    if ((this->mergeShipForm != PLAYER_FORM_DEKU) || (this->focusActor != NULL) ||
        (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) ||
        (this->stateFlags2 & PLAYER_STATE2_GRABBING_DYNAPOLY) ||
        ((this->actor.parent != NULL) && (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT)) ||
        (Player_Action_8084377C == this->actionFunc)) {
        return false;
    }

    if (this->stateFlags3 & PLAYER_STATE3_100) {
        MergeShip_Camera_RequestMMMode(play, MM_CAM_MODE_DEKUHIDE);
        return true;
    }

    if (this->stateFlags1 &
        (PLAYER_STATE1_BOOMERANG_THROWN | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE)) {
        return false;
    }

    if ((this->stateFlags3 & PLAYER_STATE3_2000) && (this->actor.velocity.y < 0.0f)) {
        MergeShip_Camera_RequestMMMode(play, (this->stateFlags1 & (PLAYER_STATE1_PARALLEL |
                                                                   PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE))
                                                 ? MM_CAM_MODE_DEKUFLYZ
                                                 : MM_CAM_MODE_DEKUFLY);
        return true;
    }

    // MM: a Goron or Deku holding its guard keeps the camera still.
    if (!(this->stateFlags1 & (PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE)) &&
        (this->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        *camMode = CAM_MODE_STILL;
    }

    return false;
}

// ---- drawing ------------------------------------------------------------------------------------------------

typedef struct {
    s16 frame;
    Vec3s scale;
} MergeShipDekuFlowerKey;

// MM func_80124618 (z_player_lib.c).
static void MergeShip_Deku_func_80124618(MergeShipDekuFlowerKey arg0[], f32 curFrame, Vec3f* arg2) {
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

// MM Player_Draw's flower: while burrowed (PLAYER_STATE3_100) the flower closes over the Deku, and after a launch
// from scene geometry (PLAYER_STATE3_40000) it is drawn where the Deku left it. Returns true when the Deku is fully
// burrowed and must not be drawn.
s32 MergeShip_Deku_DrawFlower(PlayState* play, Player* this) {
    static MergeShipDekuFlowerKey D_8085D428[] = {
        { 0, { 0, 0, 0 } }, { 1, { 80, 170, 80 } }, { 3, { 100, 80, 100 } }, { 7, { 100, 100, 100 } },
        { 8, { 100, 100, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D450[] = {
        { 0, { 0, 0, 0 } }, { 1, { 80, 170, 80 } }, { 3, { 100, 80, 100 } }, { 7, { 100, 100, 100 } },
        { 8, { 100, 100, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D478[] = {
        { 0, { 0, 0, 0 } },
        { 8, { 0, 0, 0 } },
    };
    static MergeShipDekuFlowerKey D_8085D488[] = {
        { 0, { 100, 100, 100 } }, { 1, { 100, 60, 100 } }, { 3, { 100, 140, 100 } }, { 7, { 100, 80, 100 } },
        { 9, { 100, 100, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D4B0[] = {
        { 0, { 100, 100, 100 } }, { 1, { 100, 70, 100 } },  { 3, { 100, 120, 100 } },
        { 6, { 100, 80, 100 } },  { 8, { 100, 100, 100 } }, { 9, { 100, 100, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D4E0[] = {
        { 0, { 0, 0, 0 } },       { 1, { 0, 0, 0 } },    { 3, { 100, 130, 100 } },
        { 5, { 130, 130, 130 } }, { 7, { 80, 90, 80 } }, { 9, { 100, 100, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D510[] = {
        { 0, { 0, 50, 0 } },
        { 1, { 0, 50, 0 } },
    };
    static MergeShipDekuFlowerKey D_8085D520[] = {
        { 0, { 100, 120, 100 } },
        { 1, { 100, 120, 100 } },
    };
    static MergeShipDekuFlowerKey D_8085D530[] = {
        { 0, { 160, 120, 160 } },
        { 1, { 160, 120, 160 } },
    };
    static MergeShipDekuFlowerKey* D_8085D550[3] = { D_8085D488, D_8085D4B0, D_8085D4E0 };
    static MergeShipDekuFlowerKey* D_8085D55C[3] = { D_8085D428, D_8085D450, D_8085D478 };
    static MergeShipDekuFlowerKey* D_8085D568[3] = { D_8085D510, D_8085D520, D_8085D530 };
    static const char* D_8085D574[] = {
        mm_object_link_nuts_DL_009C48,
        mm_object_link_nuts_DL_009AB8,
        mm_object_link_nuts_DL_009DB8,
    };

    if (this->mergeShipForm != PLAYER_FORM_DEKU) {
        return false;
    }

    if (this->stateFlags3 & (PLAYER_STATE3_100 | PLAYER_STATE3_40000)) {
        MergeShipDekuFlowerKey** spE8 = D_8085D550;
        MergeShipDekuFlowerKey** spE4;
        f32 spE0;
        s32 i;

        OPEN_DISPS(play->state.gfxCtx);

        MS_INTERP_PUSH();
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        if (this->stateFlags3 & PLAYER_STATE3_40000) {
            static Vec3s sZeroRot = { 0, 0, 0 };

            Matrix_SetTranslateRotateYXZ(this->unk_AF0[0].x, this->unk_AF0[0].y, this->unk_AF0[0].z, &sZeroRot);
            Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
            spE8 = D_8085D568;
            spE0 = 0.0f;
        } else {
            Matrix_Translate(0.0f, -this->unk_ABC, 0.0f, MTXMODE_APPLY);
            spE0 = this->av2.actionVar2 - 6;
            if (spE0 < 0.0f) {
                spE8 = D_8085D55C;
                spE0 = this->unk_B86[0];
            }
        }

        spE4 = spE8;

        for (i = 0; i < 3; i++, spE4++) {
            MS_INTERP_PUSH();
            MergeShip_Deku_func_80124618(*spE4, spE0, &this->unk_AF0[1]);
            Matrix_Scale(this->unk_AF0[1].x, this->unk_AF0[1].y, this->unk_AF0[1].z, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)D_8085D574[i]);
            MS_INTERP_POP();
        }
        MS_INTERP_POP();

        CLOSE_DISPS(play->state.gfxCtx);
    }

    return !(this->unk_ABC > -3900.0f);
}
