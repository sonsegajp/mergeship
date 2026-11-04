// MergeShip2 -- the unified randomizer's goal on 2Ship's side (UnifiedGoal.h).

#include "UnifiedGoal.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <set>

#include <mergeship/MsItems.h>
#include <mergeship/SharedSave.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "2s2h/BenGui/Notification.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "Rando/Rando.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

namespace {

// The harness's goal: "goal fake" plays a local shared save (no host needed); "goal usehost" plays the host's shared
// save without the randomizer-file checks (the harness's files are not the unified seed's).
MsSharedSave sLocal;
bool sFake = false;
bool sUseHost = false;
// The harness's "goal majora": treat the next ending transition as Majora's, wherever Link is.
bool sTestMajora = false;

void Log(const char* fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip goal: {}", line);
    MergeShipHostTest_Log("GOAL %s", line); // trace_2ship.log (host harness); no-op without a script
    MergeShipTest_Log("GOAL %s", line);     // 2Ship's own harness; no-op without a script
}

void Notify(const std::string& message) {
    Notification::Emit({ .message = message, .remainingTime = 12.0f });
}

MsSharedSave* Shared() {
    if (sFake) {
        return &sLocal;
    }
    return static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
}

void Commit() {
    if (!sFake) {
        MergeShipHost_CommitSharedSave();
    }
}

// The shared save's goal, when this file plays its seed.
MsSharedSave* ActiveShared() {
    MsSharedSave* shared = Shared();
    if ((shared == nullptr) || (shared->goalType == MS_GOAL_NONE)) {
        return nullptr;
    }
    if (!sFake && !sUseHost && (!IS_RANDO || (shared->goalSeed != gSaveContext.save.shipSaveInfo.rando.finalSeed))) {
        return nullptr;
    }
    return shared;
}

int FormMasksOwned() {
    int count = 0;
    for (u8 mask : { (u8)ITEM_MASK_DEKU, (u8)ITEM_MASK_GORON, (u8)ITEM_MASK_ZORA, (u8)ITEM_MASK_FIERCE_DEITY }) {
        count += (INV_CONTENT(mask) == mask) ? 1 : 0;
    }
    return count;
}

// Triforce pieces the ledger records, found in either world (each check once; a harness give, check 0, each time).
int LedgerPieces(const MsSharedSave* shared) {
    if (!sFake && !sUseHost && (shared->ledgerSeed != 0) &&
        (shared->ledgerSeed != gSaveContext.save.shipSaveInfo.rando.finalSeed)) {
        return 0;
    }
    std::set<uint32_t> seen;
    int pieces = 0;
    const uint32_t count = std::min<uint32_t>(shared->ledgerCount, MS_SHARED_LEDGER_MAX);
    for (uint32_t i = 0; i < count; i++) {
        const MsLedgerEntry& entry = shared->ledger[i];
        if (entry.item != MSI_TRIFORCE_PIECE) {
            continue;
        }
        if ((entry.check != 0) && !seen.insert(((uint32_t)entry.world << 16) | entry.check).second) {
            continue;
        }
        pieces++;
    }
    return pieces;
}

// 2Ship's own count has the pieces found here and those the ledger gave it; the ledger has every piece of both worlds.
// Neither counts one piece twice, so the larger is the count.
int TriforcePieces(const MsSharedSave* shared) {
    return std::max<int>(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces, LedgerPieces(shared));
}

std::string Count(int n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

// What stands between Link and Majora: 2Ship's own moon and Majora requirements, and the goal's transformation masks.
std::string MajoraRequirements(const MsSharedSave* shared) {
    std::string text;
    auto add = [&text](const std::string& part) { text += (text.empty() ? "" : ", ") + part; };
    if (IS_RANDO) {
        const int remains = (int)std::max(RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_REMAINS_COUNT],
                                          RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_REMAINS_COUNT]);
        const int masks = (int)std::max(RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_MASKS_COUNT],
                                        RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_MASKS_COUNT]);
        if (remains > 0) {
            add(Count(remains, "boss remains", "boss remains"));
        }
        if (masks > 0) {
            add(masks >= 20 ? std::string("all 20 masks") : Count(masks, "mask", "masks"));
        }
    }
    if (shared->majoraFormMasks > 0) {
        add(shared->majoraFormMasks >= 4 ? std::string("all 4 transformation masks")
                                         : Count(shared->majoraFormMasks, "transformation mask",
                                                 "transformation masks"));
    }
    return text.empty() ? "only the way to the moon" : text;
}

