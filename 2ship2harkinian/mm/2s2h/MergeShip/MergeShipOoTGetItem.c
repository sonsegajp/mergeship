// MergeShip2 -- Ocarina of Time's get-item models in Majora's Mask (2Ship).
//
// Port of OoT z_draw.c (Shipwright/soh/src/code/z_draw.c): the sDrawItemTable rows and GetItem_Draw* functions that
// the OoT items of MergeShipOoTItems.h use, keyed by OoT's own GID (OoT get-item table, VanillaItemTable_Init). The
// display lists are Ocarina of Time's, read from the OoT archive mounted under "oot/" (MergeShipAssets.h), so no object
// has to be loaded and no segment 6 is needed. MM's graphics API: Gfx_SetupDL25_Opa/Xlu, Gfx_SetupDL26_Opa,
// MATRIX_FINALIZE_AND_LOAD (MM's MATRIX_NEWMTX + G_MTX_MODELVIEW | G_MTX_LOAD), Gfx_TwoTexScrollEx.

#include "global.h"
#include "align_asset_macro.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"

// OoT object_gi_* display lists (names from OoT's object headers), as OoT archive paths.
#define MS_GI_DL(object, name) OOT_ASSET("objects/" object "/" name)

static const ALIGN_ASSET(2) char sGiMagicSpellDiamondDL[] = MS_GI_DL("object_gi_goddess", "gGiMagicSpellDiamondDL");
static const ALIGN_ASSET(2) char sGiDinsFireColorDL[] = MS_GI_DL("object_gi_goddess", "gGiDinsFireColorDL");
static const ALIGN_ASSET(2) char sGiFaroresWindColorDL[] = MS_GI_DL("object_gi_goddess", "gGiFaroresWindColorDL");
static const ALIGN_ASSET(2) char sGiNayrusLoveColorDL[] = MS_GI_DL("object_gi_goddess", "gGiNayrusLoveColorDL");
static const ALIGN_ASSET(2) char sGiMagicSpellOrbDL[] = MS_GI_DL("object_gi_goddess", "gGiMagicSpellOrbDL");
static const ALIGN_ASSET(2) char sGiBoomerangDL[] = MS_GI_DL("object_gi_boomerang", "gGiBoomerangDL");
static const ALIGN_ASSET(2) char sGiHammerDL[] = MS_GI_DL("object_gi_hammer", "gGiHammerDL");
static const ALIGN_ASSET(2) char sGiSkullMaskDL[] = MS_GI_DL("object_gi_skj_mask", "gGiSkullMaskDL");
static const ALIGN_ASSET(2) char sGiSpookyMaskDL[] = MS_GI_DL("object_gi_redead_mask", "gGiSpookyMaskDL");
static const ALIGN_ASSET(2) char sGiGerudoMaskDL[] = MS_GI_DL("object_gi_gerudomask", "gGiGerudoMaskDL");
// BUG-0066 (SoH sDrawItemTable GID_MASK_KEATON .. GID_MASK_TRUTH)
static const ALIGN_ASSET(2) char sGiKeatonMaskDL[] = MS_GI_DL("object_gi_ki_tan_mask", "gGiKeatonMaskDL");
static const ALIGN_ASSET(2) char sGiKeatonMaskEyesDL[] = MS_GI_DL("object_gi_ki_tan_mask", "gGiKeatonMaskEyesDL");
static const ALIGN_ASSET(2) char sGiBunnyHoodDL[] = MS_GI_DL("object_gi_rabit_mask", "gGiBunnyHoodDL");
static const ALIGN_ASSET(2) char sGiBunnyHoodEyesDL[] = MS_GI_DL("object_gi_rabit_mask", "gGiBunnyHoodEyesDL");
static const ALIGN_ASSET(2) char sGiGoronMaskDL[] = MS_GI_DL("object_gi_golonmask", "gGiGoronMaskDL");
static const ALIGN_ASSET(2) char sGiZoraMaskDL[] = MS_GI_DL("object_gi_zoramask", "gGiZoraMaskDL");
static const ALIGN_ASSET(2) char sGiMaskOfTruthDL[] = MS_GI_DL("object_gi_truth_mask", "gGiMaskOfTruthDL");
static const ALIGN_ASSET(2) char sGiMaskOfTruthAccentsDL[] = MS_GI_DL("object_gi_truth_mask", "gGiMaskOfTruthAccentsDL");
static const ALIGN_ASSET(2) char sGiLetterBottleContentsDL[] = MS_GI_DL("object_gi_bottle_letter", "gGiLetterBottleContentsDL");
static const ALIGN_ASSET(2) char sGiLetterBottleDL[] = MS_GI_DL("object_gi_bottle_letter", "gGiLetterBottleDL");
static const ALIGN_ASSET(2) char sGiEggMaterialDL[] = MS_GI_DL("object_gi_egg", "gGiEggMaterialDL");
static const ALIGN_ASSET(2) char sGiEggDL[] = MS_GI_DL("object_gi_egg", "gGiEggDL");
static const ALIGN_ASSET(2) char sGiChickenDL[] = MS_GI_DL("object_gi_niwatori", "gGiChickenDL");
static const ALIGN_ASSET(2) char sGiChickenColorDL[] = MS_GI_DL("object_gi_niwatori", "gGiChickenColorDL");
static const ALIGN_ASSET(2) char sGiChickenEyesDL[] = MS_GI_DL("object_gi_niwatori", "gGiChickenEyesDL");
static const ALIGN_ASSET(2) char sGiCojiroColorDL[] = MS_GI_DL("object_gi_niwatori", "gGiCojiroColorDL");
static const ALIGN_ASSET(2) char sGiLetterDL[] = MS_GI_DL("object_gi_letter", "gGiLetterDL");
static const ALIGN_ASSET(2) char sGiLetterWritingDL[] = MS_GI_DL("object_gi_letter", "gGiLetterWritingDL");
static const ALIGN_ASSET(2) char sGiOddMushroomDL[] = MS_GI_DL("object_gi_mushroom", "gGiOddMushroomDL");
static const ALIGN_ASSET(2) char sGiOddPotionDL[] = MS_GI_DL("object_gi_powder", "gGiOddPotionDL");
static const ALIGN_ASSET(2) char sGiSawDL[] = MS_GI_DL("object_gi_saw", "gGiSawDL");
static const ALIGN_ASSET(2) char sGiPrescriptionDL[] = MS_GI_DL("object_gi_prescription", "gGiPrescriptionDL");
static const ALIGN_ASSET(2) char sGiPrescriptionWritingDL[] = MS_GI_DL("object_gi_prescription", "gGiPrescriptionWritingDL");
static const ALIGN_ASSET(2) char sGiFrogDL[] = MS_GI_DL("object_gi_frog", "gGiFrogDL");
static const ALIGN_ASSET(2) char sGiFrogEyesDL[] = MS_GI_DL("object_gi_frog", "gGiFrogEyesDL");
static const ALIGN_ASSET(2) char sGiEyeDropsCapDL[] = MS_GI_DL("object_gi_eye_lotion", "gGiEyeDropsCapDL");
static const ALIGN_ASSET(2) char sGiEyeDropsBottleDL[] = MS_GI_DL("object_gi_eye_lotion", "gGiEyeDropsBottleDL");
static const ALIGN_ASSET(2) char sGiClaimCheckDL[] = MS_GI_DL("object_gi_ticketstone", "gGiClaimCheckDL");
static const ALIGN_ASSET(2) char sGiClaimCheckWritingDL[] = MS_GI_DL("object_gi_ticketstone", "gGiClaimCheckWritingDL");
static const ALIGN_ASSET(2) char sGiSeedDL[] = MS_GI_DL("object_gi_seed", "gGiSeedDL");
static const ALIGN_ASSET(2) char sGiSlingshotDL[] = MS_GI_DL("object_gi_pachinko", "gGiSlingshotDL");
static const ALIGN_ASSET(2) char sGiLongshotDL[] = MS_GI_DL("object_gi_hookshot", "gGiLongshotDL");

