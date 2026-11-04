// MergeShip2 -- Majora's Mask's transformation masks as items: their get-item models, their get-item text
// outside a randomizer seed, and Ocarina of Time's Goron and Zora masks merged into the transformation masks.
//
// Ocarina of Time's Goron Mask and Zora Mask (the Happy Mask Shop's masks)
// ------------------------------------------------------------------------
// With CVAR_ENHANCEMENT("MergeShip.MergeOoTMasks") on (the default), they *are* the Goron and Zora
// transformation masks:
//  * Getting one -- borrowing it at the Happy Mask Shop, or finding the randomizer's RG_GORON_MASK /
//    RG_ZORA_MASK -- also gives the transformation mask for good (the Masks page, gMergeShipSave.ownedMasks).
//    MM's transformation masks are never taken back, so this one is not either.
//  * The Happy Mask Shop's own sequence is untouched: the mask still goes to the child trade slot, the
//    shop still lends and swaps its masks there, and the child trade quest (selling Keaton, Skull,
//    Spooky, Bunny) is unchanged. Swapping the Goron Mask for another borrowed mask empties the trade
//    slot, but the transformation stays on the Masks page.
//  * Wearing the trade slot's Goron or Zora Mask from a C button transforms (Player_ItemToItemAction), and
//    the button follows MM's rules for that transformation mask (MergeShipItems.c). The cosmetic Ocarina
//    of Time mask is no longer put on, so NPCs' reactions to it are not seen; no Ocarina of Time reward
//    depends on wearing either mask (the Deku Theater's prizes are for the Skull Mask and Mask of Truth).
//  * In a randomizer seed with "Shuffle Transformation Masks" on, the transformation masks are items of
//    their own with the logic weight the user chose (Goron = Megaton Hammer, Zora = Iron Boots), so there
//    the shop's Goron and Zora masks stay cosmetic: they would otherwise hand out those items through the
//    child trade quest, outside the logic's placement.
//
// Get-item models: MM z_draw.c sDrawItemTable, from mm.o2r:
//   GID_MASK_DEKU         GetItem_DrawOpa0Xlu1 { gGiDekuMaskEmptyDL, gGiDekuMaskDL }
//   GID_MASK_GORON        GetItem_DrawOpa0Xlu1 { gGiGoronMaskEmptyDL, gGiGoronMaskDL }
//   GID_MASK_ZORA         GetItem_DrawOpa01    { gGiZoraMaskEmptyDL, gGiZoraMaskDL }
//   GID_MASK_FIERCE_DEITY GetItem_DrawOpa01    { gGiFierceDeityMaskFaceDL, gGiFierceDeityMaskHairAndHatDL }
//
// Majora's Mask's other 20 masks (RG_MM_MASK_OF_TRUTH..RG_MM_GIANTS_MASK, RSK_SHUFFLE_MM_MASKS)
// ----------------------------------------------------------------------------------------------
// Randomizer items (outside a seed only Item_Give hands them out, e.g. a debug option). Finding one gives the mask
// (MergeShip_GiveMMMask, the Masks page) and sets its RAND_INF_OBTAINED_MM_* flag, from which a loaded file gets it
// back. Their get-item models are MM's: kMMMaskGi holds each mask's sDrawItemTable entry.

#include "soh/MergeShip/MergeShipMaskItems.h"

#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "mm/objects/object_gi_nutsmask/object_gi_nutsmask.h"
#include "mm/objects/object_gi_golonmask/object_gi_golonmask.h"
#include "mm/objects/object_gi_zoramask/object_gi_zoramask.h"
#include "mm/objects/object_gi_mask03/object_gi_mask03.h"
#include "mm/objects/object_gi_truth_mask/object_gi_truth_mask.h"
#include "mm/objects/object_gi_mask05/object_gi_mask05.h"
#include "mm/objects/object_gi_mask06/object_gi_mask06.h"
#include "mm/objects/object_gi_rabit_mask/object_gi_rabit_mask.h"
#include "mm/objects/object_gi_ki_tan_mask/object_gi_ki_tan_mask.h"
#include "mm/objects/object_gi_mask09/object_gi_mask09.h"
#include "mm/objects/object_gi_mask10/object_gi_mask10.h"
#include "mm/objects/object_gi_mask11/object_gi_mask11.h"
#include "mm/objects/object_gi_mask12/object_gi_mask12.h"
#include "mm/objects/object_gi_mask13/object_gi_mask13.h"
#include "mm/objects/object_gi_mask14/object_gi_mask14.h"
#include "mm/objects/object_gi_mask15/object_gi_mask15.h"
#include "mm/objects/object_gi_mask16/object_gi_mask16.h"
#include "mm/objects/object_gi_mask17/object_gi_mask17.h"
#include "mm/objects/object_gi_mask18/object_gi_mask18.h"
#include "mm/objects/object_gi_stonemask/object_gi_stonemask.h"
#include "mm/objects/object_gi_mask20/object_gi_mask20.h"
#include "mm/objects/object_gi_mask21/object_gi_mask21.h"
#include "mm/objects/object_gi_mask22/object_gi_mask22.h"
#include "mm/objects/object_gi_mask23/object_gi_mask23.h"
extern PlayState* gPlayState;
u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey); // OTRGlobals.cpp (declared for C only)
}

