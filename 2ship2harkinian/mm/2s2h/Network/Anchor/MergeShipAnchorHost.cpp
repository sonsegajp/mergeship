// MergeShip2 -- 2 Ship 2 Harkinian's Anchor with one connection for both worlds (MergeShipAnchor.h;
// include/mergeship/AnchorNet.h). The counterpart of SoH's soh/Network/Anchor/MergeShipAnchorHost.cpp.
//
// In mergeship.exe the Anchor socket is the host's (host/NetService.cpp). This engine's Anchor is a view of it
// (Network::UseHostTransport): a link thread, started when the game starts and running whether Termina is active or
// parked, mirrors the host's connection and drains this engine's queue. Crossing never disconnects; the arriving
// engine tells the room where the player now is; the parked engine keeps its world's items and flags for the player's
// return; once-per-process packets are claimed from the host. In 2ship.exe Anchor has its own SDL_net connection.
//
// Also here: Anchor's instance, the canonical item names (MsItems.def), scene names, the loopback test player and the
// test harness command.

#include "2s2h/Network/Anchor/MergeShipAnchor.h"

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/JsonConversions.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/AnchorHost.hpp>
#include <mergeship/MsItems.h>
#include <mergeship/SharedSave.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

// The shared-save projector's receiver of a teammate's item from the other world (AnchorNet.h, "Cross-world items"):
// the unified randomizer's delivery (2s2h/Rando/MergeShip/UnifiedDelivery.cpp).
extern "C" int32_t MergeShipShared_ReceiveRemoteItem(int32_t msItemKey);
#define MS_ANCHOR_RECEIVE_REMOTE_ITEM(key) MergeShipShared_ReceiveRemoteItem(key)

namespace {

MsAnchorHost sHost;
const MsHostServices* sServices = nullptr;

bool SharedConnectionOn() {
    return CVarGetInteger(MS_CVAR_ANCHOR_SHARED_CONNECTION, 1) != 0;
}

void HostLog(const char* message) {
    if (sServices != nullptr && sServices->Log != nullptr) {
        sServices->Log(sServices->host, 0, message);
    }
}

// Anchor exists from the start (as SoH's, made in InitOTR), and reconnects if it was on when the game closed.
void EnsureAnchor() {
    if (Anchor::Instance == nullptr) {
        Anchor::Instance = new Anchor();
    }
}

static RegisterShipInitFunc sAnchorInit([]() {
    static bool sStarted = false;
    if (sStarted) {
        return;
    }
    sStarted = true;
    EnsureAnchor();
    MergeShipAnchor_AttachHost(Anchor::Instance);
    if (CVarGetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0) && !Anchor::Instance->isEnabled) {
        Anchor::Instance->Enable();
    }
});

// ---- the canonical item table (include/mergeship/MsItems.def) --------------------------------------------------

struct MsItemRow {
    s32 key;
    RandoItemId ri;
    const char* name;
    const char* displayName;
    MsKind kind;
};

// clang-format off
const MsItemRow kMsItems[] = {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) { MSI_##key, ri, #key, name, kind },
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) { MSI_##key, RI_OOT_##key, #key, name, kind },
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
};
// clang-format on

