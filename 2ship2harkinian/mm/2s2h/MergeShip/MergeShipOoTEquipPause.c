// MergeShip2 -- "Equipment": Ocarina of Time's equipment page, in Majora's Mask's pause screen. See MergeShipOoTEquip.h.
//
// Majora's Mask has no equipment page (its sword and shield sit on the Quest page), so the page is a view of the Quest
// face: C-Up cycles the Quest face Termina's quest status -> Hyrule's quest status -> this page -> Termina's
// (z_kaleido_mergeship.c MergeShip_QuestPage_*), B goes back to Termina's quest status. It is Ocarina of Time's
// equipment page (OoT z_kaleido_equipment.c) on Ocarina of Time's page panels: the upgrades in the left column (Bullet
// Bag, -, strength, scale) and, right of where Ocarina of Time draws Link, a row each of swords, shields, tunics and
// boots, at Ocarina of Time's places (KaleidoScope_InitVertices' equipVtx), with Ocarina of Time's icons, equipped
// outline and names, and Majora's Mask's cursor and name panel.
//
// The page shows only Ocarina of Time's items, each in its cell when owned: the Master Sword, Biggoron's Sword (or the
// Giant's Knife), the Deku, Hylian and Mirror Shields, the tunics and the boots. A on one equips it, as Ocarina of
// Time's page does: a sword on B (in place of MM's own), a shield (in place of MM's own), a tunic, boots.
// MM's own sword and shield are chosen on the Termina Equipment view (MergeShipTerminaEquipPause.c); the old A-press
// "Put Away" swap back to them here is gone (the user's rule).

#include "global.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTEquipAssets.h"
#include "2s2h/MergeShip/MergeShipPauseLink.h"
#include "2s2h/MergeShip/MergeShipOwnership.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

void MergeShipTest_Log(const char* fmt, ...);

#define EQ_COLS 4
#define EQ_ROWS 4
#define EQ_CELLS (EQ_COLS * EQ_ROWS)
#define EQ_CELL(row, col) ((row) * EQ_COLS + (col))

// Page coordinates of the cells' top-left corners: Ocarina of Time's equipment grid (OoT KaleidoScope_InitVertices,
// D_8082B12C and rows of 32 from 58), which its page panels are drawn for. MM's page coordinates are the same.
static const s16 sCellX[EQ_COLS] = { -114, 12, 44, 76 };
static const s16 sCellY[EQ_ROWS] = { 58, 26, -6, -38 };

static s32 sOpen = false;
static s32 sCursor = EQ_CELL(0, 1);
static Vtx sCellVtx[EQ_CELLS * 4];    // the items, 28 x 28 inside their cell (the one under the cursor 32 x 32)
static Vtx sOutlineVtx[EQ_CELLS * 4]; // the equipped outline, the cell

s32 MergeShip_OoTEquipPause_IsOpen(void) {
    return sOpen;
}

void MergeShip_OoTEquipPause_Close(void) {
    sOpen = false;
}

// Harness "ooteqcursor": the cursor on a cell of the open view (and the pause page then, BUG-0069's test).
void MergeShip_OoTEquipPause_DebugCursor(PlayState* play, s32 cell) {
    if ((cell >= 0) && (cell < EQ_CELLS)) {
        sCursor = cell;
    }
    MergeShipTest_Log("OOTEQPAUSE cursor cell=%d open=%d page=%d", sCursor, sOpen,
                      (play != NULL) ? play->pauseCtx.pageIndex : -1);
}

// ---- names and icons ----------------------------------------------------------------------------------------------------

