// MergeShip2 -- Anchor with one connection for both worlds (MergeShipAnchor.h; include/mergeship/AnchorNet.h).
//
// In mergeship.exe the player crosses between Hyrule (this engine) and Termina (2ship.dll). The Anchor socket is the
// host's (host/NetService.cpp): it was opened once, by whichever engine the player enabled Anchor in, and it stays
// open whichever engine runs. This engine's Anchor is a view of it (Network::UseHostTransport): a link thread, started
// when the game starts and running whether this engine is active or parked, mirrors the host's connection into
// isEnabled/isConnected and drains this engine's queue of received packets. So:
//   - crossing never disconnects: same socket, same client id, same room; the arriving engine only tells the room
//     where the player now is (UPDATE_CLIENT_STATE with "world" and "sceneName");
//   - the parked engine keeps listening (room list, flags and items of its world queue up and are applied when the
//     player comes back); per-frame packets (other players' movement and sounds) are dropped while parked;
//   - what must happen once per process (an item, a reply to a team state request, a notification) is claimed from the
//     host: the engine the player is in applies it, or, for an item of the other world, gives it through the shared
//     save (MergeShipShared_ReceiveRemoteItem) and only then claims it.
// Standalone soh.exe has no host: nothing here changes Anchor there, except that a Termina teammate's items
// (MM_GIVE_ITEM) are offered to MergeShipShared_ReceiveRemoteItem.

#include "soh/Network/Anchor/MergeShipAnchor.h"

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include "soh/MergeShip/MergeShipMsItems.h"
#include "soh/MergeShip/MergeShipHost.h" // MergeShip_Save_SharedFileId
#include "soh/Notification/Notification.h"
#include "soh/SohGui/ImGuiUtils.h" // MergeShip2: MergeShip_NotificationIcon (BUG-0090)
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/OTRGlobals.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/AnchorHost.hpp>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h" // Flags_GetRandomizerInf
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

// The shared-save projector's receiver of a teammate's item from the other world (AnchorNet.h, "Cross-world items"):
// the unified randomizer's delivery (soh/MergeShip/MergeShipUnified.cpp).
extern "C" int32_t MergeShipShared_ReceiveRemoteItem(int32_t msItemKey);
#define MS_ANCHOR_RECEIVE_REMOTE_ITEM(key) MergeShipShared_ReceiveRemoteItem(key)

namespace {

MsAnchorHost sHost;
const MsHostServices* sServices = nullptr;

bool SharedConnectionOn() {
    return CVarGetInteger(MS_CVAR_ANCHOR_SHARED_CONNECTION, 1) != 0;
}

bool CrossWorldItemsOn() {
    return CVarGetInteger(MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS, 1) != 0;
}

void HostLog(const char* message) {
    if (sServices != nullptr && sServices->Log != nullptr) {
        sServices->Log(sServices->host, 0, message);
    }
}

// MsItems.def key name ("HOOKSHOT") -> MsItemKey.
MsItemKey KeyOfName(const std::string& name) {
    static std::unordered_map<std::string, MsItemKey> sKeys;
    if (sKeys.empty()) {
        for (int i = 0; i < MSI_MAX; i++) {
            const MsItemInfo* info = MsItems_Get((MsItemKey)i);
            if (info != nullptr && info->keyName != nullptr) {
                sKeys[info->keyName] = (MsItemKey)i;
            }
        }
    }
    auto it = sKeys.find(name);
    return it != sKeys.end() ? it->second : MSI_MAX;
}

} // namespace

// ---- engine glue ----------------------------------------------------------------------------------------------------

extern "C" void MergeShipAnchor_SetHostServices(const MsHostServices* services) {
    sServices = services;
    sHost.Set(services, MS_ENGINE_SOH);
}

