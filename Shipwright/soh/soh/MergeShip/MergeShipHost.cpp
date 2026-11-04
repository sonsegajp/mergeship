// MergeShip2 -- Ship of Harkinian's side of the host: the door to Termina (M3) and the shared save's transfers (M4).
// See MergeShipHost.h and Shipwright/mergeship2/docs/HOST_DESIGN.md.
//
// soh.exe (MERGESHIP_HOSTED off) gets the stubs at the end: never hosted, Ocarina of Time's exits untouched.

#include "soh/MergeShip/MergeShipSingleplayer.h"
#include "soh/MergeShip/MergeShipHost.h"

#ifdef MERGESHIP_HOSTED

#include "soh/MergeShip/MergeShipSharedSave.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipEntrances.h"
#include "soh/MergeShip/MergeShipUnifiedTracker.h"
#include "soh/MergeShip/MergeShipUnified.h"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipOwnership.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

#include <mergeship/EngineApi.h>
#include <mergeship/MsEntrances.h>
#include <mergeship/SharedSave.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h" // NAME_LANGUAGE_PAL

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

// debugSaveEditor.cpp: Ocarina of Time's file-name characters (PAL and NTSC tables) to text.
char z2ASCII(int code);
std::string decodeNTSCPlayerNameChar(int code);

static_assert(std::string_view(CVAR_ENHANCEMENT("MergeShip.TerminaDoor")) == MS_CVAR_TERMINA_DOOR,
              "SoH's Termina door option must be the CVar both engines read (EngineApi.h MS_CVAR_TERMINA_DOOR)");