const MsItemRow* RowOfKey(s32 key) {
    for (const MsItemRow& row : kMsItems) {
        if (row.key == key) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace

std::string MergeShipAnchor_MsItemNameOfRI(s32 randoItemId) {
    static std::unordered_map<s32, const char*> sNames;
    if (sNames.empty()) {
        for (const MsItemRow& row : kMsItems) {
            sNames[(s32)row.ri] = row.name;
        }
    }
    auto it = sNames.find(randoItemId);
    return it != sNames.end() ? std::string(it->second) : std::string();
}

s32 MergeShipAnchor_MsItemKeyOfName(const std::string& name) {
    static std::unordered_map<std::string, s32> sKeys;
    if (sKeys.empty()) {
        for (const MsItemRow& row : kMsItems) {
            sKeys[row.name] = row.key;
        }
    }
    auto it = sKeys.find(name);
    return it != sKeys.end() ? it->second : -1;
}

std::string MergeShipAnchor_MsItemDisplayName(s32 msItemKey) {
    const MsItemRow* row = RowOfKey(msItemKey);
    return row != nullptr ? std::string(row->displayName) : std::string("?");
}

// The 2Ship RandoItemId of an MsItemKey (Packets/GiveItem.cpp: Rando::MergeShip::OriginTag, the item's own game, not
// the packet it arrived on), -1 when the key is unknown.
s32 MergeShipAnchor_MsItemRIOfKey(s32 msItemKey) {
    const MsItemRow* row = RowOfKey(msItemKey);
    return row != nullptr ? (s32)row->ri : -1;
}

bool MergeShipAnchor_MsItemIsJunk(s32 msItemKey) {
    const MsItemRow* row = RowOfKey(msItemKey);
    return row == nullptr || row->kind == MS_KIND_JUNK;
}

bool MergeShipAnchor_ReceiveRemoteItem(s32 msItemKey) {
    const bool taken = MS_ANCHOR_RECEIVE_REMOTE_ITEM(msItemKey) != 0;
    MergeShipHostTest_Log("ANCHOR remote item %s: %s", MergeShipAnchor_MsItemDisplayName(msItemKey).c_str(),
                          taken ? "received in Termina" : "not here, left for Hyrule");
    return taken;
}

// ---- scene names ------------------------------------------------------------------------------------------------

// 2S2H Added columns to scene table: entranceSceneId, betterMapSelectIndex, humanName
#define DEFINE_SCENE(_name, enumValue, _textId, _drawConfig, _restrictionFlags, _persistentCycleFlags, \
                     _entranceSceneId, _betterMapSelectIndex, humanName)                               \
    { enumValue, humanName },
#define DEFINE_SCENE_UNSET(_enumValue)
// MergeShipAnchorChat.cpp: the chat history is a blob on this connection.
const MsAnchorHost* MergeShipAnchor_Host() {
    return &sHost;
}

static const std::unordered_map<s16, const char*> sAnchorSceneNames = {
#include "tables/scene_table.h"
};
#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET

std::string Anchor::SceneName(s16 sceneId) {
    auto it = sAnchorSceneNames.find(sceneId);
    return it != sAnchorSceneNames.end() ? std::string(it->second) : std::string("?");
}

// ---- engine glue -------------------------------------------------------------------------------------------------

extern "C" void MergeShipAnchor_SetHostServices(const MsHostServices* services) {
    sServices = services;
    sHost.Set(services, MS_ENGINE_2SHIP);
}

void MergeShipAnchor_AttachHost(Anchor* anchor) {
    if (anchor == nullptr || !sHost.Present() || anchor->IsHostTransport() || !SharedConnectionOn()) {
        return;
    }
    anchor->UseHostTransport(sHost.net, sHost.host, sHost.engineId);
    SPDLOG_INFO("[Anchor] MergeShip2: using mergeship.exe's shared connection");
}

extern "C" void MergeShipAnchor_OnEngineReady(void) {
    // From now on this engine follows the host's connection, even if the player enabled Anchor in Hyrule.
    EnsureAnchor();
    MergeShipAnchor_AttachHost(Anchor::Instance);
    if (Anchor::Instance->IsHostTransport()) {
        HostLog("anchor: 2 Ship 2 Harkinian follows the shared Anchor connection");
    }
}

// The room settings the host's connection was made with (by either engine) become this game's too: one player, one
// room, one name. True when the connection is up (or wanted) and its settings were taken.
// BUG-0063/0064/0078: the room's state (syncItemsAndFlags above all) as the other world's engine last received it. The
// server sends it once, on joining; an engine that was parked then (or not yet listening) never saw it, and with
// syncItemsAndFlags off it neither sent nor applied any item or flag.
void MergeShipAnchor_ShareRoomState(const nlohmann::json& state) {
    if (sHost.Present() && state.is_object()) {
        sHost.SetBlobJson(MS_ANCHOR_BLOB_ROOM_STATE, state);
    }
}

static void AdoptRoomState(Anchor* anchor) {
    nlohmann::json state;
    if ((anchor == nullptr) || !sHost.Present() || !sHost.GetBlobJson(MS_ANCHOR_BLOB_ROOM_STATE, state) ||
        !state.is_object()) {
        return;
    }
    anchor->roomState.ownerClientId = state.value("ownerClientId", anchor->roomState.ownerClientId);
    anchor->roomState.pvpMode = state.value("pvpMode", anchor->roomState.pvpMode);
    anchor->roomState.showLocationsMode = state.value("showLocationsMode", anchor->roomState.showLocationsMode);
    anchor->roomState.teleportMode = state.value("teleportMode", anchor->roomState.teleportMode);
    anchor->roomState.syncItemsAndFlags = state.value("syncItemsAndFlags", anchor->roomState.syncItemsAndFlags);
    MergeShipHostTest_Log("ANCHOR room state adopted: syncItemsAndFlags=%d showLocationsMode=%d teleportMode=%d",
                      anchor->roomState.syncItemsAndFlags, anchor->roomState.showLocationsMode,
                      anchor->roomState.teleportMode);
}

static bool AdoptHostSettings(Anchor* anchor) {
    nlohmann::json settings;
    if (anchor == nullptr || !anchor->IsHostTransport() || !sHost.Status().enabled ||
        !sHost.GetBlobJson(MS_ANCHOR_BLOB_SETTINGS, settings)) {
        return false;
    }
    AdoptRoomState(anchor);
    CVarSetString(CVAR_REMOTE_ANCHOR("Host"), settings.value("host", std::string("anchor.hm64.org")).c_str());
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), settings.value("port", 43383));
    CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), settings.value("roomId", std::string()).c_str());
    CVarSetString(CVAR_REMOTE_ANCHOR("Name"), settings.value("name", std::string()).c_str());
    CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), settings.value("teamId", std::string("default")).c_str());
    if (settings.contains("color")) {
        Color_RGB8 color = settings["color"].get<Color_RGB8>();
        CVarSetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), color);
    }
    if (anchor->ownClientId == 0 && settings.value("clientId", (uint32_t)0) != 0) {
        anchor->ownClientId = settings["clientId"].get<uint32_t>();
    }
    return true;
}

