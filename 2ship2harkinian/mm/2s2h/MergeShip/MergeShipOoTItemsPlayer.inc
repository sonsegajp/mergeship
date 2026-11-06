// MergeShip2 -- Ocarina of Time's items in Majora's Mask's player (MergeShipOoTItems.h). Included at the end of
// z_player.c, whose statics it uses; z_player.c calls in with one-line hooks (prototypes in
// MergeShipOoTItemsPlayer.h). Every function names the Ocarina of Time function it ports (Shipwright
// soh/src/overlays/actors/ovl_player_actor/z_player.c); OoT's unk_6AD is MM's unk_AA5 (OoT 4 = MM PLAYER_UNKAA5_5),
// OoT's unk_834 MM's unk_ACC, OoT's unk_845 MM's unk_ADD, OoT's func_80834758 MM's func_80830B88 (shield),
// func_80834EB8 MM's func_80831010, func_80834E44 MM's func_80830F9C, func_80839FFC MM's func_808369F4, func_80853080
// MM's func_80839E74, func_80842D20 MM's func_808400CC, func_80832630 MM's func_8082DF2C. OoT's
// PLAYER_STATE1_USING_BOOMERANG / BOOMERANG_THROWN are MM's PLAYER_STATE1_USING_ZORA_BOOMERANG / ZORA_BOOMERANG_THROWN
// (the same bits), OoT's player->boomerangActor MM's zoraBoomerangActor, OoT's PLAYER_STATE1_IN_ITEM_CS /
// IN_CUTSCENE MM's PLAYER_STATE1_10000000 / PLAYER_STATE1_20000000.

#include "2s2h/MergeShip/MergeShipOoTSpells.h"
#include "overlays/actors/ovl_OoT_En_Boom/z_oot_en_boom.h"
#include "2s2h/CustomMessage/CustomMessage.h"

