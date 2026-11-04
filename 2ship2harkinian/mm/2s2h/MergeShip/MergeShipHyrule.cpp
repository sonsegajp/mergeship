// MergeShip2 -- Hyrule in Termina (MergeShipHyrule.h): Ocarina of Time's songs, warp songs and quest items in
// Majora's Mask. Ocarina of Time's data (texts, fanfare sequences, sound fonts, icons, get-item models) comes from
// oot-assets.o2r, mounted under "oot/" (MergeShipAssets.h).

#include "MergeShipHyrule.h"
#include "MergeShipAssets.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#define _strdup strdup // MergeShip2 Android
#endif
#include <map>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "BenPort.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "z64ocarina.h"
#include "audio/load.h"
#include "overlays/actors/ovl_Oceff_Wipe/z_oceff_wipe.h"

extern s16 sLastPlayedSong; // z_message.c
s32 MergeShip_OoTItems_SetActiveOoTText(u16 textId); // MergeShipOoTGetItemText.cpp
s32 MergeShip_OoTItems_HookshotLevel(void);            // MergeShipOoTItems.c (BUG-0095)
extern f32 sNESFontWidths[160]; // z_message_nes.c
extern char** gSequenceMap;
extern size_t gSequenceMapSize;
extern char** gFontMap;
extern size_t gFontMapSize;
extern AudioContext gAudioCtx;
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
char** ResourceMgr_ListFiles(const char* searchMask, int* resultSize);
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
}

#ifdef MERGESHIP_HOSTED
#include "2s2h/MergeShip/MergeShipHost.h"
#endif