typedef void (*MsOoTGetItemDrawFunc)(PlayState* play, const char* const* dlists);

typedef struct {
    MsOoTGetItemDrawFunc drawFunc;
    const char* dlists[3];
} MsOoTDrawItemTableEntry;

// OoT GetItem_DrawOpa0
static void MsOoT_GetItem_DrawOpa0(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT GetItem_DrawOpa0Xlu1
static void MsOoT_GetItem_DrawOpa0Xlu1(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[0]);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dlists[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT GetItem_DrawOpa10Xlu2
static void MsOoT_GetItem_DrawOpa10Xlu2(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[1]);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[0]);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dlists[2]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT GetItem_DrawMaskOrBombchu
static void MsOoT_GetItem_DrawMaskOrBombchu(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL26_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT GetItem_DrawEggOrMedallion
static void MsOoT_GetItem_DrawEggOrMedallion(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL26_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[0]);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlists[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT GetItem_DrawMagicSpell: gGiMagicSpellOrbDL calls segment 8 for its two scrolling texture tiles.
static void MsOoT_GetItem_DrawMagicSpell(PlayState* play, const char* const* dlists) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 1 * (play->state.frames * 2), 1 * -(play->state.frames * 6),
                                  32, 32, 1, 1 * (play->state.frames * 1), -1 * (play->state.frames * 2), 32, 32, 2, -6,
                                  1, -2));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dlists[0]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dlists[1]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dlists[2]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT sDrawItemTable rows, named by OoT GID.
static const MsOoTDrawItemTableEntry sGidDinsFire = {
    MsOoT_GetItem_DrawMagicSpell, { sGiMagicSpellDiamondDL, sGiDinsFireColorDL, sGiMagicSpellOrbDL }
};
static const MsOoTDrawItemTableEntry sGidFaroresWind = {
    MsOoT_GetItem_DrawMagicSpell, { sGiMagicSpellDiamondDL, sGiFaroresWindColorDL, sGiMagicSpellOrbDL }
};
static const MsOoTDrawItemTableEntry sGidNayrusLove = {
    MsOoT_GetItem_DrawMagicSpell, { sGiMagicSpellDiamondDL, sGiNayrusLoveColorDL, sGiMagicSpellOrbDL }
};
static const MsOoTDrawItemTableEntry sGidBoomerang = { MsOoT_GetItem_DrawOpa0, { sGiBoomerangDL } };
static const MsOoTDrawItemTableEntry sGidHammer = { MsOoT_GetItem_DrawOpa0, { sGiHammerDL } };
static const MsOoTDrawItemTableEntry sGidMaskSkull = { MsOoT_GetItem_DrawOpa0, { sGiSkullMaskDL } };
static const MsOoTDrawItemTableEntry sGidMaskSpooky = { MsOoT_GetItem_DrawOpa0, { sGiSpookyMaskDL } };
static const MsOoTDrawItemTableEntry sGidMaskGerudo = { MsOoT_GetItem_DrawMaskOrBombchu, { sGiGerudoMaskDL } };
static const MsOoTDrawItemTableEntry sGidMaskKeaton = { MsOoT_GetItem_DrawOpa0Xlu1, { sGiKeatonMaskDL, sGiKeatonMaskEyesDL } };
static const MsOoTDrawItemTableEntry sGidMaskBunny = { MsOoT_GetItem_DrawOpa0Xlu1, { sGiBunnyHoodDL, sGiBunnyHoodEyesDL } };
static const MsOoTDrawItemTableEntry sGidMaskGoron = { MsOoT_GetItem_DrawMaskOrBombchu, { sGiGoronMaskDL } };
static const MsOoTDrawItemTableEntry sGidMaskZora = { MsOoT_GetItem_DrawMaskOrBombchu, { sGiZoraMaskDL } };
static const MsOoTDrawItemTableEntry sGidMaskTruth = {
    MsOoT_GetItem_DrawOpa0Xlu1, { sGiMaskOfTruthDL, sGiMaskOfTruthAccentsDL }
};
static const MsOoTDrawItemTableEntry sGidLetterRuto = {
    MsOoT_GetItem_DrawOpa0Xlu1, { sGiLetterBottleContentsDL, sGiLetterBottleDL }
};
static const MsOoTDrawItemTableEntry sGidEgg = { MsOoT_GetItem_DrawEggOrMedallion, { sGiEggMaterialDL, sGiEggDL } };
static const MsOoTDrawItemTableEntry sGidChicken = {
    MsOoT_GetItem_DrawOpa10Xlu2, { sGiChickenDL, sGiChickenColorDL, sGiChickenEyesDL }
};
static const MsOoTDrawItemTableEntry sGidCojiro = {
    MsOoT_GetItem_DrawOpa10Xlu2, { sGiChickenDL, sGiCojiroColorDL, sGiChickenEyesDL }
};
static const MsOoTDrawItemTableEntry sGidLetterZelda = { MsOoT_GetItem_DrawOpa0Xlu1, { sGiLetterDL, sGiLetterWritingDL } };
static const MsOoTDrawItemTableEntry sGidOddMushroom = { MsOoT_GetItem_DrawOpa0, { sGiOddMushroomDL } };
static const MsOoTDrawItemTableEntry sGidOddPotion = { MsOoT_GetItem_DrawOpa0, { sGiOddPotionDL } };
static const MsOoTDrawItemTableEntry sGidSaw = { MsOoT_GetItem_DrawOpa0, { sGiSawDL } };
static const MsOoTDrawItemTableEntry sGidPrescription = {
    MsOoT_GetItem_DrawOpa0Xlu1, { sGiPrescriptionDL, sGiPrescriptionWritingDL }
};
static const MsOoTDrawItemTableEntry sGidFrog = { MsOoT_GetItem_DrawOpa0Xlu1, { sGiFrogDL, sGiFrogEyesDL } };
static const MsOoTDrawItemTableEntry sGidEyedrops = {
    MsOoT_GetItem_DrawOpa0Xlu1, { sGiEyeDropsCapDL, sGiEyeDropsBottleDL }
};
static const MsOoTDrawItemTableEntry sGidClaimCheck = {
    MsOoT_GetItem_DrawOpa0Xlu1, { sGiClaimCheckDL, sGiClaimCheckWritingDL }
};
static const MsOoTDrawItemTableEntry sGidSeeds = { MsOoT_GetItem_DrawOpa0, { sGiSeedDL } };
static const MsOoTDrawItemTableEntry sGidSlingshot = { MsOoT_GetItem_DrawOpa0, { sGiSlingshotDL } };
static const MsOoTDrawItemTableEntry sGidLongshot = { MsOoT_GetItem_DrawOpa0, { sGiLongshotDL } };

// The item's OoT GI -> GID (OoT get-item table, soh/OTRGlobals.cpp VanillaItemTable_Init).
static const MsOoTDrawItemTableEntry* MsOoT_GetDrawEntry(u8 item) {
    switch (item) {
        case ITEM_OOT_DINS_FIRE: // GI_DINS_FIRE
            return &sGidDinsFire;
        case ITEM_OOT_FARORES_WIND: // GI_FARORES_WIND
            return &sGidFaroresWind;
        case ITEM_OOT_NAYRUS_LOVE: // GI_NAYRUS_LOVE
            return &sGidNayrusLove;
        case ITEM_OOT_BOOMERANG: // GI_BOOMERANG
            return &sGidBoomerang;
        case ITEM_OOT_HAMMER: // GI_HAMMER
            return &sGidHammer;
        case ITEM_OOT_MASK_SKULL: // GI_MASK_SKULL
            return &sGidMaskSkull;
        case ITEM_OOT_MASK_SPOOKY: // GI_MASK_SPOOKY
            return &sGidMaskSpooky;
        case ITEM_OOT_MASK_GERUDO: // GI_MASK_GERUDO
            return &sGidMaskGerudo;
        case ITEM_OOT_MASK_KEATON: // GI_MASK_KEATON
            return &sGidMaskKeaton;
        case ITEM_OOT_MASK_BUNNY: // GI_MASK_BUNNY
            return &sGidMaskBunny;
        case ITEM_OOT_MASK_GORON: // GI_MASK_GORON
            return &sGidMaskGoron;
        case ITEM_OOT_MASK_ZORA: // GI_MASK_ZORA
            return &sGidMaskZora;
        case ITEM_OOT_MASK_TRUTH: // GI_MASK_TRUTH
            return &sGidMaskTruth;
        case ITEM_OOT_LETTER_RUTO: // GI_LETTER_RUTO
            return &sGidLetterRuto;
        case ITEM_OOT_WEIRD_EGG:  // GI_WEIRD_EGG
        case ITEM_OOT_POCKET_EGG: // GI_POCKET_EGG
            return &sGidEgg;
        case ITEM_OOT_CHICKEN:      // GI_CHICKEN
        case ITEM_OOT_POCKET_CUCCO: // GI_POCKET_CUCCO
            return &sGidChicken;
        case ITEM_OOT_COJIRO: // GI_COJIRO
            return &sGidCojiro;
        case ITEM_OOT_LETTER_ZELDA: // GI_LETTER_ZELDA
            return &sGidLetterZelda;
        case ITEM_OOT_ODD_MUSHROOM: // GI_ODD_MUSHROOM
            return &sGidOddMushroom;
        case ITEM_OOT_ODD_POTION: // GI_ODD_POTION
            return &sGidOddPotion;
        case ITEM_OOT_SAW: // GI_SAW
            return &sGidSaw;
        case ITEM_OOT_PRESCRIPTION: // GI_PRESCRIPTION
            return &sGidPrescription;
        case ITEM_OOT_FROG: // GI_FROG
            return &sGidFrog;
        case ITEM_OOT_EYEDROPS: // GI_EYEDROPS
            return &sGidEyedrops;
        case ITEM_OOT_CLAIM_CHECK: // GI_CLAIM_CHECK
            return &sGidClaimCheck;
        case ITEM_OOT_DEKU_SEEDS: // GI_SEEDS_5
            return &sGidSeeds;
        case ITEM_SLINGSHOT: // GI_SLINGSHOT (OoT's Fairy Slingshot)
            return &sGidSlingshot;
        case ITEM_LONGSHOT: // GI_LONGSHOT
            return &sGidLongshot;
        default:
            return NULL;
    }
}

void MergeShip_OoTItems_DrawGetItem(PlayState* play, u8 item) {
    const MsOoTDrawItemTableEntry* entry = MsOoT_GetDrawEntry(item);

    if ((entry == NULL) || !MergeShip_IsOcarinaMounted()) {
        return;
    }
    entry->drawFunc(play, entry->dlists);
}
