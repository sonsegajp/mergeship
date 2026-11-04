// MergeShip2 -- the "mergeShip" save section: the form the player is in, the transformation masks
// they own, and the button assignments each Majora's Mask form keeps for itself.
//
// A section of its own rather than new fields in SaveContext: Ocarina of Time's save stays
// byte-for-byte what it was, and a file saved before any of this existed loads with every value
// at its MM new-file default.

#include "soh/SaveManager.h"
#include <random>

#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
}

// This file's id in its slot's shared save (MsSharedSave.hyruleFileId; MergeShipHost_ClaimSharedSave), 0 for a file from
// before it. Kept outside gMergeShipSave so the struct every form file includes stays as it is.
static u32 sSharedFileId = 0;
extern "C" u32 MergeShip_Save_SharedFileId(void) {
    return sSharedFileId;
}

namespace {

void InitMergeShipSave(bool isDebug) {
    (void)isDebug;
    gMergeShipSave.mmForm = MM_FORM_HUMAN;
    gMergeShipSave.ownedMasks = 0;
    gMergeShipSave.transformSeen = 0;
    // MM sSaveDefaultItemEquips B column. Human's B is OoT's own and lives in gSaveContext.equips.
    gMergeShipSave.bButtonItem[MM_FORM_FIERCE_DEITY] = ITEM_MM_SWORD_FIERCE_DEITY;
    // MM's Goron and Zora B is ITEM_SWORD_KOKIRI: never drawn (their B label covers it), it makes B a
    // Goron punch, and Player_ItemToItemAction turns it into the Zora's fin boomerangs.
    gMergeShipSave.bButtonItem[MM_FORM_GORON] = ITEM_SWORD_KOKIRI;
    gMergeShipSave.bButtonItem[MM_FORM_ZORA] = ITEM_SWORD_KOKIRI;
    gMergeShipSave.bButtonItem[MM_FORM_DEKU] = ITEM_MM_FD;
    gMergeShipSave.bButtonItem[MM_FORM_HUMAN] = ITEM_NONE;
    gMergeShipSave.ownedMMMasks = 0;
    gMergeShipSave.guardSawMMKeaton = 0;
    gMergeShipSave.equippedMMMask = MM_MASK_NONE;
    // MergeShip2: MM's equipment (MergeShipMMEquipment.h): none owned.
    gMergeShipSave.mmEquipOwned = 0;
    gMergeShipSave.mmEquipSelected = 0;
    gMergeShipSave.mmEquipSlotOnlyMM = 0;
    gMergeShipSave.mmRazorHealth = 0;
    // MergeShip2: MM's songs and quest items (MergeShipSongs.h); MM's new-file values.
    gMergeShipSave.mmSongs = 0;
    gMergeShipSave.mmQuestItems = 0;
    gMergeShipSave.owlActivationFlags = 0;
    // A new file's own id for its slot's shared save (MsSharedSave.hyruleFileId): nonzero, random. A loaded file reads
    // its saved one over it.
    static std::mt19937 sFileIds{ std::random_device{}() };
    do {
        sSharedFileId = (u32)sFileIds();
    } while (sSharedFileId == 0);
    gMergeShipSave.timeSpeedOffset = 0;
    // MergeShip2: MM's stray fairies (MergeShipStrayFairy.h); MM Sram_InitNewSave clears inventory.strayFairies.
    for (int area = 0; area < (int)sizeof(gMergeShipSave.strayFairies); area++) { // u8 per area
        gMergeShipSave.strayFairies[area] = 0;
    }
    gMergeShipSave.adultMaskTimelineAge = 0xFF; // Link is his real age
}

void LoadMergeShipVersion1() {
    SaveManager::Instance->LoadData("mmForm", gMergeShipSave.mmForm, (u8)MM_FORM_HUMAN);
    SaveManager::Instance->LoadData("ownedMasks", gMergeShipSave.ownedMasks, (u8)0);
    SaveManager::Instance->LoadData("transformSeen", gMergeShipSave.transformSeen, (u8)0);
    SaveManager::Instance->LoadArray("bButtonItem", MM_FORM_MAX, [](size_t f) {
        SaveManager::Instance->LoadData("", gMergeShipSave.bButtonItem[f], gMergeShipSave.bButtonItem[f]);
    });
    SaveManager::Instance->LoadData("ownedMMMasks", gMergeShipSave.ownedMMMasks, (u32)0);
    SaveManager::Instance->LoadData("guardSawMMKeaton", gMergeShipSave.guardSawMMKeaton, (u8)0);
    SaveManager::Instance->LoadData("equippedMMMask", gMergeShipSave.equippedMMMask, (u8)MM_MASK_NONE);
    if (gMergeShipSave.equippedMMMask >= MM_MASK_MAX) {
        gMergeShipSave.equippedMMMask = MM_MASK_NONE;
    }
    if (gMergeShipSave.mmForm >= MM_FORM_MAX) {
        gMergeShipSave.mmForm = MM_FORM_HUMAN;
    }
    // MergeShip2: MM's equipment (MergeShipMMEquipment.h); a file saved before it existed owns none.
    SaveManager::Instance->LoadData("mmEquipOwned", gMergeShipSave.mmEquipOwned, (u8)0);
    SaveManager::Instance->LoadData("mmEquipSelected", gMergeShipSave.mmEquipSelected, (u8)0);
    SaveManager::Instance->LoadData("mmEquipSlotOnlyMM", gMergeShipSave.mmEquipSlotOnlyMM, (u8)0);
    SaveManager::Instance->LoadData("mmRazorHealth", gMergeShipSave.mmRazorHealth, (u8)0);
    // MergeShip2: MM's songs and quest items. A file saved before they existed loads MM's new-file values.
    SaveManager::Instance->LoadData("mmSongs", gMergeShipSave.mmSongs, (u16)0);
    SaveManager::Instance->LoadData("mmQuestItems", gMergeShipSave.mmQuestItems, (u8)0);
    SaveManager::Instance->LoadData("owlActivationFlags", gMergeShipSave.owlActivationFlags, (u16)0);
    SaveManager::Instance->LoadData("sharedFileId", sSharedFileId, (u32)0);
    SaveManager::Instance->LoadData("timeSpeedOffset", gMergeShipSave.timeSpeedOffset, (s8)0);
    if ((gMergeShipSave.timeSpeedOffset != 0) && (gMergeShipSave.timeSpeedOffset != -2)) {
        gMergeShipSave.timeSpeedOffset = 0;
    }
    // MergeShip2: MM's stray fairies per area; a file saved before they existed has none.
    SaveManager::Instance->LoadArray("strayFairies", sizeof(gMergeShipSave.strayFairies), [](size_t area) {
        SaveManager::Instance->LoadData("", gMergeShipSave.strayFairies[area], (u8)0);
    });
    // MergeShip2: the Adult Mask's kept timeline; a file saved before it existed is at its real age.
    SaveManager::Instance->LoadData("adultMaskTimelineAge", gMergeShipSave.adultMaskTimelineAge, (u8)0xFF);
}

void SaveMergeShip(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("mmForm", gMergeShipSave.mmForm);
    SaveManager::Instance->SaveData("ownedMasks", gMergeShipSave.ownedMasks);
    SaveManager::Instance->SaveData("transformSeen", gMergeShipSave.transformSeen);
    SaveManager::Instance->SaveArray("bButtonItem", MM_FORM_MAX,
                                     [](size_t f) { SaveManager::Instance->SaveData("", gMergeShipSave.bButtonItem[f]); });
    SaveManager::Instance->SaveData("ownedMMMasks", gMergeShipSave.ownedMMMasks);
    SaveManager::Instance->SaveData("guardSawMMKeaton", gMergeShipSave.guardSawMMKeaton);
    SaveManager::Instance->SaveData("equippedMMMask", gMergeShipSave.equippedMMMask);
    SaveManager::Instance->SaveData("mmEquipOwned", gMergeShipSave.mmEquipOwned);
    SaveManager::Instance->SaveData("mmEquipSelected", gMergeShipSave.mmEquipSelected);
    SaveManager::Instance->SaveData("mmEquipSlotOnlyMM", gMergeShipSave.mmEquipSlotOnlyMM);
    SaveManager::Instance->SaveData("mmRazorHealth", gMergeShipSave.mmRazorHealth);
    SaveManager::Instance->SaveData("mmSongs", gMergeShipSave.mmSongs);
    SaveManager::Instance->SaveData("mmQuestItems", gMergeShipSave.mmQuestItems);
    SaveManager::Instance->SaveData("owlActivationFlags", gMergeShipSave.owlActivationFlags);
    SaveManager::Instance->SaveData("sharedFileId", sSharedFileId);
    SaveManager::Instance->SaveData("timeSpeedOffset", gMergeShipSave.timeSpeedOffset);
    SaveManager::Instance->SaveArray("strayFairies", sizeof(gMergeShipSave.strayFairies), [](size_t area) {
        SaveManager::Instance->SaveData("", gMergeShipSave.strayFairies[area]);
    });
    SaveManager::Instance->SaveData("adultMaskTimelineAge", gMergeShipSave.adultMaskTimelineAge);
}

// Quick play: with gGeneral.MergeShipDpadMasks set, every loaded file owns the four transformation
// masks and has them on the D-pad (Up Deku, Down Goron, Left Zora, Right Fierce Deity), with SoH's
// D-pad equips turned on. For trying the forms before the masks have a place in the quest.
void EquipMasksOnDpad() {
    if (!CVarGetInteger(CVAR_GENERAL("MergeShipDpadMasks"), 0)) {
        return;
    }
    static const u8 kDpadMasks[4] = { ITEM_MM_MASK_DEKU, ITEM_MM_MASK_GORON, ITEM_MM_MASK_ZORA,
                                      ITEM_MM_MASK_FIERCE_DEITY };
    if (IS_RANDO) {
        // Inside a randomizer seed the masks are found (Shuffle Transformation Masks), never given at the start:
        // only the masks already owned go on the D-pad, and only onto an empty D-pad button.
        for (int i = 0; i < 4; i++) {
            if (MergeShip_HasTransformationMask((u8)(kDpadMasks[i] - ITEM_MM_MASK_FIERCE_DEITY)) &&
                (gSaveContext.equips.buttonItems[4 + i] == ITEM_NONE)) {
                gSaveContext.equips.buttonItems[4 + i] = kDpadMasks[i];
                gSaveContext.equips.cButtonSlots[3 + i] = SLOT_NONE;
            }
        }
        return;
    }
    for (int f = MM_FORM_FIERCE_DEITY; f <= MM_FORM_DEKU; f++) {
        MergeShip_GiveTransformationMask((u8)f);
    }
    for (int i = 0; i < 4; i++) {
        // buttonItems[4..7] are D-pad Up, Down, Left, Right; their inventory slot is none (masks are
        // not in the item grid, and every slot lookup is limited to bottles and trade items).
        gSaveContext.equips.buttonItems[4 + i] = kDpadMasks[i];
        gSaveContext.equips.cButtonSlots[3 + i] = SLOT_NONE;
    }
    if (!CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0)) {
        CVarSetInteger(CVAR_ENHANCEMENT("DpadEquips"), 1);
    }
}

