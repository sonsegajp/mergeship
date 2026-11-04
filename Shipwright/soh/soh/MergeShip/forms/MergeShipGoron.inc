// MergeShip2 -- Majora's Mask Goron form, ported from 2ship2harkinian/mm into Ocarina of Time's player.
// Included at the end of z_player.c. See docs/PORTING_GUIDE.md.
//
// Every function names the MM function it ports (2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/
// z_player.c unless stated otherwise). Values, frame numbers and ordering are MM's. The Goron lives in
// Hyrule: where MM consults an MM-only system (Actor_SetPlayerImpact, MM's staircase doors, Termina's
// actors), Ocarina of Time's nearest equivalent is used or the MM-only part is left out, as noted.

#include "soh/MergeShip/forms/MergeShipGoron.h"
#include "soh/MergeShip/MergeShipMMScene.h"
#include "mm/objects/object_link_goron/object_link_goron.h"
#include "src/overlays/actors/ovl_MM_En_Test/z_mm_en_test.h"

#define GORON_ANIM(name) ((LinkAnimationHeader*)&mm_##name)
#define GORON_IS_PUNCHING_MWA(mwa) \
    (((mwa) >= MM_PLAYER_MWA_GORON_PUNCH_LEFT) && ((mwa) <= MM_PLAYER_MWA_GORON_PUNCH_BUTT))

// MM SFX_FLAG and the floor offsets MM compares floorSfxOffset against.
#define GORON_SFX_FLAG 0x800
#define GORON_FLOOR_SFX(mmWalkSfx) ((mmWalkSfx) - GORON_SFX_FLAG)

// MM func_80173B48 (game.c): this frame's spare RSP/RDP time in nanoseconds, used to budget dust
// particles. On PC there is no RDP time to subtract, so it is the frame's full retrace budget.
#define GORON_FRAME_BUDGET_NS ((s32)(R_UPDATE_RATE * 16683333))

static s8 sGoronActionHandlerList12[] = {
    // MM sActionHandlerList12
    PLAYER_ACTION_HANDLER_13,
    PLAYER_ACTION_HANDLER_2,
    -PLAYER_ACTION_HANDLER_TALK,
};

// MM func_8082DA90.
static s32 MergeShip_Goron_func_8082DA90(PlayState* play) {
    return (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF);
}

// MM Player_StopHorizontalMovement.
static void MergeShip_Goron_StopHorizontalMovement(Player* this) {
    this->linearVelocity = 0.0f;
    this->actor.speedXZ = 0.0f;
}

// MM func_8082DD2C (OoT func_80832440, plus the fields MM also clears).
static void MergeShip_Goron_func_8082DD2C(PlayState* play, Player* this) {
    func_80832440(play, this);
    this->actor.shape.rot.z = 0;
    this->unk_ABC = 0.0f;
    this->unk_AC0 = 0.0f;
}

// MM Player_AnimSfx_PlayFloorLand.
static void MergeShip_Goron_PlayFloorLand(Player* this) {
    MergeShip_PlayMMFloorLandSfx(this);
}

// MM Player_RequestQuake (OoT's is identical).
// MM Player_RequestQuakeAndRumble.
static void MergeShip_Goron_RequestQuakeAndRumble(PlayState* play, Player* this, u16 mmSfxId) {
    Player_RequestQuake(play, 27767, 7, 20);
    Player_RequestRumble(this, 255, 20, 150, SQ(0));
    MergeShip_PlayMMSfxActor(&this->actor, mmSfxId);
}

// MM Actor_GetSlopeDirection (OoT Player_GetSlopeDirection).
#define MergeShip_Goron_GetSlopeDirection Player_GetSlopeDirection

// ---- the player cylinder ----------------------------------------------------------------------------

// MM Player_ResetCylinder. Hyrule's player cylinder is restored to Ocarina of Time's own values
// (D_80854624), which Hyrule's enemies are tuned against; MM differs only in its AC mask (0xF7CFFFFF).
static void MergeShip_Goron_ResetCylinder(Player* this) {
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

// MM Player_SetCylinderForAttack. `dmgFlags` are MM's; they are mapped to Hyrule's here.
static void MergeShip_Goron_SetCylinderForAttack(Player* this, s32 mmAttack, s32 damage, s32 radius) {
    u32 dmgFlags;

    switch (mmAttack) {
        case 0: // DMG_GORON_POUND
            dmgFlags = MERGESHIP_GORON_DMG_POUND;
            break;
        case 1: // DMG_GORON_SPIKES
            dmgFlags = MERGESHIP_GORON_DMG_SPIKES;
            break;
        default: // DMG_NORMAL_ROLL
            dmgFlags = MERGESHIP_GORON_DMG_NORMAL_ROLL;
            break;
    }

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

    if (mmAttack == 0) { // DMG_GORON_POUND
        this->cylinder.base.acFlags = AC_NONE;
    } else {
        this->cylinder.base.colType = COLTYPE_NONE;
        this->cylinder.info.bumper.dmgFlags = 0xFFCFFFFF;
        this->cylinder.base.acFlags = AC_ON | AC_TYPE_ENEMY;
    }
}
#define GORON_ATTACK_POUND 0
#define GORON_ATTACK_SPIKES 1
#define GORON_ATTACK_NORMAL_ROLL 2

// ---- MM Magic_Consume(MAGIC_CONSUME_GORON_ZORA) / Magic_Update's CONSUME_GORON_ZORA states --------------
// Ocarina of Time's magic meter has no continuous-drain state, so the spiked roll's drain is run here:
// the meter flashes (MAGIC_STATE_METER_FLASH_3, which Ocarina of Time's Magic_Update only flashes) while
// the Goron takes 2 at once and then 1 every 10 frames, exactly as MM's Magic_Update does. Magic_Reset
// ends it, as in MM.

static s32 MergeShip_Goron_MagicConsume(PlayState* play) {
    if (!gSaveContext.isMagicAcquired) {
        return false;
    }
    if ((gSaveContext.magic - 2) < 0) {
        if (gSaveContext.magicCapacity != 0) {
            MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        }
        return false;
    }
    if (gSaveContext.magic != 0) {
        gMergeShipGoron.magicTimer = 10;
        gMergeShipGoron.magicDrain = 1;
        gSaveContext.magicState = MAGIC_STATE_METER_FLASH_3;
        return true;
    }
    return false;
}

static void MergeShip_Goron_UpdateMagicDrain(PlayState* play) {
    if (gMergeShipGoron.magicDrain == 0) {
        return;
    }
    if (gSaveContext.magicState != MAGIC_STATE_METER_FLASH_3) {
        // Magic_Reset (or anything else) ended it.
        gMergeShipGoron.magicDrain = 0;
        return;
    }

    if (gMergeShipGoron.magicDrain == 1) {
        // MAGIC_STATE_CONSUME_GORON_ZORA_SETUP
        if (!MergeShip_Chateau_IsActive()) { // MergeShip2: MM !CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI)
            gSaveContext.magic -= 2;
        }
        if (gSaveContext.magic <= 0) {
            gSaveContext.magic = 0;
        }
        gMergeShipGoron.magicDrain = 2;
    }

    // MAGIC_STATE_CONSUME_GORON_ZORA
    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->transitionMode == TRANS_MODE_OFF) && !Play_InCsMode(play)) {
        gMergeShipGoron.magicTimer--;
        if (gMergeShipGoron.magicTimer == 0) {
            if (!MergeShip_Chateau_IsActive()) { // MergeShip2: MM, Chateau Romani's effect takes nothing
                gSaveContext.magic--;
            }
            if (gSaveContext.magic <= 0) {
                gSaveContext.magic = 0;
            }
            gMergeShipGoron.magicTimer = 10;
        }
    }
}

static void MergeShip_Goron_MagicReset(PlayState* play) {
    gMergeShipGoron.magicDrain = 0;
    Magic_Reset(play);
}

// ---- MM EffectTireMark (z_eff_tire_mark.c) ------------------------------------------------------------
// Ocarina of Time's effect system has no tire mark type, so the Goron keeps its one tire mark here (MM
// Player::meleeWeaponEffectIndex[2]) and updates and draws it itself.

#define GORON_TIRE_MARK_ELEMENT_FLAG_1 (1 << 0)
#define GORON_TIRE_MARK_ELEMENT_FLAG_2 (1 << 1)

typedef struct GoronTireMarkElement {
    u16 flags;
    Vec3s p1;
    Vec3s p2;
    s16 life;
    CollisionPoly* colPoly;
} GoronTireMarkElement;

typedef struct GoronTireMark {
    GoronTireMarkElement elements[64];
    s16 unk600;
    s16 numElements;
    s16 elemDuration;
    Color_RGBA8 color;
} GoronTireMark;

static GoronTireMark sGoronTireMark;

// MM func_800BFD84 (z_bgcheck.c): the y of a poly's plane at (x, z).
static f32 MergeShip_Goron_func_800BFD84(CollisionPoly* poly, f32 arg1, f32 arg2) {
    return (COLPOLY_GET_NORMAL(poly->normal.x * arg1 + poly->normal.z * arg2) + poly->dist) /
           COLPOLY_GET_NORMAL(-poly->normal.y);
}

// MM func_800BFDEC (z_bgcheck.c): the vertices two polys share.
static s32 MergeShip_Goron_func_800BFDEC(CollisionPoly* polyA, CollisionPoly* polyB, u32* outVtxId0,
                                         u32* outVtxId1) {
    s32 vtxIdA[3];
    s32 vtxIdB[3];
    s32 i;
    s32 j;
    s32 count;

    *outVtxId0 = *outVtxId1 = 0;
    for (i = 0; i < ARRAY_COUNT(vtxIdA); i++) {
        vtxIdA[i] = COLPOLY_VTX_INDEX(polyA->vtxData[i]);
        vtxIdB[i] = COLPOLY_VTX_INDEX(polyB->vtxData[i]);
    }

    count = 0;
    for (i = 0; i < 2; i++) {
        for (j = i + 1; j < 3; j++) {
            if (vtxIdA[i] == vtxIdB[j]) {
                if (count == 0) {
                    *outVtxId0 = vtxIdA[i];
                } else if (count == 1) {
                    *outVtxId1 = vtxIdA[i];
                }
                count++;
            }
        }
    }
    return count;
}

// MM Math3D_LineSegMakePerpLineSeg (sys_math3d.c).
static s32 MergeShip_Goron_LineSegMakePerpLineSeg(Vec3f* lineAPointA, Vec3f* lineAPointB, Vec3f* lineBPointA,
                                                  Vec3f* lineBPointB, Vec3f* lineAClosestToB,
                                                  Vec3f* lineBClosestToA) {
    f32 magSq;
    f32 scaleB;
    f32 lineAx = lineAPointB->x - lineAPointA->x;
    f32 lineAy = lineAPointB->y - lineAPointA->y;
    f32 lineAz = lineAPointB->z - lineAPointA->z;
    f32 lineBx = lineBPointB->x - lineBPointA->x;
    f32 lineBy = lineBPointB->y - lineBPointA->y;
    f32 lineBz = lineBPointB->z - lineBPointA->z;
    f32 compAAlongB;
    f32 compBAAlongB;
    Vec3f lineAPerpB;
    Vec3f lineBAPerpB;
    f32 tA;
    f32 tB;

    magSq = SQ(lineBx) + SQ(lineBy) + SQ(lineBz);
    if (IS_ZERO(magSq)) {
        return false;
    }

    scaleB = 1.0f / magSq;

    compAAlongB = ((lineAx * lineBx) + (lineAy * lineBy) + (lineAz * lineBz)) * scaleB;

    compBAAlongB = ((lineBx * (lineAPointA->x - lineBPointA->x)) + (lineBy * (lineAPointA->y - lineBPointA->y)) +
                    (lineBz * (lineAPointA->z - lineBPointA->z))) *
                   scaleB;

    lineAPerpB.x = lineAx - (lineBx * compAAlongB);
    lineAPerpB.y = lineAy - (lineBy * compAAlongB);
    lineAPerpB.z = lineAz - (lineBz * compAAlongB);

    magSq = SQ(lineAPerpB.x) + SQ(lineAPerpB.y) + SQ(lineAPerpB.z);
    if (IS_ZERO(magSq)) {
        return false;
    }

    lineBAPerpB.x = (lineAPointA->x - lineBPointA->x) - (lineBx * compBAAlongB);
    lineBAPerpB.y = (lineAPointA->y - lineBPointA->y) - (lineBy * compBAAlongB);
    lineBAPerpB.z = (lineAPointA->z - lineBPointA->z) - (lineBz * compBAAlongB);

    tA = -((lineAPerpB.x * lineBAPerpB.x) + (lineAPerpB.y * lineBAPerpB.y) + (lineAPerpB.z * lineBAPerpB.z)) / magSq;
    lineAClosestToB->x = (lineAx * tA) + lineAPointA->x;
    lineAClosestToB->y = (lineAy * tA) + lineAPointA->y;
    lineAClosestToB->z = (lineAz * tA) + lineAPointA->z;

    tB = (compAAlongB * tA) + compBAAlongB;
    lineBClosestToA->x = (lineBx * tB) + lineBPointA->x;
    lineBClosestToA->y = (lineBy * tB) + lineBPointA->y;
    lineBClosestToA->z = (lineBz * tB) + lineBPointA->z;

    return true;
}

