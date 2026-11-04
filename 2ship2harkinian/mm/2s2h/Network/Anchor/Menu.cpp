// MergeShip2 -- Anchor's menu in 2 Ship 2 Harkinian ("Network" > "Anchor"), ported from Ship of Harkinian's
// Network/Anchor/Menu.cpp. The same CVars as SoH's (gRemote.Anchor.*), so both games show the same room, name and
// team. In mergeship.exe the connection is shared with Hyrule: enabling or disabling it in either game does it for
// both.
#include <ship/Context.h>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/MenuTypes.h"
#include "2s2h/BenGui/UIWidgets.hpp"

#include <imgui.h>
#include <set>

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
}

namespace {

std::shared_ptr<AnchorRoomWindow> sAnchorRoomWindow;

const char* sPvpModes[3] = { "Off", "On", "On + Friendly Fire" };
const char* sTeleportModes[3] = { "None", "Team Only", "All" };
const char* sShowLocationsModes[3] = { "None", "Team Only", "All" };

bool IsEmpty(const std::string& text) {
    return text.find_first_not_of(" \t") == std::string::npos;
}

void SaveCVars() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void AnchorMainMenu(WidgetInfo& info) {
    if (Anchor::Instance == nullptr) {
        Anchor::Instance = new Anchor();
    }
    auto anchor = Anchor::Instance;

    std::string host = CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org");
    int32_t port = CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383);
    std::string anchorTeamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    std::string anchorRoomId = CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), "");
    std::string anchorName = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    bool isFormValid = !IsEmpty(host) && port > 1024 && port < 65535 && !IsEmpty(anchorRoomId) && !IsEmpty(anchorName);

    ImGui::SeparatorText("Connection Settings");

    ImGui::BeginDisabled(anchor->isEnabled);
    if (UIWidgets::InputString("Host", &host)) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Host"), host.c_str());
        SaveCVars();
    }
    if (UIWidgets::InputInt("Port", &port)) {
        CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), port);
        SaveCVars();
    }
    if (UIWidgets::InputString("Name", &anchorName)) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), anchorName.c_str());
        SaveCVars();
    }
    UIWidgets::CVarColorPicker("Color", CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100, 255 });
    if (UIWidgets::InputString("Room ID", &anchorRoomId, UIWidgets::InputOptions().IsSecret(anchor->isEnabled))) {
        CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), anchorRoomId.c_str());
        SaveCVars();
    }
    if (UIWidgets::InputString("Team ID (Items & Flags Shared)", &anchorTeamId)) {
        CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), anchorTeamId.c_str());
        SaveCVars();
    }
    ImGui::Spacing();

    if (UIWidgets::Button("Restore Defaults", UIWidgets::ButtonOptions().Color(UIWidgets::Colors::Red))) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org");
        CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383);
        CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
        CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), "");
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), "");
        SaveCVars();
    }
    if (UIWidgets::Button("Global Room",
                          UIWidgets::ButtonOptions()
                              .Color(UIWidgets::Colors::Indigo)
                              .Tooltip("Always-online public room so you don't have to experience Termina alone. PVP "
                                       "and syncing are disabled."))) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org");
        CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383);
        CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
        CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), "soh-global");
        SaveCVars();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();

    ImGui::BeginDisabled(!isFormValid);
    if (UIWidgets::Button(anchor->isEnabled ? "Disable" : "Enable",
                          UIWidgets::ButtonOptions().Color(anchor->isEnabled ? UIWidgets::Colors::Red
                                                                             : UIWidgets::Colors::Green))) {
        if (anchor->isEnabled) {
            CVarClear(CVAR_REMOTE_ANCHOR("Enabled"));
            SaveCVars();
            anchor->Disable();
        } else {
            CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 1);
            SaveCVars();
            anchor->Enable();
        }
    }
    ImGui::EndDisabled();
    ImGui::Spacing();

    if (!anchor->isEnabled) {
        return;
    }

    if (!anchor->isConnected) {
        ImGui::Text("Connecting...");
        return;
    }

    ImGui::SeparatorText("Current Room");
    ImGui::Text("Connected");
    // In mergeship.exe, the one connection Termina and Hyrule share
    std::string summary = MergeShipAnchor_ConnectionSummary();
    if (!summary.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 1, 0.6f), "%s", summary.c_str());
    }

    if (UIWidgets::Button("Request Team State",
                          UIWidgets::ButtonOptions().Tooltip(
                              "Try this if you are missing items or flags that your team members have collected"))) {
        anchor->SendPacket_RequestTeamState();
    }

    if (sAnchorRoomWindow != nullptr) {
        UIWidgets::WindowButton("Toggle Anchor Room Window", CVAR_ANCHOR_WINDOW, sAnchorRoomWindow);
    }

    ImGui::Spacing();

    if (sAnchorRoomWindow != nullptr && !sAnchorRoomWindow->IsVisible()) {
        sAnchorRoomWindow->DrawElement();
    }
}

