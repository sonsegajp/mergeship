/*
 * File: z_mm_en_elforg.c
 * Overlay: ovl_MM_En_Elforg
 * Description: Majora's Mask's Stray Fairy
 *
 * MergeShip2 -- ported from 2ship2harkinian/mm/src/overlays/actors/ovl_En_Elforg/z_en_elforg.c, function by function
 * (MM names kept after the MMEnElforg_ prefix). Model, animation, animated materials and sounds are MM's (mm.o2r's
 * gameplay_keep and MM's sound tables).
 *
 * Host-game differences:
 * - The area: MM takes the current temple (Map_IsInDungeonOrBossScene, dungeonSceneSharedIndex) when it is in one and
 *   the params' area otherwise. Hyrule has no MM temple, so it is always the params' area.
 * - The give (MM EnElforg_FreeFloating's GameInteractor_Should(VB_GIVE_ITEM_FROM_ELFORG) block) is
 *   MergeShip_StrayFairy_OnCollected: the count, MM's text and fanfare, and the Clock Town plea. In MM a temple fairy
 *   counts only inside its temple; here a fairy counts for its own area wherever it is found.
 * - The Clock Town plea: MM freezes the player and runs CS_ID_GLOBAL_TALK; Ocarina of Time has no cutscene manager,
 *   so only the freeze (freezeTimer, PLAYER_STATE1_IN_CUTSCENE) and the textbox are kept. MM's
 *   SET_WEEKEVENTREG(WEEKEVENTREG_08_80) is the Clock Town count (gMergeShipSave.strayFairies[0]).
 * - MERGESHIP_STRAY_FAIRY_FLAG_NONE: no scene flag is read or set (MergeShipStrayFairy.h).
 * - 2S2H's CosmeticEditor_SetStrayFairyMaterial and its VB_SET_DRAW_FOR_SAVED_STRAY_FAIRY hook are 2S2H additions
 *   and are not ported.
 */

#include "z_mm_en_elforg.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMMMasks.h"
#include "soh/MergeShip/MergeShipTransformFx.h" // MergeShip_PlayMMSfx*, MM_NA_SE_*
#include "soh/MergeShip/MergeShipAnimatedMat.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED)

void MMEnElforg_TrappedByBubble(MMEnElforg* this, PlayState* play);
void MMEnElforg_TurnInFairy(MMEnElforg* this, PlayState* play);
void MMEnElforg_FreeFloatingFairyFountain(MMEnElforg* this, PlayState* play);
void MMEnElforg_FreeFloating(MMEnElforg* this, PlayState* play);
void MMEnElforg_SetupTrappedByEnemy(MMEnElforg* this, PlayState* play);
void MMEnElforg_HiddenByCollider(MMEnElforg* this, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        // MM 0xF7CFFFFF: every damage type but MM's shield/light-ray bits, which are Ocarina of Time's DMG_SHIELD and
        // DMG_MIR_RAY at the same positions.
        { 0xF7CFFFFF, 0x00, 0x00 },
        TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 16, 32, 0, { 0, 0, 0 } },
};

// MM Math_Atan2S_XY(x, y) is Ocarina of Time's Math_Atan2S(x, y) (see MergeShipMMMasks.c MM_Atan2S_XY).
static s16 MMEnElforg_Atan2S_XY(f32 x, f32 y) {
    return Math_Atan2S(x, y);
}

static s32 MMEnElforg_HasFlag(PlayState* play, s32 type, s32 flag) {
    if (flag == MERGESHIP_STRAY_FAIRY_FLAG_NONE) {
        return false;
    }
    switch (type) {
        case MM_STRAY_FAIRY_TYPE_COLLECTIBLE:
            return Flags_GetCollectible(play, flag);
        case MM_STRAY_FAIRY_TYPE_CHEST:
            return Flags_GetTreasure(play, flag);
        default:
            return Flags_GetSwitch(play, flag);
    }
}

void MMEnElforg_InitializeParams(MMEnElforg* this) {
    this->actor.speedXZ = 1.0f;
    this->targetSpeedXZ = 1.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.world.rot.y = Rand_CenteredFloat(0x10000);
    this->timer = 0;
    this->secondaryTimer = Rand_ZeroFloat(100.0f);
    this->actor.shape.yOffset = 0.0f;
    this->skelAnime.curFrame = (s32)Rand_ZeroFloat(5.0f);
}

