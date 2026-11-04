/*
 * MergeShip2: Ganondorf mode's magic -- see z_ms_ganon_magic.h. Values, timings and visuals are the bosses'; what
 * changes is who they fly at (what the player is locked on to, or straight ahead) and what they hurt (enemies:
 * AT_TYPE_PLAYER colliders, so never the player).
 */

#include "z_ms_ganon_magic.h"
#include "overlays/ovl_Boss_Ganon/ovl_Boss_Ganon.h"
#include "objects/object_fhg/object_fhg.h"
#include "soh/frame_interpolation.h"
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h"
#include "overlays/actors/ovl_Boss_Va/z_boss_va.h"
#include "overlays/actors/ovl_Boss_Ganondrof/z_boss_ganondrof.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

Vec3f gMsGanonMagicChargePos;
void MergeShipTest_Log(const char* fmt, ...);
s16 gMergeShipActorId_MS_Ganon_Magic = -1;

// BossGanon's sLightBallCylinderInit, turned on enemies: AT_TYPE_PLAYER, light arrows' damage, no AC (nothing plays
// tennis with the player's ball).
static ColliderCylinderInit sBallCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_ARROW_LIGHT, 0x00, 0x04 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 20, 40, -20, { 0, 0, 0 } },
};

// ---- the bosses' effects (BossGanonEff_*, BossGanon_UpdateEffects / _DrawEffects) ---------------------------------------

typedef enum {
    MS_EFF_NONE,
    MS_EFF_SPARKLE,
    MS_EFF_LIGHT_RAY,
    MS_EFF_IMPACT_DUST_LIGHT,
    MS_EFF_SHOCKWAVE,
    MS_EFF_BLACK_DOT,
    MS_EFF_FHG_LIGHT,
} MsGanonEffectType;

typedef struct {
    u8 type;
    u8 timer;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
    s16 alpha;
    s16 unk_2E;
    s16 unk_30;
    f32 scale;
    f32 unk_38;
    f32 unk_3C;
    f32 unk_40;
    f32 unk_44;
    f32 unk_48;
    u32 epoch;
} MsGanonEffect;

#define MS_EFFECT_COUNT 200
static MsGanonEffect sEffects[MS_EFFECT_COUNT];
static Vec3f sZero = { 0.0f, 0.0f, 0.0f };

static MsGanonEffect* MsGanonMagic_FreeEffect(s32 limit) {
    s32 i;

    for (i = 0; i < limit; i++) {
        if (sEffects[i].type == MS_EFF_NONE) {
            sEffects[i].epoch++;
            return &sEffects[i];
        }
    }
    return NULL;
}

void MsGanonMagic_SpawnSparkle(Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, s16 fade) {
    MsGanonEffect* eff = MsGanonMagic_FreeEffect(150);

    if (eff != NULL) {
        eff->type = MS_EFF_SPARKLE;
        eff->pos = *pos;
        eff->velocity = *velocity;
        eff->accel = *accel;
        eff->scale = scale / 1000.0f;
        eff->unk_2E = (s16)Rand_ZeroFloat(100.0f) + 0xC8;
        eff->unk_30 = fade;
        eff->timer = (s16)Rand_ZeroFloat(10.0f);
    }
}

void MsGanonMagic_SpawnLightRay(Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, f32 length, s16 fade) {
    MsGanonEffect* eff = MsGanonMagic_FreeEffect(150);

    if (eff != NULL) {
        eff->type = MS_EFF_LIGHT_RAY;
        eff->pos = *pos;
        eff->velocity = *velocity;
        eff->accel = *accel;
        eff->scale = scale / 1000.0f;
        eff->unk_38 = 1.0f;
        eff->unk_40 = length;
        eff->unk_2E = (s16)Rand_ZeroFloat(100.0f) + 0xC8;
        eff->unk_30 = fade;
        eff->timer = (s16)Rand_ZeroFloat(10.0f);
        eff->unk_48 = Math_Atan2F(eff->velocity.z, eff->velocity.x);
        eff->unk_44 = -Math_Atan2F(sqrtf(SQXZ(eff->velocity)), eff->velocity.y);
    }
}

void MsGanonMagic_SpawnDustLight(Vec3f* pos, f32 scale, f32 target, s16 slot) {
    MsGanonEffect* eff = &sEffects[slot];

    eff->type = MS_EFF_IMPACT_DUST_LIGHT;
    eff->pos = *pos;
    eff->velocity = sZero;
    eff->accel = sZero;
    eff->unk_40 = 1.0f;
    eff->scale = scale;
    eff->unk_38 = target;
    eff->unk_30 = Rand_ZeroFloat(100.0f);
    eff->unk_2E = eff->timer = eff->alpha = 0;
    eff->epoch++;
}

void MsGanonMagic_SpawnShockwave(Vec3f* pos, f32 scale, f32 target) {
    MsGanonEffect* eff = MsGanonMagic_FreeEffect(150);

    if (eff != NULL) {
        eff->type = MS_EFF_SHOCKWAVE;
        eff->pos = *pos;
        eff->velocity = sZero;
        eff->accel = sZero;
        eff->alpha = 255;
        eff->unk_40 = 0.6f;
        eff->scale = scale;
        eff->unk_38 = target;
        eff->unk_30 = (s16)Rand_ZeroFloat(100.0f);
        eff->unk_2E = eff->timer = 0;
    }
}

void MsGanonMagic_SpawnBlackDot(Vec3f* pos, f32 scale) {
    MsGanonEffect* eff = MsGanonMagic_FreeEffect(150);

    if (eff != NULL) {
        eff->type = MS_EFF_BLACK_DOT;
        eff->pos = *pos;
        eff->velocity = sZero;
        eff->accel = sZero;
        eff->unk_38 = 0.0f;
        eff->scale = scale / 1000.0f;
        eff->timer = 0;
        eff->alpha = 0;
        eff->unk_2E = 0;
    }
}

