/*
 * File: z_en_dog.c
 * Overlay: ovl_En_Dog
 * Description: Dog
 */

#include "z_en_dog.h"
#include "objects/object_dog/object_dog.h"
// MergeShip2: MM EnDg's Bremen Mask follower.
#include "soh/MergeShip/MergeShipBremen.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
// MergeShip2 (soh_fd): MM EnDg's reactions to the Deku, Goron and Zora.
#include "soh/MergeShip/MergeShipSohFd.h"
#include "mm/objects/object_dog/object_dog.h"

#define FLAGS 0

void EnDog_Init(Actor* thisx, PlayState* play);
void EnDog_Destroy(Actor* thisx, PlayState* play);
void EnDog_Update(Actor* thisx, PlayState* play);
void EnDog_Draw(Actor* thisx, PlayState* play);
void EnDog_Reset(void); // MergeShip2: MM EnDg_Reset -- clears the Bremen Mask follower

// MergeShip2: MM EnDg's Bremen Mask follower (z_en_dg.c).
void EnDog_BremenSetupApproachPlayer(EnDog* this, PlayState* play);
void EnDog_BremenApproachPlayer(EnDog* this, PlayState* play);
void EnDog_BremenSitNextToPlayer(EnDog* this, PlayState* play);

// MergeShip2: MM EnDg sBremenMaskFollowerIndex -- the dog itself here (NULL is MM's
// ENDG_INDEX_NO_BREMEN_MASK_FOLLOWER), since Ocarina of Time's dogs carry no MM index.
static EnDog* sBremenMaskFollower = NULL;

void EnDog_FollowPath(EnDog* this, PlayState* play);
void EnDog_ChooseMovement(EnDog* this, PlayState* play);
void EnDog_FollowPlayer(EnDog* this, PlayState* play);
void EnDog_RunAway(EnDog* this, PlayState* play);
void EnDog_FaceLink(EnDog* this, PlayState* play);
void EnDog_Wait(EnDog* this, PlayState* play);

// MergeShip2 (soh_fd): MM EnDg's reactions to the Deku, Goron and Zora (z_en_dg.c).
void EnDog_MMBackAwayFromGoron(EnDog* this, PlayState* play);
void EnDog_MMRunAwayFromGoron(EnDog* this, PlayState* play);
void EnDog_MMBarkAtGoron(EnDog* this, PlayState* play);
void EnDog_MMApproachPlayerToAttack(EnDog* this, PlayState* play);
void EnDog_MMRunAfterAttacking(EnDog* this, PlayState* play);
void EnDog_MMSitNextToPlayer(EnDog* this, PlayState* play);
void EnDog_MMJumpAttack(EnDog* this, PlayState* play);
void EnDog_MMFall(EnDog* this, PlayState* play);
void EnDog_MMApproachPlayer(EnDog* this, PlayState* play);
void EnDog_MMSlowlyBackUpBeforeAttacking(EnDog* this, PlayState* play);

const ActorInit En_Dog_InitVars = {
    ACTOR_EN_DOG,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_DOG,
    sizeof(EnDog),
    (ActorFunc)EnDog_Init,
    (ActorFunc)EnDog_Destroy,
    (ActorFunc)EnDog_Update,
    (ActorFunc)EnDog_Draw,
    (ActorResetFunc)EnDog_Reset, // MergeShip2: MM En_Dg_Profile's EnDg_Reset
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 16, 20, 0, { 0 } },
};

static CollisionCheckInfoInit2 sColChkInfoInit = { 0, 0, 0, 0, 50 };

typedef enum {
    /* 0 */ ENDOG_ANIM_0,
    /* 1 */ ENDOG_ANIM_1,
    /* 2 */ ENDOG_ANIM_2,
    /* 3 */ ENDOG_ANIM_3,
    /* 4 */ ENDOG_ANIM_4,
    /* 5 */ ENDOG_ANIM_5,
    /* 6 */ ENDOG_ANIM_6,
    /* 7 */ ENDOG_ANIM_7
} EnDogAnimation;

static AnimationInfo sAnimationInfo[] = {
    { &gDogWalkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },
    { &gDogWalkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, -6.0f },
    { &gDogRunAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, -6.0f },
    { &gDogBarkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, -6.0f },
    { &gDogSitAnim, 1.0f, 0.0f, 4.0f, ANIMMODE_ONCE, -6.0f },
    { &gDogSitAnim, 1.0f, 5.0f, 25.0f, ANIMMODE_LOOP_PARTIAL, -6.0f },
    { &gDogBowAnim, 1.0f, 0.0f, 6.0f, ANIMMODE_ONCE, -6.0f },
    { &gDogBow2Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, -6.0f },
};

typedef enum {
    /* 0x00 */ DOG_WALK,
    /* 0x01 */ DOG_RUN,
    /* 0x02 */ DOG_BARK,
    /* 0x03 */ DOG_SIT,
    /* 0x04 */ DOG_SIT_2,
    /* 0x05 */ DOG_BOW,
    /* 0x06 */ DOG_BOW_2
} DogBehavior;

void EnDog_PlayWalkSFX(EnDog* this) {
    AnimationHeader* walk = &gDogWalkAnim;

    if (this->skelAnime.animation == walk) {
        if ((this->skelAnime.curFrame == 1.0f) || (this->skelAnime.curFrame == 7.0f)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_CHIBI_WALK);
        }
    }
}

void EnDog_PlayRunSFX(EnDog* this) {
    AnimationHeader* run = &gDogRunAnim;

    if (this->skelAnime.animation == run) {
        if ((this->skelAnime.curFrame == 2.0f) || (this->skelAnime.curFrame == 4.0f)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_CHIBI_WALK);
        }
    }
}

void EnDog_PlayBarkSFX(EnDog* this) {
    AnimationHeader* bark = &gDogBarkAnim;

    if (this->skelAnime.animation == bark) {
        if ((this->skelAnime.curFrame == 13.0f) || (this->skelAnime.curFrame == 19.0f)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_SMALL_DOG_BARK);
        }
    }
}

