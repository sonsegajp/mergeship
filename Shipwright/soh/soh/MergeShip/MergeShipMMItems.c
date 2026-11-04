// MergeShip2 -- Majora's Mask's remaining C-button items, bottle contents and trade items in Hyrule (the rules; see
// MergeShipMMItems.h for the map of files).
//
// Where each piece comes from (2ship2harkinian/mm):
//   Item_Give / Item_CheckObtainability ........ z_parameter.c Item_GiveImpl / Item_CheckObtainabilityImpl
//   the bottle timer ........................... z_parameter.c Interface_StartBottleTimer, Interface_UpdateBottleTimers
//   the bottle colours ......................... z_player_lib.c sPlayerBottleColors (in z_player_lib.c's sBottleColors)
//   pouring, showing ........................... z_player.c Player_ActionHandler_13, Player_Action_70,
//                                                Player_Action_ExchangeItem (MergeShipMMItemsPlayer.inc)
//   the Powder Keg ............................. z_player.c Player_InitExplosiveIA, sPlayerExplosiveInfo, Player_Init
//   get-item models ............................ z_draw.c sDrawItemTable (mm.o2r object_gi_*)
//
// Hyrule differences (nothing here is invented; where MM's behaviour needs something Hyrule does not have, the MM
// rule is kept and the missing part is named):
//   * Releasing the Deku Princess, a Zora Egg or the Seahorse: MM releases them only where En_Dnq (the Deku King's
//     chamber), En_Zoraegg (the Marine Research Lab's tank) or En_Ot (Pinnacle Rock) asks for them (MM unk_B2B,
//     reset every frame, func_8012300C). Everywhere else MM shows the bottle ("This item doesn't seem to work
//     here..."). None of those actors exists in Hyrule, so here they are always shown and kept -- which is what MM
//     does anywhere outside those three places. Gold Dust, the Magic Mushroom and the Hylian Loach are shown the
//     same way in MM (they are only ever handed to someone).
//   * Spring Water and Hot Spring Water are poured as in MM (Obj_Aqua, ovl_MM_Obj_Aqua). MM's things that react to
//     it (beans, the frozen Goron, Gibdos) are not in Hyrule.
//   * The trade items are shown as in MM ("This item doesn't seem to work here..."): no Hyrule NPC asks for them.
//   * MM keeps its bottle timer counting during MM's own texts 0x100-0x200 and 0x1BB2-0x1BB6; those ids are other
//     texts in Hyrule, so here it pauses for every text.

#include "global.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipChateau.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipOwnership.h"
#include "soh/MergeShip/MergeShipHost.h"
#include <mergeship/SharedSave.h>
#include "mm/archives/item_name_static/item_name_static.h"
#include "mm/objects/object_gi_bigbomb/object_gi_bigbomb.h"
#include "mm/objects/object_gi_camera/object_gi_camera.h"
#include "mm/objects/object_gi_bottle/object_gi_bottle.h"
#include "mm/objects/object_gi_bottle_15/object_gi_bottle_15.h"
#include "mm/objects/object_gi_bottle_16/object_gi_bottle_16.h"
#include "mm/objects/object_gi_magicmushroom/object_gi_magicmushroom.h"
#include "mm/objects/object_gi_loach/object_gi_loach.h"
#include "mm/objects/object_gi_reserve00/object_gi_reserve00.h"
#include "mm/objects/object_gi_reserve01/object_gi_reserve01.h"
#include "mm/objects/object_gi_reserve_b_00/object_gi_reserve_b_00.h"
#include "mm/objects/object_gi_reserve_b_01/object_gi_reserve_b_01.h"
#include "mm/objects/object_gi_reserve_c_00/object_gi_reserve_c_00.h"
#include "mm/objects/object_gi_reserve_c_01/object_gi_reserve_c_01.h"
#include "soh/MergeShip/MergeShipAnimatedMat.h"
#include "soh/MergeShip/MergeShipPicto.h"

// The MM actors this module spawns (registered at run time: ovl_MM_En_Bom_Keg, ovl_MM_Obj_Aqua).
extern s16 gMergeShipActorId_MM_En_Bom_Keg;
extern s16 gMergeShipActorId_MM_Obj_Aqua;
// ovl_MM_Obj_Aqua's params (z_mm_obj_aqua.h MM_AQUA_PARAMS / MM_AQUA_TYPE_*).
#define MERGESHIP_AQUA_PARAMS_COLD 0
#define MERGESHIP_AQUA_PARAMS_HOT 1

// z_parameter.c's BUTTON_STATUS_INDEX (local to that file): buttonStatus[4] is the A button.
#define MS_BUTTON_STATUS_INDEX(button) (((button) >= 4) ? ((button) + 1) : (button))
#define MS_NUM_BUTTON_ITEMS 8 // ARRAY_COUNT(gSaveContext.equips.buttonItems): B, three C, four D-pad

MERGESHIP_STATIC_ASSERT((PLAYER_IA_MM_PENDANT_OF_MEMORIES - PLAYER_IA_MM_POWDER_KEG) ==
                            (ITEM_MM_PENDANT_OF_MEMORIES - ITEM_MM_POWDER_KEG),
                        "the MM items' item actions are in their item ids' order");
MERGESHIP_STATIC_ASSERT((PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER - PLAYER_IA_MM_BOTTLE_DEKU_PRINCESS) ==
                            (PLAYER_BOTTLE_MM_LAST - PLAYER_BOTTLE_MM_FIRST),
                        "one Player_ActionToBottle index per MM bottle content");

MergeShipMMItemsSave gMergeShipMMItemsSave;
u16 gMergeShipPowderKegTimer = 0;

void MergeShip_MMItems_InitSave(void) {
    gMergeShipMMItemsSave.powderKeg = ITEM_NONE;
    gMergeShipMMItemsSave.powderKegAmmo = 0;
    gMergeShipMMItemsSave.pictographBox = ITEM_NONE;
    gMergeShipMMItemsSave.tradeDeed = ITEM_NONE;
    gMergeShipMMItemsSave.tradeKeyMama = ITEM_NONE;
    gMergeShipMMItemsSave.tradeCouple = ITEM_NONE;
    gMergeShipMMItemsSave.tradeOwned = 0;
    // MM Sram: the keg timer lives outside the save, and a loaded file never carries a lit keg.
    gMergeShipPowderKegTimer = 0;
}

// ---- options --------------------------------------------------------------------------------------------------

s32 MergeShip_MMItems_Enabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMItems"), 1) && MergeShip_IsMajorasMaskMounted();
}

s32 MergeShip_MMItems_HotWaterCools(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMItems.HotWaterCools"), 1);
}

s32 MergeShip_MMItems_KegCarriesOver(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMItems.KegCarriesOver"), 1);
}

// ---- ownership ------------------------------------------------------------------------------------------------