std::string Describe(const MsSharedSave* shared) {
    const bool hyrule = (shared->goalFlags & MS_GOAL_FLAG_HYRULE) != 0;
    const bool majora = (shared->goalFlags & MS_GOAL_FLAG_MAJORA) != 0;
    switch (shared->goalType) {
        case MS_GOAL_GANON:
            return std::string("The goal is Hyrule's: ") + (hyrule ? "done" : "Ganon awaits") +
                   ". Majora is optional.";
        case MS_GOAL_MAJORA:
            return "The goal is Majora (Majora needs " + MajoraRequirements(shared) + "). Ganon is optional.";
        case MS_GOAL_BOTH:
            return std::string("The goal is Ganon and Majora. Hyrule: ") + (hyrule ? "done" : "not yet") +
                   ". Majora: " + (majora ? "defeated" : "needs " + MajoraRequirements(shared)) + ".";
        case MS_GOAL_TRIFORCE:
            return "The goal is the Triforce: " + std::to_string(TriforcePieces(shared)) + " of " +
                   std::to_string(shared->triforceRequired) + " pieces, from Hyrule and Termina.";
        default:
            return "";
    }
}

bool IsMajorasEnding(PlayState* play) {
    return (play->transitionTrigger == TRANS_TRIGGER_START) && (play->nextEntrance == ENTRANCE(TERMINA_FIELD, 0)) &&
           (gSaveContext.nextCutsceneIndex == 0xFFF7) && ((play->sceneId == SCENE_LAST_BS) || sTestMajora);
}

// Majora's Wrath fell and the ending transition has started (Boss07_Wrath_DeathCutscene).
void OnMajoraDefeated(PlayState* play, MsSharedSave* shared) {
    sTestMajora = false;
    shared->goalFlags |= MS_GOAL_FLAG_MAJORA;
    const bool complete = ((shared->goalFlags & MS_GOAL_FLAG_COMPLETE) != 0) ||
                          (shared->goalType == MS_GOAL_MAJORA) ||
                          ((shared->goalType == MS_GOAL_BOTH) && (shared->goalFlags & MS_GOAL_FLAG_HYRULE));
    if (complete) {
        shared->goalFlags |= MS_GOAL_FLAG_COMPLETE;
        Commit();
        Log("majora defeated: the goal is complete, Majora's Mask's ending plays (flags 0x%02X)", shared->goalFlags);
        Notify("Majora is defeated. The goal is complete!");
        return;
    }
    Commit();
    // Not the whole goal: back to the dawn of the first day, as the Song of Time takes Link (Player's SoT).
    play->nextEntrance = (gSaveContext.save.saveInfo.playerData.threeDayResetCount == 1) ? ENTRANCE(CUTSCENE, 1)
                                                                                         : ENTRANCE(CUTSCENE, 0);
    gSaveContext.nextCutsceneIndex = 0xFFF7;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    std::string message = "Majora is defeated! ";
    switch (shared->goalType) {
        case MS_GOAL_BOTH:
            message += "Ganon still threatens Hyrule.";
            break;
        case MS_GOAL_TRIFORCE:
            message += "But the goal is the Triforce: " + std::to_string(TriforcePieces(shared)) + " of " +
                       std::to_string(shared->triforceRequired) + " pieces.";
            break;
        default:
            message += "But the goal is Hyrule's.";
            break;
    }
    Log("majora defeated: %s -> the dawn of the first day (entrance 0x%X, flags 0x%02X)", message.c_str(),
        play->nextEntrance, shared->goalFlags);
    Notify(message);
}

// The Triforce Hunt over both worlds is complete: Majora's Mask's ending, as 2Ship's own hunt plays it (GiveItem.cpp).
void CheckTriforce(PlayState* play, MsSharedSave* shared) {
    if ((shared->goalType != MS_GOAL_TRIFORCE) || (shared->goalFlags & MS_GOAL_FLAG_COMPLETE) ||
        (shared->triforceRequired == 0) || (TriforcePieces(shared) < shared->triforceRequired)) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if ((player == nullptr) || (player->stateFlags1 & (PLAYER_STATE1_400 | PLAYER_STATE1_TALKING)) ||
        (play->msgCtx.msgMode != MSGMODE_NONE) || (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        return; // after the get-item and its textbox
    }
    shared->goalFlags |= MS_GOAL_FLAG_COMPLETE;
    Commit();
    Log("triforce complete: %d of %d pieces, Majora's Mask's ending plays", TriforcePieces(shared),
        (int)shared->triforceRequired);
    Notify("The Triforce is complete!");
    GameInteractor_ExecuteOnGameCompletion();
    GameInteractor::Instance->events.emplace_back(GIEventTransition{ .entrance = ENTRANCE(TERMINA_FIELD, 0),
                                                                     .cutsceneIndex = 0xFFF7,
                                                                     .transitionTrigger = TRANS_TRIGGER_START,
                                                                     .transitionType = TRANS_TYPE_FADE_BLACK });
}

void OnFrame() {
    if (gPlayState == nullptr) {
        return;
    }
    MsSharedSave* shared = ActiveShared();
    if (shared == nullptr) {
        return;
    }
    if (IsMajorasEnding(gPlayState)) {
        OnMajoraDefeated(gPlayState, shared);
    }
    CheckTriforce(gPlayState, shared);
}

// Registered once; each hook does nothing unless ActiveShared() finds the file's unified goal (or the harness's).
void RegisterHooks() {
    COND_HOOK(OnGameStateMainFinish, true, OnFrame);

    // The moon child who lets Link in to Majora: the goal's transformation masks, on top of 2Ship's own counts
    // (ActorBehavior/EnJs.cpp OverrideMainJsText, same refusal).
    COND_ID_HOOK(OnOpenText, 0x21FC, true, [](u16* textId, bool* loadFromMessageTable) {
        MsSharedSave* shared = ActiveShared();
        if ((shared == nullptr) || (FormMasksOwned() >= shared->majoraFormMasks)) {
            return;
        }
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "You are not strong enough to play with me... Come back with " +
                    Count(shared->majoraFormMasks, "transformation mask...", "transformation masks...");
        entry.nextMessageID = 0x21FD;
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
        Log("moon child refuses: %d of %d transformation masks", FormMasksOwned(), (int)shared->majoraFormMasks);
    });

    // The Clock Tower rooftop, where the moon looms: what the goal takes.
    COND_HOOK(OnSceneInit, true, [](s8 sceneId, s8 spawnNum) {
        (void)spawnNum;
        MsSharedSave* shared = ActiveShared();
        if ((shared == nullptr) || (sceneId != SCENE_OKUJOU)) {
            return;
        }
        const std::string text = Describe(shared);
        Log("rooftop: %s", text.c_str());
        Notify(text);
    });
}

static RegisterShipInitFunc sInitFunc(RegisterHooks, {});

} // namespace

