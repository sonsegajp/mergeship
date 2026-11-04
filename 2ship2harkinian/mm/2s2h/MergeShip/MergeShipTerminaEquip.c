// MergeShip2 -- Termina Equipment: which of Majora's Mask's swords and shields Termina holds, equipping them, and the
// shared save's part. See MergeShipTerminaEquip.h.

#include "global.h"
#include "2s2h/MergeShip/MergeShipTerminaEquip.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include <libultraship/bridge/consolevariablebridge.h>
#ifdef MERGESHIP_HOSTED
#include <mergeship/EngineApi.h> // MS_ENGINE_* (BUG-0120)
#include "2s2h/MergeShip/MergeShipHost.h" // MergeShipHostTest_Log
#include <mergeship/SharedSave.h>
#endif

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)
#define PLAYER_DATA (gSaveContext.save.saveInfo.playerData)

#define MS_TEQ_SWORD_BITS 0x07  // Kokiri, Razor, Gilded
#define MS_TEQ_SHIELD_BITS 0x03 // Hero's, Mirror
#define MS_TEQ_RAZOR_HEALTH 100 // MM Item_GiveImpl: a new Razor Sword

void MergeShipTest_Log(const char* fmt, ...);

s32 MergeShip_TerminaEquip_Enabled(void) {
    return CVarGetInteger(MERGESHIP_TERMINA_EQUIP_CVAR, 1) && MergeShip_IsOcarinaMounted();
}

// ---- items and values -------------------------------------------------------------------------------------------------

static s32 MsTeq_IsSwordValue(s32 value) {
    return (value >= EQUIP_VALUE_SWORD_KOKIRI) && (value <= EQUIP_VALUE_SWORD_GILDED);
}

static s32 MsTeq_IsShieldValue(s32 value) {
    return (value == EQUIP_VALUE_SHIELD_HERO) || (value == EQUIP_VALUE_SHIELD_MIRROR);
}

// The sword value of ITEM_SWORD_KOKIRI .. ITEM_SWORD_GILDED, else 0.
static s32 MsTeq_SwordValue(u8 item) {
    return ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED))
               ? (item - ITEM_SWORD_KOKIRI + EQUIP_VALUE_SWORD_KOKIRI)
               : EQUIP_VALUE_SWORD_NONE;
}

// The shield value of ITEM_SHIELD_HERO / ITEM_SHIELD_MIRROR, else 0.
static s32 MsTeq_ShieldValue(u8 item) {
    return ((item == ITEM_SHIELD_HERO) || (item == ITEM_SHIELD_MIRROR))
               ? (item - ITEM_SHIELD_HERO + EQUIP_VALUE_SHIELD_HERO)
               : EQUIP_VALUE_SHIELD_NONE;
}

// MM's own sword is the human's B sword (no Ocarina of Time sword nor the Great Fairy's Sword on B instead).
static s32 MsTeq_MMSwordInUse(void) {
    return !MergeShip_OoTEquip_Enabled() || (OOT_EQUIP.sword == MS_OOTEQ_SWORD_MM);
}

// MM's own shield is the human's shield (no Ocarina of Time shield instead).
static s32 MsTeq_MMShieldInUse(void) {
    return !MergeShip_OoTEquip_Enabled() || (OOT_EQUIP.shield == MS_OOTEQ_SHIELD_MM);
}

static s32 MsTeq_OwnsSwordValue(s32 value) {
    return MsTeq_IsSwordValue(value) && ((OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BIT(value)) ||
                                         (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == value));
}

static s32 MsTeq_OwnsShieldValue(s32 value) {
    return MsTeq_IsShieldValue(value) && ((OOT_EQUIP.terminaShields & MS_TEQ_SHIELD_BIT(value)) ||
                                          (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == value));
}

static s32 MsTeq_OwnsGreatFairySword(void) {
    return INV_CONTENT(ITEM_SWORD_GREAT_FAIRY) == ITEM_SWORD_GREAT_FAIRY;
}

s32 MergeShip_TerminaEquip_Owns(u8 item) {
    if (item == ITEM_SWORD_GREAT_FAIRY) {
        return MsTeq_OwnsGreatFairySword();
    }
    if (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE) {
        return MsTeq_OwnsSwordValue(MsTeq_SwordValue(item));
    }
    if (MsTeq_ShieldValue(item) != EQUIP_VALUE_SHIELD_NONE) {
        return MsTeq_OwnsShieldValue(MsTeq_ShieldValue(item));
    }
    return false;
}

