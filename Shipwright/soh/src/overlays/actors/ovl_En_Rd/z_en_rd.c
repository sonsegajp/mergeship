#include "z_en_rd.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMaskReactions.h" // MergeShip2: MM Gibdo Mask / Captain's Hat / Garo's Mask
#include "soh/MergeShip/MergeShipAssets.h"
#include "objects/object_rd/object_rd.h"
#include "mm/objects/object_rd/object_rd.h" // MergeShip2: MM's ReDead dances
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER)

void EnRd_Init(Actor* thisx, PlayState* play);
void EnRd_Destroy(Actor* thisx, PlayState* play);
void EnRd_Update(Actor* thisx, PlayState* play);
void EnRd_Draw(Actor* thisx, PlayState* play);

void EnRd_SetupIdle(EnRd* this);
void EnRd_Idle(EnRd* this, PlayState* play);
void EnRd_SetupRiseFromCoffin(EnRd* this);
void EnRd_RiseFromCoffin(EnRd* this, PlayState* play);
void EnRd_WalkToPlayer(EnRd* this, PlayState* play);
void EnRd_SetupWalkToHome(EnRd* this, PlayState* play);
void EnRd_WalkToHome(EnRd* this, PlayState* play);
void EnRd_SetupWalkToParent(EnRd* this);
void EnRd_WalkToParent(EnRd* this, PlayState* play);
void EnRd_SetupGrab(EnRd* this);
void EnRd_SetupStandUp(EnRd* this);
void EnRd_SetupCrouch(EnRd* this);
void EnRd_Grab(EnRd* this, PlayState* play);
void EnRd_SetupAttemptPlayerFreeze(EnRd* this);
void EnRd_AttemptPlayerFreeze(EnRd* this, PlayState* play);
void EnRd_StandUp(EnRd* this, PlayState* play);
void EnRd_Crouch(EnRd* this, PlayState* play);
void EnRd_Damaged(EnRd* this, PlayState* play);
void EnRd_Dead(EnRd* this, PlayState* play);
void EnRd_Stunned(EnRd* this, PlayState* play);
// MergeShip2: MM En_Rd -- a ReDead can't grab the Goron or the Deku; it flinches, turns away shaking its head.
void EnRd_SetupGrabFail(EnRd* this);
void EnRd_GrabFail(EnRd* this, PlayState* play);
void EnRd_SetupTurnAwayAndShakeHead(EnRd* this);
void EnRd_TurnAwayAndShakeHead(EnRd* this, PlayState* play);

// MM: EN_RD_ACTION_FAILING_GRAB / EN_RD_ACTION_TURNING_AWAY_AND_SHAKING_HEAD (Ocarina of Time uses 0-11).
#define EN_RD_ACTION_MS_FAILING_GRAB 12
#define EN_RD_ACTION_MS_SHAKING_HEAD 13

static s32 EnRd_PlayerIsUngrabbableForm(Player* player) {
    return (player->mergeShipForm == PLAYER_FORM_GORON) || (player->mergeShipForm == PLAYER_FORM_DEKU);
}

// MergeShip2: MM En_Rd dancing ReDeads (MM EnRd_SetupSquattingDance .. EnRd_EndPirouetteWhenPlayerIsClose).
// Every ReDead Majora's Mask actually places is one of its dancing types (EN_RD_TYPE_SQUATTING_DANCE,
// _CLAPPING_DANCE, _PIROUETTE); OoT's ReDeads are mapped onto them, each keeping one dance, chosen from its home
// position. Gibdos (params < -1) never dance: MM's EnRd_ShouldNotDance is only consulted by the dancing types.
#define EN_RD_MS_DANCE_NONE 0
#define EN_RD_MS_DANCE_SQUATTING 1
#define EN_RD_MS_DANCE_CLAPPING 2
#define EN_RD_MS_DANCE_PIROUETTE 3
// MM: EN_RD_ACTION_SQUATTING_DANCE / _CLAPPING_DANCE / _PIROUETTE (Ocarina of Time uses 0-11, MergeShip 12-13).
#define EN_RD_ACTION_MS_SQUATTING_DANCE 14
#define EN_RD_ACTION_MS_CLAPPING_DANCE 15
#define EN_RD_ACTION_MS_PIROUETTE 16
void EnRd_SetupSquattingDance(EnRd* this);
void EnRd_SquattingDance(EnRd* this, PlayState* play);
void EnRd_SetupClappingDance(EnRd* this);
void EnRd_ClappingDance(EnRd* this, PlayState* play);
void EnRd_EndClappingOrSquattingDanceWhenPlayerIsClose(EnRd* this, PlayState* play);
void EnRd_SetupPirouette(EnRd* this);
void EnRd_Pirouette(EnRd* this, PlayState* play);
void EnRd_EndPirouetteWhenPlayerIsClose(EnRd* this, PlayState* play);
void EnRd_SetupDanceIfConditionsMet(EnRd* this, PlayState* play);

// MergeShip2: MM EnRailgibud / EnTalkGibud -- under the Gibdo Mask a Gibdo neither freezes nor grabs the wearer
// (MM EnRailgibud_PlayerInRangeWithCorrectState returns false). OoT's Gibdos are En_Rd with params < -1.
static s32 EnRd_MergeShipGibdoMaskPacifies(EnRd* this, PlayState* play) {
    return (this->actor.params < -1) && MergeShip_GibdoMaskWorn(play);
}
void EnRd_MergeShipCheckForGibdoMask(EnRd* this, PlayState* play);

const ActorInit En_Rd_InitVars = {
    ACTOR_EN_RD,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_RD,
    sizeof(EnRd),
    (ActorFunc)EnRd_Init,
    (ActorFunc)EnRd_Destroy,
    (ActorFunc)EnRd_Update,
    (ActorFunc)EnRd_Draw,
    NULL,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 20, 70, 0, { 0, 0, 0 } },
};

