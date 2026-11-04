/*
 * File: z_mm_obj_aqua.c
 * Overlay: ovl_MM_Obj_Aqua
 * Description: Majora's Mask's water poured out of a bottle
 *
 * MergeShip2 -- ported from 2ship2harkinian/mm/src/overlays/actors/ovl_Obj_Aqua/z_obj_aqua.c. Same struct,
 * functions, constants, frame counts and order as MM; the model is MM's gEffFire1DL (mm.o2r, gameplay_keep).
 *
 * Host-game differences:
 * - Collider: MM's AT toucher carries MM damage flags 0xF7CFFFFF (AT_TYPE_OTHER). MM and Ocarina of Time give
 *   those bits different meanings, and in Hyrule they would hit Ocarina of Time's enemies as swords, arrows and
 *   fire. No Ocarina of Time actor reacts to MM's bottle water, so the toucher's dmgFlags are 0 (a toucher with
 *   no flags in common with a bumper never hits). The collider keeps MM's shape, AT/OC setup and per-frame
 *   radius so MM actors ported later can test for it (`ac->id == gMergeShipActorId_MM_Obj_Aqua`).
 * - Hot water's steam: MM's EffectSsIceSmoke draws gameplay_keep's gEffIceSmokeDL and fades out over its last
 *   10 frames; Ocarina of Time's effect of the same name needs object_fz (Freezard's steam) and does nothing
 *   without it. MM's effect is ported here (MMObjAqua_SpawnIceSmoke: MM's init/update/draw, inserted into
 *   Ocarina of Time's soft sprite table with EffectSs_Insert, priority 128 as MM spawns it).
 * - MM func_800C0094 (floor-poly matrix) builds a different matrix than Ocarina of Time's func_80038A28; MM's
 *   is ported here. MM Matrix_MtxFToYXZRot is Ocarina of Time's Matrix_MtxFToYXZRotS (same algorithm).
 * - MM Math_Vec3f_StepTo does not exist in Ocarina of Time; ported here.
 * - MM ObjAqua_IsUnderwater uses WaterBox_GetSurfaceImpl, which in MM also searches BgActor water boxes.
 *   Ocarina of Time has no BgActor water boxes; WaterBox_GetSurface1 is used (with the y input set to the
 *   actor's y, as Actor_UpdateBgCheckInfo does) so Zora's Domain's special water box agrees with the actor's
 *   own BGCHECKFLAG_WATER.
 * - MM actor.depthInWater is Ocarina of Time's actor.yDistToWater (surface y - actor y, same sign).
 * - MM SoundSource_PlaySfxAtFixedWorldPos with an MM sound id: Ocarina of Time's sound sources play Ocarina of
 *   Time ids, so MMObjAqua_PlayMMSfxAtFixedWorldPos takes a play->soundSources slot exactly as Ocarina of Time
 *   (and MM) do and plays the MM id at its projected position; SoundSource_UpdateAll moves the position and
 *   stops the sound when the countdown ends (Audio_StopSfxByPos also stops MM sounds).
 * - EffectSsGSplash, EffectSsBubble and the circle shadow are Ocarina of Time's (same arguments; the effects
 *   are the same in both games).
 * - Matrix_RotateZS/YS (binary angles) are Ocarina of Time's Matrix_RotateZ/Y with BINANG_TO_RAD.
 */

#include "z_mm_obj_aqua.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "soh/MergeShip/MergeShipTransformFx.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)

s16 gMergeShipActorId_MM_Obj_Aqua = -1;

// Defined in z_bgcheck.c, not declared in a header.
void CollisionPoly_GetNormalF(CollisionPoly* poly, f32* nx, f32* ny, f32* nz);

static void func_80ACBC70(MMObjAqua* this);
static void func_80ACBDCC(MMObjAqua* this);
static void func_80ACBC8C(MMObjAqua* this, PlayState* play);
static void func_80ACBD48(MMObjAqua* this, PlayState* play);
static void func_80ACBDFC(MMObjAqua* this, PlayState* play);