#define NAME(tex) MS_OOTEQ_TEX("item_name_static", tex)
static const ALIGN_ASSET(2) char sNameMaster[] = NAME("gMasterSwordItemNameENGTex");
static const ALIGN_ASSET(2) char sNameKokiriSword[] = NAME("gKokiriSwordItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBiggoron[] = NAME("gBiggoronsSwordItemNameENGTex");
static const ALIGN_ASSET(2) char sNameKnife[] = NAME("gGiantsKnifeItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBroken[] = NAME("gBrokenGiantsKnifeItemNameENGTex");
static const ALIGN_ASSET(2) char sNameDeku[] = NAME("gDekuShieldItemNameENGTex");
static const ALIGN_ASSET(2) char sNameHylian[] = NAME("gHylianShieldItemNameENGTex");
static const ALIGN_ASSET(2) char sNameMirror[] = NAME("gMirrorShieldItemNameENGTex");
static const ALIGN_ASSET(2) char sNameKokiriTunic[] = NAME("gKokiriTunicItemNameENGTex");
static const ALIGN_ASSET(2) char sNameGoronTunic[] = NAME("gGoronTunicItemNameENGTex");
static const ALIGN_ASSET(2) char sNameZoraTunic[] = NAME("gZoraTunicItemNameENGTex");
static const ALIGN_ASSET(2) char sNameKokiriBoots[] = NAME("gKokiriBootsItemNameENGTex");
static const ALIGN_ASSET(2) char sNameIronBoots[] = NAME("gIronBootsItemNameENGTex");
static const ALIGN_ASSET(2) char sNameHoverBoots[] = NAME("gHoverBootsItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBracelet[] = NAME("gGoronsBraceletItemNameENGTex");
static const ALIGN_ASSET(2) char sNameSilverGauntlets[] = NAME("gSilverGauntletsItemNameENGTex");
static const ALIGN_ASSET(2) char sNameGoldenGauntlets[] = NAME("gGoldenGauntletsItemNameENGTex");
static const ALIGN_ASSET(2) char sNameSilverScale[] = NAME("gSilverScaleItemNameENGTex");
static const ALIGN_ASSET(2) char sNameGoldenScale[] = NAME("gGoldenScaleItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBulletBag30[] = NAME("gBulletBag30ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBulletBag40[] = NAME("gBulletBag40ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBulletBag50[] = NAME("gBulletBag50ItemNameENGTex");

static u8 MsEqP_BulletBag(void) {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTItems.bulletBag;
}

// Ocarina of Time's own quiver and bomb bag (the shared save's hyruleBags, MergeShipOwnership.h), in the upgrade column
// as Ocarina of Time's page has them: the quiver (adult) or the Bullet Bag (child) at the top, the bomb bag below. Their
// cells hold Majora's Mask's ITEM_QUIVER_30 .. / ITEM_BOMB_BAG_20 .. item ids at Ocarina of Time's level, drawn with
// Ocarina of Time's icons and named (MsEqP_NameId) with Ocarina of Time's names.
static const ALIGN_ASSET(2) char sIconQuiver30[] = "__OTR__oot/textures/icon_item_static/gItemIconQuiver30Tex";
static const ALIGN_ASSET(2) char sIconQuiver40[] = "__OTR__oot/textures/icon_item_static/gItemIconQuiver40Tex";
static const ALIGN_ASSET(2) char sIconQuiver50[] = "__OTR__oot/textures/icon_item_static/gItemIconQuiver50Tex";
static const ALIGN_ASSET(2) char sIconBombBag20[] = "__OTR__oot/textures/icon_item_static/gItemIconBombBag20Tex";
static const ALIGN_ASSET(2) char sIconBombBag30[] = "__OTR__oot/textures/icon_item_static/gItemIconBombBag30Tex";
static const ALIGN_ASSET(2) char sIconBombBag40[] = "__OTR__oot/textures/icon_item_static/gItemIconBombBag40Tex";
static const ALIGN_ASSET(2) char sNameQuiver30[] = NAME("gQuiver30ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameQuiver40[] = NAME("gQuiver40ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameQuiver50[] = NAME("gQuiver50ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBombBag20[] = NAME("gBombBag20ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBombBag30[] = NAME("gBombBag30ItemNameENGTex");
static const ALIGN_ASSET(2) char sNameBombBag40[] = NAME("gBombBag40ItemNameENGTex");
static const char* const sIconBags[6] = { sIconQuiver30, sIconQuiver40, sIconQuiver50,
                                          sIconBombBag20, sIconBombBag30, sIconBombBag40 };
static const char* const sNameBags[6] = { sNameQuiver30, sNameQuiver40, sNameQuiver50,
                                          sNameBombBag20, sNameBombBag30, sNameBombBag40 };
// The pause cursor's name id of those cells (past the Hyrule quest status's 0x300 .. , z_kaleido_mergeship.c).
#define EQ_BAG_NAME_ID_BASE 0x380

// 0 .. 2 quiver 30 .. 50, 3 .. 5 bomb bag 20 .. 40; -1 for any other item.
static s32 MsEqP_BagIndex(u8 item) {
    if ((item >= ITEM_QUIVER_30) && (item <= ITEM_QUIVER_50)) {
        return item - ITEM_QUIVER_30;
    }
    if ((item >= ITEM_BOMB_BAG_20) && (item <= ITEM_BOMB_BAG_40)) {
        return 3 + (item - ITEM_BOMB_BAG_20);
    }
    return -1;
}

static u8 MsEqP_Quiver(void) {
    const s32 level = MergeShip_Own_OoTUpgrade(UPG_QUIVER);
    return (level > 0) ? (u8)(ITEM_QUIVER_30 + MIN(level, 3) - 1) : ITEM_NONE;
}

static u8 MsEqP_BombBag(void) {
    const s32 level = MergeShip_Own_OoTUpgrade(UPG_BOMB_BAG);
    return (level > 0) ? (u8)(ITEM_BOMB_BAG_20 + MIN(level, 3) - 1) : ITEM_NONE;
}

const char* MergeShip_OoTEquip_NameTexture(u8 item) {
    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
            return sNameMaster;
        case ITEM_OOT_SWORD_KOKIRI:
            return sNameKokiriSword;
        case ITEM_OOT_SWORD_BIGGORON:
            return sNameBiggoron;
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
            return sNameKnife;
        case ITEM_OOT_SWORD_BROKEN:
            return sNameBroken;
        case ITEM_OOT_SHIELD_DEKU:
            return sNameDeku;
        case ITEM_OOT_SHIELD_HYLIAN:
            return sNameHylian;
        case ITEM_OOT_SHIELD_MIRROR:
            return sNameMirror;
        case ITEM_OOT_TUNIC_KOKIRI:
            return sNameKokiriTunic;
        case ITEM_OOT_TUNIC_GORON:
            return sNameGoronTunic;
        case ITEM_OOT_TUNIC_ZORA:
            return sNameZoraTunic;
        case ITEM_OOT_BOOTS_KOKIRI:
            return sNameKokiriBoots;
        case ITEM_OOT_BOOTS_IRON:
            return sNameIronBoots;
        case ITEM_OOT_BOOTS_HOVER:
            return sNameHoverBoots;
        case ITEM_OOT_BRACELET:
            return sNameBracelet;
        case ITEM_OOT_GAUNTLETS_SILVER:
            return sNameSilverGauntlets;
        case ITEM_OOT_GAUNTLETS_GOLD:
            return sNameGoldenGauntlets;
        case ITEM_OOT_SCALE_SILVER:
            return sNameSilverScale;
        case ITEM_OOT_SCALE_GOLDEN:
            return sNameGoldenScale;
        case ITEM_OOT_BULLET_BAG:
            return (MsEqP_BulletBag() >= 3) ? sNameBulletBag50 : (MsEqP_BulletBag() == 2) ? sNameBulletBag40 : sNameBulletBag30;
        default:
            return NULL;
    }
}

const char* MergeShip_OoTEquip_IconTexture(u8 item) {
    if (item == ITEM_OOT_BULLET_BAG) {
        return (MsEqP_BulletBag() >= 3)    ? gOoTEqIconBulletBag50Tex
               : (MsEqP_BulletBag() == 2) ? gOoTEqIconBulletBag40Tex
                                           : gOoTEqIconBulletBag30Tex;
    }
    if (ITEM_IS_OOT_EQUIP(item)) {
        return gItemIcons[item]; // z_inventory.c
    }
    return NULL;
}

const char* MergeShip_OoTEquip_Name(u8 item) {
    static const char* sNames[] = {
        "Master Sword",  "Biggoron's Sword", "Giant's Knife",    "Broken Giant's Knife", "Deku Shield",
        "Hylian Shield", "Mirror Shield",    "Kokiri Tunic",     "Goron Tunic",          "Zora Tunic",
        "Kokiri Boots",  "Iron Boots",       "Hover Boots",      "Goron's Bracelet",     "Silver Gauntlets",
        "Golden Gauntlets", "Silver Scale",  "Golden Scale",     "Bullet Bag",
    };
    if (item == ITEM_OOT_SWORD_KOKIRI) {
        return "Kokiri Sword";
    }
    return ITEM_IS_OOT_EQUIP(item) ? sNames[item - ITEM_OOT_SWORD_MASTER] : "?";
}

const char* MergeShip_OoTEquipPause_NameTexture(u32 item) {
    if (sOpen && (item >= EQ_BAG_NAME_ID_BASE) && (item < EQ_BAG_NAME_ID_BASE + 6)) {
        return sNameBags[item - EQ_BAG_NAME_ID_BASE];
    }
    return (item <= 0xFF) ? MergeShip_OoTEquip_NameTexture((u8)item) : NULL;
}

// ---- cells ----------------------------------------------------------------------------------------------------------------

static s32 MsEqP_OwnsLong(void) {
    return MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_BIGGORON) || MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_GIANTS_KNIFE) ||
           MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_BROKEN);
}

// A sword or shield cell: A on its equipped item puts it away (MM's own sword / shield again).
static s32 MsEqP_IsSwordOrShieldCell(s32 cell) {
    return (cell / EQ_COLS <= 1) && (cell % EQ_COLS != 0);
}

// The item a cell shows (ITEM_NONE for an empty cell).
static u8 MsEqP_CellItem(s32 cell) {
    switch (cell) {
        // Ocarina of Time's page: the quiver when adult, the Bullet Bag when a child (either, when only it is owned).
        case EQ_CELL(0, 0):
            if ((MergeShip_AdultLink_GetSaveAge() == MERGESHIP_HUMAN_AGE_ADULT) || (MsEqP_BulletBag() == 0)) {
                if (MsEqP_Quiver() != ITEM_NONE) {
                    return MsEqP_Quiver();
                }
            }
            return (MsEqP_BulletBag() > 0) ? ITEM_OOT_BULLET_BAG : ITEM_NONE;
        case EQ_CELL(1, 0):
            return MsEqP_BombBag();
        case EQ_CELL(2, 0):
            return (OOT_EQUIP.strength >= 3)   ? ITEM_OOT_GAUNTLETS_GOLD
                   : (OOT_EQUIP.strength == 2) ? ITEM_OOT_GAUNTLETS_SILVER
                   : (OOT_EQUIP.strength == 1) ? ITEM_OOT_BRACELET
                                               : ITEM_NONE;
        case EQ_CELL(3, 0):
            return (OOT_EQUIP.scale >= 2) ? ITEM_OOT_SCALE_GOLDEN
                   : (OOT_EQUIP.scale == 1) ? ITEM_OOT_SCALE_SILVER
                                            : ITEM_NONE;
        // Ocarina of Time's sword row: the Kokiri Sword, the Master Sword, Biggoron's Sword / the Giant's Knife.
        case EQ_CELL(0, 1):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_KOKIRI) ? ITEM_OOT_SWORD_KOKIRI : ITEM_NONE;
        case EQ_CELL(0, 2):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SWORD_MASTER) ? ITEM_OOT_SWORD_MASTER : ITEM_NONE;
        case EQ_CELL(0, 3):
            return MsEqP_OwnsLong() ? MergeShip_OoTEquip_SwordItem(MS_OOTEQ_SWORD_LONG) : ITEM_NONE;
        case EQ_CELL(1, 1):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_DEKU) ? ITEM_OOT_SHIELD_DEKU : ITEM_NONE;
        case EQ_CELL(1, 2):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_HYLIAN) ? ITEM_OOT_SHIELD_HYLIAN : ITEM_NONE;
        case EQ_CELL(1, 3):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_MIRROR) ? ITEM_OOT_SHIELD_MIRROR : ITEM_NONE;
        case EQ_CELL(2, 1):
            return ITEM_OOT_TUNIC_KOKIRI;
        case EQ_CELL(2, 2):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_TUNIC_GORON) ? ITEM_OOT_TUNIC_GORON : ITEM_NONE;
        case EQ_CELL(2, 3):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_TUNIC_ZORA) ? ITEM_OOT_TUNIC_ZORA : ITEM_NONE;
        case EQ_CELL(3, 1):
            return ITEM_OOT_BOOTS_KOKIRI;
        case EQ_CELL(3, 2):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_BOOTS_IRON) ? ITEM_OOT_BOOTS_IRON : ITEM_NONE;
        case EQ_CELL(3, 3):
            return MergeShip_OoTEquip_Owns(ITEM_OOT_BOOTS_HOVER) ? ITEM_OOT_BOOTS_HOVER : ITEM_NONE;
        default:
            return ITEM_NONE;
    }
}

