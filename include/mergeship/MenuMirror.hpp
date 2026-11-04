/*
 * MergeShip2 -- one settings menu for both games (BUG-0096). Each game's menu shows the other game's settings, so the
 * player never has to cross the door to change one:
 *   - at its start, each engine walks its own menu (Ship of Harkinian's SohMenu, 2 Ship 2 Harkinian's BenMenu) into an
 *     MsMenuMirror -- plain data: every CVar checkbox, slider and list with its label, tooltip, range and default -- and
 *     hands it to the host (MsHostServices.SetMenuMirror);
 *   - the other game's Mergeship menu draws it (MsMenuMirror_Draw: "Termina Settings" in SoH, "Hyrule Settings" in
 *     2Ship), bound to the other game's CVars (its Ship::Context, MsHostServices.GetEngineContext): a change is written
 *     into them and saved to that game's config at once;
 *   - the settings both games have (include/mergeship/SettingsMap.def) are left out: they are already one setting,
 *     shared by the settings sync (SettingsSync.h), and each game's own menu sets them;
 *   - what a game runs when one of its options changes (ShipInit, as its menu does) runs when that game resumes: the
 *     names wait in its CVar MS_MENU_MIRROR_DIRTY (MsMenuMirror_ApplyDirty).
 * Header-only: both engines include it (mergeship.exe only, MERGESHIP_HOSTED).
 */
#ifndef MERGESHIP_MENU_MIRROR_HPP
#define MERGESHIP_MENU_MIRROR_HPP

#include <stdint.h>

/* The plain data (C layout: it crosses between the engines through the host). */
typedef enum MsMenuMirrorKind {
    MS_MIRROR_HEADER,   /* a separator's title */
    MS_MIRROR_CHECKBOX, /* integer 0/1 */
    MS_MIRROR_INT,      /* integer slider */
    MS_MIRROR_FLOAT,    /* float slider */
    MS_MIRROR_COMBO,    /* integer from a list */
} MsMenuMirrorKind;

typedef struct MsMenuMirrorItem {
    const char* menu;    /* the top menu ("Enhancements") */
    const char* sidebar; /* its page ("Graphics") */
    const char* name;
    const char* cvar; /* NULL for a header */
    const char* tooltip;
    int32_t kind; /* MsMenuMirrorKind */
    float min;
    float max;
    float def;
    int32_t percent;    /* a float slider shown x100 */
    const char* format; /* the slider's printf format */
    int32_t optionCount;
    const int32_t* optionValues;
    const char* const* optionLabels;
} MsMenuMirrorItem;

typedef struct MsMenuMirror {
    int32_t count;
    const MsMenuMirrorItem* items;
} MsMenuMirror;

#ifdef __cplusplus

#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <ship/config/ConsoleVariable.h>

#include "SettingsSync.h"

/* A game's own CVar: the names of its options the other game's menu changed, ';'-separated, until it resumes. */
#define MS_MENU_MIRROR_DIRTY "gMergeShip.MenuMirror.Dirty"

/* Builds an MsMenuMirror and owns its strings and lists. */
class MsMenuMirrorBuilder {
  public:
    const char* Keep(const std::string& text) {
        mStrings.push_back(text);
        return mStrings.back().c_str();
    }
    MsMenuMirrorItem& Add(const std::string& menu, const std::string& sidebar, const std::string& name,
                          const char* cvar, const std::string& tooltip, MsMenuMirrorKind kind) {
        MsMenuMirrorItem item = {};
        item.menu = Keep(menu);
        item.sidebar = Keep(sidebar);
        item.name = Keep(name);
        item.cvar = (cvar != nullptr) ? Keep(cvar) : nullptr;
        item.tooltip = Keep(tooltip);
        item.kind = kind;
        item.format = "";
        mItems.push_back(item);
        return mItems.back();
    }
    void SetOptions(MsMenuMirrorItem& item, const std::vector<std::pair<int32_t, std::string>>& options) {
        mValues.emplace_back();
        mLabels.emplace_back();
        for (const auto& option : options) {
            mValues.back().push_back(option.first);
            mLabels.back().push_back(Keep(option.second));
        }
        item.optionCount = (int32_t)options.size();
        item.optionValues = mValues.back().data();
        item.optionLabels = mLabels.back().data();
    }
    const MsMenuMirror* Finish() {
        mMirror.count = (int32_t)mItems.size();
        mMirror.items = mItems.data();
        return &mMirror;
    }

