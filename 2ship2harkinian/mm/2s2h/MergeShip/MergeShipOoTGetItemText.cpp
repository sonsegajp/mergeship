// MergeShip2 -- Ocarina of Time's English get-item texts in Majora's Mask (2Ship), and the give-with-cutscene event.
//
// The text is Ocarina of Time's own, read at run time from the OoT archive mounted under "oot/" (MergeShipAssets.h):
// SoH exports the message table as a "Text" resource (SoH TextFactory.cpp / type/Text.h). 2Ship's own "Text" factory
// reads MM's format, so the raw file is parsed here instead of going through the resource manager.
//
// OoT's control codes (OoT include/message_data_fmt.h) are converted to MM's (MM include/message_data_fmt_nes.h, as
// MM's z_message_nes.c decodes them). OoT's textbox type, position and "unskippable" code become MM's message header
// (built by CustomMessage::LoadCustomMessageIntoFont); OoT's item icon code becomes the header's icon.

#include "MergeShipOoTItems.h"
#include "MergeShipAssets.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/GameInteractor/GameInteractor.h"

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <string>
#include <unordered_map>

extern "C" {
#include "z64.h"
}

namespace {

struct OoTMessage {
    u8 textboxType; // OoT TextBoxType
    u8 textboxPos;  // OoT TextBoxPosition
    std::string msg;
};

// SoH exports the English table as nes_message_data_static (PAL) or ntsc_nes_message_data_static (NTSC); SoH's
// OTRMessage_Init prefers the PAL one.
const char* const sOoTMessageTablePaths[] = {
    "oot/text/nes_message_data_static/nes_message_data_static",
    "oot/text/nes_message_data_static/ntsc_nes_message_data_static",
};

// LUS resource header: endianness (u8 @0x00), type (u32 @0x04), version (u32 @0x08), id (u64 @0x0C); payload @0x40.
constexpr size_t kResourceHeaderSize = 0x40;
constexpr uint32_t kResourceTypeText = 0x4F545854; // 'OTXT', SoH ResourceType::SOH_Text

std::unordered_map<u16, OoTMessage> sOoTMessages;
bool sOoTMessagesLoaded = false;

struct Reader {
    const std::vector<char>& buf;
    size_t pos;
    bool bigEndian;
    bool ok = true;