s32 MergeShip_TerminaEquip_IsEquipped(u8 item) {
    if (!MergeShip_TerminaEquip_Owns(item)) {
        return false;
    }
    if (item == ITEM_SWORD_GREAT_FAIRY) {
        return MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_GREAT_FAIRY);
    }
    if (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE) {
        return MsTeq_MMSwordInUse() && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == MsTeq_SwordValue(item));
    }
    return MsTeq_MMShieldInUse() && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == MsTeq_ShieldValue(item));
}

s32 MergeShip_TerminaEquip_CanEquip(u8 item) {
    if (!MergeShip_TerminaEquip_Owns(item)) {
        return false;
    }
    if ((item == ITEM_SWORD_GREAT_FAIRY) || (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE)) {
        // A transformed Link's B is his form's own (SoH's view: swords not while transformed).
        if (GET_PLAYER_FORM != PLAYER_FORM_HUMAN) {
            return false;
        }
        // The Great Fairy's Sword on B is Ocarina of Time Equipment's selection, which keeps B in step.
        if ((item == ITEM_SWORD_GREAT_FAIRY) && !MergeShip_OoTEquip_Enabled()) {
            return false;
        }
    }
    return true;
}

// ---- what Termina holds -------------------------------------------------------------------------------------------------

static s32 sObservedSword = -1;
static s32 sObservedShield = -1;

static void MsTeq_Record(void) {
    s32 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    s32 shield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);

    if (MsTeq_IsSwordValue(sword)) {
        OOT_EQUIP.terminaSwords |= MS_TEQ_SWORD_BIT(sword);
    }
    if (MsTeq_IsShieldValue(shield)) {
        OOT_EQUIP.terminaShields |= MS_TEQ_SHIELD_BIT(shield);
    }
}

// A change made here (the view, the shared save), not by one of MM's events: the next frame compares with it.
static void MsTeq_SyncObserver(void) {
    MsTeq_Record();
    sObservedSword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    sObservedShield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
}

void MergeShip_TerminaEquip_ResetObserver(void) {
    sObservedSword = -1;
    sObservedShield = -1;
}

void MergeShip_TerminaEquip_Observe(void) {
    s32 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    s32 shield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);

    if (MsTeq_IsSwordValue(sObservedSword) && (sword != sObservedSword)) {
        if (sword == EQUIP_VALUE_SWORD_NONE) {
            // MM took it: handed to the smith for reforging, or stolen by Takkuri (bought back, it is given again).
            OOT_EQUIP.terminaSwords &= ~MS_TEQ_SWORD_BIT(sObservedSword);
            MergeShipTest_Log("TERMINAEQ sword %d taken by MM (swords=%X)", sObservedSword, OOT_EQUIP.terminaSwords);
        } else if ((sObservedSword == EQUIP_VALUE_SWORD_RAZOR) && (sword == EQUIP_VALUE_SWORD_KOKIRI)) {
            // MM func_8083FFEC (the Razor Sword broke) or Sram_SaveEndOfCycle (the Song of Time): Kokiri again.
            OOT_EQUIP.terminaSwords &= ~MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_RAZOR);
            MergeShipTest_Log("TERMINAEQ Razor Sword gone (health=%d swords=%X)", PLAYER_DATA.swordHealth,
                              OOT_EQUIP.terminaSwords);
        }
    }
    if (MsTeq_IsShieldValue(sObservedShield) && (shield == EQUIP_VALUE_SHIELD_NONE)) {
        // MM took it (a Like Like: Inventory_DeleteEquipment).
        OOT_EQUIP.terminaShields &= ~MS_TEQ_SHIELD_BIT(sObservedShield);
        MergeShipTest_Log("TERMINAEQ shield %d taken by MM (shields=%X)", sObservedShield, OOT_EQUIP.terminaShields);
    }
    MsTeq_Record();
    sObservedSword = sword;
    sObservedShield = shield;
}