  private:
    std::deque<std::string> mStrings;
    std::deque<std::vector<int32_t>> mValues;
    std::deque<std::vector<const char*>> mLabels;
    std::vector<MsMenuMirrorItem> mItems;
    MsMenuMirror mMirror = {};
};

/* True if `cvar` is one of the settings both games share (SettingsMap.def): `shipSide` names 2Ship's column. */
inline bool MsMenuMirror_IsShared(const char* cvar, bool shipSide) {
    static std::unordered_set<std::string> sSoh;
    static std::unordered_set<std::string> sShip;
    if (sSoh.empty()) {
        for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
            sSoh.insert(kMsSettingsMap[i].soh);
            sShip.insert(kMsSettingsMap[i].ship);
        }
    }
    return (shipSide ? sShip : sSoh).count(cvar) != 0;
}

inline void MsMenuMirror_MarkDirty(Ship::ConsoleVariable* cvars, const char* cvar) {
    std::string list = cvars->GetString(MS_MENU_MIRROR_DIRTY, "");
    const std::string entry = std::string(cvar) + ";";
    if (list.find(entry) == std::string::npos) {
        list += entry;
        cvars->SetString(MS_MENU_MIRROR_DIRTY, list.c_str());
    }
    cvars->Save();
}

/* Run in the resuming game (its own CVars): `init` for each option the other game's menu changed (ShipInit::Init). */
template <typename InitFunc> inline void MsMenuMirror_ApplyDirty(Ship::ConsoleVariable* own, InitFunc init) {
    const std::string list = own->GetString(MS_MENU_MIRROR_DIRTY, "");
    if (list.empty()) {
        return;
    }
    own->ClearVariable(MS_MENU_MIRROR_DIRTY);
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(';', start);
        if (end == std::string::npos) {
            end = list.size();
        }
        if (end > start) {
            init(list.substr(start, end - start));
        }
        start = end + 1;
    }
    own->Save();
}

inline bool MsMenuMirror_Matches(const MsMenuMirrorItem& item, const char* filter) {
    if ((filter == nullptr) || (filter[0] == '\0')) {
        return true;
    }
    auto contains = [filter](const char* text) {
        if (text == nullptr) {
            return false;
        }
        std::string a = text;
        std::string b = filter;
        for (auto& c : a) {
            c = (char)tolower((unsigned char)c);
        }
        for (auto& c : b) {
            c = (char)tolower((unsigned char)c);
        }
        return a.find(b) != std::string::npos;
    };
    return contains(item.name) || contains(item.tooltip) || contains(item.sidebar) || contains(item.menu);
}

/* The other game's settings: `mirror` (its menu) bound to `cvars` (its CVars). `otherIsShip`: the other game is 2Ship
 * (the shared settings are left out by its column of the map). `world` names it in the texts ("Termina"). */