static const ALIGN_ASSET(2) char sMsOoTAnimHammerHit[] = "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_hit";
static const ALIGN_ASSET(2) char sMsOoTAnimHammerHitEnd[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_hit_end";
static const ALIGN_ASSET(2) char sMsOoTAnimHammerHitEndR[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_hit_endR";
static const ALIGN_ASSET(2) char sMsOoTAnimHammerSideHit[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_side_hit";
static const ALIGN_ASSET(2) char sMsOoTAnimHammerSideHitEnd[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_side_hit_end";
static const ALIGN_ASSET(2) char sMsOoTAnimHammerSideHitEndR[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_hammer_side_hit_endR";
static const ALIGN_ASSET(2) char sMsOoTAnimBoomThrowWait2WaitR[] =
    "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_boom_throw_wait2waitR";
static const ALIGN_ASSET(2) char sMsOoTAnimBoomThrowR[] = "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_boom_throwR";
static const ALIGN_ASSET(2) char sMsOoTAnimBoomThrowL[] = "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_boom_throwL";
static const ALIGN_ASSET(2) char sMsOoTAnimBoomCatch[] = "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_boom_catch";
static const ALIGN_ASSET(2) char sMsOoTAnimMagicTame[] = "__OTR__oot/objects/gameplay_keep/gPlayerAnim_link_magic_tame";

// ==== Item use (Player_UseItem, Player_ActionHandler_13) ============================================================

// OoT sMagicSpellCosts[spell] for Farore's Wind (3), Nayru's Love (4), Din's Fire (5).
static u8 sMsOoTMagicSpellCosts[] = { 12, 24, 24, 12, 24, 12 };

// OoT Player_ActionToMagicSpell: 3 Farore's Wind, 4 Nayru's Love, 5 Din's Fire, else -1.
s32 MergeShip_OoTItems_ActionToMagicSpell(Player* this, s32 itemAction) {
    switch (itemAction) {
        case PLAYER_IA_OOT_FARORES_WIND:
            return 3;
        case PLAYER_IA_OOT_NAYRUS_LOVE:
            return 4;
        case PLAYER_IA_OOT_DINS_FIRE:
            return 5;
        default:
            return -1;
    }
}

// Inside Player_UseItem's "can use an item now" branch, before MM's own item kinds. OoT Player_UseItem's branches for
// spells (use with enough magic), masks (put on / take off at once) and the exchange items (shown), which MM's item
// action ranges don't cover. Returns true when it handled the item.
s32 MergeShip_OoTItems_UseItem(PlayState* play, Player* this, ItemId item, PlayerItemAction itemAction) {
    s32 spell;

    // BUG-0066: the five masks Hyrule added sit past the trade item.
    if ((itemAction < PLAYER_IA_OOT_SLINGSHOT) || (itemAction > PLAYER_IA_OOT_MASK_TRUTH)) {
        return false;
    }
    if ((this->transformation != PLAYER_FORM_HUMAN) || !MergeShip_OoTItems_Enabled()) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return true;
    }

    spell = MergeShip_OoTItems_ActionToMagicSpell(this, itemAction);
    if (spell >= 0) {
        // OoT: Farore's Wind with a point set needs no magic (it offers the warp).
        if (((itemAction != PLAYER_IA_OOT_FARORES_WIND) || (MergeShip_OoTItems_FaroresTop()->data <= 0)) &&
            ((gSaveContext.magicState != MAGIC_STATE_IDLE) ||
             (gSaveContext.save.saveInfo.playerData.magic < sMsOoTMagicSpellCosts[spell]))) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            return true;
        }
        this->itemAction = itemAction;
        this->unk_AA5 = PLAYER_UNKAA5_5;
        return true;
    }

    if (((itemAction >= PLAYER_IA_OOT_MASK_SKULL) && (itemAction <= PLAYER_IA_OOT_MASK_GERUDO)) ||
        ((itemAction >= PLAYER_IA_OOT_MASK_KEATON) && (itemAction <= PLAYER_IA_OOT_MASK_TRUTH))) { // BUG-0066
        // OoT Player_UseItem's mask branch: on or off at once, with NA_SE_PL_CHANGE_ARMS either way. One mask at a
        // time: an Ocarina of Time mask takes Majora's Mask's (non-transformation) mask off, and the other way round
        // (MergeShip_OoTItems_PlayerUpdate).
        if (MergeShip_OoTItems_WornMask() == item) {
            MergeShip_OoTItems_SetWornMask(0);
        } else {
            MergeShip_OoTItems_SetWornMask(item);
            if (this->currentMask != PLAYER_MASK_NONE) {
                this->prevMask = this->currentMask;
                this->currentMask = PLAYER_MASK_NONE;
                gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
            }
        }
        func_8082E1F0(this, NA_SE_PL_CHANGE_ARMS);
        MergeShipTest_Log("OOTITEMS mask %s", MergeShip_OoTItems_Name(MergeShip_OoTItems_WornMask()));
        return true;
    }

    if ((itemAction == PLAYER_IA_OOT_TRADE_ITEM) || (itemAction == PLAYER_IA_OOT_BOTTLE_RUTO_LETTER)) {
        // OoT: exchange items (and bottles) are taken out in Player_ActionHandler_13.
        MergeShip_OoTItems_SetShownItem(item);
        this->itemAction = itemAction;
        this->unk_AA5 = PLAYER_UNKAA5_5;
        return true;
    }

    return false;
}

void MergeShip_OoTItems_SetupMagicSpell(PlayState* play, Player* this, s32 magicSpell);
void MergeShip_OoTItems_Action_FaroresWindChoice(Player* this, PlayState* play);

// Player_ActionHandler_13 with unk_AA5 == PLAYER_UNKAA5_5, before MM's own kinds (OoT Player_ActionHandler_13's
// spell branch, and Ruto's Letter read from its bottle). Returns true when it started an action.
s32 MergeShip_OoTItems_ActionHandler13(Player* this, PlayState* play) {
    s32 spell = MergeShip_OoTItems_ActionToMagicSpell(this, this->itemAction);

    if (spell >= 0) {
        Player_StopCutscene(this);
        if ((spell != 3) || (MergeShip_OoTItems_FaroresTop()->data <= 0)) {
            MergeShip_OoTItems_SetupMagicSpell(play, this, spell); // OoT func_8083AF44
        } else {
            // OoT: Farore's Wind with a point set asks whether to return to it (message 0x3B).
            Player_SetAction(play, this, MergeShip_OoTItems_Action_FaroresWindChoice, 1);
            this->stateFlags1 |= PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000;
            Player_Anim_PlayOnce(play, this, Player_GetIdleAnim(this));
            this->csId = play->playerCsIds[PLAYER_CS_ID_ITEM_SHOW]; // OoT Player_SetTurnAroundCamera(CAM_ITEM_TYPE_4)
        }
        func_8082DAD4(this); // OoT func_80832224
        return true;
    }

    if (this->itemAction == PLAYER_IA_OOT_BOTTLE_RUTO_LETTER) {
        // OoT: shown to no one, Link reads the letter (gPlayerAnim_link_bottle_read, message 0x4005); an actor that
        // asked for an exchange gets it offered, as MM does with any exchange item.
        Actor* talkActor;
        s32 heldItemTemp = this->itemAction;

        Player_StopCutscene(this);
        this->itemAction = PLAYER_IA_NONE;
        Player_SetAction_PreserveItemAction(play, this, Player_Action_ExchangeItem, 0);
        talkActor = this->talkActor;
        this->itemAction = heldItemTemp;
        this->csId = CS_ID_NONE;

        if ((talkActor != NULL) && (this->exchangeItemAction > PLAYER_IA_NONE)) {
            this->stateFlags1 |= (PLAYER_STATE1_20000000 | PLAYER_STATE1_TALKING);
            this->csId = CS_ID_GLOBAL_TALK;
            talkActor->flags |= ACTOR_FLAG_TALK;
            this->actor.textId = 0;
            this->focusActor = this->talkActor;
            this->av1.actionVar1 = 0;
            Player_Anim_PlayOnce(play, this, &gPlayerAnim_link_normal_give_other);
        } else {
            this->stateFlags1 |= (PLAYER_STATE1_20000000 | PLAYER_STATE1_10000000 | PLAYER_STATE1_TALKING);
            this->csId = play->playerCsIds[PLAYER_CS_ID_ITEM_SHOW]; // OoT CAM_ITEM_TYPE_1
            this->av1.actionVar1 = 1;
            this->actor.textId = MergeShip_OoTItems_SetActiveOoTText(0x4005) ? CUSTOM_MESSAGE_ID : 0xFE;
            Player_Anim_PlayOnce(play, this, &gPlayerAnim_link_bottle_read);
        }
        this->actor.flags |= ACTOR_FLAG_TALK;
        this->exchangeItemAction = this->itemAction;
        func_8082DAD4(this);
        return true;
    }

    return false;
}

// ==== Magic spells (OoT func_8083AF44, Player_Action_808507F4, Player_SpawnMagicSpell) ===============================

static PlayerAnimationHeader* sMsOoTSpellAnims1[] = {
    &gPlayerAnim_link_magic_kaze1,
    &gPlayerAnim_link_magic_honoo1,
    &gPlayerAnim_link_magic_tamashii1,
};

static PlayerAnimationHeader* sMsOoTSpellAnims2[] = {
    &gPlayerAnim_link_magic_kaze2,
    &gPlayerAnim_link_magic_honoo2,
    &gPlayerAnim_link_magic_tamashii2,
};

static PlayerAnimationHeader* sMsOoTSpellAnims3[] = {
    &gPlayerAnim_link_magic_kaze3,
    &gPlayerAnim_link_magic_honoo3,
    &gPlayerAnim_link_magic_tamashii3,
};

static u8 sMsOoTSpellHoldFrames[] = { 70, 10, 10 };

static AnimSfxEntry sMsOoTSpellStartSfx[] = {
    ANIMSFX(ANIMSFX_TYPE_GENERAL, 20, NA_SE_PL_SKIP, CONTINUE),
    ANIMSFX(ANIMSFX_TYPE_VOICE, 20, NA_SE_VO_LI_SWORD_N, CONTINUE),
    ANIMSFX(ANIMSFX_TYPE_6, 26, 0, STOP),
};

static AnimSfxEntry sMsOoTSpellCastSfx[][2] = {
    {
        ANIMSFX(ANIMSFX_TYPE_8, 20, 0, CONTINUE),
        ANIMSFX(ANIMSFX_TYPE_VOICE, 30, NA_SE_VO_LI_MAGIC_FROL, STOP),
    },
    {
        ANIMSFX(ANIMSFX_TYPE_8, 20, 0, CONTINUE),
        ANIMSFX(ANIMSFX_TYPE_VOICE, 44, NA_SE_VO_LI_MAGIC_NALE, STOP),
    },
    {
        ANIMSFX(ANIMSFX_TYPE_VOICE, 20, NA_SE_VO_LI_MAGIC_ATTACK, CONTINUE),
        ANIMSFX(ANIMSFX_TYPE_GENERAL, 20, NA_SE_IT_SWORD_SWING_HARD, STOP),
    },
};

void MergeShip_OoTItems_Action_MagicSpell(Player* this, PlayState* play);

// OoT Player_SpawnMagicSpell: 0 Farore's Wind, 1 Nayru's Love, 2 Din's Fire.
Actor* MergeShip_OoTItems_SpawnMagicSpell(PlayState* play, Player* this, s32 spell) {
    static s16 sMagicSpellActorIds[] = { ACTOR_OOT_MAGIC_WIND, ACTOR_OOT_MAGIC_DARK, ACTOR_OOT_MAGIC_FIRE };

    return Actor_Spawn(&play->actorCtx, play, sMagicSpellActorIds[spell], this->actor.world.pos.x,
                       this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, 0);
}

// OoT func_8083AF44.
void MergeShip_OoTItems_SetupMagicSpell(PlayState* play, Player* this, s32 magicSpell) {
    Player_SetAction_PreserveItemAction(play, this, MergeShip_OoTItems_Action_MagicSpell, 0);

    this->av1.actionVar1 = magicSpell - 3;
    this->av2.actionVar2 = 0;

    //! OoT @bug (kept): MAGIC_CONSUME_WAIT_PREVIEW is not guaranteed to succeed.
    Magic_Consume(play, sMsOoTMagicSpellCosts[magicSpell], MAGIC_CONSUME_WAIT_PREVIEW);

    PlayerAnimation_PlayOnceSetSpeed(play, &this->skelAnime, (PlayerAnimationHeader*)sMsOoTAnimMagicTame,
                                     PLAYER_ANIM_ADJUSTED_SPEED);

    // OoT: Din's Fire uses OnePointCutscene 1100, the others Player_SetTurnAroundCamera(CAM_ITEM_TYPE_10). Majora's
    // Mask has neither; its item-show camera (the one its own shown items use) stands in for both.
    this->csId = play->playerCsIds[PLAYER_CS_ID_ITEM_SHOW];
    MergeShipTest_Log("OOTITEMS spell %d cast magic=%d", magicSpell, gSaveContext.save.saveInfo.playerData.magic);
}

// OoT Player_Action_808507F4.
void MergeShip_OoTItems_Action_MagicSpell(Player* this, PlayState* play) {
    if (PlayerAnimation_Update(play, &this->skelAnime)) {
        if (this->av1.actionVar1 < 0) {
            if ((this->itemAction == PLAYER_IA_OOT_NAYRUS_LOVE) || (gSaveContext.magicState == MAGIC_STATE_IDLE)) {
                Player_StopCutscene(this);
                func_808369F4(this, play); // OoT func_80839FFC
            }
        } else {
            if (this->av2.actionVar2 == 0) {
                PlayerAnimation_PlayOnceSetSpeed(play, &this->skelAnime, sMsOoTSpellAnims1[this->av1.actionVar1],
                                                 PLAYER_ANIM_ADJUSTED_SPEED);

                if (MergeShip_OoTItems_SpawnMagicSpell(play, this, this->av1.actionVar1) != NULL) {
                    this->stateFlags1 |= PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000;
                    if ((this->av1.actionVar1 != 0) || (MergeShip_OoTItems_FaroresTop()->data <= 0)) {
                        gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
                    }
                } else {
                    Magic_Reset(play);
                }
            } else {
                PlayerAnimation_PlayLoopSetSpeed(play, &this->skelAnime, sMsOoTSpellAnims2[this->av1.actionVar1],
                                                 PLAYER_ANIM_ADJUSTED_SPEED);

                if (this->av1.actionVar1 == 0) {
                    this->av2.actionVar2 = -10;
                }
            }

            this->av2.actionVar2++;
        }
    } else {
        if (this->av2.actionVar2 < 0) {
            this->av2.actionVar2++;

            if (this->av2.actionVar2 == 0) {
                // OoT: the point is set (respawn[TOP] from the player, data 1) and saved as gSaveContext.fw from
                // respawn[DOWN]; here respawn[TOP] is MergeShip's own (see MergeShip_OoTItems_FaroresTop).
                RespawnData* top = MergeShip_OoTItems_FaroresTop();
                RespawnData down = gSaveContext.respawn[RESPAWN_MODE_DOWN];

                top->entrance = gSaveContext.save.entrance;
                top->roomIndex = play->roomCtx.curRoom.num;
                top->pos = this->actor.world.pos;
                top->yaw = this->actor.shape.rot.y;
                top->playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_WARP_SONG);
                top->tempSwitchFlags = play->actorCtx.sceneFlags.switches[2];
                top->unk_18 = play->actorCtx.sceneFlags.collectible[1];
                top->tempCollectFlags = play->actorCtx.sceneFlags.collectible[2];
                top->data = 1;
                down.playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_WARP_SONG);
                MergeShip_OoTItems_SetFaroresWind(&down);
                MergeShipTest_Log("OOTITEMS farore set entrance=0x%04X room=%d pos=(%.0f,%.0f,%.0f)", top->entrance,
                                  top->roomIndex, top->pos.x, top->pos.y, top->pos.z);
                this->av2.actionVar2 = 2;
            }
        } else if (this->av1.actionVar1 >= 0) {
            if (this->av2.actionVar2 == 0) {
                Player_PlayAnimSfx(this, sMsOoTSpellStartSfx);
            } else if (this->av2.actionVar2 == 1) {
                Player_PlayAnimSfx(this, sMsOoTSpellCastSfx[this->av1.actionVar1]);
                if ((this->av1.actionVar1 == 2) && PlayerAnimation_OnFrame(&this->skelAnime, 30.0f)) {
                    this->stateFlags1 &= ~(PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000);
                }
            } else if (sMsOoTSpellHoldFrames[this->av1.actionVar1] < this->av2.actionVar2++) {
                PlayerAnimation_PlayOnceSetSpeed(play, &this->skelAnime, sMsOoTSpellAnims3[this->av1.actionVar1],
                                                 PLAYER_ANIM_ADJUSTED_SPEED);
                this->yaw = this->actor.shape.rot.y;
                this->av1.actionVar1 = -1;
            }
        }
    }

    Player_StopHorizontalMovement(this); // OoT Player_DecelerateToZero
}

// OoT Player_Action_8085063C: "Return to the Warp Point?" (message 0x3B, Ocarina of Time's own text).
void MergeShip_OoTItems_Action_FaroresWindChoice(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_20; // OoT PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET

    PlayerAnimation_Update(play, &this->skelAnime);
    Player_UpdateUpperBody(this, play);

    if (this->av2.actionVar2 == 0) {
        if (MergeShip_OoTItems_SetActiveOoTText(0x3B)) {
            Message_StartTextbox(play, CUSTOM_MESSAGE_ID, &this->actor);
        }
        this->av2.actionVar2 = 1;
        return;
    }

    // OoT's choice box closes itself (TEXT_STATE_CLOSING); MM's waits in TEXT_STATE_CHOICE for the caller to read the
    // choice once A is pressed and close it (MM Message_ShouldAdvance + Message_CloseTextbox).
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
        RespawnData* top = MergeShip_OoTItems_FaroresTop();

        Message_CloseTextbox(play);
        MergeShipTest_Log("OOTITEMS farore choice %d", play->msgCtx.choiceIndex);
        if (play->msgCtx.choiceIndex == 0) { // Returns to the point
            MergeShip_OoTItems_FaroresWindWarp(play);
            return;
        }

        if (play->msgCtx.choiceIndex == 1) { // Unsets the point
            top->data = -top->data;
            MergeShip_OoTItems_ClearFaroresWind();
            Audio_PlaySfx_AtPos(&top->pos, NA_SE_PL_MAGIC_WIND_VANISH);
        }

        Player_StopCutscene(this);
        func_80839E74(this, play); // OoT func_80853080
    }
}