namespace {

// ---- Ocarina of Time's data -----------------------------------------------------------------------------------

// OoT sOcarinaSongFanfares (z_message_PAL.c) for the songs handled here, as Ocarina of Time's sequence ids.
constexpr u16 kOoTSeqOcaMinuet = 0x34;   // NA_BGM_OCA_MINUET
constexpr u16 kOoTSeqOcaBolero = 0x33;   // NA_BGM_OCA_BOLERO
constexpr u16 kOoTSeqOcaSerenade = 0x35; // NA_BGM_OCA_SERENADE
constexpr u16 kOoTSeqOcaRequiem = 0x36;  // NA_BGM_OCA_REQUIEM
constexpr u16 kOoTSeqOcaNocturne = 0x37; // NA_BGM_OCA_NOCTURNE
constexpr u16 kOoTSeqOcaLight = 0x25;    // NA_BGM_OCA_LIGHT (Prelude of Light)
constexpr u16 kOoTSeqOcaSaria = 0x44;    // NA_BGM_OCA_SARIA
constexpr u16 kOoTSeqOcaZelda = 0x46;    // NA_BGM_OCA_ZELDA

constexpr u16 kWarpSongFanfares[MERGESHIP_WARP_SONG_COUNT] = {
    kOoTSeqOcaMinuet, kOoTSeqOcaBolero, kOoTSeqOcaSerenade, kOoTSeqOcaRequiem, kOoTSeqOcaNocturne, kOoTSeqOcaLight,
};
constexpr u16 kRegisteredOoTSeqs[] = {
    0, // Ocarina of Time's sound effects (MergeShipOoTSfx.c plays them on their own sequence player)
    kOoTSeqOcaMinuet, kOoTSeqOcaBolero, kOoTSeqOcaSerenade, kOoTSeqOcaRequiem,
    kOoTSeqOcaNocturne, kOoTSeqOcaLight, kOoTSeqOcaSaria, kOoTSeqOcaZelda,
};

// OoT sWarpSongEntrances (z_player.c).
constexpr u16 kWarpSongEntrances[MERGESHIP_WARP_SONG_COUNT] = {
    MERGESHIP_OOT_ENTR_SACRED_FOREST_MEADOW_WARP_PAD, MERGESHIP_OOT_ENTR_DEATH_MOUNTAIN_CRATER_WARP_PAD,
    MERGESHIP_OOT_ENTR_LAKE_HYLIA_WARP_PAD,           MERGESHIP_OOT_ENTR_DESERT_COLOSSUS_WARP_PAD,
    MERGESHIP_OOT_ENTR_GRAVEYARD_WARP_PAD,            MERGESHIP_OOT_ENTR_TEMPLE_OF_TIME_WARP_PAD,
};

// Ocarina of Time's text ids (nes_message_data_static).
constexpr u16 kOoTTextWarpPromptFirst = 0x88D; // "Warp to the ...?" + (song - Minuet), OoT z_message_PAL.c:3801
constexpr u16 kOoTTextPlayedWarpFirst = 0x893; // "You played the Minuet of Forest." + (song - Minuet)
constexpr u16 kOoTTextPlayedSaria = 0x899;
constexpr u16 kOoTTextPlayedLullaby = 0x89B;
constexpr u16 kOoTTextPlayedSun = 0x89C;
constexpr u16 kOoTTextLearnedWarpFirst = 0x73; // "You have learned the Minuet of Forest!" + (song - Minuet)
constexpr u16 kOoTTextLearnedLullaby = 0xD4;
constexpr u16 kOoTTextStoneOfAgony = 0x68; // OoT GI_STONE_OF_AGONY
constexpr u16 kOoTTextGerudoCard = 0x7B;   // OoT GI_GERUDO_CARD
// Medallions (OoT MERGESHIP_OOT_QUEST_MEDALLION_* order) and Spiritual Stones.
constexpr u16 kOoTTextMedallions[6] = { 0x3E, 0x3C, 0x3D, 0x3F, 0x41, 0x40 };
constexpr u16 kOoTTextStones[3] = { 0x80, 0x81, 0x82 };

constexpr u16 kMMTextNothingHappened = 0x1B95; // MM "Your notes echoed far... but nothing happened."

struct OoTMessage {
    u8 textboxType = 0;
    u8 textboxYPos = 0;
    std::string text; // Ocarina of Time's encoding
    bool found = false;
};

std::map<u16, OoTMessage> sOoTMessages;
bool sOoTMessagesLoaded = false;

// Ocarina of Time's English message table, read straight from oot/text (the OTR "Text" resource: u32 count, then per
// message u16 id, u8 textbox type, u8 textbox y position, u32 length, bytes).
void LoadOoTMessages() {
    if (sOoTMessagesLoaded) {
        return;
    }
    sOoTMessagesLoaded = true;
    if (!MergeShip_IsOcarinaMounted()) {
        return;
    }
    auto file = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(
        "oot/text/nes_message_data_static/ntsc_nes_message_data_static");
    if ((file == nullptr) || (file->Buffer == nullptr) || (file->Buffer->size() < 0x44)) {
        SPDLOG_ERROR("MergeShip: Ocarina of Time's message table is missing from oot/text");
        return;
    }
    const std::vector<char>& data = *file->Buffer;
    size_t off = 0x40; // OTR resource header
    auto rd = [&](size_t n) -> u32 {
        u32 v = 0;
        for (size_t i = 0; i < n; i++) {
            v |= (u32)(u8)data[off + i] << (8 * i);
        }
        off += n;
        return v;
    };
    u32 count = rd(4);
    for (u32 i = 0; (i < count) && (off + 8 <= data.size()); i++) {
        OoTMessage msg;
        u16 id = (u16)rd(2);
        msg.textboxType = (u8)rd(1);
        msg.textboxYPos = (u8)rd(1);
        u32 len = rd(4);
        if (off + len > data.size()) {
            break;
        }
        msg.text.assign(&data[off], len);
        off += len;
        msg.found = true;
        sOoTMessages[id] = msg;
    }
    SPDLOG_INFO("MergeShip: {} Ocarina of Time messages read from oot/text", sOoTMessages.size());
}

const OoTMessage& GetOoTMessage(u16 id) {
    static const OoTMessage sMissing;
    LoadOoTMessages();
    auto it = sOoTMessages.find(id);
    return (it != sOoTMessages.end()) ? it->second : sMissing;
}

// Ocarina of Time's message encoding (OoT message_data_fmt.h) to Majora's Mask's (message_data_fmt_nes.h).
std::string ConvertOoTText(const std::string& in, bool dropTrailingLines) {
    std::string out;
    size_t n = in.size();
    for (size_t i = 0; i < n; i++) {
        u8 c = (u8)in[i];
        auto arg = [&](size_t k) -> u8 { return (i + k < n) ? (u8)in[i + k] : 0; };
        switch (c) {
            case 0x01: // NEWLINE
                out += '\x11';
                break;
            case 0x02: // END
                i = n;
                break;
            case 0x04: // BOX_BREAK
                out += '\x10';
                break;
            case 0x05: { // COLOR
                static const u8 kColors[8] = { 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x04, 0x00 };
                out += (char)kColors[arg(1) & 7];
                i += 1;
                break;
            }
            case 0x06: // SHIFT n
                out += '\x14';
                out += (char)arg(1);
                i += 1;
                break;
            case 0x07: // TEXTID xx xx
                i += 2;
                break;
            case 0x08: // QUICKTEXT_ENABLE
                out += '\x17';
                break;
            case 0x09: // QUICKTEXT_DISABLE
                out += '\x18';
                break;
            case 0x0A: // PERSISTENT
                out += '\x1A';
                break;
            case 0x0B: // EVENT
                out += '\x19';
                break;
            case 0x0C: // BOX_BREAK_DELAYED n
                out += '\x1B';
                out += (char)arg(1);
                i += 1;
                break;
            case 0x0E: // FADE n
                out += '\x1C';
                out += (char)arg(1);
                i += 1;
                break;
            case 0x0F: // NAME
                out += '\x16';
                break;
            case 0x11: // FADE2 xx xx
            case 0x12: // SFX xx xx
                i += 2;
                break;
            case 0x13: // ITEM_ICON n (Majora's Mask puts the icon in the header)
            case 0x14: // TEXT_SPEED n
                i += 1;
                break;
            case 0x15: // BACKGROUND xx yy zz
                i += 3;
                break;
            case 0x1B: // TWO_CHOICE
                out += '\xC2';
                break;
            case 0x1C: // THREE_CHOICE
                out += '\xC3';
                break;
            case 0x1E: // HIGHSCORE n
                i += 1;
                break;
            case 0x03:
            case 0x0D:
            case 0x10:
            case 0x16:
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1A:
            case 0x1D:
            case 0x1F:
                break; // no Majora's Mask counterpart (unskippable, ocarina, timers, counters)
            default:
                if ((c >= 0x9F) && (c <= 0xAB)) {
                    // Button glyphs: A B C L R Z C-Up C-Down C-Left C-Right triangle stick dpad
                    static const u8 kButtons[13] = { 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
                                                     0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBB };
                    out += (char)kButtons[c - 0x9F];
                } else {
                    out += (char)c;
                }
                break;
        }
    }
    if (dropTrailingLines) {
        // Ocarina of Time leaves blank lines under "You played ..." for its staff; Majora's Mask's box has none.
        while (!out.empty() && ((out.back() == '\x11') || (out.back() == '\x18'))) {
            out.pop_back();
        }
    }
    out += '\xBF';
    return out;
}

// A Majora's Mask custom message holding Ocarina of Time's text `ootId`.
bool SetOoTMessage(u16 ootId, bool dropTrailingLines, s32 textboxType = -1, s32 textboxYPos = -1) {
    const OoTMessage& msg = GetOoTMessage(ootId);
    if (!msg.found) {
        SPDLOG_ERROR("MergeShip: Ocarina of Time message 0x{:X} not found", ootId);
        return false;
    }
    CustomMessage::Entry entry;
    entry.textboxType = (textboxType >= 0) ? (u8)textboxType : msg.textboxType;
    entry.textboxYPos = (textboxYPos >= 0) ? (u8)textboxYPos : msg.textboxYPos;
    entry.autoFormat = false;
    std::string text = ConvertOoTText(msg.text, dropTrailingLines);
    CustomMessage::SetActiveCustomMessage(text, entry);
    MergeShipTest_Log("OOTTEXT 0x%X type=%d y=%d len=%d", ootId, entry.textboxType, entry.textboxYPos,
                      (int)text.size());
    return true;
}

// MM's own "You played ..." layout for an Ocarina of Time song (MM 0x1B72.. e.g. 0x1B79 "You played the \x01Song of
// Healing\x00!"): no quick-text or shift codes, the song name in MM's red (0x01), then MM's "!", in a type 3 box at
// y position 0. Ocarina of Time's own text (OoT 0x893..) centres the line with a shift and ends in "." with blank
// lines for its staff, which sits off-centre in MM's box. The name is Ocarina of Time's (the coloured part of its
// text); "the" stays wherever Ocarina of Time's sentence has it (Saria's Song and Zelda's Lullaby have none; OoT's
// "Played the Nocturne of Shadow" is its own width workaround and gets MM's "You played the"). Like MM's, a line
// wider than MM's widest one-line song line ("You played the Song of Soaring!") breaks before the name's last word
// (MM: "Sonata of\x11Awakening", "Inverted Song of\x11Time").
bool SetMMSongPlayedMessage(u16 ootId) {
    const OoTMessage& msg = GetOoTMessage(ootId);
    if (!msg.found) {
        SPDLOG_ERROR("MergeShip: Ocarina of Time message 0x{:X} not found", ootId);
        return false;
    }
    std::string plainBefore; // plain text before the song name
    std::string name;
    bool inName = false;
    bool nameDone = false;
    for (size_t i = 0; (i < msg.text.size()) && !nameDone; i++) {
        u8 c = (u8)msg.text[i];
        if (c == 0x05) { // COLOR x
            u8 color = (i + 1 < msg.text.size()) ? (u8)msg.text[i + 1] : 0x40;
            i++;
            if (!inName && (color != 0x40)) {
                inName = true;
            } else if (inName) {
                nameDone = true;
            }
        } else if ((c == 0x06) || (c == 0x13) || (c == 0x14) || (c == 0x0C) || (c == 0x0E) || (c == 0x1E)) {
            i++; // one-byte argument
        } else if ((c == 0x07) || (c == 0x11) || (c == 0x12)) {
            i += 2;
        } else if (c == 0x02) {
            break;
        } else if (c >= 0x20) {
            (inName ? name : plainBefore) += (char)c;
        }
    }
    if (name.empty()) {
        SPDLOG_ERROR("MergeShip: no song name in Ocarina of Time message 0x{:X}", ootId);
        return false;
    }
    const bool hasThe = plainBefore.find("the") != std::string::npos;
    std::string before = hasThe ? "You played the " : "You played ";

    auto width = [](const std::string& t) {
        f32 w = 0.0f;
        for (char ch : t) {
            u8 c = (u8)ch;
            if ((c >= 0x20) && (c < 0x20 + 160)) {
                w += sNESFontWidths[c - 0x20];
            }
        }
        return w;
    };
    std::string shownName = name;
    const size_t lastSpace = name.find_last_of(' ');
    if ((width(before + name + "!") > width("You played the Song of Soaring!")) && (lastSpace != std::string::npos)) {
        shownName = name.substr(0, lastSpace) + '\x11' + name.substr(lastSpace + 1);
    }

    CustomMessage::Entry entry;
    entry.textboxType = 3;
    entry.textboxYPos = 0;
    entry.autoFormat = false;
    std::string text = before + '\x01' + shownName + '\x00' + "!" + '\xBF';
    CustomMessage::SetActiveCustomMessage(text, entry);
    MergeShipTest_Log("OOTTEXT played 0x%X as MM layout: \"%s%s!\" len=%d", ootId, before.c_str(), name.c_str(),
                      (int)text.size());
    return true;
}

// ---- The warp prompt ------------------------------------------------------------------------------------------

u8 sPromptSong = 0xFF;

void RegisterHooks() {
    const bool enabled = MergeShip_Hyrule_Enabled();

    // BUG-0095: Tingle floats on his balloon whenever Link has something to pop it with -- magic (the Deku bubble),
    // either game's bow (both in MM's bow slot), a hookshot of either game, the Zora's fins -- not only with Majora's
    // Mask's magic: Hyrule may send Link here before any (MM grounds him without it, so he can still be talked to).
    COND_VB_SHOULD(VB_HAVE_MAGIC_FOR_TINGLE, enabled, {
        if (!*should) {
            *should = (INV_CONTENT(ITEM_BOW) == ITEM_BOW) || (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT) ||
                      (INV_CONTENT(ITEM_MASK_ZORA) == ITEM_MASK_ZORA) || (MergeShip_OoTItems_HookshotLevel() > 0);
        }
    });

    // Zelda's Lullaby: MM's ocarina knows its notes (song 21) but MM has no quest bit for it.
    COND_VB_SHOULD(VB_SONG_AVAILABLE_TO_PLAY, enabled, {
        uint8_t* songIndex = va_arg(args, uint8_t*);
        if ((*songIndex == OCARINA_SONG_ZELDAS_LULLABY) && MergeShip_Hyrule_IsSongPlayable(*songIndex)) {
            *should = true;
        }
    });

    // Message_Update MSGMODE_TEXT_CLOSING after a warp song in free play: Ocarina of Time's "Warp to ...?" (OoT
    // z_message_PAL.c:3801, unless the scene restricts warping, OoT interfaceCtx->restrictions.warpSongs; MM's own
    // counterpart is the Song of Soaring's restriction).
    COND_VB_SHOULD(VB_MSG_CAPTURE_MSGMODE_TEXT_CLOSING_OCARINA_ACTION, enabled, {
        PlayState* play = gPlayState;
        MessageContext* msgCtx = &play->msgCtx;

        if (!MERGESHIP_SONG_IS_OOT_WARP(sLastPlayedSong) || (msgCtx->ocarinaAction != OCARINA_ACTION_FREE_PLAY_DONE)) {
            return;
        }
        *should = true;
        sPromptSong = (u8)sLastPlayedSong;
        if (play->interfaceCtx.restrictions.songOfSoaring != 0) {
            MergeShipTest_Log("HYRULEWARP song=%d restricted in scene %d", sPromptSong, play->sceneId);
            Message_StartTextbox(play, kMMTextNothingHappened, NULL);
            play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
            return;
        }
        // MM's own ocarina prompts (0x1B93 "Soar to ...?") use box type 6 at y position 1; so does this one.
        if (SetOoTMessage(kOoTTextWarpPromptFirst + (sPromptSong - MERGESHIP_SONG_MINUET), false, 6, 1)) {
            Message_StartTextbox(play, CUSTOM_MESSAGE_ID, NULL);
            play->msgCtx.ocarinaMode = MERGESHIP_OCARINA_MODE_PROCESS_HYRULE_WARP;
            MergeShipTest_Log("HYRULEWARP prompt song=%d text=0x%X", sPromptSong,
                              kOoTTextWarpPromptFirst + (sPromptSong - MERGESHIP_SONG_MINUET));
        } else {
            Message_StartTextbox(play, kMMTextNothingHappened, NULL);
            play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
        }
    });

    // Message_Update MSGMODE_TEXT_DONE: the answer to "Warp to ...?" (OoT: OK -> OCARINA_MODE_02, No -> 04).
    COND_VB_SHOULD(VB_MSG_CAPTURE_MSGMODE_TEXT_DONE, enabled, {
        PlayState* play = gPlayState;
        MessageContext* msgCtx = &play->msgCtx;

        if ((msgCtx->textboxEndType != TEXTBOX_ENDTYPE_TWO_CHOICE) ||
            (play->msgCtx.ocarinaMode != MERGESHIP_OCARINA_MODE_PROCESS_HYRULE_WARP)) {
            return;
        }
        *should = true;
        if (!Message_ShouldAdvance(play)) {
            return;
        }
        if (msgCtx->choiceIndex == 0) {
            Audio_PlaySfx_MessageDecide();
            MergeShipTest_Log("HYRULEWARP ok song=%d", sPromptSong);
            MergeShip_RequestHyruleWarp(MergeShip_Hyrule_WarpSongEntrance(sPromptSong));
        } else {
            Audio_PlaySfx_MessageCancel();
            MergeShipTest_Log("HYRULEWARP no song=%d", sPromptSong);
            play->msgCtx.ocarinaMode = OCARINA_MODE_END;
            Message_CloseTextbox(play);
        }
    });
}

static RegisterShipInitFunc sInitFunc(RegisterHooks, { MERGESHIP_OOT_SONGS_CVAR });

// ---- Audio -----------------------------------------------------------------------------------------------------

std::map<u16, u16> sOoTSeqToMM; // Ocarina of Time's sequence id -> MM sequence id
std::map<u16, u16> sOoTFontToMM; // Ocarina of Time's sound font index -> MM font id
// Fonts registered on their own (no sequence of theirs is): font 30, Ganondorf's Theme's -- its organ is Ganondorf's
// ocarina (Ganondorf mode: seqplayer.c)
constexpr u16 kRegisteredOoTFonts[] = { 30 };

// The Ocarina of Time font resource with index `ootFont`: oot/audio/fonts/<index>_<name> (the exporter's naming).
std::string FindOoTFont(u16 ootFont, char** fontList, int fontCount) {
    for (int i = 0; i < fontCount; i++) {
        const char* base = strrchr(fontList[i], '/');
        base = (base != nullptr) ? base + 1 : fontList[i];
        if ((base[0] >= '0') && (base[0] <= '9') && (atoi(base) == ootFont)) {
            return fontList[i];
        }
    }
    return "";
}

} // namespace