s32 EnDog_PlayAnimAndSFX(EnDog* this) {
    s32 animation;

    if (this->behavior != this->nextBehavior) {
        if (this->nextBehavior == DOG_SIT_2) {
            this->nextBehavior = DOG_SIT;
        }
        if (this->nextBehavior == DOG_BOW_2) {
            this->nextBehavior = DOG_BOW;
        }

        this->behavior = this->nextBehavior;
        switch (this->behavior) {
            case DOG_WALK:
                animation = ENDOG_ANIM_1;
                break;
            case DOG_RUN:
                animation = ENDOG_ANIM_2;
                break;
            case DOG_BARK:
                animation = ENDOG_ANIM_3;
                break;
            case DOG_SIT:
                animation = ENDOG_ANIM_4;
                break;
            case DOG_BOW:
                animation = ENDOG_ANIM_6;
                break;
        }
        Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, animation);
    }

    switch (this->behavior) {
        case DOG_SIT:
            if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
                Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, ENDOG_ANIM_5);
                this->behavior = this->nextBehavior = DOG_SIT_2;
            }
            break;
        case DOG_BOW:
            if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
                Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, ENDOG_ANIM_7);
                this->behavior = this->nextBehavior = DOG_BOW_2;
            }
            break;
        case DOG_WALK:
            EnDog_PlayWalkSFX(this);
            break;
        case DOG_RUN:
            EnDog_PlayRunSFX(this);
            break;
        case DOG_BARK:
            EnDog_PlayBarkSFX(this);
            break;
    }
    return 0;
}

s8 EnDog_CanFollow(EnDog* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        return 2;
    }

    if (play->sceneNum == SCENE_MARKET_DAY) {
        return 0;
    }

    if (this->collider.base.ocFlags2 & OC2_HIT_PLAYER) {
        this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;
        if (gSaveContext.dogParams != 0) {
            return 0;
        }
        gSaveContext.dogParams = (this->actor.params & 0x7FFF);
        return 1;
    }

    return 0;
}

s32 EnDog_UpdateWaypoint(EnDog* this, PlayState* play) {
    s32 change;

    if (this->path == NULL) {
        return 0;
    }

    if (this->reverse) {
        change = -1;
    } else {
        change = 1;
    }

    this->waypoint += change;

    if (this->reverse) {
        if (this->waypoint < 0) {
            this->waypoint = this->path->count - 1;
        }
    } else {
        if ((this->path->count - 1) < this->waypoint) {
            this->waypoint = 0;
        }
    }

    return 1;
}

s32 EnDog_Orient(EnDog* this, PlayState* play) {
    s16 targetYaw;
    f32 waypointDistSq;

    waypointDistSq = Path_OrientAndGetDistSq(&this->actor, this->path, this->waypoint, &targetYaw);
    Math_SmoothStepToS(&this->actor.world.rot.y, targetYaw, 10, 1000, 1);

    if ((waypointDistSq > 0.0f) && (waypointDistSq < 1000.0f)) {
        return EnDog_UpdateWaypoint(this, play);
    } else {
        return 0;
    }
}

void EnDog_Init(Actor* thisx, PlayState* play) {
    EnDog* this = (EnDog*)thisx;
    s16 followingDog;
    s32 pad;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 24.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, &gDogSkel, NULL, this->jointTable, this->morphTable, 13);
    Animation_ChangeByInfo(&this->skelAnime, sAnimationInfo, ENDOG_ANIM_0);

    if ((this->actor.params & 0x8000) == 0) {
        this->actor.params = (this->actor.params & 0xF0FF) | ((((this->actor.params & 0x0F00) >> 8) + 1) << 8);
    }

    followingDog = ((gSaveContext.dogParams & 0x0F00) >> 8);
    if (followingDog == ((this->actor.params & 0x0F00) >> 8) && ((this->actor.params & 0x8000) == 0)) {
        Actor_Kill(&this->actor);
        return;
    }

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    // MergeShip2 (soh_fd): MM EnDg's attack (sCylinderInit's toucher), submitted only while the dog lunges at a Deku
    // (EnDog_MMUpdateCollision). Its damage is 0: the lunge knocks the Deku over, it does not hurt him.
    this->collider.base.atFlags = AT_ON | AT_TYPE_ENEMY;
    this->collider.info.toucherFlags = TOUCH_ON | TOUCH_SFX_NORMAL;
    this->collider.info.toucher.dmgFlags = 0xF7CFFFFF;
    this->collider.info.toucher.effect = 0x04;
    this->collider.info.toucher.damage = 0x00;
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, 0, &sColChkInfoInit);
    Actor_SetScale(&this->actor, 0.0075f);
    this->waypoint = 0;
    this->actor.gravity = -1.0f;
    this->path = Path_GetByIndex(play, (this->actor.params & 0x00F0) >> 4, 0xF);

    switch (play->sceneNum) {
        case SCENE_MARKET_NIGHT:
            if ((!gSaveContext.dogIsLost) && (((this->actor.params & 0x0F00) >> 8) == 1)) {
                Actor_Kill(&this->actor);
            }
            break;
        case SCENE_DOG_LADY_HOUSE: // Richard's Home
            if (!(this->actor.params & 0x8000)) {
                if (!gSaveContext.dogIsLost) {
                    this->nextBehavior = DOG_SIT;
                    this->actionFunc = EnDog_Wait;
                    this->actor.speedXZ = 0.0f;
                    return;
                } else {
                    Actor_Kill(&this->actor);
                    return;
                }
            }
            break;
    }

    if (this->actor.params & 0x8000) {
        this->nextBehavior = DOG_WALK;
        this->actionFunc = EnDog_FollowPlayer;
    } else {
        this->nextBehavior = DOG_SIT;
        this->actionFunc = EnDog_ChooseMovement;
    }
}

void EnDog_Destroy(Actor* thisx, PlayState* play) {
    EnDog* this = (EnDog*)thisx;
    Collider_DestroyCylinder(play, &this->collider);

    // MergeShip2: MM EnDg -- the follower is gone (MM keeps an index, which cannot dangle).
    if (sBremenMaskFollower == this) {
        sBremenMaskFollower = NULL;
    }
}

