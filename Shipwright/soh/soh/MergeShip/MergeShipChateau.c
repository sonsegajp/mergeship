// MergeShip2 -- Majora's Mask's Chateau Romani in Hyrule: the item, its drink, the cow (MergeShipChateau.h).
// The SoH side (its get-item text, pause name, item table entry, when the drink's effect ends) is in
// MergeShipChateau.cpp.

#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/MergeShip/MergeShipChateau.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "mm/objects/object_gi_bottle_21/object_gi_bottle_21.h"

// z_parameter.c's BUTTON_STATUS_INDEX (local to that file): buttonStatus[4] is the A button, so SoH's D-pad item
// buttons (buttonItems[4..]) sit one further on.
#define MERGESHIP_BUTTON_STATUS_INDEX(button) (((button) >= 4) ? ((button) + 1) : (button))

// MM WEEKEVENTREG_DRANK_CHATEAU_ROMANI. Never saved: MergeShipChateau.cpp clears it when a file is loaded and when
// Link dies (MM clears it with the three-day cycle). In mergeship.exe the shared save carries it (BUG-0153).
static s32 sDrankChateauRomani = false;

s32 MergeShip_Chateau_IsActive(void) {
    return sDrankChateauRomani && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ChateauMagic"), 1);
}

void MergeShip_Chateau_Reset(void) {
    sDrankChateauRomani = false;
}

s32 MergeShip_Chateau_Drunk(void) {
    return sDrankChateauRomani;
}

void MergeShip_Chateau_SetDrunk(s32 drunk) {
    sDrankChateauRomani = drunk ? true : false;
}

// MM Player_Action_67, PLAYER_IA_BOTTLE_CHATEAU: D_8085D790 is 1 | 2 -- health refilled (healthAccumulator 0x140) and
// magic refilled (MM Magic_Add(play, MAGIC_FILL_TO_CAPACITY); Ocarina of Time's blue potion does the same with
// Magic_Fill, which the drink's "wait for the meter" step expects) -- then SET_WEEKEVENTREG(DRANK_CHATEAU_ROMANI).
// MM's jinxTimer = 0 has no Hyrule counterpart.
void MergeShip_Chateau_Drink(PlayState* play) {
    gSaveContext.healthAccumulator = MAX_HEALTH;
    Magic_Fill(play);
    if (CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ChateauMagic"), 1)) {
        sDrankChateauRomani = true;
    }
}

// MM GetItem_Draw(GID_CHATEAU) (z_draw.c sDrawItemTable, OBJECT_GI_BOTTLE_21):
// GetItem_DrawOpa0Xlu1 { gGiChateauRomaniBottleEmptyDL, gGiChateauRomaniBottleDL }, from mm.o2r.
static void MergeShip_Chateau_DrawGi(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MILK);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gGiChateauRomaniBottleEmptyDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGiChateauRomaniBottleDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM sGetItemTable[GI_CHATEAU - 1]: GET_ITEM(ITEM_CHATEAU_2, OBJECT_GI_BOTTLE_21, GID_CHATEAU, 0x91,
// GIFIELD(GIFIELD_NO_COLLECTIBLE, 0), CHEST_ANIM_LONG). The model is drawn from mm.o2r by drawFunc; OBJECT_GI_MILK and
// GID_MILK (Ocarina of Time's milk bottle) are only what is loaded/drawn when mm.o2r is missing.
GetItemEntry MergeShip_Chateau_GetItemEntry(void) {
    GetItemEntry entry = GET_ITEM(ITEM_MM_CHATEAU, OBJECT_GI_MILK, GID_MILK, MERGESHIP_TEXT_CHATEAU_FILLED, 0x80,
                                  CHEST_ANIM_LONG, ITEM_CATEGORY_JUNK, MOD_NONE, MERGESHIP_GI_MM_CHATEAU);

    entry.drawFunc = MergeShip_Chateau_DrawGi;
    return entry;
}

// MM Item_Give, ITEM_CHATEAU_2: the first empty bottle becomes ITEM_CHATEAU, and a C button holding that bottle shows
// it (Ocarina of Time's Item_Give does the same for its bottled items).
u8 MergeShip_Chateau_Give(PlayState* play) {
    u8 result = ITEM_MM_CHATEAU;
    s16 i;
    s32 buttonIndex;

    for (i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
            for (buttonIndex = 1; buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems); buttonIndex++) {
                if ((SLOT_BOTTLE_1 + i) == gSaveContext.equips.cButtonSlots[buttonIndex - 1]) {
                    gSaveContext.equips.buttonItems[buttonIndex] = ITEM_MM_CHATEAU;
                    if (play != NULL) {
                        Interface_LoadItemIcon2(play, buttonIndex);
                    }
                    gSaveContext.buttonStatus[MERGESHIP_BUTTON_STATUS_INDEX(buttonIndex)] = BTN_ENABLED;
                    break;
                }
            }
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = ITEM_MM_CHATEAU;
            result = ITEM_NONE;
            break;
        }
    }

    // As Item_Give's Return_Item: the item-receive hooks run whatever the result.
    GameInteractor_ExecuteOnItemReceiveHooks(MergeShip_Chateau_GetItemEntry());
    return result;
}

u8 MergeShip_Chateau_CheckObtainability(void) {
    s16 i;

    for (i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
            return ITEM_NONE;
        }
    }
    return ITEM_MM_CHATEAU;
}

// The user's rule for Hyrule's cows (MergeShipChateau.h). En_Cow has already checked for an empty bottle
// (EnCow_CheckForEmptyBottle) and let a randomizer cow give its shuffled item first (VB_GIVE_ITEM_FROM_COW).
s32 MergeShip_Chateau_CowOffer(Actor* cow, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.RomaniChateau"), 1) || !MergeShip_IsMajorasMaskMounted() ||
        (player == NULL) || !MERGESHIP_WEARS_MM_MASK(player, MM_MASK_ROMANI)) {
        return false;
    }
    GiveItemEntryFromActor(cow, play, MergeShip_Chateau_GetItemEntry(), 10000.0f, 100.0f);
    return true;
}
