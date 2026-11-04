/*
 * File: z_oot_en_boom.c
 * Overlay: ovl_OoT_En_Boom
 * Description: MergeShip2 -- Ocarina of Time's Thrown Boomerang (OoT En_Boom), ported 1:1 to Majora's Mask.
 *              Actor spawns when thrown and is killed when caught.
 *
 * Source reference: Shipwright/soh/src/overlays/actors/ovl_En_Boom/z_en_boom.c (OoT). MM's own En_Boom
 * (the Zora's fins, src/overlays/actors/ovl_En_Boom) is the MM descendant of the same code and was used only
 * to map OoT API names to MM ones.
 *
 * Player handshake (OoT z_player.c func_808359FC / func_80835B60), on MM's player fields:
 *   OoT player->boomerangActor              -> MM player->zoraBoomerangActor
 *   OoT PLAYER_STATE1_BOOMERANG_THROWN (1<<25) -> MM PLAYER_STATE1_ZORA_BOOMERANG_THROWN (1<<25)
 *   The thrower sets `moveTo` and `returnTimer` right after spawning; this actor clears THROWN when caught.
 */

#include "z_oot_en_boom.h"
#include "2s2h/MergeShip/MergeShipAssets.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

// OoT's draw converts binangs to radians with the double-precision M_PI, which MM's headers do not define.
#define OOT_M_PI 3.14159265358979323846

// OoT gameplay_keep's boomerang display list, from the OoT archive mounted under "oot/".
static const ALIGN_ASSET(2) char sOoTBoomerangRefDL[] = OOT_ASSET("objects/gameplay_keep/gBoomerangRefDL");

void OoT_En_Boom_Init(Actor* thisx, PlayState* play);
void OoT_En_Boom_Destroy(Actor* thisx, PlayState* play);
void OoT_En_Boom_Update(Actor* thisx, PlayState* play);
void OoT_En_Boom_Draw(Actor* thisx, PlayState* play);

void OoTEnBoom_Fly(OoTEnBoom* this, PlayState* play);

// OoT: ACTORCAT_MISC. Kept (instead of MM's ACTORCAT_ITEMACTION used by the fins) so the actor freezes under the
// same player states it does in OoT (MM's MISC freeze mask includes PLAYER_STATE1_20000000 / talking / dead).
ActorProfile OoT_En_Boom_Profile = {
    /**/ ACTOR_OOT_EN_BOOM,
    /**/ ACTORCAT_MISC,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OoTEnBoom),
    /**/ OoT_En_Boom_Init,
    /**/ OoT_En_Boom_Destroy,
    /**/ OoT_En_Boom_Update,
    /**/ OoT_En_Boom_Draw,
};

static ColliderQuadInit sQuadInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK2,
        // OoT: { 0x00000010 (DMG_BOOMERANG), effect 0x00, damage 0x01 }.
        // MM's bit 4 is DMG_ZORA_BOOMERANG -- the same bit, and the flag every MM actor's damage table and
        // AC bumper keys its "thrown boomerang" reaction on (stun / item pickup / token grab). OoT's damage (1)
        // is kept; MM's fins use 2.
        { DMG_ZORA_BOOMERANG, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_S8(attentionRangeType, ATTENTION_RANGE_5, ICHAIN_CONTINUE), // OoT: targetMode 5
    ICHAIN_VEC3S(shape.rot, 0, ICHAIN_STOP),
};

void OoTEnBoom_SetupAction(OoTEnBoom* this, OoTEnBoomActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_En_Boom_Init(Actor* thisx, PlayState* play) {
    OoTEnBoom* this = (OoTEnBoom*)thisx;
    EffectBlureInit1 blure;

    this->actor.room = -1;

    Actor_ProcessInitChain(&this->actor, sInitChain);

    blure.p1StartColor[0] = 255;
    blure.p1StartColor[1] = 255;
    blure.p1StartColor[2] = 100;
    blure.p1StartColor[3] = 255;

    blure.p2StartColor[0] = 255;
    blure.p2StartColor[1] = 255;
    blure.p2StartColor[2] = 100;
    blure.p2StartColor[3] = 64;

    blure.p1EndColor[0] = 255;
    blure.p1EndColor[1] = 255;
    blure.p1EndColor[2] = 100;
    blure.p1EndColor[3] = 0;

    blure.p2EndColor[0] = 255;
    blure.p2EndColor[1] = 255;
    blure.p2EndColor[2] = 100;
    blure.p2EndColor[3] = 0;

    blure.elemDuration = 8;
    blure.unkFlag = 0;
    blure.calcMode = 0;
    // (SoH's `trailType = TRAIL_TYPE_BOOMERANG` is a SoH cosmetics field; MM's EffectBlureInit1 has no such field.)

    Effect_Add(play, &this->effectIndex, EFFECT_BLURE1, 0, 0, &blure);

    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sQuadInit);

    OoTEnBoom_SetupAction(this, OoTEnBoom_Fly);
}

