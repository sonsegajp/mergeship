// MergeShip2: Ganondorf mode's abilities (MergeShipGanondorf.h), compiled into z_player.c.
//
// Ganondorf never draws Link's sword (B holds no sword for him, Player_GetItemOnButton) and never raises Link's shield.
// B and R are the bosses' own attacks instead, each a player action that plays the boss's animation on his rig
// (MergeShip_Ganondorf_SetPose) with the boss's timings, sounds and magic (ovl_MS_Ganon_Magic):
//
//   Ganondorf (both ages; the tower boss, BossGanon)
//     B          BossGanon_ChargeLightBall + _PlayTennis: the light ball, thrown at what he is locked on to
//     B held     BossGanon_ChargeBigMagic: the big magic (release once charged; it fires by itself when full)
//     R          BossGanon_Block: his barrier (nothing hurts him while it is up)
//     R, then B  BossGanon_PoundFloor: he rises and dives; the impact's shockwave knocks everything around
//   Phantom Ganon (the Deku Mask's place; BossGanondrof)
//     R          he floats (BossGanondrof's flight) and drifts with the stick; let go to come down
//     B          BossGanondrof_Throw's energy ball, homing on what he is locked on to; hold B to keep it charged on the
//                spear tip (as the Deku's bubble) and let go to throw it -- standing or floating
//     A floating BossGanondrof_Charge: his dive at what he is locked on to
//   Beast Ganon (the Fierce Deity's Mask's place; BossGanon2, his own skeleton)
//     B          his two swords in turn (func_80900580 / func_80900650: the hit window around the swing)
//     R          his guard (guard stance; guard walk with the stick), nothing hurts him
//     R, then B  his roar (stuns what is around him)

#include "objects/object_ganon_anime1/object_ganon_anime1.h"
#include "objects/object_ganon_anime3/object_ganon_anime3.h"
#include "objects/object_gnd/object_gnd.h"
#include "objects/object_ganon2/object_ganon2.h"
#include "src/overlays/actors/ovl_MS_Ganon_Magic/z_ms_ganon_magic.h"
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"

// His attacks run quicker than the bosses' (the user, 2026-10-16: "a bit faster for ganon overall").
#define MSGD_SPEED 1.8f
// The light ball (the user, 2026-10-17: slightly faster than the 14-frame charge): the boss's charge in 12 frames, his
// throw at 2.5 times his pace (~0.8 s in all). B held all through the charge: the big magic instead.
#define MSGD_LIGHT_BALL_CHARGE_SPEED 2.1f
#define MSGD_LIGHT_BALL_THROW_SPEED 2.5f
#define MSGD_LIGHT_BALL_CHARGE_FRAMES 12
// Holding B (the user, 2026-10-17): after half a second he aims in first person, as the Deku aims its bubble (the ball
// goes where he looks when B is let go); held on to two seconds, he leaves first person for the big magic.
#define MSGD_AIM_HOLD 10
#define MSGD_BIG_MAGIC_HOLD 70 // (the user, 2026-10-16: "let the aim be a little bit longer")
// The light ball's charge (the user, 2026-10-16): held this long it hits harder (z_ms_ganon_magic.c sTiers)
#define MSGD_TIER2_HOLD 30
// The big magic (the user, 2026-10-16): no firing by itself; held up to ten seconds, its damage from one second (the
// least) to ten (the most) -- z_ms_ganon_magic.c, its `level` in seconds.
#define MSGD_BIG_MAGIC_MAX_FRAMES (10 * 20)
static s32 sMsGdBigCharge;
#define MSGD_TIER3_HOLD 50
#define MSGD_PHANTOM_SPEED 1.3f
#define MSGD_BEAST_SPEED 2.2f // (the user, 2026-10-16: his attacks "too slow")
// Beast Ganon's charge (the user, 2026-10-16): held attack, or his roll, revs him up in place; let go, he charges.
#define MSGD_BEAST_REV_HOLD 10     // B held this long into a swing: the rev
#define MSGD_BEAST_REV_FULL 40     // frames of revving for the strongest charge
#define MSGD_BEAST_CHARGE_SPEED 16.0f
// The big magic's price (the user, 2026-10-16: "have his charged energy blast cost magic"): Din's Fire's.
#define MSGD_BIG_MAGIC_COST 12

void MergeShip_StartTransformation(PlayState* play, Player* this, u8 targetForm); // MergeShipPlayerTransform.inc

typedef struct {
    const char* anim; // the boss animation on him
    f32 frame;
    f32 prevFrame;
    f32 speed;
    f32 end;
    u8 loop;
    u8 done;
    u8 started;
    u8 floating; // Phantom Ganon: in the air
    f32 weight;
    s16 state;
    s16 timer;
    s16 hold;
    s16 count;
    f32 baseY;
    f32 hitFrame;
    f32 savedGravity;
    MsGanonMagic* hit;
    u8 swingRight;
    u8 queued;
} MsGdAbility;

static MsGdAbility sMsGd;

static void MsGd_Play(const char* anim, f32 speed, s32 loop) {
    sMsGd.anim = anim;
    sMsGd.speed = speed;
    sMsGd.loop = loop;
    sMsGd.frame = 0.0f;
    sMsGd.prevFrame = -1.0f;
    sMsGd.done = false;
    sMsGd.started = false;
    sMsGd.end = Animation_GetLastFrame((void*)anim);
}

static void MsGd_Step(void) {
    if (!sMsGd.started) {
        sMsGd.started = true;
        sMsGd.prevFrame = -1.0f;
        return;
    }
    sMsGd.prevFrame = sMsGd.frame;
    sMsGd.frame += sMsGd.speed;
    if (sMsGd.frame >= sMsGd.end) {
        if (sMsGd.loop && (sMsGd.end > 0.0f)) {
            sMsGd.frame -= sMsGd.end;
            sMsGd.prevFrame -= sMsGd.end;
        } else {
            sMsGd.frame = sMsGd.end;
            sMsGd.done = true;
        }
    }
}

static s32 MsGd_OnFrame(f32 frame) {
    return (sMsGd.prevFrame < frame) && (frame <= sMsGd.frame);
}

static void MsGd_Pose(void) {
    Math_StepToF(&sMsGd.weight, 1.0f, 0.34f);
    MergeShip_Ganondorf_SetPose(sMsGd.anim, sMsGd.frame, sMsGd.weight);
}

static void MsGd_Sfx(Player* this, u16 sfxId) {
    Audio_PlayActorSound2(&this->actor, sfxId);
}

// What he aims at: the actor he is locked on to, else straight ahead of him at `from`'s height.
static void MsGd_AimPoint(Player* this, Vec3f* from, Vec3f* out, f32 dist) {
    if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
        *out = this->focusActor->focus.pos;
    } else {
        out->x = from->x + Math_SinS(this->actor.shape.rot.y) * dist;
        out->y = from->y;
        out->z = from->z + Math_CosS(this->actor.shape.rot.y) * dist;
    }
}

static MsGanonMagic* MsGd_Spawn(PlayState* play, Player* this, s16 type, Vec3f* pos, Vec3f* aim) {
    Actor* magic;
    s16 yaw = this->actor.shape.rot.y;
    s16 pitch = 0;

    MergeShip_Ganondorf_EnsureActorsRegistered();
    if (gMergeShipActorId_MS_Ganon_Magic < 0) {
        return NULL;
    }
    if (aim != NULL) {
        yaw = Math_Vec3f_Yaw(pos, aim);
        pitch = -Math_Vec3f_Pitch(pos, aim);
    }
    magic = Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MS_Ganon_Magic, pos->x, pos->y, pos->z, pitch, yaw, 0,
                        type);
    if (magic == NULL) {
        return NULL;
    }
    magic->parent = &this->actor;
    magic->child = ((this->focusActor != NULL) && (this->focusActor->update != NULL)) ? this->focusActor : NULL;
    if (aim != NULL) {
        ((MsGanonMagic*)magic)->targetPos = *aim;
    }
    return (MsGanonMagic*)magic;
}

// A hitbox on him (Beast Ganon's swords and roar, Phantom Ganon's charge).
static MsGanonMagic* MsGd_SpawnHit(PlayState* play, Player* this, f32 ahead, s16 radius, s16 height, u32 dmgFlags,
                                   u8 damage, s16 frames) {
    MsGanonMagic* hit = MsGd_Spawn(play, this, MS_GANON_MAGIC_HIT, &this->actor.world.pos, NULL);

    if (hit != NULL) {
        hit->stepScale = ahead;
        hit->timer = frames;
        hit->collider.dim.radius = radius;
        hit->collider.dim.height = height;
        hit->collider.dim.yShift = 0;
        hit->collider.info.toucher.dmgFlags = dmgFlags;
        hit->collider.info.toucher.damage = damage;
    }
    return hit;
}

// He turns to what he is locked on to, or with the stick.
static void MsGd_Turn(Player* this, PlayState* play, s16 step) {
    f32 speed;
    s16 yaw;

    if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
        yaw = Actor_WorldYawTowardActor(&this->actor, this->focusActor);
    } else if (Player_GetMovementSpeedAndYaw(this, &speed, &yaw, SPEED_MODE_LINEAR, play) && (speed > 0.0f)) {
    } else {
        return;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.y, yaw, step);
    this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
}

static void MsGd_ClearFx(void) {
    gMsGanondorfFx.handBallScale = 0.0f;
    gMsGanondorfFx.bigCircle = gMsGanondorfFx.bigCircleAlpha = 0.0f;
    gMsGanondorfFx.bigFleck = gMsGanondorfFx.bigFleckAlpha = 0.0f;
    gMsGanondorfFx.bigCore = 0.0f;
    gMsGanondorfFx.bigRays = 0;
    gMsGanondorfFx.barrier = false;
}

static void MsGd_Begin(PlayState* play, Player* this, PlayerActionFunc actionFunc) {
    Player_SetupAction(play, this, actionFunc, 0);
    Player_AnimPlayLoop(play, this, Player_GetIdleAnim(this));
    memset(&sMsGd, 0, sizeof(sMsGd));
    sMsGd.savedGravity = this->actor.gravity;
    sMsGd.baseY = this->actor.world.pos.y;
    this->linearVelocity = 0.0f;
    this->unk_6AE_rotFlags = 0;
    this->upperLimbRot.x = this->upperLimbRot.y = this->upperLimbRot.z = 0;
}

// The ability is over: back to standing (or falling, in the air).
static void MsGd_End(PlayState* play, Player* this) {
    MsGd_ClearFx();
    MergeShip_Ganondorf_CapeBlock(3); // (his standing cape, whatever ended it)
    sMsGd.hit = NULL; // (a hitbox ends by itself)
    sMsGd.anim = NULL;
    this->actor.shape.rot.x = 0;
    this->actor.gravity = sMsGd.savedGravity;
    MergeShip_Ganondorf_BeastAnim(NULL, 1.0f, false, 0.0f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        func_8083C148(this, play);
    } else {
        Player_SetupAction(play, this, Player_Action_8084411C, 1);
        Player_AnimPlayOnce(play, this, &gPlayerAnim_link_normal_fall);
        this->actor.velocity.y = MIN(this->actor.velocity.y, 0.0f);
    }
}

