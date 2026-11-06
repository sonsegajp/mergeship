// MergeShip2 -- Majora's Mask's equipment in Ocarina of Time: ownership, the variant each shared slot is switched to,
// the pause screen (Ocarina of Time's Equipment page, the Termina view, the Items page's A toggle), age rules, icons and
// names (see MergeShipMMEquipment.h). The player's use of it is in
// MergeShipMMEquipment.inc (z_player.c), its drawing in MergeShipMMEquipmentDraw.inc (z_player_lib.c), the SoH hooks
// in MergeShipMMEquipment.cpp.

#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipDekuBeans.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "mm/archives/item_name_static/item_name_static.h"
#include "mm/archives/icon_item_static/icon_item_static_yar.h" // MM's bow with its magic arrows (BUG-0148)
#include "soh/MergeShip/MergeShipOwnership.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/OTRGlobals.h" // Randomizer_GetSettingValue (the seed's Bombchu Bag mode)
#include <mergeship/SharedSave.h>
#include <mergeship/MsDelivery.h> // MsDelivery_BottleOnePerWorld

#define MMEQUIP_BIT(id) (1 << (id))
// A bottle content only Majora's Mask has (Chateau Romani, Deku Princess .. Hot Spring Water): MM's bottle.
#define MSOWN_IS_MM_BOTTLE_CONTENT(item) \
    (((item) == ITEM_MM_CHATEAU) || (((item) >= ITEM_MM_DEKU_PRINCESS) && ((item) <= ITEM_MM_HOT_SPRING_WATER)))

extern u8 gEquipAgeReqs[][4]; // z_kaleido_scope_PAL.c

void MergeShipTest_Log(const char* fmt, ...); // MergeShipTest.cpp (no-op outside the harness)

// The MM items of each slot (-1: none), after the Ocarina of Time item.
#define MMEQUIP_SLOT_VARIANTS 3
static const s8 sSlotVariants[MMEQUIP_SLOT_MAX][MMEQUIP_SLOT_VARIANTS] = {
    { MMEQUIP_RAZOR, MMEQUIP_GILDED, MMEQUIP_KOKIRI }, // MMEQUIP_SLOT_KOKIRI_SWORD (MM's own Kokiri Sword too, 2026-09-24)
    { MMEQUIP_GREAT_FAIRY, -1, -1 },                   // MMEQUIP_SLOT_BIGGORON
    { MMEQUIP_HERO_SHIELD, -1, -1 },                   // MMEQUIP_SLOT_HYLIAN
    { MMEQUIP_MIRROR_SHIELD, -1, -1 },                 // MMEQUIP_SLOT_MIRROR
    { MMEQUIP_HOOKSHOT, -1, -1 },                      // MMEQUIP_SLOT_HOOKSHOT
    { MMEQUIP_BOW, -1, -1 },                           // MMEQUIP_SLOT_BOW
};

// The slot each MM item shares.
static const u8 sItemSlot[MMEQUIP_MAX] = {
    MMEQUIP_SLOT_KOKIRI_SWORD, MMEQUIP_SLOT_KOKIRI_SWORD, MMEQUIP_SLOT_BIGGORON, MMEQUIP_SLOT_HYLIAN,
    MMEQUIP_SLOT_MIRROR,       MMEQUIP_SLOT_HOOKSHOT,     MMEQUIP_SLOT_BOW,     MMEQUIP_SLOT_KOKIRI_SWORD,
};

// Slots whose selected MM variant was taken by an enemy (a Like Like eats the shield in use; OoT's
// Inventory_DeleteEquipment). The variant comes back with the shield it drops. Not saved: a reload returns it.
static u8 sTakenSlots = 0;

s32 MergeShip_MMEquip_IsEnabled(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMEquipment"), 1) && MergeShip_IsMajorasMaskMounted();
}

s32 MergeShip_MMEquip_Owns(s32 id) {
    return (id >= 0) && (id < MMEQUIP_MAX) && (gMergeShipSave.mmEquipOwned & MMEQUIP_BIT(id));
}

static s32 MergeShip_MMEquip_SlotMask(s32 slot) {
    s32 mask = 0;
    s32 i;

    for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
        if (sSlotVariants[slot][i] >= 0) {
            mask |= MMEQUIP_BIT(sSlotVariants[slot][i]);
        }
    }
    return mask;
}

// The slot's Ocarina of Time bit / inventory item is there (whoever it belongs to).
static s32 MergeShip_MMEquip_SlotPresent(s32 slot) {
    switch (slot) {
        case MMEQUIP_SLOT_KOKIRI_SWORD:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) != 0;
        case MMEQUIP_SLOT_BIGGORON:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON) != 0;
        case MMEQUIP_SLOT_HYLIAN:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN) != 0;
        case MMEQUIP_SLOT_MIRROR:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR) != 0;
        case MMEQUIP_SLOT_HOOKSHOT:
            return (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT) || (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_LONGSHOT);
        case MMEQUIP_SLOT_BOW:
            return INV_CONTENT(ITEM_BOW) == ITEM_BOW;
        default:
            return false;
    }
}

// Puts the slot's Ocarina of Time item in place for an MM variant to occupy it (as Ocarina of Time's Item_Give would).
static void MergeShip_MMEquip_FillSlot(s32 slot) {
    switch (slot) {
        case MMEQUIP_SLOT_KOKIRI_SWORD:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            break;
        case MMEQUIP_SLOT_BIGGORON:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
            break;
        case MMEQUIP_SLOT_HYLIAN:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN);
            break;
        case MMEQUIP_SLOT_MIRROR:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
            break;
        case MMEQUIP_SLOT_HOOKSHOT:
            INV_CONTENT(ITEM_HOOKSHOT) = ITEM_HOOKSHOT;
            break;
        case MMEQUIP_SLOT_BOW:
            // MM Item_Give(ITEM_BOW), as Ocarina of Time's: the first quiver, full.
            if (CUR_UPG_VALUE(UPG_QUIVER) == 0) {
                Inventory_ChangeUpgrade(UPG_QUIVER, 1);
                AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 1);
            }
            INV_CONTENT(ITEM_BOW) = ITEM_BOW;
            break;
        default:
            break;
    }
    gMergeShipSave.mmEquipSlotOnlyMM |= MMEQUIP_BIT(slot);
}

// Empties a slot held only for MM variants that are gone.
static void MergeShip_MMEquip_ClearSlot(s32 slot) {
    s32 i;

    switch (slot) {
        case MMEQUIP_SLOT_KOKIRI_SWORD:
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
            }
            break;
        case MMEQUIP_SLOT_BIGGORON:
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
            if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
            }
            break;
        case MMEQUIP_SLOT_HYLIAN:
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN);
            if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HYLIAN) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
            }
            break;
        case MMEQUIP_SLOT_MIRROR:
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
            if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
            }
            break;
        case MMEQUIP_SLOT_HOOKSHOT:
        case MMEQUIP_SLOT_BOW: {
            u8 item = (slot == MMEQUIP_SLOT_BOW) ? ITEM_BOW : INV_CONTENT(ITEM_HOOKSHOT);

            for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                if (gSaveContext.equips.buttonItems[i] == item) {
                    gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                    gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
                }
            }
            if (slot == MMEQUIP_SLOT_BOW) {
                INV_CONTENT(ITEM_BOW) = ITEM_NONE;
            } else {
                INV_CONTENT(ITEM_HOOKSHOT) = ITEM_NONE;
            }
            break;
        }
        default:
            break;
    }
    gMergeShipSave.mmEquipSlotOnlyMM &= ~MMEQUIP_BIT(slot);
}

s32 MergeShip_MMEquip_OoTItemOwned(s32 slot) {
    return MergeShip_MMEquip_SlotPresent(slot) && !(gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(slot));
}

u8 MergeShip_MMEquip_OoTPageSlotItem(s32 invSlot) {
    u8 item;

    // The user's rule: MM's items are on the MM item menu only. A Bow / Hookshot slot held only for the Hero's Bow or
    // MM's Hookshot is empty on Ocarina of Time's page.
    if (((invSlot == SLOT_BOW) && (gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(MMEQUIP_SLOT_BOW))) ||
        ((invSlot == SLOT_HOOKSHOT) && (gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(MMEQUIP_SLOT_HOOKSHOT)))) {
        return ITEM_NONE;
    }
    item = gSaveContext.inventory.items[invSlot];
    // So are MM's bombs, bombchus, sticks, nuts and beans held in Ocarina of Time's slot (MergeShipOwnership.h), and a
    // bottle holding one of MM's own contents.
    switch (invSlot) {
        case SLOT_BOMB:
        case SLOT_BOMBCHU:
        case SLOT_STICK:
        case SLOT_NUT:
            return ((item != ITEM_NONE) && !MergeShip_Own_OoTOwns(item)) ? ITEM_NONE : item;
        case SLOT_BEAN:
            // BUG-0068: Termina's Magic Beans in the slot, and the Deku Beans: the slot is the Deku Beans'.
            if ((item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected()) {
                return ITEM_BEAN;
            }
            return ((item != ITEM_NONE) && !MergeShip_Own_OoTOwns(item)) ? ITEM_NONE : item;
        case SLOT_BOTTLE_1:
        case SLOT_BOTTLE_2:
        case SLOT_BOTTLE_3:
        case SLOT_BOTTLE_4:
            // Majora's Mask's bottles are the shared save's (MergeShip_Own_MMBottlesHere): these four are Ocarina of
            // Time's, whatever they hold. (soh.exe, or combined: MM's contents are on the MM item menu, as before.)
            if (MergeShip_Own_MMBottlesHere()) {
                return MergeShip_Own_HyruleBottleIsMM(invSlot - SLOT_BOTTLE_1) ? ITEM_NONE : item;
            }
            return (MSOWN_IS_MM_BOTTLE_CONTENT(item) || MergeShip_Own_HyruleBottleIsMM(invSlot - SLOT_BOTTLE_1))
                       ? ITEM_NONE
                       : item;
        default:
            return item;
    }
}

// The MM item can be used now: owned, not taken, its slot there, the feature on.
static s32 MergeShip_MMEquip_Usable(s32 id) {
    s32 slot;

    if (!MergeShip_MMEquip_IsEnabled() || !MergeShip_MMEquip_Owns(id)) {
        return false;
    }
    slot = sItemSlot[id];
    return !(sTakenSlots & MMEQUIP_BIT(slot)) && MergeShip_MMEquip_SlotPresent(slot);
}

// BUG-0077: while an Anchor teammate's dummy is drawn, the MM equipment in use is his (MergeShipAnchor.cpp).
static s32 sDrawOverride = false;
static u32 sDrawOverrideBits = 0;

s32 MergeShip_MMEquip_IsSelected(s32 id) {
    if (sDrawOverride) {
        return (id >= 0) && (id < MMEQUIP_MAX) && (sDrawOverrideBits & MMEQUIP_BIT(id));
    }
    return MergeShip_MMEquip_Usable(id) && (gMergeShipSave.mmEquipSelected & MMEQUIP_BIT(id));
}

u32 MergeShip_MMEquip_SelectedBits(void) {
    u32 bits = 0;
    s32 id;

    for (id = 0; id < MMEQUIP_MAX; id++) {
        if (MergeShip_MMEquip_IsSelected(id)) {
            bits |= MMEQUIP_BIT(id);
        }
    }
    return bits;
}

void MergeShip_MMEquip_SetDrawOverride(s32 on, u32 bits) {
    sDrawOverride = on;
    sDrawOverrideBits = bits;
}

s32 MergeShip_MMEquip_DrawOverrideActive(void) {
    return sDrawOverride;
}

// The slot's selected variant: -1 for the Ocarina of Time item, else the MM item.
static s32 MergeShip_MMEquip_SlotSelection(s32 slot) {
    s32 i;

    for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
        if ((sSlotVariants[slot][i] >= 0) && MergeShip_MMEquip_IsSelected(sSlotVariants[slot][i])) {
            return sSlotVariants[slot][i];
        }
    }
    return -1;
}