void MsGanonMagic_SpawnFhgLight(Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, u8 color) {
    MsGanonEffect* eff = MsGanonMagic_FreeEffect(MS_EFFECT_COUNT);

    // EffectSsFhgFlash_Init, FHGFLASH_LIGHTBALL
    if (eff != NULL) {
        eff->type = MS_EFF_FHG_LIGHT;
        eff->pos = *pos;
        eff->velocity = *velocity;
        eff->accel = *accel;
        eff->scale = scale;
        eff->alpha = 255;
        eff->unk_2E = color % 7;
        eff->unk_30 = 0;
        eff->timer = 0;
    }
}

s32 MsGanonMagic_IsMagic(Actor* actor) {
    MsGanonMagic* magic = (MsGanonMagic*)actor;

    return (actor != NULL) && (gMergeShipActorId_MS_Ganon_Magic >= 0) && (actor->id == gMergeShipActorId_MS_Ganon_Magic) &&
           (actor->update != NULL) && (magic->type != MS_GANON_MAGIC_BURST);
}

s32 MsGanonMagic_IsOrb(Actor* actor) {
    MsGanonMagic* magic = (MsGanonMagic*)actor;

    return (actor != NULL) && (gMergeShipActorId_MS_Ganon_Magic >= 0) && (actor->id == gMergeShipActorId_MS_Ganon_Magic) &&
           (actor->update != NULL) &&
           ((magic->type == MS_GANON_MAGIC_LIGHT_BALL) || (magic->type == MS_GANON_MAGIC_PHANTOM_ORB) ||
            ((magic->type == MS_GANON_MAGIC_BIG_MAGIC) && (magic->state == 0)));
}

void MsGanonMagic_UpdateEffects(PlayState* play) {
    MsGanonEffect* eff = sEffects;
    Vec3f spA0 = { 0.0f, 0.0f, 0.0f };
    f32 xDiff;
    f32 yDiff;
    f32 zDiff;
    s32 i;

    for (i = 0; i < MS_EFFECT_COUNT; i++, eff++) {
        if (eff->type == MS_EFF_NONE) {
            continue;
        }
        eff->pos.x += eff->velocity.x;
        eff->pos.y += eff->velocity.y;
        eff->pos.z += eff->velocity.z;
        eff->timer++;
        eff->velocity.x += eff->accel.x;
        eff->velocity.y += eff->accel.y;
        eff->velocity.z += eff->accel.z;

        if ((eff->type == MS_EFF_SPARKLE) || (eff->type == MS_EFF_LIGHT_RAY)) {
            eff->unk_3C += Rand_ZeroFloat(M_PI / 2) + M_PI / 2;
            eff->unk_2E -= eff->unk_30;
            if (eff->unk_2E <= 0) {
                eff->unk_2E = 0;
                eff->type = MS_EFF_NONE;
            }
            eff->alpha = MIN(eff->unk_2E, 255);
            if (eff->type == MS_EFF_LIGHT_RAY) {
                Math_ApproachF(&eff->unk_38, eff->unk_40, 1.0f, (eff->unk_40 / 15.0f) * 4.0f);
            }
        } else if (eff->type == MS_EFF_BLACK_DOT) {
            f32 yRot;
            f32 xRot;

            xDiff = gMsGanonMagicChargePos.x - eff->pos.x;
            yDiff = gMsGanonMagicChargePos.y - eff->pos.y;
            zDiff = gMsGanonMagicChargePos.z - eff->pos.z;
            yRot = Math_FAtan2F(xDiff, zDiff);
            xRot = -Math_FAtan2F(yDiff, sqrtf(SQ(xDiff) + SQ(zDiff)));
            spA0.z = eff->unk_38;
            Matrix_RotateY(yRot, MTXMODE_NEW);
            Matrix_RotateX(xRot, MTXMODE_APPLY);
            Matrix_MultVec3f(&spA0, &eff->velocity);
            Math_ApproachF(&eff->unk_38, 10.0f, 1.0f, 0.5f);
            eff->alpha = MIN(eff->alpha + 10, 255);
            if ((sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff)) < 20.0f) || (eff->timer > 70)) {
                eff->type = MS_EFF_NONE;
            }
        } else if (eff->type == MS_EFF_IMPACT_DUST_LIGHT) {
            if (i == 0) {
                Sfx_PlaySfxCentered(NA_SE_EN_GANON_WAVE_GND - SFX_FLAG);
            }
            if (eff->unk_2E == 0) {
                eff->alpha += 100;
                if (eff->alpha > 255) {
                    eff->alpha = 255;
                    eff->unk_2E = 1;
                }
            } else if (eff->unk_2E == 1) {
                if (eff->timer >= 20) {
                    eff->unk_2E = 2;
                }
            } else {
                eff->alpha -= 30;
                if (eff->alpha < 0) {
                    eff->alpha = 0;
                    eff->type = MS_EFF_NONE;
                }
            }
            Math_ApproachF(&eff->scale, eff->unk_38, 1.0f, 0.1f);
            Math_ApproachF(&eff->unk_40, 1.0f, 1.0f, 0.15f);
        } else if (eff->type == MS_EFF_FHG_LIGHT) {
            // EffectSsFhgFlash_UpdateLightBall
            eff->unk_30 += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;
            eff->scale -= 10.0f;
            eff->alpha -= 10;
            if ((eff->scale <= 0.0f) || (eff->alpha <= 0)) {
                eff->type = MS_EFF_NONE;
            }
        } else if (eff->type == MS_EFF_SHOCKWAVE) {
            eff->alpha -= 30;
            if (eff->alpha < 0) {
                eff->alpha = 0;
                eff->type = MS_EFF_NONE;
            }
            Math_ApproachF(&eff->scale, eff->unk_38, 1.0f, 0.13f);
        }
    }
}