static void func_80ACBD34(MMObjAqua* this);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_OTHER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        // MM 0xF7CFFFFF (MM damage bits); 0 in Hyrule, see the header comment.
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 6, 10, 0, { 0, 0, 0 } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3S(shape.rot, 0, ICHAIN_CONTINUE),
    ICHAIN_VEC3S(world.rot, 0, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -900, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(minVelocityY, -4000, ICHAIN_CONTINUE),     // MM terminalVelocity
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),         // MM cullingVolumeDistance
    ICHAIN_F32(uncullZoneScale, 300, ICHAIN_CONTINUE),            // MM cullingVolumeScale
    ICHAIN_F32(uncullZoneDownward, 300, ICHAIN_STOP),             // MM cullingVolumeDownward
};

static Vec3f D_80ACC308 = { 1.0f / 1000.0f, 7.0f / 10000.0f, 1.0f / 1000.0f };
static Vec3f D_80ACC314 = { 8.6f / 1000.0f, 8.0f / 10000.0f, 8.6f / 1000.0f };
static Vec3f D_80ACC320 = { 1.0f / 100.0f, 2.6f / 1000.0f, 1.0f / 100.0f };

static Vec3f sMMZeroVec3f = { 0.0f, 0.0f, 0.0f }; // MM gZeroVec3f

// ---- MM helpers Ocarina of Time lacks ----------------------------------------------------------------------

// MM Math_Vec3f_StepTo (z_lib.c)
static f32 MMObjAqua_Math_Vec3f_StepTo(Vec3f* start, Vec3f* target, f32 speed) {
    Vec3f diff;
    f32 f2;
    f32 f0;

    Math_Vec3f_Diff(target, start, &diff);
    f0 = Math3D_Vec3fMagnitude(&diff);
    if (speed < f0) {
        f2 = speed / f0;
        f0 -= speed;
        start->x += f2 * diff.x;
        start->y += f2 * diff.y;
        start->z += f2 * diff.z;
    } else {
        Math_Vec3f_Copy(start, target);
        f0 = 0.0f;
    }

    return f0;
}

// MM func_800C0094 (z_bgcheck.c)
static void MMObjAqua_func_800C0094(CollisionPoly* poly, f32 tx, f32 ty, f32 tz, MtxF* dest) {
    f32 nx;
    f32 ny;
    f32 nz;
    f32 z_f14;
    f32 phi_f14;
    f32 phi_f12;
    f32 inv_z_f14;

    if (poly == NULL) {
        return;
    }
    CollisionPoly_GetNormalF(poly, &nx, &ny, &nz);

    z_f14 = sqrtf(SQ(ny) + SQ(nz));
    if (!IS_ZERO(z_f14)) {
        inv_z_f14 = 1.0f / z_f14;
        phi_f14 = ny * inv_z_f14;
        phi_f12 = nz * inv_z_f14;
    } else {
        phi_f14 = 1.0f;
        phi_f12 = 0.0f;
    }
    dest->xx = z_f14;
    dest->yx = (-nx) * phi_f14;
    dest->zx = (-nx) * phi_f12;
    dest->xy = nx;
    dest->yy = ny;
    dest->zy = nz;
    dest->xz = 0.0f;
    dest->yz = -phi_f12;
    dest->zz = phi_f14;
    dest->xw = tx;
    dest->yw = ty;
    dest->zw = tz;
    dest->wx = 0.0f;
    dest->wy = 0.0f;
    dest->wz = 0.0f;
    dest->ww = 1.0f;
}

// MM SoundSource_PlaySfxAtFixedWorldPos (SoundSource_Add, playSfxEachFrame = false) with an MM sound id.
// The slot choice is Ocarina of Time's and MM's: a free source, else the one closest to finishing.
static void MMObjAqua_PlayMMSfxAtFixedWorldPos(PlayState* play, Vec3f* worldPos, s32 duration, u16 mmSfxId) {
    s32 countdown;
    SoundSource* source;
    s32 smallestCountdown = 0xFFFF;
    SoundSource* backupSource = &play->soundSources[0];
    s32 i;

    source = &play->soundSources[0];
    for (i = 0; i < ARRAY_COUNT(play->soundSources); i++) {
        if (source->countdown == 0) {
            break;
        }

        countdown = source->countdown;
        if (countdown < smallestCountdown) {
            smallestCountdown = countdown;
            backupSource = source;
        }
        source++;
    }

    if (i >= ARRAY_COUNT(play->soundSources)) {
        source = backupSource;
        Audio_StopSfxByPos(&source->projectedPos);
    }

    source->worldPos = *worldPos;
    source->countdown = (u16)duration;

    SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &source->worldPos, &source->projectedPos);
    MergeShip_PlayMMSfxAtPos(&source->projectedPos, mmSfxId);
}