static void MergeShip_MMEquip_Select(s32 slot, s32 id) {
    gMergeShipSave.mmEquipSelected &= ~MergeShip_MMEquip_SlotMask(slot);
    if (id >= 0) {
        gMergeShipSave.mmEquipSelected |= MMEQUIP_BIT(id);
    }
}

// The variants the slot can be switched to, in order (-1: Ocarina of Time's). Returns their count. (The Items page's
// Bow / Hookshot toggle.)
static s32 MergeShip_MMEquip_SlotChoices(s32 slot, s32* choices) {
    s32 count = 0;
    s32 i;

    if (MergeShip_MMEquip_OoTItemOwned(slot)) {
        choices[count++] = -1;
    }
    for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
        if ((sSlotVariants[slot][i] >= 0) && MergeShip_MMEquip_Usable(sSlotVariants[slot][i])) {
            choices[count++] = sSlotVariants[slot][i];
        }
    }
    return count;
}

// The item the B button holds for a sword slot's variant.
static u8 MergeShip_MMEquip_SwordItem(s32 slot, s32 id) {
    if (id >= 0) {
        return MERGESHIP_MMEQUIP_ITEM(id);
    }
    if (slot == MMEQUIP_SLOT_BIGGORON) {
        return ((gSaveContext.bgsFlag == 0) &&
                CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                   ? ITEM_SWORD_KNIFE
                   : ITEM_SWORD_BGS;
    }
    return ITEM_SWORD_KOKIRI;
}

static s32 MergeShip_MMEquip_SlotFromEquip(s32 equipType, s32 equipValue) {
    if (equipType == EQUIP_TYPE_SWORD) {
        if (equipValue == EQUIP_VALUE_SWORD_KOKIRI) {
            return MMEQUIP_SLOT_KOKIRI_SWORD;
        }
        if (equipValue == EQUIP_VALUE_SWORD_BIGGORON) {
            return MMEQUIP_SLOT_BIGGORON;
        }
    } else if (equipType == EQUIP_TYPE_SHIELD) {
        if (equipValue == EQUIP_VALUE_SHIELD_HYLIAN) {
            return MMEQUIP_SLOT_HYLIAN;
        }
        if (equipValue == EQUIP_VALUE_SHIELD_MIRROR) {
            return MMEQUIP_SLOT_MIRROR;
        }
    }
    return -1;
}

void MergeShip_MMEquip_Sanitize(PlayState* play) {
    s32 slot;
    s32 i;
    u8 b;
    u8 newB;

    MergeShip_Own_Sync(); // Majora's Mask's bombs .. beans and bags held in Ocarina of Time's slots

    if (!MergeShip_MMEquip_IsEnabled()) {
        // Off: an MM sword on B becomes its Ocarina of Time partner (MM variants are not drawn or used).
        b = gSaveContext.equips.buttonItems[0];
        if (MERGESHIP_ITEM_IS_MM_SWORD(b) && !MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
            slot = (b == ITEM_MM_SWORD_GREAT_FAIRY) ? MMEQUIP_SLOT_BIGGORON : MMEQUIP_SLOT_KOKIRI_SWORD;
            gSaveContext.equips.buttonItems[0] =
                MergeShip_MMEquip_SlotPresent(slot) ? MergeShip_MMEquip_SwordItem(slot, -1) : ITEM_NONE;
            if (play != NULL) {
                Interface_LoadItemIcon1(play, 0);
            }
        }
        return;
    }

    for (slot = 0; slot < MMEQUIP_SLOT_MAX; slot++) {
        s32 mmOwned = gMergeShipSave.mmEquipOwned & MergeShip_MMEquip_SlotMask(slot);
        s32 onlyMM = (gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(slot)) != 0;
        s32 taken = (sTakenSlots & MMEQUIP_BIT(slot)) != 0;

        if (onlyMM && !mmOwned) {
            // Its MM variants are gone: nothing holds the slot any more.
            if (MergeShip_MMEquip_SlotPresent(slot)) {
                MergeShip_MMEquip_ClearSlot(slot);
            }
            gMergeShipSave.mmEquipSlotOnlyMM &= ~MMEQUIP_BIT(slot);
        } else if (mmOwned && !taken && !MergeShip_MMEquip_SlotPresent(slot)) {
            // An MM variant is owned but its slot is empty (the Ocarina of Time item was taken, or the save predates
            // the slot being held): the MM variant holds it.
            MergeShip_MMEquip_FillSlot(slot);
        }

        // The selection names a usable variant, and a slot held only for MM variants uses one of them.
        for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
            s32 id = sSlotVariants[slot][i];

            if ((id >= 0) && (gMergeShipSave.mmEquipSelected & MMEQUIP_BIT(id)) && !MergeShip_MMEquip_Usable(id) &&
                !taken) {
                gMergeShipSave.mmEquipSelected &= ~MMEQUIP_BIT(id);
            }
        }
        if ((gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(slot)) && (MergeShip_MMEquip_SlotSelection(slot) < 0)) {
            for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
                if ((sSlotVariants[slot][i] >= 0) && MergeShip_MMEquip_Usable(sSlotVariants[slot][i])) {
                    MergeShip_MMEquip_Select(slot, sSlotVariants[slot][i]);
                    break;
                }
            }
        }
    }

    // The B button follows the sword slots' selections (a transformed Link's B is his form's own).
    if (MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
        return;
    }
    b = gSaveContext.equips.buttonItems[0];
    newB = b;
    if ((b == ITEM_SWORD_KOKIRI) || (b == ITEM_MM_SWORD_RAZOR) || (b == ITEM_MM_SWORD_GILDED) ||
        (b == ITEM_MM_SWORD_KOKIRI)) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI) {
            if (MergeShip_MMEquip_SlotPresent(MMEQUIP_SLOT_KOKIRI_SWORD)) {
                newB = MergeShip_MMEquip_SwordItem(MMEQUIP_SLOT_KOKIRI_SWORD,
                                                   MergeShip_MMEquip_SlotSelection(MMEQUIP_SLOT_KOKIRI_SWORD));
            }
        } else if (b != ITEM_SWORD_KOKIRI) {
            // An MM sword on B is always its slot's sword: equipped, or gone.
            if (MergeShip_MMEquip_Usable(MERGESHIP_MMEQUIP_ID(b))) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            } else {
                newB = ITEM_NONE;
            }
        }
    } else if ((b == ITEM_SWORD_BGS) || (b == ITEM_SWORD_KNIFE) || (b == ITEM_MM_SWORD_GREAT_FAIRY)) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON) {
            if (MergeShip_MMEquip_SlotPresent(MMEQUIP_SLOT_BIGGORON)) {
                s32 sel = MergeShip_MMEquip_SlotSelection(MMEQUIP_SLOT_BIGGORON);

                newB = (sel >= 0) ? ITEM_MM_SWORD_GREAT_FAIRY
                                  : ((b == ITEM_MM_SWORD_GREAT_FAIRY) ? MergeShip_MMEquip_SwordItem(
                                                                            MMEQUIP_SLOT_BIGGORON, -1)
                                                                      : b);
            }
        } else if (b == ITEM_MM_SWORD_GREAT_FAIRY) {
            if (MergeShip_MMEquip_Usable(MMEQUIP_GREAT_FAIRY)) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_BIGGORON);
            } else {
                newB = ITEM_NONE;
            }
        }
    }
    if (newB != b) {
        gSaveContext.equips.buttonItems[0] = newB;
        if (play != NULL) {
            Interface_LoadItemIcon1(play, 0);
        }
    }
    // BUG-0046: an MM sword on B is a sword -- not the "swordless" state a file with no Ocarina of Time sword keeps.
    if (MERGESHIP_ITEM_IS_MM_SWORD(gSaveContext.equips.buttonItems[0]) && Flags_GetInfTable(INFTABLE_SWORDLESS)) {
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
    }
}

// ---- getting the items ------------------------------------------------------------------------------------------

