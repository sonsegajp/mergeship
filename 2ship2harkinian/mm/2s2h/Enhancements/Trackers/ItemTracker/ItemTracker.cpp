#include "ItemTracker.h"
#include "ItemTrackerSettings.h"

#include "2s2h/BenGui/UIWidgets.hpp"
#include "Rando/Rando.h"
#include "Rando/ActorBehavior/Souls.h"
#include "Rando/MiscBehavior/ClockShuffle.h"
#include "2s2h/BenPort.h"
#include "2s2h/ShipUtils.h"
#include <spdlog/fmt/fmt.h>
#include <algorithm>

extern "C" {
#include "z64save.h"
#include "variables.h"
#include <macros.h>
#include <functions.h>
#include "2s2h_assets.h"
#include "assets/archives/icon_item_static/icon_item_static_yar.h"
#include "overlays/actors/ovl_En_Si/z_en_si.h"
}

#include <fast/Fast3dGui.h>
#include <mergeship/SharedSave.h>     // MergeShip2 (BUG-0131): Hyrule's items
#include "2s2h/MergeShip/MergeShipHost.h" // MergeShipHost_SharedSave
#include "2s2h/MergeShip/MergeShipHyrule.h" // MergeShip_Hyrule_QuestIconTexture
extern "C" {
#include "2s2h/MergeShip/MergeShipOwnership.h"    // MergeShip2 (BUG-0151): MergeShip_Own_MMOwns
#include "2s2h/MergeShip/MergeShipTerminaEquip.h" // MergeShip_TerminaEquip_Owns
}

namespace BenGui {
extern std::shared_ptr<ItemTrackerWindow> mItemTrackerWindow;
}

#define FORMAT_COUNT "{}/{}"

#define CVAR_NAME_VISIBILITY_MODE "gSettings.ItemTracker.VisibilityMode"
#define CVAR_NAME_VISIBILITY_BTN "gSettings.ItemTracker.VisibilityBtn"
#define CVAR_VISIBILITY_MODE CVarGetInteger(CVAR_NAME_VISIBILITY_MODE, ITEM_TRACKER_VISIBILITY_MODE_ALWAYS)
#define CVAR_VISIBILITY_BTN CVarGetInteger(CVAR_NAME_VISIBILITY_BTN, BTN_CUSTOM_MODIFIER1)

std::vector<TrackerGroup> itemTrackerGroups;
static bool sItemTrackerBtnState = false;

static bool IsTradeItemObtained(RandoItemId randoItemId) {
    ItemId itemId = Rando::StaticData::Items[randoItemId].itemId;
    if (INV_CONTENT(itemId) == itemId) {
        return true;
    }

    switch (randoItemId) {
        case RI_MOONS_TEAR:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_MOONS_TEAR);
        case RI_DEED_LAND:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_DEED_LAND);
        case RI_DEED_SWAMP:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_DEED_SWAMP);
        case RI_DEED_MOUNTAIN:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_DEED_MOUNTAIN);
        case RI_DEED_OCEAN:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_DEED_OCEAN);
        case RI_ROOM_KEY:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_ROOM_KEY);
        case RI_LETTER_TO_MAMA:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_LETTER_TO_MAMA);
        case RI_LETTER_TO_KAFEI:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_LETTER_TO_KAFEI);
        case RI_PENDANT_OF_MEMORIES:
            return Flags_GetRandoInf(RANDO_INF_OBTAINED_PENDANT_OF_MEMORIES);
        default:
            return false;
    }
}

// When the file isn't loaded everythin is ocarinas, so we fall back to the first safe item
u32 GetVanillaItemIdForSlot(u32 slot) {
    bool isSaveLoaded = gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL;
    u32 vanillaItemId = isSaveLoaded ? gSaveContext.save.saveInfo.inventory.items[slot] : ITEM_NONE;

    if (vanillaItemId == ITEM_NONE || vanillaItemId >= ITEM_RECOVERY_HEART) {
        vanillaItemId = safeItemsForInventorySlot[slot][0];
    }

    return vanillaItemId;
}

