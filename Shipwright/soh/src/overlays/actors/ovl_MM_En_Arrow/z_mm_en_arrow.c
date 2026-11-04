/*
 * File: z_mm_en_arrow.c
 * Overlay: ovl_MM_En_Arrow
 * Description: Majora's Mask's En_Arrow, for the Deku's bubble and dropped Deku nuts
 *
 * MergeShip2 -- ported from 2ship2harkinian/mm/src/overlays/actors/ovl_En_Arrow/z_en_arrow.c. MM's arrow and
 * magic-arrow types (skeleton, blure trail, elemental child actors) are not ported: the human form keeps
 * Ocarina of Time's bow. The slingshot, Deku bubble and Deku nut types are MM's code.
 *
 * Differences forced by the host game, each marked where it happens:
 * - Damage flags: Ocarina of Time's enemies read Ocarina of Time's DMG_ bits. MM's DMG_DEKU_BUBBLE (1 damage
 *   for nearly every MM enemy) becomes DMG_SLINGSHOT (1 damage for nearly every Hyrule enemy); DMG_DEKU_NUT is
 *   the same bit in both games.
 * - The Deku nut's flash is Ocarina of Time's En_M_Fire1 (the same actor MM spawns) and iREG(50) flash.
 * - MM's EFFECT_SS_SBN (the bubble's splat left on walls) has no Ocarina of Time counterpart and is not drawn.
 * - player->unk_D57 (MM's "just fired" timer) is Ocarina of Time's unk_A73, which OoT's player decrements.
 */

#include "z_mm_en_arrow.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
#include "soh/framebuffer_effects.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

#define MM_ARROW_IS_ARROW(arrowType) ((arrowType) < MM_ARROW_TYPE_SLINGSHOT)

s16 gMergeShipActorId_MM_En_Arrow = -1;

static void func_8088A594_MM(MMEnArrow* this, PlayState* play);
static void func_8088ACE0_MM(MMEnArrow* this, PlayState* play);

static ColliderQuadInit sQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000020, 0x00, 0x02 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(minVelocityY, -150, ICHAIN_STOP),
};

// MM Actor_GetArrowDmgFlags, in Ocarina of Time's damage bits (see the file comment).
static u32 MMEnArrow_GetDmgFlags(s32 params) {
    switch (params) {
        case MM_ARROW_TYPE_DEKU_NUT:
            return DMG_DEKU_NUT;
        case MM_ARROW_TYPE_DEKU_BUBBLE: // MM DMG_DEKU_BUBBLE
        case MM_ARROW_TYPE_SLINGSHOT:
        default:
            return DMG_SLINGSHOT;
    }
}

// MM Actor_SetSpeeds.
static void MMEnArrow_SetSpeeds(Actor* actor, f32 speed) {
    actor->speedXZ = Math_CosS(actor->world.rot.x) * speed;
    actor->velocity.y = -Math_SinS(actor->world.rot.x) * speed;
}

void MMEnArrow_Init(Actor* thisx, PlayState* play) {
    MMEnArrow* this = (MMEnArrow*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    if (this->actor.params == -MM_ARROW_TYPE_DEKU_NUT) {
        this->unk_263 = 1;
        this->actor.params = MM_ARROW_TYPE_DEKU_NUT;
    }

    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sQuadInit);

    if (this->actor.params < MM_ARROW_TYPE_NORMAL_LIT) {
        this->collider.base.atFlags = (AT_TYPE_ENEMY | AT_ON);
    } else {
        this->collider.info.toucher.dmgFlags = MMEnArrow_GetDmgFlags(this->actor.params);
        if (this->actor.params == MM_ARROW_TYPE_DEKU_NUT) {
            this->collider.info.toucher.damage = 1;
        }

        if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
            Actor_SetScale(&this->actor, 1.0f);
        }
    }
    this->actionFunc = func_8088A594_MM;
}

