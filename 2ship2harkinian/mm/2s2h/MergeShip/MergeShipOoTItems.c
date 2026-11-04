// MergeShip2 -- Ocarina of Time's C-button, mask, trade and bottle items in Majora's Mask: inventory, giving and
// the tables Majora's Mask indexes by item id. See MergeShipOoTItems.h.

#include "global.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipOoTAssets.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOwnership.h"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/ShipUtils.h" // Ship_LoadGuiTextureOnce (BUG-0124)
#include <mergeship/EngineApi.h> // MS_ENGINE_* (BUG-0129)
#include <mergeship/SharedSave.h>
#include <libultraship/bridge/consolevariablebridge.h>

// Ocarina of Time's icons and names of the items both games have (Hyrule's copies on the Items (Hyrule) page,
// MergeShipOwnership.h), from Ocarina of Time's archive.
static const ALIGN_ASSET(2) char gOoTItemIconBombTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBombTex";
static const ALIGN_ASSET(2) char gOoTItemIconBombchuTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBombchuTex";
static const ALIGN_ASSET(2) char gOoTItemIconDekuStickTex[] = "__OTR__oot/textures/icon_item_static/gItemIconDekuStickTex";
static const ALIGN_ASSET(2) char gOoTItemIconDekuNutTex[] = "__OTR__oot/textures/icon_item_static/gItemIconDekuNutTex";
static const ALIGN_ASSET(2) char gOoTItemIconMagicBeanTex[] = "__OTR__oot/textures/icon_item_static/gItemIconMagicBeanTex";
static const ALIGN_ASSET(2) char gOoTItemIconBowTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBowTex";
// Ocarina of Time's bow with a magic arrow on it (OoT gItemIcons[ITEM_BOW_ARROW_FIRE ..]): the icon of the button a
// Hyrule magic arrow is equipped to (MergeShip_OoTItems_BowArrowButtonIcon).
static const ALIGN_ASSET(2) char gOoTItemIconBowFireTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBowFireTex";
static const ALIGN_ASSET(2) char gOoTItemIconBowIceTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBowIceTex";
static const ALIGN_ASSET(2) char gOoTItemIconBowLightTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBowLightTex";
static const ALIGN_ASSET(2) char gOoTItemIconOcarinaFairyTex[] =
    "__OTR__oot/textures/icon_item_static/gItemIconOcarinaFairyTex";
static const ALIGN_ASSET(2) char gOoTItemIconOcarinaOfTimeTex[] =
    "__OTR__oot/textures/icon_item_static/gItemIconOcarinaOfTimeTex";
static const ALIGN_ASSET(2) char gOoTBombItemNameENGTex[] = "__OTR__oot/textures/item_name_static/gBombItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTBombchuItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBombchuItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTDekuStickItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gDekuStickItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTDekuNutItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gDekuNutItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTMagicBeansItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gMagicBeansItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTFairyBowItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gFairyBowItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTFairyOcarinaItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gFairyOcarinaItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTOcarinaOfTimeItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gOcarinaOfTimeItemNameENGTex";


// Ocarina of Time's bottles and their contents (its four bottles on the Items (Hyrule) page, MergeShipOwnership.h).
static const ALIGN_ASSET(2) char gOoTItemIconBottleEmptyTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleEmptyTex";
static const ALIGN_ASSET(2) char gOoTEmptyBottleItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gEmptyBottleItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottlePotionRedTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottlePotionRedTex";
static const ALIGN_ASSET(2) char gOoTRedPotionItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gRedPotionItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottlePotionGreenTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottlePotionGreenTex";
static const ALIGN_ASSET(2) char gOoTGreenPotionItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gGreenPotionItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottlePotionBlueTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottlePotionBlueTex";
static const ALIGN_ASSET(2) char gOoTBluePotionItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBluePotionItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleFairyTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleFairyTex";
static const ALIGN_ASSET(2) char gOoTBottledFairyItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBottledFairyItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleFishTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleFishTex";
static const ALIGN_ASSET(2) char gOoTFishItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gFishItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleMilkFullTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleMilkFullTex";
static const ALIGN_ASSET(2) char gOoTFullMilkItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gFullMilkItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleMilkHalfTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleMilkHalfTex";
static const ALIGN_ASSET(2) char gOoTHalfMilkItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gHalfMilkItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleBlueFireTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleBlueFireTex";
static const ALIGN_ASSET(2) char gOoTBlueFireItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBlueFireItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleBugTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleBugTex";
static const ALIGN_ASSET(2) char gOoTBugItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBugItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottleBigPoeTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottleBigPoeTex";
static const ALIGN_ASSET(2) char gOoTBigPoeItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gBigPoeItemNameENGTex";
static const ALIGN_ASSET(2) char gOoTItemIconBottlePoeTex[] = "__OTR__oot/textures/icon_item_static/gItemIconBottlePoeTex";
static const ALIGN_ASSET(2) char gOoTPoeItemNameENGTex[] =
    "__OTR__oot/textures/item_name_static/gPoeItemNameENGTex";

// Hyrule's ocarina is its Ocarina of Time (else its Fairy Ocarina): the shared save's Hyrule bit.
static s32 MsOwn_HyruleOcarinaOfTime(void) {
    const MsSharedSave* shared = MergeShipHost_IsHosted() ? (const MsSharedSave*)MergeShipHost_SharedSave() : NULL;
    return (shared != NULL) && (shared->items & (1u << MS_ITEM_OCARINA_OF_TIME));
}

s32 MergeShip_OoTItems_FairyOcarinaShown(void) {
    const MsSharedSave* shared = MergeShipHost_IsHosted() ? (const MsSharedSave*)MergeShipHost_SharedSave() : NULL;
    return (shared != NULL) && (shared->items & (1u << MS_ITEM_OCARINA_FAIRY)) &&
           !(shared->items & (1u << MS_ITEM_OCARINA_OF_TIME));
}

// Hyrule's "Form Instrument Icons" (Shipwright MergeShipSohFd.c; mergeship2.o2r's icons, mounted here by
// MergeShip_OoTItems_MountInstrumentIcons): the Deku's, Goron's and Zora's instrument as the ocarina's icon.
static const ALIGN_ASSET(2) char sMsDekuPipesIconTex[] = "__OTR__mergeship2/textures/gItemIconDekuPipesTex";
static const ALIGN_ASSET(2) char sMsGoronDrumsIconTex[] = "__OTR__mergeship2/textures/gItemIconGoronDrumsTex";
static const ALIGN_ASSET(2) char sMsZoraGuitarIconTex[] = "__OTR__mergeship2/textures/gItemIconZoraGuitarTex";
s32 gMergeShipInstrumentIconsMounted = false;

void MergeShip_OoTItems_UpdateOcarinaLook(PlayState* play) {
    static TexturePtr sMMOcarinaIcon = NULL;
    static TexturePtr sShown = NULL;
    TexturePtr want;
    Player* player = ((play != NULL) && (gGameState == (GameState*)play)) ? GET_PLAYER(play) : NULL;
    s32 btn;

    if (sMMOcarinaIcon == NULL) {
        sMMOcarinaIcon = gItemIcons[ITEM_OCARINA_OF_TIME];
    }
    want = MergeShip_OoTItems_FairyOcarinaShown() ? (TexturePtr)gOoTItemIconOcarinaFairyTex : sMMOcarinaIcon;
    if ((player != NULL) && gMergeShipInstrumentIconsMounted &&
        CVarGetInteger("gEnhancements.MergeShip.FormInstrumentIcons", 0)) {
        if (player->transformation == PLAYER_FORM_DEKU) {
            want = (TexturePtr)sMsDekuPipesIconTex;
        } else if (player->transformation == PLAYER_FORM_GORON) {
            want = (TexturePtr)sMsGoronDrumsIconTex;
        } else if (player->transformation == PLAYER_FORM_ZORA) {
            want = (TexturePtr)sMsZoraGuitarIconTex;
        }
    }
    if (want == sShown) {
        return;
    }
    sShown = want;
    gItemIcons[ITEM_OCARINA_OF_TIME] = want;
    Ship_LoadGuiTextureOnce((const char*)want); // (BUG-0124: the item tracker draws it by name)
    if ((play != NULL) && (gGameState == (GameState*)play)) {
        for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
            if (BUTTON_ITEM_EQUIP(0, btn) == ITEM_OCARINA_OF_TIME) {
                Interface_LoadItemIconImpl(play, btn);
            }
        }
    }
}

#define OOT_ITEMS (gSaveContext.save.shipSaveInfo.mergeShipOoTItems)

s32 MergeShip_OoTItems_Enabled(void) {
    return CVarGetInteger(MERGESHIP_OOT_ITEMS_CVAR, 1) && MergeShip_IsOcarinaMounted();
}

// ---- Ownership ------------------------------------------------------------------------------------------------------

// BUG-0066: the own bits of Hyrule's Keaton, Bunny, Goron, Zora and Truth masks, in ownedMasks.
#define MS_OOT_OWN_MASKS_FIRST 32

static s32 MergeShip_OoTItems_OwnTest(s32 bit) {
    return (bit >= MS_OOT_OWN_MASKS_FIRST) ? ((OOT_ITEMS.ownedMasks >> (bit - MS_OOT_OWN_MASKS_FIRST)) & 1)
                                           : ((OOT_ITEMS.owned >> bit) & 1);
}

static void MergeShip_OoTItems_OwnSet(s32 bit, s32 owned) {
    if (bit >= MS_OOT_OWN_MASKS_FIRST) {
        u8 mask = (u8)(1u << (bit - MS_OOT_OWN_MASKS_FIRST));

        OOT_ITEMS.ownedMasks = owned ? (OOT_ITEMS.ownedMasks | mask) : (OOT_ITEMS.ownedMasks & ~mask);
    } else if (bit >= 0) {
        OOT_ITEMS.owned = owned ? (OOT_ITEMS.owned | (1u << bit)) : (OOT_ITEMS.owned & ~(1u << bit));
    }
}

static s32 MergeShip_OoTItems_OwnBit(u8 item) {
    if (item == ITEM_SLINGSHOT) {
        return MS_OOT_OWN_SLINGSHOT;
    }
    if (item == ITEM_LONGSHOT) {
        return MS_OOT_OWN_LONGSHOT;
    }
    switch (item) {
        case ITEM_ARROW_FIRE:
            return MS_OOT_OWN_ARROW_FIRE;
        case ITEM_ARROW_ICE:
            return MS_OOT_OWN_ARROW_ICE;
        case ITEM_ARROW_LIGHT:
            return MS_OOT_OWN_ARROW_LIGHT;
        case ITEM_LENS_OF_TRUTH:
            return MS_OOT_OWN_LENS;
        default:
            break;
    }
    if ((item >= ITEM_OOT_DINS_FIRE) && (item <= ITEM_OOT_ODD_MUSHROOM)) {
        return item - ITEM_OOT_DINS_FIRE;
    }
    if ((item >= ITEM_OOT_ODD_POTION) && (item <= ITEM_OOT_DEKU_SEEDS)) {
        return item - ITEM_OOT_ODD_POTION + MS_OOT_OWN_ODD_POTION;
    }
    // BUG-0066: Hyrule's other masks, past `owned`'s 32 bits (MergeShipOoTItemsSave::ownedMasks).
    switch (item) {
        case ITEM_OOT_MASK_KEATON:
            return MS_OOT_OWN_MASKS_FIRST + 0;
        case ITEM_OOT_MASK_BUNNY:
            return MS_OOT_OWN_MASKS_FIRST + 1;
        case ITEM_OOT_MASK_GORON:
            return MS_OOT_OWN_MASKS_FIRST + 2;
        case ITEM_OOT_MASK_ZORA:
            return MS_OOT_OWN_MASKS_FIRST + 3;
        case ITEM_OOT_MASK_TRUTH:
            return MS_OOT_OWN_MASKS_FIRST + 4;
        default:
            break;
    }
    return -1;
}

