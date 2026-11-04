#pragma once

// MergeShip2 -- 2 Ship 2 Harkinian's Anchor glue (MergeShipAnchorHost.cpp): the host's one connection shared with
// Hyrule (include/mergeship/AnchorNet.h), items that cross between the worlds, and the test harness. The counterpart
// of SoH's soh/Network/Anchor/MergeShipAnchorHost.cpp.

#ifdef __cplusplus

#include <nlohmann/json.hpp>
#include <string>

extern "C" {
#include "z64.h"
}

// Internal (never sent): when this engine received a packet, MergeShipAnchor_NowMs (Anchor::OnIncomingJson). A team
// state counts as the answer to our own request by when it arrived, not by when it is handled: one that waited while
// Termina was parked, or until a file was played, came before the request and is no join answer.
#ifndef MS_ANCHOR_KEY_RECV_MS
#define MS_ANCHOR_KEY_RECV_MS "__msRecvMs"
#endif

class Anchor;
struct MsSharedSave;
// Milliseconds on a clock that only goes forward (the join window; MS_ANCHOR_KEY_RECV_MS).
uint64_t MergeShipAnchor_NowMs();
// Anchor::Enable, first thing: become a view of the host's connection (once), if there is one and
// MS_CVAR_ANCHOR_SHARED_CONNECTION is on.
void MergeShipAnchor_AttachHost(Anchor* anchor);
// Anchor::Enable, before connecting: when Hyrule already connected, adopt its room settings; otherwise publish ours.
void MergeShipAnchor_BeforeEnable(Anchor* anchor);
// BUG-0063/0064/0078: the room's state, received here, for the other world's engine (adopted on its resume).
void MergeShipAnchor_ShareRoomState(const nlohmann::json& state);
// Anchor::OnIncomingJson (network thread): false drops the packet (per-frame packets while this engine is parked).
bool MergeShipAnchor_KeepIncoming(Anchor* anchor, const std::string& packetType);
// The host runs this engine now (always true in 2ship.exe).
bool MergeShipAnchor_IsActiveEngine();
// Once-per-process packets (AnchorNet.h, Claim).
bool MergeShipAnchor_Claim(const nlohmann::json& payload);
// Remembers `id` as this process's (MS_ANCHOR_BLOB_OWN_IDS); Claim refuses packets from any of them.
void MergeShipAnchor_NoteOwnClientId(uint32_t id);
bool MergeShipAnchor_CanClaim(const nlohmann::json& payload);
// The packet is our own, echoed back by the server (it came from one of this process's client ids).
bool MergeShipAnchor_IsOwnPacket(const nlohmann::json& payload);
// UPDATE_TEAM_STATE's shared-save part (AnchorNet.h MS_ANCHOR_CLAIM_SHARED) is merged once per process: true for the
// first engine that claims it, always for a packet that did not come through the host. Never our own echo.
bool MergeShipAnchor_ClaimShared(const nlohmann::json& payload);
// Termina's part of the team state (Anchor::PrepTeamState) kept for Hyrule's engine, as {fileId, seed, state} (SoH
// MergeShipAnchorHost.cpp UnwrapTeamMM): the MS_ANCHOR_BLOB_TEAM_MM blob, and Save/mergeship_team_mm_<slot>.json for
// the next run. Only for a Termina file that belongs to a Hyrule file (MergeShipAnchor_LinkedSharedSave); with another
// Termina file loaded the blob is erased. Game thread.
void MergeShipAnchor_PersistTerminaTeamState(Anchor* anchor);
// A teammate's item is announced once per process, even when one engine leaves it to the other (AnchorNet.h,
// MS_ANCHOR_CLAIM_ANNOUNCED): true when this engine shows its notification.
bool MergeShipAnchor_Announce(const nlohmann::json& payload);
// Hyrule's part of the team state: from SoH in this process (hosted, the Hyrule file this Termina belongs to), else the
// last one received; either only of this playthrough (MergeShipAnchor_LocalSeed), or nothing.
nlohmann::json MergeShipAnchor_OoTTeamState(Anchor* anchor);
// The shared save of the Hyrule file the loaded Termina file belongs to (MergeShipHost_SharedSave), as Anchor sees it:
// NULL when the Termina file is not linked, or when that Hyrule slot has started another playthrough since the player
// arrived (its Termina file is the earlier one's until the next trip remakes it). Every part Anchor sends or merges for
// "this playthrough" goes through it.
MsSharedSave* MergeShipAnchor_LinkedSharedSave();
// The playthrough (AnchorNet.h MS_ANCHOR_KEY_SEED): for a linked Termina file (MergeShipAnchor_LinkedSharedSave),
// Hyrule's number as SoH published it when the player arrived from that file; otherwise (2ship.exe, or a Termina file
// of Majora's Mask's own) 2Ship's own randomizer seed, 0 for a file that is not a randomizer one.
uint32_t MergeShipAnchor_LocalSeed();
// Anchor::ProcessIncomingPacketQueue, before a packet is handled: false drops a save-changing packet of another
// playthrough (logged). A team state passes: its handler checks its "state" (MergeShipAnchor_TeamStateSeedAccepts)
// and replays its queue either way, each queued packet checked on its own.
bool MergeShipAnchor_SeedAccepts(Anchor* anchor, const nlohmann::json& payload);
// UPDATE_TEAM_STATE: its "state" is this playthrough's (or empty). False is logged.
bool MergeShipAnchor_TeamStateSeedAccepts(const nlohmann::json& payload);
// "ANCHOR <why>: ..." to spdlog, the test trace and the host's log.
void MergeShipAnchor_LogState(const char* why);
// A status line for the Anchor menu ("" in 2ship.exe).
std::string MergeShipAnchor_ConnectionSummary();