// Quick play: with gGeneral.MergeShipAllMMMasks set, every loaded file outside a randomizer seed owns
// Majora's Mask's other twenty masks (the Masks page shows them; they are equipped from there).
void GiveAllMMMasks() {
    if (!CVarGetInteger(CVAR_GENERAL("MergeShipAllMMMasks"), 0) || IS_RANDO) {
        return;
    }
    for (int m = MM_MASK_TRUTH; m < MM_MASK_MAX; m++) {
        MergeShip_GiveMMMask((u8)m);
    }
}

void RegisterMergeShipSave() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    SaveManager::Instance->AddLoadFunction("mergeShip", 1, LoadMergeShipVersion1);
    SaveManager::Instance->AddSaveFunction("mergeShip", 1, SaveMergeShip, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(InitMergeShipSave);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        (void)fileNum;
        EquipMasksOnDpad();
        GiveAllMMMasks();
    });
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipSave, {});

// MergeShip2: "Masks on D-pad" and "All Majora's Mask Masks" take effect the moment they are turned on in the menu:
// with a file loaded, turning one on does now what loading the file does. Turning one off takes nothing away (a
// reload does not either: what was granted is in the save). ShipInit also runs this at boot and on a config import,
// where there is no file loaded (boot) or it grants what the option says (import).
void ApplyDpadMasksNow() {
    if (GameInteractor::IsSaveLoaded(true)) {
        EquipMasksOnDpad();
    }
}

void ApplyAllMMMasksNow() {
    if (GameInteractor::IsSaveLoaded(true)) {
        GiveAllMMMasks();
    }
}

static RegisterShipInitFunc sDpadMasksNow(ApplyDpadMasksNow, { CVAR_GENERAL("MergeShipDpadMasks") });
static RegisterShipInitFunc sAllMMMasksNow(ApplyAllMMMasksNow, { CVAR_GENERAL("MergeShipAllMMMasks") });

} // namespace