// MM puts a sword or shield it gives in use (Item_GiveImpl). With "Don't Auto-Equip New Swords and Shields" (the user,
// 2026-09-25: "dont auto equip swords/shields when u get them") it is only owned: the one in use before stays.
static s32 sMsTeqPreGive = false;
static u8 sMsTeqPreGiveItem;
static s32 sMsTeqPreSword;
static s32 sMsTeqPreShield;
static u8 sMsTeqPreB;
static u8 sMsTeqPreFormB;

static s32 MsTeq_IsGear(u8 item) {
    return (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE) || (MsTeq_ShieldValue(item) != EQUIP_VALUE_SHIELD_NONE);
}

void MergeShip_TerminaEquip_BeforeItemGiven(u8 item) {
    sMsTeqPreGive = MsTeq_IsGear(item);
    if (!sMsTeqPreGive) {
        return;
    }
    sMsTeqPreGiveItem = item;
    sMsTeqPreSword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    sMsTeqPreShield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    sMsTeqPreB = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    sMsTeqPreFormB = BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_B);
}

void MergeShip_TerminaEquip_OnItemGiven(u8 item) {
    // MM's Item_GiveImpl has set the value already; recorded now in case another give follows before the next frame.
    if (MsTeq_IsGear(item)) {
        MsTeq_Record();
    }
    if (sMsTeqPreGive && (item == sMsTeqPreGiveItem) && CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1)) {
        if (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE) {
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, sMsTeqPreSword);
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = sMsTeqPreB;
            BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_B) = sMsTeqPreFormB;
            if (gPlayState != NULL) {
                Interface_LoadItemIcon(gPlayState, EQUIP_SLOT_B);
            }
        } else {
            SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, sMsTeqPreShield);
        }
        if ((gPlayState != NULL) && (GET_PLAYER(gPlayState) != NULL)) {
            Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
        }
        MsTeq_SyncObserver();
        MergeShipTest_Log("TERMINAEQ 0x%02X given, not equipped (sword %d, shield %d)", item, sMsTeqPreSword,
                          sMsTeqPreShield);
    }
    sMsTeqPreGive = false;
}

void MergeShip_TerminaEquip_OnEndOfCycle(void) {
    // MM keeps one sword, so the Song of Time only ever meets a Razor Sword in use; one held here besides the sword in
    // use goes too. 2S2H's "Do not reset Razor Sword" and a randomizer seed keep it (EndOfCycle.cpp, OnCycleSave.cpp).
    if (CVarGetInteger("gEnhancements.Cycle.DoNotResetRazorSword", 0) ||
        (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO)) {
        return;
    }
    if ((OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_RAZOR)) &&
        (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_RAZOR)) {
        OOT_EQUIP.terminaSwords &= ~MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_RAZOR);
        MergeShipTest_Log("TERMINAEQ Razor Sword gone at the Song of Time (swords=%X)", OOT_EQUIP.terminaSwords);
    }
}

// ---- equipping ------------------------------------------------------------------------------------------------------------

// MM's own sword `value` on the human's B, as MM's Item_GiveImpl puts it there; an Ocarina of Time sword or the Great
// Fairy's Sword on B is put away (MergeShip_OoTEquip_EquipSword with MM's own sets B to the sword of MM's value).
static void MsTeq_SetSword(PlayState* play, s32 value) {
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, value);
    if ((value == EQUIP_VALUE_SWORD_RAZOR) && (PLAYER_DATA.swordHealth <= 0)) {
        PLAYER_DATA.swordHealth = MS_TEQ_RAZOR_HEALTH;
    }
    MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_MM);
}

static void MsTeq_SetShield(PlayState* play, s32 value) {
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, value);
    // MM's own shield in use (an Ocarina of Time shield put away); the player's shield follows
    // (Player_SetEquipmentData).
    MergeShip_OoTEquip_EquipShield(play, MS_OOTEQ_SHIELD_MM);
}

s32 MergeShip_TerminaEquip_Equip(PlayState* play, u8 item) {
    if (!MergeShip_TerminaEquip_CanEquip(item)) {
        return false;
    }
    if (item == ITEM_SWORD_GREAT_FAIRY) {
        MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_GREAT_FAIRY);
    } else if (MsTeq_SwordValue(item) != EQUIP_VALUE_SWORD_NONE) {
        MsTeq_SetSword(play, MsTeq_SwordValue(item));
    } else {
        MsTeq_SetShield(play, MsTeq_ShieldValue(item));
    }
    MsTeq_SyncObserver();
    return true;
}