static const char* MsEqP_ItemIcon(u8 item) {
    const char* oot = MergeShip_OoTEquip_IconTexture(item);

    if (MsEqP_BagIndex(item) >= 0) {
        return sIconBags[MsEqP_BagIndex(item)];
    }
    return (oot != NULL) ? oot : (const char*)gItemIcons[item];
}

// Whether the cell's item is the one in use (the selection, which MergeShip_OoTEquip_Update keeps B in step with).
static s32 MsEqP_CellEquipped(s32 cell, u8 item) {
    if (item == ITEM_NONE) {
        return false;
    }
    switch (cell) {
        case EQ_CELL(0, 1):
            return OOT_EQUIP.sword == MS_OOTEQ_SWORD_KOKIRI;
        case EQ_CELL(0, 2):
            return OOT_EQUIP.sword == MS_OOTEQ_SWORD_MASTER;
        case EQ_CELL(0, 3):
            return OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG;
        case EQ_CELL(1, 1):
            return OOT_EQUIP.shield == MS_OOTEQ_SHIELD_DEKU;
        case EQ_CELL(1, 2):
            return OOT_EQUIP.shield == MS_OOTEQ_SHIELD_HYLIAN;
        case EQ_CELL(1, 3):
            return OOT_EQUIP.shield == MS_OOTEQ_SHIELD_MIRROR;
        case EQ_CELL(2, 1):
        case EQ_CELL(2, 2):
        case EQ_CELL(2, 3):
            return OOT_EQUIP.tunic == (cell - EQ_CELL(2, 1));
        case EQ_CELL(3, 1):
        case EQ_CELL(3, 2):
        case EQ_CELL(3, 3):
            return OOT_EQUIP.boots == (cell - EQ_CELL(3, 1));
        default:
            return false;
    }
}

