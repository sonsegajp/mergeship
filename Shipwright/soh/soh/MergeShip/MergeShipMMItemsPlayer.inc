// MergeShip2 -- Majora's Mask's bottle contents, trade items and Powder Keg: the parts of MM's player that need
// z_player.c's static helpers. Included at the very end of z_player.c. See MergeShipMMItems.h / .c.

// MM Player_ActionHandler_13 (the item cutscene, unk_AA5 == PLAYER_UNKAA5_5; Ocarina of Time's unk_6AD == 4) for MM's
// bottle contents and trade items:
//   * Spring Water and Hot Spring Water (MM PLAYER_BOTTLE_SPRING_WATER / _HOT_SPRING_WATER, between
//     PLAYER_BOTTLE_EMPTY and PLAYER_BOTTLE_POE) are poured: MM Player_Action_70, the "fish out" animation, MM's
//     Obj_Aqua spawned at frame 76 (Player_Action_8084EFC0 with MergeShip_MMItems_PourActor).
//   * The Zora Egg .. Hylian Loach, the Deku Princess among them (MM PLAYER_IA_BOTTLE_ZORA_EGG ..
//     PLAYER_IA_BOTTLE_HYLIAN_LOACH), and the trade items (past PLAYER_IA_BOTTLE_FAIRY) are shown, unless an actor
//     asked for this very item this frame (MM unk_B2B: En_Dnq, En_Zoraegg, En_Ot -- none exists in Hyrule, see
//     MergeShipMMItems.c). With nobody to take them MM holds the item up (D_8085D1F8[1], link_normal_take_out) and says
//     "This item doesn't seem to work here..." (MM text 0xFE). Ocarina of Time's own "show to nobody" path is the same
//     (D_80854548[2], the same animation, CAM_ITEM_TYPE_4), so it is used with MM's text.
// Returns true when it handled the item action.
s32 MergeShip_MMItems_ActionHandler13(PlayState* play, Player* this) {
    GetItemEntry giEntry;

    if (!MergeShip_MMItems_IsCutsceneItemAction(this->itemAction)) {
        return false;
    }

    if ((this->itemAction == PLAYER_IA_MM_BOTTLE_SPRING_WATER) ||
        (this->itemAction == PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER)) {
        Player_SetupActionPreserveItemAction(play, this, Player_Action_8084EFC0, 0);
        Player_AnimPlayOnceAdjusted(play, this, &gPlayerAnim_link_bottle_fish_out);
        Player_SetTurnAroundCamera(play, CAM_ITEM_TYPE_5);
        this->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
        func_80832224(this);
        return true;
    }

    if ((play->actorCtx.titleCtx.delayTimer == 0) && (play->actorCtx.titleCtx.alpha == 0)) {
        Player_SetupActionPreserveItemAction(play, this, Player_Action_ExchangeItem, 0);

        if (MERGESHIP_IA_IS_MM_TRADE(this->itemAction)) {
            // Ocarina of Time loads a shown trade item's get-item object here (MM does it on frame 2 of the animation);
            // the MM model itself is drawn from mm.o2r.
            giEntry = MergeShip_MMItems_GetItemEntry(ITEM_MM_POWDER_KEG + (this->itemAction - PLAYER_IA_MM_POWDER_KEG));
            func_8083AE40(this, giEntry.objectId);
        }

        this->stateFlags1 |= PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
        this->av1.actionVar1 = 2;
        this->actor.textId = MERGESHIP_TEXT_MM_ITEM_NO_EFFECT;
        Player_SetTurnAroundCamera(play, CAM_ITEM_TYPE_4);
        this->actor.flags |= ACTOR_FLAG_TALK;
        this->exchangeItemId = MERGESHIP_EXCH_ITEM_MM;
        // MergeShip2: MM's human plays MM's take_out, as Ocarina of Time's own show does.
        Player_AnimPlayOnce(play, this, MergeShip_MMChild_Anim(this, D_80854548[this->av1.actionVar1]));
        func_80832224(this);
    }
    return true;
}

// MM Player_Init: `if (gSaveContext.powderKegTimer != 0)` the Goron comes into the new scene still carrying its lit
// keg (the fuse keeps counting from where it was). Only the Goron uses kegs, so for anyone else the keg is gone.
void MergeShip_MMItems_PlayerInit(PlayState* play, Player* this) {
    if (gMergeShipPowderKegTimer == 0) {
        return;
    }
    if (!MergeShip_MMItems_Enabled() || !MergeShip_MMItems_KegCarriesOver() ||
        (this->mergeShipForm != PLAYER_FORM_GORON) || (gMergeShipActorId_MM_En_Bom_Keg < 0)) {
        gMergeShipPowderKegTimer = 0;
        return;
    }

    this->nextModelGroup = Player_ActionToModelGroup(this, PLAYER_IA_MM_POWDER_KEG);
    this->heldItemId = ITEM_MM_POWDER_KEG;
    Player_InitItemAction(play, this, PLAYER_IA_MM_POWDER_KEG);
    func_80835688(this, play); // MM func_808313F0
}