void EnDog_FollowPath(EnDog* this, PlayState* play) {
    s32 behaviors[] = { DOG_SIT, DOG_BOW, DOG_BARK };
    s32 unused[] = { 40, 80, 20 };
    f32 speed;
    s32 frame;

    if (EnDog_CanFollow(this, play) == 1) {
        this->actionFunc = EnDog_FollowPlayer;
    }

    if (DECR(this->behaviorTimer) != 0) {
        if (this->nextBehavior == DOG_WALK) {
            speed = 1.0f;
        } else {
            speed = 4.0f;
        }
        Math_SmoothStepToF(&this->actor.speedXZ, speed, 0.4f, 1.0f, 0.0f);
        EnDog_Orient(this, play);
        this->actor.shape.rot = this->actor.world.rot;

        // Used to change between two text boxes for Richard's owner in the Market Day scene
        // depending on where he is on his path. En_Hy checks these event flags.
        if (this->waypoint < 9) {
            // Richard is close to her, text says something about his coat
            gSaveContext.eventInf[3] |= 1;
        } else {
            // Richard is far, text says something about running fast
            gSaveContext.eventInf[3] &= ~1;
        }
    } else {
        frame = play->state.frames % 3;
        this->nextBehavior = behaviors[frame];
        // no clue why they're using the behavior id to calculate timer. possibly meant to use the unused array?
        this->behaviorTimer = Rand_S16Offset(60, behaviors[frame]);
        this->actionFunc = EnDog_ChooseMovement;
    }
}

void EnDog_ChooseMovement(EnDog* this, PlayState* play) {
    if (EnDog_CanFollow(this, play) == 1) {
        this->actionFunc = EnDog_FollowPlayer;
    }

    if (DECR(this->behaviorTimer) == 0) {
        this->behaviorTimer = Rand_S16Offset(200, 100);
        if (play->state.frames % 2) {
            this->nextBehavior = DOG_WALK;
        } else {
            this->nextBehavior = DOG_RUN;
        }

        if (this->nextBehavior == DOG_RUN) {
            this->behaviorTimer /= 2;
        }
        this->actionFunc = EnDog_FollowPath;
    }
    Math_SmoothStepToF(&this->actor.speedXZ, 0.0f, 0.4f, 1.0f, 0.0f);
}

void EnDog_FollowPlayer(EnDog* this, PlayState* play) {
    f32 speed;

    if (gSaveContext.dogParams == 0) {
        this->nextBehavior = DOG_SIT;
        this->actionFunc = EnDog_Wait;
        this->actor.speedXZ = 0.0f;
        return;
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0)) {
        // If the dog is too far away it's usually because they are stuck in a hole or on a different floor, this gives
        // them a push
        if (this->actor.xyzDistToPlayerSq > 250000.0f) {
            Player* player = GET_PLAYER(play);
            if (PlayerGrounded(player))
                this->actor.world.pos.y = player->actor.world.pos.y;
        }

        // If doggo is in the water make sure it's floating
        if (this->actor.bgCheckFlags & 0x20) {
            this->actor.gravity = 0.0f;
            if (this->actor.yDistToWater > 11.0f) {
                this->actor.world.pos.y += 2.0f;
            } else if (this->actor.yDistToWater < 8.0f) {
                this->actor.world.pos.y -= 2.0f;
            }
        } else {
            this->actor.gravity = -1.0f;
        }
    }

    if (this->actor.xzDistToPlayer > 400.0f) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0)) {
            // Instead of stopping following when the dog gets too far, just speed them up.
            speed = this->actor.xzDistToPlayer / 25.0f;
        } else {
            if (this->nextBehavior != DOG_SIT && this->nextBehavior != DOG_SIT_2) {
                this->nextBehavior = DOG_BOW;
            }
            gSaveContext.dogParams = 0;
            speed = 0.0f;
        }
    } else if (this->actor.xzDistToPlayer > 100.0f) {
        this->nextBehavior = DOG_RUN;
        speed = 4.0f;
    } else if (this->actor.xzDistToPlayer < 40.0f) {
        if (this->nextBehavior != DOG_BOW && this->nextBehavior != DOG_BOW_2) {
            this->nextBehavior = DOG_BOW;
        }
        speed = 0.0f;
    } else {
        this->nextBehavior = DOG_WALK;
        speed = 1.0f;
    }

    Math_ApproachF(&this->actor.speedXZ, speed, 0.6f, 1.0f);

    if (!(this->actor.xzDistToPlayer > 400.0f) || CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0)) {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 10, 1000, 1);
        this->actor.shape.rot = this->actor.world.rot;
    }
}

void EnDog_RunAway(EnDog* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < 200.0f) {
        Math_ApproachF(&this->actor.speedXZ, 4.0f, 0.6f, 1.0f);
        Math_SmoothStepToS(&this->actor.world.rot.y, (this->actor.yawTowardsPlayer ^ 0x8000), 10, 1000, 1);
    } else {
        this->actionFunc = EnDog_FaceLink;
    }
    this->actor.shape.rot = this->actor.world.rot;
}

void EnDog_FaceLink(EnDog* this, PlayState* play) {
    s16 rotTowardLink;
    s16 prevRotY;
    f32 absAngleDiff;

    // if the dog is more than 200 units away from Link, turn to face him then wait
    if (200.0f <= this->actor.xzDistToPlayer) {
        this->nextBehavior = DOG_WALK;

        Math_ApproachF(&this->actor.speedXZ, 1.0f, 0.6f, 1.0f);

        rotTowardLink = this->actor.yawTowardsPlayer;
        prevRotY = this->actor.world.rot.y;
        Math_SmoothStepToS(&this->actor.world.rot.y, rotTowardLink, 10, 1000, 1);

        absAngleDiff = this->actor.world.rot.y;
        absAngleDiff -= prevRotY;
        absAngleDiff = fabsf(absAngleDiff);
        if (absAngleDiff < 200.0f) {
            this->nextBehavior = DOG_SIT;
            this->actionFunc = EnDog_Wait;
            this->actor.speedXZ = 0.0f;
        }
    } else {
        this->nextBehavior = DOG_RUN;
        this->actionFunc = EnDog_RunAway;
    }
    this->actor.shape.rot = this->actor.world.rot;
}

void EnDog_Wait(EnDog* this, PlayState* play) {
    this->unusedAngle = (this->actor.yawTowardsPlayer - this->actor.shape.rot.y);

    // If another dog is following Link and he gets within 200 units of waiting dog, run away
    if ((gSaveContext.dogParams != 0) && (this->actor.xzDistToPlayer < 200.0f)) {
        this->nextBehavior = DOG_RUN;
        this->actionFunc = EnDog_RunAway;
    }
}