// ==== State =======================================================================================================

extern "C" s32 MergeShip_Hyrule_Enabled(void) {
    return CVarGetInteger(MERGESHIP_OOT_SONGS_CVAR, 1) != 0;
}

extern "C" u32 MergeShip_Hyrule_GetQuestItems(void) {
    return gSaveContext.save.shipSaveInfo.mergeShipOoTQuest.questItems;
}

extern "C" s32 MergeShip_Hyrule_HasQuestItem(u8 ootQuest) {
    if (ootQuest >= MERGESHIP_OOT_QUEST_MAX) {
        return false;
    }
    switch (ootQuest) {
        // Songs Termina has too are Majora's Mask's own.
        case MERGESHIP_OOT_QUEST_SONG_EPONA:
            return CHECK_QUEST_ITEM(QUEST_SONG_EPONA);
        case MERGESHIP_OOT_QUEST_SONG_SARIA:
            return CHECK_QUEST_ITEM(QUEST_SONG_SARIA);
        case MERGESHIP_OOT_QUEST_SONG_SUN:
            return CHECK_QUEST_ITEM(QUEST_SONG_SUN);
        case MERGESHIP_OOT_QUEST_SONG_TIME:
            return CHECK_QUEST_ITEM(QUEST_SONG_TIME);
        case MERGESHIP_OOT_QUEST_SONG_STORMS:
            return CHECK_QUEST_ITEM(QUEST_SONG_STORMS);
        default:
            return (MergeShip_Hyrule_GetQuestItems() >> ootQuest) & 1;
    }
}