s32 MergeShip_MMItems_Handles(u8 item) {
    return (item >= ITEM_MM_POWDER_KEG) && (item <= ITEM_MM_PENDANT_OF_MEMORIES);
}

// Termina owns MM's own version of an item both games have (the shared save's MS_ITEM_MM_* bit: MM's magic arrows and
// Lens, which Hyrule's inventory cannot hold).
static s32 MergeShip_MMItems_TerminaOwns(s32 msItemBit) {
    const MsSharedSave* shared = (const MsSharedSave*)MergeShipHost_SharedSave();
    return (shared != NULL) && ((shared->items & (1u << msItemBit)) != 0);
}

// MM's magic arrows and Lens: on this page when Termina owns MM's version and Hyrule's own slot does not hold it.
static u8 MergeShip_MMItems_MMVersionItem(u8 ootItem, s32 msItemBit) {
    // Only MM's own (the user, 2026-09-24: Ocarina of Time's items are on its own page, never on MM's).
    return MergeShip_MMItems_TerminaOwns(msItemBit) ? ootItem : ITEM_NONE;
}

s32 MergeShip_MMItems_IsPageItem(u8 item) {
    return (item == ITEM_MM_POWDER_KEG) || (item == ITEM_MM_PICTOGRAPH_BOX) || MERGESHIP_ITEM_IS_MM_TRADE(item) ||
           (item == ITEM_MM_BOW_HERO) || (item == ITEM_MM_HOOKSHOT) || (item == ITEM_MM_SWORD_GREAT_FAIRY) ||
           // MM's magic arrows, Lens and ocarina held only as Termina's (no Hyrule slot): equipped without one
           (((item == ITEM_ARROW_FIRE) || (item == ITEM_ARROW_ICE) || (item == ITEM_ARROW_LIGHT) ||
             (item == ITEM_LENS)) &&
            (INV_CONTENT(item) != item)) ||
           // The ocarina by the same rule: Hyrule's slot does not hold this item. Hyrule's Fairy Ocarina is another
           // item (ITEM_OCARINA_FAIRY), so with it in the slot MM's ocarina still lands with no slot -- through
           // MergeShip_Kaleido_FinishMaskEquip, not Ocarina of Time's slot swap, which would take every slot-less
           // button (masks, this page's items) for the same item -- and on a button ITEM_OCARINA_TIME is MM's.
           ((item == ITEM_OCARINA_TIME) && (INV_CONTENT(ITEM_OCARINA_FAIRY) != ITEM_OCARINA_TIME));
}

s16 MergeShip_MMItems_PageSlotOoTSlot(s16 slot) {
    const u8 item = MergeShip_MMItems_PageSlotItem(slot);

    if (item == ITEM_NONE) {
        return SLOT_NONE;
    }
    // Majora's Mask's own bottles and bombs .. beans (MergeShipOwnership.h): their button slots.
    if (MergeShip_Own_MMBottlesHere() && (slot >= MERGESHIP_MM_SLOT_BOTTLE_1) &&
        (slot < MERGESHIP_MM_SLOT_BOTTLE_1 + 6)) {
        return MS_MM_BOTTLE_BUTTON_SLOT(slot - MERGESHIP_MM_SLOT_BOTTLE_1);
    }
    if (MergeShip_Own_MMBottlesHere() && (slot >= MERGESHIP_MM_SLOT_BOMB) && (slot <= MERGESHIP_MM_SLOT_BOMB + 4)) {
        return MS_MM_AMMO_BUTTON_SLOT(slot - MERGESHIP_MM_SLOT_BOMB);
    }
    if ((slot >= MERGESHIP_MM_SLOT_BOTTLE_1) && (slot < MERGESHIP_MM_SLOT_BOTTLE_1 + 4)) {
        return SLOT_BOTTLE_1 + (slot - MERGESHIP_MM_SLOT_BOTTLE_1);
    }
    if (MergeShip_MMItems_IsPageItem(item)) {
        return SLOT_NONE;
    }
    return (INV_CONTENT(item) == item) ? SLOT(item) : SLOT_NONE;
}

// MM's trade slot of a trade item (MM gItemSlots: SLOT_TRADE_DEED, SLOT_TRADE_KEY_MAMA, SLOT_TRADE_COUPLE).
static u8* MergeShip_MMItems_TradeSlot(u8 item) {
    switch (item) {
        case ITEM_MM_MOONS_TEAR:
        case ITEM_MM_DEED_LAND:
        case ITEM_MM_DEED_SWAMP:
        case ITEM_MM_DEED_MOUNTAIN:
        case ITEM_MM_DEED_OCEAN:
            return &gMergeShipMMItemsSave.tradeDeed;
        case ITEM_MM_ROOM_KEY:
        case ITEM_MM_LETTER_TO_MAMA:
            return &gMergeShipMMItemsSave.tradeKeyMama;
        case ITEM_MM_LETTER_TO_KAFEI:
        case ITEM_MM_PENDANT_OF_MEMORIES:
            return &gMergeShipMMItemsSave.tradeCouple;
        default:
            return NULL;
    }
}