// MM func_800AEF44.
static void MergeShip_Goron_func_800AEF44(GoronTireMark* this) {
    if (this->numElements > 0) {
        GoronTireMarkElement* elem = &this->elements[this->numElements - 1];

        elem->flags |= GORON_TIRE_MARK_ELEMENT_FLAG_2;
    }
}

// MM EffectTireMark_InitElement.
static void MergeShip_Goron_TireMarkInitElement(GoronTireMarkElement* elem) {
    elem->flags = 0;
    elem->life = 0;
    elem->colPoly = NULL;
    elem->p1.x = elem->p1.y = elem->p1.z = elem->p2.x = elem->p2.y = elem->p2.z = 0;
}

// MM EffectTireMark_Init, with MM Player_Init's D_8085D330 = { 0, 63, sTireMarkGoronColor }.
static void MergeShip_Goron_TireMarkInit(GoronTireMark* this) {
    static Color_RGBA8 sTireMarkGoronColor = { 0, 0, 15, 100 }; // MM D_8085D338
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->elements); i++) {
        MergeShip_Goron_TireMarkInitElement(&this->elements[i]);
    }

    this->unk600 = 0;
    this->numElements = 0;
    this->elemDuration = 62; // MM clamps the requested 63 to 62
    this->color = sTireMarkGoronColor;
}

// MM func_800AE930.
static void MergeShip_Goron_func_800AE930(CollisionContext* colCtx, GoronTireMark* this, Vec3f* pos, f32 arg3,
                                          s16 angle, CollisionPoly* colPoly, s32 bgId) {
    Vec3s spB8;
    Vec3s spB0;
    GoronTireMarkElement* spAC;
    GoronTireMarkElement* spA8;
    u32 spA4;
    u32 spA0;
    Vec3s* vtxList = colCtx->colHeader->vtxList;

    if ((bgId != BGCHECK_SCENE) || (this->numElements >= (ARRAY_COUNT(this->elements) - 1)) || (colPoly == NULL)) {
        MergeShip_Goron_func_800AEF44(this);
        return;
    }

    spB8.x = (Math_SinS(angle - 0x4000) * arg3) + pos->x;
    spB8.z = (Math_CosS(angle - 0x4000) * arg3) + pos->z;
    spB8.y = MergeShip_Goron_func_800BFD84(colPoly, spB8.x, spB8.z) + 2.0f;

    spB0.x = (Math_SinS(angle + 0x4000) * arg3) + pos->x;
    spB0.z = (Math_CosS(angle + 0x4000) * arg3) + pos->z;
    spB0.y = MergeShip_Goron_func_800BFD84(colPoly, spB0.x, spB0.z) + 2.0f;

    if (this->numElements == 0) {
        spA8 = &this->elements[0];
        spA8->flags = 0;
        spA8->p1 = spB0;
        spA8->p2 = spB8;
        spA8->life = this->elemDuration;
        spA8->colPoly = colPoly;
        this->numElements++;
        return;
    }

    spAC = &this->elements[this->numElements - 1];

    if ((spAC->colPoly != NULL) && (colPoly != spAC->colPoly) &&
        (MergeShip_Goron_func_800BFDEC(spAC->colPoly, colPoly, &spA4, &spA0) == 2)) {
        Vec3f sp90;
        Vec3f sp84;
        Vec3f sp78;
        Vec3f sp6C;
        Vec3f sp60;
        Vec3f sp54;
        Vec3f sp48;
        Vec3f sp3C;
        Vec3f sp30;

        sp84.x = vtxList[spA4].x;
        sp84.y = vtxList[spA4].y;
        sp84.z = vtxList[spA4].z;
        sp90.x = vtxList[spA0].x;
        sp90.y = vtxList[spA0].y;
        sp90.z = vtxList[spA0].z;
        sp54.x = spAC->p1.x;
        sp54.y = spAC->p1.y;
        sp54.z = spAC->p1.z;
        sp60.x = spB0.x;
        sp60.y = spB0.y;
        sp60.z = spB0.z;
        sp3C.x = spAC->p2.x;
        sp3C.y = spAC->p2.y;
        sp3C.z = spAC->p2.z;
        sp48.x = spB8.x;
        sp48.y = spB8.y;
        sp48.z = spB8.z;

        if (MergeShip_Goron_LineSegMakePerpLineSeg(&sp84, &sp90, &sp54, &sp60, &sp6C, &sp30) &&
            MergeShip_Goron_LineSegMakePerpLineSeg(&sp84, &sp90, &sp3C, &sp48, &sp78, &sp30)) {
            if (!(spAC->flags & GORON_TIRE_MARK_ELEMENT_FLAG_2)) {
                spAC->flags |= GORON_TIRE_MARK_ELEMENT_FLAG_1;
            }

            spA8 = &this->elements[this->numElements];
            spA8->flags = 0;
            spA8->p1.x = sp6C.x;
            spA8->p1.y = sp6C.y;
            spA8->p1.z = sp6C.z;
            spA8->p2.x = sp78.x;
            spA8->p2.y = sp78.y;
            spA8->p2.z = sp78.z;
            spA8->life = this->elemDuration;
            spA8->colPoly = NULL;
            this->numElements++;

            spAC = &this->elements[this->numElements - 1];
            if (!(spAC->flags & GORON_TIRE_MARK_ELEMENT_FLAG_2)) {
                spAC->flags |= GORON_TIRE_MARK_ELEMENT_FLAG_1;
            }

            spA8 = &this->elements[this->numElements];
            spA8->flags = 0;
            spA8->p1 = spB0;
            spA8->p2 = spB8;
            spA8->life = this->elemDuration;
            spA8->colPoly = colPoly;
            this->numElements++;
            return;
        }

        if (!(spAC->flags & GORON_TIRE_MARK_ELEMENT_FLAG_2)) {
            spAC->flags |= GORON_TIRE_MARK_ELEMENT_FLAG_1;
        }

        spA8 = &this->elements[this->numElements];
        spA8->flags = 0;
        spA8->p1 = spB0;
        spA8->p2 = spB8;
        spA8->life = this->elemDuration;
        spA8->colPoly = colPoly;

        this->numElements++;
        return;
    }

    if (!(spAC->flags & GORON_TIRE_MARK_ELEMENT_FLAG_2)) {
        spAC->flags |= GORON_TIRE_MARK_ELEMENT_FLAG_1;
    }

    spA8 = &this->elements[this->numElements];
    spA8->flags = 0;
    spA8->p1 = spB0;
    spA8->p2 = spB8;
    spA8->life = this->elemDuration;
    spA8->colPoly = colPoly;

    this->numElements++;
}

// MM EffectTireMark_Update.
static void MergeShip_Goron_TireMarkUpdate(GoronTireMark* this) {
    s32 i;
    s32 j;

    if (this->numElements == 0) {
        return;
    }

    for (i = 0; i < this->numElements; i++) {
        this->elements[i].life--;
    }

    if (this->elements[0].life <= 0) {
        for (j = 0; j < ARRAY_COUNT(this->elements) - 1; j++) {
            this->elements[j] = this->elements[j + 1];
        }

        MergeShip_Goron_TireMarkInitElement(&this->elements[j]);

        this->numElements--;
        if (this->numElements < 0) {
            this->numElements = 0;
        }
    }
}

// MM EffectTireMark_SetVertices.
static void MergeShip_Goron_TireMarkSetVertices(Vtx* vtx, GoronTireMarkElement* elem, s32 i, s32 alpha) {
    s32 i1 = i * 2;
    s32 i2 = i * 2 + 1;

    vtx[i1].v.ob[0] = elem->p1.x;
    vtx[i1].v.ob[1] = elem->p1.y;
    vtx[i1].v.ob[2] = elem->p1.z;
    vtx[i1].v.flag = 0;
    vtx[i1].v.tc[0] = 2048;
    vtx[i1].v.tc[1] = 0;
    vtx[i1].v.cn[0] = 255;
    vtx[i1].v.cn[1] = 255;
    vtx[i1].v.cn[2] = 255;
    vtx[i1].v.cn[3] = alpha;

    vtx[i2].v.ob[0] = elem->p2.x;
    vtx[i2].v.ob[1] = elem->p2.y;
    vtx[i2].v.ob[2] = elem->p2.z;
    vtx[i2].v.flag = 0;
    vtx[i2].v.tc[0] = 2048;
    vtx[i2].v.tc[1] = 1024;
    vtx[i2].v.cn[0] = 255;
    vtx[i2].v.cn[1] = 255;
    vtx[i2].v.cn[2] = 255;
    vtx[i2].v.cn[3] = alpha;
}

// MM EffectTireMark_Draw.
static void MergeShip_Goron_TireMarkDraw(GoronTireMark* this, GraphicsContext* gfxCtx) {
    GoronTireMarkElement* elem;
    Vtx* vtx;
    f32 sp34;
    s32 i;

    sp34 = 1.0f / this->elemDuration;

    OPEN_DISPS(gfxCtx);

    if (this->numElements >= 2) {
        vtx = Graph_Alloc(gfxCtx, this->numElements * (2 * sizeof(Vtx)));

        if (vtx != NULL) {
            gSPMatrix(POLY_OPA_DISP++, &gMtxClear, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, 0x2C); // MM SETUPDL_44
            gDPSetRenderMode(POLY_OPA_DISP++, G_RM_PASS, G_RM_ZB_CLD_SURF2);

            gDPLoadTextureBlock(POLY_OPA_DISP++, mm_gameplay_keep_Tex_014570, G_IM_FMT_I, G_IM_SIZ_8b, 64, 32, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_WRAP, 6, 5, G_TX_NOLOD, G_TX_NOLOD);

            gDPSetCombineLERP(POLY_OPA_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED, 0, 0, 0,
                              COMBINED);
            gSPSetGeometryMode(POLY_OPA_DISP++, G_SHADE | G_SHADING_SMOOTH);
            gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);

            MergeShip_Goron_TireMarkSetVertices(vtx, &this->elements[0], 0, 255);

            vtx[0].v.tc[0] = 0;
            vtx[0].v.tc[1] = 0;
            vtx[1].v.tc[0] = 0;
            vtx[1].v.tc[1] = 1024;

            for (i = 1; i < this->numElements; i++) {
                elem = &this->elements[i];

                MergeShip_Goron_TireMarkSetVertices(vtx, elem, i, 255);

                if ((elem - 1)->flags & GORON_TIRE_MARK_ELEMENT_FLAG_1) {
                    if (!(elem->flags & GORON_TIRE_MARK_ELEMENT_FLAG_1)) {
                        vtx[i * 2 + 0].v.tc[0] = 0;
                        vtx[i * 2 + 0].v.tc[1] = 0;
                        vtx[i * 2 + 1].v.tc[0] = 0;
                        vtx[i * 2 + 1].v.tc[1] = 1024;
                    } else if ((i >= 2) && !((elem - 2)->flags & GORON_TIRE_MARK_ELEMENT_FLAG_1)) {
                        vtx[i * 2 - 2].v.tc[0] = 0;
                        vtx[i * 2 - 2].v.tc[1] = 0;
                        vtx[i * 2 - 1].v.tc[0] = 0;
                        vtx[i * 2 - 1].v.tc[1] = 1024;
                    }

                    if (i == (this->numElements - 1)) {
                        vtx[i * 2 + 0].v.tc[0] = 0;
                        vtx[i * 2 + 0].v.tc[1] = 0;
                        vtx[i * 2 + 1].v.tc[0] = 0;
                        vtx[i * 2 + 1].v.tc[1] = 1024;
                    }

                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, this->color.r, this->color.g, this->color.b,
                                    (s32)((f32)this->color.a * (f32)elem->life * sp34));
                    gSPVertex(POLY_OPA_DISP++, &vtx[i * 2 - 2], 4, 0);
                    gSP2Triangles(POLY_OPA_DISP++, 0, 1, 3, 0, 0, 3, 2, 0);
                }
            }
        }
    }

    CLOSE_DISPS(gfxCtx);
}

// ---- form setup -----------------------------------------------------------------------------------

// MM Player_InitCommon's Goron guard skeleton (Player::unk_2C8) and Player_Init's Goron tire mark and
// shield cylinder. Called from Player_InitCommon and whenever the player is rebuilt as a new form.
static Vec3s sGoronShieldingJointTable[LINK_GORON_SHIELDING_LIMB_MAX];
static Vec3s sGoronShieldingMorphTable[LINK_GORON_SHIELDING_LIMB_MAX];