namespace {

const MsHostServices* sHost = nullptr;

// The door, as this engine sees it.
enum class Door {
    Idle,    // nothing going on
    Away,    // the player went to Termina; the transition that took them there is held, faded out
    Arrived, // the player is back: the held transition commits sArrivalEntrance on the next frame
};
Door sDoor = Door::Idle;
s16 sArrivalEntrance = ENTR_KOKIRI_FOREST_UPPER_EXIT;
// The body the player comes back in (MsTransfer.playerAge / playerForm), or -1.
s32 sArrivalAge = -1;
s32 sArrivalForm = -1;
// The arrival is an entrance randomizer key (MS_TRANSFER_FLAG_RANDO_ENTRANCE, MergeShipEntrances.h), resolved as it
// commits.
bool sArrivalRando = false;
s16 sPendingOwlWarp = -1; // MergeShipHost_RequestOwlWarp: the owl statue the fade-out to Termina soars to
// BUG-0036: Anchor's warp to a teammate in the other world -- going (to Termina) or arriving (from it).
struct AnchorWarp {
    bool set = false;
    s32 entrance = 0;
    s32 room = 0;
    f32 pos[3] = {};
    s32 yaw = 0;
};
AnchorWarp sPendingAnchorWarp; // the fade-out to Termina takes Link to this teammate
AnchorWarp sArrivalAnchorWarp; // the arrival from Termina puts Link where this teammate is
bool sTransferRequested = false;
const char* sDoorState = "idle";

void HostLog(int32_t level, const char* message) {
    if (sHost != nullptr && sHost->Log != nullptr) {
        sHost->Log(sHost->host, level, message);
    }
}

void DoorLog(const char* state, const char* fmt, ...) {
    sDoorState = state;
    char message[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    MergeShipTest_Log("DOOR %s: %s", state, message);
    char line[300];
    snprintf(line, sizeof(line), "SoH door %s: %s", state, message);
    HostLog(0, line);
}

// The file slot the trip belongs to: 0-2, or -1 for a file that is never saved (the debug file, 0xFF/0xFE).
int32_t SaveSlot() {
    return (gSaveContext.fileNum >= 0 && gSaveContext.fileNum < 3) ? gSaveContext.fileNum : -1;
}

void PlayerNameAscii(char* out, size_t size) {
    std::string name;
    for (int i = 0; i < 8; i++) {
        if (gSaveContext.ship.filenameLanguage == NAME_LANGUAGE_PAL) {
            name += z2ASCII(gSaveContext.playerName[i]);
        } else {
            name += decodeNTSCPlayerNameChar(gSaveContext.playerName[i]);
        }
    }
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    snprintf(out, size, "%s", name.c_str());
}

// The fade-out to Termina has finished: hand the player to Majora's Mask. randoKey: the Termina entrance a shuffled
// entrance leads to (MsEntrances.h), or -1 for the door; owl: the owl statue the Song of Soaring soars to, or -1.
void RequestTerminaTransfer(PlayState* play, s16 randoKey, s16 owl = -1) {
    MsTransfer transfer = {};
    transfer.size = sizeof(MsTransfer);
    transfer.from = MS_ENGINE_SOH;
    transfer.to = MS_ENGINE_2SHIP;
    transfer.entrance = (owl >= 0) ? owl : (randoKey >= 0) ? randoKey : MS_ENTRANCE_2SHIP_CLOCK_TOWN;
    transfer.flags = (owl >= 0)        ? MS_TRANSFER_FLAG_OWL_WARP
                     : (randoKey >= 0) ? MS_TRANSFER_FLAG_RANDO_ENTRANCE
                                       : MS_TRANSFER_FLAG_TERMINA_DOOR;
    if (sPendingAnchorWarp.set) {
        // BUG-0036: Anchor's warp to a teammate in Termina.
        transfer.entrance = sPendingAnchorWarp.entrance;
        transfer.flags = MS_TRANSFER_FLAG_ANCHOR_WARP;
        transfer.anchorRoom = sPendingAnchorWarp.room;
        transfer.anchorPos[0] = sPendingAnchorWarp.pos[0];
        transfer.anchorPos[1] = sPendingAnchorWarp.pos[1];
        transfer.anchorPos[2] = sPendingAnchorWarp.pos[2];
        transfer.anchorYaw = sPendingAnchorWarp.yaw;
        sPendingAnchorWarp.set = false;
    }
    // The seed's overrides of Termina's entrances go with every trip (2Ship applies them, MergeShipEntrances.h).
    int32_t entrancePairs = 0;
    const int16_t* entranceTable = MergeShipEntrances_Table(&entrancePairs);
    if (entranceTable != nullptr) {
        transfer.entranceTable = entranceTable;
        transfer.entranceTableCount = entrancePairs;
        transfer.flags |= MS_TRANSFER_FLAG_ENTRANCE_TABLE;
    }
    // A unified seed's Hyrule checks go with every trip too, for Termina's hints and check tracker
    // (MergeShipUnifiedTracker.h, include/mergeship/MsWorldCatalog.h).
    if (const MsWorldCatalog* catalog = MergeShip_Catalog_Hyrule()) {
        transfer.worldCatalog = catalog;
        transfer.flags |= MS_TRANSFER_FLAG_WORLD_CATALOG;
    }
    // A unified seed's file makes Termina's file a randomizer file of the same seed (2Ship's UnifiedSeed.h); a trip
    // without the flag tells 2Ship the Hyrule file is not unified.
    std::string seedFile;
    if (MergeShipUnified_SeedHandoff(seedFile, transfer.unifiedSeed)) {
        snprintf(transfer.unifiedSeedFile, sizeof(transfer.unifiedSeedFile), "%s", seedFile.c_str());
        transfer.flags |= MS_TRANSFER_FLAG_UNIFIED_SEED;
        DoorLog("away", "unified seed %u, seed file %s", transfer.unifiedSeed,
                seedFile.empty() ? "(none found)" : transfer.unifiedSeedFile);
    }
    transfer.saveSlot = SaveSlot();
    PlayerNameAscii(transfer.playerName, sizeof(transfer.playerName));
    // Link goes as he is: adult or child, or in the Majora's Mask form he wears (SoH's PlayerForm is MS_FORM_*).
    transfer.flags |= MS_TRANSFER_FLAG_PLAYER_BODY;
    transfer.playerAge = gSaveContext.linkAge;
    transfer.playerForm = MergeShip_GetPlayerForm();

    // Singleplayer (never a randomizer file): Termina as New Game Plus, Navi in Tatl's place.
    if (MergeShip_Singleplayer_Active()) {
        transfer.flags |= MS_TRANSFER_FLAG_SINGLEPLAYER;
        if (CVarGetInteger(MERGESHIP_SINGLEPLAYER_FD_MOON_CVAR, 0)) {
            transfer.flags |= MS_TRANSFER_FLAG_SP_FD_FROM_MOON;
        }
    }

    // "Share items during Termina's first cycle" is 2Ship's to apply; this menu's copy can turn it off for the trip.
    if (!CVarGetInteger(MS_CVAR_SHARE_FIRST_CYCLE, 1)) {
        transfer.flags |= MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE;
    }

    // M4: what the player carries goes with them.
    if (sHost->GetSharedSave != nullptr) {
        MsSharedSave* shared = static_cast<MsSharedSave*>(sHost->GetSharedSave(sHost->host, transfer.saveSlot));
        MergeShipShared_Capture(shared);
        sHost->CommitSharedSave(sHost->host, transfer.saveSlot);
        transfer.flags |= MS_TRANSFER_FLAG_SHARED_SAVE;
    }

    DoorLog("away", "%s room %d -> Termina 0x%X (file slot %d, \"%s\", age %d, form %d, %d entrance overrides)",
            (owl >= 0)        ? "Song of Soaring to an owl statue, scene"
            : (randoKey >= 0) ? "shuffled entrance, scene"
                              : "Lost Woods",
            play->roomCtx.curRoom.num, transfer.entrance,
            transfer.saveSlot, transfer.playerName, transfer.playerAge, transfer.playerForm, entrancePairs);
    sHost->RequestTransfer(sHost->host, &transfer);
    sTransferRequested = true;
}

// Coming back: Link returns adult or child as Termina had him, or in the transformation he wore there (if Hyrule's
// file owns that mask; otherwise as the human of his age). The age takes effect as the scene loads (Play_Destroy swaps
// the age's equipment when linkAgeOnLoad differs), the form through the MergeShip save's form.
void ApplyArrivalBody(PlayState* play) {
    if (sArrivalForm < 0) {
        return;
    }
    const s32 age = ((sArrivalAge == LINK_AGE_ADULT) || (sArrivalAge == LINK_AGE_CHILD)) ? sArrivalAge
                                                                                          : gSaveContext.linkAge;
    u8 form = (u8)sArrivalForm;
    if (!MERGESHIP_FORM_IS_MM(form) || !MergeShip_HasTransformationMask(MergeShip_FormToMM(form))) {
        form = (u8)age;
    }
    const u8 oldForm = MergeShip_GetPlayerForm();
    if (MergeShip_FormToMM(oldForm) != MergeShip_FormToMM(form)) {
        MergeShip_SwapFormEquips(oldForm, form);
    }
    MergeShip_SetPlayerForm(form);
    play->linkAgeOnLoad = age;
    DoorLog("body", "age %d form %d (Termina sent age %d form %d)", age, form, sArrivalAge, sArrivalForm);
    sArrivalAge = sArrivalForm = -1;
}

// BUG-0036: Link arrives at the teammate's place (as Anchor's own TELEPORT_TO does, TeleportTo.cpp).
void ApplyArrivalAnchorWarp(PlayState* play) {
    if (!sArrivalAnchorWarp.set) {
        return;
    }
    sArrivalAnchorWarp.set = false;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = (s16)sArrivalAnchorWarp.entrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = (s8)sArrivalAnchorWarp.room;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos.x = sArrivalAnchorWarp.pos[0];
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos.y = sArrivalAnchorWarp.pos[1];
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos.z = sArrivalAnchorWarp.pos[2];
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = (s16)sArrivalAnchorWarp.yaw;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0xDFF;
    gSaveContext.respawnFlag = 1;
    static HOOK_ID hookId = 0;
    hookId = REGISTER_VB_SHOULD(VB_INFLICT_VOID_DAMAGE, {
        *should = false;
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(hookId);
    });
    DoorLog("body", "Anchor warp: at the teammate's place in room %d", sArrivalAnchorWarp.room);
}

void TakeArrivalBody(const MsTransfer* arrival) {
    if ((arrival->flags & MS_TRANSFER_FLAG_PLAYER_BODY) && MS_TRANSFER_HAS(arrival, playerForm)) {
        sArrivalAge = arrival->playerAge;
        sArrivalForm = arrival->playerForm;
    } else {
        sArrivalAge = sArrivalForm = -1;
    }
}

} // namespace

extern "C" {

s32 MergeShipHost_IsHosted(void) {
    return sHost != nullptr;
}

s32 MergeShipHost_TerminaDoorEnabled(void) {
    return (sHost != nullptr) && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TerminaDoor"), 1);
}

void* MergeShipHost_SharedSave(void) {
    if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr)) {
        return nullptr;
    }
    return sHost->GetSharedSave(sHost->host, SaveSlot());
}