// ---- the shared save ------------------------------------------------------------------------------------------------------

#ifdef MERGESHIP_HOSTED

// The shared save's bit (MsShield) for MM's shield `value`: MM's own bit in the split model (MS_SHARED_ITEMS_MODEL_SPLIT),
// the bit it shared with Hyrule's shield before.
static u8 MsTeq_SharedShieldBit(const MsSharedSave* shared, s32 value) {
    if (shared->itemsModel >= MS_SHARED_ITEMS_MODEL_SPLIT) {
        return (u8)(1u << ((value == EQUIP_VALUE_SHIELD_HERO) ? MS_SHIELD_HERO : MS_SHIELD_MM_MIRROR));
    }
    return (u8)(1u << ((value == EQUIP_VALUE_SHIELD_HERO) ? MS_SHIELD_HYLIAN : MS_SHIELD_MIRROR));
}

void MergeShip_TerminaEquip_CaptureShared(MsSharedSave* shared) {
    s32 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);

    if (shared == NULL) {
        return;
    }
    if (!CVarGetInteger(MERGESHIP_TERMINA_EQUIP_CVAR, 1)) {
        // Nothing chosen here to bring back: the next apply leaves Termina's equipment as the rest of it sets it.
        shared->terminaGearValid = 0;
        return;
    }
    MsTeq_Record();
    shared->terminaSwords = OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BITS;
    shared->terminaShields = OOT_EQUIP.terminaShields & MS_TEQ_SHIELD_BITS;
    shared->terminaSword = (u8)(MsTeq_IsSwordValue(sword) ? sword : EQUIP_VALUE_SWORD_NONE);
    if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_GREAT_FAIRY)) {
        shared->terminaSword |= MS_TEQ_SHARED_GREAT_FAIRY_ON_B;
    }
    shared->terminaShield = (u8)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    // The Razor and Gilded Swords Termina holds are the shared chain's (as the capture adds the one in use), and the
    // Razor Sword keeps its hits left while another sword is in use.
    if (OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_KOKIRI)) {
        shared->swords |= (u8)(1u << MS_SWORD_MM_KOKIRI); // MM's own Kokiri Sword (2026-09-24)
    }
    if (OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_RAZOR)) {
        shared->swords |= (u8)(1u << MS_SWORD_RAZOR);
        shared->razorSwordHealth = (u8)PLAYER_DATA.swordHealth;
    }
    if (OOT_EQUIP.terminaSwords & MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_GILDED)) {
        shared->swords |= (u8)((1u << MS_SWORD_RAZOR) | (1u << MS_SWORD_GILDED));
    }
    // (MM's shields Termina holds go to `shields` in the capture's own equipment section, through
    // MergeShip_TerminaEquip_Owns.)
    // Everything the capture wrote: what Hyrule adds after this is new to Termina (MergeShip_TerminaEquip_ApplyShared).
    shared->terminaSeenSwords = shared->swords;
    shared->terminaSeenShields = shared->shields;
    shared->terminaGearValid = 1;
    MergeShipTest_Log("TERMINAEQ capture swords=%X shields=%X sword=0x%02X shield=%d razor=%d", shared->terminaSwords,
                      shared->terminaShields, shared->terminaSword, shared->terminaShield, shared->razorSwordHealth);
}