s32 MergeShip_OoTItems_Owns(u8 item) {
    s32 bit;

    if (item == ITEM_OOT_ROCS_FEATHER) {
        return MergeShip_OoTItems_HyruleExtra(MS_HYRULE_EXTRA_ROCS_FEATHER);
    }

    // Hyrule's copies of the items both games have that are one slot here (MergeShipOwnership.h).
    switch (item) {
        case ITEM_BOMB:
        case ITEM_BOMBCHU:
        case ITEM_DEKU_STICK:
        case ITEM_DEKU_NUT:
        case ITEM_MAGIC_BEANS:
        case ITEM_BOW:
        case ITEM_OCARINA_OF_TIME:
            return MergeShip_Own_OoTOwns(item);
        default:
            break;
    }

    if (item == ITEM_OOT_LETTER_RUTO) {
        for (bit = 0; bit < 4; bit++) {
            if (MergeShip_Own_OoTBottle(bit) == ITEM_OOT_LETTER_RUTO) {
                return true; // in one of Hyrule's own bottles
            }
        }
        return Inventory_HasItemInBottle(ITEM_OOT_LETTER_RUTO);
    }
    if (item == ITEM_OOT_DEKU_SEEDS) {
        return OOT_ITEMS.seeds > 0;
    }
    if (item == ITEM_LONGSHOT) {
        return MergeShip_OoTItems_HookshotLevel() > 0; // the hookshot cell: Hyrule's Hookshot or Longshot
    }
    bit = MergeShip_OoTItems_OwnBit(item);
    return (bit >= 0) && MergeShip_OoTItems_OwnTest(bit);
}

// Hyrule's hookshot, carried by the door (MergeShipSharedSave.cpp): the Longshot, else the Hookshot (MS_OOT_OWN_HOOKSHOT;
// Ocarina of Time's slot holds one of them, as this cell does, ITEM_LONGSHOT, with its reach, name and icon).
s32 MergeShip_OoTItems_HookshotLevel(void) {
    if (OOT_ITEMS.owned & (1u << MS_OOT_OWN_LONGSHOT)) {
        return 2;
    }
    return (OOT_ITEMS.owned & (1u << MS_OOT_OWN_HOOKSHOT)) ? 1 : 0;
}

// Ocarina of Time's trade sequences, in their order (OoT ItemId order within SLOT_TRADE_CHILD / SLOT_TRADE_ADULT). The
// child slot also holds OoT's masks, all eight of them (BUG-0066: Hyrule's Keaton, Bunny, Goron, Zora and Truth masks
// too, which are not Majora's Mask's).
static const u8 sChildTradeItems[] = {
    ITEM_OOT_WEIRD_EGG,  ITEM_OOT_CHICKEN,    ITEM_OOT_LETTER_ZELDA, ITEM_OOT_MASK_KEATON, ITEM_OOT_MASK_SKULL,
    ITEM_OOT_MASK_SPOOKY, ITEM_OOT_MASK_BUNNY, ITEM_OOT_MASK_GORON,   ITEM_OOT_MASK_ZORA,   ITEM_OOT_MASK_GERUDO,
    ITEM_OOT_MASK_TRUTH,
};
static const u8 sAdultTradeItems[] = {
    ITEM_OOT_POCKET_EGG, ITEM_OOT_POCKET_CUCCO, ITEM_OOT_COJIRO,       ITEM_OOT_ODD_MUSHROOM, ITEM_OOT_ODD_POTION,
    ITEM_OOT_SAW,        ITEM_OOT_PRESCRIPTION, ITEM_OOT_FROG, ITEM_OOT_EYEDROPS,     ITEM_OOT_CLAIM_CHECK,
};

// BUG-0014: Nayru's Love's cell holds SoH's Roc's Feather too (as Hyrule's does: SoH RocsFeatherCycle.c), A switching.
static const u8 sNayruCellItems[] = { ITEM_OOT_NAYRUS_LOVE, ITEM_OOT_ROCS_FEATHER };
static const ALIGN_ASSET(2) char sRocsIconTex[] = MERGESHIP_ROCS_ICON_PATH;
static const ALIGN_ASSET(2) char sRocsNameTex[] = MERGESHIP_ROCS_NAME_PATH;
static u8 sNayruCellCurrent = 0;

static s32 MergeShip_OoTItems_HyruleExtra(u8 bit) {
    const MsSharedSave* shared = (const MsSharedSave*)MergeShipHost_SharedSave();
    return (shared != NULL) && (shared->hyruleExtras & MS_HYRULE_EXTRA_VALID) && (shared->hyruleExtras & bit);
}

static s32 MergeShip_OoTItems_TradeList(s32 ootSlot, const u8** list) {
    if (ootSlot == MS_OOT_SLOT_NAYRUS_LOVE) {
        *list = sNayruCellItems;
        return ARRAY_COUNT(sNayruCellItems);
    }
    if (ootSlot == MS_OOT_SLOT_TRADE_CHILD) {
        *list = sChildTradeItems;
        return ARRAY_COUNT(sChildTradeItems);
    }
    if (ootSlot == MS_OOT_SLOT_TRADE_ADULT) {
        *list = sAdultTradeItems;
        return ARRAY_COUNT(sAdultTradeItems);
    }
    *list = NULL;
    return 0;
}

static u8* MergeShip_OoTItems_TradeCurrent(s32 ootSlot) {
    if (ootSlot == MS_OOT_SLOT_NAYRUS_LOVE) {
        return &sNayruCellCurrent;
    }
    return (ootSlot == MS_OOT_SLOT_TRADE_CHILD) ? &OOT_ITEMS.tradeChild : &OOT_ITEMS.tradeAdult;
}

// The item a trade cell shows: the one last given or cycled to, else the latest owned one of the sequence.
static u8 MergeShip_OoTItems_TradeItem(s32 ootSlot) {
    const u8* list;
    s32 count = MergeShip_OoTItems_TradeList(ootSlot, &list);
    u8 current = *MergeShip_OoTItems_TradeCurrent(ootSlot);
    s32 i;

    if ((current != 0) && MergeShip_OoTItems_Owns(current)) {
        return current;
    }
    for (i = count - 1; i >= 0; i--) {
        if (MergeShip_OoTItems_Owns(list[i])) {
            return list[i];
        }
    }
    return ITEM_NONE;
}

u8 MergeShip_OoTItems_GetSlotItem(s32 ootSlot) {
    static const u8 sSlotItems[MS_OOT_SLOT_MAX] = {
        [MS_OOT_SLOT_DINS_FIRE] = ITEM_OOT_DINS_FIRE,       [MS_OOT_SLOT_SLINGSHOT] = ITEM_SLINGSHOT,
        [MS_OOT_SLOT_HOOKSHOT] = ITEM_LONGSHOT,             [MS_OOT_SLOT_FARORES_WIND] = ITEM_OOT_FARORES_WIND,
        [MS_OOT_SLOT_BOOMERANG] = ITEM_OOT_BOOMERANG,       [MS_OOT_SLOT_HAMMER] = ITEM_OOT_HAMMER,
        [MS_OOT_SLOT_NAYRUS_LOVE] = ITEM_OOT_NAYRUS_LOVE,
    };
    u8 item;

    if ((ootSlot < 0) || (ootSlot >= MS_OOT_SLOT_MAX)) {
        return ITEM_NONE;
    }
    if ((ootSlot == MS_OOT_SLOT_TRADE_CHILD) || (ootSlot == MS_OOT_SLOT_TRADE_ADULT) ||
        (ootSlot == MS_OOT_SLOT_NAYRUS_LOVE)) {
        return MergeShip_OoTItems_TradeItem(ootSlot);
    }
    // Hyrule's copies of the items both games have (the user, 2026-09-24: each on its own game's menu, usable here):
    // bombs .. beans and the bow held in Majora's Mask's slots (MergeShipOwnership.h), the ocarina, the magic arrows and
    // the Lens (Majora's Mask's item ids, Ocarina of Time's icons and names).
    switch (ootSlot) {
        case MS_OOT_SLOT_DEKU_STICK:
            return MergeShip_OoTItems_Owns(ITEM_DEKU_STICK) ? ITEM_DEKU_STICK : ITEM_NONE;
        case MS_OOT_SLOT_DEKU_NUT:
            return MergeShip_OoTItems_Owns(ITEM_DEKU_NUT) ? ITEM_DEKU_NUT : ITEM_NONE;
        case MS_OOT_SLOT_BOMB:
            return MergeShip_OoTItems_Owns(ITEM_BOMB) ? ITEM_BOMB : ITEM_NONE;
        case MS_OOT_SLOT_BOW:
            return MergeShip_OoTItems_Owns(ITEM_BOW) ? ITEM_BOW : ITEM_NONE;
        case MS_OOT_SLOT_OCARINA:
            return MergeShip_OoTItems_Owns(ITEM_OCARINA_OF_TIME) ? ITEM_OCARINA_OF_TIME : ITEM_NONE;
        case MS_OOT_SLOT_BOMBCHU:
            return MergeShip_OoTItems_Owns(ITEM_BOMBCHU) ? ITEM_BOMBCHU : ITEM_NONE;
        case MS_OOT_SLOT_BEAN:
            return MergeShip_OoTItems_Owns(ITEM_MAGIC_BEANS) ? ITEM_MAGIC_BEANS : ITEM_NONE;
        case MS_OOT_SLOT_ARROW_FIRE:
            return MergeShip_OoTItems_Owns(ITEM_ARROW_FIRE) ? ITEM_ARROW_FIRE : ITEM_NONE;
        case MS_OOT_SLOT_ARROW_ICE:
            return MergeShip_OoTItems_Owns(ITEM_ARROW_ICE) ? ITEM_ARROW_ICE : ITEM_NONE;
        case MS_OOT_SLOT_ARROW_LIGHT:
            return MergeShip_OoTItems_Owns(ITEM_ARROW_LIGHT) ? ITEM_ARROW_LIGHT : ITEM_NONE;
        case MS_OOT_SLOT_LENS:
            return MergeShip_OoTItems_Owns(ITEM_LENS_OF_TRUTH) ? ITEM_LENS_OF_TRUTH : ITEM_NONE;
        // Hyrule's four bottles (the shared save's ootBottles), what each holds.
        case MS_OOT_SLOT_BOTTLE_1:
        case MS_OOT_SLOT_BOTTLE_2:
        case MS_OOT_SLOT_BOTTLE_3:
        case MS_OOT_SLOT_BOTTLE_4:
            return MergeShip_Own_OoTBottle(ootSlot - MS_OOT_SLOT_BOTTLE_1);
        default:
            break;
    }
    item = sSlotItems[ootSlot];
    // Unlisted cells are 0 (ITEM_OCARINA_OF_TIME's value): Majora's Mask's own items, empty on this face.
    if ((item == 0) || !MergeShip_OoTItems_Owns(item)) {
        return ITEM_NONE;
    }
    return item;
}

