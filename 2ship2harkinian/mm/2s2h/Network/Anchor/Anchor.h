#pragma once

// MergeShip2 -- Anchor for 2 Ship 2 Harkinian (Majora's Mask), ported from Ship of Harkinian's soh/Network/Anchor.
//
// The same server, the same room and the same packets as SoH's Anchor, so Termina and Hyrule players share one room.
// What is Majora's Mask's own:
//   - players: the remote player is mirrored by a dummy Player built from Majora's Mask's Player (its form's skeleton,
//     its mask, the Goron ball, the Deku flower, the Zora fins), drawn only when the remote player is in Termina, in
//     the same scene (client state "world" = "mm", sceneNum = MS_ANCHOR_MM_SCENE_BASE + scene);
//   - items and flags: MM_GIVE_ITEM, MM_SET_FLAG / MM_UNSET_FLAG (week events, scene flags, randomizer flags),
//     MM_UPDATE_DUNGEON_ITEMS, MM_SET_CHECK_STATUS; cycle-aware: a flag that does not survive the Song of Time is
//     only applied by a player in the same three-day cycle (see Packets/SetFlag.cpp);
//   - team state: Majora's Mask's part of UPDATE_TEAM_STATE ("state"."mergeShipMM"), next to Hyrule's;
//   - items teammates find in Hyrule (GIVE_ITEM with "msItem") are received here through the shared save.
// In mergeship.exe the connection is the host's (include/mergeship/AnchorNet.h, MergeShipAnchorHost.cpp): crossing
// between the worlds never disconnects.

#ifdef __cplusplus

#include "2s2h/Network/Network.h"
#include <mergeship/AnchorNet.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "z64.h"
}

// The same CVars as SoH's Anchor (CVAR_REMOTE_ANCHOR in soh/cvar_prefixes.h): one set of settings for both games.
#ifndef CVAR_REMOTE_ANCHOR
#define CVAR_REMOTE_ANCHOR(var) "gRemote.Anchor." var
#endif
#define CVAR_ANCHOR_WINDOW "gOpenWindows.AnchorRoom"

void DummyPlayer_Init(Actor* actor, PlayState* play);
void DummyPlayer_Update(Actor* actor, PlayState* play);
void DummyPlayer_Draw(Actor* actor, PlayState* play);
void DummyPlayer_Destroy(Actor* actor, PlayState* play);

// The dummy's actor id once it is no longer ACTOR_PLAYER (an empty slot of the actor table, like SoH's En_Oe2).
#define ANCHOR_DUMMY_ACTOR_ID ACTOR_UNSET_13

#define ANCHOR_JOINT_COUNT PLAYER_LIMB_MAX