// MergeShip2: MM EnDg EnDg_FindFollowerForBremenMask -- the dog that follows a marching Link: the one he has
// targeted, else the nearest. A dog already following Link for the lost dog quest (EnDog_FollowPlayer) is not a
// candidate, so it keeps its quest state.
s32 EnDog_FindFollowerForBremenMask(PlayState* play) {
    Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].head; // MM EnDg is ACTORCAT_ENEMY
    f32 minDist = 9999.0f;
    f32 dist;

    while (npc != NULL) {
        if ((npc->id == ACTOR_EN_DOG) && (npc->update != NULL) &&
            (((EnDog*)npc)->actionFunc != EnDog_FollowPlayer)) {
            if (npc->isTargeted) { // MM isLockedOn
                sBremenMaskFollower = (EnDog*)npc;
                return true;
            }

            dist = npc->xzDistToPlayer;
            if (dist < minDist) {
                sBremenMaskFollower = (EnDog*)npc;
                minDist = dist;
            }
        }

        npc = npc->next;
    }

    if (sBremenMaskFollower != NULL) {
        return true;
    }

    return false;
}

// MergeShip2: MM EnDg -- MM's release puts the dog back on its path (EnDg_SetupIdleMove, EnDg_IdleMove). Ocarina
// of Time's dogs go back to what they were doing: roaming their path, or sitting.
void EnDog_BremenReturnToIdle(EnDog* this, PlayState* play) {
    this->actor.gravity = -1.0f; // EnDog_Init's (MM's approach used its own -3.0f)
    // Touching Link while following must not count as the lost dog quest's touch afterwards; a new touch still does.
    this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;

    if ((this->bremenPrevActionFunc == EnDog_ChooseMovement) || (this->bremenPrevActionFunc == EnDog_FollowPath)) {
        this->behaviorTimer = 0; // choose the next leg of the path at once, as MM's dog resumes its path
        this->actionFunc = EnDog_ChooseMovement;
    } else {
        this->nextBehavior = DOG_SIT;
        this->actor.speedXZ = 0.0f;
        this->actionFunc = EnDog_Wait;
    }
    this->bremenPrevActionFunc = NULL;
}

// MergeShip2: MM EnDg EnDg_CheckForBremenMaskMarch
// This checks for two conditions:
// - If the player has started marching with the Bremen Mask, then it makes the dog
//   start following the player around (assuming it's the best candidate found in
//   EnDog_FindFollowerForBremenMask above).
// - If the player has stopped marching with the Bremen Mask, and this dog was
//   following them around, then it returns the dog to its idle state.
void EnDog_CheckForBremenMaskMarch(EnDog* this, PlayState* play) {
    if (MergeShip_BremenMarching(play)) { // MM player->stateFlags3 & PLAYER_STATE3_20000000
        if (sBremenMaskFollower == NULL) {
            EnDog_FindFollowerForBremenMask(play);
        }

        if (this == sBremenMaskFollower) {
            if (!this->bremenFollowing) {
                this->bremenFollowing = true;
                this->bremenPrevActionFunc = this->actionFunc;
                this->nextBehavior = DOG_WALK; // MM DOG_ANIM_WALK
                // MM's EnDg_SetupBremenMaskApproachPlayer does not move the dog; Ocarina of Time's EnDog_Update
                // moves every dog, so it stands still this way.
                this->actor.speedXZ = 0.0f;
                this->actionFunc = EnDog_BremenSetupApproachPlayer;
            } else if ((this->actionFunc == EnDog_BremenApproachPlayer) ||
                       (this->actionFunc == EnDog_BremenSitNextToPlayer)) {
                if (DECR(this->bremenBarkTimer) == 0) {
                    this->bremenBarkTimer = 10;
                    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_BARK);
                }
            }
        }
    } else if (this == sBremenMaskFollower) {
        this->bremenFollowing = false;
        sBremenMaskFollower = NULL;
        EnDog_BremenReturnToIdle(this, play);
    }
}

// MergeShip2: MM EnDg EnDg_SetupBremenMaskApproachPlayer -- waits for a beat of the Bremen March.
void EnDog_BremenSetupApproachPlayer(EnDog* this, PlayState* play) {
    if (MergeShip_Bremen_OnMarchBeat()) { // MM func_801A46F8() == 1
        this->bremenBarkTimer = 10;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_BARK);
        this->actionFunc = EnDog_BremenApproachPlayer;
    }
}

// MergeShip2: MM EnDg EnDg_ApproachPlayer -- approach the player non-aggressively. If the player stops moving,
// then eventually the dog will sit down next to them.
void EnDog_BremenApproachPlayer(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if ((this->actor.xzDistToPlayer < 60.0f) && (this->collider.base.ocFlags1 & OC1_HIT)) {
        this->actor.shape.rot.y += 0x71C;
    } else {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 40.0f) {
        this->nextBehavior = DOG_SIT; // MM DOG_ANIM_SIT_DOWN
        this->actionFunc = EnDog_BremenSitNextToPlayer;
    } else if (MergeShip_BremenMarching(play)) {
        if ((this->actor.xzDistToPlayer > 40.0f) && (player->linearVelocity == 0.0f)) { // MM player->speedXZ
            Math_ApproachF(&this->actor.speedXZ, 1.5f, 0.2f, 1.0f);
        } else {
            Math_ApproachF(&this->actor.speedXZ, player->actor.speedXZ, 0.2f, 1.0f);
        }
    } else {
        Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.2f, 1.0f);
    }
    // (MM EnDg_CheckForBremenMaskMarch runs from EnDog_Update, and EnDog_Update moves the dog.)
}

// MergeShip2: MM EnDg EnDg_SitNextToPlayer -- makes the dog sit next to the player until they get too far away,
// at which point it will attempt to approach them again.
void EnDog_BremenSitNextToPlayer(EnDog* this, PlayState* play) {
    // MM's sitting dog does not move (EnDg_SitNextToPlayer never calls Actor_MoveWithGravity).
    this->actor.speedXZ = 0.0f;

    if (this->actor.xzDistToPlayer < 50.0f) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else {
        if (MergeShip_BremenMarching(play)) {
            this->nextBehavior = DOG_WALK; // MM DOG_ANIM_WALK
        } else {
            this->nextBehavior = DOG_RUN; // MM DOG_ANIM_RUN
        }

        this->actionFunc = EnDog_BremenApproachPlayer;
    }
}