void MergeShipHost_CommitSharedSave(void) {
    if ((sHost != nullptr) && (sHost->CommitSharedSave != nullptr)) {
        sHost->CommitSharedSave(sHost->host, SaveSlot());
    }
}

s16 MergeShipHost_OnSceneExit(PlayState* play, s16 nextEntranceIndex) {
    if (!MergeShipHost_TerminaDoorEnabled() || (play->sceneNum != SCENE_LOST_WOODS) ||
        (nextEntranceIndex != ENTR_KOKIRI_FOREST_UPPER_EXIT)) {
        return nextEntranceIndex;
    }
    // Singleplayer: the woods lead to Termina only after Ocarina of Time, once the salesman has met Link here.
    if (MergeShip_Singleplayer_TerminaLocked() || MergeShip_Singleplayer_DoorPending()) {
        return nextEntranceIndex;
    }
    // Exit 1 is the real way back to Kokiri Forest only at the opening the woods' Kokiri Forest entrance (spawn 0)
    // leads in through: the first clearing's south side, ~240 units behind that start position. Its other exit-1
    // openings (the clearing's north side, and nine more in the maze) are the wrong turns; the clearing's is the door.
    Player* player = GET_PLAYER(play);
    const ActorEntry* entries = play->linkActorEntry - play->setupEntranceList[play->curSpawn].spawn;
    const ActorEntry* kokiriForestStart = &entries[play->setupEntranceList[0].spawn];
    Vec3f start = { (f32)kokiriForestStart->pos.x, (f32)kokiriForestStart->pos.y, (f32)kokiriForestStart->pos.z };
    const f32 dist = Math_Vec3f_DistXZ(&player->actor.world.pos, &start);
    if (dist < 400.0f) {
        return nextEntranceIndex;
    }
    // BUG-0075 (the testers, 2026-10-10; the user: "one exit"): only the first clearing's wrong turn, straight ahead of
    // the Kokiri Forest entrance, is the door; the maze's other wrong turns lead back to Kokiri Forest, as in Ocarina of
    // Time.
    if (play->roomCtx.curRoom.num != play->setupEntranceList[0].room) {
        return nextEntranceIndex;
    }
    DoorLog("exit", "wrong turn out of Lost Woods room %d at (%.0f, %.0f), %.0f from the Kokiri Forest entrance",
            play->roomCtx.curRoom.num, player->actor.world.pos.x, player->actor.world.pos.z, dist);
    MergeShipEntrances_ClearPending(); // the door, not a shuffled entrance
    return ENTR_MERGESHIP_TERMINA;
}