// Link's own hurt/fall handling stays: a hit, a ledge or water ends the ability.
static s32 MsGd_Interrupted(PlayState* play, Player* this) {
    if ((this->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE)) ||
        (this->invincibilityTimer > 0) || !MergeShip_Ganondorf_IsPlayer(this)) {
        this->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
        MsGd_End(play, this);
        return true;
    }
    return false;
}

// ---- Ganondorf: the light ball and the big magic -----------------------------------------------------------------------

void MsGd_Action_LightBall(Player* this, PlayState* play) {
    MsGanondorfFx* fx = &gMsGanondorfFx;
    Vec3f pos;
    Vec3f aim;
    s32 held = CHECK_BTN_ALL(sControlInput->cur.button, BTN_B);
    s32 i;

    LinkAnimation_Update(play, &this->skelAnime);
    this->linearVelocity = 0.0f;
    if (MsGd_Interrupted(play, this)) {
        return;
    }
    MsGd_Turn(this, play, 0x7D0);
    MsGd_Step();
    gMsGanonMagicChargePos.x = gMsGanondorfChestPos.x;
    gMsGanonMagicChargePos.y = gMsGanondorfChestPos.y + 50.0f;
    gMsGanonMagicChargePos.z = gMsGanondorfChestPos.z;

    switch (sMsGd.state) {
        case 0: // BossGanon_ChargeLightBall, in a few frames: the spark, the light gathering in his hand
            sMsGd.timer--;
            sMsGd.hold = held ? (sMsGd.hold + 1) : 0;
            if (sMsGd.timer == 8) {
                MsGd_Sfx(this, NA_SE_EN_GANON_SPARK);
            }
            if (sMsGd.timer < 7) {
                fx->handBallSpin += Rand_ZeroFloat(M_PI / 2) + (M_PI / 2);
                Math_ApproachF(&fx->handBallScale, 10.0f, 0.5f, 1.8f);
            }
            if (sMsGd.hold >= MSGD_AIM_HOLD) {
                // held half a second: he aims (first person), or locked on, holds the light and keeps charging
                if ((this->focusActor == NULL) || (this->focusActor->update == NULL)) {
                    this->actor.focus.rot.x = 0;
                    this->actor.focus.rot.y = this->actor.shape.rot.y;
                    this->stateFlags1 |= PLAYER_STATE1_FIRST_PERSON;
                }
                sMsGd.state = 5;
                break;
            }
            if (sMsGd.timer <= 0) {
                MsGd_Play(gGanondorfThrowAnim, MSGD_LIGHT_BALL_THROW_SPEED, false); // BossGanon_SetupPlayTennis
                sMsGd.state = 1;
            }
            break;

        case 5: { // holding B: aiming in first person (the Deku's bubble), or locked on
            Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);
            s32 aiming = (this->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) != 0;

            sMsGd.hold = held ? (sMsGd.hold + 1) : sMsGd.hold;
            fx->handBallSpin += Rand_ZeroFloat(M_PI / 2) + (M_PI / 2);
            // the charge climbing: the ball grows, and each step sounds
            if ((sMsGd.hold == MSGD_TIER2_HOLD) || (sMsGd.hold == MSGD_TIER3_HOLD)) {
                MsGd_Sfx(this, NA_SE_EN_GANON_SPARK);
                MsGd_Sfx(this, NA_SE_PL_SWORD_CHARGE);
            }
            Math_ApproachF(&fx->handBallScale,
                           (sMsGd.hold >= MSGD_TIER3_HOLD) ? 17.0f : (sMsGd.hold >= MSGD_TIER2_HOLD) ? 13.5f : 10.0f,
                           0.5f, 1.8f);
            if (aiming) {
                // Player's first-person look (the C-up view's: the stick, its inversion, the right stick, gyro)
                Camera_RequestMode(cam, CAM_MODE_FIRST_PERSON);
                this->actor.shape.rot.y = func_8084ABD8(play, this, 0, 0);
                this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
            }
            if (!held) {
                // let go: the ball, where he looks (along the camera's sight) or at what he is locked on to
                Vec3f from = gMsGanondorfHandPos;

                if (aiming && (cam != NULL)) {
                    f32 dx = cam->at.x - cam->eye.x;
                    f32 dy = cam->at.y - cam->eye.y;
                    f32 dz = cam->at.z - cam->eye.z;
                    f32 len = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

                    if (len < 1.0f) {
                        len = 1.0f;
                    }
                    from = cam->eye;
                    from.y -= 10.0f;
                    aim.x = cam->eye.x + dx / len * 1000.0f;
                    aim.y = cam->eye.y + dy / len * 1000.0f;
                    aim.z = cam->eye.z + dz / len * 1000.0f;
                } else {
                    MsGd_AimPoint(this, &from, &aim, 500.0f);
                }
                MsGd_Sfx(this, NA_SE_EN_GANON_THROW);
                MsGd_Sfx(this, NA_SE_EN_GANON_THROW_MASIC);
                gMsGanonMagicChargeLevel = (sMsGd.hold >= MSGD_TIER3_HOLD)   ? 3
                                           : (sMsGd.hold >= MSGD_TIER2_HOLD) ? 2
                                                                             : 1;
                MsGd_Spawn(play, this, MS_GANON_MAGIC_LIGHT_BALL, &from, &aim);
                gMsGanonMagicChargeLevel = 0;
                fx->handBallScale = 0.0f;
                this->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
                // the camera back behind him: he is seen finishing his throw (BossGanon_PlayTennis from the ball's
                // frame on; the user, 2026-10-16)
                MsGd_Play(gGanondorfThrowAnim, MSGD_LIGHT_BALL_THROW_SPEED, false);
                sMsGd.frame = 11.0f;
                sMsGd.weight = 1.0f;
                sMsGd.state = 7;
                break;
            }
            if (sMsGd.hold >= MSGD_BIG_MAGIC_HOLD) {
                // held on: out of first person, BossGanon_SetupChargeBigMagic (if he has the magic for it)
                this->stateFlags1 &= ~PLAYER_STATE1_FIRST_PERSON;
                Camera_RequestMode(cam, CAM_MODE_NORMAL);
                if (gSaveContext.magic < MSGD_BIG_MAGIC_COST) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                    sMsGd.hold = MSGD_AIM_HOLD; // (keeps aiming the light ball)
                    break;
                }
                Magic_RequestChange(play, MSGD_BIG_MAGIC_COST, MAGIC_CONSUME_NOW);
                fx->handBallScale = 0.0f;
                fx->bigSeed = (s16)Rand_ZeroFloat(20000.0f);
                fx->bigRays = 0;
                for (i = 0; i < 10; i++) {
                    fx->bigRayAlpha[i] = 0.0f;
                }
                MsGd_Play(gGanondorfBigMagicChargeStartAnim, MSGD_SPEED, false);
                sMsGd.state = 10;
            }
            break;
        }

        case 7: // after a throw from first person: the rest of the throw
            if (sMsGd.done) {
                MsGd_End(play, this);
                return;
            }
            break;

        case 1: // BossGanon_PlayTennis: the ball goes at frame 11
            Math_ApproachZeroF(&fx->handBallScale, 1.0f, 0.2f);
            if (MsGd_OnFrame(11.0f)) {
                MsGd_Sfx(this, NA_SE_EN_GANON_THROW);
                MsGd_Sfx(this, NA_SE_EN_GANON_THROW_MASIC);
                pos = gMsGanondorfHandPos;
                MsGd_AimPoint(this, &pos, &aim, 500.0f);
                MsGd_Spawn(play, this, MS_GANON_MAGIC_LIGHT_BALL, &pos, &aim);
            }
            if (MsGd_OnFrame(12.0f)) {
                fx->handBallScale = 0.0f;
            }
            if (sMsGd.done) {
                MsGd_End(play, this);
                return;
            }
            break;

        case 10: // ChargeStart
            if (sMsGd.done) {
                MsGd_Play(gGanondorfBigMagicChargeHoldAnim, 1.0f, true);
                sMsGd.state = 11;
                sMsGd.timer = 100;
                sMsGdBigCharge = 0;
            }
            break;

        case 11: { // ChargeHold: BossGanon_ChargeBigMagic case 2, its timer counting down from 100 (twice as fast)
            s16 t = sMsGd.timer = MAX(sMsGd.timer - 2, 0); // (full at 0, and held there)

            if (sMsGdBigCharge < MSGD_BIG_MAGIC_MAX_FRAMES) {
                if (++sMsGdBigCharge == MSGD_BIG_MAGIC_MAX_FRAMES) {
                    MsGd_Sfx(this, NA_SE_EN_GANON_SPARK); // ten seconds: as strong as it gets
                }
            }

            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_CHARGE_MASIC - SFX_FLAG);
            Math_ApproachF(&fx->bigCircle, 0.25f, 0.1f, 0.006f);
            Math_ApproachF(&fx->bigCircleAlpha, 255.0f, 1.0f, 255.0f);
            Math_ApproachF(&fx->bigFleck, 0.25f, 0.1f, 0.006f);
            if ((t > 20) && (t < 60)) {
                Math_ApproachF(&fx->bigFleckAlpha, 255.0f, 1.0f, 15.0f);
            }
            if ((t >= 7) && (t < 26)) {
                if (fx->bigRays < 10) {
                    fx->bigRays++;
                }
                for (i = 0; i < fx->bigRays; i++) {
                    Math_ApproachF(&fx->bigRayAlpha[i], 200.0f, 1.0f, 40.0f);
                }
            }
            if (t <= 30) {
                Math_ApproachF(&fx->bigCircle, 0.4f, 0.5f, 0.017f);
                fx->bigFleck = fx->bigCircle;
                Math_ApproachF(&fx->bigCore, 45.0f, 0.1f, 10.0f);
            }
            if (t > 30) {
                Vec3f ring = { 0.0f, 0.0f, 0.0f };
                Vec3f offset;

                ring.y = Rand_ZeroFloat(10.0f) + 150.0f;
                Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
                Matrix_RotateZ(Rand_ZeroFloat(65536.0f), MTXMODE_APPLY);
                Matrix_MultVec3f(&ring, &offset);
                pos.x = gMsGanonMagicChargePos.x + offset.x;
                pos.y = gMsGanonMagicChargePos.y + offset.y;
                pos.z = gMsGanonMagicChargePos.z + offset.z;
                MsGanonMagic_SpawnBlackDot(&pos, 20.0f);
            }
            // it goes when the player lets go (never by itself)
            if (!held) {
                MsGd_Play(gGanondorfBigMagicWindupAnim, MSGD_SPEED, false);
                MsGd_Sfx(this, NA_SE_EN_GANON_DARKWAVE);
                sMsGd.state = 12;
                sMsGd.timer = 4;
                sMsGd.count = 10;
            }
            break;
        }

        case 12: // Windup
            for (i = 0; i < 10; i++) {
                Math_ApproachF(&fx->bigRayAlpha[i], 0.0f, 1.0f, 40.0f);
            }
            if (sMsGd.timer > 0) {
                sMsGd.timer--;
            } else {
                Math_ApproachZeroF(&fx->bigCircle, 1.0f, 0.08f);
                fx->bigFleck = fx->bigCircle;
                Math_ApproachZeroF(&fx->bigCore, 1.0f, 10.0f);
            }
            if (--sMsGd.count <= 0) {
                MsGd_Play(gGanondorfBigMagicThrowAnim, MSGD_SPEED, false);
                MsGd_ClearFx();
                sMsGd.state = 13;
            }
            break;

        case 13: // Throw: five balls snake out at what he is locked on to
            if (MsGd_OnFrame(5.0f)) {
                pos = gMsGanondorfChestPos;
                MsGd_AimPoint(this, &pos, &aim, 600.0f);
                // its strength: the seconds he held it, 1 to 10
                gMsGanonMagicChargeLevel = CLAMP((sMsGdBigCharge + 19) / 20, 1, 10);
                for (i = 0; i < 5; i++) {
                    MsGanonMagic* ball = MsGd_Spawn(play, this, MS_GANON_MAGIC_BIG_MAGIC, &pos, NULL);

                    if (ball != NULL) {
                        // func_808E2544 case 10: each starts off its own way (params 0x104..0x108: a fan across
                        // half a turn) and curves in on what he threw them at
                        ball->targetPos = aim;
                        ball->frame = 0;
                        ball->stepScale = 0.0f;
                        ball->actor.shape.rot.y = ball->actor.world.rot.y =
                            Math_Vec3f_Yaw(&pos, &aim) + (s16)((i - 2) * 0x2000);
                    }
                }
                gMsGanonMagicChargeLevel = 0;
                MsGd_Sfx(this, NA_SE_EN_GANON_BIGMASIC);
                MsGd_Sfx(this, NA_SE_EN_GANON_THROW_BIG);
            }
            if (sMsGd.done) {
                MsGd_Play(gGanondorfBigMagicThrowEndAnim, MSGD_SPEED, false);
                sMsGd.state = 14;
            }
            break;

        case 14:
            if (sMsGd.done) {
                MsGd_End(play, this);
                return;
            }
            break;
    }
    MsGd_Pose();
}

