// MergeShip2 -- Anchor text chat in Termina (include/mergeship/AnchorChat.hpp): the chat box, MS_ANCHOR_CHAT packets,
// and each player's latest message above their head (a name tag over their dummy, or over Link for this player's).
// The counterpart of SoH's soh/Network/Anchor/MergeShipAnchorChat.cpp.

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/NameTag/NameTag.h"
#include "2s2h/GameInteractor/GameInteractor.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/AnchorChat.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
}

const MsAnchorHost* MergeShipAnchor_Host(); // MergeShipAnchorHost.cpp

static MsAnchorChat sChat;
static const char* const kBubbleTag = "MergeShipAnchorChat";
static constexpr uint32_t kSelfKey = 0xFFFFFFFF;

static bool ChatOn() {
    return CVarGetInteger(MS_CVAR_ANCHOR_CHAT, 1) != 0;
}

static MsAnchorChat& Chat() {
    sChat.host = MergeShipAnchor_Host();
    return sChat;
}

// Anchor::ProcessIncomingPacketQueue: a teammate's message (once per process, by the engine that claims it).
void MergeShipAnchor_HandleChat(Anchor* anchor, const nlohmann::json& payload) {
    MsChatMessage message;

    if (!MsAnchorChat::FromPacket(payload, message) || !MergeShipAnchor_Claim(payload)) {
        return;
    }
    // The name and colour the room shows for the sender.
    auto client = anchor->clients.find(message.clientId);
    if (client != anchor->clients.end()) {
        message.name = client->second.name;
        message.r = client->second.color.r;
        message.g = client->second.color.g;
        message.b = client->second.color.b;
    }
    Chat().Add(message);
}

static void SendChat(const std::string& text) {
    Anchor* anchor = Anchor::Instance;
    MsChatMessage message;
    Color_RGB8 color = CVarGetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100 });

    message.self = true;
    message.clientId = anchor->ownClientId;
    message.name = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    auto own = anchor->clients.find(anchor->ownClientId);
    if (own != anchor->clients.end()) {
        color = own->second.color;
        if (!own->second.name.empty()) {
            message.name = own->second.name;
        }
    }
    if (message.name.empty()) {
        message.name = "You";
    }
    message.r = color.r;
    message.g = color.g;
    message.b = color.b;
    message.text = text;
    message.timeMs = MsAnchorChat::NowMs();
    anchor->SendJsonToRemote(MsAnchorChat::Packet(text, message.name, color.r, color.g, color.b));
    Chat().Add(message);
}

static void UpdateBubbles();

// The chat box: always "open" as a window, drawn only while there is something to show (AnchorChat.hpp).
class AnchorChatWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override {
        // The bubbles' frame hook, once the game's hooks exist (the menus, and this window, are made before them).
        static bool sHooked = false;
        if (!sHooked && (GameInteractor::Instance != nullptr)) {
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() { UpdateBubbles(); });
            sHooked = true;
        }
        Anchor* anchor = Anchor::Instance;
        const bool canChat = ChatOn() && (anchor != nullptr) && anchor->isConnected && MergeShipAnchor_IsActiveEngine();
        const bool menuOpen = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenuOrMenubarVisible();

        // Only the world being played draws the chat (in mergeship.exe the other one is parked).
        if (!ChatOn() || !MergeShipAnchor_IsActiveEngine()) {
            return;
        }
        Chat().Draw(canChat, menuOpen, SendChat);
    }
};

static std::shared_ptr<AnchorChatWindow> sChatWindow;

// Each game frame: the latest message of every player in this scene over their head, for kBubbleMs.
static void UpdateBubbles() {
    static std::string sShown;
    static u32 sFrames = 0;
    Anchor* anchor = Anchor::Instance;
    std::map<uint32_t, std::string> bubbles;
    std::vector<std::pair<Actor*, std::string>> tags;
    std::string shown;

    if ((gPlayState != nullptr) && ChatOn() && (anchor != nullptr) && anchor->isConnected) {
        bubbles = Chat().Bubbles(kSelfKey);
    }
    if (!bubbles.empty()) {
        Player* player = GET_PLAYER(gPlayState);
        auto self = bubbles.find(kSelfKey);

        if ((self != bubbles.end()) && (player != nullptr)) {
            tags.emplace_back(&player->actor, self->second);
        }
        for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].first; actor != nullptr;
             actor = actor->next) {
            if (actor->update != DummyPlayer_Update) {
                continue;
            }
            auto bubble = bubbles.find(anchor->GetDummyPlayerClientId(actor));
            if (bubble != bubbles.end()) {
                tags.emplace_back(actor, bubble->second);
            }
        }
    }
    for (const auto& tag : tags) {
        shown += std::to_string((uintptr_t)tag.first) + ":" + tag.second + "\n";
    }
    // Re-made when they change, and now and then (a scene change drops every name tag).
    if ((shown == sShown) && ((++sFrames % 20) != 0)) {
        return;
    }
    sShown = shown;
    NameTag_RemoveAllByTag(kBubbleTag);
    for (const auto& tag : tags) {
        NameTagOptions options = {};
        options.tag = kBubbleTag;
        // Above the player's name (a dummy's), or just above Link's head (this player's, who has no name tag).
        options.yOffset = (tag.first->update == DummyPlayer_Update) ? 22 : 4;
        NameTag_RegisterForActorWithOptions(tag.first, tag.second.c_str(), options);
    }
}

// Menu.cpp RegisterAnchorMenu, beside the Anchor Room window.
void MergeShipAnchor_RegisterChat() {
    if (sChatWindow != nullptr) {
        return;
    }
    sChatWindow = std::make_shared<AnchorChatWindow>("gOpenWindows.MergeShipAnchorChat", true, "Anchor Chat (Termina)");
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->AddGuiWindow(sChatWindow);
}