s32 MergeShipHost_HoldTransition(PlayState* play) {
    if (sDoor == Door::Arrived) {
        play->nextEntranceIndex = sArrivalRando ? MergeShipEntrances_ResolveArrival(sArrivalEntrance) : sArrivalEntrance;
        sArrivalRando = false;
        ApplyArrivalBody(play);
        ApplyArrivalAnchorWarp(play);
        sDoor = Door::Idle;
        DoorLog("idle", "arrival committed: entrance 0x%X", sArrivalEntrance);
        return false;
    }
    if (play->nextEntranceIndex != ENTR_MERGESHIP_TERMINA) {
        return false;
    }
    if (sDoor == Door::Away) {
        return true;
    }
    // Anchor's warp to a teammate in Termina (MergeShipHost_RequestAnchorWarp; RequestTerminaTransfer takes it).
    if (sPendingAnchorWarp.set) {
        sDoor = Door::Away;
        RequestTerminaTransfer(play, -1);
        return true;
    }
    // The Song of Soaring to an owl statue (MergeShipHost_RequestOwlWarp).
    if (sPendingOwlWarp >= 0) {
        const s16 owl = sPendingOwlWarp;
        sPendingOwlWarp = -1;
        sDoor = Door::Away;
        RequestTerminaTransfer(play, -1, owl);
        return true;
    }
    // A shuffled entrance that leads to Termina (MergeShipEntrances.h), or the door.
    const s16 randoKey = MergeShipEntrances_TakePending();
    if ((randoKey < 0) && !MergeShipHost_TerminaDoorEnabled()) {
        // Turned off between the exit and the end of the fade: Ocarina of Time's own exit.
        play->nextEntranceIndex = ENTR_KOKIRI_FOREST_UPPER_EXIT;
        return false;
    }
    sDoor = Door::Away;
    RequestTerminaTransfer(play, randoKey);
    return true;
}