    uint32_t Read(size_t size) {
        if (pos + size > buf.size()) {
            ok = false;
            return 0;
        }
        uint32_t value = 0;
        for (size_t i = 0; i < size; i++) {
            uint8_t byte = (uint8_t)buf[pos + (bigEndian ? i : (size - 1 - i))];
            value = (value << 8) | byte;
        }
        pos += size;
        return value;
    }
};

bool ParseOoTMessageTable(const std::vector<char>& buf) {
    if (buf.size() < kResourceHeaderSize + 4) {
        return false;
    }
    Reader header{ buf, 4, buf[0] != 0 };
    if (header.Read(4) != kResourceTypeText) {
        return false;
    }

    // SoH ResourceFactoryBinaryTextV0: u32 count; per message u16 id, u8 textboxType, u8 textboxYPos, string (u32 size
    // + bytes).
    Reader reader{ buf, kResourceHeaderSize, buf[0] != 0 };
    uint32_t count = reader.Read(4);
    std::unordered_map<u16, OoTMessage> messages;
    for (uint32_t i = 0; (i < count) && reader.ok; i++) {
        OoTMessage message;
        u16 id = (u16)reader.Read(2);
        message.textboxType = (u8)reader.Read(1);
        message.textboxPos = (u8)reader.Read(1);
        uint32_t size = reader.Read(4);
        if (!reader.ok || (reader.pos + size > buf.size())) {
            return false;
        }
        message.msg.assign(buf.data() + reader.pos, size);
        reader.pos += size;
        messages.emplace(id, std::move(message)); // first entry wins (the table ends with a 0xFFFF sentinel copy)
    }
    if (!reader.ok || messages.empty()) {
        return false;
    }
    sOoTMessages = std::move(messages);
    return true;
}

bool LoadOoTMessageTable() {
    if (sOoTMessagesLoaded) {
        return true;
    }
    if (!MergeShip_IsOcarinaMounted()) {
        return false;
    }
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for (const char* path : sOoTMessageTablePaths) {
        auto file = archives->LoadFile(path);
        if ((file == nullptr) || (file->Buffer == nullptr)) {
            continue;
        }
        if (ParseOoTMessageTable(*file->Buffer)) {
            sOoTMessagesLoaded = true;
            SPDLOG_INFO("MergeShip: {} OoT messages loaded from {}", sOoTMessages.size(), path);
            return true;
        }
        SPDLOG_ERROR("MergeShip: OoT message table {} could not be parsed", path);
    }
    return false;
}

// The get-item text id of each item: OoT get-item table (soh/OTRGlobals.cpp VanillaItemTable_Init, textId column).
// GI_CHICKEN and GI_POCKET_CUCCO have 0x48 there, which is the Magic Bean's text ("You got a Magic Bean!"): OoT never
// shows it for them (the eggs hatch in the inventory), so they have no OoT text and use the fallback.
s32 GetOoTGetItemTextId(u8 item) {
    switch (item) {
        case ITEM_OOT_DINS_FIRE:
            return 0xAD; // GI_DINS_FIRE
        case ITEM_OOT_FARORES_WIND:
            return 0xAE; // GI_FARORES_WIND
        case ITEM_OOT_NAYRUS_LOVE:
            return 0xAF; // GI_NAYRUS_LOVE
        case ITEM_OOT_BOOMERANG:
            return 0x35; // GI_BOOMERANG
        case ITEM_OOT_HAMMER:
            return 0x38; // GI_HAMMER
        case ITEM_OOT_MASK_SKULL:
            return 0x10; // GI_MASK_SKULL
        case ITEM_OOT_MASK_SPOOKY:
            return 0x11; // GI_MASK_SPOOKY
        case ITEM_OOT_MASK_GERUDO:
            return 0x16; // GI_MASK_GERUDO
        case ITEM_OOT_MASK_KEATON: // BUG-0066
            return 0x12; // GI_MASK_KEATON
        case ITEM_OOT_MASK_BUNNY:
            return 0x13; // GI_MASK_BUNNY
        case ITEM_OOT_MASK_GORON:
            return 0x14; // GI_MASK_GORON
        case ITEM_OOT_MASK_ZORA:
            return 0x15; // GI_MASK_ZORA
        case ITEM_OOT_MASK_TRUTH:
            return 0x17; // GI_MASK_TRUTH
        case ITEM_OOT_LETTER_RUTO:
            return 0x99; // GI_LETTER_RUTO
        case ITEM_OOT_WEIRD_EGG:
            return 0x9A; // GI_WEIRD_EGG
        case ITEM_OOT_LETTER_ZELDA:
            return 0x69; // GI_LETTER_ZELDA
        case ITEM_OOT_POCKET_EGG:
            return 0x01; // GI_POCKET_EGG
        case ITEM_OOT_COJIRO:
            return 0x02; // GI_COJIRO
        case ITEM_OOT_ODD_MUSHROOM:
            return 0x03; // GI_ODD_MUSHROOM
        case ITEM_OOT_ODD_POTION:
            return 0x04; // GI_ODD_POTION
        case ITEM_OOT_SAW:
            return 0x05; // GI_SAW
        case ITEM_OOT_PRESCRIPTION:
            return 0x09; // GI_PRESCRIPTION
        case ITEM_OOT_FROG:
            return 0x0D; // GI_FROG
        case ITEM_OOT_EYEDROPS:
            return 0x0E; // GI_EYEDROPS
        case ITEM_OOT_CLAIM_CHECK:
            return 0x0A; // GI_CLAIM_CHECK
        case ITEM_OOT_DEKU_SEEDS:
            return 0xDC; // GI_SEEDS_5
        case ITEM_SLINGSHOT:
            return 0x30; // GI_SLINGSHOT
        case ITEM_LONGSHOT:
            return 0x4F; // GI_LONGSHOT
        case ITEM_OOT_CHICKEN:      // GI_CHICKEN: 0x48 (Magic Bean text)
        case ITEM_OOT_POCKET_CUCCO: // GI_POCKET_CUCCO: 0x48 (Magic Bean text)
        default:
            return -1;
    }
}

// OoT text colors (COLOR(x): 0x40 | index) -> MM color codes. OoT's "adjustable" color is green in a normal textbox.
u8 ConvertColor(u8 ootColor) {
    switch (ootColor & 0x0F) {
        case 0x1:
            return 0x01; // RED -> MESSAGE_COLOR_RED
        case 0x2:
            return 0x02; // ADJUSTABLE (green) -> MESSAGE_COLOR_GREEN
        case 0x3:
            return 0x03; // BLUE -> MESSAGE_COLOR_BLUE
        case 0x4:
            return 0x05; // LIGHTBLUE -> MESSAGE_COLOR_LIGHTBLUE
        case 0x5:
            return 0x06; // PURPLE -> MESSAGE_COLOR_PINK
        case 0x6:
            return 0x04; // YELLOW -> MESSAGE_COLOR_YELLOW
        case 0x0: // DEFAULT -> MESSAGE_COLOR_DEFAULT
        case 0x7: // BLACK: MM has no black text color; MM's default is the box's own text color
        default:
            return 0x00;
    }
}

// OoT accented letters 0x80..0x9E (OoT sFontWidths order) -> MM's NES font (MM sNESFontWidths order).
const u8 sOoTAccentToMM[0x9F - 0x80] = {
    0x80, // À
    0xA2, // î
    0x82, // Â
    0x83, // Ä
    0x84, // Ç
    0x85, // È
    0x86, // É
    0x87, // Ê
    0x88, // Ë
    0x8C, // Ï
    0x90, // Ô
    0x91, // Ö
    0x92, // Ù
    0x94, // Û
    0x95, // Ü
    0x96, // ß
    0x97, // à
    0x98, // á
    0x99, // â
    0x9A, // ä
    0x9B, // ç
    0x9C, // è
    0x9D, // é
    0x9E, // ê
    0x9F, // ë
    0xA3, // ï
    0xA7, // ô
    0xA8, // ö
    0xA9, // ù
    0xAB, // û
    0xAC, // ü
};

// OoT textbox type -> MM textbox type (low nibble of MM header byte 0). Both games pick the same background and color
// for 0..3 (OoT messageStaticIndices {0,1,3,2} == MM D_801CFC78[0..3]; black 0,0,0 / wooden 70,50,30 / blue 0,10,50 /
// ocarina); 4 and 5 are "no box" in both.
u8 ConvertTextboxType(u8 ootType) {
    switch (ootType) {
        case 0: // TEXTBOX_TYPE_BLACK
        case 1: // TEXTBOX_TYPE_WOODEN
        case 2: // TEXTBOX_TYPE_BLUE
        case 3: // TEXTBOX_TYPE_OCARINA
        case 4: // TEXTBOX_TYPE_NONE_BOTTOM
        case 5: // TEXTBOX_TYPE_NONE_NO_SHADOW
            return ootType;
        default: // TEXTBOX_TYPE_CREDITS and unknown
            return 0;
    }
}

// OoT message -> MM message body (ends with MM's MESSAGE_END) plus MM header bytes 0 and 1.
bool ConvertOoTMessage(const OoTMessage& oot, std::string* out, u8* mmType, u8* mmPos, s32* droppedCodes) {
    const std::string& in = oot.msg;
    bool unskippable = false;
    s32 dropped = 0;
    std::string mm;

    for (size_t i = 0; i < in.size(); i++) {
        u8 c = (u8)in[i];
        // Argument bytes of the current code (0 when the message is cut short).
        auto arg = [&](size_t n) -> u8 { return (i + n < in.size()) ? (u8)in[i + n] : 0; };

        if (c == 0x02) { // END
            break;
        }
        switch (c) {
            case 0x01: // NEWLINE
                mm += '\x11';
                break;
            case 0x04: // BOX_BREAK
                mm += '\x10';
                break;
            case 0x05: // COLOR(x)
                mm += (char)ConvertColor(arg(1));
                i += 1;
                break;
            case 0x06: // SHIFT(x): OoT x pixels, MM x spaces of 6 pixels
                mm += '\x14';
                mm += (char)((arg(1) + 3) / 6);
                i += 1;
                break;
            case 0x07: // TEXTID(hi, lo): continues in another OoT message; MM would look the id up in MM's table
                i += 2;
                dropped++;
                i = in.size();
                break;
            case 0x08: // QUICKTEXT_ENABLE
                mm += '\x17';
                break;
            case 0x09: // QUICKTEXT_DISABLE
                mm += '\x18';
                break;
            case 0x0A: // PERSISTENT
                mm += '\x1A';
                break;
            case 0x0B: // EVENT
                mm += '\x19';
                break;
            case 0x0C: // BOX_BREAK_DELAYED(x): MM takes a 16-bit frame count
                mm += '\x1B';
                mm += '\x00';
                mm += (char)arg(1);
                i += 1;
                break;
            case 0x0E: // FADE(x): MM takes a 16-bit frame count
                mm += '\x1C';
                mm += '\x00';
                mm += (char)arg(1);
                i += 1;
                break;
            case 0x0F: // NAME
                mm += '\x16';
                break;
            case 0x11: // FADE2(hi, lo)
                mm += '\x1C';
                mm += (char)arg(1);
                mm += (char)arg(2);
                i += 2;
                break;
            case 0x13: // ITEM_ICON(x): MM draws the header's icon
                i += 1;
                break;
            case 0x1A: // UNSKIPPABLE: MM keeps it in the header
                unskippable = true;
                break;
            case 0x1B: // TWO_CHOICE
                mm += '\xC2';
                break;
            case 0x1C: // THREE_CHOICE
                mm += '\xC3';
                break;
            case 0x1F: // TIME
                mm += '\xCA';
                break;

            // No MM equivalent: dropped (with their arguments).
            case 0x12: // SFX(hi, lo): an OoT sound id; MM's ids differ
                i += 2;
                dropped++;
                break;
            case 0x14: // TEXT_SPEED(x): MM's MESSAGE_TEXT_SPEED takes no argument byte
            case 0x1E: // HIGHSCORE(x)
                i += 1;
                dropped++;
                break;
            case 0x15: // BACKGROUND(x, y, z)
                i += 3;
                dropped++;
                break;
            case 0x0D: // AWAIT_BUTTON_PRESS
            case 0x10: // OCARINA
            case 0x16: // MARATHON_TIME
            case 0x17: // RACE_TIME
            case 0x18: // POINTS
            case 0x19: // TOKENS
            case 0x1D: // FISH_INFO
                dropped++;
                break;

            default:
                if ((c >= 0x20) && (c <= 0x7F)) { // ASCII, same in both fonts
                    mm += (char)c;
                } else if ((c >= 0x80) && (c <= 0x9E)) {
                    mm += (char)sOoTAccentToMM[c - 0x80];
                } else if ((c >= 0x9F) && (c <= 0xAA)) { // [A] [B] [C] [L] [R] [Z] [C-Up..C-Right] ▼ [Control-Pad]
                    mm += (char)(0xB0 + (c - 0x9F));
                } else { // 0x00, 0x03 (unused codes), 0xAB [D-Pad] (no MM glyph) and beyond
                    dropped++;
                }
                break;
        }
    }
    mm += '\xBF'; // MESSAGE_END

    *out = std::move(mm);
    *mmType = ConvertTextboxType(oot.textboxType);
    // MM header byte 1: position in the high nibble (0 variable, 1 top, 2 middle, 3+ bottom, as OoT's), and 1 in the
    // low nibble for an unskippable message (MM's own get-item texts use 1).
    *mmPos = (u8)(((oot.textboxPos & 0x0F) << 4) | (unskippable ? 1 : 0));
    if (droppedCodes != nullptr) {
        *droppedCodes = dropped;
    }
    return true;
}

bool BuildOoTGetItemText(u8 item, std::string* out, u8* mmType, u8* mmPos) {
    s32 textId = GetOoTGetItemTextId(item);
    if ((textId < 0) || !LoadOoTMessageTable()) {
        return false;
    }
    auto it = sOoTMessages.find((u16)textId);
    if (it == sOoTMessages.end()) {
        return false;
    }
    s32 dropped = 0;
    ConvertOoTMessage(it->second, out, mmType, mmPos, &dropped);
    if (dropped != 0) {
        MergeShipTest_Log("OoT get-item text 0x%04X (item 0x%02X): %d control code(s) without an MM equivalent dropped",
                          textId, item, dropped);
    }
    return true;
}

} // namespace

