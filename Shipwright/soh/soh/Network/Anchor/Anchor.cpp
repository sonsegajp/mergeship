#include <filesystem>
#include <nlohmann/json.hpp>

#include "Anchor.h"
#include "soh/Enhancements/nametag.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include <mergeship/AnchorHost.hpp>   // MergeShip2: MsAnchorHost::Seq
#include <mergeship/AnchorReplay.hpp> // MergeShip2: MsAnchor_IsReplaySeq

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
void MergeShipTest_Log(const char* fmt, ...);
extern PlayState* gPlayState;
}

// MergeShip2: the saved client id belongs to the game (folder and player name) the server gave it to. A copy of this
// config in another folder or under another name -- a second game running at the same time -- asks for its own id;
// with the same id the server keeps dropping one of them for the other, and neither stays connected.
static std::string MergeShipAnchor_ClientIdOwner() {
    std::string folder;
    try {
        folder = std::filesystem::current_path().string();
    } catch (...) {}
    return folder + "|" + CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
}

std::string MergeShipAnchor_ClientIdOwnerOf() {
    return MergeShipAnchor_ClientIdOwner();
}

// MARK: - Overrides

void Anchor::Enable() {
    // MergeShip2: in mergeship.exe the connection is the host's, shared with Termina (MergeShipAnchorHost.cpp); the
    // client id is read first because the host sends the handshake built here.
    MergeShipAnchor_AttachHost(this);
    ownClientId = CVarGetInteger(CVAR_REMOTE_ANCHOR("LastClientId"), 0);
    if (MergeShipAnchor_ClientIdOwner() != CVarGetString(CVAR_REMOTE_ANCHOR("LastClientIdOwner"), "")) {
        ownClientId = 0; // MergeShip2: not this game's id (MergeShipAnchor_ClientIdOwner)
    }
    roomState.ownerClientId = 0;
    MergeShipAnchor_BeforeEnable(this);
    Network::Enable(CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org"),
                    CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383));
}

void Anchor::Disable() {
    Network::Disable();

    mergeShipDeferredPackets.clear(); // MergeShip2: the room left behind (Enable may join another)
    clients.clear();
    RefreshClientActors();
}

void Anchor::OnConnected() {
    // MergeShip2: on the host's shared connection (MergeShipAnchorHost.cpp) the host already sent the handshake, and
    // only the engine the player is in speaks for him: where he is now (the stored handshake may be older), and the
    // team's state. The engine of the other world only listens.
    if (IsHostTransport()) {
        RegisterHooks();
        if (MergeShipAnchor_IsActiveEngine()) {
            SendPacket_UpdateClientState();
            if (IsSaveLoaded()) {
                SendPacket_RequestTeamState();
            }
        }
        MergeShipAnchor_LogState("connected");
        return;
    }
    SendPacket_Handshake();
    RegisterHooks();

    if (IsSaveLoaded()) {
        SendPacket_RequestTeamState();
    }
}

void Anchor::OnDisconnected() {
    RegisterHooks();
}

void Anchor::ProcessOutgoingPackets() {
    // Copy all queued packets while holding the lock, then send them after releasing
    std::queue<nlohmann::json> packetsToSend;
    {
        std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
        packetsToSend.swap(outgoingPacketQueue);
    }

    // Send packets without holding the lock
    while (!packetsToSend.empty()) {
        nlohmann::json payload = packetsToSend.front();
        packetsToSend.pop();

        if (!payload.contains("quiet")) {
            SPDLOG_DEBUG("[Anchor] Sending payload:\n{}", payload.dump());
        }
        Network::SendJsonToRemote(payload);
    }
}

void Anchor::SendJsonToRemote(nlohmann::json payload) {
    if (!isConnected) {
        return;
    }

    payload["clientId"] = ownClientId;
    // MergeShip2: the playthrough this packet belongs to (AnchorNet.h MS_ANCHOR_KEY_SEED).
    payload[MS_ANCHOR_KEY_SEED] = MergeShipAnchor_LocalSeed();
    // MergeShip2: a packet the server may queue for an offline teammate is made unique (AnchorReplay.hpp).
    if (payload.contains("addToQueue")) {
        payload["msId"] = MsAnchor_NewPacketId();
    }
    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Queuing payload:\n{}", payload.dump());
    }

    if (payload["type"] == HANDSHAKE) {
        // MergeShip2: on the host's connection, the hello for its reconnects.
        Network::SendHelloToRemote(payload);
        return;
    }

    // Queue the packet to be sent on the network thread
    std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
    outgoingPacketQueue.push(payload);
}