TrackerImageObject GetImageObject(TrackerItemType itemType, u32 itemId) {
    bool isSaveLoaded = gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL;
    bool itemObtained = false;
    TrackerImageObject trackerImageObject = {
        .textureColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        .textureDimensions = ImVec2(ITEM_TEXTURE_SIZE, ITEM_TEXTURE_SIZE),
    };

    switch (itemType) {
        case TRACKER_ITEM_RANDO: {
            RandoItemId randoItemId = (RandoItemId)itemId;
            switch (randoItemId) {
                case RI_GS_TOKEN_SWAMP: {
                    itemObtained = Inventory_GetSkullTokenCount(SCENE_KINSTA1) > 0;
                } break;
                case RI_GS_TOKEN_OCEAN: {
                    itemObtained = Inventory_GetSkullTokenCount(SCENE_KINDAN2) > 0;
                } break;
                case RI_WOODFALL_SMALL_KEY: {
                    itemObtained = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE) > 0;
                } break;
                case RI_SNOWHEAD_SMALL_KEY: {
                    itemObtained = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE) > 0;
                } break;
                case RI_GREAT_BAY_SMALL_KEY: {
                    itemObtained = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE) > 0;
                } break;
                case RI_STONE_TOWER_SMALL_KEY: {
                    itemObtained = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE) > 0;
                } break;
                case RI_WOODFALL_STRAY_FAIRY: {
                    itemObtained =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE] > 0;
                } break;
                case RI_SNOWHEAD_STRAY_FAIRY: {
                    itemObtained =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE] > 0;
                } break;
                case RI_GREAT_BAY_STRAY_FAIRY: {
                    itemObtained =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE] > 0;
                } break;
                case RI_STONE_TOWER_STRAY_FAIRY: {
                    itemObtained =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] > 0;
                } break;
                case RI_TRIFORCE_PIECE: {
                    itemObtained = gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces > 0;
                } break;
                case RI_SONG_SARIA: {
                    itemObtained = gSaveContext.save.shipSaveInfo.rando.sariaHintsAvailable > 0;
                } break;
                case RI_MOONS_TEAR:
                case RI_DEED_LAND:
                case RI_DEED_SWAMP:
                case RI_DEED_MOUNTAIN:
                case RI_DEED_OCEAN:
                case RI_ROOM_KEY:
                case RI_LETTER_TO_MAMA:
                case RI_LETTER_TO_KAFEI:
                case RI_PENDANT_OF_MEMORIES: {
                    itemObtained = IsTradeItemObtained(randoItemId);
                } break;
                default: {
                    itemObtained = !Rando::IsItemObtainable(randoItemId);
                } break;
            }

            trackerImageObject.textureColor = Ship_GetRandoItemColorTint(randoItemId);
            const char* texturePath = Rando::StaticData::GetIconTexturePath(randoItemId);
            if (texturePath != nullptr) {
                trackerImageObject.textureId =
                    std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                        ->GetTextureByName(texturePath);
            }
            if (randoItemId >= RI_OWL_CLOCK_TOWN_SOUTH && randoItemId <= RI_OWL_ZORA_CAPE) {
                trackerImageObject.textureDimensions.y = 24.0f;
            } else if (randoItemId >= RI_SONG_DOUBLE_TIME && randoItemId <= RI_SONG_TIME) {
                trackerImageObject.textureDimensions.x = 32.0f;
            }
        } break;
        case TRACKER_ITEM_SLOT: {
            itemObtained = gSaveContext.save.saveInfo.inventory.items[itemId] != ITEM_NONE;
            auto vanillaItemId = GetVanillaItemIdForSlot(itemId);

            trackerImageObject.textureId =
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName((const char*)gItemIcons[vanillaItemId]);
        } break;
        case TRACKER_ITEM_SWORD: {
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) > EQUIP_VALUE_SWORD_NONE) {
                itemObtained = true;
            }
            int vanillaItemId = ITEM_SWORD_KOKIRI;
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) > EQUIP_VALUE_SWORD_KOKIRI) {
                vanillaItemId = ITEM_SWORD_KOKIRI + GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) - EQUIP_VALUE_SWORD_KOKIRI;
            }

            trackerImageObject.textureId =
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName((const char*)gItemIcons[vanillaItemId]);
        } break;
        case TRACKER_ITEM_SHIELD: {
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) > EQUIP_VALUE_SHIELD_NONE) {
                itemObtained = true;
            }
            int vanillaItemId = ITEM_SHIELD_HERO;
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) > EQUIP_VALUE_SHIELD_HERO) {
                vanillaItemId = ITEM_SHIELD_MIRROR;
            }

            trackerImageObject.textureId =
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName((const char*)gItemIcons[vanillaItemId]);
        } break;
        case TRACKER_ITEM_WALLET: {
            if (CUR_UPG_VALUE(UPG_WALLET) >= 1) {
                itemObtained = true;
            }
            auto vanillaItemId = ITEM_WALLET_ADULT;
            if (CUR_UPG_VALUE(UPG_WALLET) >= 2) {
                vanillaItemId = ITEM_WALLET_GIANT;
            }

            trackerImageObject.textureId =
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName((const char*)gItemIcons[vanillaItemId]);
        } break;
        case TRACKER_ITEM_MAGIC: {
            if (gSaveContext.save.saveInfo.playerData.isMagicAcquired) {
                itemObtained = true;
            }
            auto vanillaItemId = ITEM_MAGIC_JAR_SMALL;
            if (gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired) {
                vanillaItemId = ITEM_MAGIC_JAR_BIG;
            }

            trackerImageObject.textureId =
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName((const char*)gItemIcons[vanillaItemId]);
        } break;
        default:
            break;
    }

    if (!isSaveLoaded) {
        itemObtained = false;
    }

    trackerImageObject.textureColor.w = itemObtained ? 1.0f : 0.4f;
    return trackerImageObject;
}

std::string GetItemCounts(TrackerItemType itemType, u32 itemId) {
    std::string countStr = "";

    switch (itemType) {
        case TRACKER_ITEM_RANDO: {
            switch (itemId) {
                case RI_TRIFORCE_PIECE: {
                    if (IS_RANDO && RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES]) {
                        auto max = RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED];
                        countStr = fmt::format(FORMAT_COUNT, gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces,
                                               max > 999 ? "1k" : std::to_string(max));
                    }
                } break;
                case RI_SONG_SARIA: {
                    auto count = gSaveContext.save.shipSaveInfo.rando.sariaHintsAvailable;
                    if (count > 1) {
                        countStr = std::to_string(count);
                    }
                } break;
                case RI_GS_TOKEN_OCEAN:
                case RI_GS_TOKEN_SWAMP: {
                    auto max =
                        IS_RANDO ? RANDO_SAVE_OPTIONS[RO_SKULLTULA_TOKENS_REQUIRED] : SPIDER_HOUSE_TOKENS_REQUIRED;
                    auto count =
                        Inventory_GetSkullTokenCount(itemId == RI_GS_TOKEN_SWAMP ? SCENE_KINSTA1 : SCENE_KINDAN2);
                    countStr = fmt::format(FORMAT_COUNT, count, max);
                } break;
                case RI_WOODFALL_STRAY_FAIRY: {
                    auto max = IS_RANDO ? RANDO_SAVE_OPTIONS[RO_STRAY_FAIRIES_REQUIRED] : STRAY_FAIRY_SCATTERED_TOTAL;
                    auto count = gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE];
                    countStr = fmt::format(FORMAT_COUNT, count, max);
                } break;
                case RI_SNOWHEAD_STRAY_FAIRY: {
                    auto max = IS_RANDO ? RANDO_SAVE_OPTIONS[RO_STRAY_FAIRIES_REQUIRED] : STRAY_FAIRY_SCATTERED_TOTAL;
                    auto count = gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE];
                    countStr = fmt::format(FORMAT_COUNT, count, max);
                } break;
                case RI_GREAT_BAY_STRAY_FAIRY: {
                    auto max = IS_RANDO ? RANDO_SAVE_OPTIONS[RO_STRAY_FAIRIES_REQUIRED] : STRAY_FAIRY_SCATTERED_TOTAL;
                    auto count =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE];
                    countStr = fmt::format(FORMAT_COUNT, count, max);
                } break;
                case RI_STONE_TOWER_STRAY_FAIRY: {
                    auto max = IS_RANDO ? RANDO_SAVE_OPTIONS[RO_STRAY_FAIRIES_REQUIRED] : STRAY_FAIRY_SCATTERED_TOTAL;
                    auto count =
                        gSaveContext.save.saveInfo.inventory.strayFairies[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE];
                    countStr = fmt::format(FORMAT_COUNT, count, max);
                } break;
                case RI_WOODFALL_SMALL_KEY: {
                    auto count = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
                    if (count > 0)
                        countStr = fmt::format(FORMAT_COUNT, count, 1);
                } break;
                case RI_SNOWHEAD_SMALL_KEY: {
                    auto count = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
                    if (count > 0)
                        countStr = fmt::format(FORMAT_COUNT, count, 3);
                } break;
                case RI_GREAT_BAY_SMALL_KEY: {
                    auto count = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
                    if (count > 0)
                        countStr = fmt::format(FORMAT_COUNT, count, 1);
                } break;
                case RI_STONE_TOWER_SMALL_KEY: {
                    auto count = DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
                    if (count > 0)
                        countStr = fmt::format(FORMAT_COUNT, count, 4);
                } break;
                default:
                    break;
            }
        } break;
        case TRACKER_ITEM_SLOT: {
            switch (itemId) {
                case SLOT_BOW:
                case SLOT_BOMB:
                case SLOT_BOMBCHU:
                case SLOT_DEKU_STICK:
                case SLOT_DEKU_NUT:
                case SLOT_MAGIC_BEANS:
                case SLOT_POWDER_KEG: {
                    auto count = gSaveContext.save.saveInfo.inventory.ammo[itemId];
                    if (count > 0)
                        countStr = std::to_string(count);
                } break;
                default:
                    break;
            }
        } break;
        case TRACKER_ITEM_WALLET: {
            countStr = std::to_string(CUR_CAPACITY(UPG_WALLET));
        } break;
        default:
            break;
    }

    return countStr;
}

