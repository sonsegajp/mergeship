#pragma once

#include <imgui.h>
#include <array>
#include <map>
#include <string>
#include "variables.h" //only for gItemIcons

const char* GetTextureForItemId(uint32_t itemId);
// MergeShip2 (BUG-0090): the notification icon of any item id -- Ocarina of Time's (the item map) or Majora's Mask's
// (gItemIcons, mm.o2r), registered the first time it is asked for. nullptr when the item has none.
const char* MergeShip_NotificationIcon(uint32_t itemId);
void RegisterImGuiItemIcons();

typedef struct {
    uint32_t id;
    std::string name;
    std::string nameFaded;
    std::string texturePath;
} ItemMapEntry;

#define ITEM_MAP_ENTRY(id)                                            \
    {                                                                 \
        id, {                                                         \
            id, #id, #id "_Faded", static_cast<char*>(gItemIcons[id]) \
        }                                                             \
    }

// Maps items ids to info for use in ImGui
extern std::map<uint32_t, ItemMapEntry> itemMapping;

// MergeShip2: Majora's Mask's transformation masks by RandomizerGet. Their icons come from mm.o2r, so they are only
// loaded while it is mounted (MergeShip_IsMajorasMaskMounted).
extern std::map<uint32_t, ItemMapEntry> transformationMaskMapping;

typedef struct {
    uint32_t id;
    std::string name;
    std::string nameFaded;
    std::string texturePath;
} QuestMapEntry;

#define QUEST_MAP_ENTRY(id, tex)       \
    {                                  \
        id, {                          \
            id, #id, #id "_Faded", tex \
        }                              \
    }

// Maps quest items ids to info for use in ImGui
extern std::map<uint32_t, QuestMapEntry> questMapping;

typedef struct {
    uint32_t id;
    std::string name;
    std::string nameFaded;
    ImVec4 color;
} SongMapEntry;

#define SONG_MAP_ENTRY(id, r, g, b)                                                 \
    {                                                                               \
        id, {                                                                       \
            id, #id, #id "_Faded", ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f) \
        }                                                                           \
    }

// Maps song ids to info for use in ImGui
extern std::map<QuestItem, SongMapEntry> songMapping;

#define VANILLA_SONG_MAP_ENTRY(id, r, g, b) \
    { id, #id "_Vanilla", #id "_Vanilla_Faded", ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f) }

// Maps song ids to info for use in ImGui
extern std::array<SongMapEntry, 12> vanillaSongMapping;
