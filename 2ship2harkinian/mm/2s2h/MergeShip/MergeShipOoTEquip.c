// MergeShip2 -- Ocarina of Time's equipment in Majora's Mask: ownership, giving, equipping and what it does to the
// player. See MergeShipOoTEquip.h.

#include "global.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOwnership.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)
// The Bullet Bag and the seeds belong to Ocarina of Time's items (MergeShipOoTItems.h), which own the Slingshot.
#define OOT_ITEMS (gSaveContext.save.shipSaveInfo.mergeShipOoTItems)

#define LONG_SWORD_BITS (MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_BIGGORON) | MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_GIANTS_KNIFE))

void MergeShip_OoTEquip_UpdateTunicColor(s32 tunic); // MergeShipOoTEquip.cpp
void MergeShipTest_Log(const char* fmt, ...);

s32 MergeShip_OoTEquip_Enabled(void) {
    return CVarGetInteger(MERGESHIP_OOT_EQUIP_CVAR, 1) && MergeShip_IsOcarinaMounted();
}

MergeShipOoTEquipSave* MergeShip_OoTEquip_Save(void) {
    return &OOT_EQUIP;
}

// ---- ownership ----------------------------------------------------------------------------------------------------------

static s32 MsEq_OwnsLongSword(void) {
    return (OOT_EQUIP.owned & LONG_SWORD_BITS) != 0;
}

s32 MergeShip_OoTEquip_Owns(u8 item) {
    switch (item) {
        case ITEM_OOT_TUNIC_KOKIRI:
        case ITEM_OOT_BOOTS_KOKIRI:
            return true; // Ocarina of Time: always owned
        case ITEM_OOT_SWORD_BIGGORON:
            return MsEq_OwnsLongSword() && OOT_EQUIP.bgsFlag;
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
            return MsEq_OwnsLongSword() && !OOT_EQUIP.bgsFlag && (OOT_EQUIP.swordHealth > 0);
        case ITEM_OOT_SWORD_BROKEN:
            return MsEq_OwnsLongSword() && !OOT_EQUIP.bgsFlag && (OOT_EQUIP.swordHealth <= 0);
        case ITEM_OOT_BRACELET:
            return OOT_EQUIP.strength >= 1;
        case ITEM_OOT_GAUNTLETS_SILVER:
            return OOT_EQUIP.strength >= 2;
        case ITEM_OOT_GAUNTLETS_GOLD:
            return OOT_EQUIP.strength >= 3;
        case ITEM_OOT_SCALE_SILVER:
            return OOT_EQUIP.scale >= 1;
        case ITEM_OOT_SCALE_GOLDEN:
            return OOT_EQUIP.scale >= 2;
        case ITEM_OOT_BULLET_BAG:
            return OOT_ITEMS.bulletBag > 0;
        default:
            if (ITEM_IS_OOT_EQUIP(item)) {
                return (OOT_EQUIP.owned & MS_OOTEQ_OWN_BIT(item)) != 0;
            }
            return false;
    }
}

static u8 MsEq_LongSwordItem(void) {
    if (OOT_EQUIP.bgsFlag) {
        return ITEM_OOT_SWORD_BIGGORON;
    }
    return (OOT_EQUIP.swordHealth > 0) ? ITEM_OOT_SWORD_GIANTS_KNIFE : ITEM_OOT_SWORD_BROKEN;
}

static u8 MsEq_MMSwordItem(void) {
    s32 value = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);

    if ((value >= EQUIP_VALUE_SWORD_KOKIRI) && (value <= EQUIP_VALUE_SWORD_GILDED)) {
        return ITEM_SWORD_KOKIRI + value - EQUIP_VALUE_SWORD_KOKIRI;
    }
    return ITEM_NONE;
}

u8 MergeShip_OoTEquip_SwordItem(s32 sword) {
    switch (sword) {
        case MS_OOTEQ_SWORD_MASTER:
            return ITEM_OOT_SWORD_MASTER;
        case MS_OOTEQ_SWORD_KOKIRI:
            return ITEM_OOT_SWORD_KOKIRI;
        case MS_OOTEQ_SWORD_GREAT_FAIRY:
            return ITEM_SWORD_GREAT_FAIRY;
        case MS_OOTEQ_SWORD_LONG:
            return MsEq_LongSwordItem();
        default:
            return MsEq_MMSwordItem();
    }
}