extern "C" void MergeShip_Hyrule_SetQuestItem(u8 ootQuest) {
    switch (ootQuest) {
        case MERGESHIP_OOT_QUEST_SONG_EPONA:
            SET_QUEST_ITEM(QUEST_SONG_EPONA);
            break;
        case MERGESHIP_OOT_QUEST_SONG_SARIA:
            SET_QUEST_ITEM(QUEST_SONG_SARIA);
            break;
        case MERGESHIP_OOT_QUEST_SONG_SUN:
            SET_QUEST_ITEM(QUEST_SONG_SUN);
            break;
        case MERGESHIP_OOT_QUEST_SONG_TIME:
            SET_QUEST_ITEM(QUEST_SONG_TIME);
            break;
        case MERGESHIP_OOT_QUEST_SONG_STORMS:
            SET_QUEST_ITEM(QUEST_SONG_STORMS);
            break;
        default:
            if (ootQuest < MERGESHIP_OOT_QUEST_MAX) {
                gSaveContext.save.shipSaveInfo.mergeShipOoTQuest.questItems |= (1u << ootQuest);
            }
            break;
    }
}

extern "C" s32 MergeShip_Hyrule_IsSongLearned(u8 song) {
    if (song == OCARINA_SONG_ZELDAS_LULLABY) {
        return MergeShip_Hyrule_HasQuestItem(MERGESHIP_OOT_QUEST_SONG_LULLABY);
    }
    if (MERGESHIP_SONG_IS_OOT_WARP(song)) {
        return MergeShip_Hyrule_HasQuestItem(MERGESHIP_OOT_QUEST_SONG_MINUET + (song - MERGESHIP_SONG_MINUET));
    }
    if (song == OCARINA_SONG_SARIAS) {
        return CHECK_QUEST_ITEM(QUEST_SONG_SARIA);
    }
    if (song == OCARINA_SONG_SUNS) {
        return CHECK_QUEST_ITEM(QUEST_SONG_SUN);
    }
    return false;
}

extern "C" s32 MergeShip_Hyrule_IsSongPlayable(u8 song) {
    if (!MergeShip_Hyrule_Enabled()) {
        return false;
    }
    if ((song != OCARINA_SONG_ZELDAS_LULLABY) && !MERGESHIP_SONG_IS_OOT_WARP(song)) {
        return false;
    }
    return MergeShip_Hyrule_IsSongLearned(song);
}

extern "C" u16 MergeShip_Hyrule_WarpSongEntrance(u8 song) {
    return MERGESHIP_SONG_IS_OOT_WARP(song) ? kWarpSongEntrances[song - MERGESHIP_SONG_MINUET] : 0xFFFF;
}