void MMEnArrow_Destroy(Actor* thisx, PlayState* play) {
    MMEnArrow* this = (MMEnArrow*)thisx;

    Collider_DestroyQuad(play, &this->collider);

    if ((this->unk_264 != NULL) && (this->unk_264->update != NULL)) {
        this->unk_264->flags &= ~ACTOR_FLAG_ATTACHED_TO_ARROW;
    }
}

// MM func_8088A514
static void func_8088A514_MM(MMEnArrow* this) {
    f32 temp_f0 = 16.0f - this->bubble.unk_144;

    MMEnArrow_SetSpeeds(&this->actor, CLAMP(temp_f0, 1.0f, 80.0f));
}

// MM func_8088A594: held (the bubble grows while Deku Link blows it), then released.
static void func_8088A594_MM(MMEnArrow* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->actor.parent != NULL) {
        if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
            if (Math_SmoothStepToF(&this->bubble.unk_144, 16.0f, 0.07f, 1.8f, 0.0f) > 0.5f) {
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_DEKUNUTS_BUBLE_BREATH - SFX_FLAG);
                return;
            }

            this->bubble.unk_148++;
            if (this->bubble.unk_148 > 20) {
                this->actionFunc = func_8088ACE0_MM;
                Magic_Reset(play);
            }
        }
    } else {
        // MM player->unk_D57 == OoT player->unk_A73.
        if ((this->actor.params != MM_ARROW_TYPE_DEKU_NUT) && (player->unk_A73 == 0)) {
            if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
                Magic_Reset(play);
            }
            Actor_Kill(&this->actor);
            return;
        }

        switch (this->actor.params) {
            case MM_ARROW_TYPE_SLINGSHOT:
                MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_IT_SLING_SHOT);
                break;

            case MM_ARROW_TYPE_DEKU_BUBBLE:
                MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_PL_DEKUNUTS_FIRE);
                break;

            default:
                break;
        }

        this->actionFunc = func_8088ACE0_MM;
        Math_Vec3f_Copy(&this->unk_228, &this->actor.world.pos);

        if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
            this->bubble.unk_144 = CLAMP_MIN(this->bubble.unk_144, 3.5f);
            func_8088A514_MM(this);
            this->unk_260 = 99;
            Magic_Reset(play);
        } else if (!MM_ARROW_IS_ARROW(this->actor.params)) {
            if ((this->actor.params == MM_ARROW_TYPE_DEKU_NUT) && (this->actor.world.rot.x < 0)) {
                Actor_SetScale(&this->actor, 0.009f);
                this->unk_260 = 40;
            } else {
                MMEnArrow_SetSpeeds(&this->actor, 80.0f);
                this->unk_260 = 15;
            }
            this->actor.shape.rot.x = 0;
            this->actor.shape.rot.y = 0;
            this->actor.shape.rot.z = 0;
        } else {
            MMEnArrow_SetSpeeds(&this->actor, 150.0f);
            this->unk_260 = 16;
        }
    }
}

// MM func_8088AA98: splash on entering water.
static void func_8088AA98_MM(MMEnArrow* this, PlayState* play) {
    WaterBox* waterBox;
    f32 sp50 = this->actor.world.pos.y;
    Vec3f sp44;
    f32 temp_f0;

    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &sp50, &waterBox) &&
        (this->actor.world.pos.y < sp50) && !(this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
        this->actor.bgCheckFlags |= BGCHECKFLAG_WATER;

        Math_Vec3f_Diff(&this->actor.world.pos, &this->actor.home.pos, &sp44);

        if (sp44.y != 0.0f) {
            temp_f0 = sqrtf(SQ(sp44.x) + SQ(sp44.z));
            if (temp_f0 != 0.0f) {
                temp_f0 = (((sp50 - this->actor.home.pos.y) / sp44.y) * temp_f0) / temp_f0;
            }
            sp44.x = this->actor.home.pos.x + (sp44.x * temp_f0);
            sp44.y = sp50;
            sp44.z = this->actor.home.pos.z + (sp44.z * temp_f0);
            EffectSsGSplash_Spawn(play, &sp44, NULL, NULL, 0, 300);
        }

        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER_L);

        EffectSsGRipple_Spawn(play, &sp44, 100, 500, 0);
        EffectSsGRipple_Spawn(play, &sp44, 100, 500, 4);
        EffectSsGRipple_Spawn(play, &sp44, 100, 500, 8);
    }
}