u8 MergeShip_OoTEquip_ShieldItem(s32 shield) {
    s32 value;

    switch (shield) {
        case MS_OOTEQ_SHIELD_DEKU:
            return ITEM_OOT_SHIELD_DEKU;
        case MS_OOTEQ_SHIELD_HYLIAN:
            return ITEM_OOT_SHIELD_HYLIAN;
        case MS_OOTEQ_SHIELD_MIRROR:
            return ITEM_OOT_SHIELD_MIRROR;
        default:
            value = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
            return (value != EQUIP_VALUE_SHIELD_NONE) ? (ITEM_SHIELD_HERO + value - EQUIP_VALUE_SHIELD_HERO) : ITEM_NONE;
    }
}

static s32 MsEq_OwnsSword(s32 sword) {
    switch (sword) {
        case MS_OOTEQ_SWORD_MASTER:
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_MASTER);
        case MS_OOTEQ_SWORD_KOKIRI:
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_KOKIRI);
        case MS_OOTEQ_SWORD_GREAT_FAIRY:
            return INV_CONTENT(ITEM_SWORD_GREAT_FAIRY) == ITEM_SWORD_GREAT_FAIRY;
        case MS_OOTEQ_SWORD_LONG:
            return MsEq_OwnsLongSword();
        default:
            return true;
    }
}

static s32 MsEq_OwnsShield(s32 shield) {
    u8 item = MergeShip_OoTEquip_ShieldItem(shield);

    return (shield == MS_OOTEQ_SHIELD_MM) || MergeShip_OoTEquip_Owns(item);
}

// ---- giving -------------------------------------------------------------------------------------------------------------

u8 MergeShip_OoTEquip_Give(PlayState* play, u8 item) {
    static const u8 sSeedCapacity[] = { 0, 30, 40, 50 }; // OoT gUpgradeCapacities[UPG_BULLET_BAG]

    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
            // OoT Item_Give: the Master Sword goes on B (not with "Don't Auto-Equip New Swords and Shields").
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(item);
            if (!CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1)) {
                MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_MASTER);
            }
            break;
        case ITEM_OOT_SWORD_KOKIRI:
            // Ocarina of Time's Kokiri Sword: on B only when Link has no sword there (MM's stays in use otherwise).
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(item);
            if ((BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_NONE) && !CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1)) {
                MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_KOKIRI);
            }
            break;
        case ITEM_OOT_SWORD_BIGGORON:
            // OoT: the Giant's Knife and Biggoron's Sword are one item; Biggoron's never breaks (bgsFlag).
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(item);
            OOT_EQUIP.bgsFlag = true;
            OOT_EQUIP.swordHealth = MS_OOTEQ_KNIFE_HEALTH;
            break;
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(item);
            OOT_EQUIP.swordHealth = MS_OOTEQ_KNIFE_HEALTH;
            break;
        case ITEM_OOT_SWORD_BROKEN:
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(ITEM_OOT_SWORD_GIANTS_KNIFE);
            if (!OOT_EQUIP.bgsFlag) {
                OOT_EQUIP.swordHealth = 0;
            }
            break;
        case ITEM_OOT_BRACELET:
            OOT_EQUIP.strength = MAX(OOT_EQUIP.strength, 1);
            break;
        case ITEM_OOT_GAUNTLETS_SILVER:
            OOT_EQUIP.strength = MAX(OOT_EQUIP.strength, 2);
            break;
        case ITEM_OOT_GAUNTLETS_GOLD:
            OOT_EQUIP.strength = 3;
            break;
        case ITEM_OOT_SCALE_SILVER:
            OOT_EQUIP.scale = MAX(OOT_EQUIP.scale, 1);
            break;
        case ITEM_OOT_SCALE_GOLDEN:
            OOT_EQUIP.scale = 2;
            break;
        case ITEM_OOT_BULLET_BAG:
            // OoT ITEM_BULLET_BAG_30/40/50, progressive: the next bag, full of seeds.
            if (OOT_ITEMS.bulletBag < 3) {
                OOT_ITEMS.bulletBag++;
            }
            OOT_ITEMS.seeds = sSeedCapacity[OOT_ITEMS.bulletBag];
            break;
        default:
            // Shields, tunics and boots: owned, not equipped (OoT Item_Give).
            OOT_EQUIP.owned |= MS_OOTEQ_OWN_BIT(item);
            break;
    }

    // The long sword on B shows its new state (OoT: a new knife replaces the broken one on B).
    if ((play != NULL) && (OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG) && (CUR_FORM == 0) &&
        ITEM_IS_OOT_EQUIP_SWORD(BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B))) {
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = MsEq_LongSwordItem();
        Interface_LoadItemIcon(play, EQUIP_SLOT_B);
    }
    MergeShipTest_Log("OOTEQ give 0x%02X owned=%08X strength=%d scale=%d bgs=%d health=%d bag=%d", item,
                      OOT_EQUIP.owned, OOT_EQUIP.strength, OOT_EQUIP.scale, OOT_EQUIP.bgsFlag, OOT_EQUIP.swordHealth,
                      OOT_ITEMS.bulletBag);
    return ITEM_NONE;
}

