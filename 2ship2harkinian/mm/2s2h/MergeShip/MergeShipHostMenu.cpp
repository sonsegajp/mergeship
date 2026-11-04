// MergeShip2 -- 2 Ship 2 Harkinian's "Mergeship" menu, "Termina" page: the door between the engines (mergeship.exe
// only; MergeShipHost.h). The option is the same CVar SoH's Mergeship menu sets for its end of the door
// (EngineApi.h MS_CVAR_TERMINA_DOOR), stored in each game's own config.

// Only in 2ship.dll (MERGESHIP_HOSTED): the option does nothing in 2ship.exe.
#ifdef MERGESHIP_HOSTED

#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/MenuTypes.h"

#include <mergeship/EngineApi.h>
#include <mergeship/AnchorNet.h>
#include <mergeship/MenuMirror.hpp> // BUG-0096: Hyrule's settings in this menu

#include <algorithm>

// EngineExport.cpp: SoH's menu as plain data and its CVars (NULL before SoH has started).
const MsMenuMirror* MergeShip_OtherMenuMirror();
Ship::ConsoleVariable* MergeShip_OtherConsoleVariables();

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
using namespace UIWidgets;

static void RegisterMergeShipHostMenu() {
    mBenMenu->AddMenuEntry("Mergeship", "gSettings.Menu.MergeShipSidebarSection");
    WidgetPath path = { "Mergeship", "Termina", SECTION_COLUMN_1 };
    mBenMenu->AddSidebarEntry("Mergeship", path.sidebarName, 1);
    mBenMenu->AddWidget(path, "The Door to Hyrule", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path, "Clock Tower Leads to Hyrule", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_TERMINA_DOOR)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In MergeShip2 (mergeship.exe), a player who came from Hyrule's Lost Woods goes back the way Link came "
            "in: the stone doors behind the Happy Mask Salesman's room in the Clock Tower stand open, and the passage "
            "beyond leads to Hyrule's Lost Woods. What Link carries goes with him. Has no effect in 2ship.exe."));
    mBenMenu->AddWidget(path, "Share Items During Termina's First Cycle", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_SHARE_FIRST_CYCLE)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Link arrives in Termina with everything he has in Hyrule, even during Majora's Mask's first cycle "
            "(before its first Song of Time), and what he gets there goes back with him. Majora's Mask's opening "
            "still plays: its Ocarina of Time, the Song of Time, the Song of Healing and the transformation masks "
            "wait until the Happy Mask Salesman has healed Link, so the first cycle plays as written. Off, nothing "
            "is shared until the first cycle is over. Hyrule's Mergeship menu has the same option; either one "
            "turns it off. Has no effect in 2ship.exe."));
    // Settings sharing (MergeShipSettingsSync.h): the switch is SoH's; this is a note.
    mBenMenu->AddWidget(path, "Settings From Hyrule", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path,
                        "In MergeShip2, the options set in Ship of Harkinian (frame rate, V-Sync, resolution, volumes, "
                        "D-pad items and boots, D-pad ocarina, aiming and camera, time savers, cheats, colours...) "
                        "apply here too, each time Link comes to Termina. Changing one of them here changes it in "
                        "Hyrule as well (the few with no exact Hyrule counterpart keep what you set here until the "
                        "Hyrule option is changed). Turn it off with \"Share Settings with Termina\" in Ship of "
                        "Harkinian's Mergeship menu.",
                        WIDGET_TEXT);
    // Anchor (Network menu): one connection for both worlds (include/mergeship/AnchorNet.h). The same options are on
    // the Anchor page; these are the Mergeship menu's copies.
    mBenMenu->AddWidget(path, "Anchor Across the Worlds", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path, "Anchor: Share the Connection Between Worlds", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_ANCHOR_SHARED_CONNECTION)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Termina and Hyrule share one Anchor connection: going through the door never leaves the room, and the "
            "world you are not in keeps receiving its items and flags. Takes effect the next time Anchor is "
            "enabled."));
    mBenMenu->AddWidget(path, "Anchor: Items From the Other World", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "An item a teammate finds in Hyrule reaches you in Termina (and the other way round), through the shared "
            "inventory both worlds keep. Items only one world can hold wait for you there."));
    mBenMenu->AddWidget(path, "Skip the First Dawn Card", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.SkipFirstDawnCard")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The first trip to Termina starts the First Day without the \"Dawn of the First Day\" card."));
    mBenMenu->AddWidget(path, "Item Tracker: Both Worlds' Items", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.ItemTrackerBothWorlds")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Item Tracker shows Hyrule's items beside Termina's: Ocarina of Time's items, equipment, upgrades, "
            "medallions, Spiritual Stones, songs and masks, as Hyrule's file holds them. Hyrule's tracker shows "
            "Termina's beside Hyrule's the same way."));
    mBenMenu->AddWidget(path, "Sprint (Hold A Out of a Roll)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.Sprint")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Young Link and adult Link sprint when A is still held as a roll ends, for as long as A and the stick "
            "stay held: faster, leaning forward, arms pumping. Jumps keep the sprint's speed and it goes on after "
            "landing; a Bunny Hood adds its 1.5x on top. Hyrule and Termina share it."));
    mBenMenu->AddWidget(path, "Sprint Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gEnhancements.MergeShip.SprintSpeed")
        .Options(FloatSliderOptions().Min(1.1f).Max(2.0f).DefaultValue(1.5f).Format("%.2fx").Tooltip(
            "The sprint's speed, as a multiple of the run's."));
    mBenMenu->AddWidget(path, "Hyrule's HUD Layout", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.HyruleHudLayout")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Termina's HUD sits where Hyrule's does: the hearts, magic and counters against the screen's left edge, the "
            "buttons and the minimap against its right, as Ship of Harkinian places them, instead of Majora's Mask's "
            "4:3 frame in the middle. Elements given a mode of their own in the HUD Editor keep it. Hyrule's colour "
            "scheme (Cosmetics Editor, GameCube) carries over to the buttons too."));
    mBenMenu->AddWidget(path, "Ocarina Items Glitch", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.OcarinaItems")
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Ocarina of Time's \"Ocarina Items\" trick in Termina: with a bug or fish in a bottle, backflip, press "
            "the bottle's button and then B (the sword or a Deku Stick) before landing, and Link takes out the ocarina "
            "instead. Majora's Mask fixed it; this brings Ocarina of Time's behaviour back."));
    // The unified randomizer's hints and check tracker over both worlds (Rando/MergeShip/UnifiedHyrule.h)
    mBenMenu->AddWidget(path, "Unified Randomizer", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path, "Hints Name Hyrule's Checks", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.HintHyrule")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In a seed over Hyrule and Termina, Termina's hints also point into Hyrule: Gossip Stones may name an "
            "item on one of Hyrule's checks (\"in Hyrule's Kokiri Forest\"), and the hints about the Oath to Order, "
            "the Song of Soaring, the Hookshot, the remains and the transformation masks name Hyrule's check when "
            "Hyrule holds the item."));
    mBenMenu->AddWidget(path, "Check Tracker Shows Hyrule", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.TrackHyrule")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In a seed over Hyrule and Termina, the Check Tracker lists Hyrule's checks after Termina's, by Hyrule's "
            "areas. A check found in Hyrule shows as found here, with the item it held."));

    // BUG-0096: Hyrule's own settings, set from here (include/mergeship/MenuMirror.hpp). The ones both worlds share
    // are not repeated: they are this menu's own.
    path.sidebarName = "Hyrule Settings";
    mBenMenu->AddSidebarEntry("Mergeship", path.sidebarName, 1);
    path.column = SECTION_COLUMN_1;
    mBenMenu->AddWidget(path, "Hyrule Settings##MergeShipMirror", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        MsMenuMirror_Draw(MergeShip_OtherMenuMirror(), MergeShip_OtherConsoleVariables(), false, "Hyrule");
    });
}