static DamageTable sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(2, 0xF),
    /* Slingshot     */ DMG_ENTRY(0, 0x0),
    /* Explosive     */ DMG_ENTRY(0, 0x0),
    /* Boomerang     */ DMG_ENTRY(0, 0x0),
    /* Normal arrow  */ DMG_ENTRY(0, 0x0),
    /* Hammer swing  */ DMG_ENTRY(2, 0xF),
    /* Hookshot      */ DMG_ENTRY(0, 0x1),
    /* Kokiri sword  */ DMG_ENTRY(1, 0xF),
    /* Master sword  */ DMG_ENTRY(2, 0xF),
    /* Giant's Knife */ DMG_ENTRY(4, 0xF),
    /* Fire arrow    */ DMG_ENTRY(0, 0x0),
    /* Ice arrow     */ DMG_ENTRY(0, 0x0),
    /* Light arrow   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 1   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(0, 0x0),
    /* Fire magic    */ DMG_ENTRY(4, 0xE),
    /* Ice magic     */ DMG_ENTRY(0, 0x6),
    /* Light magic   */ DMG_ENTRY(3, 0xD),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(1, 0xF),
    /* Giant spin    */ DMG_ENTRY(4, 0xF),
    /* Master spin   */ DMG_ENTRY(2, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(2, 0xF),
    /* Giant jump    */ DMG_ENTRY(8, 0xF),
    /* Master jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(targetArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -3500, ICHAIN_STOP),
};

static Vec3f D_80AE4918 = { 0.0f, 0.0f, 0.0f };

// I'm guessing these are primitive and environment colors that go unused
static Color_RGBA8 D_80AE4924 = { 200, 200, 255, 255 };
static Color_RGBA8 D_80AE4928 = { 0, 0, 255, 0 };

static Vec3f D_80AE492C = { 0.0f, 0.0f, 0.0f };
static Color_RGBA8 D_80AE4938 = { 200, 200, 255, 255 };
static Color_RGBA8 D_80AE493C = { 0, 0, 255, 0 };

static Vec3f D_80AE4940 = { 300.0f, 0.0f, 0.0f };
static Vec3f D_80AE494C = { 300.0f, 0.0f, 0.0f };
static Vec3f D_80AE4958 = { 0.25f, 0.25f, 0.25f };

void EnRd_SetupAction(EnRd* this, EnRdActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnRd_Init(Actor* thisx, PlayState* play) {
    EnRd* this = (EnRd*)thisx;

    Actor_ProcessInitChain(thisx, sInitChain);
    thisx->targetMode = 0;
    thisx->colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    this->upperBodyYRotation = this->headYRotation = 0;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 50.0f;
    thisx->colChkInfo.mass = MASS_HEAVY;
    thisx->colChkInfo.health = 8;
    this->alpha = this->unk_31D = 0xFF;
    this->rdFlags = (thisx->params & 0xFF00) >> 8;

    if (thisx->params & 0x80) {
        thisx->params |= 0xFF00;
    } else {
        thisx->params &= 0xFF;
    }

    if (thisx->params >= -1) {
        SkelAnime_InitFlex(play, &this->skelAnime, &gRedeadSkel, &gGibdoRedeadIdleAnim, this->jointTable,
                           this->morphTable, 26);
        thisx->naviEnemyId = 0x2A;
        // MergeShip2: MM EnRd_Init -- the dance of MM's EN_RD_TYPE_SQUATTING_DANCE / _CLAPPING_DANCE / _PIROUETTE
        // (its setupDanceFunc), fixed per ReDead by where it stands.
        this->mergeShipDance =
            EN_RD_MS_DANCE_SQUATTING + (((u32)((s32)thisx->home.pos.x * 7 + (s32)thisx->home.pos.z * 3)) % 3);
    } else {
        SkelAnime_InitFlex(play, &this->skelAnime, &gGibdoSkel, &gGibdoRedeadIdleAnim, this->jointTable,
                           this->morphTable, 26);
        thisx->naviEnemyId = 0x2D;
    }

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);

    if (thisx->params >= -2) {
        EnRd_SetupIdle(this);
    } else {
        EnRd_SetupRiseFromCoffin(this);
    }

    SkelAnime_Update(&this->skelAnime);

    if (thisx->params == 3) {
        thisx->flags |= ACTOR_FLAG_REACT_TO_LENS;
    }
}

void EnRd_Destroy(Actor* thisx, PlayState* play) {
    EnRd* this = (EnRd*)thisx;

    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE) {
        gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
    }
    Collider_DestroyCylinder(play, &this->collider);
}

void EnRd_UpdateMourningTarget(PlayState* play, Actor* thisx, s32 arg2) {
    Actor* enemyIt = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;

    while (enemyIt != NULL) {
        if ((enemyIt->id != ACTOR_EN_RD) || (enemyIt == thisx) || (enemyIt->params < 0)) {
            enemyIt = enemyIt->next;
            continue;
        }

        if (arg2 != 0) {
            enemyIt->parent = thisx;
        } else if (enemyIt->parent == thisx) {
            enemyIt->parent = NULL;
        }
        enemyIt = enemyIt->next;
    }
}

void EnRd_SetupIdle(EnRd* this) {
    if (this->actor.params != 2) {
        Animation_MorphToLoop(&this->skelAnime, &gGibdoRedeadIdleAnim, -6.0f);
    } else {
        Animation_PlayLoop(&this->skelAnime, &gGibdoRedeadSobbingAnim);
    }

    this->action = 0;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnRd_SetupAction(this, EnRd_Idle);
}

void EnRd_Idle(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);

    if ((this->actor.params == 2) && (0.0f == this->skelAnime.curFrame)) {
        if (Rand_ZeroOne() >= 0.5f) {
            Animation_PlayLoop(&this->skelAnime, &gGibdoRedeadSobbingAnim);
        } else {
            Animation_PlayLoop(&this->skelAnime, &gGibdoRedeadWipingTearsAnim);
        }
    } else {
        this->timer--;
        if (this->timer == 0) {
            this->timer = (Rand_ZeroOne() * 10.0f) + 10.0f;
            this->skelAnime.curFrame = 0.0f;
        }
    }

    if (this->actor.parent != NULL) {
        if (this->isMourning == 0) {
            if (this->actor.params != 2) {
                EnRd_SetupWalkToParent(this);
            } else {
                EnRd_SetupStandUp(this);
            }
        }
    } else {
        if (this->isMourning != 0) {
            if (this->actor.params != 2) {
                EnRd_SetupAttemptPlayerFreeze(this);
            } else {
                EnRd_SetupStandUp(this);
            }
        }

        this->isMourning = 0;

        if (this->actor.xzDistToPlayer <= 150.0f && func_8002DDE4(play) &&
            // MergeShip2: MM EnRailgibud_WalkInCircles -- `Player_GetMask(play) != PLAYER_MASK_GIBDO`.
            !EnRd_MergeShipGibdoMaskPacifies(this, play)) {
            // Add a height check to redeads/gibdos freeze when Enemy Randomizer is on.
            // Without the height check, redeads/gibdos can freeze the player from insane distances in
            // vertical rooms (like the first room in Deku Tree), making these rooms nearly unplayable.
            s8 enemyRandoCCActive = CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0) ||
                                    (CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 0));
            if (!enemyRandoCCActive ||
                (enemyRandoCCActive && this->actor.yDistToPlayer <= 100.0f && this->actor.yDistToPlayer >= -100.0f)) {
                if ((this->actor.params != 2) && (this->isMourning == 0)) {
                    EnRd_SetupAttemptPlayerFreeze(this);
                } else {
                    EnRd_SetupStandUp(this);
                }
            }
        }
    }

    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

void EnRd_SetupRiseFromCoffin(EnRd* this) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadIdleAnim, 0, 0, Animation_GetLastFrame(&gGibdoRedeadIdleAnim),
                     ANIMMODE_LOOP, -6.0f);
    this->action = 11;
    this->timer = 6;
    this->actor.shape.rot.x = -0x4000;
    this->actor.gravity = 0.0f;
    this->actor.shape.yOffset = 0.0f;
    this->actor.speedXZ = 0.0f;
    EnRd_SetupAction(this, EnRd_RiseFromCoffin);
}

// Rising out of coffin
void EnRd_RiseFromCoffin(EnRd* this, PlayState* play) {
    if (this->actor.shape.rot.x != -0x4000) {
        Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 1, 0x7D0, 0);
        if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.3f, 2.0f, 0.3f) == 0.0f) {
            this->actor.gravity = -3.5f;
            EnRd_SetupIdle(this);
        }
    } else {
        if (this->actor.world.pos.y == this->actor.home.pos.y) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
        }
        if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 50.0f, 0.3f, 2.0f, 0.3f) == 0.0f) {
            if (this->timer != 0) {
                this->timer--;
                Math_SmoothStepToF(&this->actor.speedXZ, 6.0f, 0.3f, 1.0f, 0.3f);
            } else if (Math_SmoothStepToF(&this->actor.speedXZ, 0.0f, 0.3f, 1.0f, 0.3f) == 0.0f) {
                Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 1, 0x7D0, 0);
            }
        }
    }
}

void EnRd_SetupWalkToPlayer(EnRd* this, PlayState* play) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadWalkAnim, 1.0f, 4.0f, Animation_GetLastFrame(&gGibdoRedeadWalkAnim),
                     ANIMMODE_LOOP_INTERP, -4.0f);
    this->actor.speedXZ = 0.4f;
    this->action = 4;
    EnRd_SetupAction(this, EnRd_WalkToPlayer);
}

void EnRd_WalkToPlayer(EnRd* this, PlayState* play) {
    Vec3f sp44 = D_80AE4918;
    Color_RGBA8 sp40 = D_80AE4924;
    Color_RGBA8 sp3C = D_80AE4928;
    Player* player = GET_PLAYER(play);
    s32 pad;
    s16 sp32 = this->actor.yawTowardsPlayer - this->actor.shape.rot.y - this->headYRotation - this->upperBodyYRotation;

    this->skelAnime.playSpeed = this->actor.speedXZ;
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA, 0);
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);

    if (Actor_WorldDistXYZToPoint(&player->actor, &this->actor.home.pos) >= 150.0f) {
        EnRd_SetupWalkToHome(this, play);
    }

    if (!EnRd_MergeShipGibdoMaskPacifies(this, play) && // MergeShip2: MM EnRailgibud_WalkToPlayer
        (ABS(sp32) < 0x1554) && (Actor_WorldDistXYZToActor(&this->actor, &player->actor) <= 150.0f)) {
        if (!(player->stateFlags1 &
              (PLAYER_STATE1_DEAD | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
               PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER)) &&
            !(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY)) {
            if (this->playerStunWaitTimer == 0) {
                if (!(this->rdFlags & PLAYER_STATE2_GRABBED_BY_ENEMY) &&
                    GameInteractor_Should(VB_REDEAD_GIBDO_FREEZE_LINK, true, this)) {
                    player->actor.freezeTimer = 40;
                    Player_SetAutoLockOnActor(play, &this->actor);
                    GET_PLAYER(play)->autoLockOnActor = &this->actor;
                    Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 0x14, 0x96);
                }
                this->playerStunWaitTimer = 0x3C;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_AIM);
            }
        } else {
            EnRd_SetupWalkToHome(this, play);
        }
    }

    if (this->grabWaitTimer != 0) {
        this->grabWaitTimer--;
    }

    if (EnRd_MergeShipGibdoMaskPacifies(this, play)) {
        // MergeShip2: MM EnRailgibud_WalkToPlayer -- with EnRailgibud_PlayerInRangeWithCorrectState false under the
        // Gibdo Mask there is no freeze and no grab; the Gibdo turns back home once it reaches the wearer.
        if ((this->grabWaitTimer == 0) && (this->actor.xzDistToPlayer <= 45.0f)) {
            EnRd_SetupWalkToHome(this, play);
        }
    } else if (!this->grabWaitTimer && (Actor_WorldDistXYZToActor(&this->actor, &player->actor) <= 45.0f) &&
               Actor_IsFacingPlayer(&this->actor, 0x38E3)) {
        player->actor.freezeTimer = 0;
        if (EnRd_PlayerIsUngrabbableForm(player)) {
            // MM EnRd_WalkToPlayer: the grab fails on Goron or Deku Link; the ReDead appears to take damage and
            // shakes its head.
            if (Actor_WorldDistXYZToPoint(&this->actor, &this->actor.home.pos) < 150.0f) {
                EnRd_SetupGrabFail(this);
            } else {
                EnRd_SetupWalkToHome(this, play);
            }
        } else if (play->grabPlayer(play, player)) {
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            EnRd_SetupGrab(this);
        }
    } else if (this->actor.params > 0) {
        if (this->actor.parent != NULL) {
            EnRd_SetupWalkToParent(this);
        } else {
            this->isMourning = 0;
        }
    }

    if ((this->skelAnime.curFrame == 10.0f) || (this->skelAnime.curFrame == 22.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_RIZA_WALK);
    } else if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

void EnRd_SetupWalkToHome(EnRd* this, PlayState* play) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadWalkAnim, 0.5f, 0, Animation_GetLastFrame(&gGibdoRedeadWalkAnim),
                     ANIMMODE_LOOP_INTERP, -4.0f);
    this->action = 2;
    EnRd_SetupAction(this, EnRd_WalkToHome);
}

void EnRd_WalkToHome(EnRd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 pad;
    s16 targetY = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);

    if (Actor_WorldDistXYZToPoint(&this->actor, &this->actor.home.pos) >= 5.0f) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, targetY, 1, 0x1C2, 0);
    } else {
        this->actor.speedXZ = 0.0f;
        if (Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.home.rot.y, 1, 0x1C2, 0) == 0) {
            if (this->actor.params != 2) {
                EnRd_SetupIdle(this);
            } else {
                EnRd_SetupCrouch(this);
            }
        }
    }

    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);

    if (!(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                                 PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER)) &&
        !(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) &&
        !EnRd_PlayerIsUngrabbableForm(player) && // MergeShip2: MM EnRd_WalkToHome
        !EnRd_MergeShipGibdoMaskPacifies(this, play) && // MergeShip2: MM EnRailgibud_WalkToHome
        (Actor_WorldDistXYZToPoint(&player->actor, &this->actor.home.pos) < 150.0f)) {
        this->actor.targetMode = 0;
        EnRd_SetupWalkToPlayer(this, play);
    } else if (this->actor.params > 0) {
        if (this->actor.parent != NULL) {
            EnRd_SetupWalkToParent(this);
        } else {
            this->isMourning = 0;
        }
    }

    if (this->skelAnime.curFrame == 10.0f || this->skelAnime.curFrame == 22.0f) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_RIZA_WALK);
    } else if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

// MM EnRd_SetupGrabFail / EnRd_GrabFail: it recoils as if hit, then turns away.
void EnRd_SetupGrabFail(EnRd* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, &gGibdoRedeadDamageAnim, -6.0f);
    this->actor.speedXZ = -2.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_DAMAGE);
    this->action = EN_RD_ACTION_MS_FAILING_GRAB;
    EnRd_SetupAction(this, EnRd_GrabFail);
}

void EnRd_GrabFail(EnRd* this, PlayState* play) {
    if (this->actor.speedXZ < 0.0f) {
        this->actor.speedXZ += 0.15f;
    }

    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x12C, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x12C, 0);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.world.rot.y = this->actor.shape.rot.y;
        EnRd_SetupTurnAwayAndShakeHead(this);
    }
}

// MM EnRd_SetupTurnAwayAndShakeHead / EnRd_TurnAwayAndShakeHead (MM's headShakeTimer is `timer` here).
void EnRd_SetupTurnAwayAndShakeHead(EnRd* this) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadWalkAnim, 0.5f, 0.0f, Animation_GetLastFrame(&gGibdoRedeadWalkAnim),
                     ANIMMODE_LOOP_INTERP, -4.0f);
    this->action = EN_RD_ACTION_MS_SHAKING_HEAD;
    this->timer = 0;
    EnRd_SetupAction(this, EnRd_TurnAwayAndShakeHead);
}

void EnRd_TurnAwayAndShakeHead(EnRd* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.y, (s16)(this->actor.yawTowardsPlayer + 0x8000), 5, 0xDAC, 0xC8);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    if (this->timer > 60) {
        EnRd_SetupWalkToHome(this, play);
        this->timer = 0;
    } else {
        this->headYRotation = Math_SinS(this->timer * 4000) * (0x256F * ((60 - this->timer) / 60.0f));
        SkelAnime_Update(&this->skelAnime);
        this->timer++;
    }
}

// MergeShip2: MM EnRd_SetupDanceIfConditionsMet -- a dancing ReDead that is not already dancing, stunned, hurt, dead
// or holding the player starts its dance while the Gibdo Mask, Captain's Hat or Garo's Mask is worn.
void EnRd_SetupDanceIfConditionsMet(EnRd* this, PlayState* play) {
    if ((this->mergeShipDance != EN_RD_MS_DANCE_NONE) && (this->actionFunc != EnRd_SquattingDance) &&
        (this->actionFunc != EnRd_ClappingDance) && (this->actionFunc != EnRd_Pirouette) &&
        (this->actionFunc != EnRd_Stunned) && (this->actionFunc != EnRd_Grab) && (this->actionFunc != EnRd_Damaged) &&
        (this->actionFunc != EnRd_Dead)) {
        if (MergeShip_RedeadsShouldDance(play) && MergeShip_IsMajorasMaskMounted()) {
            // MM this->setupDanceFunc(this)
            switch (this->mergeShipDance) {
                case EN_RD_MS_DANCE_SQUATTING:
                    EnRd_SetupSquattingDance(this);
                    break;
                case EN_RD_MS_DANCE_CLAPPING:
                    EnRd_SetupClappingDance(this);
                    break;
                default:
                    EnRd_SetupPirouette(this);
                    break;
            }
        }
    }
}

// MergeShip2: MM EnRd_SetupSquattingDance
void EnRd_SetupSquattingDance(EnRd* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)mm_gGibdoRedeadSquattingDanceAnim, -6.0f);
    this->action = EN_RD_ACTION_MS_SQUATTING_DANCE;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f; // MM animationJudderTimer
    this->mergeShipDanceEndTimer = 0;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnRd_SetupAction(this, EnRd_SquattingDance);
}

// MergeShip2: MM EnRd_SquattingDance. MM's hintId updates are dropped: OoT's ReDeads keep their Navi entry throughout.
// As in MM, taking the mask off near the player first picks the look-back and then EnRd_SetupIdle overrides it.
void EnRd_SquattingDance(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);

    if (this->isMourning) {
        EnRd_SetupAttemptPlayerFreeze(this);
    }

    this->isMourning = false;
    if ((this->actor.xzDistToPlayer <= 150.0f) && !MergeShip_RedeadsShouldDance(play) && func_8002DDE4(play)) {
        Animation_Change(&this->skelAnime, &gGibdoRedeadLookBackAnim, 0.0f, 0.0f, 19.0f, ANIMMODE_ONCE, -10.0f);
        EnRd_SetupAction(this, EnRd_EndClappingOrSquattingDanceWhenPlayerIsClose);
    }

    if (!MergeShip_RedeadsShouldDance(play)) {
        EnRd_SetupIdle(this);
    }

    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

// MergeShip2: MM EnRd_SetupClappingDance
void EnRd_SetupClappingDance(EnRd* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)mm_gGibdoRedeadClappingDanceAnim, -6.0f);
    this->action = EN_RD_ACTION_MS_CLAPPING_DANCE;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f; // MM animationJudderTimer
    this->mergeShipDanceEndTimer = 0;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnRd_SetupAction(this, EnRd_ClappingDance);
}

// MergeShip2: MM EnRd_ClappingDance
void EnRd_ClappingDance(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);

    if (this->isMourning) {
        EnRd_SetupAttemptPlayerFreeze(this);
    }

    this->isMourning = false;
    if ((this->actor.xzDistToPlayer <= 150.0f) && !MergeShip_RedeadsShouldDance(play) && func_8002DDE4(play)) {
        Animation_Change(&this->skelAnime, &gGibdoRedeadLookBackAnim, 0.0f, 0.0f, 19.0f, ANIMMODE_ONCE, -10.0f);
        EnRd_SetupAction(this, EnRd_EndClappingOrSquattingDanceWhenPlayerIsClose);
    }

    if (!MergeShip_RedeadsShouldDance(play)) {
        EnRd_SetupIdle(this);
    }

    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

// MergeShip2: MM EnRd_EndClappingOrSquattingDanceWhenPlayerIsClose
void EnRd_EndClappingOrSquattingDanceWhenPlayerIsClose(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }

    this->mergeShipDanceEndTimer++;
    if (this->mergeShipDanceEndTimer > 10) {
        if ((this->actor.params != 2) && (!this->isMourning)) {
            EnRd_SetupAttemptPlayerFreeze(this);
        } else {
            EnRd_SetupStandUp(this);
        }
        this->mergeShipDanceEndTimer = 0;
    }
}

// MergeShip2: MM EnRd_SetupPirouette
void EnRd_SetupPirouette(EnRd* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)mm_gGibdoRedeadPirouetteAnim, -6.0f);
    this->action = EN_RD_ACTION_MS_PIROUETTE;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f; // MM animationJudderTimer
    this->mergeShipPirouetteAngularVelocity = 0x1112;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnRd_SetupAction(this, EnRd_Pirouette);
}

// MergeShip2: MM EnRd_Pirouette
void EnRd_Pirouette(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);

    if (this->isMourning) {
        EnRd_SetupAttemptPlayerFreeze(this);
    }

    this->isMourning = false;
    if ((this->actor.xzDistToPlayer <= 150.0f) && !MergeShip_RedeadsShouldDance(play) && func_8002DDE4(play)) {
        EnRd_SetupAction(this, EnRd_EndPirouetteWhenPlayerIsClose);
    }

    if (!MergeShip_RedeadsShouldDance(play)) {
        EnRd_SetupIdle(this);
    }

    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }

    if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        this->mergeShipPirouetteAngularVelocity = 0x1112;
    } else if (Animation_OnFrame(&this->skelAnime, 15.0f)) {
        this->mergeShipPirouetteAngularVelocity = 0x199A;
    }

    this->actor.world.rot.y -= this->mergeShipPirouetteAngularVelocity;
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

// MergeShip2: MM EnRd_EndPirouetteWhenPlayerIsClose
void EnRd_EndPirouetteWhenPlayerIsClose(EnRd* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }

    this->actor.world.rot.y -= this->mergeShipPirouetteAngularVelocity;
    this->actor.shape.rot.y = this->actor.world.rot.y;

    this->mergeShipPirouetteAngularVelocity -= 0x64;
    if ((this->mergeShipPirouetteAngularVelocity < 0x834) && (this->mergeShipPirouetteAngularVelocity >= 0x7D0)) {
        Animation_Change(&this->skelAnime, &gGibdoRedeadLookBackAnim, 0.0f, 0.0f, 19.0f, ANIMMODE_ONCE, -10.0f);
    } else if (this->mergeShipPirouetteAngularVelocity < 0x3E8) {
        if ((this->actor.params != 2) && (!this->isMourning)) {
            EnRd_SetupAttemptPlayerFreeze(this);
        } else {
            EnRd_SetupStandUp(this);
        }
    }
}

// MergeShip2: MM EnRailgibud_CheckIfTalkingToPlayer -- a Gibdo spoken to through the Gibdo Mask says MM's 0x13B2, then
// 0x13B3 (MERGESHIP_TEXT_GIBDO_*). OoT's player opens actor.textId on accepting, so it is set when the talk is offered.
static void EnRd_MergeShipCheckIfTalkingToPlayer(EnRd* this, PlayState* play) {
    if (this->mergeShipTextId == 0) {
        if (Actor_ProcessTalkRequest(&this->actor, play)) {
            this->mergeShipIsInvincible = true;
            this->mergeShipTextId = MERGESHIP_TEXT_GIBDO_BRING_THEM;
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_AIM);
            this->mergeShipTalkSpeed = this->actor.speedXZ;
            this->actor.speedXZ = 0.0f;
        } else if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY) &&
                   !(this->collider.base.acFlags & AC_HIT)) {
            this->actor.textId = MERGESHIP_TEXT_GIBDO_BRING_THEM;
            Actor_OfferTalk(&this->actor, play, 100.0f);
        }
    } else {
        switch (Message_GetState(&play->msgCtx)) {
            case TEXT_STATE_EVENT:
                if (Message_ShouldAdvance(play)) {
                    Message_ContinueTextbox(play, MERGESHIP_TEXT_GIBDO_OUR_FRIENDS);
                    this->mergeShipTextId = MERGESHIP_TEXT_GIBDO_OUR_FRIENDS;
                }
                break;

            case TEXT_STATE_DONE:
                if (Message_ShouldAdvance(play)) {
                    this->mergeShipTextId = 0;
                    this->mergeShipIsInvincible = false;
                    // MM resumes its patrol at 0.6f; OoT's Gibdos resume whatever they were doing.
                    this->actor.speedXZ = this->mergeShipTalkSpeed;
                }
                break;

            default:
                break;
        }
    }
}

// MergeShip2: MM EnRailgibud_CheckForGibdoMask (MM EnTalkGibud_CheckForGibdoMask is the same for a standing Gibdo):
// the Gibdo Mask turns a Gibdo friendly and sends it home, where OoT's Gibdo stands (MM's patrols); without the mask it
// is hostile again. OoT's rising-from-coffin Gibdo finishes rising first.
void EnRd_MergeShipCheckForGibdoMask(EnRd* this, PlayState* play) {
    if ((this->actor.params >= -1) || (this->actionFunc == EnRd_Grab) || (this->actionFunc == EnRd_Damaged) ||
        (this->actionFunc == EnRd_GrabFail) || (this->actionFunc == EnRd_TurnAwayAndShakeHead) ||
        (this->actionFunc == EnRd_Dead) || (this->actionFunc == EnRd_RiseFromCoffin)) {
        return;
    }

    if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)) {
        if (MergeShip_GibdoMaskWorn(play)) {
            this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
            this->actor.flags |= (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY);
            this->actor.naviEnemyId = 0xFF; // MM hintId = TATL_HINT_ID_NONE
            this->actor.textId = 0;
            // MM WalkInCircles is OoT's Idle for a standing Gibdo.
            if ((this->actionFunc != EnRd_Idle) && (this->actionFunc != EnRd_WalkToHome)) {
                EnRd_SetupWalkToHome(this, play);
                this->actor.speedXZ = 0.4f; // MM EnRailgibud_SetupWalkToHome
            }
        }
    } else if (!MergeShip_GibdoMaskWorn(play)) {
        this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY);
        this->actor.flags |= (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        this->actor.naviEnemyId = 0x2D; // MM hintId = TATL_HINT_ID_GIBDO
        this->actor.textId = 0;
    }

    EnRd_MergeShipCheckIfTalkingToPlayer(this, play);
}

void EnRd_SetupWalkToParent(EnRd* this) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadWalkAnim, 0.5f, 0, Animation_GetLastFrame(&gGibdoRedeadWalkAnim),
                     ANIMMODE_LOOP_INTERP, -4.0f);
    this->action = 3;
    this->isMourning = 1;
    EnRd_SetupAction(this, EnRd_WalkToParent);
}

void EnRd_WalkToParent(EnRd* this, PlayState* play) {
    if (this->actor.parent != NULL) {
        s32 pad;
        s16 targetY;
        Vec3f thisPos = this->actor.parent->world.pos;

        targetY = Actor_WorldYawTowardPoint(&this->actor, &thisPos);

        Math_SmoothStepToS(&this->actor.shape.rot.y, targetY, 1, 0xFA, 0);

        if (Actor_WorldDistXYZToPoint(&this->actor, &thisPos) >= 45.0f) {
            this->actor.speedXZ = 0.4f;
        } else {
            this->actor.speedXZ = 0.0f;

            if (this->actor.params != 2) {
                EnRd_SetupIdle(this);
            } else {
                EnRd_SetupCrouch(this);
            }
        }

        Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x64, 0);
        Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x64, 0);
    } else {
        EnRd_SetupWalkToPlayer(this, play);
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);

    if (this->skelAnime.curFrame == 10.0f || this->skelAnime.curFrame == 22.0f) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_RIZA_WALK);
    } else if ((play->gameplayFrames & 0x5F) == 0) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_CRY);
    }
}

void EnRd_SetupGrab(EnRd* this) {
    Animation_PlayOnce(&this->skelAnime, &gGibdoRedeadGrabStartAnim);
    this->timer = this->grabState = 0;
    this->grabDamageTimer = 200;
    this->action = 8;
    this->actor.speedXZ = 0.0f;
    EnRd_SetupAction(this, EnRd_Grab);
}

void EnRd_Grab(EnRd* this, PlayState* play) {
    s32 pad;
    Player* player = GET_PLAYER(play);

    if (SkelAnime_Update(&this->skelAnime)) {
        this->grabState++;
    }

    switch (this->grabState) {
        case 1:
            Animation_PlayLoop(&this->skelAnime, &gGibdoRedeadGrabAttackAnim);
            this->grabState++;
            play->damagePlayer(play, -8);
            Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 1, 0xC);
            this->grabDamageTimer = 20;
        case 0:
            Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x5DC, 0);
            Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x5DC, 0);
        case 2:
            if (!(player->stateFlags2 & 0x80)) {
                Animation_Change(&this->skelAnime, &gGibdoRedeadGrabEndAnim, 0.5f, 0.0f,
                                 Animation_GetLastFrame(&gGibdoRedeadGrabEndAnim), ANIMMODE_ONCE_INTERP, 0.0f);
                this->grabState++;
                this->action = 4;
                return;
            }

            // MergeShip2: MM lowers itself onto the human only; a Zora or Fierce Deity is grabbed standing.
            if (!LINK_IS_ADULT && !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
                Math_SmoothStepToF(&this->actor.shape.yOffset, -1500.0f, 1.0f, 150.0f, 0.0f);
            }

            Math_SmoothStepToF(&this->actor.world.pos.x,
                               (Math_SinS(player->actor.shape.rot.y) * -25.0f) + player->actor.world.pos.x, 1.0f, 10.0f,
                               0.0f);
            Math_SmoothStepToF(&this->actor.world.pos.y, player->actor.world.pos.y, 1.0f, 10.0f, 0.0f);
            Math_SmoothStepToF(&this->actor.world.pos.z,
                               (Math_CosS(player->actor.shape.rot.y) * -25.0f) + player->actor.world.pos.z, 1.0f, 10.0f,
                               0.0f);
            Math_SmoothStepToS(&this->actor.shape.rot.y, player->actor.shape.rot.y, 1, 0x1770, 0);

            if (this->skelAnime.curFrame == 0.0f) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_ATTACK);
            }
            this->grabDamageTimer--;

            if (this->grabDamageTimer == 0) {
                play->damagePlayer(play, -8);
                Rumble_Request(this->actor.xzDistToPlayer, 0xF0, 1, 0xC);
                this->grabDamageTimer = 20;
                Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->voiceSfxIdOffset);
            }
            break;
        case 3:
            if (!LINK_IS_ADULT) {
                Math_SmoothStepToF(&this->actor.shape.yOffset, 0, 1.0f, 400.0f, 0.0f);
            }
            break;
        case 4:
            if (!LINK_IS_ADULT) {
                Math_SmoothStepToF(&this->actor.shape.yOffset, 0, 1.0f, 400.0f, 0.0f);
            }
            this->actor.targetMode = 0;
            this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
            this->playerStunWaitTimer = 0xA;
            this->grabWaitTimer = 0xF;
            EnRd_SetupWalkToPlayer(this, play);
            break;
    }
}

void EnRd_SetupAttemptPlayerFreeze(EnRd* this) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadLookBackAnim, 0.0f, 0.0f,
                     Animation_GetLastFrame(&gGibdoRedeadLookBackAnim), ANIMMODE_ONCE, 0.0f);
    this->action = 7;
    EnRd_SetupAction(this, EnRd_AttemptPlayerFreeze);
}

void EnRd_AttemptPlayerFreeze(EnRd* this, PlayState* play) {
    Vec3f sp34 = D_80AE492C;
    Color_RGBA8 sp30 = D_80AE4938;
    Color_RGBA8 sp2C = D_80AE493C;
    Player* player = GET_PLAYER(play);
    s16 temp_v0 =
        this->actor.yawTowardsPlayer - this->actor.shape.rot.y - this->headYRotation - this->upperBodyYRotation;

    if (ABS(temp_v0) < 0x2008) {
        if (!(this->rdFlags & 0x80) && GameInteractor_Should(VB_REDEAD_GIBDO_FREEZE_LINK, true, this)) {
            player->actor.freezeTimer = 60;
            Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 0x14, 0x96);
            Player_SetAutoLockOnActor(play, &this->actor);
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_AIM);
        EnRd_SetupWalkToPlayer(this, play);
    }
}

void EnRd_SetupStandUp(EnRd* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, &gGibdoRedeadStandUpAnim, -4.0f);
    this->action = 5;
    EnRd_SetupAction(this, EnRd_StandUp);
}

void EnRd_StandUp(EnRd* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->actor.parent != NULL) {
            EnRd_SetupWalkToParent(this);
        } else {
            EnRd_SetupAttemptPlayerFreeze(this);
        }
    }
}

void EnRd_SetupCrouch(EnRd* this) {
    Animation_Change(&this->skelAnime, &gGibdoRedeadStandUpAnim, -1.0f,
                     Animation_GetLastFrame(&gGibdoRedeadStandUpAnim), 0.0f, ANIMMODE_ONCE, -4.0f);
    this->action = 6;
    EnRd_SetupAction(this, EnRd_Crouch);
}

void EnRd_Crouch(EnRd* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        EnRd_SetupIdle(this);
    }
}

void EnRd_SetupDamaged(EnRd* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, &gGibdoRedeadDamageAnim, -6.0f);

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.speedXZ = -2.0f;
    }

    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_DAMAGE);
    this->action = 9;
    EnRd_SetupAction(this, EnRd_Damaged);
}

void EnRd_Damaged(EnRd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->actor.speedXZ < 0.0f) {
        this->actor.speedXZ += 0.15f;
    }

    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x12C, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x12C, 0);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.world.rot.y = this->actor.shape.rot.y;

        if (this->actor.parent != NULL) {
            EnRd_SetupWalkToParent(this);
        } else if (Actor_WorldDistXYZToPoint(&player->actor, &this->actor.home.pos) >= 150.0f) {
            EnRd_SetupWalkToHome(this, play);
        } else {
            EnRd_SetupWalkToPlayer(this, play);
        }

        this->unk_31D = 0xFF;
    }
}

void EnRd_SetupDead(EnRd* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, &gGibdoRedeadDeathAnim, -1.0f);
    this->action = 10;
    this->timer = 300;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_REDEAD_DEAD);
    EnRd_SetupAction(this, EnRd_Dead);
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
}

void EnRd_Dead(EnRd* this, PlayState* play) {
    if (this->actor.category != ACTORCAT_PROP) {
        Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    }

    Math_SmoothStepToS(&this->headYRotation, 0, 1, 0x7D0, 0);
    Math_SmoothStepToS(&this->upperBodyYRotation, 0, 1, 0x7D0, 0);

    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->timer == 0) {
            s8 enemyRandoCCActive = CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0) ||
                                    (CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 0));
            // Don't set this flag in Enemy Rando as it can overlap with other objects using the same flag.
            if (!Flags_GetSwitch(play, this->rdFlags & 0x7F) && !enemyRandoCCActive) {
                Flags_SetSwitch(play, this->rdFlags & 0x7F);
            }
            if (this->alpha != 0) {
                if (this->alpha == 0xB4) {
                    EnRd_UpdateMourningTarget(play, &this->actor, 0);
                }
                this->actor.scale.y -= 0.000075f;
                this->alpha -= 5;
            } else {
                Actor_Kill(&this->actor);
            }
        } else {
            this->timer--;
        }
    } else if (((s32)this->skelAnime.curFrame == 33) || ((s32)this->skelAnime.curFrame == 40)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_RIZA_DOWN);
    }
}

void EnRd_SetupStunned(EnRd* this) {
    this->action = 1;
    this->actor.speedXZ = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE) {
        this->stunnedBySunsSong = 1;
        this->sunsSongStunTimer = 0x258;
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_LIGHT_ARROW_HIT);
        Actor_SetColorFilter(&this->actor, -0x8000, -0x7F38, 0, 0xFF);
    } else if (this->damageReaction == 1) {
        Actor_SetColorFilter(&this->actor, 0, 0xC8, 0, 0x50);
    } else {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_LIGHT_ARROW_HIT);
        Actor_SetColorFilter(&this->actor, -0x8000, 0xC8, 0, 0x50);
    }
    EnRd_SetupAction(this, EnRd_Stunned);
}

void EnRd_Stunned(EnRd* this, PlayState* play) {
    if ((this->stunnedBySunsSong != 0) && (this->sunsSongStunTimer != 0)) {
        this->sunsSongStunTimer--;
        if (this->sunsSongStunTimer >= 0xFF) {
            Actor_SetColorFilter(&this->actor, -0x8000, 0xC8, 0, 0xFF);
        }
        if (this->sunsSongStunTimer == 0) {
            this->stunnedBySunsSong = 0;
            gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
        }
    }

    if (this->actor.colorFilterTimer == 0) {
        if (this->actor.colChkInfo.health == 0) {
            EnRd_UpdateMourningTarget(play, &this->actor, 1);
            EnRd_SetupDead(this);
            Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x90);
        } else {
            EnRd_SetupDamaged(this);
        }
    }
}

void EnRd_TurnTowardsPlayer(EnRd* this, PlayState* play) {
    s16 temp1;
    s16 temp2;
    s16 temp3;

    temp1 = this->actor.yawTowardsPlayer - (s16)(this->actor.shape.rot.y + this->upperBodyYRotation);
    temp2 = CLAMP(temp1, -500, 500);

    temp1 -= this->headYRotation;
    temp3 = CLAMP(temp1, -500, 500);

    if ((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y) >= 0) {
        this->upperBodyYRotation += ABS(temp2);
        this->headYRotation += ABS(temp3);
    } else {
        this->upperBodyYRotation -= ABS(temp2);
        this->headYRotation -= ABS(temp3);
    }

    this->upperBodyYRotation = CLAMP(this->upperBodyYRotation, -18783, 18783);
    this->headYRotation = CLAMP(this->headYRotation, -9583, 9583);
}

void EnRd_UpdateDamage(EnRd* this, PlayState* play) {
    s32 pad;
    Player* player = GET_PLAYER(play);

    if ((gSaveContext.sunsSongState != SUNSSONG_INACTIVE) && (this->actor.shape.rot.x == 0) &&
        (this->stunnedBySunsSong == 0) && (this->action != 9) && (this->action != 10) && (this->action != 1)) {
        EnRd_SetupStunned(this);
        return;
    }

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->damageReaction = this->actor.colChkInfo.damageEffect;

        if (this->action != 11) {
            Actor_SetDropFlag(&this->actor, &this->collider.info, 1);
            if (player->unk_844 != 0) {
                this->unk_31D = player->unk_845;
            }

            if ((this->damageReaction != 0) && (this->damageReaction != 6)) {
                if (((this->damageReaction == 1) || (this->damageReaction == 13)) && (this->action != 1)) {
                    Actor_ApplyDamage(&this->actor);
                    EnRd_SetupStunned(this);
                    return;
                }

                this->stunnedBySunsSong = 0;
                this->sunsSongStunTimer = 0;

                if (this->damageReaction == 0xE) {
                    Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0, 0x50);
                    this->fireTimer = 0x28;
                } else {
                    Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0, 8);
                }

                Actor_ApplyDamage(&this->actor);
                if (this->actor.colChkInfo.health == 0) {
                    EnRd_UpdateMourningTarget(play, &this->actor, 1);
                    EnRd_SetupDead(this);
                    Item_DropCollectibleRandom(play, 0, &this->actor.world.pos, 0x90);
                } else {
                    EnRd_SetupDamaged(this);
                }
            }
        }
    }
}

void EnRd_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    EnRd* this = (EnRd*)thisx;
    Player* player = GET_PLAYER(play);
    s32 pad2;

    // MergeShip2: MM EnRailgibud_Update -- the Gibdo Mask check comes before damage; a Gibdo in conversation cannot
    // be hurt (MM isInvincible).
    EnRd_MergeShipCheckForGibdoMask(this, play);
    if (this->mergeShipIsInvincible) {
        this->collider.base.acFlags &= ~AC_HIT;
    }

    EnRd_UpdateDamage(this, play);

    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE && this->stunnedBySunsSong == 0) {
        gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
    }

    if (this->damageReaction != 6 && ((this->action != 11) || (this->damageReaction != 14))) {
        if (this->playerStunWaitTimer != 0) {
            this->playerStunWaitTimer--;
        }

        this->actionFunc(this, play);
        if (this->action != 8 && this->actor.speedXZ != 0.0f) {
            Actor_MoveXZGravity(&this->actor);
        }

        if ((this->actor.shape.rot.x == 0) && (this->action != 8) && (this->actor.speedXZ != 0.0f)) {
            Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 20.0f, 35.0f, 0x1D);
        }

        if (this->action == 7) {
            EnRd_TurnTowardsPlayer(this, play);
        }
    }

    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 50.0f;

    if ((this->actor.colChkInfo.health > 0) && (this->action != 8)) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        if ((this->action != 9) || ((player->unk_844 != 0) && (player->unk_845 != this->unk_31D))) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }

    // MergeShip2: MM EnRd_Update -- after collision, as MM does.
    EnRd_SetupDanceIfConditionsMet(this, play);
}

s32 EnRd_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx, Gfx** gfx) {
    EnRd* this = (EnRd*)thisx;

    if (limbIndex == 23) {
        rot->y += this->headYRotation;
    } else if (limbIndex == 12) {
        rot->y += this->upperBodyYRotation;
    }
    return false;
}

void EnRd_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    Vec3f sp2C = D_80AE4940;
    EnRd* this = (EnRd*)thisx;
    s32 idx = -1;
    Vec3f destPos;

    if ((this->fireTimer != 0) || ((this->actor.colorFilterTimer != 0) && (this->actor.colorFilterParams & 0x4000))) {
        switch (limbIndex - 1) {
            case 23:
                idx = 0;
                break;
            case 0:
                idx = 1;
                break;
            case 21:
                idx = 2;
                break;
            case 17:
                idx = 3;
                break;
            case 13:
                idx = 4;
                break;
            case 24:
                idx = 5;
                break;
            case 8:
                idx = 6;
                break;
            case 3:
                idx = 7;
                break;
            case 10:
                idx = 8;
                break;
            case 5:
                idx = 9;
                break;
        }

        if (idx >= 0) {
            Matrix_MultVec3f(&sp2C, &destPos);
            this->firePos[idx].x = destPos.x;
            this->firePos[idx].y = destPos.y;
            this->firePos[idx].z = destPos.z;
        }
    }
}

void EnRd_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    EnRd* this = (EnRd*)thisx;
    Vec3f thisPos = thisx->world.pos;

    OPEN_DISPS(play->state.gfxCtx);

    if (this->alpha == 0xFF) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_OPA_DISP++, 8, &D_80116280[2]);
        POLY_OPA_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               EnRd_OverrideLimbDraw, EnRd_PostLimbDraw, this, POLY_OPA_DISP);
        func_80033C30(&thisPos, &D_80AE4958, 255, play);
        if (this->fireTimer != 0) {
            thisx->colorFilterTimer++;
            this->fireTimer--;
            if (this->fireTimer % 4 == 0) {
                EffectSsEnFire_SpawnVec3s(play, thisx, &this->firePos[this->fireTimer >> 2], 0x4B, 0, 0,
                                          (this->fireTimer >> 2));
            }
        }
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_XLU_DISP++, 8, &D_80116280[0]);
        POLY_XLU_DISP =
            SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                               EnRd_OverrideLimbDraw, NULL, this, POLY_XLU_DISP);

        func_80033C30(&thisPos, &D_80AE4958, this->alpha, play);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