void MMEnElforg_Init(Actor* thisx, PlayState* play) {
    MMEnElforg* this = (MMEnElforg*)thisx;

    if (!MergeShip_StrayFairy_IsEnabled()) {
        Actor_Kill(thisx);
        return;
    }

    Actor_SetScale(thisx, 0.01f);
    this->strayFairyFlags = 0;
    this->direction = 0;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)mm_gStrayFairySkel,
                       (AnimationHeader*)mm_gStrayFairyFlyingAnim, this->jointTable, this->jointTable,
                       STRAY_FAIRY_LIMB_MAX);
    this->skelAnime.playSpeed = 1.0f;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    thisx->shape.shadowAlpha = 255;

    switch (MM_STRAY_FAIRY_TYPE(thisx)) {
        case MM_STRAY_FAIRY_TYPE_CLOCK_TOWN:
            // MM CHECK_WEEKEVENTREG(WEEKEVENTREG_08_80)
            if (MergeShip_StrayFairy_GetCount(MM_STRAY_FAIRY_AREA_CLOCK_TOWN) != 0) {
                Actor_Kill(thisx);
                return;
            }
            break;

        case MM_STRAY_FAIRY_TYPE_COLLECTIBLE:
            if (MMEnElforg_HasFlag(play, MM_STRAY_FAIRY_TYPE_COLLECTIBLE, MM_STRAY_FAIRY_GET_FLAG(thisx))) {
                Actor_Kill(thisx);
                return;
            }
            break;

        case MM_STRAY_FAIRY_TYPE_FAIRY_FOUNTAIN:
        case MM_STRAY_FAIRY_TYPE_BUBBLE:
        case MM_STRAY_FAIRY_TYPE_CHEST:
        case MM_STRAY_FAIRY_TYPE_RETURNING_TO_FOUNTAIN:
            break;

        default:
            if (MMEnElforg_HasFlag(play, MM_STRAY_FAIRY_TYPE_FREE_FLOATING, MM_STRAY_FAIRY_GET_FLAG(thisx))) {
                Actor_Kill(thisx);
                return;
            }
            break;
    }

    // MM: the current temple, else the params' area (see the file comment).
    this->area = MM_STRAY_FAIRY_GET_NON_DUNGEON_AREA(thisx);

    switch (MM_STRAY_FAIRY_TYPE(thisx)) {
        case MM_STRAY_FAIRY_TYPE_FAIRY_FOUNTAIN:
            MMEnElforg_InitializeParams(this);
            this->actionFunc = MMEnElforg_FreeFloatingFairyFountain;
            this->targetSpeedXZ = Rand_ZeroFloat(2.0f) + 1.0f;
            this->targetDistanceFromHome = Rand_ZeroFloat(100.0f) + 50.0f;
            break;

        case MM_STRAY_FAIRY_TYPE_RETURNING_TO_FOUNTAIN:
            MMEnElforg_InitializeParams(this);
            this->actionFunc = MMEnElforg_TurnInFairy;
            this->secondaryTimer = 60;
            break;

        case MM_STRAY_FAIRY_TYPE_BUBBLE:
            this->timer = 0;
            this->actionFunc = MMEnElforg_TrappedByBubble;
            break;

        case MM_STRAY_FAIRY_TYPE_ENEMY:
            this->actionFunc = MMEnElforg_SetupTrappedByEnemy;
            MMEnElforg_SetupTrappedByEnemy(this, play);
            thisx->draw = NULL;
            break;

        case MM_STRAY_FAIRY_TYPE_COLLIDER:
            this->actionFunc = MMEnElforg_HiddenByCollider;
            thisx->draw = NULL;
            // MM Collider_InitAndSetCylinder
            Collider_InitCylinder(play, &this->collider);
            Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
            Collider_UpdateCylinder(thisx, &this->collider);
            break;

        default:
            MMEnElforg_InitializeParams(this);
            this->actionFunc = MMEnElforg_FreeFloating;
            break;
    }

    thisx->shape.rot.y = 0;
}

void MMEnElforg_Destroy(Actor* thisx, PlayState* play) {
    MMEnElforg* this = (MMEnElforg*)thisx;

    if (MM_STRAY_FAIRY_TYPE(&this->actor) == MM_STRAY_FAIRY_TYPE_COLLIDER) {
        Collider_DestroyCylinder(play, &this->collider);
    }
}