// ---- MergeShip2 (soh_fd): MM EnDg's reactions to the Deku, Goron and Zora -------------------------------------------
//
// Majora's Mask's dogs react to Link's forms (2S2H z_en_dg.c): they back away from the Goron growling, turn and run
// whimpering, and bark at it from a distance; they run up to the Zora with a bark and sit by it whining; they chase
// the Deku and lunge at it, knocking it over. Which dogs react to which form follows the "Dogs React to ..." options.
// The actions are MM's, with MM's animations (mm.o2r's object_dog, whose skeleton Ocarina of Time's dog shares) and
// MM's sounds. While one runs, the dog updates as MM's does -- the action moves it (EnDog_Update).

#define DOG_FLAG_JUMP_ATTACKING (1 << 1) // MM
#define DOG_FLAG_BOUNCED (1 << 3)        // MM

typedef enum {
    /* 0 */ DOG_BEHAVIOR_INITIAL,
    /* 1 */ DOG_BEHAVIOR_HUMAN,
    /* 2 */ DOG_BEHAVIOR_GORON,
    /* 3 */ DOG_BEHAVIOR_GORON_WAIT,
    /* 4 */ DOG_BEHAVIOR_ZORA,
    /* 5 */ DOG_BEHAVIOR_ZORA_WAIT,
    /* 6 */ DOG_BEHAVIOR_DEKU,
    /* 7 */ DOG_BEHAVIOR_DEKU_WAIT,
    /* 8 */ DOG_BEHAVIOR_DEFAULT
} DogFormBehavior; // MM DogBehavior

typedef enum {
    /* 0 */ DOG_MM_ANIM_WALK,           // MM DOG_ANIM_WALK
    /* 1 */ DOG_MM_ANIM_RUN,            // MM DOG_ANIM_RUN
    /* 2 */ DOG_MM_ANIM_BARK,           // MM DOG_ANIM_BARK
    /* 3 */ DOG_MM_ANIM_SIT_DOWN,       // MM DOG_ANIM_SIT_DOWN
    /* 4 */ DOG_MM_ANIM_WALK_BACKWARDS, // MM DOG_ANIM_WALK_BACKWARDS
    /* 5 */ DOG_MM_ANIM_JUMP_ATTACK,    // MM DOG_ANIM_JUMP_ATTACK
    /* 6 */ DOG_MM_ANIM_MAX
} DogMMAnimation;

// MM AnimationInfoS: the frame count is the end frame, or -1 for the last one.
typedef struct {
    AnimationHeader* animation;
    f32 playSpeed;
    s16 startFrame;
    s16 frameCount;
    u8 mode;
    s16 morphFrames;
} DogMMAnimationInfo;

// MM sAnimationInfo, the entries the reactions use. It is shared by every dog and changed as MM changes it: the lunge's
// speed is re-rolled for each lunge, and the backing-up speed stays -1.0 once a dog has backed up to lunge.
static DogMMAnimationInfo sMMAnimationInfo[DOG_MM_ANIM_MAX] = {
    { (AnimationHeader*)mm_gDogWalkAnim, 1.0f, 0, -1, ANIMMODE_LOOP, -6 },
    { (AnimationHeader*)mm_gDogRunAnim, 1.0f, 0, -1, ANIMMODE_LOOP, 0 },
    { (AnimationHeader*)mm_gDogBarkAnim, 1.0f, 0, -1, ANIMMODE_LOOP, -6 },
    { (AnimationHeader*)mm_gDogSitAnim, 1.0f, 0, -1, ANIMMODE_LOOP_PARTIAL, -6 },
    { (AnimationHeader*)mm_gDogWalkAnim, -1.5f, -1, 0, ANIMMODE_LOOP, -6 },
    { (AnimationHeader*)mm_gDogJumpAttackAnim, 1.2f, 0, -1, ANIMMODE_ONCE, 0 },
};

// MM EnDg_ChangeAnim.
static void EnDog_MMChangeAnim(SkelAnime* skelAnime, s32 animIndex) {
    DogMMAnimationInfo* animInfo = &sMMAnimationInfo[animIndex];
    f32 endFrame;

    if (animInfo->frameCount < 0) {
        endFrame = Animation_GetLastFrame(animInfo->animation);
    } else {
        endFrame = animInfo->frameCount;
    }
    Animation_Change(skelAnime, animInfo->animation, animInfo->playSpeed, animInfo->startFrame, endFrame,
                     animInfo->mode, animInfo->morphFrames);
}

// MM EnDg_PlaySfxWalk / Bark / AngryBark / Whine / Growl.
static void EnDog_MMPlaySfxWalk(EnDog* this) {
    if (Animation_OnFrame(&this->skelAnime, 1.0f) || Animation_OnFrame(&this->skelAnime, 7.0f)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_MONKEY_WALK);
    }
}

static void EnDog_MMPlaySfxBark(EnDog* this, f32 frame) {
    if (Animation_OnFrame(&this->skelAnime, frame)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_BARK);
    }
}

static void EnDog_MMPlaySfxAngryBark(EnDog* this, f32 frame) {
    if (Animation_OnFrame(&this->skelAnime, frame)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_ANG_BARK);
    }
}

static void EnDog_MMPlaySfxWhine(EnDog* this) {
    if (Animation_OnFrame(&this->skelAnime, 23.0f) || Animation_OnFrame(&this->skelAnime, 28.0f)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_WHINE);
    }
}

static void EnDog_MMPlaySfxGrowl(EnDog* this, f32 frame) {
    if (Animation_OnFrame(&this->skelAnime, frame)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_GROAN);
    }
}

// MM EnDg_SpawnFloorDustRing. MM staggers its dogs by their index; this dog's is its params index.
static void EnDog_MMSpawnFloorDustRing(EnDog* this, PlayState* play) {
    s16 curFrame = this->skelAnime.curFrame;
    s16 mod = (this->actor.speedXZ > 6.0f) ? 2 : 3;
    Vec3f pos;

    if (((((this->actor.params & 0x0F00) >> 8) + curFrame) % mod) == 0) {
        pos.x = Rand_CenteredFloat(15.0f) + this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y;
        pos.z = Rand_CenteredFloat(15.0f) + this->actor.world.pos.z;
        Actor_SpawnFloorDustRing(play, &this->actor, &pos, 10.0f, 0, 2.0f, 300, 0, true);
    }
}

