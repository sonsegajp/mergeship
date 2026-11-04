#pragma once

// MergeShip2 -- Ocarina of Time's C-button, mask, trade and bottle items in Majora's Mask (2Ship).
//
// Item ids are in z64item.h (ITEM_OOT_*, 0xE0..0xEF and 0xF3..0xF9, plus MM's leftover ITEM_SLINGSHOT and
// ITEM_LONGSHOT), item actions in z64player.h (PLAYER_IA_OOT_*), actors in tables/actor_table.h (ACTOR_OOT_*).
//
// MM's 24-slot item grid is full, so these items live on their own pause page, "Items (Hyrule)" -- the Item face's
// Ocarina of Time view, switched to with C-Up on MM's Select Item -- laid out as Ocarina of Time's item page (OoT SLOT_*
// order). The cells whose Ocarina of Time item Majora's Mask already has (sticks, nuts,
// bombs, bow, arrows, ocarina, bombchus, lens, beans, bottles) stay empty there: those items are on MM's own page.
// Ruto's Letter is a bottle content and lives in one of MM's six bottles.
//
// Only the human can use them (gPlayerFormItemRestrictions); everything is gated by MERGESHIP_OOT_ITEMS_CVAR (default
// on) and needs the Ocarina of Time archive (oot-assets.o2r, mounted under "oot/", MergeShipAssets.h).

#include "ultra64.h"
#include "z64item.h"
#include "z64save.h"

#define MERGESHIP_OOT_ITEMS_CVAR "gEnhancements.MergeShip.OoTItems"
// BUG-0014: SoH's Roc's Feather, its icon and name read from SoH's soh.o2r and mounted here (MergeShipOoTItems.cpp).
#define MERGESHIP_ROCS_ICON_PATH "__OTR__mergeship/rocs/gRocsFeatherTex"
#define MERGESHIP_ROCS_NAME_PATH "__OTR__mergeship/rocs/gRocsFeatherItemNameENGTex"

// The "Items (Hyrule)" grid, Ocarina of Time's item page order (OoT z64item.h InventorySlot).
typedef enum MergeShipOoTSlot {
    /*  0 */ MS_OOT_SLOT_DEKU_STICK, // MM's own item: empty on this face
    /*  1 */ MS_OOT_SLOT_DEKU_NUT,   // MM's own item
    /*  2 */ MS_OOT_SLOT_BOMB,       // MM's own item
    /*  3 */ MS_OOT_SLOT_BOW,        // MM's own item
    /*  4 */ MS_OOT_SLOT_ARROW_FIRE, // MM's own item
    /*  5 */ MS_OOT_SLOT_DINS_FIRE,
    /*  6 */ MS_OOT_SLOT_SLINGSHOT,
    /*  7 */ MS_OOT_SLOT_OCARINA, // MM's own item
    /*  8 */ MS_OOT_SLOT_BOMBCHU, // MM's own item
    /*  9 */ MS_OOT_SLOT_HOOKSHOT, // Hyrule's Hookshot or Longshot (ITEM_LONGSHOT, MergeShip_OoTItems_HookshotLevel)
    /* 10 */ MS_OOT_SLOT_ARROW_ICE, // MM's own item
    /* 11 */ MS_OOT_SLOT_FARORES_WIND,
    /* 12 */ MS_OOT_SLOT_BOOMERANG,
    /* 13 */ MS_OOT_SLOT_LENS,  // MM's own item
    /* 14 */ MS_OOT_SLOT_BEAN,  // MM's own item
    /* 15 */ MS_OOT_SLOT_HAMMER,
    /* 16 */ MS_OOT_SLOT_ARROW_LIGHT, // MM's own item
    /* 17 */ MS_OOT_SLOT_NAYRUS_LOVE,
    /* 18 */ MS_OOT_SLOT_BOTTLE_1, // MM's bottles (Ruto's Letter goes into one of those)
    /* 19 */ MS_OOT_SLOT_BOTTLE_2,
    /* 20 */ MS_OOT_SLOT_BOTTLE_3,
    /* 21 */ MS_OOT_SLOT_BOTTLE_4,
    /* 22 */ MS_OOT_SLOT_TRADE_ADULT, // Pocket Egg .. Claim Check (one at a time; A cycles the owned ones)
    /* 23 */ MS_OOT_SLOT_TRADE_CHILD, // Weird Egg, Chicken, Zelda's Letter, Skull/Spooky/Gerudo masks (A cycles)
    /* 24 */ MS_OOT_SLOT_MAX
} MergeShipOoTSlot;

// A Hyrule cell as a button slot (C_SLOT_EQUIP / DPAD_SLOT_EQUIP, PauseContext::equipTargetSlot): past MM's 48
// inventory slots, so no inventory lookup ever takes it for an MM slot.
#define MS_OOT_BUTTON_SLOT_FIRST 0x30
#define MS_OOT_BUTTON_SLOT(ootSlot) (MS_OOT_BUTTON_SLOT_FIRST + (ootSlot))
#define MS_OOT_BUTTON_SLOT_IS_OOT(slot) \
    (((slot) >= MS_OOT_BUTTON_SLOT_FIRST) && ((slot) < MS_OOT_BUTTON_SLOT_FIRST + MS_OOT_SLOT_MAX))

