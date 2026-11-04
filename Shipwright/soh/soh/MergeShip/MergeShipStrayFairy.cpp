// MergeShip2 -- Majora's Mask's stray fairies, SoH side (MergeShipStrayFairy.h): the actor's ActorDB registration, the
// container hooks, the chest hooks, the get-item entries and MM's texts.
//
// Texts: MM's English messages from mm.o2r (mm/text/message_data_static), re-encoded with Ocarina of Time's control
// codes as MergeShipChateau.cpp does (MM 0x17/0x18 quick text -> 0x08/0x09, 0x11 newline -> 0x01, 0x10 box break ->
// 0x04, 0x01 red / 0x05 light blue / 0x00 default -> 0x05 0x41 / 0x05 0x44 / 0x05 0x40, 0x1C fade -> 0x0E, 0xBF end ->
// 0x02):
//   0x11 (type 2, icon ITEM_STRAY_FAIRIES): 0x17 "You found a " 0x01 "Stray Fairy" 0x00 "!" 0x18 0x11 0x1F 0x00 0x0A
//        "This is your " 0x01 0x0C 0x00 " one!" 0x1C 0x00 0x28 0xBF
//   0xF3 (type 2, icon ITEM_STRAY_FAIRIES): 0x17 "You found all the " 0x01 "Stray Fairies" 0x00 "!" 0x18 0x11 0x1F 0x00
//        0x0A "Quick! Take them to their fellow" 0x11 "fairies of the " 0x01 "same color" 0x00 "!" 0x1C 0x00 0x28 0xBF
//   0x579 (type 0): 0x1E 0x6938 0x05 "Please, hear my plea!" 0x11 "The masked Skull Kid has broken" 0x11 "me apart and
//        scattered my" 0x11 "pieces!" 0x10 "Please find a way to return me to" 0x11 "the " 0x01 "Fairy Fountain" 0x05
//        " in " 0x01 "North Clock" 0x11 "Town" 0x05 "." 0xBF
// MM 0x0C (MESSAGE_STRAY_FAIRIES) is the count with its English ordinal ("1st", "2nd", "3rd", "21st", ... "th"),
// z_message_nes.c. MM's 0x1F 0x000A (a 10-frame pause before the second line) has no Ocarina of Time code and is
// dropped. MM's text sound 0x1E 0x6938 (NA_SE_VO_TFVO00, the fairy's voice) names MM's sound tables, so it is played
// through MergeShip_PlayMMSfxCentered when the text opens.
//
// The icon: MM draws gStrayFairyGlowingCircleIconTex (I4, 32 x 24) in the temple's colours (z_message.c
// Message_DrawItemIcon's ITEM_STRAY_FAIRIES branch). gItemIcons has no such texture, so the icon is loaded and drawn by
// VB_LOAD_ITEM_ICON / VB_DRAW_ITEM_ICON hooks. SoH's randomizer registers the same hooks for every icon past
// ITEM_CUSTOM; these are registered again each time one of these texts opens so they run last (see LoadIcon).

#include <string>

#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"

extern "C" {
#include <z64.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "message_data_fmt.h"
#include "soh/MergeShip/MergeShipStrayFairy.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
#include "src/overlays/actors/ovl_MM_En_Elforg/z_mm_en_elforg.h"
#include "mm/interface/parameter_static/parameter_static.h"
extern PlayState* gPlayState;
}

extern "C" s16 gMergeShipTextboxIconHeight; // z_message_PAL.c