// OoT Player_Action_8085076C: arriving by Farore's Wind (invisible for 20 frames, then dropped 60 units above).
void MergeShip_OoTItems_Action_FaroresWindArrive(Player* this, PlayState* play) {
    RespawnData* top = MergeShip_OoTItems_FaroresTop();

    if (this->av2.actionVar2 > 20) {
        this->actor.draw = Player_Draw;
        this->actor.world.pos.y += 60.0f;
        func_80833AA0(this, play); // OoT func_80837B9C: fall
        return;
    }

    if (this->av2.actionVar2++ == 20) {
        top->data++;
        Audio_PlaySfx_AtPos(&top->pos, NA_SE_PL_MAGIC_WIND_WARP);
    }
}

// Player_Init, after MM's start mode: OoT Player_StartMode_FaroresWind, and OoT Player_Init's Nayru's Love restore.
void MergeShip_OoTItems_PlayerInit(PlayState* play, Player* this) {
    if (this->actor.id != ACTOR_PLAYER) {
        return;
    }
    if (MergeShip_OoTItems_TakeFaroresArrival()) {
        this->actor.draw = NULL;
        Player_SetAction(play, this, MergeShip_OoTItems_Action_FaroresWindArrive, 0);
        this->stateFlags1 |= PLAYER_STATE1_20000000;
        this->av2.actionVar2 = 0;
        MergeShipTest_Log("OOTITEMS farore arrive pos=(%.0f,%.0f,%.0f)", this->actor.world.pos.x,
                          this->actor.world.pos.y, this->actor.world.pos.z);
    }
    if (gMergeShipNayrusLoveTimer != 0) {
        gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
        MergeShip_OoTItems_SpawnMagicSpell(play, this, 1);
    }
}