static s32 MsEqP_EquipItem(PlayState* play, u8 item) {
    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
            return MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_MASTER);
        case ITEM_OOT_SWORD_KOKIRI:
            return MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_KOKIRI);
        case ITEM_OOT_SWORD_BIGGORON:
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
        case ITEM_OOT_SWORD_BROKEN:
            return MergeShip_OoTEquip_EquipSword(play, MS_OOTEQ_SWORD_LONG);
        case ITEM_OOT_SHIELD_DEKU:
            return MergeShip_OoTEquip_EquipShield(play, MS_OOTEQ_SHIELD_DEKU);
        case ITEM_OOT_SHIELD_HYLIAN:
            return MergeShip_OoTEquip_EquipShield(play, MS_OOTEQ_SHIELD_HYLIAN);
        case ITEM_OOT_SHIELD_MIRROR:
            return MergeShip_OoTEquip_EquipShield(play, MS_OOTEQ_SHIELD_MIRROR);
        case ITEM_OOT_TUNIC_KOKIRI:
        case ITEM_OOT_TUNIC_GORON:
        case ITEM_OOT_TUNIC_ZORA:
            return MergeShip_OoTEquip_EquipTunic(item - ITEM_OOT_TUNIC_KOKIRI);
        case ITEM_OOT_BOOTS_KOKIRI:
        case ITEM_OOT_BOOTS_IRON:
        case ITEM_OOT_BOOTS_HOVER:
            return MergeShip_OoTEquip_EquipBoots(play, item - ITEM_OOT_BOOTS_KOKIRI);
        default:
            return false;
    }
}

