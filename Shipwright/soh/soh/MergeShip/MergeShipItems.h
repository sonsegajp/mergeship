#pragma once

// MergeShip2 -- Majora's Mask's rules for what each form may do with its item buttons.
//
// MM keeps them in z_parameter.c: gPlayerFormItemRestrictions (which C-button items a form may use),
// Interface_UpdateButtonsPart2 (the B button per form, Deku flight, water, ledges) and the Fierce
// Deity's Mask scene rule. Ocarina of Time's func_80083108 hands the buttons to
// MergeShip_UpdateButtonsMM while the player is an MM form; see MergeShipItems.c for how each Hyrule
// item is mapped onto MM's table.

#include "z64.h"
#include "soh/MergeShip/MergeShipForms.h"

#ifdef __cplusplus
extern "C" {
#endif

// MM gPlayerFormItemRestrictions[form][item] for a C/D-pad item. `form` is a PlayerForm; the two human
// ages always answer true (their items follow Ocarina of Time's age rules instead).
s32 MergeShip_FormCanUseItem(u8 form, u8 item);

// MM Interface_UpdateButtonsPart2's Fierce Deity's Mask case: usable only in a boss's lair.
s32 MergeShip_FierceDeityMaskAllowedHere(PlayState* play);
// MM's Giant's Mask scene rule (Twinmold's lair only), see MergeShipItems.c.
s32 MergeShip_GiantsMaskAllowedHere(PlayState* play);

// MM Player_GetEnvironmentalHazard for an MM form (the Zora is never "underwater", only on the floor
// in its underwater boots); Ocarina of Time's for the human ages.
s32 MergeShip_GetEnvironmentalHazard(PlayState* play);

// MM Interface_UpdateButtonsPart2 (from its Deku-flight case on) for an MM form. Returns false for the
// human ages, which keep Ocarina of Time's logic. `restoreHud` is func_80083108's "sp28".
s32 MergeShip_UpdateButtonsMM(PlayState* play, s16* restoreHud);

// For the human ages: MM's "masks" restriction class for the transformation masks, which Ocarina of
// Time's scene restrictions know nothing about (enabled, except the Fierce Deity's Mask outside a lair).
void MergeShip_UpdateTransformationMaskButtons(PlayState* play, s16* restoreHud);

// How Interface_Draw should draw the B button (MM Interface_DrawBButtonIcons for MM forms).
typedef enum MergeShipBButtonDraw {
    MERGESHIP_BDRAW_OOT,       // Ocarina of Time's item icon / label logic
    MERGESHIP_BDRAW_ICON_AMMO, // the B item with its ammo count (MM: Deku flying, nuts on B)
    MERGESHIP_BDRAW_NOTHING,   // a Goron/Zora/Deku B with no label draws nothing
} MergeShipBButtonDraw;
s32 MergeShip_GetBButtonDrawMode(PlayState* play);

// MM Interface_Update: Deku Link's B becomes Deku nuts once magic is acquired.
void MergeShip_UpdateDekuBButtonForMagic(void);

// Inventory_SwapAgeEquipment brackets: an MM form's B button is parked while OoT swaps the human's
// age equipment, so the adult/child equip sets never receive a form's B item.
void MergeShip_BeginAgeSwap(void);
void MergeShip_EndAgeSwap(void);

#ifdef __cplusplus
}
#endif
