// MergeShip2 -- Majora's Mask's songs, the SoH side (MergeShipSongs.h): MM's messages, the pause menu's names for MM's
// quest items, their get-item entries, and the once-per-frame update.
//
// Text: MM's English messages, read out of mm.o2r (mm/text/message_data_static) and re-encoded with Ocarina of
// Time's control codes: MM 0x11 newline -> 0x01, 0x10 / 0x12 box break -> 0x04, 0x17 / 0x18 quick text -> 0x08 /
// 0x09, colours 0x00 default / 0x01 red / 0x02 green -> 0x05 0x40 / 0x41 / 0x42, 0xC2 two choices -> 0x1B,
// 0x1C xx xx fade -> 0x0E xx, 0xB0 A button -> 0x9F, 0xBF end -> 0x02. MM text is English only; every language shows
// it. MM's box types map to Ocarina of Time's: type 3 (the song's own box) is Ocarina of Time's ocarina box, as
// Ocarina of Time's own "You played ..." boxes; MM's type 6 prompts (Ocarina of Time has no type 6) use the black box
// at the bottom that Ocarina of Time's own post-song prompts use ("Warp to ...?"); get-item texts use the blue box
// with the item's icon, as the Chateau Romani port.
//   "You played the ..."      MM 0x1B72 + MM song      (MM type 3)
//   0x1B95 0xFB 0x1B93 0x1B8B..0x1B8D 0x1B8E..0x1B92 0x1B94  (MM type 6)
//   GI 0x55..0x58 (remains), 0x50 (Bombers' Notebook)       (MM type 2)
// Names: MM item_name_static (English), as the Masks page and Chateau Romani.

#include <cstring>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include <z64.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipTermina.h"
#include "mm/archives/item_name_static/item_name_static.h"
extern PlayState* gPlayState;
}