u8 MergeShip_MMItems_PageSlotItem(s16 slot) {
    switch (slot) {
        case MERGESHIP_MM_SLOT_POWDER_KEG:
            return gMergeShipMMItemsSave.powderKeg;
        case MERGESHIP_MM_SLOT_PICTOGRAPH_BOX:
            return gMergeShipMMItemsSave.pictographBox;
        case MERGESHIP_MM_SLOT_TRADE_DEED:
            return gMergeShipMMItemsSave.tradeDeed;
        case MERGESHIP_MM_SLOT_TRADE_KEY_MAMA:
            return gMergeShipMMItemsSave.tradeKeyMama;
        case MERGESHIP_MM_SLOT_TRADE_COUPLE:
            return gMergeShipMMItemsSave.tradeCouple;
        // MM's gear, owned as MM's (MergeShipMMEquipment.h): also on MM's own page, at MM's slot, the user's rule
        // (every MM item is on the Termina items face). They still live in Ocarina of Time's matching slot too.
        case MERGESHIP_MM_SLOT_BOW:
            return MergeShip_MMEquip_Owns(MMEQUIP_BOW) ? ITEM_MM_BOW_HERO : ITEM_NONE;
        // Every other MM item (the user's rule: all of MM's items on MM's page). Those Hyrule holds in Ocarina of
        // Time's grid (one ocarina, the ammo items, the bottles) are shown from there and equipped through their slot
        // (MergeShip_MMItems_PageSlotOoTSlot); MM's own magic arrows and Lens from the shared save.
        // MM's ocarina, Termina's (the user, 2026-09-24: "When getting the Fairy Ocarina, don't put it in MM's
        // inventory"): never Hyrule's Fairy Ocarina / Ocarina of Time, which are on Ocarina of Time's page.
        case MERGESHIP_MM_SLOT_OCARINA:
            return MergeShip_MMItems_MMVersionItem(ITEM_OCARINA_TIME, MS_ITEM_MM_OCARINA);
        case MERGESHIP_MM_SLOT_ARROW_FIRE:
            return MergeShip_MMItems_MMVersionItem(ITEM_ARROW_FIRE, MS_ITEM_MM_ARROW_FIRE);
        case MERGESHIP_MM_SLOT_ARROW_FIRE + 1:
            return MergeShip_MMItems_MMVersionItem(ITEM_ARROW_ICE, MS_ITEM_MM_ARROW_ICE);
        case MERGESHIP_MM_SLOT_ARROW_FIRE + 2:
            return MergeShip_MMItems_MMVersionItem(ITEM_ARROW_LIGHT, MS_ITEM_MM_ARROW_LIGHT);
        // Majora's Mask's own bombs, bombchus, sticks, nuts and beans (MergeShipOwnership.h): held in Ocarina of Time's
        // slot, with its ammo, and shown here only; Ocarina of Time's own are on its page.
        case MERGESHIP_MM_SLOT_BOMB:
            return MergeShip_Own_MMOwns(ITEM_BOMB) ? ITEM_BOMB : ITEM_NONE;
        case MERGESHIP_MM_SLOT_BOMB + 1:
            return MergeShip_Own_MMOwns(ITEM_BOMBCHU) ? ITEM_BOMBCHU : ITEM_NONE;
        case MERGESHIP_MM_SLOT_BOMB + 2:
            return MergeShip_Own_MMOwns(ITEM_STICK) ? ITEM_STICK : ITEM_NONE;
        case MERGESHIP_MM_SLOT_BOMB + 3:
            return MergeShip_Own_MMOwns(ITEM_NUT) ? ITEM_NUT : ITEM_NONE;
        case MERGESHIP_MM_SLOT_BOMB + 4:
            return MergeShip_Own_MMOwns(ITEM_BEAN) ? ITEM_BEAN : ITEM_NONE;
        case MERGESHIP_MM_SLOT_LENS:
            return MergeShip_MMItems_MMVersionItem(ITEM_LENS, MS_ITEM_MM_LENS_OF_TRUTH);
        // Majora's Mask's six bottles (the shared save's, MergeShipOwnership.h); soh.exe / combined: a bottle Hyrule
        // holds with one of Majora's Mask's own contents (Ocarina of Time's page shows the rest).
        case MERGESHIP_MM_SLOT_BOTTLE_1 + 4:
        case MERGESHIP_MM_SLOT_BOTTLE_1 + 5:
            return MergeShip_Own_MMBottlesHere() ? MergeShip_Own_MMBottle(slot - MERGESHIP_MM_SLOT_BOTTLE_1)
                                                 : ITEM_NONE;
        case MERGESHIP_MM_SLOT_BOTTLE_1:
        case MERGESHIP_MM_SLOT_BOTTLE_1 + 1:
        case MERGESHIP_MM_SLOT_BOTTLE_1 + 2:
        case MERGESHIP_MM_SLOT_BOTTLE_1 + 3: {
            if (MergeShip_Own_MMBottlesHere()) {
                return MergeShip_Own_MMBottle(slot - MERGESHIP_MM_SLOT_BOTTLE_1);
            }
            const u8 content = gSaveContext.inventory.items[SLOT_BOTTLE_1 + (slot - MERGESHIP_MM_SLOT_BOTTLE_1)];
            return ((content == ITEM_MM_CHATEAU) ||
                    ((content >= ITEM_MM_DEKU_PRINCESS) && (content <= ITEM_MM_HOT_SPRING_WATER)) ||
                    MergeShip_Own_HyruleBottleIsMM(slot - MERGESHIP_MM_SLOT_BOTTLE_1))
                       ? content
                       : ITEM_NONE;
        }
        case MERGESHIP_MM_SLOT_HOOKSHOT:
            return MergeShip_MMEquip_Owns(MMEQUIP_HOOKSHOT) ? ITEM_MM_HOOKSHOT : ITEM_NONE;
        case MERGESHIP_MM_SLOT_SWORD_GREAT_FAIRY:
            return MergeShip_MMEquip_Owns(MMEQUIP_GREAT_FAIRY) ? ITEM_MM_SWORD_GREAT_FAIRY : ITEM_NONE;
        // Hooks: MM items other ports add without an Ocarina of Time slot go here, at MM's slot --
        //   MERGESHIP_MM_SLOT_HOOKSHOT (ITEM_MM_HOOKSHOT), MERGESHIP_MM_SLOT_SWORD_GREAT_FAIRY
        //   (ITEM_MM_SWORD_GREAT_FAIRY), and the Hero's Bow (MM SLOT_BOW, 1) if it gets a slot of its own.
        // Return the owned item (or ITEM_NONE) and make MergeShip_MMItems_IsPageItem accept it; the Termina items face
        // then draws it, names it (MergeShip_MMItems_NameTexture) and equips it.
        default:
            return ITEM_NONE;
    }
}

s32 MergeShip_MMItems_Owns(u8 item) {
    u8* tradeSlot;

    if (item == ITEM_MM_POWDER_KEG) {
        return gMergeShipMMItemsSave.powderKeg == ITEM_MM_POWDER_KEG;
    }
    if (item == ITEM_MM_PICTOGRAPH_BOX) {
        return gMergeShipMMItemsSave.pictographBox == ITEM_MM_PICTOGRAPH_BOX;
    }
    if (item == ITEM_MM_BOW_HERO) {
        return MergeShip_MMEquip_Owns(MMEQUIP_BOW);
    }
    if (item == ITEM_MM_HOOKSHOT) {
        return MergeShip_MMEquip_Owns(MMEQUIP_HOOKSHOT);
    }
    if (item == ITEM_MM_SWORD_GREAT_FAIRY) {
        return MergeShip_MMEquip_Owns(MMEQUIP_GREAT_FAIRY);
    }
    // MM's ocarina, magic arrows and Lens, held only as Termina's (MergeShip_MMItems_IsPageItem: no Hyrule slot of
    // their own): owned when Termina owns them. Not listed here, the frame's button check (ValidateButtons) took them
    // straight off any button they were equipped to (the user, 2026-09-30: "I can't use MM ocarina in OOT").
    switch (item) {
        case ITEM_OCARINA_TIME:
            return MergeShip_MMItems_TerminaOwns(MS_ITEM_MM_OCARINA);
        case ITEM_ARROW_FIRE:
            return MergeShip_MMItems_TerminaOwns(MS_ITEM_MM_ARROW_FIRE);
        case ITEM_ARROW_ICE:
            return MergeShip_MMItems_TerminaOwns(MS_ITEM_MM_ARROW_ICE);
        case ITEM_ARROW_LIGHT:
            return MergeShip_MMItems_TerminaOwns(MS_ITEM_MM_ARROW_LIGHT);
        case ITEM_LENS:
            return MergeShip_MMItems_TerminaOwns(MS_ITEM_MM_LENS_OF_TRUTH);
        default:
            break;
    }
    tradeSlot = MergeShip_MMItems_TradeSlot(item);
    return (tradeSlot != NULL) &&
           ((*tradeSlot == item) || (gMergeShipMMItemsSave.tradeOwned & MERGESHIP_MM_TRADE_BIT(item)));
}

