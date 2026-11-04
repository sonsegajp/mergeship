/*
 * File: z_mm_en_osn_travel.c
 * Overlay: ovl_MM_En_Osn_Travel
 * Description: the traveling Happy Mask Salesman (MergeShip2 singleplayer, docs/SINGLEPLAYER.md)
 *
 * Majora's Mask's own salesman (mm.o2r object_osn): MM En_Osn's model, eyes and mouth, head tracking and talking
 * poses (2ship2harkinian/mm/src/overlays/actors/ovl_En_Osn), roaming Ocarina of Time's ground.
 *
 * Walking: MM has no walk cycle for him, only gHappyMaskSalesmanWalkAwayAnim (78 frames: turning round, then walking
 * off). Its frames 51 and 69 are the same pose, so 51..69 loop seamlessly -- an 18-frame stride whose root moves
 * -271.72 model units a frame along -z. The draw holds the root where frame 51 has it and the actor moves itself at
 * that pace (2.72 units a frame at his 0.01 scale); in that stride he faces -z, so the model turns half round.
 *
 * Talking: MM's Happy Mask Salesman theme plays (MergeShip_MMAudio_PlayHMSTheme), and each text snaps him into a new
 * pose with no blend, as MM's salesman does (sTalkPoses). What he sells: MergeShipSingleplayer.h.
 */

#include "z_mm_en_osn_travel.h"
#include "soh/MergeShip/MergeShipSingleplayer.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include "soh/MergeShip/MergeShipMMSfx.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED)

void MergeShipTest_Log(const char* fmt, ...);

s16 gMergeShipActorId_MM_En_Osn_Travel = -1;

typedef enum MMEnOsnTravelAnim {
    OSNT_ANIM_IDLE,
    OSNT_ANIM_WALK,
    OSNT_ANIM_BOWING,
    OSNT_ANIM_HANDS_CLASPED,
    OSNT_ANIM_THINK,
    OSNT_ANIM_BELIEVE,
    OSNT_ANIM_SHAKE_HEAD,
    OSNT_ANIM_FAST_BOWS,
    OSNT_ANIM_ARMS_OUT,
    OSNT_ANIM_HAND_OUT,
    OSNT_ANIM_REMINISCE,
    OSNT_ANIM_SHAKE,
    OSNT_ANIM_WALK_AWAY, // the whole clip: he turns round and walks off (the Lost Woods meeting's end)
    OSNT_ANIM_MAX
} MMEnOsnTravelAnim;

typedef enum MMEnOsnTravelTalk {
    OSNT_TALK_NONE,
    OSNT_TALK_GREET,
    OSNT_TALK_ADULT,
    OSNT_TALK_MENU,
    OSNT_TALK_CONFIRM,
    OSNT_TALK_BOUGHT,
    OSNT_TALK_NO_RUPEES,
    OSNT_TALK_FD_GIFT,
    OSNT_TALK_END, // farewell / sold out: the box closes the conversation
    OSNT_TALK_DOOR_ASK, // the Lost Woods: bring the masks?
    OSNT_TALK_DOOR_BYE, // ... his last words; then he walks away
} MMEnOsnTravelTalk;

// The walk loop (see the file header).
#define OSNT_WALK_START 51.0f
#define OSNT_WALK_END 69.0f
#define OSNT_WALK_ROOT_Z51 (-9198.0f)
#define OSNT_WALK_ROOT_DZ (-271.72f)
#define OSNT_WALK_SPEED 2.72f