namespace {

// ---- the actor -----------------------------------------------------------------------------------------------------

void RegisterMMEnElforg() {
    if (gMergeShipActorId_MM_En_Elforg >= 0) {
        return;
    }

    ActorDBInit entry;
    entry.name = "MM_En_Elforg";
    entry.desc = "Majora's Mask stray fairy";
    entry.category = ACTORCAT_ITEMACTION;
    entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    entry.objectId = OBJECT_GAMEPLAY_KEEP;
    entry.instanceSize = sizeof(MMEnElforg);
    entry.init = (ActorFunc)MMEnElforg_Init;
    entry.destroy = (ActorFunc)MMEnElforg_Destroy;
    entry.update = (ActorFunc)MMEnElforg_Update;
    entry.draw = (ActorFunc)MMEnElforg_Draw;

    gMergeShipActorId_MM_En_Elforg = (s16)ActorDB::Instance->AddEntry(entry).entry.id;
}

// ---- texts ---------------------------------------------------------------------------------------------------------

#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_BOX_BREAK "\x04"
#define MS_COLOR_DEFAULT "\x05\x40"
#define MS_COLOR_RED "\x05\x41"
#define MS_COLOR_LIGHTBLUE "\x05\x44"
#define MS_QUICKTEXT_ENABLE "\x08"
#define MS_QUICKTEXT_DISABLE "\x09"
#define MS_FADE "\x0E"
#define MS_ITEM_ICON "\x13"

// MM MESSAGE_STRAY_FAIRIES: the number and its ordinal.
std::string Ordinal(s32 count) {
    const char* suffix = "th";
    if ((count == 1) || (count == 21)) {
        suffix = "st";
    } else if ((count == 2) || (count == 22)) {
        suffix = "nd";
    } else if ((count == 3) || (count == 23)) {
        suffix = "rd";
    }
    return std::to_string(count) + suffix;
}

std::string StrayFairyIcon() {
    return std::string(MS_ITEM_ICON) + static_cast<char>(ITEM_MM_STRAY_FAIRY);
}

void LoadText(const std::string& text, TextBoxType type, TextBoxPosition position, bool* loadFromMessageTable) {
    CustomMessage msg(text, text, text, type, position);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterIconHooks();
// The area whose colours the icon of the text on screen takes.
s32 sIconArea = MM_STRAY_FAIRY_AREA_WOODFALL;

// MM 0x11, or MM 0xF3 for the fifteenth (MM Message_StartTextbox's remap).
void BuildFoundMessage(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    s32 area;
    s32 count;
    std::string text;

    MergeShip_StrayFairy_GetMessageArea(&area, &count);
    if ((area != MM_STRAY_FAIRY_AREA_CLOCK_TOWN) && (count == MM_STRAY_FAIRY_SCATTERED_TOTAL)) {
        text = StrayFairyIcon() + MS_QUICKTEXT_ENABLE "You found all the " MS_COLOR_RED "Stray Fairies" MS_COLOR_DEFAULT
                                  "!" MS_QUICKTEXT_DISABLE MS_NEWLINE "Quick! Take them to their fellow" MS_NEWLINE
                                  "fairies of the " MS_COLOR_RED "same color" MS_COLOR_DEFAULT "!" MS_FADE "\x28" MS_END;
    } else {
        text = StrayFairyIcon() + MS_QUICKTEXT_ENABLE "You found a " MS_COLOR_RED "Stray Fairy" MS_COLOR_DEFAULT
                                  "!" MS_QUICKTEXT_DISABLE MS_NEWLINE "This is your " MS_COLOR_RED +
               Ordinal(count) + MS_COLOR_DEFAULT " one!" MS_FADE "\x28" MS_END;
    }
    sIconArea = area;
    RegisterIconHooks();
    // MM textbox type 2 / y 0: Ocarina of Time's blue item box at the bottom (as MergeShipChateau.cpp).
    LoadText(text, TEXTBOX_TYPE_BLUE, TEXTBOX_POS_BOTTOM, loadFromMessageTable);
}

// MM 0x579, the Clock Town fairy.
void BuildClockTownMessage(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    static const std::string text =
        MS_COLOR_LIGHTBLUE "Please, hear my plea!" MS_NEWLINE "The masked Skull Kid has broken" MS_NEWLINE
                           "me apart and scattered my" MS_NEWLINE "pieces!" MS_BOX_BREAK
                           "Please find a way to return me to" MS_NEWLINE "the " MS_COLOR_RED
                           "Fairy Fountain" MS_COLOR_LIGHTBLUE " in " MS_COLOR_RED "North Clock" MS_NEWLINE
                           "Town" MS_COLOR_LIGHTBLUE "." MS_END;

    // MM 0x1E 0x6938: NA_SE_VO_TFVO00.
    MergeShip_PlayMMSfxCentered(MM_NA_SE_VO_TFVO00);
    // MM textbox type 0 / y 0: the black box (as MergeShipMaskMessages.cpp).
    LoadText(text, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE, loadFromMessageTable);
}

// ---- the icon ------------------------------------------------------------------------------------------------------

bool ShowingStrayFairyText() {
    if (gPlayState == nullptr) {
        return false;
    }
    u16 textId = gPlayState->msgCtx.textId;
    return (textId == MERGESHIP_TEXT_STRAY_FAIRY_FOUND) || (textId == MERGESHIP_TEXT_STRAY_FAIRY_FOUND_ALL);
}

// Message_Decode, MESSAGE_ITEM_ICON with ITEM_MM_STRAY_FAIRY: Message_LoadItemIcon's placement for a 32-texel-wide
// icon (the texture itself is drawn straight from mm.o2r by DrawIcon). msgBufPos is on the icon code until someone
// consumes it; SoH's randomizer hook may already have (it advances msgBufPos too), and must not be advanced twice.
void LoadIcon(bool displayAsEnglish) {
    static const s16 sIconItem32XOffsets[] = { 74, 74, 74, 54 };
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Font* font = &msgCtx->font;
    u8 language = displayAsEnglish ? LANGUAGE_ENG : gSaveContext.language;
    bool unconsumed = ((u8)font->msgBuf[msgCtx->msgBufPos] == MESSAGE_ITEM_ICON) &&
                      ((u8)font->msgBuf[msgCtx->msgBufPos + 1] == ITEM_MM_STRAY_FAIRY);
    bool consumed = (msgCtx->msgBufPos > 0) && ((u8)font->msgBuf[msgCtx->msgBufPos - 1] == MESSAGE_ITEM_ICON) &&
                    ((u8)font->msgBuf[msgCtx->msgBufPos] == ITEM_MM_STRAY_FAIRY);

    if (!unconsumed && !consumed) {
        return;
    }
    R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem32XOffsets[language < ARRAY_COUNT(sIconItem32XOffsets) ? language : 0];
    // MM places the 24-texel-high circle as Ocarina of Time places its 24 x 24 icons.
    R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 10;
    R_TEXTBOX_ICON_SIZE = 32;
    gMergeShipTextboxIconHeight = 24; // the 32 x 24 circle, not stretched (z_message_PAL.c)
    if (unconsumed) {
        msgCtx->msgBufPos++;
        msgCtx->choiceNum = 1;
    }
}

// MM Message_DrawItemIcon, ITEM_STRAY_FAIRIES: the glowing circle in the temple's colours.
void DrawIcon(Gfx** gfxP) {
    MessageContext* msgCtx = &gPlayState->msgCtx;

    // MM's two layers: the area's glowing circle, then its fairy (z_message_PAL.c draws the fairy's rectangle).
    MergeShip_StrayFairy_DrawTextIcon(gfxP, sIconArea, msgCtx->textPosX + R_TEXTBOX_ICON_XPOS, R_TEXTBOX_ICON_YPOS,
                                      (u8)msgCtx->textColorAlpha);
}

// Registered (again) whenever one of these texts opens, so they come after any other VB_LOAD/DRAW_ITEM_ICON hook.
void RegisterIconHooks() {
    static HOOK_ID sLoadHook = 0;
    static HOOK_ID sDrawHook = 0;

    GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(sLoadHook);
    GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(sDrawHook);
    sLoadHook = REGISTER_VB_SHOULD(VB_LOAD_ITEM_ICON, {
        if (!*should && ShowingStrayFairyText()) {
            LoadIcon(static_cast<bool>(va_arg(args, int)));
        }
    });
    sDrawHook = REGISTER_VB_SHOULD(VB_DRAW_ITEM_ICON, {
        if (!*should && ShowingStrayFairyText()) {
            DrawIcon(va_arg(args, Gfx**));
        }
    });
}

// ---- hooks ---------------------------------------------------------------------------------------------------------

void RegisterGetItems() {
    for (s32 area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN; area < MM_STRAY_FAIRY_AREA_MAX; area++) {
        ItemTableManager::Instance->AddItemEntry(MOD_NONE, MERGESHIP_GI_MM_STRAY_FAIRY(area),
                                                 MergeShip_StrayFairy_GetItemEntry(area));
    }
}

void RegisterMergeShipStrayFairy() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    RegisterMMEnElforg();

    // The vanilla item table exists once a file is loaded (as MergeShipChateau.cpp).
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        (void)fileNum;
        RegisterGetItems();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(
        [](void* actor) { MergeShip_StrayFairy_OnActorUpdate(static_cast<Actor*>(actor)); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorKill>(
        [](void* actor) { MergeShip_StrayFairy_OnActorKill(static_cast<Actor*>(actor)); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>(
        [](void* actor) { MergeShip_StrayFairy_OnActorDestroy(static_cast<Actor*>(actor)); });

    // A chest holding a fairy: MM GI_STRAY_FAIRY is ITEM_NONE, so Link kicks the lid (no slow chest cutscene) and holds
    // nothing up; the chest lets the fairy out (MergeShip_StrayFairy_OnActorUpdate).
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_CHEST, {
        Actor* chest = va_arg(args, Actor*);
        if (MergeShip_StrayFairy_ContainerHoldsFairy(chest)) {
            MergeShip_StrayFairy_KickOpenChest(gPlayState);
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_PLAY_SLOW_CHEST_CS, {
        Actor* chest = va_arg(args, Actor*);
        if (MergeShip_StrayFairy_ContainerHoldsFairy(chest)) {
            *should = false;
        }
    });

    // The squeak of a fairy saved (BUG-0016: the user, 2026-10-07: the sound of the one picked up in Clock Town --
    // NA_SE_VO_TFVO00, MM's 0x579 text sound): whenever a textbox shows a stray fairy's icon -- picked up, or a
    // randomizer get-item -- never for a fairy delivered without a textbox. The icon code is either still unread or just
    // consumed by another load hook (LoadIcon above), so both places are checked.
    REGISTER_VB_SHOULD(VB_LOAD_ITEM_ICON, {
        if ((gPlayState == nullptr) || *should) {
            return;
        }
        MessageContext* msgCtx = &gPlayState->msgCtx;
        Font* font = &msgCtx->font;
        const bool unconsumed = ((u8)font->msgBuf[msgCtx->msgBufPos] == MESSAGE_ITEM_ICON) &&
                                ((u8)font->msgBuf[msgCtx->msgBufPos + 1] == ITEM_MM_STRAY_FAIRY);
        const bool consumed = (msgCtx->msgBufPos > 0) &&
                              ((u8)font->msgBuf[msgCtx->msgBufPos - 1] == MESSAGE_ITEM_ICON) &&
                              ((u8)font->msgBuf[msgCtx->msgBufPos] == ITEM_MM_STRAY_FAIRY);
        static u16 sSqueakedText = 0xFFFF;
        static u32 sSqueakedFrame = 0;
        if ((unconsumed || consumed) &&
            ((msgCtx->textId != sSqueakedText) || (gPlayState->gameplayFrames - sSqueakedFrame > 30))) {
            sSqueakedText = msgCtx->textId;
            sSqueakedFrame = gPlayState->gameplayFrames;
            MergeShip_PlayMMSfxCentered(MM_NA_SE_VO_TFVO00);
        }
    });

    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_STRAY_FAIRY_FOUND,
                                                                                BuildFoundMessage);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_STRAY_FAIRY_FOUND_ALL,
                                                                                BuildFoundMessage);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_STRAY_FAIRY_CLOCK_TOWN,
                                                                                BuildClockTownMessage);
}

RegisterShipInitFunc sInitFunc(RegisterMergeShipStrayFairy, {});

} // namespace

// BUG-0003: GameInteractor runs a vanilla-behavior hook list in no fixed order (an unordered_map), so the randomizer's
// own icon hooks (ItemMessages.cpp) must stand aside for these texts rather than rely on running first: when they ran
// last, they loaded their Triforce piece fallback over the fairy.
extern "C" bool MergeShip_StrayFairy_OwnsTextIcon(void) {
    return ShowingStrayFairyText();
}