u8 MergeShip_OoTEquip_CheckObtainability(u8 item) {
    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
        case ITEM_OOT_SWORD_KOKIRI:
        case ITEM_OOT_SHIELD_HYLIAN:
        case ITEM_OOT_SHIELD_MIRROR:
        case ITEM_OOT_TUNIC_GORON:
        case ITEM_OOT_TUNIC_ZORA:
        case ITEM_OOT_BOOTS_IRON:
        case ITEM_OOT_BOOTS_HOVER:
            // MM's rule for its shields: an owned one cannot be obtained again.
            return MergeShip_OoTEquip_Owns(item) ? item : ITEM_NONE;
        default:
            return ITEM_NONE;
    }
}

void MergeShip_OoTEquip_OnMMItemGiven(u8 item) {
    // (Majora's Mask's own bombs .. beans: MergeShipOwnership.h, its own hooks.)
    // With "Don't Auto-Equip New Swords and Shields" the one in use before stays (MergeShip_TerminaEquip_OnItemGiven).
    if (CVarGetInteger("gEnhancements.MergeShip.NoAutoEquipGear", 1)) {
        return;
    }
    // MM equips its swords and shields when it gives them (Item_GiveImpl sets B / the shield).
    if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED)) {
        OOT_EQUIP.sword = MS_OOTEQ_SWORD_MM;
        OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_MASTER;
    } else if (item == ITEM_SHIELD_HERO) {
        OOT_EQUIP.shield = MS_OOTEQ_SHIELD_MM;
        OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_HYLIAN;
    } else if (item == ITEM_SHIELD_MIRROR) {
        OOT_EQUIP.shield = MS_OOTEQ_SHIELD_MM;
        OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_OOT_MIRROR;
    }
}

// ---- equipping ----------------------------------------------------------------------------------------------------------

static s32 MsEq_IsMMBSword(u8 item) {
    return (item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_GILDED);
}

s32 MergeShip_OoTEquip_IsBSwordItem(u8 item) {
    return MsEq_IsMMBSword(item) || ITEM_IS_OOT_EQUIP_SWORD(item) || (item == ITEM_SWORD_GREAT_FAIRY);
}

static void MsEq_SetBSword(PlayState* play, u8 item) {
    if (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == item) {
        return;
    }
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = item;
    if ((play != NULL) && (CUR_FORM == 0)) {
        Interface_LoadItemIcon(play, EQUIP_SLOT_B);
    }
}

static void MsEq_RefreshPlayer(PlayState* play) {
    Player* player;

    if ((play == NULL) || (gPlayState == NULL)) {
        return;
    }
    player = GET_PLAYER(play);
    if ((player != NULL) && (player->actor.id == ACTOR_PLAYER)) {
        Player_SetEquipmentData(play, player);
    }
}

s32 MergeShip_OoTEquip_EquipSword(PlayState* play, s32 sword) {
    if ((sword < 0) || (sword >= MS_OOTEQ_SWORD_MAX) || !MsEq_OwnsSword(sword)) {
        return false;
    }
    OOT_EQUIP.sword = sword;
    if (sword == MS_OOTEQ_SWORD_MASTER) {
        OOT_EQUIP.variants |= MS_OOTEQ_VARIANT_MASTER;
    } else if (sword == MS_OOTEQ_SWORD_MM) {
        OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_MASTER;
    } else if (sword == MS_OOTEQ_SWORD_LONG) {
        OOT_EQUIP.variants |= MS_OOTEQ_VARIANT_BIGGORON;
    } else if (sword == MS_OOTEQ_SWORD_GREAT_FAIRY) {
        OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_BIGGORON;
    }
    MsEq_SetBSword(play, MergeShip_OoTEquip_SwordItem(sword));
    MsEq_RefreshPlayer(play);
    MergeShipTest_Log("OOTEQ sword=%d B=0x%02X", sword, BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B));
    return true;
}