void OoT_En_Boom_Destroy(Actor* thisx, PlayState* play) {
    OoTEnBoom* this = (OoTEnBoom*)thisx;
    Player* player = GET_PLAYER(play);

    Effect_Destroy(play, this->effectIndex);
    Collider_DestroyQuad(play, &this->collider);

    // MM-only pointer hygiene: OoT leaves player->boomerangActor dangling (only read while THROWN is set), but MM
    // code reads zoraBoomerangActor outside that state (e.g. Deku flight gates its nut drop on it being NULL).
    if ((player != NULL) && (player->zoraBoomerangActor == &this->actor)) {
        player->zoraBoomerangActor = NULL;
    }
}

/**
 * OoT func_8002F9EC: a projectile hitting a floor-type-8 ("flesh") wall. MM's func_800B90AC keeps only the check;
 * OoT also bleeds blue and plays the squish sound, so that part is done here.
 */
static s32 OoTEnBoom_CheckFleshWall(PlayState* play, Actor* actor, CollisionPoly* poly, s32 bgId, Vec3f* pos) {
    if (SurfaceType_GetFloorType(&play->colCtx, poly, bgId) == FLOOR_TYPE_8) {
        // OoT: play->roomCtx.unk_74[0] = 1; -- Jabu-Jabu's scene draw config (wall wobble). No MM equivalent.
        CollisionCheck_BlueBlood(play, NULL, pos);
        Actor_PlaySfx(actor, NA_SE_IT_WALL_HIT_BUYO);
        return true;
    }

    return false;
}

void OoTEnBoom_Fly(OoTEnBoom* this, PlayState* play) {
    Actor* target;
    Player* player;
    s32 collided;
    s16 yawTarget;
    s16 yawDiff;
    s16 pitchTarget;
    s16 pitchDiff;
    f32 distXYZScale;
    f32 distFromLink;
    s32 hitDynaID;
    Vec3f hitPoint;

    player = GET_PLAYER(play);
    target = this->moveTo;

    // If the boomerang is moving toward a targeted actor, handle setting the proper x and y angle to fly toward it.
    if (target != NULL) {
        yawTarget = Actor_WorldYawTowardPoint(&this->actor, &target->focus.pos);
        yawDiff = this->actor.world.rot.y - yawTarget;

        pitchTarget = Actor_WorldPitchTowardPoint(&this->actor, &target->focus.pos);
        pitchDiff = this->actor.world.rot.x - pitchTarget;

        distXYZScale = (200.0f - Math_Vec3f_DistXYZ(&this->actor.world.pos, &target->focus.pos)) * 0.005f;
        if (distXYZScale < 0.12f) {
            distXYZScale = 0.12f;
        }

        if ((target != &player->actor) && ((target->update == NULL) || (ABS(yawDiff) > 0x4000))) {
            //! @bug (OoT) This condition is why the boomerang will randomly fly off in a the down left direction
            //      sometimes. If the actor targetted is not Link and the difference between the 2 y angles is greater
            //      than 0x4000, the moveTo pointer is nulled and it flies off in a seemingly random direction.
            this->moveTo = NULL;
        } else {
            Math_ScaledStepToS(&this->actor.world.rot.y, yawTarget, (s16)(ABS(yawDiff) * distXYZScale));
            Math_ScaledStepToS(&this->actor.world.rot.x, pitchTarget, (s16)(ABS(pitchDiff) * distXYZScale));
        }
    }

    // Set xyz speed, move forward, and play the boomerang sound
    Actor_SetSpeeds(&this->actor, 12.0f);    // OoT func_8002D9A4 / Actor_SetProjectileSpeed
    Actor_MoveWithGravity(&this->actor);     // OoT Actor_MoveForward / Actor_MoveXZGravity
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_IT_BOOMERANG_FLY - SFX_FLAG); // OoT func_8002F974

    // If the boomerang collides with EnItem00 or a Skulltula token, set grabbed pointer to pick it up
    collided = this->collider.base.atFlags & AT_HIT;
    collided = !!(collided);
    if (collided) {
        if (((this->collider.base.at->id == ACTOR_EN_ITEM00) || (this->collider.base.at->id == ACTOR_EN_SI))) {
            this->grabbed = this->collider.base.at;
            if (this->collider.base.at->id == ACTOR_EN_SI) {
                this->collider.base.at->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
            }
        }
    }

    // Decrement the return timer and check if it's 0. If it is, check if Link can catch it and handle accordingly.
    // Otherwise handle grabbing and colliding.
    // (SoH's `|| player->boomerangQuickRecall` is a SoH enhancement, not OoT; not ported.)
    if (DECR(this->returnTimer) == 0) {
        distFromLink = Math_Vec3f_DistXYZ(&this->actor.world.pos, &player->actor.focus.pos);
        this->moveTo = &player->actor;

        // If the boomerang is less than 40 units away from Link, he can catch it.
        if (distFromLink < 40.0f) {
            target = this->grabbed;
            if (target != NULL) {
                Math_Vec3f_Copy(&target->world.pos, &player->actor.world.pos);

                // If the grabbed actor is EnItem00 (HP/Key etc) set gravity and flags so it falls in front of Link.
                // Otherwise if it's a Skulltula Token, just set flags so he collides with it to collect it.
                if (target->id == ACTOR_EN_ITEM00) {
                    target->gravity = -0.9f;
                    target->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH); // OoT: ~0x03
                } else {
                    target->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
                }
            }
            // Set player flags and kill the boomerang because Link caught it.
            // OoT: player->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN (same bit, 1 << 25).
            player->stateFlags1 &= ~PLAYER_STATE1_ZORA_BOOMERANG_THROWN;
            Actor_Kill(&this->actor);
        }
    } else {
        collided = (this->collider.base.atFlags & AT_HIT);
        collided = (!!(collided));
        if (collided) {
            // Copy the position from the prevous frame to the boomerang to start the bounce back.
            Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
        } else {
            collided = BgCheck_EntityLineTest1(&play->colCtx, &this->actor.prevPos, &this->actor.world.pos, &hitPoint,
                                               &this->actor.wallPoly, true, true, true, true, &hitDynaID);

            if (collided) {
                // OoT: if the wall is flesh, or it is a Jabu-Jabu object actor (ACTOR_BG_BDAN_OBJECTS) with params 0,
                // the boomerang goes through. Otherwise play a clank and bounce back.
                // Bg_Bdan_Objects does not exist in MM (no ported OoT id either), so only the flesh check remains.
                if (OoTEnBoom_CheckFleshWall(play, &this->actor, this->actor.wallPoly, hitDynaID, &hitPoint)) {
                    collided = false;
                } else {
                    CollisionCheck_SpawnShieldParticlesMetal(play, &hitPoint);
                }
            }
        }

        // If the boomerang needs to bounce back, set x and y angle accordingly.
        // Set timer to 0 and set return actor to player so it goes back to Link.
        if (collided) {
            this->actor.world.rot.x = -this->actor.world.rot.x;
            this->actor.world.rot.y += 0x8000;
            this->moveTo = &player->actor;
            this->returnTimer = 0;
        }
    }

    // If the actor the boomerang is holding has a null update function, set grabbed to null.
    // Otherwise, copy the position from the boomerang to the actor to move it.
    target = this->grabbed;
    if (target != NULL) {
        if (target->update == NULL) {
            this->grabbed = NULL;
        } else {
            Math_Vec3f_Copy(&target->world.pos, &this->actor.world.pos);
        }
    }
}