void MsGanonMagic_DrawEffects(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    MsGanonEffect* eff;
    u8 flag;
    s32 i;

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    for (flag = 0, eff = sEffects, i = 0; i < 150; i++, eff++) {
        if (eff->type == MS_EFF_SPARKLE) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            if (flag == 0) {
                gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
                flag++;
            }
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(eff->scale, eff->scale, 1.0f, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
            FrameInterpolation_RecordCloseChild();
        }
    }
    for (flag = 0, eff = sEffects, i = 0; i < 150; i++, eff++) {
        if (eff->type == MS_EFF_LIGHT_RAY) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            if (flag == 0) {
                gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
                flag++;
            }
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_RotateY(eff->unk_48, MTXMODE_APPLY);
            Matrix_RotateX(eff->unk_44, MTXMODE_APPLY);
            Matrix_RotateZ(eff->unk_3C, MTXMODE_APPLY);
            Matrix_Scale(eff->scale, eff->scale, eff->unk_38 * eff->scale, MTXMODE_APPLY);
            Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
            FrameInterpolation_RecordCloseChild();
        }
    }
    for (eff = sEffects, i = 0; i < 150; i++, eff++) {
        if (eff->type == MS_EFF_IMPACT_DUST_LIGHT) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 200, 100, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, eff->timer * 4, 0, 32, 64, 1, eff->timer * 2,
                                          eff->timer * -20, 32, 32, 4, 0, 2, -20));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(eff->scale, eff->unk_40 * eff->scale, eff->scale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfImpactLightDL);
            FrameInterpolation_RecordCloseChild();
        }
    }
    for (eff = sEffects, i = 0; i < 150; i++, eff++) {
        if (eff->type == MS_EFF_SHOCKWAVE) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 150, 255, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (eff->timer * 100), 0, 64, 32, 1, (eff->timer * 100),
                                          0, 64, 32, 100, 0, 100, 0));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale((eff->scale * 200.0f) / 1500.0f, (eff->unk_40 * 200.0f) / 1500.0f,
                         (eff->scale * 200.0f) / 1500.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfShockwaveDL);
            FrameInterpolation_RecordCloseChild();
        }
    }
    for (eff = sEffects, i = 0; i < 150; i++, eff++) {
        if (eff->type == MS_EFF_BLACK_DOT) {
            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 170, 0, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 128);
            gSPSegment(POLY_XLU_DISP++, 0x0A,
                       (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 32, 1, eff->timer * 2, eff->timer * -20, 64,
                                          64, 0, 0, 2, -20));
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(eff->scale, eff->scale, 1.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfDotDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    for (eff = sEffects, i = 0; i < MS_EFFECT_COUNT; i++, eff++) {
        if (eff->type == MS_EFF_FHG_LIGHT) {
            // EffectSsFhgFlash_DrawLightBall
            static Color_RGB8 sFhgColors[] = {
                { 165, 255, 61 }, { 0, 255, 255 }, { 255, 40, 0 }, { 255, 255, 0 },
                { 0, 0, 255 },    { 255, 0, 255 }, { 255, 150, 0 },
            };
            f32 scale = eff->scale / 100.0f;
            Color_RGB8* color = &sFhgColors[eff->unk_2E];

            FrameInterpolation_RecordOpenChild(eff, eff->epoch);
            Gfx_SetupDL_25Xlu(gfxCtx);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, eff->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, color->r, color->g, color->b, 0);
            gDPPipeSync(POLY_XLU_DISP++);
            Matrix_Translate(eff->pos.x, eff->pos.y, eff->pos.z, MTXMODE_NEW);
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_RotateZ((eff->unk_30 / 32768.0f) * 3.1416f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPhantomEnergyBallDL);
            FrameInterpolation_RecordCloseChild();
        }
    }

    CLOSE_DISPS(gfxCtx);
}

// ---- what his magic does to what it hits ------------------------------------------------------------------------------
//
// (The user, 2026-10-16: his attacks must hurt Iron Knuckles, work on weak spots like Queen Gohma's eye, and hit hard.)
// An enemy's damage table answers to the lowest damage flag of a hit, so before each hit the flag is picked for the enemy
// about to be hit: for the bosses whose weak spot wants one weapon, that weapon's (with the Biggoron Sword's jump slash
// behind it, for when the spot is open -- CollisionCheck_GetSwordDamage); otherwise whichever of the strong ones its
// table rates highest (an armoured Iron Knuckle shrugs off arrows and light, not a Biggoron slash).

static const u32 sStrongFlags[] = {
    DMG_JUMP_GIANT, DMG_SPIN_GIANT, DMG_SLASH_GIANT, DMG_HAMMER_JUMP, DMG_ARROW_LIGHT, DMG_EXPLOSIVE, DMG_HAMMER_SWING,
};
// The light ball by how long he charged it (the user, 2026-10-16: "nerf the base damage of the normal charge but the
// longer u charge it the more damage it does"): a quick throw hits as the Master Sword, held it climbs to the strong ones.
static const u32 sTier0Flags[] = {
    DMG_SLASH_MASTER, DMG_ARROW_NORMAL,
};
static const u32 sTier1Flags[] = {
    DMG_SLASH_GIANT, DMG_SLASH_MASTER, DMG_ARROW_FIRE,
};
static const u32 sLightFlags[] = {
    DMG_SLASH_GIANT, DMG_ARROW_LIGHT, DMG_EXPLOSIVE, DMG_HAMMER_SWING, DMG_SLASH_MASTER,
};
static const struct {
    const u32* flags;
    s32 count;
    u32 sword;
    s32 damage; // the collider's damage, in quarters
} sTiers[4] = {
    { sTier0Flags, ARRAY_COUNT(sTier0Flags), DMG_SLASH_MASTER, 4 },
    { sTier1Flags, ARRAY_COUNT(sTier1Flags), DMG_SLASH_GIANT, 5 },
    { sLightFlags, ARRAY_COUNT(sLightFlags), DMG_SLASH_GIANT, 6 },
    { sStrongFlags, ARRAY_COUNT(sStrongFlags), DMG_JUMP_GIANT, 8 },
};
s16 gMsGanonMagicChargeLevel;