// ---- knockback (the user, 2026-10-17: "have ganon's slam knock enemies back too, have his axe knock enemies back") ---
//
// What his pound's shockwave or his axe strikes is thrown away from him: pushed along the floor over a few frames (its
// own movement can't hold it), a little hop, stopped by walls. Enemies only (no bosses).

#define MSGD_KNOCK_MAX 16

typedef struct {
    Actor* actor;
    Vec3f vel;
    s16 frames;
} MsGdKnock;

static MsGdKnock sMsGdKnocks[MSGD_KNOCK_MAX];

static s32 MsGd_Knocking(Actor* actor) {
    s32 i;

    for (i = 0; i < MSGD_KNOCK_MAX; i++) {
        if ((sMsGdKnocks[i].actor == actor) && (sMsGdKnocks[i].frames > 0)) {
            return true;
        }
    }
    return false;
}

static void MsGd_Knock(Player* this, Actor* actor, f32 speed, f32 hop) {
    s32 i;
    s32 slot = -1;
    s16 yaw;

    if ((actor == NULL) || (actor->update == NULL) || (actor->category != ACTORCAT_ENEMY) || MsGd_Knocking(actor)) {
        return;
    }
    for (i = 0; i < MSGD_KNOCK_MAX; i++) {
        if (sMsGdKnocks[i].frames <= 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return;
    }
    yaw = Actor_WorldYawTowardActor(&this->actor, actor);
    sMsGdKnocks[slot].actor = actor;
    sMsGdKnocks[slot].vel.x = Math_SinS(yaw) * speed;
    sMsGdKnocks[slot].vel.y = 0.0f;
    sMsGdKnocks[slot].vel.z = Math_CosS(yaw) * speed;
    sMsGdKnocks[slot].frames = 10;
    actor->velocity.y = MAX(actor->velocity.y, hop);
}

// What of the enemies is within `radius` (and `height` above or below) of `at`.
static void MsGd_KnockAround(PlayState* play, Player* this, Vec3f* at, f32 radius, f32 height, f32 speed, f32 hop) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;

    while (actor != NULL) {
        Actor* next = actor->next;

        if ((Math_Vec3f_DistXZ(&actor->world.pos, at) < radius) && (fabsf(actor->world.pos.y - at->y) < height)) {
            MsGd_Knock(this, actor, speed, hop);
        }
        actor = next;
    }
}

// Each frame (Player_Update): the knocked are pushed, slowing, and stopped by walls.
static void MsGd_UpdateKnocks(PlayState* play) {
    s32 i;

    for (i = 0; i < MSGD_KNOCK_MAX; i++) {
        MsGdKnock* k = &sMsGdKnocks[i];
        Vec3f from;
        Vec3f to;
        Vec3f hit;
        CollisionPoly* poly;
        s32 bgId;

        if (k->frames <= 0) {
            continue;
        }
        if ((k->actor == NULL) || (k->actor->update == NULL)) {
            k->frames = 0;
            continue;
        }
        from = k->actor->world.pos;
        from.y += 20.0f;
        to = from;
        to.x += k->vel.x;
        to.z += k->vel.z;
        if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
            k->frames = 0; // (a wall)
            continue;
        }
        k->actor->world.pos.x += k->vel.x;
        k->actor->world.pos.z += k->vel.z;
        k->vel.x *= 0.8f;
        k->vel.z *= 0.8f;
        k->frames--;
    }
}

// ---- his axe's trail (as the swords' trail: Link's own trail effect, the axe's head in it) ---------------------------

static void MsGd_AxeTrail(Player* this, s32 on) {
    static s32 sTrailing;
    EffectBlure* blure = Effect_GetByIndex(this->meleeWeaponEffectIndex);

    if (blure == NULL) {
        return;
    }
    if (on && (gPlayState != NULL) && ((s32)(gPlayState->gameplayFrames - gMsGanondorfAxeHeadFrame) <= 2)) {
        EffectBlure_AddVertex(blure, &gMsGanondorfAxeTip, &gMsGanondorfAxeBase);
        sTrailing = true;
    } else if (sTrailing) {
        EffectBlure_AddSpace(blure);
        sTrailing = false;
    }
}

// ---- Ganondorf: the block and the floor pound ---------------------------------------------------------------------------

static void MsGd_PoundImpactEffects(PlayState* play, Player* this) {
    Vec3f pos = this->actor.world.pos;
    Vec3f vel;
    s32 i;

    // BossGanon_PoundFloor case 1 at the floor
    Actor_RequestQuakeAndRumble(&this->actor, play, 10, 20);
    Player_RequestRumble(this, 255, 20, 150, 0);
    MsGd_Sfx(this, NA_SE_EN_GANON_HIT_GND_IMP);
    gMsGanondorfFx.handBallScale = 0.0f;
    for (i = 0; i < 80; i++) {
        vel.x = Rand_CenteredFloat(25.0f);
        vel.y = Rand_ZeroFloat(17.0f);
        vel.z = Rand_CenteredFloat(25.0f);
        MsGanonMagic_SpawnLightRay(&pos, &vel, &(Vec3f){ 0.0f, 0.0f, 0.0f }, Rand_ZeroFloat(300.0f) + 500.0f, 13.0f,
                                   0x1E);
    }
    MsGanonMagic_SpawnDustLight(&pos, 0.0f, 3.0f, 10);
    MsGanonMagic_SpawnShockwave(&pos, 0.0f, 3.0f);
    MsGd_Spawn(play, this, MS_GANON_MAGIC_SHOCKWAVE, &pos, NULL);
    // a bomb's blast where he lands: bombable walls, boulders, grotto covers and floors break
    MsGd_Spawn(play, this, MS_GANON_MAGIC_BLAST, &pos, NULL);
    // the shockwave throws what is around him back
    MsGd_KnockAround(play, this, &pos, 260.0f, 120.0f, 22.0f, 8.0f);
}


// ---- his guard turns projectiles back --------------------------------------------------------------------------------
//
// (The user, 2026-10-16: "have it reflect any projectiles back at whoever sends them at him".) Guarding (R), what is
// shot at him and reaches him turns and flies straight back at whoever sent it: its own look, its own update set aside
// for a flight of ours; on arriving it strikes its sender with his magic's hit (MS_GANON_MAGIC_HIT: the damage the
// sender's table rates highest, z_ms_ganon_magic.c).

#define MSGD_REFLECT_RANGE 95.0f
#define MSGD_REFLECT_SPEED 20.0f
#define MSGD_REFLECT_MAX 8

typedef struct {
    Actor* actor;
    Actor* target;
    Vec3f dir;
    s16 life;
    s32 frame; // the frame it last flew (a slot not flown last frame is free: its actor may be gone)
} MsGdReflected;

static MsGdReflected sMsGdReflected[MSGD_REFLECT_MAX];

// What counts as a projectile: Deku Scrubs' nuts, Octoroks' rocks, Phantom Ganon's and Ganondorf's balls, Twinrova's
// blasts, flame traps' flames, Anubis's fire, Skull Kid's needles, Volvagia's rocks, Arwings' lasers, flying pots,
// enemies' arrows.
static s32 MsGd_IsProjectile(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_NUTSBALL:
        case ACTOR_EN_ANUBICE_FIRE:
        case ACTOR_EN_SKJNEEDLE:
        case ACTOR_EN_VB_BALL:
        case ACTOR_EN_TUBO_TRAP:
            return true;
        case ACTOR_EN_OKUTA:
            return actor->params == 0x10;
        case ACTOR_EN_FHG_FIRE:
            return actor->params == 50; // FHGFIRE_ENERGY_BALL
        case ACTOR_BOSS_GANON:
            return (actor->params >= 0x64) && (actor->params < 0xC8); // his light ball
        case ACTOR_BOSS_TW:
            return (actor->params == 0x64) || (actor->params == 0x66) || (actor->params == 0x68) ||
                   (actor->params == 0x69); // the fire and ice blasts, the death balls
        case ACTOR_EN_HONOTRAP:
            return actor->params != 0; // the flames, not the eye
        case ACTOR_EN_CLEAR_TAG:
            return actor->params == 100; // CLEAR_TAG_LASER
        case ACTOR_EN_ARROW:
            return (actor->parent != NULL) && (actor->parent->id != ACTOR_PLAYER);
        default:
            return false;
    }
}