// Ownership bits (MergeShipOoTItemsSave::owned).
typedef enum MergeShipOoTOwnBit {
    MS_OOT_OWN_DINS_FIRE,
    MS_OOT_OWN_FARORES_WIND,
    MS_OOT_OWN_NAYRUS_LOVE,
    MS_OOT_OWN_BOOMERANG,
    MS_OOT_OWN_HAMMER,
    MS_OOT_OWN_MASK_SKULL,
    MS_OOT_OWN_MASK_SPOOKY,
    MS_OOT_OWN_MASK_GERUDO,
    MS_OOT_OWN_LETTER_RUTO, // unused for ownership (the letter is in a bottle); kept so bits follow the item ids
    MS_OOT_OWN_WEIRD_EGG,
    MS_OOT_OWN_CHICKEN,
    MS_OOT_OWN_LETTER_ZELDA,
    MS_OOT_OWN_POCKET_EGG,
    MS_OOT_OWN_POCKET_CUCCO,
    MS_OOT_OWN_COJIRO,
    MS_OOT_OWN_ODD_MUSHROOM,
    MS_OOT_OWN_ODD_POTION,
    MS_OOT_OWN_SAW,
    MS_OOT_OWN_PRESCRIPTION,
    MS_OOT_OWN_FROG,
    MS_OOT_OWN_EYEDROPS,
    MS_OOT_OWN_CLAIM_CHECK,
    MS_OOT_OWN_DEKU_SEEDS, // unused (seeds are ammo)
    MS_OOT_OWN_SLINGSHOT,
    MS_OOT_OWN_LONGSHOT,
    MS_OOT_OWN_HOOKSHOT, // Hyrule's Hookshot: the hookshot cell at level 1 (ITEM_LONGSHOT, OoT's reach 13 : 26)
    // Hyrule's magic arrows and Lens of Truth (the shared save's Hyrule bits), each game's own version (SharedSave.h):
    // their cells hold MM's item ids (ITEM_ARROW_FIRE .., ITEM_LENS_OF_TRUTH), which play as MM's do, with OoT's
    // icon and name, and never make MM's own version owned.
    MS_OOT_OWN_ARROW_FIRE,
    MS_OOT_OWN_ARROW_ICE,
    MS_OOT_OWN_ARROW_LIGHT,
    MS_OOT_OWN_LENS,
    MS_OOT_OWN_MAX
} MergeShipOoTOwnBit;

// The save data, MergeShipOoTItemsSave, is in z64save.h (ShipSaveInfo::mergeShipOoTItems).

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;
struct Actor;

// The option and the archive.
s32 MergeShip_OoTItems_Enabled(void);

// Inventory.
s32 MergeShip_OoTItems_Owns(u8 item); // ITEM_LONGSHOT: the hookshot cell holds Hyrule's Hookshot or Longshot
// Hyrule's hookshot in the hookshot cell (ITEM_LONGSHOT): 0 none, 1 the Hookshot, 2 the Longshot.
s32 MergeShip_OoTItems_HookshotLevel(void);
u8 MergeShip_OoTItems_GetSlotItem(s32 ootSlot); // ITEM_NONE when the cell is empty (or MM's own item)
s32 MergeShip_OoTItems_SlotOfItem(u8 item);     // MergeShipOoTSlot, or -1
void MergeShip_OoTItems_CycleTradeSlot(s32 ootSlot); // A on a trade cell: the next owned item there
s32 MergeShip_OoTItems_SeedCapacity(void);
u8 MergeShip_OoTItems_Give(struct PlayState* play, u8 item); // Item_GiveImpl for these items; ITEM_NONE when given
u8 MergeShip_OoTItems_CheckObtainability(u8 item);           // Item_CheckObtainabilityImpl for these items
void MergeShip_OoTItems_Remove(u8 item);
// Farore's Wind.
void MergeShip_OoTItems_SetFaroresWind(const RespawnData* point);
void MergeShip_OoTItems_ClearFaroresWind(void);
RespawnData* MergeShip_OoTItems_FaroresTop(void); // OoT gSaveContext.respawn[RESPAWN_MODE_TOP]
void MergeShip_OoTItems_FaroresWindSceneInit(struct PlayState* play);
void MergeShip_OoTItems_FaroresWindSceneCleanup(struct PlayState* play);
void MergeShip_OoTItems_DrawFaroresWindPointer(struct PlayState* play);
void MergeShip_OoTItems_FaroresWindWarp(struct PlayState* play);
s32 MergeShip_OoTItems_TakeFaroresArrival(void);
s32 MergeShip_OoTItems_SetActiveOoTText(u16 textId); // MergeShipOoTGetItemText.cpp