s32 MergeShip_OoTEquip_EquipShield(PlayState* play, s32 shield) {
    if ((shield < 0) || (shield >= MS_OOTEQ_SHIELD_MAX) || !MsEq_OwnsShield(shield)) {
        return false;
    }
    OOT_EQUIP.shield = shield;
    if (shield == MS_OOTEQ_SHIELD_HYLIAN) {
        OOT_EQUIP.variants |= MS_OOTEQ_VARIANT_HYLIAN;
    } else if (shield == MS_OOTEQ_SHIELD_MIRROR) {
        OOT_EQUIP.variants |= MS_OOTEQ_VARIANT_OOT_MIRROR;
    } else if (shield == MS_OOTEQ_SHIELD_MM) {
        if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HERO) {
            OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_HYLIAN;
        } else if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) {
            OOT_EQUIP.variants &= ~MS_OOTEQ_VARIANT_OOT_MIRROR;
        }
    }
    MsEq_RefreshPlayer(play);
    MergeShipTest_Log("OOTEQ shield=%d", shield);
    return true;
}

s32 MergeShip_OoTEquip_EquipTunic(s32 tunic) {
    static const u8 sTunicItems[] = { ITEM_OOT_TUNIC_KOKIRI, ITEM_OOT_TUNIC_GORON, ITEM_OOT_TUNIC_ZORA };

    if ((tunic < 0) || (tunic > 2) || !MergeShip_OoTEquip_Owns(sTunicItems[tunic])) {
        return false;
    }
    OOT_EQUIP.tunic = tunic;
    MergeShipTest_Log("OOTEQ tunic=%d", tunic);
    return true;
}

s32 MergeShip_OoTEquip_EquipBoots(PlayState* play, s32 boots) {
    static const u8 sBootItems[] = { ITEM_OOT_BOOTS_KOKIRI, ITEM_OOT_BOOTS_IRON, ITEM_OOT_BOOTS_HOVER };

    if ((boots < 0) || (boots > 2) || !MergeShip_OoTEquip_Owns(sBootItems[boots])) {
        return false;
    }
    OOT_EQUIP.boots = boots;
    MsEq_RefreshPlayer(play);
    MergeShipTest_Log("OOTEQ boots=%d", boots);
    return true;
}

s32 MergeShip_OoTEquip_IsWearableItem(u8 item) {
    return ((item >= ITEM_OOT_TUNIC_KOKIRI) && (item <= ITEM_OOT_TUNIC_ZORA)) ||
           ((item >= ITEM_OOT_BOOTS_KOKIRI) && (item <= ITEM_OOT_BOOTS_HOVER));
}

s32 MergeShip_OoTEquip_UseWearable(PlayState* play, Player* player, u8 item) {
    (void)play;
    (void)player;
    return MergeShip_OoTEquip_IsWearableItem(item); // the D-pad hook toggles it (MergeShipDpadBoots.cpp)
}

s32 MergeShip_OoTEquip_ToggleWearable(PlayState* play, Player* player, u8 item) {
    const s32 isBoots = (item >= ITEM_OOT_BOOTS_KOKIRI) && (item <= ITEM_OOT_BOOTS_HOVER);
    s32 value;
    s32 ok;

    if (!MergeShip_OoTEquip_IsWearableItem(item)) {
        return false;
    }
    // The human wears Ocarina of Time's tunics and boots; a form keeps its own body.
    if (!MergeShip_OoTEquip_Enabled() || (player->transformation != PLAYER_FORM_HUMAN)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return false;
    }
    if (!isBoots) {
        value = item - ITEM_OOT_TUNIC_KOKIRI;
        ok = MergeShip_OoTEquip_EquipTunic((OOT_EQUIP.tunic == value) ? 0 : value);
    } else {
        value = item - ITEM_OOT_BOOTS_KOKIRI;
        value = (OOT_EQUIP.boots == value) ? 0 : value;
        ok = MergeShip_OoTEquip_EquipBoots(play, value);
    }
    if (!ok) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
    } else if (isBoots && (value == 1)) {
        Player_PlaySfx(player, NA_SE_PL_LAND_HEAVYBOOTS);
    } else {
        Player_PlaySfx(player, NA_SE_PL_CHANGE_ARMS);
    }
    return ok;
}