// Once a frame from Player_Update: masks stay one at a time, and only the human wears an Ocarina of Time mask.
void MergeShip_OoTItems_RocsUpdate(PlayState* play, Player* this);

void MergeShip_OoTItems_PlayerUpdate(PlayState* play, Player* this) {
    if (this->actor.id != ACTOR_PLAYER) {
        return;
    }
    MergeShip_OoTItems_RocsUpdate(play, this); // BUG-0011 / BUG-0014
    if ((MergeShip_OoTItems_WornMask() != 0) &&
        ((this->currentMask != PLAYER_MASK_NONE) || (this->transformation != PLAYER_FORM_HUMAN))) {
        MergeShip_OoTItems_SetWornMask(0);
    }
    // OoT Player_Update's PLAYER_STATE3_RESTORE_NAYRUS_LOVE: Nayru's Love's diamond vanishes while a song plays
    // (OoT_Magic_Dark kills itself on the ocarina) and comes back once the song is over, the timer still running.
    // OoT's song effects raise a flag when they end; Majora's Mask's have none, so the diamond returns once no message
    // or song is up any more and no diamond is out.
    if ((gMergeShipNayrusLoveTimer != 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (gSaveContext.magicState == MAGIC_STATE_IDLE)) {
        Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;

        while ((actor != NULL) && (actor->id != ACTOR_OOT_MAGIC_DARK)) {
            actor = actor->next;
        }
        if (actor == NULL) {
            gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
            MergeShip_OoTItems_SpawnMagicSpell(play, this, 1);
        }
    }
    // OoT Player_Update's gravity rule while Farore's Wind's point is appearing (Player_Action_8084411C).
    if ((MergeShip_OoTItems_FaroresTop()->data > 40) &&
        (MergeShip_OoTItems_Action_FaroresWindArrive == this->actionFunc)) {
        this->actor.gravity = 0.0f;
    }
}

// ==== Slingshot (OoT func_80834380, Player_InitBowOrSlingshotIA) =====================================================

// func_808305BC for the Slingshot: OoT's item and arrow type, and its ammo (the Deku Seeds). Returns the ammo, or -1
// when this isn't the Slingshot.
s32 MergeShip_OoTItems_SlingshotAmmo(PlayState* play, Player* this, ItemId* item, ArrowType* typeParam) {
    if (this->heldItemAction != PLAYER_IA_OOT_SLINGSHOT) {
        return -1;
    }
    *item = ITEM_SLINGSHOT;
    *typeParam = ARROW_TYPE_SLINGSHOT;
    if (this->stateFlags3 & PLAYER_STATE3_400) {
        return 1;
    }
    if (gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE) {
        return play->interfaceCtx.minigameAmmo;
    }
    return gSaveContext.save.shipSaveInfo.mergeShipOoTItems.seeds;
}

// OoT D_80854398 / D_808543DC: the Slingshot's draw and empty sounds instead of the bow's.
u16 MergeShip_OoTItems_BowSfx(Player* this, u16 bowSfx) {
    if (this->heldItemAction == PLAYER_IA_OOT_SLINGSHOT) {
        if (bowSfx == NA_SE_PL_BOW_DRAW) {
            return NA_SE_IT_SLING_DRAW;
        }
        if (bowSfx == NA_SE_IT_BOW_FLICK) {
            return NA_SE_IT_SLING_FLICK;
        }
    }
    return bowSfx;
}

// ==== Boomerang (OoT Player_InitBoomerangIA, func_80835800 .. func_80835C08) =========================================

s32 MergeShip_OoTItems_UpperAction_BoomerangWait(Player* this, PlayState* play);
s32 MergeShip_OoTItems_UpperAction_BoomerangAimStart(Player* this, PlayState* play);
s32 MergeShip_OoTItems_UpperAction_BoomerangAim(Player* this, PlayState* play);
s32 MergeShip_OoTItems_UpperAction_BoomerangThrow(Player* this, PlayState* play);
s32 MergeShip_OoTItems_UpperAction_BoomerangFlying(Player* this, PlayState* play);
s32 MergeShip_OoTItems_UpperAction_BoomerangCatch(Player* this, PlayState* play);

void MergeShip_OoTItems_InitBoomerangIA(PlayState* play, Player* this) {
    this->stateFlags1 |= PLAYER_STATE1_USING_ZORA_BOOMERANG;
}

// OoT func_80834D2C's boomerang branch + func_80834F2C.
static s32 MergeShip_OoTItems_BoomerangStartAim(Player* this, PlayState* play) {
    if ((this->doorType == PLAYER_DOORTYPE_NONE) && !(this->stateFlags1 & PLAYER_STATE1_ZORA_BOOMERANG_THROWN)) {
        if (sPlayerUseHeldItem || func_80830F9C(play)) {
            Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangAimStart);
            this->unk_ACC = 10;
            PlayerAnimation_PlayOnce(play, &this->skelAnimeUpper, (PlayerAnimationHeader*)sMsOoTAnimBoomThrowWait2WaitR);

            if (this->stateFlags1 & PLAYER_STATE1_800000) {
                Player_Anim_PlayLoop(play, this, &gPlayerAnim_link_uma_anim_walk);
            } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !Player_UpdateHostileLockOn(this)) {
                Player_Anim_PlayLoop(play, this, D_8085BE84[PLAYER_ANIMGROUP_wait][this->modelAnimType]);
            }
            return func_80831010(this, play); // OoT func_80834EB8
        }
    }
    return false;
}