// soh/Enhancements/randomizer/Messages/ItemMessages.cpp: the randomizer's custom get-item icon in a text box.
void LoadCustomItemIcon(bool displayAsEnglish);
void DrawCustomItemIcon(Gfx** p);

namespace {

// OPEN_DISPS / CLOSE_DISPS redeclare these at block scope; declared C here so that, inside this namespace, they
// still name the C functions in frame_interpolation.cpp.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

// MM GetItem_DrawOpa0Xlu1: the first list opaque, the second translucent.
void DrawOpa0Xlu1(PlayState* play, const char* opa, const char* xlu) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)opa);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)xlu);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MM GetItem_DrawOpa01: both lists opaque.
void DrawOpa01(PlayState* play, const char* first, const char* second) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)first);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)second);

    CLOSE_DISPS(play->state.gfxCtx);
}

bool IsMMMaskEntry(const GetItemEntry& entry) {
    return (entry.modIndex == MOD_RANDOMIZER) && (entry.getItemId >= RG_MM_DEKU_MASK) &&
           (entry.getItemId <= RG_MM_FIERCE_DEITY_MASK);
}

bool IsMMRegularMaskRG(int rg) {
    return (rg >= RG_MM_MASK_OF_TRUTH) && (rg <= RG_MM_GIANTS_MASK);
}

// One of Majora's Mask's other 20 masks, found as a randomizer item.
bool IsMMRegularMaskEntry(const GetItemEntry& entry) {
    return (entry.modIndex == MOD_RANDOMIZER) && IsMMRegularMaskRG(entry.getItemId);
}

// MMMask of RG_MM_MASK_OF_TRUTH..RG_MM_GIANTS_MASK (the same order as MM_MASK_TRUTH..MM_MASK_GIANT).
u8 MMMaskOfRandoMask(int rg) {
    return (u8)(MM_MASK_TRUTH + (rg - RG_MM_MASK_OF_TRUTH));
}

void GiveRegularMMMask(u8 mmMask) {
    if (!MergeShip_HasMMMask(mmMask)) {
        MergeShip_GiveMMMask(mmMask);
        SPDLOG_INFO("MergeShip: MM mask {} owned", mmMask);
    }
}

// MM z_draw.c sDrawItemTable entries of the 20 masks, in RG_MM_MASK_OF_TRUTH order: the draw function, then its
// two display lists in drawResources order.
struct MMMaskGi {
    bool opa0Xlu1; // GetItem_DrawOpa0Xlu1, else GetItem_DrawOpa01
    const char* dl0;
    const char* dl1;
};
static_assert(RG_MM_GIANTS_MASK - RG_MM_MASK_OF_TRUTH == MM_MASK_GIANT - MM_MASK_TRUTH,
              "one randomizer item per MM mask, in MMMask order");
