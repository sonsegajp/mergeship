// MergeShip2 (soh_fd) -- the player half of the features carried over from soh_fd / soh_fd_deku (MergeShipSohFd.h).
//
// Included at the end of z_player.c (after MergeShipPlayerTransform.inc), so it can use that file's statics:
// sAgeProperties, the action functions, MergeShip_StartTransformation and MergeShip_ApplyDebugForm.

#include "soh/MergeShip/MergeShipSohFd.h"
#include "soh/MergeShip/MergeShipSohFdMessages.h"

// ---- turning back into Link --------------------------------------------------------------------------------------

// A revert asked for while it could not start (Navi's "Yes" is answered inside a text box).
static s32 sSohFdRevertPending = false;

// soh_fd's white-fade revert, on Majora's Mask's transformation: the form takes its mask off and the sequence goes
// straight to its white flash, as a skipped transformation does (MergeShip_Action_PutOnMask), so the player is
// rebuilt as Link behind the flash and MergeShip_Action_TransformEnd fades it out.
static s32 MergeShip_SohFd_RevertWithFlash(PlayState* play, Player* this) {
    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm) || this->mergeShipTransforming) {
        return false;
    }
    this->prevMask = this->currentMask;
    this->currentMask = PLAYER_MASK_NONE;
    MergeShip_StartTransformation(play, this, (u8)gSaveContext.linkAge);
    R_PLAY_FILL_SCREEN_ON = 45;
    R_PLAY_FILL_SCREEN_R = 220;
    R_PLAY_FILL_SCREEN_G = 220;
    R_PLAY_FILL_SCREEN_B = 220;
    R_PLAY_FILL_SCREEN_ALPHA = 0;
    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_SY_TRANSFORM_MASK_FLASH);
    return true;
}

// Link again at once, without a flash: the form becomes Link where he stands, as MM's Player_Init builds a player
// in his saved form (MergeShip_ApplyDebugForm).
static void MergeShip_SohFd_RevertInstantly(PlayState* play, Player* this) {
    if (MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        MergeShip_ApplyDebugForm(this, play, (u8)gSaveContext.linkAge);
        this->prevMask = this->currentMask = PLAYER_MASK_NONE;
        MergeShip_SohFd_AfterFormChange(this, (u8)gSaveContext.linkAge);
    }
}

// ---- Hyrule's masks over a form ("Forms Wear Trade Masks") -------------------------------------------------------
// A form's currentMask is its own mask, so the Hyrule mask worn over it is kept aside (MergeShip_SohFd_FormTradeMask)
// and drawn from the form's head (Player_DrawGameplay). Actors see it through Player_GetMask.

// Player_UseItem, a Hyrule mask chosen while transformed: worn over the form, or taken off if it is the one worn, with
// Link's own sound. Returns false with the option off (MergeShip's refusal).
s32 MergeShip_SohFd_FormUseTradeMask(Player* this, s32 itemAction) {
    u8 mask = itemAction - PLAYER_IA_MASK_KEATON + 1;

    if (!MergeShip_SohFd_FormsWearTradeMasks()) {
        return false;
    }
    if (MergeShip_SohFd_FormTradeMask() == mask) {
        mask = PLAYER_MASK_NONE;
    }
    MergeShip_SohFd_SetFormTradeMask(mask);
    gSaveContext.ship.maskMemory = mask;
    func_808328EC(this, NA_SE_PL_CHANGE_ARMS);
    return true;
}

// Player_ProcessItemButtons: a mask worn over a form comes off once it is on no button, as Link's does -- unless
// "Persistent Masks" keeps masks on (VB_PLAYER_UNEQUIP_MASK_WITHOUT_BUTTON).
void MergeShip_SohFd_ProcessFormTradeMask(Player* this) {
    u8 mask = MergeShip_SohFd_FormTradeMask();
    s32 maskItemAction;
    s32 onButton;
    s32 i;

    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm) || (mask == PLAYER_MASK_NONE) ||
        !GameInteractor_Should(VB_PLAYER_UNEQUIP_MASK_WITHOUT_BUTTON, true, this)) {
        return;
    }
    maskItemAction = mask - 1 + PLAYER_IA_MASK_KEATON;
    onButton = Player_ItemIsItemAction(C_BTN_ITEM(0), maskItemAction) ||
               Player_ItemIsItemAction(C_BTN_ITEM(1), maskItemAction) ||
               Player_ItemIsItemAction(C_BTN_ITEM(2), maskItemAction);
    if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
        for (i = 0; i < 4; i++) {
            onButton |= Player_ItemIsItemAction(DPAD_ITEM(i), maskItemAction);
        }
    }
    if (!onButton) {
        MergeShip_SohFd_SetFormTradeMask(PLAYER_MASK_NONE);
        gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
    }
}

