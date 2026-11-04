// MergeShip2 -- Majora's Mask Zora form, ported from 2ship2harkinian/mm into Ocarina of Time's player.
// Included at the end of z_player.c. See docs/PORTING_GUIDE.md.
//
// Every function names the MM function it ports (mm/src/overlays/actors/ovl_player_actor/z_player.c unless
// stated). Ocarina of Time's player reaches these through small hooks placed where MM's `transformation ==
// PLAYER_FORM_ZORA` branches sit; each hook calls a function declared in MergeShipZora.h.
//
// Names MM and Ocarina of Time spell differently (same meaning): MM skelAnimeUpper = upperSkelAnime,
// skelAnimeUpperBlendWeight = upperAnimInterpWeight, unk_ACC = unk_834, unk_ACE = unk_836, unk_ADC = unk_844,
// unk_ADD = unk_845, unk_AA5 = unk_6AD (MM PLAYER_UNKAA5_3 "aiming" = 2), speedXZ = linearVelocity,
// depthInWater = yDistToWater, sPlayerUseHeldItem = sUseHeldItem, sPlayerHeldItemButtonIsHeldDown =
// sHeldItemButtonIsHeldDown, sPlayerControlInput = sControlInput, sPlayerFloorType = sFloorType,
// sPlayerYDistToFloor = sYDistToFloor, currentBoots/prevBoots = gMergeShipPlayerExt.mmBoots/gMergeShipZora.prevBoots,
// PLAYER_STATE1_USING_ZORA_BOOMERANG/_THROWN = PLAYER_STATE1_USING_BOOMERANG/_THROWN (same bits),
// PLAYER_STATE1_8000000 = PLAYER_STATE1_IN_WATER, PLAYER_STATE1_400000 = PLAYER_STATE1_SHIELDING,
// PLAYER_STATE1_20000000 = PLAYER_STATE1_IN_CUTSCENE, PLAYER_STATE2_400 = PLAYER_STATE2_UNDERWATER,
// PLAYER_STATE2_800 = PLAYER_STATE2_DIVING, PLAYER_STATE2_20 = PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET,
// PLAYER_STATE2_40 = PLAYER_STATE2_DISABLE_ROTATION_ALWAYS, PLAYER_STATE1_10 = PLAYER_STATE1_HOSTILE_LOCK_ON
// (MM reuses that bit as "barrier requested this frame"; it is cleared every frame as in MM).

#include "soh/MergeShip/forms/MergeShipZora.h"
#include "overlays/actors/ovl_MM_En_Boom/z_mm_en_boom.h"

void Play_LoadToLastEntrance(PlayState* play); // z_play.c (MM func_80169F78)
void MergeShipTest_Log(const char* fmt, ...);   // MergeShipTest.cpp (no-op outside the harness)

MergeShipZoraState gMergeShipZora;

#define ZORA_IS(this) ((this)->mergeShipForm == PLAYER_FORM_ZORA)
#define MM_ANIM(name) ((LinkAnimationHeader*)mm_##name)

// ---- Damage ------------------------------------------------------------------------------------------------
// Hyrule's enemies read Ocarina of Time's damage bits, so each MM damage kind the Zora deals is given as the
// Ocarina of Time kind that does what MM's does. MM's Zora punches deal 1 and its jump kick 2 (MM
// D_8085D09C, PLAYER_MELEEWEAPON_ZORA_BOOMERANG row), exactly the Kokiri sword's slash and jump attack; MM's
// barrier deals 1 around the body, as the Kokiri spin attack. The fins (MM DMG_ZORA_BOOMERANG) keep bit 4,
// Ocarina of Time's boomerang (see ovl_MM_En_Boom).
#define MERGESHIP_ZORA_DMG_PUNCH DMG_SLASH_KOKIRI
#define MERGESHIP_ZORA_DMG_PUNCH_STRONG DMG_JUMP_KOKIRI
#define MERGESHIP_ZORA_DMG_BARRIER DMG_SPIN_KOKIRI

// ---- MM sMeleeAttackAnimInfo, the Zora's rows (MM numbering) --------------------------------------------
typedef struct {
    s8 mwa;
    struct_80854190 info;
} MergeShipZoraAttackAnim;

static MergeShipZoraAttackAnim sZoraAttackAnims[] = {
    { MM_PLAYER_MWA_ZORA_JUMPKICK_START,
      { MM_ANIM(gPlayerAnim_pz_jumpAT), MM_ANIM(gPlayerAnim_pz_jumpATend), MM_ANIM(gPlayerAnim_pz_jumpATend), 8, 99 } },
    { MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH,
      { MM_ANIM(gPlayerAnim_pz_jumpATend), MM_ANIM(gPlayerAnim_pz_wait), MM_ANIM(gPlayerAnim_link_normal_waitR_free), 1,
        2 } },
    { MM_PLAYER_MWA_ZORA_PUNCH_LEFT,
      { MM_ANIM(gPlayerAnim_pz_attackA), MM_ANIM(gPlayerAnim_pz_attackAend), MM_ANIM(gPlayerAnim_pz_attackAendR), 2,
        5 } },
    { MM_PLAYER_MWA_ZORA_PUNCH_COMBO,
      { MM_ANIM(gPlayerAnim_pz_attackB), MM_ANIM(gPlayerAnim_pz_attackBend), MM_ANIM(gPlayerAnim_pz_attackBendR), 3,
        8 } },
    { MM_PLAYER_MWA_ZORA_PUNCH_KICK,
      { MM_ANIM(gPlayerAnim_pz_attackC), MM_ANIM(gPlayerAnim_pz_attackCend), MM_ANIM(gPlayerAnim_pz_attackCendR), 3,
        10 } },
};

static struct_80854190* MergeShip_Zora_AttackAnimInfo(s32 mwa) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sZoraAttackAnims); i++) {
        if (sZoraAttackAnims[i].mwa == mwa) {
            return &sZoraAttackAnims[i].info;
        }
    }
    return &sZoraAttackAnims[2].info;
}

// The value Player::meleeWeaponAnimation carries for Hyrule's actors while the Zora attacks. Ocarina of Time
// actors test it for spin attacks (>= PLAYER_MWA_SPIN_ATTACK_1H), jump slashes (PLAYER_MWA_JUMPSLASH_START) and
// the hammer, and index tables by it (En_Kanban); MM's Zora values would read as spin attacks there. MM's
// actors give none of those reactions to the Zora, so these carry plain slashes in the attack's direction.
static s8 MergeShip_Zora_OoTMWA(s32 mmMwa) {
    switch (mmMwa) {
        case MM_PLAYER_MWA_ZORA_PUNCH_LEFT:
            return PLAYER_MWA_LEFT_SLASH_1H;
        case MM_PLAYER_MWA_ZORA_PUNCH_COMBO:
            return PLAYER_MWA_RIGHT_SLASH_1H;
        default:
            return PLAYER_MWA_FORWARD_SLASH_1H;
    }
}

static void MergeShip_Zora_SetMWA(Player* this, s32 mmMwa) {
    gMergeShipZora.meleeWeaponAnimation = mmMwa;
    this->meleeWeaponAnimation = MergeShip_Zora_OoTMWA(mmMwa);
    // MM's camera starts the Zora kick distortion from the MM value (MergeShipCamera.h).
    MergeShip_Camera_SetMMMeleeWeaponAnimation(mmMwa);
}

// MM Player_IsUsingZoraBoomerang.
static s32 MergeShip_Zora_IsUsingZoraBoomerang(Player* this) {
    return this->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG;
}

// MM Player_GetMeleeWeaponHeld for the Zora: its B item is PLAYER_MELEEWEAPON_ZORA_BOOMERANG.
static s32 MergeShip_Zora_HasMeleeWeapon(Player* this) {
    return this->heldItemAction == PLAYER_IA_MM_ZORA_BOOMERANG;
}

// MM func_801242B4.
static s32 MergeShip_Zora_IsSwimming(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_IN_WATER) && (gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER);
}

// MM func_8082DA90: a textbox is up or the scene is changing.
static s32 MergeShip_Zora_func_8082DA90(PlayState* play) {
    return (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
           (play->msgCtx.msgMode != MSGMODE_NONE);
}

static void MergeShip_Zora_SetBoots(PlayState* play, Player* this, u8 boots) {
    gMergeShipPlayerExt.mmBoots = boots;
    Player_SetBootData(play, this);
}

// ============================================================================================================
// Items: the fin boomerangs
// ============================================================================================================

// MM Player_InitZoraBoomerangIA.
void MergeShip_Zora_InitZoraBoomerangIA(PlayState* play, Player* this) {
    this->stateFlags1 |= PLAYER_STATE1_USING_BOOMERANG;
}

s32 MergeShip_Zora_UpperAction_3(Player* this, PlayState* play);

// MM func_80830A58 for the Zora: raise the fins (MM's anchor-to-defense animations; the Zora never holds a
// two-handed weapon). MM unk_B40 is Ocarina of Time's unk_870.
static LinkAnimationHeader* MergeShip_Zora_func_80830A58(PlayState* play, Player* this) {
    Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_3);
    Player_DetachHeldActor(play, this);

    if (this->unk_870 < 0.5f) {
        return MM_ANIM(gPlayerAnim_link_anchor_waitR2defense);
    } else {
        return MM_ANIM(gPlayerAnim_link_anchor_waitL2defense);
    }
}

// MM func_80830AE8 / func_80830B38 for the Zora: the guard's raise/lower sounds.
static void MergeShip_Zora_func_80830AE8(Player* this) {
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_SHIELD_SWING);
}
static void MergeShip_Zora_func_80830B38(Player* this) {
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_SHIELD_REMOVE);
}

// MM func_80830B88, the Zora's path: raise the fins as a guard (R while Z-targeting) from the upper body.
s32 MergeShip_Zora_func_80830B88(PlayState* play, Player* this) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
        if (!(this->stateFlags1 & (PLAYER_STATE1_SHIELDING | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE))) {
            if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
                ((gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) &&
                 (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
                if ((play->shootingGalleryStatus == 0) && (this->heldItemAction == this->itemAction)) {
                    if (!(this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) && Player_IsZTargeting(this)) {
                        LinkAnimationHeader* anim = MergeShip_Zora_func_80830A58(play, this);
                        f32 endFrame = Animation_GetLastFrame(anim);

                        LinkAnimation_Change(play, &this->upperSkelAnime, anim, 1.0f, endFrame, endFrame,
                                             ANIMMODE_ONCE, 0.0f);
                        MergeShip_Zora_func_80830AE8(this);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

s32 MergeShip_Zora_UpperAction_12(Player* this, PlayState* play);
s32 MergeShip_Zora_UpperAction_13(Player* this, PlayState* play);
s32 MergeShip_Zora_UpperAction_14(Player* this, PlayState* play);
s32 MergeShip_Zora_UpperAction_15(Player* this, PlayState* play);
s32 MergeShip_Zora_UpperAction_16(Player* this, PlayState* play);

// MM func_80830E30, the Zora's path: start aiming the fins (B held after a punch).
s32 MergeShip_Zora_func_80830E30(Player* this, PlayState* play) {
    s32 mwa = gMergeShipZora.meleeWeaponAnimation;

    Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_12);

    LinkAnimation_PlayOnce(play, &this->upperSkelAnime,
                           (mwa == MM_PLAYER_MWA_ZORA_PUNCH_LEFT)
                               ? MM_ANIM(gPlayerAnim_pz_cutterwaitA)
                               : ((mwa == MM_PLAYER_MWA_ZORA_PUNCH_COMBO) ? MM_ANIM(gPlayerAnim_pz_cutterwaitB)
                                                                          : MM_ANIM(gPlayerAnim_pz_cutterwaitC)));
    this->unk_834 = 0xA;

    if (this->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        Player_AnimPlayLoop(play, this, MM_ANIM(gPlayerAnim_link_uma_anim_walk));
    }

    return true;
}

// MM func_80831094 for the Zora (MM Player_UpperAction_11 calls it).
static s32 MergeShip_Zora_func_80831094(Player* this, PlayState* play) {
    if ((this->doorType == PLAYER_DOORTYPE_NONE) && !(this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN)) {
        if (sUseHeldItem || func_80834E44(play)) {
            if (MergeShip_Zora_func_80830E30(this, play)) {
                return func_80834EB8(this, play);
            }
        }
    }
    return false;
}

// MM Player_UpperAction_11.
static s32 MergeShip_Zora_UpperAction_11(Player* this, PlayState* play) {
    if (MergeShip_Zora_func_80830B88(play, this)) {
        return true;
    }

    if (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) {
        Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_15);
    } else if (MergeShip_Zora_func_80831094(this, play)) {
        return true;
    }

    return false;
}

// MM Player_UpperAction_12: the wind-up (pz_cutterwaitA/B/C), then the held pose.
s32 MergeShip_Zora_UpperAction_12(Player* this, PlayState* play) {
    if (LinkAnimation_Update(play, &this->upperSkelAnime)) {
        Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_13);
        LinkAnimation_PlayLoop(play, &this->upperSkelAnime, MM_ANIM(gPlayerAnim_pz_cutterwaitanim));
    }
    if (this->upperSkelAnime.animation == MM_ANIM(gPlayerAnim_pz_cutterwaitanim)) {
        func_80834EB8(this, play);
    }
    return true;
}

// MM Player_UpperAction_13: hold the fins until B is let go.
s32 MergeShip_Zora_UpperAction_13(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->upperSkelAnime);
    func_80834EB8(this, play);
    if (!sHeldItemButtonIsHeldDown) {
        Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_14);
        LinkAnimation_PlayOnce(play, &this->upperSkelAnime, MM_ANIM(gPlayerAnim_pz_cutterattack));
    }
    return true;
}

// MM Player_UpperAction_14: throw both fins on frame 6.
s32 MergeShip_Zora_UpperAction_14(Player* this, PlayState* play) {
    if (LinkAnimation_Update(play, &this->upperSkelAnime)) {
        Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_15);
        this->unk_834 = 0;
    } else if (LinkAnimation_OnFrame(&this->upperSkelAnime, 6.0f) && (gMergeShipActorId_EnBoom >= 0)) {
        Vec3f pos;
        s16 untargetedRotY;

        // MM func_80835BF8 is Ocarina of Time's func_8084260C with no y offset.
        pos.x = Math_SinS(this->actor.shape.rot.y) * 0.0f + this->bodyPartsPos[PLAYER_BODYPART_L_HAND].x;
        pos.z = Math_CosS(this->actor.shape.rot.y) * 0.0f + this->bodyPartsPos[PLAYER_BODYPART_L_HAND].z;
        pos.y = this->actor.world.pos.y + 50.0f;

        untargetedRotY = this->actor.shape.rot.y - 0x190;
        this->zoraBoomerangActor =
            Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_EnBoom, pos.x, pos.y, pos.z, this->actor.focus.rot.x,
                        (this->focusActor != NULL) ? this->actor.shape.rot.y + 0x36B0 : untargetedRotY, 0,
                        MM_ZORA_BOOMERANG_LEFT);

        if (this->zoraBoomerangActor != NULL) {
            MMEnBoom* leftZoraBoomerang = (MMEnBoom*)this->zoraBoomerangActor;
            MMEnBoom* rightZoraBoomerang;

            leftZoraBoomerang->moveTo = this->focusActor;
            if (leftZoraBoomerang->moveTo != NULL) {
                leftZoraBoomerang->unk_1CF = 0x10;
            }
            leftZoraBoomerang->unk_1CC = leftZoraBoomerang->unk_1CF + 0x24;

            pos.x = Math_SinS(this->actor.shape.rot.y) * 0.0f + this->bodyPartsPos[PLAYER_BODYPART_R_HAND].x;
            pos.z = Math_CosS(this->actor.shape.rot.y) * 0.0f + this->bodyPartsPos[PLAYER_BODYPART_R_HAND].z;

            untargetedRotY = (this->actor.shape.rot.y + 0x190);
            rightZoraBoomerang = (MMEnBoom*)Actor_Spawn(
                &play->actorCtx, play, gMergeShipActorId_EnBoom, pos.x, pos.y, pos.z, this->actor.focus.rot.x,
                (this->focusActor != NULL) ? this->actor.shape.rot.y - 0x36B0 : untargetedRotY, 0,
                MM_ZORA_BOOMERANG_RIGHT);

            if (rightZoraBoomerang != NULL) {
                rightZoraBoomerang->moveTo = this->focusActor;
                if (rightZoraBoomerang->moveTo != NULL) {
                    rightZoraBoomerang->unk_1CF = 0x10;
                }

                rightZoraBoomerang->unk_1CC = rightZoraBoomerang->unk_1CF + 0x24;
                leftZoraBoomerang->actor.child = &rightZoraBoomerang->actor;
                rightZoraBoomerang->actor.parent = &leftZoraBoomerang->actor;
            }

            this->stateFlags1 |= PLAYER_STATE1_BOOMERANG_THROWN;
            this->stateFlags3 &= ~PLAYER_STATE3_ZORA_BOOMERANG_CAUGHT;

            if (!Player_CheckHostileLockOn(this)) {
                Player_SetParallel(this);
            }

            this->unk_D57 = 20;

            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_BOOMERANG_THROW);
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_N);
        }
    }

    return true;
}