void MMEnElforg_SpawnSparkles(MMEnElforg* this, PlayState* play, s32 life) {
    static Vec3f sVelocity = { 0.0f, -0.05f, 0.0f };
    static Vec3f sAccel = { 0.0f, -0.025f, 0.0f };
    static Color_RGBA8 sPrimColors[] = {
        { 255, 235, 220, 255 }, { 255, 220, 220, 255 }, { 220, 255, 220, 255 },
        { 220, 220, 255, 255 }, { 255, 255, 200, 255 },
    };
    static Color_RGBA8 sEnvColors[] = {
        { 255, 150, 0, 255 }, { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 },
    };
    Vec3f pos;
    s32 index;

    pos.x = Rand_CenteredFloat(6.0f) + this->actor.world.pos.x;
    pos.y = (Rand_ZeroOne() * 6.0f) + this->actor.world.pos.y + (this->actor.shape.yOffset * this->actor.scale.y);
    pos.z = Rand_CenteredFloat(6.0f) + this->actor.world.pos.z;
    index = (this->area < MM_STRAY_FAIRY_AREA_CLOCK_TOWN || this->area >= MM_STRAY_FAIRY_AREA_MAX)
                ? MM_STRAY_FAIRY_AREA_CLOCK_TOWN
                : this->area;
    EffectSsKiraKira_SpawnDispersed(play, &pos, &sVelocity, &sAccel, &sPrimColors[index], &sEnvColors[index], 1000,
                                    life);
}

void MMEnElforg_ApproachTargetYPosition(MMEnElforg* this, Vec3f* targetPos) {
    f32 yDifference = targetPos->y - this->actor.world.pos.y;

    if (fabsf(yDifference) < this->actor.speedXZ) {
        this->actor.world.pos.y = targetPos->y;
    } else if (yDifference > 0.0f) {
        this->actor.world.pos.y += this->actor.speedXZ;
    } else {
        this->actor.world.pos.y -= this->actor.speedXZ;
    }
}

void MMEnElforg_ApproachTargetSpeedXZ(MMEnElforg* this) {
    if (this->actor.speedXZ > this->targetSpeedXZ) {
        this->actor.speedXZ *= 0.9f;
    } else if (this->actor.speedXZ < (this->targetSpeedXZ - 0.1f)) {
        this->actor.speedXZ += 0.1f;
    } else {
        this->actor.speedXZ = this->targetSpeedXZ;
    }
}

void MMEnElforg_MoveToTargetFairyFountain(MMEnElforg* this, Vec3f* homePos) {
    f32 xzDistance;
    f32 zDifference;
    f32 xDifference;
    s16 angleAdjustment;
    s16 targetAngle;

    this->actor.shape.yOffset += 100.0f * Math_SinS(this->timer * 0x200);
    MMEnElforg_ApproachTargetYPosition(this, homePos);
    xDifference = this->actor.world.pos.x - homePos->x;
    zDifference = this->actor.world.pos.z - homePos->z;
    targetAngle = MMEnElforg_Atan2S_XY(-zDifference, -xDifference);
    xzDistance = sqrtf(SQ(xDifference) + SQ(zDifference));

    if ((this->targetDistanceFromHome + 10.0f) < xzDistance) {
        angleAdjustment = 0x1000;
    } else if ((this->targetDistanceFromHome - 10.0f) > xzDistance) {
        angleAdjustment = 0x6000;
    } else {
        angleAdjustment = 0x4000;
    }

    targetAngle += angleAdjustment;
    Math_SmoothStepToS(&this->actor.world.rot.y, targetAngle, 2, 0xFA0, 0x3E8);
    MMEnElforg_ApproachTargetSpeedXZ(this);
    // MM Actor_MoveWithGravity
    Actor_MoveXZGravity(&this->actor);
}

/**
 * Adjust's the Stray Fairy's speed, rotation, and y-position to bring it
 * closer to targetPos. Since it doesn't directly point the fairy towards
 * its target, it can take strange paths or even "orbit" the target.
 */