// ---- MM EffectSsIceSmoke (ovl_Effect_Ss_Ice_Smoke), see the header comment --------------------------------

#define rAlpha regs[0]
#define rScale regs[1]
#define rScrollX regs[2]
#define rScrollY regs[3]

static void MMObjAqua_IceSmoke_Draw(PlayState* play, u32 index, EffectSs* this) {
    Mtx* mtx;
    f32 scale;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 195, 235, 235, this->rAlpha);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, this->rScrollX * this->life,
                                             this->rScrollY * this->life, 0x20, 0x40, 1, 0, 0, 0x20, 0x20,
                                             -this->rScrollX, -this->rScrollY, 0, 0));
    Matrix_Translate(this->pos.x, this->pos.y, this->pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    scale = this->rScale * 0.0001f;
    Matrix_Scale(scale, scale, 1.0f, MTXMODE_APPLY);
    mtx = MATRIX_NEWMTX(play->state.gfxCtx);
    if (mtx != NULL) {
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gEffIceSmokeDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void MMObjAqua_IceSmoke_Update(PlayState* play, u32 index, EffectSs* this) {
    if (this->life < 10) {
        Math_StepToS(&this->rAlpha, 0, 10);
    } else if (this->rAlpha < 100) {
        this->rAlpha += 10;
    }
}

// MM EffectSsIceSmoke_Spawn + EffectSsIceSmoke_Init.
static void MMObjAqua_SpawnIceSmoke(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale) {
    // Frame interpolation keys a soft sprite by (slot, epoch); a private range keeps these apart from the
    // epochs EffectSs_Spawn counts up per slot.
    static u32 sEpoch = 0x40000000;
    EffectSs effect;

    EffectSs_Reset(&effect);
    Math_Vec3f_Copy(&effect.pos, pos);
    Math_Vec3f_Copy(&effect.velocity, velocity);
    Math_Vec3f_Copy(&effect.accel, accel);
    effect.rAlpha = 0;
    effect.rScale = ABS(scale);
    if (scale < 0) {
        effect.rScrollX = 1;
        effect.rScrollY = 2;
    } else {
        effect.rScrollX = 3;
        effect.rScrollY = 15;
    }
    effect.life = 50;
    effect.draw = MMObjAqua_IceSmoke_Draw;
    effect.update = MMObjAqua_IceSmoke_Update;
    effect.type = EFFECT_SS_ICE_SMOKE;
    effect.priority = 128;
    effect.epoch = sEpoch++;

    EffectSs_Insert(play, &effect);
}

#undef rAlpha
#undef rScale
#undef rScrollX
#undef rScrollY

// ---- MM ObjAqua ---------------------------------------------------------------------------------------------

// MM func_80ACB6A0: splash on the water surface.
static void func_80ACB6A0(MMObjAqua* this, PlayState* play) {
    Vec3f sp58;
    s32 angleOffset = 0;
    s32 i;

    sp58.y = this->actor.world.pos.y + this->actor.yDistToWater;
    for (i = 0; i < 4; i++) {
        sp58.x = this->actor.world.pos.x + Math_SinS((s16)((s32)(Rand_ZeroOne() * 7200.0f) + angleOffset)) * 8.0f;
        sp58.z = this->actor.world.pos.z + Math_CosS((s16)((s32)(Rand_ZeroOne() * 7200.0f) + angleOffset)) * 8.0f;
        EffectSsGSplash_Spawn(play, &sp58, NULL, NULL, 0, 120);
        angleOffset += 0x4000;
    }
    sp58.x = this->actor.world.pos.x;
    sp58.z = this->actor.world.pos.z;
    EffectSsGSplash_Spawn(play, &sp58, NULL, NULL, 0, 300);
}

// MM func_80ACB7F4: splash on the floor.
static void func_80ACB7F4(MMObjAqua* this, PlayState* play) {
    Vec3f effectPos;
    s32 angleOffset = 0;
    s32 i;

    effectPos.y = this->actor.floorHeight;
    for (i = 0; i < 4; i++) {
        effectPos.x =
            this->actor.world.pos.x + Math_SinS((s16)((s32)(Rand_ZeroOne() * 7200.0f) + angleOffset)) * 8.0f;
        effectPos.z =
            this->actor.world.pos.z + Math_CosS((s16)((s32)(Rand_ZeroOne() * 7200.0f) + angleOffset)) * 8.0f;
        EffectSsGSplash_Spawn(play, &effectPos, NULL, NULL, 0, 120);
        angleOffset += 0x4000;
    }
    effectPos.x = this->actor.world.pos.x;
    effectPos.z = this->actor.world.pos.z;
    EffectSsGSplash_Spawn(play, &effectPos, NULL, NULL, 0, 300);
}

// MM func_80ACB940: steam from hot water.
static void func_80ACB940(MMObjAqua* this, PlayState* play) {
    Vec3f effectPos;
    Vec3f effectVelocity;

    effectVelocity.x = Rand_ZeroOne() - 0.5f;
    effectVelocity.y = 2.0f;
    effectVelocity.z = Rand_ZeroOne() - 0.5f;
    effectPos.x = this->actor.world.pos.x + (effectVelocity.x * 40.0f);
    effectPos.y = this->actor.world.pos.y;
    effectPos.z = this->actor.world.pos.z + (effectVelocity.z * 40.0f);
    MMObjAqua_SpawnIceSmoke(play, &effectPos, &effectVelocity, &sMMZeroVec3f,
                            (s16)((s32)(Rand_ZeroOne() * 24.0f) + 70));
}

// MM func_80ACBA10: lie flat on the floor poly.
static void func_80ACBA10(MMObjAqua* this) {
    MtxF sp2C;

    MMObjAqua_func_800C0094(this->actor.floorPoly, this->actor.world.pos.x, this->actor.floorHeight,
                            this->actor.world.pos.z, &sp2C);
    Matrix_MtxFToYXZRotS(&sp2C, &this->actor.shape.rot, false);
}

// MM ObjAqua_IsUnderwater
static s32 MMObjAqua_IsUnderwater(MMObjAqua* this, PlayState* play) {
    WaterBox* waterBox;
    f32 waterSurface = this->actor.world.pos.y;

    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                             &waterBox) &&
        (this->actor.world.pos.y < waterSurface)) {
        return true;
    }
    return false;
}