// MM D_8085C318 (sShieldCylinderInit), with Hyrule's collider type names.
static ColliderCylinderInit sGoronShieldCylinderInit = {
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

void MergeShip_Goron_InitCommon(Player* this, PlayState* play) {
    // (Dark Link is built by the same Player_InitCommon; the Goron's single-instance state is the player's.)
    if ((this->mergeShipForm == PLAYER_FORM_DEKU) && (this->actor.id == ACTOR_PLAYER)) {
        // MM Player_Init adds the tire mark for every form; the Deku's spin lays it (MM Player_Action_95), in
        // MM's non-Goron colour D_8085D33C.
        static Color_RGBA8 sTireMarkNonGoronColor = { 0, 0, 0, 150 }; // MM D_8085D33C

        MergeShip_Goron_TireMarkInit(&sGoronTireMark);
        sGoronTireMark.color = sTireMarkNonGoronColor;
        gMergeShipGoron.tireMarkActive = true;
        return;
    }
    if ((this->mergeShipForm != PLAYER_FORM_GORON) || (this->actor.id != ACTOR_PLAYER)) {
        return;
    }

    SkelAnime_InitFlex(play, &this->unk_2C8, (FlexSkeletonHeader*)mm_gLinkGoronShieldingSkel,
                       (AnimationHeader*)mm_gLinkGoronShieldingAnim, sGoronShieldingJointTable,
                       sGoronShieldingMorphTable, LINK_GORON_SHIELDING_LIMB_MAX);

    Collider_InitCylinder(play, &this->shieldCylinder);
    Collider_SetCylinder(play, &this->shieldCylinder, &this->actor, &sGoronShieldCylinderInit);

    MergeShip_Goron_TireMarkInit(&sGoronTireMark);
    gMergeShipGoron.tireMarkActive = true;
    gMergeShipGoron.meleeWeaponAnimation = -1;
    MergeShip_Camera_SetMMMeleeWeaponAnimation(-1);
    gMergeShipGoron.bgCheckFlag800 = false;
    gMergeShipGoron.magicDrain = 0;
    gMergeShipGoron.ocarinaStaffPos = -1;

    // MM Player_Init: only the Goron presses heavy switches (ACTOR_FLAG_CAN_PRESS_HEAVY_SWITCHES); Hyrule has
    // no such switch, so only the ordinary switch flag (set by the transformation code) applies.
}

// MM Player_SetAction, the parts that concern the Goron: leaving the roll (Player_Action_96) restores the
// shadow, ends the spike magic drain and rights the player; a Goron that leaves any action with its fists
// "drawn" (the B item) puts them away at once; and MM's per-action working values are cleared.
void MergeShip_Goron_OnSetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc) {
    s32 i;

    if (this->mergeShipForm == PLAYER_FORM_DEKU) {
        MergeShip_Goron_func_800AEF44(&sGoronTireMark); // MM Player_SetAction ends the Deku spin's tire mark too
        return;
    }
    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }

    if (MergeShip_Goron_Action_96 == this->actionFunc) {
        this->actor.shape.shadowDraw = ActorShadow_DrawFeet;
        this->actor.shape.shadowScale = this->ageProperties->shadowScale;
        this->unk_ABC = 0.0f;
        if (this->stateFlags3 & PLAYER_STATE3_80000) {
            MergeShip_Goron_MagicReset(play);
        }
        MergeShip_Goron_func_8082DD2C(play, this);
        this->actor.shape.rot.x = 0;
        this->actor.shape.rot.z = 0;
        gMergeShipGoron.bgCheckFlag800 = false;
    } else if (Player_GetMeleeWeaponHeld(this) != 0) {
        Player_UseItem(play, this, ITEM_NONE);
    }

    MergeShip_Goron_func_800AEF44(&sGoronTireMark);

    this->stateFlags3 &= ~(PLAYER_STATE3_200 | PLAYER_STATE3_2000 | PLAYER_STATE3_8000 | PLAYER_STATE3_10000 |
                           PLAYER_STATE3_20000 | PLAYER_STATE3_40000 | PLAYER_STATE3_80000 | PLAYER_STATE3_200000 |
                           PLAYER_STATE3_1000000 | PLAYER_STATE3_20000000);

    this->unk_B86[0] = 0;
    this->unk_B86[1] = 0;
    this->unk_B8A = 0;
    this->unk_B8C = 0;
    this->unk_B8E = 0;
    for (i = 0; i < ARRAY_COUNT(this->unk_B10); i++) {
        this->unk_B10[i] = 0.0f;
    }
    this->actor.shape.rot.z = 0;

    MergeShip_Goron_ResetCylinder(this);
}

// ---- the punch combo (B) ------------------------------------------------------------------------------

// MM unk_ADD is s8; Ocarina of Time's equivalent unk_845 is u8 (its sword code never stores -1).
#define GORON_UNK_ADD(this) ((s8)(this)->unk_845)

typedef struct {
    LinkAnimationHeader* unk_0; // start
    LinkAnimationHeader* unk_4; // end
    LinkAnimationHeader* unk_8; // end, locked on
    u8 unk_C;                   // active from
    u8 unk_D;                   // active until
} GoronAttackAnimInfo;

// MM sMeleeAttackAnimInfo[PLAYER_MWA_GORON_PUNCH_LEFT .. PLAYER_MWA_GORON_PUNCH_BUTT].
static GoronAttackAnimInfo sGoronAttackAnimInfo[3] = {
    { GORON_ANIM(gPlayerAnim_pg_punchA), GORON_ANIM(gPlayerAnim_pg_punchAend), GORON_ANIM(gPlayerAnim_pg_punchAendR),
      6, 8 },
    { GORON_ANIM(gPlayerAnim_pg_punchB), GORON_ANIM(gPlayerAnim_pg_punchBend), GORON_ANIM(gPlayerAnim_pg_punchBendR),
      12, 18 },
    { GORON_ANIM(gPlayerAnim_pg_punchC), GORON_ANIM(gPlayerAnim_pg_punchCend), GORON_ANIM(gPlayerAnim_pg_punchCendR),
      8, 14 },
};

static GoronAttackAnimInfo* MergeShip_Goron_AttackInfo(void) {
    s32 index = gMergeShipGoron.meleeWeaponAnimation - MM_PLAYER_MWA_GORON_PUNCH_LEFT;

    return &sGoronAttackAnimInfo[CLAMP(index, 0, 2)];
}

// MM D_8085D094[1].
static s8 sGoronPunchCombo[] = {
    MM_PLAYER_MWA_GORON_PUNCH_LEFT,
    MM_PLAYER_MWA_GORON_PUNCH_RIGHT,
    MM_PLAYER_MWA_GORON_PUNCH_BUTT,
};

// MM func_808335F4 (Goron branch).
static s32 MergeShip_Goron_func_808335F4(Player* this) {
    s32 unk_ADD = GORON_UNK_ADD(this);
    s32 meleeWeaponAnim = sGoronPunchCombo[CLAMP(unk_ADD, 0, 2)];

    if (unk_ADD != 0) {
        gMergeShipGoron.meleeWeaponAnimation = meleeWeaponAnim;
        if (unk_ADD >= 2) {
            this->unk_845 = (u8)-1;
        }
    }
    return meleeWeaponAnim;
}

// MM func_80833728 / func_8083375C for the Goron: D_8085D09C[PLAYER_MELEEWEAPON_NONE] = { DMG_GORON_PUNCH,
// 2, 2, 0, 0 } -- 2 damage for every punch.
static void MergeShip_Goron_func_8083375C(Player* this) {
    s32 i;

    for (i = 0; i < 2; i++) {
        this->meleeWeaponQuads[i].info.toucher.dmgFlags = MERGESHIP_GORON_DMG_PUNCH;
        this->meleeWeaponQuads[i].info.toucher.damage = 2;
        this->meleeWeaponQuads[i].info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    }
}

// MM func_80833864 for the Goron. The attack runs in Player_Action_808502D0 (MM Player_Action_84), whose
// Goron branch is MergeShip_Goron_Action_84.
static void MergeShip_Goron_func_80833864(PlayState* play, Player* this, s32 meleeWeaponAnim) {
    MergeShip_Goron_func_8083375C(this);
    Player_SetupAction(play, this, Player_Action_808502D0, 0);
    this->av2.actionVar2 = 0;

    // (MM: not FLIPSLASH_FINISH..ZORA_JUMPKICK_FINISH, true for every Goron punch.)
    func_80832318(this);

    if ((meleeWeaponAnim != gMergeShipGoron.meleeWeaponAnimation) || (GORON_UNK_ADD(this) >= 3)) {
        this->unk_845 = 0;
    }

    this->unk_845 = (u8)(GORON_UNK_ADD(this) + 1);
    if (GORON_UNK_ADD(this) >= 3) {
        meleeWeaponAnim += 2;
    }

    gMergeShipGoron.meleeWeaponAnimation = meleeWeaponAnim;
    // Hyrule's actors and camera read Player::meleeWeaponAnimation (see MergeShipGoron.h); MM's camera
    // distorts the screen for the butt punch from MM's value.
    this->meleeWeaponAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
    MergeShip_Camera_SetMMMeleeWeaponAnimation(meleeWeaponAnim);
    Player_AnimPlayOnceAdjusted(play, this, MergeShip_Goron_AttackInfo()->unk_0);
    this->unk_844 = this->skelAnime.animLength + 4.0f;

    Player_StartAnimMovement(play, this, 0x209); // MM ANIM_FLAG_1 | ANIM_FLAG_ENABLE_MOVEMENT | ANIM_FLAG_NOMOVE
    this->yaw = this->actor.shape.rot.y;
}

// MM func_808396B8, for the Goron: a punch needs the fists (the B item) and the ground.
static s32 MergeShip_Goron_func_808396B8(PlayState* play, Player* this) {
    if (!(this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        (((this->actor.category != ACTORCAT_PLAYER) && CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) ||
         ((Player_GetMeleeWeaponHeld(this) != 0) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
          sUseHeldItem))) {
        return true;
    }
    return false;
}

// MM Player_ActionHandler_7, for the Goron.
s32 MergeShip_Goron_ActionHandler_7(Player* this, PlayState* play) {
    if (!func_8083C6B8(play, this)) {
        if (MergeShip_Goron_func_808396B8(play, this)) {
            MergeShip_Goron_func_80833864(play, this, MergeShip_Goron_func_808335F4(this));
            // (MM: no Goron punch is a spin attack.)
        } else {
            return false;
        }
    }
    return true;
}

// MM func_8082FA5C for the Goron (the swing sound; MM also recolours the sword trail, which the Goron's
// fists never draw).
void MergeShip_Goron_func_8082FA5C(PlayState* play, Player* this, s32 meleeWeaponState) {
    if (this->meleeWeaponState == 0) {
        MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_IT_GORON_PUNCH_SWING);
        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_N);
    }

    this->meleeWeaponState = meleeWeaponState;
}

// MM func_8083FCF0.
static s32 MergeShip_Goron_func_8083FCF0(PlayState* play, Player* this, f32 arg2, f32 arg3, f32 arg4) {
    if (arg4 < this->skelAnime.curFrame) {
        func_80832318(this);
    } else if (arg2 <= this->skelAnime.curFrame) {
        this->stateFlags3 |= PLAYER_STATE3_2000000;
        MergeShip_Goron_func_8082FA5C(play, this, (arg3 <= this->skelAnime.curFrame) ? 1 : -1);
        return true;
    }
    return false;
}

// MM Player_Action_85: after a punch rebounds off a wall.
static void MergeShip_Goron_Action_85(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    Player_DecelerateToZero(this);

    if (this->skelAnime.curFrame >= 6.0f) {
        func_80839F90(this, play);
    }
}

// MM func_808400CC for the Goron (no rebound animation: the punch's own animation carries on).
static void MergeShip_Goron_func_808400CC(PlayState* play, Player* this) {
    if (Player_Action_80843188 != this->actionFunc) {
        u8 savedMovementFlags = this->skelAnime.movementFlags;

        MergeShip_Goron_func_8082DD2C(play, this);
        this->skelAnime.movementFlags = 0;
        Player_SetupAction(play, this, MergeShip_Goron_Action_85, 0);
        this->skelAnime.movementFlags = savedMovementFlags;
    }

    Player_RequestRumble(this, 180, 20, 100, SQ(0));
    this->linearVelocity = -18.0f;
}