void MergeShip_OoTEquip_ApplyToPlayer(Player* player) {
    if (!MergeShip_OoTEquip_Enabled() || (player->actor.id != ACTOR_PLAYER) ||
        (player->transformation != PLAYER_FORM_HUMAN)) {
        return;
    }
    // The shield's behaviour: the Deku and Hylian Shields block as the Hero's Shield, Ocarina of Time's Mirror Shield
    // reflects as MM's (Player_HasMirrorShieldEquipped / Player_IsHoldingMirrorShield).
    switch (OOT_EQUIP.shield) {
        case MS_OOTEQ_SHIELD_DEKU:
        case MS_OOTEQ_SHIELD_HYLIAN:
            player->currentShield = PLAYER_SHIELD_HEROS_SHIELD;
            break;
        case MS_OOTEQ_SHIELD_MIRROR:
            player->currentShield = PLAYER_SHIELD_MIRROR_SHIELD;
            break;
        default:
            break;
    }
    // The boots (the Giant's Mask keeps its own).
    if ((player->currentBoots == PLAYER_BOOTS_HYLIAN) || (player->currentBoots == PLAYER_BOOTS_IRON) ||
        (player->currentBoots == PLAYER_BOOTS_HOVER)) {
        player->currentBoots = (OOT_EQUIP.boots == 1)   ? PLAYER_BOOTS_IRON
                               : (OOT_EQUIP.boots == 2) ? PLAYER_BOOTS_HOVER
                                                        : PLAYER_BOOTS_HYLIAN;
    }
}

// ---- every frame --------------------------------------------------------------------------------------------------------

static s32 sHoverBootsTimer = 0; // OoT Player::hoverBootsTimer
static s32 sWasEnabled = false;

void MergeShip_OoTEquip_Update(PlayState* play) {
    MergeShip_Own_Sync(); // Hyrule's bombs .. beans, bow and bags held in Majora's Mask's slots
    Player* player = GET_PLAYER(play);
    u8 bItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    s32 enabled = MergeShip_OoTEquip_Enabled();
    s32 isHuman = (GET_PLAYER_FORM == PLAYER_FORM_HUMAN);

    if (!enabled) {
        if (sWasEnabled) {
            // The option went off: Majora's Mask's own sword, shield, tunic and boots again.
            if (ITEM_IS_OOT_EQUIP_SWORD(bItem) ||
                ((OOT_EQUIP.sword == MS_OOTEQ_SWORD_GREAT_FAIRY) && (bItem == ITEM_SWORD_GREAT_FAIRY))) {
                MsEq_SetBSword(play, MsEq_MMSwordItem());
            }
            SET_EQUIP_VALUE(EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_NONE);
            MergeShip_OoTEquip_UpdateTunicColor(0);
            sWasEnabled = false;
            MsEq_RefreshPlayer(play);
        }
        return;
    }
    sWasEnabled = true;

    // Selections of what is no longer owned (a broken Razor Sword, a burnt shield, a stolen sword) fall back to MM's.
    if (!MsEq_OwnsSword(OOT_EQUIP.sword)) {
        OOT_EQUIP.sword = MS_OOTEQ_SWORD_MM;
    }
    if (!MsEq_OwnsShield(OOT_EQUIP.shield)) {
        OOT_EQUIP.shield = MS_OOTEQ_SHIELD_MM;
        MsEq_RefreshPlayer(play);
    }
    if ((OOT_EQUIP.tunic > 2) || ((OOT_EQUIP.tunic == 1) && !MergeShip_OoTEquip_Owns(ITEM_OOT_TUNIC_GORON)) ||
        ((OOT_EQUIP.tunic == 2) && !MergeShip_OoTEquip_Owns(ITEM_OOT_TUNIC_ZORA))) {
        OOT_EQUIP.tunic = 0;
    }
    if ((OOT_EQUIP.boots > 2) || ((OOT_EQUIP.boots == 1) && !MergeShip_OoTEquip_Owns(ITEM_OOT_BOOTS_IRON)) ||
        ((OOT_EQUIP.boots == 2) && !MergeShip_OoTEquip_Owns(ITEM_OOT_BOOTS_HOVER))) {
        OOT_EQUIP.boots = 0;
    }

    // The human's B: MM puts its own sword back on B in places (end of a minigame, the three-day reset); the selected
    // Ocarina of Time sword stays there. MM's other temporary B items (bow, bombs) are left alone.
    if (OOT_EQUIP.sword != MS_OOTEQ_SWORD_MM) {
        u8 want = MergeShip_OoTEquip_SwordItem(OOT_EQUIP.sword);

        if ((bItem != want) &&
            (MsEq_IsMMBSword(bItem) || ITEM_IS_OOT_EQUIP_SWORD(bItem) || (bItem == ITEM_SWORD_GREAT_FAIRY) ||
             ((bItem == ITEM_NONE) && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE)))) {
            MsEq_SetBSword(play, want);
        }
    } else if (ITEM_IS_OOT_EQUIP_SWORD(bItem)) {
        MsEq_SetBSword(play, MsEq_MMSwordItem());
    }

    // MM's hazard update (z_parameter.c) reads Ocarina of Time's tunic: only the human wears it.
    SET_EQUIP_VALUE(EQUIP_TYPE_TUNIC, isHuman ? (EQUIP_VALUE_TUNIC_KOKIRI + OOT_EQUIP.tunic) : EQUIP_VALUE_TUNIC_KOKIRI);
    MergeShip_OoTEquip_UpdateTunicColor(OOT_EQUIP.tunic);

    if ((player != NULL) && (player->actor.id == ACTOR_PLAYER)) {
        s32 prevBoots = player->currentBoots;

        MergeShip_OoTEquip_ApplyToPlayer(player);
        if (player->currentBoots != prevBoots) {
            func_80123140(play, player);
        }
    }
}