// Who sent it: its parent if that is an enemy, else the enemy it came from (the nearest along the way it came).
static Actor* MsGd_ReflectTarget(PlayState* play, Player* this, Actor* proj) {
    static const u8 sCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };
    Actor* best = NULL;
    f32 bestScore = 1e9f;
    Vec3f back;
    f32 len;
    s32 c;

    if ((proj->parent != NULL) && (proj->parent->update != NULL) && (proj->parent != &this->actor) &&
        ((proj->parent->category == ACTORCAT_ENEMY) || (proj->parent->category == ACTORCAT_BOSS))) {
        return proj->parent;
    }
    back.x = -proj->velocity.x;
    back.y = -proj->velocity.y;
    back.z = -proj->velocity.z;
    len = sqrtf(SQ(back.x) + SQ(back.y) + SQ(back.z));
    if (len < 0.5f) {
        back.x = proj->world.pos.x - this->actor.world.pos.x;
        back.y = 0.0f;
        back.z = proj->world.pos.z - this->actor.world.pos.z;
        len = sqrtf(SQ(back.x) + SQ(back.z));
    }
    if (len < 0.01f) {
        return NULL;
    }
    back.x /= len;
    back.y /= len;
    back.z /= len;
    for (c = 0; c < ARRAY_COUNT(sCategories); c++) {
        Actor* actor;

        for (actor = play->actorCtx.actorLists[sCategories[c]].head; actor != NULL; actor = actor->next) {
            Vec3f d;
            f32 dist;
            f32 along;

            if ((actor->update == NULL) || (actor == proj) || MsGd_IsProjectile(actor)) {
                continue;
            }
            d.x = actor->focus.pos.x - proj->world.pos.x;
            d.y = actor->focus.pos.y - proj->world.pos.y;
            d.z = actor->focus.pos.z - proj->world.pos.z;
            dist = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
            if ((dist < 1.0f) || (dist > 3000.0f)) {
                continue;
            }
            along = (d.x * back.x + d.y * back.y + d.z * back.z) / dist;
            if (along < 0.6f) {
                continue; // (not within ~50 degrees of where it came from)
            }
            if (dist * (2.0f - along) < bestScore) {
                bestScore = dist * (2.0f - along);
                best = actor;
            }
        }
    }
    return best;
}

static MsGdReflected* MsGd_ReflectedSlot(Actor* actor, PlayState* play) {
    s32 i;

    for (i = 0; i < MSGD_REFLECT_MAX; i++) {
        if ((sMsGdReflected[i].actor == actor) && (sMsGdReflected[i].frame >= (s32)play->gameplayFrames - 1)) {
            return &sMsGdReflected[i];
        }
    }
    return NULL;
}

// The reflected projectile's update: straight at its sender (following it), struck on arrival.
static void MsGd_ReflectedUpdate(Actor* thisx, PlayState* play) {
    MsGdReflected* r = MsGd_ReflectedSlot(thisx, play);
    Player* player = GET_PLAYER(play);
    Actor* target;

    if (r == NULL) {
        Actor_Kill(thisx);
        return;
    }
    r->frame = play->gameplayFrames;
    target = r->target;
    if ((target != NULL) && (target->update == NULL)) {
        target = r->target = NULL;
    }
    if (target != NULL) {
        Vec3f d;
        f32 dist;

        d.x = target->focus.pos.x - thisx->world.pos.x;
        d.y = target->focus.pos.y - thisx->world.pos.y;
        d.z = target->focus.pos.z - thisx->world.pos.z;
        dist = sqrtf(SQ(d.x) + SQ(d.y) + SQ(d.z));
        if (dist < MAX(MSGD_REFLECT_SPEED * 1.5f, target->colChkInfo.cylRadius)) {
            // there: his magic strikes the sender
            MsGanonMagic* hit = MsGd_Spawn(play, player, MS_GANON_MAGIC_HIT, &target->world.pos, NULL);

            if (hit != NULL) {
                hit->actor.parent = target;
                hit->actor.child = target;
                hit->stepScale = 0.0f;
                hit->timer = 3;
                hit->collider.dim.radius = 80;
                hit->collider.dim.height = 160;
                hit->collider.dim.yShift = -40;
                hit->collider.info.toucher.dmgFlags = DMG_ARROW_LIGHT;
                hit->collider.info.toucher.damage = 0x08;
            }
            EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &thisx->world.pos);
            Actor_Kill(thisx);
            r->actor = NULL;
            return;
        }
        // (turning to follow it)
        d.x /= dist;
        d.y /= dist;
        d.z /= dist;
        r->dir.x += (d.x - r->dir.x) * 0.5f;
        r->dir.y += (d.y - r->dir.y) * 0.5f;
        r->dir.z += (d.z - r->dir.z) * 0.5f;
    }
    thisx->velocity.x = r->dir.x * MSGD_REFLECT_SPEED;
    thisx->velocity.y = r->dir.y * MSGD_REFLECT_SPEED;
    thisx->velocity.z = r->dir.z * MSGD_REFLECT_SPEED;
    thisx->world.pos.x += thisx->velocity.x;
    thisx->world.pos.y += thisx->velocity.y;
    thisx->world.pos.z += thisx->velocity.z;
    thisx->world.rot.y = thisx->shape.rot.y = Math_Atan2S(r->dir.z, r->dir.x);
    thisx->world.rot.x = thisx->shape.rot.x = -Math_Atan2S(sqrtf(SQ(r->dir.x) + SQ(r->dir.z)), r->dir.y);
    thisx->focus.pos = thisx->world.pos;
    if (--r->life <= 0) {
        Actor_Kill(thisx);
        r->actor = NULL;
    }
}

// Guarding, each frame: what has reached him turns back. Returns 0 (nothing), else 1 + the side it came from (0 his
// left, 1 his right).
static s32 MsGd_Reflect(PlayState* play, Player* this) {
    s32 reflected = 0;
    static const u8 sCategories[] = { ACTORCAT_ENEMY, ACTORCAT_BOSS, ACTORCAT_PROP, ACTORCAT_MISC, ACTORCAT_ITEMACTION };
    Vec3f center = this->actor.world.pos;
    s32 c;

    center.y += 60.0f;
    for (c = 0; c < ARRAY_COUNT(sCategories); c++) {
        Actor* actor;

        for (actor = play->actorCtx.actorLists[sCategories[c]].head; actor != NULL; actor = actor->next) {
            MsGdReflected* r = NULL;
            Actor* target;
            s32 i;

            if ((actor->update == NULL) || (actor->update == MsGd_ReflectedUpdate) || !MsGd_IsProjectile(actor) ||
                (Math_Vec3f_DistXYZ(&center, &actor->world.pos) > MSGD_REFLECT_RANGE)) {
                continue;
            }
            for (i = 0; i < MSGD_REFLECT_MAX; i++) {
                if ((sMsGdReflected[i].actor == NULL) || (sMsGdReflected[i].frame < (s32)play->gameplayFrames - 1)) {
                    r = &sMsGdReflected[i];
                    break;
                }
            }
            if (r == NULL) {
                return reflected;
            }
            target = MsGd_ReflectTarget(play, this, actor);
            {
                // which hand swats it: the side of him it reached
                s16 side = Actor_WorldYawTowardPoint(&this->actor, &actor->world.pos) - this->actor.shape.rot.y;

                reflected = 1 + (side < 0);
            }
            r->actor = actor;
            r->target = target;
            r->frame = play->gameplayFrames;
            r->life = 90;
            if (target != NULL) {
                f32 len;

                r->dir.x = target->focus.pos.x - actor->world.pos.x;
                r->dir.y = target->focus.pos.y - actor->world.pos.y;
                r->dir.z = target->focus.pos.z - actor->world.pos.z;
                len = sqrtf(SQ(r->dir.x) + SQ(r->dir.y) + SQ(r->dir.z));
                r->dir.x /= MAX(len, 1.0f);
                r->dir.y /= MAX(len, 1.0f);
                r->dir.z /= MAX(len, 1.0f);
            } else {
                // no sender to be found: straight back out the way he faces
                r->dir.x = Math_SinS(this->actor.shape.rot.y);
                r->dir.y = 0.0f;
                r->dir.z = Math_CosS(this->actor.shape.rot.y);
                r->life = 40;
            }
            actor->update = MsGd_ReflectedUpdate;
            actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
            Audio_PlayActorSound2(actor, NA_SE_IT_SHIELD_REFLECT_MG);
            MergeShip_Ganondorf_CapeBlock(0); // his cape swept over it
            MsGd_Sfx(this, NA_SE_EV_GANON_MANTLE);
            EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &actor->world.pos);
            MergeShipTest_Log("GANONREFLECT %d params %d -> %d", actor->id, actor->params,
                              (target != NULL) ? target->id : -1);
        }
    }
    return reflected;
}

void MsGd_Action_Block(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    this->linearVelocity = 0.0f;
    if ((sMsGd.state < 20) && MsGd_Interrupted(play, this)) {
        return;
    }
    MsGd_Step();

    switch (sMsGd.state) {
        case 0: // BossGanon_Block: up while R is held
            MsGd_Turn(this, play, 0x7D0);
            gMsGanondorfFx.barrier = true;
            MergeShip_Ganondorf_CapeBlock(1);
            {
                s32 side = MsGd_Reflect(play, this);

                if (side != 0) {
                    // BossGanon_PlayTennis: he swats it back (the user, 2026-10-16: "the reflect anim")
                    MsGd_Play((side == 1) ? gGanondorfVolleyLeftAnim : gGanondorfVolleyRightAnim, 2.0f, false);
                    MsGd_Sfx(this, NA_SE_EN_GANON_THROW);
                    sMsGd.state = 2;
                    break;
                }
            }
            if (this->invincibilityTimer <= 0) {
                this->invincibilityTimer = -2;
            }
            if (CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
                // BossGanon_SetupPoundFloor
                gMsGanondorfFx.barrier = false;
                this->invincibilityTimer = 0;
                sMsGd.state = 20;
                sMsGd.timer = 18; // (BossGanon's 26 from the floor)
                sMsGd.baseY = this->actor.world.pos.y;
                MsGd_Play(gGanondorfFloatAnim, 1.0f, true);
                this->actor.gravity = 0.0f;
                this->actor.velocity.y = 0.0f;
                break;
            }
            if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
                MergeShip_Ganondorf_CapeBlock(2);
                MsGd_Play(gGanondorfBlockReleaseAnim, MSGD_SPEED, false);
                gMsGanondorfFx.barrier = false;
                this->invincibilityTimer = 0;
                MsGd_Sfx(this, NA_SE_EV_GANON_MANTLE);
                sMsGd.state = 1;
            }
            break;

        case 2: // the swat, then his guard again (still guarding: more can be turned back meanwhile)
            gMsGanondorfFx.barrier = true;
            if (this->invincibilityTimer <= 0) {
                this->invincibilityTimer = -2;
            }
            MergeShip_Ganondorf_CapeBlock(1);
            MsGd_Reflect(play, this);
            if (sMsGd.done) {
                // his guard pose, held (its last frame)
                MsGd_Play(gGanondorfBlockAnim, MSGD_SPEED, false);
                sMsGd.frame = sMsGd.end;
                sMsGd.state = 0;
            }
            break;

        case 1:
            if (sMsGd.done) {
                MergeShip_Ganondorf_CapeBlock(3);
                MsGd_End(play, this);
                return;
            }
            break;

        case 20: // PoundFloor case 0: up, the light gathering in his hand
            this->actor.gravity = 0.0f;
            sMsGd.timer--;
            if (sMsGd.timer == 4) {
                MsGd_Sfx(this, NA_SE_EN_GANON_HIT_GND);
            }
            if (sMsGd.timer < 9) {
                gMsGanondorfFx.handBallSpin += Rand_ZeroFloat(M_PI / 2) + (M_PI / 2);
                Math_ApproachF(&gMsGanondorfFx.handBallScale, 7.0f, 0.5f, 1.0f);
            }
            this->actor.velocity.y = CLAMP((sMsGd.baseY + ((sMsGd.timer < 9) ? 150.0f : 120.0f) -
                                            this->actor.world.pos.y) * 0.15f, 0.0f, 20.0f);
            if (sMsGd.timer == 9) {
                MsGd_Play(gGanondorfPoundAnim, MSGD_SPEED, false);
            }
            if (sMsGd.timer <= 0) {
                sMsGd.state = 21;
                sMsGd.timer = 40;
                this->actor.velocity.y = 0.0f;
            }
            break;

        case 21: // case 1: the dive
            this->actor.gravity = 0.0f;
            Math_ApproachF(&this->actor.velocity.y, -50.0f, 1.0f, 10.0f);
            if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (--sMsGd.timer <= 0)) {
                this->actor.velocity.y = 0.0f;
                this->actor.gravity = sMsGd.savedGravity;
                MsGd_PoundImpactEffects(play, this);
                sMsGd.state = 22;
                sMsGd.timer = 7;
                sMsGd.count = 0;
            }
            break;

        case 22: // case 2: the dust rises in three
            sMsGd.count++;
            if ((sMsGd.count == 3) || (sMsGd.count == 6)) {
                MsGanonMagic_SpawnDustLight(&this->actor.world.pos, 0.0f, 3.0f, (sMsGd.count == 3) ? 5 : 0);
            }
            if (--sMsGd.timer <= 0) {
                MsGd_Play(gGanondorfPoundEndAnim, MSGD_SPEED, false);
                sMsGd.state = 23;
            }
            break;

        case 23:
            if (sMsGd.done) {
                MsGd_Play(gGanondorfGetUp3Anim, MSGD_SPEED, false);
                MsGd_Sfx(this, NA_SE_EV_GANON_MANTLE);
                sMsGd.state = 24;
            }
            break;

        case 24:
            if (sMsGd.done) {
                MsGd_End(play, this);
                return;
            }
            break;
    }
    MsGd_Pose();
}