s32 MergeShipHost_RequestAnchorWarp(PlayState* play, s32 mmEntrance, s32 room, const f32 pos[3], s32 yaw) {
    if ((play == nullptr) || !MergeShipHost_TerminaDoorEnabled() || (sDoor != Door::Idle) ||
        MergeShip_Singleplayer_TerminaLocked()) {
        return false;
    }
    MergeShipEntrances_ClearPending();
    sPendingOwlWarp = -1;
    sPendingAnchorWarp.set = true;
    sPendingAnchorWarp.entrance = mmEntrance;
    sPendingAnchorWarp.room = room;
    sPendingAnchorWarp.pos[0] = pos[0];
    sPendingAnchorWarp.pos[1] = pos[1];
    sPendingAnchorWarp.pos[2] = pos[2];
    sPendingAnchorWarp.yaw = yaw;
    play->nextEntranceIndex = ENTR_MERGESHIP_TERMINA;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
    DoorLog("exit", "Anchor warp to a teammate in Termina (entrance 0x%X)", mmEntrance);
    return true;
}

s32 MergeShipHost_RequestOwlWarp(PlayState* play, u8 owl) {
    if ((play == nullptr) || !MergeShipHost_TerminaDoorEnabled() || (sDoor != Door::Idle) ||
        MergeShip_Singleplayer_TerminaLocked()) {
        return false;
    }
    // As Majora's Mask's own warp (2Ship PauseOwlWarp.cpp): a fade to white, then the statue; the transfer happens
    // where the fade commits the entrance (MergeShipHost_HoldTransition).
    MergeShipEntrances_ClearPending();
    sPendingOwlWarp = owl;
    play->nextEntranceIndex = ENTR_MERGESHIP_TERMINA;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    DoorLog("exit", "Song of Soaring to owl statue %d", owl);
    return true;
}

void MergeShipHost_ClaimSharedSave(s32 fileNum) {
    if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr) || (fileNum < 0) || (fileNum >= 3)) {
        return;
    }
    MsSharedSave* shared = static_cast<MsSharedSave*>(sHost->GetSharedSave(sHost->host, fileNum));
    // Another file's (a new file in the slot), or another seed's (a unified file whose seed is not the ledger's).
    const bool otherFile = shared->hyruleFileId != MergeShip_Save_SharedFileId();
    const uint32_t seed = IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : 0u;
    const bool otherSeed = IS_RANDO && MergeShip_Termina_Enabled() && (shared->ledgerSeed != 0) &&
                           (shared->ledgerSeed != seed);
    if (!otherFile && !otherSeed) {
        return;
    }
    char line[200];
    snprintf(line, sizeof(line),
             "SoH file %d: the slot's shared save was %s (file id %08X, seed %u, generation %u; this file %08X, seed "
             "%u): the slot starts anew, Termina's file is made again on the next trip",
             fileNum, otherFile ? "another file's" : "another seed's", shared->hyruleFileId, shared->ledgerSeed,
             shared->generation, MergeShip_Save_SharedFileId(), seed);
    const uint32_t magic = shared->magic;
    const uint32_t version = shared->version;
    const uint32_t size = shared->size;
    memset(shared, 0, sizeof(MsSharedSave));
    shared->magic = magic;
    shared->version = version;
    shared->size = size;
    shared->saveSlot = fileNum;
    shared->hyruleFileId = MergeShip_Save_SharedFileId();
    shared->terminaFileStale = 1;
    sHost->CommitSharedSave(sHost->host, fileNum);
    HostLog(0, line);
}