// Choose a different font for the font size otherwise it gets scaled and blurry.
static ImFont* GetItemCountFont(float fontSize) {
    if (fontSize >= 22.0f) {
        return OTRGlobals::Instance->fontMonoLargest;
    }
    if (fontSize >= 18.0f) {
        return OTRGlobals::Instance->fontMonoLarger;
    }
    return OTRGlobals::Instance->fontMono;
}

void DrawItemCounts(TrackerItemType itemType, u32 itemId, ImVec2 cellMin, ImVec2 cellSize, float scale) {
    std::string itemCount = GetItemCounts(itemType, itemId);

    if (itemCount.empty()) {
        return;
    }

    float fontSize = std::max(cellSize.x * 0.44f, 12.0f) * ImGui::GetIO().FontGlobalScale;
    ImFont* font = GetItemCountFont(fontSize);
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, itemCount.c_str());

    float pad = 2.0f * scale;
    ImVec2 textPos(cellMin.x + cellSize.x - textSize.x - pad, cellMin.y + cellSize.y - textSize.y - pad);

    static const ImVec2 outlineDirections[] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 },
                                                { 1, 0 },   { -1, 1 }, { 0, 1 },  { 1, 1 } };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float outline = std::max(1.0f, fontSize / 12.0f);
    for (const ImVec2& direction : outlineDirections) {
        drawList->AddText(font, fontSize, textPos + direction * outline, IM_COL32(0, 0, 0, 255), itemCount.c_str());
    }
    drawList->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 255), itemCount.c_str());
}

bool DrawItemTrackerSlot(TrackerItemType itemType, u32 itemId, float scale, bool clickable) {
    ImVec2 cellSize(ITEM_TEXTURE_SIZE * scale, ITEM_TEXTURE_SIZE * scale);

    TrackerImageObject imageObject = GetImageObject(itemType, itemId);

    // Create an invisible button for layout + clicks
    bool clicked = false;
    if (clickable) {
        imageObject.textureColor.w = 1.0f; // force full opacity for clickable items
        clicked = ImGui::InvisibleButton(std::to_string(itemId).c_str(), cellSize);
    } else {
        ImGui::Dummy(cellSize);
    }

    // Draw image centered inside the cell
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();

    if (clickable) {
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        ImU32 bg;
        if (held) {
            bg = IM_COL32(0, 0, 0, 60); // pressed
        } else if (hovered) {
            bg = IM_COL32(255, 255, 255, 20); // hover
        } else {
            bg = IM_COL32(0, 0, 0, 0); // idle subtle
        }

        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, bg, 4.0f); // small rounding
    }

    float aspect = imageObject.textureDimensions.x / imageObject.textureDimensions.y;

    // fit to height
    ImVec2 drawSize(cellSize.y * aspect, cellSize.y);

    // clamp if too wide
    if (drawSize.x > cellSize.x) {
        drawSize.x = cellSize.x;
        drawSize.y = cellSize.x / aspect;
    }

    ImVec2 offset((cellSize.x - drawSize.x) * 0.5f, (cellSize.y - drawSize.y) * 0.5f);

    // Draw a glow behind skulltula tokens
    if (itemType == TRACKER_ITEM_RANDO && (itemId == RI_GS_TOKEN_SWAMP || itemId == RI_GS_TOKEN_OCEAN)) {
        ImVec4 tintColor = ImVec4(25.0f / 255.0f, 251.0f / 255.0f, 0.0f, imageObject.textureColor.w); // Swamp tint
        if (itemId == RI_GS_TOKEN_OCEAN) {
            tintColor = ImVec4(0.0f, 209.0f / 256.0f, 231.0f / 256.0f, imageObject.textureColor.w); // Ocean tint
        }
        auto textureId =
            std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                ->GetTextureByName(gMagicArrowEquipEffectTex);

        // Draw texture behind the actual item icon
        ImGui::GetWindowDrawList()->AddImage(textureId, p0 + offset - ImVec2(8.0f, 8.0f),
                                             p0 + offset + drawSize + ImVec2(8.0f, 8.0f), ImVec2(0, 0), ImVec2(1, 1),
                                             ImGui::GetColorU32(tintColor));
    }

    if (itemType == TRACKER_ITEM_RANDO && itemId >= RI_SOUL_BOSS_GOHT && itemId <= RI_SOUL_BOSS_TWINMOLD) {
        ImVec4 tintColor =
            ImVec4(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, imageObject.textureColor.w); // Swamp tint
        auto textureId =
            std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                ->GetTextureByName(gMagicArrowEquipEffectTex);

        // Draw texture behind the actual item icon
        ImGui::GetWindowDrawList()->AddImage(textureId, p0 + offset - ImVec2(8.0f, 8.0f),
                                             p0 + offset + drawSize + ImVec2(8.0f, 8.0f), ImVec2(0, 0), ImVec2(1, 1),
                                             ImGui::GetColorU32(tintColor));
    }

    if (imageObject.textureId != nullptr) {
        ImGui::GetWindowDrawList()->AddImage(imageObject.textureId, p0 + offset, p0 + offset + drawSize, ImVec2(0, 0),
                                             ImVec2(1, 1), ImGui::GetColorU32(imageObject.textureColor));
    }
    auto itemName = GetItemTrackerItemName(itemType, itemId);
    if (!itemName.empty()) {
        UIWidgets::Tooltip(itemName.c_str());
    }

    if (CVarGetInteger("gSettings.ItemTracker.ItemCounts", 1)) {
        DrawItemCounts(itemType, itemId, p0, cellSize, scale);
    }

    return clicked;
}