s32 MergeShip_OoTItems_SlotOfItem(u8 item) {
    const u8* list;
    s32 slot;
    s32 i;

    switch (item) {
        case ITEM_OOT_DINS_FIRE:
            return MS_OOT_SLOT_DINS_FIRE;
        case ITEM_SLINGSHOT:
        case ITEM_OOT_DEKU_SEEDS:
            return MS_OOT_SLOT_SLINGSHOT;
        case ITEM_LONGSHOT:
            return MS_OOT_SLOT_HOOKSHOT;
        case ITEM_OOT_FARORES_WIND:
            return MS_OOT_SLOT_FARORES_WIND;
        case ITEM_OOT_BOOMERANG:
            return MS_OOT_SLOT_BOOMERANG;
        case ITEM_OOT_HAMMER:
            return MS_OOT_SLOT_HAMMER;
        case ITEM_OOT_NAYRUS_LOVE:
        case ITEM_OOT_ROCS_FEATHER:
            return MS_OOT_SLOT_NAYRUS_LOVE;
        case ITEM_BOW:
            // Hyrule's Fairy Bow's cell (held in MM's bow slot): the harness's "ootbutton" equips it as the page does,
            // with that cell's button slot (MergeShip_Own_ButtonBowIsOoT). Give / Remove never reach a bow here.
            return MS_OOT_SLOT_BOW;
        case ITEM_ARROW_FIRE:
            // Hyrule's magic arrow cells: the harness's "ootbutton" can still put the bare arrow on a button as the
            // page did before 2026-09-30 (MergeShipSharedSave.cpp MigrateOoTArrowButton moves it onto the bow).
            return MS_OOT_SLOT_ARROW_FIRE;
        case ITEM_ARROW_ICE:
            return MS_OOT_SLOT_ARROW_ICE;
        case ITEM_ARROW_LIGHT:
            return MS_OOT_SLOT_ARROW_LIGHT;
        case ITEM_BOW_FIRE:
        case ITEM_BOW_ICE:
        case ITEM_BOW_LIGHT:
            // The Fairy Bow with a Hyrule magic arrow sits on a button with the bow cell's slot, as in Ocarina of Time
            // (MergeShip_OoTItems_BowArrowButtonIcon): the harness's "ootbutton" puts it there as the page does.
            return MS_OOT_SLOT_BOW;
        default:
            break;
    }
    for (slot = MS_OOT_SLOT_TRADE_ADULT; slot <= MS_OOT_SLOT_TRADE_CHILD; slot++) {
        s32 count = MergeShip_OoTItems_TradeList(slot, &list);

        for (i = 0; i < count; i++) {
            if (list[i] == item) {
                return slot;
            }
        }
    }
    return -1;
}

// OoT Item_Give's trade branch: any button holding the cell's previous item now holds the new one.
static void MergeShip_OoTItems_ReplaceOnButtons(s32 ootSlot, u8 oldItem, u8 newItem) {
    s32 btn;

    if ((oldItem == ITEM_NONE) || (oldItem == newItem)) {
        return;
    }
    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if ((BUTTON_ITEM_EQUIP(0, btn) == oldItem) && (C_SLOT_EQUIP(0, btn) == MS_OOT_BUTTON_SLOT(ootSlot))) {
            BUTTON_ITEM_EQUIP(0, btn) = newItem;
            if (gPlayState != NULL) {
                Interface_LoadItemIconImpl(gPlayState, btn);
            }
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        if ((DPAD_BUTTON_ITEM_EQUIP(0, btn) == oldItem) && (DPAD_SLOT_EQUIP(0, btn) == MS_OOT_BUTTON_SLOT(ootSlot))) {
            DPAD_BUTTON_ITEM_EQUIP(0, btn) = newItem;
            if (gPlayState != NULL) {
                Interface_Dpad_LoadItemIconImpl(gPlayState, btn);
            }
        }
    }
}

void MergeShip_OoTItems_CycleTradeSlot(s32 ootSlot) {
    const u8* list;
    s32 count = MergeShip_OoTItems_TradeList(ootSlot, &list);
    u8 current = MergeShip_OoTItems_TradeItem(ootSlot);
    s32 index = 0;
    s32 i;

    if ((count == 0) || (current == ITEM_NONE)) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (list[i] == current) {
            index = i;
        }
    }
    for (i = 1; i <= count; i++) {
        u8 next = list[(index + i) % count];

        if (MergeShip_OoTItems_Owns(next)) {
            *MergeShip_OoTItems_TradeCurrent(ootSlot) = next;
            MergeShip_OoTItems_ReplaceOnButtons(ootSlot, current, next);
            MergeShipTest_Log("OOTITEMS cycle slot=%d %s -> %s", ootSlot, MergeShip_OoTItems_Name(current),
                              MergeShip_OoTItems_Name(next));
            return;
        }
    }
}

// OoT CUR_CAPACITY(UPG_BULLET_BAG) (gUpgradeCapacities bullet bag row).
s32 MergeShip_OoTItems_SeedCapacity(void) {
    static const s16 sBulletBagCapacities[] = { 0, 30, 40, 50 };

    return sBulletBagCapacities[CLAMP_MAX(OOT_ITEMS.bulletBag, 3)];
}

// ---- Giving ---------------------------------------------------------------------------------------------------------

// Majora's Mask's Item_GiveImpl for Ocarina of Time's items: OoT Item_Give's branches for them (soh z_parameter.c).
// Returns ITEM_NONE when the item was given, as MM's Item_Give does.
u8 MergeShip_OoTItems_Give(PlayState* play, u8 item) {
    s32 bit = MergeShip_OoTItems_OwnBit(item);
    s32 slot;
    s32 i;

    MergeShipTest_Log("OOTITEMS give %s (0x%02X)", MergeShip_OoTItems_Name(item), item);

    if (item == ITEM_SLINGSHOT) {
        // OoT: Inventory_ChangeUpgrade(UPG_BULLET_BAG, 1); INV_CONTENT(ITEM_SLINGSHOT) = ITEM_SLINGSHOT; AMMO = 30.
        if (OOT_ITEMS.bulletBag < 1) {
            OOT_ITEMS.bulletBag = 1;
        }
        OOT_ITEMS.owned |= 1u << bit;
        OOT_ITEMS.seeds = 30;
        return ITEM_NONE;
    }
    if (item == ITEM_OOT_DEKU_SEEDS) {
        // OoT ITEM_SEEDS: +5, capped at the bullet bag.
        s32 seeds = OOT_ITEMS.seeds + 5;

        OOT_ITEMS.seeds = CLAMP_MAX(seeds, MergeShip_OoTItems_SeedCapacity());
        return ITEM_NONE;
    }
    if (item == ITEM_OOT_LETTER_RUTO) {
        // OoT: Ruto's Letter comes in a new bottle -- one of Hyrule's own (MergeShipOwnership.h), else (soh-less, or
        // combined) the first empty bottle slot.
        if (MergeShip_Own_OoTBottlesHere()) {
            return MergeShip_Own_GiveOoTBottle(ITEM_OOT_LETTER_RUTO) ? ITEM_NONE : item;
        }
        for (i = SLOT_BOTTLE_1; i <= SLOT_BOTTLE_6; i++) {
            if (gSaveContext.save.saveInfo.inventory.items[i] == ITEM_NONE) {
                gSaveContext.save.saveInfo.inventory.items[i] = ITEM_OOT_LETTER_RUTO;
                return ITEM_NONE;
            }
        }
        return item;
    }
    if (bit < 0) {
        return item;
    }

    slot = MergeShip_OoTItems_SlotOfItem(item);
    if ((slot == MS_OOT_SLOT_TRADE_CHILD) || (slot == MS_OOT_SLOT_TRADE_ADULT)) {
        // OoT: temp = INV_CONTENT(item); INV_CONTENT(item) = item; the buttons holding temp now hold item.
        u8 old = MergeShip_OoTItems_TradeItem(slot);

        MergeShip_OoTItems_OwnSet(bit, true);
        *MergeShip_OoTItems_TradeCurrent(slot) = item;
        MergeShip_OoTItems_ReplaceOnButtons(slot, old, item);
        return ITEM_NONE;
    }

    MergeShip_OoTItems_OwnSet(bit, true);
    return ITEM_NONE;
}

void MergeShip_OoTItems_SetOwned(u8 item, s32 owned) {
    MergeShip_OoTItems_OwnSet(MergeShip_OoTItems_OwnBit(item), owned);
}

// Item_CheckObtainabilityImpl for Ocarina of Time's items: ITEM_NONE when giving it would add something.
u8 MergeShip_OoTItems_CheckObtainability(u8 item) {
    if ((item == ITEM_OOT_DEKU_SEEDS) || (item == ITEM_OOT_LETTER_RUTO)) {
        return ITEM_NONE;
    }
    if (item == ITEM_LONGSHOT) {
        return (MergeShip_OoTItems_HookshotLevel() >= 2) ? item : ITEM_NONE; // the Longshot upgrades the Hookshot
    }
    if ((item == ITEM_SLINGSHOT) || ITEM_IS_OOT_ITEM(item)) {
        return MergeShip_OoTItems_Owns(item) ? item : ITEM_NONE;
    }
    return ITEM_NONE;
}

void MergeShip_OoTItems_Remove(u8 item) {
    s32 bit = MergeShip_OoTItems_OwnBit(item);
    s32 slot = MergeShip_OoTItems_SlotOfItem(item);
    s32 btn;

    if (bit >= 0) {
        MergeShip_OoTItems_OwnSet(bit, false);
    }
    if ((slot == MS_OOT_SLOT_TRADE_CHILD) || (slot == MS_OOT_SLOT_TRADE_ADULT)) {
        u8* current = MergeShip_OoTItems_TradeCurrent(slot);

        if (*current == item) {
            *current = 0;
        }
    }
    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (BUTTON_ITEM_EQUIP(0, btn) == item) {
            BUTTON_ITEM_EQUIP(0, btn) = ITEM_NONE;
            C_SLOT_EQUIP(0, btn) = SLOT_NONE;
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        if (DPAD_BUTTON_ITEM_EQUIP(0, btn) == item) {
            DPAD_BUTTON_ITEM_EQUIP(0, btn) = ITEM_NONE;
            DPAD_SLOT_EQUIP(0, btn) = SLOT_NONE;
        }
    }
    if (OOT_ITEMS.wornMask == item) {
        OOT_ITEMS.wornMask = 0;
    }
}

// ---- Farore's Wind --------------------------------------------------------------------------------------------------
//
// Ocarina of Time keeps the warp point in gSaveContext.fw (saved) and works on gSaveContext.respawn[RESPAWN_MODE_TOP]
// (in memory; .data is the point's state: >0 set, counting up while it appears, 0x28 settled, <0 vanishing). Majora's
// Mask uses respawn[RESPAWN_MODE_TOP] itself (owl warps, Player_Init copies DOWN into it), so the point lives here:
// sFaroresTop is OoT's respawn[RESPAWN_MODE_TOP], OOT_ITEMS.faroresWind its gSaveContext.fw. Only the warp itself
// hands it to MM's respawn[RESPAWN_MODE_TOP] (respawnFlag 3), right before the transition, as OoT's warp does.

static RespawnData sFaroresTop;
static LightInfo sFaroresLightInfo; // OoT D_8015BC00
static LightNode* sFaroresLight;    // OoT D_8015BC10
static s32 sFaroresTimer;           // OoT D_8015BC14
static f32 sFaroresMove;            // OoT D_8015BC18
static s32 sFaroresArriving;        // the next Player_Init arrives by Farore's Wind

RespawnData* MergeShip_OoTItems_FaroresTop(void) {
    return &sFaroresTop;
}

void MergeShip_OoTItems_SetFaroresWind(const RespawnData* point) {
    OOT_ITEMS.faroresWind = *point;
    OOT_ITEMS.faroresWindSet = true;
}

void MergeShip_OoTItems_ClearFaroresWind(void) {
    OOT_ITEMS.faroresWindSet = false;
}

// OoT func_8002FA60 (Actor_InitContext): the saved point becomes the scene's point, with its light.
void MergeShip_OoTItems_FaroresWindSceneInit(PlayState* play) {
    Vec3f lightPos;

    sFaroresLight = NULL;
    if (!MergeShip_OoTItems_Enabled()) {
        return;
    }
    if (OOT_ITEMS.faroresWindSet) {
        sFaroresTop = OOT_ITEMS.faroresWind;
        sFaroresTop.data = 0x28;
    } else {
        sFaroresTop.data = 0;
        sFaroresTop.pos.x = 0.0f;
        sFaroresTop.pos.y = 0.0f;
        sFaroresTop.pos.z = 0.0f;
    }

    lightPos.x = sFaroresTop.pos.x;
    lightPos.y = sFaroresTop.pos.y + 80.0f;
    lightPos.z = sFaroresTop.pos.z;
    Lights_PointNoGlowSetInfo(&sFaroresLightInfo, lightPos.x, lightPos.y, lightPos.z, 0xFF, 0xFF, 0xFF, -1);
    sFaroresLight = LightContext_InsertLight(play, &play->lightCtx, &sFaroresLightInfo);
    sFaroresTimer = 0;
    sFaroresMove = 0.0f;
}

