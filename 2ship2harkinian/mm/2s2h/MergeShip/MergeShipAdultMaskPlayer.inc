// MergeShip2 -- the Adult Mask's transformation in Majora's Mask's player (MergeShipAdultMask.h). Included at the end of
// z_player.c, whose statics it uses; z_player.c calls in with one-line hooks.
//
// The mask is used as the Fierce Deity's Mask (Player_ItemToItemAction): Majora's Mask's own checks decide whether it
// can be put on now (the ceiling, water, a ledge...), and Player_ActionHandler_13 comes here instead of its mask branch.
// What differs from the Fierce Deity's is only where the transformation leads: the human form (MM func_808388B8 with
// PLAYER_FORM_HUMAN, as taking a transformation mask off does) of the other age -- the age MergeShipAdultLink.h
// already keeps, flipped once the cutscene's flash hides him (MergeShip_AdultMask_Sync, from Player_Init).

static u8 sMsAdultMaskUsing = false; // the item being used is the Adult Mask (Player_UseItem -> ActionHandler_13)

s32 MergeShip_AdultMask_UseItem(PlayState* play, Player* this, u8 item) {
    sMsAdultMaskUsing = (item == ITEM_MS_MASK_ADULT);
    if (!sMsAdultMaskUsing) {
        return false;
    }
    if (!MergeShip_AdultMask_Owned()) {
        sMsAdultMaskUsing = false;
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return true;
    }
    return false;
}

s32 MergeShip_AdultMask_ActionHandler13(Player* this, PlayState* play) {
    if (!sMsAdultMaskUsing || (this->itemAction != PLAYER_IA_MASK_FIERCE_DEITY)) {
        return false;
    }
    sMsAdultMaskUsing = false;

    // MM Player_ActionHandler_13's mask branch, putting a transformation mask on: the mask goes on (drawn as the Adult
    // Mask, MergeShip_AdultMask_Drawing), and the transformation starts, to the human of the other age -- from young
    // Link the adult, from the adult young Link, and from a form (which ends with it) the other age of the human.
    // The adult puts it on the same way, as the Child Mask (the user, 2026-10-01: "Every time Adult puts it on, give
    // him a transformation cutscene"): MM's human cutscene, the mask on his face, his scream.
    this->prevMask = this->currentMask;
    this->currentMask = PLAYER_MASK_FIERCE_DEITY;
    MergeShip_AdultMask_BeginTransform(MergeShip_AdultLink_GetSaveAge() != MERGESHIP_HUMAN_AGE_ADULT);
    func_808388B8(play, this, PLAYER_FORM_HUMAN);
    gSaveContext.save.equippedMask = PLAYER_MASK_NONE; // no mask is worn once he is the adult (or young) human
    this->stateFlags1 |= (PLAYER_STATE1_20000000 | PLAYER_STATE1_10000000);
    func_8082DAD4(this);
    return true;
}

void MergeShip_AdultMask_EndTransform(void);
void MergeShip_AdultMask_SyncIcon(PlayState* play);

void MergeShip_AdultMask_PlayerUpdate(PlayState* play, Player* this) {
    MergeShip_AdultMask_SyncIcon(play);
    if (MergeShip_AdultMask_Drawing(this) && (this->actionFunc != Player_Action_86) &&
        (this->actionFunc != Player_Action_87) && !(this->stateFlags1 & PLAYER_STATE1_2)) {
        MergeShip_AdultMask_EndTransform();
    }
}
