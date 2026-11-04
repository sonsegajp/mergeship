#pragma once

// MergeShip2 -- "Termina Equipment": Majora's Mask's own swords and shields, seen and chosen on a view of 2Ship's
// pause screen. The port of SoH's Termina view of its Equipment page (Shipwright
// soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_mergeship.c "The Termina equipment view",
// soh/soh/MergeShip/MergeShipMMEquipment.c MergeShip_MMEquip_Termina*), where Hyrule's Link chooses MM's gear.
//
// Majora's Mask has no equipment page (its sword and shield sit on the Quest page) and no way to choose one: it keeps
// one sword and one shield, the last it was given. Here Termina keeps the swords and shields it has obtained
// (MergeShipOoTEquipSave::terminaSwords / terminaShields, z64save.h) and the view equips any of them:
//   - a sword: MM's equipment value (ItemEquips::equipment, EQUIP_TYPE_SWORD) and the human's B button, as MM's
//     Item_GiveImpl sets them; an Ocarina of Time sword on B (MergeShipOoTEquip.h) is put away. The Razor Sword keeps
//     its hits left (playerData.swordHealth) while another sword is in use;
//   - the Great Fairy's Sword: on the human's B (MergeShipOoTEquip.h MS_OOTEQ_SWORD_GREAT_FAIRY; it stays MM's C item
//     too), as SoH's view puts it on B. Needs "Ocarina of Time Equipment" (which keeps B in step), grey otherwise;
//   - a shield: MM's equipment value (EQUIP_TYPE_SHIELD); an Ocarina of Time shield in use is put away.
// Swords are for the human only (grey while transformed: the B button is the form's), as SoH's view.
//
// What Termina holds follows Majora's Mask's own events (MergeShip_TerminaEquip_Observe, every frame): a sword or
// shield MM gives or equips is added; one MM takes -- handed to the smith for reforging, stolen by Takkuri, eaten by a
// Like Like: MM's value goes to "none" -- is gone; the Razor Sword is gone when it breaks (MM func_8083FFEC) and at the
// Song of Time (MM Sram_SaveEndOfCycle), unless 2S2H's "Do not reset Razor Sword" or a randomizer seed keeps it.
// Ocarina of Time's gear stays Ocarina of Time's (its own view, MergeShipOoTEquipPause.c): Hyrule's Mirror Shield is
// not Termina's.
//
// Pause: a view of the Quest face, after Hyrule's equipment on C-Up's cycle: Termina's quest status -> Hyrule's quest
// status -> Hyrule's equipment -> Termina's equipment -> Termina's quest status (z_kaleido_mergeship.c; a view whose
// option is off is skipped). B goes back to Termina's quest status (MergeShipTerminaEquipPause.c).
//
// Door: the shared save carries Termina's record and the sword and shield in use (MsSharedSave::terminaSwords ..
// terminaGearValid, include/mergeship/SharedSave.h), so the choice made here comes back from Hyrule as it was; gear
// Hyrule adds in between (the shared save's own apply) is equipped as MM equips a new sword.
//
// Option: MERGESHIP_TERMINA_EQUIP_CVAR (Enhancements > Mergeship "Termina Equipment", default on). The view draws on
// Ocarina of Time's equipment page panels, so it also needs oot-assets.o2r (MergeShipAssets.h).

#include "ultra64.h"
#include "z64item.h"

#define MERGESHIP_TERMINA_EQUIP_CVAR "gEnhancements.MergeShip.TerminaEquipment"

// Bits of MergeShipOoTEquipSave::terminaSwords (EQUIP_VALUE_SWORD_KOKIRI .. _GILDED, the same order as the shared
// save's MS_SWORD_KOKIRI .. MS_SWORD_GILDED) and ::terminaShields (EQUIP_VALUE_SHIELD_HERO, _MIRROR).
#define MS_TEQ_SWORD_BIT(value) (1u << ((value) - 1))
#define MS_TEQ_SHIELD_BIT(value) (1u << ((value) - 1))