// Graphics and names (Ocarina of Time's own, from oot/).
const char* MergeShip_OoTItems_IconTexture(u8 item);     // 32x32 RGBA32 (OoT icon_item_static)
// A C / D-pad button's bow with a magic arrow (ITEM_BOW_FIRE .. ITEM_BOW_LIGHT): Ocarina of Time's combined icon when
// its slot is the Items (Hyrule) bow cell's (a Hyrule magic arrow equipped onto Hyrule's Fairy Bow), else NULL (MM's).
const char* MergeShip_OoTItems_BowArrowButtonIcon(u8 item, u8 buttonSlot);
const char* MergeShip_OoTItems_NameTexture(u8 item);     // 128x16 IA4 (OoT item_name_static, English)
const char* MergeShip_OoTItems_Name(u8 item);            // English name for logs / menus
// Draws the get-item model of `item` (OoT z_draw.c GetItem_Draw with OoT's GID) at the current matrix.
void MergeShip_OoTItems_DrawGetItem(struct PlayState* play, u8 item);
// Ocarina of Time's English get-item text for `item`, converted to MM's message codes (MergeShipOoTGetItem.cpp).
// Returns 0 when the Ocarina of Time text is unavailable.
s32 MergeShip_OoTItems_GetItemText(u8 item, char* out, s32 outSize, u8* textboxType);
// The next MM textbox header whose icon byte is `item` shows that item's own icon (MM remaps header icons through
// D_801CFF94, where 0xE0.. are Bomber's Notebook photos): set right before an OoT get-item text starts, taken (once) by
// Message_DecodeHeader.
void MergeShip_OoTItems_SetTextIconPending(u8 item);
// BUG-0066: whether a message's icon `itemId` is the Ocarina of Time item a get-item text named (the Zora Mask and the
// Mask of Truth share their ids with Bombers' Notebook photos): true for every other Ocarina of Time item.
s32 MergeShip_OoTItems_TextIconIsOoT(u8 itemId);
// Owned (or not) by Termina's Hyrule items: the shared save's Hyrule masks and trade items (MergeShipSharedSave.cpp).
void MergeShip_OoTItems_SetOwned(u8 item, s32 owned);
s32 MergeShip_OoTItems_TakeTextIcon(u8 headerIcon);
// Gives `item` the way Majora's Mask gives any item: Link holds it up (OoT's model) and OoT's text shows.
void MergeShip_OoTItems_GiveWithCutscene(u8 item);

// The Ocarina of Time item Link is holding up to show (a trade item, Ruto's Letter), or ITEM_NONE.
u8 MergeShip_OoTItems_ShownItem(struct Player* player);
void MergeShip_OoTItems_SetShownItem(u8 item);

// z_player_lib.c draw hooks (MergeShipOoTItemsDraw.inc).
void MergeShip_OoTItems_OverrideHandDL(struct Player* player, s32 limbIndex, Gfx** dList);
// BUG-0031: Termina's ocarina is Hyrule's best ocarina; with only the Fairy Ocarina there, it looks like it -- its icon
// (gItemIcons, MergeShip_OoTItems_UpdateOcarinaLook once a frame) and in hand: young Link holds Ocarina of Time's child
// hand and Fairy Ocarina (MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL), the adult the ocarina in his own hand.
#define MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL "__OTR__oot/objects/object_link_child/gLinkChildRightHandHoldingFairyOcarinaNearDL"
s32 MergeShip_OoTItems_FairyOcarinaShown(void);
void MergeShip_OoTItems_UpdateOcarinaLook(struct PlayState* play);
// At start (BenPort.cpp): Hyrule's Deku pipes / Goron drums / Zora guitar icons from mergeship2.o2r.
void MergeShip_OoTItems_MountInstrumentIcons(void);
void MergeShip_OoTItems_OverrideFirstPersonHandDL(struct Player* player, s32 limbIndex, Gfx** dList);
s32 MergeShip_OoTItems_DrawSlingshotString(struct PlayState* play, struct Player* player);
void MergeShip_OoTItems_PostLimbDraw(struct PlayState* play, struct Player* player, s32 limbIndex);
s32 MergeShip_OoTItems_DrawHeldGetItem(struct PlayState* play, struct Player* player);

// z_arms_hook.c (ArmsHook_Draw): the flying hookshot's tip and chain, Ocarina of Time's own for the ported Longshot
// (the same DLs Ocarina of Time's own ArmsHook_Draw uses for both its ages) -- NULL for Majora's Mask's own Hookshot,
// whose tip/chain stay MM's own (the caller's default). Mirrors Shipwright's MergeShip_MMEquip_HookshotTipDL/
// ChainDL, which do the same the other way for MM's Hookshot in Hyrule.
const char* MergeShip_OoTItems_HookshotTipDL(struct Player* player);
const char* MergeShip_OoTItems_HookshotChainDL(struct Player* player);

// Worn Ocarina of Time mask (drawn on the human's head, z_player_lib.c).
u8 MergeShip_OoTItems_WornMask(void);
void MergeShip_OoTItems_SetWornMask(u8 item);
void MergeShip_OoTItems_DrawWornMask(struct PlayState* play, struct Player* player);

// Test/log hook (no-op outside the MergeShip test harness).
void MergeShipTest_Log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
