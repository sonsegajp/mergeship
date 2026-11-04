// MergeShip2 (soh_fd) -- the text boxes of soh_fd's form rules: Navi refusing Epona and the Master Sword to a form
// ("Prevent Restricted Actions"), Navi offering to turn Fierce Deity back into Link where he could be stuck in deep
// water ("Stuck Safeguards"; "Yes" is handled in z_en_elf.c), and the fishing-pond owner stopping Fierce Deity at
// the door (Majora's Mask 3D's fishing holes). Text is soh_fd's (TransformationMaskSafeguards.cpp), itself verbatim
// from the Aegiker hack's 0x71B3-0x71B5 plus the fishing line. The ids are free in Ocarina of Time's tables.
//
// SoH CustomMessage codes: %c light blue, %w white, %g green (choices), @ the player's name, & newline, ^ new box,
// \x1B two-choice.

#include "soh/MergeShip/MergeShipSohFdMessages.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"

extern "C" {
#include "z64.h"
}

#define MERGESHIP_SOHFD_MSG_TABLE_ID "MergeShipSohFd"

namespace {

void ServeMessage(uint16_t id, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessageManager::Instance->RetrieveMessage(MERGESHIP_SOHFD_MSG_TABLE_ID, id, MF_FORMATTED);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void RegisterMergeShipSohFdMessages() {
    CustomMessageManager* cmm = CustomMessageManager::Instance;
    cmm->AddCustomMessageTable(MERGESHIP_SOHFD_MSG_TABLE_ID);

    // Epona, refused to the big forms (Fierce Deity, Goron).
    cmm->CreateMessage(MERGESHIP_SOHFD_MSG_TABLE_ID, MERGESHIP_TEXT_FORM_TOO_BIG,
                       CustomMessage("%cYou're too big!!", TEXTBOX_TYPE_BLACK));
    // Epona refused to the small forms, and the Master Sword's pedestal refused to every form.
    cmm->CreateMessage(MERGESHIP_SOHFD_MSG_TABLE_ID, MERGESHIP_TEXT_FORM_CANT_DO_THAT,
                       CustomMessage("%cYou can't do that in your&current form!", TEXTBOX_TYPE_BLACK));
    // The deep-water turn-back offer; "Yes" (choice 0) turns the form back into Link.
    cmm->CreateMessage(MERGESHIP_SOHFD_MSG_TABLE_ID, MERGESHIP_TEXT_FORM_WATER_WARNING,
                       CustomMessage("%c@... I'm not sure you can&climb out of this water in&your current form!^"
                                     "%cDo you want to turn back&into a %whuman%c?&\x1B%gYes&No%g",
                                     TEXTBOX_TYPE_BLACK));
    // The fishing-pond owner, to a Fierce Deity heading for the door.
    cmm->CreateMessage(MERGESHIP_SOHFD_MSG_TABLE_ID, MERGESHIP_TEXT_FISHING_SCARY_FACE,
                       CustomMessage("Hey. Hey! What's with that&scary face, fella?^"
                                     "If ya leave here lookin' like&that, you'll scare away&"
                                     "potential customers and&ruin our reputation!",
                                     TEXTBOX_TYPE_BLACK));

    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_FORM_TOO_BIG, true,
                 [](u16* textId, bool* loadFromMessageTable) { ServeMessage(*textId, loadFromMessageTable); });
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_FORM_CANT_DO_THAT, true,
                 [](u16* textId, bool* loadFromMessageTable) { ServeMessage(*textId, loadFromMessageTable); });
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_FORM_WATER_WARNING, true,
                 [](u16* textId, bool* loadFromMessageTable) { ServeMessage(*textId, loadFromMessageTable); });
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_FISHING_SCARY_FACE, true,
                 [](u16* textId, bool* loadFromMessageTable) { ServeMessage(*textId, loadFromMessageTable); });
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipSohFdMessages);

} // namespace
