// MergeShip2 -- Majora's Mask's player behaviour for its four forms, inside Ocarina of Time's player.
//
// Included by z_player.c after everything it uses. Each function names the MM function it ports
// (2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/z_player.c unless stated). The forms live in
// Hyrule, so where MM consults MM-only world state (the three-day clock, Tatl, MM text ids) Ocarina of
// Time's equivalent is used; everything the form itself does follows MM.

#include "soh/MergeShip/MergeShipHud.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"

#define MMFORM(this) MergeShip_FormToMM((this)->mergeShipForm)

#include "soh/MergeShip/MergeShipCamera.h"

// ---- MM's player sound helpers, for MM forms (MM z_player.c ~1750-1860). All ids are MM_NA_SE_*. ----
// Shared by every form's port: use these rather than adding copies.

// MM Player_AnimSfx_PlayVoice (MM's Giant's Mask and Mask of Scents branches have no Hyrule
// counterpart).
void MergeShip_PlayMMVoiceSfx(Player* this, u16 mmSfxId) {
    if (this->actor.id == ACTOR_PLAYER) {
        MergeShip_PlayMMSfxActor(&this->actor, mmSfxId + this->ageProperties->voiceSfxIdOffset);
    }
}

// MM Player_GetFloorSfx / Player_AnimSfx_PlayFloor.
u16 MergeShip_GetMMFloorSfx(Player* this, u16 mmSfxId) {
    return mmSfxId + this->floorSfxOffset;
}
void MergeShip_PlayMMFloorSfx(Player* this, u16 mmSfxId) {
    MergeShip_PlayMMSfxActor(&this->actor, MergeShip_GetMMFloorSfx(this, mmSfxId));
}

// MM Player_GetFloorSfxByAge / Player_AnimSfx_PlayFloorByAge.
u16 MergeShip_GetMMFloorSfxByAge(Player* this, u16 mmSfxId) {
    return mmSfxId + this->floorSfxOffset + this->ageProperties->surfaceSfxIdOffset;
}
void MergeShip_PlayMMFloorSfxByAge(Player* this, u16 mmSfxId) {
    MergeShip_PlayMMSfxActor(&this->actor, MergeShip_GetMMFloorSfxByAge(this, mmSfxId));
}

// MM Player_AnimSfx_PlayFloorJump / PlayFloorLand.
void MergeShip_PlayMMFloorJumpSfx(Player* this) {
    MergeShip_PlayMMSfxActor(&this->actor, MergeShip_GetMMFloorSfxByAge(this, MM_NA_SE_PL_JUMP_GROUND));
}
void MergeShip_PlayMMFloorLandSfx(Player* this) {
    MergeShip_PlayMMSfxActor(&this->actor, MergeShip_GetMMFloorSfxByAge(this, MM_NA_SE_PL_LAND_GROUND));
}

// MM func_8082E1F0.
void MergeShip_PlayMMSfxAndFootstepFlag(Player* this, u16 mmSfxId) {
    MergeShip_PlayMMSfxActor(&this->actor, mmSfxId);
    this->stateFlags2 |= PLAYER_STATE2_FOOTSTEP;
}

// MM Player_PlayAnimSfx for lists whose ids are MM_NA_SE_*. The walking types use MM's metal-effect
// synced call; the audio port supplies MergeShip_PlayMMFloorWalkSfx.
void MergeShip_PlayMMFloorWalkSfx(Player* this, f32 freqVolumeLerp);
void MergeShip_PlayMMAnimSfxList(Player* this, AnimSfxEntry* entry) {
    s32 cont;

    do {
        s32 data = ABS(entry->data);
        s32 type = ANIMSFX_GET_TYPE(data);

        if (LinkAnimation_OnFrame(&this->skelAnime, fabsf(ANIMSFX_GET_FRAME(data)))) {
            if (type == ANIMSFX_SHIFT_TYPE(1)) {        // GENERAL
                MergeShip_PlayMMSfxActor(&this->actor, entry->sfxId);
            } else if (type == ANIMSFX_SHIFT_TYPE(2)) { // FLOOR
                MergeShip_PlayMMFloorSfx(this, entry->sfxId);
            } else if (type == ANIMSFX_SHIFT_TYPE(3)) { // FLOOR_BY_AGE
                MergeShip_PlayMMFloorSfxByAge(this, entry->sfxId);
            } else if (type == ANIMSFX_SHIFT_TYPE(4)) { // VOICE
                MergeShip_PlayMMVoiceSfx(this, entry->sfxId);
            } else if (type == ANIMSFX_SHIFT_TYPE(5)) { // FLOOR_LAND
                MergeShip_PlayMMFloorLandSfx(this);
            } else if (type == ANIMSFX_SHIFT_TYPE(6)) {
                MergeShip_PlayMMFloorWalkSfx(this, 6.0f);
            } else if (type == ANIMSFX_SHIFT_TYPE(7)) { // FLOOR_JUMP
                MergeShip_PlayMMFloorJumpSfx(this);
            } else if (type == ANIMSFX_SHIFT_TYPE(8)) {
                MergeShip_PlayMMFloorWalkSfx(this, 0.0f);
            } else if (type == ANIMSFX_SHIFT_TYPE(10)) { // SURFACE
                MergeShip_PlayMMSfxActor(&this->actor, entry->sfxId + this->ageProperties->surfaceSfxIdOffset);
            }
        }

        cont = (entry->data >= 0);
        entry++;
    } while (cont);
}