// OoT func_80835800.
s32 MergeShip_OoTItems_UpperAction_BoomerangWait(Player* this, PlayState* play) {
    if (func_80830B88(play, this)) {
        return true;
    }

    if (this->stateFlags1 & PLAYER_STATE1_ZORA_BOOMERANG_THROWN) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangFlying);
    } else if (MergeShip_OoTItems_BoomerangStartAim(this, play)) {
        return true;
    }

    return false;
}

// OoT func_80835884.
s32 MergeShip_OoTItems_UpperAction_BoomerangAimStart(Player* this, PlayState* play) {
    if (PlayerAnimation_Update(play, &this->skelAnimeUpper)) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangAim);
        PlayerAnimation_PlayLoop(play, &this->skelAnimeUpper, &gPlayerAnim_link_boom_throw_waitR);
    }

    func_80831010(this, play);

    return true;
}

// OoT func_808358F0.
s32 MergeShip_OoTItems_UpperAction_BoomerangAim(Player* this, PlayState* play) {
    PlayerAnimationHeader* anim = this->skelAnime.animation;

    if ((func_8082EF54(this) == anim) || (func_8082EF9C(this) == anim)) {
        AnimTaskQueue_AddCopy(play, this->skelAnime.limbCount, this->skelAnimeUpper.jointTable,
                              this->skelAnime.jointTable);
    } else {
        PlayerAnimation_Update(play, &this->skelAnimeUpper);
    }

    func_80831010(this, play);

    if (!sPlayerHeldItemButtonIsHeldDown) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangThrow);
        PlayerAnimation_PlayOnce(play, &this->skelAnimeUpper,
                                 (this->unk_B40 < 0.5f) ? (PlayerAnimationHeader*)sMsOoTAnimBoomThrowR
                                                        : (PlayerAnimationHeader*)sMsOoTAnimBoomThrowL);
    }

    return true;
}

// OoT func_808359FC: the throw, on frame 6.
s32 MergeShip_OoTItems_UpperAction_BoomerangThrow(Player* this, PlayState* play) {
    if (PlayerAnimation_Update(play, &this->skelAnimeUpper)) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangFlying);
        this->unk_ACC = 0;
    } else if (PlayerAnimation_OnFrame(&this->skelAnimeUpper, 6.0f)) {
        f32 posX = (Math_SinS(this->actor.shape.rot.y) * 10.0f) + this->actor.world.pos.x;
        f32 posZ = (Math_CosS(this->actor.shape.rot.y) * 10.0f) + this->actor.world.pos.z;
        s32 yaw = (this->focusActor != NULL) ? this->actor.shape.rot.y + 14000 : this->actor.shape.rot.y;
        OoTEnBoom* boomerang =
            (OoTEnBoom*)Actor_Spawn(&play->actorCtx, play, ACTOR_OOT_EN_BOOM, posX, this->actor.world.pos.y + 30.0f,
                                    posZ, this->actor.focus.rot.x, yaw, 0, 0);

        this->zoraBoomerangActor = &boomerang->actor;

        if (boomerang != NULL) {
            boomerang->moveTo = this->focusActor;
            boomerang->returnTimer = 20;
            this->stateFlags1 |= PLAYER_STATE1_ZORA_BOOMERANG_THROWN;

            if (!Player_CheckHostileLockOn(this)) {
                Player_SetParallel(this);
            }

            this->unk_D57 = 4; // OoT unk_A73
            Player_PlaySfx(this, NA_SE_IT_BOOMERANG_THROW);
            Player_AnimSfx_PlayVoice(this, NA_SE_VO_LI_SWORD_N);
            MergeShipTest_Log("OOTITEMS boomerang thrown yaw=%d target=%p", yaw, (void*)this->focusActor);
        }
    }

    return true;
}