// MM Player_UpperAction_15: wait for a fin to come back.
s32 MergeShip_Zora_UpperAction_15(Player* this, PlayState* play) {
    if (MergeShip_Zora_func_80830B88(play, this)) {
        return true;
    }

    if (this->stateFlags3 & PLAYER_STATE3_ZORA_BOOMERANG_CAUGHT) {
        Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_16);
        LinkAnimation_PlayOnce(play, &this->upperSkelAnime, MM_ANIM(gPlayerAnim_pz_cuttercatch));
        this->stateFlags3 &= ~PLAYER_STATE3_ZORA_BOOMERANG_CAUGHT;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_CATCH_BOOMERANG);
        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_N);
        return true;
    }

    return false;
}

// MM Player_UpperAction_16: catch.
s32 MergeShip_Zora_UpperAction_16(Player* this, PlayState* play) {
    if (!MergeShip_Zora_UpperAction_11(this, play) && LinkAnimation_Update(play, &this->upperSkelAnime)) {
        if (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) {
            Player_SetUpperActionFunc(this, MergeShip_Zora_UpperAction_15);
            this->unk_834 = 0;
        } else {
            Player_SetUpperActionFunc(this, func_8083485C);
        }
    }
    return true;
}

// MM sPlayerItemChangeInfo[PLAYER_ITEM_CHG_14]: the fins drawn out/put back (pz_bladeon, change frame 4).
#define MERGESHIP_ZORA_ITEM_CHG_14 14

// MM Player_StartChangingHeldItem, for the Zora. The item change animations are MM's (the Zora is MM's
// skeleton); the change types are MM's sPlayerItemChangeTypes, which equal Ocarina of Time's sItemChangeTypes.
void MergeShip_Zora_StartChangingHeldItem(Player* this, PlayState* play) {
    static LinkAnimationHeader* sMMItemChangeAnims[] = {
        MM_ANIM(gPlayerAnim_link_normal_free2free),      MM_ANIM(gPlayerAnim_link_normal_normal2fighter),
        MM_ANIM(gPlayerAnim_link_hammer_normal2long),    MM_ANIM(gPlayerAnim_link_normal_normal2free),
        MM_ANIM(gPlayerAnim_link_fighter_fighter2long),  MM_ANIM(gPlayerAnim_link_normal_fighter2free),
        MM_ANIM(gPlayerAnim_link_hammer_long2free),      MM_ANIM(gPlayerAnim_link_hammer_long2long),
        MM_ANIM(gPlayerAnim_link_normal_free2free),      MM_ANIM(gPlayerAnim_link_normal_normal2bom),
        MM_ANIM(gPlayerAnim_link_normal_long2bom),       MM_ANIM(gPlayerAnim_link_normal_free2bom),
        MM_ANIM(gPlayerAnim_link_anchor_anchor2fighter), MM_ANIM(gPlayerAnim_link_normal_free2freeB),
        MM_ANIM(gPlayerAnim_pz_bladeon),
    };
    LinkAnimationHeader* anim;
    s32 nextModelAnimType;
    s32 curModelAnimType;
    s32 itemChangeType;
    s8 heldItemAction = Player_ItemToItemAction(this->heldItemId);
    f32 startFrame;
    f32 endFrame;
    f32 frameSpeed;

    Player_SetUpperActionFunc(this, Player_UpperAction_ChangeHeldItem);

    // MM gPlayerModelTypes: the Zora's fins (model group 10 here) animate as PLAYER_ANIMTYPE_DEFAULT.
    nextModelAnimType = (this->nextModelGroup == PLAYER_MODELGROUP_10)
                            ? PLAYER_ANIMTYPE_0
                            : gPlayerModelTypes[this->nextModelGroup][PLAYER_MODELGROUPENTRY_ANIM];
    curModelAnimType = (this->modelGroup == PLAYER_MODELGROUP_10)
                           ? PLAYER_ANIMTYPE_0
                           : gPlayerModelTypes[this->modelGroup][PLAYER_MODELGROUPENTRY_ANIM];
    itemChangeType = sItemChangeTypes[curModelAnimType][nextModelAnimType];

    if ((heldItemAction == PLAYER_IA_MM_ZORA_BOOMERANG) || (this->heldItemAction == PLAYER_IA_MM_ZORA_BOOMERANG)) {
        itemChangeType = (heldItemAction == PLAYER_IA_NONE) ? -MERGESHIP_ZORA_ITEM_CHG_14 : MERGESHIP_ZORA_ITEM_CHG_14;
    } else if ((heldItemAction == PLAYER_IA_BOTTLE) ||
               ((heldItemAction == PLAYER_IA_NONE) && (this->heldItemAction == PLAYER_IA_BOTTLE))) {
        itemChangeType = (heldItemAction == PLAYER_IA_NONE) ? -PLAYER_ITEM_CHG_13 : PLAYER_ITEM_CHG_13;
    }

    this->itemChangeType = ABS(itemChangeType);
    anim = sMMItemChangeAnims[this->itemChangeType];

    if ((anim == MM_ANIM(gPlayerAnim_link_normal_fighter2free)) && (this->currentShield == PLAYER_SHIELD_NONE)) {
        anim = MM_ANIM(gPlayerAnim_link_normal_free2fighter_free);
    }

    endFrame = Animation_GetLastFrame(anim);

    if (itemChangeType >= 0) {
        frameSpeed = 1.2f;
        startFrame = 0.0f;
    } else {
        frameSpeed = -1.2f;
        startFrame = endFrame;
        endFrame = 0.0f;
    }

    if (heldItemAction != PLAYER_IA_NONE) {
        frameSpeed *= 2.0f;
    }

    LinkAnimation_Change(play, &this->upperSkelAnime, anim, frameSpeed, startFrame, endFrame, ANIMMODE_ONCE, 0.0f);

    this->stateFlags1 &= ~PLAYER_STATE1_START_CHANGING_HELD_ITEM;
}

// MM Player_WaitToFinishItemChange for the Zora (MM sPlayerItemChangeInfo change frames, including
// PLAYER_ITEM_CHG_14, which Ocarina of Time's table does not have). Returns true when it handled the frame.
s32 MergeShip_Zora_WaitToFinishItemChange(PlayState* play, Player* this) {
    static s8 sMMItemChangeFrames[] = { 12, 6, 8, 8, 8, 10, 7, 11, 12, 4, 4, 4, 5, 13, 4 };
    f32 changeFrame;

    if (this->itemChangeType >= ARRAY_COUNT(sMMItemChangeFrames)) {
        return false;
    }

    changeFrame = sMMItemChangeFrames[this->itemChangeType];
    if (this->upperSkelAnime.playSpeed < 0.0f) {
        changeFrame -= 1.0f;
    }

    if (LinkAnimation_OnFrame(&this->upperSkelAnime, changeFrame)) {
        Player_FinishItemChange(play, this);
    }

    Player_UpdateHostileLockOn(this);
    return true;
}

// MM func_8082EF54 / func_8082EF9C (and MM's side-walk getters): while the fins are aimed (MM func_8082EF20)
// the lower body stands in MM's boomerang poses; MM has no boomerang side-walk, so side-stepping keeps the
// form's own side-walk. `which`: 0 waitR, 1 waitL, 2 side_walkR, 3 side_walkL.
LinkAnimationHeader* MergeShip_Zora_BoomerangWaitAnim(Player* this, s32 which) {
    switch (which) {
        case 0:
            return MM_ANIM(gPlayerAnim_link_boom_throw_waitR);
        case 1:
            return MM_ANIM(gPlayerAnim_link_boom_throw_waitL);
        case 2:
            return GET_PLAYER_ANIM(PLAYER_ANIMGROUP_side_walkR, this->modelAnimType);
        default:
            return GET_PLAYER_ANIM(PLAYER_ANIMGROUP_side_walkL, this->modelAnimType);
    }
}

// MM func_8083868C, the Zora's aim: MM_CAM_MODE_ZORAFIN while aiming the fins, else first person. The MM
// mode is requested instead of an Ocarina of Time mode (MergeShipCamera.h); MM returns the mode the camera
// took, and ZORAFIN is valid in every setting the Zora can aim in, so this reports success.
s32 MergeShip_Zora_func_8083868C(PlayState* play, Player* this) {
    if (this->unk_6AD == 2) {
        if (func_8002DD6C(this)) {
            return Camera_RequestMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_AIM_ADULT);
        }
        MergeShip_Camera_RequestMMMode(play, MM_CAM_MODE_ZORAFIN);
        return true;
    }

    return Camera_RequestMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_FIRST_PERSON);
}

// ============================================================================================================
// Guard (fins raised on R) and the electric barrier
// ============================================================================================================

// MM func_80830CE8 for the Zora: lower the fins.
static void MergeShip_Zora_func_80830CE8(PlayState* play, Player* this) {
    Player_SetUpperActionFunc(this, func_80834C74);

    if (this->itemAction < 0) {
        func_8008EC70(this);
    }

    Animation_Reverse(&this->upperSkelAnime);
    MergeShip_Zora_func_80830B38(this);
}

// MM Player_UpperAction_3: holding the fin guard; B while guarding raises the barrier.
s32 MergeShip_Zora_UpperAction_3(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->upperSkelAnime);
    if (!CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
        MergeShip_Zora_func_80830CE8(play, this);
    } else {
        this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        Player_SetModelsForHoldingShield(this);
        if (ZORA_IS(this) && CHECK_BTN_ALL(sControlInput->cur.button, BTN_B)) {
            MergeShip_Zora_func_8082F164(this, BTN_R | BTN_B);
        }
    }
    return true;
}

// MM func_8082F164: request the barrier this frame while `button` is held. MM sets PLAYER_STATE1_10, a bit
// its player uses only for this; in Ocarina of Time that bit is PLAYER_STATE1_HOSTILE_LOCK_ON, which Hyrule's
// code reads, so the request is kept in MergeShipZoraState instead (cleared before the action runs, read after,
// as MM does with the bit).
void MergeShip_Zora_func_8082F164(Player* this, u16 button) {
    if (ZORA_IS(this) && CHECK_BTN_ALL(sControlInput->cur.button, button)) {
        gMergeShipZora.barrierRequest = true;
    }
}

// MM sZoraBarrierEnvLighting.
static u8 sZoraBarrierAmbient[3] = { 0, 0, 0 };
static u8 sZoraBarrierDiffuse[3] = { 255, 255, 155 };
static u8 sZoraBarrierFog[3] = { 20, 20, 50 };
#define ZORA_BARRIER_FOG_NEAR 940
#define ZORA_BARRIER_Z_FAR 5000

// MM Environment_LerpRGB8 (z_kankyo.c) onto Ocarina of Time's adjustment fields.
static void MergeShip_Zora_LerpRGB8(u8* from, u8* to, f32 lerp, s16* dst) {
    s32 i;

    for (i = 0; i < 3; i++) {
        // MM Color_RGB8_Lerp: from + (to - from) * lerp, truncated.
        u8 result = (u8)(from[i] + (s32)((to[i] - from[i]) * lerp));

        dst[i] = result - from[i];
    }
}

// MM Player_LerpEnvLighting. Ocarina of Time has a single diffuse light adjustment (adjLight1Color); MM writes
// its light2 adjustment with the same colour.
static void MergeShip_Zora_LerpEnvLighting(PlayState* play, f32 lerp) {
    MergeShip_Zora_LerpRGB8(play->envCtx.lightSettings.ambientColor, sZoraBarrierAmbient, lerp,
                            play->envCtx.adjAmbientColor);
    MergeShip_Zora_LerpRGB8(play->envCtx.lightSettings.light1Color, sZoraBarrierDiffuse, lerp,
                            play->envCtx.adjLight1Color);
    MergeShip_Zora_LerpRGB8(play->envCtx.lightSettings.fogColor, sZoraBarrierFog, lerp, play->envCtx.adjFogColor);
    play->envCtx.adjFogNear = (ZORA_BARRIER_FOG_NEAR - play->envCtx.lightSettings.fogNear) * lerp;
    play->envCtx.adjFogFar = (ZORA_BARRIER_Z_FAR - play->envCtx.lightSettings.fogFar) * lerp;
}

// MM Magic_Consume(play, 0, MAGIC_CONSUME_GORON_ZORA). Ocarina of Time's magic meter has no Goron/Zora drain
// state, so the drain MM's Magic_Update runs for it (MAGIC_STATE_CONSUME_GORON_ZORA_SETUP/_GORON_ZORA) is run
// here, and the meter is put in MAGIC_STATE_METER_FLASH_3 ("flash the border, nothing else"), which is what
// MM's drain state looks like on screen and keeps other magic users out as MM's non-idle state does.
static void MergeShip_Zora_MagicConsumeGoronZora(PlayState* play) {
    if (gSaveContext.magic != 0) {
        gMergeShipZora.magicTimer = 10;
        gMergeShipZora.barrierDraining = 2; // SETUP
        gSaveContext.magicState = MAGIC_STATE_METER_FLASH_3;
        MergeShipTest_Log("ZORABARRIER drain start mp=%d", gSaveContext.magic);
    } else {
        MergeShipTest_Log("ZORABARRIER refused: no magic");
    }
}

// MM Magic_Update, cases MAGIC_STATE_CONSUME_GORON_ZORA_SETUP and MAGIC_STATE_CONSUME_GORON_ZORA.
static void MergeShip_Zora_UpdateMagicDrain(PlayState* play) {
    if (gMergeShipZora.barrierDraining == 0) {
        return;
    }
    if (gSaveContext.magicState != MAGIC_STATE_METER_FLASH_3) {
        gMergeShipZora.barrierDraining = 0;
        return;
    }

    if (gMergeShipZora.barrierDraining == 2) {
        if (!MergeShip_Chateau_IsActive()) { // MergeShip2: MM !CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI)
            gSaveContext.magic -= 2;
        }
        if (gSaveContext.magic <= 0) {
            gSaveContext.magic = 0;
        }
        gMergeShipZora.barrierDraining = 1;
    }

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->transitionMode == TRANS_MODE_OFF) && !Play_InCsMode(play)) {
        gMergeShipZora.magicTimer--;
        if (gMergeShipZora.magicTimer == 0) {
            if (!MergeShip_Chateau_IsActive()) { // MergeShip2: MM, Chateau Romani's effect takes nothing
                gSaveContext.magic--;
            }
            if (gSaveContext.magic <= 0) {
                gSaveContext.magic = 0;
            }
            gMergeShipZora.magicTimer = 10;
        }
    }
}

// MM Play_UpdateWaterCamera (z_play.c): with the camera under water, the fast-swimming Zora's view warps with
// DISTORTION_TYPE_ZORA_SWIMMING, otherwise with DISTORTION_TYPE_NON_ZORA_SWIMMING; out of the water both stop.
// (Ocarina of Time's camera keeps its own underwater flag, unk_14C & 0x100, and its own weaker wobble, which
// MM's distortion replaces while requested.)
static void MergeShip_Zora_UpdateWaterDistortion(Player* this, PlayState* play) {
    Camera* camera = Play_GetCamera(play, CAM_ID_MAIN);

    if ((camera != NULL) && (camera->unk_14C & 0x100)) {
        if (this->stateFlags3 & PLAYER_STATE3_8000) {
            MergeShip_Distortion_Request(MERGESHIP_DISTORTION_ZORA_SWIMMING);
            MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_NON_ZORA_SWIMMING);
        } else {
            MergeShip_Distortion_Request(MERGESHIP_DISTORTION_NON_ZORA_SWIMMING);
            MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_ZORA_SWIMMING);
        }
    } else {
        MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_NON_ZORA_SWIMMING);
        MergeShip_Distortion_RemoveRequest(MERGESHIP_DISTORTION_ZORA_SWIMMING);
    }
}

// MM func_8082F1AC: run the barrier.
static void MergeShip_Zora_func_8082F1AC(PlayState* play, Player* this) {
    s32 sp4C = this->unk_B62;
    f32 temp;
    s16 sp46;
    s16 sp44;
    f32 sp40;
    f32 sp3C;
    s32 var_v0;

    if ((gSaveContext.magic != 0) && gMergeShipZora.barrierRequest) {
        if (gSaveContext.magicState == MAGIC_STATE_IDLE) {
            MergeShip_Zora_MagicConsumeGoronZora(play);
        }

        temp = 16.0f;
        if (gSaveContext.magic >= 16) {
            var_v0 = 255;
        } else {
            var_v0 = (gSaveContext.magic / temp) * 255.0f;
        }
        Math_StepToS(&this->unk_B62, var_v0, 50);
    } else if (Math_StepToS(&this->unk_B62, 0, 50) && (gSaveContext.magicState != MAGIC_STATE_IDLE)) {
        Magic_Reset(play);
        gMergeShipZora.barrierDraining = 0;
    }

    if ((this->unk_B62 != 0) || (sp4C != 0)) {
        f32 sp34;
        f32 new_var;

        sp46 = play->gameplayFrames * 7000;
        sp44 = play->gameplayFrames * 14000;
        MergeShip_Zora_LerpEnvLighting(play, this->unk_B62 / 255.0f);

        sp34 = Math_SinS(sp44) * 40.0f;
        sp40 = Math_CosS(sp44) * 40.0f;
        sp3C = Math_SinS(sp46) * sp34;
        new_var = Math_CosS(sp46) * sp34;

        Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x + sp40, this->actor.world.pos.y + sp3C,
                                  this->actor.world.pos.z + new_var, 100, 200, 255, 600);

        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_ZORA_SPARK_BARRIER - SFX_FLAG);
        // MM also calls Actor_SetPlayerImpact(PLAYER_IMPACT_ZORA_BARRIER): only MM's actors read player impacts;
        // Hyrule's have no counterpart.
    }
}