// MsSharedSave::terminaSword: MM's sword value (EQUIP_VALUE_SWORD_*), and this bit while the Great Fairy's Sword is on B.
#define MS_TEQ_SHARED_GREAT_FAIRY_ON_B 0x80

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct MsSharedSave;

// ---- state (MergeShipTerminaEquip.c) ----------------------------------------------------------------------------------
// The option is on and the Ocarina of Time archive (the page's panels) is mounted.
s32 MergeShip_TerminaEquip_Enabled(void);
// The view's items: ITEM_SWORD_KOKIRI, ITEM_SWORD_RAZOR, ITEM_SWORD_GILDED, ITEM_SWORD_GREAT_FAIRY, ITEM_SHIELD_HERO,
// ITEM_SHIELD_MIRROR. Owned by Termina now.
s32 MergeShip_TerminaEquip_Owns(u8 item);
// The one in use: MM's sword on B / MM's shield (no Ocarina of Time sword or shield instead), the Great Fairy's Sword
// on B.
s32 MergeShip_TerminaEquip_IsEquipped(u8 item);
// It can be equipped now (owned; a sword by the human only; the Great Fairy's Sword with "Ocarina of Time Equipment").
s32 MergeShip_TerminaEquip_CanEquip(u8 item);
// A on it: it is equipped (see the top of the file). False, changing nothing, when it can't be now.
s32 MergeShip_TerminaEquip_Equip(struct PlayState* play, u8 item);

// Every frame (the player's update) and on a give: keeps the record in step with MM's own events.
void MergeShip_TerminaEquip_Observe(void);
void MergeShip_TerminaEquip_OnItemGiven(u8 item);
void MergeShip_TerminaEquip_BeforeItemGiven(u8 item); // ShouldItemGive: the sword / shield in use before a give
// A file was loaded or created: what the last frame saw belongs to another file.
void MergeShip_TerminaEquip_ResetObserver(void);
// BeforeEndOfCycleSave: the Razor Sword does not survive the Song of Time (MM), unless kept (see above).
void MergeShip_TerminaEquip_OnEndOfCycle(void);

// ---- the shared save (MergeShipTerminaEquip.c; mergeship.exe only, MERGESHIP_HOSTED) ---------------------------------
// MergeShipShared_Capture, last: Termina's record and the sword and shield in use.
void MergeShip_TerminaEquip_CaptureShared(struct MsSharedSave* shared);
// MergeShipShared_Apply, after its equipment: the record, and the sword and shield Termina had in use -- unless the
// apply just equipped one Hyrule added since that capture.
void MergeShip_TerminaEquip_ApplyShared(const struct MsSharedSave* shared);
// BUG-0120: the sword on B, carried between the worlds (MsSharedSave.bSword). Capture: Termina's (MergeShipShared_Capture);
// apply: Hyrule's, put on B if Termina holds it (MergeShipShared_Apply, last), once.
void MergeShip_TerminaEquip_CaptureBSword(struct MsSharedSave* shared);
void MergeShip_TerminaEquip_ApplyBSword(const struct MsSharedSave* shared);

// ---- pause (MergeShipTerminaEquipPause.c) ------------------------------------------------------------------------------
s32 MergeShip_TerminaEquipPause_IsOpen(void);
// The Quest face switches to the view (C-Up, z_kaleido_mergeship.c): the cursor on the sword in use.
void MergeShip_TerminaEquipPause_Open(struct PlayState* play);
void MergeShip_TerminaEquipPause_Close(void);
// z_kaleido_collect.c: true while the view replaces the Quest page (its drawing / its cursor and A).
s32 MergeShip_TerminaEquipPause_DrawQuestPage(struct PlayState* play);
s32 MergeShip_TerminaEquipPause_UpdateQuestCursor(struct PlayState* play);

#ifdef __cplusplus
}
#endif