// MM func_808401F4 for the Goron: a fist that strikes a wall shakes the screen and rebounds.
static s32 MergeShip_Goron_func_808401F4(PlayState* play, Player* this) {
    if (this->meleeWeaponState >= 1) {
        // (MM: meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H, true for every Goron punch.)
        if (!(this->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) &&
            !(this->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED)) {
            if (this->skelAnime.curFrame >= 2.0f) {
                CollisionPoly* poly;
                s32 bgId;
                Vec3f spC8;
                Vec3f pos;
                Vec3f spB0;
                Vec3f* temp_a0 = &this->meleeWeaponInfo[0].tip;
                f32 var_fv1;

                if (this->linearVelocity >= 0.0f) {
                    var_fv1 = Math_Vec3f_DistXYZAndStoreDiff(temp_a0, &this->unk_AF0[1], &spB0);
                    if (var_fv1 != 0.0f) {
                        var_fv1 = (var_fv1 + 10.0f) / var_fv1;
                    }

                    spC8.x = temp_a0->x + (spB0.x * var_fv1);
                    spC8.y = temp_a0->y + (spB0.y * var_fv1);
                    spC8.z = temp_a0->z + (spB0.z * var_fv1);
                    if (BgCheck_EntityLineTest1(&play->colCtx, &spC8, temp_a0, &pos, &poly, true, false, false, true,
                                                &bgId)) {
                        if (!SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId) &&
                            (SurfaceType_GetFloorType(&play->colCtx, poly, bgId) != 6) &&
                            !func_8002F9EC(play, &this->actor, poly, bgId, &pos)) {
                            func_80832630(play);
                            MergeShip_Goron_RequestQuakeAndRumble(play, this, MM_NA_SE_IT_HAMMER_HIT);
                            if (bgId != BGCHECK_SCENE) { // MergeShip option GoronBreaksBombWalls
                                MergeShip_Goron_RecordStruckBg(play, bgId);
                            }
                            // MM also calls Actor_SetPlayerImpact(PLAYER_IMPACT_BONK), which only Termina's
                            // actors read.
                            // MM: the crater mark (ACTOR_EN_TEST, params 500), turned to the struck surface.
                            if (gMergeShipActorId_En_Test >= 0) {
                                MtxF sp64;
                                Vec3s actorRot;

                                MergeShip_func_800C0094(poly, pos.x, pos.y, pos.z, &sp64);
                                MergeShip_Matrix_MtxFToYXZRot(&sp64, &actorRot, true);
                                Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_En_Test, pos.x, pos.y, pos.z,
                                            actorRot.x, actorRot.y, actorRot.z, 500);
                            }

                            if (bgId != BGCHECK_SCENE) {
                                DynaPolyActor* temp_v0 = DynaPoly_GetActor(&play->colCtx, bgId);

                                if ((temp_v0 != NULL) &&
                                    (((this->meleeWeaponQuads[0].base.atFlags & AT_HIT) &&
                                      (&temp_v0->actor == this->meleeWeaponQuads[0].base.at)) ||
                                     ((this->meleeWeaponQuads[1].base.atFlags & AT_HIT) &&
                                      (&temp_v0->actor == this->meleeWeaponQuads[1].base.at)))) {
                                    return false;
                                }
                            }

                            MergeShip_Goron_func_808400CC(play, this);
                            return false;
                        }
                    }
                }
            }
        } else {
            MergeShip_Goron_func_808400CC(play, this);
            func_80832630(play);
            return true;
        }

        if ((this->meleeWeaponQuads[0].base.atFlags & AT_HIT) || (this->meleeWeaponQuads[1].base.atFlags & AT_HIT)) {
            // (MM skips the hit-stop freeze for the Goron; neither the Deku stick nor the Razor Sword applies.)
            if (this->actor.colChkInfo.atHitEffect == 1) {
                this->actor.colChkInfo.damage = 8;
                func_80837C0C(play, this, PLAYER_HIT_RESPONSE_ELECTRIFIED, 0.0f, 0.0f, this->actor.shape.rot.y, 20);
                return true;
            }
        }
    }

    return false;
}

// MM Player_Action_84, the Goron's punch.
void MergeShip_Goron_Action_84(Player* this, PlayState* play) {
    GoronAttackAnimInfo* attackInfoEntry = MergeShip_Goron_AttackInfo();

    if (this->skelAnime.curFrame < (this->skelAnime.endFrame - 6.0f)) {
        this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    }

    if (MergeShip_Goron_func_808401F4(play, this)) {
        return;
    }

    if (this->linearVelocity >= 0.0f) {
        MergeShip_Goron_func_8083FCF0(play, this, 5.0f, attackInfoEntry->unk_C, attackInfoEntry->unk_D);
    }

    if ((gMergeShipGoron.meleeWeaponAnimation == MM_PLAYER_MWA_GORON_PUNCH_LEFT) ||
        (gMergeShipGoron.meleeWeaponAnimation == MM_PLAYER_MWA_GORON_PUNCH_RIGHT)) {
        this->unk_3D0.unk_00 = 3;
    }

    if ((this->stateFlags2 & PLAYER_STATE2_SWORD_LUNGE) && LinkAnimation_OnFrame(&this->skelAnime, 0.0f)) {
        this->linearVelocity = 15.0f;
        this->stateFlags2 &= ~PLAYER_STATE2_SWORD_LUNGE;
    }

    if (this->linearVelocity > 12.0f) {
        func_8084269C(play, this);
    }

    Math_StepToF(&this->linearVelocity, 0.0f, 5.0f);
    func_8083C50C(this);

    if (LinkAnimation_Update(play, &this->skelAnime)) {
        sUseHeldItem = this->av2.actionVar2;

        if (!MergeShip_Goron_ActionHandler_7(this, play)) {
            LinkAnimationHeader* anim =
                Player_CheckHostileLockOn(this) ? attackInfoEntry->unk_8 : attackInfoEntry->unk_4;
            u8 savedMovementFlags;

            func_80832318(this);

            savedMovementFlags = this->skelAnime.movementFlags;
            this->skelAnime.movementFlags = 0;
            Player_SetupAction(play, this, Player_Action_Idle, 1);
            LinkAnimation_PlayOnceSetSpeed(play, &this->skelAnime, anim, sWaterSpeedFactor);
            this->yaw = this->actor.shape.rot.y;
            this->skelAnime.movementFlags = savedMovementFlags;
            this->stateFlags3 |= PLAYER_STATE3_FINISHED_ATTACKING;
        }
    } else if (gMergeShipGoron.meleeWeaponAnimation != MM_PLAYER_MWA_GORON_PUNCH_BUTT) {
        this->av2.actionVar2 |= CHECK_BTN_ALL(sControlInput->press.button, BTN_B) ? 1 : 0;
    }
}

// ---- the guard (R) ----------------------------------------------------------------------------------

// MM Player_ActionHandler_11, for the Goron: always allowed and with no animation of its own (MM draws the
// guard skeleton unk_2C8 instead), with MM func_80830AE8's squat sound.
s32 MergeShip_Goron_ActionHandler_11(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R) && (this->unk_6AD == 0) &&
        (play->shootingGalleryStatus == 0)) {
        func_80832318(this);
        Player_DetachHeldActor(play, this);
        if (Player_SetupAction(play, this, Player_Action_80843188, 0)) {
            this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_SQUAT); // MM func_80830AE8
        }
        return true;
    }
    return false;
}

// MM Player_Action_18, the Goron's branch.
void MergeShip_Goron_Action_18(Player* this, PlayState* play) {
    Player_DecelerateToZero(this);

    SkelAnime_Update(&this->unk_2C8);

    if (!func_80842964(this, play)) {
        if (!MergeShip_Goron_ActionHandler_11(this, play)) {
            this->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;

            if (this->itemAction < 0) {
                func_8008EC70(this);
            }

            func_8083A098(this, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_defense_end, this->modelAnimType), play);
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BALL_TO_GORON); // MM func_80830B38
        } else {
            this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        }
    }
}

// ---- curling up and rolling (A) -----------------------------------------------------------------------

// MM func_80834CD0.
static void MergeShip_Goron_func_80834CD0(Player* this, f32 arg1, u16 mmSfxId) {
    this->actor.velocity.y = arg1 * sWaterSpeedFactor;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;

    if (mmSfxId != 0) {
        MergeShip_PlayMMFloorJumpSfx(this);
        MergeShip_PlayMMVoiceSfx(this, mmSfxId);
    }

    this->stateFlags1 |= PLAYER_STATE1_JUMPING;
    this->fallStartHeight = this->actor.world.pos.y;
}

// MM func_8083A4A4.
static s32 MergeShip_Goron_func_8083A4A4(Player* this, f32* speedTarget, s16* yawTarget, f32 decelerationRate) {
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

// MM func_8083F828.
static void MergeShip_Goron_func_8083F828(Vec3f* arg0, Vec3f* arg1, f32 arg2, f32 arg3, f32 arg4) {
    arg1->x = Rand_CenteredFloat(arg3) + arg0->x;
    arg1->y = Rand_CenteredFloat(arg4) + (arg0->y + arg2);
    arg1->z = Rand_CenteredFloat(arg3) + arg0->z;
}

// MM func_8083F8A8: dust (or snow, or grass) kicked up by the roll.
static s32 MergeShip_Goron_func_8083F8A8(PlayState* play, Player* this, f32 radius, s32 countMax, f32 randAccelWeight,
                                         s32 scale, s32 scaleStep, s32 useLighting) {
    static Color_RGBA8 D_8085D26C = { 255, 255, 255, 255 };
    static Vec3f D_8085D270 = { 0.0f, 0.04f, 0.0f };
    static Vec3f D_8085D27C = { 0.0f, 0.0f, 0.0f };
    static Vec3f D_8085D288 = { 0.0f, 0.0f, 0.0f };

    if ((countMax < 0) || (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_SNOW))) {
        s32 count = GORON_FRAME_BUDGET_NS / 20000000;
        Vec3f pos;
        s32 i;

        count = (count >= ABS(countMax)) ? ABS(countMax) : count;
        for (i = 0; i < count; i++) {
            MergeShip_Goron_func_8083F828(&this->actor.world.pos, &pos, 0.0f, 40.0f, 10.0f);
            D_8085D27C.x = Rand_CenteredFloat(3.0f);
            D_8085D27C.z = Rand_CenteredFloat(3.0f);
            EffectSsDust_Spawn(play, 0, &pos, &D_8085D27C, &D_8085D270, &D_8085D26C, &D_8085D26C, scale, scaleStep,
                               42, 0);
        }

        return true;
    } else if ((this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_GROUND)) ||
               (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_SAND))) {
        s32 count = GORON_FRAME_BUDGET_NS / 12000000;

        if (count > 0) {
            Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, radius,
                                     (count < countMax) ? count : countMax, randAccelWeight, scale, scaleStep,
                                     useLighting);
            return true;
        }
    } else if (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_GRASS)) {
        s32 count = GORON_FRAME_BUDGET_NS / 12000000;
        Vec3f velocity;
        Vec3f pos;
        s32 i;

        count = (count >= countMax) ? countMax : count;
        for (i = 0; i < count; i++) {
            MergeShip_Goron_func_8083F828(&this->actor.world.pos, &pos, 0.0f, 20.0f, 20.0f);
            velocity.x = Rand_CenteredFloat(3.0f);
            velocity.y = Rand_ZeroFloat(2.0f);
            velocity.z = Rand_CenteredFloat(3.0f);
            D_8085D288.y = -0.1f;
            EffectSsHahen_Spawn(play, &pos, &velocity, &D_8085D288, 0, 0x96, 1, 0x10, (Gfx*)mm_gKakeraLeafTipDL);
        }
    }

    return false;
}

// MM func_80840A30: bonking into a wall, or into a tree or a crate, at speed.
static s32 MergeShip_Goron_func_80840A30(PlayState* play, Player* this, f32* arg2, f32 arg3) {
    Actor* cylinderOc = NULL;

    if (arg3 <= *arg2) {
        if (((this->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) && (sWorldYawToTouchedWall < 0x1C00)) ||
            (((this->cylinder.base.ocFlags1 & OC1_HIT) && ((cylinderOc = this->cylinder.base.oc) != NULL)) &&
             // (MM also bonks Termina's beaver-race rings, snowy trees and ObjTree; Hyrule's trees are EnWood02.)
             ((cylinderOc->id == ACTOR_EN_WOOD02) &&
              (ABS((s16)(this->actor.world.rot.y - cylinderOc->yawTowardsPlayer)) > 0x6000)))) {

            if (!MergeShip_Goron_func_8082DA90(play)) {
                // (MM's PLAYER_DOORTYPE_STAIRCASE does not exist in Hyrule.)
                if (cylinderOc != NULL) {
                    cylinderOc->home.rot.y = 1;
                } else if (this->actor.wallBgId != BGCHECK_SCENE) {
                    DynaPolyActor* wallPolyActor = DynaPoly_GetActor(&play->colCtx, this->actor.wallBgId);

                    // Large crates (MM also breaks its barrels and palm trees).
                    if ((wallPolyActor != NULL) && (wallPolyActor->actor.id == ACTOR_OBJ_KIBAKO2)) {
                        wallPolyActor->actor.home.rot.z = 1;
                    }
                }

                // (The roll keeps rolling: MM only changes action when not curled up.)

                this->linearVelocity = -this->linearVelocity;
                Player_RequestQuake(play, 33267, 3, 12);
                Player_RequestRumble(this, 255, 20, 150, SQ(0));
                // MM Actor_SetPlayerImpact(PLAYER_IMPACT_BONK): only Termina's actors read it.
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BODY_HIT);
                MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
                return true;
            }
        }
    }
    return false;
}

// MM func_80836AD8.
static void MergeShip_Goron_func_80836AD8(PlayState* play, Player* this) {
    Player_SetupAction(play, this, MergeShip_Goron_Action_96, 0);
    this->unk_B28 = 0;
    this->unk_B86[1] = 0;
    this->unk_AF0[0].x = 0.0f;
    this->unk_AF0[0].y = 0.0f;
    this->unk_AF0[0].z = 0.0f;
    this->unk_B08 = 0.0f;
    this->unk_B0C = 0.0f;
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_TO_BALL);
}