u8 MergeShip_MMItems_GetKegAmmo(void) {
    return gMergeShipMMItemsSave.powderKegAmmo;
}

// MM Inventory_ChangeAmmo(ITEM_POWDER_KEG, change): AMMO += change, never below 0 (MM's capacity for the keg is 1).
void MergeShip_MMItems_ChangeKegAmmo(s16 change) {
    s16 ammo = gMergeShipMMItemsSave.powderKegAmmo + change;

    if (ammo < 0) {
        ammo = 0;
    } else if (ammo > MERGESHIP_POWDER_KEG_MAX_AMMO) {
        ammo = MERGESHIP_POWDER_KEG_MAX_AMMO;
    }
    gMergeShipMMItemsSave.powderKegAmmo = (u8)ammo;
}

// ---- get-item models (MM z_draw.c sDrawItemTable, from mm.o2r) -------------------------------------------------

// MM GetItem_DrawOpa0Xlu1.
static void MergeShip_MMItems_DrawOpa0Xlu1(PlayState* play, const char* opa, const char* xlu) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)opa);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)xlu);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM GetItem_DrawOpa01.
static void MergeShip_MMItems_DrawOpa01(PlayState* play, const char* first, const char* second) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)first);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)second);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM GetItem_DrawMoonsTear: the stone's animated texture, then its glow turned to face the camera.
static void MergeShip_MMItems_DrawMoonsTear(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    MergeShip_AnimatedMat_Draw(play, (void*)mm_gGiMoonsTearTexAnim);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_gGiMoonsTearItemDL);

    Matrix_ReplaceRotation(&play->billboardMtxF);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gGiMoonsTearGlowDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void MergeShip_MMItems_DrawGi(PlayState* play, GetItemEntry* getItemEntry) {
    u8 item = (u8)getItemEntry->itemId;

    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, (s16)getItemEntry->gid);
        return;
    }

    switch (item) {
        case ITEM_MM_POWDER_KEG: // GID_POWDER_KEG, OBJECT_GI_BIGBOMB
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiPowderKegBarrelDL, mm_gGiPowderKegGoronSkullAndFuseDL);
            break;
        case ITEM_MM_PICTOGRAPH_BOX: // GID_PICTOGRAPH_BOX, OBJECT_GI_CAMERA
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiPictoBoxFrameDL, mm_gGiPictoBoxBodyAndLensDL);
            break;
        case ITEM_MM_ZORA_EGG: // GID_ZORA_EGG, OBJECT_GI_BOTTLE_15
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiZoraEggBottleContentsDL, mm_gGiZoraEggBottleGlassAndCorkDL);
            break;
        case ITEM_MM_GOLD_DUST: // GI_GOLD_DUST: GID_SEAHORSE, OBJECT_GI_BOTTLE_16 (MM's bottle of Gold Dust)
        case ITEM_MM_SEAHORSE:  // GI_SEAHORSE: GID_SEAHORSE, OBJECT_GI_BOTTLE_16
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiSeahorseBottleEmptyDL, mm_gGiSeahorseBottleGlassAndCorkDL);
            break;
        case ITEM_MM_MUSHROOM: // GID_MUSHROOM, OBJECT_GI_MAGICMUSHROOM
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiMagicMushroomEmptyDL, mm_gGiMagicMushroomDL);
            break;
        case ITEM_MM_HYLIAN_LOACH: // GI_HYLIAN_LOACH_2: GID_HYLIAN_LOACH, OBJECT_GI_LOACH
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiHylianLoachEmptyDL, mm_gGiHylianLoachDL);
            break;
        case ITEM_MM_MOONS_TEAR: // GID_MOONS_TEAR, OBJECT_GI_RESERVE00
            MergeShip_MMItems_DrawMoonsTear(play);
            break;
        case ITEM_MM_DEED_LAND: // GID_DEED_*, OBJECT_GI_RESERVE01
            MergeShip_MMItems_DrawOpa01(play, mm_gGiTitleDeedEmptyDL, mm_gGiTitleDeedLandColorDL);
            break;
        case ITEM_MM_DEED_SWAMP:
            MergeShip_MMItems_DrawOpa01(play, mm_gGiTitleDeedEmptyDL, mm_gGiTitleDeedSwampColorDL);
            break;
        case ITEM_MM_DEED_MOUNTAIN:
            MergeShip_MMItems_DrawOpa01(play, mm_gGiTitleDeedEmptyDL, mm_gGiTitleDeedMountainColorDL);
            break;
        case ITEM_MM_DEED_OCEAN:
            MergeShip_MMItems_DrawOpa01(play, mm_gGiTitleDeedEmptyDL, mm_gGiTitleDeedOceanColorDL);
            break;
        case ITEM_MM_ROOM_KEY: // GID_ROOM_KEY, OBJECT_GI_RESERVE_B_00
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiRoomKeyEmptyDL, mm_gGiRoomKeyDL);
            break;
        case ITEM_MM_LETTER_TO_MAMA: // GID_LETTER_MAMA, OBJECT_GI_RESERVE_B_01
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiLetterToMamaEnvelopeLetterDL,
                                           mm_gGiLetterToMamaInscriptionsDL);
            break;
        case ITEM_MM_LETTER_TO_KAFEI: // GID_LETTER_TO_KAFEI, OBJECT_GI_RESERVE_C_00
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiLetterToKafeiEnvelopeLetterDL,
                                           mm_gGiLetterToKafeiInscriptionsDL);
            break;
        case ITEM_MM_PENDANT_OF_MEMORIES: // GID_PENDANT_OF_MEMORIES, OBJECT_GI_RESERVE_C_01
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiPendantOfMemoriesEmptyDL, mm_gGiPendantOfMemoriesDL);
            break;
        default:
            // The Deku Princess (MM GI_DEKU_PRINCESS draws GID_BOTTLE), Spring Water and Hot Spring Water (MM scoops
            // them and has no model of its own; they are drawn as the bottle they go into): MM's empty bottle.
            MergeShip_MMItems_DrawOpa0Xlu1(play, mm_gGiEmptyBottleCorkDL, mm_gGiEmptyBottleGlassDL);
            break;
    }
}