// A form takes the dog from what it was doing.
static void EnDog_MMSetAction(EnDog* this, EnDogActionFunc actionFunc) {
    if (this->mmPrevActionFunc == NULL) {
        this->mmPrevActionFunc = this->actionFunc;
    }
    this->actionFunc = actionFunc;
}

// MM EnDg_SetupIdleMove: the dog goes back to its idle -- here Ocarina of Time's, roaming its path or sitting,
// whichever it was doing when a form took it (as the Bremen Mask's follower does, EnDog_BremenReturnToIdle).
static void EnDog_MMSetupIdleMove(EnDog* this, PlayState* play) {
    if ((this->mmPrevActionFunc == NULL) || (this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
        return;
    }
    this->actor.gravity = -1.0f; // EnDog_Init's
    this->mmDogFlags = 0;
    // Touching Link during the reaction must not count as the lost dog quest's touch afterwards.
    this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;
    this->behavior = -1; // EnDog_PlayAnimAndSFX puts Ocarina of Time's animation back
    if ((this->mmPrevActionFunc == EnDog_ChooseMovement) || (this->mmPrevActionFunc == EnDog_FollowPath)) {
        this->behaviorTimer = 0; // choose the next leg of the path at once, as MM's dog resumes its path
        this->nextBehavior = DOG_WALK;
        this->actionFunc = EnDog_ChooseMovement;
    } else {
        this->nextBehavior = DOG_SIT;
        this->actor.speedXZ = 0.0f;
        this->actionFunc = EnDog_Wait;
    }
    this->mmPrevActionFunc = NULL;
}

static s32 EnDog_MMIsRichard(EnDog* this, PlayState* play) {
    // Richard is the lost dog of the night market: EnDog_Init's own "not lost" test.
    return (play->sceneNum == SCENE_MARKET_NIGHT) && (((this->actor.params & 0x0F00) >> 8) == 1);
}

// The "Dogs React to <form>" option, for this dog.
static s32 EnDog_MMReactsToForm(EnDog* this, PlayState* play, u8 form) {
    switch (MergeShip_SohFd_DogReactionMode(form)) {
        case MERGESHIP_DOGS_ALL:
            return true;
        case MERGESHIP_DOGS_ALL_BUT_RICHARD:
            return !EnDog_MMIsRichard(this, play);
        case MERGESHIP_DOGS_ONLY_RICHARD:
            return EnDog_MMIsRichard(this, play);
        default:
            return false;
    }
}

// MM EnDg_ShouldReactToNonHumanPlayer. A dog following Link (the lost dog, or the Bremen Mask's march) keeps
// following him.
static s32 EnDog_MMShouldReactToNonHumanPlayer(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->actor.params & 0x8000) || (this->actionFunc == EnDog_FollowPlayer) || this->bremenFollowing ||
        !EnDog_MMReactsToForm(this, play, player->mergeShipForm)) {
        return false;
    }
    switch (player->mergeShipForm) {
        case PLAYER_FORM_GORON:
        case PLAYER_FORM_ZORA:
            if (this->actor.xzDistToPlayer < 300.0f) {
                return true;
            }
            // fallthrough
        case PLAYER_FORM_DEKU:
            if (this->actor.xzDistToPlayer < 250.0f) {
                return true;
            }
            break;
    }
    return false;
}

// MM EnDg_ChooseActionForForm: a dog reacts only once the player moves (MM: it would be strange for the dog to attack
// the instant the Deku Mask goes on).
static void EnDog_MMChooseActionForForm(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
        switch (player->mergeShipForm) {
            case PLAYER_FORM_ZORA:
                this->mmDogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->mmBehavior != DOG_BEHAVIOR_ZORA) && (player->actor.speedXZ > 1.0f)) {
                    this->mmBehavior = DOG_BEHAVIOR_ZORA;
                    EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN);
                    EnDog_MMSetAction(this, EnDog_MMApproachPlayer);
                }

                if ((this->mmBehavior != DOG_BEHAVIOR_ZORA_WAIT) && (this->mmBehavior != DOG_BEHAVIOR_ZORA)) {
                    this->mmBehavior = DOG_BEHAVIOR_ZORA_WAIT;
                    EnDog_MMSetupIdleMove(this, play);
                }
                break;

            case PLAYER_FORM_GORON:
                this->mmDogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->mmBehavior != DOG_BEHAVIOR_GORON) && (player->actor.speedXZ > 1.0f)) {
                    this->mmBehavior = DOG_BEHAVIOR_GORON;
                    EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_WALK_BACKWARDS);
                    this->mmTimer = 50;
                    EnDog_MMSetAction(this, EnDog_MMBackAwayFromGoron);
                }

                if ((this->mmBehavior != DOG_BEHAVIOR_GORON_WAIT) && (this->mmBehavior != DOG_BEHAVIOR_GORON)) {
                    this->mmBehavior = DOG_BEHAVIOR_GORON_WAIT;
                    EnDog_MMSetupIdleMove(this, play);
                }
                break;

            case PLAYER_FORM_DEKU:
                this->mmDogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
                if ((this->mmBehavior != DOG_BEHAVIOR_DEKU) && (player->actor.speedXZ > 1.0f)) {
                    this->mmBehavior = DOG_BEHAVIOR_DEKU;
                    EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN);
                    EnDog_MMSetAction(this, EnDog_MMApproachPlayerToAttack);
                }

                if ((this->mmBehavior != DOG_BEHAVIOR_DEKU_WAIT) && (this->mmBehavior != DOG_BEHAVIOR_DEKU)) {
                    this->mmBehavior = DOG_BEHAVIOR_DEKU_WAIT;
                    EnDog_MMSetupIdleMove(this, play);
                }
                break;

            default:
                break;
        }
    }
}