// MM func_8088ACE0: flying.
static void func_8088ACE0_MM(MMEnArrow* this, PlayState* play) {
    CollisionPoly* spAC;
    s32 bgId;
    Vec3f sp9C;
    s32 phi_a2 = 0;
    u16 sp82;
    f32 sp78;
    f32 sp74;
    f32 temp_f12_2;
    s32 sp50;

    if ((DECR(this->unk_260) == 0) ||
        ((this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) &&
         ((this->unk_262 != 0) || (phi_a2 = (this->collider.base.atFlags & AT_HIT) != 0)))) {
        if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
            if (phi_a2 && (this->collider.info.atHitInfo != NULL) &&
                (this->collider.info.atHitInfo->elemType != ELEMTYPE_UNK4) &&
                (this->collider.base.atFlags & AT_BOUNCED)) {
                if ((this->collider.base.at != NULL) && (this->collider.base.at->id != ACTOR_OBJ_SYOKUDAI)) {
                    Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
                    this->actor.world.rot.y += (s16)(0x8000 + (s16)(s32)Rand_CenteredFloat(0x1F40));
                    this->actor.velocity.y = -this->actor.velocity.y;
                    this->bubble.unk_149 = -1;
                    return;
                }
            }

            if ((this->unk_262 != 0) || phi_a2) {
                EffectSsStone1_Spawn(play, &this->actor.world.pos, 0);
            }

            MergeShip_PlayMMSfxAtPos(&this->actor.world.pos, MM_NA_SE_IT_DEKUNUTS_BUBLE_VANISH);
            // MM spawns EFFECT_SS_SBN here when the bubble burst on a scene wall; Ocarina of Time has no such
            // effect.
        }
        Actor_Kill(&this->actor);
        return;
    }

    sp50 = (this->actor.params != MM_ARROW_TYPE_NORMAL_LIT) && (this->actor.params < MM_ARROW_TYPE_DEKU_NUT) &&
           (this->collider.base.atFlags & AT_HIT);

    if (sp50 || (this->unk_262 != 0)) {
        if (!MM_ARROW_IS_ARROW(this->actor.params)) {
            if (sp50) {
                this->actor.world.pos.x = (this->actor.world.pos.x + this->actor.prevPos.x) * 0.5f;
                this->actor.world.pos.y = (this->actor.world.pos.y + this->actor.prevPos.y) * 0.5f;
                this->actor.world.pos.z = (this->actor.world.pos.z + this->actor.prevPos.z) * 0.5f;
            }

            if (this->actor.params == MM_ARROW_TYPE_DEKU_NUT) {
                // MM R_TRANS_FADE_FLASH_ALPHA_STEP is Ocarina of Time's iREG(50).
                iREG(50) = -1;
                Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_FIRE1, this->actor.world.pos.x, this->actor.world.pos.y,
                            this->actor.world.pos.z, 0, 0, 0, this->actor.speedXZ == 0.0f);
                sp82 = MM_NA_SE_IT_DEKU;
            } else {
                sp82 = MM_NA_SE_IT_SLING_REFLECT;
            }
            EffectSsStone1_Spawn(play, &this->actor.world.pos, 0);
            MergeShip_PlayMMSfxAtPos(&this->actor.world.pos, sp82);
            Actor_Kill(&this->actor);
        }
    } else {
        func_8088AA98_MM(this, play);
        if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
            if (this->bubble.unk_149 == 0) {
                sp78 = sqrtf(SQ(this->actor.speedXZ) + SQ(this->actor.velocity.y));
                sp74 = Math_SinS(this->actor.world.rot.y) * this->actor.speedXZ;
                temp_f12_2 = Math_CosS(this->actor.world.rot.y) * this->actor.speedXZ;

                this->actor.prevPos.x = this->actor.world.pos.x - (sp74 * (10.0f / sp78));
                this->actor.prevPos.y = this->actor.world.pos.y - (this->actor.velocity.y * (10.0f / sp78));
                this->actor.prevPos.z = this->actor.world.pos.z - (temp_f12_2 * (10.0f / sp78));

                this->bubble.unk_149 = 1;
            }

            if (Math_StepToF(&this->bubble.unk_144, 1.0f, 0.4f)) {
                this->unk_260 = 0;
            } else {
                this->bubble.unk_14A += (s16)(s32)(this->bubble.unk_144 * (500.0f + Rand_ZeroFloat(1400.0f)));
                this->actor.world.rot.x += (s16)(s32)(500.0f * Math_SinS(this->bubble.unk_14A));
                this->actor.shape.rot.x = this->actor.world.rot.x;

                this->bubble.unk_14C += (s16)(s32)(this->bubble.unk_144 * (500.0f + Rand_ZeroFloat(1400.0f)));
                this->actor.world.rot.y += (s16)(s32)(500.0f * Math_SinS(this->bubble.unk_14C));

                this->actor.shape.rot.y = this->actor.world.rot.y;

                func_8088A514_MM(this);
            }
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_DEKUNUTS_BUBLE_SHOT_LEVEL - SFX_FLAG);
        } else if (this->unk_260 < 7) {
            this->actor.gravity = -0.4f;
        }

        Math_Vec3f_Copy(&this->unk_228, &this->actor.world.pos);

        if (this->actor.speedXZ == 0.0f) {
            this->actor.velocity.y -= 1.0f;
            if (this->actor.velocity.y < this->actor.minVelocityY) {
                this->actor.velocity.y = this->actor.minVelocityY;
            }
            Actor_UpdatePos(&this->actor);
        } else {
            Actor_MoveXZGravity(&this->actor);
        }

        if ((this->unk_262 = BgCheck_ProjectileLineTest(&play->colCtx, &this->actor.prevPos, &this->actor.world.pos,
                                                        &sp9C, &this->actor.wallPoly, true, true, true, true, &bgId))) {
            // MM func_800B90AC; Ocarina of Time's counterpart also lets projectiles trigger its eye switches.
            func_8002F9EC(play, &this->actor, this->actor.wallPoly, bgId, &sp9C);
            Math_Vec3f_Copy(&this->actor.world.pos, &sp9C);
            this->actor.wallBgId = bgId;
        }
    }

    if (this->unk_264 != NULL) {
        if (this->unk_264->update != NULL) {
            Vec3f sp60;
            Vec3f sp54;

            Math_Vec3f_Sum(&this->unk_228, &this->unk_268, &sp60);
            Math_Vec3f_Sum(&this->actor.world.pos, &this->unk_268, &sp54);

            if (BgCheck_EntityLineTest1(&play->colCtx, &sp60, &sp54, &sp9C, &spAC, true, true, true, true, &bgId)) {
                this->unk_264->world.pos.x = ((sp54.x <= sp9C.x) ? 1.0f : -1.0f) + sp9C.x;
                this->unk_264->world.pos.y = ((sp54.y <= sp9C.y) ? 1.0f : -1.0f) + sp9C.y;
                this->unk_264->world.pos.z = ((sp54.z <= sp9C.z) ? 1.0f : -1.0f) + sp9C.z;

                Math_Vec3f_Diff(&this->unk_264->world.pos, &this->actor.world.pos, &this->unk_268);
                this->unk_264->flags &= ~ACTOR_FLAG_ATTACHED_TO_ARROW;
                this->unk_264 = NULL;
            } else {
                Math_Vec3f_Sum(&this->actor.world.pos, &this->unk_268, &this->unk_264->world.pos);
            }

            if ((this->unk_262 != 0) && (this->unk_264 != NULL)) {
                this->unk_264->flags &= ~ACTOR_FLAG_ATTACHED_TO_ARROW;
                this->unk_264 = NULL;
            }
        } else {
            this->unk_264 = NULL;
        }
    }
}

