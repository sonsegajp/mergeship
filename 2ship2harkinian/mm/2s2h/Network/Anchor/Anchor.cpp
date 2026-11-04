// MergeShip2 -- Anchor for 2 Ship 2 Harkinian (Anchor.h), ported from Ship of Harkinian's soh/Network/Anchor/Anchor.cpp.
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mergeship/AnchorReplay.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/MergeShip/MergeShipHost.h" // MergeShipHostTest_Log
#include "2s2h/NameTag/NameTag.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include <mergeship/AnchorHost.hpp>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

Anchor* Anchor::Instance = nullptr;

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

// MergeShip2: the save-changing packet types (MsAnchor_IsSeededPacket) ProcessIncomingPacketQueue hands to a handler
// here. The others -- SET_FLAG, UNSET_FLAG, SET_CHECK_STATUS, UPDATE_DUNGEON_ITEMS, UPDATE_BEANS_COUNT,
// ENTRANCE_DISCOVERED, CONSUME_ADULT_TRADE_ITEM -- are about Ocarina of Time's save and SoH's alone: kept (while
// Termina is parked, or until a file is played) they would only push out packets this engine does apply.
static bool MergeShipHandlesSeeded(const std::string& type) {
    return type == Anchor::GIVE_ITEM || type == Anchor::MM_GIVE_ITEM || type == Anchor::GAME_COMPLETE ||
           type == Anchor::UPDATE_TEAM_STATE || type == Anchor::REQUEST_TEAM_STATE ||
           type == Anchor::MM_SET_CHECK_STATUS || type == Anchor::MM_SET_FLAG || type == Anchor::MM_UNSET_FLAG ||
           type == Anchor::MM_UPDATE_DUNGEON_ITEMS || type == MS_ANCHOR_AMMO_SYNC;
}

// A payload a handler can be given: an object whose "type" is a string (MergeShip2: anything else -- a malformed
// replayed queue entry, "{}", "null", "[1]" -- used to throw outside the handlers' try, ending the game).
static bool MergeShipHasType(const nlohmann::json& payload) {
    return payload.is_object() && payload.contains("type") && payload["type"].is_string();
}

// MARK: - Overrides

void Anchor::Enable() {
    // mergeship.exe: the host's connection, shared with Hyrule (MergeShipAnchorHost.cpp). The client id is read first
    // because the host sends the handshake built here.
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

    clients.clear();
    RefreshClientActors();
}

void Anchor::OnConnected() {
    // On the host's shared connection the host already sent the handshake, and only the engine the player is in
    // speaks for him; the other world's engine only listens.
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
        // On the host's connection: the hello for its reconnects.
        Network::SendHelloToRemote(payload);
        return;
    }

    // Queue the packet to be sent on the network thread
    std::lock_guard<std::mutex> lock(outgoingPacketQueueMutex);
    outgoingPacketQueue.push(payload);
}

void Anchor::OnIncomingJson(nlohmann::json payload) {
    // If it doesn't contain a type, it's not a valid payload (MergeShip2: nor is one whose type is not a string)
    if (!MergeShipHasType(payload)) {
        return;
    }

    if (!payload.contains("quiet")) {
        SPDLOG_DEBUG("[Anchor] Received payload:\n{}", payload.dump());
    }

    std::string packetType = payload["type"].get<std::string>();

    // While the player is in the other world this engine keeps only what it must apply later.
    if (!MergeShipAnchor_KeepIncoming(this, packetType)) {
        return;
    }
    // MergeShip2: Hyrule's own save-changing packets are never this engine's to apply (MergeShipHandlesSeeded).
    if (MsAnchor_IsSeededPacket(packetType) && !MergeShipHandlesSeeded(packetType)) {
        return;
    }

    // Ignore packets from mismatched clients, except for ALL_CLIENT_STATE, UPDATE_CLIENT_STATE, and PLAYER_UPDATE
    if (packetType != ALL_CLIENT_STATE && packetType != UPDATE_CLIENT_STATE && packetType != PLAYER_UPDATE) {
        if (payload.contains("clientId") && payload["clientId"].is_number()) {
            uint32_t clientId = payload["clientId"].get<uint32_t>();
            if (clients.contains(clientId) && clients[clientId].clientVersion != clientVersion) {
                return;
            }
        }
    }

    // MergeShip2: when it came. A team state is the answer to our own request by its arrival, not by when the game
    // thread gets to it (Anchor::MergeShipIsJoinAnswer).
    if (packetType == UPDATE_TEAM_STATE) {
        payload[MS_ANCHOR_KEY_RECV_MS] = MergeShipAnchor_NowMs();
    }

    // Queue all packets to be processed on the game thread
    std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
    incomingPacketQueue.push(payload);
}