void MergeShipAnchor_BeforeEnable(Anchor* anchor) {
    if (anchor == nullptr || !anchor->IsHostTransport()) {
        return;
    }
    nlohmann::json settings;
    if (AdoptHostSettings(anchor)) {
        return;
    }
    settings["host"] = CVarGetString(CVAR_REMOTE_ANCHOR("Host"), "anchor.hm64.org");
    settings["port"] = CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"), 43383);
    settings["roomId"] = CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"), "");
    settings["name"] = CVarGetString(CVAR_REMOTE_ANCHOR("Name"), "");
    settings["teamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    settings["color"] = CVarGetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100 });
    settings["clientId"] = anchor->ownClientId;
    sHost.SetBlobJson(MS_ANCHOR_BLOB_SETTINGS, settings);
}

bool MergeShipAnchor_IsActiveEngine() {
    return sHost.IsActive();
}

bool MergeShipAnchor_KeepIncoming(Anchor* anchor, const std::string& packetType) {
    if (anchor == nullptr || !anchor->IsHostTransport() || sHost.IsActive()) {
        return true;
    }
    // Parked: other players' movement, sounds and hits are only meaningful now, and a teleport request is the
    // active engine's to answer. Everything else (room list, items, flags, team state) waits for the player.
    // Chat is the active engine's to add to the shared history.
    return packetType != Anchor::PLAYER_UPDATE && packetType != Anchor::PLAYER_SFX &&
           packetType != Anchor::OCARINA_SFX && packetType != Anchor::DAMAGE_PLAYER &&
           packetType != Anchor::REQUEST_TELEPORT && packetType != Anchor::TELEPORT_TO && packetType != MS_ANCHOR_CHAT;
}

// Every client id this process has had (a reconnect gives a new one, and an engine may still send with the old one for
// a while): the server relays a sender's packets to every other id, so a packet from one of them is our own, echoed.
static bool IsOwnClientId(uint32_t id) {
    if (id == 0) {
        return false;
    }
    if ((Anchor::Instance != nullptr) && (Anchor::Instance->ownClientId == id)) {
        return true;
    }
    nlohmann::json ids;
    if (sHost.Present() && sHost.GetBlobJson(MS_ANCHOR_BLOB_OWN_IDS, ids) && ids.is_array()) {
        for (const auto& known : ids) {
            if (known.is_number() && (known.get<uint32_t>() == id)) {
                return true;
            }
        }
    }
    return false;
}

void MergeShipAnchor_NoteOwnClientId(uint32_t id) {
    if ((id == 0) || !sHost.Present()) {
        return;
    }
    nlohmann::json ids;
    if (!sHost.GetBlobJson(MS_ANCHOR_BLOB_OWN_IDS, ids) || !ids.is_array()) {
        ids = nlohmann::json::array();
    }
    for (const auto& known : ids) {
        if (known.is_number() && (known.get<uint32_t>() == id)) {
            return;
        }
    }
    ids.push_back(id);
    sHost.SetBlobJson(MS_ANCHOR_BLOB_OWN_IDS, ids);
}

bool MergeShipAnchor_Claim(const nlohmann::json& payload) {
    if (Anchor::Instance != nullptr) {
        MergeShipAnchor_NoteOwnClientId(Anchor::Instance->ownClientId);
    }
    if (payload.contains("clientId") && payload["clientId"].is_number() &&
        IsOwnClientId(payload["clientId"].get<uint32_t>())) {
        MergeShipHostTest_Log("ANCHOR %s from our own client %u (an echo): ignored", payload.value("type", "?").c_str(),
                          payload["clientId"].get<uint32_t>());
        return false;
    }
    return sHost.Claim(payload);
}

bool MergeShipAnchor_CanClaim(const nlohmann::json& payload) {
    // As Claim: our own packet, echoed, is never ours to apply (a receiver that asks first, then gives through the
    // shared save, then claims, would otherwise give our own find back to us).
    if (payload.contains("clientId") && payload["clientId"].is_number() &&
        IsOwnClientId(payload["clientId"].get<uint32_t>())) {
        return false;
    }
    return sHost.CanClaim(payload);
}

bool MergeShipAnchor_Announce(const nlohmann::json& payload) {
    return sHost.Announce(payload);
}

bool MergeShipAnchor_IsOwnPacket(const nlohmann::json& payload) {
    return payload.contains("clientId") && payload["clientId"].is_number() &&
           IsOwnClientId(payload["clientId"].get<uint32_t>());
}

bool MergeShipAnchor_ClaimShared(const nlohmann::json& payload) {
    // Our own team state, echoed: its counts are this player's own from before he spent some, and the merge only
    // raises them (SharedSaveJson.hpp): taking it would give back what he spent.
    if (MergeShipAnchor_IsOwnPacket(payload)) {
        return false;
    }
    const uint64_t seq = MsAnchorHost::Seq(payload);
    if (!sHost.Present() || (seq == 0)) {
        return true;
    }
    return sHost.net->Claim(sHost.host, seq | MS_ANCHOR_CLAIM_SHARED, sHost.engineId) != 0;
}

// ---- the playthrough (AnchorNet.h MS_ANCHOR_KEY_SEED) ------------------------------------------------------------

namespace {

// The Hyrule file the player last arrived from (MergeShipAnchor_OnHyruleArrival): its shared save's hyruleFileId and
// the playthrough SoH published for it then. The Termina file of that slot is this one's until another arrival,
// whatever SoH loads in between: a manual switch (Ctrl+F12) can bring Termina back with SoH on another slot or seed,
// and MS_ANCHOR_BLOB_SEED then names that one. Written on the game thread, read by the link thread too (a packet's
// stamp).
struct HyruleArrival {
    bool known = false;
    uint32_t fileId = 0;
    bool seedKnown = false;
    uint32_t seed = 0;
};
std::mutex sArrivalMutex;
HyruleArrival sArrival;

bool BlobSeed(uint32_t& seed) {
    std::string text;
    if (sHost.Present() && sHost.GetBlob(MS_ANCHOR_BLOB_SEED, text)) {
        try {
            seed = (uint32_t)std::stoul(text);
            return true;
        } catch (...) {}
    }
    return false;
}

HyruleArrival Arrival() {
    std::lock_guard<std::mutex> lock(sArrivalMutex);
    return sArrival;
}

} // namespace

extern "C" void MergeShipAnchor_OnHyruleArrival(u32 hyruleFileId) {
    HyruleArrival arrival;
    arrival.known = true;
    arrival.fileId = hyruleFileId;
    // (SoH publishes its file's number on every send and as it is suspended for this trip.)
    arrival.seedKnown = BlobSeed(arrival.seed);
    {
        std::lock_guard<std::mutex> lock(sArrivalMutex);
        sArrival = arrival;
    }
    MergeShipHostTest_Log("ANCHOR arrived from Hyrule file %08X (seed %u%s)", hyruleFileId, arrival.seed,
                          arrival.seedKnown ? "" : ", not published");
}

MsSharedSave* MergeShipAnchor_LinkedSharedSave() {
    MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
    if (shared == nullptr) {
        return nullptr;
    }
    // The slot's Hyrule file started another playthrough since the player came from it (SoH
    // MergeShipHost_ClaimSharedSave gave the shared save a new hyruleFileId): the Termina file loaded is the earlier
    // playthrough's (remade on the next trip, MsSharedSave.terminaFileStale), not this one's. (No arrival noted at all:
    // as linked, as before.)
    const HyruleArrival arrival = Arrival();
    if (arrival.known && (arrival.fileId != shared->hyruleFileId)) {
        return nullptr;
    }
    return shared;
}

uint32_t MergeShipAnchor_LocalSeed() {
    // A Termina file of a Hyrule file: Hyrule's number (one player, one playthrough), as SoH published it when the
    // player came from that file (Arrival), else as it publishes it now.
    if (MergeShipAnchor_LinkedSharedSave() != nullptr) {
        const HyruleArrival arrival = Arrival();
        if (arrival.known && arrival.seedKnown) {
            return arrival.seed;
        }
        uint32_t seed = 0;
        if (BlobSeed(seed)) {
            return seed;
        }
    }
    // 2ship.exe, or a Termina file of Majora's Mask's own (loaded from its file select): the file's own randomizer
    // seed. Hyrule's number is not this file's: under it, the file's finds would reach Hyrule's teammates, and theirs
    // this file.
    return IS_RANDO ? gSaveContext.save.shipSaveInfo.rando.finalSeed : 0;
}

bool MergeShipAnchor_SeedAccepts(Anchor* anchor, const nlohmann::json& payload) {
    (void)anchor;
    const std::string type = payload.value("type", std::string());
    // A team state's own handler checks its "state" (MergeShipAnchor_TeamStateSeedAccepts): its queue is replayed
    // whatever the state's playthrough -- each queued packet carries its own and is checked as it is handled -- or a
    // stored state of an earlier seed would take this seed's queued finds down with it (the push that answers it clears
    // the server's queue).
    if (!MsAnchor_IsSeededPacket(type) || (type == Anchor::UPDATE_TEAM_STATE)) {
        return true;
    }
    const uint32_t local = MergeShipAnchor_LocalSeed();
    if (MsAnchor_SeedMatches(payload, local)) {
        return true;
    }
    const uint32_t theirs = MsAnchor_PacketSeed(payload);
    SPDLOG_INFO("[Anchor] MergeShip2: {} of seed {} ignored (this file's seed is {})", type, theirs, local);
    MergeShipHostTest_Log("ANCHOR %s of seed %u ignored (local seed %u)", type.c_str(), theirs, local);
    return false;
}

bool MergeShipAnchor_TeamStateSeedAccepts(const nlohmann::json& payload) {
    // The server keeps only a team state's "state" and "queue": the playthrough is the state's own. With no state there
    // is nothing of it to apply.
    if (!payload.contains("state") || !payload["state"].is_object() || payload["state"].empty()) {
        return true;
    }
    const nlohmann::json& state = payload["state"];
    const uint32_t local = MergeShipAnchor_LocalSeed();
    if (MsAnchor_SeedMatches(state, local)) {
        return true;
    }
    const uint32_t theirs = MsAnchor_PacketSeed(state);
    SPDLOG_INFO("[Anchor] MergeShip2: team state of seed {} not merged (this file's seed is {})", theirs, local);
    MergeShipHostTest_Log("ANCHOR UPDATE_TEAM_STATE of seed %u ignored (local seed %u)", theirs, local);
    return false;
}

nlohmann::json MergeShipAnchor_OoTTeamState(Anchor* anchor) {
    // Only this playthrough's Hyrule part goes out under its number: one of another seed or file (the blob or a
    // received copy outlive the file they came with) would give every teammate of this seed that one's items (the
    // user's new seed starting with an earlier seed's Nayru's Love, 2026-09-30).
    const uint32_t local = MergeShipAnchor_LocalSeed();
    const MsSharedSave* linked = MergeShipAnchor_LinkedSharedSave();
    nlohmann::json oot;
    if ((linked != nullptr) && sHost.Present() && sHost.GetBlobJson(MS_ANCHOR_BLOB_TEAM_OOT, oot) && oot.is_object() &&
        !oot.empty()) {
        // Hyrule's latest, from this process's own soh.dll: the file this Termina belongs to (when it says), its seed.
        // (A received copy's "fileId", if any, is its sender's file: only the blob's is ours to compare.)
        const bool sameFile = !oot.contains("fileId") ||
                              (oot["fileId"].is_number() && (oot["fileId"].get<uint32_t>() == linked->hyruleFileId));
        if (sameFile && MsAnchor_SeedMatches(oot, local)) {
            return oot;
        }
        MergeShipHostTest_Log("ANCHOR Hyrule's team state (seed %u) is not this playthrough's (seed %u): not sent",
                              MsAnchor_PacketSeed(oot), local);
    }
    // Otherwise the last one received (the team's, as the server last had it). As with SoH's own team state, the
    // sender's state is the team's: SoH sets its inventory from it (Anchor's rule: the player with the most progress
    // loads first).
    if ((anchor != nullptr) && anchor->ootTeamState.is_object() && !anchor->ootTeamState.empty() &&
        MsAnchor_SeedMatches(anchor->ootTeamState, local)) {
        return anchor->ootTeamState;
    }
    return nlohmann::json();
}

std::string MergeShipAnchor_ConnectionSummary() {
    if (Anchor::Instance == nullptr || !Anchor::Instance->IsHostTransport()) {
        return std::string();
    }
    MsNetStatus status = sHost.Status();
    char line[160];
    snprintf(line, sizeof(line), "Shared with Hyrule: connection %u%s", status.connectionId,
             status.reconnects != 0 ? " (reconnected)" : "");
    return line;
}

void MergeShipAnchor_LogState(const char* why) {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    int players = 0;
    int hyrule = 0;
    for (auto& [clientId, client] : anchor->clients) {
        if (client.online && !client.self) {
            players++;
            if (!Anchor::IsOwnWorld(client)) {
                hyrule++;
            }
        }
    }
    MsNetStatus status = sHost.Status();
    char line[320];
    snprintf(line, sizeof(line),
             "ANCHOR %s: engine 2ship, hosted %d, active %d, enabled %d, connected %d, connection %u, reconnects %u, "
             "clientId %u, players %d (%d in Hyrule)",
             why, anchor->IsHostTransport() ? 1 : 0, sHost.IsActive() ? 1 : 0, anchor->isEnabled ? 1 : 0,
             anchor->isConnected ? 1 : 0, status.connectionId, status.reconnects, anchor->ownClientId, players,
             hyrule);
    SPDLOG_INFO("{}", line);
    MergeShipHostTest_Log("%s", line);
    HostLog(line);
}

// ---- Termina's part of the team state, kept for Hyrule ---------------------------------------------------------

namespace {

// `text` becomes the file at `path` whole or not at all (a temporary file beside it, then renamed over it): SoH may read
// it at any time, and a crash mid-write must not leave half a team state.
bool WriteFileAtomically(const std::filesystem::path& path, const std::string& text) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(text.data(), (std::streamsize)text.size());
        out.flush();
        if (!out) {
            return false;
        }
    }
    std::filesystem::rename(temp, path, error);
    if (error) {
        // (A file system that will not rename over an existing file.)
        std::filesystem::remove(path, error);
        std::filesystem::rename(temp, path, error);
    }
    if (error) {
        std::filesystem::remove(temp, error);
        return false;
    }
    return true;
}

} // namespace

