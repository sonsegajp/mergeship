#pragma once

// MergeShip2 -- items both games have that are one inventory slot in Majora's Mask: bombs, bombchus, Deku Sticks, Deku
// Nuts, magic beans, the bow (and the ocarina), the quiver, bomb bag and stick / nut capacities, and the bottles. Which
// game owns each is in the shared save (include/mergeship/SharedSave.h MsSharedSave.hyruleOwn / terminaOwn / hyruleBags
// / terminaBags), written live by both engines as items are given (SoH's side: soh/MergeShip/MergeShipOwnership.h).
//
// The user's rule (2026-09-24): every item is its own game's -- on its own game's menu only, in both games -- and usable
// in either game as soon as it is found ("you can use ANY mm item and OOT item when you get them"; "you should ONLY get
// the MM version on ITS OWN menu"). So Termina holds Ocarina of Time's bombs (and the rest) in Majora's Mask's slot and
// shows them on the Items (Hyrule) page only: Majora's Mask's item page shows a slot only when Majora's Mask owns its
// item (z_kaleido_item.c, MergeShip_Own_MMPageSlotItem), its Quest Status page Majora's Mask's own bags
// (z_kaleido_collect.c), the Equipment (Hyrule) page Ocarina of Time's (MergeShipOoTEquipPause.c).
//
// The count is ONE shared pool (task 1, 2026-09-30: the user reversed the earlier per-game split -- bombs, bombchus,
// arrows, Deku Sticks and Deku Nuts are the same count in Hyrule and Termina, carried across the Lost Woods door).
// MsSharedSave.ammo holds it; its capacity is the better bag/quiver level either game has found (hyruleBags /
// terminaBags, MAX of the two) except the Bombchu Bag (chuBagLevel, its own progression, task 3). Both pages show that
// one count (MergeShip_Own_PageAmmo / PageCapacity) -- there is no more "copy in use" to switch, so OnPageEquip /
// OnUseItem are now no-ops kept only so their call sites (z_kaleido_item.c, z_kaleido_oot_item.c, z_player.c) need no
// change. Ocarina of Time's four bottles are the shared save's `ootBottles`, on the Items (Hyrule) page and on a
// button as MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_1 + i); Majora's Mask's six are its own slots. With the unified
// seed's "Combine Shared Items" both games' are one inventory slot too, as before.
//
// Outside mergeship.exe (no shared save) everything is Majora's Mask's, as always.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"

// Majora's Mask owns `item` (ITEM_BOMB, ITEM_BOMBCHU, ITEM_DEKU_STICK, ITEM_DEKU_NUT, ITEM_MAGIC_BEANS, ITEM_BOW,
// ITEM_OCARINA_OF_TIME): its own copy. Any other item: whether its slot holds it.
s32 MergeShip_Own_MMOwns(u8 item);
// Ocarina of Time owns it (Hyrule's copy, held here in Majora's Mask's slot). False outside mergeship.exe.
s32 MergeShip_Own_OoTOwns(u8 item);
// True while Termina holds only Hyrule's Fairy Bow in the shared bow slot (has not found its own Hero's Bow): the
// icon and name shown for it must be Hyrule's then (the C button / D-pad icon and the pause name panel,
// MergeShipSharedSave.cpp / z_kaleido_scope_NES.c).
s32 MergeShip_Own_BowIsOoT(void);
// A button holding ITEM_BOW (its C_SLOT_EQUIP / DPAD_SLOT_EQUIP slot): Ocarina of Time's Fairy Bow when it was equipped
// from the Items (Hyrule) page (that page's bow cell, MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOW)), whatever else Termina owns;
// otherwise MergeShip_Own_BowIsOoT. The C / D-pad icon (MergeShipSharedSave.cpp) and the pause equip animation
// (z_parameter.c) ask this.
s32 MergeShip_Own_ButtonBowIsOoT(u8 buttonSlot);
// A magic arrow on the Items page (mmPage) needs Majora's Mask's Hero's Bow, one on the Items (Hyrule) page Ocarina of
// Time's Fairy Bow (task 5); with the seed's "Combine Shared Items" either bow held counts.
s32 MergeShip_Own_HasBowForArrows(s32 mmPage);
// The bombchu slot's capacity: the shared pool's (Ocarina of Time's 50, or the seed's Bombchu Bag) in mergeship.exe,
// Majora's Mask's bomb bag's (CUR_CAPACITY(UPG_BOMB_BAG)) outside it.
s32 MergeShip_Own_ChuCapacity(void);
// Each game's own quiver / bomb bag level (UPG_QUIVER, UPG_BOMB_BAG), 0 .. 3; Majora's Mask's for any other upgrade is
// GET_CUR_UPG_VALUE, Ocarina of Time's 0.
s32 MergeShip_Own_MMUpgrade(s32 upgrade);
s32 MergeShip_Own_OoTUpgrade(s32 upgrade);
// Majora's Mask's item page reads its slots through this: ITEM_NONE where the slot holds only Ocarina of Time's item.
u8 MergeShip_Own_MMPageSlotItem(s32 slot);