// ---- Phantom Ganon: the float, the energy ball, the charge -------------------------------------------------------------
//
// BossGanondrof_Neutral: he flies about 100 above the floor, bobbing, eased toward where he goes, the spear's tip
// sparking green, his hum; _Throw: the spear lit, the ball at frame 25 with MASIC and his voice; _Charge: the windup,
// then at what he is locked on to (40 up), homing the first ten frames, green light spiralling off the spear.

#define MSGD_PHANTOM_HEIGHT 100.0f

static void MsGd_PhantomStartThrow(Player* this) {
    MsGd_Play(gPhantomGanonThrowAnim, MSGD_PHANTOM_SPEED, false);
    MsGd_Sfx(this, NA_SE_EN_FANTOM_STICK);
    sMsGd.state = 1;
    sMsGd.hold = 0;
}

static void MsGd_PhantomSparks(PlayState* play, s32 count) {
    Vec3f pos;
    Vec3f vel = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, -0.08f, 0.0f };
    s32 i;

    for (i = 0; i < count; i++) {
        pos.x = Rand_CenteredFloat(20.0f) + gMsGanondorfSpearTip.x;
        pos.y = Rand_CenteredFloat(20.0f) + gMsGanondorfSpearTip.y;
        pos.z = Rand_CenteredFloat(20.0f) + gMsGanondorfSpearTip.z;
        MsGanonMagic_SpawnFhgLight(&pos, &vel, &accel, (s16)(Rand_ZeroOne() * 80.0f) + 150,
                                        FHGFLASH_LIGHTBALL_GREEN);
    }
}

void MsGd_Action_Phantom(Player* this, PlayState* play) {
    s32 heldB = CHECK_BTN_ALL(sControlInput->cur.button, BTN_B);
    Vec3f pos;
    Vec3f aim;
    f32 speed = 0.0f;
    s16 yaw = this->actor.shape.rot.y;
    s32 i;

    LinkAnimation_Update(play, &this->skelAnime);
    if ((sMsGd.state < 2) && MsGd_Interrupted(play, this)) {
        return;
    }
    MsGd_Step();

    if (sMsGd.floating && (sMsGd.state < 2)) {
        // BossGanondrof_Neutral's flight
        f32 floor = MAX(this->actor.floorHeight, sMsGd.baseY - 150.0f);
        f32 goal = floor + MSGD_PHANTOM_HEIGHT;

        this->actor.gravity = 0.0f;
        this->actor.velocity.y = CLAMP((goal - this->actor.world.pos.y) * 0.1f, -10.0f, 10.0f) +
                                 0.5f * Math_SinS(play->gameplayFrames * 1500);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_FLOAT - SFX_FLAG);
        Player_GetMovementSpeedAndYaw(this, &speed, &yaw, SPEED_MODE_LINEAR, play);
        Math_StepToF(&this->linearVelocity, (sMsGd.state == 0) ? MIN(speed * 0.8f, 10.0f) : 0.0f, 1.0f);
        if (speed > 0.0f) {
            Math_ScaledStepToS(&this->yaw, yaw, 0xBB8);
        }
        if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
            Math_ScaledStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, this->focusActor),
                               0xBB8);
        } else if (speed > 0.0f) {
            Math_ScaledStepToS(&this->actor.shape.rot.y, this->yaw, 0xBB8);
        }
        if ((sMsGd.state == 0) && ((play->gameplayFrames % 2) == 0)) {
            MsGd_PhantomSparks(play, 3);
        }
    } else if (sMsGd.state < 2) {
        this->linearVelocity = 0.0f;
        MsGd_Turn(this, play, 0x7D0);
    }

    switch (sMsGd.state) {
        case 0: // floating
            if (CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
                MsGd_PhantomStartThrow(this);
            } else if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A)) {
                // BossGanondrof_SetupCharge
                MsGd_Play(gPhantomGanonChargeWindupAnim, MSGD_PHANTOM_SPEED, true);
                MsGd_Sfx(this, NA_SE_EN_FANTOM_ATTACK);
                sMsGd.state = 2;
                sMsGd.timer = 12; // (the boss's 20)
            } else if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
                MsGd_End(play, this);
                return;
            }
            break;

        case 1: // BossGanondrof_Throw (FHGFIRE_SPEAR_LIGHT on the tip until the ball goes)
            if (sMsGd.frame < 25.0f) {
                MsGd_PhantomSparks(play, 2);
            }
            // held, it stays charged on the spear (the Deku's bubble), up to three seconds
            if (heldB && (sMsGd.frame >= 20.0f) && (sMsGd.frame < 25.0f) && (sMsGd.hold < 60)) {
                sMsGd.hold++;
                sMsGd.frame = 20.0f;
                sMsGd.prevFrame = 19.9f;
            }
            if (MsGd_OnFrame(25.0f)) {
                MsGd_Sfx(this, sMsGd.floating ? NA_SE_EN_FANTOM_MASIC2 : NA_SE_EN_FANTOM_MASIC1);
                MsGd_Sfx(this, NA_SE_EN_FANTOM_VOICE);
                pos = gMsGanondorfSpearTip;
                MsGd_AimPoint(this, &pos, &aim, 500.0f);
                MsGd_Spawn(play, this, MS_GANON_MAGIC_PHANTOM_ORB, &pos, &aim);
            }
            if (sMsGd.done) {
                if (sMsGd.floating && CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
                    MsGd_Play(gPhantomGanonNeutralAnim, 1.0f, true);
                    sMsGd.state = 0;
                } else {
                    MsGd_End(play, this);
                    return;
                }
            }
            break;

        case 2: // CHARGE_WINDUP: he hangs, turning to his target
            this->actor.gravity = 0.0f;
            this->actor.velocity.y = 0.0f;
            this->linearVelocity = 0.0f;
            MsGd_Turn(this, play, 0x7D0);
            if (--sMsGd.timer <= 0) {
                MsGd_Play(gPhantomGanonChargeStartAnim, MSGD_PHANTOM_SPEED, false);
                sMsGd.state = 3;
                sMsGd.timer = 10;
                sMsGd.count = 40;
                this->linearVelocity = 0.0f;
                sMsGd.hit = MsGd_SpawnHit(play, this, 20.0f, 45, 80, DMG_SLASH_GIANT, 8, 45);
            }
            break;

        case 3: { // CHARGE_START / CHARGE_ATTACK
            Vec3f to;
            Vec3f offset;
            Vec3f base = { 0.0f, 10.0f, 0.0f };
            Vec3f vel;
            Vec3f accel;
            s16 pitch;

            if (sMsGd.done && (sMsGd.anim == gPhantomGanonChargeStartAnim)) {
                MsGd_Play(gPhantomGanonChargeAnim, MSGD_PHANTOM_SPEED, true);
            }
            pos = this->actor.world.pos;
            if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
                to = this->focusActor->world.pos;
                to.y += 40.0f;
            } else {
                to.x = pos.x + Math_SinS(this->actor.shape.rot.y) * 300.0f;
                to.y = this->actor.floorHeight + 40.0f;
                to.z = pos.z + Math_CosS(this->actor.shape.rot.y) * 300.0f;
            }
            if (sMsGd.timer > 0) {
                sMsGd.timer--;
                Math_ApproachS(&this->actor.shape.rot.y, Math_Vec3f_Yaw(&pos, &to), 5, 0x7D0);
                this->yaw = this->actor.shape.rot.y;
                pitch = -Math_Vec3f_Pitch(&pos, &to);
                sMsGd.baseY = Math_SinS(pitch); // (the dive's slope, held once the homing stops)
            }
            Math_ApproachF(&this->linearVelocity, 20.0f, 1.0f, 1.0f);
            this->actor.gravity = 0.0f;
            this->actor.velocity.y = this->linearVelocity * sMsGd.baseY;
            // (BossGanondrof_Charge leaves his body upright: only his flight points at the target)
            for (i = 0; i < 10; i++) {
                // the spiral of green light off the spear
                Matrix_Push();
                Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
                Matrix_RotateZ(BINANG_TO_RAD(sMsGd.hold), MTXMODE_APPLY);
                Matrix_MultVec3f(&base, &offset);
                Matrix_Pop();
                pos.x = gMsGanondorfSpearTip.x + offset.x;
                pos.y = gMsGanondorfSpearTip.y + offset.y;
                pos.z = gMsGanondorfSpearTip.z + offset.z;
                vel.x = offset.x * 0.5f;
                vel.y = offset.y * 0.5f;
                vel.z = offset.z * 0.5f;
                accel.x = offset.x * -0.05f;
                accel.y = offset.y * -0.05f;
                accel.z = offset.z * -0.05f;
                MsGanonMagic_SpawnFhgLight(&pos, &vel, &accel, 150, i % 7);
                sMsGd.hold += 0x1A5C;
            }
            if ((--sMsGd.count <= 0) || (Math_Vec3f_DistXYZ(&this->actor.world.pos, &to) < 60.0f) ||
                (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL))) {
                // CHARGE_FINISH
                this->linearVelocity *= 0.3f;
                this->actor.velocity.y = 0.0f;
                if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R) &&
                    !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
                    sMsGd.state = 0;
                    sMsGd.floating = true;
                    sMsGd.baseY = this->actor.floorHeight;
                    MsGd_Play(gPhantomGanonNeutralAnim, 1.0f, true);
                } else {
                    MsGd_End(play, this);
                    return;
                }
            }
            break;
        }
    }
    MsGd_Pose();
}