// MM EnDg_Update's first step.
static void EnDog_MMUpdateFormReaction(EnDog* this, PlayState* play) {
    if (EnDog_MMShouldReactToNonHumanPlayer(this, play)) {
        EnDog_MMChooseActionForForm(this, play);
    } else if (this->mmBehavior != DOG_BEHAVIOR_DEFAULT) {
        this->mmBehavior = DOG_BEHAVIOR_DEFAULT;
        EnDog_MMSetupIdleMove(this, play);
    }
}

// MM EnDg_UpdateCollision.
static void EnDog_MMUpdateCollision(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Collider_UpdateCylinder(&this->actor, &this->collider);

    if ((player->mergeShipForm == PLAYER_FORM_DEKU) && (this->actionFunc == EnDog_MMJumpAttack)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    } else {
        Collider_ResetCylinderAT(play, &this->collider.base);
    }

    // The dog passes through the player if it hits them with its jump attack.
    if (!(this->mmDogFlags & DOG_FLAG_JUMP_ATTACKING)) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    } else {
        Collider_ResetCylinderOC(play, &this->collider.base);
    }

    Actor_UpdateBgCheckInfo(play, &this->actor, 26.0f, 10.0f, 0.0f, 5);
}

// MM EnDg_BackAwayFromGoron: slowly back away from the player while growling, then run.
void EnDog_MMBackAwayFromGoron(EnDog* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;

    if (DECR(this->mmTimer) == 0) {
        this->mmTimer = 50;
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN);
        this->actionFunc = EnDog_MMRunAwayFromGoron;
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x3E8, 1);
        if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            this->actor.shape.rot.y = this->actor.wallYaw;
        }

        this->actor.world.rot.y = this->actor.shape.rot.y;
        Math_ApproachF(&this->actor.speedXZ, -1.5f, 0.2f, 1.0f);
        Actor_MoveXZGravity(&this->actor);
    }

    EnDog_MMPlaySfxWalk(this);
    EnDog_MMPlaySfxGrowl(this, 0);
}

// MM EnDg_RunAwayFromGoron: run from the player, then bark at them from far enough away.
void EnDog_MMRunAwayFromGoron(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yRotation;

    if (this->actor.xzDistToPlayer < 250.0f) {
        Math_ApproachS(&this->actor.shape.rot.y, -this->actor.yawTowardsPlayer, 4, 0xC00);

        if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            yRotation = this->actor.wallYaw;
        } else {
            yRotation = 0;
        }

        Math_SmoothStepToS(&this->actor.world.rot.y, yRotation, 4, 0x3E8, 1);
        this->actor.world.rot.y = this->actor.shape.rot.y;
        if (player->actor.speedXZ != 0.0f) {
            Math_ApproachF(&this->actor.speedXZ, player->actor.speedXZ, 0.2f, 1.0f);
        } else {
            Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.2f, 1.0f);
        }
    } else {
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_BARK);
        this->actionFunc = EnDog_MMBarkAtGoron;
    }

    if (this->actor.speedXZ > 7.0f) {
        this->actor.speedXZ = 7.0f;
    }

    Actor_MoveXZGravity(&this->actor);
    EnDog_MMPlaySfxWalk(this);

    if (Animation_OnFrame(&this->skelAnime, 3.0f)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_SMALL_DOG_CRY);
    } else if (Animation_OnFrame(&this->skelAnime, 6.0f)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_MONKEY_WALK);
    }
}

// MM EnDg_BarkAtGoron: bark at the player until they come close, then back away again.
void EnDog_MMBarkAtGoron(EnDog* this, PlayState* play) {
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 250.0f) {
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_WALK_BACKWARDS);
        this->mmTimer = 50;
        this->actionFunc = EnDog_MMBackAwayFromGoron;
    }

    EnDog_MMPlaySfxAngryBark(this, 13.0f);
    EnDog_MMPlaySfxAngryBark(this, 19.0f);
}

// MM EnDg_ApproachPlayerToAttack.
void EnDog_MMApproachPlayerToAttack(EnDog* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.shape.rot.y = this->actor.wallYaw;
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 70.0f) {
        Math_ApproachZeroF(&this->actor.speedXZ, 0.2f, 1.0f);
        if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
            s16 yawDiff = ABS(player->actor.shape.rot.y - this->actor.shape.rot.y);

            this->mmAttackTimer = 20;
            if (yawDiff < 0x4000) {
                EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_JUMP_ATTACK);
                this->actionFunc = EnDog_MMJumpAttack;
            } else {
                EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_WALK_BACKWARDS);
                sMMAnimationInfo[DOG_MM_ANIM_WALK_BACKWARDS].playSpeed = -1.0f;
                this->actionFunc = EnDog_MMSlowlyBackUpBeforeAttacking;
            }
        }
    } else {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
        Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.2f, 1.0f);
    }

    Actor_MoveXZGravity(&this->actor);
    EnDog_MMPlaySfxWalk(this);
    EnDog_MMPlaySfxAngryBark(this, 5.0f);
}

// MM EnDg_RunAfterAttacking: run around for a bit after a lunge, then approach the player again.
void EnDog_MMRunAfterAttacking(EnDog* this, PlayState* play) {
    this->mmDogFlags &= ~DOG_FLAG_BOUNCED;
    Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.1f, 0.5f);
    Actor_MoveXZGravity(&this->actor);
    if (DECR(this->mmAttackTimer) == 0) {
        this->mmAttackTimer = 20;
        this->actionFunc = EnDog_MMApproachPlayerToAttack;
    }

    EnDog_MMPlaySfxWalk(this);
    EnDog_MMPlaySfxAngryBark(this, 3.0f);
}

// MM EnDg_SitNextToPlayer: sit by the player, whining, until they move off; then approach them again.
void EnDog_MMSitNextToPlayer(EnDog* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->actionFunc = EnDog_MMFall;
    }

    if (this->actor.xzDistToPlayer < 50.0f) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else {
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN); // (MM: the walk while the Bremen Mask marches)
        this->actionFunc = EnDog_MMApproachPlayer;
    }

    EnDog_MMPlaySfxWhine(this);
}