// OoT func_80030488.
void MergeShip_OoTItems_FaroresWindSceneCleanup(PlayState* play) {
    if (sFaroresLight != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, sFaroresLight);
        sFaroresLight = NULL;
    }
}

static const ALIGN_ASSET(2) char sMsOoTEffFlash1DL[] = "__OTR__oot/objects/gameplay_keep/gEffFlash1DL";

// OoT Actor_DrawFaroresWindPointer (Actor_DrawAll, before the lights' glow).
void MergeShip_OoTItems_DrawFaroresWindPointer(PlayState* play) {
    s32 lightRadius = -1;
    s32 params;

    if ((sFaroresLight == NULL) || !MergeShip_OoTItems_Enabled()) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    params = sFaroresTop.data;

    if (params) {
        f32 yOffset = 60.0f; // OoT: LINK_IS_ADULT ? 80.0f : 60.0f -- MM's human is young Link's size
        f32 ratio = 1.0f;
        s32 alpha = 255;
        s32 temp = params - 40;

        if (temp < 0) {
            sFaroresTop.data = ++params;
            ratio = ABS(params) * 0.025f;
            sFaroresTimer = 60;
            sFaroresMove = 1.0f;
        } else if (sFaroresTimer) {
            sFaroresTimer--;
        } else if (sFaroresMove > 0.0f) {
            static Vec3f effectVel = { 0.0f, -0.05f, 0.0f };
            static Vec3f effectAccel = { 0.0f, -0.025f, 0.0f };
            static Color_RGBA8 effectPrimCol = { 255, 255, 255, 0 };
            static Color_RGBA8 effectEnvCol = { 100, 200, 0, 0 };
            Vec3f* curPos = &sFaroresTop.pos;
            Vec3f* nextPos = &gSaveContext.respawn[RESPAWN_MODE_DOWN].pos;
            f32 prevNum = sFaroresMove;
            Vec3f dist;
            f32 diff = Math_Vec3f_DistXYZAndStoreDiff(nextPos, curPos, &dist);
            Vec3f effectPos;
            f32 factor;
            f32 length;
            f32 dx;
            f32 speed;

            if (diff < 20.0f) {
                sFaroresMove = 0.0f;
                Math_Vec3f_Copy(curPos, nextPos);
            } else {
                length = diff * (1.0f / sFaroresMove);
                speed = 20.0f / length;
                speed = CLAMP_MIN(speed, 0.05f);
                Math_StepToF(&sFaroresMove, 0.0f, speed);
                factor = (diff * (sFaroresMove / prevNum)) / diff;
                curPos->x = nextPos->x + (dist.x * factor);
                curPos->y = nextPos->y + (dist.y * factor);
                curPos->z = nextPos->z + (dist.z * factor);
                length *= 0.5f;
                dx = diff - length;
                yOffset += sqrtf(SQ(length) - SQ(dx)) * 0.2f;
            }

            effectPos.x = curPos->x + Rand_CenteredFloat(6.0f);
            effectPos.y = curPos->y + 80.0f + (6.0f * Rand_ZeroOne());
            effectPos.z = curPos->z + Rand_CenteredFloat(6.0f);

            EffectSsKirakira_SpawnDispersed(play, &effectPos, &effectVel, &effectAccel, &effectPrimCol, &effectEnvCol,
                                            1000, 16);

            if (sFaroresMove == 0.0f) {
                // OoT: respawn[TOP] = respawn[DOWN], playerParams 0x06FF (PLAYER_START_MODE_FARORES_WIND), data 40.
                // MM has no Farore's Wind start mode: its warp-song arrival (invisible, in a cutscene) is used, and
                // Player_Init turns it into OoT's arrival (MergeShip_OoTItems_TakeFaroresArrival).
                sFaroresTop = gSaveContext.respawn[RESPAWN_MODE_DOWN];
                sFaroresTop.playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_WARP_SONG);
                sFaroresTop.data = 40;
            }
        } else if (temp > 0) {
            Vec3f* curPos = &sFaroresTop.pos;
            f32 nextRatio = 1.0f - temp * 0.1f;
            f32 curRatio = 1.0f - (f32)(temp - 1) * 0.1f;
            Vec3f eye;
            Vec3f dist;
            f32 diff;

            if (nextRatio > 0.0f) {
                eye.x = play->view.eye.x;
                eye.y = play->view.eye.y - yOffset;
                eye.z = play->view.eye.z;
                diff = Math_Vec3f_DistXYZAndStoreDiff(&eye, curPos, &dist);
                diff = (diff * (nextRatio / curRatio)) / diff;
                curPos->x = eye.x + (dist.x * diff);
                curPos->y = eye.y + (dist.y * diff);
                curPos->z = eye.z + (dist.z * diff);
            }

            alpha = 255 - (temp * 30);

            if (alpha < 0) {
                OOT_ITEMS.faroresWindSet = false;
                sFaroresTop.data = 0;
                alpha = 0;
            } else {
                sFaroresTop.data = ++params;
            }

            ratio = 1.0f + ((f32)temp * 0.2);
        }

        lightRadius = 500.0f * ratio;

        if ((play->csCtx.state == CS_STATE_IDLE) && (sFaroresTop.entrance == gSaveContext.save.entrance) &&
            (sFaroresTop.roomIndex == play->roomCtx.curRoom.num)) {
            f32 scale = 0.025f * ratio;

            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);

            Matrix_Translate(sFaroresTop.pos.x, sFaroresTop.pos.y + yOffset, sFaroresTop.pos.z, MTXMODE_NEW);
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
            Matrix_Push();

            gDPPipeSync(POLY_XLU_DISP++);
            gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, 255, 255, 200, alpha);
            gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 0, 255);

            Matrix_RotateZF(((play->gameplayFrames * 1500) & 0xFFFF) * 3.14159265358979323846 / 32768.0f, MTXMODE_APPLY);
            MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMsOoTEffFlash1DL);

            Matrix_Pop();
            Matrix_RotateZF(~((play->gameplayFrames * 1200) & 0xFFFF) * 3.14159265358979323846 / 32768.0f, MTXMODE_APPLY);

            MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMsOoTEffFlash1DL);
        }

        Lights_PointNoGlowSetInfo(&sFaroresLightInfo, sFaroresTop.pos.x, sFaroresTop.pos.y + yOffset, sFaroresTop.pos.z,
                                  255, 255, 255, lightRadius);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT Player_Action_8085063C, choice 0 ("Return to the Warp Point"): hand the point to MM's respawn system and go.
void MergeShip_OoTItems_FaroresWindWarp(PlayState* play) {
    gSaveContext.respawn[RESPAWN_MODE_TOP] = sFaroresTop;
    gSaveContext.respawnFlag = 3;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntrance = sFaroresTop.entrance;
    play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
    sFaroresArriving = true;
    MergeShipTest_Log("OOTITEMS farore warp entrance=0x%04X room=%d pos=(%.0f,%.0f,%.0f)", sFaroresTop.entrance,
                      sFaroresTop.roomIndex, sFaroresTop.pos.x, sFaroresTop.pos.y, sFaroresTop.pos.z);
}

// Player_Init: true once when this spawn is Farore's Wind's arrival (OoT PLAYER_START_MODE_FARORES_WIND).
s32 MergeShip_OoTItems_TakeFaroresArrival(void) {
    s32 arriving = sFaroresArriving && (gSaveContext.respawnFlag == 3);

    sFaroresArriving = false;
    return arriving;
}

// ---- Shown item ---------------------------------------------------------------------------------------------------

static u8 sShownItem = ITEM_NONE;

void MergeShip_OoTItems_SetShownItem(u8 item) {
    sShownItem = item;
}

// Link holds up an Ocarina of Time item while MM's exchange action shows it (getItemDrawIdPlusOne set from
// D_8085D1A4) with one of these item actions.
u8 MergeShip_OoTItems_ShownItem(Player* player) {
    if ((player->getItemDrawIdPlusOne == 0) || (player->transformation != PLAYER_FORM_HUMAN)) {
        return ITEM_NONE;
    }
    if ((player->itemAction == PLAYER_IA_OOT_TRADE_ITEM) || (player->itemAction == PLAYER_IA_OOT_BOTTLE_RUTO_LETTER) ||
        (player->exchangeItemAction == PLAYER_IA_OOT_TRADE_ITEM) ||
        (player->exchangeItemAction == PLAYER_IA_OOT_BOTTLE_RUTO_LETTER)) {
        return sShownItem;
    }
    return ITEM_NONE;
}

// ---- Worn mask -----------------------------------------------------------------------------------------------------

u8 MergeShip_OoTItems_WornMask(void) {
    return OOT_ITEMS.wornMask;
}

void MergeShip_OoTItems_SetWornMask(u8 item) {
    OOT_ITEMS.wornMask = item;
}

// ---- Textbox icon -------------------------------------------------------------------------------------------------

static s16 sPendingTextIcon = -1;
static s16 sTakenTextIcon = -1; // BUG-0066: the item the current text's header named (MergeShip_OoTItems_TextIconIsOoT)

void MergeShip_OoTItems_SetTextIconPending(u8 item) {
    sPendingTextIcon = item;
}

s32 MergeShip_OoTItems_TakeTextIcon(u8 headerIcon) {
    if ((sPendingTextIcon >= 0) && (headerIcon == (u8)sPendingTextIcon)) {
        sPendingTextIcon = -1;
        sTakenTextIcon = headerIcon;
        return true;
    }
    sTakenTextIcon = -1;
    return false;
}

s32 MergeShip_OoTItems_TextIconIsOoT(u8 itemId) {
    if ((itemId != ITEM_OOT_MASK_ZORA) && (itemId != ITEM_OOT_MASK_TRUTH)) {
        return true;
    }
    return sTakenTextIcon == itemId; // else a Bombers' Notebook photo (ITEM_C9 / ITEM_CA)
}

// ---- Icons and names ------------------------------------------------------------------------------------------------