// ---- the player ---------------------------------------------------------------------------------------------------------

s32 MergeShip_OoTEquip_ItemAction(Player* player, u8 item) {
    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
            // One-handed, with the Gilded Sword's reach (Ocarina of Time's 4000); damage in MergeShip_OoTEquip_MeleeDamage.
            return PLAYER_IA_SWORD_GILDED;
        case ITEM_OOT_SWORD_KOKIRI:
            // The same sword as MM's Kokiri Sword (reach, 1 / 2 damage), drawn as Ocarina of Time's (MergeShipOoTEquipDraw.c).
            return PLAYER_IA_SWORD_KOKIRI;
        case ITEM_OOT_SWORD_BIGGORON:
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
        case ITEM_OOT_SWORD_BROKEN:
            // Two-handed (OoT PLAYER_IA_SWORD_BIGGORON), 5500 reach and 4 / 8 damage, as MM's two-handed sword.
            return PLAYER_IA_SWORD_TWO_HANDED;
        default:
            return PLAYER_IA_NONE;
    }
}

static s32 MsEq_HoldsOoTSword(Player* player, u8 item) {
    return (player->actor.id == ACTOR_PLAYER) && (player->transformation == PLAYER_FORM_HUMAN) &&
           (player->heldItemId == item);
}

s32 MergeShip_OoTEquip_HoldsOoTMirrorShield(Player* player) {
    return MergeShip_OoTEquip_Enabled() && (player->transformation == PLAYER_FORM_HUMAN) &&
           (OOT_EQUIP.shield == MS_OOTEQ_SHIELD_MIRROR);
}

s32 MergeShip_OoTEquip_HoldsBrokenKnife(Player* player) {
    // OoT Player_HoldsBrokenKnife: the long sword in hand with no health left.
    return MergeShip_OoTEquip_Enabled() && (player->actor.id == ACTOR_PLAYER) &&
           (player->transformation == PLAYER_FORM_HUMAN) && (player->heldItemAction == PLAYER_IA_SWORD_TWO_HANDED) &&
           ITEM_IS_OOT_EQUIP_SWORD(player->heldItemId) && !OOT_EQUIP.bgsFlag && (OOT_EQUIP.swordHealth <= 0);
}