// ---- Beast Ganon: the swords, the guard, the roar ----------------------------------------------------------------------

static void MsGd_BeastSwing(Player* this) {
    const char* anim = sMsGd.swingRight ? gGanonRightSwordSwingAnim : gGanonLeftSwordSwingAnim;
    f32 last = Animation_GetLastFrame((void*)anim);

    // func_80900580
    MergeShip_Ganondorf_BeastAnim(anim, MSGD_BEAST_SPEED, false, -3.0f);
    sMsGd.end = last - 8.0f; // (the swing's follow-through cut short: the next comes sooner)
    sMsGd.hitFrame = (last - 15.0f) - 5.0f;
    sMsGd.queued = false;
    sMsGd.hold = 1;
    sMsGd.state = 1;
}

void MsGd_Action_Beast(Player* this, PlayState* play);
static u16 sMsGdRevButton;
static Vec3f sMsGdZero = { 0.0f, 0.0f, 0.0f };

// The rev: running in place, the ground shaking, while `button` is held.
static void MsGd_BeastStartRev(PlayState* play, Player* this, u16 button) {
    MergeShip_Ganondorf_BeastAnim(gGanonWalkAnim, 3.0f, true, -4.0f);
    MsGd_Sfx(this, NA_SE_EN_MGANON_ROAR);
    sMsGdRevButton = button;
    sMsGd.hit = NULL;
    sMsGd.count = 0;
    sMsGd.timer = 0;
    sMsGd.state = 5;
}

// Player_SetupRoll: his roll is the rev (A held), let go to charge. True: it is his.
s32 MergeShip_Ganondorf_BeastRoll(Player* this, PlayState* play) {
    if (!MergeShip_Ganondorf_IsPlayer(this) || (MergeShip_Ganondorf_Form() != MS_GANON_FORM_BEAST) ||
        (this->actor.id != ACTOR_PLAYER)) {
        return false;
    }
    MsGd_Begin(play, this, MsGd_Action_Beast);
    MsGd_BeastStartRev(play, this, BTN_A);
    return true;
}

void MsGd_Action_Beast(Player* this, PlayState* play) {
    f32 frame;
    f32 speed;
    s16 yaw;

    LinkAnimation_Update(play, &this->skelAnime);
    if (MsGd_Interrupted(play, this)) {
        return;
    }
    frame = MergeShip_Ganondorf_BeastFrame();

    switch (sMsGd.state) {
        case 1: // func_80900650: the swing, its sword's window around the hit frame
            this->linearVelocity = 0.0f;
            MsGd_Turn(this, play, 0x400);
            if (CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
                sMsGd.queued = true;
            }
            // B held on: the rev
            sMsGd.hold = CHECK_BTN_ALL(sControlInput->cur.button, BTN_B) ? (sMsGd.hold + 1) : 0;
            if (sMsGd.hold >= MSGD_BEAST_REV_HOLD) {
                MsGd_BeastStartRev(play, this, BTN_B);
                break;
            }
            if (MergeShip_Ganondorf_BeastOnFrame(sMsGd.hitFrame)) {
                MsGd_Sfx(this, NA_SE_EN_MGANON_SWORD);
                MsGd_Sfx(this, NA_SE_EN_MGANON_ROAR);
            }
            if ((sMsGd.hit == NULL) && (frame > (sMsGd.hitFrame - 4.0f)) && (frame < (sMsGd.hitFrame + 6.0f))) {
                sMsGd.hit = MsGd_SpawnHit(play, this, 70.0f, 75, 120, DMG_SLASH_GIANT, 8, 8);
            }
            if (MergeShip_Ganondorf_BeastOnFrame(sMsGd.end) || (frame >= sMsGd.end)) {
                sMsGd.swingRight ^= 1;
                sMsGd.hit = NULL;
                if (sMsGd.queued) {
                    MsGd_BeastSwing(this);
                } else {
                    MsGd_End(play, this);
                    return;
                }
            }
            break;

        case 2: // guard: stance, guard walk with the stick
            if (this->invincibilityTimer <= 0) {
                this->invincibilityTimer = -2;
            }
            Player_GetMovementSpeedAndYaw(this, &speed, &yaw, SPEED_MODE_LINEAR, play);
            Math_StepToF(&this->linearVelocity, MIN(speed * 0.3f, 2.5f), 0.5f);
            if (speed > 0.0f) {
                this->yaw = yaw;
            }
            if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
                Math_ScaledStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, this->focusActor),
                                   0x600);
            } else if (speed > 0.0f) {
                Math_ScaledStepToS(&this->actor.shape.rot.y, yaw, 0x600);
            }
            if ((this->linearVelocity > 0.5f) != (sMsGd.count != 0)) {
                sMsGd.count = (this->linearVelocity > 0.5f);
                MergeShip_Ganondorf_BeastAnim(sMsGd.count ? gGanonGuardWalkAnim : gGanonGuardIdleAnim, 1.0f, true,
                                              -4.0f);
            }
            if (CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
                // the roar
                this->invincibilityTimer = 0;
                this->linearVelocity = 0.0f;
                MergeShip_Ganondorf_BeastAnim(gGanonRoarAnim, 1.0f, false, -5.0f);
                MsGd_Sfx(this, NA_SE_EN_MGANON_ROAR);
                sMsGd.end = Animation_GetLastFrame((void*)gGanonRoarAnim);
                sMsGd.state = 3;
                sMsGd.timer = 0;
            } else if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
                this->invincibilityTimer = 0;
                MsGd_End(play, this);
                return;
            }
            break;

        case 5: { // the rev: in place, faster and harder the longer it is held
            Player_GetMovementSpeedAndYaw(this, &speed, &yaw, SPEED_MODE_LINEAR, play);
            this->linearVelocity = 0.0f;
            if ((this->focusActor != NULL) && (this->focusActor->update != NULL)) {
                Math_ScaledStepToS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, this->focusActor),
                                   0x800);
            } else if (speed > 0.0f) {
                Math_ScaledStepToS(&this->actor.shape.rot.y, yaw, 0x800);
            }
            this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
            if (sMsGd.count < MSGD_BEAST_REV_FULL) {
                sMsGd.count++;
            }
            if ((++sMsGd.timer % 4) == 0) {
                // his feet churning the ground
                Vec3f dust = this->actor.world.pos;

                dust.x -= Math_SinS(this->actor.shape.rot.y) * 40.0f;
                dust.z -= Math_CosS(this->actor.shape.rot.y) * 40.0f;
                func_800286CC(play, &dust, &sMsGdZero, &sMsGdZero, 200 + sMsGd.count * 5, 30);
                MsGd_Sfx(this, NA_SE_EN_MGANON_WALK);
                Actor_RequestQuakeAndRumble(&this->actor, play, 1, 4);
            }
            if (sMsGd.count == MSGD_BEAST_REV_FULL) {
                if (sMsGd.timer % 20 == 0) {
                    MsGd_Sfx(this, NA_SE_EN_MGANON_UNARI);
                }
            }
            if (!CHECK_BTN_ALL(sControlInput->cur.button, sMsGdRevButton)) {
                // let go: he charges, as long and as hard as he revved
                f32 power = (f32)sMsGd.count / MSGD_BEAST_REV_FULL;

                MergeShip_Ganondorf_BeastAnim(gGanonWalkAnim, 4.0f, true, -2.0f);
                MsGd_Sfx(this, NA_SE_EN_MGANON_ROAR);
                sMsGd.timer = 12 + (s16)(power * 18.0f);
                sMsGd.hit = MsGd_SpawnHit(play, this, 60.0f, 90, 130, DMG_HAMMER_SWING, 4 + (u8)(power * 8.0f),
                                          sMsGd.timer);
                this->linearVelocity = MSGD_BEAST_CHARGE_SPEED * (0.6f + 0.4f * power);
                sMsGd.state = 6;
            }
            break;
        }

        case 6: // the charge: straight on, until it runs out or a wall stops him
            this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
            if ((sMsGd.timer % 3) == 0) {
                MsGd_Sfx(this, NA_SE_EN_MGANON_WALK);
                func_800286CC(play, &this->actor.world.pos, &sMsGdZero, &sMsGdZero, 250, 30);
            }
            if ((--sMsGd.timer <= 0) || (this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
                if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
                    Actor_RequestQuakeAndRumble(&this->actor, play, 8, 15);
                    MsGd_Sfx(this, NA_SE_EN_GANON_HIT_GND);
                }
                this->linearVelocity = 0.0f;
                MsGd_End(play, this);
                return;
            }
            break;

        case 3: // the roar: what is around him is stunned
            this->linearVelocity = 0.0f;
            if (++sMsGd.timer == 10) {
                MsGd_SpawnHit(play, this, 0.0f, 250, 150, DMG_DEKU_NUT, 0, 5);
                Actor_RequestQuakeAndRumble(&this->actor, play, 6, 15);
            }
            if (frame >= sMsGd.end) {
                MsGd_End(play, this);
                return;
            }
            break;
    }
}

// func_8083AA10: in the air, these are not a fall (the pound, the float, the charge, an attack off a ledge).
s32 MergeShip_Ganondorf_IsAirAction(Player* this) {
    return (this->actionFunc == MsGd_Action_Block) || (this->actionFunc == MsGd_Action_Phantom) ||
           (this->actionFunc == MsGd_Action_LightBall) || (this->actionFunc == MsGd_Action_Beast);
}

// ---- starting them ----------------------------------------------------------------------------------------------------

// Player_TryActionHandlerList, before Link's handlers: his B and R. True: an ability started (or B put an item away).
// ---- the Iron Knuckle's axe (the Biggoron Sword's place; MergeShip_Ganondorf_AxeOut) ------------------------------------
//
// The user, 2026-10-17: his equipment's Biggoron Sword is the Iron Knuckle's axe, swung with the Iron Knuckle's own
// animations (object_ik, on the tower's rig: tools/ik_retarget.py) and its timings (EnIk): B, its overhead blow
// (func_80A74E2C: the axe slams the ground, sticks, is pulled out); B again while it recovers, or B on the move, its
// sweep and back-swing (func_80A751C8, its eased speed). It hurts what the Giant's Knife hurts (the Iron Knuckles too:
// their own table's Giant's Knife rows) and breaks the Spirit Temple's pillars and thrones as an Iron Knuckle's swing
// does (BgJyaIronobj). Walking and running with it out: the Iron Knuckle's walk and run.

#include "objects/object_ik/object_ik.h"

s32 BgJyaIronobj_BreakByAxe(Actor* thisx, PlayState* play, Actor* attacker, s32 attackType); // z_bg_jya_ironobj.c

#define MSGD_AXE_VERTICAL_SPEED 1.5f   // (EnIk: its overhead blow's and both recoveries' play speed)
#define MSGD_AXE_HIT_AHEAD 70.0f       // (EnIk: where the blow lands, 70 ahead)