const MMMaskGi kMMMaskGi[] = {
    /* GID_MASK_TRUTH         */ { true, mm_gGiMaskOfTruthDL, mm_gGiMaskOfTruthAccentsDL },
    /* GID_MASK_KAFEIS_MASK   */ { false, mm_gGiKafeiMaskDL, mm_gGiKafeiMaskEmptyDL },
    /* GID_MASK_ALL_NIGHT     */ { true, mm_gGiAllNightMaskEyesDL, mm_gGiAllNightMaskFaceDL },
    /* GID_MASK_BUNNY         */ { true, mm_gGiBunnyHoodDL, mm_gGiBunnyHoodEyesDL },
    /* GID_MASK_KEATON        */ { true, mm_gGiKeatonMaskDL, mm_gGiKeatonMaskEyesDL },
    /* GID_MASK_GARO          */ { true, mm_gGiGarosMaskCloakDL, mm_gGiGarosMaskFaceDL },
    /* GID_MASK_ROMANI        */ { true, mm_gGiRomaniMaskCapDL, mm_gGiRomaniMaskNoseEyeDL },
    /* GID_MASK_CIRCUS_LEADER */ { true, mm_gGiCircusLeaderMaskFaceDL, mm_gGiCircusLeaderMaskEyebrowsDL },
    /* GID_MASK_POSTMAN       */ { true, mm_gGiPostmanHatCapDL, mm_gGiPostmanHatBunnyLogoDL },
    /* GID_MASK_COUPLE        */ { true, mm_gGiCouplesMaskFullDL, mm_gGiCouplesMaskHalfDL },
    /* GID_MASK_GREAT_FAIRY   */ { true, mm_gGiGreatFairyMaskFaceDL, mm_gGiGreatFairyMaskLeavesDL },
    /* GID_MASK_GIBDO         */ { true, mm_gGiGibdoMaskEmptyDL, mm_gGiGibdoMaskDL },
    /* GID_MASK_DON_GERO      */ { true, mm_gGiDonGeroMaskFaceDL, mm_gGiDonGeroMaskBodyDL },
    /* GID_MASK_KAMARO        */ { false, mm_gGiKamaroMaskDL, mm_gGiKamaroMaskEmptyDL },
    /* GID_MASK_CAPTAIN       */ { false, mm_gGiCaptainsHatBodyDL, mm_gGiCaptainsHatFaceDL },
    /* GID_MASK_STONE         */ { true, mm_gGiStoneMaskEmptyDL, mm_gGiStoneMaskDL },
    /* GID_MASK_BREMEN        */ { false, mm_gGiBremenMaskDL, mm_gGiBremenMaskEmptyDL },
    /* GID_MASK_BLAST         */ { true, mm_gGiBlastMaskEmptyDL, mm_gGiBlastMaskDL },
    /* GID_MASK_SCENTS        */ { false, mm_gGiMaskOfScentsFaceDL, mm_gGiMaskOfScentsTeethDL },
    /* GID_MASK_GIANT         */ { false, mm_gGiGiantMaskEmptyDL, mm_gGiGiantMaskDL },
};
static_assert(sizeof(kMMMaskGi) / sizeof(kMMMaskGi[0]) == RG_MM_GIANTS_MASK - RG_MM_MASK_OF_TRUTH + 1,
              "a get-item model per MM mask");

// MM form (MMPlayerForm) of a randomizer transformation mask.
u8 MMFormOfRandoMask(uint16_t rg) {
    switch (rg) {
        case RG_MM_DEKU_MASK:
            return MM_FORM_DEKU;
        case RG_MM_GORON_MASK:
            return MM_FORM_GORON;
        case RG_MM_ZORA_MASK:
            return MM_FORM_ZORA;
        case RG_MM_FIERCE_DEITY_MASK:
            return MM_FORM_FIERCE_DEITY;
        default:
            return MM_FORM_HUMAN;
    }
}

bool OoTMasksMergeOptionOn() {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MergeOoTMasks"), 1) != 0;
}

void GiveMergedMask(u8 mmForm) {
    if (!MergeShip_HasTransformationMask(mmForm)) {
        MergeShip_GiveTransformationMask(mmForm);
        SPDLOG_INFO("MergeShip: transformation mask {} owned", mmForm);
    }
}

// Every item received, vanilla or randomizer: the randomizer's transformation masks, and Ocarina of Time's
// Goron / Zora masks while they are the transformation masks.
void OnItemReceive(GetItemEntry entry) {
    if (IsMMMaskEntry(entry)) {
        GiveMergedMask(MMFormOfRandoMask(entry.getItemId));
        return;
    }
    if (IsMMRegularMaskEntry(entry)) {
        GiveRegularMMMask(MMMaskOfRandoMask(entry.getItemId));
        return;
    }
    if (!MergeShip_OoTMasksAreTransformationMasks()) {
        return;
    }
    bool goron = false;
    bool zora = false;
    if (entry.modIndex == MOD_NONE) {
        goron = (entry.itemId == ITEM_MASK_GORON);
        zora = (entry.itemId == ITEM_MASK_ZORA);
    } else if (entry.modIndex == MOD_RANDOMIZER) {
        goron = (entry.getItemId == RG_GORON_MASK);
        zora = (entry.getItemId == RG_ZORA_MASK);
    }
    if (goron) {
        GiveMergedMask(MM_FORM_GORON);
    } else if (zora) {
        GiveMergedMask(MM_FORM_ZORA);
    }
}