s16 MergeShip_MMItems_GetItemId(u8 item) {
    if ((item >= ITEM_MM_POWDER_KEG) && (item <= ITEM_MM_SEAHORSE)) {
        return MERGESHIP_GI_MM_ITEMS_BASE + (item - ITEM_MM_POWDER_KEG); // 0x190..0x196
    }
    if ((item >= ITEM_MM_MOONS_TEAR) && (item <= ITEM_MM_PENDANT_OF_MEMORIES)) {
        return MERGESHIP_GI_MM_ITEMS_BASE + 7 + (item - ITEM_MM_MOONS_TEAR); // 0x197..0x19F
    }
    return GI_NONE; // Hylian Loach, Spring Water, Hot Spring Water
}
MERGESHIP_STATIC_ASSERT((MERGESHIP_GI_MM_ITEMS_BASE + 7 + (ITEM_MM_PENDANT_OF_MEMORIES - ITEM_MM_MOONS_TEAR)) ==
                            MERGESHIP_GI_MM_ITEMS_LAST,
                        "the MM items' get-item ids fill 0x0190-0x019F exactly");

// The MOD_NONE get-item entry of an MM item (MergeShip_MMItems_GetItemId). The text id is this
// module's (the .cpp builds it from MM's message); the object and gid are Ocarina of Time's, only loaded / drawn when
// mm.o2r is missing.
GetItemEntry MergeShip_MMItems_GetItemEntry(u8 item) {
    s16 objectId = OBJECT_GI_BOTTLE;
    s16 gid = GID_BOTTLE;
    GetItemEntry entry;

    switch (item) {
        case ITEM_MM_POWDER_KEG:
            objectId = OBJECT_GI_BOMB_1;
            gid = GID_BOMB;
            break;
        case ITEM_MM_PICTOGRAPH_BOX:
            objectId = OBJECT_GI_GLASSES;
            gid = GID_LENS;
            break;
        case ITEM_MM_MOONS_TEAR:
        case ITEM_MM_DEED_LAND:
        case ITEM_MM_DEED_SWAMP:
        case ITEM_MM_DEED_MOUNTAIN:
        case ITEM_MM_DEED_OCEAN:
        case ITEM_MM_ROOM_KEY:
        case ITEM_MM_LETTER_TO_MAMA:
        case ITEM_MM_LETTER_TO_KAFEI:
        case ITEM_MM_PENDANT_OF_MEMORIES:
            objectId = OBJECT_GI_LETTER;
            gid = GID_LETTER_ZELDA;
            break;
        default:
            break;
    }

    // MM: every one of these is GIFIELD_NO_COLLECTIBLE, CHEST_ANIM_LONG.
    entry = (GetItemEntry)GET_ITEM(item, objectId, gid, MERGESHIP_TEXT_MM_ITEMS_BASE + (item - ITEM_MM_POWDER_KEG), 0x80,
                                   CHEST_ANIM_LONG, ITEM_CATEGORY_JUNK, MOD_NONE, MergeShip_MMItems_GetItemId(item));
    entry.drawFunc = MergeShip_MMItems_DrawGi;
    return entry;
}

// ---- Item_Give / Item_CheckObtainability ----------------------------------------------------------------------

// MM Inventory_UpdateItem / Inventory_ReplaceItem's button half: every button showing `oldItem` (or, for a bottle,
// inventory slot `slot`) now shows `newItem`.
static void MergeShip_MMItems_UpdateButtons(PlayState* play, s16 slot, u8 oldItem, u8 newItem) {
    s32 button;

    for (button = 1; button < MS_NUM_BUTTON_ITEMS; button++) {
        if (((slot != SLOT_NONE) && (gSaveContext.equips.cButtonSlots[button - 1] == slot)) ||
            ((slot == SLOT_NONE) && (oldItem != ITEM_NONE) && (gSaveContext.equips.buttonItems[button] == oldItem))) {
            gSaveContext.equips.buttonItems[button] = newItem;
            if (play != NULL) {
                Interface_LoadItemIcon1(play, button);
            }
        }
    }
}

// MM Item_Give's bottle branches. Gold Dust and the Hylian Loach (MM ITEM_GOLD_DUST / ITEM_HYLIAN_LOACH, not their
// "_2" prizes) come in a bottle of their own: an empty bottle slot takes it. Every other content (MM ITEM_POTION_RED ..
// ITEM_OBABA_DRINK) fills the first empty bottle, the button holding that bottle showing it (and enabled).
static u8 MergeShip_MMItems_GiveBottleContent(PlayState* play, u8 item) {
    s16 i;
    s32 button;

    if ((item == ITEM_MM_GOLD_DUST) || (item == ITEM_MM_HYLIAN_LOACH)) {
        // MergeShip2: Majora's Mask's own bottle (MergeShipOwnership.h), on its menu.
        if (MergeShip_Own_MMBottlesHere()) {
            return MergeShip_Own_GiveMMBottle(item) ? ITEM_NONE : item;
        }
        for (i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = item;
                return ITEM_NONE;
            }
        }
        return item;
    }

    if (MergeShip_Own_FillMMBottle(play, item)) {
        return ITEM_NONE; // MergeShip2: Majora's Mask's content into Majora's Mask's own empty bottle first
    }
    for (i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
            if (item == ITEM_MM_HOT_SPRING_WATER) {
                MergeShip_MMItems_StartBottleTimer(i);
            }
            for (button = 1; button < MS_NUM_BUTTON_ITEMS; button++) {
                if (gSaveContext.equips.cButtonSlots[button - 1] == (SLOT_BOTTLE_1 + i)) {
                    gSaveContext.equips.buttonItems[button] = item;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, button);
                    }
                    gSaveContext.buttonStatus[MS_BUTTON_STATUS_INDEX(button)] = BTN_ENABLED;
                    break;
                }
            }
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = item;
            return ITEM_NONE;
        }
    }
    return item;
}

u8 MergeShip_MMItems_Give(PlayState* play, u8 item) {
    u8 result = ITEM_NONE;
    u8* tradeSlot;
    u8 previous;

    if (!MergeShip_MMItems_Enabled()) {
        return item;
    }

    if (item == ITEM_MM_POWDER_KEG) {
        // MM: INV_CONTENT(ITEM_POWDER_KEG) = ITEM_POWDER_KEG; AMMO(ITEM_POWDER_KEG) = 1.
        gMergeShipMMItemsSave.powderKeg = ITEM_MM_POWDER_KEG;
        gMergeShipMMItemsSave.powderKegAmmo = MERGESHIP_POWDER_KEG_MAX_AMMO;
    } else if (item == ITEM_MM_PICTOGRAPH_BOX) {
        // MM's default branch: INV_CONTENT(item) = item.
        gMergeShipMMItemsSave.pictographBox = ITEM_MM_PICTOGRAPH_BOX;
    } else if (MERGESHIP_ITEM_IS_MM_BOTTLE_CONTENT(item)) {
        result = MergeShip_MMItems_GiveBottleContent(play, item);
    } else if ((tradeSlot = MergeShip_MMItems_TradeSlot(item)) != NULL) {
        // MM: the trade slot takes the new item, and a button that held the slot's previous item holds this one.
        previous = *tradeSlot;
        *tradeSlot = item;
        gMergeShipMMItemsSave.tradeOwned |= MERGESHIP_MM_TRADE_BIT(item);
        if ((previous != ITEM_NONE) && (previous != item)) {
            MergeShip_MMItems_UpdateButtons(play, SLOT_NONE, previous, item);
        }
    } else {
        return item;
    }

    // As Item_Give's Return_Item: the item-receive hooks run whatever the result.
    GameInteractor_ExecuteOnItemReceiveHooks(MergeShip_MMItems_GetItemEntry(item));
    return result;
}