namespace Rando {

namespace MergeShip {

bool GoalActive() {
    return ActiveShared() != nullptr;
}

std::string GoalDescription() {
    MsSharedSave* shared = ActiveShared();
    return (shared != nullptr) ? Describe(shared) : std::string();
}

bool GoalTestCommand(const std::string& op, const std::vector<std::string>& args) {
    if (op != "goal") {
        return false;
    }
    const std::string sub = args.empty() ? "state" : args[0];
    auto num = [&args](size_t i, int fallback) { return (i < args.size()) ? atoi(args[i].c_str()) : fallback; };
    if (sub == "fake") {
        sFake = true;
        sUseHost = false;
        sLocal = MsSharedSave{};
        sLocal.goalType = (uint8_t)num(1, MS_GOAL_BOTH);
        sLocal.triforceRequired = (uint16_t)num(2, 0);
        sLocal.majoraFormMasks = (uint8_t)num(3, 0);
    } else if (sub == "usehost") {
        sFake = false;
        sUseHost = true;
        MsSharedSave* shared = Shared();
        if ((shared != nullptr) && (args.size() > 1)) {
            shared->goalType = (uint8_t)num(1, MS_GOAL_BOTH); // SoH's capture wrote none: the harness's file is not rando
        }
    } else if (sub == "hyrule") {
        if (MsSharedSave* shared = Shared()) {
            if (num(1, 1)) {
                shared->goalFlags |= MS_GOAL_FLAG_HYRULE;
            } else {
                shared->goalFlags &= (uint8_t)~MS_GOAL_FLAG_HYRULE;
            }
        }
    } else if (sub == "majora") {
        // What Boss07_Wrath_DeathCutscene does when its light fills the screen.
        if (gPlayState != nullptr) {
            sTestMajora = true;
            gPlayState->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);
            gSaveContext.nextCutsceneIndex = 0xFFF7;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        }
    } else if (sub == "pieces") {
        gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = (u16)num(1, 0);
    } else if ((sub == "warp") && (args.size() > 1) && (gPlayState != nullptr)) {
        // To a scene the goal speaks in (the Clock Tower rooftop, ENTRANCE(CLOCK_TOWER_ROOFTOP, 0) = 0x2C00).
        gPlayState->nextEntrance = (u16)strtol(args[1].c_str(), nullptr, 0);
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    }
    MsSharedSave* shared = Shared();
    MsSharedSave* active = ActiveShared();
    Log("%s fake=%d hosted=%d active=%d type=%d flags=0x%02X seed=%u fileSeed=%u required=%d pieces=%d (own %d, "
        "ledger %d) formMasks=%d/%d text=\"%s\"",
        sub.c_str(), sFake ? 1 : 0, MergeShipHost_IsHosted(), (active != nullptr) ? 1 : 0,
        (shared != nullptr) ? shared->goalType : -1, (shared != nullptr) ? shared->goalFlags : 0,
        (shared != nullptr) ? shared->goalSeed : 0u, gSaveContext.save.shipSaveInfo.rando.finalSeed,
        (shared != nullptr) ? shared->triforceRequired : 0, (shared != nullptr) ? TriforcePieces(shared) : 0,
        (int)gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces,
        (shared != nullptr) ? LedgerPieces(shared) : 0, FormMasksOwned(),
        (shared != nullptr) ? shared->majoraFormMasks : 0,
        (shared != nullptr) ? Describe(shared).c_str() : "");
    return true;
}

} // namespace MergeShip

} // namespace Rando