void MMObjAqua_Init(Actor* thisx, PlayState* play) {
    MMObjAqua* this = (MMObjAqua*)thisx;
    s32 i;

    Actor_ProcessInitChain(thisx, sInitChain);
    this->actor.scale.x = 0.0009f;
    this->actor.scale.y = 0.0005f;
    this->actor.scale.z = 0.0009f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 60.0f);

    thisx->shape.shadowAlpha = 140;
    this->alpha = 255;
    if (MMObjAqua_IsUnderwater(this, play)) {
        for (i = 0; i < 8; i++) {
            EffectSsBubble_Spawn(play, &thisx->world.pos, -4.0f, 4.0f, 4.0f, (Rand_ZeroOne() * 0.09f) + 0.03f);
        }
        func_80ACBDCC(this);
    } else {
        func_80ACBC70(this);
    }
}

void MMObjAqua_Destroy(Actor* thisx, PlayState* play) {
    MMObjAqua* this = (MMObjAqua*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

// MM func_80ACBC70: falling through the air.
static void func_80ACBC70(MMObjAqua* this) {
    this->counter = 200;
    this->actionFunc = func_80ACBC8C;
}

static void func_80ACBC8C(MMObjAqua* this, PlayState* play) {
    if (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WATER)) {
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            func_80ACB7F4(this, play);
            func_80ACBA10(this);
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_BOTTLE_WATERING);
            func_80ACBD34(this);
        } else {
            func_80ACB6A0(this, play);
            MMObjAqua_PlayMMSfxAtFixedWorldPos(play, &this->actor.world.pos, 0x28, MM_NA_SE_EV_BOMB_DROP_WATER);
            Actor_Kill(&this->actor);
        }
    } else if (this->counter <= 0) {
        Actor_Kill(&this->actor);
    }
}

// MM func_80ACBD34: a puddle on the floor.
static void func_80ACBD34(MMObjAqua* this) {
    this->actionFunc = func_80ACBD48;
}