void Anchor::OnIncomingJson(nlohmann::json payload) {
    // If it doesn't contain a type, it's not a valid payload
    if (!payload.contains("type") || !payload["type"].is_string()) {
        return;
    }
    // MergeShip2: when it came (MergeShipIsJoinAnswer: a team state answers our request by when it arrived).
    payload[kMergeShipRecvMsKey] = MergeShipNowMs();

    // If it's not a quiet payload, log it
    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Received payload:\n{}", payload.dump());
    }

    std::string packetType = payload["type"].get<std::string>();

    // MergeShip2: while the player is in the other world this engine keeps only what it must apply later.
    if (!MergeShipAnchor_KeepIncoming(this, packetType)) {
        return;
    }
    // MergeShip2: the harness trace sees every save-changing packet as it arrives (Anchor tests).
    const bool msTrace = (packetType != PLAYER_UPDATE) && (packetType != PLAYER_SFX) && !payload.contains("quiet");

    // Ignore packets from mismatched clients, except for ALL_CLIENT_STATE, UPDATE_CLIENT_STATE, and PLAYER_UPDATE
    if (packetType != ALL_CLIENT_STATE && packetType != UPDATE_CLIENT_STATE && packetType != PLAYER_UPDATE) {
        if (payload.contains("clientId") && payload["clientId"].is_number()) {
            uint32_t clientId = payload["clientId"].get<uint32_t>();
            if (clients.contains(clientId) && clients[clientId].clientVersion != clientVersion) {
                if (msTrace) {
                    MergeShipTest_Log("ANCHOR %s from client %u ignored: client version %s, ours %s",
                                      packetType.c_str(), clientId, clients[clientId].clientVersion.c_str(),
                                      clientVersion.c_str());
                }
                return;
            }
        }
    }

    if (msTrace) {
        MergeShipTest_Log("ANCHOR in %s", packetType.c_str());
    }
    // Queue all packets to be processed on the game thread
    std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
    incomingPacketQueue.push(payload);
}

// MergeShip2: a packet that changes the save and that this engine handles (ProcessIncomingPacketQueue's dispatch): one
// that arrives with no file loaded waits for one. Majora's Mask's own (MM_SET_FLAG, MM_UNSET_FLAG,
// MM_UPDATE_DUNGEON_ITEMS) and CONSUME_ADULT_TRADE_ITEM carry a playthrough too but are never handled here: they are
// dropped at once, not kept to fill the wait list (kMergeShipDeferredMax) and push out the oldest items. A
// REQUEST_TEAM_STATE is not kept either: with no file there is no state to answer with, and a late answer would miss
// the requester's join window (the teammates who have a file loaded answer it).
static bool MergeShipWaitsForFile(const std::string& type) {
    return (type == Anchor::GIVE_ITEM) || (type == Anchor::SET_CHECK_STATUS) || (type == Anchor::SET_FLAG) ||
           (type == Anchor::UNSET_FLAG) || (type == Anchor::UPDATE_BEANS_COUNT) ||
           (type == Anchor::UPDATE_DUNGEON_ITEMS) || (type == Anchor::ENTRANCE_DISCOVERED) ||
           (type == Anchor::GAME_COMPLETE) || (type == Anchor::UPDATE_TEAM_STATE) || (type == MS_ANCHOR_MM_GIVE_ITEM) ||
           (type == MS_ANCHOR_MM_SET_CHECK_STATUS) || (type == MS_ANCHOR_AMMO_SYNC);
}

// MergeShip2: the player is in a textbox or receiving an item, when MergeShipUnified's OnFrame waits too. A team state
// merged then could change a save the game is in the middle of changing: a 4th Piece of Heart only becomes a container
// when its textbox closes (z_message_PAL.c), and a merge that applied the shared save meanwhile left 0 pieces and no
// container -- a heart lost for good. A save is loaded (gPlayState and the player exist).
static bool MergeShipPlayerBusy() {
    Player* player = GET_PLAYER(gPlayState);
    return (gPlayState->msgCtx.msgMode != MSGMODE_NONE) ||
           ((player != nullptr) && (player->stateFlags1 & (PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS)));
}