void DrawItemTrackerGroup(TrackerGroup& trackerGroup) {
    int columns = trackerGroup.columns;
    if (trackerGroup.items.size() < trackerGroup.columns) {
        columns = trackerGroup.items.size();
    }

    float scale = CVarGetInteger("gSettings.ItemTracker.WindowGroup", 0)
                      ? trackerGroup.scale
                      : CVarGetFloat("gSettings.ItemTracker.Scale", 1.0f);

    if (ImGui::BeginTable(trackerGroup.name.c_str(), columns)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        for (auto& [itemType, itemId] : trackerGroup.items) {
            ImGui::TableNextColumn();
            DrawItemTrackerSlot(itemType, itemId, scale, false);
        }

        ImGui::PopStyleVar(2);
        ImGui::EndTable();
    }
}

// ---- MergeShip2 (BUG-0131/0151): both worlds' items ------------------------------------------------------------------
// "Both Worlds' Items" (gEnhancements.MergeShip.ItemTrackerBothWorlds, on; mergeship.exe): the item tracker is the one
// tracker Ship of Harkinian draws too -- Hyrule's half (Ocarina of Time's items, equipment, upgrades, dungeon rewards and
// songs, as Hyrule's file holds them in the shared save, MsSharedSave) and Termina's beside it, in the same order. Owned
// at full colour, faded otherwise, as the tracker draws its own.

