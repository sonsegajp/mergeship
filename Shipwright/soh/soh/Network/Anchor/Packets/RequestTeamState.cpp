#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include <nlohmann/json.hpp>

#include <chrono>

/**
 * REQUEST_TEAM_STATE
 *
 * Requests team state from the server, which will pass on the request to any connected teammates, or send the last
 * known state if no teammates are connected.
 *
 * This fires when loading into a file while Anchor is connected, or when Anchor is connected while a file is already
 * loaded
 *
 * Note: This can additionally be fired with a button in the menus to fix any desyncs that may have occurred in the save
 * state
 */

void Anchor::SendPacket_RequestTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = REQUEST_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // MergeShip2: the answers are this player joining the team (MergeShipIsJoinAnswer, HandlePacket_UpdateTeamState).
    // The time first: an answer can only arrive after the request is sent, and a reader that sees the window open
    // must see this request's time.
    mergeShipTeamStateRequestMs = MergeShipNowMs();
    mergeShipPushedOwnTeamState = false;
    mergeShipAwaitingTeamState = true;
    SendJsonToRemote(payload);
}

int64_t Anchor::MergeShipNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// MergeShip2: several teammates may answer, each with their own state, so the window stays open for all of them. It
// is judged by when the team state arrived (OnIncomingJson's kMergeShipRecvMsKey): one that waited in a queue from
// before the request (the player was in the other world, at file select, or busy) is a teammate's old save, and one
// that arrived in the window still answers it however late it is handled. A team state arriving later is a
// teammate's save, merged without the joiner's extras.
bool Anchor::MergeShipIsJoinAnswer(const nlohmann::json& payload) {
    if (!mergeShipAwaitingTeamState) {
        return false;
    }
    const int64_t requestMs = mergeShipTeamStateRequestMs;
    int64_t receivedMs = MergeShipNowMs(); // a packet that did not come through OnIncomingJson: now
    if (payload.is_object() && payload.contains(kMergeShipRecvMsKey) && payload[kMergeShipRecvMsKey].is_number()) {
        receivedMs = payload[kMergeShipRecvMsKey].get<int64_t>();
    }
    return (receivedMs >= requestMs) && (receivedMs - requestMs <= kMergeShipJoinWindowMs);
}

void Anchor::HandlePacket_RequestTeamState(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }
    // MergeShip2: one answer per process (it carries both worlds' parts).
    if (!MergeShipAnchor_Claim(payload)) {
        return;
    }

    // MergeShip2: on the game thread: the shared save is captured into the answer as it is now.
    SendPacket_UpdateTeamState(true);
}