void MergeShipAnchor_PersistTerminaTeamState(Anchor* anchor) {
    if (anchor == nullptr || !anchor->IsSaveLoaded()) {
        return;
    }
    // Only a Termina file that belongs to a Hyrule file has a part of that file's team state. Any other (one loaded
    // from Majora's Mask's own file select, or the earlier playthrough's of a slot started anew) erases the blob: SoH
    // must not take an unrelated file's Termina as its own file's.
    const MsSharedSave* shared = MergeShipAnchor_LinkedSharedSave();
    if (shared == nullptr) {
        if (sHost.Present()) {
            sHost.SetBlob(MS_ANCHOR_BLOB_TEAM_MM, std::string()); // (a zero length erases)
        }
        return;
    }
    nlohmann::json mm;
    try {
        mm = anchor->PrepTeamState();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Anchor] MergeShip2: team state: {}", e.what());
        return;
    }
    // Whose it is: the Hyrule file (SharedSave.h hyruleFileId: a new file or seed in the slot is another playthrough)
    // and the seed. SoH checks both against the file it has loaded (MergeShipAnchorHost.cpp UnwrapTeamMM), so the blob
    // and the file below need no guessing at whose they are.
    nlohmann::json file;
    file["fileId"] = shared->hyruleFileId;
    file["seed"] = MergeShipAnchor_LocalSeed();
    file["state"] = mm;
    // For Hyrule's engine to send along with its own while the player is there, connected now or later.
    if (sHost.Present()) {
        sHost.SetBlobJson(MS_ANCHOR_BLOB_TEAM_MM, file);
    }
    // And on disk, for a later run that starts in Hyrule (the blob lives only as long as the process).
    if (shared->saveSlot < 0) {
        return;
    }
    // SoH's save folder ("soh": the same folder wherever the platform keeps each game's own).
    const std::filesystem::path path =
        std::filesystem::path(Ship::Context::GetPathRelativeToAppDirectory("Save", "soh")) /
        ("mergeship_team_mm_" + std::to_string(shared->saveSlot + 1) + ".json");
    if (!WriteFileAtomically(path, file.dump())) {
        SPDLOG_ERROR("[Anchor] MergeShip2: could not write {}", path.string());
        return;
    }
    MergeShipHostTest_Log("ANCHOR Termina team state written for Hyrule file slot %d (seed %u)", shared->saveSlot,
                          file["seed"].get<uint32_t>());
}