// MM Item_Give for the seven items. MM equips a new sword on B, and a new shield; here a sword is equipped when the
// Kokiri Sword slot (or nothing) is in use, so adult Link keeps a Master Sword he is holding, and a shield when none
// is worn. Each new item becomes its slot's selected variant.
void MergeShip_MMEquip_Give(PlayState* play, u8 item) {
    s32 id = (item == ITEM_NONE) ? -1 : MERGESHIP_MMEQUIP_ID(item);
    s32 slot;
    s32 hadSlot;

    if ((id < 0) || (id >= MMEQUIP_MAX)) {
        return;
    }
    slot = sItemSlot[id];
    hadSlot = MergeShip_MMEquip_SlotPresent(slot);
    gMergeShipSave.mmEquipOwned |= MMEQUIP_BIT(id);
    sTakenSlots &= ~MMEQUIP_BIT(slot);
    if (!hadSlot) {
        MergeShip_MMEquip_FillSlot(slot);
    }
    if (item == ITEM_MM_SWORD_RAZOR) {
        gMergeShipSave.mmRazorHealth = 100; // MM Item_Give: swordHealth = 100
    }
    if (item == ITEM_MM_BOW_HERO) {
        // MM's Hero's Bow comes with MM's quiver (Item_Give(ITEM_BOW): CUR_UPG_VALUE(UPG_QUIVER) 1), Termina's own,
        // full (AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER)).
        MergeShip_Own_GiveMM(MS_OWN_BOW);
        MergeShip_Own_RaiseMMUpgrade(UPG_QUIVER, 1);
        MergeShip_Own_FillMM(MS_OWN_BOW);
    }

    if (!MergeShip_MMEquip_IsEnabled()) {
        return;
    }
    // The Hero's Bow and MM's Hookshot are separate items on the Items (Termina) page (ITEM_MM_BOW_HERO,
    // ITEM_MM_HOOKSHOT): getting one never selects it, whatever "Don't Auto-Equip New Swords and Shields" says (the
    // reported bug: obtaining MM's Hookshot while Ocarina of Time's own is already owned swapped the Items page
    // straight to MM's icon). Selection follows use (MergeShip_MMEquip_OnUseItem), not acquisition; Sanitize still
    // picks it for a slot held for MM's alone.
    if ((item == ITEM_MM_HOOKSHOT) || (item == ITEM_MM_BOW_HERO)) {
        MergeShip_MMEquip_Sanitize(play);
        return;
    }
    // A new sword or shield is owned, not put in use (the user, 2026-09-25: "dont auto equip swords/shields when u get
    // them"): its slot keeps the variant it had; Sanitize picks it only for a slot held for MM variants alone.
    if (((item == ITEM_MM_SWORD_RAZOR) || (item == ITEM_MM_SWORD_GILDED) || (item == ITEM_MM_SWORD_KOKIRI) ||
         (item == ITEM_MM_SWORD_GREAT_FAIRY) || (item == ITEM_MM_SHIELD_HERO) || (item == ITEM_MM_SHIELD_MIRROR)) &&
        CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.NoAutoEquipGear"), 1)) {
        MergeShip_MMEquip_Sanitize(play);
        return;
    }
    MergeShip_MMEquip_Select(slot, id);

    if ((item == ITEM_MM_SWORD_RAZOR) || (item == ITEM_MM_SWORD_GILDED) || (item == ITEM_MM_SWORD_KOKIRI)) {
        // MM: CUR_FORM_EQUIP(EQUIP_SLOT_B) = item, SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, ...).
        if (((CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE) ||
             (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI)) &&
            !MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            gSaveContext.equips.buttonItems[0] = item;
            Flags_UnsetInfTable(INFTABLE_SWORDLESS);
            if (play != NULL) {
                Interface_LoadItemIcon1(play, 0);
            }
        }
    } else if ((item == ITEM_MM_SHIELD_HERO) || (item == ITEM_MM_SHIELD_MIRROR)) {
        // MM: SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, ...) and Player_SetEquipmentData.
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_NONE) {
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, (item == ITEM_MM_SHIELD_HERO) ? EQUIP_VALUE_SHIELD_HYLIAN
                                                                                        : EQUIP_VALUE_SHIELD_MIRROR);
            if ((play != NULL) && (GET_PLAYER(play) != NULL)) {
                Player_SetEquipmentData(play, GET_PLAYER(play));
            }
        }
    }
    MergeShip_MMEquip_Sanitize(play);
}

void MergeShip_MMEquip_OnOoTItemGiven(u8 item) {
    s32 slot;

    MergeShip_Own_OnOoTItemGiven(item); // bombs .. beans, bags, bow, ocarina: Ocarina of Time's own

    switch (item) {
        case ITEM_SWORD_KOKIRI:
            slot = MMEQUIP_SLOT_KOKIRI_SWORD;
            break;
        case ITEM_SWORD_BGS:
            slot = MMEQUIP_SLOT_BIGGORON;
            break;
        case ITEM_SHIELD_HYLIAN:
            slot = MMEQUIP_SLOT_HYLIAN;
            break;
        case ITEM_SHIELD_MIRROR:
            slot = MMEQUIP_SLOT_MIRROR;
            break;
        case ITEM_HOOKSHOT:
        case ITEM_LONGSHOT:
            slot = MMEQUIP_SLOT_HOOKSHOT;
            break;
        case ITEM_BOW:
            slot = MMEQUIP_SLOT_BOW;
            break;
        default:
            return;
    }
    if (sTakenSlots & MMEQUIP_BIT(slot)) {
        // The shield an enemy took and dropped again is the MM one it ate.
        sTakenSlots &= ~MMEQUIP_BIT(slot);
        return;
    }
    if ((gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(slot)) &&
        ((slot == MMEQUIP_SLOT_HOOKSHOT) || (slot == MMEQUIP_SLOT_BOW))) {
        // The slot was held for MM's Hookshot / Hero's Bow alone, so Sanitize had selected it: Ocarina of Time's own
        // now fills it and is the one its slot uses (MM's is a separate item, ITEM_MM_HOOKSHOT / ITEM_MM_BOW_HERO,
        // selected again when used, MergeShip_MMEquip_OnUseItem). Swords and shields keep theirs (no auto-equip).
        MergeShip_MMEquip_Select(slot, -1);
    }
    gMergeShipSave.mmEquipSlotOnlyMM &= ~MMEQUIP_BIT(slot);
}

// Ocarina of Time's Inventory_DeleteEquipment took the equipped sword or shield (GameInteractor OnEquipmentDelete).
void MergeShip_MMEquip_OnEquipmentDelete(s32 equipType, s32 equipValue) {
    s32 slot = MergeShip_MMEquip_SlotFromEquip(equipType, equipValue);
    s32 sel = -1;
    s32 i;

    if ((slot < 0) || !MergeShip_MMEquip_IsEnabled()) {
        return;
    }
    // The bit is already gone; the variant that was in use is the one lost.
    for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
        s32 id = sSlotVariants[slot][i];

        if ((id >= 0) && (gMergeShipSave.mmEquipOwned & gMergeShipSave.mmEquipSelected & MMEQUIP_BIT(id))) {
            sel = id;
        }
    }
    if (sel >= 0) {
        // The MM variant was taken. An owned Ocarina of Time item is still there (unequipped, as the theft left it).
        sTakenSlots |= MMEQUIP_BIT(slot);
        if (!(gMergeShipSave.mmEquipSlotOnlyMM & MMEQUIP_BIT(slot))) {
            MergeShip_MMEquip_FillSlot(slot);
            gMergeShipSave.mmEquipSlotOnlyMM &= ~MMEQUIP_BIT(slot);
            MergeShip_MMEquip_Select(slot, -1);
        }
    } else if (gMergeShipSave.mmEquipOwned & MergeShip_MMEquip_SlotMask(slot)) {
        // Ocarina of Time's item was taken; the MM variant stays and now holds the slot.
        MergeShip_MMEquip_FillSlot(slot);
        for (i = 0; i < MMEQUIP_SLOT_VARIANTS; i++) {
            if ((sSlotVariants[slot][i] >= 0) && MergeShip_MMEquip_Owns(sSlotVariants[slot][i])) {
                MergeShip_MMEquip_Select(slot, sSlotVariants[slot][i]);
                break;
            }
        }
    }
}

s8 MergeShip_MMEquip_ItemToItemAction(s32 item) {
    switch (item) {
        case ITEM_MM_SWORD_RAZOR:
            return PLAYER_IA_MM_SWORD_RAZOR;
        case ITEM_MM_SWORD_KOKIRI:
            return PLAYER_IA_SWORD_KOKIRI; // the same sword as Ocarina of Time's, drawn as MM's (MMEquipmentDraw.inc)
        case ITEM_MM_SWORD_GILDED:
            return PLAYER_IA_MM_SWORD_GILDED;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return PLAYER_IA_MM_SWORD_GREAT_FAIRY;
        case ITEM_MM_HOOKSHOT:
            return PLAYER_IA_HOOKSHOT;
        case ITEM_MM_BOW_HERO:
            return PLAYER_IA_BOW;
        default:
            return PLAYER_IA_NONE;
    }
}

// ---- age rules --------------------------------------------------------------------------------------------------

static s32 MergeShip_MMEquip_Timeless(void) {
    return MergeShip_MMEquip_IsEnabled() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TimelessMMGear"), 0);
}

s32 MergeShip_MMEquip_EquipSlotIgnoresAge(s32 equipType, s32 equipValue) {
    s32 slot = MergeShip_MMEquip_SlotFromEquip(equipType, equipValue);

    // "Young Link Uses the Mirror Shield": Ocarina of Time's Mirror Shield at either age.
    if ((equipType == EQUIP_TYPE_SHIELD) && (equipValue == EQUIP_VALUE_SHIELD_MIRROR) &&
        CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ChildMirrorShield"), 0)) {
        return true;
    }
    // BUG-0142 ("have young link be able to use tunics"): "Young Link Wears Every Tunic" -- the Goron and Zora Tunics
    // at either age, as young Link wears them in Termina.
    if ((equipType == EQUIP_TYPE_TUNIC) &&
        ((equipValue == EQUIP_VALUE_TUNIC_GORON) || (equipValue == EQUIP_VALUE_TUNIC_ZORA)) &&
        CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ChildTunics"), 1)) {
        return true;
    }
    if (!MergeShip_MMEquip_IsEnabled() || ((equipType != EQUIP_TYPE_SWORD) && (equipType != EQUIP_TYPE_SHIELD))) {
        return false;
    }
    // The page shows and equips only Ocarina of Time's own items, so their own age rules hold (an MM variant the slot
    // is switched to does not lift them: A there equips the Ocarina of Time item).
    // TimelessMMGear: the swords and shields with MM partners, and the Master Sword between them.
    return MergeShip_MMEquip_Timeless() &&
           ((slot >= 0) || ((equipType == EQUIP_TYPE_SWORD) && (equipValue == EQUIP_VALUE_SWORD_MASTER)));
}

s32 MergeShip_MMEquip_ItemIgnoresAge(s32 item) {
    // BUG-0142: "Young Link Wears Every Tunic" (the D-pad's tunics too).
    if (((item == ITEM_TUNIC_GORON) || (item == ITEM_TUNIC_ZORA)) &&
        CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.ChildTunics"), 1)) {
        return true;
    }
    if (!MergeShip_MMEquip_IsEnabled()) {
        return false;
    }
    switch (item) {
        // The Hero's Bow, and MM's human shoots magic arrows from it.
        case ITEM_BOW:
        case ITEM_BOW_ARROW_FIRE:
        case ITEM_BOW_ARROW_ICE:
        case ITEM_BOW_ARROW_LIGHT:
        case ITEM_ARROW_FIRE:
        case ITEM_ARROW_ICE:
        case ITEM_ARROW_LIGHT:
            return MergeShip_MMEquip_IsSelected(MMEQUIP_BOW);
        case ITEM_HOOKSHOT:
        case ITEM_LONGSHOT:
            return MergeShip_MMEquip_IsSelected(MMEQUIP_HOOKSHOT);
        case ITEM_SWORD_KOKIRI:
        case ITEM_SWORD_MASTER:
        case ITEM_SWORD_BGS:
        case ITEM_SHIELD_HYLIAN:
        case ITEM_SHIELD_MIRROR:
            return MergeShip_MMEquip_Timeless();
        default:
            return false;
    }
}

s32 MergeShip_MMEquip_InvSlotIgnoresAge(s32 slot) {
    if (!MergeShip_MMEquip_IsEnabled()) {
        return false;
    }
    switch (slot) {
        case SLOT_BOW:
        case SLOT_ARROW_FIRE:
        case SLOT_ARROW_ICE:
        case SLOT_ARROW_LIGHT:
            return MergeShip_MMEquip_IsSelected(MMEQUIP_BOW);
        case SLOT_HOOKSHOT:
            return MergeShip_MMEquip_IsSelected(MMEQUIP_HOOKSHOT);
        default:
            return false;
    }
}