namespace {

#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_BOX_BREAK "\x04"
#define MS_COLOR_DEFAULT "\x05\x40"
#define MS_COLOR_RED "\x05\x41"
#define MS_COLOR_GREEN "\x05\x42"
#define MS_QUICKTEXT_ENABLE "\x08"
#define MS_QUICKTEXT_DISABLE "\x09"
#define MS_FADE "\x0E"
#define MS_ITEM_ICON "\x13"
#define MS_TWO_CHOICE "\x1B"
#define MS_BTN_A "\x9F"

// "You played the ..." (MM 0x1B72 + MM song), laid out as Ocarina of Time's own (four lines, the song under them).
const char* const kPlayedTexts[MERGESHIP_SONG_COUNT] = {
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Sonata of" MS_NEWLINE "Awakening" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B72
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Goron Lullaby" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B73
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "New Wave Bossa" MS_NEWLINE "Nova" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B74
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Elegy of" MS_NEWLINE "Emptiness" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B75
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Oath to Order" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B76
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Song of Healing" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B79
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Song of Soaring" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B7B
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Inverted Song of" MS_NEWLINE "Time" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B7E
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Song of Double" MS_NEWLINE "Time" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B7F
    MS_QUICKTEXT_ENABLE "You played the " MS_COLOR_RED "Lullaby Intro" MS_COLOR_DEFAULT
                        "!" MS_NEWLINE MS_NEWLINE MS_NEWLINE MS_QUICKTEXT_DISABLE MS_END, // 0x1B80
};

// MM 0x1B8E..0x1B92, "Proceed to the ...?" (Yes / No).
const char* const kDoubleTimeTexts[5] = {
    "Proceed to the" MS_NEWLINE MS_COLOR_RED "Dawn of the Final Day" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_TWO_CHOICE
        MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END, // 0x1B8E
    "Proceed to the" MS_NEWLINE MS_COLOR_RED "Night of the Final Day" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_TWO_CHOICE
        MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END, // 0x1B8F
    "Proceed to the" MS_NEWLINE MS_COLOR_RED "Night of the 2nd Day" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_TWO_CHOICE
        MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END, // 0x1B90
    "Proceed to the" MS_NEWLINE MS_COLOR_RED "Night of the First Day" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_TWO_CHOICE
        MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END, // 0x1B91
    "Proceed to the" MS_NEWLINE MS_COLOR_RED "Dawn of the 2nd Day" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_TWO_CHOICE
        MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END, // 0x1B92
};

// MM GI 0x55..0x58: "You've seized <boss>'s Remains!" and the spirit it freed.
const char* const kRemainsBosses[4] = { "Odolwa", "Goht", "Gyorg", "Twinmold" };

std::string ItemIcon(u8 item) {
    return std::string(MS_ITEM_ICON) + static_cast<char>(item);
}

std::string GetItemText(u8 item) {
    if ((item >= ITEM_MM_REMAINS_ODOLWA) && (item <= ITEM_MM_REMAINS_TWINMOLD)) {
        const char* boss = kRemainsBosses[item - ITEM_MM_REMAINS_ODOLWA];
        return ItemIcon(item) + MS_QUICKTEXT_ENABLE "You've seized " MS_COLOR_RED + boss +
               "'s Remains" MS_COLOR_DEFAULT "!" MS_QUICKTEXT_DISABLE MS_BOX_BREAK + ItemIcon(item) +
               "You have just freed the innocent" MS_NEWLINE "spirit that this dark mask had" MS_NEWLINE
               "kept imprisoned within the body" MS_NEWLINE "of evil " +
               boss + "." MS_END;
    }
    // MM GI 0x50 (MM 0x1F 0x00 0x0A, a ten-frame pause, has no Ocarina of Time counterpart and is left out).
    return ItemIcon(item) + MS_QUICKTEXT_ENABLE "You got the " MS_COLOR_RED "Bombers' Notebook" MS_COLOR_DEFAULT
                            "!" MS_QUICKTEXT_DISABLE MS_NEWLINE "It allows you to keep track of" MS_NEWLINE
                            "people's schedules." MS_BOX_BREAK +
           ItemIcon(item) + "Press " MS_COLOR_RED "START" MS_COLOR_DEFAULT " to access the " MS_COLOR_GREEN
                            "Quest" MS_NEWLINE "Status Screen" MS_COLOR_DEFAULT ", then highlight the" MS_NEWLINE
                            MS_COLOR_RED "Notebook " MS_COLOR_DEFAULT "and press " MS_BTN_A " to read it." MS_END;
}

void LoadText(const std::string& text, TextBoxType type, TextBoxPosition pos, bool* loadFromMessageTable) {
    CustomMessage msg(text, text, text, type, pos);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildMessage(uint16_t* textId, bool* loadFromMessageTable) {
    u16 id = *textId;
    std::string text;

    if ((id >= MERGESHIP_TEXT_SONG_PLAYED_BASE) && (id < MERGESHIP_TEXT_SONG_PLAYED_BASE + MERGESHIP_SONG_COUNT)) {
        LoadText(kPlayedTexts[id - MERGESHIP_TEXT_SONG_PLAYED_BASE], TEXTBOX_TYPE_OCARINA, TEXTBOX_POS_VARIABLE,
                 loadFromMessageTable);
        return;
    }
    if ((id >= MERGESHIP_TEXT_DOUBLE_TIME_BASE) && (id < MERGESHIP_TEXT_DOUBLE_TIME_BASE + 5)) {
        LoadText(kDoubleTimeTexts[id - MERGESHIP_TEXT_DOUBLE_TIME_BASE], TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM,
                 loadFromMessageTable);
        return;
    }
    if ((id >= MERGESHIP_TEXT_GET_BASE) && (id < MERGESHIP_TEXT_GET_BASE + 5)) {
        LoadText(GetItemText((u8)(ITEM_MM_REMAINS_ODOLWA + (id - MERGESHIP_TEXT_GET_BASE))), TEXTBOX_TYPE_BLUE,
                 TEXTBOX_POS_BOTTOM, loadFromMessageTable);
        return;
    }
    switch (id) {
        case MERGESHIP_TEXT_NOTHING_HAPPENED: // MM 0x1B95
            text = "Your notes echoed far..." MS_NEWLINE "but nothing happened." MS_END;
            break;
        case MERGESHIP_TEXT_NO_OWLS: // MM 0xFB
            text = "You have yet to leave your " MS_COLOR_RED "mark" MS_NEWLINE MS_COLOR_DEFAULT
                   "on any of the " MS_COLOR_RED "statues " MS_COLOR_DEFAULT "you have" MS_NEWLINE
                   "come across. Hurry up and find" MS_NEWLINE "some." MS_END;
            break;
        case MERGESHIP_TEXT_SOAR_TO: // MM 0x1B93, MM's 0xD4 being the owl under the cursor (MM sOwlWarpTextENG)
            text = std::string("Soar to " MS_COLOR_GREEN) + MergeShip_Termina_OwlName(MergeShip_OwlWarp_GetCursorOwl()) +
                   MS_COLOR_DEFAULT "?" MS_NEWLINE " " MS_NEWLINE MS_TWO_CHOICE MS_COLOR_GREEN "Yes" MS_NEWLINE
                   "No" MS_COLOR_DEFAULT MS_END;
            break;
        case MERGESHIP_TEXT_TIME_SLOW: // MM 0x1B8C
            text = MS_COLOR_RED "Slow " MS_COLOR_DEFAULT "the current flow of time?" MS_NEWLINE " " MS_NEWLINE
                   MS_TWO_CHOICE MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END;
            break;
        case MERGESHIP_TEXT_TIME_NORMAL: // MM 0x1B8D
            text = "Return the flow of time to" MS_NEWLINE MS_COLOR_RED "normal" MS_COLOR_DEFAULT "?" MS_NEWLINE
                   MS_TWO_CHOICE MS_COLOR_GREEN "Yes" MS_NEWLINE "No" MS_COLOR_DEFAULT MS_END;
            break;
        case MERGESHIP_TEXT_TIME_NOTHING: // MM 0x1B8B (MM 0x1C 0x00 0x1E: fades after 30 frames)
            text = "But nothing happened..." MS_FADE "\x1E" MS_END;
            break;
        case MERGESHIP_TEXT_TIME_NO_FURTHER: // MM 0x1B94
            text = "You cannot proceed any further" MS_NEWLINE "through time..." MS_FADE "\x1E" MS_END;
            break;
        default:
            return;
    }
    LoadText(text, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM, loadFromMessageTable);
}

// MM item_name_static, for the Termina quest sub-page's name panel.
const char* NameTexture(u32 item) {
    switch (item) {
        case ITEM_MM_REMAINS_ODOLWA:
            return mm_gItemNameOdolwasRemainsENGTex;
        case ITEM_MM_REMAINS_GOHT:
            return mm_gItemNameGohtsRemainsENGTex;
        case ITEM_MM_REMAINS_GYORG:
            return mm_gItemNameGyorgsRemainsENGTex;
        case ITEM_MM_REMAINS_TWINMOLD:
            return mm_gItemNameTwinmoldsRemainsENGTex;
        case ITEM_MM_BOMBERS_NOTEBOOK:
            return mm_gItemNameBombersNotebookENGTex;
        case ITEM_MM_SONG_SONATA:
            return mm_gItemNameSonataOfAwakeningENGTex;
        case ITEM_MM_SONG_GORON_LULLABY:
            return mm_gItemNameGoronLullabyENGTex;
        case ITEM_MM_SONG_NEW_WAVE:
            return mm_gItemNameNewWaveBossaNovaENGTex;
        case ITEM_MM_SONG_ELEGY:
            return mm_gItemNameElegyOfEmptynessENGTex;
        case ITEM_MM_SONG_OATH:
            return mm_gItemNameOathToOrderENGTex;
        case ITEM_MM_SONG_HEALING:
            return mm_gItemNameSongOfHealingENGTex;
        case ITEM_MM_SONG_SOARING:
            return mm_gItemNameSongOfSoaringENGTex;
        case ITEM_MM_SONG_GORON_LULLABY_INTRO:
            return mm_gItemNameLullabyIntroENGTex;
        default:
            return nullptr;
    }
}

void RegisterGetItems() {
    static const u8 kItems[] = { ITEM_MM_REMAINS_ODOLWA, ITEM_MM_REMAINS_GOHT, ITEM_MM_REMAINS_GYORG,
                                 ITEM_MM_REMAINS_TWINMOLD, ITEM_MM_BOMBERS_NOTEBOOK };
    for (u8 item : kItems) {
        GetItemEntry entry = MergeShip_Songs_GetItemEntry(item);
        ItemTableManager::Instance->AddItemEntry(MOD_NONE, entry.getItemId, entry);
    }
}

void RegisterMergeShipSongs() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    for (u16 id = MERGESHIP_TEXT_SONG_PLAYED_BASE; id <= MERGESHIP_TEXT_LAST; id++) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildMessage);
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        (void)fileNum;
        RegisterGetItems();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { MergeShip_Songs_Update(gPlayState); });
    // KaleidoScope_UpdateNamePanel: MM's quest items are past Ocarina of Time's name table.
    REGISTER_VB_SHOULD(VB_DRAW_CUSTOM_ITEM_NAME, {
        u32 namedItem = va_arg(args, u32);
        const char* tex = NameTexture(namedItem);
        if ((tex != nullptr) && (gPlayState != nullptr)) {
            *should = true;
            memcpy(gPlayState->pauseCtx.nameSegment, tex, strlen(tex) + 1);
        }
    });
}

RegisterShipInitFunc sInitFunc(RegisterMergeShipSongs, {});

} // namespace