static void func_80ACBD48(MMObjAqua* this, PlayState* play) {
    if ((MM_AQUA_GET_TYPE(&this->actor) == MM_AQUA_TYPE_HOT) && (this->alpha > 90)) {
        func_80ACB940(this, play);
    }
    if (this->alpha > 5) {
        this->alpha -= 5;
    } else {
        this->alpha = 0;
    }
    if (this->actor.shape.shadowAlpha > 2) {
        this->actor.shape.shadowAlpha -= 2;
    } else {
        Actor_Kill(&this->actor);
    }
}

// MM func_80ACBDCC: poured underwater.
static void func_80ACBDCC(MMObjAqua* this) {
    this->actor.gravity = -0.1f;
    this->counter = 40;
    this->alpha = 140;
    this->actionFunc = func_80ACBDFC;
}

static void func_80ACBDFC(MMObjAqua* this, PlayState* play) {
    f32 temp = this->counter;

    this->alpha = (u8)((s32)(temp * 3.25f) + 10);
    this->actor.shape.shadowAlpha = this->alpha;
    this->unk_198 += 1000;
    if (MM_AQUA_GET_TYPE(&this->actor) == MM_AQUA_TYPE_HOT) {
        f32 temp_f2 = this->actor.scale.x * 10000.0f;

        EffectSsBubble_Spawn(play, &this->actor.world.pos, temp_f2 * -0.5f, temp_f2, temp_f2,
                             (Rand_ZeroOne() * 0.1f) + 0.03f);
    }
    if (this->counter <= 0) {
        Actor_Kill(&this->actor);
    }
}

void MMObjAqua_Update(Actor* thisx, PlayState* play) {
    MMObjAqua* this = (MMObjAqua*)thisx;

    if (this->counter > 0) {
        this->counter--;
    }
    this->actionFunc(this, play);
    if (this->actor.update != NULL) {
        if (this->actionFunc == func_80ACBC8C) {
            MMObjAqua_Math_Vec3f_StepTo(&this->actor.scale, &D_80ACC308, 0.00006f);
        } else if (this->actionFunc == func_80ACBD48) {
            MMObjAqua_Math_Vec3f_StepTo(&this->actor.scale, &D_80ACC314, 0.00095f);
        } else {
            MMObjAqua_Math_Vec3f_StepTo(&this->actor.scale, &D_80ACC320, 0.0004f);
        }
        this->actor.velocity.y *= 0.9f;
        Actor_MoveXZGravity(&this->actor); // MM Actor_MoveWithGravity
        // MM UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4: walls, floor and water (Ocarina of Time's 1 | 4).
        Actor_UpdateBgCheckInfo(play, &this->actor, 12.0f, 4.0f, 0.0f, 1 | 4);
        if (this->actionFunc != func_80ACBDFC) {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            this->collider.dim.radius = (s16)(this->actor.scale.x * 3000.0f);
            CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

void MMObjAqua_Draw(Actor* thisx, PlayState* play) {
    MMObjAqua* this = (MMObjAqua*)thisx;
    s32 framesTemp;
    s16 yaw = (s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000);
    s32 actionFuncTemp = this->actionFunc == func_80ACBDFC;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    // MM ((gameplayFrames & 0x7FFFFFFF) * -0xA) & 0x1FF, in unsigned arithmetic as MM's u32 gameplayFrames.
    framesTemp = (s32)(((play->gameplayFrames & 0x7FFFFFFF) * (u32)-0xA) & 0x1FF);
    if (actionFuncTemp) {
        framesTemp >>= 1;
    }
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, framesTemp, 0x20, 0x80, 0,
                                             0, 0, actionFuncTemp ? -5 : -0xA));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 170, 255, 255, this->alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 150, 255, 0);
    if (actionFuncTemp) {
        s16 rotation = (s16)(Math_SinS(this->unk_198) * 8000.0f);

        Matrix_RotateZ(BINANG_TO_RAD(rotation), MTXMODE_APPLY);
        Matrix_Scale(1.3f, 1.0f, 1.0f, MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD((s16)(rotation * -1)), MTXMODE_APPLY);
        Matrix_Scale(10.0f / 13.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    }

    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gEffFire1DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