s32 MergeShip_MMItems_TradeCycle(PlayState* play, s16 pageSlot) {
    u8* tradeSlot;
    u8 first;
    u8 last;
    u8 item;
    u8 next;

    switch (pageSlot) {
        case MERGESHIP_MM_SLOT_TRADE_DEED:
            first = ITEM_MM_MOONS_TEAR;
            last = ITEM_MM_DEED_OCEAN;
            break;
        case MERGESHIP_MM_SLOT_TRADE_KEY_MAMA:
            first = ITEM_MM_ROOM_KEY;
            last = ITEM_MM_LETTER_TO_MAMA;
            break;
        case MERGESHIP_MM_SLOT_TRADE_COUPLE:
            first = ITEM_MM_LETTER_TO_KAFEI;
            last = ITEM_MM_PENDANT_OF_MEMORIES;
            break;
        default:
            return false;
    }
    tradeSlot = MergeShip_MMItems_TradeSlot(first);
    item = *tradeSlot;
    if ((item >= first) && (item <= last)) {
        gMergeShipMMItemsSave.tradeOwned |= MERGESHIP_MM_TRADE_BIT(item);
    }
    // The next owned one after the shown one, wrapping.
    next = ITEM_NONE;
    for (u8 step = 1; step <= (u8)(last - first + 1); step++) {
        u8 candidate = (u8)(first + ((((item >= first) && (item <= last)) ? (item - first) : 0) + step) %
                                        (last - first + 1));
        if (gMergeShipMMItemsSave.tradeOwned & MERGESHIP_MM_TRADE_BIT(candidate)) {
            next = candidate;
            break;
        }
    }
    if ((next == ITEM_NONE) || (next == item)) {
        return false;
    }
    *tradeSlot = next;
    if (item != ITEM_NONE) {
        MergeShip_MMItems_UpdateButtons(play, SLOT_NONE, item, next);
    }
    return true;
}

u8 MergeShip_MMItems_CheckObtainability(u8 item) {
    s16 i;

    if (!MergeShip_MMItems_Enabled()) {
        return item;
    }
    if (item == ITEM_MM_PICTOGRAPH_BOX) {
        // MM: ITEM_NONE until owned, then the item.
        return gMergeShipMMItemsSave.pictographBox;
    }
    if ((item == ITEM_MM_GOLD_DUST) || (item == ITEM_MM_HYLIAN_LOACH)) {
        // MM returns ITEM_NONE for these (a bottle of their own) whatever the slots hold.
        return ITEM_NONE;
    }
    if (MERGESHIP_ITEM_IS_MM_BOTTLE_CONTENT(item)) {
        for (i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_BOTTLE) {
                return ITEM_NONE;
            }
        }
        return item;
    }
    // MM: the Powder Keg and the trade items are always obtainable.
    return ITEM_NONE;
}

// ---- the player -------------------------------------------------------------------------------------------------

s32 MergeShip_MMItems_ItemToItemAction(u8 item) {
    if (!MergeShip_MMItems_Handles(item)) {
        return -1;
    }
    if (!MergeShip_MMItems_Enabled()) {
        return PLAYER_IA_NONE;
    }
    // MM sItemActions: ITEM_POWDER_KEG -> PLAYER_IA_POWDER_KEG ... ITEM_PENDANT_OF_MEMORIES ->
    // PLAYER_IA_PENDANT_OF_MEMORIES (the reserved ids keep the same order).
    return PLAYER_IA_MM_POWDER_KEG + (item - ITEM_MM_POWDER_KEG);
}

s32 MergeShip_MMItems_ActionToBottle(s32 itemAction) {
    if (MERGESHIP_IA_IS_MM_BOTTLE_CONTENT(itemAction)) {
        return MERGESHIP_IA_TO_MM_BOTTLE(itemAction);
    }
    return -1;
}

s32 MergeShip_MMItems_IsCutsceneItemAction(s32 itemAction) {
    return MERGESHIP_IA_IS_MM_BOTTLE_CONTENT(itemAction) || MERGESHIP_IA_IS_MM_TRADE(itemAction);
}

s32 MergeShip_MMItems_ExchangeGetItemEntry(PlayState* play, Player* this, GetItemEntry* entry) {
    u8 item;

    if (this->exchangeItemId != MERGESHIP_EXCH_ITEM_MM) {
        return false;
    }
    if (!(MERGESHIP_IA_IS_MM_BOTTLE_CONTENT(this->itemAction) || MERGESHIP_IA_IS_MM_TRADE(this->itemAction))) {
        *entry = (GetItemEntry)GET_ITEM_NONE;
        return true;
    }
    item = ITEM_MM_POWDER_KEG + (this->itemAction - PLAYER_IA_MM_POWDER_KEG);
    *entry = MergeShip_MMItems_GetItemEntry(item);

    if (MERGESHIP_IA_IS_MM_BOTTLE_CONTENT(this->itemAction)) {
        // MM Player_Action_ExchangeItem: a bottle is not held up (Player_BottleFromIA > NONE); the bottle stays in the
        // hand, drawn with its content's colour.
        entry->gi = 0;
        return true;
    }

    // The item held up (Player_DrawGetItemImpl draws this entry's drawFunc) until the text closes; then the entry is
    // cleared, so it never stands in for a later exchange's model.
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING) {
        this->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
    } else {
        this->getItemEntry = *entry;
    }
    return true;
}

s32 MergeShip_MMItems_PourActor(s32 itemAction, s16* actorId, s16* params) {
    // MM Player_Action_70's D_8085D80C: { ACTOR_OBJ_AQUA, AQUA_PARAMS(AQUA_TYPE_COLD / AQUA_TYPE_HOT) }.
    if ((itemAction != PLAYER_IA_MM_BOTTLE_SPRING_WATER) && (itemAction != PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER)) {
        return false;
    }
    if (gMergeShipActorId_MM_Obj_Aqua < 0) {
        return false;
    }
    *actorId = gMergeShipActorId_MM_Obj_Aqua;
    *params = (itemAction == PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER) ? MERGESHIP_AQUA_PARAMS_HOT
                                                                    : MERGESHIP_AQUA_PARAMS_COLD;
    return true;
}