s32 MergeShip_OoTEquip_MeleeDamage(Player* player, s32 strong, s32 damage) {
    if (!MergeShip_OoTEquip_Enabled()) {
        return damage;
    }
    // OoT damage flags, in MM's damage units: Master 2 (jump / spin 4); Biggoron's Sword and the Giant's Knife 4 (8),
    // MM's two-handed sword's; the broken knife slashes as the Kokiri Sword, 1 (2) (OoT func_80837948: temp = 1).
    if (MsEq_HoldsOoTSword(player, ITEM_OOT_SWORD_MASTER)) {
        return strong ? 4 : 2;
    }
    if (MergeShip_OoTEquip_HoldsBrokenKnife(player)) {
        return strong ? 2 : 1;
    }
    return damage;
}

s32 MergeShip_OoTEquip_SpinWaveType(Player* player, s32 type) {
    // OoT func_80837530: the wave carries the sword's own damage class (DMG_SPIN_MASTER); in MM's En_M_Thunder terms
    // that is the Razor Sword's (1), the Master Sword's slashes being the Razor Sword's 2 / 4.
    if (MergeShip_OoTEquip_Enabled() && MsEq_HoldsOoTSword(player, ITEM_OOT_SWORD_MASTER)) {
        return PLAYER_IA_SWORD_RAZOR - PLAYER_IA_SWORD_KOKIRI;
    }
    return type;
}

s32 MergeShip_OoTEquip_SwordHit(PlayState* play, Player* player) {
    // OoT func_80842B7C (MM func_8083FFEC's place): each hit wears the Giant's Knife; at 0 it breaks.
    if (!MergeShip_OoTEquip_Enabled() || (player->actor.id != ACTOR_PLAYER) ||
        (player->heldItemAction != PLAYER_IA_SWORD_TWO_HANDED) || !ITEM_IS_OOT_EQUIP_SWORD(player->heldItemId)) {
        return false;
    }
    if (!OOT_EQUIP.bgsFlag && (OOT_EQUIP.swordHealth > 0)) {
        OOT_EQUIP.swordHealth--;
        MergeShipTest_Log("OOTEQ knife hit health=%d", OOT_EQUIP.swordHealth);
        if (OOT_EQUIP.swordHealth <= 0) {
            // OoT: EffectSsStick_Spawn, func_800849EC (B becomes the broken knife), NA_SE_IT_MAJIN_SWORD_BROKEN.
            EffectSsStick_Spawn(play, &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND],
                                BINANG_ADD(player->actor.shape.rot.y, 0x8000));
            if (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_OOT_SWORD_GIANTS_KNIFE) {
                BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_OOT_SWORD_BROKEN;
                Interface_LoadItemIcon(play, EQUIP_SLOT_B);
            }
            player->heldItemId = ITEM_OOT_SWORD_BROKEN;
            Player_PlaySfx(player, NA_SE_IT_MAJIN_SWORD_BROKEN);
        }
    }
    return true;
}

u8 MergeShip_OoTEquip_Strength(u8 strength) {
    if (!MergeShip_OoTEquip_Enabled() || (GET_PLAYER_FORM != PLAYER_FORM_HUMAN)) {
        return strength;
    }
    // MM's human is PLAYER_STRENGTH_HUMAN and already lifts Bomb Flowers (the Goron's Bracelet's job in Ocarina of Time).
    // Ocarina of Time's gauntlets push and lift what MM gives the Zora's and the Goron's strength.
    if (OOT_EQUIP.strength >= 3) {
        return MAX(strength, PLAYER_STRENGTH_GORON);
    }
    if (OOT_EQUIP.strength >= 2) {
        return MAX(strength, PLAYER_STRENGTH_ZORA);
    }
    return strength;
}

s32 MergeShip_OoTEquip_CanLiftHeavy(Player* player, Actor* actor) {
    if (!MergeShip_OoTEquip_Enabled() || (player->transformation != PLAYER_FORM_HUMAN) || (actor == NULL)) {
        return false;
    }
    // Ocarina of Time's silver rocks need the Silver Gauntlets; the Golden Gauntlets lift anything (OoT's huge pillars),
    // so also MM's Rock Sirloin. Powder Kegs stay the Goron's: MM's licence rule, not a question of strength.
    if ((actor->id == ACTOR_EN_ISHI) && (actor->params & 1)) {
        return OOT_EQUIP.strength >= 2;
    }
    if (actor->id == ACTOR_EN_MM) {
        return OOT_EQUIP.strength >= 3;
    }
    return false;
}