static AnimationInfo sAnimationInfo[OSNT_ANIM_MAX] = {
    { (AnimationHeader*)gHappyMaskSalesmanIdleAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, -6.0f },                      // IDLE
    { (AnimationHeader*)gHappyMaskSalesmanWalkAwayAnim, 1.0f, OSNT_WALK_START, OSNT_WALK_END, ANIMMODE_LOOP_PARTIAL, 0.0f }, // WALK
    { (AnimationHeader*)gHappyMaskSalesmanBowingAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                     // BOWING
    { (AnimationHeader*)gHappyMaskSalesmanHandsClaspedAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },               // HANDS_CLASPED
    { (AnimationHeader*)gHappyMaskSalesmanThinkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                      // THINK
    { (AnimationHeader*)gHappyMaskSalesmanBelieveAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                    // BELIEVE
    { (AnimationHeader*)gHappyMaskSalesmanShakeHeadAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                  // SHAKE_HEAD
    { (AnimationHeader*)gHappyMaskSalesmanFastBowsAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                   // FAST_BOWS
    { (AnimationHeader*)gHappyMaskSalesmanArmsOutAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                    // ARMS_OUT
    { (AnimationHeader*)gHappyMaskSalesmanHandOutAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                    // HAND_OUT
    { (AnimationHeader*)gHappyMaskSalesmanReminisceAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                  // REMINISCE
    { (AnimationHeader*)gHappyMaskSalesmanShakeAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },                      // SHAKE
    { (AnimationHeader*)gHappyMaskSalesmanWalkAwayAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, -6.0f },                  // WALK_AWAY
};

// His voice with each text, as MM's own texts for him use them (MM message data: NA_SE_VO_OMVO00 opens his greetings,
// OMVO06 presents a mask, OMVO03 haggles, OMVO01 praises, OMVO02 takes his leave, OMVO05 marvels).
static const u16 sTalkVoices[] = {
    /* NONE */ 0,
    /* GREET */ MM_NA_SE_VO_OMVO00,
    /* ADULT */ MM_NA_SE_VO_OMVO02,
    /* MENU */ MM_NA_SE_VO_OMVO06,
    /* CONFIRM */ MM_NA_SE_VO_OMVO06,
    /* BOUGHT */ MM_NA_SE_VO_OMVO01,
    /* NO_RUPEES */ MM_NA_SE_VO_OMVO03,
    /* FD_GIFT */ MM_NA_SE_VO_OMVO05,
    /* END */ MM_NA_SE_VO_OMVO02,
    /* DOOR_ASK */ MM_NA_SE_VO_OMVO00,
    /* DOOR_BYE */ MM_NA_SE_VO_OMVO02,
};

// Each text snaps him into one of these (MM picks a pose per message the same way, En_Osn's sTextIdAnims).
static const u8 sTalkPoses[][3] = {
    /* NONE */ { OSNT_ANIM_IDLE, OSNT_ANIM_IDLE, OSNT_ANIM_IDLE },
    /* GREET */ { OSNT_ANIM_BOWING, OSNT_ANIM_ARMS_OUT, OSNT_ANIM_BELIEVE },
    /* ADULT */ { OSNT_ANIM_REMINISCE, OSNT_ANIM_THINK, OSNT_ANIM_HAND_OUT },
    /* MENU */ { OSNT_ANIM_HANDS_CLASPED, OSNT_ANIM_THINK, OSNT_ANIM_HAND_OUT },
    /* CONFIRM */ { OSNT_ANIM_HAND_OUT, OSNT_ANIM_BELIEVE, OSNT_ANIM_HANDS_CLASPED },
    /* BOUGHT */ { OSNT_ANIM_FAST_BOWS, OSNT_ANIM_BOWING, OSNT_ANIM_ARMS_OUT },
    /* NO_RUPEES */ { OSNT_ANIM_SHAKE_HEAD, OSNT_ANIM_SHAKE, OSNT_ANIM_SHAKE_HEAD },
    /* FD_GIFT */ { OSNT_ANIM_ARMS_OUT, OSNT_ANIM_BELIEVE, OSNT_ANIM_HAND_OUT },
    /* END */ { OSNT_ANIM_BOWING, OSNT_ANIM_FAST_BOWS, OSNT_ANIM_HANDS_CLASPED },
    /* DOOR_ASK */ { OSNT_ANIM_ARMS_OUT, OSNT_ANIM_REMINISCE, OSNT_ANIM_BELIEVE },
    /* DOOR_BYE */ { OSNT_ANIM_BOWING, OSNT_ANIM_HANDS_CLASPED, OSNT_ANIM_REMINISCE },
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 30, 40, 0, { 0, 0, 0 } },
};

static void MMEnOsnTravel_Roam(MMEnOsnTravel* this, PlayState* play);
static void MMEnOsnTravel_StartWalking(MMEnOsnTravel* this);
static void MMEnOsnTravel_StopWalking(MMEnOsnTravel* this, s16 idleFrames);
static void MMEnOsnTravel_Talk(MMEnOsnTravel* this, PlayState* play);
static void MMEnOsnTravel_WalkAway(MMEnOsnTravel* this, PlayState* play);

static void MMEnOsnTravel_ChangeAnim(MMEnOsnTravel* this, s32 anim) {
    AnimationInfo* info = &sAnimationInfo[anim];
    f32 end = (info->frameCount < 0.0f) ? Animation_GetLastFrame(info->animation) : info->frameCount;

    this->animIndex = (u8)anim;
    Animation_Change(&this->skelAnime, info->animation, info->playSpeed, info->startFrame, end, info->mode,
                     info->morphFrames);
}

// A text opens: a new pose, no blend.
static void MMEnOsnTravel_TalkPose(MMEnOsnTravel* this, s32 talkState) {
    this->talkState = (s16)talkState;
    MMEnOsnTravel_ChangeAnim(this, sTalkPoses[talkState][(s32)(Rand_ZeroOne() * 2.999f)]);
    if (sTalkVoices[talkState] != 0) {
        MergeShip_MMSfx_PlaySfx(sTalkVoices[talkState], &this->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

static void MMEnOsnTravel_Say(MMEnOsnTravel* this, PlayState* play, s32 talkState, u16 textId) {
    Message_ContinueTextbox(play, textId);
    MMEnOsnTravel_TalkPose(this, talkState);
    MergeShipTest_Log("SPSALESMAN text 0x%04X (state %d)", textId, talkState);
}

// The yaw from him to the camera (the viewer).
static s16 MMEnOsnTravel_YawToViewer(MMEnOsnTravel* this, PlayState* play) {
    Camera* cam = GET_ACTIVE_CAM(play);
    return (cam != NULL) ? Math_Vec3f_Yaw(&this->actor.world.pos, &cam->eye) : this->actor.yawTowardsPlayer;
}

// Where to walk next: a point within the roaming radius, on ground he can stand on.
static void MMEnOsnTravel_PickTarget(MMEnOsnTravel* this) {
    s16 angle = (s16)(Rand_ZeroOne() * 0x10000);
    f32 dist = this->radius * (0.3f + Rand_ZeroOne() * 0.7f);

    this->target.x = this->home.x + Math_SinS(angle) * dist;
    this->target.y = this->home.y;
    this->target.z = this->home.z + Math_CosS(angle) * dist;
}

// Ground ahead he can walk onto: a floor within a step up or down, and no water over it.
static s32 MMEnOsnTravel_GroundAhead(MMEnOsnTravel* this, PlayState* play) {
    CollisionPoly* poly;
    s32 bgId;
    WaterBox* waterBox;
    f32 waterY;
    Vec3f ahead;
    f32 floorY;

    ahead.x = this->actor.world.pos.x + Math_SinS(this->actor.world.rot.y) * 40.0f;
    ahead.y = this->actor.world.pos.y + 40.0f;
    ahead.z = this->actor.world.pos.z + Math_CosS(this->actor.world.rot.y) * 40.0f;
    floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &this->actor, &ahead);
    if ((floorY <= BGCHECK_Y_MIN) || (fabsf(floorY - this->actor.world.pos.y) > 20.0f)) {
        return false;
    }
    if (WaterBox_GetSurface1(play, &play->colCtx, ahead.x, ahead.z, &waterY, &waterBox) && (waterY > floorY)) {
        return false;
    }
    return true;
}

void MMEnOsnTravel_Init(Actor* thisx, PlayState* play) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;
    CollisionPoly* poly;
    s32 bgId;
    f32 floorY;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gHappyMaskSalesmanSkel,
                       (AnimationHeader*)gHappyMaskSalesmanIdleAnim, this->jointTable, this->morphTable,
                       HAPPY_MASK_SALESMAN_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.targetMode = 0;
    this->actor.gravity = -1.0f;

    // Stand on the ground under the spawn point.
    floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &this->actor, &this->actor.world.pos);
    if (floorY > BGCHECK_Y_MIN) {
        this->actor.world.pos.y = floorY;
    }
    // The schedule's plan for him (MergeShipSingleplayer.h): roam a stay, or walk through.
    {
        const MsSalesmanPlan* plan = &gMsSalesmanPlan;
        s32 i;

        this->planMode = plan->mode;
        this->wayCount = (s16)CLAMP_MAX(plan->wayCount, 24);
        for (i = 0; i < this->wayCount; i++) {
            this->way[i][0] = plan->way[i][0];
            this->way[i][1] = plan->way[i][1];
            this->way[i][2] = plan->way[i][2];
        }
        this->wayIndex = 0;
        this->stay = plan->stay;
        this->fadeOut = plan->fadeOut;
        this->endAtHub = plan->endAtHub;
        this->alpha = plan->fadeIn ? 0 : 255;
        this->home.x = plan->hub[0];
        this->home.y = plan->hub[1];
        this->home.z = plan->hub[2];
        this->radius = CLAMP_MIN(plan->radius, 40.0f);
    }
    this->timer = 20;
    if (this->planMode == MS_SALESMAN_PLAN_TRAVEL) {
        MMEnOsnTravel_StartWalking(this);
    } else {
        MMEnOsnTravel_ChangeAnim(this, OSNT_ANIM_IDLE);
    }
    this->actionFunc = MMEnOsnTravel_Roam;
}

void MMEnOsnTravel_Destroy(Actor* thisx, PlayState* play) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;

    if (this->talkState != OSNT_TALK_NONE) {
        MergeShip_MMAudio_StopHMSTheme();
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static void MMEnOsnTravel_StartWalking(MMEnOsnTravel* this) {
    this->walking = true;
    MMEnOsnTravel_ChangeAnim(this, OSNT_ANIM_WALK);
}

static void MMEnOsnTravel_StopWalking(MMEnOsnTravel* this, s16 idleFrames) {
    AnimationInfo* idle = &sAnimationInfo[OSNT_ANIM_IDLE];

    this->walking = false;
    this->actor.speedXZ = 0.0f;
    this->timer = idleFrames;
    // No blend out of the walk (nor into it, sAnimationInfo): the walk clip's root carries a stride's travel and a
    // half turn that the draw only takes out while the walk itself plays -- blended, he would spin and jump.
    this->animIndex = OSNT_ANIM_IDLE;
    Animation_Change(&this->skelAnime, idle->animation, idle->playSpeed, idle->startFrame,
                     Animation_GetLastFrame(idle->animation), idle->mode, 0.0f);
}

static void MMEnOsnTravel_Roam(MMEnOsnTravel* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 playerDist = Actor_WorldDistXZToActor(&this->actor, &player->actor);

    // Spoken to: the shop opens.
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->walking = false;
        this->actor.speedXZ = 0.0f;
        MergeShip_MMAudio_PlayHMSTheme();
        this->talked = true;
        this->actionFunc = MMEnOsnTravel_Talk;
        MergeShipTest_Log("SPSALESMAN talk 0x%04X", this->actor.textId);
        if (this->planMode == MS_SALESMAN_PLAN_DOOR) {
            this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            MMEnOsnTravel_TalkPose(this, (this->actor.textId == MERGESHIP_TEXT_SP_DOOR_ASK) ? OSNT_TALK_DOOR_ASK
                                                                                          : OSNT_TALK_DOOR_BYE);
            return;
        }
        MergeShip_Singleplayer_ShopBegin();
        MMEnOsnTravel_TalkPose(this, (this->actor.textId == MERGESHIP_TEXT_SP_ADULT) ? OSNT_TALK_ADULT
                                     : (this->actor.textId == MERGESHIP_TEXT_SP_SOLD_OUT) ? OSNT_TALK_END
                                                                                         : OSNT_TALK_GREET);
        return;
    }

    // The Lost Woods meeting: he stands in Link's way (the woods lead nowhere new until Link has spoken to him).
    if (this->planMode == MS_SALESMAN_PLAN_DOOR) {
        // He faces the viewer (the camera), whom Link's approach puts behind Link.
        Math_SmoothStepToS(&this->actor.shape.rot.y, MMEnOsnTravel_YawToViewer(this, play), 4, 0x800, 0x100);
        this->actor.world.rot.y = this->actor.shape.rot.y;
        if (playerDist < 200.0f) {
            this->actor.textId = MergeShip_Singleplayer_HasAnyMask() ? MERGESHIP_TEXT_SP_DOOR_ASK
                                                                     : MERGESHIP_TEXT_SP_DOOR_NONE;
            Actor_OfferTalk(&this->actor, play, 120.0f); // Link speaks to him (A)
        }
        return;
    }

    // Fading in as he arrives, or out as he leaves (MM EnOsn_FadeOut); gone once faded.
    if (this->leaving) {
        if (this->alpha <= 15) {
            MergeShip_Singleplayer_LeftScene(this->stay);
            Actor_Kill(&this->actor);
            return;
        }
        this->alpha -= 15;
    } else if (this->alpha < 255) {
        this->alpha = (this->alpha > 240) ? 255 : (this->alpha + 15);
    }

    // Link close by: he can be spoken to (A), and keeps to his way -- he only stops once Link talks to him. Standing
    // still, he turns to Link.
    if ((playerDist < 100.0f) && !this->leaving) {
        if (!this->walking) {
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x800, 0x100);
            this->actor.world.rot.y = this->actor.shape.rot.y;
        }
        this->actor.textId = MergeShip_Singleplayer_AdultMaskOffered() ? MERGESHIP_TEXT_SP_ADULT
                             : MergeShip_Singleplayer_SoldOut()       ? MERGESHIP_TEXT_SP_SOLD_OUT
                                                                      : MERGESHIP_TEXT_SP_GREET;
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }

    // Walking through: one waypoint after the other (paths planned on the scene's collision), then he stays at the hub
    // or leaves the scene.
    if (this->planMode == MS_SALESMAN_PLAN_TRAVEL) {
        // Link in his way: he waits for him to step aside.
        if ((playerDist < 70.0f) && (ABS((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y)) < 0x3000)) {
            if (this->walking) {
                MMEnOsnTravel_StopWalking(this, 20);
            }
            return;
        }
        if (!this->walking) {
            if (this->timer > 0) {
                this->timer--;
                return;
            }
            MMEnOsnTravel_StartWalking(this);
        }
        if (this->wayIndex >= this->wayCount) {
            if (this->endAtHub) {
                this->planMode = MS_SALESMAN_PLAN_STAY;
                MMEnOsnTravel_StopWalking(this, 60);
            } else if (this->fadeOut) {
                this->leaving = true;
            } else {
                MergeShip_Singleplayer_LeftScene(this->stay);
                Actor_Kill(&this->actor);
            }
            return;
        }
        this->target.x = this->way[this->wayIndex][0];
        this->target.y = this->way[this->wayIndex][1];
        this->target.z = this->way[this->wayIndex][2];
        Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->target), 3, 0x800,
                           0x100);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        this->actor.speedXZ = OSNT_WALK_SPEED;
        if (Math_Vec3f_DistXZ(&this->actor.world.pos, &this->target) < 20.0f) {
            this->wayIndex++;
        }
        return;
    }

    // Staying: when the stay is over he sets off by the way out of this scene.
    if ((play->gameplayFrames % 20) == 0) {
        MsSalesmanPlan plan;
        if (MergeShip_Singleplayer_StayOver(this->stay) && MergeShip_Singleplayer_DeparturePlan(&plan)) {
            s32 i;
            this->planMode = MS_SALESMAN_PLAN_TRAVEL;
            this->wayCount = (s16)CLAMP_MAX(plan.wayCount, 24);
            for (i = 0; i < this->wayCount; i++) {
                this->way[i][0] = plan.way[i][0];
                this->way[i][1] = plan.way[i][1];
                this->way[i][2] = plan.way[i][2];
            }
            this->wayIndex = 0;
            this->stay = plan.stay;
            this->fadeOut = plan.fadeOut;
            this->endAtHub = plan.endAtHub;
            MMEnOsnTravel_StartWalking(this);
            MergeShipTest_Log("SPSALESMAN stay over: leaving by %d waypoints", this->wayCount);
            return;
        }
    }

    if (!this->walking) {
        // He only lingers where Link can see him stop; on his own he walks on to the next spot at once.
        if ((this->timer > 0) && (playerDist < 300.0f)) {
            this->timer--;
            return;
        }
        this->timer = 0;
        MMEnOsnTravel_PickTarget(this);
        MMEnOsnTravel_StartWalking(this);
    }

    // Walking: toward the target, never off the ground (a ledge, a slope too steep, water) nor into a wall.
    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &this->target), 4, 0x600,
                       0x80);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.speedXZ = OSNT_WALK_SPEED;
    // Link in his way is a wall too: he pauses, then goes elsewhere (no marching on the spot against him).
    if (!MMEnOsnTravel_GroundAhead(this, play) || (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) ||
        ((playerDist < 70.0f) && (ABS((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y)) < 0x3000))) {
        MMEnOsnTravel_StopWalking(this, 20 + (s16)(Rand_ZeroOne() * 40.0f));
        return;
    }
    if (Math_Vec3f_DistXZ(&this->actor.world.pos, &this->target) < 20.0f) {
        MMEnOsnTravel_StopWalking(this, 40 + (s16)(Rand_ZeroOne() * 100.0f));
    }
}

static void MMEnOsnTravel_EndTalk(MMEnOsnTravel* this) {
    MergeShip_MMAudio_StopHMSTheme();
    this->talkState = OSNT_TALK_NONE;
    MMEnOsnTravel_StopWalking(this, 60);
    this->actionFunc = MMEnOsnTravel_Roam;
    MergeShipTest_Log("SPSALESMAN talk over");
}

// After a sale (or the Adult Mask's refusal): the Fierce Deity's gift, more masks, or goodbye.
static void MMEnOsnTravel_AfterSale(MMEnOsnTravel* this, PlayState* play) {
    if (MergeShip_Singleplayer_GiveFierceDeityIfDone()) {
        MMEnOsnTravel_Say(this, play, OSNT_TALK_FD_GIFT, MERGESHIP_TEXT_SP_FD_GIFT);
    } else if (MergeShip_Singleplayer_OfferCount() > 0) {
        MergeShip_Singleplayer_ShopBegin();
        if (MergeShip_Singleplayer_OfferCount() > 0) {
            MMEnOsnTravel_Say(this, play, OSNT_TALK_MENU, MERGESHIP_TEXT_SP_MENU);
        } else {
            MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_FAREWELL);
        }
    } else {
        MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_FAREWELL);
    }
}