void MMEnElforg_MoveToTarget(MMEnElforg* this, Vec3f* targetPos) {
    s16 targetAngle;

    this->actor.shape.yOffset += 100.0f * Math_SinS(this->timer * 0x200);
    MMEnElforg_ApproachTargetYPosition(this, targetPos);
    targetAngle = MMEnElforg_Atan2S_XY(-(this->actor.world.pos.z - targetPos->z),
                                       -(this->actor.world.pos.x - targetPos->x));

    if (this->targetSpeedXZ > 2.0f) {
        Math_SmoothStepToS(&this->actor.world.rot.y, targetAngle, 2, 0x400, 0x100);
    } else {
        // If the speed is below a threshold, deliberately mess up the target
        // angle so the fairy "orbits" the target instead of reaching it.
        targetAngle += 0x2000;
        Math_SmoothStepToS(&this->actor.world.rot.y, targetAngle, 10, 0x200, 0x80);
    }

    MMEnElforg_ApproachTargetSpeedXZ(this);
    Actor_MoveXZGravity(&this->actor);
}

// MM func_80ACCBB8: play->actorCtx.flags |= ACTORCTX_FLAG_3.
void MMEnElforg_SetStrayFairyNearby(MMEnElforg* this, PlayState* play) {
    gMergeShipStrayFairyNearby = true;
}

void MMEnElforg_TrappedByBubble(MMEnElforg* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);

    if ((this->actor.parent == NULL) || (this->actor.parent->update == NULL)) {
        MMEnElforg_InitializeParams(this);
        this->actionFunc = MMEnElforg_FreeFloating;
    } else {
        this->actor.shape.yOffset += 10.0f * Math_SinS(this->timer * 0x200);
        this->actor.world.pos = this->actor.parent->world.pos;
        this->actor.world.pos.y += 12.0f;
    }

    MMEnElforg_SetStrayFairyNearby(this, play);
}

void MMEnElforg_TurnInFairy(MMEnElforg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 xzDistToPlayer;
    s16 rotationTemp;
    s16 newAngle;

    // This code makes the fairy briefly circle the player before
    // flying towards the fountain's center.
    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.yOffset *= 0.9f;
    this->actor.speedXZ = 5.0f;
    MMEnElforg_ApproachTargetYPosition(this, &player->bodyPartsPos[PLAYER_BODYPART_WAIST]);

    xzDistToPlayer = this->actor.xzDistToPlayer;
    if (xzDistToPlayer < 0.0f) {
        xzDistToPlayer = 10.0f;
    }

    newAngle = 0x28000 / xzDistToPlayer;
    Math_SmoothStepToF(&xzDistToPlayer, 40.0f, 0.2f, 100.0f, 1.0f);
    rotationTemp = this->actor.yawTowardsPlayer - newAngle;
    this->actor.world.pos.x = player->actor.world.pos.x - (Math_SinS(rotationTemp) * xzDistToPlayer);
    this->actor.world.pos.z = player->actor.world.pos.z - (Math_CosS(rotationTemp) * xzDistToPlayer);
    MMEnElforg_SpawnSparkles(this, play, 16);

    if (this->secondaryTimer > 0) {
        this->secondaryTimer--;
    } else {
        this->actor.world.rot.y = rotationTemp + 0x4000;
        this->timer = 0;
        this->secondaryTimer = Rand_ZeroFloat(100.0f);
        this->actor.shape.yOffset = 0.0f;
        this->targetSpeedXZ = 3.0f;
        this->targetDistanceFromHome = 50.0f;
        this->actionFunc = MMEnElforg_FreeFloatingFairyFountain;
        this->strayFairyFlags &= ~MM_STRAY_FAIRY_FLAG_CIRCLES_QUICKLY_IN_FOUNTAIN;
    }
}

void MMEnElforg_QuicklyCircleFairyFountain(MMEnElforg* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    MMEnElforg_MoveToTargetFairyFountain(this, &this->actor.home.pos);

    if (this->secondaryTimer <= 30) {
        this->actionFunc = MMEnElforg_TurnInFairy;
    }

    this->secondaryTimer--;
}