static s32 MsGanonMagic_FlagIndex(u32 flag) {
    s32 i;

    for (i = 0; i < 32; i++) {
        if (flag & (1U << i)) {
            return i;
        }
    }
    return 0;
}

static u32 MsGanonMagic_FlagsFor(Actor* target, u32 preferred, s32 tier) {
    const u32* flags = sTiers[tier].flags;
    s32 flagCount = sTiers[tier].count;
    u32 sword = sTiers[tier].sword;
    DamageTable* table;
    u32 best = preferred;
    s32 bestDamage;
    u32 all = 0;
    s32 i;

    switch ((target != NULL) ? target->id : -1) {
        case ACTOR_BOSS_GOMA:
        case ACTOR_EN_GOMA:
            return DMG_SLINGSHOT | sword; // the eye: stunned as by a seed; stunned, cut as by the Biggoron
        case ACTOR_BOSS_VA:
            if (target->params == BOSSVA_BODY) {
                break; // her body: by her table (her boomerang entry does nothing)
            }
            return DMG_BOOMERANG | sword; // her tentacles, zappers and jellyfish: the boomerang's
        case ACTOR_BOSS_SST:
            return DMG_ARROW_NORMAL | sword;
        case ACTOR_BOSS_FD:
        case ACTOR_BOSS_FD2:
            return DMG_HAMMER_JUMP | sword; // the head: the hammer's (0x40000040)
        case ACTOR_BOSS_MO:
            return DMG_HOOKSHOT | sword;
        case ACTOR_BOSS_GANONDROF:
            // in a painting, arrows hurt him; flying, an arrow only thuds -- a blade stuns and cuts
            return (((BossGanondrof*)target)->flyMode == GND_FLY_PAINTING) ? DMG_ARROW_NORMAL : sword;
        case ACTOR_BOSS_GANON:
            return DMG_ARROW_LIGHT | sword; // the light arrow lays him open, the blade cuts
        case ACTOR_BOSS_GANON2:
            return DMG_ARROW_LIGHT | DMG_JUMP_MASTER; // only the Master Sword cuts Ganon
        case ACTOR_BOSS_DODONGO:
        case ACTOR_BOSS_TW:
            return sword;
        case -1:
            return preferred;
        default:
            break;
    }
    table = target->colChkInfo.damageTable;
    if (table == NULL) {
        // a boss with no table takes the blade's (CollisionCheck_GetSwordDamage)
        return (target->category == ACTORCAT_BOSS) ? (preferred | sword) : preferred;
    }
    bestDamage = table->table[MsGanonMagic_FlagIndex(preferred)] & 0xF;
    for (i = 0; i < flagCount; i++) {
        s32 damage = table->table[MsGanonMagic_FlagIndex(flags[i])] & 0xF;

        all |= flags[i];
        if (damage > bestDamage) {
            bestDamage = damage;
            best = flags[i];
        }
    }
    // the chosen flag is the lowest; the higher strong ones only widen what will take the hit
    return best | (all & ~((best << 1) - 1));
}

static Actor* MsGanonMagic_Nearest(PlayState* play, Vec3f* pos, f32 range) {
    static const u8 sCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    Actor* nearest = NULL;
    f32 nearestDist = range;
    s32 c;

    for (c = 0; c < ARRAY_COUNT(sCategories); c++) {
        Actor* actor;

        for (actor = play->actorCtx.actorLists[sCategories[c]].head; actor != NULL; actor = actor->next) {
            f32 dist = Math_Vec3f_DistXYZ(pos, &actor->focus.pos);

            if ((actor->update != NULL) && (dist < nearestDist)) {
                nearestDist = dist;
                nearest = actor;
            }
        }
    }
    return nearest;
}

static Actor* MsGanonMagic_NearestBoss(PlayState* play, Vec3f* pos, f32 range) {
    Actor* nearest = NULL;
    f32 nearestDist = range;
    Actor* actor;

    for (actor = play->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != NULL; actor = actor->next) {
        f32 dist = Math_Vec3f_DistXYZ(pos, &actor->world.pos);

        if ((actor->update != NULL) && (dist < nearestDist)) {
            nearestDist = dist;
            nearest = actor;
        }
    }
    return nearest;
}

// Before CollisionCheck_SetAT: the damage for the enemy nearest the hitbox (twice the bosses' own where no table rates it).
static void MsGanonMagic_PrepareAT(MsGanonMagic* this, PlayState* play, Actor* target) {
    s32 tier = (this->type == MS_GANON_MAGIC_LIGHT_BALL) ? CLAMP(this->level, 0, 3) : 3;
    s32 quarters;

    if (this->type == MS_GANON_MAGIC_BIG_MAGIC) {
        // the big magic by the seconds he charged it: 1 the least (a sword's), 10 the most (four times that, and the
        // strongest flags)
        s32 seconds = CLAMP(this->level, 1, 10);

        tier = (seconds <= 3) ? 1 : (seconds <= 6) ? 2 : 3;
        quarters = 4 + (seconds - 1) * 4 / 3;
    } else {
        quarters = sTiers[tier].damage;
    }
    if (this->baseFlags == 0) {
        this->baseFlags = this->collider.info.toucher.dmgFlags;
        this->collider.info.toucher.damage = MIN(this->collider.info.toucher.damage * quarters / 4, 0x20);
    }
    if ((this->baseFlags == DMG_DEKU_NUT) || (this->type == MS_GANON_MAGIC_BLAST)) {
        return; // the roar only stuns; the blast is a bomb's
    }
    if (target == NULL) {
        // what it was thrown at; else a boss in the room (a boss is big: his middle can be far from where it strikes
        // him); else the enemy nearest to it
        Vec3f center = this->actor.world.pos;

        center.x = this->collider.dim.pos.x;
        center.y = this->collider.dim.pos.y + this->collider.dim.height * 0.5f;
        center.z = this->collider.dim.pos.z;
        if ((this->actor.child != NULL) && (this->actor.child->update != NULL) &&
            ((this->actor.child->category == ACTORCAT_ENEMY) || (this->actor.child->category == ACTORCAT_BOSS))) {
            target = this->actor.child;
        } else {
            target = MsGanonMagic_NearestBoss(play, &center, 1500.0f);
            if (target == NULL) {
                target = MsGanonMagic_Nearest(play, &center, this->collider.dim.radius + 150.0f);
            }
        }
    }
    this->collider.info.toucher.dmgFlags = MsGanonMagic_FlagsFor(target, this->baseFlags, tier);
}