void MMEnArrow_Update(Actor* thisx, PlayState* play) {
    MMEnArrow* this = (MMEnArrow*)thisx;
    Player* player = GET_PLAYER(play);

    if ((this->unk_263 != 0) ||
        ((this->actor.params >= MM_ARROW_TYPE_NORMAL_LIT) &&
         ((this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) || (player->unk_A73 != 0))) ||
        !Player_InBlockingCsMode(play, player)) {
        this->actionFunc(this, play);
    }

    Math_Vec3f_Copy(&this->actor.home.pos, &this->actor.prevPos);
}

typedef struct {
    /* 0x00 */ Vec3f unk_00[2];
    /* 0x18 */ Vec3f unk_18[2][2];
    /* 0x48 */ Vec3f unk_48;
} MMEnArrowUnkStruct; // size = 0x54

// MM func_8088B88C: moves the quad collider along the projectile.
static void func_8088B88C_MM(PlayState* play, MMEnArrow* this, MMEnArrowUnkStruct* arg2) {
    Vec3f* sp4C;
    Vec3f sp40;
    Vec3f sp34;

    Matrix_MultVec3f(&arg2->unk_48, &this->unk_234);
    if (func_8088ACE0_MM == this->actionFunc) {
        if (!this->unk_244.active) {
            sp4C = arg2->unk_00;
        } else {
            sp4C = arg2->unk_18[play->gameplayFrames % 2];
        }
        Matrix_MultVec3f(&sp4C[0], &sp40);
        Matrix_MultVec3f(&sp4C[1], &sp34);
        if (this->actor.params < MM_ARROW_TYPE_DEKU_NUT) {
            if (this->unk_264 == NULL) {
                // MM func_80126440 is Ocarina of Time's func_80090480.
                func_80090480(play, &this->collider, &this->unk_244, &sp40, &sp34);
            }
        }
    }
}