void MMEnElforg_FreeFloatingFairyFountain(MMEnElforg* this, PlayState* play) {

    if (this->strayFairyFlags & MM_STRAY_FAIRY_FLAG_MOVES_QUICKLY_TO_HOME) {
        // This happens when "turning in" the last batch of Stray Fairies to
        // a Fairy Fountain. The ones being "turned in" will fly very
        // quickly to the center of the fountain.
        if (this->targetSpeedXZ < 8.0f) {
            this->targetSpeedXZ += 0.1f;
        }

        if (this->targetDistanceFromHome > 0.0f) {
            this->targetDistanceFromHome -= 2.0f;
        }
    } else if ((this->timer & 127) == 127) {
        if (Math_Vec3f_DistXZ(&this->actor.world.pos, &this->actor.home.pos) > 150.0f) {
            this->targetSpeedXZ = 5.0f;
        } else {
            this->targetSpeedXZ = Rand_ZeroFloat(2.0f) + 1.0f;
        }

        this->targetDistanceFromHome = Rand_ZeroFloat(100.0f) + 50.0f;
    }

    SkelAnime_Update(&this->skelAnime);
    MMEnElforg_MoveToTargetFairyFountain(this, &this->actor.home.pos);

    if (this->strayFairyFlags & MM_STRAY_FAIRY_FLAG_CIRCLES_QUICKLY_IN_FOUNTAIN) {
        // A small number of fairies will do this when the player walks into
        // the center of the fountain to be healed.
        this->actionFunc = MMEnElforg_QuicklyCircleFairyFountain;
    }

    if (this->strayFairyFlags & MM_STRAY_FAIRY_FLAG_SPARKLES_AND_SHRINKS) {
        // This happens right before the Great Fairy appears once all
        // Stray Fairies are saved.
        if (MM_STRAY_FAIRY_SPARKLE_COUNT(&this->actor) > 0) {
            MMEnElforg_SpawnSparkles(this, play, 10);
            MM_STRAY_FAIRY_SPARKLE_COUNT(&this->actor)--;
        }

        Actor_SetScale(&this->actor, this->actor.scale.x * 0.9f);
        if (this->actor.scale.x < 0.001f) {
            Actor_Kill(&this->actor);
        }
    }
}

void MMEnElforg_CirclePlayer(MMEnElforg* this, PlayState* play) {
    Actor* playerActor = &GET_PLAYER(play)->actor;
    Player* player = GET_PLAYER(play);
    f32 orbitRadius;

    // MM GET_PLAYER_FORM == PLAYER_FORM_GORON
    if (player->mergeShipForm == PLAYER_FORM_GORON) {
        orbitRadius = 40.0f;
    } else {
        orbitRadius = 20.0f;
    }

    this->actor.world.pos.x = (Math_SinS(this->timer * 0x1000) * orbitRadius) + playerActor->world.pos.x;
    this->actor.world.pos.z = (Math_CosS(this->timer * 0x1000) * orbitRadius) + playerActor->world.pos.z;
    this->actor.world.pos.y = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y;

    MMEnElforg_SpawnSparkles(this, play, 16);
}

void MMEnElforg_FairyCollected(MMEnElforg* this, PlayState* play) {
    MMEnElforg_CirclePlayer(this, play);

    if (this->timer > 80) {
        Actor_Kill(&this->actor);
        return;
    }

    // MM Actor_PlaySfx_Flagged(NA_SE_PL_CHIBI_FAIRY_HEAL - SFX_FLAG)
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_CHIBI_FAIRY_HEAL - SFX_FLAG);
}

void MMEnElforg_SetupFairyCollected(MMEnElforg* this, PlayState* play) {
    Actor* playerActor = &GET_PLAYER(play)->actor;
    Player* player = GET_PLAYER(play);

    this->actor.world.pos.x = playerActor->world.pos.x;
    this->actor.world.pos.y = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y;
    this->actor.world.pos.z = playerActor->world.pos.z;
    this->actionFunc = MMEnElforg_FairyCollected;
    this->timer = 0;
    this->secondaryTimer = 0;
    this->actor.shape.yOffset = 0.0f;
}

void MMEnElforg_ClockTownFairyCollected(MMEnElforg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    MMEnElforg_CirclePlayer(this, play);

    player->actor.freezeTimer = 100;
    // MM PLAYER_STATE1_20000000
    player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;

    if (Actor_TextboxIsClosing(&this->actor, play)) {
        player->actor.freezeTimer = 0;
        player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
        Actor_Kill(&this->actor);
        // MM SET_WEEKEVENTREG(WEEKEVENTREG_08_80). MM's CutsceneManager_Stop(CS_ID_GLOBAL_TALK) has no counterpart.
        MergeShip_StrayFairy_Give(play, MM_STRAY_FAIRY_AREA_CLOCK_TOWN);
        return;
    }

    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_CHIBI_FAIRY_HEAL - SFX_FLAG);
    // MM queues/starts CS_ID_GLOBAL_TALK here (see the file comment).
}