// ---- pause screen -----------------------------------------------------------------------------------------------

u16 MergeShip_MMEquip_PauseSlotItem(s32 equipType, s32 equipValue, u16 ootItem) {
    s32 slot = MergeShip_MMEquip_SlotFromEquip(equipType, equipValue);
    s32 sel;

    if ((slot < 0) || !MergeShip_MMEquip_IsEnabled()) {
        return ootItem;
    }
    sel = MergeShip_MMEquip_SlotSelection(slot);
    return (sel >= 0) ? (u16)MERGESHIP_MMEQUIP_ITEM(sel) : ootItem;
}

s32 MergeShip_MMEquip_PauseSlotHidden(s32 equipType, s32 equipValue) {
    s32 slot = MergeShip_MMEquip_SlotFromEquip(equipType, equipValue);

    // (With the feature off, a slot held for MM gear is Ocarina of Time's item again, as MM's variants are not used.)
    return (slot >= 0) && MergeShip_MMEquip_IsEnabled() && MergeShip_MMEquip_SlotPresent(slot) &&
           !MergeShip_MMEquip_OoTItemOwned(slot);
}

s32 MergeShip_MMEquip_PauseEquippedIsMM(s32 equipType) {
    s32 slot;

    if ((equipType != EQUIP_TYPE_SWORD) && (equipType != EQUIP_TYPE_SHIELD)) {
        return false;
    }
    slot = MergeShip_MMEquip_SlotFromEquip(equipType, CUR_EQUIP_VALUE(equipType));
    return (slot >= 0) && MergeShip_MMEquip_IsEnabled() && (MergeShip_MMEquip_SlotSelection(slot) >= 0);
}

s32 MergeShip_MMEquip_PauseSelectOoT(s32 equipType, s32 equipValue) {
    s32 slot = MergeShip_MMEquip_SlotFromEquip(equipType, equipValue);

    if ((slot < 0) || !MergeShip_MMEquip_IsEnabled() || !MergeShip_MMEquip_OoTItemOwned(slot) ||
        (MergeShip_MMEquip_SlotSelection(slot) < 0)) {
        return false;
    }
    MergeShip_MMEquip_Select(slot, -1);
    MergeShipTest_Log("MMEQUIP ootpage slot=%d back to Ocarina of Time's item (type=%d value=%d)", slot, equipType,
                      equipValue);
    return true;
}

u16 MergeShip_MMEquip_OoTPartnerItem(u16 item) {
    switch (item) {
        case ITEM_MM_SWORD_RAZOR:
        case ITEM_MM_SWORD_GILDED:
        case ITEM_MM_SWORD_KOKIRI:
            return ITEM_SWORD_KOKIRI;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return ITEM_SWORD_BGS;
        case ITEM_MM_SHIELD_HERO:
            return ITEM_SHIELD_HYLIAN;
        case ITEM_MM_SHIELD_MIRROR:
            return ITEM_SHIELD_MIRROR;
        case ITEM_MM_HOOKSHOT:
            return ITEM_HOOKSHOT;
        case ITEM_MM_BOW_HERO:
            return ITEM_BOW;
        default:
            return item;
    }
}

static s32 MergeShip_MMEquip_ItemPageSlot(s32 invSlot) {
    if (invSlot == SLOT_BOW) {
        return MMEQUIP_SLOT_BOW;
    }
    if (invSlot == SLOT_HOOKSHOT) {
        return MMEQUIP_SLOT_HOOKSHOT;
    }
    return -1;
}

s32 MergeShip_MMEquip_ItemSlotCanCycle(s32 invSlot) {
    // The user's rule: no A-press swap to the MM variant any more. MM's Hero's Bow and Hookshot are on the Termina items
    // page (MergeShipMMItems.c), and using an item picks its version (MergeShip_MMEquip_OnUseItem).
    (void)invSlot;
    return false;
}

static s32 sMergeShipItemButton = -1;

void MergeShip_MMEquip_NoteItemButton(s32 button) {
    sMergeShipItemButton = button;
}

s32 MergeShip_MMEquip_ArrowButtonBowIsMM(s32 button) {
    const s32 mmBow = MergeShip_MMEquip_Owns(MMEQUIP_BOW);
    const s32 ootBow = MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BOW);

    if (!MergeShip_MMEquip_IsEnabled() || (button < 1) || (button >= ARRAY_COUNT(gSaveContext.equips.buttonItems))) {
        return false;
    }
    // The Termina items page's arrows land with no slot; Ocarina of Time's page's with the bow's (or their own).
    if (gSaveContext.equips.cButtonSlots[button - 1] == SLOT_NONE) {
        return mmBow || !ootBow;
    }
    return mmBow && !ootBow;
}

void* MergeShip_MMEquip_ArrowButtonIcon(s32 button, void* texture) {
    static const char* sMMBowArrowIcons[] = { mm_gItemIconBowFireTex, mm_gItemIconBowIceTex, mm_gItemIconBowLightTex };
    u8 item;

    if ((button < 1) || (button >= ARRAY_COUNT(gSaveContext.equips.buttonItems))) {
        return texture;
    }
    item = gSaveContext.equips.buttonItems[button];
    if ((item < ITEM_BOW_ARROW_FIRE) || (item > ITEM_BOW_ARROW_LIGHT) || !MergeShip_MMEquip_ArrowButtonBowIsMM(button)) {
        return texture;
    }
    return (void*)sMMBowArrowIcons[item - ITEM_BOW_ARROW_FIRE];
}

// Player_UseItem: the version of the Bow / Hookshot slot follows the item used -- MM's from the Termina items page
// (ITEM_MM_BOW_HERO, ITEM_MM_HOOKSHOT), Ocarina of Time's from its own slot -- so its model, reach and name match.
// BUG-0148/0160: a magic arrow, the bow its button belongs to (MergeShip_MMEquip_ArrowButtonBowIsMM).
void MergeShip_MMEquip_OnUseItem(s32 item) {
    s32 slot;
    s32 mm;

    MergeShip_Own_OnUseItem((u8)item); // each game's bombs .. beans: the button's copy is the one in use
    if (!MergeShip_MMEquip_IsEnabled()) {
        return;
    }
    if ((item >= ITEM_BOW_ARROW_FIRE) && (item <= ITEM_BOW_ARROW_LIGHT) &&
        MergeShip_MMEquip_ArrowButtonBowIsMM(sMergeShipItemButton)) {
        mm = sSlotVariants[MMEQUIP_SLOT_BOW][0];
        if ((mm >= 0) && (MergeShip_MMEquip_SlotSelection(MMEQUIP_SLOT_BOW) != mm)) {
            MergeShip_MMEquip_Select(MMEQUIP_SLOT_BOW, mm);
        }
    } else if ((item == ITEM_MM_BOW_HERO) || (item == ITEM_MM_HOOKSHOT)) {
        slot = (item == ITEM_MM_BOW_HERO) ? MMEQUIP_SLOT_BOW : MMEQUIP_SLOT_HOOKSHOT;
        mm = sSlotVariants[slot][0];
        if ((mm >= 0) && MergeShip_MMEquip_Owns(mm) && (MergeShip_MMEquip_SlotSelection(slot) != mm)) {
            MergeShip_MMEquip_Select(slot, mm);
        }
    } else if ((item == ITEM_BOW) || ((item >= ITEM_BOW_ARROW_FIRE) && (item <= ITEM_BOW_ARROW_LIGHT)) ||
               (item == ITEM_HOOKSHOT) || (item == ITEM_LONGSHOT)) {
        slot = ((item == ITEM_HOOKSHOT) || (item == ITEM_LONGSHOT)) ? MMEQUIP_SLOT_HOOKSHOT : MMEQUIP_SLOT_BOW;
        if (MergeShip_MMEquip_OoTItemOwned(slot) && (MergeShip_MMEquip_SlotSelection(slot) >= 0)) {
            MergeShip_MMEquip_Select(slot, -1);
        }
    }
}

void MergeShip_MMEquip_ItemSlotCycle(s32 invSlot) {
    s32 slot = MergeShip_MMEquip_ItemPageSlot(invSlot);

    if (!MergeShip_MMEquip_ItemSlotCanCycle(invSlot)) {
        return;
    }
    // Two choices on the Items page: Ocarina of Time's item and MM's.
    MergeShip_MMEquip_Select(slot, (MergeShip_MMEquip_SlotSelection(slot) >= 0) ? -1 : sSlotVariants[slot][0]);

    // Young Link switching to Ocarina of Time's adult-only Bow / Hookshot takes it off his buttons (as SoH's item
    // cycles do for an item the wrong age can't use).
    if ((MergeShip_MMEquip_SlotSelection(slot) < 0) && !LINK_IS_ADULT &&
        !CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) {
        s32 i;

        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            u8 b = gSaveContext.equips.buttonItems[i];
            s32 isSlotItem = (slot == MMEQUIP_SLOT_BOW)
                                 ? ((b == ITEM_BOW) || ((b >= ITEM_BOW_ARROW_FIRE) && (b <= ITEM_BOW_ARROW_LIGHT)))
                                 : ((b == ITEM_HOOKSHOT) || (b == ITEM_LONGSHOT));

            if (isSlotItem) {
                gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
            }
        }
    }
}

void* MergeShip_MMEquip_ItemSlotOtherIcon(s32 invSlot) {
    s32 slot = MergeShip_MMEquip_ItemPageSlot(invSlot);
    u8 item;

    if (slot < 0) {
        return NULL;
    }
    item = gSaveContext.inventory.items[invSlot];
    if (MergeShip_MMEquip_SlotSelection(slot) >= 0) {
        return (item < ITEM_NONE_FE) ? gItemIcons[item] : NULL; // Ocarina of Time's, drawn unswapped
    }
    return gItemIcons[MERGESHIP_MMEQUIP_ITEM(sSlotVariants[slot][0])];
}

s32 MergeShip_MMEquip_ItemSlotOtherIsGray(s32 invSlot) {
    s32 slot = MergeShip_MMEquip_ItemPageSlot(invSlot);

    // The other one is Ocarina of Time's adult-only Bow / Hookshot while young Link uses the MM one.
    return (slot >= 0) && (MergeShip_MMEquip_SlotSelection(slot) >= 0) && !LINK_IS_ADULT &&
           !CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0);
}

void* MergeShip_MMEquip_SwapIcon(void* texture) {
    // Swaps nothing any more: MM's Hero's Bow and Hookshot are separate items (ITEM_MM_BOW_HERO, ITEM_MM_HOOKSHOT)
    // whose buttons draw their own icons, so Ocarina of Time's Bow / Hookshot / Longshot always show Ocarina of Time's.
    // Repainting every ITEM_BOW / ITEM_HOOKSHOT texture while MM's was selected (selection follows use) put MM's icon
    // on Ocarina of Time's own button. The selection drives only the model and the reach. Kept for its z_parameter.c
    // call sites.
    return texture;
}