// BUG-0061/0065/0072: a teammate on Termina's minimap is a compass arrow in his own colour, as on Hyrule's (SoH's
// OnMinimapDrawCompassIcons): 1 with `color` and `scale` for an Anchor dummy to draw so, 2 for one not to draw at all
// (the room shows no locations, another team, another room, the option off), 0 for any other actor. The player's own
// arrow takes his Anchor colour while Anchor shows the others (MergeShipAnchor_MinimapOwnColor).
extern "C" s32 MergeShipAnchor_MinimapIcon(Actor* actor, Color_RGB8* color, f32* scale) {
    Anchor* anchor = Anchor::Instance;
    if ((actor == nullptr) || (actor->id != ANCHOR_DUMMY_ACTOR_ID) || (anchor == nullptr)) {
        return 0;
    }
    const uint32_t clientId = anchor->GetDummyPlayerClientId(actor);
    if ((clientId == 0) || !anchor->clients.contains(clientId)) {
        return 2;
    }
    const AnchorClient& client = anchor->clients[clientId];
    const std::string teamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    // BUG-0159: only a teammate in this very scene and room -- one elsewhere (another scene, the other world, the file
    // select) keeps his dummy parked far away, and his arrow was drawn on this room's map all the same.
    if (!CVarGetInteger(CVAR_REMOTE_ANCHOR("ShowOtherPlayersOnMinimap"), 1) ||
        (anchor->roomState.showLocationsMode == 0) || !client.online || !client.isSaveLoaded ||
        !anchor->IsInOwnScene(client) ||
        ((anchor->roomState.showLocationsMode != 2) && (client.teamId != teamId)) ||
        ((gPlayState != nullptr) && (client.curRoomNum != gPlayState->roomCtx.curRoom.num))) {
        return 2;
    }
    *color = client.color;
    *scale = 0.3f;
    return 1;
}