// Item_Give of a Majora's Mask item: before it (ShouldItemGive) is now a no-op (task 1: there is no "copy" left to
// prep); after it (OnItemGive) Majora's Mask owns it (or its bag).
void MergeShip_Own_BeforeMMItemGiven(u8 item);
void MergeShip_Own_OnMMItemGiven(u8 item);
// task 3: while this is true no bombchu ammo is obtainable anywhere in either game (a chest, a check, a drop, a shop)
// -- the unified seed uses the Bombchu Bag (MsSharedSave.hyruleChuBag) and neither game has raised chuBagLevel yet.
s32 MergeShip_Own_ChusLocked(void);
// The unified randomizer delivered Hyrule's Bombchu Bag to Termina (UnifiedDelivery.cpp): merge it in as SoH would
// (one more level, or the whole bag in the seed's single-bag mode).
s32 MergeShip_Own_UnlockMMChus(void);
// Ocarina of Time's copy given here (MsOwnBit), an Ocarina of Time bag reaching `level`, and Ocarina of Time's count of
// `ownBit` filled to its (shared) capacity: the unified randomizer's Hyrule items found in Termina (UnifiedDelivery.cpp).
void MergeShip_Own_GiveOoT(s32 ownBit);
void MergeShip_Own_RaiseOoTUpgrade(s32 upgrade, s32 level);
void MergeShip_Own_FillOoT(s32 ownBit);
// Every frame (MergeShip_OoTEquip_Update): the slots hold what either game owns, at the shared pool's capacity, and
// its one count is kept in the shared save.
void MergeShip_Own_Sync(void);
// An apply changed the shared save: the next Sync binds the pool's counts again.
void MergeShip_Own_Unbind(void);
// BUG-0129: a magic bean was planted (z_player.c, after Inventory_ChangeAmmo(ITEM_MAGIC_BEANS, -1)): while the bean
// slot holds only Hyrule's beans, it was one of Hyrule's (MsSharedSave.hyruleBeans).
void MergeShip_Own_OnBeanPlanted(void);
// Vestigial since ammo became one pool (task 1): no-ops, kept so their call sites need no change.
void MergeShip_Own_OnPageEquip(u8 item, s32 mmPage);
void MergeShip_Own_OnUseItem(u8 item, s32 heldItemButton);
// Vestigial since ammo became one pool (task 1): a no-op, kept so its call site (BeforeEndOfCycleSave) needs no change.
void MergeShip_Own_BeforeEndOfCycle(void);
// AfterEndOfCycleSave: the Song of Time emptied the ammo slots; the shared count survives it, so they take it back.
void MergeShip_Own_AfterEndOfCycle(void);
// The one shared count / capacity a page shows for `item` (ITEM_BOMB .. ITEM_MAGIC_BEANS, ITEM_BOW for the arrows);
// `mmCopy` is unused (both pages show the same values) and kept only for the call sites.
s16 MergeShip_Own_PageAmmo(s16 item, s32 mmCopy);
s16 MergeShip_Own_PageCapacity(s16 item, s32 mmCopy);

// ---- Ocarina of Time's bottles (the shared save's ootBottles) ----
s32 MergeShip_Own_OoTBottlesHere(void);
u8 MergeShip_Own_OoTBottle(s32 index); // 0 .. 3, as Majora's Mask's item (ITEM_NONE: none)
// A button slot is Ocarina of Time's bottle `index` (MS_OOT_BUTTON_SLOT(MS_OOT_SLOT_BOTTLE_1 + index)), or -1.
s32 MergeShip_Own_OoTBottleOfButtonSlot(u8 buttonSlot);
// Inventory_UpdateBottleItem for it: its content is now `item`.
void MergeShip_Own_SetOoTBottle(s32 index, u8 item);
// A new bottle of Ocarina of Time's holding `content` (ITEM_BOTTLE: empty). False when it has four.
s32 MergeShip_Own_GiveOoTBottle(u8 content);
// Item_Give of a bottle content when none of Majora's Mask's bottles is empty: Ocarina of Time's first empty one.
s32 MergeShip_Own_FillOoTBottle(PlayState* play, u8 content);
s32 MergeShip_Own_OoTHasEmptyBottle(void);
// Inventory_ConsumeFairy when Majora's Mask's bottles hold none: Ocarina of Time's.
s32 MergeShip_Own_ConsumeOoTFairy(PlayState* play);

// MergeShipSharedSave.cpp: bottle contents (MsBottle) and items.
u8 MergeShipShared_ItemFromBottle(u8 bottle);
u8 MergeShipShared_BottleFromItem(u8 item);

#ifdef __cplusplus
}
#endif