// MergeShip_UseTransformationMask: the Hyrule mask Link wears stays on through the transformation, over the form.
void MergeShip_SohFd_CarryTradeMask(Player* this) {
    if (MergeShip_SohFd_FormsWearTradeMasks() && !MERGESHIP_FORM_IS_MM(this->mergeShipForm) &&
        (this->currentMask != PLAYER_MASK_NONE) && (this->currentMask < PLAYER_MASK_MAX)) {
        MergeShip_SohFd_SetFormTradeMask(this->currentMask);
    }
}

// A form change (MergeShip_ReinitAsForm, or an instant revert): a new form keeps the mask worn over the old one; back
// as Link, he wears it himself. maskMemory follows it, for "Persistent Masks".
void MergeShip_SohFd_AfterFormChange(Player* this, u8 form) {
    u8 mask = MergeShip_SohFd_FormTradeMask();

    if (MERGESHIP_FORM_IS_MM(form)) {
        gSaveContext.ship.maskMemory = mask;
        return;
    }
    if (mask != PLAYER_MASK_NONE) {
        this->currentMask = mask;
        gSaveContext.ship.maskMemory = mask;
    }
    MergeShip_SohFd_SetFormTradeMask(PLAYER_MASK_NONE);
}

// Navi's deep-water offer, answered "Yes" (z_en_elf.c): turn back once the text box has closed.
void MergeShip_SohFd_RequestRevertToHuman(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((player != NULL) && MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        sSohFdRevertPending = true;
    }
}

// A boss's blue warp (z_door_warp1.c), stepped into by a Fierce Deity who may not leave the lair as one: turn back
// into Link first (soh_fd reverted on contact; the warp then takes Link, still standing in it). Returns true while
// the warp has to wait.
s32 MergeShip_SohFd_BlueWarpRevert(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((player == NULL) || (player->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) || MergeShip_SohFd_FdAnywhere()) {
        return false;
    }
    if (!player->mergeShipTransforming) {
        MergeShip_SohFd_RevertWithFlash(play, player);
    }
    return true;
}

// ---- scene-start safeguards (Deku, Goron) ------------------------------------------------------------------------

// The water at `pos` is too deep for the form to stand in: its floor is further below the surface than the form's
// deep-water depth (ageProperties->unk_2C, the depth at which it stops wading).
static s32 MergeShip_SohFd_InDeepWater(PlayState* play, Player* this, Vec3f* pos) {
    f32 ySurface;
    WaterBox* waterBox;
    CollisionPoly* floorPoly;
    s32 floorBgId;
    Vec3f checkPos;
    f32 floorY;

    if (!WaterBox_GetSurface1(play, &play->colCtx, pos->x, pos->z, &ySurface, &waterBox) ||
        (ySurface < pos->y - 10.0f)) {
        return false;
    }
    checkPos = *pos;
    checkPos.y = ySurface + 10.0f;
    floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &floorPoly, &floorBgId, &checkPos);
    return (floorY <= BGCHECK_Y_MIN) || ((ySurface - floorY) > this->ageProperties->unk_2C);
}

// A Deku on a floor that sets it alight (the lava floors: Ocarina of Time floor types 2 and 3, func_80838144).
static s32 MergeShip_SohFd_OnFireFloor(PlayState* play, Player* this, Vec3f* pos) {
    CollisionPoly* floorPoly;
    s32 floorBgId;
    Vec3f checkPos = *pos;

    checkPos.y += 20.0f;
    if (BgCheck_EntityRaycastFloor3(&play->colCtx, &floorPoly, &floorBgId, &checkPos) <= BGCHECK_Y_MIN) {
        return false;
    }
    return func_80838144(SurfaceType_GetFloorType(&play->colCtx, floorPoly, floorBgId)) >= 0;
}

// The Aegiker hack's Goron comes back as Link when it drowns; here a Deku or Goron that would start stuck -- in water
// it can only sink in, or (the Deku) on a burning floor -- is Link from the start. Most notably Zora's Fountain after
// Barinade's blue warp: once the sapphire scene ends, Link starts 30 units under water that is ~410 deep.
static s32 MergeShip_SohFd_FormStartsStuck(PlayState* play, Player* this) {
    if (!MergeShip_SohFd_SceneStartSafeguards() || (this->actor.category != ACTORCAT_PLAYER)) {
        return false;
    }
    if (this->mergeShipForm == PLAYER_FORM_DEKU) {
        return MergeShip_SohFd_InDeepWater(play, this, &this->actor.world.pos) ||
               MergeShip_SohFd_OnFireFloor(play, this, &this->actor.world.pos);
    }
    if (this->mergeShipForm == PLAYER_FORM_GORON) {
        return MergeShip_SohFd_InDeepWater(play, this, &this->actor.world.pos);
    }
    return false;
}