inline void MsMenuMirror_Draw(const MsMenuMirror* mirror, Ship::ConsoleVariable* cvars, bool otherIsShip,
                              const char* world) {
    static char sFilter[64] = "";

    if ((mirror == nullptr) || (cvars == nullptr) || (mirror->count == 0)) {
        ImGui::TextWrapped("%s's settings are not loaded.", world);
        return;
    }
    ImGui::TextWrapped("%s's own settings, set from here: a change is saved to %s's settings at once and takes effect "
                       "there. The settings both worlds share (video, audio, controls, HUD colours and many "
                       "enhancements) are not listed: they are one setting, set in either menu.",
                       world, world);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##MsMirrorFilter", "Search", sFilter, sizeof(sFilter));
    const bool filtering = sFilter[0] != '\0';

    const char* groupMenu = nullptr;
    const char* groupSidebar = nullptr;
    bool open = false;
    for (int32_t i = 0; i < mirror->count; i++) {
        const MsMenuMirrorItem& item = mirror->items[i];

        if ((item.cvar != nullptr) && MsMenuMirror_IsShared(item.cvar, otherIsShip)) {
            continue;
        }
        if (filtering && ((item.kind == MS_MIRROR_HEADER) || !MsMenuMirror_Matches(item, sFilter))) {
            continue;
        }
        if ((groupMenu == nullptr) || (strcmp(groupMenu, item.menu) != 0) || (strcmp(groupSidebar, item.sidebar) != 0)) {
            groupMenu = item.menu;
            groupSidebar = item.sidebar;
            char title[160];
            snprintf(title, sizeof(title), "%s - %s###MsMirror%s%s", item.menu, item.sidebar, item.menu, item.sidebar);
            if (filtering) {
                ImGui::SetNextItemOpen(true);
            }
            open = ImGui::CollapsingHeader(title);
        }
        if (!open) {
            continue;
        }
        ImGui::PushID(i);
        bool changed = false;
        switch (item.kind) {
            case MS_MIRROR_HEADER:
                ImGui::SeparatorText(item.name);
                break;
            case MS_MIRROR_CHECKBOX: {
                bool value = cvars->GetInteger(item.cvar, (int32_t)item.def) != 0;
                if (ImGui::Checkbox(item.name, &value)) {
                    cvars->SetInteger(item.cvar, value ? 1 : 0);
                    changed = true;
                }
                break;
            }
            case MS_MIRROR_INT: {
                int value = cvars->GetInteger(item.cvar, (int32_t)item.def);
                ImGui::TextUnformatted(item.name);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::SliderInt("##value", &value, (int)item.min, (int)item.max,
                                     (item.format[0] != '\0') ? item.format : "%d", ImGuiSliderFlags_AlwaysClamp)) {
                    cvars->SetInteger(item.cvar, value);
                    changed = true;
                }
                break;
            }
            case MS_MIRROR_FLOAT: {
                const float scale = item.percent ? 100.0f : 1.0f;
                float value = cvars->GetFloat(item.cvar, item.def) * scale;
                ImGui::TextUnformatted(item.name);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::SliderFloat("##value", &value, item.min * scale, item.max * scale,
                                       item.percent ? "%.0f%%" : ((item.format[0] != '\0') ? item.format : "%.2f"),
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    cvars->SetFloat(item.cvar, value / scale);
                    changed = true;
                }
                break;
            }
            case MS_MIRROR_COMBO: {
                const int32_t value = cvars->GetInteger(item.cvar, (int32_t)item.def);
                const char* preview = "";
                for (int32_t o = 0; o < item.optionCount; o++) {
                    if (item.optionValues[o] == value) {
                        preview = item.optionLabels[o];
                    }
                }
                ImGui::TextUnformatted(item.name);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##value", preview)) {
                    for (int32_t o = 0; o < item.optionCount; o++) {
                        if (ImGui::Selectable(item.optionLabels[o], item.optionValues[o] == value)) {
                            cvars->SetInteger(item.cvar, item.optionValues[o]);
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            default:
                break;
        }
        if ((item.kind != MS_MIRROR_HEADER) && (item.tooltip != nullptr) && (item.tooltip[0] != '\0') &&
            ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", item.tooltip);
        }
        if (changed) {
            MsMenuMirror_MarkDirty(cvars, item.cvar);
        }
        ImGui::PopID();
    }
}

#endif /* __cplusplus */

#endif /* MERGESHIP_MENU_MIRROR_HPP */
