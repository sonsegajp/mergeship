#pragma once

#ifdef __cplusplus

#include "soh/Network/Network.h"
#include <mergeship/AnchorNet.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <queue>
#include <mutex>
#include <unordered_set>

extern "C" {
#include "variables.h"
#include "z64.h"
}

void DummyPlayer_Init(Actor* actor, PlayState* play);
void DummyPlayer_Update(Actor* actor, PlayState* play);
void DummyPlayer_Draw(Actor* actor, PlayState* play);
void DummyPlayer_Destroy(Actor* actor, PlayState* play);

typedef struct {
    uint32_t clientId;
    std::string name;
    Color_RGB8 color;
    std::string clientVersion;
    std::string teamId;
    bool online;
    bool self;
    uint32_t seed;
    bool isSaveLoaded;
    bool isGameComplete;
    s16 sceneNum;
    s8 curRoomNum;
    s32 entranceIndex;
    // MergeShip2: which game the player is in (MS_ANCHOR_WORLD_OOT / _MM; a stock SoH client says nothing: "oot"),
    // and the name of their scene, so a player in the other world is shown with a place.
    std::string world;
    std::string sceneName;

    // Only available in PLAYER_UPDATE packets
    s32 linkAge;
    PosRot posRot;
    Vec3s jointTable[24];
    u8 movementFlags;
    Vec3s prevTransl;
    Vec3s upperLimbRot;
    s8 currentBoots;
    s8 currentShield;
    s8 currentTunic;
    u8 ganondorf; // MergeShip2: Ganondorf mode is on for them (MergeShipGanondorf.h)
    u32 ganondorfArmor; // MergeShip2: and his colours (0: as they are)
    u32 ganondorfCape;
    s8 ganondorfBeastAnim; // MergeShip2 (BUG-0111): Beast Ganon's animation (-1: none) and frame
    f32 ganondorfBeastFrame;
    u32 stateFlags1;
    u32 stateFlags2;
    u8 buttonItem0;
    s8 itemAction;
    s8 heldItemAction;
    u8 modelGroup;
    s8 invincibilityTimer;
    f32 unk_85C;
    s16 unk_862;
    s8 actionVar1;
    u8 ocarinaNote;
    f32 ocarinaModulator;
    s8 ocarinaBend;

    // Ptr to the dummy player
    Player* player;
} AnchorClient;

typedef struct {
    uint32_t ownerClientId;
    u8 pvpMode;           // 0 = off, 1 = on, 2 = on with friendly fire
    u8 showLocationsMode; // 0 = none, 1 = team, 2 = all
    u8 teleportMode;      // 0 = off, 1 = team, 2 = all
    u8 syncItemsAndFlags; // 0 = off, 1 = on
} RoomState;

class Anchor : public Network {
  private:
    uint32_t spawningDummyPlayerForClientId = 0;
    bool shouldRefreshActors = false;
    bool justLoadedSave = false;
    bool isHandlingUpdateTeamState = false;
    bool isProcessingIncomingPacket = false;
    std::queue<nlohmann::json> incomingPacketQueue;
    std::mutex incomingPacketQueueMutex;
    std::queue<nlohmann::json> outgoingPacketQueue;
    std::mutex outgoingPacketQueueMutex;
    // MergeShip2 (ProcessIncomingPacketQueue): packets that change the save, received while no file is loaded (title
    // screen, file select, a scene load), in the order they came; applied, oldest first, once a file is. Game thread
    // only.
    std::deque<nlohmann::json> mergeShipDeferredPackets;
    static constexpr size_t kMergeShipDeferredMax = 4096;
    // MergeShip2 (ProcessIncomingPacketQueue): the replayed team queue entries (AnchorReplay.hpp ids) this engine has
    // handled. The server hands the same queue to every request until someone's team state clears it (SoH's and
    // 2Ship's own requests, a reconnect's), and what only this engine applies (flags, keys, beans, entrances) is not
    // claimed from the host: each entry is handled once per process. Game thread only.
    std::unordered_set<uint64_t> mergeShipReplayed;
    // MergeShip2: set by a save (OnSaveFile runs on SaveManager's worker thread), sent on the next game frame: the
    // team state reads the save and the shared save, which only the game thread may touch.
    std::atomic<bool> mergeShipSaveSendPending{ false };

    nlohmann::json PrepClientState();
    nlohmann::json PrepRoomState();
    nlohmann::json PrepPlayerUpdate(); // MergeShip2: the PLAYER_UPDATE payload (also fed back by the loopback)
    void RegisterHooks();
    void RefreshClientActors();
    void SetDummyPlayerClientId(const Actor* actor, uint32_t clientId);