// BUG-0096: this menu as plain data for Hyrule's menu ("Termina Settings"): every CVar checkbox, slider and list, with
// the separators' titles; buttons, windows and custom widgets are left out (they act in Termina).
const MsMenuMirror* MergeShip_BuildMenuMirror() {
    static MsMenuMirrorBuilder sMirror;
    static const MsMenuMirror* sBuilt = nullptr;

    if ((sBuilt != nullptr) || (mBenMenu == nullptr)) {
        return sBuilt;
    }
    const auto& entries = mBenMenu->GetMenuEntries();
    for (const std::string& menuName : mBenMenu->GetMenuOrder()) {
        auto menuIt = entries.find(menuName);
        if ((menuIt == entries.end()) || (menuName == "Dev Tools")) {
            continue;
        }
        const MainMenuEntry& menu = menuIt->second;
        for (const std::string& sidebarName : menu.sidebarOrder) {
            auto sidebarIt = menu.sidebars.find(sidebarName);
            if (sidebarIt == menu.sidebars.end()) {
                continue;
            }
            for (const auto& column : sidebarIt->second.columnWidgets) {
                for (const WidgetInfo& widget : column) {
                    const char* tip = (widget.options != nullptr) ? widget.options->tooltip : nullptr;
                    const std::string tooltip = (tip != nullptr) ? tip : "";
                    if (widget.type == WIDGET_SEPARATOR_TEXT) {
                        sMirror.Add(menu.label, sidebarName, widget.name, nullptr, "", MS_MIRROR_HEADER);
                        continue;
                    }
                    if ((widget.cVar == nullptr) || (widget.options == nullptr)) {
                        continue;
                    }
                    switch (widget.type) {
                        case WIDGET_CVAR_CHECKBOX: {
                            auto* options = static_cast<CheckboxOptions*>(widget.options.get());
                            MsMenuMirrorItem& item = sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar,
                                                                 tooltip, MS_MIRROR_CHECKBOX);
                            item.def = options->defaultValue ? 1.0f : 0.0f;
                            break;
                        }
                        case WIDGET_CVAR_SLIDER_INT: {
                            auto* options = static_cast<IntSliderOptions*>(widget.options.get());
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_INT);
                            item.min = (float)options->min;
                            item.max = (float)options->max;
                            item.def = (float)options->defaultValue;
                            item.format = sMirror.Keep((options->format != nullptr) ? options->format : "");
                            break;
                        }
                        case WIDGET_CVAR_SLIDER_FLOAT: {
                            auto* options = static_cast<FloatSliderOptions*>(widget.options.get());
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_FLOAT);
                            item.min = options->min;
                            item.max = options->max;
                            item.def = options->defaultValue;
                            item.percent = options->isPercentage;
                            item.format = sMirror.Keep((options->format != nullptr) ? options->format : "");
                            break;
                        }
                        case WIDGET_CVAR_COMBOBOX: {
                            auto* options = static_cast<ComboboxOptions*>(widget.options.get());
                            std::vector<std::pair<int32_t, std::string>> list;
                            if (std::holds_alternative<ComboMap_t>(options->comboVariant)) {
                                ComboMap_t map = std::get<ComboMap_t>(options->comboVariant);
                                if (map != nullptr) {
                                    for (const auto& [value, label] : *map) {
                                        list.emplace_back(value, (label != nullptr) ? label : "");
                                    }
                                    std::sort(list.begin(), list.end(),
                                              [](const auto& a, const auto& b) { return a.first < b.first; });
                                }
                            } else {
                                ComboVec_t vec = std::get<ComboVec_t>(options->comboVariant);
                                if (vec != nullptr) {
                                    for (size_t i = 0; i < vec->size(); i++) {
                                        list.emplace_back((int32_t)i, ((*vec)[i] != nullptr) ? (*vec)[i] : "");
                                    }
                                }
                            }
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_COMBO);
                            item.def = (float)options->defaultIndex;
                            sMirror.SetOptions(item, list);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
    }
    sBuilt = sMirror.Finish();
    return sBuilt;
}

static RegisterMenuInitFunc sMergeShipHostMenuInit(RegisterMergeShipHostMenu);

} // namespace BenGui

#endif // MERGESHIP_HOSTED
