/*
 * File: z_mm_obj_etcetera.c
 * Overlay: ovl_MM_Obj_Etcetera
 * Description: Majora's Mask's Deku Flower
 *
 * MergeShip2 -- ported from 2ship2harkinian/mm/src/overlays/actors/ovl_Obj_Etcetera/z_obj_etcetera.c. The
 * flower's model, animations and collision are MM's (mm.o2r, gameplay_keep). Its collision polys carry the
 * vertex-B 0x4000 flag the Deku's A button looks for (MM func_800C9DDC, MergeShip_MM_func_800C9DDC).
 *
 * Host-game differences:
 * - AC damage mask: MM's 0x01000202 (Deku stick, sword, spin attack) in Ocarina of Time's bits.
 * - MM's Update changes the scene light setting to the floor's while the player is burrowed
 *   (Environment_ChangeLightSetting); Ocarina of Time's environment has no such call, so it is not done.
 */

#include "z_mm_obj_etcetera.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)

s16 gMergeShipActorId_MM_Obj_Etcetera = -1;

void MMObjEtcetera_PlayRustleAnimation(MMObjEtcetera* this, PlayState* play);
void MMObjEtcetera_DoBounceOscillation(MMObjEtcetera* this, PlayState* play);
void MMObjEtcetera_Setup(MMObjEtcetera* this, PlayState* play);
void MMObjEtcetera_DrawIdle(Actor* thisx, PlayState* play);
void MMObjEtcetera_DrawAnimated(Actor* thisx, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        // MM 0x01000202: DMG_DEKU_STICK | DMG_SWORD | DMG_SPIN_ATTACK.
        { DMG_DEKU_STICK | DMG_SLASH | DMG_SPIN_ATTACK, 0x00, 0x00 },
        TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 20, 14, 0, { 0, 0, 0 } },
};

/**
 * Most interactions with a Deku Flower cause it to slightly oscillate on the X and Z axes.
 */
static f32 sOscillationTable[] = {
    -1.0f, -1.0f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f,
};

void MMObjEtcetera_Init(Actor* thisx, PlayState* play) {
    MMObjEtcetera* this = (MMObjEtcetera*)thisx;
    s32 objectSlot;
    s32 type = MM_DEKU_FLOWER_TYPE(&this->dyna.actor);
    s32 floorBgId;
    Vec3f pos;

    if ((type < MM_DEKU_FLOWER_TYPE_PINK) || (type >= MM_DEKU_FLOWER_TYPE_MAX)) {
        type = MM_DEKU_FLOWER_TYPE_PINK;
    }

    // MM sObjectIds: every type is GAMEPLAY_KEEP.
    objectSlot = Object_GetIndex(&play->objectCtx, OBJECT_GAMEPLAY_KEEP);
    if (objectSlot > -1) {
        this->objectSlot = objectSlot;
    }

    pos.x = this->dyna.actor.world.pos.x;
    pos.y = this->dyna.actor.world.pos.y + 10.0f;
    pos.z = this->dyna.actor.world.pos.z;
    BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->dyna.actor.floorPoly, &floorBgId, &this->dyna.actor, &pos);
    this->dyna.actor.floorBgId = floorBgId;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
    this->actionFunc = MMObjEtcetera_Setup;
    Actor_SetScale(&this->dyna.actor, 0.01f);
    this->dyna.actor.scale.y = 0.02f;
    this->burrowFlag = 0;
}

void MMObjEtcetera_Destroy(Actor* thisx, PlayState* play) {
    MMObjEtcetera* this = (MMObjEtcetera*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    Collider_DestroyCylinder(play, &this->collider);
}

void MMObjEtcetera_DoNormalOscillation(MMObjEtcetera* this, PlayState* play) {
    if (this->oscillationTimer > 0) {
        Actor_SetScale(&this->dyna.actor,
                       (sOscillationTable[play->gameplayFrames % 18] * (0.0001f * this->oscillationTimer)) + 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->oscillationTimer--;
    } else {
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
    }
}

void MMObjEtcetera_StartRustleAnimation(MMObjEtcetera* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)mm_gDekuFlowerRustleAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((void*)mm_gDekuFlowerRustleAnim), ANIMMODE_ONCE, 0.0f);
    this->dyna.actor.draw = MMObjEtcetera_DrawAnimated;
    this->actionFunc = MMObjEtcetera_PlayRustleAnimation;
}