void AnchorAdminMenu(WidgetInfo& info) {
    auto anchor = Anchor::Instance;
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (anchor == nullptr || !anchor->isEnabled || !anchor->isConnected ||
        anchor->roomState.ownerClientId != anchor->ownClientId || isGlobalRoom) {
        return;
    }

    ImGui::SeparatorText("Room Settings (Admin Only)");

    if (UIWidgets::Button("Clear All Team State")) {
        std::set<std::string> teams;
        for (auto& [clientId, client] : anchor->clients) {
            teams.insert(client.teamId);
        }
        for (auto& team : teams) {
            anchor->SendPacket_ClearTeamState(team);
        }
    }

    if (UIWidgets::CVarCombobox("PvP Mode:", CVAR_REMOTE_ANCHOR("RoomSettings.PvpMode"), sPvpModes,
                                UIWidgets::ComboboxOptions().DefaultIndex(1))) {
        anchor->SendPacket_UpdateRoomState();
    }
    if (UIWidgets::CVarCombobox("Show Locations For:", CVAR_REMOTE_ANCHOR("RoomSettings.ShowLocationsMode"),
                                sShowLocationsModes, UIWidgets::ComboboxOptions().DefaultIndex(1))) {
        anchor->SendPacket_UpdateRoomState();
    }
    if (UIWidgets::CVarCombobox("Allow Teleporting To:", CVAR_REMOTE_ANCHOR("RoomSettings.TeleportMode"),
                                sTeleportModes, UIWidgets::ComboboxOptions().DefaultIndex(1))) {
        anchor->SendPacket_UpdateRoomState();
    }
    if (UIWidgets::CVarCheckbox("Sync Items & Flags", CVAR_REMOTE_ANCHOR("RoomSettings.SyncItemsAndFlags"),
                                UIWidgets::CheckboxOptions().DefaultValue(true))) {
        anchor->SendPacket_UpdateRoomState();
    }
}

void AnchorInstructionsMenu(WidgetInfo& info) {
    ImGui::SeparatorText("Usage Instructions");

    ImGui::TextWrapped("1. All players involved should start at the file select screen");
    ImGui::TextWrapped("2. Come up with a unique Room ID (this is basically your password) and enter it, along with "
                       "your desired player name and team ID and click Enable");
    ImGui::TextWrapped("3. For a randomizer, all players should load the same seed, make sure it matches, then create "
                       "a new file.");
    ImGui::TextWrapped("4. All players should now load into their game. IMPORTANT! If using an existing save/seed "
                       "ensure the player with the most progress loads the file first.");
    ImGui::TextWrapped("5. After everyone has loaded in, verify in the room list that it doesn't warn about anyone "
                       "being on a wrong version or seed.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Note: Team ID is used to group players together in the same team, sharing items and flags. Majora's Mask's "
        "flags that a Song of Time keeps are always shared; those of the three days only between players in the same "
        "cycle. Players in Hyrule (Ship of Harkinian, MergeShip2) share the room: they are listed with their place "
        "there, and the items they find reach you here.");
}

void MergeShipOptions(WidgetInfo& info) {
    ImGui::SeparatorText("MergeShip2");
    UIWidgets::CVarCheckbox(
        "One Anchor Connection for Both Worlds", MS_CVAR_ANCHOR_SHARED_CONNECTION,
        UIWidgets::CheckboxOptions().DefaultValue(true).Tooltip(
            "In mergeship.exe, Termina and Hyrule share one Anchor connection: going through the door never leaves "
            "the room. Takes effect the next time Anchor is enabled. No effect in 2ship.exe."));
    UIWidgets::CVarCheckbox(
        "Receive Items From the Other World", MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS,
        UIWidgets::CheckboxOptions().DefaultValue(true).Tooltip(
            "An item a teammate finds in Hyrule reaches you in Termina (and the other way round), through the "
            "shared inventory both worlds keep. Items only one world can hold wait for you there."));
    UIWidgets::CVarCheckbox(
        "Anchor Text Chat", MS_CVAR_ANCHOR_CHAT,
        UIWidgets::CheckboxOptions().DefaultValue(true).Tooltip(
            "While connected to Anchor, press T to chat: a chat box opens in the bottom left and the game ignores "
            "your input until you press Enter to send (Enter on an empty line just closes it). Messages show in the "
            "chat box with each player's name in their Anchor colour, and above the sender's head for a few "
            "seconds. Hyrule and Termina share one conversation."));
}

void RegisterAnchorMenu() {
    // The room window: created here, with the menu (BenGui's own windows are made in BenGui.cpp).
    if (sAnchorRoomWindow == nullptr) {
        sAnchorRoomWindow = std::make_shared<AnchorRoomWindow>(CVAR_ANCHOR_WINDOW, "Anchor Room");
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->AddGuiWindow(sAnchorRoomWindow);
    }
    MergeShipAnchor_RegisterChat(); // MergeShip2: Anchor text chat

    BenGui::mBenMenu->AddMenuEntry("Network", "gSettings.Menu.NetworkSidebarSection");
    WidgetPath path = { "Network", "Anchor", SECTION_COLUMN_1 };
    BenGui::mBenMenu->AddSidebarEntry("Network", path.sidebarName, 2);
    BenGui::mBenMenu->AddWidget(path, "AnchorMainMenu", WIDGET_CUSTOM)
        .CustomFunction(AnchorMainMenu)
        .HideInSearch(true);
    path.column = SECTION_COLUMN_2;
    BenGui::mBenMenu->AddWidget(path, "AnchorAdminMenu", WIDGET_CUSTOM)
        .CustomFunction(AnchorAdminMenu)
        .HideInSearch(true);
    BenGui::mBenMenu->AddWidget(path, "AnchorInstructionsMenu", WIDGET_CUSTOM)
        .CustomFunction(AnchorInstructionsMenu)
        .HideInSearch(true);
    BenGui::mBenMenu->AddWidget(path, "AnchorMergeShipOptions", WIDGET_CUSTOM)
        .CustomFunction(MergeShipOptions)
        .HideInSearch(true);
}

static RegisterMenuInitFunc sAnchorMenuInit(RegisterAnchorMenu);

} // namespace
