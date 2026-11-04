// MergeShip2 -- Anchor's room window in 2 Ship 2 Harkinian, ported from Ship of Harkinian's AnchorRoomWindow.cpp.
// A teammate in Hyrule is listed with his place there ("(Hyrule)"), as SoH lists a Termina player's.
#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Rando/Rando.h"

#include <imgui.h>
#include <set>

extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
}

void AnchorRoomWindow::Draw() {
    if (!IsVisible() || Anchor::Instance == nullptr || !Anchor::Instance->isConnected) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    auto vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::Begin("Anchor Room", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    DrawElement();

    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void AnchorRoomWindow::DrawElement() {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    bool isGlobalRoom = (std::string("soh-global") == CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), ""));

    if (isGlobalRoom) {
        u32 activeClients = 0;
        for (auto& [clientId, client] : anchor->clients) {
            if (client.online) {
                activeClients++;
            }
        }
        ImGui::Text("Players Online: %d", activeClients);
        return;
    }

    // First build a list of teams
    std::set<std::string> teams;
    for (auto& [clientId, client] : anchor->clients) {
        teams.insert(client.teamId);
    }

    for (auto& team : teams) {
        if (teams.size() > 1) {
            ImGui::SeparatorText(team.c_str());
        }
        bool isOwnTeam = team == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
        for (auto& [clientId, client] : anchor->clients) {
            if (client.teamId != team) {
                continue;
            }

            ImGui::PushID(clientId);

            if (client.clientId == anchor->roomState.ownerClientId) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", "*");
                ImGui::SameLine();
            }

            if (client.self) {
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s", CVarGetString(CVAR_REMOTE_ANCHOR("Name"), ""));
            } else if (!client.online) {
                ImGui::TextColored(ImVec4(1, 1, 1, 0.3f), "%s - offline", client.name.c_str());
                ImGui::PopID();
                continue;
            } else {
                ImGui::Text("%s", client.name.c_str());
            }

            if (anchor->roomState.showLocationsMode == 2 || (anchor->roomState.showLocationsMode == 1 && isOwnTeam)) {
                if (client.self ? anchor->IsSaveLoaded() : client.isSaveLoaded) {
                    ImGui::SameLine();
                    if (client.self) {
                        ImGui::TextColored(ImVec4(1, 1, 1, 0.5f), "- %s",
                                           anchor->SceneName(gPlayState->sceneId).c_str());
                    } else if (Anchor::IsOwnWorld(client)) {
                        ImGui::TextColored(ImVec4(1, 1, 1, 0.5f), "- %s",
                                           anchor->SceneName(Anchor::LocalScene(client)).c_str());
                    } else {
                        // A player in Hyrule, where Ocarina of Time's engine named the place
                        ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 0.7f), "- %s (Hyrule)",
                                           client.sceneName.empty() ? "?" : client.sceneName.c_str());
                    }
                }
            }

            if (anchor->CanTeleportTo(client.clientId)) {
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                if (ImGui::Button(">", ImVec2(20.0f, 20.0f))) {
                    anchor->SendPacket_RequestTeleport(client.clientId);
                }
                ImGui::PopStyleVar();
            }

            if (client.clientVersion != Anchor::clientVersion) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "!");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Incompatible version! Will not work together!");
                    ImGui::Text("Yours: %s", Anchor::clientVersion.c_str());
                    ImGui::Text("Theirs: %s", client.clientVersion.c_str());
                    ImGui::EndTooltip();
                }
            }
            // (a Hyrule player's seed is Ocarina of Time's)
            uint32_t seed = IS_RANDO ? gSaveContext.save.shipSaveInfo.rando.finalSeed : 0;
            if (client.isSaveLoaded && anchor->IsSaveLoaded() && client.seed != seed && client.online &&
                !client.self && Anchor::IsOwnWorld(client)) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "!");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Seed mismatch! Continuing will break things!");
                    ImGui::Text("Yours: %u", seed);
                    ImGui::Text("Theirs: %u", client.seed);
                    ImGui::EndTooltip();
                }
            }
            ImGui::PopID();
        }
    }
}