// OoT func_80835B60: waiting for the catch.
s32 MergeShip_OoTItems_UpperAction_BoomerangFlying(Player* this, PlayState* play) {
    if (func_80830B88(play, this)) {
        return true;
    }

    if (!(this->stateFlags1 & PLAYER_STATE1_ZORA_BOOMERANG_THROWN)) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangCatch);
        PlayerAnimation_PlayOnce(play, &this->skelAnimeUpper, (PlayerAnimationHeader*)sMsOoTAnimBoomCatch);
        // OoT func_808357E8(this, gPlayerLeftHandBoomerangDLs): the boomerang is back in the fist (LH_4 draws it,
        // MergeShipOoTItemsDraw.inc).
        Player_PlaySfx(this, NA_SE_PL_CATCH_BOOMERANG);
        Player_AnimSfx_PlayVoice(this, NA_SE_VO_LI_SWORD_N);
        MergeShipTest_Log("OOTITEMS boomerang caught");
        return true;
    }

    return false;
}

// OoT func_80835C08.
s32 MergeShip_OoTItems_UpperAction_BoomerangCatch(Player* this, PlayState* play) {
    if (!MergeShip_OoTItems_UpperAction_BoomerangWait(this, play) &&
        PlayerAnimation_Update(play, &this->skelAnimeUpper)) {
        Player_SetUpperAction(play, this, MergeShip_OoTItems_UpperAction_BoomerangWait);
    }

    return true;
}

// ==== Megaton Hammer (OoT func_80837818's hammer branch, func_80842D5C's ground hit, D_80854488[hammer]) ==============

// The hammer swings use MM's two unused backslash attack slots (after the jump attacks, before the Goron punches,
// where OoT has PLAYER_MWA_HAMMER_FORWARD / _SIDE), so every "< PLAYER_MWA_SPIN_ATTACK_1H" rule treats them as OoT does.
#define MS_OOT_MWA_HAMMER_FORWARD PLAYER_MWA_BACKSLASH_RIGHT
#define MS_OOT_MWA_HAMMER_SIDE PLAYER_MWA_BACKSLASH_LEFT

// OoT sMeleeAttackAnims[PLAYER_MWA_HAMMER_FORWARD / _SIDE].
static AttackAnimInfo sMsOoTHammerAttackAnimInfo[] = {
    { (PlayerAnimationHeader*)sMsOoTAnimHammerHit, (PlayerAnimationHeader*)sMsOoTAnimHammerHitEnd,
      (PlayerAnimationHeader*)sMsOoTAnimHammerHitEndR, 3, 10 },
    { (PlayerAnimationHeader*)sMsOoTAnimHammerSideHit, (PlayerAnimationHeader*)sMsOoTAnimHammerSideHitEnd,
      (PlayerAnimationHeader*)sMsOoTAnimHammerSideHitEndR, 2, 11 },
};

AttackAnimInfo* MergeShip_OoTItems_AttackAnimInfo(Player* this, s32 meleeWeaponAnim) {
    if ((this->heldItemAction == PLAYER_IA_OOT_HAMMER) && (meleeWeaponAnim == MS_OOT_MWA_HAMMER_FORWARD)) {
        return &sMsOoTHammerAttackAnimInfo[0];
    }
    if ((this->heldItemAction == PLAYER_IA_OOT_HAMMER) && (meleeWeaponAnim == MS_OOT_MWA_HAMMER_SIDE)) {
        return &sMsOoTHammerAttackAnimInfo[1];
    }
    return &sMeleeAttackAnimInfo[meleeWeaponAnim];
}

// OoT func_80837818's hammer branch (D_80854484 by the stick direction; the combo counter restarts). Returns -1 when
// this isn't the hammer.
s32 MergeShip_OoTItems_HammerAttack(Player* this) {
    static s8 sHammerAttacks[] = {
        MS_OOT_MWA_HAMMER_FORWARD, // PLAYER_STICK_DIR_FORWARD
        MS_OOT_MWA_HAMMER_SIDE,    // PLAYER_STICK_DIR_LEFT
        MS_OOT_MWA_HAMMER_FORWARD, // PLAYER_STICK_DIR_BACKWARD
        MS_OOT_MWA_HAMMER_SIDE,    // PLAYER_STICK_DIR_RIGHT
    };
    s32 controlStickDirection = this->controlStickDirections[this->controlStickDataIndex];

    if (this->heldItemAction != PLAYER_IA_OOT_HAMMER) {
        return -1;
    }
    if (controlStickDirection <= PLAYER_STICK_DIR_NONE) {
        controlStickDirection = PLAYER_STICK_DIR_FORWARD;
    }
    this->unk_ADD = 0;
    return sHammerAttacks[controlStickDirection];
}

// After func_8083375C set the weapon quads: OoT D_80854488[hammer] = { DMG_HAMMER_SWING, DMG_HAMMER_JUMP }, mapped to
// Majora's Mask's blunt strikes: the swing to DMG_GORON_PUNCH (breaks what a Goron's punch breaks, as the hammer's
// swing breaks rocks and rusted switches in OoT), the jump attack to DMG_GORON_POUND (MM's ground-slam damage).
void MergeShip_OoTItems_HammerDamage(Player* this, s32 meleeWeaponAnim) {
    u32 dmgFlags;

    if (this->heldItemAction != PLAYER_IA_OOT_HAMMER) {
        return;
    }
    dmgFlags = ((meleeWeaponAnim >= PLAYER_MWA_FLIPSLASH_START) && (meleeWeaponAnim <= PLAYER_MWA_ZORA_JUMPKICK_FINISH))
                   ? DMG_GORON_POUND
                   : DMG_GORON_PUNCH;
    this->meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags = dmgFlags;
    this->meleeWeaponQuads[1].elem.atDmgInfo.dmgFlags = dmgFlags;
}

// func_808401F4 when the weapon's tip meets the floor or a wall: OoT func_80842D5C's hammer branch (hit stop,
// func_80842A28's quake, rumble, NA_SE_IT_HAMMER_HIT and actorCtx.unk_02 = 4 -- MM's actorCtx.unk2, which stuns the
// same enemies -- then the rebound). OoT spawns no shockwave here; its ring is the swing's own (below).
s32 MergeShip_OoTItems_HammerHit(PlayState* play, Player* this) {
    if (this->heldItemAction != PLAYER_IA_OOT_HAMMER) {
        return false;
    }
    func_8082DF2C(play);
    Player_RequestQuakeAndRumble(play, this, NA_SE_IT_HAMMER_HIT);
    play->actorCtx.unk2 = 4;
    func_808400CC(play, this);
    MergeShipTest_Log("OOTITEMS hammer hit ground pos=(%.0f,%.0f,%.0f)", this->actor.world.pos.x,
                      this->actor.world.pos.y, this->actor.world.pos.z);
    return true;
}