// ---- the actor --------------------------------------------------------------------------------------------------------

// Spawned with world.rot aimed (the player's MergeShipGanondorf.c); a target point in home.pos for the big magic.
void MsGanonMagic_Init(Actor* thisx, PlayState* play) {
    MsGanonMagic* this = (MsGanonMagic*)thisx;
    s32 i;

    this->type = this->actor.params;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->alpha = 255.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sBallCylinderInit);
    this->targetPos = this->actor.home.pos;

    switch (this->type) {
        case MS_GANON_MAGIC_LIGHT_BALL:
            this->actor.speedXZ = 16.0f; // BossGanon_Init, the light ball (its 12, quicker in his hand)
            this->level = gMsGanonMagicChargeLevel;
            gMsGanonMagicChargeLevel = 0;
            break;
        case MS_GANON_MAGIC_BIG_MAGIC:
            // func_808E2544 case 10
            this->level = gMsGanonMagicChargeLevel; // (the player's: the seconds he charged it; 0 the boss's own)
            this->timer = 14;
            this->collider.dim.radius = 15;
            this->collider.dim.height = 20;
            this->collider.dim.yShift = -10;
            this->actor.speedXZ = 20.0f;
            this->frame = (s16)Rand_ZeroFloat(10000.0f);
            this->actor.shape.rot.y = this->actor.world.rot.y;
            for (i = 0; i < 15; i++) {
                this->trail[i] = this->actor.world.pos;
            }
            break;
        case MS_GANON_MAGIC_SHOCKWAVE:
            this->actor.speedXZ = 0.0f;
            this->collider.info.toucher.dmgFlags = DMG_EXPLOSIVE;
            this->collider.info.toucher.damage = 0x04;
            this->collider.dim.radius = 40;
            this->collider.dim.height = 60;
            this->collider.dim.yShift = 0;
            break;
        case MS_GANON_MAGIC_PHANTOM_ORB:
            // EnFhgFire_Init, FHGFIRE_ENERGY_BALL (its 8, quicker in his hand; its 70 frames)
            this->actor.speedXZ = 12.0f;
            this->timer = 70;
            this->alpha = 200.0f;
            this->collider.dim.radius = 40;
            this->collider.dim.height = 50;
            this->collider.dim.yShift = -25;
            break;
        case MS_GANON_MAGIC_PHANTOM_BURST:
            // EnFhgFire_Init, FHGFIRE_LIGHTNING_BURST (rot.x 0xC8: its scale 2, its collider 26)
            this->actor.speedXZ = 0.0f;
            this->alpha = 255.0f;
            this->timer = 32;
            this->state = 50; // FHGFIRE_FX_TIMER
            this->ringScale = 0.0f;
            this->collider.dim.radius = 26;
            this->collider.dim.height = 26;
            this->collider.dim.yShift = 0;
            break;
        case MS_GANON_MAGIC_HIT:
            this->actor.speedXZ = 0.0f;
            this->timer = 10;
            break;
        case MS_GANON_MAGIC_BLAST:
            // EnBom's explosion, as the bombable walls, rocks, grotto covers and floors take it (and the hammer's)
            this->actor.speedXZ = 0.0f;
            this->timer = 4;
            this->collider.info.toucher.dmgFlags = DMG_EXPLOSIVE | DMG_HAMMER_JUMP | DMG_HAMMER_SWING;
            this->collider.info.toucher.damage = 0x08;
            this->collider.dim.radius = 150;
            this->collider.dim.height = 120;
            this->collider.dim.yShift = -30;
            break;
        case MS_GANON_MAGIC_BURST:
        default:
            this->actor.speedXZ = 0.0f;
            this->ringScale = 6.0f;
            break;
    }
}

void MsGanonMagic_Destroy(Actor* thisx, PlayState* play) {
    MsGanonMagic* this = (MsGanonMagic*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

// A light ball's end (BossGanon_LightBall_Update): the light rays, the thunder and the ring.
static void MsGanonMagic_Break(MsGanonMagic* this, PlayState* play, s32 rays) {
    Vec3f vel;
    s32 i;

    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_GANON_HIT_THUNDER);
    for (i = 0; i < rays; i++) {
        vel.x = Rand_CenteredFloat(30.0f);
        vel.y = Rand_CenteredFloat(30.0f);
        vel.z = Rand_CenteredFloat(30.0f);
        MsGanonMagic_SpawnLightRay(&this->actor.world.pos, &vel, &sZero, Rand_ZeroFloat(200.0f) + 500.0f, 15.0f, 0x1E);
    }
    this->type = MS_GANON_MAGIC_BURST;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity = sZero;
    this->ringScale = this->actor.scale.x * 100.0f;
}

// After its CollisionCheck_SetAT: whether its hit is over. A boss it struck it flies on through for a few frames (the
// bosses that find the Fierce Deity's beam by its distance from their middle find it there), hitting nothing more.
static s32 MsGanonMagic_Struck(MsGanonMagic* this) {
    if (this->pierce > 0) {
        return ++this->pierce > 8;
    }
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (this->collider.base.at != NULL) {
            MergeShipTest_Log("GANONMAGIC hit actor 0x%04X params %d cat %d flags 0x%08X health %d type %d",
                              this->collider.base.at->id, this->collider.base.at->params,
                              this->collider.base.at->category, this->collider.info.toucher.dmgFlags,
                              this->collider.base.at->colChkInfo.health, this->type);
        }
        if ((this->collider.base.at != NULL) && (this->collider.base.at->category == ACTORCAT_BOSS)) {
            this->pierce = 1;
            return false;
        }
        return true;
    }
    return false;
}