// MM's own names (item_name_static, IA4 128x16 like Ocarina of Time's; English only).
static const char* MergeShip_MMEquip_Name(s32 item) {
    switch (item) {
        case ITEM_MM_SWORD_RAZOR:
            return mm_gItemNameRazorSwordENGTex;
        case ITEM_MM_SWORD_KOKIRI:
            return mm_gItemNameKokiriSwordENGTex;
        case ITEM_MM_SWORD_GILDED:
            return mm_gItemNameGildedSwordENGTex;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            return mm_gItemNameGreatFairysSwordENGTex;
        case ITEM_MM_SHIELD_HERO:
            return mm_gItemNameHerosShieldENGTex;
        case ITEM_MM_SHIELD_MIRROR:
            return mm_gItemNameMirrorShieldENGTex;
        case ITEM_MM_HOOKSHOT:
            return mm_gItemNameHookshotENGTex;
        case ITEM_MM_BOW_HERO:
            return mm_gItemNameHerosBowENGTex;
        default:
            return NULL;
    }
}

void* MergeShip_MMEquip_NameTexture(PlayState* play, void* nameTexture) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const char* name = NULL;

    if (!MergeShip_MMEquip_IsEnabled() || (pauseCtx->namedItem == PAUSE_ITEM_NONE)) {
        return nameTexture;
    }
    // The Equipment face (its Termina view) only. The Items page's Bow / Hookshot slots are Ocarina of Time's own and
    // keep its names whichever variant is selected (as their icons do, MergeShip_MMEquip_SwapIcon); MM's are separate
    // items named on the Items (Termina) page (MergeShip_MMItems_NameTexture).
    if (pauseCtx->pageIndex == PAUSE_EQUIP) {
        name = MergeShip_MMEquip_Name(pauseCtx->cursorItem[PAUSE_EQUIP]);
    }
    return (name != NULL) ? (void*)name : nameTexture;
}

// ---- the Equipment page's Termina view --------------------------------------------------------------------------

// The slot an item of the Termina view shares and its variant (-1: the slot's Ocarina of Time item), or -1.
static s32 MergeShip_MMEquip_TerminaSlot(u16 item, s32* id) {
    *id = -1;
    switch (item) {
        case ITEM_MM_SWORD_RAZOR:
        case ITEM_MM_SWORD_GILDED:
        case ITEM_MM_SWORD_KOKIRI: // MM's own Kokiri Sword (Hyrule's is on Ocarina of Time's page, 2026-09-24)
            *id = MERGESHIP_MMEQUIP_ID(item);
            return MMEQUIP_SLOT_KOKIRI_SWORD;
        case ITEM_MM_SWORD_GREAT_FAIRY:
            *id = MMEQUIP_GREAT_FAIRY;
            return MMEQUIP_SLOT_BIGGORON;
        case ITEM_MM_SHIELD_HERO:
            *id = MMEQUIP_HERO_SHIELD;
            return MMEQUIP_SLOT_HYLIAN;
        case ITEM_MM_SHIELD_MIRROR:
            *id = MMEQUIP_MIRROR_SHIELD;
            return MMEQUIP_SLOT_MIRROR;
        default:
            return -1;
    }
}

// The equipment type and value of a sword or shield slot.
static void MergeShip_MMEquip_SlotEquip(s32 slot, s32* equipType, s32* equipValue) {
    switch (slot) {
        case MMEQUIP_SLOT_KOKIRI_SWORD:
            *equipType = EQUIP_TYPE_SWORD;
            *equipValue = EQUIP_VALUE_SWORD_KOKIRI;
            break;
        case MMEQUIP_SLOT_BIGGORON:
            *equipType = EQUIP_TYPE_SWORD;
            *equipValue = EQUIP_VALUE_SWORD_BIGGORON;
            break;
        case MMEQUIP_SLOT_HYLIAN:
            *equipType = EQUIP_TYPE_SHIELD;
            *equipValue = EQUIP_VALUE_SHIELD_HYLIAN;
            break;
        default:
            *equipType = EQUIP_TYPE_SHIELD;
            *equipValue = EQUIP_VALUE_SHIELD_MIRROR;
            break;
    }
}

s32 MergeShip_MMEquip_TerminaOwns(u16 item) {
    s32 id;
    s32 slot = MergeShip_MMEquip_TerminaSlot(item, &id);

    if ((slot < 0) || !MergeShip_MMEquip_IsEnabled()) {
        return false;
    }
    return (id < 0) ? MergeShip_MMEquip_OoTItemOwned(slot) : MergeShip_MMEquip_Usable(id);
}

s32 MergeShip_MMEquip_TerminaEquipped(u16 item) {
    s32 id;
    s32 slot = MergeShip_MMEquip_TerminaSlot(item, &id);
    s32 equipType;
    s32 equipValue;

    if (!MergeShip_MMEquip_TerminaOwns(item)) {
        return false;
    }
    MergeShip_MMEquip_SlotEquip(slot, &equipType, &equipValue);
    return (CUR_EQUIP_VALUE(equipType) == equipValue) && (MergeShip_MMEquip_SlotSelection(slot) == id);
}

s32 MergeShip_MMEquip_TerminaCanEquip(u16 item) {
    s32 id;
    s32 slot = MergeShip_MMEquip_TerminaSlot(item, &id);
    s32 equipType;
    s32 equipValue;

    if (!MergeShip_MMEquip_TerminaOwns(item)) {
        return false;
    }
    MergeShip_MMEquip_SlotEquip(slot, &equipType, &equipValue);
    // A transformed Link's B is his form's own (his human B waits in the MergeShip save).
    if ((equipType == EQUIP_TYPE_SWORD) && MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
        return false;
    }
    // MM's variants are usable at either age; the Kokiri Sword (Ocarina of Time's) only by the age that can use it.
    if (id < 0) {
        u8 req = gEquipAgeReqs[equipType][equipValue];

        if ((req != 9) && (req != gSaveContext.linkAge) && !MergeShip_MMEquip_Timeless() &&
            !CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) { // 9: AGE_REQ_NONE (z_kaleido_scope.h)
            return false;
        }
    }
    return true;
}

s32 MergeShip_MMEquip_TerminaEquip(PlayState* play, u16 item) {
    s32 id;
    s32 slot = MergeShip_MMEquip_TerminaSlot(item, &id);
    s32 equipType;
    s32 equipValue;

    if (!MergeShip_MMEquip_TerminaCanEquip(item)) {
        return false;
    }
    MergeShip_MMEquip_SlotEquip(slot, &equipType, &equipValue);
    MergeShip_MMEquip_Select(slot, id);
    // The Equipment page's A (z_kaleido_equipment.c): Inventory_ChangeEquipment, and a sword on B.
    Inventory_ChangeEquipment(equipType, equipValue);
    if (equipType == EQUIP_TYPE_SWORD) {
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
        gSaveContext.equips.buttonItems[0] = MergeShip_MMEquip_SwordItem(slot, id);
        if (play != NULL) {
            Interface_LoadItemIcon1(play, 0);
        }
    }
    return true;
}

s32 MergeShip_MMEquip_ChildHoldsHeroShield(void) {
    return MergeShip_MMEquip_IsSelected(MMEQUIP_HERO_SHIELD);
}

// ---- collision --------------------------------------------------------------------------------------------------

// ovl_MM_En_M_Thunder (its struct's `type` is MM's EnMThunderType).
#include "overlays/actors/ovl_MM_En_M_Thunder/z_mm_en_m_thunder.h"

s32 MergeShip_MMEquip_HoldsMMKokiriSword(Player* player) {
    return (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI) &&
           (gSaveContext.equips.buttonItems[0] == ITEM_MM_SWORD_KOKIRI) && MergeShip_MMEquip_IsEnabled();
}

// An enemy's table value for one damage flag.
static f32 MSEq_TableValue(DamageTable* tbl, u32 flag) {
    s32 i = 0;

    while (flag > 1) {
        flag >>= 1;
        i++;
    }
    return (f32)(tbl->table[i] & 0xF);
}

// The enemy's MM sword multiplier for this kind of hit (slash, spin, jump): its best weakness among Ocarina of Time's
// three sword classes, each entry over that class's usual value (Kokiri 1 / Master 2 / Biggoron 4, spins the same,
// jumps twice). The best, not the Master Sword's alone: an enemy only the child meets takes the Kokiri Sword and
// nothing else (Big Octo, BUG-0080), one only the adult meets the Master Sword (a Freezard, 0 from the Kokiri Sword).
// Negative: not a sword hit.
static f32 MSEq_SwordMultiplier(DamageTable* tbl, u32 dmgFlags) {
    f32 m;

    if (dmgFlags & DMG_JUMP_SLASH) {
        m = MSEq_TableValue(tbl, DMG_JUMP_KOKIRI) / 2.0f;
        m = MAX(m, MSEq_TableValue(tbl, DMG_JUMP_MASTER) / 4.0f);
        return MAX(m, MSEq_TableValue(tbl, DMG_JUMP_GIANT) / 8.0f);
    }
    if (dmgFlags & DMG_SPIN_ATTACK) {
        m = MSEq_TableValue(tbl, DMG_SPIN_KOKIRI);
        m = MAX(m, MSEq_TableValue(tbl, DMG_SPIN_MASTER) / 2.0f);
        return MAX(m, MSEq_TableValue(tbl, DMG_SPIN_GIANT) / 4.0f);
    }
    if (dmgFlags & (DMG_SLASH_KOKIRI | DMG_SLASH_MASTER | DMG_SLASH_GIANT)) {
        m = MSEq_TableValue(tbl, DMG_SLASH_KOKIRI);
        m = MAX(m, MSEq_TableValue(tbl, DMG_SLASH_MASTER) / 2.0f);
        return MAX(m, MSEq_TableValue(tbl, DMG_SLASH_GIANT) / 4.0f);
    }
    return -1.0f;
}