// MM func_80836B3C, the Goron's branch: where a human rolls, the Goron curls up and rolls away.
void MergeShip_Goron_func_80836B3C(PlayState* play, Player* this) {
    // MM (2S2H VB_PATCH_SIDEROLL, vanilla true).
    this->yaw = this->actor.shape.rot.y;
    this->actor.world.rot.y = this->actor.shape.rot.y;

    MergeShip_Goron_func_80836AD8(play, this);
    LinkAnimation_Change(play, &this->skelAnime, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_landing_roll, this->modelAnimType),
                         1.5f * sWaterSpeedFactor, 0.0f, 6.0f, ANIMMODE_ONCE, 0.0f);
}

// MM func_80839F98: A as a Goron curls up (rolling off at once when already moving).
static s32 MergeShip_Goron_func_80839F98(PlayState* play, Player* this) {
    if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        if (this->linearVelocity != 0.0f) {
            MergeShip_Goron_func_80836B3C(play, this);
            return true;
        }
        MergeShip_Goron_func_80836AD8(play, this);
        LinkAnimation_Change(play, &this->skelAnime, GORON_ANIM(gPlayerAnim_pg_maru_change), PLAYER_ANIM_ADJUSTED_SPEED,
                             0.0f, 7.0f, ANIMMODE_ONCE, 0.0f);
        return true;
    }
    return false;
}

// MM Player_ActionHandler_6, for the Goron.
s32 MergeShip_Goron_ActionHandler_6(Player* this, PlayState* play) {
    if (!sUpperBodyIsBusy && !(this->stateFlags1 & PLAYER_STATE1_ON_HORSE) && !Player_UpdateHostileLockOn(this)) {
        if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A) && !Player_UpdateHostileLockOn(this)) {
            if (MergeShip_Goron_func_80839F98(play, this)) {
                return true;
            }

            if ((this->putAwayCooldownTimer == 0) && (this->heldItemAction >= PLAYER_IA_SWORD_MASTER)) {
                Player_UseItem(play, this, ITEM_NONE);
            } else {
                this->stateFlags2 ^= PLAYER_STATE2_NAVI_ACTIVE;
            }
        }
    }

    return false;
}

// MM func_80857640: B while rolling -- the leap before a ground pound.
static void MergeShip_Goron_func_80857640(Player* this, f32 arg1, s32 arg2) {
    MergeShip_Goron_func_80834CD0(this, arg1, MM_NA_SE_VO_LI_SWORD_N);
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_BALLJUMP);
    MergeShip_Goron_StopHorizontalMovement(this);
    if (this->av2.actionVar2 < arg2) {
        this->av2.actionVar2 = arg2;
    }
    this->av1.actionVar1 = 1;
    this->unk_B48 = 1.0f;
}

// MM func_808576BC: skidding.
static void MergeShip_Goron_func_808576BC(PlayState* play, Player* this) {
    s32 var_v0 =
        ((this->actor.velocity.z * Math_CosS(this->yaw)) + (this->actor.velocity.x * Math_SinS(this->yaw))) * 800.0f;

    var_v0 -= this->av2.actionVar2;
    var_v0 = ABS(var_v0);

    if (var_v0 <= 0x7D0) {
        return;
    }

    if (var_v0 > 0x1770) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_SLIP - SFX_FLAG);
    }

    if (MergeShip_Goron_func_8083F8A8(play, this, 12.0f, -1 - (var_v0 >> 0xC), (var_v0 >> 0xA) + 1.0f,
                                      (var_v0 >> 7) + 160, 20, true)) {
        MergeShip_PlayMMSfxActor(&this->actor, (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_SNOW))
                                                   ? MM_NA_SE_PL_ROLL_SNOW_DUST - GORON_SFX_FLAG
                                                   : MM_NA_SE_PL_ROLL_DUST - GORON_SFX_FLAG);
    }
}

// MM func_808577E0: the ball's squash as it rolls.
static void MergeShip_Goron_func_808577E0(Player* this) {
    f32 temp_fa1 = ABS(this->av2.actionVar2) * 0.00004f;

    if (this->unk_ABC < temp_fa1) {
        this->unk_B48 += 0.08f;
    } else {
        this->unk_B48 += -0.07f;
    }

    this->unk_B48 = CLAMP(this->unk_B48, -0.2f, 0.14f);
    if (fabsf(this->unk_B48) < 0.12f) {
        if (Math_StepUntilF(&this->unk_ABC, temp_fa1, this->unk_B48)) {
            this->unk_B48 = 0.0f;
        }
    } else {
        this->unk_ABC += this->unk_B48;
        this->unk_ABC = CLAMP(this->unk_ABC, -0.7f, 0.3f);
    }
}

// MM func_80857950: releasing A (or falling after a pound) uncurls.
static s32 MergeShip_Goron_func_80857950(PlayState* play, Player* this) {
    if (((this->unk_B86[1] == 0) && !CHECK_BTN_ALL(sControlInput->cur.button, BTN_A)) ||
        ((this->av1.actionVar1 == 3) && (this->actor.velocity.y < 0.0f))) {
        Player_SetupAction(play, this, Player_Action_Idle, 1);
        Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
        LinkAnimation_Change(play, &this->skelAnime, GORON_ANIM(gPlayerAnim_pg_maru_change),
                             -PLAYER_ANIM_ADJUSTED_SPEED, 7.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BALL_TO_GORON);
        return true;
    }

    return false;
}

// MM func_80857A44: the curl animation ends and the ball starts rolling.
static s32 MergeShip_Goron_func_80857A44(PlayState* play, Player* this) {
    if (LinkAnimation_Update(play, &this->skelAnime)) {
        Player_FinishAnimMovement(this);

        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        gMergeShipGoron.bgCheckFlag800 = true; // MM BGCHECKFLAG_PLAYER_800
        this->av1.actionVar1 = 4;
        this->actor.shape.shadowScale = 30.0f;
        this->av2.actionVar2 = this->linearVelocity * 500.0f;
        this->unk_B08 = this->linearVelocity;
        this->unk_B0C = 0.0f;
        this->actor.home.rot.y = this->yaw;

        return true;
    }

    return false;
}

static Vec3f sGoronZeroVec = { 0.0f, 0.0f, 0.0f }; // MM gZeroVec3f

// MM func_80857AEC: landing -- after a leap, the ground pound.
static void MergeShip_Goron_func_80857AEC(PlayState* play, Player* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->unk_B0C += this->unk_B08 * 0.05f;

        if (this->unk_B86[1] == 0) {
            if (this->av1.actionVar1 == 1) {
                this->av1.actionVar1 = 2;
                MergeShip_Goron_RequestQuakeAndRumble(play, this, MM_NA_SE_PL_GORON_PUNCH);
                play->actorCtx.unk_02 = 4;
                EffectSsBlast_SpawnWhiteShockwave(play, &this->actor.world.pos, &sGoronZeroVec, &sGoronZeroVec);
                this->av2.actionVar2 = 0;
                this->unk_B08 = 0.0f;
                // MM: the crater mark (ACTOR_EN_TEST, params 0), which finds and lies on the floor itself.
                if (gMergeShipActorId_En_Test >= 0) {
                    Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_En_Test, this->actor.world.pos.x,
                                this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, 0);
                }
            } else {
                this->av1.actionVar1 = 4;
            }
        }

        MergeShip_Goron_PlayFloorLand(this);
    }
}

// MergeShip2 (BUG-0011): Roc's Feather on a C button / the D-pad, curled up or rolling: the ball jumps (the roll's own
// bounce, MergeShip_Goron_func_80834CD0, as off a ledge), once per landing. (The roll never reaches the item buttons.)
static s32 MergeShip_Goron_RocsBallJump(PlayState* play, Player* this) {
    s32 i;

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    // z_player.c sItemButtons: B, the C buttons, the D-pad.
    for (i = 1; i < ARRAY_COUNT(sItemButtons); i++) {
        if (CHECK_BTN_ALL(sControlInput->press.button, sItemButtons[i]) &&
            (Player_GetItemOnButton(play, i) == ITEM_ROCS_FEATHER)) {
            MergeShip_Goron_func_80834CD0(this, 12.0f, 0);
            Player_PlaySfx(&this->actor, NA_SE_PL_SKIP);
            return true;
        }
    }
    return false;
}

