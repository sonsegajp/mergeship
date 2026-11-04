// MergeShip2 -- Majora's Mask's remaining C-button items, bottle contents and trade items: the SoH side (see
// MergeShipMMItems.h). The save section, the get-item entries in the MOD_NONE item table, the get-item texts, the
// pause-menu names of the bottle contents (they sit in Ocarina of Time's bottle slots), the Hot Spring Water's timer.
//
// Texts: MM's English messages, read out of mm.o2r (mm/text/message_data_static) and kept below as MM wrote them (MM
// control codes), then re-encoded with Ocarina of Time's codes at run time (MMToOoT), laid out as Ocarina of Time's
// get-item messages are (the item's icon opening each box, as MergeShipChateau.cpp does). MM text is English only;
// every language shows it.
//   MM -> OoT: 0x11 newline -> 0x01; 0x10 / 0x12 box break -> 0x04; 0x17 / 0x18 quick text -> 0x08 / 0x09;
//   0x19 event -> 0x0B; 0x1C fade (2-byte frames) -> 0x0E (1 byte); colours 0x00..0x08 -> 0x05 0x4n; button glyphs
//   0xB0.. -> 0x9F..; 0xC2 two choices -> 0x1B; 0xBF end -> 0x02. MM's 0x13 (carriage return), 0x1F (delay, 2 bytes)
//   and 0x1E (sound, 2 bytes) have no Ocarina of Time counterpart and are dropped.

#include <cstring>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include <z64.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/MergeShip/MergeShipMMItems.h"
extern PlayState* gPlayState;
}