// Canonical items (include/mergeship/MsItems.def).
// The MsItems.def key name ("HOOKSHOT") of a 2Ship RandoItemId, "" when it has none.
std::string MergeShipAnchor_MsItemNameOfRI(s32 randoItemId);
// MsItemKey (as int) of a key name, -1 when unknown.
s32 MergeShipAnchor_MsItemKeyOfName(const std::string& name);
// Display name and whether the item is junk, for notifications.
std::string MergeShipAnchor_MsItemDisplayName(s32 msItemKey);
bool MergeShipAnchor_MsItemIsJunk(s32 msItemKey);
// The 2Ship RandoItemId of an MsItemKey, -1 when unknown (Packets/GiveItem.cpp: Rando::MergeShip::OriginTag).
s32 MergeShipAnchor_MsItemRIOfKey(s32 msItemKey);
// Offers a teammate's item from the other world to the shared-save projector (AnchorNet.h, "Cross-world items").
bool MergeShipAnchor_ReceiveRemoteItem(s32 msItemKey);

// A randomizer check's status set outside Anchor (Rando::MergeShip::ApplyTeamChecks): Anchor takes it as known and
// does not send it as this player's own find (Packets/SetCheckStatus.cpp).
void MergeShipAnchor_NoteCheckStatus(s32 randoCheckId);

// Majora's Mask's cycle rules (Packets/SetFlag.cpp): does the flag survive a Song of Time?
bool MergeShipAnchor_FlagPersists(s16 sceneId, s16 flagType, s32 flag);
u16 MergeShipAnchor_Cycle();

// HookHandlers.cpp: plays a teammate's sound on his dummy without sending it back.
void MergeShipAnchor_PlayRemoteSfx(Player* dummy, u16 sfxId);

// Text chat (MergeShipAnchorChat.cpp; include/mergeship/AnchorChat.hpp).
// Anchor::ProcessIncomingPacketQueue: an MS_ANCHOR_CHAT packet.
void MergeShipAnchor_HandleChat(Anchor* anchor, const nlohmann::json& payload);
// Menu.cpp RegisterAnchorMenu: the chat box window and the messages above heads.
void MergeShipAnchor_RegisterChat();

extern "C" {
#endif

struct MsHostServices;
// EngineExport.cpp (mergeship.exe): the host's services, before the game starts.
void MergeShipAnchor_SetHostServices(const struct MsHostServices* services);
// EngineExport.cpp: the game has started: create Anchor and follow the host's connection from now on.
void MergeShipAnchor_OnEngineReady(void);
// EngineExport.cpp: MsEngine_Suspend / MsEngine_Resume.
void MergeShipAnchor_OnEngineSuspend(void);
void MergeShipAnchor_OnEngineResume(void);
// MergeShipHost_OnResume (mergeship.exe): the player arrives from the Hyrule file whose shared save is `hyruleFileId`;
// Termina's file of that slot belongs to it, and to the playthrough SoH published as it left (MS_ANCHOR_BLOB_SEED),
// whatever SoH loads later (MergeShipAnchor_LocalSeed).
void MergeShipAnchor_OnHyruleArrival(u32 hyruleFileId);
// Test harness: shows the local player mirrored as another Anchor player (colour r,g,b, offset dx,dz).
void MergeShipAnchor_TestLoopback(u8 r, u8 g, u8 b, f32 dx, f32 dz);
// Test harness (MergeShipHostTest.cpp): "anchor <command ...>", see MergeShipAnchorHost.cpp.
void MergeShipAnchor_TestCommand(const char* args);

#ifdef __cplusplus
}
#endif