void MMEnElforg_FreeFloating(MMEnElforg* this, PlayState* play) {
    Vec3f pos;
    f32 scaledYDistance;
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);

    // MM Player_GetMask(play) == PLAYER_MASK_GREAT_FAIRY
    if (MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GREAT_FAIRY)) {
        pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
        this->targetSpeedXZ = 5.0f;
        MMEnElforg_MoveToTarget(this, &pos);
    } else {
        this->targetSpeedXZ = 1.0f;
        MMEnElforg_MoveToTarget(this, &this->actor.home.pos);
    }

    // MM playerHeightRel
    scaledYDistance = this->actor.yDistToPlayer - (this->actor.shape.yOffset * this->actor.scale.y);

    if (!Player_InCsMode(play)) {
        if ((this->actor.xzDistToPlayer < 30.0f) && (scaledYDistance < 12.0f) && (scaledYDistance > -68.0f)) {
            s32 flag = MM_STRAY_FAIRY_GET_FLAG(&this->actor);

            MMEnElforg_SetupFairyCollected(this, play);
            Health_ChangeBy(play, 0x30);

            if (flag != MERGESHIP_STRAY_FAIRY_FLAG_NONE) {
                switch (MM_STRAY_FAIRY_TYPE(&this->actor)) {
                    case MM_STRAY_FAIRY_TYPE_COLLECTIBLE:
                        Flags_SetCollectible(play, flag);
                        break;

                    case MM_STRAY_FAIRY_TYPE_CHEST:
                        Flags_SetTreasure(play, flag);
                        break;

                    default:
                        Flags_SetSwitch(play, flag);
                        break;
                }
            }

            // MM: the Clock Town fairy's plea (Message_StartTextbox 0x579) or a temple fairy's count and
            // "You found a Stray Fairy!" (0x11), see MergeShip_StrayFairy_OnCollected.
            if (MergeShip_StrayFairy_OnCollected(play, &this->actor, this->area, MM_STRAY_FAIRY_TYPE(&this->actor),
                                                 this->userData)) {
                player->actor.freezeTimer = 100;
                player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
                this->actionFunc = MMEnElforg_ClockTownFairyCollected;
                return;
            }
        }
        // MM UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4 (walls, ceiling, floor).
        Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 20.0f, 1 | 2 | 4);
        MMEnElforg_SetStrayFairyNearby(this, play);

        if (MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GREAT_FAIRY)) {
            if (!(this->strayFairyFlags & MM_STRAY_FAIRY_FLAG_GREAT_FAIRYS_MASK_EQUIPPED)) {
                // MM Audio_PlaySfx(NA_SE_SY_FAIRY_MASK_SUCCESS)
                MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_FAIRY_MASK_SUCCESS);
            }

            this->strayFairyFlags |= MM_STRAY_FAIRY_FLAG_GREAT_FAIRYS_MASK_EQUIPPED;
        } else {
            this->strayFairyFlags &= ~MM_STRAY_FAIRY_FLAG_GREAT_FAIRYS_MASK_EQUIPPED;
        }
    }
}

/**
 * This finds the enemy that is "holding" the Stray Fairy hostage. When
 * this enemy is killed, the Stray Fairy will spawn. This function only
 * works if the enemy and the Stray Fairy have the exact same home
 * coordinates when the Stray Fairy first spawns.
 */
Actor* MMEnElforg_GetHoldingEnemy(MMEnElforg* this, PlayState* play) {
    Actor* enemy;

    for (enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        if ((enemy->home.pos.x == this->actor.home.pos.x) && (enemy->home.pos.y == this->actor.home.pos.y) &&
            (enemy->home.pos.z == this->actor.home.pos.z)) {
            return enemy;
        }
    }

    return NULL;
}

void MMEnElforg_TrappedByEnemy(MMEnElforg* this, PlayState* play) {
    f32 posTemp;

    if (this->enemy->update == NULL) {
        MMEnElforg_InitializeParams(this);
        this->actionFunc = MMEnElforg_FreeFloating;
        this->actor.draw = MMEnElforg_Draw;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_CHIBI_FAIRY_SAVED);
    } else {
        // The enemy is still alive, so have the Stray Fairy
        // track the enemy in case it's moving around.
        posTemp = this->enemy->world.pos.x;
        this->actor.world.pos.x = posTemp;
        this->actor.home.pos.x = posTemp;
        posTemp = this->enemy->world.pos.y + 30.0f;
        this->actor.world.pos.y = posTemp;
        this->actor.home.pos.y = posTemp;
        posTemp = this->enemy->world.pos.z;
        this->actor.world.pos.z = posTemp;
        this->actor.home.pos.z = posTemp;
    }

    MMEnElforg_SetStrayFairyNearby(this, play);
}