void MergeShipAnchor_AttachHost(Anchor* anchor) {
    if (anchor == nullptr || !sHost.Present() || anchor->IsHostTransport() || !SharedConnectionOn()) {
        return;
    }
    anchor->UseHostTransport(sHost.net, sHost.host, sHost.engineId);
    SPDLOG_INFO("[Anchor] MergeShip2: using mergeship.exe's shared connection");
}

extern "C" void MergeShipAnchor_OnEngineReady(void) {
    // From now on this engine follows the host's connection, even if the player enabled Anchor in Termina.
    MergeShipAnchor_AttachHost(Anchor::Instance);
    if (Anchor::Instance != nullptr && Anchor::Instance->IsHostTransport()) {
        HostLog("anchor: Ship of Harkinian follows the shared Anchor connection");
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
    MergeShipTest_Log("ANCHOR room state adopted: syncItemsAndFlags=%d showLocationsMode=%d teleportMode=%d",
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

// MergeShipAnchorChat.cpp: the chat history is a blob on this connection.
const MsAnchorHost* MergeShipAnchor_Host() {
    return &sHost;
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
        MergeShipTest_Log("ANCHOR %s from our own client %u (an echo): ignored", payload.value("type", "?").c_str(),
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

bool MergeShipAnchor_ClaimShared(const nlohmann::json& payload) {
    const uint64_t seq = MsAnchorHost::Seq(payload);
    if (!sHost.Present() || (seq == 0)) {
        return true;
    }
    return sHost.net->Claim(sHost.host, seq | MS_ANCHOR_CLAIM_SHARED, sHost.engineId) != 0;
}

bool MergeShipAnchor_IsOwnEcho(const nlohmann::json& payload) {
    return payload.contains("clientId") && payload["clientId"].is_number() &&
           IsOwnClientId(payload["clientId"].get<uint32_t>());
}

// A teammate's item is announced once per process, even when one engine leaves it to the other (AnchorNet.h,
// MS_ANCHOR_CLAIM_ANNOUNCED): true when this engine shows its notification. (Packets/GiveItem.cpp declares it.)
bool MergeShipAnchor_Announce(const nlohmann::json& payload) {
    return sHost.Announce(payload);
}

bool Anchor::IsOwnWorld(const AnchorClient& client) {
    return client.world.empty() || client.world == MS_ANCHOR_WORLD_OOT;
}

std::string MergeShipAnchor_MsItemName(u16 modId, s16 getItemId) {
    RandomizerGet rg = RG_NONE;
    if (modId == MOD_RANDOMIZER) {
        rg = (RandomizerGet)getItemId;
    } else {
        rg = Rando::StaticData::ItemFromGIID(getItemId).GetRandomizerGet();
    }
    // MergeShip2: a progressive Majora's Mask item is named by the step it gave (OnItemReceive runs after
    // Randomizer_Item_Give set the step's flag), as 2Ship's SendPacket_MMGiveItem sends its resolved step. Hyrule's
    // RG_PROGRESSIVE_MM_SWORD is 2S2H's RI_PROGRESSIVE_SWORD without its first step (Ocarina of Time's own Kokiri Sword
    // stands for it): Razor, then Gilded. A Termina teammate converting PROGRESSIVE_MM_SWORD on RI_PROGRESSIVE_SWORD's
    // chain starts at MM's Kokiri Sword, another step. getItemId stays the progressive item: a Hyrule teammate converts
    // it on this same chain.
    if (rg == RG_PROGRESSIVE_MM_SWORD) {
        rg = Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GILDED_SWORD) ? RG_MM_GILDED_SWORD : RG_MM_RAZOR_SWORD;
    } else if (rg == RG_PROGRESSIVE_GORON_LULLABY) {
        rg = Flags_GetRandomizerInf(RAND_INF_OBTAINED_MM_GORON_LULLABY) ? RG_MM_GORONS_LULLABY
                                                                         : RG_MM_GORON_LULLABY_INTRO;
    }
    const MsItemInfo* info = MsItems_Get(MsItems_KeyOfRG(rg));
    return (info != nullptr && info->keyName != nullptr) ? std::string(info->keyName) : std::string();
}

// ---- the playthrough (AnchorNet.h MS_ANCHOR_KEY_SEED) ------------------------------------------------------------

uint32_t MergeShipAnchor_LocalSeed() {
    static uint32_t sPublished = 0;
    static bool sHasPublished = false;
    const uint32_t seed = IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : 0;
    // Termina's engine stamps and compares Hyrule's number (one player, one playthrough).
    if (sHost.Present() && (!sHasPublished || sPublished != seed)) {
        sHost.SetBlob(MS_ANCHOR_BLOB_SEED, std::to_string(seed));
        sPublished = seed;
        sHasPublished = true;
    }
    return seed;
}

bool MergeShipAnchor_SeedAccepts(Anchor* anchor, const nlohmann::json& payload) {
    (void)anchor;
    const std::string type = payload.value("type", std::string());
    // A team state is two things the server keeps apart: the team's last "state", whose playthrough is the state's own,
    // and the "queue" of packets for players who were offline, each of which carries its own. The handler skips a
    // state of another playthrough and still replays the queue, every packet facing this check as it is handled
    // (Packets/UpdateTeamState.cpp): dropping the whole packet for an old seed's state lost this seed's queued items.
    if (!MsAnchor_IsSeededPacket(type) || (type == Anchor::UPDATE_TEAM_STATE)) {
        return true;
    }
    const uint32_t local = MergeShipAnchor_LocalSeed();
    if (MsAnchor_SeedMatches(payload, local)) {
        return true;
    }
    const uint32_t theirs = MsAnchor_PacketSeed(payload);
    SPDLOG_INFO("[Anchor] MergeShip2: {} of seed {} ignored (this file's seed is {})", type, theirs, local);
    MergeShipTest_Log("ANCHOR %s of seed %u ignored (local seed %u)", type.c_str(), theirs, local);
    return false;
}

// ---- Termina's part of the team state (MS_ANCHOR_KEY_MM_TEAM_STATE) ------------------------------------------------
//
// Every part sent belongs to the loaded file and its playthrough (AnchorNet.h MS_ANCHOR_KEY_SEED): a part of another
// file or seed is never sent under this one's.

namespace {

// A file and playthrough: MergeShip_Save_SharedFileId (MsSharedSave.hyruleFileId) and MergeShipAnchor_LocalSeed.
struct TeamOwner {
    bool known = false;
    uint32_t fileId = 0;
    uint32_t seed = 0;

    bool Is(const TeamOwner& other) const {
        return known && other.known && (fileId == other.fileId) && (seed == other.seed);
    }
};

TeamOwner LoadedTeamOwner() {
    TeamOwner owner;
    if ((gSaveContext.fileNum >= 0) && (gSaveContext.fileNum <= 2)) {
        owner.known = true;
        owner.fileId = MergeShip_Save_SharedFileId();
        owner.seed = MergeShipAnchor_LocalSeed();
    }
    return owner;
}

std::mutex sTeamMMMutex; // read as the team state is sent, written as the engine is suspended and resumed
// MS_ANCHOR_BLOB_TEAM_MM is Termina's engine's latest part, written as the player leaves Termina (2Ship
// MergeShipAnchor_OnEngineSuspend). It does not say whose it is, and it outlives a file (load another, and it is still
// the last one's). So this engine notes the blob it last saw (as it is suspended and resumed); when it resumes to a
// new one, the player came back from that file's Termina: the file this engine has loaded is its owner.
std::string sTeamMMBlobSeen;
bool sTeamMMBlobPresentSeen = false;
TeamOwner sTeamMMBlobOwner;
// The last Save/mergeship_team_mm_<slot>.json parsed, by path and write time.
std::string sTeamMMFilePath;
std::filesystem::file_time_type sTeamMMFileTime{};
nlohmann::json sTeamMMFileDoc;

// A part that says whose it is ({"fileId", "seed", "state"}, as 2Ship writes it to disk): its state, when that is
// `owner`.
bool UnwrapTeamMM(const nlohmann::json& doc, const TeamOwner& owner, nlohmann::json& out) {
    if (!owner.known || !doc.is_object() || !doc.contains("fileId") || !doc["fileId"].is_number() ||
        !doc.contains("seed") || !doc["seed"].is_number() || !doc.contains("state") || !doc["state"].is_object() ||
        doc["state"].empty()) {
        return false;
    }
    if ((doc["fileId"].get<uint32_t>() != owner.fileId) || (doc["seed"].get<uint32_t>() != owner.seed)) {
        return false;
    }
    out = doc["state"];
    return true;
}

// Termina's part 2Ship left on disk for this file slot: what Termina has, when the player has not been there in this
// session (the host blob is in memory only). Relative to the working directory, as the host's shared saves
// (Save\mergeship_shared_N.bin).
bool ReadTeamMMFile(const TeamOwner& owner, nlohmann::json& out) {
    if (!owner.known) {
        return false;
    }
    const std::string path = "Save/mergeship_team_mm_" + std::to_string(gSaveContext.fileNum + 1) + ".json";
    std::error_code error;
    const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, error);
    if (error) {
        return false;
    }
    std::lock_guard<std::mutex> lock(sTeamMMMutex);
    if ((path != sTeamMMFilePath) || (time != sTeamMMFileTime)) {
        std::ifstream in(path, std::ios::binary);
        nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
        if (doc.is_discarded()) {
            return false; // written as we read it, or damaged: tried again next time
        }
        sTeamMMFilePath = path;
        sTeamMMFileTime = time;
        sTeamMMFileDoc = std::move(doc);
    }
    return UnwrapTeamMM(sTeamMMFileDoc, owner, out);
}

// This player's own Termina part for `owner`: Termina's latest, from this process's own 2ship.dll, else what 2Ship
// last wrote to disk for this file slot. (Not a teammate's copy: that is the team's, not where this player's Termina
// is.)
bool OwnMMTeamState(const TeamOwner& owner, nlohmann::json& out) {
    if (!owner.known) {
        return false;
    }
    nlohmann::json blob;
    if (sHost.Present() && sHost.GetBlobJson(MS_ANCHOR_BLOB_TEAM_MM, blob) && blob.is_object() && !blob.empty()) {
        if (UnwrapTeamMM(blob, owner, out)) {
            return true;
        }
        if (!blob.contains("state")) {
            std::lock_guard<std::mutex> lock(sTeamMMMutex);
            if (sTeamMMBlobOwner.Is(owner)) {
                out = std::move(blob);
                return true;
            }
        }
    }
    return ReadTeamMMFile(owner, out);
}

} // namespace

bool MergeShipAnchor_SameTerminaCycle(const nlohmann::json& theirMM) {
    nlohmann::json own;
    if (!theirMM.is_object() || !theirMM.contains("cycle") || !theirMM["cycle"].is_number() ||
        !OwnMMTeamState(LoadedTeamOwner(), own) || !own.contains("cycle") || !own["cycle"].is_number()) {
        return false;
    }
    return theirMM["cycle"].get<uint32_t>() == own["cycle"].get<uint32_t>();
}

nlohmann::json MergeShipAnchor_MMTeamState(Anchor* anchor) {
    const TeamOwner owner = LoadedTeamOwner();
    if (!owner.known) {
        return nlohmann::json();
    }
    nlohmann::json mm;
    // This player's own: Termina's latest, else what Termina had when 2Ship last wrote this file slot's part.
    if (OwnMMTeamState(owner, mm)) {
        return mm;
    }
    // The last one a teammate sent, in this file and playthrough.
    if (anchor != nullptr) {
        std::lock_guard<std::mutex> lock(anchor->mergeShipMMTeamStateMutex);
        const TeamOwner received{ true, anchor->mergeShipMMTeamStateFileId, anchor->mergeShipMMTeamStateSeed };
        if (anchor->mergeShipMMTeamState.is_object() && !anchor->mergeShipMMTeamState.empty() && received.Is(owner)) {
            return anchor->mergeShipMMTeamState;
        }
    }
    return nlohmann::json();
}

std::string MergeShipAnchor_ConnectionSummary() {
    if (Anchor::Instance == nullptr || !Anchor::Instance->IsHostTransport()) {
        return std::string();
    }
    MsNetStatus status = sHost.Status();
    char line[160];
    snprintf(line, sizeof(line), "Shared with Termina: connection %u%s", status.connectionId,
             status.reconnects != 0 ? " (reconnected)" : "");
    return line;
}

void MergeShipAnchor_LogState(const char* why) {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    int players = 0;
    int termina = 0;
    for (auto& [clientId, client] : anchor->clients) {
        if (client.online && !client.self) {
            players++;
            if (!Anchor::IsOwnWorld(client)) {
                termina++;
            }
        }
    }
    MsNetStatus status = sHost.Status();
    char line[320];
    snprintf(line, sizeof(line),
             "ANCHOR %s: engine soh, hosted %d, active %d, enabled %d, connected %d, connection %u, reconnects %u, "
             "clientId %u, players %d (%d in Termina)",
             why, anchor->IsHostTransport() ? 1 : 0, sHost.IsActive() ? 1 : 0, anchor->isEnabled ? 1 : 0,
             anchor->isConnected ? 1 : 0, status.connectionId, status.reconnects, anchor->ownClientId, players,
             termina);
    SPDLOG_INFO("{}", line);
    MergeShipTest_Log("%s", line);
    HostLog(line);
}

extern "C" void MergeShipAnchor_OnEngineSuspend(void) {
    MergeShipAnchor_LocalSeed(); // Termina's engine stamps and compares Hyrule's playthrough (MS_ANCHOR_BLOB_SEED)
    {
        // Termina's part as it is now: one that differs when this engine resumes was written during this trip, for
        // this file (sTeamMMBlobOwner).
        std::string blob;
        const bool present = sHost.Present() && sHost.GetBlob(MS_ANCHOR_BLOB_TEAM_MM, blob);
        std::lock_guard<std::mutex> lock(sTeamMMMutex);
        sTeamMMBlobPresentSeen = present;
        sTeamMMBlobSeen = present ? std::move(blob) : std::string();
    }
    Anchor* anchor = Anchor::Instance;
    // Hyrule's part of the team state, for Termina's engine to send along with its own while the player is there:
    // the loaded file's as it is now, connected or not (Anchor may connect in Termina), with whose it is ("fileId",
    // MergeShip_Save_SharedFileId; its "msSeed" is the playthrough's), so Termina's engine never sends an earlier
    // file's or seed's under this one. With no file loaded there is none: an older one is erased, never left to be sent
    // for whatever is loaded next.
    if (sHost.Present()) {
        bool written = false;
        if ((anchor != nullptr) && anchor->IsSaveLoaded()) {
            try {
                nlohmann::json oot = anchor->PrepTeamState();
                oot["fileId"] = MergeShip_Save_SharedFileId();
                sHost.SetBlobJson(MS_ANCHOR_BLOB_TEAM_OOT, oot);
                written = true;
            } catch (const std::exception& e) { SPDLOG_ERROR("[Anchor] MergeShip2: team state: {}", e.what()); }
        }
        if (!written) {
            sHost.SetBlob(MS_ANCHOR_BLOB_TEAM_OOT, std::string()); // length 0: the host erases the blob
        }
    }
    if (anchor == nullptr || !anchor->IsHostTransport()) {
        return;
    }
    if (anchor->isEnabled) {
        MergeShipAnchor_LogState("suspended");
    }
}

extern "C" void MergeShipAnchor_OnEngineResume(void) {
    {
        // Termina's engine wrote its part while the player was there, in the file this engine still has loaded (the
        // door and the owl warps keep the slot): that is whose it is. An unchanged blob keeps the owner it had.
        std::string blob;
        const bool present = sHost.Present() && sHost.GetBlob(MS_ANCHOR_BLOB_TEAM_MM, blob);
        const TeamOwner owner = LoadedTeamOwner();
        std::lock_guard<std::mutex> lock(sTeamMMMutex);
        if (present && (!sTeamMMBlobPresentSeen || (blob != sTeamMMBlobSeen))) {
            sTeamMMBlobOwner = owner;
        }
        sTeamMMBlobPresentSeen = present; // seen: a second resume with no trip in between changes nothing
        sTeamMMBlobSeen = present ? std::move(blob) : std::string();
    }
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr || !anchor->IsHostTransport()) {
        return;
    }
    if (!anchor->isEnabled) {
        // Anchor was disabled in Termina: nobody left to show.
        anchor->clients.clear();
        return;
    }
    // (Anchor may have been enabled in the other world: its room, name and team are this game's too.)
    AdoptHostSettings(anchor);
    if (anchor->isConnected) {
        // The player is in Hyrule again: tell the room where, and keep the host's handshake current for reconnects.
        anchor->SendPacket_UpdateClientState();
        anchor->SendHelloToRemote(anchor->PrepHandshake());
    }
    MergeShipAnchor_LogState("resumed");
}

// ---- a teammate's item from Termina -----------------------------------------------------------------------------

// The game `info` is from, for a notification (mirrors 2Ship's Rando::MergeShip::OriginTag, UnifiedDelivery.cpp): ""
// for masks (their name already carries " (MM)", or the Deku Mask / Fierce Deity's Mask exception). Otherwise
// MsItems.def's oot/mm columns when they say only one game uses the item; both usable (Majora's Mask's own items
// Hyrule can also hold, like the Pictograph Box) is not "neither game's" -- MsItemInfo::proxy (2Ship has no
// RandoItemId of its own for the item, so it is Hyrule's alone) breaks the tie, as IsProxy does on 2Ship's side.
static std::string MergeShipMMOriginTag(const MsItemInfo* info) {
    if ((info == nullptr) || (info->kind == MS_KIND_MASK)) {
        return "";
    }
    if (info->usableInOoT && !info->usableInMM) {
        return " (OOT)";
    }
    if (!info->usableInOoT && info->usableInMM) {
        return " (MM)";
    }
    return info->proxy ? " (OOT)" : " (MM)";
}

// MergeShip2: every item a Termina teammate finds is announced here (the user's rule): junk included, and an item
// only Termina can hold too (Termina gives it when the player is there). Tagged with the item's own game (not
// "found in Termina": MM_GIVE_ITEM also carries a Hyrule item a Termina check placed, the 2026-09-30 fix -- a Fairy
// Bow a teammate found in Termina is still Hyrule's, " (OOT)", not " (MM)").
static void MergeShipAnchor_NotifyMMFind(const nlohmann::json& payload, s32 key) {
    const MsItemInfo* info = MsItems_Get((MsItemKey)key);
    if ((info == nullptr) || (Anchor::Instance == nullptr)) {
        return;
    }
    // Once per process: Termina's engine may have announced it already and left it to Hyrule, or will give it later
    // without announcing it again (AnchorNet.h, MS_ANCHOR_CLAIM_ANNOUNCED).
    if (!MergeShipAnchor_Announce(payload)) {
        return;
    }
    std::string name = info->name;
    const std::string tag = MergeShipMMOriginTag(info);
    if (!tag.empty() && (name.find(tag) == std::string::npos)) {
        name += tag;
    }
    uint32_t clientId = payload.value("clientId", (uint32_t)0);
    auto& clients = Anchor::Instance->clients;
    // BUG-0090: with the item's icon (its RandomizerGet's item: Majora's Mask's items read from mm.o2r).
    const char* icon = nullptr;
    if ((info->rg != RG_NONE) && (info->rg < RG_MAX)) {
        Rando::Item& rItem = Rando::StaticData::RetrieveItem(info->rg);
        icon = rItem.HasCustomIcon() ? rItem.GetCustomIcon() : MergeShip_NotificationIcon(rItem.GetGIEntry_Copy().itemId);
    }
    Notification::Emit({
        .itemIcon = icon,
        .prefix = clients.contains(clientId) ? clients[clientId].name : std::string("?"),
        .message = "found",
        .suffix = name,
    });
}

void Anchor::HandlePacket_MMGiveItem(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !CrossWorldItemsOn()) {
        return;
    }
    const std::string name = payload.value(MS_ANCHOR_KEY_MS_ITEM, std::string());
    const MsItemKey key = KeyOfName(name);
    if (key == MSI_MAX || !MergeShipAnchor_CanClaim(payload)) {
        return;
    }
    if (!MS_ANCHOR_RECEIVE_REMOTE_ITEM((int32_t)key)) {
        // Hyrule cannot hold it: Termina's engine gives it natively when the player is there again.
        MergeShipTest_Log("ANCHOR MM_GIVE_ITEM %s: not here, left for Termina", name.c_str());
        MergeShipAnchor_NotifyMMFind(payload, key); // announced now; Termina gives it
        return;
    }
    MergeShipAnchor_Claim(payload);
    MergeShipTest_Log("ANCHOR MM_GIVE_ITEM %s: received in Hyrule", name.c_str());

    MergeShipAnchor_NotifyMMFind(payload, key);
}

// ---- test harness ---------------------------------------------------------------------------------------------------

// "anchor <command ...>" from MergeShipTest.cpp:
//   anchor enable <host> <port> <room> <name> [team]   set the settings (for this run) and enable Anchor
//   anchor disable
//   anchor state                                        log the connection (ANCHOR state: ...)
//   anchor loop <r> <g> <b> [dx dz]                     the loopback player (MergeShipAnchor_TestLoopback)
//   anchor give <rg>                                    send GIVE_ITEM for a RandomizerGet (as if found here)
//   anchor send <json>                                  send a raw packet (e.g. a mock server's control packet)
extern "C" void MergeShipAnchor_TestCommand(const char* args) {
    Anchor* anchor = Anchor::Instance;
    std::istringstream in(args != nullptr ? args : "");
    std::string op;
    in >> op;
    if (anchor == nullptr) {
        MergeShipTest_Log("ANCHOR %s: no Anchor", op.c_str());
        return;
    }
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
        int r = 255, g = 80, b = 80;
        float dx = 80.0f, dz = 0.0f;
        in >> r >> g >> b >> dx >> dz;
        MergeShipAnchor_TestLoopback((u8)r, (u8)g, (u8)b, dx, dz);
    } else if (op == "give") {
        int rg = 0;
        in >> rg;
        anchor->SendPacket_GiveItem(MOD_RANDOMIZER, (s16)rg);
        MergeShipTest_Log("ANCHOR give rg %d (msItem %s)", rg, MergeShipAnchor_MsItemName(MOD_RANDOMIZER, rg).c_str());
    } else if (op == "send") {
        std::string rest;
        std::getline(in, rest);
        try {
            anchor->SendJsonToRemote(nlohmann::json::parse(rest));
            MergeShipTest_Log("ANCHOR sent %s", rest.c_str());
        } catch (const std::exception& e) { MergeShipTest_Log("ANCHOR send: bad json: %s", e.what()); }
    } else {
        MergeShipTest_Log("ANCHOR unknown command '%s'", op.c_str());
    }
}