static void MsGanonMagic_SetAT(MsGanonMagic* this, PlayState* play) {
    if (this->pierce == 0) {
        MsGanonMagic_PrepareAT(this, play, NULL);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

static s32 MsGanonMagic_HitSomething(MsGanonMagic* this, PlayState* play) {
    Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 15.0f, 15.0f, 1 | 4);
    if (MsGanonMagic_Struck(this)) {
        return true;
    }
    return (this->pierce == 0) &&
           ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING)) != 0);
}

void MsGanonMagic_Update(Actor* thisx, PlayState* play) {
    MsGanonMagic* this = (MsGanonMagic*)thisx;
    Vec3f pos;
    Vec3f vel;
    Vec3f accel;
    s32 i;

    this->frame++;
    if (this->timer != 0) {
        this->timer--;
    }

    switch (this->type) {
        case MS_GANON_MAGIC_LIGHT_BALL:
            // BossGanon_LightBall_Update: the fire, the flicker, the spin, flying straight; sparkles about it
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_FIRE - SFX_FLAG);
            Actor_SetScale(&this->actor, ((this->frame % 2) != 0) ? 6.0f : 5.25f);
            this->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 20000.0f) + 0x4000;
            Actor_UpdateVelocityXYZ(&this->actor);
            Actor_UpdatePos(&this->actor);
            for (i = 0; i < 2; i++) {
                accel.x = accel.z = 0.0f;
                accel.y = 0.2f;
                pos.x = Rand_CenteredFloat(30.0f) + this->actor.world.pos.x;
                pos.y = Rand_CenteredFloat(30.0f) + this->actor.world.pos.y;
                pos.z = Rand_CenteredFloat(30.0f) + this->actor.world.pos.z;
                MsGanonMagic_SpawnSparkle(&pos, &sZero, &accel, Rand_ZeroFloat(500.0f) + 700.0f, 0x1E);
            }
            Collider_UpdateCylinder(&this->actor, &this->collider);
            MsGanonMagic_SetAT(this, play);
            if (MsGanonMagic_HitSomething(this, play) || (this->frame > 90)) {
                MsGanonMagic_Break(this, play, 70);
            }
            break;

        case MS_GANON_MAGIC_PHANTOM_ORB: {
            // EnFhgFire_EnergyBall: the flicker and the spin, three green flashes a frame about it; at what he threw
            // it at, its lightning burst; into a wall or the floor, it fizzles
            s32 burst = false;

            Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_FIRE - SFX_FLAG);
            if (this->actor.child != NULL) {
                if (this->actor.child->update == NULL) {
                    this->actor.child = NULL;
                } else {
                    Math_ApproachS(&this->actor.world.rot.y,
                                   Actor_WorldYawTowardPoint(&this->actor, &this->actor.child->focus.pos), 2, 0x800);
                    Math_ApproachS(&this->actor.world.rot.x,
                                   -Math_Vec3f_Pitch(&this->actor.world.pos, &this->actor.child->focus.pos), 2, 0x800);
                    burst = Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->actor.child->focus.pos) <= 30.0f;
                }
            }
            Actor_UpdateVelocityXYZ(&this->actor);
            Actor_UpdatePos(&this->actor);
            Actor_SetScale(&this->actor, ((this->frame % 2) != 0) ? 6.0f : 5.25f);
            this->actor.shape.rot.z += (s16)(Rand_ZeroOne() * 0x4E20) + 0x4000;
            for (i = 0; i < 3; i++) {
                accel = sZero;
                accel.y = -0.08f;
                pos.x = Rand_CenteredFloat(20.0f) + this->actor.world.pos.x;
                pos.y = Rand_CenteredFloat(20.0f) + this->actor.world.pos.y;
                pos.z = Rand_CenteredFloat(20.0f) + this->actor.world.pos.z;
                MsGanonMagic_SpawnFhgLight(&pos, &sZero, &accel, (s16)(Rand_ZeroOne() * 80.0f) + 150,
                                                FHGFLASH_LIGHTBALL_GREEN);
            }
            Collider_UpdateCylinder(&this->actor, &this->collider);
            MsGanonMagic_SetAT(this, play);
            if (MsGanonMagic_Struck(this)) {
                burst = true;
            }
            Actor_UpdateBgCheckInfo(play, &this->actor, 50.0f, 50.0f, 100.0f, 7);
            if (burst || ((this->pierce == 0) && (this->actor.bgCheckFlags & 0x19)) || (this->timer == 0)) {
                Vec3f fall = { 0.0f, -0.1f, 0.0f };

                for (i = 0; i < 30; i++) {
                    vel.x = Rand_CenteredFloat(20.0f);
                    vel.y = Rand_CenteredFloat(20.0f);
                    vel.z = Rand_CenteredFloat(20.0f);
                    MsGanonMagic_SpawnFhgLight(&this->actor.world.pos, &vel, &fall,
                                                    (s16)(Rand_ZeroOne() * 50.0f) + 100, FHGFLASH_LIGHTBALL_GREEN);
                }
                if (burst) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_HIT_THUNDER);
                    Actor_Spawn(&play->actorCtx, play, this->actor.id, this->actor.world.pos.x,
                                this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0,
                                MS_GANON_MAGIC_PHANTOM_BURST);
                } else {
                    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_FANTOM_THUNDER_GND);
                }
                Actor_Kill(&this->actor);
            }
            break;
        }

        case MS_GANON_MAGIC_PHANTOM_BURST:
            // EnFhgFire_LightningBurst: the white flicker, the blast swelling and fading, a shock on what it catches
            play->envCtx.fillScreen = true;
            this->actor.shape.rot.y += 0x1000;
            this->state--;
            if (this->state >= 48) {
                play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] =
                    255;
                play->envCtx.screenFillColor[3] = ((this->timer % 2) != 0) ? 70 : 0;
            } else {
                play->envCtx.screenFillColor[3] = 0;
            }
            if (this->timer <= 20) {
                Math_ApproachZeroF(&this->alpha, 1.0f, 45.0f);
                Math_ApproachZeroF(&this->ringScale, 1.0f, 0.5f);
            } else {
                Math_ApproachF(&this->ringScale, 2.0f, 0.5f, 3.0f);
            }
            Actor_SetScale(&this->actor, this->ringScale);
            if (this->timer > 20) {
                Collider_UpdateCylinder(&this->actor, &this->collider);
                MsGanonMagic_PrepareAT(this, play, NULL);
                CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
            }
            if (this->timer == 0) {
                play->envCtx.fillScreen = false;
                Actor_Kill(&this->actor);
            }
            break;


        case MS_GANON_MAGIC_BIG_MAGIC: {
            // func_808E2544 cases 11 (seeking, snaking) and 1 (fading); the trail of 15 positions
            f32 xDiff;
            f32 yDiff;
            f32 zDiff;
            s16 yaw;
            f32 wobble;

            Actor_SetScale(&this->actor, 0.01f);
            Actor_UpdateVelocityXYZ(&this->actor);
            Actor_UpdatePos(&this->actor);
            this->trailIdx = (this->trailIdx + 1) % 15;
            this->trail[this->trailIdx] = this->actor.world.pos;
            this->trailRot[this->trailIdx].x = BINANG_TO_RAD(this->actor.world.rot.x);
            this->trailRot[this->trailIdx].y = BINANG_TO_RAD(this->actor.world.rot.y);
            if (this->state == 0) {
                if ((this->actor.child != NULL) && (this->actor.child->update != NULL)) {
                    this->targetPos = this->actor.child->focus.pos;
                }
                if (this->timer != 0) {
                    xDiff = this->targetPos.x - this->actor.world.pos.x;
                    yDiff = this->targetPos.y - this->actor.world.pos.y;
                    zDiff = this->targetPos.z - this->actor.world.pos.z;
                    yaw = RADF_TO_BINANG(Math_FAtan2F(xDiff, zDiff));
                    this->actor.shape.rot.x = RADF_TO_BINANG(Math_FAtan2F(yDiff, sqrtf(SQ(xDiff) + SQ(zDiff))));
                    Math_ApproachS(&this->actor.shape.rot.y, yaw, 1, this->stepScale);
                    Math_ApproachF(&this->stepScale, 4096.0f, 1.0f, 256.0f);
                }
                wobble = MIN((Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->targetPos) * 200.0f) / 10.0f, 13824.0f);
                this->actor.world.rot.x = (Math_CosS(this->frame * 0x3400) * wobble * 0.1f) + this->actor.shape.rot.x;
                this->actor.world.rot.y = (Math_SinS(this->frame * 0x1A00) * wobble) + this->actor.shape.rot.y;
                Collider_UpdateCylinder(&this->actor, &this->collider);
                MsGanonMagic_SetAT(this, play);
                if (MsGanonMagic_HitSomething(this, play) ||
                    (Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->targetPos) < 30.0f) || (this->frame > 70)) {
                    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 80, NA_SE_EN_FANTOM_THUNDER);
                    for (i = 0; i < 40; i++) {
                        vel.x = Rand_CenteredFloat(30.0f);
                        vel.y = Rand_CenteredFloat(30.0f);
                        vel.z = Rand_CenteredFloat(30.0f);
                        MsGanonMagic_SpawnLightRay(&this->actor.world.pos, &vel, &sZero,
                                                   Rand_ZeroFloat(200.0f) + 500.0f, 15.0f, 0x1E);
                    }
                    this->state = 1;
                    this->actor.speedXZ = 0.0f;
                }
            } else {
                Math_ApproachZeroF(&this->alpha, 1.0f, 40.0f);
                if (this->alpha == 0.0f) {
                    Actor_Kill(&this->actor);
                }
            }
            break;
        }

        case MS_GANON_MAGIC_SHOCKWAVE: {
            // BossGanon_PoundFloor's shockwave: its ring (GDF_EFF_SHOCKWAVE: scale -> 3, 0.13 a frame) knocks what it
            // passes -- here the enemies, each once, with the actor's collider put on them in turn
            Actor* enemy;
            f32 dist;

            Math_ApproachF(&this->ringScale, 3.0f, 1.0f, 0.13f);
            for (enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
                s32 done = false;

                for (i = 0; i < this->hitCount; i++) {
                    done |= (this->hit[i] == enemy);
                }
                dist = Actor_WorldDistXZToPoint(enemy, &this->actor.world.pos);
                if (!done && (enemy->update != NULL) && (fabsf(enemy->world.pos.y - this->actor.world.pos.y) < 80.0f) &&
                    ((this->ringScale * 150.0f) < dist) && (dist < (this->ringScale * 300.0f)) && (this->hitCount < 16)) {
                    this->hit[this->hitCount++] = enemy;
                    this->collider.dim.pos.x = enemy->world.pos.x;
                    this->collider.dim.pos.y = enemy->world.pos.y;
                    this->collider.dim.pos.z = enemy->world.pos.z;
                    MsGanonMagic_PrepareAT(this, play, enemy);
                    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
                    break; // one a frame
                }
            }
            if (this->frame > 30) {
                Actor_Kill(&this->actor);
            }
            break;
        }

        case MS_GANON_MAGIC_HIT:
            if ((this->actor.parent == NULL) || (this->actor.parent->update == NULL) || (this->timer == 0)) {
                Actor_Kill(&this->actor);
                break;
            }
            if (this->stepScale < 0.0f) {
                // MergeShip2: on a point its parent keeps (Ganondorf's axe head: its hitbox rides the blade)
                this->actor.world.pos = this->targetPos;
            } else {
                this->actor.world.pos.x =
                    this->actor.parent->world.pos.x + Math_SinS(this->actor.parent->shape.rot.y) * this->stepScale;
                this->actor.world.pos.y = this->actor.parent->world.pos.y;
                this->actor.world.pos.z =
                    this->actor.parent->world.pos.z + Math_CosS(this->actor.parent->shape.rot.y) * this->stepScale;
            }
            Collider_UpdateCylinder(&this->actor, &this->collider);
            MsGanonMagic_PrepareAT(this, play, NULL);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
            break;

        case MS_GANON_MAGIC_BLAST:
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
            if (this->timer == 0) {
                Actor_Kill(&this->actor);
            }
            break;

        case MS_GANON_MAGIC_BURST:
        default:
            // BossGanon_LightBall_Update, unk_1A8 == 1: the ring spins out and fades
            this->actor.shape.rot.y += 0x1000;
            Math_ApproachZeroF(&this->alpha, 1.0f, 30.0f);
            Math_ApproachF(&this->ringScale, 20.0f, 0.5f, 100.0f);
            Actor_SetScale(&this->actor, this->ringScale);
            this->spin += ((M_PI / 2) + Rand_ZeroFloat(M_PI / 4));
            if (this->alpha == 0.0f) {
                Actor_Kill(&this->actor);
            }
            break;
    }
}