// Player_Init, once the spawn (or respawn) position is known.
void MergeShip_SohFd_OnPlayerInit(Player* this, PlayState* play) {
    if (this->actor.category != ACTORCAT_PLAYER) {
        return;
    }
    // A Hyrule mask worn over a form comes off at a scene change, as Link's own does; "Persistent Masks" keeps it, as it
    // keeps Link's (maskMemory).
    if (MERGESHIP_FORM_IS_MM(this->mergeShipForm) && CVarGetInteger(CVAR_ENHANCEMENT("PersistentMasks"), 0) &&
        (gSaveContext.ship.maskMemory != PLAYER_MASK_NONE) && (gSaveContext.ship.maskMemory < PLAYER_MASK_MAX)) {
        MergeShip_SohFd_SetFormTradeMask(gSaveContext.ship.maskMemory);
    } else {
        MergeShip_SohFd_SetFormTradeMask(PLAYER_MASK_NONE);
    }

    if (MergeShip_SohFd_FormStartsStuck(play, this)) {
        MergeShip_SohFd_RevertInstantly(play, this);
    }
}

// ---- Fierce Deity: Navi's deep-water offer (soh_fd's stuck safeguard) ----------------------------------------------

static u8 sSohFdWaterWarned = false;
// soh_fd drove Player::naviWarning, which this Player does not have: the pending warning is kept here.
static s16 sSohFdNaviWarning = 0;

// soh_fd (from the Aegiker hack's LOCATION_HAS_WATER_PROBLEMS): where a swimming Fierce Deity can be stuck.
static s32 MergeShip_SohFd_FdWaterProblemHere(PlayState* play, Player* this) {
    return ((play->sceneNum == SCENE_ZORAS_RIVER) && (play->roomCtx.curRoom.num == 1)) ||
           (play->sceneNum == SCENE_LOST_WOODS) ||
           ((play->sceneNum == SCENE_WATER_TEMPLE) && (play->roomCtx.curRoom.num == 0) &&
            (this->actor.world.pos.x < 536.0f) && (this->actor.world.pos.z < 436.0f)) ||
           (gSaveContext.entranceIndex == ENTR_GROTTOS_11) ||
           // MergeShip2 (BUG-0135): the well's water (Kakariko's only deep water) -- its rim is out of his reach
           (play->sceneNum == SCENE_KAKARIKO_VILLAGE);
}

// ---- once a frame --------------------------------------------------------------------------------------------------

static s32 sSohFdCutscenePlaying = false;

// Player_UpdateCommon, first thing (before the action function reads naviTextId).
void MergeShip_SohFd_UpdateCommonEarly(Player* this, PlayState* play) {
    s32 cutscenePlaying;

    if (this->actor.category != ACTORCAT_PLAYER) {
        return;
    }

    MergeShip_SohFd_UpdateAdultVoice(); // Majora's Mask's own voice drums back once Adult Link's voice has played

    // Navi's "Yes": turn back as soon as nothing is being said.
    if (sSohFdRevertPending && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
        sSohFdRevertPending = false;
        MergeShip_SohFd_RevertWithFlash(play, this);
    }

    // Control handed back by a cutscene to a Deku or Goron that would be stuck where it stands.
    cutscenePlaying = (play->csCtx.state != CS_STATE_IDLE) || (this->csAction != 0);
    if (sSohFdCutscenePlaying && !cutscenePlaying && !this->mergeShipTransforming &&
        MergeShip_SohFd_FormStartsStuck(play, this)) {
        MergeShip_SohFd_RevertInstantly(play, this);
    }
    sSohFdCutscenePlaying = cutscenePlaying;

    // soh_fd's persistent Navi warning: re-armed into naviTextId every frame until Navi is speaking (the action
    // function reads naviTextId; Player_UpdateCommon clears it after), so a warning raised mid-swim is not lost.
    if (sSohFdNaviWarning != 0) {
        this->naviTextId = -sSohFdNaviWarning;
        if (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
            sSohFdNaviWarning = 0;
        }
    } else if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        if ((this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) && MergeShip_SohFd_StuckSafeguards() &&
            !sSohFdWaterWarned && MergeShip_SohFd_FdWaterProblemHere(play, this)) {
            sSohFdNaviWarning = MERGESHIP_TEXT_FORM_WATER_WARNING;
            sSohFdWaterWarned = true;
        }
    } else {
        sSohFdWaterWarned = false; // out of the water: the same spot asks again
    }
}

// ---- Fierce Deity: doors and climbing on adult proportions ---------------------------------------------------------