namespace {

struct HyruleCell {
    const char* texture; // __OTR__oot/... (NULL: an empty cell)
    const char* name;
    bool owned;
    ImVec4 tint;
};

#define MS_OOT_ICON(name) "__OTR__oot/textures/icon_item_static/" name

#define MS_OOT_QUEST_ICON(name) "__OTR__oot/textures/icon_item_24_static/" name

// BUG-0151: the order Ship of Harkinian's tracker draws Hyrule in (randomizer_item_tracker.cpp mainWindowItems: the
// inventory, the equipment, the misc items, the dungeon rewards, the songs), six to a row, so that both games draw the
// one tracker the same way.
std::vector<HyruleCell> HyruleCells(const MsSharedSave* s) {
    const ImVec4 white(1, 1, 1, 1);
    auto item = [&](MsItem bit) { return (s->items & (1u << bit)) != 0; };
    auto own = [&](MsOwnBit bit) { return (s->hyruleOwn & (1u << bit)) != 0; };
    auto equip = [&](int shift, int index) { return (s->ootOwnedEquipment & (1u << (shift + index))) != 0; };
    auto quest = [&](int bit) { return (s->hyruleQuest & (1u << bit)) != 0; };
    auto song = [&](MsSong bit) { return (s->songs & (1u << bit)) != 0; };
    const uint32_t strength = (s->ootUpgrades >> 6) & 7;
    const uint32_t scale = (s->ootUpgrades >> 9) & 7;
    // Ocarina of Time's masks and trade items, as MsHyruleItem numbers them
    static const char* kTradeIcons[MS_HYRULE_ITEM_COUNT] = {
        MS_OOT_ICON("gItemIconWeirdEggTex"),     MS_OOT_ICON("gItemIconChickenTex"),
        MS_OOT_ICON("gItemIconZeldasLetterTex"), MS_OOT_ICON("gItemIconMaskKeatonTex"),
        MS_OOT_ICON("gItemIconMaskSkullTex"),    MS_OOT_ICON("gItemIconMaskSpookyTex"),
        MS_OOT_ICON("gItemIconMaskBunnyHoodTex"), MS_OOT_ICON("gItemIconMaskGoronTex"),
        MS_OOT_ICON("gItemIconMaskZoraTex"),     MS_OOT_ICON("gItemIconMaskGerudoTex"),
        MS_OOT_ICON("gItemIconMaskTruthTex"),    MS_OOT_ICON("gItemIconPocketEggTex"),
        MS_OOT_ICON("gItemIconPocketCuccoTex"),  MS_OOT_ICON("gItemIconCojiroTex"),
        MS_OOT_ICON("gItemIconOddMushroomTex"),  MS_OOT_ICON("gItemIconOddPotionTex"),
        MS_OOT_ICON("gItemIconPoachersSawTex"),  MS_OOT_ICON("gItemIconBrokenGoronsSwordTex"),
        MS_OOT_ICON("gItemIconPrescriptionTex"), MS_OOT_ICON("gItemIconEyeballFrogTex"),
        MS_OOT_ICON("gItemIconEyeDropsTex"),     MS_OOT_ICON("gItemIconClaimCheckTex"),
    };
    static const char* kTradeNames[MS_HYRULE_ITEM_COUNT] = {
        "Weird Egg",    "Chicken",       "Zelda's Letter", "Keaton Mask",   "Skull Mask",    "Spooky Mask",
        "Bunny Hood",   "Goron Mask",    "Zora Mask",      "Gerudo Mask",   "Mask of Truth", "Pocket Egg",
        "Pocket Cucco", "Cojiro",        "Odd Mushroom",   "Odd Potion",    "Poacher's Saw", "Broken Goron's Sword",
        "Prescription", "Eyeball Frog",  "World's Finest Eye Drops", "Claim Check",
    };
    auto trade = [&](uint8_t held, MsHyruleItem fallback) -> HyruleCell {
        if (held < MS_HYRULE_ITEM_COUNT) {
            return { kTradeIcons[held], kTradeNames[held], true, white };
        }
        return { kTradeIcons[fallback], kTradeNames[fallback], false, white };
    };
    // the bottles: Hyrule's own four, or the combined set's first four
    const uint8_t* bottles = s->itemsCombined ? s->bottles : s->ootBottles;

    std::vector<HyruleCell> cells = {
        // the inventory, as Ocarina of Time's Select Item page orders it
        { MS_OOT_ICON("gItemIconDekuStickTex"), "Deku Stick", own(MS_OWN_STICK), white },
        { MS_OOT_ICON("gItemIconDekuNutTex"), "Deku Nut", own(MS_OWN_NUT), white },
        { MS_OOT_ICON("gItemIconBombTex"), "Bombs", own(MS_OWN_BOMB), white },
        { MS_OOT_ICON("gItemIconBowTex"), "Fairy Bow", item(MS_ITEM_BOW), white },
        { MS_OOT_ICON("gItemIconArrowFireTex"), "Fire Arrow", item(MS_ITEM_ARROW_FIRE), white },
        { MS_OOT_ICON("gItemIconDinsFireTex"), "Din's Fire", item(MS_ITEM_DINS_FIRE), white },
        { MS_OOT_ICON("gItemIconSlingshotTex"), "Fairy Slingshot", item(MS_ITEM_SLINGSHOT), white },
        { item(MS_ITEM_OCARINA_OF_TIME) ? MS_OOT_ICON("gItemIconOcarinaOfTimeTex") : MS_OOT_ICON("gItemIconOcarinaFairyTex"),
          item(MS_ITEM_OCARINA_OF_TIME) ? "Ocarina of Time" : "Fairy Ocarina",
          item(MS_ITEM_OCARINA_OF_TIME) || own(MS_OWN_OCARINA), white },
        { MS_OOT_ICON("gItemIconBombchuTex"), "Bombchus", own(MS_OWN_BOMBCHU), white },
        { item(MS_ITEM_LONGSHOT) ? MS_OOT_ICON("gItemIconLongshotTex") : MS_OOT_ICON("gItemIconHookshotTex"),
          item(MS_ITEM_LONGSHOT) ? "Longshot" : "Hookshot", item(MS_ITEM_LONGSHOT) || item(MS_ITEM_OOT_HOOKSHOT), white },
        { MS_OOT_ICON("gItemIconArrowIceTex"), "Ice Arrow", item(MS_ITEM_ARROW_ICE), white },
        { MS_OOT_ICON("gItemIconFaroresWindTex"), "Farore's Wind", item(MS_ITEM_FARORES_WIND), white },
        { MS_OOT_ICON("gItemIconBoomerangTex"), "Boomerang", item(MS_ITEM_BOOMERANG), white },
        { MS_OOT_ICON("gItemIconLensOfTruthTex"), "Lens of Truth", item(MS_ITEM_LENS_OF_TRUTH), white },
        { MS_OOT_ICON("gItemIconMagicBeanTex"), "Magic Beans", own(MS_OWN_BEAN), white },
        { MS_OOT_ICON("gItemIconHammerTex"), "Megaton Hammer", item(MS_ITEM_MEGATON_HAMMER), white },
        { MS_OOT_ICON("gItemIconArrowLightTex"), "Light Arrow", item(MS_ITEM_ARROW_LIGHT), white },
        { MS_OOT_ICON("gItemIconNayrusLoveTex"), "Nayru's Love", item(MS_ITEM_NAYRUS_LOVE), white },
    };
    for (int i = 0; i < 4; i++) {
        cells.push_back({ MS_OOT_ICON("gItemIconBottleEmptyTex"), "Bottle", bottles[i] != MS_BOTTLE_NONE, white });
    }
    cells.push_back(trade(s->hyruleTradeAdult, MS_HYRULE_POCKET_EGG));
    cells.push_back(trade(s->hyruleTradeChild, MS_HYRULE_WEIRD_EGG));
    const std::vector<HyruleCell> rest = {
        // the equipment (inventory.equipment: swords from bit 0, shields 4, tunics 8, boots 12), in the tracker's order
        { MS_OOT_ICON("gItemIconSwordKokiriTex"), "Kokiri Sword", equip(0, 0), white },
        { MS_OOT_ICON("gItemIconSwordMasterTex"), "Master Sword", equip(0, 1), white },
        { MS_OOT_ICON("gItemIconSwordBiggoronTex"), "Biggoron's Sword", equip(0, 2) || equip(0, 3), white },
        { MS_OOT_ICON("gItemIconTunicKokiriTex"), "Kokiri Tunic", equip(8, 0), white },
        { MS_OOT_ICON("gItemIconTunicGoronTex"), "Goron Tunic", equip(8, 1), white },
        { MS_OOT_ICON("gItemIconTunicZoraTex"), "Zora Tunic", equip(8, 2), white },
        { MS_OOT_ICON("gItemIconShieldDekuTex"), "Deku Shield", equip(4, 0), white },
        { MS_OOT_ICON("gItemIconShieldHylianTex"), "Hylian Shield", equip(4, 1), white },
        { MS_OOT_ICON("gItemIconShieldMirrorTex"), "Mirror Shield", equip(4, 2), white },
        { MS_OOT_ICON("gItemIconBootsKokiriTex"), "Kokiri Boots", equip(12, 0), white },
        { MS_OOT_ICON("gItemIconBootsIronTex"), "Iron Boots", equip(12, 1), white },
        { MS_OOT_ICON("gItemIconBootsHoverTex"), "Hover Boots", equip(12, 2), white },
        // the misc items (inventory.upgrades: strength from bit 6, scale from 9)
        { (strength >= 3)   ? MS_OOT_ICON("gItemIconGoldenGauntletsTex")
          : (strength == 2) ? MS_OOT_ICON("gItemIconSilverGauntletsTex")
                            : MS_OOT_ICON("gItemIconGoronsBraceletTex"),
          (strength >= 3) ? "Golden Gauntlets" : (strength == 2) ? "Silver Gauntlets" : "Goron's Bracelet", strength > 0,
          white },
        { (scale >= 2) ? MS_OOT_ICON("gItemIconScaleGoldenTex") : MS_OOT_ICON("gItemIconScaleSilverTex"),
          (scale >= 2) ? "Golden Scale" : "Silver Scale", scale > 0, white },
        { (s->walletLevel >= 2) ? MS_OOT_ICON("gItemIconGiantsWalletTex") : MS_OOT_ICON("gItemIconAdultsWalletTex"),
          (s->walletLevel >= 2) ? "Giant's Wallet" : "Adult's Wallet", s->walletLevel > 0, white },
        { MS_OOT_QUEST_ICON("gQuestIconHeartContainerTex"), "Heart Containers", s->healthCapacity > 0x30, white },
        { MS_OOT_QUEST_ICON("gQuestIconHeartPieceTex"), "Pieces of Heart", s->heartPieces > 0, white },
        { (s->magicLevel >= 2) ? MS_OOT_QUEST_ICON("gQuestIconMagicJarBigTex") : MS_OOT_QUEST_ICON("gQuestIconMagicJarSmallTex"),
          "Magic", s->magicLevel > 0, white },
        { MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_GERUDO_CARD), "Gerudo's Card",
          quest(MERGESHIP_OOT_QUEST_GERUDO_CARD), white },
        { MS_OOT_QUEST_ICON("gQuestIconGoldSkulltulaTex"), "Gold Skulltula Tokens", s->gsTokens > 0, white },
        { MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_STONE_OF_AGONY), "Stone of Agony",
          quest(MERGESHIP_OOT_QUEST_STONE_OF_AGONY), white },
    };
    cells.insert(cells.end(), rest.begin(), rest.end());
    // the dungeon rewards: the Spiritual Stones (finishing the misc items' row), then the medallions
    static const char* kStoneNames[] = { "Kokiri's Emerald", "Goron's Ruby", "Zora's Sapphire" };
    for (int i = 0; i < 3; i++) {
        cells.push_back({ MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_KOKIRI_EMERALD + i), kStoneNames[i],
                          quest(MERGESHIP_OOT_QUEST_KOKIRI_EMERALD + i), white });
    }
    static const char* kQuestNames[] = { "Forest Medallion", "Fire Medallion", "Water Medallion", "Spirit Medallion",
                                         "Shadow Medallion", "Light Medallion" };
    for (int i = 0; i < 6; i++) {
        cells.push_back({ MergeShip_Hyrule_QuestIconTexture(MERGESHIP_OOT_QUEST_MEDALLION_FOREST + i), kQuestNames[i],
                          quest(MERGESHIP_OOT_QUEST_MEDALLION_FOREST + i), white });
    }
    // the songs: Ocarina of Time's note, in its pause screen's colours for the warp songs
    struct SongCell {
        MsSong bit;
        const char* name;
        ImVec4 tint;
    };
    const SongCell kSongs[] = {
        { MS_SONG_ZELDAS_LULLABY, "Zelda's Lullaby", white },  { MS_SONG_EPONA, "Epona's Song", white },
        { MS_SONG_SARIA, "Saria's Song", white },               { MS_SONG_SUN, "Sun's Song", white },
        { MS_SONG_TIME, "Song of Time", white },                { MS_SONG_STORMS, "Song of Storms", white },
        { MS_SONG_MINUET, "Minuet of Forest", ImVec4(150 / 255.0f, 1.0f, 100 / 255.0f, 1) },
        { MS_SONG_BOLERO, "Bolero of Fire", ImVec4(1.0f, 80 / 255.0f, 40 / 255.0f, 1) },
        { MS_SONG_SERENADE, "Serenade of Water", ImVec4(100 / 255.0f, 150 / 255.0f, 1.0f, 1) },
        { MS_SONG_REQUIEM, "Requiem of Spirit", ImVec4(1.0f, 160 / 255.0f, 0, 1) },
        { MS_SONG_NOCTURNE, "Nocturne of Shadow", ImVec4(1.0f, 100 / 255.0f, 1.0f, 1) },
        { MS_SONG_PRELUDE, "Prelude of Light", ImVec4(1.0f, 240 / 255.0f, 100 / 255.0f, 1) },
    };
    for (const SongCell& sc : kSongs) {
        cells.push_back({ MS_OOT_ICON("gSongNoteTex"), sc.name, song(sc.bit), sc.tint });
    }
    return cells;
}