// Ocarina of Time's Goron / Zora Mask already held (the child trade slot, or the randomizer's flags) while they are the
// transformation masks: the transformation mask too.
void GiveOwnedOoTMasks() {
    if (!MergeShip_OoTMasksAreTransformationMasks()) {
        return;
    }
    u8 tradeChild = INV_CONTENT(ITEM_TRADE_CHILD);
    if ((tradeChild == ITEM_MASK_GORON) || (IS_RANDO && Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON))) {
        GiveMergedMask(MM_FORM_GORON);
    }
    if ((tradeChild == ITEM_MASK_ZORA) || (IS_RANDO && Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA))) {
        GiveMergedMask(MM_FORM_ZORA);
    }
}

// A file saved before these rules (or before the save section existed) may already hold the masks.
void OnLoadGame(int32_t fileNum) {
    (void)fileNum;
    if (IS_RANDO) {
        static const struct {
            RandomizerInf flag;
            u8 mmForm;
        } kFound[] = {
            { RAND_INF_OBTAINED_MM_DEKU_MASK, MM_FORM_DEKU },
            { RAND_INF_OBTAINED_MM_GORON_MASK, MM_FORM_GORON },
            { RAND_INF_OBTAINED_MM_ZORA_MASK, MM_FORM_ZORA },
            { RAND_INF_OBTAINED_MM_FIERCE_DEITY_MASK, MM_FORM_FIERCE_DEITY },
        };
        for (const auto& f : kFound) {
            if (Flags_GetRandomizerInf(f.flag)) {
                GiveMergedMask(f.mmForm);
            }
        }
        // Majora's Mask's other 20 masks: RAND_INF_OBTAINED_MM_MASK_OF_TRUTH.. are in the items' order.
        for (int rg = RG_MM_MASK_OF_TRUTH; rg <= RG_MM_GIANTS_MASK; rg++) {
            if (Flags_GetRandomizerInf(
                    (RandomizerInf)(RAND_INF_OBTAINED_MM_MASK_OF_TRUTH + (rg - RG_MM_MASK_OF_TRUTH)))) {
                GiveRegularMMMask(MMMaskOfRandoMask(rg));
            }
        }
    }
    GiveOwnedOoTMasks();
}

// MergeShip2: "Hyrule's Goron and Zora Masks Transform" takes effect the moment it is turned on: with a file loaded,
// a Goron or Zora Mask already held gives its transformation mask now, as loading the file does. Turning it off takes
// nothing back (MM's transformation masks are never taken back).
void ApplyMergeOoTMasksNow() {
    if (GameInteractor::IsSaveLoaded(true)) {
        GiveOwnedOoTMasks();
    }
}

bool HoldingUpMMMaskOutsideRando() {
    if (IS_RANDO || (gPlayState == nullptr)) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    // Every randomizer-table item held up outside a seed: the masks, and all of MM's other items and songs a
    // MergeShip feature or the harness gives through their RandomizerGet (their text id is
    // TEXT_RANDOMIZER_CUSTOM_ITEM, whose text only the randomizer's own hook builds, and only in a seed).
    return (player != nullptr) &&
           (IsMMMaskEntry(player->getItemEntry) || IsMMRegularMaskEntry(player->getItemEntry) ||
            ((player->getItemEntry.modIndex == MOD_RANDOMIZER) &&
             (player->getItemEntry.textId == TEXT_RANDOMIZER_CUSTOM_ITEM)));
}