const char* MergeShipHost_DoorState(void) {
    return sDoorState;
}

} // extern "C"

void MergeShipHost_SetServices(const MsHostServices* services) {
    sHost = services;
}

void MergeShipHost_Log(int32_t level, const std::string& message) {
    HostLog(level, message.c_str());
}

bool MergeShipHost_CanExtractRom(int32_t engineId) {
    return (sHost != nullptr) && MS_HOST_HAS(sHost, ExtractRom) && (sHost->ExtractRom != nullptr) &&
           (sHost->ExtractRom(sHost->host, engineId, nullptr, nullptr, nullptr, nullptr) == 0);
}

bool MergeShipHost_ExtractRom(int32_t engineId, const std::string& romPath, const std::string& destDir,
                              void (*progress)(void* user, uint64_t done, uint64_t total), void* user) {
    return MergeShipHost_CanExtractRom(engineId) &&
           (sHost->ExtractRom(sHost->host, engineId, romPath.c_str(), destDir.c_str(), progress, user) == 0);
}

namespace {

// The file slot a save just wrote, not captured into its shared save yet (-1: none). OnSaveFile runs on SaveManager's
// worker thread, while the game thread captures, merges and applies the same shared save (Anchor's team state): a
// capture there between a merge and its apply undid the merge, and two commits raced on the same .tmp file. So the
// worker only notes the slot, and the game thread captures it on its next frame.
std::atomic<int32_t> sSaveCapturePending{ -1 };

void CaptureSavedFile(int32_t fileNum) {
    MergeShipShared_Capture(static_cast<MsSharedSave*>(sHost->GetSharedSave(sHost->host, fileNum)));
    sHost->CommitSharedSave(sHost->host, fileNum);
}

} // namespace

// M4: the shared save also follows the file itself. Loading a file applies its shared save (the last trip through
// the door may be newer than the file: Termina's gains, the file not saved since); saving the file captures it (so
// the shared save is never older than what the file holds).
void MergeShipHost_RegisterHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr) || (fileNum < 0) || (fileNum >= 3)) {
            return;
        }
        MergeShipHost_ClaimSharedSave(fileNum);
        // A save of this slot that no frame captured yet (the load came right after it): the file just loaded is what
        // that save wrote, so it is captured now, before the shared save is applied over it.
        if (sSaveCapturePending.exchange(-1) == fileNum) {
            CaptureSavedFile(fileNum);
        }
        MergeShip_Own_Unbind(); // each game's counts come from the shared save (MergeShipOwnership.h)
        const MsSharedSave* shared = static_cast<const MsSharedSave*>(sHost->GetSharedSave(sHost->host, fileNum));
        if (shared->lastWriter != 0) {
            MergeShipShared_Apply(shared);
            char line[96];
            snprintf(line, sizeof(line), "SoH file %d loaded: shared save generation %u applied", fileNum,
                     shared->generation);
            HostLog(0, line);
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveFile>([](int32_t fileNum, int32_t sectionID) {
        (void)sectionID;
        if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr) || (fileNum < 0) || (fileNum >= 3)) {
            return;
        }
        sSaveCapturePending = fileNum; // captured on the game thread (OnGameFrameUpdate below)
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        const int32_t fileNum = sSaveCapturePending.exchange(-1);
        if (fileNum < 0) {
            return;
        }
        // The file the save wrote is still the one in memory (the next frame after a save, in practice always): its
        // data then are the save's. Another (the player quit and loaded another file between): nothing of it is
        // left to capture.
        if (fileNum == SaveSlot()) {
            CaptureSavedFile(fileNum);
        } else {
            char line[120];
            snprintf(line, sizeof(line), "SoH file %d saved, but file %d is loaded now: its shared save not captured",
                     fileNum, SaveSlot());
            HostLog(1, line);
        }
    });
}

bool MergeShipHost_TakeTransferRequested() {
    const bool requested = sTransferRequested;
    sTransferRequested = false;
    return requested;
}