extern "C" s32 MergeShip_OoTItems_GetItemText(u8 item, char* out, s32 outSize, u8* textboxType) {
    std::string text;
    u8 mmType = 0;
    u8 mmPos = 0;

    if ((out == nullptr) || (outSize <= 0) || !BuildOoTGetItemText(item, &text, &mmType, &mmPos)) {
        return 0;
    }
    if ((s32)text.size() > outSize) {
        return 0;
    }
    // The body can contain 0x00 (MM's default color code): the length is the return value.
    memcpy(out, text.data(), text.size());
    if ((s32)text.size() < outSize) {
        out[text.size()] = '\0';
    }
    if (textboxType != nullptr) {
        *textboxType = mmType;
    }
    return (s32)text.size();
}

extern "C" void MergeShip_OoTItems_GiveWithCutscene(u8 item) {
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = true,
        .param = (s16)item,
        .giveItem =
            [](Actor* actor, PlayState* play) {
                u8 item = (u8)CUSTOM_ITEM_PARAM;
                CustomMessage::Entry entry;
                std::string text;

                if (BuildOoTGetItemText(item, &text, &entry.textboxType, &entry.textboxYPos)) {
                    entry.autoFormat = false; // OoT's own line and box breaks
                } else {
                    const char* name = MergeShip_OoTItems_Name(item);
                    text = std::string("You got %r") + ((name != nullptr) ? name : "an item") + "%w!";
                    entry.textboxType = 2; // MM's get-item textbox (blue, like OoT's)
                }
                entry.icon = item;
                // MM's Message_DecodeHeader maps the header icon through D_801CFF94; this makes the next header
                // keep `item` as the item id (one-shot).
                MergeShip_OoTItems_SetTextIconPending(item);

                if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                    CustomMessage::SetActiveCustomMessage(text, entry);
                } else {
                    // No get-item cutscene (the player is in the air): the text fades out on its own, as 2Ship's
                    // other give-item events do.
                    if (!entry.autoFormat && !text.empty() && ((u8)text.back() == 0xBF)) {
                        text.pop_back();
                        text += "\x1C\x02\x10\xBF";
                    } else {
                        text += "\x1C\x02\x10";
                    }
                    CustomMessage::StartTextbox(text, entry);
                }

                // MM's Item_Give keeps ITEM_LONGSHOT as MM's "new bottle of Red Potion" (GI_POTION_RED_BOTTLE), so
                // the OoT Longshot goes to the OoT item code directly; every other OoT item is routed there by
                // Item_GiveImpl.
                if (item == ITEM_LONGSHOT) {
                    MergeShip_OoTItems_Give(play, item);
                } else {
                    Item_Give(play, item);
                }
            },
        .drawItem =
            [](Actor* actor, PlayState* play) {
                Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                MergeShip_OoTItems_DrawGetItem(play, (u8)CUSTOM_ITEM_PARAM);
            } });
}

// Makes Ocarina of Time's message `textId` (converted to MM's codes) the active custom message, for a textbox started
// with CUSTOM_MESSAGE_ID (e.g. the player's own textId). Returns 0 when the OoT text is unavailable.
extern "C" s32 MergeShip_OoTItems_SetActiveOoTText(u16 textId) {
    std::string text;
    CustomMessage::Entry entry;
    s32 dropped = 0;

    if (!LoadOoTMessageTable()) {
        return 0;
    }
    auto it = sOoTMessages.find(textId);
    if (it == sOoTMessages.end()) {
        return 0;
    }
    ConvertOoTMessage(it->second, &text, &entry.textboxType, &entry.textboxYPos, &dropped);
    entry.autoFormat = false; // OoT's own line and box breaks
    CustomMessage::SetActiveCustomMessage(text, entry);
    MergeShipTest_Log("OOTITEMS text 0x%04X active (%d code(s) dropped)", textId, dropped);
    return 1;
}