// MM Player_UpdateCommon, after the action has run: the barrier (func_8082F1AC) and its collider on
// shieldCylinder. Called once per frame from Ocarina of Time's Player_UpdateCommon for the Zora.
void MergeShip_Zora_UpdateBarrier(Player* this, PlayState* play) {
    MergeShip_Zora_UpdateMagicDrain(play);
    MergeShip_Zora_UpdateWaterDistortion(this, play);

    if ((this->unk_B62 != 0) || ((gSaveContext.magicState == MAGIC_STATE_IDLE) && (gSaveContext.magic != 0) &&
                                 gMergeShipZora.barrierRequest)) {
        MergeShip_Zora_func_8082F1AC(play, this);
    }

    if (this->unk_B62 != 0) {
        // MM: the cylinder spans the feet to the head, lifted by this frame's vertical movement.
        f32 dy = this->actor.world.pos.y - this->actor.prevPos.y;
        f32 feet =
            dy + ((this->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y + this->bodyPartsPos[PLAYER_BODYPART_R_FOOT].y) * 0.5f);
        f32 head = dy + this->bodyPartsPos[PLAYER_BODYPART_HEAD].y + 10.0f;

        this->shieldCylinder.base.acFlags = AC_NONE;
        this->shieldCylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
        this->shieldCylinder.info.toucher.dmgFlags = MERGESHIP_ZORA_DMG_BARRIER;
        this->shieldCylinder.info.toucher.damage = 1;
        this->shieldCylinder.info.toucherFlags = TOUCH_ON;
        this->shieldCylinder.info.bumperFlags = BUMP_NONE;
        this->shieldCylinder.dim.height = 80;
        this->shieldCylinder.dim.radius = 50;
        this->shieldCylinder.dim.yShift = ((head + feet) * 0.5f - 40.0f) - this->actor.world.pos.y;

        Collider_UpdateCylinder(&this->actor, &this->shieldCylinder);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->shieldCylinder.base);
    }
}

// MM Player_UpdateCommon, end: the shield cylinder's hit flags are reset for the next frame.
void MergeShip_Zora_ResetBarrierColliders(Player* this, PlayState* play) {
    Collider_ResetCylinderAC(play, &this->shieldCylinder.base);
    Collider_ResetCylinderAT(play, &this->shieldCylinder.base);
}

// ============================================================================================================
// Melee: punch / punch / kick combo on B, jump kick
// ============================================================================================================

// MM D_8085CF88 (sBlureColors).
static Color_RGBA8 sZoraBlureColors[][4] = {
    { { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 } },
    { { 165, 185, 255, 185 }, { 205, 225, 255, 50 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 } },
};

// MM Player_OverrideBlureColors. MM's player has two blures (one per fin); Ocarina of Time's has one, and the
// Zora's second is gMergeShipZora.blureIndex2 (see MergeShip_Zora_Init).
static void MergeShip_Zora_OverrideBlureColors(PlayState* play, Player* this, s32 colorType, s32 elemDuration) {
    EffectBlure* blures[2];
    s32 i;
    s32 j;

    blures[0] = Effect_GetByIndex(this->meleeWeaponEffectIndex);
    blures[1] = gMergeShipZora.blureValid ? Effect_GetByIndex(gMergeShipZora.blureIndex2) : NULL;

    for (j = 0; j < 2; j++) {
        if (blures[j] == NULL) {
            continue;
        }
        blures[j]->p1StartColor = sZoraBlureColors[colorType][0];
        blures[j]->p2StartColor = sZoraBlureColors[colorType][1];
        blures[j]->p1EndColor = sZoraBlureColors[colorType][2];
        blures[j]->p2EndColor = sZoraBlureColors[colorType][3];
        blures[j]->elemDuration = elemDuration;
    }
    (void)i;
}

// MM func_8082FA5C for the Zora: the swing sounds, and the blure back to white.
static void MergeShip_Zora_func_8082FA5C(PlayState* play, Player* this, s32 meleeWeaponState) {
    u16 voiceSfxId;
    u16 itemSfxId;

    if (this->meleeWeaponState == 0) {
        voiceSfxId = MM_NA_SE_VO_LI_SWORD_N;
        itemSfxId = 0;
        if (gMergeShipZora.meleeWeaponAnimation >= MM_PLAYER_MWA_SPIN_ATTACK_1H) {
            voiceSfxId = MM_NA_SE_VO_LI_SWORD_L;
        } else if (gMergeShipZora.meleeWeaponAnimation == MM_PLAYER_MWA_ZORA_PUNCH_KICK) {
            itemSfxId = MM_NA_SE_IT_GORON_PUNCH_SWING;
        } else {
            itemSfxId = MM_NA_SE_IT_SWORD_SWING_HARD;
            if ((s8)this->unk_845 >= 3) {
                voiceSfxId = MM_NA_SE_VO_LI_SWORD_L;
            } else {
                itemSfxId = MM_NA_SE_IT_SWORD_SWING;
            }
        }

        if (itemSfxId != 0) {
            MergeShip_PlayMMSfxAndFootstepFlag(this, itemSfxId);
        }

        if (!((gMergeShipZora.meleeWeaponAnimation >= MM_PLAYER_MWA_FLIPSLASH_START) &&
              (gMergeShipZora.meleeWeaponAnimation <= MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH))) {
            MergeShip_PlayMMVoiceSfx(this, voiceSfxId);
        }

        MergeShip_Zora_OverrideBlureColors(play, this, 0, 4);
    }

    this->meleeWeaponState = meleeWeaponState;
}

// MM func_8083FCF0: the attack's active frames.
static s32 MergeShip_Zora_func_8083FCF0(PlayState* play, Player* this, f32 arg2, f32 arg3, f32 arg4) {
    if (arg4 < this->skelAnime.curFrame) {
        func_80832318(this);
    } else if (arg2 <= this->skelAnime.curFrame) {
        this->stateFlags3 |= PLAYER_STATE3_2000000;
        MergeShip_Zora_func_8082FA5C(play, this, (arg3 <= this->skelAnime.curFrame) ? 1 : -1);
        return true;
    }
    return false;
}

// MM D_8085D094[0]: the Zora's combo.
static s8 sZoraPunchCombo[] = {
    MM_PLAYER_MWA_ZORA_PUNCH_LEFT,
    MM_PLAYER_MWA_ZORA_PUNCH_COMBO,
    MM_PLAYER_MWA_ZORA_PUNCH_KICK,
};

// MM func_808335F4, the Zora's path. MM's unk_ADD is signed and set to -1 after the kick; Ocarina of Time's
// unk_845 is a u8, so it is read and written as s8 here.
s32 MergeShip_Zora_func_808335F4(Player* this) {
    s32 unk_ADD = (s8)this->unk_845;
    s32 meleeWeaponAnim;

    // MM indexes D_8085D094[0][unk_ADD]; unk_ADD is 3 only for the few frames after a jump kick lands, where MM
    // reads past the Zora's row into the Goron's. The Zora keeps its first punch there.
    meleeWeaponAnim = sZoraPunchCombo[((unk_ADD >= 0) && (unk_ADD < 3)) ? unk_ADD : 0];

    if (unk_ADD != 0) {
        MergeShip_Zora_SetMWA(this, meleeWeaponAnim);
        if (unk_ADD >= 2) {
            this->unk_845 = (u8)-1;
        }
    }
    return meleeWeaponAnim;
}

// MM func_80833728 / func_8083375C for the Zora (MM D_8085D09C, PLAYER_MELEEWEAPON_ZORA_BOOMERANG: 1 normal,
// 2 for the jump kick; see MERGESHIP_ZORA_DMG_*).
static void MergeShip_Zora_func_8083375C(Player* this, s32 meleeWeaponAnim) {
    s32 strong = (meleeWeaponAnim >= MM_PLAYER_MWA_FLIPSLASH_START) &&
                 (meleeWeaponAnim <= MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH);
    u32 dmgFlags = strong ? MERGESHIP_ZORA_DMG_PUNCH_STRONG : MERGESHIP_ZORA_DMG_PUNCH;
    s32 damage = strong ? 2 : 1;
    s32 i;

    for (i = 0; i < 2; i++) {
        func_80837918(this, i, dmgFlags);
        this->meleeWeaponQuads[i].info.toucher.damage = damage;
    }
}

void MergeShip_Zora_Action_84(Player* this, PlayState* play);
void MergeShip_Zora_Action_29(Player* this, PlayState* play);

// MM func_80833864: start an attack.
void MergeShip_Zora_func_80833864(PlayState* play, Player* this, s32 meleeWeaponAnim) {
    s32 unk_ADD;

    MergeShip_Zora_func_8083375C(this, meleeWeaponAnim);
    Player_SetupAction(play, this, MergeShip_Zora_Action_84, 0);
    this->av2.actionVar2 = 0;

    if ((meleeWeaponAnim < MM_PLAYER_MWA_FLIPSLASH_FINISH) || (meleeWeaponAnim > MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH)) {
        func_80832318(this);
    }

    // Accumulate consecutive attacks for the "third attack" types.
    unk_ADD = (s8)this->unk_845;
    if ((meleeWeaponAnim != gMergeShipZora.meleeWeaponAnimation) || (unk_ADD >= 3)) {
        unk_ADD = 0;
    }

    unk_ADD++;
    if (unk_ADD >= 3) {
        meleeWeaponAnim += 2;
    }
    this->unk_845 = unk_ADD;

    MergeShip_Zora_SetMWA(this, meleeWeaponAnim);
    Player_AnimPlayOnceAdjusted(play, this, MergeShip_Zora_AttackAnimInfo(meleeWeaponAnim)->unk_00);
    this->unk_844 = this->skelAnime.animLength + 4.0f;

    if ((meleeWeaponAnim < MM_PLAYER_MWA_FLIPSLASH_START) || (meleeWeaponAnim > MM_PLAYER_MWA_ZORA_JUMPKICK_START)) {
        Player_StartAnimMovement(play, this, 0x209); // MM ANIM_FLAG_1 | ANIM_FLAG_ENABLE_MOVEMENT | ANIM_FLAG_NOMOVE
    }
    this->yaw = this->actor.shape.rot.y;
}

// MM func_808400CC for a transformed player: bounce back without a rebound animation.
static void MergeShip_Zora_Action_85(Player* this, PlayState* play);
static void MergeShip_Zora_func_808400CC(PlayState* play, Player* this) {
    if (Player_Action_80843188 != this->actionFunc) {
        u8 savedMovementFlags = this->skelAnime.movementFlags;

        func_80832440(play, this);
        this->skelAnime.movementFlags = 0;
        Player_SetupAction(play, this, MergeShip_Zora_Action_85, 0);
        this->skelAnime.movementFlags = savedMovementFlags;
    }

    Player_RequestRumble(this, 180, 20, 100, 0);
    this->linearVelocity = -18.0f;
    // MM func_80840094: Deku stick and razor sword wear; the Zora holds neither.
}

// MM Player_Action_85: bounced off something hard.
static void MergeShip_Zora_Action_85(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    Player_DecelerateToZero(this);

    if (this->skelAnime.curFrame >= 6.0f) {
        func_80839F90(this, play);
    }
}

// MM func_808401F4, the Zora's path: walls deflect the fins; hits freeze the frame.
static s32 MergeShip_Zora_func_808401F4(PlayState* play, Player* this) {
    if (this->meleeWeaponState >= 1) {
        s32 temp_v0_3;

        if (gMergeShipZora.meleeWeaponAnimation < MM_PLAYER_MWA_SPIN_ATTACK_1H) {
            if (!(this->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) &&
                !(this->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED)) {
                if (this->skelAnime.curFrame >= 2.0f) {
                    CollisionPoly* poly;
                    s32 bgId;
                    Vec3f spC8;
                    Vec3f pos;
                    Vec3f spB0;
                    Vec3f* var_a1;
                    Vec3f* temp_a0 = &this->meleeWeaponInfo[0].tip;
                    f32 var_fv1;

                    if (this->linearVelocity >= 0.0f) {
                        var_a1 = &this->meleeWeaponInfo[0].base;

                        var_fv1 = Math_Vec3f_DistXYZAndStoreDiff(temp_a0, var_a1, &spB0);
                        if (var_fv1 != 0.0f) {
                            var_fv1 = (var_fv1 + 10.0f) / var_fv1;
                        }

                        spC8.x = temp_a0->x + (spB0.x * var_fv1);
                        spC8.y = temp_a0->y + (spB0.y * var_fv1);
                        spC8.z = temp_a0->z + (spB0.z * var_fv1);
                        if (BgCheck_EntityLineTest2(&play->colCtx, &spC8, temp_a0, &pos, &poly, true, false, false,
                                                    true, &bgId, &this->actor)) {
                            if (!SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId) &&
                                (SurfaceType_GetFloorType(&play->colCtx, poly, bgId) != 6) &&
                                !func_8002F9EC(play, &this->actor, poly, bgId, &pos)) {
                                if (this->linearVelocity >= 0.0f) {
                                    // Hyrule's surfaces: Ocarina of Time's material ids (0xA wood, 0xB soft dirt).
                                    s32 surfaceMaterial = func_80041F10(&play->colCtx, poly, bgId);

                                    if (surfaceMaterial == 0xA) {
                                        CollisionCheck_SpawnShieldParticlesWood(play, &pos, &this->actor.projectedPos);
                                    } else {
                                        pos.x += 8.0f * COLPOLY_GET_NORMAL(poly->normal.x);
                                        pos.y += 8.0f * COLPOLY_GET_NORMAL(poly->normal.y);
                                        pos.x += 8.0f * COLPOLY_GET_NORMAL(poly->normal.z);
                                        CollisionCheck_SpawnShieldParticles(play, &pos);

                                        if (surfaceMaterial == 0xB) {
                                            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_WALL_HIT_SOFT);
                                        } else {
                                            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_WALL_HIT_HARD);
                                        }
                                    }

                                    Player_RequestRumble(this, 180, 20, 100, 0);
                                    this->linearVelocity = -14.0f;
                                }
                            }
                        }
                    }
                }
            } else {
                MergeShip_Zora_func_808400CC(play, this);
                func_80832630(play);
                return true;
            }
        }

        temp_v0_3 = (this->meleeWeaponQuads[0].base.atFlags & AT_HIT) != 0;
        if (temp_v0_3 || (this->meleeWeaponQuads[1].base.atFlags & AT_HIT)) {
            if (gMergeShipZora.meleeWeaponAnimation < MM_PLAYER_MWA_SPIN_ATTACK_1H) {
                Actor* temp_v1 = this->meleeWeaponQuads[temp_v0_3 ? 0 : 1].base.at;

                if ((temp_v1 != NULL) && (temp_v1->id != ACTOR_EN_KANBAN)) {
                    func_80832630(play);
                }
            }

            if (this->actor.colChkInfo.atHitEffect == 1) {
                this->actor.colChkInfo.damage = 8;
                func_80837C0C(play, this, PLAYER_HIT_RESPONSE_ELECTRIFIED, 0.0f, 0.0f, this->actor.shape.rot.y, 20);
                return true;
            }
        }
    }

    return false;
}