// Player_Action_84 while the swing plays: OoT Player_Action_808502D0's hammer branch. The forward swing's head lands
// on frame 7 (the jump attack's finish on frame 2); with ground within 40 units of the spot in front of him (OoT
// D_80854A40), func_80842A28 (quake, rumble, NA_SE_IT_HAMMER_HIT, actorCtx.unk_02 = 4 -- MM's unk2, which flips and
// stuns the enemies that read it) and the white shockwave ring on that ground. Meanwhile his focus pitches down
// towards the head (func_80836AB8(this, true): MM's func_80832754).
void MergeShip_OoTItems_HammerSwing(PlayState* play, Player* this) {
    static Vec3f sHeadOffset = { 0.0f, 40.0f, 45.0f }; // OoT D_80854A40
    Vec3f shockwavePos;
    f32 drop;

    if ((this->heldItemAction != PLAYER_IA_OOT_HAMMER) ||
        ((this->meleeWeaponAnimation != MS_OOT_MWA_HAMMER_FORWARD) &&
         (this->meleeWeaponAnimation != PLAYER_MWA_JUMPSLASH_FINISH))) {
        return;
    }
    shockwavePos.y = func_80835D2C(play, this, &sHeadOffset, &shockwavePos);
    drop = this->actor.world.pos.y - shockwavePos.y;

    // OoT Math_Atan2S(45.0f, drop): its first parameter is the run, MM's is the rise.
    Math_ScaledStepToS(&this->actor.focus.rot.x, Math_Atan2S(drop, 45.0f), 800);
    func_80832754(this, true);

    if ((((this->meleeWeaponAnimation == MS_OOT_MWA_HAMMER_FORWARD) &&
          PlayerAnimation_OnFrame(&this->skelAnime, 7.0f)) ||
         ((this->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_FINISH) &&
          PlayerAnimation_OnFrame(&this->skelAnime, 2.0f))) &&
        (drop > -40.0f) && (drop < 40.0f)) {
        Player_RequestQuakeAndRumble(play, this, NA_SE_IT_HAMMER_HIT);
        play->actorCtx.unk2 = 4;
        EffectSsBlast_SpawnWhiteShockwave(play, &shockwavePos, &gZeroVec3f, &gZeroVec3f);
        MergeShipTest_Log("OOTITEMS hammer shockwave pos=(%.0f,%.0f,%.0f)", shockwavePos.x, shockwavePos.y,
                          shockwavePos.z);
    }
}

// ---- SoH's Roc's Feather and Hyrule's Deku Beans in Termina (BUG-0011, BUG-0012, BUG-0014) -----------------------------
//
// The Roc's Feather (SoH RocsFeather.cpp): a hop, once until Link has stood on the ground for three frames -- any form
// (the Goron curled up or rolling: the ball's own bounce, MergeShip_OoTItems_RocsBallJump, as the roll never reaches
// the item buttons). Hyrule's Deku Beans: the Deku planting the Magic Beans (which the Deku cannot use in Majora's Mask)
// plants a Deku Flower (Majora's Mask's own, of the kind Hyrule has: gold with the Gold Deku Beans), from the beans'
// own count when they are ammo (MergeShipOoTItemsSave dekuBeans / goldBeans, BUG-0105: Termina's, not Hyrule's); at
// most three stand at a time.

static u8 sMsRocsUsed = false;
static u8 sMsRocsGroundFrames = 0;
#define MS_BEAN_FLOWERS_MAX 3
static Actor* sMsBeanFlowers[MS_BEAN_FLOWERS_MAX];
static PlayState* sMsBeanFlowersPlay = NULL;

extern s32 gMergeShipRocsJumpMounted; // MergeShipOoTItems.cpp
static const ALIGN_ASSET(2) char sMsRocsJumpAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump";

static void MergeShip_OoTItems_RocsJump(PlayState* play, Player* this) {
    // BUG-0079: as SoH's (RocsFeather.cpp): young Link 7.0 up, the adult 7.5 (and the Fierce Deity, his size), forward
    // 5; the ripple and splash at his home position, 1.5 times as large for the adult; a hop (side hop, backflip)
    // ends, so a ledge can be grabbed again. BUG-0161: SoH's own feather jump (gPlayerAnim_link_rocs_feather_jump,
    // from soh.o2r), and actionVar2 1 as SoH sets it -- Player_Action_25 then holds that pose to the ground instead of
    // morphing into the landing pose as soon as Link starts to fall.
    const s32 adult = MergeShip_AdultLink_IsActive(this) || (this->transformation == PLAYER_FORM_FIERCE_DEITY);
    const f32 effectsScale = adult ? 1.5f : 1.0f;
    Vec3f pos;

    if (sMsRocsUsed) {
        return;
    }
    sMsRocsUsed = true;
    if ((this->transformation == PLAYER_FORM_GORON) && (Player_Action_96 == this->actionFunc)) {
        func_80834CD0(this, 12.0f, 0);
    } else {
        const s32 humanLike =
            (this->transformation == PLAYER_FORM_HUMAN) || (this->transformation == PLAYER_FORM_FIERCE_DEITY);

        func_80834D50(play, this,
                      humanLike ? (gMergeShipRocsJumpMounted ? (PlayerAnimationHeader*)sMsRocsJumpAnim
                                                             : &gPlayerAnim_link_normal_jump)
                                : NULL,
                      (this->transformation == PLAYER_FORM_DEKU) ? 6.0f : (adult ? 7.5f : 7.0f), 0);
        if (humanLike) {
            this->av2.actionVar2 = 1;
        }
        this->speedXZ = 5.0f;
        this->actor.world.rot.y = this->yaw = this->actor.shape.rot.y;
    }
    this->stateFlags2 &= ~PLAYER_STATE2_80000; // MM's hopping flag (OoT PLAYER_STATE2_HOPPING)
    pos = this->actor.home.pos;
    pos.y += 3.0f;
    EffectSsGRipple_Spawn(play, &pos, (s16)(200 * effectsScale), (s16)(300 * effectsScale), 1);
    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, (s16)(150 * effectsScale));
    Player_PlaySfx(this, NA_SE_PL_SKIP);
    MergeShipTest_Log("OOTITEMS Roc's Feather hop form=%d vy=%.1f ball=%d", this->transformation, this->actor.velocity.y,
                      (Player_Action_96 == this->actionFunc) ? 1 : 0);
}