void MergeShipHost_OnResume(const MsTransfer* arrival) {
    if (arrival == nullptr) {
        // A manual switch (Ctrl+F12) while the player is in Termina: this world carries on, from Ocarina of Time's
        // own exit (Kokiri Forest), rather than holding a black screen.
        if (sDoor == Door::Away) {
            sArrivalEntrance = ENTR_KOKIRI_FOREST_UPPER_EXIT;
            sDoor = Door::Arrived;
            DoorLog("arrived", "switched back without a transfer: Kokiri Forest");
        }
        return;
    }
    const bool refused = (arrival->entrance < 0);
    if (!refused && (arrival->flags & MS_TRANSFER_FLAG_SHARED_SAVE) && (arrival->saveSlot == SaveSlot()) &&
        (arrival->sharedSave != nullptr) && (arrival->sharedSaveSize >= sizeof(MsSharedSave))) {
        // M4: set this file's share of the inventory from what came back.
        MergeShipShared_Apply(static_cast<const MsSharedSave*>(arrival->sharedSave));
    }
    const s16 entrance = refused ? (s16)ENTR_KOKIRI_FOREST_UPPER_EXIT : (s16)arrival->entrance;
    sArrivalRando = !refused && (arrival->flags & MS_TRANSFER_FLAG_RANDO_ENTRANCE);
    TakeArrivalBody(arrival);
    sArrivalAnchorWarp.set = !refused && (arrival->flags & MS_TRANSFER_FLAG_ANCHOR_WARP) &&
                             MS_TRANSFER_HAS(arrival, anchorYaw);
    if (sArrivalAnchorWarp.set) {
        sArrivalAnchorWarp.entrance = entrance;
        sArrivalAnchorWarp.room = arrival->anchorRoom;
        sArrivalAnchorWarp.pos[0] = arrival->anchorPos[0];
        sArrivalAnchorWarp.pos[1] = arrival->anchorPos[1];
        sArrivalAnchorWarp.pos[2] = arrival->anchorPos[2];
        sArrivalAnchorWarp.yaw = arrival->anchorYaw;
    }
    if (sDoor == Door::Away) {
        sArrivalEntrance = entrance;
        sDoor = Door::Arrived;
        DoorLog("arrived", "%s: entrance 0x%X", refused ? "the host refused the trip" : "back from Termina",
                entrance);
        return;
    }
    // Not waiting at the door (the player had switched back by hand and went on playing): take them there anyway.
    if ((gPlayState != nullptr) && (gSaveContext.gameMode == GAMEMODE_NORMAL) && !refused) {
        gPlayState->nextEntranceIndex = sArrivalRando ? MergeShipEntrances_ResolveArrival(entrance) : entrance;
        sArrivalRando = false;
        ApplyArrivalBody(gPlayState);
        ApplyArrivalAnchorWarp(gPlayState);
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        DoorLog("arrived", "back from Termina while playing: entrance 0x%X", entrance);
    } else {
        DoorLog("idle", "arrival 0x%X ignored: no game in progress", entrance);
    }
}

#else // !MERGESHIP_HOSTED: soh.exe

extern "C" {
s32 MergeShipHost_IsHosted(void) {
    return 0;
}
s32 MergeShipHost_TerminaDoorEnabled(void) {
    return 0;
}
void* MergeShipHost_SharedSave(void) {
    return nullptr;
}
void MergeShipHost_CommitSharedSave(void) {
}
s16 MergeShipHost_OnSceneExit(PlayState* play, s16 nextEntranceIndex) {
    (void)play;
    return nextEntranceIndex;
}
s32 MergeShipHost_HoldTransition(PlayState* play) {
    (void)play;
    return 0;
}
const char* MergeShipHost_DoorState(void) {
    return "not hosted";
}
s32 MergeShipHost_RequestAnchorWarp(PlayState* play, s32 mmEntrance, s32 room, const f32 pos[3], s32 yaw) {
    return 0;
}

s32 MergeShipHost_RequestOwlWarp(PlayState* play, u8 owl) {
    (void)play;
    (void)owl;
    return 0;
}
void MergeShipHost_ClaimSharedSave(s32 fileNum) {
    (void)fileNum;
}
}

#endif // MERGESHIP_HOSTED