const char* MergeShip_OoTItems_IconTexture(u8 item) {
    switch (item) { // Hyrule's copies of the items both games have (their cells hold MM's item ids)
        case ITEM_OOT_ROCS_FEATHER:
            return sRocsIconTex;
        case ITEM_BOTTLE:
            return gOoTItemIconBottleEmptyTex;
        case ITEM_POTION_RED:
            return gOoTItemIconBottlePotionRedTex;
        case ITEM_POTION_GREEN:
            return gOoTItemIconBottlePotionGreenTex;
        case ITEM_POTION_BLUE:
            return gOoTItemIconBottlePotionBlueTex;
        case ITEM_FAIRY:
            return gOoTItemIconBottleFairyTex;
        case ITEM_FISH:
            return gOoTItemIconBottleFishTex;
        case ITEM_MILK_BOTTLE:
            return gOoTItemIconBottleMilkFullTex;
        case ITEM_MILK_HALF:
            return gOoTItemIconBottleMilkHalfTex;
        case ITEM_BLUE_FIRE:
            return gOoTItemIconBottleBlueFireTex;
        case ITEM_BUG:
            return gOoTItemIconBottleBugTex;
        case ITEM_BIG_POE:
            return gOoTItemIconBottleBigPoeTex;
        case ITEM_POE:
            return gOoTItemIconBottlePoeTex;
        case ITEM_BOMB:
            return gOoTItemIconBombTex;
        case ITEM_BOMBCHU:
            return gOoTItemIconBombchuTex;
        case ITEM_DEKU_STICK:
            return gOoTItemIconDekuStickTex;
        case ITEM_DEKU_NUT:
            return gOoTItemIconDekuNutTex;
        case ITEM_MAGIC_BEANS:
            return gOoTItemIconMagicBeanTex;
        case ITEM_BOW:
            return gOoTItemIconBowTex;
        case ITEM_OCARINA_OF_TIME:
            return MsOwn_HyruleOcarinaOfTime() ? gOoTItemIconOcarinaOfTimeTex : gOoTItemIconOcarinaFairyTex;
        case ITEM_ARROW_FIRE:
            return gOoTItemIconArrowFireTex;
        case ITEM_ARROW_ICE:
            return gOoTItemIconArrowIceTex;
        case ITEM_ARROW_LIGHT:
            return gOoTItemIconArrowLightTex;
        case ITEM_LENS_OF_TRUTH:
            return gOoTItemIconLensOfTruthTex;
        // Hyrule's Fairy Bow with its magic arrow, only ever on a button (MergeShip_OoTItems_BowArrowButtonIcon).
        case ITEM_BOW_FIRE:
            return gOoTItemIconBowFireTex;
        case ITEM_BOW_ICE:
            return gOoTItemIconBowIceTex;
        case ITEM_BOW_LIGHT:
            return gOoTItemIconBowLightTex;
        default:
            break;
    }
    if (item == ITEM_SLINGSHOT) {
        return gOoTItemIconSlingshotTex;
    }
    if (item == ITEM_LONGSHOT) {
        return (MergeShip_OoTItems_HookshotLevel() == 1) ? gOoTItemIconHookshotTex : gOoTItemIconLongshotTex;
    }
    if (ITEM_IS_OOT_ITEM(item)) {
        return (const char*)gItemIcons[item];
    }
    if ((item >= ITEM_POTION_RED) && (item <= ITEM_OBABA_DRINK)) {
        return (const char*)gItemIcons[item]; // a content of Majora's Mask's in one of Hyrule's bottles
    }
    return NULL;
}

// A button holding the bow with a magic arrow (ITEM_BOW_FIRE .. ITEM_BOW_LIGHT) whose slot is the Items (Hyrule) bow
// cell's is Hyrule's Fairy Bow with Hyrule's arrow: a Hyrule magic arrow equips onto Hyrule's bow as Ocarina of Time's
// does (OoT KaleidoScope_UpdateItemEquip puts ITEM_BOW_ARROW_FIRE .. on the button with SLOT_BOW; here
// z_kaleido_oot_item.c / KaleidoScope_UpdateItemEquip put MM's ITEM_BOW_FIRE .. with that cell's slot), so the button
// shows Ocarina of Time's combined icon. NULL for Majora's Mask's own arrows on its own bow (SLOT_BOW), which keep MM's
// icon (gItemIcons[ITEM_BOW_FIRE ..]), and without Ocarina of Time's archive.
const char* MergeShip_OoTItems_BowArrowButtonIcon(u8 item, u8 buttonSlot) {
    if ((item < ITEM_BOW_FIRE) || (item > ITEM_BOW_LIGHT) || (buttonSlot != MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW)) ||
        !MergeShip_IsOcarinaMounted()) {
        return NULL;
    }
    return MergeShip_OoTItems_IconTexture(item);
}

const char* MergeShip_OoTItems_NameTexture(u8 item) {
    switch (item) {
        case ITEM_OOT_ROCS_FEATHER:
            return sRocsNameTex;
        case ITEM_BOTTLE:
            return gOoTEmptyBottleItemNameENGTex;
        case ITEM_POTION_RED:
            return gOoTRedPotionItemNameENGTex;
        case ITEM_POTION_GREEN:
            return gOoTGreenPotionItemNameENGTex;
        case ITEM_POTION_BLUE:
            return gOoTBluePotionItemNameENGTex;
        case ITEM_FAIRY:
            return gOoTBottledFairyItemNameENGTex;
        case ITEM_FISH:
            return gOoTFishItemNameENGTex;
        case ITEM_MILK_BOTTLE:
            return gOoTFullMilkItemNameENGTex;
        case ITEM_MILK_HALF:
            return gOoTHalfMilkItemNameENGTex;
        case ITEM_BLUE_FIRE:
            return gOoTBlueFireItemNameENGTex;
        case ITEM_BUG:
            return gOoTBugItemNameENGTex;
        case ITEM_BIG_POE:
            return gOoTBigPoeItemNameENGTex;
        case ITEM_POE:
            return gOoTPoeItemNameENGTex;
        case ITEM_BOMB:
            return gOoTBombItemNameENGTex;
        case ITEM_BOMBCHU:
            return gOoTBombchuItemNameENGTex;
        case ITEM_DEKU_STICK:
            return gOoTDekuStickItemNameENGTex;
        case ITEM_DEKU_NUT:
            return gOoTDekuNutItemNameENGTex;
        case ITEM_MAGIC_BEANS:
            return gOoTMagicBeansItemNameENGTex;
        case ITEM_BOW:
            return gOoTFairyBowItemNameENGTex;
        case ITEM_OCARINA_OF_TIME:
            return MsOwn_HyruleOcarinaOfTime() ? gOoTOcarinaOfTimeItemNameENGTex : gOoTFairyOcarinaItemNameENGTex;
        case ITEM_ARROW_FIRE:
            return gOoTFireArrowItemNameENGTex;
        case ITEM_ARROW_ICE:
            return gOoTIceArrowItemNameENGTex;
        case ITEM_ARROW_LIGHT:
            return gOoTLightArrowItemNameENGTex;
        case ITEM_LENS_OF_TRUTH:
            return gOoTLensItemNameENGTex;
        case ITEM_OOT_DINS_FIRE:
            return gOoTDinsFireItemNameENGTex;
        case ITEM_OOT_FARORES_WIND:
            return gOoTFaroresWindItemNameENGTex;
        case ITEM_OOT_NAYRUS_LOVE:
            return gOoTNayrusLoveItemNameENGTex;
        case ITEM_OOT_BOOMERANG:
            return gOoTBoomerangItemNameENGTex;
        case ITEM_OOT_HAMMER:
            return gOoTMegatonHammerItemNameENGTex;
        case ITEM_OOT_MASK_SKULL:
            return gOoTSkullMaskItemNameENGTex;
        case ITEM_OOT_MASK_SPOOKY:
            return gOoTSpookyMaskItemNameENGTex;
        case ITEM_OOT_MASK_GERUDO:
            return gOoTGerudoMaskItemNameENGTex;
        case ITEM_OOT_MASK_KEATON:
            return gOoTKeatonMaskItemNameENGTex;
        case ITEM_OOT_MASK_BUNNY:
            return gOoTBunnyHoodItemNameENGTex;
        case ITEM_OOT_MASK_GORON:
            return gOoTGoronMaskItemNameENGTex;
        case ITEM_OOT_MASK_ZORA:
            return gOoTZoraMaskItemNameENGTex;
        case ITEM_OOT_MASK_TRUTH:
            return gOoTMaskofTruthItemNameENGTex;
        case ITEM_OOT_LETTER_RUTO:
            return gOoTRutosLetterItemNameENGTex;
        case ITEM_OOT_WEIRD_EGG:
            return gOoTWeirdEggItemNameENGTex;
        case ITEM_OOT_CHICKEN:
            return gOoTCuccoItemNameENGTex;
        case ITEM_OOT_LETTER_ZELDA:
            return gOoTZeldasLetterItemNameENGTex;
        case ITEM_OOT_POCKET_EGG:
            return gOoTPocketEggItemNameENGTex;
        case ITEM_OOT_POCKET_CUCCO:
            return gOoTPocketCuccoItemNameENGTex;
        case ITEM_OOT_COJIRO:
            return gOoTCojiroItemNameENGTex;
        case ITEM_OOT_ODD_MUSHROOM:
            return gOoTOddMushroomItemNameENGTex;
        case ITEM_OOT_ODD_POTION:
            return gOoTOddPotionItemNameENGTex;
        case ITEM_OOT_SAW:
            return gOoTPoachersSawItemNameENGTex;
        case ITEM_OOT_PRESCRIPTION:
            return gOoTPrescriptionItemNameENGTex;
        case ITEM_OOT_FROG:
            return gOoTEyeBallFrogItemNameENGTex;
        case ITEM_OOT_EYEDROPS:
            return gOoTEyeDropsItemNameENGTex;
        case ITEM_OOT_CLAIM_CHECK:
            return gOoTClaimCheckItemNameENGTex;
        case ITEM_SLINGSHOT:
            return gOoTFairySlingshotItemNameENGTex;
        case ITEM_LONGSHOT:
            return (MergeShip_OoTItems_HookshotLevel() == 1) ? gOoTHookshotItemNameENGTex : gOoTLongshotItemNameENGTex;
        default:
            return NULL;
    }
}

const char* MergeShip_OoTItems_Name(u8 item) {
    switch (item) {
        case ITEM_OOT_ROCS_FEATHER:
            return "Roc's Feather";
        case ITEM_BOMB:
            return "Bombs (OOT)";
        case ITEM_BOMBCHU:
            return "Bombchus (OOT)";
        case ITEM_DEKU_STICK:
            return "Deku Stick (OOT)";
        case ITEM_DEKU_NUT:
            return "Deku Nuts (OOT)";
        case ITEM_MAGIC_BEANS:
            return "Magic Beans (OOT)";
        case ITEM_BOW:
            return "Fairy Bow (OOT)";
        case ITEM_OCARINA_OF_TIME:
            return MsOwn_HyruleOcarinaOfTime() ? "Ocarina of Time (OOT)" : "Fairy Ocarina (OOT)";
        case ITEM_ARROW_FIRE:
            return "Fire Arrow (OOT)";
        case ITEM_ARROW_ICE:
            return "Ice Arrow (OOT)";
        case ITEM_ARROW_LIGHT:
            return "Light Arrow (OOT)";
        case ITEM_LENS_OF_TRUTH:
            return "Lens of Truth (OOT)";
        case ITEM_OOT_DINS_FIRE:
            return "Din's Fire";
        case ITEM_OOT_FARORES_WIND:
            return "Farore's Wind";
        case ITEM_OOT_NAYRUS_LOVE:
            return "Nayru's Love";
        case ITEM_OOT_BOOMERANG:
            return "Boomerang";
        case ITEM_OOT_HAMMER:
            return "Megaton Hammer";
        case ITEM_OOT_MASK_SKULL:
            return "Skull Mask";
        case ITEM_OOT_MASK_SPOOKY:
            return "Spooky Mask";
        case ITEM_OOT_MASK_GERUDO:
            return "Gerudo Mask";
        case ITEM_OOT_MASK_KEATON:
            return "Keaton Mask";
        case ITEM_OOT_MASK_BUNNY:
            return "Bunny Hood";
        case ITEM_OOT_MASK_GORON:
            return "Goron Mask";
        case ITEM_OOT_MASK_ZORA:
            return "Zora Mask";
        case ITEM_OOT_MASK_TRUTH:
            return "Mask of Truth";
        case ITEM_OOT_LETTER_RUTO:
            return "Ruto's Letter";
        case ITEM_OOT_WEIRD_EGG:
            return "Weird Egg";
        case ITEM_OOT_CHICKEN:
            return "Cucco";
        case ITEM_OOT_LETTER_ZELDA:
            return "Zelda's Letter";
        case ITEM_OOT_POCKET_EGG:
            return "Pocket Egg";
        case ITEM_OOT_POCKET_CUCCO:
            return "Pocket Cucco";
        case ITEM_OOT_COJIRO:
            return "Cojiro";
        case ITEM_OOT_ODD_MUSHROOM:
            return "Odd Mushroom";
        case ITEM_OOT_ODD_POTION:
            return "Odd Potion";
        case ITEM_OOT_SAW:
            return "Poacher's Saw";
        case ITEM_OOT_PRESCRIPTION:
            return "Prescription";
        case ITEM_OOT_FROG:
            return "Eyeball Frog";
        case ITEM_OOT_EYEDROPS:
            return "World's Finest Eye Drops";
        case ITEM_OOT_CLAIM_CHECK:
            return "Claim Check";
        case ITEM_OOT_DEKU_SEEDS:
            return "Deku Seeds";
        case ITEM_SLINGSHOT:
            return "Fairy Slingshot";
        case ITEM_LONGSHOT:
            return (MergeShip_OoTItems_HookshotLevel() == 1) ? "Hookshot" : "Longshot";
        default:
            return "?";
    }
}

