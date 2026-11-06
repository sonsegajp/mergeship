// MergeShip2 -- Majora's Mask's Chateau Romani: its get-item text, its pause-menu name, its item table entry, and when
// the drink's effect ends (MergeShipChateau.h; the item and the drink are in MergeShipChateau.c).
//
// Text: MM's English message 0x91 (GI_CHATEAU), read out of mm.o2r (mm/text/message_data_static) and re-encoded with
// Ocarina of Time's control codes, laid out as Ocarina of Time's own get-item messages are (the item icon opening each
// box, quick text until the item's name):
//   MM:  0x17 "Your bottle was filled with" 0x11 0x01 "Chateau Romani" 0x00 "!" 0x18 0x11
//        "Drink it to replenish all of your" 0x11 "magic power." 0x10
//        "The drink even improves your" 0x11 "stamina! Swirl it around with your" 0x11
//        "tongue to enjoy its flavor to the" 0x11 "fullest!" 0xBF
//   (MM textbox type 2 = Ocarina of Time's blue item box; MM icon 0x6F = ITEM_CHATEAU.)
//   MM 0x17/0x18 quick text on/off -> OoT 0x08/0x09, 0x11 newline -> 0x01, 0x10 box break -> 0x04, 0x01/0x00 red/
//   default colour -> 0x05 0x41 / 0x05 0x40, 0xBF end -> 0x02; the icon is OoT's 0x13 ITEM_ICON with ITEM_MM_CHATEAU
//   (drawn 32x32 like MM's, see z_message_PAL.c). MM text is English only; every language shows it.
// Name: MM item_name_static gItemNameChateauRomaniENGTex (English only in 2S2H's archive). Chateau Romani is not a
// mask, so it keeps its own name (no " (MM)").

#include <cstring>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/ShipInit.hpp"
#include <mergeship/SharedSave.h>           // BUG-0153: chateauMagic
#include "soh/MergeShip/MergeShipHost.h"    // MergeShipHost_SharedSave

extern "C" {
#include <z64.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/MergeShip/MergeShipChateau.h"
#include "mm/archives/item_name_static/item_name_static.h"
extern PlayState* gPlayState;
}

namespace {

// OoT message control codes (include/message_data_fmt.h). Kept as separate literals so a following letter is never
// read as part of a hex escape.
#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_BOX_BREAK "\x04"
#define MS_COLOR_RED "\x05\x41"
#define MS_COLOR_DEFAULT "\x05\x40"
#define MS_QUICKTEXT_ENABLE "\x08"
#define MS_QUICKTEXT_DISABLE "\x09"
#define MS_ITEM_ICON "\x13"

// MM 0x91, see the top of the file.
const std::string& ChateauFilledText() {
    static const std::string text = std::string(MS_ITEM_ICON) + static_cast<char>(ITEM_MM_CHATEAU) +
                                    MS_QUICKTEXT_ENABLE "Your bottle was filled with" MS_NEWLINE MS_COLOR_RED
                                    "Chateau Romani" MS_COLOR_DEFAULT "!" MS_QUICKTEXT_DISABLE MS_NEWLINE
                                    "Drink it to replenish all of your" MS_NEWLINE "magic power." MS_BOX_BREAK +
                                    std::string(MS_ITEM_ICON) + static_cast<char>(ITEM_MM_CHATEAU) +
                                    "The drink even improves your" MS_NEWLINE "stamina! Swirl it around with your" MS_NEWLINE
                                    "tongue to enjoy its flavor to the" MS_NEWLINE "fullest!" MS_END;
    return text;
}

void BuildChateauFilledMessage(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    const std::string& text = ChateauFilledText();
    CustomMessage msg(text, text, text, TEXTBOX_TYPE_BLUE, TEXTBOX_POS_BOTTOM);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// The get-item entry in the MOD_NONE item table, so every ItemTable_Retrieve(MERGESHIP_GI_MM_CHATEAU) (the player
// re-reading its pending item, Anchor's item packets) finds it. The vanilla table exists once a file is loaded.
void RegisterChateauGetItem() {
    ItemTableManager::Instance->AddItemEntry(MOD_NONE, MERGESHIP_GI_MM_CHATEAU, MergeShip_Chateau_GetItemEntry());
}

void OnLoadGame(int32_t fileNum) {
    (void)fileNum;
    RegisterChateauGetItem();
    // MM: WEEKEVENTREG_DRANK_CHATEAU_ROMANI belongs to the cycle; here, to the play session (MergeShipChateau.h).
    MergeShip_Chateau_Reset();
    // BUG-0153: in mergeship.exe, as the shared save holds it (Termina's cycle may still have it). The shared save's
    // own apply (MergeShipHost.cpp's load hook) sets the same; this hook may come after it.
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());
    if ((shared != nullptr) && (shared->lastWriter != 0) && (shared->chateauMagic & MS_CHATEAU_VALID)) {
        MergeShip_Chateau_SetDrunk((shared->chateauMagic & MS_CHATEAU_ACTIVE) != 0);
    }
}

void OnExitGame(int32_t fileNum) {
    (void)fileNum;
    MergeShip_Chateau_Reset();
}

// Link has died (the game over screen, not a fairy's revival, which never enters these states).
void OnGameFrameUpdate() {
    if ((gPlayState != nullptr) && (gPlayState->gameOverCtx.state >= GAMEOVER_DEATH_START) &&
        (gPlayState->gameOverCtx.state <= GAMEOVER_DEATH_MENU)) {
        MergeShip_Chateau_Reset();
    }
}

void RegisterMergeShipChateau() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>(OnExitGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnGameFrameUpdate);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_CHATEAU_FILLED,
                                                                                BuildChateauFilledMessage);
    // KaleidoScope_UpdateNamePanel: ITEM_MM_CHATEAU is past Ocarina of Time's name table.
    REGISTER_VB_SHOULD(VB_DRAW_CUSTOM_ITEM_NAME, {
        u32 namedItem = va_arg(args, u32);
        if (namedItem == ITEM_MM_CHATEAU) {
            *should = true;
            memcpy(gPlayState->pauseCtx.nameSegment, mm_gItemNameChateauRomaniENGTex,
                   strlen(mm_gItemNameChateauRomaniENGTex) + 1);
        }
    });
}

RegisterShipInitFunc sInitFunc(RegisterMergeShipChateau, {});

} // namespace