f32 MergeShip_OoTEquip_DiveDepth(Player* player, f32 depth) {
    // OoT D_80854784[CUR_UPG_VALUE(UPG_SCALE)] = 120 / 240 / 360; MM's human dives 120 (no scale).
    static const f32 sDiveDepths[] = { 120.0f, 240.0f, 360.0f };

    if (!MergeShip_OoTEquip_Enabled() || (player->transformation != PLAYER_FORM_HUMAN) || (OOT_EQUIP.scale > 2)) {
        return depth;
    }
    return MAX(depth, sDiveDepths[OOT_EQUIP.scale]);
}

void MergeShip_OoTEquip_BurnDekuShield(PlayState* play, Player* player) {
    // OoT func_8083819C (VB_BURN_SHIELD): the Deku Shield burns. MM has no Item_Shield actor to drop the burning one.
    static const char sText[] = "\x17\x05Your \x00shield \x05is gone!\x00\x18\x1C\x00<\xBF";

    if (!MergeShip_OoTEquip_Enabled() || (player->actor.id != ACTOR_PLAYER) ||
        (player->transformation != PLAYER_FORM_HUMAN) || (OOT_EQUIP.shield != MS_OOTEQ_SHIELD_DEKU)) {
        return;
    }
    OOT_EQUIP.owned &= ~MS_OOTEQ_OWN_BIT(ITEM_OOT_SHIELD_DEKU);
    OOT_EQUIP.shield = MS_OOTEQ_SHIELD_MM;
    player->currentShield = (s8)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    Player_SetEquipmentData(play, player);
    MergeShip_OoTEquip_ShowText(sText);
    MergeShipTest_Log("OOTEQ deku shield burned");
}

// ---- Hover Boots ----------------------------------------------------------------------------------------------------------

s32 MergeShip_OoTEquip_HoverTimer(void) {
    return sHoverBootsTimer;
}

void MergeShip_OoTEquip_HoverResetTimer(void) {
    sHoverBootsTimer = 0;
}

// OoT func_8083F... (MM func_808430E0's place): the hover timer runs down in the air and is refilled (19) on ground the
// boots cannot hover over. Returns true when the player should be treated as off the ground.
s32 MergeShip_OoTEquip_HoverUpdateGround(PlayState* play, Player* player, s32 floorIsWaterOrSinking) {
    s32 canHoverOnGround;

    if (player->actor.id != ACTOR_PLAYER) {
        return false;
    }
    if ((player->currentBoots == PLAYER_BOOTS_HOVER) && (sHoverBootsTimer != 0)) {
        sHoverBootsTimer--;
    } else {
        sHoverBootsTimer = 0;
    }
    canHoverOnGround = (player->currentBoots == PLAYER_BOOTS_HOVER) && floorIsWaterOrSinking;
    if (canHoverOnGround && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (sHoverBootsTimer != 0)) {
        return true;
    }
    if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !canHoverOnGround) {
        sHoverBootsTimer = 19;
    }
    return false;
}

s32 MergeShip_OoTEquip_HoverKeepsAirborne(Player* player) {
    return (player->actor.id == ACTOR_PLAYER) && (player->currentBoots == PLAYER_BOOTS_HOVER) &&
           (sHoverBootsTimer != 0);
}

s32 MergeShip_OoTEquip_HoverSlides(Player* player) {
    // OoT Player_UpdateCommon: no traction on the Hover Boots, out of water and cutscenes.
    return (player->actor.id == ACTOR_PLAYER) && (player->currentBoots == PLAYER_BOOTS_HOVER) &&
           !(player->stateFlags1 & (PLAYER_STATE1_8000000 | PLAYER_STATE1_20000000));
}

void MergeShip_OoTEquip_HoverSfx(Player* player) {
    // OoT func_80840450: the boots' hum while they hold Link up (instead of footsteps).
    if ((player->actor.id == ACTOR_PLAYER) && (player->currentBoots == PLAYER_BOOTS_HOVER) &&
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (sHoverBootsTimer != 0)) {
        Actor_PlaySfx_Flagged2(&player->actor, NA_SE_PL_HOBBERBOOTS_LV - SFX_FLAG);
    }
}
