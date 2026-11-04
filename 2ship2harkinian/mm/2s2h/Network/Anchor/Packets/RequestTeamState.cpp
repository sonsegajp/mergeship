#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
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

// MergeShip2: milliseconds on a clock that only goes forward, for the join window (MergeShipAnchor.h).
uint64_t MergeShipAnchor_NowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Anchor::SendPacket_RequestTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = REQUEST_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // MergeShip2: every answer that arrives from now until the window closes is a join answer (MergeShipIsJoinAnswer),
    // and one of another playthrough makes ours the team's once (HandlePacket_UpdateTeamState). The time goes first:
    // the game thread reads it once it sees the flag.
    mergeShipTeamStateRequestMs = MergeShipAnchor_NowMs();
    mergeShipSentOwnTeamState = false;
    mergeShipAwaitingTeamState = true;
    SendJsonToRemote(payload);
}

bool Anchor::MergeShipIsJoinAnswer(const nlohmann::json& payload) {
    if (!mergeShipAwaitingTeamState) {
        return false;
    }
    // By when the packet arrived (OnIncomingJson), not when it is handled: Termina parked in Hyrule keeps every
    // teammate's team state, and a file's first frames defer them, so at an arrival hour-old states are handled just
    // after our request goes out. Those came before it: a teammate's progress of then, not an answer (as one they would
    // raise this player's counters to what somebody had at that old moment). A packet without the stamp (a replayed
    // queue entry) is no answer either.
    const uint64_t requestMs = mergeShipTeamStateRequestMs.load();
    const uint64_t receivedMs =
        (payload.contains(MS_ANCHOR_KEY_RECV_MS) && payload[MS_ANCHOR_KEY_RECV_MS].is_number_unsigned())
            ? payload[MS_ANCHOR_KEY_RECV_MS].get<uint64_t>()
            : 0;
    if ((receivedMs == 0) || (receivedMs < requestMs)) {
        return false;
    }
    // One that came after the window closed is a teammate's progress, not the answer to our request.
    return (receivedMs - requestMs) <= kMergeShipJoinWindowMs;
}

void Anchor::HandlePacket_RequestTeamState(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }
    // One answer per process (it carries both worlds' parts).
    if (!MergeShipAnchor_Claim(payload)) {
        return;
    }

    SendPacket_UpdateTeamState();
    // MergeShip2: what the team was just given of Termina is Hyrule's to send while the player is there, this run and
    // the next (MergeShipAnchor_PersistTerminaTeamState).
    MergeShipAnchor_PersistTerminaTeamState(this);
}