// BUG-0151: the Termina half, Ship of Harkinian's Termina column cell for cell (MergeShipUnifiedTracker.cpp
// MergeShip_ItemTracker_DrawTerminaColumn): MM's swords and shields, its other items, the trade items, the remains, the
// songs and the masks, owned as this file holds them.
std::vector<HyruleCell> TerminaCells() {
    const ImVec4 white(1, 1, 1, 1);
    struct TerminaIcon {
        u8 item; // ITEM_NONE: an empty cell, so that each group starts a row
        const char* name;
        ImVec4 tint;
    };
    static const TerminaIcon kIcons[] = {
        { ITEM_SWORD_KOKIRI, "Kokiri Sword (MM)", white },
        { ITEM_SWORD_RAZOR, "Razor Sword", white },
        { ITEM_SWORD_GILDED, "Gilded Sword", white },
        { ITEM_SWORD_GREAT_FAIRY, "Great Fairy's Sword", white },
        { ITEM_SHIELD_HERO, "Hero's Shield", white },
        { ITEM_SHIELD_MIRROR, "Mirror Shield (MM)", white },
        { ITEM_BOW, "Hero's Bow", white },
        { ITEM_HOOKSHOT, "Hookshot (MM)", white },
        { ITEM_POWDER_KEG, "Powder Keg", white },
        { ITEM_PICTOGRAPH_BOX, "Pictograph Box", white },
        { ITEM_BOMBERS_NOTEBOOK, "Bombers' Notebook", white },
        { ITEM_MOONS_TEAR, "Moon's Tear", white },
        { ITEM_DEED_LAND, "Land Title Deed", white },
        { ITEM_DEED_SWAMP, "Swamp Title Deed", white },
        { ITEM_DEED_MOUNTAIN, "Mountain Title Deed", white },
        { ITEM_DEED_OCEAN, "Ocean Title Deed", white },
        { ITEM_ROOM_KEY, "Room Key", white },
        { ITEM_LETTER_TO_KAFEI, "Letter to Kafei", white },
        { ITEM_LETTER_MAMA, "Express Mail to Mama", white },
        { ITEM_PENDANT_OF_MEMORIES, "Pendant of Memories", white },
        { ITEM_REMAINS_ODOLWA, "Odolwa's Remains", white },
        { ITEM_REMAINS_GOHT, "Goht's Remains", white },
        { ITEM_REMAINS_GYORG, "Gyorg's Remains", white },
        { ITEM_REMAINS_TWINMOLD, "Twinmold's Remains", white },
        { ITEM_SONG_SONATA, "Sonata of Awakening", ImVec4(150 / 255.0f, 1.0f, 100 / 255.0f, 1) },
        { ITEM_SONG_LULLABY, "Goron's Lullaby", ImVec4(1.0f, 80 / 255.0f, 40 / 255.0f, 1) },
        { ITEM_SONG_NOVA, "New Wave Bossa Nova", ImVec4(100 / 255.0f, 150 / 255.0f, 1.0f, 1) },
        { ITEM_SONG_ELEGY, "Elegy of Emptiness", ImVec4(1.0f, 160 / 255.0f, 0, 1) },
        { ITEM_SONG_OATH, "Oath to Order", ImVec4(1.0f, 100 / 255.0f, 1.0f, 1) },
        { ITEM_SONG_HEALING, "Song of Healing", white },
        { ITEM_SONG_SOARING, "Song of Soaring", white },
        { ITEM_NONE, "", white },
        { ITEM_NONE, "", white },
        { ITEM_NONE, "", white },
        { ITEM_NONE, "", white },
        { ITEM_NONE, "", white },
        { ITEM_MASK_DEKU, "Deku Mask", white },
        { ITEM_MASK_GORON, "Goron Mask (MM)", white },
        { ITEM_MASK_ZORA, "Zora Mask (MM)", white },
        { ITEM_MASK_FIERCE_DEITY, "Fierce Deity's Mask", white },
        { ITEM_MASK_TRUTH, "Mask of Truth (MM)", white },
        { ITEM_MASK_KAFEIS_MASK, "Kafei's Mask", white },
        { ITEM_MASK_ALL_NIGHT, "All-Night Mask", white },
        { ITEM_MASK_BUNNY, "Bunny Hood (MM)", white },
        { ITEM_MASK_KEATON, "Keaton Mask (MM)", white },
        { ITEM_MASK_GARO, "Garo's Mask", white },
        { ITEM_MASK_ROMANI, "Romani's Mask", white },
        { ITEM_MASK_CIRCUS_LEADER, "Circus Leader's Mask", white },
        { ITEM_MASK_POSTMAN, "Postman's Hat", white },
        { ITEM_MASK_COUPLE, "Couple's Mask", white },
        { ITEM_MASK_GREAT_FAIRY, "Great Fairy's Mask", white },
        { ITEM_MASK_GIBDO, "Gibdo Mask", white },
        { ITEM_MASK_DON_GERO, "Don Gero's Mask", white },
        { ITEM_MASK_KAMARO, "Kamaro's Mask", white },
        { ITEM_MASK_CAPTAIN, "Captain's Hat", white },
        { ITEM_MASK_STONE, "Stone Mask", white },
        { ITEM_MASK_BREMEN, "Bremen Mask", white },
        { ITEM_MASK_BLAST, "Blast Mask", white },
        { ITEM_MASK_SCENTS, "Mask of Scents", white },
        { ITEM_MASK_GIANT, "Giant's Mask", white },
    };
    auto owns = [](u8 item) -> bool {
        switch (item) {
            case ITEM_SWORD_KOKIRI:
            case ITEM_SWORD_RAZOR:
            case ITEM_SWORD_GILDED:
            case ITEM_SWORD_GREAT_FAIRY:
            case ITEM_SHIELD_HERO:
            case ITEM_SHIELD_MIRROR:
                return MergeShip_TerminaEquip_Owns(item);
            case ITEM_BOMBERS_NOTEBOOK:
                return CHECK_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK);
            case ITEM_MOONS_TEAR:
                return IsTradeItemObtained(RI_MOONS_TEAR);
            case ITEM_DEED_LAND:
                return IsTradeItemObtained(RI_DEED_LAND);
            case ITEM_DEED_SWAMP:
                return IsTradeItemObtained(RI_DEED_SWAMP);
            case ITEM_DEED_MOUNTAIN:
                return IsTradeItemObtained(RI_DEED_MOUNTAIN);
            case ITEM_DEED_OCEAN:
                return IsTradeItemObtained(RI_DEED_OCEAN);
            case ITEM_ROOM_KEY:
                return IsTradeItemObtained(RI_ROOM_KEY);
            case ITEM_LETTER_TO_KAFEI:
                return IsTradeItemObtained(RI_LETTER_TO_KAFEI);
            case ITEM_LETTER_MAMA:
                return IsTradeItemObtained(RI_LETTER_TO_MAMA);
            case ITEM_PENDANT_OF_MEMORIES:
                return IsTradeItemObtained(RI_PENDANT_OF_MEMORIES);
            case ITEM_REMAINS_ODOLWA:
            case ITEM_REMAINS_GOHT:
            case ITEM_REMAINS_GYORG:
            case ITEM_REMAINS_TWINMOLD:
                return CHECK_QUEST_ITEM(QUEST_REMAINS_ODOLWA + (item - ITEM_REMAINS_ODOLWA));
            case ITEM_SONG_SONATA:
            case ITEM_SONG_LULLABY:
            case ITEM_SONG_NOVA:
            case ITEM_SONG_ELEGY:
            case ITEM_SONG_OATH:
            case ITEM_SONG_HEALING:
            case ITEM_SONG_SOARING:
                return CHECK_QUEST_ITEM(QUEST_SONG_SONATA + (item - ITEM_SONG_SONATA));
            default:
                return MergeShip_Own_MMOwns(item);
        }
    };
    const bool isSaveLoaded = (gPlayState != NULL) && (gSaveContext.gameMode == GAMEMODE_NORMAL);
    std::vector<HyruleCell> cells;
    for (const TerminaIcon& icon : kIcons) {
        if (icon.item == ITEM_NONE) {
            cells.push_back({ nullptr, "", false, white });
            continue;
        }
        cells.push_back({ (const char*)gItemIcons[icon.item], icon.name, isSaveLoaded && owns(icon.item), icon.tint });
    }
    return cells;
}