extern "C" s32 MergeShipAnchor_MinimapOwnColor(Color_RGB8* color) {
    Anchor* anchor = Anchor::Instance;
    if ((anchor == nullptr) || !anchor->isConnected || !CVarGetInteger(CVAR_REMOTE_ANCHOR("ShowOtherPlayersOnMinimap"), 1) ||
        (anchor->roomState.showLocationsMode == 0)) {
        return false;
    }
    *color = CVarGetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { 100, 255, 100 });
    return true;
}

extern "C" void MergeShipAnchor_OnEngineSuspend(void) {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    // Termina's part of the team state, for Hyrule's engine (whether Anchor is connected now or connects later).
    MergeShipAnchor_PersistTerminaTeamState(anchor);
    if (!anchor->IsHostTransport()) {
        return;
    }
    if (anchor->isEnabled) {
        MergeShipAnchor_LogState("suspended");
    }
}

extern "C" void MergeShipAnchor_OnEngineResume(void) {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr || !anchor->IsHostTransport()) {
        return;
    }
    if (!anchor->isEnabled) {
        // Anchor was disabled in Hyrule: nobody left to show.
        anchor->clients.clear();
        return;
    }
    // (Anchor may have been enabled in the other world: its room, name and team are this game's too.)
    AdoptHostSettings(anchor);
    if (anchor->isConnected) {
        // The player is in Termina again: tell the room where, and keep the host's handshake current for reconnects.
        anchor->SendPacket_UpdateClientState();
        anchor->SendHelloToRemote(anchor->PrepHandshake());
    }
    MergeShipAnchor_LogState("resumed");
}