    void HandlePacket_AllClientState(nlohmann::json payload);
    void HandlePacket_ConsumeAdultTradeItem(nlohmann::json payload);
    void HandlePacket_DamagePlayer(nlohmann::json payload);
    void HandlePacket_DisableAnchor(nlohmann::json payload);
    void HandlePacket_EntranceDiscovered(nlohmann::json payload);
    void HandlePacket_GameComplete(nlohmann::json payload);
    void HandlePacket_GiveItem(nlohmann::json payload);
    void HandlePacket_OcarinaSfx(nlohmann::json payload);
    void HandlePacket_PlayerSfx(nlohmann::json payload);
    void HandlePacket_PlayerUpdate(nlohmann::json payload);
    void HandlePacket_RequestTeamState(nlohmann::json payload);
    void HandlePacket_RequestTeleport(nlohmann::json payload);
    void HandlePacket_ServerMessage(nlohmann::json payload);
    void HandlePacket_SetCheckStatus(nlohmann::json payload);
    void HandlePacket_SetFlag(nlohmann::json payload);
    void HandlePacket_TeleportTo(nlohmann::json payload);
    void HandlePacket_UnsetFlag(nlohmann::json payload);
    void HandlePacket_UpdateBeansCount(nlohmann::json payload);
    void HandlePacket_UpdateClientState(nlohmann::json payload);
    void HandlePacket_UpdateDungeonItems(nlohmann::json payload);
    void HandlePacket_UpdateRoomState(nlohmann::json payload);
    void HandlePacket_UpdateTeamState(nlohmann::json payload);
    // MergeShip2 (MergeShipAnchorHost.cpp): an item a teammate found in Termina.
    void HandlePacket_MMGiveItem(nlohmann::json payload);
    // MergeShip2 (Packets/AmmoSync.cpp): the shared ammo pool converging across the team (AnchorNet.h).
    void HandlePacket_AmmoSync(nlohmann::json payload);
    // MergeShip2 (Packets/SetCheckStatus.cpp): a Termina check a teammate obtained, kept in the shared save.
    void HandlePacket_MMSetCheckStatus(nlohmann::json payload);

  protected:
    // MergeShip2: the HANDSHAKE the host sends first on every (re)connect of its shared connection.
    std::string PrepHello() override;

  public:
    uint32_t ownClientId;
    // MergeShip2: both engines report one version (SoH's and 2Ship's git hashes differ, and Anchor ignores most
    // packets from another version), so Hyrule and Termina players can play together.
    inline static const std::string clientVersion = MS_ANCHOR_CLIENT_VERSION;

    // Packet types //
    inline static const std::string ALL_CLIENT_STATE = "ALL_CLIENT_STATE";
    inline static const std::string DAMAGE_PLAYER = "DAMAGE_PLAYER";
    inline static const std::string DISABLE_ANCHOR = "DISABLE_ANCHOR";
    inline static const std::string ENTRANCE_DISCOVERED = "ENTRANCE_DISCOVERED";
    inline static const std::string GAME_COMPLETE = "GAME_COMPLETE";
    inline static const std::string GIVE_ITEM = "GIVE_ITEM";
    inline static const std::string HANDSHAKE = "HANDSHAKE";
    inline static const std::string OCARINA_SFX = "OCARINA_SFX";
    inline static const std::string PLAYER_SFX = "PLAYER_SFX";
    inline static const std::string PLAYER_UPDATE = "PLAYER_UPDATE";
    inline static const std::string REQUEST_TEAM_STATE = "REQUEST_TEAM_STATE";
    inline static const std::string REQUEST_TELEPORT = "REQUEST_TELEPORT";
    inline static const std::string SERVER_MESSAGE = "SERVER_MESSAGE";
    inline static const std::string SET_CHECK_STATUS = "SET_CHECK_STATUS";
    inline static const std::string SET_FLAG = "SET_FLAG";
    inline static const std::string TELEPORT_TO = "TELEPORT_TO";
    inline static const std::string UNSET_FLAG = "UNSET_FLAG";
    inline static const std::string UPDATE_BEANS_COUNT = "UPDATE_BEANS_COUNT";
    inline static const std::string UPDATE_CLIENT_STATE = "UPDATE_CLIENT_STATE";
    inline static const std::string UPDATE_DUNGEON_ITEMS = "UPDATE_DUNGEON_ITEMS";
    inline static const std::string UPDATE_ROOM_STATE = "UPDATE_ROOM_STATE";
    inline static const std::string UPDATE_TEAM_STATE = "UPDATE_TEAM_STATE";

    static Anchor* Instance;
    std::map<uint32_t, AnchorClient> clients;
    RoomState roomState;

    void Enable();
    void Disable();
    void OnIncomingJson(nlohmann::json payload);
    void OnConnected();
    void OnDisconnected();
    void ProcessOutgoingPackets();
    void DrawMenu();
    void ProcessIncomingPacketQueue();
    void SendJsonToRemote(nlohmann::json packet);
    bool IsSaveLoaded();
    bool CanTeleportTo(uint32_t clientId);
    uint32_t GetDummyPlayerClientId(const Actor* actor);