bool MergeShip_ItemTracker_BothWorlds() {
    return MergeShipHost_IsHosted() && CVarGetInteger("gEnhancements.MergeShip.ItemTrackerBothWorlds", 1) &&
           (MergeShipHost_SharedSave() != nullptr);
}

void MergeShip_ItemTracker_DrawGrid(const std::vector<HyruleCell>& cells) {
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return;
    }
    const float scale = CVarGetFloat("gSettings.ItemTracker.Scale", 1.0f);
    const ImVec2 cellSize(ITEM_TEXTURE_SIZE * scale, ITEM_TEXTURE_SIZE * scale);

    // BUG-0149/0156: a fixed grid of cells, not a table: a table spans the window's whole width, so whatever stood beside
    // it pushed the window wider every frame, into a grey bar across the screen.
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    s32 index = 0;
    for (const HyruleCell& cell : cells) {
        if ((index++ % 6) != 0) {
            ImGui::SameLine();
        }
        ImGui::Dummy(cellSize);
        if ((cell.texture == nullptr) || (cell.texture[0] == '\0')) {
            continue;
        }
        Ship_LoadGuiTextureOnce(cell.texture);
        ImVec4 color = cell.tint;
        color.w = cell.owned ? 1.0f : 0.3f;
        const ImVec2 p0 = ImGui::GetItemRectMin();
        // (Ocarina of Time's icons are 32 pixels, its quest icons 24: each fills the cell's height)
        ImGui::GetWindowDrawList()->AddImage(gui->GetTextureByName(cell.texture), p0, p0 + cellSize, ImVec2(0, 0),
                                             ImVec2(1, 1), ImGui::GetColorU32(color));
        UIWidgets::Tooltip(cell.name);
    }
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
}