void MMEnArrow_Draw(Actor* thisx, PlayState* play) {
    static MMEnArrowUnkStruct D_8088C2EC[] = {
        {
            {
                { 400.0f, 400.0f, 0.0f },
                { -400.0f, 400.0f, 0.0f },
            },
            {
                {
                    { 400.0f, 400.0f, 1500.0f },
                    { -400.0f, 400.0f, 1500.0f },
                },
                {
                    { 400.0f, 400.0f, 1500.0f },
                    { -400.0f, 400.0f, 1500.0f },
                },
            },
            { 0.0f, 0.0f, -300.0f },
        },
        {
            {
                { 20.0f, -20.0f, 20.0f },
                { 20.0f, -20.0f, 20.0f },
            },
            {
                {
                    { 20.0f, -20.0f, 20.0f },
                    { -20.0f, 20.0f, 20.0f },
                },
                {
                    { -20.0f, -20.0f, 20.0f },
                    { 20.0f, 20.0f, 20.0f },
                },
            },
            { 0.0f, 0.0f, 20.0f },
        },
    };
    MMEnArrow* this = (MMEnArrow*)thisx;
    s32 phi_v0;

    if (this->actor.params == MM_ARROW_TYPE_DEKU_BUBBLE) {
        s32 spA4 = 255 - (s32)(this->bubble.unk_144 * 4.0f);
        f32 spA0 = (this->actor.speedXZ * 0.1f) + 1.0f;
        f32 sp9C = (1.0f / spA0);

        OPEN_DISPS(play->state.gfxCtx);

        if (this->bubble.unk_149 >= 0) {
            func_8088B88C_MM(play, this, &D_8088C2EC[1]);
        } else {
            Collider_ResetQuadAT(play, &this->collider.base);
        }

        sp9C *= 0.002f;
        spA0 *= 0.002f;

        Matrix_Scale(this->bubble.unk_144 * sp9C, this->bubble.unk_144 * sp9C, this->bubble.unk_144 * spA0,
                     MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, 460.0f, MTXMODE_APPLY);

        if (this->actor.speedXZ == 0.0f) {
            // MM func_800B8118.
            func_8002ED80(&this->actor, play, 0);

            // As 2S2H: capture the frame so the bubble's DL can refract it from segment 0x0F.
            {
                Gfx* gfx = POLY_XLU_DISP;
                FB_WriteFramebufferSliceToCPU(&gfx, play->state.gfxCtx->curFrameBuffer, true);
                POLY_XLU_DISP = gfx;
            }
            gSPInvalidateTexCache(POLY_XLU_DISP++,
                                  (uintptr_t)(play->state.gfxCtx->curFrameBuffer) + ((104 * 320 + 144) * 2));
            gSPSegment(POLY_XLU_DISP++, 0x0F, (uintptr_t)play->state.gfxCtx->curFrameBuffer);

            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gameplay_keep_DL_06F380);
            gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
            gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL1, 0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,
                              COMBINED, 0, ENVIRONMENT, 0, COMBINED, 0, ENVIRONMENT, 0);
            gDPSetEnvColor(POLY_XLU_DISP++, 230, 225, 150, spA4);

            Matrix_ReplaceRotation(&gMtxFClear);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            // MM D_01000000_TO_SEGMENTED: the billboard matrix.
            gSPMatrix(POLY_XLU_DISP++, play->billboardMtx, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gameplay_keep_DL_06F9F0);
        } else {
            // MM func_800B8050.
            func_8002EBCC(&this->actor, play, 0);

            {
                Gfx* gfx = POLY_OPA_DISP;
                FB_WriteFramebufferSliceToCPU(&gfx, play->state.gfxCtx->curFrameBuffer, true);
                POLY_OPA_DISP = gfx;
            }
            gSPInvalidateTexCache(POLY_OPA_DISP++,
                                  (uintptr_t)(play->state.gfxCtx->curFrameBuffer) + ((104 * 320 + 144) * 2));
            gSPSegment(POLY_OPA_DISP++, 0x0F, (uintptr_t)play->state.gfxCtx->curFrameBuffer);

            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gameplay_keep_DL_06F380);
            gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL1, 0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0,
                              COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x7F, 230, 225, 150, 255);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gameplay_keep_DL_06FAE0);
        }

        CLOSE_DISPS(play->state.gfxCtx);

        return;
    } else if (this->actor.speedXZ != 0.0f) {
        u8 sp63 = (Math_CosS(this->unk_260 * 5000) * 127.5f) + 127.5f;
        f32 sp5C;

        OPEN_DISPS(play->state.gfxCtx);

        Gfx_SetupDL_25Xlu2(play->state.gfxCtx);

        gSPClearGeometryMode(POLY_XLU_DISP++, G_FOG | G_LIGHTING);

        if (this->actor.params == MM_ARROW_TYPE_SLINGSHOT) {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, sp63);
            sp5C = 50.0f;
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 12, 0, 0, 255);
            gDPSetEnvColor(POLY_XLU_DISP++, 250, 250, 0, sp63);
            sp5C = 150.0f;
        }

        Matrix_Push();
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);

        if (this->actor.speedXZ == 0.0f) {
            phi_v0 = 0;
        } else {
            phi_v0 = (play->gameplayFrames % 256) * 4000;
        }

        Matrix_RotateZ(BINANG_TO_RAD((s16)phi_v0), MTXMODE_APPLY);
        Matrix_Scale(sp5C, sp5C, sp5C, MTXMODE_APPLY);

        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gEffSparklesDL);

        Matrix_Pop();
        Matrix_RotateY(BINANG_TO_RAD(this->actor.world.rot.y), MTXMODE_APPLY);

        CLOSE_DISPS(play->state.gfxCtx);
    } else if (this->actor.velocity.y != 0.0f) {
        OPEN_DISPS(play->state.gfxCtx);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gameplay_keep_DL_058BA0);

        CLOSE_DISPS(play->state.gfxCtx);
    }

    func_8088B88C_MM(play, this, &D_8088C2EC[0]);
}