void MergeShip_TerminaEquip_ApplyShared(const MsSharedSave* shared) {
    u8 newSwords;
    u8 newShields;
    s32 sword;
    s32 shield;
    s32 want;
    s32 swordIsNew;
    s32 shieldIsNew;

    if ((shared == NULL) || !CVarGetInteger(MERGESHIP_TERMINA_EQUIP_CVAR, 1)) {
        MsTeq_SyncObserver();
        return;
    }
    // What Hyrule added since Termina's capture (the shared save's own apply equips the best of it; with the seed's
    // "Combine Shared Items", Hyrule's Kokiri Sword and shields count as MM's, SharedSave.h MsShared_Combine*).
    // Before Termina's first capture, everything Hyrule holds is new to it (BUG-0008: MM's swords found in Hyrule
    // before the first visit to Termina were never Termina's).
    newSwords = MsShared_CombineSwords(shared->swords, shared->itemsCombined) &
                ~(shared->terminaGearValid ? MsShared_CombineSwords(shared->terminaSeenSwords, shared->itemsCombined) : 0);
    newShields = MsShared_CombineShields(shared->shields, shared->itemsCombined) &
                 ~(shared->terminaGearValid ? MsShared_CombineShields(shared->terminaSeenShields, shared->itemsCombined)
                                            : 0);
    if (shared->terminaGearValid) {
        OOT_EQUIP.terminaSwords |= shared->terminaSwords & MS_TEQ_SWORD_BITS;
        OOT_EQUIP.terminaShields |= shared->terminaShields & MS_TEQ_SHIELD_BITS;
    }
    // With "Don't Auto-Equip New Swords and Shields" (the shared save's apply equips none of them): Termina's own now,
    // on the Termina Equipment view.
    if (CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1)) {
        if (newSwords & (1u << MS_SWORD_MM_KOKIRI)) {
            OOT_EQUIP.terminaSwords |= MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_KOKIRI);
        }
        if (newSwords & (1u << MS_SWORD_RAZOR)) {
            OOT_EQUIP.terminaSwords |= MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_RAZOR);
        }
        if (newSwords & (1u << MS_SWORD_GILDED)) {
            OOT_EQUIP.terminaSwords |= MS_TEQ_SWORD_BIT(EQUIP_VALUE_SWORD_GILDED);
        }
        if (newShields & MsTeq_SharedShieldBit(shared, EQUIP_VALUE_SHIELD_HERO)) {
            OOT_EQUIP.terminaShields |= MS_TEQ_SHIELD_BIT(EQUIP_VALUE_SHIELD_HERO);
        }
        if (newShields & MsTeq_SharedShieldBit(shared, EQUIP_VALUE_SHIELD_MIRROR)) {
            OOT_EQUIP.terminaShields |= MS_TEQ_SHIELD_BIT(EQUIP_VALUE_SHIELD_MIRROR);
        }
    }

    // The sword: kept when the apply just equipped a new one (MM equips a sword it gives), else the one Termina had.
    sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    // (MM's Kokiri Sword is MS_SWORD_MM_KOKIRI; Razor and Gilded follow MS_SWORD_KOKIRI's run.)
    swordIsNew = MsTeq_IsSwordValue(sword) &&
                 (newSwords & ((sword == EQUIP_VALUE_SWORD_KOKIRI)
                                   ? (1u << MS_SWORD_MM_KOKIRI)
                                   : (1u << (MS_SWORD_KOKIRI + sword - EQUIP_VALUE_SWORD_KOKIRI))));
    want = shared->terminaSword & ~MS_TEQ_SHARED_GREAT_FAIRY_ON_B;
    if (shared->terminaGearValid && !swordIsNew && (sword != EQUIP_VALUE_SWORD_DIETY) && MsTeq_OwnsSwordValue(want) &&
        (want != sword)) {
        u8 bItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);

        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, want);
        // The human's B shows it, unless something else than MM's sword is there (Ocarina of Time's sword stays: its
        // selection is this file's own).
        if ((bItem == ITEM_NONE) || ((bItem >= ITEM_SWORD_KOKIRI) && (bItem <= ITEM_SWORD_GILDED))) {
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_SWORD_KOKIRI + want - EQUIP_VALUE_SWORD_KOKIRI;
        }
    }
    if ((GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR) && (shared->razorSwordHealth != 0)) {
        PLAYER_DATA.swordHealth = shared->razorSwordHealth;
    }
    // The Great Fairy's Sword on B, as it was.
    if (shared->terminaGearValid && MergeShip_OoTEquip_Enabled()) {
        s32 gfsOnB = (shared->terminaSword & MS_TEQ_SHARED_GREAT_FAIRY_ON_B) && !swordIsNew && MsTeq_OwnsGreatFairySword();

        if (gfsOnB && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_MM)) {
            MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_GREAT_FAIRY);
        } else if (!gfsOnB && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_GREAT_FAIRY)) {
            MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_MM);
        }
    }

    // The shield: likewise (MsTeq_SharedShieldBit: MM's shield's bit in the shared save).
    shield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    shieldIsNew = MsTeq_IsShieldValue(shield) && (newShields & MsTeq_SharedShieldBit(shared, shield));
    want = shared->terminaShield;
    if (shared->terminaGearValid && !shieldIsNew && MsTeq_OwnsShieldValue(want) && (want != shield)) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, want);
    }

    MsTeq_SyncObserver();
    MergeShipTest_Log("TERMINAEQ apply swords=%X shields=%X sword=%d shield=%d (new: swords=%X shields=%X) B=0x%02X "
                      "ootSword=%d razor=%d",
                      OOT_EQUIP.terminaSwords, OOT_EQUIP.terminaShields, GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD),
                      GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD), newSwords, newShields, BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B),
                      OOT_EQUIP.sword, PLAYER_DATA.swordHealth);
}