// The axe's blade (the user, 2026-10-17: "the attacks dont land properly. They hit before the axe collides with the
// enemy"): its hitbox rides the axe head where it was drawn (gMsGanondorfAxeHead), live through the Iron Knuckle's own
// swing windows only -- as EnIk's axe collider follows its blade.
#define MSGD_AXE_HIT_RADIUS 38

static s32 MsGd_AxeHead(PlayState* play, Vec3f* out) {
    if ((s32)(play->gameplayFrames - gMsGanondorfAxeHeadFrame) > 2) {
        return false; // (not drawn lately)
    }
    *out = gMsGanondorfAxeHead;
    return true;
}

// The blade's hitbox this frame: made as the window opens, kept on the blade while it is open.
static void MsGd_AxeHit(PlayState* play, Player* this, u32 dmgFlags, u8 damage) {
    Vec3f head;

    if (!MsGd_AxeHead(play, &head)) {
        return;
    }
    if (sMsGd.hit == NULL) {
        sMsGd.hit = MsGd_SpawnHit(play, this, -1.0f, MSGD_AXE_HIT_RADIUS, 2 * MSGD_AXE_HIT_RADIUS, dmgFlags, damage, 3);
        if (sMsGd.hit == NULL) {
            return;
        }
        sMsGd.hit->collider.dim.yShift = -MSGD_AXE_HIT_RADIUS;
    }
    sMsGd.hit->targetPos = head;
    sMsGd.hit->actor.world.pos = head;
    sMsGd.hit->timer = 3; // (open while the window is)
    // what the blade reaches is thrown back (the overhead blow harder)
    MsGd_KnockAround(play, this, &head, MSGD_AXE_HIT_RADIUS + 30.0f, 80.0f, (dmgFlags == DMG_JUMP_GIANT) ? 20.0f : 16.0f,
                     6.0f);
}

// The window closed: the blade's hitbox with it (not a frame later).
static void MsGd_AxeHitEnd(void) {
    if (sMsGd.hit != NULL) {
        Actor_Kill(&sMsGd.hit->actor);
        sMsGd.hit = NULL;
    }
}

// What the blade reaches of the Spirit Temple's pillars and thrones breaks (EnIk's axe collider hitting their cylinder).
static void MsGd_AxeBreakProps(PlayState* play, Player* this, f32 reach, s32 attackType) {
    Actor* prop = play->actorCtx.actorLists[ACTORCAT_PROP].head;
    Vec3f at;

    if (!MsGd_AxeHead(play, &at)) {
        return;
    }
    while (prop != NULL) {
        Actor* next = prop->next;

        // (their cylinder: 30 round, 150 tall from their base)
        if ((prop->id == ACTOR_BG_JYA_IRONOBJ) && (Math_Vec3f_DistXZ(&prop->world.pos, &at) < reach + 30.0f) &&
            (at.y > prop->world.pos.y - reach) && (at.y < prop->world.pos.y + 150.0f + reach)) {
            BgJyaIronobj_BreakByAxe(prop, play, &this->actor, attackType);
        }
        prop = next;
    }
}

// B (the user, 2026-10-17): a tap, the overhead blow; held, the sweep. Which is told in MSGD_AXE_HOLD frames, in his
// stance (the Iron Knuckle's walk's first frame).
#define MSGD_AXE_HOLD 6

static void MsGd_AxeDecide(void) {
    MsGd_Play(gIronKnuckleWalkAnim, 0.0f, true);
    sMsGd.state = 0;
    sMsGd.timer = 0;
    sMsGd.hit = NULL;
}

static void MsGd_AxeStartSweep(Player* this) {
    MsGd_Play(gIronKnuckleHorizontalAttackAnim, 0.0f, false);
    sMsGd.state = 4;
    sMsGd.count = 0;     // (EnIk unk_300: the sweep's eased speed)
    sMsGd.hit = NULL;
    sMsGd.queued = false;
}

void MsGd_Action_Axe(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    this->linearVelocity = 0.0f;
    if (MsGd_Interrupted(play, this)) {
        return;
    }
    if (((sMsGd.state == 3) || (sMsGd.state == 5)) && CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
        MsGd_AxeDecide(); // (the next blow as it recovers)
    }
    if (sMsGd.state == 0) {
        // a tap or a hold
        if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_B)) {
            MsGd_Play(gIronKnuckleVerticalAttackAnim, MSGD_AXE_VERTICAL_SPEED, false);
            sMsGd.state = 1;
        } else if (++sMsGd.timer >= MSGD_AXE_HOLD) {
            MsGd_AxeStartSweep(this);
        }
    }
    if (sMsGd.state == 4) {
        // func_80A75260: its speed eased, |sin| of a turning angle
        sMsGd.count += 0x1C2;
        sMsGd.speed = fabsf(Math_SinS(sMsGd.count));
    }
    MsGd_Step();
    // the trail: through the overhead blow's fall, the sweep and its back-swing
    MsGd_AxeTrail(this, ((sMsGd.state == 1) && (sMsGd.frame > 12.0f) && (sMsGd.frame < 23.0f)) ||
                            ((sMsGd.state == 4) && (sMsGd.frame > 0.5f) && (sMsGd.frame < 19.0f)));

    switch (sMsGd.state) {
        case 1: // func_80A74EBC: the overhead blow
            if (sMsGd.frame < 10.0f) {
                MsGd_Turn(this, play, 0x5DC);
            }
            if (MsGd_OnFrame(15.0f)) {
                MsGd_Sfx(this, NA_SE_EN_IRONNACK_SWING_AXE);
            }
            if ((sMsGd.frame > 17.0f) && (sMsGd.frame < 23.0f)) {
                MsGd_AxeHit(play, this, DMG_JUMP_GIANT, 8);
                MsGd_AxeBreakProps(play, this, MSGD_AXE_HIT_RADIUS, 1);
            } else {
                MsGd_AxeHitEnd();
            }
            if (MsGd_OnFrame(21.0f)) {
                Vec3f slam;

                slam.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y + 0x6A4) * MSGD_AXE_HIT_AHEAD;
                slam.y = this->actor.world.pos.y;
                slam.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y + 0x6A4) * MSGD_AXE_HIT_AHEAD;
                MsGd_Sfx(this, NA_SE_EN_IRONNACK_HIT_GND);
                Camera_AddQuake(&play->mainCamera, 2, 0x19, 5);
                Player_RequestRumble(this, 255, 20, 150, 0);
                if (MsGd_AxeHead(play, &slam)) {
                    slam.y = this->actor.world.pos.y; // (where the blade meets the ground)
                }
                CollisionCheck_SpawnShieldParticles(play, &slam);
            }
            if (sMsGd.done) {
                // func_80A7506C: stuck in the ground, pulled out
                MsGd_Play(gIronKnuckleAxeStuckAnim, 1.0f, true);
                sMsGd.timer = (s16)Animation_GetLastFrame((void*)gIronKnuckleAxeStuckAnim);
                sMsGd.state = 2;
                sMsGd.hit = NULL;
                MsGd_Sfx(this, NA_SE_EN_IRONNACK_PULLOUT);
            }
            break;

        case 2: // func_80A7510C
            if (--sMsGd.timer <= 0) {
                MsGd_Play(gIronKnuckleRecoverFromVerticalAttackAnim, MSGD_AXE_VERTICAL_SPEED, false);
                sMsGd.state = 3;
            }
            break;

        case 4: { // func_80A75260: the sweep (attack type 2), then the back-swing (3)
            s32 attackType = (sMsGd.frame > 11.0f) ? 3 : 2;
            s32 window = ((sMsGd.frame > 1.0f) && (sMsGd.frame < 9.0f)) || ((sMsGd.frame > 13.0f) && (sMsGd.frame < 18.0f));

            if (sMsGd.frame < 10.0f) {
                MsGd_Turn(this, play, 0x5DC);
            }
            if (window) {
                if (sMsGd.hold == 0) {
                    MsGd_Sfx(this, NA_SE_EN_IRONNACK_SWING_AXE);
                }
                sMsGd.hold = 1;
                MsGd_AxeHit(play, this, DMG_SLASH_GIANT, 4);
                MsGd_AxeBreakProps(play, this, MSGD_AXE_HIT_RADIUS, attackType);
            } else {
                sMsGd.hold = 0;
                MsGd_AxeHitEnd();
            }
            if (sMsGd.done) {
                // func_80A753D0
                MsGd_Play(gIronKnuckleRecoverFromHorizontalAttackAnim, MSGD_AXE_VERTICAL_SPEED, false);
                sMsGd.state = 5;
                sMsGd.hold = 0;
                sMsGd.hit = NULL;
            }
            break;
        }

        case 3: // func_80A7510C's recovery
        case 5: // func_80A7545C
            if (sMsGd.done) {
                MsGd_End(play, this);
                return;
            }
            break;
    }
    MsGd_Pose();
}

// B with the axe out: the overhead blow, or on the move the sweep.
static s32 MsGd_AxeStart(PlayState* play, Player* this) {
    MsGd_Begin(play, this, MsGd_Action_Axe);
    MsGd_AxeDecide();
    return true;
}

// Player_Update, each frame: standing, walking or running with the axe out, the Iron Knuckle's walk and run.
void MergeShip_Ganondorf_AxeLocomotion(Player* this, PlayState* play) {
    static const char* sAnim = NULL;
    static f32 sFrame = 0.0f;
    static f32 sWeight = 0.0f;
    PlayerActionFunc f = this->actionFunc;
    const char* anim;
    f32 speed;

    if (this->actor.id == ACTOR_PLAYER) {
        MsGd_UpdateKnocks(play); // (his knockbacks, each frame)
        if (this->actionFunc != MsGd_Action_Axe) {
            MsGd_AxeTrail(this, false);
        }
    }

    if (!MergeShip_Ganondorf_AxeOut(this) || (this->heldItemAction != PLAYER_IA_NONE) ||
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_WATER |
                              PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING)) ||
        !((f == Player_Action_Idle) || (f == Player_Action_80840450) || (f == Player_Action_808407CC) ||
          (f == Player_Action_80840DE4) || (f == Player_Action_808414F8) || (f == Player_Action_8084170C) ||
          (f == Player_Action_808417FC) || (f == Player_Action_8084193C) || (f == Player_Action_TurnInPlace) ||
          (f == Player_Action_80842180) || (f == Player_Action_8084227C))) {
        sWeight = 0.0f;
        return;
    }
    speed = this->linearVelocity;
    // (EnIk: it walks at 0.9 a frame, runs at 2.5, each at its animation's own pace; he strides as it does)
    anim = (speed > 4.0f) ? gIronKnuckleRunAnim : gIronKnuckleWalkAnim;
    if (anim != sAnim) {
        sAnim = anim;
        sFrame = 0.0f;
    }
    if (speed > 0.5f) {
        f32 last = Animation_GetLastFrame((void*)anim);

        sFrame += (anim == gIronKnuckleRunAnim) ? (speed / 2.5f) * 0.45f : (speed / 0.9f) * 0.35f;
        while ((last > 0.0f) && (sFrame >= last)) {
            sFrame -= last;
        }
    } else {
        sFrame = 0.0f; // standing: its stance
    }
    Math_StepToF(&sWeight, 1.0f, 0.25f);
    MergeShip_Ganondorf_SetPose(anim, sFrame, sWeight);
}