void Anchor::ProcessIncomingPacketQueue() {
    MergeShipNoteGameThread();

    std::queue<nlohmann::json> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        packetsToProcess.swap(incomingPacketQueue);
    }

    // MergeShip2: what waited for a Termina file to be played goes first, in the order it came.
    const bool saveLoaded = IsSaveLoaded();
    if (saveLoaded && !mergeShipDeferredPackets.empty()) {
        std::queue<nlohmann::json> ordered;
        for (nlohmann::json& deferred : mergeShipDeferredPackets) {
            ordered.push(std::move(deferred));
        }
        mergeShipDeferredPackets.clear();
        while (!packetsToProcess.empty()) {
            ordered.push(std::move(packetsToProcess.front()));
            packetsToProcess.pop();
        }
        packetsToProcess.swap(ordered);
    }

    // MergeShip2: a team state's queue replayed on an earlier frame (HandlePacket_UpdateTeamState) is in this batch:
    // once it is handled, our team state goes out, which clears the server's queue -- otherwise every later reply (our
    // next request, the other world's, a reconnect's) carries the same packets again.
    const bool pushAfterReplay = saveLoaded && mergeShipReplayPushPending;
    if (pushAfterReplay) {
        mergeShipReplayPushPending = false;
    }

    while (!packetsToProcess.empty()) {
        nlohmann::json payload = std::move(packetsToProcess.front());
        packetsToProcess.pop();

        // MergeShip2: checked here, outside the try below (a replayed queue entry never went through OnIncomingJson).
        if (!MergeShipHasType(payload)) {
            continue;
        }
        std::string packetType = payload["type"].get<std::string>();

        // MergeShip2: a packet that changes the save waits while no Termina file is being played -- at the title, the
        // file select, or the first frames after the player first arrives, before Play_Init -- where its handler would
        // drop it (the user, 2026-09-30: a joiner gets everything, and so does a player who was away). Its playthrough
        // is checked once it is handled. Only the types handled here wait; Hyrule's own go at once. (A team state
        // request is not for us to answer then: another engine does, or nobody needs our state.)
        if (!saveLoaded && MsAnchor_IsSeededPacket(packetType) && packetType != REQUEST_TEAM_STATE) {
            if (!MergeShipHandlesSeeded(packetType)) {
                continue;
            }
            mergeShipDeferredPackets.push_back(std::move(payload));
            if (mergeShipDeferredPackets.size() > kMergeShipDeferredMax) {
                // (Logged: a dropped item or flag is a desync the player should be able to trace.)
                const std::string dropped = mergeShipDeferredPackets.front()["type"].get<std::string>();
                SPDLOG_WARN("[Anchor] MergeShip2: more than {} packets waiting for a Termina file: the oldest ({}) "
                            "dropped",
                            kMergeShipDeferredMax, dropped);
                MergeShipHostTest_Log("ANCHOR deferred %s dropped (more than %u waiting)", dropped.c_str(),
                                      (unsigned)kMergeShipDeferredMax);
                mergeShipDeferredPackets.pop_front();
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
            if (packetType == ALL_CLIENT_STATE)
                HandlePacket_AllClientState(payload);
            else if (packetType == DAMAGE_PLAYER)
                HandlePacket_DamagePlayer(payload);
            else if (packetType == DISABLE_ANCHOR)
                HandlePacket_DisableAnchor(payload);
            else if (packetType == GAME_COMPLETE)
                HandlePacket_GameComplete(payload);
            else if (packetType == GIVE_ITEM)
                HandlePacket_GiveItem(payload);
            else if (packetType == MM_GIVE_ITEM)
                HandlePacket_MMGiveItem(payload);
            else if (packetType == MS_ANCHOR_CHAT) // MergeShip2: text chat (MergeShipAnchorChat.cpp)
                MergeShipAnchor_HandleChat(this, payload);
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
            else if (packetType == MM_SET_CHECK_STATUS)
                HandlePacket_MMSetCheckStatus(payload);
            else if (packetType == MM_SET_FLAG)
                HandlePacket_MMSetFlag(payload);
            else if (packetType == TELEPORT_TO)
                HandlePacket_TeleportTo(payload);
            else if (packetType == MM_UNSET_FLAG)
                HandlePacket_MMUnsetFlag(payload);
            else if (packetType == UPDATE_CLIENT_STATE)
                HandlePacket_UpdateClientState(payload);
            else if (packetType == UPDATE_ROOM_STATE)
                HandlePacket_UpdateRoomState(payload);
            else if (packetType == MM_UPDATE_DUNGEON_ITEMS)
                HandlePacket_MMUpdateDungeonItems(payload);
            else if (packetType == MS_ANCHOR_AMMO_SYNC) // MergeShip2: the shared ammo pool (Packets/AmmoSync.cpp)
                HandlePacket_AmmoSync(payload);
            // (Hyrule's own packets -- SET_FLAG, UNSET_FLAG, SET_CHECK_STATUS, UPDATE_DUNGEON_ITEMS,
            // UPDATE_BEANS_COUNT, ENTRANCE_DISCOVERED -- are about Ocarina of Time's save and are left to SoH.)
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] Exception while processing incoming packet {}", e.what());
            SPDLOG_ERROR("[Anchor] Packet: {}", payload.dump());
        }

        isProcessingIncomingPacket = false;
    }

    if (pushAfterReplay) {
        try {
            SendPacket_UpdateTeamState();
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Anchor] MergeShip2: team state after the queue: {}", e.what());
        }
    }
}