// MM Player_Action_96: rolling.
void MergeShip_Goron_Action_96(Player* this, PlayState* play) {
    if (Player_TryActionHandlerList(play, this, sGoronActionHandlerList12, false)) {
        return;
    }
    MergeShip_Goron_RocsBallJump(play, this);

    if ((this->av1.actionVar1 == 0) && !MergeShip_Goron_func_80857A44(play, this)) {
        return;
    }

    this->stateFlags3 |= PLAYER_STATE3_1000;
    MergeShip_Goron_func_808577E0(this);

    if (!MergeShip_Goron_func_80857950(play, this)) {
        f32 speedTarget = 0.0f;
        s16 yawTarget = this->yaw;
        u16 spE0;
        s32 spDC;
        s32 spD8;

        // (MM passes 0 for its staircase doors, which Hyrule does not have.)
        if (MergeShip_Goron_func_80840A30(play, this, &this->unk_B08, 12.0f)) {
            if (MergeShip_Goron_Action_96 != this->actionFunc) {
                return;
            }

            this->linearVelocity *= 0.1f;
            MergeShip_Goron_func_80834CD0(this, 10.0f, 0);
            if (this->unk_B86[1] != 0) {
                this->unk_B86[1] = 0;
                this->av1.actionVar1 = 3;
            }
        } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && (this->unk_B08 >= 12.0f)) {
            s16 temp_v0 = this->yaw - (s16)(this->actor.wallYaw + 0x8000);
            s16 temp_v2;
            s32 var_a2 = ABS(temp_v0);

            this->unk_B0C += this->unk_B08 * 0.05f;
            temp_v2 = ((temp_v0 >= 0) ? 1 : -1) * ((var_a2 + 0x100) & ~0x1FF);
            this->yaw += (s16)(0x8000 - (s16)(temp_v2 * 2));
            this->actor.home.rot.y = this->yaw;
            this->actor.shape.rot.y = this->yaw;

            this->unk_B8C = 4;
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_GORON_ROLLING_REFLECTION);
        }

        this->stateFlags2 |= (PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS);

        if (this->unk_B8E != 0) {
            this->unk_B8E--;
        } else {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
            speedTarget *= 2.6f;
        }

        if (this->unk_B8C != 0) {
            this->unk_B8C--;
            yawTarget = this->yaw;
        }

        { void MergeShipTest_Log(const char* fmt, ...); MergeShipTest_Log("CAMDBG roll av1=%d av2=%d B86=%d B08=%.2f magic=%d mstate=%d s3=%08X", this->av1.actionVar1, this->av2.actionVar2, this->unk_B86[1], this->unk_B08, gSaveContext.magic, gSaveContext.magicState, this->stateFlags3); } // CAMDBG TEMP

        if (this->unk_B86[1] != 0) {
            speedTarget = 18.0f;
            // MM Math_StepToC(&actionVar1, 4, 1).
            if (this->av1.actionVar1 < 4) {
                this->av1.actionVar1++;
            } else if (this->av1.actionVar1 > 4) {
                this->av1.actionVar1--;
            }

            if ((this->stateFlags3 & PLAYER_STATE3_80000) &&
                (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_A) || (gSaveContext.magic == 0) ||
                 ((this->av1.actionVar1 == 4) && (this->unk_B08 < 12.0f)))) {
                if (Math_StepToS(&this->unk_B86[1], 0, 1)) {
                    this->stateFlags3 &= ~PLAYER_STATE3_80000;
                    MergeShip_Goron_MagicReset(play);
                    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_BALL_CHARGE_FAILED);
                }
                this->av1.actionVar1 = 4;
            } else if (this->unk_B86[1] < 7) {
                if (!(this->stateFlags3 & PLAYER_STATE3_80000)) {
                    this->unk_3D0.unk_00 = 2;
                }
                this->unk_B86[1]++;
            }
        }

        spDC = speedTarget * 900.0f;

        Math_AsymStepToF(&this->unk_B10[0], (this->unk_B8A != 0) ? 1.0f : 0.0f, 0.8f, 0.05f);
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            MergeShip_Goron_func_80857AEC(play, this);
            if (this->av1.actionVar1 == 2) {
                if (this->unk_B8A == 0) {
                    this->av1.actionVar1 = 4;
                } else {
                    this->unk_B8A--;
                    this->unk_ABC = 0.0f;
                    this->unk_B48 = 0.14f;
                }
            } else if ((this->unk_B86[1] == 0) && CHECK_BTN_ALL(sControlInput->press.button, BTN_B) &&
                       (gSaveContext.equips.buttonItems[0] != ITEM_NONE) &&
                       (gSaveContext.equips.buttonItems[0] != ITEM_NONE_FE) &&
                       (gSaveContext.equips.buttonItems[0] != ITEM_MM_FD)) {
                // MM: `Inventory_GetBtnBItem(play) < ITEM_FD`.
                MergeShip_Goron_func_80857640(this, 14.0f, 0x1F40);
            } else {
                f32 spCC;
                s16 spCA;
                s16 spC8;
                s32 var_a0;
                s32 spC0;
                f32 spBC;
                f32 spB8;
                f32 spB4;
                f32 spB0;
                f32 spAC;
                f32 spA8;
                f32 spA4;
                f32 spA0;
                Vec3f slopeNormal;
                s16 downwardSlopeYaw;
                s16 sp90 = this->yaw;
                s16 sp8E = this->yaw - this->actor.home.rot.y;
                f32 sp88 = Math_CosS(sp8E);

                if (this->unk_B86[1] == 0) {
                    this->unk_B0C = 0.0f;
                    if (this->av1.actionVar1 >= 0x36) {
                        MergeShip_Goron_MagicConsume(play);
                        this->unk_B08 = 18.0f;
                        this->unk_B86[1] = 1;
                        this->stateFlags3 |= PLAYER_STATE3_80000;
                        MergeShip_PlayMMSfxAndFootstepFlag(this, MM_NA_SE_PL_GORON_BALL_CHARGE_DASH);
                    }
                } else {
                    this->unk_B0C = CLAMP(this->unk_B0C, 0.0f, 0.9f);
                }

                spBC = (1.0f - this->unk_B0C) * this->unk_B08 * sp88;
                if ((spBC < 0.0f) || ((speedTarget == 0.0f) && (ABS(sp8E) > 0xFA0))) {
                    spBC = 0.0f;
                }

                Math_StepToF(&this->unk_B0C, 0.0f, fabsf(sp88) * 20.0f);
                var_a0 = spBC * 500.0f;
                var_a0 = CLAMP_MIN(var_a0, 0);

                spC0 = (s32)(speedTarget * 400.0f) - var_a0;
                spC0 = CLAMP_MIN(spC0, 0);

                spDC = CLAMP_MIN(spDC, var_a0);

                spAC = spBC * Math_SinS(this->actor.home.rot.y);
                spA8 = spBC * Math_CosS(this->actor.home.rot.y);
                spB4 = this->unk_B08 * Math_SinS(this->yaw);
                spB0 = this->unk_B08 * Math_CosS(this->yaw);

                spA4 = spB4 - spAC;
                spA0 = spB0 - spA8;
                this->linearVelocity = spBC;
                this->yaw = this->actor.home.rot.y;
                spCC = speedTarget;
                spCA = yawTarget;

                if (MergeShip_Goron_func_8083A4A4(this, &spCC, &spCA, (this->av1.actionVar1 >= 5) ? 0.0f : 1.0f)) {
                    if (this->unk_B86[1] == 0) {
                        this->av1.actionVar1 = 4;
                    }

                    if (this->av1.actionVar1 == 4) {
                        spDC = -0xFA0;
                    }
                } else {
                    static Vec3f D_8085D978 = { -30.0f, 60.0f, 0.0f };
                    static Vec3f D_8085D984 = { 30.0f, 60.0f, 0.0f };
                    f32 sp84 = (((this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_SNOW)) ||
                                 (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_ICE)) ||
                                 (this->floorSfxOffset == GORON_FLOOR_SFX(MM_NA_SE_PL_WALK_SAND)) ||
                                 (sFloorType == 5)) &&
                                (spC0 >= 0x7D0))
                                   ? 0.08f
                                   : this->av2.actionVar2 * 0.0003f;
                    f32 sp80 = (Math_SinS(this->floorPitch) * 8.0f) + 0.6f;
                    s16 var_a3;
                    s16 sp7C;
                    Vec3f sp70;
                    f32 sp6C;
                    f32 var_fa1;

                    if (this->unk_B86[1] == 0) {
                        if ((gSaveContext.magicState == MAGIC_STATE_IDLE) && (gSaveContext.magic >= 2) &&
                            (this->av2.actionVar2 >= 0x36B0)) {
                            this->av1.actionVar1++;
                            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_GORON_BALL_CHARGE - SFX_FLAG);
                        } else {
                            this->av1.actionVar1 = 4;
                        }
                    }

                    if (speedTarget != spCC) {
                        this->yaw = yawTarget;
                    }

                    sp84 = CLAMP_MIN(sp84, 0.0f);
                    sp80 = CLAMP_MIN(sp80, 0.0f);

                    Math_AsymStepToF(&this->linearVelocity, speedTarget, sp84, sp80);
                    spC8 = (s16)(s32)(fabsf(this->actor.speedXZ) * 20.0f) + 300;
                    spC8 = CLAMP_MIN(spC8, 100);

                    sp7C = (s32)((s16)(yawTarget - this->yaw) * -0.5f);
                    this->unk_B0C += (f32)(SQ(sp7C)) * 8e-9f;
                    Math_ScaledStepToS(&this->yaw, yawTarget, spC8);
                    sp6C = func_8083973C(play, this, &D_8085D978, &sp70);

                    var_fa1 = func_8083973C(play, this, &D_8085D984, &sp70) - sp6C;
                    if (fabsf(var_fa1) > 100.0f) {
                        var_fa1 = 0.0f;
                    }

                    var_a3 = Math_Atan2S(60.0f, var_fa1);
                    if (ABS(var_a3) > 0x2AAA) {
                        var_a3 = 0;
                    }

                    Math_ScaledStepToS(&this->actor.shape.rot.z, var_a3 + sp7C, spC8);
                }

                spBC = this->linearVelocity;
                this->actor.home.rot.y = this->yaw;
                this->yaw = sp90;
                MergeShip_Goron_GetSlopeDirection(this->actor.floorPoly, &slopeNormal, &downwardSlopeYaw);

                spB8 = sqrtf(SQ(spA4) + SQ(spA0));
                if (this->unk_B86[1] != 0) {
                    if ((ABS(sp8E) + ABS(this->floorPitch)) > 0x3A98) {
                        this->unk_B86[1] = 0;
                        this->av1.actionVar1 = 4;
                        this->unk_B8E = 0x14;
                        this->av2.actionVar2 = 0;
                        this->stateFlags3 &= ~PLAYER_STATE3_80000;
                        MergeShip_Goron_MagicReset(play);
                    }
                } else {
                    f32 temp_ft4_2 = (0.6f * slopeNormal.x) + spA4;
                    f32 temp_ft5 = (0.6f * slopeNormal.z) + spA0;
                    f32 temp_fv0_3 = sqrtf(SQ(temp_ft4_2) + SQ(temp_ft5));

                    if ((temp_fv0_3 < spB8) || (temp_fv0_3 < 6.0f)) {
                        spA4 = temp_ft4_2;
                        spA0 = temp_ft5;
                        spB8 = temp_fv0_3;
                    }
                }

                if (spB8 != 0.0f) {
                    f32 sp54 = spB8 - 0.3f;

                    sp54 = CLAMP_MIN(sp54, 0.0f);

                    spB8 = sp54 / spB8;

                    spA4 *= spB8;
                    spA0 *= spB8;

                    if (sp54 != 0.0f) {
                        this->unk_B28 = Math_Atan2S(spA0, spA4);
                    }

                    if (this->av2.actionVar2 == 0) {
                        s32 temp_v0_10 = this->unk_B86[0];
                        s32 temp_ft3_2 = sp54 * 800.0f;

                        this->unk_B86[0] += (s16)temp_ft3_2;
                        if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (temp_ft3_2 != 0) &&
                            (((temp_v0_10 + temp_ft3_2) * temp_v0_10) <= 0)) {
                            // MM Audio_PlaySfx_AtPosWithSyncedFreqAndVolume(.., sp54).
                            spE0 = MergeShip_GetMMFloorSfx(this, MM_NA_SE_PL_GORON_ROLL);
                            MergeShip_PlayMMSfxAtPos(&this->actor.projectedPos, spE0);
                        }
                    }
                }

                spAC = Math_SinS(this->actor.home.rot.y) * spBC;
                spA8 = Math_CosS(this->actor.home.rot.y) * spBC;

                spB4 = spAC + spA4;
                spB0 = spA8 + spA0;

                this->unk_B08 = sqrtf(SQ(spB4) + SQ(spB0));
                this->unk_B08 = CLAMP_MAX(this->unk_B08, 18.0f);

                this->yaw = Math_Atan2S(spB0, spB4);
            }

            MergeShip_Goron_func_808576BC(play, this);

            if (ABS(this->av2.actionVar2) > 0xFA0) {
                this->stateFlags2 |= PLAYER_STATE2_FOOTSTEP; // MM PLAYER_STATE2_8
            }

            if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
                this->linearVelocity = this->unk_B08 * Math_CosS(this->floorPitch);
                this->actor.velocity.y = this->unk_B08 * Math_SinS(this->floorPitch);
            }

            // MM SurfaceType_HasMaterialProperty(MATERIAL_PROPERTY_SOFT_IMPRINT): dirt, sand, shallow dirt and
            // snow take a track. Hyrule's surface materials are numbered as MM's.
            if ((this->unk_B86[1] != 0) ||
                ((this->actor.floorPoly != NULL) &&
                 ((func_80041F10(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) == 0) ||
                  (func_80041F10(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) == 1) ||
                  (func_80041F10(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) == 3) ||
                  (func_80041F10(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) == 14)))) {
                MergeShip_Goron_func_800AE930(&play->colCtx, &sGoronTireMark, &this->actor.world.pos, 15.0f,
                                              this->actor.shape.rot.y, this->actor.floorPoly, this->actor.floorBgId);
            } else {
                MergeShip_Goron_func_800AEF44(&sGoronTireMark);
            }
        } else {
            Math_ScaledStepToS(&this->actor.shape.rot.z, 0, 0x190);

            this->unk_B86[0] = 0;
            if (this->unk_B86[1] != 0) {
                this->actor.gravity = -1.0f;
                Math_ScaledStepToS(&this->actor.home.rot.y, yawTarget, 0x190);

                this->unk_B08 = sqrtf(SQ(this->linearVelocity) + SQ(this->actor.velocity.y)) *
                                ((this->linearVelocity >= 0.0f) ? 1.0f : -1.0f);
                this->unk_B08 = CLAMP_MAX(this->unk_B08, 18.0f);
            } else {
                this->unk_B48 += this->actor.velocity.y * 0.005f;
                if (this->av1.actionVar1 == 1) {
                    if (this->actor.velocity.y > 0.0f) {
                        if ((this->actor.velocity.y + this->actor.gravity) < 0.0f) {
                            this->actor.velocity.y = -this->actor.gravity;
                        }
                    } else {
                        this->unk_B8A = 0xA;
                        if (this->actor.velocity.y > -1.0f) {
                            this->actor.gravity = -0.2f;
                        } else {
                            this->unk_3D0.unk_00 = 1;
                            this->actor.gravity = -10.0f;
                        }
                    }
                }
                this->unk_B08 = this->linearVelocity;
            }

            MergeShip_Goron_func_800AEF44(&sGoronTireMark);
        }

        Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.home.rot.y, 0x7D0);

        // MM Math_AsymStepToS(&actionVar2, spDC, (spDC >= 0) ? 0x7D0 : 0x3E8, 0x4B0).
        {
            s16 step = (spDC >= 0) ? 0x7D0 : 0x3E8;

            if (this->av2.actionVar2 != spDC) {
                if (this->av2.actionVar2 < spDC) {
                    this->av2.actionVar2 += step;
                    if (this->av2.actionVar2 > spDC) {
                        this->av2.actionVar2 = spDC;
                    }
                } else {
                    this->av2.actionVar2 -= 0x4B0;
                    if (this->av2.actionVar2 < spDC) {
                        this->av2.actionVar2 = spDC;
                    }
                }
            }
        }

        if (this->av2.actionVar2 != 0) {
            spD8 = this->actor.shape.rot.x;
            this->actor.shape.rot.x += this->av2.actionVar2;

            Math_ScaledStepToS(&this->unk_B86[0], 0, ABS(this->av2.actionVar2));
            if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (((this->av2.actionVar2 + spD8) * spD8) <= 0)) {
                // MM Audio_PlaySfx_AtPosWithSyncedFreqAndVolume(.., unk_B08).
                spE0 = MergeShip_GetMMFloorSfx(this, (this->unk_B86[1] != 0) ? MM_NA_SE_PL_GORON_CHG_ROLL
                                                                             : MM_NA_SE_PL_GORON_ROLL);
                MergeShip_PlayMMSfxAtPos(&this->actor.projectedPos, spE0);
            }
        }

        if (this->av1.actionVar1 == 2) {
            MergeShip_Goron_SetCylinderForAttack(this, GORON_ATTACK_POUND, 4, 60);
            // MM Actor_SetPlayerImpact(PLAYER_IMPACT_GORON_GROUND_POUND): only Termina's actors read it.
        } else if (this->unk_B86[1] != 0) {
            MergeShip_Goron_SetCylinderForAttack(this, GORON_ATTACK_SPIKES, 1, 25);
        } else {
            MergeShip_Goron_SetCylinderForAttack(this, GORON_ATTACK_NORMAL_ROLL, 1, 25);
        }
    }
}

// ---- water -------------------------------------------------------------------------------------------