// MM Player_Action_84: the attack.
void MergeShip_Zora_Action_84(Player* this, PlayState* play) {
    struct_80854190* attackInfoEntry = MergeShip_Zora_AttackAnimInfo(gMergeShipZora.meleeWeaponAnimation);
    s32 mwa = gMergeShipZora.meleeWeaponAnimation;

    if (this->skelAnime.curFrame < (this->skelAnime.endFrame - 6.0f)) {
        this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    }

    if (MergeShip_Zora_func_808401F4(play, this)) {
        return;
    }

    if (this->linearVelocity >= 0.0f) {
        MergeShip_Zora_func_8083FCF0(play, this, 0.0f, attackInfoEntry->unk_0C, attackInfoEntry->unk_0D);
    }

    if ((this->stateFlags2 & PLAYER_STATE2_SWORD_LUNGE) && LinkAnimation_OnFrame(&this->skelAnime, 0.0f)) {
        this->linearVelocity = 15.0f;
        this->stateFlags2 &= ~PLAYER_STATE2_SWORD_LUNGE;
    }

    if (this->linearVelocity > 12.0f) {
        func_8084269C(play, this); // MM func_8083FBC4
    }

    Math_StepToF(&this->linearVelocity, 0.0f, 5.0f);
    func_8083C50C(this); // MM func_8083A548

    if (LinkAnimation_Update(play, &this->skelAnime) ||
        ((mwa >= MM_PLAYER_MWA_FLIPSLASH_FINISH) && (mwa <= MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH) &&
         (this->skelAnime.curFrame > 2.0f) && Player_CanSpinAttack(this))) {
        sUseHeldItem = this->av2.actionVar2;

        if (!MergeShip_Zora_ActionHandler_7(this, play)) {
            LinkAnimationHeader* anim =
                Player_CheckHostileLockOn(this) ? attackInfoEntry->unk_08 : attackInfoEntry->unk_04;
            u8 savedMovementFlags;

            func_80832318(this);

            savedMovementFlags = this->skelAnime.movementFlags;

            if (MergeShip_Zora_ActionHandler_8(this, play)) {
                anim = this->upperSkelAnime.animation;
            }
            this->unk_844 = 0;

            this->skelAnime.movementFlags = 0;
            Player_SetupAction(play, this, Player_Action_Idle, 1);
            func_8083328C(play, this, anim); // MM Player_Anim_PlayOnceWaterAdjustment
            this->yaw = this->actor.shape.rot.y;
            this->skelAnime.movementFlags = savedMovementFlags;

            this->stateFlags3 |= PLAYER_STATE3_FINISHED_ATTACKING; // MM PLAYER_STATE3_8
        }
    } else if ((mwa != MM_PLAYER_MWA_ZORA_PUNCH_KICK) && (mwa != MM_PLAYER_MWA_ZORA_JUMPKICK_FINISH)) {
        this->av2.actionVar2 |= CHECK_BTN_ALL(sControlInput->press.button, BTN_B) ? 1 : 0;
    }
}

// MM func_808395F0: the jump kick (the Zora's jump attack), 10% faster and 10% lower than a human's.
void MergeShip_Zora_func_808395F0(PlayState* play, Player* this, s32 meleeWeaponAnim, f32 linearVelocity,
                                  f32 yVelocity) {
    linearVelocity *= 1.1f;
    meleeWeaponAnim = MM_PLAYER_MWA_ZORA_JUMPKICK_START;
    yVelocity *= 0.9f;

    MergeShip_Zora_func_80833864(play, this, meleeWeaponAnim);
    Player_SetupAction(play, this, MergeShip_Zora_Action_29, 0);
    this->stateFlags3 |= PLAYER_STATE3_MIDAIR; // MM PLAYER_STATE3_2
    this->linearVelocity = linearVelocity;
    this->yaw = this->actor.shape.rot.y;
    this->actor.velocity.y = yVelocity;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    MergeShip_PlayMMFloorJumpSfx(this);
    MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_L);
}

// MM func_808396B8, the Zora's path: B uses the fins unless they are in flight.
s32 MergeShip_Zora_func_808396B8(PlayState* play, Player* this) {
    if (!(this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        (((this->actor.category != ACTORCAT_PLAYER) && CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) ||
         (MergeShip_Zora_HasMeleeWeapon(this) && !(this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) &&
          sUseHeldItem))) {
        return true;
    }

    return false;
}

// MM func_80839770: B in the air is a jump kick.
s32 MergeShip_Zora_func_80839770(Player* this, PlayState* play) {
    if (MergeShip_Zora_func_808396B8(play, this)) {
        if (sFloorType != 7) {
            MergeShip_Zora_func_808395F0(play, this, MM_PLAYER_MWA_ZORA_JUMPKICK_START, 3.0f, 4.5f);
            return true;
        }
    }
    return false;
}

// MM Player_ActionHandler_7: B attacks.
s32 MergeShip_Zora_ActionHandler_7(Player* this, PlayState* play) {
    if (!func_8083C6B8(play, this)) { // MM func_8083A6C0: bottles
        if (MergeShip_Zora_func_808396B8(play, this)) {
            s32 meleeWeaponAnim = MergeShip_Zora_func_808335F4(this);

            MergeShip_Zora_func_80833864(play, this, meleeWeaponAnim);
            // (MM's spin attack and Fierce Deity beam branches do not apply to the Zora.)
        } else {
            return false;
        }
    }
    return true;
}

// MM Player_ActionHandler_8, the Zora's path: B held after a punch aims the fins.
s32 MergeShip_Zora_ActionHandler_8(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_B)) {
        if (!(this->stateFlags1 & PLAYER_STATE1_SHIELDING) && MergeShip_Zora_HasMeleeWeapon(this)) {
            if (this->unk_844 > 0) {
                MergeShip_Zora_func_80830E30(this, play);
                return true;
            }
        }
    } else {
        func_8083C50C(this); // MM func_8083A548
    }
    return false;
}

// MM Player_ActionHandler_10: Z-targeted A: jump kick forward, sidehops/backflip otherwise.
s32 MergeShip_Zora_ActionHandler_10(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A) &&
        (play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_2) && (sFloorType != 7) &&
        (SurfaceType_GetFloorEffect(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) != FLOOR_EFFECT_1)) {
        s32 controlStickDirection = this->controlStickDirections[this->controlStickDataIndex];

        if (controlStickDirection <= PLAYER_STICK_DIR_FORWARD) {
            if (Player_IsZTargeting(this)) {
                if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) && MergeShip_Zora_HasMeleeWeapon(this) &&
                    Player_CanUpdateItems(this)) {
                    MergeShip_Zora_func_808395F0(play, this, MM_PLAYER_MWA_JUMPSLASH_START, 5.0f, 5.0f);
                } else {
                    // MM func_80839A84 is the Deku's; for the Zora MM rolls (func_80836B3C).
                    Player_SetupRoll(this, play);
                }

                return true;
            }
        } else {
            func_8083BCD0(this, play, controlStickDirection);
            return true;
        }
    }

    return false;
}

// MM func_8083CBC4: air control. MM's unk_B50 is the current run speed limit (MM Player_Update keeps it at
// R_RUN_SPEED_LIMIT scaled by the floor); it is used as that limit here when the port has not set it.
static s32 MergeShip_Zora_func_8083CBC4(Player* this, f32 arg1, s16 arg2, f32 arg3, f32 arg4, f32 arg5, s16 arg6) {
    s16 temp_v0 = this->yaw - arg2;
    f32 speedLimit = (this->unk_B50 > 0.0f) ? this->unk_B50 : (R_RUN_SPEED_LIMIT / 100.0f);

    if ((speedLimit * 1.5f) < fabsf(this->linearVelocity)) {
        arg5 *= 4.0f;
        arg3 *= 4.0f;
    }

    if (ABS(temp_v0) > 0x6000) {
        if (!Math_StepToF(&this->linearVelocity, 0.0f, arg3)) {
            return false;
        }

        this->yaw = arg2;
    } else {
        Math_AsymStepToF(&this->linearVelocity, arg1, arg4, arg5);
        Math_ScaledStepToS(&this->yaw, arg2, arg6);
    }

    return true;
}

// MM Player_Action_29: the jump kick in the air, then its landing.
void MergeShip_Zora_Action_29(Player* this, PlayState* play) {
    struct_80854190* attackInfoEntry = MergeShip_Zora_AttackAnimInfo(gMergeShipZora.meleeWeaponAnimation);
    f32 speedTarget;
    s16 yawTarget;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    this->actor.gravity = -0.8f;

    LinkAnimation_Update(play, &this->skelAnime);

    if (!MergeShip_Zora_func_808401F4(play, this)) {
        MergeShip_Zora_func_8083FCF0(play, this, 6.0f, attackInfoEntry->unk_0C, attackInfoEntry->unk_0D);
        if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
            MergeShip_Zora_func_8083CBC4(this, speedTarget, this->yaw, 1.0f, 0.05f, 0.1f, 200);
        } else if (func_80843E64(play, this) >= 0) { // MM func_80836F10: the player did not die of the fall
            s32 mwa = gMergeShipZora.meleeWeaponAnimation + 3;

            MergeShip_Zora_func_80833864(play, this, mwa);
            this->unk_845 = 3;
            this->meleeWeaponState = 0;
            MergeShip_PlayMMFloorLandSfx(this);
        }
    }
}

// ============================================================================================================
// Guard and put-away action handlers
// ============================================================================================================

// MM Player_ActionHandler_11, the Zora's path: R raises the fins as a guard when nothing is targeted.
s32 MergeShip_Zora_ActionHandler_11(Player* this, PlayState* play) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R) && (this->unk_6AD == 0) &&
        (play->shootingGalleryStatus == 0)) {
        if (!(this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) && !Player_FriendlyLockOnOrParallel(this) &&
            (this->focusActor == NULL)) {
            func_80832318(this);
            Player_DetachHeldActor(play, this);
            if (Player_SetupAction(play, this, Player_Action_80843188, 0)) {
                LinkAnimationHeader* anim;
                f32 endFrame;

                this->stateFlags1 |= PLAYER_STATE1_SHIELDING;

                Player_SetModelsForHoldingShield(this);
                anim = GET_PLAYER_ANIM(PLAYER_ANIMGROUP_defense, this->modelAnimType);

                if (anim != this->skelAnime.animation) {
                    if (Player_CheckHostileLockOn(this)) {
                        this->unk_86C = 1.0f; // MM unk_B3C
                    } else {
                        this->unk_86C = 0.0f;
                        func_80833C3C(this); // MM func_8082FC60
                    }
                    this->upperLimbRot.x = 0;
                    this->upperLimbRot.y = 0;
                    this->upperLimbRot.z = 0;
                }

                endFrame = Animation_GetLastFrame(anim);
                LinkAnimation_Change(play, &this->skelAnime, anim, PLAYER_ANIM_ADJUSTED_SPEED, endFrame, endFrame,
                                     ANIMMODE_ONCE, 0.0f);
                MergeShip_Zora_func_80830AE8(this);
            }

            return true;
        }
    }

    return false;
}

static s32 MergeShip_Zora_IsAction54(Player* this);

// MM func_8083A04C: in water, B sinks the Zora to walk on the bottom and A makes it swim again.
static void MergeShip_Zora_func_8083A04C(Player* this) {
    if (gMergeShipPlayerExt.mmBoots == MM_BOOTS_ZORA_UNDERWATER) {
        if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A)) {
            gMergeShipPlayerExt.mmBoots = MM_BOOTS_ZORA_LAND;
        }
        if (MergeShip_Zora_IsAction54(this)) {
            this->av2.actionVar2 = 20;
        }
    } else {
        if (CHECK_BTN_ALL(sControlInput->press.button, BTN_B)) {
            gMergeShipPlayerExt.mmBoots = MM_BOOTS_ZORA_UNDERWATER;
        }
    }
}

// MM Player_ActionHandler_14 (last in MM's sActionHandlerList11, the swimming list).
static s32 MergeShip_Zora_ActionHandler_14(Player* this, PlayState* play) {
    if (!sUpperBodyIsBusy && ZORA_IS(this)) {
        MergeShip_Zora_func_8083A04C(this);
    }
    return false;
}

// MM Player_ActionHandler_6, the Zora's path: in water A/B toggle swimming and walking on the bottom; on land
// A rolls or puts the fins away. Returns true when an action was chosen.
s32 MergeShip_Zora_ActionHandler_Roll(Player* this, PlayState* play) {
    if (!sUpperBodyIsBusy && !(this->stateFlags1 & PLAYER_STATE1_ON_HORSE) && !Player_UpdateHostileLockOn(this)) {
        if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
            MergeShip_Zora_func_8083A04C(this);
        } else if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A) && !Player_UpdateHostileLockOn(this)) {
            if (Player_TryRoll(this, play)) { // MM func_80839800
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

// MM sActionHandlerList11 = Ocarina of Time's plus MM's Player_ActionHandler_14 at the end.
static s32 MergeShip_Zora_TryActionHandlerList11(PlayState* play, Player* this, s32 updateUpperBody) {
    if (Player_TryActionHandlerList(play, this, sActionHandlerList11, updateUpperBody)) {
        return true;
    }
    if (!(this->stateFlags1 & (PLAYER_STATE1_LOADING | PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) &&
        !(this->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) &&
        (Player_UpperAction_ChangeHeldItem != this->upperActionFunc)) {
        return MergeShip_Zora_ActionHandler_14(this, play);
    }
    return false;
}

// ============================================================================================================
// Swimming
// ============================================================================================================

void MergeShip_Zora_Action_27(Player* this, PlayState* play);
void MergeShip_Zora_Action_28(Player* this, PlayState* play);
void MergeShip_Zora_Action_54(Player* this, PlayState* play);
void MergeShip_Zora_Action_56(Player* this, PlayState* play);
void MergeShip_Zora_Action_57(Player* this, PlayState* play);
void MergeShip_Zora_Action_58(Player* this, PlayState* play);
void MergeShip_Zora_Action_59(Player* this, PlayState* play);
void MergeShip_Zora_Action_60(Player* this, PlayState* play);
void MergeShip_Zora_Action_61(Player* this, PlayState* play);

static s32 MergeShip_Zora_IsAction54(Player* this) {
    return this->actionFunc == MergeShip_Zora_Action_54;
}

// True while the Zora is in one of MM's swimming actions (MM func_8083BB4C's exclusion list).
s32 MergeShip_Zora_IsSwimAction(Player* this) {
    return (this->actionFunc == MergeShip_Zora_Action_54) || (this->actionFunc == MergeShip_Zora_Action_56) ||
           (this->actionFunc == MergeShip_Zora_Action_57) || (this->actionFunc == MergeShip_Zora_Action_58) ||
           (this->actionFunc == MergeShip_Zora_Action_59) || (this->actionFunc == MergeShip_Zora_Action_60) ||
           (this->actionFunc == MergeShip_Zora_Action_61) || (this->actionFunc == Player_Action_8084D7C4) ||
           (this->actionFunc == Player_Action_8084E30C) || (this->actionFunc == Player_Action_8084E368) ||
           (this->actionFunc == Player_Action_8084B1D8);
}

// MM func_8084748C: swim speed and turning.
static void MergeShip_Zora_func_8084748C(Player* this, f32* speed, f32 speedTarget, s16 yawTarget) {
    f32 incrStep = this->skelAnime.curFrame - 10.0f;
    f32 maxSpeed = (R_RUN_SPEED_LIMIT / 100.0f) * 0.8f;

    if (*speed > maxSpeed) {
        *speed = maxSpeed;
    }

    if ((0.0f < incrStep) && (incrStep < 16.0f)) {
        incrStep = fabsf(incrStep) * 0.5f;
    } else {
        speedTarget = 0.0f;
        incrStep = 0.0f;
    }
    Math_AsymStepToF(speed, speedTarget * 0.8f, incrStep, (fabsf(*speed) * 0.02f) + 0.05f);
    Math_ScaledStepToS(&this->yaw, yawTarget, 0x640);
}

// MM func_808475B4: buoyancy (and sinking while walking on the bottom).
static void MergeShip_Zora_func_808475B4(Player* this) {
    f32 sp4;
    f32 temp_fv0;
    f32 var_ft4 = -5.0f;
    f32 var_ft5 = this->ageProperties->unk_28;
    f32 var_ft5_4;

    temp_fv0 = this->actor.yDistToWater - var_ft5;
    if (this->actor.velocity.y < 0.0f) {
        var_ft5 += 1.0f;
    }

    if (this->actor.yDistToWater < var_ft5) {
        temp_fv0 = CLAMP(temp_fv0, -0.4f, -0.1f);
        sp4 = temp_fv0 - ((this->actor.velocity.y <= 0.0f) ? 0.0f : this->actor.velocity.y * 0.5f);
    } else {
        if (!(this->stateFlags1 & PLAYER_STATE1_DEAD) &&
            (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) && (this->actor.velocity.y >= -5.0f)) {
            sp4 = -0.3f;
        } else {
            var_ft4 = 2.0f;
            var_ft5_4 = CLAMP(temp_fv0, 0.1f, 0.4f);
            sp4 = ((this->actor.velocity.y >= 0.0f) ? 0.0f : this->actor.velocity.y * -0.3f) + var_ft5_4;
        }

        if (this->actor.yDistToWater > 100.0f) {
            this->stateFlags2 |= PLAYER_STATE2_UNDERWATER;
        }
    }

    this->actor.velocity.y += sp4;
    if (((this->actor.velocity.y - var_ft4) * sp4) > 0.0f) {
        this->actor.velocity.y = var_ft4;
    }
    this->actor.gravity = 0.0f;
}

// MM calls func_808475B4 / func_8084748C wherever Ocarina of Time calls its own func_8084B000 / func_8084AEEC
// (aiming in the water, MM Player_Action_43; hurt, talking or getting an item in the water; ...). Ocarina of
// Time's buoyancy only sinks the iron boots, so it floated the Zora walking on the bottom up to the surface as
// soon as it aimed its fins; these send the Zora to MM's.
void MergeShip_Zora_Buoyancy(Player* this) {
    MergeShip_Zora_func_808475B4(this);
}

void MergeShip_Zora_SwimSpeed(Player* this, f32* speed, f32 speedTarget, s16 yawTarget) {
    MergeShip_Zora_func_8084748C(this, speed, speedTarget, yawTarget);
}

// MM func_808477D0: swim animation speed follows the swim speed (faster on a button press).
static void MergeShip_Zora_func_808477D0(PlayState* play, Player* this, Input* input, f32 arg3) {
    f32 var_fv0;

    if ((input != NULL) && CHECK_BTN_ANY(input->press.button, BTN_B | BTN_A)) {
        var_fv0 = 1.0f;
    } else {
        var_fv0 = 0.5f;
    }

    var_fv0 *= arg3;
    var_fv0 = CLAMP(var_fv0, 1.0f, 2.5f);
    this->skelAnime.playSpeed = var_fv0;

    LinkAnimation_Update(play, &this->skelAnime);
}

// MM D_8085D5DC.
static AnimSfxEntry sZoraSwimSfx[] = {
    { MM_NA_SE_PL_SWIM, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 0) },
};

// MM func_80847FF8 (MM func_80847F1C, pushing Mikau's En_Zog, has no Hyrule counterpart).
static void MergeShip_Zora_func_80847FF8(Player* this, f32* arg1, f32 arg2, s16 arg3) {
    MergeShip_Zora_func_8084748C(this, arg1, arg2, arg3);
    MergeShip_PlayMMAnimSfxList(this, sZoraSwimSfx);
}

// MM func_80848048.
static void MergeShip_Zora_func_80848048(PlayState* play, Player* this) {
    Player_SetupAction(play, this, MergeShip_Zora_Action_58, 0);
    Player_AnimChangeLoopSlowMorph(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim));
}

// MM func_80848094: strafing while Z-targeted.
static s32 MergeShip_Zora_func_80848094(PlayState* play, Player* this, f32* arg2, s16* arg3) {
    LinkAnimationHeader* anim;
    s16 temp_v0 = this->yaw - *arg3;
    s32 temp_v0_2;

    if (ABS(temp_v0) > 0x6000) {
        anim = MM_ANIM(gPlayerAnim_link_swimer_swim_wait);
        if (Math_StepToF(&this->linearVelocity, 0.0f, 1.0f)) {
            this->yaw = *arg3;
        } else {
            *arg2 = 0.0f;
            *arg3 = this->yaw;
        }
    } else {
        temp_v0_2 = func_8083FD78(this, arg2, arg3, play); // MM func_8083E514
        if (temp_v0_2 > 0) {
            anim = MM_ANIM(gPlayerAnim_link_swimer_swim);
        } else if (temp_v0_2 < 0) {
            anim = MM_ANIM(gPlayerAnim_link_swimer_back_swim);
        } else {
            s16 diff = this->actor.shape.rot.y - *arg3;

            if (diff > 0) {
                anim = MM_ANIM(gPlayerAnim_link_swimer_Rside_swim);
            } else {
                anim = MM_ANIM(gPlayerAnim_link_swimer_Lside_swim);
            }
        }
    }

    if (anim != this->skelAnime.animation) {
        Player_AnimChangeLoopSlowMorph(play, this, anim);
        return true;
    }
    return false;
}

// MM func_808481CC: diving movement.
static void MergeShip_Zora_func_808481CC(PlayState* play, Player* this, f32 arg2) {
    f32 speedTarget;
    s16 yawTarget;

    Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
    MergeShip_Zora_func_8084748C(this, &this->linearVelocity, speedTarget / 2.0f, yawTarget);
    MergeShip_Zora_func_8084748C(this, &this->actor.velocity.y, arg2, this->yaw);
}

// MM func_808353DC: back to treading water.
void MergeShip_Zora_func_808353DC(PlayState* play, Player* this) {
    Player_SetupAction(play, this, MergeShip_Zora_Action_54, 0);
    Player_AnimChangeLoopSlowMorph(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim_wait));
}

