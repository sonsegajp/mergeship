/**
 * This file handles custom messages relating to Items,
 * such as Get Item messages for non-vanilla items,
 * Vanilla/MQ hints when collecting Maps, Ice Trap messages,
 * etc.
 */

#include <cstdarg>
#include <algorithm>

#include <soh/OTRGlobals.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/Traps.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/ShipInit.hpp"
#include "soh/MergeShip/MergeShipUnified.h"
extern "C" bool MergeShip_StrayFairy_OwnsTextIcon(void); // MergeShipStrayFairy.cpp
extern "C" {
#include "soh/MergeShip/MergeShipMMEquipment.h"
}
#include "soh/MergeShip/MergeShipStrayFairy.h" // MergeShip2: the stray fairy text icon
#include "soh/MergeShip/MergeShipDekuBeans.h"  // MergeShip2: Progressive Deku Beans' step names
#include <soh/ResourceManagerHelpers.h>

extern "C" {
#include "variables.h"
#include "macros.h"
#include "functions.h"
#include "z64item.h"
extern PlayState* gPlayState;
}

void BuildTriforcePieceMessage(CustomMessage& msg) {
    auto rando = OTRGlobals::Instance->gRandomizer;
    uint8_t current = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected + 1;
    // if any settings are off, 0 them out here as a precaution
    uint8_t bridge = rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE) == RO_BRIDGE_TRIFORCE_PIECES
                         ? rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT)
                         : 0;
    uint8_t wincon = rando->GetRandoSettingValue(RSK_WINCON) == RO_WINCON_TRIFORCE_PIECES
                         ? rando->GetRandoSettingValue(RSK_WINCON_TRIFORCE_COUNT)
                         : 0;
    uint8_t GBK = rando->GetRandoSettingValue(RSK_GANONS_BOSS_KEY) == RO_GANON_BOSS_KEY_TRIFORCE_PIECES
                      ? rando->GetRandoSettingValue(RSK_GBK_TRIFORCE_COUNT)
                      : 0;
    uint8_t soul = rando->GetRandoSettingValue(RSK_GANONS_SOUL) == RO_GANONS_SOUL_TRIFORCE_PIECES
                       ? rando->GetRandoSettingValue(RSK_GANONS_SOUL_TRIFORCE_COUNT)
                       : 0;

    // If we reach wincon, we win!
    if (current == wincon) {
        msg = { "You completed the %yTriforce of Courage%w! %gGG%w!",
                "Das %yTriforce des Mutes%w! Du hast alle Splitter gefunden. %gGut gemacht%w!",
                "Vous avez complété la %yTriforce du Courage%w! %gFélicitations%w!" };
        // otherwise prioritise the different triggers
    } else if (current == bridge) {
        msg = { "You made your wish to the %yTriforce%w! %rTh%ye R%gai%cnb%bow %pBr%rid%yge %gha%cs r%bai%psed%w!",
                TODO_TRANSLATE, TODO_TRANSLATE };
    } else if (current == GBK) {
        msg = { "You completed the %yTriforce of Power%w! %rThe Key to Evil is yours%w!", TODO_TRANSLATE,
                TODO_TRANSLATE };
    } else if (current == soul) {
        msg = { "You completed the %yTriforce of Wisdom%w! %bGanon's soul is reclaimed%w!", TODO_TRANSLATE,
                TODO_TRANSLATE };
        // if everything is zero, then there's no goal...
    } else if (bridge + wincon + GBK + soul == 0) {
        msg = { "You found a %yTriforce Piece%w! But it's %puseless%w...", TODO_TRANSLATE, TODO_TRANSLATE };
    } else {
        // if nothing is complete, we need to check is we have more than we need
        uint8_t highest = std::max({ current, bridge, wincon, GBK, soul });
        if (highest == current) {
            // RANDOTODO TODO_TRANSLATE you could maybe make this sound cleaner because InsertNumber allows for dynamic
            // plurals
            msg = { "You found a spare %yTriforce Piece%w! You only needed %c[[d]]%w, but you have %g[[current]]%w!",
                    "Ein übriger %yTriforce-Splitter%w! Du hast nun %g[[current]]%w von %c[[d]]%w nötigen gefunden.",
                    "Vous avez trouvé un %yFragment de Triforce%w en plus! Vous n'aviez besoin que de %c[[d]]%w, "
                    "mais vous en avez %g[[current]]%w en tout!" };
            msg.InsertNumber(std::max({ bridge, wincon, GBK, soul }));
        } else {
            // find the next goal by setting everything below current (including failed conditions set to 0 before)
            // to a high number, then looking for the lowest.
            // if we have the exact amount, it will be caught by the first check, so no worries there
            if (bridge < current) {
                bridge = 255;
            }
            if (GBK < current) {
                GBK = 255;
            }
            if (soul < current) {
                soul = 255;
            }
            if (wincon < current) {
                wincon = 255;
            }
            uint8_t next = std::min({ bridge, GBK, soul, wincon });

            uint8_t remaining = next - current;
            float percentageCollected = (float)current / (float)next;

            if (percentageCollected <= 0.25) {
                msg = { "You found a %yTriforce Piece%w! %g[[current]]%w down, %c[[d]]%w more and you [[condition]]! "
                        "It's a start!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected <= 0.5) {
                msg = { "You found a %yTriforce Piece%w! that makes %g[[current]]%w, %c[[d]]%w to go until you "
                        "[[condition]]! Progress!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected <= 0.75) {
                msg = { "You found a %yTriforce Piece%w! You have %g[[current]]%w and need %c[[d]]%w more and you "
                        "[[condition]]! Over half-way there!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected < 1.0) {
                msg = { "You found a %yTriforce Piece%w! %g[[current]]%w down, %c[[d]]%w left until you [[condition]]! "
                        "Almost done!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            }

            // default condition is soul
            CustomMessage condition = { "%brelease Ganons Soul%w", TODO_TRANSLATE, TODO_TRANSLATE };
            if (next == wincon) {
                condition = { "%gWin the game%w", TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (next == bridge) {
                condition = { "%csummon the Rainbow Bridge%w", TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (next == GBK) {
                condition = { "%rfind the key to Ganondorf's Lair%w", TODO_TRANSLATE, TODO_TRANSLATE };
            }
            msg.Replace("[[condition]]", condition);
            msg.InsertNumber(remaining);
        }
    }
    msg.Replace("[[current]]", std::to_string(current));
    msg.AutoFormat(ITEM_CUSTOM);
}

void BuildTriforceMessage(CustomMessage& msg) {
    msg = { "You completed the %yTriforce of&Courage%w! %gGG%w!",
            "Das %yTriforce des Mutes%w! Du hast&alle Splitter gefunden. %gGut gemacht%w!",
            "Vous avez complété la %yTriforce&du Courage%w! %gFélicitations%w!" };
    msg.Format(ITEM_CUSTOM);
}

void BuildCustomItemMessage(Player* player, CustomMessage& msg) {
    int16_t rgid;
    msg = CustomMessage("You found [[article]][[color]][[name]]%w!", "Du hast [[article]][[color]][[name]]%w gefunden!",
                        "Vous avez trouvé [[article]][[color]][[name]]%w!", TEXTBOX_TYPE_BLUE);
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        rgid = player->getItemEntry.getItemId;
    } else {
        rgid = player->getItemId;
    }
    // MergeShip2: an item of Termina's (the unified randomizer) is found here for there (MergeShipUnified.h).
    if (MergeShipUnified_ForTermina(static_cast<RandomizerGet>(rgid))) {
        msg = CustomMessage("You found [[article]][[color]][[name]]%w for %rTermina%w!",
                            "Du hast [[article]][[color]][[name]]%w für %rTermina%w gefunden!",
                            "Vous avez trouvé [[article]][[color]][[name]]%w pour %rTermina%w!", TEXTBOX_TYPE_BLUE);
    }
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetName(), TEXTBOX_TYPE_BLUE);
    if (rgid == RG_OPEN_CHEST &&
        OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_OPEN_CHEST).Is(RO_OPEN_CHEST_PROGRESSIVE)) {
        // message is built before the item is given, so the flags still say which copy this is
        name = Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST)
                   ? CustomMessage("Open Big Chests", "Große Truhen öffnen", "Ouvrir les grands coffres",
                                   TEXTBOX_TYPE_BLUE)
                   : CustomMessage("Open Small Chests", "Kleine Truhen öffnen", "Ouvrir les petits coffres",
                                   TEXTBOX_TYPE_BLUE);
    }
    // MergeShip2: Progressive Deku Beans says which step it is (Deku, Gold Deku Beans), the message being built
    // before the item is given.
    if (rgid == RG_PROGRESSIVE_DEKU_BEANS) {
        switch (MergeShip_DekuBeans_NextStep()) {
            case 2:
                name = CustomMessage("Deku Beans", "Deku-Bohnen", "Haricots Mojo", TEXTBOX_TYPE_BLUE);
                break;
            default:
                name = CustomMessage("Gold Deku Beans", "Goldene Deku-Bohnen", "Haricots Mojo dores",
                                     TEXTBOX_TYPE_BLUE);
                break;
        }
    }
    // MergeShip2: MM's progressive sword says which sword it is (the user, 2026-09-24: the Razor Sword is not labelled
    // a progressive sword; it does not break). The message is built before the item is given.
    if (rgid == RG_PROGRESSIVE_MM_SWORD) {
        name = MergeShip_MMEquip_Owns(MMEQUIP_RAZOR) || MergeShip_MMEquip_Owns(MMEQUIP_GILDED)
                   ? CustomMessage("Gilded Sword (MM)", "Goldenes Schwert (MM)", "Epee doree (MM)", TEXTBOX_TYPE_BLUE)
                   : CustomMessage("Razor Sword (MM)", "Rasiermesser-Schwert (MM)", "Epee rasoir (MM)",
                                   TEXTBOX_TYPE_BLUE);
    }
    CustomMessage article = CustomMessage(
        Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetArticle(), TEXTBOX_TYPE_BLUE);
    msg.Replace("[[article]]", article);
    msg.Replace("[[color]]", Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetColor());
    msg.Replace("[[name]]", name);
    if (Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).HasCustomIcon()) {
        msg.AutoFormat(ITEM_CUSTOM);
    } else {
        msg.AutoFormat();
    }
}

extern "C" s16 gMergeShipTextboxIconHeight; // z_message_PAL.c

void LoadCustomItemIcon(bool displayAsEnglish) {
    Player* player = GET_PLAYER(gPlayState);
    const char* customIcon = nullptr;
    CustomIconSize iconSize = ICON_SIZE_32;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        customIcon = Rando::StaticData::RetrieveItem(rgid).GetCustomIcon();
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    } else {
        // if we're seeing an icon and we don't have a GI, assume we're in the alter text showing a triforce piece
        customIcon = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetCustomIcon();
        iconSize = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetCustomIconSize();
    }
    if (customIcon != nullptr) {
        static int16_t sIconItem32XOffsets[] = { 74, 74, 74, 54 };
        static int16_t sIconItem24XOffsets[] = { 72, 72, 72, 50 };
        MessageContext* msgCtx = &gPlayState->msgCtx;
        uint8_t language = displayAsEnglish ? LANGUAGE_ENG : (Language)gSaveContext.language;
        if ((iconSize == ICON_SIZE_32) || (iconSize == ICON_SIZE_32X24)) {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem32XOffsets[language];
            // MergeShip2: a 32x24 icon (clamped below its last row) sits 4 lower, centred like a 32 square.
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + ((iconSize == ICON_SIZE_32X24) ? 10 : 6);
            R_TEXTBOX_ICON_SIZE = 32;
            if (iconSize == ICON_SIZE_32X24) {
                gMergeShipTextboxIconHeight = 24; // drawn 32 x 24 (z_message_PAL.c)
            }
        } else {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem24XOffsets[language];
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 10;
            R_TEXTBOX_ICON_SIZE = 24;
            if (iconSize == ICON_SIZE_NOTE_IA8) {
                // MergeShip2: 16 wide in the 24 square, centred (the texture clamps to its transparent edge).
                R_TEXTBOX_ICON_XPOS += 4;
            }
        }
        strcpy((char*)((uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE), customIcon);
        msgCtx->msgBufPos++;
        msgCtx->choiceNum = 1;
    }
}

void DrawCustomItemIcon(Gfx** p) {
    Gfx* gfx = *p;
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Player* player = GET_PLAYER(gPlayState);
    CustomIconSize iconSize = ICON_SIZE_32;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    }
    if (iconSize == ICON_SIZE_24) {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 24, 24, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else if (iconSize == ICON_SIZE_32X24) {
        // MergeShip2: Majora's Mask's stray fairy icon as MM draws it: the area's glowing circle, then its fairy
        // (MergeShipStrayFairy.h); the fairy's 32x24 rectangle is z_message_PAL.c's.
        s32 area = MM_STRAY_FAIRY_AREA_CLOCK_TOWN;
        switch (player->getItemEntry.getItemId) {
            case RG_MM_WOODFALL_STRAY_FAIRY:
                area = MM_STRAY_FAIRY_AREA_WOODFALL;
                break;
            case RG_MM_SNOWHEAD_STRAY_FAIRY:
                area = MM_STRAY_FAIRY_AREA_SNOWHEAD;
                break;
            case RG_MM_GREAT_BAY_STRAY_FAIRY:
                area = MM_STRAY_FAIRY_AREA_GREAT_BAY;
                break;
            case RG_MM_STONE_TOWER_STRAY_FAIRY:
                area = MM_STRAY_FAIRY_AREA_STONE_TOWER;
                break;
            default:
                break;
        }
        MergeShip_StrayFairy_DrawTextIcon(&gfx, area, msgCtx->textPosX + R_TEXTBOX_ICON_XPOS, R_TEXTBOX_ICON_YPOS,
                                          (u8)msgCtx->textColorAlpha);
    } else if (iconSize == ICON_SIZE_NOTE_IA8) {
        // MergeShip2: Majora's Mask's song note (16x24 IA8), drawn in the 24 square.
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_IA,
                            G_IM_SIZ_8b, 16, 24, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    *p = gfx;
}

void BuildItemMessage(u16* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    CustomMessage msg;

    if (player->getItemEntry.getItemId == RG_ICE_TRAP) {
        Rando::Traps::BuildIceTrapMessage(msg, player->getItemEntry);
    } else if (player->getItemEntry.getItemId == RG_TRIFORCE_PIECE) {
        BuildTriforcePieceMessage(msg);
    } else if (player->getItemEntry.getItemId == RG_TRIFORCE) {
        BuildTriforceMessage(msg);
    } else {
        BuildCustomItemMessage(player, msg);
    }
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildMapMessage(uint16_t* textId, bool* loadFromMessageTable) {
    GetItemEntry itemEntry = GET_PLAYER(gPlayState)->getItemEntry;
    auto ctx = OTRGlobals::Instance->gRandoContext;
    CustomMessage msg =
        CustomMessage("You found the %g[[name]]%w! [[typeHint]]", "Du erhältst das %g[[name]]%w! [[typeHint]]",
                      "Vous obtenez %g[[name]]%w! [[typeHint]]", TEXTBOX_TYPE_BLUE);
    int sceneNum = -1;
    switch (itemEntry.getItemId) {
        case RG_DEKU_TREE_MAP:
            sceneNum = SCENE_DEKU_TREE;
            break;
        case RG_DODONGOS_CAVERN_MAP:
            sceneNum = SCENE_DODONGOS_CAVERN;
            break;
        case RG_JABU_JABUS_BELLY_MAP:
            sceneNum = SCENE_JABU_JABU;
            break;
        case RG_FOREST_TEMPLE_MAP:
            sceneNum = SCENE_FOREST_TEMPLE;
            break;
        case RG_FIRE_TEMPLE_MAP:
            sceneNum = SCENE_FIRE_TEMPLE;
            break;
        case RG_WATER_TEMPLE_MAP:
            sceneNum = SCENE_WATER_TEMPLE;
            break;
        case RG_SPIRIT_TEMPLE_MAP:
            sceneNum = SCENE_SPIRIT_TEMPLE;
            break;
        case RG_SHADOW_TEMPLE_MAP:
            sceneNum = SCENE_SHADOW_TEMPLE;
            break;
        case RG_BOTTOM_OF_THE_WELL_MAP:
            sceneNum = SCENE_BOTTOM_OF_THE_WELL;
            break;
        case RG_ICE_CAVERN_MAP:
            sceneNum = SCENE_ICE_CAVERN;
            break;
    }
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(itemEntry.getItemId)).GetName());
    msg.Replace("[[name]]", name);
    if (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_NONE) ||
        (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_SET_NUMBER) &&
         ctx->GetOption(RSK_MQ_DUNGEON_COUNT).Is(MAX_MQ_DUNGEON_COUNT))) {
        msg.Replace("[[typeHint]]", "");
    } else if (ResourceMgr_IsSceneMasterQuest(sceneNum)) {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_MASTERFUL].GetHintMessage());
    } else {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_ORDINARY].GetHintMessage());
    }
    *loadFromMessageTable = false;
    msg.AutoFormat(ITEM_DUNGEON_MAP);
    msg.LoadIntoFont();
}

void BuildBossKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildSmallKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GERUDO_FORTRESS_SMALL_KEY &&
        OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).Is(RO_GERUDO_KEYS_VANILLA)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GERUDO_FORTRESS_SMALL_KEY &&
        DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void RegisterItemMessages() {
    COND_ID_HOOK(OnOpenText, TEXT_RANDOMIZER_CUSTOM_ITEM, IS_RANDO, BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_DUNGEON_MAP, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildMapMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_COMPASS, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_BOSS,
                 (DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)),
                 BuildBossKeyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_SMALL,
                 (OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).IsNot(RO_GERUDO_KEYS_VANILLA) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)),
                 BuildSmallKeyMessage);
}

static RegisterShipInitFunc initFunc(RegisterItemMessages, { "IS_RANDO" });

void RegisterCustomIconHooks() {
    COND_VB_SHOULD(VB_LOAD_ITEM_ICON, IS_RANDO, {
        // MergeShip2 (BUG-0003): MM's stray fairy texts draw their own icon (MergeShipStrayFairy.cpp).
        if ((*should == false) && !MergeShip_StrayFairy_OwnsTextIcon()) {
            LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
        }
    });
    COND_VB_SHOULD(VB_DRAW_ITEM_ICON, IS_RANDO, {
        if ((*should == false) && !MergeShip_StrayFairy_OwnsTextIcon()) {
            DrawCustomItemIcon(va_arg(args, Gfx**));
        }
    });
}

static RegisterShipInitFunc customIconInitFunc(RegisterCustomIconHooks, { "IS_RANDO" });