// BUG-0151: the one tracker both games draw -- Hyrule's half, then Termina's beside it, as Ship of Harkinian draws them.
void MergeShip_ItemTracker_DrawBothWorlds() {
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());
    if (shared == nullptr) {
        return;
    }
    const float scale = CVarGetFloat("gSettings.ItemTracker.Scale", 1.0f);
    MergeShip_ItemTracker_DrawGrid(HyruleCells(shared));
    ImGui::SameLine(0.0f, ITEM_TEXTURE_SIZE * scale * 0.5f);
    MergeShip_ItemTracker_DrawGrid(TerminaCells());
}

} // namespace

void ItemTrackerWindow::Draw() {
    if (!IsVisible()) {
        return;
    }

    if (CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_ONLY_ON_PAUSE_MENU &&
        (!gPlayState || !gPlayState->pauseCtx.state)) {
        return;
    }

    if ((CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_BUTTON_TOGGLE ||
         CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_BUTTON_HOLD) &&
        !sItemTrackerBtnState) {
        return;
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;

    if (!CVarGetInteger("gSettings.ItemTracker.WindowType", 0)) {
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking;
    }
    bool shouldWindowSplit = CVarGetInteger("gSettings.ItemTracker.WindowGroup", 0);

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    // Adjust opacity
    ImVec4 windowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    windowBg.w = CVarGetFloat("gSettings.ItemTracker.Opacity", 0.5f);

    bool singleWindowOpen = false;
    if (!shouldWindowSplit) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBg);
        singleWindowOpen = ImGui::Begin("Item Tracker", nullptr, windowFlags);
    }

    // MergeShip2 (BUG-0131/0151): with both worlds' items, the one tracker Ship of Harkinian draws too -- Hyrule's half
    // and Termina's -- in place of Termina's own groups.
    const bool msBothWorlds = !shouldWindowSplit && singleWindowOpen && MergeShip_ItemTracker_BothWorlds();
    if (msBothWorlds) {
        MergeShip_ItemTracker_DrawBothWorlds();
    }
    uint32_t index = 0;
    for (auto& group : (msBothWorlds ? std::vector<TrackerGroup>() : itemTrackerGroups)) {
        if (group.items.empty()) {
            index++;
            continue;
        }

        bool isWindowOpen = false;

        if (shouldWindowSplit) {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBg);
            std::string name = std::string(group.name) + "##" + std::to_string(index);
            isWindowOpen = ImGui::Begin(name.c_str(), nullptr, windowFlags);
        } else {
            isWindowOpen = singleWindowOpen;
        }

        if (isWindowOpen) {
            DrawItemTrackerGroup(group);
        }

        if (shouldWindowSplit) {
            ImGui::PopStyleColor(1);
            ImGui::End();
        }

        index++;
    }
    if (!shouldWindowSplit) {
        ImGui::PopStyleColor(1);
        ImGui::End();
    }

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);
}

void ItemTrackerWindow::InitElement() {
}

void ItemTrackerWindow::DrawElement() {
}

static RegisterShipInitFunc initFunc(
    []() {
        COND_HOOK(OnGameStateMainStart, CVAR_VISIBILITY_MODE >= ITEM_TRACKER_VISIBILITY_MODE_BUTTON_TOGGLE, []() {
            Input* input = CONTROLLER1(gGameState);

            if (CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_BUTTON_HOLD) {
                if (CHECK_BTN_ALL(input->cur.button, CVAR_VISIBILITY_BTN)) {
                    sItemTrackerBtnState = true;
                } else {
                    sItemTrackerBtnState = false;
                }
            } else {
                if (CHECK_BTN_ALL(input->cur.button, CVAR_VISIBILITY_BTN) &&
                    CHECK_BTN_ANY(input->press.button, CVAR_VISIBILITY_BTN)) {
                    sItemTrackerBtnState = !sItemTrackerBtnState;
                }
            }
        });
    },
    { CVAR_NAME_VISIBILITY_MODE });