// MM func_8083B73C: start swimming.
void MergeShip_Zora_func_8083B73C(PlayState* play, Player* this, s16 yaw) {
    Player_SetupAction(play, this, MergeShip_Zora_Action_57, 0);
    Player_AnimChangeLoopSlowMorph(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim));
    this->actor.shape.rot.y = yaw;
    this->yaw = yaw;
}

// MM func_8083B798, the Zora's path: keep swimming (animation run backwards) after a dive.
static void MergeShip_Zora_func_8083B798(PlayState* play, Player* this) {
    Player_SetupAction(play, this, MergeShip_Zora_Action_57, 0);
    LinkAnimation_Change(play, &this->skelAnime, MM_ANIM(gPlayerAnim_link_swimer_swim), 1.0f,
                         Animation_GetLastFrame(MM_ANIM(gPlayerAnim_link_swimer_swim)), 0.0f, ANIMMODE_LOOP, 0.0f);
    this->unk_B48 = 2.0f;
    this->unk_6C2 = 0x3E80; // MM unk_AAA
}

// MM func_8083B850: start the Zora's fast swim.
static void MergeShip_Zora_func_8083B850(PlayState* play, Player* this) {
    gMergeShipPlayerExt.mmBoots = MM_BOOTS_ZORA_LAND;
    gMergeShipZora.prevBoots = MM_BOOTS_ZORA_LAND;
    Player_SetupAction(play, this, MergeShip_Zora_Action_56, 0);
    this->unk_B48 = sqrtf(SQ(this->linearVelocity) + SQ(this->actor.velocity.y));
    MergeShip_Zora_OverrideBlureColors(play, this, 1, 8);
    gMergeShipPlayerExt.mmBoots = MM_BOOTS_ZORA_LAND;
    gMergeShipZora.prevBoots = MM_BOOTS_ZORA_LAND;
}

// MM func_8083B8D0: splash on entering the water.
static void MergeShip_Zora_func_8083B8D0(PlayState* play, Player* this) {
    if (func_8083CFA8(play, this, this->actor.velocity.y, 500)) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_DIVE_INTO_WATER);
        if (this->fallDistance > 800) {
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
        }
    }
}

// MM func_8083B930: entering the water.
void MergeShip_Zora_func_8083B930(PlayState* play, Player* this) {
    if ((gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        func_80832564(play, this); // MM func_8082DE50

        if (MergeShip_Zora_Action_28 == this->actionFunc) {
            MergeShip_Zora_func_8083B850(play, this);
            this->stateFlags3 |= PLAYER_STATE3_8000;
        } else if ((MergeShip_Zora_Action_27 == this->actionFunc) ||
                   (Player_Action_80844A44 == this->actionFunc)) {
            MergeShip_Zora_func_8083B850(play, this);
            this->stateFlags3 |= PLAYER_STATE3_8000;
            Player_AnimPlayLoopAdjusted(play, this, MM_ANIM(gPlayerAnim_pz_fishswim));
        } else if ((gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER) &&
                   (this->stateFlags2 & PLAYER_STATE2_UNDERWATER)) {
            this->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
            MergeShip_Zora_func_8083B3B4(play, this, NULL);
            this->av1.actionVar1 = 1;
        } else {
            Player_SetupAction(play, this, MergeShip_Zora_Action_54, 1);
            Player_AnimChangeOnceMorph(play, this,
                                       (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)
                                           ? MM_ANIM(gPlayerAnim_link_swimer_wait2swim_wait)
                                           : MM_ANIM(gPlayerAnim_link_swimer_land2swim_wait));
        }
    }
    if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) || (this->actor.yDistToWater < this->ageProperties->unk_2C)) {
        MergeShip_Zora_func_8083B8D0(play, this);
    }

    this->stateFlags1 |= PLAYER_STATE1_IN_WATER;
    this->stateFlags2 |= PLAYER_STATE2_UNDERWATER;
    this->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);

    this->unk_854 = 0.0f; // MM unk_AEC
    Player_SetBootData(play, this);
}

// MM func_8083B3B4, the Zora's path. MM's first block (A dives from the surface) excludes the Zora, whose
// A is the fast swim; what remains is surfacing (after walking on the bottom, a dive, or a fast swim that
// breaches: the jump out of the water, MM Player_Action_28).
s32 MergeShip_Zora_func_8083B3B4(PlayState* play, Player* this, Input* input) {
    if (((this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) ||
         ((this->stateFlags2 & PLAYER_STATE2_UNDERWATER) &&
          (((MergeShip_Zora_Action_56 != this->actionFunc) && !(this->stateFlags3 & PLAYER_STATE3_8000)) ||
           (this->unk_6C2 < -0x1555)))) &&
        ((this->actor.yDistToWater - this->actor.velocity.y) < this->ageProperties->unk_30)) {
        s32 temp_v0_3;
        s16 sp2A;
        f32 sp24;

        this->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
        func_80832340(play, this); // MM func_8082DC64
        temp_v0_3 = func_8083CFA8(play, this, this->actor.velocity.y, 0x1F4);
        if (this->stateFlags3 & PLAYER_STATE3_8000) {
            sp2A = this->unk_B86[1];
            sp24 = this->unk_B48 * 1.5f;
            Player_SetupAction(play, this, MergeShip_Zora_Action_28, 1);
            this->stateFlags3 |= PLAYER_STATE3_8000;
            this->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
            sp24 = CLAMP_MAX(sp24, 13.5f);
            this->linearVelocity = Math_CosS(this->unk_6C2) * sp24;
            this->actor.velocity.y = -Math_SinS(this->unk_6C2) * sp24;
            this->unk_B86[1] = sp2A;
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_JUMP_OUT_WATER);
            return true;
        }

        (void)temp_v0_3;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_FACE_UP);

        if (input != NULL) {
            Player_SetupAction(play, this, MergeShip_Zora_Action_60, 1);
            if (this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) {
                this->stateFlags1 |= (PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_CUTSCENE);
            }
            this->av2.actionVar2 = 2;
        }

        Player_AnimChangeOnceMorph(play, this,
                                   (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)
                                       ? MM_ANIM(gPlayerAnim_link_swimer_swim_get)
                                       : MM_ANIM(gPlayerAnim_link_swimer_swim_deep_end));
        return true;
    }

    return false;
}

// MM func_8083BB4C, the Zora's path: the per-frame water check.
void MergeShip_Zora_func_8083BB4C(PlayState* play, Player* this) {
    f32 sp1C = this->actor.yDistToWater - this->ageProperties->unk_2C;

    if (sp1C < 0.0f) {
        this->underwaterTimer = 0;
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            gMergeShipPlayerExt.mmBoots = MM_BOOTS_ZORA_LAND;
        }
        Audio_SetBaseFilter(0);
    } else {
        Audio_SetBaseFilter(0x20);
        // The Zora never runs out of breath.
        this->underwaterTimer = 0;
    }

    if ((this->actor.parent == NULL) && (Player_Action_80845668 != this->actionFunc) &&
        (Player_Action_8084BDFC != this->actionFunc) &&
        ((MergeShip_Zora_Action_28 != this->actionFunc) || (this->actor.velocity.y < -2.0f))) {
        if (this->ageProperties->unk_2C < this->actor.yDistToWater) {
            if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
                (((gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER) ||
                  !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) &&
                 !MergeShip_Zora_IsSwimAction(this) && (Player_Action_8084DC48 != this->actionFunc) &&
                 (Player_Action_8084E1EC != this->actionFunc))) {
                MergeShip_Zora_func_8083B930(play, this);
            }
        } else if ((this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
                   (this->actor.yDistToWater < this->ageProperties->unk_24) &&
                   (((MergeShip_Zora_Action_56 != this->actionFunc) && !(this->stateFlags3 & PLAYER_STATE3_8000)) ||
                    (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
            if (this->skelAnime.movementFlags == 0) {
                Player_SetupTurnInPlace(play, this, this->actor.shape.rot.y);
            }
            func_8083D0A8(play, this, this->actor.velocity.y); // MM func_8083B32C
        }
    }
}

// MergeShip option CVAR_ENHANCEMENT("MergeShip.ZoraWaterfallGate") (default on; off is MM's behaviour). Hyrule's
// waterfalls are steps between water boxes (Zora's River climbs -60, 60, 120, 180, 250, 300, 500); a water box
// is water from its surface all the way down, so a Zora that gets under or over a step into the higher box -- by
// diving under the lip, or with the breach out of the fast swim (MM Player_Action_28) -- swims up the waterfall
// as MM's Zora would, and out at its top. With the gate on, the step is a wall for the swimming Zora: moving
// into water whose surface is more than MERGESHIP_ZORA_WATERFALL_STEP above the water it swims in, where it is
// deep enough to swim (ageProperties->unk_2C, MM's swim depth), or over any such water during the breach, keeps
// the previous x/z. Wading up shallow rapids on the bottom, and leaving the water onto land, are unaffected.
// Called after the move, before the scene collision.
#define MERGESHIP_ZORA_WATERFALL_STEP 40.0f
static f32 sMergeShipZoraGateSurface; // surface of the water the Zora last swam in (or breached from)
static s32 sMergeShipZoraGateValid;

void MergeShip_Zora_WaterfallGate(PlayState* play, Player* this) {
    WaterBox* waterBox;
    f32 newSurface;
    f32 prevSurface;
    s32 swimming;
    s32 breaching;

    if ((this->mergeShipForm != PLAYER_FORM_ZORA) ||
        !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ZoraWaterfallGate"), 1) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD))) {
        sMergeShipZoraGateValid = false;
        return;
    }

    swimming = (this->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
    breaching = (this->actionFunc == MergeShip_Zora_Action_28);

    newSurface = this->actor.world.pos.y;
    if (WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &newSurface,
                             &waterBox)) {
        if (swimming) {
            prevSurface = this->actor.prevPos.y;
            if (!WaterBox_GetSurface1(play, &play->colCtx, this->actor.prevPos.x, this->actor.prevPos.z,
                                      &prevSurface, &waterBox)) {
                prevSurface = newSurface;
            }
        } else if (breaching && sMergeShipZoraGateValid) {
            prevSurface = sMergeShipZoraGateSurface;
        } else {
            prevSurface = newSurface;
        }

        // (A breach may not land in the higher water at all, even over its surface: that is the jump up the fall.)
        if ((swimming || breaching) && (newSurface > (prevSurface + MERGESHIP_ZORA_WATERFALL_STEP)) &&
            (breaching || ((newSurface - this->actor.world.pos.y) > this->ageProperties->unk_2C))) {
            MergeShipTest_Log("Z gate: blocked (%.0f %.0f %.0f) surface %.0f -> %.0f", this->actor.world.pos.x,
                              this->actor.world.pos.y, this->actor.world.pos.z, prevSurface, newSurface);
            this->actor.world.pos.x = this->actor.prevPos.x;
            this->actor.world.pos.z = this->actor.prevPos.z;
            return;
        }
        if (swimming) {
            sMergeShipZoraGateSurface = newSurface;
            sMergeShipZoraGateValid = true;
        }
    }

    if (!swimming && !breaching) {
        sMergeShipZoraGateValid = false;
    }
}

// MM func_8083BF54, the water half (ripples and splashes at the surface, bubbles below), for the Zora.
void MergeShip_Zora_func_8083BF54(PlayState* play, Player* this) {
    Vec3f sp84;

    if ((this->stateFlags3 & PLAYER_STATE3_CHECK_FLOOR_WATER_COLLISION) &&
        (this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
        if (this->actor.yDistToWater < 50.0f) {
            f32 temp_fv1_5;
            Vec3f* bodyPartsPos;
            f32 var_fa0_3;
            f32 var_ft4_2;

            var_ft4_2 = fabsf(this->bodyPartsPos[PLAYER_BODYPART_WAIST].x - this->unk_A88.x) +
                        fabsf(this->bodyPartsPos[PLAYER_BODYPART_WAIST].y - this->unk_A88.y) +
                        fabsf(this->bodyPartsPos[PLAYER_BODYPART_WAIST].z - this->unk_A88.z);
            var_ft4_2 = CLAMP_MAX(var_ft4_2, 4.0f);

            this->unk_854 += var_ft4_2;
            if (this->unk_854 > 15.0f) {
                this->unk_854 = 0.0f;
                sp84.x = (Rand_ZeroOne() * 10.0f) + this->actor.world.pos.x;
                sp84.y = this->actor.world.pos.y + this->actor.yDistToWater;
                sp84.z = (Rand_ZeroOne() * 10.0f) + this->actor.world.pos.z;

                EffectSsGRipple_Spawn(play, &sp84, 100, 500, 0);

                if ((this->linearVelocity > 4.0f) && !MergeShip_Zora_IsSwimming(this) &&
                    ((this->actor.world.pos.y + this->actor.yDistToWater) <
                     this->bodyPartsPos[PLAYER_BODYPART_WAIST].y)) {
                    func_8083CFA8(play, this, 20.0f,
                                  (fabsf(this->linearVelocity) * 50.0f) + (this->actor.yDistToWater * 5.0f));
                } else if (this->stateFlags3 & PLAYER_STATE3_8000) {
                    s32 i;

                    var_fa0_3 = (this->actor.world.pos.y + this->actor.yDistToWater) - 5.0f;
                    bodyPartsPos = this->bodyPartsPos;

                    for (i = 0; i < PLAYER_BODYPART_MAX; i++, bodyPartsPos++) {
                        temp_fv1_5 = bodyPartsPos->y - var_fa0_3;

                        if (temp_fv1_5 > 0.0f) {
                            func_8083CFA8(play, this, 20.0f,
                                          fabsf(this->linearVelocity) * 20.0f + (temp_fv1_5 * 10.0f));
                        }
                    }
                }
            }
        }

        if (this->ageProperties->unk_2C < this->actor.yDistToWater) {
            s32 numBubbles = 0;
            s32 i;
            f32 var_fv1;

            var_fv1 = (this->stateFlags1 & PLAYER_STATE1_DAMAGED)
                          ? -fabsf(this->linearVelocity)
                          : ((MergeShip_Zora_Action_56 == this->actionFunc)
                                 ? (ABS(this->unk_B8A) * -0.004f) + (this->unk_B48 * -0.38f)
                                 : this->actor.velocity.y);

            if ((var_fv1 > -1.0f) || ((gMergeShipPlayerExt.mmBoots == MM_BOOTS_ZORA_UNDERWATER) &&
                                      (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
                if (Rand_ZeroOne() < 0.2f) {
                    numBubbles = 1;
                }
            } else {
                numBubbles = var_fv1 * -0.3f;
                if (numBubbles > 8) {
                    numBubbles = 8;
                }
            }

            for (i = 0; i < numBubbles; i++) {
                EffectSsBubble_Spawn(play, &this->actor.world.pos, 20.0f, 10.0f, 20.0f, 0.13f);
            }
        }
    }
}

// MM func_80850734: A while swimming starts the fast swim (a roll into pz_waterroll).
static s32 MergeShip_Zora_func_80850734(PlayState* play, Player* this) {
    if ((this->windSpeed == 0.0f) && (gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER) &&
        CHECK_BTN_ALL(sControlInput->cur.button, BTN_A)) {
        MergeShip_Zora_func_8083B850(play, this);
        this->stateFlags2 |= PLAYER_STATE2_UNDERWATER;
        LinkAnimation_Change(play, &this->skelAnime, MM_ANIM(gPlayerAnim_pz_waterroll), PLAYER_ANIM_ADJUSTED_SPEED, 4.0f,
                             Animation_GetLastFrame(MM_ANIM(gPlayerAnim_pz_waterroll)), ANIMMODE_ONCE, -6.0f);
        this->av2.actionVar2 = 5;
        this->unk_B86[0] = 0;
        this->unk_B48 = this->linearVelocity;
        this->actor.velocity.y = 0.0f;
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_ZORA_SWIM_DASH);
        return true;
    }
    return false;
}

// MM Player_Action_54: treading water / walking on the bottom.
void MergeShip_Zora_Action_54(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    func_80832CB0(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim_wait));
    MergeShip_Zora_func_808475B4(this);

    if (this->av2.actionVar2 != 0) {
        this->av2.actionVar2--;
    }

    MergeShip_Zora_func_8082F164(this, BTN_R);

    if (CHECK_BTN_ALL(sControlInput->press.button, BTN_A)) {
        this->av2.actionVar2 = 0;
    }

    if (!Player_IsTalking(play) && !MergeShip_Zora_TryActionHandlerList11(play, this, true) &&
        !MergeShip_Zora_func_8083B3B4(play, this, sControlInput) &&
        ((this->av2.actionVar2 != 0) || !MergeShip_Zora_func_80850734(play, this))) {
        speedTarget = 0.0f;
        yawTarget = this->actor.shape.rot.y;

        // MM: `unk_AA5 > PLAYER_UNKAA5_2` (aiming, cutscene or ocarina) is reset; those are Ocarina of Time's
        // unk_6AD 2 and above.
        if (this->unk_6AD >= 2) {
            this->unk_6AD = 0;
        }

        if (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) {
            if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
                func_8083A098(this, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_short_landing, this->modelAnimType), play);
                MergeShip_PlayMMFloorLandSfx(this);
            }
        } else {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

            if (speedTarget != 0.0f) {
                s16 diff = this->actor.shape.rot.y - yawTarget;

                if ((ABS(diff) > 0x6000) && !Math_StepToF(&this->linearVelocity, 0.0f, 1.0f)) {
                    return;
                }

                if (Player_IsZTargetingWithHostileUpdate(this)) {
                    MergeShip_Zora_func_80848048(play, this);
                } else {
                    MergeShip_Zora_func_8083B73C(play, this, yawTarget);
                }
            }
        }

        MergeShip_Zora_func_8084748C(this, &this->linearVelocity, speedTarget, yawTarget);
    }
}