// MM EnDg_JumpAttack.
void EnDog_MMJumpAttack(EnDog* this, PlayState* play) {
    s16 curFrame = this->skelAnime.curFrame;

    if ((this->actor.xyzDistToPlayerSq < 800.0f) && (this->collider.base.atFlags & AT_BOUNCED)) {
        this->mmAttackTimer = 60;
        this->mmDogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
        this->mmDogFlags |= DOG_FLAG_BOUNCED;
        this->collider.base.atFlags &= ~AT_BOUNCED;
        this->actor.speedXZ *= -1.0f;
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN);
        this->actionFunc = EnDog_MMRunAfterAttacking;
        return;
    }

    if (DECR(this->mmAttackTimer) == 0) {
        this->mmAttackTimer = 60;
        this->mmDogFlags &= ~DOG_FLAG_JUMP_ATTACKING;
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_RUN);
        this->actionFunc = EnDog_MMRunAfterAttacking;
    }

    if (curFrame < 9) {
        if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
            sMMAnimationInfo[DOG_MM_ANIM_JUMP_ATTACK].playSpeed = Rand_CenteredFloat(1.0f) + 3.0f;
        }

        EnDog_MMSpawnFloorDustRing(this, play);
    } else {
        this->mmDogFlags |= DOG_FLAG_JUMP_ATTACKING;
        if (Animation_OnFrame(&this->skelAnime, 9.0f)) {
            f32 rand = Rand_CenteredFloat(1.5f);

            sMMAnimationInfo[DOG_MM_ANIM_JUMP_ATTACK].playSpeed = 1.2f;
            this->actor.velocity.y = 2.0f * rand + 3.0f;
            this->actor.speedXZ = 8.0f + rand;
        } else if (curFrame > 20) {
            Math_ApproachF(&this->actor.speedXZ, 2.5f, 0.2f, 1.0f);
        }

        if (curFrame > 23) {
            EnDog_MMSpawnFloorDustRing(this, play);
        }

        Actor_MoveXZGravity(&this->actor);
    }

    EnDog_MMPlaySfxGrowl(this, 10.0f);
}

// MM EnDg_Fall: once it lands, the dog goes back to its idle (MM EnDg_IdleMove).
void EnDog_MMFall(EnDog* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        EnDog_MMSetupIdleMove(this, play);
        return;
    }

    Actor_MoveXZGravity(&this->actor);
    EnDog_MMPlaySfxBark(this, 3.0f);
}

// MM EnDg_ApproachPlayer: approach the player, barking; sit by them once close.
void EnDog_MMApproachPlayer(EnDog* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if ((this->actor.xzDistToPlayer < 60.0f) && (this->collider.base.ocFlags1 & OC1_HIT)) {
        this->actor.shape.rot.y += 0x71C;
    } else {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 40.0f) {
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_SIT_DOWN);
        this->actionFunc = EnDog_MMSitNextToPlayer;
    } else {
        // (MM: the player's speed while the Bremen Mask marches.)
        Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.2f, 1.0f);
    }

    Actor_MoveXZGravity(&this->actor);
    EnDog_MMPlaySfxWalk(this);
    EnDog_MMPlaySfxBark(this, 3.0f);
}

// MM EnDg_SlowlyBackUpBeforeAttacking: back up and face the player before lunging.
void EnDog_MMSlowlyBackUpBeforeAttacking(EnDog* this, PlayState* play) {
    if (this->actor.xzDistToPlayer > 72.0f) {
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_JUMP_ATTACK);
        this->actionFunc = EnDog_MMJumpAttack;
    }

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x3E8, 1);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.shape.rot.y = this->actor.wallYaw;
        EnDog_MMChangeAnim(&this->skelAnime, DOG_MM_ANIM_JUMP_ATTACK);
        this->actionFunc = EnDog_MMJumpAttack;
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_ApproachF(&this->actor.speedXZ, -1.0f, 0.2f, 1.0f);
    Actor_MoveXZGravity(&this->actor);
    EnDog_MMPlaySfxWalk(this);
    EnDog_MMPlaySfxGrowl(this, 4.0f);
}

void EnDog_Reset(void) {
    sBremenMaskFollower = NULL; // MergeShip2: MM EnDg_Reset
}

void EnDog_Update(Actor* thisx, PlayState* play) {
    EnDog* this = (EnDog*)thisx;
    s32 pad;

    // MergeShip2 (soh_fd): MM EnDg_Update -- the dog reacts to the Deku, Goron or Zora first. While a form has the
    // dog, it updates in MM's order: the action (which moves it), its collision, then its skeleton.
    EnDog_MMUpdateFormReaction(this, play);
    if (this->mmPrevActionFunc != NULL) {
        this->actionFunc(this, play);
        EnDog_MMUpdateCollision(this, play);
        SkelAnime_Update(&this->skelAnime);
        return;
    }

    EnDog_PlayAnimAndSFX(this);
    SkelAnime_Update(&this->skelAnime);
    Actor_UpdateBgCheckInfo(play, &this->actor, this->collider.dim.radius, this->collider.dim.height * 0.5f, 0.0f, 5);
    Actor_MoveXZGravity(&this->actor);
    // MergeShip2: MM EnDg EnDg_CheckForBremenMaskMarch (MM calls it from its idle and follow actions). Not for a dog
    // following Link for the lost dog quest, which keeps its quest state.
    if (this->bremenFollowing || (this->actionFunc != EnDog_FollowPlayer)) {
        EnDog_CheckForBremenMaskMarch(this, play);
    }
    this->actionFunc(this, play);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

s32 EnDog_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    return false;
}

void EnDog_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
}

void EnDog_Draw(Actor* thisx, PlayState* play) {
    EnDog* this = (EnDog*)thisx;
    Color_RGB8 colors[] = { { 255, 255, 200 }, { 150, 100, 50 } };

    if (CVarGetInteger(CVAR_COSMETIC("NPC.Dog1.Changed"), 0)) {
        colors[0] = CVarGetColor24(CVAR_COSMETIC("NPC.Dog1.Value"), colors[0]);
    }
    if (CVarGetInteger(CVAR_COSMETIC("NPC.Dog2.Changed"), 0)) {
        colors[1] = CVarGetColor24(CVAR_COSMETIC("NPC.Dog2.Value"), colors[1]);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, colors[this->actor.params & 0xF].r, colors[this->actor.params & 0xF].g,
                   colors[this->actor.params & 0xF].b, 0);

    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, EnDog_OverrideLimbDraw, EnDog_PostLimbDraw, this);

    CLOSE_DISPS(play->state.gfxCtx);
}