static Gfx* sBigMagicLightStreakDLists[] = {
    (Gfx*)gGanondorfLightStreak12DL, (Gfx*)gGanondorfLightStreak11DL, (Gfx*)gGanondorfLightStreak10DL,
    (Gfx*)gGanondorfLightStreak9DL,  (Gfx*)gGanondorfLightStreak8DL,  (Gfx*)gGanondorfLightStreak7DL,
    (Gfx*)gGanondorfLightStreak6DL,  (Gfx*)gGanondorfLightStreak5DL,  (Gfx*)gGanondorfLightStreak4DL,
    (Gfx*)gGanondorfLightStreak3DL,  (Gfx*)gGanondorfLightStreak2DL,  (Gfx*)gGanondorfLightStreak1DL,
};

void MsGanonMagic_Draw(Actor* thisx, PlayState* play) {
    MsGanonMagic* this = (MsGanonMagic*)thisx;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    switch (this->type) {
        case MS_GANON_MAGIC_LIGHT_BALL:
        case MS_GANON_MAGIC_BURST: {
            // BossGanon_LightBall_Draw: its light on the floor, then the ball (or the ring of eight)
            f32 alpha = ((this->frame % 2) != 0) ? this->alpha * 0.4f : this->alpha * 0.35f;

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 155, (s8)alpha);
            Matrix_Push();
            Matrix_Translate(this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z, MTXMODE_NEW);
            Matrix_Scale(this->actor.scale.x * 0.75f, 1.0f, this->actor.scale.z * 0.75f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightCoreDL);
            Matrix_Pop();
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)this->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 0, 0);
            if (this->type == MS_GANON_MAGIC_BURST) {
                for (i = 0; i < 8; i++) {
                    FrameInterpolation_RecordOpenChild(this, i);
                    Matrix_Push();
                    Matrix_RotateY(i * (M_PI / 8), MTXMODE_APPLY);
                    Matrix_RotateZ(this->spin, MTXMODE_APPLY);
                    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
                    Matrix_Pop();
                    FrameInterpolation_RecordCloseChild();
                }
            } else {
                Matrix_ReplaceRotation(&play->billboardMtxF);
                Matrix_RotateZ((this->actor.shape.rot.z / 32768.0f) * 3.1416f, MTXMODE_APPLY);
                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
            }
            break;
        }

        case MS_GANON_MAGIC_PHANTOM_BURST:
            // EnFhgFire_Draw, FHGFIRE_LIGHTNING_BURST
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)this->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 165, 255, 75, 0);
            gDPPipeSync(POLY_XLU_DISP++);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPhantomLightningBlastDL);
            break;

        case MS_GANON_MAGIC_PHANTOM_ORB:
            // EnFhgFire_Draw, FHGFIRE_ENERGY_BALL
            Matrix_ReplaceRotation(&play->billboardMtxF);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s8)this->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 165, 255, 75, 0);
            gDPPipeSync(POLY_XLU_DISP++);
            Matrix_RotateZ((this->actor.shape.rot.z / (f32)0x8000) * 3.1416f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gPhantomEnergyBallDL);
            break;

        case MS_GANON_MAGIC_BIG_MAGIC: {
            // func_808E324C: the streak along its last positions, and its light
            Mtx* mtx = Graph_Alloc(play->state.gfxCtx, 12 * sizeof(Mtx));

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, (s8)this->alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 150, 255, 0, 128);
            gSPSegment(POLY_XLU_DISP++, 0x0D, (uintptr_t)mtx);
            for (i = 0; i < 12; i++) {
                s32 k = ((this->trailIdx - i) + 15) % 15;

                Matrix_Translate(this->trail[k].x, this->trail[k].y, this->trail[k].z, MTXMODE_NEW);
                Matrix_RotateY(this->trailRot[k].y, MTXMODE_APPLY);
                Matrix_RotateX(-this->trailRot[k].x, MTXMODE_APPLY);
                Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
                Matrix_RotateY(M_PI / 2, MTXMODE_APPLY);
                MATRIX_TOMTX(mtx);
                gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, sBigMagicLightStreakDLists[i]);
                mtx++;
            }
            Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(10.0f, 10.0f, 10.0f, MTXMODE_APPLY);
            Matrix_RotateZ(Rand_CenteredFloat(M_PI), MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfLightBallMaterialDL);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanondorfSquareDL);
            break;
        }

        default:
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