// MM func_80850BA8.
static void MergeShip_Zora_func_80850BA8(Player* this) {
    this->linearVelocity = Math_CosS(this->unk_6C2) * this->unk_B48;
    this->actor.velocity.y = -Math_SinS(this->unk_6C2) * this->unk_B48;
}

// MM func_80850BF8: fast swim speed and steering.
static void MergeShip_Zora_func_80850BF8(Player* this, f32 arg1) {
    f32 temp_fv0;
    s16 temp_ft0;
    s8 stickX = sControlInput->rel.stick_x;

    Math_AsymStepToF(&this->unk_B48, arg1, 1.0f, (fabsf(this->unk_B48) * 0.01f) + 0.4f);
    temp_fv0 = Math_CosS(stickX * 0x10E);

    temp_ft0 = (((stickX >= 0) ? 1 : -1) * (1.0f - temp_fv0) * -1100.0f);
    temp_ft0 = CLAMP(temp_ft0, -0x1F40, 0x1F40);

    this->yaw += temp_ft0;
}

// MM func_80850D20 -> func_8083F8A8(play, this, 12.0f, -1, 1.0f, 160, 20, true): stir up the floor when
// swimming fast near it (MM's negative count takes its first branch: one dust puff, as MM's frame budget
// allows at 20 fps).
static void MergeShip_Zora_func_80850D20(PlayState* play, Player* this) {
    static Vec3f sDustVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f sDustAccel = { 0.0f, 0.04f, 0.0f };      // MM D_8085D270
    static Color_RGBA8 sDustColor = { 255, 255, 255, 255 }; // MM D_8085D26C
    Vec3f pos;

    // MM func_8083F828(&world.pos, &pos, 0.0f, 40.0f, 10.0f).
    pos.x = Rand_CenteredFloat(40.0f) + this->actor.world.pos.x;
    pos.y = Rand_CenteredFloat(10.0f) + this->actor.world.pos.y;
    pos.z = Rand_CenteredFloat(40.0f) + this->actor.world.pos.z;
    sDustVelocity.x = Rand_CenteredFloat(3.0f);
    sDustVelocity.z = Rand_CenteredFloat(3.0f);
    EffectSsDust_Spawn(play, 0, &pos, &sDustVelocity, &sDustAccel, &sDustColor, &sDustColor, 160, 20, 42, 0);
}

// MM func_80840A30, the fast swim's part: swimming into a wall (or a tree's trunk) bounces the Zora off.
static s32 MergeShip_Zora_func_80840A30(PlayState* play, Player* this, f32* arg2, f32 arg3) {
    Actor* cylinderOc = NULL;

    if (arg3 <= *arg2) {
        if (((this->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) && (sWorldYawToTouchedWall < 0x1C00)) ||
            (((this->cylinder.base.ocFlags1 & OC1_HIT) && (cylinderOc = this->cylinder.base.oc) != NULL) &&
             (cylinderOc->id == ACTOR_EN_WOOD02) &&
             (ABS((s16)(this->actor.world.rot.y - cylinderOc->yawTowardsPlayer)) > 0x6000))) {
            if (!MergeShip_Zora_func_8082DA90(play)) {
                if (cylinderOc != NULL) {
                    cylinderOc->home.rot.y = 1;
                } else if (this->actor.wallBgId != BGCHECK_SCENE) {
                    DynaPolyActor* wallPolyActor = DynaPoly_GetActor(&play->colCtx, this->actor.wallBgId);

                    if ((wallPolyActor != NULL) && (wallPolyActor->actor.id == ACTOR_OBJ_KIBAKO2)) {
                        wallPolyActor->actor.home.rot.z = 1;
                    }
                }

                if ((this->stateFlags3 & PLAYER_STATE3_8000) && (MergeShip_Zora_Action_28 != this->actionFunc)) {
                    Player_SetupAction(play, this, MergeShip_Zora_Action_61, 0);
                    Player_AnimPlayOnceAdjusted(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim_hit));
                    func_80832440(play, this); // MM func_8082DD2C
                    this->linearVelocity *= 0.2f;
                } else {
                    // MM Player_Action_26 (the roll) with its bonk flag.
                    Player_SetupAction(play, this, Player_Action_Roll, 0);
                    Player_AnimPlayOnce(play, this, GET_PLAYER_ANIM(PLAYER_ANIMGROUP_hip_down, this->modelAnimType));
                    this->av2.actionVar2 = 1;
                }

                this->linearVelocity = -this->linearVelocity;
                Player_RequestQuake(play, 33267, 3, 12);
                Player_RequestRumble(this, 255, 20, 150, 0);
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BODY_HIT);
                MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
                return true;
            }
        }
    }
    return false;
}

// MM Player_Action_56: the Zora's fast swim.
void MergeShip_Zora_Action_56(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 sp42;
    s16 yawTarget;
    s16 sp3E;
    s16 sp3C;
    s16 sp3A;
    s8 stickX = sControlInput->rel.stick_x;
    s8 stickY = sControlInput->rel.stick_y;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    MergeShip_Zora_func_808475B4(this);
    MergeShip_Zora_func_8082F164(this, BTN_R);

    if (MergeShip_Zora_TryActionHandlerList11(play, this, false)) {
        return;
    }

    if (MergeShip_Zora_func_8083B3B4(play, this, sControlInput)) {
        return;
    }

    if (MergeShip_Zora_func_80840A30(play, this, &this->linearVelocity, 0.0f)) {
        return;
    }

    speedTarget = 0.0f;

    if (this->av2.actionVar2 != 0) {
        if ((!MergeShip_Zora_func_8082DA90(play) && !CHECK_BTN_ALL(sControlInput->cur.button, BTN_A)) ||
            (gMergeShipPlayerExt.mmBoots != MM_BOOTS_ZORA_LAND)) {
            this->unk_B86[0] = 1;
        }

        if (LinkAnimation_Update(play, &this->skelAnime) && (DECR(this->av2.actionVar2) == 0)) {
            if (this->unk_B86[0] != 0) {
                this->stateFlags3 &= ~PLAYER_STATE3_8000;
                Player_AnimPlayOnceAdjusted(play, this, MM_ANIM(gPlayerAnim_pz_swimtowait));
            } else {
                Player_AnimPlayLoopAdjusted(play, this, MM_ANIM(gPlayerAnim_pz_fishswim));
            }
        } else {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
            Math_ScaledStepToS(&this->yaw, yawTarget, 0x640);

            if (this->skelAnime.curFrame >= 13.0f) {
                speedTarget = 12.0f;

                if (LinkAnimation_OnFrame(&this->skelAnime, 13.0f)) {
                    this->unk_B48 = 16.0f;
                }
                this->stateFlags3 |= PLAYER_STATE3_8000;
            } else {
                speedTarget = 0.0f;
            }
        }

        Math_SmoothStepToS(&this->unk_B86[1], stickX * 0xC8, 0xA, 0x3E8, 0x64);
        Math_SmoothStepToS(&this->unk_B8E, this->unk_B86[1], IREG(40) + 1, IREG(41), IREG(42));
    } else if (this->unk_B86[0] == 0) {
        LinkAnimation_Update(play, &this->skelAnime);

        if ((!MergeShip_Zora_func_8082DA90(play) && !CHECK_BTN_ALL(sControlInput->cur.button, BTN_A)) ||
            (gMergeShipPlayerExt.mmBoots != MM_BOOTS_ZORA_LAND) || (this->windSpeed > 9.0f)) {
            this->stateFlags3 &= ~PLAYER_STATE3_8000;
            Player_AnimPlayOnceAdjusted(play, this, MM_ANIM(gPlayerAnim_pz_swimtowait));
            this->unk_B86[0] = 1;
        } else {
            speedTarget = 9.0f;
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_ZORA_SWIM_LV - SFX_FLAG);
        }

        // Y
        sp3E = stickY * 0xC8;
        if (this->unk_B8C != 0) {
            this->unk_B8C--;
            sp3E = CLAMP_MAX(sp3E, (s16)(this->floorPitch - 0xFA0));
        }

        if ((this->unk_6C2 >= -0x1555) && (this->actor.yDistToWater < (this->ageProperties->unk_24 + 10.0f))) {
            sp3E = CLAMP_MIN(sp3E, 0x7D0);
        }
        Math_SmoothStepToS(&this->unk_6C2, sp3E, 4, 0xFA0, 0x190);

        // X
        sp42 = stickX * 0x64;
        if (Math_ScaledStepToS(&this->unk_B8A, sp42, 0x384) && (sp42 == 0)) {
            Math_SmoothStepToS(&this->unk_B86[1], 0, 4, 0x5DC, 0x64);
            Math_SmoothStepToS(&this->unk_B8E, this->unk_B86[1], IREG(44) + 1, IREG(45), IREG(46));
        } else {
            sp3C = this->unk_B86[1];
            sp3A = (this->unk_B8A < 0) ? -0x3A98 : 0x3A98;
            this->unk_B86[1] += this->unk_B8A;
            Math_SmoothStepToS(&this->unk_B8E, this->unk_B86[1], IREG(47) + 1, IREG(48), IREG(49));

            if ((ABS(this->unk_B8A) > 0xFA0) && ((((sp3C + this->unk_B8A) - sp3A) * (sp3C - sp3A)) <= 0)) {
                MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_ZORA_SWIM_ROLL);
            }
        }

        if (sYDistToFloor < 20.0f) {
            MergeShip_Zora_func_80850D20(play, this);
        }
    } else {
        Math_SmoothStepToS(&this->unk_B86[1], 0, 4, 0xFA0, 0x190);
        if ((this->skelAnime.curFrame <= 5.0f) || !MergeShip_Zora_func_80850734(play, this)) {
            if (LinkAnimation_Update(play, &this->skelAnime)) {
                MergeShip_Zora_func_808353DC(play, this);
            }
        }

        // MM Player_ResetCylinder.
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

    if ((this->unk_B8C < 8) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        // (MM exempts the Beaver race's En_Twig floor; Hyrule has none.)
        this->unk_6C2 += (s16)((-this->floorPitch - this->unk_6C2) * 2);
        this->unk_B8C = 0xF;

        MergeShip_Zora_func_80850D20(play, this);
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_PL_BODY_BOUND);
    }

    MergeShip_Zora_func_80850BF8(this, speedTarget);
    MergeShip_Zora_func_80850BA8(this);
}

// MM Player_Action_57: swimming.
void MergeShip_Zora_Action_57(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;
    s16 sp30;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    MergeShip_Zora_func_808475B4(this);
    MergeShip_Zora_func_8082F164(this, BTN_R);
    if (!MergeShip_Zora_TryActionHandlerList11(play, this, true) &&
        !MergeShip_Zora_func_8083B3B4(play, this, sControlInput)) {
        MergeShip_Zora_func_808477D0(play, this, sControlInput, this->linearVelocity);
        if (MergeShip_Zora_func_8082DA90(play)) {
            speedTarget = this->linearVelocity;
            yawTarget = this->actor.shape.rot.y;
        } else {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
        }
        sp30 = this->actor.shape.rot.y - yawTarget;
        if (!MergeShip_Zora_func_80850734(play, this)) {
            if (Player_IsZTargetingWithHostileUpdate(this)) {
                MergeShip_Zora_func_80848048(play, this);
            } else if ((speedTarget == 0.0f) || (ABS(sp30) > 0x6000) ||
                       (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER)) {
                MergeShip_Zora_func_808353DC(play, this);
            }
            MergeShip_Zora_func_80847FF8(this, &this->linearVelocity, speedTarget, yawTarget);
        }
    }
}

// MM Player_Action_58: swimming while Z-targeting.
void MergeShip_Zora_Action_58(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;

    MergeShip_Zora_func_808477D0(play, this, sControlInput, this->linearVelocity);
    MergeShip_Zora_func_808475B4(this);
    MergeShip_Zora_func_8082F164(this, BTN_R);

    if (!MergeShip_Zora_TryActionHandlerList11(play, this, true) &&
        !MergeShip_Zora_func_8083B3B4(play, this, sControlInput)) {
        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

        if (speedTarget == 0.0f) {
            MergeShip_Zora_func_808353DC(play, this);
        } else if (!Player_IsZTargetingWithHostileUpdate(this)) {
            MergeShip_Zora_func_8083B73C(play, this, yawTarget);
        } else {
            MergeShip_Zora_func_80848094(play, this, &speedTarget, &yawTarget);
        }

        MergeShip_Zora_func_80847FF8(this, &this->linearVelocity, speedTarget, yawTarget);
    }
}