// MM func_8083B8D0.
static void MergeShip_Goron_func_8083B8D0(PlayState* play, Player* this) {
    if (func_8083CFA8(play, this, this->actor.velocity.y, 500)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER);
        if (this->fallDistance > 800) {
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
        }
    }
}

// MM func_80834140: sink and void out (MM Player_Action_77 is Ocarina of Time's Player_Action_8084F88C).
static void MergeShip_Goron_func_80834140(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    if (!(this->stateFlags1 & PLAYER_STATE1_DEAD)) {
        // MM func_80834104.
        Player_SetupAction(play, this, Player_Action_8084F88C, 0);
        this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED;

        if (MergeShip_Goron_func_8082DA90(play)) {
            this->av2.actionVar2 = -30;
        }
        this->stateFlags1 |= PLAYER_STATE1_DEAD;
        LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, 84.0f, ANIMMODE_ONCE, -6.0f);
        this->av1.actionVar1 = 1;
        this->linearVelocity = 0.0f;
    }
}

// MM func_8083BB4C's Goron branch: a Goron cannot swim. Deeper than it can stand, it sinks and voids out,
// and (MM func_808345C8) comes back as the human.
s32 MergeShip_Goron_EnterDeepWater(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return false;
    }
    if (this->stateFlags1 & PLAYER_STATE1_DEAD) {
        return true; // already sinking
    }

    MergeShip_Goron_func_80834140(play, this, GORON_ANIM(gPlayerAnim_link_swimer_swim_down));
    // MM func_808345C8: `gSaveContext.save.playerForm = PLAYER_FORM_HUMAN` for the respawn.
    MergeShip_SwapFormEquips(PLAYER_FORM_GORON, (u8)gSaveContext.linkAge);
    MergeShip_SetPlayerForm((u8)gSaveContext.linkAge);
    MergeShip_Goron_func_8083B8D0(play, this);
    return true;
}

// ---- scene collision while rolling --------------------------------------------------------------------

// MM Player_ProcessSceneCollision: a fast Goron roll checks walls, ceiling and floor with MM's
// UPDBGCHECKINFO_FLAG_100 (keep vertical speed on the ground, so ramps launch the ball) and FLAG_200
// (roll over low step-like walls), and without FLAG_8 (no -4 downward pull).
#define GORON_UPDBGCHECKINFO_FLAG_100 (1 << 8)
#define GORON_UPDBGCHECKINFO_FLAG_200 (1 << 9)

void MergeShip_Goron_AdjustBgCheckFlags(Player* this, u32* flags) {
    if ((this->mergeShipForm == PLAYER_FORM_GORON) &&
        !(this->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED)) &&
        (this->stateFlags3 & (PLAYER_STATE3_1000 | PLAYER_STATE3_80000)) && (this->linearVelocity >= 8.0f)) {
        *flags = 0x1 | 0x2 | 0x4 | 0x10 | 0x20 | GORON_UPDBGCHECKINFO_FLAG_100 | GORON_UPDBGCHECKINFO_FLAG_200;
    }
}

// MM func_800B761C (z_actor.c).
static s32 MergeShip_Goron_func_800B761C(Actor* actor, f32 arg1, u32 flags) {
    if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        actor->bgCheckFlags |= BGCHECKFLAG_GROUND_LEAVE;

        if ((actor->velocity.y < 0.0f) && (flags & 0x10)) {
            actor->velocity.y = 0.0f;
        }
        return false;
    }

    return true;
}

static s32 sGoronCeilingBgId;

// MM func_800B7678 (z_actor.c), with BGCHECKFLAG_PLAYER_800 kept in gMergeShipGoron.
static s32 MergeShip_Goron_func_800B7678(PlayState* play, Actor* actor, Vec3f* pos, u32 flags) {
    f32 distToFloor;
    s32 bgId;

    pos->y += 50.0f; // (MM UPDBGCHECKINFO_FLAG_800 is not used by the player)

    actor->floorHeight = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &actor->floorPoly, &bgId, actor, pos);
    actor->bgCheckFlags &= ~(BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_GROUND_LEAVE | BGCHECKFLAG_GROUND_STRICT);
    if (actor->floorHeight <= BGCHECK_Y_MIN) {
        return MergeShip_Goron_func_800B761C(actor, BGCHECK_Y_MIN, flags);
    }

    distToFloor = actor->floorHeight - actor->world.pos.y;
    actor->floorBgId = bgId;
    if ((distToFloor >= 0.0f) || ((actor->bgCheckFlags & BGCHECKFLAG_GROUND) && !gMergeShipGoron.bgCheckFlag800 &&
                                  (distToFloor >= -11.0f) && (actor->velocity.y < 0.0f))) {
        actor->bgCheckFlags |= BGCHECKFLAG_GROUND_STRICT;

        if (actor->bgCheckFlags & BGCHECKFLAG_CEILING) {
            if (bgId != sGoronCeilingBgId) {
                if (distToFloor > 15.0f) {
                    actor->bgCheckFlags |= BGCHECKFLAG_CRUSHED;
                }
            } else {
                actor->world.pos.x = actor->prevPos.x;
                actor->world.pos.z = actor->prevPos.z;
            }
        }

        actor->world.pos.y = actor->floorHeight;

        if (actor->velocity.y <= 0.0f) {
            if (!(actor->bgCheckFlags & BGCHECKFLAG_GROUND)) {
                actor->bgCheckFlags |= BGCHECKFLAG_GROUND_TOUCH;
            } else if ((flags & 0x8) && (actor->gravity < 0.0f)) {
                actor->velocity.y = -4.0f;
            } else if (!(flags & GORON_UPDBGCHECKINFO_FLAG_100)) {
                actor->velocity.y = 0.0f;
            }

            actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
            func_80043334(&play->colCtx, actor, actor->floorBgId);
        }
    } else {
        return MergeShip_Goron_func_800B761C(actor, distToFloor, flags);
    }

    return true;
}

// MM Actor_UpdateBgCheckInfo (z_actor.c), used for the rolling Goron.
s32 MergeShip_Goron_UpdateBgCheckInfo(PlayState* play, Player* this, f32 wallCheckHeight, f32 wallCheckRadius,
                                      f32 ceilingCheckHeight, u32 flags) {
    Actor* actor = &this->actor;
    f32 sp94;
    Vec3f pos;

    if ((this->mergeShipForm != PLAYER_FORM_GORON) ||
        (!gMergeShipGoron.bgCheckFlag800 && !(flags & (GORON_UPDBGCHECKINFO_FLAG_100 | GORON_UPDBGCHECKINFO_FLAG_200)))) {
        return false;
    }

    sp94 = actor->world.pos.y - actor->prevPos.y;

    if ((actor->floorBgId != BGCHECK_SCENE) && (actor->bgCheckFlags & BGCHECKFLAG_GROUND)) {
        func_800433A4(&play->colCtx, actor->floorBgId, actor);
    }

    if (flags & 0x1) {
        s32 bgId;

        if ((!(flags & 0x80) &&
             BgCheck_EntitySphVsWall3(&play->colCtx, &pos, &actor->world.pos, &actor->prevPos, wallCheckRadius,
                                      &actor->wallPoly, &bgId, actor, wallCheckHeight)) ||
            ((flags & 0x80) &&
             BgCheck_EntitySphVsWall4(&play->colCtx, &pos, &actor->world.pos, &actor->prevPos, wallCheckRadius,
                                      &actor->wallPoly, &bgId, actor, wallCheckHeight))) {
            CollisionPoly* sp7C = actor->wallPoly;

            actor->bgCheckFlags |= BGCHECKFLAG_WALL;
            // MM: with FLAG_200, a "wall" that is really a floor-facing step (MM BGCHECKFLAG_PLAYER_1000, set by
            // its wall test when the line hit a floor poly) is ignored while barely displaced.
            if ((flags & GORON_UPDBGCHECKINFO_FLAG_200) && (COLPOLY_GET_NORMAL(sp7C->normal.y) > 0.5f) &&
                ((s32)sp7C->normal.y > 0) &&
                (sqrtf(SQ(actor->colChkInfo.displacement.x) + SQ(actor->colChkInfo.displacement.y) +
                       SQ(actor->colChkInfo.displacement.z)) < 10.0f)) {
                actor->bgCheckFlags &= ~BGCHECKFLAG_WALL;
            } else if (actor->bgCheckFlags & BGCHECKFLAG_WALL) {
                Math_Vec3f_Copy(&actor->world.pos, &pos);
            }

            actor->wallYaw = Math_Atan2S(sp7C->normal.z, sp7C->normal.x);
            actor->wallBgId = bgId;
        } else {
            actor->bgCheckFlags &= ~BGCHECKFLAG_WALL;
        }
    }

    pos.x = actor->world.pos.x;
    pos.z = actor->world.pos.z;
    if (flags & 0x2) {
        f32 y;
        CollisionPoly* ceilingPoly;

        pos.y = actor->prevPos.y + 4.0f;
        if (BgCheck_EntityCheckCeiling(&play->colCtx, &y, &pos, (ceilingCheckHeight + sp94) - 4.0f, &ceilingPoly,
                                       &sGoronCeilingBgId, actor)) {
            actor->bgCheckFlags |= BGCHECKFLAG_CEILING;
            actor->world.pos.y = (y + sp94) - 4.0f;
        } else {
            actor->bgCheckFlags &= ~BGCHECKFLAG_CEILING;
        }
    }

    if (flags & 0x4) {
        WaterBox* waterbox;
        f32 y;

        pos.y = actor->prevPos.y;
        MergeShip_Goron_func_800B7678(play, actor, &pos, flags);
        y = actor->world.pos.y;

        if (WaterBox_GetSurface1(play, &play->colCtx, actor->world.pos.x, actor->world.pos.z, &y, &waterbox)) {
            actor->yDistToWater = y - actor->world.pos.y;
            if (actor->yDistToWater <= 0.0f) {
                actor->bgCheckFlags &= ~(BGCHECKFLAG_WATER | BGCHECKFLAG_WATER_TOUCH);
            } else if (!(actor->bgCheckFlags & BGCHECKFLAG_WATER)) {
                actor->bgCheckFlags |= (BGCHECKFLAG_WATER | BGCHECKFLAG_WATER_TOUCH);
                if (!(flags & 0x40)) {
                    Vec3f sp64;

                    sp64.x = actor->world.pos.x;
                    sp64.y = y;
                    sp64.z = actor->world.pos.z;

                    EffectSsGRipple_Spawn(play, &sp64, 100, 500, 0);
                    EffectSsGRipple_Spawn(play, &sp64, 100, 500, 4);
                    EffectSsGRipple_Spawn(play, &sp64, 100, 500, 8);
                }
            }
        } else {
            actor->bgCheckFlags &= ~(BGCHECKFLAG_WATER | BGCHECKFLAG_WATER_TOUCH);
            actor->yDistToWater = BGCHECK_Y_MIN;
        }
    }

    GameInteractor_Should(VB_AFTER_ACTOR_UPDATE_BGCHECKINFO, true, actor);
    return true;
}

// MM Player_HandleSlopes for the Goron: never while rolling, and MM's own slow-down when climbing a slope.
s32 MergeShip_Goron_HandleSlopes(PlayState* play, Player* this, CollisionPoly* floorPoly, s32* result) {
    static LinkAnimationHeader* sSlopeSlideAnims[] = {
        GORON_ANIM(gPlayerAnim_link_normal_down_slope_slip),
        GORON_ANIM(gPlayerAnim_link_normal_up_slope_slip),
    };

    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return false;
    }

    *result = false;
    if (!Player_InBlockingCsMode(play, this) && !(this->cylinder.base.ocFlags1 & OC1_HIT) &&
        (Player_Action_SlideOnSlope != this->actionFunc) && (MergeShip_Goron_Action_96 != this->actionFunc) &&
        (SurfaceType_GetFloorEffect(&play->colCtx, floorPoly, this->actor.floorBgId) == FLOOR_EFFECT_1)) {
        s16 playerVelYaw = Math_Atan2S(this->actor.velocity.z, this->actor.velocity.x);
        Vec3f slopeNormal;
        s16 downwardSlopeYaw;
        s16 velYawToDownwardSlope;
        f32 slopeSlowdownSpeed;
        f32 temp_fv1;
        f32 slopeSlowdownSpeedStep;

        MergeShip_Goron_GetSlopeDirection(floorPoly, &slopeNormal, &downwardSlopeYaw);
        velYawToDownwardSlope = downwardSlopeYaw - playerVelYaw;

        if (ABS(velYawToDownwardSlope) > 0x3E80) {
            slopeSlowdownSpeed = (1.0f - slopeNormal.y) * 40.0f;
            temp_fv1 = fabsf(this->actor.speedXZ) + slopeSlowdownSpeed;
            slopeSlowdownSpeedStep = SQ(temp_fv1) * 0.011f;
            slopeSlowdownSpeedStep = CLAMP_MIN(slopeSlowdownSpeedStep, 2.2f);

            this->pushedYaw = downwardSlopeYaw;
            Math_StepToF(&this->pushedSpeed, slopeSlowdownSpeed, slopeSlowdownSpeedStep);
        } else {
            Player_SetupAction(play, this, Player_Action_SlideOnSlope, 0);
            func_80832564(play, this);

            Player_AnimChangeLoopMorph(play, this, sSlopeSlideAnims[this->av1.facingUpSlope]);

            this->linearVelocity = sqrtf(SQ(this->actor.velocity.x) + SQ(this->actor.velocity.z));
            this->yaw = downwardSlopeYaw;

            if (sFloorShapePitch >= 0) {
                this->av1.facingUpSlope = true;
                MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_HANG);
            }

            *result = true;
        }
    }
    return true;
}