// ---- loopback test -----------------------------------------------------------------------------------------------

namespace {
const uint32_t kLoopbackClientId = 0x4D530002; // "MS" 2
f32 sLoopbackOffsetX = 0.0f;
int sLoopbackAge = -1; // "anchor loopage": the loopback ghost's age (-1: this player's)
f32 sLoopbackOffsetZ = 0.0f;
} // namespace

// The local player mirrored as another Anchor player, through the PLAYER_UPDATE packet path, without a server.
void Anchor::MergeShipLoopbackStart(Color_RGB8 color, f32 offsetX, f32 offsetZ) {
    if (!IsSaveLoaded()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    AnchorClient client = {};

    sLoopbackOffsetX = offsetX;
    sLoopbackOffsetZ = offsetZ;
    client.clientId = kLoopbackClientId;
    client.name = "Loopback";
    client.color = color;
    client.clientVersion = clientVersion;
    client.teamId = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    client.online = true;
    client.self = false;
    client.isSaveLoaded = true;
    client.world = MS_ANCHOR_WORLD_MM;
    client.sceneNum = WireScene(gPlayState->sceneId);
    client.form = player->transformation;
    client.posRot = player->actor.world;
    client.scale = player->actor.scale;
    clients[kLoopbackClientId] = client;

    mergeShipLoopback = true;
    RegisterHooks();
    MergeShipLoopbackFrame();
    RefreshClientActors();
    MergeShipHostTest_Log("ANCHOR loopback on color=(%d,%d,%d) offset=(%.0f,%.0f) form=%d", color.r, color.g, color.b,
                          offsetX, offsetZ, player->transformation);
}

// Called from the player's update hook: this frame's PLAYER_UPDATE, as another client would receive it.
void Anchor::MergeShipLoopbackFrame() {
    if (!mergeShipLoopback || !IsSaveLoaded() || !clients.contains(kLoopbackClientId)) {
        return;
    }
    nlohmann::json payload = PrepPlayerUpdate();

    payload["clientId"] = kLoopbackClientId;
    if (sLoopbackAge >= 0) {
        payload["humanAdult"] = sLoopbackAge; // "anchor loopage": a ghost of the other age
    }
    payload["posRot"]["pos"]["x"] = payload["posRot"]["pos"]["x"].get<f32>() + sLoopbackOffsetX;
    payload["posRot"]["pos"]["z"] = payload["posRot"]["pos"]["z"].get<f32>() + sLoopbackOffsetZ;
    // Through text, as it would travel.
    HandlePacket_PlayerUpdate(nlohmann::json::parse(payload.dump()));
    clients[kLoopbackClientId].online = true;
    clients[kLoopbackClientId].isSaveLoaded = true;
}

extern "C" void MergeShipAnchor_TestLoopback(u8 r, u8 g, u8 b, f32 dx, f32 dz) {
    EnsureAnchor();
    Anchor::Instance->MergeShipLoopbackStart(Color_RGB8{ r, g, b }, dx, dz);
}

// ---- test harness ------------------------------------------------------------------------------------------------

// "anchor <command ...>" from MergeShipHostTest.cpp (mergeship.exe) and MergeShipTest.cpp (2ship.exe):
//   anchor enable <host> <port> <room> <name> [team]   set the settings (for this run) and enable Anchor
//   anchor disable
//   anchor state                                        log the connection (ANCHOR state: ...)
//   anchor loop <r> <g> <b> [dx dz]                     the loopback player (MergeShipAnchor_TestLoopback)
//   anchor loopage <0|1|-1>                             the loopback ghost young / adult / as this player
//   anchor give <mmItem>                                send MM_GIVE_ITEM for a Majora's Mask ItemId
//   anchor send <json>                                  send a raw packet (e.g. a mock server's control packet)
//   anchor dummies                                      log the dummy players in the scene
extern "C" void MergeShipAnchor_TestCommand(const char* args) {
    EnsureAnchor();
    Anchor* anchor = Anchor::Instance;
    std::istringstream in(args != nullptr ? args : "");
    std::string op;
    in >> op;
    if (op == "enable") {
        std::string host, room, name, team;
        int port = 43383;
        in >> host >> port >> room >> name >> team;
        CVarSetString(CVAR_REMOTE_ANCHOR("Host"), host.c_str());
        CVarSetInteger(CVAR_REMOTE_ANCHOR("Port"), port);
        CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), room.c_str());
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), name.c_str());
        CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), team.empty() ? "default" : team.c_str());
        anchor->Enable();
        MergeShipAnchor_LogState("enable");
    } else if (op == "disable") {
        anchor->Disable();
        MergeShipAnchor_LogState("disable");
    } else if (op == "state") {
        MergeShipAnchor_LogState("state");
    } else if (op == "loop") {
        int r = 80, g = 80, b = 255;
        float dx = 80.0f, dz = 0.0f;
        in >> r >> g >> b >> dx >> dz;
        MergeShipAnchor_TestLoopback((u8)r, (u8)g, (u8)b, dx, dz);
    } else if (op == "loopage") {
        int age = -1;
        in >> age;
        sLoopbackAge = age;
        MergeShipHostTest_Log("ANCHOR loopback age %d", age);
    } else if (op == "give") {
        int item = ITEM_NONE;
        in >> item;
        anchor->SendPacket_MMGiveItem(-1, (u8)item);
        MergeShipHostTest_Log("ANCHOR give mm item %d", item);
    } else if (op == "send") {
        std::string rest;
        std::getline(in, rest);
        try {
            anchor->SendJsonToRemote(nlohmann::json::parse(rest));
            MergeShipHostTest_Log("ANCHOR sent %s", rest.c_str());
        } catch (const std::exception& e) { MergeShipHostTest_Log("ANCHOR send: bad json: %s", e.what()); }
    } else if (op == "dummies") {
        if (gPlayState != nullptr) {
            for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].first; actor != nullptr;
                 actor = actor->next) {
                if (actor->id == ANCHOR_DUMMY_ACTOR_ID && actor->update == DummyPlayer_Update) {
                    Player* dummy = (Player*)actor;
                    MergeShipHostTest_Log("ANCHOR dummy client=%u form=%d mask=%d pos=(%.0f,%.0f,%.0f)",
                                          anchor->GetDummyPlayerClientId(actor), dummy->transformation,
                                          dummy->currentMask, actor->world.pos.x, actor->world.pos.y,
                                          actor->world.pos.z);
                }
            }
        }
    } else {
        MergeShipHostTest_Log("ANCHOR unknown command '%s'", op.c_str());
    }
}