// A on a cell: equip what it shows; on the equipped sword or shield, put it away (MM's own sword / shield again).
static void MsEqP_PressA(PlayState* play, s32 cell) {
    u8 item = MsEqP_CellItem(cell);
    s32 ok;

    if ((item == ITEM_NONE) || (EQ_CELL(0, 0) == cell) || (EQ_CELL(1, 0) == cell) || (EQ_CELL(2, 0) == cell) ||
        (EQ_CELL(3, 0) == cell)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    // The user's rule: no A-press swap back to MM's sword or shield here any more (the Termina Equipment view picks
    // MM's gear). A on the equipped one keeps it.
    if (MsEqP_IsSwordOrShieldCell(cell) && MsEqP_CellEquipped(cell, item)) {
        ok = true;
    } else {
        ok = MsEqP_EquipItem(play, item);
    }
    if (ok) {
        Audio_PlaySfx(NA_SE_SY_DECIDE);
        MergeShipTest_Log("OOTEQPAUSE A cell=%d item=0x%02X sword=%d shield=%d tunic=%d boots=%d B=0x%02X", cell, item,
                          OOT_EQUIP.sword, OOT_EQUIP.shield, OOT_EQUIP.tunic, OOT_EQUIP.boots,
                          BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B));
    } else {
        Audio_PlaySfx(NA_SE_SY_ERROR);
    }
}