// MergeShip2: named, so its MergeShip fields can have default values (an unnamed typedef struct cannot).
typedef struct AnchorClient {
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
    s16 sceneNum; // as on the wire: a Termina player's is MS_ANCHOR_MM_SCENE_BASE + its scene
    s8 curRoomNum;
    s32 entranceIndex;
    std::string world; // MS_ANCHOR_WORLD_OOT / _MM ("oot" for a client that does not say)
    std::string sceneName;
    u16 cycle; // a Termina player's threeDayResetCount

    // Only available in PLAYER_UPDATE packets (Termina players)
    u8 form;
    u8 humanAdult; // MergeShip2: the human is Ocarina of Time's adult (MergeShipAdultLink.h); the ghost is built as it
    u8 ganondorf = 0;      // MergeShip2: Ganondorf mode is on for them: 1 + his form (MergeShipGanondorf.h), 0 off
    u32 ganondorfArmor = 0; // MergeShip2: his colours (0x01RRGGBB, 0: his own)
    u32 ganondorfCape = 0;
    s8 ganondorfBeastAnim = -1; // MergeShip2 (BUG-0111): Beast Ganon's animation and frame
    f32 ganondorfBeastFrame = 0.0f;
    PosRot posRot;
    alignas(16) Vec3s jointTable[ANCHOR_JOINT_COUNT + 2];
    u8 movementFlags;
    Vec3s prevTransl;
    Vec3s upperLimbRot;
    Vec3s headLimbRot;
    s8 currentBoots;
    s8 currentShield;
    u8 currentMask;
    u8 adultMask = 0; // MergeShip2 (BUG-0057): MergeShip_AdultMask_DrawingKind
    // MergeShip2 (BUG-0077): the Ocarina of Time equipment he wears (MergeShipOoTEquipSave's sword, shield, tunic,
    // boots, strength, bgsFlag, swordHealth), drawn on his dummy.
    bool hasOoTEquip = false;
    u8 ootSword = 0, ootShield = 0, ootTunic = 0, ootBoots = 0, ootStrength = 0, ootBgsFlag = 0;
    s16 ootSwordHealth = 0;
    u8 prevMask;
    u32 stateFlags1;
    u32 stateFlags2;
    u32 stateFlags3;
    u8 buttonItemB;
    u16 equipment;
    s8 itemAction;
    s8 heldItemAction;
    u8 heldItemId;
    u8 modelGroup;
    s8 invincibilityTimer;
    s8 actionVar1;
    s16 actionVar2;
    f32 unk_ABC;
    s16 unk_B86[2];
    s16 unk_B28;
    f32 unk_B10[6];
    f32 arr_AF0[6];
    s16 unk_B62;
    s16 unk_B8A[6]; // unk_B8A .. unk_B94
    Vec3f scale;
    f32 speedXZ;
    f32 velocityY;
    u8 ocarinaPitch;
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
    // MergeShip2: save-changing packets this engine handles that came while no Termina file was being played (the first
    // frames after the first arrival run before Play_Init, and every handler needs the save), in order;
    // ProcessIncomingPacketQueue puts them back in front of the queue once the file is loaded. Oldest dropped (logged)
    // past kMergeShipDeferredMax.
    std::deque<nlohmann::json> mergeShipDeferredPackets;
    static constexpr size_t kMergeShipDeferredMax = 4096;
    // MergeShip2: a team state's queue was replayed into incomingPacketQueue (HandlePacket_UpdateTeamState): once those
    // packets are handled, our team state goes out so the server clears the queue (game thread only).
    bool mergeShipReplayPushPending = false;
    // The game thread (ProcessIncomingPacketQueue and the always-on frame hook record it): the shared save is captured
    // only there (SendPacket_UpdateTeamState).
    std::thread::id mergeShipGameThread;

    // What the game last looked like (HookHandlers.cpp): changes are sent (scene, dungeon items, checks, ocarina).
    s32 lastStateSignature = -1;
    std::vector<u8> lastDungeonItems;
    std::vector<s8> lastDungeonKeys;
    std::vector<s8> lastStrayFairies;
    std::vector<u8> lastCheckObtained; // randomizer: per RandoCheckId, obtained | skipped << 1
    u8 lastOcarinaPitch = 0xFF;

    nlohmann::json PrepClientState();
    nlohmann::json PrepRoomState();
    nlohmann::json PrepPlayerUpdate();
    void RegisterHooks();
    void RefreshClientActors();
    void SetDummyPlayerClientId(const Actor* actor, uint32_t clientId);
    void PollGameState(); // once a frame, on the game thread
    void ResetPollState();

    void HandlePacket_AllClientState(nlohmann::json payload);
    void HandlePacket_DamagePlayer(nlohmann::json payload);
    void HandlePacket_DisableAnchor(nlohmann::json payload);
    void HandlePacket_GameComplete(nlohmann::json payload);
    void HandlePacket_GiveItem(nlohmann::json payload); // a Hyrule item (cross-world)
    void HandlePacket_MMGiveItem(nlohmann::json payload);
    void HandlePacket_OcarinaSfx(nlohmann::json payload);
    void HandlePacket_PlayerSfx(nlohmann::json payload);
    void HandlePacket_PlayerUpdate(nlohmann::json payload);
    void HandlePacket_RequestTeamState(nlohmann::json payload);
    void HandlePacket_RequestTeleport(nlohmann::json payload);
    void HandlePacket_ServerMessage(nlohmann::json payload);
    void HandlePacket_MMSetCheckStatus(nlohmann::json payload);
    void HandlePacket_MMSetFlag(nlohmann::json payload);
    void HandlePacket_MMUnsetFlag(nlohmann::json payload);
    void HandlePacket_TeleportTo(nlohmann::json payload);
    void HandlePacket_UpdateClientState(nlohmann::json payload);
    void HandlePacket_MMUpdateDungeonItems(nlohmann::json payload);
    // MergeShip2 (Packets/AmmoSync.cpp): the shared ammo pool converging across the team (AnchorNet.h).
    void HandlePacket_AmmoSync(nlohmann::json payload);
    void HandlePacket_UpdateRoomState(nlohmann::json payload);
    void HandlePacket_UpdateTeamState(nlohmann::json payload);

  protected:
    std::string PrepHello() override;

  public:
    uint32_t ownClientId = 0;
    // One version for both MergeShip2 games (AnchorNet.h).
    inline static const std::string clientVersion = MS_ANCHOR_CLIENT_VERSION;

    // Packet types //
    inline static const std::string ALL_CLIENT_STATE = "ALL_CLIENT_STATE";
    inline static const std::string DAMAGE_PLAYER = "DAMAGE_PLAYER";
    inline static const std::string DISABLE_ANCHOR = "DISABLE_ANCHOR";
    inline static const std::string GAME_COMPLETE = "GAME_COMPLETE";
    inline static const std::string GIVE_ITEM = "GIVE_ITEM";
    inline static const std::string HANDSHAKE = "HANDSHAKE";
    inline static const std::string OCARINA_SFX = "OCARINA_SFX";
    inline static const std::string PLAYER_SFX = "PLAYER_SFX";
    inline static const std::string PLAYER_UPDATE = "PLAYER_UPDATE";
    inline static const std::string REQUEST_TEAM_STATE = "REQUEST_TEAM_STATE";
    inline static const std::string REQUEST_TELEPORT = "REQUEST_TELEPORT";
    inline static const std::string SERVER_MESSAGE = "SERVER_MESSAGE";
    inline static const std::string TELEPORT_TO = "TELEPORT_TO";
    inline static const std::string UPDATE_CLIENT_STATE = "UPDATE_CLIENT_STATE";
    inline static const std::string UPDATE_ROOM_STATE = "UPDATE_ROOM_STATE";
    inline static const std::string UPDATE_TEAM_STATE = "UPDATE_TEAM_STATE";
    inline static const std::string MM_GIVE_ITEM = MS_ANCHOR_MM_GIVE_ITEM;
    inline static const std::string MM_SET_FLAG = MS_ANCHOR_MM_SET_FLAG;
    inline static const std::string MM_UNSET_FLAG = MS_ANCHOR_MM_UNSET_FLAG;
    inline static const std::string MM_UPDATE_DUNGEON_ITEMS = MS_ANCHOR_MM_UPDATE_DUNGEON_ITEMS;
    inline static const std::string MM_SET_CHECK_STATUS = MS_ANCHOR_MM_SET_CHECK_STATUS;

    static Anchor* Instance;
    std::map<uint32_t, AnchorClient> clients;
    RoomState roomState = {};
    // Hyrule's part of the team state, carried along so one team state on the server holds both worlds: the last one
    // received of this playthrough, cleared when a file is loaded (HookHandlers.cpp), sent only while it still is
    // (MergeShipAnchor_OoTTeamState).
    nlohmann::json ootTeamState;
    // MergeShip2: the join window. A REQUEST_TEAM_STATE of ours (Packets/RequestTeamState.cpp) is answered by every
    // teammate online, or by the server with the team's stored state; every answer that arrives (MS_ANCHOR_KEY_RECV_MS)
    // within kMergeShipJoinWindowMs after the request is a join answer (MergeShipIsJoinAnswer): the joiner also raises
    // the counters that go down as they are spent (rupees, small keys) to the team's, never lowering his own (the
    // server's stored state may be his own older one).
    std::atomic<bool> mergeShipAwaitingTeamState{ false };
    std::atomic<uint64_t> mergeShipTeamStateRequestMs{ 0 };
    // An answer in this window was another playthrough's and ours was sent as the team's
    // (HandlePacket_UpdateTeamState): once per request.
    std::atomic<bool> mergeShipSentOwnTeamState{ false };
    static constexpr uint64_t kMergeShipJoinWindowMs = 20000;
    bool MergeShipIsJoinAnswer(const nlohmann::json& payload);
    // The caller runs on the game thread (mergeShipGameThread).
    bool MergeShipOnGameThread() const {
        return std::this_thread::get_id() == mergeShipGameThread;
    }
    void MergeShipNoteGameThread() {
        mergeShipGameThread = std::this_thread::get_id();
    }

    void Enable();
    void Disable();
    void OnIncomingJson(nlohmann::json payload) override;
    void OnConnected() override;
    void OnDisconnected() override;
    void ProcessOutgoingPackets() override;
    void ProcessIncomingPacketQueue();
    void SendJsonToRemote(nlohmann::json packet) override;
    bool IsSaveLoaded();
    bool CanTeleportTo(uint32_t clientId);
    uint32_t GetDummyPlayerClientId(const Actor* actor);
    // The client is in Termina, in the scene the player is in (its dummy is drawn).
    bool IsInOwnScene(const AnchorClient& client);
    static bool IsOwnWorld(const AnchorClient& client);
    // This client's scene number on the wire (MS_ANCHOR_MM_SCENE_BASE + scene), and back.
    static s16 WireScene(s16 sceneId);
    static s16 LocalScene(const AnchorClient& client);
    std::string SceneName(s16 sceneId);
    bool IsProcessingIncomingPacket() const {
        return isProcessingIncomingPacket;
    }
    // The dummies are killed and spawned again on the next frame (a remote player changed form).
    void RequestActorRefresh() {
        shouldRefreshActors = true;
    }

    // Majora's Mask's part of UPDATE_TEAM_STATE ("state"."mergeShipMM"). ApplyTeamState merges a teammate's into this
    // save (`join`: the answer to our own request, MergeShipIsJoinAnswer) and is true when anything changed.
    nlohmann::json PrepTeamState();
    bool ApplyTeamState(const nlohmann::json& mm, bool join);
    nlohmann::json PrepHandshake();

    // Test harness (MergeShipAnchorHost.cpp): the local player mirrored as another Anchor player.
    bool mergeShipLoopback = false;
    void MergeShipLoopbackStart(Color_RGB8 color, f32 offsetX, f32 offsetZ);
    void MergeShipLoopbackFrame();

    void SendPacket_ClearTeamState(std::string teamId);
    void SendPacket_DamagePlayer(u32 clientId, u8 damageEffect, u8 damage);
    void SendPacket_GameComplete();
    // resolvedItemId: the concrete item randoItemId's check actually gave (Rando::MergeShip::ResolvedItemOf), for a
    // teammate in the other world's "msItem" -- a progressive item's placed pool id (randoItemId) alone does not say
    // which step was given, and only a same-world teammate can convert it himself ("ri", GiveItem.cpp). -1 (default):
    // randoItemId itself, for a plain item or a vanilla give (mmItem, randoItemId < 0), where they are the same.
    void SendPacket_MMGiveItem(s32 randoItemId, u8 mmItem, s32 resolvedItemId = -1);
    void SendPacket_Handshake();
    void SendPacket_OcarinaSfx(uint8_t pitch, float modulator, int8_t bend);
    void SendPacket_PlayerSfx(u16 sfxId);
    void SendPacket_PlayerUpdate();
    void SendPacket_RequestTeamState();
    void SendPacket_RequestTeleport(u32 clientId);
    void SendPacket_MMSetCheckStatus(s32 randoCheckId, bool obtained, bool skipped);
    void SendPacket_MMSetFlag(s16 sceneId, s16 flagType, s32 flag);
    void SendPacket_TeleportTo(u32 clientId);
    void SendPacket_MMUnsetFlag(s16 sceneId, s16 flagType, s32 flag);
    void SendPacket_UpdateClientState();
    void SendPacket_MMUpdateDungeonItems();
    void SendPacket_UpdateRoomState();
    void SendPacket_UpdateTeamState();

    // MergeShip2 (Packets/AmmoSync.cpp): the shared ammo pool converging across the team (AnchorNet.h). Sends one
    // MsOwnBit's new count when it rises since the last send; called every OnPlayerUpdate.
    void SendPacket_AmmoSync(s32 ownBit, s32 count);
    void MergeShip_CheckAmmoSync();
    // MergeShip2 (Packets/SetCheckStatus.cpp): a check's status set outside Anchor (the team's Termina checks from the
    // shared save) becomes the last known one, so PollGameState does not send it as this player's own find.
    void MergeShip_NoteCheckStatus(s32 randoCheckId);
};

typedef enum {
    // The dummy's own damage effects (a DamageTable entry holds 4 bits of effect: the top three values, which only
    // the dummy's table uses; SoH continues Ocarina of Time's response values instead)
    DUMMY_PLAYER_HIT_RESPONSE_STUN = 0xD,
    DUMMY_PLAYER_HIT_RESPONSE_FIRE = 0xE,
    DUMMY_PLAYER_HIT_RESPONSE_NORMAL = 0xF,
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