void Anchor::ProcessIncomingPacketQueue() {
    // Copy all queued packets while holding the lock, then process them after releasing
    std::queue<nlohmann::json> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        packetsToProcess.swap(incomingPacketQueue);
    }

    // MergeShip2: a packet that changes the save is never dropped for want of a loaded file. One that arrives at the
    // title screen, at file select or during a scene load used to reach its handler, which returned (no file) or, for a
    // team state, wrote into a SaveContext the file's load then overwrote: a connected player lost every item and flag
    // the team found meanwhile. Such packets wait, in order, and go first once a file is loaded
    // (MergeShipWaitsForFile). Their playthrough is checked then (MergeShipAnchor_SeedAccepts, below), against the file
    // actually loaded. A team state also waits while the player is busy (MergeShipPlayerBusy).
    const bool saveLoaded = IsSaveLoaded();
    const bool playerBusy = saveLoaded && MergeShipPlayerBusy();
    if (saveLoaded && !mergeShipDeferredPackets.empty()) {
        std::queue<nlohmann::json> ordered;
        for (auto& deferred : mergeShipDeferredPackets) {
            ordered.push(std::move(deferred));
        }
        mergeShipDeferredPackets.clear();
        while (!packetsToProcess.empty()) {
            ordered.push(std::move(packetsToProcess.front()));
            packetsToProcess.pop();
        }
        packetsToProcess.swap(ordered);
    }

    // Process packets without holding the lock
    while (!packetsToProcess.empty()) {
        nlohmann::json payload = std::move(packetsToProcess.front());
        packetsToProcess.pop();

        // MergeShip2: a replayed team queue entry bypasses OnIncomingJson's check (UpdateTeamState.cpp skips those).
        if (!payload.contains("type") || !payload["type"].is_string()) {
            continue;
        }
        std::string packetType = payload["type"].get<std::string>();

        if ((!saveLoaded && MergeShipWaitsForFile(packetType)) || (playerBusy && (packetType == UPDATE_TEAM_STATE))) {
            mergeShipDeferredPackets.push_back(std::move(payload));
            if (mergeShipDeferredPackets.size() > kMergeShipDeferredMax) {
                mergeShipDeferredPackets.pop_front();
                SPDLOG_WARN("[Anchor] MergeShip2: more than {} packets waiting for a file; the oldest was dropped",
                            kMergeShipDeferredMax);
            }
            continue;
        }

        // MergeShip2: a replayed team queue has been applied (HandlePacket_UpdateTeamState): our team state, which now
        // holds it, becomes the team's, and the server clears the queue it keeps handing to every request. Never
        // kept: with no file loaded now the replayed packets wait for one, and its next save sends the state.
        if (packetType == kMergeShipReplayDone) {
            if (saveLoaded) {
                SendPacket_UpdateTeamState(true);
            }
            continue;
        }

        isProcessingIncomingPacket = true;

        try {
            // MergeShip2: nothing of another playthrough changes this save (AnchorNet.h MS_ANCHOR_KEY_SEED).
            if (!MergeShipAnchor_SeedAccepts(this, payload)) {
                isProcessingIncomingPacket = false;
                continue;
            }
            // MergeShip2: a replayed team queue entry is handled once per process however many replies carry it
            // (mergeShipReplayed; AnchorReplay.hpp). Noted only now, once it is this playthrough's and handled.
            const uint64_t replaySeq = MsAnchorHost::Seq(payload);
            if (MsAnchor_IsReplaySeq(replaySeq) && !mergeShipReplayed.insert(replaySeq).second) {
                SPDLOG_DEBUG("[Anchor] MergeShip2: queued {} replayed again: already handled", packetType);
                isProcessingIncomingPacket = false;
                continue;
            }
            // packetType here is a string so we can't use a switch statement
            if (packetType == ALL_CLIENT_STATE)
                HandlePacket_AllClientState(payload);
            else if (packetType == DAMAGE_PLAYER)
                HandlePacket_DamagePlayer(payload);
            else if (packetType == DISABLE_ANCHOR)
                HandlePacket_DisableAnchor(payload);
            else if (packetType == ENTRANCE_DISCOVERED)
                HandlePacket_EntranceDiscovered(payload);
            else if (packetType == GAME_COMPLETE)
                HandlePacket_GameComplete(payload);
            else if (packetType == GIVE_ITEM)
                HandlePacket_GiveItem(payload);
            else if (packetType == OCARINA_SFX)
                HandlePacket_OcarinaSfx(payload);
            else if (packetType == PLAYER_UPDATE)
                HandlePacket_PlayerUpdate(payload);
            else if (packetType == PLAYER_SFX)
                HandlePacket_PlayerSfx(payload);
            else if (packetType == UPDATE_TEAM_STATE)
                HandlePacket_UpdateTeamState(payload);
            else if (packetType == REQUEST_TEAM_STATE)
                HandlePacket_RequestTeamState(payload);
            else if (packetType == REQUEST_TELEPORT)
                HandlePacket_RequestTeleport(payload);
            else if (packetType == SERVER_MESSAGE)
                HandlePacket_ServerMessage(payload);
            else if (packetType == SET_CHECK_STATUS)
                HandlePacket_SetCheckStatus(payload);
            else if (packetType == SET_FLAG)
                HandlePacket_SetFlag(payload);
            else if (packetType == TELEPORT_TO)
                HandlePacket_TeleportTo(payload);
            else if (packetType == UNSET_FLAG)
                HandlePacket_UnsetFlag(payload);
            else if (packetType == UPDATE_BEANS_COUNT)
                HandlePacket_UpdateBeansCount(payload);
            else if (packetType == UPDATE_CLIENT_STATE)
                HandlePacket_UpdateClientState(payload);
            else if (packetType == UPDATE_ROOM_STATE)
                HandlePacket_UpdateRoomState(payload);
            else if (packetType == UPDATE_DUNGEON_ITEMS)
                HandlePacket_UpdateDungeonItems(payload);
            else if (packetType == MS_ANCHOR_MM_GIVE_ITEM) // MergeShip2: found in Termina
                HandlePacket_MMGiveItem(payload);
            else if (packetType == MS_ANCHOR_CHAT) // MergeShip2: text chat (MergeShipAnchorChat.cpp)
                MergeShipAnchor_HandleChat(this, payload);
            else if (packetType == MS_ANCHOR_AMMO_SYNC) // MergeShip2: the shared ammo pool (Packets/AmmoSync.cpp)
                HandlePacket_AmmoSync(payload);
            else if (packetType == MS_ANCHOR_MM_SET_CHECK_STATUS) // MergeShip2: the team's Termina checks
                HandlePacket_MMSetCheckStatus(payload);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] Exception while processing incoming packet {}", e.what());
            SPDLOG_ERROR("[Anchor] Packet: {}", payload.dump());
        }

        isProcessingIncomingPacket = false;
    }
}