// A D-pad direction on a tunic or boots: binds it to that D-pad button (2S2H's D-pad Equips), where a press in play puts it
// on or the Kokiri one back (MergeShip_OoTEquip_UseWearable) -- Ship of Harkinian's "Assignable Tunics and Boots" on
// Termina's D-pad (the user, 2026-10-01: "Assure boots and tunics are able to be bound to dpad in MM"). One button per
// item: bound again elsewhere, it moves.
static void MsEqP_TryBindDpad(PlayState* play, s32 cell) {
    static const u16 kButtons[] = { BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
    static const s32 kSlots[] = { EQUIP_SLOT_D_RIGHT, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_UP };
    Input* input = CONTROLLER1(&play->state);
    u8 item;
    s32 i, j;

    for (i = 0; i < ARRAY_COUNT(kButtons); i++) {
        if (CHECK_BTN_ALL(input->press.button, kButtons[i])) {
            break;
        }
    }
    if (i == ARRAY_COUNT(kButtons)) {
        return;
    }
    item = MsEqP_CellItem(cell);
    if (!CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) ||
        !(((item >= ITEM_OOT_TUNIC_KOKIRI) && (item <= ITEM_OOT_TUNIC_ZORA)) ||
          ((item >= ITEM_OOT_BOOTS_KOKIRI) && (item <= ITEM_OOT_BOOTS_HOVER)))) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    for (j = 0; j < ARRAY_COUNT(kSlots); j++) {
        if ((j != i) && (DPAD_BUTTON_ITEM_EQUIP(0, kSlots[j]) == item)) {
            DPAD_BUTTON_ITEM_EQUIP(0, kSlots[j]) = ITEM_NONE;
            DPAD_SLOT_EQUIP(0, kSlots[j]) = SLOT_NONE;
            Interface_Dpad_LoadItemIconImpl(play, kSlots[j]);
        }
    }
    DPAD_BUTTON_ITEM_EQUIP(0, kSlots[i]) = item;
    DPAD_SLOT_EQUIP(0, kSlots[i]) = MS_OOT_EQUIP_BUTTON_SLOT;
    Interface_Dpad_LoadItemIconImpl(play, kSlots[i]);
    Audio_PlaySfx(NA_SE_SY_DECIDE);
    MergeShipTest_Log("OOTEQPAUSE dpad %d = 0x%02X", kSlots[i], item);
}

// The A button's label: "Put Away" on the equipped Ocarina of Time sword or shield (A takes it off and MM's own is in
// use again), "Decide" elsewhere.
static void MsEqP_UpdateAButtonLabel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    (void)pauseCtx;
    Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE); // no "Put Away" swap any more
}

// ---- Quest page hooks -------------------------------------------------------------------------------------------------------

static void MsEqP_SetCursorItem(PauseContext* pauseCtx) {
    u8 item = MsEqP_CellItem(sCursor);

    pauseCtx->cursorItem[PAUSE_QUEST] = (item == ITEM_NONE)             ? PAUSE_ITEM_NONE
                                        : (MsEqP_BagIndex(item) >= 0) ? (EQ_BAG_NAME_ID_BASE + MsEqP_BagIndex(item))
                                                                      : item;
    pauseCtx->nameColorSet = PAUSE_NAME_COLOR_SET_WHITE;
    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_WHITE;
}