// MM Player_Action_59: diving (entered as a keep-swimming after a dive into the water).
void MergeShip_Zora_Action_59(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    this->actor.gravity = 0.0f;
    Player_UpdateUpperBody(this, play);
    MergeShip_Zora_func_8082F164(this, BTN_R);

    if (Player_ActionHandler_13(this, play)) {
        return;
    }

    if (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) {
        MergeShip_Zora_func_808353DC(play, this);
    } else if (this->av1.actionVar1 == 0) {
        f32 temp_fv0;

        if (this->av2.actionVar2 == 0) {
            if (LinkAnimation_Update(play, &this->skelAnime) ||
                ((this->skelAnime.curFrame >= 22.0f) && !CHECK_BTN_ALL(sControlInput->cur.button, BTN_A))) {
                MergeShip_Zora_func_8083B798(play, this);
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 20.0f)) {
                this->actor.velocity.y = -2.0f;
            }
            Player_DecelerateToZero(this);
        } else {
            MergeShip_Zora_func_808477D0(play, this, sControlInput, this->actor.velocity.y);
            this->unk_6C2 = 0x3E80;

            if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_A) && !Player_ActionHandler_2(this, play) &&
                !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (this->actor.yDistToWater < 120.0f)) {
                MergeShip_Zora_func_808481CC(play, this, -2.0f);
            } else {
                this->av1.actionVar1++;
                Player_AnimChangeLoopSlowMorph(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim_wait));
            }
        }

        temp_fv0 = (this->actor.yDistToWater - this->ageProperties->unk_30) * 0.04f;
        if (temp_fv0 < this->actor.velocity.y) {
            this->actor.velocity.y = temp_fv0;
        }
    } else if (this->av1.actionVar1 == 1) {
        LinkAnimation_Update(play, &this->skelAnime);
        MergeShip_Zora_func_808475B4(this);
        if (this->unk_6C2 < 0x2710) {
            this->av1.actionVar1++;
            this->av2.actionVar2 = this->actor.yDistToWater;
            Player_AnimChangeLoopSlowMorph(play, this, MM_ANIM(gPlayerAnim_link_swimer_swim));
        }
    } else if (!MergeShip_Zora_func_8083B3B4(play, this, sControlInput)) {
        f32 var_fv1 = (this->av2.actionVar2 * 0.018f) + 4.0f;
        Input* input = sControlInput;

        if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
            input = NULL;
        }

        MergeShip_Zora_func_808477D0(play, this, input, fabsf(this->actor.velocity.y));
        Math_ScaledStepToS(&this->unk_6C2, -0x2710, 0x320);

        var_fv1 = CLAMP_MAX(var_fv1, 8.0f);
        MergeShip_Zora_func_808481CC(play, this, var_fv1);
    }
}

// MM Player_Action_60: surfacing. Carrying something found underwater, Ocarina of Time's own surfacing
// action takes over (the item is Hyrule's).
void MergeShip_Zora_Action_60(Player* this, PlayState* play) {
    if (this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) {
        this->actionFunc = Player_Action_8084E1EC;
        Player_Action_8084E1EC(this, play);
        return;
    }

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    MergeShip_Zora_func_8082F164(this, BTN_R);
    if (((this->skelAnime.curFrame <= 1.0f) || !MergeShip_Zora_func_80850734(play, this)) &&
        LinkAnimation_Update(play, &this->skelAnime)) {
        // MM func_80848250.
        this->unk_862 = 0;
        this->stateFlags1 &= ~(PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR);
        this->getItemId = GI_NONE;
        func_8005B1A4(Play_GetCamera(play, CAM_ID_MAIN));

        MergeShip_Zora_func_808353DC(play, this);
        func_80832340(play, this);
    } else if (LinkAnimation_OnFrame(&this->skelAnime, 5.0f)) {
        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_BREATH_DRINK);
    }

    MergeShip_Zora_func_808475B4(this);
    MergeShip_Zora_func_8084748C(this, &this->linearVelocity, 0.0f, this->actor.shape.rot.y);
}

// MM Player_Action_61: bounced off a wall in the fast swim.
void MergeShip_Zora_Action_61(Player* this, PlayState* play) {
    MergeShip_Zora_func_808475B4(this);
    Math_StepToF(&this->linearVelocity, 0.0f, 0.4f);
    if (LinkAnimation_Update(play, &this->skelAnime) && (this->linearVelocity < 10.0f)) {
        MergeShip_Zora_func_808353DC(play, this);
    }
}

// MM func_80836B3C, the non-Goron path: land from the air into a roll from frame `arg2`. MM first faces the
// movement the way the body faces (its VB_PATCH_SIDEROLL hook defaults to MM's code).
static void MergeShip_Zora_func_80836B3C(PlayState* play, Player* this, f32 arg2) {
    LinkAnimationHeader* anim = GET_PLAYER_ANIM(PLAYER_ANIMGROUP_landing_roll, this->modelAnimType);

    this->yaw = this->actor.shape.rot.y;
    this->actor.world.rot.y = this->actor.shape.rot.y;

    Player_SetupAction(play, this, Player_Action_Roll, 0); // MM Player_Action_26
    LinkAnimation_Change(play, &this->skelAnime, anim, 1.25f * sWaterSpeedFactor, arg2, Animation_GetLastFrame(anim),
                         ANIMMODE_ONCE, 0.0f);
}

// MM Player_Action_27: the running dive (A held while running off a ledge above deep water).
void MergeShip_Zora_Action_27(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    if (LinkAnimation_Update(play, &this->skelAnime)) {
        Player_AnimPlayLoop(play, this, MM_ANIM(gPlayerAnim_link_normal_run_jump_water_fall_wait));
    }

    Math_StepToF(&this->linearVelocity, 0.0f, 0.05f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        if (this->fallDistance >= 400) {
            this->actor.colChkInfo.damage = 0x10;
            func_80837C0C(play, this, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE, 4.0f, 5.0f, this->actor.shape.rot.y, 20);
        } else {
            MergeShip_Zora_func_80836B3C(play, this, 4.0f);
        }
    }
}

// MM Player_Action_28: out of the water after breaching from the fast swim (the dolphin jump).
void MergeShip_Zora_Action_28(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;

    if (LinkAnimation_Update(play, &this->skelAnime)) {
        Player_AnimPlayLoopAdjusted(play, this, MM_ANIM(gPlayerAnim_pz_fishswim));
    }

    Math_SmoothStepToS(&this->unk_B86[1], 0, 6, 0x7D0, 0x190);
    if (!MergeShip_Zora_func_80840A30(play, this, &this->linearVelocity, 0.0f)) {
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            if (this->unk_6C2 > 0x36B0) {
                this->actor.colChkInfo.damage = 0x10;
                func_80837C0C(play, this, PLAYER_HIT_RESPONSE_KNOCKBACK_LARGE, 4.0f, 5.0f, this->actor.shape.rot.y,
                              20);
            } else {
                MergeShip_Zora_func_80836B3C(play, this, 4.0f);
            }
        } else {
            this->actor.gravity = -1.0f;
            // MM Math_Atan2S_XY(speed, -velocity.y). MM's Math_Atan2S_XY is Ocarina of Time's Math_Atan2S with the
            // same argument order ("Match the OoT implementation", sys_math_atan.c): the pitch of the arc, negative
            // on the way up.
            this->unk_6C2 = Math_Atan2S(this->actor.speedXZ, -this->actor.velocity.y);
            MergeShip_Zora_func_8082F164(this, BTN_R);
        }
    }
}

// MM func_8083827C, the Zora's part: holding A while running off a ledge above deep water dives in.
// Returns true when it started the dive.
s32 MergeShip_Zora_RunningDive(PlayState* play, Player* this) {
    static Vec3f sProbe = { 0.0f, 0.0f, 100.0f }; // MM D_8085D154
    CollisionPoly* floorPoly;
    s32 floorBgId;
    WaterBox* waterBox;
    Vec3f probePos;
    f32 floorY;
    f32 waterY;

    if (!(this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
        (CHECK_BTN_ALL(sControlInput->cur.button, BTN_A) || (sPrevFloorProperty == FLOOR_PROPERTY_11))) {
        floorY = func_808396F4(play, this, &sProbe, &probePos, &floorPoly, &floorBgId);
        waterY = this->actor.world.pos.y;

        if (WaterBox_GetSurface1(play, &play->colCtx, probePos.x, probePos.z, &waterY, &waterBox) &&
            ((waterY - floorY) > 50.0f)) {
            func_808389E8(this, MM_ANIM(gPlayerAnim_link_normal_run_jump_water_fall), 6.0f, play);
            Player_SetupAction(play, this, MergeShip_Zora_Action_27, 0);
            return true;
        }
    }
    return false;
}

// MM Player_UpdateCommon, the Zora's parts before the action runs: the swim blend unk_B10[0] and the
// per-frame clears MM makes of its own stateFlags3 bits (Ocarina of Time's Player_UpdateCommon only clears its
// low byte). Also clears the barrier request (MM PLAYER_STATE1_10).
void MergeShip_Zora_UpdateCommonEarly(Player* this, PlayState* play) {
    static f32 D_8085D3FC[] = { 0.005f, 0.05f };
    s32 var_v0 = (this->stateFlags1 & PLAYER_STATE1_IN_WATER) ? 1 : 0;

    // A Zora that was not a Zora last frame (a transformation) starts with MM's prevBoots equal to its boots, as
    // MM's Player_Init leaves them.
    if (gMergeShipZora.lastUpdateFrame != (u32)(play->gameplayFrames - 1)) {
        gMergeShipZora.prevBoots = gMergeShipPlayerExt.mmBoots;
    }
    gMergeShipZora.lastUpdateFrame = play->gameplayFrames;

    // Harness trace (no-op outside tests): MM's swim state that the P line does not show.
    MergeShipTest_Log("Z depth=%.1f pitch=%d B48=%.2f boots=%d/%d s3=%08X fr=%.1f av=%d/%d B86=%d/%d B8A=%d grav=%.2f "
                      "rotY=%d yaw=%d wallYaw=%d",
                      this->actor.yDistToWater, this->unk_6C2, this->unk_B48, gMergeShipPlayerExt.mmBoots,
                      gMergeShipZora.prevBoots, this->stateFlags3, this->skelAnime.curFrame, this->av1.actionVar1,
                      this->av2.actionVar2, this->unk_B86[0], this->unk_B86[1], this->unk_B8A, this->actor.gravity,
                      this->actor.shape.rot.y, this->yaw, sWorldYawToTouchedWall);

    Math_StepToF(&this->unk_B10[0], var_v0, D_8085D3FC[var_v0]);

    if (this->unk_D57 != 0) {
        this->unk_D57--;
    }

    gMergeShipZora.barrierRequest = false;
    this->stateFlags3 &= ~(PLAYER_STATE3_100 | PLAYER_STATE3_800 | PLAYER_STATE3_1000 | PLAYER_STATE3_100000 |
                           PLAYER_STATE3_2000000 | PLAYER_STATE3_4000000 | PLAYER_STATE3_8000000 |
                           PLAYER_STATE3_10000000);
}

// MM Player_UpdateCommon: `if (currentBoots != prevBoots)` -- the Zora sinking to walk on the bottom or
// starting to swim again.
void MergeShip_Zora_UpdateBootsChange(Player* this, PlayState* play) {
    if (gMergeShipPlayerExt.mmBoots != gMergeShipZora.prevBoots) {
        if (gMergeShipPlayerExt.mmBoots == MM_BOOTS_ZORA_UNDERWATER) {
            if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
                func_80832340(play, this); // MM func_8082DC64
                if (this->ageProperties->unk_2C < this->actor.yDistToWater) {
                    this->stateFlags2 |= PLAYER_STATE2_UNDERWATER;
                }
            }
        } else if ((this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
                   ((gMergeShipZora.prevBoots == MM_BOOTS_ZORA_UNDERWATER) ||
                    (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
            MergeShip_Zora_func_8083B930(play, this);
            this->stateFlags2 &= ~PLAYER_STATE2_UNDERWATER;
            if (MergeShip_Zora_IsAction54(this)) {
                this->av2.actionVar2 = 20;
            }
        }
        gMergeShipZora.prevBoots = gMergeShipPlayerExt.mmBoots;
        Player_SetBootData(play, this);
    }
}

// ============================================================================================================
// Camera
// ============================================================================================================

// MM Player_UpdateCamAndSeqModes, for the Zora. MM's modes map to Ocarina of Time's (MM BOWARROWZ is Ocarina
// of Time's Z_AIM); the fast swim's MM_CAM_MODE_GORONDASH has no Ocarina of Time counterpart and is requested
// from the MM camera port. The music half (sequence mode, enemy BGM) is Ocarina of Time's, as the world is.
void MergeShip_Zora_UpdateCamAndSeqModes(PlayState* play, Player* this) {
    u8 seqMode;
    Camera* camera;
    s32 camMode = CAM_MODE_NORMAL;
    s32 mmMode = MM_CAM_MODE_NONE;

    if (this->actor.category != ACTORCAT_PLAYER) {
        return;
    }

    seqMode = SEQ_MODE_DEFAULT;
    camera = Play_GetCamera(play, CAM_ID_MAIN);

    if (this->csAction != 0) {
        Camera_RequestMode(camera, CAM_MODE_NORMAL);
    } else if (!(this->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
        if ((this->actor.parent != NULL) && (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT)) {
            camMode = CAM_MODE_HOOKSHOT;
            Camera_SetParam(camera, 8, this->actor.parent);
        } else if (Player_Action_8084377C == this->actionFunc) { // MM Player_Action_21
            camMode = CAM_MODE_STILL;
        } else if (this->stateFlags3 & PLAYER_STATE3_8000) {
            if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
                mmMode = MM_CAM_MODE_GORONDASH;
            } else {
                camMode = CAM_MODE_FREEFALL;
            }
        } else if (this->stateFlags2 & PLAYER_STATE2_GRABBING_DYNAPOLY) {
            camMode = CAM_MODE_PUSHPULL;
        } else if (this->focusActor != NULL) {
            if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_TALK)) {
                camMode = CAM_MODE_TALK;
            } else if (this->stateFlags1 & PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS) {
                if (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) {
                    camMode = CAM_MODE_FOLLOWBOOMERANG;
                } else {
                    camMode = CAM_MODE_FOLLOWTARGET;
                }
            } else {
                camMode = CAM_MODE_BATTLE;
            }
            Camera_SetParam(camera, 8, this->focusActor);
        } else if (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) {
            camMode = CAM_MODE_CHARGE;
        } else if (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) {
            camMode = CAM_MODE_FOLLOWBOOMERANG;
            Camera_SetParam(camera, 8, this->zoraBoomerangActor);
        } else if (this->stateFlags1 & (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE)) {
            if (Player_FriendlyLockOnOrParallel(this)) {
                camMode = CAM_MODE_HANGZ;
            } else {
                camMode = CAM_MODE_HANG;
            }
        } else if (this->stateFlags1 & (PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE)) {
            if (func_8002DD78(this) || func_808334B4(this)) {
                camMode = CAM_MODE_Z_AIM;
            } else if (this->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) {
                camMode = CAM_MODE_CLIMBZ;
            } else {
                camMode = CAM_MODE_TARGET;
            }
        } else if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
            // MM: a transformed player guarding holds the camera still.
            camMode = CAM_MODE_STILL;
        } else if (this->stateFlags1 & PLAYER_STATE1_JUMPING) {
            camMode = CAM_MODE_JUMP;
        } else if (this->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) {
            camMode = CAM_MODE_CLIMB;
        } else if (this->stateFlags1 & PLAYER_STATE1_FREEFALL) {
            camMode = CAM_MODE_FREEFALL;
        } else if (((MergeShip_Zora_Action_84 == this->actionFunc) &&
                    (gMergeShipZora.meleeWeaponAnimation <= MM_PLAYER_MWA_ZORA_PUNCH_KICK)) ||
                   (this->stateFlags3 & PLAYER_STATE3_FINISHED_ATTACKING)) {
            camMode = CAM_MODE_STILL;
        } else {
            camMode = CAM_MODE_NORMAL;
            if ((this->linearVelocity == 0.0f) &&
                (!(this->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (this->rideActor->speedXZ == 0.0f))) {
                seqMode = SEQ_MODE_STILL;
            }
        }

        if (mmMode != MM_CAM_MODE_NONE) {
            MergeShip_Camera_RequestMMMode(play, mmMode);
        } else {
            Camera_RequestMode(camera, camMode);
        }
    } else {
        seqMode = SEQ_MODE_STILL;
    }

    if (play->actorCtx.targetCtx.bgmEnemy != NULL) {
        seqMode = SEQ_MODE_ENEMY;
        Audio_SetBgmEnemyVolume(sqrtf(play->actorCtx.targetCtx.bgmEnemy->xyzDistToPlayerSq));
    }

    if (play->sceneNum != SCENE_FISHING_POND) {
        Audio_SetSequenceMode(seqMode);
    }
}

// ============================================================================================================
// Doors, fire, damage
// ============================================================================================================

// MM Player_Door_Knob: D_8085D118 / D_8085D124 for the Zora.
LinkAnimationHeader* MergeShip_Zora_DoorOpenAnim(Player* this, s32 left) {
    return left ? MM_ANIM(gPlayerAnim_pz_doorA_open) : MM_ANIM(gPlayerAnim_pz_doorB_open);
}

// MM Player_Action_77: knocked out (burnt): void out back to the room's entrance.
static void MergeShip_Zora_Action_77(Player* this, PlayState* play) {
    if (this->skelAnime.animation == NULL) {
        this->stateFlags2 |= PLAYER_STATE2_FROZEN; // MM PLAYER_STATE2_4000
    } else {
        LinkAnimation_Update(play, &this->skelAnime);
        if ((this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_derth_rebirth)) &&
            LinkAnimation_OnFrame(&this->skelAnime, 60.0f)) {
            MergeShip_PlayMMFloorSfx(this, MM_NA_SE_PL_BOUND);
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_DAMAGE_S);
        }
    }

    if ((this->av2.actionVar2++ >= 9) && !MergeShip_Zora_func_8082DA90(play)) {
        if (this->av1.actionVar1 != 0) {
            // MM func_80169EFC is Ocarina of Time's Play_TriggerVoidOut (MM func_80169FDC its return to the last
            // entrance).
            if (this->av1.actionVar1 < 0) {
                Play_LoadToLastEntrance(play);
            } else {
                Play_TriggerVoidOut(play);
            }
            play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
            MergeShip_PlayMMSfxCentered(MM_NA_SE_OC_ABYSS);
        } else {
            play->transitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
        }

        play->transitionTrigger = TRANS_TRIGGER_START;
    }
}