s32 MergeShip_Ganondorf_ActionHandler(Player* this, PlayState* play) {
    s32 form;
    s32 pressB;
    s32 holdR;

    if ((this->actor.id != ACTOR_PLAYER) || !MergeShip_Ganondorf_IsPlayer(this) || Player_InCsMode(play) ||
        (play->shootingGalleryStatus != 0) || (play->bombchuBowlingStatus != 0) ||
        (this->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE |
                              PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_HANGING_OFF_LEDGE |
                              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_TALKING |
                              PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_USING_BOOMERANG | PLAYER_STATE1_IN_CUTSCENE)) ||
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    pressB = CHECK_BTN_ALL(sControlInput->press.button, BTN_B);
    holdR = CHECK_BTN_ALL(sControlInput->cur.button, BTN_R);
    if (!pressB && !holdR) {
        return false;
    }
    if ((this->heldItemAction != PLAYER_IA_NONE) && pressB) {
        // an item in hand (the bow, a stick...): B puts it away, as it does for Link
        Player_UseItem(play, this, ITEM_NONE);
        return true;
    }
    form = MergeShip_Ganondorf_Form();
    if (form == MS_GANON_FORM_BEAST) {
        MsGd_Begin(play, this, MsGd_Action_Beast);
        if (pressB) {
            MsGd_BeastSwing(this);
        } else {
            MergeShip_Ganondorf_BeastAnim(gGanonGuardIdleAnim, 1.0f, true, -4.0f);
            sMsGd.state = 2;
        }
    } else if (form == MS_GANON_FORM_PHANTOM) {
        MsGd_Begin(play, this, MsGd_Action_Phantom);
        if (pressB) {
            MsGd_PhantomStartThrow(this);
        } else {
            sMsGd.floating = true;
            sMsGd.baseY = this->actor.floorHeight;
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            MsGd_Play(gPhantomGanonNeutralAnim, 1.0f, true);
            MsGd_Sfx(this, NA_SE_EN_FANTOM_LAUGH);
        }
    } else if (pressB && MergeShip_Ganondorf_AxeOut(this)) {
        return MsGd_AxeStart(play, this); // the Iron Knuckle's axe (the Biggoron Sword's place)
    } else if (pressB) {
        MsGd_Begin(play, this, MsGd_Action_LightBall);
        MsGd_Play(gGanondorfChargeLightBallAnim, MSGD_LIGHT_BALL_CHARGE_SPEED, false);
        sMsGd.timer = MSGD_LIGHT_BALL_CHARGE_FRAMES; // (BossGanon's 25)
        sMsGd.hold = 1; // (counted from the press)
    } else {
        MsGd_Begin(play, this, MsGd_Action_Block);
        MsGd_Play(gGanondorfBlockAnim, MSGD_SPEED, false);
        MergeShip_Ganondorf_CapeBlock(0); // BossGanon_SetupBlock: his cape up over his arm
        MsGd_Sfx(this, NA_SE_EV_GANON_MANTLE);
    }
    return true;
}

// ---- the masks' transformation ----------------------------------------------------------------------------------------
//
// MM's (MergeShipPlayerTransform.inc): the mask comes to his face, the light and the flash; at the flash he becomes the
// form (gMsGanondorfPendingForm). Into Beast Ganon he plays Ganondorf's own transformation (BossGanon2's cutscene:
// gGanondorfTransformStart / Loop) and Ganon roars as the white fades; Phantom Ganon puts his mask on
// (gPhantomGanonMaskOnAnim).

s32 MergeShip_Ganondorf_StartTransform(PlayState* play, Player* this, s32 form, s32 mask) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || Player_InCsMode(play) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CLIMBING_LADDER |
                              PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_CARRYING_ACTOR))) {
        return false;
    }
    MsGd_ClearFx();
    gMsGanondorfPendingForm = form;
    sMsGd.state = 0;
    sMsGd.anim = NULL;
    sMsGd.weight = 0.0f;
    // As Link's (the user, 2026-10-16: "visible masks ... like links"): the mask going on is the current one (in his
    // hand, then on his face), the one coming off the previous one (pulled off after the flash). Deku slot: Phantom
    // Ganon's; Fierce Deity slot: Ganon's.
    this->prevMask = (MergeShip_Ganondorf_Form() == MS_GANON_FORM_PHANTOM) ? PLAYER_MASK_MM_DEKU
                     : (MergeShip_Ganondorf_Form() == MS_GANON_FORM_BEAST) ? PLAYER_MASK_MM_FIERCE_DEITY
                                                                          : PLAYER_MASK_NONE;
    this->currentMask = (form == MS_GANON_FORM_PHANTOM) ? PLAYER_MASK_MM_DEKU
                        : (form == MS_GANON_FORM_BEAST) ? PLAYER_MASK_MM_FIERCE_DEITY
                                                        : PLAYER_MASK_NONE;
    MergeShip_StartTransformation(play, this, this->mergeShipForm);
    if (this->prevMask != PLAYER_MASK_NONE) {
        // taking a mask off, as MM's forms do (the user, 2026-10-16: "the proper mm revert animation like links"):
        // Phantom Ganon, on Link's skeleton, pulls at his mask as the Fierce Deity does (MM's
        // sMMTransformStartAnims[MM_FORM_FIERCE_DEITY]); after the flash Ganondorf pulls it off (cl_maskoff)
        Player_AnimChangeOnceMorphAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_pz_maskoffstart);
    }
    return true;
}

// MergeShip_Action_PutOnMask, each frame. Putting a mask on: Link's own (his hands bring the mask to his face). Taking
// it off (the user, 2026-10-16: "No revert anims"), as MM's forms each pull theirs off in their own way: Phantom Ganon
// puts his mask on backwards (gPhantomGanonMaskOnAnim, his hand takes it from his face); Ganon staggers, stunned
// (gGanonStunStartAnim, then its loop), until the flash.
static s32 sMsGdRevertBeastLoop;

// MergeShip_Action_PutOnMask: no close transformation camera for Ganon taking his mask off -- it frames a Link-sized
// player and sits inside the beast; the camera he plays with shows him whole.
static s32 MergeShip_Ganondorf_NoTransformCam(Player* this) {
    return MergeShip_Ganondorf_IsPlayer(this) && (MergeShip_Ganondorf_Form() == MS_GANON_FORM_BEAST);
}

static void MergeShip_Ganondorf_PutOnMaskFrame(Player* this, PlayState* play) {
    s32 form = MergeShip_Ganondorf_Form();

    if (!MergeShip_Ganondorf_IsPlayer(this) || (gMsGanondorfPendingForm < 0) || (gMsGanondorfPendingForm == form)) {
        return;
    }
    if (form == MS_GANON_FORM_PHANTOM) {
        // (Link's animation: MM's pz_maskoffstart, MergeShip_Ganondorf_StartTransform)
        if (this->av1.actionVar1 == 0) {
            MsGd_Sfx(this, NA_SE_EN_FANTOM_DAMAGE2);
        }
    } else if (form == MS_GANON_FORM_BEAST) {
        if (this->av1.actionVar1 == 0) {
            MergeShip_Ganondorf_BeastAnim(gGanonStunStartAnim, 1.0f, false, -4.0f);
            sMsGdRevertBeastLoop = false;
            MsGd_Sfx(this, NA_SE_EN_MGANON_DAMAGE);
        } else if (!sMsGdRevertBeastLoop &&
                   (MergeShip_Ganondorf_BeastFrame() >= Animation_GetLastFrame((void*)gGanonStunStartAnim))) {
            MergeShip_Ganondorf_BeastAnim(gGanonStunLoopAnim, 1.0f, true, -2.0f);
            sMsGdRevertBeastLoop = true;
        }
    }
}

// MergeShip_Action_PutOnMask at the flash.
static void MergeShip_Ganondorf_TransformFlash(Player* this) {
    if (gMsGanondorfPendingForm >= 0) {
        if (MergeShip_Ganondorf_Form() == MS_GANON_FORM_BEAST) {
            MergeShip_Ganondorf_BeastAnim(NULL, 1.0f, false, 0.0f); // (his stagger ends with him)
        }
        MergeShip_Ganondorf_SetForm(gMsGanondorfPendingForm);
        gMsGanondorfPendingForm = -1;
        sMsGd.anim = NULL;
        sMsGd.state = 100; // MergeShip_Ganondorf_TransformEndFrame starts the form's own
    }
}

// MergeShip_ProcessMMAnimSfxList: his voice for MM's transformation scream. True: played.
static s32 MergeShip_Ganondorf_TransformVoice(Player* this, u16 mmSfxId) {
    if ((mmSfxId != MM_NA_SE_PL_TRANSFORM_VOICE) || !MergeShip_Ganondorf_IsPlayer(this)) {
        return false;
    }
    MsGd_Sfx(this, (gMsGanondorfPendingForm == MS_GANON_FORM_PHANTOM) ? NA_SE_EN_FANTOM_TRANSFORM
                   : (gMsGanondorfPendingForm == MS_GANON_FORM_BEAST) ? NA_SE_EN_GANON_DAMAGE2
                                                                      : NA_SE_EN_GANON_LAUGH);
    return true;
}

// MergeShip_Action_TransformEnd, each frame: the new form's entrance. True while it still plays.
static s32 MergeShip_Ganondorf_TransformEndFrame(Player* this, PlayState* play) {
    s32 form = MergeShip_Ganondorf_Form();

    if (!MergeShip_Ganondorf_IsPlayer(this) || (sMsGd.state < 100)) {
        return false;
    }
    if (form == MS_GANON_FORM_BEAST) {
        if (sMsGd.state == 100) {
            MergeShip_Ganondorf_BeastAnim(gGanonRoarAnim, 1.0f, false, 0.0f);
            sMsGd.end = Animation_GetLastFrame((void*)gGanonRoarAnim);
            sMsGd.state = 101;
            sMsGd.timer = 0;
        }
        if (++sMsGd.timer == 12) {
            MsGd_Sfx(this, NA_SE_EN_MGANON_ROAR);
            Actor_RequestQuakeAndRumble(&this->actor, play, 6, 20);
        }
        if (MergeShip_Ganondorf_BeastFrame() >= sMsGd.end) {
            MergeShip_Ganondorf_BeastAnim(NULL, 1.0f, false, 0.0f);
            sMsGd.state = 0;
            return false;
        }
        return true;
    }
    if (form == MS_GANON_FORM_PHANTOM) {
        if (sMsGd.state == 100) {
            MsGd_Play(gPhantomGanonMaskOnAnim, 1.0f, false);
            sMsGd.weight = 1.0f;
            sMsGd.state = 101;
        }
        MsGd_Step();
        MsGd_Pose();
        if (MsGd_OnFrame(10.0f)) {
            MsGd_Sfx(this, NA_SE_EN_FANTOM_LAUGH);
        }
        if (sMsGd.done) {
            sMsGd.state = 0;
            sMsGd.anim = NULL;
            return false;
        }
        return true;
    }
    sMsGd.state = 0;
    return false;
}