static void MergeShip_OoTItems_PlantDekuBean(PlayState* play, Player* this) {
    MsSharedSave* shared = (MsSharedSave*)MergeShipHost_SharedSave();
    s32 gold;
    u8* ammo = NULL;
    s16 yaw = this->actor.shape.rot.y;
    Vec3f pos = { this->actor.world.pos.x + Math_SinS(yaw) * 40.0f, this->actor.world.pos.y + 30.0f,
                  this->actor.world.pos.z + Math_CosS(yaw) * 40.0f };
    CollisionPoly* poly = NULL;
    s32 bgId;
    f32 floorY;
    Actor* flower;
    s32 i;

    if ((shared == NULL) || !(shared->hyruleExtras & MS_HYRULE_EXTRA_VALID) ||
        !(shared->hyruleExtras & MS_HYRULE_EXTRA_DEKU_BEANS) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    gold = (shared->hyruleExtras & MS_HYRULE_EXTRA_GOLD_BEANS) != 0;
    if (shared->hyruleExtras & MS_HYRULE_EXTRA_BEAN_AMMO) {
        MergeShipOoTItemsSave* own = &gSaveContext.save.shipSaveInfo.mergeShipOoTItems;

        ammo = gold ? &own->goldBeans : &own->dekuBeans; // count + 1; 0: never had any, starts full
        if (*ammo == 0) {
            *ammo = 10 + 1;
        }
        if (*ammo == 1) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            return;
        }
    }
    floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pos);
    if ((poly == NULL) || (fabsf(floorY - this->actor.world.pos.y) > 30.0f)) {
        pos = this->actor.world.pos; // a wall or a drop in front: at his feet
        floorY = pos.y;
    }
    pos.y = floorY;

    // The flowers stand until the scene changes; the oldest goes for a fourth.
    if (sMsBeanFlowersPlay != play) {
        memset(sMsBeanFlowers, 0, sizeof(sMsBeanFlowers));
        sMsBeanFlowersPlay = play;
    }
    for (i = 0; i < MS_BEAN_FLOWERS_MAX; i++) {
        if ((sMsBeanFlowers[i] != NULL) && (sMsBeanFlowers[i]->update == NULL)) {
            sMsBeanFlowers[i] = NULL;
        }
    }
    if (sMsBeanFlowers[MS_BEAN_FLOWERS_MAX - 1] != NULL) {
        Actor_Kill(sMsBeanFlowers[0]);
        memmove(&sMsBeanFlowers[0], &sMsBeanFlowers[1], sizeof(Actor*) * (MS_BEAN_FLOWERS_MAX - 1));
        sMsBeanFlowers[MS_BEAN_FLOWERS_MAX - 1] = NULL;
    }
    flower = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_ETCETERA, pos.x, pos.y, pos.z, 0, yaw, 0,
                         DEKU_FLOWER_PARAMS(gold ? DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE
                                                 : DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE));
    if (flower == NULL) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    flower->room = -1;
    for (i = 0; i < MS_BEAN_FLOWERS_MAX; i++) {
        if (sMsBeanFlowers[i] == NULL) {
            sMsBeanFlowers[i] = flower;
            break;
        }
    }
    if (ammo != NULL) {
        (*ammo)--;
    }
    Actor_PlaySfx(flower, NA_SE_PL_PLANT_GROW_UP);
    MergeShipTest_Log("OOTITEMS Deku Bean planted (%s) left=%d", gold ? "gold" : "deku", (ammo != NULL) ? *ammo - 1 : -1);
}

// Player_ProcessItemButtons, before Player_UseItem: true when this used the item.
s32 MergeShip_OoTItems_UseInstant(PlayState* play, Player* this, u8 item) {
    if (item == ITEM_OOT_ROCS_FEATHER) {
        MergeShip_OoTItems_RocsJump(play, this);
        return true;
    }
    if ((item == ITEM_MAGIC_BEANS) && (this->transformation == PLAYER_FORM_DEKU)) {
        MergeShip_OoTItems_PlantDekuBean(play, this);
        return true;
    }
    return false;
}

// Player_Action_96 (curled up or rolling): the Roc's Feather on a C button or the D-pad makes the ball jump.
void MergeShip_OoTItems_RocsBallJump(PlayState* play, Player* this) {
    static const u16 sC[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
    static const u16 sD[] = { BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sC); i++) {
        if (CHECK_BTN_ALL(sPlayerControlInput->press.button, sC[i]) &&
            (Player_GetItemOnButton(play, this, EQUIP_SLOT_C_LEFT + i) == ITEM_OOT_ROCS_FEATHER)) {
            MergeShip_OoTItems_RocsJump(play, this);
            return;
        }
    }
    for (i = 0; i < ARRAY_COUNT(sD); i++) {
        if (CHECK_BTN_ALL(sPlayerControlInput->press.button, sD[i]) &&
            (Player_Dpad_GetItemOnButton(play, this, EQUIP_SLOT_D_RIGHT + i) == ITEM_OOT_ROCS_FEATHER)) {
            MergeShip_OoTItems_RocsJump(play, this);
            return;
        }
    }
}

// Every frame: the hop comes back after three frames on the ground; every form may use the Roc's Feather, and the Deku
// the Magic Beans while Hyrule has its Deku Beans.
void MergeShip_OoTItems_RocsUpdate(PlayState* play, Player* this) {
    const MsSharedSave* shared = (const MsSharedSave*)MergeShipHost_SharedSave();
    s32 f;

    (void)play;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        if (sMsRocsGroundFrames < 3) {
            sMsRocsGroundFrames++;
        }
    } else {
        sMsRocsGroundFrames = 0;
    }
    if (sMsRocsGroundFrames >= 3) {
        sMsRocsUsed = false;
    }
    for (f = 0; f < PLAYER_FORM_MAX; f++) {
        gPlayerFormItemRestrictions[f][ITEM_OOT_ROCS_FEATHER] = true;
    }
    gPlayerFormItemRestrictions[PLAYER_FORM_DEKU][ITEM_MAGIC_BEANS] =
        (shared != NULL) && (shared->hyruleExtras & MS_HYRULE_EXTRA_VALID) &&
        (shared->hyruleExtras & MS_HYRULE_EXTRA_DEKU_BEANS);
}