// The cell the cursor lands on when the page is switched to: the equipped sword's (as OoT's page opens on the swords).
void MergeShip_OoTEquipPause_Open(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (!MergeShip_OoTEquip_Enabled()) {
        return;
    }
    sOpen = true;
    sCursor = (OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG)     ? EQ_CELL(0, 3)
              : (OOT_EQUIP.sword == MS_OOTEQ_SWORD_KOKIRI) ? EQ_CELL(0, 1)
                                                           : EQ_CELL(0, 2);
    if (MsEqP_CellItem(sCursor) == ITEM_NONE) {
        sCursor = (MsEqP_CellItem(EQ_CELL(0, 2)) != ITEM_NONE)   ? EQ_CELL(0, 2)
                  : (MsEqP_CellItem(EQ_CELL(0, 3)) != ITEM_NONE) ? EQ_CELL(0, 3)
                                                                 : EQ_CELL(0, 1);
    }
    pauseCtx->cursorShrinkRate = 4.0f;
    MsEqP_SetCursorItem(pauseCtx);
    MsEqP_UpdateAButtonLabel(play);
    MergeShipTest_Log("OOTEQPAUSE open cell=%d", sCursor);
}

// The first cell of a column with an item in it, from the top (OoT's page leaving a page arrow), or -1.
static s32 MsEqP_FirstInColumn(s32 col) {
    s32 row;

    for (row = 0; row < EQ_ROWS; row++) {
        if (MsEqP_CellItem(EQ_CELL(row, col)) != ITEM_NONE) {
            return EQ_CELL(row, col);
        }
    }
    return -1;
}

s32 MergeShip_OoTEquipPause_UpdateQuestCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s32 row, col, old, i;

    if (!MergeShip_OoTEquip_Enabled()) {
        sOpen = false;
        return false;
    }
    if (!sOpen) {
        // C-Up opens it (z_kaleido_mergeship.c); A on MM's sword or shield is MM's own again (their description).
        return false;
    }

    if ((pauseCtx->state != PAUSE_STATE_MAIN) || (pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE)) {
        MsEqP_SetCursorItem(pauseCtx);
        return true;
    }

    // The page arrows (MM's KaleidoScope_UpdateQuestCursor): back onto the page's nearest column with an item.
    if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (pauseCtx->stickAdjX > 30) {
            for (i = 0, old = -1; (i < EQ_COLS) && (old < 0); i++) {
                old = MsEqP_FirstInColumn(i);
            }
            KaleidoScope_MoveCursorFromSpecialPos(play);
            sCursor = (old >= 0) ? old : EQ_CELL(0, 1);
        }
        MsEqP_SetCursorItem(pauseCtx);
        MsEqP_UpdateAButtonLabel(play);
        return true;
    }
    if (pauseCtx->cursorSpecialPos != 0) {
        if (pauseCtx->stickAdjX < -30) {
            for (i = EQ_COLS - 1, old = -1; (i >= 0) && (old < 0); i--) {
                old = MsEqP_FirstInColumn(i);
            }
            KaleidoScope_MoveCursorFromSpecialPos(play);
            sCursor = (old >= 0) ? old : EQ_CELL(0, 1);
        }
        MsEqP_SetCursorItem(pauseCtx);
        MsEqP_UpdateAButtonLabel(play);
        return true;
    }

    row = sCursor / EQ_COLS;
    col = sCursor % EQ_COLS;
    old = sCursor;
    if (pauseCtx->stickAdjX < -30) {
        if (col == 0) {
            // Off the page's left edge: MM's left page arrow.
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            MsEqP_SetCursorItem(pauseCtx);
            MsEqP_UpdateAButtonLabel(play);
            return true;
        }
        col--;
    } else if (pauseCtx->stickAdjX > 30) {
        if (col == EQ_COLS - 1) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
            MsEqP_SetCursorItem(pauseCtx);
            MsEqP_UpdateAButtonLabel(play);
            return true;
        }
        col++;
    }
    if (pauseCtx->stickAdjY > 30) {
        row = MAX(row - 1, 0);
    } else if (pauseCtx->stickAdjY < -30) {
        row = MIN(row + 1, EQ_ROWS - 1);
    }
    sCursor = EQ_CELL(row, col);
    if (sCursor != old) {
        pauseCtx->cursorShrinkRate = 4.0f;
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
    MsEqP_SetCursorItem(pauseCtx);

    if (gSaveContext.buttonStatus[EQUIP_SLOT_A] == BTN_DISABLED) {
        gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_ENABLED;
        gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
        Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
    }

    // (B, back to Termina's quest status, is taken before the pause screen's own B: MergeShip_QuestPage_PreUpdate.)
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        MsEqP_PressA(play, sCursor);
        MsEqP_SetCursorItem(pauseCtx);
    }
    MsEqP_TryBindDpad(play, sCursor); // MergeShip2: a tunic or boots onto the D-pad
    MsEqP_UpdateAButtonLabel(play);
    return true;
}