    // MergeShip2 (MergeShipAnchor.cpp): a local mirror of this player, drawn as another Anchor player through the
    // PLAYER_UPDATE packet path, for testing without a server.
    bool mergeShipLoopback = false;
    void MergeShipLoopbackStart(Color_RGB8 color, f32 offsetX, f32 offsetZ);
    void MergeShipLoopbackFrame();
    // MergeShip2 (MergeShipAnchorHost.cpp): one connection for both worlds.
    // The client is in this game's world (not a Termina player).
    static bool IsOwnWorld(const AnchorClient& client);
    // SaveContext's part of UPDATE_TEAM_STATE ("state", without Termina's part).
    nlohmann::json PrepTeamState();
    // Termina's part of the team state, carried along so one team state on the server holds both worlds: the last one
    // received, and the file (MergeShip_Save_SharedFileId) and playthrough (MergeShipAnchor_LocalSeed) it was received
    // for -- sent only for those (MergeShipAnchor_MMTeamState). Kept under its lock, though every send now runs on the
    // game thread (a save's waits for the next frame).
    nlohmann::json mergeShipMMTeamState;
    uint32_t mergeShipMMTeamStateFileId = 0;
    uint32_t mergeShipMMTeamStateSeed = 0;
    std::mutex mergeShipMMTeamStateMutex;
    // A REQUEST_TEAM_STATE of ours, sent at mergeShipTeamStateRequestMs (MergeShipNowMs): every team state that
    // ARRIVED within kMergeShipJoinWindowMs after it is an answer (each teammate online sends one), the state of a team
    // this player is joining (MergeShipIsJoinAnswer). Also HandlePacket_UpdateTeamState: an answer of another
    // playthrough makes ours the team's, once per request (mergeShipPushedOwnTeamState). Atomic: a request goes out
    // from the host link's thread too (OnConnected), while the game thread reads them.
    std::atomic<bool> mergeShipAwaitingTeamState{ false };
    std::atomic<int64_t> mergeShipTeamStateRequestMs{ 0 };
    std::atomic<bool> mergeShipPushedOwnTeamState{ false };
    static constexpr int64_t kMergeShipJoinWindowMs = 20000;
    // The steady clock in milliseconds (OnIncomingJson stamps each packet's arrival with it, kMergeShipRecvMsKey).
    static int64_t MergeShipNowMs();
    // Internal (never sent): when OnIncomingJson received the packet. A packet waits (no file, the player busy) and is
    // handled later; whether it answers our request is decided by when it came, not when it is handled -- a team state
    // that waited from before the request is a teammate's old save, not the team's answer.
    inline static const char* const kMergeShipRecvMsKey = "__msRecvMs";
    // Internal packet type (never sent): queued after a replayed team queue (HandlePacket_UpdateTeamState), so this
    // player's team state goes out once the replayed packets are applied, and the server clears its queue.
    inline static const std::string kMergeShipReplayDone = "__MS_REPLAY_DONE";
    // True when `payload`, a team state, arrived within kMergeShipJoinWindowMs after our request.
    bool MergeShipIsJoinAnswer(const nlohmann::json& payload);
    nlohmann::json PrepHandshake();

    void SendPacket_ClearTeamState(std::string teamId);
    void SendPacket_DamagePlayer(u32 clientId, u8 damageEffect, u8 damage);
    void SendPacket_EntranceDiscovered(u16 entranceIndex);
    void SendPacket_GameComplete();
    void SendPacket_GiveItem(u16 modId, s16 getItemId);
    void SendPacket_Handshake();
    void SendPacket_OcarinaSfx(uint8_t note, float modulator, int8_t bend);
    void SendPacket_PlayerSfx(u16 sfxId);
    void SendPacket_PlayerUpdate();
    void SendPacket_RequestTeamState();
    void SendPacket_RequestTeleport(u32 clientId);
    void SendPacket_SetCheckStatus(RandomizerCheck rc);
    void SendPacket_SetFlag(s16 sceneNum, s16 flagType, s16 flag);
    void SendPacket_TeleportTo(u32 clientId);
    void SendPacket_UnsetFlag(s16 sceneNum, s16 flagType, s16 flag);
    void SendPacket_UpdateBeansCount();
    void SendPacket_UpdateClientState();
    void SendPacket_UpdateDungeonItems();
    void SendPacket_UpdateRoomState();
    // MergeShip2: game thread only (a save's send waits for the next frame, mergeShipSaveSendPending). captureShared:
    // the shared save is captured into its copy first, so the copy sent is this moment's.
    void SendPacket_UpdateTeamState(bool captureShared = false);

    // MergeShip2 (Packets/AmmoSync.cpp): the shared ammo pool converging across the team (AnchorNet.h). Sends one
    // MsOwnBit's new count when it rises since the last send; called every OnPlayerUpdate.
    void SendPacket_AmmoSync(s32 ownBit, s32 count);
    void MergeShip_CheckAmmoSync();
};

typedef enum {
    // Starting at 5 to continue from the last value in the PlayerDamageResponseType enum
    DUMMY_PLAYER_HIT_RESPONSE_STUN = 5,
    DUMMY_PLAYER_HIT_RESPONSE_FIRE,
    DUMMY_PLAYER_HIT_RESPONSE_NORMAL,
} DummyPlayerDamageResponseType;

class AnchorRoomWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override;
    void Draw() override;
    void UpdateElement() override{};
};

#endif // __cplusplus
