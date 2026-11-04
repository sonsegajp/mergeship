// MergeShip2 -- Majora's Mask's Pictograph Box: the SoH side (MergeShipPicto.h; the item's MM code is MergeShipPicto.c).
//
// Save: the "mergeShipPicto" section holds MM's gSaveContext.pictoPhotoI5 (the photo, 160x112 compressed to 5 bits a
// pixel, base64 in the file), MM's QUEST_PICTOGRAPH (a photo is kept) and MM's saveInfo.pictoFlags0/1. A section of its
// own: Ocarina of Time's save is unchanged, and a file from before loads with no photo (MM's new-file state).
//
// Text: MM's English message 0xF8, read out of mm.o2r (mm/text/message_data_static):
//   header 0x0671 (MM textbox type 6, a black box; position 7, MM's fixed y 158 at the bottom; unskippable)
//   "Keep this" 0x01 " picture" 0x00 "?" 0x11 0x02 " " 0x11 0xC2 "Yes" 0x11 "No" 0xBF
// re-encoded with Ocarina of Time's control codes: 0x01 red -> 0x05 0x41, 0x00 default -> 0x05 0x40, 0x02 green ->
// 0x05 0x42, 0x11 newline -> 0x01, 0xC2 two choices -> 0x1B, 0xBF end -> 0x02; a black box at the bottom. MM text is
// English only; every language shows it.
//
// Hooks: OnInterfaceUpdate (MM Interface_UpdateButtonsPart1's picto branches), OnPlayDrawEnd (the frame capture, after
// the world and before the HUD), OnSceneInit (MM Play_Init), OnLoadGame / OnExitGame (nothing in progress).

#include <cstring>
#include <string>

#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
#include <z64.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/MergeShip/MergeShipPicto.h"
extern PlayState* gPlayState;
}

namespace {

// OoT message control codes (include/message_data_fmt.h), kept as separate literals so a following letter is never read
// as part of a hex escape.
#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_COLOR_RED "\x05\x41"
#define MS_COLOR_GREEN "\x05\x42"
#define MS_COLOR_DEFAULT "\x05\x40"
#define MS_TWO_CHOICE "\x1B"

// MM 0xF8, see the top of the file.
const std::string& PictoKeepText() {
    static const std::string text = "Keep this" MS_COLOR_RED " picture" MS_COLOR_DEFAULT "?" MS_NEWLINE MS_COLOR_GREEN
                                    " " MS_NEWLINE MS_TWO_CHOICE "Yes" MS_NEWLINE "No" MS_END;
    return text;
}

void BuildPictoKeepMessage(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    const std::string& text = PictoKeepText();
    CustomMessage msg(text, text, text, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// ---- save ----------------------------------------------------------------------------------------------------------

const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(const u8* data, size_t size) {
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (size_t i = 0; i < size; i += 3) {
        u32 n = (u32)data[i] << 16;
        if (i + 1 < size) {
            n |= (u32)data[i + 1] << 8;
        }
        if (i + 2 < size) {
            n |= (u32)data[i + 2];
        }
        out.push_back(kBase64Chars[(n >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < size) ? kBase64Chars[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < size) ? kBase64Chars[n & 0x3F] : '=');
    }
    return out;
}

// Decodes into `data` (at most `size` bytes); anything past the string stays as it is.
void DecodeBase64(const std::string& text, u8* data, size_t size) {
    u32 n = 0;
    s32 bits = 0;
    size_t pos = 0;
    for (char c : text) {
        const char* found = (c != '\0') ? std::strchr(kBase64Chars, c) : nullptr;
        if (found == nullptr) {
            continue; // '=' padding, whitespace
        }
        n = (n << 6) | (u32)(found - kBase64Chars);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (pos < size) {
                data[pos++] = (u8)((n >> bits) & 0xFF);
            }
        }
    }
}

// MM Sram_InitNewSave: no photo, QUEST_PICTOGRAPH clear, pictoFlags clear.
void InitPictoSave(bool isDebug) {
    (void)isDebug;
    std::memset(gMergeShipPictoPhotoI5, 0, sizeof(gMergeShipPictoPhotoI5));
    gMergeShipPictoHasPhoto = false;
    gMergeShipPictoFlags[0] = 0;
    gMergeShipPictoFlags[1] = 0;
}

void LoadPictoVersion1() {
    std::string photo;

    std::memset(gMergeShipPictoPhotoI5, 0, sizeof(gMergeShipPictoPhotoI5));
    SaveManager::Instance->LoadData("hasPhoto", gMergeShipPictoHasPhoto, (u8)0);
    SaveManager::Instance->LoadData("photoI5", photo, std::string());
    DecodeBase64(photo, gMergeShipPictoPhotoI5, MERGESHIP_PICTO_PHOTO_COMPRESSED_SIZE);
    SaveManager::Instance->LoadArray("pictoFlags", 2, [](size_t i) {
        SaveManager::Instance->LoadData("", gMergeShipPictoFlags[i], (u32)0);
    });
    if (gMergeShipPictoHasPhoto > 1) {
        gMergeShipPictoHasPhoto = 1;
    }
}

void SavePicto(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("hasPhoto", gMergeShipPictoHasPhoto);
    // Only a kept photo is written; MM's buffer keeps a discarded one too, which nothing can show.
    SaveManager::Instance->SaveData("photoI5",
                                    gMergeShipPictoHasPhoto
                                        ? EncodeBase64(gMergeShipPictoPhotoI5, MERGESHIP_PICTO_PHOTO_COMPRESSED_SIZE)
                                        : std::string());
    SaveManager::Instance->SaveArray("pictoFlags", 2, [](size_t i) {
        SaveManager::Instance->SaveData("", gMergeShipPictoFlags[i]);
    });
}

// ---- hooks ---------------------------------------------------------------------------------------------------------

void OnInterfaceUpdate() {
    if (gPlayState != nullptr) {
        MergeShip_Picto_InterfaceUpdate(gPlayState);
    }
}

void OnPlayDrawEnd() {
    if (gPlayState != nullptr) {
        MergeShip_Picto_OnPlayDrawEnd(gPlayState);
    }
}

void RegisterMergeShipPicto() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    SaveManager::Instance->AddLoadFunction("mergeShipPicto", 1, LoadPictoVersion1);
    SaveManager::Instance->AddSaveFunction("mergeShipPicto", 1, SavePicto, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(InitPictoSave);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnInterfaceUpdate>(OnInterfaceUpdate);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEnd);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        (void)sceneNum;
        MergeShip_Picto_OnSceneInit();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        (void)fileNum;
        MergeShip_Picto_Reset();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t fileNum) {
        (void)fileNum;
        MergeShip_Picto_Reset();
    });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_MM_PICTO_KEEP,
                                                                                BuildPictoKeepMessage);
}

RegisterShipInitFunc sInitFunc(RegisterMergeShipPicto, {});

} // namespace