// ==== Items =======================================================================================================

extern "C" s32 MergeShip_Hyrule_IsHyruleItem(u8 item) {
    return (item == ITEM_OOT_STONE_OF_AGONY) || (item == ITEM_OOT_GERUDO_CARD) ||
           ((item >= ITEM_OOT_SONG_LULLABY) && (item <= ITEM_OOT_SONG_PRELUDE));
}

extern "C" s32 MergeShip_Hyrule_GiveItem(u8 item) {
    if (!MergeShip_Hyrule_IsHyruleItem(item)) {
        return false;
    }
    // Ocarina of Time Item_Give: SET_QUEST_ITEM for its songs and its two quest items.
    if (item == ITEM_OOT_STONE_OF_AGONY) {
        MergeShip_Hyrule_SetQuestItem(MERGESHIP_OOT_QUEST_STONE_OF_AGONY);
    } else if (item == ITEM_OOT_GERUDO_CARD) {
        MergeShip_Hyrule_SetQuestItem(MERGESHIP_OOT_QUEST_GERUDO_CARD);
    } else if (item == ITEM_OOT_SONG_LULLABY) {
        MergeShip_Hyrule_SetQuestItem(MERGESHIP_OOT_QUEST_SONG_LULLABY);
    } else {
        MergeShip_Hyrule_SetQuestItem(MERGESHIP_OOT_QUEST_SONG_MINUET + (item - ITEM_OOT_SONG_MINUET));
    }
    MergeShipTest_Log("GIVE hyrule item=0x%02X quest=0x%06X", item, MergeShip_Hyrule_GetQuestItems());
    return true;
}

extern "C" s32 MergeShip_Hyrule_GiveQuestItem(u8 ootQuest) {
    if ((ootQuest > MERGESHIP_OOT_QUEST_MEDALLION_LIGHT) &&
        ((ootQuest < MERGESHIP_OOT_QUEST_KOKIRI_EMERALD) || (ootQuest > MERGESHIP_OOT_QUEST_ZORA_SAPPHIRE))) {
        return false;
    }
    MergeShip_Hyrule_SetQuestItem(ootQuest);
    MergeShipTest_Log("GIVE hyrule quest=%d bits=0x%06X", ootQuest, MergeShip_Hyrule_GetQuestItems());
    return true;
}

namespace {

// Ocarina of Time's get-item text for an ITEM_OOT_* id or 0x100 + quest bit.
u16 GetItemTextId(u16 item) {
    if (item >= 0x100) {
        u8 q = item - 0x100;
        if (q <= MERGESHIP_OOT_QUEST_MEDALLION_LIGHT) {
            return kOoTTextMedallions[q];
        }
        return kOoTTextStones[q - MERGESHIP_OOT_QUEST_KOKIRI_EMERALD];
    }
    if (item == ITEM_OOT_STONE_OF_AGONY) {
        return kOoTTextStoneOfAgony;
    }
    if (item == ITEM_OOT_GERUDO_CARD) {
        return kOoTTextGerudoCard;
    }
    if (item == ITEM_OOT_SONG_LULLABY) {
        return kOoTTextLearnedLullaby;
    }
    return kOoTTextLearnedWarpFirst + (item - ITEM_OOT_SONG_MINUET);
}

// Ocarina of Time's get-item models (OoT z_draw.c sDrawItemTable), drawn from oot/objects.
const char* const kMedallionFaces[6] = {
    OOT_ASSET("objects/object_gi_medal/gGiForestMedallionFaceDL"),
    OOT_ASSET("objects/object_gi_medal/gGiFireMedallionFaceDL"),
    OOT_ASSET("objects/object_gi_medal/gGiWaterMedallionFaceDL"),
    OOT_ASSET("objects/object_gi_medal/gGiSpiritMedallionFaceDL"),
    OOT_ASSET("objects/object_gi_medal/gGiShadowMedallionFaceDL"),
    OOT_ASSET("objects/object_gi_medal/gGiLightMedallionFaceDL"),
};
const char* const kWarpSongColors[6] = {
    OOT_ASSET("objects/object_gi_melody/gGiMinuetColorDL"),   OOT_ASSET("objects/object_gi_melody/gGiBoleroColorDL"),
    OOT_ASSET("objects/object_gi_melody/gGiSerenadeColorDL"), OOT_ASSET("objects/object_gi_melody/gGiRequiemColorDL"),
    OOT_ASSET("objects/object_gi_melody/gGiNocturneColorDL"), OOT_ASSET("objects/object_gi_melody/gGiPreludeColorDL"),
};
struct JewelColors {
    u8 primXlu[3], envXlu[3], primOpa[3], envOpa[3];
    const char* gem;
    const char* setting;
};
// OoT GetItem_DrawJewelKokiri / Goron / Zora.
const JewelColors kJewels[3] = {
    { { 255, 255, 160 }, { 0, 255, 0 }, { 255, 255, 170 }, { 150, 120, 0 },
      OOT_ASSET("objects/object_gi_jewel/gGiKokiriEmeraldGemDL"),
      OOT_ASSET("objects/object_gi_jewel/gGiKokiriEmeraldSettingDL") },
    { { 255, 170, 255 }, { 255, 0, 100 }, { 255, 255, 170 }, { 150, 120, 0 },
      OOT_ASSET("objects/object_gi_jewel/gGiGoronRubyGemDL"), OOT_ASSET("objects/object_gi_jewel/gGiGoronRubySettingDL") },
    { { 50, 255, 255 }, { 50, 0, 150 }, { 255, 255, 170 }, { 150, 120, 0 },
      OOT_ASSET("objects/object_gi_jewel/gGiZoraSapphireGemDL"),
      OOT_ASSET("objects/object_gi_jewel/gGiZoraSapphireSettingDL") },
};

// OPEN_DISPS / CLOSE_DISPS declare these at block scope; inside this namespace they need their C linkage declared.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

void DrawGetItemModel(PlayState* play, u16 item) {
    OPEN_DISPS(play->state.gfxCtx);

    if ((item >= 0x100) && ((item - 0x100) <= MERGESHIP_OOT_QUEST_MEDALLION_LIGHT)) {
        // OoT GetItem_DrawEggOrMedallion
        Gfx_SetupDL26_Opa(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)kMedallionFaces[item - 0x100]);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)OOT_ASSET("objects/object_gi_medal/gGiMedallionDL"));
    } else if (item >= 0x100) {
        // OoT GetItem_DrawJewel
        const JewelColors& j = kJewels[(item - 0x100) - MERGESHIP_OOT_QUEST_KOKIRI_EMERALD];

        gSPSegment(POLY_XLU_DISP++, 9,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 255, 64, 64, 1, 0, 255, 16, 16, 0, 0, 0,
                                                 0));
        gSPSegment(POLY_OPA_DISP++, 8, (uintptr_t)Gfx_TexScrollEx(play->state.gfxCtx, 0, 0, 16, 16, 0, 0));
        Matrix_Push();
        Matrix_RotateZYX(0, -0x4000, 0x4000, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 128, j.primXlu[0], j.primXlu[1], j.primXlu[2], 255);
        gDPSetEnvColor(POLY_XLU_DISP++, j.envXlu[0], j.envXlu[1], j.envXlu[2], 255);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)j.gem);
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 128, j.primOpa[0], j.primOpa[1], j.primOpa[2], 255);
        gDPSetEnvColor(POLY_OPA_DISP++, j.envOpa[0], j.envOpa[1], j.envOpa[2], 255);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)j.setting);
        Matrix_Pop();
    } else if ((item == ITEM_OOT_STONE_OF_AGONY) || (item == ITEM_OOT_GERUDO_CARD)) {
        // OoT GetItem_DrawOpa0
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (item == ITEM_OOT_STONE_OF_AGONY)
                                            ? (Gfx*)OOT_ASSET("objects/object_gi_map/gGiStoneOfAgonyDL")
                                            : (Gfx*)OOT_ASSET("objects/object_gi_gerudo/gGiGerudoCardDL"));
    } else if (item == ITEM_OOT_SONG_LULLABY) {
        // SoH GetItem_DrawGenericMusicNote, Zelda's Lullaby's colour
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 109, 73, 143, 255);
        gSPGrayscale(POLY_XLU_DISP++, true);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)OOT_ASSET("objects/object_gi_melody/gGiSongNoteDL"));
        gSPGrayscale(POLY_XLU_DISP++, false);
    } else {
        // OoT GetItem_DrawXlu01 (warp song notes)
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)kWarpSongColors[item - ITEM_OOT_SONG_MINUET]);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)OOT_ASSET("objects/object_gi_melody/gGiSongNoteDL"));
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawGetItem(Actor* actor, PlayState* play) {
    // Ocarina of Time's get-item models at the size its GetItem_Draw gives them (as MergeShip_OoTItems_GiveWithCutscene).
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    DrawGetItemModel(play, (u16)CUSTOM_ITEM_PARAM);
}