// MM func_80834140.
static void MergeShip_Zora_func_80834140(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    if (!(this->stateFlags1 & PLAYER_STATE1_DEAD)) {
        Player_SetupAction(play, this, MergeShip_Zora_Action_77, 0); // MM func_80834104
        this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED;
        if (MergeShip_Zora_func_8082DA90(play)) {
            this->av2.actionVar2 = -30;
        }
        this->stateFlags1 |= PLAYER_STATE1_DEAD;
        LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, 84.0f, ANIMMODE_ONCE, -6.0f);
        this->av1.actionVar1 = 1;
        this->linearVelocity = 0.0f;
    }
}

// MM Player_UpdateBodyBurn, the Zora's path: a burning Zora on the ground is knocked out; the flames burn down
// without the running-speed decay a human gets. Returns true when it handled the frame.
s32 MergeShip_Zora_UpdateBodyBurn(PlayState* play, Player* this) {
    s32 i;
    s32 spawnedFlame = false;
    u8* timerPtr = this->bodyFlameTimers;

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        if (this->cylinder.base.ocFlags1 & OC1_HIT) {
            Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.prevPos);
            this->linearVelocity = 0.0f;
        }
        MergeShip_Zora_func_80834140(play, this, MM_ANIM(gPlayerAnim_link_derth_rebirth));
    }

    for (i = 0; i < PLAYER_BODYPART_MAX; i++, timerPtr++) {
        if (*timerPtr <= 0) {
            *timerPtr = 0;
        } else {
            f32 flameScale;
            f32 flameIntensity;

            spawnedFlame = true;
            if (*timerPtr > 20.0f) {
                f32 temp_fv0 = (*timerPtr - 20.0f) * 0.01f;

                flameScale = CLAMP(temp_fv0, 0.19999999f, 0.2f);
            } else {
                flameScale = *timerPtr * 0.01f;
            }

            flameIntensity = (*timerPtr - 25.0f) * 0.02f;
            flameIntensity = CLAMP(flameIntensity, 0.0f, 1.0f);
            EffectSsFireTail_SpawnFlameOnPlayer(play, flameScale, i, flameIntensity);
        }
    }

    if (spawnedFlame) {
        MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_EV_TORCH - SFX_FLAG);
        if ((play->gameplayFrames % 4) == 0) {
            Player_InflictDamage(play, -1);
        }
    } else {
        this->bodyIsBurning = false;
    }

    return true;
}

// MM func_80834600, the Zora's parts. Returns 1 when the Zora's rule decides the frame (0: no damage, as MM
// returns false), -1 to let Ocarina of Time's damage code continue.
//  - The barrier's collider hitting something (MM shieldCylinder AT_HIT) means no contact damage this frame.
//  - A hot floor that is not a damage wall sets a Zora on the ground (out of the water) alight (MM
//    func_80834534), instead of the heat damage a human takes.
s32 MergeShip_Zora_DamageCheck(PlayState* play, Player* this, s32 hotFloor) {
    if (hotFloor < 0) {
        if (this->shieldCylinder.base.atFlags & AT_HIT) {
            return 0;
        }
        return -1;
    }

    if (!SurfaceType_IsWallDamage(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId) &&
        !(this->stateFlags1 & PLAYER_STATE1_IN_WATER) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        (this->actor.yDistToWater < -30.0f)) {
        s32 i;

        // MM func_80834534 -> func_808344C0.
        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_FALL_L);
        for (i = 0; i < ARRAY_COUNT(this->bodyFlameTimers); i++) {
            this->bodyFlameTimers[i] = Rand_S16Offset(0, 200);
        }
        this->bodyIsBurning = true;
        // MM Player_UpdateBodyBurn: the Deku and the Zora share this branch (a Deku on a hot floor burns too).
        if (this->mergeShipForm == PLAYER_FORM_DEKU) {
            MergeShip_Deku_UpdateBodyBurn(play, this);
        } else {
            MergeShip_Zora_UpdateBodyBurn(play, this);
        }
        return 1;
    }
    return -1;
}

// MM func_8083456C for the Deku and the Zora: a fire hit (Ocarina of Time's HIT_SPECIAL_EFFECT_FIRE, MM
// acHitEffect 1) sets them alight (Ocarina of Time's func_80838280 has already set the flame timers and played
// the voice) and, on the ground, knocks them out at once; MM then skips the damage reaction (func_80834600
// returns true). Returns true when the form was knocked out.
s32 MergeShip_MM_BurnOnFireHit(PlayState* play, Player* this) {
    if (((this->mergeShipForm != PLAYER_FORM_DEKU) && (this->mergeShipForm != PLAYER_FORM_ZORA)) ||
        (this->actor.colChkInfo.acHitEffect != 1) || !this->bodyIsBurning) {
        return false;
    }
    if (this->mergeShipForm == PLAYER_FORM_DEKU) {
        MergeShip_Deku_UpdateBodyBurn(play, this);
    } else {
        MergeShip_Zora_UpdateBodyBurn(play, this);
    }
    return (this->stateFlags1 & PLAYER_STATE1_DEAD) != 0;
}

// ============================================================================================================
// Setup
// ============================================================================================================

// MM D_8085C318 (sShieldCylinderInit), as Ocarina of Time's ColliderCylinderInit. Its AT bits are MM's shield
// push (DMG_NORMAL_SHIELD, bit 20 in both games).
static ColliderCylinderInit sMergeShipShieldCylinderInit = {
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

// MM's second melee blure (D_8085D30C, the same as Ocarina of Time's sword blure).
static EffectBlureInit2 sMergeShipZoraBlure = {
    0, 8, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, 4,
    0, 2, 0, { 0, 0, 0, 0 },         { 0, 0, 0, 0 },         TRAIL_TYPE_SWORDS,
};

// MM Player_InitCommon: the shield cylinder (the Zora's barrier collider) and MM's second blure. Called from
// Ocarina of Time's Player_InitCommon for every player, so a later transformation finds them ready.
void MergeShip_Zora_Init(Player* this, PlayState* play) {
    Collider_InitCylinder(play, &this->shieldCylinder);
    Collider_SetCylinder(play, &this->shieldCylinder, &this->actor, &sMergeShipShieldCylinderInit);
    this->unk_B62 = 0;
    this->zoraBoomerangActor = NULL;

    // The Zora state is the player's alone (Dark Link also builds a Player).
    if (this->actor.category != ACTORCAT_PLAYER) {
        return;
    }

    Effect_Add(play, &gMergeShipZora.blureIndex2, EFFECT_BLURE2, 0, 0, &sMergeShipZoraBlure);
    gMergeShipZora.blureValid = true;
    gMergeShipZora.prevBoots = gMergeShipPlayerExt.mmBoots;
    gMergeShipZora.barrierRequest = false;
    gMergeShipZora.barrierDraining = 0;
}

// MM Player_Destroy: the second blure and the shield cylinder.
void MergeShip_Zora_Destroy(Player* this, PlayState* play) {
    Collider_DestroyCylinder(play, &this->shieldCylinder);
    if ((this->actor.category == ACTORCAT_PLAYER) && gMergeShipZora.blureValid) {
        Effect_Delete(play, gMergeShipZora.blureIndex2);
        gMergeShipZora.blureValid = false;
    }
}

// MM Player_Action_82, the Zora's path: six frames into being frozen, the Zora is knocked out (MM
// func_80834104 with no animation, which keeps it drawn frozen) and voids out. Returns true when it acted.
s32 MergeShip_Zora_Frozen(PlayState* play, Player* this) {
    if ((this->av1.actionVar1 >= 6) && ZORA_IS(this)) {
        Player_SetupAction(play, this, MergeShip_Zora_Action_77, 0);
        this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED;
        this->skelAnime.animation = NULL;
        this->av2.actionVar2 = -0x28;
        this->av1.actionVar1 = 1;
        this->linearVelocity = 0.0f;
        return true;
    }
    return false;
}

// MM func_8083827C's action exclusions: the Zora's running dive and breach carry on in the air.
s32 MergeShip_Zora_IsAirAction(Player* this) {
    return (this->actionFunc == MergeShip_Zora_Action_27) || (this->actionFunc == MergeShip_Zora_Action_28);
}

// MM Player_SetAction, for the Zora: every action change resets MM's per-action working values (unk_B86,
// unk_B8A..unk_B8E, unk_B10[], shape.rot.z) and MM's per-action stateFlags3 bits, among them
// PLAYER_STATE3_8000 (the fast swim), which the swim code sets again after choosing its action.
void MergeShip_Zora_OnSetupAction(PlayState* play, Player* this) {
    s32 i;

    this->unk_B86[0] = 0;
    this->unk_B86[1] = 0;
    this->unk_B8A = 0;
    this->unk_B8C = 0;
    this->unk_B8E = 0;
    for (i = 0; i < ARRAY_COUNT(this->unk_B10); i++) {
        this->unk_B10[i] = 0.0f;
    }
    this->actor.shape.rot.z = 0;

    this->stateFlags3 &= ~(PLAYER_STATE3_200 | PLAYER_STATE3_2000 | PLAYER_STATE3_8000 | PLAYER_STATE3_10000 |
                           PLAYER_STATE3_20000 | PLAYER_STATE3_40000 | PLAYER_STATE3_80000 | PLAYER_STATE3_200000 |
                           PLAYER_STATE3_1000000 | PLAYER_STATE3_20000000);
}

// MM Player_UpdateInterface: `func_801242B4(this) || (Player_Action_28 == this->actionFunc)`.
s32 MergeShip_Zora_IsAction28(Player* this) {
    return this->actionFunc == MergeShip_Zora_Action_28;
}

// MM FaceChange_UpdateBlinkingNonHuman(&faceChange, 20, 80, 3) (z_actor.c), which MM's Player_UpdateCommon uses
// for the Goron, Zora and Deku: a shorter blink than the human's. Ocarina of Time's unk_3A8 is MM's FaceChange
// { face, timer }.
void MergeShip_Zora_UpdateBlinking(Player* this) {
    s16* faceChange = this->unk_3A8;

    if (DECR(faceChange[1]) == 0) {
        faceChange[1] = Rand_S16Offset(20, 80);
    }

    if (faceChange[1] - 3 > 0) {
        faceChange[0] = 0;
    } else if (faceChange[1] - 3 == 0) {
        faceChange[0] = 1;
    } else {
        faceChange[0] = 2;
    }
}

// ============================================================================================================
// Climbing up a ledge (and out of the water onto one), for every MM form
// ============================================================================================================

// MM Player_ActionHandler_12, what MM's climb does differently from Ocarina of Time's (called after Ocarina of
// Time's code has placed the player): MM's own animations (gameplay_keep), and for a step-up MM lifts the player
// 10 units more, then settles it onto the floor found there (MM BgCheck_EntityRaycastFloor5 is Ocarina of Time's
// BgCheck_EntityRaycastFloor4) and stops its fall.
void MergeShip_MM_ClimbLedgeSetup(PlayState* play, Player* this, LinkAnimationHeader** anim) {
    CollisionPoly* poly;
    s32 bgId;
    f32 yIntersect;

    if (*anim == &gPlayerAnim_link_normal_250jump_start) {
        *anim = MM_ANIM(gPlayerAnim_link_normal_250jump_start);
        return;
    }

    if (*anim == &gPlayerAnim_link_swimer_swim_15step_up) {
        *anim = MM_ANIM(gPlayerAnim_link_swimer_swim_15step_up);
    } else if (*anim == &gPlayerAnim_link_normal_150step_up) {
        *anim = MM_ANIM(gPlayerAnim_link_normal_150step_up);
    } else {
        *anim = MM_ANIM(gPlayerAnim_link_normal_100step_up);
    }

    this->actor.world.pos.y += 10.0f;
    yIntersect = BgCheck_EntityRaycastFloor4(&play->colCtx, &poly, &bgId, &this->actor, &this->actor.world.pos);
    if ((this->actor.world.pos.y - yIntersect) <= 20.0f) {
        this->actor.world.pos.y = yIntersect;
        if (bgId != BGCHECK_SCENE) {
            DynaPoly_SetPlayerOnTop(&play->colCtx, bgId);
        }
    }
    this->actor.velocity.y = 0.0f;
}

// MM Player_Action_33: the climb. MM's unk_ABC is the model's y offset (MM Player_UpdateCommon sets
// shape.yOffset = unk_ABC + unk_AC0), so it is kept in shape.yOffset here, as Ocarina of Time's climb keeps it.
void MergeShip_MM_Action_33(Player* this, PlayState* play) {
    s32 animFinished;
    s32 interruptResult;
    f32 frame;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    animFinished = LinkAnimation_Update(play, &this->skelAnime);

    if (this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_normal_250jump_start)) {
        this->linearVelocity = 1.0f;
        if (LinkAnimation_OnFrame(&this->skelAnime, 8.0f)) {
            f32 speed = this->yDistToLedge;

            speed = CLAMP_MAX(speed, this->ageProperties->unk_0C);
            if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
                speed *= 0.085f;
            } else {
                speed *= 0.072f;
            }
            // (MM adds 1.0f for the human only.)

            // MM func_80834D50(play, this, NULL, speed, NA_SE_VO_LI_AUTO_JUMP).
            func_80838940(this, NULL, speed, play, NA_SE_VO_LI_AUTO_JUMP);
            this->av2.actionVar2 = -1;
        }
    } else {
        interruptResult = Player_TryActionInterrupt(play, this, &this->skelAnime, 4.0f);

        if (interruptResult == PLAYER_INTERRUPT_NEW_ACTION) {
            this->stateFlags1 &= ~(PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_JUMPING); // MM 4|4000|40000
            return;
        }

        if (animFinished || (interruptResult >= PLAYER_INTERRUPT_MOVE)) {
            func_8083C0E8(this, play); // MM func_80839E74
            this->stateFlags1 &= ~(PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_JUMPING); // MM 4|4000|40000
            this->actor.shape.yOffset = 0.0f;
            return;
        }

        frame = 0.0f;
        if (this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_swimer_swim_15step_up)) {
            if (LinkAnimation_OnFrame(&this->skelAnime, 30.0f)) {
                func_8083D0A8(play, this, 10.0f); // MM func_8083B32C
            }
            frame = 50.0f;
        } else if (this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_normal_150step_up)) {
            frame = 30.0f;
        } else if (this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_normal_100step_up)) {
            frame = 16.0f;
        }

        if (LinkAnimation_OnFrame(&this->skelAnime, frame)) {
            MergeShip_PlayMMFloorLandSfx(this);
            MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_CLIMB_END);
        }

        if ((this->skelAnime.animation == MM_ANIM(gPlayerAnim_link_normal_100step_up)) ||
            (this->skelAnime.curFrame > 5.0f)) {
            if (this->av2.actionVar2 == 0) {
                MergeShip_PlayMMFloorJumpSfx(this);
                this->av2.actionVar2 = 1;
            }
            Math_SmoothStepToF(&this->actor.shape.yOffset, 0.0f, 0.1f, 400.0f, 150.0f);
        }
    }
}