// MARK: - Misc/Helpers

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

// Kills all existing anchor actors and respawns them with the new client data
void Anchor::RefreshClientActors() {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].first;

    while (actor != NULL) {
        if (actor->id == ANCHOR_DUMMY_ACTOR_ID && actor->update == DummyPlayer_Update) {
            NameTag_RemoveAllForActor(actor);
            Actor_Kill(actor);
        }
        actor = actor->next;
    }

    for (auto& [clientId, client] : clients) {
        client.player = nullptr;
        // (Hyrule's players are never in Termina's scenes: no dummy for them.)
        if (!client.online || client.self || !IsOwnWorld(client)) {
            continue;
        }

        spawningDummyPlayerForClientId = clientId;
        // ShouldActorInit (HookHandlers.cpp) turns the Player being spawned into a dummy: it records the client
        // id on it and swaps in the dummy's init/update/draw/destroy.
        auto dummy =
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_PLAYER, client.posRot.pos.x, client.posRot.pos.y,
                        client.posRot.pos.z, 0, client.posRot.rot.y, 0, 0);
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

    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return false;
    }

    return true;
}

bool Anchor::IsOwnWorld(const AnchorClient& client) {
    return client.world == MS_ANCHOR_WORLD_MM;
}

s16 Anchor::WireScene(s16 sceneId) {
    return (s16)(MS_ANCHOR_MM_SCENE_BASE + sceneId);
}

s16 Anchor::LocalScene(const AnchorClient& client) {
    if (!IsOwnWorld(client) || client.sceneNum < MS_ANCHOR_MM_SCENE_BASE) {
        return -1;
    }
    return (s16)(client.sceneNum - MS_ANCHOR_MM_SCENE_BASE);
}

bool Anchor::IsInOwnScene(const AnchorClient& client) {
    return IsSaveLoaded() && LocalScene(client) == gPlayState->sceneId;
}