// MM CollisionCheck_GetDamageAndEffectOnElementAC for MM's swords (the user, 2026-10-08: "they should match MM's
// damage values 1:1"): the sword's own damage (MM D_8085D09C for the blade, EnMThunder sDamages for the wave) times
// the enemy's multiplier for it (MSEq_SwordMultiplier). MM's swords hit with the Master Sword's flags (the Great
// Fairy's with the Biggoron's), so the damage effect is the Master Sword's.
f32 MergeShip_MMEquip_AdjustDamage(Actor* attacker, ColliderInfo* attackerInfo, DamageTable* tbl, ColliderInfo* info,
                                   f32 damage) {
    f32 mmDamage = 0.0f;
    f32 multiplier;
    u32 dmgFlags;

    if ((attacker == NULL) || (attackerInfo == NULL) || !MergeShip_MMEquip_IsEnabled()) {
        return damage;
    }
    dmgFlags = attackerInfo->toucher.dmgFlags;
    if (attacker->id == ACTOR_PLAYER) {
        Player* player = (Player*)attacker;

        if (MERGESHIP_FORM_IS_MM(player->mergeShipForm) ||
            ((attackerInfo != &player->meleeWeaponQuads[0].info) &&
             (attackerInfo != &player->meleeWeaponQuads[1].info))) {
            return damage;
        }
        // MM D_8085D09C dmgHumanNormal; the jump attacks (flip / jump slash) dmgHumanStrong, twice it.
        if (MergeShip_MMEquip_HoldsMMKokiriSword(player)) {
            mmDamage = 1.0f;
        } else if (player->heldItemAction == PLAYER_IA_MM_SWORD_RAZOR) {
            mmDamage = 2.0f;
        } else if (player->heldItemAction == PLAYER_IA_MM_SWORD_GILDED) {
            mmDamage = 3.0f;
        } else if (player->heldItemAction == PLAYER_IA_MM_SWORD_GREAT_FAIRY) {
            mmDamage = 4.0f;
        } else {
            return damage;
        }
        if (dmgFlags & DMG_JUMP_SLASH) {
            mmDamage *= 2.0f;
        }
    } else if ((gMergeShipActorId_En_M_Thunder >= 0) && (attacker->id == gMergeShipActorId_En_M_Thunder) &&
               !MMEnMThunder_IsSwordBeam(attacker)) {
        mmDamage = attackerInfo->toucher.damage; // MM sDamages, set by the wave
    } else {
        return damage;
    }
    if (tbl == NULL) {
        // Ocarina of Time: the toucher's damage less the defense (MM CollisionCheck_ApplyElementATDefense)
        mmDamage -= info->bumper.defense;
        return (mmDamage < 0.0f) ? 0.0f : mmDamage;
    }
    multiplier = MSEq_SwordMultiplier(tbl, dmgFlags);
    if (multiplier < 0.0f) {
        return damage;
    }
    MergeShipTest_Log("MMSWORD hit flags=0x%08X mm=%.1f x%.2f -> %.1f (OoT's %.1f)", dmgFlags, mmDamage, multiplier,
                      mmDamage * multiplier, damage);
    return mmDamage * multiplier;
}

// ---- which game owns the items both games have (MergeShipOwnership.h) -------------------------------------------------

// MsOwnBit order: the slots' items.
static const u8 sOwnItems[MS_OWN_COUNT] = { ITEM_BOMB, ITEM_BOMBCHU, ITEM_STICK, ITEM_NUT, ITEM_BEAN, ITEM_BOW,
                                            ITEM_OCARINA_FAIRY };
// Each counted item's capacity upgrade and its MS_BAGS field (-1: none), MsOwnBit MS_OWN_BOMB .. MS_OWN_BOW.
static const s8 sOwnUpgrade[MS_OWN_AMMO_COUNT] = { UPG_BOMB_BAG, -1, UPG_STICKS, UPG_NUTS, -1, UPG_QUIVER };
static const s8 sOwnBagsField[MS_OWN_AMMO_COUNT] = { MS_BAGS_FIELD_BOMB_BAG, -1, MS_BAGS_FIELD_STICKS,
                                                     MS_BAGS_FIELD_NUTS,     -1, MS_BAGS_FIELD_QUIVER };
// The slots' counts are the shared save's one ammo pool (task 1, 2026-09-30): true once this engine has bound them
// into the live inventory (cleared by an apply / a load: the next Sync binds again).
static s32 sOwnBound = false;

static MsSharedSave* MsOwn_Shared(void) {
    MsSharedSave* shared;

    if (!MergeShipHost_IsHosted() || (gSaveContext.fileNum < 0) || (gSaveContext.fileNum > 2)) {
        return NULL;
    }
    shared = (MsSharedSave*)MergeShipHost_SharedSave();
    // Only once it is this file's (MergeShipHost_ClaimSharedSave): an earlier playthrough's never reaches this one.
    if ((shared == NULL) || (shared->hyruleFileId != MergeShip_Save_SharedFileId())) {
        return NULL;
    }
    return shared;
}

static s32 MsOwn_Combined(void) {
    return MergeShipShared_CombinedInGame();
}

static s32 MsOwn_Bit(u8 item) {
    switch (item) {
        case ITEM_BOMB:
            return MS_OWN_BOMB;
        case ITEM_BOMBCHU:
            return MS_OWN_BOMBCHU;
        case ITEM_STICK:
            return MS_OWN_STICK;
        case ITEM_NUT:
            return MS_OWN_NUT;
        case ITEM_BEAN:
            return MS_OWN_BEAN;
        case ITEM_BOW:
            return MS_OWN_BOW;
        case ITEM_OCARINA_FAIRY:
        case ITEM_OCARINA_TIME:
            return MS_OWN_OCARINA;
        default:
            return -1;
    }
}

static s32 MsOwn_Field(s32 upgrade) {
    return (upgrade == UPG_QUIVER)     ? MS_BAGS_FIELD_QUIVER
           : (upgrade == UPG_BOMB_BAG) ? MS_BAGS_FIELD_BOMB_BAG
           : (upgrade == UPG_STICKS)   ? MS_BAGS_FIELD_STICKS
           : (upgrade == UPG_NUTS)     ? MS_BAGS_FIELD_NUTS
                                       : -1;
}

// A file from before these bits: everything it holds is Ocarina of Time's (written once).
static void MsOwn_EnsureHyrule(MsSharedSave* shared) {
    u8 own = 0;
    u8 bags;
    s32 i;

    if (shared->ownValid & MS_OWN_VALID_HYRULE) {
        return;
    }
    for (i = MS_OWN_BOMB; i <= MS_OWN_BEAN; i++) {
        own |= (INV_CONTENT(sOwnItems[i]) == sOwnItems[i]) ? (1 << i) : 0;
    }
    own |= MergeShip_MMEquip_OoTItemOwned(MMEQUIP_SLOT_BOW) ? (1 << MS_OWN_BOW) : 0;
    own |= (INV_CONTENT(ITEM_OCARINA_FAIRY) != ITEM_NONE) ? (1 << MS_OWN_OCARINA) : 0;
    shared->hyruleOwn = own;
    bags = MS_BAGS_MAKE(CUR_UPG_VALUE(UPG_QUIVER), CUR_UPG_VALUE(UPG_BOMB_BAG));
    bags = MS_BAGS_WITH(bags, MS_BAGS_FIELD_STICKS, CUR_UPG_VALUE(UPG_STICKS));
    shared->hyruleBags = MS_BAGS_WITH(bags, MS_BAGS_FIELD_NUTS, CUR_UPG_VALUE(UPG_NUTS));
    shared->ownValid |= MS_OWN_VALID_HYRULE;
}

static MsSharedSave* MsOwn_SharedReady(void) {
    MsSharedSave* shared = MsOwn_Shared();

    if (shared != NULL) {
        MsOwn_EnsureHyrule(shared);
    }
    return shared;
}

s32 MergeShip_Own_OoTOwns(u8 item) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 bit = MsOwn_Bit(item);

    if ((shared == NULL) || (bit < 0) || (bit > MS_OWN_BEAN)) {
        return INV_CONTENT(item) == item;
    }
    return (shared->hyruleOwn >> bit) & 1;
}

// Harness "ownbit": which game owns a shared-slot item (BUG-0068's test: Termina's Magic Beans in the slot).
s32 MergeShip_Own_DebugSet(u8 item, s32 hyrule, s32 termina) {
    MsSharedSave* shared = MsOwn_SharedReady(); // set up from the inventory first, so this stays
    s32 bit = MsOwn_Bit(item);

    if ((shared == NULL) || (bit < 0)) {
        return false;
    }
    shared->hyruleOwn = (shared->hyruleOwn & ~(1u << bit)) | ((hyrule ? 1u : 0u) << bit);
    shared->terminaOwn = (shared->terminaOwn & ~(1u << bit)) | ((termina ? 1u : 0u) << bit);
    return true;
}

s32 MergeShip_Own_MMOwns(u8 item) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 bit = MsOwn_Bit(item);

    return (shared != NULL) && (bit >= 0) && ((shared->terminaOwn >> bit) & 1);
}

s32 MergeShip_Own_OoTUpgrade(s32 upgrade) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared != NULL) && ((upgrade == UPG_QUIVER) || (upgrade == UPG_BOMB_BAG))) {
        return MS_BAGS_LEVEL(shared->hyruleBags, MsOwn_Field(upgrade));
    }
    return CUR_UPG_VALUE(upgrade);
}

s32 MergeShip_Own_MMUpgrade(s32 upgrade) {
    MsSharedSave* shared = MsOwn_Shared();

    if ((shared == NULL) || (MsOwn_Field(upgrade) < 0)) {
        return 0;
    }
    return MS_BAGS_LEVEL(shared->terminaBags, MsOwn_Field(upgrade));
}

s32 MergeShip_Own_OoTUpgradeForLogic(s32 upgrade, s32 logicValue, const void* saveContext) {
    if ((saveContext != (const void*)&gSaveContext) || (MsOwn_Shared() == NULL) ||
        ((upgrade != UPG_QUIVER) && (upgrade != UPG_BOMB_BAG))) {
        return logicValue;
    }
    return MergeShip_Own_OoTUpgrade(upgrade);
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

// ---- ammo: each game's own counts (BUG-0105, 2026-10-12: "no shared ammo period"; the one pool of 2026-09-30 is gone,
// MsSharedSave.ammo is no longer read or written). Either game's bag still sets the capacity. -----------------------

static s32 MsOwn_Has(const MsSharedSave* shared, s32 bit, s32 mm) {
    return ((mm ? shared->terminaOwn : shared->hyruleOwn) >> bit) & 1;
}

// The better bag level either game has found for `bit` (an owned item holds at least the first).
static s32 MsOwn_Level(const MsSharedSave* shared, s32 bit) {
    s32 level;

    if (sOwnBagsField[bit] < 0) {
        return 0;
    }
    level = MAX(MS_BAGS_LEVEL(shared->hyruleBags, sOwnBagsField[bit]), MS_BAGS_LEVEL(shared->terminaBags, sOwnBagsField[bit]));
    return (level == 0) ? 1 : level;
}

static s32 MsOwn_Capacity(const MsSharedSave* shared, s32 bit) {
    if (sOwnUpgrade[bit] >= 0) {
        return gUpgradeCapacities[sOwnUpgrade[bit]][MsOwn_Level(shared, bit)];
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

// The slot's capacity follows the better bag either game has found.
static void MsOwn_SetLevel(const MsSharedSave* shared, s32 bit) {
    const s32 upgrade = sOwnUpgrade[bit];

    if ((upgrade >= 0) && ((s32)CUR_UPG_VALUE(upgrade) != MsOwn_Level(shared, bit))) {
        Inventory_ChangeUpgrade(upgrade, MsOwn_Level(shared, bit));
    }
}

static s16 MsOwn_Clamp(const MsSharedSave* shared, s32 bit, s32 count) {
    const s32 capacity = MsOwn_Capacity(shared, bit);

    return (s16)((count > capacity) ? capacity : count);
}

// The slots keep Hyrule's own counts (its file's), clamped to the capacity either game's bag gives.
static void MsOwn_Bind(MsSharedSave* shared) {
    s32 bit;

    for (bit = 0; bit < MS_OWN_AMMO_COUNT; bit++) {
        if (!MsOwn_Has(shared, bit, 0) && !MsOwn_Has(shared, bit, 1)) {
            continue;
        }
        MsOwn_SetLevel(shared, bit);
        AMMO(sOwnItems[bit]) = MsOwn_Clamp(shared, bit, AMMO(sOwnItems[bit]));
    }
    sOwnBound = true;
}

void MergeShip_Own_Unbind(void) {
    sOwnBound = false;
}

// task 3: no bombchu ammo anywhere (a chest, a check, a grass/pot/enemy drop, a shop) before the unified seed's
// Bombchu Bag has a level -- in either game (the user saw "bombchu ammo dropping despite not getting the bag").
s32 MergeShip_Own_ChusLocked(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    return (shared != NULL) && shared->hyruleChuBag &&
           (MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid) == 0) &&
           !MsOwn_Has(shared, MS_OWN_BOMBCHU, 0) && !MsOwn_Has(shared, MS_OWN_BOMBCHU, 1);
}

// The bombchu capacity in effect here (AmmoSync.cpp's clamp for a teammate's count): the unified seed's Bombchu Bag
// (the pool's, either game's level, MsOwn_Capacity); else SoH's own -- the randomizer's progressive bag
// (Bombchus.cpp VB_CHECK_BOMBCHU_CAPACITY, Context::GetBombchuCapacity: MsShared_ChuBagCapacity's tiers) or
// Item_Give's flat 50. Never the bomb bag's.
s32 MergeShip_Own_ChuCapacity(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared != NULL) && shared->hyruleChuBag) {
        return MsOwn_Capacity(shared, MS_OWN_BOMBCHU);
    }
    if (IS_RANDO && (Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_PROGRESSIVE)) {
        return MsShared_ChuBagCapacity(gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel);
    }
    return 50;
}