void OoT_En_Boom_Update(Actor* thisx, PlayState* play) {
    OoTEnBoom* this = (OoTEnBoom*)thisx;
    Player* player = GET_PLAYER(play);

    // OoT: PLAYER_STATE1_IN_CUTSCENE (1 << 29) == MM PLAYER_STATE1_20000000.
    if (!(player->stateFlags1 & PLAYER_STATE1_20000000)) {
        this->actionFunc(this, play);
        Actor_SetFocus(&this->actor, 0.0f);
        this->activeTimer = this->activeTimer + 1;
    }
}

void OoT_En_Boom_Draw(Actor* thisx, PlayState* play) {
    static Vec3f sMultVec1 = { -960.0f, 0.0f, 0.0f };
    static Vec3f sMultVec2 = { 960.0f, 0.0f, 0.0f };
    OoTEnBoom* this = (OoTEnBoom*)thisx;
    Vec3f vec1;
    Vec3f vec2;

    OPEN_DISPS(play->state.gfxCtx);

    // OoT's float Matrix_RotateY/Z/X == MM's Matrix_Rotate*F, with the same binang->radian conversion.
    Matrix_RotateYF(this->actor.world.rot.y * (OOT_M_PI / 0x8000), MTXMODE_APPLY);
    Matrix_RotateZF(0x1F40 * (OOT_M_PI / 0x8000), MTXMODE_APPLY);
    Matrix_RotateXF(this->actor.world.rot.x * (OOT_M_PI / 0x8000), MTXMODE_APPLY);
    Matrix_MultVec3f(&sMultVec1, &vec1);
    Matrix_MultVec3f(&sMultVec2, &vec2);

    // OoT func_80090480 == MM func_80126440 (weapon quad update).
    if (func_80126440(play, &this->collider, &this->boomerangInfo, &vec1, &vec2) != 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->effectIndex), &vec1, &vec2);
    }

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateYF((this->activeTimer * 12000) * (OOT_M_PI / 0x8000), MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sOoTBoomerangRefDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
