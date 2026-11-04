#pragma once

// MergeShip2 -- Anchor players in Majora's Mask's transformation forms (MergeShipAnchor.cpp).
//
// PLAYER_UPDATE carries a "mergeShip" object with the sender's form and the state the forms' drawing reads;
// the dummy Player that mirrors a remote player is rebuilt as that form (its own skeleton, property row and
// hand/model lists), sized and posed from it, and drawn in the remote player's Anchor colour.
// Gated by CVAR_ENHANCEMENT("MergeShip.AnchorForms") (default on).

#ifdef __cplusplus

#include <nlohmann/json.hpp>

extern "C" {
#include "z64.h"
}

// Sender: adds the form and its drawing state to a PLAYER_UPDATE payload.
void MergeShipAnchor_WritePlayerUpdate(nlohmann::json& payload, Player* player);
// Receiver: keeps them for `clientId` (a payload without them is a player in Link's own form).
void MergeShipAnchor_ReadPlayerUpdate(uint32_t clientId, const nlohmann::json& payload);

// DummyPlayer_Init, after Player_InitCommon built Link at the client's age.
void MergeShipAnchor_DummyInit(Player* dummy, PlayState* play, uint32_t clientId, s32 linkAge);
// DummyPlayer_Update, after Anchor's own fields are copied: form, size and the forms' drawing state.
void MergeShipAnchor_DummyUpdate(Player* dummy, PlayState* play, uint32_t clientId, s32 linkAge);
// The x/z factor Player_ApplyAnimMovementScaledByAge applies to root motion for the dummy's form.
f32 MergeShipAnchor_DummyRootScaleXZ(Player* dummy, s32 linkAge);
// Around DummyPlayer_Draw's Player_Draw. Begin returns false when the remote player is not drawn at all
// (e.g. rebuilt behind the transformation's flash); End must still be called.
bool MergeShipAnchor_DummyBeginDraw(Player* dummy, PlayState* play, uint32_t clientId);
void MergeShipAnchor_DummyEndDraw(Player* dummy, PlayState* play);

// ---- one connection for both worlds (MergeShipAnchorHost.cpp; include/mergeship/AnchorNet.h) -------------------------
//
// In mergeship.exe the Anchor socket is the host's: Hyrule and Termina are two views of one client in the room, so
// crossing between them never disconnects. Standalone soh.exe keeps its own SDL_net connection (none of this applies).

class Anchor;
// Anchor::Enable, first thing: become a view of the host's connection (once), if there is one and
// MS_CVAR_ANCHOR_SHARED_CONNECTION is on.
void MergeShipAnchor_AttachHost(Anchor* anchor);
// Anchor::Enable, before connecting: when the other world already connected, adopt its room settings (host, port,
// room, name, team, colour) and client id; otherwise publish ours for it.
void MergeShipAnchor_BeforeEnable(Anchor* anchor);
// BUG-0063/0064/0078: the room's state, received here, for the other world's engine (adopted on its resume).
void MergeShipAnchor_ShareRoomState(const nlohmann::json& state);
// Logs Anchor's connection state (spdlog, the test trace and the host's log): "ANCHOR <why>: ...".
void MergeShipAnchor_LogState(const char* why);
// Anchor::OnIncomingJson (network thread): false drops the packet (per-frame packets while this engine is parked).
bool MergeShipAnchor_KeepIncoming(Anchor* anchor, const std::string& packetType);
// The host runs this engine now (always true standalone).
bool MergeShipAnchor_IsActiveEngine();
// Once-per-process packets: true when this engine applies `payload` (AnchorNet.h, Claim).
bool MergeShipAnchor_Claim(const nlohmann::json& payload);
// Remembers `id` as this process's (MS_ANCHOR_BLOB_OWN_IDS); Claim refuses packets from any of them.
void MergeShipAnchor_NoteOwnClientId(uint32_t id);
bool MergeShipAnchor_CanClaim(const nlohmann::json& payload);
// UPDATE_TEAM_STATE's "mergeShipShared" part is merged into this process's one shared save by one engine: true for the
// first that claims it (AnchorNet.h MS_ANCHOR_CLAIM_SHARED). A packet that did not come through the host is always
// ours.
bool MergeShipAnchor_ClaimShared(const nlohmann::json& payload);
// The packet is this process's own, echoed back (sent with one of its client ids, by either engine).
bool MergeShipAnchor_IsOwnEcho(const nlohmann::json& payload);
// The canonical item name (MsItems.def key) of a GIVE_ITEM, "" when it has none.
std::string MergeShipAnchor_MsItemName(u16 modId, s16 getItemId);
// Termina's part of the team state for the loaded file and playthrough, or an empty json: Termina's engine's latest
// (hosted), else the one 2Ship left on disk (Save/mergeship_team_mm_<slot>.json), else the last one received.
nlohmann::json MergeShipAnchor_MMTeamState(Anchor* anchor);
// A teammate's Termina part (`theirMM`, "mergeShipMM") is from the three-day cycle this player's own Termina is in
// (its "cycle", threeDayResetCount). False when either is unknown (this player's Termina part is its own engine's: the
// host blob or the file 2Ship wrote, never a teammate's copy): what a Song of Time takes away is then not taken.
bool MergeShipAnchor_SameTerminaCycle(const nlohmann::json& theirMM);
// The playthrough this file belongs to (AnchorNet.h MS_ANCHOR_KEY_SEED), stamped on every packet; published for
// Termina's engine (MS_ANCHOR_BLOB_SEED).
uint32_t MergeShipAnchor_LocalSeed();
// Anchor::ProcessIncomingPacketQueue, before a packet is handled: false drops a save-changing packet of another
// playthrough (logged). A team state always passes: its handler skips a state of another playthrough and still
// replays its queue (each queued packet is checked here as it is handled).
bool MergeShipAnchor_SeedAccepts(Anchor* anchor, const nlohmann::json& payload);
// A status line for the Anchor menu ("", standalone).
std::string MergeShipAnchor_ConnectionSummary();

// ---- text chat (MergeShipAnchorChat.cpp; include/mergeship/AnchorChat.hpp) ---------------------------------------------
// Anchor::ProcessIncomingPacketQueue: an MS_ANCHOR_CHAT packet.
void MergeShipAnchor_HandleChat(Anchor* anchor, const nlohmann::json& payload);
// SohGui: the chat box window and the messages above heads.
void MergeShipAnchor_RegisterChat();
void MergeShipAnchor_UnregisterChat();

extern "C" {
#endif

// Test harness: shows the local player mirrored as another Anchor player (colour r,g,b, offset dx,dz).
void MergeShipAnchor_TestLoopback(u8 r, u8 g, u8 b, f32 dx, f32 dz);

struct MsHostServices;
// EngineExport.cpp (mergeship.exe): the host's services, before the game starts.
void MergeShipAnchor_SetHostServices(const struct MsHostServices* services);
// EngineExport.cpp: the game has started (Anchor::Instance exists): follow the host's connection from now on.
void MergeShipAnchor_OnEngineReady(void);
// EngineExport.cpp: MsEngine_Suspend / MsEngine_Resume.
void MergeShipAnchor_OnEngineSuspend(void);
void MergeShipAnchor_OnEngineResume(void);
// Test harness (MergeShipTest.cpp): "anchor <command ...>", see MergeShipAnchorHost.cpp.
void MergeShipAnchor_TestCommand(const char* args);

#ifdef __cplusplus
}
#endif