// MM func_801242B4: swimming, i.e. in water and not weighed down (Goron, Zora on the bottom).
static s32 MergeShip_IsSwimmingMM(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_IN_WATER) && (gMergeShipPlayerExt.mmBoots < MM_BOOTS_ZORA_UNDERWATER);
}

// MM func_800C9DDC (z_bgcheck.c): the floor poly's vertex-B flag 0x4000. Ocarina of Time's bgcheck has no
// accessor for this bit.
s32 MergeShip_MM_func_800C9DDC(CollisionContext* colCtx, CollisionPoly* poly, s32 bgId) {
    if (BgCheck_GetCollisionHeader(colCtx, bgId) == NULL) {
        return true;
    }
    return !!(poly->flags_vIB & 0x4000);
}

// MM Player_IsGoronOrDeku.
static s32 MergeShip_IsGoronOrDeku(Player* this) {
    return (this->mergeShipForm == PLAYER_FORM_GORON) || (this->mergeShipForm == PLAYER_FORM_DEKU);
}

// MM Interface_SetBButtonPlayerDoAction (z_parameter.c): the label shows while the form's B item is a
// sword, empty or Deku nuts; a Deku without magic shows none.
static void MergeShip_SetMMBButtonDoActionForForm(PlayState* play, Player* this, s32 bButtonDoAction) {
    u8 bItem = gSaveContext.equips.buttonItems[0];

    // MergeShip2: MM's own swords on B (the Razor, Gilded and Great Fairy's Swords, MergeShipMMEquipment.h) count as
    // swords too, as they do in MM.
    if (((bItem >= ITEM_SWORD_KOKIRI) && (bItem <= ITEM_SWORD_BGS)) || MERGESHIP_ITEM_IS_MM_SWORD(bItem) ||
        (bItem == ITEM_NONE) || (bItem == ITEM_NUT)) {
        if ((this->mergeShipForm == PLAYER_FORM_DEKU) && (gSaveContext.magicLevel == 0)) {
            MergeShip_ClearMMBButtonDoAction(play);
        } else {
            MergeShip_SetMMBButtonDoAction(play, bButtonDoAction);
        }
    } else {
        MergeShip_ClearMMBButtonDoAction(play);
    }
}