// MM Player_InitExplosiveIA, PLAYER_EXPLOSIVE_POWDER_KEG: the fuse starts at 200 frames unless a keg is already lit
// (carried through a loading zone), MM En_Bom (here ovl_MM_En_Bom_Keg) is spawned held, and one keg is used up.
void MergeShip_MMItems_InitKegIA(PlayState* play, Player* this) {
    Actor* keg = NULL;

    if (gMergeShipPowderKegTimer == 0) {
        gMergeShipPowderKegTimer = 200;
    }

    if (gMergeShipActorId_MM_En_Bom_Keg >= 0) {
        keg = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, gMergeShipActorId_MM_En_Bom_Keg,
                                 this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, 0,
                                 this->actor.shape.rot.y, 0, 0);
    }

    if (keg != NULL) {
        MergeShip_MMItems_ChangeKegAmmo(-1);
        // MM func_8082F5FC, as Ocarina of Time's Player_InitExplosiveIA sets up a held bomb.
        this->interactRangeActor = keg;
        this->heldActor = keg;
        this->getItemId = GI_NONE;
        this->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
        this->unk_3BC.y = keg->shape.rot.y - this->actor.shape.rot.y;
        this->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
    } else {
        gMergeShipPowderKegTimer = 0;
    }
}

void MergeShip_MMItems_OnPlayerDeath(void) {
    gMergeShipPowderKegTimer = 0;
}

// ---- the bottle timer (MM Interface_StartBottleTimer / Interface_UpdateBottleTimers) ------------------------------

#define MS_BOTTLE_TIMER_STATE_OFF 0
#define MS_BOTTLE_TIMER_STATE_COUNTING 1
#define MS_SECONDS_TO_TIMER(seconds) ((seconds) * 100)
#define MS_OSTIME_TO_TIMER_ALT(osTime) ((osTime) / 10000 * 64 / 3000)

// MM gSaveContext.bottleTimer*: outside the save (a loaded file's Hot Spring Water stays hot, as in MM).
static u8 sBottleTimerStates[4];
static u64 sBottleTimerCurTimes[4];
static u64 sBottleTimerTimeLimits[4];
static u64 sBottleTimerStartOsTimes[4];
static u64 sBottleTimerPausedOsTimes[4];
static u64 sBottleTimerPausedOsTime = 0;
static s32 sIsBottleTimerPaused = false;

void MergeShip_MMItems_StartBottleTimer(s16 bottleIndex) {
    if ((bottleIndex < 0) || (bottleIndex >= 4)) {
        return;
    }
    // MM Interface_StartBottleTimer(60, i).
    sBottleTimerStates[bottleIndex] = MS_BOTTLE_TIMER_STATE_COUNTING;
    sBottleTimerCurTimes[bottleIndex] = MS_SECONDS_TO_TIMER(60);
    sBottleTimerTimeLimits[bottleIndex] = sBottleTimerCurTimes[bottleIndex];
    sBottleTimerStartOsTimes[bottleIndex] = osGetTime();
    sBottleTimerPausedOsTimes[bottleIndex] = 0;
    sBottleTimerPausedOsTime = 0;
}

static void MergeShip_MMItems_ValidateButtons(PlayState* play);

// MM Inventory_UpdateItem(play, slot, item).
static void MergeShip_MMItems_UpdateSlot(PlayState* play, s16 slot, u8 item) {
    gSaveContext.inventory.items[slot] = item;
    MergeShip_MMItems_UpdateButtons(play, slot, ITEM_NONE, item);
}

void MergeShip_MMItems_UpdateBottleTimers(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    s16 i;
    s16 j;
    u64 osTime;

    // (Every form: a trade item replaced since it was put on a button follows its slot.)
    if (play->pauseCtx.state == 0) {
        MergeShip_MMItems_ValidateButtons(play);
    }

    if (!MergeShip_MMItems_Enabled() || !MergeShip_MMItems_HotWaterCools()) {
        return;
    }

    // Not satisfying any of these conditions pauses the bottle timer.
    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) &&
        (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (msgCtx->msgMode == MSGMODE_NONE) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
        !Play_InCsMode(play)) {

        // Account for osTime when the timer is paused.
        if (sIsBottleTimerPaused) {
            osTime = osGetTime();

            for (j = 0; j < 4; j++) {
                if (sBottleTimerStates[j] == MS_BOTTLE_TIMER_STATE_COUNTING) {
                    sBottleTimerPausedOsTimes[j] += osTime - sBottleTimerPausedOsTime;
                }
            }

            sIsBottleTimerPaused = false;
        }

        for (i = 0; i < 4; i++) {
            if (sBottleTimerStates[i] == MS_BOTTLE_TIMER_STATE_COUNTING) {
                osTime = osGetTime();

                // The total unpaused time since the start of the timer, in centiseconds.
                osTime = MS_OSTIME_TO_TIMER_ALT(osTime - sBottleTimerPausedOsTimes[i] - sBottleTimerStartOsTimes[i]);

                if (osTime == 0) {
                    sBottleTimerCurTimes[i] = sBottleTimerTimeLimits[i] - osTime;
                } else if (osTime <= sBottleTimerTimeLimits[i]) {
                    if (osTime >= sBottleTimerTimeLimits[i]) {
                        sBottleTimerCurTimes[i] = MS_SECONDS_TO_TIMER(0);
                    } else {
                        sBottleTimerCurTimes[i] = sBottleTimerTimeLimits[i] - osTime;
                    }
                } else {
                    // The time limit has been exceeded.
                    sBottleTimerCurTimes[i] = MS_SECONDS_TO_TIMER(0);

                    if (gSaveContext.inventory.items[i + SLOT_BOTTLE_1] == ITEM_MM_HOT_SPRING_WATER) {
                        MergeShip_MMItems_UpdateSlot(play, i + SLOT_BOTTLE_1, ITEM_MM_SPRING_WATER);
                        Message_StartTextbox(play, MERGESHIP_TEXT_MM_HOT_WATER_COOLED, NULL);
                    }
                    sBottleTimerStates[i] = MS_BOTTLE_TIMER_STATE_OFF;
                }
            }
        }
    } else if (!sIsBottleTimerPaused) {
        sBottleTimerPausedOsTime = osGetTime();
        sIsBottleTimerPaused = true;
    }
}

void MergeShip_MMItems_ResetBottleTimers(void) {
    s16 i;

    for (i = 0; i < 4; i++) {
        sBottleTimerStates[i] = MS_BOTTLE_TIMER_STATE_OFF;
    }
    sIsBottleTimerPaused = false;
}

// ---- the interface ----------------------------------------------------------------------------------------------