void MMObjEtcetera_Idle(MMObjEtcetera* this, PlayState* play) {
    s16 minOscillationTimer;
    Player* player = GET_PLAYER(play);

    if ((player->stateFlags3 & PLAYER_STATE3_200) && (this->dyna.actor.xzDistToPlayer < 20.0f)) {
        // Player is launching out of the Deku Flower
        Animation_Change(&this->skelAnime, (AnimationHeader*)mm_gDekuFlowerBounceAnim, 1.0f, 0.0f,
                         Animation_GetLastFrame((void*)mm_gDekuFlowerBounceAnim), ANIMMODE_ONCE, 0.0f);
        this->dyna.actor.draw = MMObjEtcetera_DrawAnimated;
        this->actionFunc = MMObjEtcetera_DoBounceOscillation;
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->bounceOscillationScale = 0.003f;
        this->oscillationTimer = 30;
        this->burrowFlag &= ~1;
    } else if ((player->stateFlags3 & PLAYER_STATE3_2000) && (this->dyna.actor.xzDistToPlayer < 30.0f) &&
               (this->dyna.actor.yDistToPlayer > 0.0f)) {
        // Player is hovering above the Deku Flower (MM playerHeightRel is OoT's yDistToPlayer)
        minOscillationTimer = 10 - (s32)(this->dyna.actor.yDistToPlayer * 0.05f);
        if (this->oscillationTimer < minOscillationTimer) {
            this->oscillationTimer = minOscillationTimer;
        }
    } else {
        if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
            if (!(this->burrowFlag & 1)) {
                // Player is walking onto the Deku Flower, or falling on it from a height
                this->oscillationTimer = 10;
                MMObjEtcetera_StartRustleAnimation(this);
            } else if ((player->actor.speedXZ > 0.1f) ||
                       ((player->unk_ABC < 0.0f) && !(player->stateFlags3 & PLAYER_STATE3_100))) {
                // Player is walking on top of the flower, or at the very start of burrowing or launching
                this->oscillationTimer = 10;
            }

            this->burrowFlag |= 1;
        } else {
            if (this->burrowFlag & 1) {
                // Player is walking off the Deku Flower
                this->oscillationTimer = 10;
                MMObjEtcetera_StartRustleAnimation(this);
            }

            this->burrowFlag &= ~1;
        }
    }

    if ((this->collider.base.acFlags & AC_HIT)) {
        this->oscillationTimer = 10;
        MMObjEtcetera_StartRustleAnimation(this);
    }

    MMObjEtcetera_DoNormalOscillation(this, play);
}

void MMObjEtcetera_PlayRustleAnimation(MMObjEtcetera* this, PlayState* play) {
    if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
        this->burrowFlag |= 1;
    } else {
        this->burrowFlag &= ~1;
    }

    if (SkelAnime_Update(&this->skelAnime)) {
        this->dyna.actor.draw = MMObjEtcetera_DrawIdle;
        this->actionFunc = MMObjEtcetera_Idle;
    }

    MMObjEtcetera_DoNormalOscillation(this, play);
}

void MMObjEtcetera_DoBounceOscillation(MMObjEtcetera* this, PlayState* play) {
    DynaPolyActor* dyna = &this->dyna;
    f32 scaleTemp;

    if (DynaPolyActor_IsPlayerOnTop(dyna)) {
        this->burrowFlag |= 1;
    } else {
        this->burrowFlag &= ~1;
    }

    SkelAnime_Update(&this->skelAnime);

    if (this->oscillationTimer > 0) {
        this->oscillationTimer--;
    } else {
        this->dyna.actor.draw = MMObjEtcetera_DrawIdle;
        this->actionFunc = MMObjEtcetera_Idle;
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->oscillationTimer = 0;
        this->bounceOscillationScale = 0.0f;
        return;
    }

    this->bounceOscillationScale *= 0.8f;
    this->bounceOscillationScale -= (this->dyna.actor.scale.x - 0.01f) * 0.4f;
    scaleTemp = dyna->actor.scale.x + this->bounceOscillationScale;
    Actor_SetScale(&this->dyna.actor, scaleTemp);
    this->dyna.actor.scale.y = 2.0f * scaleTemp;
}