// ---- which game owns the items both games have (MergeShipOwnership.h) -------------------------------------------------

// MsOwnBit order: Majora's Mask's slots' items.
static const u8 sMsOwnItems[MS_OWN_COUNT] = { ITEM_BOMB,        ITEM_BOMBCHU, ITEM_DEKU_STICK,     ITEM_DEKU_NUT,
                                              ITEM_MAGIC_BEANS, ITEM_BOW,     ITEM_OCARINA_OF_TIME };
// Each counted item's capacity upgrade and its MS_BAGS field (-1: none), MsOwnBit MS_OWN_BOMB .. MS_OWN_BOW.
static const s8 sMsOwnUpgrade[MS_OWN_AMMO_COUNT] = { UPG_BOMB_BAG, -1, UPG_DEKU_STICKS, UPG_DEKU_NUTS, -1, UPG_QUIVER };
static const s8 sMsOwnBagsField[MS_OWN_AMMO_COUNT] = { MS_BAGS_FIELD_BOMB_BAG, -1, MS_BAGS_FIELD_STICKS,
                                                       MS_BAGS_FIELD_NUTS,     -1, MS_BAGS_FIELD_QUIVER };
// The slots' counts are the shared save's one ammo pool (task 1, 2026-09-30): true once this engine has bound them
// into the live inventory (cleared by an apply: the next Sync binds again).
static s32 sMsOwnBound = false;

static MsSharedSave* MsOwn_Shared(void) {
    return MergeShipHost_IsHosted() ? (MsSharedSave*)MergeShipHost_SharedSave() : NULL;
}

static s32 MsOwn_Combined(const MsSharedSave* shared) {
    return shared->itemsCombined != 0;
}

static s32 MsOwn_Bit(u8 item) {
    s32 i;

    for (i = 0; i < MS_OWN_COUNT; i++) {
        if (sMsOwnItems[i] == item) {
            return i;
        }
    }
    return -1;
}

static s32 MsOwn_Field(s32 upgrade) {
    return (upgrade == UPG_QUIVER)        ? MS_BAGS_FIELD_QUIVER
           : (upgrade == UPG_BOMB_BAG)    ? MS_BAGS_FIELD_BOMB_BAG
           : (upgrade == UPG_DEKU_STICKS) ? MS_BAGS_FIELD_STICKS
           : (upgrade == UPG_DEKU_NUTS)   ? MS_BAGS_FIELD_NUTS
                                          : -1;
}

// A file from before these bits: everything it holds is Majora's Mask's (written once).
static void MsOwn_EnsureTermina(MsSharedSave* shared) {
    u8 own = 0;
    u8 bags;
    s32 i;

    if (shared->ownValid & MS_OWN_VALID_TERMINA) {
        return;
    }
    for (i = 0; i < MS_OWN_COUNT; i++) {
        own |= (INV_CONTENT(sMsOwnItems[i]) == sMsOwnItems[i]) ? (1 << i) : 0;
    }
    shared->terminaOwn = own;
    bags = MS_BAGS_MAKE(CUR_UPG_VALUE(UPG_QUIVER), CUR_UPG_VALUE(UPG_BOMB_BAG));
    bags = MS_BAGS_WITH(bags, MS_BAGS_FIELD_STICKS, CUR_UPG_VALUE(UPG_DEKU_STICKS));
    shared->terminaBags = MS_BAGS_WITH(bags, MS_BAGS_FIELD_NUTS, CUR_UPG_VALUE(UPG_DEKU_NUTS));
    shared->ownValid |= MS_OWN_VALID_TERMINA;
}

static MsSharedSave* MsOwn_SharedReady(void) {
    MsSharedSave* shared = MsOwn_Shared();

    if ((shared != NULL) && (gSaveContext.gameMode == GAMEMODE_NORMAL)) {
        MsOwn_EnsureTermina(shared);
    }
    return shared;
}

s32 MergeShip_Own_MMOwns(u8 item) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 bit = MsOwn_Bit(item);

    if ((shared == NULL) || (bit < 0) || !(shared->ownValid & MS_OWN_VALID_TERMINA)) {
        return INV_CONTENT(item) == item;
    }
    return (shared->terminaOwn >> bit) & 1;
}

s32 MergeShip_Own_OoTOwns(u8 item) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 bit = MsOwn_Bit(item);

    return (shared != NULL) && (bit >= 0) && ((shared->hyruleOwn >> bit) & 1);
}

s32 MergeShip_Own_MMUpgrade(s32 upgrade) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared != NULL) && (shared->ownValid & MS_OWN_VALID_TERMINA) &&
        ((upgrade == UPG_QUIVER) || (upgrade == UPG_BOMB_BAG))) {
        return MS_BAGS_LEVEL(shared->terminaBags, MsOwn_Field(upgrade));
    }
    return CUR_UPG_VALUE(upgrade);
}

s32 MergeShip_Own_OoTUpgrade(s32 upgrade) {
    MsSharedSave* shared = MsOwn_Shared();

    if ((shared == NULL) || (MsOwn_Field(upgrade) < 0)) {
        return 0;
    }
    return MS_BAGS_LEVEL(shared->hyruleBags, MsOwn_Field(upgrade));
}

u8 MergeShip_Own_MMPageSlotItem(s32 slot) {
    u8 item = gSaveContext.save.saveInfo.inventory.items[slot];

    if ((item != ITEM_NONE) && (MsOwn_Bit(item) >= 0) && (MsOwn_Bit(item) != MS_OWN_OCARINA) &&
        (SLOT(item) == slot) && !MergeShip_Own_MMOwns(item)) {
        return ITEM_NONE; // Ocarina of Time's, held here: on the Items (Hyrule) page
    }
    return item;
}

// Raises a bag's level; true when it went up.
static s32 MsOwn_RaiseBags(u8* bags, s32 field, s32 level) {
    if (level > 3) {
        level = 3;
    }
    if ((field < 0) || (level <= MS_BAGS_LEVEL(*bags, field))) {
        return false;
    }
    *bags = MS_BAGS_WITH(*bags, field, level);
    return true;
}

// ---- ammo: each game's own counts (BUG-0105, 2026-10-12); either game's bag sets the capacity ------------------------

static s32 MsOwn_Has(const MsSharedSave* shared, s32 bit, s32 mm) {
    return ((mm ? shared->terminaOwn : shared->hyruleOwn) >> bit) & 1;
}

// The shared bow slot's (ITEM_BOW) icon / name should be Ocarina of Time's Fairy Bow, not Majora's Mask's Hero's Bow,
// while Termina holds only Hyrule's copy (does not own MS_OWN_BOW itself): the C button / D-pad icon and the pause
// name panel must show Hyrule's then, as the Items (Hyrule) page does (the reported bug, 2026-09-30). Ammo became one
// pool the same day (task 1), which retired the old "copy in use" flag this used to read; ownership says the same
// thing here: Hyrule owns a bow and Termina has not found its own. terminaOwn counts only once it is written
// (MS_OWN_VALID_TERMINA, MsOwn_SharedReady), and MM's minigames lend Majora's Mask's bow on B whatever either game owns
// (bButtonAmmoPlusOne >= 2, as MM's own interface tests it).
s32 MergeShip_Own_BowIsOoT(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared == NULL) || !(shared->ownValid & MS_OWN_VALID_TERMINA) ||
        ((gPlayState != NULL) && (gPlayState->bButtonAmmoPlusOne >= 2))) {
        return false;
    }
    return !MsOwn_Has(shared, MS_OWN_BOW, 1) && MsOwn_Has(shared, MS_OWN_BOW, 0);
}

// Which bow a button holding ITEM_BOW is: its own slot says, as MM's swap-by-slot does. The Items (Hyrule) page equips
// the Fairy Bow as that page's bow cell (z_kaleido_oot_item.c, MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW)); Majora's Mask's
// page equips the Hero's Bow as SLOT_BOW. The global test alone (MergeShip_Own_BowIsOoT) turned every Fairy Bow button
// into the Hero's Bow as soon as Termina owned one too (the reported bug, 2026-09-30: "Fairy bow icon still switches to
// Hero's bow"). A button with no Hyrule mark -- B (MM's horseback / minigame bow), a SLOT_BOW button of a file from
// before the Items (Hyrule) bow cell -- is the only bow Termina holds: MergeShip_Own_BowIsOoT.
s32 MergeShip_Own_ButtonBowIsOoT(u8 buttonSlot) {
    if (buttonSlot == MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW)) {
        return true;
    }
    return MergeShip_Own_BowIsOoT();
}

// The better bag level either game has found for `bit` (an owned item holds at least the first).
static s32 MsOwn_Level(const MsSharedSave* shared, s32 bit) {
    s32 level;

    if (sMsOwnBagsField[bit] < 0) {
        return 0;
    }
    level = MAX(MS_BAGS_LEVEL(shared->hyruleBags, sMsOwnBagsField[bit]),
                MS_BAGS_LEVEL(shared->terminaBags, sMsOwnBagsField[bit]));
    return (level == 0) ? 1 : level;
}

static s32 MsOwn_Capacity(const MsSharedSave* shared, s32 bit) {
    if (sMsOwnUpgrade[bit] >= 0) {
        return gUpgradeCapacities[sMsOwnUpgrade[bit]][MsOwn_Level(shared, bit)];
    }
    if (bit == MS_OWN_BOMBCHU) {
        // task 3: with the seed's Bombchu Bag, the bag's level (MsShared_ChuBagLevel: 0 for a file from before
        // it); without one (no bag in the seed, or no randomizer), Ocarina of Time's flat 50 -- the bag's level is
        // 0 there, and it emptied every chu the moment the pool bound (2026-09-30, the door test).
        return shared->hyruleChuBag
                   ? MsShared_ChuBagCapacity(MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid))
                   : 50;
    }
    return 20; // the bean slot has no bag of its own (vanilla's fixed cap)
}

s32 MergeShip_Own_ChuCapacity(void) {
    MsSharedSave* shared = MsOwn_Shared();

    // The pool's chus are Hyrule's too, whose capacity is OoT's (50, or the seed's Bombchu Bag), not MM's bomb bag's.
    return (shared != NULL) ? MsOwn_Capacity(shared, MS_OWN_BOMBCHU) : CUR_CAPACITY(UPG_BOMB_BAG);
}

s32 MergeShip_Own_HasBowForArrows(s32 mmPage) {
    MsSharedSave* shared = MsOwn_Shared();

    if (mmPage ? MergeShip_Own_MMOwns(ITEM_BOW) : MergeShip_Own_OoTOwns(ITEM_BOW)) {
        return true;
    }
    // "Combine Shared Items": either game's bow is both (MsShared_CombineVersions), and the apply fills this slot from
    // Hyrule's without setting terminaOwn -- any bow held here counts.
    return (shared != NULL) && MsOwn_Combined(shared) && (INV_CONTENT(ITEM_BOW) == ITEM_BOW);
}

// The slot's capacity follows the better bag either game has found.
static void MsOwn_SetLevel(const MsSharedSave* shared, s32 bit) {
    const s32 upgrade = sMsOwnUpgrade[bit];

    if ((upgrade >= 0) && ((s32)CUR_UPG_VALUE(upgrade) != MsOwn_Level(shared, bit))) {
        Inventory_ChangeUpgrade(upgrade, MsOwn_Level(shared, bit));
    }
}