void GiveGetItem(Actor* actor, PlayState* play) {
    u16 item = (u16)CUSTOM_ITEM_PARAM;
    u16 textId = GetItemTextId(item);

    if (item >= 0x100) {
        MergeShip_Hyrule_GiveQuestItem((u8)(item - 0x100));
    } else {
        Item_Give(play, (u8)item);
    }
    // MergeShip2: the OoT-items text converter (MergeShipOoTGetItemText.cpp), whose get-item boxes display; this
    // file's converter left the box empty for these messages.
    if (MergeShip_OoTItems_SetActiveOoTText(textId) || SetOoTMessage(textId, false)) {
        if (!(CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE)) {
            Message_StartTextbox(play, CUSTOM_MESSAGE_ID, NULL);
        }
    }
}

} // namespace

extern "C" void MergeShip_Hyrule_DrawGetItemModel(PlayState* play, u16 item) {
    if ((play == nullptr) || !MergeShip_IsOcarinaMounted()) {
        return;
    }
    if ((item >= 0x100) ? ((item - 0x100) > MERGESHIP_OOT_QUEST_ZORA_SAPPHIRE ||
                           ((item - 0x100) > MERGESHIP_OOT_QUEST_MEDALLION_LIGHT &&
                            (item - 0x100) < MERGESHIP_OOT_QUEST_KOKIRI_EMERALD))
                        : !MergeShip_Hyrule_IsHyruleItem((u8)item)) {
        return;
    }
    DrawGetItemModel(play, item);
}

extern "C" void MergeShip_Hyrule_QueueGetItem(u16 item) {
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = true,
        .param = (s16)item,
        .giveItem = GiveGetItem,
        .drawItem = DrawGetItem,
    });
    MergeShipTest_Log("GETITEM queued 0x%X text=0x%X", item, GetItemTextId(item));
}

// ==== The song's steps ============================================================================================

extern "C" s32 MergeShip_Hyrule_SpawnSongEffect(PlayState* play, u8 song) {
    if (!MergeShip_Hyrule_Enabled()) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    if (MERGESHIP_SONG_IS_OOT_WARP(song)) {
        return true; // Ocarina of Time spawns no effect actor for its warp songs
    }
    if ((song == OCARINA_SONG_ZELDAS_LULLABY) || (song == OCARINA_SONG_SARIAS)) {
        // OoT sOcarinaEffectActorIds: Zelda's Lullaby Oceff_Wipe (OCEFF_WIPE_ZL), Saria's Song Oceff_Wipe3. MM's own
        // table has no lullaby entry and gives Saria's Song the Sonata's Oceff_Wipe5.
        play->msgCtx.ocarinaSongEffectActive = true;
        Actor_Spawn(&play->actorCtx, play, (song == OCARINA_SONG_SARIAS) ? ACTOR_OCEFF_WIPE3 : ACTOR_OCEFF_WIPE,
                    player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                    OCEFF_WIPE_ZL);
        MergeShipTest_Log("SONGEFFECT song=%d actor=0x%X", song,
                          (song == OCARINA_SONG_SARIAS) ? ACTOR_OCEFF_WIPE3 : ACTOR_OCEFF_WIPE);
        return true;
    }
    return false;
}