// MM Player_UpdateInterface, for an MM form. Text-box gating is Ocarina of Time's (the dialogue is
// Hyrule's); the labels and their order are MM's.
void MergeShip_UpdateInterfaceMM(PlayState* play, Player* this) {
    s32 doActionB = MM_DO_ACTION_UNDEFINED;
    s32 sp38;

    if (this->actor.category != ACTORCAT_PLAYER) {
        return;
    }

    // MM Player_Action_28 (the Zora's surface swim) also counts; it is part of the Zora port.
    sp38 = MergeShip_IsSwimmingMM(this) || MergeShip_Zora_IsAction28(this);

    // B do action.
    if (this->mergeShipForm == PLAYER_FORM_GORON) {
        if (this->stateFlags3 & PLAYER_STATE3_80000) {
            doActionB = MM_DO_ACTION_NONE;
        } else if (this->stateFlags3 & PLAYER_STATE3_1000) {
            doActionB = MM_DO_ACTION_POUND;
        } else {
            doActionB = MM_DO_ACTION_PUNCH;
        }
    } else if (this->mergeShipForm == PLAYER_FORM_ZORA) {
        if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) || (!sp38 && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
            doActionB = MM_DO_ACTION_PUNCH;
        } else {
            doActionB = MM_DO_ACTION_DIVE;
        }
    } else if (this->mergeShipForm == PLAYER_FORM_DEKU) {
        doActionB = MM_DO_ACTION_SHOOT;
    }

    if (doActionB > MM_DO_ACTION_UNDEFINED) {
        MergeShip_SetMMBButtonDoActionForForm(play, this, doActionB);
    } else {
        MergeShip_ClearMMBButtonDoAction(play);
    }

    // A do action.
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
        Actor* heldActor = this->heldActor;
        Actor* interactRangeActor = this->interactRangeActor;
        s32 controlStickDirection = this->controlStickDirections[this->controlStickDataIndex];
        s32 sp24;
        s32 doActionA = ((this->mergeShipForm == PLAYER_FORM_GORON) && !(this->stateFlags1 & PLAYER_STATE1_SHIELDING))
                            ? MM_DO_ACTION_CURL
                            : MM_DO_ACTION_NONE;

        if (Player_InBlockingCsMode(play, this) || (this->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) ||
            (this->stateFlags3 & PLAYER_STATE3_80000)) {
            doActionA = MM_DO_ACTION_NONE;
        } else if (this->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) {
            doActionA = MM_DO_ACTION_RETURN;
        } else if ((this->heldItemAction == PLAYER_IA_FISHING_POLE) && (this->unk_860 != 0)) {
            doActionA = (this->unk_860 == 2) ? MM_DO_ACTION_REEL : MM_DO_ACTION_NONE;
        } else if (this->stateFlags3 & PLAYER_STATE3_2000) {
            doActionA = MM_DO_ACTION_DOWN;
        } else if ((this->doorType != PLAYER_DOORTYPE_NONE) && !(this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR)) {
            // (MM also excludes its PLAYER_DOORTYPE_STAIRCASE; Hyrule has no such door.)
            doActionA = MM_DO_ACTION_OPEN;
        } else if (this->stateFlags3 & PLAYER_STATE3_200000) {
            static u8 sHopDoActions[] = {
                MM_DO_ACTION_1, MM_DO_ACTION_2, MM_DO_ACTION_3, MM_DO_ACTION_4,
                MM_DO_ACTION_5, MM_DO_ACTION_6, MM_DO_ACTION_7, MM_DO_ACTION_8,
            };

            doActionA = sHopDoActions[this->remainingHopsCounter];
        } else if ((!(this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) || (heldActor == NULL)) &&
                   (interactRangeActor != NULL) && (this->getItemId < GI_NONE)) {
            doActionA = MM_DO_ACTION_OPEN;
        } else if (!sp38 && (this->stateFlags2 & PLAYER_STATE2_DO_ACTION_GRAB)) {
            doActionA = MM_DO_ACTION_GRAB;
        } else if ((this->stateFlags2 & PLAYER_STATE2_DO_ACTION_CLIMB) ||
                   (!(this->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (this->rideActor != NULL))) {
            doActionA = MM_DO_ACTION_CLIMB;
        } else if ((this->stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER) && (this->talkActor != NULL)) {
            if ((this->talkActor->category == ACTORCAT_NPC) || (this->talkActor->category == ACTORCAT_ENEMY)) {
                doActionA = MM_DO_ACTION_SPEAK;
            } else {
                doActionA = MM_DO_ACTION_CHECK;
            }
        } else if (this->stateFlags1 & (PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER)) {
            doActionA = MM_DO_ACTION_DOWN;
        } else if ((this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && (this->getItemId == GI_NONE) &&
                   (heldActor != NULL)) {
            if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (heldActor->id == ACTOR_EN_NIW)) {
                if (!Player_CanThrowCarriedActor(this, heldActor)) {
                    doActionA = MM_DO_ACTION_DROP;
                } else {
                    doActionA = MM_DO_ACTION_THROW;
                }
            } else {
                doActionA = MM_DO_ACTION_NONE;
            }
        } else if (this->stateFlags2 & PLAYER_STATE2_DO_ACTION_ENTER) {
            // Ocarina of Time's crawlspace prompt (MM gives this bit a different use).
            doActionA = MM_DO_ACTION_ENTER;
        } else if (this->stateFlags2 & PLAYER_STATE2_DIVING) {
            static u8 sDiveDoActions[] = { MM_DO_ACTION_1, MM_DO_ACTION_2 };
            s32 var_v0 = ((120.0f - this->actor.yDistToWater) / 40.0f);

            var_v0 = CLAMP(var_v0, 0, ARRAY_COUNT(sDiveDoActions) - 1);
            doActionA = sDiveDoActions[var_v0];
        } else if (this->stateFlags3 & PLAYER_STATE3_100) {
            doActionA = MM_DO_ACTION_JUMP;
        } else if (this->stateFlags3 & PLAYER_STATE3_1000) {
            doActionA = MM_DO_ACTION_RETURN;
        } else if (!Player_IsZTargeting(this) && (this->stateFlags1 & PLAYER_STATE1_IN_WATER) && !sp38) {
            doActionA = MM_DO_ACTION_SURFACE;
        } else if (((this->mergeShipForm != PLAYER_FORM_DEKU) &&
                    (sp38 || ((this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
                              !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)))) ||
                   ((this->mergeShipForm == PLAYER_FORM_DEKU) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                    MergeShip_MM_func_800C9DDC(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId))) {
            doActionA = (this->mergeShipForm == PLAYER_FORM_ZORA) ? MM_DO_ACTION_SWIM : MM_DO_ACTION_DIVE;
        } else {
            sp24 = Player_IsZTargeting(this);
            if ((sp24 && (this->mergeShipForm != PLAYER_FORM_DEKU)) || !(this->stateFlags1 & PLAYER_STATE1_SHIELDING) ||
                !MergeShip_IsGoronOrDeku(this)) {
                if ((this->mergeShipForm != PLAYER_FORM_GORON) &&
                    !(this->stateFlags1 & (PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_CLIMBING_LEDGE)) &&
                    (controlStickDirection <= PLAYER_STICK_DIR_FORWARD) &&
                    (Player_CheckHostileLockOn(this) ||
                     ((sFloorType != 7) &&
                      (Player_FriendlyLockOnOrParallel(this) ||
                       ((play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_2) &&
                        !(this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
                        (controlStickDirection == PLAYER_STICK_DIR_FORWARD)))))) {
                    doActionA = MM_DO_ACTION_ATTACK;
                } else if ((play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_2) && sp24 &&
                           (controlStickDirection >= PLAYER_STICK_DIR_LEFT)) {
                    doActionA = MM_DO_ACTION_JUMP;
                } else if ((this->mergeShipForm == PLAYER_FORM_DEKU) && !(this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
                           (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
                    doActionA = MM_DO_ACTION_ATTACK;
                } else if (((this->mergeShipForm == PLAYER_FORM_ZORA) ||
                            ((this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) && MergeShip_SohFd_FdCanSheathe())) &&
                           ((this->heldItemAction >= PLAYER_IA_SWORD_MASTER) ||
                            ((this->stateFlags2 & PLAYER_STATE2_NAVI_ACTIVE) &&
                             (play->actorCtx.targetCtx.arrowPointedActor == NULL)))) {
                    // MM allows the put-away label for the human and the Zora only. MergeShip2 (soh_fd): "FD Can
                    // Sheathe Sword" gives it to the Fierce Deity too: A puts away only after the label's cooldown
                    // (Player_ActionHandler_Roll), which any other label resets.
                    doActionA = MM_DO_ACTION_PUTAWAY;
                }
            }
        }

        // MM: `if (play->actorCtx.flags & ACTORCTX_FLAG_PICTO_BOX_ON) doActionA = DO_ACTION_SNAP` (MergeShipPicto.c).
        doActionA = MergeShip_Picto_DoActionA(doActionA, MM_DO_ACTION_SNAP);

        if (doActionA != MM_DO_ACTION_PUTAWAY) {
            this->putAwayCooldownTimer = 20;
        } else if (this->putAwayCooldownTimer != 0) {
            doActionA = MM_DO_ACTION_NONE;
            this->putAwayCooldownTimer--;
        }

        MergeShip_SetMMAButtonDoAction(play, doActionA);

        // MM sets Tatl's call here; Hyrule's fairy is Navi, handled as Ocarina of Time does.
        if (this->stateFlags2 & PLAYER_STATE2_NAVI_ALERT) {
            if (this->focusActor != NULL) {
                Interface_SetNaviCall(play, 0x1E);
            } else {
                Interface_SetNaviCall(play, 0x1D);
            }
            Interface_SetNaviCall(play, 0x1E);
        } else {
            Interface_SetNaviCall(play, 0x1F);
        }
    }
}