void MMObjEtcetera_Setup(MMObjEtcetera* this, PlayState* play) {
    CollisionHeader* colHeader = NULL;
    s32 type;
    void* collisionHeaders[] = {
        (void*)mm_gPinkDekuFlowerCol,
        (void*)mm_gPinkDekuFlowerCol,
        (void*)mm_gGoldDekuFlowerCol,
        (void*)mm_gGoldDekuFlowerCol,
    };
    void* thisCollisionHeader;

    type = MM_DEKU_FLOWER_TYPE(&this->dyna.actor);
    if ((type < MM_DEKU_FLOWER_TYPE_PINK) || (type >= MM_DEKU_FLOWER_TYPE_MAX)) {
        type = MM_DEKU_FLOWER_TYPE_PINK;
    }

    if (Object_IsLoaded(&play->objectCtx, this->objectSlot)) {
        this->dyna.actor.objBankIndex = this->objectSlot;
        Actor_SetObjectDependency(play, &this->dyna.actor);
        // MM DYNA_TRANSFORM_POS
        DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
        thisCollisionHeader = collisionHeaders[type];
        if (thisCollisionHeader != NULL) {
            CollisionHeader_GetVirtual(thisCollisionHeader, &colHeader);
        }

        this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);

        type = MM_DEKU_FLOWER_TYPE(&this->dyna.actor);
        switch (type) {
            case MM_DEKU_FLOWER_TYPE_PINK:
            case MM_DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE:
                SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)mm_gPinkDekuFlowerSkel,
                               (AnimationHeader*)mm_gDekuFlowerBounceAnim, this->jointTable, this->morphTable,
                               PINK_DEKU_FLOWER_LIMB_MAX);
                this->dList = (Gfx*)mm_gPinkDekuFlowerIdleDL;
                break;

            case MM_DEKU_FLOWER_TYPE_GOLD:
            case MM_DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE:
                this->dList = (Gfx*)mm_gGoldDekuFlowerIdleDL;
                SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)mm_gGoldDekuFlowerSkel,
                               (AnimationHeader*)mm_gDekuFlowerBounceAnim, this->jointTable, this->morphTable,
                               GOLD_DEKU_FLOWER_LIMB_MAX);
                this->collider.dim.height = 20;
                break;

            default:
                break;
        }

        type = MM_DEKU_FLOWER_TYPE(&this->dyna.actor);
        switch (type) {
            case MM_DEKU_FLOWER_TYPE_PINK:
            case MM_DEKU_FLOWER_TYPE_GOLD:
                this->dyna.actor.draw = MMObjEtcetera_DrawIdle;
                this->actionFunc = MMObjEtcetera_Idle;
                Actor_SetScale(&this->dyna.actor, 0.01f);
                this->dyna.actor.scale.y = 0.02f;
                this->dyna.actor.focus.pos.y = this->dyna.actor.home.pos.y + 10.0f;
                // MM ATTENTION_RANGE_3
                this->dyna.actor.targetMode = 3;
                break;

            case MM_DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE:
            case MM_DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE:
                Animation_Change(&this->skelAnime, (AnimationHeader*)mm_gDekuFlowerBounceAnim, 1.0f, 0.0f,
                                 Animation_GetLastFrame((void*)mm_gDekuFlowerBounceAnim), ANIMMODE_ONCE, 0.0f);
                this->dyna.actor.draw = MMObjEtcetera_DrawAnimated;
                this->actionFunc = MMObjEtcetera_DoBounceOscillation;
                Actor_SetScale(&this->dyna.actor, 0.0f);
                this->oscillationTimer = 30;
                this->bounceOscillationScale = 0.0f;
                this->dyna.actor.focus.pos.y = this->dyna.actor.home.pos.y + 10.0f;
                this->dyna.actor.targetMode = 3;
                break;

            default:
                break;
        }
    }
}

void MMObjEtcetera_Update(Actor* thisx, PlayState* play) {
    MMObjEtcetera* this = (MMObjEtcetera*)thisx;

    // MM changes the light setting to the floor's while the player is burrowed; see the file comment.
    this->actionFunc(this, play);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

void MMObjEtcetera_DrawIdle(Actor* thisx, PlayState* play) {
    MMObjEtcetera* this = (MMObjEtcetera*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, this->dList);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MMObjEtcetera_DrawAnimated(Actor* thisx, PlayState* play) {
    MMObjEtcetera* this = (MMObjEtcetera*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, NULL, &this->dyna.actor);
}