extern "C" s32 MergeShip_Hyrule_PlaySongFanfare(u8 song, u8 formIoData) {
    if (!MergeShip_Hyrule_Enabled()) {
        return false;
    }
    u16 ootSeq;
    if (MERGESHIP_SONG_IS_OOT_WARP(song)) {
        ootSeq = kWarpSongFanfares[song - MERGESHIP_SONG_MINUET];
    } else if (song == OCARINA_SONG_ZELDAS_LULLABY) {
        ootSeq = kOoTSeqOcaZelda;
    } else if ((song == OCARINA_SONG_SARIAS) && !(IS_RANDO && RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA])) {
        // MM's table gives Saria's Song NA_BGM_MAJORAS_LAIR; the randomizer's Saria hints set their own fanfare.
        ootSeq = kOoTSeqOcaSaria;
    } else {
        return false;
    }
    u16 seqId = MergeShip_Audio_OoTSeqId(ootSeq);
    if (seqId != 0xFFFF) {
        // MM Audio_PlayFanfareWithPlayerIOPort7: the melody itself is the ocarina playback on the form's instrument
        // (sPlayerFormOcarinaInstruments); the fanfare accompanies it, as in both games.
        Audio_PlayFanfareWithPlayerIOPort7(seqId, formIoData);
    }
    MergeShipTest_Log("FANFARE song=%d ootSeq=0x%02X mmSeq=%d io=0x%02X", song, ootSeq, seqId, formIoData);
    return true;
}

extern "C" s32 MergeShip_Hyrule_ContinueSongPlayedText(PlayState* play, u8 song) {
    if (!MergeShip_Hyrule_Enabled()) {
        return false;
    }
    u16 ootText;
    if (MERGESHIP_SONG_IS_OOT_WARP(song)) {
        ootText = kOoTTextPlayedWarpFirst + (song - MERGESHIP_SONG_MINUET);
    } else if (song == OCARINA_SONG_ZELDAS_LULLABY) {
        ootText = kOoTTextPlayedLullaby;
    } else if (song == OCARINA_SONG_SARIAS) {
        ootText = kOoTTextPlayedSaria; // MM 0x1B77 is empty
    } else if (song == OCARINA_SONG_SUNS) {
        ootText = kOoTTextPlayedSun; // MM 0x1B7D is empty
    } else {
        return false;
    }
    // MM's own "You played ..." layout with Ocarina of Time's song name (SetMMSongPlayedMessage).
    if (!SetMMSongPlayedMessage(ootText)) {
        return false;
    }
    Message_ContinueTextbox(play, CUSTOM_MESSAGE_ID);
    return true;
}

// ==== The warp ====================================================================================================

extern "C" s32 MergeShip_RequestHyruleWarp(u16 ootEntrance) {
    PlayState* play = gPlayState;

#ifdef MERGESHIP_HOSTED
    // The door's trip back (MergeShipHost.cpp): file slot, shared save and Link's body go with the player.
    if (MergeShipHost_RequestHyruleWarp(ootEntrance)) {
        SPDLOG_INFO("MergeShip: warp song -> requested transfer to Ocarina of Time, entrance 0x{:X}", ootEntrance);
        MergeShipTest_Log("HYRULEWARP transfer requested entrance=0x%X", ootEntrance);
        if (play != nullptr) {
            play->msgCtx.ocarinaMode = OCARINA_MODE_END;
            Message_CloseTextbox(play);
        }
        return true;
    }
    SPDLOG_WARN("MergeShip: the host offers no RequestTransfer; warp to 0x{:X} not possible", ootEntrance);
#endif

    // No Hyrule to warp to: MM's answer where a song cannot take Link (MM 0x1B95,
    // OCARINA_MODE_PROCESS_RESTRICTED_SONG; MM's TEXT_DONE closes it and ends the ocarina).
    SPDLOG_INFO("MergeShip: warp song to Ocarina of Time entrance 0x{:X} requested; not hosted, nothing happens",
                ootEntrance);
    MergeShipTest_Log("HYRULEWARP standalone entrance=0x%X -> nothing happened", ootEntrance);
    if (play != nullptr) {
        if (play->msgCtx.msgMode != MSGMODE_NONE) {
            Message_ContinueTextbox(play, kMMTextNothingHappened);
        } else {
            Message_StartTextbox(play, kMMTextNothingHappened, NULL);
        }
        play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
    }
    return false;
}

// ==== Audio =======================================================================================================

extern "C" u16 MergeShip_Audio_OoTFontId(u16 ootFont) {
    auto it = sOoTFontToMM.find(ootFont);
    return (it != sOoTFontToMM.end()) ? it->second : 0xFFFF;
}

extern "C" u16 MergeShip_Audio_OoTSeqId(u16 ootSeqId) {
    auto it = sOoTSeqToMM.find(ootSeqId);
    return (it != sOoTSeqToMM.end()) ? it->second : 0xFFFF;
}