// MARK: - Misc/Helpers

// Kills all existing anchor actors and respawns them with the new client data

struct DummyPlayerClientId {
    uint32_t clientId = 0;
};
static ObjectExtension::Register<DummyPlayerClientId> DummyPlayerClientIdRegister;

uint32_t Anchor::GetDummyPlayerClientId(const Actor* actor) {
    const DummyPlayerClientId* clientId = ObjectExtension::GetInstance().Get<DummyPlayerClientId>(actor);
    return clientId != nullptr ? clientId->clientId : 0;
}

void Anchor::SetDummyPlayerClientId(const Actor* actor, uint32_t clientId) {
    ObjectExtension::GetInstance().Set<DummyPlayerClientId>(actor, DummyPlayerClientId{ clientId });
}

void Anchor::RefreshClientActors() {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;

    while (actor != NULL) {
        if (actor->id == ACTOR_EN_OE2 && actor->update == DummyPlayer_Update) {
            NameTag_RemoveAllForActor(actor);
            Actor_Kill(actor);
        }
        actor = actor->next;
    }

    for (auto& [clientId, client] : clients) {
        if (!client.online || client.self) {
            continue;
        }

        spawningDummyPlayerForClientId = clientId;
        // We are using a hook `ShouldActorInit` to override the init/update/draw/destroy functions of the Player we
        // spawn We quickly store a mapping of "index" to clientId, then within the init function we use this to get the
        // clientId and store it on player->zTargetActiveTimer (unused s32 for the dummy) for convenience
        auto dummy =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_PLAYER, client.posRot.pos.x, client.posRot.pos.y,
                        client.posRot.pos.z, client.posRot.rot.x, client.posRot.rot.y, client.posRot.rot.z, 0);
        client.player = (Player*)dummy;
    }
    spawningDummyPlayerForClientId = 0;
}

bool Anchor::IsSaveLoaded() {
    if (gPlayState == nullptr) {
        return false;
    }

    if (GET_PLAYER(gPlayState) == nullptr) {
        return false;
    }

    if (gSaveContext.fileNum < 0 || gSaveContext.fileNum > 2) {
        return false;
    }

    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    return true;
}