// The randomizer's items say "You found <item>!" through its own text hook, which only runs in a seed.
// Outside one (a transformation mask given by another feature, or the test harness), the same text.
void BuildMMMaskGetItemMessage(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    if (!HoldingUpMMMaskOutsideRando()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    const Rando::Item& item = Rando::StaticData::RetrieveItem((RandomizerGet)player->getItemEntry.getItemId);
    CustomMessage msg("You found [[article]][[color]][[name]]%w!", "Du hast [[article]][[color]][[name]]%w gefunden!",
                      "Vous avez trouvé [[article]][[color]][[name]]%w!", TEXTBOX_TYPE_BLUE);
    msg.Replace("[[article]]", CustomMessage(item.GetArticle(), TEXTBOX_TYPE_BLUE));
    msg.Replace("[[color]]", item.GetColor());
    msg.Replace("[[name]]", CustomMessage(item.GetName(), TEXTBOX_TYPE_BLUE));
    // As the randomizer's own text (ItemMessages.cpp BuildCustomItemMessage): an icon only for an item that has one.
    // Without it the icon code is printed (ITEM_CUSTOM as a letter) and the first line is indented for nothing.
    if (Rando::StaticData::RetrieveItem((RandomizerGet)player->getItemEntry.getItemId).HasCustomIcon()) {
        msg.AutoFormat(ITEM_CUSTOM);
    } else {
        msg.AutoFormat();
    }
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterMergeShipMaskItems() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemReceive>(OnItemReceive);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_RANDOMIZER_CUSTOM_ITEM,
                                                                                BuildMMMaskGetItemMessage);
    REGISTER_VB_SHOULD(VB_LOAD_ITEM_ICON, {
        if (!*should && HoldingUpMMMaskOutsideRando()) {
            LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
        }
    });
    REGISTER_VB_SHOULD(VB_DRAW_ITEM_ICON, {
        if (!*should && HoldingUpMMMaskOutsideRando()) {
            DrawCustomItemIcon(va_arg(args, Gfx**));
        }
    });
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipMaskItems, {});
static RegisterShipInitFunc sMergeOoTMasksNow(ApplyMergeOoTMasksNow, { CVAR_ENHANCEMENT("MergeShip.MergeOoTMasks") });

} // namespace

extern "C" {

void MergeShip_GiDrawOpa0Xlu1(PlayState* play, const char* opa, const char* xlu) {
    DrawOpa0Xlu1(play, opa, xlu);
}

void MergeShip_GiDrawOpa01(PlayState* play, const char* first, const char* second) {
    DrawOpa01(play, first, second);
}

void MergeShip_DrawGiDekuMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MASK_SKULL);
        return;
    }
    DrawOpa0Xlu1(play, mm_gGiDekuMaskEmptyDL, mm_gGiDekuMaskDL);
}

void MergeShip_DrawGiGoronMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MASK_GORON);
        return;
    }
    DrawOpa0Xlu1(play, mm_gGiGoronMaskEmptyDL, mm_gGiGoronMaskDL);
}

void MergeShip_DrawGiZoraMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MASK_ZORA);
        return;
    }
    DrawOpa01(play, mm_gGiZoraMaskEmptyDL, mm_gGiZoraMaskDL);
}

void MergeShip_DrawGiFierceDeityMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    if (!MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, GID_MASK_TRUTH);
        return;
    }
    DrawOpa01(play, mm_gGiFierceDeityMaskFaceDL, mm_gGiFierceDeityMaskHairAndHatDL);
}

// Majora's Mask's other 20 masks. The entry's draw item (the mask itself, or the mask an ice trap looks like) picks
// the MM model; without mm.o2r the item's own Ocarina of Time gid (a mask whose object the item loads) is drawn.
void MergeShip_DrawGiMMMask(PlayState* play, GetItemEntry* getItemEntry) {
    int rg = getItemEntry->drawItemId;
    if (!IsMMRegularMaskRG(rg)) {
        rg = getItemEntry->getItemId;
    }
    if (!IsMMRegularMaskRG(rg) || !MergeShip_IsMajorasMaskMounted()) {
        GetItem_Draw(play, (s16)getItemEntry->gid);
        return;
    }
    const MMMaskGi& gi = kMMMaskGi[rg - RG_MM_MASK_OF_TRUTH];
    if (gi.opa0Xlu1) {
        DrawOpa0Xlu1(play, gi.dl0, gi.dl1);
    } else {
        DrawOpa01(play, gi.dl0, gi.dl1);
    }
}

int MergeShip_OoTMasksAreTransformationMasks(void) {
    if (!OoTMasksMergeOptionOn() || !MergeShip_IsMajorasMaskMounted()) {
        return false;
    }
    if (IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_TRANSFORMATION_MASKS)) {
        return false;
    }
    return true;
}

unsigned char MergeShip_ResolveMaskItem(unsigned char item) {
    if (((item == ITEM_MASK_GORON) || (item == ITEM_MASK_ZORA)) && MergeShip_OoTMasksAreTransformationMasks()) {
        return (item == ITEM_MASK_GORON) ? ITEM_MM_MASK_GORON : ITEM_MM_MASK_ZORA;
    }
    return item;
}

} // extern "C"