extern "C" void MergeShip_Audio_RegisterOoTSequences(void) {
    if (!sOoTSeqToMM.empty() || !MergeShip_IsOcarinaMounted()) {
        if (!MergeShip_IsOcarinaMounted()) {
            SPDLOG_WARN("MergeShip: oot-assets.o2r not mounted; Ocarina of Time's song fanfares are unavailable");
        }
        return;
    }

    int seqCount = 0;
    int fontCount = 0;
    char** seqList = ResourceMgr_ListFiles("oot/audio/sequences*", &seqCount);
    char** fontList = ResourceMgr_ListFiles("oot/audio/fonts*", &fontCount);
    std::map<u16, std::string> ootSeqNames;
    for (int i = 0; i < seqCount; i++) {
        const char* base = strrchr(seqList[i], '/');
        base = (base != nullptr) ? base + 1 : seqList[i];
        if ((base[0] >= '0') && (base[0] <= '9')) {
            ootSeqNames[(u16)atoi(base)] = seqList[i];
        }
    }

    std::map<u16, u16> fontIds; // Ocarina of Time font index -> MM font id
    std::vector<std::string> newFonts;
    std::vector<std::string> newSeqs;
    std::vector<u16> newSeqOoT;

    for (u16 ootSeq : kRegisteredOoTSeqs) {
        auto name = ootSeqNames.find(ootSeq);
        if (name == ootSeqNames.end()) {
            SPDLOG_ERROR("MergeShip: Ocarina of Time sequence {} is missing from oot/audio/sequences", ootSeq);
            continue;
        }
        SequenceData* seq = ResourceMgr_LoadSeqPtrByName(name->second.c_str());
        if ((seq == nullptr) || (seq->seqData == nullptr) || (seq->numFonts <= 0)) {
            SPDLOG_ERROR("MergeShip: Ocarina of Time sequence {} did not load", name->second);
            continue;
        }
        bool ok = true;
        u16 mapped[16];
        for (s32 f = 0; (f < seq->numFonts) && (f < 16); f++) {
            u16 ootFont = seq->fonts[f];
            auto known = fontIds.find(ootFont);
            if (known != fontIds.end()) {
                mapped[f] = known->second;
                continue;
            }
            std::string fontPath = FindOoTFont(ootFont, fontList, fontCount);
            SoundFont* sf = fontPath.empty() ? nullptr : ResourceMgr_LoadAudioSoundFontByName(fontPath.c_str());
            if (sf == nullptr) {
                SPDLOG_ERROR("MergeShip: Ocarina of Time sound font {} (for {}) did not load", ootFont, name->second);
                ok = false;
                break;
            }
            u16 id = (u16)(gFontMapSize + newFonts.size());
            // Samples: count the instruments whose samples resolved (oot/audio/samples, Ocarina of Time's format).
            int instruments = 0;
            int withSamples = 0;
            for (int k = 0; k < sf->numInstruments; k++) {
                Instrument* inst = (sf->instruments != nullptr) ? sf->instruments[k] : nullptr;
                if (inst == nullptr) {
                    continue;
                }
                instruments++;
                if (inst->normalPitchTunedSample.sample != nullptr) {
                    withSamples++;
                }
            }
            sf->fntIndex = id;
            fontIds[ootFont] = id;
            newFonts.push_back(fontPath);
            mapped[f] = id;
            SPDLOG_INFO("MergeShip: Ocarina of Time font {} ({}) as MM font {}: {} of {} instruments have samples",
                        ootFont, fontPath, id, withSamples, instruments);
            MergeShipTest_Log("OOTFONT %d %s -> %d instruments=%d samples=%d", ootFont, fontPath.c_str(), id,
                              instruments, withSamples);
        }
        if (!ok) {
            continue;
        }
        for (s32 f = 0; (f < seq->numFonts) && (f < 16); f++) {
            seq->fonts[f] = mapped[f];
        }
        u16 id = (u16)(gSequenceMapSize + newSeqs.size());
        seq->seqNumber = id;
        newSeqs.push_back(name->second);
        newSeqOoT.push_back(ootSeq);
    }

    for (u16 ootFont : kRegisteredOoTFonts) {
        if (fontIds.find(ootFont) != fontIds.end()) {
            continue;
        }
        std::string fontPath = FindOoTFont(ootFont, fontList, fontCount);
        SoundFont* sf = fontPath.empty() ? nullptr : ResourceMgr_LoadAudioSoundFontByName(fontPath.c_str());
        if (sf == nullptr) {
            SPDLOG_ERROR("MergeShip: Ocarina of Time sound font {} did not load", ootFont);
            continue;
        }
        u16 id = (u16)(gFontMapSize + newFonts.size());
        sf->fntIndex = id;
        fontIds[ootFont] = id;
        newFonts.push_back(fontPath);
        MergeShipTest_Log("OOTFONT %d %s -> %d (by itself)", ootFont, fontPath.c_str(), id);
    }

    // Grow MM's tables (load.c): the sequence and font name maps and their load states.
    if (!newSeqs.empty()) {
        size_t base = gSequenceMapSize;
        char** map = (char**)realloc(gSequenceMap, (base + newSeqs.size()) * sizeof(char*));
        u8* status = (u8*)realloc(gAudioCtx.seqLoadStatus, base + newSeqs.size());
        if ((map != nullptr) && (status != nullptr)) {
            gSequenceMap = map;
            gAudioCtx.seqLoadStatus = status;
            for (size_t i = 0; i < newSeqs.size(); i++) {
                gSequenceMap[base + i] = _strdup(newSeqs[i].c_str());
                // As load.c does for custom sequences: past MM's ids, AudioLoad_SetSeqLoadStatus writes the status
                // of AudioEditor_GetOriginalSeq(id) (0 for an id no audio-editor replacement names), so this id's
                // own status would never read complete and the sequence player would disable itself on its first
                // update. Marked permanent, AudioLoad_IsSeqLoadComplete holds; the data is still (re)loaded through
                // the caches by AudioLoad_SyncLoad.
                gAudioCtx.seqLoadStatus[base + i] = LOAD_STATUS_PERMANENT;
                sOoTSeqToMM[newSeqOoT[i]] = (u16)(base + i);
            }
            gSequenceMapSize = base + newSeqs.size();
        } else {
            if (map != nullptr) {
                gSequenceMap = map;
            }
            if (status != nullptr) {
                gAudioCtx.seqLoadStatus = status;
            }
            SPDLOG_ERROR("MergeShip: out of memory registering Ocarina of Time's sequences");
            sOoTSeqToMM.clear();
        }
    }
    if (!newFonts.empty()) {
        size_t base = gFontMapSize;
        char** map = (char**)realloc(gFontMap, (base + newFonts.size()) * sizeof(char*));
        u8* status = (u8*)realloc(gAudioCtx.fontLoadStatus, base + newFonts.size());
        if ((map != nullptr) && (status != nullptr)) {
            gFontMap = map;
            gAudioCtx.fontLoadStatus = status;
            for (size_t i = 0; i < newFonts.size(); i++) {
                gFontMap[base + i] = _strdup(newFonts[i].c_str());
                gAudioCtx.fontLoadStatus[base + i] = 0;
            }
            for (auto& f : fontIds) {
                sOoTFontToMM[f.first] = f.second;
            }
            gFontMapSize = base + newFonts.size();
        } else {
            if (map != nullptr) {
                gFontMap = map;
            }
            if (status != nullptr) {
                gAudioCtx.fontLoadStatus = status;
            }
            SPDLOG_ERROR("MergeShip: out of memory registering Ocarina of Time's sound fonts");
            sOoTSeqToMM.clear();
        }
    }

    for (int i = 0; i < seqCount; i++) {
        free(seqList[i]);
    }
    free(seqList);
    for (int i = 0; i < fontCount; i++) {
        free(fontList[i]);
    }
    free(fontList);

    SPDLOG_INFO("MergeShip: {} Ocarina of Time song fanfares and {} sound fonts registered in MM's audio tables",
                sOoTSeqToMM.size(), newFonts.size());
    MergeShipTest_Log("OOTSEQS registered=%d fonts=%d seqMapSize=%d fontMapSize=%d", (int)sOoTSeqToMM.size(),
                      (int)newFonts.size(), (int)gSequenceMapSize, (int)gFontMapSize);
}

// ---- Arrival (MergeShipAdultLink.h) ------------------------------------------------------------------------------

extern "C" void MergeShip_AdultLink_SetArrivalAge(u8 ootAge); // MergeShipAdultLink.c

extern "C" void MergeShip_Hyrule_SetArrivalAge(u8 ootAge) {
    MergeShip_AdultLink_SetArrivalAge(ootAge);
}