static s16 MsOwn_Clamp(const MsSharedSave* shared, s32 bit, s32 count) {
    const s32 capacity = MsOwn_Capacity(shared, bit);

    return (s16)((count > capacity) ? capacity : count);
}

// Ammo is each game's own (BUG-0105, 2026-10-12: "no shared ammo period"; the one pool of 2026-09-30 is gone,
// MsSharedSave.ammo is no longer read or written): the slots keep Termina's own counts (its file's), clamped to the
// capacity either game's bag gives.
static void MsOwn_Bind(MsSharedSave* shared) {
    s32 bit;

    for (bit = 0; bit < MS_OWN_AMMO_COUNT; bit++) {
        if (!MsOwn_Has(shared, bit, 0) && !MsOwn_Has(shared, bit, 1)) {
            continue;
        }
        MsOwn_SetLevel(shared, bit);
        AMMO(sMsOwnItems[bit]) = MsOwn_Clamp(shared, bit, AMMO(sMsOwnItems[bit]));
    }
    sMsOwnBound = true;
}

void MergeShip_Own_Unbind(void) {
    sMsOwnBound = false;
}

// task 3: no bombchu ammo anywhere (a chest, a check, a grass/pot/enemy drop, a shop) before the unified seed's
// Bombchu Bag has a level -- in either game (the user saw "bombchu ammo dropping despite not getting the bag").
s32 MergeShip_Own_ChusLocked(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    return (shared != NULL) && shared->hyruleChuBag &&
           (MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid) == 0) &&
           !MsOwn_Has(shared, MS_OWN_BOMBCHU, 0) && !MsOwn_Has(shared, MS_OWN_BOMBCHU, 1);
}

// The MsOwnBit an MM give makes Majora's Mask own (-1: none), with the bag it raises (MS_BAGS field, level).
static s32 MsOwn_MMGiveInfo(const MsSharedSave* shared, u8 item, s32* field, s32* level) {
    *field = -1;
    *level = 0;
    switch (item) {
        case ITEM_BOMB_BAG_20:
        case ITEM_BOMB_BAG_30:
        case ITEM_BOMB_BAG_40:
            *field = MS_BAGS_FIELD_BOMB_BAG;
            *level = item - ITEM_BOMB_BAG_20 + 1;
            return MS_OWN_BOMB;
        case ITEM_BOW:
            *field = MS_BAGS_FIELD_QUIVER;
            *level = 1;
            return MS_OWN_BOW;
        case ITEM_QUIVER_30:
        case ITEM_QUIVER_40:
        case ITEM_QUIVER_50:
            *field = MS_BAGS_FIELD_QUIVER;
            *level = item - ITEM_QUIVER_30 + 1;
            return MS_OWN_BOW;
        case ITEM_BOMBCHU:
        case ITEM_BOMBCHUS_1:
        case ITEM_BOMBCHUS_5:
        case ITEM_BOMBCHUS_10:
        case ITEM_BOMBCHUS_20:
            // task 3: a chest, a drop or a shop's chus before the bag give nothing (MergeShip_Own_ChusLocked; the
            // native chokepoints -- Item_CheckObtainabilityImpl, EnGirlA_CanBuyBombchus -- already keep them from
            // ever reaching here, this is a second lock).
            return MergeShip_Own_ChusLocked() ? -1 : MS_OWN_BOMBCHU;
        case ITEM_DEKU_STICK:
        case ITEM_DEKU_STICKS_5:
        case ITEM_DEKU_STICKS_10:
        case ITEM_DEKU_STICK_UPGRADE_20:
        case ITEM_DEKU_STICK_UPGRADE_30:
            // A single stick picked up (a drop, where Ocarina of Time's sticks are what the slot holds) fills those.
            if ((item == ITEM_DEKU_STICK) && MsOwn_Has(shared, MS_OWN_STICK, 0) && !MsOwn_Has(shared, MS_OWN_STICK, 1)) {
                return -1;
            }
            *field = MS_BAGS_FIELD_STICKS;
            *level = (item == ITEM_DEKU_STICK_UPGRADE_20) ? 2 : (item == ITEM_DEKU_STICK_UPGRADE_30) ? 3 : 1;
            return MS_OWN_STICK;
        case ITEM_DEKU_NUT:
        case ITEM_DEKU_NUTS_5:
        case ITEM_DEKU_NUTS_10:
        case ITEM_DEKU_NUT_UPGRADE_30:
        case ITEM_DEKU_NUT_UPGRADE_40:
            if ((item == ITEM_DEKU_NUT) && MsOwn_Has(shared, MS_OWN_NUT, 0) && !MsOwn_Has(shared, MS_OWN_NUT, 1)) {
                return -1;
            }
            *field = MS_BAGS_FIELD_NUTS;
            *level = (item == ITEM_DEKU_NUT_UPGRADE_30) ? 2 : (item == ITEM_DEKU_NUT_UPGRADE_40) ? 3 : 1;
            return MS_OWN_NUT;
        case ITEM_MAGIC_BEANS:
            return MS_OWN_BEAN;
        case ITEM_OCARINA_OF_TIME:
            return MS_OWN_OCARINA;
        default:
            return -1;
    }
}

// The bow slot was empty before this give (MergeShip_Own_BeforeMMItemGiven): MM's Item_Give of a quiver then puts
// MM's own bow there (Item_GiveImpl's ITEM_QUIVER_* branch).
static s32 sMsOwnBowSlotWasEmpty = false;

// Before MM's Item_Give (the bag / ownership bits are set after it, MergeShip_Own_OnMMItemGiven): what a quiver gives
// depends on what the bow slot held.
void MergeShip_Own_BeforeMMItemGiven(u8 item) {
    (void)item;
    sMsOwnBowSlotWasEmpty = (INV_CONTENT(ITEM_BOW) == ITEM_NONE);
}

// BUG-0129: the bean slot holds only Hyrule's beans (Termina has found none of its own): its count is Hyrule's, which
// Hyrule wrote into the shared save (0 writer: a file from before it, the slot's own count then).
static s32 MsOwn_BeansAreHyrules(const MsSharedSave* shared) {
    return (shared != NULL) && (shared->hyruleBeansWriter != 0) && !MsOwn_Combined(shared) &&
           MsOwn_Has(shared, MS_OWN_BEAN, 0) && !MsOwn_Has(shared, MS_OWN_BEAN, 1);
}

void MergeShip_Own_OnBeanPlanted(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if (MsOwn_BeansAreHyrules(shared) && (shared->hyruleBeans > 0)) {
        shared->hyruleBeans--;
        shared->hyruleBeansWriter = MS_ENGINE_2SHIP;
        MergeShipHostTest_Log("BEANS planted one of Hyrule's: %d left", shared->hyruleBeans);
    }
}

void MergeShip_Own_OnMMItemGiven(u8 item) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 field;
    s32 level;
    s32 bit;

    if (shared == NULL) {
        return;
    }
    bit = MsOwn_MMGiveInfo(shared, item, &field, &level);
    if (bit < 0) {
        return;
    }
    MsOwn_RaiseBags(&shared->terminaBags, field, level);
    // A quiver (the Town / Swamp shooting galleries, played with Hyrule's Fairy Bow) is a bigger quiver for the bow
    // already there, not Majora's Mask's Hero's Bow: Termina comes to own MM's bow only by finding it, or when the
    // quiver filled an empty slot (MM's Item_Give then puts its bow there, as an MM randomizer's quiver does). Else the
    // Fairy Bow's slot turned into the Hero's Bow on every icon (the reported bug, 2026-09-30).
    if ((bit == MS_OWN_BOW) && (item >= ITEM_QUIVER_30) && (item <= ITEM_QUIVER_50) && !sMsOwnBowSlotWasEmpty &&
        !MsOwn_Has(shared, MS_OWN_BOW, 1)) {
        return;
    }
    // BUG-0129: Termina's first beans of its own (the bean seller): MM's Item_Give added them to the slot's count, which
    // was Hyrule's -- the slot's count is Termina's own from here, without Hyrule's.
    if ((bit == MS_OWN_BEAN) && MsOwn_BeansAreHyrules(shared)) {
        AMMO(ITEM_MAGIC_BEANS) = (s8)MAX(1, AMMO(ITEM_MAGIC_BEANS) - shared->hyruleBeans);
    }
    shared->terminaOwn |= (u8)(1 << bit);
}

// task 3: the unified randomizer delivered Hyrule's Bombchu Bag to Termina (Rando/MergeShip/UnifiedDelivery.cpp
// Native::ChuBag, a check in Termina's own world holding Hyrule's item): merge it into the shared pool as SoH's
// Context::HandleGetBombchuBag would. SoH never runs that for this copy (chuBagLevel carries it,
// MsDelivery_StateCarried), so this is the only place that raises chuBagLevel for it. Progressive: one more level,
// filled to its capacity. Single (MS_CHU_BAG_SINGLE): the whole bag (level 3) with 20 chus, and 10 more for another.
s32 MergeShip_Own_UnlockMMChus(void) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 live;
    s32 count;
    u8 level;

    if ((shared == NULL) || !(shared->ownValid & MS_OWN_VALID_TERMINA)) {
        return false;
    }
    shared->terminaOwn |= (u8)(1 << MS_OWN_BOMBCHU);
    INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
    live = true; // Termina's own count (BUG-0105)
    count = AMMO(ITEM_BOMBCHU);
    level = MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid);
    if (shared->hyruleChuBag == MS_CHU_BAG_SINGLE) {
        count = (level == 0) ? MAX(count, 20) : (count + 10);
        level = 3;
    } else if (level < 3) {
        level++;
    }
    shared->chuBagLevel = level;
    shared->ownValid |= MS_OWN_VALID_CHU_BAG; // this build wrote it: the raw byte is now trustworthy
    if (shared->hyruleChuBag != MS_CHU_BAG_SINGLE) {
        count = MAX(count, MsOwn_Capacity(shared, MS_OWN_BOMBCHU));
    }
    count = MIN(count, MsOwn_Capacity(shared, MS_OWN_BOMBCHU));
    if (live) {
        AMMO(ITEM_BOMBCHU) = count;
    }
    return true;
}

void MergeShip_Own_GiveOoT(s32 ownBit) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared != NULL) && (ownBit >= 0) && (ownBit < MS_OWN_COUNT)) {
        shared->hyruleOwn |= (u8)(1 << ownBit);
        MergeShip_Own_Sync();
    }
}

void MergeShip_Own_RaiseOoTUpgrade(s32 upgrade, s32 level) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if (shared != NULL) {
        MsOwn_RaiseBags(&shared->hyruleBags, MsOwn_Field(upgrade), level);
        if ((upgrade == UPG_BOMB_BAG) && (level > 0)) {
            shared->hyruleOwn |= (u8)(1 << MS_OWN_BOMB); // Hyrule's bomb bag holds Hyrule's bombs
        }
        if ((upgrade == UPG_QUIVER) && (level > 0)) {
            shared->hyruleOwn |= (u8)(1 << MS_OWN_BOW); // (a quiver is only given with the bow)
        }
        MergeShip_Own_Sync();
    }
}

void MergeShip_Own_FillOoT(s32 ownBit) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared == NULL) || (ownBit < 0) || (ownBit >= MS_OWN_AMMO_COUNT) || !MsOwn_Has(shared, ownBit, 0)) {
        return;
    }
    // Termina's own count, filled (BUG-0105: each game's own).
    MsOwn_SetLevel(shared, ownBit);
    AMMO(sMsOwnItems[ownBit]) = (s8)MsOwn_Capacity(shared, ownBit);
}