// The menu, or the confirm text straight away when he has a single mask left to show.
static void MMEnOsnTravel_OpenMenu(MMEnOsnTravel* this, PlayState* play) {
    if (MergeShip_Singleplayer_OfferCount() <= 0) {
        MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_SOLD_OUT);
    } else if (MergeShip_Singleplayer_OfferCount() == 1) {
        MergeShip_Singleplayer_ChooseOffer(0);
        MMEnOsnTravel_Say(this, play, OSNT_TALK_CONFIRM, MERGESHIP_TEXT_SP_CONFIRM);
    } else {
        MMEnOsnTravel_Say(this, play, OSNT_TALK_MENU, MERGESHIP_TEXT_SP_MENU);
    }
}

static void MMEnOsnTravel_Talk(MMEnOsnTravel* this, PlayState* play) {
    u8 state = Message_GetState(&play->msgCtx);

    Math_SmoothStepToS(&this->actor.shape.rot.y,
                       (this->planMode == MS_SALESMAN_PLAN_DOOR) ? MMEnOsnTravel_YawToViewer(this, play)
                                                                 : this->actor.yawTowardsPlayer,
                       4, 0x800, 0x100);
    this->actor.world.rot.y = this->actor.shape.rot.y;

    if ((state == TEXT_STATE_NONE) || (state == TEXT_STATE_CLOSING)) {
        if (play->msgCtx.msgMode == MSGMODE_NONE) {
            if (this->planMode == MS_SALESMAN_PLAN_DOOR) {
                // MM's own exit for him: the whole walk-away clip, his root carrying him off.
                this->talkState = OSNT_TALK_DOOR_BYE; // the theme plays on until he is gone (Destroy stops it)
                this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED);
                this->walkAwayRootZ0 = this->jointTable[0].z /* the root's translation */;
                this->walkAwayPrevRootZ = this->walkAwayRootZ0;
                MMEnOsnTravel_ChangeAnim(this, OSNT_ANIM_WALK_AWAY);
                this->actionFunc = MMEnOsnTravel_WalkAway;
                MergeShipTest_Log("SPDOOR he walks away");
                return;
            }
            MMEnOsnTravel_EndTalk(this);
        }
        return;
    }

    switch (this->talkState) {
        case OSNT_TALK_GREET:
            if ((state == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                MMEnOsnTravel_OpenMenu(this, play);
            }
            break;

        case OSNT_TALK_ADULT:
            if ((state == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                if (play->msgCtx.choiceIndex == 0) {
                    if (MergeShip_Singleplayer_BuyAdultMask()) {
                        MMEnOsnTravel_Say(this, play, OSNT_TALK_BOUGHT, MERGESHIP_TEXT_SP_BOUGHT);
                    } else {
                        MMEnOsnTravel_Say(this, play, OSNT_TALK_NO_RUPEES, MERGESHIP_TEXT_SP_NO_RUPEES);
                    }
                } else {
                    MMEnOsnTravel_OpenMenu(this, play);
                }
            }
            break;

        case OSNT_TALK_MENU:
            if ((state == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                if (MergeShip_Singleplayer_ChooseOffer(play->msgCtx.choiceIndex)) {
                    MMEnOsnTravel_Say(this, play, OSNT_TALK_CONFIRM, MERGESHIP_TEXT_SP_CONFIRM);
                } else {
                    MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_FAREWELL);
                }
            }
            break;

        case OSNT_TALK_CONFIRM:
            if ((state == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                if (play->msgCtx.choiceIndex == 0) {
                    if (MergeShip_Singleplayer_BuyChosen()) {
                        MMEnOsnTravel_Say(this, play, OSNT_TALK_BOUGHT, MERGESHIP_TEXT_SP_BOUGHT);
                    } else {
                        MMEnOsnTravel_Say(this, play, OSNT_TALK_NO_RUPEES, MERGESHIP_TEXT_SP_NO_RUPEES);
                    }
                } else {
                    MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_FAREWELL);
                }
            }
            break;

        case OSNT_TALK_BOUGHT:
            if ((state == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                MMEnOsnTravel_AfterSale(this, play);
            }
            break;

        case OSNT_TALK_DOOR_ASK:
            if ((state == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                if (play->msgCtx.choiceIndex == 0) {
                    MMEnOsnTravel_Say(this, play, OSNT_TALK_DOOR_BYE, MERGESHIP_TEXT_SP_DOOR_BRING);
                } else {
                    MergeShip_Singleplayer_SellAllMasks();
                    MMEnOsnTravel_Say(this, play, OSNT_TALK_DOOR_BYE, MERGESHIP_TEXT_SP_DOOR_SOLD);
                }
            }
            break;

        case OSNT_TALK_NO_RUPEES:
        case OSNT_TALK_FD_GIFT:
            if ((state == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                MMEnOsnTravel_Say(this, play, OSNT_TALK_END, MERGESHIP_TEXT_SP_FAREWELL);
            }
            break;

        default:
            break;
    }
}

// After the Lost Woods meeting: the walk-away clip to its end, then on with the walk loop from where its root left him,
// fading out (MM EnOsn_FadeOut); gone, the woods lead to Termina.
static void MMEnOsnTravel_WalkAway(MMEnOsnTravel* this, PlayState* play) {
    if (this->animIndex == OSNT_ANIM_WALK_AWAY) {
        if (this->skelAnime.curFrame < Animation_GetLastFrame(sAnimationInfo[OSNT_ANIM_WALK_AWAY].animation)) {
            return;
        }
        // The clip has walked him off (the actor with it, MMEnOsnTravel_Update): he walks on with the walk loop.
        this->actor.shape.rot.y += 0x8000;
        this->actor.world.rot.y = this->actor.shape.rot.y;
        MMEnOsnTravel_StartWalking(this);
        this->leaving = true;
    }
    this->actor.speedXZ = OSNT_WALK_SPEED;
    if (this->alpha <= 6) {
        MergeShip_Singleplayer_DoorDone();
        Actor_Kill(&this->actor);
        return;
    }
    this->alpha -= 6;
}

void MMEnOsnTravel_Update(Actor* thisx, PlayState* play) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;

    this->actionFunc(this, play);
    SkelAnime_Update(&this->skelAnime);
    // The walk-away clip walks him off along the model's -z: the actor follows its root, frame by frame, so he (and
    // his collision) really leaves; the draw keeps the root where the clip began.
    if (this->animIndex == OSNT_ANIM_WALK_AWAY) {
        s16 rootZ = this->jointTable[0].z /* the root's translation */;
        f32 step = (this->skelAnime.morphWeight <= 0.0f) ? (rootZ - this->walkAwayPrevRootZ) * this->actor.scale.z
                                                         : 0.0f;

        this->walkAwayPrevRootZ = rootZ;
        this->actor.world.pos.x += Math_SinS(this->actor.shape.rot.y) * step;
        this->actor.world.pos.z += Math_CosS(this->actor.shape.rot.y) * step;
    }
    // The walk loop: the actor goes as far as the clip's root does each frame (his feet stay planted); the draw holds the
    // root still. Wrapping from frame 69 back to 51 adds one stride's travel. While a blend into the clip runs, the root
    // is the blend's, not the walk's: a steady pace instead.
    if (this->animIndex == OSNT_ANIM_WALK) {
        s16 rootZ = this->jointTable[0].z /* the root's translation */;
        f32 dz = rootZ - this->walkPrevRootZ;

        if (this->walkPrevValid && (this->skelAnime.morphWeight <= 0.0f)) {
            if (dz > 0.0f) {
                dz += OSNT_WALK_ROOT_DZ * (OSNT_WALK_END - OSNT_WALK_START);
            }
            // A step is one frame of the stride (the clip's root moves about 200..340 a frame); anything else is no
            // step of the walk (the clip just set up), and he keeps his pace.
            if ((this->actor.speedXZ > 0.0f) && (dz < 0.0f) && (dz > -600.0f)) {
                this->actor.speedXZ = -dz * this->actor.scale.z;
            }
        }
        this->walkPrevValid = (this->skelAnime.morphWeight <= 0.0f);
        this->walkPrevRootZ = rootZ;
    } else {
        this->walkPrevValid = false;
    }
    // MM EnOsn: his steps on the walk's frames 17, 27 ... 67 (57 and 67 inside the loop).
    if (((this->animIndex == OSNT_ANIM_WALK) &&
         (Animation_OnFrame(&this->skelAnime, 57.0f) || Animation_OnFrame(&this->skelAnime, 67.0f))) ||
        ((this->animIndex == OSNT_ANIM_WALK_AWAY) &&
         (Animation_OnFrame(&this->skelAnime, 17.0f) || Animation_OnFrame(&this->skelAnime, 27.0f) ||
          Animation_OnFrame(&this->skelAnime, 37.0f) || Animation_OnFrame(&this->skelAnime, 47.0f) ||
          Animation_OnFrame(&this->skelAnime, 57.0f) || Animation_OnFrame(&this->skelAnime, 67.0f)))) {
        MergeShip_MMSfx_PlaySfx(MM_NA_SE_EV_OMENYA_WALK, &this->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 26.0f, 20.0f, 60.0f, 0x1D);
    Actor_SetFocus(&this->actor, 60.0f);
    Actor_TrackPlayer(play, &this->actor, &this->headRot, &this->torsoRot, this->actor.focus.pos);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 MMEnOsnTravel_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;

    if ((limbIndex == HAPPY_MASK_SALESMAN_LIMB_ROOT) && (this->animIndex == OSNT_ANIM_WALK)) {
        // The walk loop moves its root along -z: held still where the idle has him (the actor moves itself instead,
        // MMEnOsnTravel_Update, by the root's own travel).
        pos->z = 0;
    }
    if ((limbIndex == HAPPY_MASK_SALESMAN_LIMB_ROOT) && (this->animIndex == OSNT_ANIM_WALK_AWAY)) {
        pos->z = this->walkAwayRootZ0; // the actor itself has gone the root's way (MMEnOsnTravel_Update)
    }
    if ((limbIndex == HAPPY_MASK_SALESMAN_LIMB_HEAD) && (this->talkState != OSNT_TALK_NONE)) {
        Matrix_RotateX(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
    }
    return false;
}

static void MMEnOsnTravel_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;
    Vec3f headOffset = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == HAPPY_MASK_SALESMAN_LIMB_HEAD) {
        Matrix_MultVec3f(&headOffset, &this->actor.focus.pos);
    }
}

// MM renderModeSetNoneDL: its display lists may call into the segment at an offset (its other variants), so every
// entry ends.
// MM renderModeSetXluSingleCycleDL, padded the same way, for him fading in and out.
static Gfx sMMRenderModeXluDL[] = {
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
    gsDPSetRenderMode(AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA),
                      G_RM_AA_ZB_XLU_SURF2),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

static s32 MMEnOsnTravel_OverrideLimbDrawXlu(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                             void* thisx, Gfx** gfx) {
    return MMEnOsnTravel_OverrideLimbDraw(play, limbIndex, dList, pos, rot, thisx);
}

static void MMEnOsnTravel_PostLimbDrawXlu(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                          Gfx** gfx) {
    MMEnOsnTravel_PostLimbDraw(play, limbIndex, dList, rot, thisx);
}

static Gfx sMMRenderModeNoneDL[] = {
    gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList(),
    gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList(),
};

void MMEnOsnTravel_Draw(Actor* thisx, PlayState* play) {
    MMEnOsnTravel* this = (MMEnOsnTravel*)thisx;
    const void* eyes = gHappyMaskSalesmanEyeClosedHappyTex;
    const void* mouth = gHappyMaskSalesmanSmileTex;

    // MM EnOsn_Draw's faces.
    if ((this->animIndex == OSNT_ANIM_HAND_OUT) || (this->talkState == OSNT_TALK_FD_GIFT)) {
        eyes = gHappyMaskSalesmanEyeOpenTex;
    } else if ((this->animIndex == OSNT_ANIM_SHAKE_HEAD) || (this->animIndex == OSNT_ANIM_REMINISCE) ||
               (this->animIndex == OSNT_ANIM_FAST_BOWS)) {
        eyes = gHappyMaskSalesmanEyeClosedAngryTex;
        mouth = (this->animIndex == OSNT_ANIM_FAST_BOWS) ? gHappyMaskSalesmanSmileTex : gHappyMaskSalesmanFrownTex;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // The walk-away clip has him turned round (facing and walking -z): half round again, he walks the way he faces.
    if (this->animIndex == OSNT_ANIM_WALK) {
        Matrix_RotateY(M_PI, MTXMODE_APPLY);
    }

    // Fading (MM EnOsn_Draw's translucent branch).
    if (this->alpha < 255) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eyes));
        gSPSegment(POLY_XLU_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(mouth));
        gSPSegment(POLY_XLU_DISP++, 0x0C, sMMRenderModeXluDL);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, this->alpha);
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                           this->skelAnime.dListCount, MMEnOsnTravel_OverrideLimbDrawXlu,
                                           MMEnOsnTravel_PostLimbDrawXlu, this, POLY_XLU_DISP);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eyes));
        gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(mouth));
        // MM Scene_SetRenderModeXlu(play, 0, 1): MM's display lists call segment 0x0C for their render mode, an empty
        // list when opaque. Ocarina of Time's scene keeps its own list there, which must not run inside his model.
        gSPSegment(POLY_OPA_DISP++, 0x0C, sMMRenderModeNoneDL);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              MMEnOsnTravel_OverrideLimbDraw, MMEnOsnTravel_PostLimbDraw, this);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