void MMEnElforg_SetupTrappedByEnemy(MMEnElforg* this, PlayState* play) {
    Actor* enemy = MMEnElforg_GetHoldingEnemy(this, play);

    if ((enemy != NULL) && (enemy->update != NULL)) {
        this->actionFunc = MMEnElforg_TrappedByEnemy;
        this->enemy = enemy;
    }
}

void MMEnElforg_HiddenByCollider(MMEnElforg* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        MMEnElforg_InitializeParams(this);
        this->actionFunc = MMEnElforg_FreeFloating;
        this->actor.draw = MMEnElforg_Draw;
        this->actor.world.pos.y += 40.0f;
        this->actor.home.pos.y += 40.0f;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_CHIBI_FAIRY_SAVED);
    } else {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }

    MMEnElforg_SetStrayFairyNearby(this, play);
}

// MergeShip2: "Stray Fairies" turned off in the Mergeship menu takes effect at once: a fairy still waiting to be found
// (free, in a bubble, held by an enemy, hidden by a collider) stops, cannot be touched and is not drawn -- nor sensed
// by the Great Fairy's Mask -- until the option is on again. One already being collected finishes (its text and the
// player's freeze are under way).
static s32 MMEnElforg_PausedByOption(MMEnElforg* this) {
    if (MergeShip_StrayFairy_IsEnabled()) {
        return false;
    }
    return (this->actionFunc == MMEnElforg_FreeFloating) || (this->actionFunc == MMEnElforg_TrappedByBubble) ||
           (this->actionFunc == MMEnElforg_TrappedByEnemy) || (this->actionFunc == MMEnElforg_SetupTrappedByEnemy) ||
           (this->actionFunc == MMEnElforg_HiddenByCollider);
}

void MMEnElforg_Update(Actor* thisx, PlayState* play) {
    MMEnElforg* this = (MMEnElforg*)thisx;

    if (MMEnElforg_PausedByOption(this)) {
        return;
    }

    this->actionFunc(this, play);

    if ((this->timer == 0) && (this->secondaryTimer > 0)) {
        this->secondaryTimer--;
    } else {
        this->timer++;
    }

    if (this->direction < 0) {
        this->direction++;
        if (this->direction == 0) {
            this->direction = Rand_ZeroFloat(20.0f) + 20.0f;
        }
    } else if (this->direction > 0) {
        this->direction--;
    } else {
        this->direction = -Rand_ZeroFloat(20.0f) - 20.0f;
    }
}

s32 MMEnElforg_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                Gfx** gfx) {
    MMEnElforg* this = (MMEnElforg*)thisx;

    if (this->direction < 0) {
        if (limbIndex == STRAY_FAIRY_LIMB_LEFT_FACING_HEAD) {
            *dList = NULL;
        }
    } else if (limbIndex == STRAY_FAIRY_LIMB_RIGHT_FACING_HEAD) {
        *dList = NULL;
    }

    return false;
}

// MM's per-area animated materials (gStrayFairy<Area>TexAnim), by MMStrayFairyArea.
static const char* sStrayFairyTexAnims[MM_STRAY_FAIRY_AREA_MAX] = {
    mm_gStrayFairyClockTownTexAnim, mm_gStrayFairyWoodfallTexAnim,   mm_gStrayFairySnowheadTexAnim,
    mm_gStrayFairyGreatBayTexAnim,  mm_gStrayFairyStoneTowerTexAnim,
};

void MMEnElforg_Draw(Actor* thisx, PlayState* play) {
    MMEnElforg* this = (MMEnElforg*)thisx;
    s32 area = ((this->area > MM_STRAY_FAIRY_AREA_CLOCK_TOWN) && (this->area < MM_STRAY_FAIRY_AREA_MAX))
                   ? this->area
                   : MM_STRAY_FAIRY_AREA_CLOCK_TOWN; // MM's default case

    if (MMEnElforg_PausedByOption(this)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // MM AnimatedMat_Draw(play, Lib_SegmentedToVirtual(gStrayFairy<Area>TexAnim))
    MergeShip_AnimatedMat_Draw(play, (void*)sStrayFairyTexAnims[area]);

    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);

    POLY_XLU_DISP =
        SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                           MMEnElforg_OverrideLimbDraw, NULL, &this->actor, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}