// Raises the shared Bombchu Bag (chuBagLevel) to `level`; only raises, never lowers. True when it went up.
static s32 MsOwn_RaiseChuBag(MsSharedSave* shared, s32 level) {
    if (level <= MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid)) {
        return false;
    }
    shared->chuBagLevel = (u8)((level > 3) ? 3 : level);
    shared->ownValid |= MS_OWN_VALID_CHU_BAG; // this build wrote it: the raw byte is now trustworthy
    return true;
}

// Hyrule's own Bombchu Bag holds Hyrule's chus: Ocarina of Time owns them (its page shows them, the pool carries them).
// Context::HandleGetBombchuBag and the starting bag fill INV_CONTENT / AMMO directly, never through Item_Give's
// MergeShip_Own_OnOoTItemGiven, so nothing else marks them.
static void MsOwn_OwnOoTChus(MsSharedSave* shared) {
    if (MsOwn_Has(shared, MS_OWN_BOMBCHU, 0)) {
        return;
    }
    shared->hyruleOwn |= (u8)(1 << MS_OWN_BOMBCHU);
}

// SoH's own randomizer raised its Bombchu Bag (SeedContext.cpp Context::HandleGetBombchuBag): merge the level into the
// shared save. 2Ship has no progression of its own (MsItems.def PROGRESSIVE_BOMBCHU_BAG is Hyrule's item); it only
// reads chuBagLevel back (MergeShip_Own_ChusLocked, its own capacity).
void MergeShip_Own_RaiseOoTChuBag(s32 level) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared == NULL) || (level <= 0)) {
        return;
    }
    MsOwn_RaiseChuBag(shared, level);
    MsOwn_OwnOoTChus(shared);
    MergeShip_Own_Sync();
}

void MergeShip_Own_GrantOoTSticks(void) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if (CUR_UPG_VALUE(UPG_STICKS) == 0) {
        Inventory_ChangeUpgrade(UPG_STICKS, 1);
    }
    if (shared != NULL) {
        MsOwn_RaiseBags(&shared->hyruleBags, MS_BAGS_FIELD_STICKS, 1);
        shared->hyruleOwn |= (u8)(1 << MS_OWN_STICK);
    }
}

void MergeShip_Own_OnOoTItemGiven(u8 item) {
    MsSharedSave* shared = MsOwn_SharedReady();
    s32 bit = -1;
    s32 field = -1;
    s32 level = 0;

    if (shared == NULL) {
        return;
    }
    switch (item) {
        case ITEM_BOMB_BAG_20:
        case ITEM_BOMB_BAG_30:
        case ITEM_BOMB_BAG_40:
            field = MS_BAGS_FIELD_BOMB_BAG;
            level = item - ITEM_BOMB_BAG_20 + 1;
            bit = MS_OWN_BOMB;
            break;
        case ITEM_BOW:
            field = MS_BAGS_FIELD_QUIVER;
            level = 1;
            bit = MS_OWN_BOW;
            break;
        case ITEM_QUIVER_30:
        case ITEM_QUIVER_40:
        case ITEM_QUIVER_50:
            field = MS_BAGS_FIELD_QUIVER;
            level = item - ITEM_QUIVER_30 + 1;
            bit = MS_OWN_BOW;
            break;
        case ITEM_BOMBCHU:
        case ITEM_BOMBCHUS_5:
        case ITEM_BOMBCHUS_20:
            // task 3: a chest, a drop or a shop's chus before the bag give nothing (MergeShip_Own_ChusLocked).
            if (MergeShip_Own_ChusLocked()) {
                return;
            }
            bit = MS_OWN_BOMBCHU;
            break;
        case ITEM_STICK:
        case ITEM_STICKS_5:
        case ITEM_STICKS_10:
        case ITEM_STICK_UPGRADE_20:
        case ITEM_STICK_UPGRADE_30:
            // A single stick picked up (a drop, where Majora's Mask's sticks are what the slot holds) fills those.
            if ((item == ITEM_STICK) && MsOwn_Has(shared, MS_OWN_STICK, 1) && !MsOwn_Has(shared, MS_OWN_STICK, 0)) {
                return;
            }
            field = MS_BAGS_FIELD_STICKS;
            level = (item == ITEM_STICK_UPGRADE_20) ? 2 : (item == ITEM_STICK_UPGRADE_30) ? 3 : 1;
            bit = MS_OWN_STICK;
            break;
        case ITEM_NUT:
        case ITEM_NUTS_5:
        case ITEM_NUTS_10:
        case ITEM_NUT_UPGRADE_30:
        case ITEM_NUT_UPGRADE_40:
            if ((item == ITEM_NUT) && MsOwn_Has(shared, MS_OWN_NUT, 1) && !MsOwn_Has(shared, MS_OWN_NUT, 0)) {
                return;
            }
            field = MS_BAGS_FIELD_NUTS;
            level = (item == ITEM_NUT_UPGRADE_30) ? 2 : (item == ITEM_NUT_UPGRADE_40) ? 3 : 1;
            bit = MS_OWN_NUT;
            break;
        case ITEM_BEAN:
            bit = MS_OWN_BEAN;
            break;
        case ITEM_OCARINA_FAIRY:
        case ITEM_OCARINA_TIME:
            bit = MS_OWN_OCARINA;
            break;
        default:
            return;
    }
    MsOwn_RaiseBags(&shared->hyruleBags, field, level);
    shared->hyruleOwn |= (u8)(1 << bit);
}

void MergeShip_Own_GiveMM(s32 ownBit) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared != NULL) && (ownBit >= 0) && (ownBit < MS_OWN_COUNT)) {
        shared->terminaOwn |= (u8)(1 << ownBit);
        MergeShip_Own_Sync();
    }
}

void MergeShip_Own_RaiseMMUpgrade(s32 upgrade, s32 level) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if (shared != NULL) {
        MsOwn_RaiseBags(&shared->terminaBags, MsOwn_Field(upgrade), level);
        if ((upgrade == UPG_BOMB_BAG) && (level > 0)) {
            shared->terminaOwn |= (u8)(1 << MS_OWN_BOMB); // MM's bomb bag holds MM's bombs
        }
        MergeShip_Own_Sync();
    }
}

void MergeShip_Own_FillMM(s32 ownBit) {
    MsSharedSave* shared = MsOwn_SharedReady();

    if ((shared == NULL) || (ownBit < 0) || (ownBit >= MS_OWN_AMMO_COUNT) || !MsOwn_Has(shared, ownBit, 1)) {
        return;
    }
    // Hyrule's own count, filled (BUG-0105: each game's own).
    MsOwn_SetLevel(shared, ownBit);
    AMMO(sOwnItems[ownBit]) = (s8)MsOwn_Capacity(shared, ownBit);
}

// Older builds put Majora's Mask's bottles into Hyrule's own slots (hyruleBottleMM): they go to Majora's Mask's.
static void MsOwn_MoveOldMMBottles(MsSharedSave* shared) {
    s32 i;
    s32 button;

    if ((shared->hyruleBottleMM == 0) || MsOwn_Combined()) {
        return;
    }
    for (i = 0; i < 4; i++) {
        if (!((shared->hyruleBottleMM >> i) & 1)) {
            continue;
        }
        if ((gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] != ITEM_NONE) &&
            MergeShip_Own_GiveMMBottle(gSaveContext.inventory.items[SLOT_BOTTLE_1 + i])) {
            gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = ITEM_NONE;
            for (button = 1; button < ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
                if (gSaveContext.equips.cButtonSlots[button - 1] == SLOT_BOTTLE_1 + i) {
                    gSaveContext.equips.buttonItems[button] = ITEM_NONE;
                    gSaveContext.equips.cButtonSlots[button - 1] = SLOT_NONE;
                }
            }
        }
        shared->hyruleBottleMM &= (u8)~(1 << i);
    }
}