// ---- once a frame -------------------------------------------------------------------------------------

// MM Player_UpdateCommon's Goron parts that run every frame: the afterimages fade (func_80122C20), the spike
// roll's magic drain (MM Magic_Update), and the tire mark ages (MM Effect_UpdateAll).
void MergeShip_Goron_UpdateCommon(Player* this, PlayState* play) {
    MergeShip_Goron_func_80122C20(play, &this->unk_3D0);

    if ((this->mergeShipForm == PLAYER_FORM_DEKU) && gMergeShipGoron.tireMarkActive) {
        MergeShip_Goron_TireMarkUpdate(&sGoronTireMark); // the Deku spin's tire mark (MM Effect_UpdateAll)
    }
    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }

    MergeShip_Goron_UpdateMagicDrain(play);
    MergeShip_BombWalls_DebugLog(play, this); // (test harness only)
    if (gMergeShipGoron.tireMarkActive) {
        MergeShip_Goron_TireMarkUpdate(&sGoronTireMark);
    }
}

// MM Player_UpdateCommon, just before the action function: the stateFlags3 bits MM's actions set anew every
// frame are cleared (Ocarina of Time's own clear only knows its eight bits).
void MergeShip_Goron_UpdatePreAction(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }
    this->stateFlags3 &= ~(PLAYER_STATE3_100 | PLAYER_STATE3_800 | PLAYER_STATE3_1000 | PLAYER_STATE3_100000 |
                           PLAYER_STATE3_2000000 | PLAYER_STATE3_4000000 | PLAYER_STATE3_8000000 |
                           PLAYER_STATE3_10000000);
}

// MM Player_UpdateCamAndSeqModes: the spiked roll's camera (GORONDASH on the ground, GORONJUMP in the air).
// Returns true when an MM-only mode was requested, in place of Ocarina of Time's Camera_RequestMode.
s32 MergeShip_Goron_RequestCamera(PlayState* play, Player* this) {
    if ((this->mergeShipForm == PLAYER_FORM_GORON) && (this->csAction == 0) &&
        !((this->actor.parent != NULL) && (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT)) &&
        (this->stateFlags3 & PLAYER_STATE3_80000)) {
        MergeShip_Camera_RequestMMMode(play,
                                       (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ? MM_CAM_MODE_GORONDASH
                                                                                       : MM_CAM_MODE_GORONJUMP);
        return true;
    }
    return false;
}

// MM Player_UpdateCommon's collider block, the Goron guard: a hard shield cylinder, and the body cylinder
// shrunk to it.
void MergeShip_Goron_UpdateColliders(PlayState* play, Player* this) {
    if ((this->mergeShipForm == PLAYER_FORM_GORON) && (this->stateFlags1 & PLAYER_STATE1_SHIELDING)) {
        this->shieldCylinder.base.acFlags = AC_ON | AC_HARD | AC_TYPE_ENEMY;
        this->shieldCylinder.info.toucher.dmgFlags = 0x100000;
        this->shieldCylinder.info.toucherFlags = TOUCH_NONE;
        this->shieldCylinder.info.bumperFlags = BUMP_ON;
        this->shieldCylinder.dim.height = 35;
        this->shieldCylinder.dim.radius = 30;
        this->shieldCylinder.dim.yShift = 0;
        Collider_UpdateCylinder(&this->actor, &this->shieldCylinder);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->shieldCylinder.base);
        this->cylinder.dim.yShift = 0;
        this->cylinder.dim.height = this->shieldCylinder.dim.height;
    }
}

// MM Player_UpdateCommon: the body cylinder is an attack collider too while rolling or pounding (MM registers
// it whenever it has AT flags, except in the pound's fall). Also MM's end-of-frame shield cylinder reset.
void MergeShip_Goron_SetColliderAT(PlayState* play, Player* this) {
    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }

    if (!(this->stateFlags2 & PLAYER_STATE2_FROZEN) && !(this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_DAMAGED)) &&
        (this->invincibilityTimer == 0) && (this->cylinder.base.atFlags != AT_NONE) &&
        ((MergeShip_Goron_Action_96 != this->actionFunc) || (this->av1.actionVar1 != 1))) {
        // (Ocarina of Time already registers it itself while the timer is negative.)
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->cylinder.base);
    }

    Collider_ResetCylinderAC(play, &this->shieldCylinder.base);
    Collider_ResetCylinderAT(play, &this->shieldCylinder.base);
}

// MM func_80834600: a curled or rolling Goron's damage rules.
s32 MergeShip_Goron_ShieldBounced(Player* this) {
    return (this->mergeShipForm == PLAYER_FORM_GORON) && (this->shieldCylinder.base.acFlags & AC_BOUNCED);
}

s32 MergeShip_Goron_IgnoresFloorDamage(PlayState* play, Player* this) {
    // MM: out of water, a Goron takes no damage from hot floors (FLOOR_TYPE_2/3) unless they are wall-damage
    // surfaces; only a damaging wall still hurts it.
    if ((this->mergeShipForm == PLAYER_FORM_GORON) && !(this->actor.yDistToWater > 0.0f) &&
        ((this->actor.floorPoly == NULL) ||
         !SurfaceType_IsWallDamage(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId))) {
        return !((this->actor.wallPoly != NULL) &&
                 SurfaceType_IsWallDamage(&play->colCtx, this->actor.wallPoly, this->actor.wallBgId));
    }
    return false;
}

// ---- doors ------------------------------------------------------------------------------------------------

// MM D_8085D118 / D_8085D124 (Player_Door_Knob).
LinkAnimationHeader* MergeShip_Goron_DoorOpenAnim(Player* this, s32 doorDirection) {
    return (doorDirection < 0) ? GORON_ANIM(gPlayerAnim_pg_doorA_open) : GORON_ANIM(gPlayerAnim_pg_doorB_open);
}

// ---- drawing --------------------------------------------------------------------------------------------

// MM func_80846460: with no skeleton drawn, the body parts all sit at the ball.
static void MergeShip_Goron_func_80846460(Player* this) {
    Vec3f* pos;
    Vec3f* bodyPartPosPtr;
    s32 i;

    this->actor.focus.pos.x = this->actor.world.pos.x;
    this->actor.focus.pos.z = this->actor.world.pos.z;
    this->actor.focus.pos.y = this->actor.world.pos.y + 24.0f;

    pos = &this->actor.world.pos;
    bodyPartPosPtr = this->bodyPartsPos;
    for (i = 0; i < PLAYER_BODYPART_MAX; i++) {
        Math_Vec3f_Copy(bodyPartPosPtr, pos);
        bodyPartPosPtr++;
    }

    this->bodyPartsPos[PLAYER_BODYPART_HEAD].y = this->actor.world.pos.y + 24.0f;
    this->bodyPartsPos[PLAYER_BODYPART_WAIST].y = this->actor.world.pos.y + 60.0f;
    Math_Vec3f_Copy(&this->actor.shape.feetPos[0], pos);
    Math_Vec3f_Copy(&this->actor.shape.feetPos[1], pos);
}

// MM D_8085D540: the spikes growing out.
static MergeShipScaleKey sGoronSpikeKeys[] = {
    { 0, { 0, 0, 0 } },
    { 2, { 100, 100, 100 } },
};

// MM Player_Draw's Goron branches: curled up (the ball, its spikes and its charge flames), or guarding (the
// guard skeleton unk_2C8). Returns true when the player's own skeleton must not be drawn.
s32 MergeShip_Goron_Draw(PlayState* play, Player* this) {
    static Color_RGB8 D_8085D580 = { 255, 255, 255 };
    static Color_RGB8 D_8085D584 = { 80, 80, 200 };

    s32 drawn = false;

    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return false;
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (this->stateFlags3 & PLAYER_STATE3_1000) {
        Color_RGB8 spBC;
        f32 spB8 = this->unk_ABC + 1.0f;
        f32 spB4 = 1.0f - (this->unk_ABC * 0.5f);

        MergeShip_Goron_func_80846460(this);
        Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y + (1200.0f * this->actor.scale.y * spB8),
                         this->actor.world.pos.z, MTXMODE_NEW);

        if (this->unk_B86[0] != 0) {
            Matrix_RotateY(BINANG_TO_RAD(this->unk_B28), MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->unk_B86[0]), MTXMODE_APPLY);
            Matrix_RotateY(BINANG_TO_RAD(-this->unk_B28), MTXMODE_APPLY);
        }

        Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(this->actor.shape.rot.z), MTXMODE_APPLY);

        Matrix_Scale(this->actor.scale.x * spB4 * 1.15f, this->actor.scale.y * spB8 * 1.15f,
                     CLAMP_MIN(spB8, spB4) * this->actor.scale.z * 1.15f, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(this->actor.shape.rot.x), MTXMODE_APPLY);
        MergeShip_Scene_SetRenderModeXlu(play, 0, 1);

        // MM Color_RGB8_Lerp.
        spBC.r = D_8085D580.r + (D_8085D584.r - D_8085D580.r) * this->unk_B10[0];
        spBC.g = D_8085D580.g + (D_8085D584.g - D_8085D580.g) * this->unk_B10[0];
        spBC.b = D_8085D580.b + (D_8085D584.b - D_8085D580.b) * this->unk_B10[0];

        gDPSetEnvColor(POLY_OPA_DISP++, spBC.r, spBC.g, spBC.b, 255);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gLinkGoronCurledDL);

        if (this->unk_B86[1] != 0) {
            if (this->unk_B86[1] < 3) {
                MergeShip_Goron_func_80124618(sGoronSpikeKeys, this->unk_B86[1], this->unk_AF0);
                Matrix_Scale(this->unk_AF0[0].x, this->unk_AF0[0].y, this->unk_AF0[0].z, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            }

            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_link_goron_DL_00C540);
        }

        MergeShip_Goron_func_80122BA4(play, &this->unk_3D0, 1, 255);
        MergeShip_Goron_func_80122BA4(play, &this->unk_3D0, 2, 255);

        if (this->unk_B86[1] < 3) {
            if (this->av1.actionVar1 >= 5) {
                f32 var_fa1 = (this->av1.actionVar1 - 4) * 0.02f;
                u8 sp9B;

                if (this->unk_B86[1] != 0) {
                    sp9B = (-this->unk_B86[1] * 0x55) + 0xFF;
                } else {
                    sp9B = (200.0f * var_fa1);
                }

                if (this->unk_B86[1] != 0) {
                    var_fa1 = 0.65f;
                } else {
                    var_fa1 *= 1.0f;
                }

                Matrix_Scale(1.0f, var_fa1, var_fa1, MTXMODE_APPLY);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                MergeShip_Goron_DrawMatAnim013138(play);
                gDPSetEnvColor(POLY_XLU_DISP++, 155, 0, 0, sp9B);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_object_link_goron_DL_0127B0);
                MergeShip_Goron_DrawMatAnim014684(play);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_object_link_goron_DL_0134D0);
            }
        }

        drawn = true;
    } else if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        MergeShip_Goron_func_80846460(this);
        gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);
        SkelAnime_DrawFlexOpa(play, this->unk_2C8.skeleton, this->unk_2C8.jointTable, this->unk_2C8.dListCount, NULL,
                              NULL, NULL);
        drawn = true;
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return drawn;
}

// MM Player_Draw's func_80122D44 (the afterimages) and MM Effect_DrawAll's share for the Goron's tire mark.
void MergeShip_Goron_DrawEffects(PlayState* play, Player* this) {
    MergeShip_Goron_func_80122D44(play, &this->unk_3D0);

    if (((this->mergeShipForm == PLAYER_FORM_GORON) || (this->mergeShipForm == PLAYER_FORM_DEKU)) &&
        gMergeShipGoron.tireMarkActive) {
        MergeShip_Goron_TireMarkDraw(&sGoronTireMark, play->state.gfxCtx);
    }
}

// MM Player_Action_95's func_800AE930(&play->colCtx, Effect_GetByIndex(meleeWeaponEffectIndex[2]),
// &world.pos, 2.0f, yaw, floorPoly, floorBgId): the Deku spin lays the tire mark (MM EffectTireMark, ported
// above for the Goron's roll).
void MergeShip_Deku_LayTireMark(PlayState* play, Player* this) {
    if (gMergeShipGoron.tireMarkActive) {
        MergeShip_Goron_func_800AE930(&play->colCtx, &sGoronTireMark, &this->actor.world.pos, 2.0f, this->yaw,
                                      this->actor.floorPoly, this->actor.floorBgId);
    }
}