// MM's Item_Give of a bag or quiver it already had a level of ends in INV_CONTENT(item) = item, and those items have no
// slot of their own (gItemSlots: 0, the ocarina's): an older build put the Bomb Bag in the ocarina's cell that way (the
// user, 2026-09-25). The ocarina's slot holds only the ocarina: anything else there, and on a button from it, goes.
static void MsOwn_RepairOcarinaSlot(void) {
    u8* items = gSaveContext.save.saveInfo.inventory.items;
    u8 fixed;
    s32 btn;

    if ((items[SLOT_OCARINA] == ITEM_NONE) || (items[SLOT_OCARINA] == ITEM_OCARINA_OF_TIME)) {
        return;
    }
    fixed = MergeShip_Own_MMOwns(ITEM_OCARINA_OF_TIME) ? ITEM_OCARINA_OF_TIME : ITEM_NONE;
    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if ((C_SLOT_EQUIP(0, btn) == SLOT_OCARINA) && (BUTTON_ITEM_EQUIP(0, btn) == items[SLOT_OCARINA])) {
            BUTTON_ITEM_EQUIP(0, btn) = fixed;
            C_SLOT_EQUIP(0, btn) = (fixed == ITEM_NONE) ? SLOT_NONE : SLOT_OCARINA;
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        if ((DPAD_SLOT_EQUIP(0, btn) == SLOT_OCARINA) && (DPAD_BUTTON_ITEM_EQUIP(0, btn) == items[SLOT_OCARINA])) {
            DPAD_BUTTON_ITEM_EQUIP(0, btn) = fixed;
            DPAD_SLOT_EQUIP(0, btn) = (fixed == ITEM_NONE) ? SLOT_NONE : SLOT_OCARINA;
        }
    }
    items[SLOT_OCARINA] = fixed;
}

void MergeShipShared_ApplyButtons(void); // MergeShipSharedSave.cpp (BUG-0101)

void MergeShip_Own_Sync(void) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 i;
    s32 want;

    if ((shared == NULL) || !(shared->ownValid & MS_OWN_VALID_TERMINA)) {
        return;
    }
    MsOwn_RepairOcarinaSlot();
    // Hyrule's bombs .. beans and bow held in Majora's Mask's slots, for use here. The ocarina is not: Hyrule's plays
    // from its Items (Hyrule) cell.
    for (i = MS_OWN_BOMB; i <= MS_OWN_BOW; i++) {
        if (((shared->hyruleOwn >> i) & 1) && (INV_CONTENT(sMsOwnItems[i]) != sMsOwnItems[i])) {
            INV_CONTENT(sMsOwnItems[i]) = sMsOwnItems[i];
            // BUG-0105: new to Termina (Hyrule's find), it arrives full -- its count is Termina's own from here.
            if (i < MS_OWN_AMMO_COUNT) {
                MsOwn_SetLevel(shared, i);
                AMMO(sMsOwnItems[i]) = (s8)MsOwn_Capacity(shared, i);
            }
        }
    }
    if (MsOwn_Combined(shared)) {
        // "Combine Shared Items": both games' are one -- the larger bag, one count (the shared save's apply).
        if (((shared->hyruleOwn >> MS_OWN_STICK) & 1) && (CUR_UPG_VALUE(UPG_DEKU_STICKS) == 0)) {
            Inventory_ChangeUpgrade(UPG_DEKU_STICKS, 1);
        }
        if (((shared->hyruleOwn >> MS_OWN_NUT) & 1) && (CUR_UPG_VALUE(UPG_DEKU_NUTS) == 0)) {
            Inventory_ChangeUpgrade(UPG_DEKU_NUTS, 1);
        }
        want = MAX(MS_BAGS_QUIVER(shared->hyruleBags), MS_BAGS_QUIVER(shared->terminaBags));
        if ((s32)CUR_UPG_VALUE(UPG_QUIVER) < want) {
            Inventory_ChangeUpgrade(UPG_QUIVER, want);
        }
        want = MAX(MS_BAGS_BOMB_BAG(shared->hyruleBags), MS_BAGS_BOMB_BAG(shared->terminaBags));
        if ((s32)CUR_UPG_VALUE(UPG_BOMB_BAG) < want) {
            Inventory_ChangeUpgrade(UPG_BOMB_BAG, want);
        }
        MergeShipShared_ApplyButtons(); // BUG-0101
        return;
    }

    if (!sMsOwnBound) {
        MsOwn_Bind(shared);
    }
    for (i = 0; i < MS_OWN_AMMO_COUNT; i++) {
        if (!MsOwn_Has(shared, i, 0) && !MsOwn_Has(shared, i, 1)) {
            continue;
        }
        MsOwn_SetLevel(shared, i);
    }
    // BUG-0129: the bean slot holding only Hyrule's beans counts Hyrule's (not a free full slot, and not emptied by
    // Majora's Mask's end of the cycle: they are Hyrule's beans)
    if (MsOwn_BeansAreHyrules(shared) && (INV_CONTENT(ITEM_MAGIC_BEANS) == ITEM_MAGIC_BEANS)) {
        AMMO(ITEM_MAGIC_BEANS) = (s8)shared->hyruleBeans;
    }
    MergeShipShared_ApplyButtons(); // BUG-0101: the buttons Hyrule left, now that the slots hold its items
}

// Vestigial since ammo became one pool (task 1, 2026-09-30): Majora's Mask's Song of Time no longer needs to empty a
// separate "copy" and restore Ocarina of Time's afterwards -- there is only the one shared count, and it survives the
// cycle exactly as the rest of the shared save does (MergeShip_Own_AfterEndOfCycle). Kept as a no-op so its call site
// (MergeShipOoTEquip.cpp's BeforeEndOfCycleSave hook) needs no change.
void MergeShip_Own_BeforeEndOfCycle(void) {
}

// MM's Sram_SaveEndOfCycle zeroes every ammo slot. Since ammo is Termina's own again (BUG-0105) that is Majora's Mask's
// own rule (or 2Ship's "Do Not Reset Consumables"), Hyrule's counts untouched: nothing to restore.
void MergeShip_Own_AfterEndOfCycle(void) {
    return;
}

static s32 MsOwn_CountedBit(u8 item) {
    switch (item) {
        case ITEM_BOW:
        case ITEM_ARROW_FIRE:
        case ITEM_ARROW_ICE:
        case ITEM_ARROW_LIGHT:
        case ITEM_BOW_FIRE:
        case ITEM_BOW_ICE:
        case ITEM_BOW_LIGHT:
            return MS_OWN_BOW;
        default: {
            s32 bit = MsOwn_Bit(item);
            return ((bit >= 0) && (bit < MS_OWN_AMMO_COUNT)) ? bit : -1;
        }
    }
}

// Vestigial since ammo became one pool (task 1, 2026-09-30): there is no "copy in use" left to switch when a page or a
// button equips an item, so these are no-ops. Kept (with their call sites in z_kaleido_item.c, z_kaleido_oot_item.c,
// z_player.c) rather than pulled out everywhere they're called from.
void MergeShip_Own_OnPageEquip(u8 item, s32 mmPage) {
    (void)item;
    (void)mmPage;
}

void MergeShip_Own_OnUseItem(u8 item, s32 heldItemButton) {
    (void)item;
    (void)heldItemButton;
}

s16 MergeShip_Own_PageAmmo(s16 item, s32 mmCopy) {
    MsSharedSave* shared = MsOwn_SharedReady();
    const s32 bit = MsOwn_CountedBit((u8)item);

    (void)mmCopy; // one count per slot: both pages show Termina's own (BUG-0105)
    (void)shared;
    (void)bit;
    return AMMO(item);
}

s16 MergeShip_Own_PageCapacity(s16 item, s32 mmCopy) {
    MsSharedSave* shared = MsOwn_SharedReady();
    const s32 bit = MsOwn_CountedBit((u8)item);

    (void)mmCopy; // one shared pool now (task 1): both pages show the same capacity
    if ((shared == NULL) || (bit < 0)) {
        return 0;
    }
    return (s16)MsOwn_Capacity(shared, bit);
}

// ---- Ocarina of Time's bottles -------------------------------------------------------------------------------------

s32 MergeShip_Own_OoTBottlesHere(void) {
    MsSharedSave* shared = MsOwn_Shared();

    return (shared != NULL) && !MsOwn_Combined(shared);
}

u8 MergeShip_Own_OoTBottle(s32 index) {
    MsSharedSave* shared = MsOwn_Shared();

    if ((shared == NULL) || MsOwn_Combined(shared) || (index < 0) || (index >= 4)) {
        return ITEM_NONE;
    }
    return MergeShipShared_ItemFromBottle(shared->ootBottles[index]);
}

s32 MergeShip_Own_OoTBottleOfButtonSlot(u8 buttonSlot) {
    if ((buttonSlot >= MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_1)) &&
        (buttonSlot <= MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_4))) {
        return buttonSlot - MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_1);
    }
    return -1;
}

void MergeShip_Own_SetOoTBottle(s32 index, u8 item) {
    MsSharedSave* shared = MsOwn_Shared();
    const u8 bottle = MergeShipShared_BottleFromItem(item);

    if ((shared != NULL) && (index >= 0) && (index < 4) && (bottle != MS_BOTTLE_NONE)) {
        shared->ootBottles[index] = bottle;
    }
}

s32 MergeShip_Own_GiveOoTBottle(u8 content) {
    MsSharedSave* shared = MsOwn_Shared();
    const u8 bottle = MergeShipShared_BottleFromItem(content);
    s32 i;

    if ((shared == NULL) || (bottle == MS_BOTTLE_NONE)) {
        return false;
    }
    for (i = 0; i < 4; i++) {
        if (shared->ootBottles[i] == MS_BOTTLE_NONE) {
            shared->ootBottles[i] = bottle;
            return true;
        }
    }
    return false;
}

// Every button holding Ocarina of Time's bottle `index` now shows `item`.
static void MsOwn_RefreshOoTBottleButtons(PlayState* play, s32 index, u8 item) {
    const u8 slot = MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_1 + index);
    s32 btn;

    for (btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (C_SLOT_EQUIP(0, btn) == slot) {
            BUTTON_ITEM_EQUIP(0, btn) = item;
            if (play != NULL) {
                Interface_LoadItemIconImpl(play, btn);
            }
            gSaveContext.buttonStatus[btn] = BTN_ENABLED;
        }
    }
    for (btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        if (DPAD_SLOT_EQUIP(0, btn) == slot) {
            DPAD_BUTTON_ITEM_EQUIP(0, btn) = item;
            if (play != NULL) {
                Interface_Dpad_LoadItemIconImpl(play, btn);
            }
        }
    }
}

s32 MergeShip_Own_FillOoTBottle(PlayState* play, u8 content) {
    MsSharedSave* shared = MsOwn_Shared();
    const u8 bottle = MergeShipShared_BottleFromItem(content);
    s32 i;

    if ((shared == NULL) || MsOwn_Combined(shared) || (bottle == MS_BOTTLE_NONE)) {
        return false;
    }
    for (i = 0; i < 4; i++) {
        if (shared->ootBottles[i] == MS_BOTTLE_EMPTY) {
            shared->ootBottles[i] = bottle;
            MsOwn_RefreshOoTBottleButtons(play, i, content);
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_OoTHasEmptyBottle(void) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 i;

    for (i = 0; (shared != NULL) && !MsOwn_Combined(shared) && (i < 4); i++) {
        if (shared->ootBottles[i] == MS_BOTTLE_EMPTY) {
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_ConsumeOoTFairy(PlayState* play) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 i;

    for (i = 0; (shared != NULL) && !MsOwn_Combined(shared) && (i < 4); i++) {
        if (shared->ootBottles[i] == MS_BOTTLE_FAIRY) {
            shared->ootBottles[i] = MS_BOTTLE_EMPTY;
            MsOwn_RefreshOoTBottleButtons(play, i, ITEM_BOTTLE);
            return true;
        }
    }
    return false;
}