void MergeShip_Own_Sync(void) {
    MsSharedSave* shared = MsOwn_SharedReady();
    u8 either;
    s32 i;
    s32 want;
    s32 ootChuBag;

    if (shared == NULL) {
        return;
    }
    // task 3: a Bombchu Bag Hyrule holds that Context::HandleGetBombchuBag never merged -- the randomizer's starting bag
    // (savefile.cpp sets bombchuUpgradeLevel / INV_CONTENT directly) or one found in a file from before chuBagLevel --
    // merges here; otherwise the pool's chu capacity (MsOwn_Capacity) stays 0 in both engines.
    if (shared->hyruleChuBag && IS_RANDO) {
        if (Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_SINGLE) {
            // "Single" has no levels (HandleGetBombchuBag merges its one bag as the top): Hyrule's own chus in the
            // slot -- not only Termina's, which this Sync puts there -- mean Hyrule has the bag.
            ootChuBag = ((INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) &&
                         (MsOwn_Has(shared, MS_OWN_BOMBCHU, 0) || !MsOwn_Has(shared, MS_OWN_BOMBCHU, 1)))
                            ? 3
                            : 0;
        } else {
            ootChuBag = gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel;
        }
        // Only a level above the shared one is Hyrule's own bag (an equal one may be Termina's, copied down below).
        if (MsOwn_RaiseChuBag(shared, ootChuBag)) {
            MsOwn_OwnOoTChus(shared);
        }
        // A file whose bag the previous build merged (chuBagLevel set, hyruleOwn never): the chus in the slot, owned by
        // neither game, can only be Hyrule's own bag's -- Termina's bag always marks terminaOwn, this Sync fills the
        // slot only for an owner, and Item_Give no longer fills it while the chus are locked.
        if ((MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid) > 0) &&
            (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) && !MsOwn_Has(shared, MS_OWN_BOMBCHU, 0) &&
            !MsOwn_Has(shared, MS_OWN_BOMBCHU, 1)) {
            MsOwn_OwnOoTChus(shared);
        }
    }
    either = shared->hyruleOwn | shared->terminaOwn;
    for (i = MS_OWN_BOMB; i <= MS_OWN_BEAN; i++) {
        if (((either >> i) & 1) && (INV_CONTENT(sOwnItems[i]) != sOwnItems[i])) {
            INV_CONTENT(sOwnItems[i]) = sOwnItems[i];
            // BUG-0105: new to Hyrule (the other game's find), it arrives full -- its count is Hyrule's own from here.
            MsOwn_SetLevel(shared, i);
            AMMO(sOwnItems[i]) = (s8)MsOwn_Capacity(shared, i);
        }
    }
    MsOwn_MoveOldMMBottles(shared);

    if (MsShared_ChuBagLevel(shared->chuBagLevel, shared->ownValid) >
        gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel) {
        // task 3: Termina found a Bombchu Bag copy Hyrule hasn't yet -- keep Hyrule's own capacity clamp
        // (Enhancements/randomizer/Bombchus.cpp VB_CHECK_BOMBCHU_CAPACITY, off this local level) from fighting the
        // shared pool's (higher) capacity.
        gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel = shared->chuBagLevel;
    }

    if (MsOwn_Combined()) {
        // "Combine Shared Items": both games' are one -- the larger bag, one count (the shared save's apply).
        if (((shared->terminaOwn >> MS_OWN_STICK) & 1) && (CUR_UPG_VALUE(UPG_STICKS) == 0)) {
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
        }
        if (((shared->terminaOwn >> MS_OWN_NUT) & 1) && (CUR_UPG_VALUE(UPG_NUTS) == 0)) {
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
        }
        want = MAX(MS_BAGS_QUIVER(shared->hyruleBags), MS_BAGS_QUIVER(shared->terminaBags));
        if (CUR_UPG_VALUE(UPG_QUIVER) < want) {
            Inventory_ChangeUpgrade(UPG_QUIVER, want);
        }
        want = MAX(MS_BAGS_BOMB_BAG(shared->hyruleBags), MS_BAGS_BOMB_BAG(shared->terminaBags));
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) < want) {
            Inventory_ChangeUpgrade(UPG_BOMB_BAG, want);
        }
        return;
    }

    if (!sOwnBound) {
        MsOwn_Bind(shared);
    }
    for (i = 0; i < MS_OWN_AMMO_COUNT; i++) {
        if (!MsOwn_Has(shared, i, 0) && !MsOwn_Has(shared, i, 1)) {
            continue;
        }
        MsOwn_SetLevel(shared, i);
    }
}

static s32 MsOwn_CountedBit(u8 item) {
    switch (item) {
        case ITEM_BOW:
        case ITEM_MM_BOW_HERO:
        case ITEM_ARROW_FIRE:
        case ITEM_ARROW_ICE:
        case ITEM_ARROW_LIGHT:
        case ITEM_BOW_ARROW_FIRE:
        case ITEM_BOW_ARROW_ICE:
        case ITEM_BOW_ARROW_LIGHT:
            return MS_OWN_BOW;
        default: {
            s32 bit = MsOwn_Bit(item);
            return ((bit >= 0) && (bit < MS_OWN_AMMO_COUNT)) ? bit : -1;
        }
    }
}

// Vestigial since ammo became one pool (task 1, 2026-09-30): there is no "copy in use" left to switch when a page or a
// button equips an item, so these are no-ops. Kept (with their call sites in z_kaleido_item.c, z_kaleido_mm_item.c,
// Player_UseItem) rather than pulled out everywhere they're called from.
void MergeShip_Own_OnPageEquip(u8 item, s32 mmPage) {
    (void)item;
    (void)mmPage;
}

void MergeShip_Own_OnUseItem(u8 item) {
    (void)item;
}

s16 MergeShip_Own_PageAmmo(s16 item, s32 mmCopy) {
    MsSharedSave* shared = MsOwn_SharedReady();
    const s32 bit = MsOwn_CountedBit((u8)item);

    (void)mmCopy; // one count per slot: both pages show Hyrule's own (BUG-0105)
    (void)shared;
    (void)bit;
    return AMMO(item);
}

// ---- bottles ---------------------------------------------------------------------------------------------------------

s32 MergeShip_Own_MMBottlesHere(void) {
    return (MsOwn_Shared() != NULL) && !MsOwn_Combined();
}

u8 MergeShip_Own_MMBottle(s32 index) {
    MsSharedSave* shared = MsOwn_Shared();

    if ((shared == NULL) || (index < 0) || (index >= MS_SHARED_BOTTLE_SLOTS)) {
        return ITEM_NONE;
    }
    return MergeShipShared_ItemFromBottle(shared->bottles[index]);
}

u8 MergeShip_Own_ButtonSlotItem(u8 slot) {
    if (MS_MM_BOTTLE_BUTTON_SLOT_IS(slot)) {
        return MergeShip_Own_MMBottle(slot - MS_MM_BOTTLE_BUTTON_SLOT_FIRST);
    }
    if (MS_MM_AMMO_BUTTON_SLOT_IS(slot)) {
        return sOwnItems[slot - MS_MM_AMMO_BUTTON_SLOT_FIRST];
    }
    if (slot >= ARRAY_COUNT(gSaveContext.inventory.items)) {
        return ITEM_NONE;
    }
    return gSaveContext.inventory.items[slot];
}

void MergeShip_Own_SetButtonSlotBottle(u8 slot, u8 item) {
    MsSharedSave* shared = MsOwn_Shared();
    u8 bottle = MergeShipShared_BottleFromItem(item);

    if ((shared != NULL) && MS_MM_BOTTLE_BUTTON_SLOT_IS(slot) && (bottle != MS_BOTTLE_NONE)) {
        shared->bottles[slot - MS_MM_BOTTLE_BUTTON_SLOT_FIRST] = bottle;
    }
}

// A content Majora's Mask keeps one of per world (MsDelivery.h MsDelivery_BottleOnePerWorld: the Deku Princess, the
// Seahorse) that the world has already, as far as Hyrule sees it: in one of Majora's Mask's six (the shared save's), or
// with "Combine Shared Items" one of Hyrule's four (the same set then). Termina's week events are its own: 2Ship's
// MergeShipShared_Apply empties a princess given here while she is back in the Deku King's chamber.
static s32 MsOwn_WorldHasOnePerWorld(const MsSharedSave* shared, u8 bottle) {
    s32 i;

    if (!MsDelivery_BottleOnePerWorld(bottle)) {
        return false;
    }
    for (i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        if (shared->bottles[i] == bottle) {
            return true;
        }
    }
    for (i = 0; MsOwn_Combined() && (i < 4); i++) {
        if (MergeShipShared_BottleFromItem(gSaveContext.inventory.items[SLOT_BOTTLE_1 + i]) == bottle) {
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_GiveMMBottle(u8 content) {
    MsSharedSave* shared = MsOwn_Shared();
    u8 bottle = MergeShipShared_BottleFromItem(content);
    s32 i;

    if ((shared == NULL) || (bottle == MS_BOTTLE_NONE)) {
        return false;
    }
    // Never a second one (a "Bottle with Deku Princess" found, from a teammate, or an older build's bottle moved here
    // while the world has her): the bottle comes empty (the user, 2026-09-30).
    if (MsOwn_WorldHasOnePerWorld(shared, bottle)) {
        MergeShipTest_Log("MMBOTTLE 0x%02X: the world has its one already, the bottle comes empty", content);
        bottle = MS_BOTTLE_EMPTY;
    }
    for (i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        if (shared->bottles[i] == MS_BOTTLE_NONE) {
            shared->bottles[i] = bottle;
            return true;
        }
    }
    return false;
}

// Every button holding Majora's Mask's bottle `index` now shows `item`.
static void MsOwn_RefreshBottleButtons(PlayState* play, s32 index, u8 item) {
    s32 button;

    for (button = 1; button < ARRAY_COUNT(gSaveContext.equips.buttonItems); button++) {
        if (gSaveContext.equips.cButtonSlots[button - 1] == MS_MM_BOTTLE_BUTTON_SLOT(index)) {
            gSaveContext.equips.buttonItems[button] = item;
            if (play != NULL) {
                Interface_LoadItemIcon1(play, button);
            }
            gSaveContext.buttonStatus[((button) >= 4) ? ((button) + 1) : (button)] = BTN_ENABLED; // z_parameter.c BUTTON_STATUS_INDEX
        }
    }
}

s32 MergeShip_Own_FillMMBottle(PlayState* play, u8 content) {
    MsSharedSave* shared = MsOwn_Shared();
    const u8 bottle = MergeShipShared_BottleFromItem(content);
    s32 i;

    if ((shared == NULL) || MsOwn_Combined() || (bottle == MS_BOTTLE_NONE)) {
        return false;
    }
    for (i = 0; i < MS_SHARED_BOTTLE_SLOTS; i++) {
        if (shared->bottles[i] == MS_BOTTLE_EMPTY) {
            shared->bottles[i] = bottle;
            MsOwn_RefreshBottleButtons(play, i, content);
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_MMHasEmptyBottle(void) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 i;

    for (i = 0; (shared != NULL) && !MsOwn_Combined() && (i < MS_SHARED_BOTTLE_SLOTS); i++) {
        if (shared->bottles[i] == MS_BOTTLE_EMPTY) {
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_ConsumeMMFairy(PlayState* play) {
    MsSharedSave* shared = MsOwn_Shared();
    s32 i;

    for (i = 0; (shared != NULL) && !MsOwn_Combined() && (i < MS_SHARED_BOTTLE_SLOTS); i++) {
        if (shared->bottles[i] == MS_BOTTLE_FAIRY) {
            shared->bottles[i] = MS_BOTTLE_EMPTY;
            MsOwn_RefreshBottleButtons(play, i, ITEM_BOTTLE);
            return true;
        }
    }
    return false;
}

s32 MergeShip_Own_HyruleBottleIsMM(s32 index) {
    MsSharedSave* shared = MsOwn_Shared();

    return (shared != NULL) && (index >= 0) && (index < 4) && ((shared->hyruleBottleMM >> index) & 1);
}