// BUG-0120: the sword on the human's B, as the shared save names it: an Ocarina of Time sword or the Great Fairy's
// Sword through the Hyrule Equipment view, else Majora's Mask's own in use.
void MergeShip_TerminaEquip_CaptureBSword(MsSharedSave* shared) {
    u8 code = MS_BSWORD_NONE;

    if (shared == NULL) {
        return;
    }
    if (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_NONE) {
        code = MS_BSWORD_NONE;
    } else if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_KOKIRI)) {
        code = MS_BSWORD_OOT_KOKIRI;
    } else if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_MASTER)) {
        code = MS_BSWORD_OOT_MASTER;
    } else if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG)) {
        code = MS_BSWORD_OOT_BIGGORON;
    } else if ((MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_GREAT_FAIRY)) ||
               (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_GREAT_FAIRY)) {
        code = MS_BSWORD_MM_GREAT_FAIRY;
    } else {
        switch (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) {
            case EQUIP_VALUE_SWORD_KOKIRI:
                code = MS_BSWORD_MM_KOKIRI;
                break;
            case EQUIP_VALUE_SWORD_RAZOR:
                code = MS_BSWORD_MM_RAZOR;
                break;
            case EQUIP_VALUE_SWORD_GILDED:
                code = MS_BSWORD_MM_GILDED;
                break;
            default:
                break;
        }
    }
    shared->bSword = code;
    shared->bWriter = MS_ENGINE_2SHIP;
}

void MergeShip_TerminaEquip_ApplyBSword(const MsSharedSave* shared) {
    s32 value = EQUIP_VALUE_SWORD_NONE;

    if ((shared == NULL) || (shared->bWriter != MS_ENGINE_SOH)) {
        return;
    }
    ((MsSharedSave*)shared)->bWriter = 0; // once
    switch (shared->bSword) {
        case MS_BSWORD_OOT_KOKIRI:
            if (MergeShip_OoTEquip_Enabled()) {
                MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_KOKIRI);
            }
            break;
        case MS_BSWORD_OOT_MASTER:
            if (MergeShip_OoTEquip_Enabled()) {
                MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_MASTER);
            }
            break;
        case MS_BSWORD_OOT_BIGGORON:
            if (MergeShip_OoTEquip_Enabled()) {
                MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_LONG);
            }
            break;
        case MS_BSWORD_MM_GREAT_FAIRY:
            if (MergeShip_OoTEquip_Enabled() && MsTeq_OwnsGreatFairySword()) {
                MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_GREAT_FAIRY);
            }
            break;
        case MS_BSWORD_MM_KOKIRI:
            value = EQUIP_VALUE_SWORD_KOKIRI;
            break;
        case MS_BSWORD_MM_RAZOR:
            value = EQUIP_VALUE_SWORD_RAZOR;
            break;
        case MS_BSWORD_MM_GILDED:
            value = EQUIP_VALUE_SWORD_GILDED;
            break;
        default:
            break;
    }
    // Majora's Mask's own sword: as its pause screen equips it (the human's B shows it).
    if ((value != EQUIP_VALUE_SWORD_NONE) && MsTeq_OwnsSwordValue(value)) {
        if (MergeShip_OoTEquip_Enabled() && (OOT_EQUIP.sword != MS_OOTEQ_SWORD_MM)) {
            MergeShip_OoTEquip_EquipSword(gPlayState, MS_OOTEQ_SWORD_MM);
        }
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, value);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_SWORD_KOKIRI + value - EQUIP_VALUE_SWORD_KOKIRI;
    }
    MergeShipHostTest_Log("BSWORD from Hyrule: code %d -> B 0x%02X ootSword=%d sword=%d", shared->bSword,
                      BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), OOT_EQUIP.sword, GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD));
}

#endif // MERGESHIP_HOSTED