namespace {

// ---- texts ----------------------------------------------------------------------------------------------------

#define MM_TEXT(literal) std::string(literal, sizeof(literal) - 1)

// MM's get-item messages, ITEM_MM_POWDER_KEG .. ITEM_MM_PENDANT_OF_MEMORIES (MM control codes).
const std::string& MMGetItemText(u8 item) {
    static const std::string texts[] = {
        // ITEM_MM_POWDER_KEG: MM 0x34
        MM_TEXT("\x17" "You got a " "\x01" "Powder Keg" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Press " "\x04" "\xB2" " " "\x00" "to light it, then tap " "\x04" "\xB2" "\x11" "\x00" "to place it. Be careful--its sheer" "\x11" "power and size are immense!" "\xBF"),
        // ITEM_MM_PICTOGRAPH_BOX: MM 0x43
        MM_TEXT("\x17" "You got a " "\x01" "Pictograph Box" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Press " "\x04" "\xB2" " " "\x00" "to look through it and" "\x11" "press " "\xB0" " to snap a picture." "\x11" "\x12" "You can shoot only one pictograph" "\x11" "at a time, but you can retake it" "\x11" "as many times as you like." "\xBF"),
        // ITEM_MM_DEKU_PRINCESS: MM 0x5F
        MM_TEXT("\x17" "You put the " "\x01" "Deku princess" "\x00" " in a" "\x11" "bottle!" "\x18" "\x11" "\x13" "\x12" "It's a tight fit, but she says she'll" "\x11" "be fine if you hurry. Use " "\x04" "\xB2" "\x00" " to" "\x11" "let her out." "\xBF"),
        // ITEM_MM_ZORA_EGG: MM 0x69
        MM_TEXT("\x17" "You put a " "\x01" "Zora Egg" "\x00" " in a bottle!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "It doesn't look very healthy." "\x11" "You'd better have someone" "\x11" "examine it quick!" "\xBF"),
        // ITEM_MM_GOLD_DUST: MM 0x6A (GI_GOLD_DUST, a bottle of it)
        MM_TEXT("\x17" "You got a bottle of " "\x01" "Gold Dust" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "This is the finest quality" "\x11" "available!" "\xBF"),
        // ITEM_MM_MUSHROOM: MM 0x6B
        MM_TEXT("\x17" "You got a " "\x01" "Magic Mushroom" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Your nose is tickled by its" "\x11" "fragrant scent!" "\xBF"),
        // ITEM_MM_SEAHORSE: MM 0x95 (GI_SEAHORSE_CAUGHT; GI_SEAHORSE's 0x6E is empty)
        MM_TEXT("\x17" "You got a " "\x01" "seahorse" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Hurry! Take it to its home at" "\x11" "Pinnacle Rock!" "\xBF"),
        // ITEM_MM_HYLIAN_LOACH: MM's own texts (0x70, 0x94) are empty -- the item is unused in MM -- so this is MM's
        // get-item wording for a bottled catch (0x69's), with the loach's name.
        MM_TEXT("\x17" "You put a " "\x01" "Hylian Loach" "\x00" " in a bottle!" "\x18" "\xBF"),
        // ITEM_MM_SPRING_WATER: MM 0x67
        MM_TEXT("\x17" "You filled your bottle with" "\x11" "\x01" "Spring Water" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Use " "\x04" "\xB2" "\x00" " to pour it on whatever" "\x11" "may need it." "\xBF"),
        // ITEM_MM_HOT_SPRING_WATER: MM 0x68
        MM_TEXT("\x17" "You filled your bottle with" "\x11" "\x01" "Hot Spring Water" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Use it with " "\x04" "\xB2" "\x00" " before it cools." "\xBF"),
        // ITEM_MM_MOONS_TEAR: MM 0x96
        MM_TEXT("\x17" "You got the " "\x01" "Moon's Tear" "\x00" "!" "\x18" "\x11" "\x1F" "\x00" "\x0A" "Its sparkling beauty radiates" "\x11" "rather strangely." "\xBF"),
        // ITEM_MM_DEED_LAND: MM 0x97
        MM_TEXT("\x17" "You traded the Moon's Tear for a" "\x11" "\x01" "Land Title Deed" "\x00" "!" "\x18" "\xBF"),
        // ITEM_MM_DEED_SWAMP: MM 0x98
        MM_TEXT("\x17" "You traded your Land Title Deed" "\x11" "for a " "\x01" "Swamp Title Deed" "\x00" "!" "\x18" "\xBF"),
        // ITEM_MM_DEED_MOUNTAIN: MM 0x99
        MM_TEXT("\x17" "You traded your Swamp Title Deed" "\x11" "for a " "\x01" "Mountain Title Deed" "\x00" "!" "\x18" "\xBF"),
        // ITEM_MM_DEED_OCEAN: MM 0x9A
        MM_TEXT("\x17" "You traded your Mountain Title" "\x11" "Deed for an " "\x01" "Ocean Title Deed" "\x00" "!" "\x18" "\xBF"),
        // ITEM_MM_ROOM_KEY: MM 0xA0
        MM_TEXT("\x17" "You got a " "\x01" "Room Key" "\x00" "!" "\x18" "\x11" "\x13" "\x13" "\x12" "???" "\x11" "You're not sure why, but you" "\x11" "apparently had a reservation..." "\xBF"),
        // ITEM_MM_LETTER_TO_MAMA: MM 0xA1
        MM_TEXT("\x17" "You were given " "\x01" "express mail to" "\x11" "Mama" "\x00" "." "\x18" "\x11" "\x13" "\x12" "This is priority mail, so hurry up" "\x11" "and deliver it!" "\xBF"),
        // ITEM_MM_LETTER_TO_KAFEI: MM 0xAA
        MM_TEXT("\x17" "You were given a " "\x01" "letter to Kafei" "\x00" "!" "\x18" "\x11" "\x13" "\x13" "Quick! Deliver it for her! Take it" "\x10" "to a " "\x01" "mailbox" "\x00" "." "\xBF"),
        // ITEM_MM_PENDANT_OF_MEMORIES: MM 0xAB
        MM_TEXT("\x17" "You accepted the " "\x01" "Pendant of" "\x11" "Memories" "\x00" "!" "\x18" "\x11" "\x13" "\x12" "Though you may not understand" "\x11" "these grown-up matters, you" "\x11" "should probably take the pendant" "\x11" "to " "\x01" "Anju" "\x00" ", anyway." "\xBF"),
    };
    static_assert(sizeof(texts) / sizeof(texts[0]) == ITEM_MM_PENDANT_OF_MEMORIES - ITEM_MM_POWDER_KEG + 1,
                  "a get-item text per MM item");
    return texts[item - ITEM_MM_POWDER_KEG];
}

// MM 0xFA (Interface_UpdateBottleTimers).
const std::string kMMHotWaterCooled = MM_TEXT("Your " "\x01" "Hot Spring Water " "\x00" "cooled." "\x11" "Now it's just plain old spring" "\x11" "water!" "\x1C" "\x00" "(" "\xBF");
// MM 0xFE (an item shown to nobody).
const std::string kMMItemNoEffect = MM_TEXT("\x03" "This item doesn't seem to work" "\x11" "here..." "\xBF");

// MM text colour (CTRL_COLOR_*) -> Ocarina of Time's colour byte (0x40 + CTRL_*).
char MMColorToOoT(unsigned char mmColor) {
    switch (mmColor) {
        case 0x01: // red
            return 0x41;
        case 0x02: // green
            return 0x42;
        case 0x03: // blue
            return 0x43;
        case 0x04: // yellow
        case 0x08: // orange
            return 0x46;
        case 0x05: // light blue
            return 0x44;
        case 0x06: // pink
            return 0x45;
        default: // default, silver
            return 0x40;
    }
}

// Re-encodes an MM message for Ocarina of Time (see the top of the file). `iconItem` != ITEM_NONE opens each box with
// the item's icon, as Ocarina of Time's get-item messages do.
std::string MMToOoT(const std::string& mm, u8 iconItem) {
    std::string out;
    size_t i = 0;

    auto icon = [&]() {
        if (iconItem != ITEM_NONE) {
            out += '\x13';
            out += static_cast<char>(iconItem);
        }
    };

    icon();
    while (i < mm.size()) {
        unsigned char c = static_cast<unsigned char>(mm[i++]);

        if ((c >= 0x20) && (c < 0x7F)) {
            out += static_cast<char>(c);
        } else if (c <= 0x08) {
            out += '\x05';
            out += MMColorToOoT(c);
        } else if ((c == 0x10) || (c == 0x12)) {
            out += '\x04';
            icon();
        } else if (c == 0x11) {
            out += '\x01';
        } else if (c == 0x17) {
            out += '\x08';
        } else if (c == 0x18) {
            out += '\x09';
        } else if (c == 0x19) {
            out += '\x0B';
        } else if (c == 0x1C) {
            // MM fade: two bytes of frames; Ocarina of Time's takes one.
            unsigned int frames = 0;
            if (i + 1 < mm.size()) {
                frames = (static_cast<unsigned char>(mm[i]) << 8) | static_cast<unsigned char>(mm[i + 1]);
            }
            i += 2;
            out += '\x0E';
            out += static_cast<char>((frames > 0xFF) ? 0xFF : frames);
        } else if ((c == 0x1E) || (c == 0x1F)) {
            i += 2; // MM sound / delay: two argument bytes, no counterpart
        } else if ((c >= 0xB0) && (c <= 0xBB)) {
            out += static_cast<char>(0x9F + (c - 0xB0)); // A, B, C, L, R, Z, C-up/down/left/right, target, stick
        } else if (c == 0xC2) {
            out += '\x1B';
        } else if (c == 0xBF) {
            break;
        }
        // Anything else (MM 0x13 carriage return ...) has no counterpart.
    }
    out += '\x02';
    return out;
}

void LoadMessage(const std::string& text, TextBoxType type, bool* loadFromMessageTable) {
    CustomMessage msg(text, text, text, type, TEXTBOX_POS_BOTTOM);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// ---- save section ----------------------------------------------------------------------------------------------

void InitMMItemsSave(bool isDebug) {
    (void)isDebug;
    MergeShip_MMItems_InitSave();
}

void LoadMMItemsVersion1() {
    SaveManager::Instance->LoadData("powderKeg", gMergeShipMMItemsSave.powderKeg, (u8)ITEM_NONE);
    SaveManager::Instance->LoadData("powderKegAmmo", gMergeShipMMItemsSave.powderKegAmmo, (u8)0);
    SaveManager::Instance->LoadData("pictographBox", gMergeShipMMItemsSave.pictographBox, (u8)ITEM_NONE);
    SaveManager::Instance->LoadData("tradeDeed", gMergeShipMMItemsSave.tradeDeed, (u8)ITEM_NONE);
    SaveManager::Instance->LoadData("tradeKeyMama", gMergeShipMMItemsSave.tradeKeyMama, (u8)ITEM_NONE);
    SaveManager::Instance->LoadData("tradeCouple", gMergeShipMMItemsSave.tradeCouple, (u8)ITEM_NONE);
    SaveManager::Instance->LoadData("tradeOwned", gMergeShipMMItemsSave.tradeOwned, (u16)0);
    // A save from before the owned set: the items the slots show are owned.
    for (u8 shown : { gMergeShipMMItemsSave.tradeDeed, gMergeShipMMItemsSave.tradeKeyMama,
                      gMergeShipMMItemsSave.tradeCouple }) {
        if (MERGESHIP_ITEM_IS_MM_TRADE(shown)) {
            gMergeShipMMItemsSave.tradeOwned |= MERGESHIP_MM_TRADE_BIT(shown);
        }
    }
    if (gMergeShipMMItemsSave.powderKegAmmo > MERGESHIP_POWDER_KEG_MAX_AMMO) {
        gMergeShipMMItemsSave.powderKegAmmo = MERGESHIP_POWDER_KEG_MAX_AMMO;
    }
}

void SaveMMItems(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("powderKeg", gMergeShipMMItemsSave.powderKeg);
    SaveManager::Instance->SaveData("powderKegAmmo", gMergeShipMMItemsSave.powderKegAmmo);
    SaveManager::Instance->SaveData("pictographBox", gMergeShipMMItemsSave.pictographBox);
    SaveManager::Instance->SaveData("tradeDeed", gMergeShipMMItemsSave.tradeDeed);
    SaveManager::Instance->SaveData("tradeKeyMama", gMergeShipMMItemsSave.tradeKeyMama);
    SaveManager::Instance->SaveData("tradeCouple", gMergeShipMMItemsSave.tradeCouple);
    SaveManager::Instance->SaveData("tradeOwned", gMergeShipMMItemsSave.tradeOwned);
}

// ---- hooks -----------------------------------------------------------------------------------------------------

// The get-item entries in the MOD_NONE item table (every ItemTable_Retrieve of their ids finds them). The vanilla
// table exists once a file is loaded.
void RegisterGetItems() {
    for (int item = ITEM_MM_POWDER_KEG; item <= ITEM_MM_PENDANT_OF_MEMORIES; item++) {
        s16 getItemId = MergeShip_MMItems_GetItemId(static_cast<u8>(item));
        if (getItemId != GI_NONE) {
            ItemTableManager::Instance->AddItemEntry(MOD_NONE, getItemId,
                                                     MergeShip_MMItems_GetItemEntry(static_cast<u8>(item)));
        }
    }
}

void OnLoadGame(int32_t fileNum) {
    (void)fileNum;
    RegisterGetItems();
    // MM keeps the bottle timers and the keg's fuse outside the save.
    MergeShip_MMItems_ResetBottleTimers();
    gMergeShipPowderKegTimer = 0;
}

// MM Interface_UpdateBottleTimers runs every frame from Interface_Update, paused or not (it measures the paused time).
void OnGameFrameUpdate() {
    if (gPlayState != nullptr) {
        MergeShip_MMItems_UpdateBottleTimers(gPlayState);
    }
}

void RegisterMergeShipMMItems() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    SaveManager::Instance->AddLoadFunction("mergeShipMMItems", 1, LoadMMItemsVersion1);
    SaveManager::Instance->AddSaveFunction("mergeShipMMItems", 1, SaveMMItems, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(InitMMItemsSave);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(OnLoadGame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnGameFrameUpdate);

    for (int item = ITEM_MM_POWDER_KEG; item <= ITEM_MM_PENDANT_OF_MEMORIES; item++) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            MERGESHIP_TEXT_MM_ITEMS_BASE + (item - ITEM_MM_POWDER_KEG),
            [item](uint16_t* textId, bool* loadFromMessageTable) {
                (void)textId;
                // MM textbox type 2 = Ocarina of Time's blue item box.
                LoadMessage(MMToOoT(MMGetItemText(static_cast<u8>(item)), static_cast<u8>(item)), TEXTBOX_TYPE_BLUE,
                            loadFromMessageTable);
            });
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        MERGESHIP_TEXT_MM_HOT_WATER_COOLED, [](uint16_t* textId, bool* loadFromMessageTable) {
            (void)textId;
            LoadMessage(MMToOoT(kMMHotWaterCooled, ITEM_NONE), TEXTBOX_TYPE_BLACK, loadFromMessageTable);
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        MERGESHIP_TEXT_MM_ITEM_NO_EFFECT, [](uint16_t* textId, bool* loadFromMessageTable) {
            (void)textId;
            LoadMessage(MMToOoT(kMMItemNoEffect, ITEM_NONE), TEXTBOX_TYPE_BLACK, loadFromMessageTable);
        });

    // KaleidoScope_UpdateNamePanel: the MM bottle contents in Ocarina of Time's bottle slots are past its name table.
    REGISTER_VB_SHOULD(VB_DRAW_CUSTOM_ITEM_NAME, {
        u32 namedItem = va_arg(args, u32);
        if ((namedItem <= 0xFF) && MergeShip_MMItems_Handles(static_cast<u8>(namedItem)) && (gPlayState != nullptr)) {
            const char* texture = MergeShip_MMItems_NameTexture(static_cast<u8>(namedItem));
            if (texture != nullptr) {
                *should = true;
                memcpy(gPlayState->pauseCtx.nameSegment, texture, strlen(texture) + 1);
            }
        }
    });

    // MM Inventory_UpdateBottleItem: Hot Spring Water put into a bottle starts that bottle's timer.
    REGISTER_VB_SHOULD(VB_UPDATE_BOTTLE_ITEM, {
        int button = va_arg(args, int);
        int item = va_arg(args, int);
        if ((item == ITEM_MM_HOT_SPRING_WATER) && (button >= 1) && (button <= 7)) {
            s16 slot = gSaveContext.equips.cButtonSlots[button - 1];
            if ((slot >= SLOT_BOTTLE_1) && (slot <= SLOT_BOTTLE_4)) {
                MergeShip_MMItems_StartBottleTimer(slot - SLOT_BOTTLE_1);
            }
        }
    });
}

RegisterShipInitFunc sInitFunc(RegisterMergeShipMMItems, {});

} // namespace