// soh_fd's door fix: the door, ladder and ledge-climb animations are authored for a 0.01-scale adult, and the Fierce
// Deity's unk_08 (1.5) scales their baked root motion on top of his 0.015 draw scale, dragging him into the floor at
// a door and past the top of a ladder. For those actions he runs on Adult Link's properties (unk_08 1.0).
static s32 MergeShip_SohFd_FdOnAdultProportions(Player* this) {
    return (Player_Action_80845EF8 == this->actionFunc) || // door
           (Player_Action_8084BF1C == this->actionFunc) || // ladder / vine climb
           (Player_Action_8084BDFC == this->actionFunc);   // ledge climb up
}

// The door's setup, before its animation's root motion starts.
void MergeShip_SohFd_DoorAgeProperties(Player* this) {
    if (this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) {
        this->ageProperties = &sAgeProperties[PLAYER_FORM_ADULT];
    }
}

// ---- MM roll attack ------------------------------------------------------------------------------------------------

// Set by Player_Action_Roll for the frames MM's roll attacks in, and cleared before every action runs, so the body
// cylinder cannot stay an attack once the roll ends or is interrupted.
static s32 sSohFdRollAttackActive = false;
static s32 sSohFdRollAttackArmed = false;

// Player_UpdateCommon, right before the action function runs.
void MergeShip_SohFd_UpdatePreAction(Player* this, PlayState* play) {
    if (this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) {
        this->ageProperties = MergeShip_SohFd_FdOnAdultProportions(this) ? &sAgeProperties[PLAYER_FORM_ADULT]
                                                                          : &sAgeProperties[PLAYER_FORM_FIERCE_DEITY];
    }
    sSohFdRollAttackActive = false;
}

// Player_Action_Roll. MM Player_Action_26: frames 8-18 of the roll, Player_SetCylinderForAttack(DMG_NORMAL_ROLL, 1, 12).
void MergeShip_SohFd_RollAttackFrame(Player* this) {
    if (MergeShip_SohFd_RollDamageApplies(this) && (this->skelAnime.curFrame >= 8.0f) &&
        (this->skelAnime.curFrame < 18.0f)) {
        sSohFdRollAttackActive = true;
    }
}

// Player_UpdateCommon, where the body cylinder is submitted as an attack (the roll's negative invincibility frames).
// MM's DMG_NORMAL_ROLL is carried as Ocarina of Time's DMG_UNKNOWN_1, the flag MergeShip's Goron roll uses for it:
// Ocarina of Time's damage tables give it 0 almost everywhere, as Majora's Mask's give its roll.
void MergeShip_SohFd_UpdateRollAttack(Player* this) {
    if (sSohFdRollAttackActive) {
        this->cylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
        this->cylinder.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
        this->cylinder.info.toucher.dmgFlags = DMG_UNKNOWN_1;
        this->cylinder.info.toucher.damage = 1;
        this->cylinder.dim.radius = 12;
        sSohFdRollAttackArmed = true;
    } else if (sSohFdRollAttackArmed) {
        this->cylinder.base.atFlags = AT_NONE;
        this->cylinder.info.toucherFlags = TOUCH_NONE;
        this->cylinder.info.toucher.dmgFlags = 0;
        this->cylinder.info.toucher.damage = 0;
        sSohFdRollAttackArmed = false;
    }
}

// ---- Epona and the Master Sword ------------------------------------------------------------------------------------

// Player_ActionHandler_3, A pressed next to a horse: a form cannot mount it. Returns true when refused.
s32 MergeShip_SohFd_HorseRefused(Player* this) {
    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm) || !MergeShip_SohFd_PreventRestrictedActions()) {
        return false;
    }
    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
    this->naviTextId = -(((this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) ||
                          (this->mergeShipForm == PLAYER_FORM_GORON))
                             ? MERGESHIP_TEXT_FORM_TOO_BIG
                             : MERGESHIP_TEXT_FORM_CANT_DO_THAT);
    return true;
}

// Player_ActionHandler_2, the Master Sword's pedestal (drawing or placing it): refused to a form.
s32 MergeShip_SohFd_PedestalRefused(Player* this, Actor* interactedActor) {
    if ((interactedActor == NULL) || (interactedActor->id != ACTOR_BG_TOKI_SWD) ||
        !MERGESHIP_FORM_IS_MM(this->mergeShipForm) || !MergeShip_SohFd_PreventRestrictedActions()) {
        return false;
    }
    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
    this->naviTextId = -MERGESHIP_TEXT_FORM_CANT_DO_THAT;
    return true;
}

// ---- the Fierce Deity's items and sword ----------------------------------------------------------------------------

// Player_UseItem's Fierce Deity gate: besides its sword, its mask and bottles (Majora's Mask's rule), what the
// Fierce Deity options allow (MergeShip_SohFd_FormItemOverride).
s32 MergeShip_SohFd_FdMayUseItem(s32 item) {
    return (item >= 0) && (item < ITEM_NONE_FE) &&
           (MergeShip_SohFd_FormItemOverride(PLAYER_FORM_FIERCE_DEITY, (u8)item) == 1);
}