// A trade item of Majora's Mask on a button that was replaced since (an age swap brings back the other age's
// buttons): the button follows the slot's current item, as MM's own buttons do when the slot changes.
static void MergeShip_MMItems_ValidateButtons(PlayState* play) {
    s32 button;

    for (button = 1; button < MS_NUM_BUTTON_ITEMS; button++) {
        u8 item = gSaveContext.equips.buttonItems[button];
        u8* tradeSlot;

        if (!MergeShip_MMItems_IsPageItem(item) || MergeShip_MMItems_Owns(item)) {
            continue;
        }
        tradeSlot = MergeShip_MMItems_TradeSlot(item);
        gSaveContext.equips.buttonItems[button] = (tradeSlot != NULL) ? *tradeSlot : ITEM_NONE;
        gSaveContext.equips.cButtonSlots[button - 1] = SLOT_NONE;
        if (gSaveContext.equips.buttonItems[button] != ITEM_NONE) {
            Interface_LoadItemIcon1(play, button);
        }
    }
}

static void MergeShip_MMItems_SetStatus(s32 button, u8 status, s16* restoreHud) {
    s32 statusIndex = MS_BUTTON_STATUS_INDEX(button);

    if (gSaveContext.buttonStatus[statusIndex] != status) {
        *restoreHud = true;
    }
    gSaveContext.buttonStatus[statusIndex] = status;
}

void MergeShip_MMItems_UpdateHumanButtons(PlayState* play, s16* restoreHud) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s32 button;

    MergeShip_MMItems_ValidateButtons(play);

    for (button = 1; button < MS_NUM_BUTTON_ITEMS; button++) {
        u8 item = gSaveContext.equips.buttonItems[button];

        if (!MergeShip_MMItems_Handles(item)) {
            continue;
        }
        if (!MergeShip_MMItems_Enabled()) {
            MergeShip_MMItems_SetStatus(button, BTN_DISABLED, restoreHud);
        } else if (item == ITEM_MM_POWDER_KEG) {
            // MM gPlayerFormItemRestrictions[PLAYER_FORM_HUMAN][ITEM_POWDER_KEG] = false: the Goron's alone.
            MergeShip_MMItems_SetStatus(button, BTN_DISABLED, restoreHud);
        } else if (MERGESHIP_ITEM_IS_MM_TRADE(item)) {
            // Ocarina of Time's trade-item class: never disabled by the scene's "all" restriction, disabled by its
            // trade-item restriction.
            MergeShip_MMItems_SetStatus(button,
                                        ((interfaceCtx->restrictions.tradeItems == 0) ||
                                         !GameInteractor_Should(VB_DISABLE_TRADE_ITEM_BUTTON, true, item))
                                            ? BTN_ENABLED
                                            : BTN_DISABLED,
                                        restoreHud);
        }
        // The Pictograph Box (MM's human uses it) follows Ocarina of Time's generic class, and the bottle contents
        // its bottle class (MERGESHIP_ITEM_IS_BOTTLE), both already applied by the caller.
    }
}

s32 MergeShip_MMItems_HasButtonAmmo(u8 item, s16* ammo) {
    if ((item == ITEM_MM_POWDER_KEG) && MergeShip_MMItems_Enabled()) {
        *ammo = gMergeShipMMItemsSave.powderKegAmmo;
        return true;
    }
    if ((item == ITEM_MM_PICTOGRAPH_BOX) && MergeShip_MMItems_Enabled()) {
        // MM Interface_DrawAmmoCount: 1 while a photo is kept (QUEST_PICTOGRAPH), else 0.
        *ammo = MergeShip_Picto_HasPhoto() ? 1 : 0;
        return true;
    }
    if ((item == ITEM_MM_BOW_HERO) && MergeShip_MMItems_Enabled()) {
        // task 4: the Hero's Bow's arrows are Ocarina of Time's Fairy Bow slot's count (MergeShipOwnership.h): it has
        // no AMMO() cell of its own (ITEM_MM_BOW_HERO isn't a real inventory slot), so the C-button HUD never showed
        // it without this (the reported bug: "ammo is not displayed on the Hero's Bow").
        *ammo = MergeShip_Own_PageAmmo(ITEM_BOW, 1);
        return true;
    }
    return false;
}

// ---- pause menu ---------------------------------------------------------------------------------------------

const char* MergeShip_MMItems_NameTexture(u8 item) {
    // MM item_name_static (English; 2S2H's archive carries only English).
    switch (item) {
        case ITEM_MM_POWDER_KEG:
            return mm_gItemNamePowderKegENGTex;
        case ITEM_MM_PICTOGRAPH_BOX:
            return mm_gItemNamePictographBoxENGTex;
        case ITEM_MM_DEKU_PRINCESS:
            return mm_gItemNameDekuPrincessENGTex;
        case ITEM_MM_ZORA_EGG:
            return mm_gItemNameZoraEggENGTex;
        case ITEM_MM_GOLD_DUST:
            return mm_gItemNameGoldDustENGTex;
        case ITEM_MM_MUSHROOM:
            return mm_gItemNameMagicalMushroomENGTex;
        case ITEM_MM_SEAHORSE:
            return mm_gItemNameSeaHorseENGTex;
        case ITEM_MM_SPRING_WATER:
            return mm_gItemNameSpringWaterENGTex;
        case ITEM_MM_HOT_SPRING_WATER:
            return mm_gItemNameHotSpringWaterENGTex;
        case ITEM_MM_MOONS_TEAR:
            return mm_gItemNameMoonsTearENGTex;
        case ITEM_MM_DEED_LAND:
            return mm_gItemNameLandTitleDeedENGTex;
        case ITEM_MM_DEED_SWAMP:
            return mm_gItemNameSwampTitleDeedENGTex;
        case ITEM_MM_DEED_MOUNTAIN:
            return mm_gItemNameMountainTitleDeedENGTex;
        case ITEM_MM_DEED_OCEAN:
            return mm_gItemNameOceanTitleDeedENGTex;
        case ITEM_MM_ROOM_KEY:
            return mm_gItemNameRoomKeyENGTex;
        case ITEM_MM_LETTER_TO_MAMA:
            return mm_gItemNameSpecialDeliveryToMamaENGTex;
        case ITEM_MM_LETTER_TO_KAFEI:
            return mm_gItemNameLetterToKafeiENGTex;
        case ITEM_MM_PENDANT_OF_MEMORIES:
            return mm_gItemNamePendantOfMemoriesENGTex;
        case ITEM_MM_BOW_HERO:
            return mm_gItemNameHerosBowENGTex;
        case ITEM_MM_HOOKSHOT:
            return mm_gItemNameHookshotENGTex;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return mm_gItemNameGreatFairysSwordENGTex;
        default:
            // The Hylian Loach: MM's item_name_static has no name for it (the item is unused in MM).
            return NULL;
    }
}