static void MsEqP_SetQuad(Vtx* v, s16 x, s16 y, s16 size, u8 alpha) {
    s32 i;

    for (i = 0; i < 4; i++) {
        v[i].v.ob[0] = x + ((i & 1) ? size : 0);
        v[i].v.ob[1] = y - ((i & 2) ? size : 0);
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.tc[0] = (i & 1) ? (32 << 5) : 0; // the 32 x 32 texture over the quad
        v[i].v.tc[1] = (i & 2) ? (32 << 5) : 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = 255;
        v[i].v.cn[3] = alpha;
    }
}

s32 MergeShip_OoTEquipPause_DrawQuestPage(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 cell;

    if (!sOpen || !MergeShip_OoTEquip_Enabled()) {
        return false;
    }

    // OoT KaleidoScope_InitVertices: the items 28 x 28 inside their cell, the equipped outline the 32 x 32 cell; the
    // item under the cursor drawn at 32 x 32 (OoT KaleidoScope_DrawEquipment).
    for (cell = 0; cell < EQ_CELLS; cell++) {
        s16 x = sCellX[cell % EQ_COLS];
        s16 y = sCellY[cell / EQ_COLS] + pauseCtx->offsetY;

        if ((cell == sCursor) && (pauseCtx->cursorSpecialPos == 0) && (pauseCtx->pageIndex == PAUSE_QUEST)) {
            MsEqP_SetQuad(&sCellVtx[cell * 4], x, y, 32, pauseCtx->alpha);
        } else {
            MsEqP_SetQuad(&sCellVtx[cell * 4], x + 2, y - 2, 28, pauseCtx->alpha);
        }
        MsEqP_SetQuad(&sOutlineVtx[cell * 4], x, y, 32, pauseCtx->alpha);
    }
    if (pauseCtx->pageIndex == PAUSE_QUEST) {
        // MM's cursor goes round the cell's item (KaleidoScope_UpdateCursorSize, MergeShip_QuestPage_SetCursorPos).
        MergeShip_QuestPage_SetCursorQuad(sCellX[sCursor % EQ_COLS] + 2, sCellY[sCursor / EQ_COLS] - 2, 28, 28);
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (cell = 0; cell < EQ_CELLS; cell++) {
        u8 item = MsEqP_CellItem(cell);

        if (item == ITEM_NONE) {
            continue;
        }
        if (MsEqP_CellEquipped(cell, item)) {
            // OoT KaleidoScope_DrawEquipment: the equipped item's outline behind it (IA8, OoT parameter_static).
            gSPVertex(POLY_OPA_DISP++, &sOutlineVtx[cell * 4], 4, 0);
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)gOoTEqEquippedOutlineTex, 32, 32, 0);
        }
        gSPVertex(POLY_OPA_DISP++, &sCellVtx[cell * 4], 4, 0);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, (TexturePtr)MsEqP_ItemIcon(item), 32, 32, 0);
    }

    CLOSE_DISPS(play->state.gfxCtx);

    // Link in the centre panel, where Ocarina of Time's equipment page draws him (MergeShipPauseLink.h).
    MergeShip_PauseLink_Draw(play, "hyrule");
    return true;
}

const char* MergeShip_OoTEquipPause_QuestIcon(s32 isShield, const char* mmIcon) {
    if (!MergeShip_OoTEquip_Enabled()) {
        return mmIcon;
    }
    if (isShield) {
        u8 item = MergeShip_OoTEquip_ShieldItem(OOT_EQUIP.shield);

        return (OOT_EQUIP.shield != MS_OOTEQ_SHIELD_MM) ? MsEqP_ItemIcon(item) : mmIcon;
    } else {
        if (OOT_EQUIP.sword != MS_OOTEQ_SWORD_MM) {
            return MsEqP_ItemIcon(MergeShip_OoTEquip_SwordItem(OOT_EQUIP.sword));
        }
        return mmIcon;
    }
}
